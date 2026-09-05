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

#if !defined(AFX_CIECHARTVIEW_H__FA46DF20_DBB5_4B59_8EBE_85FAE9A931A4__INCLUDED_)
#define AFX_CIECHARTVIEW_H__FA46DF20_DBB5_4B59_8EBE_85FAE9A931A4__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// CIEChartView.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CCIEChartView form view

#ifndef __AFXEXT_H__
#include <afxext.h>
#endif

class CDataSetDoc;

#include "PPTooltip.h" 

class CCIEGraphPoint
{
// Operations
public:
	CCIEGraphPoint(const ColorXYZ& color, double WhiteYRef, CString aName, BOOL bConvertCIEuv, BOOL bConvertCIEab);
	int GetGraphX(CRect rect) const;
	int GetGraphY(CRect rect) const;
	CPoint GetGraphPoint(CRect rect) const;
    ColorXYZ GetNormalizedColor() const {return m_color;}
    ColorXYZ GetAbsoluteColor() const {return a_color;}

// Attributes
public:
	double	x,a;
	double	y,b;
	double	L, YWhite;
	CString name;
	BOOL	bCIEuv;
	BOOL	bCIEab;
    ColorXYZ m_color;
    ColorXYZ a_color;
};

namespace Gdiplus { class Graphics; }

class CCIEChartGrapher
{
 public:
	CCIEChartGrapher ();

	CBitmap m_bgBitmap;
	CBitmap m_drawBitmap;
	CBitmap m_gamutBitmap;
	CBitmap m_refRedPrimaryBitmap;
	CBitmap m_refGreenPrimaryBitmap;
	CBitmap m_refBluePrimaryBitmap;
	CBitmap m_refYellowSecondaryBitmap;
	CBitmap m_refCyanSecondaryBitmap;
	CBitmap m_refMagentaSecondaryBitmap;
	CBitmap m_illuminantPointBitmap;
	CBitmap m_colorTempPointBitmap;
	CBitmap m_redPrimaryBitmap;
	CBitmap m_greenPrimaryBitmap;
	CBitmap m_bluePrimaryBitmap;
	CBitmap m_yellowSecondaryBitmap;
	CBitmap m_cyanSecondaryBitmap;
	CBitmap m_magentaSecondaryBitmap;
	CBitmap m_redSatRefBitmap;
	CBitmap m_greenSatRefBitmap;
	CBitmap m_blueSatRefBitmap;
	CBitmap m_yellowSatRefBitmap;
	CBitmap m_cyanSatRefBitmap;
	CBitmap m_magentaSatRefBitmap;
	CBitmap m_cc24SatRefBitmap;
	CBitmap m_grayPlotBitmap;
	CBitmap m_measurePlotBitmap;
	CBitmap m_selectedPlotBitmap;
	CBitmap	m_datarefRedBitmap;
	CBitmap	m_datarefGreenBitmap;
	CBitmap	m_datarefBlueBitmap;
	CBitmap	m_datarefYellowBitmap;
	CBitmap	m_datarefCyanBitmap;
	CBitmap	m_datarefMagentaBitmap;

	// Updatable flags. Initialized with default values in constructor, can be changed before calling drawing functions
	BOOL m_doDisplayBackground;
	BOOL m_doDisplayDeltaERef;
	BOOL m_doShowReferences;
	BOOL m_doShowDataRef;
	BOOL m_doShowGrayScale;
	BOOL m_doShowSaturationScale;
	BOOL m_doShowSaturationScaleTarg;
	BOOL m_doShowCCScale;
	BOOL m_doShowCCScaleTarg;
	BOOL m_doShowMeasurements;
	BOOL m_bCIEuv;
	BOOL m_bCIEab;
	BOOL m_bdE10;
	double dE10;
	BOOL isSat;

	int		m_ttID; //tooltip index, max of 5000 entries per chart

	// Shared GDI+ Graphics while a DrawChart pass runs (one per pass instead
	// of one per marker); NULL outside DrawChart.
	Gdiplus::Graphics *	m_pMarkerGraphics;
	float	m_markerScale;	// DPI scale, resolved once per DrawChart pass

	// More per-pass invariants: the tone-mapped white (a getL_EOTF call) and
	// the Lab-conversion reference are identical for every point of a pass,
	// but were being recomputed inside each DrawAlphaBitmap call — with a few
	// hundred points that dominated every full repaint. Set by DrawChart,
	// cleared when the pass ends; DrawAlphaBitmap falls back to computing
	// them itself when unset.
	double	m_passTmWhite;
	const CColorReference *	m_pPassRef;

	// MakeBgBitmap cache key: skip rebuilding when the background would be
	// identical (helps live resize, where OnSize and OnUpdate both rebuild)
	int		m_bgW, m_bgH;
	BOOL	m_bgWhite, m_bgUv, m_bgAb, m_bgShowBg, m_bgShowDE;
	double	m_bgWhitex, m_bgWhitey;

	// Gamut-coverage chip staleness tracking: the standard and measured
	// primaries the last shown coverage % was computed for. When the gamut is
	// changed but the primaries have not been re-measured, the percentages are
	// hidden (the gamut-name chip still shows).
	BOOL			m_covValid;
	ColorStandard	m_covStandard;
	ColorXYZ		m_covPrimaries[3];

	// Zoom handling, for window mode
	UINT	m_ZoomFactor;	// Zoom factor = 1000 for 1:1 scale, 2000 for 2x zoom, and so on
	int		m_DeltaX;		// When zoom active, delta values for picture scrolling in pixels
	int		m_DeltaY;

	// Operations
	void MakeBgBitmap(CRect rect,BOOL bWhiteBkgnd);
	void DrawCoverageChips(CDC *pDC, CRect rcAnchor, CDataSetDoc * pDoc);
	void DrawAlphaBitmap(CDC *pDC, const CCIEGraphPoint& aGraphPoint, CBitmap *pBitmap, CRect rect, CPPToolTip * pTooltip, CWnd * pWnd, CCIEGraphPoint * pRefPoint = NULL, bool isSelected = FALSE, double dE10=100.0, bool isPrimeSec = FALSE);
	bool DrawGdiPlusMarker(CDC *pDC, CBitmap *pBitmap, int x, int y, const CCIEGraphPoint& aGraphPoint, bool isSelected);
	void DrawChart(CDataSetDoc * pDoc, CDC* pDC, CRect rect, CPPToolTip * pTooltip, CWnd * pWnd);
	void SaveGraphFile ( CDataSetDoc * pDoc, CSize ImageSize, LPCSTR lpszPathName, int ImageFormat = 0, int ImageQuality = 95, bool PDF=FALSE );
};

class CCIEChartView : public CSavingView
{
protected:
	CCIEChartView();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CCIEChartView)

// Form Data
public:
	//{{AFX_DATA(CCIEChartView)
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

protected:
// Attributes
	BOOL	m_bDelayedUpdate;

	// Set when a realtime hint arrives mid-sweep: the next OnDraw paints just
	// the new measurement over the retained chart bitmap instead of running a
	// full DrawChart pass (which re-derives every reference point and tooltip,
	// and made sweeps several times slower while this chart was visible).
	BOOL	m_bRealtimeIncrement;

	// Retained-chart repaint (CIE-001/CIE-006): m_drawBitmap keeps the last
	// full DrawChart output, so a paint where neither the data (m_bChartDirty,
	// set by OnUpdate) nor the content signature below changed is a plain
	// blit. During a live resize the retained chart is StretchBlt'ed and the
	// real render happens once, on the resize settle timer.
	BOOL	m_bChartDirty;
	BOOL	m_bResizeSettling;

	// Content signature of the last full DrawChart: render size, the grapher
	// display toggles (GetUserInfo bits), and the MainView/document state
	// DrawChart reads directly without any update hint reaching this view.
	int		m_chartW, m_chartH;
	DWORD	m_chartUserInfo;
	int		m_chartMode;
	int		m_chartEdit;
	double	m_chartdE10;
	BOOL	m_chartSelValid;
	ColorXYZ	m_chartSel;

	// Pinch-to-zoom state: distance between the two touch points and the
	// zoom factor captured when the gesture began (GF_BEGIN).
	double	m_gestureStartDist;
	UINT	m_gestureStartZoom;

	// Client-sized compose buffer: the chart blit and the pinned chip
	// overlay are combined here and reach the screen in one blit, otherwise
	// the chips flicker on every pan/zoom repaint.
	CBitmap	m_composeBitmap;
	CSize	m_composeSize;

	CCIEChartGrapher m_Grapher;

	double	m_refDeltaE;

	CPPToolTip m_tooltip;

	CPoint	m_CurMousePoint;

public:
	CDataSetDoc * GetDocument() const { return (CDataSetDoc *) CView::GetDocument (); }
	virtual DWORD	GetUserInfo ();
	virtual void	SetUserInfo ( DWORD dwUserInfo );

// Operations
public:
	void	UpdateTestColor ( CPoint point );
	void	GetReferenceRect ( LPRECT lpRect );		// Returns client rect with size increased regarding zoom factor
	void	ZoomChart ( int nNewFactor, CPoint ptAnchorClient );	// Anchored zoom shared by wheel, menu and pinch
	void	SchedulePreviewSettle ( UINT nDelayMs );	// Preview paints until the settle timer runs the real render
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CCIEChartView)
	public:
	virtual void OnInitialUpdate();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void OnDraw(CDC* pDC);
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	//}}AFX_VIRTUAL

// Implementation
protected:
	void SaveChart();

	virtual ~CCIEChartView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
	//{{AFX_MSG(CCIEChartView)
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnUpdateCieShowbackground(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCieShowDeltaE(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCieShowreferences(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCieGraphShowDataRef(CCmdUI* pCmdUI);
	afx_msg void OnCieShowreferences();
	afx_msg void OnCieGraphShowDataRef();
	afx_msg void OnCieShowbackground();
	afx_msg void OnCieShowDeltaE();
	afx_msg void OnCieShowGrayScale();
	afx_msg void OnCieShowSaturationScale();
	afx_msg void OnCieShowSaturationScaleTarg();
	afx_msg void OnCieShowCCScale();
	afx_msg void OnCieShowdE10();
	afx_msg void OnCieShowCCScaleTarg();
	afx_msg void OnCieShowMeasurements();
	afx_msg void OnGraphZoomIn();
	afx_msg void OnGraphZoomOut();
	afx_msg void OnUpdateCieShowMeasurements(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCieShowGrayScale(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCieShowSaturationScale(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCieShowSaturationScaleTarg(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCieShowCCScale(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCieShowdE10(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCieShowCCScaleTarg(CCmdUI* pCmdUI);
	afx_msg void OnCieSavechart();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnHelp();
	afx_msg void OnCieUv();
	afx_msg void OnCieab();
	afx_msg void OnUpdateCieUv(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCieab(CCmdUI* pCmdUI);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void NotifyDisplayTooltip(NMHDR * pNMHDR, LRESULT * result);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg LRESULT OnGestureMsg(WPARAM wParam, LPARAM lParam);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CIECHARTVIEW_H__FA46DF20_DBB5_4B59_8EBE_85FAE9A931A4__INCLUDED_)
