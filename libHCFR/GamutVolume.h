///////////////////////////////////////////////////////////////////////////////
// GamutVolume.h: volumetric gamut coverage (pure geometry, no UI).
//
// The 3D analogue of GamutCoverage.h: instead of intersecting two chromaticity
// TRIANGLES, it intersects two SOLIDS in CIE L*a*b* and reports
//
//   volume   = vol(measured) / vol(reference)                 -- uncapped
//   coverage = vol(measured INTERSECT reference) / vol(reference)  -- capped at 100%
//
// Coverage is the "96% of DCI-P3" figure everyone quotes; volume is what
// separates "bigger gamut" from "better aligned gamut" (a display can read 121%
// volume and 89% coverage at the same time).
//
// The measured solid comes from a display-profile cube capture (CMeasure's
// m_profileMeasureArray): an N^3 grid of RGB drive levels in generation order,
// r slowest and b fastest, exactly as GenerateProfileColors emits it. The cube
// is split into tetrahedra (6 per cell, the standard Kuhn decomposition), which
// makes the volume an exact sum over the piecewise-linear interpolant and needs
// no assumption that the gamut is convex or star-shaped.
//
// Both percentages are ratios against a reference solid tessellated on the SAME
// grid as the measurement (the cube's own gray ramp supplies the steps), so the
// error the straight chords introduce cancels: a display that reproduces its
// reference exactly reads 100.0 / 100.0 at every cube size, not the 99-and-change
// an independently sampled reference produces. Accuracy on cases with a known
// answer -- a synthetic Rec.709 display against a DCI-P3 reference must read
// 66.6% for both metrics, since 709 sits entirely inside P3 -- is within ~0.5
// points at 5^3 and better above it.
//
// Only the two ratios are meaningful; the absolute volumes are estimates on a
// coarse grid, and in HDR they carry whatever normalisation the caller chose.
///////////////////////////////////////////////////////////////////////////////

#ifndef GAMUTVOLUME_H_INCLUDED
#define GAMUTVOLUME_H_INCLUDED

#include "Color.h"

struct GamutVolumeResult
{
    bool   valid;           // false when the cube is absent, short, holed, or
                            // carries no usable gray ramp to share a grid with
    double measured;        // L*a*b* volume of the measured solid
    double reference;       // L*a*b* volume of the reference gamut solid
    double intersection;    // L*a*b* volume of measured INTERSECT reference

    GamutVolumeResult() : valid(false), measured(0.0), reference(0.0), intersection(0.0) {}

    // Percent of the reference gamut's volume. Uncapped: a wide-gamut display
    // measured against a smaller reference reads well over 100.
    double VolumeRatio() const
    {
        return ( valid && reference > 0.0 ) ? 100.0 * measured / reference : 0.0;
    }

    // Percent of the reference gamut actually reproducible, capped at 100 --
    // coverage saturates by definition, and the tiny overshoot a discrete cube
    // can produce for a fully containing display is estimator noise.
    double Coverage() const
    {
        if ( !valid || reference <= 0.0 )
            return 0.0;
        double c = 100.0 * intersection / reference;
        return ( c > 100.0 ) ? 100.0 : c;
    }
};

// Volume/coverage of a measured profile cube against a reference gamut.
//
// Media-relative, and anchored to the cube's OWN white corner rather than to any
// white measured elsewhere in the document: gamut coverage is conventionally a
// question about shape at the medium's own white, so a dim display is not
// penalised for being dim, and an HDR display that clips well below the
// container's peak is judged on its gamut rather than its luminance headroom
// (which every other part of HCFR already reports). Anchoring inside the cube is
// also what lets a flawless display read exactly 100.0 -- a white taken from a
// different measurement would scale the solid slightly and drag it off.
//
// White ERROR still counts: L*a*b* is computed against the reference white's
// chromaticity, exactly as ColorLab does everywhere else in HCFR, so a display
// with an off-target white loses a little coverage.
//
// cube    - cubeN^3 measured XYZ in profile generation order (r slowest, b fastest)
// cubeN   - samples per cube edge (>= 2)
// ref     - reference gamut; for the HDTVa/HDTVb pseudo-spaces substitute a real
//           gamut first (see SpecialModeGamutReference), as the 3D viewer does
GamutVolumeResult ComputeGamutVolume(const ColorXYZ * cube, int cubeN,
                                     const CColorReference & ref);

// L*a*b* volume of a reference gamut on its own (exposed for tests).
double ReferenceGamutVolume(const CColorReference & ref);

#endif // GAMUTVOLUME_H_INCLUDED
