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


BEGIN_MESSAGE_MAP(CGDIGenePropPage, CPropertyPageWithHelp)
	//{{AFX_MSG_MAP(CGDIGenePropPage)
	ON_BN_CLICKED(IDC_OVERLAY, OnTestOverlay)
	//}}AFX_MSG_MAP
	ON_CBN_DROPDOWN(IDC_MONITOR_COMBO, OnDropdownMonitorCombo)
	ON_CBN_SELCHANGE(IDC_GEN_OUTPUT_COMBO, OnSelchangeOutput)
	ON_BN_CLICKED(IDC_DISP_TRIP3, OnUserPatternClick)
	ON_BN_CLICKED(IDC_PGEN_SETTINGS_BTN, OnPgenSettings)
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
		m_pgenReadout.Create(WS_CHILD | ES_MULTILINE | ES_READONLY | WS_TABSTOP | WS_BORDER, CRect(rpt.x, rpt.y, rpt.x + M.w(166), rpt.y + M.ht(80)), this, IDC_PGEN_READOUT);
		m_pgenReadout.SetFont(font);
		{ int pgtab = 80; m_pgenReadout.SendMessage(EM_SETTABSTOPS, 1, (LPARAM)&pgtab); }
		m_pgenReadout.SetWindowText(_T("PGenerator device info will appear here."));
	}
	if (m_pgenSettingsBtn.GetSafeHwnd()) m_pgenSettingsBtn.DestroyWindow();
	{
		CPoint spt = M.at(LBL_X, 130);
		m_pgenSettingsBtn.Create(LS(IDS_GEN_PGEN_SETTINGS), WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, CRect(spt.x, spt.y, spt.x + M.w(92), spt.y + M.ht(14)), this, IDC_PGEN_SETTINGS_BTN);
		m_pgenSettingsBtn.SetFont(font);
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
		int innerW = grpRightPx - M.w(3) - innerLeftPx;
		{ CPoint rp = M.at(LBL_X, cy); m_pgenReadout.MoveWindow(rp.x, rp.y, innerW, M.ht(78)); m_pgenReadout.ShowWindow(SW_SHOW); }
		cy += 82;
		{ CPoint bp = M.at(LBL_X, cy); m_pgenSettingsBtn.MoveWindow(bp.x, bp.y, M.w(92), M.ht(14)); m_pgenSettingsBtn.ShowWindow(SW_SHOW); }
		cy += 18;
		if (m_lblOffset) { CPoint p = M.at(LBL_X, cy + 2); m_lblOffset->MoveWindow(p.x, p.y, M.w(28), M.ht(9)); m_lblOffset->ShowWindow(SW_SHOW); }
		if (m_lblXoff) { CPoint p = M.at(40, cy + 2); m_lblXoff->MoveWindow(p.x, p.y, M.w(24), M.ht(9)); m_lblXoff->ShowWindow(SW_SHOW); }
		{ CWnd* xo = GetDlgItem(IDC_XOFFSET_EDIT); if (xo) { CPoint p = M.at(64, cy); xo->MoveWindow(p.x, p.y, M.w(28), M.ht(12)); xo->ShowWindow(SW_SHOW); } }
		if (m_lblYoff) { CPoint p = M.at(100, cy + 2); m_lblYoff->MoveWindow(p.x, p.y, M.w(24), M.ht(9)); m_lblYoff->ShowWindow(SW_SHOW); }
		{ CWnd* yo = GetDlgItem(IDC_YOFFSET_EDIT); if (yo) { CPoint p = M.at(124, cy); yo->MoveWindow(p.x, p.y, M.w(28), M.ht(12)); yo->ShowWindow(SW_SHOW); } }
		cy += ROW_F;
		PlaceChk(GetDlgItem(IDC_DISP_TRIP3), M, cy); cy += ROW_C;
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
		pTrip->EnableWindow(!userPat);
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

void CGDIGenePropPage::QueryPGenerator()
{
	if (!m_pgenReadout.GetSafeHwnd()) return;
	CWaitCursor wait;
	CString q; q.LoadString(IDS_PGEN_ST_QUERYING); m_pgenReadout.SetWindowText(q);
	m_pgenReadout.UpdateWindow();
	CStringArray vals;
	CString err;
	BOOL ok = m_pGenerator ? m_pGenerator->QueryPGeneratorInfo(vals, err) : FALSE;
	if (m_pgenSettingsBtn.GetSafeHwnd()) m_pgenSettingsBtn.EnableWindow(ok);
	if (GetDlgItem(IDC_XOFFSET_EDIT)) GetDlgItem(IDC_XOFFSET_EDIT)->EnableWindow(ok);
	if (GetDlgItem(IDC_YOFFSET_EDIT)) GetDlgItem(IDC_YOFFSET_EDIT)->EnableWindow(ok);
	if (GetDlgItem(IDC_DISP_TRIP3)) GetDlgItem(IDC_DISP_TRIP3)->EnableWindow(ok);
	if (!ok)
	{
		CString ng; ng.LoadString(IDS_PGEN_ST_NOGEN); m_pgenReadout.SetWindowText(err.IsEmpty() ? ng : err);
		return;
	}
	static const UINT lblIds[9] = {
		IDS_PGEN_RO_NAME, IDS_PGEN_RO_IP, IDS_PGEN_RO_VERSION, IDS_PGEN_RO_DYNRANGE,
		IDS_PGEN_RO_RESOLUTION, IDS_PGEN_RO_BITDEPTH, IDS_PGEN_RO_COLORSPACE, IDS_PGEN_RO_COLORFORMAT, IDS_PGEN_RO_SIGRANGE };
	CString out;
	for (int i = 0; i < 9 && i < vals.GetSize(); i++)
	{
		CString lbl; lbl.LoadString(lblIds[i]); out += lbl;
		out += _T("\t");
		out += vals[i];
		if (i < 8) out += _T("\r\n");
	}
	m_pgenReadout.SetWindowText(out);
}


BEGIN_MESSAGE_MAP(CPGenSettingsDlg, CDialog)
	ON_CBN_SELCHANGE(IDC_PGEN_RANGE_COMBO + 1, OnFormatChanged)
	ON_BN_CLICKED(IDC_PGEN_REBOOT_BTN, OnReboot)
	ON_BN_CLICKED(IDC_PGEN_RESTART_BTN, OnRestartSw)
END_MESSAGE_MAP()

CPGenSettingsDlg::CPGenSettingsDlg(CWnd* pParent) : CDialog(CPGenSettingsDlg::IDD, pParent)
{
	m_pGenerator = NULL;
}

static const TCHAR* kPgSetCmd[4] = {
	_T("SET_PGENERATOR_CONF_RGB_QUANT_RANGE"),
	_T("SET_PGENERATOR_CONF_COLOR_FORMAT"),
	_T("SET_PGENERATOR_CONF_MAX_BPC"),
	_T("SET_PGENERATOR_CONF_COLORIMETRY") };
static const UINT kPgLblId[4] = { IDS_PGEN_RO_SIGRANGE, IDS_PGEN_RO_COLORFORMAT, IDS_PGEN_RO_BITDEPTH, IDS_PGEN_RO_COLORSPACE };
static const int kPgValIdx[4] = { 8, 7, 5, 6 };
static const TCHAR* kPgItems0[] = { _T("Full"), _T("Limited") };
static const int    kPgVals0[]  = { 2, 1 };
static const TCHAR* kPgItems1[] = { _T("RGB"), _T("YCbCr 4:4:4"), _T("YCbCr 4:2:2") };
static const int    kPgVals1[]  = { 0, 1, 2 };
static const TCHAR* kPgItems2[] = { _T("8-bit"), _T("10-bit") };
static const int    kPgVals2[]  = { 8, 10 };
static const TCHAR* kPgItems3[] = { _T("Default"), _T("BT.709 (YCC)"), _T("BT.2020 (RGB)") };
static const int    kPgVals3[]  = { 0, 2, 9 };
static const TCHAR** kPgItems[4] = { kPgItems0, kPgItems1, kPgItems2, kPgItems3 };
static const int*    kPgVals[4]  = { kPgVals0, kPgVals1, kPgVals2, kPgVals3 };
static const int     kPgCount[4] = { 2, 3, 2, 3 };

BOOL CPGenSettingsDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	SetWindowText(LS(IDS_PGEN_DLG_TITLE));
	DlgMap M; M.h = GetSafeHwnd();
	CFont* font = GetFont();

	CStringArray vals;
	CString err;
	BOOL haveData = FALSE;
	if (m_pGenerator) { CWaitCursor wait; haveData = m_pGenerator->QueryPGeneratorInfo(vals, err); }

	{
		CPoint lp = M.at(12, 18);
		m_resLabel.Create(LS(IDS_PGEN_RO_RESOLUTION), WS_CHILD | WS_VISIBLE, CRect(lp.x, lp.y, lp.x + M.w(85), lp.y + M.ht(9)), this);
		m_resLabel.SetFont(font);
		CPoint cp = M.at(100, 16);
		m_resCombo.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, CRect(cp.x, cp.y, cp.x + M.w(100), cp.y + M.ht(160)), this, IDC_PGEN_RANGE_COMBO + 10);
		m_resCombo.SetFont(font);
		int curId = -1;
		if (m_pGenerator) { CWaitCursor wait; CStringArray ml; curId = m_pGenerator->QueryPGeneratorModes(ml, m_resIds); for (int j = 0; j < ml.GetSize(); j++) m_resCombo.AddString(ml[j]); }
		m_resInit = 0;
		for (int j = 0; j < m_resIds.GetSize(); j++) if (m_resIds[j] == curId) { m_resInit = j; break; }
		m_resCombo.SetCurSel(m_resInit);
	}

	for (int i = 0; i < 4; i++)
	{
		int y = 16 + (i + 1) * 20;
		CPoint lp = M.at(12, y + 2);
		m_label[i].Create(LS(kPgLblId[i]), WS_CHILD | WS_VISIBLE, CRect(lp.x, lp.y, lp.x + M.w(85), lp.y + M.ht(9)), this);
		m_label[i].SetFont(font);
		CPoint cp = M.at(100, y);
		m_combo[i].Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, CRect(cp.x, cp.y, cp.x + M.w(100), cp.y + M.ht(120)), this, IDC_PGEN_RANGE_COMBO + i);
		m_combo[i].SetFont(font);
		for (int j = 0; j < kPgCount[i]; j++) m_combo[i].AddString(kPgItems[i][j]);
		int sel = 0;
		if (haveData && kPgValIdx[i] < vals.GetSize())
		{
			int fx = m_combo[i].FindStringExact(-1, vals[kPgValIdx[i]]);
			if (fx >= 0) sel = fx;
		}
		m_combo[i].SetCurSel(sel);
		m_initSel[i] = sel;
	}

	if (GetDlgItem(IDOK)) GetDlgItem(IDOK)->SetWindowText(LS(IDS_PGEN_APPLY));
	if (GetDlgItem(IDCANCEL)) GetDlgItem(IDCANCEL)->SetWindowText(LS(IDS_PGEN_CLOSE));
	{
		CPoint rb = M.at(6, 112);
		m_rebootBtn.Create(LS(IDS_PGEN_REBOOT), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, CRect(rb.x, rb.y, rb.x + M.w(44), rb.y + M.ht(14)), this, IDC_PGEN_REBOOT_BTN);
		m_rebootBtn.SetFont(font);
		CPoint rs = M.at(54, 112);
		m_restartBtn.Create(LS(IDS_PGEN_RESTART_SW), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, CRect(rs.x, rs.y, rs.x + M.w(76), rs.y + M.ht(14)), this, IDC_PGEN_RESTART_BTN);
		m_restartBtn.SetFont(font);
	}
	UpdateRangeState();
	return TRUE;
}

void CPGenSettingsDlg::OnOK()
{
	CStringArray cmds;
	int rsel = m_resCombo.GetCurSel();
	if (rsel >= 0 && rsel != m_resInit && rsel < m_resIds.GetSize())
	{
		CString c;
		c.Format(_T("CMD:SET_MODE:%d"), m_resIds[rsel]);
		cmds.Add(c);
	}
	for (int i = 0; i < 4; i++)
	{
		int sel = m_combo[i].GetCurSel();
		if (sel < 0 || sel == m_initSel[i]) continue;
		CString c;
		c.Format(_T("CMD:%s:%d"), kPgSetCmd[i], kPgVals[i][sel]);
		cmds.Add(c);
	}
	if (cmds.GetSize() > 0 && m_pGenerator)
	{
		CWaitCursor wait;
		m_pGenerator->ApplyPGeneratorConf(cmds);
	}
	CDialog::OnOK();
}

void CPGenSettingsDlg::OnFormatChanged()
{
	UpdateRangeState();
}

void CPGenSettingsDlg::UpdateRangeState()
{
	if (!m_combo[1].GetSafeHwnd() || !m_combo[0].GetSafeHwnd()) return;
	BOOL isRgb = (m_combo[1].GetCurSel() == 0);
	if (!isRgb)
	{
		int lim = m_combo[0].FindStringExact(-1, _T("Limited"));
		if (lim >= 0) m_combo[0].SetCurSel(lim);
	}
	m_combo[0].EnableWindow(isRgb);
}

void CPGenSettingsDlg::OnReboot()
{
	if (AfxMessageBox(LS(IDS_PGEN_REBOOT_CONFIRM), MB_YESNO | MB_ICONQUESTION) != IDYES) return;
	if (m_pGenerator) { CWaitCursor wait; m_pGenerator->SendPGeneratorCommand("CMD:REBOOT"); }
}

void CPGenSettingsDlg::OnRestartSw()
{
	if (m_pGenerator) { CWaitCursor wait; m_pGenerator->SendPGeneratorCommand("RESTARTPGENERATOR:"); }
}

void CGDIGenePropPage::OnPgenSettings()
{
	CPGenSettingsDlg dlg(this);
	dlg.m_pGenerator = m_pGenerator;
	dlg.DoModal();
	QueryPGenerator();
}

void CGDIGenePropPage::OnOK()
{
	m_activeMonitorNum = m_monitorComboCtrl.GetCurSel();
	m_selectedGcastNum = m_cCastComboCtrl.GetCurSel();

	m_doScreenBlanking = (m_blankCheck.GetSafeHwnd() && m_blankCheck.GetCheck() == BST_CHECKED) ? TRUE : FALSE;

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
		char nameBuf[1024];
		m_cCastComboCtrl.GetWindowTextA(nameBuf, 1024);
		GetConfig()->WriteProfileInt("GDIGenerator","CCastIp",m_GCast.getCcastIpAddress(m_GCast[nameBuf]));
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
