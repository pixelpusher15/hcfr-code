/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2008 Association Homecinema Francophone.  All rights reserved.
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
//    Georges GALLERAND
//    John Adcock
//    Ian C
/////////////////////////////////////////////////////////////////////////////

// ArgyllSensorPropPage.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "ArgyllSensor.h"
#include "ArgyllSensorPropPage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CArgyllSensorPropPage property page

IMPLEMENT_DYNCREATE(CArgyllSensorPropPage, CPropertyPageWithHelp)

CArgyllSensorPropPage::CArgyllSensorPropPage() : CPropertyPageWithHelp(CArgyllSensorPropPage::IDD)
{
    //{{AFX_DATA_INIT(CArgyllSensorPropPage)
    m_DisplayType = 0;
    m_SpectralType = "Default";
    m_intTime = 1;
    m_ReadingType = 0;
    m_MeterName = "";
    m_DebugMode = FALSE;
    m_HiRes = FALSE;
    m_Adapt = FALSE;
    //}}AFX_DATA_INIT
    m_bNotifyAmbientSwitch = FALSE;
    m_DisableAIO = FALSE;
    m_AIOEnabled = FALSE;
    m_bAdaptCreated = FALSE;
}

CArgyllSensorPropPage::~CArgyllSensorPropPage()
{
}

void CArgyllSensorPropPage::DoDataExchange(CDataExchange* pDX)
{
    CPropertyPageWithHelp::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_ARGYLLSENSOR_DISPLAYTYPE_COMBO, m_DisplayTypeCombo);
    DDX_Control(pDX, IDC_ARGYLLSENSOR_SPECTRALTYPE_COMBO, m_SpectralTypeCombo);
    DDX_Control(pDX, IDC_ARGYLLSENSOR_INTTIME_COMBO, m_IntTypeCombo);
    DDX_Control(pDX, IDC_ARGYLL_SENSOR_HIRES, m_HiResCheckBox);
//    DDX_Control(pDX, IDC_ARGYLL_SENSOR_ADAPT, m_AdaptCheckBox);

    if(m_DisplayTypeCombo.GetCount() == 0)
    {
        m_pSensor->FillDisplayTypeCombo(m_DisplayTypeCombo);
    }
    m_DisplayTypeCombo.EnableWindow((m_DisplayTypeCombo.GetCount() != 0)?TRUE:FALSE);
    if (m_SpectralTypeCombo.GetCurSel() == CB_ERR)
    {
        m_pSensor->FillSpectralTypeCombo(m_SpectralTypeCombo);
        m_SpectralTypeCombo.SetCurSel(0);
    }
    m_SpectralTypeCombo.EnableWindow(m_obTypeEnabled);
    m_IntTypeCombo.EnableWindow(m_intTimeEnabled);

    m_HiResCheckBox.EnableWindow(m_HiResCheckBoxEnabled);
    // m_AdaptCheckBox is created programmatically in OnInitDialog; guard against
    // a NULL HWND here (this exchange also runs during base OnInitDialog, before
    // the control exists).
    if (::IsWindow(m_AdaptCheckBox.GetSafeHwnd()))
        m_AdaptCheckBox.EnableWindow(m_AIOEnabled);
	m_HiRes = (m_HiResCheckBoxEnabled?m_HiRes:0);

    if ( m_DisplayTypeCombo.GetCount() == 0 && m_ReadingType == 0 && m_MeterName == "X-Rite i1 DisplayPro, ColorMunki Display" )
    {
        m_ReadingType = 2;
        // Defer the modal notification: showing a message box inside DoDataExchange
        // re-enters the modal loop mid data-transfer (MFC winocc.cpp assert) and can
        // pop during a measurement. Flag it and notify from OnSetActive instead.
        m_bNotifyAmbientSwitch = TRUE;
    }

    DDX_CBIndex(pDX, IDC_ARGYLLSENSOR_DISPLAYTYPE_COMBO, m_DisplayType);
    DDX_CBString(pDX, IDC_ARGYLLSENSOR_SPECTRALTYPE_COMBO, m_SpectralType);
    DDX_CBIndex(pDX, IDC_ARGYLLSENSOR_INTTIME_COMBO, m_intTime);
	DDX_CBIndex(pDX, IDC_ARGYLLSENSOR_READINGTYPE_COMBO, m_ReadingType);
    DDX_Text(pDX, IDC_ARGYLLSENSOR_METER_NAME, m_MeterName);
    DDX_Check(pDX, IDC_ARGYLL_SENSOR_DEBUG_CB, m_DebugMode);
    DDX_Check(pDX, IDC_ARGYLL_SENSOR_HIRES, m_HiRes);

    // The "Disable AIO" checkbox is created programmatically (see OnInitDialog),
    // so transfer its state by hand rather than via a resource-bound DDX_Check.
    if (pDX->m_bSaveAndValidate && ::IsWindow(m_AdaptCheckBox.GetSafeHwnd()))
        m_DisableAIO = (m_AdaptCheckBox.GetCheck() == BST_CHECKED);
}


BEGIN_MESSAGE_MAP(CArgyllSensorPropPage, CPropertyPageWithHelp)
    //{{AFX_MSG_MAP(CArgyllSensorPropPage)
    ON_BN_CLICKED(IDC_ARGYLL_CALIBRATE, OnCalibrate)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CArgyllSensorPropPage message handlers
/////////////////////////////////////////////////////////////////////////////

void CArgyllSensorPropPage::OnCalibrate()
{
    UpdateData(TRUE);
    m_pSensor->GetPropertiesSheetValues();
	m_pSensor->Calibrate();
}


UINT CArgyllSensorPropPage::GetHelpId ( LPSTR lpszTopic )
{
    return HID_SENSOR_EYEONE;
}

BOOL CArgyllSensorPropPage::OnSetActive() 
{
    // TODO: Add your specialized code here and/or call the base class
    if (m_SpectralType == "")
        m_SpectralType = "Default";
    BOOL bRet = CPropertyPageWithHelp::OnSetActive();
    if (bRet && m_bNotifyAmbientSwitch)
    {
        m_bNotifyAmbientSwitch = FALSE;
        GetColorApp()->InMeasureMessageBox("Diffuser is deployed, switching to Ambient mode","Wrong mode!",MB_OK);
    }
    return bRet;
}

BOOL CArgyllSensorPropPage::OnInitDialog()
{
    BOOL bRet = CPropertyPageWithHelp::OnInitDialog();

    // Create the "Disable AIO" checkbox programmatically. The dialog template
    // lives in the active language resource DLL (which may not be rebuilt for
    // every language), so we add the control at runtime instead. It is placed in
    // the free area to the right of the Hi-Res checkbox, inside the Configuration
    // group. Coordinates are in dialog units, mapped to pixels for the DPI.
    if (!m_bAdaptCreated)
    {
        CRect rc(114, 86, 166, 106);    // dlu: right of Hi-Res, left of Integration Time
        MapDialogRect(&rc);
        if (m_AdaptCheckBox.Create(_T("Disable Rev. B AIO"),
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_MULTILINE | WS_TABSTOP,
                rc, this, IDC_ARGYLL_SENSOR_ADAPT))
        {
            m_AdaptCheckBox.SetFont(GetFont());
            m_bAdaptCreated = TRUE;
        }
    }
    if (::IsWindow(m_AdaptCheckBox.GetSafeHwnd()))
    {
        m_AdaptCheckBox.SetCheck(m_DisableAIO ? BST_CHECKED : BST_UNCHECKED);
        m_AdaptCheckBox.EnableWindow(m_AIOEnabled);
    }
    return bRet;
}
