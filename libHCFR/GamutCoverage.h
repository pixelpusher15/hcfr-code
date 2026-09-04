///////////////////////////////////////////////////////////////////////////////
// GamutCoverage.h: gamut coverage computation (pure geometry, no UI).
//
// Coverage = area(measured triangle intersect reference triangle) / area(reference
// triangle), computed in a 2D chromaticity plane (CIE 1931 xy or CIE 1976
// u'v').  This is the industry "P3 coverage 96%" style metric: it saturates
// at 1.0 and penalizes misaligned primaries even when the measured gamut is
// larger than the reference.
///////////////////////////////////////////////////////////////////////////////

#ifndef GAMUTCOVERAGE_H_INCLUDED
#define GAMUTCOVERAGE_H_INCLUDED

#include "Color.h"
#include <vector>

enum GamutPlane
{
    GAMUT_PLANE_XY,     // CIE 1931 xy
    GAMUT_PLANE_UV      // CIE 1976 u'v'
};

// CIE 1931 xy -> CIE 1976 u'v'
void xyToUv(double x, double y, double & u, double & v);

// Coverage of the reference triangle by the measured triangle in the given
// plane.  Both arrays are the R, G, B primaries as xyY (Y is ignored).
// Returns a value in [0, 1].  Returns 0 if either triangle is degenerate.
double GamutCoverage(const ColorxyY measured[3], const ColorxyY reference[3], GamutPlane plane);

// A single point in an already-projected 2D plane. Used for planes this
// module has no business knowing how to compute (e.g. a*b*, whose
// projection needs a white point and reference standard) -- the caller
// supplies the coordinates, this module only does the geometry.
struct GamutPoint
{
    double x;
    double y;
};

// General coverage metric for the CIE a*b* view: the a*/b* hexagon formed by
// R,Y,G,C,B,M (in that hue order, or any order -- winding is corrected
// internally) is far more sensitive to a pure luminance shortfall than the
// R/G/B-only triangle GamutCoverage() above uses. That's intentional: L*
// doesn't appear as a plotted axis in this chart, but it still feeds a* and
// b* through the shared f(Y/Yn) term in the Lab formulas, so a channel that
// loses luminance at an unchanged xy chromaticity visibly compresses toward
// the neutral point on this hexagon even though it wouldn't move at all on
// the xy or u'v' triangle. Whereas a 3-point triangle only has R,G,B to work
// with, and dilutes a single bad channel's effect across just three
// vertices, the 6-point hexagon lets each primary/secondary's own chroma
// loss show up directly, undiluted -- which is the point of using it here.
//
// Unlike GamutCoverage() (which assumes its 3-point arrays are already a
// valid triangle), this takes the convex hull of each input internally --
// a hexagon of real display secondaries is not guaranteed to be convex
// (e.g. an undersaturated cyan can sit inside the chord between green and
// blue), and the clipping algorithm requires a convex clip polygon to be
// well defined. Point count need not match between measured/reference and
// need not be pre-sorted.
//
// Returns a value in [0, 1]. Returns 0 if either hull collapses to fewer
// than 3 distinct vertices (degenerate/collinear input).
double GamutCoveragePolygon(const std::vector<GamutPoint> & measured, const std::vector<GamutPoint> & reference);

#endif // GAMUTCOVERAGE_H_INCLUDED
