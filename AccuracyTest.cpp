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
// measures grid uses (CMainView::UpdateGrid + GetItemText: the unified HDR
// GetHDRRefScale reference rescale (= 105.95640 with tone mapping off),
// tmWhite RefWhite normalization, per-mode YWhite source).
//
// A perfectly modeled configuration reads dE ~ 0 for every patch, including
// patches clipped above m_TargetMaxL in PQ (the reference models the same
// per-channel clip). Any dE above the epsilon means the reference no longer
// models the wire for that combination - exactly the class of regression a
// color-math change can introduce.
//
// ...AND WHAT THE dE ~ 0 INVARIANT CANNOT VERIFY
// ----------------------------------------------
// That invariant holds under ANY self-consistent normalization, so it cannot
// tell two CONSUMERS apart when they disagree about a convention - it only
// catches a reference that stopped modeling the wire. Every dE bug that
// reached a user during the 2026-07 HDR rescale unification was of the first
// kind and left all 708 combos green.
//
// The conv* families close that hole with a DIFFERENTIAL check: perturb the
// measurement, then require the measures grid's formula and the 3D viewer's
// formula to produce the SAME dE. convPV runs it at nominal whites, convPVw
// with the stored whites pushed off target and off each other, and convNW with
// no white measured at all. See ConvCheck and tests\ACCURACYTEST.md.
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
struct EotfDef   { int mode; BOOL toneMap; double maxL; const char * name; };	// maxL 0 = default rule
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
// maxL overrides m_TargetMaxL for the entry (0 = the default rule in
// ApplyComboConfig: 700 for HDR, 300 for tone-mapped HDR, 120 for SDR).
// Tone mapping runs at BOTH knee positions: 300 nits puts BT.2390's knee BELOW
// diffuse white (so the HDR reference-rescale / white-anchor conventions get
// real coverage - see ApplyComboConfig), while 700 nits leaves diffuse white
// untouched and moves the roll-off up among the AXIS patches that CLIP with
// tone mapping off. Without the 700 row the "patches above m_TargetMaxL still
// read dE ~ 0" invariant was only ever checked with tone mapping off.
static const EotfDef kEotfs[] =
{
	{ 0, FALSE,   0.0, "Power2.2"  },
	{ 4, FALSE,   0.0, "BT.1886"   },
	{ 6, FALSE,   0.0, "L-star"    },
	{ 5, FALSE,   0.0, "PQ"        },
	{ 5, TRUE,    0.0, "PQ+TM300"  },
	{ 5, TRUE,  700.0, "PQ+TM700"  },
	{ 7, FALSE,   0.0, "HLG"       },
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

// "quick" runs a reduced subset for fast iteration. Only the WHITE and GRID
// axes shrink - every color space, transfer function, generator and intensity
// still runs, because those are the branchy axes. The subset is chosen so that
// every kKnownFails entry still fires (so the stale-entry check, and therefore
// the exit code, keeps its meaning): DCUST covers the custom-white entries,
// 8b-lim covers the PQ half-code tie, 8b-full covers the full-range gray gap.
// What it gives up is level dependence - several modeling gaps are worst on a
// grid or white the subset drops - so it is a pre-flight, not a substitute.
static bool s_bQuick = false;
static int  s_nQuickWhites = 2;		// kWhites[0] = D65, kWhites[2] = DCUST
static const int kQuickWhiteIdx[] = { 0, 2 };
static int  s_nQuickGrids = 2;		// kGrids[0] = 8b-lim, kGrids[1] = 8b-full
static const int kQuickGridIdx[] = { 0, 1 };

static const GridDef kGrids[] =
{
	{ false, true,  "8b-lim"  },	// 219 codes
	{ false, false, "8b-full" },	// 255
	{ true,  true,  "10b-lim" },	// 876
	{ true,  false, "10b-full"},	// 1023
};

enum Family { FAM_GRAY = 0, FAM_PRIM, FAM_SAT100, FAM_SAT75, FAM_CC_GCD, FAM_CC_AXIS,
			  FAM_CONV, FAM_CONVW, FAM_CONVNW, FAM_COUNT };
static const char * kFamilyName[FAM_COUNT] =
	{ "gray", "prim", "sat100", "sat75", "ccGCD", "ccAXIS", "convPV", "convPVw", "convNW" };

// The three convention families are DIFFERENTIAL (|grid dE - viewer dE|), not
// dE ~ 0 invariants - see ConvCheck.
static bool IsConvFamily ( int fam )
{
	return ( fam == FAM_CONV || fam == FAM_CONVW || fam == FAM_CONVNW );
}

struct Combo
{
	ColorStandard	std;
	int				eotf;		// m_GammaOffsetType for the combo
	BOOL			toneMap;
	double			maxL;		// m_TargetMaxL override, 0 = default rule
	WhiteTarget		white;
	bool			b10;
	bool			lim;
	int				gridIdx;	// index into kGrids
	double			intensity;	// 1.0 or 0.9 (GDI window Intensity fraction)
	bool			dvd;		// manual generator (enumManual)
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
	int				dvd;		// -1 = any generator, 0 = GDI only, 1 = manual only
	double			ceiling;	// dE above which this is a REAL fail, not the known gap
	const char *	reason;
};

#define GRID_ANY   0xF
#define GRID_8LIM  0x1	// kGrids[0]
#define GRID_FULL  0xA	// kGrids[1] (8b-full) | kGrids[3] (10b-full)
#define GRID_LIM   0x5	// kGrids[0] (8b-lim) | kGrids[2] (10b-lim)

// The `ceiling` on each entry bounds the known gap: a matching combo is only
// downgraded to KNOWN-FAIL while its worst dE stays <= ceiling; a larger dE is
// a REAL fail (the known issue got WORSE = a regression the entry must not
// mask). Ceilings sit above the first-full-run worst per class (comments give
// the observed max) with headroom, tight enough to still catch a regression.
static const KnownFail kKnownFails[] =
{
	// GetRefSat's UHDTV3/UHDTV4 sweep endpoints come from hardcoded xy tables
	// (p3Ref/p3sRef/rRef/rsRef, Measure.cpp ~6955-6977). The secondary
	// entries are white-point mixtures evaluated AT D65, so under any custom
	// white the reference endpoints no longer match the wire patches built
	// from ContainerPrimaryLinear (which follows the active white).
	// GetRefPrimary/GetRefSecondary for these pseudo-spaces route through
	// GetRefSat(i, 1.0), so the primaries family inherits the same skew.
	// (observed worst ~8 dE)
	{ UHDTV3, 999, -1, FAM_PRIM,   GRID_ANY, -1, 15.0, "hardcoded D65 endpoint xy tables in GetRefSat (p3Ref/p3sRef)" },
	{ UHDTV3, 999, -1, FAM_SAT100, GRID_ANY, -1, 15.0, "hardcoded D65 endpoint xy tables in GetRefSat (p3Ref/p3sRef)" },
	{ UHDTV3, 999, -1, FAM_SAT75,  GRID_ANY, -1, 15.0, "hardcoded D65 endpoint xy tables in GetRefSat (p3Ref/p3sRef)" },
	{ UHDTV4, 999, -1, FAM_PRIM,   GRID_ANY, -1, 15.0, "hardcoded D65 endpoint xy tables in GetRefSat (rRef/rsRef)" },
	{ UHDTV4, 999, -1, FAM_SAT100, GRID_ANY, -1, 15.0, "hardcoded D65 endpoint xy tables in GetRefSat (rRef/rsRef)" },
	{ UHDTV4, 999, -1, FAM_SAT75,  GRID_ANY, -1, 15.0, "hardcoded D65 endpoint xy tables in GetRefSat (rRef/rsRef)" },
	// HDTVa/HDTVb under custom whites: the wire tables, the pRef/sRef
	// reference endpoints, the simulated sensor's decode space and the dE
	// space are all fixed Rec.709/D65 constructions, while the gray/sat
	// reference targets follow the active custom white - custom whites are
	// outside the special modes' model by design (CC passes because both
	// sides share the same decode chain). (observed worst: gray ~12, sat ~14)
	{ HDTVa, 999, -1, FAM_GRAY,   GRID_ANY, -1, 20.0, "special modes are fixed Rec.709/D65: gray targets follow the custom white, the wire cannot" },
	{ HDTVb, 999, -1, FAM_GRAY,   GRID_ANY, -1, 20.0, "special modes are fixed Rec.709/D65: gray targets follow the custom white, the wire cannot" },
	{ HDTVa, 999, -1, FAM_SAT100, GRID_ANY, -1, 20.0, "75%-mode wire tables and pRef/sRef references are fixed Rec.709/D65 constructions" },
	{ HDTVa, 999, -1, FAM_SAT75,  GRID_ANY, -1, 20.0, "75%-mode wire tables and pRef/sRef references are fixed Rec.709/D65 constructions" },
	{ HDTVb, 999, -1, FAM_SAT100, GRID_ANY, -1, 20.0, "plasma-mode wire tables and pRef/sRef references are fixed Rec.709/D65 constructions" },
	{ HDTVb, 999, -1, FAM_SAT75,  GRID_ANY, -1, 20.0, "plasma-mode wire tables and pRef/sRef references are fixed Rec.709/D65 constructions" },
	// HDTVa/b custom-white primaries overlap the HDR-analog gap below when
	// the EOTF is PQ/HLG, so this entry (which matches first) must clear the
	// same ~70 dE. (observed worst ~68 dE)
	{ HDTVa, 999, -1, FAM_PRIM,   GRID_ANY, -1, 90.0, "75%-mode wire tables and pRef/sRef references are fixed Rec.709/D65 constructions" },
	{ HDTVb, 999, -1, FAM_PRIM,   GRID_ANY, -1, 90.0, "plasma-mode wire tables and pRef/sRef references are fixed Rec.709/D65 constructions" },
	// HDTVa/b primaries under PQ/HLG: WireModeledPrimaryReference
	// deliberately returns the ANALOG color for modes 5/7 (legacy behavior,
	// see its header comment) while the wire re-encodes the 75% tables with
	// the active EOTF - the two conventions are far apart (dE ~70 on blue).
	{ HDTVa, -1, 57, FAM_PRIM, GRID_ANY, -1, 90.0, "legacy analog primaries reference under HDR (WireModeledPrimaryReference modes 5/7 early-out)" },
	{ HDTVb, -1, 57, FAM_PRIM, GRID_ANY, -1, 90.0, "legacy analog primaries reference under HDR (WireModeledPrimaryReference modes 5/7 early-out)" },
	// HDTVa/b PQ reduced-stim 100%-saturation point, 8-bit limited grid
	// only: the PQ 50% anchor is exactly code 110/219, so 0.75 stim lands
	// the encoded signal on an exact half-code (82.5). The generator and
	// the reference reach that value through different matrix round trips
	// whose sub-1e-9 dust falls on opposite sides of the tie -> a one-code
	// split on two channels (constant dE 0.74 at yellow). No other
	// grid/level combination lands on a half-code. (observed worst 0.74)
	{ HDTVa, -1, 5, FAM_SAT75, GRID_8LIM, -1, 2.0, "PQ 50% anchor (code 110/219) x 0.75 stim = exact half-code tie; generator/reference dust splits it" },
	{ HDTVb, -1, 5, FAM_SAT75, GRID_8LIM, -1, 2.0, "PQ 50% anchor (code 110/219) x 0.75 stim = exact half-code tie; generator/reference dust splits it" },
	// Grayscale on the FULL-range grids: the whole gray pipeline
	// (GetGrayPercent, ArrayIndexToGrayLevel, GrayLevelToGrayProp) has no
	// range parameter - ramp codes and references live on the 219/876
	// limited grids, and on a full-range wire the re-snap to 255/1023 is
	// not modeled (<= ~0.4 dE, worst on the PQ 8-bit knee). Fixing requires
	// adding a range parameter through libHCFR, which moves ColorMathTest
	// T2/T3 goldens - out of scope for this harness. Tight ceiling: this
	// entry is broad (any space/white/eotf), so it must NOT swallow a real
	// grayscale/EOTF regression that pushes dE past ~2. (observed worst 0.39)
	{ -1, -1, -1, FAM_GRAY, GRID_FULL, -1, 2.0, "gray ramp codes/references are limited-grid only (no range parameter in GetGrayPercent/GrayLevelToGrayProp)" },
	// MANUAL GENERATOR: the measures grid and the 3D viewer genuinely disagree.
	// GetItemText keeps the legacy DVD conventions for mode 5 - the Mascior
	// disc's 92.254965-nit white in place of TmDiffuseWhiteNits, the fixed
	// 105.95640 reference rescale in place of GetHDRRefScale, and (in its
	// second sub-branch) the extra YWhite * 94.37844 / tmWhite measured-white
	// rescale - while C3DColorView::BuildScene has NO manual-generator branch
	// at all and always uses the unified GetHDRRefScale/GetColorDEWhiteY pair.
	// So the same patch reports one dE in the pane and another in the 3D
	// viewer whenever the user is measuring from a disc. This is the same class
	// of bug the 2026-07 unification fixed for the GDI wire; DVD was
	// deliberately left legacy there, and closing it means deciding WHICH
	// convention wins (a user-visible number change on a legacy path), so it is
	// recorded rather than silently changed.
	//
	// Two magnitudes, matching GetItemText's two sub-branches. Branch A (sat
	// modes for HDTV/UHDTV, and UHDTV2's last saturation column) leaves the
	// measured white alone, so the full 105.95640-vs-10000/92.254965 reference
	// offset - 2.25% - survives: observed 0.49-0.70. Branch B additionally
	// rescales YWhite by 94.37844/tmWhite, which very nearly cancels it:
	// observed 0.02-0.07.
	//
	// LIMITATION, deliberately recorded rather than papered over: these ceilings
	// are keyed on color SPACE, but the two sub-branches are selected by
	// displayMode - and RunSats and RunCC both accumulate into the SAME
	// stats[FAM_CONV]/[FAM_CONVW], which keeps only the worst. So for
	// HDTV/UHDTV/UHDTV2, where branch A reaches ~0.70, a branch-B (color
	// checker) regression is hidden until it exceeds 1.5. Separating them needs
	// per-displayMode FamStats, which is a bigger change than this entry table.
	// The four spaces that only ever take branch B get the tight 0.3 ceiling and
	// are fully protected.
	{ HDTV,   -1, 5, -1, GRID_LIM, 1, 1.5, "manual generator: grid keeps the legacy DVD HDR conventions, the 3D viewer has no DVD branch (GetItemText ~3103 vs Color3DView BuildScene)" },
	{ UHDTV,  -1, 5, -1, GRID_LIM, 1, 1.5, "manual generator: grid keeps the legacy DVD HDR conventions, the 3D viewer has no DVD branch (GetItemText ~3103 vs Color3DView BuildScene)" },
	{ UHDTV2, -1, 5, -1, GRID_LIM, 1, 1.5, "manual generator: grid keeps the legacy DVD HDR conventions, the 3D viewer has no DVD branch (GetItemText ~3103 vs Color3DView BuildScene)" },
	{ UHDTV3, -1, 5, -1, GRID_LIM, 1, 0.3, "manual generator: DVD YWhite * 94.37844/tmWhite rescale nearly cancels the fixed 105.95640, the 3D viewer applies neither" },
	{ UHDTV4, -1, 5, -1, GRID_LIM, 1, 0.3, "manual generator: DVD YWhite * 94.37844/tmWhite rescale nearly cancels the fixed 105.95640, the 3D viewer applies neither" },
	{ HDTVa,  -1, 5, -1, GRID_LIM, 1, 0.3, "manual generator: DVD YWhite * 94.37844/tmWhite rescale nearly cancels the fixed 105.95640, the 3D viewer applies neither" },
	{ HDTVb,  -1, 5, -1, GRID_LIM, 1, 0.3, "manual generator: DVD YWhite * 94.37844/tmWhite rescale nearly cancels the fixed 105.95640, the 3D viewer applies neither" },
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

// Single source for the pane-side HDR-10 (mode 5) reference rescale the
// measures grid applies to non-Mascior sat/CC references (UpdateGrid ~4294):
// the unified CMeasure::GetHDRRefScale() (= 10000 / tone-mapped diffuse
// white; reduces to the legacy 105.95640 with tone mapping off), the same
// scale the 3D viewer uses. FAM_CONV below locks that unification: swapping
// this back to the fixed 105.95640 makes convPV nonzero on the tone-map-ON
// combos (measured 0.015-0.023 dE at TargetMaxL=300 - small because a
// common normalizer shift largely cancels inside a dE difference, hence
// FAM_CONV's dedicated 0.005 tolerance in TolFor).
// Replicates CMainView::GetItemText's color-mode dE (MainView.cpp
// ~3097-3154) for the non-grayscale families. displayMode: 1 = primaries,
// 5..10 = saturation sweeps, 11 = color checker. Both generator branches are
// modeled: the manual-generator (DVD) side keeps its own 92.254965 white and
// 94.37844/tmWhite measured-white rescale.
// The dE evaluation spaces, rebuilt once per combo by ApplyComboConfig.
// Constructing a CColorReference is not cheap (matrix inversion + secondary
// derivation) and these appear in the innermost dE loops; ContainerTransportReference
// in particular builds a whole new reference on every call.
static CColorReference *	s_pGridDERef = NULL;	// GetItemText's bRef
static CColorReference *	s_pViewDERef = NULL;	// AppendMeasure's dERef

// UpdateGrid's sat/CC YWhite selection, whole chain (MainView.cpp ~3686-3706
// then ~3814-3826): the special standards read the ON/OFF white, everyone else
// the prime white - but prime falls back to ON/OFF when it was never measured,
// and both fall back to m_TargetMaxL. Reading GetPrimeWhite().GetY() directly
// (as this used to) skips both fallbacks and would return FX_NODATA (-99999.99)
// for the common "contrast run done, primaries not run, sweep measured" state,
// while the viewer's GetColorDEWhiteY handles it - a phantom divergence.
static double GridYWhite ( const CMeasure & m, bool bSpecial, bool bCC )
{
	CColor prime = m.GetPrimeWhite();
	CColor onoff = m.GetOnOffWhite();
	double yOnOff = onoff.isValid() ? onoff.GetY() : -1.0;
	double yPrime = prime.isValid() ? prime.GetY() : yOnOff;
	double y = bSpecial ? yOnOff : yPrime;
	if ( y == -1.0 )
		y = GetConfig()->m_TargetMaxL;
	// CC only, SDR only: a primaries run made below 90% stimulus (UpdateGrid
	// ~3819-3826).
	if ( bCC && onoff.isValid() && GetConfig()->m_GammaOffsetType != 5
		 && yOnOff > 0 && yPrime / yOnOff < 0.9 )
		y = yOnOff;
	return y;
}

// UpdateGrid's mode-5 sat/CC reference rescale (MainView.cpp ~4313-4333):
// * 100 for the Mascior-style HDR CC sets, the legacy fixed 105.95640 for the
// manual generator, the tone-map-aware GetHDRRefScale otherwise.
static double PaneRefScale ( const CMeasure & m, bool mascior )
{
	if ( mascior )
		return 100.;
	if ( GetConfig()->GetGeneratorType() == CColorHCFRConfig::enumManual )
		return 105.95640;
	return m.GetHDRRefScale();
}

static double GridColorDE ( const CMeasure & m, const CColor & aMeasure, const CColor & aReference,
							double YWhiteIn, int displayMode, int nCol, int satsize )
{
	CColorHCFRConfig * cfg = GetConfig();
	BOOL isHDR = ( cfg->m_GammaOffsetType == 5 && (displayMode == 1 || (displayMode >= 5 && displayMode <= 11)) );
	double YWhite = YWhiteIn, RefWhite = 1.0;
	const CColorReference & cRef = GetColorReference();

	if ( isHDR )
	{
		CColor White = m.GetOnOffWhite();
		CColor Black = m.GetOnOffBlack();
		BOOL DVD = ( cfg->GetGeneratorType() == CColorHCFRConfig::enumManual );
		double tmWhite;
		if ( DVD )
		{
			// The Mascior disc redefines white as its level-502 (50.0%) patch =
			// 92.254965 nits, un-snapped, instead of the GDI wire's 50.22831%
			// code -> TmDiffuseWhiteNits. (MainView.cpp ~3103-3126.)
			bool shiftDiffuse = ( fabs(cfg->m_DiffuseL - 94.0) > 0.5 );
			tmWhite = getL_EOTF(0.50, White, Black, cfg->m_GammaRel, cfg->m_Split, 5, cfg->m_DiffuseL,
								cfg->m_MasterMinL, cfg->m_MasterMaxL, cfg->m_TargetMinL, cfg->m_TargetMaxL,
								cfg->m_useToneMap, FALSE, cfg->m_TargetSysGamma,
								cfg->m_BT2390_BS, cfg->m_BT2390_WS, cfg->m_BT2390_WS1) * 100.0;
			if ( displayMode == 1 )
			{
				if ( cRef.m_standard == UHDTV2 || cRef.m_standard == HDTV || cRef.m_standard == UHDTV || cRef.m_standard == UHDTV3 || cRef.m_standard == UHDTV4 || nCol == 7 )
					RefWhite = YWhite / ( !shiftDiffuse ? 92.254965 : tmWhite );
				else
				{
					RefWhite = YWhite / tmWhite;
					YWhite = YWhite * 94.37844 / tmWhite;
				}
			}
			else
			{
				if ( ( (cRef.m_standard == UHDTV2 && nCol == satsize) || cRef.m_standard == HDTV || cRef.m_standard == UHDTV ) && displayMode != 11 )
					RefWhite = YWhite / tmWhite;
				else
				{
					RefWhite = YWhite / tmWhite;
					YWhite = YWhite * 94.37844 / tmWhite;
				}
			}
		}
		else if ( displayMode == 1 )
		{
			tmWhite = TmDiffuseWhiteNits(White, Black);
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
				// Unified convention (GetItemText sat/CC, non-DVD): reference is
				// GetHDRRefScale-scaled, measured white stays unrescaled.
				tmWhite = TmDiffuseWhiteNits(White, Black);
				RefWhite = YWhite / tmWhite;
			}
		}
	}

	return aMeasure.GetDeltaE(YWhite, aReference, RefWhite, *s_pGridDERef, cfg->m_dE_form, false,
							  cfg->m_GammaOffsetType == 5 ? 3 : cfg->gw_Weight);
}

/////////////////////////////////////////////////////////////////////////////
// The convention families: pane-vs-viewer dE equality.
//
// The 3D viewer computes sat/CC dE as
//   measured.GetDeltaE( GetColorDEWhiteY(), ref * (10000/that white), 1.0, ... )
// (Color3DView BuildScene + AppendMeasure). The measures grid uses the
// GridColorDE chain above with the GetHDRRefScale reference rescale. With the
// ideal sensor a perfect patch reads dE ~ 0 under EITHER convention, so the
// wire-model families cannot see a convention mismatch. This check perturbs
// the measured color (fixed asymmetric XYZ gains -> a few dE of luminance +
// chroma error) and requires both formulas to yield the SAME dE.
//
// What is genuinely SHARED with production here is the dE white: the viewer
// side calls CMeasure::GetColorDEWhiteY, while the pane side is fed the
// harness's own UpdateGrid emulation. So the check also locks "UpdateGrid's
// YWhite selection == GetColorDEWhiteY", which is what the white-source
// families below exercise. The reference scale and the dE evaluation space are
// still re-derived on both sides - see ACCURACYTEST.md coverage gap 7.
//
// Three families run this same comparison under different measured state:
//   convPV   nominal whites (the ideal sensor's prime white lands exactly on
//            TmDiffuseWhiteNits, so the white SOURCE cannot be told apart)
//   convPVw  the three stored whites pulled apart and off target (RunConvWhite)
//   convNW   no white measured at all (RunConvNoWhite)
// wDE is CMeasure::GetColorDEWhiteY for this family, passed in rather than
// re-read per patch: the helper returns CColor copies internally, and
// C3DColorView::BuildScene hoists it out of its patch loops for exactly that
// reason - so hoisting here also keeps the emulation faithful.
static void ConvCheck ( const CMeasure & m, const CColor & aColor, const CColor & refRaw,
						double YWhite, double wDE, int displayMode, int nCol, int satsize,
						FamStat & stat, const char * name, int idx )
{
	CColorHCFRConfig * cfg = GetConfig();
	if ( !aColor.isValid() || !refRaw.isValid() )
		return;

	bool isHDR = ( cfg->m_GammaOffsetType == 5 );
	bool bCC = ( displayMode == 11 );
	bool mascior = ( bCC && cfg->m_CCMode >= MASCIOR50 && cfg->m_CCMode <= CCMAXHDR );

	CColor pert = aColor;
	pert.SetX(pert.GetX() * 1.07);
	pert.SetY(pert.GetY() * 1.05);
	pert.SetZ(pert.GetZ() * 1.03);

	// Pane side: the Mascior HDR CC sets keep their * 100 convention on BOTH
	// sides (RunCC applies it too) - scaling only the viewer side here would
	// report a ~1.06x phantom mismatch. SDR rescales neither side (UpdateGrid's
	// rescale block and the viewer's hdr10Refs are both mode-5 gated).
	CColor refPane = refRaw;
	if ( isHDR )
		ScaleXYZ(refPane, PaneRefScale(m, mascior));
	double dEpane = GridColorDE(m, pert, refPane, YWhite, displayMode, nCol, satsize);

	// Viewer side, as C3DColorView::BuildScene builds it: the reference scaled
	// to the SHARED grid white (CMeasure::GetColorDEWhiteY) with YWhiteRef 1.0,
	// and the grid's dE evaluation space. Scaling by 10000/white with
	// YWhiteRef 1.0 is algebraically the grid's (ref * GetHDRRefScale(),
	// RefWhite = YWhite/tmWhite) pair, so any drift between the two - a
	// reference rescale, a white source, or a dE space - shows up here.
	if ( wDE <= 0.0 )
	{
		// Unreachable while GetColorDEWhiteY ends in its m_TargetMaxL fallback,
		// and it must FAIL loudly if that ever stops being true: the grid would
		// keep printing numbers off m_TargetMaxL while the viewer's
		// AppendMeasure treats ywForDE <= 0 as "blackish" and drops the dE
		// entirely - a real two-consumer divergence. Recording 0.0 here (which
		// this used to do) promoted the family from the -1.0 "not run" sentinel
		// to a 0.000 PASS: it failed OPEN on exactly the state convNW exists for.
		stat.Add(999.0, "%s %d (no dE white: GetColorDEWhiteY returned %.3f)", name, idx, wDE);
		return;
	}
	CColor refView = refRaw;
	if ( isHDR )
		ScaleXYZ(refView, mascior ? 100. : 10000. / wDE);
	// AppendMeasure's gw: 3 in mode 5, the configured weight otherwise.
	double dEview = pert.GetDeltaE(wDE, refView, 1.0, *s_pViewDERef, cfg->m_dE_form, false,
								   isHDR ? 3 : cfg->gw_Weight);

	stat.Add(fabs(dEpane - dEview), "%s %d (pane %.3f vs viewer %.3f)", name, idx, dEpane, dEview);
}

// Measured patch + raw reference kept from the sat/CC runs so the white-source
// families can REPLAY the convention comparison under different stored whites
// without re-measuring (the whites are CMeasure state, not per-patch state).
struct ConvSample
{
	CColor			meas;
	CColor			ref;
	int				displayMode;	// 5..10 saturation, 11 color checker
	int				nCol;
	const char *	name;
	int				idx;
};
static const int kMaxConvSamples = 48;	// high-water mark is 36 (30 sat + 6 stratified CC)
static ConvSample	s_convSamples[kMaxConvSamples];
static int			s_nConvSamples = 0;

static void KeepConvSample ( const CColor & meas, const CColor & ref, int displayMode,
							 int nCol, const char * name, int idx )
{
	if ( s_nConvSamples >= kMaxConvSamples )
		return;
	ConvSample & s = s_convSamples[s_nConvSamples ++];
	s.meas = meas; s.ref = ref; s.displayMode = displayMode;
	s.nCol = nCol; s.name = name; s.idx = idx;
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
	// Tone-map-ON combos target 300 nits instead: BT.2390's knee
	// (KS = 1.5*PQ(Lmax) - 0.5) then falls BELOW the 50.23% diffuse-white
	// code, so tone mapping actually compresses diffuse white and the HDR
	// reference-rescale/white-anchor conventions get real coverage - at 700
	// nits the knee sits ~245 nits and diffuse white passes through untouched
	// (TM-off PQ keeps 700 for hard-clip coverage).
	// TargetMinL stays 0: the references target ideal black (Y=0), while the
	// sensor pins the black patch to TargetMinL - a nonzero floor is a
	// modeled DISPLAY property that would show up as a constant black-patch
	// dE, not a wire-modeling error. (With black 0, BT.1886 degenerates to a
	// pure 2.4 power - consistent on both sides.)
	BOOL bHdr = ( c.eotf == 5 || c.eotf == 7 );
	cfg->m_TargetMaxL = ( c.maxL > 0.0 ) ? c.maxL : ( bHdr ? ( c.toneMap ? 300.0 : 700.0 ) : 120.0 );
	cfg->m_TargetMinL = 0.0;
	cfg->m_bOverRideTargs = FALSE;
	cfg->m_userBlack = FALSE;

	// Generator FIRST: SetGeneratorType calls RefreshUse10bitLevels(), which
	// re-derives the cached grid flags from the scratch ini's generator keys.
	// Setting it after the flags below would silently reset every combo to the
	// ini's 8-bit-limited default - which is exactly what it did until a
	// half-code known-fail started firing on all four grids instead of one.
	// Guarded because SetGeneratorType also does a WriteProfileString (a full
	// ini rewrite) plus several reads whose results the flags below overwrite:
	// unguarded, that is ~876 file rewrites per run for ~168 real changes.
	CColorHCFRConfig::GeneratorType wantGen = c.dvd ? CColorHCFRConfig::enumManual
													: CColorHCFRConfig::enumAutomatic;
	if ( cfg->GetGeneratorType() != wantGen )
		cfg->SetGeneratorType(wantGen);

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

	// Cache the two dE evaluation spaces for the combo (see s_pGridDERef).
	// GetItemText's bRef: transport space for the UHDTV3/4 pseudo-spaces,
	// Rec.709 for the special standards, the active reference otherwise.
	// AppendMeasure's dERef: transport for the pseudo-spaces, active otherwise
	// (HDTVa/b need no branch - GetDeltaE forces Rec.709 for them internally).
	// NULL between the delete and the new: if a CColorReference construction
	// throws (its ctor inverts a matrix), the next call must not free a stale
	// pointer, and GridColorDE/ConvCheck must fault on a NULL rather than on
	// freed memory.
	const CColorReference & aRef = GetColorReference();
	bool pseudo = ( aRef.m_standard == UHDTV3 || aRef.m_standard == UHDTV4 );
	delete s_pGridDERef;	s_pGridDERef = NULL;
	delete s_pViewDERef;	s_pViewDERef = NULL;
	s_pGridDERef = new CColorReference( pseudo ? ContainerTransportReference(aRef)
						 : ( aRef.m_standard == HDTVa || aRef.m_standard == HDTVb ) ? CColorReference(HDTV) : aRef );
	s_pViewDERef = new CColorReference( pseudo ? ContainerTransportReference(aRef) : aRef );
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
static void RunSats ( const Combo & c, CMeasure & m, CSimulatedSensor & sensor, double stim, FamStat & stat, FamStat & convStat )
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
	// Loop invariants, hoisted exactly as C3DColorView::BuildScene hoists them.
	double YWhite = GridYWhite(m, special, false);
	double wDE = m.GetColorDEWhiteY(special, false, false);
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
			ConvCheck(m, aColor, refColor, YWhite, wDE, 5 + s, j + 1, kSatSize, convStat, names[s], j);
			// Only the 100% sweep feeds the white-source replays: the 75% sweep
			// exercises the same normalization with a different reference, so
			// keeping both would double the replay cost for no new convention.
			if ( stim >= 1.0 )
				KeepConvSample(aColor, refColor, 5 + s, j + 1, names[s], j);
			if ( cfg->m_GammaOffsetType == 5 )
				ScaleXYZ(refColor, PaneRefScale(m, false));	// UpdateGrid ~4313
			double dE = GridColorDE(m, aColor, refColor, YWhite, 5 + s, j + 1, kSatSize);
			stat.Add(dE, "%s sat %d%% stim %.0f%%", names[s], j * 25, stim * 100.);
		}
	}
}

// One color-checker set (GCD 24 or AXIS 71).
static void RunCC ( const Combo & c, CMeasure & m, CSimulatedSensor & sensor, CCPatterns ccMode, int count, FamStat & stat, FamStat & convStat )
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

	// UpdateGrid's default-case YWhite, including the CC-only sub-90%-stimulus
	// fallback - see GridYWhite. Omitting the special-standard selection here
	// would normalize HDTVa/b CC dE by the 75% PrimeWhite instead of peak white
	// and diverge from the grid.
	bool special = ( cfg->m_colorStandard == HDTVa || cfg->m_colorStandard == HDTVb );
	double YWhite = GridYWhite(m, special, true);
	bool mascior = ( ccMode >= MASCIOR50 && ccMode <= CCMAXHDR );
	double wDE = m.GetColorDEWhiteY(special, true, mascior);

	for ( int j = 0 ; j < count ; j ++ )
	{
		// CC patches are NOT in GDIGenerator's Intensity-dimmed pattern types
		CColor aColor = Meas(sensor, GenColors[j], 1.0);
		CColor refColor;
		m.GetRefCC24Sat(j, refColor);
		ConvCheck(m, aColor, refColor, YWhite, wDE, 11, j + 1, kSatSize, convStat, "patch", j);
		// One CC set feeds the replays (the GCD run, which is also the CC mode
		// left active afterwards - see RunConvWhite), stratified every 4th
		// patch: a white-source split shows on every patch, so the replay only
		// needs a spread across the luminance range, not all 24.
		if ( ccMode == GCD && (j % 4) == 0 )
			KeepConvSample(aColor, refColor, 11, j + 1, "patch", j);
		if ( cfg->m_GammaOffsetType == 5 )
			ScaleXYZ(refColor, PaneRefScale(m, mascior));	// UpdateGrid ~4313-4333
		double dE = GridColorDE(m, aColor, refColor, YWhite, 11, j + 1, kSatSize);
		stat.Add(dE, "patch %d (%.2f/%.2f/%.2f%%)", j, GenColors[j][0], GenColors[j][1], GenColors[j][2]);
	}

	cfg->m_CCMode = GCD;
}

/////////////////////////////////////////////////////////////////////////////
// FAM_CONVW: the same pane-vs-viewer comparison with the measured whites
// pulled OFF their theoretical values, and off EACH OTHER.
//
// This is coverage gap 1. With the ideal sensor the measured prime white lands
// exactly on TmDiffuseWhiteNits (the 50.22831% wire code and the helper's
// snapped 0.5022283 reach the same grid code on all four grids), so every
// candidate white - prime, ON/OFF, grayscale top, theoretical tmWhite - has
// the SAME value and a consumer normalizing by the wrong one is invisible.
// That is exactly how the 3D viewer shipped a ~1% dE offset against the grid
// (grid 8.80 vs viewer 8.72 on a display 3.5% below target).
//
// The perturbation is applied to the STORED whites rather than to the sensor:
// both consumers read the same CMeasure, so a differential check only needs
// the state to differ, and scaling the three whites by DIFFERENT factors makes
// them mutually distinguishable - a consumer reading prime where the grid
// reads ON/OFF (or the theoretical white) cannot hide behind a common factor.
// A real display cannot have its grayscale top and its ON/OFF white disagree;
// that is deliberate, and it is why this family asserts only the differential,
// never dE ~ 0.
//
// The gains are otherwise arbitrary; 0.965 on prime white reproduces the
// 3.5%-low display that surfaced the original bug.
// Only two whites are perturbed. A third gain on the grayscale TOP was
// dropped: that white is read only through GetColorDEWhiteY's Mascior branch,
// and RunCC restores m_CCMode = GCD before this runs, so it was inert - it
// changed no computed value and gave the false impression that this family
// locks the Mascior white source. Covering that source needs a Mascior CC set
// in the matrix (ACCURACYTEST.md coverage gap 3).
static const double kPrimeWhiteGain = 0.965;
static const double kOnOffWhiteGain = 0.982;

static void RunConvWhite ( CMeasure & m, FamStat & stat )
{
	CColorHCFRConfig * cfg = GetConfig();
	bool special = ( cfg->m_colorStandard == HDTVa || cfg->m_colorStandard == HDTVb );
	if ( s_nConvSamples == 0 )
		return;

	CColor savePrime = m.GetPrimeWhite();
	CColor saveOnOff = m.GetOnOffWhite();

	CColor p = savePrime; ScaleXYZ(p, kPrimeWhiteGain); m.SetPrimeWhite(p);
	CColor o = saveOnOff; ScaleXYZ(o, kOnOffWhiteGain); m.SetOnOffWhite(o);

	// Deliberately NO RefreshHdrTargets here: leaving m_TargetMaxL (and with it
	// tmWhite / GetHDRRefScale) at its converged value is what DECOUPLES the
	// measured white from the theoretical one. Refreshing would drag the
	// theoretical white along and restore the accidental equality.
	bool mascior = ( cfg->m_CCMode >= MASCIOR50 && cfg->m_CCMode <= CCMAXHDR );

	// UpdateGrid's YWhite recomputed from the PERTURBED state, plus the matching
	// shared white - one pair for saturation samples, one for CC samples.
	double YWhiteSat = GridYWhite(m, special, false);
	double YWhiteCC  = GridYWhite(m, special, true);
	double wDESat = m.GetColorDEWhiteY(special, false, false);
	double wDECC  = m.GetColorDEWhiteY(special, true, mascior);

	for ( int i = 0 ; i < s_nConvSamples ; i ++ )
	{
		const ConvSample & s = s_convSamples[i];
		bool bCC = ( s.displayMode == 11 );
		ConvCheck(m, s.meas, s.ref, bCC ? YWhiteCC : YWhiteSat, bCC ? wDECC : wDESat,
				  s.displayMode, s.nCol, kSatSize, stat, s.name, s.idx);
	}

	m.SetPrimeWhite(savePrime);
	m.SetOnOffWhite(saveOnOff);
}

/////////////////////////////////////////////////////////////////////////////
// FAM_CONVNW: a saturation sweep with NO grayscale and NO primaries run.
//
// Coverage gap 5: the matrix always runs gray -> primaries -> sat/CC, so
// PrimeWhite and OnOffWhite are always valid and neither the grid's
// m_TargetMaxL fallback (MainView.cpp ~3699-3706) nor GetColorDEWhiteY's
// matching fallback is ever reached. A user who measures a saturation sweep
// first hits exactly this state. The wire-model dE is meaningless here (the
// references are normalized by a white that was never measured), so this
// family asserts only the pane/viewer agreement - and that nothing produces a
// NaN, which is what an unguarded divide by a missing white looks like.
static void RunConvNoWhite ( const Combo & c, CSimulatedSensor & sensor, FamStat & stat )
{
	CColorHCFRConfig * cfg = GetConfig();
	bool special = ( cfg->m_colorStandard == HDTVa || cfg->m_colorStandard == HDTVb );
	static const char * names[6] = { "red", "green", "blue", "yellow", "cyan", "magenta" };
	static const bool flags[6][3] =
	{
		{ true,false,false }, { false,true,false }, { false,false,true },
		{ true,true,false }, { false,true,true }, { true,false,true },
	};

	// A default-constructed CMeasure is NOT white-less: the constructor
	// pre-loads m_PrimeWhite, m_OnOffWhite and the grayscale top to plausible
	// display values at Y = m_TargetMaxL (Measure.cpp ~203, ~223-225), and
	// isValid() only rejects components <= -1.0 - so all three read as
	// measured. Invalidating the two whites explicitly is what actually
	// reaches the missing-white path; without it this family silently tested
	// nothing, because the ctor's Y happens to equal the m_TargetMaxL the pane
	// side falls back to, so both sides agreed for the wrong reason.
	// The gray array is deliberately left alone: GetRefSat indexes
	// GetGray(GetGrayScaleSize()-1) unguarded.
	CMeasure m;
	m.SetSaturationSize(kSatSize);
	m.SetPrimeWhite(noDataColor);
	m.SetOnOffWhite(noDataColor);

	// Both whites are now invalid, so UpdateGrid falls all the way through to
	// m_TargetMaxL (MainView.cpp ~3699-3706) and GetColorDEWhiteY must land on
	// the same fallback. Note m_TargetMaxL is whatever RefreshHdrTargets left
	// after RunGray (the measured on/off white on HDR combos), NOT the value
	// ApplyComboConfig set - which is fine, both sides read the same config.
	double YWhite = cfg->m_TargetMaxL;
	double wDE = m.GetColorDEWhiteY(special, false, false);

	for ( int s = 0 ; s < 6 ; s ++ )
	{
		ColorRGBDisplay GenColors[kSatSize];
		GenerateSaturationColors(GetColorReference(), GenColors, kSatSize, flags[s][0], flags[s][1], flags[s][2],
								 cfg->m_GammaOffsetType, 1.0, cfg->GetUse10bitLevels() != FALSE, cfg->GetRGB16_235() != FALSE);
		for ( int j = 0 ; j < kSatSize ; j ++ )
		{
			CColor aColor = Meas(sensor, GenColors[j], c.intensity);
			CColor refColor = m.GetRefSat(s, (double)j / (double)(kSatSize - 1), special, 1.0);
			ConvCheck(m, aColor, refColor, YWhite, wDE, 5 + s, j + 1, kSatSize,
					  stat, names[s], j);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// Tolerances and known-fail matching

static double TolFor ( const Combo & c, int fam )
{
	// Matched pane/viewer conventions agree to machine precision (identical
	// GetDeltaE arguments), while the legacy 105.95640 pane convention reads
	// 0.015-0.023 under 300-nit tone mapping - the default 0.05 would let a
	// convention regression back in. The white-source families need the same
	// tightness: normalizing by the wrong white moves dE by ~1%, which on a
	// ~5 dE perturbed patch is ~0.05 - exactly the default tolerance.
	if ( IsConvFamily(fam) )
		return 0.005;
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
		if ( k.dvd != -1 && (k.dvd != 0) != c.dvd ) continue;
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
	s_nConvSamples = 0;

	// Order matters and mirrors real usage: grayscale bootstraps
	// OnOffWhite/OnOffBlack (the HLG/BT.1886 reference decode White), then
	// primaries set PrimeWhite (the sat/CC dE normalizer).
	RunGray(c, m, sensor, stats[FAM_GRAY]);
	RunPrimaries(c, m, sensor, stats[FAM_PRIM]);
	RunSats(c, m, sensor, 1.0, stats[FAM_SAT100], stats[FAM_CONV]);
	RunSats(c, m, sensor, 0.75, stats[FAM_SAT75], stats[FAM_CONV]);
	RunCC(c, m, sensor, GCD, 24, stats[FAM_CC_GCD], stats[FAM_CONV]);
	RunCC(c, m, sensor, AXIS, 71, stats[FAM_CC_AXIS], stats[FAM_CONV]);

	// White-source coverage, last: RunConvWhite mutates the stored whites (and
	// restores them), RunConvNoWhite builds its own empty CMeasure. Neither
	// must run before the wire-model families above.
	RunConvWhite(m, stats[FAM_CONVW]);
	RunConvNoWhite(c, sensor, stats[FAM_CONVNW]);

	// Manual generator: reference == wire is NOT testable. HCFR generates no
	// patches at all in this mode - the "wire" is the disc the user plays, and
	// the DVD conventions are built for the Mascior disc's 50.0% / 92.254965-nit
	// white, not for the 50.22831% code the simulated sensor is fed. The
	// wire-model families still RUN (they bootstrap the measured whites and
	// collect the convention samples) but their result is not scored; what the
	// DVD combos exist to check is the convention families, which compare two
	// consumers reading the same state.
	if ( c.dvd )
	{
		for ( int f = 0 ; f < FAM_COUNT ; f ++ )
			if ( !IsConvFamily(f) )
				stats[f].worst = -1.0;
	}

	// Evaluate. MSVC's _snprintf returns NEGATIVE on truncation and does not
	// NUL-terminate, and the remaining-space argument is size_t - so a raw
	// `n += _snprintf(line+n, sizeof(line)-1-n, ...)` would write before the
	// buffer AND widen its own bound the moment it overflowed. Clamp n after
	// every append and terminate explicitly.
	char line[512];
	const int kLineMax = (int)sizeof(line) - 1;
	int n = _snprintf(line, kLineMax, "%-8s %-10s %-6s %-8s %3.0f%%  %-4s ",
					  c.stdName, c.eotfName, c.whiteName, c.gridName, c.intensity * 100.,
					  c.dvd ? "DVD" : "GDI");
	if ( n < 0 || n > kLineMax ) n = kLineMax;
	line[n] = 0;
	BOOL bFail = FALSE, bKnown = FALSE;
	for ( int fam = 0 ; fam < FAM_COUNT ; fam ++ )
	{
		double tol = TolFor(c, fam);
		int iKnown = MatchKnownFail(c, fam);
		double w = stats[fam].worst;
		BOOL over = ( w > tol );
		// A known-fail only absorbs the result while it stays within the
		// entry's documented ceiling; beyond that, the known issue has grown
		// = a real regression, so treat it as a hard FAIL.
		BOOL absorbed = ( iKnown >= 0 && w <= kKnownFails[iKnown].ceiling );
		int nAdd = _snprintf(line + n, kLineMax - n, "%7.3f%c", w, over ? (absorbed ? '#' : '*') : ' ');
		n = ( nAdd < 0 || n + nAdd > kLineMax ) ? kLineMax : n + nAdd;
		line[n] = 0;
		if ( over )
		{
			if ( absorbed )
			{
				bKnown = TRUE;
				s_knownFailFired[iKnown] = true;
				Detail("KNOWN-FAIL  %s %s %s %s %.0f%% %s [%s]: worst dE %.3f at %s\n            reason: %s\n",
					   c.stdName, c.eotfName, c.whiteName, c.gridName, c.intensity * 100., c.dvd ? "DVD" : "GDI",
					   kFamilyName[fam], w, stats[fam].desc, kKnownFails[iKnown].reason);
			}
			else if ( iKnown >= 0 )
			{
				bFail = TRUE;
				Detail("FAIL(>ceil) %s %s %s %s %.0f%% %s [%s]: worst dE %.3f exceeds known-fail ceiling %.2f at %s\n            the known gap GREW - likely a regression: %s\n",
					   c.stdName, c.eotfName, c.whiteName, c.gridName, c.intensity * 100., c.dvd ? "DVD" : "GDI",
					   kFamilyName[fam], w, kKnownFails[iKnown].ceiling, stats[fam].desc, kKnownFails[iKnown].reason);
			}
			else
			{
				bFail = TRUE;
				Detail("FAIL        %s %s %s %s %.0f%% %s [%s]: worst dE %.3f (tol %.3f) at %s\n",
					   c.stdName, c.eotfName, c.whiteName, c.gridName, c.intensity * 100., c.dvd ? "DVD" : "GDI",
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

int RunAccuracyTest ( const char * pReportPath, bool bQuick )
{
	s_bQuick = bQuick;

	// Show progress/summary when launched from a console
	BOOL bConsole = AttachConsole(ATTACH_PARENT_PROCESS);
	if ( bConsole )
	{
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
		printf("\n");
	}

	CColorHCFRConfig * cfg = GetConfig();

	// Profile traffic is already on the scratch ini: the config was built with
	// CColorHCFRConfig::s_bHeadless set (see the InitInstance hook), so its
	// constructor never touched the user's real config and put m_iniFileName on
	// this same scratch path. Re-assert it here (idempotent) as defense in
	// depth, and because the process exits via ExitProcess, no settings-saving
	// teardown ever runs either.
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
	if ( s_bQuick )
		fprintf(s_fReport, "*** QUICK subset: D65 + xyCust whites, 8b-lim + 8b-full grids only.\n"
						   "*** All spaces/EOTFs/generators/intensities run and every known-fail\n"
						   "*** entry still fires, so the exit code means the same thing - but the\n"
						   "*** dropped grids and white carry level-dependent gaps. Run the FULL\n"
						   "*** matrix before committing.\n");
	fprintf(s_fReport, "Columns are the worst dE per patch family; '*' = over tolerance (FAIL),\n");
	fprintf(s_fReport, "'#' = over tolerance but documented KNOWN-FAIL (see detail below).\n");
	fprintf(s_fReport, "Tolerance: 0.05; Intensity-90%% combos: 0.9 (power law) / 1.5 (other EOTFs); conv*: 0.005\n");
	fprintf(s_fReport, "dE settings: dE_form=3 (CIE2000), dE_gray=1 (gamma-predicted gray target), gw_Weight=0\n");
	fprintf(s_fReport, "conv* families are DIFFERENTIAL: |grid dE - 3D-viewer dE| for a perturbed patch\n");
	fprintf(s_fReport, "(-1.000 = family not run). convPV = nominal whites; convPVw = the three stored\n");
	fprintf(s_fReport, "whites pulled off target and off each other; convNW = no white measured at all.\n");
	fprintf(s_fReport, "gen: GDI = automatic generator, DVD = manual generator. DVD combos score ONLY the\n");
	fprintf(s_fReport, "conv* columns - HCFR emits no patches in that mode, so reference == wire has no\n");
	fprintf(s_fReport, "meaning; what they check is that the grid's DVD conventions and the viewer agree.\n\n");
	fprintf(s_fReport, "%-8s %-10s %-6s %-8s %-5s %-4s %8s %7s %7s %7s %7s %7s %7s %7s %7s  %s\n",
			"space", "eotf", "white", "grid", "inten", "gen",
			"gray", "prim", "sat100", "sat75", "ccGCD", "ccAXIS", "convPV", "convPVw", "convNW", "result");

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

			int nWhites = s_bQuick ? s_nQuickWhites : (int)(sizeof(kWhites)/sizeof(kWhites[0]));
			int nGrids  = s_bQuick ? s_nQuickGrids  : (int)(sizeof(kGrids)/sizeof(kGrids[0]));
			for ( int jWhite = 0 ; jWhite < nWhites ; jWhite ++ )
			{
				iWhite = s_bQuick ? kQuickWhiteIdx[jWhite] : jWhite;
				for ( int jGrid = 0 ; jGrid < nGrids ; jGrid ++ )
				{
					iGrid = s_bQuick ? kQuickGridIdx[jGrid] : jGrid;
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
						c.maxL = ( sp.cs == sRGB ) ? 0.0 : e.maxL;
						c.white = kWhites[iWhite].wt;
						c.b10 = kGrids[iGrid].b10;
						c.lim = kGrids[iGrid].lim;
						c.gridIdx = iGrid;
						c.intensity = iInt ? 0.9 : 1.0;
						c.dvd = false;
						c.eotfName = ( sp.cs == sRGB ) ? "sRGB" : e.name;
						c.whiteName = kWhites[iWhite].name;
						c.gridName = kGrids[iGrid].name;
						c.stdName = sp.name;
						RunCombo(c);

						// Manual-generator pass. Every DVD carve-out in
						// GetItemText / UpdateGrid sits inside the mode-5 HDR
						// block, so only PQ combos differ at all; and the
						// carve-outs are white-independent (they swap the
						// diffuse-white CONSTANT, not the chromaticity), so
						// D65 covers them - running all three whites would
						// triple the cost for identical branches.
						// LIMITED-RANGE grids only: RefreshUse10bitLevels
						// forces m_bRGB16_235 = TRUE for the manual generator
						// ("consumer disc video is 16-235 by definition"), so a
						// full-range DVD combo models a wire the app cannot
						// emit - and would calibrate the DVD known-fail
						// ceilings on unreachable rows.
						if ( eotf == 5 && kWhites[iWhite].wt == D65 && iInt == 0 && kGrids[iGrid].lim )
						{
							c.dvd = true;
							RunCombo(c);
						}
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
	fprintf(s_fReport, "SUMMARY:%s %d combos: %d pass, %d FAIL, %d known-fail%s\n",
			s_bQuick ? " [QUICK subset]" : "",
			s_nCombos, s_nPass, s_nFail, s_nKnownFail,
			s_nUnexpectedPass ? " (stale known-fail entries present!)" : "");
	fclose(s_fReport);

	// Fail the run on stale known-fail entries too: a never-fired entry means
	// a combo was dropped or an issue was fixed (and the entry now sits ready
	// to mask a NEW nearby regression) - a coverage regression CI must catch,
	// not a silent exit 0.
	int rc = ( s_nFail > 0 || s_nUnexpectedPass > 0 ) ? 1 : 0;
	if ( bConsole )
	{
		printf("accuracytest: %d combos: %d pass, %d FAIL, %d known-fail, %d stale -> exit %d (report: %s)\n",
			   s_nCombos, s_nPass, s_nFail, s_nKnownFail, s_nUnexpectedPass, rc, pPath);
		fflush(stdout);
	}
	return rc;
}
