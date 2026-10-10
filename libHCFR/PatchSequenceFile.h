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

// PatchSequenceFile - the .hps patch-sequence file, and the CSV dialects at
// its edges. MFC-free, libHCFR.
//
// The .hps file is a self-contained, deterministic text asset whose data
// rows are a STRICT SUPERSET of the headered patch-list CSV dialect
// (CsvPatchRows.h): the first five columns are PatchNumber,R,G,B,Name with
// the same "# BITDEPTH n, RANGE x" declaration, so a build that knows only
// the CSV dialect loads an .hps as a plain patch list. Row order is
// MEASUREMENT order; column 1 is the source identity; there is no second
// order. Plain CSV (legacy or headered) is an import source and a lossy
// export, never the native form.
//
// Grammar (normative; the T20 oracle pins the bytes):
//   - ASCII. LF line ends on write; CR LF accepted on read. A reader also
//     tolerates a UTF-8 byte-order mark, blank lines, and leading spaces
//     or tabs before the first directive (editors add all three).
//   - "#" lines before the first data row are DIRECTIVES, "# KEY value";
//     "#" lines after the first data row are comments. Keys are upper-case.
//     A reader IGNORES unknown keys and unknown columns (forward
//     compatibility by construction) and REFUSES a major version above its
//     own. Directive values: integers, "%g" reals, single tokens, or a
//     double-quoted string containing no '"'.
//   - Line 1 is exactly "# HCFR-SEQUENCE 1".
//   - Directives are written in THIS order, each only when set (a
//     repeatable key writes one line per item, in item order):
//       HCFR-SEQUENCE, "BITDEPTH <bits>, RANGE <FULL|LEGAL|EXTENDED>"
//       (CsvFormatHeaderLine), SEQUENCE-ID, NAME, CREATED, GENERATOR,
//       SOURCE, REQUIRES, DISPLAY-PROFILE, PANEL-RECORD, TARGET,
//       GAMUT-RELATION, STEP*, PATCH-GEOMETRY, GROUP*, READ-AT, ORDER,
//       TARGET-LOAD, LOAD-MODEL, WINDOW, DWELL, MAX-STEP, SETTLE-AFTER-STEP,
//       ABL-LIMIT, ANCHOR*, PREROLL, COLUMNS.
//     Formats: SOURCE is `recipe` or `csv "<path>" sha256 <hex>` (hex may
//     be empty: `csv "<path>"`); REQUIRES is a space-separated token list;
//     PATCH-GEOMETRY is `area=<int> background=<int>` (each only when set);
//     GROUP is `<name>` followed by, each only when set, `integration=<g>
//     average=<n> settle=<g> hold=<g> readat=<token> trace=<g> until=stable
//     deadband=<g>`; ANCHOR is `<id> every=<int>`; PREROLL is `<id>
//     count=<int>`; DWELL is `latency=<g>`; ABL-LIMIT is `apl=<g>`;
//     WINDOW, TARGET-LOAD, MAX-STEP, SETTLE-AFTER-STEP are one "%g" real;
//     LOAD-MODEL is written verbatim (its parameters belong to the
//     ordering stage). COLUMNS is always the full fixed list below.
//   - Columns, always all eleven, in this order:
//       PatchNumber,R,G,B,Name,Role,Group,Settle,Hold,Integration,Average
//     Codes are integers at BITDEPTH/RANGE (CsvCodeFromPercent). Name and
//     Group may be empty (the field is still present). Role is one letter.
//     Settle/Hold/Integration are "%g" seconds, Average an int; each is
//     empty when the row's PatchTiming field is -1.
//   - Data rows: at most MAX_USER_CC_PATCH_SIZE (CsvPatchRows.h). A
//     sequence above that is refused at write; a file above it at read,
//     for every dialect.
//
// What the writer (FormatHps / Write) refuses, so that every file it emits
// is one every reader here AND the CSV-only reader decode the same way:
//   - hdr.version other than 1 (it writes only what it can read back);
//   - an empty sequence, or more rows than the cap;
//   - a component not CsvPercentRepresentable at hdr.bits / hdr.range (the
//     same rule as PatchSequence::WriteCsv - no silent clamp, ever);
//   - any directive string value (name, generator, ids, paths, steps,
//     tokens, the load model) containing '"', CR or LF, or containing the
//     words BITDEPTH or RANGE in any letter case - because the CSV-only
//     reader scans every leading '#' line for those words, and a NAME of
//     "Range extended near-white" would flip its decoding;
//   - a header group whose name fails PatchSequence::ValidGroupName.
//
// Reading (Read): the dialect is detected from the first non-blank line
//   (after an optional BOM and leading whitespace) - "# HCFR-SEQUENCE" ->
//   .hps; otherwise the CsvPatchRows rules decide headered vs legacy.
//   Every dialect yields a PatchSequence (percents via CsvCodeToPercent at
//   the file's bits/range, codes truncated to int exactly as the CSV parser
//   does; an EXTENDED file switches the sequence to extended so no code is
//   clamped) plus a header. For a CSV import the header carries bits/range,
//   sourceKind "csv", the path, and nothing else; legacy CSV = 8-bit LEGAL,
//   identity = row index, all roles 'C'. A headered CSV's PatchNumber
//   column is used as identity ONLY when it can serve as one: when any two
//   rows share a number with different R,G,B (hand-edited files, a
//   non-numeric column that atoi collapses to 0), every identity falls back
//   to the row index and hdr.identitiesFromRows is set - the file still
//   loads, as it does in the app's own importer. For .hps: rows carry their
//   identity and role; a repeated identity with different R,G,B is refused
//   (AddRow's rule); the Group column names a GROUP directive (a name with
//   no directive is kept as is); timing columns fill the row's PatchTiming;
//   the parsed version is stored in hdr.version. A refusal names the row:
//   "bad patch row 12" (1-based over data rows).
//
// Export (ExportCsv): the headered CSV dialect only. EXPORT_MEASUREMENT
//   writes rows in sequence order; EXPORT_DEFINITION writes one row per
//   identity - the first occurrence - sorted by identity ascending. With
//   includeRoles false only rows whose role is exportable
//   (PatchSequence::RoleIsExportable: 'C' and 'V') are written; with
//   includeRoles true every row is. Identity, codes and name go out exactly
//   as PatchSequence::WriteCsv writes them, under WriteCsv's refusals.
//
// Error convention: every operation returns bool; on false the outputs are
// untouched and LastError() carries a one-line lowercase reason.

#ifndef PATCHSEQUENCEFILE_H
#define PATCHSEQUENCEFILE_H

#include "PatchSequence.h"

#include <string>
#include <utility>
#include <vector>

struct PatchGroupSpec
{
    std::string name;
    PatchTiming timing;         // integration / average / settle / hold
    std::string readAt;         // "" unset; else peak | plateau | t=<s>
    double      trace;          // seconds; -1 unset
    bool        untilStable;
    double      deadband;       // percent; -1 unset
    PatchGroupSpec() : trace(-1.0), untilStable(false), deadband(-1.0) {}
};

struct PatchAnchorSpec
{
    int id;
    int every;
    PatchAnchorSpec() : id(-1), every(-1) {}
};

struct PatchSequenceHeader
{
    int         version;                    // 1; what ParseHps read
    int         bits;                       // 8 | 10
    int         range;                      // CSV_RANGE_FULL | LEGAL | EXTENDED
    std::string sequenceId;
    std::string name;
    std::string created;                    // ISO 8601 UTC text, verbatim
    std::string generator;
    std::string sourceKind;                 // "" | "recipe" | "csv"
    std::string sourcePath;
    std::string sourceHash;
    bool        identitiesFromRows;         // CSV import fell back to row indices
    std::vector<std::string> required;      // REQUIRES tokens
    std::string displayProfile;
    std::string panelRecord;
    std::string target;
    std::string gamutRelation;
    std::vector<std::string> steps;         // STEP values, verbatim
    int         geometryArea;               // -1 unset
    int         geometryBackground;         // -1 unset
    std::vector<PatchGroupSpec> groups;
    std::string readAt;
    std::string order;                      // "" | none | constant-load
    double      targetLoad;                 // -1 unset
    std::string loadModel;                  // verbatim
    double      window;                     // seconds; -1 unset
    double      dwellLatency;               // seconds; -1 unset
    double      maxStep;                    // -1 unset
    double      settleAfterStep;            // -1 unset
    double      ablLimit;                   // apl percent; -1 unset
    std::vector<PatchAnchorSpec> anchors;
    int         prerollId;                  // -1 unset
    int         prerollCount;               // -1 unset

    PatchSequenceHeader();

    // Applies a PatchSequence::Deduplicate renumbering to every identity
    // this header names: each anchor id and prerollId becomes its new
    // number, or -1 when the old number is not in the map (so a stale
    // reference is visible, never silently wrong). Ids already -1 stay -1.
    void RemapIdentities(const std::vector< std::pair<int, int> >& renumbering);
};

class PatchSequenceFile
{
public:
    enum Dialect { DIALECT_HPS = 0, DIALECT_CSV_HEADERED = 1, DIALECT_CSV_LEGACY = 2 };
    enum ExportOrder { EXPORT_MEASUREMENT = 0, EXPORT_DEFINITION = 1 };

    // Any dialect from a file. On success seq and hdr are replaced and
    // dialectOut (when non-null) says which dialect was read.
    bool Read(const char* path, PatchSequence& seq, PatchSequenceHeader& hdr,
              Dialect* dialectOut);

    // The .hps dialect from / to text (what Read/Write use underneath, and
    // what the oracles pin).
    bool ParseHps(const std::string& text, PatchSequence& seq,
                  PatchSequenceHeader& hdr);
    bool FormatHps(std::string& out, const PatchSequence& seq,
                   const PatchSequenceHeader& hdr);

    // FormatHps to a file via WriteTextFileAtomic.
    bool Write(const char* path, const PatchSequence& seq,
               const PatchSequenceHeader& hdr);

    // Headered CSV export (see the header comment).
    bool ExportCsv(std::string& out, const PatchSequence& seq, int bits,
                   int range, ExportOrder order, bool includeRoles);
    bool ExportCsvFile(const char* path, const PatchSequence& seq, int bits,
                       int range, ExportOrder order, bool includeRoles);

    const std::string& LastError() const { return m_lastError; }

private:
    std::string m_lastError;
};

#endif // PATCHSEQUENCEFILE_H
