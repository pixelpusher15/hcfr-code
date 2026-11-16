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

// PatchSequence - deterministic builder for custom patch sequences.
//
// This is the PRODUCER side of the patch-list CSV dialect the app imports
// (ReadCsvPatchRows / CsvCodeToPercent in CsvPatchRows.h) and the in-memory
// model behind the .hps patch-sequence file (PatchSequenceFile.h). It lives
// in libHCFR with no MFC dependency so it can be used from the math core,
// console tools and the ColorMathTest oracles alike.
//
// Placement model, v1: every primitive is a pure function of its arguments
// appending patches in a documented order, so a sequence regenerates
// bit-identically from the calls that made it (no randomness, no hidden
// state). Placement is in SIGNAL space (stimulus percent). A second,
// display-model-driven tier (preimage placement) slots in later as
// additional primitives on this same builder. Patch ORDERING for
// drift/thermal behavior is a separate stage (a pure function from one
// sequence to another) and is NOT this class's concern: patches here are
// emitted in generation order.
//
// Units: stimulus percent per component, 0..100 inclusive by default (the
// house unit of ColorRGBDisplay). With SetExtended(true) the accepted span
// widens to the 10-bit video-range code span 0..1023 expressed in percent,
// [CsvCodeToPercent(0,10,EXTENDED), CsvCodeToPercent(1023,10,EXTENDED)] =
// [-7.3059.., 109.4748..], so an EXTENDED patch list (super-white,
// sub-black) round-trips through the file reader without silent clamping.
// Whether a value is then REPRESENTABLE at a given output encoding is a
// separate question answered by CsvPercentRepresentable at write time (an
// 8-bit EXTENDED file cannot carry 109.47%). Every Add refuses non-finite
// or out-of-span components, and stores a negative zero as +0.0 so stored
// values have one bit pattern per value (which is what makes Deduplicate's
// bit-pattern rule exact). Names are metadata carried into the CSV name
// column; so that every accepted name survives the importer byte-for-byte,
// a name must be printable ASCII (0x20..0x7E), contain no ',' (the column
// delimiter), and carry no leading or trailing spaces (the importer trims
// them). Anything else is refused at Add time; empty names are fine. Group
// names follow the same rules, are non-empty, and contain no spaces (they
// are single tokens in the file's GROUP directive); ValidGroupName is that
// rule, shared with the file writer.
//
// Identity, roles, timing (the .hps model):
//   - Every entry carries an IDENTITY (the PatchNumber column): an int in
//     0..kMaxIdentity, unique per DEFINITION. The first entry carrying an
//     identity defines its stimulus; any later entry with the same identity
//     is a REPEAT of that definition (an anchor instance, a preroll copy, a
//     deliberate late re-measure) and must carry the same R,G,B. The
//     placement primitives assign fresh identities in generation order
//     (0, 1, 2, ... continuing from the largest identity present + 1).
//   - Every entry carries a ROLE, one of 'C' characterization (default),
//     'A' anchor, 'P' preroll, 'V' verification, 'I' interstitial (shown,
//     never measured), 'X' identification (measured, excluded from the fit).
//     The role predicates below are the ONE place that knowledge lives:
//     measured = every role but 'I'; fitted = 'C' and 'V'; exportable to a
//     third-party tool = 'C' and 'V'.
//   - Every entry carries an optional GROUP name (probe band) and an
//     optional per-row PatchTiming override; -1 in a PatchTiming field means
//     "not set here, use the group / the run's default".
//
// Error convention: Add*/Set*/Write* return bool. A refused call leaves the
// sequence (and any output object/file) exactly as it was. File and
// serialization failures record a one-line lowercase reason in
// LastError(), cleared by the next successful Write.

#ifndef PATCHSEQUENCE_H
#define PATCHSEQUENCE_H

#include <map>
#include <string>
#include <utility>
#include <vector>

// Per-row probe/timing overrides; -1 = not set. Seconds for settle, hold,
// integration; a count for average. Refused when set to anything negative
// other than -1, or to a non-finite value.
struct PatchTiming
{
    double settle;
    double hold;
    double integration;
    int    average;
    PatchTiming() : settle(-1.0), hold(-1.0), integration(-1.0), average(-1) {}
};

// Writes `text` to `path` through a sibling temporary file moved over the
// target - an atomic replace (MoveFileEx) on Windows - so a failed write
// never destroys an existing file. On the non-Windows fallback path, if
// replacing the destination fails after its removal the fully-written
// temporary is LEFT ON DISK (path + ".tmp") so the new content is never
// lost either. On failure `reason` receives a one-line lowercase reason.
// Shared by every file writer in libHCFR that needs the guarantee.
bool WriteTextFileAtomic(const char* path, const std::string& text, std::string& reason);

class PatchSequence
{
public:
    // Identities above this are refused (leaves headroom for "+1").
    static const int kMaxIdentity = 1000000000;

    // The role letters and their meanings (see the header comment).
    static bool ValidRole(char role);
    static bool RoleIsMeasured(char role);      // all but 'I'
    static bool RoleIsFitted(char role);        // 'C', 'V'
    static bool RoleIsExportable(char role);    // 'C', 'V'

    // The group-name rule (see the header comment).
    static bool ValidGroupName(const std::string& group);

    PatchSequence();

    int Count() const;

    // Reads patch `index` (0-based, generation order): rgb = stimulus
    // percents, and when name is non-null the patch's name (possibly
    // empty). Returns false (writing nothing) on an out-of-range index.
    bool GetPatch(int index, double rgb[3], std::string* name) const;

    // Per-entry accessors. GetIdentity returns -1 and GetRole returns '\0'
    // on an out-of-range index; the bool forms return false writing nothing.
    int  GetIdentity(int index) const;
    char GetRole(int index) const;
    bool GetGroup(int index, std::string& group) const;
    bool GetTiming(int index, PatchTiming& timing) const;

    // Per-entry mutators. Refuse an out-of-range index, a role outside the
    // six letters, a group name that is non-empty and fails ValidGroupName
    // (empty clears the group), or a timing with a negative value other
    // than -1 / a non-finite value.
    bool SetRole(int index, char role);
    bool SetGroup(int index, const std::string& group);
    bool SetTiming(int index, const PatchTiming& timing);

    // Extended span (see the header comment). Switching extended OFF while
    // an entry lies outside 0..100 is refused (the sequence is unchanged).
    bool SetExtended(bool extended);
    bool IsExtended() const;

    // Appends one explicit patch with a fresh identity and role 'C'.
    // Refuses non-finite components, components outside the accepted span,
    // or a name outside the rules above.
    bool AddPatch(double rPct, double gPct, double bPct,
                  const std::string& name);

    // Appends one row with an EXPLICIT identity and role (the file reader's
    // primitive; also how anchors and preroll rows are placed by the
    // ordering stage). Refuses id < 0 or id > kMaxIdentity, a bad role, bad
    // components / name - and, when `id` already exists in the sequence, an
    // R,G,B that differs from the definition's (a repeat must repeat).
    // Names on a repeat may differ from the definition's. The next fresh
    // identity becomes max(existing) + 1.
    bool AddRow(int id, double rPct, double gPct, double bPct,
                const std::string& name, char role);

    // Appends a neutral ramp: count >= 2 patches with r == g == b at
    // levels spaced uniformly from loPct to hiPct inclusive - level i is
    // loPct*(1-u) + hiPct*u with u = i/(count-1), the endpoint-exact
    // form, so the first and last patches sit at exactly loPct and hiPct.
    // Names are "Gray <level>" with the level printed via printf "%g".
    // Refuses count < 2, non-finite or out-of-span endpoints, or
    // loPct >= hiPct.
    bool AddGrayRamp(int count, double loPct, double hiPct);

    // Appends a neutral ramp on a POWER-LAW grid: level i is
    // loPct + (hiPct-loPct) * u^exponent with u = i/(count-1), endpoint-
    // exact (u = 0 and u = 1 are computed as exactly loPct and hiPct, not
    // through pow). exponent > 1 packs levels toward loPct - the near-black
    // placement rule. Names "Gray <level>" ("%g"). Refuses everything
    // AddGrayRamp refuses, plus a non-finite or non-positive exponent.
    bool AddGrayPowerRamp(int count, double loPct, double hiPct, double exponent);

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

    // Appends the AddCubeShell(n) walk with every component mapped
    // linearly from [0,100] onto [insetPct, 100-insetPct] - the ring just
    // inside the gamut edge, where clipping behavior lives. A face node at
    // 100 lands at exactly 100-insetPct and one at 0 at exactly insetPct
    // (endpoint-exact: the mapping is insetPct*(1-u) + (100-insetPct)*u
    // with u the node's grid fraction). Names "Inset <r>/<g>/<b>" ("%g").
    // Refuses n outside 2..33 and insetPct outside the open interval
    // (0, 50).
    bool AddShellInset(int n, double insetPct);

    // Appends `count` REPEAT rows: re-measures of existing definitions,
    // chosen deterministically as every k-th role-'C' definition in
    // identity order with k = floor(definitions / count) (k >= 1), taking
    // the first `count` so chosen. Each appended row carries its source's
    // identity and R,G,B, role 'C', and the name "<source name> repeat"
    // (or "repeat" when the source name is empty). Where the repeats sit
    // in the measured order is the ordering stage's decision. Refuses
    // count < 1 or count greater than the number of role-'C' definitions.
    bool AddRepeats(int count);

    // Removes duplicate DEFINITIONS: an entry is removed when it is the
    // first entry carrying its identity, its role is 'C', and an earlier
    // surviving role-'C' definition has identical bit patterns in all
    // three components. The first occurrence (its name included) is kept.
    // Repeats, and rows of any other role, are never removed and never
    // cause a removal. After removal every surviving definition is
    // renumbered contiguously from 0 in sequence order, and every repeat
    // follows its definition's new identity; a repeat whose definition was
    // removed now refers to the surviving equal definition. Returns the
    // number of entries removed.
    // When `renumbering` is non-null it receives one (old, new) pair per
    // identity that existed before the call, sorted by old identity -
    // survivors mapped to their new number, removed duplicates mapped to
    // their surviving equal's new number - so anything that names patches
    // by identity outside the sequence (the file header's ANCHOR and
    // PREROLL lines, PatchSequenceHeader::RemapIdentities) can follow.
    int Deduplicate(std::vector< std::pair<int, int> >* renumbering = 0);

    void Clear();

    // Exchanges contents with `other` in O(1); no copy of the entries.
    void Swap(PatchSequence& other);

    // CSV emission in the importer's dialect. First line
    // "# BITDEPTH <bits>, RANGE <FULL|LEGAL|EXTENDED>", then one row per
    // entry in sequence order, "identity,R,G,B,name" (the ",name" field is
    // omitted when the name is empty), LF-terminated, ASCII. Codes come
    // from CsvCodeFromPercent at the chosen encoding, so all fields are
    // integers and the output is locale-independent by construction.
    // Roles, groups and timings are NOT written (see PatchSequenceFile for
    // the full form). Round-trip guarantee: parsing the output with
    // ReadCsvPatchRows and mapping codes back through CsvCodeToPercent
    // reproduces each percent within half a code step, with names and
    // identities preserved - guaranteed because a component for which
    // CsvPercentRepresentable(bits, range) is false is REFUSED (so a LEGAL
    // or FULL write refuses anything outside 0..100, and an 8-bit EXTENDED
    // write refuses what only 10 bits can carry). Also refuses (output
    // untouched, LastError set) an empty sequence, bits other than 8 or
    // 10, or a range other than CSV_RANGE_FULL / LEGAL / EXTENDED.
    bool WriteCsv(std::string& out, int bits, int range) const;

    // WriteCsv to a file via WriteTextFileAtomic.
    bool WriteCsvFile(const char* path, int bits, int range) const;

    const std::string& LastError() const { return m_lastError; }

private:
    struct Entry
    {
        int         id;
        char        role;
        double      rgb[3];
        std::string name;
        std::string group;
        PatchTiming timing;
    };

    bool ValidComponentHere(double v) const;

    // Appends a fully-formed entry, maintaining the identity index and the
    // next fresh identity. Every Add* path ends here.
    void Append(const Entry& e);

    // One walker per shape, shared by the public primitives so grid and
    // shell (and gray and channel ramps) can never drift apart:
    // channel -1 = neutral ramp; shellOnly skips interior lattice nodes.
    bool AppendRamp(int channel, int count, double loPct, double hiPct);
    bool AppendLattice(int n, bool shellOnly);

    std::vector<Entry> m_entries;
    std::map<int, size_t> m_firstAt;    // identity -> index of its definition
    int  m_nextId;                      // max identity present + 1 (0 when empty)
    bool m_extended;
    mutable std::string m_lastError;
};

#endif // PATCHSEQUENCE_H
