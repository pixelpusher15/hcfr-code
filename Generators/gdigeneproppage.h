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

#if !defined(AFX_GDIGENEPROPPAGE_H__761EFDB2_CC92_492A_8393_6C6049498029__INCLUDED_)
#define AFX_GDIGENEPROPPAGE_H__761EFDB2_CC92_492A_8393_6C6049498029__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GDIGenePropPage.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CGDIGenePropPage dialog

class CGDIGenerator;

class CGDIGenePropPage : public CPropertyPageWithHelp
{
	DECLARE_DYNCREATE(CGDIGenePropPage)

// Construction
public:
	CGDIGenePropPage();
	~CGDIGenePropPage();

// Dialog Data
	//{{AFX_DATA(CGDIGenePropPage)
	enum { IDD = IDD_GENERATOR_GDI_PROP_PAGE };
	CComboBox	m_monitorComboCtrl;
	CComboBox	m_cCastComboCtrl;
	UINT	m_rectSizePercent;
	UINT	m_bgStimPercent;
	UINT	m_Intensity;
	int		m_offsetx,m_offsety;
	//}}AFX_DATA

	CArray <CString,CString> m_monitorNameArray;
    CEdit m_madVREdit;
    CEdit m_madVREdit2;
    CEdit m_madVREdit3;
    CEdit m_madVREdit4;
    CEdit m_usePicEdit;
	CEdit m_bIntensity;
	int m_activeMonitorNum;
	int	m_nDisplayMode;
	int m_selectedGcastNum;
	BOOL m_b16_235;
	BOOL m_busePic;
	BOOL m_bdispTrip,m_brPi_user;
    BOOL m_madVR_3d;
    BOOL m_madVR_vLUT, m_madVR_HDR;
	BOOL m_madVR_OSD;
	BOOL m_bLinear;
	BOOL m_bHdr10;
	bool m_castHasDevice;
	BOOL m_doScreenBlanking;
	CButton m_blankCheck;

	HMONITOR	m_monitorHandle [ 16 ];
	CGoogleCastWrapper m_GCast;
	CGDIGenerator*	m_pGenerator;

	// Runtime-built "Pattern output" layout (branch uiFixes). The dialog-template
	// positions are ignored; every control is repositioned in Relayout() so all 5
	// localized templates produce one identical, overlap-free layout. The old
	// display-mode radios are hidden and replaced by m_outputCombo.
	CComboBox	m_outputCombo;
	CObArray	m_dynAll;     // runtime-created decoration (for cleanup)
	CButton		*m_grpDisplay, *m_grpMadvr, *m_grpCast, *m_grpPgen, *m_grpSignal, *m_grpPattern, *m_grpBlanking;
	CStatic		*m_lblOutput, *m_lblScreen, *m_lblSize, *m_lblApl, *m_lblIntensity;
	CStatic		*m_lblXoff, *m_lblYoff, *m_lblCastDev, *m_lblRange;
	CStatic		*m_lblOffset;
	CEdit		m_pgenReadout;
	CButton		m_pgenSettingsBtn;

	virtual UINT GetHelpId ( LPSTR lpszTopic );

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CGDIGenePropPage)
	public:
	virtual void OnOK();
	virtual BOOL OnSetActive();
	virtual BOOL OnKillActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CGDIGenePropPage)
	virtual BOOL OnInitDialog();
	afx_msg void OnTestOverlay();
	afx_msg void OnDropdownMonitorCombo();
	//}}AFX_MSG
	afx_msg void OnSelchangeOutput();
	afx_msg void OnUserPatternClick();
	afx_msg void OnPgenSettings();
	DECLARE_MESSAGE_MAP()

	void BuildRuntimeLayout();
	void Relayout();
	void QueryPGenerator();
	void PopulateCast();
	int  ComboToMode(int sel);
	int  ModeToCombo(int mode);
};

class CPGenSettingsDlg : public CDialog
{
public:
	CPGenSettingsDlg(CWnd* pParent = NULL);
	enum { IDD = IDD_PGEN_SETTINGS };
	CGDIGenerator* m_pGenerator;
protected:
	CComboBox m_combo[4];
	CStatic m_label[4];
	int m_initSel[4];
	CComboBox m_resCombo;
	CStatic m_resLabel;
	CArray<int,int> m_resIds;
	int m_resInit;
	CButton m_rebootBtn;
	CButton m_restartBtn;
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnFormatChanged();
	void UpdateRangeState();
	afx_msg void OnReboot();
	afx_msg void OnRestartSw();
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GDIGENEPROPPAGE_H__761EFDB2_CC92_492A_8393_6C6049498029__INCLUDED_)
