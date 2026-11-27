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

// SummarySources.h: the rules behind the text HCFR generates about what is in
// use - the white and black the gray graphs take and where each comes from,
// the patch-list average gamma under the grid, and which patch lists the
// Summary names. Pure C++, no MFC, header only. The application gathers the
// facts (CMeasure, CMainView) and words the text; ColorMathTest T61 checks
// every branch.

#if !defined(SUMMARYSOURCES_H_INCLUDED_)
#define SUMMARYSOURCES_H_INCLUDED_

#include "Color.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

// Where the white the gray graphs use comes from (CMeasure::GetGrayGraphWhite).
enum GrayGraphWhiteSource
{
	GRAY_WHITE_NONE = 0,	// nothing usable (on a patch list the white-relative graphs are not drawn)
	GRAY_WHITE_SWEEP_TOP,	// the gray scale sweep's 100% slot
	GRAY_WHITE_LIST,		// the patch list's 100% gray
	GRAY_WHITE_CONTRAST,	// the on/off (contrast) white
	GRAY_WHITE_NOMINAL		// the nominal white a new document starts with: not a reading
};

struct GrayGraphWhiteFacts
{
	bool listRamp;			// the gray graphs draw the patch list (GetGrayGraphRamp)
	bool sweepTopUsable;	// the sweep's top slot passes GrayScaleWhite's test
	bool listWhiteUsable;	// the list's 100% gray passes GrayRampWhiteUsable
	bool contrastValid;		// the on/off white is valid
	bool contrastMeasured;	// the on/off white is flagged measured and holds a reading
	bool contrastUsable;	// the on/off white passes GrayRampWhiteUsable
	GrayGraphWhiteFacts() : listRamp(false), sweepTopUsable(false), listWhiteUsable(false),
	                        contrastValid(false), contrastMeasured(false), contrastUsable(false) {}
};

// The order GetGrayGraphWhite takes. A patch list: its 100% gray, else a
// measured, usable on/off white, else none. The sweep (GetGrayScaleWhite):
// its top slot, else the on/off white whatever it holds - measured, or the
// constructor's nominal white when it was never measured.
inline GrayGraphWhiteSource ChooseGrayGraphWhiteSource ( const GrayGraphWhiteFacts & f )
{
	if ( f.listRamp )
	{
		if ( f.listWhiteUsable )
			return GRAY_WHITE_LIST;
		if ( f.contrastMeasured && f.contrastUsable )
			return GRAY_WHITE_CONTRAST;
		return GRAY_WHITE_NONE;
	}
	if ( f.sweepTopUsable )
		return GRAY_WHITE_SWEEP_TOP;
	if ( ! f.contrastValid )
		return GRAY_WHITE_NONE;
	return f.contrastMeasured ? GRAY_WHITE_CONTRAST : GRAY_WHITE_NOMINAL;
}

// Where the black the gray graphs use comes from (CMeasure::GetGrayGraphBlack).
enum GrayGraphBlackSource
{
	GRAY_BLACK_NONE = 0,	// nothing valid
	GRAY_BLACK_OVERRIDE,	// the user's black level ("Override black" / "Use Black Level")
	GRAY_BLACK_SWEEP_ZERO,	// the gray scale sweep's 0% slot
	GRAY_BLACK_LIST,		// the patch list's 0% gray
	GRAY_BLACK_CONTRAST,	// the on/off (contrast) black
	GRAY_BLACK_NOMINAL		// the nominal black a new document starts with: not a reading
};

struct GrayGraphBlackFacts
{
	bool listRamp;				// the gray graphs draw the patch list
	bool overrideInEffect;		// CMeasure::BlackOverrideInEffect
	bool docOverride;			// CMeasure::GrayScaleBlackIsOverride (the document's recorded decision)
	bool listBlackUsable;		// the list's 0% gray passes GrayRampBlack's test
	bool sweepBlackUsable;		// the sweep's 0% slot passes GrayScaleBlack's test
	bool sweepBlackSynthetic;	// that slot holds the override's synthetic black (SweepConfirmRule::IsSyntheticBlack)
	bool contrastValid;			// the on/off black is valid
	bool contrastSynthetic;		// the on/off black is the override's synthetic black
	bool contrastNominal;		// the on/off black is the constructor's placeholder
	GrayGraphBlackFacts() : listRamp(false), overrideInEffect(false), docOverride(false), listBlackUsable(false),
	                        sweepBlackUsable(false), sweepBlackSynthetic(false), contrastValid(false),
	                        contrastSynthetic(false), contrastNominal(false) {}
};

// The order GetGrayGraphBlack takes. A patch list with the override not in
// effect: its 0% gray when it has one. Every other case is GetGrayScaleBlack:
// the document's override, else the sweep's 0% slot, else the on/off black.
// A slot that holds the override's synthetic black is the user's black, not a
// reading, wherever it sits. The document's override is always in effect
// (BlackOverrideInEffect includes it); it is tested on its own as well, so the
// rule holds even for facts that leave overrideInEffect out.
inline GrayGraphBlackSource ChooseGrayGraphBlackSource ( const GrayGraphBlackFacts & f )
{
	if ( f.listRamp && ! f.overrideInEffect && ! f.docOverride && f.listBlackUsable )
		return GRAY_BLACK_LIST;
	if ( f.docOverride )
		return GRAY_BLACK_OVERRIDE;
	if ( f.sweepBlackUsable )
		return f.sweepBlackSynthetic ? GRAY_BLACK_OVERRIDE : GRAY_BLACK_SWEEP_ZERO;
	if ( ! f.contrastValid )
		return GRAY_BLACK_NONE;
	if ( f.contrastSynthetic )
		return GRAY_BLACK_OVERRIDE;
	if ( f.contrastNominal )
		return GRAY_BLACK_NOMINAL;
	return GRAY_BLACK_CONTRAST;
}

// The decimals the magnitude a alone calls for (SummaryLuminanceDecimals).
inline int SummaryLuminanceDecimalsOfMagnitude ( double a )
{
	if ( a >= 100.0 )
		return 1;
	if ( a >= 10.0 )
		return 2;
	if ( a >= 1.0 )
		return 3;
	return 4;
}

// Decimals a luminance in cd/m2 is printed with: 1 from 100 up, 2 from 10, 3
// from 1, 4 below (a black), judged on the PRINTED number: 99.996 printed with
// 2 decimals reads "100.00", so it gets 1 and prints "100.0". The rounding is
// the CRT's own (the same "%.*f" the caller formats with). The boundaries are a
// factor of 10 apart, so one step down is always enough. A value that is not
// finite gets 4.
inline int SummaryLuminanceDecimals ( double y )
{
	const double a = fabs ( y );
	if ( ! ( a <= 1e300 ) )
		return 4;
	const int d = SummaryLuminanceDecimalsOfMagnitude ( a );
	if ( d == 1 )
		return 1;
	char buf[64];	// a < 100 here: "99.9999" fits with room to spare
	snprintf ( buf, sizeof ( buf ), "%.*f", d, a );
	const int printed = SummaryLuminanceDecimalsOfMagnitude ( strtod ( buf, NULL ) );
	return printed < d ? printed : d;
}

// Whether the grid's patch-list line shows the average gamma: exactly when the
// gamma graph draws its average line (GammaHistoView.cpp) - a fit, a reading
// among the ramp's points, and not a PQ or HLG transfer (types 5 and 7, which
// draw no average line).
inline bool PatchListGammaShown ( bool fit, bool dataPresent, int gammaOffsetType )
{
	return fit && dataPresent && gammaOffsetType != 5 && gammaOffsetType != 7;
}

// Whether any of n readings is valid: the test the built-in color checker sets
// have in the Summary (any measured patch marks the set as present).
inline bool AnyValidReading ( const CColor * readings, int n )
{
	if ( readings == NULL )
		return false;
	for ( int i = 0; i < n; i++ )
		if ( readings[i].isValid () )
			return true;
	return false;
}

// Whether the Summary names a user patch set: any valid reading, taken from
// the USER band for the active set (its live storage) and from the set's own
// stored readings (CUserPatchSet::measures) for every other set.
inline bool UserSetSummaryListed ( bool active, const CColor * band, int nBand, const CColor * stored, int nStored )
{
	return active ? AnyValidReading ( band, nBand ) : AnyValidReading ( stored, nStored );
}

// Whether a set name has nothing to print (empty or only blanks): the Summary
// then says the list is unnamed.
inline bool PatchListNameIsEmpty ( const std::string & name )
{
	for ( size_t i = 0; i < name.size (); i++ )
		if ( name[i] != ' ' && name[i] != '\t' && name[i] != '\r' && name[i] != '\n' )
			return false;
	return true;
}

#endif // SUMMARYSOURCES_H_INCLUDED_
