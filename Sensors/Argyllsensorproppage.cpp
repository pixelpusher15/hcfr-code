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

CWnd* CArgyllSensorPropPage::FindRowLabel(CWnd* pCtrl)
{
    if (pCtrl == NULL)
        return NULL;
    CRect rcCtrl;
    pCtrl->GetWindowRect(&rcCtrl);   // screen coords
    CWnd* pBest = NULL;
    int bestRight = -1000000;
    for (CWnd* pChild = GetWindow(GW_CHILD); pChild != NULL; pChild = pChild->GetNextWindow())
    {
        TCHAR cls[32] = {0};
        ::GetClassName(pChild->GetSafeHwnd(), cls, 32);
        if (_tcsicmp(cls, _T("Static")) != 0)
            continue;
        CRect rc;
        pChild->GetWindowRect(&rc);
        // require vertical overlap with the control's row and sit to its left
        if (rc.bottom <= rcCtrl.top || rc.top >= rcCtrl.bottom)
            continue;
        if (rc.right > rcCtrl.left + 4)
            continue;
        if (rc.right > bestRight)
        {
            bestRight = rc.right;
            pBest = pChild;
        }
    }
    return pBest;
}

BOOL CArgyllSensorPropPage::OnInitDialog()
{
    BOOL bRet = CPropertyPageWithHelp::OnInitDialog();

    // dlu -> client pixel origin helper
    struct Mapper { HWND h; CPoint at(int x, int y) { CRect r(x, y, x + 1, y + 1); ::MapDialogRect(h, &r); return CPoint(r.left, r.top); } } M = { GetSafeHwnd() };

    // Relayout the Configuration group into one clean left-aligned column:
    // every labelled field (incl. Integration Time, previously floating top-right)
    // becomes a row with the label at x=13 and the control at x=81. Done at runtime
    // (rather than in the .rc) so it applies to whichever language DLL is loaded.
    // Labels are IDC_STATIC, so they are matched to their control by geometry.
    const int CTRL_X = 81, LBL_X = 13;
    struct Row { UINT id; int y; } rows[] = {
        { IDC_ARGYLLSENSOR_METER_NAME,        14 },
        { IDC_ARGYLLSENSOR_DISPLAYTYPE_COMBO, 30 },
        { IDC_ARGYLLSENSOR_READINGTYPE_COMBO, 46 },
        { IDC_ARGYLLSENSOR_SPECTRALTYPE_COMBO, 62 },
        { IDC_ARGYLLSENSOR_INTTIME_COMBO,     78 },
    };
    for (int i = 0; i < (int)(sizeof(rows) / sizeof(rows[0])); ++i)
    {
        CWnd* pCtrl = GetDlgItem(rows[i].id);
        if (pCtrl == NULL)
            continue;
        CWnd* pLbl = FindRowLabel(pCtrl);
        CPoint cc = M.at(CTRL_X, rows[i].y);
        pCtrl->SetWindowPos(NULL, cc.x, cc.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        if (pLbl != NULL)
        {
            CPoint lc = M.at(LBL_X, rows[i].y + 3);
            if (rows[i].id == IDC_ARGYLLSENSOR_INTTIME_COMBO)
            {
                // the "Integration Time" label was narrow (2-line) in its old
                // corner spot; widen it to match the other row labels
                CRect rcLbl; pLbl->GetWindowRect(&rcLbl);
                CRect wr(0, 0, 62, 8); MapDialogRect(&wr);
                pLbl->SetWindowPos(NULL, lc.x, lc.y, wr.right, rcLbl.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
            }
            else
            {
                pLbl->SetWindowPos(NULL, lc.x, lc.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
    }

    // The property sheet sizes every page to the tallest one (the Sensor-matrix
    // page is 191 dlu vs this page's 176), so there is spare vertical room at the
    // bottom at runtime. Push the lower block down and grow the Configuration group
    // so the rows can use a comfortable pitch and the two checkboxes get their own
    // stacked rows below the fields.
    const int PUSH = 18;    // dlu, downward shift of the lower block
    const int GROW = 16;    // dlu, extra height for the Configuration group box
    CRect rP(0, 0, 1, PUSH); MapDialogRect(&rP); int pushPx = rP.bottom;
    CRect rG(0, 0, 1, GROW); MapDialogRect(&rG); int growPx = rG.bottom;

    // Locate the two group boxes (class "Button" + BS_GROUPBOX) by vertical order,
    // and the help text (the largest static), so they can be relocated without ids.
    CWnd *pCfgGroup = NULL, *pCalGroup = NULL, *pHelp = NULL;
    int cfgTop = INT_MAX, calTop = INT_MIN;
    long helpArea = 0;
    for (CWnd* pChild = GetWindow(GW_CHILD); pChild != NULL; pChild = pChild->GetNextWindow())
    {
        TCHAR cls[32] = {0};
        ::GetClassName(pChild->GetSafeHwnd(), cls, 32);
        CRect rc; pChild->GetWindowRect(&rc); ScreenToClient(&rc);
        if (_tcsicmp(cls, _T("Button")) == 0 && (pChild->GetStyle() & BS_TYPEMASK) == BS_GROUPBOX)
        {
            if (rc.top < cfgTop) { cfgTop = rc.top; pCfgGroup = pChild; }
            if (rc.top > calTop) { calTop = rc.top; pCalGroup = pChild; }
        }
        else if (_tcsicmp(cls, _T("Static")) == 0)
        {
            long a = (long)rc.Width() * rc.Height();
            if (a > helpArea) { helpArea = a; pHelp = pChild; }
        }
    }

    CWnd* lower[] = { pCalGroup, GetDlgItem(IDC_ARGYLL_CALIBRATE), GetDlgItem(IDC_ARGYLL_SENSOR_DEBUG_CB), pHelp };
    for (int i = 0; i < 4; ++i)
    {
        if (lower[i] == NULL) continue;
        CRect rc; lower[i]->GetWindowRect(&rc); ScreenToClient(&rc);
        lower[i]->SetWindowPos(NULL, rc.left, rc.top + pushPx, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (pCfgGroup != NULL)
    {
        CRect rc; pCfgGroup->GetWindowRect(&rc); ScreenToClient(&rc);
        pCfgGroup->SetWindowPos(NULL, 0, 0, rc.Width(), rc.Height() + growPx, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    // Two stacked checkbox rows, left edge aligned with the dropdowns (x = CTRL_X),
    // each a single full-width line so the long Hi-Res caption never wraps.
    const int HIRES_Y = 96;
    const int AIO_Y = 108;
    CWnd* pHiRes = GetDlgItem(IDC_ARGYLL_SENSOR_HIRES);
    if (pHiRes != NULL)
    {
        CRect rc(CTRL_X, HIRES_Y, CTRL_X + 206, HIRES_Y + 10);
        MapDialogRect(&rc);
        pHiRes->MoveWindow(&rc);
    }

    // (Re)create the checkbox if its window doesn't exist. The page object is
    // reused across property-sheet opens (see CSensor::Configure), so a guard that
    // only created it once would leave the control missing on every reopen; detach
    // any handle left over from a previous, now-destroyed page instance first.
    if (!::IsWindow(m_AdaptCheckBox.GetSafeHwnd()))
    {
        if (m_AdaptCheckBox.GetSafeHwnd() != NULL)
            m_AdaptCheckBox.Detach();
        CRect rc(CTRL_X, AIO_Y, CTRL_X + 206, AIO_Y + 10);
        MapDialogRect(&rc);
        if (m_AdaptCheckBox.Create(_T("Disable Rev. B AIO"),
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_MULTILINE | WS_TABSTOP,
                rc, this, IDC_ARGYLL_SENSOR_ADAPT))
        {
            m_AdaptCheckBox.SetFont(GetFont());
        }
    }
    if (::IsWindow(m_AdaptCheckBox.GetSafeHwnd()))
    {
        m_AdaptCheckBox.SetCheck(m_DisableAIO ? BST_CHECKED : BST_UNCHECKED);
        m_AdaptCheckBox.EnableWindow(m_AIOEnabled);
    }
    return bRet;
}
