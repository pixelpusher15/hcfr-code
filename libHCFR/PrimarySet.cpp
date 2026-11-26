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

// PrimarySet.cpp: the primaries and secondaries a patch list can supply. See
// PrimarySet.h.

#include "PrimarySet.h"
#include "GrayRamp.h"
#include <vector>

bool PrimarySetRowCounts ( const CsvPatchRow & row, int color )
{
	if ( color < PATCH_COLOR_RED || color > PATCH_COLOR_MAGENTA || row.label.color != color )
		return false;
	const int kind = ( color <= PATCH_COLOR_BLUE ) ? PATCH_KIND_PRIMARY : PATCH_KIND_SECONDARY;
	// Exactly 100%: CsvCodeToPercent gives exactly 100.0 for the white code in
	// every bit depth and range, and the label's level is that value.
	return row.label.kind == kind && row.label.level == 100.0 && PatchListRoleCounts ( row.role );
}

void BuildListPrimarySet ( const CsvPatchRow * rows, int nRows, const CColor * readings, int nReadings, ListPrimarySet * out )
{
	*out = ListPrimarySet ();
	if ( rows == NULL || readings == NULL )
		return;
	const int n = ( nRows < nReadings ) ? nRows : nReadings;

	std::vector<const CColor *> members[6];
	int firstRow[6] = { -1, -1, -1, -1, -1, -1 };
	for ( int i = 0; i < n; i++ )
	{
		const int color = rows[i].label.color;
		if ( ! PrimarySetRowCounts ( rows[i], color ) || ! readings[i].isValid() )
			continue;
		members[color].push_back ( & readings[i] );
		if ( firstRow[color] < 0 )
			firstRow[color] = i;
	}

	for ( int c = 0; c < 6; c++ )
	{
		if ( members[c].empty () )
			continue;
		PrimarySetEntry & e = out->entries[c];
		e.reading = AverageGrayReadings ( & members[c][0], (int) members[c].size (), & readings[firstRow[c]] );
		e.present = true;
		e.members = (int) members[c].size ();
		e.firstRow = firstRow[c];
	}
}

bool ListPrimariesMatchRun ( double runLevel, bool runPureChannels )
{
	return runPureChannels && runLevel == 100.0;
}

PrimarySetSource ChoosePrimarySetSource ( bool dedicatedHoldsReading, bool listHasEntry, bool listMatchesRun )
{
	if ( dedicatedHoldsReading )
		return PRIMARY_SET_DEDICATED;
	return ( listHasEntry && listMatchesRun ) ? PRIMARY_SET_LIST : PRIMARY_SET_DEDICATED;
}
