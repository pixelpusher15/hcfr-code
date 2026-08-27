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
    // full white measures kWhiteY.
    void MakeIdealCube(const CColorReference & panel, int N, double lumScale,
                       std::vector<ColorXYZ> & out, double gamma = 2.2)
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
                    double b = pow( (double)bi / ( N - 1 ), gamma );
                    out.push_back( ColorXYZ( ( r * R[0] + g * G[0] + b * B[0] ) * norm,
                                             ( r * R[1] + g * G[1] + b * B[1] ) * norm,
                                             ( r * R[2] + g * G[2] + b * B[2] ) * norm ) );
                }
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
