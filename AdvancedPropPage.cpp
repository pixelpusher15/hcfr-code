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
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// AdvancedPropPage.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "AdvancedPropPage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAdvancedPropPage property page

// Local child-control id for the programmatic calibration-method dropdown.
#define IDC_ADV_CALIB_COMBO 1650

IMPLEMENT_DYNCREATE(CAdvancedPropPage, CPropertyPageWithHelp)

CAdvancedPropPage::CAdvancedPropPage() : CPropertyPageWithHelp(CAdvancedPropPage::IDD)
{
	//{{AFX_DATA_INIT(CAdvancedPropPage)
	m_bConfirmMeasures = TRUE;
	m_comPort = _T("");
	m_calibrationMethod = CALIB_HCFR_DEFAULT;
	m_bUseImperialUnits = FALSE;
	m_nLuminanceCurveMode = 0;
	m_bPreferLuxmeter = FALSE;
	m_dE_form = 5;
	m_dE_preset = 1;
	m_dE_gray = 2;
    gw_Weight = 0;
    doHighlight = TRUE;
	//}}AFX_DATA_INIT

	m_isModified = FALSE;
}

CAdvancedPropPage::~CAdvancedPropPage()
{
}

void CAdvancedPropPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPageWithHelp::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAdvancedPropPage)
	DDX_Control(pDX, IDC_COMBO_dE_WEIGHT, m_gwWeightEdit);
	DDX_Control(pDX, IDC_COMBO_dE_GRAY, m_dEgrayEdit);
	DDX_Control(pDX, IDC_COMBO_dE, m_dEform);
	DDX_Control(pDX, IDC_COMBO_DE_TOLERANCE, m_dEtolCombo);
	DDX_Check(pDX, IDC_CHECK_CONFIRM, m_bConfirmMeasures);
	DDX_CBString(pDX, IDC_LUXMETER_COM_COMBO, m_comPort);
	DDX_CBIndex(pDX, IDC_COMBO_dE, m_dE_form);
	DDX_CBIndex(pDX, IDC_COMBO_dE_GRAY, m_dE_gray);
	DDX_CBIndex(pDX, IDC_COMBO_dE_WEIGHT, gw_Weight);
	// Calibration method is driven by m_calibMethodCombo (created in OnInitDialog);
	// it is read back in SyncCalibMethodFromCombo, so no DDX_Radio here.
	DDX_Check(pDX, IDC_HIGHLIGHT, doHighlight);
	DDX_Check(pDX, IDC_CHECK_IMPERIAL, m_bUseImperialUnits);
	DDX_Radio(pDX, IDC_RADIO1, m_nLuminanceCurveMode);
	DDX_Check(pDX, IDC_CHECK_PREFER_LUXMETER, m_bPreferLuxmeter);
	//}}AFX_DATA_MAP
	DDX_CBIndex(pDX, IDC_COMBO_DE_TOLERANCE, m_dE_preset);
}


BEGIN_MESSAGE_MAP(CAdvancedPropPage, CPropertyPageWithHelp)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_CHECK_CONFIRM, IDC_CHECK_CONFIRM, OnControlClicked)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_CHECK_IMPERIAL, IDC_CHECK_IMPERIAL, OnControlClicked)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_CHECK_PREFER_LUXMETER, IDC_CHECK_PREFER_LUXMETER, OnControlClicked)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_CHECK_DELTAE_GRAY_LUMA, IDC_CHECK_DELTAE_GRAY_LUMA, OnControlClicked)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_RADIO1, IDC_RADIO3, OnControlClicked)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_HIGHLIGHT, IDC_HIGHLIGHT, OnControlClicked)
    ON_CBN_SELCHANGE(IDC_ADV_CALIB_COMBO, OnSelchangeCalibMethod)

	//{{AFX_MSG_MAP(CAdvancedPropPage)
	ON_CBN_SELCHANGE(IDC_LUXMETER_COM_COMBO, OnSelchangeLuxmeterComCombo)
	ON_CBN_SELCHANGE(IDC_COMBO_dE, OnSelchangedECombo)
	ON_CBN_SELCHANGE(IDC_COMBO_dE_GRAY, OnSelchangedECombo)
	ON_CBN_SELCHANGE(IDC_COMBO_dE_WEIGHT, OnSelchangedECombo)
	ON_CBN_SELCHANGE(IDC_COMBO_DE_TOLERANCE, OnSelchangeDeTolerance)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAdvancedPropPage message handlers

void CAdvancedPropPage::OnControlClicked(UINT nID) 
{
	// m_isModified becomes true only when imperial units or luminance curve display flag changes. This flag
	// allow parent dialog to refresh all views to change data displayed
	if ( nID == IDC_CHECK_IMPERIAL || nID == IDC_CHECK_PREFER_LUXMETER || nID == IDC_RADIO1 || nID == IDC_RADIO2 || nID == IDC_RADIO3 || nID == IDC_CHECK_DELTAE_GRAY_LUMA || nID == IDC_HIGHLIGHT )
		m_isModified=TRUE;
	SetModified(TRUE);	
}

void CAdvancedPropPage::OnSelchangeLuxmeterComCombo()
{
	SetModified(TRUE);
}

void CAdvancedPropPage::OnSelchangeDeTolerance()
{
	m_isModified = TRUE;	// have the parent refresh every dE indicator (grid, bars, target)
	SetModified(TRUE);
}

void CAdvancedPropPage::OnSelchangedECombo() 
{
	if (m_dEform.GetCurSel() == 5)
	{
		m_dE_gray = 2;
		m_dEgrayEdit.EnableWindow(FALSE);
		((CComboBox&)m_dEgrayEdit).SetCurSel(m_dE_gray);
		gw_Weight = 0;
		m_gwWeightEdit.EnableWindow(FALSE);
		((CComboBox&)m_gwWeightEdit).SetCurSel(gw_Weight);
	}
	else if (((CComboBox&)m_dEgrayEdit).GetCurSel() == 0)
	{
		gw_Weight = 0;
		m_gwWeightEdit.EnableWindow(FALSE);
		((CComboBox&)m_gwWeightEdit).SetCurSel(gw_Weight);
	}
	else
	{
		m_dEgrayEdit.EnableWindow(TRUE);
		m_gwWeightEdit.EnableWindow(TRUE);
	}

	m_isModified=TRUE;
	m_bSave = TRUE;
	SetModified(TRUE);
}

BOOL CAdvancedPropPage::OnApply()
{
    SyncCalibMethodFromCombo();
    m_gwWeightEdit.EnableWindow(TRUE);
    m_dEgrayEdit.EnableWindow(TRUE);
    if (m_dE_form == 5)
    {
        m_dE_gray = 2;
        m_dEgrayEdit.EnableWindow(FALSE);
        gw_Weight = 0;
        m_gwWeightEdit.EnableWindow(FALSE);
    }
    else if (m_dE_gray == 0)
    {
        gw_Weight = 0;
        m_gwWeightEdit.EnableWindow(FALSE);
    }

//	m_isModified=TRUE;
//	GetConfig()->ApplySettings(FALSE);
//	m_isModified = FALSE;
	m_bSave = TRUE;
	return CPropertyPageWithHelp::OnApply();
}

UINT CAdvancedPropPage::GetHelpId ( LPSTR lpszTopic )
{
	return HID_PREF_ADVANCED;
}

BOOL CAdvancedPropPage::OnInitDialog()
{
	CPropertyPageWithHelp::OnInitDialog();

	// Populate the dE tolerance presets (localized) and select the current one.
	static const UINT ids[CColorHCFRConfig::DE_PRESET_COUNT] =
		{ IDS_DEPRESET_REFERENCE, IDS_DEPRESET_PROFESSIONAL, IDS_DEPRESET_CONSUMER, IDS_DEPRESET_RELAXED };
	m_dEtolCombo.ResetContent();
	for ( int i = 0; i < CColorHCFRConfig::DE_PRESET_COUNT; i++ )
	{
		CString s;
		s.LoadString(ids[i]);
		double good, warn;
		GetConfig()->GetDEThresholdsFor(i, good, warn);
		CString item;
		item.Format("%s (dE %g)", (LPCSTR)s, warn);	// show the tolerance (fail) limit
		m_dEtolCombo.AddString(item);
	}
	if ( m_dE_preset < 0 || m_dE_preset >= CColorHCFRConfig::DE_PRESET_COUNT )
		m_dE_preset = 1;
	m_dEtolCombo.SetCurSel(m_dE_preset);

	// A single dropdown replaces what used to be three calibration-method radios,
	// so a fourth method fits without editing the localized dialog templates. Place
	// the combobox where the first radio sat - x=13, y=48 in dialog units, the same
	// in every localized .rc, inside the "Calibration files" group box. Items come
	// from the string table and carry their CalibrationMatrixMethod value as item data.
	if ( !::IsWindow(m_calibMethodCombo.GetSafeHwnd()) )
	{
		CRect comboRect(13, 48, 13 + 180, 48 + 9);	// dialog units
		MapDialogRect(&comboRect);					// -> client pixels
		comboRect.bottom = comboRect.top + 120;		// add the drop-down list extent
		m_calibMethodCombo.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
			comboRect, this, IDC_ADV_CALIB_COMBO);
		m_calibMethodCombo.SetFont(GetFont());

		static const struct { UINT ids; int method; } kItems[] = {
			{ IDS_CALIB_RGB_MATRIX, CALIB_CLASSIC_NIST },
			{ IDS_CALIB_FCMM_NOLUM, CALIB_FCMM_NO_LUM },
			{ IDS_CALIB_FCMM_LUM,   CALIB_HCFR_DEFAULT },
			{ IDS_CALIB_BODNER,     CALIB_BODNER_THREEMATRIX },
		};
		for ( int i = 0; i < 4; i++ )
		{
			CString s; s.LoadString(kItems[i].ids);
			int idx = m_calibMethodCombo.AddString(s);
			m_calibMethodCombo.SetItemData(idx, kItems[i].method);
			if ( kItems[i].method == m_calibrationMethod )
				m_calibMethodCombo.SetCurSel(idx);
		}
		if ( m_calibMethodCombo.GetCurSel() == CB_ERR )
			m_calibMethodCombo.SetCurSel(0);
	}

	return TRUE;
}


BOOL CAdvancedPropPage::OnSetActive() 
{
	BOOL	bOk = CPropertyPageWithHelp::OnSetActive();
    m_gwWeightEdit.EnableWindow(TRUE);
    m_dEgrayEdit.EnableWindow(TRUE);
    if (m_dE_form == 5)
    {
        m_dE_gray = 2;
        m_dEgrayEdit.EnableWindow(FALSE);
        gw_Weight = 0;
        m_gwWeightEdit.EnableWindow(FALSE);
    }
    else if (m_dE_gray == 0)
    {
        gw_Weight = 0;
        m_gwWeightEdit.EnableWindow(FALSE);
    }
	m_bSave = GetConfig()->m_bSave2;
	GetConfig()->ApplySettings(FALSE);
	m_isModified=FALSE;
	SetModified(FALSE);
	return bOk;
}

// Pull the selected calibration method out of the dropdown into m_calibrationMethod.
// (There is no DDX for the combo because it is created programmatically.)
void CAdvancedPropPage::SyncCalibMethodFromCombo()
{
	if ( ::IsWindow(m_calibMethodCombo.GetSafeHwnd()) )
	{
		int idx = m_calibMethodCombo.GetCurSel();
		if ( idx != CB_ERR )
			m_calibrationMethod = (int) m_calibMethodCombo.GetItemData(idx);
	}
}

void CAdvancedPropPage::OnSelchangeCalibMethod()
{
	SyncCalibMethodFromCombo();
	SetModified(TRUE);
}

BOOL CAdvancedPropPage::OnKillActive()
{
	// Capture the dropdown selection when leaving the page (covers OK while this
	// page is active as well as navigating away before pressing OK).
	SyncCalibMethodFromCombo();
	return CPropertyPageWithHelp::OnKillActive();
}
