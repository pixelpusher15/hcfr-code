/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2011 Association Homecinema Francophone.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////
//
//  This file is subject to the terms of the GNU General Public License as
//  published by the Free Software Foundation.  A copy of this license is
//  included with this software distribution in the file COPYING.htm. If you
//  do not have a copy, you may obtain a copy by writing to the Free
//  Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//2.22
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details
/////////////////////////////////////////////////////////////////////////////
//  Author(s):
//	François-Xavier CHABOUD
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// SatLumShiftView.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "DataSetDoc.h"
#include "DocTempl.h"
#include "SatLumShiftView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// CSatLumShiftGrapher

CSatLumShiftGrapher::CSatLumShiftGrapher ()
{
	CString		Msg;

	Msg.LoadString ( IDS_PERCENTSATRED );
	m_redSatGraphID = m_graphCtrl.AddGraph(RGB(255,0,0), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_PERCENTSATGREEN );
	m_greenSatGraphID = m_graphCtrl.AddGraph(RGB(0,255,0), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_PERCENTSATBLUE );
	m_blueSatGraphID = m_graphCtrl.AddGraph(RGB(70,70,255), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_PERCENTSATYELLOW );
	m_yellowSatGraphID = m_graphCtrl.AddGraph(RGB(255,255,0), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_PERCENTSATCYAN );
	m_cyanSatGraphID = m_graphCtrl.AddGraph(RGB(0,255,255), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_PERCENTSATMAGENTA );
	m_magentaSatGraphID = m_graphCtrl.AddGraph(RGB(255,0,255), (LPSTR)(LPCSTR)Msg);

	Msg.LoadString ( IDS_PERCENTSATREDDATAREF );
	m_redSatDataRefGraphID = m_graphCtrl.AddGraph(RGB(255,0,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_PERCENTSATGREENDATAREF );
	m_greenSatDataRefGraphID = m_graphCtrl.AddGraph(RGB(0,255,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_PERCENTSATBLUEDATAREF );
	m_blueSatDataRefGraphID = m_graphCtrl.AddGraph(RGB(70,70,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_PERCENTSATYELLOWDATAREF );
	m_yellowSatDataRefGraphID = m_graphCtrl.AddGraph(RGB(255,255,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_PERCENTSATCYANDATAREF );
	m_cyanSatDataRefGraphID = m_graphCtrl.AddGraph(RGB(0,255,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_PERCENTSATMAGENTADATAREF );
	m_magentaSatDataRefGraphID = m_graphCtrl.AddGraph(RGB(255,0,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT);

	Msg.LoadString ( IDS_REFERENCE );
	m_refGraphID = m_graphCtrl.AddGraph(RGB(230,230,230), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	
	m_graphCtrl.SetXAxisProps("% Sat", 10, 0, 100);
	m_graphCtrl.SetYAxisProps("%", 5, -100, 100);
	m_graphCtrl.SetScale(0,100,-20,20);
	m_graphCtrl.ReadSettings("Saturation Shift Sat");

	Msg.LoadString ( IDS_DELTAERED );
	m_redColorGraphID = m_graphCtrl2.AddGraph(RGB(255,0,0), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_DELTAEGREEN );
	m_greenColorGraphID = m_graphCtrl2.AddGraph(RGB(0,255,0), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_DELTAEBLUE );
	m_blueColorGraphID = m_graphCtrl2.AddGraph(RGB(70,70,255), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_DELTAEYELLOW );
	m_yellowColorGraphID = m_graphCtrl2.AddGraph(RGB(255,255,0), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_DELTAECYAN );
	m_cyanColorGraphID = m_graphCtrl2.AddGraph(RGB(0,255,255), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_DELTAEMAGENTA );
	m_magentaColorGraphID = m_graphCtrl2.AddGraph(RGB(255,0,255), (LPSTR)(LPCSTR)Msg);

	Msg.LoadString ( IDS_DELTAEREDDATAREF );
	m_redColorDataRefGraphID = m_graphCtrl2.AddGraph(RGB(255,0,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_DELTAEGREENDATAREF );
	m_greenColorDataRefGraphID = m_graphCtrl2.AddGraph(RGB(0,255,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_DELTAEBLUEDATAREF );
	m_blueColorDataRefGraphID = m_graphCtrl2.AddGraph(RGB(70,70,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_DELTAEYELLOWDATAREF );
	m_yellowColorDataRefGraphID = m_graphCtrl2.AddGraph(RGB(255,255,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_DELTAECYANDATAREF );
	m_cyanColorDataRefGraphID = m_graphCtrl2.AddGraph(RGB(0,255,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_DELTAEMAGENTADATAREF );
	m_magentaColorDataRefGraphID = m_graphCtrl2.AddGraph(RGB(255,0,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT);

	m_graphCtrl2.SetXAxisProps("% Sat", 10, 0, 100);
	m_graphCtrl2.SetYAxisProps("", 2, 0, 20);
	m_graphCtrl2.SetScale(0,100,0,20);
	m_graphCtrl2.ReadSettings("Saturation Shift Color");

	m_showReference=GetConfig()->GetProfileInt("Saturation Shift","Show Reference",TRUE);
	m_showRed=GetConfig()->GetProfileInt("Saturation Shift","Show Red",TRUE);
	m_showGreen=GetConfig()->GetProfileInt("Saturation Shift","Show Green",TRUE);
	m_showBlue=GetConfig()->GetProfileInt("Saturation Shift","Show Blue",TRUE);
	m_showYellow=GetConfig()->GetProfileInt("Saturation Shift","Show Yellow",TRUE);
	m_showCyan=GetConfig()->GetProfileInt("Saturation Shift","Show Cyan",TRUE);
	m_showMagenta=GetConfig()->GetProfileInt("Saturation Shift","Show Magenta",TRUE);
	m_showDataRef=GetConfig()->GetProfileInt("Saturation Shift","Show Reference Data",TRUE);
	CString strTitle;
	strTitle.LoadString ( IDS_GRAPH_SATSCANSHIFTS );
	m_graphCtrl.m_graphArray[0].p_Title=strTitle;

}

void CSatLumShiftGrapher::UpdateGraph ( CDataSetDoc * pDoc )
{
	CDataSetDoc *pDataRef = GetDataRef();
	int size=pDoc->GetMeasure()->GetSaturationSize();

	if ( pDataRef )
	{
		// Check if data reference is comparable
		if ( pDataRef->GetMeasure()->GetSaturationSize() != size )
		{
			// Cannot use data reference
			pDataRef = NULL;
		}
	}

	if(IsWindow(m_graphCtrl.m_hWnd))
		m_graphCtrl.ShowWindow(SW_SHOW);
	if(IsWindow(m_graphCtrl2.m_hWnd))
		m_graphCtrl2.ShowWindow(SW_SHOW);

	m_graphCtrl.ClearGraph(m_refGraphID);
	
	if (m_showReference && m_refGraphID != -1 && size > 0)
	{	
		m_graphCtrl.AddPoint(m_refGraphID, 0, 0.0);
		m_graphCtrl.AddPoint(m_refGraphID, 100, 0.0);
	}

	// GetRefSat's hue order: 0=R 1=G 2=B 3=Y 4=C 5=M
	const struct { BOOL bShow; long satID, colorID, satRefID, colorRefID; } hues[6] =
	{
		{ m_showRed,     m_redSatGraphID,     m_redColorGraphID,     m_redSatDataRefGraphID,     m_redColorDataRefGraphID },
		{ m_showGreen,   m_greenSatGraphID,   m_greenColorGraphID,   m_greenSatDataRefGraphID,   m_greenColorDataRefGraphID },
		{ m_showBlue,    m_blueSatGraphID,    m_blueColorGraphID,    m_blueSatDataRefGraphID,    m_blueColorDataRefGraphID },
		{ m_showYellow,  m_yellowSatGraphID,  m_yellowColorGraphID,  m_yellowSatDataRefGraphID,  m_yellowColorDataRefGraphID },
		{ m_showCyan,    m_cyanSatGraphID,    m_cyanColorGraphID,    m_cyanSatDataRefGraphID,    m_cyanColorDataRefGraphID },
		{ m_showMagenta, m_magentaSatGraphID, m_magentaColorGraphID, m_magentaSatDataRefGraphID, m_magentaColorDataRefGraphID },
	};
	bool bDataRef = ( m_showDataRef && pDataRef != NULL && pDataRef != pDoc );

	for ( int h = 0; h < 6; h++ )
	{
		m_graphCtrl.ClearGraph(hues[h].satID);
		m_graphCtrl2.ClearGraph(hues[h].colorID);
		m_graphCtrl.ClearGraph(hues[h].satRefID);
		m_graphCtrl2.ClearGraph(hues[h].colorRefID);
		if ( ! hues[h].bShow )
			continue;
		PlotHue ( pDoc, h, size, hues[h].satID, hues[h].colorID );
		if ( bDataRef )
			PlotHue ( pDataRef, h, size, hues[h].satRefID, hues[h].colorRefID );
	}
}

static CColor GetSatMeasure ( CMeasure * pMeasure, int hue, int i )
{
	switch ( hue )
	{
		case 0:	return pMeasure->GetRedSat(i);
		case 1:	return pMeasure->GetGreenSat(i);
		case 2:	return pMeasure->GetBlueSat(i);
		case 3:	return pMeasure->GetYellowSat(i);
		case 4:	return pMeasure->GetCyanSat(i);
		default: return pMeasure->GetMagentaSat(i);
	}
}

// Plot one hue's saturation sweep from pSrc (the document, or the comparison
// document): the saturation shift on m_graphCtrl and the dE on m_graphCtrl2.
// Each document is graded against its OWN references and whites.
//
// The references come from CMeasure::GetRefSat at the document's active
// stimulus level - the same call the measures grid and the 3D viewer make - and
// the dE normalisation from CMeasure::GetColorDENorm, the shared form of what
// the grid computes inline, so a patch reads the same dE here as in the grid.
// This view used to re-derive the reference itself, and that copy had drifted:
// it modelled P3/709-in-2020 patches in the inner space instead of the BT.2020
// transport they are sent in, which bends the hue under per-channel PQ tone
// mapping. The saturation axis is likewise this level's own 0% -> 100%
// reference: at reduced stimulus in PQ the 100% point's chromaticity moves, so
// a fixed full-stimulus endpoint showed a perfect patch as shifted.
void CSatLumShiftGrapher::PlotHue ( CDataSetDoc * pSrc, int hue, int size, long satID, long colorID )
{
	CMeasure * pMeasure = pSrc->GetMeasure();
	if ( satID == -1 || size <= 0 || ! GetSatMeasure ( pMeasure, hue, 0 ).isValid() )
		return;

	CColorHCFRConfig * cfg = GetConfig();
	bool bSpecial = ( cfg->m_colorStandard == HDTVa || cfg->m_colorStandard == HDTVb );
	const ColorDENorm norm = pMeasure->GetColorDENorm ( 5 + hue );
	// dE space and gray-world weight exactly as GetItemText's saturation branch
	CColorReference bRef = GetColorReference();
	if ( bRef.m_standard == UHDTV3 || bRef.m_standard == UHDTV4 )
		bRef = ContainerTransportReference ( bRef );
	else if ( bSpecial )
		bRef = CColorReference ( HDTV );
	int gw = ( cfg->m_GammaOffsetType == 5 ) ? 3 : cfg->gw_Weight;

	ColorxyY start = pMeasure->GetRefSat ( hue, 0.0, bSpecial ).GetxyYValue();
	ColorxyY end = pMeasure->GetRefSat ( hue, 1.0, bSpecial ).GetxyYValue();
	double xstart = start[0], ystart = start[1];
	double dx = end[0] - xstart;
	double dy = end[1] - ystart;

	for ( int i = 0; i < size; i++ )
	{
		CColor meas = GetSatMeasure ( pMeasure, hue, i );
		CColor ref = pMeasure->GetRefSat ( hue, (double) i / (double) ( size - 1 ), bSpecial );
		if ( ! meas.isValid() || ! ref.isValid() )
			continue;

		// Saturation shift: project the measured xy onto the 0% -> 100% line
		// and express its distance from the target along that line in percent
		ColorxyY m = meas.GetxyYValue();
		ColorxyY t = ref.GetxyYValue();
		double k = ( ( m[0] - xstart ) + ( ystart - m[1] ) * ( dx / dy ) ) / ( ( dx * dx / dy ) + dy );
		double xpoint = m[0] - k * dy;
		double satshift = ( xpoint - t[0] ) / dx * 100.0;

		ColorXYZ r = ref.GetXYZValue();
		ColorXYZ rScaled ( r[0] * norm.markScale, r[1] * norm.markScale, r[2] * norm.markScale );
		double deltaE = meas.GetDeltaE ( norm.whiteY, rScaled, norm.refWhite, bRef, cfg->m_dE_form, false, gw );

		m_graphCtrl.AddPoint(satID, i*100/(size-1), satshift);
		m_graphCtrl2.AddPoint(colorID, i*100/(size-1), deltaE);
	}
}

/////////////////////////////////////////////////////////////////////////////
// CSatLumShiftView

IMPLEMENT_DYNCREATE(CSatLumShiftView, CSavingView)

CSatLumShiftView::CSatLumShiftView()
	: CSavingView()
{
	//{{AFX_DATA_INIT(CSatLumShiftView)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}
 
CSatLumShiftView::~CSatLumShiftView()
{
}
 
BEGIN_MESSAGE_MAP(CSatLumShiftView, CSavingView)
	//{{AFX_MSG_MAP(CSatLumShiftView)
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_LUM_GRAPH_SHOWREF, OnSatGraphShowRef)
	ON_COMMAND(IDM_LUM_GRAPH_REDSHIFT, OnSatGraphRed)
	ON_COMMAND(IDM_LUM_GRAPH_GREENSHIFT, OnSatGraphGreen)
	ON_COMMAND(IDM_LUM_GRAPH_BLUESHIFT, OnSatGraphBlue)
	ON_COMMAND(IDM_LUM_GRAPH_YELLOWSHIFT, OnSatGraphYellow)
	ON_COMMAND(IDM_LUM_GRAPH_CYANSHIFT, OnSatGraphCyan)
	ON_COMMAND(IDM_LUM_GRAPH_MAGENTASHIFT, OnSatGraphMagenta)
	ON_COMMAND(IDM_LUM_GRAPH_DATAREF, OnSatGraphShowDataRef)
	ON_COMMAND(IDM_GRAPH_SETTINGS, OnGraphSettings)
	ON_COMMAND(IDM_GRAPH_SAVE, OnGraphSave)
	ON_COMMAND(IDM_GRAPH_SCALE_FIT, OnGraphScaleFit)
	ON_COMMAND(IDM_LUMINANCE_GRAPH_Y_SCALE1, OnLuminanceGraphYScale1)
	ON_COMMAND(IDM_LUMINANCE_GRAPH_Y_SCALE2, OnLuminanceGraphYScale2)
	ON_COMMAND(IDM_GRAPH_Y_SHIFT_BOTTOM, OnGraphYShiftBottom)
	ON_COMMAND(IDM_GRAPH_Y_SHIFT_TOP, OnGraphYShiftTop)
	ON_COMMAND(IDM_GRAPH_Y_ZOOM_IN, OnGraphYZoomIn)
	ON_COMMAND(IDM_GRAPH_Y_ZOOM_OUT, OnGraphYZoomOut)
	ON_COMMAND(IDM_GRAPH_Y_SCALE_FIT, OnGraphYScaleFit)
	ON_COMMAND(IDM_GRAPH_SCALE_CUSTOM, OnGraphScaleCustom)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SCALE1, OnDeltaEGraphYScale1)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SCALE2, OnDeltaEGraphYScale2)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SCALE_FIT, OnDeltaEGraphYScaleFit)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SHIFT_BOTTOM, OnDeltaEGraphYShiftBottom)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SHIFT_TOP, OnDeltaEGraphYShiftTop)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_ZOOM_IN, OnDeltaEGraphYZoomIn)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_ZOOM_OUT, OnDeltaEGraphYZoomOut)
	ON_COMMAND(IDM_DELTAE_GRAPH_SCALE_CUSTOM, OnDeltaEGraphScaleCustom)
	ON_COMMAND(IDM_HELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSatLumShiftView diagnostics

#ifdef _DEBUG
void CSatLumShiftView::AssertValid() const
{
	CSavingView::AssertValid();
}

void CSatLumShiftView::Dump(CDumpContext& dc) const
{
	CSavingView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CSatLumShiftView message handlers

void CSatLumShiftView::OnInitialUpdate() 
{
	CSavingView::OnInitialUpdate();

	CRect rect;
	GetClientRect(&rect);	// fill entire window

	m_Grapher.m_graphCtrl.Create(_T("Graph Window"), rect, this, IDC_LUMINANCEHISTO_GRAPH);
	m_Grapher.m_graphCtrl2.Create(_T("Graph Window"), rect, this, IDC_LUMINANCEHISTOLOG_GRAPH);

	OnSize(SIZE_RESTORED,rect.Width(),rect.Height());	// to size graph according to m_showDeltaE
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
	// Do nothing when not concerned
	switch ( lHint )
	{
//		case UPD_GRAYSCALE: //used as catchall for preferences change
		case UPD_NEARBLACK:
		case UPD_NEARWHITE:
		case UPD_CONTRAST:
		case UPD_FREEMEASURES:
		case UPD_GENERATORCONFIG:
		case UPD_SELECTEDCOLOR:
			 return;
	}

	m_Grapher.UpdateGraph ( GetDocument () );

	RedrawWindow( NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW );
}

DWORD CSatLumShiftView::GetUserInfo ()
{
	return	( ( m_Grapher.m_showReference	& 0x0001 )	<< 0 )
		  + ( ( m_Grapher.m_showRed			& 0x0001 )	<< 1 )
		  + ( ( m_Grapher.m_showGreen		& 0x0001 )	<< 2 )
		  + ( ( m_Grapher.m_showBlue		& 0x0001 )	<< 3 )
		  + ( ( m_Grapher.m_showYellow		& 0x0001 )	<< 4 )
		  + ( ( m_Grapher.m_showCyan		& 0x0001 )	<< 5 )
		  + ( ( m_Grapher.m_showMagenta		& 0x0001 )	<< 6 )
		  + ( ( m_Grapher.m_showDataRef		& 0x0001 )	<< 7 );
}

void CSatLumShiftView::SetUserInfo ( DWORD dwUserInfo )
{
	m_Grapher.m_showReference	= ( dwUserInfo >> 0 ) & 0x0001;
	m_Grapher.m_showRed			= ( dwUserInfo >> 1 ) & 0x0001;
	m_Grapher.m_showGreen		= ( dwUserInfo >> 2 ) & 0x0001;
	m_Grapher.m_showBlue		= ( dwUserInfo >> 3 ) & 0x0001;
	m_Grapher.m_showYellow		= ( dwUserInfo >> 4 ) & 0x0001;
	m_Grapher.m_showCyan		= ( dwUserInfo >> 5 ) & 0x0001;
	m_Grapher.m_showMagenta		= ( dwUserInfo >> 6 ) & 0x0001;
	m_Grapher.m_showDataRef		= ( dwUserInfo >> 7 ) & 0x0001;
}

void CSatLumShiftView::OnSize(UINT nType, int cx, int cy) 
{
	CSavingView::OnSize(nType, cx, cy);
	
	if(IsWindow(m_Grapher.m_graphCtrl.m_hWnd))
		m_Grapher.m_graphCtrl.MoveWindow(0,0,cx,cy/2);
	if(IsWindow(m_Grapher.m_graphCtrl2.m_hWnd))
		m_Grapher.m_graphCtrl2.MoveWindow(0,cy/2,cx,cy/2);
}

void CSatLumShiftView::OnDraw(CDC* pDC) 
{
	// TODO: Add your specialized code here and/or call the base class
}

BOOL CSatLumShiftView::OnEraseBkgnd(CDC* pDC) 
{
	return TRUE;
}


void CSatLumShiftView::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	// load and display popup menu
	CNewMenu menu;
	menu.LoadMenu(IDR_SATLUMSHIFT_GRAPH_MENU);
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup);
	
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_SHOWREF, m_Grapher.m_showReference ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
 	pPopup->CheckMenuItem(IDM_LUM_GRAPH_DATAREF, m_Grapher.m_showDataRef ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND); //Ki
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_REDSHIFT, m_Grapher.m_showRed ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_GREENSHIFT, m_Grapher.m_showGreen ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_BLUESHIFT, m_Grapher.m_showBlue ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_YELLOWSHIFT, m_Grapher.m_showYellow ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_CYANSHIFT, m_Grapher.m_showCyan ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_MAGENTASHIFT, m_Grapher.m_showMagenta ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);

	pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL, point.x, point.y, this);
}

void CSatLumShiftView::OnSatGraphShowRef() 
{
	m_Grapher.m_showReference = !m_Grapher.m_showReference;
	GetConfig()->WriteProfileInt("Saturation Shift","Show Reference",m_Grapher.m_showReference);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnSatGraphRed() 
{
	m_Grapher.m_showRed = !m_Grapher.m_showRed;
	GetConfig()->WriteProfileInt("Saturation Shift","Show Red",m_Grapher.m_showRed);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnSatGraphGreen() 
{
	m_Grapher.m_showGreen = !m_Grapher.m_showGreen;
	GetConfig()->WriteProfileInt("Saturation Shift","Show Green",m_Grapher.m_showGreen);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnSatGraphBlue() 
{
	m_Grapher.m_showBlue = !m_Grapher.m_showBlue;
	GetConfig()->WriteProfileInt("Saturation Shift","Show Blue",m_Grapher.m_showBlue);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnSatGraphYellow() 
{
	m_Grapher.m_showYellow = !m_Grapher.m_showYellow;
	GetConfig()->WriteProfileInt("Saturation Shift","Show Yellow",m_Grapher.m_showYellow);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnSatGraphCyan() 
{
	m_Grapher.m_showCyan = !m_Grapher.m_showCyan;
	GetConfig()->WriteProfileInt("Saturation Shift","Show Cyan",m_Grapher.m_showCyan);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnSatGraphMagenta() 
{
	m_Grapher.m_showMagenta = !m_Grapher.m_showMagenta;
	GetConfig()->WriteProfileInt("Saturation Shift","Show Magenta",m_Grapher.m_showMagenta);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnSatGraphShowDataRef()
{
	m_Grapher.m_showDataRef = !m_Grapher.m_showDataRef;
	GetConfig()->WriteProfileInt("Saturation Shift","Show Reference Data",m_Grapher.m_showDataRef);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnGraphSave() 
{
	m_Grapher.m_graphCtrl.SaveGraphs(&m_Grapher.m_graphCtrl2);
}

void CSatLumShiftView::OnGraphSettings() 
{
	// Add delta E graph to first graph control to allow setting change 
	int tmpGraphID1=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_redColorGraphID]);
	int tmpGraphID2=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_greenColorGraphID]);
	int tmpGraphID3=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_blueColorGraphID]);
	int tmpGraphID4=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_yellowColorGraphID]);
	int tmpGraphID5=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_cyanColorGraphID]);
	int tmpGraphID6=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_magentaColorGraphID]);
	int tmpGraphID7=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_redColorDataRefGraphID]);
	int tmpGraphID8=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_greenColorDataRefGraphID]);
	int tmpGraphID9=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_blueColorDataRefGraphID]);
	int tmpGraphID10=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_yellowColorDataRefGraphID]);
	int tmpGraphID11=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_cyanColorDataRefGraphID]);
	int tmpGraphID12=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_magentaColorDataRefGraphID]);

	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID1 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID2 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID3 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID4 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID5 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID6 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID7 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID8 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID9 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID10 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID11 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID12 );

	m_Grapher.m_graphCtrl.ChangeSettings();

	// Update m_Grapher.m_graphCtrl2 setting according to graph and values of tmpGraphIDs
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID1,m_Grapher.m_redColorGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID2,m_Grapher.m_greenColorGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID3,m_Grapher.m_blueColorGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID4,m_Grapher.m_yellowColorGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID5,m_Grapher.m_cyanColorGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID6,m_Grapher.m_magentaColorGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID7,m_Grapher.m_redColorDataRefGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID8,m_Grapher.m_greenColorDataRefGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID9,m_Grapher.m_blueColorDataRefGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID10,m_Grapher.m_yellowColorDataRefGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID11,m_Grapher.m_cyanColorDataRefGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID12,m_Grapher.m_magentaColorDataRefGraphID);

	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID12);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID11);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID10);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID9);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID8);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID7);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID6);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID5);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID4);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID3);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID2);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID1);

	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnGraphScaleFit() 
{
	m_Grapher.m_graphCtrl.FitXScale(TRUE);
	m_Grapher.m_graphCtrl.SetYAxisProps("%", 5, -100, 100);
	m_Grapher.m_graphCtrl.FitYScale(TRUE,10);
	OnDeltaEGraphYScaleFit();
	m_Grapher.m_graphCtrl.WriteSettings("Saturation Shift Sat");
	m_Grapher.m_graphCtrl2.WriteSettings("Saturation Shift Color");
}

void CSatLumShiftView::OnLuminanceGraphYScale1() 
{
	m_Grapher.m_graphCtrl.SetYScale(-100,100);
	m_Grapher.m_graphCtrl.SetYAxisProps("%", 20, -100, 100);
	m_Grapher.m_graphCtrl.WriteSettings("Saturation Shift Sat");
	m_Grapher.m_graphCtrl2.WriteSettings("Saturation Shift Color");
	Invalidate(FALSE);
}

void CSatLumShiftView::OnLuminanceGraphYScale2() 
{
	m_Grapher.m_graphCtrl.SetYScale(-20,20);
	m_Grapher.m_graphCtrl.SetYAxisProps("%", 5, -100, 100);
	m_Grapher.m_graphCtrl.WriteSettings("Saturation Shift Sat");
	m_Grapher.m_graphCtrl2.WriteSettings("Saturation Shift Color");
	Invalidate(FALSE);
}

void CSatLumShiftView::OnGraphYScaleFit() 
{
	m_Grapher.m_graphCtrl.FitYScale(TRUE);
	m_Grapher.m_graphCtrl.WriteSettings("Saturation Shift Sat");
	m_Grapher.m_graphCtrl2.WriteSettings("Saturation Shift Color");
	Invalidate(FALSE);
}

void CSatLumShiftView::OnGraphYShiftBottom() 
{
	m_Grapher.m_graphCtrl.ShiftYScale(10);
	Invalidate(FALSE);
}

void CSatLumShiftView::OnGraphYShiftTop() 
{
	m_Grapher.m_graphCtrl.ShiftYScale(-10);
	Invalidate(FALSE);
}

void CSatLumShiftView::OnGraphYZoomIn() 
{
	m_Grapher.m_graphCtrl.GrowYScale(0,-10);
	Invalidate(FALSE);
}

void CSatLumShiftView::OnGraphYZoomOut() 
{
	m_Grapher.m_graphCtrl.GrowYScale(0,10);
	Invalidate(FALSE);
}

void CSatLumShiftView::OnGraphScaleCustom() 
{
	m_Grapher.m_graphCtrl.ChangeScale();
	m_Grapher.m_graphCtrl.WriteSettings("Saturation Shift Sat");
	m_Grapher.m_graphCtrl2.WriteSettings("Saturation Shift Color");
}

void CSatLumShiftView::OnDeltaEGraphYScale1() 
{
	m_Grapher.m_graphCtrl2.SetYScale(0,20);
	m_Grapher.m_graphCtrl2.SetYAxisProps("", 2, 0, 40);
	m_Grapher.m_graphCtrl.WriteSettings("Saturation Shift Sat");
	m_Grapher.m_graphCtrl2.WriteSettings("Saturation Shift Color");
	Invalidate(FALSE);
}

void CSatLumShiftView::OnDeltaEGraphYScale2() 
{
	m_Grapher.m_graphCtrl2.SetYScale(0,5);
	m_Grapher.m_graphCtrl2.SetYAxisProps("", 1, 0, 40);
	m_Grapher.m_graphCtrl.WriteSettings("Saturation Shift Sat");
	m_Grapher.m_graphCtrl2.WriteSettings("Saturation Shift Color");
	Invalidate(FALSE);
}

void CSatLumShiftView::OnDeltaEGraphYScaleFit() 
{
	m_Grapher.m_graphCtrl2.FitYScale(TRUE,1);
	m_Grapher.m_graphCtrl2.m_yAxisStep=(m_Grapher.m_graphCtrl2.m_maxY-m_Grapher.m_graphCtrl2.m_minY)/10;
	m_Grapher.m_graphCtrl.WriteSettings("Saturation Shift Sat");
	m_Grapher.m_graphCtrl2.WriteSettings("Saturation Shift Color");
	Invalidate(FALSE);
}

void CSatLumShiftView::OnDeltaEGraphYShiftBottom() 
{
	m_Grapher.m_graphCtrl2.ShiftYScale(1);
	Invalidate(FALSE);
}

void CSatLumShiftView::OnDeltaEGraphYShiftTop() 
{
	m_Grapher.m_graphCtrl2.ShiftYScale(-1);
	Invalidate(FALSE);
}

void CSatLumShiftView::OnDeltaEGraphYZoomIn() 
{
	m_Grapher.m_graphCtrl2.GrowYScale(0,-4);
	Invalidate(FALSE);
}

void CSatLumShiftView::OnDeltaEGraphYZoomOut() 
{
	m_Grapher.m_graphCtrl2.GrowYScale(0,+4);
	Invalidate(FALSE);
}

void CSatLumShiftView::OnDeltaEGraphScaleCustom() 
{
	m_Grapher.m_graphCtrl2.ChangeScale();
	m_Grapher.m_graphCtrl.WriteSettings("Saturation Shift Sat");
	m_Grapher.m_graphCtrl2.WriteSettings("Saturation Shift Color");
}

void CSatLumShiftView::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_SATSHIFT, NULL );
}

