///////////////////////////////////////////////////////////////////////////////
// GamutCoverage.cpp: gamut coverage computation (see GamutCoverage.h).
///////////////////////////////////////////////////////////////////////////////

#include "GamutCoverage.h"

#include <math.h>
#include <vector>
#include <algorithm>

void xyToUv(double x, double y, double & u, double & v)
{
    double d = -2.0 * x + 12.0 * y + 3.0;
    if (d == 0.0)
    {
        u = 0.0;
        v = 0.0;
        return;
    }
    u = (4.0 * x) / d;
    v = (9.0 * y) / d;
}

namespace
{
    struct Pt
    {
        double x;
        double y;
    };

    // Twice the signed area of the polygon (shoelace).  Positive when the
    // vertices are counter-clockwise.
    double signedArea2(const std::vector<Pt> & poly)
    {
        double a = 0.0;
        size_t n = poly.size();
        for (size_t i = 0; i < n; i++)
        {
            const Pt & p = poly[i];
            const Pt & q = poly[(i + 1) % n];
            a += p.x * q.y - q.x * p.y;
        }
        return a;
    }

    // Cross product (b-a) x (p-a): positive when p is left of a->b.
    double cross(const Pt & a, const Pt & b, const Pt & p)
    {
        return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
    }

    Pt intersect(const Pt & p, const Pt & q, const Pt & a, const Pt & b)
    {
        double d1 = cross(a, b, p);
        double d2 = cross(a, b, q);
        double t = d1 / (d1 - d2);
        Pt r = { p.x + t * (q.x - p.x), p.y + t * (q.y - p.y) };
        return r;
    }

    // Sutherland-Hodgman: clip a polygon against a convex, CCW-wound clip
    // polygon of any vertex count (a triangle for the xy/u'v' path, a hull
    // of arbitrary size for the a*b* path).
    std::vector<Pt> clipPolygon(std::vector<Pt> poly, const std::vector<Pt> & clip)
    {
        size_t nClip = clip.size();
        for (size_t e = 0; e < nClip && !poly.empty(); e++)
        {
            const Pt & a = clip[e];
            const Pt & b = clip[(e + 1) % nClip];
            std::vector<Pt> out;
            size_t n = poly.size();
            for (size_t i = 0; i < n; i++)
            {
                const Pt & p = poly[i];
                const Pt & q = poly[(i + 1) % n];
                bool pIn = cross(a, b, p) >= 0.0;
                bool qIn = cross(a, b, q) >= 0.0;
                if (pIn)
                {
                    out.push_back(p);
                    if (!qIn)
                        out.push_back(intersect(p, q, a, b));
                }
                else if (qIn)
                {
                    out.push_back(intersect(p, q, a, b));
                }
            }
            poly.swap(out);
        }
        return poly;
    }

    Pt project(const ColorxyY & c, GamutPlane plane)
    {
        Pt p = { c[0], c[1] };
        if (plane == GAMUT_PLANE_UV)
            xyToUv(c[0], c[1], p.x, p.y);
        return p;
    }

    // Ensure counter-clockwise winding, for a polygon of any vertex count.
    void makeCCW(std::vector<Pt> & poly)
    {
        if (signedArea2(poly) < 0.0)
            std::reverse(poly.begin(), poly.end());
    }

    bool ptLess(const Pt & a, const Pt & b)
    {
        return (a.x < b.x) || (a.x == b.x && a.y < b.y);
    }

    // Andrew's monotone chain: convex hull of an arbitrary point set, CCW
    // wound, duplicate/collinear points on an edge dropped. Needed because a
    // hexagon of real display primaries+secondaries is not guaranteed
    // convex on its own (e.g. an undersaturated secondary can sit inside the
    // chord between its two neighbors), and clipPolygon's clip argument must
    // be a valid convex polygon.
    std::vector<Pt> convexHull(std::vector<Pt> pts)
    {
        size_t n = pts.size();
        if (n < 3)
            return std::vector<Pt>();

        std::sort(pts.begin(), pts.end(), ptLess);
        pts.erase(std::unique(pts.begin(), pts.end(),
            [](const Pt & a, const Pt & b) { return a.x == b.x && a.y == b.y; }), pts.end());
        n = pts.size();
        if (n < 3)
            return std::vector<Pt>();

        std::vector<Pt> hull(2 * n);
        int k = 0;
        for (size_t i = 0; i < n; i++)  // lower chain
        {
            while (k >= 2 && cross(hull[k-2], hull[k-1], pts[i]) <= 0.0)
                k--;
            hull[k++] = pts[i];
        }
        int lower = k + 1;
        for (int i = (int)n - 2; i >= 0; i--)  // upper chain
        {
            while (k >= lower && cross(hull[k-2], hull[k-1], pts[i]) <= 0.0)
                k--;
            hull[k++] = pts[i];
        }
        hull.resize(k - 1);  // last point == first point, drop the dup
        return hull;
    }
}

double GamutCoverage(const ColorxyY measured[3], const ColorxyY reference[3], GamutPlane plane)
{
    std::vector<Pt> meas(3), ref(3);
    for (int i = 0; i < 3; i++)
    {
        meas[i] = project(measured[i], plane);
        ref[i] = project(reference[i], plane);
    }
    makeCCW(meas);
    makeCCW(ref);

    // Chromaticity coordinates are O(1), so their doubled triangle areas are
    // well above this floor unless the triangle is genuinely degenerate.
    const double kAreaEpsilon = 1e-12;
    double refArea2 = signedArea2(ref);
    if (refArea2 <= kAreaEpsilon || fabs(signedArea2(meas)) <= kAreaEpsilon)
        return 0.0;

    std::vector<Pt> inter = clipPolygon(meas, ref);
    if (inter.size() < 3)
        return 0.0;

    return fabs(signedArea2(inter)) / refArea2;
}

double GamutCoveragePolygon(const std::vector<GamutPoint> & measured, const std::vector<GamutPoint> & reference)
{
    std::vector<Pt> measPts(measured.size()), refPts(reference.size());
    for (size_t i = 0; i < measured.size(); i++)
        measPts[i] = Pt{ measured[i].x, measured[i].y };
    for (size_t i = 0; i < reference.size(); i++)
        refPts[i] = Pt{ reference[i].x, reference[i].y };

    std::vector<Pt> measHull = convexHull(measPts);
    std::vector<Pt> refHull = convexHull(refPts);
    if (measHull.size() < 3 || refHull.size() < 3)
        return 0.0;

    makeCCW(measHull);
    makeCCW(refHull);

    const double kAreaEpsilon = 1e-12;
    double refArea2 = signedArea2(refHull);
    if (refArea2 <= kAreaEpsilon || fabs(signedArea2(measHull)) <= kAreaEpsilon)
        return 0.0;

    std::vector<Pt> inter = clipPolygon(measHull, refHull);
    if (inter.size() < 3)
        return 0.0;

    return fabs(signedArea2(inter)) / refArea2;
}
