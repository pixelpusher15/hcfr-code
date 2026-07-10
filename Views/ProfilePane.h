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
	enum Action { PA_NONE = 0, PA_START, PA_PAUSE, PA_NEWPROFILE, PA_INSPECT };

	CProfilePane();

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
			   HOT_START = 10, HOT_PAUSE = 11, HOT_NEWPROFILE = 12,
			   HOT_WORST_FIRST = 100 };

	// stats over the measured profile, recomputed lazily (m_statsValid)
	struct SProfStats
	{
		int		count;			// measured (valid) patches
		double	avgDE, maxDE, pct95DE;
		double	pctGood;		// fraction of patches under the good threshold
		int		histo[16];		// dE histogram, bin width warn/8
		double	histoBinW;
		// region rows: 0 gray axis, 1 near black, 2 low sat, 3 high sat
		double	regAvg[4], regMax[4];
		int		regCnt[4];
		std::vector<int> worst;	// patch indices, worst first (up to 20)

		SProfStats() { Reset(); }
		void Reset()	// NEVER memset this struct: it holds a std::vector
		{
			count = 0;
			avgDE = maxDE = pct95DE = pctGood = 0.0;
			histoBinW = 0.0;
			for ( int i = 0; i < 16; i++ ) histo[i] = 0;
			for ( int r = 0; r < 4; r++ ) { regAvg[r] = regMax[r] = 0.0; regCnt[r] = 0; }
			worst.clear();
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

	// hover state
	int				m_hot;
	bool			m_trackingMouse;

	// hit-test rectangles rebuilt on every paint
	CRect			m_rcPresets[5];
	CRect			m_rcStart, m_rcPause, m_rcNewProfile;
	std::vector<std::pair<CRect,int> > m_rcWorstRows;	// rect -> patch index

	CMeasure * Measure() const;
	double WhiteYForDE() const;
	double PatchDE(int i) const;
	void ComputeStats();
	void InvalidateStats() { m_statsValid = false; }
	void SendAction(Action a, int inspectIdx = -1);
	int  PatchCountFor(int preset) const;
	double EstimateSeconds(int patches) const;
	int  HotFromPoint(CPoint pt) const;
	void SyncChildren();		// checkbox visibility/position/state per pane state

	void PaintSetup(Gdiplus::Graphics & g, const CRect & rc, bool dark);
	void PaintRunning(Gdiplus::Graphics & g, const CRect & rc, bool dark);
	void PaintSummary(Gdiplus::Graphics & g, const CRect & rc, bool dark);

	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

	//{{AFX_MSG(CProfilePane)
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg LRESULT OnMouseLeave(WPARAM wParam, LPARAM lParam);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#endif // !defined(PROFILEPANE_H_INCLUDED_)
