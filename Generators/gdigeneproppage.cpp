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

// Local child-control ids for the programmatic DVDO AVLab TPG config panel.
#define IDC_DVDO_STATUS       1705
#define IDC_DVDO_PATCAT_COMBO 1707
#define IDC_DVDO_PAT_COMBO    1708
#define IDC_DVDO_SHOW_BTN     1709
#define IDC_DVDO_OFF_BTN      1710
#define IDC_DVDO_READOUT      1711
#define IDC_DVDO_REFRESH_BTN  1712
#define IDC_DVDO_SETTINGS_BTN 1713
#define IDC_DVDO_DLG_COM      1750
#define IDC_DVDO_DLG_RES      1751
#define IDC_DVDO_DLG_FMT      1752
#define IDC_DVDO_DLG_RANGE    1753
#define IDC_DVDO_DLG_TEST     1754
#define IDC_DVDO_DLG_APPLY    1755
#define IDC_DVDO_DLG_CLOSE    1756
// Murideo Seven-G config panel child-control ids.
#define IDC_MURI_COM_COMBO    1720
#define IDC_MURI_TGRP_COMBO   1721
#define IDC_MURI_TIMING_COMBO 1722
#define IDC_MURI_PGRP_COMBO   1724
#define IDC_MURI_PAT_COMBO    1725
#define IDC_MURI_TEST_BTN     1726
#define IDC_MURI_APPLY_BTN    1727
#define IDC_MURI_SHOW_BTN     1728
#define IDC_MURI_STATUS       1729
#define IDC_MURI_IP_EDIT      1730
#define IDC_MURI_NET_CHECK    1731
#define IDC_MURI_READOUT      1732
#define IDC_MURI_REFRESH_BTN  1733
#define IDC_MURI_HDR_COMBO    1734
#define IDC_MURI_SETTINGS_BTN 1735
#define IDC_MURI_FMT_COMBO    1736
#define IDC_MURI_RANGE_COMBO  1737
#define IDC_MURI_GAMUT_COMBO  1738
#define IDC_MURI_DEPTH_COMBO  1739
#define IDC_MURI_CLOSE_BTN    1740
#define IDC_MURI_EDID_BTN     1741		// main-panel "Sink EDID" button
#define IDC_MURI_EDID_READOUT 1742
#define IDC_MURI_EDID_REFRESH_BTN 1743
#define IDC_MURI_EDID_COPY_BTN    1744
#define IDC_MURI_EDID_CLOSE_BTN   1745

// Implemented in GDIGenerator.cpp: open the port, run output setup, then close.
extern bool CGDIGenerator_DvdoTestConnection(const CString& comPort, int colorSpace, int range, CString& fwOut);
// Built-in pattern (command 80) + output format (command 61) tables & actions.
extern int         CGDIGenerator_DvdoCatCount();
extern const char* CGDIGenerator_DvdoCatName(int ci);
extern int         CGDIGenerator_DvdoPatCountInCat(int ci);
extern const char* CGDIGenerator_DvdoPatName(int ci, int pi);
extern int         CGDIGenerator_DvdoPatCode(int ci, int pi);
extern bool        CGDIGenerator_DvdoFindPattern(int code, int& ciOut, int& piOut);
extern bool        CGDIGenerator_DvdoShowPattern(const CString& comPort, int colorSpace, int outputFormat, int patternCode, CString& msgOut);
extern bool        CGDIGenerator_DvdoQueryReadout(const CString& comPort, int csConfig, bool lim, CString& out);
extern int         CGDIGenerator_DvdoFmtCount();
extern const char* CGDIGenerator_DvdoFmtName(int i);
extern int         CGDIGenerator_DvdoFmtCode(int i);
extern bool        CGDIGenerator_DvdoApplyOutput(const CString& comPort, int colorSpace, int formatCode, CString& msgOut);
extern int         CGDIGenerator_DvdoFmtIndexForCode(int code);
// Murideo Seven-G preset tables + actions (GDIGenerator.cpp).
extern int         CGDIGenerator_MuriTimingGroups();
extern const char* CGDIGenerator_MuriTimingGroupName(int gi);
extern int         CGDIGenerator_MuriTimingCount(int gi);
extern const char* CGDIGenerator_MuriTimingName(int gi, int i);
extern int         CGDIGenerator_MuriTimingId(int gi, int i);
extern bool        CGDIGenerator_MuriFindTiming(int id, int& gi, int& ii);
extern int         CGDIGenerator_MuriPatGroups();
extern const char* CGDIGenerator_MuriPatGroupName(int gi);
extern int         CGDIGenerator_MuriPatCount(int gi);
extern const char* CGDIGenerator_MuriPatName(int gi, int i);
extern int         CGDIGenerator_MuriPatId(int gi, int i);
extern int         CGDIGenerator_MuriPatBer(int gi, int i);
extern bool        CGDIGenerator_MuriFindPat(int id, int ber, int& gi, int& ii);
extern int         CGDIGenerator_MuriCsCount();
extern const char* CGDIGenerator_MuriCsName(int i);
extern int         CGDIGenerator_MuriCsId(int i);
extern int         CGDIGenerator_MuriCsIndexForId(int id);
extern bool        CGDIGenerator_MuriTestConnection(bool useNet, const CString& ip, const CString& comPort, CString& msgOut);
extern bool        CGDIGenerator_MuriApplyOutput(bool useNet, const CString& ip, const CString& comPort, int timingId, int csId, int bt2020, int hdrMode, int bitDepth, CString& msgOut);
extern bool        CGDIGenerator_MuriReadSinkInfo(bool useNet, const CString& ip, const CString& comPort, int tcpPort, CString& summaryOut);
extern bool        CGDIGenerator_MuriShowPattern(bool useNet, const CString& ip, const CString& comPort, int patternId, int patternBer, CString& msgOut);
extern bool        CGDIGenerator_MuriQueryReadout(const CString& ip, CString& readoutOut);
extern bool        CGDIGenerator_MuriQueryReadoutSerial(const CString& comPort, CString& readoutOut);

IMPLEMENT_DYNCREATE(CGDIGenePropPage, CPropertyPageWithHelp)

CGDIGenePropPage::CGDIGenePropPage() : CPropertyPageWithHelp(CGDIGenePropPage::IDD)
{
	//{{AFX_DATA_INIT(CGDIGenePropPage)
	m_rectSizePercent = 0;
	m_offsetx = 0;
	m_pgenQuerying = FALSE;
	m_dvdoQuerying = FALSE;
	m_muriQuerying = FALSE;
	m_pgenQuerySettle = FALSE;
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

	m_grpDisplay = m_grpMadvr = m_grpCast = m_grpPgen = m_grpSignal = m_grpPattern = m_grpBlanking = m_grpDvdo = m_grpMuri = NULL;
	m_lblOutput = m_lblScreen = m_lblSize = m_lblApl = m_lblIntensity = NULL;
	m_lblXoff = m_lblYoff = m_lblCastDev = m_lblRange = m_lblOffset = NULL;
	m_lblDvdoPatCat = m_lblDvdoPat = NULL;
	m_lblMuriPatGrp = m_lblMuriPat = NULL;
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
#define WM_DVDO_QUERY_DONE (WM_USER + 173)
#define WM_MURI_QUERY_DONE (WM_USER + 174)

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
	ON_BN_CLICKED(IDC_DVDO_SHOW_BTN, OnDvdoShow)
	ON_BN_CLICKED(IDC_DVDO_OFF_BTN, OnDvdoOff)
	ON_BN_CLICKED(IDC_DVDO_REFRESH_BTN, OnDvdoRefresh)
	ON_BN_CLICKED(IDC_DVDO_SETTINGS_BTN, OnDvdoSettings)
	ON_CBN_SELCHANGE(IDC_DVDO_PATCAT_COMBO, OnDvdoCatChange)
	ON_BN_CLICKED(IDC_MURI_SHOW_BTN, OnMuriShow)
	ON_BN_CLICKED(IDC_MURI_REFRESH_BTN, OnMuriRefresh)
	ON_BN_CLICKED(IDC_MURI_SETTINGS_BTN, OnMuriSettings)
	ON_BN_CLICKED(IDC_MURI_EDID_BTN, OnMuriEdid)
	ON_CBN_SELCHANGE(IDC_MURI_PGRP_COMBO, OnMuriPatGrpChange)
	ON_MESSAGE(WM_PGEN_QUERY_DONE, OnPgenQueryDone)
	ON_MESSAGE(WM_DVDO_QUERY_DONE, OnDvdoQueryDone)
	ON_MESSAGE(WM_MURI_QUERY_DONE, OnMuriQueryDone)
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

// Load a localized format string and format it with one string arg.
static CString LSf(UINT id, LPCTSTR a) { CString s; s.Format(LS(id), a); return s; }

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
static const int kComboToMode[8] = { DISPLAY_GDI, DISPLAY_GDI_nBG, DISPLAY_GDI_Hide, DISPLAY_madVR, DISPLAY_ccast, DISPLAY_rPI, DISPLAY_DVDO, DISPLAY_MURIDEO };

} // namespace

int CGDIGenePropPage::ComboToMode(int sel)
{
    if (sel < 0 || sel > 7) return DISPLAY_GDI;
    return kComboToMode[sel];
}

int CGDIGenePropPage::ModeToCombo(int mode)
{
    for (int i = 0; i < 8; i++) if (kComboToMode[i] == mode) return i;
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
	m_grpDisplay = m_grpMadvr = m_grpCast = m_grpPgen = m_grpSignal = m_grpPattern = m_grpBlanking = m_grpDvdo = m_grpMuri = NULL;
	m_lblOutput = m_lblScreen = m_lblSize = m_lblApl = m_lblIntensity = NULL;
	m_lblXoff = m_lblYoff = m_lblCastDev = m_lblRange = m_lblOffset = NULL;
	m_lblDvdoPatCat = m_lblDvdoPat = NULL;
	m_lblMuriPatGrp = m_lblMuriPat = NULL;

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
		m_outputCombo.AddString(_T("DVDO AVLab TPG"));
		m_outputCombo.AddString(_T("Murideo Seven-G"));
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
		m_pgenReadout.SetWindowText(LS(IDS_GEN_PGEN_INFO_PLACEHOLDER));
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

	// DVDO AVLab TPG config group + controls (positioned/shown in Relayout when
	// that output mode is selected). The combos persist across layout rebuilds;
	// the group frame and labels are recreated with the rest of m_dynAll.
	m_grpDvdo = new CButton();
	{ CPoint gp = M.at(GRP_X, 26); m_grpDvdo->Create(_T("DVDO AVLab TPG"), WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(gp.x, gp.y, gp.x + M.w(GRP_W), gp.y + M.ht(80)), this, (UINT)IDC_STATIC); m_grpDvdo->SetFont(font); m_dynAll.Add(m_grpDvdo); }
	m_lblDvdoPatCat = AddText(this, m_dynAll, font, M, LS(IDS_GEN_PATTERN_GROUP), LBL_X, 0, 60, 9);
	m_lblDvdoPat    = AddText(this, m_dynAll, font, M, LS(IDS_GEN_PATTERN),       LBL_X, 0, 60, 9);
	if (!m_dvdoPatCatCombo.GetSafeHwnd()) { m_dvdoPatCatCombo.Create(WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, CRect(0,0,M.w(120),M.ht(140)), this, IDC_DVDO_PATCAT_COMBO); m_dvdoPatCatCombo.SetFont(font);
		for (int i = 0; i < CGDIGenerator_DvdoCatCount(); ++i) m_dvdoPatCatCombo.AddString(CString(CGDIGenerator_DvdoCatName(i))); }
	if (!m_dvdoPatCombo.GetSafeHwnd())    { m_dvdoPatCombo.Create(WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, CRect(0,0,M.w(120),M.ht(200)), this, IDC_DVDO_PAT_COMBO); m_dvdoPatCombo.SetFont(font); }
	if (!m_dvdoShowBtn.GetSafeHwnd())    { m_dvdoShowBtn.Create(LS(IDS_GEN_SHOW_PATTERN), WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, CRect(0,0,M.w(64),M.ht(14)), this, IDC_DVDO_SHOW_BTN); m_dvdoShowBtn.SetFont(font); }
	if (!m_dvdoOffBtn.GetSafeHwnd())     { m_dvdoOffBtn.Create(LS(IDS_GEN_PATTERNS_OFF), WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, CRect(0,0,M.w(60),M.ht(14)), this, IDC_DVDO_OFF_BTN); m_dvdoOffBtn.SetFont(font); }
	if (!m_dvdoStatus.GetSafeHwnd())     { m_dvdoStatus.Create(_T(""), WS_CHILD | SS_LEFT, CRect(0,0,M.w(150),M.ht(9)), this, IDC_DVDO_STATUS); m_dvdoStatus.SetFont(font); }
	if (!m_dvdoReadout.GetSafeHwnd())    { m_dvdoReadout.Create(WS_CHILD | ES_MULTILINE | ES_READONLY | WS_TABSTOP, CRect(0,0,M.w(166),M.ht(72)), this, IDC_DVDO_READOUT); m_dvdoReadout.SetFont(font); }
	if (!m_dvdoRefreshBtn.GetSafeHwnd()) { m_dvdoRefreshBtn.Create(LS(IDS_GEN_REFRESH), WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, CRect(0,0,M.w(44),M.ht(14)), this, IDC_DVDO_REFRESH_BTN); m_dvdoRefreshBtn.SetFont(font); }
	if (!m_dvdoSettingsBtn.GetSafeHwnd()) { m_dvdoSettingsBtn.Create(LS(IDS_GEN_DVDO_SETTINGS_BTN), WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, CRect(0,0,M.w(96),M.ht(14)), this, IDC_DVDO_SETTINGS_BTN); m_dvdoSettingsBtn.SetFont(font); }
	{
		// Restore the pattern pickers from the saved pattern code (default: first group).
		int ci = 0, pi = 0;
		CGDIGenerator_DvdoFindPattern(GetConfig()->GetProfileInt("GDIGenerator","DvdoPatternCode",0), ci, pi);
		m_dvdoPatCatCombo.SetCurSel(ci);
		PopulateDvdoPatternCombo(ci);
		m_dvdoPatCombo.SetCurSel(pi);
	}

	// --- Murideo Seven-G config panel (programmatic, mirrors the DVDO panel) ---
	m_grpMuri = new CButton();
	{ CPoint gp = M.at(GRP_X, 26); m_grpMuri->Create(_T("Murideo Seven-G"), WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(gp.x, gp.y, gp.x + M.w(GRP_W), gp.y + M.ht(80)), this, (UINT)IDC_STATIC); m_grpMuri->SetFont(font); m_dynAll.Add(m_grpMuri); }
	m_lblMuriPatGrp    = AddText(this, m_dynAll, font, M, LS(IDS_GEN_PATTERN_GROUP),  LBL_X, 0, 60, 9);
	m_lblMuriPat       = AddText(this, m_dynAll, font, M, LS(IDS_GEN_PATTERN),        LBL_X, 0, 60, 9);
	if (!m_muriReadout.GetSafeHwnd())      { m_muriReadout.Create(WS_CHILD | ES_MULTILINE | ES_READONLY | WS_TABSTOP, CRect(0,0,M.w(166),M.ht(88)), this, IDC_MURI_READOUT); m_muriReadout.SetFont(font); }
	if (!m_muriRefreshBtn.GetSafeHwnd())   { m_muriRefreshBtn.Create(LS(IDS_GEN_REFRESH), WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, CRect(0,0,M.w(44),M.ht(14)), this, IDC_MURI_REFRESH_BTN); m_muriRefreshBtn.SetFont(font); }
	if (!m_muriSettingsBtn.GetSafeHwnd())  { m_muriSettingsBtn.Create(LS(IDS_GEN_MURI_SETTINGS_BTN), WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, CRect(0,0,M.w(100),M.ht(14)), this, IDC_MURI_SETTINGS_BTN); m_muriSettingsBtn.SetFont(font); }
	if (!m_muriEdidBtn.GetSafeHwnd())       { m_muriEdidBtn.Create(LS(IDS_GEN_MURI_EDID_BTN), WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, CRect(0,0,M.w(70),M.ht(14)), this, IDC_MURI_EDID_BTN); m_muriEdidBtn.SetFont(font); }
	if (!m_muriPatGrpCombo.GetSafeHwnd())  { m_muriPatGrpCombo.Create(WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, CRect(0,0,M.w(90),M.ht(120)), this, IDC_MURI_PGRP_COMBO); m_muriPatGrpCombo.SetFont(font);
		for (int i = 0; i < CGDIGenerator_MuriPatGroups(); ++i) m_muriPatGrpCombo.AddString(CString(CGDIGenerator_MuriPatGroupName(i))); }
	if (!m_muriPatCombo.GetSafeHwnd())     { m_muriPatCombo.Create(WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, CRect(0,0,M.w(120),M.ht(200)), this, IDC_MURI_PAT_COMBO); m_muriPatCombo.SetFont(font); }
	if (!m_muriShowBtn.GetSafeHwnd())      { m_muriShowBtn.Create(LS(IDS_GEN_SHOW_PATTERN), WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, CRect(0,0,M.w(64),M.ht(14)), this, IDC_MURI_SHOW_BTN); m_muriShowBtn.SetFont(font); }
	if (!m_muriStatus.GetSafeHwnd())       { m_muriStatus.Create(_T(""), WS_CHILD | SS_LEFT, CRect(0,0,M.w(150),M.ht(9)), this, IDC_MURI_STATUS); m_muriStatus.SetFont(font); }
	{
		int gi = 0, ii = 0;
		if (CGDIGenerator_MuriFindPat(GetConfig()->GetProfileInt("GDIGenerator","MuriPatternId",-1), GetConfig()->GetProfileInt("GDIGenerator","MuriPatternBer",0), gi, ii)) { m_muriPatGrpCombo.SetCurSel(gi); PopulateMuriPatCombo(gi); m_muriPatCombo.SetCurSel(ii); }
		else { m_muriPatGrpCombo.SetCurSel(0); PopulateMuriPatCombo(0); }
	}

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
	BOOL isDvdo    = (mode == DISPLAY_DVDO);
	BOOL isMuri    = (mode == DISPLAY_MURIDEO);
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
	CWnd* groups[] = { m_grpDisplay, m_grpMadvr, m_grpCast, m_grpPgen, m_grpSignal, m_grpPattern, m_grpBlanking, m_grpDvdo, m_grpMuri };
	for (int i = 0; i < (int)(sizeof(groups)/sizeof(groups[0])); i++) if (groups[i]) groups[i]->ShowWindow(SW_HIDE);
	if (m_pgenReadout.GetSafeHwnd()) m_pgenReadout.ShowWindow(SW_HIDE);
	if (m_pgenSettingsBtn.GetSafeHwnd()) m_pgenSettingsBtn.ShowWindow(SW_HIDE);
	if (m_tenBitCheck.GetSafeHwnd()) m_tenBitCheck.ShowWindow(SW_HIDE);
	if (m_tenBitMadvrCheck.GetSafeHwnd()) m_tenBitMadvrCheck.ShowWindow(SW_HIDE);
	if (m_pgenRefreshBtn.GetSafeHwnd()) m_pgenRefreshBtn.ShowWindow(SW_HIDE);
	{ CWnd* dv[] = { &m_dvdoStatus, &m_dvdoPatCatCombo, &m_dvdoPatCombo, &m_dvdoShowBtn, &m_dvdoOffBtn, &m_dvdoReadout, &m_dvdoRefreshBtn, &m_dvdoSettingsBtn };
	  for (int i = 0; i < (int)(sizeof(dv)/sizeof(dv[0])); i++) if (dv[i]->GetSafeHwnd()) dv[i]->ShowWindow(SW_HIDE); }
	{ CWnd* mu[] = { &m_muriPatGrpCombo, &m_muriPatCombo, &m_muriShowBtn, &m_muriStatus,
	                 &m_muriReadout, &m_muriRefreshBtn, &m_muriSettingsBtn, &m_muriEdidBtn };
	  for (int i = 0; i < (int)(sizeof(mu)/sizeof(mu[0])); i++) if (mu[i]->GetSafeHwnd()) mu[i]->ShowWindow(SW_HIDE); }
	CWnd* labels[] = { m_lblScreen, m_lblSize, m_lblApl, m_lblIntensity, m_lblXoff, m_lblYoff, m_lblCastDev, m_lblRange, m_lblOffset,
	                   m_lblDvdoPatCat, m_lblDvdoPat,
	                   m_lblMuriPatGrp, m_lblMuriPat };
	for (int i = 0; i < (int)(sizeof(labels)/sizeof(labels[0])); i++) if (labels[i]) labels[i]->ShowWindow(SW_HIDE);

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
	if (isDvdo)
	{
		int top = y, cy = top + TOP_INSET;
		// Live status readout (Murideo-style) with "DVDO settings..." + Refresh beneath. All
		// output controls (COM / resolution / format / range) live in the settings popup.
		{ CPoint rp = M.at(LBL_X, cy); int rw = grpRightPx - M.w(4) - rp.x; m_dvdoReadout.MoveWindow(rp.x, rp.y, rw, M.ht(80)); m_dvdoReadout.ShowWindow(SW_SHOW); }
		cy += 84;
		{ CPoint p = M.at(LBL_X, cy); m_dvdoSettingsBtn.MoveWindow(p.x, p.y, M.w(96), M.ht(14)); m_dvdoSettingsBtn.ShowWindow(SW_SHOW); }
		{ CPoint p = M.at(LBL_X, cy); m_dvdoRefreshBtn.MoveWindow(grpRightPx - M.w(4) - M.w(44), p.y, M.w(44), M.ht(14)); m_dvdoRefreshBtn.ShowWindow(SW_SHOW); }
		cy += ROW_F + 4;
		// Built-in test-pattern picker: group -> pattern, plus Show / Off.
		MoveWnd(m_lblDvdoPatCat, M, LBL_X, cy + 2, 60, 9); m_lblDvdoPatCat->ShowWindow(SW_SHOW);
		{ CPoint p = M.at(72, cy); m_dvdoPatCatCombo.MoveWindow(p.x, p.y, M.w(120), M.ht(140)); m_dvdoPatCatCombo.ShowWindow(SW_SHOW); }
		cy += ROW_F;
		MoveWnd(m_lblDvdoPat, M, LBL_X, cy + 2, 60, 9); m_lblDvdoPat->ShowWindow(SW_SHOW);
		{ CPoint p = M.at(72, cy); m_dvdoPatCombo.MoveWindow(p.x, p.y, M.w(120), M.ht(200)); m_dvdoPatCombo.ShowWindow(SW_SHOW); }
		cy += ROW_F;
		{ CPoint p = M.at(72, cy); m_dvdoShowBtn.MoveWindow(p.x, p.y, M.w(64), M.ht(14)); m_dvdoShowBtn.ShowWindow(SW_SHOW); }
		{ CPoint p = M.at(72 + 68, cy); m_dvdoOffBtn.MoveWindow(p.x, p.y, M.w(60), M.ht(14)); m_dvdoOffBtn.ShowWindow(SW_SHOW); }
		cy += ROW_F;
		{ CPoint p = M.at(LBL_X, cy + 1); m_dvdoStatus.MoveWindow(p.x, p.y, grpRightPx - M.w(4) - p.x, M.ht(9)); m_dvdoStatus.ShowWindow(SW_SHOW); }
		cy += ROW_F;
		int fb = cy + BOT_PAD;
		PlaceGroup(m_grpDvdo, M, top, fb - top, grpRightPx);
		y = fb + GRP_GAP;
	}
	if (isMuri)
	{
		int top = y, cy = top + TOP_INSET;
		// Live status readout (PGenerator-style), with Refresh + Settings... beneath.
		{ CPoint rp = M.at(LBL_X, cy); int rw = grpRightPx - M.w(4) - rp.x; m_muriReadout.MoveWindow(rp.x, rp.y, rw, M.ht(86)); m_muriReadout.ShowWindow(SW_SHOW); }
		cy += 90;
		{ CPoint p = M.at(LBL_X, cy); m_muriSettingsBtn.MoveWindow(p.x, p.y, M.w(100), M.ht(14)); m_muriSettingsBtn.ShowWindow(SW_SHOW); }
		{ CPoint p = M.at(LBL_X, cy); m_muriRefreshBtn.MoveWindow(grpRightPx - M.w(4) - M.w(44), p.y, M.w(44), M.ht(14)); m_muriRefreshBtn.ShowWindow(SW_SHOW); }
		cy += ROW_F;
		{ CPoint p = M.at(LBL_X, cy); m_muriEdidBtn.MoveWindow(p.x, p.y, M.w(80), M.ht(14)); m_muriEdidBtn.ShowWindow(SW_SHOW); }
		cy += ROW_F + 4;
		// Pattern picker: group -> pattern -> Show.
		MoveWnd(m_lblMuriPatGrp, M, LBL_X, cy + 2, 60, 9); m_lblMuriPatGrp->ShowWindow(SW_SHOW);
		{ CPoint p = M.at(72, cy); m_muriPatGrpCombo.MoveWindow(p.x, p.y, M.w(90), M.ht(120)); m_muriPatGrpCombo.ShowWindow(SW_SHOW); }
		cy += ROW_F;
		MoveWnd(m_lblMuriPat, M, LBL_X, cy + 2, 60, 9); m_lblMuriPat->ShowWindow(SW_SHOW);
		{ CPoint p = M.at(72, cy); m_muriPatCombo.MoveWindow(p.x, p.y, grpRightPx - M.w(4) - M.at(72,0).x, M.ht(200)); m_muriPatCombo.ShowWindow(SW_SHOW); }
		cy += ROW_F;
		{ CPoint p = M.at(72, cy); m_muriShowBtn.MoveWindow(p.x, p.y, M.w(64), M.ht(14)); m_muriShowBtn.ShowWindow(SW_SHOW); }
		{ CPoint p = M.at(LBL_X, cy + 2); m_muriStatus.MoveWindow(M.at(72 + 68, cy).x, p.y, grpRightPx - M.w(4) - M.at(72 + 68, 0).x, M.ht(9)); m_muriStatus.ShowWindow(SW_SHOW); }
		cy += ROW_F;
		int fb = cy + BOT_PAD;
		PlaceGroup(m_grpMuri, M, top, fb - top, grpRightPx);
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
	bool settle = (pg->m_pgenQuerySettle != FALSE); pg->m_pgenQuerySettle = FALSE;
	PgenQueryResult* r = new PgenQueryResult;
	if (settle) Sleep(1200);		// the daemon is restarting after a successful Apply
	r->ok = gen ? gen->QueryPGeneratorInfo(r->vals, r->err) : FALSE;
	if (settle && !r->ok)			// one retry through the restart window before giving up
	{ Sleep(1500); r->ok = gen ? gen->QueryPGeneratorInfo(r->vals, r->err) : FALSE; }
	if (!(hwnd && IsWindow(hwnd) && ::PostMessage(hwnd, WM_PGEN_QUERY_DONE, 0, (LPARAM)r)))
		delete r;
	return 0;
}

void CGDIGenePropPage::QueryPGenerator(bool settle)
{
	if (!m_pgenReadout.GetSafeHwnd()) return;
	if (m_pgenQuerying) return;
	m_pgenQuerying = TRUE;
	m_pgenQuerySettle = settle;		// armed only once we actually start the worker, so an
									// early-return above can't leak it into the next query
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
	m_applied = FALSE;
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
	if (cmds.GetSize() > 0 && m_pGenerator) { CWaitCursor wait; m_pGenerator->ApplyPGeneratorConf(cmds); m_applied = TRUE; }
	// Apply applies and closes. ApplyPGeneratorConf already ends every batch with
	// RESTARTPGENERATOR:, so the settings take effect without a manual restart; closing here
	// also avoids the stale-baseline hazards of a kept-open dialog (the *Init change-detection
	// baselines are captured once in OnInitDialog). The caller auto-refreshes the main panel
	// when m_applied, so the change is still reflected without reopening.
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

// ---- Murideo Seven-G settings dialog ----------------------------------------
CMuriSettingsDlg::CMuriSettingsDlg(CWnd* pParent) : CDialog(CMuriSettingsDlg::IDD, pParent) {}

BEGIN_MESSAGE_MAP(CMuriSettingsDlg, CDialog)
	ON_BN_CLICKED(IDC_MURI_TEST_BTN, OnTest)
	ON_BN_CLICKED(IDC_MURI_APPLY_BTN, OnApply)
	ON_BN_CLICKED(IDC_MURI_CLOSE_BTN, OnClose2)
	ON_BN_CLICKED(IDC_MURI_NET_CHECK, OnNetToggle)
	ON_CBN_SELCHANGE(IDC_MURI_TGRP_COMBO, OnTgrpChange)
	ON_CBN_SELCHANGE(IDC_MURI_FMT_COMBO, OnFmtChange)
END_MESSAGE_MAP()

void CMuriSettingsDlg::MuriXport(bool& useNet, CString& ip, CString& com)
{
	useNet = (m_netCheck.GetCheck() == BST_CHECKED);
	m_ipEdit.GetWindowText(ip); ip.Trim();
	m_comCombo.GetWindowText(com); com.Trim();
}

void CMuriSettingsDlg::PopulateComPorts()
{
	CString current = GetConfig()->GetProfileString("GDIGenerator","MuriComPort","");
	m_comCombo.ResetContent();
	HKEY hKey;
	if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &hKey))
	{
		char name[256], val[256]; DWORD idx = 0, cbN = sizeof(name), cbV = sizeof(val), type;
		while (ERROR_SUCCESS == RegEnumValue(hKey, idx, name, &cbN, NULL, &type, (LPBYTE)val, &cbV))
		{
			if (type == REG_SZ && _strnicmp(val, "COM", 3) == 0 && m_comCombo.FindStringExact(-1, val) == CB_ERR)
				m_comCombo.AddString(val);
			idx++; cbN = sizeof(name); cbV = sizeof(val);
		}
		RegCloseKey(hKey);
	}
	if (!current.IsEmpty() && m_comCombo.FindStringExact(-1, current) == CB_ERR) m_comCombo.AddString(current);
	int sel = current.IsEmpty() ? CB_ERR : m_comCombo.FindStringExact(-1, current);
	if (sel != CB_ERR) m_comCombo.SetCurSel(sel); else if (m_comCombo.GetCount() > 0) m_comCombo.SetCurSel(0);
}

void CMuriSettingsDlg::PopulateTimingCombo(int grp)
{
	m_timingCombo.ResetContent();
	int n = CGDIGenerator_MuriTimingCount(grp);
	for (int i = 0; i < n; ++i) m_timingCombo.AddString(CString(CGDIGenerator_MuriTimingName(grp, i)));
	if (n > 0) m_timingCombo.SetCurSel(0);
}

void CMuriSettingsDlg::OnTgrpChange()
{
	int g = m_tgrpCombo.GetCurSel(); if (g < 0) g = 0;
	PopulateTimingCombo(g);
}

// Colour format (0=RGB,1=YC444,2=YC422,3=YC420) + range (0=Full,1=Limited) -> cat-99 id.
int CMuriSettingsDlg::ComboCsId()
{
	int fmt = m_fmtCombo.GetCurSel(); if (fmt < 0) fmt = 0;
	int rng = m_rangeCombo.GetCurSel(); if (rng < 0) rng = 0;
	if (fmt == 0) return (rng == 0) ? 0 : 1;	// RGB: Full=0, Limited=1
	return fmt + 1;								// YC444=2, YC422=3, YC420=4 (always 16-235)
}

// Only RGB supports Full range; YCbCr is always Limited, so lock the range combo there.
void CMuriSettingsDlg::OnFmtChange()
{
	bool isRgb = (m_fmtCombo.GetCurSel() <= 0);
	if (!isRgb) m_rangeCombo.SetCurSel(1);		// Limited
	m_rangeCombo.EnableWindow(isRgb);
}

void CMuriSettingsDlg::UpdateTransportEnable()
{
	bool net = (m_netCheck.GetCheck() == BST_CHECKED);
	m_ipEdit.EnableWindow(net);   m_lblIp.EnableWindow(net);
	m_comCombo.EnableWindow(!net); m_lblCom.EnableWindow(!net);
}
void CMuriSettingsDlg::OnNetToggle() { UpdateTransportEnable(); }

BOOL CMuriSettingsDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	SetWindowText(LS(IDS_GEN_MURI_SETTINGS_TITLE));
	DlgMap M; M.h = GetSafeHwnd();
	CFont* font = GetParent() ? GetParent()->GetFont() : GetFont();

	const int LX = 8, LW = 74, CX = 86, CW = 128;
	int y = 8;
	#define MK_LBL(ctl,txt) { CPoint p = M.at(LX, y + 2); ctl.Create(txt, WS_CHILD | WS_VISIBLE, CRect(p.x, p.y, p.x + M.w(LW), p.y + M.ht(9)), this); ctl.SetFont(font); }
	#define MK_CB(ctl,id,h) { CPoint p = M.at(CX, y); ctl.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, CRect(p.x, p.y, p.x + M.w(CW), p.y + M.ht(h)), this, id); ctl.SetFont(font); }

	{ CPoint p = M.at(CX, y); m_netCheck.Create(LS(IDS_GEN_USE_NETWORK), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, CRect(p.x, p.y, p.x + M.w(CW), p.y + M.ht(12)), this, IDC_MURI_NET_CHECK); m_netCheck.SetFont(font); }
	y += 16;
	MK_LBL(m_lblIp, LS(IDS_GEN_IP_ADDRESS));
	{ CPoint p = M.at(CX, y); m_ipEdit.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, CRect(p.x, p.y, p.x + M.w(CW), p.y + M.ht(12)), this, IDC_MURI_IP_EDIT); m_ipEdit.SetFont(font); }
	y += 15;
	MK_LBL(m_lblCom, LS(IDS_GEN_COM_PORT));
	{ CPoint p = M.at(CX, y); m_comCombo.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWN | WS_VSCROLL, CRect(p.x, p.y, p.x + M.w(CW), p.y + M.ht(120)), this, IDC_MURI_COM_COMBO); m_comCombo.SetFont(font); }
	y += 16;
	{ CPoint p = M.at(CX, y); m_testBtn.Create(LS(IDS_GEN_DETECT_TEST), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, CRect(p.x, p.y, p.x + M.w(72), p.y + M.ht(14)), this, IDC_MURI_TEST_BTN); m_testBtn.SetFont(font); }
	y += 17;
	{ CPoint p = M.at(LX, y + 1); m_status.Create(_T(""), WS_CHILD | WS_VISIBLE | SS_LEFT, CRect(p.x, p.y, p.x + M.w(LW + CW), p.y + M.ht(9)), this); m_status.SetFont(font); }
	y += 15;

	MK_LBL(m_lblTgrp, LS(IDS_GEN_RESOLUTION_GRP)); MK_CB(m_tgrpCombo, IDC_MURI_TGRP_COMBO, 120);
	for (int i = 0; i < CGDIGenerator_MuriTimingGroups(); ++i) m_tgrpCombo.AddString(CString(CGDIGenerator_MuriTimingGroupName(i)));
	y += 15;
	MK_LBL(m_lblTiming, LS(IDS_GEN_RESOLUTION)); MK_CB(m_timingCombo, IDC_MURI_TIMING_COMBO, 200);
	y += 15;
	MK_LBL(m_lblFmt, LS(IDS_GEN_COLOR_FORMAT)); MK_CB(m_fmtCombo, IDC_MURI_FMT_COMBO, 100);
	m_fmtCombo.AddString(_T("RGB")); m_fmtCombo.AddString(_T("YCbCr 4:4:4")); m_fmtCombo.AddString(_T("YCbCr 4:2:2")); m_fmtCombo.AddString(_T("YCbCr 4:2:0"));
	y += 15;
	MK_LBL(m_lblRange, LS(IDS_GEN_SIGNAL_RANGE)); MK_CB(m_rangeCombo, IDC_MURI_RANGE_COMBO, 80);
	m_rangeCombo.AddString(_T("Full (0-255)")); m_rangeCombo.AddString(_T("Limited (16-235)"));
	y += 15;
	MK_LBL(m_lblGamut, LS(IDS_GEN_COLOR_SPACE)); MK_CB(m_gamutCombo, IDC_MURI_GAMUT_COMBO, 80);
	m_gamutCombo.AddString(_T("BT.709")); m_gamutCombo.AddString(_T("BT.2020"));
	y += 15;
	MK_LBL(m_lblHdr, LS(IDS_GEN_DYNAMIC_RANGE)); MK_CB(m_hdrCombo, IDC_MURI_HDR_COMBO, 80);
	m_hdrCombo.AddString(_T("SDR")); m_hdrCombo.AddString(_T("HDR")); m_hdrCombo.AddString(_T("HLG"));
	y += 15;
	MK_LBL(m_lblDepth, LS(IDS_GEN_BIT_DEPTH)); MK_CB(m_depthCombo, IDC_MURI_DEPTH_COMBO, 60);
	m_depthCombo.AddString(_T("8 bit")); m_depthCombo.AddString(_T("10 bit"));
	y += 20;
	{ CPoint p = M.at(CX, y); m_applyBtn.Create(LS(IDS_GEN_APPLY), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, CRect(p.x, p.y, p.x + M.w(58), p.y + M.ht(14)), this, IDC_MURI_APPLY_BTN); m_applyBtn.SetFont(font); }
	{ CPoint p = M.at(CX + 66, y); m_closeBtn.Create(LS(IDS_GEN_CLOSE), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, CRect(p.x, p.y, p.x + M.w(58), p.y + M.ht(14)), this, IDC_MURI_CLOSE_BTN); m_closeBtn.SetFont(font); }
	int bottomY = y + 20;
	#undef MK_LBL
	#undef MK_CB

	// Hide the reused IDD_PGEN_SETTINGS shell buttons (we use our own Apply/Close).
	if (GetDlgItem(IDOK))     GetDlgItem(IDOK)->ShowWindow(SW_HIDE);
	if (GetDlgItem(IDCANCEL)) GetDlgItem(IDCANCEL)->ShowWindow(SW_HIDE);
	if (GetDlgItem(IDHELP))   GetDlgItem(IDHELP)->ShowWindow(SW_HIDE);

	// Resize the dialog to fit our controls and re-centre.
	{
		CRect wr, cr; GetWindowRect(&wr); GetClientRect(&cr);
		int bW = wr.Width() - cr.Width(), bH = wr.Height() - cr.Height();
		SetWindowPos(NULL, 0, 0, M.w(CX + CW + 10) + bW, M.ht(bottomY) + bH, SWP_NOMOVE | SWP_NOZORDER);
		CenterWindow();
	}

	// Load current values from config.
	m_netCheck.SetCheck(GetConfig()->GetProfileInt("GDIGenerator","MuriUseNetwork",1) ? BST_CHECKED : BST_UNCHECKED);
	m_ipEdit.SetWindowText(GetConfig()->GetProfileString("GDIGenerator","MuriIp","192.168.1.239"));
	PopulateComPorts();
	m_fmtCombo.SetCurSel(GetConfig()->GetProfileInt("GDIGenerator","MuriColorFormat",0));
	m_rangeCombo.SetCurSel(GetConfig()->GetProfileInt("GDIGenerator","MuriRange",0));
	m_gamutCombo.SetCurSel(GetConfig()->GetProfileInt("GDIGenerator","MuriBt2020",0));
	m_hdrCombo.SetCurSel(GetConfig()->GetProfileInt("GDIGenerator","MuriHdrMode",0));
	m_depthCombo.SetCurSel(GetConfig()->GetProfileInt("GDIGenerator","MuriBitDepth",0));
	{
		int gi = 0, ii = 0;
		if (CGDIGenerator_MuriFindTiming(GetConfig()->GetProfileInt("GDIGenerator","MuriTimingId",-1), gi, ii))
		{ m_tgrpCombo.SetCurSel(gi); PopulateTimingCombo(gi); m_timingCombo.SetCurSel(ii); }
		else { m_tgrpCombo.SetCurSel(0); PopulateTimingCombo(0); }
	}
	OnFmtChange();
	UpdateTransportEnable();
	return TRUE;
}

void CMuriSettingsDlg::OnTest()
{
	bool net; CString ip, com; MuriXport(net, ip, com);
	if (net && ip.IsEmpty()) { m_status.SetWindowText(LS(IDS_GEN_ENTER_MURI_IP_FIRST)); return; }
	if (!net && com.IsEmpty()) { m_status.SetWindowText(LS(IDS_GEN_SELECT_COM_FIRST)); return; }
	m_status.SetWindowText(LS(IDS_GEN_CONNECTING));
	CString msg; CGDIGenerator_MuriTestConnection(net, ip, com, msg);
	m_status.SetWindowText(msg);
}


void CMuriSettingsDlg::OnApply()
{
	bool net; CString ip, com; MuriXport(net, ip, com);
	if (net && ip.IsEmpty()) { m_status.SetWindowText(LS(IDS_GEN_ENTER_MURI_IP_FIRST)); return; }
	int tg = m_tgrpCombo.GetCurSel(); if (tg < 0) tg = 0;
	int ti = m_timingCombo.GetCurSel(); if (ti < 0) ti = 0;
	int timingId = CGDIGenerator_MuriTimingId(tg, ti);
	int csId  = ComboCsId();
	int gamut = m_gamutCombo.GetCurSel(); if (gamut < 0) gamut = 0;	// 0=BT.709,1=BT.2020 -> cat112
	int hdr   = m_hdrCombo.GetCurSel();   if (hdr < 0) hdr = 0;
	int depth = m_depthCombo.GetCurSel(); if (depth < 0) depth = 0;	// 0=8bit,1=10bit -> cat100
	CString msg;
	CGDIGenerator_MuriApplyOutput(net, ip, com, timingId, csId, gamut, hdr, depth, msg);
	m_status.SetWindowText(msg);
}

void CMuriSettingsDlg::SaveToConfig()
{
	CString ip; m_ipEdit.GetWindowText(ip); ip.Trim();
	CString com; m_comCombo.GetWindowText(com); com.Trim();
	GetConfig()->WriteProfileString("GDIGenerator","MuriIp",ip);
	GetConfig()->WriteProfileString("GDIGenerator","MuriComPort",com);
	GetConfig()->WriteProfileInt("GDIGenerator","MuriUseNetwork", m_netCheck.GetCheck() == BST_CHECKED ? 1 : 0);
	int fmt = m_fmtCombo.GetCurSel() < 0 ? 0 : m_fmtCombo.GetCurSel();
	int rng = m_rangeCombo.GetCurSel() < 0 ? 0 : m_rangeCombo.GetCurSel();
	GetConfig()->WriteProfileInt("GDIGenerator","MuriColorFormat", fmt);
	GetConfig()->WriteProfileInt("GDIGenerator","MuriRange", rng);
	GetConfig()->WriteProfileInt("GDIGenerator","MuriBt2020", m_gamutCombo.GetCurSel() < 0 ? 0 : m_gamutCombo.GetCurSel());
	GetConfig()->WriteProfileInt("GDIGenerator","MuriHdrMode", m_hdrCombo.GetCurSel() < 0 ? 0 : m_hdrCombo.GetCurSel());
	GetConfig()->WriteProfileInt("GDIGenerator","MuriBitDepth", m_depthCombo.GetCurSel() < 0 ? 0 : m_depthCombo.GetCurSel());
	GetConfig()->WriteProfileInt("GDIGenerator","MuriColorSpaceId", ComboCsId());
	// Effective range (YCbCr is always Limited) -> HCFR range flag for the main-page stimulus.
	int effRange = (fmt == 0) ? rng : 1;
	GetConfig()->WriteProfileInt("GDIGenerator","RGB_16_235", effRange);
	if (m_tgrpCombo.GetCurSel() >= 0 && m_timingCombo.GetCurSel() >= 0)
		GetConfig()->WriteProfileInt("GDIGenerator","MuriTimingId",
			CGDIGenerator_MuriTimingId(m_tgrpCombo.GetCurSel(), m_timingCombo.GetCurSel()));
}

void CMuriSettingsDlg::OnClose2() { SaveToConfig(); EndDialog(IDOK); }

// ---- Murideo connected-sink EDID report dialog ------------------------------
CMuriEdidDlg::CMuriEdidDlg(CWnd* pParent) : CDialog(CMuriEdidDlg::IDD, pParent) {}

BEGIN_MESSAGE_MAP(CMuriEdidDlg, CDialog)
	ON_BN_CLICKED(IDC_MURI_EDID_REFRESH_BTN, OnRefresh)
	ON_BN_CLICKED(IDC_MURI_EDID_COPY_BTN, OnCopy)
	ON_BN_CLICKED(IDC_MURI_EDID_CLOSE_BTN, OnClose2)
END_MESSAGE_MAP()

BOOL CMuriEdidDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	SetWindowText(LS(IDS_GEN_MURI_EDID_TITLE));
	DlgMap M; M.h = GetSafeHwnd();
	CFont* font = GetParent() ? GetParent()->GetFont() : GetFont();
	m_mono.CreatePointFont(90, _T("Consolas"));		// fixed pitch so the columns line up

	const int LX = 8, W = 250, H = 250;
	{ CPoint p = M.at(LX, 6); m_readout.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, CRect(p.x, p.y, p.x + M.w(W), p.y + M.ht(H)), this, IDC_MURI_EDID_READOUT); m_readout.SetFont(&m_mono); }
	int by = 6 + H + 6;
	{ CPoint p = M.at(LX, by); m_refreshBtn.Create(LS(IDS_GEN_REFRESH), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, CRect(p.x, p.y, p.x + M.w(70), p.y + M.ht(14)), this, IDC_MURI_EDID_REFRESH_BTN); m_refreshBtn.SetFont(font); }
	{ CPoint p = M.at(LX + 78, by); m_copyBtn.Create(LS(IDS_GEN_COPY_CLIPBOARD), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, CRect(p.x, p.y, p.x + M.w(96), p.y + M.ht(14)), this, IDC_MURI_EDID_COPY_BTN); m_copyBtn.SetFont(font); }
	{ CPoint p = M.at(W - 62, by); m_closeBtn.Create(LS(IDS_GEN_CLOSE), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, CRect(p.x, p.y, p.x + M.w(60), p.y + M.ht(14)), this, IDC_MURI_EDID_CLOSE_BTN); m_closeBtn.SetFont(font); }

	if (GetDlgItem(IDOK))     GetDlgItem(IDOK)->ShowWindow(SW_HIDE);
	if (GetDlgItem(IDCANCEL)) GetDlgItem(IDCANCEL)->ShowWindow(SW_HIDE);
	if (GetDlgItem(IDHELP))   GetDlgItem(IDHELP)->ShowWindow(SW_HIDE);
	{
		CRect wr, cr; GetWindowRect(&wr); GetClientRect(&cr);
		int bW = wr.Width() - cr.Width(), bH = wr.Height() - cr.Height();
		SetWindowPos(NULL, 0, 0, M.w(W + 16) + bW, M.ht(by + 20) + bH, SWP_NOMOVE | SWP_NOZORDER);
		CenterWindow();
	}
	LoadEdid();
	return TRUE;
}

void CMuriEdidDlg::LoadEdid()
{
	bool net = GetConfig()->GetProfileInt("GDIGenerator", "MuriUseNetwork", 1) != 0;
	CString ip = GetConfig()->GetProfileString("GDIGenerator", "MuriIp", "192.168.1.239");
	CString com = GetConfig()->GetProfileString("GDIGenerator", "MuriComPort", "");
	int port = GetConfig()->GetProfileInt("GDIGenerator", "MuriTcpPort", 23);
	if (net && ip.IsEmpty()) { m_readout.SetWindowText(LS(IDS_GEN_SET_MURI_IP_SETTINGS)); return; }
	if (!net && com.IsEmpty()) { m_readout.SetWindowText(LS(IDS_GEN_SET_MURI_COM_SETTINGS)); return; }
	m_readout.SetWindowText(LS(IDS_GEN_READING_EDID));
	m_readout.UpdateWindow();
	CString report;
	CGDIGenerator_MuriReadSinkInfo(net, ip, com, port, report);
	m_readout.SetWindowText(report);
}

void CMuriEdidDlg::OnRefresh() { LoadEdid(); }

void CMuriEdidDlg::OnCopy()
{
	CString t; m_readout.GetWindowText(t);
	if (t.IsEmpty() || !OpenClipboard()) return;
	EmptyClipboard();
	int cb = (t.GetLength() + 1) * sizeof(TCHAR);
	HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, cb);
	if (h) { void* p = GlobalLock(h); if (p) { memcpy(p, (LPCTSTR)t, cb); GlobalUnlock(h); SetClipboardData(sizeof(TCHAR) == 2 ? CF_UNICODETEXT : CF_TEXT, h); } }
	CloseClipboard();
}

void CMuriEdidDlg::OnClose2() { EndDialog(IDOK); }

// ---- DVDO AVLab TPG settings dialog (mirrors CMuriSettingsDlg) ---------------
CDvdoSettingsDlg::CDvdoSettingsDlg(CWnd* pParent) : CDialog(CDvdoSettingsDlg::IDD, pParent) {}

BEGIN_MESSAGE_MAP(CDvdoSettingsDlg, CDialog)
	ON_BN_CLICKED(IDC_DVDO_DLG_TEST, OnTest)
	ON_BN_CLICKED(IDC_DVDO_DLG_APPLY, OnApply)
	ON_BN_CLICKED(IDC_DVDO_DLG_CLOSE, OnClose2)
END_MESSAGE_MAP()

void CDvdoSettingsDlg::PopulateComPorts()
{
	CString current = GetConfig()->GetProfileString("GDIGenerator","DvdoComPort","");
	m_comCombo.ResetContent();
	HKEY hKey;
	if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &hKey))
	{
		char name[256], val[256]; DWORD idx = 0, cbN = sizeof(name), cbV = sizeof(val), type;
		while (ERROR_SUCCESS == RegEnumValue(hKey, idx, name, &cbN, NULL, &type, (LPBYTE)val, &cbV))
		{
			if (type == REG_SZ && _strnicmp(val, "COM", 3) == 0 && m_comCombo.FindStringExact(-1, val) == CB_ERR) m_comCombo.AddString(val);
			idx++; cbN = sizeof(name); cbV = sizeof(val);
		}
		RegCloseKey(hKey);
	}
	if (!current.IsEmpty() && m_comCombo.FindStringExact(-1, current) == CB_ERR) m_comCombo.AddString(current);
	int sel = current.IsEmpty() ? CB_ERR : m_comCombo.FindStringExact(-1, current);
	if (sel != CB_ERR) m_comCombo.SetCurSel(sel); else if (m_comCombo.GetCount() > 0) m_comCombo.SetCurSel(0);
}

BOOL CDvdoSettingsDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	SetWindowText(LS(IDS_GEN_DVDO_SETTINGS_TITLE));
	DlgMap M; M.h = GetSafeHwnd();
	CFont* font = GetParent() ? GetParent()->GetFont() : GetFont();
	const int LX = 8, LW = 74, CX = 86, CW = 128;
	int y = 8;
	#define DK_LBL(ctl,txt) { CPoint p = M.at(LX, y + 2); ctl.Create(txt, WS_CHILD | WS_VISIBLE, CRect(p.x, p.y, p.x + M.w(LW), p.y + M.ht(9)), this); ctl.SetFont(font); }
	#define DK_CB(ctl,id,h)  { CPoint p = M.at(CX, y); ctl.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, CRect(p.x, p.y, p.x + M.w(CW), p.y + M.ht(h)), this, id); ctl.SetFont(font); }

	DK_LBL(m_lblCom, LS(IDS_GEN_COM_PORT));
	{ CPoint p = M.at(CX, y); m_comCombo.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWN | WS_VSCROLL, CRect(p.x, p.y, p.x + M.w(CW), p.y + M.ht(120)), this, IDC_DVDO_DLG_COM); m_comCombo.SetFont(font); }
	y += 16;
	DK_LBL(m_lblRes, LS(IDS_GEN_RESOLUTION)); DK_CB(m_resCombo, IDC_DVDO_DLG_RES, 220);
	for (int i = 0; i < CGDIGenerator_DvdoFmtCount(); ++i) m_resCombo.AddString(CString(CGDIGenerator_DvdoFmtName(i)));
	y += 15;
	DK_LBL(m_lblFmt, LS(IDS_GEN_COLOR_FORMAT)); DK_CB(m_fmtCombo, IDC_DVDO_DLG_FMT, 100);
	m_fmtCombo.AddString(_T("RGB")); m_fmtCombo.AddString(_T("YCbCr 4:4:4")); m_fmtCombo.AddString(_T("YCbCr 4:2:2"));
	y += 15;
	DK_LBL(m_lblRange, LS(IDS_GEN_SIGNAL_RANGE)); DK_CB(m_rangeCombo, IDC_DVDO_DLG_RANGE, 80);
	m_rangeCombo.AddString(_T("Limited (16-235)")); m_rangeCombo.AddString(_T("Full (0-255)"));
	y += 17;
	{ CPoint p = M.at(CX, y); m_testBtn.Create(LS(IDS_GEN_DETECT_TEST), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, CRect(p.x, p.y, p.x + M.w(72), p.y + M.ht(14)), this, IDC_DVDO_DLG_TEST); m_testBtn.SetFont(font); }
	y += 17;
	{ CPoint p = M.at(LX, y + 1); m_status.Create(_T(""), WS_CHILD | WS_VISIBLE | SS_LEFT, CRect(p.x, p.y, p.x + M.w(LW + CW), p.y + M.ht(18)), this); m_status.SetFont(font); }
	y += 24;
	{ CPoint p = M.at(CX, y); m_applyBtn.Create(LS(IDS_GEN_APPLY), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, CRect(p.x, p.y, p.x + M.w(58), p.y + M.ht(14)), this, IDC_DVDO_DLG_APPLY); m_applyBtn.SetFont(font); }
	{ CPoint p = M.at(CX + 66, y); m_closeBtn.Create(LS(IDS_GEN_CLOSE), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, CRect(p.x, p.y, p.x + M.w(58), p.y + M.ht(14)), this, IDC_DVDO_DLG_CLOSE); m_closeBtn.SetFont(font); }
	int bottomY = y + 20;
	#undef DK_LBL
	#undef DK_CB

	if (GetDlgItem(IDOK))     GetDlgItem(IDOK)->ShowWindow(SW_HIDE);
	if (GetDlgItem(IDCANCEL)) GetDlgItem(IDCANCEL)->ShowWindow(SW_HIDE);
	if (GetDlgItem(IDHELP))   GetDlgItem(IDHELP)->ShowWindow(SW_HIDE);
	{
		CRect wr, cr; GetWindowRect(&wr); GetClientRect(&cr);
		int bW = wr.Width() - cr.Width(), bH = wr.Height() - cr.Height();
		SetWindowPos(NULL, 0, 0, M.w(CX + CW + 10) + bW, M.ht(bottomY) + bH, SWP_NOMOVE | SWP_NOZORDER);
		CenterWindow();
	}
	PopulateComPorts();
	m_resCombo.SetCurSel(CGDIGenerator_DvdoFmtIndexForCode(GetConfig()->GetProfileInt("GDIGenerator","DvdoOutputFormat",0)));
	m_fmtCombo.SetCurSel(GetConfig()->GetProfileInt("GDIGenerator","DvdoColorSpace",0));
	m_rangeCombo.SetCurSel(GetConfig()->GetProfileInt("GDIGenerator","RGB_16_235",1) ? 0 : 1);
	return TRUE;
}

void CDvdoSettingsDlg::OnTest()
{
	CString com; m_comCombo.GetWindowText(com); com.Trim();
	if (com.IsEmpty()) { m_status.SetWindowText(LS(IDS_GEN_SELECT_COM_FIRST)); return; }
	int cs = (m_fmtCombo.GetCurSel() >= 0) ? m_fmtCombo.GetCurSel() : 0;
	m_status.SetWindowText(LS(IDS_GEN_CONNECTING));
	CString msg; CGDIGenerator_DvdoTestConnection(com, cs, 0, msg);
	m_status.SetWindowText(msg);
}

void CDvdoSettingsDlg::OnApply()
{
	CString com; m_comCombo.GetWindowText(com); com.Trim();
	if (com.IsEmpty()) { m_status.SetWindowText(LS(IDS_GEN_SELECT_COM_FIRST)); return; }
	int cs  = (m_fmtCombo.GetCurSel() >= 0) ? m_fmtCombo.GetCurSel() : 0;
	int res = (m_resCombo.GetCurSel() >= 0) ? CGDIGenerator_DvdoFmtCode(m_resCombo.GetCurSel()) : 0;
	CString msg; CGDIGenerator_DvdoApplyOutput(com, cs, res, msg);
	m_status.SetWindowText(msg);
}

void CDvdoSettingsDlg::SaveToConfig()
{
	CString com; m_comCombo.GetWindowText(com); com.Trim();
	GetConfig()->WriteProfileString("GDIGenerator","DvdoComPort",com);
	if (m_fmtCombo.GetCurSel() >= 0)   GetConfig()->WriteProfileInt("GDIGenerator","DvdoColorSpace",m_fmtCombo.GetCurSel());
	if (m_resCombo.GetCurSel() >= 0)   GetConfig()->WriteProfileInt("GDIGenerator","DvdoOutputFormat",CGDIGenerator_DvdoFmtCode(m_resCombo.GetCurSel()));
	if (m_rangeCombo.GetCurSel() >= 0) GetConfig()->WriteProfileInt("GDIGenerator","RGB_16_235",(m_rangeCombo.GetCurSel() == 0) ? 1 : 0);
}

void CDvdoSettingsDlg::OnClose2() { SaveToConfig(); EndDialog(IDOK); }

BOOL CGDIGenePropPage::PreTranslateMessage(MSG* pMsg)
{
	if (m_pageTip.GetSafeHwnd()) m_pageTip.RelayEvent(pMsg);
	return CPropertyPageWithHelp::PreTranslateMessage(pMsg);
}

HBRUSH CGDIGenePropPage::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CPropertyPageWithHelp::OnCtlColor(pDC, pWnd, nCtlColor);
	if (pWnd && (pWnd->GetDlgCtrlID() == IDC_PGEN_READOUT || pWnd->GetDlgCtrlID() == IDC_MURI_READOUT || pWnd->GetDlgCtrlID() == IDC_DVDO_READOUT))
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
	// Auto-refresh the status readout after the settings dialog closes (Dominic's request):
	// - a reboot/restart/shutdown (m_action) takes the device down -> show it disconnected;
	// - otherwise refresh only when Apply actually sent a batch (m_applied) - Close/Escape with
	//   no change shouldn't blank the readout for nothing. Apply ends with a daemon restart, so
	//   pass settle=true: the query settles+retries through the restart window instead of
	//   landing on "Not connected".
	if (dlg.m_action != 0)
		ShowPgenDisconnected();
	else if (dlg.m_applied)
		QueryPGenerator(true);
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

	// Capture the previous output range so a change can re-fire the USER Extended-range guard.
	// Default 1 = the canonical RGB_16_235 default: on a fresh install (no INI key) the dialog
	// shows 16-235 from the generator's default, so oldRange must default the same way or a real
	// 16-235 -> Full uncheck would look unchanged and skip the guard.
	BOOL oldRange = GetConfig()->GetProfileInt("GDIGenerator","RGB_16_235",1) ? TRUE : FALSE;
	GetConfig()->WriteProfileInt("GDIGenerator","DisplayMode",m_nDisplayMode);
	GetConfig()->WriteProfileInt("GDIGenerator","RGB_16_235",m_b16_235);
	GetConfig()->WriteProfileInt("GDIGenerator","EnableHDR10",m_bHdr10);
	// The guard otherwise only fires on a CC-mode switch; re-run it here so flipping the
	// output range to Full after loading an Extended USER set warns instead of silently
	// clamping super-white (it no-ops unless CC mode is USER and the set is Extended).
	if ((m_b16_235 ? TRUE : FALSE) != oldRange)
		GuardCCModeOutputRange(GetConfig()->m_CCMode);
	if (m_nDisplayMode == DISPLAY_ccast && m_castHasDevice)
	{
		CString name; m_cCastComboCtrl.GetWindowText(name);
		GetConfig()->WriteProfileInt("GDIGenerator","CCastIp",m_GCast.getCcastIpAddress(m_GCast[(LPCTSTR)name]));
	}

	// DVDO AVLab TPG: output config (COM / resolution / colour format / range) is owned by the
	// "DVDO settings..." dialog; here we only mirror the range flag from what that dialog saved,
	// and persist the pattern selected in the main-panel picker.
	if (m_nDisplayMode == DISPLAY_DVDO)
		m_b16_235 = GetConfig()->GetProfileInt("GDIGenerator","RGB_16_235", m_b16_235);
	if (m_dvdoPatCatCombo.GetSafeHwnd() && m_dvdoPatCombo.GetSafeHwnd() && m_dvdoPatCatCombo.GetCurSel() >= 0 && m_dvdoPatCombo.GetCurSel() >= 0)
		GetConfig()->WriteProfileInt("GDIGenerator","DvdoPatternCode",CGDIGenerator_DvdoPatCode(m_dvdoPatCatCombo.GetCurSel(), m_dvdoPatCombo.GetCurSel()));

	// Murideo Seven-G: output config (IP/transport/resolution/colour space) is saved by
	// the Settings dialog; here we only persist the selected pattern. If Murideo is the
	// active mode, mirror the range flag from the colour space the dialog stored.
	if (m_nDisplayMode == DISPLAY_MURIDEO)
		m_b16_235 = (GetConfig()->GetProfileInt("GDIGenerator","MuriColorSpaceId",0) == 0) ? FALSE : TRUE;
	if (m_muriPatGrpCombo.GetSafeHwnd() && m_muriPatCombo.GetSafeHwnd() && m_muriPatGrpCombo.GetCurSel() >= 0 && m_muriPatCombo.GetCurSel() >= 0)
	{
		int pg = m_muriPatGrpCombo.GetCurSel(), pi = m_muriPatCombo.GetCurSel();
		GetConfig()->WriteProfileInt("GDIGenerator","MuriPatternId", CGDIGenerator_MuriPatId(pg, pi));
		GetConfig()->WriteProfileInt("GDIGenerator","MuriPatternBer", CGDIGenerator_MuriPatBer(pg, pi));	// bank, so restore/Show pick the right one (ids collide across banks)
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

// Refill the specific-pattern dropdown for the given category, preserving nothing.
void CGDIGenePropPage::PopulateDvdoPatternCombo(int cat)
{
	if (!m_dvdoPatCombo.GetSafeHwnd()) return;
	m_dvdoPatCombo.ResetContent();
	int n = CGDIGenerator_DvdoPatCountInCat(cat);
	for (int i = 0; i < n; ++i) m_dvdoPatCombo.AddString(CString(CGDIGenerator_DvdoPatName(cat, i)));
	if (n > 0) m_dvdoPatCombo.SetCurSel(0);
}

void CGDIGenePropPage::OnDvdoCatChange()
{
	int cat = m_dvdoPatCatCombo.GetSafeHwnd() ? m_dvdoPatCatCombo.GetCurSel() : 0;
	if (cat < 0) cat = 0;
	PopulateDvdoPatternCombo(cat);
}

// Send the selected built-in pattern (command 80) to the TPG right now.
void CGDIGenePropPage::OnDvdoShow()
{
	CString com = GetConfig()->GetProfileString("GDIGenerator","DvdoComPort","");	// owned by the settings dialog
	if (com.IsEmpty()) { m_dvdoStatus.SetWindowText(LS(IDS_GEN_SET_DVDO_COM_SETTINGS)); return; }
	int cs  = GetConfig()->GetProfileInt("GDIGenerator","DvdoColorSpace",0);
	int fmt = GetConfig()->GetProfileInt("GDIGenerator","DvdoOutputFormat",0);
	int cat = m_dvdoPatCatCombo.GetSafeHwnd() ? m_dvdoPatCatCombo.GetCurSel() : 0; if (cat < 0) cat = 0;
	int pi  = m_dvdoPatCombo.GetSafeHwnd() ? m_dvdoPatCombo.GetCurSel() : 0; if (pi < 0) pi = 0;
	int code = CGDIGenerator_DvdoPatCode(cat, pi);

	m_dvdoStatus.SetWindowText(LS(IDS_GEN_SENDING_PATTERN));
	CString msg;
	CGDIGenerator_DvdoShowPattern(com, cs, fmt, code, msg);
	m_dvdoStatus.SetWindowText(msg);
}

// Turn built-in patterns off: maps to 80=35 (full-black field, keeps the TPG armed) - never
// 80=0, which disarms it; the next AA/AF patch then displays normally.
void CGDIGenePropPage::OnDvdoOff()
{
	CString com = GetConfig()->GetProfileString("GDIGenerator","DvdoComPort","");
	if (com.IsEmpty()) { m_dvdoStatus.SetWindowText(LS(IDS_GEN_SET_DVDO_COM_SETTINGS)); return; }
	int cs  = GetConfig()->GetProfileInt("GDIGenerator","DvdoColorSpace",0);
	int fmt = GetConfig()->GetProfileInt("GDIGenerator","DvdoOutputFormat",0);

	CString msg;
	CGDIGenerator_DvdoShowPattern(com, cs, fmt, 0 /*Off*/, msg);
	m_dvdoStatus.SetWindowText(msg);
}

// Live status readout: query the TPG (name/firmware/resolution) and show it PGenerator-style.
// Threaded DVDO status query (mirrors QueryPGenerator / PgenQueryThread) so the serial
// round-trip never freezes the UI. Inputs are captured on the UI thread; the worker calls
// the (blocking) readout query and posts the formatted text back to OnDvdoQueryDone.
struct DvdoQueryCtx { HWND hwnd; CString com; int cs; BOOL lim; CString text; };

static UINT AFX_CDECL DvdoQueryThread(LPVOID p)
{
	DvdoQueryCtx* c = (DvdoQueryCtx*)p;
	CString ro;
	BOOL ok = CGDIGenerator_DvdoQueryReadout(c->com, c->cs, c->lim != 0, ro) && !ro.IsEmpty();
	c->text = ok ? ro : (ro.IsEmpty() ? LSf(IDS_GEN_NO_RESPONSE_FROM, c->com) : ro);
	if (!(c->hwnd && IsWindow(c->hwnd) && ::PostMessage(c->hwnd, WM_DVDO_QUERY_DONE, 0, (LPARAM)c)))
		delete c;
	return 0;
}

void CGDIGenePropPage::RefreshDvdoStatus()
{
	if (!m_dvdoReadout.GetSafeHwnd()) return;
	if (m_dvdoQuerying) return;		// a query is already in flight (see OnDvdoQueryDone)
	int tabs = 96; m_dvdoReadout.SendMessage(EM_SETTABSTOPS, 1, (LPARAM)&tabs);	// align the value column
	CString com = GetConfig()->GetProfileString("GDIGenerator","DvdoComPort","");	// owned by the settings dialog
	if (com.IsEmpty()) { m_dvdoReadout.SetWindowText(LS(IDS_GEN_DVDO_READOUT_HINT)); return; }
	DvdoQueryCtx* c = new DvdoQueryCtx;
	c->hwnd = GetSafeHwnd();
	c->com  = com;
	c->cs   = GetConfig()->GetProfileInt("GDIGenerator","DvdoColorSpace",0);
	c->lim  = (m_b16_235 != 0);
	m_dvdoQuerying = TRUE;
	m_dvdoReadout.SetWindowText(LS(IDS_GEN_QUERYING));
	// If the worker never starts, clear the guard and free the ctx here - otherwise
	// m_dvdoQuerying stays TRUE (only OnDvdoQueryDone clears it) and every later
	// Refresh returns early, wedging the panel on "Querying...".
	if (!AfxBeginThread(DvdoQueryThread, c))
	{
		m_dvdoQuerying = FALSE; delete c;
		m_dvdoReadout.SetWindowText(LS(IDS_GEN_QUERY_START_FAIL));
	}
	else
	{
		// The worker owns s_dvdoPort until it posts WM_DVDO_QUERY_DONE - lock out the controls
		// that also drive the port so a click can't interleave a write, or close the handle,
		// mid-query. Re-enabled in OnDvdoQueryDone.
		if (m_dvdoShowBtn.GetSafeHwnd())     m_dvdoShowBtn.EnableWindow(FALSE);
		if (m_dvdoOffBtn.GetSafeHwnd())      m_dvdoOffBtn.EnableWindow(FALSE);
		if (m_dvdoSettingsBtn.GetSafeHwnd()) m_dvdoSettingsBtn.EnableWindow(FALSE);
	}
}

LRESULT CGDIGenePropPage::OnDvdoQueryDone(WPARAM, LPARAM lp)
{
	DvdoQueryCtx* c = (DvdoQueryCtx*)lp;
	m_dvdoQuerying = FALSE;
	if (m_dvdoShowBtn.GetSafeHwnd())     m_dvdoShowBtn.EnableWindow(TRUE);		// re-enable the port-sharing controls
	if (m_dvdoOffBtn.GetSafeHwnd())      m_dvdoOffBtn.EnableWindow(TRUE);
	if (m_dvdoSettingsBtn.GetSafeHwnd()) m_dvdoSettingsBtn.EnableWindow(TRUE);
	if (m_dvdoReadout.GetSafeHwnd()) m_dvdoReadout.SetWindowText(c->text);
	delete c;
	return 0;
}

void CGDIGenePropPage::OnDvdoRefresh() { RefreshDvdoStatus(); }

void CGDIGenePropPage::OnDvdoSettings()
{
	CDvdoSettingsDlg dlg(this);
	if (dlg.DoModal() == IDOK)
	{
		// The dialog wrote RGB_16_235 from the chosen range; sync our flag so the main-page
		// stimulus follows, then refresh the readout.
		m_b16_235 = GetConfig()->GetProfileInt("GDIGenerator","RGB_16_235", m_b16_235);
		RefreshDvdoStatus();
	}
}

// ---- Murideo Seven-G handlers ------------------------------------------------
void CGDIGenePropPage::PopulateMuriPatCombo(int grp)
{
	if (!m_muriPatCombo.GetSafeHwnd()) return;
	m_muriPatCombo.ResetContent();
	int n = CGDIGenerator_MuriPatCount(grp);
	for (int i = 0; i < n; ++i) m_muriPatCombo.AddString(CString(CGDIGenerator_MuriPatName(grp, i)));
	if (n > 0) m_muriPatCombo.SetCurSel(0);
}

void CGDIGenePropPage::OnMuriPatGrpChange()
{
	int g = m_muriPatGrpCombo.GetSafeHwnd() ? m_muriPatGrpCombo.GetCurSel() : 0; if (g < 0) g = 0;
	PopulateMuriPatCombo(g);
}

// Transport now lives in the Settings dialog; read it from config so Show/Refresh use it.
void CGDIGenePropPage::MuriXport(bool& useNet, CString& ip, CString& com)
{
	useNet = GetConfig()->GetProfileInt("GDIGenerator","MuriUseNetwork",1) != 0;
	ip = GetConfig()->GetProfileString("GDIGenerator","MuriIp","192.168.1.239");
	com = GetConfig()->GetProfileString("GDIGenerator","MuriComPort","");
}

// Open the Murideo output-settings dialog; on OK reflect the new range + status.
void CGDIGenePropPage::OnMuriSettings()
{
	CMuriSettingsDlg dlg(this);
	if (dlg.DoModal() == IDOK)
	{
		// The dialog wrote RGB_16_235 from the chosen colour space; sync our flag so the
		// main-page stimulus follows, then refresh the readout.
		m_b16_235 = GetConfig()->GetProfileInt("GDIGenerator","RGB_16_235", m_b16_235);
		// Bit-depth may have changed: recompute the page's 10-bit-levels flag so the
		// reference grid matches what we'll send (see RefreshUse10bitLevels).
		GetConfig()->RefreshUse10bitLevels();
		RefreshMuriStatus();
	}
}

void CGDIGenePropPage::OnMuriEdid()
{
	CMuriEdidDlg dlg(this);
	dlg.DoModal();
}



// Threaded Murideo status query (mirrors QueryPGenerator) so the HTTP round-trip (up to a
// multi-second timeout) never freezes the UI. Serial mode has no HTTP readback, so it stays
// synchronous/instant. AWAITING HW VALIDATION next session (network path unchanged in effect).
struct MuriQueryCtx { HWND hwnd; bool net; CString ip; CString com; CString text; };

static UINT AFX_CDECL MuriQueryThread(LPVOID p)
{
	MuriQueryCtx* c = (MuriQueryCtx*)p;
	CString ro;
	if (c->net)
	{
		BOOL ok = CGDIGenerator_MuriQueryReadout(c->ip, ro) && !ro.IsEmpty();
		c->text = ok ? (LS(IDS_GEN_IP_ADDRESS) + _T("\t") + c->ip + _T("\r\n") + ro)
		             : LSf(IDS_GEN_NO_RESPONSE_FROM, c->ip);
	}
	else
	{
		BOOL ok = CGDIGenerator_MuriQueryReadoutSerial(c->com, ro) && !ro.IsEmpty();
		c->text = ok ? (LS(IDS_GEN_COM_PORT) + _T("\t") + c->com + _T("\r\n") + ro)
		             : LSf(IDS_GEN_NO_RESPONSE_ON, c->com);
	}
	if (!(c->hwnd && IsWindow(c->hwnd) && ::PostMessage(c->hwnd, WM_MURI_QUERY_DONE, 0, (LPARAM)c)))
		delete c;
	return 0;
}

void CGDIGenePropPage::RefreshMuriStatus()
{
	if (!m_muriReadout.GetSafeHwnd()) return;
	if (m_muriQuerying) return;		// a query is already in flight (see OnMuriQueryDone)
	// Tab stop so the value column aligns (matches the PGenerator readout).
	int tabs = 80; m_muriReadout.SendMessage(EM_SETTABSTOPS, 1, (LPARAM)&tabs);
	bool net; CString ip, com; MuriXport(net, ip, com);
	if (net && ip.IsEmpty())  { m_muriReadout.SetWindowText(LS(IDS_GEN_MURI_IP_HINT)); return; }
	if (!net && com.IsEmpty()) { m_muriReadout.SetWindowText(LS(IDS_GEN_MURI_COM_HINT)); return; }
	MuriQueryCtx* c = new MuriQueryCtx;
	c->hwnd = GetSafeHwnd();
	c->net  = net;
	c->ip   = ip;
	c->com  = com;
	m_muriQuerying = TRUE;
	m_muriReadout.SetWindowText(LS(IDS_GEN_QUERYING));
	// If the worker never starts, clear the guard and free the ctx here - otherwise
	// m_muriQuerying stays TRUE (only OnMuriQueryDone clears it) and every later
	// Refresh returns early, wedging the panel on "Querying...".
	if (!AfxBeginThread(MuriQueryThread, c))
	{
		m_muriQuerying = FALSE; delete c;
		m_muriReadout.SetWindowText(LS(IDS_GEN_QUERY_START_FAIL));
	}
	else
	{
		// Lock out the controls that also drive s_muriPort (serial) / the device while the
		// worker queries, so a click can't interleave with it. Re-enabled in OnMuriQueryDone.
		if (m_muriShowBtn.GetSafeHwnd())     m_muriShowBtn.EnableWindow(FALSE);
		if (m_muriSettingsBtn.GetSafeHwnd()) m_muriSettingsBtn.EnableWindow(FALSE);
		if (m_muriEdidBtn.GetSafeHwnd())     m_muriEdidBtn.EnableWindow(FALSE);
	}
}

LRESULT CGDIGenePropPage::OnMuriQueryDone(WPARAM, LPARAM lp)
{
	MuriQueryCtx* c = (MuriQueryCtx*)lp;
	m_muriQuerying = FALSE;
	if (m_muriShowBtn.GetSafeHwnd())     m_muriShowBtn.EnableWindow(TRUE);		// re-enable the port-sharing controls
	if (m_muriSettingsBtn.GetSafeHwnd()) m_muriSettingsBtn.EnableWindow(TRUE);
	if (m_muriEdidBtn.GetSafeHwnd())     m_muriEdidBtn.EnableWindow(TRUE);
	if (m_muriReadout.GetSafeHwnd()) m_muriReadout.SetWindowText(c->text);
	delete c;
	return 0;
}

void CGDIGenePropPage::OnMuriRefresh() { RefreshMuriStatus(); }

void CGDIGenePropPage::OnMuriShow()
{
	bool net; CString ip, com; MuriXport(net, ip, com);
	if (net && ip.IsEmpty()) { m_muriStatus.SetWindowText(LS(IDS_GEN_ENTER_MURI_IP_FIRST)); return; }
	int pg = m_muriPatGrpCombo.GetSafeHwnd() ? m_muriPatGrpCombo.GetCurSel() : 0; if (pg < 0) pg = 0;
	int pi = m_muriPatCombo.GetSafeHwnd() ? m_muriPatCombo.GetCurSel() : 0; if (pi < 0) pi = 0;
	int patternId = CGDIGenerator_MuriPatId(pg, pi);
	int patternBer = CGDIGenerator_MuriPatBer(pg, pi);
	CString msg;
	bool ok = CGDIGenerator_MuriShowPattern(net, ip, com, patternId, patternBer, msg);
	if (ok) { CString pn; m_muriPatCombo.GetLBText(pi, pn); m_muriStatus.SetWindowText(LS(IDS_GEN_SHOWING) + pn); }
	else m_muriStatus.SetWindowText(msg);		// show the HTTP diagnostic only on failure
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
