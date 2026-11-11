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

// Ti3Export: CGATS CTI3 writer for display readings. See Ti3Export.h.
//
// Keyword and field names follow the CTI3 conventions ArgyllCMS documents for
// display measurement files (DEVICE_CLASS, COLOR_REP, LUMINANCE_XYZ_CDM2,
// NORMALIZED_TO_Y_100, SPECTRAL_BANDS/START_NM/END_NM, SPEC_nnn fields). The
// file itself is produced by the in-tree CGATS library through its public API.

#include "Ti3Export.h"
#include "../Tools/spectro/cgats.h"

#include <cmath>
#include <cstdio>

static bool IsWholeNm(double nm)
{
	return std::fabs(nm - std::floor(nm + 0.5)) < 1e-6;
}

bool Ti3HasSpectralColumns(const Ti3Table & t)
{
	if (t.specBands < 2 || !(t.specEndNm > t.specStartNm))
		return false;
	double step = (t.specEndNm - t.specStartNm) / (t.specBands - 1);
	for (int i = 0; i < t.specBands; i++)
		if (!IsWholeNm(t.specStartNm + i * step))
			return false;
	if (t.readings.empty())
		return false;
	for (size_t r = 0; r < t.readings.size(); r++)
		if ((int)t.readings[r].spectrum.size() != t.specBands)
			return false;
	return true;
}

std::string Ti3SpectralFieldName(const Ti3Table & t, int band)
{
	double step = (t.specBands > 1) ? (t.specEndNm - t.specStartNm) / (t.specBands - 1) : 0.0;
	char buf[32];
	sprintf(buf, "SPEC_%03d", (int)std::floor(t.specStartNm + band * step + 0.5));
	return buf;
}

std::string Ti3LuminanceKeyword(const double whiteXYZ[3])
{
	char buf[128];
	sprintf(buf, "%.6f %.6f %.6f", whiteXYZ[0], whiteXYZ[1], whiteXYZ[2]);
	return buf;
}

// A failed CGATS call leaves its message in p->e.m; carry it out and free p.
static bool CgatsFail(cgats * p, const char * what, std::string & err)
{
	err = what;
	if (p != NULL && p->e.m[0] != '\0')
	{
		err += ": ";
		err += p->e.m;
	}
	if (p != NULL)
		p->del(p);
	return false;
}

bool WriteTi3File(const char * path, const Ti3Table & t, std::string & err)
{
	err.clear();
	if (path == NULL || path[0] == '\0')
	{
		err = "no file name";
		return false;
	}
	if (!(t.whiteXYZ[1] > 0.0))
	{
		err = "the normalizing white has no luminance";
		return false;
	}
	if (t.readings.empty())
	{
		err = "no readings";
		return false;
	}

	// NORMALIZED_TO_Y_100: every XYZ (and spectral value) is multiplied by
	// 100 / Y_white, so the white the caller chose reads Y = 100 and the
	// absolute scale survives in LUMINANCE_XYZ_CDM2.
	const double scale = 100.0 / t.whiteXYZ[1];
	const bool spectral = Ti3HasSpectralColumns(t);

	cgats * p = new_cgats();
	if (p == NULL)
	{
		err = "cannot create the CGATS object";
		return false;
	}
	if (p->add_other(p, "CTI3") < 0 || p->add_table(p, tt_other, 0) < 0)
		return CgatsFail(p, "cannot create the CTI3 table", err);

	char buf[64];
	std::string lum = Ti3LuminanceKeyword(t.whiteXYZ);
	if (p->add_kword(p, 0, "DESCRIPTOR", t.descriptor.empty() ? "Display readings" : t.descriptor.c_str(), NULL) < 0
	 || p->add_kword(p, 0, "ORIGINATOR", t.originator.empty() ? "ColorHCFR" : t.originator.c_str(), NULL) < 0
	 || p->add_kword(p, 0, "DEVICE_CLASS", "DISPLAY", NULL) < 0
	 || p->add_kword(p, 0, "COLOR_REP", "RGB_XYZ", NULL) < 0
	 || p->add_kword(p, 0, "LUMINANCE_XYZ_CDM2", lum.c_str(), NULL) < 0
	 || p->add_kword(p, 0, "NORMALIZED_TO_Y_100", "YES", NULL) < 0
	 || p->add_kword(p, 0, "INSTRUMENT_TYPE_SPECTRAL", spectral ? "YES" : "NO", NULL) < 0)
		return CgatsFail(p, "cannot add a keyword", err);
	if (spectral)
	{
		sprintf(buf, "%d", t.specBands);
		if (p->add_kword(p, 0, "SPECTRAL_BANDS", buf, NULL) < 0)
			return CgatsFail(p, "cannot add a keyword", err);
		sprintf(buf, "%f", t.specStartNm);
		if (p->add_kword(p, 0, "SPECTRAL_START_NM", buf, NULL) < 0)
			return CgatsFail(p, "cannot add a keyword", err);
		sprintf(buf, "%f", t.specEndNm);
		if (p->add_kword(p, 0, "SPECTRAL_END_NM", buf, NULL) < 0)
			return CgatsFail(p, "cannot add a keyword", err);
	}

	static const char * kFields[7] = { "SAMPLE_ID", "RGB_R", "RGB_G", "RGB_B", "XYZ_X", "XYZ_Y", "XYZ_Z" };
	if (p->add_field(p, 0, kFields[0], nqcs_t) < 0)
		return CgatsFail(p, "cannot add a field", err);
	for (int f = 1; f < 7; f++)
		if (p->add_field(p, 0, kFields[f], r_t) < 0)
			return CgatsFail(p, "cannot add a field", err);
	const int nSpec = spectral ? t.specBands : 0;
	for (int b = 0; b < nSpec; b++)
		if (p->add_field(p, 0, Ti3SpectralFieldName(t, b).c_str(), r_t) < 0)
			return CgatsFail(p, "cannot add a field", err);

	std::vector<cgats_set_elem> row(7 + nSpec);
	for (size_t r = 0; r < t.readings.size(); r++)
	{
		const Ti3Reading & rd = t.readings[r];
		char id[16];
		sprintf(id, "%d", rd.sampleId);
		row[0].c = id;
		for (int c = 0; c < 3; c++)
		{
			row[1 + c].d = rd.rgb[c];
			row[4 + c].d = rd.xyz[c] * scale;
		}
		for (int b = 0; b < nSpec; b++)
			row[7 + b].d = rd.spectrum[b] * scale;
		if (p->add_setarr(p, 0, &row[0]) < 0)
			return CgatsFail(p, "cannot add a reading", err);
	}

	if (p->write_name(p, path) < 0)
		return CgatsFail(p, "cannot write the file", err);
	p->del(p);
	return true;
}
