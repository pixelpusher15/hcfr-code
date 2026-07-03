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

#endif // GAMUTCOVERAGE_H_INCLUDED
