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

// GDIGenePropPage.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "GDIGenerator.h"
#include <uxtheme.h>
#include <vsstyle.h>
#pragma comment(lib, "uxtheme.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////
// CGDIGenePropPage property page

IMPLEMENT_DYNCREATE(CGDIGenePropPage, CPropertyPageWithHelp)

CGDIGenePropPage::CGDIGenePropPage() : CPropertyPageWithHelp(CGDIGenePropPage::IDD)
{
	//{{AFX_DATA_INIT(CGDIGenePropPage)
	m_rectSizePercent = 0;
	m_offsetx = 0;
	m_pgenQuerying = FALSE;
	m_offsety =0;
	m_bgStimPercent = 0;
	m_Intensity = 0;
	//}}AFX_DATA_INIT
	m_activeMonitorNum = 0;
	m_pGenerator = NULL;
	m_nDisplayMode = GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE);
	m_b16_235 = FALSE;
	m_busePic = FALSE;
    m_madVR_3d = FALSE;
    m_madVR_vLUT = FALSE;
	m_madVR_HDR = FALSE;
	m_madVR_OSD = FALSE;
	m_bdispTrip = FALSE;
	m_bdispTrip = GetConfig()->GetProfileInt("GDIGenerator","DISPLAYTRIPLETS",1);
	m_brPi_user = FALSE;
	m_b10bitPGen = FALSE;
	m_b10bitMadvr = FALSE;
	m_bLinear = FALSE;
	m_bHdr10 = GetConfig()->GetProfileInt("GDIGenerator","EnableHDR10",0);
	m_castHasDevice = false;
	m_doScreenBlanking = FALSE;

	m_grpDisplay = m_grpMadvr = m_grpCast = m_grpPgen = m_grpSignal = m_grpPattern = m_grpBlanking = NULL;
	m_lblOutput = m_lblScreen = m_lblSize = m_lblApl = m_lblIntensity = NULL;
	m_lblXoff = m_lblYoff = m_lblCastDev = m_lblRange = m_lblOffset = NULL;
}

CGDIGenePropPage::~CGDIGenePropPage()
{
}

void CGDIGenePropPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPageWithHelp::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CGDIGenePropPage)
    DDX_Control(pDX, IDC_MADVR_3D, m_madVREdit);
	DDX_Control(pDX, IDC_MONITOR_COMBO, m_monitorComboCtrl);
	DDX_Control(pDX, IDC_CCAST_COMBO, m_cCastComboCtrl);
    DDX_Control(pDX, IDC_MADVR_3D2, m_madVREdit2);
    DDX_Control(pDX, IDC_MADVR_HDR, m_madVREdit4);
    DDX_Control(pDX, IDC_MADVR_OSD, m_madVREdit3);
    DDX_Control(pDX, IDC_USEPIC, m_usePicEdit);
	DDX_Text(pDX, IDC_PATTERNSIZE_EDIT, m_rectSizePercent);
	DDX_Text(pDX, IDC_BGSTIM_EDIT, m_bgStimPercent);
	DDX_Text(pDX, IDC_INTENSITY_EDIT, m_Intensity);
	DDX_Text(pDX, IDC_XOFFSET_EDIT, m_offsetx);
	DDX_Text(pDX, IDC_YOFFSET_EDIT, m_offsety);
	DDX_Control(pDX, IDC_INTENSITY_EDIT, m_bIntensity);
	DDV_MinMaxUInt(pDX, m_rectSizePercent, 1, 100);
	DDV_MinMaxUInt(pDX, m_bgStimPercent, 0, 100);
	DDV_MinMaxUInt(pDX, m_Intensity, 1, 100);
	DDX_Check(pDX, IDC_MADVR_3D, m_madVR_3d);
	DDX_Check(pDX, IDC_MADVR_3D2, m_madVR_vLUT);
	DDX_Check(pDX, IDC_MADVR_HDR, m_madVR_HDR);
	DDX_Check(pDX, IDC_MADVR_OSD, m_madVR_OSD);
	DDX_Check(pDX, IDC_USEPIC, m_busePic);
	DDX_Check(pDX, IDC_DISP_TRIP, m_bdispTrip);
	DDX_Check(pDX, IDC_DISP_TRIP2, m_bLinear);
	DDX_Check(pDX, IDC_DISP_TRIP3, m_brPi_user);
	DDX_Check(pDX, IDC_ENBL_HDR, m_bHdr10);
	//}}AFX_DATA_MAP
}


#define WM_PGEN_QUERY_DONE (WM_USER + 172)

#define IDC_PGEN_FORMAT_COMBO   (IDC_PGEN_AVI_BASE + 1)
#define IDC_PGEN_DYNRANGE_COMBO (IDC_PGEN_AVI_BASE + 5)

BEGIN_MESSAGE_MAP(CGDIGenePropPage, CPropertyPageWithHelp)
	//{{AFX_MSG_MAP(CGDIGenePropPage)
	ON_BN_CLICKED(IDC_OVERLAY, OnTestOverlay)
	//}}AFX_MSG_MAP
	ON_CBN_DROPDOWN(IDC_MONITOR_COMBO, OnDropdownMonitorCombo)
	ON_CBN_SELCHANGE(IDC_GEN_OUTPUT_COMBO, OnSelchangeOutput)
	ON_BN_CLICKED(IDC_DISP_TRIP3, OnUserPatternClick)
	ON_BN_CLICKED(IDC_PGEN_10BIT_CHECK, On10bitClick)
	ON_BN_CLICKED(IDC_PGEN_SETTINGS_BTN, OnPgenSettings)
	ON_BN_CLICKED(IDC_PGEN_REFRESH_BTN, OnPgenRefresh)
	ON_MESSAGE(WM_PGEN_QUERY_DONE, OnPgenQueryDone)
	ON_WM_CTLCOLOR()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Runtime-built layout helpers (mirrors the redesigned References page).
// The .rc template positions for IDD_GENERATOR_GDI_PROP_PAGE are ignored;
// every visible control is placed here so all 5 localized templates yield one
// identical layout. Rows inside a group use a fixed pitch and groups are
// stacked top-to-bottom, so no two controls can overlap.
namespace {

// dlu metrics. Group spans the dialog width (x = GRP_X .. GRP_X+GRP_W on a
// 186-dlu page). Rows: a labelled field puts the label at LBL_X and the input
// at FLD_X; checkboxes span the group. ROW_F/ROW_C are the vertical pitches.
const int GRP_X = 3, GRP_W = 180;
const int LBL_X = 10, FLD_X = 98, FLD_W = 26, CHK_W = 168;
const int TOP_INSET = 11, ROW_F = 15, ROW_C = 15, BOT_PAD = 2, GRP_GAP = 4;

struct DlgMap
{
    HWND h;
    CPoint at(int x, int y) { CRect r(x, y, x + 1, y + 1); ::MapDialogRect(h, &r); return CPoint(r.left, r.top); }
    int w(int n)  { CRect r(0, 0, n, 1); ::MapDialogRect(h, &r); return r.right; }
    int ht(int n) { CRect r(0, 0, 1, n); ::MapDialogRect(h, &r); return r.bottom; }
};

static CString LS(UINT id)
{
    CString s;
    if (id) s.LoadString(id);
    return s;
}

static int CALLBACK PgFontEnumProc(const LOGFONT*, const TEXTMETRIC*, DWORD, LPARAM lp) { *(BOOL*)lp = TRUE; return 0; }
static void PgMakeGlyphFont(CFont& f, int pt = 9)
{
	HDC hdc = ::GetDC(NULL);
	int px = -MulDiv(pt, ::GetDeviceCaps(hdc, LOGPIXELSY), 72);
	LOGFONT lf; ZeroMemory(&lf, sizeof(lf));
	lf.lfHeight = px; lf.lfCharSet = DEFAULT_CHARSET;
	lstrcpy(lf.lfFaceName, _T("Segoe Fluent Icons"));
	BOOL found = FALSE;
	LOGFONT probe; ZeroMemory(&probe, sizeof(probe)); probe.lfCharSet = DEFAULT_CHARSET;
	lstrcpy(probe.lfFaceName, _T("Segoe Fluent Icons"));
	::EnumFontFamiliesEx(hdc, &probe, PgFontEnumProc, (LPARAM)&found, 0);
	if (!found) lstrcpy(lf.lfFaceName, _T("Segoe MDL2 Assets"));
	::ReleaseDC(NULL, hdc);
	f.CreateFontIndirect(&lf);
}
static void PgMakeGlyphBtn(CButton& btn, CWnd* parent, CFont& gf, int id, const wchar_t* glyph, CPoint pt, int w, int h, DWORD exStyle = 0)
{
	{ HWND old = btn.Detach(); if (old) ::DestroyWindow(old); }
	HWND hh = ::CreateWindowExW(exStyle, L"BUTTON", glyph, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, pt.x, pt.y, w, h, parent->GetSafeHwnd(), (HMENU)(INT_PTR)id, AfxGetInstanceHandle(), NULL);
	if (hh) { btn.Attach(hh); ::SendMessageW(hh, WM_SETFONT, (WPARAM)gf.GetSafeHandle(), TRUE); }
}

static void PgHeaderRule(CWnd* parent, CStatic& line, CStatic& lbl, CFont* font, int rightPx, BOOL visible)
{
	CString s; lbl.GetWindowText(s);
	CClientDC dc(parent);
	CFont* of = dc.SelectObject(font);
	CSize sz = dc.GetTextExtent(s);
	dc.SelectObject(of);
	CRect lr; lbl.GetWindowRect(&lr); parent->ScreenToClient(&lr);
	int x1 = lr.left + sz.cx + 6;
	int yc = lr.top + lr.Height() / 2;
	if (line.GetSafeHwnd()) line.DestroyWindow();
	DWORD st = WS_CHILD | SS_ETCHEDHORZ; if (visible) st |= WS_VISIBLE;
	line.Create(_T(""), st, CRect(x1, yc, rightPx, yc + 2), parent);
}

static LRESULT CALLBACK PgReadoutProc(HWND h, UINT msg, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR ref)
{
	if (msg == WM_NCDESTROY) { ::RemoveWindowSubclass(h, PgReadoutProc, id); return ::DefSubclassProc(h, msg, w, l); }
	if (msg == WM_SIZE)
	{
		LRESULT r = ::DefSubclassProc(h, msg, w, l);
		RECT rc; ::GetClientRect(h, &rc);
		RECT fr; fr.left = 7; fr.top = 4; fr.right = rc.right - 5; fr.bottom = rc.bottom;
		::SendMessage(h, EM_SETRECTNP, 0, (LPARAM)&fr);
		return r;
	}
	if (msg == WM_PAINT)
	{
		LRESULT r = ::DefSubclassProc(h, msg, w, l);
		RECT rc; ::GetClientRect(h, &rc);
		HDC dc = ::GetDC(h);
		HBRUSH bbr = ::CreateSolidBrush(RGB(207, 224, 243));
		::FrameRect(dc, &rc, bbr);
		::DeleteObject(bbr);
		::ReleaseDC(h, dc);
		return r;
	}
	return ::DefSubclassProc(h, msg, w, l);
}

static CStatic* AddText(CWnd* pg, CObArray& all, CFont* font, DlgMap& M,
                        const CString& text, int x, int y, int w, int h, DWORD style = SS_LEFT)
{
    CStatic* p = new CStatic();
    CPoint pt = M.at(x, y);
    p->Create(text, WS_CHILD | WS_VISIBLE | style, CRect(pt.x, pt.y, pt.x + M.w(w), pt.y + M.ht(h)), pg);
    p->SetFont(font);
    all.Add(p);
    return p;
}

static CButton* AddGroup(CWnd* pg, CObArray& all, CFont* font, DlgMap& M,
                         UINT ids, int x, int y, int w, int h)
{
    CButton* p = new CButton();
    CPoint pt = M.at(x, y);
    p->Create(LS(ids), WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(pt.x, pt.y, pt.x + M.w(w), pt.y + M.ht(h)), pg, (UINT)IDC_STATIC);
    p->SetFont(font);
    all.Add(p);
    return p;
}

static void MoveWnd(CWnd* c, DlgMap& M, int x, int y, int w, int h)
{
    if (!c || !c->GetSafeHwnd()) return;
    CPoint pt = M.at(x, y);
    c->MoveWindow(pt.x, pt.y, M.w(w), M.ht(h));
}

static void ShowIds(CWnd* pg, const UINT* ids, int n, BOOL show)
{
    for (int i = 0; i < n; i++)
    {
        CWnd* c = pg->GetDlgItem(ids[i]);
        if (c) c->ShowWindow(show ? SW_SHOW : SW_HIDE);
    }
}

// A labelled-field row: localized label at the left column, numeric input at
// the field column.
static void PlaceLF(CStatic* lbl, CWnd* field, DlgMap& M, int cy)
{
    if (lbl) { CPoint p = M.at(LBL_X, cy + 2); lbl->MoveWindow(p.x, p.y, M.w(84), M.ht(9)); lbl->ShowWindow(SW_SHOW); }
    if (field) { CPoint p = M.at(FLD_X, cy); field->MoveWindow(p.x, p.y, M.w(FLD_W), M.ht(12)); field->ShowWindow(SW_SHOW); }
}

// A full-width checkbox row.
static void PlaceChk(CWnd* chk, DlgMap& M, int cy)
{
    if (!chk || !chk->GetSafeHwnd()) return;
    CPoint p = M.at(LBL_X, cy); chk->MoveWindow(p.x, p.y, M.w(CHK_W), M.ht(10)); chk->ShowWindow(SW_SHOW);
}

static void PlaceGroup(CButton* g, DlgMap& M, int top, int h, int rightPx)
{
	if (!g || !g->GetSafeHwnd()) return;
	CPoint tp = M.at(GRP_X, top);
	g->MoveWindow(tp.x, tp.y, rightPx - tp.x, M.ht(h));
	g->ShowWindow(SW_SHOW);
}

// Pattern-output dropdown order <-> stored DISPLAY_* mode.
static const int kComboToMode[6] = { DISPLAY_GDI, DISPLAY_GDI_nBG, DISPLAY_GDI_Hide, DISPLAY_madVR, DISPLAY_ccast, DISPLAY_rPI };

} // namespace

int CGDIGenePropPage::ComboToMode(int sel)
{
    if (sel < 0 || sel > 5) return DISPLAY_GDI;
    return kComboToMode[sel];
}

int CGDIGenePropPage::ModeToCombo(int mode)
{
    for (int i = 0; i < 6; i++) if (kComboToMode[i] == mode) return i;
    return 0;
}

/////////////////////////////////////////////////////////////////////////////
// CGDIGenePropPage message handlers

BOOL CGDIGenePropPage::OnInitDialog()
{
	CPropertyPageWithHelp::OnInitDialog();
	BuildRuntimeLayout();
	Relayout();
	return TRUE;
}

void CGDIGenePropPage::BuildRuntimeLayout()
{
	// Page object is reused across opens: drop any controls from a prior open.
	for (int i = 0; i < m_dynAll.GetSize(); i++)
	{
		CWnd* c = (CWnd*)m_dynAll.GetAt(i);
		if (c) { if (c->GetSafeHwnd()) c->DestroyWindow(); delete c; }
	}
	m_dynAll.RemoveAll();
	m_grpDisplay = m_grpMadvr = m_grpCast = m_grpPgen = m_grpSignal = m_grpPattern = m_grpBlanking = NULL;
	m_lblOutput = m_lblScreen = m_lblSize = m_lblApl = m_lblIntensity = NULL;
	m_lblXoff = m_lblYoff = m_lblCastDev = m_lblRange = m_lblOffset = NULL;

	DlgMap M; M.h = GetSafeHwnd();
	CFont* font = GetFont();

	// Hide the template decoration (group boxes + labels carry IDC_STATIC).
	for (CWnd* c = GetWindow(GW_CHILD); c != NULL; c = c->GetWindow(GW_HWNDNEXT))
	{
		int id = c->GetDlgCtrlID();
		if (id <= 0 || id == 0xffff) c->ShowWindow(SW_HIDE);
	}
	// Hide the old display-mode radios and the dead "Test Overlay" button.
	static const UINT hideIds[] = { IDC_RADIO1, IDC_RADIO3, IDC_RADIO4, IDC_RADIO5, IDC_RADIO6, IDC_RADIO8, IDC_OVERLAY };
	ShowIds(this, hideIds, sizeof(hideIds) / sizeof(hideIds[0]), FALSE);

	// Relabel the reused template controls with localized strings.
	GetDlgItem(IDC_USEPIC)->SetWindowText(LS(IDS_GEN_USE_IMAGE_BG));
	GetDlgItem(IDC_ENBL_HDR)->SetWindowText(LS(IDS_GEN_HDR10));
	GetDlgItem(IDC_DISP_TRIP2)->SetWindowText(LS(IDS_GEN_BYPASS_VLUT));
	GetDlgItem(IDC_DISP_TRIP)->SetWindowText(LS(IDS_GEN_LOG_TRIPLETS));
	GetDlgItem(IDC_DISP_TRIP3)->SetWindowText(LS(IDS_GEN_USER_PATTERN));
	GetDlgItem(IDC_MADVR_3D)->SetWindowText(LS(IDS_GEN_DISABLE_3DLUT));
	GetDlgItem(IDC_MADVR_3D2)->SetWindowText(LS(IDS_GEN_DISABLE_VLUT));
	GetDlgItem(IDC_MADVR_OSD)->SetWindowText(LS(IDS_GEN_OSD));
	GetDlgItem(IDC_MADVR_HDR)->SetWindowText(LS(IDS_GEN_HDR_PASS));
	GetDlgItem(IDC_RGBLEVEL_RADIO1)->SetWindowText("0 - 255");
	GetDlgItem(IDC_RGBLEVEL_RADIO2)->SetWindowText("16 - 235");

	// Pattern-output dropdown (created at runtime, like the References combo).
	m_lblOutput = AddText(this, m_dynAll, font, M, LS(IDS_GEN_OUTPUT), LBL_X, 9, 60, 9);
	if (m_outputCombo.GetSafeHwnd()) m_outputCombo.DestroyWindow();
	{
		CPoint cpt = M.at(68, 7);
		m_outputCombo.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
			CRect(cpt.x, cpt.y, cpt.x + M.w(115), cpt.y + M.ht(120)), this, IDC_GEN_OUTPUT_COMBO);
		m_outputCombo.SetFont(font);
		m_outputCombo.AddString(LS(IDS_GEN_OUT_FULLSCREEN));
		m_outputCombo.AddString(LS(IDS_GEN_OUT_OVERLAY));
		m_outputCombo.AddString(LS(IDS_GEN_OUT_FLOATING));
		m_outputCombo.AddString(LS(IDS_GEN_OUT_MADVR));
		m_outputCombo.AddString(LS(IDS_GEN_OUT_CAST));
		m_outputCombo.AddString(LS(IDS_GEN_OUT_PGEN));
	}
	m_outputCombo.SetCurSel(ModeToCombo(m_nDisplayMode));

	if (m_blankCheck.GetSafeHwnd()) m_blankCheck.DestroyWindow();
	{
		CPoint bpt = M.at(LBL_X, 0);
		m_blankCheck.Create(LS(IDS_GEN_BLANK_SCREEN), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
			CRect(bpt.x, bpt.y, bpt.x + M.w(CHK_W), bpt.y + M.ht(10)), this, IDC_BLANKING_CHECK);
		m_blankCheck.SetFont(font);
	}
	m_blankCheck.SetCheck(m_doScreenBlanking ? BST_CHECKED : BST_UNCHECKED);
	if (m_pgenReadout.GetSafeHwnd()) m_pgenReadout.DestroyWindow();
	{
		CPoint rpt = M.at(LBL_X, 40);
		m_pgenReadout.Create(WS_CHILD | ES_MULTILINE | ES_READONLY | WS_TABSTOP, CRect(rpt.x, rpt.y, rpt.x + M.w(166), rpt.y + M.ht(80)), this, IDC_PGEN_READOUT);
		m_pgenReadout.SetFont(font);
		{ int pgtab = 80; m_pgenReadout.SendMessage(EM_SETTABSTOPS, 1, (LPARAM)&pgtab); }
		m_pgenReadout.SetWindowText(_T("PGenerator device info will appear here."));
		::SetWindowSubclass(m_pgenReadout.GetSafeHwnd(), PgReadoutProc, 1, 0);
	}
	if (m_pgenSettingsBtn.GetSafeHwnd()) m_pgenSettingsBtn.DestroyWindow();
	{
		CPoint spt = M.at(LBL_X, 130);
		m_pgenSettingsBtn.Create(LS(IDS_GEN_PGEN_SETTINGS), WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, CRect(spt.x, spt.y, spt.x + M.w(106), spt.y + M.ht(14)), this, IDC_PGEN_SETTINGS_BTN);
		m_pgenSettingsBtn.SetFont(font);
	if (m_tenBitCheck.GetSafeHwnd()) m_tenBitCheck.DestroyWindow();
	m_tenBitCheck.Create(LS(IDS_GEN_10BIT_PGEN), WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, CRect(0, 0, M.w(140), M.ht(10)), this, IDC_PGEN_10BIT_CHECK);
	m_tenBitCheck.SetFont(font);
	m_tenBitCheck.SetCheck(m_b10bitPGen ? BST_CHECKED : BST_UNCHECKED);
	if (m_tenBitMadvrCheck.GetSafeHwnd()) m_tenBitMadvrCheck.DestroyWindow();
	m_tenBitMadvrCheck.Create(LS(IDS_GEN_10BIT_PGEN), WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, CRect(0, 0, M.w(140), M.ht(10)), this, IDC_MADVR_10BIT_CHECK);
	m_tenBitMadvrCheck.SetFont(font);
	m_tenBitMadvrCheck.SetCheck(m_b10bitMadvr ? BST_CHECKED : BST_UNCHECKED);
	}
	{
		CPoint rfp = M.at(LBL_X, 130);
		if (!m_glyphFont.GetSafeHandle()) PgMakeGlyphFont(m_glyphFont);
		PgMakeGlyphBtn(m_pgenRefreshBtn, this, m_glyphFont, IDC_PGEN_REFRESH_BTN, L"\xE72C", rfp, M.w(18), M.ht(14));
		if (!m_pageTip.GetSafeHwnd()) { m_pageTip.Create(this); m_pageTip.Activate(TRUE); }
		m_pageTip.AddTool(&m_pgenRefreshBtn, LS(IDS_PGEN_REFRESH));
	}
	m_lblOffset = AddText(this, m_dynAll, font, M, LS(IDS_GEN_OFFSET), LBL_X, 0, 28, 9);

	// Group frames + row labels. Real positions are assigned in Relayout().
	m_grpDisplay = AddGroup(this, m_dynAll, font, M, IDS_GEN_GRP_DISPLAY, GRP_X, 26, GRP_W, 90);
	m_grpMadvr   = AddGroup(this, m_dynAll, font, M, IDS_GEN_GRP_MADVR,   GRP_X, 26, GRP_W, 70);
	m_grpCast    = AddGroup(this, m_dynAll, font, M, IDS_GEN_GRP_CAST,    GRP_X, 26, GRP_W, 28);
	m_grpPgen    = AddGroup(this, m_dynAll, font, M, IDS_GEN_GRP_PGEN,    GRP_X, 26, GRP_W, 56);
	m_grpSignal  = AddGroup(this, m_dynAll, font, M, IDS_GEN_GRP_SIGNAL,  GRP_X, 26, GRP_W, 56);
	m_grpPattern = AddGroup(this, m_dynAll, font, M, IDS_GEN_GRP_PATTERN, GRP_X, 26, GRP_W, 56);
	m_grpBlanking = AddGroup(this, m_dynAll, font, M, IDS_GEN_GRP_BLANKING, GRP_X, 26, GRP_W, 31);

	m_lblScreen    = AddText(this, m_dynAll, font, M, LS(IDS_GEN_TARGET_SCREEN), LBL_X, 0, 62, 9);
	m_lblSize      = AddText(this, m_dynAll, font, M, LS(IDS_GEN_PATTERN_SIZE),  LBL_X, 0, 84, 9);
	m_lblApl       = AddText(this, m_dynAll, font, M, LS(IDS_GEN_APL),           LBL_X, 0, 84, 9);
	m_lblIntensity = AddText(this, m_dynAll, font, M, LS(IDS_GEN_INTENSITY),     LBL_X, 0, 84, 9);
	m_lblXoff      = AddText(this, m_dynAll, font, M, _T("X (px)"),       LBL_X, 0, 84, 9);
	m_lblYoff      = AddText(this, m_dynAll, font, M, _T("Y (px)"),       LBL_X, 0, 84, 9);
	m_lblCastDev   = AddText(this, m_dynAll, font, M, LS(IDS_GEN_CAST_DEVICE),   LBL_X, 0, 28, 9);
	m_lblRange     = AddText(this, m_dynAll, font, M, LS(IDS_GEN_RGB_RANGE),     LBL_X, 0, 42, 9);
}

void CGDIGenePropPage::Relayout()
{
	if (!m_grpDisplay) return;   // not built yet

	int sel = m_outputCombo.GetSafeHwnd() ? m_outputCombo.GetCurSel() : ModeToCombo(m_nDisplayMode);
	if (sel < 0) sel = 0;
	int mode = ComboToMode(sel);

	BOOL isGDI     = (mode == DISPLAY_GDI);
	BOOL isDesktop = (mode == DISPLAY_GDI || mode == DISPLAY_GDI_nBG || mode == DISPLAY_GDI_Hide);
	BOOL isMadvr   = (mode == DISPLAY_madVR);
	BOOL isCast    = (mode == DISPLAY_ccast);
	BOOL isPgen    = (mode == DISPLAY_rPI);
	BOOL hasSignal = (isDesktop || isPgen);

	DlgMap M; M.h = GetSafeHwnd();

	CRect cr; GetClientRect(&cr);
	int grpRightPx = cr.right - M.w(GRP_X);
	if (m_outputCombo.GetSafeHwnd()) { CPoint ocp = M.at(72, 7); m_outputCombo.MoveWindow(ocp.x, ocp.y, grpRightPx - M.w(4) - ocp.x, M.ht(120)); }

	// Hide every managed control; the relevant ones are re-shown below.
	static const UINT fieldIds[] = {
		IDC_MONITOR_COMBO, IDC_PATTERNSIZE_EDIT, IDC_BGSTIM_EDIT, IDC_INTENSITY_EDIT,
		IDC_USEPIC, IDC_RGBLEVEL_RADIO1, IDC_RGBLEVEL_RADIO2, IDC_ENBL_HDR,
		IDC_DISP_TRIP2, IDC_DISP_TRIP, IDC_MADVR_3D, IDC_MADVR_3D2, IDC_MADVR_HDR,
		IDC_MADVR_OSD, IDC_CCAST_COMBO, IDC_XOFFSET_EDIT, IDC_YOFFSET_EDIT, IDC_DISP_TRIP3 };
	ShowIds(this, fieldIds, sizeof(fieldIds) / sizeof(fieldIds[0]), FALSE);
	CWnd* groups[] = { m_grpDisplay, m_grpMadvr, m_grpCast, m_grpPgen, m_grpSignal, m_grpPattern, m_grpBlanking };
	for (int i = 0; i < 7; i++) if (groups[i]) groups[i]->ShowWindow(SW_HIDE);
	if (m_pgenReadout.GetSafeHwnd()) m_pgenReadout.ShowWindow(SW_HIDE);
	if (m_pgenSettingsBtn.GetSafeHwnd()) m_pgenSettingsBtn.ShowWindow(SW_HIDE);
	if (m_tenBitCheck.GetSafeHwnd()) m_tenBitCheck.ShowWindow(SW_HIDE);
	if (m_tenBitMadvrCheck.GetSafeHwnd()) m_tenBitMadvrCheck.ShowWindow(SW_HIDE);
	if (m_pgenRefreshBtn.GetSafeHwnd()) m_pgenRefreshBtn.ShowWindow(SW_HIDE);
	CWnd* labels[] = { m_lblScreen, m_lblSize, m_lblApl, m_lblIntensity, m_lblXoff, m_lblYoff, m_lblCastDev, m_lblRange, m_lblOffset };
	for (int i = 0; i < 9; i++) if (labels[i]) labels[i]->ShowWindow(SW_HIDE);

	int y = 26;

	// Display (desktop output only): which screen, plus the GDI-fullscreen image
	// background. madVR/Cast/PGenerator pick their own surface, so no monitor here.
	if (isDesktop)
	{
		int top = y, cy = top + TOP_INSET;
		MoveWnd(m_lblScreen, M, LBL_X, cy + 2, 60, 9); m_lblScreen->ShowWindow(SW_SHOW);
		{ CPoint mcp = M.at(72, cy); GetDlgItem(IDC_MONITOR_COMBO)->MoveWindow(mcp.x, mcp.y, grpRightPx - M.w(4) - mcp.x, M.ht(90)); GetDlgItem(IDC_MONITOR_COMBO)->ShowWindow(SW_SHOW); }
		cy += ROW_F;
		if (isGDI) { PlaceChk(GetDlgItem(IDC_USEPIC), M, cy); cy += ROW_C; }
		int fb = cy + BOT_PAD;
		PlaceGroup(m_grpDisplay, M, top, fb - top, grpRightPx);
		y = fb + GRP_GAP;
	}
	if (isCast)
	{
		int top = y, cy = top + TOP_INSET;
		MoveWnd(m_lblCastDev, M, LBL_X, cy + 2, 60, 9); m_lblCastDev->ShowWindow(SW_SHOW);
		{ CPoint ccp = M.at(72, cy); GetDlgItem(IDC_CCAST_COMBO)->MoveWindow(ccp.x, ccp.y, grpRightPx - M.w(4) - ccp.x, M.ht(90)); GetDlgItem(IDC_CCAST_COMBO)->ShowWindow(SW_SHOW); }
		cy += ROW_F;
		int fb = cy + BOT_PAD;
		PlaceGroup(m_grpCast, M, top, fb - top, grpRightPx);
		y = fb + GRP_GAP;
		PopulateCast();
	}
	if (isPgen)
	{
		int top = y, cy = top + TOP_INSET;
		int innerLeftPx = M.at(LBL_X, cy).x;
		int innerW = grpRightPx - M.w(7) - innerLeftPx;
		{ CPoint rp = M.at(LBL_X, cy); m_pgenReadout.MoveWindow(rp.x, rp.y, innerW, M.ht(78)); m_pgenReadout.ShowWindow(SW_SHOW); }
		cy += 82;
		{ CPoint bp = M.at(LBL_X, cy); m_pgenSettingsBtn.MoveWindow(bp.x, bp.y, M.w(106), M.ht(14)); m_pgenSettingsBtn.ShowWindow(SW_SHOW); }
			{ CPoint rfp = M.at(LBL_X + 110, cy); m_pgenRefreshBtn.MoveWindow(rfp.x, rfp.y, M.w(18), M.ht(14)); m_pgenRefreshBtn.ShowWindow(SW_SHOW); }
		cy += 18;
		if (m_lblOffset) { CPoint p = M.at(LBL_X, cy + 2); m_lblOffset->MoveWindow(p.x, p.y, M.w(28), M.ht(9)); m_lblOffset->ShowWindow(SW_SHOW); }
		if (m_lblXoff) { CPoint p = M.at(40, cy + 2); m_lblXoff->MoveWindow(p.x, p.y, M.w(24), M.ht(9)); m_lblXoff->ShowWindow(SW_SHOW); }
		{ CWnd* xo = GetDlgItem(IDC_XOFFSET_EDIT); if (xo) { CPoint p = M.at(64, cy); xo->MoveWindow(p.x, p.y, M.w(28), M.ht(12)); xo->ShowWindow(SW_SHOW); } }
		if (m_lblYoff) { CPoint p = M.at(100, cy + 2); m_lblYoff->MoveWindow(p.x, p.y, M.w(24), M.ht(9)); m_lblYoff->ShowWindow(SW_SHOW); }
		{ CWnd* yo = GetDlgItem(IDC_YOFFSET_EDIT); if (yo) { CPoint p = M.at(124, cy); yo->MoveWindow(p.x, p.y, M.w(28), M.ht(12)); yo->ShowWindow(SW_SHOW); } }
		cy += ROW_F;
		PlaceChk(GetDlgItem(IDC_DISP_TRIP3), M, cy); cy += ROW_C;
		PlaceChk(&m_tenBitCheck, M, cy); m_tenBitCheck.ShowWindow(SW_SHOW); cy += ROW_C;
		int fb = cy + BOT_PAD;
		PlaceGroup(m_grpPgen, M, top, fb - top, grpRightPx);
		y = fb + GRP_GAP;
	}
	if (isMadvr)
	{
		int top = y, cy = top + TOP_INSET;
		PlaceChk(GetDlgItem(IDC_MADVR_3D),  M, cy); cy += ROW_C;
		PlaceChk(GetDlgItem(IDC_MADVR_3D2), M, cy); cy += ROW_C;
		PlaceChk(GetDlgItem(IDC_MADVR_OSD), M, cy); cy += ROW_C;
		PlaceChk(GetDlgItem(IDC_MADVR_HDR), M, cy); cy += ROW_C;
		PlaceChk(&m_tenBitMadvrCheck, M, cy); m_tenBitMadvrCheck.ShowWindow(SW_SHOW); cy += ROW_C;
		int fb = cy + BOT_PAD;
		PlaceGroup(m_grpMadvr, M, top, fb - top, grpRightPx);
		y = fb + GRP_GAP;
	}
	// Pattern geometry/level feeds EVERY backend: GDI paints size/APL directly;
	// madVR via madVR_SetPatternConfig; Cast/PGenerator via the APL window math;
	// intensity is applied centrally (DisplayRGBColor) before dispatch.
	{
		int top = y, cy = top + TOP_INSET;
		PlaceLF(m_lblSize,      GetDlgItem(IDC_PATTERNSIZE_EDIT), M, cy); cy += ROW_F;
		PlaceLF(m_lblApl,       GetDlgItem(IDC_BGSTIM_EDIT),      M, cy); cy += ROW_F;
		PlaceLF(m_lblIntensity, GetDlgItem(IDC_INTENSITY_EDIT),   M, cy); cy += ROW_F;
		int fb = cy + BOT_PAD;
		PlaceGroup(m_grpPattern, M, top, fb - top, grpRightPx);
		y = fb + GRP_GAP;
	}
	if (hasSignal)
	{
		int top = y, cy = top + TOP_INSET;
		MoveWnd(m_lblRange, M, LBL_X, cy + 1, 50, 9); m_lblRange->ShowWindow(SW_SHOW);
		MoveWnd(GetDlgItem(IDC_RGBLEVEL_RADIO1), M, 64, cy, 40, 10); GetDlgItem(IDC_RGBLEVEL_RADIO1)->ShowWindow(SW_SHOW);
		MoveWnd(GetDlgItem(IDC_RGBLEVEL_RADIO2), M, 108, cy, 48, 10); GetDlgItem(IDC_RGBLEVEL_RADIO2)->ShowWindow(SW_SHOW);
		cy += ROW_C;
		// HDR10 and the GPU video-LUT bypass act on the local desktop output only.
		if (isDesktop)
		{
			PlaceChk(GetDlgItem(IDC_ENBL_HDR), M, cy); cy += ROW_C;
			PlaceChk(GetDlgItem(IDC_DISP_TRIP2), M, cy); cy += ROW_C;
		}
		PlaceChk(GetDlgItem(IDC_DISP_TRIP), M, cy); cy += ROW_C;
		int fb = cy + BOT_PAD;
		PlaceGroup(m_grpSignal, M, top, fb - top, grpRightPx);
		y = fb + GRP_GAP;
	}

	{
		int top = y, cy = top + TOP_INSET;
		MoveWnd(&m_blankCheck, M, LBL_X, cy, CHK_W, 10); m_blankCheck.ShowWindow(SW_SHOW);
		cy += ROW_C;
		int fb = cy + BOT_PAD;
		PlaceGroup(m_grpBlanking, M, top, fb - top, grpRightPx);
		y = fb + GRP_GAP;
	}

	// --- enable/state rules (preserve the original behavior) ---
	CheckRadioButton(IDC_RGBLEVEL_RADIO1, IDC_RGBLEVEL_RADIO2, IDC_RGBLEVEL_RADIO1 + (m_b16_235 ? 1 : 0));

	BOOL intensityOn = !(GetConfig()->m_GammaOffsetType == 5 || GetConfig()->m_GammaOffsetType == 7);
	CWnd* pInt = GetDlgItem(IDC_INTENSITY_EDIT);
	if (pInt) pInt->EnableWindow(intensityOn);

	CWnd* pTrip = GetDlgItem(IDC_DISP_TRIP);
	if (pTrip)
	{
		BOOL userPat = (isPgen && IsDlgButtonChecked(IDC_DISP_TRIP3));
		BOOL tenBit = (isPgen && m_tenBitCheck.GetSafeHwnd() && m_tenBitCheck.GetCheck() == BST_CHECKED);
		pTrip->EnableWindow(!userPat && !tenBit);
	}
}

void CGDIGenePropPage::PopulateCast()
{
	m_cCastComboCtrl.ResetContent();
	m_GCast.RefreshList();
	unsigned int ccastIp = GetConfig()->GetProfileInt("GDIGenerator", "CCastIp", 0);
	int sel = -1, comboIdx = 0;
	for (int i = 0; i < m_GCast.getCount(); i++)
	{
		if (m_GCast[i]->typ == cctyp_Audio)
			continue;   // audio Cast devices (speakers) can't show patterns
		m_cCastComboCtrl.AddString(m_GCast[i]->name);
		if (ccastIp && sel < 0 && m_GCast.getCcastIpAddress(m_GCast[i]) == ccastIp)
			sel = comboIdx;
		comboIdx++;
	}
	m_castHasDevice = (comboIdx > 0);
	if (m_castHasDevice)
		m_cCastComboCtrl.SetCurSel(sel >= 0 ? sel : 0);
	else
	{
		m_cCastComboCtrl.AddString(LS(IDS_GEN_NO_CAST));
		m_cCastComboCtrl.SetCurSel(0);
	}
	m_cCastComboCtrl.EnableWindow(m_castHasDevice);
}

void CGDIGenePropPage::OnSelchangeOutput()
{
	int s = m_outputCombo.GetCurSel();
	if (s < 0) return;
	m_nDisplayMode = ComboToMode(s);
	if (m_nDisplayMode == DISPLAY_ccast)
		m_b16_235 = FALSE;
	Relayout();
	if (m_nDisplayMode == DISPLAY_rPI)
		QueryPGenerator();
}

void CGDIGenePropPage::OnUserPatternClick()
{
	Relayout();
}

void CGDIGenePropPage::On10bitClick()
{
	// 10-bit patterns force the direct RECTANGLE10bit command, which has no
	// triplet overlay (the daemon template renderer is 8-bit), so grey out
	// Show-RGB-triplets while 10-bit is on.
	Relayout();
}

struct PgenQueryResult { CStringArray vals; CString err; BOOL ok; };

static UINT AFX_CDECL PgenQueryThread(LPVOID p)
{
	CGDIGenePropPage* pg = (CGDIGenePropPage*)p;
	CGDIGenerator* gen = pg->m_pGenerator;
	HWND hwnd = pg->GetSafeHwnd();
	PgenQueryResult* r = new PgenQueryResult;
	r->ok = gen ? gen->QueryPGeneratorInfo(r->vals, r->err) : FALSE;
	if (!(hwnd && IsWindow(hwnd) && ::PostMessage(hwnd, WM_PGEN_QUERY_DONE, 0, (LPARAM)r)))
		delete r;
	return 0;
}

void CGDIGenePropPage::QueryPGenerator()
{
	if (!m_pgenReadout.GetSafeHwnd()) return;
	if (m_pgenQuerying) return;
	m_pgenQuerying = TRUE;
	if (m_pgenSettingsBtn.GetSafeHwnd()) m_pgenSettingsBtn.EnableWindow(FALSE);
	if (GetDlgItem(IDC_XOFFSET_EDIT)) GetDlgItem(IDC_XOFFSET_EDIT)->EnableWindow(FALSE);
	if (GetDlgItem(IDC_YOFFSET_EDIT)) GetDlgItem(IDC_YOFFSET_EDIT)->EnableWindow(FALSE);
	if (GetDlgItem(IDC_DISP_TRIP3)) GetDlgItem(IDC_DISP_TRIP3)->EnableWindow(FALSE);
	CString q; q.LoadString(IDS_PGEN_ST_QUERYING); m_pgenReadout.SetWindowText(q);
	AfxBeginThread(PgenQueryThread, this);
}

LRESULT CGDIGenePropPage::OnPgenQueryDone(WPARAM, LPARAM lp)
{
	PgenQueryResult* r = (PgenQueryResult*)lp;
	m_pgenQuerying = FALSE;
	if (m_pgenSettingsBtn.GetSafeHwnd()) m_pgenSettingsBtn.EnableWindow(r->ok);
	if (GetDlgItem(IDC_XOFFSET_EDIT)) GetDlgItem(IDC_XOFFSET_EDIT)->EnableWindow(r->ok);
	if (GetDlgItem(IDC_YOFFSET_EDIT)) GetDlgItem(IDC_YOFFSET_EDIT)->EnableWindow(r->ok);
	if (GetDlgItem(IDC_DISP_TRIP3)) GetDlgItem(IDC_DISP_TRIP3)->EnableWindow(r->ok);
	if (!r->ok) { ShowPgenDisconnected(); delete r; return 0; }
	static const UINT lblIds[9] = {
		IDS_PGEN_RO_NAME, IDS_PGEN_RO_IP, IDS_PGEN_RO_VERSION, IDS_PGEN_RO_DYNRANGE,
		IDS_PGEN_RO_RESOLUTION, IDS_PGEN_RO_BITDEPTH, IDS_PGEN_RO_COLORSPACE, IDS_PGEN_RO_COLORFORMAT, IDS_PGEN_RO_SIGRANGE };
	CString out;
	for (int i = 0; i < 9 && i < r->vals.GetSize(); i++)
	{
		CString lbl; lbl.LoadString(lblIds[i]); out += lbl;
		out += _T("\t");
		out += r->vals[i];
		if (i < 8) out += _T("\r\n");
	}
	if (m_pgenReadout.GetSafeHwnd()) m_pgenReadout.SetWindowText(out);
	delete r;
	return 0;
}

BEGIN_MESSAGE_MAP(CPGenSettingsDlg, CDialog)
	ON_CBN_SELCHANGE(IDC_PGEN_FORMAT_COMBO, OnFormatChanged)
	ON_CBN_SELCHANGE(IDC_PGEN_DYNRANGE_COMBO, OnDynRangeChanged)
	ON_BN_CLICKED(IDC_PGEN_REBOOT_BTN, OnReboot)
	ON_BN_CLICKED(IDC_PGEN_RESTART_BTN, OnRestartSw)
	ON_BN_CLICKED(IDC_PGEN_SHUTDOWN_BTN, OnShutdown)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

CPGenSettingsDlg::CPGenSettingsDlg(CWnd* pParent) : CDialog(CPGenSettingsDlg::IDD, pParent)
{
	m_pGenerator = NULL;
	m_action = 0;
}

static const TCHAR* const kPgCF[] = { _T("RGB"), _T("YCbCr 4:4:4"), _T("YCbCr 4:2:2") };
static const int kPgCFv[] = { 0, 1, 2 };
static const TCHAR* const kPgQR[] = { _T("Full"), _T("Limited") };
static const int kPgQRv[] = { 2, 1 };
static const TCHAR* const kPgBD[] = { _T("8-bit"), _T("10-bit") };
static const int kPgBDv[] = { 8, 10 };
static const TCHAR* const kPgCM[] = { _T("Default"), _T("BT.709 (YCC)"), _T("BT.2020 (RGB)") };
static const int kPgCMv[] = { 0, 2, 9 };
static const TCHAR* const kPgDR[] = { _T("SDR"), _T("HDR"), _T("Dolby Vision") };
static const TCHAR* const kPgEO[] = { _T("SDR (gamma)"), _T("HDR (gamma)"), _T("ST.2084 / PQ"), _T("HLG") };
static const int kPgEOv[] = { 0, 1, 2, 3 };
static const TCHAR* const kPgPR[] = { _T("Rec.709"), _T("Rec.2020"), _T("P3 / D65"), _T("DCI-P3"), _T("P3 / D60") };
static const int kPgPRv[] = { 0, 1, 2, 3, 4 };
static const TCHAR* const* const kAviItems[5] = { 0, kPgCF, kPgQR, kPgBD, kPgCM };
static const int* const kAviVals[5] = { 0, kPgCFv, kPgQRv, kPgBDv, kPgCMv };
static const int kAviCnt[5] = { 0, 3, 2, 2, 3 };
static const UINT kAviLblId[5] = { IDS_PGEN_RO_RESOLUTION, IDS_PGEN_RO_COLORFORMAT, IDS_PGEN_RO_SIGRANGE, IDS_PGEN_RO_BITDEPTH, IDS_PGEN_RO_COLORSPACE };

BOOL CPGenSettingsDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	SetWindowText(LS(IDS_PGEN_DLG_TITLE));
	DlgMap M; M.h = GetSafeHwnd();
	CFont* font = GetParent() ? GetParent()->GetFont() : GetFont();

	CStringArray modeLabels;
	int curMode = -1;
	PGenSettings st; st.valid = FALSE;
	st.colorFormat = st.quantRange = st.bitDepth = st.colorimetry = 0;
	st.isHdr = st.isLLDovi = st.isStdDovi = st.eotf = st.primaries = 0;
	st.doviMode = 1;
	st.maxLuma = 1000; st.minLuma = 5; st.maxCll = 1000; st.maxFall = 400;
	if (m_pGenerator)
	{
		CWaitCursor wait;
		curMode = m_pGenerator->QueryPGeneratorModes(modeLabels, m_resIds, st);
	}

	const int LX = 8, LW = 82, CX = 94, CW = 98;
	const int FULLW = 200, PITCH = 14, Y0 = 20;

	if (!m_glyphFont.GetSafeHandle()) PgMakeGlyphFont(m_glyphFont);

	{ CPoint p = M.at(LX, 5); m_hdrAvi.Create(LS(IDS_PGEN_HDR_AVI), WS_CHILD | WS_VISIBLE, CRect(p.x, p.y, p.x + M.w(FULLW - LX), p.y + M.ht(9)), this); m_hdrAvi.SetFont(font); }
	PgHeaderRule(this, m_hdrAviLine, m_hdrAvi, font, M.at(FULLW - 8, 0).x, TRUE);

	{
		int y = Y0;
		CPoint lp = M.at(LX, y + 2); m_aviL[5].Create(LS(IDS_PGEN_DYNRANGE), WS_CHILD | WS_VISIBLE, CRect(lp.x, lp.y, lp.x + M.w(LW), lp.y + M.ht(9)), this); m_aviL[5].SetFont(font);
		CPoint cp = M.at(CX, y); m_avi[5].Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, CRect(cp.x, cp.y, cp.x + M.w(CW), cp.y + M.ht(80)), this, IDC_PGEN_AVI_BASE + 5); m_avi[5].SetFont(font);
		for (int j = 0; j < 3; j++) m_avi[5].AddString(kPgDR[j]);
		int sel = (st.isLLDovi || st.isStdDovi) ? 2 : (st.isHdr ? 1 : 0);
		m_avi[5].SetCurSel(sel); m_aviInit[5] = sel;
	}
	{
		int y = Y0 + PITCH;
		CPoint lp = M.at(LX, y + 2); m_aviL[0].Create(LS(IDS_PGEN_RO_RESOLUTION), WS_CHILD | WS_VISIBLE, CRect(lp.x, lp.y, lp.x + M.w(LW), lp.y + M.ht(9)), this); m_aviL[0].SetFont(font);
		CPoint cp = M.at(CX, y); m_avi[0].Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, CRect(cp.x, cp.y, cp.x + M.w(CW), cp.y + M.ht(160)), this, IDC_PGEN_AVI_BASE + 0); m_avi[0].SetFont(font);
		for (int j = 0; j < modeLabels.GetSize(); j++) m_avi[0].AddString(modeLabels[j]);
		int sel = 0; for (int j = 0; j < m_resIds.GetSize(); j++) if (m_resIds[j] == curMode) { sel = j; break; }
		m_avi[0].SetCurSel(sel); m_aviInit[0] = sel;
	}
	int aviVal[5] = { 0, st.colorFormat, st.quantRange, st.bitDepth, st.colorimetry };
	for (int i = 1; i <= 4; i++)
	{
		int y = Y0 + (i + 1) * PITCH;
		CPoint lp = M.at(LX, y + 2); m_aviL[i].Create(LS(kAviLblId[i]), WS_CHILD | WS_VISIBLE, CRect(lp.x, lp.y, lp.x + M.w(LW), lp.y + M.ht(9)), this); m_aviL[i].SetFont(font);
		CPoint cp = M.at(CX, y); m_avi[i].Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, CRect(cp.x, cp.y, cp.x + M.w(CW), cp.y + M.ht(120)), this, IDC_PGEN_AVI_BASE + i); m_avi[i].SetFont(font);
		for (int j = 0; j < kAviCnt[i]; j++) m_avi[i].AddString(kAviItems[i][j]);
		int sel = 0; for (int j = 0; j < kAviCnt[i]; j++) if (kAviVals[i][j] == aviVal[i]) { sel = j; break; }
		m_avi[i].SetCurSel(sel); m_aviInit[i] = sel;
	}
	{
		int y = Y0 + 6 * PITCH;
		CPoint lp = M.at(LX, y + 2); m_doviLbl.Create(LS(IDS_PGEN_DOVI_MODE), WS_CHILD, CRect(lp.x, lp.y, lp.x + M.w(LW), lp.y + M.ht(9)), this); m_doviLbl.SetFont(font);
		CPoint cp = M.at(CX, y); m_doviCombo.Create(WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, CRect(cp.x, cp.y, cp.x + M.w(CW), cp.y + M.ht(80)), this, IDC_PGEN_DOVI_COMBO); m_doviCombo.SetFont(font);
		m_doviCombo.AddString(_T("Verify / Absolute"));
		m_doviCombo.AddString(_T("Calibrate / Relative"));
		int sel = (st.doviMode == 2) ? 1 : 0;
		m_doviCombo.SetCurSel(sel); m_doviInit = sel;
	}
	{ CPoint p = M.at(LX, Y0 + 6 * PITCH + 3); m_hdrDrm.Create(LS(IDS_PGEN_HDR_DRM), WS_CHILD, CRect(p.x, p.y, p.x + M.w(FULLW - LX), p.y + M.ht(9)), this); m_hdrDrm.SetFont(font); }
	PgHeaderRule(this, m_hdrDrmLine, m_hdrDrm, font, M.at(FULLW - 8, 0).x, FALSE);
	{
		int y = Y0 + 7 * PITCH + 2; CPoint lp = M.at(LX, y + 2); m_drmL[0].Create(_T("EOTF"), WS_CHILD, CRect(lp.x, lp.y, lp.x + M.w(LW), lp.y + M.ht(9)), this); m_drmL[0].SetFont(font);
		CPoint cp = M.at(CX, y); m_drm[0].Create(WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, CRect(cp.x, cp.y, cp.x + M.w(CW), cp.y + M.ht(90)), this, IDC_PGEN_DRM_BASE + 0); m_drm[0].SetFont(font);
		for (int j = 0; j < 4; j++) m_drm[0].AddString(kPgEO[j]);
		int sel = 0; for (int j = 0; j < 4; j++) if (kPgEOv[j] == st.eotf) { sel = j; break; }
		m_drm[0].SetCurSel(sel); m_drmInit[0] = sel;
	}
	{
		int y = Y0 + 8 * PITCH + 2; CPoint lp = M.at(LX, y + 2); m_drmL[1].Create(LS(IDS_PGEN_PRIMARIES), WS_CHILD, CRect(lp.x, lp.y, lp.x + M.w(LW), lp.y + M.ht(9)), this); m_drmL[1].SetFont(font);
		CPoint cp = M.at(CX, y); m_drm[1].Create(WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, CRect(cp.x, cp.y, cp.x + M.w(CW), cp.y + M.ht(90)), this, IDC_PGEN_DRM_BASE + 1); m_drm[1].SetFont(font);
		for (int j = 0; j < 5; j++) m_drm[1].AddString(kPgPR[j]);
		int sel = 0; for (int j = 0; j < 5; j++) if (kPgPRv[j] == st.primaries) { sel = j; break; }
		m_drm[1].SetCurSel(sel); m_drmInit[1] = sel;
	}
	{
		static const TCHAR* const edLbl[4] = { _T("Max MDL (nits)"), _T("Min MDL (nits)"), _T("MaxCLL (nits)"), _T("MaxFALL (nits)") };
		CString edTxt[4];
		edTxt[0].Format(_T("%d"), st.maxLuma);
		edTxt[1].Format(_T("%.4f"), st.minLuma / 10000.0);
		edTxt[2].Format(_T("%d"), st.maxCll);
		edTxt[3].Format(_T("%d"), st.maxFall);
		for (int i = 0; i < 4; i++)
		{
			int y = Y0 + (i + 9) * PITCH + 2;
			CPoint lp = M.at(LX, y + 2); m_edL[i].Create(edLbl[i], WS_CHILD, CRect(lp.x, lp.y, lp.x + M.w(LW), lp.y + M.ht(9)), this); m_edL[i].SetFont(font);
			CPoint cp = M.at(CX, y); m_ed[i].Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, CRect(cp.x, cp.y, cp.x + M.w(CW), cp.y + M.ht(12)), this, IDC_PGEN_EDIT_BASE + i); m_ed[i].SetFont(font);
			m_ed[i].SetWindowText(edTxt[i]); m_edInit[i] = edTxt[i];
		}
	}
	if (!m_glyphFontBig.GetSafeHandle()) PgMakeGlyphFont(m_glyphFontBig, 9);
	PgMakeGlyphBtn(m_rebootBtn, this, m_glyphFontBig, IDC_PGEN_REBOOT_BTN, L"\xE777", M.at(LX, Y0), M.w(18), M.ht(14));
	PgMakeGlyphBtn(m_restartBtn, this, m_glyphFont, IDC_PGEN_RESTART_BTN, L"\xE895", M.at(LX, Y0), M.w(18), M.ht(14));
	PgMakeGlyphBtn(m_shutdownBtn, this, m_glyphFont, IDC_PGEN_SHUTDOWN_BTN, L"\xE7E8", M.at(LX, Y0), M.w(18), M.ht(14));
	if (GetDlgItem(IDOK)) { GetDlgItem(IDOK)->SetFont(font); GetDlgItem(IDOK)->SetWindowText(LS(IDS_PGEN_APPLY)); }
	if (GetDlgItem(IDCANCEL)) { GetDlgItem(IDCANCEL)->SetFont(font); GetDlgItem(IDCANCEL)->SetWindowText(LS(IDS_PGEN_CLOSE)); }
	m_tip.Create(this);
	m_tip.AddTool(&m_rebootBtn, LS(IDS_PGEN_REBOOT));
	m_tip.AddTool(&m_restartBtn, LS(IDS_PGEN_RESTART_SW));
	m_tip.AddTool(&m_shutdownBtn, LS(IDS_PGEN_SHUTDOWN));
	m_tip.Activate(TRUE);

	UpdateDynRangeState();
	return TRUE;
}

void CPGenSettingsDlg::OnOK()
{
	CStringArray cmds;
	int drSel = (m_avi[5].GetSafeHwnd()) ? m_avi[5].GetCurSel() : -1;
	{ int sel = m_avi[0].GetCurSel(); if (sel >= 0 && sel != m_aviInit[0] && sel < m_resIds.GetSize()) { CString c; c.Format(_T("CMD:SET_MODE:%d"), m_resIds[sel]); cmds.Add(c); } }
	{
		static const TCHAR* const nm[5] = { 0, _T("SET_PGENERATOR_CONF_COLOR_FORMAT"), _T("SET_PGENERATOR_CONF_RGB_QUANT_RANGE"), _T("SET_PGENERATOR_CONF_MAX_BPC"), _T("SET_PGENERATOR_CONF_COLORIMETRY") };
		for (int i = 1; i <= 4; i++) { int sel = m_avi[i].GetCurSel(); if (sel < 0 || sel == m_aviInit[i]) continue; CString c; c.Format(_T("CMD:%s:%d"), nm[i], kAviVals[i][sel]); cmds.Add(c); }
	}
	{
		int sel = m_avi[5].GetCurSel();
		if (sel >= 0 && sel != m_aviInit[5])
		{
			int isSdr = (sel == 0) ? 1 : 0, isHdr = (sel >= 1) ? 1 : 0, dovi = (sel == 2) ? 1 : 0;
			CString c;
			c.Format(_T("CMD:SET_PGENERATOR_CONF_IS_SDR:%d"), isSdr); cmds.Add(c);
			c.Format(_T("CMD:SET_PGENERATOR_CONF_IS_HDR:%d"), isHdr); cmds.Add(c);
			c.Format(_T("CMD:SET_PGENERATOR_CONF_IS_LL_DOVI:%d"), 0); cmds.Add(c);
			c.Format(_T("CMD:SET_PGENERATOR_CONF_IS_STD_DOVI:%d"), dovi); cmds.Add(c);
			c.Format(_T("CMD:SET_PGENERATOR_CONF_DV_STATUS:%d"), dovi); cmds.Add(c);
			c.Format(_T("CMD:SET_PGENERATOR_CONF_DV_INTERFACE:%d"), 0); cmds.Add(c);
		}
	}
	{ int sel = m_doviCombo.GetCurSel(); if (sel >= 0 && sel != m_doviInit && drSel == 2) { CString c; c.Format(_T("CMD:SET_PGENERATOR_CONF_DV_MAP_MODE:%d"), (sel == 1) ? 2 : 1); cmds.Add(c); } }
	{ int sel = m_drm[0].GetCurSel(); if (sel >= 0 && sel != m_drmInit[0] && drSel == 1) { CString c; c.Format(_T("CMD:SET_PGENERATOR_CONF_EOTF:%d"), kPgEOv[sel]); cmds.Add(c); } }
	{ int sel = m_drm[1].GetCurSel(); if (sel >= 0 && sel != m_drmInit[1] && drSel == 1) { CString c; c.Format(_T("CMD:SET_PGENERATOR_CONF_PRIMARIES:%d"), kPgPRv[sel]); cmds.Add(c); } }
	{
		static const TCHAR* const nm[4] = { _T("SET_PGENERATOR_CONF_MAX_LUMA"), _T("SET_PGENERATOR_CONF_MIN_LUMA"), _T("SET_PGENERATOR_CONF_MAX_CLL"), _T("SET_PGENERATOR_CONF_MAX_FALL") };
		if (drSel == 1) for (int i = 0; i < 4; i++)
		{
			CString t; m_ed[i].GetWindowText(t); t.TrimLeft(); t.TrimRight();
			if (t == m_edInit[i]) continue;
			int v = (i == 1) ? (int)(atof((LPCTSTR)t) * 10000.0 + 0.5) : atoi((LPCTSTR)t);
			CString c; c.Format(_T("CMD:%s:%d"), nm[i], v); cmds.Add(c);
		}
	}
	if (cmds.GetSize() > 0 && m_pGenerator) { CWaitCursor wait; m_pGenerator->ApplyPGeneratorConf(cmds); }
	CDialog::OnOK();
}

void CPGenSettingsDlg::OnFormatChanged()
{
	UpdateRangeState();
}

void CPGenSettingsDlg::UpdateRangeState()
{
	if (!m_avi[1].GetSafeHwnd() || !m_avi[2].GetSafeHwnd()) return;
	BOOL isRgb = (m_avi[1].GetCurSel() == 0);
	if (!isRgb)
	{
		int lim = m_avi[2].FindStringExact(-1, _T("Limited"));
		if (lim >= 0) m_avi[2].SetCurSel(lim);
	}
	m_avi[2].EnableWindow(isRgb);
}

void CPGenSettingsDlg::OnDynRangeChanged()
{
	UpdateDynRangeState();
}

void CPGenSettingsDlg::UpdateDynRangeState()
{
	if (!m_avi[5].GetSafeHwnd()) return;
	int dr = m_avi[5].GetCurSel();
	BOOL isDovi = (dr == 2);
	BOOL isHdr = (dr == 1);
	if (m_avi[0].GetSafeHwnd()) m_avi[0].EnableWindow(TRUE);
	for (int i = 1; i <= 4; i++) if (m_avi[i].GetSafeHwnd()) m_avi[i].EnableWindow(!isDovi);
	int sw = isDovi ? SW_SHOW : SW_HIDE;
	if (m_doviLbl.GetSafeHwnd()) m_doviLbl.ShowWindow(sw);
	if (m_doviCombo.GetSafeHwnd()) m_doviCombo.ShowWindow(sw);
	int dsw = isHdr ? SW_SHOW : SW_HIDE;
	if (m_hdrDrm.GetSafeHwnd()) m_hdrDrm.ShowWindow(dsw);
	if (m_hdrDrmLine.GetSafeHwnd()) m_hdrDrmLine.ShowWindow(dsw);
	for (int i = 0; i < 2; i++) { if (m_drm[i].GetSafeHwnd()) m_drm[i].ShowWindow(dsw); if (m_drmL[i].GetSafeHwnd()) m_drmL[i].ShowWindow(dsw); }
	for (int i = 0; i < 4; i++) { if (m_ed[i].GetSafeHwnd()) m_ed[i].ShowWindow(dsw); if (m_edL[i].GetSafeHwnd()) m_edL[i].ShowWindow(dsw); }
	if (!isDovi) UpdateRangeState();

	DlgMap M; M.h = GetSafeHwnd();
	const int Y0r = 20, PITCHr = 14;
	int cbot;
	if (isHdr) cbot = Y0r + 13 * PITCHr + 2;
	else if (isDovi) cbot = Y0r + 7 * PITCHr;
	else cbot = Y0r + 6 * PITCHr;
	int by = cbot + 4;
	if (m_rebootBtn.GetSafeHwnd()) { CPoint p = M.at(8, by); m_rebootBtn.MoveWindow(p.x, p.y, M.w(18), M.ht(14)); }
	if (m_restartBtn.GetSafeHwnd()) { CPoint p = M.at(28, by); m_restartBtn.MoveWindow(p.x, p.y, M.w(18), M.ht(14)); }
	if (m_shutdownBtn.GetSafeHwnd()) { CPoint p = M.at(48, by); m_shutdownBtn.MoveWindow(p.x, p.y, M.w(18), M.ht(14)); }
	if (GetDlgItem(IDOK)) { CPoint p = M.at(96, by); GetDlgItem(IDOK)->MoveWindow(p.x, p.y, M.w(48), M.ht(14)); }
	if (GetDlgItem(IDCANCEL)) { CPoint p = M.at(148, by); GetDlgItem(IDCANCEL)->MoveWindow(p.x, p.y, M.w(48), M.ht(14)); }
	CRect wr, cr; GetWindowRect(wr); GetClientRect(cr);
	int newW = M.w(200) + (wr.Width() - cr.Width());
	int newH = M.at(0, by + 20).y + (wr.Height() - cr.Height());
	SetWindowPos(NULL, 0, 0, newW, newH, SWP_NOMOVE | SWP_NOZORDER);
	CenterWindow();
}

void CPGenSettingsDlg::OnReboot()
{
	if (AfxMessageBox(LS(IDS_PGEN_REBOOT_CONFIRM), MB_YESNO | MB_ICONQUESTION) != IDYES) return;
	if (m_pGenerator) { CWaitCursor wait; m_pGenerator->SendPGeneratorCommand("CMD:REBOOT"); }
	m_action = 1;
	CDialog::OnCancel();
}

void CPGenSettingsDlg::OnRestartSw()
{
	if (AfxMessageBox(LS(IDS_PGEN_RESTART_CONFIRM), MB_YESNO | MB_ICONQUESTION) != IDYES) return;
	if (m_pGenerator) { CWaitCursor wait; m_pGenerator->SendPGeneratorCommand("RESTARTPGENERATOR:"); }
	m_action = 1;
	CDialog::OnCancel();
}

void CPGenSettingsDlg::OnShutdown()
{
	if (AfxMessageBox(LS(IDS_PGEN_SHUTDOWN_CONFIRM), MB_YESNO | MB_ICONQUESTION) != IDYES) return;
	if (m_pGenerator) { CWaitCursor wait; m_pGenerator->SendPGeneratorCommand("CMD:HALT"); }
	m_action = 2;
	CDialog::OnCancel();
}

BOOL CPGenSettingsDlg::PreTranslateMessage(MSG* pMsg)
{
	if (m_tip.GetSafeHwnd()) m_tip.RelayEvent(pMsg);
	return CDialog::PreTranslateMessage(pMsg);
}

BOOL CGDIGenePropPage::PreTranslateMessage(MSG* pMsg)
{
	if (m_pageTip.GetSafeHwnd()) m_pageTip.RelayEvent(pMsg);
	return CPropertyPageWithHelp::PreTranslateMessage(pMsg);
}

HBRUSH CGDIGenePropPage::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CPropertyPageWithHelp::OnCtlColor(pDC, pWnd, nCtlColor);
	if (pWnd && pWnd->GetDlgCtrlID() == IDC_PGEN_READOUT)
	{
		if (!m_roBrush.GetSafeHandle()) m_roBrush.CreateSolidBrush(RGB(238, 244, 251));
		pDC->SetBkColor(RGB(238, 244, 251));
		return (HBRUSH)m_roBrush.GetSafeHandle();
	}
	return hbr;
}

void CPGenSettingsDlg::OnDestroy()
{
	if (m_rebootBtn.GetSafeHwnd()) m_rebootBtn.Detach();
	if (m_restartBtn.GetSafeHwnd()) m_restartBtn.Detach();
	if (m_shutdownBtn.GetSafeHwnd()) m_shutdownBtn.Detach();
	CDialog::OnDestroy();
}

void CGDIGenePropPage::OnDestroy()
{
	if (m_pgenRefreshBtn.GetSafeHwnd()) m_pgenRefreshBtn.Detach();
	CPropertyPageWithHelp::OnDestroy();
}

void CGDIGenePropPage::OnPgenRefresh()
{
	CGenerator::InvalidatePGenCache();
	QueryPGenerator();
}

void CGDIGenePropPage::ShowPgenDisconnected()
{
	if (m_pgenSettingsBtn.GetSafeHwnd()) m_pgenSettingsBtn.EnableWindow(FALSE);
	if (GetDlgItem(IDC_XOFFSET_EDIT)) GetDlgItem(IDC_XOFFSET_EDIT)->EnableWindow(FALSE);
	if (GetDlgItem(IDC_YOFFSET_EDIT)) GetDlgItem(IDC_YOFFSET_EDIT)->EnableWindow(FALSE);
	if (GetDlgItem(IDC_DISP_TRIP3)) GetDlgItem(IDC_DISP_TRIP3)->EnableWindow(FALSE);
	if (m_pgenReadout.GetSafeHwnd()) m_pgenReadout.SetWindowText(LS(IDS_PGEN_ST_NOTCONN));
}

void CGDIGenePropPage::OnPgenSettings()
{
	CPGenSettingsDlg dlg(this);
	dlg.m_pGenerator = m_pGenerator;
	dlg.DoModal();
	if (dlg.m_action != 0)
		ShowPgenDisconnected();
}

void CGDIGenePropPage::OnOK()
{
	m_activeMonitorNum = m_monitorComboCtrl.GetCurSel();
	m_selectedGcastNum = m_cCastComboCtrl.GetCurSel();

	m_doScreenBlanking = (m_blankCheck.GetSafeHwnd() && m_blankCheck.GetCheck() == BST_CHECKED) ? TRUE : FALSE;
	m_b10bitPGen = (m_tenBitCheck.GetSafeHwnd() && m_tenBitCheck.GetCheck() == BST_CHECKED) ? TRUE : FALSE;
	m_b10bitMadvr = (m_tenBitMadvrCheck.GetSafeHwnd() && m_tenBitMadvrCheck.GetCheck() == BST_CHECKED) ? TRUE : FALSE;

	int sel = m_outputCombo.GetSafeHwnd() ? m_outputCombo.GetCurSel() : ModeToCombo(m_nDisplayMode);
	if (sel < 0) sel = 0;
	m_nDisplayMode = ComboToMode(sel);

	if (m_nDisplayMode == DISPLAY_madVR || m_nDisplayMode == DISPLAY_ccast)
		m_b16_235 = FALSE;
	else
		m_b16_235 = IsDlgButtonChecked(IDC_RGBLEVEL_RADIO2) ? TRUE : FALSE;

	if (m_nDisplayMode == DISPLAY_GDI || m_nDisplayMode == DISPLAY_GDI_Hide || m_nDisplayMode == DISPLAY_GDI_nBG)
		m_bHdr10 = IsDlgButtonChecked(IDC_ENBL_HDR) ? TRUE : FALSE;
	else
		m_bHdr10 = FALSE;

	if (m_nDisplayMode != DISPLAY_GDI)
		m_busePic = FALSE;

	GetConfig()->WriteProfileInt("GDIGenerator","DisplayMode",m_nDisplayMode);
	GetConfig()->WriteProfileInt("GDIGenerator","RGB_16_235",m_b16_235);
	GetConfig()->WriteProfileInt("GDIGenerator","EnableHDR10",m_bHdr10);
	if (m_nDisplayMode == DISPLAY_ccast && m_castHasDevice)
	{
		CString name; m_cCastComboCtrl.GetWindowText(name);
		GetConfig()->WriteProfileInt("GDIGenerator","CCastIp",m_GCast.getCcastIpAddress(m_GCast[(LPCTSTR)name]));
	}

	if (GetConfig()->m_GammaOffsetType == 5)
		m_Intensity = 100;

	GetConfig()->m_isModified = true;
	GetConfig()->ApplySettings(FALSE);
	GetConfig()->m_isModified = false;
	CPropertyPageWithHelp::OnOK();
}

BOOL CGDIGenePropPage::OnSetActive()
{
	// Refresh the monitor list so hot-plugged displays appear.
	if (m_pGenerator)
		m_pGenerator->GetMonitorList();

	m_monitorComboCtrl.ResetContent();
	for(int i=0;i<m_monitorNameArray.GetSize();i++)
		m_monitorComboCtrl.AddString(m_monitorNameArray[i]);

	if( m_activeMonitorNum < m_monitorComboCtrl.GetCount() )
		m_monitorComboCtrl.SetCurSel(m_activeMonitorNum);
	else
		m_monitorComboCtrl.SetCurSel(0);

	if (m_outputCombo.GetSafeHwnd())
		m_outputCombo.SetCurSel(ModeToCombo(m_nDisplayMode));

	Relayout();
	if (m_outputCombo.GetSafeHwnd() && ComboToMode(m_outputCombo.GetCurSel()) == DISPLAY_rPI)
		QueryPGenerator();
	return CPropertyPageWithHelp::OnSetActive();
}

BOOL CGDIGenePropPage::OnKillActive()
{
	m_activeMonitorNum = m_monitorComboCtrl.GetCurSel();

	if (m_outputCombo.GetSafeHwnd())
	{
		int s = m_outputCombo.GetCurSel();
		if (s >= 0) m_nDisplayMode = ComboToMode(s);
	}

	m_b16_235  = IsDlgButtonChecked(IDC_RGBLEVEL_RADIO2) ? TRUE : FALSE;
	m_bHdr10   = IsDlgButtonChecked(IDC_ENBL_HDR) ? TRUE : FALSE;
	m_madVR_HDR = IsDlgButtonChecked(IDC_MADVR_HDR) ? TRUE : FALSE;
	m_doScreenBlanking = (m_blankCheck.GetSafeHwnd() && m_blankCheck.GetCheck() == BST_CHECKED) ? TRUE : FALSE;
	m_b10bitPGen = (m_tenBitCheck.GetSafeHwnd() && m_tenBitCheck.GetCheck() == BST_CHECKED) ? TRUE : FALSE;
	m_b10bitMadvr = (m_tenBitMadvrCheck.GetSafeHwnd() && m_tenBitMadvrCheck.GetCheck() == BST_CHECKED) ? TRUE : FALSE;

	return CPropertyPageWithHelp::OnKillActive();
}

void CGDIGenePropPage::OnTestOverlay()
{
	CFullScreenWindow	OverlayWnd ( TRUE );

	m_activeMonitorNum=m_monitorComboCtrl.GetCurSel();

	OverlayWnd.MoveToMonitor(m_monitorHandle[m_activeMonitorNum]);

	if ( OverlayWnd.m_nDisplayMode == DISPLAY_OVERLAY )
		OverlayWnd.DisplayRGBColor ( ColorRGBDisplay(0.5), TRUE );

	if ( OverlayWnd.m_nDisplayMode == DISPLAY_OVERLAY )
		MessageBox ( "Overlay window created (small gray rectangle on top-right). You can use advanced display properties to change settings.\r\nClick OK to close overlay window.", "Overlay", MB_OK | MB_ICONINFORMATION );
	else
		MessageBox ( "An error occured during Overlay creation.", "Overlay", MB_OK | MB_ICONHAND );
}

void CGDIGenePropPage::OnDropdownMonitorCombo()
{
	// Re-enumerate monitors each time the list opens so a just-plugged
	// display shows up without reopening the dialog or restarting HCFR.
	CString cur;
	int sel = m_monitorComboCtrl.GetCurSel();
	if (sel >= 0)
		m_monitorComboCtrl.GetLBText(sel, cur);

	if (m_pGenerator)
		m_pGenerator->GetMonitorList();

	m_monitorComboCtrl.ResetContent();
	for (int i = 0; i < m_monitorNameArray.GetSize(); i++)
		m_monitorComboCtrl.AddString(m_monitorNameArray[i]);

	// Keep the previous selection if it still exists.
	int idx = cur.IsEmpty() ? -1 : m_monitorComboCtrl.FindStringExact(-1, cur);
	if (idx >= 0)
		m_monitorComboCtrl.SetCurSel(idx);
	else if (m_monitorComboCtrl.GetCount() > 0)
		m_monitorComboCtrl.SetCurSel(0);
}

UINT CGDIGenePropPage::GetHelpId ( LPSTR lpszTopic )
{
	return HID_GENERATOR_GDI;
}
