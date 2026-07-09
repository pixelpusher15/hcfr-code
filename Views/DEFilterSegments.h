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

// DEFilterSegments.h : three-segment dE filter next to the info-pane dropdown
// ([ Show all | Hide < good | Hide < warn ]). The two thresholds come live
// from CColorHCFRConfig::GetDEThresholds, so the labels track the tolerance
// preset chosen in Advanced settings. Selecting a segment fires BN_CLICKED to
// the parent.
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_DEFILTERSEGMENTS_H__8A4C2E11_5B7D_4F3A_A6C8_1D9E0B2F7C44__INCLUDED_)
#define AFX_DEFILTERSEGMENTS_H__8A4C2E11_5B7D_4F3A_A6C8_1D9E0B2F7C44__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CDEFilterSegments : public CWnd
{
public:
	CDEFilterSegments();

	BOOL Create(const CRect & rc, CWnd * pParent, UINT nID);

	int  GetSel() const { return m_sel; }
	void SetSel(int sel);

protected:
	int m_sel;   // 0 = show all, 1 = hide < good, 2 = hide < warn

	int SegmentFromPoint(CPoint pt) const;

	//{{AFX_MSG(CDEFilterSegments)
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}

#endif // !defined(AFX_DEFILTERSEGMENTS_H__...)
