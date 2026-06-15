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

// Implementation
public:
	virtual ~CStatsBarWnd();

protected:
	void Parse(LPCTSTR lpszText);
	void AddGroup(const CString& strGroup, BOOL bSplitCommas);

	CString			m_strText;	// last text received (to skip redundant repaints)
	CStringArray	m_segments;	// parsed display segments
	CFont			m_font;		// larger bold font for the header chips

	// Generated message map functions
protected:
	//{{AFX_MSG(CStatsBarWnd)
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
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
