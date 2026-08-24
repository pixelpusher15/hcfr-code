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

// CubeLUT implementation - reader and writer for the .cube text format,
// written from the published Adobe Cube LUT Specification 1.0 description
// (Adobe Systems, "Cube LUT Specification", version 1.0, 2013). No code
// from any other project. See CubeLUT.h for the full contract.
//
// Locale note: the printf/scanf families honor the global C locale, so a
// user running under a comma-decimal locale would otherwise write and read
// files that no other tool can parse. Every numeric conversion here goes
// through the MSVC _l variants bound to a cached "C" locale, so the '.'
// decimal separator is used no matter what the host locale is.

#include "CubeLUT.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <float.h>

namespace
{
    const int kMinSize = 2;
    const int kMaxSize = 256;

    // Cached "C" locale used for every numeric conversion in this file.
    _locale_t CNumericLocale()
    {
        static _locale_t loc = _create_locale(LC_NUMERIC, "C");
        return loc;
    }

    bool IsFiniteValue(double v)
    {
        return _finite(v) != 0;
    }

    void AppendDouble(std::string& out, double v)
    {
        char buf[64];
        buf[0] = '\0';
        _snprintf_l(buf, sizeof(buf) - 1, "%.17g", CNumericLocale(), v);
        buf[sizeof(buf) - 1] = '\0';
        out += buf;
    }

    bool IsSpaceChar(char c)
    {
        return c == ' ' || c == '\t' || c == '\r';
    }

    // Splits on runs of spaces and tabs. Carriage returns have already been
    // stripped from the line, but tolerate them here as separators too.
    void SplitTokens(const std::string& line, std::vector<std::string>& tokens)
    {
        tokens.clear();
        size_t i = 0;
        while (i < line.size())
        {
            while (i < line.size() && IsSpaceChar(line[i]))
                ++i;
            size_t start = i;
            while (i < line.size() && !IsSpaceChar(line[i]))
                ++i;
            if (i > start)
                tokens.push_back(line.substr(start, i - start));
        }
    }

    // Locale-independent conversion of a complete token. Returns false when
    // anything is left over after the number; 'value' may be non-finite so
    // callers can tell "nan" apart from junk.
    bool ParseNumberToken(const std::string& token, double& value)
    {
        if (token.empty())
            return false;
        const char* begin = token.c_str();
        char* end = 0;
        value = _strtod_l(begin, &end, CNumericLocale());
        return end == begin + token.size();
    }

    bool ParseFiniteToken(const std::string& token, double& value)
    {
        return ParseNumberToken(token, value) && IsFiniteValue(value);
    }

    bool ParseIntToken(const std::string& token, int& value)
    {
        if (token.empty())
            return false;
        const char* begin = token.c_str();
        char* end = 0;
        long v = strtol(begin, &end, 10);
        if (end != begin + token.size())
            return false;
        if (v < -2147483647L || v > 2147483647L)
            return false;
        value = (int)v;
        return true;
    }

    bool IsKeywordStart(char c)
    {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
    }

    // Splits the text into lines, accepting LF and CRLF.
    void SplitLines(const std::string& text, std::vector<std::string>& lines)
    {
        lines.clear();
        size_t pos = 0;
        while (pos < text.size())
        {
            size_t nl = text.find('\n', pos);
            size_t end = (nl == std::string::npos) ? text.size() : nl;
            size_t len = end - pos;
            if (len > 0 && text[pos + len - 1] == '\r')
                --len;
            lines.push_back(text.substr(pos, len));
            if (nl == std::string::npos)
                break;
            pos = nl + 1;
        }
    }
}

CubeLUT::CubeLUT()
    : m_size(0)
{
    m_domainMin[0] = m_domainMin[1] = m_domainMin[2] = 0.0;
    m_domainMax[0] = m_domainMax[1] = m_domainMax[2] = 1.0;
}

bool CubeLUT::Create(int size)
{
    if (size < kMinSize || size > kMaxSize)
        return false;

    // Build the whole lattice before touching any member, so a failure
    // (only bad_alloc here) cannot leave a half-built object.
    std::vector<double> data((size_t)size * (size_t)size * (size_t)size * 3, 0.0);
    const double last = (double)(size - 1);
    for (int b = 0; b < size; ++b)
        for (int g = 0; g < size; ++g)
            for (int r = 0; r < size; ++r)
            {
                size_t base = (((size_t)b * (size_t)size + (size_t)g) * (size_t)size
                               + (size_t)r) * 3;
                data[base + 0] = r / last;
                data[base + 1] = g / last;
                data[base + 2] = b / last;
            }

    m_data.swap(data);
    m_size = size;
    m_title.clear();
    m_domainMin[0] = m_domainMin[1] = m_domainMin[2] = 0.0;
    m_domainMax[0] = m_domainMax[1] = m_domainMax[2] = 1.0;
    return true;
}

bool CubeLUT::SetEntry(int r, int g, int b, const double rgb[3])
{
    if (m_size == 0 || rgb == 0)
        return false;
    if (r < 0 || r >= m_size || g < 0 || g >= m_size || b < 0 || b >= m_size)
        return false;
    for (int k = 0; k < 3; ++k)
        if (!IsFiniteValue(rgb[k]))
            return false;

    size_t base = (((size_t)b * (size_t)m_size + (size_t)g) * (size_t)m_size
                   + (size_t)r) * 3;
    for (int k = 0; k < 3; ++k)
        m_data[base + (size_t)k] = rgb[k];
    return true;
}

bool CubeLUT::GetEntry(int r, int g, int b, double rgb[3]) const
{
    if (m_size == 0 || rgb == 0)
        return false;
    if (r < 0 || r >= m_size || g < 0 || g >= m_size || b < 0 || b >= m_size)
        return false;

    size_t base = (((size_t)b * (size_t)m_size + (size_t)g) * (size_t)m_size
                   + (size_t)r) * 3;
    for (int k = 0; k < 3; ++k)
        rgb[k] = m_data[base + (size_t)k];
    return true;
}

bool CubeLUT::SetTitle(const std::string& title)
{
    if (title.find('"') != std::string::npos)
        return false;
    if (title.find('\r') != std::string::npos)
        return false;
    if (title.find('\n') != std::string::npos)
        return false;
    m_title = title;
    return true;
}

bool CubeLUT::SetDomain(const double dmin[3], const double dmax[3])
{
    if (dmin == 0 || dmax == 0)
        return false;
    for (int k = 0; k < 3; ++k)
    {
        if (!IsFiniteValue(dmin[k]) || !IsFiniteValue(dmax[k]))
            return false;
        if (!(dmax[k] > dmin[k]))
            return false;
    }
    for (int k = 0; k < 3; ++k)
    {
        m_domainMin[k] = dmin[k];
        m_domainMax[k] = dmax[k];
    }
    return true;
}

void CubeLUT::GetDomain(double dmin[3], double dmax[3]) const
{
    for (int k = 0; k < 3; ++k)
    {
        dmin[k] = m_domainMin[k];
        dmax[k] = m_domainMax[k];
    }
}

// Tetrahedral interpolation: each lattice cell is cut along its main
// diagonal into six tetrahedra, one per ordering of the three cell-local
// fractions, and the output is the barycentric blend along the path
// c000 -> +hi axis -> +hi+mid axes -> c111 with weights (1-f[hi],
// f[hi]-f[mid], f[mid]-f[lo], f[lo]). Technique analyzed in Kasson &
// Plouffe, "An Analysis of Selected Computer Interchange Color Spaces",
// ACM Trans. Graphics 11(4), 1992. See CubeLUT.h for the contract.
bool CubeLUT::Evaluate(const double in[3], double out[3]) const
{
    if (out == 0)
        return false;
    out[0] = out[1] = out[2] = 0.0;
    if (m_size == 0 || in == 0)
        return false;
    for (int k = 0; k < 3; ++k)
        if (!IsFiniteValue(in[k]))
            return false;

    // Map into the domain box (clamping out-of-domain inputs to the nearest
    // face), then split into a cell index and a cell-local fraction. The
    // last cell keeps f = 1 rather than starting a cell that does not exist,
    // so the top face of the lattice evaluates exactly at its nodes.
    // SetDomain guarantees max > min, so the division cannot divide by zero.
    const int lastCell = m_size - 2;
    int index[3];
    double frac[3];
    for (int k = 0; k < 3; ++k)
    {
        double t = (in[k] - m_domainMin[k]) / (m_domainMax[k] - m_domainMin[k]);
        if (t < 0.0)
            t = 0.0;
        else if (t > 1.0)
            t = 1.0;

        double s = t * (double)(m_size - 1);
        int i = (int)s;                     // s >= 0, so this truncates down
        if (i < 0)
            i = 0;
        else if (i > lastCell)
            i = lastCell;
        double f = s - (double)i;
        if (f < 0.0)
            f = 0.0;
        else if (f > 1.0)
            f = 1.0;

        index[k] = i;
        frac[k] = f;
    }

    // Order the axes by descending fraction. Ties may break either way: the
    // two paths differ only in a vertex whose weight is zero when the
    // fractions are equal, so the result is the same and the evaluator stays
    // continuous across the diagonal planes.
    int hi = 0, mid = 1, lo = 2;
    if (frac[mid] > frac[hi]) { int t = hi; hi = mid; mid = t; }
    if (frac[lo] > frac[mid]) { int t = mid; mid = lo; lo = t; }
    if (frac[mid] > frac[hi]) { int t = hi; hi = mid; mid = t; }

    // Storage steps of one lattice unit along red, green and blue, matching
    // the ((b*N + g)*N + r)*3 layout.
    const size_t n = (size_t)m_size;
    size_t step[3];
    step[0] = 3;
    step[1] = n * 3;
    step[2] = n * n * 3;

    size_t base = ((size_t)index[2] * n + (size_t)index[1]) * n * 3
                  + (size_t)index[0] * 3;
    size_t p1 = base + step[hi];
    size_t p2 = p1 + step[mid];
    size_t p3 = p2 + step[lo];

    const double w0 = 1.0 - frac[hi];
    const double w1 = frac[hi] - frac[mid];
    const double w2 = frac[mid] - frac[lo];
    const double w3 = frac[lo];

    for (int k = 0; k < 3; ++k)
    {
        size_t o = (size_t)k;
        out[k] = w0 * m_data[base + o]
               + w1 * m_data[p1 + o]
               + w2 * m_data[p2 + o]
               + w3 * m_data[p3 + o];
    }
    return true;
}

bool CubeLUT::WriteToString(std::string& out) const
{
    out.clear();
    if (m_size == 0)
    {
        m_lastError = "no lattice to write";
        return false;
    }

    std::string text;
    if (!m_title.empty())
    {
        text += "TITLE \"";
        text += m_title;
        text += "\"\n";
    }

    char sizeBuf[32];
    sizeBuf[0] = '\0';
    _snprintf_l(sizeBuf, sizeof(sizeBuf) - 1, "LUT_3D_SIZE %d\n", CNumericLocale(),
                m_size);
    sizeBuf[sizeof(sizeBuf) - 1] = '\0';
    text += sizeBuf;

    text += "DOMAIN_MIN";
    for (int k = 0; k < 3; ++k)
    {
        text += ' ';
        AppendDouble(text, m_domainMin[k]);
    }
    text += "\nDOMAIN_MAX";
    for (int k = 0; k < 3; ++k)
    {
        text += ' ';
        AppendDouble(text, m_domainMax[k]);
    }
    text += '\n';

    // Data lines in file order: red fastest, then green, then blue - which
    // is exactly the storage order, so the vector streams straight out.
    size_t count = m_data.size() / 3;
    for (size_t i = 0; i < count; ++i)
    {
        AppendDouble(text, m_data[i * 3 + 0]);
        text += ' ';
        AppendDouble(text, m_data[i * 3 + 1]);
        text += ' ';
        AppendDouble(text, m_data[i * 3 + 2]);
        text += '\n';
    }

    out.swap(text);
    m_lastError.clear();
    return true;
}

bool CubeLUT::ReadFromString(const std::string& text)
{
    // Everything is parsed into locals; the members are only touched once
    // the whole input has validated.
    int size = 0;
    bool haveSize = false;
    bool haveTitle = false;
    bool haveMin = false;
    bool haveMax = false;
    bool inData = false;
    std::string title;
    double dmin[3] = { 0.0, 0.0, 0.0 };
    double dmax[3] = { 1.0, 1.0, 1.0 };
    std::vector<double> data;
    size_t expected = 0;

    std::vector<std::string> lines;
    SplitLines(text, lines);

    std::vector<std::string> tokens;
    for (size_t li = 0; li < lines.size(); ++li)
    {
        const std::string& line = lines[li];
        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos)
            continue;                       // blank line
        if (line[first] == '#')
            continue;                       // comment line

        SplitTokens(line, tokens);
        if (tokens.empty())
            continue;
        const std::string& key = tokens[0];

        bool isKeyword = (key == "TITLE" || key == "DOMAIN_MIN" ||
                          key == "DOMAIN_MAX" || key == "LUT_3D_SIZE");
        if (!isKeyword)
        {
            // A token that converts completely to a number starts the data
            // section; anything else beginning with a letter is a keyword we
            // do not accept.
            double probe = 0.0;
            if (!ParseNumberToken(key, probe))
            {
                if (key == "LUT_1D_SIZE")
                    m_lastError = "LUT_1D_SIZE: 1D LUTs are not supported";
                else if (IsKeywordStart(key[0]))
                    m_lastError = "unknown keyword '" + key + "'";
                else
                    m_lastError = "malformed line '" + line + "'";
                return false;
            }
        }

        if (isKeyword)
        {
            if (inData)
            {
                m_lastError = "keyword '" + key + "' after the data section started";
                return false;
            }
            if (key == "LUT_3D_SIZE")
            {
                if (haveSize)
                {
                    m_lastError = "duplicate LUT_3D_SIZE";
                    return false;
                }
                int v = 0;
                if (tokens.size() != 2 || !ParseIntToken(tokens[1], v))
                {
                    m_lastError = "LUT_3D_SIZE needs one integer";
                    return false;
                }
                if (v < kMinSize || v > kMaxSize)
                {
                    m_lastError = "LUT_3D_SIZE out of range (2..256)";
                    return false;
                }
                size = v;
                haveSize = true;
                expected = (size_t)size * (size_t)size * (size_t)size;
                data.reserve(expected * 3);
            }
            else if (key == "TITLE")
            {
                if (haveTitle)
                {
                    m_lastError = "duplicate TITLE";
                    return false;
                }
                size_t p = line.find("TITLE");
                p += 5;
                while (p < line.size() && (line[p] == ' ' || line[p] == '\t'))
                    ++p;
                if (p >= line.size() || line[p] != '"')
                {
                    m_lastError = "TITLE must be a quoted string";
                    return false;
                }
                size_t close = line.find('"', p + 1);
                if (close == std::string::npos)
                {
                    m_lastError = "TITLE is missing its closing quote";
                    return false;
                }
                size_t tail = line.find_first_not_of(" \t", close + 1);
                if (tail != std::string::npos)
                {
                    m_lastError = "trailing text after the TITLE string";
                    return false;
                }
                title = line.substr(p + 1, close - p - 1);
                haveTitle = true;
            }
            else
            {
                bool isMin = (key == "DOMAIN_MIN");
                if ((isMin && haveMin) || (!isMin && haveMax))
                {
                    m_lastError = "duplicate " + key;
                    return false;
                }
                if (tokens.size() != 4)
                {
                    m_lastError = key + " needs three numbers";
                    return false;
                }
                double v[3];
                for (int k = 0; k < 3; ++k)
                    if (!ParseFiniteToken(tokens[(size_t)k + 1], v[k]))
                    {
                        m_lastError = key + " has a non-numeric or non-finite value";
                        return false;
                    }
                for (int k = 0; k < 3; ++k)
                {
                    if (isMin)
                        dmin[k] = v[k];
                    else
                        dmax[k] = v[k];
                }
                if (isMin)
                    haveMin = true;
                else
                    haveMax = true;
            }
            continue;
        }

        // Data line.
        if (!haveSize)
        {
            m_lastError = "data line before LUT_3D_SIZE";
            return false;
        }
        inData = true;
        if (tokens.size() != 3)
        {
            m_lastError = "data line needs exactly three numbers";
            return false;
        }
        if (data.size() / 3 >= expected)
        {
            m_lastError = "too many data lines";
            return false;
        }
        for (int k = 0; k < 3; ++k)
        {
            double v = 0.0;
            if (!ParseFiniteToken(tokens[(size_t)k], v))
            {
                m_lastError = "data line has a non-numeric or non-finite value";
                return false;
            }
            data.push_back(v);
        }
    }

    if (!haveSize)
    {
        m_lastError = "no LUT_3D_SIZE line";
        return false;
    }
    if (data.size() / 3 != expected)
    {
        m_lastError = "too few data lines";
        return false;
    }
    for (int k = 0; k < 3; ++k)
        if (!(dmax[k] > dmin[k]))
        {
            m_lastError = "DOMAIN_MAX must be greater than DOMAIN_MIN in every component";
            return false;
        }

    m_size = size;
    m_data.swap(data);
    m_title = title;
    for (int k = 0; k < 3; ++k)
    {
        m_domainMin[k] = dmin[k];
        m_domainMax[k] = dmax[k];
    }
    m_lastError.clear();
    return true;
}

bool CubeLUT::WriteFile(const char* path) const
{
    if (path == 0)
    {
        m_lastError = "no output path";
        return false;
    }
    std::string text;
    if (!WriteToString(text))
        return false;                       // WriteToString set m_lastError

    FILE* f = fopen(path, "wb");
    if (f == 0)
    {
        m_lastError = std::string("cannot open '") + path + "' for writing";
        return false;
    }
    size_t written = text.empty() ? 0 : fwrite(text.c_str(), 1, text.size(), f);
    bool ok = (written == text.size());
    if (fclose(f) != 0)
        ok = false;
    if (!ok)
    {
        m_lastError = std::string("cannot write '") + path + "'";
        return false;
    }
    m_lastError.clear();
    return true;
}

bool CubeLUT::ReadFile(const char* path)
{
    if (path == 0)
    {
        m_lastError = "no input path";
        return false;
    }
    FILE* f = fopen(path, "rb");
    if (f == 0)
    {
        m_lastError = std::string("cannot open '") + path + "' for reading";
        return false;
    }

    std::string text;
    char buf[4096];
    for (;;)
    {
        size_t got = fread(buf, 1, sizeof(buf), f);
        if (got > 0)
            text.append(buf, got);
        if (got < sizeof(buf))
            break;
    }
    bool ioError = (ferror(f) != 0);
    fclose(f);
    if (ioError)
    {
        m_lastError = std::string("cannot read '") + path + "'";
        return false;
    }
    return ReadFromString(text);
}
