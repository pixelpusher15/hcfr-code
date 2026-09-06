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

// CsvPatchRows - the patch-list CSV dialect, MFC-free.
//
// One parser for every consumer of a patch-list CSV: ReadColorsFromCsv
// (percentages), CColorHCFRConfig::GetCColors (codes + names), and the
// patch-sequence file reader. It lives outside Color.h so translation units
// that must stay free of MFC types (PatchSequence, PatchSequenceFile, console
// tools) can parse the dialect directly; Color.h includes this header and
// keeps a CString forwarder for the app-side callers.
//
// This header is also the ONE home of the dialect's vocabulary - the range
// enum, its spelling in the header line, the code <-> percent mapping in
// both directions, and the format-directive grammar - so the CSV writers,
// the .hps writer and every reader share one definition instead of copies.
//
// The dialect (ReadCsvPatchRows):
//   - Lines are trimmed of trailing CR / spaces / tabs; blank lines skipped.
//   - A leading '#' line (before any data row) carrying a BITDEPTH or RANGE
//     directive declares the format and marks the file headered, whose row
//     schema is PatchNumber,R,G,B[,Name]. Any other '#' line is a comment,
//     before or after data. Without a directive the file is legacy:
//     R,G,B[,Name], 8-bit legal, PatchNumber = row index.
//   - At most five fields are read per row; further fields are ignored.
//   - Codes are read as numbers and truncated to int ((int)atof).
//   - Names are trimmed of edge spaces and tabs.

#ifndef CSVPATCHROWS_H
#define CSVPATCHROWS_H

#include <string>
#include <vector>

// Upper bound on the rows a patch list may carry: the app's USER band
// arrays are allocated to exactly this many slots.
#define MAX_USER_CC_PATCH_SIZE 10000

// Range a patch-list CSV can declare in its header (import-range guard).
// CSV_RANGE_NONE = no header / legacy default. ReadCsvPatchRows reports the
// declared range via rangeOut so producers and consumers share one spelling.
enum { CSV_RANGE_NONE = -1, CSV_RANGE_FULL = 0, CSV_RANGE_LEGAL = 1, CSV_RANGE_EXTENDED = 2 };

struct CsvPatchRow { int patch, r, g, b; std::string name; };

// Reads up to maxRows rows into `rows` (cleared first). Returns the row
// count, or -1 when the file cannot be opened. bitsOut (8 or 10, default 8)
// and rangeOut (CSV_RANGE_*, default CSV_RANGE_NONE) receive the declared
// format; either may be null.
int ReadCsvPatchRows(const char* csvPath, std::vector<CsvPatchRow>& rows, int maxRows, int* bitsOut, int* rangeOut);

// The format-directive grammar, shared by every reader of the dialect. Scans
// one '#' line (with or without its leading '#'): a BITDEPTH token followed
// by 8 or 10 sets `bits`; a RANGE token followed (after any of " \t,=:") by
// FULL / LEGAL / EXTENDED (case-insensitive) sets `range`. A value that is
// absent or unrecognized leaves its output untouched. Returns true when
// either was set - which is what marks a CSV headered.
bool ParseCsvFormatDirective(const std::string& line, int& bits, int& range);

// "FULL" / "LEGAL" / "EXTENDED"; "" for any other value.
const char* CsvRangeWord(int range);

// The header line every writer of the dialect emits, LF-terminated:
// "# BITDEPTH <bits>, RANGE <word>\n".
std::string CsvFormatHeaderLine(int bits, int range);

// One source code -> HCFR stimulus percentage for a bit depth (8 or 10) and
// CSV range. FULL: 0..2^bits-1 -> 0..100. LEGAL / EXTENDED: black..white
// (16..235 at 8-bit, 64..940 at 10-bit) -> 0..100; EXTENDED keeps super-white
// (> 100%) and sub-black (< 0%), LEGAL and the legacy default clamp to [0,100].
double CsvCodeToPercent(double code, int bits, int range);

// The inverse: percent -> nearest integer code at bits/range, clamped to the
// code span 0..2^bits-1. FULL maps 0..100 onto the full span; LEGAL and
// EXTENDED onto the video range (identical for 0..100; EXTENDED is where
// out-of-range percents land beyond it). This is the live signal path's
// quantizer (PiPercentToCode in Color.cpp), so what a file says is what the
// generator drives.
int CsvCodeFromPercent(double pct, int bits, int range);

// True when `pct` survives the round trip CsvCodeFromPercent ->
// CsvCodeToPercent within half a code step at bits/range - i.e. it is
// representable there without a silent clamp. False for a non-finite value,
// for anything outside 0..100 at FULL or LEGAL, and for an EXTENDED value
// beyond the code span (above 109.13% at 8-bit, 109.47% at 10-bit, or
// below -7.31%). Every writer of the dialect refuses a component for which
// this is false.
bool CsvPercentRepresentable(double pct, int bits, int range);

#endif // CSVPATCHROWS_H
