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

// PatchLabel.cpp: the patch-list row label. See PatchLabel.h.

#include "Color.h"
#include "PatchLabel.h"

namespace
{
	// The primary a channel index lights, and the secondary the other two light.
	const int kPrimaryOf[3]   = { PATCH_COLOR_RED,  PATCH_COLOR_GREEN,   PATCH_COLOR_BLUE   };
	const int kSecondaryOf[3] = { PATCH_COLOR_CYAN, PATCH_COLOR_MAGENTA, PATCH_COLOR_YELLOW };
}

PatchLabel ClassifyPatch ( int r, int g, int b, int bits, int range )
{
	PatchLabel label;
	if ( bits != 8 && bits != 10 )
		return label;

	// The nominal black and white codes, as CsvCodeToPercent decodes them.
	const int maxCode = ( 1 << bits ) - 1;
	int black, white;
	if ( range == CSV_RANGE_FULL )
	{
		black = 0;
		white = maxCode;
	}
	else
	{
		black = 16  << ( bits - 8 );
		white = 235 << ( bits - 8 );
	}

	// LEGAL and NONE clamp in CsvCodeToPercent: classify what is driven.
	int c[3] = { r, g, b };
	if ( range != CSV_RANGE_FULL && range != CSV_RANGE_EXTENDED )
		for ( int k = 0; k < 3; k++ )
			c[k] = ( c[k] < black ) ? black : ( c[k] > white ? white : c[k] );

	label.kind = PATCH_KIND_OTHER;

	if ( c[0] == c[1] && c[1] == c[2] )
	{
		label.kind = PATCH_KIND_GRAY;
		label.level = CsvCodeToPercent ( c[0], bits, range );
		label.isBlack = ( c[0] == black );
		label.isWhite = ( c[0] == white );
		return label;
	}

	// Exactly two channels equal: k is the odd one out, pair its partners' code.
	int k;
	if ( c[1] == c[2] )
		k = 0;
	else if ( c[0] == c[2] )
		k = 1;
	else if ( c[0] == c[1] )
		k = 2;
	else
		return label;	// three different codes: other
	const int single = c[k];
	const int pair = c[( k + 1 ) % 3];

	if ( pair == black )
	{
		label.kind = PATCH_KIND_PRIMARY;
		label.color = kPrimaryOf[k];
		label.level = CsvCodeToPercent ( single, bits, range );
	}
	else if ( single == black )
	{
		label.kind = PATCH_KIND_SECONDARY;
		label.color = kSecondaryOf[k];
		label.level = CsvCodeToPercent ( pair, bits, range );
	}
	else if ( black < pair && pair < single )
	{
		// Two lower channels equal: on the primary's line toward white.
		label.kind = PATCH_KIND_SATURATION;
		label.color = kPrimaryOf[k];
		label.level = CsvCodeToPercent ( single, bits, range );
		label.desaturation = CsvCodeToPercent ( pair, bits, range ) / label.level;
	}
	else if ( black < single && single < pair )
	{
		// Two upper channels equal: on the secondary's line toward white.
		label.kind = PATCH_KIND_SATURATION;
		label.color = kSecondaryOf[k];
		label.level = CsvCodeToPercent ( pair, bits, range );
		label.desaturation = CsvCodeToPercent ( single, bits, range ) / label.level;
	}
	// else a lower channel below black: off every line, other.
	return label;
}

void LabelPatchRows ( std::vector<CsvPatchRow> & rows, int bits, int range )
{
	for ( size_t i = 0; i < rows.size(); i++ )
		rows[i].label = ClassifyPatch ( rows[i].r, rows[i].g, rows[i].b, bits, range );
}

PatchLabel PatchLabelFromReading ( const CColor & aReading )
{
	if ( !aReading.HasProvenance() || !aReading.HasStimulusValue() )
		return PatchLabel();
	const CColorProvenance prov = aReading.GetProvenance();
	if ( prov.srcBits != 8 && prov.srcBits != 10 )
		return PatchLabel();
	const ColorRGBDisplay stim = aReading.GetStimulusValue();
	return ClassifyPatch ( CsvPercentToCode ( stim[0], prov.srcBits, prov.srcRange ),
	                       CsvPercentToCode ( stim[1], prov.srcBits, prov.srcRange ),
	                       CsvPercentToCode ( stim[2], prov.srcBits, prov.srcRange ),
	                       prov.srcBits, prov.srcRange );
}

void RelabelReading ( CColor & aReading )
{
	if ( !aReading.HasProvenance() )
		return;
	CColorProvenance prov = aReading.GetProvenance();
	prov.label = PatchLabelFromReading ( aReading );
	aReading.SetProvenance ( prov );
}
