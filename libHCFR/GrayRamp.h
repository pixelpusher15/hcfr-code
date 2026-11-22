/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2012 Association Homecinema Francophone.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////
//
//  This file is subject to the terms of the GNU General Public License as
//  published by the Free Software Foundation.  A copy of this license is
//  included with this software distribution in the file COPYING.htm. If you
//  do not have a copy, you may obtain a copy by writing to the Free
//  Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details
/////////////////////////////////////////////////////////////////////////////
//  Author(s):
//  PhoenixF15E
/////////////////////////////////////////////////////////////////////////////

// GrayRamp.h: the gray readings a gray graph draws, one point per signal
// level, whatever the readings came from. Pure C++, no MFC.
//
// Two sources fill it:
//
//   The gray sweep (BuildSweepGrayRamp): slot i of the sweep is point i, in
//   slot order, with exactly the level, proportion and slot proportion the
//   GraySeries helpers in Color.h give it. Slot 0 is the black and the last
//   slot the white, as the sweep has always had them. A view that walks this
//   ramp draws the sweep exactly as it drew it from GetGray ( i ).
//
//   A patch list (BuildListGrayRamp): the rows labeled gray (PatchLabel.h),
//   from the row's codes and never its name. Rows that drive the same code
//   (after the clamp the list's range applies) are one signal: their valid
//   readings are averaged into one point. Points are sorted by level, so the
//   order of the rows does not matter, and every point sits at its own level:
//   the reading's stamped wire drive when every reading of the point has the
//   same one, else the level the list's code decodes to. Black and white are
//   the grays at exactly 0% and 100% of the list's nominal range. A gray below
//   0% or above 100% (an extended-range list) is kept, with a proportion
//   outside 0..1, but is neither black nor white.
//
// Which source a document's graphs use is CMeasure::GetGrayGraphRamp: the
// sweep when it holds a reading, else the active list when it has a measured
// gray, else the sweep (ChooseGrayRampSource).

#if !defined(GRAYRAMP_H_INCLUDED_)
#define GRAYRAMP_H_INCLUDED_

#include "Color.h"
#include <vector>

enum GrayRampSource
{
	GRAY_RAMP_SWEEP = 0,
	GRAY_RAMP_LIST  = 1
};

struct GrayRampPoint
{
	CColor reading;       // sweep: slot i as stored; list: the average of the
	                      // point's readings (AverageGrayReadings)
	double level;         // percent: the x the charts draw it at
	double prop;          // 0..1: what the reference and the fit evaluate
	double nominalProp;   // the level asked for, 0..1: sweep GraySeriesSlotProp,
	                      // list the code's level / 100
	bool   stamped;       // prop is a stamped wire drive (ReadingWireGrayProp)
	bool   isBlack;       // sweep: slot 0; list: the gray at exactly 0%
	bool   isWhite;       // sweep: the last slot; list: the gray at exactly 100%
	int    members;       // readings averaged into the point (sweep: 1)
	int    firstRow;      // sweep: the slot; list: the lowest row of the point
	GrayRampPoint() : level(0.0), prop(0.0), nominalProp(0.0), stamped(false),
	                  isBlack(false), isWhite(false), members(0), firstRow(-1) {}
};

struct GrayRamp
{
	GrayRampSource             source;
	std::vector<GrayRampPoint> points;
	int                        blackIndex;	// the point with isBlack, or -1
	int                        whiteIndex;	// the point with isWhite, or -1
	GrayRamp() : source(GRAY_RAMP_SWEEP), blackIndex(-1), whiteIndex(-1) {}
};

// The sweep, slot for slot (see above). An empty series gives no points.
void BuildSweepGrayRamp ( const GraySeriesView & v, GrayRamp * out );

// Whether a patch-list row is drawn on the gray graphs: labeled gray, with
// the role a CSV leaves (-1), COLOR or VERIFICATION. Anchors, warm-up
// patches, interstitials and identification patches are not.
bool GrayRampRowCounts ( const CsvPatchRow & row );

// The list's gray points: rows[i] paired with readings[i], for i below both
// counts; a row counts when GrayRampRowCounts says so and its reading is
// valid. A gray with no valid reading has no point.
void BuildListGrayRamp ( const CsvPatchRow * rows, int nRows, const CColor * readings, int nReadings, GrayRamp * out );

// The point one signal's readings make. One reading: that reading, unchanged.
// More: XYZ is the mean; lux and the raw XYZ are means when every reading has
// one and absent otherwise; the wire setup is kept when every reading has the
// same one; the stimulus and provenance are first's (first must be one of
// members). Spectrum and timestamp are dropped. The readings are summed in
// order of value, so the order they are handed over in cannot change a bit.
CColor AverageGrayReadings ( const CColor * const * members, int n, const CColor * first );

// A white a chart may divide by: valid, with a finite Y above 0 (the test
// GrayScaleWhite applies).
bool GrayRampWhiteUsable ( const CColor & c );

// The white point's reading when GrayRampWhiteUsable, else fallback.
CColor GrayRampWhite ( const GrayRamp & r, const CColor & fallback );

// The black point's reading when it is valid with a finite Y (the test
// GrayScaleBlack applies), else fallback.
CColor GrayRampBlack ( const GrayRamp & r, const CColor & fallback );

// The sweep when it holds a reading, else the list when it has a point, else
// the sweep.
GrayRampSource ChooseGrayRampSource ( bool sweepHoldsReading, bool listHasPoint );

// Whether point i is drawn: every sweep point, and a list point inside 0..1.
bool GrayRampDraws ( const GrayRamp & r, int i );

// Whether point i of a LIST ramp enters the gamma fit: a gray strictly between
// 0% and 100%. The per-point gamma is log(Y) / log(level): at 0% and below the
// level has no logarithm, at 100% it is 0 (the white is what Y is divided by),
// and above 100% the display is past the white the curve is defined to. A
// list's black and white can sit anywhere in its points (an extended-range
// list has grays below 0% and above 100%), so the test is by value. A sweep
// ramp keeps its own rule (FailedReadingRule::MeanPointGamma, points 1 ..
// size-3): false here.
bool GrayRampFitsPoint ( const GrayRamp & r, int i );

#endif // GRAYRAMP_H_INCLUDED_
