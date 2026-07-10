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

// ProfilePane.cpp : implementation of the display-profile capture pane.

#include "stdafx.h"
#include "ColorHCFR.h"
#include "DataSetDoc.h"
#include "Measure.h"
#include "ProfilePane.h"
#include "fxcolor.h"
#include <uxtheme.h>
#include <math.h>
#include <algorithm>

#pragma comment(lib, "uxtheme.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Cube presets: dyadically nested 5/9/17 plus the odd extras; every N is odd so
// the 50% point exists on each axis. 21^3 with extras stays under
// MAX_USER_CC_PATCH_SIZE (10000).
static const struct { int cubeN; const char * name; } kPresets[5] =
{
	{ 5,  "Quick"     },
	{ 9,  "Standard"  },
	{ 11, "Fine"      },
	{ 17, "Reference" },
	{ 21, "Maximum"   },
};

static const char * kRegionNames[4] = { "Gray axis", "Near black", "Low saturation", "High saturation" };

// child-control ids (checkboxes)
#define IDC_PP_GRAYEXTRAS	1
#define IDC_PP_DRIFTCOMP	2


BEGIN_MESSAGE_MAP(CProfilePane, CWnd)
	//{{AFX_MSG_MAP(CProfilePane)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_MESSAGE(WM_MOUSELEAVE, OnMouseLeave)
	ON_WM_SIZE()
	ON_WM_CTLCOLOR()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CProfilePane::CProfilePane()
	: m_pDoc(NULL)
	, m_state(PS_SETUP)
	, m_pendingAction(PA_NONE)
	, m_inspectIdx(-1)
	, m_paused(FALSE)
	, m_lastPatchTick(0)
	, m_emaPatchSecs(0.0)
	, m_lastSeenIndex(-1)
	, m_runSumDE(0.0)
	, m_runMaxDE(0.0)
	, m_runDECount(0)
	, m_statsValid(false)
	, m_hot(HOT_NONE)
	, m_trackingMouse(false)
{
	m_preset = GetConfig()->GetProfileInt("MainView", "Profile Preset", 1);
	if ( m_preset < 0 || m_preset > 4 ) m_preset = 1;
	m_grayExtras = GetConfig()->GetProfileInt("MainView", "Profile GrayExtras", 1);
	m_driftComp = GetConfig()->GetProfileInt("MainView", "Profile DriftComp", 1);
}

BOOL CProfilePane::Create(const CRect & rc, CWnd * pParent, UINT nID)
{
	if ( !CWnd::Create( AfxRegisterWndClass( 0, ::LoadCursor( NULL, IDC_ARROW ) ),
						NULL, WS_CHILD | WS_CLIPCHILDREN, rc, pParent, nID ) )
		return FALSE;

	// standard Windows checkboxes as real child controls
	CRect rcInit( 0, 0, 10, 10 );
	m_chkGrayExtras.Create( "Extra gray-axis and near-black samples",
							WS_CHILD | BS_AUTOCHECKBOX, rcInit, this, IDC_PP_GRAYEXTRAS );
	m_chkDriftComp.Create( "Drift compensation (re-measures white every 64 patches)",
						   WS_CHILD | BS_AUTOCHECKBOX, rcInit, this, IDC_PP_DRIFTCOMP );
	CFont * pFont = pParent->GetFont();
	if ( pFont )
	{
		m_chkGrayExtras.SetFont( pFont );
		m_chkDriftComp.SetFont( pFont );
	}
	if ( fxUseCustomColor )
	{
		// dark-mode visuals for the themed check glyph
		SetWindowTheme( m_chkGrayExtras.GetSafeHwnd(), L"DarkMode_Explorer", NULL );
		SetWindowTheme( m_chkDriftComp.GetSafeHwnd(), L"DarkMode_Explorer", NULL );
	}
	m_chkGrayExtras.SetCheck( m_grayExtras ? BST_CHECKED : BST_UNCHECKED );
	m_chkDriftComp.SetCheck( m_driftComp ? BST_CHECKED : BST_UNCHECKED );
	return TRUE;
}

CMeasure * CProfilePane::Measure() const
{
	return m_pDoc ? m_pDoc->GetMeasure() : NULL;
}

int CProfilePane::GetCubeSize() const
{
	return kPresets[m_preset].cubeN;
}

int CProfilePane::PatchCountFor(int preset) const
{
	int n = GenerateProfileColors ( NULL, 0, kPresets[preset].cubeN, m_grayExtras != FALSE );
	return ( n > 0 ) ? n : 0;
}

double CProfilePane::EstimateSeconds(int patches) const
{
	// per-patch seconds learned from the last capture (registry, milliseconds)
	double perPatch = GetConfig()->GetProfileInt("MainView", "Profile PatchMs", 1800) / 1000.0;
	if ( perPatch < 0.2 ) perPatch = 0.2;
	int anchors = m_driftComp ? ( patches / 64 + 2 ) : 0;
	return ( patches + anchors ) * perPatch;
}

void CProfilePane::SyncChildren()
{
	if ( !m_chkGrayExtras.GetSafeHwnd() )
		return;
	bool show = ( m_state == PS_SETUP ) && IsWindowVisible();
	m_chkGrayExtras.ShowWindow( show ? SW_SHOW : SW_HIDE );
	m_chkDriftComp.ShowWindow( show ? SW_SHOW : SW_HIDE );
	if ( show )
	{
		CRect rc;
		GetClientRect( &rc );
		int mgn = GetConfig()->Scale( 12 );
		int cardBottom = mgn + GetConfig()->Scale( 30 ) + GetConfig()->Scale( 84 );
		int h = GetConfig()->Scale( 20 );
		int w = GetConfig()->Scale( 330 );
		m_chkGrayExtras.MoveWindow( mgn, cardBottom + GetConfig()->Scale( 12 ), w, h );
		m_chkDriftComp.MoveWindow( mgn, cardBottom + GetConfig()->Scale( 12 ) + h + GetConfig()->Scale( 8 ),
								   GetConfig()->Scale( 420 ), h );
		m_chkGrayExtras.SetCheck( m_grayExtras ? BST_CHECKED : BST_UNCHECKED );
		m_chkDriftComp.SetCheck( m_driftComp ? BST_CHECKED : BST_UNCHECKED );
	}
}

void CProfilePane::RefreshState()
{
	if ( m_state == PS_RUNNING )
		return;		// capture in flight; LeaveRunning decides the landing state
	CMeasure * pMeasure = Measure();
	m_state = ( pMeasure && pMeasure->HasProfileMeasures() ) ? PS_SUMMARY : PS_SETUP;
	InvalidateStats();
	SyncChildren();
	if ( ::IsWindow( m_hWnd ) )
		Invalidate( FALSE );
}

void CProfilePane::EnterRunning()
{
	m_state = PS_RUNNING;
	m_paused = FALSE;
	m_lastPatchTick = GetTickCount();
	m_emaPatchSecs = GetConfig()->GetProfileInt("MainView", "Profile PatchMs", 1800) / 1000.0;
	m_lastSeenIndex = -1;
	m_runSumDE = 0.0;
	m_runMaxDE = 0.0;
	m_runDECount = 0;
	SyncChildren();
	if ( ::IsWindow( m_hWnd ) )
	{
		Invalidate( FALSE );
		UpdateWindow();		// the measure loop blocks; paint the running state now
	}
}

void CProfilePane::LeaveRunning()
{
	m_state = PS_SETUP;	// RefreshState below lands on SUMMARY when data exists
	// remember the learned patch pace for future time estimates
	if ( m_emaPatchSecs > 0.0 )
		GetConfig()->WriteProfileInt("MainView", "Profile PatchMs", (int)( m_emaPatchSecs * 1000.0 ));
	m_paused = FALSE;
	RefreshState();
}

void CProfilePane::SetPaused(BOOL b)
{
	m_paused = b;
	if ( ::IsWindow( m_hWnd ) )
	{
		Invalidate( FALSE );
		UpdateWindow();
	}
}

double CProfilePane::WhiteYForDE() const
{
	CMeasure * pMeasure = Measure();
	if ( !pMeasure )
		return 0.0;
	// mirror the grid / 3D-view convention: prime white if valid, else on/off white
	CColor w = pMeasure->GetPrimeWhite();
	if ( !w.isValid() )
		w = pMeasure->GetOnOffWhite();
	return w.isValid() ? w.GetY() : 0.0;
}

double CProfilePane::PatchDE(int i) const
{
	CMeasure * pMeasure = Measure();
	if ( !pMeasure )
		return -1.0;
	CColor c = pMeasure->GetProfileMeasure( i );
	if ( !c.isValid() )
		return -1.0;
	ColorXYZ xyz = c.GetXYZValue();
	double ywForDE = WhiteYForDE();
	if ( ( xyz[0] + xyz[1] + xyz[2] ) < 1e-6 || ywForDE <= 0.0 )
		return -1.0;	// blackish: no defined chromaticity, dE meaningless
	CColor refC;
	pMeasure->GetRefProfileSat( i, refC );
	if ( !refC.isValid() )
		return -1.0;
	int gw = ( GetConfig()->m_GammaOffsetType == 5 ) ? 3 : GetConfig()->gw_Weight;
	double dE = c.GetDeltaE( ywForDE, refC, 1.0, GetColorReference(), GetConfig()->m_dE_form, false, gw );
	if ( !( dE == dE ) || dE < 0.0 )
		return -1.0;
	if ( dE > 1.0e6 )	// non-finite / absurd: poisons stats and histogram binning
	{
		return -1.0;
	}
	return dE;
}

void CProfilePane::OnCaptureProgress()
{
	CMeasure * pMeasure = Measure();
	if ( !pMeasure || m_state != PS_RUNNING )
		return;

	int cur = pMeasure->m_currentIndex;
	if ( cur != m_lastSeenIndex )
	{
		DWORD now = GetTickCount();
		if ( m_lastSeenIndex >= 0 && cur > m_lastSeenIndex && !m_paused )
		{
			double secs = ( now - m_lastPatchTick ) / 1000.0 / ( cur - m_lastSeenIndex );
			m_emaPatchSecs = ( m_emaPatchSecs <= 0.0 ) ? secs : ( 0.8 * m_emaPatchSecs + 0.2 * secs );
		}
		m_lastPatchTick = now;

		// fold the dE of every patch completed since the last poke into the
		// running stats (incremental: never rescan the whole array per paint)
		for ( int i = ( m_lastSeenIndex < 0 ? 0 : m_lastSeenIndex ); i < cur && i < pMeasure->GetProfileMeasureSize(); i++ )
		{
			double dE = PatchDE( i );
			if ( dE >= 0.0 )
			{
				m_runSumDE += dE;
				if ( dE > m_runMaxDE ) m_runMaxDE = dE;
				m_runDECount++;
			}
		}
		m_lastSeenIndex = cur;
	}

	if ( ::IsWindow( m_hWnd ) )
	{
		Invalidate( FALSE );
		UpdateWindow();		// the measure loop blocks between pumps; show progress now
	}
}

void CProfilePane::SendAction(Action a, int inspectIdx)
{
	m_pendingAction = a;
	m_inspectIdx = inspectIdx;
	GetParent()->SendMessage( WM_COMMAND,
							  MAKEWPARAM( GetDlgCtrlID(), BN_CLICKED ),
							  (LPARAM)m_hWnd );
}

BOOL CProfilePane::OnEraseBkgnd(CDC* /*pDC*/)
{
	return TRUE;
}

BOOL CProfilePane::OnCommand(WPARAM wParam, LPARAM lParam)
{
	if ( HIWORD(wParam) == BN_CLICKED )
	{
		switch ( LOWORD(wParam) )
		{
			case IDC_PP_GRAYEXTRAS:
				m_grayExtras = ( m_chkGrayExtras.GetCheck() == BST_CHECKED );
				GetConfig()->WriteProfileInt("MainView", "Profile GrayExtras", m_grayExtras);
				Invalidate( FALSE );	// patch counts / estimates change
				return TRUE;

			case IDC_PP_DRIFTCOMP:
				m_driftComp = ( m_chkDriftComp.GetCheck() == BST_CHECKED );
				GetConfig()->WriteProfileInt("MainView", "Profile DriftComp", m_driftComp);
				Invalidate( FALSE );
				return TRUE;
		}
	}
	return CWnd::OnCommand( wParam, lParam );
}

HBRUSH CProfilePane::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	// checkbox labels sit on the pane's custom background
	static CBrush s_darkBrush( RGB(48,48,50) );
	static CBrush s_lightBrush( RGB(255,255,255) );
	const bool dark = ( fxUseCustomColor != FALSE );
	pDC->SetBkMode( TRANSPARENT );
	pDC->SetTextColor( dark ? RGB(235,235,240) : RGB(28,28,32) );
	return dark ? (HBRUSH)s_darkBrush.GetSafeHandle() : (HBRUSH)s_lightBrush.GetSafeHandle();
}

void CProfilePane::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize( nType, cx, cy );
	SyncChildren();
}

// ---------------------------------------------------------------------------
// painting

static Gdiplus::Color Mix(const Gdiplus::Color & a, const Gdiplus::Color & b, double t)
{
	return Gdiplus::Color(
		(BYTE)( a.GetR() + ( b.GetR() - a.GetR() ) * t ),
		(BYTE)( a.GetG() + ( b.GetG() - a.GetG() ) * t ),
		(BYTE)( a.GetB() + ( b.GetB() - a.GetB() ) * t ) );
}

// green under good, yellow-to-orange between good and warn, red above --
// anchored to the same shared thresholds the grid and the 3D heatmap use
static Gdiplus::Color DEColor(double dE, double good, double warn)
{
	if ( dE < good )
		return Gdiplus::Color( 106, 168, 79 );
	if ( dE < warn )
		return Mix( Gdiplus::Color( 241, 167, 60 ), Gdiplus::Color( 224, 106, 62 ),
					( warn > good ) ? ( dE - good ) / ( warn - good ) : 0.0 );
	return Gdiplus::Color( 229, 87, 86 );
}

// approximate display swatch for a stimulus percent triplet
static Gdiplus::Color SwatchColor(const ColorRGBDisplay & rgb)
{
	double r = pow( max( rgb[0], 0.0 ) / 100.0, 1.0 / 2.2 );
	double g = pow( max( rgb[1], 0.0 ) / 100.0, 1.0 / 2.2 );
	double b = pow( max( rgb[2], 0.0 ) / 100.0, 1.0 / 2.2 );
	return Gdiplus::Color( (BYTE)( min( r, 1.0 ) * 255.0 ),
						   (BYTE)( min( g, 1.0 ) * 255.0 ),
						   (BYTE)( min( b, 1.0 ) * 255.0 ) );
}

static CString FormatDuration(double secs)
{
	CString s;
	if ( secs < 3600.0 )
		s.Format( "~%d min", max( 1, (int)( secs / 60.0 + 0.5 ) ) );
	else
		s.Format( "~%.1f hr", secs / 3600.0 );
	return s;
}

struct SPaneTheme
{
	Gdiplus::Color face, card, cardHot, cardSel, cardSelHot, border, borderSel,
					text, dimtxt, accent, btnFace, btnFaceHot, btnText, danger, track;
};

static SPaneTheme PaneTheme(bool dark)
{
	SPaneTheme t;
	if ( dark )
	{
		t.face       = Gdiplus::Color( 48, 48, 50 );
		t.card       = Gdiplus::Color( 62, 62, 65 );
		t.cardHot    = Gdiplus::Color( 74, 74, 78 );
		t.cardSel    = Gdiplus::Color( 66, 80, 104 );
		t.cardSelHot = Gdiplus::Color( 74, 90, 116 );
		t.border     = Gdiplus::Color( 104, 104, 108 );
		t.borderSel  = Gdiplus::Color( 130, 168, 232 );
		t.text       = Gdiplus::Color( 240, 240, 244 );
		t.dimtxt     = Gdiplus::Color( 198, 198, 205 );
		t.accent     = Gdiplus::Color( 120, 158, 224 );
		t.btnFace    = Gdiplus::Color( 76, 110, 170 );
		t.btnFaceHot = Gdiplus::Color( 92, 128, 192 );
		t.btnText    = Gdiplus::Color( 244, 248, 252 );
		t.danger     = Gdiplus::Color( 232, 112, 110 );
		t.track      = Gdiplus::Color( 62, 62, 65 );
	}
	else
	{
		t.face       = Gdiplus::Color( 255, 255, 255 );
		t.card       = Gdiplus::Color( 246, 246, 248 );
		t.cardHot    = Gdiplus::Color( 238, 238, 242 );
		t.cardSel    = Gdiplus::Color( 226, 236, 250 );
		t.cardSelHot = Gdiplus::Color( 216, 229, 248 );
		t.border     = Gdiplus::Color( 200, 200, 205 );
		t.borderSel  = Gdiplus::Color( 56, 108, 190 );
		t.text       = Gdiplus::Color( 28, 28, 32 );
		t.dimtxt     = Gdiplus::Color( 90, 90, 98 );
		t.accent     = Gdiplus::Color( 52, 104, 186 );
		t.btnFace    = Gdiplus::Color( 56, 106, 182 );
		t.btnFaceHot = Gdiplus::Color( 72, 122, 198 );
		t.btnText    = Gdiplus::Color( 250, 252, 255 );
		t.danger     = Gdiplus::Color( 186, 58, 58 );
		t.track      = Gdiplus::Color( 236, 236, 240 );
	}
	return t;
}

static void RoundPath(Gdiplus::GraphicsPath & path, const CRect & rc, int r)
{
	int d = r * 2;
	path.Reset();
	path.AddArc( rc.left, rc.top, d, d, 180.0f, 90.0f );
	path.AddArc( rc.right - d - 1, rc.top, d, d, 270.0f, 90.0f );
	path.AddArc( rc.right - d - 1, rc.bottom - d - 1, d, d, 0.0f, 90.0f );
	path.AddArc( rc.left, rc.bottom - d - 1, d, d, 90.0f, 90.0f );
	path.CloseFigure();
}

static void FillRound(Gdiplus::Graphics & g, const CRect & rc, int r, const Gdiplus::Color & fill)
{
	Gdiplus::GraphicsPath path;
	RoundPath( path, rc, r );
	Gdiplus::SolidBrush br( fill );
	g.FillPath( &br, &path );
}

static void DrawRound(Gdiplus::Graphics & g, const CRect & rc, int r, const Gdiplus::Color & clr, float width)
{
	Gdiplus::GraphicsPath path;
	RoundPath( path, rc, r );
	Gdiplus::Pen pen( clr, width );
	g.DrawPath( &pen, &path );
}

static void DrawStr(Gdiplus::Graphics & g, const CString & s, Gdiplus::Font & f,
					const Gdiplus::RectF & rc, const Gdiplus::Color & clr,
					Gdiplus::StringAlignment ha = Gdiplus::StringAlignmentNear,
					Gdiplus::StringAlignment va = Gdiplus::StringAlignmentNear)
{
	Gdiplus::SolidBrush br( clr );
	Gdiplus::StringFormat sf;
	sf.SetAlignment( ha );
	sf.SetLineAlignment( va );
	sf.SetFormatFlags( Gdiplus::StringFormatFlagsNoWrap );
	sf.SetTrimming( Gdiplus::StringTrimmingEllipsisCharacter );
	CStringW wide( s );
	g.DrawString( wide, -1, &f, rc, &sf, &br );
}

static void DrawButton(Gdiplus::Graphics & g, const CRect & rc, const CString & label,
					   Gdiplus::Font & f, const Gdiplus::Color & face, const Gdiplus::Color & txt,
					   const Gdiplus::Color & border)
{
	FillRound( g, rc, 5, face );
	DrawRound( g, rc, 5, border, 1.0f );
	DrawStr( g, label, f, Gdiplus::RectF( (float)rc.left, (float)rc.top, (float)rc.Width(), (float)rc.Height() - 1 ),
			 txt, Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter );
}

void CProfilePane::OnPaint()
{
	CPaintDC dc( this );
	EnsureGdiplus();

	CRect rc;
	GetClientRect( &rc );
	if ( rc.Width() <= 0 || rc.Height() <= 0 )
		return;

	const bool dark = ( fxUseCustomColor != FALSE );
	Gdiplus::Bitmap bmp( rc.Width(), rc.Height() );
	Gdiplus::Graphics g( &bmp );
	g.SetTextRenderingHint( Gdiplus::TextRenderingHintClearTypeGridFit );
	g.SetSmoothingMode( Gdiplus::SmoothingModeAntiAlias );

	SPaneTheme t = PaneTheme( dark );
	Gdiplus::SolidBrush faceBrush( t.face );
	g.FillRectangle( &faceBrush, 0, 0, rc.Width(), rc.Height() );

	// reset hit rects; the active state's painter rebuilds its own
	int i;
	for ( i = 0; i < 5; i++ ) m_rcPresets[i].SetRectEmpty();
	m_rcStart.SetRectEmpty(); m_rcPause.SetRectEmpty();
	m_rcNewProfile.SetRectEmpty();
	m_rcWorstRows.clear();

	switch ( m_state )
	{
		case PS_SETUP:   PaintSetup( g, rc, dark );   break;
		case PS_RUNNING: PaintRunning( g, rc, dark ); break;
		case PS_SUMMARY: PaintSummary( g, rc, dark ); break;
	}

	Gdiplus::Graphics screen( dc.GetSafeHdc() );
	screen.DrawImage( &bmp, 0, 0 );
}

void CProfilePane::PaintSetup(Gdiplus::Graphics & g, const CRect & rc, bool dark)
{
	SPaneTheme t = PaneTheme( dark );
	int mgn = GetConfig()->Scale( 12 );
	Gdiplus::Font fTitle( L"Segoe UI", 11.0f, Gdiplus::FontStyleBold );
	Gdiplus::Font fBody( L"Segoe UI", 10.0f );
	Gdiplus::Font fSmall( L"Segoe UI", 9.0f );

	CMeasure * pMeasure = Measure();
	bool hasOld = ( pMeasure && pMeasure->HasProfileMeasures() );

	int y = mgn;
	DrawStr( g, "Measure an RGB cube grid to characterize this display", fBody,
			 Gdiplus::RectF( (float)mgn, (float)y, (float)( rc.Width() - 2 * mgn ), 22.0f ), t.dimtxt );
	y += GetConfig()->Scale( 30 );

	// preset cards
	int cardH = GetConfig()->Scale( 84 );
	int cardW = ( rc.Width() - mgn * 6 ) / 5;
	int x = mgn;
	for ( int p = 0; p < 5; p++ )
	{
		CRect rcCard( x, y, x + cardW, y + cardH );
		m_rcPresets[p] = rcCard;
		bool sel = ( p == m_preset );
		bool hot = ( m_hot == p );
		FillRound( g, rcCard, 6, sel ? ( hot ? t.cardSelHot : t.cardSel )
									 : ( hot ? t.cardHot : t.card ) );
		DrawRound( g, rcCard, 6, sel ? t.borderSel : t.border, sel ? 2.0f : 1.0f );

		int patches = PatchCountFor( p );
		CString line1, line2, line3;
		line1 = kPresets[p].name;
		line2.Format( "%dx%dx%d cube", kPresets[p].cubeN, kPresets[p].cubeN, kPresets[p].cubeN );
		line3.Format( "%d patches - %s", patches, (LPCTSTR)FormatDuration( EstimateSeconds( patches ) ) );
		float cy = (float)rcCard.top + 8.0f;
		DrawStr( g, line1, fTitle, Gdiplus::RectF( (float)rcCard.left + 10, cy, (float)rcCard.Width() - 20, 24.0f ), t.text );
		cy += 26.0f;
		DrawStr( g, line2, fBody, Gdiplus::RectF( (float)rcCard.left + 10, cy, (float)rcCard.Width() - 20, 20.0f ), t.dimtxt );
		cy += 21.0f;
		DrawStr( g, line3, fSmall, Gdiplus::RectF( (float)rcCard.left + 10, cy, (float)rcCard.Width() - 20, 18.0f ), t.dimtxt );
		x += cardW + mgn;
	}
	y += cardH + mgn;

	// the two option checkboxes are real child controls placed by SyncChildren;
	// reserve their vertical band here
	int optH = GetConfig()->Scale( 12 ) + 2 * GetConfig()->Scale( 20 ) + GetConfig()->Scale( 8 );
	int rowBottom = y + optH;

	// Start button, right-aligned on the options band
	int btnW = GetConfig()->Scale( 130 ), btnH = GetConfig()->Scale( 32 );
	m_rcStart = CRect( rc.Width() - mgn - btnW, rowBottom - btnH, rc.Width() - mgn, rowBottom );
	DrawButton( g, m_rcStart, "Start profile", fBody,
				m_hot == HOT_START ? t.btnFaceHot : t.btnFace, t.btnText, t.borderSel );

	// summary line left of the button
	int patches = PatchCountFor( m_preset );
	CString sum;
	sum.Format( "%d patches - %s", patches, (LPCTSTR)FormatDuration( EstimateSeconds( patches ) ) );
	if ( hasOld )
		sum += "   (replaces the existing capture)";
	DrawStr( g, sum, fSmall,
			 Gdiplus::RectF( (float)( rc.Width() / 2 ), (float)( m_rcStart.top + 8 ),
							 (float)( m_rcStart.left - rc.Width() / 2 - mgn ), 18.0f ),
			 hasOld ? t.danger : t.dimtxt, Gdiplus::StringAlignmentFar );
}

void CProfilePane::PaintRunning(Gdiplus::Graphics & g, const CRect & rc, bool dark)
{
	SPaneTheme t = PaneTheme( dark );
	int mgn = GetConfig()->Scale( 12 );
	Gdiplus::Font fTitle( L"Segoe UI", 11.0f, Gdiplus::FontStyleBold );
	Gdiplus::Font fBody( L"Segoe UI", 10.0f );
	Gdiplus::Font fSmall( L"Segoe UI", 9.0f );

	CMeasure * pMeasure = Measure();
	if ( !pMeasure )
		return;
	int total = pMeasure->GetProfileMeasureSize();
	int cur = min( pMeasure->m_currentIndex, total );

	int y = mgn;
	CString hdr;
	int nCube = pMeasure->GetProfileCubeSize();
	hdr.Format( "Profiling %dx%dx%d%s%s%s", nCube, nCube, nCube,
				pMeasure->GetProfileGrayExtras() ? " + gray/near-black" : "",
				m_driftComp ? ", drift compensation on" : "",
				m_paused ? "  -  PAUSED" : "" );
	DrawStr( g, hdr, fBody, Gdiplus::RectF( (float)mgn, (float)y, (float)( rc.Width() - 2 * mgn ), 22.0f ), t.dimtxt );
	y += GetConfig()->Scale( 28 );

	// current patch swatch + label + progress bar
	int swSz = GetConfig()->Scale( 48 );
	ColorRGBDisplay cur_rgb = ( total > 0 ) ? pMeasure->GetProfilePatchRGB( min( cur, total - 1 ) )
											: ColorRGBDisplay( 0.0 );
	CRect rcSw( mgn, y, mgn + swSz, y + swSz );
	FillRound( g, rcSw, 5, SwatchColor( cur_rgb ) );
	DrawRound( g, rcSw, 5, t.border, 1.0f );

	int barX = mgn + swSz + mgn;
	int barW = rc.Width() - barX - mgn;
	CString line;
	line.Format( "Patch %d of %d  -  RGB %.0f, %.0f, %.0f", min( cur + 1, total ), total,
				 cur_rgb[0], cur_rgb[1], cur_rgb[2] );
	DrawStr( g, line, fBody, Gdiplus::RectF( (float)barX, (float)y, (float)barW * 0.6f, 21.0f ), t.text );

	double frac = total > 0 ? (double)cur / total : 0.0;
	int remain = total - cur;
	CString eta;
	eta.Format( "%d%%  -  about %s left", (int)( frac * 100.0 + 0.5 ),
				(LPCTSTR)FormatDuration( remain * ( m_emaPatchSecs > 0 ? m_emaPatchSecs : 1.8 ) ) );
	DrawStr( g, eta, fSmall, Gdiplus::RectF( (float)barX + (float)barW * 0.6f, (float)y + 2, (float)barW * 0.4f, 19.0f ),
			 t.dimtxt, Gdiplus::StringAlignmentFar );

	int barY = y + GetConfig()->Scale( 26 );
	int barH = GetConfig()->Scale( 12 );
	CRect rcTrack( barX, barY, barX + barW, barY + barH );
	FillRound( g, rcTrack, barH / 2, t.track );
	DrawRound( g, rcTrack, barH / 2, t.border, 1.0f );
	int fillW = (int)( barW * frac );
	if ( fillW > barH )
	{
		CRect rcFill( barX, barY, barX + fillW, barY + barH );
		FillRound( g, rcFill, barH / 2, t.accent );
	}

	y += swSz + mgn + GetConfig()->Scale( 4 );

	// stat tiles: avg dE / max dE / drift / per patch
	double good = 2.0, warn = 3.0;
	GetConfig()->GetDEThresholds( good, warn );
	CString v[4], lbl[4];
	lbl[0] = "Avg dE";  lbl[1] = "Max dE"; lbl[2] = "Drift"; lbl[3] = "Per patch";
	if ( m_runDECount > 0 )
	{
		v[0].Format( "%.1f", m_runSumDE / m_runDECount );
		v[1].Format( "%.1f", m_runMaxDE );
	}
	else
		v[0] = v[1] = "-";
	if ( pMeasure->GetProfileDriftComp() || m_driftComp )
		v[2].Format( "%+.1f%%", pMeasure->m_profileCurrentDrift * 100.0 );
	else
		v[2] = "off";
	v[3].Format( "%.1f s", m_emaPatchSecs );

	int tileW = GetConfig()->Scale( 108 ), tileH = GetConfig()->Scale( 46 );
	int x = mgn;
	for ( int i = 0; i < 4; i++ )
	{
		CRect rcTile( x, y, x + tileW, y + tileH );
		FillRound( g, rcTile, 6, t.card );
		DrawRound( g, rcTile, 6, t.border, 1.0f );
		DrawStr( g, lbl[i], fSmall, Gdiplus::RectF( (float)x + 10, (float)y + 5, (float)tileW - 20, 17.0f ), t.dimtxt );
		Gdiplus::Color vc = t.text;
		if ( i == 1 && m_runDECount > 0 && m_runMaxDE >= warn ) vc = t.danger;
		if ( i == 2 && fabs( pMeasure->m_profileCurrentDrift ) > 0.02 ) vc = t.danger;
		DrawStr( g, v[i], fTitle, Gdiplus::RectF( (float)x + 10, (float)y + 21, (float)tileW - 20, 22.0f ), vc );
		x += tileW + mgn;
	}

	// Pause button bottom-right; stopping uses the red Stop button / ESC, so no
	// second stop control here
	int btnW = GetConfig()->Scale( 110 ), btnH = GetConfig()->Scale( 30 );
	m_rcPause = CRect( rc.Width() - mgn - btnW, y + tileH - btnH, rc.Width() - mgn, y + tileH );
	DrawButton( g, m_rcPause, m_paused ? "Resume" : "Pause", fBody,
				m_hot == HOT_PAUSE ? t.cardHot : t.card, t.text, t.border );

	CString hint = "3D view fills in live  -  stop with the red Stop button or ESC";
	DrawStr( g, hint, fSmall, Gdiplus::RectF( (float)x, (float)( y + tileH - 20 ), (float)( m_rcPause.left - x - mgn ), 18.0f ),
			 t.dimtxt, Gdiplus::StringAlignmentFar );
}

void CProfilePane::ComputeStats()
{
	if ( m_statsValid )
		return;
	m_stats.Reset();
	m_statsValid = true;

	CMeasure * pMeasure = Measure();
	if ( !pMeasure || !pMeasure->HasProfileMeasures() )
		return;

	double good = 2.0, warn = 3.0;
	GetConfig()->GetDEThresholds( good, warn );
	m_stats.histoBinW = warn / 8.0;
	if ( m_stats.histoBinW <= 0.0 ) m_stats.histoBinW = 0.5;

	int n = pMeasure->GetProfileMeasureSize();
	std::vector<double> des;
	des.reserve( n );
	std::vector<std::pair<double,int> > order;
	int nGoodCnt = 0;

	for ( int i = 0; i < n; i++ )
	{
		double dE = PatchDE( i );
		if ( dE < 0.0 )
			continue;
		des.push_back( dE );
		order.push_back( std::make_pair( -dE, i ) );
		m_stats.avgDE += dE;
		if ( dE > m_stats.maxDE ) m_stats.maxDE = dE;
		if ( dE < good ) nGoodCnt++;
		// clamp BOTH sides: an out-of-range float->int cast is undefined and can
		// go hugely negative, and histo[negative]++ corrupts memory
		int bin = (int)( dE / m_stats.histoBinW );
		if ( bin < 0 ) bin = 0;
		if ( bin > 15 ) bin = 15;
		m_stats.histo[bin]++;

		// region classification from the generated stimulus
		ColorRGBDisplay rgb = pMeasure->GetProfilePatchRGB( i );
		double mx = max( rgb[0], max( rgb[1], rgb[2] ) );
		double mn = min( rgb[0], min( rgb[1], rgb[2] ) );
		int reg;
		if ( mx < 10.0 )
			reg = 1;											// near black
		else if ( ( mx - mn ) < 1e-6 )
			reg = 0;											// gray axis
		else if ( mx > 0.0 && ( mx - mn ) / mx < 0.5 )
			reg = 2;											// low saturation
		else
			reg = 3;											// high saturation
		m_stats.regAvg[reg] += dE;
		if ( dE > m_stats.regMax[reg] ) m_stats.regMax[reg] = dE;
		m_stats.regCnt[reg]++;
	}

	m_stats.count = (int)des.size();
	if ( m_stats.count )
	{
		m_stats.avgDE /= m_stats.count;
		m_stats.pctGood = (double)nGoodCnt / m_stats.count;
		std::vector<double> sorted( des );
		std::sort( sorted.begin(), sorted.end() );
		int i95 = (int)( 0.95 * ( sorted.size() - 1 ) + 0.5 );
		m_stats.pct95DE = sorted[i95];
	}
	for ( int r = 0; r < 4; r++ )
		if ( m_stats.regCnt[r] )
			m_stats.regAvg[r] /= m_stats.regCnt[r];

	std::sort( order.begin(), order.end() );
	int nw = min( (int)order.size(), 20 );
	for ( int w = 0; w < nw; w++ )
		m_stats.worst.push_back( order[w].second );
}

void CProfilePane::PaintSummary(Gdiplus::Graphics & g, const CRect & rc, bool dark)
{
	SPaneTheme t = PaneTheme( dark );
	int mgn = GetConfig()->Scale( 12 );
	Gdiplus::Font fTitle( L"Segoe UI", 12.0f, Gdiplus::FontStyleBold );
	Gdiplus::Font fBody( L"Segoe UI", 10.0f );
	Gdiplus::Font fSmall( L"Segoe UI", 9.0f );

	CMeasure * pMeasure = Measure();
	if ( !pMeasure )
		return;
	ComputeStats();

	double good = 2.0, warn = 3.0;
	GetConfig()->GetDEThresholds( good, warn );

	// header + "New profile..." button
	int nCube = pMeasure->GetProfileCubeSize();
	CString hdr;
	hdr.Format( "%dx%dx%d%s  -  %d of %d patches  -  %s%s",
				nCube, nCube, nCube,
				pMeasure->GetProfileGrayExtras() ? " + gray/near-black" : "",
				m_stats.count, pMeasure->GetProfileMeasureSize(),
				(LPCTSTR)FormatDuration( pMeasure->GetProfileCaptureSeconds() ),
				pMeasure->GetProfileDriftComp() ? "  -  drift comp" : "" );
	int btnW = GetConfig()->Scale( 120 ), btnH = GetConfig()->Scale( 28 );
	m_rcNewProfile = CRect( rc.Width() - mgn - btnW, mgn, rc.Width() - mgn, mgn + btnH );
	DrawButton( g, m_rcNewProfile, "New profile...", fSmall,
				m_hot == HOT_NEWPROFILE ? t.cardHot : t.card, t.text, t.border );
	DrawStr( g, hdr, fBody, Gdiplus::RectF( (float)mgn, (float)mgn + 4, (float)( m_rcNewProfile.left - 2 * mgn ), 21.0f ), t.dimtxt );

	int y = mgn + btnH + GetConfig()->Scale( 10 );

	// stat tiles (left column)
	CString v[4], lbl[4];
	lbl[0] = "Avg dE"; lbl[1] = "95th pct"; lbl[2] = "Max dE"; lbl[3] = "Within target";
	v[0].Format( "%.1f", m_stats.avgDE );
	v[1].Format( "%.1f", m_stats.pct95DE );
	v[2].Format( "%.1f", m_stats.maxDE );
	v[3].Format( "%d%%", (int)( m_stats.pctGood * 100.0 + 0.5 ) );
	int leftW = rc.Width() * 55 / 100;
	int tileW = ( leftW - mgn * 5 ) / 4;
	int tileH = GetConfig()->Scale( 48 );
	int x = mgn;
	for ( int i = 0; i < 4; i++ )
	{
		CRect rcTile( x, y, x + tileW, y + tileH );
		FillRound( g, rcTile, 6, t.card );
		DrawRound( g, rcTile, 6, t.border, 1.0f );
		DrawStr( g, lbl[i], fSmall, Gdiplus::RectF( (float)x + 10, (float)y + 5, (float)tileW - 20, 17.0f ), t.dimtxt );
		Gdiplus::Color vc = t.text;
		if ( i == 2 && m_stats.maxDE >= warn ) vc = t.danger;
		DrawStr( g, v[i], fTitle, Gdiplus::RectF( (float)x + 10, (float)y + 21, (float)tileW - 20, 24.0f ), vc );
		x += tileW + mgn;
	}

	// dE histogram under the tiles (left column)
	int histoY = y + tileH + GetConfig()->Scale( 24 );
	int regRows = GetConfig()->Scale( 18 ) * 4 + GetConfig()->Scale( 22 );
	int histoH = max( GetConfig()->Scale( 40 ), rc.Height() - histoY - regRows - GetConfig()->Scale( 30 ) );
	int histoW = leftW - 2 * mgn;
	DrawStr( g, "dE distribution", fSmall, Gdiplus::RectF( (float)mgn, (float)histoY - 17, 220.0f, 16.0f ), t.dimtxt );
	int maxBin = 1;
	int b;
	for ( b = 0; b < 16; b++ )
		if ( m_stats.histo[b] > maxBin ) maxBin = m_stats.histo[b];
	int bw = histoW / 16;
	for ( b = 0; b < 16; b++ )
	{
		int bh = (int)( (double)m_stats.histo[b] / maxBin * ( histoH - 2 ) );
		if ( bh < 1 ) continue;
		double binMid = ( b + 0.5 ) * m_stats.histoBinW;
		CRect rcBar( mgn + b * bw, histoY + histoH - bh, mgn + b * bw + bw - 3, histoY + histoH );
		if ( bh > 4 )
			FillRound( g, rcBar, 2, DEColor( binMid, good, warn ) );
		else
		{
			Gdiplus::SolidBrush bb( DEColor( binMid, good, warn ) );
			g.FillRectangle( &bb, rcBar.left, rcBar.top, rcBar.Width(), rcBar.Height() );
		}
	}
	Gdiplus::Pen axPen( t.border, 1.0f );
	g.DrawLine( &axPen, mgn, histoY + histoH, mgn + histoW, histoY + histoH );
	CString axL, axM, axR;
	axL = "0";
	axM.Format( "%.3g (good)", good );
	axR.Format( "%.3g+ (warn)", warn );
	DrawStr( g, axL, fSmall, Gdiplus::RectF( (float)mgn, (float)( histoY + histoH + 4 ), 40.0f, 16.0f ), t.dimtxt );
	DrawStr( g, axM, fSmall, Gdiplus::RectF( (float)( mgn + (int)( good / m_stats.histoBinW ) * bw - 40 ), (float)( histoY + histoH + 4 ), 90.0f, 16.0f ), t.dimtxt, Gdiplus::StringAlignmentCenter );
	DrawStr( g, axR, fSmall, Gdiplus::RectF( (float)( mgn + histoW - 90 ), (float)( histoY + histoH + 4 ), 90.0f, 16.0f ), t.dimtxt, Gdiplus::StringAlignmentFar );

	// by-region table under the histogram
	int regY = histoY + histoH + GetConfig()->Scale( 28 );
	DrawStr( g, "By region", fSmall, Gdiplus::RectF( (float)mgn, (float)regY - 17, 150.0f, 16.0f ), t.dimtxt );
	for ( int r = 0; r < 4; r++ )
	{
		if ( regY + 18 > rc.Height() ) break;
		CString row;
		if ( m_stats.regCnt[r] )
			row.Format( "%s:  avg %.1f,  max %.1f", kRegionNames[r], m_stats.regAvg[r], m_stats.regMax[r] );
		else
			row.Format( "%s:  no data", kRegionNames[r] );
		DrawStr( g, row, fSmall, Gdiplus::RectF( (float)mgn, (float)regY, (float)histoW, 17.0f ), t.text );
		regY += GetConfig()->Scale( 18 );
	}

	// worst patches (right column), clickable
	int wx = leftW + mgn;
	int wy = y;
	DrawStr( g, "Worst patches  -  click to inspect in the 3D view", fSmall,
			 Gdiplus::RectF( (float)wx, (float)wy - 2, (float)( rc.Width() - wx - mgn ), 16.0f ), t.dimtxt );
	wy += GetConfig()->Scale( 20 );
	int rowH = GetConfig()->Scale( 20 );
	for ( size_t w = 0; w < m_stats.worst.size(); w++ )
	{
		if ( wy + rowH > rc.Height() - mgn )
			break;
		int pi = m_stats.worst[w];
		ColorRGBDisplay rgb = pMeasure->GetProfilePatchRGB( pi );
		double dE = PatchDE( pi );
		CRect rcRow( wx, wy, rc.Width() - mgn, wy + rowH );
		m_rcWorstRows.push_back( std::make_pair( rcRow, pi ) );

		if ( m_hot == HOT_WORST_FIRST + (int)w )
			FillRound( g, rcRow, 4, t.cardHot );

		CRect rcSw( wx + 3, wy + 3, wx + rowH - 3, wy + rowH - 3 );
		FillRound( g, rcSw, 3, SwatchColor( rgb ) );
		DrawRound( g, rcSw, 3, t.border, 1.0f );

		CString row;
		row.Format( "RGB %.0f, %.0f, %.0f", rgb[0], rgb[1], rgb[2] );
		DrawStr( g, row, fSmall, Gdiplus::RectF( (float)( wx + rowH + 6 ), (float)wy + 2, (float)( rc.Width() - wx - mgn - rowH - 60 ), 17.0f ), t.text );
		CString de;
		de.Format( "%.1f", dE );
		DrawStr( g, de, fSmall, Gdiplus::RectF( (float)( rc.Width() - mgn - 52 ), (float)wy + 2, 48.0f, 17.0f ),
				 DEColor( dE, good, warn ), Gdiplus::StringAlignmentFar );
		wy += rowH;
	}
}

int CProfilePane::HotFromPoint(CPoint pt) const
{
	switch ( m_state )
	{
		case PS_SETUP:
			for ( int p = 0; p < 5; p++ )
				if ( m_rcPresets[p].PtInRect( pt ) )
					return p;
			if ( m_rcStart.PtInRect( pt ) )
				return HOT_START;
			break;

		case PS_RUNNING:
			if ( m_rcPause.PtInRect( pt ) )
				return HOT_PAUSE;
			break;

		case PS_SUMMARY:
			if ( m_rcNewProfile.PtInRect( pt ) )
				return HOT_NEWPROFILE;
			for ( size_t w = 0; w < m_rcWorstRows.size(); w++ )
				if ( m_rcWorstRows[w].first.PtInRect( pt ) )
					return HOT_WORST_FIRST + (int)w;
			break;
	}
	return HOT_NONE;
}

void CProfilePane::OnMouseMove(UINT nFlags, CPoint point)
{
	if ( !m_trackingMouse )
	{
		TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hWnd, 0 };
		TrackMouseEvent( &tme );
		m_trackingMouse = true;
	}
	int hot = HotFromPoint( point );
	if ( hot != m_hot )
	{
		m_hot = hot;
		Invalidate( FALSE );
	}
	CWnd::OnMouseMove( nFlags, point );
}

LRESULT CProfilePane::OnMouseLeave(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	m_trackingMouse = false;
	if ( m_hot != HOT_NONE )
	{
		m_hot = HOT_NONE;
		Invalidate( FALSE );
	}
	return 0;
}

void CProfilePane::OnLButtonDown(UINT nFlags, CPoint point)
{
	switch ( m_state )
	{
		case PS_SETUP:
		{
			for ( int p = 0; p < 5; p++ )
			{
				if ( m_rcPresets[p].PtInRect( point ) && p != m_preset )
				{
					m_preset = p;
					GetConfig()->WriteProfileInt("MainView", "Profile Preset", m_preset);
					Invalidate( FALSE );
					CWnd::OnLButtonDown( nFlags, point );
					return;
				}
			}
			if ( m_rcStart.PtInRect( point ) )
				SendAction( PA_START );
			break;
		}

		case PS_RUNNING:
			if ( m_rcPause.PtInRect( point ) )
				SendAction( PA_PAUSE );
			break;

		case PS_SUMMARY:
		{
			if ( m_rcNewProfile.PtInRect( point ) )
			{
				m_state = PS_SETUP;
				SyncChildren();
				Invalidate( FALSE );
				break;
			}
			for ( size_t w = 0; w < m_rcWorstRows.size(); w++ )
			{
				if ( m_rcWorstRows[w].first.PtInRect( point ) )
				{
					SendAction( PA_INSPECT, m_rcWorstRows[w].second );
					break;
				}
			}
			break;
		}
	}
	CWnd::OnLButtonDown( nFlags, point );
}
