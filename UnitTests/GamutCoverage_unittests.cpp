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

    // a*/b* hexagons (R,Y,G,C,B,M) from a real measured display against a
    // Rec.2020 a*/b* target, and the same measured hexagon after simulating
    // every channel's luminance being cut (xy chromaticity held fixed) --
    // both cross-checked against an independent Python/NumPy Lab-conversion
    // pipeline before being ported in here.
    const GamutPoint measuredAbBaseline[6] = {
        { 115.67,  94.40 },   // R
        { -20.97, 121.49 },   // Y
        {-169.95, 104.04 },   // G
        { -96.50, -26.38 },   // C
        {  86.73,-116.54 },   // B
        { 134.05, -67.69 },   // M
    };
    const GamutPoint targetAbRec2020[6] = {
        { 117.33, 100.50 },   // R
        { -21.48, 136.89 },   // Y
        {-172.32, 116.62 },   // G
        {-106.24, -19.32 },   // C
        {  86.11,-120.27 },   // B
        { 130.53, -61.18 },   // M
    };
    const GamutPoint measuredAbAllChannelsDimmed[6] = {
        {  87.2,  66.8 },     // R
        { -16.6,  96.1 },     // Y
        {-135.1,  82.7 },     // G
        { -88.1, -24.1 },     // C
        {  66.9, -89.9 },     // B
        { 108.1, -54.6 },     // M
    };
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
    CPPUNIT_TEST( AbHexagonSelfCoverageTest );
    CPPUNIT_TEST( AbHexagonBaselineTest );
    CPPUNIT_TEST( AbHexagonLuminanceLossTest );
    CPPUNIT_TEST( AbHexagonWindingInvarianceTest );
    CPPUNIT_TEST( AbHexagonNonConvexInputTest );
    CPPUNIT_TEST( AbHexagonDegenerateTest );
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

    // --- a*b* hexagon path (GamutCoveragePolygon) ---

    void AbHexagonSelfCoverageTest()
    {
        // a hexagon covers itself exactly
        std::vector<GamutPoint> target(targetAbRec2020, targetAbRec2020 + 6);
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 1.0, GamutCoveragePolygon(target, target), 1e-9 );
    }

    void AbHexagonBaselineTest()
    {
        // Real measured-display-vs-Rec.2020 a*b* hexagon: cross-checked
        // against an independent Python/NumPy Lab pipeline, ~92-93% (the
        // convex-hull-based area here can differ by ~1pt from a strict
        // hue-ordered-polygon area if the hexagon isn't perfectly convex --
        // this is a coarse sanity range, not a bit-exact regression check).
        std::vector<GamutPoint> meas(measuredAbBaseline, measuredAbBaseline + 6);
        std::vector<GamutPoint> target(targetAbRec2020, targetAbRec2020 + 6);
        double cov = GamutCoveragePolygon(meas, target);
        CPPUNIT_ASSERT( cov > 0.88 && cov < 0.95 );
    }

    void AbHexagonLuminanceLossTest()
    {
        // The whole point of this metric: cutting every channel's luminance
        // (xy chromaticity held fixed) collapses a*b* coverage hard, where
        // the xy/u'v' triangle metric wouldn't move at all for the same
        // input. Matches the independent Python validation almost exactly.
        std::vector<GamutPoint> measDimmed(measuredAbAllChannelsDimmed, measuredAbAllChannelsDimmed + 6);
        std::vector<GamutPoint> target(targetAbRec2020, targetAbRec2020 + 6);
        double cov = GamutCoveragePolygon(measDimmed, target);
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 0.591, cov, 0.01 );
    }

    void AbHexagonWindingInvarianceTest()
    {
        // vertex order must not matter, same as the triangle path
        std::vector<GamutPoint> meas(measuredAbBaseline, measuredAbBaseline + 6);
        std::vector<GamutPoint> target(targetAbRec2020, targetAbRec2020 + 6);
        std::vector<GamutPoint> reversed(meas.rbegin(), meas.rend());
        CPPUNIT_ASSERT_DOUBLES_EQUAL( GamutCoveragePolygon(meas, target),
                                      GamutCoveragePolygon(reversed, target), 1e-9 );
    }

    void AbHexagonNonConvexInputTest()
    {
        // An undersaturated secondary sitting inside the chord between its
        // two neighbors (i.e. the raw hue-ordered hexagon is NOT convex on
        // its own) must not crash or silently misbehave -- this is exactly
        // why GamutCoveragePolygon takes a convex hull internally rather
        // than assuming its input already is one.
        GamutPoint nonConvex[6] = {
            { 100.0,   0.0 },   // R
            {  20.0,  60.0 },   // Y
            { -80.0, 100.0 },   // G
            { -10.0,  10.0 },   // C -- pulled in, inside the G-B chord
            { -80.0,-100.0 },   // B
            {  20.0, -60.0 },   // M
        };
        std::vector<GamutPoint> meas(nonConvex, nonConvex + 6);
        std::vector<GamutPoint> target(targetAbRec2020, targetAbRec2020 + 6);
        double cov = GamutCoveragePolygon(meas, target);
        CPPUNIT_ASSERT( cov >= 0.0 );  // no crash, no negative/garbage result
    }

    void AbHexagonDegenerateTest()
    {
        // collinear measured hexagon -> zero coverage, no crash
        GamutPoint line[6] = {
            { 0.0, 0.0 }, { 1.0, 1.0 }, { 2.0, 2.0 },
            { 3.0, 3.0 }, { 4.0, 4.0 }, { 5.0, 5.0 },
        };
        std::vector<GamutPoint> meas(line, line + 6);
        std::vector<GamutPoint> target(targetAbRec2020, targetAbRec2020 + 6);
        CPPUNIT_ASSERT_DOUBLES_EQUAL( 0.0, GamutCoveragePolygon(meas, target), 1e-12 );
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(THIS_TEST_CASE);
