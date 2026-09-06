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

#include "libHCFR_Config.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <set>

#ifdef LIBHCFR_HAS_WIN32_API
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

// The percent -> device-code quantizer of the live signal path
// (Color.cpp). Declared here instead of including Color.h, which drags MFC
// types into the translation unit; the signature is plain types, so any
// drift is a link error.
extern int PiPercentToCode(double percent, bool is16_235, int bits);

namespace
{
    // Range codes accepted by WriteCsv. These MUST match the shared
    // CSV_RANGE_* enum in Color.h (the source of truth:
    // CSV_RANGE_NONE = -1, CSV_RANGE_FULL = 0, CSV_RANGE_LEGAL = 1,
    // CSV_RANGE_EXTENDED = 2). They are restated here as plain ints
    // because Color.h drags MFC types into its declarations and this
    // translation unit must stay MFC-free; the T17 round trip through the
    // real importer is the executable proof the numbers agree.
    const int kRangeFull     = 0;
    const int kRangeLegal    = 1;
    const int kRangeExtended = 2;

    bool ValidComponent(double v)
    {
        if (!_finite(v))
            return false;
        return (v >= 0.0 && v <= 100.0);
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

    std::string FormatCubeName(double r, double g, double b)
    {
        char buf[192];
        snprintf(buf, sizeof(buf), "Cube %g/%g/%g", r, g, b);
        return std::string(buf);
    }

    bool ValidRampArgs(int count, double lo, double hi)
    {
        if (count < 2)
            return false;
        if (!ValidComponent(lo) || !ValidComponent(hi))
            return false;
        return (lo < hi);
    }

    // Nearest-integer inverse of CsvCodeToPercent (Color.cpp) for the
    // chosen encoding - which is exactly the live signal path's
    // PiPercentToCode: FULL is the full 0..2^bits-1 code range, LEGAL and
    // EXTENDED the video range, and its clamps are no-ops for the 0..100
    // percents this class guarantees.
    int PercentToCode(double pct, int bits, int range)
    {
        return PiPercentToCode(pct, range != kRangeFull, bits);
    }

    const char* RangeWord(int range)
    {
        if (range == kRangeFull)
            return "FULL";
        if (range == kRangeLegal)
            return "LEGAL";
        return "EXTENDED";
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
    : m_extended(false)
{
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

// Fresh identities continue from the largest present.
int PatchSequence::NextIdentity() const
{
    int next = 0;
    for (size_t i = 0; i < m_entries.size(); ++i)
        if (m_entries[i].id >= next)
            next = m_entries[i].id + 1;
    return next;
}

// NOT YET IMPLEMENTED (red baseline): the extended span. Until then the
// strict 0..100 rule applies regardless of m_extended.
bool PatchSequence::ValidComponentHere(double v) const
{
    return ValidComponent(v);
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

// NOT YET IMPLEMENTED: red-baseline stubs so the T20 oracles link and fail.
// The behavioral contract lives in PatchSequence.h.

bool PatchSequence::SetRole(int, char)
{
    return false;
}

bool PatchSequence::SetGroup(int, const std::string&)
{
    return false;
}

bool PatchSequence::SetTiming(int, const PatchTiming&)
{
    return false;
}

bool PatchSequence::SetExtended(bool)
{
    return false;
}

bool PatchSequence::IsExtended() const
{
    return m_extended;
}

bool PatchSequence::AddRow(int, double, double, double, const std::string&, char)
{
    return false;
}

bool PatchSequence::AddGrayPowerRamp(int, double, double, double)
{
    return false;
}

bool PatchSequence::AddShellInset(int, double)
{
    return false;
}

bool PatchSequence::AddRepeats(int)
{
    return false;
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
    e.id = NextIdentity();
    e.role = 'C';
    e.rgb[0] = NormalizeComponent(rPct);
    e.rgb[1] = NormalizeComponent(gPct);
    e.rgb[2] = NormalizeComponent(bPct);
    e.name = name;
    m_entries.push_back(e);
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
    int nextId = NextIdentity();
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
        m_entries.push_back(e);
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

    size_t total = (size_t)n * (size_t)n * (size_t)n;
    if (shellOnly)
        total -= (size_t)(n - 2) * (size_t)(n - 2) * (size_t)(n - 2);
    m_entries.reserve(m_entries.size() + total);
    int nextId = NextIdentity();

    for (int r = 0; r < n; ++r)
    {
        double rv = GridLevel(r, n, 0.0, 100.0);
        bool rEdge = (r == 0 || r == n - 1);
        for (int g = 0; g < n; ++g)
        {
            double gv = GridLevel(g, n, 0.0, 100.0);
            bool gEdge = (g == 0 || g == n - 1);
            for (int b = 0; b < n; ++b)
            {
                bool bEdge = (b == 0 || b == n - 1);
                if (shellOnly && !rEdge && !gEdge && !bEdge)
                    continue;               // interior node
                double bv = GridLevel(b, n, 0.0, 100.0);
                Entry e;
                e.id = nextId++;
                e.role = 'C';
                e.rgb[0] = rv;
                e.rgb[1] = gv;
                e.rgb[2] = bv;
                e.name = FormatCubeName(rv, gv, bv);
                m_entries.push_back(e);
            }
        }
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

int PatchSequence::Deduplicate()
{
    std::set<const double*, RgbLess> seen;
    std::vector<Entry> kept;
    kept.reserve(m_entries.size());
    for (size_t i = 0; i < m_entries.size(); ++i)
        if (seen.insert(m_entries[i].rgb).second)
            kept.push_back(m_entries[i]);
    int removed = (int)(m_entries.size() - kept.size());
    m_entries.swap(kept);
    return removed;
}

void PatchSequence::Clear()
{
    m_entries.clear();
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
    if (range != kRangeFull && range != kRangeLegal && range != kRangeExtended)
    {
        m_lastError = "unsupported csv range";
        return false;
    }

    char buf[256];
    std::string text;
    text.reserve(m_entries.size() * 48 + 32);
    snprintf(buf, sizeof(buf), "# BITDEPTH %d, RANGE %s\n", bits,
             RangeWord(range));
    text = buf;

    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        const Entry& e = m_entries[i];
        snprintf(buf, sizeof(buf), "%d,%d,%d,%d", e.id,
                 PercentToCode(e.rgb[0], bits, range),
                 PercentToCode(e.rgb[1], bits, range),
                 PercentToCode(e.rgb[2], bits, range));
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
    if (path == 0 || path[0] == '\0')
    {
        m_lastError = "no output path";
        return false;
    }

    std::string text;
    if (!WriteCsv(text, bits, range))
        return false;                       // WriteCsv set m_lastError

    // Write a sibling temporary file, then move it over the target, so a
    // failed write never destroys an existing file.
    std::string tmp = std::string(path) + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f)
    {
        m_lastError = "cannot open output file";
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
        m_lastError = "cannot write output file";
        return false;
    }

#ifdef LIBHCFR_HAS_WIN32_API
    // Atomic replace: at every instant the destination is either the old
    // file or the new one, so neither version can be lost.
    if (!MoveFileExA(tmp.c_str(), path, MOVEFILE_REPLACE_EXISTING))
    {
        remove(tmp.c_str());                // destination is untouched
        m_lastError = "cannot replace output file";
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
            m_lastError = "cannot replace output file; new content kept in "
                          + tmp;
            return false;
        }
    }
#endif

    m_lastError.clear();
    return true;
}
