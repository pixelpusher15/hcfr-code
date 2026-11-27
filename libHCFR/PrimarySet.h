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
// percent: 100 except HDR10, which drives 50.22831; runPureChannels when the
// run drives single and paired channels - not the 75% gamuts HDTVa / HDTVb,
// nor P3 / Rec.709 inside BT.2020, which drive the inner gamut's primary
// encoded in the container):
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

// The primaries page of the main grid (CMainView, display mode 1). Its page
// columns (the grid column minus one) are 0..2 red, green, blue, 3..5 yellow,
// cyan, magenta, 6 the white and 7 the black. Each group of the page shows what
// the CIE chart shows for it (ChoosePrimarySetShown, chosen per group); the
// white and the black are the dedicated measurement's.
enum PrimaryPageColumnRead
{
	PRIMARY_PAGE_DEDICATED   = 0,	// the dedicated measurement, exactly as before:
									// the white, the black, a group shown from the
									// dedicated measurement, any other column
	PRIMARY_PAGE_LIST        = 1,	// the list's readings, with the references
	PRIMARY_PAGE_LIST_NO_REF = 2,	// the list's readings, without the references
									// (referenceApplies false): no dE
	PRIMARY_PAGE_LIST_NO_WHITE = 3,	// the list's readings, with the references but
									// no white to judge them against
									// (SHOWN_WHITE_NONE): no dE, no luminance
									// against the reference; xy error kept
	PRIMARY_PAGE_WHITE_NONE  = 4	// the white column with no white
									// (SHOWN_WHITE_NONE): every row empty
};

// What page column col reads, given the choices for the primaries and the
// secondaries.
PrimaryPageColumnRead PrimaryPageColumnSource ( int col, const PrimarySetChoice & primaries, const PrimarySetChoice & secondaries );

// Whether an edit, a paste or a clear of a cell in page column col may write
// the dedicated measurement: only for a column that reads it. A column showing
// a list's readings is read-only, so the list's readings never reach the
// dedicated primaries, secondaries or white.
bool PrimaryPageColumnWritesDedicated ( int col, const PrimarySetChoice & primaries, const PrimarySetChoice & secondaries );

// Whether a row of the page (GetItemText's component) is derived from the
// app's references: the dE (3), the xy error (4), and the luminance against the
// reference and the reference luminance (5 and 6; 7 and 8 when a data reference
// document puts its dE and xy error at 5 and 6). The values (0..2) and the data
// reference document's rows are not. A PRIMARY_PAGE_LIST_NO_REF column leaves
// these rows empty.
bool PrimaryPageRowUsesReference ( int component, bool hasDataRef );

// The note the page's caption carries: which groups show a list's readings.
enum PrimaryPageNote
{
	PRIMARY_PAGE_NOTE_NONE        = 0,
	PRIMARY_PAGE_NOTE_PRIMARIES   = 1,
	PRIMARY_PAGE_NOTE_SECONDARIES = 2,
	PRIMARY_PAGE_NOTE_BOTH        = 3
};
PrimaryPageNote PrimaryPageSourceNote ( const PrimarySetChoice & primaries, const PrimarySetChoice & secondaries );

// The white the shown primaries and secondaries are judged against
// (CMeasure::GetShownPrimaryWhite): the primaries page's luminance percentages
// and dE, its white column, and the CIE chart's primary and secondary markers
// divide by it. HCFR's own primaries run measures its white (the prime white)
// at the primaries' level: 100%, and 50.22831% under HDR10 (PrimeWhiteIRELevel
// in Measure.cpp). A list's white is its gray at that level: every gray row
// (GrayRampRowCounts) whose level gives the same PrimarySetLevelCode, its valid
// readings averaged with AverageGrayReadings, the first being the lowest row -
// the members and the averaging the list's gray ramp gives a point (rows at
// two codes that land on one 10-bit legal code are one signal here, as for the
// primaries). At 100% it is the gray ramp's white bit for bit. TRUE with *out
// when the average is a white a chart may divide by (GrayRampWhiteUsable);
// FALSE with *out untouched otherwise.
bool BuildListWhiteAtLevel ( const CsvPatchRow * rows, int nRows, const CColor * readings, int nReadings, double level, CColor * out );

enum ShownWhiteSource
{
	SHOWN_WHITE_CALLER = 0,	// each caller's own white, exactly as before
	SHOWN_WHITE_LIST   = 1,	// the list's white (BuildListWhiteAtLevel)
	SHOWN_WHITE_NONE   = 2,	// no white: the groups shown from the list are
							// judged against nothing (no dE, no luminance
							// against the reference)
	SHOWN_WHITE_GRAY_SCALE = 3,	// the gray scale's measured 100% (ChooseShownPrimaryWhite)
	SHOWN_WHITE_ON_OFF     = 4	// a measured on/off white (ChooseShownPrimaryWhite)
};

// The one white rule for the views of the shown primaries and secondaries -
// the main grid's primaries page, the RGB levels widget and the CIE chart
// (markers, hover and the a*b* coverage figure) - behind
// CMeasure::GetShownPrimaryWhiteSource. With the prime white measured, or no
// group shown from the list: SHOWN_WHITE_CALLER, every caller keeps exactly
// its own white. Otherwise: (1) the list's own white (listHasWhite,
// BuildListWhiteAtLevel at the prime white's level); else (2), only when the
// run measures its white at 100% (whiteLevelIsFull: not HDR10, whose prime
// white is its 50.22831% diffuse white, nor HDTVa's 75%), a white the document
// measured: the gray scale's 100% (grayScaleWhiteMeasured) first, then the
// on/off white (onOffWhiteMeasured) - the caller passes false for a
// constructor placeholder, which is never measured; else (3) SHOWN_WHITE_NONE.
// ChooseShownWhite with callerWhiteMeasured false is steps (1) and (3).
ShownWhiteSource ChooseShownPrimaryWhite ( bool primeWhiteMeasured, bool anyGroupFromList, bool listHasWhite, bool whiteLevelIsFull, bool grayScaleWhiteMeasured, bool onOffWhiteMeasured );

// Whether ws puts a white of its own in place of the caller's: the list's, the
// gray scale's or the on/off white. Such a white is the one the shown groups
// are judged against and the white column shows (read-only).
bool ShownWhiteIsChosen ( ShownWhiteSource ws );

// The list's white only when the prime white was not measured, at least one
// group is shown from the list, and the list has a white at the run's white
// level. When the list has no such white and the caller's own chain would land
// on no measured white either (callerWhiteMeasured false: the constructor's
// placeholder or a stand-in like the target luminance), no white at all.
// Otherwise each caller keeps its own white and fallbacks. Never a white the
// list does not hold. With callerWhiteMeasured left true, SHOWN_WHITE_NONE is
// never given.
ShownWhiteSource ChooseShownWhite ( bool primeWhiteMeasured, bool anyGroupFromList, bool listHasWhite, bool callerWhiteMeasured = true );

// Whether a caller's own white chain lands on a measured white: its first
// white when that one is valid (primeValid; measured when primeMeasured), else
// its fallback (measured when fallbackValid and fallbackMeasured). A chain
// that ends on a stand-in has none. The app no longer calls it: each view's
// own chain gave way to the one rule, ChooseShownPrimaryWhite.
bool CallerWhiteIsMeasured ( bool primeValid, bool primeMeasured, bool fallbackValid, bool fallbackMeasured );

// Whether a group shown as c has a white to be judged against, given the
// white choice ws: not a group shown from the list when there is no white
// (SHOWN_WHITE_NONE); every other group, including any group shown from the
// dedicated measurement. Without it a view shows no dE for the group and
// nothing computed against a white (the CIE chart's L*a*b* line).
bool PrimarySetHasWhite ( const PrimarySetChoice & c, ShownWhiteSource ws );

// PrimaryPageColumnSource and PrimaryPageColumnWritesDedicated with the white
// column: whiteFromList (a white the rule chose, ShownWhiteIsChosen: the
// list's, the gray scale's or the on/off white) makes column 6
// PRIMARY_PAGE_LIST - the reference white applies to it - so it shows that
// white and is read-only. noWhite (SHOWN_WHITE_NONE) makes column 6
// PRIMARY_PAGE_WHITE_NONE and a PRIMARY_PAGE_LIST column
// PRIMARY_PAGE_LIST_NO_WHITE; a PRIMARY_PAGE_LIST_NO_REF column and the
// dedicated columns stay as they are. whiteFromList wins over noWhite. Without
// either, exactly the three-argument forms.
PrimaryPageColumnRead PrimaryPageColumnSource ( int col, const PrimarySetChoice & primaries, const PrimarySetChoice & secondaries, bool whiteFromList, bool noWhite = false );
bool PrimaryPageColumnWritesDedicated ( int col, const PrimarySetChoice & primaries, const PrimarySetChoice & secondaries, bool whiteFromList, bool noWhite = false );

// Whether a row of the page (GetItemText's component) is derived from the
// white: the dE (3), and the luminance against the reference and the reference
// luminance (5 and 6; with a data reference document its dE at 5, which is
// judged against the white too, and the luminance rows at 7 and 8). The values
// (0..2), the xy error (4, chromaticity only) and the data reference's xy
// error (6 with one) are not.
bool PrimaryPageRowUsesWhite ( int component, bool hasDataRef );

// Whether a row of a page column has a value, by what the column reads: every
// row for PRIMARY_PAGE_DEDICATED and PRIMARY_PAGE_LIST; not the rows the
// references make (PrimaryPageRowUsesReference) for PRIMARY_PAGE_LIST_NO_REF;
// not the rows the white makes (PrimaryPageRowUsesWhite) for
// PRIMARY_PAGE_LIST_NO_WHITE; no row for PRIMARY_PAGE_WHITE_NONE.
bool PrimaryPageRowShown ( PrimaryPageColumnRead read, int component, bool hasDataRef );

// What the page shows in a cell: PrimaryPageRowShown, and for a
// PRIMARY_PAGE_LIST_NO_REF column beside a data reference document also not
// the rows comparing it with that document (its dE and xy error, components 5
// and 6): they would set the list's 100% readings (HDR10, no references)
// against the other document's run-level primaries. Every other column kind
// is exactly PrimaryPageRowShown.
bool PrimaryPageCellShown ( PrimaryPageColumnRead read, int component, bool hasDataRef );

// The white the active user patch list's OWN points are judged against
// (CMeasure::GetListPointWhiteSource): the patch-list page of the main grid
// (display mode 11 on the user set) - its dE, luminance rows and dE average -
// the list-point markers of the CIE chart, the RGB levels widget and the
// selected-color pane for a list column. The same rule as the shown primaries
// (ChooseShownPrimaryWhite) with the list's points always shown from the list,
// so a reading judged as a list point and as a primary marker gets the same
// white: with the prime white measured, SHOWN_WHITE_CALLER (each view keeps
// exactly its own white chain); else the list's own white at the prime white's
// level (listHasWhite: BuildListWhiteAtLevel at 100%, 50.22831% under HDR10);
// else, only when that level is 100% (whiteLevelIsFull), the gray scale's
// measured 100% and then a measured on/off white; else SHOWN_WHITE_NONE (no dE
// and no white-relative figures). Not a user list (userList false: a built-in
// color checker or saturation set) or a special 75% standard (HDTVa, HDTVb,
// whose color checker points are judged against the on/off white, not the
// prime white's 75% level): SHOWN_WHITE_CALLER, unchanged.
ShownWhiteSource ChooseListPointWhite ( bool userList, bool specialStandard, bool primeWhiteMeasured, bool listHasWhite, bool whiteLevelIsFull, bool grayScaleWhiteMeasured, bool onOffWhiteMeasured );

// Whether a row of the patch-list page (GetItemText's component, laid out as
// the primaries page's) is judged against the list point's white, so is empty
// when there is none: the dE (3), the dE against a data reference document (5
// with one), and the luminance against the reference and the reference
// luminance (5 and 6; 7 and 8 with a data reference). With absoluteLuminance
// (HDR10: those two rows are the reference's own luminance against the
// tone-mapped diffuse white, which no measured white enters) the luminance
// rows are not. The values (0..2) and the xy errors (4; 6 with a data
// reference) are chromaticity or raw readings and never are.
bool PatchListRowUsesWhite ( int component, bool hasDataRef, bool absoluteLuminance );

// Which readings of the active user list give its points' own white
// (ChooseListPointWhite's listHasWhite) while a run of that list is in
// progress. During the run the patch-list page shows the LIVE readings (the
// sweep wiped them at its start and fills them patch by patch), while the
// list's stored readings (the master band) are the previous run's until the
// run ends. So: (1) the white of this run's live readings, as soon as they
// hold a usable one at the white level (liveHasWhite); else (2) the stored
// readings' white (masterHasWhite, the previous run of the list); else (3)
// none, and the caller's later fallbacks apply. Outside a run
// (runInProgress false) the live readings are never used: the stored white or
// none, exactly as before.
enum ListPointWhiteBand
{
	LIST_WHITE_BAND_NONE   = 0,	// neither holds a white at the level
	LIST_WHITE_BAND_LIVE   = 1,	// this run's live readings
	LIST_WHITE_BAND_MASTER = 2	// the list's stored readings
};
ListPointWhiteBand ChooseListPointWhiteBand ( bool runInProgress, bool liveHasWhite, bool masterHasWhite );

// That choice applied: BuildListWhiteAtLevel on the live readings (only with
// runInProgress) and on the stored ones, both paired with the same rows, then
// ChooseListPointWhiteBand. *out is the chosen band's white (same level rule
// and averaging as BuildListWhiteAtLevel); untouched with LIST_WHITE_BAND_NONE.
ListPointWhiteBand BuildListPointWhite ( const CsvPatchRow * rows, int nRows, const CColor * liveReadings, int nLive, const CColor * masterReadings, int nMaster, bool runInProgress, double level, CColor * out );

// Whether the list point white changed between two passes of the patch-list
// page: a different source, or the same chosen source (ShownWhiteIsChosen)
// with a different white (X, Y or Z not bit-identical). The whites of a source
// that chooses none (SHOWN_WHITE_CALLER, SHOWN_WHITE_NONE) are not compared.
// The page's incremental pass during a run keeps earlier columns computed
// against the white of their own pass, so a change forces a full pass.
bool ListPointWhiteChanged ( ShownWhiteSource before, const CColor & whiteBefore, ShownWhiteSource now, const CColor & whiteNow );

// Whether the RGB levels widget shows nothing for a list point with the white
// choice ws: with no white (SHOWN_WHITE_NONE), except under absoluteLuminance
// (HDR10), where its bars are judged against the tone-mapped diffuse white
// and only its dE would need the missing white; the widget cannot leave its
// dE alone empty, so there it keeps its own white chain.
bool PatchListLevelsHidden ( ShownWhiteSource ws, bool absoluteLuminance );

#endif // PRIMARYSET_H_INCLUDED_
