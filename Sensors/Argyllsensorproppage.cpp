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

// Local control IDs for the programmatically-created spectral-correction row.
// Kept out of resource.h (and the .rc templates) on purpose; values are well
// clear of the dialog's template control ids (1266..1292).
#define IDC_ARGYLL_SPECTRAL_BROWSE   1600
#define IDC_ARGYLL_SPECTRAL_CLEAR    1601
#define IDC_ARGYLL_SPECTRAL_STATUS   1602

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
    ON_BN_CLICKED(IDC_ARGYLL_SPECTRAL_BROWSE, OnSpectralBrowse)
    ON_BN_CLICKED(IDC_ARGYLL_SPECTRAL_CLEAR, OnSpectralClear)
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

    // The five language dialog templates are authored differently, which caused
    // overlapping/clipped controls. To make every language render identically, the
    // whole page is laid out here at runtime to one fixed design: every control is
    // positioned absolutely (template positions are ignored), with the note and the
    // Hi-Res caption auto-sized so multi-line text in any language has room.
    struct Mapper {
        HWND h;
        CPoint at(int x, int y) { CRect r(x, y, x + 1, y + 1); ::MapDialogRect(h, &r); return CPoint(r.left, r.top); }
        int w(int n) { CRect r(0, 0, n, 1); ::MapDialogRect(h, &r); return r.right; }
        int ht(int n) { CRect r(0, 0, 1, n); ::MapDialogRect(h, &r); return r.bottom; }
    } M = { GetSafeHwnd() };

    // Wrapped text height (px) for a control's caption at a given pixel width.
    struct Measure {
        CWnd* pg;
        int operator()(CWnd* c, int wpx) {
            CString s; c->GetWindowText(s);
            CFont* pf = c->GetFont(); if (pf == NULL) pf = pg->GetFont();
            CClientDC dc(c); CFont* pOld = dc.SelectObject(pf);
            CRect tr(0, 0, wpx, 0); dc.DrawText(s, &tr, DT_CALCRECT | DT_WORDBREAK);
            if (pOld) dc.SelectObject(pOld);
            return tr.Height();
        }
    } measure = { this };

    const int COL = 81, LBLX = 13, GRPW = 299;
    int glyphPx = ::GetSystemMetrics(SM_CXMENUCHECK) + 6;
    int lineHpx = M.ht(9);
    int gapPx   = M.ht(3);

    // Locate the two group boxes (top = Configuration, bottom = Calibration) and
    // the note (largest static), so they can be retitled / repositioned without ids.
    CWnd *pCfg = NULL, *pCal = NULL, *pNote = NULL;
    int cfgTop = INT_MAX, calTop = INT_MIN; long noteArea = 0;
    for (CWnd* ch = GetWindow(GW_CHILD); ch != NULL; ch = ch->GetNextWindow())
    {
        TCHAR cls[32] = {0}; ::GetClassName(ch->GetSafeHwnd(), cls, 32);
        CRect rc; ch->GetWindowRect(&rc); ScreenToClient(&rc);
        if (_tcsicmp(cls, _T("Button")) == 0 && (ch->GetStyle() & BS_TYPEMASK) == BS_GROUPBOX)
        {
            if (rc.top < cfgTop) { cfgTop = rc.top; pCfg = ch; }
            if (rc.top > calTop) { calTop = rc.top; pCal = ch; }
        }
        else if (_tcsicmp(cls, _T("Static")) == 0)
        {
            long a = (long)rc.Width() * rc.Height();
            if (a > noteArea) { noteArea = a; pNote = ch; }
        }
    }

    // Field rows: capture each label (at its template position) before moving.
    UINT fieldIds[] = { IDC_ARGYLLSENSOR_METER_NAME, IDC_ARGYLLSENSOR_DISPLAYTYPE_COMBO,
        IDC_ARGYLLSENSOR_READINGTYPE_COMBO, IDC_ARGYLLSENSOR_SPECTRALTYPE_COMBO, IDC_ARGYLLSENSOR_INTTIME_COMBO };
    CWnd* fieldLabels[5];
    for (int i = 0; i < 5; ++i) fieldLabels[i] = FindRowLabel(GetDlgItem(fieldIds[i]));

    int rowY[]   = { 16, 32, 48, 64, 80 };
    int fieldW[] = { 210, 210, 210, 210, 110 };
    for (int i = 0; i < 5; ++i)
    {
        CWnd* c = GetDlgItem(fieldIds[i]);
        if (c != NULL)
        {
            CPoint p = M.at(COL, rowY[i]);
            int h = (fieldIds[i] == IDC_ARGYLLSENSOR_METER_NAME) ? M.ht(12) : M.ht(90);  // combos keep dropdown height
            c->MoveWindow(p.x, p.y, M.w(fieldW[i]), h);
        }
        if (fieldLabels[i] != NULL)
        {
            CPoint lp = M.at(LBLX, rowY[i] + 3);
            fieldLabels[i]->MoveWindow(lp.x, lp.y, M.w(64), M.ht(9));
        }
    }

    // Checkboxes: Disable-AIO (top) then Use-Hi-Res (below), each auto-sized.
    int chkWpx = M.w(212);
    int chkTextPx = chkWpx - glyphPx;
    CPoint aioPt = M.at(COL, 96);
    if (!::IsWindow(m_AdaptCheckBox.GetSafeHwnd()))   // recreate (page reused across opens)
    {
        if (m_AdaptCheckBox.GetSafeHwnd() != NULL) m_AdaptCheckBox.Detach();
        CString sAIO; sAIO.LoadString(IDS_DISABLE_AIO);   // localized caption
        m_AdaptCheckBox.Create(sAIO, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_MULTILINE | WS_TABSTOP,
            CRect(aioPt.x, aioPt.y, aioPt.x + chkWpx, aioPt.y + lineHpx), this, IDC_ARGYLL_SENSOR_ADAPT);
        m_AdaptCheckBox.SetFont(GetFont());
    }
    int hAio = lineHpx;
    if (::IsWindow(m_AdaptCheckBox.GetSafeHwnd()))
    {
        int th = measure(&m_AdaptCheckBox, chkTextPx); hAio = (th + 2 > lineHpx) ? th + 2 : lineHpx;
        m_AdaptCheckBox.MoveWindow(aioPt.x, aioPt.y, chkWpx, hAio);
        m_AdaptCheckBox.SetCheck(m_DisableAIO ? BST_CHECKED : BST_UNCHECKED);
        m_AdaptCheckBox.EnableWindow(m_AIOEnabled);
    }
    int yHiRes = aioPt.y + hAio + gapPx;
    int hHiRes = lineHpx;
    CWnd* pHiRes = GetDlgItem(IDC_ARGYLL_SENSOR_HIRES);
    if (pHiRes != NULL)
    {
        int th = measure(pHiRes, chkTextPx); hHiRes = (th + 2 > lineHpx) ? th + 2 : lineHpx;
        pHiRes->MoveWindow(aioPt.x, yHiRes, chkWpx, hHiRes);
    }
    int chkBottom = yHiRes + hHiRes;

    // Configuration group encloses the fields + checkboxes.
    if (pCfg != NULL)
    {
        CPoint g = M.at(5, 4);
        pCfg->MoveWindow(g.x, g.y, M.w(GRPW), (chkBottom + gapPx) - g.y);
    }

    // Meter Calibration group: full width, retitled, with the note inside (top) and
    // the Calibrate button centred below it.
    int calTopPx = chkBottom + gapPx + M.ht(4);
    if (pCal != NULL) { CString s; s.LoadString(IDS_METER_CALIBRATION); pCal->SetWindowText(s); }

    int noteTop = calTopPx + M.ht(11);
    int noteWpx = M.w(280);
    int noteH = lineHpx;
    if (pNote != NULL)
    {
        CString s; s.LoadString(IDS_CAL_NOTE); pNote->SetWindowText(s);
        pNote->ModifyStyle(WS_BORDER, 0);
        pNote->ModifyStyleEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WS_EX_STATICEDGE);
        int th = measure(pNote, noteWpx - M.w(8)); int want = th + M.ht(6);
        noteH = (want > lineHpx) ? want : lineHpx;
        CPoint np = M.at(LBLX, 0);
        pNote->SetWindowPos(NULL, np.x, noteTop, noteWpx, noteH, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }

    int btnW = M.w(70), btnH = M.ht(14);
    int btnX = M.at(5, 0).x + (M.w(GRPW) - btnW) / 2;
    int btnY = noteTop + noteH + M.ht(6);
    CWnd* pBtn = GetDlgItem(IDC_ARGYLL_CALIBRATE);
    if (pBtn != NULL) pBtn->MoveWindow(btnX, btnY, btnW, btnH);

    int calBottom = btnY + btnH + M.ht(6);
    if (pCal != NULL) { CPoint cp = M.at(5, 0); pCal->MoveWindow(cp.x, calTopPx, M.w(GRPW), calBottom - calTopPx); }

    // Debug checkbox below the calibration group.
    CWnd* pDbg = GetDlgItem(IDC_ARGYLL_SENSOR_DEBUG_CB);
    if (pDbg != NULL) { CPoint dp = M.at(LBLX + 1, 0); pDbg->MoveWindow(dp.x, calBottom + M.ht(4), M.w(220), M.ht(10)); }

    // Spectral (ccss/CSV) correction row — created only for meters that support
    // spectral samples (i1D3 etc.). Programmatic so no .rc template is needed.
    if ( m_pSensor != NULL && m_pSensor->MeterSupportsSpectralSamples() )
    {
        int specTop = calBottom + M.ht(4) + M.ht(10) + M.ht(8);
        CPoint lp = M.at(LBLX, specTop);
        if ( !::IsWindow(m_spectralStatus.GetSafeHwnd()) )
            m_spectralStatus.Create("", WS_CHILD | WS_VISIBLE | SS_LEFT,
                CRect(lp.x, lp.y, lp.x + M.w(280), lp.y + M.ht(18)), this, IDC_ARGYLL_SPECTRAL_STATUS);
        m_spectralStatus.SetFont(GetFont());
        m_spectralStatus.MoveWindow(lp.x, lp.y, M.w(280), M.ht(18));

        int btnY = specTop + M.ht(20);
        int bw = M.w(64), bh = M.ht(14), gap = M.w(6);
        CPoint bp = M.at(LBLX, btnY);
        if ( !::IsWindow(m_spectralBrowseBtn.GetSafeHwnd()) )
            m_spectralBrowseBtn.Create("Browse...", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                CRect(bp.x, bp.y, bp.x + bw, bp.y + bh), this, IDC_ARGYLL_SPECTRAL_BROWSE);
        m_spectralBrowseBtn.SetFont(GetFont());
        m_spectralBrowseBtn.MoveWindow(bp.x, bp.y, bw, bh);

        if ( !::IsWindow(m_spectralClearBtn.GetSafeHwnd()) )
            m_spectralClearBtn.Create("Clear", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                CRect(bp.x + bw + gap, bp.y, bp.x + 2*bw + gap, bp.y + bh), this, IDC_ARGYLL_SPECTRAL_CLEAR);
        m_spectralClearBtn.SetFont(GetFont());
        m_spectralClearBtn.MoveWindow(bp.x + bw + gap, bp.y, bw, bh);

        RefreshSpectralStatus();
    }

    return bRet;
}

void CArgyllSensorPropPage::RefreshSpectralStatus()
{
    if ( m_pSensor == NULL || !::IsWindow(m_spectralStatus.GetSafeHwnd()) )
        return;
    CString s;
    if ( m_pSensor->HasSpectralCorrection() )
    {
        CString desc = m_pSensor->GetSpectralCorrectionDesc();
        s = "Spectral correction: " + (desc.IsEmpty() ? CString("(loaded)") : desc);
    }
    else
        s = "Spectral correction: None";
    m_spectralStatus.SetWindowText(s);
    if ( ::IsWindow(m_spectralClearBtn.GetSafeHwnd()) )
        m_spectralClearBtn.EnableWindow(m_pSensor->HasSpectralCorrection());
}

void CArgyllSensorPropPage::OnSpectralBrowse()
{
    if ( m_pSensor == NULL )
        return;
    CFileDialog dlg(TRUE, "ccss", NULL, OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
        "Spectral correction (*.ccss;*.csv)|*.ccss;*.csv|CCSS (*.ccss)|*.ccss|ColourSpace CSV (*.csv)|*.csv||");
    if ( dlg.DoModal() == IDOK )
    {
        if ( m_pSensor->ApplySpectralCorrection(dlg.GetPathName()) )
            RefreshSpectralStatus();
    }
}

void CArgyllSensorPropPage::OnSpectralClear()
{
    if ( m_pSensor == NULL )
        return;
    m_pSensor->ClearSpectralCorrection();
    RefreshSpectralStatus();
}
