/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2011 Association Homecinema Francophone.  All rights reserved.
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
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// TargetWnd.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "TargetWnd.h"
#include "Color.h"
#include "MainFrm.h"
#include "DataSetDoc.h"
#include "MainView.h"
#include "fxcolor.h"
#include <math.h>
#include <vector>
#include <algorithm>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#include "GdiPlusAA.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTargetWnd

// Live instances, so a dE-tolerance change made on one target widget can
// repaint the others (e.g. the mini target in the main view).
static std::vector<CTargetWnd *> s_targetWnds;

CTargetWnd::CTargetWnd()
{
	s_targetWnds.push_back(this);
	m_deltax=0.0;
	m_deltay=0.0;
	m_deltaE=-1.0;
	m_clr = RGB(0,0,0);
	m_pRefColor = NULL;
    m_pDocument = NULL;
	centerXYZ = GetColorReference().GetWhite();
	m_testClr = ColorRGBDisplay(0.0);
	m_trailCenter = centerXYZ;
	m_trailCol = -999;
	m_trailMode = -999;
	m_pBgBitmap = NULL;
	m_bgCx = -1;
	m_bgCy = -1;
	m_bgDark = -1;
	m_bgTol = -1.0;
}

CTargetWnd::~CTargetWnd()
{
	s_targetWnds.erase(std::remove(s_targetWnds.begin(), s_targetWnds.end(), this), s_targetWnds.end());
	delete m_pBgBitmap;
}

// The target of primary/secondary patch idx, in BuildPrimarySecondaryColors'
// order: R, G, B, Y, C, M, then white (6) and black (7).
static ColorXYZ PrimarySecondaryTarget ( int idx )
{
	switch ( idx )
	{
		case 0:		return GetColorReference().GetRed();
		case 1:		return GetColorReference().GetGreen();
		case 2:		return GetColorReference().GetBlue();
		case 3:		return GetColorReference().GetYellow();
		case 4:		return GetColorReference().GetCyan();
		case 5:		return GetColorReference().GetMagenta();
		default:	return GetColorReference().GetWhite();
	}
}

void CTargetWnd::Refresh(BOOL m_b16_235, int minCol, int nSize, int m_DisplayMode, CDataSetDoc * pDoc, int target)
{
	if (m_pRefColor)
	{
		bool DVD = (GetConfig()->GetGeneratorType() == CColorHCFRConfig::enumManual);
		bool isHDR10 = GetConfig()->m_GammaOffsetType == 5;
		BOOL b10bit = GetConfig()->GetUse10bitLevels();
		BOOL bLimited = GetConfig()->GetRGB16_235();

		// m_testClr is the patch the sweep for this mode sends for column minCol,
		// at full precision: the test window shows it and the continuous and single
		// measures send it on, so it is built by the sweeps' own code.
		if (minCol > 0)
		{
			ColorRGBDisplay	GenColors [MAX_USER_CC_PATCH_SIZE + 10];

			if ((m_DisplayMode == 0 || m_DisplayMode == 2 || m_DisplayMode == 3 || m_DisplayMode == 4 ))
			{
				double level;
				centerXYZ = GetColorReference().GetWhite();
	            if (nSize > 0)
				{
					if (m_DisplayMode == 0)
						// Grayscale: honor the explicit (possibly non-uniform) IRE levels,
						// matching what DisplayGray() actually shows/measures.
						level = pDoc->GetMeasure()->GetGrayPercent(minCol - 1, GetConfig()->m_bUseRoundDown, b10bit, bLimited);
					else if (m_DisplayMode == 3)
						// Near black: the sweep's 1% (PQ: 2%) steps on the active grid
						level = ArrayIndexToGrayLevel((minCol - 1) * (isHDR10 ? 2 : 1), 101, GetConfig()->m_bUseRoundDown, b10bit, bLimited);
					else
						level = 100.0 * (double)(minCol-1) / (double)(nSize-1);
				}
			    else
					// Near white (nSize is minus the scale size): the sweep's levels below the clip
					level = ArrayIndexToGrayLevel(pDoc->GetMeasure()->m_NearWhiteClipCol+nSize+minCol-1, 101, GetConfig()->m_bUseRoundDown, b10bit, bLimited);

				m_testClr = ColorRGBDisplay(level);
			}
			else if (m_DisplayMode > 4 && m_DisplayMode < 12) //saturations
			{
				bool special = (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb);
				switch (m_DisplayMode)
				{
					case 5:
						centerXYZ =  pDoc->GetMeasure()->GetRefSat(0, double(minCol-1) / double(nSize - 1), special).GetXYZValue();
						GenerateSaturationColors (GetColorReference(), GenColors, nSize, true, false, false, GetConfig()->m_GammaOffsetType, pDoc->GetMeasure()->GetActiveSatLevel(), b10bit, bLimited);
						break;
					case 6:
						centerXYZ =  pDoc->GetMeasure()->GetRefSat(1, double(minCol-1) / double(nSize - 1), special).GetXYZValue();
						GenerateSaturationColors (GetColorReference(), GenColors, nSize, false, true, false, GetConfig()->m_GammaOffsetType, pDoc->GetMeasure()->GetActiveSatLevel(), b10bit, bLimited);
						break;
					case 7:
						centerXYZ =  pDoc->GetMeasure()->GetRefSat(2, double(minCol-1) / double(nSize - 1), special).GetXYZValue();
						GenerateSaturationColors (GetColorReference(), GenColors, nSize, false, false, true, GetConfig()->m_GammaOffsetType, pDoc->GetMeasure()->GetActiveSatLevel(), b10bit, bLimited);
						break;
					case 8:
						centerXYZ =  pDoc->GetMeasure()->GetRefSat(3, double(minCol-1) / double(nSize-1), special).GetXYZValue();
						GenerateSaturationColors (GetColorReference(), GenColors, nSize, true, true, false, GetConfig()->m_GammaOffsetType, pDoc->GetMeasure()->GetActiveSatLevel(), b10bit, bLimited);
						break;
					case 9:
						centerXYZ =  pDoc->GetMeasure()->GetRefSat(4, double(minCol-1) / double(nSize-1), special).GetXYZValue();
						GenerateSaturationColors (GetColorReference(), GenColors, nSize, false, true, true, GetConfig()->m_GammaOffsetType, pDoc->GetMeasure()->GetActiveSatLevel(), b10bit, bLimited);
						break;
					case 10:
						centerXYZ =  pDoc->GetMeasure()->GetRefSat(5, double(minCol-1) / double(nSize-1), special).GetXYZValue();
						GenerateSaturationColors (GetColorReference(), GenColors, nSize, true, false, true, GetConfig()->m_GammaOffsetType, pDoc->GetMeasure()->GetActiveSatLevel(), b10bit, bLimited);
						break;
					case 11:
					{
						CColor clr;
						pDoc->GetMeasure()->GetRefCC24Sat(minCol - 1, clr);
						centerXYZ = clr.GetXYZValue();
					}
						GenerateCC24Colors (GetColorReference(), GenColors, GetConfig()->m_CCMode, GetConfig()->m_GammaOffsetType, b10bit, bLimited);
						if (GetConfig()->m_CCMode == MCD)
						{
							if (minCol >= 7)
								minCol -= 6;
							else
								minCol = 25 - minCol;	// 1..6 -> 24..19
						}
						break;
				}
				m_testClr = GenColors[minCol-1];
			}
			else if ( m_DisplayMode == 13 )	// display profile patch
			{
				// during a capture the test window shows what the generator is
				// showing: a prologue patch, a remapped cube patch or a drift anchor
				if ( pDoc->GetMeasure()->m_bProfileCapturing && target == TARGET_TESTWINDOW )
					m_testClr = pDoc->GetMeasure()->m_profileShownRGB;
				else
				{
					CColor clr;
					pDoc->GetMeasure()->GetRefProfileSat(minCol - 1, clr);
					centerXYZ = clr.GetXYZValue();
					// the capture's wire: P3/709-in-2020 remap the inner cube into the
					// container, while the reference stays on the inner cube
					m_testClr = pDoc->GetMeasure()->GetProfilePatchRGB(minCol - 1);
					RemapProfileToTransport ( &m_testClr, 1, GetColorReference(), GetConfig()->m_GammaOffsetType,
											  GetConfig()->GetUse10bitLevels() != FALSE, GetConfig()->GetRGB16_235() != FALSE );
				}
			}
			else if ( m_DisplayMode == 1 && minCol <= 8 )
			{
				// Primaries/secondaries, then white (column 7) and black (8)
				pDoc->GetMeasure()->BuildPrimarySecondaryColors(pDoc->GetGenerator(), GenColors);
				centerXYZ = PrimarySecondaryTarget(minCol - 1);
				m_testClr = GenColors[minCol - 1];
			}
		} //mincol > 0
		else if ( m_DisplayMode == 1 && DVD && GetConfig()->m_bDetectPrimaries )
		{
			// Disc patterns with no column selected: recognize the patch on screen
			// from the measured chromaticity (first match in R,G,B,Y,C,M,W order).
			ColorRGBDisplay	GenColors [ 8 ];
			pDoc->GetMeasure()->BuildPrimarySecondaryColors(pDoc->GetGenerator(), GenColors);
			for (int idx = 0; idx < 7; idx++)
			{
				if (m_pRefColor->GetDeltaxy(PrimarySecondaryTarget(idx), GetColorReference()) < 0.05)
				{
					centerXYZ = PrimarySecondaryTarget(idx);
					m_testClr = GenColors[idx];
					break;
				}
			}
		}

		m_clr = m_testClr.GetColorRef(false);

		//Update test window for display when selected. Not during a sweep whose
		//generator paints this window itself (Window mode): it already shows the
		//patch being measured.
		BOOL		bDisplayColor = GetConfig () -> m_bDisplayTestColors
						&& ! ( IsMeasureSweepActive () && pDoc && pDoc -> GetGenerator () && pDoc -> GetGenerator () -> PaintsTestColorWnd () );

		if (bDisplayColor && target <= TARGET_ALL)
			( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) ->m_wndTestColorWnd.SetTestColor ( m_testClr, m_b16_235 );
		
		if (target >= TARGET_ALL)
		{
			//update RGB

			ColorxyY aColor = m_pRefColor -> GetxyYValue();
			ColorxyY centerxyY(centerXYZ);

			m_deltax = (aColor[0]-centerxyY[0])/centerxyY[0];
			m_deltay = (aColor[1]-centerxyY[1])/centerxyY[1];

			UpdateDeltaE();

			// Trail bookkeeping: a new target (patch, mode or reference change)
			// starts a fresh trail; a moved reading appends to it.
			if ( m_trailCol != minCol || m_trailMode != m_DisplayMode ||
				 fabs(m_trailCenter[0]-centerXYZ[0]) > 1e-9 ||
				 fabs(m_trailCenter[1]-centerXYZ[1]) > 1e-9 ||
				 fabs(m_trailCenter[2]-centerXYZ[2]) > 1e-9 )
			{
				m_trail.clear();
				m_trailCol = minCol;
				m_trailMode = m_DisplayMode;
				m_trailCenter = centerXYZ;
			}

			STrailPoint pt;
			pt.angle = atan2(m_deltay, m_deltax);
			pt.radius = RingRadius(m_deltaE);
			if ( m_trail.empty() || fabs(m_trail.back().angle - pt.angle) > 1e-6 ||
				 fabs(m_trail.back().radius - pt.radius) > 1e-6 )
			{
				m_trail.push_back(pt);
				while ( m_trail.size() > 5 )	// current read + up to 4 predecessors
					m_trail.pop_front();
			}

			Invalidate(TRUE);
		}
	} //have valid m_prefcolor
}


BEGIN_MESSAGE_MAP(CTargetWnd, CWnd)
	//{{AFX_MSG_MAP(CTargetWnd)
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_WHATS_THIS, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CTargetWnd message handlers

// Display name of the configured colour-difference formula (the combo in the
// advanced settings). "Recommended" computes CIE2000 for colour patches and
// CIE76uv for grayscale, so it is shown as the pair.
static const WCHAR * DeltaEFormulaName()
{
	switch ( GetConfig()->m_dE_form )
	{
		case 0: return L"CIE76uv";
		case 1: return L"CIE76ab";
		case 2: return L"CIE94";
		case 3: return L"CIE2000";
		case 4: return L"CMC";
		case 5: return L"CIE2000/76uv";
		case 6: return L"dICtCp";
	}
	return L"";
}

// Pill stat chips are drawn with the shared DrawStatChip helper (GdiPlusAA.h).

void CTargetWnd::OnPaint()
{
	CPaintDC dc(this); // device context for painting
    CHMemDC pDC(&dc);

	CRect rect;
	GetClientRect(&rect);

	// windows problem with "negative" client height. Can occur when mainframe is really small
	if ( rect.bottom == 32767 )
		rect.bottom = 0;
	if ( rect.Width() <= 0 || rect.Height() <= 0 )
		return;

	FxEnsureGdiplus();
	BOOL bDark = GetConfig()->m_darkTheme;

	double deGood, deWarn;
	GetConfig()->GetDEThresholds(deGood, deWarn);
	if ( !m_pBgBitmap || m_bgCx != rect.Width() || m_bgCy != rect.Height() || m_bgDark != bDark ||
		 m_bgTol != deWarn )
		RebuildBackground(rect, bDark);

	Gdiplus::Graphics g(pDC->GetSafeHdc());
	g.DrawImage(m_pBgBitmap, 0, 0, rect.Width(), rect.Height());

	if ( m_pRefColor == NULL || !m_pRefColor->isValid() )
		return;		// Draw nothing more

	g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

	double cx, cy, R;
	GetGeometry(rect, cx, cy, R);

	// Current read on the ring scale: direction = xy cast direction,
	// distance = dE.
	double curAngle = atan2(m_deltay, m_deltax);
	double curFrac = RingRadius(m_deltaE);
	Gdiplus::REAL curX = (Gdiplus::REAL)(cx + cos(curAngle) * curFrac * R);
	Gdiplus::REAL curY = (Gdiplus::REAL)(cy - sin(curAngle) * curFrac * R);

	// Fading trail of the previous reads, linked into the current dot
	// (the last stored point is the current read).
	int nTrail = (int)m_trail.size();
	if ( nTrail > 1 )
	{
		BYTE trailGray = bDark ? 210 : 80;
		Gdiplus::Pen linkPen(Gdiplus::Color(110, trailGray, trailGray, trailGray), 1.0f);
		linkPen.SetDashStyle(Gdiplus::DashStyleDash);
		Gdiplus::REAL trailR = (Gdiplus::REAL)max(2.5, 0.022 * R);
		Gdiplus::REAL prevX = 0.0f, prevY = 0.0f;
		for ( int i = 0; i < nTrail; i++ )
		{
			Gdiplus::REAL px = (Gdiplus::REAL)(cx + cos(m_trail[i].angle) * m_trail[i].radius * R);
			Gdiplus::REAL py = (Gdiplus::REAL)(cy - sin(m_trail[i].angle) * m_trail[i].radius * R);
			if ( i > 0 )
				g.DrawLine(&linkPen, prevX, prevY, px, py);
			if ( i < nTrail - 1 )
			{
				int alpha = 60 + 110 * (i + 1) / nTrail;
				Gdiplus::SolidBrush dotBrush(Gdiplus::Color((BYTE)alpha, trailGray, trailGray, trailGray));
				g.FillEllipse(&dotBrush, px - trailR, py - trailR, 2.0f * trailR, 2.0f * trailR);
			}
			prevX = px;
			prevY = py;
		}
	}

	// Measured dot, filled with the measured colour
	Gdiplus::REAL dotR = (Gdiplus::REAL)max(4.0, 0.055 * R);
	Gdiplus::Color fill(255, GetRValue(m_clr), GetGValue(m_clr), GetBValue(m_clr));
	Gdiplus::Color edge = bDark ? Gdiplus::Color(255, 240, 240, 240) : Gdiplus::Color(255, 50, 50, 50);
	if ( m_deltaE > 10.0 )
	{
		// off scale: hollow marker pinned just outside the outer ring
		Gdiplus::Pen fillPen(fill, (Gdiplus::REAL)max(2.0, dotR * 0.45));
		g.DrawEllipse(&fillPen, curX - dotR, curY - dotR, 2.0f * dotR, 2.0f * dotR);
		Gdiplus::Pen edgePen(edge, 1.0f);
		g.DrawEllipse(&edgePen, curX - dotR - 1.0f, curY - dotR - 1.0f, 2.0f * dotR + 2.0f, 2.0f * dotR + 2.0f);
	}
	else
	{
		Gdiplus::SolidBrush dotBrush(fill);
		g.FillEllipse(&dotBrush, curX - dotR, curY - dotR, 2.0f * dotR, 2.0f * dotR);
		Gdiplus::Pen edgePen(edge, 1.5f);
		g.DrawEllipse(&edgePen, curX - dotR, curY - dotR, 2.0f * dotR, 2.0f * dotR);
	}

	// Large widget only: stat chips bottom-right (dE tinted with the data
	// grid's highlight colours, xy offsets beside it) and the ring formula
	// note bottom-left.
	BOOL compact = min(rect.Width(), rect.Height()) < 220;
	if ( !compact )
	{
		g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
		Gdiplus::REAL chipFontPx = (Gdiplus::REAL)max(14.0, 0.058 * R + 1.0);
		Gdiplus::Font chipFont(L"Segoe UI", chipFontPx, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
		const Gdiplus::REAL pad = 8.0f;

		// dE chip tinted exactly like the measures grid cell, from the shared
		// tolerance preset (black text like the grid).
		COLORREF gridClr = GetConfig()->GetDEColor(m_deltaE, bDark);
		Gdiplus::Color chipFill(255, GetRValue(gridClr), GetGValue(gridClr), GetBValue(gridClr));
		Gdiplus::Color chipBorder(255, GetRValue(gridClr) * 70 / 100, GetGValue(gridClr) * 70 / 100, GetBValue(gridClr) * 70 / 100);
		Gdiplus::Color chipText(255, 0, 0, 0);

		WCHAR buf[64];
		Gdiplus::REAL xRight = (Gdiplus::REAL)rect.Width() - pad;
		Gdiplus::REAL yBottom = (Gdiplus::REAL)rect.Height() - pad;
		swprintf_s(buf, 64, L"dE %.1f", m_deltaE);
		Gdiplus::REAL wChip = DrawStatChip(g, chipFont, buf, xRight, yBottom, chipFill, chipBorder, chipText);

		Gdiplus::Color nFill   = bDark ? Gdiplus::Color(255, 42, 42, 42)    : Gdiplus::Color(255, 255, 255, 255);
		Gdiplus::Color nBorder = bDark ? Gdiplus::Color(255, 72, 72, 72)    : Gdiplus::Color(255, 205, 207, 213);
		Gdiplus::Color nText   = bDark ? Gdiplus::Color(255, 215, 215, 215) : Gdiplus::Color(255, 70, 74, 80);
		swprintf_s(buf, 64, L"dx %+.1f%%   dy %+.1f%%", m_deltax * 100.0, m_deltay * 100.0);
		DrawStatChip(g, chipFont, buf, xRight - wChip - 6.0f, yBottom, nFill, nBorder, nText);

		// ring formula note, plain muted text (localized prefix)
		Gdiplus::REAL noteFontPx = (Gdiplus::REAL)max(11.0, 0.055 * R);
		Gdiplus::Font noteFont(L"Segoe UI", noteFontPx, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
		CStringW noteFmt;
		if ( !noteFmt.LoadString(IDS_TARGET_RINGS) )
			noteFmt = L"Rings: dE %s";
		CStringW note;
		note.Format(noteFmt, DeltaEFormulaName());
		Gdiplus::RectF noteBounds;
		g.MeasureString(note, -1, &noteFont, Gdiplus::PointF(0.0f, 0.0f), &noteBounds);
		Gdiplus::SolidBrush noteBrush(bDark ? Gdiplus::Color(190, 205, 205, 205) : Gdiplus::Color(200, 95, 99, 106));
		g.DrawString(note, -1, &noteFont, Gdiplus::PointF(pad, yBottom - noteBounds.Height), &noteBrush);
	}
}

// Ring anchors: dE 1/3/5/10 sit at 25/50/75/100% of the scale radius,
// piecewise linear in between, pinned just outside the rim when off scale.
double CTargetWnd::RingRadius(double dE)
{
	if ( dE < 0.0 )
		return 0.0;
	if ( dE <= 1.0 )
		return 0.25 * dE;
	if ( dE <= 3.0 )
		return 0.25 + 0.25 * (dE - 1.0) / 2.0;
	if ( dE <= 5.0 )
		return 0.50 + 0.25 * (dE - 3.0) / 2.0;
	if ( dE <= 10.0 )
		return 0.75 + 0.25 * (dE - 5.0) / 5.0;
	return 1.04;
}

void CTargetWnd::GetGeometry(const CRect & rect, double & cx, double & cy, double & R) const
{
	cx = rect.Width() / 2.0;
	cy = rect.Height() / 2.0;
	double margin = max(8.0, 0.05 * min(rect.Width(), rect.Height()));
	R = min(rect.Width(), rect.Height()) / 2.0 - margin;
	if ( R < 10.0 )
		R = 10.0;
}

// The dE shown is the same number as the RGB-levels dE bar (full configured
// formula including the HDR handling), read from the sibling widget - both
// targets are CMainView children and the levels widget refreshes first. When
// the sibling has none (it zeroes dE for the first grayscale column), fall
// back to the plain formula against the current target.
void CTargetWnd::UpdateDeltaE()
{
	m_deltaE = -1.0;
	CWnd * pParent = GetParent();
	if ( pParent && pParent->IsKindOf(RUNTIME_CLASS(CMainView)) )
	{
		float dE = ((CMainView *)pParent)->m_RGBLevels.m_dEValue;
		if ( dE > 0.0f )
			m_deltaE = dE;
	}
	if ( m_deltaE < 0.0 && m_pRefColor && m_pRefColor->isValid() )
		m_deltaE = m_pRefColor->GetDeltaE(CColor(centerXYZ));
	if ( m_deltaE < 0.0 )
		m_deltaE = 0.0;
}

static void HueToRGB(double hue, double v, int & r, int & g, int & b)	// HSV -> RGB with S = 1, hue in [0,360)
{
	double h6 = hue / 60.0;
	int i = (int)h6;
	double f = h6 - i;
	double q = v * (1.0 - f);
	double t = v * f;
	double rd, gd, bd;
	switch ( i )
	{
		case 0:  rd = v;   gd = t;   bd = 0.0; break;
		case 1:  rd = q;   gd = v;   bd = 0.0; break;
		case 2:  rd = 0.0; gd = v;   bd = t;   break;
		case 3:  rd = 0.0; gd = q;   bd = v;   break;
		case 4:  rd = t;   gd = 0.0; bd = v;   break;
		default: rd = v;   gd = 0.0; bd = q;   break;
	}
	r = (int)(rd * 255.0 + 0.5);
	g = (int)(gd * 255.0 + 0.5);
	b = (int)(bd * 255.0 + 0.5);
}

void CTargetWnd::RebuildBackground(const CRect & rect, BOOL bDark)	// Cached: panel, hue wheel, rings, labels
{
	int w = rect.Width();
	int h = rect.Height();

	delete m_pBgBitmap;
	// DEBUG_NEW's placement form doesn't match Gdiplus::GdiplusBase::operator new
#ifdef _DEBUG
#undef new
#endif
	m_pBgBitmap = new Gdiplus::Bitmap(w, h, PixelFormat32bppPARGB);
#ifdef _DEBUG
#define new DEBUG_NEW
#endif
	double deGood, deWarn;
	GetConfig()->GetDEThresholds(deGood, deWarn);
	m_bgTol = deWarn;	// tolerance ring sits at the fail (yellow->red) limit
	m_bgCx = w;
	m_bgCy = h;
	m_bgDark = bDark;

	double cx, cy, R;
	GetGeometry(rect, cx, cy, R);

	// Panel gradient behind everything (matches the RGB level widget), with
	// the hue wheel blended in inside the circle: direction from center =
	// hue of the cast, neutral at the target, saturating toward the rim.
	int bgTop = bDark ? 26 : 250;
	int bgBot = bDark ? 12 : 235;
	double wheelAlpha = bDark ? 0.45 : 0.35;
	double wheelV = bDark ? 0.80 : 0.85;

	Gdiplus::BitmapData bd;
	Gdiplus::Rect lockRect(0, 0, w, h);
	if ( m_pBgBitmap->LockBits(&lockRect, Gdiplus::ImageLockModeWrite, PixelFormat32bppPARGB, &bd) == Gdiplus::Ok )
	{
		for ( int y = 0; y < h; y++ )
		{
			DWORD * row = (DWORD *)((BYTE *)bd.Scan0 + y * bd.Stride);
			int v = bgTop - (h > 1 ? (bgTop - bgBot) * y / (h - 1) : 0);
			int bgR = v, bgG = v, bgB = bDark ? v : min(255, v + 2);
			for ( int x = 0; x < w; x++ )
			{
				int r = bgR, gg = bgG, b = bgB;
				double fx = x - cx;
				double fy = cy - y;
				double rr = sqrt(fx * fx + fy * fy) / R;
				if ( rr <= 1.0 )
				{
					double a = atan2(fy, fx) * 180.0 / PI;
					if ( a < 0.0 )
						a += 360.0;
					// green up, red right, magenta down, cyan left (the old
					// widget's axis convention)
					double hue;
					if ( a < 90.0 )
						hue = a * (120.0 / 90.0);
					else if ( a < 180.0 )
						hue = 120.0 + (a - 90.0) * (60.0 / 90.0);
					else if ( a < 270.0 )
						hue = 180.0 + (a - 180.0) * (120.0 / 90.0);
					else
						hue = 300.0 + (a - 270.0) * (60.0 / 90.0);
					int hr, hg, hb;
					HueToRGB(hue, wheelV, hr, hg, hb);
					double alpha = wheelAlpha * (rr - 0.12) / 0.88;
					if ( alpha < 0.0 )
						alpha = 0.0;
					double aaEdge = (1.0 - rr) * R / 1.5;	// anti-alias the rim
					if ( aaEdge < 1.0 )
						alpha *= max(0.0, aaEdge);
					r  = (int)(r  + (hr - r ) * alpha);
					gg = (int)(gg + (hg - gg) * alpha);
					b  = (int)(b  + (hb - b ) * alpha);
				}
				row[x] = 0xFF000000 | (r << 16) | (gg << 8) | b;
			}
		}
		m_pBgBitmap->UnlockBits(&bd);
	}

	Gdiplus::Graphics gb(m_pBgBitmap);

	// panel border, crisp (no AA yet)
	Gdiplus::Pen borderPen(bDark ? Gdiplus::Color(255, 56, 56, 56) : Gdiplus::Color(255, 205, 207, 213), 1.0f);
	gb.DrawRectangle(&borderPen, 0, 0, w - 1, h - 1);

	gb.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	gb.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

	BOOL compact = min(w, h) < 220;

	// crosshair axes, under the rings
	Gdiplus::Pen axisPen(bDark ? Gdiplus::Color(70, 235, 235, 235) : Gdiplus::Color(60, 30, 30, 30), 1.0f);
	gb.DrawLine(&axisPen, (Gdiplus::REAL)(cx - R), (Gdiplus::REAL)cy, (Gdiplus::REAL)(cx + R), (Gdiplus::REAL)cy);
	gb.DrawLine(&axisPen, (Gdiplus::REAL)cx, (Gdiplus::REAL)(cy - R), (Gdiplus::REAL)cx, (Gdiplus::REAL)(cy + R));

	// dE scale rings at 1/3/5/10; the dashed tolerance ring is drawn on top
	// at the configured tolerance
	Gdiplus::Pen ringPen(bDark ? Gdiplus::Color(140, 130, 130, 130) : Gdiplus::Color(150, 165, 169, 178), 1.0f);
	static const double ringFrac[4] = { 0.25, 0.50, 0.75, 1.00 };
	for ( int i = 0; i < 4; i++ )
	{
		double rr = ringFrac[i] * R;
		gb.DrawEllipse(&ringPen, (Gdiplus::REAL)(cx - rr), (Gdiplus::REAL)(cy - rr), (Gdiplus::REAL)(2.0 * rr), (Gdiplus::REAL)(2.0 * rr));
	}

	Gdiplus::Pen tolPen(bDark ? Gdiplus::Color(255, 47, 191, 143) : Gdiplus::Color(255, 29, 158, 117), 1.4f);
	tolPen.SetDashStyle(Gdiplus::DashStyleDash);
	double tolR = RingRadius(m_bgTol) * R;
	gb.DrawEllipse(&tolPen, (Gdiplus::REAL)(cx - tolR), (Gdiplus::REAL)(cy - tolR), (Gdiplus::REAL)(2.0 * tolR), (Gdiplus::REAL)(2.0 * tolR));

	// center marker
	double ctrR = max(2.0, 0.014 * R);
	Gdiplus::SolidBrush ctrBrush(bDark ? Gdiplus::Color(255, 235, 235, 235) : Gdiplus::Color(255, 40, 40, 40));
	gb.FillEllipse(&ctrBrush, (Gdiplus::REAL)(cx - ctrR), (Gdiplus::REAL)(cy - ctrR), (Gdiplus::REAL)(2.0 * ctrR), (Gdiplus::REAL)(2.0 * ctrR));

	// dE ring labels along the upper-right diagonal (dropped in the small pane)
	if ( !compact )
	{
		gb.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
		Gdiplus::REAL fontPx = (Gdiplus::REAL)max(11.0, 0.068 * R);
		Gdiplus::Font labelFont(L"Segoe UI", fontPx, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
		Gdiplus::SolidBrush labelBrush(bDark ? Gdiplus::Color(210, 205, 205, 205) : Gdiplus::Color(220, 95, 99, 106));
		static const double labelFrac[4] = { 0.25, 0.50, 0.75, 1.00 };
		static const WCHAR * labelText[4] = { L"1", L"3", L"5", L"10" };
		const double diag = 0.70710678;
		for ( int i = 0; i < 4; i++ )
		{
			Gdiplus::PointF pos((Gdiplus::REAL)(cx + labelFrac[i] * R * diag + 2.0),
								(Gdiplus::REAL)(cy - labelFrac[i] * R * diag - fontPx - 2.0));
			gb.DrawString(labelText[i], -1, &labelFont, pos, &labelBrush);
		}
	}
}

void CTargetWnd::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
	Invalidate(FALSE);	// background rebuild happens lazily in OnPaint
}

BOOL CTargetWnd::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;	
}

void CTargetWnd::OnContextMenu(CWnd* pWnd, CPoint point)
{
	// load and display popup menu, extended with the dE tolerance presets
	// (also configurable in the advanced settings; drives every dE indicator)
	static const UINT ids[CColorHCFRConfig::DE_PRESET_COUNT] =
		{ IDS_DEPRESET_REFERENCE, IDS_DEPRESET_PROFESSIONAL, IDS_DEPRESET_CONSUMER, IDS_DEPRESET_RELAXED };

	CNewMenu menu;
	menu.LoadMenu(IDR_WHATS_THIS);
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup);

	pPopup->AppendMenu(MF_SEPARATOR);
	for ( int i = 0; i < CColorHCFRConfig::DE_PRESET_COUNT; i++ )
	{
		CString name;
		name.LoadString(ids[i]);
		double good, warn;
		GetConfig()->GetDEThresholdsFor(i, good, warn);
		CString item;
		item.Format("%s (dE %g)", (LPCSTR)name, warn);	// show the tolerance (fail) limit
		UINT flags = MF_STRING | ( GetConfig()->m_dE_preset == i ? MF_CHECKED : 0 );
		pPopup->AppendMenu(flags, ID_TARGET_TOL_FIRST + i, item);
	}

	int cmd = pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL | TPM_RETURNCMD,
		point.x, point.y, this);
	if ( cmd >= ID_TARGET_TOL_FIRST && cmd < ID_TARGET_TOL_FIRST + CColorHCFRConfig::DE_PRESET_COUNT )
	{
		GetConfig()->m_dE_preset = cmd - ID_TARGET_TOL_FIRST;
		GetConfig()->WriteProfileInt("Advanced","dE_preset",GetConfig()->m_dE_preset);
		// Repaint every live target widget and its RGB-levels sibling; the
		// measures grid recolours on the next measurement refresh.
		for ( size_t i = 0; i < s_targetWnds.size(); i++ )
			if ( ::IsWindow(s_targetWnds[i]->m_hWnd) )
			{
				s_targetWnds[i]->Invalidate(FALSE);
				CWnd * par = s_targetWnds[i]->GetParent();
				if ( par && par->IsKindOf(RUNTIME_CLASS(CMainView)) )
					((CMainView *)par)->m_RGBLevels.Invalidate(FALSE);
			}
	}
	else if ( cmd != 0 )
		SendMessage(WM_COMMAND, cmd);
}

void CTargetWnd::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_CTRL_TARGET, NULL );
}

