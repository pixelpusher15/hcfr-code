/////////////////////////////////////////////////////////////////////////////
//
//  This file is subject to the terms of the GNU General Public License as
//  published by the Free Software Foundation.  A copy of this license is
//  included with this software distribution in the file COPYING.htm. If you
//  do not have a copy, you may obtain a copy by writing to the Free
//  Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  This software is distributed in the hope that it is useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details
/////////////////////////////////////////////////////////////////////////////

// SaturationTracking: the per-step error quantities of a saturation sweep and
// the anchor-preview transform used by the "Saturation - shifts" view. Pure
// math on plain doubles (no MFC, no CColor) so the console harness can test it.
//
// Plane: CIE 1976 u'v' (u' = 4X / (X + 15Y + 3Z), v' = 9Y / (X + 15Y + 3Z)).
// Every angle and radius is taken about the sweep's white, i.e. the 0 percent
// reference of that sweep, which the caller supplies.
//
// The anchor transform is a FIRST-ORDER model of a display's CMS controls for
// one color: a hue knob that rotates the chromaticity about the white, a
// saturation knob that scales its distance from the white, and a luminance
// knob that scales Y. The knob settings are chosen so the anchor step lands
// exactly on its target; every other step of the sweep receives the same
// rotation and the same two scale factors. Real CMS controls are not exactly
// this (they act in the display's own encoding and fall off toward the white),
// so the predicted sweep is a preview, not a promise.

#ifndef SATURATIONTRACKING_H_INCLUDED
#define SATURATIONTRACKING_H_INCLUDED

// XYZ -> u'v'. Returns false (and u = v = 0) when X + 15Y + 3Z <= 0.
bool SatTrackXYZToUv(double X, double Y, double Z, double & u, double & v);

// u'v' + Y -> XYZ. Returns false (and X = Y = Z = 0) when v <= 0.
bool SatTrackUvYToXYZ(double u, double v, double Y, double & X, double & Yout, double & Z);

// Signed hue error in degrees: the angle of (measured - white) minus the angle
// of (target - white), in u'v', wrapped to -180..180. Positive = the measured
// point sits counter-clockwise of its target as seen from the white. Returns 0
// when either point coincides with the white (no hue is defined there).
double SatTrackHueErrorDeg(double uMeas, double vMeas, double uTarget, double vTarget,
						   double uWhite, double vWhite);

// Radial saturation error in percent: 100 * (|measured - white| /
// |target - white| - 1) in u'v'. Returns 0 when the target coincides with the
// white.
double SatTrackRadialErrorPct(double uMeas, double vMeas, double uTarget, double vTarget,
							  double uWhite, double vWhite);

// Luminance error in percent: 100 * (yMeas / yTarget - 1). Both in the same
// units. Returns 0 when yTarget <= 0.
double SatTrackLuminanceErrorPct(double yMeas, double yTarget);

// The three errors measured at the anchor step, and the white they refer to.
struct SatTrackAnchor
{
	bool	valid;		// false: the transform is the identity
	double	hueDeg;		// SatTrackHueErrorDeg at the anchor
	double	satPct;		// SatTrackRadialErrorPct at the anchor
	double	lumPct;		// SatTrackLuminanceErrorPct at the anchor
	double	uWhite, vWhite;

	SatTrackAnchor() : valid(false), hueDeg(0.0), satPct(0.0), lumPct(0.0), uWhite(0.0), vWhite(0.0) {}
};

// Errors of the anchor step. measXYZ and targetXYZ must be in the same units.
// Invalid (identity) when either point has no chromaticity, either point sits
// on the white (closer than 1e-9 in u'v'), or either has zero Y
// (no finite knob setting could then reach the target).
SatTrackAnchor SatTrackComputeAnchor(const double measXYZ[3], const double targetXYZ[3],
									 double uWhite, double vWhite);

// Applies the inverse of the anchor errors to one measured color: rotates its
// u'v' about the white by -hueDeg, scales its distance from the white by
// 1 / (1 + satPct / 100) and its Y by 1 / (1 + lumPct / 100). An invalid anchor
// copies the input unchanged. Returns false (output = input) when the input
// has no chromaticity, or when the corrected color would have a negative X, Y
// or Z - a point pushed past the spectrum locus, which no display can show;
// the caller should then treat that step as having no prediction. X or Z
// within 1e-9 * Y below 0 is round-off on a locus target and is clamped to 0.
bool SatTrackApplyAnchor(const SatTrackAnchor & anchor, const double inXYZ[3], double outXYZ[3]);

#endif // SATURATIONTRACKING_H_INCLUDED
