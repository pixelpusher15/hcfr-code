/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2026 Association Homecinema Francophone.  All rights reserved.
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

// Color3DView.cpp : implementation of the 3D color-space viewer.
//
// Everything with depth (CIE tongue floor, measured points, gamut faces) is
// rasterized in software into a 32-bit top-down DIB with a per-pixel z-buffer;
// GDI+ draws only the gamut wireframe / axes / labels on top. The projection is
// orthographic, so the floor is an affine texture map and gamut faces are flat.

#include "stdafx.h"
#include "ColorHCFR.h"
#include "DataSetDoc.h"
#include "DocTempl.h"
#include "Color3DView.h"
#include "MainView.h"
#include "GdiPlusAA.h"
#include "math.h"
#include <stdio.h>
#include <algorithm>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Provided by Tools/CIEChartImage.cpp (no public header). Renders the CIE 1931
// chromaticity "tongue" into pDC over cxMax x cyMax pixels; the xy->pixel map is
// px = (x+0.075)/0.9 * W,  py = (1 - (y+0.05)) * H   (full chart, non-uv).
// The Ex variant adds bHideLabels to drop the "nnn nm" wavelength ticks on the
// floor (the plain DrawCIEChart wrapper keeps them for the 2D CIE chart).
extern void DrawCIEChartEx(CDC* pDC, int cxMax, int cyMax, BOOL doFullChart, BOOL doShowBlack, BOOL bCIEuv, BOOL bCIEab, BOOL bHideLabels);

static const double k2PI = 6.283185307179586;

// Chromaticity rectangle the tongue floor covers (must match the tex mapping).
static const double kFloorX0 = 0.0, kFloorX1 = 0.75;
static const double kFloorY0 = 0.0, kFloorY1 = 0.85;

// Scale of the xyY luminance axis: 1.0 = natural (normalized Y 0..1 on the same
// scale as the chromaticity axes), no vertical exaggeration.
static const double kLumStretchXYY = 1.0;

// The CIE tongue floor sits slightly BELOW the model floor so it never z-fights
// (banding) with the gamut volume's base/wall geometry at Y=0.
static const double kFloorDrop = 0.03;

// Gamut edge/face tessellation subdivisions (per cube edge). Higher = smoother
// curves, more triangles.
static const int kGamutN = 12;

// Above this many points the orbs drop to the smaller radius.
static const size_t kOrbDenseThreshold = 4000;

// Chromaticity distance under which a free measurement is auto-matched to the
// white/primary/secondary reference (mirrors the measures grid convention).
static const double kChromaMatch = 0.05;

// Fill a buffer with a repeated value via doubling memcpy -- far faster than an
// element loop for the multi-megabyte per-frame clears.
template <typename T>
static void FillDoubling(T * p, size_t n, T value)
{
	if ( n == 0 )
		return;
	size_t seed = n < 256 ? n : 256;
	for ( size_t s = 0; s < seed; s++ ) p[s] = value;
	for ( size_t done = seed; done < n; done += done )
	{
		size_t chunk = ( done < n - done ) ? done : n - done;
		memcpy( p + done, p, chunk * sizeof( T ) );
	}
}

// Camera distance for the perspective projection (model units; scene radius
// ~2.3). Larger = flatter/more orthographic; 5.0 chosen by eye.
static const double kCamDist = 5.0;

// CIE floor subdivisions: under perspective an affine texture map warps across
// large triangles, so the floor quad is rasterized as an NxN grid instead.
static const int kFloorGridN = 8;

// Translucency (0..255) of the shaded gamut faces and the CIE tongue floor.
static const int kGamutFaceAlpha = 16;
static const int kFloorAlpha     = 115;

static inline double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline double min3(double a, double b, double c) { double m = a < b ? a : b; return m < c ? m : c; }
static inline double max3(double a, double b, double c) { double m = a > b ? a : b; return m > c ? m : c; }

// Pack an 8-bit RGB triple into a 32-bit BI_RGB DIB pixel (0x00RRGGBB).
static inline DWORD DibColor(int r, int g, int b)
{
	return ((DWORD)(r & 0xFF) << 16) | ((DWORD)(g & 0xFF) << 8) | (DWORD)(b & 0xFF);
}

// Blend src over dst (alpha 0..255), both 0x00RRGGBB DIB pixels.
static inline DWORD BlendDib(DWORD src, DWORD dst, int a)
{
	int sr = ( src >> 16 ) & 0xFF, sg = ( src >> 8 ) & 0xFF, sb = src & 0xFF;
	int dr = ( dst >> 16 ) & 0xFF, dg = ( dst >> 8 ) & 0xFF, db = dst & 0xFF;
	return DibColor( ( sr * a + dr * ( 255 - a ) ) / 255,
					 ( sg * a + dg * ( 255 - a ) ) / 255,
					 ( sb * a + db * ( 255 - a ) ) / 255 );
}

// Gamma-encode a linear ColorRGB into a DIB swatch pixel so the dot reads
// roughly like the patch on screen.
static DWORD RgbSwatch(const ColorRGB & rgb)
{
	double r = pow( clampd( rgb[0], 0.0, 1.0 ), 1.0 / 2.2 );
	double g = pow( clampd( rgb[1], 0.0, 1.0 ), 1.0 / 2.2 );
	double b = pow( clampd( rgb[2], 0.0, 1.0 ), 1.0 / 2.2 );
	return DibColor( (int)( r * 255.0 + 0.5 ), (int)( g * 255.0 + 0.5 ), (int)( b * 255.0 + 0.5 ) );
}

// dE heatmap anchored to the shared tolerance thresholds so the bands agree
// with the data grid's colouring (GetDEColor): greens below "good", yellows to
// orange between "good" and "warn", reds at or above "warn". Gradients within
// each band keep the magnitude readable.
static DWORD HeatColor(double dE, double good, double warn)
{
	if ( dE < good )
	{
		double t = ( good > 0.0 ) ? clampd( dE / good, 0.0, 1.0 ) : 0.0;
		return DibColor( (int)( 140.0 * t ), (int)( 180.0 + 40.0 * t ), 50 );          // green -> yellow-green
	}
	if ( dE < warn )
	{
		double t = ( warn > good ) ? clampd( ( dE - good ) / ( warn - good ), 0.0, 1.0 ) : 0.0;
		return DibColor( 255, (int)( 220.0 - 70.0 * t ), 35 );                          // yellow -> orange
	}
	double t = clampd( ( dE - warn ) / ( warn > 0.0 ? warn : 1.0 ), 0.0, 1.0 );
	return DibColor( 255 - (int)( 40.0 * t ), (int)( 60.0 - 40.0 * t ), 45 );           // red, deepening
}

/////////////////////////////////////////////////////////////////////////////
// C3DColorView

IMPLEMENT_DYNCREATE(C3DColorView, CSavingView)

BEGIN_MESSAGE_MAP(C3DColorView, CSavingView)
	//{{AFX_MSG_MAP(C3DColorView)
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSEWHEEL()
	ON_WM_CONTEXTMENU()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

C3DColorView::C3DColorView()
	: CSavingView()
	, m_sceneDirty(true)
	, m_freeInScene(0)
	, m_profileInScene(0)
	, m_lumTop(1.0)
	, m_baseValid(false)
	, m_gamutValid(false)
	, m_texDC(NULL), m_texBmp(NULL), m_texOld(NULL), m_texBits(NULL), m_texW(0), m_texH(0), m_texSig(-1)
	, m_space(SPACE_XYY)
	, m_yaw(0.70), m_pitch(0.45), m_zoom(1.0)
	, m_panX(0.0), m_panY(0.0)
	, m_showGamut(true), m_showFloor(true), m_shadeGamut(true)
	, m_pointColor(PTCOLOR_DE), m_deFilter(0), m_showTails(true), m_showProfilePts(true)
	, m_bDragging(false)
	, m_selected(-1)
	, m_orbKernelR(-1)
	, m_memDC(NULL), m_memBmp(NULL), m_oldBmp(NULL), m_bits(NULL), m_bw(0), m_bh(0)
	, m_cx(0), m_cyc(0), m_scale(0), m_ry(1), m_rsy(0), m_rp(1), m_rsp(0)
{
}

C3DColorView::~C3DColorView()
{
	FreeBackbuffer();
	FreeTongueTexture();
}

/////////////////////////////////////////////////////////////////////////////
// scene

// Map a measured/reference XYZ to a model-space coordinate (roughly a unit cube)
// for the current plot space.
void C3DColorView::ToModel(const ColorXYZ & xyz, double whiteY, CColorReference & ref,
						   double & mx, double & my, double & mz) const
{
	switch ( m_space )
	{
		case SPACE_RGB:
		{
			ColorRGB rgb( xyz, ref );
			mx = ( clampd( rgb[0], 0.0, 1.0 ) - 0.5 ) / 0.5;
			my = ( clampd( rgb[1], 0.0, 1.0 ) - 0.5 ) / 0.5;
			mz = ( clampd( rgb[2], 0.0, 1.0 ) - 0.5 ) / 0.5;
			break;
		}
		case SPACE_LAB:
		{
			ColorLab lab( xyz, whiteY, ref );
			mx = lab[1] / 128.0;            // a*
			my = ( lab[0] - 50.0 ) / 50.0;  // L* centred
			mz = lab[2] / 128.0;            // b*
			break;
		}
		case SPACE_XYY:
		default:
		{
			double X = xyz[0], Yv = xyz[1], Z = xyz[2], sum = X + Yv + Z;
			double xx, yy;
			if ( sum > 1e-9 ) { xx = X / sum; yy = Yv / sum; }
			else { ColorxyY w( ref.GetWhite() ); xx = w[0]; yy = w[1]; }  // black -> white chromaticity
			double Y = whiteY > 0.0 ? Yv / whiteY : Yv;
			mx = ( xx - 0.3 ) / 0.35;
			my = ( ( clampd( Y, 0.0, 1.0 ) - 0.5 ) / 0.5 ) * kLumStretchXYY;
			mz = ( yy - 0.3 ) / 0.35;
			break;
		}
	}
}

// One measured colour -> one ScenePoint (model coordinate + display colours +
// target/dE when a reference exists); silently drops no-data samples.
void C3DColorView::AppendMeasure(const CColor & c, double whiteY, CColorReference & ref,
								 const CColor & dETarget, const CColor & markerTarget,
								 bool isGS, double ywForDE, const wchar_t * label,
								 int srcType, int srcA, int srcB, int srcC,
								 double dEOverride)
{
	if ( !c.isValid() )
		return;
	ColorXYZ xyz = c.GetXYZValue();
	if ( !xyz.isValid() )
		return;

	// Model geometry is normalised by m_lumTop (peak in HDR-10), while dE and
	// the white-relative ratios keep using whiteY / ywForDE - the calibration
	// math stays anchored to diffuse white, only the plot axis grows.
	double mx, my, mz;
	ToModel( xyz, m_lumTop > 0.0 ? m_lumTop : whiteY, ref, mx, my, mz );

	ScenePoint p;
	p.mx = (float)mx; p.my = (float)my; p.mz = (float)mz;
	p.trueColor = RgbSwatch( c.GetRGBValue( ref ) );
	p.tx = p.ty = p.tz = 0.0f;
	p.dE = 0.0f;
	p.hasTarget = false;
	p.label = label ? label : L"";
	p.srcType = (BYTE)srcType;
	p.srcA = (short)srcA;
	p.srcB = (short)srcB;
	p.srcC = (short)srcC;
	ColorxyY mxyY = c.GetxyYValue();
	p.mcx = (float)mxyY[0]; p.mcy = (float)mxyY[1];
	p.mYr = (float)( whiteY > 0.0 ? mxyY[2] / whiteY : mxyY[2] );
	p.tcx = p.tcy = p.tYr = 0.0f;

	// A (near-)black measurement has no defined chromaticity (X+Y+Z ~ 0), so a
	// chromaticity-based dE against it is meaningless -- it shows up as a bogus
	// dE of ~100 (full L* span). Give such samples no target.
	bool blackish = ( xyz[0] + xyz[1] + xyz[2] ) < 1e-6 || ywForDE <= 0.0;

	if ( dETarget.isValid() && markerTarget.isValid() && !blackish )
	{
		// Same GetDeltaE conventions as the measures grid (MainView GetItemText):
		// YWhiteRef always 1.0, gw_Weight forced to 3 in HDR mode 5. Profile points
		// pass a precomputed dE (ComputeProfileDE) so the viewer, the summary pane
		// and the RGB-levels widget can never disagree.
		int gw = ( GetConfig()->m_GammaOffsetType == 5 ) ? 3 : GetConfig()->gw_Weight;
		p.dE = ( dEOverride >= 0.0 ) ? (float)dEOverride
									 : (float)c.GetDeltaE( ywForDE, dETarget, 1.0, ref, GetConfig()->m_dE_form, isGS, gw );
		if ( !( p.dE == p.dE ) || p.dE < 0.0f )   // NaN / negative: no usable dE
		{
			m_points.push_back( p );
			return;
		}

		// Marker target is relative to white=1; scale into measured units so it
		// plots on the same axes as the measured points.
		ColorXYZ t = markerTarget.GetXYZValue();
		ColorXYZ ta( t[0] * whiteY, t[1] * whiteY, t[2] * whiteY );
		double tx, ty, tz;
		ToModel( ta, m_lumTop > 0.0 ? m_lumTop : whiteY, ref, tx, ty, tz );
		p.tx = (float)tx; p.ty = (float)ty; p.tz = (float)tz;
		p.hasTarget = true;
		ColorxyY txyY = markerTarget.GetxyYValue();
		p.tcx = (float)txyY[0]; p.tcy = (float)txyY[1]; p.tYr = (float)txyY[2];

		// In non-heatmap mode, colour the dot by its TARGET colour: it identifies
		// which patch this is regardless of how far off the display measured.
		p.trueColor = RgbSwatch( markerTarget.GetRGBValue( ref ) );
	}
	m_points.push_back( p );
}

// Diffuse-white luminance used to normalise the scene and scale reference
// targets into measured units. Normally the measured grayscale / prime white;
// for a STANDALONE profile capture (no grayscale run -- "# of measures: 0")
// there is none, so fall back to the diffuse-white CONVENTION: TmDiffuseWhiteNits
// in PQ-HDR, else the measured white cube node (100/100/100 = diffuse white in
// SDR). Without this whiteY defaults to 1.0 and profile targets scale wildly
// out of bounds (measured in nits vs a white-relative reference).
static double SceneDiffuseWhiteY( CMeasure * pM )
{
	// HDR-10: the diffuse-white NORMALISER is the tone-mapped diffuse white, NOT
	// the measured 100% white -- in PQ, 100% is PEAK (up to ~100x diffuse). The
	// references are built diffuse-white-relative, so the plot must normalise to
	// the same diffuse white or bright patches/targets fly out of scale.
	if ( GetConfig()->m_GammaOffsetType == 5 )
	{
		double tm = TmDiffuseWhiteNits( pM->GetOnOffWhite(), pM->GetOnOffBlack() );
		// never fall through to the SDR path here: its measured-white fallbacks
		// return PEAK white in PQ (up to ~100x diffuse), which would collapse
		// the whole scene toward the floor. Same fallback GetHDRRefScale uses.
		return ( tm > 0.0 ) ? tm : 94.37844;
	}
	// SDR: measured grayscale / prime white (100% == diffuse == peak); for a
	// standalone profile fall back to the measured white cube node.
	CColor cw = pM->GetPrimeWhite();
	if ( !cw.isValid() || cw.GetY() <= 0.0 )
		cw = pM->GetOnOffWhite();
	if ( cw.isValid() && cw.GetY() > 0.0 )
		return cw.GetY();
	if ( pM->HasProfileMeasures() )
	{
		int cubeN = pM->GetProfileCubeSize();
		int wi = cubeN * cubeN * cubeN - 1;
		if ( wi >= 0 && wi < pM->GetProfileMeasureSize() )
		{
			CColor w = pM->GetProfileMeasure( wi );
			if ( w.isValid() && w.GetY() > 0.0 )
				return w.GetY();
		}
	}
	return 1.0;
}

void C3DColorView::BuildScene()
{
	m_points.clear();
	m_gamutValid = false;
	m_baseValid  = false;
	m_selected   = -1;    // indices are invalid after a rebuild

	CDataSetDoc * pDoc = GetDocument();
	if ( pDoc == NULL )
		return;
	CMeasure * pMeasure = pDoc->GetMeasure();
	if ( pMeasure == NULL )
		return;

	CColorReference ref = GetColorReference();

	// White luminance reference for L*a*b* / xyY normalisation (mirrors the CIE
	// chart: prefer the pseudo-colour-space prime white, else grayscale white;
	// for a standalone profile it falls back to the diffuse-white convention).
	double whiteY = SceneDiffuseWhiteY( pMeasure );

	// Vertical axis normaliser: peak in HDR-10 (measurements legitimately
	// exceed diffuse white up to TargetMaxL), diffuse white otherwise.
	m_lumTop = whiteY;
	if ( GetConfig()->m_GammaOffsetType == 5 && GetConfig()->m_TargetMaxL > whiteY )
		m_lumTop = GetConfig()->m_TargetMaxL;

	int i, n;
	ColorxyY wChroma( ref.GetWhite() );
	const int  gammaMode = GetConfig()->m_GammaOffsetType;
	const int  dEgray    = GetConfig()->m_dE_gray;
	const bool bRound    = GetConfig()->m_bUseRoundDown != FALSE;
	const bool b10bit    = GetConfig()->GetUse10bitLevels() != FALSE;

	// Grayscale (+ near-black/white): reference chromaticity is the target white;
	// the reference luminance and the YWhite passed to GetDeltaE follow the
	// measures grid's m_dE_gray convention.
	n = pMeasure->GetGrayScaleSize();
	double gammaOpt = 2.2, gammaOffset = 0.0;
	if ( n > 0 && dEgray == 1 && gammaMode < 4 )
		pDoc->ComputeGammaAndOffset( &gammaOpt, &gammaOffset, 1, 1, n, false );
	for ( i = 0; i < n; i++ )
	{
		CColor c = pMeasure->GetGray( i );
		if ( !c.isValid() )
			continue;
		double measRatio = whiteY > 0.0 ? c[1] / whiteY : c[1];
		double markRatio = measRatio;         // default: chroma-only target
		if ( dEgray == 1 )
		{
			// theoretical luminance from the target gamma / EOTF curve
			double x    = pMeasure->GetGrayPercent( i, bRound, b10bit );
			double valx = GrayLevelToGrayProp( x, bRound, b10bit );
			if ( gammaMode >= 4 )
			{
				double L = getL_EOTF( valx, pMeasure->GetOnOffWhite(), pMeasure->GetOnOffBlack(),
									  GetConfig()->m_GammaRel, GetConfig()->m_Split, gammaMode,
									  GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL,
									  GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL, GetConfig()->m_useToneMap,
									  FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS,
									  GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1 );
				markRatio = ( gammaMode == 5 ) ? L / 100.0 : L;
			}
			else
			{
				double vx = ( valx + gammaOffset ) / ( 1.0 + gammaOffset );
				markRatio = pow( vx, GetConfig()->m_useMeasuredGamma ? GetConfig()->m_GammaAvg : GetConfig()->m_GammaRef );
			}
		}
		CColor dET, markT;
		double ywForDE = whiteY;
		if ( dEgray > 0 || GetConfig()->m_dE_form == 5 )
			dET.SetxyYValue( wChroma[0], wChroma[1], ( dEgray == 2 || GetConfig()->m_dE_form == 5 ) ? measRatio : markRatio );
		else
		{
			dET.SetxyYValue( wChroma[0], wChroma[1], 1.0 );   // chroma-only: luminance cancels
			ywForDE = c[1];
		}
		markT.SetxyYValue( wChroma[0], wChroma[1], markRatio );
		wchar_t lbl[48];
		swprintf( lbl, 48, L"Gray %d%%", (int)( pMeasure->GetGrayPercent( i, bRound, b10bit ) + 0.5 ) );
		AppendMeasure( c, whiteY, ref, dET, markT, true, ywForDE, lbl, SRC_GRAY, i );
	}

	// Near-black / near-white: chroma-only grayscale targets at the measured level.
	for ( int nb = 0; nb < 2; nb++ )
	{
		n = nb ? pMeasure->GetNearWhiteScaleSize() : pMeasure->GetNearBlackScaleSize();
		for ( i = 0; i < n; i++ )
		{
			CColor c = nb ? pMeasure->GetNearWhite( i ) : pMeasure->GetNearBlack( i );
			if ( !c.isValid() )
				continue;
			CColor dET, markT;
			dET.SetxyYValue( wChroma[0], wChroma[1], 1.0 );
			markT.SetxyYValue( wChroma[0], wChroma[1], whiteY > 0.0 ? c[1] / whiteY : c[1] );
			wchar_t lbl[48];
			swprintf( lbl, 48, nb ? L"Near white #%d" : L"Near black #%d", i + 1 );
			AppendMeasure( c, whiteY, ref, dET, markT, true, c[1], lbl, nb ? SRC_NEARWHITE : SRC_NEARBLACK, i );
		}
	}

	static const wchar_t * primName[3] = { L"Red primary",      L"Green primary",  L"Blue primary" };
	static const wchar_t * secName[3]  = { L"Yellow secondary", L"Cyan secondary", L"Magenta secondary" };
	for ( i = 0; i < 3; i++ )
	{
		CColor refP = pMeasure->GetRefPrimary( i );
		AppendMeasure( pMeasure->GetPrimary( i ), whiteY, ref, refP, refP, false, whiteY, primName[i], SRC_PRIMARY, i );
		CColor refS = pMeasure->GetRefSecondary( i );
		AppendMeasure( pMeasure->GetSecondary( i ), whiteY, ref, refS, refS, false, whiteY, secName[i], SRC_SECONDARY, i );
	}

	// Saturation sweeps: plot EVERY measured stimulus level from the store, not
	// just the active one, so the full color-volume envelope is visible. The
	// label and per-level reference (GetRefSat with the level's amplitude) make
	// each level distinct; srcC records the level for click-to-inspect.
	// HDR-10: GetRefSat/GetRefCC24Sat return the 1.0 = 10000 nits convention;
	// the scene needs the diffuse-white-relative convention (YWhiteRef is 1.0
	// in AppendMeasure). The scale is 10000 / tone-mapped white - the legacy
	// 105.95640 with tone mapping off (* 100 for the Mascior-style HDR CC
	// sets, matching the measures grid).
	const bool hdr10Refs = ( GetConfig()->m_GammaOffsetType == 5 );
	const double hdrRefScale = hdr10Refs ? pMeasure->GetHDRRefScale() : 1.0;
	const bool satSpecial = ( ref.m_standard == HDTVa || ref.m_standard == HDTVb );
	static const wchar_t * hueName[6] = { L"Red", L"Green", L"Blue", L"Yellow", L"Cyan", L"Magenta" };
	int nSatLevels = pMeasure->GetSatLevelCount();
	for ( int L = 0; L < nSatLevels; L++ )
	{
		const CSatLevelSet & set = pMeasure->GetSatLevelSet( L );
		double stim   = set.stimLevel;
		int    stimPct = (int)( stim * 100.0 + 0.5 );
		int    nStep  = (int) set.sat[0].size();
		for ( i = 0; i < nStep; i++ )
		{
			double satRatio = ( nStep > 1 ) ? (double)i / (double)( nStep - 1 ) : 1.0;
			// GetRefSat hue order: 0=R 1=G 2=B 3=Y 4=C 5=M
			for ( int h = 0; h < 6; h++ )
			{
				if ( i >= (int) set.sat[h].size() )
					continue;
				CColor refC = pMeasure->GetRefSat( h, satRatio, satSpecial, stim );
				if ( hdr10Refs && refC.isValid() )
				{
					refC.SetX( refC.GetX() * hdrRefScale );
					refC.SetY( refC.GetY() * hdrRefScale );
					refC.SetZ( refC.GetZ() * hdrRefScale );
				}
				wchar_t lbl[64];
				swprintf( lbl, 64, L"%s %d%% sat @ %d%% stim", hueName[h], (int)( satRatio * 100.0 + 0.5 ), stimPct );
				AppendMeasure( set.sat[h][i], whiteY, ref, refC, refC, false, whiteY, lbl, SRC_SAT, i, h, L );
			}
		}
	}

	// Color checker patches (count follows the configured CC pattern set).
	n = GetConfig()->GetCColorsSize();
	if ( GetConfig()->m_CCMode == AXIS )
		n = 71;
	if ( n > MAX_USER_CC_PATCH_SIZE )
		n = MAX_USER_CC_PATCH_SIZE;   // the measure arrays are allocated to this;
									  // GetCC24Sat indexes unchecked past it
	for ( i = 0; i < n; i++ )
	{
		CColor c = pMeasure->GetCC24Sat( i );
		if ( !c.isValid() )
			continue;
		CColor refC;
		pMeasure->GetRefCC24Sat( i, refC );
		if ( hdr10Refs && refC.isValid() )
		{
			double s = ( GetConfig()->m_CCMode >= MASCIOR50 && GetConfig()->m_CCMode <= CCMAXHDR ) ? 100. : hdrRefScale;
			refC.SetX( refC.GetX() * s );
			refC.SetY( refC.GetY() * s );
			refC.SetZ( refC.GetZ() * s );
		}
		wchar_t lbl[48];
		swprintf( lbl, 48, L"Color checker #%d", i + 1 );
		AppendMeasure( c, whiteY, ref, refC, refC, false, whiteY, lbl, SRC_CC24, i );
	}

	// Free measures. Shared with the incremental UPD_FREEMEASUREAPPENDED path.
	m_freeInScene = 0;
	AppendNewFreeMeasures();

	// Display profile patches (dense cube capture, the 3D-LUT source). Shared
	// with the incremental per-patch path driven by UPD_REALTIME+13.
	m_profileInScene = 0;
	AppendNewProfileMeasures();

	BuildGamut( ref );
	EnsureTongueTexture( ref );
}

// Model coords in xyY with an explicit chromaticity fallback: zero-luminance
// gamut samples (the floor rim, the black corner) still have a well-defined
// chromaticity -- that of the colour DIRECTION being scaled to zero -- which a
// plain XYZ->xyY of black cannot supply.
static void XyYModelPt(const ColorXYZ & xyz, const ColorXYZ & dirXyz, double refWhiteY,
					   double & mx, double & my, double & mz)
{
	double sum = xyz[0] + xyz[1] + xyz[2];
	const ColorXYZ & c = ( sum > 1e-9 ) ? xyz : dirXyz;
	double s2 = c[0] + c[1] + c[2];
	double xx = 0.3127, yy = 0.3290;
	if ( s2 > 1e-9 ) { xx = c[0] / s2; yy = c[1] / s2; }
	double Y = refWhiteY > 0.0 ? xyz[1] / refWhiteY : xyz[1];
	mx = ( xx - 0.3 ) / 0.35;
	my = ( ( clampd( Y, 0.0, 1.0 ) - 0.5 ) / 0.5 ) * kLumStretchXYY;
	mz = ( yy - 0.3 ) / 0.35;
}

// Tessellate the target gamut so its edges and faces follow the real (curved)
// boundary in the current space. The RGB cube's 12 edges and 6 faces are sampled
// as LINEAR combinations of the reference primaries (the exact gamut definition),
// then mapped through the same space transform as the measured points.
//
// xyY is special at the bottom: the solid's closure has a FLAT BASE -- the gamut
// triangle at Y=0 -- because any in-gamut chromaticity can be shown at arbitrarily
// low luminance. Sampling the cube uniformly makes the lower faces sag to the
// black corner instead of standing on that base (near black, every ray keeps its
// chromaticity while only luminance shrinks), so in xyY the three zero-faces are
// rebuilt as vertical walls over the triangle sides and the base rim is stored
// for outlining.
void C3DColorView::BuildGamut(CColorReference & ref)
{
	// Reference primaries already sum to the reference white, so normalise heights
	// by the reference white's OWN luminance -- not the measured white (else, with
	// measurements in absolute cd/m^2, the solid collapses toward L*=0).
	// In HDR-10 this unscaled solid is exactly the display's channel-clip
	// volume once the scene's vertical axis is normalised by TargetMaxL
	// (m_lumTop): each linear channel independently reaches TargetMaxL, so
	// e.g. a clipped full red measures KR * TargetMaxL - which lands
	// precisely on this solid's red corner (KR) under peak normalisation.
	double refWhiteY = ref.GetWhite()[1];
	if ( refWhiteY <= 0.0 ) refWhiteY = 1.0;
	ColorXYZ R = ref.GetRed(), G = ref.GetGreen(), B = ref.GetBlue();

	// HLG (mode 7): the display applies the OOTF per TRANSPORT channel -
	// nits(t) = (MaxTL-MinTL) * t^gsys + MinTL - so with gsys < 1 saturated
	// colors sit above their linear-light ratios and the achievable volume
	// bulges above the linear solid. Lift every sampled gamut point in the
	// TRANSPORT container's channels: for pseudo-spaces the display decodes
	// BT.2020 channels, not inner (P3/709) ones - lifting inner cube
	// coefficients paints a wrong (overhanging) solid; for plain spaces
	// transport == ref and this reduces to the own-channel lift. SDR/PQ: no-op.
	const bool   hlgLift  = ( GetConfig()->m_GammaOffsetType == 7 );
	const double hlgGamma = GetConfig()->m_TargetSysGamma;
	CColorReference liftRef = ContainerTransportReference( ref );
	struct HlgLifter {
		bool on; double g; CColorReference * pRef;
		ColorXYZ operator()( const ColorXYZ & xyz ) const
		{
			if ( !on )
				return xyz;
			ColorRGB t( xyz, *pRef );
			for ( int c = 0; c < 3; c++ )
			{
				double v = ( t[c] < 0.0 ) ? 0.0 : t[c];	// matrix dust
				t[c] = ( v > 0.0 && v < 1.0 ) ? pow( v, g ) : v;
			}
			return ColorXYZ( t, *pRef );
		}
	} Lift = { hlgLift, hlgGamma, &liftRef };

	// L*a*b* is cube-root compressed: uniform LINEAR sampling puts the first
	// row up from black at L* ~ 35 already, and the straight chords between
	// samples cut far inside the true (bulging) surface - dark measurements
	// (a 25% drive patch sits at L* ~ 25) then plot OUTSIDE the drawn solid.
	// Warp the sweep parameter s -> s^3 in Lab so sample rows are ~evenly
	// spaced in L* and the chords hug the real surface. Identity elsewhere.
	const bool labWarp = ( m_space == SPACE_LAB );
	struct SweepWarp {
		bool on;
		double operator()( double s ) const { return on ? s * s * s : s; }
	} Warp = { labWarp };

	const int N = kGamutN;
	m_edgeV.assign( (size_t)12 * ( N + 1 ) * 3, 0.0f );
	m_faceV.assign( (size_t)6 * ( N + 1 ) * ( N + 1 ) * 3, 0.0f );

	// 12 cube edges, each sweeping one channel between two corners.
	static const double edgeRGB[12][6] = {
		{0,0,0, 1,0,0}, {0,0,0, 0,1,0}, {0,0,0, 0,0,1},
		{1,0,0, 1,1,0}, {1,0,0, 1,0,1},
		{0,1,0, 1,1,0}, {0,1,0, 0,1,1},
		{0,0,1, 1,0,1}, {0,0,1, 0,1,1},
		{1,1,0, 1,1,1}, {0,1,1, 1,1,1}, {1,0,1, 1,1,1}
	};
	for ( int e = 0; e < 12; e++ )
		for ( int t = 0; t <= N; t++ )
		{
			double s = Warp( (double)t / N );
			double r = edgeRGB[e][0] + s * ( edgeRGB[e][3] - edgeRGB[e][0] );
			double g = edgeRGB[e][1] + s * ( edgeRGB[e][4] - edgeRGB[e][1] );
			double b = edgeRGB[e][2] + s * ( edgeRGB[e][5] - edgeRGB[e][2] );
			ColorXYZ xyz = Lift( ColorXYZ( r*R[0] + g*G[0] + b*B[0], r*R[1] + g*G[1] + b*B[1], r*R[2] + g*G[2] + b*B[2] ) );
			double mx, my, mz;
			if ( m_space == SPACE_XYY )
			{
				// Chromaticity direction = the edge's far end. Makes the three
				// black->primary edges clean vertical lines at the primary corners.
				double r2 = edgeRGB[e][3], g2 = edgeRGB[e][4], b2 = edgeRGB[e][5];
				ColorXYZ dir = Lift( ColorXYZ( r2*R[0] + g2*G[0] + b2*B[0], r2*R[1] + g2*G[1] + b2*B[1], r2*R[2] + g2*G[2] + b2*B[2] ) );
				XyYModelPt( xyz, dir, refWhiteY, mx, my, mz );
			}
			else
				ToModel( xyz, refWhiteY, ref, mx, my, mz );
			float * o = &m_edgeV[ ( (size_t)e * ( N + 1 ) + t ) * 3 ];
			o[0] = (float)mx; o[1] = (float)my; o[2] = (float)mz;
		}

	// 6 cube faces: one channel pinned to 0/1, the other two swept 0..1.
	// (xyY zero-faces become vertical walls -- see the function comment.)
	static const int    faceFixed[6] = { 0, 0, 1, 1, 2, 2 };
	static const double faceVal[6]   = { 0, 1, 0, 1, 0, 1 };
	for ( int f = 0; f < 6; f++ )
		for ( int i = 0; i <= N; i++ )
			for ( int j = 0; j <= N; j++ )
			{
				double mx, my, mz;
				if ( m_space == SPACE_XYY && faceVal[f] == 0.0 )
				{
					// Wall: j sweeps the rim segment (via the face's two top cube
					// edges), i sweeps luminance fraction floor..top; chromaticity
					// is constant along each vertical.
					double s = (double)j / N, frac = (double)i / N;
					double c1 = ( s <= 0.5 ? 1.0 : 2.0 - 2.0 * s );
					double c2 = ( s <= 0.5 ? 2.0 * s : 1.0 );
					double wr, wg, wb;
					if ( faceFixed[f] == 0 )      { wr = 0.0; wg = c1; wb = c2; }  // G -> C -> B
					else if ( faceFixed[f] == 1 ) { wg = 0.0; wr = c1; wb = c2; }  // R -> M -> B
					else                          { wb = 0.0; wr = c1; wg = c2; }  // R -> Y -> G
					// per-channel lift commutes with scaling ((frac*t)^g =
					// frac^g * t^g), so Lift(dir*frac) stays colinear with
					// Lift(dir) and the wall verticals keep their chromaticity
					ColorXYZ dir0( wr*R[0] + wg*G[0] + wb*B[0], wr*R[1] + wg*G[1] + wb*B[1], wr*R[2] + wg*G[2] + wb*B[2] );
					ColorXYZ dir = Lift( dir0 );
					ColorXYZ xyz = Lift( ColorXYZ( dir0[0] * frac, dir0[1] * frac, dir0[2] * frac ) );
					XyYModelPt( xyz, dir, refWhiteY, mx, my, mz );
				}
				else
				{
					double u = Warp( (double)i / N ), v = Warp( (double)j / N ), r, g, b;
					if ( faceFixed[f] == 0 )      { r = faceVal[f]; g = u; b = v; }
					else if ( faceFixed[f] == 1 ) { g = faceVal[f]; r = u; b = v; }
					else                          { b = faceVal[f]; r = u; g = v; }
					ColorXYZ xyz = Lift( ColorXYZ( r*R[0] + g*G[0] + b*B[0], r*R[1] + g*G[1] + b*B[1], r*R[2] + g*G[2] + b*B[2] ) );
					if ( m_space == SPACE_XYY )
						XyYModelPt( xyz, xyz, refWhiteY, mx, my, mz );
					else
						ToModel( xyz, refWhiteY, ref, mx, my, mz );
				}
				float * o = &m_faceV[ ( ( (size_t)f * ( N + 1 ) + i ) * ( N + 1 ) + j ) * 3 ];
				o[0] = (float)mx; o[1] = (float)my; o[2] = (float)mz;
			}

	// xyY: the base rim -- the gamut triangle at floor level the walls stand on.
	if ( m_space == SPACE_XYY )
	{
		ColorXYZ prim[3] = { Lift( R ), Lift( G ), Lift( B ) };
		for ( int k = 0; k < 3; k++ )
		{
			double mx, my, mz;
			XyYModelPt( ColorXYZ( 0.0, 0.0, 0.0 ), prim[k], refWhiteY, mx, my, mz );
			m_baseV[k][0] = (float)mx; m_baseV[k][1] = (float)my; m_baseV[k][2] = (float)mz;
		}
		m_baseValid = true;
	}

	m_gamutValid = true;
}

void C3DColorView::SetDEFilter(int filter)
{
	if ( filter < 0 ) filter = 0;
	if ( filter > 2 ) filter = 2;
	if ( filter == m_deFilter )
		return;
	m_deFilter = filter;
	if ( ::IsWindow( m_hWnd ) )
		Invalidate( FALSE );
}

void C3DColorView::SetSpace(int space)
{
	if ( space < 0 || space >= SPACE_COUNT || space == m_space )
		return;
	m_space = space;
	m_sceneDirty = true;          // model coords are space-dependent
	if ( ::IsWindow( m_hWnd ) )
		Invalidate( FALSE );
}

/////////////////////////////////////////////////////////////////////////////
// CIE tongue floor texture

void C3DColorView::EnsureTongueTexture(CColorReference & ref)
{
	int sig = (int)ref.m_standard;
	if ( m_texBits != NULL && m_texSig == sig )
		return;
	FreeTongueTexture();

	// Stored at 2x the old size, and RENDERED at 2x that again with a box-filter
	// downsample: the tongue boundary is a hard colour step, and bilinear
	// magnification of a hard step still shows the texel grid as scalloped
	// jaggies. Supersampling anti-aliases the edge in texture space.
	const int W = 1400, H = 1556;          // 0.9:1.0 aspect -> isotropic chromaticity px
	const int SS = 2;
	const int BW = W * SS, BH = H * SS;

	BITMAPINFO bmi;
	ZeroMemory( &bmi, sizeof( bmi ) );
	bmi.bmiHeader.biSize        = sizeof( BITMAPINFOHEADER );
	bmi.bmiHeader.biWidth       = BW;
	bmi.bmiHeader.biHeight      = -BH;   // top-down
	bmi.bmiHeader.biPlanes      = 1;
	bmi.bmiHeader.biBitCount    = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	// oversized render target (temporary)
	void * bigBits = NULL;
	HBITMAP hBig = CreateDIBSection( NULL, &bmi, DIB_RGB_COLORS, &bigBits, NULL, 0 );
	if ( hBig == NULL )
		return;
	HDC hBigDC = CreateCompatibleDC( NULL );
	if ( hBigDC == NULL )
	{
		DeleteObject( hBig );
		return;
	}
	HBITMAP hBigOld = (HBITMAP)SelectObject( hBigDC, hBig );

	DWORD * bp = (DWORD *)bigBits;
	FillDoubling( bp, (size_t)BW * BH, DibColor( 18, 18, 22 ) );   // dark floor background

	CDC * p = CDC::FromHandle( hBigDC );
	p->SetBkMode( TRANSPARENT );
	DrawCIEChartEx( p, BW, BH, TRUE, FALSE, FALSE, FALSE, TRUE /*hide nm labels*/ );
	GdiFlush();

	// the stored texture
	bmi.bmiHeader.biWidth  = W;
	bmi.bmiHeader.biHeight = -H;
	void * bits = NULL;
	m_texBmp = CreateDIBSection( NULL, &bmi, DIB_RGB_COLORS, &bits, NULL, 0 );
	m_texDC  = m_texBmp ? CreateCompatibleDC( NULL ) : NULL;
	if ( m_texDC == NULL )
	{
		if ( m_texBmp ) { DeleteObject( m_texBmp ); m_texBmp = NULL; }
		SelectObject( hBigDC, hBigOld );
		DeleteDC( hBigDC );
		DeleteObject( hBig );
		return;
	}
	m_texOld  = (HBITMAP)SelectObject( m_texDC, m_texBmp );
	m_texBits = (DWORD *)bits;
	m_texW = W;
	m_texH = H;

	// 2x2 box-filter downsample
	for ( int y = 0; y < H; y++ )
	{
		const DWORD * r0 = bp + (size_t)( y * SS )     * BW;
		const DWORD * r1 = bp + (size_t)( y * SS + 1 ) * BW;
		DWORD * dst = m_texBits + (size_t)y * W;
		for ( int x = 0; x < W; x++ )
		{
			const DWORD c0 = r0[x * SS], c1 = r0[x * SS + 1];
			const DWORD c2 = r1[x * SS], c3 = r1[x * SS + 1];
			int r = ( ( ( c0 >> 16 ) & 0xFF ) + ( ( c1 >> 16 ) & 0xFF ) + ( ( c2 >> 16 ) & 0xFF ) + ( ( c3 >> 16 ) & 0xFF ) ) >> 2;
			int g = ( ( ( c0 >> 8 ) & 0xFF )  + ( ( c1 >> 8 ) & 0xFF )  + ( ( c2 >> 8 ) & 0xFF )  + ( ( c3 >> 8 ) & 0xFF ) ) >> 2;
			int b = ( ( c0 & 0xFF ) + ( c1 & 0xFF ) + ( c2 & 0xFF ) + ( c3 & 0xFF ) ) >> 2;
			dst[x] = DibColor( r, g, b );
		}
	}

	SelectObject( hBigDC, hBigOld );
	DeleteDC( hBigDC );
	DeleteObject( hBig );

	m_texSig = sig;
}

void C3DColorView::FreeTongueTexture()
{
	if ( m_texDC != NULL )
	{
		if ( m_texOld != NULL )
			SelectObject( m_texDC, m_texOld );
		DeleteDC( m_texDC );
		m_texDC = NULL;
	}
	if ( m_texBmp != NULL )
	{
		DeleteObject( m_texBmp );
		m_texBmp = NULL;
	}
	m_texOld = NULL;
	m_texBits = NULL;
	m_texW = m_texH = 0;
	m_texSig = -1;
}

/////////////////////////////////////////////////////////////////////////////
// backbuffer

bool C3DColorView::EnsureBackbuffer(int w, int h)
{
	if ( m_memDC != NULL && m_bw == w && m_bh == h )
		return true;

	FreeBackbuffer();
	if ( w <= 0 || h <= 0 )
		return false;

	BITMAPINFO bmi;
	ZeroMemory( &bmi, sizeof( bmi ) );
	bmi.bmiHeader.biSize        = sizeof( BITMAPINFOHEADER );
	bmi.bmiHeader.biWidth       = w;
	bmi.bmiHeader.biHeight      = -h;
	bmi.bmiHeader.biPlanes      = 1;
	bmi.bmiHeader.biBitCount    = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	void * bits = NULL;
	m_memBmp = CreateDIBSection( NULL, &bmi, DIB_RGB_COLORS, &bits, NULL, 0 );
	if ( m_memBmp == NULL )
		return false;
	m_memDC = CreateCompatibleDC( NULL );
	if ( m_memDC == NULL )
	{
		DeleteObject( m_memBmp );
		m_memBmp = NULL;
		return false;
	}
	m_oldBmp = (HBITMAP)SelectObject( m_memDC, m_memBmp );
	m_bits   = (DWORD *)bits;
	m_bw     = w;
	m_bh     = h;
	m_zbuf.assign( (size_t)w * h, -1e30f );
	return true;
}

void C3DColorView::FreeBackbuffer()
{
	if ( m_memDC != NULL )
	{
		if ( m_oldBmp != NULL )
			SelectObject( m_memDC, m_oldBmp );
		DeleteDC( m_memDC );
		m_memDC = NULL;
	}
	if ( m_memBmp != NULL )
	{
		DeleteObject( m_memBmp );
		m_memBmp = NULL;
	}
	m_oldBmp = NULL;
	m_bits   = NULL;
	m_bw = m_bh = 0;
	m_zbuf.clear();
}

/////////////////////////////////////////////////////////////////////////////
// software rasterization

void C3DColorView::ProjectModel(double mx, double my, double mz,
								double & sx, double & sy, double & depth) const
{
	double x1 =  mx * m_ry + mz * m_rsy;
	double z1 = -mx * m_rsy + mz * m_ry;
	double y2 =  my * m_rp - z1 * m_rsp;
	double z2 =  my * m_rsp + z1 * m_rp;
	// mild perspective: nearer geometry (larger z2) projects slightly larger
	double den = kCamDist - z2;
	if ( den < 0.5 ) den = 0.5;
	double k = kCamDist / den;
	sx = m_cx + m_scale * x1 * k;
	sy = m_cyc - m_scale * y2 * k;
	depth = z2;                    // larger == nearer the viewer
}

// Precompute the orb shading kernel for a radius: per covered pixel, a diffuse
// factor from a sphere normal lit from the upper left, plus a small specular
// glint. Rebuilt only when the radius changes.
void C3DColorView::BuildOrbKernel(int radius)
{
	if ( m_orbKernelR == radius )
		return;
	m_orbKernelR = radius;
	m_orbKernel.clear();

	const double Lx = -0.45, Ly = -0.60, Lz = 0.66;   // normalized-ish light dir
	double r = (double)radius;
	for ( int dy = -radius; dy <= radius; dy++ )
		for ( int dx = -radius; dx <= radius; dx++ )
		{
			double d2 = ( dx * dx + dy * dy ) / ( r * r );
			if ( d2 > 1.0 )
				continue;
			double nx = dx / r, ny = dy / r, nz = sqrt( 1.0 - d2 );
			double dot = nx * Lx + ny * Ly + nz * Lz;
			if ( dot < 0.0 ) dot = 0.0;
			OrbPx px;
			px.dx = (short)dx;
			px.dy = (short)dy;
			px.shade = (float)( 0.35 + 0.65 * dot );                       // never fully black
			px.spec  = (BYTE)( dot > 0.90 ? ( dot - 0.90 ) / 0.10 * 90.0 : 0.0 );
			m_orbKernel.push_back( px );
		}
}

void C3DColorView::SplatOrb(int cx, int cy, float depth, DWORD color)
{
	if ( m_orbKernel.empty() )
		return;
	int w = m_bw, h = m_bh;
	int r = ( color >> 16 ) & 0xFF, g = ( color >> 8 ) & 0xFF, b = color & 0xFF;
	// raw pointers: MSVC Debug bounds-checks every vector[] access, which adds
	// up in this per-pixel loop
	const OrbPx * pk = &m_orbKernel[0];
	const size_t  n  = m_orbKernel.size();
	float *       zb = &m_zbuf[0];
	DWORD *       px = m_bits;
	for ( size_t k = 0; k < n; k++ )
	{
		const OrbPx & o = pk[k];
		int xx = cx + o.dx, yy = cy + o.dy;
		if ( xx < 0 || xx >= w || yy < 0 || yy >= h ) continue;
		int idx = yy * w + xx;
		if ( depth > zb[idx] )
		{
			int rr = (int)( r * o.shade ) + o.spec;
			int gg = (int)( g * o.shade ) + o.spec;
			int bb = (int)( b * o.shade ) + o.spec;
			zb[idx] = depth;
			px[idx] = DibColor( rr > 255 ? 255 : rr, gg > 255 ? 255 : gg, bb > 255 ? 255 : bb );
		}
	}
}

// Alpha-blend one pixel, z-tested against the buffer without writing z (used by
// tails/markers, which are annotations rather than occluders).
void C3DColorView::BlendPixel(int x, int y, float z, DWORD color, double alpha)
{
	if ( x < 0 || x >= m_bw || y < 0 || y >= m_bh || alpha <= 0.0 )
		return;
	int idx = y * m_bw + x;
	if ( z <= m_zbuf[idx] )
		return;
	if ( alpha > 1.0 ) alpha = 1.0;
	m_bits[idx] = BlendDib( color, m_bits[idx], (int)( alpha * 255.0 + 0.5 ) );
}

// Xiaolin Wu anti-aliased line with linear depth interpolation -- visually on a
// par with a GDI+ 1px AA line but ~nanoseconds per tail, so tails never need a
// point-count cap. Does not write z.
void C3DColorView::WuLine(double x0, double y0, float z0, double x1, double y1, float z1,
						  DWORD color, double alpha)
{
	bool steep = fabs( y1 - y0 ) > fabs( x1 - x0 );
	if ( steep ) { double t = x0; x0 = y0; y0 = t; t = x1; x1 = y1; y1 = t; }
	if ( x0 > x1 )
	{
		double t = x0; x0 = x1; x1 = t;
		t = y0; y0 = y1; y1 = t;
		float tz = z0; z0 = z1; z1 = tz;
	}
	double dx = x1 - x0;
	if ( dx < 1e-9 )
		return;
	double grad = ( y1 - y0 ) / dx;
	int    xs   = (int)floor( x0 + 0.5 );
	int    xe   = (int)floor( x1 + 0.5 );
	double y    = y0 + ( xs - x0 ) * grad;
	for ( int x = xs; x <= xe; x++, y += grad )
	{
		double f  = y - floor( y );
		int    yi = (int)floor( y );
		float  z  = z0 + (float)( ( x - x0 ) / dx ) * ( z1 - z0 );
		if ( steep )
		{
			BlendPixel( yi,     x, z, color, alpha * ( 1.0 - f ) );
			BlendPixel( yi + 1, x, z, color, alpha * f );
		}
		else
		{
			BlendPixel( x, yi,     z, color, alpha * ( 1.0 - f ) );
			BlendPixel( x, yi + 1, z, color, alpha * f );
		}
	}
}

// Axis-aligned cross marking a target position: crisp without anti-aliasing and
// unmistakable for a measurement dot. Slightly translucent so it annotates
// rather than shouts.
void C3DColorView::SplatCross(int cx, int cy, float z, DWORD color, int arm)
{
	const double alpha = 0.72;
	for ( int d = -arm; d <= arm; d++ )
	{
		BlendPixel( cx + d, cy, z, color, alpha );
		if ( d != 0 )
			BlendPixel( cx, cy + d, z, color, alpha );
	}
}

// Compute the inclusive pixel span [xl, xr] a triangle covers on scanline py,
// given the barycentric row bases/slopes, clipped to [minx, maxx]. Returns
// false for an empty row. Doing this analytically (three half-plane bounds)
// makes rasterization cost proportional to COVERED pixels: with the old
// bounding-box scan, a zoomed-in triangle whose box clamps to the whole window
// barycentric-tested millions of pixels only to reject most of them.
static inline bool RowSpan(const double base[3], const double slope[3],
						   int minx, int maxx, int & xl, int & xr)
{
	double lo = (double)minx, hi = (double)maxx;
	for ( int e = 0; e < 3; e++ )
	{
		if ( fabs( slope[e] ) < 1e-12 )
		{
			if ( base[e] < 0.0 )
				return false;                       // whole row outside this edge
		}
		else
		{
			double x = -base[e] / slope[e];
			if ( slope[e] > 0.0 ) { if ( x > lo ) lo = x; }
			else                  { if ( x < hi ) hi = x; }
		}
	}
	xl = (int)ceil ( lo - 1e-9 );
	xr = (int)floor( hi + 1e-9 );
	if ( xl < minx ) xl = minx;
	if ( xr > maxx ) xr = maxx;
	return xl <= xr;
}

// Translucent flat triangle: z-tested against the buffer but does NOT write z,
// so opaque points already drawn stay crisp and later faces still blend.
void C3DColorView::RasterTriFlat(const double v[3][3], DWORD color, int alpha)
{
	int w = m_bw, h = m_bh;
	double x0 = v[0][0], y0 = v[0][1], z0 = v[0][2];
	double x1 = v[1][0], y1 = v[1][1], z1 = v[1][2];
	double x2 = v[2][0], y2 = v[2][1], z2 = v[2][2];
	double denom = ( y1 - y2 ) * ( x0 - x2 ) + ( x2 - x1 ) * ( y0 - y2 );
	if ( fabs( denom ) < 1e-9 ) return;
	double invDen = 1.0 / denom;

	int minx = (int)floor( min3( x0, x1, x2 ) ), maxx = (int)ceil( max3( x0, x1, x2 ) );
	int miny = (int)floor( min3( y0, y1, y2 ) ), maxy = (int)ceil( max3( y0, y1, y2 ) );
	if ( minx < 0 ) minx = 0; if ( miny < 0 ) miny = 0;
	if ( maxx > w - 1 ) maxx = w - 1; if ( maxy > h - 1 ) maxy = h - 1;
	if ( minx > maxx || miny > maxy ) return;

	// barycentrics as linear functions of the pixel: b = C + A*px + B*py
	double A0 = ( y1 - y2 ) * invDen, B0 = ( x2 - x1 ) * invDen;
	double C0 = ( -( y1 - y2 ) * x2 - ( x2 - x1 ) * y2 ) * invDen;
	double A1 = ( y2 - y0 ) * invDen, B1 = ( x0 - x2 ) * invDen;
	double C1 = ( -( y2 - y0 ) * x2 - ( x0 - x2 ) * y2 ) * invDen;
	double slope[3] = { A0, A1, -( A0 + A1 ) };

	float * zb = &m_zbuf[0];
	for ( int py = miny; py <= maxy; py++ )
	{
		double base[3];
		base[0] = C0 + B0 * py;
		base[1] = C1 + B1 * py;
		base[2] = 1.0 - base[0] - base[1];
		int xl, xr;
		if ( !RowSpan( base, slope, minx, maxx, xl, xr ) )
			continue;
		double b0 = base[0] + slope[0] * xl;
		double b1 = base[1] + slope[1] * xl;
		double b2 = base[2] + slope[2] * xl;
		double z    = b0 * z0 + b1 * z1 + b2 * z2;
		double dzdx = slope[0] * z0 + slope[1] * z1 + slope[2] * z2;
		int rowIdx = py * w;
		for ( int px = xl; px <= xr; px++, z += dzdx )
		{
			int idx = rowIdx + px;
			if ( z > zb[idx] )
				m_bits[idx] = BlendDib( color, m_bits[idx], alpha );
		}
	}
}

// Opaque textured triangle (the CIE floor): z-tested AND writes z. Bilinear
// sampling: nearest-neighbour looked blocky/jagged as soon as the floor was
// magnified by zooming in.
void C3DColorView::RasterTriTex(const double v[3][3], const double uv[3][2])
{
	if ( m_texBits == NULL ) return;
	int w = m_bw, h = m_bh;
	double x0 = v[0][0], y0 = v[0][1], z0 = v[0][2];
	double x1 = v[1][0], y1 = v[1][1], z1 = v[1][2];
	double x2 = v[2][0], y2 = v[2][1], z2 = v[2][2];
	double denom = ( y1 - y2 ) * ( x0 - x2 ) + ( x2 - x1 ) * ( y0 - y2 );
	if ( fabs( denom ) < 1e-9 ) return;
	double invDen = 1.0 / denom;

	int minx = (int)floor( min3( x0, x1, x2 ) ), maxx = (int)ceil( max3( x0, x1, x2 ) );
	int miny = (int)floor( min3( y0, y1, y2 ) ), maxy = (int)ceil( max3( y0, y1, y2 ) );
	if ( minx < 0 ) minx = 0; if ( miny < 0 ) miny = 0;
	if ( maxx > w - 1 ) maxx = w - 1; if ( maxy > h - 1 ) maxy = h - 1;
	if ( minx > maxx || miny > maxy ) return;

	double A0 = ( y1 - y2 ) * invDen, B0 = ( x2 - x1 ) * invDen;
	double C0 = ( -( y1 - y2 ) * x2 - ( x2 - x1 ) * y2 ) * invDen;
	double A1 = ( y2 - y0 ) * invDen, B1 = ( x0 - x2 ) * invDen;
	double C1 = ( -( y2 - y0 ) * x2 - ( x0 - x2 ) * y2 ) * invDen;
	double slope[3] = { A0, A1, -( A0 + A1 ) };

	const int    TW = m_texW, TH = m_texH;
	const double USCALE = (double)( TW - 1 ), VSCALE = (double)( TH - 1 );
	float *       zb = &m_zbuf[0];
	const DWORD * tex = m_texBits;

	for ( int py = miny; py <= maxy; py++ )
	{
		double base[3];
		base[0] = C0 + B0 * py;
		base[1] = C1 + B1 * py;
		base[2] = 1.0 - base[0] - base[1];
		int xl, xr;
		if ( !RowSpan( base, slope, minx, maxx, xl, xr ) )
			continue;
		double b0 = base[0] + slope[0] * xl;
		double b1 = base[1] + slope[1] * xl;
		double b2 = base[2] + slope[2] * xl;
		double z    = b0 * z0 + b1 * z1 + b2 * z2;
		double dzdx = slope[0] * z0 + slope[1] * z1 + slope[2] * z2;
		double u    = ( b0 * uv[0][0] + b1 * uv[1][0] + b2 * uv[2][0] ) * USCALE;
		double dudx = ( slope[0] * uv[0][0] + slope[1] * uv[1][0] + slope[2] * uv[2][0] ) * USCALE;
		double tvv  = ( b0 * uv[0][1] + b1 * uv[1][1] + b2 * uv[2][1] ) * VSCALE;
		double dvdx = ( slope[0] * uv[0][1] + slope[1] * uv[1][1] + slope[2] * uv[2][1] ) * VSCALE;
		int rowIdx = py * w;
		for ( int px = xl; px <= xr; px++, z += dzdx, u += dudx, tvv += dvdx )
		{
			int idx = rowIdx + px;
			if ( z > zb[idx] )
			{
				// bilinear 4-tap
				double cu = u < 0.0 ? 0.0 : ( u > USCALE ? USCALE : u );
				double cv = tvv < 0.0 ? 0.0 : ( tvv > VSCALE ? VSCALE : tvv );
				int tx = (int)cu, ty = (int)cv;
				int tx1 = tx + 1 < TW ? tx + 1 : tx;
				int ty1 = ty + 1 < TH ? ty + 1 : ty;
				int fx = (int)( ( cu - tx ) * 256.0 ), fy = (int)( ( cv - ty ) * 256.0 );
				DWORD c00 = tex[ty * TW + tx],  c10 = tex[ty * TW + tx1];
				DWORD c01 = tex[ty1 * TW + tx], c11 = tex[ty1 * TW + tx1];
				int r = ( ( ( ( c00 >> 16 ) & 0xFF ) * ( 256 - fx ) + ( ( c10 >> 16 ) & 0xFF ) * fx ) * ( 256 - fy )
					  +   ( ( ( c01 >> 16 ) & 0xFF ) * ( 256 - fx ) + ( ( c11 >> 16 ) & 0xFF ) * fx ) * fy ) >> 16;
				int g = ( ( ( ( c00 >> 8 ) & 0xFF ) * ( 256 - fx ) + ( ( c10 >> 8 ) & 0xFF ) * fx ) * ( 256 - fy )
					  +   ( ( ( c01 >> 8 ) & 0xFF ) * ( 256 - fx ) + ( ( c11 >> 8 ) & 0xFF ) * fx ) * fy ) >> 16;
				int b = ( ( ( c00 & 0xFF ) * ( 256 - fx ) + ( c10 & 0xFF ) * fx ) * ( 256 - fy )
					  +   ( ( c01 & 0xFF ) * ( 256 - fx ) + ( c11 & 0xFF ) * fx ) * fy ) >> 16;
				// Blend the tongue toward the (dark) background so the floor reads
				// as a translucent surface rather than a bright opaque one.
				m_bits[idx] = BlendDib( DibColor( r, g, b ), m_bits[idx], kFloorAlpha );
				zb[idx] = (float)z;
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// rendering

void C3DColorView::Render(const CRect& rc)
{
	int w = rc.Width(), h = rc.Height();
	if ( !EnsureBackbuffer( w, h ) )
		return;
	EnsureGdiplus();

	if ( m_sceneDirty )
	{
		BuildScene();
		m_sceneDirty = false;
	}

	// Clear colour + depth: at fullscreen this touches ~2M pixels per buffer per
	// frame, and a plain element loop is a real cost (especially in Debug builds).
	size_t total = (size_t)w * h;
	FillDoubling( m_bits, total, DibColor( 24, 24, 28 ) );
	FillDoubling( &m_zbuf[0], total, -1e30f );

	// per-frame camera basis
	m_cx    = w * 0.5 + m_panX;
	m_cyc   = h * 0.5 + m_panY;
	m_scale = 0.42 * ( w < h ? w : h ) * m_zoom;
	m_ry = cos( m_yaw );  m_rsy = sin( m_yaw );
	m_rp = cos( m_pitch ); m_rsp = sin( m_pitch );

	// 1) CIE tongue floor (xyY only), just below the Y=0 plane. Rasterized as a
	// grid of small quads so the affine texture mapping stays accurate under
	// the perspective projection.
	if ( m_showFloor && m_space == SPACE_XYY && m_texBits != NULL )
	{
		const int FN = kFloorGridN;
		for ( int gi = 0; gi < FN; gi++ )
			for ( int gj = 0; gj < FN; gj++ )
			{
				double fx[4], fy[4], sv[4][3], uv[4][2];
				fx[0] = fx[3] = kFloorX0 + ( kFloorX1 - kFloorX0 ) * gi / FN;
				fx[1] = fx[2] = kFloorX0 + ( kFloorX1 - kFloorX0 ) * ( gi + 1 ) / FN;
				fy[0] = fy[1] = kFloorY0 + ( kFloorY1 - kFloorY0 ) * gj / FN;
				fy[2] = fy[3] = kFloorY0 + ( kFloorY1 - kFloorY0 ) * ( gj + 1 ) / FN;
				for ( int k = 0; k < 4; k++ )
				{
					double mx = ( fx[k] - 0.3 ) / 0.35, mz = ( fy[k] - 0.3 ) / 0.35;
					ProjectModel( mx, -kLumStretchXYY - kFloorDrop, mz, sv[k][0], sv[k][1], sv[k][2] );
					uv[k][0] = ( fx[k] + 0.075 ) / 0.9;
					uv[k][1] = 1.0 - ( fy[k] + 0.05 ) / 1.0;
				}
				double a[3][3]   = { { sv[0][0], sv[0][1], sv[0][2] }, { sv[1][0], sv[1][1], sv[1][2] }, { sv[2][0], sv[2][1], sv[2][2] } };
				double auv[3][2] = { { uv[0][0], uv[0][1] }, { uv[1][0], uv[1][1] }, { uv[2][0], uv[2][1] } };
				RasterTriTex( a, auv );
				double b[3][3]   = { { sv[0][0], sv[0][1], sv[0][2] }, { sv[2][0], sv[2][1], sv[2][2] }, { sv[3][0], sv[3][1], sv[3][2] } };
				double buv[3][2] = { { uv[0][0], uv[0][1] }, { uv[2][0], uv[2][1] }, { uv[3][0], uv[3][1] } };
				RasterTriTex( b, buv );
			}
	}

	// Shared tolerance thresholds drive both the heat colours and the display
	// filter (-1 = filter off); values track the preset in Advanced settings.
	double deGood, deWarn;
	GetConfig()->GetDEThresholds( deGood, deWarn );
	double deThr = ( m_deFilter == 0 ) ? -1.0 : ( m_deFilter == 1 ? deGood : deWarn );

	// 2) measured points as shaded orbs (opaque, z-tested); smaller for dense clouds
	BuildOrbKernel( ( m_points.size() > kOrbDenseThreshold ) ? 2 : 4 );
	for ( size_t k = 0; k < m_points.size(); k++ )
	{
		const ScenePoint & P = m_points[k];
		if ( !m_showProfilePts && P.srcType == SRC_PROFILE )
			continue;
		if ( deThr >= 0.0 && P.hasTarget && P.dE < deThr )
			continue;
		double sx, sy, dep;
		ProjectModel( P.mx, P.my, P.mz, sx, sy, dep );
		DWORD clr;
		if ( m_pointColor == PTCOLOR_PLAIN )              clr = DibColor( 235, 235, 238 );
		else if ( m_pointColor == PTCOLOR_DE && P.hasTarget ) clr = HeatColor( P.dE, deGood, deWarn );
		else                                               clr = P.trueColor;
		SplatOrb( (int)( sx + 0.5 ), (int)( sy + 0.5 ), (float)dep, clr );
	}

	// 2b) target tails + cross markers, software-rasterized with the same depth
	// buffer -- no point-count cap, and correctly occluded by nearer geometry.
	if ( m_showTails )
	{
		const DWORD tailClr  = DibColor( 200, 200, 212 );   // brighter: subtle tails vanished when zoomed out
		const DWORD crossClr = DibColor( 204, 204, 210 );   // ~80% white
		for ( size_t k = 0; k < m_points.size(); k++ )
		{
			const ScenePoint & P = m_points[k];
			if ( !m_showProfilePts && P.srcType == SRC_PROFILE )
				continue;
			if ( !P.hasTarget )
				continue;
			if ( deThr >= 0.0 && P.dE < deThr )
				continue;
			double sx, sy, sd, tx, ty, td;
			ProjectModel( P.mx, P.my, P.mz, sx, sy, sd );
			ProjectModel( P.tx, P.ty, P.tz, tx, ty, td );
			double ddx = sx - tx, ddy = sy - ty;
			if ( ddx * ddx + ddy * ddy > 0.5 )   // skip sub-pixel tails (invisible)
				WuLine( tx, ty, (float)td, sx, sy, (float)sd, tailClr, 0.8 );
			SplatCross( (int)( tx + 0.5 ), (int)( ty + 0.5 ), (float)td, crossClr, 3 );
		}
	}

	// 3) target gamut translucent faces (tessellated; back-to-front, z-tested, no z write)
	if ( m_shadeGamut && m_gamutValid )
	{
		const int N = kGamutN, stride = N + 1;
		size_t nfv = (size_t)6 * stride * stride;
		std::vector<double> fs( nfv * 3 );      // projected face vertices (sx,sy,depth)
		for ( size_t vi = 0; vi < nfv; vi++ )
		{
			const float * m = &m_faceV[vi * 3];
			ProjectModel( m[0], m[1], m[2], fs[vi*3], fs[vi*3+1], fs[vi*3+2] );
		}
		struct Tri { double v[3][3]; double depth; };
		std::vector<Tri> tris;
		tris.reserve( (size_t)6 * N * N * 2 );
		for ( int f = 0; f < 6; f++ )
			for ( int a = 0; a < N; a++ )
				for ( int b = 0; b < N; b++ )
				{
					size_t i0 = ( (size_t)f * stride + a )     * stride + b;
					size_t i1 = ( (size_t)f * stride + a + 1 ) * stride + b;
					size_t i2 = ( (size_t)f * stride + a + 1 ) * stride + b + 1;
					size_t i3 = ( (size_t)f * stride + a )     * stride + b + 1;
					const double *p0=&fs[i0*3], *p1=&fs[i1*3], *p2=&fs[i2*3], *p3=&fs[i3*3];
					Tri t1, t2; int c;
					for ( c = 0; c < 3; c++ ) { t1.v[0][c]=p0[c]; t1.v[1][c]=p1[c]; t1.v[2][c]=p2[c]; }
					t1.depth = ( p0[2]+p1[2]+p2[2] ) / 3.0; tris.push_back( t1 );
					for ( c = 0; c < 3; c++ ) { t2.v[0][c]=p0[c]; t2.v[1][c]=p2[c]; t2.v[2][c]=p3[c]; }
					t2.depth = ( p0[2]+p2[2]+p3[2] ) / 3.0; tris.push_back( t2 );
				}
		if ( m_baseValid )   // xyY: the flat base the walls stand on
		{
			Tri bt;
			for ( int c = 0; c < 3; c++ )
				ProjectModel( m_baseV[c][0], m_baseV[c][1], m_baseV[c][2], bt.v[c][0], bt.v[c][1], bt.v[c][2] );
			bt.depth = ( bt.v[0][2] + bt.v[1][2] + bt.v[2][2] ) / 3.0;
			tris.push_back( bt );
		}
		std::sort( tris.begin(), tris.end(), []( const Tri & A, const Tri & B ){ return A.depth < B.depth; } );
		for ( size_t k = 0; k < tris.size(); k++ )
			RasterTriFlat( tris[k].v, DibColor( 205, 210, 220 ), kGamutFaceAlpha );
	}

	// 4) chrome: gamut wireframe, axes, labels, overlay (GDI+)
	{
		Gdiplus::Graphics g( m_memDC );
		g.SetSmoothingMode( Gdiplus::SmoothingModeAntiAlias );
		g.SetTextRenderingHint( Gdiplus::TextRenderingHintAntiAlias );

		if ( m_showGamut && m_gamutValid )
		{
			const int N = kGamutN, stride = N + 1;
			Gdiplus::Pen gpen( Gdiplus::Color( 195, 230, 235, 245 ), 1.3f );
			std::vector<Gdiplus::PointF> poly( stride );
			for ( int e = 0; e < 12; e++ )
			{
				for ( int t = 0; t <= N; t++ )
				{
					const float * m = &m_edgeV[ ( (size_t)e * stride + t ) * 3 ];
					double sx, sy, dep; ProjectModel( m[0], m[1], m[2], sx, sy, dep );
					poly[t] = Gdiplus::PointF( (float)sx, (float)sy );
				}
				g.DrawLines( &gpen, &poly[0], stride );
			}

			// xyY: outline the base rim (the gamut triangle the walls stand on)
			// like any other fixed edge -- steady from every angle.
			if ( m_baseValid )
			{
				Gdiplus::PointF bp[4];
				for ( int k = 0; k < 3; k++ )
				{
					double sx, sy, dep;
					ProjectModel( m_baseV[k][0], m_baseV[k][1], m_baseV[k][2], sx, sy, dep );
					bp[k] = Gdiplus::PointF( (float)sx, (float)sy );
				}
				bp[3] = bp[0];
				g.DrawLines( &gpen, bp, 4 );
			}
		}

		// axes: a corner frame just outside the plotted region (typical 3D-chart
		// style) instead of lines through the solid. Three edges meet at a floor
		// corner: two along the floor, one vertical.
		const wchar_t * labs[3];
		double lo[3], hi[3];   // model-space bounds of the plotted region
		if ( m_space == SPACE_RGB )
		{
			labs[0] = L"R"; labs[1] = L"G"; labs[2] = L"B";
			lo[0] = lo[1] = lo[2] = -1.0; hi[0] = hi[1] = hi[2] = 1.0;
		}
		else if ( m_space == SPACE_LAB )
		{
			labs[0] = L"a*"; labs[1] = L"L*"; labs[2] = L"b*";
			lo[0] = lo[1] = lo[2] = -1.0; hi[0] = hi[1] = hi[2] = 1.0;
		}
		else
		{
			labs[0] = L"x"; labs[1] = L"Y"; labs[2] = L"y";
			lo[0] = ( kFloorX0 - 0.3 ) / 0.35; hi[0] = ( kFloorX1 - 0.3 ) / 0.35;
			lo[2] = ( kFloorY0 - 0.3 ) / 0.35; hi[2] = ( kFloorY1 - 0.3 ) / 0.35;
			lo[1] = -kLumStretchXYY;           hi[1] = kLumStretchXYY;
		}

		Gdiplus::Pen        axisPen( Gdiplus::Color( 170, 150, 150, 160 ), 1.4f );
		Gdiplus::SolidBrush lblBrush( Gdiplus::Color( 235, 225, 225, 230 ) );
		Gdiplus::Font       font( L"Segoe UI", 8.5f );
		const double mgn = 0.06;   // push the frame outward so it clears the volume
		double corner[3] = { lo[0] - mgn, lo[1], lo[2] - mgn };
		double ox, oy, od;
		ProjectModel( corner[0], corner[1], corner[2], ox, oy, od );
		for ( int a = 0; a < 3; a++ )
		{
			double e[3] = { corner[0], corner[1], corner[2] };
			e[a] = hi[a] + ( a == 1 ? 0.0 : mgn );   // full axis run; no overshoot up top
			double exx, eyy, edd;
			ProjectModel( e[0], e[1], e[2], exx, eyy, edd );
			g.DrawLine( &axisPen, Gdiplus::PointF( (float)ox, (float)oy ), Gdiplus::PointF( (float)exx, (float)eyy ) );
			g.DrawString( labs[a], -1, &font, Gdiplus::PointF( (float)exx, (float)eyy ), &lblBrush );
		}

		// overlay text
		Gdiplus::SolidBrush dim( Gdiplus::Color( 210, 185, 185, 190 ) );
		Gdiplus::Font       fontSmall( L"Segoe UI", 8.0f );   // NB: 'small' is a Windows SDK macro (=char)
		const wchar_t * spaceName = ( m_space == SPACE_LAB ) ? L"CIE L*a*b*"
								   : ( m_space == SPACE_RGB ) ? L"RGB cube"
								   :                            L"CIE xyY";
		double sumDE = 0.0, maxDE = 0.0;
		int nDE = 0, nHidden = 0, nVisible = 0;
		for ( size_t k = 0; k < m_points.size(); k++ )
		{
			if ( !m_showProfilePts && m_points[k].srcType == SRC_PROFILE )
				continue;   // hidden layer: excluded from count and dE stats
			nVisible++;
			if ( m_points[k].hasTarget )
			{
				sumDE += m_points[k].dE;
				if ( m_points[k].dE > maxDE ) maxDE = m_points[k].dE;
				nDE++;
				if ( deThr >= 0.0 && m_points[k].dE < deThr )
					nHidden++;
			}
		}
		wchar_t buf[160];
		if ( nDE > 0 )
			// NB: "\x0394" and "E" must be separate literals -- \x greedily
			// consumes the E as a hex digit otherwise (0x394E = a CJK glyph).
			swprintf( buf, 160, L"%s   \x2022   %d points   \x2022   avg \x0394" L"E %.1f   max %.1f",
					  spaceName, nVisible, sumDE / nDE, maxDE );
		else
			swprintf( buf, 160, L"%s   \x2022   %d points", spaceName, nVisible );
		if ( nHidden > 0 )
		{
			size_t len = wcslen( buf );
			swprintf( buf + len, 160 - len, L"   \x2022   %d hidden", nHidden );
		}
		g.DrawString( buf, -1, &fontSmall, Gdiplus::PointF( 8.0f, 6.0f ), &dim );
		g.DrawString( L"drag: rotate    shift+drag: pan    wheel: zoom    click: inspect    right-click: options",
					  -1, &fontSmall, Gdiplus::PointF( 8.0f, (float)( h - 20 ) ), &dim );

		// selection: halo around the picked point + a small readout panel
		if ( m_selected >= 0 && m_selected < (int)m_points.size() )
		{
			const ScenePoint & S = m_points[m_selected];
			double sx, sy, sd;
			ProjectModel( S.mx, S.my, S.mz, sx, sy, sd );

			Gdiplus::Pen haloOut( Gdiplus::Color( 200, 20, 20, 24 ), 1.6f );
			Gdiplus::Pen haloIn ( Gdiplus::Color( 235, 250, 250, 252 ), 1.6f );
			g.DrawEllipse( &haloOut, (float)( sx - 7.5 ), (float)( sy - 7.5 ), 15.0f, 15.0f );
			g.DrawEllipse( &haloIn,  (float)( sx - 6.0 ), (float)( sy - 6.0 ), 12.0f, 12.0f );

			wchar_t line[4][96];
			int nLines = 0;
			swprintf( line[nLines++], 96, L"%s", S.label.c_str() );
			swprintf( line[nLines++], 96, L"measured   x %.4f   y %.4f   Y %.1f%%", S.mcx, S.mcy, S.mYr * 100.0 );
			if ( S.hasTarget )
			{
				swprintf( line[nLines++], 96, L"target       x %.4f   y %.4f   Y %.1f%%", S.tcx, S.tcy, S.tYr * 100.0 );
				swprintf( line[nLines++], 96, L"\x0394" L"E %.2f", S.dE );
			}

			// size the panel to its widest line
			Gdiplus::RectF bounds;
			float panW = 0.0f;
			const float lineH = 15.0f, padX = 9.0f, padY = 7.0f;
			for ( int li = 0; li < nLines; li++ )
			{
				g.MeasureString( line[li], -1, &fontSmall, Gdiplus::PointF( 0, 0 ), &bounds );
				if ( bounds.Width > panW ) panW = bounds.Width;
			}
			float boxW = panW + 2.0f * padX;
			float boxH = nLines * lineH + 2.0f * padY;
			float bx = (float)sx + 14.0f, by = (float)sy - boxH - 6.0f;
			if ( bx + boxW > w - 4 ) bx = (float)sx - boxW - 14.0f;
			if ( bx < 4 )            bx = 4;
			if ( by < 4 )            by = (float)sy + 14.0f;
			if ( by + boxH > h - 4 ) by = (float)( h - 4 ) - boxH;

			Gdiplus::SolidBrush panBg( Gdiplus::Color( 232, 32, 32, 38 ) );
			Gdiplus::Pen        panBorder( Gdiplus::Color( 190, 120, 120, 132 ), 1.0f );
			g.FillRectangle( &panBg, bx, by, boxW, boxH );
			g.DrawRectangle( &panBorder, bx, by, boxW, boxH );

			Gdiplus::SolidBrush titleBrush( Gdiplus::Color( 250, 240, 240, 244 ) );
			Gdiplus::SolidBrush textBrush ( Gdiplus::Color( 235, 205, 205, 212 ) );
			for ( int li = 0; li < nLines; li++ )
				g.DrawString( line[li], -1, &fontSmall,
							  Gdiplus::PointF( bx + padX, by + padY + li * lineH ),
							  li == 0 ? &titleBrush : &textBrush );
		}

		if ( m_points.empty() )
		{
			Gdiplus::Font        big( L"Segoe UI", 11.0f );
			Gdiplus::SolidBrush  wb( Gdiplus::Color( 210, 200, 200, 205 ) );
			Gdiplus::StringFormat sf;
			sf.SetAlignment( Gdiplus::StringAlignmentCenter );
			sf.SetLineAlignment( Gdiplus::StringAlignmentCenter );
			Gdiplus::RectF box( 0, 0, (float)w, (float)h );
			g.DrawString( L"No measurement data to display", -1, &big, box, &sf, &wb );
		}
	} // Graphics flushed here, before the blit in OnDraw
}

void C3DColorView::OnDraw(CDC* pDC)
{
	CRect rc;
	GetClientRect( &rc );
	if ( rc.Width() <= 0 || rc.Height() <= 0 )
		return;
	Render( rc );
	if ( m_memDC != NULL )
		pDC->BitBlt( 0, 0, rc.Width(), rc.Height(), CDC::FromHandle( m_memDC ), 0, 0, SRCCOPY );
}

/////////////////////////////////////////////////////////////////////////////
// framework overrides

void C3DColorView::OnInitialUpdate()
{
	CSavingView::OnInitialUpdate();   // calls OnUpdate(NULL,0,NULL) -> marks dirty
	EnsureGdiplus();

	// Start from the persisted dE filter: the info-pane host pushes it when its
	// segments change, but as a full-window tab this is the only initialisation.
	int f = GetConfig()->GetProfileInt( "MainView", "ThreeD dE Filter", 0 );
	m_deFilter = ( f < 0 || f > 2 ) ? 0 : f;
}

// Incrementally add free measurements appended since the last (re)build. During
// a large profile capture every appended point fires UPD_FREEMEASUREAPPENDED; a
// full scene rebuild per point (n conversions + dE each) would make the capture
// O(n^2), so only the new points are converted.
void C3DColorView::AppendNewFreeMeasures()
{
	CDataSetDoc * pDoc = GetDocument();
	if ( pDoc == NULL || pDoc->GetMeasure() == NULL )
		return;
	CMeasure * pMeasure = pDoc->GetMeasure();

	CColorReference ref = GetColorReference();
	double whiteY = 1.0;
	CColor cw = pMeasure->GetPrimeWhite();
	if ( !cw.isValid() || cw.GetY() <= 0.0 )
		cw = pMeasure->GetOnOffWhite();
	if ( cw.isValid() && cw.GetY() > 0.0 )
		whiteY = cw.GetY();

	CColor wRef( ref.GetWhite() );
	int n = pMeasure->GetMeasurementsSize();
	for ( int i = m_freeInScene; i < n; i++ )
	{
		CColor c = pMeasure->GetMeasurement( i );
		if ( !c.isValid() )
			continue;
		CColor refC = noDataColor;
		if ( c.GetDeltaxy( wRef, ref ) < kChromaMatch )
			refC = wRef;
		else
		{
			for ( int k = 0; k < 3 && !refC.isValid(); k++ )
			{
				if ( c.GetDeltaxy( pMeasure->GetRefPrimary( k ), ref ) < kChromaMatch )
					refC = pMeasure->GetRefPrimary( k );
				else if ( c.GetDeltaxy( pMeasure->GetRefSecondary( k ), ref ) < kChromaMatch )
					refC = pMeasure->GetRefSecondary( k );
			}
		}
		wchar_t lbl[48];
		swprintf( lbl, 48, L"Measurement #%d", i + 1 );
		AppendMeasure( c, whiteY, ref, refC, refC, false, whiteY, lbl, SRC_FREE, i );
	}
	m_freeInScene = n;
}


void C3DColorView::AppendNewProfileMeasures()
{
	CDataSetDoc * pDoc = GetDocument();
	if ( pDoc == NULL || pDoc->GetMeasure() == NULL )
		return;
	CMeasure * pMeasure = pDoc->GetMeasure();

	int n = pMeasure->GetProfileMeasureSize();
	if ( m_profileInScene > 0 &&
		 ( m_profileInScene > n || !pMeasure->GetProfileMeasure( m_profileInScene - 1 ).isValid() ) )
	{
		// a new capture replaced the array underneath the scene: full rebuild
		m_sceneDirty = true;
		return;
	}

	// The capture remaps the cube inner->transport and the sensor recovers the
	// INNER (content, e.g. P3) colors, so plot/swatch in the active reference --
	// the measured cloud then lands inside the P3 gamut solid, not stretched to 2020.
	CColorReference ref = GetColorReference();
	// Diffuse white (measured, or the standalone-profile fallback) so reference
	// targets scale into measured units instead of collapsing at whiteY = 1.0.
	double whiteY = SceneDiffuseWhiteY( pMeasure );

	// During a LIVE capture only patches below the measured frontier exist; when
	// not measuring (post-capture / .chc load / full rebuild) the whole array is
	// present. Either way SKIP invalid holes rather than stopping at them -- an
	// ignored sensor error can leave a gap mid-array, and a plain break would
	// drop every later patch from the cloud (even on a full rebuild).
	int frontier = pMeasure->m_binMeasure ? min( n, pMeasure->m_currentIndex + 1 ) : n;
	int lastAppended = m_profileInScene;	// exclusive high-water of VALID appends
	// HDR-10: GetRefProfileSat returns the 1.0 = 10000 nits convention like
	// GetRefSat/GetRefCC24Sat; rescale to diffuse-white-relative exactly as
	// BuildScene does for the sat/CC24 references.
	const bool hdr10Refs = ( GetConfig()->m_GammaOffsetType == 5 );
	const double hdrRefScale = hdr10Refs ? pMeasure->GetHDRRefScale() : 1.0;
	for ( int i = m_profileInScene; i < frontier; i++ )
	{
		CColor c = pMeasure->GetProfileMeasure( i );
		if ( !c.isValid() )
			continue;
		CColor refC;
		pMeasure->GetRefProfileSat( i, refC );
		if ( hdr10Refs && refC.isValid() )
		{
			refC.SetX( refC.GetX() * hdrRefScale );
			refC.SetY( refC.GetY() * hdrRefScale );
			refC.SetZ( refC.GetZ() * hdrRefScale );
		}
		ColorRGBDisplay rgb = pMeasure->GetProfilePatchRGB( i );
		wchar_t lbl[64];
		swprintf( lbl, 64, L"Profile #%d (RGB %.0f,%.0f,%.0f)", i + 1, rgb[0], rgb[1], rgb[2] );
		// Use the exact same dE the summary pane shows (SDR + PQ-HDR bridge).
		double patchDE = pMeasure->ComputeProfileDE( c, i );
		AppendMeasure( c, whiteY, ref, refC, refC, false, whiteY, lbl, SRC_PROFILE, i, 0, 0, patchDE );
		lastAppended = i + 1;	// keep m_profileInScene-1 pointing at a valid patch
	}
	m_profileInScene = lastAppended;
}

void C3DColorView::SelectProfilePoint(int patchIdx)
{
	m_showProfilePts = true;   // an explicit inspect un-hides the layer so the halo is visible
	for ( size_t i = 0; i < m_points.size(); i++ )
	{
		if ( m_points[i].srcType == SRC_PROFILE && m_points[i].srcA == patchIdx )
		{
			m_selected = (int)i;
			if ( ::IsWindow( m_hWnd ) )
				Invalidate( FALSE );
			return;
		}
	}
}

void C3DColorView::OnUpdate(CView* /*pSender*/, LPARAM lHint, CObject* /*pHint*/)
{
	if ( lHint == UPD_FREEMEASUREAPPENDED && !m_sceneDirty && !m_points.empty() )
	{
		AppendNewFreeMeasures();
		if ( ::IsWindow( m_hWnd ) )
			Invalidate( FALSE );
		return;
	}
	if ( lHint == UPD_REALTIME + 13 && !m_sceneDirty && !m_points.empty() )
	{
		// per-patch profile-capture hint: incremental append, no full rebuild
		// (AppendNewProfileMeasures marks the scene dirty itself when stale)
		AppendNewProfileMeasures();
		if ( ::IsWindow( m_hWnd ) )
			Invalidate( FALSE );
		return;
	}
	m_sceneDirty = true;
	if ( ::IsWindow( m_hWnd ) )
		Invalidate( FALSE );
}

// Bit layout: 0-1 space, 2-11 yaw, 12-18 pitch, 19-24 zoom, 25-26 point-colour
// mode, 27 tails, 28 gamut, 29 floor, 30 shaded faces.
DWORD C3DColorView::GetUserInfo()
{
	DWORD yawQ   = (DWORD)( fmod( m_yaw / k2PI + 1.0, 1.0 ) * 1024.0 ) & 0x3FF;
	DWORD pitchQ = (DWORD)( clampd( ( m_pitch + 1.5 ) / 3.0, 0.0, 1.0 ) * 127.0 ) & 0x7F;
	DWORD zoomQ  = (DWORD)( clampd( ( log( m_zoom ) / log( 6.0 ) + 1.0 ) / 2.0, 0.0, 1.0 ) * 63.0 ) & 0x3F;
	return ( (DWORD)( m_space & 0x3 ) )
		 | ( yawQ   << 2 )
		 | ( pitchQ << 12 )
		 | ( zoomQ  << 19 )
		 | ( (DWORD)( m_pointColor & 0x3 ) << 25 )
		 | ( m_showTails  ? ( 1u << 27 ) : 0u )
		 | ( m_showGamut  ? ( 1u << 28 ) : 0u )
		 | ( m_showFloor  ? ( 1u << 29 ) : 0u )
		 | ( m_shadeGamut ? ( 1u << 30 ) : 0u )
		 | ( m_showProfilePts ? 0u : ( 1u << 31 ) );   // store the HIDE bit so old/default (0) decodes as shown
}

void C3DColorView::SetUserInfo(DWORD dwUserInfo)
{
	if ( dwUserInfo == 0 )
		return;   // fresh view: keep constructor defaults
	m_space = dwUserInfo & 0x3;
	if ( m_space >= SPACE_COUNT || m_space == SPACE_RGB )
		m_space = SPACE_XYY;   // RGB cube is hidden; never restore a saved session into it
	m_yaw   = (double)( ( dwUserInfo >> 2  ) & 0x3FF ) / 1024.0 * k2PI;
	m_pitch = (double)( ( dwUserInfo >> 12 ) & 0x7F  ) / 127.0 * 3.0 - 1.5;
	double zn = (double)( ( dwUserInfo >> 19 ) & 0x3F ) / 63.0 * 2.0 - 1.0;
	m_zoom  = pow( 6.0, zn );
	m_pointColor = (int)( ( dwUserInfo >> 25 ) & 0x3 );
	if ( m_pointColor > PTCOLOR_PLAIN )
		m_pointColor = PTCOLOR_DE;
	m_showTails  = ( ( dwUserInfo >> 27 ) & 1 ) != 0;
	m_showGamut  = ( ( dwUserInfo >> 28 ) & 1 ) != 0;
	m_showFloor  = ( ( dwUserInfo >> 29 ) & 1 ) != 0;
	m_shadeGamut = ( ( dwUserInfo >> 30 ) & 1 ) != 0;
	m_showProfilePts = ( ( dwUserInfo >> 31 ) & 1 ) == 0;   // HIDE bit; default shown
	m_sceneDirty = true;
}

/////////////////////////////////////////////////////////////////////////////
// message handlers

BOOL C3DColorView::OnEraseBkgnd(CDC* /*pDC*/)
{
	return TRUE;   // Render() paints every pixel
}

void C3DColorView::OnSize(UINT nType, int cx, int cy)
{
	CSavingView::OnSize( nType, cx, cy );
	Invalidate( FALSE );
}

void C3DColorView::OnLButtonDown(UINT nFlags, CPoint point)
{
	SetCapture();
	m_bDragging = true;
	m_lastMouse = point;
	m_downPos   = point;
	CSavingView::OnLButtonDown( nFlags, point );
}

void C3DColorView::OnLButtonUp(UINT nFlags, CPoint point)
{
	if ( m_bDragging )
	{
		ReleaseCapture();
		m_bDragging = false;

		// A press-and-release without real movement is a click: select the
		// nearest point within 8px (ties go to the nearer-in-depth point).
		int mdx = point.x - m_downPos.x, mdy = point.y - m_downPos.y;
		if ( mdx * mdx + mdy * mdy <= 9 )
		{
			int    best     = -1;
			double bestDist = 8.0 * 8.0;
			double bestDep  = -1e30;
			double deThr    = -1.0;   // hidden points are not clickable
			if ( m_deFilter > 0 )
			{
				double good, warn;
				GetConfig()->GetDEThresholds( good, warn );
				deThr = ( m_deFilter == 1 ) ? good : warn;
			}
			for ( size_t k = 0; k < m_points.size(); k++ )
			{
				if ( !m_showProfilePts && m_points[k].srcType == SRC_PROFILE )
					continue;   // hidden layer is not clickable
				if ( deThr >= 0.0 && m_points[k].hasTarget && m_points[k].dE < deThr )
					continue;
				double sx, sy, dep;
				ProjectModel( m_points[k].mx, m_points[k].my, m_points[k].mz, sx, sy, dep );
				double dx = sx - point.x, dy = sy - point.y;
				double d2 = dx * dx + dy * dy;
				if ( d2 < bestDist || ( d2 == bestDist && dep > bestDep ) )
				{
					bestDist = d2;
					bestDep  = dep;
					best     = (int)k;
				}
			}
			m_selected = best;   // -1 (empty space) clears the selection
			if ( best >= 0 )
				PushSelectionToMainView( m_points[best] );
			Invalidate( FALSE );
		}
	}
	CSavingView::OnLButtonUp( nFlags, point );
}

void C3DColorView::OnMouseMove(UINT nFlags, CPoint point)
{
	if ( m_bDragging )
	{
		int dx = point.x - m_lastMouse.x;
		int dy = point.y - m_lastMouse.y;
		m_lastMouse = point;
		if ( nFlags & MK_SHIFT )          // shift + drag pans instead of rotating
		{
			m_panX += dx;
			m_panY += dy;
		}
		else
		{
			m_yaw   += dx * 0.01;
			m_pitch += dy * 0.01;
			m_pitch  = clampd( m_pitch, -1.5, 1.5 );
		}
		Invalidate( FALSE );
	}
	CSavingView::OnMouseMove( nFlags, point );
}

BOOL C3DColorView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if ( nFlags & MK_SHIFT )   // shift + wheel pans vertically, like shift + drag
		m_panY += ( zDelta > 0 ) ? -60.0 : 60.0;
	else
	{
		m_zoom *= ( zDelta > 0 ) ? 1.1 : ( 1.0 / 1.1 );
		m_zoom  = clampd( m_zoom, 0.2, 6.0 );
	}
	Invalidate( FALSE );
	return CSavingView::OnMouseWheel( nFlags, zDelta, pt );
}

// Re-fetch the clicked point's original CColor (spectrum and lux data intact)
// and select it in the main view -- same path as clicking its grid column, so
// the selected-colour panel, RGB levels and target widget all follow.
void C3DColorView::PushSelectionToMainView(const ScenePoint & S)
{
	CDataSetDoc * pDoc = GetDocument();
	if ( pDoc == NULL || pDoc->GetMeasure() == NULL )
		return;
	CMeasure * pM = pDoc->GetMeasure();

	// The scene can briefly be stale after measurements are deleted (the
	// rebuild is deferred to the next paint, and a queued click can arrive
	// first), so re-validate the stored index against the CURRENT arrays --
	// the Get* accessors index CArrays unchecked.
	int nAvail = 0;
	switch ( S.srcType )
	{
		case SRC_GRAY:      nAvail = pM->GetGrayScaleSize();      break;
		case SRC_NEARBLACK: nAvail = pM->GetNearBlackScaleSize(); break;
		case SRC_NEARWHITE: nAvail = pM->GetNearWhiteScaleSize(); break;
		case SRC_PRIMARY:
		case SRC_SECONDARY: nAvail = 3;                           break;
		case SRC_SAT:       nAvail = pM->GetSaturationSize();     break;
		case SRC_CC24:      nAvail = MAX_USER_CC_PATCH_SIZE;      break;
		case SRC_FREE:      nAvail = pM->GetMeasurementsSize();   break;
		case SRC_PROFILE:   nAvail = pM->GetProfileMeasureSize(); break;
	}
	if ( S.srcA < 0 || S.srcA >= nAvail )
		return;

	CColor sel = noDataColor;
	switch ( S.srcType )
	{
		case SRC_GRAY:      sel = pM->GetGray( S.srcA );      break;
		case SRC_NEARBLACK: sel = pM->GetNearBlack( S.srcA ); break;
		case SRC_NEARWHITE: sel = pM->GetNearWhite( S.srcA ); break;
		case SRC_PRIMARY:   sel = pM->GetPrimary( S.srcA );   break;
		case SRC_SECONDARY: sel = pM->GetSecondary( S.srcA ); break;
		case SRC_CC24:      sel = pM->GetCC24Sat( S.srcA );   break;
		case SRC_FREE:      sel = pM->GetMeasurement( S.srcA ); break;
		case SRC_PROFILE:   sel = pM->GetProfileMeasure( S.srcA ); break;
		case SRC_SAT:
			// The clicked point may belong to a non-active stimulus level; make that
			// level active (so the grid, CIE chart and stimulus dropdown follow) and
			// read the colour from the now-bound sweeps.
			if ( S.srcC >= 0 && S.srcC < pM->GetSatLevelCount() )
			{
				if ( pM->BindSatLevel( pM->GetSatLevelAt( S.srcC ) ) )
					pDoc->UpdateAllViews( NULL, UPD_ALLSATURATIONS );
			}
			switch ( S.srcB )
			{
				case 0: sel = pM->GetRedSat( S.srcA );     break;
				case 1: sel = pM->GetGreenSat( S.srcA );   break;
				case 2: sel = pM->GetBlueSat( S.srcA );    break;
				case 3: sel = pM->GetYellowSat( S.srcA );  break;
				case 4: sel = pM->GetCyanSat( S.srcA );    break;
				case 5: sel = pM->GetMagentaSat( S.srcA ); break;
			}
			break;
	}
	if ( !sel.isValid() )
		return;

	// Map the point to the data grid's display mode + column (same layout as
	// CMainView's grid selection handler: 0=gray, 1=primaries(1-3)+secondaries
	// (4-6), 2=free measures, 3=near black, 4=near white, 5-10=R/G/B/Y/C/M sat,
	// 11=color checker).
	int mode = -1, col = -1;
	switch ( S.srcType )
	{
		case SRC_GRAY:      mode = 0;            col = S.srcA + 1; break;
		case SRC_PRIMARY:   mode = 1;            col = S.srcA + 1; break;
		case SRC_SECONDARY: mode = 1;            col = S.srcA + 4; break;
		case SRC_FREE:      mode = 2;            col = S.srcA + 1; break;
		case SRC_NEARBLACK: mode = 3;            col = S.srcA + 1; break;
		case SRC_NEARWHITE: mode = 4;            col = S.srcA + 1; break;
		case SRC_SAT:       mode = 5 + S.srcB;   col = S.srcA + 1; break;
		case SRC_CC24:      mode = 11;           col = S.srcA + 1; break;
		case SRC_PROFILE:   mode = 13;           col = -1;         break;	// pane, no grid columns
	}

	POSITION pos = pDoc->GetFirstViewPosition();
	while ( pos != NULL )
	{
		CView * pView = pDoc->GetNextView( pos );
		if ( pView != NULL && pView->IsKindOf( RUNTIME_CLASS( CMainView ) ) )
		{
			CMainView * pMain = (CMainView *)pView;
			if ( mode >= 0 && pMain->m_comboMode.GetSafeHwnd() != NULL
			  && pMain->m_comboMode.GetCurSel() != mode )
			{
				// switch the data grid to the right measurement category
				pMain->m_comboMode.SetCurSel( mode );
				pMain->OnSelchangeComboMode();
			}
			if ( S.srcType == SRC_PROFILE )
			{
				// mode 13 has no grid to feed the reference comparator; drive the
				// selected-color panel + reference widgets the same way the pane's
				// inspect flow does (SetSelectedColor alone leaves the ref stale)
				pMain->SelectProfilePatch( S.srcA );
			}
			else
			{
				if ( col >= 1 )
					pMain->HighlightMeasuringColumn( col );   // select + scroll to the column
				pMain->SetSelectedColor( sel );
			}
			break;
		}
	}
}

void C3DColorView::OnContextMenu(CWnd* /*pWnd*/, CPoint point)
{
	CString sColorSpace, sPointColor, sPcDE, sPcTarget, sPcPlain,
			sShowGamut, sShadeGamut, sShowFloor, sShowTails, sResetView,
			sFilter, sFilterAll, sFilter1, sFilter2;
	sFilter.LoadString ( IDS_3DVIEW_FILTER );
	CDEFilterSegments::FormatFilterLabels( sFilterAll, sFilter1, sFilter2 );
	sColorSpace.LoadString ( IDS_3DVIEW_COLORSPACE );
	sPointColor.LoadString ( IDS_3DVIEW_POINTCOLOR );
	sPcDE.LoadString       ( IDS_3DVIEW_PC_DE );
	sPcTarget.LoadString   ( IDS_3DVIEW_PC_TARGET );
	sPcPlain.LoadString    ( IDS_3DVIEW_PC_PLAIN );
	sShowGamut.LoadString  ( IDS_3DVIEW_SHOWGAMUT );
	sShadeGamut.LoadString ( IDS_3DVIEW_SHADEGAMUT );
	sShowFloor.LoadString  ( IDS_3DVIEW_SHOWFLOOR );
	sShowTails.LoadString  ( IDS_3DVIEW_SHOWTAILS );
	sResetView.LoadString  ( IDS_3DVIEW_RESETVIEW );
	CString sShowProfile;
	sShowProfile.LoadString ( IDS_3DVIEW_SHOWPROFILE );

	CMenu menu, spaceMenu, colorMenu, filterMenu;
	menu.CreatePopupMenu();

	// dE filter, mirroring the info-pane segments (and the only way to set it
	// when the view runs as a full-window tab)
	filterMenu.CreatePopupMenu();
	filterMenu.AppendMenu( MF_STRING, 501, sFilterAll );
	filterMenu.AppendMenu( MF_STRING, 502, sFilter1 );
	filterMenu.AppendMenu( MF_STRING, 503, sFilter2 );
	filterMenu.CheckMenuRadioItem( 501, 503, 501 + m_deFilter, MF_BYCOMMAND );

	spaceMenu.CreatePopupMenu();
	// space names are technical terms, identical in every language
	spaceMenu.AppendMenu( MF_STRING, 101, _T("CIE xyY") );
	spaceMenu.AppendMenu( MF_STRING, 102, _T("CIE L*a*b*") );
	// RGB cube is hidden for now: its measured-point placement (linear light) reads
	// as confusing, and its real use is 3D-LUT work not yet built. The SPACE_RGB
	// enum + rasterizer are kept; the mode is just not offered here.
	spaceMenu.CheckMenuRadioItem( 101, 102, 101 + m_space, MF_BYCOMMAND );

	colorMenu.CreatePopupMenu();
	colorMenu.AppendMenu( MF_STRING, 204, sPcDE );
	colorMenu.AppendMenu( MF_STRING, 206, sPcTarget );
	colorMenu.AppendMenu( MF_STRING, 207, sPcPlain );
	colorMenu.CheckMenuRadioItem( 204, 207,
		m_pointColor == PTCOLOR_TARGET ? 206 : ( m_pointColor == PTCOLOR_PLAIN ? 207 : 204 ), MF_BYCOMMAND );

	menu.AppendMenu( MF_POPUP, (UINT_PTR)spaceMenu.m_hMenu, sColorSpace );
	menu.AppendMenu( MF_POPUP, (UINT_PTR)colorMenu.m_hMenu, sPointColor );
	menu.AppendMenu( MF_POPUP, (UINT_PTR)filterMenu.m_hMenu, sFilter );
	menu.AppendMenu( MF_SEPARATOR );
	menu.AppendMenu( MF_STRING | ( m_showGamut  ? MF_CHECKED : 0 ), 201, sShowGamut );
	menu.AppendMenu( MF_STRING | ( m_shadeGamut ? MF_CHECKED : 0 ), 203, sShadeGamut );
	menu.AppendMenu( MF_STRING | ( m_showFloor  ? MF_CHECKED : 0 ), 202, sShowFloor );
	menu.AppendMenu( MF_STRING | ( m_showTails  ? MF_CHECKED : 0 ), 205, sShowTails );
	menu.AppendMenu( MF_STRING | ( m_showProfilePts ? MF_CHECKED : 0 ), 208, sShowProfile );
	menu.AppendMenu( MF_SEPARATOR );
	menu.AppendMenu( MF_STRING, 301, sResetView );

	// The parent menu owns the submenus now; detach the wrappers so their
	// destructors don't destroy the handles a second time.
	spaceMenu.Detach();
	colorMenu.Detach();
	filterMenu.Detach();

	if ( point.x == -1 && point.y == -1 )
	{
		CRect wr; GetWindowRect( &wr ); point = wr.CenterPoint();
	}
	int cmd = menu.TrackPopupMenu( TPM_LEFTALIGN | TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, this );
	switch ( cmd )
	{
		case 101: SetSpace( SPACE_XYY ); break;
		case 102: SetSpace( SPACE_LAB ); break;
		// case 103 (RGB cube) intentionally removed: mode hidden from the menu
		case 201: m_showGamut  = !m_showGamut;  Invalidate( FALSE ); break;
		case 203: m_shadeGamut = !m_shadeGamut; Invalidate( FALSE ); break;
		case 202: m_showFloor  = !m_showFloor;  Invalidate( FALSE ); break;
		case 204: m_pointColor = PTCOLOR_DE;     Invalidate( FALSE ); break;
		case 206: m_pointColor = PTCOLOR_TARGET; Invalidate( FALSE ); break;
		case 207: m_pointColor = PTCOLOR_PLAIN;  Invalidate( FALSE ); break;
		case 205: m_showTails  = !m_showTails;   Invalidate( FALSE ); break;
		case 208: m_showProfilePts = !m_showProfilePts; Invalidate( FALSE ); break;
		case 301: m_yaw = 0.70; m_pitch = 0.45; m_zoom = 1.0; m_panX = m_panY = 0.0; Invalidate( FALSE ); break;
		case 501: case 502: case 503:
		{
			int f = cmd - 501;
			SetDEFilter( f );
			GetConfig()->WriteProfileInt( "MainView", "ThreeD dE Filter", f );
			// keep the info-pane segments in step if they exist
			CDataSetDoc * pDoc = GetDocument();
			POSITION pos = pDoc ? pDoc->GetFirstViewPosition() : NULL;
			while ( pos != NULL )
			{
				CView * pView = pDoc->GetNextView( pos );
				if ( pView != NULL && pView->IsKindOf( RUNTIME_CLASS( CMainView ) ) )
				{
					CMainView * pMain = (CMainView *)pView;
					if ( pMain->m_3dDEFilter.GetSafeHwnd() )
						pMain->m_3dDEFilter.SetSel( f );
					break;
				}
			}
			break;
		}
		default: break;
	}
}
