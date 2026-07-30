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
#include "PngIconLoader.h"
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
	ON_WM_LBUTTONUP()
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
	, m_pressed(HOT_NONE)
	, m_trackingMouse(false)
	, m_contentDX(0)
	, m_contentDY(0)
	, m_hRefIcon(NULL)
	, m_refIconDark(false)
{
	m_preset = GetConfig()->GetProfileInt("MainView", "Profile Preset", 1);
	if ( m_preset < 0 || m_preset > 4 ) m_preset = 1;
	m_grayExtras = GetConfig()->GetProfileInt("MainView", "Profile GrayExtras", 1);
	m_driftComp = GetConfig()->GetProfileInt("MainView", "Profile DriftComp", 1);
	// Hit-test rects are rebuilt each paint, but a WM_MOUSEMOVE / WM_LBUTTONDOWN
	// can arrive before the first WM_PAINT -- start them empty so HotFromPoint
	// never reads garbage RECTs (spurious hover / unintended action).
	for ( int i = 0; i < 5; i++ ) m_rcPresets[i].SetRectEmpty();
	m_rcStart.SetRectEmpty(); m_rcPause.SetRectEmpty(); m_rcStop.SetRectEmpty();
	m_rcRefs.SetRectEmpty(); m_rcCtx.SetRectEmpty(); m_rcClear.SetRectEmpty();
}

CProfilePane::~CProfilePane()
{
	if ( m_hRefIcon )
		DestroyIcon( m_hRefIcon );
}

BOOL CProfilePane::Create(const CRect & rc, CWnd * pParent, UINT nID)
{
	if ( !CWnd::Create( AfxRegisterWndClass( 0, ::LoadCursor( NULL, IDC_ARROW ) ),
						NULL, WS_CHILD | WS_CLIPCHILDREN, rc, pParent, nID ) )
		return FALSE;

	// Frame/title insets are size-independent constants; compute them once here
	// so SyncChildren (which can run on WM_SIZE before the first WM_PAINT) always
	// reads valid offsets. OnPaint reasserts the same values.
	// contentDY = header height (Scale 34) + Scale(12) gap below it.
	m_contentDX = GetConfig()->Scale( 12 );
	m_contentDY = GetConfig()->Scale( 34 ) + GetConfig()->Scale( 12 );

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
	// Gate on state only, NOT IsWindowVisible(): on the first switch into mode 13
	// this runs mid-layout when the pane's visible flag isn't settled yet, which
	// left the checkboxes hidden until a capture cycle. Children of a hidden parent
	// are hidden by Windows anyway, so WS_VISIBLE here is safe when mode != 13.
	bool show = ( m_state == PS_SETUP );
	m_chkGrayExtras.ShowWindow( show ? SW_SHOW : SW_HIDE );
	m_chkDriftComp.ShowWindow( show ? SW_SHOW : SW_HIDE );
	if ( show )
	{
		// Checkboxes are real child controls STACKED on the left, one below the
		// other under the preset cards; their width is capped to the left half so
		// their windows never reach the right-pinned Start button (a child window
		// there would punch an opaque hole through the owner-drawn button).
		// Positions mirror PaintSetup's content layout, shifted into pane client
		// coords by the frame/title inset (m_contentDX/DY).
		CRect rc;
		GetClientRect( &rc );
		int contentW = rc.Width() - 2 * m_contentDX;
		int contentBottom = rc.Height() - GetConfig()->Scale( 12 );	// matches OnPaint's bottom inset
		int h = GetConfig()->Scale( 20 );
		int x = m_contentDX;
		int w = min( GetConfig()->Scale( 360 ), contentW / 2 - GetConfig()->Scale( 10 ) );
		if ( w < GetConfig()->Scale( 120 ) ) w = GetConfig()->Scale( 120 );
		// stacked, anchored to the content bottom (same row band as the Start button)
		int y2 = contentBottom - h;						// bottom checkbox row
		int y1 = y2 - h - GetConfig()->Scale( 6 );		// row above it
		m_chkGrayExtras.MoveWindow( x, y1, w, h );
		m_chkDriftComp.MoveWindow( x, y2, w, h );
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
	// centralised in CMeasure so it matches the measures grid exactly, including
	// the PQ-HDR (mode 5) absolute-nits bridge
	return pMeasure->ComputeProfileDE( pMeasure->GetProfileMeasure( i ), i );
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
	// checkbox labels sit on the pane BODY, which is the panel fill; match it
	// exactly (and follow theme changes) so the label background never conflicts
	static CBrush s_brush;
	static COLORREF s_col = CLR_INVALID;
	COLORREF bg = FxGetMenuBgColor();
	if ( bg != s_col )
	{
		s_brush.DeleteObject();
		s_brush.CreateSolidBrush( bg );
		s_col = bg;
	}
	pDC->SetBkMode( TRANSPARENT );
	pDC->SetTextColor( FxGetSysColor( COLOR_WINDOWTEXT ) );
	return (HBRUSH)s_brush.GetSafeHandle();
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

// dE colours tuned for TEXT legibility on the pane body (bar fills use DEColor).
static Gdiplus::Color DETextColor(double dE, double good, double warn, bool dark)
{
	if ( dE < good )
		return dark ? Gdiplus::Color( 124, 200, 110 ) : Gdiplus::Color(  56, 128,  40 );	// green
	if ( dE < warn )
		return dark ? Gdiplus::Color( 236, 178,  92 ) : Gdiplus::Color( 170, 106,  16 );	// amber
	return dark ? Gdiplus::Color( 238, 122, 120 ) : Gdiplus::Color( 188,  52,  52 );		// red
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

// Colours mirror the app theme (SetFxColors: window/menu/accent/text) so the
// pane reads as a sibling of the View/Sensor/Generator panels.
//   body   = window colour   (recessed area, matches the data-grid background)
//   header = menu/panel colour (raised header strip; also the stat chips, cards
//            and secondary buttons)
//   border = panel border, accent = selection blue.
struct SPaneTheme
{
	Gdiplus::Color body, header, card, cardHot, cardSel, cardSelHot, border, btnBorder, borderSel,
					text, dimtxt, accent, btnFace, btnFaceHot, btnText, danger, track;
};

static SPaneTheme PaneTheme(bool dark)
{
	SPaneTheme t;
	// Derive the structural colours from the SAME functions the other panels use
	// (CMainView::OnEraseBkgnd fills them with FxGetMenuBgColor + a 64,64,70 /
	// COLOR_3DSHADOW border; XPGroupBox captions use COLOR_BTNFACE), so the pane
	// matches its siblings by construction in any theme.
	t.body   = GpColor( FxGetMenuBgColor() );					// panel fill
	t.header = GpColor( FxGetSysColor( COLOR_BTNFACE ) );		// panel caption / header strip
	t.border = GpColor( ( fxUseCustomColor != FALSE ) ? RGB( 64, 64, 70 ) : FxGetSysColor( COLOR_3DSHADOW ) );
	t.text   = GpColor( FxGetSysColor( COLOR_WINDOWTEXT ) );

	t.borderSel  = Gdiplus::Color(   0, 120, 215 );	// selection blue
	t.accent     = Gdiplus::Color(   0, 120, 215 );
	t.btnFace    = Gdiplus::Color(   0, 120, 215 );	// primary (Start) button
	t.btnFaceHot = Gdiplus::Color(  38, 143, 226 );
	t.btnText    = Gdiplus::Color( 250, 252, 255 );

	if ( dark )
	{
		t.card       = Gdiplus::Color(  57,  57,  61 );	// raised chips/cards, a touch brighter than the header
		t.cardHot    = Gdiplus::Color(  70,  70,  74 );
		t.cardSel    = Gdiplus::Color(  38,  54,  78 );
		t.cardSelHot = Gdiplus::Color(  46,  64,  92 );
		t.btnBorder  = Gdiplus::Color(  96,  96, 102 );	// button outline, brighter than the panel border
		t.dimtxt     = Gdiplus::Color( 148, 148, 154 );
		t.danger     = Gdiplus::Color( 232, 112, 110 );
		t.track      = Gdiplus::Color(  58,  58,  62 );
	}
	else
	{
		t.card       = Gdiplus::Color( 255, 255, 255 );	// raised white chips on the gray panel body
		t.cardHot    = Gdiplus::Color( 246, 246, 248 );
		t.cardSel    = Gdiplus::Color( 225, 238, 252 );
		t.cardSelHot = Gdiplus::Color( 214, 231, 250 );
		t.btnBorder  = Gdiplus::Color( 178, 180, 188 );	// button outline, a touch stronger than the panel border
		t.dimtxt     = Gdiplus::Color(  96,  98, 104 );
		t.danger     = Gdiplus::Color( 186,  58,  58 );
		t.track      = Gdiplus::Color( 228, 228, 231 );
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

// Fill a rect with only its TOP two corners rounded (for the header strip that
// sits inside the rounded frame; its bottom edge is square against the body).
static void FillRoundTop(Gdiplus::Graphics & g, const CRect & rc, int r, const Gdiplus::Color & fill)
{
	int d = r * 2;
	Gdiplus::GraphicsPath path;
	path.AddArc( rc.left, rc.top, d, d, 180.0f, 90.0f );
	path.AddArc( rc.right - d - 1, rc.top, d, d, 270.0f, 90.0f );
	path.AddLine( (Gdiplus::REAL)( rc.right - 1 ), (Gdiplus::REAL)( rc.top + r ),
				  (Gdiplus::REAL)( rc.right - 1 ), (Gdiplus::REAL)rc.bottom );
	path.AddLine( (Gdiplus::REAL)( rc.right - 1 ), (Gdiplus::REAL)rc.bottom,
				  (Gdiplus::REAL)rc.left, (Gdiplus::REAL)rc.bottom );
	path.CloseFigure();
	Gdiplus::SolidBrush br( fill );
	g.FillPath( &br, &path );
}

// StringFormatFlagsNoClip lets descenders/overhangs render even when the rect
// is a hair short -- the fix for the pervasive glyph clipping. Callers still
// pass rects at least one line-height tall; NoClip is the safety net.
static void DrawStr(Gdiplus::Graphics & g, const CString & s, Gdiplus::Font & f,
					const Gdiplus::RectF & rc, const Gdiplus::Color & clr,
					Gdiplus::StringAlignment ha = Gdiplus::StringAlignmentNear,
					Gdiplus::StringAlignment va = Gdiplus::StringAlignmentNear)
{
	Gdiplus::SolidBrush br( clr );
	Gdiplus::StringFormat sf;
	sf.SetAlignment( ha );
	sf.SetLineAlignment( va );
	sf.SetFormatFlags( Gdiplus::StringFormatFlagsNoWrap | Gdiplus::StringFormatFlagsNoClip );
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

// Rounded button with a leading icon (PNG HICON) or a Fluent glyph, then the
// label; the icon+label group is centred in the button.
static void DrawIconButton(Gdiplus::Graphics & g, const CRect & rc, const CString & label,
						   Gdiplus::Font & fLabel, HICON hIcon, wchar_t glyph, Gdiplus::Font * pGlyphFont,
						   int iconSz, const Gdiplus::Color & face, const Gdiplus::Color & txt,
						   const Gdiplus::Color & border)
{
	FillRound( g, rc, 5, face );
	DrawRound( g, rc, 5, border, 1.0f );

	Gdiplus::RectF bb;
	CStringW wlabel( label );
	g.MeasureString( wlabel, -1, &fLabel, Gdiplus::PointF( 0.0f, 0.0f ), &bb );
	int textW = (int)( bb.Width + 0.5f );
	int iconW = ( hIcon || ( glyph && pGlyphFont ) ) ? iconSz : 0;
	int ggap  = iconW ? GetConfig()->Scale( 4 ) : 0;
	int startX = rc.left + ( rc.Width() - ( iconW + ggap + textW ) ) / 2;
	int iy = rc.top + ( rc.Height() - iconSz ) / 2;

	if ( hIcon )
	{
		HDC hdc = g.GetHDC();
		DrawIconEx( hdc, startX, iy, hIcon, iconSz, iconSz, 0, NULL, DI_NORMAL );
		g.ReleaseHDC( hdc );
	}
	else if ( glyph && pGlyphFont )
	{
		Gdiplus::SolidBrush gb( txt );
		Gdiplus::StringFormat sf;
		sf.SetAlignment( Gdiplus::StringAlignmentCenter );
		sf.SetLineAlignment( Gdiplus::StringAlignmentCenter );
		wchar_t gs[2] = { glyph, 0 };
		g.DrawString( gs, 1, pGlyphFont,
					  Gdiplus::RectF( (float)startX, (float)rc.top, (float)iconSz, (float)rc.Height() ), &sf, &gb );
	}

	DrawStr( g, label, fLabel,
			 Gdiplus::RectF( (float)( startX + iconW + ggap ), (float)rc.top, (float)( textW + 6 ), (float)rc.Height() ),
			 txt, Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter );
}

// Width a chrome button needs to fit its icon/glyph + label without truncation
// (icon + gap + text + symmetric horizontal padding).
static int ContentButtonWidth(Gdiplus::Graphics & g, const CString & label, Gdiplus::Font & f,
							  int iconSz, int padX)
{
	Gdiplus::RectF bb;
	CStringW w( label );
	g.MeasureString( w, -1, &f, Gdiplus::PointF( 0.0f, 0.0f ), &bb );
	return iconSz + GetConfig()->Scale( 4 ) + (int)( bb.Width + 0.5f ) + 2 * padX;
}

// Media transport marks drawn as crisp geometric shapes (pause bars / stop
// square / resume triangle) so they render identically to the Segoe media
// glyphs without any font/codepoint ambiguity.
enum { MEDIA_PAUSE = 1, MEDIA_STOP = 2, MEDIA_PLAY = 3 };
static void DrawMediaShape(Gdiplus::Graphics & g, int kind, int cx, int cy, int s, const Gdiplus::Color & clr)
{
	Gdiplus::SolidBrush br( clr );
	if ( kind == MEDIA_PAUSE )
	{
		int bw = max( 2, (int)( s * 0.28 ) ), bh = (int)( s * 0.9 ), gp = max( 2, (int)( s * 0.20 ) );
		g.FillRectangle( &br, cx - gp / 2 - bw, cy - bh / 2, bw, bh );
		g.FillRectangle( &br, cx + gp / 2,      cy - bh / 2, bw, bh );
	}
	else if ( kind == MEDIA_STOP )
	{
		int q = (int)( s * 0.82 );
		CRect sq( cx - q / 2, cy - q / 2, cx + q / 2, cy + q / 2 );
		FillRound( g, sq, 2, clr );
	}
	else if ( kind == MEDIA_PLAY )
	{
		Gdiplus::PointF pts[3] = {
			Gdiplus::PointF( (float)( cx - s * 0.26 ), (float)( cy - s * 0.45 ) ),
			Gdiplus::PointF( (float)( cx + s * 0.38 ), (float)cy ),
			Gdiplus::PointF( (float)( cx - s * 0.26 ), (float)( cy + s * 0.45 ) ) };
		g.FillPolygon( &br, pts, 3 );
	}
}

static void DrawMediaButton(Gdiplus::Graphics & g, const CRect & rc, const CString & label,
							Gdiplus::Font & fLabel, int mediaShape, const Gdiplus::Color & shapeClr,
							int iconSz, const Gdiplus::Color & face, const Gdiplus::Color & txt,
							const Gdiplus::Color & border)
{
	FillRound( g, rc, 5, face );
	DrawRound( g, rc, 5, border, 1.0f );

	Gdiplus::RectF bb;
	CStringW wlabel( label );
	g.MeasureString( wlabel, -1, &fLabel, Gdiplus::PointF( 0.0f, 0.0f ), &bb );
	int textW = (int)( bb.Width + 0.5f );
	int ggap  = GetConfig()->Scale( 6 );
	int startX = rc.left + ( rc.Width() - ( iconSz + ggap + textW ) ) / 2;

	DrawMediaShape( g, mediaShape, startX + iconSz / 2, rc.top + rc.Height() / 2, iconSz, shapeClr );
	DrawStr( g, label, fLabel,
			 Gdiplus::RectF( (float)( startX + iconSz + ggap ), (float)rc.top, (float)( textW + 6 ), (float)rc.Height() ),
			 txt, Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter );
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
	// The rounded corners reveal whatever the parent painted behind the pane;
	// fill that gap with the EXACT parent background (CMainView::OnEraseBkgnd
	// uses FxGetMenuBgColors) so the corners blend seamlessly in any theme.
	COLORREF ctop = 0, cbot = 0;
	FxGetMenuBgColors( ctop, cbot );
	Gdiplus::SolidBrush hostBrush( GpColor( ctop ) );
	g.FillRectangle( &hostBrush, 0, 0, rc.Width(), rc.Height() );

	CRect rcFrame( 1, 1, rc.Width() - 2, rc.Height() - 2 );
	FillRound( g, rcFrame, 8, t.body );

	int headerBottom = GetConfig()->Scale( 34 );	// short header strip, no divider line
	CRect rcHeader( 1, 1, rc.Width() - 2, headerBottom );
	FillRoundTop( g, rcHeader, 8, t.header );

	DrawRound( g, rcFrame, 8, t.border, 1.0f );

	// reset hit rects; the chrome + active state's painter rebuild their own
	int i;
	for ( i = 0; i < 5; i++ ) m_rcPresets[i].SetRectEmpty();
	m_rcStart.SetRectEmpty(); m_rcPause.SetRectEmpty(); m_rcStop.SetRectEmpty();
	m_rcRefs.SetRectEmpty(); m_rcCtx.SetRectEmpty(); m_rcClear.SetRectEmpty();
	m_rcWorstRows.clear();

	// The chrome status line shows the summary count, so stats must be current
	// BEFORE PaintChrome (PaintSummary would otherwise compute them afterwards,
	// leaving a stale count on the first paint after a capture). Cheap on repeat
	// (guarded by m_statsValid).
	if ( m_state == PS_SUMMARY )
		ComputeStats();

	// fixed chrome row (title + status + buttons), drawn in client coords so it
	// is identical across all three states
	PaintChrome( g, rc, dark );

	// content is inset inside the border and below the chrome row; the Paint*
	// methods draw from (0,0), so translate here and shift mouse points to match
	// content sits Scale(12) below the header bottom, matching the L/R/B insets so
	// it is equidistant from the left edge and the header
	m_contentDX = GetConfig()->Scale( 12 );
	m_contentDY = GetConfig()->Scale( 34 ) + GetConfig()->Scale( 12 );
	CRect content( 0, 0, rc.Width() - 2 * m_contentDX,
				   rc.Height() - m_contentDY - GetConfig()->Scale( 12 ) );	// equal L/R/B insets

	Gdiplus::GraphicsState gs = g.Save();
	g.TranslateTransform( (float)m_contentDX, (float)m_contentDY );

	switch ( m_state )
	{
		case PS_SETUP:   PaintSetup( g, content, dark );   break;
		case PS_RUNNING: PaintRunning( g, content, dark ); break;
		case PS_SUMMARY: PaintSummary( g, content, dark ); break;
	}
	g.Restore( gs );

	Gdiplus::Graphics screen( dc.GetSafeHdc() );
	screen.DrawImage( &bmp, 0, 0 );
}

CString CProfilePane::StatusLine() const
{
	CMeasure * pMeasure = Measure();
	CString s;
	switch ( m_state )
	{
		case PS_SETUP:
			s = "Measure an RGB cube grid to characterize this display";
			break;

		case PS_RUNNING:
			if ( pMeasure )
			{
				int n = pMeasure->GetProfileCubeSize();
				s.Format( "Profiling %dx%dx%d%s%s%s", n, n, n,
						  pMeasure->GetProfileGrayExtras() ? " + gray/near-black" : "",
						  m_driftComp ? ", drift compensation on" : "",
						  m_paused ? "   -   PAUSED" : "" );
			}
			break;

		case PS_SUMMARY:
			if ( pMeasure )
			{
				int n = pMeasure->GetProfileCubeSize();
				s.Format( "%dx%dx%d%s   -   %d of %d patches   -   %s%s", n, n, n,
						  pMeasure->GetProfileGrayExtras() ? " + gray/near-black" : "",
						  m_stats.count, pMeasure->GetProfileMeasureSize(),
						  (LPCTSTR)FormatDuration( pMeasure->GetProfileCaptureSeconds() ),
						  pMeasure->GetProfileDriftComp() ? "   -   Drift comp on" : "" );
			}
			break;
	}
	return s;
}

// Fixed header row in pane CLIENT coords: title (left) + status (middle) +
// context button + References button (right). Identical position/font in every
// state so nothing shifts between views. Buttons are pinned to the top-right.
void CProfilePane::PaintChrome(Gdiplus::Graphics & g, const CRect & client, bool dark)
{
	SPaneTheme t = PaneTheme( dark );
	Gdiplus::Font fTitle ( L"Segoe UI", 10.0f, Gdiplus::FontStyleBold );
	Gdiplus::Font fStatus( L"Segoe UI", 9.5f );
	Gdiplus::Font fBtn   ( L"Segoe UI", 9.0f );
	Gdiplus::Font fGlyph ( L"Segoe Fluent Icons", 9.0f );

	int pad     = GetConfig()->Scale( 12 );
	int headerBottom = GetConfig()->Scale( 34 );	// header strip spans client y = 1 .. headerBottom
	int btnH    = GetConfig()->Scale( 24 );
	int top     = ( 1 + headerBottom - btnH ) / 2;	// centred within the visible strip
	int titleW  = GetConfig()->Scale( 116 );
	int iconSz  = GetConfig()->Scale( 15 );
	int padX    = GetConfig()->Scale( 10 );			// horizontal padding inside each chrome button

	// title, left-aligned with the body content (pad), centred on the chrome row
	DrawStr( g, "Display profile", fTitle,
			 Gdiplus::RectF( (float)pad, (float)top, (float)titleW, (float)btnH ),
			 t.text, Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter );

	// References button (rightmost), width fitted to its content, using the same
	// PNG glyph as the toolbar Refs button (cached, reloaded on theme change)
	if ( m_hRefIcon == NULL || m_refIconDark != dark )
	{
		if ( m_hRefIcon ) DestroyIcon( m_hRefIcon );
		m_hRefIcon = HCFR_LoadPngHIcon( _T("toolbar"), _T("references"), dark, iconSz, iconSz );
		m_refIconDark = dark;
	}
	int refsW = ContentButtonWidth( g, "References...", fBtn, iconSz, padX );
	m_rcRefs = CRect( client.right - pad - refsW, top, client.right - pad, top + btnH );
	DrawIconButton( g, m_rcRefs, "References...", fBtn, m_hRefIcon, 0, NULL, iconSz,
					m_hot == HOT_REFS ? t.cardHot : t.card, t.text, t.btnBorder );

	// per-state context button to the left of References, width fitted to its label
	m_ctxLabel.Empty();
	if ( m_state == PS_SUMMARY )
		m_ctxLabel = "New profile...";
	else if ( m_state == PS_SETUP && Measure() && Measure()->HasProfileMeasures() )
		m_ctxLabel = "Back to summary";

	int statusRight = m_rcRefs.left - GetConfig()->Scale( 12 );
	if ( !m_ctxLabel.IsEmpty() )
	{
		int ctxW = ContentButtonWidth( g, m_ctxLabel, fBtn, iconSz, padX );
		m_rcCtx = CRect( m_rcRefs.left - GetConfig()->Scale( 8 ) - ctxW, top,
						 m_rcRefs.left - GetConfig()->Scale( 8 ), top + btnH );
		// Fluent "Add" for New profile, "ChevronLeft" for Back to summary
		wchar_t ctxGlyph = ( m_state == PS_SUMMARY ) ? (wchar_t)0xE710 : (wchar_t)0xE76B;
		DrawIconButton( g, m_rcCtx, m_ctxLabel, fBtn, NULL, ctxGlyph, &fGlyph, iconSz,
						m_hot == HOT_CTX ? t.cardHot : t.card, t.text, t.btnBorder );
		statusRight = m_rcCtx.left - GetConfig()->Scale( 12 );
	}

	// Delete button (summary only) discards the captured profile from the document
	if ( m_state == PS_SUMMARY && !m_rcCtx.IsRectEmpty() )
	{
		int delW = ContentButtonWidth( g, "Delete", fBtn, iconSz, padX );
		m_rcClear = CRect( m_rcCtx.left - GetConfig()->Scale( 8 ) - delW, top,
						   m_rcCtx.left - GetConfig()->Scale( 8 ), top + btnH );
		DrawIconButton( g, m_rcClear, "Delete", fBtn, NULL, (wchar_t)0xE74D, &fGlyph, iconSz,	// Fluent "Delete"
						m_hot == HOT_CLEAR ? t.cardHot : t.card, t.danger, t.btnBorder );
		statusRight = m_rcClear.left - GetConfig()->Scale( 12 );
	}

	// status text fills the gap between the title and the buttons
	int statusLeft = pad + titleW + GetConfig()->Scale( 8 );
	if ( statusRight - statusLeft > GetConfig()->Scale( 40 ) )
		DrawStr( g, StatusLine(), fStatus,
				 Gdiplus::RectF( (float)statusLeft, (float)top,
								 (float)( statusRight - statusLeft ), (float)btnH ),
				 t.text, Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter );
}

void CProfilePane::PaintSetup(Gdiplus::Graphics & g, const CRect & rc, bool dark)
{
	SPaneTheme t = PaneTheme( dark );
	Gdiplus::Font fTitle( L"Segoe UI", 11.0f, Gdiplus::FontStyleBold );
	Gdiplus::Font fBody( L"Segoe UI", 10.0f );
	Gdiplus::Font fSmall( L"Segoe UI", 9.0f );

	CMeasure * pMeasure = Measure();
	bool hasOld = ( pMeasure && pMeasure->HasProfileMeasures() );

	const int CW = rc.Width();
	const int gap = GetConfig()->Scale( 10 );

	// preset cards, single row of 5 across the full width
	int cardH = GetConfig()->Scale( 80 );
	int cardW = max( 1, ( CW - gap * 4 ) / 5 );	// floor so a very narrow pane can't make degenerate rects
	int x = 0, y = 0;
	for ( int p = 0; p < 5; p++ )
	{
		CRect rcCard( x, y, x + cardW, y + cardH );
		m_rcPresets[p] = rcCard;
		bool sel = ( p == m_preset );
		bool hot = ( m_hot == p );
		FillRound( g, rcCard, 6, sel ? ( hot ? t.cardSelHot : t.cardSel )
									 : ( hot ? t.cardHot : t.card ) );
		DrawRound( g, rcCard, 6, sel ? t.borderSel : t.btnBorder, 1.0f );

		int patches = PatchCountFor( p );
		CString line1, line2, line3;
		line1 = kPresets[p].name;
		line2.Format( "%dx%dx%d cube", kPresets[p].cubeN, kPresets[p].cubeN, kPresets[p].cubeN );
		line3.Format( "%d patches  -  %s", patches, (LPCTSTR)FormatDuration( EstimateSeconds( patches ) ) );
		int tx = rcCard.left + GetConfig()->Scale( 12 );
		int tw = rcCard.Width() - GetConfig()->Scale( 24 );
		DrawStr( g, line1, fTitle, Gdiplus::RectF( (float)tx, (float)( y + GetConfig()->Scale( 8 ) ), (float)tw, 24.0f ), t.text );
		DrawStr( g, line2, fBody,  Gdiplus::RectF( (float)tx, (float)( y + GetConfig()->Scale( 34 ) ), (float)tw, 20.0f ), t.dimtxt );
		DrawStr( g, line3, fSmall, Gdiplus::RectF( (float)tx, (float)( y + GetConfig()->Scale( 55 ) ), (float)tw, 18.0f ), t.dimtxt );
		x += cardW + gap;
	}
	(void)y;
	const int CH = rc.Height();

	// Start button pinned to the content BOTTOM-RIGHT (equal gap from the right
	// and bottom edges, since the content inset is symmetric). The two checkboxes
	// are child controls stacked bottom-LEFT (placed by SyncChildren). Help text
	// sits just left of Start.
	Gdiplus::Font fGlyph( L"Segoe Fluent Icons", 9.0f );
	int btnW = GetConfig()->Scale( 130 ), btnH = GetConfig()->Scale( 32 );
	m_rcStart = CRect( CW - btnW, CH - btnH, CW, CH );
	DrawIconButton( g, m_rcStart, "Start profile", fBody, NULL, (wchar_t)0xE768, &fGlyph,	// Fluent "Play"
					GetConfig()->Scale( 14 ),
					m_hot == HOT_START ? t.btnFaceHot : t.btnFace, t.btnText, t.borderSel );

	int patches = PatchCountFor( m_preset );
	CString help;
	help.Format( "%d patches  -  %s", patches, (LPCTSTR)FormatDuration( EstimateSeconds( patches ) ) );
	if ( hasOld )
		help += "   (replaces the existing capture)";
	int helpLeft = CW / 2 + GetConfig()->Scale( 10 );
	int helpRight = m_rcStart.left - GetConfig()->Scale( 12 );
	if ( helpRight - helpLeft > GetConfig()->Scale( 60 ) )
		DrawStr( g, help, fSmall,
				 Gdiplus::RectF( (float)helpLeft, (float)( m_rcStart.top ), (float)( helpRight - helpLeft ), (float)btnH ),
				 hasOld ? t.danger : t.dimtxt, Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentCenter );
}

void CProfilePane::PaintRunning(Gdiplus::Graphics & g, const CRect & rc, bool dark)
{
	SPaneTheme t = PaneTheme( dark );
	Gdiplus::Font fBig( L"Segoe UI", 12.0f, Gdiplus::FontStyleBold );
	Gdiplus::Font fBody( L"Segoe UI", 10.0f );
	Gdiplus::Font fSmall( L"Segoe UI", 9.0f );

	CMeasure * pMeasure = Measure();
	if ( !pMeasure )
		return;
	int total = pMeasure->GetProfileMeasureSize();
	int cur = min( pMeasure->m_currentIndex, total );
	const int CW = rc.Width();
	const int CH = rc.Height();

	// current patch swatch + label + progress bar (status is in the chrome row)
	int swSz = GetConfig()->Scale( 48 );
	int y = 0;
	ColorRGBDisplay cur_rgb = ( total > 0 ) ? pMeasure->GetProfilePatchRGB( min( cur, total - 1 ) )
											: ColorRGBDisplay( 0.0 );
	CRect rcSw( 0, y, swSz, y + swSz );
	FillRound( g, rcSw, 5, SwatchColor( cur_rgb ) );
	DrawRound( g, rcSw, 5, t.border, 1.0f );

	int barX = swSz + GetConfig()->Scale( 12 );
	int barW = CW - barX;
	int etaW = GetConfig()->Scale( 210 );
	CString line;
	line.Format( "Patch %d of %d  -  RGB %.0f, %.0f, %.0f", min( cur + 1, total ), total,
				 cur_rgb[0], cur_rgb[1], cur_rgb[2] );
	DrawStr( g, line, fBody, Gdiplus::RectF( (float)barX, (float)y, (float)( barW - etaW - GetConfig()->Scale( 8 ) ), 22.0f ), t.text );

	double frac = total > 0 ? (double)cur / total : 0.0;
	int remain = total - cur;
	CString eta;
	eta.Format( "%d%%  -  about %s left", (int)( frac * 100.0 + 0.5 ),
				(LPCTSTR)FormatDuration( remain * ( m_emaPatchSecs > 0 ? m_emaPatchSecs : 1.8 ) ) );
	DrawStr( g, eta, fBody, Gdiplus::RectF( (float)( CW - etaW ), (float)y, (float)etaW, 22.0f ),
			 t.text, Gdiplus::StringAlignmentFar );

	int barY = y + GetConfig()->Scale( 28 );
	int barH = GetConfig()->Scale( 12 );
	CRect rcTrack( barX, barY, CW, barY + barH );
	FillRound( g, rcTrack, barH / 2, t.track );
	DrawRound( g, rcTrack, barH / 2, t.border, 1.0f );
	int fillW = (int)( barW * frac );
	if ( fillW > barH )
	{
		CRect rcFill( barX, barY, barX + fillW, barY + barH );
		FillRound( g, rcFill, barH / 2, t.accent );
	}

	y = swSz + GetConfig()->Scale( 14 );

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

	int gap = GetConfig()->Scale( 10 );
	int tileH = GetConfig()->Scale( 46 );

	// stat tiles in a row just below the progress bar, left-aligned
	int tileW = max( 1, min( GetConfig()->Scale( 150 ), ( CW - 3 * gap ) / 4 ) );
	int x = 0;
	for ( int i = 0; i < 4; i++ )
	{
		CRect rcTile( x, y, x + tileW, y + tileH );
		FillRound( g, rcTile, 6, t.card );		// filled chip, no border (stats, not buttons)
		DrawStr( g, lbl[i], fSmall, Gdiplus::RectF( (float)x + 10, (float)y + 5, (float)tileW - 20, 18.0f ), t.dimtxt );
		Gdiplus::Color vc = t.text;
		if ( i == 1 && m_runDECount > 0 && m_runMaxDE >= warn ) vc = t.danger;
		if ( i == 2 && fabs( pMeasure->m_profileCurrentDrift ) > 0.02 ) vc = t.danger;
		DrawStr( g, v[i], fBig, Gdiplus::RectF( (float)x + 10, (float)y + 20, (float)tileW - 20, 24.0f ), vc );
		x += tileW + gap;
	}

	// Pause + Stop pinned to the content BOTTOM-RIGHT (side measure/stop chrome is
	// hidden in this mode, so the pane owns capture control; Stop keeps partials)
	int cbW = GetConfig()->Scale( 110 ), cbH = GetConfig()->Scale( 30 );
	int glyphSz = GetConfig()->Scale( 12 );
	m_rcStop  = CRect( CW - cbW, CH - cbH, CW, CH );
	m_rcPause = CRect( m_rcStop.left - gap - cbW, CH - cbH, m_rcStop.left - gap, CH );
	DrawMediaButton( g, m_rcStop, "Stop", fBody, MEDIA_STOP, t.danger, glyphSz,
					 m_hot == HOT_STOP ? t.cardHot : t.card, t.danger, t.danger );
	DrawMediaButton( g, m_rcPause, m_paused ? "Resume" : "Pause", fBody,
					 m_paused ? MEDIA_PLAY : MEDIA_PAUSE, t.text, glyphSz,
					 m_hot == HOT_PAUSE ? t.cardHot : t.card, t.text, t.btnBorder );
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
	Gdiplus::Font fBig( L"Segoe UI", 12.0f, Gdiplus::FontStyleBold );
	Gdiplus::Font fSmall( L"Segoe UI", 9.0f );

	CMeasure * pMeasure = Measure();
	if ( !pMeasure )
		return;
	ComputeStats();

	double good = 2.0, warn = 3.0;
	GetConfig()->GetDEThresholds( good, warn );

	const int CW = rc.Width();
	const int H  = rc.Height();
	const int gap = GetConfig()->Scale( 10 );
	int leftW = CW * 56 / 100;			// tiles + histogram + region line
	int rightX = leftW + GetConfig()->Scale( 16 );	// worst-patches column
	int rightW = CW - rightX;

	// ---- left column: stat tiles (single row of 4) ----
	CString v[4], lbl[4];
	lbl[0] = "Avg dE"; lbl[1] = "95th pct"; lbl[2] = "Max dE"; lbl[3] = "Within target";
	v[0].Format( "%.1f", m_stats.avgDE );
	v[1].Format( "%.1f", m_stats.pct95DE );
	v[2].Format( "%.1f", m_stats.maxDE );
	v[3].Format( "%d%%", (int)( m_stats.pctGood * 100.0 + 0.5 ) );
	int tileW = max( 1, ( leftW - gap * 3 ) / 4 );
	int tileH = GetConfig()->Scale( 46 );
	int x = 0, y = 0;
	for ( int i = 0; i < 4; i++ )
	{
		CRect rcTile( x, y, x + tileW, y + tileH );
		FillRound( g, rcTile, 6, t.card );		// filled chip, no border (these are stats, not buttons)
		DrawStr( g, lbl[i], fSmall, Gdiplus::RectF( (float)x + 10, (float)y + 5, (float)tileW - 20, 16.0f ), t.dimtxt );
		Gdiplus::Color vc = t.text;
		if ( i == 2 && m_stats.maxDE >= warn ) vc = t.danger;
		DrawStr( g, v[i], fBig, Gdiplus::RectF( (float)x + 10, (float)y + 20, (float)tileW - 20, 24.0f ), vc );
		x += tileW + gap;
	}

	// ---- left column: dE histogram, sized to leave one axis row + one region
	// row below it so nothing clips at the pane bottom ----
	int histoLblY = y + tileH + GetConfig()->Scale( 8 );
	int histoY = histoLblY + GetConfig()->Scale( 20 );
	int histoW = leftW;
	int axisRowH = GetConfig()->Scale( 16 );
	int regRowH  = GetConfig()->Scale( 18 );
	int histoBottom = H - regRowH - axisRowH - GetConfig()->Scale( 1 );
	int histoH = histoBottom - histoY;		// shrinks with the space, never floored
	DrawStr( g, "dE distribution", fSmall, Gdiplus::RectF( 0.0f, (float)histoLblY, 220.0f, 18.0f ), t.dimtxt );
	int maxBin = 1;
	int b;
	for ( b = 0; b < 16; b++ )
		if ( m_stats.histo[b] > maxBin ) maxBin = m_stats.histo[b];
	int bw = max( 1, histoW / 16 );
	for ( b = 0; histoH > 2 && b < 16; b++ )
	{
		int bh = (int)( (double)m_stats.histo[b] / maxBin * ( histoH - 2 ) );
		if ( bh < 1 ) continue;
		int barTop = max( histoY, histoBottom - bh );	// never rise into the label band
		double binMid = ( b + 0.5 ) * m_stats.histoBinW;
		CRect rcBar( b * bw, barTop, b * bw + bw - 3, histoBottom );
		if ( rcBar.Height() > 4 )
			FillRound( g, rcBar, 2, DEColor( binMid, good, warn ) );
		else
		{
			Gdiplus::SolidBrush bb( DEColor( binMid, good, warn ) );
			g.FillRectangle( &bb, rcBar.left, rcBar.top, rcBar.Width(), rcBar.Height() );
		}
	}
	Gdiplus::Pen axPen( t.border, 1.0f );
	g.DrawLine( &axPen, 0, histoBottom, histoW, histoBottom );
	CString axM, axR;
	axM.Format( "%.3g (good)", good );
	axR.Format( "%.3g+ (warn)", warn );
	int axY = histoBottom + GetConfig()->Scale( 1 );
	DrawStr( g, "0", fSmall, Gdiplus::RectF( 0.0f, (float)axY, 40.0f, 16.0f ), t.dimtxt );
	DrawStr( g, axM, fSmall, Gdiplus::RectF( (float)( (int)( good / m_stats.histoBinW ) * bw - 45 ), (float)axY, 90.0f, 16.0f ), t.dimtxt, Gdiplus::StringAlignmentCenter );
	DrawStr( g, axR, fSmall, Gdiplus::RectF( (float)( histoW - 90 ), (float)axY, 90.0f, 16.0f ), t.dimtxt, Gdiplus::StringAlignmentFar );

	// ---- left column: region breakdown, one compact line ----
	CString regLine = "By region";
	for ( int r = 0; r < 4; r++ )
	{
		CString seg;
		if ( m_stats.regCnt[r] )
			seg.Format( "    %s %.1f / %.1f", kRegionNames[r], m_stats.regAvg[r], m_stats.regMax[r] );
		else
			seg.Format( "    %s -", kRegionNames[r] );
		regLine += seg;
	}
	DrawStr( g, regLine, fSmall, Gdiplus::RectF( 0.0f, (float)( axY + axisRowH ), (float)histoW, 18.0f ), t.dimtxt );

	// ---- right column: worst patches, clickable, fills the height ----
	int wy = 0;
	DrawStr( g, "Worst patches  -  click to inspect", fSmall,
			 Gdiplus::RectF( (float)rightX, (float)wy, (float)rightW, 18.0f ), t.dimtxt );
	wy += GetConfig()->Scale( 22 );
	int rowH = GetConfig()->Scale( 19 );
	for ( size_t w = 0; w < m_stats.worst.size(); w++ )
	{
		if ( wy + rowH > H )
			break;
		int pi = m_stats.worst[w];
		ColorRGBDisplay rgb = pMeasure->GetProfilePatchRGB( pi );
		double dE = PatchDE( pi );
		CRect rcRow( rightX, wy, CW, wy + rowH );
		m_rcWorstRows.push_back( std::make_pair( rcRow, pi ) );

		if ( m_hot == HOT_WORST_FIRST + (int)w )
			FillRound( g, rcRow, 4, t.cardHot );

		CRect rcSw( rightX + 3, wy + 3, rightX + rowH - 3, wy + rowH - 3 );
		FillRound( g, rcSw, 3, SwatchColor( rgb ) );
		DrawRound( g, rcSw, 3, t.border, 1.0f );

		CString row;
		row.Format( "RGB %.0f, %.0f, %.0f", rgb[0], rgb[1], rgb[2] );
		DrawStr( g, row, fSmall, Gdiplus::RectF( (float)( rightX + rowH + 6 ), (float)wy + 1, (float)( rightW - rowH - 60 ), 17.0f ), t.text );
		CString de;
		de.Format( "%.1f", dE );
		DrawStr( g, de, fSmall, Gdiplus::RectF( (float)( CW - 50 ), (float)wy + 1, 48.0f, 17.0f ),
				 DETextColor( dE, good, warn, dark ), Gdiplus::StringAlignmentFar );
		wy += rowH;
	}
}

// clientPt is in pane client coords (for the chrome buttons); the body rects
// live in content coords, so shift a copy by (m_contentDX, m_contentDY).
int CProfilePane::HotFromPoint(CPoint clientPt) const
{
	// chrome buttons first (client coords, present in every state)
	if ( !m_rcRefs.IsRectEmpty() && m_rcRefs.PtInRect( clientPt ) )
		return HOT_REFS;
	if ( !m_rcCtx.IsRectEmpty() && m_rcCtx.PtInRect( clientPt ) )
		return HOT_CTX;
	if ( !m_rcClear.IsRectEmpty() && m_rcClear.PtInRect( clientPt ) )
		return HOT_CLEAR;

	CPoint pt( clientPt.x - m_contentDX, clientPt.y - m_contentDY );	// content space
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
			if ( m_rcStop.PtInRect( pt ) )
				return HOT_STOP;
			break;

		case PS_SUMMARY:
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

// Perform the action for an element the user pressed AND released over.
void CProfilePane::ActivateHot(int id)
{
	if ( id >= HOT_PRESET_FIRST && id <= HOT_PRESET_LAST )
	{
		if ( id != m_preset )
		{
			m_preset = id;
			GetConfig()->WriteProfileInt("MainView", "Profile Preset", m_preset);
			Invalidate( FALSE );
		}
		return;
	}
	if ( id >= HOT_WORST_FIRST )
	{
		int w = id - HOT_WORST_FIRST;
		if ( w >= 0 && w < (int)m_rcWorstRows.size() )
			SendAction( PA_INSPECT, m_rcWorstRows[w].second );
		return;
	}
	switch ( id )
	{
		case HOT_START: SendAction( PA_START ); break;
		case HOT_PAUSE: SendAction( PA_PAUSE ); break;
		case HOT_STOP:  SendAction( PA_STOP );  break;
		case HOT_REFS:  SendAction( PA_REFS );  break;
		case HOT_CLEAR: SendAction( PA_CLEAR ); break;
		case HOT_CTX:
			// "New profile..." (summary) -> setup, or "Back to summary" (setup) ->
			// summary. Nothing is destroyed until a capture actually starts.
			m_state = ( m_state == PS_SUMMARY ) ? PS_SETUP : PS_SUMMARY;
			SyncChildren();
			Invalidate( FALSE );
			break;
	}
}

void CProfilePane::OnLButtonDown(UINT nFlags, CPoint point)
{
	// Record what was pressed and capture the mouse; the action fires on release
	// (OnLButtonUp) only if the cursor is still over the same element.
	m_pressed = HotFromPoint( point );
	if ( m_pressed != HOT_NONE )
		SetCapture();
	CWnd::OnLButtonDown( nFlags, point );
}

void CProfilePane::OnLButtonUp(UINT nFlags, CPoint point)
{
	if ( GetCapture() == this )
		ReleaseCapture();
	int up = HotFromPoint( point );
	if ( up != HOT_NONE && up == m_pressed )
		ActivateHot( up );
	m_pressed = HOT_NONE;
	CWnd::OnLButtonUp( nFlags, point );
}
