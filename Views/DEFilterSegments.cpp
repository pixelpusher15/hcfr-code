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

// DEFilterSegments.cpp : implementation of the info-pane dE filter segments.

#include "stdafx.h"
#include "ColorHCFR.h"
#include "DEFilterSegments.h"
#include "GdiPlusAA.h"
#include "fxcolor.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

BEGIN_MESSAGE_MAP(CDEFilterSegments, CWnd)
	//{{AFX_MSG_MAP(CDEFilterSegments)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CDEFilterSegments::CDEFilterSegments()
	: m_sel(0)
{
}

BOOL CDEFilterSegments::Create(const CRect & rc, CWnd * pParent, UINT nID)
{
	return CWnd::Create( AfxRegisterWndClass( 0, ::LoadCursor( NULL, IDC_ARROW ) ),
						 NULL, WS_CHILD, rc, pParent, nID );
}

void CDEFilterSegments::SetSel(int sel)
{
	if ( sel < 0 ) sel = 0;
	if ( sel > 2 ) sel = 2;
	if ( sel == m_sel )
		return;
	m_sel = sel;
	if ( ::IsWindow( m_hWnd ) )
		Invalidate( FALSE );
}

int CDEFilterSegments::SegmentFromPoint(CPoint pt) const
{
	CRect rc;
	GetClientRect( &rc );
	if ( rc.Width() <= 0 )
		return 0;
	int seg = pt.x * 3 / rc.Width();
	return seg < 0 ? 0 : ( seg > 2 ? 2 : seg );
}

BOOL CDEFilterSegments::OnEraseBkgnd(CDC* /*pDC*/)
{
	return TRUE;   // OnPaint covers the whole client area
}

void CDEFilterSegments::OnPaint()
{
	CPaintDC dc( this );
	EnsureGdiplus();

	CRect rc;
	GetClientRect( &rc );
	int w = rc.Width(), h = rc.Height();
	if ( w <= 0 || h <= 0 )
		return;

	const bool dark = ( fxUseCustomColor != FALSE );
	Gdiplus::Color face   = dark ? Gdiplus::Color( 60, 60, 62 )    : Gdiplus::Color( 255, 255, 255 );
	Gdiplus::Color active = dark ? Gdiplus::Color( 96, 96, 100 )   : Gdiplus::Color( 224, 232, 244 );
	Gdiplus::Color border = dark ? Gdiplus::Color( 95, 95, 98 )    : Gdiplus::Color( 210, 210, 210 );
	Gdiplus::Color text   = dark ? Gdiplus::Color( 230, 230, 234 ) : Gdiplus::Color( 32, 32, 36 );
	Gdiplus::Color dimtxt = dark ? Gdiplus::Color( 185, 185, 190 ) : Gdiplus::Color( 96, 96, 104 );

	Gdiplus::Bitmap bmp( w, h );
	Gdiplus::Graphics g( &bmp );
	g.SetTextRenderingHint( Gdiplus::TextRenderingHintAntiAlias );

	Gdiplus::SolidBrush faceBrush( face );
	g.FillRectangle( &faceBrush, 0, 0, w, h );

	// segment labels: "Show all" | "Hide < good dE" | "Hide < warn dE", with the
	// thresholds read live from the tolerance preset in Advanced settings
	double good = 2.0, warn = 3.0;
	GetConfig()->GetDEThresholds( good, warn );
	CString sAll, sFmt, seg1, seg2, num;
	sAll.LoadString ( IDS_3DVIEW_FILTER_ALL );
	sFmt.LoadString ( IDS_3DVIEW_FILTER_HIDE );
	num.Format ( "%.3g", good );
	seg1.Format ( sFmt, (LPCTSTR)num );
	num.Format ( "%.3g", warn );
	seg2.Format ( sFmt, (LPCTSTR)num );
	const CString * labels[3] = { &sAll, &seg1, &seg2 };

	Gdiplus::Font font( L"Segoe UI", 8.5f );
	Gdiplus::StringFormat sf;
	sf.SetAlignment( Gdiplus::StringAlignmentCenter );
	sf.SetLineAlignment( Gdiplus::StringAlignmentCenter );

	for ( int s = 0; s < 3; s++ )
	{
		float x0 = (float)( s * w ) / 3.0f;
		float x1 = (float)( ( s + 1 ) * w ) / 3.0f;
		if ( s == m_sel )
		{
			Gdiplus::SolidBrush selBrush( active );
			g.FillRectangle( &selBrush, x0, 0.0f, x1 - x0, (float)h );
		}
		CStringW wide( *labels[s] );
		Gdiplus::SolidBrush textBrush( s == m_sel ? text : dimtxt );
		Gdiplus::RectF cell( x0, 0.0f, x1 - x0, (float)h );
		g.DrawString( wide, -1, &font, cell, &sf, &textBrush );
	}

	// separators + outer border
	Gdiplus::Pen borderPen( border, 1.0f );
	g.DrawLine( &borderPen, w / 3, 1, w / 3, h - 2 );
	g.DrawLine( &borderPen, 2 * w / 3, 1, 2 * w / 3, h - 2 );
	g.DrawRectangle( &borderPen, 0, 0, w - 1, h - 1 );

	Gdiplus::Graphics screen( dc.GetSafeHdc() );
	screen.DrawImage( &bmp, 0, 0 );
}

void CDEFilterSegments::OnLButtonDown(UINT nFlags, CPoint point)
{
	int seg = SegmentFromPoint( point );
	if ( seg != m_sel )
	{
		m_sel = seg;
		Invalidate( FALSE );
		GetParent()->SendMessage( WM_COMMAND,
								  MAKEWPARAM( GetDlgCtrlID(), BN_CLICKED ),
								  (LPARAM)m_hWnd );
	}
	CWnd::OnLButtonDown( nFlags, point );
}
