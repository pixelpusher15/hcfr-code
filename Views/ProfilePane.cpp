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
static const struct { int cubeN; UINT nameId; } kPresets[5] =
{
	{ 5,  IDS_PP_PRESET_QUICK     },
	{ 9,  IDS_PP_PRESET_STANDARD  },
	{ 11, IDS_PP_PRESET_FINE      },
	{ 17, IDS_PP_PRESET_REFERENCE },
	{ 21, IDS_PP_PRESET_MAXIMUM   },
};

// Colour-area rows: the neutral axis first, then the hue circle from red.
// The English "Gray" matches the spelling the rest of the UI already uses
// (grayscale mode, the measures grid) rather than introducing a second one.
static const UINT kAreaFamIds[7] = { IDS_PP_FAM_GRAY, IDS_PP_FAM_RED, IDS_PP_FAM_YELLOW,
									 IDS_PP_FAM_GREEN, IDS_PP_FAM_CYAN, IDS_PP_FAM_BLUE,
									 IDS_PP_FAM_MAGENTA };

// Every visible string in this pane comes from the string table, so all five
// satellite DLLs carry a translation (CLAUDE.md: localize in the same change).
static CString S(UINT id)
{
	CString s;
	s.LoadString( id );
	return s;
}

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
	, m_filterFam(-1)
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
	m_rcFilterChip.SetRectEmpty();
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
	m_chkGrayExtras.Create( S( IDS_PP_CHK_GRAYEXTRAS ),
							WS_CHILD | BS_AUTOCHECKBOX, rcInit, this, IDC_PP_GRAYEXTRAS );
	m_chkDriftComp.Create( S( IDS_PP_CHK_DRIFTCOMP ),
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
	// the data underneath changed; a filter carried over from the previous capture
	// would silently hide most of the new one
	m_filterFam = -1;
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
		s.Format( S( IDS_PP_MINUTES ), max( 1, (int)( secs / 60.0 + 0.5 ) ) );
	else
		s.Format( S( IDS_PP_HOURS ), secs / 3600.0 );
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

// Dismiss mark as two crossed lines, not a font glyph -- the same reasoning as
// DrawMediaShape further down: exact centring on a point, no dependency on Segoe
// Fluent Icons being installed, and no MBCS codepoint round-trip. The Fluent
// glyph also rendered visibly high, since its em box is not centred on the mark.
static void DrawCloseMark(Gdiplus::Graphics & g, int cx, int cy, int s, const Gdiplus::Color & clr)
{
	Gdiplus::Pen pen( clr, max( 1.0f, (float)s / 7.0f ) );
	pen.SetStartCap( Gdiplus::LineCapRound );
	pen.SetEndCap( Gdiplus::LineCapRound );
	float h = (float)s / 2.0f;
	g.DrawLine( &pen, (float)cx - h, (float)cy - h, (float)cx + h, (float)cy + h );
	g.DrawLine( &pen, (float)cx - h, (float)cy + h, (float)cx + h, (float)cy - h );
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
	m_rcFilterChip.SetRectEmpty();
	m_rcWorstRows.clear();
	m_rcAreaHits.clear();

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
			s = S( IDS_PP_SETUPSTATUS );
			break;

		case PS_RUNNING:
			if ( pMeasure )
			{
				int n = pMeasure->GetProfileCubeSize();
				s.Format( S( IDS_PP_RUNSTATUS ), n, n, n,
						  (LPCTSTR)( pMeasure->GetProfileGrayExtras() ? S( IDS_PP_GRAYNEARBLACK ) : CString() ),
						  (LPCTSTR)( m_driftComp ? S( IDS_PP_DRIFTCOMPON ) : CString() ),
						  (LPCTSTR)( m_paused ? S( IDS_PP_PAUSED ) : CString() ) );
			}
			break;

		case PS_SUMMARY:
			if ( pMeasure )
			{
				int n = pMeasure->GetProfileCubeSize();
				// Report the drift that was actually MEASURED rather than just that
				// the option was on. The anchors are serialized with the capture, so
				// this still reads correctly on a reloaded document.
				CString drift;
				if ( pMeasure->GetProfileDriftComp() )
				{
					double d = ProfileWhiteDrift();
					if ( d != 0.0 )
						drift.Format( S( IDS_PP_WHITEDRIFT ), d * 100.0 );
					else
						drift = S( IDS_PP_DRIFTCOMPSHORT );
				}
				s.Format( S( IDS_PP_SUMSTATUS ), n, n, n,
						  (LPCTSTR)( pMeasure->GetProfileGrayExtras() ? S( IDS_PP_GRAYNEARBLACK ) : CString() ),
						  m_stats.count, pMeasure->GetProfileMeasureSize(),
						  (LPCTSTR)FormatDuration( pMeasure->GetProfileCaptureSeconds() ),
						  (LPCTSTR)drift );
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
	DrawStr( g, S( IDS_PP_TITLE ), fTitle,
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
	CString refsLbl = S( IDS_PP_REFS );
	int refsW = ContentButtonWidth( g, refsLbl, fBtn, iconSz, padX );
	m_rcRefs = CRect( client.right - pad - refsW, top, client.right - pad, top + btnH );
	DrawIconButton( g, m_rcRefs, refsLbl, fBtn, m_hRefIcon, 0, NULL, iconSz,
					m_hot == HOT_REFS ? t.cardHot : t.card, t.text, t.btnBorder );

	// per-state context button to the left of References, width fitted to its label
	m_ctxLabel.Empty();
	if ( m_state == PS_SUMMARY )
		m_ctxLabel = S( IDS_PP_NEWPROFILE );
	else if ( m_state == PS_SETUP && Measure() && Measure()->HasProfileMeasures() )
		m_ctxLabel = S( IDS_PP_BACKTOSUMMARY );

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
		CString delLbl = S( IDS_PP_DELETE );
			int delW = ContentButtonWidth( g, delLbl, fBtn, iconSz, padX );
		m_rcClear = CRect( m_rcCtx.left - GetConfig()->Scale( 8 ) - delW, top,
						   m_rcCtx.left - GetConfig()->Scale( 8 ), top + btnH );
		DrawIconButton( g, m_rcClear, delLbl, fBtn, NULL, (wchar_t)0xE74D, &fGlyph, iconSz,	// Fluent "Delete"
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
		line1 = S( kPresets[p].nameId );
		line2.Format( S( IDS_PP_CUBEFMT ), kPresets[p].cubeN, kPresets[p].cubeN, kPresets[p].cubeN );
		line3.Format( S( IDS_PP_PATCHESTIME ), patches, (LPCTSTR)FormatDuration( EstimateSeconds( patches ) ) );
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
	DrawIconButton( g, m_rcStart, S( IDS_PP_STARTPROFILE ), fBody, NULL, (wchar_t)0xE768, &fGlyph,	// Fluent "Play"
					GetConfig()->Scale( 14 ),
					m_hot == HOT_START ? t.btnFaceHot : t.btnFace, t.btnText, t.borderSel );

	int patches = PatchCountFor( m_preset );
	CString help;
	help.Format( S( IDS_PP_PATCHESTIME ), patches, (LPCTSTR)FormatDuration( EstimateSeconds( patches ) ) );
	if ( hasOld )
		help += S( IDS_PP_REPLACESEXISTING );
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
	line.Format( S( IDS_PP_PATCHOF ), min( cur + 1, total ), total,
				 cur_rgb[0], cur_rgb[1], cur_rgb[2] );
	DrawStr( g, line, fBody, Gdiplus::RectF( (float)barX, (float)y, (float)( barW - etaW - GetConfig()->Scale( 8 ) ), 22.0f ), t.text );

	double frac = total > 0 ? (double)cur / total : 0.0;
	int remain = total - cur;
	CString eta;
	eta.Format( S( IDS_PP_ETA ), (int)( frac * 100.0 + 0.5 ),
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
	lbl[0] = S( IDS_PP_AVGDE );  lbl[1] = S( IDS_PP_MAXDE ); lbl[2] = S( IDS_PP_DRIFT ); lbl[3] = S( IDS_PP_PERPATCH );
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
		v[2] = S( IDS_PP_OFF );
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
	DrawMediaButton( g, m_rcStop, S( IDS_PP_STOP ), fBody, MEDIA_STOP, t.danger, glyphSz,
					 m_hot == HOT_STOP ? t.cardHot : t.card, t.danger, t.danger );
	DrawMediaButton( g, m_rcPause, m_paused ? S( IDS_PP_RESUME ) : S( IDS_PP_PAUSE ), fBody,
					 m_paused ? MEDIA_PLAY : MEDIA_PAUSE, t.text, glyphSz,
					 m_hot == HOT_PAUSE ? t.cardHot : t.card, t.text, t.btnBorder );
}

// Hue family of a generated patch stimulus: 0 neutral, then 1..6 walking the hue
// circle from red in 60-degree sectors (R Y G C B M). Taken from the STIMULUS and
// not from the measurement, so it is exact and costs nothing: the cube's neutral
// nodes have r == g == b bit-for-bit, and every other node falls unambiguously
// inside one sector.
static int HueFamily(const ColorRGBDisplay & rgb)
{
	double r = rgb[0], g = rgb[1], b = rgb[2];
	double mx = max( r, max( g, b ) );
	double mn = min( r, min( g, b ) );
	double d = mx - mn;
	if ( d < 1e-6 )
		return 0;						// neutral axis (0,0,0 included)

	double h;							// hue in degrees, red at 0
	if ( mx == r )		h = 60.0 * ( ( g - b ) / d );
	else if ( mx == g )	h = 60.0 * ( ( b - r ) / d + 2.0 );
	else				h = 60.0 * ( ( r - g ) / d + 4.0 );
	if ( h < 0.0 ) h += 360.0;

	// Sectors are CENTRED on the primaries/secondaries rather than starting at
	// them, so shift by half a sector before bucketing: 330..30 is red, 30..90 is
	// yellow, and so on. Without the shift a pure red patch would sit on a
	// boundary and split arbitrarily between red and magenta.
	return 1 + ( (int)( ( h + 30.0 ) / 60.0 ) % 6 );
}

// Brightness band from the stimulus max channel, matching the "<35%" / ">70%"
// column headers. Both comparisons are strict, so the 11- and 21-cubes -- whose
// grids land a node exactly on 35 and 70 -- put those nodes in Mid. Deterministic
// either way; it just has to agree with what the headers claim.
static int ToneBand(const ColorRGBDisplay & rgb)
{
	double mx = max( rgb[0], max( rgb[1], rgb[2] ) );
	if ( mx < 35.0 ) return 0;
	if ( mx > 70.0 ) return 2;
	return 1;
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
	m_stats.sorted.reserve( n );
	m_stats.bucket.assign( n, (signed char)-1 );
	int nGoodCnt = 0;
	double sumL2 = 0.0, sumC2 = 0.0, sumH2 = 0.0;

	for ( int i = 0; i < n; i++ )
	{
		// one call for the total AND its breakdown: the reference model behind it
		// (GetRefProfileSat) is the expensive part and is shared
		CMeasure::ProfileDEParts parts;
		double dE = pMeasure->ComputeProfileDEEx( pMeasure->GetProfileMeasure( i ), i, &parts );
		if ( dE < 0.0 )
			continue;
		des.push_back( dE );
		m_stats.sorted.push_back( std::make_pair( -dE, i ) );
		m_stats.avgDE += dE;
		if ( dE > m_stats.maxDE ) m_stats.maxDE = dE;
		if ( dE < good ) nGoodCnt++;
		sumL2 += parts.dL * parts.dL;
		sumC2 += parts.dC * parts.dC;
		sumH2 += parts.dH * parts.dH;

		// clamp BOTH sides: an out-of-range float->int cast is undefined and can
		// go hugely negative, and histo[negative]++ corrupts memory
		int bin = (int)( dE / m_stats.histoBinW );
		if ( bin < 0 ) bin = 0;
		if ( bin > 15 ) { bin = 15; m_stats.histoOver++; }	// counted so the axis can say so
		m_stats.histo[bin]++;

		// colour-area classification from the generated stimulus
		ColorRGBDisplay rgb = pMeasure->GetProfilePatchRGB( i );
		int fam = HueFamily( rgb ), band = ToneBand( rgb );
		m_stats.bucket[i] = (signed char)( fam * AREA_BANDS + band );
		m_stats.areaAvg[fam][band] += dE;
		m_stats.areaCnt[fam][band]++;
		m_stats.famCnt[fam]++;
	}

	m_stats.count = (int)des.size();
	if ( m_stats.count )
	{
		m_stats.avgDE /= m_stats.count;
		m_stats.pctGood = (double)nGoodCnt / m_stats.count;
		std::vector<double> sortedDE( des );
		std::sort( sortedDE.begin(), sortedDE.end() );
		int i95 = (int)( 0.95 * ( sortedDE.size() - 1 ) + 0.5 );
		m_stats.pct95DE = sortedDE[i95];
		// RMS, not mean: the components combine in quadrature, so this is the form
		// in which they add back up to the overall error
		m_stats.rmsL = sqrt( sumL2 / m_stats.count );
		m_stats.rmsC = sqrt( sumC2 / m_stats.count );
		m_stats.rmsH = sqrt( sumH2 / m_stats.count );
	}
	for ( int f = 0; f < AREA_FAMS; f++ )
		for ( int b = 0; b < AREA_BANDS; b++ )
			if ( m_stats.areaCnt[f][b] )
				m_stats.areaAvg[f][b] /= m_stats.areaCnt[f][b];

	std::sort( m_stats.sorted.begin(), m_stats.sorted.end() );
}

// Filter predicate for the worst list. Reads the per-patch bucket ComputeStats
// already assigned, so filtering never re-derives a classification or a dE.
bool CProfilePane::PatchPassesFilter(int patchIdx) const
{
	if ( m_filterFam < 0 )
		return true;
	if ( patchIdx < 0 || patchIdx >= (int)m_stats.bucket.size() )
		return false;
	int code = m_stats.bucket[patchIdx];
	if ( code < 0 )
		return false;					// skipped patch; never reaches the list anyway
	return ( code / AREA_BANDS == m_filterFam );
}

// Just the family name. SETTLED: the chip deliberately carries no patch count --
// the matrix's own totals row already shows it, directly under the highlighted
// column, and a count inside the chip only added a second rounded shape to a
// 90px control. Do not reintroduce it.
CString CProfilePane::FilterLabel() const
{
	return ( m_filterFam >= 0 ) ? S( kAreaFamIds[m_filterFam] ) : CString();
}

// Re-picking the family that is already active clears the filter, so the matrix
// doubles as its own toggle and there is always a way back to All even if the
// chip is off-screen in a narrow pane.
void CProfilePane::SetFilter(int fam)
{
	m_filterFam = ( fam == m_filterFam ) ? -1 : fam;
	if ( ::IsWindow( m_hWnd ) )
		Invalidate( FALSE );
}

// Measured white drift across the capture: last drift anchor over the first.
// Anchors only exist when drift compensation actually ran, and they are
// serialized with the capture, so this still reports on a reloaded document.
// The measurements have ALREADY been corrected by this factor -- the number says
// how far the display moved, not how much error is left behind.
double CProfilePane::ProfileWhiteDrift() const
{
	CMeasure * pMeasure = Measure();
	if ( !pMeasure || pMeasure->GetProfileDriftAnchorCount() < 2 )
		return 0.0;
	CColor first = pMeasure->GetProfileDriftAnchor( 0 );
	CColor last  = pMeasure->GetProfileDriftAnchor( pMeasure->GetProfileDriftAnchorCount() - 1 );
	if ( !first.isValid() || !last.isValid() || first.GetY() <= 0.0 )
		return 0.0;
	return last.GetY() / first.GetY() - 1.0;
}

// Round a bin count up to a round number so the histogram's gridlines carry
// readable labels instead of whatever the tallest bin happened to hold.
// Works on TENTHS of the leading digit so 1.5x and 2.5x steps exist: with only
// 1/2/5 available, 124 rounded to 200 and the tallest bar filled 62% of the plot,
// which makes the count look far smaller than it is. 124 now rounds to 150.
static int NiceCeil(int v)
{
	if ( v <= 5 ) return 5;
	int mag = 1;
	while ( v / mag >= 10 ) mag *= 10;
	// Pick the first candidate that actually covers v. Deriving the step from a
	// truncated leading digit instead returned a "ceiling" BELOW its input for any
	// v just past a power of ten (101 -> 100, 1001 -> 1000), which drew the tallest
	// bar past the top of the plot and mislabelled the axis.
	static const int kSteps10[] = { 10, 15, 20, 25, 30, 40, 50, 60, 80, 100 };
	for ( int i = 0; i < 10; i++ )
	{
		int cand = ( kSteps10[i] * mag ) / 10;
		if ( cand >= v )
			return cand;
	}
	return mag * 10;
}

// Category colours for the error-type split. Deliberately NOT the good/warn/bad
// ramp: in this pane green/amber/red mean SEVERITY everywhere else, so reusing
// them made "Luminance" look healthy and "Hue" look broken no matter what the
// numbers were. A single-hue sequence reads as three parts of one quantity.
static Gdiplus::Color ErrPartColor(int i, bool dark)
{
	if ( dark )
	{
		if ( i == 0 ) return Gdiplus::Color( 122, 170, 226 );
		if ( i == 1 ) return Gdiplus::Color(  86, 126, 178 );
		return Gdiplus::Color( 58, 86, 124 );
	}
	if ( i == 0 ) return Gdiplus::Color(  38, 110, 190 );
	if ( i == 1 ) return Gdiplus::Color( 104, 152, 212 );
	return Gdiplus::Color( 162, 194, 230 );
}

// Column hover in the colour-area matrix: a faint wash rather than a filled card.
// The band spans a heading, three cells and a total, and at cardHot's brightness a
// hover read like a selection.
static Gdiplus::Color ColumnHover(bool dark)
{
	return dark ? Gdiplus::Color( 24, 255, 255, 255 ) : Gdiplus::Color( 18, 0, 0, 0 );
}

// Cell wash for the colour-area matrix: the shared dE ramp at low alpha, so the
// number stays the primary signal and the tint is only a scan aid. Alpha rises
// with severity -- at a flat alpha a green and a red cell read equally loud.
static Gdiplus::Color AreaTint(double dE, double good, double warn)
{
	Gdiplus::Color c = DEColor( dE, good, warn );
	BYTE a = ( dE < good ) ? 70 : ( ( dE < warn ) ? 112 : 140 );
	return Gdiplus::Color( a, c.GetR(), c.GetG(), c.GetB() );
}

// Measured width of a string. Every label in this pane is English today but the
// app ships four translated satellites, where these strings get materially longer
// (German especially). Layout here is therefore driven by what the text actually
// measures rather than by constants eyeballed against the English build.
static int TextW(Gdiplus::Graphics & g, const CString & s, Gdiplus::Font & f)
{
	Gdiplus::RectF bb;
	CStringW w( s );
	g.MeasureString( w, -1, &f, Gdiplus::PointF( 0.0f, 0.0f ), &bb );
	return (int)( bb.Width + 0.5f );
}

// Row labels for the matrix's brightness bands, with the bounds spelled in so the
// split is self-documenting. One definition so measuring and drawing agree.
static CString BandLabel(int bnd)
{
	if ( bnd == 0 ) return S( IDS_PP_BAND_DARK );
	if ( bnd == 2 ) return S( IDS_PP_BAND_BRIGHT );
	return S( IDS_PP_BAND_MID );
}

// Row chips for the colour-area families, muted so they read as identity marks
// instead of competing with the dE ramp in the cells beside them.
static const struct { BYTE r, g, b; } kAreaFamSwatch[7] =
{
	{ 154, 154, 158 },	// gray
	{ 204,  77,  77 },	// red
	{ 204, 192,  77 },	// yellow
	{  90, 168,  79 },	// green
	{  77, 188, 192 },	// cyan
	{  77, 122, 204 },	// blue
	{ 176,  77, 188 }	// magenta
};

void CProfilePane::PaintSummary(Gdiplus::Graphics & g, const CRect & rc, bool dark)
{
	SPaneTheme t = PaneTheme( dark );
	Gdiplus::Font fBig( L"Segoe UI", 12.0f, Gdiplus::FontStyleBold );
	Gdiplus::Font fSmall( L"Segoe UI", 9.0f );
	Gdiplus::Font fTiny( L"Segoe UI", 8.5f );
	Gdiplus::Font fHdr( L"Segoe UI", 9.0f, Gdiplus::FontStyleBold );	// panel headings

	CMeasure * pMeasure = Measure();
	if ( !pMeasure )
		return;
	ComputeStats();

	double good = 2.0, warn = 3.0;
	GetConfig()->GetDEThresholds( good, warn );

	const int CW = rc.Width();
	const int H  = rc.Height();
	const int gap = GetConfig()->Scale( 9 );

	if ( m_stats.count <= 0 )
	{
		DrawStr( g, S( IDS_PP_NOMEASUREMENTS ), fSmall,
				 Gdiplus::RectF( 0.0f, 0.0f, (float)CW, 20.0f ), t.dimtxt );
		return;
	}

	// ------------------------------------------------ tile row
	int tileH = GetConfig()->Scale( 46 );
	CString v[4], lbl[4];
	lbl[0] = S( IDS_PP_AVGDE );
	lbl[1] = S( IDS_PP_95THPCT );
	lbl[2] = S( IDS_PP_MAXDE );
	// Spell the threshold into the label. "Within target (2)" left the 2 unexplained
	// -- it is the good-dE threshold, so say that outright.
	lbl[3].Format( S( IDS_PP_WITHINDE ), good );
	v[0].Format( "%.1f", m_stats.avgDE );
	v[1].Format( "%.1f", m_stats.pct95DE );
	v[2].Format( "%.1f", m_stats.maxDE );
	v[3].Format( "%d%%", (int)( m_stats.pctGood * 100.0 + 0.5 ) );

	int i, x = 0, y = 0;
	int tpad = GetConfig()->Scale( 10 );

	// Tiles are as wide as their WIDEST caption needs, not a fixed share of the
	// pane, so a longer translation grows the tile instead of being clipped. The
	// error-type chip absorbs the difference and keeps a floor of its own.
	int tileNeed = 0;
	for ( i = 0; i < 4; i++ )
		tileNeed = max( tileNeed, TextW( g, lbl[i], fSmall ) );
	tileNeed += 2 * tpad;
	int tileW = max( tileNeed, ( CW - CW * 34 / 100 - 4 * gap ) / 4 );
	int tileMax = ( CW - GetConfig()->Scale( 150 ) - 4 * gap ) / 4;	// leave the chip its minimum
	if ( tileW > tileMax ) tileW = tileMax;
	if ( tileW < GetConfig()->Scale( 62 ) ) tileW = GetConfig()->Scale( 62 );
	for ( i = 0; i < 4; i++ )
	{
		CRect rcTile( x, y, x + tileW, y + tileH );
		FillRound( g, rcTile, 6, t.card );		// filled chip, no border (these are stats, not buttons)
		DrawStr( g, lbl[i], fSmall,
				 Gdiplus::RectF( (float)( x + tpad ), (float)( y + GetConfig()->Scale( 5 ) ),
								 (float)( tileW - 2 * tpad + GetConfig()->Scale( 4 ) ), 16.0f ), t.dimtxt );
		Gdiplus::Color vc = t.text;
		if ( i == 2 && m_stats.maxDE >= warn ) vc = t.danger;
		DrawStr( g, v[i], fBig,
				 Gdiplus::RectF( (float)( x + tpad ), (float)( y + GetConfig()->Scale( 20 ) ),
								 (float)( tileW - 2 * tpad ), 24.0f ), vc );
		x += tileW + gap;
	}

	// Error-type chip: the RMS split of the SAME dE the tiles report. RMS and not
	// the mean because the components combine in quadrature -- this is the form in
	// which they add back up to the overall error.
	int errX = x, errW = CW - errX;
	if ( errW >= GetConfig()->Scale( 150 ) )
	{
		CRect rcErr( errX, y, CW, y + tileH );
		FillRound( g, rcErr, 6, t.card );

		// Under CIE76uv/ab the formula's 2nd and 3rd terms are du/dv and da/db, not
		// chroma and hue (see CMeasure::ComputeProfileDEEx), so there they collapse
		// into one honest "Color" term rather than being mislabelled.
		int deForm = GetConfig()->m_dE_form;
		bool splitCH = ( deForm >= 2 && deForm <= 5 );

		double parts[3];
		CString pl[3];
		int nParts;
		if ( splitCH )
		{
			parts[0] = m_stats.rmsL; parts[1] = m_stats.rmsC; parts[2] = m_stats.rmsH;
			pl[0].Format( S( IDS_PP_LUMINANCE ), parts[0] );
			pl[1].Format( S( IDS_PP_CHROMA ), parts[1] );
			pl[2].Format( S( IDS_PP_HUE ), parts[2] );
			nParts = 3;
		}
		else
		{
			parts[0] = m_stats.rmsL;
			parts[1] = sqrt( m_stats.rmsC * m_stats.rmsC + m_stats.rmsH * m_stats.rmsH );
			parts[2] = 0.0;
			pl[0].Format( S( IDS_PP_LUMINANCE ), parts[0] );
			pl[1].Format( S( IDS_PP_COLORTERM ), parts[1] );
			nParts = 2;
		}
		int capW = TextW( g, S( IDS_PP_ERRORTYPE ), fSmall ) + GetConfig()->Scale( 4 );
		DrawStr( g, S( IDS_PP_ERRORTYPE ), fSmall,
				 Gdiplus::RectF( (float)( errX + tpad ), (float)( y + GetConfig()->Scale( 5 ) ), (float)capW, 16.0f ), t.dimtxt );

		// Legend: a colour dot keyed to the bar segment, then the label in normal
		// text. Colouring the LABEL was unreadable at the pale end of the sequence
		// and, before that, borrowed the severity ramp and implied a verdict.
		int dotSz = GetConfig()->Scale( 7 );
		int lw[3], totalLw = 0, legendGap = GetConfig()->Scale( 12 ), p;
		for ( p = 0; p < nParts; p++ )
		{
			lw[p] = dotSz + GetConfig()->Scale( 4 ) + TextW( g, pl[p], fTiny );
			totalLw += lw[p];
		}
		totalLw += legendGap * ( nParts - 1 );
		int lx = CW - tpad - totalLw;
		if ( lx > errX + tpad + capW )
		{
			// Dot and label share one band and are both centred on it, rather than
			// the label being top-aligned in a raw 16px box while the dot sits at a
			// hardcoded +8 -- those two drift apart as soon as DPI scaling makes the
			// text taller than the box.
			int legTop = y + GetConfig()->Scale( 5 );
			int legH   = GetConfig()->Scale( 16 );
			int lmid   = legTop + legH / 2;
			for ( p = 0; p < nParts; p++ )
			{
				FillRound( g, CRect( lx, lmid - dotSz / 2, lx + dotSz, lmid - dotSz / 2 + dotSz ), 2,
						   ErrPartColor( p, dark ) );
				DrawStr( g, pl[p], fTiny,
						 Gdiplus::RectF( (float)( lx + dotSz + GetConfig()->Scale( 4 ) ), (float)legTop,
										 (float)( lw[p] + 4 ), (float)legH ), t.text,
						 Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter );
				lx += lw[p] + legendGap;
			}
		}

		// stacked bar, segment widths by share of SQUARED error
		int barY = y + GetConfig()->Scale( 26 ), barH = GetConfig()->Scale( 12 );
		int barX = errX + tpad, barW = CW - tpad - barX;
		CRect rcBarAll( barX, barY, barX + barW, barY + barH );
		FillRound( g, rcBarAll, 3, t.track );
		double sum2 = 0.0;
		for ( p = 0; p < nParts; p++ ) sum2 += parts[p] * parts[p];
		// Below the precision the labels print at, the split is pure noise: a
		// capture whose components are 0.02 / 0.04 / 0.03 would still fill the bar
		// edge to edge, reading as a large error while every label says 0.0. Leave
		// the track empty instead -- there is no error to apportion.
		if ( sqrt( sum2 ) >= 0.05 && barW > 4 )
		{
			// clip to the rounded track so the segments read as one bar
			Gdiplus::GraphicsPath clipPath;
			RoundPath( clipPath, rcBarAll, 3 );
			Gdiplus::GraphicsState st = g.Save();
			g.SetClip( &clipPath );
			int px = barX;
			for ( p = 0; p < nParts; p++ )
			{
				int segW = ( p == nParts - 1 ) ? ( barX + barW - px )
											   : (int)( barW * ( parts[p] * parts[p] ) / sum2 + 0.5 );
				if ( segW > 0 )
				{
					Gdiplus::SolidBrush sb( ErrPartColor( p, dark ) );
					g.FillRectangle( &sb, px, barY, segW, barH );
				}
				px += segW;
			}
			g.Restore( st );
		}
	}

	// ------------------------------------------------ three panels below the tiles
	// Scale(8), not Scale(10): the matrix column headings need the reclaimed room
	// for the colour key rule to clear the first row of cells. rowsY below adds it
	// back, so the first data row lands in exactly the same absolute place.
	int bodyY = tileH + GetConfig()->Scale( 8 );
	int bodyH = H - bodyY;
	if ( bodyH < GetConfig()->Scale( 40 ) )
		return;								// too short for anything but the tiles

	int pgap  = GetConfig()->Scale( 14 );
	int avail = CW - 2 * pgap;

	// ---- panel widths from measured content, not fixed percentages ----
	// The matrix is the constrained one: seven column headings plus a row-label
	// gutter, all of which are translatable. Size it from what it needs, cap it so
	// it can never eat the pane, then split the remainder.
	int cellGap = GetConfig()->Scale( 4 );
	int mLabNeed = TextW( g, S( IDS_PP_PATCHES ), fTiny );
	int bnd0;
	for ( bnd0 = 0; bnd0 < AREA_BANDS; bnd0++ )
		mLabNeed = max( mLabNeed, TextW( g, BandLabel( bnd0 ), fTiny ) );
	mLabNeed += GetConfig()->Scale( 8 );
	int mCellNeed = TextW( g, "999.9", fSmall );		// dE is not bounded at two digits
	int f0;
	for ( f0 = 0; f0 < AREA_FAMS; f0++ )
		mCellNeed = max( mCellNeed, TextW( g, S( kAreaFamIds[f0] ), fTiny ) );
	mCellNeed += GetConfig()->Scale( 8 );
	int bNeed = mLabNeed + AREA_FAMS * mCellNeed + ( AREA_FAMS - 1 ) * cellGap;

	int bW = min( bNeed, avail * 45 / 100 );
	int rest = avail - bW;
	int aW = rest * 38 / 100;
	int aMin = GetConfig()->Scale( 150 );
	if ( aW < aMin ) aW = min( aMin, rest / 2 );
	int aX = 0;
	int bX = aX + aW + pgap;
	int cX = bX + bW + pgap;
	int cW = CW - cX;
	int hdrH  = GetConfig()->Scale( 16 );
	// First matrix row: clears its column headings and the colour key rule under
	// them. The worst list does NOT share this -- it has only a one-line header, and
	// starting it this far down cost it two of its five rows.
	int rowsY = bodyY + GetConfig()->Scale( 36 );

	// ---------------- panel A: dE distribution ----------------
	{
		// No good/warn key here: this panel is the narrowest of the three and the
		// key never had room, so it rendered as clipped stubs. The dashed rules are
		// already labelled by their own axis ticks below, warn in the danger colour.
		DrawStr( g, S( IDS_PP_DEDISTRIBUTION ), fHdr,
				 Gdiplus::RectF( (float)aX, (float)bodyY, (float)aW, (float)hdrH ), t.text );
		// no sub-label here: this panel is the narrowest and the heading alone can
		// already need the full width once translated

		// Ceiling FIRST, then a gutter measured from the widest label it must hold.
		// A hardcoded Scale(22) gutter ellipsised three-digit counts: a 124-patch
		// bar against a "200" axis rendered as "20", making the bar read as 12.
		int b, maxBin = 1;
		for ( b = 0; b < 16; b++ )
			if ( m_stats.histo[b] > maxBin ) maxBin = m_stats.histo[b];
		int ceilCnt = NiceCeil( maxBin );
		CString widest;
		widest.Format( "%d", ceilCnt );
		int gutter = TextW( g, widest, fTiny ) + GetConfig()->Scale( 7 );
		int plotX = aX + gutter, plotW = aW - gutter;
		int plotTop = bodyY + GetConfig()->Scale( 18 );
		int baseY = bodyY + bodyH - GetConfig()->Scale( 15 );
		int plotH = baseY - plotTop;

		if ( plotH > GetConfig()->Scale( 20 ) && plotW > 32 )
		{
			Gdiplus::Pen gridPen( t.border, 1.0f );
			int gl;
			for ( gl = 0; gl < 2; gl++ )
			{
				int cnt = ( gl == 0 ) ? ceilCnt : ceilCnt / 2;
				int gy = baseY - (int)( (double)cnt / ceilCnt * plotH );
				g.DrawLine( &gridPen, plotX, gy, plotX + plotW, gy );
				CString cl;
				cl.Format( "%d", cnt );
				DrawStr( g, cl, fTiny, Gdiplus::RectF( (float)aX, (float)( gy - 7 ), (float)( gutter - 4 ), 14.0f ),
						 t.dimtxt, Gdiplus::StringAlignmentFar );
			}

			int bw = max( 1, plotW / 16 );
			// Threshold rules at their TRUE fractional positions. This used to snap
			// them to a bin edge and then park the "warn" label at the far right of
			// the axis -- which is where 16*binW lands, not warn.
			int goodX = plotX + (int)( good / m_stats.histoBinW * bw + 0.5 );
			int warnX = plotX + (int)( warn / m_stats.histoBinW * bw + 0.5 );
			Gdiplus::Pen pGood( t.dimtxt, 1.0f ); pGood.SetDashStyle( Gdiplus::DashStyleDash );
			Gdiplus::Pen pWarn( t.danger, 1.0f ); pWarn.SetDashStyle( Gdiplus::DashStyleDash );
			if ( goodX > plotX && goodX < plotX + plotW )
				g.DrawLine( &pGood, goodX, plotTop, goodX, baseY + GetConfig()->Scale( 3 ) );
			if ( warnX > plotX && warnX < plotX + plotW )
				g.DrawLine( &pWarn, warnX, plotTop, warnX, baseY + GetConfig()->Scale( 3 ) );

			// the last bin collects everything above its range; say so
			if ( m_stats.histoOver > 0 )
			{
				Gdiplus::Pen pOver( t.dimtxt, 1.0f ); pOver.SetDashStyle( Gdiplus::DashStyleDot );
				g.DrawLine( &pOver, plotX + 15 * bw - 2, plotTop, plotX + 15 * bw - 2, baseY );
			}

			for ( b = 0; b < 16; b++ )
			{
				if ( m_stats.histo[b] <= 0 ) continue;
				int bh = (int)( (double)m_stats.histo[b] / ceilCnt * plotH + 0.5 );
				if ( bh < 2 ) bh = 2;			// a non-empty bin must stay visible
				int barTop = max( plotTop, baseY - bh );
				double binMid = ( b + 0.5 ) * m_stats.histoBinW;
				CRect rcBar( plotX + b * bw, barTop, plotX + b * bw + max( 1, bw - 3 ), baseY );
				if ( rcBar.Height() > 4 )
					FillRound( g, rcBar, 2, DEColor( binMid, good, warn ) );
				else
				{
					Gdiplus::SolidBrush bb( DEColor( binMid, good, warn ) );
					g.FillRectangle( &bb, rcBar.left, rcBar.top, rcBar.Width(), rcBar.Height() );
				}
			}

			Gdiplus::Pen axPen( t.border, 1.0f );
			g.DrawLine( &axPen, plotX, baseY, plotX + plotW, baseY );

			int axY = baseY + GetConfig()->Scale( 3 );
			CString ax;
			DrawStr( g, "0", fTiny, Gdiplus::RectF( (float)plotX, (float)axY, 24.0f, 14.0f ), t.dimtxt );
			// drop the good label rather than let it collide with warn on tight thresholds
			if ( goodX < plotX + plotW && warnX - goodX >= GetConfig()->Scale( 30 ) )
			{
				ax.Format( "%.3g", good );
				DrawStr( g, ax, fTiny, Gdiplus::RectF( (float)( goodX - 24 ), (float)axY, 48.0f, 14.0f ),
						 t.dimtxt, Gdiplus::StringAlignmentCenter );
			}
			if ( warnX < plotX + plotW )
			{
				ax.Format( "%.3g", warn );
				DrawStr( g, ax, fTiny, Gdiplus::RectF( (float)( warnX - 24 ), (float)axY, 48.0f, 14.0f ),
						 t.danger, Gdiplus::StringAlignmentCenter );
			}
			ax.Format( "%.3g%s", 16.0 * m_stats.histoBinW, ( m_stats.histoOver > 0 ) ? "+" : "" );
			DrawStr( g, ax, fTiny, Gdiplus::RectF( (float)( plotX + plotW - 44 ), (float)axY, 44.0f, 14.0f ),
					 ( m_stats.histoOver > 0 ) ? t.danger : t.dimtxt, Gdiplus::StringAlignmentFar );
		}
	}

	// ---------------- panel B: colour-area matrix ----------------
	{
		CString bHdr = S( IDS_PP_ERRBYCOLORAREA );
		int bHdrW = TextW( g, bHdr, fHdr );
		DrawStr( g, bHdr, fHdr,
				 Gdiplus::RectF( (float)bX, (float)bodyY, (float)min( bHdrW + 4, bW ), (float)hdrH ), t.text );
		// the unit sub-label yields to the heading rather than colliding with it
		CString bSub = S( IDS_PP_AVGDELC );
		int bSubW = TextW( g, bSub, fTiny );
		if ( bHdrW + GetConfig()->Scale( 10 ) + bSubW <= bW )
			DrawStr( g, bSub, fTiny,
					 Gdiplus::RectF( (float)( bX + bW - bSubW - 2 ), (float)bodyY, (float)( bSubW + 4 ), (float)hdrH ),
					 t.dimtxt, Gdiplus::StringAlignmentFar );

		// Families across the COLUMNS and brightness bands down the rows. The other
		// way round needed seven rows, and only four of them ever fitted the pane's
		// height -- cyan, blue and magenta simply never drew. Three rows always fit,
		// and this pane has width to spare.
		// Gutter and cells come from the measured needs computed above, so a longer
		// translated band label or family name widens its column instead of clipping.
		int labW = min( mLabNeed, bW / 3 );
		int cellW = ( bW - labW - ( AREA_FAMS - 1 ) * cellGap ) / AREA_FAMS;
		int mRowH = GetConfig()->Scale( 19 );

		if ( cellW >= GetConfig()->Scale( 30 ) )
		{
			int f, bnd;
			int cellX0 = bX + labW;
			int cntY = rowsY + AREA_BANDS * mRowH;
			bool showCnt = ( cntY + mRowH <= bodyY + bodyH );
			// Band starts BELOW the header row: at Scale(13) its top edge cut into
			// the heading and the "avg dE" label sitting on that line. It runs to the
			// full bottom of the totals row -- stopping Scale(2) short left the
			// selection looking cropped against the last row of numbers.
			int colTop = bodyY + hdrH;		// never inside the heading row
			int colHdrY = colTop + GetConfig()->Scale( 2 );
			int colBot = min( bodyY + bodyH,
							  ( showCnt ? cntY + mRowH : rowsY + AREA_BANDS * mRowH ) + GetConfig()->Scale( 1 ) );

			// A whole COLUMN is one target -- heading, three bands and the total.
			// Per-cell filtering sliced the worst list down to a row or two, which
			// is not a useful answer to "show me the bad greens".
			for ( f = 0; f < AREA_FAMS; f++ )
			{
				if ( m_stats.famCnt[f] <= 0 )
					continue;				// an empty family can never select anything
				int cx = cellX0 + f * ( cellW + cellGap );
				CRect rcCol( cx - GetConfig()->Scale( 2 ), colTop, cx + cellW + GetConfig()->Scale( 2 ), colBot );
				m_rcAreaHits.push_back( std::make_pair( rcCol, f ) );
				if ( m_filterFam == f )
				{
					FillRound( g, rcCol, 4, t.cardSel );
					DrawRound( g, rcCol, 4, t.borderSel, 1.0f );
				}
				else if ( m_hot == HOT_AREA_FIRST + f )
					FillRound( g, rcCol, 4, ColumnHover( dark ) );
			}

			for ( f = 0; f < AREA_FAMS; f++ )
			{
				int cx = cellX0 + f * ( cellW + cellGap );
				DrawStr( g, S( kAreaFamIds[f] ), fTiny,
						 Gdiplus::RectF( (float)cx, (float)colHdrY, (float)cellW, (float)GetConfig()->Scale( 13 ) ),
						 t.text, Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter );
				// family colour as a rule under its heading: a chip beside the text
				// would need centring maths and eat width the names already want
				int keyW = cellW * 3 / 5, keyX = cx + ( cellW - keyW ) / 2;
				int keyY = colHdrY + GetConfig()->Scale( 13 );
				FillRound( g, CRect( keyX, keyY, keyX + keyW, keyY + GetConfig()->Scale( 3 ) ), 1,
						   Gdiplus::Color( kAreaFamSwatch[f].r, kAreaFamSwatch[f].g, kAreaFamSwatch[f].b ) );
			}

			for ( bnd = 0; bnd < AREA_BANDS; bnd++ )
			{
				int ry = rowsY + bnd * mRowH;
				if ( ry + mRowH > bodyY + bodyH )
					break;
				// Everything on a row is centred on the SAME band, rather than each
				// element getting its own top offset and 14/16px rect: the row label
				// and the cell numbers use different point sizes, so top-aligning
				// them left their baselines visibly out of step.
				Gdiplus::RectF rcRowF( (float)bX, (float)ry, (float)( labW - GetConfig()->Scale( 4 ) ), (float)mRowH );
				DrawStr( g, BandLabel( bnd ), fTiny, rcRowF, t.dimtxt,
						 Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter );

				for ( f = 0; f < AREA_FAMS; f++ )
				{
					int cx = cellX0 + f * ( cellW + cellGap );
					// symmetric inset, so the tinted box is centred in the band too
					CRect rcCell( cx, ry + GetConfig()->Scale( 2 ), cx + cellW, ry + mRowH - GetConfig()->Scale( 2 ) );
					Gdiplus::RectF rcCellF( (float)cx, (float)rcCell.top, (float)cellW, (float)rcCell.Height() );
					if ( m_stats.areaCnt[f][bnd] > 0 )
					{
						double av = m_stats.areaAvg[f][bnd];
						FillRound( g, rcCell, 3, AreaTint( av, good, warn ) );
						CString cv;
						cv.Format( "%.1f", av );
						DrawStr( g, cv, fSmall, rcCellF, t.text,
								 Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter );
					}
					else
						DrawStr( g, "-", fSmall, rcCellF, t.dimtxt,
								 Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter );
				}
			}

			if ( showCnt )
			{
				// same centring rule as the band rows above
				DrawStr( g, S( IDS_PP_PATCHES ), fTiny,
						 Gdiplus::RectF( (float)bX, (float)cntY,
										 (float)( labW - GetConfig()->Scale( 4 ) ), (float)mRowH ), t.dimtxt,
						 Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter );
				for ( f = 0; f < AREA_FAMS; f++ )
				{
					int cx = cellX0 + f * ( cellW + cellGap );
					CString cs;
					if ( m_stats.famCnt[f] > 0 )
						cs.Format( "%d", m_stats.famCnt[f] );
					else
						cs = "-";
					DrawStr( g, cs, fTiny,
							 Gdiplus::RectF( (float)cx, (float)cntY, (float)cellW, (float)mRowH ), t.dimtxt,
							 Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter );
				}
			}
		}
	}

	// ---------------- panel C: worst patches ----------------
	{
		// Measured, not Scale(82): the fixed width clipped "Worst patches" to
		// "Worst patc..." in the English build, and every translation is longer.
		CString cHdr = S( IDS_PP_WORSTPATCHES );
		int hdrLabW = min( TextW( g, cHdr, fHdr ) + GetConfig()->Scale( 4 ), cW );
		DrawStr( g, cHdr, fHdr,
				 Gdiplus::RectF( (float)cX, (float)bodyY, (float)hdrLabW, (float)hdrH ), t.text );

		// Active-filter chip, doubling as the clear control: a filtered list that
		// does not say it is filtered is a trap.
		CString fl = FilterLabel();
		int chipRight = cX + hdrLabW;
		if ( !fl.IsEmpty() && m_filterFam >= 0 )
		{
			// Colour dot, family name, dismiss mark. No count by design (see
			// FilterLabel): the matrix totals row already carries it.
			bool chipHot = ( m_hot == HOT_FILTER );
			int pad   = GetConfig()->Scale( 8 );
			int swz   = GetConfig()->Scale( 8 );
			// The dot labels the name, so it sits tight against it and the slack
			// moves to the far side. gapDot + gapX is the constant that matters --
			// keep their SUM at Scale(14) and the chip width does not change.
			int gapDot = GetConfig()->Scale( 4 );
			int gapX   = GetConfig()->Scale( 10 );
			int nameW = TextW( g, fl, fTiny );
			int xSz   = GetConfig()->Scale( 8 );
			int chipH = GetConfig()->Scale( 17 );
			int chipW = pad + swz + gapDot + nameW + gapX + xSz + pad;
			int chipX = cX + hdrLabW + GetConfig()->Scale( 6 );
			int chipY = bodyY + ( hdrH - chipH ) / 2;

			// drop the chip rather than draw it clipped; the matrix still shows the
			// selection, so nothing is lost when the panel is too narrow
			if ( chipX + chipW <= cX + cW )
			{
				m_rcFilterChip = CRect( chipX, chipY, chipX + chipW, chipY + chipH );
				FillRound( g, m_rcFilterChip, chipH / 2, chipHot ? t.cardSelHot : t.cardSel );
				DrawRound( g, m_rcFilterChip, chipH / 2, t.borderSel, 1.0f );

				int mid = chipY + chipH / 2;
				// the family's own colour, so the chip visibly belongs to that column
				int fx = chipX + pad;
				FillRound( g, CRect( fx, mid - swz / 2, fx + swz, mid - swz / 2 + swz ), 2,
						   Gdiplus::Color( kAreaFamSwatch[m_filterFam].r,
										   kAreaFamSwatch[m_filterFam].g,
										   kAreaFamSwatch[m_filterFam].b ) );

				int nx = fx + swz + gapDot;
				DrawStr( g, fl, fTiny,
						 Gdiplus::RectF( (float)nx, (float)chipY, (float)( nameW + 4 ), (float)chipH ),
						 t.text, Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter );

				// centred on the same midline as the dot and the name, by construction
				DrawCloseMark( g, nx + nameW + gapX + xSz / 2, mid, xSz,
							   chipHot ? t.text : t.dimtxt );
				chipRight = m_rcFilterChip.right;
			}
		}
		// unit label, kept because "25, 25, 25" reads like an 8-bit code otherwise
		int hintX = chipRight + GetConfig()->Scale( 8 );
		if ( cX + cW - hintX > GetConfig()->Scale( 60 ) )
			DrawStr( g, S( IDS_PP_STIMULUSPCT ), fTiny,
					 Gdiplus::RectF( (float)hintX, (float)bodyY, (float)( cX + cW - hintX ), (float)hdrH ),
					 t.dimtxt, Gdiplus::StringAlignmentFar );

		// Its own geometry, tighter than the matrix's: one header line to clear
		// instead of a heading plus column headings, and rows that carry a swatch
		// and two short strings rather than a tinted cell.
		int wRowsY = bodyY + GetConfig()->Scale( 18 );
		int rowH   = GetConfig()->Scale( 18 );
		int maxRows = ( bodyY + bodyH - wRowsY ) / rowH;
		if ( maxRows > 0 )
		{
			// Column width from what a row actually measures -- widest stimulus
			// triplet, widest dE, plus the swatch -- then fit as many whole columns
			// as that allows. The pane is short, so columns are the only way to show
			// more than a handful of patches.
			int swSz0 = rowH - GetConfig()->Scale( 8 );
			// "999.9", not "99.9": ComputeProfileDEEx admits dE well past 100, and a
			// column measured for two digits ellipsised "128.5" to "12..."
			int deW   = TextW( g, "999.9", fSmall ) + GetConfig()->Scale( 8 );
			int colNeed = GetConfig()->Scale( 2 ) + swSz0 + GetConfig()->Scale( 6 )
						+ TextW( g, "100, 100, 100", fSmall ) + GetConfig()->Scale( 6 )
						+ deW + GetConfig()->Scale( 4 );
			int cols = ( colNeed > 0 ) ? ( cW / colNeed ) : 1;
			if ( cols < 1 ) cols = 1;
			if ( cols > 3 ) cols = 3;
			int colGap = GetConfig()->Scale( 8 );
			int colW = ( cW - ( cols - 1 ) * colGap ) / cols;
			// Bound the row count to the hot-id block as well as to the geometry:
			// HOT_WORST_FIRST..HOT_AREA_FIRST is only 100 ids wide, and a tall pane
			// with three columns can exceed that. Row 100 would otherwise produce
			// id 200, which ActivateHot reads as a colour-area filter.
			int cap = cols * maxRows;
			if ( cap > HOT_AREA_FIRST - HOT_WORST_FIRST )
				cap = HOT_AREA_FIRST - HOT_WORST_FIRST;
			int shown = 0;
			size_t w;
			for ( w = 0; w < m_stats.sorted.size() && shown < cap; w++ )
			{
				int pi = m_stats.sorted[w].second;
				if ( !PatchPassesFilter( pi ) )
					continue;
				int col = shown / maxRows, row = shown % maxRows;
				int x0 = cX + col * ( colW + colGap );
				int ry = wRowsY + row * rowH;
				CRect rcRow( x0, ry, x0 + colW, ry + rowH - GetConfig()->Scale( 1 ) );
				m_rcWorstRows.push_back( std::make_pair( rcRow, pi ) );
				if ( m_hot == HOT_WORST_FIRST + shown )
					FillRound( g, rcRow, 4, t.cardHot );

				ColorRGBDisplay rgb = pMeasure->GetProfilePatchRGB( pi );
				double dE = -m_stats.sorted[w].first;		// the sort key IS the dE, negated

				// Swatch and both text columns are centred on the row rect's own
				// midline. Previously the swatch was placed from the row TOP and the
				// text from a separate offset with a fixed 16px box, so the hover
				// highlight, the swatch and the baseline all disagreed slightly.
				int rowMid = ( rcRow.top + rcRow.bottom ) / 2;
				int swSz = rowH - GetConfig()->Scale( 8 );
				CRect rcSw( x0 + GetConfig()->Scale( 3 ), rowMid - swSz / 2,
							x0 + GetConfig()->Scale( 3 ) + swSz, rowMid - swSz / 2 + swSz );
				FillRound( g, rcSw, 3, SwatchColor( rgb ) );
				DrawRound( g, rcSw, 3, t.border, 1.0f );

				int txtX = rcSw.right + GetConfig()->Scale( 6 );
				int rowRight = x0 + colW - GetConfig()->Scale( 4 );
				int rgbW = rowRight - deW - txtX;	// deW is measured with the column width
				CString rgbTxt;
				rgbTxt.Format( "%.0f, %.0f, %.0f", rgb[0], rgb[1], rgb[2] );
				DrawStr( g, rgbTxt, fSmall,
						 Gdiplus::RectF( (float)txtX, (float)rcRow.top,
										 (float)max( 8, rgbW ), (float)rcRow.Height() ), t.text,
						 Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter );
				CString de;
				de.Format( "%.1f", dE );
				DrawStr( g, de, fSmall,
						 Gdiplus::RectF( (float)( rowRight - deW ), (float)rcRow.top, (float)deW, (float)rcRow.Height() ),
						 DETextColor( dE, good, warn, dark ),
						 Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentCenter );
				shown++;
			}
		}
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
			// the chip sits in the worst-list header, so test it before the rows
			if ( !m_rcFilterChip.IsRectEmpty() && m_rcFilterChip.PtInRect( pt ) )
				return HOT_FILTER;
			for ( size_t w = 0; w < m_rcWorstRows.size(); w++ )
				if ( m_rcWorstRows[w].first.PtInRect( pt ) )
					return HOT_WORST_FIRST + (int)w;
			// cells are pushed before their row label is tested, and the two never
			// overlap, so order between them does not matter
			for ( size_t a = 0; a < m_rcAreaHits.size(); a++ )
				if ( m_rcAreaHits[a].first.PtInRect( pt ) )
					return HOT_AREA_FIRST + m_rcAreaHits[a].second;
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
	// Area ids sit ABOVE the worst-row ids, so they must be tested first: the
	// open-ended `id >= HOT_WORST_FIRST` below would otherwise swallow them and
	// index the worst-row vector far out of range.
	if ( id >= HOT_AREA_FIRST )
	{
		int fam = id - HOT_AREA_FIRST;
		if ( fam >= 0 && fam < AREA_FAMS )
			SetFilter( fam );
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
		case HOT_FILTER: SetFilter( -1 ); break;			// the chip clears the filter
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
