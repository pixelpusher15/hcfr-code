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

// Ti3Export: write display readings as a CGATS CTI3 file (the "ti3" format
// ArgyllCMS and DisplayCAL read for display profiling). MFC-free so the
// console test harness can reach it. The file is written through the CGATS
// I/O library that already ships in the tree (Tools/spectro/cgats.c, MIT
// licensed) and is linked, not copied: this module only calls its API.
//
// One table per file, device class DISPLAY, color representation RGB_XYZ:
//   SAMPLE_ID RGB_R RGB_G RGB_B XYZ_X XYZ_Y XYZ_Z [SPEC_nnn ...]
// RGB is the stimulus in percent (0-100) of the signal range, exactly as HCFR
// stores it with each reading. XYZ is normalized so that the white the caller
// names has Y = 100 (NORMALIZED_TO_Y_100 YES); the white's absolute XYZ in
// cd/m2 goes in LUMINANCE_XYZ_CDM2 so a reader can undo the scale. Spectral
// columns are written only when every reading carries a spectrum on the
// table's band layout; the spectra are scaled by the same factor as XYZ.
// Real numbers are written by the library at 6 significant digits.

#ifndef TI3EXPORT_H_INCLUDED
#define TI3EXPORT_H_INCLUDED

#include <string>
#include <vector>

struct Ti3Reading
{
	int sampleId;					// written as SAMPLE_ID (HCFR uses slot index + 1)
	double rgb[3];					// stimulus, percent of the signal range
	double xyz[3];					// absolute XYZ, cd/m2
	std::vector<double> spectrum;	// absolute spectral values; empty when none
	Ti3Reading() : sampleId(0) { rgb[0] = rgb[1] = rgb[2] = 0.0; xyz[0] = xyz[1] = xyz[2] = 0.0; }
};

struct Ti3Table
{
	std::string descriptor;			// DESCRIPTOR keyword (what the readings are)
	std::string originator;			// ORIGINATOR keyword (the writing program)
	double whiteXYZ[3];				// absolute XYZ of the normalizing white, cd/m2; Y must be > 0
	int specBands;					// spectral layout shared by every reading (0 = none)
	double specStartNm;
	double specEndNm;
	std::vector<Ti3Reading> readings;
	Ti3Table() : specBands(0), specStartNm(0.0), specEndNm(0.0) { whiteXYZ[0] = whiteXYZ[1] = whiteXYZ[2] = 0.0; }
};

// True when the table will carry spectral columns: a layout with at least two
// bands on whole-nanometer wavelengths, and every reading has exactly that
// many values.
bool Ti3HasSpectralColumns(const Ti3Table & t);

// The CGATS field name of spectral band i ("SPEC_380" ...).
std::string Ti3SpectralFieldName(const Ti3Table & t, int band);

// The LUMINANCE_XYZ_CDM2 keyword value for an absolute white ("X Y Z", %.6f).
std::string Ti3LuminanceKeyword(const double whiteXYZ[3]);

// Write the table to path. Returns false with a message in err when the white
// is unusable, there are no readings, or the CGATS library reports an error.
bool WriteTi3File(const char * path, const Ti3Table & t, std::string & err);

#endif // TI3EXPORT_H_INCLUDED
