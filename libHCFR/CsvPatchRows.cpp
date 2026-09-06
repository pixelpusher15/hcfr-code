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

// The patch-list CSV parser, moved verbatim from Color.cpp so it can be
// used from MFC-free translation units. Behavior is unchanged; the CString
// overload in Color.cpp forwards here.

#include "CsvPatchRows.h"

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <sstream>

// The percent -> device-code quantizer of the live signal path
// (Color.cpp). Declared here instead of including Color.h, which drags MFC
// types into the translation unit; the signature is plain types, so any
// drift is a link error.
extern int PiPercentToCode(double percent, bool is16_235, int bits);

double CsvCodeToPercent(double code, int bits, int range)
{
	int maxCode = (1 << bits) - 1;
	if (range == CSV_RANGE_FULL)
		return code * 100.0 / maxCode;
	double black = 16.0  * (1 << (bits - 8));
	double white = 235.0 * (1 << (bits - 8));
	double pct = ( code - black ) / ( white - black ) * 100.0;
	if (range == CSV_RANGE_EXTENDED)
		return pct;
	return ( pct < 0.0 ) ? 0.0 : ( pct > 100.0 ? 100.0 : pct );
}

int ReadCsvPatchRows(const char* csvPath, std::vector<CsvPatchRow> & rows, int maxRows, int * bitsOut, int * rangeOut)
{
	rows.clear();
	if (bitsOut)  *bitsOut  = 8;
	if (rangeOut) *rangeOut = CSV_RANGE_NONE;
	std::ifstream colorFile(csvPath);
	if (!colorFile)
		return -1;

	std::string line;
	int bits = 8;
	int range = CSV_RANGE_NONE;
	bool hasHeader = false;

	while (std::getline(colorFile, line) && (int)rows.size() < maxRows)
	{
		while (!line.empty() && (line[line.size()-1] == '\r' || line[line.size()-1] == ' ' || line[line.size()-1] == '\t'))
			line.erase(line.size()-1);
		size_t b0 = line.find_first_not_of(" \t");
		if (b0 == std::string::npos)
			continue;

		if (line[b0] == '#')
		{
			// Only a leading '#' line (before any data) that carries a BITDEPTH or RANGE
			// directive declares the format - and that declaration is what marks the file
			// headered (schema = PatchNumber,R,G,B[,Name]). A plain comment must NOT flip a
			// legacy R,G,B[,Name] file's columns, and the range must come from the RANGE
			// token (not a bare "full"/"legal"/"extended" substring that "# full sweep" or
			// "# illegal codes" would trip). Later '#' lines are treated as comments.
			if (rows.empty() && ParseCsvFormatDirective(line, bits, range))
				hasHeader = true;
			continue;
		}

		std::string fld[5];
		int nf = 0;
		std::istringstream s(line);
		std::string field;
		while (nf < 5 && std::getline(s, field, ','))
			fld[nf++] = field;
		int base = (hasHeader && nf >= 4) ? 1 : 0;   // skip the leading PatchNumber column when headered
		if (nf < base + 3)
			continue;

		CsvPatchRow pr;
		pr.patch = base ? atoi(fld[0].c_str()) : (int)rows.size();
		pr.r = (int)atof(fld[base].c_str());
		pr.g = (int)atof(fld[base+1].c_str());
		pr.b = (int)atof(fld[base+2].c_str());
		if (nf > base + 3)
		{
			std::string nm = fld[base+3];
			size_t s0 = nm.find_first_not_of(" \t");
			size_t s1 = nm.find_last_not_of(" \t");
			pr.name = (s0 == std::string::npos) ? std::string() : nm.substr(s0, s1 - s0 + 1);
		}
		rows.push_back(pr);
	}
	if (bitsOut)  *bitsOut  = bits;
	if (rangeOut) *rangeOut = range;
	return (int)rows.size();
}

// The format-directive grammar, in ONE place: ReadCsvPatchRows above calls
// this, and so does the .hps reader. A value is only taken when it is one
// the dialect knows, so prose like "# full sweep of illegal codes" never
// flips a file's decoding - the words must follow a BITDEPTH / RANGE token.
bool ParseCsvFormatDirective(const std::string& line, int& bits, int& range)
{
    std::string up = line;
    for (size_t k = 0; k < up.size(); k++) up[k] = (char)toupper((unsigned char)up[k]);

    bool declared = false;

    size_t bp = up.find("BITDEPTH");
    if (bp != std::string::npos)
    {
        size_t p = bp + 8;                  // first token after "BITDEPTH"
        while (p < up.size() && (up[p] == ' ' || up[p] == '\t' || up[p] == ',' || up[p] == '=' || up[p] == ':')) p++;
        int v = atoi(up.c_str() + p);
        if (v == 8 || v == 10) { bits = v; declared = true; }
    }

    size_t rp = up.find("RANGE");
    if (rp != std::string::npos)
    {
        size_t p = rp + 5;                  // first token after "RANGE"
        while (p < up.size() && (up[p] == ' ' || up[p] == '\t' || up[p] == ',' || up[p] == '=' || up[p] == ':')) p++;
        int r = -2;
        if      (up.compare(p, 8, "EXTENDED") == 0) r = CSV_RANGE_EXTENDED;
        else if (up.compare(p, 4, "FULL")     == 0) r = CSV_RANGE_FULL;
        else if (up.compare(p, 5, "LEGAL")    == 0) r = CSV_RANGE_LEGAL;
        if (r != -2) { range = r; declared = true; }
    }

    return declared;
}

const char* CsvRangeWord(int range)
{
    if (range == CSV_RANGE_FULL)     return "FULL";
    if (range == CSV_RANGE_LEGAL)    return "LEGAL";
    if (range == CSV_RANGE_EXTENDED) return "EXTENDED";
    return "";
}

std::string CsvFormatHeaderLine(int bits, int range)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "# BITDEPTH %d, RANGE %s\n", bits, CsvRangeWord(range));
    return std::string(buf);
}

// The live signal path's quantizer is the inverse, so what a file says is
// what the generator drives.
int CsvCodeFromPercent(double pct, int bits, int range)
{
    return PiPercentToCode(pct, range != CSV_RANGE_FULL, bits);
}

bool CsvPercentRepresentable(double pct, int bits, int range)
{
    if (!_finite(pct))
        return false;
    // Only EXTENDED carries super-white / sub-black; anywhere else the
    // quantizer's clamp would silently rewrite the value.
    if (range != CSV_RANGE_EXTENDED && (pct < 0.0 || pct > 100.0))
        return false;

    double back = CsvCodeToPercent(CsvCodeFromPercent(pct, bits, range), bits, range);

    // One code step in percent, and the epsilon the round-trip oracle uses:
    // a value sitting exactly on a half step (37.5% at 10-bit legal) must
    // not be refused by a last-ulp difference.
    double span = (range == CSV_RANGE_FULL)
        ? (double)((1 << bits) - 1)
        : 219.0 * (double)(1 << (bits - 8));
    return fabs(back - pct) <= 50.0 / span + 1e-9;
}
