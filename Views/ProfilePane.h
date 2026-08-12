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

// ProfilePane.h : display-profile capture pane shown in place of the measures
// grid when the Mode dropdown is on "Display profile". Three states: setup
// (cube presets + options + Start), running (progress/ETA/live stats), and
// summary (dE stats, histogram, worst patches) once a capture exists.

#if !defined(PROFILEPANE_H_INCLUDED_)
#define PROFILEPANE_H_INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif

#include <vector>
#include <utility>
#include "GdiPlusAA.h"

class CDataSetDoc;
class CMeasure;

class CProfilePane : public CWnd
{
public:
	enum State  { PS_SETUP = 0, PS_RUNNING, PS_SUMMARY };
	enum Action { PA_NONE = 0, PA_START, PA_PAUSE, PA_STOP, PA_INSPECT, PA_REFS, PA_CLEAR };

	CProfilePane();
	virtual ~CProfilePane();

	BOOL Create(const CRect & rc, CWnd * pParent, UINT nID);

	void SetDocument(CDataSetDoc * pDoc) { m_pDoc = pDoc; }

	// derive setup-vs-summary from the document (keeps RUNNING while a capture is live)
	void RefreshState();
	// per-patch poke from CMainView::OnUpdate during a capture
	void OnCaptureProgress();
	void EnterRunning();
	void LeaveRunning();

	Action GetPendingAction() const { return m_pendingAction; }
	void ClearPendingAction() { m_pendingAction = PA_NONE; }
	int GetInspectIndex() const { return m_inspectIdx; }	// patch index behind PA_INSPECT

	int  GetCubeSize() const;
	BOOL GetGrayExtras() const { return m_grayExtras; }
	BOOL GetDriftComp() const { return m_driftComp; }
	BOOL IsRunning() const { return m_state == PS_RUNNING; }
	void SetPaused(BOOL b);

protected:
	// hover-tracking ids for owner-drawn interactive elements
	enum Hot { HOT_NONE = -1, HOT_PRESET_FIRST = 0, HOT_PRESET_LAST = 4,
			   HOT_START = 10, HOT_PAUSE = 11, HOT_STOP = 12,
			   HOT_REFS = 13, HOT_CTX = 14, HOT_CLEAR = 15,		// chrome buttons (client-space)
			   HOT_FILTER = 16,									// clears the worst-list filter
			   HOT_WORST_FIRST = 100,
			   HOT_AREA_FIRST = 200 };							// + colour-area family index

	// Colour-area buckets: hue family x brightness band. Family 0 is the neutral
	// axis (gray), 1..6 are the RGBCMY hue sectors; bands split on the stimulus
	// max channel. Both are derived from the GENERATED patch stimulus, never from
	// the measurement, so the classification is exact and free.
	enum { AREA_FAMS = 7, AREA_BANDS = 3 };

	// stats over the measured profile, recomputed lazily (m_statsValid)
	struct SProfStats
	{
		int		count;			// measured (valid) patches
		double	avgDE, maxDE, pct95DE;
		double	pctGood;		// fraction of patches under the good threshold
		int		histo[16];		// dE histogram, bin width warn/8
		double	histoBinW;
		int		histoOver;		// patches beyond the last bin (they fold INTO histo[15])
		// RMS split of the total error, in dE units. Squares because the terms
		// combine in quadrature: rmsL^2+rmsC^2+rmsH^2 reconstructs the overall RMS
		// dE exactly for CIE76/CIE94, and closely (not exactly) for CIE2000/CMC,
		// which carry a cross-term GetDeltaLCH does not hand back.
		double	rmsL, rmsC, rmsH;
		// area matrix (avg only -- the cells render the mean; a per-cell max was
		// tracked here for a while with no reader, so it went)
		double	areaAvg[AREA_FAMS][AREA_BANDS];
		int		areaCnt[AREA_FAMS][AREA_BANDS];
		int		famCnt[AREA_FAMS];
		// EVERY measured patch, worst dE first, as (-dE, patch index). Kept whole
		// instead of truncated to a top-N so the worst list can be filtered down to
		// a colour area without recomputing a single dE. 9261 entries at a 21-cube.
		std::vector<std::pair<double,int> > sorted;
		// per patch index: family*AREA_BANDS + band, or -1 for a skipped patch
		std::vector<signed char> bucket;

		SProfStats() { Reset(); }
		void Reset()	// NEVER memset this struct: it holds std::vectors
		{
			count = 0;
			avgDE = maxDE = pct95DE = pctGood = 0.0;
			histoBinW = 0.0;
			histoOver = 0;
			rmsL = rmsC = rmsH = 0.0;
			for ( int i = 0; i < 16; i++ ) histo[i] = 0;
			for ( int f = 0; f < AREA_FAMS; f++ )
			{
				famCnt[f] = 0;
				for ( int b = 0; b < AREA_BANDS; b++ )
				{
					areaAvg[f][b] = 0.0;
					areaCnt[f][b] = 0;
				}
			}
			sorted.clear();
			bucket.clear();
		}
	};

	CDataSetDoc *	m_pDoc;
	int				m_state;
	Action			m_pendingAction;
	int				m_inspectIdx;

	// setup options (persisted in the registry profile)
	int				m_preset;		// index into the preset table
	BOOL			m_grayExtras;
	BOOL			m_driftComp;

	// standard Windows checkboxes (children, shown only in the setup state)
	CButton			m_chkGrayExtras;
	CButton			m_chkDriftComp;

	// running-state observables
	BOOL			m_paused;
	DWORD			m_lastPatchTick;
	double			m_emaPatchSecs;
	int				m_lastSeenIndex;
	double			m_runSumDE, m_runMaxDE;
	int				m_runDECount;

	// summary cache
	SProfStats		m_stats;
	bool			m_statsValid;

	// hover + press state (buttons activate on release, over the same element)
	int				m_hot;
	int				m_pressed;
	bool			m_trackingMouse;

	// self-drawn pane frame: content is inset below the title and inside the
	// border, so the Paint* methods draw in a translated (0,0)-origin space and
	// mouse points are shifted back by (m_contentDX, m_contentDY)
	int				m_contentDX, m_contentDY;

	// References-button PNG icon (same asset as the toolbar Refs button), cached
	// and reloaded on theme change
	HICON			m_hRefIcon;
	bool			m_refIconDark;

	// hit-test rectangles rebuilt on every paint.
	// chrome buttons (m_rcRefs, m_rcCtx) are in pane CLIENT coords; body rects
	// (presets, start, pause, stop, worst rows) are in translated CONTENT coords.
	CRect			m_rcRefs, m_rcCtx, m_rcClear;	// client-space chrome
	CString			m_ctxLabel;					// "" hides the context button
	CRect			m_rcPresets[5];
	CRect			m_rcStart, m_rcPause, m_rcStop;
	std::vector<std::pair<CRect,int> > m_rcWorstRows;	// rect -> patch index
	std::vector<std::pair<CRect,int> > m_rcAreaHits;	// one rect per family COLUMN -> family index
	CRect			m_rcFilterChip;				// clears the filter; empty while unfiltered

	// Worst-list filter, driven by the colour-area matrix rather than by a combo
	// box: the matrix already lists every category, and a real child control here
	// would punch an opaque hole through the owner-drawn body (see SyncChildren).
	// A whole family at a time -- a single band of one family is too narrow a slice
	// to be worth a click, and it made the worst list collapse to a row or two.
	int				m_filterFam;	// 0..6, or -1 for all

	CMeasure * Measure() const;
	double PatchDE(int i) const;
	void ComputeStats();
	void InvalidateStats() { m_statsValid = false; }
	bool PatchPassesFilter(int patchIdx) const;	// against m_filterFam
	CString FilterLabel() const;				// "" while unfiltered
	void SetFilter(int fam);					// -1 clears; re-picking the same family clears too
	double ProfileWhiteDrift() const;			// measured white drift across the capture, 0 if unknown
	void SendAction(Action a, int inspectIdx = -1);
	int  PatchCountFor(int preset) const;
	double EstimateSeconds(int patches) const;
	int  HotFromPoint(CPoint pt) const;
	void ActivateHot(int hotId);	// perform the action for a pressed-and-released element
	void SyncChildren();		// checkbox visibility/position/state per pane state

	void PaintChrome(Gdiplus::Graphics & g, const CRect & client, bool dark);	// title + status + buttons (client space)
	CString StatusLine() const;	// per-state text for the chrome status slot
	void PaintSetup(Gdiplus::Graphics & g, const CRect & rc, bool dark);
	void PaintRunning(Gdiplus::Graphics & g, const CRect & rc, bool dark);
	void PaintSummary(Gdiplus::Graphics & g, const CRect & rc, bool dark);

	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

	//{{AFX_MSG(CProfilePane)
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg LRESULT OnMouseLeave(WPARAM wParam, LPARAM lParam);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#endif // !defined(PROFILEPANE_H_INCLUDED_)
