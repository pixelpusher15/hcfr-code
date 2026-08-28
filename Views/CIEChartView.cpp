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

// CIEChartView.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "MainFrm.h"
#include "DataSetDoc.h"
#include "DocTempl.h"
#include "CIEChartView.h"
#include <math.h>
#include "ximage.h"
#include "savegraphdialog.h"
#include "graphcontrol.h"
#include "GdiPlusAA.h"
#include "GamutCoverage.h"
#include "GamutName.h"
#include "Views\MainView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern void DrawCIEChart(CDC* pDC,int aWidth, int aHeight, BOOL doFullChart, BOOL doShowBlack, BOOL bCIEuv, BOOL bCIEab);
extern void DrawDeltaECurve(CDC* pDC, int cxMax, int cyMax, double DeltaE, BOOL bCIEuv, BOOL bCIEab );

#define FX_MINSIZETOSHOW_SCALEDETAILS 300
#define FX_MINSIZETOSHOW_TRIANGLEDETAILS 100
#define FX_MINSIZETOSHOW_REFDETAILS 300

// Zoom limits. The retained chart bitmaps are canvas-sized, so the canvas
// cap bounds their memory (~64MB per bitmap at 4096px, which a 32-bit
// process can afford); it is what actually limits zoom on large panes.
#define FX_MAXZOOMFACTOR   8000
#define FX_MAXZOOMCANVASPX 4096

// The HDTV reference used to derive display colours for plot dots and
// tooltips is built from fixed constants only (verified: the constructor and
// the reference luma getters read nothing mutable), so build it once instead
// of once per point per paint — its constructor inverts matrices.
static const CColorReference & HdtvPlotRef()
{
	static const CColorReference hdtvRef(HDTV);
	return hdtvRef;
}

/////////////////////////////////////////////////////////////////////////////
// CCIEGraphPoint

CCIEGraphPoint::CCIEGraphPoint(const ColorXYZ& color, double WhiteYRef, CString aName, BOOL bConvertCIEuv, BOOL bConvertCIEab) :
    name(aName),
    bCIEuv(bConvertCIEuv),
	bCIEab(bConvertCIEab),
    m_color(color),
	a_color(color)
{
	CColorReference  bRef = ((GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4)?CColorReference(UHDTV2):(GetColorReference().m_standard == HDTVa || GetColorReference().m_standard == HDTVb)?CColorReference(HDTV):GetColorReference());
    m_color = ColorXYZ(color / WhiteYRef);
    a_color = ColorXYZ(color);
	YWhite = WhiteYRef;
    ColorxyY colorxyY(color);
    ColorLab colorLab(color, WhiteYRef, bRef);
    double aX = colorxyY[0]; 
    double aY = colorxyY[1];
    a = colorLab[1]; 
    b = colorLab[2];
	L = colorLab[0];
	if ( bCIEuv )
	{
		x = ( 4.0 * aX ) / ( (-2.0 * aX ) + ( 12.0 * aY ) + 3.0 );
		y = ( 9.0 * aY ) / ( (-2.0 * aX ) + ( 12.0 * aY ) + 3.0 );
	}
	else
	{
		x = aX; 
		y = aY; 
	}
}

int CCIEGraphPoint::GetGraphX(CRect rect) const
{
	if (bCIEab)
		return (int)((a+220)*(double)rect.Width()/(400));

	return (int)((x+.075)*(double)rect.Width()/(bCIEuv?0.8:0.9));	// graph is from 0 to 0.8 in width
}

int CCIEGraphPoint::GetGraphY(CRect rect) const
{
	if (bCIEab)
		return (int)((double)rect.bottom - (b+200)*(double)rect.Height()/(400));

	return (int)((double)rect.bottom - (y+.05)*(double)rect.Height()/(bCIEuv?0.8:1.0));	// graph is from 0 to 0.9 in height
}

CPoint CCIEGraphPoint::GetGraphPoint(CRect rect) const
{
	return CPoint(GetGraphX(rect),GetGraphY(rect));
}


/////////////////////////////////////////////////////////////////////////////
// CCIEChartGrapher

CCIEChartGrapher::CCIEChartGrapher()
{
	m_refRedPrimaryBitmap.LoadBitmap(IDB_REFREDPRIMARY_BITMAP);
	m_refGreenPrimaryBitmap.LoadBitmap(IDB_REFGREENPRIMARY_BITMAP);
	m_refBluePrimaryBitmap.LoadBitmap(IDB_REFBLUEPRIMARY_BITMAP);
	m_refYellowSecondaryBitmap.LoadBitmap(IDB_REFYELLOWSECONDARY_BITMAP);
	m_refCyanSecondaryBitmap.LoadBitmap(IDB_REFCYANSECONDARY_BITMAP);
	m_refMagentaSecondaryBitmap.LoadBitmap(IDB_REFMAGENTASECONDARY_BITMAP);
	m_illuminantPointBitmap.LoadBitmap(IDB_ILLUMINANTPOINT_BITMAP);
	m_colorTempPointBitmap.LoadBitmap(IDB_COLORTEMPPOINT_BITMAP);
	m_redPrimaryBitmap.LoadBitmap(IDB_REDPRIMARY_BITMAP);
	m_greenPrimaryBitmap.LoadBitmap(IDB_GREENPRIMARY_BITMAP);
	m_bluePrimaryBitmap.LoadBitmap(IDB_BLUEPRIMARY_BITMAP);
	m_yellowSecondaryBitmap.LoadBitmap(IDB_YELLOWSECONDARY_BITMAP);
	m_cyanSecondaryBitmap.LoadBitmap(IDB_CYANSECONDARY_BITMAP);
	m_magentaSecondaryBitmap.LoadBitmap(IDB_MAGENTASECONDARY_BITMAP);
	m_redSatRefBitmap.LoadBitmap(IDB_REFREDSAT_BITMAP);
	m_greenSatRefBitmap.LoadBitmap(IDB_REFGREENSAT_BITMAP);
	m_blueSatRefBitmap.LoadBitmap(IDB_REFBLUESAT_BITMAP);
	m_yellowSatRefBitmap.LoadBitmap(IDB_REFYELLOWSAT_BITMAP);
	m_cyanSatRefBitmap.LoadBitmap(IDB_REFCYANSAT_BITMAP);
	m_magentaSatRefBitmap.LoadBitmap(IDB_REFMAGENTASAT_BITMAP);
	m_cc24SatRefBitmap.LoadBitmap(IDB_REFCC24SAT_BITMAP);
	m_grayPlotBitmap.LoadBitmap(IDB_GRAYPLOT_BITMAP);
	m_measurePlotBitmap.LoadBitmap(IDB_MEASUREPLOT_BITMAP);
	m_selectedPlotBitmap.LoadBitmap(IDB_SELECTEDPLOT_BITMAP);

	m_datarefRedBitmap.LoadBitmap(IDB_REFCROSS_RED);
	m_datarefGreenBitmap.LoadBitmap(IDB_REFCROSS_GREEN);
	m_datarefBlueBitmap.LoadBitmap(IDB_REFCROSS_BLUE);
	m_datarefYellowBitmap.LoadBitmap(IDB_REFCROSS_YELLOW);
	m_datarefCyanBitmap.LoadBitmap(IDB_REFCROSS_CYAN);
	m_datarefMagentaBitmap.LoadBitmap(IDB_REFCROSS_MAGENTA);

	m_doDisplayBackground=GetConfig()->GetProfileInt("CIE Chart","Display Background",TRUE);
	m_doDisplayDeltaERef=GetConfig()->GetProfileInt("CIE Chart","Display Delta E",FALSE);
	m_doShowReferences=GetConfig()->GetProfileInt("CIE Chart","Show References",TRUE);
	m_doShowDataRef=GetConfig()->GetProfileInt("CIE Chart","Show Reference Data",TRUE);
	m_doShowGrayScale=GetConfig()->GetProfileInt("CIE Chart","Display GrayScale",TRUE);
	m_doShowSaturationScale=GetConfig()->GetProfileInt("CIE Chart","Display Saturation Scale",TRUE);
	m_doShowSaturationScaleTarg=GetConfig()->GetProfileInt("CIE Chart","Display Saturation Scale Targets",TRUE);
	m_doShowCCScale=GetConfig()->GetProfileInt("CIE Chart","Display Color Checker Measures",TRUE);
	m_doShowCCScaleTarg=GetConfig()->GetProfileInt("CIE Chart","Display Color Checker Targets",TRUE);
	m_doShowMeasurements=GetConfig()->GetProfileInt("CIE Chart","Show Measurements",TRUE);
	m_bCIEuv=GetConfig()->GetProfileInt("CIE Chart","CIE uv mode",FALSE);
	m_bCIEab=GetConfig()->GetProfileInt("CIE Chart","CIE ab mode",FALSE);
	m_bdE10=GetConfig()->GetProfileInt("CIE Chart","Worst dE",FALSE);

	m_ZoomFactor = 1000;
	m_DeltaX = 0;
	m_DeltaY = 0;
	dE10 = 0.;
	isSat = FALSE;

	m_covValid = FALSE;

	m_pMarkerGraphics = NULL;
	m_markerScale = 1.0f;
	m_passTmWhite = 0.0;
	m_pPassRef = NULL;
	m_bgW = -1;
	m_bgH = -1;
	m_bgWhite = m_bgUv = m_bgAb = m_bgShowBg = m_bgShowDE = FALSE;
	m_bgWhitex = m_bgWhitey = 0.0;
}

std::vector <COLORREF> stRGB,eRGB;

void CCIEChartGrapher::MakeBgBitmap(CRect rect, BOOL bWhiteBkgnd)	// Create background bitmap
{
	// The source chart bitmaps are built by a startup thread: wait for them
	// before building (and caching) a background from their content.
	CColorHCFRApp * pApp = GetColorApp();
	if ( pApp -> m_hCIEEvent && WAIT_TIMEOUT == WaitForSingleObject ( pApp -> m_hCIEEvent, 0 ) )
	{
		CWaitCursor wait;
		if ( pApp -> m_hCIEThread )
			::SetThreadPriority ( pApp -> m_hCIEThread, THREAD_PRIORITY_NORMAL );
		WaitForSingleObject ( pApp -> m_hCIEEvent, INFINITE );
	}

	// Skip the rebuild when nothing that affects the background changed:
	// during a live resize OnSize and OnUpdate both come through here, and
	// the double HALFTONE StretchBlt of the 1400x1000 chart is expensive.
	ColorxyY bgWhite ( GetColorReference().GetWhite() );
	if ( rect.Width() == m_bgW && rect.Height() == m_bgH
	  && bWhiteBkgnd == m_bgWhite && m_bCIEuv == m_bgUv && m_bCIEab == m_bgAb
	  && m_doDisplayBackground == m_bgShowBg && m_doDisplayDeltaERef == m_bgShowDE
	  && bgWhite[0] == m_bgWhitex && bgWhite[1] == m_bgWhitey )
		return;

	m_bgW = rect.Width();
	m_bgH = rect.Height();
	m_bgWhite = bWhiteBkgnd;
	m_bgUv = m_bCIEuv;
	m_bgAb = m_bCIEab;
	m_bgShowBg = m_doDisplayBackground;
	m_bgShowDE = m_doDisplayDeltaERef;
	m_bgWhitex = bgWhite[0];
	m_bgWhitey = bgWhite[1];

    int		i;
	CDC		ScreenDC;
	
	ScreenDC.CreateDC ( "DISPLAY", NULL, NULL, NULL );

    CDC bgDC;
    bgDC.CreateCompatibleDC(&ScreenDC);


    if(m_drawBitmap.m_hObject)
        m_drawBitmap.DeleteObject();
    m_drawBitmap.CreateCompatibleBitmap(&ScreenDC,rect.Width(),rect.Height());

    if(m_bgBitmap.m_hObject)
        m_bgBitmap.DeleteObject();
    m_bgBitmap.CreateCompatibleBitmap(&ScreenDC,rect.Width(),rect.Height());

    if(m_gamutBitmap.m_hObject)
        m_gamutBitmap.DeleteObject();
	if(m_doDisplayBackground)
	    m_gamutBitmap.CreateCompatibleBitmap(&ScreenDC,rect.Width(),rect.Height());

	BITMAP bm;
	GetColorApp() -> m_chartBitmap.GetBitmap(&bm);

    CBitmap *pOldBitmap=bgDC.SelectObject(&m_bgBitmap);
	int oldMode=bgDC.GetStretchBltMode();

	CDC memDC;
	memDC.CreateCompatibleDC( &ScreenDC );

	ScreenDC.DeleteDC ();
	
	if(m_doDisplayBackground)
	{
		CBitmap* pOld;
		
		if ( bWhiteBkgnd )
		{
			if ( m_bCIEuv )
				pOld = memDC.SelectObject( & GetColorApp() -> m_chartBitmap_uv_white );
			else if (m_bCIEab)
				pOld = memDC.SelectObject( & GetColorApp() -> m_chartBitmap_ab_white );
			else
				pOld = memDC.SelectObject( & GetColorApp() -> m_chartBitmap_white );
		}
		else		
		{
			if ( m_bCIEuv )
				pOld = memDC.SelectObject( & GetColorApp() -> m_chartBitmap_uv );
			else if (m_bCIEab)
				pOld = memDC.SelectObject( & GetColorApp() -> m_chartBitmap_ab );
			else
				pOld = memDC.SelectObject( & GetColorApp() -> m_chartBitmap );
		}

		bgDC.SetStretchBltMode(HALFTONE);
		SetBrushOrgEx(bgDC, 0,0, NULL);
		bgDC.StretchBlt(0,0,rect.right,rect.bottom,&memDC,0,0,bm.bmWidth,bm.bmHeight,SRCCOPY);
	    memDC.SelectObject(pOld);
	}
	else
		bgDC.FillSolidRect(rect,bWhiteBkgnd?RGB(255,255,255):RGB(0,0,0));

	if ( m_doDisplayDeltaERef )
	{
		DrawDeltaECurve(&bgDC, rect.Width(),rect.Height(), 3.0, m_bCIEuv, m_bCIEab );
		DrawDeltaECurve(&bgDC, rect.Width(),rect.Height(), 10.0, m_bCIEuv, m_bCIEab );
	}

	// Draw axis
	bgDC.SetTextAlign(TA_BOTTOM);
	bgDC.SetTextColor(bWhiteBkgnd?RGB(0,0,0):RGB(255,255,255));
	bgDC.SetBkMode(TRANSPARENT);

	// Initializes a CFont object with the specified characteristics. 
	CFont font;
	font.CreateFont(16,0,0,0,FW_THIN,FALSE,FALSE,FALSE,0,OUT_TT_ONLY_PRECIS,CLIP_DEFAULT_PRECIS,PROOF_QUALITY,VARIABLE_PITCH | FF_SWISS,_T("Segoe UI"));

	CFont* pOldFont = bgDC.SelectObject(&font);

	BOOL bShowLabels = min(rect.Width(),rect.Height()) > FX_MINSIZETOSHOW_REFDETAILS;

	// Grid lines as a translucent overlay (GDI+): reads as a quiet guide over
	// both the dark surround and the coloured tongue instead of hard lines.
	// Draw every grid line first and let the GDI+ Graphics release the DC
	// before any GDI TextOut -- GDI and GDI+ must not interleave on one HDC.
	EnsureGdiplus();
	{
		Gdiplus::Graphics gridG(bgDC.GetSafeHdc());
		Gdiplus::Pen gridPen(bWhiteBkgnd ? Gdiplus::Color(50,0,0,0) : Gdiplus::Color(50,255,255,255), 1.0f);

		for(i=0;i<(m_bCIEab?21.0:m_bCIEuv?7:8);i++)	// X axis grid
		{
			int x=(int)(rect.Width()*((i + 0.75)/(m_bCIEuv?8.0:9.0)));
			if (m_bCIEab)
				x=(int)(rect.Width()*((i)/20.));
			gridG.DrawLine(&gridPen,x,0,x,(int)rect.bottom);
		}

		for(i=0;i<(m_bCIEab?20.0:m_bCIEuv?7:9);i++) 	// Y axis grid
		{
			int y=(int)(rect.Height()*((i + 0.5)/(m_bCIEuv?8.0:10.0)));
			if (m_bCIEab)
				y=(int)(rect.Height()*((i)/20.));
			gridG.DrawLine(&gridPen,0,y,(int)rect.right,y);
		}
	}

	// Axis labels (GDI), after the GDI+ Graphics above has been destroyed
	if (bShowLabels)
	{
		for(i=1;i<(m_bCIEab?21.0:m_bCIEuv?7:8);i++)	// X axis labels
		{
			int x=(int)(rect.Width()*((i + 0.75)/(m_bCIEuv?8.0:9.0)));
			if (m_bCIEab)
				x=(int)(rect.Width()*((i)/20.));
			CString str;
			if (m_bCIEab)
				str.Format("%.1f",i*20.0-220.0);
			else
				str.Format("%.1f",i/10.0);
			bgDC.TextOut(x+2,rect.bottom,str);
		}

		for(i=1;i<(m_bCIEab?20.0:m_bCIEuv?7:9);i++) 	// Y axis labels
		{
			int y=(int)(rect.Height()*((i + 0.5)/(m_bCIEuv?8.0:10.0)));
			if (m_bCIEab)
				y=(int)(rect.Height()*((i)/20.));
			CString str;
			if (m_bCIEab)
				str.Format("%.1f",200.0-i*20.0);
			else
				str.Format("%.1f",(m_bCIEuv?0.7:0.9)-i/10.0);
			bgDC.TextOut(2,y,str);
		}
	}

	bgDC.SelectObject(pOldFont);
	
	// Create stretched bitmap for gamut hilighting

	if(m_doDisplayBackground)
	{
		bgDC.SelectObject(&m_gamutBitmap);
		GetColorApp() -> m_lightenChartBitmap.GetBitmap(&bm);
		CBitmap* pOld = memDC.SelectObject(m_bCIEab ? & GetColorApp() -> m_lightenChartBitmap_ab: (m_bCIEuv ? & GetColorApp() -> m_lightenChartBitmap_uv : & GetColorApp() -> m_lightenChartBitmap));
		bgDC.SetStretchBltMode(HALFTONE);
		SetBrushOrgEx(bgDC, 0,0, NULL);

		bgDC.StretchBlt(0,0,rect.right,rect.bottom,&memDC,0,0,bm.bmWidth,bm.bmHeight,SRCCOPY);

		memDC.SelectObject(pOld);

		if ( m_doDisplayDeltaERef )
		{
			DrawDeltaECurve(&bgDC, rect.Width(),rect.Height(), 3.0, m_bCIEuv, m_bCIEab );
			DrawDeltaECurve(&bgDC, rect.Width(),rect.Height(), 10.0, m_bCIEuv, m_bCIEab );
		}
	}

	bgDC.SetStretchBltMode(oldMode); 
	bgDC.SelectObject(pOldBitmap);
}

// Modern anti-aliased markers replacing the legacy point bitmaps: measured
// values plot as dots filled with their own colour inside a light ring,
// reference targets as outline shapes, reference-document data as crosses.
// Returns false for a bitmap with no mapped style (caller falls back to the
// legacy alpha blit).
bool CCIEChartGrapher::DrawGdiPlusMarker(CDC *pDC, CBitmap *pBitmap, int x, int y, const CCIEGraphPoint& aGraphPoint, bool isSelected)
{
	enum MarkerKind { KIND_DOT, KIND_TARGET, KIND_DIAMOND, KIND_CROSS };
	MarkerKind kind;
	COLORREF clr = RGB(255,255,255);

	if ( pBitmap == &m_grayPlotBitmap || pBitmap == &m_measurePlotBitmap || pBitmap == &m_selectedPlotBitmap
	  || pBitmap == &m_redPrimaryBitmap || pBitmap == &m_greenPrimaryBitmap || pBitmap == &m_bluePrimaryBitmap
	  || pBitmap == &m_yellowSecondaryBitmap || pBitmap == &m_cyanSecondaryBitmap || pBitmap == &m_magentaSecondaryBitmap )
	{
		kind = KIND_DOT;
		CColor measColor = aGraphPoint.GetNormalizedColor();
		ColorRGB measCol = ColorRGB(measColor.GetRGBValue(HdtvPlotRef()));
		int r = (int)floor(pow(min(max(measCol[0],0),1),1.0/2.2)*255.+0.5);
		int g = (int)floor(pow(min(max(measCol[1],0),1),1.0/2.2)*255.+0.5);
		int b = (int)floor(pow(min(max(measCol[2],0),1),1.0/2.2)*255.+0.5);
		clr = RGB(r,g,b);
	}
	// Primary/secondary reference targets are diamonds -- the distinct landmark
	// shape. Saturation-sweep and other targets are plain squares; a sweep's
	// 100% point lands on its primary vertex, so the diamond over the square
	// reads as two distinct series rather than one misaligned square.
	else if ( pBitmap == &m_refRedPrimaryBitmap )       { kind = KIND_DIAMOND; clr = RGB(255,55,55); }
	else if ( pBitmap == &m_refGreenPrimaryBitmap )     { kind = KIND_DIAMOND; clr = RGB(60,240,60); }
	else if ( pBitmap == &m_refBluePrimaryBitmap )      { kind = KIND_DIAMOND; clr = RGB(120,135,255); }
	else if ( pBitmap == &m_refYellowSecondaryBitmap )  { kind = KIND_DIAMOND; clr = RGB(255,240,60); }
	else if ( pBitmap == &m_refCyanSecondaryBitmap )    { kind = KIND_DIAMOND; clr = RGB(55,240,240); }
	else if ( pBitmap == &m_refMagentaSecondaryBitmap ) { kind = KIND_DIAMOND; clr = RGB(250,80,250); }
	else if ( pBitmap == &m_cc24SatRefBitmap )          { kind = KIND_TARGET; clr = RGB(235,235,235); }
	else if ( pBitmap == &m_redSatRefBitmap )        { kind = KIND_TARGET; clr = RGB(255,55,55); }
	else if ( pBitmap == &m_greenSatRefBitmap )      { kind = KIND_TARGET; clr = RGB(60,240,60); }
	else if ( pBitmap == &m_blueSatRefBitmap )       { kind = KIND_TARGET; clr = RGB(120,135,255); }
	else if ( pBitmap == &m_yellowSatRefBitmap )     { kind = KIND_TARGET; clr = RGB(255,240,60); }
	else if ( pBitmap == &m_cyanSatRefBitmap )       { kind = KIND_TARGET; clr = RGB(55,240,240); }
	else if ( pBitmap == &m_magentaSatRefBitmap )    { kind = KIND_TARGET; clr = RGB(250,80,250); }
	else if ( pBitmap == &m_illuminantPointBitmap )  { kind = KIND_TARGET; clr = RGB(235,235,235); }
	else if ( pBitmap == &m_colorTempPointBitmap )   { kind = KIND_TARGET; clr = RGB(200,200,200); }
	else if ( pBitmap == &m_datarefRedBitmap )     { kind = KIND_CROSS; clr = RGB(255,55,55); }
	else if ( pBitmap == &m_datarefGreenBitmap )   { kind = KIND_CROSS; clr = RGB(60,240,60); }
	else if ( pBitmap == &m_datarefBlueBitmap )    { kind = KIND_CROSS; clr = RGB(120,135,255); }
	else if ( pBitmap == &m_datarefYellowBitmap )  { kind = KIND_CROSS; clr = RGB(255,240,60); }
	else if ( pBitmap == &m_datarefCyanBitmap )    { kind = KIND_CROSS; clr = RGB(55,240,240); }
	else if ( pBitmap == &m_datarefMagentaBitmap ) { kind = KIND_CROSS; clr = RGB(250,80,250); }
	else
		return false;

	float s = m_markerScale;
	float fx = (float)x, fy = (float)y;
	// Thin dark underlay behind outline markers: keeps them readable over
	// the bright parts of the tongue without making the strokes heavier
	Gdiplus::Color haloClr(160,0,0,0);

	auto drawMarker = [&](Gdiplus::Graphics & g)
	{
	switch ( kind )
	{
		case KIND_DOT:
		{
			float r = 4.0f * s;
			Gdiplus::Pen contour(haloClr, 1.0f*s);
			g.DrawEllipse(&contour, fx-r-1.0f*s, fy-r-1.0f*s, 2.0f*(r+1.0f*s), 2.0f*(r+1.0f*s));
			Gdiplus::SolidBrush fill(GpColor(clr));
			g.FillEllipse(&fill, fx-r, fy-r, 2.0f*r, 2.0f*r);
			// Selection shows as a gold ring in place of the white one, so it
			// adds no footprint and never hides a target square around it
			BOOL bSel = ( isSelected || pBitmap == &m_selectedPlotBitmap );
			Gdiplus::Pen ring(bSel ? Gdiplus::Color(255,255,215,0) : Gdiplus::Color(255,255,255,255), bSel ? 1.7f*s : 1.2f*s);
			g.DrawEllipse(&ring, fx-r, fy-r, 2.0f*r, 2.0f*r);
			break;
		}
		case KIND_TARGET:
		{
			float h = 4.5f * s;
			Gdiplus::Pen halo(haloClr, 2.8f*s);
			g.DrawRectangle(&halo, fx-h, fy-h, 2.0f*h, 2.0f*h);
			Gdiplus::Pen pen(GpColor(clr), 1.5f*s);
			g.DrawRectangle(&pen, fx-h, fy-h, 2.0f*h, 2.0f*h);
			break;
		}
		case KIND_DIAMOND:
		{
			float h = 5.5f * s;
			Gdiplus::PointF d[4] = {
				Gdiplus::PointF(fx, fy-h), Gdiplus::PointF(fx+h, fy),
				Gdiplus::PointF(fx, fy+h), Gdiplus::PointF(fx-h, fy) };
			Gdiplus::Pen halo(haloClr, 2.8f*s);
			g.DrawPolygon(&halo, d, 4);
			Gdiplus::Pen pen(GpColor(clr), 1.5f*s);
			g.DrawPolygon(&pen, d, 4);
			break;
		}
		case KIND_CROSS:
		{
			float a = 4.5f * s;
			Gdiplus::Pen halo(haloClr, 2.8f*s);
			g.DrawLine(&halo, fx-a, fy, fx+a, fy);
			g.DrawLine(&halo, fx, fy-a, fx, fy+a);
			Gdiplus::Pen pen(GpColor(clr), 1.5f*s);
			g.DrawLine(&pen, fx-a, fy, fx+a, fy);
			g.DrawLine(&pen, fx, fy-a, fx, fy+a);
			break;
		}
	}
	};

	// Reuse the per-DrawChart Graphics (one per pass instead of one per
	// marker keeps live-resize repaints cheap); stack fallback otherwise.
	if ( m_pMarkerGraphics )
		drawMarker(*m_pMarkerGraphics);
	else
	{
		EnsureGdiplus();
		Gdiplus::Graphics g(pDC->GetSafeHdc());
		g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
		drawMarker(g);
	}
	return true;
}

void CCIEChartGrapher::DrawAlphaBitmap(CDC *pDC, const CCIEGraphPoint& aGraphPoint, CBitmap *pBitmap, CRect rect, CPPToolTip * pTooltip, CWnd * pWnd, CCIEGraphPoint * pRefPoint, bool isSelected, double dE10, bool isPrimeSat)
{
	ASSERT(pBitmap);
	ASSERT(pDC);
	BITMAP bm;
	pBitmap->GetBitmap(&bm);
	bool bDrawBMP = !m_bdE10;
	double RefWhite = 1.0, YWhite = 1.0, L = 100., a = 0., b = 0.;
	CColorReference  bRef = ( m_pPassRef ? *m_pPassRef :
		((GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4)?CColorReference(UHDTV2):(GetColorReference().m_standard == HDTVa || GetColorReference().m_standard == HDTVb)?CColorReference(HDTV):GetColorReference()) );

	if (isSelected)
		bDrawBMP = TRUE;

	// Add tootip if name is defined
	if(!aGraphPoint.name.IsEmpty() && pWnd && !isSelected)
	{
		CString str, str1, str2, str3;
		CColor NoDataColor;
		double tmWhite = ( m_passTmWhite > 0.0 ) ? m_passTmWhite : TmDiffuseWhiteNits(NoDataColor, NoDataColor);
		
		int x=aGraphPoint.GetGraphX(rect)+m_DeltaX;
		int y=aGraphPoint.GetGraphY(rect)+m_DeltaY;		
		
		// Note: remove 5 pixels for transparency area of bitmap
		CRect rect_tip(x-bm.bmWidth/2+5,y-bm.bmHeight/2+5,x+bm.bmWidth/2-5,y+bm.bmHeight/2-5);
		CColor aColor1 = aGraphPoint.GetNormalizedColor();

		bool dark = FALSE;
		
		if (aColor1.GetY() < 0.30)
			dark = TRUE;

		if ( m_bCIEuv )
			if (dark)
				str.Format("<font color=\"#A0EE80\">u': %.3f, v': %.3f\n",aGraphPoint.x,aGraphPoint.y);
			else
				str.Format("<font color=\"#004080\">u': %.3f, v': %.3f\n",aGraphPoint.x,aGraphPoint.y);
		else
			if (dark)
				str.Format("<font color=\"#A0EE80\">x: %.3f, y: %.3f, Y: %.2f%%\n",aGraphPoint.x,aGraphPoint.y,aGraphPoint.GetNormalizedColor()[1]*100);
			else
				str.Format("<font color=\"#004080\">x: %.3f, y: %.3f, Y: %.2f%%\n",aGraphPoint.x,aGraphPoint.y,aGraphPoint.GetNormalizedColor()[1]*100);


			str1.Format("L*a*b*: %.2f %.3f %.3f",aGraphPoint.L,aGraphPoint.a,aGraphPoint.b);
			str += str1;

		if ( pRefPoint )
		{
			double dC, dH;
         	CColor aColor2 = pRefPoint->GetNormalizedColor();
			
			if (GetConfig()->m_GammaOffsetType == 5 && GetConfig()->m_bHDR100 && !isSat && !isPrimeSat)
			{
				aColor2.SetX(aColor2.GetX()*105.95640);
				aColor2.SetY(aColor2.GetY()*105.95640);
				aColor2.SetZ(aColor2.GetZ()*105.95640);
			}
							

			CColor inColor = aGraphPoint.GetNormalizedColor();

			// Saturation-page points are already normalized on the unified
			// convention (ref by YWhite/10000 via the GetHDRRefScale-form
			// YWhiteRef, measurement by YWhite), which now matches the measures
			// grid - no further white adjustment. The legacy 94.37844/tmWhite
			// adjust remains only for the UHDTV3/4 primaries page (out of the
			// unified sat/CC scope). Identical with tone mapping off.
			if (GetConfig()->m_GammaOffsetType == 5 && (GetConfig()->m_colorStandard == UHDTV3 || GetConfig()->m_colorStandard == UHDTV4) && !isSat ) //check for primaries/secondaries page
			{
				RefWhite = 94.37844 / tmWhite;
				YWhite = YWhite * 94.37844 / tmWhite ;
			}

			if (GetConfig()->m_dE_form == 6) //dICtCp
			{
				YWhite = aGraphPoint.YWhite;
				inColor.SetX(inColor.GetX() * tmWhite);
				inColor.SetY(inColor.GetY() * tmWhite);
				inColor.SetZ(inColor.GetZ() * tmWhite);
				RefWhite = 1.0 * YWhite / tmWhite;
			}

			double dE  = inColor.GetDeltaE(YWhite, aColor2.GetXYZValue(), RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->gw_Weight );
            double dL  = inColor.GetDeltaLCH(YWhite, aColor2.GetXYZValue(), RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->gw_Weight, dC, dH );

//			if (GetConfig()->m_GammaOffsetType == 5) //check for primaries/secondaries page
//				RefWhite = RefWhite * (tmWhite) / 94.37844 ;					

			if (dE > dE10)
				bDrawBMP = TRUE;
//			str1.Format("L*a*b*: %.2f %.3f %.3f",aGraphPoint.L,aGraphPoint.a,aGraphPoint.b);

//			L  = ColorLab(aColor2.GetXYZValue(), RefWhite, bRef)[0];
//			a  = ColorLab(aColor2.GetXYZValue(), RefWhite, bRef)[1];
//			b  = ColorLab(aColor2.GetXYZValue(), RefWhite, bRef)[2];
			L  = pRefPoint->L;
			a  = pRefPoint->a;
			b  = pRefPoint->b;
			str1.Format("\nL*a*b*: %.2f %.3f %.3f <b>[Ref]</b>\n",L,a,b);
			str += str1;

			switch (GetConfig()->m_dE_form)
			{
				case 0:
					str2.Format ( "\n<font face=\"Symbol\">D</font>E [<font face=\"Symbol\">D</font>L*,<font face=\"Symbol\">D</font>u*,<font face=\"Symbol\">D</font>v*]: %.1f [%.1f,%.1f,%.1f]\n",dE,dL,dC,dH );
					break;
				case 1:
					str2.Format ( "\n<font face=\"Symbol\">D</font>E [<font face=\"Symbol\">D</font>L*,<font face=\"Symbol\">D</font>a*,<font face=\"Symbol\">D</font>b*]: %.1f [%.1f,%.1f,%.1f]\n",dE,dL,dC,dH );
					break;
				case 2:
				case 3:
				case 4:
				case 5:
					str2.Format ( "\n<font face=\"Symbol\">D</font>E [<font face=\"Symbol\">D</font>L*,<font face=\"Symbol\">D</font>C*,<font face=\"Symbol\">D</font>H*]: %.1f [%.1f,%.1f,%.1f]\n",dE,dL,dC,dH );
					break;
				case 6:
					str2.Format ( "\n<font face=\"Symbol\">D</font>E [<font face=\"Symbol\">D</font>L,<font face=\"Symbol\">D</font>M,<font face=\"Symbol\">D</font>S]: %.1f [%.1f,%.1f,%.1f]\n",dE,dL,dC,dH );
					break;
			}
				str3.LoadString (IDS_DISTANCEINCIEXY);
				str2 += str3; 

				double dXY = aGraphPoint.GetNormalizedColor().GetDeltaxy(pRefPoint->GetNormalizedColor(), bRef);
				str3.Format ( ": %.3f xy</font>", dXY );
				str2 += str3;
				aColor1 = aGraphPoint.GetNormalizedColor();

//				if (GetConfig()->m_GammaOffsetType == 5 && !(GetConfig()->m_colorStandard == UHDTV3 || GetConfig()->m_colorStandard == UHDTV4))
//				{
//					aColor2.SetX(aColor2.GetX()* 94.37844 / (tmWhite));
//					aColor2.SetY(aColor2.GetY()* 94.37844 / (tmWhite));
//					aColor2.SetZ(aColor2.GetZ()* 94.37844 / (tmWhite));
//				}

//				if (GetConfig()->m_GammaOffsetType == 5 && isSat && !(GetConfig()->m_colorStandard == UHDTV3 || GetConfig()->m_colorStandard == UHDTV4))
//				{
//					aColor1.SetX(aColor1.GetX()* 94.37844 / (tmWhite));
//					aColor1.SetY(aColor1.GetY()* 94.37844 / (tmWhite));
//					aColor1.SetZ(aColor1.GetZ()* 94.37844 / (tmWhite));
//				}

				if (GetConfig()->m_GammaOffsetType == 5 && isSat)// && !(GetConfig()->m_colorStandard == UHDTV3 || GetConfig()->m_colorStandard == UHDTV4))
				{
					aColor1.SetX(aColor1.GetX() * YWhite / tmWhite);
					aColor1.SetY(aColor1.GetY() * YWhite / tmWhite);
					aColor1.SetZ(aColor1.GetZ() * YWhite / tmWhite);
					aColor2.SetX(aColor2.GetX() * YWhite / tmWhite);
					aColor2.SetY(aColor2.GetY() * YWhite / tmWhite);
					aColor2.SetZ(aColor2.GetZ() * YWhite / tmWhite);
				}

				ColorRGB measCol = ColorRGB(aColor1.GetRGBValue(HdtvPlotRef()));
				ColorRGB refCol = ColorRGB(aColor2.GetRGBValue(HdtvPlotRef()));
				double r1=min(max(measCol[0],0),1);
				double g1=min(max(measCol[1],0),1);
				double b1=min(max(measCol[2],0),1);
				double r2=min(max(refCol[0],0),1);
				double g2=min(max(refCol[1],0),1);
				double b2=min(max(refCol[2],0),1);
				
				stRGB.push_back(RGB(floor(pow(r1,1.0/2.2)*255.+0.5),floor(pow(g1,1.0/2.2)*255.+0.5),floor(pow(b1,1.0/2.2)*255.+0.5)));
				eRGB.push_back(RGB(floor(pow(r2,1.0/2.2)*255.+0.5),floor(pow(g2,1.0/2.2)*255.+0.5),floor(pow(b2,1.0/2.2)*255.+0.5)));

				if (dark)
					pTooltip -> AddTool(pWnd, "<b><font color=\"#EFEFEF\">"+CString(aGraphPoint.name) +"</font></b> \n\n\n" +str+str2,&rect_tip, m_ttID);
				else
					pTooltip -> AddTool(pWnd, "<b><font color=\"#101010\">"+CString(aGraphPoint.name) +"</font></b> \n\n\n" +str+str2,&rect_tip, m_ttID);

					m_ttID++;
		}
		else
		{
			CColor aColor = aGraphPoint.GetNormalizedColor();

			ColorRGB measCol = ColorRGB(aColor.GetRGBValue(HdtvPlotRef()));
			double r1=min(max(measCol[0],0),1);
			double g1=min(max(measCol[1],0),1);
			double b1=min(max(measCol[2],0),1);
			
			stRGB.push_back(RGB(floor(pow(r1,1.0/2.2)*255.+0.5),floor(pow(g1,1.0/2.2)*255.+0.5),floor(pow(b1,1.0/2.2)*255.+0.5)));
			eRGB.push_back(RGB(floor(pow(r1,1.0/2.2)*255.+0.5),floor(pow(g1,1.0/2.2)*255.+0.5),floor(pow(b1,1.0/2.2)*255.+0.5)));

			if (dark)
				pTooltip -> AddTool(pWnd, "<b><font color=\"#EFEFEF\">"+CString(aGraphPoint.name) +"</font></b> \n" +str+str2,&rect_tip, m_ttID);
			else
				pTooltip -> AddTool(pWnd, "<b><font color=\"#101010\">"+CString(aGraphPoint.name) +"</font></b> \n" +str+str2,&rect_tip, m_ttID);

				m_ttID++;

			bDrawBMP = TRUE;
		}
		
	}

	if (bDrawBMP)
	{
		if ( ! DrawGdiPlusMarker(pDC, pBitmap, aGraphPoint.GetGraphX(rect), aGraphPoint.GetGraphY(rect), aGraphPoint, isSelected) )
		{
			// Legacy alpha-bitmap blit for any marker without a mapped style
			CDC memDC;
			memDC.CreateCompatibleDC( pDC );

			CBitmap* pOld = memDC.SelectObject(pBitmap);
			BLENDFUNCTION bf;
			bf.BlendOp=AC_SRC_OVER;
			bf.BlendFlags=0;
			bf.AlphaFormat=0x01;  // AC_SRC_ALPHA=0x01
			bf.SourceConstantAlpha=255;
			AlphaBlend(*pDC,aGraphPoint.GetGraphX(rect)-bm.bmWidth/2,aGraphPoint.GetGraphY(rect)-bm.bmHeight/2,bm.bmWidth,bm.bmHeight,memDC,0,0,bm.bmWidth,bm.bmHeight,bf);

			memDC.SelectObject(pOld);
		}
	}

}

// Gamut coverage chips (top right): [gamut] [xy: n%] [u'v': n%]. The
// gamut-name chip always shows; the coverage percentages are the measured
// primaries triangle vs the displayed reference triangle (area intersection
// / reference area) in xy and u'v'. Not shown in CIE a*b* mode (the metric
// is a chromaticity-plane one). The percentages hide when the gamut is
// changed until the primaries are re-measured, since old primaries don't
// belong to the new reference.
// The row anchors to rcAnchor's top-right: the client rect when overlaid on
// screen paints (OnDraw calls this after every blit so the chips stay pinned
// under zoom/pan), the image rect when baked into exports.
void CCIEChartGrapher::DrawCoverageChips ( CDC * pDC, CRect rcAnchor, CDataSetDoc * pDoc )
{
	if ( m_bCIEab || min(rcAnchor.Width(),rcAnchor.Height()) <= FX_MINSIZETOSHOW_REFDETAILS )
		return;

	ColorXYZ redPrimaryColor=pDoc->GetMeasure()->GetRedPrimary().GetXYZValue();
	ColorXYZ greenPrimaryColor=pDoc->GetMeasure()->GetGreenPrimary().GetXYZValue();
	ColorXYZ bluePrimaryColor=pDoc->GetMeasure()->GetBluePrimary().GetXYZValue();
	BOOL hasPrimaries = redPrimaryColor.isValid() && greenPrimaryColor.isValid() &&
						bluePrimaryColor.isValid();

	WCHAR gamutName[64];
	GamutShortName(GetColorReference(), gamutName, 64);

	// Decide whether the coverage percentages are current. They are stale
	// (and hidden) if the reference standard changed while the measured
	// primaries stayed put -- i.e. the gamut was switched without a fresh
	// measurement. Re-measuring changes the primaries and re-shows them.
	ColorStandard curStd = GetColorReference().m_standard;
	BOOL showPct = FALSE;
	double covXY = 0.0, covUV = 0.0;
	if (hasPrimaries)
	{
		BOOL primsSame = m_covValid
			&& redPrimaryColor[0]   == m_covPrimaries[0][0] && redPrimaryColor[1]   == m_covPrimaries[0][1] && redPrimaryColor[2]   == m_covPrimaries[0][2]
			&& greenPrimaryColor[0] == m_covPrimaries[1][0] && greenPrimaryColor[1] == m_covPrimaries[1][1] && greenPrimaryColor[2] == m_covPrimaries[1][2]
			&& bluePrimaryColor[0]  == m_covPrimaries[2][0] && bluePrimaryColor[1]  == m_covPrimaries[2][1] && bluePrimaryColor[2]  == m_covPrimaries[2][2];
		BOOL stdSame = m_covValid && (m_covStandard == curStd);

		if (m_covValid && !stdSame && primsSame)
		{
			showPct = FALSE;	// gamut changed, primaries not re-measured
		}
		else
		{
			showPct = TRUE;
			ColorxyY measured[3] = { ColorxyY(redPrimaryColor),
									 ColorxyY(greenPrimaryColor),
									 ColorxyY(bluePrimaryColor) };
			// the triangle drawn on the chart is the active reference's primaries
			// (for P3/709 inside a 2020 container these are already the inner gamut)
			ColorxyY refTri[3] = { ColorxyY(GetColorReference().GetRed()),
								   ColorxyY(GetColorReference().GetGreen()),
								   ColorxyY(GetColorReference().GetBlue()) };
			covXY = GamutCoverage(measured, refTri, GAMUT_PLANE_XY) * 100.0;
			covUV = GamutCoverage(measured, refTri, GAMUT_PLANE_UV) * 100.0;
			m_covStandard = curStd;
			m_covPrimaries[0] = redPrimaryColor;
			m_covPrimaries[1] = greenPrimaryColor;
			m_covPrimaries[2] = bluePrimaryColor;
			m_covValid = TRUE;
		}
	}

	// Pill stat chips like the target widget's, anchored top right,
	// growing leftward from the corner.
	EnsureGdiplus();
	Gdiplus::Graphics g(pDC->GetSafeHdc());
	GpApplyDCOrigin(g, pDC);
	g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
	Gdiplus::Font chipFont(L"Segoe UI", 15.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

	// neutral chip palette shared with the target widget's dx/dy chip
	BOOL bDark = !m_bgWhite;
	Gdiplus::Color nFill   = bDark ? Gdiplus::Color(255, 42, 42, 42)    : Gdiplus::Color(255, 255, 255, 255);
	Gdiplus::Color nBorder = bDark ? Gdiplus::Color(255, 72, 72, 72)    : Gdiplus::Color(255, 205, 207, 213);
	Gdiplus::Color nText   = bDark ? Gdiplus::Color(255, 215, 215, 215) : Gdiplus::Color(255, 70, 74, 80);

	// Visual order left-to-right is [gamut] [xy] [u'v']; priority is the
	// reverse, so if the row can't fit the chart width drop u'v' first,
	// then xy, always keeping the gamut name.
	WCHAR uvBuf[64], xyBuf[64];
	const WCHAR * ordered[3];
	int nChips = 0;
	ordered[nChips++] = gamutName;
	if (showPct)
	{
		swprintf_s(xyBuf, 64, L"xy: %.1f%%", covXY);
		swprintf_s(uvBuf, 64, L"u'v': %.1f%%", covUV);
		ordered[nChips++] = xyBuf;
		ordered[nChips++] = uvBuf;
	}

	const Gdiplus::REAL pad = 8.0f, gap = 6.0f;
	Gdiplus::REAL avail = (Gdiplus::REAL)rcAnchor.Width() - 2.0f * pad;
	Gdiplus::REAL total = 0.0f;
	for (int i = 0; i < nChips; i++)
		total += MeasureStatChip(g, chipFont, ordered[i]).Width + (i ? gap : 0.0f);
	while (nChips > 1 && total > avail)	// drop lowest-priority (rightmost) chips
		total -= MeasureStatChip(g, chipFont, ordered[--nChips]).Width + gap;

	Gdiplus::REAL bottom = (Gdiplus::REAL)rcAnchor.top + pad + MeasureStatChip(g, chipFont, gamutName).Height;
	Gdiplus::REAL xRight = (Gdiplus::REAL)rcAnchor.right - pad;
	for (int i = nChips - 1; i >= 0; i--)	// draw right to left
		xRight -= DrawStatChip(g, chipFont, ordered[i], xRight, bottom, nFill, nBorder, nText) + gap;
}

void CCIEChartGrapher::DrawChart(CDataSetDoc * pDoc, CDC* pDC, CRect rect, CPPToolTip * pTooltip, CWnd * pWnd)
{
	// One shared anti-aliased Graphics for every marker drawn in this pass
	// (constructing a Graphics per marker is measurable with 100+ points)
	EnsureGdiplus();
	Gdiplus::Graphics chartMarkerGraphics(pDC->GetSafeHdc());
	chartMarkerGraphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	struct CMarkerGraphicsScope
	{
		CCIEChartGrapher * p;
		~CMarkerGraphicsScope() { p->m_pMarkerGraphics = NULL; p->m_passTmWhite = 0.0; p->m_pPassRef = NULL; }
	} markerScope = { this };
	m_pMarkerGraphics = & chartMarkerGraphics;
	m_markerScale = (float)GetConfig()->Scale(100) / 100.0f;	// resolve DPI once per pass, not per marker

	CColorHCFRApp *	pApp = GetColorApp();
	CString			Msg, Msg2, Msg3;
	CDataSetDoc *	pDataRef = GetDataRef();
	POSITION pos = pDoc -> GetFirstViewPosition ();
	CView *pView = pDoc->GetNextView(pos);
	int current_mode = ((CMainView*)pView)->m_displayMode;
	BOOL onEdit =  ((CMainView*)pView)->m_editCheckButton.GetCheck();
	CColor NoDataColor;
	double tmWhite = TmDiffuseWhiteNits(NoDataColor, NoDataColor);
	CColorReference  bRef = ((GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4)?CColorReference(UHDTV2):(GetColorReference().m_standard == HDTVa || GetColorReference().m_standard == HDTVb)?CColorReference(HDTV):GetColorReference());

	// Publish the pass invariants so DrawAlphaBitmap doesn't recompute them
	// per point (cleared by markerScope when this pass ends)
	m_passTmWhite = tmWhite;
	m_pPassRef = & bRef;

	if (pTooltip)
	{
		m_ttID = 0;
		stRGB.clear();
		eRGB.clear();
		pTooltip->RemoveAllTools();
	}

	dE10 = ((CMainView*)pView)->dE10min;
	
//	if (current_mode != 11 && !pDoc->GetMeasure()->m_binMeasure && !onEdit && pDoc->GetMeasure()->GetCC24Sat(0).isValid())
//	{
//		((CMainView*)pView)->m_displayMode = 11;
//		((CMainView*)pView)->UpdateAllGrids();
//		dE10 = ((CMainView*)pView)->dE10min;
//		((CMainView*)pView)->m_displayMode = current_mode;
//		((CMainView*)pView)->UpdateAllGrids();
//	}
                   
				char*  PatName[96]={
                    "White",
                    "6J",
                    "5F",
                    "6I",
                    "6K",
                    "5G",
                    "6H",
                    "5H",
                    "7K",
                    "6G",
                    "5I",
                    "6F",
                    "8K",
                    "5J",
                    "Black",
                    "2B",
                    "2C",
                    "2D",
                    "2E",
                    "2F",
                    "2G",
                    "2H",
                    "2I",
                    "2J",
                    "2K",
                    "2L",
                    "2M",
                    "3B",
                    "3C",
                    "3D",
                    "3E",
                    "3F",
                    "3G",
                    "3H",
                    "3I",
                    "3J",
                    "3K",
                    "3L",
                    "3M",
                    "4B",
                    "4C",
                    "4D",
                    "4E",
                    "4F",
                    "4G",
                    "4H",
                    "4I",
                    "4J",
                    "4K",
                    "4L",
                    "4M",
                    "5B",
                    "5C",
                    "5D",
                    "5K",
                    "5L",
                    "5M",
                    "6B",
                    "6C",
                    "6D",
                    "6L",
                    "6M",
                    "7B",
                    "7C",
                    "7D",
                    "7E",
                    "7F",
                    "7G",
                    "7H",
                    "7I",
                    "7J",
                    "7L",
                    "7M",
                    "8B",
                    "8C",
                    "8D",
                    "8E",
                    "8F",
                    "8G",
                    "8H",
                    "8I",
                    "8J",
                    "8L",
                    "8M",
                    "9B",
                    "9C",
                    "9D",
                    "9E",
                    "9F",
                    "9G",
                    "9H",
                    "9I",
                    "9J",
                    "9K",
                    "9L",
                    "9M" };

                    
					char*  PatNameCMS[19]={
						"White",
						"Black",
						"2E",
						"2F",
						"2K",
						"5D",
						"7E",
						"7F",
						"7G",
						"7H",
						"7I",
						"7J",
						"8D",
						"8E",
						"8F",
						"8G",
						"8H",
						"8I",
						"8J" };
                    
						char*  PatNameCPS[19]={
						"White",
						"D7",
						"D8",
						"E7",
						"E8",
						"F7",
						"F8",
						"G7",
						"G8",
						"H7",
						"H8",
						"I7",
						"I8",
						"J7",
						"J8",
						"CP-Light",
						"CP-Dark",
						"Dark Skin",
						"Light Skin" };

						char*  PatNameAXIS[71]={
						"Black",
						"White 10",
						"White 20",
						"White 30",
						"White 40",
						"White 50",
						"White 60",
						"White 70",
						"White 80",
						"White 90",
						"White 100",
						"Red 10",
						"Red 20",
						"Red 30",
						"Red 40",
						"Red 50",
						"Red 60",
						"Red 70",
						"Red 80",
						"Red 90",
						"Red 100",
						"Green 10",
						"Green 20",
						"Green 30",
						"Green 40",
						"Green 50",
						"Green 60",
						"Green 70",
						"Green 80",
						"Green 90",
						"Green 100",
						"Blue 10",
						"Blue 20",
						"Blue 30",
						"Blue 40",
						"Blue 50",
						"Blue 60",
						"Blue 70",
						"Blue 80",
						"Blue 90",
						"Blue 100", 
						"Cyan 10",
						"Cyan 20",
						"Cyan 30",
						"Cyan 40",
						"Cyan 50",
						"Cyan 60",
						"Cyan 70",
						"Cyan 80",
						"Cyan 90",
						"Cyan 100", 
						"Magenta 10",
						"Magenta 20",
						"Magenta 30",
						"Magenta 40",
						"Magenta 50",
						"Magenta 60",
						"Magenta 70",
						"Magenta 80",
						"Magenta 90",
						"Magenta 100", 
						"Yellow 10",
						"Yellow 20",
						"Yellow 30",
						"Yellow 40",
						"Yellow 50",
						"Yellow 60",
						"Yellow 70",
						"Yellow 80",
						"Yellow 90",
						"Yellow 100"
						};

	if ( pDataRef == pDoc || ! m_doShowDataRef )
		pDataRef = NULL;

	// Wait for background thread terminating background bitmaps creation
	if ( WAIT_TIMEOUT == WaitForSingleObject ( pApp -> m_hCIEEvent, 0 ) )
	{
		CWaitCursor wait;
		if ( pApp -> m_hCIEThread )
		{
			// Increase background thread priority
			::SetThreadPriority ( pApp -> m_hCIEThread, THREAD_PRIORITY_NORMAL );
		}
		
		WaitForSingleObject ( pApp -> m_hCIEEvent, INFINITE );
	}

	// Default is SDTV / NTSC
	Msg.LoadString ( IDS_NTSCREDREF );
	Msg2.LoadString ( IDS_NTSCGREENREF );
	Msg3.LoadString ( IDS_NTSCBLUEREF );

	if(GetConfig()->m_colorStandard == PALSECAM)
	{
		Msg.LoadString ( IDS_PALREDREF );
		Msg2.LoadString ( IDS_PALGREENREF );
		Msg3.LoadString ( IDS_PALBLUEREF );
	} else if (GetConfig()->m_colorStandard == CUSTOM)
	{
		Msg.LoadString ( IDS_CUSTREDREF );
		Msg2.LoadString ( IDS_CUSTGREENREF );
		Msg3.LoadString ( IDS_CUSTBLUEREF );
	} else if (GetConfig()->m_colorStandard == UHDTV)
	{
		Msg.LoadString ( IDS_UHDTVREDREF );
		Msg2.LoadString ( IDS_UHDTVGREENREF );
		Msg3.LoadString ( IDS_UHDTVBLUEREF );
	} else if (GetConfig()->m_colorStandard == UHDTV2)
	{
		Msg.LoadString ( IDS_UHDTV2REDREF );
		Msg2.LoadString ( IDS_UHDTV2GREENREF );
		Msg3.LoadString ( IDS_UHDTV2BLUEREF );
	} else if (GetConfig()->m_colorStandard == UHDTV3)
	{
		Msg.LoadString ( IDS_UHDTV3REDREF );
		Msg2.LoadString ( IDS_UHDTV3GREENREF );
		Msg3.LoadString ( IDS_UHDTV3BLUEREF );
	} else if (GetConfig()->m_colorStandard == UHDTV4)
	{
		Msg.LoadString ( IDS_UHDTV4REDREF );
		Msg2.LoadString ( IDS_UHDTV4GREENREF );
		Msg3.LoadString ( IDS_UHDTV4BLUEREF );
	} else if (GetConfig()->m_colorStandard == HDTV || GetConfig()->m_colorStandard == sRGB)
	{
		Msg.LoadString ( IDS_REC709REDREF );
		Msg2.LoadString ( IDS_REC709GREENREF );
		Msg3.LoadString ( IDS_REC709BLUEREF );
	} else if (GetConfig()->m_colorStandard == HDTVa || GetConfig()->m_colorStandard == HDTVb )
	{
		Msg.LoadString ( IDS_REC709aREDREF );
		Msg2.LoadString ( IDS_REC709aGREENREF );
		Msg3.LoadString ( IDS_REC709aBLUEREF );
	} else if (GetConfig()->m_colorStandard == CC6)
	{
		Msg.LoadString ( IDS_RECCC6REDREF );
		Msg2.LoadString ( IDS_RECCC6GREENREF );
		Msg3.LoadString ( IDS_RECCC6BLUEREF );
	}

	ColorXYZ cR=GetColorReference().GetRed();
	ColorXYZ cG=GetColorReference().GetGreen();
	ColorXYZ cB=GetColorReference().GetBlue();
	ColorXYZ cY=GetColorReference().GetYellow();
	ColorXYZ cC=GetColorReference().GetCyan();
	ColorXYZ cM=GetColorReference().GetMagenta();

	CColor aColor[6];
	
	aColor[0].SetXYZValue(cR);
	aColor[1].SetXYZValue(cG);
	aColor[2].SetXYZValue(cB);
	aColor[3].SetXYZValue(cY);
	aColor[4].SetXYZValue(cC);
	aColor[5].SetXYZValue(cM);
	int mode = GetConfig()->m_GammaOffsetType;

	if (mode == 5)
	{
		GetConfig()->m_bHDR100 = TRUE;
		if (GetConfig()->m_colorStandard == UHDTV3 || GetConfig()->m_colorStandard == UHDTV4)
		{
			for (int i=0; i<6; i++)
			{
				aColor[i].SetX(aColor[i].GetX()/105.95640);
				aColor[i].SetY(aColor[i].GetY()/105.95640);
				aColor[i].SetZ(aColor[i].GetZ()/105.95640);
			}
		}
	}
	
	ColorRGB rgb[6];
	for(int i=0;i<6;i++)
		rgb[i]=aColor[i].GetRGBValue ( bRef );
	double r[6],g[6],b[6];
    double gamma=(GetConfig()->m_useMeasuredGamma)?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef);
	CColor White = pDoc->GetMeasure()->GetOnOffWhite();
	CColor Black = pDoc->GetMeasure()->GetOnOffBlack();
    bool isSpecial = (GetColorReference().m_standard==HDTVa||GetColorReference().m_standard==CC6||GetColorReference().m_standard==HDTVb||GetColorReference().m_standard==UHDTV3||GetColorReference().m_standard==UHDTV4);

	if (isSpecial)
	{
		for(int i=0;i<6;i++) //needed only for special subset colorspaces that depend on gamma
		{
			double r1,g1,b1;
			r[i]=rgb[i][0];
			g[i]=rgb[i][1];
			b[i]=rgb[i][2];
			if (GetConfig()->m_colorStandard == sRGB) mode = 99;
			if ( mode >= 4 )
			{
				if (mode == 5 || mode == 7)
				{
					r1=getL_EOTF(r[i], noDataColor, noDataColor, 2.4, 0.9, -1*mode);
					g1=getL_EOTF(g[i], noDataColor, noDataColor, 2.4, 0.9, -1*mode);
					b1=getL_EOTF(b[i], noDataColor, noDataColor, 2.4, 0.9, -1*mode);
					// model the wire quantization on the active grid (bit depth + range)
					bool b10 = !!GetConfig()->GetUse10bitLevels();
					bool lim = !!GetConfig()->GetRGB16_235();
					r1 = SnapToVideoGrid( r1, b10, lim );
					g1 = SnapToVideoGrid( g1, b10, lim );
					b1 = SnapToVideoGrid( b1, b10, lim );
				    r1 = getL_EOTF(r1,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) / (mode==5?100.:1.0);
				    g1 = getL_EOTF(g1,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) / (mode==5?100.:1.0);
				    b1 = getL_EOTF(b1,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) / (mode==5?100.:1.0);
				}
				else
				{
				   r1 = (r[i]<=0.0||r[i]>=1.0)?min(max(r[i],0),1):getL_EOTF(pow(r[i],1.0/2.22),White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
				   g1 = (g[i]<=0.0||g[i]>=1.0)?min(max(g[i],0),1):getL_EOTF(pow(g[i],1.0/2.22),White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
				   b1 = (b[i]<=0.0||b[i]>=1.0)?min(max(b[i],0),1):getL_EOTF(pow(b[i],1.0/2.22),White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
				}
			}
			else
			{
				r1=(r[i]<=0||r[i]>=1)?min(max(r[i],0),1):pow(pow(r[i],1.0/2.22),gamma);
				g1=(g[i]<=0||g[i]>=1)?min(max(g[i],0),1):pow(pow(g[i],1.0/2.22),gamma);
				b1=(b[i]<=0||b[i]>=1)?min(max(b[i],0),1):pow(pow(b[i],1.0/2.22),gamma);
			}

			aColor[i].SetRGBValue (ColorRGB(r1,g1,b1),bRef);	
		}
	}
	
	// Take sum of primary colors Y by default, in case of no white measure found
//	double YWhite = redPrimaryColor[2]+greenPrimaryColor[2]+bluePrimaryColor[2];
	double YWhite = 1.0;
	
	if ( pDoc -> GetMeasure () -> GetPrimeWhite ().isValid() && !( (current_mode>=4 && current_mode<=12) && (GetConfig()->m_colorStandard==HDTVb||GetConfig()->m_colorStandard==HDTVa)) )
		YWhite = pDoc -> GetMeasure () -> GetPrimeWhite () [ 1 ]; //check here first
	else if ( pDoc -> GetMeasure () -> GetOnOffWhite ().isValid() )
		YWhite = pDoc -> GetMeasure () -> GetOnOffWhite () [ 1 ]; //onoff white is always grayscale white
		
	double YWhiteRef = 1.0;
	isSat = FALSE;

	if (GetConfig()->m_GammaOffsetType == 5 && (GetConfig()->m_colorStandard == UHDTV3 || GetConfig()->m_colorStandard == UHDTV4))
		YWhiteRef = 1.0 / 105.9564 * tmWhite / 94.37844;

	if (GetConfig()->m_GammaOffsetType == 5)
		YWhiteRef = YWhiteRef / tmWhite * YWhite;

	CCIEGraphPoint refRedPrimaryPoint(aColor[0].GetXYZValue(), YWhiteRef, Msg, m_bCIEuv, m_bCIEab);
	CCIEGraphPoint refGreenPrimaryPoint(aColor[1].GetXYZValue(), YWhiteRef, Msg2, m_bCIEuv, m_bCIEab);
	CCIEGraphPoint refBluePrimaryPoint(aColor[2].GetXYZValue(), YWhiteRef, Msg3, m_bCIEuv, m_bCIEab);

	CCIEGraphPoint whiteRef(GetColorReference().GetWhite(), YWhiteRef, "", m_bCIEuv, m_bCIEab);

	Msg.LoadString ( (GetColorReference().m_standard!=CC6)?IDS_YELLOWSECONDARYREF:IDS_CC6YELLOWSECONDARYREF );
	CCIEGraphPoint refYellowSecondaryPoint(aColor[3].GetXYZValue(), YWhiteRef, Msg, m_bCIEuv, m_bCIEab);

	Msg.LoadString (  (GetColorReference().m_standard!=CC6)?IDS_CYANSECONDARYREF:IDS_CC6CYANSECONDARYREF );
	CCIEGraphPoint refCyanSecondaryPoint(aColor[4].GetXYZValue(), YWhiteRef, Msg, m_bCIEuv, m_bCIEab);

	Msg.LoadString ( (GetColorReference().m_standard!=CC6)?IDS_MAGENTASECONDARYREF:IDS_CC6MAGENTASECONDARYREF );
	CCIEGraphPoint refMagentaSecondaryPoint(aColor[5].GetXYZValue(), YWhiteRef, Msg, m_bCIEuv, m_bCIEab);

	CString strIll;
	strIll.LoadString ( IDS_CIE_ILLUMINANT );
	CCIEGraphPoint illuminantA(ColorXYZ(ColorxyY(0.4476,0.4074)),1.0, strIll+" A", m_bCIEuv, m_bCIEab);
	CCIEGraphPoint illuminantB(ColorXYZ(ColorxyY(0.3484,0.3516)), 1.0, strIll+" B", m_bCIEuv, m_bCIEab);
	CCIEGraphPoint illuminantC(ColorXYZ(ColorxyY(0.3101,0.3162)), 1.0, strIll+" C", m_bCIEuv, m_bCIEab);
	CCIEGraphPoint illuminantD65(ColorXYZ(ColorxyY(0.3127,0.3291)), 1.0, strIll+" D65", m_bCIEuv, m_bCIEab);

	Msg.LoadString ( IDS_TEMPERATURE );
	CCIEGraphPoint colorTempPoint2700(ColorXYZ(ColorxyY(0.4614,0.4158)), 1.0, Msg+" 2700", m_bCIEuv, m_bCIEab);   
	CCIEGraphPoint colorTempPoint3000(ColorXYZ(ColorxyY(0.4388,0.4095)), 1.0, Msg+" 3000", m_bCIEuv, m_bCIEab);	  
//	CCIEGraphPoint colorTempPoint3500(0.4075,0.3962,100.0,Msg+" 3500", m_bCIEuv, m_bCIEab);   
	CCIEGraphPoint colorTempPoint4000(ColorXYZ(ColorxyY(0.3827,0.3820)), 1.0, Msg+" 4000", m_bCIEuv, m_bCIEab);  
	CCIEGraphPoint colorTempPoint5500(ColorXYZ(ColorxyY(0.3346,0.3451)), 1.0, Msg+" 5500", m_bCIEuv, m_bCIEab);   
	CCIEGraphPoint colorTempPoint9300(ColorXYZ(ColorxyY(0.2866,0.2950)), 1.0, Msg+" 9300", m_bCIEuv, m_bCIEab);   

	ColorXYZ redPrimaryColor=pDoc->GetMeasure()->GetRedPrimary().GetXYZValue();
	ColorXYZ greenPrimaryColor=pDoc->GetMeasure()->GetGreenPrimary().GetXYZValue();
	ColorXYZ bluePrimaryColor=pDoc->GetMeasure()->GetBluePrimary().GetXYZValue();

	BOOL hasPrimaries= redPrimaryColor.isValid() && greenPrimaryColor.isValid() &&
					   bluePrimaryColor.isValid();

	//data
	Msg.LoadString ( IDS_REDPRIMARY );
	CCIEGraphPoint redPrimaryPoint(redPrimaryColor, YWhite, Msg, m_bCIEuv, m_bCIEab);
	Msg.LoadString ( IDS_GREENPRIMARY );
	CCIEGraphPoint greenPrimaryPoint(greenPrimaryColor, YWhite, Msg, m_bCIEuv, m_bCIEab);
	Msg.LoadString ( IDS_BLUEPRIMARY );
	CCIEGraphPoint bluePrimaryPoint(bluePrimaryColor, YWhite, Msg, m_bCIEuv, m_bCIEab);

	ColorXYZ yellowSecondaryColor=pDoc->GetMeasure()->GetYellowSecondary().GetXYZValue();
	ColorXYZ cyanSecondaryColor=pDoc->GetMeasure()->GetCyanSecondary().GetXYZValue();
	ColorXYZ magentaSecondaryColor=pDoc->GetMeasure()->GetMagentaSecondary().GetXYZValue();

	BOOL hasSecondaries= yellowSecondaryColor.isValid() && cyanSecondaryColor.isValid() &&
					   magentaSecondaryColor.isValid();

	Msg.LoadString ( IDS_YELLOWSECONDARY );
	CCIEGraphPoint yellowSecondaryPoint(yellowSecondaryColor, YWhite, Msg, m_bCIEuv, m_bCIEab);
	Msg.LoadString ( IDS_CYANSECONDARY );
	CCIEGraphPoint cyanSecondaryPoint(cyanSecondaryColor, YWhite, Msg, m_bCIEuv, m_bCIEab);
	Msg.LoadString ( IDS_MAGENTASECONDARY );
	CCIEGraphPoint magentaSecondaryPoint(magentaSecondaryColor, YWhite, Msg, m_bCIEuv, m_bCIEab);

	ColorXYZ datarefRed;
	ColorXYZ datarefGreen;
	ColorXYZ datarefBlue;
	ColorXYZ datarefYellow;
	ColorXYZ datarefCyan;
	ColorXYZ datarefMagenta;
	
	BOOL hasdatarefPrimaries = FALSE;
	BOOL hasdatarefSecondaries = FALSE;

	if ( pDataRef )
	{
		datarefRed=pDataRef->GetMeasure()->GetRedPrimary().GetXYZValue();
		datarefGreen=pDataRef->GetMeasure()->GetGreenPrimary().GetXYZValue();
		datarefBlue=pDataRef->GetMeasure()->GetBluePrimary().GetXYZValue();
		datarefYellow=pDataRef->GetMeasure()->GetYellowSecondary().GetXYZValue();
		datarefCyan=pDataRef->GetMeasure()->GetCyanSecondary().GetXYZValue();
		datarefMagenta=pDataRef->GetMeasure()->GetMagentaSecondary().GetXYZValue();

		hasdatarefPrimaries = datarefRed.isValid() && datarefGreen.isValid() && datarefBlue.isValid();
		hasdatarefSecondaries = datarefYellow.isValid() && datarefCyan.isValid() && datarefMagenta.isValid();

//		YWhiteRef = datarefRed[1]+datarefGreen[1]+datarefBlue[1];
		
	if ( pDataRef -> GetMeasure () -> GetPrimeWhite ().isValid() && !( (current_mode>=4 && current_mode<=12) && (GetConfig()->m_colorStandard==HDTVb||GetConfig()->m_colorStandard==HDTVa)) )
			YWhite = pDataRef -> GetMeasure () -> GetPrimeWhite () [ 1 ];
		else if ( pDataRef -> GetMeasure () -> GetOnOffWhite ().isValid() )
			YWhite = pDataRef -> GetMeasure () -> GetOnOffWhite () [ 1 ]; //onoff white is always grayscale white
	}

	Msg.LoadString ( IDS_DATAREF_RED );
	CCIEGraphPoint datarefRedPoint(datarefRed, YWhite, Msg, m_bCIEuv, m_bCIEab);
	Msg.LoadString ( IDS_DATAREF_GREEN );
	CCIEGraphPoint datarefGreenPoint(datarefGreen, YWhite, Msg, m_bCIEuv, m_bCIEab);
	Msg.LoadString ( IDS_DATAREF_BLUE );
	CCIEGraphPoint datarefBluePoint(datarefBlue, YWhite, Msg, m_bCIEuv, m_bCIEab);

	Msg.LoadString ( IDS_DATAREF_YELLOW );
	CCIEGraphPoint datarefYellowPoint(datarefYellow, YWhite, Msg, m_bCIEuv, m_bCIEab);
	Msg.LoadString ( IDS_DATAREF_CYAN );
	CCIEGraphPoint datarefCyanPoint(datarefCyan, YWhite, Msg, m_bCIEuv, m_bCIEab);
	Msg.LoadString ( IDS_DATAREF_MAGENTA );
	CCIEGraphPoint datarefMagentaPoint(datarefMagenta, YWhite, Msg, m_bCIEuv, m_bCIEab);

	// Draw background bitmap
	CDC dcBg;
	dcBg.CreateCompatibleDC(pDC);
	CBitmap *pOldBitmap=dcBg.SelectObject(&m_bgBitmap);
	pDC->BitBlt(0,0,rect.Width(),rect.Height(),&dcBg,0,0,SRCCOPY);
	dcBg.SelectObject(pOldBitmap);

	// Fill triangle with lighten chart
	if(m_doDisplayBackground && hasPrimaries)
	{
		CBrush gamutBrush;
		CPoint ptVertex[6];
		gamutBrush.CreatePatternBrush(&m_gamutBitmap);
		CBrush *pOldBrush=pDC->SelectObject(&gamutBrush);
		if (hasSecondaries)
		{
			ptVertex[0] = redPrimaryPoint.GetGraphPoint(rect);
			ptVertex[1] = yellowSecondaryPoint.GetGraphPoint(rect);
			ptVertex[2] = greenPrimaryPoint.GetGraphPoint(rect);
			ptVertex[3] = cyanSecondaryPoint.GetGraphPoint(rect);
			ptVertex[4] = bluePrimaryPoint.GetGraphPoint(rect);
			ptVertex[5] = magentaSecondaryPoint.GetGraphPoint(rect);
		}
		else
		{
			ptVertex[0] = redPrimaryPoint.GetGraphPoint(rect);
			ptVertex[1] = redPrimaryPoint.GetGraphPoint(rect);
			ptVertex[2] = greenPrimaryPoint.GetGraphPoint(rect);
			ptVertex[3] = greenPrimaryPoint.GetGraphPoint(rect);
			ptVertex[4] = bluePrimaryPoint.GetGraphPoint(rect);
			ptVertex[5] = bluePrimaryPoint.GetGraphPoint(rect);
		}

		CRgn triangleRgn;
		triangleRgn.CreatePolygonRgn(ptVertex,6,WINDING);
		pDC->PaintRgn(&triangleRgn);
		pDC->SelectObject(pOldBrush);
	}

	int penWidth=(min(rect.Width(),rect.Height()) > FX_MINSIZETOSHOW_TRIANGLEDETAILS) ? 3: 2;

 	// Reference gamut: no outline -- collect the gamut polygon (with the
 	// curved a*b* edges) and dim everything OUTSIDE it instead. The shading
 	// itself marks the target boundary.
	pDC->SetBkMode(TRANSPARENT);
	{
	struct CGamutPolygon
	{
		std::vector<Gdiplus::PointF> pts;
		void MoveTo(CPoint p) { pts.clear(); pts.push_back(Gdiplus::PointF((float)p.x,(float)p.y)); }
		void LineTo(CPoint p) { pts.push_back(Gdiplus::PointF((float)p.x,(float)p.y)); }
	} aaLine;

	aaLine.MoveTo(refRedPrimaryPoint.GetGraphPoint(rect));

	if (m_bCIEab)
	{
		//for ab space curvature
		double x1=refRedPrimaryPoint.x;
		double y1=refRedPrimaryPoint.y;
		double Y1=refRedPrimaryPoint.GetNormalizedColor()[1];
		double x2=refYellowSecondaryPoint.x;
		double y2=refYellowSecondaryPoint.y;
		double Y2=refYellowSecondaryPoint.GetNormalizedColor()[1];
		double dx=(x2-x1) / 10.;
		double dy=(y2-y1) / 10.;
		double dY=(Y2-Y1) / 10;
		for (int i=0;i<10;i++)
		{
			ColorxyY ixyY(x1+dx,y1+dy,Y1+dY);
			ColorXYZ iXYZ(ixyY);
			CCIEGraphPoint iGP(iXYZ, 1, "iteration",FALSE, TRUE);
			aaLine.LineTo(iGP.GetGraphPoint(rect));
		}
	}

	aaLine.LineTo(refYellowSecondaryPoint.GetGraphPoint(rect));
	if (m_bCIEab)
	{
		//for ab space curvature
		double x1=refYellowSecondaryPoint.x;
		double y1=refYellowSecondaryPoint.y;
		double Y1=refYellowSecondaryPoint.GetNormalizedColor()[1];
		double x2=refGreenPrimaryPoint.x;
		double y2=refGreenPrimaryPoint.y;
		double Y2=refGreenPrimaryPoint.GetNormalizedColor()[1];
		double dx=(x2-x1) / 10.;
		double dy=(y2-y1) / 10.;
		double dY=(Y2-Y1) / 10;
		for (int i=0;i<10;i++)
		{
			ColorxyY ixyY(x1+dx,y1+dy,Y1+dY);
			ColorXYZ iXYZ(ixyY);
			CCIEGraphPoint iGP(iXYZ, 1, "iteration",FALSE, TRUE);
			aaLine.LineTo(iGP.GetGraphPoint(rect));
		}
	}
	aaLine.LineTo(refGreenPrimaryPoint.GetGraphPoint(rect));
	if (m_bCIEab)
	{
		//for ab space curvature
		double x2=refCyanSecondaryPoint.x;
		double y2=refCyanSecondaryPoint.y;
		double Y2=refCyanSecondaryPoint.GetNormalizedColor()[1];
		double x1=refGreenPrimaryPoint.x;
		double y1=refGreenPrimaryPoint.y;
		double Y1=refGreenPrimaryPoint.GetNormalizedColor()[1];
		double dx=(x2-x1) / 10.;
		double dy=(y2-y1) / 10.;
		double dY=(Y2-Y1) / 10;
		for (int i=0;i<10;i++)
		{
			ColorxyY ixyY(x1+dx,y1+dy,Y1+dY);
			ColorXYZ iXYZ(ixyY);
			CCIEGraphPoint iGP(iXYZ, 1, "iteration",FALSE, TRUE);
			aaLine.LineTo(iGP.GetGraphPoint(rect));
		}
	}
	aaLine.LineTo(refCyanSecondaryPoint.GetGraphPoint(rect));
	if (m_bCIEab)
	{
		//for ab space curvature
		double x1=refCyanSecondaryPoint.x;
		double y1=refCyanSecondaryPoint.y;
		double Y1=refCyanSecondaryPoint.GetNormalizedColor()[1];
		double x2=refBluePrimaryPoint.x;
		double y2=refBluePrimaryPoint.y;
		double Y2=refBluePrimaryPoint.GetNormalizedColor()[1];
		double dx=(x2-x1) / 10.;
		double dy=(y2-y1) / 10.;
		double dY=(Y2-Y1) / 10 ;
		for (int i=0;i<10;i++)
		{
			ColorxyY ixyY(x1+dx,y1+dy,Y1+dY);
			ColorXYZ iXYZ(ixyY);
			CCIEGraphPoint iGP(iXYZ, 1, "iteration",FALSE, TRUE);
			aaLine.LineTo(iGP.GetGraphPoint(rect));
		}
	}
	aaLine.LineTo(refBluePrimaryPoint.GetGraphPoint(rect));
	if (m_bCIEab)
	{
		//for ab space curvature
		double x2=refMagentaSecondaryPoint.x;
		double y2=refMagentaSecondaryPoint.y;
		double Y2=refMagentaSecondaryPoint.GetNormalizedColor()[1];
		double x1=refBluePrimaryPoint.x;
		double y1=refBluePrimaryPoint.y;
		double Y1=refBluePrimaryPoint.GetNormalizedColor()[1];
		double dx=(x2-x1) / 10.;
		double dy=(y2-y1) / 10.;
		double dY=(Y2-Y1) / 10;
		for (int i=0;i<10;i++)
		{
			ColorxyY ixyY(x1+dx,y1+dy,Y1+dY);
			ColorXYZ iXYZ(ixyY);
			CCIEGraphPoint iGP(iXYZ, 1, "iteration",FALSE, TRUE);
			aaLine.LineTo(iGP.GetGraphPoint(rect));
		}
	}
	aaLine.LineTo(refMagentaSecondaryPoint.GetGraphPoint(rect));
	if (m_bCIEab)
	{
		//for ab space curvature
		double x1=refMagentaSecondaryPoint.x;
		double y1=refMagentaSecondaryPoint.y;
		double Y1=refMagentaSecondaryPoint.GetNormalizedColor()[1];
		double x2=refRedPrimaryPoint.x;
		double y2=refRedPrimaryPoint.y;
		double Y2=refRedPrimaryPoint.GetNormalizedColor()[1];
		double dx=(x2-x1) / 10.;
		double dy=(y2-y1) / 10.;
		double dY=(Y2-Y1) / 10.;
		for (int i=0;i<10;i++)
		{
			ColorxyY ixyY(x1+dx,y1+dy,Y1+dY);
			ColorXYZ iXYZ(ixyY);
			CCIEGraphPoint iGP(iXYZ, 1, "iteration",FALSE, TRUE);
			aaLine.LineTo(iGP.GetGraphPoint(rect));
		}
	}
	aaLine.LineTo(refRedPrimaryPoint.GetGraphPoint(rect));

	if ( aaLine.pts.size() >= 3 && m_pMarkerGraphics )
	{
		// Even-odd fill between the chart rectangle and the gamut polygon:
		// dims only the out-of-gamut area, with an anti-aliased boundary
		Gdiplus::GraphicsPath path(Gdiplus::FillModeAlternate);
		path.AddRectangle(Gdiplus::Rect(0,0,rect.Width(),rect.Height()));
		path.AddPolygon(&aaLine.pts[0],(INT)aaLine.pts.size());
		Gdiplus::SolidBrush shade(Gdiplus::Color(100,0,0,0));
		m_pMarkerGraphics->FillPath(&shade,&path);
	}
	}

 	// Draw reference gamut triangle 2020 outside P3

	if ( hasdatarefPrimaries )
	{
		CAAPolyline aaLine(pDC,(float)(penWidth-1),RGB(192,192,192));

		aaLine.MoveTo(datarefRedPoint.GetGraphPoint(rect));
		aaLine.LineTo(datarefGreenPoint.GetGraphPoint(rect));
		aaLine.LineTo(datarefBluePoint.GetGraphPoint(rect));
		aaLine.LineTo(datarefRedPoint.GetGraphPoint(rect));
	}

	// Draw gamut triangle
	if(hasPrimaries)
	{
		CAAPolyline aaLine(pDC,(float)penWidth,RGB(255,255,255));
		if (hasSecondaries)
		{
			aaLine.MoveTo(redPrimaryPoint.GetGraphPoint(rect));
			if (m_bCIEab)
			{
				//for ab space curvature
				double x1=redPrimaryPoint.x;
				double y1=redPrimaryPoint.y;
				double Y1=redPrimaryPoint.GetNormalizedColor()[1];
				double x2=yellowSecondaryPoint.x;
				double y2=yellowSecondaryPoint.y;
				double Y2=yellowSecondaryPoint.GetNormalizedColor()[1];
				double dx=(x2-x1) / 10.;
				double dy=(y2-y1) / 10.;
				double dY=(Y2-Y1) / 10;
				for (int i=0;i<10;i++)
				{
					ColorxyY ixyY(x1+dx,y1+dy,Y1+dY);
					ColorXYZ iXYZ(ixyY);
					CCIEGraphPoint iGP(iXYZ, 1, "iteration",FALSE, TRUE);
					aaLine.LineTo(iGP.GetGraphPoint(rect));
				}
			}
			aaLine.LineTo(yellowSecondaryPoint.GetGraphPoint(rect));
			if (m_bCIEab)
			{
				//for ab space curvature
				double x2=greenPrimaryPoint.x;
				double y2=greenPrimaryPoint.y;
				double Y2=greenPrimaryPoint.GetNormalizedColor()[1];
				double x1=yellowSecondaryPoint.x;
				double y1=yellowSecondaryPoint.y;
				double Y1=yellowSecondaryPoint.GetNormalizedColor()[1];
				double dx=(x2-x1) / 10.;
				double dy=(y2-y1) / 10.;
				double dY=(Y2-Y1) / 10;
				for (int i=0;i<10;i++)
				{
					ColorxyY ixyY(x1+dx,y1+dy,Y1+dY);
					ColorXYZ iXYZ(ixyY);
					CCIEGraphPoint iGP(iXYZ, 1, "iteration",FALSE, TRUE);
					aaLine.LineTo(iGP.GetGraphPoint(rect));
				}
			}
			aaLine.LineTo(greenPrimaryPoint.GetGraphPoint(rect));
			if (m_bCIEab)
			{
				//for ab space curvature
				double x1=greenPrimaryPoint.x;
				double y1=greenPrimaryPoint.y;
				double Y1=greenPrimaryPoint.GetNormalizedColor()[1];
				double x2=cyanSecondaryPoint.x;
				double y2=cyanSecondaryPoint.y;
				double Y2=cyanSecondaryPoint.GetNormalizedColor()[1];
				double dx=(x2-x1) / 10.;
				double dy=(y2-y1) / 10.;
				double dY=(Y2-Y1) / 10;
				for (int i=0;i<10;i++)
				{
					ColorxyY ixyY(x1+dx,y1+dy,Y1+dY);
					ColorXYZ iXYZ(ixyY);
					CCIEGraphPoint iGP(iXYZ, 1, "iteration",FALSE, TRUE);
					aaLine.LineTo(iGP.GetGraphPoint(rect));
				}
			}
			aaLine.LineTo(cyanSecondaryPoint.GetGraphPoint(rect));
			if (m_bCIEab)
			{
				//for ab space curvature
				double x2=bluePrimaryPoint.x;
				double y2=bluePrimaryPoint.y;
				double Y2=bluePrimaryPoint.GetNormalizedColor()[1];
				double x1=cyanSecondaryPoint.x;
				double y1=cyanSecondaryPoint.y;
				double Y1=cyanSecondaryPoint.GetNormalizedColor()[1];
				double dx=(x2-x1) / 10.;
				double dy=(y2-y1) / 10.;
				double dY=(Y2-Y1) / 10;
				for (int i=0;i<10;i++)
				{
					ColorxyY ixyY(x1+dx,y1+dy,Y1+dY);
					ColorXYZ iXYZ(ixyY);
					CCIEGraphPoint iGP(iXYZ, 1, "iteration",FALSE, TRUE);
					aaLine.LineTo(iGP.GetGraphPoint(rect));
				}
			}
			aaLine.LineTo(bluePrimaryPoint.GetGraphPoint(rect));
			if (m_bCIEab)
			{
				//for ab space curvature
				double x1=bluePrimaryPoint.x;
				double y1=bluePrimaryPoint.y;
				double Y1=bluePrimaryPoint.GetNormalizedColor()[1];
				double x2=magentaSecondaryPoint.x;
				double y2=magentaSecondaryPoint.y;
				double Y2=magentaSecondaryPoint.GetNormalizedColor()[1];
				double dx=(x2-x1) / 10.;
				double dy=(y2-y1) / 10.;
				double dY=(Y2-Y1) / 10;
				for (int i=0;i<10;i++)
				{
					ColorxyY ixyY(x1+dx,y1+dy,Y1+dY);
					ColorXYZ iXYZ(ixyY);
					CCIEGraphPoint iGP(iXYZ, 1, "iteration",FALSE, TRUE);
					aaLine.LineTo(iGP.GetGraphPoint(rect));
				}
			}
			aaLine.LineTo(magentaSecondaryPoint.GetGraphPoint(rect));
			if (m_bCIEab)
			{
				//for ab space curvature
				double x2=redPrimaryPoint.x;
				double y2=redPrimaryPoint.y;
				double Y2=redPrimaryPoint.GetNormalizedColor()[1];
				double x1=magentaSecondaryPoint.x;
				double y1=magentaSecondaryPoint.y;
				double Y1=magentaSecondaryPoint.GetNormalizedColor()[1];
				double dx=(x2-x1) / 10.;
				double dy=(y2-y1) / 10.;
				double dY=(Y2-Y1) / 10;
				for (int i=0;i<10;i++)
				{
					ColorxyY ixyY(x1+dx,y1+dy,Y1+dY);
					ColorXYZ iXYZ(ixyY);
					CCIEGraphPoint iGP(iXYZ, 1, "iteration",FALSE, TRUE);
					aaLine.LineTo(iGP.GetGraphPoint(rect));
				}
			}
			aaLine.LineTo(redPrimaryPoint.GetGraphPoint(rect));
		}
		else
		{
			aaLine.MoveTo(redPrimaryPoint.GetGraphPoint(rect));
			if (m_bCIEab)
			{
				//for ab space curvature
				double x1=redPrimaryPoint.x;
				double y1=redPrimaryPoint.y;
				double Y1=redPrimaryPoint.GetNormalizedColor()[1];
				double x2=greenPrimaryPoint.x;
				double y2=greenPrimaryPoint.y;
				double Y2=greenPrimaryPoint.GetNormalizedColor()[1];
				double dx=(x2-x1) / 10.;
				double dy=(y2-y1) / 10.;
				double dY=(Y2-Y1) / 10;
				for (int i=0;i<10;i++)
				{
					ColorxyY ixyY(x1+dx,y1+dy,Y1+dY);
					ColorXYZ iXYZ(ixyY);
					CCIEGraphPoint iGP(iXYZ, 1, "iteration",FALSE, TRUE);
					aaLine.LineTo(iGP.GetGraphPoint(rect));
				}
			}
			aaLine.LineTo(greenPrimaryPoint.GetGraphPoint(rect));
			if (m_bCIEab)
			{
				//for ab space curvature
				double x1=greenPrimaryPoint.x;
				double y1=greenPrimaryPoint.y;
				double Y1=greenPrimaryPoint.GetNormalizedColor()[1];
				double x2=bluePrimaryPoint.x;
				double y2=bluePrimaryPoint.y;
				double Y2=bluePrimaryPoint.GetNormalizedColor()[1];
				double dx=(x2-x1) / 10.;
				double dy=(y2-y1) / 10.;
				double dY=(Y2-Y1) / 10;
				for (int i=0;i<10;i++)
				{
					ColorxyY ixyY(x1+dx,y1+dy,Y1+dY);
					ColorXYZ iXYZ(ixyY);
					CCIEGraphPoint iGP(iXYZ, 1, "iteration",FALSE, TRUE);
					aaLine.LineTo(iGP.GetGraphPoint(rect));
				}
			}
			aaLine.LineTo(bluePrimaryPoint.GetGraphPoint(rect));
			if (m_bCIEab)
			{
				//for ab space curvature
				double x1=bluePrimaryPoint.x;
				double y1=bluePrimaryPoint.y;
				double Y1=bluePrimaryPoint.GetNormalizedColor()[1];
				double x2=redPrimaryPoint.x;
				double y2=redPrimaryPoint.y;
				double Y2=redPrimaryPoint.GetNormalizedColor()[1];
				double dx=(x2-x1) / 10.;
				double dy=(y2-y1) / 10.;
				double dY=(Y2-Y1) / 10;
				for (int i=0;i<10;i++)
				{
					ColorxyY ixyY(x1+dx,y1+dy,Y1+dY);
					ColorXYZ iXYZ(ixyY);
					CCIEGraphPoint iGP(iXYZ, 1, "iteration",FALSE, TRUE);
					aaLine.LineTo(iGP.GetGraphPoint(rect));
				}
			}
			aaLine.LineTo(redPrimaryPoint.GetGraphPoint(rect));
		}
	}

	// Draw white reference white dashed cross
	if(min(rect.Width(),rect.Height()) > FX_MINSIZETOSHOW_SCALEDETAILS )
	{
		CAAPolyline aaLine(pDC,1.0f,RGB(192,192,192),PS_DOT);

		aaLine.MoveTo(CPoint(5,whiteRef.GetGraphY(rect)));
		aaLine.LineTo(CPoint(rect.right-5,whiteRef.GetGraphY(rect)));
		aaLine.MoveTo(CPoint(whiteRef.GetGraphX(rect),5));
		aaLine.LineTo(CPoint(whiteRef.GetGraphX(rect),rect.bottom-5));
	}

	// Draw bitmaps on triangle vertex
	if(min(rect.Width(),rect.Height()) > FX_MINSIZETOSHOW_TRIANGLEDETAILS)  // Enough room to draw details
	{
		DrawAlphaBitmap(pDC,refRedPrimaryPoint,&m_refRedPrimaryBitmap,rect,pTooltip,pWnd);
		DrawAlphaBitmap(pDC,refGreenPrimaryPoint,&m_refGreenPrimaryBitmap,rect,pTooltip,pWnd);
		DrawAlphaBitmap(pDC,refBluePrimaryPoint,&m_refBluePrimaryBitmap,rect,pTooltip,pWnd);
		
		DrawAlphaBitmap(pDC,refYellowSecondaryPoint,&m_refYellowSecondaryBitmap,rect,pTooltip,pWnd);
		DrawAlphaBitmap(pDC,refCyanSecondaryPoint,&m_refCyanSecondaryBitmap,rect,pTooltip,pWnd);
		DrawAlphaBitmap(pDC,refMagentaSecondaryPoint,&m_refMagentaSecondaryBitmap,rect,pTooltip,pWnd);

		// Reference labels (illuminants, colour temperatures): white over the
		// chart, dark only when the white-background option is active
		pDC->SetTextColor(m_bgWhite ? RGB(64,64,64) : RGB(255,255,255));
		pDC->SetBkMode(TRANSPARENT);

		if(m_doShowReferences)
		{
			BITMAP bm;
			GetColorApp() -> m_chartBitmap.GetBitmap(&bm);
			// Initializes a CFont object with the specified characteristics. 
			CFont font;
			font.CreateFont(16,0,0,0,FW_THIN,FALSE,FALSE,FALSE,0,OUT_TT_ONLY_PRECIS,CLIP_DEFAULT_PRECIS,PROOF_QUALITY,VARIABLE_PITCH | FF_SWISS,_T("Segoe UI"));

			CFont* pOldFont = pDC->SelectObject(&font);

			// Draw ref illuminant points
			pDC->SetTextAlign(TA_TOP|TA_LEFT);
			DrawAlphaBitmap(pDC,illuminantA,&m_illuminantPointBitmap,rect,pTooltip,pWnd);
			pDC->TextOut(illuminantA.GetGraphX(rect)-2,illuminantA.GetGraphY(rect)+4,"A");
			DrawAlphaBitmap(pDC,illuminantB,&m_illuminantPointBitmap,rect,pTooltip,pWnd);
			pDC->TextOut(illuminantB.GetGraphX(rect)+4,illuminantB.GetGraphY(rect)+4,"B");
			DrawAlphaBitmap(pDC,illuminantC,&m_illuminantPointBitmap,rect,pTooltip,pWnd);
			pDC->TextOut(illuminantC.GetGraphX(rect)+4,illuminantC.GetGraphY(rect)+4,"C");
			DrawAlphaBitmap(pDC,illuminantD65,&m_illuminantPointBitmap,rect,pTooltip,pWnd);
			pDC->SetTextAlign(TA_BOTTOM|TA_RIGHT);
			pDC->TextOut(illuminantC.GetGraphX(rect)-4,illuminantC.GetGraphY(rect)-4,"D65");

			// Draw color temp points
			pDC->SetTextAlign(TA_BOTTOM|TA_RIGHT);
			DrawAlphaBitmap(pDC,colorTempPoint9300,&m_colorTempPointBitmap,rect,pTooltip,pWnd);
			pDC->TextOut(colorTempPoint9300.GetGraphX(rect)-2,colorTempPoint9300.GetGraphY(rect)-2,"9300");
			pDC->SetTextAlign(TA_BOTTOM|TA_RIGHT);
			DrawAlphaBitmap(pDC,colorTempPoint4000,&m_colorTempPointBitmap,rect,pTooltip,pWnd);
			pDC->TextOut(colorTempPoint4000.GetGraphX(rect)-2,colorTempPoint4000.GetGraphY(rect)-2,"4000");
			DrawAlphaBitmap(pDC,colorTempPoint5500,&m_colorTempPointBitmap,rect,pTooltip,pWnd);
			pDC->TextOut(colorTempPoint5500.GetGraphX(rect)-2,colorTempPoint5500.GetGraphY(rect)-2,"5500");
			DrawAlphaBitmap(pDC,colorTempPoint3000,&m_colorTempPointBitmap,rect,pTooltip,pWnd);
			pDC->SetTextAlign(TA_BOTTOM|TA_RIGHT);
			pDC->TextOut(colorTempPoint3000.GetGraphX(rect)+2,colorTempPoint3000.GetGraphY(rect)-2,"3000");
			DrawAlphaBitmap(pDC,colorTempPoint2700,&m_colorTempPointBitmap,rect,pTooltip,pWnd);
			pDC->SetTextAlign(TA_BOTTOM|TA_LEFT);
			pDC->TextOut(colorTempPoint2700.GetGraphX(rect)-2,colorTempPoint2700.GetGraphY(rect)-2,"2700");

			pDC->SelectObject(pOldFont);
		}

		// Gamut coverage chips (top right): [gamut] [xy: n%] [u'v': n%]. The
		// gamut-name chip always shows; the coverage percentages are the measured
		// primaries triangle vs the displayed reference triangle (area
		// intersection / reference area) in xy and u'v'. Not shown in CIE a*b*
		// mode (the metric is a chromaticity-plane one). The percentages hide
		// when the gamut is changed until the primaries are re-measured, since
		// old primaries don't belong to the new reference.
		// Coverage chips are baked into exports here; screen paints overlay
		// them in OnDraw after the blit instead, so they stay pinned to the
		// window's top-right corner while the chart is zoomed or panned.
		if ( ! pWnd )
			DrawCoverageChips ( pDC, rect, pDoc );

		if(hasPrimaries && hasSecondaries && !m_bCIEab)
		{
			// Draw lines between primaries and secondaries
			CAAPolyline aaLine(pDC,1.0f,RGB(64,64,64),PS_DOT);

			aaLine.MoveTo(redPrimaryPoint.GetGraphPoint(rect));
			aaLine.LineTo(cyanSecondaryPoint.GetGraphPoint(rect));

			aaLine.MoveTo(greenPrimaryPoint.GetGraphPoint(rect));
			aaLine.LineTo(magentaSecondaryPoint.GetGraphPoint(rect));

			aaLine.MoveTo(bluePrimaryPoint.GetGraphPoint(rect));
			aaLine.LineTo(yellowSecondaryPoint.GetGraphPoint(rect));
		}

		if ( hasPrimaries )
		{
			DrawAlphaBitmap(pDC,redPrimaryPoint,&m_redPrimaryBitmap,rect,pTooltip,pWnd,&refRedPrimaryPoint, FALSE, dE10, TRUE);
			DrawAlphaBitmap(pDC,greenPrimaryPoint,&m_greenPrimaryBitmap,rect,pTooltip,pWnd,&refGreenPrimaryPoint, FALSE, dE10, TRUE);
			DrawAlphaBitmap(pDC,bluePrimaryPoint,&m_bluePrimaryBitmap,rect,pTooltip,pWnd,&refBluePrimaryPoint, FALSE, dE10, TRUE);
		}
		
		if ( hasSecondaries )
		{
			DrawAlphaBitmap(pDC,yellowSecondaryPoint,&m_yellowSecondaryBitmap,rect,pTooltip,pWnd,&refYellowSecondaryPoint, FALSE, dE10, TRUE);
			DrawAlphaBitmap(pDC,cyanSecondaryPoint,&m_cyanSecondaryBitmap,rect,pTooltip,pWnd,&refCyanSecondaryPoint, FALSE, dE10,  TRUE);
			DrawAlphaBitmap(pDC,magentaSecondaryPoint,&m_magentaSecondaryBitmap,rect,pTooltip,pWnd,&refMagentaSecondaryPoint, FALSE, dE10, TRUE);
		}

		if ( hasdatarefPrimaries )
		{
			DrawAlphaBitmap(pDC,datarefRedPoint,&m_datarefRedBitmap,rect,pTooltip,pWnd,&refRedPrimaryPoint, FALSE, dE10);
			DrawAlphaBitmap(pDC,datarefGreenPoint,&m_datarefGreenBitmap,rect,pTooltip,pWnd,&refGreenPrimaryPoint, FALSE, dE10);
			DrawAlphaBitmap(pDC,datarefBluePoint,&m_datarefBlueBitmap,rect,pTooltip,pWnd,&refBluePrimaryPoint, FALSE, dE10);
		}

		if ( hasdatarefSecondaries )
		{
			DrawAlphaBitmap(pDC,datarefYellowPoint,&m_datarefYellowBitmap,rect,pTooltip,pWnd,&refYellowSecondaryPoint, FALSE, dE10);
			DrawAlphaBitmap(pDC,datarefCyanPoint,&m_datarefCyanBitmap,rect,pTooltip,pWnd,&refCyanSecondaryPoint, FALSE, dE10);
			DrawAlphaBitmap(pDC,datarefMagentaPoint,&m_datarefMagentaBitmap,rect,pTooltip,pWnd,&refMagentaSecondaryPoint, FALSE, dE10);
		}
	}

	if(m_doShowGrayScale)
	{
		BOOL bIRE = pDoc->GetMeasure()->m_bIREScaleMode;
		int nSize = pDoc->GetMeasure()->GetGrayScaleSize();
		
		double YWhiteGray = YWhite;
		if ( nSize > 0 )
			YWhiteGray = pDoc -> GetMeasure () -> GetGray ( nSize - 1 ) [ 1 ];
		
		
		CColor GrayClr;
		
		double Gamma, Offset;
		pDoc->ComputeGammaAndOffset(&Gamma, &Offset, 3, 1, nSize, false);

		for(int i=0;i<nSize;i++)
		{
			CString str;
			Msg.LoadString ( IDS_GRAYIRE );
			str.Format(Msg,(int)(pDoc->GetMeasure()->GetGrayPercent(i, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235())+0.5));
            ColorXYZ aColor(pDoc->GetMeasure()->GetGray(i).GetXYZValue());
            ColorXYZ refColor(GetColorReference().GetWhite());
            double valy;

            // Determine Reference Y luminance for Delta E calculus
            if ( GetConfig ()->m_dE_gray > 0 || GetConfig ()->m_dE_form == 5 )
            {
				double x = pDoc->GetMeasure()->GetGrayPercent ( i, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235() );
    			CColor White = pDoc -> GetMeasure () -> GetGray ( nSize - 1 );
				CColor Black = pDoc->GetMeasure()->GetOnOffBlack();
				if (GetConfig()->m_colorStandard == sRGB) mode = 99;
				if (  (mode >= 4) )
			    {
				   double valx = GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());
                   valy = getL_EOTF(valx, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
//				   valy = min(valy, GetConfig()->m_TargetMaxL);
			    }
			    else
			    {
				   double valx=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235())+Offset)/(1.0+Offset);
				   valy=pow(valx, GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef));
					if (mode == 1) //black compensation target
						valy = (Black.GetY() + ( valy * ( YWhite - Black.GetY() ) )) / YWhite;
			    }

                ColorxyY tmpColor(GetColorReference().GetWhite());
                tmpColor[2] = valy;
				if (GetConfig()->m_GammaOffsetType == 5)
	                    tmpColor[2] = valy * 100. / YWhite;

				if ( GetConfig ()->m_dE_gray == 2 || GetConfig ()->m_dE_form == 5 )
	                    tmpColor[2] = aColor [ 1 ] / YWhite;

                refColor = ColorXYZ(tmpColor);
            }
            else
            {
                // Use actual gray luminance as correct reference (absolute) 
                    YWhiteGray = aColor [ 1 ];
            }

            CCIEGraphPoint grayRef(refColor, 1.0,"", m_bCIEuv, m_bCIEab);

            CCIEGraphPoint grayPoint(pDoc->GetMeasure()->GetGray(i).GetXYZValue(), YWhiteGray, str, m_bCIEuv, m_bCIEab);

            DrawAlphaBitmap(pDC,grayPoint,&m_grayPlotBitmap,rect,pTooltip,pWnd,&grayRef, FALSE, dE10);
		}
	}
	
	if ( pDoc -> GetMeasure () -> GetPrimeWhite ().isValid() && !( (current_mode>=4 && current_mode<=12) && (GetConfig()->m_colorStandard==HDTVb||GetConfig()->m_colorStandard==HDTVa)) )
		YWhite = pDoc -> GetMeasure () -> GetPrimeWhite () [ 1 ]; //check here first
	else if ( pDoc -> GetMeasure () -> GetOnOffWhite ().isValid() )
		YWhite = pDoc -> GetMeasure () -> GetOnOffWhite () [ 1 ]; //onoff white is always grayscale white

	YWhiteRef = 1.0;

	if (GetConfig()->m_GammaOffsetType == 5)	
		YWhiteRef = 1. / 105.9564 * tmWhite / 94.37844;

	if(m_doShowSaturationScaleTarg) 
	{
		CString str;
		for(int i=0;i<pDoc->GetMeasure()->GetSaturationSize();i++)
		{

			if (i != 0)
			{
			Msg.LoadString ( IDS_REDSATPERCENTREF );
			str.Format(Msg, (i*100/(pDoc->GetMeasure()->GetSaturationSize()-1)));
			CCIEGraphPoint RedPointRef(pDoc->GetMeasure()->GetRefSat(0, (double)i / (double)(pDoc->GetMeasure()->GetSaturationSize()-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb)).GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,RedPointRef,&m_redSatRefBitmap,rect,pTooltip,pWnd);
			Msg.LoadString ( IDS_GREENSATPERCENTREF );
			str.Format(Msg, (i*100/(pDoc->GetMeasure()->GetSaturationSize()-1)));
			CCIEGraphPoint GreenPointRef(pDoc->GetMeasure()->GetRefSat(1, (double)i / (double)(pDoc->GetMeasure()->GetSaturationSize()-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb)).GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,GreenPointRef,&m_greenSatRefBitmap,rect,pTooltip,pWnd);
						Msg.LoadString ( IDS_BLUESATPERCENTREF );
			str.Format(Msg, (i*100/(pDoc->GetMeasure()->GetSaturationSize()-1)));
			CCIEGraphPoint BluePointRef(pDoc->GetMeasure()->GetRefSat(2, (double)i / (double)(pDoc->GetMeasure()->GetSaturationSize()-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb)).GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,BluePointRef,&m_blueSatRefBitmap,rect,pTooltip,pWnd);
			Msg.LoadString ( IDS_YELLOWSATPERCENTREF );
			str.Format(Msg, (i*100/(pDoc->GetMeasure()->GetSaturationSize()-1)));
			CCIEGraphPoint YellowPointRef(pDoc->GetMeasure()->GetRefSat(3, (double)i / (double)(pDoc->GetMeasure()->GetSaturationSize()-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb)).GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,YellowPointRef,&m_yellowSatRefBitmap,rect,pTooltip,pWnd);
			Msg.LoadString ( IDS_CYANSATPERCENTREF );
			str.Format(Msg, (i*100/(pDoc->GetMeasure()->GetSaturationSize()-1)));
			CCIEGraphPoint CyanPointRef(pDoc->GetMeasure()->GetRefSat(4, (double)i / (double)(pDoc->GetMeasure()->GetSaturationSize()-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb)).GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,CyanPointRef,&m_cyanSatRefBitmap,rect,pTooltip,pWnd);
			Msg.LoadString ( IDS_MAGENTASATPERCENTREF );
			str.Format(Msg, (i*100/(pDoc->GetMeasure()->GetSaturationSize()-1)));
			CCIEGraphPoint MagentaPointRef(pDoc->GetMeasure()->GetRefSat(5, (double)i / (double)(pDoc->GetMeasure()->GetSaturationSize()-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb)).GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,MagentaPointRef,&m_magentaSatRefBitmap,rect,pTooltip,pWnd);
			}

		}
	}
	if(m_doShowCCScaleTarg) 
	{
		CColor ccolor;
		CString str;
		BOOL isExtPat =( GetConfig()->m_CCMode == USER || GetConfig()->m_CCMode == CM10SAT || GetConfig()->m_CCMode == CM10SAT75 || GetConfig()->m_CCMode == CM5SAT || GetConfig()->m_CCMode == CM5SAT75 || GetConfig()->m_CCMode == CM4SAT || GetConfig()->m_CCMode == CM4SAT75 || GetConfig()->m_CCMode == CM4LUM || GetConfig()->m_CCMode == CM5LUM || GetConfig()->m_CCMode == CM10LUM || GetConfig()->m_CCMode == RANDOM250 || GetConfig()->m_CCMode == RANDOM500 || GetConfig()->m_CCMode == CM6NB || GetConfig()->m_CCMode==MASCIOR50 || GetConfig()->m_CCMode == CMDNR);
		isExtPat = (isExtPat || GetConfig()->m_CCMode > 19);
        if (GetConfig()->m_CCMode != CCSG && !isExtPat && GetConfig()->m_CCMode != CMS && GetConfig()->m_CCMode != CPS && GetConfig()->m_CCMode != AXIS)
        {
			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_1a:(GetConfig()->m_CCMode == SKIN?IDS_CC_1b:IDS_CC_1) );
			str.Format(Msg, 10);
			pDoc->GetMeasure()->GetRefCC24Sat(0, ccolor);
			CCIEGraphPoint cc1Point(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,cc1Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);
		
			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_2a:(GetConfig()->m_CCMode == SKIN?IDS_CC_2b:IDS_CC_2) );
			str.Format(Msg, 10);
			pDoc->GetMeasure()->GetRefCC24Sat(1, ccolor);
			CCIEGraphPoint cc2Point(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,cc2Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_3a:(GetConfig()->m_CCMode == SKIN?IDS_CC_3b:IDS_CC_3) );
			str.Format(Msg, 10);
			pDoc->GetMeasure()->GetRefCC24Sat(2, ccolor);
			CCIEGraphPoint cc3Point(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,cc3Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_4a:(GetConfig()->m_CCMode == SKIN?IDS_CC_4b:IDS_CC_4) );
			str.Format(Msg, 10);
			pDoc->GetMeasure()->GetRefCC24Sat(3, ccolor);
			CCIEGraphPoint cc4Point(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,cc4Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_5a:(GetConfig()->m_CCMode == SKIN?IDS_CC_5b:IDS_CC_5) );
			str.Format(Msg, 10);
			pDoc->GetMeasure()->GetRefCC24Sat(4, ccolor);
			CCIEGraphPoint cc5Point(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,cc5Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_6a:(GetConfig()->m_CCMode == SKIN?IDS_CC_6b:IDS_CC_6) );
			str.Format(Msg, 10);
			pDoc->GetMeasure()->GetRefCC24Sat(5, ccolor);
			CCIEGraphPoint cc6Point(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,cc6Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_7a:(GetConfig()->m_CCMode == SKIN?IDS_CC_7b:IDS_CC_7) );
			str.Format(Msg, 10);
			pDoc->GetMeasure()->GetRefCC24Sat(6, ccolor);
			CCIEGraphPoint cc7Point(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,cc7Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_8a:(GetConfig()->m_CCMode == SKIN?IDS_CC_8b:IDS_CC_8) );
			str.Format(Msg, 10);
			pDoc->GetMeasure()->GetRefCC24Sat(7, ccolor);
			CCIEGraphPoint cc8Point(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,cc8Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_9a:(GetConfig()->m_CCMode == SKIN?IDS_CC_9b:IDS_CC_9) );
			str.Format(Msg, 10);
			pDoc->GetMeasure()->GetRefCC24Sat(8, ccolor);
			CCIEGraphPoint cc9Point(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,cc9Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_10a:(GetConfig()->m_CCMode == SKIN?IDS_CC_10b:IDS_CC_10) );
			str.Format(Msg, 10);
			pDoc->GetMeasure()->GetRefCC24Sat(9, ccolor);
			CCIEGraphPoint cc10Point(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,cc10Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_11a:(GetConfig()->m_CCMode == SKIN?IDS_CC_11b:IDS_CC_11) );
			str.Format(Msg, 10);
			pDoc->GetMeasure()->GetRefCC24Sat(10, ccolor);
			CCIEGraphPoint cc11Point(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,cc11Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_12a:(GetConfig()->m_CCMode == SKIN?IDS_CC_12b:IDS_CC_12) );
			str.Format(Msg, 10);
			pDoc->GetMeasure()->GetRefCC24Sat(11, ccolor);
			CCIEGraphPoint cc12Point(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,cc12Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_13a:(GetConfig()->m_CCMode == SKIN?IDS_CC_13b:IDS_CC_13) );
			str.Format(Msg, 10);
			pDoc->GetMeasure()->GetRefCC24Sat(12, ccolor);
			CCIEGraphPoint cc13Point(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,cc13Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_14a:(GetConfig()->m_CCMode == SKIN?IDS_CC_14b:IDS_CC_14) );
			str.Format(Msg, 10);
			pDoc->GetMeasure()->GetRefCC24Sat(13, ccolor);
			CCIEGraphPoint cc14Point(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,cc14Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_15a:(GetConfig()->m_CCMode == SKIN?IDS_CC_15b:IDS_CC_15) );
			str.Format(Msg, 10);
			pDoc->GetMeasure()->GetRefCC24Sat(14, ccolor);
			CCIEGraphPoint cc15Point(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,cc15Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_16a:(GetConfig()->m_CCMode == SKIN?IDS_CC_16b:IDS_CC_16) );
			str.Format(Msg, 10);
			pDoc->GetMeasure()->GetRefCC24Sat(15, ccolor);
			CCIEGraphPoint cc16Point(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,cc16Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_17a:(GetConfig()->m_CCMode == SKIN?IDS_CC_17b:IDS_CC_17) );
			str.Format(Msg, 10);
			pDoc->GetMeasure()->GetRefCC24Sat(16, ccolor);
			CCIEGraphPoint cc17Point(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,cc17Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_18a:(GetConfig()->m_CCMode == SKIN?IDS_CC_18b:IDS_CC_18) );
			str.Format(Msg, 10);
			pDoc->GetMeasure()->GetRefCC24Sat(17, ccolor);
			CCIEGraphPoint cc18Point(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,cc18Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_19a:(GetConfig()->m_CCMode == SKIN?IDS_CC_19b:IDS_CC_19) );
			str.Format(Msg, 10);
			pDoc->GetMeasure()->GetRefCC24Sat(18, ccolor);
			CCIEGraphPoint cc19Point(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,cc19Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_20a:(GetConfig()->m_CCMode == SKIN?IDS_CC_20b:IDS_CC_20) );
			str.Format(Msg, 10);
			pDoc->GetMeasure()->GetRefCC24Sat(19, ccolor);
			CCIEGraphPoint cc20Point(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,cc20Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_21a:(GetConfig()->m_CCMode == SKIN?IDS_CC_21b:IDS_CC_21) );
			str.Format(Msg, 10);
			pDoc->GetMeasure()->GetRefCC24Sat(20, ccolor);
			CCIEGraphPoint cc21Point(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,cc21Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_22a:(GetConfig()->m_CCMode == SKIN?IDS_CC_22b:IDS_CC_22) );
			str.Format(Msg, 10);			
			pDoc->GetMeasure()->GetRefCC24Sat(21, ccolor);
			CCIEGraphPoint cc22Point(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,cc22Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);
			
			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_23a:(GetConfig()->m_CCMode == SKIN?IDS_CC_23b:IDS_CC_23) );
			str.Format(Msg, 10);
			pDoc->GetMeasure()->GetRefCC24Sat(22, ccolor);
			CCIEGraphPoint cc23Point(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab);
			DrawAlphaBitmap(pDC,cc23Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_24a:(GetConfig()->m_CCMode == SKIN?IDS_CC_24b:IDS_CC_24) );
			str.Format(Msg, 10);
			pDoc->GetMeasure()->GetRefCC24Sat(23, ccolor);
			CCIEGraphPoint cc24Point(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc24Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);
            }
            else
            {
				CCPatterns ccPat = GetConfig()->m_CCMode;
                if (ccPat == CCSG || ccPat == CMS || ccPat == CPS || ccPat == AXIS )
                {
                    for (int i=0; i < (ccPat==CCSG?96:(ccPat==AXIS?71:19)); i++)
                    {
            			Msg.SetString ( ccPat==CCSG?PatName[i]:(ccPat==CMS?PatNameCMS[i]:(ccPat==CPS?PatNameCPS[i]:PatNameAXIS[i])) );
	            		str.Format(Msg, 10);
		            	pDoc->GetMeasure()->GetRefCC24Sat(i, ccolor);
		            	CCIEGraphPoint cc24Point(ccolor.GetXYZValue(),
	        					  YWhiteRef,
		        				  str, m_bCIEuv, m_bCIEab );
			            DrawAlphaBitmap(pDC,cc24Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);
                    }
                }
                else
                {
                    for (int i=0; i < GetConfig()->GetCColorsSize(); i++)
                    {
						std::string name = GetConfig()->GetCColorsN(i);
						// Patch name is user data and may contain '%' (e.g. "Gray 50%"). Passing it as a Format
						// ARGUMENT keeps it inert - only the resource string is read as a template - and CString
						// sizes itself, so there is no fixed-buffer overflow on a long name either.
						CString ccFmt; ccFmt.LoadString ( IDS_CC_COLORNAMEREF );
						str.Format( (LPCSTR)ccFmt, name.c_str() );
		        	    pDoc->GetMeasure()->GetRefCC24Sat(i, ccolor);
		        	    CCIEGraphPoint cc24Point(ccolor.GetXYZValue(),
			        					  YWhiteRef,
				        				  str, m_bCIEuv, m_bCIEab );
			            DrawAlphaBitmap(pDC,cc24Point,&m_cc24SatRefBitmap,rect,pTooltip,pWnd);
                    }
                }

            }
	}


	if (GetConfig()->m_GammaOffsetType == 5 && !(GetConfig()->m_colorStandard == UHDTV3 || GetConfig()->m_colorStandard == UHDTV4))
	{
		YWhiteRef = 1. / 105.9564 * tmWhite / 94.37844;
		YWhite = YWhite;
	}

	if (GetConfig()->m_GammaOffsetType == 5)
		YWhiteRef = YWhiteRef / tmWhite * YWhite;

	isSat = TRUE;
	if(m_doShowSaturationScale) 
	{
		CString str,cc24str;
		for(int i=0;i<pDoc->GetMeasure()->GetSaturationSize();i++)
		{

			Msg.LoadString ( IDS_REDSATPERCENT );
			str.Format(Msg, (i*100/(pDoc->GetMeasure()->GetSaturationSize()-1)));
			CCIEGraphPoint RedPoint(pDoc->GetMeasure()->GetRedSat(i).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			CCIEGraphPoint RedPointRef(pDoc->GetMeasure()->GetRefSat(0, (double)i / (double)(pDoc->GetMeasure()->GetSaturationSize()-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb)).GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,RedPoint,&m_redPrimaryBitmap,rect,pTooltip,pWnd,&RedPointRef, FALSE, dE10, FALSE);

			Msg.LoadString ( IDS_GREENSATPERCENT );
			str.Format(Msg, (i*100/(pDoc->GetMeasure()->GetSaturationSize()-1)));
			CCIEGraphPoint GreenPoint(pDoc->GetMeasure()->GetGreenSat(i).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			CCIEGraphPoint GreenPointRef(pDoc->GetMeasure()->GetRefSat(1, (double)i / (double)(pDoc->GetMeasure()->GetSaturationSize()-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb)).GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,GreenPoint,&m_greenPrimaryBitmap,rect,pTooltip,pWnd,&GreenPointRef, FALSE, dE10);

			Msg.LoadString ( IDS_BLUESATPERCENT );
			str.Format(Msg, (i*100/(pDoc->GetMeasure()->GetSaturationSize()-1)));
			CCIEGraphPoint BluePoint(pDoc->GetMeasure()->GetBlueSat(i).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			CCIEGraphPoint BluePointRef(pDoc->GetMeasure()->GetRefSat(2, (double)i / (double)(pDoc->GetMeasure()->GetSaturationSize()-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb)).GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,BluePoint,&m_bluePrimaryBitmap,rect,pTooltip,pWnd,&BluePointRef, FALSE, dE10);

			Msg.LoadString ( IDS_YELLOWSATPERCENT );
			str.Format(Msg, (i*100/(pDoc->GetMeasure()->GetSaturationSize()-1)));
			CCIEGraphPoint YellowPoint(pDoc->GetMeasure()->GetYellowSat(i).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			CCIEGraphPoint YellowPointRef(pDoc->GetMeasure()->GetRefSat(3, (double)i / (double)(pDoc->GetMeasure()->GetSaturationSize()-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb)).GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,YellowPoint,&m_yellowSecondaryBitmap,rect,pTooltip,pWnd,&YellowPointRef, FALSE, dE10);

			Msg.LoadString ( IDS_CYANSATPERCENT );
			str.Format(Msg, (i*100/(pDoc->GetMeasure()->GetSaturationSize()-1)));
			CCIEGraphPoint CyanPoint(pDoc->GetMeasure()->GetCyanSat(i).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			CCIEGraphPoint CyanPointRef(pDoc->GetMeasure()->GetRefSat(4, (double)i / (double)(pDoc->GetMeasure()->GetSaturationSize()-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb)).GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,CyanPoint,&m_cyanSecondaryBitmap,rect,pTooltip,pWnd,&CyanPointRef, FALSE, dE10);

			Msg.LoadString ( IDS_MAGENTASATPERCENT );
			str.Format(Msg, (i*100/(pDoc->GetMeasure()->GetSaturationSize()-1)));
			CCIEGraphPoint MagentaPoint(pDoc->GetMeasure()->GetMagentaSat(i).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			CCIEGraphPoint MagPointRef(pDoc->GetMeasure()->GetRefSat(5, (double)i / (double)(pDoc->GetMeasure()->GetSaturationSize()-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb)).GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,MagentaPoint,&m_magentaSecondaryBitmap,rect,pTooltip,pWnd,&MagPointRef, FALSE, dE10);
		}
	}
	if(m_doShowCCScale)
	{
		CColor ccolor;
		CString str;
		BOOL isExtPat =( GetConfig()->m_CCMode == USER || GetConfig()->m_CCMode == CM10SAT || GetConfig()->m_CCMode == CM10SAT75 || GetConfig()->m_CCMode == CM5SAT || GetConfig()->m_CCMode == CM5SAT75 || GetConfig()->m_CCMode == CM4SAT || GetConfig()->m_CCMode == CM4SAT75 || GetConfig()->m_CCMode == CM4LUM || GetConfig()->m_CCMode == CM5LUM || GetConfig()->m_CCMode == CM10LUM || GetConfig()->m_CCMode == RANDOM250 || GetConfig()->m_CCMode == RANDOM500 || GetConfig()->m_CCMode == CM6NB || GetConfig()->m_CCMode==MASCIOR50 || GetConfig()->m_CCMode == CMDNR);
		isExtPat = (isExtPat || GetConfig()->m_CCMode > 19);
        if (GetConfig()->m_CCMode != CCSG && !isExtPat && GetConfig()->m_CCMode != CMS && GetConfig()->m_CCMode != CPS && GetConfig()->m_CCMode != AXIS)
        {
			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_1a:(GetConfig()->m_CCMode == SKIN?IDS_CC_1b:IDS_CC_1) );
			str.Format(Msg, 10);
			CCIEGraphPoint cc1Point(pDoc->GetMeasure()->GetCC24Sat(0).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			pDoc->GetMeasure()->GetRefCC24Sat(0, ccolor);
			CCIEGraphPoint ccRefPoint(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc1Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&ccRefPoint, FALSE, dE10);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_2a:(GetConfig()->m_CCMode == SKIN?IDS_CC_2b:IDS_CC_2) );
			str.Format(Msg, 10);
			CCIEGraphPoint cc2Point(pDoc->GetMeasure()->GetCC24Sat(1).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			pDoc->GetMeasure()->GetRefCC24Sat(1, ccolor);
			ccRefPoint = CCIEGraphPoint(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc2Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&ccRefPoint, FALSE, dE10);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_3a:(GetConfig()->m_CCMode == SKIN?IDS_CC_3b:IDS_CC_3) );
			str.Format(Msg, 10);
			CCIEGraphPoint cc3Point(pDoc->GetMeasure()->GetCC24Sat(2).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			pDoc->GetMeasure()->GetRefCC24Sat(2, ccolor);
			ccRefPoint = CCIEGraphPoint(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc3Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&ccRefPoint, FALSE, dE10);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_4a:(GetConfig()->m_CCMode == SKIN?IDS_CC_4b:IDS_CC_4) );
			str.Format(Msg, 10);
			CCIEGraphPoint cc4Point(pDoc->GetMeasure()->GetCC24Sat(3).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			pDoc->GetMeasure()->GetRefCC24Sat(3, ccolor);
			ccRefPoint = CCIEGraphPoint(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc4Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&ccRefPoint, FALSE, dE10);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_5a:(GetConfig()->m_CCMode == SKIN?IDS_CC_5b:IDS_CC_5) );
			str.Format(Msg, 10);
			CCIEGraphPoint cc5Point(pDoc->GetMeasure()->GetCC24Sat(4).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			pDoc->GetMeasure()->GetRefCC24Sat(4, ccolor);
			ccRefPoint = CCIEGraphPoint(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc5Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&ccRefPoint, FALSE, dE10);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_6a:(GetConfig()->m_CCMode == SKIN?IDS_CC_6b:IDS_CC_6) );
			str.Format(Msg, 10);
			CCIEGraphPoint cc6Point(pDoc->GetMeasure()->GetCC24Sat(5).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			pDoc->GetMeasure()->GetRefCC24Sat(5, ccolor);
			ccRefPoint = CCIEGraphPoint(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc6Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&ccRefPoint, FALSE, dE10);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_7a:(GetConfig()->m_CCMode == SKIN?IDS_CC_7b:IDS_CC_7) );
			str.Format(Msg, 10);
			CCIEGraphPoint cc7Point(pDoc->GetMeasure()->GetCC24Sat(6).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			pDoc->GetMeasure()->GetRefCC24Sat(6, ccolor);
			ccRefPoint = CCIEGraphPoint(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc7Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&ccRefPoint, FALSE, dE10);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_8a:(GetConfig()->m_CCMode == SKIN?IDS_CC_8b:IDS_CC_8) );
			str.Format(Msg, 10);
			CCIEGraphPoint cc8Point(pDoc->GetMeasure()->GetCC24Sat(7).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			pDoc->GetMeasure()->GetRefCC24Sat(7, ccolor);
			ccRefPoint = CCIEGraphPoint(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc8Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&ccRefPoint, FALSE, dE10);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_9a:(GetConfig()->m_CCMode == SKIN?IDS_CC_9b:IDS_CC_9) );
			str.Format(Msg, 10);
			CCIEGraphPoint cc9Point(pDoc->GetMeasure()->GetCC24Sat(8).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			pDoc->GetMeasure()->GetRefCC24Sat(8, ccolor);
			ccRefPoint = CCIEGraphPoint(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc9Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&ccRefPoint, FALSE, dE10);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_10a:(GetConfig()->m_CCMode == SKIN?IDS_CC_10b:IDS_CC_10) );
			str.Format(Msg, 10);
			CCIEGraphPoint cc10Point(pDoc->GetMeasure()->GetCC24Sat(9).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			pDoc->GetMeasure()->GetRefCC24Sat(9, ccolor);
			ccRefPoint = CCIEGraphPoint(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc10Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&ccRefPoint, FALSE, dE10);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_11a:(GetConfig()->m_CCMode == SKIN?IDS_CC_11b:IDS_CC_11) );
			str.Format(Msg, 10);
			CCIEGraphPoint cc11Point(pDoc->GetMeasure()->GetCC24Sat(10).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			pDoc->GetMeasure()->GetRefCC24Sat(10, ccolor);
			ccRefPoint = CCIEGraphPoint(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc11Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&ccRefPoint, FALSE, dE10);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_12a:(GetConfig()->m_CCMode == SKIN?IDS_CC_12b:IDS_CC_12) );
			str.Format(Msg, 10);
			CCIEGraphPoint cc12Point(pDoc->GetMeasure()->GetCC24Sat(11).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			pDoc->GetMeasure()->GetRefCC24Sat(11, ccolor);
			ccRefPoint = CCIEGraphPoint(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc12Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&ccRefPoint, FALSE, dE10);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_13a:(GetConfig()->m_CCMode == SKIN?IDS_CC_13b:IDS_CC_13) );
			str.Format(Msg, 10);
			CCIEGraphPoint cc13Point(pDoc->GetMeasure()->GetCC24Sat(12).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			pDoc->GetMeasure()->GetRefCC24Sat(12, ccolor);
			ccRefPoint = CCIEGraphPoint(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc13Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&ccRefPoint, FALSE, dE10);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_14a:(GetConfig()->m_CCMode == SKIN?IDS_CC_14b:IDS_CC_14) );
			str.Format(Msg, 10);
			CCIEGraphPoint cc14Point(pDoc->GetMeasure()->GetCC24Sat(13).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			pDoc->GetMeasure()->GetRefCC24Sat(13, ccolor);
			ccRefPoint = CCIEGraphPoint(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc14Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&ccRefPoint, FALSE, dE10);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_15a:(GetConfig()->m_CCMode == SKIN?IDS_CC_15b:IDS_CC_15) );
			str.Format(Msg, 10);
			CCIEGraphPoint cc15Point(pDoc->GetMeasure()->GetCC24Sat(14).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			pDoc->GetMeasure()->GetRefCC24Sat(14, ccolor);
			ccRefPoint = CCIEGraphPoint(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc15Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&ccRefPoint, FALSE, dE10);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_16a:(GetConfig()->m_CCMode == SKIN?IDS_CC_16b:IDS_CC_16) );
			str.Format(Msg, 10);
			CCIEGraphPoint cc16Point(pDoc->GetMeasure()->GetCC24Sat(15).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			pDoc->GetMeasure()->GetRefCC24Sat(15, ccolor);
			ccRefPoint = CCIEGraphPoint(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc16Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&ccRefPoint, FALSE, dE10);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_17a:(GetConfig()->m_CCMode == SKIN?IDS_CC_17b:IDS_CC_17) );
			str.Format(Msg, 10);
			CCIEGraphPoint cc17Point(pDoc->GetMeasure()->GetCC24Sat(16).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			pDoc->GetMeasure()->GetRefCC24Sat(16, ccolor);
			ccRefPoint = CCIEGraphPoint(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc17Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&ccRefPoint, FALSE, dE10);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_18a:(GetConfig()->m_CCMode == SKIN?IDS_CC_18b:IDS_CC_18) );
			str.Format(Msg, 10);
			CCIEGraphPoint cc18Point(pDoc->GetMeasure()->GetCC24Sat(17).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			pDoc->GetMeasure()->GetRefCC24Sat(17, ccolor);
			ccRefPoint = CCIEGraphPoint(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc18Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&ccRefPoint, FALSE, dE10);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_19a:(GetConfig()->m_CCMode == SKIN?IDS_CC_19b:IDS_CC_19) );
			str.Format(Msg, 10);
			CCIEGraphPoint cc19Point(pDoc->GetMeasure()->GetCC24Sat(18).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			pDoc->GetMeasure()->GetRefCC24Sat(18, ccolor);
			ccRefPoint = CCIEGraphPoint(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc19Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&ccRefPoint, FALSE, dE10);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_20a:(GetConfig()->m_CCMode == SKIN?IDS_CC_20b:IDS_CC_20) );
			str.Format(Msg, 10);
			CCIEGraphPoint cc20Point(pDoc->GetMeasure()->GetCC24Sat(19).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			pDoc->GetMeasure()->GetRefCC24Sat(19, ccolor);
			ccRefPoint = CCIEGraphPoint(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc20Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&ccRefPoint, FALSE, dE10);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_21a:(GetConfig()->m_CCMode == SKIN?IDS_CC_21b:IDS_CC_21) );
			str.Format(Msg, 10);
			CCIEGraphPoint cc21Point(pDoc->GetMeasure()->GetCC24Sat(20).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			pDoc->GetMeasure()->GetRefCC24Sat(20, ccolor);
			ccRefPoint = CCIEGraphPoint(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc21Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&ccRefPoint, FALSE, dE10);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_22a:(GetConfig()->m_CCMode == SKIN?IDS_CC_22b:IDS_CC_22) );
			str.Format(Msg, 10);
			CCIEGraphPoint cc22Point(pDoc->GetMeasure()->GetCC24Sat(21).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			pDoc->GetMeasure()->GetRefCC24Sat(21, ccolor);
			ccRefPoint = CCIEGraphPoint(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc22Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&ccRefPoint, FALSE, dE10);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_23a:(GetConfig()->m_CCMode == SKIN?IDS_CC_23b:IDS_CC_23) );
			str.Format(Msg, 10);
			CCIEGraphPoint cc23Point(pDoc->GetMeasure()->GetCC24Sat(22).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			pDoc->GetMeasure()->GetRefCC24Sat(22, ccolor);
			ccRefPoint = CCIEGraphPoint(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc23Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&ccRefPoint, FALSE, dE10);

			Msg.LoadString ( GetConfig()->m_CCMode == CMC?IDS_CC_24a:(GetConfig()->m_CCMode == SKIN?IDS_CC_24b:IDS_CC_24) );
			str.Format(Msg, 10);
			CCIEGraphPoint cc24Point(pDoc->GetMeasure()->GetCC24Sat(23).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			pDoc->GetMeasure()->GetRefCC24Sat(23, ccolor);
			ccRefPoint = CCIEGraphPoint(ccolor.GetXYZValue(),
								  YWhiteRef,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,cc24Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&ccRefPoint, FALSE, dE10);
         }
         else
         {
			 CCPatterns ccPat = GetConfig()->m_CCMode;
             if (ccPat == CCSG || ccPat == CMS || ccPat == CPS || ccPat == AXIS)
             {
	            for (int i = 0; i < (ccPat == CCSG?96:(ccPat==AXIS?71:19)); i++)
                {
          			Msg.SetString ( ccPat==CCSG?PatName[i]:(ccPat==CMS?PatNameCMS[i]:(ccPat==CPS?PatNameCPS[i]:PatNameAXIS[i])) );
	    	    	str.Format(Msg, 10);
		            pDoc->GetMeasure()->GetRefCC24Sat(i, ccolor);
		            CCIEGraphPoint cc24PointRef(ccolor.GetXYZValue(),
								  YWhiteRef,
				        				  str, m_bCIEuv, m_bCIEab );
		    	    CCIEGraphPoint cc24Point(pDoc->GetMeasure()->GetCC24Sat(i).GetXYZValue(),
			    					  YWhite,
				    				  str, m_bCIEuv, m_bCIEab );
			        DrawAlphaBitmap(pDC,cc24Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&cc24PointRef, FALSE, dE10);
                }
             }
             else
             {
                    for (int i=0; i < GetConfig()->GetCColorsSize(); i++)
                    {
						std::string name = GetConfig()->GetCColorsN(i);
						// Patch name may contain '%' - passed as a Format argument, never as the template (see above).
						CString ccFmt; ccFmt.LoadString ( IDS_CC_COLORNAME );
						str.Format( (LPCSTR)ccFmt, name.c_str() );
		        	    pDoc->GetMeasure()->GetRefCC24Sat(i, ccolor);
		        	    CCIEGraphPoint cc24PointRef(ccolor.GetXYZValue(),
								  YWhiteRef,
				        				  str, m_bCIEuv, m_bCIEab );
    		    	    CCIEGraphPoint cc24Point(pDoc->GetMeasure()->GetCC24Sat(i).GetXYZValue(),
			    					  YWhite,
				    				  str, m_bCIEuv, m_bCIEab );
	    		        DrawAlphaBitmap(pDC,cc24Point,&m_grayPlotBitmap,rect,pTooltip,pWnd,&cc24PointRef, FALSE, dE10);
                    }
             }
         }
	}

	if(m_doShowMeasurements)
		for(int i=max(0,pDoc->GetMeasure()->GetMeasurementsSize()-20);i<pDoc->GetMeasure()->GetMeasurementsSize();i++)
		{
			CString str;
			Msg.LoadString ( IDS_MEASURENUM );
			str.Format(Msg,i);
			CCIEGraphPoint measurePoint(pDoc->GetMeasure()->GetMeasurement(i).GetXYZValue(),
								  YWhite,
								  str, m_bCIEuv, m_bCIEab );
			DrawAlphaBitmap(pDC,measurePoint,&m_measurePlotBitmap,rect,pTooltip,pWnd);
		}

	if ( pDoc->m_SelectedColor.isValid())
	{
		Msg.LoadString ( IDS_SELECTION );
		CCIEGraphPoint measurePoint(pDoc->m_SelectedColor.GetXYZValue(),
								 YWhite,
								 Msg, m_bCIEuv, m_bCIEab );
		DrawAlphaBitmap(pDC,measurePoint,&m_selectedPlotBitmap,rect,pTooltip,pWnd, NULL, TRUE, dE10);
	}
}

void CCIEChartGrapher::SaveGraphFile ( CDataSetDoc * pDoc, CSize ImageSize, LPCSTR lpszPathName, int ImageFormat, int ImageQuality, bool PDF )
{
	int				format;

	switch ( ImageFormat )
	{
		case 0: format = CXIMAGE_FORMAT_JPG; break;
		case 1: format = CXIMAGE_FORMAT_BMP; break;
		case 2: format = CXIMAGE_FORMAT_PNG; break;
		default: format = CXIMAGE_FORMAT_JPG; break;
	}

    CRect rect(0,0,ImageSize.cx,ImageSize.cy);

	CDC ScreenDC;
	ScreenDC.CreateDC ( "DISPLAY", NULL, NULL, NULL );

	CDC dc2;
    dc2.CreateCompatibleDC(&ScreenDC);

	CBitmap bitmap; 
    bitmap.CreateCompatibleBitmap(&ScreenDC,rect.Width(),rect.Height());

	ScreenDC.DeleteDC ();

    CBitmap *pOldBitmap=dc2.SelectObject(&bitmap);

	MakeBgBitmap(rect,GetConfig()->m_bWhiteBkgndOnFile && !PDF);
	DrawChart ( pDoc, & dc2, rect, NULL, NULL );
	dc2.SelectObject(pOldBitmap);

	CxImage *pImage = new CxImage();
	pImage->CreateFromHBITMAP(bitmap);

	if (pImage->IsValid())
	{
		pImage->SetJpegQuality(ImageQuality);
		pImage->Save(lpszPathName,format);
	}

	delete pImage;
}

/////////////////////////////////////////////////////////////////////////////
// CCIEChartView

IMPLEMENT_DYNCREATE(CCIEChartView, CSavingView)

CCIEChartView::CCIEChartView()
	: CSavingView()
{
	m_bDelayedUpdate = FALSE;
	m_bRealtimeIncrement = FALSE;
	m_bChartDirty = TRUE;
	m_bResizeSettling = FALSE;
	m_chartW = 0;
	m_chartH = 0;
	m_chartUserInfo = 0;
	m_chartMode = -1;
	m_chartEdit = -1;
	m_chartdE10 = 0.0;
	m_chartSelValid = FALSE;
	m_gestureStartDist = 0.0;
	m_gestureStartZoom = 1000;
	m_composeSize = CSize ( 0, 0 );
}

CCIEChartView::~CCIEChartView()
{
}

BEGIN_MESSAGE_MAP(CCIEChartView, CSavingView)
	//{{AFX_MSG_MAP(CCIEChartView)
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
	ON_WM_TIMER()
	ON_WM_CONTEXTMENU()
	ON_UPDATE_COMMAND_UI(IDM_CIE_SHOWBACKGROUND, OnUpdateCieShowbackground)
	ON_UPDATE_COMMAND_UI(IDM_CIE_SHOWDELTAE, OnUpdateCieShowDeltaE)
	ON_UPDATE_COMMAND_UI(IDM_CIE_SHOWREFERENCES, OnUpdateCieShowreferences)
	ON_UPDATE_COMMAND_UI(IDM_LUM_GRAPH_DATAREF, OnUpdateCieGraphShowDataRef)
	ON_COMMAND(IDM_CIE_SHOWREFERENCES, OnCieShowreferences)
	ON_COMMAND(IDM_LUM_GRAPH_DATAREF, OnCieGraphShowDataRef)
	ON_COMMAND(IDM_CIE_SHOWBACKGROUND, OnCieShowbackground)
	ON_COMMAND(IDM_CIE_SHOWDELTAE, OnCieShowDeltaE)
	ON_COMMAND(IDM_CIE_SHOWGRAYSCALE, OnCieShowGrayScale)
	ON_COMMAND(IDM_CIE_SHOWSATURATIONSCALE, OnCieShowSaturationScale)
	ON_COMMAND(IDM_CIE_SHOWSATURATIONSCALETARG, OnCieShowSaturationScaleTarg)
	ON_COMMAND(IDM_CIE_SHOWCCSCALE, OnCieShowCCScale)
	ON_COMMAND(IDM_CIE_SHOWCCSCALETARG, OnCieShowCCScaleTarg)
	ON_COMMAND(IDM_CIE_SHOWMEASUREMENTS, OnCieShowMeasurements)
	ON_COMMAND(IDM_GRAPH_Y_ZOOM_IN, OnGraphZoomIn)
	ON_COMMAND(IDM_GRAPH_Y_ZOOM_OUT, OnGraphZoomOut)
	ON_UPDATE_COMMAND_UI(IDM_CIE_SHOWMEASUREMENTS, OnUpdateCieShowMeasurements)
	ON_UPDATE_COMMAND_UI(IDM_CIE_SHOWGRAYSCALE, OnUpdateCieShowGrayScale)
	ON_UPDATE_COMMAND_UI(IDM_CIE_SHOWSATURATIONSCALE, OnUpdateCieShowSaturationScale)
	ON_UPDATE_COMMAND_UI(IDM_CIE_SHOWSATURATIONSCALETARG, OnUpdateCieShowSaturationScaleTarg)
	ON_UPDATE_COMMAND_UI(IDM_CIE_SHOWCCSCALE, OnUpdateCieShowCCScale)
	ON_UPDATE_COMMAND_UI(IDM_CIE_SHOWCCSCALETARG, OnUpdateCieShowCCScaleTarg)
	ON_COMMAND(IDM_CIE_SAVECHART, OnCieSavechart)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_COMMAND(IDM_HELP, OnHelp)
	ON_COMMAND(IDM_CIE_UV, OnCieUv)
	ON_UPDATE_COMMAND_UI(IDM_CIE_UV, OnUpdateCieUv)
	ON_COMMAND(IDM_CIE_AB, OnCieab)
	ON_UPDATE_COMMAND_UI(IDM_CIE_AB, OnUpdateCieab)
	ON_COMMAND(IDM_CIE_WORST10, OnCieShowdE10)
	ON_UPDATE_COMMAND_UI(IDM_CIE_WORST10, OnUpdateCieShowdE10)
	ON_WM_MOUSEWHEEL()
	ON_WM_PAINT()
	ON_WM_KEYDOWN()
	ON_MESSAGE(WM_GESTURE, OnGestureMsg)
	ON_NOTIFY (UDM_TOOLTIP_DISPLAY, NULL, NotifyDisplayTooltip)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCIEChartView diagnostics

#ifdef _DEBUG
void CCIEChartView::AssertValid() const
{
	CSavingView::AssertValid();
}

void CCIEChartView::Dump(CDumpContext& dc) const
{
	CSavingView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CCIEChartView message handlers

void CCIEChartView::OnInitialUpdate() 
{
	m_tooltip.Create(this);	
	m_tooltip.SetBehaviour(PPTOOLTIP_MULTIPLE_SHOW);
	m_tooltip.SetNotify(TRUE);
	m_tooltip.SetBorder(::CreateSolidBrush(RGB(212,175,55)),1,1);

	// Touch: keep the pinch gesture for zooming, but block the pan gesture so
	// single-finger drags promote to the mouse messages the existing
	// drag-pan/selection code already handles.
	GESTURECONFIG gestureConfig[] = { { GID_ZOOM, GC_ZOOM, 0 }, { GID_PAN, 0, GC_PAN } };
	::SetGestureConfig ( m_hWnd, 0, 2, gestureConfig, sizeof ( GESTURECONFIG ) );
}

void CCIEChartView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	CRect	Rect;

	// Any update hint means something changed: mark the retained chart
	// bitmap stale even for the hints below that skip the immediate repaint,
	// so the next natural paint re-renders (the pre-retained behavior).
	m_bChartDirty = TRUE;

	// Do nothing when not concerned
	switch ( lHint )
	{
		case UPD_NEARBLACK:
		case UPD_NEARWHITE:
		case UPD_CONTRAST:
		case UPD_GENERATORCONFIG:
		case UPD_SENSORCONFIG:
		case UPD_GENERALREFERENCES:
		return;
	}

	if ( IsWindowVisible () )
	{
		m_bDelayedUpdate = FALSE;
		GetReferenceRect ( & Rect );
		if (lHint != UPD_FREEMEASURES && lHint != UPD_REALTIME && lHint != UPD_FREEMEASUREAPPENDED && !GetDocument()->GetMeasure()->m_binMeasure)
			m_Grapher.MakeBgBitmap(Rect,GetConfig()->m_bWhiteBkgndOnScreen);
		// A realtime hint during a sweep adds exactly one measurement: paint it
		// incrementally. The sweep-end update comes as a non-realtime hint with
		// m_binMeasure off, which repaints everything (targets, tooltips, dE).
		if ( lHint >= UPD_REALTIME && lHint != UPD_DISPLAYPROFILE && lHint != UPD_REALTIME + 13 && GetDocument()->GetMeasure()->m_binMeasure )
			m_bRealtimeIncrement = TRUE;
		RedrawWindow( NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW );
		// The flag must not outlive the synchronous repaint above: if the paint
		// was skipped (window clipped to nothing), a later unrelated WM_PAINT
		// would otherwise take the incremental path instead of a full redraw.
		m_bRealtimeIncrement = FALSE;
	}
	else
	{
		// CIE chart is inside CMultiFrame window and is currently hidden. Do not recompute bitmap
		m_bDelayedUpdate = TRUE;
	}
}

DWORD CCIEChartView::GetUserInfo ()
{
	return	( ( m_Grapher.m_doDisplayBackground		& 0x0001 )	<< 0 )
		  + ( ( m_Grapher.m_doDisplayDeltaERef		& 0x0001 )	<< 1 )
		  + ( ( m_Grapher.m_doShowReferences		& 0x0001 )	<< 2 )
		  + ( ( m_Grapher.m_doShowDataRef			& 0x0001 )	<< 3 )
		  + ( ( m_Grapher.m_doShowGrayScale			& 0x0001 )	<< 4 )
		  + ( ( m_Grapher.m_doShowSaturationScale	& 0x0001 )	<< 5 )
		  + ( ( m_Grapher.m_doShowSaturationScaleTarg	& 0x0001 )	<< 6 )
		  + ( ( m_Grapher.m_doShowCCScale	& 0x0001 )	<< 7 )
		  + ( ( m_Grapher.m_doShowCCScaleTarg	& 0x0001 )	<< 8 )
		  + ( ( m_Grapher.m_doShowMeasurements		& 0x0001 )	<< 9 )
		  + ( ( m_Grapher.m_bCIEuv					& 0x0001 )	<< 10 )
		  + ( ( m_Grapher.m_bdE10				& 0x0001 )	<< 11 )
		  + ( ( m_Grapher.m_bCIEab				& 0x0001 )	<< 12 );
}

void CCIEChartView::SetUserInfo ( DWORD dwUserInfo )
{
	m_Grapher.m_doDisplayBackground		= ( dwUserInfo >> 0 ) & 0x0001;
	m_Grapher.m_doDisplayDeltaERef		= ( dwUserInfo >> 1 ) & 0x0001;
	m_Grapher.m_doShowReferences		= ( dwUserInfo >> 2 ) & 0x0001;
	m_Grapher.m_doShowDataRef			= ( dwUserInfo >> 3 ) & 0x0001;
	m_Grapher.m_doShowGrayScale			= ( dwUserInfo >> 4 ) & 0x0001;
	m_Grapher.m_doShowSaturationScale	= ( dwUserInfo >> 5 ) & 0x0001;
	m_Grapher.m_doShowSaturationScaleTarg	= ( dwUserInfo >> 6 ) & 0x0001;
	m_Grapher.m_doShowCCScale	= ( dwUserInfo >> 7 ) & 0x0001;
	m_Grapher.m_doShowCCScaleTarg	= ( dwUserInfo >> 8 ) & 0x0001;
	m_Grapher.m_doShowMeasurements		= ( dwUserInfo >> 9 ) & 0x0001;
	m_Grapher.m_bCIEuv					= ( dwUserInfo >> 10 ) & 0x0001;
	m_Grapher.m_bdE10					= ( dwUserInfo >> 11 ) & 0x0001;
	m_Grapher.m_bCIEab					= ( dwUserInfo >> 12 ) & 0x0001;
	
	m_bDelayedUpdate = TRUE;
}

void CCIEChartView::OnDraw(CDC* pDC) 
{
	if ( m_bDelayedUpdate )
	{
		// Perform late update
		OnUpdate ( NULL, 0, NULL );
	}

	CRect rect, refrect;
	GetClientRect(&rect);
	GetReferenceRect(&refrect);
	if ( rect.Width () <= 0 || rect.Height () <= 0 )
		return;

	BOOL bScalePreview = FALSE;

	// State DrawChart reads directly, with no update hint reaching this view
	// when it changes: MainView mode/edit state, the worst-dE threshold, and
	// the selected color. Together with the render size and the display
	// toggles it forms the content signature of the retained bitmap.
	CDataSetDoc * pDoc = GetDocument ();
	POSITION pos = pDoc -> GetFirstViewPosition ();
	CMainView * pMainView = (CMainView *) pDoc -> GetNextView ( pos );
	int curMode = pMainView -> m_displayMode;
	int curEdit = pMainView -> m_editCheckButton.GetCheck();
	double curdE10 = pMainView -> dE10min;
	BOOL selValid = pDoc -> m_SelectedColor.isValid();
	ColorXYZ selColor;
	if ( selValid )
		selColor = pDoc -> m_SelectedColor.GetXYZValue();

	BOOL bSigMatch = refrect.Width() == m_chartW && refrect.Height() == m_chartH
		&& GetUserInfo() == m_chartUserInfo
		&& curMode == m_chartMode && curEdit == m_chartEdit && curdE10 == m_chartdE10
		&& selValid == m_chartSelValid
		&& ( !selValid || ( selColor[0] == m_chartSel[0] && selColor[1] == m_chartSel[1] && selColor[2] == m_chartSel[2] ) );

	CDC dcDraw;
	dcDraw.CreateCompatibleDC(pDC);
	CBitmap *pOldBitmap=dcDraw.SelectObject(&m_Grapher.m_drawBitmap);
	if ( m_bRealtimeIncrement )
	{
		// Mid-sweep: the retained bitmap already shows the chart as of the
		// previous patch; add the just-measured point as a plain dot. Marker
		// styling, dE colouring and tooltips are restored by the full repaint
		// at sweep end.
		CDataSetDoc * pDoc = GetDocument ();
		if ( pDoc->m_SelectedColor.isValid() )
		{
			double YWhite = 1.0;
			if ( pDoc->GetMeasure()->GetPrimeWhite().isValid() )
				YWhite = pDoc->GetMeasure()->GetPrimeWhite() [ 1 ];
			else if ( pDoc->GetMeasure()->GetOnOffWhite().isValid() )
				YWhite = pDoc->GetMeasure()->GetOnOffWhite() [ 1 ];

			CCIEGraphPoint newPoint ( pDoc->m_SelectedColor.GetXYZValue(), YWhite, "", m_Grapher.m_bCIEuv, m_Grapher.m_bCIEab );
			m_Grapher.DrawGdiPlusMarker ( & dcDraw, & m_Grapher.m_measurePlotBitmap,
				newPoint.GetGraphX(refrect), newPoint.GetGraphY(refrect), newPoint, false );
		}
	}
	else if ( !m_bChartDirty && bSigMatch )
	{
		// Nothing changed since the last full render: the retained bitmap is
		// current and the blit below is the whole repaint (hover, uncover)
	}
	else if ( m_bResizeSettling && m_chartW > 0 )
	{
		// Live resize or zoom: scale the retained chart into the current
		// canvas placement (refrect at the pan deltas) and skip the full
		// render; the settle timer renders once for real when it pauses
		bScalePreview = TRUE;
	}
	else
	{
		m_Grapher.DrawChart ( GetDocument (), & dcDraw, refrect, & m_tooltip, this );
		m_chartW = refrect.Width();
		m_chartH = refrect.Height();
		m_chartUserInfo = GetUserInfo();
		m_chartMode = curMode;
		m_chartEdit = curEdit;
		m_chartdE10 = curdE10;
		m_chartSelValid = selValid;
		m_chartSel = selColor;
		m_bChartDirty = FALSE;
	}
	m_bRealtimeIncrement = FALSE;

	// Compose the visible chart and the pinned chip overlay off-screen and
	// flip to the window in a single blit: painting them in two steps
	// directly on screen made the chip corner flicker on every pan repaint
	if ( m_composeSize != rect.Size () )
	{
		if ( m_composeBitmap.m_hObject )
			m_composeBitmap.DeleteObject ();
		m_composeBitmap.CreateCompatibleBitmap ( pDC, rect.Width (), rect.Height () );
		// On GDI resource exhaustion keep the size null so creation retries
		m_composeSize = m_composeBitmap.m_hObject ? rect.Size () : CSize ( 0, 0 );
	}
	CDC dcOut;
	dcOut.CreateCompatibleDC ( pDC );
	CBitmap * pOldOut = m_composeBitmap.m_hObject ? dcOut.SelectObject ( & m_composeBitmap ) : NULL;
	// If the compose bitmap couldn't be created, paint the window directly
	// (the chips may flicker) rather than blitting from an empty DC
	CDC * pOut = pOldOut ? & dcOut : pDC;

	if ( bScalePreview )
	{
		pOut->SetStretchBltMode(HALFTONE);
		SetBrushOrgEx(pOut->GetSafeHdc(), 0, 0, NULL);
		pOut->StretchBlt(m_Grapher.m_DeltaX, m_Grapher.m_DeltaY, refrect.Width(), refrect.Height(),
			&dcDraw, 0, 0, m_chartW, m_chartH, SRCCOPY);
	}
	else
		pOut->BitBlt(0,0,rect.Width(),rect.Height(),&dcDraw,-m_Grapher.m_DeltaX,-m_Grapher.m_DeltaY,SRCCOPY);
	dcDraw.SelectObject(pOldBitmap);

	// The coverage chips are not part of the retained chart, so they stay
	// pinned to the window's top-right whatever the zoom/pan state
	m_Grapher.DrawCoverageChips ( pOut, rect, pDoc );

	if ( pOldOut )
	{
		pDC->BitBlt(0,0,rect.Width(),rect.Height(),&dcOut,0,0,SRCCOPY);
		dcOut.SelectObject(pOldOut);
	}
}

void CCIEChartView::SaveChart() 
{
	CSaveGraphDialog dialog;

	if(dialog.DoModal()!=IDOK)
		return;

    CRect rect;
	CSize size;

	switch(dialog.m_sizeType)
	{
		case 0:
		    GetClientRect(&rect);
			size = CSize(rect.Width(),rect.Height());
			break;
		case 1:
			size = CSize(300,200);
			break;
		case 2:
			size = CSize(600,400);
			break;
		case 3:
			size = CSize(dialog.m_saveWidth,dialog.m_saveHeight);
			break;

	}
 
	char *defExt;
	char *filter;

	switch(dialog.m_fileType)
	{
		case 0:
			defExt="jpg";
			filter="Jpeg File (*.jpg)|*.jpg||";
			break;
		case 1:
			defExt="bmp";
			filter="Bitmap File (*.bmp)|*.bmp||";
			break;
		case 2:
			defExt="jpg";
			filter="Portable Network Graphic File (*.png)|*.png||";
			break;
	}

	CFileDialog fileSaveDialog( FALSE, defExt, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, filter );
	if(fileSaveDialog.DoModal()==IDOK)
	{
		m_Grapher.SaveGraphFile ( GetDocument (), size, fileSaveDialog.GetPathName(), dialog.m_fileType, dialog.m_jpegQuality );

		// Recompute BgBitmap to match client size; this recreates m_drawBitmap
		// so the retained chart is gone and the next paint must render fully
		CRect clientRect;
		GetReferenceRect(&clientRect);
		m_Grapher.MakeBgBitmap(clientRect,GetConfig()->m_bWhiteBkgndOnScreen);
		m_bChartDirty = TRUE;
	}
}

BOOL CCIEChartView::OnEraseBkgnd(CDC* pDC) 
{
	return TRUE;
}

// CIE-001: MakeBgBitmap HALFTONE-resamples the whole chromaticity background,
// so running it per WM_SIZE makes drag-resize choppy. Defer to a settle-timer;
// rebuild once after the drag stops.
#define IDT_CIE_RESIZE_SETTLE 4201

void CCIEChartView::OnSize(UINT nType, int cx, int cy) 
{
	if(cx && cy)
	{
		CRect ClientRect = CRect(CPoint(0,0),CSize(cx,cy));

		if ( m_Grapher.m_ZoomFactor > 1000 )
		{
			// Zoom is active: keep the zoomed canvas below the size cap
			// (the factor is continuous now, so clamp instead of stepping)
			// and adjust deltaX and deltaY
			int nMaxFactor = min ( FX_MAXZOOMFACTOR, FX_MAXZOOMCANVASPX * 1000 / max ( cx, cy ) );
			m_Grapher.m_ZoomFactor = max ( 1000, min ( (int) m_Grapher.m_ZoomFactor, nMaxFactor ) );

			CRect RefRect(CPoint(0,0),CSize(cx*m_Grapher.m_ZoomFactor/1000,cy*m_Grapher.m_ZoomFactor/1000));

			if ( RefRect.right + m_Grapher.m_DeltaX < ClientRect.right )
				m_Grapher.m_DeltaX = ClientRect.right - RefRect.right;

			if ( RefRect.bottom + m_Grapher.m_DeltaY < ClientRect.bottom )
				m_Grapher.m_DeltaY = ClientRect.bottom - RefRect.bottom;
		}

		SchedulePreviewSettle ( 80 );
	}
	Invalidate(FALSE);
}

void CCIEChartView::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == IDT_CIE_RESIZE_SETTLE)
	{
		KillTimer(IDT_CIE_RESIZE_SETTLE);
		m_bResizeSettling = FALSE;
		CRect RefRect;
		GetReferenceRect(&RefRect);
		// MakeBgBitmap recreates m_drawBitmap at the new size, discarding the
		// retained chart: the next paint must be a full render
		m_Grapher.MakeBgBitmap(RefRect, GetConfig()->m_bWhiteBkgndOnScreen);
		m_bChartDirty = TRUE;
		Invalidate(FALSE);
		return;
	}
	CSavingView::OnTimer(nIDEvent);
}

void CCIEChartView::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	// load and display popup menu
	CNewMenu menu;
	menu.LoadMenu(IDR_CIE_MENU);
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup);
	
	pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL,
		point.x, point.y, GetParent());
}

void CCIEChartView::OnUpdateCieShowbackground(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable();
	pCmdUI->SetCheck(m_Grapher.m_doDisplayBackground);
}

void CCIEChartView::OnUpdateCieShowDeltaE(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable();
	pCmdUI->SetCheck(m_Grapher.m_doDisplayDeltaERef);
}

void CCIEChartView::OnUpdateCieShowreferences(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable();
	pCmdUI->SetCheck(m_Grapher.m_doShowReferences);
}

void CCIEChartView::OnUpdateCieGraphShowDataRef(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable();
	pCmdUI->SetCheck(m_Grapher.m_doShowDataRef);
}

void CCIEChartView::OnUpdateCieShowGrayScale(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable();
	pCmdUI->SetCheck(m_Grapher.m_doShowGrayScale);
}

void CCIEChartView::OnUpdateCieShowSaturationScale(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable();
	pCmdUI->SetCheck(m_Grapher.m_doShowSaturationScale);
}

void CCIEChartView::OnUpdateCieShowSaturationScaleTarg(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable();
	pCmdUI->SetCheck(m_Grapher.m_doShowSaturationScaleTarg);
}

void CCIEChartView::OnUpdateCieShowCCScale(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable();
	pCmdUI->SetCheck(m_Grapher.m_doShowCCScale);
}

void CCIEChartView::OnUpdateCieShowdE10(CCmdUI* pCmdUI) 
{
	if (m_Grapher.dE10 > 0)
	{
		pCmdUI->Enable();
		pCmdUI->SetCheck(m_Grapher.m_bdE10);
	}
	else
		pCmdUI->Enable(FALSE);
}

void CCIEChartView::OnUpdateCieShowCCScaleTarg(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable();
	pCmdUI->SetCheck(m_Grapher.m_doShowCCScaleTarg);
}

void CCIEChartView::OnUpdateCieShowMeasurements(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable();
	pCmdUI->SetCheck(m_Grapher.m_doShowMeasurements);
}

void CCIEChartView::OnCieShowbackground() 
{
	m_Grapher.m_doDisplayBackground = !m_Grapher.m_doDisplayBackground;
	GetConfig()->WriteProfileInt("CIE Chart","Display Background",m_Grapher.m_doDisplayBackground);
	CRect rect;
	GetReferenceRect(&rect);
	m_Grapher.MakeBgBitmap(rect,GetConfig()->m_bWhiteBkgndOnScreen);
	Invalidate(FALSE);
}

void CCIEChartView::OnCieShowDeltaE() 
{
	m_Grapher.m_doDisplayDeltaERef = !m_Grapher.m_doDisplayDeltaERef;
	GetConfig()->WriteProfileInt("CIE Chart","Display Delta E",m_Grapher.m_doDisplayDeltaERef);
	CRect rect;
	GetReferenceRect(&rect);
	m_Grapher.MakeBgBitmap(rect,GetConfig()->m_bWhiteBkgndOnScreen);
	Invalidate(FALSE);
}

void CCIEChartView::OnCieShowreferences() 
{
	m_Grapher.m_doShowReferences = !m_Grapher.m_doShowReferences;
	GetConfig()->WriteProfileInt("CIE Chart","Show References",m_Grapher.m_doShowReferences);
	Invalidate(FALSE);
}

void CCIEChartView::OnCieGraphShowDataRef()
{
	m_Grapher.m_doShowDataRef = !m_Grapher.m_doShowDataRef;
	GetConfig()->WriteProfileInt("CIE Chart","Show Reference Data",m_Grapher.m_doShowDataRef);
	Invalidate(FALSE);
}


void CCIEChartView::OnCieShowGrayScale() 
{
	m_Grapher.m_doShowGrayScale = !m_Grapher.m_doShowGrayScale;
	GetConfig()->WriteProfileInt("CIE Chart","Display GrayScale",m_Grapher.m_doShowGrayScale);
	Invalidate(FALSE);
}

void CCIEChartView::OnCieShowSaturationScale() 
{
	m_Grapher.m_doShowSaturationScale = !m_Grapher.m_doShowSaturationScale;
	GetConfig()->WriteProfileInt("CIE Chart","Display Saturation Scale",m_Grapher.m_doShowSaturationScale);
	Invalidate(FALSE);
}

void CCIEChartView::OnCieShowSaturationScaleTarg() 
{
	m_Grapher.m_doShowSaturationScaleTarg = !m_Grapher.m_doShowSaturationScaleTarg;
	GetConfig()->WriteProfileInt("CIE Chart","Display Saturation Scale Targets",m_Grapher.m_doShowSaturationScaleTarg);
	Invalidate(FALSE);
}

void CCIEChartView::OnCieShowCCScale() 
{
	m_Grapher.m_doShowCCScale = !m_Grapher.m_doShowCCScale;
	GetConfig()->WriteProfileInt("CIE Chart","Display Color Checker measures",m_Grapher.m_doShowCCScale);
	Invalidate(FALSE);
}

void CCIEChartView::OnCieShowdE10() 
{
	m_Grapher.m_bdE10 = !m_Grapher.m_bdE10;
	GetConfig()->WriteProfileInt("CIE Chart","Worst dE",m_Grapher.m_bdE10);
	Invalidate(FALSE);
}

void CCIEChartView::OnCieShowCCScaleTarg() 
{
	m_Grapher.m_doShowCCScaleTarg = !m_Grapher.m_doShowCCScaleTarg;
	GetConfig()->WriteProfileInt("CIE Chart","Display Color Checker Targets",m_Grapher.m_doShowCCScaleTarg);
	Invalidate(FALSE);
}

void CCIEChartView::OnCieShowMeasurements() 
{
	m_Grapher.m_doShowMeasurements = !m_Grapher.m_doShowMeasurements;
	GetConfig()->WriteProfileInt("CIE Chart","Show Measurements",m_Grapher.m_doShowMeasurements);
	Invalidate(FALSE);
}

// Restart the settle timer: OnDraw paints scaled previews of the retained
// chart until the timer fires and runs the one real render.
void CCIEChartView::SchedulePreviewSettle ( UINT nDelayMs )
{
	KillTimer(IDT_CIE_RESIZE_SETTLE);
	SetTimer(IDT_CIE_RESIZE_SETTLE, nDelayMs, NULL);
	m_bResizeSettling = TRUE;
}

// Anchored zoom shared by the mouse wheel, the menu commands and the pinch
// gesture: the chart point under ptAnchorClient stays put while the factor
// changes. Paints preview by scaling the retained chart; the settle timer
// runs the one real render when the zooming pauses.
void CCIEChartView::ZoomChart ( int nNewFactor, CPoint ptAnchorClient )
{
	CRect	ClientRect, RefRect;
	GetClientRect ( & ClientRect );
	if ( ClientRect.Width () <= 0 || ClientRect.Height () <= 0 )
		return;

	// Keep the zoomed canvas (= retained bitmap size) capped
	int nMaxSide = max ( ClientRect.Width (), ClientRect.Height () );
	int nMaxFactor = min ( FX_MAXZOOMFACTOR, FX_MAXZOOMCANVASPX * 1000 / nMaxSide );
	nNewFactor = max ( 1000, min ( nNewFactor, nMaxFactor ) );

	int nOldFactor = (int) m_Grapher.m_ZoomFactor;
	if ( nNewFactor == nOldFactor )
		return;

	// Keep the canvas point under the anchor stationary: canvas coordinates
	// scale by the factor ratio, deltas are the (negative) canvas origin
	m_Grapher.m_ZoomFactor = nNewFactor;
	m_Grapher.m_DeltaX = ptAnchorClient.x - (int) ( (__int64) ( ptAnchorClient.x - m_Grapher.m_DeltaX ) * nNewFactor / nOldFactor );
	m_Grapher.m_DeltaY = ptAnchorClient.y - (int) ( (__int64) ( ptAnchorClient.y - m_Grapher.m_DeltaY ) * nNewFactor / nOldFactor );

	GetReferenceRect ( & RefRect );
	if ( m_Grapher.m_DeltaX > 0 )
		m_Grapher.m_DeltaX = 0;
	else if ( m_Grapher.m_DeltaX < ClientRect.right - RefRect.right )
		m_Grapher.m_DeltaX = ClientRect.right - RefRect.right;

	if ( m_Grapher.m_DeltaY > 0 )
		m_Grapher.m_DeltaY = 0;
	else if ( m_Grapher.m_DeltaY < ClientRect.bottom - RefRect.bottom )
		m_Grapher.m_DeltaY = ClientRect.bottom - RefRect.bottom;

	// Preview now, render for real once the zoom pauses
	SchedulePreviewSettle ( 120 );
	Invalidate ( FALSE );
}

void CCIEChartView::OnGraphZoomIn()
{
	CRect ClientRect;
	GetClientRect ( & ClientRect );
	ZoomChart ( m_Grapher.m_ZoomFactor * 5 / 4, ClientRect.CenterPoint () );
}

void CCIEChartView::OnGraphZoomOut()
{
	CRect ClientRect;
	GetClientRect ( & ClientRect );
	ZoomChart ( m_Grapher.m_ZoomFactor * 4 / 5, ClientRect.CenterPoint () );
}

void CCIEChartView::OnCieUv() 
{
	m_Grapher.m_bCIEab = FALSE;
	m_Grapher.m_bCIEuv = !m_Grapher.m_bCIEuv;
	GetConfig()->WriteProfileInt("CIE Chart","CIE uv mode",m_Grapher.m_bCIEuv);
	GetConfig()->WriteProfileInt("CIE Chart","CIE ab mode",FALSE);

	CRect rect;
	GetReferenceRect(&rect);
	m_Grapher.MakeBgBitmap(rect,GetConfig()->m_bWhiteBkgndOnScreen);
	Invalidate(FALSE);
}

void CCIEChartView::OnCieab() 
{

	m_Grapher.m_bCIEuv = FALSE;
	m_Grapher.m_bCIEab = !m_Grapher.m_bCIEab;
	GetConfig()->WriteProfileInt("CIE Chart","CIE ab mode",m_Grapher.m_bCIEab);
	GetConfig()->WriteProfileInt("CIE Chart","CIE uv mode",FALSE);

	CRect rect;
	GetReferenceRect(&rect);
	m_Grapher.MakeBgBitmap(rect,GetConfig()->m_bWhiteBkgndOnScreen);
	Invalidate(FALSE);
}

void CCIEChartView::OnUpdateCieUv(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable();
	pCmdUI->SetCheck(m_Grapher.m_bCIEuv);
}

void CCIEChartView::OnUpdateCieab(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable();
	pCmdUI->SetCheck(m_Grapher.m_bCIEab);
}

BOOL CCIEChartView::PreTranslateMessage(MSG* pMsg) 
{

	if(pMsg->message == WM_SYSKEYDOWN)
	{
		if (pMsg->wParam == VK_UP)
		{
			OnGraphZoomIn();
			return TRUE;
		}
		if (pMsg->wParam == VK_DOWN)
		{
			OnGraphZoomOut();
			return TRUE;
		}
	}

	if(pMsg->message == WM_KEYDOWN)
	{
		if (pMsg->wParam == VK_UP || pMsg->wParam == VK_DOWN || pMsg->wParam == VK_RIGHT || pMsg->wParam == VK_LEFT)
		{
			OnKeyDown(pMsg->wParam, 2, 0);
			return TRUE;
		}
	}

	m_tooltip.RelayEvent(pMsg);	
	
	return CSavingView::PreTranslateMessage(pMsg);
}

void CCIEChartView::OnCieSavechart() 
{
	SaveChart();
}

void CCIEChartView::OnLButtonDown(UINT nFlags, CPoint point) 
{
	// TODO: Add your message handler code here and/or call default
	SetCapture ();
	m_CurMousePoint = point;
	UpdateTestColor ( point );
}

void CCIEChartView::OnLButtonUp(UINT nFlags, CPoint point) 
{
	// TODO: Add your message handler code here and/or call default
	if ( GetCapture () )
	{
		if ( m_Grapher.m_ZoomFactor > 1000 )
		{
			// Update 
			int		OldDeltaX = m_Grapher.m_DeltaX;
			int		OldDeltaY = m_Grapher.m_DeltaY;
			CRect	ClientRect, RefRect;

			GetClientRect ( & ClientRect );
			GetReferenceRect ( & RefRect );

			m_Grapher.m_DeltaX += point.x - m_CurMousePoint.x;
			if ( m_Grapher.m_DeltaX > 0 )
				m_Grapher.m_DeltaX = 0;
			else if ( m_Grapher.m_DeltaX < ClientRect.right - RefRect.right )
				m_Grapher.m_DeltaX = ClientRect.right - RefRect.right;

			m_Grapher.m_DeltaY += point.y - m_CurMousePoint.y;
			if ( m_Grapher.m_DeltaY > 0 )
				m_Grapher.m_DeltaY = 0;
			else if ( m_Grapher.m_DeltaY < ClientRect.bottom - RefRect.bottom )
				m_Grapher.m_DeltaY = ClientRect.bottom - RefRect.bottom;
			
			m_CurMousePoint = point;

			if ( m_Grapher.m_DeltaX != OldDeltaX || m_Grapher.m_DeltaY != OldDeltaY )
			{
				// Repaint from the retained canvas at the new deltas (a cheap
				// blit). ScrollWindow would drag the pinned chip overlay along
				// with the pixels and leave trails. The settle render then
				// re-anchors the tooltip rects to the new deltas.
				Invalidate ( FALSE );
				SchedulePreviewSettle ( 80 );
			}
		}

		UpdateTestColor ( point );
		ReleaseCapture ();
	}
}

void CCIEChartView::OnMouseMove(UINT nFlags, CPoint point) 
{
	// TODO: Add your message handler code here and/or call default
	if ( GetCapture () )
	{
		if ( m_Grapher.m_ZoomFactor > 1000 )
		{
			// Update 
			int		OldDeltaX = m_Grapher.m_DeltaX;
			int		OldDeltaY = m_Grapher.m_DeltaY;
			CRect	ClientRect, RefRect;

			GetClientRect ( & ClientRect );
			GetReferenceRect ( & RefRect );

			m_Grapher.m_DeltaX += point.x - m_CurMousePoint.x;
			if ( m_Grapher.m_DeltaX > 0 )
				m_Grapher.m_DeltaX = 0;
			else if ( m_Grapher.m_DeltaX < ClientRect.right - RefRect.right )
				m_Grapher.m_DeltaX = ClientRect.right - RefRect.right;

			m_Grapher.m_DeltaY += point.y - m_CurMousePoint.y;
			if ( m_Grapher.m_DeltaY > 0 )
				m_Grapher.m_DeltaY = 0;
			else if ( m_Grapher.m_DeltaY < ClientRect.bottom - RefRect.bottom )
				m_Grapher.m_DeltaY = ClientRect.bottom - RefRect.bottom;
			
			m_CurMousePoint = point;

			// Repaint at the new deltas instead of ScrollWindow (see OnLButtonUp)
			if ( m_Grapher.m_DeltaX != OldDeltaX || m_Grapher.m_DeltaY != OldDeltaY )
			{
				Invalidate ( FALSE );
				SchedulePreviewSettle ( 80 );
			}
		}

		UpdateTestColor ( point );
	}
}


BOOL CCIEChartView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	// Wheel up zooms in, anchored at the cursor. Precision touchpads deliver
	// pinches as fine-grained Ctrl+wheel, so scale the step by the actual
	// delta instead of a fixed notch for a smooth pinch.
	ScreenToClient ( & pt );
	double ratio = pow ( 1.25, (double) zDelta / 120.0 );
	ZoomChart ( (int) ( m_Grapher.m_ZoomFactor * ratio + 0.5 ), pt );
	return TRUE;
}

// Touch pinch (WM_GESTURE / GID_ZOOM): ullArguments carries the distance
// between the two touch points; zoom follows the ratio to the distance
// captured when the gesture began, anchored at the gesture center.
LRESULT CCIEChartView::OnGestureMsg(WPARAM wParam, LPARAM lParam)
{
	GESTUREINFO gi;
	ZeroMemory ( & gi, sizeof ( gi ) );
	gi.cbSize = sizeof ( gi );

	if ( ! ::GetGestureInfo ( (HGESTUREINFO) lParam, & gi ) || gi.dwID != GID_ZOOM )
		return DefWindowProc ( WM_GESTURE, wParam, lParam );

	if ( gi.dwFlags & GF_BEGIN )
	{
		m_gestureStartDist = (double) (ULONGLONG) gi.ullArguments;
		m_gestureStartZoom = m_Grapher.m_ZoomFactor;
	}
	else if ( gi.dwFlags & GF_END )
	{
		// The END event repeats the last distance; nothing to zoom. Reset the
		// baseline so a stray GID_ZOOM without GF_BEGIN can't scale against a
		// previous gesture's distance.
		m_gestureStartDist = 0.0;
	}
	else if ( m_gestureStartDist > 0.0 )
	{
		CPoint pt ( gi.ptsLocation.x, gi.ptsLocation.y );
		ScreenToClient ( & pt );
		ZoomChart ( (int) ( (double) m_gestureStartZoom * (double) (ULONGLONG) gi.ullArguments / m_gestureStartDist + 0.5 ), pt );
	}

	::CloseGestureInfoHandle ( (HGESTUREINFO) lParam );
	return 0;
}

void CCIEChartView::UpdateTestColor ( CPoint point )
{
	int		nR = 0, nG = 0, nB = 0;
	CRect	rect;
	double	x, y;
	double	u, v;
	double	cmax;
	double	base, coef;
    double gamma = (GetConfig()->m_useMeasuredGamma)?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef);
    ColorRGB	RGBColor;

	GetReferenceRect ( & rect );

	//-0.75 & -0.05 offsets from Chartimage drawing
	x = (double)(point.x-m_Grapher.m_DeltaX) / (double)rect.Width() * (m_Grapher.m_bCIEuv ? 0.8 : 0.9) - 0.075;
	y = (double)(rect.bottom-(point.y-m_Grapher.m_DeltaY)) / (double)rect.Height() * (m_Grapher.m_bCIEuv ? 0.8 : 1.0) - 0.05;

	//need to convert xy->ab colors
//	if (m_Grapher.m_bCIEab)
//	{
//		x = (double)(point.x-m_Grapher.m_DeltaX) / (double)rect.Width() * (400) - 220.;
//		y = (double)(rect.bottom-(point.y-m_Grapher.m_DeltaY)) / (double)rect.Height() * (400) - 200;
//	}

	if ( m_Grapher.m_bCIEuv )
	{
		u = x;
		v = y;
		x = ( 9.0 * u ) / ( ( 6.0 * u ) - ( 16.0 * v ) + 12.0 );
		y = ( 4.0 * v ) / ( ( 6.0 * u ) - ( 16.0 * v ) + 12.0 );
	}

	if ( x > 0.0 && x < 1.0 && y > 0.0 && y < 1.0 )
	{
		CColor	ClickedColor ( x, y );
		CColor White = GetDocument()->GetMeasure()->GetOnOffWhite();
		CColor Black = GetDocument()->GetMeasure()->GetOnOffBlack();
		int cRef=GetColorReference().m_standard;
		ClickedColor.SetY(1.0);
		RGBColor = ClickedColor.GetRGBValue ((cRef == HDTVa || cRef == HDTVb)?CColorReference(HDTV):(cRef == UHDTV3 || cRef == UHDTV4)?CColorReference(UHDTV2):GetColorReference());
        double r=RGBColor[0],g=RGBColor[1],b=RGBColor[2];

		cmax = max(r,g);
		if ( b>cmax)
			cmax=b;

		base = 0.0;
		coef = 255.0;

		nR = (int) (r/cmax*coef+base);
		nG = (int) (g/cmax*coef+base);
		nB = (int) (b/cmax*coef+base);
	}

	( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) ->m_wndTestColorWnd.m_colorPicker.SetColor ( RGB(nR,nG,nB) );
	( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) ->m_wndTestColorWnd.RedrawWindow ();
}

void CCIEChartView::GetReferenceRect ( LPRECT lpRect )
{
	GetClientRect ( lpRect );
	lpRect -> right = lpRect -> right * m_Grapher.m_ZoomFactor / 1000;
	lpRect -> bottom = lpRect -> bottom * m_Grapher.m_ZoomFactor / 1000;
}

void CCIEChartView::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_CIECHART, NULL );
}



void CCIEChartView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	// TODO: Add your message handler code here and/or call default
	int		OldDeltaX = m_Grapher.m_DeltaX;
	int		OldDeltaY = m_Grapher.m_DeltaY;
	CRect	ClientRect, RefRect;

	GetClientRect ( & ClientRect );
	GetReferenceRect ( & RefRect );

	switch ( nChar )
	{
		case VK_UP:
			 m_Grapher.m_DeltaY += 10;
			 break;
		case VK_DOWN:
			 m_Grapher.m_DeltaY -= 10;
			 break;
		case VK_LEFT:
			 m_Grapher.m_DeltaX += 10;
			 break;
		case VK_RIGHT:
			 m_Grapher.m_DeltaX -= 10;
			 break;
	}

	if ( m_Grapher.m_DeltaX > 0 )
		m_Grapher.m_DeltaX = 0;
	else if ( m_Grapher.m_DeltaX < ClientRect.right - RefRect.right )
		m_Grapher.m_DeltaX = ClientRect.right - RefRect.right;

	if ( m_Grapher.m_DeltaY > 0 )
		m_Grapher.m_DeltaY = 0;
	else if ( m_Grapher.m_DeltaY < ClientRect.bottom - RefRect.bottom )
		m_Grapher.m_DeltaY = ClientRect.bottom - RefRect.bottom;

	// Repaint at the new deltas instead of ScrollWindow (see OnLButtonUp)
	if ( m_Grapher.m_DeltaX != OldDeltaX || m_Grapher.m_DeltaY != OldDeltaY )
	{
		Invalidate ( FALSE );
		SchedulePreviewSettle ( 80 );
	}

	CSavingView::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CCIEChartView::NotifyDisplayTooltip(NMHDR * pNMHDR, LRESULT * result)
{
    *result = 0;
    NM_PPTOOLTIP_DISPLAY * pNotify = (NM_PPTOOLTIP_DISPLAY*)pNMHDR;
	int nID=pNotify->ti->nIDTool;
	pNotify->ti->nEffect = CPPDrawManager::EFFECT_SOLID;
	pNotify->ti->nGranularity = 0;
	pNotify->ti->nTransparency = 0;
	pNotify->ti->crBegin=stRGB[nID];
	pNotify->ti->crEnd=eRGB[nID];
}
