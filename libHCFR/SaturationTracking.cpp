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

// SaturationTracking.cpp: see SaturationTracking.h.

#include "SaturationTracking.h"

#include <math.h>

static const double kPi = 3.14159265358979323846;
static const double kOnWhite = 1e-9;

bool SatTrackXYZToUv(double X, double Y, double Z, double & u, double & v)
{
	double d = X + 15.0 * Y + 3.0 * Z;
	if ( !( d > 0.0 ) )
	{
		u = v = 0.0;
		return false;
	}
	u = 4.0 * X / d;
	v = 9.0 * Y / d;
	return true;
}

bool SatTrackUvYToXYZ(double u, double v, double Y, double & X, double & Yout, double & Z)
{
	if ( !( v > 0.0 ) )
	{
		X = Yout = Z = 0.0;
		return false;
	}
	X = Y * 9.0 * u / ( 4.0 * v );
	Yout = Y;
	Z = Y * ( 12.0 - 3.0 * u - 20.0 * v ) / ( 4.0 * v );
	return true;
}

double SatTrackHueErrorDeg(double uMeas, double vMeas, double uTarget, double vTarget,
						   double uWhite, double vWhite)
{
	double mu = uMeas - uWhite, mv = vMeas - vWhite;
	double tu = uTarget - uWhite, tv = vTarget - vWhite;
	if ( ( mu == 0.0 && mv == 0.0 ) || ( tu == 0.0 && tv == 0.0 ) )
		return 0.0;
	// Signed angle from the target direction to the measured direction:
	// atan2(cross, dot) is already in -pi..pi, so no separate wrap is needed.
	double cross = tu * mv - tv * mu;
	double dot = tu * mu + tv * mv;
	return atan2 ( cross, dot ) * 180.0 / kPi;
}

double SatTrackRadialErrorPct(double uMeas, double vMeas, double uTarget, double vTarget,
							  double uWhite, double vWhite)
{
	double rm = sqrt ( ( uMeas - uWhite ) * ( uMeas - uWhite ) + ( vMeas - vWhite ) * ( vMeas - vWhite ) );
	double rt = sqrt ( ( uTarget - uWhite ) * ( uTarget - uWhite ) + ( vTarget - vWhite ) * ( vTarget - vWhite ) );
	if ( !( rt > 0.0 ) )
		return 0.0;
	return 100.0 * ( rm / rt - 1.0 );
}

double SatTrackLuminanceErrorPct(double yMeas, double yTarget)
{
	if ( !( yTarget > 0.0 ) )
		return 0.0;
	return 100.0 * ( yMeas / yTarget - 1.0 );
}

SatTrackAnchor SatTrackComputeAnchor(const double measXYZ[3], const double targetXYZ[3],
									 double uWhite, double vWhite)
{
	SatTrackAnchor a;
	a.uWhite = uWhite;
	a.vWhite = vWhite;

	double um, vm, ut, vt;
	if ( !SatTrackXYZToUv ( measXYZ[0], measXYZ[1], measXYZ[2], um, vm ) )
		return a;
	if ( !SatTrackXYZToUv ( targetXYZ[0], targetXYZ[1], targetXYZ[2], ut, vt ) )
		return a;
	// "On the white" within kOnWhite in u'v': far below any patch a sweep
	// anchors on (1 percent saturation is ~1e-3), but large enough that a
	// round-off-sized offset cannot set a hue knob to an arbitrary angle
	if ( hypot ( ut - uWhite, vt - vWhite ) < kOnWhite )
		return a;					// target on the white: no hue, no radius
	if ( hypot ( um - uWhite, vm - vWhite ) < kOnWhite )
		return a;					// measured on the white: no radius to scale
	if ( !( measXYZ[1] > 0.0 ) || !( targetXYZ[1] > 0.0 ) )
		return a;

	a.hueDeg = SatTrackHueErrorDeg ( um, vm, ut, vt, uWhite, vWhite );
	a.satPct = SatTrackRadialErrorPct ( um, vm, ut, vt, uWhite, vWhite );
	a.lumPct = SatTrackLuminanceErrorPct ( measXYZ[1], targetXYZ[1] );
	a.valid = true;
	return a;
}

bool SatTrackApplyAnchor(const SatTrackAnchor & anchor, const double inXYZ[3], double outXYZ[3])
{
	outXYZ[0] = inXYZ[0];
	outXYZ[1] = inXYZ[1];
	outXYZ[2] = inXYZ[2];
	if ( !anchor.valid )
		return true;

	double u, v;
	if ( !SatTrackXYZToUv ( inXYZ[0], inXYZ[1], inXYZ[2], u, v ) )
		return false;

	// Rotate (u, v) - white by -hueDeg, then scale the radius
	double a = -anchor.hueDeg * kPi / 180.0;
	double c = cos ( a ), s = sin ( a );
	double du = u - anchor.uWhite, dv = v - anchor.vWhite;
	double k = 1.0 / ( 1.0 + anchor.satPct / 100.0 );
	double ru = ( du * c - dv * s ) * k;
	double rv = ( du * s + dv * c ) * k;

	double Y = inXYZ[1] / ( 1.0 + anchor.lumPct / 100.0 );
	double X2, Y2, Z2;
	if ( !SatTrackUvYToXYZ ( anchor.uWhite + ru, anchor.vWhite + rv, Y, X2, Y2, Z2 ) )
		return false;
	// A sweep end that already sits on the spectrum locus (yellow and cyan in
	// BT.2020 have Z close to 0) can be pushed past it by the radial scale or
	// the rotation: no display can show that, and the dE formulas are not
	// defined for a negative tristimulus value. Report it instead. A target ON
	// the locus (BT.2020 red has Z = 0) comes back with a round-off-sized
	// negative component; that is the target itself, so it is clamped to 0
	// rather than refused.
	const double tol = 1e-9 * Y2;
	if ( Y2 < 0.0 || X2 < -tol || Z2 < -tol )
		return false;
	if ( X2 < 0.0 )
		X2 = 0.0;
	if ( Z2 < 0.0 )
		Z2 = 0.0;
	outXYZ[0] = X2;
	outXYZ[1] = Y2;
	outXYZ[2] = Z2;
	return true;
}
