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
	BOOL m_b10bitPGen;
	CButton m_tenBitCheck;
	BOOL m_b10bitMadvr;
	CButton m_tenBitMadvrCheck;

	HMONITOR	m_monitorHandle [ 16 ];
	CGoogleCastWrapper m_GCast;
	CGDIGenerator*	m_pGenerator;

	// Runtime-built "Pattern output" layout (branch uiFixes). The dialog-template
	// positions are ignored; every control is repositioned in Relayout() so all 5
	// localized templates produce one identical, overlap-free layout. The old
	// display-mode radios are hidden and replaced by m_outputCombo.
	CComboBox	m_outputCombo;
	CObArray	m_dynAll;     // runtime-created decoration (for cleanup)
	CButton		*m_grpDisplay, *m_grpMadvr, *m_grpCast, *m_grpPgen, *m_grpSignal, *m_grpPattern, *m_grpBlanking, *m_grpDvdo, *m_grpMuri;
	CStatic		*m_lblOutput, *m_lblScreen, *m_lblSize, *m_lblApl, *m_lblIntensity;
	CStatic		*m_lblXoff, *m_lblYoff, *m_lblCastDev, *m_lblRange;
	CStatic		*m_lblOffset;
	CStatic		*m_lblDvdoCom, *m_lblDvdoCs, *m_lblDvdoRange, *m_lblDvdoMode;
	CStatic		*m_lblDvdoFormat, *m_lblDvdoPatCat, *m_lblDvdoPat;
	CEdit		m_pgenReadout;
	CButton		m_pgenSettingsBtn;
	CButton m_pgenRefreshBtn;
	// DVDO AVLab TPG (DISPLAY_DVDO) config controls, created programmatically.
	CComboBox	m_dvdoComCombo, m_dvdoCsCombo, m_dvdoRangeCombo, m_dvdoModeCombo;
	CComboBox	m_dvdoFormatCombo, m_dvdoPatCatCombo, m_dvdoPatCombo;
	CButton		m_dvdoTestBtn, m_dvdoShowBtn, m_dvdoOffBtn, m_dvdoRefreshBtn, m_dvdoSettingsBtn;
	CStatic		m_dvdoStatus;
	CEdit		m_dvdoReadout;
	// Murideo Seven-G (DISPLAY_MURIDEO) config controls, created programmatically.
	CStatic		*m_lblMuriCom, *m_lblMuriTimingGrp, *m_lblMuriTiming, *m_lblMuriCs, *m_lblMuriPatGrp, *m_lblMuriPat, *m_lblMuriIp;
	CComboBox	m_muriComCombo, m_muriTimingGrpCombo, m_muriTimingCombo, m_muriCsCombo, m_muriPatGrpCombo, m_muriPatCombo;
	CEdit		m_muriIpEdit, m_muriReadout;
	CButton		m_muriNetCheck, m_muriTestBtn, m_muriApplyBtn, m_muriShowBtn, m_muriRefreshBtn, m_muriSettingsBtn, m_muriEdidBtn;
	CStatic		m_muriStatus;
	BOOL m_pgenQuerying;
	BOOL m_dvdoQuerying;
	BOOL m_muriQuerying;
	CBrush m_roBrush;
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);

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
	afx_msg void On10bitClick();
	afx_msg void OnPgenSettings();
	afx_msg void OnPgenRefresh();
	afx_msg void OnDvdoTest();
	afx_msg void OnDvdoShow();
	afx_msg void OnDvdoOff();
	afx_msg void OnDvdoRefresh();
	afx_msg void OnDvdoSettings();
	void RefreshDvdoStatus();
	afx_msg void OnDvdoCatChange();
	void PopulateDvdoComPorts();
	void PopulateDvdoPatternCombo(int cat);
	afx_msg void OnMuriTest();
	afx_msg void OnMuriApply();
	afx_msg void OnMuriShow();
	afx_msg void OnMuriRefresh();
	afx_msg void OnMuriSettings();
	afx_msg void OnMuriEdid();
	void RefreshMuriStatus();
	afx_msg void OnMuriTimingGrpChange();
	afx_msg void OnMuriPatGrpChange();
	void PopulateMuriComPorts();
	void PopulateMuriTimingCombo(int grp);
	void PopulateMuriPatCombo(int grp);
	void MuriXport(bool& useNet, CString& ip, CString& com);
	afx_msg void OnDestroy();
	afx_msg LRESULT OnPgenQueryDone(WPARAM, LPARAM);
	afx_msg LRESULT OnDvdoQueryDone(WPARAM, LPARAM);
	afx_msg LRESULT OnMuriQueryDone(WPARAM, LPARAM);
	CFont m_glyphFont;
	CToolTipCtrl m_pageTip;
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	DECLARE_MESSAGE_MAP()

	void BuildRuntimeLayout();
	void Relayout();
	void QueryPGenerator();
	void ShowPgenDisconnected();
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
	int m_action;
protected:
	CComboBox m_avi[6];
	CStatic m_aviL[6];
	int m_aviInit[6];
	CArray<int,int> m_resIds;
	CComboBox m_drm[2];
	CStatic m_drmL[2];
	int m_drmInit[2];
	CEdit m_ed[4];
	CStatic m_edL[4];
	CString m_edInit[4];
	CComboBox m_doviCombo;
	CStatic m_doviLbl;
	int m_doviInit;
	CButton m_rebootBtn;
	CButton m_restartBtn;
	CButton m_shutdownBtn;
	CStatic m_hdrAvi, m_hdrDrm, m_divider;
	CStatic m_hdrAviLine, m_hdrDrmLine;
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnFormatChanged();
	void UpdateRangeState();
	afx_msg void OnReboot();
	afx_msg void OnRestartSw();
	afx_msg void OnShutdown();
	afx_msg void OnDynRangeChanged();
	void UpdateDynRangeState();
	afx_msg void OnDestroy();
	CFont m_glyphFont;
	CToolTipCtrl m_tip;
	CFont m_glyphFontBig;
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	DECLARE_MESSAGE_MAP()
};

// Murideo Seven-G output settings, opened from the generator panel's "Settings..."
// button. Programmatic controls over the shared IDD_PGEN_SETTINGS shell.
class CMuriSettingsDlg : public CDialog
{
public:
	CMuriSettingsDlg(CWnd* pParent = NULL);
	enum { IDD = IDD_PGEN_SETTINGS };
protected:
	CStatic  m_lblIp, m_lblCom, m_lblTgrp, m_lblTiming, m_lblFmt, m_lblRange, m_lblGamut, m_lblHdr, m_lblDepth;
	CButton  m_netCheck;
	CEdit    m_ipEdit;
	CComboBox m_comCombo, m_tgrpCombo, m_timingCombo, m_fmtCombo, m_rangeCombo, m_gamutCombo, m_hdrCombo, m_depthCombo;
	CButton  m_testBtn, m_applyBtn, m_closeBtn;
	CStatic  m_status;
	int  ComboCsId();				// derive cat-99 colour-space id from format+range
	void MuriXport(bool& useNet, CString& ip, CString& com);
	void PopulateComPorts();
	void PopulateTimingCombo(int grp);
	void SaveToConfig();
	virtual BOOL OnInitDialog();
	afx_msg void OnTest();
	afx_msg void OnApply();
	afx_msg void OnClose2();
	afx_msg void OnTgrpChange();
	afx_msg void OnFmtChange();
	afx_msg void OnNetToggle();
	void UpdateTransportEnable();
	DECLARE_MESSAGE_MAP()
};

// Murideo connected-sink EDID report, opened from the generator panel's "Sink EDID"
// button. Scrollable read-only decode (General / Video / Audio) with Refresh/Copy/Close.
class CMuriEdidDlg : public CDialog
{
public:
	CMuriEdidDlg(CWnd* pParent = NULL);
	enum { IDD = IDD_PGEN_SETTINGS };
protected:
	CEdit    m_readout;
	CButton  m_refreshBtn, m_copyBtn, m_closeBtn;
	CFont    m_mono;
	void LoadEdid();
	virtual BOOL OnInitDialog();
	afx_msg void OnRefresh();
	afx_msg void OnCopy();
	afx_msg void OnClose2();
	DECLARE_MESSAGE_MAP()
};

// DVDO AVLab TPG output settings, opened from the generator panel's "DVDO settings..."
// button. Programmatic controls over the shared IDD_PGEN_SETTINGS shell (mirrors CMuriSettingsDlg).
class CDvdoSettingsDlg : public CDialog
{
public:
	CDvdoSettingsDlg(CWnd* pParent = NULL);
	enum { IDD = IDD_PGEN_SETTINGS };
protected:
	CStatic   m_lblCom, m_lblRes, m_lblFmt, m_lblRange;
	CComboBox m_comCombo, m_resCombo, m_fmtCombo, m_rangeCombo;
	CButton   m_testBtn, m_applyBtn, m_closeBtn;
	CStatic   m_status;
	void PopulateComPorts();
	void SaveToConfig();
	virtual BOOL OnInitDialog();
	afx_msg void OnTest();
	afx_msg void OnApply();
	afx_msg void OnClose2();
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GDIGENEPROPPAGE_H__761EFDB2_CC92_492A_8393_6C6049498029__INCLUDED_)
