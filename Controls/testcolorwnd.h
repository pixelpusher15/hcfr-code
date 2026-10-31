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

#if !defined(AFX_TESTCOLORWND_H__FAD1AC8A_D13F_4B47_9425_86E52B77FB47__INCLUDED_)
#define AFX_TESTCOLORWND_H__FAD1AC8A_D13F_4B47_9425_86E52B77FB47__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// testcolorwnd.h : header file
//

#include "ColourPickerXP.h"
#include "Color.h"

/////////////////////////////////////////////////////////////////////////////
// CTestColorWnd window

class CTestColorWnd : public CDialog
{
// Construction
public:
	CTestColorWnd(CWnd* pParent = NULL);

// Dialog Data
	//{{AFX_DATA(CTestColorWnd)
	enum { IDD = IDD_COLORTEST };
	//}}AFX_DATA

// Attributes
public:
	ColourTableEntry m_colorTableBackup[44];
	CColourPickerXP m_colorPicker;
	CRect m_colorPickerRect;

// Operations
public:
	// The patch the window stands for, at full precision. The picker holds only
	// an 8-bit full-range code, which OnPaint maps to 16-235 for a limited-range
	// generator; the exact color is kept here for the continuous and single
	// measures, which send it on to the generator. b16_235 picks the picker code
	// whose OnPaint mapping is the patch's nearest code in the window's range.
	void SetTestColor ( const ColorRGBDisplay & clr, BOOL b16_235 );
	// A color with no exact value behind it (a gray ramp, a calibration gray).
	void SetTestColor ( COLORREF clr );
	// The exact color while the picker still shows the code SetTestColor gave it,
	// else the picker's own code (the user picked a color).
	ColorRGBDisplay GetTestColor ();

private:
	CRITICAL_SECTION	m_exactLock;	// the continuous-measure thread reads the color
	ColorRGBDisplay	m_exactColor;
	COLORREF		m_exactCode;	// picker code m_exactColor was set with
	BOOL			m_bExactLimited;	// the range it was set for
	BOOL			m_bHasExact;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTestColorWnd)
	public:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CTestColorWnd();

	afx_msg LONG OnCloseUp(UINT lParam, LONG wParam);
	afx_msg LONG OnDropDown(UINT lParam, LONG wParam);

	// Generated message map functions
protected:
	//{{AFX_MSG(CTestColorWnd)
	afx_msg void OnPaint();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo(HELPINFO* pHelpInfo);
	virtual void OnCancel();
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TESTCOLORWND_H__FAD1AC8A_D13F_4B47_9425_86E52B77FB47__INCLUDED_)
