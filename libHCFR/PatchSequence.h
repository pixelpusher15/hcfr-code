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

// PatchSequence: the engine under the Custom Patch Sequence Generator - a
// deterministic, composable builder of measurement patch lists, with a CSV
// writer that emits exactly the header-aware patch-list dialect the shared
// importer (ReadCsvPatchRows / ReadColorsFromCsv in Color.cpp) reads. The
// importer landed with the CSV patch-list work; this is the missing
// producer side, so generated sequences are ordinary patch-list files any
// current build can load.
//
// Placement model, v1: every primitive is a pure function of its arguments
// appending patches in a documented order, so a sequence regenerates
// bit-identically from the calls that made it (no randomness, no hidden
// state). Placement is in SIGNAL space (stimulus percent). A second,
// display-model-driven tier (preimage placement: put density where the
// DISPLAY needs it, via the fitted model's inverse) slots in later as
// additional primitives on this same builder - deliberately mirroring how
// the weighted fit interface was staged. Patch ordering for drift/thermal
// behavior is a separate, later stage and is NOT this class's concern:
// patches here are emitted in generation order.
//
// Units: stimulus percent per component, 0..100 inclusive (the house unit
// of ColorRGBDisplay). Every Add refuses non-finite or out-of-range
// components, and stores a negative zero as +0.0 so stored values have one
// bit pattern per value (which is what makes Deduplicate's bit-pattern
// rule exact). Names are metadata carried into the CSV name column; so
// that every accepted name survives the importer byte-for-byte, a name
// must be printable ASCII (0x20..0x7E), contain no ',' (the column
// delimiter), and carry no leading or trailing spaces (the importer trims
// them). Anything else is refused at Add time; empty names are fine.
//
// Error convention: Add*/Write* return bool. A refused call leaves the
// sequence (and any output object/file) exactly as it was. File and
// serialization failures record a one-line lowercase reason in
// LastError(), cleared by the next successful Write.

#ifndef PATCHSEQUENCE_H
#define PATCHSEQUENCE_H

#include <string>
#include <vector>

class PatchSequence
{
public:
    int Count() const;

    // Reads patch `index` (0-based, generation order): rgb = stimulus
    // percents, and when name is non-null the patch's name (possibly
    // empty). Returns false (writing nothing) on an out-of-range index.
    bool GetPatch(int index, double rgb[3], std::string* name) const;

    // Appends one explicit patch. Refuses non-finite components,
    // components outside 0..100, or a name outside the rules above
    // (non-printable or non-ASCII bytes, ',', leading/trailing spaces).
    bool AddPatch(double rPct, double gPct, double bPct,
                  const std::string& name);

    // Appends a neutral ramp: count >= 2 patches with r == g == b at
    // levels spaced uniformly from loPct to hiPct inclusive - level i is
    // loPct*(1-u) + hiPct*u with u = i/(count-1), the endpoint-exact
    // form, so the first and last patches sit at exactly loPct and hiPct.
    // Names are "Gray <level>" with the level printed via printf "%g".
    // Refuses count < 2, non-finite or out-of-range endpoints, or
    // loPct >= hiPct.
    bool AddGrayRamp(int count, double loPct, double hiPct);

    // Appends a single-channel ramp (channel 0=red, 1=green, 2=blue): the
    // chosen channel sweeps loPct..hiPct on the same uniform grid as
    // AddGrayRamp while the other two components are exactly 0. Names are
    // "Red <level>" / "Green <level>" / "Blue <level>". Same refusals as
    // AddGrayRamp, plus a bad channel index.
    bool AddChannelRamp(int channel, int count, double loPct, double hiPct);

    // Appends the full n^3 signal-space lattice (2 <= n <= 33), red
    // slowest / blue fastest - the same nesting GenerateProfileColors
    // uses - with each channel at the AddGrayRamp grid level for its
    // index over 0..100 (the endpoint-exact 0*(1-u)+100*u form; note
    // GenerateProfileColors computes its nodes as i*(100/(n-1)), which
    // can differ from this form in the last ulp at non-exact nodes, so
    // the two generators agree on the nesting, not bit-for-bit on every
    // node value). Names are "Cube <r>/<g>/<b>" (percents, "%g").
    // Refuses n outside 2..33.
    bool AddCubeGrid(int n);

    // Appends only the SURFACE of the n^3 lattice - the signal-space
    // gamut shell: the n^3 - (n-2)^3 nodes with at least one component at
    // 0 or 100 - in the same red-slowest/blue-fastest order (i.e. the
    // AddCubeGrid order with interior nodes skipped), same naming.
    // Refuses n outside 2..33.
    bool AddCubeShell(int n);

    // Removes exact duplicates - identical bit patterns in all three
    // components - keeping the FIRST occurrence (its name included) and
    // preserving order otherwise. Returns the number of patches removed.
    int Deduplicate();

    void Clear();

    // CSV emission in the importer's dialect. First line
    // "# BITDEPTH <bits>, RANGE <FULL|LEGAL|EXTENDED>", then one row per
    // patch, "index,R,G,B,name" (the ",name" field is omitted when the
    // name is empty), LF-terminated, ASCII. Codes are the nearest-integer
    // inverse of CsvCodeToPercent for the chosen encoding: FULL maps
    // 0..100 onto 0..2^bits-1; LEGAL and EXTENDED map onto the video
    // range (16..235 at 8-bit, 64..940 at 10-bit; identical for in-range
    // percents, and this class only holds 0..100). All fields are
    // integers, so the output is locale-independent by construction.
    // Round-trip guarantee: parsing the output with ReadCsvPatchRows and
    // mapping codes back through CsvCodeToPercent reproduces each percent
    // within half a code step, with names and 0-based patch numbers
    // preserved. Refuses (output untouched, LastError set) an empty
    // sequence, bits other than 8 or 10, or a range other than
    // CSV_RANGE_FULL / CSV_RANGE_LEGAL / CSV_RANGE_EXTENDED (Color.h).
    bool WriteCsv(std::string& out, int bits, int range) const;

    // WriteCsv to a file, via a sibling temporary file moved over the
    // target - an atomic replace (MoveFileEx) on Windows - so a failed
    // write never destroys an existing file. On the non-Windows fallback
    // path, if replacing the destination fails after its removal the
    // fully-written temporary is LEFT ON DISK (path + ".tmp") so the new
    // content is never lost either.
    bool WriteCsvFile(const char* path, int bits, int range) const;

    const std::string& LastError() const { return m_lastError; }

private:
    struct Entry
    {
        double rgb[3];
        std::string name;
    };

    // One walker per shape, shared by the public primitives so grid and
    // shell (and gray and channel ramps) can never drift apart:
    // channel -1 = neutral ramp; shellOnly skips interior lattice nodes.
    bool AppendRamp(int channel, int count, double loPct, double hiPct);
    bool AppendLattice(int n, bool shellOnly);

    std::vector<Entry> m_entries;
    mutable std::string m_lastError;
};

#endif // PATCHSEQUENCE_H
