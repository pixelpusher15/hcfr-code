// StatsBarWnd.cpp : implementation file
//

#include "stdafx.h"
#include "StatsBarWnd.h"
#include "fxcolor.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Chip layout metrics (pixels).
#define STATSBAR_GAP		8	// horizontal gap between chips
#define STATSBAR_TEXTPAD	12	// text padding inside a chip (each side)
#define STATSBAR_VMARGIN	5	// top/bottom margin of a chip within the band
#define STATSBAR_FONTPT		10	// chip text size, points

/////////////////////////////////////////////////////////////////////////////
// CStatsBarWnd

CStatsBarWnd::CStatsBarWnd()
{
}

CStatsBarWnd::~CStatsBarWnd()
{
}

BEGIN_MESSAGE_MAP(CStatsBarWnd, CWnd)
	//{{AFX_MSG_MAP(CStatsBarWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
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

void CStatsBarWnd::OnPaint()
{
	CPaintDC dc(this);

	CRect rc;
	GetClientRect(rc);
	if (rc.Width() <= 0 || rc.Height() <= 0)
		return;

	// Build the larger bold chip font once (depends on screen DPI).
	if (m_font.m_hObject == NULL)
	{
		LOGFONT lf;
		ZeroMemory(&lf, sizeof(lf));
		CFont* pBase = GetFont();
		if (pBase == NULL || pBase->GetLogFont(&lf) == 0)
			::GetObject(::GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf);
		lf.lfHeight = -MulDiv(STATSBAR_FONTPT, dc.GetDeviceCaps(LOGPIXELSY), 72);
		lf.lfWidth = 0;
		lf.lfWeight = FW_BOLD;
		m_font.CreateFontIndirect(&lf);
	}

	// Double-buffer to keep the header flicker-free during measurement updates.
	CDC memDC;
	memDC.CreateCompatibleDC(&dc);
	CBitmap bmp;
	bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
	CBitmap* pOldBmp = memDC.SelectObject(&bmp);

	// Flat band with dark, borderless "chips" on it (light text for contrast).
	COLORREF clrBand = FxGetSysColor(COLOR_BTNFACE);
	COLORREF clrChip = RGB(28, 28, 30);
	COLORREF clrText = RGB(235, 235, 235);

	memDC.FillSolidRect(rc, clrBand);

	CFont* pOldFont = memDC.SelectObject(&m_font);
	int nOldMode = memDC.SetBkMode(TRANSPARENT);
	COLORREF clrOldTxt = memDC.SetTextColor(clrText);

	CBrush brChip(clrChip);
	CBrush* pOldBrush = memDC.SelectObject(&brChip);
	CPen* pOldPen = (CPen*) memDC.SelectStockObject(NULL_PEN);

	int chipTop = rc.top + STATSBAR_VMARGIN;
	int chipBottom = rc.bottom - STATSBAR_VMARGIN;
	int x = rc.left + STATSBAR_GAP;
	int cnt = (int) m_segments.GetSize();
	for (int i = 0; i < cnt; i++)
	{
		CString seg = m_segments[i];
		CSize sz = memDC.GetTextExtent(seg);
		int chipW = sz.cx + STATSBAR_TEXTPAD * 2;
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

	dc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
	memDC.SelectObject(pOldBmp);
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
