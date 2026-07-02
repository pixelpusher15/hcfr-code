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

#if !defined(AFX_TARGETWND_H__8F910E63_5CB4_4C82_B6B5_6E26B64A7A85__INCLUDED_)
#define AFX_TARGETWND_H__8F910E63_5CB4_4C82_B6B5_6E26B64A7A85__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TargetWnd.h : header file
//

#include <deque>

namespace Gdiplus { class Bitmap; }

/////////////////////////////////////////////////////////////////////////////
// CTargetWnd window

class CTargetWnd : public CWnd
{
// Construction
public:
	CTargetWnd();

// Attributes
public:
	double		m_deltax;
	double		m_deltay;
	double		m_deltaE;		// dE of the current read (< 0 until first refresh)
	COLORREF	m_clr;
	int nR;
	int nG;
	int nB;
	ColorXYZ centerXYZ;

	CColor *	m_pRefColor;
	CDataSetDoc *	m_pDocument;

// Operations
public:
	enum { TARGET_TESTWINDOW = - 1, TARGET_ALL = 0, TARGET_TARGET = 1 };

	void Refresh(BOOL m_b16_235, int minCol, int nSize, int m_DisplayMode, CDataSetDoc * pDoc, int target);

protected:
	// Measurement history drawn as a fading trail. Points are stored in
	// ring-scale polar form (angle, radius fraction) so window resizes and
	// theme switches don't invalidate them. The last entry is the current read.
	struct STrailPoint { double angle; double radius; };
	std::deque<STrailPoint> m_trail;
	ColorXYZ	m_trailCenter;	// target the trail belongs to
	int			m_trailCol;
	int			m_trailMode;

	// Background (panel, hue wheel, dE rings, labels) cached per size/theme.
	Gdiplus::Bitmap *	m_pBgBitmap;
	int			m_bgCx;
	int			m_bgCy;
	BOOL		m_bgDark;
	double		m_bgTol;	// tolerance ring position baked into the cache

	void GetGeometry(const CRect & rect, double & cx, double & cy, double & R) const;
	void RebuildBackground(const CRect & rect, BOOL bDark);
	void UpdateDeltaE();
	static double RingRadius(double dE);

// Implementation
public:
	virtual ~CTargetWnd();

	// Generated message map functions
protected:
	//{{AFX_MSG(CTargetWnd)
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TARGETWND_H__8F910E63_5CB4_4C82_B6B5_6E26B64A7A85__INCLUDED_)
