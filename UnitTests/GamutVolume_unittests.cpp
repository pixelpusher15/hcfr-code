#include "GamutVolume.h"
#include <math.h>
#include <vector>
#include <cppunit/config/SourcePrefix.h>
#include <cppunit/extensions/HelperMacros.h>

#define THIS_TEST_CASE GamutVolumeTestCase

namespace
{
    const double kWhiteY = 100.0;   // arbitrary: the metric is media-relative

    // A synthetic perfect display whose gamut is exactly `panel`: an N^3 cube of
    // RGB drive levels through a gamma 2.2 EOTF into the panel's primaries, in
    // GenerateProfileColors order (r slowest, b fastest), normalised so that
    // full white measures kWhiteY. blueGain over-drives the blue channel, which
    // is how PartialOverlapTest builds a panel that overshoots the reference on
    // one axis while falling short on the other two.
    void MakeIdealCube(const CColorReference & panel, int N, double lumScale,
                       std::vector<ColorXYZ> & out, double gamma = 2.2, double blueGain = 1.0)
    {
        ColorXYZ R = panel.GetRed(), G = panel.GetGreen(), B = panel.GetBlue();
        double norm = kWhiteY * lumScale / panel.GetWhite()[1];
        out.clear();
        out.reserve( (size_t)N * N * N );
        for (int ri = 0; ri < N; ri++)
            for (int gi = 0; gi < N; gi++)
                for (int bi = 0; bi < N; bi++)
                {
                    double r = pow( (double)ri / ( N - 1 ), gamma );
                    double g = pow( (double)gi / ( N - 1 ), gamma );
                    double b = pow( (double)bi / ( N - 1 ), gamma ) * blueGain;
                    out.push_back( ColorXYZ( ( r * R[0] + g * G[0] + b * B[0] ) * norm,
                                             ( r * R[1] + g * G[1] + b * B[1] ) * norm,
                                             ( r * R[2] + g * G[2] + b * B[2] ) * norm ) );
                }
    }

    // Degrade ONLY the gray diagonal, leaving every other node ideal. This is the
    // separation that gives RampDegradationTest a known answer: the ramp is
    // merely where the shared grid comes from, while the GAMUT is the other 720
    // nodes, so an ideal panel still owes exactly 100 / 100 no matter how badly
    // its ramp behaves. A whole-cube pathology cannot say that -- it deforms the
    // solid too, and then there is nothing to compare against.
    //
    // SaturateGrayRamp is a display clipping its top end (an HDR panel peaking
    // well below the container: every gray above the roll-off reads the same
    // luminance, while full-drive red, far dimmer, never reaches the ceiling).
    void SaturateGrayRamp(std::vector<ColorXYZ> & cube, int N, int fromStep)
    {
        ColorXYZ w = cube[ (size_t)N * N * N - 1 ];
        for (int i = fromStep; i < N - 1; i++)
            cube[ ( (size_t)i * N + i ) * N + i ] = w;
    }

    // ...and the same loss at the other end: a raised black floor swallowing the
    // bottom steps.
    void CrushGrayRamp(std::vector<ColorXYZ> & cube, int N, int toStep)
    {
        ColorXYZ k = cube[0];
        for (int i = 1; i <= toStep && i < N - 1; i++)
            cube[ ( (size_t)i * N + i ) * N + i ] = k;
    }

    // Volume of the reference solid over a sub-box of the RGB cube, summed here
    // with ColorLab and the same Kuhn decomposition, on a linear N-step grid.
    // Only cells whose r AND g indices are below rgMax are counted, so
    // RefSubVolume(ref, N, N) is the whole solid. Deliberately independent of
    // GamutVolume.cpp: PartialOverlapTest's expected answer must be one the
    // module never sees while computing its own.
    double RefSubVolume(const CColorReference & ref, int N, int rgMax)
    {
        ColorXYZ R = ref.GetRed(), G = ref.GetGreen(), B = ref.GetBlue();
        double wY = ref.GetWhite()[1];
        std::vector<ColorLab> lab( (size_t)N * N * N );
        for (int ri = 0; ri < N; ri++)
            for (int gi = 0; gi < N; gi++)
                for (int bi = 0; bi < N; bi++)
                {
                    double lr = (double)ri / ( N - 1 ), lg = (double)gi / ( N - 1 ),
                           lb = (double)bi / ( N - 1 );
                    ColorXYZ xyz( lr * R[0] + lg * G[0] + lb * B[0],
                                  lr * R[1] + lg * G[1] + lb * B[1],
                                  lr * R[2] + lg * G[2] + lb * B[2] );
                    lab[ ( (size_t)ri * N + gi ) * N + bi ] = ColorLab( xyz, wY, ref );
                }

        const int tets[6][4] = { {0,1,3,7}, {0,1,5,7}, {0,2,3,7},
                                 {0,2,6,7}, {0,4,5,7}, {0,4,6,7} };
        double total = 0.0;
        for (int r = 0; r + 1 < N && r < rgMax; r++)
            for (int g = 0; g + 1 < N && g < rgMax; g++)
                for (int b = 0; b + 1 < N; b++)
                {
                    const ColorLab * cell[8];
                    for (int c = 0; c < 8; c++)
                        cell[c] = &lab[ (size_t)( ( r + ( ( c >> 2 ) & 1 ) ) * N
                                                + ( g + ( ( c >> 1 ) & 1 ) ) ) * N + ( b + ( c & 1 ) ) ];
                    for (int t = 0; t < 6; t++)
                    {
                        const ColorLab & a = *cell[ tets[t][0] ];
                        double u[3], v[3], w[3];
                        for (int i = 0; i < 3; i++)
                        {
                            u[i] = (*cell[ tets[t][1] ])[i] - a[i];
                            v[i] = (*cell[ tets[t][2] ])[i] - a[i];
                            w[i] = (*cell[ tets[t][3] ])[i] - a[i];
                        }
                        total += fabs( u[0] * ( v[1]*w[2] - v[2]*w[1] )
                                     + u[1] * ( v[2]*w[0] - v[0]*w[2] )
                                     + u[2] * ( v[0]*w[1] - v[1]*w[0] ) ) / 6.0;
                    }
                }
        return total;
    }
}

class THIS_TEST_CASE : public CPPUNIT_NS::TestFixture
{
    CPPUNIT_TEST_SUITE(THIS_TEST_CASE);
    CPPUNIT_TEST( SelfCoverageTest );
    CPPUNIT_TEST( ContainmentTest );
    CPPUNIT_TEST( WideGamutTest );
    CPPUNIT_TEST( LuminanceInvarianceTest );
    CPPUNIT_TEST( ConvergenceTest );
    CPPUNIT_TEST( PartialOverlapTest );
    CPPUNIT_TEST( RampDegradationTest );
    CPPUNIT_TEST( LabConventionTest );
    CPPUNIT_TEST( InvalidInputTest );
    CPPUNIT_TEST_SUITE_END();

protected:

    void SelfCoverageTest()
    {
        // A display that reproduces its reference exactly reads 100.0 / 100.0 --
        // EXACTLY, and at every cube size, because the reference solid is
        // tessellated on the measurement's own grid. Anything less is the
        // tessellation bias leaking into the number a user sees on a perfect
        // (simulated-sensor) sweep.
        CColorReference p3(UHDTV);
        const int sizes[] = { 3, 5, 9, 17 };
        for (int s = 0; s < 4; s++)
        {
            std::vector<ColorXYZ> cube;
            MakeIdealCube( p3, sizes[s], 1.0, cube );
            GamutVolumeResult r = ComputeGamutVolume( &cube[0], sizes[s], p3 );
            CPPUNIT_ASSERT( r.valid );
            CPPUNIT_ASSERT_DOUBLES_EQUAL( 100.0, r.VolumeRatio(), 1e-6 );
            CPPUNIT_ASSERT_DOUBLES_EQUAL( 100.0, r.Coverage(), 1e-6 );
        }

        // ...and it stays exact when the display's EOTF is nowhere near the
        // target, since the grid is read back off the measured ramp.
        std::vector<ColorXYZ> cube;
        MakeIdealCube( p3, 9, 1.0, cube, 1.6 );
        GamutVolumeResult r = ComputeGamutVolume( &cube[0], 9, p3 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 100.0, r.VolumeRatio(), 1e-6 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 100.0, r.Coverage(), 1e-6 );
    }

    void ContainmentTest()
    {
        // Rec.709 sits entirely inside DCI-P3 at the same white, so a 709 panel
        // measured against a P3 reference wastes nothing: its coverage and its
        // volume are the same number, and that number is the ratio of the two
        // reference solids -- a value this code never sees while computing it.
        CColorReference rec709(HDTV), p3(UHDTV);
        double expected = 100.0 * ReferenceGamutVolume( rec709 ) / ReferenceGamutVolume( p3 );
        CPPUNIT_ASSERT( expected > 60.0 && expected < 72.0 );   // ~66.6%

        std::vector<ColorXYZ> cube;
        MakeIdealCube( rec709, 9, 1.0, cube );
        GamutVolumeResult r = ComputeGamutVolume( &cube[0], 9, p3 );
        CPPUNIT_ASSERT( r.valid );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( expected, r.VolumeRatio(), 1.0 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( expected, r.Coverage(), 1.0 );

        // ...and the same holds one gamut up: P3 inside Rec.2020
        CColorReference rec2020(UHDTV2);
        double expected2 = 100.0 * ReferenceGamutVolume( p3 ) / ReferenceGamutVolume( rec2020 );
        MakeIdealCube( p3, 9, 1.0, cube );
        GamutVolumeResult r2 = ComputeGamutVolume( &cube[0], 9, rec2020 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( expected2, r2.VolumeRatio(), 1.0 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( expected2, r2.Coverage(), 1.0 );
    }

    void WideGamutTest()
    {
        // A Rec.2020 panel against a P3 reference contains the reference whole:
        // coverage saturates at 100 while the volume runs far past it. This is
        // the case a single "bigger is better" number would misreport.
        CColorReference p3(UHDTV), rec2020(UHDTV2);
        std::vector<ColorXYZ> cube;
        MakeIdealCube( rec2020, 9, 1.0, cube );
        GamutVolumeResult r = ComputeGamutVolume( &cube[0], 9, p3 );
        CPPUNIT_ASSERT( r.valid );
        CPPUNIT_ASSERT( r.VolumeRatio() > 140.0 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 100.0, r.Coverage(), 1.0 );
        CPPUNIT_ASSERT( r.Coverage() <= 100.0 );        // coverage never exceeds 100
    }

    void LuminanceInvarianceTest()
    {
        // Media-relative: a display measured at a third of the luminance is the
        // same gamut. Nothing outside the cube has to be kept in step for that to
        // hold -- the white anchor is the cube's own white corner.
        CColorReference p3(UHDTV);
        std::vector<ColorXYZ> bright, dim;
        MakeIdealCube( p3, 7, 1.0, bright );
        MakeIdealCube( p3, 7, 1.0 / 3.0, dim );
        GamutVolumeResult a = ComputeGamutVolume( &bright[0], 7, p3 );
        GamutVolumeResult b = ComputeGamutVolume( &dim[0], 7, p3 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( a.VolumeRatio(), b.VolumeRatio(), 1e-9 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( a.Coverage(), b.Coverage(), 1e-9 );
    }

    void ConvergenceTest()
    {
        // Cancellation is exact only when the two solids coincide; on a mismatched
        // pair a residual remains, so hold the coarsest preset to the accuracy
        // the header claims for it (~0.5 points at 5^3) and require the denser
        // cubes to stay at least as close.
        CColorReference rec709(HDTV), p3(UHDTV);
        double expected = 100.0 * ReferenceGamutVolume( rec709 ) / ReferenceGamutVolume( p3 );

        double prevErr = 1e9;
        const int sizes[] = { 5, 9, 17 };
        for (int s = 0; s < 3; s++)
        {
            std::vector<ColorXYZ> cube;
            MakeIdealCube( rec709, sizes[s], 1.0, cube );
            double v = ComputeGamutVolume( &cube[0], sizes[s], p3 ).VolumeRatio();
            double err = fabs( v - expected );
            CPPUNIT_ASSERT( err < 0.5 );
            CPPUNIT_ASSERT( err <= prevErr + 0.05 );   // never gets worse with density
            prevErr = err;
        }
    }

    void PartialOverlapTest()
    {
        // The case the metric exists for, and the only one where the boundary
        // sampler decides the answer: a panel that overshoots the reference on
        // one axis while falling short on the other two. Every other fixture here
        // is total containment in one direction or the other, so the whole-in /
        // whole-out shortcuts settle every tet and a broken sampler still passes.
        //
        // Drive blue hard enough that, once the media-relative white anchor has
        // renormalised the cube, red and green reach exactly 7/8 of the reference
        // and blue runs well past it. The intersection is then the reference
        // sub-box [0,7/8] x [0,7/8] x [0,1]; 7/8 is a node of the reference grid
        // at 9^3 on a linear ramp, so the expected coverage can be summed with
        // ColorLab in RefSubVolume - a value the module never sees while
        // computing its own, and one that moves a long way if InsideFraction's
        // lattice is wrong.
        CColorReference p3(UHDTV);
        const int N = 9, rgMax = 7;
        double s        = (double)rgMax / ( N - 1 );
        double lumaB    = p3.GetBlue()[1] / p3.GetWhite()[1];
        double blueGain = 1.0 + ( 1.0 / s - 1.0 ) / lumaB;

        std::vector<ColorXYZ> cube;
        MakeIdealCube( p3, N, 1.0, cube, 1.0, blueGain );
        GamutVolumeResult r = ComputeGamutVolume( &cube[0], N, p3 );
        CPPUNIT_ASSERT( r.valid );

        double expected = 100.0 * RefSubVolume( p3, N, rgMax ) / RefSubVolume( p3, N, N );
        CPPUNIT_ASSERT( expected > 50.0 && expected < 95.0 );   // a genuine partial overlap
        CPPUNIT_ASSERT_DOUBLES_EQUAL( expected, r.Coverage(), 1.5 );

        // ...and the disambiguation the pair of numbers exists for: a bigger
        // solid and a smaller coverage, at the same time. A sampler stuck at 1.0
        // would report the two as equal; one stuck at 0.0 would lose the second
        // assertion above by a mile.
        CPPUNIT_ASSERT( r.VolumeRatio() > 100.0 );
        CPPUNIT_ASSERT( r.Coverage() < 99.0 );

        // the intersection is a subset of both operands
        CPPUNIT_ASSERT( r.intersection <= r.measured * ( 1.0 + 1e-9 ) );
        CPPUNIT_ASSERT( r.intersection <= r.reference * 1.02 );
    }

    void RampDegradationTest()
    {
        // A display that reproduces its reference exactly owes 100 / 100 however
        // badly its GRAY RAMP behaves -- the ramp is only where the shared grid
        // is read from, and the gamut lives in the other 720 nodes. Degrading
        // just the diagonal moves at most N-2 of those 729 nodes, so the true
        // answer stays within a small fraction of a point of 100 / 100 while the
        // grid loses steps outright. That asymmetry is the point: it is the one
        // arrangement in which the cost of the lost steps can be sized against
        // something. `steps` is how many levels the ramp still resolves.
        //
        // Sized by measurement, with the respacing in BuildReferenceLevels
        // removed: a ramp down to two resolved steps reports volume 110.0 against
        // a truth of 100, and coverage stays pinned at 100 because the cap hides
        // the overshoot. With the respacing every case below holds to 0.13.
        CColorReference p3(UHDTV);
        const int N = 9;
        for (int steps = N; steps >= 2; steps--)
        {
            std::vector<ColorXYZ> sat, crushed;
            MakeIdealCube( p3, N, 1.0, sat );
            MakeIdealCube( p3, N, 1.0, crushed );
            SaturateGrayRamp( sat, N, steps - 1 );
            CrushGrayRamp( crushed, N, N - steps );

            GamutVolumeResult a = ComputeGamutVolume( &sat[0], N, p3 );
            GamutVolumeResult b = ComputeGamutVolume( &crushed[0], N, p3 );
            CPPUNIT_ASSERT( a.valid && b.valid );
            CPPUNIT_ASSERT_DOUBLES_EQUAL( 100.0, a.Coverage(),    0.5 );
            CPPUNIT_ASSERT_DOUBLES_EQUAL( 100.0, a.VolumeRatio(), 0.5 );
            CPPUNIT_ASSERT_DOUBLES_EQUAL( 100.0, b.Coverage(),    0.5 );
            CPPUNIT_ASSERT_DOUBLES_EQUAL( 100.0, b.VolumeRatio(), 0.5 );
        }

        // A ramp with no usable direction at all shares no grid with the
        // reference, so there is no number to publish rather than one computed
        // off a grid that cannot cancel.
        std::vector<ColorXYZ> flat( (size_t)N * N * N, ColorXYZ( 1.0, 1.0, 1.0 ) );
        CPPUNIT_ASSERT( !ComputeGamutVolume( &flat[0], N, p3 ).valid );
    }

    void LabConventionTest()
    {
        // GamutVolume.cpp inlines the L*a*b* conversion instead of calling
        // ColorLab (which copies its CColorReference by value, tens of thousands
        // of times over a cube). Pin the two together: build the smallest
        // possible cube, sum its six Kuhn tetrahedra here using ColorLab, and
        // require the module to agree.
        CColorReference p3(UHDTV);
        std::vector<ColorXYZ> cube;
        MakeIdealCube( p3, 2, 1.0, cube );

        ColorLab lab[8];
        for (int i = 0; i < 8; i++)
            lab[i] = ColorLab( cube[i], kWhiteY, p3 );

        const int tets[6][4] = { {0,1,3,7}, {0,1,5,7}, {0,2,3,7},
                                 {0,2,6,7}, {0,4,5,7}, {0,4,6,7} };
        double expected = 0.0;
        for (int t = 0; t < 6; t++)
        {
            const ColorLab & a = lab[ tets[t][0] ];
            double u[3], v[3], w[3];
            for (int i = 0; i < 3; i++)
            {
                u[i] = lab[ tets[t][1] ][i] - a[i];
                v[i] = lab[ tets[t][2] ][i] - a[i];
                w[i] = lab[ tets[t][3] ][i] - a[i];
            }
            expected += fabs( u[0] * ( v[1]*w[2] - v[2]*w[1] )
                            + u[1] * ( v[2]*w[0] - v[0]*w[2] )
                            + u[2] * ( v[0]*w[1] - v[1]*w[0] ) ) / 6.0;
        }

        GamutVolumeResult r = ComputeGamutVolume( &cube[0], 2, p3 );
        CPPUNIT_ASSERT( expected > 0.0 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( expected, r.measured, expected * 1e-9 );
    }

    void InvalidInputTest()
    {
        CColorReference p3(UHDTV);
        std::vector<ColorXYZ> cube;
        MakeIdealCube( p3, 5, 1.0, cube );

        CPPUNIT_ASSERT( !ComputeGamutVolume( NULL, 5, p3 ).valid );
        CPPUNIT_ASSERT( !ComputeGamutVolume( &cube[0], 1, p3 ).valid );
        // no white corner to anchor the media-relative normalisation to
        std::vector<ColorXYZ> noWhite( cube );
        noWhite[124] = ColorXYZ( 0.0, 0.0, 0.0 );
        CPPUNIT_ASSERT( !ComputeGamutVolume( &noWhite[0], 5, p3 ).valid );

        // a hole anywhere in the cube (stopped or partial capture) yields no number
        cube[37] = ColorXYZ();
        GamutVolumeResult r = ComputeGamutVolume( &cube[0], 5, p3 );
        CPPUNIT_ASSERT( !r.valid );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 0.0, r.Coverage(), 1e-12 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 0.0, r.VolumeRatio(), 1e-12 );
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION( THIS_TEST_CASE );
