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

// PatchSequence implementation - the behavioral contract lives in
// PatchSequence.h. Plain C++ and CRT only (no MFC), so the generator stays
// usable from the math core and from console tools.

#include "PatchSequence.h"

#include "CsvPatchRows.h"
#include "libHCFR_Config.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <map>
#include <set>
#include <vector>

#ifdef LIBHCFR_HAS_WIN32_API
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace
{
    // The extended span, read straight off the mapping that defines it:
    // the 10-bit video-range code span 0..1023 through CsvCodeToPercent,
    // so an EXTENDED patch list round trips without silent clamping and
    // the bounds cannot drift from the reader's arithmetic.
    double ExtendedLoPct()
    {
        return CsvCodeToPercent(0.0, 10, CSV_RANGE_EXTENDED);
    }

    double ExtendedHiPct()
    {
        return CsvCodeToPercent(1023.0, 10, CSV_RANGE_EXTENDED);
    }

    bool ValidComponent(double v)
    {
        if (!_finite(v))
            return false;
        return (v >= 0.0 && v <= 100.0);
    }

    bool ValidExtendedComponent(double v)
    {
        if (!_finite(v))
            return false;
        return (v >= ExtendedLoPct() && v <= ExtendedHiPct());
    }

    // Stored values carry one bit pattern per value: fold -0.0 into +0.0
    // (which keeps Deduplicate's bit-pattern rule exact and "%g" names
    // free of "-0").
    double NormalizeComponent(double v)
    {
        return (v == 0.0) ? 0.0 : v;
    }

    // Printable ASCII, no ',' (the CSV delimiter), no leading/trailing
    // spaces (the importer trims them, which would break the documented
    // name round trip). Empty is allowed.
    bool ValidName(const std::string& name)
    {
        if (name.empty())
            return true;
        if (name[0] == ' ' || name[name.size() - 1] == ' ')
            return false;
        for (size_t i = 0; i < name.size(); ++i)
        {
            unsigned char c = (unsigned char)name[i];
            if (c < 0x20 || c > 0x7E || c == ',')
                return false;
        }
        return true;
    }

    // A timing field is either -1 ("not set here") or a non-negative
    // finite value; an average is either -1 or a count of at least one.
    bool ValidTimingField(double v)
    {
        if (!_finite(v))
            return false;
        return (v >= 0.0 || v == -1.0);
    }

    bool ValidTiming(const PatchTiming& t)
    {
        if (!ValidTimingField(t.settle) || !ValidTimingField(t.hold)
            || !ValidTimingField(t.integration))
            return false;
        return (t.average >= 1 || t.average == -1);
    }

    // Endpoint-exact uniform grid: level i of count over [lo,hi].
    double GridLevel(int i, int count, double lo, double hi)
    {
        double u = (double)i / (double)(count - 1);
        return lo * (1.0 - u) + hi * u;
    }

    std::string FormatLevelName(const char* word, double level)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "%s %g", word, level);
        return std::string(buf);
    }

    std::string FormatNodeName(const char* word, double r, double g, double b)
    {
        char buf[192];
        snprintf(buf, sizeof(buf), "%s %g/%g/%g", word, r, g, b);
        return std::string(buf);
    }

    struct LatticeNode { double v[3]; };

    // The one lattice walk: red slowest / blue fastest, the nesting
    // GenerateProfileColors uses, with `axis` carrying the n per-index
    // component values. A mapped lattice (the inset shell) therefore walks
    // exactly the node order of the plain one.
    void CollectLattice(int n, bool shellOnly, const double* axis,
                        std::vector<LatticeNode>& out)
    {
        size_t total = (size_t)n * (size_t)n * (size_t)n;
        if (shellOnly)
            total -= (size_t)(n - 2) * (size_t)(n - 2) * (size_t)(n - 2);
        out.clear();
        out.reserve(total);

        for (int r = 0; r < n; ++r)
        {
            bool rEdge = (r == 0 || r == n - 1);
            for (int g = 0; g < n; ++g)
            {
                bool gEdge = (g == 0 || g == n - 1);
                for (int b = 0; b < n; ++b)
                {
                    bool bEdge = (b == 0 || b == n - 1);
                    if (shellOnly && !rEdge && !gEdge && !bEdge)
                        continue;           // interior node
                    LatticeNode node;
                    node.v[0] = axis[r];
                    node.v[1] = axis[g];
                    node.v[2] = axis[b];
                    out.push_back(node);
                }
            }
        }
    }

    bool ValidRampArgs(int count, double lo, double hi)
    {
        if (count < 2)
            return false;
        if (!ValidComponent(lo) || !ValidComponent(hi))
            return false;
        return (lo < hi);
    }

    // Lexicographic component order for the duplicate-detection set. Stored
    // values are never NaN and never -0.0, so operator< is a strict weak
    // ordering that agrees with bit-pattern identity.
    struct RgbLess
    {
        bool operator()(const double* a, const double* b) const
        {
            for (int k = 0; k < 3; ++k)
            {
                if (a[k] < b[k]) return true;
                if (b[k] < a[k]) return false;
            }
            return false;
        }
    };
}

PatchSequence::PatchSequence()
    : m_nextId(0), m_extended(false)
{
}

// Every Add* path ends here: the identity index and the next fresh
// identity are maintained in one place.
void PatchSequence::Append(const Entry& e)
{
    size_t index = m_entries.size();
    m_entries.push_back(e);
    if (m_firstAt.find(e.id) == m_firstAt.end())
        m_firstAt[e.id] = index;
    if (e.id >= m_nextId)
        m_nextId = e.id + 1;
}

// The ONE home of role knowledge: everything that asks "is this patch
// measured / fitted / exportable" asks here, never by letter comparison.
bool PatchSequence::RoleIsMeasured(char role)
{
    return ValidRole(role) && role != 'I';  // 'I' is shown, never read
}

bool PatchSequence::RoleIsFitted(char role)
{
    return role == 'C' || role == 'V';
}

bool PatchSequence::RoleIsExportable(char role)
{
    return role == 'C' || role == 'V';
}

// A group name follows the name rules, is non-empty, and is a single
// token (it is written bare in the file's GROUP directive).
bool PatchSequence::ValidGroupName(const std::string& group)
{
    if (group.empty())
        return false;
    if (!ValidName(group))
        return false;
    return group.find(' ') == std::string::npos;
}

void PatchSequence::Swap(PatchSequence& other)
{
    m_entries.swap(other.m_entries);
    m_firstAt.swap(other.m_firstAt);
    std::swap(m_nextId, other.m_nextId);
    std::swap(m_extended, other.m_extended);
    m_lastError.swap(other.m_lastError);
}

int PatchSequence::Count() const
{
    return (int)m_entries.size();
}

bool PatchSequence::ValidRole(char role)
{
    return role == 'C' || role == 'A' || role == 'P' || role == 'V'
        || role == 'I' || role == 'X';
}

bool PatchSequence::ValidComponentHere(double v) const
{
    return m_extended ? ValidExtendedComponent(v) : ValidComponent(v);
}

PatchTiming PatchTiming::Resolved(const PatchTiming& row, const PatchTiming& group,
                                  const PatchTiming& defaults)
{
    PatchTiming r;
    r.settle      = IsSet(row.settle)      ? row.settle      : IsSet(group.settle)      ? group.settle      : IsSet(defaults.settle)      ? defaults.settle      : 0.0;
    r.hold        = IsSet(row.hold)        ? row.hold        : IsSet(group.hold)        ? group.hold        : IsSet(defaults.hold)        ? defaults.hold        : 0.0;
    r.integration = IsSet(row.integration) ? row.integration : IsSet(group.integration) ? group.integration : IsSet(defaults.integration) ? defaults.integration : 0.0;
    r.average     = IsSet(row.average)     ? row.average     : IsSet(group.average)     ? group.average     : IsSet(defaults.average)     ? defaults.average     : 0;
    return r;
}

int PatchSequence::DefinitionIndex(int id) const
{
    std::map<int, size_t>::const_iterator it = m_firstAt.find(id);
    return (it == m_firstAt.end()) ? -1 : (int)it->second;
}

int PatchSequence::GetIdentity(int index) const
{
    if (index < 0 || index >= (int)m_entries.size())
        return -1;
    return m_entries[(size_t)index].id;
}

char PatchSequence::GetRole(int index) const
{
    if (index < 0 || index >= (int)m_entries.size())
        return '\0';
    return m_entries[(size_t)index].role;
}

bool PatchSequence::GetGroup(int index, std::string& group) const
{
    if (index < 0 || index >= (int)m_entries.size())
        return false;
    group = m_entries[(size_t)index].group;
    return true;
}

bool PatchSequence::GetTiming(int index, PatchTiming& timing) const
{
    if (index < 0 || index >= (int)m_entries.size())
        return false;
    timing = m_entries[(size_t)index].timing;
    return true;
}

bool PatchSequence::SetRole(int index, char role)
{
    if (index < 0 || index >= (int)m_entries.size())
        return false;
    if (!ValidRole(role))
        return false;
    m_entries[(size_t)index].role = role;
    return true;
}

bool PatchSequence::SetGroup(int index, const std::string& group)
{
    if (index < 0 || index >= (int)m_entries.size())
        return false;
    if (!group.empty() && !ValidGroupName(group))
        return false;                       // empty clears the group
    m_entries[(size_t)index].group = group;
    return true;
}

bool PatchSequence::SetTiming(int index, const PatchTiming& timing)
{
    if (index < 0 || index >= (int)m_entries.size())
        return false;
    if (!ValidTiming(timing))
        return false;
    m_entries[(size_t)index].timing = timing;
    return true;
}

// Widening is always safe; narrowing is refused while an entry sits
// outside 0..100, so the invariant "every stored value is in the accepted
// span" holds at all times.
bool PatchSequence::SetExtended(bool extended)
{
    if (!extended)
    {
        for (size_t i = 0; i < m_entries.size(); ++i)
            for (int k = 0; k < 3; ++k)
                if (!ValidComponent(m_entries[i].rgb[k]))
                    return false;
    }
    m_extended = extended;
    return true;
}

bool PatchSequence::IsExtended() const
{
    return m_extended;
}

bool PatchSequence::AddRow(int id, double rPct, double gPct, double bPct,
                           const std::string& name, char role)
{
    if (id < 0 || id > kMaxIdentity)
        return false;
    if (!ValidRole(role))
        return false;
    if (!ValidComponentHere(rPct) || !ValidComponentHere(gPct)
        || !ValidComponentHere(bPct))
        return false;
    if (!ValidName(name))
        return false;

    double rgb[3];
    rgb[0] = NormalizeComponent(rPct);
    rgb[1] = NormalizeComponent(gPct);
    rgb[2] = NormalizeComponent(bPct);

    // A repeat must repeat: the first entry carrying this identity is the
    // definition, and its stimulus is binding, bit for bit. The index
    // makes this O(log n), so reading a 10000-row file is not quadratic.
    std::map<int, size_t>::const_iterator at = m_firstAt.find(id);
    if (at != m_firstAt.end())
    {
        const Entry& def = m_entries[at->second];
        if (def.rgb[0] != rgb[0] || def.rgb[1] != rgb[1] || def.rgb[2] != rgb[2])
            return false;
    }

    Entry e;
    e.id = id;
    e.role = role;
    e.rgb[0] = rgb[0];
    e.rgb[1] = rgb[1];
    e.rgb[2] = rgb[2];
    e.name = name;
    Append(e);
    return true;
}

// The near-black placement rule: a power-law grid over [lo,hi]. The two
// endpoints are taken as written rather than through pow, so the ramp
// starts at exactly loPct and ends at exactly hiPct whatever the exponent.
bool PatchSequence::AddGrayPowerRamp(int count, double loPct, double hiPct,
                                     double exponent)
{
    if (!ValidRampArgs(count, loPct, hiPct))
        return false;
    if (!_finite(exponent) || exponent <= 0.0)
        return false;

    m_entries.reserve(m_entries.size() + (size_t)count);
    int nextId = m_nextId;
    for (int i = 0; i < count; ++i)
    {
        double level;
        if (i == 0)
            level = loPct;
        else if (i == count - 1)
            level = hiPct;
        else
        {
            double u = (double)i / (double)(count - 1);
            level = loPct + (hiPct - loPct) * pow(u, exponent);
        }
        level = NormalizeComponent(level);

        Entry e;
        e.id = nextId++;
        e.role = 'C';
        e.rgb[0] = level;
        e.rgb[1] = level;
        e.rgb[2] = level;
        e.name = FormatLevelName("Gray", level);
        Append(e);
    }
    return true;
}

// The shell walk with every component mapped from [0,100] onto
// [insetPct, 100-insetPct] - endpoint-exact, so a face node lands on the
// inset value itself.
bool PatchSequence::AddShellInset(int n, double insetPct)
{
    if (n < 2 || n > 33)
        return false;
    if (!_finite(insetPct) || insetPct <= 0.0 || insetPct >= 50.0)
        return false;

    std::vector<double> axis((size_t)n);
    for (int i = 0; i < n; ++i)
    {
        double u = GridLevel(i, n, 0.0, 100.0) / 100.0;
        axis[(size_t)i] = insetPct * (1.0 - u) + (100.0 - insetPct) * u;
    }

    std::vector<LatticeNode> nodes;
    CollectLattice(n, true, &axis[0], nodes);

    m_entries.reserve(m_entries.size() + nodes.size());
    int nextId = m_nextId;
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        Entry e;
        e.id = nextId++;
        e.role = 'C';
        e.rgb[0] = NormalizeComponent(nodes[i].v[0]);
        e.rgb[1] = NormalizeComponent(nodes[i].v[1]);
        e.rgb[2] = NormalizeComponent(nodes[i].v[2]);
        e.name = FormatNodeName("Inset", e.rgb[0], e.rgb[1], e.rgb[2]);
        Append(e);
    }
    return true;
}

bool PatchSequence::AddRepeats(int count)
{
    if (count < 1)
        return false;

    // Definitions: the first entry carrying each identity, role 'C'. The
    // map iterates in identity order, which is the documented order.
    std::map<int, size_t> firstAt;
    for (size_t i = 0; i < m_entries.size(); ++i)
        if (firstAt.find(m_entries[i].id) == firstAt.end())
            firstAt[m_entries[i].id] = i;

    std::vector<size_t> defs;
    for (std::map<int, size_t>::const_iterator it = firstAt.begin();
         it != firstAt.end(); ++it)
        if (m_entries[it->second].role == 'C')
            defs.push_back(it->second);

    if (count > (int)defs.size())
        return false;
    size_t stride = defs.size() / (size_t)count;    // at least 1

    std::vector<Entry> added;
    added.reserve((size_t)count);
    for (int t = 0; t < count; ++t)
    {
        const Entry& src = m_entries[defs[(size_t)t * stride]];
        Entry e;
        e.id = src.id;
        e.role = 'C';
        e.rgb[0] = src.rgb[0];
        e.rgb[1] = src.rgb[1];
        e.rgb[2] = src.rgb[2];
        e.name = src.name.empty() ? std::string("repeat")
                                  : src.name + " repeat";
        added.push_back(e);
    }
    for (size_t i = 0; i < added.size(); ++i)
        Append(added[i]);
    return true;
}

bool PatchSequence::GetPatch(int index, double rgb[3], std::string* name) const
{
    if (index < 0 || index >= (int)m_entries.size())
        return false;
    const Entry& e = m_entries[(size_t)index];
    rgb[0] = e.rgb[0];
    rgb[1] = e.rgb[1];
    rgb[2] = e.rgb[2];
    if (name)
        *name = e.name;
    return true;
}

bool PatchSequence::AddPatch(double rPct, double gPct, double bPct,
                             const std::string& name)
{
    if (!ValidComponentHere(rPct) || !ValidComponentHere(gPct) || !ValidComponentHere(bPct))
        return false;
    if (!ValidName(name))
        return false;
    Entry e;
    e.id = m_nextId;
    e.role = 'C';
    e.rgb[0] = NormalizeComponent(rPct);
    e.rgb[1] = NormalizeComponent(gPct);
    e.rgb[2] = NormalizeComponent(bPct);
    e.name = name;
    Append(e);
    return true;
}

// channel -1 = neutral (all three components at the level, word "Gray");
// 0..2 = that component at the level, the others 0. Validation exhausts
// every refusal before the first append, so a false return has touched
// nothing.
bool PatchSequence::AppendRamp(int channel, int count,
                               double loPct, double hiPct)
{
    if (channel < -1 || channel > 2)
        return false;
    if (!ValidRampArgs(count, loPct, hiPct))
        return false;

    static const char* kWords[4] = { "Gray", "Red", "Green", "Blue" };
    const char* word = kWords[channel + 1];

    m_entries.reserve(m_entries.size() + (size_t)count);
    int nextId = m_nextId;
    for (int i = 0; i < count; ++i)
    {
        double level = NormalizeComponent(GridLevel(i, count, loPct, hiPct));
        Entry e;
        e.id = nextId++;
        e.role = 'C';
        if (channel < 0)
        {
            e.rgb[0] = level;
            e.rgb[1] = level;
            e.rgb[2] = level;
        }
        else
        {
            e.rgb[0] = 0.0;
            e.rgb[1] = 0.0;
            e.rgb[2] = 0.0;
            e.rgb[channel] = level;
        }
        e.name = FormatLevelName(word, level);
        Append(e);
    }
    return true;
}

bool PatchSequence::AddGrayRamp(int count, double loPct, double hiPct)
{
    return AppendRamp(-1, count, loPct, hiPct);
}

bool PatchSequence::AddChannelRamp(int channel, int count,
                                   double loPct, double hiPct)
{
    if (channel < 0)
        return false;                       // -1 is AppendRamp's gray code
    return AppendRamp(channel, count, loPct, hiPct);
}

// One lattice walker for grid and shell, so the two can never disagree on
// order, node values, or names.
bool PatchSequence::AppendLattice(int n, bool shellOnly)
{
    if (n < 2 || n > 33)
        return false;

    std::vector<double> axis((size_t)n);
    for (int i = 0; i < n; ++i)
        axis[(size_t)i] = GridLevel(i, n, 0.0, 100.0);

    std::vector<LatticeNode> nodes;
    CollectLattice(n, shellOnly, &axis[0], nodes);

    m_entries.reserve(m_entries.size() + nodes.size());
    int nextId = m_nextId;
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        Entry e;
        e.id = nextId++;
        e.role = 'C';
        e.rgb[0] = nodes[i].v[0];
        e.rgb[1] = nodes[i].v[1];
        e.rgb[2] = nodes[i].v[2];
        e.name = FormatNodeName("Cube", e.rgb[0], e.rgb[1], e.rgb[2]);
        Append(e);
    }
    return true;
}

bool PatchSequence::AddCubeGrid(int n)
{
    return AppendLattice(n, false);
}

bool PatchSequence::AddCubeShell(int n)
{
    return AppendLattice(n, true);
}

int PatchSequence::Deduplicate(std::vector< std::pair<int, int> >* renumbering)
{
    if (renumbering)
        renumbering->clear();
    // A DEFINITION is the first entry carrying its identity. Only role-'C'
    // definitions are removal candidates, and only they can cause a
    // removal: repeats and other roles are carried through untouched.
    std::set<int> seenIds;
    std::vector<bool> isDefinition(m_entries.size(), false);
    for (size_t i = 0; i < m_entries.size(); ++i)
        isDefinition[i] = seenIds.insert(m_entries[i].id).second;

    std::map<const double*, int, RgbLess> byRgb;    // stimulus -> survivor
    std::map<int, int> replaced;                    // removed id -> survivor
    std::vector<Entry> kept;
    kept.reserve(m_entries.size());
    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        const Entry& e = m_entries[i];
        if (isDefinition[i] && e.role == 'C')
        {
            std::map<const double*, int, RgbLess>::const_iterator it =
                byRgb.find(e.rgb);
            if (it != byRgb.end())
            {
                replaced[e.id] = it->second;
                continue;                   // a duplicate definition
            }
            byRgb.insert(std::make_pair((const double*)e.rgb, e.id));
        }
        kept.push_back(e);
    }
    int removed = (int)(m_entries.size() - kept.size());

    // Renumber: every surviving definition takes the next identity in
    // sequence order, and every repeat follows the definition it now
    // refers to (the surviving equal one, when its own was removed).
    std::map<int, int> renumbered;
    int next = 0;
    for (size_t i = 0; i < kept.size(); ++i)
    {
        int id = kept[i].id;
        std::map<int, int>::const_iterator r = replaced.find(id);
        if (r != replaced.end())
            id = r->second;
        std::map<int, int>::const_iterator n = renumbered.find(id);
        if (n == renumbered.end())
        {
            renumbered[id] = next;
            kept[i].id = next;
            ++next;
        }
        else
            kept[i].id = n->second;
    }

    // One (old, new) pair per identity that existed before the call, in
    // old-identity order, so anything naming patches by identity outside
    // the sequence can follow: a removed duplicate maps to the NEW number
    // of the survivor that replaced it.
    if (renumbering)
    {
        renumbering->reserve(seenIds.size());
        for (std::set<int>::const_iterator it = seenIds.begin();
             it != seenIds.end(); ++it)
        {
            int target = *it;
            std::map<int, int>::const_iterator r = replaced.find(target);
            if (r != replaced.end())
                target = r->second;
            std::map<int, int>::const_iterator n = renumbered.find(target);
            if (n != renumbered.end())
                renumbering->push_back(std::make_pair(*it, n->second));
        }
    }

    m_entries.swap(kept);
    m_firstAt.clear();
    m_nextId = 0;
    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        if (m_firstAt.find(m_entries[i].id) == m_firstAt.end())
            m_firstAt[m_entries[i].id] = i;
        if (m_entries[i].id >= m_nextId)
            m_nextId = m_entries[i].id + 1;
    }
    return removed;
}

void PatchSequence::Clear()
{
    m_entries.clear();
    m_firstAt.clear();
    m_nextId = 0;
}

bool PatchSequence::WriteCsv(std::string& out, int bits, int range) const
{
    if (m_entries.empty())
    {
        m_lastError = "empty patch sequence";
        return false;
    }
    if (bits != 8 && bits != 10)
    {
        m_lastError = "unsupported bit depth";
        return false;
    }
    if (CsvRangeWord(range)[0] == '\0')
    {
        m_lastError = "unsupported csv range";
        return false;
    }
    // No silent clamp, ever: a component this encoding cannot carry back
    // is refused rather than quantized into a different value.
    for (size_t i = 0; i < m_entries.size(); ++i)
        for (int k = 0; k < 3; ++k)
            if (!CsvPercentRepresentable(m_entries[i].rgb[k], bits, range))
            {
                m_lastError = "component not representable at this encoding";
                return false;
            }

    char buf[256];
    std::string text;
    text.reserve(m_entries.size() * 48 + 32);
    text = CsvFormatHeaderLine(bits, range);

    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        const Entry& e = m_entries[i];
        snprintf(buf, sizeof(buf), "%d,%d,%d,%d", e.id,
                 CsvCodeFromPercent(e.rgb[0], bits, range),
                 CsvCodeFromPercent(e.rgb[1], bits, range),
                 CsvCodeFromPercent(e.rgb[2], bits, range));
        text += buf;
        if (!e.name.empty())
        {
            text += ",";
            text += e.name;
        }
        text += "\n";
    }

    out.swap(text);
    m_lastError.clear();
    return true;
}

bool PatchSequence::WriteCsvFile(const char* path, int bits, int range) const
{
    std::string text;
    if (!WriteCsv(text, bits, range))
        return false;                       // WriteCsv set m_lastError

    std::string reason;
    if (!WriteTextFileAtomic(path, text, reason))
    {
        m_lastError = reason;
        return false;
    }
    m_lastError.clear();
    return true;
}

// The atomic replace every libHCFR file writer shares: a sibling temporary
// moved over the target, so a failed write never destroys an existing file.
bool WriteTextFileAtomic(const char* path, const std::string& text,
                         std::string& reason)
{
    if (path == 0 || path[0] == '\0')
    {
        reason = "no output path";
        return false;
    }

    std::string tmp = std::string(path) + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f)
    {
        reason = "cannot open output file";
        return false;
    }
    size_t want = text.size();
    size_t got = fwrite(text.data(), 1, want, f);
    bool ok = (got == want);
    if (fclose(f) != 0)
        ok = false;
    if (!ok)
    {
        remove(tmp.c_str());
        reason = "cannot write output file";
        return false;
    }

#ifdef LIBHCFR_HAS_WIN32_API
    // Atomic replace: at every instant the destination is either the old
    // file or the new one, so neither version can be lost.
    if (!MoveFileExA(tmp.c_str(), path, MOVEFILE_REPLACE_EXISTING))
    {
        remove(tmp.c_str());                // destination is untouched
        reason = "cannot replace output file";
        return false;
    }
#else
    // rename() does not overwrite an existing destination everywhere, so
    // fall back to remove-then-rename. The destination is only removed
    // once the complete temporary is safely on disk, and if the second
    // rename STILL fails the temporary is left in place so the new
    // content survives too.
    if (rename(tmp.c_str(), path) != 0)
    {
        remove(path);
        if (rename(tmp.c_str(), path) != 0)
        {
            reason = "cannot replace output file; new content kept in " + tmp;
            return false;
        }
    }
#endif

    return true;
}
