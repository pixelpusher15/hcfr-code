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
#include <math.h>

bool PrimarySetRowCounts ( const CsvPatchRow & row, int color )
{
	if ( color < PATCH_COLOR_RED || color > PATCH_COLOR_MAGENTA || row.label.color != color )
		return false;
	const int kind = ( color <= PATCH_COLOR_BLUE ) ? PATCH_KIND_PRIMARY : PATCH_KIND_SECONDARY;
	// Exactly 100%: CsvCodeToPercent gives exactly 100.0 for the white code in
	// every bit depth and range, and the label's level is that value.
	return row.label.kind == kind && row.label.level == 100.0 && PatchListRoleCounts ( row.role );
}

bool PrimarySetRowCountsAtLevel ( const CsvPatchRow & row, int color, double level )
{
	if ( color < PATCH_COLOR_RED || color > PATCH_COLOR_MAGENTA || row.label.color != color )
		return false;
	const int kind = ( color <= PATCH_COLOR_BLUE ) ? PATCH_KIND_PRIMARY : PATCH_KIND_SECONDARY;
	return row.label.kind == kind && PrimarySetLevelCode ( row.label.level ) == PrimarySetLevelCode ( level )
		&& PatchListRoleCounts ( row.role );
}

int PrimarySetLevelCode ( double level )
{
	return (int) floor ( 64.0 + level * 8.76 + 0.5 );
}

// The six entries from the rows that count: at exactly 100% (atLevel false,
// PrimarySetRowCounts) or at level (PrimarySetRowCountsAtLevel).
static void BuildEntries ( const CsvPatchRow * rows, int nRows, const CColor * readings, int nReadings, bool atLevel, double level, ListPrimarySet * out )
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
		const bool counts = atLevel ? PrimarySetRowCountsAtLevel ( rows[i], color, level ) : PrimarySetRowCounts ( rows[i], color );
		if ( ! counts || ! readings[i].isValid() )
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

void BuildListPrimarySet ( const CsvPatchRow * rows, int nRows, const CColor * readings, int nReadings, ListPrimarySet * out )
{
	BuildEntries ( rows, nRows, readings, nReadings, false, 100.0, out );
}

void BuildListPrimarySetAtLevel ( const CsvPatchRow * rows, int nRows, const CColor * readings, int nReadings, double level, ListPrimarySet * out )
{
	BuildEntries ( rows, nRows, readings, nReadings, true, level, out );
}

PrimarySetChoice ChoosePrimarySetShown ( bool dedicatedHoldsReading, bool listHasRunLevelEntry, bool listHasFullEntry, double runLevel, bool runPureChannels )
{
	PrimarySetChoice c;	// the dedicated measurement, with references
	if ( dedicatedHoldsReading || ! runPureChannels )
		return c;
	if ( PrimarySetLevelCode ( runLevel ) == PrimarySetLevelCode ( 100.0 ) )
	{
		if ( listHasFullEntry )
			c.source = PRIMARY_SET_LIST;
		return c;
	}
	if ( listHasRunLevelEntry )
	{
		c.source = PRIMARY_SET_LIST;
		c.runLevelRows = true;
	}
	else if ( listHasFullEntry )
	{
		// 100% rows under a run below 100% (HDR10): the references are the run
		// level's, and tone mapping (and, on WRGB panels, the white subpixel)
		// moves a real display's 100% primaries away from them - drawn, no dE.
		c.source = PRIMARY_SET_LIST;
		c.referenceApplies = false;
	}
	return c;
}

int PrimarySetShownKey ( const PrimarySetChoice & c )
{
	if ( c.source != PRIMARY_SET_LIST )
		return 0;
	return c.runLevelRows ? 2 : 1;
}

PrimaryPageColumnRead PrimaryPageColumnSource ( int col, const PrimarySetChoice & primaries, const PrimarySetChoice & secondaries )
{
	if ( col < 0 || col > 5 )
		return PRIMARY_PAGE_DEDICATED;	// the white, the black, outside the page
	const PrimarySetChoice & c = ( col < 3 ) ? primaries : secondaries;
	if ( c.source != PRIMARY_SET_LIST )
		return PRIMARY_PAGE_DEDICATED;
	return c.referenceApplies ? PRIMARY_PAGE_LIST : PRIMARY_PAGE_LIST_NO_REF;
}

bool PrimaryPageColumnWritesDedicated ( int col, const PrimarySetChoice & primaries, const PrimarySetChoice & secondaries )
{
	return PrimaryPageColumnSource ( col, primaries, secondaries ) == PRIMARY_PAGE_DEDICATED;
}

bool PrimaryPageRowUsesReference ( int component, bool hasDataRef )
{
	if ( component == 3 || component == 4 )
		return true;
	return hasDataRef ? ( component == 7 || component == 8 ) : ( component == 5 || component == 6 );
}

PrimaryPageNote PrimaryPageSourceNote ( const PrimarySetChoice & primaries, const PrimarySetChoice & secondaries )
{
	const bool p = ( primaries.source == PRIMARY_SET_LIST );
	const bool s = ( secondaries.source == PRIMARY_SET_LIST );
	if ( p && s )
		return PRIMARY_PAGE_NOTE_BOTH;
	if ( p )
		return PRIMARY_PAGE_NOTE_PRIMARIES;
	return s ? PRIMARY_PAGE_NOTE_SECONDARIES : PRIMARY_PAGE_NOTE_NONE;
}

bool BuildListWhiteAtLevel ( const CsvPatchRow * rows, int nRows, const CColor * readings, int nReadings, double level, CColor * out )
{
	if ( rows == NULL || readings == NULL )
		return false;
	const int n = ( nRows < nReadings ) ? nRows : nReadings;
	const int code = PrimarySetLevelCode ( level );
	std::vector<const CColor *> members;
	int first = -1;
	for ( int i = 0; i < n; i++ )
	{
		if ( ! GrayRampRowCounts ( rows[i] ) || ! readings[i].isValid() || PrimarySetLevelCode ( rows[i].label.level ) != code )
			continue;
		members.push_back ( & readings[i] );
		if ( first < 0 )
			first = i;
	}
	if ( members.empty () )
		return false;
	const CColor w = AverageGrayReadings ( & members[0], (int) members.size (), & readings[first] );
	if ( ! GrayRampWhiteUsable ( w ) )
		return false;
	*out = w;
	return true;
}

ShownWhiteSource ChooseShownWhite ( bool primeWhiteMeasured, bool anyGroupFromList, bool listHasWhite, bool callerWhiteMeasured )
{
	if ( primeWhiteMeasured || ! anyGroupFromList )
		return SHOWN_WHITE_CALLER;
	if ( listHasWhite )
		return SHOWN_WHITE_LIST;
	return callerWhiteMeasured ? SHOWN_WHITE_CALLER : SHOWN_WHITE_NONE;
}

ShownWhiteSource ChooseShownPrimaryWhite ( bool primeWhiteMeasured, bool anyGroupFromList, bool listHasWhite, bool whiteLevelIsFull, bool grayScaleWhiteMeasured, bool onOffWhiteMeasured )
{
	const ShownWhiteSource r = ChooseShownWhite ( primeWhiteMeasured, anyGroupFromList, listHasWhite, false );
	if ( r != SHOWN_WHITE_NONE )
		return r;	// the caller's own white, or the list's
	if ( whiteLevelIsFull && grayScaleWhiteMeasured )
		return SHOWN_WHITE_GRAY_SCALE;
	if ( whiteLevelIsFull && onOffWhiteMeasured )
		return SHOWN_WHITE_ON_OFF;
	return SHOWN_WHITE_NONE;
}

bool ShownWhiteIsChosen ( ShownWhiteSource ws )
{
	return ws == SHOWN_WHITE_LIST || ws == SHOWN_WHITE_GRAY_SCALE || ws == SHOWN_WHITE_ON_OFF;
}

bool PrimarySetHasWhite ( const PrimarySetChoice & c, ShownWhiteSource ws )
{
	return ! ( ws == SHOWN_WHITE_NONE && c.source == PRIMARY_SET_LIST );
}

bool CallerWhiteIsMeasured ( bool primeValid, bool primeMeasured, bool fallbackValid, bool fallbackMeasured )
{
	if ( primeValid )
		return primeMeasured;
	return fallbackValid && fallbackMeasured;
}

PrimaryPageColumnRead PrimaryPageColumnSource ( int col, const PrimarySetChoice & primaries, const PrimarySetChoice & secondaries, bool whiteFromList, bool noWhite )
{
	if ( col == 6 && whiteFromList )
		return PRIMARY_PAGE_LIST;
	if ( whiteFromList || ! noWhite )
		return PrimaryPageColumnSource ( col, primaries, secondaries );
	if ( col == 6 )
		return PRIMARY_PAGE_WHITE_NONE;
	const PrimaryPageColumnRead r = PrimaryPageColumnSource ( col, primaries, secondaries );
	return ( r == PRIMARY_PAGE_LIST ) ? PRIMARY_PAGE_LIST_NO_WHITE : r;
}

bool PrimaryPageColumnWritesDedicated ( int col, const PrimarySetChoice & primaries, const PrimarySetChoice & secondaries, bool whiteFromList, bool noWhite )
{
	return PrimaryPageColumnSource ( col, primaries, secondaries, whiteFromList, noWhite ) == PRIMARY_PAGE_DEDICATED;
}

bool PrimaryPageRowUsesWhite ( int component, bool hasDataRef )
{
	if ( component == 3 || component == 5 )
		return true;
	return hasDataRef ? ( component == 7 || component == 8 ) : ( component == 6 );
}

bool PrimaryPageRowShown ( PrimaryPageColumnRead read, int component, bool hasDataRef )
{
	switch ( read )
	{
		case PRIMARY_PAGE_LIST_NO_REF:   return ! PrimaryPageRowUsesReference ( component, hasDataRef );
		case PRIMARY_PAGE_LIST_NO_WHITE: return ! PrimaryPageRowUsesWhite ( component, hasDataRef );
		case PRIMARY_PAGE_WHITE_NONE:    return false;
		default:                         return true;
	}
}

bool PrimaryPageCellShown ( PrimaryPageColumnRead read, int component, bool hasDataRef )
{
	if ( read == PRIMARY_PAGE_LIST_NO_REF && hasDataRef && ( component == 5 || component == 6 ) )
		return false;
	return PrimaryPageRowShown ( read, component, hasDataRef );
}

ShownWhiteSource ChooseListPointWhite ( bool userList, bool specialStandard, bool primeWhiteMeasured, bool listHasWhite, bool whiteLevelIsFull, bool grayScaleWhiteMeasured, bool onOffWhiteMeasured )
{
	if ( ! userList || specialStandard )
		return SHOWN_WHITE_CALLER;
	// The list's points are shown from the list: the shown primaries' rule
	// with a group from the list.
	return ChooseShownPrimaryWhite ( primeWhiteMeasured, true, listHasWhite, whiteLevelIsFull, grayScaleWhiteMeasured, onOffWhiteMeasured );
}

bool PatchListRowUsesWhite ( int component, bool hasDataRef, bool absoluteLuminance )
{
	if ( component == 3 || ( hasDataRef && component == 5 ) )
		return true;	// a dE
	const bool luminanceRow = hasDataRef ? ( component == 7 || component == 8 ) : ( component == 5 || component == 6 );
	return luminanceRow && ! absoluteLuminance;
}

bool PatchListLevelsHidden ( ShownWhiteSource ws, bool absoluteLuminance )
{
	return ws == SHOWN_WHITE_NONE && ! absoluteLuminance;
}
