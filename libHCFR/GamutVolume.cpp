///////////////////////////////////////////////////////////////////////////////
// GamutVolume.cpp: volumetric gamut coverage (see GamutVolume.h).
///////////////////////////////////////////////////////////////////////////////

#include "GamutVolume.h"

#include <math.h>
#include <vector>

namespace
{
    // Sampling density for a reference gamut measured on its own, with no
    // measurement to mirror (ReferenceGamutVolume). The reference gamut is
    // synthetic, so its density costs nothing to measure -- only to compute.
    const int kRefCubeN = 17;

    // L*a*b* is cube-root compressed, so a cube sampled uniformly in LINEAR light
    // puts its first row up from black at L* ~ 35 and the straight chords between
    // rows cut far inside the real surface (linear sampling at 17^3 is 1.0% low;
    // warped it is 0.03%). s -> s^3 spaces the rows about evenly in L*. This is
    // the same warp C3DColorView::BuildGamut applies when it draws the solid.
    inline double RefWarp(double s) { return s * s * s; }

    // Slack on the in-gamut test. Reference colors reproduced exactly still come
    // back a few ULPs outside [0,1] through the XYZ round trip, and without this
    // a flawless display loses a sliver of coverage on its own gamut corners.
    const double kGamutEpsilon = 1e-9;

    // Barycentric lattice order for tetrahedra that straddle the reference
    // boundary: (n+2)(n+1)n/6 = 56 sample points at n = 6. Only a minority of
    // tets need it -- at 9^3 a third of them, holding a quarter of the volume --
    // because whole-in and whole-out tets are classified exactly (see below).
    const int kBoundaryLatticeOrder = 6;

    // The 6-tetrahedron Kuhn decomposition of a cube cell: every path from corner
    // 000 to corner 111 that advances one axis at a time. Corner bits are
    // v = 4*dr + 2*dg + db. The six tets tile the cell exactly, with no overlap.
    const int kTets[6][4] =
    {
        { 0, 1, 3, 7 }, { 0, 1, 5, 7 },
        { 0, 2, 3, 7 }, { 0, 2, 6, 7 },
        { 0, 4, 5, 7 }, { 0, 4, 6, 7 }
    };

    struct Vertex
    {
        double lab[3];  // where the volume is measured
        double rgb[3];  // reference linear RGB: in [0,1]^3 exactly when in gamut
    };

    // XYZ -> L*a*b*, normalised so that the reference white is the adopted white.
    // Deliberately mirrors ColorLab(XYZ, YWhiteRef, colorReference) with
    // YWhiteRef == the reference white's own Y (the caller pre-scales the
    // measurement), rather than calling it: that constructor takes its
    // CColorReference BY VALUE, and this runs over tens of thousands of vertices.
    // GamutVolume_unittests pins the two to each other.
    void ToLab(const double xyz[3], const double white[3], double lab[3])
    {
        const double epsilon = 216.0 / 24389.0;
        const double kappa   = 24389.0 / 27.0;
        double f[3];
        for (int i = 0; i < 3; i++)
        {
            double t = ( white[i] != 0.0 ) ? xyz[i] / white[i] : 0.0;
            f[i] = ( t > epsilon ) ? pow(t, 1.0 / 3.0) : ( kappa * t + 16.0 ) / 116.0;
        }
        lab[0] = 116.0 * f[1] - 16.0;
        lab[1] = 500.0 * ( f[0] - f[1] );
        lab[2] = 200.0 * ( f[1] - f[2] );
    }

    // Same result as ColorRGB(XYZ, colorReference), without the by-value copy.
    void ToRefRGB(const double xyz[3], const Matrix & xyzToRgb, double rgb[3])
    {
        for (int i = 0; i < 3; i++)
            rgb[i] = xyzToRgb(i, 0) * xyz[0] + xyzToRgb(i, 1) * xyz[1] + xyzToRgb(i, 2) * xyz[2];
    }

    double TetVolume(const Vertex & a, const Vertex & b, const Vertex & c, const Vertex & d)
    {
        double u[3], v[3], w[3];
        for (int i = 0; i < 3; i++)
        {
            u[i] = b.lab[i] - a.lab[i];
            v[i] = c.lab[i] - a.lab[i];
            w[i] = d.lab[i] - a.lab[i];
        }
        double cx = v[1] * w[2] - v[2] * w[1];
        double cy = v[2] * w[0] - v[0] * w[2];
        double cz = v[0] * w[1] - v[1] * w[0];
        return fabs( u[0] * cx + u[1] * cy + u[2] * cz ) / 6.0;
    }

    // Fraction of a tetrahedron that lies inside the reference gamut.
    //
    // The reference gamut is the CONVEX set { rgb : 0 <= r,g,b <= 1 }. Read as a
    // question about the tet's XYZ-linear hull, two cases fall out at the corners:
    //   - all four corners satisfying all six constraints  => wholly inside (1.0),
    //   - one constraint violated by all four corners      => wholly outside (0.0),
    // and the first is what makes a display measured against its own reference
    // read exactly 100.0 -- there every tet takes it. They remain CORNER tests,
    // which is an approximation rather than a proof for the solid this module
    // actually integrates: TetVolume charges for the L*a*b*-LINEAR interpolant,
    // and L*a*b* -> XYZ is not affine, so a Lab chord joining two in-gamut
    // corners can bow slightly outside. The gap is second order in the cell size,
    // and it only reaches tets that the shortcuts settle outright.
    //
    // The straddling remainder is sampled the same way, mixing the corner RGBs.
    // Sampling the L*a*b* mix instead and converting each sample back through the
    // reference matrix -- consistent with the interpolant, and cheap, since the
    // forward cube root inverts to a cube -- was tried and measured: it does not
    // move a single sample across the boundary at either 5^3 or 9^3, so the extra
    // work buys nothing and the straightforward mix stays.
    double InsideFraction(const Vertex * const corner[4])
    {
        const double lo = -kGamutEpsilon, hi = 1.0 + kGamutEpsilon;
        bool anyOutside = false;
        for (int c = 0; c < 4; c++)
            for (int i = 0; i < 3; i++)
                if ( corner[c]->rgb[i] < lo || corner[c]->rgb[i] > hi )
                    anyOutside = true;
        if ( !anyOutside )
            return 1.0;

        for (int i = 0; i < 3; i++)
        {
            bool allBelow = true, allAbove = true;
            for (int c = 0; c < 4; c++)
            {
                if ( corner[c]->rgb[i] >= lo ) allBelow = false;
                if ( corner[c]->rgb[i] <= hi ) allAbove = false;
            }
            if ( allBelow || allAbove )
                return 0.0;
        }

        const int n = kBoundaryLatticeOrder;
        int inside = 0, total = 0;
        for (int i = 0; i < n; i++)
            for (int j = 0; i + j < n; j++)
                for (int k = 0; i + j + k < n; k++)
                {
                    // the four weights sum to exactly 1: (i+j+k+l+1)/n with l = n-1-i-j-k
                    double wt[4] = { ( i + 0.25 ) / n, ( j + 0.25 ) / n, ( k + 0.25 ) / n,
                                     ( n - 1 - i - j - k + 0.25 ) / n };
                    bool in = true;
                    for (int ch = 0; ch < 3 && in; ch++)
                    {
                        double val = wt[0] * corner[0]->rgb[ch] + wt[1] * corner[1]->rgb[ch]
                                   + wt[2] * corner[2]->rgb[ch] + wt[3] * corner[3]->rgb[ch];
                        if ( val < lo || val > hi )
                            in = false;
                    }
                    if ( in )
                        inside++;
                    total++;
                }
        return (double)inside / total;
    }

    // Sum the tetrahedra of an N^3 vertex grid, in ONE pass. Returns the whole
    // volume; with clipToReference set, *clipped additionally receives the volume
    // inside the reference gamut, so the six determinants per cell are evaluated
    // once for both figures instead of once each.
    double SumTets(const std::vector<Vertex> & v, int N, bool clipToReference, double * clipped)
    {
        double total = 0.0, insideVol = 0.0;
        for (int r = 0; r + 1 < N; r++)
            for (int g = 0; g + 1 < N; g++)
                for (int b = 0; b + 1 < N; b++)
                {
                    const Vertex * cell[8];
                    for (int c = 0; c < 8; c++)
                        cell[c] = &v[ (size_t)( ( r + ( ( c >> 2 ) & 1 ) ) * N + ( g + ( ( c >> 1 ) & 1 ) ) ) * N
                                      + ( b + ( c & 1 ) ) ];
                    for (int t = 0; t < 6; t++)
                    {
                        const Vertex * corner[4] = { cell[ kTets[t][0] ], cell[ kTets[t][1] ],
                                                     cell[ kTets[t][2] ], cell[ kTets[t][3] ] };
                        double vol = TetVolume( *corner[0], *corner[1], *corner[2], *corner[3] );
                        if ( vol <= 0.0 )
                            continue;
                        total += vol;
                        if ( clipToReference )
                            insideVol += vol * InsideFraction( corner );
                    }
                }
        if ( clipped != NULL )
            *clipped = insideVol;
        return total;
    }

    // The reference gamut as a vertex grid: every linear combination
    // r*R + g*G + b*B of the reference primaries, which by construction sums to
    // the reference white at (1,1,1). levels[] gives the linear-light position of
    // each grid step (see BuildReferenceLevels).
    void BuildReferenceGrid(const CColorReference & ref, const double * levels, int N,
                            std::vector<Vertex> & out)
    {
        ColorXYZ R = ref.GetRed(), G = ref.GetGreen(), B = ref.GetBlue();
        ColorXYZ W = ref.GetWhite();
        double white[3] = { W[0], W[1], W[2] };

        out.resize( (size_t)N * N * N );
        size_t idx = 0;
        for (int ri = 0; ri < N; ri++)
            for (int gi = 0; gi < N; gi++)
                for (int bi = 0; bi < N; bi++, idx++)
                {
                    double xyz[3];
                    for (int i = 0; i < 3; i++)
                        xyz[i] = levels[ri] * R[i] + levels[gi] * G[i] + levels[bi] * B[i];
                    ToLab( xyz, white, out[idx].lab );
                    // By construction: the vertex IS r*R + g*G + b*B, so its
                    // reference RGB is the three levels. Filled rather than left
                    // at zero because zero reads as IN gamut, which would make a
                    // clipped sum over this grid quietly report "100% inside"
                    // instead of failing.
                    out[idx].rgb[0] = levels[ri];
                    out[idx].rgb[1] = levels[gi];
                    out[idx].rgb[2] = levels[bi];
                }
    }

    // Grid steps for the reference solid, taken from the measured cube's own gray
    // diagonal (black-compensated and normalised to 0..1).
    //
    // Both solids are then tessellated on the SAME parameter grid, so the error
    // the straight chords introduce -- always low, and worth ~0.5% at 5^3 -- is
    // very nearly common to numerator and denominator and cancels in the ratio.
    // A flawless display consequently reads exactly 100.0 / 100.0 at every cube
    // size, instead of the 99-and-change a fixed reference grid produces; the
    // sampling inherits the display's real EOTF and the wire's code quantisation
    // for free, since it IS the display's measured ramp.
    //
    // The reference SET is unchanged by this (it still spans the primaries from
    // black to white) -- only where it gets sampled. Returns false if the ramp
    // carries no usable direction at all, which leaves the caller with no shared
    // grid and therefore no number worth publishing.
    bool BuildReferenceLevels(const ColorXYZ * cube, int N, std::vector<double> & levels)
    {
        double black = cube[0][1];
        double white = cube[ (size_t)N * N * N - 1 ][1];
        if ( !( white > black ) )
            return false;

        levels.resize( (size_t)N );
        double prev = 0.0;
        for (int i = 0; i < N; i++)
        {
            size_t gray = ( (size_t)i * N + i ) * N + i;      // the cube's r=g=b diagonal
            double t = ( cube[gray][1] - black ) / ( white - black );
            if ( t < prev ) t = prev;                          // a dip in the ramp must not fold the mesh
            if ( t > 1.0 ) t = 1.0;                            // nor may an ABL peak overshoot white
            levels[i] = t;
            prev = t;
        }
        levels[0] = 0.0;
        levels[N - 1] = 1.0;

        // The steps then have to be made strictly increasing again. A display
        // that CLIPS (an HDR panel peaking well below the container, so every
        // gray above its roll-off reads the same luminance) or CRUSHES (a raised
        // black floor swallowing the bottom steps) returns one luminance for a
        // run of consecutive gray patches, and the two clamps above collapse that
        // run onto a single level. The measured mesh does not lose those layers
        // -- an off-diagonal node such as (100%,0,0) does not clip when the
        // diagonal does -- so leaving them coincident in the reference mesh
        // alone is exactly what breaks the cancellation the ratio depends on,
        // and it biases BOTH percentages high.
        //
        // So keep the steps the ramp genuinely resolves and respace the rest
        // between them. Where the ramp is informative the grid still IS the
        // measured ramp -- a monotone ramp is left bit-for-bit untouched, so a
        // flawless display still reads exactly 100.0 -- and where it is not, the
        // reference gets a sane spacing rather than losing a layer. Respacing is
        // even in CUBE ROOT of the level, i.e. even in L*, for the reason
        // RefWarp exists: layers spaced evenly in linear light bunch at the top
        // and leave the dark half of the solid to one coarse chord. Across a
        // whole collapsed ramp this reduces to RefWarp exactly.
        //
        // Sized by measurement, in RampDegradationTest: on a panel that is ideal
        // apart from its ramp -- so the truth is exactly 100 / 100 however few
        // steps survive -- dropping to two resolved steps costs 0.13 points with
        // this respacing and 10 points without it. Without it the overshoot also
        // HIDES: coverage saturates at a clean-looking 100 while the volume reads
        // 110, so the pair stops disagreeing in the way that would show it up.
        const double kMinStep = 1e-9;
        std::vector<int> anchor;
        anchor.push_back( 0 );
        for (int i = 1; i < N - 1; i++)
            if ( levels[i] > levels[ anchor.back() ] + kMinStep && levels[i] < 1.0 - kMinStep )
                anchor.push_back( i );
        anchor.push_back( N - 1 );
        for (size_t a = 1; a < anchor.size(); a++)
        {
            int p = anchor[a - 1], q = anchor[a];
            if ( q <= p + 1 )
                continue;
            double cLo = pow( levels[p], 1.0 / 3.0 ), cHi = pow( levels[q], 1.0 / 3.0 );
            for (int i = p + 1; i < q; i++)
            {
                double u = (double)( i - p ) / (double)( q - p );
                double c = cLo + ( cHi - cLo ) * u;
                levels[i] = c * c * c;
            }
        }
        return true;
    }
}

double ReferenceGamutVolume(const CColorReference & ref)
{
    if ( ref.GetWhite()[1] <= 0.0 )
        return 0.0;
    std::vector<double> levels( (size_t)kRefCubeN );
    for (int i = 0; i < kRefCubeN; i++)
        levels[i] = RefWarp( (double)i / ( kRefCubeN - 1 ) );
    std::vector<Vertex> grid;
    BuildReferenceGrid( ref, &levels[0], kRefCubeN, grid );
    return SumTets( grid, kRefCubeN, false, NULL );
}

GamutVolumeResult ComputeGamutVolume(const ColorXYZ * cube, int cubeN,
                                     const CColorReference & ref)
{
    GamutVolumeResult res;
    if ( cube == NULL || cubeN < 2 )
        return res;

    ColorXYZ W = ref.GetWhite();
    double refWhiteY = W[1];
    if ( refWhiteY <= 0.0 )
        return res;
    double white[3] = { W[0], W[1], W[2] };

    const size_t nVerts = (size_t)cubeN * cubeN * cubeN;
    if ( !cube[nVerts - 1].isValid() || cube[nVerts - 1][1] <= 0.0 )
        return res;             // no white corner to anchor to

    // Media-relative, anchored to the cube's own white (see GamutVolume.h). Same
    // normalisation ColorLab(XYZ, whiteY, ref) applies, hoisted to the front so
    // the in-gamut test below shares it.
    const double scale = refWhiteY / cube[nVerts - 1][1];

    std::vector<Vertex> grid( nVerts );
    for (size_t i = 0; i < grid.size(); i++)
    {
        if ( !cube[i].isValid() )
            return res;         // a hole in the cube (partial capture): no number
        double xyz[3] = { cube[i][0] * scale, cube[i][1] * scale, cube[i][2] * scale };
        ToLab( xyz, white, grid[i].lab );
        ToRefRGB( xyz, ref.XYZtoRGBMatrix, grid[i].rgb );
    }

    // Reference solid on the measured cube's own grid, so the tessellation error
    // cancels out of both ratios (see BuildReferenceLevels). Without a usable
    // ramp there is no grid to share, and hence no number worth showing: a
    // reference tessellated on some other grid does not cancel, so the chips
    // would publish a ratio several points off with nothing to flag it. Same
    // rule as a hole in the cube -- no number beats a wrong one.
    std::vector<double> levels;
    if ( !BuildReferenceLevels( cube, cubeN, levels ) )
        return res;

    res.measured = SumTets( grid, cubeN, true, &res.intersection );

    std::vector<Vertex> refGrid;
    BuildReferenceGrid( ref, &levels[0], cubeN, refGrid );
    res.reference = SumTets( refGrid, cubeN, false, NULL );

    res.valid = ( res.reference > 0.0 );
    return res;
}
