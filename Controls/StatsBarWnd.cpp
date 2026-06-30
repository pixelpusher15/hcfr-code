// StatsBarWnd.cpp : implementation file
//

#include "stdafx.h"
#include "StatsBarWnd.h"
#include "fxcolor.h"
#include "resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Chip layout metrics (pixels).
#define STATSBAR_GAP		8	// horizontal gap between chips
#define STATSBAR_TEXTPAD	12	// text padding inside a chip (each side)
#define STATSBAR_VMARGIN	5	// top/bottom margin of a chip within the band
#define STATSBAR_CHIPVPAD	6	// vertical slack between the text and the chip edge
#define STATSBAR_FONTPT		10	// chip text size, points

/////////////////////////////////////////////////////////////////////////////
// CStatsBarWnd

CStatsBarWnd::CStatsBarWnd()
{
	m_rightReserve = 0;
	m_pEditBtn = NULL;
	m_pGlyphFont = NULL;
	m_pressZone = 0;
	m_hoverZone = 0;
	m_rcEditZone.SetRectEmpty();
	m_rcPlusZone.SetRectEmpty();
	m_rcMinusZone.SetRectEmpty();
}

void CStatsBarWnd::SetHeaderModel(CButton* pEditBtn, CFont* pGlyphFont)
{
	m_pEditBtn = pEditBtn;
	m_pGlyphFont = pGlyphFont;
	if (m_pEditBtn != NULL && ::IsWindow(m_pEditBtn->GetSafeHwnd()) && ::IsWindow(GetSafeHwnd()))
	{
		m_pEditBtn->SetParent(this);		// host the native checkbox inside the bar (WS_CLIPCHILDREN protects it)
		m_pEditBtn->ModifyStyle(0, BS_VCENTER);
		m_pEditBtn->ShowWindow(SW_SHOW);
	}
	if (::IsWindow(GetSafeHwnd()))
		Invalidate(FALSE);
}

// The hosted Edit checkbox is our child, so its BN_CLICKED comes here -- forward it
// to the parent view, which runs the existing OnEditgridCheck handler.
BOOL CStatsBarWnd::OnCommand(WPARAM wParam, LPARAM lParam)
{
	if (LOWORD(wParam) == IDC_EDITGRID_CHECK)
	{
		CWnd* p = GetParent();
		if (p != NULL)
			return (BOOL) p->SendMessage(WM_COMMAND, wParam, lParam);
	}
	return CWnd::OnCommand(wParam, lParam);
}

void CStatsBarWnd::SetRightReserve(int px)
{
	if (px < 0)
		px = 0;
	if (px == m_rightReserve)
		return;
	m_rightReserve = px;
	if (::IsWindow(GetSafeHwnd()))
		Invalidate(FALSE);
}

CStatsBarWnd::~CStatsBarWnd()
{
}

BEGIN_MESSAGE_MAP(CStatsBarWnd, CWnd)
	//{{AFX_MSG_MAP(CStatsBarWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSELEAVE()
	ON_WM_SIZE()
	ON_WM_CTLCOLOR()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CStatsBarWnd message handlers

// Trim surrounding whitespace from a segment. Labels are kept verbatim
// (e.g. "Average dE") so localisation is preserved.
static CString TrimSegment(const CString& strIn)
{
	CString s(strIn);
	s.TrimLeft();
	s.TrimRight();
	return s;
}

// Split one group's inner text on top-level commas (commas not inside any
// bracket), trimming and abbreviating each piece, and append to m_segments.
void CStatsBarWnd::AddGroup(const CString& strGroup, BOOL bSplitCommas)
{
	CString t(strGroup);
	t.TrimLeft();
	t.TrimRight();
	if (t.IsEmpty())
		return;

	if (!bSplitCommas)
	{
		m_segments.Add(TrimSegment(t));
		return;
	}

	int depth = 0;
	int start = 0;
	int n = t.GetLength();
	for (int i = 0; i < n; i++)
	{
		TCHAR c = t[i];
		if (c == _T('[') || c == _T('('))
			depth++;
		else if (c == _T(']') || c == _T(')'))
			depth--;
		else if (c == _T(',') && depth == 0)
		{
			CString piece = t.Mid(start, i - start);
			piece.TrimLeft();
			piece.TrimRight();
			if (!piece.IsEmpty())
				m_segments.Add(TrimSegment(piece));
			start = i + 1;
		}
	}
	CString last = t.Mid(start);
	last.TrimLeft();
	last.TrimRight();
	if (!last.IsEmpty())
		m_segments.Add(TrimSegment(last));
}

// Parse the verbose summary string into display segments. The string is a
// sequence of top-level "( ... )" groups (gamma+contrast, then average dE)
// followed by a trailing "[ ... ]" luminance-mode tag, e.g.:
//   " ( Average Gamma: 2.36, Contrast: 12345:1 )"
//   " ( Average dE: 2.02 [0.00,0.56,1.99] max: 3.38 [dCIE76(uv)] )"
//   " [Absolute Y w/o gamma]"
// It may also carry leading plain text outside any bracket (e.g. the
// ColorChecker pattern-set name "Classic GCD" in mode 11) -- such a run
// becomes its own chip. Parenthesised groups are comma-split into separate
// cells; bracket tags are kept whole. Nested brackets (the [a,b,c] error
// breakdown, the [form] tag) are handled by tracking depth. Whitespace-only
// runs between groups trim away and add nothing.
void CStatsBarWnd::Parse(LPCTSTR lpszText)
{
	m_segments.RemoveAll();

	CString s(lpszText);
	s.TrimLeft();
	s.TrimRight();
	int n = s.GetLength();
	int i = 0;
	int plainStart = 0;		// start of the current run of non-bracket text
	while (i < n)
	{
		TCHAR c = s[i];
		if (c == _T('(') || c == _T('['))
		{
			// Flush any plain text accumulated before this group as one chip.
			if (i > plainStart)
				AddGroup(s.Mid(plainStart, i - plainStart), FALSE);

			BOOL bParen = (c == _T('('));
			int depth = 0;
			int start = i + 1;
			int j = i;
			for (; j < n; j++)
			{
				TCHAR d = s[j];
				if (d == _T('(') || d == _T('['))
					depth++;
				else if (d == _T(')') || d == _T(']'))
				{
					depth--;
					if (depth == 0)
						break;
				}
			}
			CString inner = s.Mid(start, j - start);
			AddGroup(inner, bParen);
			i = j + 1;
			plainStart = i;
		}
		else
		{
			i++;
		}
	}

	// Flush any trailing plain text (also covers strings with no brackets).
	if (n > plainStart)
		AddGroup(s.Mid(plainStart, n - plainStart), FALSE);
}

void CStatsBarWnd::SetSegmentedText(LPCTSTR lpszText)
{
	CString strNew(lpszText);
	if (strNew == m_strText)
		return;					// nothing changed: avoid needless repaint

	m_strText = strNew;
	Parse(strNew);

	if (::IsWindow(GetSafeHwnd()))
		Invalidate(FALSE);
}

BOOL CStatsBarWnd::OnEraseBkgnd(CDC* /*pDC*/)
{
	return TRUE;	// fully painted in OnPaint (double-buffered)
}

// Build the larger bold chip font once (its height depends on screen DPI).
void CStatsBarWnd::EnsureFont(CDC* pDC)
{
	if (m_font.m_hObject != NULL)
		return;
	LOGFONT lf;
	ZeroMemory(&lf, sizeof(lf));
	// GetFont() asserts on a not-yet-created window, and PreferredHeight() can be
	// called before the band is created -- guard it (the band has no custom font,
	// so this falls back to the default GUI font either way).
	CFont* pBase = ::IsWindow(GetSafeHwnd()) ? GetFont() : NULL;
	if (pBase == NULL || pBase->GetLogFont(&lf) == 0)
		::GetObject(::GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf);
	lf.lfHeight = -MulDiv(STATSBAR_FONTPT, pDC->GetDeviceCaps(LOGPIXELSY), 72);
	lf.lfWidth = 0;
	lf.lfWeight = FW_BOLD;
	m_font.CreateFontIndirect(&lf);
}

// Band height needed so the DPI-scaled chip text clears the chip edges: the
// scaled text height plus fixed chip + band padding. Only the text term scales
// with DPI, so the band grows with the font but less than proportionally --
// the chips get taller to fit the text without the padding ballooning.
int CStatsBarWnd::PreferredHeight(CDC* pDC)
{
	EnsureFont(pDC);
	CFont* pOld = pDC->SelectObject(&m_font);
	TEXTMETRIC tm;
	pDC->GetTextMetrics(&tm);
	pDC->SelectObject(pOld);
	return tm.tmHeight + 2 * (STATSBAR_VMARGIN + STATSBAR_CHIPVPAD);
}

void CStatsBarWnd::OnPaint()
{
	CPaintDC dc(this);

	CRect rc;
	GetClientRect(rc);
	if (rc.Width() <= 0 || rc.Height() <= 0)
		return;

	EnsureFont(&dc);

	// Double-buffer to keep the header flicker-free during measurement updates.
	CDC memDC;
	memDC.CreateCompatibleDC(&dc);
	CBitmap bmp;
	bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
	CBitmap* pOldBmp = memDC.SelectObject(&bmp);

	// Flat band with borderless "chips" on it. Theme-aware: dark chips with
	// light text in dark mode, light-grey chips with black text in light mode.
	BOOL bDark = (fxUseCustomColor != FALSE);
	COLORREF clrBand = FxGetSysColor(COLOR_BTNFACE);
	COLORREF clrChip = bDark ? RGB(28, 28, 30) : RGB(220, 220, 220);
	COLORREF clrText = bDark ? RGB(235, 235, 235) : RGB(0, 0, 0);

	memDC.FillSolidRect(rc, clrBand);

	CFont* pOldFont = memDC.SelectObject(&m_font);
	int nOldMode = memDC.SetBkMode(TRANSPARENT);
	COLORREF clrOldTxt = memDC.SetTextColor(clrText);

	CBrush brChip(clrChip);
	CBrush* pOldBrush = memDC.SelectObject(&brChip);
	CPen* pOldPen = (CPen*) memDC.SelectStockObject(NULL_PEN);

	int chipTop = rc.top + STATSBAR_VMARGIN;
	int chipBottom = rc.bottom - STATSBAR_VMARGIN;

	// Reserve the far right for the bar-drawn header cluster ([ ] Edit [+] [-]).
	// Drawing it as part of the bar (like the chips) means it always fills the band
	// height and can never be clipped by a sibling window.
	int clusterLeft = rc.right - m_rightReserve;
	if (m_pEditBtn != NULL)
	{
		int eb = MulDiv(18, dc.GetDeviceCaps(LOGPIXELSY), 96);		// +/- buttons: 18x18 (DPI-scaled)
		int ey = (rc.top + rc.bottom) / 2 - eb / 2;					// vertically centred
		CFont* pem = m_pEditBtn->GetFont();
		CFont* pfm = memDC.SelectObject(pem ? pem : &m_font);
		CString sEdit; m_pEditBtn->GetWindowText(sEdit);	// the actual localized label ("Edit"/"Bearbeiten"/"Editer"...)
		if (sEdit.IsEmpty()) sEdit = _T("Edit");
		int editTextW = memDC.GetTextExtent(sEdit).cx;
		memDC.SelectObject(pfm);
		int editW = ::GetSystemMetrics(SM_CXMENUCHECK) + editTextW + STATSBAR_GAP;
		m_rcMinusZone = CRect(rc.right - STATSBAR_GAP - eb, ey, rc.right - STATSBAR_GAP, ey + eb);
		m_rcPlusZone  = CRect(m_rcMinusZone.left - 3 - eb,  ey, m_rcMinusZone.left - 3,  ey + eb);
		m_rcEditZone  = CRect(m_rcPlusZone.left - STATSBAR_GAP - editW, chipTop, m_rcPlusZone.left - STATSBAR_GAP, chipBottom);
		clusterLeft = m_rcEditZone.left - STATSBAR_GAP;
		// Position the hosted native Edit checkbox (a child of the bar) in its zone.
		if (::IsWindow(m_pEditBtn->GetSafeHwnd()))
		{
			CRect cur; m_pEditBtn->GetWindowRect(&cur); ScreenToClient(&cur);
			if (cur != m_rcEditZone)
				m_pEditBtn->MoveWindow(&m_rcEditZone, TRUE);
		}
	}
	else
	{
		m_rcEditZone.SetRectEmpty(); m_rcPlusZone.SetRectEmpty(); m_rcMinusZone.SetRectEmpty();
	}

	int x = rc.left + STATSBAR_GAP;
	int cnt = (int) m_segments.GetSize();
	for (int i = 0; i < cnt; i++)
	{
		CString seg = m_segments[i];
		CSize sz = memDC.GetTextExtent(seg);
		int chipW = sz.cx + STATSBAR_TEXTPAD * 2;
		if (x + chipW > clusterLeft)
			break;	// would run under the header cluster
		CRect chip(x, chipTop, x + chipW, chipBottom);

		memDC.RoundRect(chip, CPoint(6, 6));
		memDC.DrawText(seg, chip, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

		x += chipW + STATSBAR_GAP;
	}

	memDC.SelectObject(pOldPen);
	memDC.SelectObject(pOldBrush);
	memDC.SetTextColor(clrOldTxt);
	memDC.SetBkMode(nOldMode);
	memDC.SelectObject(pOldFont);

	if (m_pEditBtn != NULL)
		DrawHeaderControls(&memDC, rc);

	dc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
	memDC.SelectObject(pOldBmp);
}

// Draw the [ ] Edit checkbox and the [+]/[-] grid-size buttons at the far right of
// the band. Zones were computed in OnPaint. The +/- and checkmark glyphs use the
// Segoe Fluent Icons font (DrawTextW, so they render in this MBCS build).
void CStatsBarWnd::DrawHeaderControls(CDC* pDC, const CRect& /*rc*/)
{
	BOOL bDark = (fxUseCustomColor != FALSE);
	COLORREF clrText   = bDark ? RGB(235,235,235) : RGB(0,0,0);
	COLORREF clrFace   = bDark ? RGB(60,60,62)    : RGB(245,245,245);	// F5F5F5
	COLORREF clrHover  = bDark ? RGB(82,82,85)    : RGB(255,255,255);	// FFFFFF
	COLORREF clrBorder = bDark ? RGB(95,95,98)    : RGB(168,168,168);
	COLORREF clrPress  = bDark ? RGB(85,85,88)    : RGB(225,225,225);

	int oldBk = pDC->SetBkMode(TRANSPARENT);
	wchar_t gPlus = (wchar_t)0xE710, gMinus = (wchar_t)0xE738;	// Fluent Add / Remove

	// [+] and [-] buttons (the Edit checkbox is a hosted native control)
	if (m_pGlyphFont != NULL)
	{
		CFont* of = pDC->SelectObject(m_pGlyphFont);
		for (int k = 0; k < 2; k++)
		{
			CRect zr  = (k == 0) ? m_rcPlusZone : m_rcMinusZone;
			int   id  = (k == 0) ? IDC_SIZE_PLUS : IDC_SIZE_MINUS;
			wchar_t* gl = (k == 0) ? &gPlus : &gMinus;
			CBrush brF((m_pressZone == id) ? clrPress : (m_hoverZone == id) ? clrHover : clrFace);
			CPen   penF(PS_SOLID, 1, clrBorder);
			CBrush* ob = pDC->SelectObject(&brF);
			CPen*   op = pDC->SelectObject(&penF);
			pDC->RoundRect(zr.left, zr.top, zr.right, zr.bottom, 5, 5);
			pDC->SelectObject(ob);
			pDC->SelectObject(op);
			pDC->SetTextColor(clrText);
			CRect gr = zr;
			::DrawTextW(pDC->GetSafeHdc(), gl, 1, &gr, DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
		}
		pDC->SelectObject(of);
	}

	pDC->SetBkMode(oldBk);
}

void CStatsBarWnd::OnLButtonDown(UINT nFlags, CPoint point)
{
	int zone = 0;
	if (m_rcPlusZone.PtInRect(point))       zone = IDC_SIZE_PLUS;
	else if (m_rcMinusZone.PtInRect(point)) zone = IDC_SIZE_MINUS;
	if (zone != 0)
	{
		m_pressZone = zone;
		SetCapture();
		Invalidate(FALSE);
		return;
	}
	CWnd::OnLButtonDown(nFlags, point);
}

void CStatsBarWnd::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_pressZone != 0)
	{
		int zone = m_pressZone;
		m_pressZone = 0;
		ReleaseCapture();
		Invalidate(FALSE);
		BOOL hit = (zone == IDC_SIZE_PLUS  && m_rcPlusZone.PtInRect(point)) ||
		           (zone == IDC_SIZE_MINUS && m_rcMinusZone.PtInRect(point));
		if (hit)
		{
			CWnd* pParent = GetParent();
			if (pParent != NULL)
				// Post, don't Send: the handler runs a full OnSize re-layout that moves
				// THIS bar (and repaints the hosted Edit checkbox). Doing that synchronously
				// from inside our own mouse handler re-enters and corrupts the checkbox paint
				// (it flashes blank with a stray block). Defer it until we have returned.
				pParent->PostMessage(WM_COMMAND, MAKEWPARAM(zone, BN_CLICKED), (LPARAM)GetSafeHwnd());
		}
		return;
	}
	CWnd::OnLButtonUp(nFlags, point);
}

void CStatsBarWnd::OnMouseMove(UINT nFlags, CPoint point)
{
	int hz = 0;
	if (m_rcPlusZone.PtInRect(point))       hz = IDC_SIZE_PLUS;
	else if (m_rcMinusZone.PtInRect(point)) hz = IDC_SIZE_MINUS;
	if (hz != m_hoverZone)
	{
		m_hoverZone = hz;
		Invalidate(FALSE);
	}
	if (hz != 0)
	{
		TRACKMOUSEEVENT tme;
		tme.cbSize = sizeof(tme);
		tme.dwFlags = TME_LEAVE;
		tme.hwndTrack = GetSafeHwnd();
		tme.dwHoverTime = 0;
		_TrackMouseEvent(&tme);		// so we get WM_MOUSELEAVE to clear the hover
	}
	CWnd::OnMouseMove(nFlags, point);
}

void CStatsBarWnd::OnMouseLeave()
{
	if (m_hoverZone != 0)
	{
		m_hoverZone = 0;
		Invalidate(FALSE);
	}
	CWnd::OnMouseLeave();
}

// Round only the TOP corners so the bar follows the measures group/grid rounding
// instead of squaring it off. The round-rect region is extended below the window
// so the bottom corners stay square (they meet the grid header).
void CStatsBarWnd::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
	if (cx > 0 && cy > 0)
	{
		CClientDC dc(this);
		int r = MulDiv(7, dc.GetDeviceCaps(LOGPIXELSX), 96);	// rounded top corners of the header band
		if (r < 2) r = 2;
		HRGN rgn = ::CreateRoundRectRgn(0, 0, cx + 1, cy + r + 1, r * 2, r * 2);
		SetWindowRgn(rgn, TRUE);	// the system owns rgn after this
	}
}

// The Edit checkbox is hosted as our child (see SetHeaderModel), so Windows asks
// US for its background colour. Paint it to match the band and the active theme,
// the same way the parent view themes its own checkboxes -- otherwise the label
// sits on a default light rectangle that does not swap in dark mode.
HBRUSH CStatsBarWnd::OnCtlColor(CDC* pDC, CWnd* /*pWnd*/, UINT /*nCtlColor*/)
{
	COLORREF clrBand = FxGetSysColor(COLOR_BTNFACE);				// same fill the band paints with
	COLORREF clrText = (fxUseCustomColor != FALSE) ? RGB(235,235,235) : RGB(0,0,0);
	if (m_ctlBrush.m_hObject != NULL)
		m_ctlBrush.DeleteObject();
	m_ctlBrush.CreateSolidBrush(clrBand);
	pDC->SetBkMode(TRANSPARENT);
	pDC->SetTextColor(clrText);
	return (HBRUSH) m_ctlBrush.GetSafeHandle();
}

/////////////////////////////////////////////////////////////////////////////
// CMeasuresGroupBox

CMeasuresGroupBox::CMeasuresGroupBox()
	: m_pBar(NULL)
{
}

void CMeasuresGroupBox::InitMeasures(CStatsBarWnd* pBar, LPCTSTR lpszCaption)
{
	m_pBar = pBar;
	// Stamp the permanent caption via the base implementation; our own
	// SetText override never touches the caption again.
	CXPGroupBox::SetText(lpszCaption);
}

CXPGroupBox& CMeasuresGroupBox::SetText(LPCTSTR lpszText)
{
	if (m_pBar != NULL && ::IsWindow(m_pBar->GetSafeHwnd()))
		m_pBar->SetSegmentedText(lpszText);
	return *this;
}
