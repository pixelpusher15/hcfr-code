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
#include <stdlib.h>
#include <fstream>
#include <sstream>

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
			if (rows.empty())
			{
				std::string up = line;
				for (size_t k = 0; k < up.size(); k++) up[k] = (char)toupper((unsigned char)up[k]);
				size_t bp = up.find("BITDEPTH");
				if (bp != std::string::npos)
				{
					int v = atoi(up.c_str() + bp + 8);
					if (v == 8 || v == 10) { bits = v; hasHeader = true; }
				}
				size_t rp = up.find("RANGE");
				if (rp != std::string::npos)
				{
					size_t p = rp + 5;			// first token after "RANGE"
					while (p < up.size() && (up[p] == ' ' || up[p] == '\t' || up[p] == ',' || up[p] == '=' || up[p] == ':')) p++;
					int r = -2;
					if      (up.compare(p, 8, "EXTENDED") == 0) r = CSV_RANGE_EXTENDED;
					else if (up.compare(p, 4, "FULL")     == 0) r = CSV_RANGE_FULL;
					else if (up.compare(p, 5, "LEGAL")    == 0) r = CSV_RANGE_LEGAL;
					if (r != -2) { range = r; hasHeader = true; }
				}
			}
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

// NOT YET IMPLEMENTED (review round): the shared vocabulary declared in
// CsvPatchRows.h. ReadCsvPatchRows above still carries its own copy of the
// directive grammar until ParseCsvFormatDirective is real and it calls it.
bool ParseCsvFormatDirective(const std::string&, int&, int&)
{
    return false;
}

const char* CsvRangeWord(int)
{
    return "";
}

std::string CsvFormatHeaderLine(int, int)
{
    return std::string();
}

int CsvCodeFromPercent(double, int, int)
{
    return 0;
}

bool CsvPercentRepresentable(double, int, int)
{
    return false;
}
