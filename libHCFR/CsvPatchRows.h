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
// The dialect (ReadCsvPatchRows):
//   - Lines are trimmed of trailing CR / spaces / tabs; blank lines skipped.
//   - A leading '#' line (before any data row) carrying a BITDEPTH or RANGE
//     directive declares the format and marks the file headered, whose row
//     schema is PatchNumber,R,G,B[,Name]. Any other '#' line is a comment,
//     before or after data. Without a directive the file is legacy:
//     R,G,B[,Name], 8-bit legal, PatchNumber = row index.
//   - At most five fields are read per row; further fields are ignored.
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

// One source code -> HCFR stimulus percentage for a bit depth (8 or 10) and
// CSV range. FULL: 0..2^bits-1 -> 0..100. LEGAL / EXTENDED: black..white
// (16..235 at 8-bit, 64..940 at 10-bit) -> 0..100; EXTENDED keeps super-white
// (> 100%) and sub-black (< 0%), LEGAL and the legacy default clamp to [0,100].
double CsvCodeToPercent(double code, int bits, int range);

#endif // CSVPATCHROWS_H
