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

// PrimarySet.h: the primaries and secondaries a patch list can supply to the
// display views. Pure C++, no MFC.
//
// The candidate for each of the six colors is the list's rows labeled
// (PatchLabel.h, from the codes, never the name) that color's primary or
// secondary at exactly 100% of the list's nominal range, with a role that
// counts (PatchListRoleCounts) and a valid reading. Those rows all drive the
// same code, so they are one signal: their readings are averaged with
// AverageGrayReadings (GrayRamp.h), which sums in order of value, so the order
// of the rows does not matter. A lower level (a primary ramp) is never the
// primary; neither is a code above white in an extended-range list (its level
// is over 100%). A color with no such row has no entry.
//
// Which source the views show is CMeasure::GetShownPrimary / GetShownSecondary:
// per group (R,G,B and Y,C,M separately, ChoosePrimarySetShown), the dedicated
// measurement when it holds a reading, else the list when it has an entry in
// that group and HCFR's own primaries run drives pure channels, else the
// dedicated measurement. Which of the list's rows: when the run drives 100%,
// the 100% rows; when it drives a lower level (HDR10, 50.22831%), the rows at
// that level (BuildListPrimarySetAtLevel), else the 100% rows - drawn, but
// without the references (no dE), see ChoosePrimarySetShown.

#if !defined(PRIMARYSET_H_INCLUDED_)
#define PRIMARYSET_H_INCLUDED_

#include "Color.h"

enum PrimarySetSource
{
	PRIMARY_SET_DEDICATED = 0,	// the primaries / secondaries measurement
	PRIMARY_SET_LIST      = 1	// the active patch list
};

struct PrimarySetEntry
{
	CColor reading;     // the average of the color's 100% readings, or noDataColor
	bool   present;     // the list has a valid 100% reading of this color
	int    members;     // readings averaged
	int    firstRow;    // the lowest row averaged, or -1
	PrimarySetEntry() : reading(noDataColor), present(false), members(0), firstRow(-1) {}
};

struct ListPrimarySet
{
	// Indexed by PatchLabelColor: red, green, blue, yellow, cyan, magenta - the
	// order of CMeasure's primaries (0..2) and secondaries (3..5 = 0..2).
	PrimarySetEntry entries[6];
	bool HasPrimary() const   { return entries[0].present || entries[1].present || entries[2].present; }
	bool HasSecondary() const { return entries[3].present || entries[4].present || entries[5].present; }
};

// Whether a patch-list row is the full-level signal of color (a
// PatchLabelColor): labeled that color's primary (red, green, blue) or
// secondary (yellow, cyan, magenta) at exactly 100%, with a role that counts.
bool PrimarySetRowCounts ( const CsvPatchRow & row, int color );

// The list's six entries: rows[i] paired with readings[i], for i below both
// counts. A row counts when PrimarySetRowCounts says so for its color and its
// reading is valid.
void BuildListPrimarySet ( const CsvPatchRow * rows, int nRows, const CColor * readings, int nReadings, ListPrimarySet * out );

// Whether a list's 100% primaries and secondaries are the signal HCFR's own
// primaries run drives, so the reference they are drawn against applies to
// them: the run drives each at runLevel percent (100 except HDR10, which drives
// 50.22831), and drives single and paired channels only when runPureChannels
// (not the 75% gamuts HDTVa / HDTVb, nor P3 / Rec.709 inside BT.2020, which
// drive the inner gamut's primary encoded in the container). Both are needed.
bool ListPrimariesMatchRun ( double runLevel, bool runPureChannels );

// One group: the dedicated measurement when it holds a reading; else the list
// when it has an entry in the group and matches the run; else the dedicated
// measurement.
PrimarySetSource ChoosePrimarySetSource ( bool dedicatedHoldsReading, bool listHasEntry, bool listMatchesRun );

// The 10-bit legal code a level lands on: round(64 + level * 8.76), level in
// percent of the nominal range, as a label's level (CsvCodeToPercent) and
// HCFR's run level both are. HDR10's run level, 50.22831, gives 504; so do the
// 8-bit legal code 126 and the full-range codes a generator drives for it (8-bit
// 128, 10-bit 514). Two levels are the same signal when they give the same
// code: an exact comparison of doubles would miss a label decoded from a code.
int PrimarySetLevelCode ( double level );

// Whether a patch-list row is color's signal at level: labeled that color's
// primary or secondary, with a role that counts, and its level and level give
// the same PrimarySetLevelCode.
bool PrimarySetRowCountsAtLevel ( const CsvPatchRow & row, int color, double level );

// BuildListPrimarySet with the rows at level (PrimarySetRowCountsAtLevel) in
// place of the 100% rows.
void BuildListPrimarySetAtLevel ( const CsvPatchRow * rows, int nRows, const CColor * readings, int nReadings, double level, ListPrimarySet * out );

// What one group of the views shows (ChoosePrimarySetShown).
struct PrimarySetChoice
{
	PrimarySetSource source;
	bool runLevelRows;		// LIST: the rows at a run level below 100%
							// (BuildListPrimarySetAtLevel); else the 100% rows
							// (BuildListPrimarySet). False for DEDICATED.
	bool referenceApplies;	// the app's references, and so a dE, apply to the
							// shown set
	PrimarySetChoice() : source(PRIMARY_SET_DEDICATED), runLevelRows(false), referenceApplies(true) {}
};

// One group of the views, given what HCFR's own primaries run drives (runLevel
// percent, runPureChannels as for ListPrimariesMatchRun):
//   - the dedicated measurement when it holds a reading (with references);
//   - a run of mixed channels (HDTVa / HDTVb, UHDTV3 / UHDTV4): the dedicated
//     measurement, whatever the list holds;
//   - a run at 100%: the list's 100% rows when it has an entry in the group,
//     with references;
//   - a run below 100% (HDR10): the list's rows at the run level when it has an
//     entry there, with references, exactly as the dedicated run; else its 100%
//     rows when it has an entry there, WITHOUT references. The references are
//     the run level's, and a real display's 100% primaries in HDR10 are not a
//     run-level color pushed up: tone mapping moves them, and on WRGB panels
//     the white subpixel desaturates them, so a dE against the run level's
//     reference would measure the display's tone mapping, not its gamut. The
//     triangle they draw still shows where the panel's full primaries are.
//   - otherwise the dedicated measurement.
// The run level's rows win over the 100% rows; the choice is per group, never
// per color, so one triangle never mixes two levels.
PrimarySetChoice ChoosePrimarySetShown ( bool dedicatedHoldsReading, bool listHasRunLevelEntry, bool listHasFullEntry, double runLevel, bool runPureChannels );

// A key that differs between the three things a group can show (the
// dedicated measurement, the list's 100% rows, the list's run-level rows), for
// caches that must treat a change of source as new data.
int PrimarySetShownKey ( const PrimarySetChoice & c );

#endif // PRIMARYSET_H_INCLUDED_
