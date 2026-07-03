#include "GamutCoverage.h"
#include <math.h>
#include <cppunit/config/SourcePrefix.h>
#include <cppunit/extensions/HelperMacros.h>

#define THIS_TEST_CASE GamutCoverageTestCase

namespace
{
    // Reference primaries (xy), matching libHCFR/Color.cpp
    const ColorxyY rec709[3]  = { ColorxyY(0.6400, 0.3300), ColorxyY(0.3000, 0.6000), ColorxyY(0.1500, 0.0600) };
    const ColorxyY p3[3]      = { ColorxyY(0.6800, 0.3200), ColorxyY(0.2650, 0.6900), ColorxyY(0.1500, 0.0600) };
    const ColorxyY rec2020[3] = { ColorxyY(0.7080, 0.2920), ColorxyY(0.1700, 0.7970), ColorxyY(0.1310, 0.0460) };
}

class THIS_TEST_CASE : public CPPUNIT_NS::TestFixture
{
    CPPUNIT_TEST_SUITE(THIS_TEST_CASE);
    CPPUNIT_TEST( SelfCoverageTest );
    CPPUNIT_TEST( ContainmentTest );
    CPPUNIT_TEST( PartialOverlapTest );
    CPPUNIT_TEST( DegenerateTest );
    CPPUNIT_TEST( WindingInvarianceTest );
    CPPUNIT_TEST( UvPlaneTest );
    CPPUNIT_TEST( xyToUvTest );
    CPPUNIT_TEST_SUITE_END();

protected:

    void SelfCoverageTest()
    {
        // a gamut covers itself exactly, in both planes
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 1.0, GamutCoverage(rec709, rec709, GAMUT_PLANE_XY), 1e-12 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 1.0, GamutCoverage(p3, p3, GAMUT_PLANE_XY), 1e-12 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 1.0, GamutCoverage(rec2020, rec2020, GAMUT_PLANE_UV), 1e-12 );
    }

    void ContainmentTest()
    {
        // Rec.709 is entirely inside Rec.2020: 2020 fully covers 709...
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 1.0, GamutCoverage(rec2020, rec709, GAMUT_PLANE_XY), 1e-9 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 1.0, GamutCoverage(rec2020, rec709, GAMUT_PLANE_UV), 1e-9 );
        // ...but 709 covers well under 100% of 2020, and P3 sits in between
        double c709 = GamutCoverage(rec709, rec2020, GAMUT_PLANE_XY);
        double cP3  = GamutCoverage(p3, rec2020, GAMUT_PLANE_XY);
        CPPUNIT_ASSERT( c709 > 0.30 && c709 < 0.60 );
        CPPUNIT_ASSERT( cP3 > c709 && cP3 < 1.0 );
    }

    void PartialOverlapTest()
    {
        // right triangle with legs 1, clipped by its own mirror shifted so the
        // intersection is analytic: clip square-ish case via known triangles
        const ColorxyY tri[3]     = { ColorxyY(0.0, 0.0), ColorxyY(1.0, 0.0), ColorxyY(0.0, 1.0) };
        const ColorxyY shifted[3] = { ColorxyY(0.5, 0.0), ColorxyY(1.5, 0.0), ColorxyY(0.5, 1.0) };
        // intersection of tri and shifted: triangle (0.5,0),(1,0),(0.5,0.5) with area 0.125
        // reference area = 0.5, so coverage = 0.25
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 0.25, GamutCoverage(shifted, tri, GAMUT_PLANE_XY), 1e-12 );
        // and disjoint triangles give zero
        const ColorxyY disjoint[3] = { ColorxyY(5.0, 5.0), ColorxyY(6.0, 5.0), ColorxyY(5.0, 6.0) };
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 0.0, GamutCoverage(disjoint, tri, GAMUT_PLANE_XY), 1e-12 );
    }

    void DegenerateTest()
    {
        // collinear measured primaries -> zero coverage, no crash
        const ColorxyY line[3] = { ColorxyY(0.1, 0.1), ColorxyY(0.2, 0.2), ColorxyY(0.3, 0.3) };
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 0.0, GamutCoverage(line, rec709, GAMUT_PLANE_XY), 1e-12 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 0.0, GamutCoverage(rec709, line, GAMUT_PLANE_XY), 1e-12 );
    }

    void WindingInvarianceTest()
    {
        // vertex order must not matter
        const ColorxyY swapped[3] = { ColorxyY(0.1500, 0.0600), ColorxyY(0.3000, 0.6000), ColorxyY(0.6400, 0.3300) };
        CPPUNIT_ASSERT_DOUBLES_EQUAL( GamutCoverage(rec709, rec2020, GAMUT_PLANE_XY),
                                      GamutCoverage(swapped, rec2020, GAMUT_PLANE_XY), 1e-12 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 1.0, GamutCoverage(swapped, rec709, GAMUT_PLANE_XY), 1e-12 );
    }

    void UvPlaneTest()
    {
        // xy and u'v' coverages differ for non-trivial pairs (sanity that the
        // plane parameter actually changes the projection)
        double xy = GamutCoverage(rec709, rec2020, GAMUT_PLANE_XY);
        double uv = GamutCoverage(rec709, rec2020, GAMUT_PLANE_UV);
        CPPUNIT_ASSERT( uv > 0.0 && uv < 1.0 );
        CPPUNIT_ASSERT( fabs(xy - uv) > 0.01 );
    }

    void xyToUvTest()
    {
        // D65 white: x=0.3127, y=0.3290 -> u'=0.1978, v'=0.4683
        double u, v;
        xyToUv(0.3127, 0.3290, u, v);
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 0.1978, u, 5e-5 );
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 0.4683, v, 5e-5 );
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(THIS_TEST_CASE);
