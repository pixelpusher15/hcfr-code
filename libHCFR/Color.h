/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2012 Association Homecinema Francophone.  All rights reserved.
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
//	François-Xavier CHABOUD
//  Georges GALLERAND
//  John Adcock
/////////////////////////////////////////////////////////////////////////////

// Color.h: interface for the CColor class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(COLOR_H_INCLUDED_)
#define COLOR_H_INCLUDED_

#ifndef PI
#define PI 3.141592653589793f
#endif

#define FX_NODATA -99999.99

#define MAX_USER_CC_PATCH_SIZE 10000

#include "libHCFR_Config.h"
#include "Matrix.h"
#include <string>   // std::string in CColorProvenance (below), before class CColor
#include "WireSetup.h"   // CColorWireSetup, before class CColor

typedef enum 
{
	PALSECAM = 0,
	SDTV = 1,
	HDTV = 2,
	HDTVa = 3, //75%
	HDTVb = 4, //Plasma
	sRGB = 5,
	UHDTV = 6,  //P3
	UHDTV2 = 7, //BT.2020
	UHDTV3 = 8, //P3 in BT.2020
	UHDTV4 = 9, //R709 in BT.2020
    CUSTOM = 10,
	CC6 = 11 //unused
} ColorStandard;

typedef enum 
{
	GCD = 0,
	MCD = 1,
	CMC = 3,
	CMS = 4,
	CPS = 5,
	SKIN = 2,
	CCSG = 6,
	AXIS = 7,
	CM4LUM = 8,
	CM5LUM = 9,
	CM10LUM = 10,
	CM4SAT = 11,
	CM4SAT75 = 12,
	CM5SAT = 13,
	CM5SAT75 = 14,
	CM10SAT = 15,
	CM10SAT75 = 16,
	CM6NB = 17,
	CMDNR = 18,
	MASCIOR50 = 19,
	LG54016 = 20,
	LG54017 = 21,
	LG100017 = 22,
	LG400017 = 23,
	LGUK65XX = 24,
	LGOLEDV12018 = 25,
	LGOLEDV22018 = 26,
	LGOLEDV32018 = 27,
	LGOLED102019 = 28,
	LGOLED222019 = 29,
	LGOLED102020 = 30,
	LGOLED222020 = 31,
	LGOLED102021 = 32,
	LGOLED222021 = 33,
	CCMAXHDR = LGOLED222021,
	RANDOM250 = 34,
	RANDOM500 = 35,
    USER = 36
} CCPatterns;

// "Is this one of the Mascior-style HDR color-checker sets?" - the sets that
// keep their own dE convention (a * 100 reference scale, normalised against the
// grayscale top rather than the prime white). This is a CONTIGUOUS-RANGE
// assumption about the enum above, so it lives here beside the constants that
// have to stay contiguous, instead of being spelled out again at each consumer.
inline bool IsMasciorCC ( int ccMode )
{
	return ( ccMode >= MASCIOR50 && ccMode <= CCMAXHDR );
}

typedef enum 
{
	Default = -1,
	D65 = 0,
	D55 = 1,
	D50 = 2,
	D75 = 3,
	D93 = 4,
	A=5,
	B=6,
	C=7,
	E=8,
	DCI=9,
    DCUST=10
} WhiteTarget;

class CSpectrum;
class CColorReference;

class ColorTriplet : public Matrix
{
protected:
    ColorTriplet();
    ColorTriplet(const Matrix& matrix);
    ColorTriplet(double a, double b, double c);
public:
    const double& operator[](const int nRow) const;
    double& operator[](const int nRow);
    bool isValid() const;
};

class ColorRGB;
class ColorxyY;

class ColorXYZ : public ColorTriplet
{
public:
    ColorXYZ();
    explicit ColorXYZ(const Matrix& matrix);
    ColorXYZ(const ColorRGB& RGB, CColorReference colorReference);
    explicit ColorXYZ(const ColorxyY& xyY);
    ColorXYZ(double X, double Y, double Z);
    int GetColorTemp(const CColorReference& colorReference) const;
    double GetDeltaLCH(double YWhite, const ColorXYZ& refColor, double YWhiteRef, const CColorReference & colorReference, int dE_form, bool isGS, int gw_Weight, double &dC, double &dH ) const;
    double GetDeltaE(double YWhite, const ColorXYZ& refColor, double YWhiteRef, const CColorReference & colorReference, int dE_form, bool isGS, int gw_Weight ) const;
    double GetOldDeltaE(const ColorXYZ& refColor) const;
	double GetDeltaxy(const ColorXYZ& refColor, const CColorReference& colorReference) const;
};

class ColorxyY: public ColorTriplet
{
public:
    ColorxyY();
    explicit ColorxyY(const Matrix& matrix);
    explicit ColorxyY(const ColorXYZ& XYZ);
    ColorxyY(double x, double y, double YY = 1.0);
};

class Colorxyz: public ColorTriplet
{
public:
    Colorxyz();
    explicit Colorxyz(const Matrix& matrix);
    explicit Colorxyz(const ColorXYZ& XYZ);
    Colorxyz(double x, double y, double z);
};

class ColorRGB: public ColorTriplet
{
public:
    ColorRGB();
    explicit ColorRGB(const Matrix& matrix);
    ColorRGB(const ColorXYZ& XYZ, CColorReference colorReference);
    ColorRGB(double r, double g, double b);
};

class ColorLab: public ColorTriplet
{
public:
    ColorLab();
    explicit ColorLab(const Matrix& matrix);
    ColorLab(const ColorXYZ& XYZ, double YWhiteRef, CColorReference colorReference);
    ColorLab(double L, double a, double b);
};

class ColorICtCp: public ColorTriplet
{
public:
    ColorICtCp();
    explicit ColorICtCp(const Matrix& matrix);
    ColorICtCp(const ColorXYZ& XYZ, double YWhiteRef, CColorReference colorReference);
    ColorICtCp(double I, double Ct, double Cp);
};

class ColorLuv: public ColorTriplet
{
public:
    ColorLuv();
    explicit ColorLuv(const Matrix& matrix);
    ColorLuv(const ColorXYZ& XYZ, double YWhiteRef, CColorReference colorReference);
    ColorLuv(double L, double u, double v);
};

class ColorLCH: public ColorTriplet
{
public:
    ColorLCH();
    explicit ColorLCH(const Matrix& matrix);
    ColorLCH(const ColorXYZ& XYZ, double YWhiteRef, CColorReference colorReference);
    ColorLCH(double L, double C, double H);
};

class ColorLMS: public ColorTriplet
{

public:
    ColorLMS();
    explicit ColorLMS(const Matrix& matrix);
    ColorLMS(const ColorXYZ& XYZ, double YWhiteRef, CColorReference colorReference);
    ColorLMS(double L, double M, double S);
};

class ColorRGBDisplay : public ColorTriplet
{
public:
    ColorRGBDisplay();
    explicit ColorRGBDisplay(double aGreyPercent);
    ColorRGBDisplay(double aRedPercent,double aGreenPercent,double aBluePercent);

#ifdef LIBHCFR_HAS_WIN32_API
    explicit ColorRGBDisplay(COLORREF aColor);
    COLORREF GetColorRef(bool is16_235) const;
    static BYTE ConvertPercentToBYTE(double percent, bool is16_235);
    static int ConvertPercentToCode(double percent, bool is16_235, int bits);
    // HCFR's test-color window: its picker holds an 8-bit full-range code, which
    // the window paints as is, or for a limited-range generator as
    // round(c * 219/255 + 16) clamped to 16..235.
    static int TestWindowShownCode(int pickerCode, bool is16_235);
    // The picker code the window shows as percent's nearest 8-bit code in the range.
    static int TestWindowPickerCode(double percent, bool is16_235);
#endif
};

// Per-measurement label provenance: which patch a reading corresponds to and how its
// source stimulus was defined (bit depth / range / name). Lets a measurement stay
// self-describing independent of the patch-list file it came from - the basis for
// browsable multi-set patch lists and volumetric characterization. Optional, lazily
// allocated, mirroring the raw-XYZ / stimulus optional fields on CColor.
//
// Stored in the CColor extension trailer as ArchiveTag::COLOR_PROVENANCE:
// patchNumber, sequenceIndex, srcBits, srcRange (4 x int), name (CString),
// then role (int). The value is append-only: a reader takes the fields it
// knows and ignores any that follow, and a value without the role reads as
// PROVENANCE_ROLE_UNKNOWN.

// What a reading was for, as the source sequence row declared it. Per reading,
// not per run, because one run mixes roles. A patch-list CSV declares none, so
// every reading it drives is UNKNOWN. Values are stored; never renumber.
enum CColorProvenanceRole
{
	PROVENANCE_ROLE_UNKNOWN        = -1,
	PROVENANCE_ROLE_COLOR          = 0,	// C: an ordinary measured patch
	PROVENANCE_ROLE_ANCHOR         = 1,	// A: a drift anchor
	PROVENANCE_ROLE_PREROLL        = 2,	// P: a warm-up patch before the run
	PROVENANCE_ROLE_VERIFICATION   = 3,	// V: a verification patch
	PROVENANCE_ROLE_INTERSTITIAL   = 4,	// I: shown between patches, never measured
	PROVENANCE_ROLE_IDENTIFICATION = 5	// X: identifies the run on the wire
};

struct CColorProvenance
{
	int         patchNumber;    // stable identity within the source list (-1 = unknown)
	int         sequenceIndex;  // order measured within the sweep (-1 = unknown)
	int         srcBits;        // source bit depth: 8 or 10 (0 = unknown)
	int         srcRange;       // source CSV range: -1 legacy, 0 full, 1 legal, 2 extended
	std::string name;           // patch name from the source list ("" = none)
	int         role;           // CColorProvenanceRole
	CColorProvenance() : patchNumber(-1), sequenceIndex(-1), srcBits(0), srcRange(-1), role(PROVENANCE_ROLE_UNKNOWN) {}
};

class CColor
{
public:
	CColor();
	CColor(const CColor& aColor);
	CColor(const ColorXYZ& aMatrix);
	CColor(double aX,double aY, double aZ);	// XYZ initialisation
	CColor(double ax,double ay);			// xy initialisation
    CColor(ifstream &theFile);
	~CColor();
	void ClearSpectrumLux();

	CColor& operator =(const CColor& obj);

	const double& operator[](const int nRow) const;
	double& operator[](const int nRow);

    bool isValid() const;

	double GetLuminance() const;
	double GetDeltaLCH(double YWhite, const CColor & refColor, double YWhiteRef, const CColorReference & colorReference, int dE_form, bool isGS, int gw_Weight, double &dC, double &dH ) const;
	double GetDeltaE(double YWhite, const CColor & refColor, double YWhiteRef, const CColorReference & colorReference, int dE_form, bool isGS, int gw_Weight ) const;
	double GetDeltaxy(const CColor & refColor, const CColorReference& colorReference) const;
    double GetDeltaE(const CColor & refColor) const;
	ColorXYZ GetXYZValue() const;
	ColorRGB GetRGBValue(CColorReference colorReference) const;
	ColorxyY GetxyYValue() const;
	Colorxyz GetxyzValue() const;
	ColorLab GetLabValue(double YWhiteRef, CColorReference colorReference) const;
	ColorLCH GetLCHValue(double YWhiteRef, CColorReference colorReference) const;
	ColorLMS GetLMSValue(double YWhiteRef, CColorReference colorReference) const;
	ColorICtCp GetICtCpValue(double YWhiteRef, CColorReference colorReference) const;

	void SetXYZValue(const ColorXYZ& aColor);
	void SetRGBValue(const ColorRGB& aColor, CColorReference colorReference);
	void SetxyYValue(double x, double y, double Y);
    void SetxyYValue(const ColorxyY& aColor);

	void SetX(double aX) { m_XYZValues[0]=aX; }
	void SetY(double aY) { m_XYZValues[1]=aY; }
	void SetZ(double aZ) { m_XYZValues[2]=aZ; }

	double GetX() const { return m_XYZValues[0]; } 
	double GetY() const { return m_XYZValues[1]; }
	double GetZ() const { return m_XYZValues[2]; }

	void SetSpectrum ( CSpectrum & aSpectrum );
	void ResetSpectrum ();
	bool HasSpectrum () const;
	CSpectrum GetSpectrum () const;

	void SetLuxValue ( double LuxValue );
	void ResetLuxValue ();
	bool HasLuxValue () const;
	double GetLuxValue () const;
	double GetLuxOrLumaValue (const int luminanceCurveMode) const;
	double GetPreferedLuxValue (bool preferLuxmeter) const;
    void applyAdjustmentMatrix(const Matrix& adjustment);

	// Uncorrected sensor reading, captured before any calibration matrix is applied.
	// Retained so a measurement can be recalibrated later without composing deltas
	// onto an already-corrected value (needed for per-sub-gamut Bodner recalibration).
	void SetRawXYZValue(const ColorXYZ& aRaw);
	void ClearRawXYZValue();
	void ScaleXYZ(double fScale);
	bool HasRawXYZValue() const;
	ColorXYZ GetRawXYZValue() const;

	// Drive stimulus (RGB %) that produced this reading, captured at measure time
	// alongside the raw XYZ. Lets an export or a later recompute report the exact
	// signal sent to the display rather than reconstructing it from the generator.
	void SetStimulusValue(const ColorRGBDisplay& aStimulus);
	void ClearStimulusValue();
	bool HasStimulusValue() const;
	ColorRGBDisplay GetStimulusValue() const;

	// Label provenance (patch#/name/bit-depth/range) for a self-describing measurement.
	// Optional, lazily allocated; mirrors the raw-XYZ / stimulus optional fields.
	void SetProvenance(const CColorProvenance& aProv);
	void ClearProvenance();
	bool HasProvenance() const;
	CColorProvenance GetProvenance() const;
	// Wall-clock time of the reading, in seconds since the Unix epoch with a
	// sub-second fraction, stamped at measure time beside the raw XYZ and the
	// stimulus. Drift anchors need time, not index, to be plotted against the
	// clock. Legacy readings carry none.
	void SetTimestamp(double aTimestamp);
	void ClearTimestamp();
	bool HasTimestamp() const;
	double GetTimestamp() const;		// 0.0 when none
	// The wire-setup stamp (WireSetup.h): whether the stimulus is the patch the
	// generator displayed for this reading, and how it went out on the wire.
	// Stamped by CSensor::MeasureColor on every reading this build takes; a
	// reading from an older file has none, which means unknown.
	void SetWireSetup(const CColorWireSetup& aWire);
	void ClearWireSetup();
	bool HasWireSetup() const;
	CColorWireSetup GetWireSetup() const;	// the defaults (realDrive false) when none

    void Output(ostream& ostr) const;

#ifdef LIBHCFR_HAS_MFC
    void Serialize(CArchive& archive);

	// When true, Serialize stores the pre-#193 layout (base 1/3/5/6, the two
	// ignore matrices, a v1 spectrum, no trailer) that a 4.1 reader accepts.
	// Set ONLY through CColorLegacyStoreScope, by the SaveOldCalibrationFile
	// writer; never on a load.
	static bool s_bStoreLegacy;
#endif

protected:
    ColorXYZ m_XYZValues;
	CSpectrum *	m_pSpectrum;
	double *	m_pLuxValue;
	ColorXYZ *	m_pRawXYZValues;
	ColorRGBDisplay *	m_pStimulus;
	CColorProvenance *	m_pProvenance;
	double *	m_pTimestamp;
	CColorWireSetup *	m_pWireSetup;

#ifdef LIBHCFR_HAS_MFC
private:
	// The load half of Serialize, run on a temporary so a refused load leaves
	// the target color untouched.
	void LoadFrom(CArchive& archive);
#endif
};

#ifdef LIBHCFR_HAS_MFC
// Switches CColor::Serialize to its legacy store layout for the lifetime of
// the scope, and restores the previous setting on exit (so scopes nest and an
// exception cannot leave it on). Used only by CCalibrationInfo::Serialize when
// it writes its v1 (SaveOldCalibrationFile) block.
struct CColorLegacyStoreScope
{
	CColorLegacyStoreScope() : m_bPrevious(CColor::s_bStoreLegacy) { CColor::s_bStoreLegacy = true; }
	~CColorLegacyStoreScope() { CColor::s_bStoreLegacy = m_bPrevious; }

private:
	bool m_bPrevious;

	CColorLegacyStoreScope(const CColorLegacyStoreScope&);
	CColorLegacyStoreScope& operator=(const CColorLegacyStoreScope&);
};
#endif

class CSpectrum: public Matrix
{
public:
	CSpectrum(int NbBands, int WaveLengthMin, int WaveLengthMax, double BandWidth );
	CSpectrum(int NbBands, int WaveLengthMin, int WaveLengthMax, double BandWidth, double * pValues );
    CSpectrum (ifstream &theFile, bool oldFileFormat = false);

	const double& operator[](const int nRow) const;

	CColor GetXYZValue() const;

#ifdef LIBHCFR_HAS_MFC
    void Serialize(CArchive& archive);
#endif

public:
	int		m_WaveLengthMin;
	int		m_WaveLengthMax;
	double	m_BandWidth;
};

class CColorReference
{
public:
	ColorStandard m_standard;
	WhiteTarget m_white;

	ColorXYZ whiteColor;
	string standardName;
	const char *whiteName;
	ColorXYZ redPrimary;
	ColorXYZ greenPrimary;
	ColorXYZ bluePrimary;
	ColorXYZ yellowSecondary;
	ColorXYZ cyanSecondary;
	ColorXYZ magentaSecondary;
	double gamma;
	Matrix RGBtoXYZMatrix;
	Matrix XYZtoRGBMatrix;

    void	UpdateSecondary ( ColorXYZ& secondary, const ColorXYZ& primary1, const ColorXYZ& primary2, const ColorXYZ& primaryOpposite );

public:
	CColorReference(ColorStandard aStandard, WhiteTarget aWhiteTarget=Default, double aGamma=-1.0, string strModified=" ", ColorXYZ c_whitecolor=ColorXYZ(), ColorxyY c_redcolor=ColorxyY(), ColorxyY c_greencolor=ColorxyY(), ColorxyY c_bluecolor=ColorxyY() );
	~CColorReference();
	ColorXYZ GetWhite() const { return whiteColor; } 
	string GetName() const {return standardName;}
	const char *GetWhiteName() const {return whiteName; }
	ColorXYZ GetRed() const { return redPrimary; }
	ColorXYZ GetGreen() const { return greenPrimary; }
	ColorXYZ GetBlue() const { return bluePrimary; }
	ColorXYZ GetYellow() const { return yellowSecondary; }
	ColorXYZ GetCyan() const { return cyanSecondary; }
	ColorXYZ GetMagenta() const { return magentaSecondary; }
	
	// Primary colors relative-to-white luminance, depending on color standard. White luminance reference value is 1.
	//standard 3 added 75% of rec709 colorspace to be used with 75% saturation patterns so ref luminance is the same as full colorspace
	//standard 5 added a set of 6 color checker patterns

	double GetRedReferenceLuma (bool sats) const
	{ 
		double luma;
		switch(m_standard)
		{
			case HDTVa:
				luma = 0.214;
				break;
			case HDTVb:
				luma = sats?0.214:0.1346;
				break;
			default:
				luma = RGBtoXYZMatrix(1,0); 
				break;
		}
		return luma;
	}
	double GetGreenReferenceLuma (bool sats) const
	{ 
		double luma;
		switch(m_standard)
		{
			case HDTVa:
				luma = 0.709;
				break;
			case HDTVb:
				luma = sats?0.709:0.4545;
				break;
			default:
				luma = RGBtoXYZMatrix(1,1); 
				break;
		}
		return luma;
	}
	double GetBlueReferenceLuma (bool sats) const
	{ 
		double luma;
		switch(m_standard)
		{
			case HDTVa:
				luma = 0.075;
				break;
			case HDTVb:
				luma = sats?0.075:0.2450;
				break;
			default:
				luma = RGBtoXYZMatrix(1,2); 
				break;
		}
		return luma;
	}
	double GetYellowReferenceLuma (bool sats) const
	{ 
		double luma;
		switch(m_standard)
		{
			case HDTVa:
				luma = 0.91125;
				break;
			case HDTVb:
				luma = sats?0.91125:0.564;
				break;
			default:
				luma = RGBtoXYZMatrix(1,1)+RGBtoXYZMatrix(1,0); 
				break;
		}
		return luma;
	}
	double GetCyanReferenceLuma (bool sats) const
	{ 
		double luma;
		switch(m_standard)
		{
			case HDTVa:
				luma = 0.7789;
				break;
			case HDTVb:
				luma = sats?0.7789:0.4798;
				break;
			default:
				luma = RGBtoXYZMatrix(1,1)+RGBtoXYZMatrix(1,2); 
				break;
		}
		return luma;
	}
	double GetMagentaReferenceLuma (bool sats) const
	{ 
		double luma;
		switch(m_standard)
		{
			case HDTVa:
				luma = 0.27296;
				break;
			case HDTVb:
				luma = sats?0.27296:0.1775;
				break;
			default:
				luma = RGBtoXYZMatrix(1,0)+RGBtoXYZMatrix(1,2); 
				break;
		}
		return luma;
	}
	double GetCC24ReferenceLuma (int nCol, int aCCMode)
	{ 
			double	YLumaGCD[24]={	0.0,
					0.3473,
					0.4982,
					0.6470,
					0.7905,
					1.0,
					0.0970,
					0.3521,
					0.1875,
					0.1288,
					0.2347,
					0.4211,
					0.2832,
					0.1149,
					0.1839,
					0.0633,
					0.4335,
					0.4262,
					0.0590,
					0.2300,
					0.1142,
					0.5953,
					0.1872,
					0.1946
					};
			double YLumaMCD[24]={	
					0.0302,
					0.0846,
					0.1911,
					0.3531,
					0.5844,
					0.8817,
					0.0944,
					0.3480,
					0.1871,
					0.1309,
					0.2333,
					0.4218,
					0.2845,
					0.1128,
					0.1860,
					0.0633,
					0.4333,
					0.4305,
					0.0591,
					0.2305,
					0.1142,
					0.5953,
					0.1843,
					0.1895
				}; 
			double YLumaCMC[24]={	
					0.0019,
					0.0089,
					0.0220,
					0.0417,
					0.0684,
					0.1026,
					0.1444,
					0.1942,
					0.0065,
					0.0301,
					0.0740,
					0.1402,
					0.2301,
					0.3449,
					0.4856,
					0.6532,
					0.0007,
					0.0030,
					0.0075,
					0.0141,
					0.0232,
					0.0348,
					0.0490,
					0.0659
				}; 
			double YLumaCMS[19]={	
					0.0019,
					0.0089,
					0.0220,
					0.0417,
					0.0684,
					0.1026,
					0.1444,
					0.1942,
					0.0065,
					0.0301,
					0.0740,
					0.1402,
					0.2301,
					0.3449,
					0.4856,
					0.6532,
					0.0007,
					0.0030,
					0.0075
				}; 
			double YLumaCPS[19]={	
					0.0019,
					0.0089,
					0.0220,
					0.0417,
					0.0684,
					0.1026,
					0.1444,
					0.1942,
					0.0065,
					0.0301,
					0.0740,
					0.1402,
					0.2301,
					0.3449,
					0.4856,
					0.6532,
					0.0007,
					0.0030,
					0.0075
			}; 
			double YLumaSKIN[24]={	
					0.7839,
					0.7031,
					0.6607,
					0.5309,
					0.5802,
					0.7605,
					0.5340,
					0.4339,
					0.4288,
					0.3620,
					0.3672,
					0.2619,
					0.2162,
					0.2073,
					0.6455,
					0.4636,
					0.2595,
					0.2218,
					0.1852,
					0.0491,
					0.2976,
					0.2320,
					0.0712,
					0.2604
				}; 
			double YLumaCCSG[96]={	
                1.0000001	,
                0.738088067	,
                0.562500085	,
                0.48443202	,
                0.412722924	,
                0.336036738	,
                0.277845557	,
                0.179393853	,
                0.145822012	,
                0.109913453	,
                0.084623747	,
                0.065152885	,
                0.031299435	,
                0.019302872	,
                0.0	,
                0.072632706	,
                0.033599124	,
                0.649729299	,
                0.068448725	,
                0.320011162	,
                0.151796246	,
                0.101612302	,
                0.195190895	,
                0.383074167	,
                0.648655441	,
                0.032527814	,
                0.122922601	,
                0.288220499	,
                0.121063543	,
                0.654810398	,
                0.286559727	,
                0.082476567	,
                0.160002235	,
                0.033351688	,
                0.41715407	,
                0.396663407	,
                0.75617621	,
                0.129543333	,
                0.028820265	,
                0.056027244	,
                0.02947327	,
                0.653485212	,
                0.027030095	,
                0.200048555	,
                0.096366392	,
                0.574202851	,
                0.165108056	,
                0.174045419	,
                0.644588514	,
                0.286302395	,
                0.11299344	,
                0.176524689	,
                0.277228756	,
                0.655379263	,
                0.63678518	,
                0.294800906	,
                0.207018188	,
                0.289558283	,
                0.030861033	,
                0.642701225	,
                0.307158163	,
                0.469206417	,
                0.028982311	,
                0.283102257	,
                0.295624136	,
                0.507752548	,
                0.318401466	,
                0.135497878	,
                0.365812171	,
                0.141352885	,
                0.319847087	,
                0.445366757	,
                0.58900034	,
                0.276587018	,
                0.201050398	,
                0.328841056	,
                0.445066932	,
                0.332020667	,
                0.32463258	,
                0.337987282	,
                0.089262765	,
                0.344029939	,
                0.302082933	,
                0.42851571	,
                0.033222132	,
                0.284512795	,
                0.186593377	,
                0.036389845	,
                0.275583029	,
                0.284388122	,
                0.187683865	,
                0.30140631	,
                0.295256131	,
                0.295157132	,
                0.437537547	,
                0.029079704					
                };
        
        switch(aCCMode)
        {
        case GCD:
            return YLumaGCD[nCol];
        case MCD:
            return YLumaMCD[nCol];
        case CMC:
            return YLumaCMC[nCol];
        case CMS:
            return YLumaCMS[nCol];
        case CPS:
            return YLumaCPS[nCol];
        case SKIN:
            return YLumaSKIN[nCol];
        case CCSG:
            return YLumaCCSG[nCol];
        }
            return YLumaGCD[nCol];
	}



#ifdef LIBHCFR_HAS_MFC
    void Serialize(CArchive& archive);
#endif
};

ostream& operator <<(ostream& ostr, const CColor& obj);

extern CColorReference GetStandardColorReference(ColorStandard aColorStandard);
extern CColorReference ContainerInnerReference(const CColorReference& active);
extern CColorReference ContainerTransportReference(const CColorReference& active);
extern CColorReference SpecialModeGamutReference(const CColorReference& active);
extern ColorRGB ContainerPrimaryLinear(const CColorReference& active, int index);

extern CColor theMeasure;
extern CColor noDataColor;

// Sensor reads come back with X, Y and Z each floored at 1e-8 before the
// sensor matrix is applied (CSensor::MeasureColor), so a reading of no light
// still has Y > 0 - at the equal-energy point, x = y = 1/3, with the default
// identity matrix, which looks like a real tint. This cut (cd/m2) tells that
// placeholder from light: 100x above the floor and
// well under the ~1e-5 the most sensitive calibration meters resolve, so
// every real reading, however dark, is kept. The contrast readouts use it
// too (grayscale header and on/off), so "Infinity" and an empty black
// agree.
const double BLACK_MIN_LIGHT_Y = 0.000001;
inline bool HasMeasuredLight ( double Y ) { return Y > BLACK_MIN_LIGHT_Y; }

// Target for black (0%) on the grayscale page - see Color.cpp.
extern CColor BlackTargetReference ( const CColor & aMeasure, double YWhite, const CColor & aReference, const CColor & refWhite );

// Tool functions
extern void GenerateSaturationColors (const CColorReference& colorReference, ColorRGBDisplay* GenColors, int nSteps, bool bRed, bool bGreen, bool bBlue, int mode = 0, double stimLevel = 1.0, bool b10bit = false, bool is16_235 = true);
extern bool GenerateCC24Colors (const CColorReference& colorReference, ColorRGBDisplay* GenColors, int aCCMode, int mode, bool b10bit = false, bool is16_235 = true);
extern int GenerateProfileColors (ColorRGBDisplay* GenColors, int maxEntries, int cubeN, bool bGrayExtras);
extern void RemapProfileToTransport (ColorRGBDisplay* GenColors, int n, const CColorReference& colorReference, int mode, bool b10bit, bool is16_235);
extern Matrix ComputeConversionMatrix(const ColorXYZ measures[3], const ColorXYZ references[3], const ColorXYZ & WhiteTest, const ColorXYZ & WhiteRef, bool	bUseOnlyPrimaries, bool bScaleLuminance = true);

// User-selectable colorimeter correction methods. Shared between CColorHCFRConfig
// (the user's chosen setting) and CSensor (which method actually produced the
// matrices a given sensor is using), so keep values in sync between the two.
enum CalibrationMatrixMethod
{
	// Values are persisted (config + .chc) - never renumber; append new methods.
	CALIB_CLASSIC_NIST			= 0,	// ASTM E1455-92 "RGB method": 3-primary tristimulus matrix, no white, no luminance scaling
	CALIB_HCFR_DEFAULT			= 1,	// NIST Four-Color Method (FCMM, Ohno & Hardis 1997) + the paper's optional luminance scaling
	CALIB_BODNER_THREEMATRIX	= 2,	// RGBW sub-gamut three-matrix method (Bodner & Robinson, SID 2019)
	CALIB_FCMM_NO_LUM			= 3,	// NIST Four-Color Method (FCMM), chromaticity only - no luminance scaling
	CALIB_SPECTRAL_SAMPLE		= 4,		// Spectral sample (ccss) correction derived from the meter driver's two
										// emission calibration matrices (libHCFR/SpectralCorrection.h); applied by
										// HCFR to native readings exactly like the FCMM matrix. Chosen by loading a
										// sample, never from the Advanced page's method list.

	// Not a method: one past the last one. Keep it last; readers refuse a
	// stored method at or above it as a file from a newer HCFR.
	CALIB_METHOD_COUNT
};

// Whether the stored readings that carry no raw sensor value (recorded before
// raw capture existed, or edited by hand) still embody the sensor's CURRENT
// single correction matrix. Only then may a matrix recalibration move them by
// composing delta = new * inverse(current) onto them - the bComposeLegacy
// argument of CMeasure::ApplySensorAdjustmentMatrix; otherwise they are left as
// they are and counted in the skipped-readings notice.
//
// This is state, not something the previous calibration method can answer. A
// Bodner fit and a matrix written without walking the readings (DDE
// SetSensorMatrix, Change Sensor) each leave them embodying a matrix the sensor
// no longer has, and every later matrix recalibration skips them again, so they
// stay that way.
// The rule that keyed on "the previous method was single-matrix" saw only the
// first of those and forgot it after one recalibration. CSensor keeps one of
// these and stores it in its stream (version 4).
class RawlessReadingsState
{
public:
	RawlessReadingsState() : m_embodyCurrentMatrix(true) {}
	bool ComposeOntoRawless() const { return m_embodyCurrentMatrix; }
	// A Bodner correction was installed: its single matrix is identity and its
	// walk skipped every raw-less reading.
	void BodnerInstalled() { m_embodyCurrentMatrix = false; }
	// The single matrix was replaced and no reading was recomputed.
	void MatrixSetWithoutWalk() { m_embodyCurrentMatrix = false; }
	// A single-matrix recalibration walked the readings with ComposeOntoRawless()
	// and skipped nSkipped raw-less ones. Composed readings embody the new matrix
	// and skipped ones still do not, so the answer stands - unless the walk found
	// none to skip: then nothing is stranded and every reading embodies the new
	// matrix. A Bodner walk never re-arms it (BodnerInstalled): a reading edited
	// under Bodner embodies the sub-gamut correction, not the identity matrix.
	void SingleMatrixWalked(int nSkipped) { if ( nSkipped == 0 ) m_embodyCurrentMatrix = true; }
	bool Get() const { return m_embodyCurrentMatrix; }
	void Set(bool b) { m_embodyCurrentMatrix = b; }
private:
	bool m_embodyCurrentMatrix;
};

// Builds the three Bodner sub-gamut matrices (index 0=rgw, 1=gbw, 2=rbw) from the
// four measured/reference primaries (index 0=R,1=G,2=B,3=W). outRawMatrix[k] holds
// the raw (uncorrected) primary matrix for sub-gamut k, used both to build
// outCalMatrix[k] = references*inverse(raw) and to reselect the sub-gamut for a
// future raw reading (see SelectAndApplyBodnerMatrix). Throws std::logic_error if
// any sub-gamut's raw matrix is singular.
extern void ComputeBodnerThreeMatrices(const ColorXYZ measuresRGBW[4], const ColorXYZ referencesRGBW[4], Matrix outRawMatrix[3], Matrix outCalMatrix[3]);

// Picks which of the three sub-gamut matrices applies to a raw XYZ reading (via the
// non-negative "drive value" test from the paper) and returns the corrected XYZ.
extern ColorXYZ SelectAndApplyBodnerMatrix(const ColorXYZ& rawXYZ, const Matrix rawMatrix[3], const Matrix calMatrix[3]);
// Precomputed-inverse variant for hot paths (per-reading / per-sweep): rawInv[k]
// and rawInvertible[k] must describe the corresponding raw sub-gamut matrix.
extern ColorXYZ SelectAndApplyBodnerMatrixInv(const ColorXYZ& rawXYZ, const Matrix rawInv[3], const bool rawInvertible[3], const Matrix calMatrix[3]);
extern int PiPercentToCode ( double percent, bool is16_235, int bits );
// Range a ColourSpace patch-list CSV can declare in its header (import-range guard).
// CSV_RANGE_NONE = no header / legacy default. The loader (ReadCsvPatchRows) reports the
// declared range via its rangeOut - the guard reads it from there so they can't diverge.
enum { CSV_RANGE_NONE = -1, CSV_RANGE_FULL = 0, CSV_RANGE_LEGAL = 1, CSV_RANGE_EXTENDED = 2 };
// Shared patch-list parsing (one parser: ReadColorsFromCsv percentages + GetCColors codes/names).
#include <vector>
#include <string>
// role is a CColorProvenanceRole; a CSV declares none, so the parser leaves -1.
struct CsvPatchRow { int patch, r, g, b; std::string name; int role = -1; };
extern int ReadCsvPatchRows ( CString csvPath, std::vector<CsvPatchRow> & rows, int maxRows, int * bitsOut, int * rangeOut );
// Resolve a CC mode to its patch-list file (predefined CSV or USER) + HDR-recalc flag; false for inline-array modes.
extern bool ResolveCCSetPath ( int aCCMode, CString & outPath, bool * recalcOut );
// One source code -> HCFR stimulus percentage for a given bit depth + CSV range (range-aware target readback).
extern double CsvCodeToPercent ( double code, int bits, int range );
// Exact inverse (round-to-nearest, clamped to the grid): stimulus percentage -> source code.
// Used to reconstruct definition codes from stored stimuli during legacy .chc migration.
extern int CsvPercentToCode ( double percent, int bits, int range );
// Two definitions drive the same codes when they have the same row count and the
// same r/g/b row by row; patch numbers and names are labels and do not count.
extern bool CsvPatchCodesMatch ( const std::vector<CsvPatchRow> & a, const std::vector<CsvPatchRow> & b );
// The display name of a patch list: the file's base name with no directory and
// with a trailing .csv, in any case, removed.
extern std::string CsvSetNameFromPath ( const char * path );
// Active user patch set override (multi-set picker): the app installs the active set's
// stored definition here so the USER drive path (GenerateCC24Colors) is self-contained -
// no file read, and the source CSV may have changed or be gone. Copied, not referenced.
extern void SetUserPatchOverride ( const std::vector<CsvPatchRow> & rows, int bits, int range );
extern void ClearUserPatchOverride ();
extern int PiBackground8ToCode ( double v255, bool is16_235, int bits );
extern double SnapToVideoGrid ( double v, bool b10bit, bool is16_235 = true );
extern double HLG_SignalToScene ( double v );
// The offset a near-chart point's level carries, as the reference curve beside
// it: the document's fitted ComputeGammaAndOffset offset when the mode
// (GammaOffsetType, 99 under the sRGB standard) is below 4, else 0.
double PointGammaEffectiveOffset ( int gammaOffsetType, bool sRGBStandard, double fittedOffset );
// A point's level for the log charts: ( prop + offset ) / ( 1 + offset ).
double PointGammaLevel ( double prop, double offset );
// is16_235 is REQUIRED (no default): it selects the 219/255/876/1023 code grid
// exactly as SnapToVideoGrid does, and a default would silently leave any
// un-updated call site quantizing a full-range wire against the limited grid.
double ArrayIndexToGrayLevel ( int nCol, int nSize, bool m_bUseRoundDown, bool m_b10bit, bool is16_235 );
double GrayLevelToGrayProp ( double Level, bool m_bUseRoundDown, bool m_b10bit, bool is16_235 );

// ---- Where an existing gray reading sits on a gray-level axis (Color.cpp) ----
// TRUE, with *pProp the level as a 0..1 proportion of the signal range, only
// when the reading's wire stamp (WireSetup.h) says HCFR put exactly this gray
// on the wire: realDrive, R = G = B, and equal codes when codes are known.
// FALSE for every other reading - no stamp (older files), realDrive false
// (placeholders, manual and external generators), unequal channels.
bool ReadingWireGrayProp ( const CColor & aReading, double * pProp );
// The reading's level as a 0..1 proportion: its stamped drive, else
// GrayLevelToGrayProp ( slotLevel, ... ) on the current grid.
double ReadingGrayProp ( const CColor & aReading, double slotLevel, bool bUseRoundDown, bool b10bit, bool is16_235 );
// The white a near-chart point is divided by, in the point's quantity: lux
// when the point is in lux (colorSpace 2, or 1 in luminance curve mode 1 with a
// point carrying lux) - the white must carry lux then - else the white's Y
// (colorSpace 0, an R, G or B trace, always: the luma normalization). The gray
// scale's top when usable (valid, that value finite and > 0), else near white's
// top, else 0: no white, no point. *pFromGrayTop, when given, says whether the
// gray top was taken.
double NearChartWhite ( int colorSpace, int luminanceCurveMode, bool pointHasLux, const CColor & grayTop, const CColor & nearWhiteTop, bool * pFromGrayTop = NULL );

// The three gray series CMeasure holds, each pairing slot i with reading i.
enum GraySeries
{
	GRAY_SERIES_GRAYSCALE  = 0,		// GetGray, at GetGrayPercent
	GRAY_SERIES_NEAR_BLACK = 1,		// GetNearBlack, 1% steps (2% in PQ)
	GRAY_SERIES_NEAR_WHITE = 2		// GetNearWhite, 1% steps up to the clip column
};

// One gray series as CMeasure holds it (CMeasure::GetGraySeriesView), and the
// CURRENT grid. Pointers are borrowed: build, use, discard.
struct GraySeriesView
{
	GraySeries		series;
	const CColor *	readings;			// readings[i] is the reading of slot i
	int				size;				// number of slots
	const double *	nominalLevels;		// grayscale: nominal % per slot, or NULL
	int				nominalCount;		// used only when it equals size
	int				nearWhiteClipCol;	// near white: the clip column (101 = none)
	bool			pqNearBlack;		// near black: PQ, 2% steps
	bool			bUseRoundDown;
	bool			b10bit;
	bool			is16_235;
};

// Slot i of the grayscale on the current grid: CMeasure::GetGrayPercent.
double GrayScaleSlotLevel ( int index, int size, const double * nominalLevels, int nominalCount, bool bUseRoundDown, bool b10bit, bool is16_235 );
// Slot i of the series on the current grid, as a percentage.
double GraySeriesSlotLevel ( const GraySeriesView & v, int index );
// Reading i of the series: ReadingWireGrayProp ( readings[index] ), FALSE
// outside the series.
bool GraySeriesDriveProp ( const GraySeriesView & v, int index, double * pProp );
// Reading i's level: its stamped drive, else slot i on the current grid.
double GraySeriesLevel ( const GraySeriesView & v, int index );		// percent
double GraySeriesProp ( const GraySeriesView & v, int index );		// 0..1
// Slot i's level as a 0..1 proportion, quantized ONCE from the level the sweep
// drives: GrayLevelToGrayProp ( i, or 2i in PQ ) for near black, ( clip - size
// + i ) for near white, the nominal level or the even ramp (floor(v)/grid under
// round-down) for the gray scale. GraySeriesProp's fallback for a reading
// without a stamped drive.
double GraySeriesSlotProp ( const GraySeriesView & v, int index );
double getL_EOTF ( double x, CColor White, CColor Black, double g_rel, double split, int mode, double m_DiffuseL = 94.37844, double m_MasterMinL = 0.0, double m_MasterMaxL = 4000.0, double m_TargetMinL = 0.00, double m_TargetMaxL = 700.0, bool ToneMap = FALSE, bool cBT2390 = FALSE, double m_TargetSysGamma = 1.2, double b_fact = 1.0, double E2_fact = 0.0, double E2_fact1 = 1.1 );

#endif // !defined(COLOR_H_INCLUDED_)
