/////////////////////////////////////////////////////////////////////////////
// AccuracyTest.cpp: headless "/accuracytest" accuracy-matrix self-test.
//
//   ColorHCFR.exe /accuracytest [report.txt]
//
// WHAT IT VERIFIES
// ----------------
// For every meaningful combination of bit depth/range grid, color space,
// transfer function, white point and window intensity, this harness checks
// the app-level invariant  reference == wire == sensor-model  (dE ~ 0):
// each patch family's wire codes (the exact percent triplets the patch
// generators emit) are pushed through CSimulatedSensor::MeasureColor with
// error injection off, and the result is compared against the corresponding
// CMeasure reference (GetRefPrimary/GetRefSecondary/GetRefSat/GetRefCC24Sat
// or the grayscale EOTF target) using the SAME normalization chain the
// measures grid uses (CMainView::UpdateGrid + GetItemText: HDR 105.95640 /
// GetHDRRefScale rescales, tmWhite RefWhite normalization, the
// YWhite * 94.37844 / tmWhite rescale, per-mode YWhite source).
//
// A perfectly modeled configuration reads dE ~ 0 for every patch, including
// patches clipped above m_TargetMaxL in PQ (the reference models the same
// per-channel clip). Any dE above the epsilon means the reference no longer
// models the wire for that combination - exactly the class of regression a
// color-math change can introduce.
//
// RELATION TO tests\ColorMathTest
// -------------------------------
// ColorMathTest.exe verify   covers the pure libHCFR layer (patch generators,
// SnapToVideoGrid, EOTF round trips) against golden files - it has no access
// to the app-coupled reference layer. THIS harness covers that layer:
// CMeasure's GetRef* functions are coupled to GetConfig() and to measured
// state (gray array top, OnOffBlack, PrimeWhite), so they can only be
// exercised inside the app. Run both after any color-math change.
//
// DESIGN NOTES
// ------------
// * No windows, no generators, no real measure loops: patches are
//   synthesized with the same construction code the measure loops use
//   (GetGrayPercent ramp, MeasurePrimaries' GenColors incl. the UHDTV3/4
//   ContainerPrimaryLinear chain, GenerateSaturationColors,
//   GenerateCC24Colors).
// * Measured-state bootstrap mirrors real usage ORDER: grayscale first
//   (OnOffWhite/OnOffBlack come from the ramp ends, and HLG/BT.1886
//   reference decodes use the measured gray top as White), then primaries
//   (PrimeWhite), then saturations/CC.
// * Window Intensity is deliberately NOT modeled by the references (it dims
//   the measured white anchor equally and cancels in the white-relative dE).
//   The 90% combos test that CANCELLATION: patches AND the white normalizer
//   are dimmed before the sensor, mirroring GDIGenerator's dimming of every
//   MT_PRIMARY/MT_SECONDARY/MT_SAT_* patch (grayscale MT_IRE and CC patches
//   are not dimmed). The cancellation is exact only for a pure power law;
//   quantization and non-power EOTFs (BT.1886, L*, sRGB) leave a small
//   residual, so those combos get a looser, documented epsilon.
// * The user's configuration is never touched: all profile reads/writes are
//   redirected to a scratch ini in %TEMP%, every relevant config member is
//   set explicitly per combo, settings are never saved, and the process
//   exits via ExitProcess before any teardown could write.
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "Measure.h"
#include "SimulatedSensor.h"
#include "AccuracyTest.h"

#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <float.h>

#ifndef ATTACH_PARENT_PROCESS
#define ATTACH_PARENT_PROCESS ((DWORD)-1)
#endif

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

namespace
{

/////////////////////////////////////////////////////////////////////////////
// Matrix dimensions

struct SpaceDef  { ColorStandard cs; const char * name; };
struct EotfDef   { int mode; BOOL toneMap; const char * name; };
struct WhiteDef  { WhiteTarget wt; const char * name; };
struct GridDef   { bool b10; bool lim; const char * name; };

static const SpaceDef kSpaces[] =
{
	{ HDTV,   "HDTV"   },	// Rec.709
	{ sRGB,   "sRGB"   },
	{ UHDTV,  "UHDTV"  },	// DCI-P3
	{ UHDTV2, "UHDTV2" },	// BT.2020
	{ UHDTV3, "UHDTV3" },	// P3 in BT.2020
	{ UHDTV4, "UHDTV4" },	// Rec.709 in BT.2020
	{ HDTVa,  "HDTVa"  },	// Rec.709 75%
	{ HDTVb,  "HDTVb"  },	// Rec.709 plasma
	// CC6 (=11) is marked unused in the ColorStandard enum and is not
	// selectable in the UI; not part of the matrix.
};

// UI-selectable transfer functions (ReferencesPropPage kComboToType, minus
// mode 1 "black compensation" which only reshapes the SDR gray target).
// sRGB-the-EOTF is exercised through the sRGB color space (every consumer
// forces mode 99 when m_colorStandard == sRGB), so the sRGB space runs a
// single EOTF entry.
static const EotfDef kEotfs[] =
{
	{ 0, FALSE, "Power2.2"  },
	{ 4, FALSE, "BT.1886"   },
	{ 6, FALSE, "L-star"    },
	{ 5, FALSE, "PQ"        },
	{ 5, TRUE,  "PQ+BT2390" },
	{ 7, FALSE, "HLG"       },
};

// D65 plus two custom whites that were never hand-tested: DCI white and a
// manual xy point. The Container*Reference functions carry the active white,
// so every reference must follow it.
static const double kManualWhiteX = 0.3067;
static const double kManualWhiteY = 0.3180;
static const WhiteDef kWhites[] =
{
	{ D65,   "D65"    },
	{ DCI,   "DCI"    },
	{ DCUST, "xyCust" },	// manual xy 0.3067, 0.3180
};

static const GridDef kGrids[] =
{
	{ false, true,  "8b-lim"  },	// 219 codes
	{ false, false, "8b-full" },	// 255
	{ true,  true,  "10b-lim" },	// 876
	{ true,  false, "10b-full"},	// 1023
};

enum Family { FAM_GRAY = 0, FAM_PRIM, FAM_SAT100, FAM_SAT75, FAM_CC_GCD, FAM_CC_AXIS, FAM_COUNT };
static const char * kFamilyName[FAM_COUNT] = { "gray", "prim", "sat100", "sat75", "ccGCD", "ccAXIS" };

struct Combo
{
	ColorStandard	std;
	int				eotf;		// m_GammaOffsetType for the combo
	BOOL			toneMap;
	WhiteTarget		white;
	bool			b10;
	bool			lim;
	int				gridIdx;	// index into kGrids
	double			intensity;	// 1.0 or 0.9 (GDI window Intensity fraction)
	const char *	eotfName;
	const char *	whiteName;
	const char *	gridName;
	const char *	stdName;
};

/////////////////////////////////////////////////////////////////////////////
// Known failures
//
// Combinations where the reference is KNOWN not to model the wire yet.
// Each entry must state the code-level reason. Matching failures are
// reported but do not fail the run; a matching combo that PASSES is flagged
// as UNEXPECTED-PASS (the entry is stale - remove it).

struct KnownFail
{
	int				std;		// ColorStandard, or -1 = any
	int				white;		// WhiteTarget, or -1 = any / 999 = any custom (non-D65)
	int				eotf;		// -1 = any / 57 = HDR (5 or 7)
	int				family;		// Family, or -1 = any
	int				grids;		// bitmask over kGrids indices (0xF = any)
	const char *	reason;
};

#define GRID_ANY   0xF
#define GRID_8LIM  0x1	// kGrids[0]
#define GRID_FULL  0xA	// kGrids[1] (8b-full) | kGrids[3] (10b-full)

static const KnownFail kKnownFails[] =
{
	// GetRefSat's UHDTV3/UHDTV4 sweep endpoints come from hardcoded xy tables
	// (p3Ref/p3sRef/rRef/rsRef, Measure.cpp ~6955-6977). The secondary
	// entries are white-point mixtures evaluated AT D65, so under any custom
	// white the reference endpoints no longer match the wire patches built
	// from ContainerPrimaryLinear (which follows the active white).
	// GetRefPrimary/GetRefSecondary for these pseudo-spaces route through
	// GetRefSat(i, 1.0), so the primaries family inherits the same skew.
	{ UHDTV3, 999, -1, FAM_PRIM,   GRID_ANY, "hardcoded D65 endpoint xy tables in GetRefSat (p3Ref/p3sRef)" },
	{ UHDTV3, 999, -1, FAM_SAT100, GRID_ANY, "hardcoded D65 endpoint xy tables in GetRefSat (p3Ref/p3sRef)" },
	{ UHDTV3, 999, -1, FAM_SAT75,  GRID_ANY, "hardcoded D65 endpoint xy tables in GetRefSat (p3Ref/p3sRef)" },
	{ UHDTV4, 999, -1, FAM_PRIM,   GRID_ANY, "hardcoded D65 endpoint xy tables in GetRefSat (rRef/rsRef)" },
	{ UHDTV4, 999, -1, FAM_SAT100, GRID_ANY, "hardcoded D65 endpoint xy tables in GetRefSat (rRef/rsRef)" },
	{ UHDTV4, 999, -1, FAM_SAT75,  GRID_ANY, "hardcoded D65 endpoint xy tables in GetRefSat (rRef/rsRef)" },
	// HDTVa/HDTVb under custom whites: the wire tables, the pRef/sRef
	// reference endpoints, the simulated sensor's decode space and the dE
	// space are all fixed Rec.709/D65 constructions, while the gray/sat
	// reference targets follow the active custom white - custom whites are
	// outside the special modes' model by design (CC passes because both
	// sides share the same decode chain).
	{ HDTVa, 999, -1, FAM_GRAY,   GRID_ANY, "special modes are fixed Rec.709/D65: gray targets follow the custom white, the wire cannot" },
	{ HDTVb, 999, -1, FAM_GRAY,   GRID_ANY, "special modes are fixed Rec.709/D65: gray targets follow the custom white, the wire cannot" },
	{ HDTVa, 999, -1, FAM_PRIM,   GRID_ANY, "75%-mode wire tables and pRef/sRef references are fixed Rec.709/D65 constructions" },
	{ HDTVa, 999, -1, FAM_SAT100, GRID_ANY, "75%-mode wire tables and pRef/sRef references are fixed Rec.709/D65 constructions" },
	{ HDTVa, 999, -1, FAM_SAT75,  GRID_ANY, "75%-mode wire tables and pRef/sRef references are fixed Rec.709/D65 constructions" },
	{ HDTVb, 999, -1, FAM_PRIM,   GRID_ANY, "plasma-mode wire tables and pRef/sRef references are fixed Rec.709/D65 constructions" },
	{ HDTVb, 999, -1, FAM_SAT100, GRID_ANY, "plasma-mode wire tables and pRef/sRef references are fixed Rec.709/D65 constructions" },
	{ HDTVb, 999, -1, FAM_SAT75,  GRID_ANY, "plasma-mode wire tables and pRef/sRef references are fixed Rec.709/D65 constructions" },
	// HDTVa/b primaries under PQ/HLG: WireModeledPrimaryReference
	// deliberately returns the ANALOG color for modes 5/7 (legacy behavior,
	// see its header comment) while the wire re-encodes the 75% tables with
	// the active EOTF - the two conventions are far apart (dE ~70 on blue).
	{ HDTVa, -1, 57, FAM_PRIM, GRID_ANY, "legacy analog primaries reference under HDR (WireModeledPrimaryReference modes 5/7 early-out)" },
	{ HDTVb, -1, 57, FAM_PRIM, GRID_ANY, "legacy analog primaries reference under HDR (WireModeledPrimaryReference modes 5/7 early-out)" },
	// HDTVa/b PQ reduced-stim 100%-saturation point, 8-bit limited grid
	// only: the PQ 50% anchor is exactly code 110/219, so 0.75 stim lands
	// the encoded signal on an exact half-code (82.5). The generator and
	// the reference reach that value through different matrix round trips
	// whose sub-1e-9 dust falls on opposite sides of the tie -> a one-code
	// split on two channels (constant dE 0.74 at yellow). No other
	// grid/level combination lands on a half-code.
	{ HDTVa, -1, 5, FAM_SAT75, GRID_8LIM, "PQ 50% anchor (code 110/219) x 0.75 stim = exact half-code tie; generator/reference dust splits it" },
	{ HDTVb, -1, 5, FAM_SAT75, GRID_8LIM, "PQ 50% anchor (code 110/219) x 0.75 stim = exact half-code tie; generator/reference dust splits it" },
	// Grayscale on the FULL-range grids: the whole gray pipeline
	// (GetGrayPercent, ArrayIndexToGrayLevel, GrayLevelToGrayProp) has no
	// range parameter - ramp codes and references live on the 219/876
	// limited grids, and on a full-range wire the re-snap to 255/1023 is
	// not modeled (<= ~0.4 dE, worst on the PQ 8-bit knee). Fixing requires
	// adding a range parameter through libHCFR, which moves ColorMathTest
	// T2/T3 goldens - out of scope for this harness.
	{ -1, -1, -1, FAM_GRAY, GRID_FULL, "gray ramp codes/references are limited-grid only (no range parameter in GetGrayPercent/GrayLevelToGrayProp)" },
};

static bool s_knownFailFired[sizeof(kKnownFails)/sizeof(kKnownFails[0])] = { false };

/////////////////////////////////////////////////////////////////////////////
// Result bookkeeping

struct FamStat
{
	double	worst;			// worst dE seen, -1 = family not run
	char	desc[160];		// description of the worst patch

	FamStat() : worst(-1.0) { desc[0] = 0; }

	void Add ( double dE, const char * fmt, ... )
	{
		if ( _isnan(dE) )
			dE = 999.0;		// NaN in a dE is always a failure
		if ( dE > worst )
		{
			worst = dE;
			va_list ap;
			va_start(ap, fmt);
			_vsnprintf(desc, sizeof(desc)-1, fmt, ap);
			desc[sizeof(desc)-1] = 0;
			va_end(ap);
		}
	}
};

static FILE *	s_fReport = NULL;
static int		s_nCombos = 0, s_nPass = 0, s_nFail = 0, s_nKnownFail = 0, s_nUnexpectedPass = 0;
static CString	s_failDetail;		// accumulated failure detail block

static void Detail ( const char * fmt, ... )
{
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	_vsnprintf(buf, sizeof(buf)-1, fmt, ap);
	buf[sizeof(buf)-1] = 0;
	va_end(ap);
	s_failDetail += buf;
}

/////////////////////////////////////////////////////////////////////////////
// Helpers

static void ScaleXYZ ( CColor & c, double s )
{
	c.SetX(c.GetX() * s);
	c.SetY(c.GetY() * s);
	c.SetZ(c.GetZ() * s);
}

// Replicates the ApplySettings reference rebuild (ColorHCFRConfig.cpp
// ~968-998) for the target combinations this harness uses.
static void RebuildColorReference ()
{
	CColorHCFRConfig * cfg = GetConfig();
	if ( GetColorApp()->m_pColorReference )
	{
		delete GetColorApp()->m_pColorReference;
		GetColorApp()->m_pColorReference = NULL;
	}
	if ( cfg->m_whiteTarget == DCUST && cfg->m_colorStandard != CUSTOM )
	{
		ColorxyY whitecolor(cfg->m_manualWhitex, cfg->m_manualWhitey);
		GetColorApp()->m_pColorReference = new CColorReference(cfg->m_colorStandard, cfg->m_whiteTarget, -1, " modified", ColorXYZ(whitecolor));
	}
	else
		GetColorApp()->m_pColorReference = new CColorReference(cfg->m_colorStandard, cfg->m_whiteTarget, -1);
}

// One synthetic sensor read. 'dim' mirrors GDIGenerator's window-Intensity
// scaling: the percent triplet is dimmed BEFORE the sensor (i.e. before the
// sensor's SnapToVideoGrid), exactly where the real wire dims it.
static CColor Meas ( CSimulatedSensor & sensor, const ColorRGBDisplay & rgb, double dim )
{
	ColorRGBDisplay p(rgb);
	p[0] *= dim;
	p[1] *= dim;
	p[2] *= dim;
	return sensor.MeasureColor(p);
}

// Replicates CMainView::GetItemText's color-mode dE (MainView.cpp
// ~2985-3047) for the non-grayscale families. displayMode: 1 = primaries,
// 5..10 = saturation sweeps, 11 = color checker. DVD (manual generator) is
// FALSE here - the harness models the GDI-family wire.
static double GridColorDE ( const CMeasure & m, const CColor & aMeasure, const CColor & aReference,
							double YWhiteIn, int displayMode, int nCol, int satsize )
{
	CColorHCFRConfig * cfg = GetConfig();
	BOOL isHDR = ( cfg->m_GammaOffsetType == 5 && (displayMode == 1 || (displayMode >= 5 && displayMode <= 11)) );
	double YWhite = YWhiteIn, RefWhite = 1.0;
	CColorReference cRef = GetColorReference();

	if ( isHDR )
	{
		CColor White = m.GetOnOffWhite();
		CColor Black = m.GetOnOffBlack();
		double tmWhite = TmDiffuseWhiteNits(White, Black);
		if ( displayMode == 1 )
		{
			if ( cRef.m_standard == UHDTV2 || cRef.m_standard == HDTV || cRef.m_standard == UHDTV || cRef.m_standard == UHDTV3 || cRef.m_standard == UHDTV4 || nCol == 7 )
				RefWhite = YWhite / tmWhite;
			else
			{
				RefWhite = YWhite / tmWhite;
				YWhite = YWhite * 94.37844 / tmWhite;
			}
		}
		else
		{
			if ( cfg->m_CCMode >= MASCIOR50 && cfg->m_CCMode <= CCMAXHDR && displayMode == 11 )
				YWhite = m.GetGray(m.GetGrayScaleSize() - 1).GetY();
			else
			{
				RefWhite = YWhite / tmWhite;
				YWhite = YWhite * 94.37844 / tmWhite;
			}
		}
	}

	CColorReference bRef = ((cRef.m_standard == UHDTV3 || cRef.m_standard == UHDTV4) ? ContainerTransportReference(cRef)
						  : (cRef.m_standard == HDTVa || cRef.m_standard == HDTVb) ? CColorReference(HDTV) : cRef);

	return aMeasure.GetDeltaE(YWhite, aReference, RefWhite, bRef, cfg->m_dE_form, false,
							  cfg->m_GammaOffsetType == 5 ? 3 : cfg->gw_Weight);
}

/////////////////////////////////////////////////////////////////////////////
// Per-combo configuration

static void ApplyComboConfig ( const Combo & c )
{
	CColorHCFRConfig * cfg = GetConfig();

	cfg->m_colorStandard = c.std;
	cfg->m_whiteTarget = c.white;
	if ( c.white == DCUST )
	{
		cfg->m_manualWhitex = kManualWhiteX;
		cfg->m_manualWhitey = kManualWhiteY;
	}
	cfg->m_CCMode = GCD;

	cfg->m_GammaOffsetType = c.eotf;
	cfg->m_useToneMap = c.toneMap;
	cfg->m_GammaRef = 2.2;
	cfg->m_GammaAvg = 2.2;
	cfg->m_useMeasuredGamma = FALSE;
	cfg->m_GammaRel = 0.0;
	cfg->m_Split = 100.0;
	cfg->m_manualGOffset = 0.099;

	cfg->m_MasterMinL = 0.0;
	cfg->m_MasterMaxL = 4000.0;
	cfg->m_DiffuseL = 94.37844;
	cfg->m_TargetSysGamma = 1.20;
	cfg->m_BT2390_BS = 1.0;
	cfg->m_BT2390_WS = 0.0;
	cfg->m_BT2390_WS1 = 25.0;

	// A realistic 700-nit display target so PQ actually clips/tone-maps the
	// upper patches (the harness asserts clipped patches STILL read dE ~ 0).
	// TargetMinL stays 0: the references target ideal black (Y=0), while the
	// sensor pins the black patch to TargetMinL - a nonzero floor is a
	// modeled DISPLAY property that would show up as a constant black-patch
	// dE, not a wire-modeling error. (With black 0, BT.1886 degenerates to a
	// pure 2.4 power - consistent on both sides.)
	BOOL bHdr = ( c.eotf == 5 || c.eotf == 7 );
	cfg->m_TargetMaxL = bHdr ? 700.0 : 120.0;
	cfg->m_TargetMinL = 0.0;
	cfg->m_bOverRideTargs = FALSE;
	cfg->m_userBlack = FALSE;

	// Grid flags: set the CACHED flags directly (RefreshUse10bitLevels would
	// re-derive them from the scratch ini's generator keys).
	cfg->m_bUseRoundDown = FALSE;
	cfg->m_bUse10bit = c.b10;
	cfg->m_bUse10bitLevels = c.b10;
	cfg->m_bRGB16_235 = c.lim;

	// dE settings: CIE2000 with the gamma-predicted gray luminance target
	// (m_dE_gray = 1) so grayscale luminance errors are visible; the default
	// dE_form 5 / dE_gray 2 substitute the measured luminance into the gray
	// reference, which would blind this test to EOTF regressions.
	cfg->m_dE_form = 3;
	cfg->m_dE_gray = 1;
	cfg->gw_Weight = 0;
	cfg->m_bHDR100 = FALSE;

	RebuildColorReference();
}

/////////////////////////////////////////////////////////////////////////////
// Family runners. Each mirrors the corresponding measure-loop patch
// construction and the UpdateGrid reference/YWhite conventions.

static const int kGraySize = 11;
static const int kSatSize  = 5;

// Grayscale ramp; also bootstraps OnOffWhite/OnOffBlack (MeasureGrayScale
// stores the ramp ends there) and mirrors UpdateGrid's HDR target refresh.
// UpdateGrid ~3840: refresh the HDR targets from the measured on/off pair.
static void RefreshHdrTargets ( CMeasure & m )
{
	CColorHCFRConfig * cfg = GetConfig();
	CColor White = m.GetOnOffWhite();
	CColor Black = m.GetOnOffBlack();
	if ( !cfg->m_bOverRideTargs && Black.isValid() && White.isValid() && (cfg->m_GammaOffsetType == 5 || cfg->m_GammaOffsetType == 7) )
	{
		if ( Black.GetY() < White.GetY() )
			cfg->m_TargetMinL = Black.GetY() > 1e-5 ? Black.GetY() : 0.0;
		else
			cfg->m_TargetMinL = 0.0;
		if ( White.GetY() > 0 )
			cfg->m_TargetMaxL = White.GetY();
		cfg->m_TargetSysGamma = floor((1.2 + 0.42 * log10(cfg->m_TargetMaxL / 1000.)) * 100. + 0.5) / 100.0;
	}
}

static void RunGray ( const Combo & c, CMeasure & m, CSimulatedSensor & sensor, FamStat & stat )
{
	CColorHCFRConfig * cfg = GetConfig();

	// Bootstrap pass: measure the on/off pair and refresh the HDR targets
	// BEFORE the ramp. In the real app UpdateGrid has already converged
	// m_TargetMinL/MaxL/m_TargetSysGamma from earlier measurements by the
	// time a sweep runs, and the sensor reads those at measure time (HLG's
	// OOTF uses TargetSysGamma) - measuring the ramp with the un-refreshed
	// system gamma would skew every HLG mid-gray by ~2 dE.
	m.SetOnOffWhite(Meas(sensor, ColorRGBDisplay(100, 100, 100), 1.0));
	m.SetOnOffBlack(Meas(sensor, ColorRGBDisplay(0, 0, 0), 1.0));
	RefreshHdrTargets(m);

	m.SetGrayScaleSize(kGraySize);
	double lv[kGraySize];
	int i;
	for ( i = 0 ; i < kGraySize ; i ++ )
		lv[i] = i * 100.0 / (kGraySize - 1);
	m.SetGrayScaleLevels(lv, kGraySize);

	for ( i = 0 ; i < kGraySize ; i ++ )
	{
		double x = m.GetGrayPercent(i, cfg->m_bUseRoundDown != FALSE, cfg->GetUse10bitLevels() != FALSE);
		// MT_IRE patches are never Intensity-dimmed
		m.SetGray(i, Meas(sensor, ColorRGBDisplay(x, x, x), 1.0));
	}
	m.SetOnOffBlack(m.GetGray(0));					// MeasureGrayScale: measuredColor[0]
	m.SetOnOffWhite(m.GetGray(kGraySize - 1));		// MeasureGrayScale: measuredColor[size-1]
	RefreshHdrTargets(m);							// converges (same values as bootstrap)

	CColor White = m.GetOnOffWhite();
	CColor Black = m.GetOnOffBlack();

	// dE per UpdateGrid case 0 + GetItemText grayscale branch. The grid
	// leaves the first column (black) without a dE; skip it the same way.
	double YWhite = m.GetGray(kGraySize - 1).GetY();
	for ( i = 1 ; i < kGraySize ; i ++ )
	{
		CColor aColor = m.GetGray(i);
		double x = m.GetGrayPercent(i, cfg->m_bUseRoundDown != FALSE, cfg->GetUse10bitLevels() != FALSE);
		int mode = cfg->m_GammaOffsetType;
		if ( cfg->m_colorStandard == sRGB ) mode = 99;
		double valy;
		if ( mode >= 4 )
		{
			double valx = GrayLevelToGrayProp(x, cfg->m_bUseRoundDown != FALSE, cfg->GetUse10bitLevels() != FALSE);
			valy = getL_EOTF(valx, White, Black, cfg->m_GammaRel, cfg->m_Split, mode, cfg->m_DiffuseL, cfg->m_MasterMinL, cfg->m_MasterMaxL, cfg->m_TargetMinL, cfg->m_TargetMaxL, cfg->m_useToneMap, FALSE, cfg->m_TargetSysGamma, cfg->m_BT2390_BS, cfg->m_BT2390_WS, cfg->m_BT2390_WS1);
			valy = min(valy, cfg->m_TargetMaxL);
		}
		else
		{
			double valx = GrayLevelToGrayProp(x, cfg->m_bUseRoundDown != FALSE, cfg->GetUse10bitLevels() != FALSE);	// Offset = 0
			valy = pow(valx, cfg->m_GammaRef);
		}

		ColorxyY tmpColor(GetColorReference().GetWhite());
		tmpColor[2] = ( mode == 5 ) ? valy * 100. / YWhite : valy;
		CColor refColor;
		refColor.SetxyYValue(tmpColor);

		double dE = aColor.GetDeltaE(YWhite, refColor, 1.0, GetColorReference(), cfg->m_dE_form, true,
									 cfg->m_GammaOffsetType == 5 ? 3 : cfg->gw_Weight);
		stat.Add(dE, "gray %d%% (code %.4f%%)", i * 10, x);
	}
}

// Primaries + secondaries + white/black rows; mirrors MeasurePrimaries'
// GenColors construction (Measure.cpp ~1693-1785) incl. the UHDTV3/4
// ContainerPrimaryLinear chain and the HDTVa/b HDR re-encode.
static void RunPrimaries ( const Combo & c, CMeasure & m, CSimulatedSensor & sensor, FamStat & stat )
{
	CColorHCFRConfig * cfg = GetConfig();
	int mode = cfg->m_GammaOffsetType;
	BOOL isSpecial = FALSE;

	double primaryIRELevel = 100.0;
	if ( mode == 5 )
		primaryIRELevel = 50.22831;		// non-DVD wire (manual-generator Mascior 50.00 not modeled here)

	ColorRGBDisplay GenColors[8] =
	{
		ColorRGBDisplay(primaryIRELevel,0,0),
		ColorRGBDisplay(0,primaryIRELevel,0),
		ColorRGBDisplay(0,0,primaryIRELevel),
		ColorRGBDisplay(primaryIRELevel,primaryIRELevel,0),
		ColorRGBDisplay(0,primaryIRELevel,primaryIRELevel),
		ColorRGBDisplay(primaryIRELevel,0,primaryIRELevel),
		ColorRGBDisplay(primaryIRELevel,primaryIRELevel,primaryIRELevel),
		ColorRGBDisplay(0,0,0)
	};
	if ( GetColorReference().m_standard == HDTVb )
	{
		GenColors[0] = ColorRGBDisplay(79.9087,10.0457,10.0457);
		GenColors[1] = ColorRGBDisplay(30.137,79.9087,30.137);
		GenColors[2] = ColorRGBDisplay(50.2283,50.2283,79.9087);
		GenColors[3] = ColorRGBDisplay(79.9087,79.9087,10.0457);
		GenColors[4] = ColorRGBDisplay(10.0457,79.9087,79.9087);
		GenColors[5] = ColorRGBDisplay(79.9087,10.0457,79.9087);
		isSpecial = TRUE;
	}
	else if ( GetColorReference().m_standard == HDTVa )
	{
		GenColors[0] = ColorRGBDisplay(68.04,20.09,20.09);
		GenColors[1] = ColorRGBDisplay(27.85,73.06,27.85);
		GenColors[2] = ColorRGBDisplay(19.18,19.18,50.22);
		GenColors[3] = ColorRGBDisplay(73.9726,73.9726,33.3333);
		GenColors[4] = ColorRGBDisplay(36.07,73.06,73.06);
		GenColors[5] = ColorRGBDisplay(64.3836,29.2237,64.3836);
		GenColors[6] = ColorRGBDisplay(75.0,75.0,75.0);
		isSpecial = TRUE;
	}
	else if ( GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4 )
	{
		if ( !(mode == 5 || mode == 7) )
			isSpecial = TRUE;
		GenColors[6] = ColorRGBDisplay(primaryIRELevel,primaryIRELevel,primaryIRELevel);
		GenColors[7] = ColorRGBDisplay(0,0,0);
	}

	if ( GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4 )
	{
		CColor tmW = m.GetGray(m.GetGrayScaleSize() - 1);
		CColor tmB = m.GetOnOffBlack();
		for ( int ci = 0 ; ci < 6 ; ci ++ )
		{
			ColorRGB clin = ContainerPrimaryLinear(GetColorReference(), ci);
			for ( int ck = 0 ; ck < 3 ; ck ++ )
			{
				double cv = clin[ck];
				if ( mode == 5 )
					cv = (cv <= 0.0) ? 0.0 : getL_EOTF(cv / 105.95640, tmW, tmB, cfg->m_GammaRel, cfg->m_Split, -5);
				else if ( mode == 7 )
					cv = (cv <= 0.0) ? 0.0 : getL_EOTF(cv, tmW, tmB, cfg->m_GammaRel, cfg->m_Split, -7);
				else
					cv = (cv <= 0.0 || cv >= 1.0) ? min(max(cv, 0.0), 1.0) : pow(cv, 1.0 / 2.22);
				GenColors[ci][ck] = min(max(cv, 0.0), 1.0) * 100.0;
			}
		}
	}

	if ( (mode == 5 || mode == 7) && isSpecial )
	{
		for ( int i = 0 ; i <= 5 ; i ++ )
		{
			GenColors[i][0] = 100. * getL_EOTF(pow(GenColors[i][0] / 100., 2.22), noDataColor, noDataColor, 0, 0, -1 * mode);
			GenColors[i][1] = 100. * getL_EOTF(pow(GenColors[i][1] / 100., 2.22), noDataColor, noDataColor, 0, 0, -1 * mode);
			GenColors[i][2] = 100. * getL_EOTF(pow(GenColors[i][2] / 100., 2.22), noDataColor, noDataColor, 0, 0, -1 * mode);
		}
	}

	// MT_PRIMARY/MT_SECONDARY patches (incl. the white anchor) are dimmed by
	// the window Intensity on the real wire.
	int i;
	for ( i = 0 ; i < 3 ; i ++ )
		m.SetPrimary(i, Meas(sensor, GenColors[i], c.intensity));
	for ( i = 3 ; i < 6 ; i ++ )
		m.SetSecondary(i - 3, Meas(sensor, GenColors[i], c.intensity));
	m.SetPrimeWhite(Meas(sensor, GenColors[6], c.intensity));

	// dE per UpdateGrid case 1 (j = 0..6; the black row has no reference)
	for ( int j = 0 ; j < 7 ; j ++ )
	{
		CColor aColor, refColor;
		double YWhite = m.GetPrimeWhite().GetY();
		if ( j < 3 )
		{
			aColor = m.GetPrimary(j);
			refColor = m.GetRefPrimary(j);
		}
		else if ( j < 6 )
		{
			aColor = m.GetSecondary(j - 3);
			refColor = m.GetRefSecondary(j - 3);
		}
		else
		{
			refColor = GetColorReference().GetWhite();
			aColor = m.GetPrimeWhite();
			YWhite = aColor.GetY();
		}
		// No 105.95640 rescale here: UpdateGrid's HDR block (~4219) only
		// covers displayMode 5..11 - primaries references are already
		// self-scaled (GetRefPrimary applies GetHDRRefScale internally for
		// the pseudo-spaces).

		static const char * names[7] = { "red", "green", "blue", "yellow", "cyan", "magenta", "white" };
		double dE = GridColorDE(m, aColor, refColor, YWhite, 1, j + 1, kSatSize);
		stat.Add(dE, "%s", names[j]);
	}
}

// Six saturation sweeps at one stimulus level.
static void RunSats ( const Combo & c, CMeasure & m, CSimulatedSensor & sensor, double stim, FamStat & stat )
{
	CColorHCFRConfig * cfg = GetConfig();
	bool special = ( cfg->m_colorStandard == HDTVa || cfg->m_colorStandard == HDTVb );
	static const char * names[6] = { "red", "green", "blue", "yellow", "cyan", "magenta" };
	static const bool flags[6][3] =
	{
		{ true,false,false }, { false,true,false }, { false,false,true },
		{ true,true,false }, { false,true,true }, { true,false,true },
	};

	m.SetSaturationSize(kSatSize);
	for ( int s = 0 ; s < 6 ; s ++ )
	{
		ColorRGBDisplay GenColors[kSatSize];
		GenerateSaturationColors(GetColorReference(), GenColors, kSatSize, flags[s][0], flags[s][1], flags[s][2],
								 cfg->m_GammaOffsetType, stim, cfg->GetUse10bitLevels() != FALSE, cfg->GetRGB16_235() != FALSE);
		for ( int j = 0 ; j < kSatSize ; j ++ )
		{
			// MT_SAT_* patches are Intensity-dimmed on the real wire
			CColor aColor = Meas(sensor, GenColors[j], c.intensity);
			CColor refColor = m.GetRefSat(s, (double)j / (double)(kSatSize - 1), special, stim);
			if ( cfg->m_GammaOffsetType == 5 )
				ScaleXYZ(refColor, 105.95640);	// UpdateGrid ~4229
			double YWhite = special ? m.GetOnOffWhite().GetY() : m.GetPrimeWhite().GetY();
			double dE = GridColorDE(m, aColor, refColor, YWhite, 5 + s, j + 1, kSatSize);
			stat.Add(dE, "%s sat %d%% stim %.0f%%", names[s], j * 25, stim * 100.);
		}
	}
}

// One color-checker set (GCD 24 or AXIS 71).
static void RunCC ( const Combo & c, CMeasure & m, CSimulatedSensor & sensor, CCPatterns ccMode, int count, FamStat & stat )
{
	CColorHCFRConfig * cfg = GetConfig();
	cfg->m_CCMode = ccMode;

	ColorRGBDisplay GenColors[128];
	if ( !GenerateCC24Colors(GetColorReference(), GenColors, ccMode, cfg->m_GammaOffsetType,
							 cfg->GetUse10bitLevels() != FALSE, cfg->GetRGB16_235() != FALSE) )
	{
		stat.Add(999.0, "GenerateCC24Colors failed for mode %d", (int)ccMode);
		return;
	}

	// UpdateGrid default-case YWhite: PrimeWhite, falling back to the
	// grayscale/contrast white when the primaries run was dimmed (<90%).
	double YWhitePrime = m.GetPrimeWhite().GetY();
	double YWhiteOnOff = m.GetOnOffWhite().GetY();
	double YWhite = YWhitePrime;
	BOOL isHDR = ( cfg->m_GammaOffsetType == 5 );
	if ( m.GetOnOffWhite().isValid() && !isHDR && YWhiteOnOff > 0 && (YWhitePrime / YWhiteOnOff < 0.9) )
		YWhite = YWhiteOnOff;

	for ( int j = 0 ; j < count ; j ++ )
	{
		// CC patches are NOT in GDIGenerator's Intensity-dimmed pattern types
		CColor aColor = Meas(sensor, GenColors[j], 1.0);
		CColor refColor;
		m.GetRefCC24Sat(j, refColor);
		if ( cfg->m_GammaOffsetType == 5 )
			ScaleXYZ(refColor, (ccMode >= MASCIOR50 && ccMode <= CCMAXHDR) ? 100. : 105.95640);	// UpdateGrid ~4221-4231
		double dE = GridColorDE(m, aColor, refColor, YWhite, 11, j + 1, kSatSize);
		stat.Add(dE, "patch %d (%.2f/%.2f/%.2f%%)", j, GenColors[j][0], GenColors[j][1], GenColors[j][2]);
	}

	cfg->m_CCMode = GCD;
}

/////////////////////////////////////////////////////////////////////////////
// Tolerances and known-fail matching

static double TolFor ( const Combo & c, int fam )
{
	if ( c.intensity < 0.999 )
	{
		// Dimmed combos verify the Intensity CANCELLATION, which is exact
		// only for an ideal power law. Quantization (the dimmed code snaps
		// to a different grid point than the undimmed one) and the non-power
		// EOTFs (BT.1886's black lift, L*'s linear toe, sRGB's linear
		// segment) leave a real residual; it is bounded and level-dependent,
		// not a modeling error. Measured residual ceilings across the full
		// matrix: pure power law ~0.8 (8-bit dark saturation steps, where a
		// one-code snap difference moves chroma most; the UHDTV3/4
		// transport-space encode adds a second quantization), BT.1886/L*/
		// sRGB ~1.2 (their toes amplify the same one-code difference).
		if ( fam == FAM_PRIM || fam == FAM_SAT100 || fam == FAM_SAT75 )
			return ( c.eotf == 0 && c.std != sRGB ) ? 0.9 : 1.5;
	}
	return 0.05;
}

static int MatchKnownFail ( const Combo & c, int fam )	// index into kKnownFails, or -1
{
	for ( int i = 0 ; i < (int)(sizeof(kKnownFails)/sizeof(kKnownFails[0])) ; i ++ )
	{
		const KnownFail & k = kKnownFails[i];
		if ( k.std != -1 && k.std != (int)c.std ) continue;
		if ( k.white == 999 ) { if ( c.white == D65 ) continue; }
		else if ( k.white != -1 && k.white != (int)c.white ) continue;
		if ( k.eotf == 57 ) { if ( c.eotf != 5 && c.eotf != 7 ) continue; }
		else if ( k.eotf != -1 && k.eotf != c.eotf ) continue;
		if ( k.family != -1 && k.family != fam ) continue;
		if ( !( k.grids & (1 << c.gridIdx) ) ) continue;
		return i;
	}
	return -1;
}

/////////////////////////////////////////////////////////////////////////////
// One combo

static void RunCombo ( const Combo & c )
{
	ApplyComboConfig(c);

	CMeasure m;
	CSimulatedSensor sensor;
	sensor.m_doOffsetError = FALSE;
	sensor.m_doGainError = FALSE;
	sensor.m_doGammaError = FALSE;
	sensor.Init(FALSE);

	FamStat stats[FAM_COUNT];

	// Order matters and mirrors real usage: grayscale bootstraps
	// OnOffWhite/OnOffBlack (the HLG/BT.1886 reference decode White), then
	// primaries set PrimeWhite (the sat/CC dE normalizer).
	RunGray(c, m, sensor, stats[FAM_GRAY]);
	RunPrimaries(c, m, sensor, stats[FAM_PRIM]);
	RunSats(c, m, sensor, 1.0, stats[FAM_SAT100]);
	RunSats(c, m, sensor, 0.75, stats[FAM_SAT75]);
	RunCC(c, m, sensor, GCD, 24, stats[FAM_CC_GCD]);
	RunCC(c, m, sensor, AXIS, 71, stats[FAM_CC_AXIS]);

	// Evaluate
	char line[512];
	int n = _snprintf(line, sizeof(line)-1, "%-8s %-10s %-6s %-8s %3.0f%%  ",
					  c.stdName, c.eotfName, c.whiteName, c.gridName, c.intensity * 100.);
	BOOL bFail = FALSE, bKnown = FALSE;
	for ( int fam = 0 ; fam < FAM_COUNT ; fam ++ )
	{
		double tol = TolFor(c, fam);
		int iKnown = MatchKnownFail(c, fam);
		double w = stats[fam].worst;
		BOOL over = ( w > tol );
		n += _snprintf(line + n, sizeof(line)-1-n, "%7.3f%c", w, over ? (iKnown >= 0 ? '#' : '*') : ' ');
		if ( over )
		{
			if ( iKnown >= 0 )
			{
				bKnown = TRUE;
				s_knownFailFired[iKnown] = true;
				Detail("KNOWN-FAIL  %s %s %s %s %.0f%% [%s]: worst dE %.3f at %s\n            reason: %s\n",
					   c.stdName, c.eotfName, c.whiteName, c.gridName, c.intensity * 100.,
					   kFamilyName[fam], w, stats[fam].desc, kKnownFails[iKnown].reason);
			}
			else
			{
				bFail = TRUE;
				Detail("FAIL        %s %s %s %s %.0f%% [%s]: worst dE %.3f (tol %.2f) at %s\n",
					   c.stdName, c.eotfName, c.whiteName, c.gridName, c.intensity * 100.,
					   kFamilyName[fam], w, tol, stats[fam].desc);
			}
		}
		// An in-tolerance result on a known-fail-covered family is fine (the
		// gaps are level/grid dependent); an entry is only stale if it never
		// fires across the entire run - checked after the matrix completes.
	}

	const char * verdict = bFail ? "FAIL" : (bKnown ? "KNOWN-FAIL" : "PASS");
	fprintf(s_fReport, "%s %s\n", line, verdict);

	s_nCombos ++;
	if ( bFail )			s_nFail ++;
	else if ( bKnown )		s_nKnownFail ++;
	else					s_nPass ++;
}

} // namespace

/////////////////////////////////////////////////////////////////////////////
// Entry point

int RunAccuracyTest ( const char * pReportPath )
{
	// Show progress/summary when launched from a console
	BOOL bConsole = AttachConsole(ATTACH_PARENT_PROCESS);
	if ( bConsole )
	{
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
		printf("\n");
	}

	CColorHCFRConfig * cfg = GetConfig();

	// Redirect ALL profile traffic to a scratch ini: nothing in this run may
	// read or write the user's real configuration. (The process also exits
	// via ExitProcess, so no settings-saving teardown ever runs.)
	char tmpPath[MAX_PATH];
	GetTempPathA(MAX_PATH, tmpPath);
	_snprintf(cfg->m_iniFileName, MAX_PATH - 1, "%saccuracytest_scratch.ini", tmpPath);
	cfg->m_iniFileName[MAX_PATH - 1] = 0;
	DeleteFileA(cfg->m_iniFileName);

	// Non-manual generator: DVD-specific reference conventions stay off and
	// the wire is the GDI-family model this harness reproduces.
	cfg->SetGeneratorType(CColorHCFRConfig::enumAutomatic);

	CSimulatedSensor::s_bInstantMeasure = TRUE;

	const char * pPath = pReportPath && pReportPath[0] ? pReportPath : "accuracytest_report.txt";
	s_fReport = fopen(pPath, "w");
	if ( !s_fReport )
	{
		if ( bConsole ) printf("accuracytest: cannot open report file '%s'\n", pPath);
		return 2;
	}

	fprintf(s_fReport, "ColorHCFR /accuracytest - reference == wire == sensor-model matrix\n");
	fprintf(s_fReport, "Columns are the worst dE per patch family; '*' = over tolerance (FAIL),\n");
	fprintf(s_fReport, "'#' = over tolerance but documented KNOWN-FAIL (see detail below).\n");
	fprintf(s_fReport, "Tolerance: 0.05; Intensity-90%% combos: 0.9 (power law) / 1.5 (other EOTFs)\n");
	fprintf(s_fReport, "dE settings: dE_form=3 (CIE2000), dE_gray=1 (gamma-predicted gray target), gw_Weight=0\n\n");
	fprintf(s_fReport, "%-8s %-10s %-6s %-8s %-5s %8s %7s %7s %7s %7s %7s  %s\n",
			"space", "eotf", "white", "grid", "inten",
			"gray", "prim", "sat100", "sat75", "ccGCD", "ccAXIS", "result");

	int iSpace, iEotf, iWhite, iGrid, iInt;
	for ( iSpace = 0 ; iSpace < (int)(sizeof(kSpaces)/sizeof(kSpaces[0])) ; iSpace ++ )
	{
		const SpaceDef & sp = kSpaces[iSpace];
		int nEotfs = ( sp.cs == sRGB ) ? 1 : (int)(sizeof(kEotfs)/sizeof(kEotfs[0]));

		for ( iEotf = 0 ; iEotf < nEotfs ; iEotf ++ )
		{
			// sRGB space: every consumer forces mode 99; run it once as
			// its own transfer function.
			const EotfDef & e = kEotfs[iEotf];
			int eotf = ( sp.cs == sRGB ) ? 0 : e.mode;
			BOOL bSdr = !( eotf == 5 || eotf == 7 );

			for ( iWhite = 0 ; iWhite < (int)(sizeof(kWhites)/sizeof(kWhites[0])) ; iWhite ++ )
			{
				for ( iGrid = 0 ; iGrid < (int)(sizeof(kGrids)/sizeof(kGrids[0])) ; iGrid ++ )
				{
					// Window Intensity is SDR-only (the generator UI
					// disables it for modes 5/7). HDTVa/b are also
					// excluded: their saturation dE normalizes to the
					// UNdimmed OnOff white (UpdateGrid isSpecial branch),
					// so the cancellation the 90% combos verify does not
					// exist for them by construction (dE ~5).
					int nInt = ( bSdr && sp.cs != HDTVa && sp.cs != HDTVb ) ? 2 : 1;
					for ( iInt = 0 ; iInt < nInt ; iInt ++ )
					{
						Combo c;
						c.std = sp.cs;
						c.eotf = eotf;
						c.toneMap = ( sp.cs == sRGB ) ? FALSE : e.toneMap;
						c.white = kWhites[iWhite].wt;
						c.b10 = kGrids[iGrid].b10;
						c.lim = kGrids[iGrid].lim;
						c.gridIdx = iGrid;
						c.intensity = iInt ? 0.9 : 1.0;
						c.eotfName = ( sp.cs == sRGB ) ? "sRGB" : e.name;
						c.whiteName = kWhites[iWhite].name;
						c.gridName = kGrids[iGrid].name;
						c.stdName = sp.name;
						RunCombo(c);
					}
				}
			}
			if ( bConsole )
			{
				printf("accuracytest: %-8s %-10s done (%d combos so far, %d fail)\n",
					   sp.name, (sp.cs == sRGB) ? "sRGB" : e.name, s_nCombos, s_nFail);
				fflush(stdout);
			}
		}
	}

	// A known-fail entry that never fired anywhere in the matrix is stale.
	for ( int k = 0 ; k < (int)(sizeof(kKnownFails)/sizeof(kKnownFails[0])) ; k ++ )
	{
		if ( !s_knownFailFired[k] )
		{
			s_nUnexpectedPass ++;
			Detail("STALE KNOWN-FAIL entry %d never fired - remove it: %s\n", k, kKnownFails[k].reason);
		}
	}

	fprintf(s_fReport, "\n");
	if ( !s_failDetail.IsEmpty() )
		fprintf(s_fReport, "---- DETAIL ----\n%s\n", (LPCSTR)s_failDetail);
	fprintf(s_fReport, "SUMMARY: %d combos: %d pass, %d FAIL, %d known-fail%s\n",
			s_nCombos, s_nPass, s_nFail, s_nKnownFail,
			s_nUnexpectedPass ? " (stale known-fail entries present!)" : "");
	fclose(s_fReport);

	int rc = ( s_nFail > 0 ) ? 1 : 0;
	if ( bConsole )
	{
		printf("accuracytest: %d combos: %d pass, %d FAIL, %d known-fail -> exit %d (report: %s)\n",
			   s_nCombos, s_nPass, s_nFail, s_nKnownFail, rc, pPath);
		fflush(stdout);
	}
	return rc;
}
