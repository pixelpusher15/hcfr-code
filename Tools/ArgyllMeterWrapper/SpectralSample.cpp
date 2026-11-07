/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2012 HCFR Project.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////
//
//  This file is subject to the terms of the GNU General Public License as
//  published by the Free Software Foundation.  A copy of this license is
//  included with this software distribution in the file COPYING.htm. If you
//  do not have a copy, you may obtain a copy by writing to the Free
//  Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details
/////////////////////////////////////////////////////////////////////////////
//  Author(s):
//    John Adcock
//    Ian C
/////////////////////////////////////////////////////////////////////////////

// SpectralSample.cpp: Wrapper for the argyll ccss type
//
//////////////////////////////////////////////////////////////////////

#include "Color.h"
#include "SpectralSample.h"
#include "ArgyllMeterWrapper.h"
#include "SpectralSampleFiles.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <cctype>

//#define SALONEINSTLIB
#define ENABLE_USB
#define ENABLE_FAST_SERIAL
#if defined(_MSC_VER)
#pragma warning(disable:4200)
#include <winsock.h>
#endif
#include "numsup.h"
#include "xspect.h"
// disptechs.h only wraps its enum in extern "C", not its functions
extern "C" {
#include "disptechs.h"
}
#include "ccss.h"
//#undef SALONEINSTLIB

// Map one of Argyll's display technology IDs (see getDisplayTechnologies) to its enum;
// anything else maps to disptech_unknown. disptech_get_strid may patch its argument, so pass a copy.
static disptech dispTechFromString(const std::string& tech)
{
	std::vector<char> buf(tech.begin(), tech.end());
	buf.push_back('\0');
	return disptech_get_strid(&buf[0])->dtech;
}

SpectralSample::SpectralSample(void) :
    m_ccss(NULL)
{
    if ((m_ccss = new_ccss()) == NULL)
    {
        throw std::logic_error("Error allocating data structure for spectral sample");
    }
}

SpectralSample::SpectralSample(const SpectralSample& s) :
    m_ccss(NULL)
{
    if ((m_ccss = new_ccss()) == NULL)
    {
        throw std::logic_error("Error allocating data structure for spectral sample");
    }

    m_Path = s.m_Path;
    m_Display = s.m_Display;
    m_Tech = s.m_Tech;
    m_Description = s.m_Description;
	m_RefInstrument = s.m_RefInstrument;
    if (s.m_ccss != NULL)
    {
        m_ccss->set_ccss(m_ccss, s.m_ccss->orig, s.m_ccss->crdate, s.m_ccss->desc, s.m_ccss->disp, s.m_ccss->dtech, s.m_ccss->refrmode, s.m_ccss->sel, s.m_ccss->ref, NULL, s.m_ccss->samples, s.m_ccss->no_samp);
    }
}

SpectralSample::~SpectralSample(void)
{
    if (m_ccss != NULL)
    {
        m_ccss->del(m_ccss);
    }
}


bool SpectralSample::operator==(const SpectralSample& s) const
{
    if (m_Path == s.m_Path)
    {
        return true;
    }
    return false;
}

SpectralSample& SpectralSample::operator=(const SpectralSample& s)
{
    if (this != &s)
    {
        m_Path = s.m_Path;
        m_Display = s.m_Display;
        m_Tech = s.m_Tech;
        m_Description = s.m_Description;
		m_RefInstrument = s.m_RefInstrument;
    
        if (s.m_ccss != NULL)
        {
            m_ccss->set_ccss(m_ccss, s.m_ccss->orig, s.m_ccss->crdate, s.m_ccss->desc, s.m_ccss->disp, s.m_ccss->dtech, s.m_ccss->refrmode, s.m_ccss->sel, s.m_ccss->ref, NULL, s.m_ccss->samples, s.m_ccss->no_samp);
        }
    }
    return *this;
}


bool SpectralSample::Read(const std::string& samplePath)
{
    if (!samplePath.empty())
    {
        if (m_ccss->read_ccss(m_ccss, (char *)samplePath.c_str()))
        {
            return false;
        }
        m_Path = samplePath;
        m_Display = m_ccss->disp ? m_ccss->disp : "";
        m_Tech = m_ccss->tech ? m_ccss->tech : "";
		m_RefInstrument = m_ccss->ref ? m_ccss->ref : "";
        setDescription(m_Tech.c_str(), m_Display.c_str()); 
        return true;
    }
    else
    {
        return false;
    }
}


bool SpectralSample::Write(const std::string& samplePath)
{	
	if(m_ccss->write_ccss(m_ccss, (char *)samplePath.c_str()) == 0)
	{
		return true;
	}
	else
	{
		throw std::logic_error("Writing CCSS file failed");
		return false;
	}
}

bool SpectralSample::createFromMeasurements(const CColor spectralReadings[], const int nReadings)
{
	// Either displayname or displaytech should be non-null. There is no reason we should not enforce both

	if (m_Display.empty() || m_Tech.empty())
	{
		std::string errorMessage = "Display name and tech must be supplied";
		throw std::logic_error(errorMessage);
	}
	setDescription(m_Tech.c_str(), m_Display.c_str()); 

	// First check to see what the highest value is

	double bigv = -1e60;

	for (int nColors = 0; nColors < nReadings; nColors++)
	{
		CSpectrum spectrum = spectralReadings[nColors].GetSpectrum();

		// Check if we have spectral data

		if (spectrum.GetRows() <= 0)
		{
			std::string errorMessage = "Did not manage to read any spectral values!";
			throw std::logic_error(errorMessage);
		}
		// spec_n is written as-is and write_ccss reads that many bands, so more than the
		// xspect array holds would read past it. length_error so the caller can show
		// its localized IDS_SPECTRAL_SAMPLE_TOO_MANY_BANDS instead of this text.
		if (spectrum.GetRows() > XSPECT_MAX_BANDS)
		{
			std::string errorMessage = "Too many spectral bands for a spectral sample";
			throw std::length_error(errorMessage);
		}
		for (int i = 0; i < spectrum.GetRows(); i++)
		{
			if (spectrum[i] > bigv)
			{
				bigv = spectrum[i];
			}
		}
	}
	
	// Now normalize the values
	
	xspect *samples = new xspect[nReadings];

	for (int nColors = 0; nColors < nReadings; nColors++)
	{
		CSpectrum spectrum = spectralReadings[nColors].GetSpectrum();
		double scale = 100;

		// norm must be set: write_ccss saves it as SPECTRAL_NORM. It must be 1.0, as in the
		// .ccss files Argyll ships: the Spyder calibration divides the sample XYZ by norm but
		// reads the sensor response from the raw values, so any other value skews it.

		samples[nColors].spec_n = spectrum.GetRows();
		samples[nColors].spec_wl_short = spectrum.m_WaveLengthMin;
		samples[nColors].spec_wl_long = spectrum.m_WaveLengthMax;
		samples[nColors].norm = 1.0;

		for (int nSpec = 0; nSpec < samples[nColors].spec_n; nSpec++)
		{
			samples[nColors].spec[nSpec] = spectrum[nSpec];
			samples[nColors].spec[nSpec] *= scale / bigv;
		}
	}

	bool bRet = !m_ccss->set_ccss(m_ccss, "HCFR ccss generator", NULL, 	
									(char *)m_Description.c_str(), 
									(char *)m_Display.c_str(), 
									dispTechFromString(m_Tech), 
                                    -1,
                                    NULL,
									(char *)m_RefInstrument.c_str(), NULL,
									samples, nReadings);

	delete [] samples;

	if (!bRet) 
	{
		std::string errorMessage = "set_ccss failed with '%s'";
		errorMessage += m_ccss->e.m;
		throw std::logic_error(errorMessage);
	}

	return bRet;
}


bool SpectralSample::createFromCorrelationCSV(const std::string& csvPath)
{
	const int BANDS = 401;
	const int START_NM = 380;
	const int END_NM = 780;

	std::ifstream fh(csvPath.c_str(), std::ios::binary);
	if (!fh.is_open())
		throw std::logic_error("Cannot open correlation CSV file");

	// Parse every row into 401 spectral values.
	std::vector< std::vector<double> > rows;
	std::string line;
	bool firstLine = true;
	while (std::getline(fh, line))
	{
		if (firstLine && line.size() >= 3 &&
			(unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF)
			line.erase(0, 3);						// strip UTF-8 BOM
		firstLine = false;

		while (!line.empty() && (line[line.size()-1] == '\r' || line[line.size()-1] == '\n'))
			line.erase(line.size()-1);
		if (line.find_first_not_of(" \t,") == std::string::npos)
			continue;								// blank / comma-only line

		std::vector<std::string> cells;
		std::stringstream ss(line);
		std::string cell;
		while (std::getline(ss, cell, ','))
			cells.push_back(cell);
		// Drop trailing empty cells (trailing comma / padding), but keep interior
		// empties: skipping an interior empty would shift every following value to
		// the wrong wavelength band.
		while (!cells.empty() && cells.back().find_first_not_of(" \t") == std::string::npos)
			cells.pop_back();

		std::vector<double> vals;
		for (size_t k = 0; k < cells.size(); ++k)
		{
			size_t a = cells[k].find_first_not_of(" \t");
			if (a == std::string::npos)
			{
				vals.push_back(0.0);				// interior empty field -> 0, preserve alignment
				continue;
			}
			size_t b = cells[k].find_last_not_of(" \t");
			vals.push_back(atof(cells[k].substr(a, b - a + 1).c_str()));
		}
		// A short row is a wrong-resolution file (81 values = a 5 nm set), not a spectrum
		// that stops early: padded with zeros it would load as 380-460 nm at 1 nm and
		// correct every reading silently.
		if ((int)vals.size() < BANDS)
			throw std::logic_error("Correlation CSV row has fewer than 401 values (1 nm, 380-780 nm expected)");
		else if ((int)vals.size() > BANDS)
			throw std::logic_error("Correlation CSV row has more than 401 values");
		rows.push_back(vals);
	}
	fh.close();

	// Global max, used both to drop empty rows and to normalize the set.
	double gmax = 0.0;
	for (size_t i = 0; i < rows.size(); ++i)
		for (int j = 0; j < BANDS; ++j)
			if (rows[i][j] > gmax) gmax = rows[i][j];
	if (gmax <= 0.0)
		throw std::logic_error("Correlation CSV file has no usable spectral data");

	// Keep only rows with real signal (drops zero/near-zero padding rows).
	double rowThresh = gmax * 1e-3;
	std::vector<int> keep;
	for (size_t i = 0; i < rows.size(); ++i)
	{
		double rmax = 0.0;
		for (int j = 0; j < BANDS; ++j)
			if (rows[i][j] > rmax) rmax = rows[i][j];
		if (rmax > rowThresh) keep.push_back((int)i);
	}
	if ((int)keep.size() < 3)
		throw std::logic_error("Correlation CSV file needs at least 3 non-empty spectra");

	// Description from the file name (correlation CSVs carry no metadata). The
	// panel technology is usually spelled out in the file name, so derive it
	// from there rather than leaving it "Unknown".
	std::string display = csvPath;
	size_t slash = display.find_last_of("/\\");
	if (slash != std::string::npos) display = display.substr(slash + 1);
	size_t dot = display.find_last_of('.');
	if (dot != std::string::npos) display = display.substr(0, dot);

	std::string lower = display;
	for (size_t i = 0; i < lower.size(); ++i) lower[i] = (char)tolower((unsigned char)lower[i]);
	const char* tech = "Unknown";
	if      (lower.find("qd-oled") != std::string::npos || lower.find("qd oled") != std::string::npos) tech = "QD OLED";
	else if (lower.find("wrgb") != std::string::npos || lower.find("woled") != std::string::npos)      tech = "WOLED";
	else if (lower.find("oled") != std::string::npos)                                                  tech = "OLED";
	else if (lower.find("mini-led") != std::string::npos || lower.find("miniled") != std::string::npos) tech = "LCD Mini-LED";
	else if (lower.find("w-led") != std::string::npos || lower.find("wled") != std::string::npos)       tech = "LCD White LED";
	else if (lower.find("ccfl") != std::string::npos)                                                   tech = "LCD CCFL";
	else if (lower.find("laser") != std::string::npos)                                                  tech = "DLP Laser";
	else if (lower.find("crt") != std::string::npos)                                                    tech = "CRT";
	else if (lower.find("led") != std::string::npos)                                                    tech = "LCD LED";

	setTech(tech);
	setDisplay(display.c_str());
	setReferenceInstrument("HCFR correlation import");
	setDescription(m_Tech.c_str(), m_Display.c_str());

	int N = (int)keep.size();
	xspect *samples = new xspect[N]();		// value-init: zero all fields (incl. norm)
	for (int k = 0; k < N; ++k)
	{
		const std::vector<double>& r = rows[keep[k]];
		samples[k].spec_n = BANDS;
		samples[k].spec_wl_short = START_NM;
		samples[k].spec_wl_long = END_NM;
		// norm 1.0, as every Argyll .ccss carries (SPECTRAL_NORM, written from
		// samples[0].norm by ccss.c and defaulted to 1.0 on read). Argyll divides by
		// norm only where it integrates through getval_xspec (xspect.c, xsp2cie
		// convert); the Spyder fit builds its XYZ that way (spyd2.c:2153) but its
		// sensor sums and its extra white samples with the unnormalized
		// value_xspect (spyd2.c:2161, :2175, :2183), so any other norm skews that
		// fit. (The i1d3 fit is norm-invariant: XYZ and sensor RGB both go through
		// convert, i1d3.c:2565/:2576.) A zero norm - the value-initialized one -
		// divides by zero and fails col_cal_spec_set with "too few calibration
		// samples".
		//
		// The 0..100 scaling of spec[] below needs no matching norm: it is one
		// factor on every sample, and both fits are linear in the samples (a
		// least-squares XYZ-from-RGB matrix, the Spyder's extra samples built from
		// the same set's envelope), so the matrix does not depend on it. Argyll's
		// own sets are absolute radiance, around 1e-3..1e-1 (Install/color).
		samples[k].norm = 1.0;
		for (int j = 0; j < BANDS && j < XSPECT_MAX_BANDS; ++j)
			samples[k].spec[j] = r[j] * 100.0 / gmax;	// normalize the set to a common max
	}

	bool bRet = !m_ccss->set_ccss(m_ccss, "HCFR correlation import", NULL,
									(char *)m_Description.c_str(),
									(char *)m_Display.c_str(),
									disptech_unknown,
									-1,
									NULL,
									(char *)m_RefInstrument.c_str(), NULL,
									samples, N);
	delete [] samples;

	if (!bRet)
	{
		std::string errorMessage = "set_ccss failed with '";
		errorMessage += m_ccss->e.m;
		errorMessage += "'";
		throw std::logic_error(errorMessage);
	}
	return bRet;
}


int SpectralSample::getNumSamples() const
{
    return m_ccss ? m_ccss->no_samp : 0;
}


const char* SpectralSample::getDescription() const
{
    return m_Description.c_str();
}


const char* SpectralSample::getDisplay() const
{
    return m_Display.c_str();
}


const char* SpectralSample::getTech() const
{
    return m_Tech.c_str();
}


const char* SpectralSample::getReferenceInstrument() const
{
    return m_RefInstrument.c_str();
}


const char* SpectralSample::getPath() const
{
    return m_Path.c_str();
}


void SpectralSample::setDescription(const char* tech, const char *disp)
{
    // we use exactly the same convention as argyll
    // ccss files are not required to have both the display or the tech specified,
    // however they must have one specified. It's best to identify the file 
    // using a description field composed of the tech and the display. This is
    // how argyll does it in the output of their programs (see spotread.c for example)

    m_Description = tech;
    m_Description += " (";
    m_Description += disp;
    m_Description += ")";
}


void SpectralSample::setDisplay(const char* disp)
{
    m_Display = disp;
}


void SpectralSample::setTech(const char* tech)
{
    m_Tech = tech;
}



std::vector<std::string> SpectralSample::getDisplayTechnologies()
{
	std::vector<std::string> techs;
	for (disptech_info* info = disptech_get_list(); info->dtech != disptech_end; info++)
	{
		techs.push_back(info->strid);
	}
	return techs;
}


void SpectralSample::setReferenceInstrument(const char* instr)
{
    m_RefInstrument = instr;
}


void SpectralSample::setAll(const char* tech, const char* disp, const char *instr, const char* path )
{
    m_Display = disp;
    m_Tech = tech;
    m_Path = path;
	m_RefInstrument = instr;
    setDescription(tech, disp);
}

ccss* SpectralSample::getCCSS() const
{
    return m_ccss;
}

