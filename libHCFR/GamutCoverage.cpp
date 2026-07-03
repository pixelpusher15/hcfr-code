///////////////////////////////////////////////////////////////////////////////
// GamutCoverage.cpp: gamut coverage computation (see GamutCoverage.h).
///////////////////////////////////////////////////////////////////////////////

#include "GamutCoverage.h"

#include <math.h>
#include <vector>

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
    // triangle.
    std::vector<Pt> clipPolygon(std::vector<Pt> poly, const Pt clip[3])
    {
        for (int e = 0; e < 3 && !poly.empty(); e++)
        {
            const Pt & a = clip[e];
            const Pt & b = clip[(e + 1) % 3];
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

    // Ensure counter-clockwise winding.
    void makeCCW(std::vector<Pt> & tri)
    {
        if (signedArea2(tri) < 0.0)
        {
            Pt tmp = tri[1];
            tri[1] = tri[2];
            tri[2] = tmp;
        }
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

    Pt clip[3] = { ref[0], ref[1], ref[2] };
    std::vector<Pt> inter = clipPolygon(meas, clip);
    if (inter.size() < 3)
        return 0.0;

    return fabs(signedArea2(inter)) / refArea2;
}
