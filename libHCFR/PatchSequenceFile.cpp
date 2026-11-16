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

// PatchSequenceFile implementation - the behavioral contract (the .hps
// grammar, the CSV import/export rules) lives in PatchSequenceFile.h.
// Plain C++ and CRT only (no MFC), so the reader/writer stays usable from
// the math core and from console tools. The CSV vocabulary - the range
// spelling, the header line, the format-directive grammar, the code <->
// percent mapping - comes from CsvPatchRows.h; there is no copy here.

#include "PatchSequenceFile.h"

#include "CsvPatchRows.h"
#include "libHCFR_Config.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <vector>

namespace
{
    // The eleven fixed columns, in file order.
    enum
    {
        COL_ID = 0, COL_R, COL_G, COL_B, COL_NAME, COL_ROLE, COL_GROUP,
        COL_SETTLE, COL_HOLD, COL_INTEGRATION, COL_AVERAGE, COL_COUNT
    };

    const char* const kColumnNames[COL_COUNT] =
    {
        "PatchNumber", "R", "G", "B", "Name", "Role", "Group",
        "Settle", "Hold", "Integration", "Average"
    };

    const char kColumnsLine[] =
        "# COLUMNS PatchNumber,R,G,B,Name,Role,Group,Settle,Hold,Integration,Average\n";

    const char kSequenceTag[] = "# HCFR-SEQUENCE";
    const size_t kSequenceTagLen = sizeof(kSequenceTag) - 1;

    const char kUtf8Bom[] = "\xEF\xBB\xBF";
    const size_t kUtf8BomLen = 3;

    std::string Real(double v)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%g", v);
        return std::string(buf);
    }

    std::string Int(int v)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", v);
        return std::string(buf);
    }

    // A timing field is written only when set (-1 means "not set here").
    std::string RealField(double v)
    {
        return (v == -1.0) ? std::string() : Real(v);
    }

    std::string IntField(int v)
    {
        return (v == -1) ? std::string() : Int(v);
    }

    std::string Trim(const std::string& s)
    {
        size_t b = s.find_first_not_of(" \t");
        if (b == std::string::npos)
            return std::string();
        size_t e = s.find_last_not_of(" \t");
        return s.substr(b, e - b + 1);
    }

    // A directive value may be a double-quoted string; strip the quotes.
    std::string Unquote(const std::string& s)
    {
        if (s.size() >= 2 && s[0] == '"' && s[s.size() - 1] == '"')
            return s.substr(1, s.size() - 2);
        return s;
    }

    std::string Upper(const std::string& s)
    {
        std::string up = s;
        for (size_t i = 0; i < up.size(); ++i)
            if (up[i] >= 'a' && up[i] <= 'z')
                up[i] = (char)(up[i] - 'a' + 'A');
        return up;
    }

    // What a directive string value may NOT contain. A quote or a line end
    // would break the grammar outright; BITDEPTH or RANGE in any letter
    // case would flip the decoding of the CSV-only reader, which scans
    // every leading '#' line for exactly those words and cannot tell a
    // NAME of "Range extended near-white" from a real declaration.
    bool ValidDirectiveText(const std::string& v)
    {
        for (size_t i = 0; i < v.size(); ++i)
            if (v[i] == '"' || v[i] == '\r' || v[i] == '\n')
                return false;
        std::string up = Upper(v);
        if (up.find("BITDEPTH") != std::string::npos)
            return false;
        return up.find("RANGE") == std::string::npos;
    }

    void SplitLines(const std::string& text, std::vector<std::string>& out)
    {
        out.clear();
        size_t pos = 0;
        while (pos <= text.size())
        {
            size_t nl = text.find('\n', pos);
            std::string line = (nl == std::string::npos)
                ? text.substr(pos) : text.substr(pos, nl - pos);
            // CR LF is accepted on read; trailing blanks are not data.
            while (!line.empty())
            {
                char c = line[line.size() - 1];
                if (c != '\r' && c != ' ' && c != '\t')
                    break;
                line.erase(line.size() - 1);
            }
            out.push_back(line);
            if (nl == std::string::npos)
                break;
            pos = nl + 1;
        }
    }

    void SplitFields(const std::string& line, char sep,
                     std::vector<std::string>& out)
    {
        out.clear();
        size_t pos = 0;
        for (;;)
        {
            size_t s = line.find(sep, pos);
            if (s == std::string::npos)
            {
                out.push_back(line.substr(pos));
                return;
            }
            out.push_back(line.substr(pos, s - pos));
            pos = s + 1;
        }
    }

    void SplitTokens(const std::string& s, std::vector<std::string>& out)
    {
        out.clear();
        size_t i = 0;
        while (i < s.size())
        {
            while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
                ++i;
            size_t b = i;
            while (i < s.size() && s[i] != ' ' && s[i] != '\t')
                ++i;
            if (i > b)
                out.push_back(s.substr(b, i - b));
        }
    }

    bool TokenText(const std::vector<std::string>& t, const char* key,
                   std::string& v)
    {
        size_t kl = strlen(key);
        for (size_t i = 0; i < t.size(); ++i)
            if (t[i].size() >= kl && t[i].compare(0, kl, key) == 0)
            {
                v = t[i].substr(kl);
                return true;
            }
        return false;
    }

    bool TokenReal(const std::vector<std::string>& t, const char* key,
                   double& v)
    {
        std::string text;
        if (!TokenText(t, key, text))
            return false;
        v = strtod(text.c_str(), 0);
        return true;
    }

    bool TokenInt(const std::vector<std::string>& t, const char* key, int& v)
    {
        std::string text;
        if (!TokenText(t, key, text))
            return false;
        v = atoi(text.c_str());
        return true;
    }

    // Codes are read as numbers and truncated to int, exactly as
    // ReadCsvPatchRows does, so both dialects decode a row the same way.
    double CodeFieldToPercent(const std::string& field, int bits, int range)
    {
        return CsvCodeToPercent((double)(int)atof(field.c_str()), bits, range);
    }

    std::string RowReason(const char* what, int dataRow)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), " row %d", dataRow);
        return std::string(what) + buf;
    }
}

PatchSequenceHeader::PatchSequenceHeader()
    : version(1), bits(8), range(1), identitiesFromRows(false),
      geometryArea(-1), geometryBackground(-1),
      targetLoad(-1.0), window(-1.0), dwellLatency(-1.0), maxStep(-1.0),
      settleAfterStep(-1.0), ablLimit(-1.0),
      prerollId(-1), prerollCount(-1)
{
}

// An identity this header names but the renumbering does not mention is a
// stale reference: it becomes -1 rather than silently pointing at whatever
// patch inherited its old number.
void PatchSequenceHeader::RemapIdentities(
    const std::vector< std::pair<int, int> >& renumbering)
{
    std::map<int, int> map;
    for (size_t i = 0; i < renumbering.size(); ++i)
        map[renumbering[i].first] = renumbering[i].second;

    for (size_t i = 0; i < anchors.size(); ++i)
    {
        if (anchors[i].id == -1)
            continue;
        std::map<int, int>::const_iterator it = map.find(anchors[i].id);
        anchors[i].id = (it == map.end()) ? -1 : it->second;
    }
    if (prerollId != -1)
    {
        std::map<int, int>::const_iterator it = map.find(prerollId);
        prerollId = (it == map.end()) ? -1 : it->second;
    }
}

bool PatchSequenceFile::FormatHps(std::string& out, const PatchSequence& seq,
                                  const PatchSequenceHeader& hdr)
{
    // It writes only what it can read back.
    if (hdr.version != 1)
    {
        m_lastError = "unsupported sequence version";
        return false;
    }
    if (seq.Count() <= 0)
    {
        m_lastError = "empty patch sequence";
        return false;
    }
    if (seq.Count() > MAX_USER_CC_PATCH_SIZE)
    {
        m_lastError = "too many patch rows";
        return false;
    }
    if (hdr.bits != 8 && hdr.bits != 10)
    {
        m_lastError = "unsupported bit depth";
        return false;
    }
    if (CsvRangeWord(hdr.range)[0] == '\0')
    {
        m_lastError = "unsupported csv range";
        return false;
    }

    // No silent clamp, ever - the same rule PatchSequence::WriteCsv applies.
    for (int i = 0; i < seq.Count(); ++i)
    {
        double rgb[3];
        seq.GetPatch(i, rgb, 0);
        for (int k = 0; k < 3; ++k)
            if (!CsvPercentRepresentable(rgb[k], hdr.bits, hdr.range))
            {
                m_lastError = "component not representable at this encoding";
                return false;
            }
    }

    // Every directive value must survive a round trip through both readers.
    std::vector<std::string> values;
    values.push_back(hdr.sequenceId);
    values.push_back(hdr.name);
    values.push_back(hdr.created);
    values.push_back(hdr.generator);
    values.push_back(hdr.sourcePath);
    values.push_back(hdr.sourceHash);
    values.push_back(hdr.displayProfile);
    values.push_back(hdr.panelRecord);
    values.push_back(hdr.target);
    values.push_back(hdr.gamutRelation);
    values.push_back(hdr.readAt);
    values.push_back(hdr.order);
    values.push_back(hdr.loadModel);
    for (size_t i = 0; i < hdr.required.size(); ++i)
        values.push_back(hdr.required[i]);
    for (size_t i = 0; i < hdr.steps.size(); ++i)
        values.push_back(hdr.steps[i]);
    for (size_t i = 0; i < hdr.groups.size(); ++i)
        values.push_back(hdr.groups[i].readAt);
    for (size_t i = 0; i < values.size(); ++i)
        if (!ValidDirectiveText(values[i]))
        {
            m_lastError = "directive value would not survive a re-read";
            return false;
        }
    for (size_t i = 0; i < hdr.groups.size(); ++i)
        if (!PatchSequence::ValidGroupName(hdr.groups[i].name))
        {
            m_lastError = "bad group name";
            return false;
        }

    std::string t;
    t.reserve((size_t)seq.Count() * 48 + 2048);
    char buf[256];

    snprintf(buf, sizeof(buf), "# HCFR-SEQUENCE %d\n", hdr.version);
    t += buf;
    t += CsvFormatHeaderLine(hdr.bits, hdr.range);

    if (!hdr.sequenceId.empty())
        t += "# SEQUENCE-ID " + hdr.sequenceId + "\n";
    if (!hdr.name.empty())
        t += "# NAME \"" + hdr.name + "\"\n";
    if (!hdr.created.empty())
        t += "# CREATED " + hdr.created + "\n";
    if (!hdr.generator.empty())
        t += "# GENERATOR \"" + hdr.generator + "\"\n";
    if (!hdr.sourceKind.empty())
    {
        if (hdr.sourceKind == "csv")
        {
            t += "# SOURCE csv \"" + hdr.sourcePath + "\"";
            if (!hdr.sourceHash.empty())
                t += " sha256 " + hdr.sourceHash;
            t += "\n";
        }
        else
            t += "# SOURCE " + hdr.sourceKind + "\n";
    }
    if (!hdr.required.empty())
    {
        t += "# REQUIRES";
        for (size_t i = 0; i < hdr.required.size(); ++i)
            t += " " + hdr.required[i];
        t += "\n";
    }
    if (!hdr.displayProfile.empty())
        t += "# DISPLAY-PROFILE " + hdr.displayProfile + "\n";
    if (!hdr.panelRecord.empty())
        t += "# PANEL-RECORD " + hdr.panelRecord + "\n";
    if (!hdr.target.empty())
        t += "# TARGET " + hdr.target + "\n";
    if (!hdr.gamutRelation.empty())
        t += "# GAMUT-RELATION " + hdr.gamutRelation + "\n";
    for (size_t i = 0; i < hdr.steps.size(); ++i)
        t += "# STEP " + hdr.steps[i] + "\n";
    if (hdr.geometryArea >= 0 || hdr.geometryBackground >= 0)
    {
        t += "# PATCH-GEOMETRY";
        if (hdr.geometryArea >= 0)
            t += " area=" + Int(hdr.geometryArea);
        if (hdr.geometryBackground >= 0)
            t += " background=" + Int(hdr.geometryBackground);
        t += "\n";
    }
    for (size_t i = 0; i < hdr.groups.size(); ++i)
    {
        const PatchGroupSpec& g = hdr.groups[i];
        t += "# GROUP " + g.name;
        if (g.timing.integration != -1.0)
            t += " integration=" + Real(g.timing.integration);
        if (g.timing.average != -1)
            t += " average=" + Int(g.timing.average);
        if (g.timing.settle != -1.0)
            t += " settle=" + Real(g.timing.settle);
        if (g.timing.hold != -1.0)
            t += " hold=" + Real(g.timing.hold);
        if (!g.readAt.empty())
            t += " readat=" + g.readAt;
        if (g.trace != -1.0)
            t += " trace=" + Real(g.trace);
        if (g.untilStable)
            t += " until=stable";
        if (g.deadband != -1.0)
            t += " deadband=" + Real(g.deadband);
        t += "\n";
    }
    if (!hdr.readAt.empty())
        t += "# READ-AT " + hdr.readAt + "\n";
    if (!hdr.order.empty())
        t += "# ORDER " + hdr.order + "\n";
    if (hdr.targetLoad != -1.0)
        t += "# TARGET-LOAD " + Real(hdr.targetLoad) + "\n";
    if (!hdr.loadModel.empty())
        t += "# LOAD-MODEL " + hdr.loadModel + "\n";
    if (hdr.window != -1.0)
        t += "# WINDOW " + Real(hdr.window) + "\n";
    if (hdr.dwellLatency != -1.0)
        t += "# DWELL latency=" + Real(hdr.dwellLatency) + "\n";
    if (hdr.maxStep != -1.0)
        t += "# MAX-STEP " + Real(hdr.maxStep) + "\n";
    if (hdr.settleAfterStep != -1.0)
        t += "# SETTLE-AFTER-STEP " + Real(hdr.settleAfterStep) + "\n";
    if (hdr.ablLimit != -1.0)
        t += "# ABL-LIMIT apl=" + Real(hdr.ablLimit) + "\n";
    for (size_t i = 0; i < hdr.anchors.size(); ++i)
    {
        t += "# ANCHOR " + Int(hdr.anchors[i].id);
        if (hdr.anchors[i].every >= 0)
            t += " every=" + Int(hdr.anchors[i].every);
        t += "\n";
    }
    if (hdr.prerollId >= 0)
    {
        t += "# PREROLL " + Int(hdr.prerollId);
        if (hdr.prerollCount >= 0)
            t += " count=" + Int(hdr.prerollCount);
        t += "\n";
    }
    t += kColumnsLine;

    for (int i = 0; i < seq.Count(); ++i)
    {
        double rgb[3];
        std::string name, group;
        PatchTiming tm;
        seq.GetPatch(i, rgb, &name);
        seq.GetGroup(i, group);
        seq.GetTiming(i, tm);
        snprintf(buf, sizeof(buf), "%d,%d,%d,%d,", seq.GetIdentity(i),
                 CsvCodeFromPercent(rgb[0], hdr.bits, hdr.range),
                 CsvCodeFromPercent(rgb[1], hdr.bits, hdr.range),
                 CsvCodeFromPercent(rgb[2], hdr.bits, hdr.range));
        t += buf;
        t += name;
        t += ',';
        t += seq.GetRole(i);
        t += ',';
        t += group;
        t += ',';
        t += RealField(tm.settle);
        t += ',';
        t += RealField(tm.hold);
        t += ',';
        t += RealField(tm.integration);
        t += ',';
        t += IntField(tm.average);
        t += '\n';
    }

    out.swap(t);
    m_lastError.clear();
    return true;
}

bool PatchSequenceFile::ParseHps(const std::string& text, PatchSequence& seq,
                                 PatchSequenceHeader& hdr)
{
    std::vector<std::string> lines;
    SplitLines(text, lines);

    // The tag line. A UTF-8 byte-order mark, blank lines and leading
    // spaces or tabs before it are all tolerated - editors add all three.
    size_t li = 0;
    std::string tagLine;
    bool haveTag = false;
    for (; li < lines.size(); ++li)
    {
        std::string line = lines[li];
        if (li == 0 && line.compare(0, kUtf8BomLen, kUtf8Bom) == 0)
            line.erase(0, kUtf8BomLen);
        size_t b = line.find_first_not_of(" \t");
        if (b == std::string::npos)
            continue;                       // a blank line before the tag
        tagLine = line.substr(b);
        haveTag = true;
        break;
    }
    if (!haveTag)
    {
        m_lastError = "empty sequence file";
        return false;
    }
    if (tagLine.size() < kSequenceTagLen
        || tagLine.compare(0, kSequenceTagLen, kSequenceTag) != 0)
    {
        m_lastError = "not a patch sequence file";
        return false;
    }

    int version = 0;
    {
        const char* p = tagLine.c_str() + kSequenceTagLen;
        while (*p == ' ' || *p == '\t')
            ++p;
        if (*p < '0' || *p > '9')
        {
            m_lastError = "missing sequence version";
            return false;
        }
        version = atoi(p);
        if (version > 1)
        {
            m_lastError = "unsupported sequence version";
            return false;
        }
    }
    ++li;                                   // continue after the tag line

    // The row cap is checked before any row is built, so an oversized file
    // costs one cheap pass instead of a full parse.
    size_t dataLines = 0;
    for (size_t i = li; i < lines.size(); ++i)
    {
        size_t b = lines[i].find_first_not_of(" \t");
        if (b != std::string::npos && lines[i][b] != '#')
            ++dataLines;
    }
    if (dataLines > (size_t)MAX_USER_CC_PATCH_SIZE)
    {
        m_lastError = "too many patch rows";
        return false;
    }

    // Everything is built into locals and swapped in at the end, so a
    // refusal leaves the caller's sequence and header untouched.
    PatchSequenceHeader h;
    h.version = version;
    PatchSequence s;
    std::vector<int> cols;
    bool inData = false;
    int dataRow = 0;                        // 1-based, named on a refusal

    for (; li < lines.size(); ++li)
    {
        const std::string& line = lines[li];
        size_t b = line.find_first_not_of(" \t");
        if (b == std::string::npos)
            continue;                       // blank

        if (line[b] == '#')
        {
            if (inData)
                continue;                   // a comment, not a directive

            std::string body = Trim(line.substr(b + 1));
            size_t sp = body.find_first_of(" \t");
            std::string key = (sp == std::string::npos) ? body : body.substr(0, sp);
            std::string value = (sp == std::string::npos)
                ? std::string() : Trim(body.substr(sp + 1));
            std::vector<std::string> tok;
            SplitTokens(value, tok);

            if (key == "BITDEPTH" || key == "RANGE")
                ParseCsvFormatDirective(body, h.bits, h.range);
            else if (key == "SEQUENCE-ID")
                h.sequenceId = Unquote(value);
            else if (key == "NAME")
                h.name = Unquote(value);
            else if (key == "CREATED")
                h.created = Unquote(value);
            else if (key == "GENERATOR")
                h.generator = Unquote(value);
            else if (key == "SOURCE")
            {
                if (!tok.empty() && tok[0] == "csv")
                {
                    h.sourceKind = "csv";
                    size_t q0 = value.find('"');
                    size_t q1 = (q0 == std::string::npos)
                        ? std::string::npos : value.find('"', q0 + 1);
                    if (q1 != std::string::npos)
                        h.sourcePath = value.substr(q0 + 1, q1 - q0 - 1);
                    // "sha256 <hex>" - the digest is the token after it.
                    for (size_t i = 0; i + 1 < tok.size(); ++i)
                        if (tok[i] == "sha256")
                            h.sourceHash = tok[i + 1];
                }
                else if (!tok.empty())
                    h.sourceKind = tok[0];
            }
            else if (key == "REQUIRES")
                h.required = tok;
            else if (key == "DISPLAY-PROFILE")
                h.displayProfile = Unquote(value);
            else if (key == "PANEL-RECORD")
                h.panelRecord = Unquote(value);
            else if (key == "TARGET")
                h.target = Unquote(value);
            else if (key == "GAMUT-RELATION")
                h.gamutRelation = Unquote(value);
            else if (key == "STEP")
                h.steps.push_back(value);
            else if (key == "PATCH-GEOMETRY")
            {
                TokenInt(tok, "area=", h.geometryArea);
                TokenInt(tok, "background=", h.geometryBackground);
            }
            else if (key == "GROUP")
            {
                if (!tok.empty())
                {
                    PatchGroupSpec g;
                    g.name = tok[0];
                    TokenReal(tok, "integration=", g.timing.integration);
                    TokenInt(tok, "average=", g.timing.average);
                    TokenReal(tok, "settle=", g.timing.settle);
                    TokenReal(tok, "hold=", g.timing.hold);
                    TokenText(tok, "readat=", g.readAt);
                    TokenReal(tok, "trace=", g.trace);
                    std::string until;
                    if (TokenText(tok, "until=", until))
                        g.untilStable = (until == "stable");
                    TokenReal(tok, "deadband=", g.deadband);
                    h.groups.push_back(g);
                }
            }
            else if (key == "READ-AT")
                h.readAt = value;
            else if (key == "ORDER")
                h.order = value;
            else if (key == "TARGET-LOAD")
                h.targetLoad = strtod(value.c_str(), 0);
            else if (key == "LOAD-MODEL")
                h.loadModel = value;
            else if (key == "WINDOW")
                h.window = strtod(value.c_str(), 0);
            else if (key == "DWELL")
                TokenReal(tok, "latency=", h.dwellLatency);
            else if (key == "MAX-STEP")
                h.maxStep = strtod(value.c_str(), 0);
            else if (key == "SETTLE-AFTER-STEP")
                h.settleAfterStep = strtod(value.c_str(), 0);
            else if (key == "ABL-LIMIT")
                TokenReal(tok, "apl=", h.ablLimit);
            else if (key == "ANCHOR")
            {
                if (!tok.empty())
                {
                    PatchAnchorSpec a;
                    a.id = atoi(tok[0].c_str());
                    TokenInt(tok, "every=", a.every);
                    h.anchors.push_back(a);
                }
            }
            else if (key == "PREROLL")
            {
                if (!tok.empty())
                {
                    h.prerollId = atoi(tok[0].c_str());
                    TokenInt(tok, "count=", h.prerollCount);
                }
            }
            else if (key == "COLUMNS")
            {
                std::vector<std::string> names;
                SplitFields(value, ',', names);
                cols.clear();
                for (size_t i = 0; i < names.size(); ++i)
                {
                    std::string n = Trim(names[i]);
                    int which = -1;         // an unknown column is ignored
                    for (int c = 0; c < COL_COUNT; ++c)
                        if (n == kColumnNames[c])
                            which = c;
                    cols.push_back(which);
                }
            }
            // Any other key is unknown: ignored by design.
            continue;
        }

        if (!inData)
        {
            inData = true;
            if (cols.empty())
                for (int c = 0; c < COL_COUNT; ++c)
                    cols.push_back(c);      // the eleven fixed columns
            if (CsvRangeWord(h.range)[0] == '\0' || (h.bits != 8 && h.bits != 10))
            {
                m_lastError = "unsupported bit depth or csv range";
                return false;
            }
            // An EXTENDED file must widen the span BEFORE any row is added,
            // so no code is clamped on the way in.
            if (h.range == CSV_RANGE_EXTENDED && !s.SetExtended(true))
            {
                m_lastError = "cannot switch the sequence to extended";
                return false;
            }
        }
        ++dataRow;

        std::vector<std::string> f;
        SplitFields(line, ',', f);
        std::string fld[COL_COUNT];
        bool have[COL_COUNT];
        for (int c = 0; c < COL_COUNT; ++c)
            have[c] = false;
        for (size_t c = 0; c < f.size() && c < cols.size(); ++c)
        {
            int which = cols[c];
            if (which < 0)
                continue;                   // an unknown column is ignored
            fld[which] = f[c];
            have[which] = true;
        }
        // Fields past the declared columns are ignored by design.
        if (!have[COL_ID] || !have[COL_R] || !have[COL_G] || !have[COL_B])
        {
            m_lastError = RowReason("incomplete patch", dataRow);
            return false;
        }

        int id = atoi(Trim(fld[COL_ID]).c_str());
        double pr = CodeFieldToPercent(fld[COL_R], h.bits, h.range);
        double pg = CodeFieldToPercent(fld[COL_G], h.bits, h.range);
        double pb = CodeFieldToPercent(fld[COL_B], h.bits, h.range);
        std::string name = Trim(fld[COL_NAME]);
        std::string group = Trim(fld[COL_GROUP]);
        std::string roleText = Trim(fld[COL_ROLE]);
        char role = roleText.empty() ? 'C' : roleText[0];

        if (!s.AddRow(id, pr, pg, pb, name, role))
        {
            m_lastError = RowReason("bad patch", dataRow);
            return false;
        }
        int index = s.Count() - 1;
        if (!group.empty() && !s.SetGroup(index, group))
        {
            m_lastError = RowReason("bad group name", dataRow);
            return false;
        }
        PatchTiming tm;
        std::string v = Trim(fld[COL_SETTLE]);
        if (!v.empty()) tm.settle = strtod(v.c_str(), 0);
        v = Trim(fld[COL_HOLD]);
        if (!v.empty()) tm.hold = strtod(v.c_str(), 0);
        v = Trim(fld[COL_INTEGRATION]);
        if (!v.empty()) tm.integration = strtod(v.c_str(), 0);
        v = Trim(fld[COL_AVERAGE]);
        if (!v.empty()) tm.average = atoi(v.c_str());
        if (!s.SetTiming(index, tm))
        {
            m_lastError = RowReason("bad patch timing", dataRow);
            return false;
        }
    }

    if (s.Count() <= 0)
    {
        m_lastError = "no patch rows";
        return false;
    }

    seq.Swap(s);
    hdr = h;
    m_lastError.clear();
    return true;
}

bool PatchSequenceFile::Read(const char* path, PatchSequence& seq,
                             PatchSequenceHeader& hdr, Dialect* dialectOut)
{
    if (path == 0 || path[0] == '\0')
    {
        m_lastError = "no input path";
        return false;
    }
    FILE* file = fopen(path, "rb");
    if (!file)
    {
        m_lastError = "cannot open input file";
        return false;
    }
    std::string text;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), file)) > 0)
        text.append(buf, n);
    fclose(file);

    // Sniff the dialect in place: the tag may sit behind a byte-order mark,
    // blank lines and leading whitespace.
    size_t at = 0;
    if (text.compare(0, kUtf8BomLen, kUtf8Bom) == 0)
        at = kUtf8BomLen;
    while (at < text.size() && (text[at] == ' ' || text[at] == '\t'
                                || text[at] == '\r' || text[at] == '\n'))
        ++at;
    bool isHps = (at + kSequenceTagLen <= text.size())
        && text.compare(at, kSequenceTagLen, kSequenceTag) == 0;

    if (isHps)
    {
        if (!ParseHps(text, seq, hdr))
            return false;                   // ParseHps set m_lastError
        if (dialectOut)
            *dialectOut = DIALECT_HPS;
        return true;
    }

    // One row past the cap, so an oversized file is detected rather than
    // silently truncated to exactly the cap.
    std::vector<CsvPatchRow> rows;
    int bits = 8;
    int range = CSV_RANGE_NONE;
    int count = ReadCsvPatchRows(path, rows, MAX_USER_CC_PATCH_SIZE + 1,
                                 &bits, &range);
    if (count < 0)
    {
        m_lastError = "cannot open input file";
        return false;
    }
    if (count == 0)
    {
        m_lastError = "no patch rows";
        return false;
    }
    if (count > MAX_USER_CC_PATCH_SIZE)
    {
        m_lastError = "too many patch rows";
        return false;
    }

    // No declared range means the legacy dialect: 8-bit LEGAL, identities
    // are row indices (ReadCsvPatchRows already numbers them), all roles C.
    bool legacy = (range == CSV_RANGE_NONE);
    int useBits = legacy ? 8 : bits;
    int useRange = legacy ? CSV_RANGE_LEGAL : range;

    // Can the PatchNumber column serve as identity? It can unless two rows
    // share a number with different R,G,B - which is what a hand-edited
    // file, or a non-numeric column that collapses to 0, looks like. When
    // it cannot, every row falls back to its index and the file still
    // loads, exactly as it does in the app's own importer.
    bool usable = true;
    {
        std::map<int, size_t> firstAt;
        for (size_t k = 0; k < rows.size(); ++k)
        {
            int id = rows[k].patch;
            if (id < 0 || id > PatchSequence::kMaxIdentity)
            {
                usable = false;
                break;
            }
            std::map<int, size_t>::const_iterator it = firstAt.find(id);
            if (it == firstAt.end())
                firstAt[id] = k;
            else if (rows[it->second].r != rows[k].r
                     || rows[it->second].g != rows[k].g
                     || rows[it->second].b != rows[k].b)
            {
                usable = false;
                break;
            }
        }
    }

    PatchSequence s;
    if (useRange == CSV_RANGE_EXTENDED && !s.SetExtended(true))
    {
        m_lastError = "cannot switch the sequence to extended";
        return false;
    }
    for (size_t k = 0; k < rows.size(); ++k)
    {
        double pr = CsvCodeToPercent(rows[k].r, useBits, useRange);
        double pg = CsvCodeToPercent(rows[k].g, useBits, useRange);
        double pb = CsvCodeToPercent(rows[k].b, useBits, useRange);
        int id = usable ? rows[k].patch : (int)k;
        if (!s.AddRow(id, pr, pg, pb, rows[k].name, 'C'))
        {
            m_lastError = RowReason("bad patch", (int)k + 1);
            return false;
        }
    }

    PatchSequenceHeader h;
    h.bits = useBits;
    h.range = useRange;
    h.sourceKind = "csv";
    h.sourcePath = path;
    h.identitiesFromRows = !usable;

    seq.Swap(s);
    hdr = h;
    if (dialectOut)
        *dialectOut = legacy ? DIALECT_CSV_LEGACY : DIALECT_CSV_HEADERED;
    m_lastError.clear();
    return true;
}

bool PatchSequenceFile::Write(const char* path, const PatchSequence& seq,
                              const PatchSequenceHeader& hdr)
{
    std::string text;
    if (!FormatHps(text, seq, hdr))
        return false;                       // FormatHps set m_lastError
    std::string reason;
    if (!WriteTextFileAtomic(path, text, reason))
    {
        m_lastError = reason;
        return false;
    }
    m_lastError.clear();
    return true;
}

bool PatchSequenceFile::ExportCsv(std::string& out, const PatchSequence& seq,
                                  int bits, int range, ExportOrder order,
                                  bool includeRoles)
{
    // What a third-party tool may see is the role predicate's business.
    std::vector<int> pick;
    for (int i = 0; i < seq.Count(); ++i)
    {
        if (!includeRoles && !PatchSequence::RoleIsExportable(seq.GetRole(i)))
            continue;
        pick.push_back(i);
    }

    if (order == EXPORT_DEFINITION)
    {
        std::map<int, int> firstAt;         // identity -> first row, sorted
        for (size_t k = 0; k < pick.size(); ++k)
        {
            int id = seq.GetIdentity(pick[k]);
            if (firstAt.find(id) == firstAt.end())
                firstAt[id] = pick[k];
        }
        pick.clear();
        for (std::map<int, int>::const_iterator it = firstAt.begin();
             it != firstAt.end(); ++it)
            pick.push_back(it->second);
    }

    if (pick.empty())
    {
        m_lastError = "no exportable patch rows";
        return false;
    }

    // The filtered/ordered rows go out through PatchSequence::WriteCsv, so
    // the export and the plain CSV writer can never drift apart - the
    // representability refusal included.
    PatchSequence tmp;
    if (seq.IsExtended() && !tmp.SetExtended(true))
    {
        m_lastError = "cannot switch the sequence to extended";
        return false;
    }
    for (size_t k = 0; k < pick.size(); ++k)
    {
        double rgb[3];
        std::string name;
        seq.GetPatch(pick[k], rgb, &name);
        if (!tmp.AddRow(seq.GetIdentity(pick[k]), rgb[0], rgb[1], rgb[2],
                        name, seq.GetRole(pick[k])))
        {
            m_lastError = RowReason("bad patch", (int)k + 1);
            return false;
        }
    }

    std::string text;
    if (!tmp.WriteCsv(text, bits, range))
    {
        m_lastError = tmp.LastError();
        return false;
    }
    out.swap(text);
    m_lastError.clear();
    return true;
}

bool PatchSequenceFile::ExportCsvFile(const char* path, const PatchSequence& seq,
                                      int bits, int range, ExportOrder order,
                                      bool includeRoles)
{
    std::string text;
    if (!ExportCsv(text, seq, bits, range, order, includeRoles))
        return false;                       // ExportCsv set m_lastError
    std::string reason;
    if (!WriteTextFileAtomic(path, text, reason))
    {
        m_lastError = reason;
        return false;
    }
    m_lastError.clear();
    return true;
}
