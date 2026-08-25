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

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace
{
    // Range codes accepted by WriteCsv. These MUST match the shared
    // CSV_RANGE_* enum in Color.h (the source of truth:
    // CSV_RANGE_NONE = -1, CSV_RANGE_FULL = 0, CSV_RANGE_LEGAL = 1,
    // CSV_RANGE_EXTENDED = 2). They are restated here as plain ints
    // because Color.h drags MFC types into its declarations and this
    // translation unit must stay MFC-free.
    const int kRangeFull     = 0;
    const int kRangeLegal    = 1;
    const int kRangeExtended = 2;

    bool ValidComponent(double v)
    {
        if (!_finite(v))
            return false;
        return (v >= 0.0 && v <= 100.0);
    }

    bool ValidName(const std::string& name)
    {
        return name.find_first_of(",\r\n") == std::string::npos;
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
    // chosen encoding. Percents here are always within 0..100.
    int PercentToCode(double pct, int bits, int range)
    {
        if (range == kRangeFull)
        {
            double maxCode = (double)((1 << bits) - 1);
            return (int)floor(pct * maxCode / 100.0 + 0.5);
        }
        double black = 16.0  * (double)(1 << (bits - 8));
        double white = 235.0 * (double)(1 << (bits - 8));
        return (int)floor(black + pct * (white - black) / 100.0 + 0.5);
    }

    const char* RangeWord(int range)
    {
        if (range == kRangeFull)
            return "FULL";
        if (range == kRangeLegal)
            return "LEGAL";
        return "EXTENDED";
    }
}

PatchSequence::PatchSequence()
{
}

int PatchSequence::Count() const
{
    return (int)m_entries.size();
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
    if (!ValidComponent(rPct) || !ValidComponent(gPct) || !ValidComponent(bPct))
        return false;
    if (!ValidName(name))
        return false;
    Entry e;
    e.rgb[0] = rPct;
    e.rgb[1] = gPct;
    e.rgb[2] = bPct;
    e.name = name;
    m_entries.push_back(e);
    return true;
}

bool PatchSequence::AddGrayRamp(int count, double loPct, double hiPct)
{
    if (!ValidRampArgs(count, loPct, hiPct))
        return false;

    std::vector<Entry> built;
    built.reserve((size_t)count);
    for (int i = 0; i < count; ++i)
    {
        double level = GridLevel(i, count, loPct, hiPct);
        Entry e;
        e.rgb[0] = level;
        e.rgb[1] = level;
        e.rgb[2] = level;
        e.name = FormatLevelName("Gray", level);
        built.push_back(e);
    }
    m_entries.insert(m_entries.end(), built.begin(), built.end());
    return true;
}

bool PatchSequence::AddChannelRamp(int channel, int count,
                                   double loPct, double hiPct)
{
    if (channel < 0 || channel > 2)
        return false;
    if (!ValidRampArgs(count, loPct, hiPct))
        return false;

    static const char* kWords[3] = { "Red", "Green", "Blue" };

    std::vector<Entry> built;
    built.reserve((size_t)count);
    for (int i = 0; i < count; ++i)
    {
        double level = GridLevel(i, count, loPct, hiPct);
        Entry e;
        e.rgb[0] = 0.0;
        e.rgb[1] = 0.0;
        e.rgb[2] = 0.0;
        e.rgb[channel] = level;
        e.name = FormatLevelName(kWords[channel], level);
        built.push_back(e);
    }
    m_entries.insert(m_entries.end(), built.begin(), built.end());
    return true;
}

bool PatchSequence::AddCubeGrid(int n)
{
    if (n < 2 || n > 33)
        return false;

    std::vector<Entry> built;
    built.reserve((size_t)n * (size_t)n * (size_t)n);
    for (int r = 0; r < n; ++r)
    {
        double rv = GridLevel(r, n, 0.0, 100.0);
        for (int g = 0; g < n; ++g)
        {
            double gv = GridLevel(g, n, 0.0, 100.0);
            for (int b = 0; b < n; ++b)
            {
                double bv = GridLevel(b, n, 0.0, 100.0);
                Entry e;
                e.rgb[0] = rv;
                e.rgb[1] = gv;
                e.rgb[2] = bv;
                e.name = FormatCubeName(rv, gv, bv);
                built.push_back(e);
            }
        }
    }
    m_entries.insert(m_entries.end(), built.begin(), built.end());
    return true;
}

bool PatchSequence::AddCubeShell(int n)
{
    if (n < 2 || n > 33)
        return false;

    std::vector<Entry> built;
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
                if (!rEdge && !gEdge && !bEdge)
                    continue;               // interior node
                double bv = GridLevel(b, n, 0.0, 100.0);
                Entry e;
                e.rgb[0] = rv;
                e.rgb[1] = gv;
                e.rgb[2] = bv;
                e.name = FormatCubeName(rv, gv, bv);
                built.push_back(e);
            }
        }
    }
    m_entries.insert(m_entries.end(), built.begin(), built.end());
    return true;
}

int PatchSequence::Deduplicate()
{
    std::vector<Entry> kept;
    kept.reserve(m_entries.size());
    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        const Entry& e = m_entries[i];
        bool seen = false;
        for (size_t k = 0; k < kept.size(); ++k)
        {
            if (kept[k].rgb[0] == e.rgb[0]
                && kept[k].rgb[1] == e.rgb[1]
                && kept[k].rgb[2] == e.rgb[2])
            {
                seen = true;
                break;
            }
        }
        if (!seen)
            kept.push_back(e);
    }
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
    snprintf(buf, sizeof(buf), "# BITDEPTH %d, RANGE %s\n", bits,
             RangeWord(range));
    text = buf;

    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        const Entry& e = m_entries[i];
        snprintf(buf, sizeof(buf), "%d,%d,%d,%d", (int)i,
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

    out = text;
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

    // Write a sibling temporary file, then rename it over the target, so a
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

    // rename() does not overwrite an existing destination on Windows, so
    // fall back to remove-then-rename. The destination is only removed once
    // the complete temporary file is safely on disk.
    if (rename(tmp.c_str(), path) != 0)
    {
        remove(path);
        if (rename(tmp.c_str(), path) != 0)
        {
            remove(tmp.c_str());
            m_lastError = "cannot replace output file";
            return false;
        }
    }

    m_lastError.clear();
    return true;
}
