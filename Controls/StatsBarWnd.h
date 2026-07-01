#if !defined(AFX_STATSBARWND_H__INCLUDED_)
#define AFX_STATSBARWND_H__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// StatsBarWnd.h : header file
//
// CStatsBarWnd : a flat, themed, owner-drawn horizontal bar that shows the
// measurement summary (gamma / contrast / average dE / luminance mode) as a
// row of segments separated by vertical dividers. It sits between the Measures
// grid and the Selected-color pane (see CMainView). The text it is fed is the
// same verbose summary string the group-box caption used to display; the bar
// parses it into segments at paint time, so the many callers that build that
// string do not have to change.
//
// CMeasuresGroupBox : a CXPGroupBox whose caption is fixed (e.g. "Measures").
// It overrides SetText so every existing m_grayScaleGroup.SetText(stats) call
// is routed to the stats bar instead of overwriting the caption.

#include "XPGroupBox.h"

/////////////////////////////////////////////////////////////////////////////
// CStatsBarWnd window

class CStatsBarWnd : public CWnd
{
// Construction
public:
	CStatsBarWnd();

// Operations
public:
	// Feed the verbose summary string; it is split into display segments.
	void SetSegmentedText(LPCTSTR lpszText);

	// Height (px) the band needs so the DPI-scaled chip text is not clipped:
	// the scaled text height plus fixed, modest padding -- so the band tracks
	// the font but its padding does not balloon at high DPI.
	int PreferredHeight(CDC* pDC);

	
	// Header controls drawn BY THE BAR (so they fill the band height like the chips
	// and can never be clipped by sibling windows): a [ ] Edit checkbox plus [+]/[-]
	// grid-size buttons at the far right. The bar reads/toggles the (hidden) edit
	// checkbox for its state and fires the existing command handlers on click.
	//   pEditBtn   : hidden CButton that holds the edit-grid checked/enabled state
	//   pGlyphFont : Segoe Fluent Icons font for the +/- and checkmark glyphs
	void SetHeaderModel(CButton* pEditBtn, CFont* pGlyphFont);

// Implementation
public:
	virtual ~CStatsBarWnd();

protected:
	void Parse(LPCTSTR lpszText);
	void AddGroup(const CString& strGroup, BOOL bSplitCommas);
	void EnsureFont(CDC* pDC);	// build the bold chip font once (DPI-scaled)

	CString			m_strText;	// last text received (to skip redundant repaints)
	CStringArray	m_segments;	// parsed display segments
	CFont			m_font;		// larger bold font for the header chips
	int				m_rightReserve;	// px reserved on the right for overlaid controls

	// Bar-drawn header controls
	CButton*		m_pEditBtn;		// hidden control holding the edit-grid state
	CFont*			m_pGlyphFont;	// Segoe Fluent Icons (not owned)
	CRect			m_rcEditZone, m_rcPlusZone, m_rcMinusZone;	// hit rects (client px)
	int				m_pressZone;	// control id being pressed, or 0
	int				m_hoverZone;	// +/- control id under the mouse, or 0

	void DrawHeaderControls(CDC* pDC, const CRect& rc);	// draws the bar's header controls: the [ ] Edit checkbox and the [+]/[-] buttons

	
	// Generated message map functions
protected:
	//{{AFX_MSG(CStatsBarWnd)
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnMouseLeave();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CMeasuresGroupBox : group box that forwards its title text to a stats bar

class CMeasuresGroupBox : public CXPGroupBox
{
public:
	CMeasuresGroupBox();

	// Wire the bar and stamp the permanent caption (call once, after the
	// group box window and the bar both exist).
	void InitMeasures(CStatsBarWnd* pBar, LPCTSTR lpszCaption);

	// Override: route the (stats) text to the bar, keep our own caption.
	virtual CXPGroupBox& SetText(LPCTSTR lpszText);

protected:
	CStatsBarWnd* m_pBar;
};

//{{AFX_INSERT_LOCATION}}

#endif // !defined(AFX_STATSBARWND_H__INCLUDED_)
