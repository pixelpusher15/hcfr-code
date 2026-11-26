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
// per group (R,G,B and Y,C,M separately, ChoosePrimarySetSource), the dedicated
// measurement when it holds a reading, else the list when it has an entry in
// that group and its 100% signal is the one HCFR's own primaries run drives
// (ListPrimariesMatchRun), else the dedicated measurement.

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

#endif // PRIMARYSET_H_INCLUDED_
