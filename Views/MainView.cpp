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

// MainView.cpp : implementation of the CMainView class
//
#include "stdafx.h"
#include "ColorHCFR.h"

#include "DataSetDoc.h"
#include "DocTempl.h"
#include "MainView.h"
#include "../Tools/NewMenu/PngIconLoader.h"
#include "Tools/GridCtrl/GridCtrl.h"
#include "MainFrm.h"
#include "MultiFrm.h"
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#include "GDIGenerator.h"
#include "LuminanceHistoView.h"
#include "NearBlackHistoView.h"
#include "NearWhiteHistoView.h"
#include "GammaHistoView.h"
#include "RGBHistoView.h"
#include "ColorTempHistoView.h"
#include "CIEChartView.h"
#include "Color3DView.h"
#include "MeasuresHistoView.h"
#include "SatLumHistoView.h"
#include "SatLumShiftView.h"
#include "SpectrumDlg.h"
#include "../ColorHCFRConfig.h"
#include "../Measure.h"

#include "DocEnumerator.h"	//Ki
#include "../ScaleSizes.h"
#include <math.h>
#include <algorithm>
#include <vector>
#include <afxpriv.h>
#include "EditEx.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define WM_SET_USER_INFO_POST_INIT	WM_USER+99

#define		LAYOUT_LEFT			0
#define		LAYOUT_RIGHT		1
#define		LAYOUT_TOP			0
#define		LAYOUT_BOTTOM		1
#define		LAYOUT_TOP_OFFSET	2

struct SCtrlLayout
{
	int		m_nCtrlID;
	int		m_LeftMode;
	int		m_RightMode;
	int		m_TopMode;
	int		m_BottomMode;
};

struct SCtrlInitPos
{
 public:
	HWND				m_hWnd;
	RECT				m_Rect;
	const SCtrlLayout *	m_pLayout;
};

static const SCtrlLayout g_CtrlLayout [] = {

{ IDC_PARAM_GROUP						, LAYOUT_LEFT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_GRAYSCALESTEPS_COMBOMODE			, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_EDITGRID_CHECK					, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_SPIN_VIEW							, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_GRAYSCALE_GROUP					, LAYOUT_LEFT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP_OFFSET	}, 
{ IDC_VALUES_STATIC						, LAYOUT_LEFT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP_OFFSET	}, 
{ IDC_GRAYSCALE_GRID					, LAYOUT_LEFT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP_OFFSET	}, 
{ IDC_MEASUREGRAYSCALE_BUTTON			, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_DELETEGRAYSCALE_BUTTON			, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
																												
{ IDC_DISPLAY_GROUP						, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP	},
{ IDC_SENSORRGB_RADIO					, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP	},
{ IDC_RGB_RADIO							, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP	},
{ IDC_XYZ_RADIO							, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP	},
{ IDC_XYZ_RADIO2						, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP	},
{ IDC_XYY_RADIO 						, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP	},
																												
{ IDC_SENSOR_GROUP						, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_SENSORNAME_STATIC					, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_SENSORNAME_STATIC2				, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDM_CONFIGURE_SENSOR					, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDM_CONFIGURE_SENSOR2					, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_AVG_LOW_LIGHT, LAYOUT_RIGHT, LAYOUT_RIGHT, LAYOUT_TOP, LAYOUT_TOP },
{ IDC_GENERATOR_GROUP					, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_GENERATORNAME_STATIC				, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDM_CONFIGURE_GENERATOR				, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_DATAREF_GROUP						, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_DATAREF_CHECK						, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_ADJUSTXYZ_CHECK					, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
																												
{ IDC_SELECTION_GROUP					, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP_OFFSET,	LAYOUT_BOTTOM		},
{ IDC_COLORDATA_STATIC					, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP_OFFSET,	LAYOUT_BOTTOM		},
{ IDC_COLORDATA_GRID					, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP_OFFSET,	LAYOUT_BOTTOM		},
{ IDC_VIEW_GROUP 						, LAYOUT_LEFT,	LAYOUT_RIGHT,	LAYOUT_TOP_OFFSET,	LAYOUT_BOTTOM		},
{ IDC_INFO_DISPLAY						, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP_OFFSET,	LAYOUT_TOP_OFFSET	},
{ IDC_STATIC_VIEW						, LAYOUT_LEFT,	LAYOUT_RIGHT,	LAYOUT_TOP_OFFSET,	LAYOUT_BOTTOM		},
{ IDC_STATIC_RGBLEVELS					, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP_OFFSET,	LAYOUT_TOP_OFFSET	},
{ IDC_RGBLEVELS							, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP_OFFSET,	LAYOUT_TOP_OFFSET	},
{ IDC_STATIC_TARGET						, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP_OFFSET,	LAYOUT_TOP_OFFSET	},
{ IDC_TARGET							, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP_OFFSET,	LAYOUT_TOP_OFFSET	},
{ IDC_STATIC_DATA						, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP_OFFSET,	LAYOUT_TOP_OFFSET	},
{ IDC_RGBLEVELS2						, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP_OFFSET,	LAYOUT_TOP_OFFSET	},
{ IDC_INFOLINE							, LAYOUT_LEFT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_TARGET2							, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP_OFFSET,	LAYOUT_TOP_OFFSET	},
{ IDC_CCOMP								, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP_OFFSET,	LAYOUT_TOP_OFFSET	},
{ IDC_CCOMP3							, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP_OFFSET,	LAYOUT_TOP_OFFSET	},
{ IDC_ANSICONTRAST_PATTERN_TEST_BUTTON  , LAYOUT_RIGHT, LAYOUT_RIGHT,	LAYOUT_TOP_OFFSET,	LAYOUT_TOP_OFFSET	},
{ IDC_REFS_BUTTON						, LAYOUT_RIGHT, LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			}

};

static const SCtrlLayout g_StatsBarLayout =
{ 0, LAYOUT_LEFT, LAYOUT_RIGHT, LAYOUT_TOP, LAYOUT_TOP };

static const SCtrlLayout g_DisplayComboLayout =
{ IDC_DISPLAYTYPE_COMBO, LAYOUT_RIGHT, LAYOUT_RIGHT, LAYOUT_TOP, LAYOUT_TOP };

static const SCtrlLayout g_3DDEFilterLayout =
{ IDC_3DVIEW_DE_FILTER, LAYOUT_LEFT, LAYOUT_LEFT, LAYOUT_TOP_OFFSET, LAYOUT_TOP_OFFSET };
static const SCtrlLayout g_ProfilePaneLayout =
{ IDC_PROFILE_PANE, LAYOUT_LEFT, LAYOUT_RIGHT, LAYOUT_TOP, LAYOUT_TOP_OFFSET };
static const SCtrlLayout g_ParamComboLayout =
{ IDC_PARAMSTEPS_COMBO, LAYOUT_LEFT, LAYOUT_LEFT, LAYOUT_TOP, LAYOUT_TOP };

static const SCtrlLayout g_ActionBtnLayout =
{ IDC_MEASURESATALLLEVELS_BUTTON, LAYOUT_RIGHT, LAYOUT_RIGHT, LAYOUT_TOP, LAYOUT_TOP };

// Sentinel item-data for the stimulus dropdown's command rows (level rows store
// their 1..100 percentage). Selecting a preset rewrites the SatStimLevels list.
enum { STIM_SEP = -1, STIM_QUICK = -2, STIM_STANDARD = -3, STIM_FINE = -4 };


static COLORREF ButtonFaceColor();
static COLORREF ButtonHoverColor();
static COLORREF ButtonBorderColor();

                    char*  PatName[96]={
                    "White",
                    "6J",
                    "5F",
                    "6I",
                    "6K",
                    "5G",
                    "6H",
                    "5H",
                    "7K",
                    "6G",
                    "5I",
                    "6F",
                    "8K",
                    "5J",
                    "Black",
                    "2B",
                    "2C",
                    "2D",
                    "2E",
                    "2F",
                    "2G",
                    "2H",
                    "2I",
                    "2J",
                    "2K",
                    "2L",
                    "2M",
                    "3B",
                    "3C",
                    "3D",
                    "3E",
                    "3F",
                    "3G",
                    "3H",
                    "3I",
                    "3J",
                    "3K",
                    "3L",
                    "3M",
                    "4B",
                    "4C",
                    "4D",
                    "4E",
                    "4F",
                    "4G",
                    "4H",
                    "4I",
                    "4J",
                    "4K",
                    "4L",
                    "4M",
                    "5B",
                    "5C",
                    "5D",
                    "5K",
                    "5L",
                    "5M",
                    "6B",
                    "6C",
                    "6D",
                    "6L",
                    "6M",
                    "7B",
                    "7C",
                    "7D",
                    "7E",
                    "7F",
                    "7G",
                    "7H",
                    "7I",
                    "7J",
                    "7L",
                    "7M",
                    "8B",
                    "8C",
                    "8D",
                    "8E",
                    "8F",
                    "8G",
                    "8H",
                    "8I",
                    "8J",
                    "8L",
                    "8M",
                    "9B",
                    "9C",
                    "9D",
                    "9E",
                    "9F",
                    "9G",
                    "9H",
                    "9I",
                    "9J",
                    "9K",
                    "9L",
                    "9M" };
                    char*  PatNameCMS[19]={
						"White",
						"Black",
						"2E",
						"2F",
						"2K",
						"5D",
						"7E",
						"7F",
						"7G",
						"7H",
						"7I",
						"7J",
						"8D",
						"8E",
						"8F",
						"8G",
						"8H",
						"8I",
						"8J" };
                    char*  PatNameCPS[19]={
						"White",
						"D7",
						"D8",
						"E7",
						"E8",
						"F7",
						"F8",
						"G7",
						"G8",
						"H7",
						"H8",
						"I7",
						"I8",
						"J7",
						"J8",
						"CP-Light",
						"CP-Dark",
						"Dark Skin",
						"Light Skin" };
                    char*  PatNameAXIS[71]={
						"Black",
						"White 10",
						"White 20",
						"White 30",
						"White 40",
						"White 50",
						"White 60",
						"White 70",
						"White 80",
						"White 90",
						"White 100",
						"Red 10",
						"Red 20",
						"Red 30",
						"Red 40",
						"Red 50",
						"Red 60",
						"Red 70",
						"Red 80",
						"Red 90",
						"Red 100",
						"Green 10",
						"Green 20",
						"Green 30",
						"Green 40",
						"Green 50",
						"Green 60",
						"Green 70",
						"Green 80",
						"Green 90",
						"Green 100",
						"Blue 10",
						"Blue 20",
						"Blue 30",
						"Blue 40",
						"Blue 50",
						"Blue 60",
						"Blue 70",
						"Blue 80",
						"Blue 90",
						"Blue 100", 
						"Cyan 10",
						"Cyan 20",
						"Cyan 30",
						"Cyan 40",
						"Cyan 50",
						"Cyan 60",
						"Cyan 70",
						"Cyan 80",
						"Cyan 90",
						"Cyan 100", 
						"Magenta 10",
						"Magenta 20",
						"Magenta 30",
						"Magenta 40",
						"Magenta 50",
						"Magenta 60",
						"Magenta 70",
						"Magenta 80",
						"Magenta 90",
						"Magenta 100", 
						"Yellow 10",
						"Yellow 20",
						"Yellow 30",
						"Yellow 40",
						"Yellow 50",
						"Yellow 60",
						"Yellow 70",
						"Yellow 80",
						"Yellow 90",
						"Yellow 100"
					};


/////////////////////////////////////////////////////////////////////////////
// CMainView

/////////////////////////////////////////////////////////////////////////////
// CCompSwatch - one half of the measured / reference split colour swatch

// Rounded-rectangle path (all four corners).
static void SwatchRoundPath(Gdiplus::GraphicsPath& p, float x, float y, float w, float h, float r)
{
	if ( r * 2.0f > w ) r = w * 0.5f;
	if ( r * 2.0f > h ) r = h * 0.5f;
	if ( r <= 0.0f ) { p.AddRectangle(Gdiplus::RectF(x, y, w, h)); return; }
	p.AddArc(x, y, r*2.0f, r*2.0f, 180.0f, 90.0f);
	p.AddArc(x+w-r*2.0f, y, r*2.0f, r*2.0f, 270.0f, 90.0f);
	p.AddArc(x+w-r*2.0f, y+h-r*2.0f, r*2.0f, r*2.0f, 0.0f, 90.0f);
	p.AddArc(x, y+h-r*2.0f, r*2.0f, r*2.0f, 90.0f, 90.0f);
	p.CloseFigure();
}

BEGIN_MESSAGE_MAP(CCompSwatch, CStatic)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

BOOL CCompSwatch::OnEraseBkgnd(CDC*)
{
	return TRUE;
}

void CCompSwatch::OnPaint()
{
	CPaintDC dc(this);
	CRect rc;
	GetClientRect(&rc);
	if ( rc.Width() <= 0 || rc.Height() <= 0 )
		return;

	int dpiY = dc.GetDeviceCaps(LOGPIXELSY);
	BOOL bDark = GetConfig()->m_darkTheme;
	COLORREF panelBg   = FxGetMenuBgColor();
	COLORREF textClr   = FxGetSysColor(COLOR_WINDOWTEXT);
	COLORREF mutedClr  = bDark ? RGB(148,148,154) : RGB(112,114,120);
	COLORREF borderClr = bDark ? RGB(72,72,78)    : RGB(196,198,204);

	CDC mem;
	mem.CreateCompatibleDC(&dc);
	CBitmap bmp;
	bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
	CBitmap* pOldBmp = mem.SelectObject(&bmp);
	mem.FillSolidRect(&rc, panelBg);

	EnsureGdiplus();
	Gdiplus::Graphics g(mem.GetSafeHdc());
	g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
	g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

	float W   = (float) rc.Width();
	float rad = (float) MulDiv(8, dpiY, 96);

	// Swatch pill (upper half): outer corners rounded, inner edge square, so the
	// measured and reference halves butt together into one split pill.
	float swTop = 0.0f;
	float swH   = ((float) rc.Height() - swTop) * 0.52f;
	Gdiplus::GraphicsPath pill;
	if ( m_side == 0 )
		SwatchRoundPath(pill, 0.5f, swTop, W + rad, swH, rad);
	else
		SwatchRoundPath(pill, -rad, swTop, W - 0.5f + rad, swH, rad);
	COLORREF fill = m_hasColor ? m_fill : panelBg;
	Gdiplus::SolidBrush pillBrush(Gdiplus::Color(255, GetRValue(fill), GetGValue(fill), GetBValue(fill)));
	g.FillPath(&pillBrush, &pill);
	Gdiplus::Pen pillPen(Gdiplus::Color(255, GetRValue(borderClr), GetGValue(borderClr), GetBValue(borderClr)), 1.0f);
	g.DrawPath(&pillPen, &pill);
	// Inset look: 1px white bottom highlight under the pill (matches the RGB tracks).
	Gdiplus::Pen hlPen(bDark ? Gdiplus::Color(26,255,255,255) : Gdiplus::Color(115,255,255,255), 1.0f);
	Gdiplus::REAL hlY = swTop + swH + 1.0f;
	if ( m_side == 0 )
		g.DrawLine(&hlPen, rad, hlY, W, hlY);
	else
		g.DrawLine(&hlPen, 0.0f, hlY, W - rad, hlY);

	// Label and RGB triplet below, aligned to the outer edge of each half so
	// the pair reads left / right across the split.
	float labelPx = (float) MulDiv(11, dpiY, 96);
	float valuePx = (float) MulDiv(12, dpiY, 96);
	Gdiplus::Font labelFont(L"Segoe UI", labelPx, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	Gdiplus::Font valueFont(L"Consolas", valuePx, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	Gdiplus::SolidBrush labelBrush(Gdiplus::Color(255, GetRValue(mutedClr), GetGValue(mutedClr), GetBValue(mutedClr)));
	Gdiplus::SolidBrush valueBrush(Gdiplus::Color(255, GetRValue(textClr), GetGValue(textClr), GetBValue(textClr)));
	Gdiplus::StringFormat fmt(Gdiplus::StringFormatFlagsNoWrap);
	fmt.SetAlignment(m_side == 0 ? Gdiplus::StringAlignmentNear : Gdiplus::StringAlignmentFar);

	float pad = (float) MulDiv(2, dpiY, 96);
	float ty  = swTop + swH + (float) MulDiv(5, dpiY, 96);
	wchar_t lblBuf[64] = L"";
	::GetWindowTextW(GetSafeHwnd(), lblBuf, 64);
	CStringW wlabel(lblBuf);
	g.DrawString(wlabel, -1, &labelFont, Gdiplus::RectF(pad, ty, W - 2.0f*pad, labelPx + 4.0f), &fmt, &labelBrush);
	if ( m_hasColor && !m_value.IsEmpty() )
	{
		CStringW wv(m_value);
		g.DrawString(wv, -1, &valueFont, Gdiplus::RectF(pad, ty + labelPx + (float) MulDiv(3, dpiY, 96), W - 2.0f*pad, valuePx + 4.0f), &fmt, &valueBrush);
	}

	dc.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
	mem.SelectObject(pOldBmp);
}

IMPLEMENT_DYNCREATE(CMainView, CFormView)

#define SIZEMOVE_TIMER_ID 0x5713

BEGIN_MESSAGE_MAP(CMainView, CFormView)
	//{{AFX_MSG_MAP(CMainView)
	ON_BN_CLICKED(IDC_XYZ_RADIO, OnXyzRadio)
	ON_BN_CLICKED(IDC_SENSORRGB_RADIO, OnSensorrgbRadio)
	ON_BN_CLICKED(IDC_RGB_RADIO, OnRgbRadio)
	ON_BN_CLICKED(IDC_XYZ_RADIO2, OnXyz2Radio)
	ON_BN_CLICKED(IDC_XYY_RADIO, OnxyYRadio)
	ON_CBN_SELCHANGE(IDC_GRAYSCALESTEPS_COMBOMODE, OnSelchangeComboMode)
	ON_CBN_DROPDOWN(IDC_GRAYSCALESTEPS_COMBOMODE, OnDropdownComboMode)
	ON_BN_CLICKED(IDC_EDITGRID_CHECK, OnEditgridCheck)
	ON_BN_CLICKED(IDC_DATAREF_CHECK, OnDatarefCheck)
	ON_BN_CLICKED(IDC_ADJUSTXYZ_CHECK, OnAdjustXYZCheck)
	ON_BN_CLICKED(IDC_AVG_LOW_LIGHT, OnAvgLowLightCheck)
	ON_BN_CLICKED(IDC_INIT_BUTTON, OnInitDefaults)
	ON_WM_ERASEBKGND()
	ON_WM_SYSCOLORCHANGE()
	ON_WM_CTLCOLOR()
	ON_BN_CLICKED(IDC_MEASUREGRAYSCALE_BUTTON, OnMeasureGrayScale)
	ON_BN_CLICKED(IDC_DELETEGRAYSCALE_BUTTON, OnDeleteGrayscale)
	ON_WM_SIZE()
	ON_WM_ENTERSIZEMOVE()
	ON_WM_EXITSIZEMOVE()
	ON_WM_TIMER()
	ON_CBN_SELCHANGE(IDC_INFO_DISPLAY, OnSelchangeInfoDisplay)
	ON_BN_CLICKED(IDC_3DVIEW_DE_FILTER, On3DDEFilterClicked)
	ON_BN_CLICKED(IDC_PROFILE_PANE, OnProfilePaneAction)
	ON_CBN_SELCHANGE(IDC_DISPLAYTYPE_COMBO, OnSelchangeDisplayType)
	ON_CBN_SELCHANGE(IDC_PARAMSTEPS_COMBO, OnSelchangeComboSteps)
	ON_CBN_SELCHANGE(IDC_STIMLEVEL_COMBO, OnSelchangeComboStimLevel)
	ON_BN_CLICKED(IDC_SIZE_PLUS, OnSizePlus)
	ON_BN_CLICKED(IDC_SIZE_MINUS, OnSizeMinus)
	ON_COMMAND(IDM_HELP, OnHelp)
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
	ON_COMMAND(ID_EDIT_CUT, OnEditCut)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateEditCut)
	ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdateEditPaste)
	ON_COMMAND(ID_EDIT_UNDO, OnEditUndo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, OnUpdateEditUndo)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_VIEW, OnDeltaposSpinView)
	ON_BN_CLICKED(IDC_ANSICONTRAST_PATTERN_TEST_BUTTON, OnAnsiContrastPatternTestButton)
	ON_BN_CLICKED(IDC_REFS_BUTTON, OnRefs)
	ON_BN_CLICKED(IDC_MEASURESATALLLEVELS_BUTTON, OnMeasureSatColorAllLevels)
	ON_EN_CHANGE(IDC_INFO_VIEW, OnChangeInfosEdit)
	//}}AFX_MSG_MAP
	// Standard printing commands
	ON_COMMAND(ID_FILE_PRINT, CFormView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_DIRECT, CFormView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_PREVIEW, CFormView::OnFilePrintPreview)


	ON_NOTIFY(GVN_BEGINLABELEDIT, IDC_GRAYSCALE_GRID, OnGrayScaleGridBeginEdit)
	ON_NOTIFY(GVN_ENDLABELEDIT, IDC_GRAYSCALE_GRID, OnGrayScaleGridEndEdit)
	ON_NOTIFY(GVN_SELCHANGED, IDC_GRAYSCALE_GRID, OnGrayScaleGridEndSelChange)
	ON_MESSAGE(WM_SET_USER_INFO_POST_INIT, OnSetUserInfoPostInitialUpdate)
	ON_MESSAGE(WM_CTLCOLORSTATIC, OnCtlColorStatic)
END_MESSAGE_MAP()


// Tool functions and variables implemented in DataSetDoc.cpp
BOOL StartBackgroundMeasures ( CDataSetDoc * pDoc );
void StopBackgroundMeasures ();
BOOL IsAllLevelsSweepActive ();
extern CDataSetDoc *	g_pDataDocRunningThread;
extern BOOL				g_bTerminateThread;
extern CWinThread*			g_hThread;
//CColorReference m_bRef(HDTV);
/////////////////////////////////////////////////////////////////////////////
// CMainView construction/destruction

static COLORREF GridBk(COLORREF c)
{
	if (!GetConfig()->m_darkTheme) return c;
	return RGB(GetRValue(c)/4, GetGValue(c)/4, GetBValue(c)/4);
}
static COLORREF GridFg(COLORREF c)
{
	if (!GetConfig()->m_darkTheme) return c;
	if ((GetRValue(c)*30 + GetGValue(c)*59 + GetBValue(c)*11)/100 < 128) return RGB(235,235,235);
	return c;
}

CMainView::CMainView()
	: CFormView(CMainView::IDD)
{
    //{{AFX_DATA_INIT(CMainView)
	m_datarefCheckButton = FALSE;
	//}}AFX_DATA_INIT
	binfoRedraw = false;
	m_displayMode = 0;
	m_infoDisplay = 5;
	m_nSizeOffset = 0;
	m_bPositionsInit = FALSE;
	m_bInSizeMove = FALSE;
	m_dwInitialUserInfo = 0;
	last_minCol = 4;
	minCol = 4;
	
	m_SelectedColor = noDataColor;
	m_LastColor = m_SelectedColor;
	m_RefColor = m_SelectedColor;
	m_lastRefColor = m_SelectedColor;;
	m_bUpdate = TRUE;

	m_pGrayScaleGrid = NULL;
	m_pSelectedColorGrid = NULL;
	m_nSelColorGridReadingType = -1;
	m_pBgBrush= new CBrush(FxGetMenuBgColor());
	m_rcButtonPanel.SetRectEmpty();

	m_pInfoWnd = NULL;
	m_pInfoWnd2 = NULL;
	m_pInfoWnd3 = NULL;
	m_pInfoWnd4 = NULL;
	m_pInfoWnd5 = NULL;
	m_pInfoWnd6 = NULL;
	m_pInfoWnd7 = NULL;
	m_pInfoWnd8 = NULL;
	m_pInfoWnd9 = NULL;
	m_pInfoWnd10 = NULL;
	m_pInfoWnd11 = NULL;
	m_pInfoWnd12 = NULL;
	m_pInfoWnd13 = NULL;

	m_displayType=GetConfig()->GetProfileInt("MainView","Display type",HCFR_xyY_VIEW);
	dEavg_gs=0;
	dEmax_gs=0;
	dEavg_cc=0;
	dEmax_cc=0;
	dEavg_sr=0;
	dEmax_sr=0;
	dEavg_sg=0;
	dEmax_sg=0;
	dEavg_sb=0;
	dEmax_sb=0;
	dEavg_sy=0;
	dEmax_sy=0;
	dEavg_sc=0;
	dEmax_sc=0;
	dEavg_sm=0;
	dEmax_sm=0;
	dEavg=0.;
	dLavg=0.;
	dCavg=0.;
	dHavg=0.;
	dEmax=0.;
	dEcnt=0;
	dE10=0.;
	dE10min=0.;
	m_nGridIncrCol = -1;
	m_nGridLastRTCol = -1;
	m_userBlack = FALSE;
	m_oldBlackGS = noDataColor;
	m_oldBlackNB = noDataColor;
	GetConfig()->m_bSave = FALSE;
	GetConfig()->m_bSave2 = FALSE;
	isSelectedWhiteY = FALSE;
	last_Col = 5;
	last_Size = 11;
	last_Display = 0;
	m_YWhite = 100.;
	m_RefWhite = 1.;
	m_ref_r = 0.,m_ref_g = 0.,m_ref_b = 0.;
	m_meas_r = 0.,m_meas_g = 0.,m_meas_b = 0.;
	m_ref_r1 = 0.,m_ref_g1 = 0.,m_ref_b1 = 0.;
	m_meas_r1 = 0.,m_meas_g1 = 0.,m_meas_b1 = 0.;
	refresh = false;
	m_infoLine = "Welcome to HCFR";
}

CMainView::~CMainView()
{
	if(m_pGrayScaleGrid)
		delete m_pGrayScaleGrid;

	if(m_pSelectedColorGrid)
		delete m_pSelectedColorGrid;

	if ( m_pInfoWnd )
		if ( m_infoDisplay < 3 )
			delete m_pInfoWnd;

	delete m_pBgBrush;

	GetConfig()->WriteProfileInt("MainView","Display type",m_displayType);

	POSITION pos = m_CtrlInitPos.GetHeadPosition ();
	while ( pos )
	{
		SCtrlInitPos * pCtrlPos = (SCtrlInitPos *) m_CtrlInitPos.GetNext ( pos );
		delete pCtrlPos;
	}
	m_CtrlInitPos.RemoveAll ();
	line_Font.DeleteObject();
}

void CMainView::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CMainView)
	DDX_Control(pDX, IDC_GRAYSCALESTEPS_COMBOMODE, m_comboMode);
	DDX_Control(pDX, IDC_GRAYSCALE_GROUP, m_grayScaleGroup);
	DDX_Control(pDX, IDC_SENSOR_GROUP, m_sensorGroup);
	DDX_Control(pDX, IDC_GENERATOR_GROUP, m_generatorGroup);
	DDX_Control(pDX, IDC_DATAREF_GROUP, m_datarefGroup);
	DDX_Control(pDX, IDC_DISPLAY_GROUP, m_displayGroup);
	DDX_Control(pDX, IDC_PARAM_GROUP, m_paramGroup);
	DDX_Control(pDX, IDC_SELECTION_GROUP, m_selectGroup);
	DDX_Control(pDX, IDC_VIEW_GROUP, m_viewGroup);
	DDX_Control(pDX, IDC_INFO_DISPLAY, m_comboDisplay);
	DDX_Control(pDX, IDC_EDITGRID_CHECK, m_editCheckButton);
	DDX_Check(pDX, IDC_DATAREF_CHECK, m_datarefCheckButton);
	DDX_Control(pDX, IDC_ADJUSTXYZ_CHECK, m_AdjustXYZCheckButton);
	DDX_Control(pDX, IDC_MEASUREGRAYSCALE_BUTTON, m_grayScaleButton);
	DDX_Control(pDX, IDC_DELETEGRAYSCALE_BUTTON, m_grayScaleDeleteButton);
	DDX_Control(pDX, IDM_CONFIGURE_SENSOR, m_configSensorButton);
	DDX_Control(pDX, IDM_CONFIGURE_GENERATOR, m_configGeneratorButton);
	DDX_Control(pDX, IDC_VALUES_STATIC, m_valuesStatic);
	DDX_Control(pDX, IDC_COLORDATA_STATIC, m_colordataStatic);
	DDX_Text(pDX, IDC_GENERATORNAME_STATIC, m_generatorName);
	DDX_Text(pDX, IDC_SENSORNAME_STATIC, m_sensorName);
	DDX_Control(pDX, IDC_INFOLINE, m_refInfo);
	DDX_Control(pDX, IDC_TARGET, m_TargetStatic);
	DDX_Control(pDX, IDC_CCOMP, m_Ccomp);
	DDX_Control(pDX, IDC_CCOMP3, m_Ccomp3);
	m_Ccomp3.m_side = 1;	// right/reference half of the split swatch
	DDX_Control(pDX, IDC_RGBLEVELS, m_RGBLevelsStatic);
	DDX_Control(pDX, IDC_STATIC_RGBLEVELS, m_RGBLevelsLabel);
	DDX_Control(pDX, IDC_ANSICONTRAST_PATTERN_TEST_BUTTON, m_testAnsiPatternButton);
	DDX_Control(pDX, IDC_REFS_BUTTON, m_refs);
	
	//}}AFX_DATA_MAP
}

void CMainView::LayoutTopRow()
	// ---- Deterministic, language-consistent top-row layout -----------------
	// The MainView form template is hand-authored per language, so the top
	// panes (View / Sensor / Generator / Parameters) sit at different
	// positions/widths in each CHCFR21_*.rc. Override the captured positions
	// with one computed layout, derived from the actual form-font line height:
	// Sensor / Generator / Parameters are sized to fit their own (localized)
	// text and right-anchored; "View" is the stretchy filler whose info text
	// wraps. The panes are sized snug to the content, then the whole lower half
	// is shifted up to meet them (no gap).
	{
		SCtrlInitPos *pView=NULL,*pCombo=NULL,*pInfo=NULL,*pSpin=NULL,*pGGrid=NULL;
		SCtrlInitPos *pSGrp=NULL,*pSName=NULL,*pSGear=NULL,*pAvg=NULL;
		SCtrlInitPos *pGGrp=NULL,*pGName=NULL,*pGGear=NULL;
		SCtrlInitPos *pPGrp=NULL,*pRef=NULL,*pXYZ=NULL;
		SCtrlInitPos *pDisp=NULL,*pDispCombo=NULL,*pGo=NULL,*pDel=NULL,*pRefsBtn=NULL,*pAnsi=NULL;
		SCtrlInitPos *pSteps=NULL,*pStim=NULL,*pModeLbl=NULL,*pStepsLbl=NULL,*pStimLbl=NULL,*pSatAll=NULL;
		POSITION lp = m_CtrlInitPos.GetHeadPosition();
		while (lp)
		{
			SCtrlInitPos* e = (SCtrlInitPos*) m_CtrlInitPos.GetNext(lp);
			switch (::GetDlgCtrlID(e->m_hWnd))
			{
			case IDC_PARAM_GROUP:               pView=e;  break;
			case IDC_GRAYSCALESTEPS_COMBOMODE:  pCombo=e; break;
			case IDC_INFOLINE:                  pInfo=e;  break;
			case IDC_SPIN_VIEW:                 pSpin=e;  break;
			case IDC_GRAYSCALE_GROUP:           pGGrid=e; break;
			case IDC_SENSOR_GROUP:              pSGrp=e;  break;
			case IDC_SENSORNAME_STATIC:         pSName=e; break;
			case IDM_CONFIGURE_SENSOR:          pSGear=e; break;
			case IDC_AVG_LOW_LIGHT:             pAvg=e;   break;
			case IDC_GENERATOR_GROUP:           pGGrp=e;  break;
			case IDC_GENERATORNAME_STATIC:      pGName=e; break;
			case IDM_CONFIGURE_GENERATOR:       pGGear=e; break;
			case IDC_DATAREF_GROUP:             pPGrp=e;  break;
			case IDC_DATAREF_CHECK:             pRef=e;   break;
			case IDC_ADJUSTXYZ_CHECK:           pXYZ=e;   break;
			case IDC_DISPLAY_GROUP:                     pDisp=e;      break;
			case IDC_DISPLAYTYPE_COMBO:                 pDispCombo=e; break;
			case IDC_PARAMSTEPS_COMBO:                  pSteps=e;     break;
			case IDC_STIMLEVEL_COMBO:                   pStim=e;      break;
			case IDC_MODE_LABEL:                        pModeLbl=e;   break;
			case IDC_PARAMSTEPS_LABEL:                  pStepsLbl=e;  break;
			case IDC_STIMLEVEL_LABEL:                   pStimLbl=e;   break;
			case IDC_MEASUREGRAYSCALE_BUTTON:           pGo=e;        break;
			case IDC_DELETEGRAYSCALE_BUTTON:            pDel=e;       break;
			case IDC_REFS_BUTTON:                       pRefsBtn=e;   break;
			case IDC_ANSICONTRAST_PATTERN_TEST_BUTTON:  pAnsi=e;      break;
			case IDC_MEASURESATALLLEVELS_BUTTON:        pSatAll=e;    break;
			}
		}
		if (pView && pSGrp && pGGrp && pPGrp && pRef && pXYZ && pAvg && pGGrid)
		{
			CColorHCFRConfig* cfg = GetConfig();
			CClientDC dc(this);
			CFont* pOldF = dc.SelectObject(GetFont());
			CString s;
			CWnd::FromHandle(pRef->m_hWnd)->GetWindowText(s);  int wRef  = dc.GetTextExtent(s, s.GetLength()).cx;
			CWnd::FromHandle(pXYZ->m_hWnd)->GetWindowText(s);  int wXYZ  = dc.GetTextExtent(s, s.GetLength()).cx;
			CWnd::FromHandle(pAvg->m_hWnd)->GetWindowText(s);  int wAvg  = dc.GetTextExtent(s, s.GetLength()).cx;
			CWnd::FromHandle(pPGrp->m_hWnd)->GetWindowText(s); int wPCap = dc.GetTextExtent(s, s.GetLength()).cx;
			CWnd::FromHandle(pSGrp->m_hWnd)->GetWindowText(s); int wSCap = dc.GetTextExtent(s, s.GetLength()).cx;
			CWnd::FromHandle(pGGrp->m_hWnd)->GetWindowText(s); int wGCap = dc.GetTextExtent(s, s.GetLength()).cx;
			int wSName = 0, wGName = 0;
			if (pSName) { CWnd::FromHandle(pSName->m_hWnd)->GetWindowText(s); wSName = dc.GetTextExtent(s, s.GetLength()).cx; }
			if (pGName) { CWnd::FromHandle(pGName->m_hWnd)->GetWindowText(s); wGName = dc.GetTextExtent(s, s.GetLength()).cx; }
			int fh = dc.GetTextExtent(_T("Ag"), 2).cy;     // form-font line height
			dc.SelectObject(pOldF);

			int PAD    = cfg->Scale(5);
			int GAPX   = cfg->Scale(3);
			int GLYPH  = cfg->Scale(16);
			int GEAR   = cfg->Scale(16) + cfg->Scale(3);
			int CAPPAD = cfg->Scale(14);
			int NAMECAP= cfg->Scale(150);
			int GCOL   = GEAR + GAPX;

			int top   = cfg->Scale(2);
			int cap   = fh + cfg->Scale(2);
			int line1 = top + cap + cfg->Scale(1);
			int line2 = line1 + fh + cfg->Scale(3);
			int chkH  = fh + cfg->Scale(2);
			int lblH  = fh;
			int rowH  = (line2 + chkH) - top + cfg->Scale(4);
			int comboVisH = fh + cfg->Scale(8);   // approx closed combo height (for dropped-list sizing)
			// Actual closed height of the mode combo, so the caption row sits just below
			// it without its opaque background overlapping the combo's bottom edge.
			int comboClosedH = comboVisH;
			if (m_comboMode.GetSafeHwnd())
			{
				int itemH = (int) m_comboMode.SendMessage(CB_GETITEMHEIGHT, (WPARAM)(-1), 0);
				if (itemH > 0) comboClosedH = itemH + 2*::GetSystemMetrics(SM_CYEDGE) + cfg->Scale(3);
			}
			int capH = cfg->Scale(12);   // short caption block so its fill clears the pane's bottom border
			int lblRowTop = line1 + comboClosedH;   // caption row snug under the dropdowns
			rowH = max(rowH, (lblRowTop + capH + cfg->Scale(3)) - top);
			int cMid  = (line1 + line2 + chkH) / 2;
			int right = pPGrp->m_Rect.right;

			int wParam = max(wPCap + CAPPAD, max(wRef, wXYZ) + GLYPH + 2*PAD);
			int wMeter = min(wSName + 2*PAD + GCOL, NAMECAP);                          // meter name (capped; ellipsizes)
			int wSens  = max(wSCap + CAPPAD, max(wMeter, wAvg + GLYPH + 2*PAD + GCOL)); // GLYPH = checkbox box allowance // fit the avg label only when it is shown
			int wGen   = max(wGCap + CAPPAD, wGName + 2*PAD + GCOL);     // fit the full generator label (no cap)

			CRect rP(right - wParam, top, right, top + rowH);
			CRect rG(rP.left - GAPX - wGen, top, rP.left - GAPX, top + rowH);
			CRect rS(rG.left - GAPX - wSens, top, rG.left - GAPX, top + rowH);
			CRect rV(pView->m_Rect.left, top, rS.left - GAPX, top + rowH);

			pView->m_Rect = rV;
			pSGrp->m_Rect = rS;
			pGGrp->m_Rect = rG;
			pPGrp->m_Rect = rP;

			pRef->m_Rect = CRect(rP.left + PAD, line1, rP.right - PAD, line1 + chkH);
			pXYZ->m_Rect = CRect(rP.left + PAD, line2, rP.right - PAD, line2 + chkH);

			int sGearX = rS.right - PAD - GEAR;
			if (pSName) pSName->m_Rect = CRect(rS.left + PAD, line1, sGearX - GAPX, line1 + lblH);
			if (pAvg)   pAvg->m_Rect   = CRect(rS.left + PAD, line2, sGearX - GAPX, line2 + chkH);
			if (pSGear) pSGear->m_Rect = CRect(sGearX, cMid - GEAR/2, rS.right - PAD, cMid - GEAR/2 + GEAR);

			int gGearX = rG.right - PAD - GEAR;
			if (pGName) pGName->m_Rect = CRect(rG.left + PAD, cMid - lblH/2, gGearX - GAPX, cMid - lblH/2 + lblH);
			if (pGGear) pGGear->m_Rect = CRect(gGearX, cMid - GEAR/2, rG.right - PAD, cMid - GEAR/2 + GEAR);

			if (pCombo)
			{
				int comboW = cfg->Scale(96);
				{
					// Make the combo a consistent width = the widest mode name in
					// this language, so it never clips whatever item is selected.
					CFont* cpo = dc.SelectObject(GetFont());
					int ccnt = m_comboMode.GetCount();
					for (int cci = 0; cci < ccnt; cci++)
					{
						CString cit; m_comboMode.GetLBText(cci, cit);
						int citw = dc.GetTextExtent(cit, cit.GetLength()).cx + ::GetSystemMetrics(SM_CXVSCROLL) + cfg->Scale(12);
						if (citw > comboW) comboW = citw;
					}
					dc.SelectObject(cpo);
				}
				m_comboMode.ModifyStyleEx(WS_EX_DLGMODALFRAME, 0, SWP_FRAMECHANGED);
				if (m_comboDisplay.GetSafeHwnd()) m_comboDisplay.ModifyStyleEx(WS_EX_DLGMODALFRAME, 0, SWP_FRAMECHANGED);
				int comboH = pCombo->m_Rect.bottom - pCombo->m_Rect.top;
				pCombo->m_Rect = CRect(rV.left + PAD, line1, rV.left + PAD + comboW, line1 + comboH);
				// Per-mode parameter dropdowns sit on line 1, immediately right of the
				// mode combo: steps then (saturation modes) stimulus level. The info
				// text wraps in its own column to the right of them. UpdateParamCombos
				// shows/hides them; OnSelchangeComboMode relayouts so the info column
				// starts after whichever dropdowns the current mode shows. Tall rects so
				// the lists can drop.
				{
					// Window height = the mode combo's closed height (comboH), NOT a tall
					// "dropped" height. The list height is fixed once at creation (rcInit);
					// forcing a tall window on each relayout makes the combo re-collapse
					// and flash a black box below it.
					// Steps combo width by mode: wide for the long CC-set names and
					// grayscale preset names, narrow for the bare step-count numbers.
					int stepsW = (m_displayMode == 11) ? cfg->Scale(210)
							   : (m_displayMode == 0)  ? cfg->Scale(150)
							   :                          cfg->Scale(58);
					int stimW  = cfg->Scale(70);
					int dropX  = rV.left + PAD + comboW + GAPX;   // right of the mode combo
					int stimX  = dropX + stepsW + GAPX;
					if (pSteps) pSteps->m_Rect = CRect(dropX, line1, dropX + stepsW, line1 + comboH);
					if (pStim)  pStim->m_Rect  = CRect(stimX, line1, stimX + stimW, line1 + comboH);
					// captions centered under each dropdown
					if (pModeLbl)  pModeLbl->m_Rect  = CRect(rV.left + PAD, lblRowTop, rV.left + PAD + comboW, lblRowTop + capH);
					if (pStepsLbl) pStepsLbl->m_Rect = CRect(dropX, lblRowTop, dropX + stepsW, lblRowTop + capH);
					if (pStimLbl)  pStimLbl->m_Rect  = CRect(stimX, lblRowTop, stimX + stimW, lblRowTop + capH);
				}
				// (the size spinner is relocated to the stats-bar header, see below)
				if (pInfo)
				{
					CWnd* pi = CWnd::FromHandle(pInfo->m_hWnd);
					pi->ModifyStyle(SS_TYPEMASK | SS_WORDELLIPSIS | SS_SUNKEN, SS_LEFT, SWP_FRAMECHANGED);   // clear ellipsis -> wrap
					// Info text starts right of the mode combo plus whatever parameter
					// dropdowns the current mode shows, then wraps over the pane's two rows.
					bool bWantStim  = ( m_displayMode >= 5 && m_displayMode <= 10 );
					bool bWantSteps = ( m_displayMode == 0 || m_displayMode == 3 || m_displayMode == 4 || m_displayMode == 11 || bWantStim );
					int iStepsW = (m_displayMode == 11) ? cfg->Scale(210)
									: (m_displayMode == 0)  ? cfg->Scale(150)
									:                          cfg->Scale(58);
						int iStimW = cfg->Scale(70);
					int iDropX  = rV.left + PAD + comboW + GAPX;
					int infoX;
					if (bWantStim)        infoX = iDropX + iStepsW + GAPX + iStimW + GAPX + cfg->Scale(4);
					else if (bWantSteps)  infoX = iDropX + iStepsW + GAPX + cfg->Scale(4);
					else                  infoX = rV.left + PAD + comboW + cfg->Scale(8);
					pInfo->m_Rect = CRect(infoX, top + cap, rV.right - PAD, top + rowH - cfg->Scale(2));
				}
			}

			// Display pane: place the runtime dropdown and shrink the group to a single
			// row, then pull the Go / Delete / Refs / ANSI buttons up into the freed
			// space. (The group + buttons shift with the grid band below.)
			if (pDisp && pDispCombo && m_comboDisplayType.GetSafeHwnd())
			{
				int dcapH = fh + cfg->Scale(3);
				int dcmbH = fh + cfg->Scale(8);
				int dx0   = pDisp->m_Rect.left  + cfg->Scale(4);
				int dx1   = pDisp->m_Rect.right - cfg->Scale(4);
				int dcTop = pDisp->m_Rect.top + dcapH;
				pDispCombo->m_Rect = CRect(dx0, dcTop, dx1, dcTop + dcmbH + cfg->Scale(120)); // tall window so the list can drop (closed combo shows one row)
				int dGrpBot = dcTop + dcmbH + cfg->Scale(4);
				pDisp->m_Rect.bottom = dGrpBot;

				int bx0 = pDisp->m_Rect.left  + cfg->Scale(3);
				int bx1 = pDisp->m_Rect.right - cfg->Scale(3);
				int bh  = pGo ? (pGo->m_Rect.bottom - pGo->m_Rect.top) : cfg->Scale(27);
				int bgap = cfg->Scale(4);
				int by  = dGrpBot + cfg->Scale(5);   // +2px: nudge the Go/Delete/Refs container down
				bool bSatMode = ( m_displayMode >= 5 && m_displayMode <= 10 );
				if (pGo)      { pGo->m_Rect  = CRect(bx0, by, bx1, by + bh); by += bh + bgap; }
				// "All levels" sits right under Go (both are measure actions); sat modes only.
				// It just adds a row in the empty space below; the other buttons keep their size.
				if (pSatAll && bSatMode) { pSatAll->m_Rect = CRect(bx0, by, bx1, by + bh); by += bh + bgap; }
				if (pDel)     { pDel->m_Rect = CRect(bx0, by, bx1, by + bh); by += bh + bgap; }
				if (pRefsBtn) pRefsBtn->m_Rect = CRect(bx0, by, bx1, by + bh);
				if (pAnsi)    pAnsi->m_Rect    = CRect(bx0, by, bx1, by + bh);   // shares the Refs slot
			}

			// Shift the whole lower half to align with the top row: UP to close a gap
			// a shorter pane leaves, or DOWN to clear an overlap when the computed band
			// is taller than the template (high DPI/font). Bottom-anchored ones grow/shrink.
			int measTop = pGGrid->m_Rect.top;
			int delta = (top + rowH + cfg->Scale(3)) - measTop;
			if (delta != 0)
			{
				POSITION rp2 = m_CtrlInitPos.GetHeadPosition();
				while (rp2)
				{
					SCtrlInitPos* e2 = (SCtrlInitPos*) m_CtrlInitPos.GetNext(rp2);
					if (e2->m_Rect.top >= measTop - cfg->Scale(2))
					{
						e2->m_Rect.top += delta;
						if (e2->m_pLayout->m_BottomMode != LAYOUT_BOTTOM)
							e2->m_Rect.bottom += delta;
					}
				}
			}
		}
	}

void CMainView::OnInitialUpdate()
{
	CFormView::OnInitialUpdate();	// called first to initialize dialog elements

	GetDlgItem ( IDC_STATIC_VIEW ) -> ShowWindow ( SW_HIDE );

//	m_displayMode = GetConfig()->GetProfileInt("MainView","Chart Display",0);
	m_displayMode = 0;
	m_comboMode.SetCurSel ( m_displayMode );	// Echelle de gris
	
    // doesn't really make sense to see sensor values
	if ( m_displayType == HCFR_SENSORRGB_VIEW )
		m_displayType = HCFR_xyY_VIEW;

	GetDlgItem ( IDC_SENSORRGB_RADIO ) -> EnableWindow ( FALSE );

	CheckDlgButton(IDC_XYZ_RADIO, m_displayType == HCFR_XYZ_VIEW ? BST_CHECKED : BST_UNCHECKED);  
	CheckDlgButton(IDC_SENSORRGB_RADIO, m_displayType == HCFR_SENSORRGB_VIEW ? BST_CHECKED : BST_UNCHECKED);  
	CheckDlgButton(IDC_RGB_RADIO, m_displayType == HCFR_RGB_VIEW ? BST_CHECKED : BST_UNCHECKED);  
	CheckDlgButton(IDC_XYZ_RADIO2, m_displayType == HCFR_xyz2_VIEW ? BST_CHECKED : BST_UNCHECKED);  
	CheckDlgButton(IDC_XYY_RADIO, m_displayType == HCFR_xyY_VIEW ? BST_CHECKED : BST_UNCHECKED);  

	// Set title of view
	CString title;
	title.LoadString(IDS_DATASETVIEW_NAME);
	GetParent()->SetWindowText(GetDocument()->GetTitle() + ": " + title);

    if (m_pGrayScaleGrid == NULL)             
    {
 	    m_pGrayScaleGrid = new CGridCtrl;     // Create the Gridctrl object
        if (!m_pGrayScaleGrid ) return;

		m_pGrayScaleGrid -> SetHScrollAlwaysVisible ( TRUE );
		m_pGrayScaleGrid -> SetVScrollAlwaysVisible ( TRUE );

		CRect rect;
        m_valuesStatic.GetWindowRect(&rect);	// size control to m_valuesStatic control size
		ScreenToClient(&rect);
        m_pGrayScaleGrid->Create(rect, this, IDC_GRAYSCALE_GRID,WS_CHILD | WS_TABSTOP | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL );		// Create the Gridctrl window
		m_pGrayScaleGrid->ShowScrollBar ( SB_BOTH );
	}

	if (m_pSelectedColorGrid == NULL)
	{
 	    m_pSelectedColorGrid = new CGridCtrl;     // Create the Gridctrl object
        if (!m_pSelectedColorGrid ) return;

		m_pSelectedColorGrid -> SetVScrollAlwaysVisible ( TRUE );

		CRect rect;
        m_colordataStatic.GetWindowRect(&rect);	// size control to m_colordataStatic control size
		ScreenToClient(&rect);
		rect.OffsetRect(2, 0);   // fine-tune: shift the Current Measure grid 2px right
		rect.bottom += 2;         // and make it 2px taller
        m_pSelectedColorGrid->Create(rect, this, IDC_COLORDATA_GRID,WS_CHILD | WS_TABSTOP | WS_VISIBLE | WS_VSCROLL);
		m_pSelectedColorGrid->ModifyStyleEx(WS_EX_CLIENTEDGE, 0, SWP_FRAMECHANGED);   // flat grid, no sunken 3D border		// Create the Gridctrl window
	}

	InitButtons();
	InitGroups();
	if(AfxGetMainWnd()) FxApplyDarkModeTree(AfxGetMainWnd()->GetSafeHwnd(), GetConfig()->m_darkTheme);

	if (m_displayMode != 0)
		OnSelchangeComboMode();

	RECT	Rect;

	if ( m_Target.m_hWnd == NULL )
	{
		m_TargetStatic.GetWindowRect ( & Rect );
		m_TargetStatic.ShowWindow ( SW_HIDE );
		ScreenToClient ( & Rect );
		m_Target.Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this, IDC_TARGET2, NULL );
		m_Target.m_pRefColor = & m_SelectedColor;
		m_Target.m_pDocument = GetDocument();

		m_RGBLevelsStatic.GetWindowRect ( & Rect );
		m_RGBLevelsStatic.ShowWindow ( SW_HIDE );
		ScreenToClient ( & Rect );
		m_RGBLevels.Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this, IDC_RGBLEVELS2, NULL );
		m_RGBLevels.m_pRefColor = & m_SelectedColor;
		m_RGBLevels.m_pDocument = GetDocument();
	}

//	RefreshSelection ();
	GetParentFrame()->RecalcLayout(); 
	
	// Do not resize parent window when opening a document containing window positions
	if ( ! GetDocument () -> m_pFramePosInfo )
		ResizeParentToFit();

	SetScrollSizes(MM_TEXT, CSize(0, 0));

	// Initialise controls positions
	m_InitialWindowSize.x = 0;
	m_InitialWindowSize.y = 0;
	for ( int i = 0; i < sizeof ( g_CtrlLayout ) / sizeof ( g_CtrlLayout [ 0 ] ) ; i ++ )
	{
		HWND			hWnd;
		SCtrlInitPos *	pCtrlPos;

		GetDlgItem ( g_CtrlLayout [ i ].m_nCtrlID, & hWnd );
		if ( hWnd )
		{
			pCtrlPos = new SCtrlInitPos;
			pCtrlPos -> m_hWnd = hWnd;
			pCtrlPos -> m_pLayout = & g_CtrlLayout [ i ];
			::GetWindowRect ( hWnd, & pCtrlPos -> m_Rect );
			::ScreenToClient ( m_hWnd, (LPPOINT) & pCtrlPos -> m_Rect.left );
			::ScreenToClient ( m_hWnd, (LPPOINT) & pCtrlPos -> m_Rect.right );
			m_CtrlInitPos.AddTail ( pCtrlPos );

			if ( m_InitialWindowSize.x < pCtrlPos -> m_Rect.right + 3 )
				m_InitialWindowSize.x = pCtrlPos -> m_Rect.right + 3;

			if ( m_InitialWindowSize.y < pCtrlPos -> m_Rect.bottom + 3 )
				m_InitialWindowSize.y = pCtrlPos -> m_Rect.bottom + 3;
		}
	}
	
	m_OriginalRect.left = 0;
	m_OriginalRect.top = 0;
	m_OriginalRect.right = m_InitialWindowSize.x;
	m_OriginalRect.bottom = m_InitialWindowSize.y;
	m_bPositionsInit = TRUE;

	// Widen the Display pane + Go/Delete/Refs buttons by 12px and take the same
	// 12px off the right of the measures (data-grid) pane, so the gap between the
	// two is preserved. Applied once to the captured (per-language) rects so it is
	// idempotent across LayoutTopRow re-runs; the LAYOUT_LEFT/RIGHT anchors then
	// keep it consistent at every window size and in every localized template.
	{
		int dwWiden = GetConfig()->Scale(12);
		int grpR = 0;
		POSITION wp = m_CtrlInitPos.GetHeadPosition();
		while (wp)
		{
			SCtrlInitPos* e = (SCtrlInitPos*) m_CtrlInitPos.GetNext(wp);
			switch (::GetDlgCtrlID(e->m_hWnd))
			{
			case IDC_GRAYSCALE_GROUP:
				e->m_Rect.right -= dwWiden;   // measures pane gives 12px back from its right edge
				grpR = e->m_Rect.right;       // remember the new right edge for the grid below
				break;
			case IDC_VALUES_STATIC:
			case IDC_GRAYSCALE_GRID:
				if ( grpR > 0 ) e->m_Rect.right = grpR - GetConfig()->Scale(2);   // stretch the grid flush to the pane's right edge (removes the ~11px gap in English)
				break;
			case IDC_DISPLAY_GROUP:
				e->m_Rect.left  -= dwWiden;   // Display pane + buttons grow 12px to the left
				break;
			}
		}
	}

	LayoutTopRow();


	{
		CClientDC sbHdrDC( this );
		int HEADER_H = m_statsBar.PreferredHeight( &sbHdrDC );
		CRect rcGroup( 0, 0, 0, 0 );
		CRect rcGrid( 0, 0, 0, 0 );
		POSITION sbPos = m_CtrlInitPos.GetHeadPosition();
		while ( sbPos )
		{
			SCtrlInitPos * pSb = (SCtrlInitPos *) m_CtrlInitPos.GetNext( sbPos );
			int sbId = ::GetDlgCtrlID( pSb->m_hWnd );
			if ( sbId == IDC_GRAYSCALE_GROUP )
				rcGroup = pSb->m_Rect;
			if ( sbId == IDC_GRAYSCALE_GRID )
				rcGrid = pSb->m_Rect;
		}
		if ( ! rcGroup.IsRectEmpty() && ! rcGrid.IsRectEmpty() )
		{
			int barTop = rcGroup.top + 2;
			int barBottom = barTop + HEADER_H;
			int gridDelta = barBottom - rcGrid.top;
			if ( gridDelta < 0 )
				gridDelta = 0;
			POSITION sbPos2 = m_CtrlInitPos.GetHeadPosition();
			while ( sbPos2 )
			{
				SCtrlInitPos * pSb2 = (SCtrlInitPos *) m_CtrlInitPos.GetNext( sbPos2 );
				int sbId2 = ::GetDlgCtrlID( pSb2->m_hWnd );
				if ( sbId2 == IDC_GRAYSCALE_GRID || sbId2 == IDC_VALUES_STATIC )
					pSb2->m_Rect.top += gridDelta;
			}
			// --- Stats-bar header --------------------------------------------------
			// The [ ] Edit checkbox and the [+]/[-] grid-size buttons are drawn BY THE
			// bar (CStatsBarWnd) at the far right, so they fill the band height like the
			// chips and can never be clipped by a sibling window.
			CRect rcBar( rcGroup.left + 2, barTop, rcGroup.right - 2, barBottom );
			LPCTSTR sbClass = AfxRegisterWndClass( CS_HREDRAW | CS_VREDRAW, ::LoadCursor( NULL, IDC_ARROW ), NULL, NULL );
			m_statsBar.CreateEx( 0, sbClass, _T(""), WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, rcBar, this, 0 );
			SCtrlInitPos * pBarPos = new SCtrlInitPos;
			pBarPos->m_hWnd = m_statsBar.GetSafeHwnd();
			pBarPos->m_Rect.left = rcBar.left;
			pBarPos->m_Rect.top = rcBar.top;
			pBarPos->m_Rect.right = rcBar.right;
			pBarPos->m_Rect.bottom = rcBar.bottom;
			pBarPos->m_pLayout = &g_StatsBarLayout;
			m_CtrlInitPos.AddTail( pBarPos );
			m_grayScaleGroup.InitMeasures( &m_statsBar, _T("") );
			m_statsBar.SetHeaderModel( &m_editCheckButton, &m_fluentFont );
				// The bar overlaps the top of the measures group box. Without sibling clipping the
				// group box repaints over the bar's top on resize/refresh (only a hover restores it).
				m_grayScaleGroup.ModifyStyle( 0, WS_CLIPSIBLINGS );
				m_statsBar.SetWindowPos( &wndTop, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
			{
				POSITION rpE = m_CtrlInitPos.GetHeadPosition();
				while (rpE) { POSITION curE = rpE; SCtrlInitPos* eE = (SCtrlInitPos*) m_CtrlInitPos.GetNext(rpE); if (::GetDlgCtrlID(eE->m_hWnd) == IDC_EDITGRID_CHECK) { delete eE; m_CtrlInitPos.RemoveAt(curE); break; } }
			}
		}
	}
	
	( (CMultiFrame *) GetParentFrame () ) -> m_MinSize2.x = m_InitialWindowSize.x + ( GetSystemMetrics ( SM_CXSIZEFRAME ) * 2 ) - 150;
	( (CMultiFrame *) GetParentFrame () ) -> m_MinSize2.y = m_InitialWindowSize.y + GetSystemMetrics ( SM_CYCAPTION ) + GetSystemMetrics ( SM_CYSIZEFRAME ) + 6 - 400;
	( (CMultiFrame *) GetParentFrame () ) -> m_bUseMinSize2 = TRUE;

	if ( m_dwInitialUserInfo == 0 )
		PostMessage ( WM_SET_USER_INFO_POST_INIT ); // Define initial view status after global initial update

	( (CMultiFrame *) GetParentFrame () )->CMDIChildWnd::MDIMaximize(); //this maximizes and calls onsize

	InitSelectedColorGrid();

	UpdateParamCombos();	// default mode's dropdowns (mode changes re-run this)

	UpdateData(FALSE);
}

LRESULT CMainView::OnSetUserInfoPostInitialUpdate(WPARAM wParam, LPARAM lParam)
{
	// The 3D viewer entry is appended here rather than in the .rc DLGINIT so it
	// appears in every language build without editing each resource; the lookup
	// keeps it idempotent. It must run BEFORE the saved-workspace restore
	// so a layout saved on the 3D viewer can reselect entry 13.
	if ( m_comboDisplay.GetSafeHwnd () )
	{
		CString str3D;
		str3D.LoadString ( IDS_3DVIEW_NAME );
		if ( m_comboDisplay.FindStringExact ( -1, str3D ) == CB_ERR )
			m_comboDisplay.AddString ( str3D );
	}

	// Same code-append trick for the measurement-mode combo: "Display profile"
	// becomes mode 13 in every language build without touching the DLGINIT blobs.
	if ( m_comboMode.GetSafeHwnd () )
	{
		CString strProf;
		strProf.LoadString ( IDS_DISPLAYPROFILE );
		if ( !strProf.IsEmpty () && m_comboMode.FindStringExact ( -1, strProf ) == CB_ERR )
			m_comboMode.AddString ( strProf );
	}

	if ( m_dwInitialUserInfo != 0 )
	{
		// Set m_displayMode
		m_comboMode.SetCurSel ( m_dwInitialUserInfo & 0x003F );
		OnSelchangeComboMode();

		// Set m_displayType
        // this doesn't work and means there is a mismtach between what's shown in the
        // radio buttons and the grid 
        ///\todo fix this
		//SendMessage ( WM_COMMAND, IDC_XYZ_RADIO + ( ( m_dwInitialUserInfo >> 6 ) & 0x000F ) );

		// Set m_infoDisplay
		m_infoDisplay = GetConfig()->GetProfileInt("MainView","Info Display",5);
		m_comboDisplay.SetCurSel ( m_infoDisplay );

		m_comboDisplay.SetCurSel ( ( m_dwInitialUserInfo >> 10 ) & 0x003F );
		OnSelchangeInfoDisplay();

		// Set m_nSizeOffset
		if ( m_nSizeOffset != (signed char) ( ( m_dwInitialUserInfo >> 16 ) & 0x00FF ) )
		{
			m_nSizeOffset = (signed char) ( ( m_dwInitialUserInfo >> 16 ) & 0x00FF );
			( (CMultiFrame *) GetParentFrame () ) -> EnsureMinimumSize ();
			InvalidateRect ( NULL );
			OnSize ( 0, 0, 0 );
		}

		if ( m_infoDisplay >= 3 && m_pInfoWnd != NULL && m_infoDisplay != 11 )
		{
			// Info display is a sub view
			DWORD	dwSubViewUserInfo = ( ( m_dwInitialUserInfo >> 24 ) & 0x00FF );
		
			( (CSavingView *) ( ( (CSubFrame *) m_pInfoWnd ) -> GetActiveView () ) ) -> SetUserInfo ( dwSubViewUserInfo );
		}

		m_dwInitialUserInfo = 0;
	}

	//restore last saved info window
	m_infoDisplay = GetConfig()->GetProfileInt("MainView","Info Display",5);
	m_comboDisplay.SetCurSel ( m_infoDisplay );
	m_bUpdate = FALSE;
	OnSelchangeInfoDisplay();
	m_bUpdate = TRUE;

	return 0;
}

void CMainView::AddColorToGrid(const ColorTriplet& color, GV_ITEM& Item, const char* format)
{
    Item.strText.Format(format, color[0]);
    ++Item.row;
    m_pSelectedColorGrid->SetItem(&Item);
    Item.strText.Format(format, color[1]);
    ++Item.row;
    m_pSelectedColorGrid->SetItem(&Item);
    Item.strText.Format(format, color[2]);
    ++Item.row;
    m_pSelectedColorGrid->SetItem(&Item);
}

// Highlight the column currently being measured by selecting it in the grayscale
// grid. Called from the measure loop (UpdateTstWnd, right before each blocking
// reading) and from the continuous/background-measure update, so the user can see
// which point is active. bForceRepaint=TRUE + UpdateWindow() paint it immediately,
// before the UI thread blocks in the sensor read. Skipped while the grid is in
// edit mode so we don't fight the user's manual edits.
void CMainView::HighlightMeasuringColumn(int gridCol)
{
	if ( !m_pGrayScaleGrid || !::IsWindow(m_pGrayScaleGrid->GetSafeHwnd()) )
		return;
	if ( gridCol < 1 || gridCol >= m_pGrayScaleGrid->GetColumnCount() )
		return;
	if ( m_editCheckButton.GetCheck() == BST_CHECKED )
		return;

	int maxRow = m_pGrayScaleGrid->GetRowCount() - 1;	// select the x/y/Y data rows of the column
	if ( maxRow > 3 )
		maxRow = 3;
	if ( maxRow < 1 )
		return;

	m_pGrayScaleGrid->EnsureVisible(1, gridCol);
	m_pGrayScaleGrid->SetSelectedRange(1, gridCol, maxRow, gridCol, TRUE);
	m_pGrayScaleGrid->UpdateWindow();
}

void CMainView::RefreshSelection(bool b_minCol, bool inMeasure)
{
	int		i, aColorTemp;
	double	YWhite = 1.0;
	CColor	aColor;
	GV_ITEM Item;
	CString	str;
	
	Item.mask = GVIF_TEXT|GVIF_FORMAT;
	Item.nFormat = DT_RIGHT;
	Item.row = 0;
	Item.col = 1;
	if (b_minCol)
		minCol = m_pGrayScaleGrid -> GetSelectedCellRange().IsValid()?m_pGrayScaleGrid -> GetSelectedCellRange().GetMinCol():-1;

	if (m_displayMode <= 11 )  
    {
        int size=GetDocument()->GetMeasure()->GetGrayScaleSize();
		if (m_displayMode == 1)
            size = 7;
		if (m_displayMode == 3)
            size = 101;
        else if (m_displayMode == 4)
	        size = -1 * GetDocument()->GetMeasure()->GetNearWhiteScaleSize();
	
		if (m_displayMode > 4 && m_displayMode < 12)
			size=GetDocument()->GetMeasure()->GetSaturationSize();

		if (m_displayMode == 2)
		{
			m_RGBLevels.Refresh(last_Col + 1, last_Display, size);
		}
		else
		{
			if (inMeasure)
			{
				if (minCol > 1)
					m_RGBLevels.Refresh(last_minCol, m_displayMode, size);
				else
					m_RGBLevels.Refresh(minCol, m_displayMode, size);
			}
			else
				m_RGBLevels.Refresh(minCol == -1?last_minCol:minCol, m_displayMode, size);
		}

		if (last_minCol != minCol && minCol > 0)
			last_minCol = minCol;

		target_Size = size;
		if (m_displayMode == 2)
		{
			if (inMeasure)
			{
				m_Target.Refresh(GetDocument()->GetGenerator()->m_b16_235,  last_Col + 1, last_Size, last_Display, GetDocument(), CTargetWnd::TARGET_TARGET);
				m_Target.Refresh(GetDocument()->GetGenerator()->m_b16_235,  last_Col + 1, last_Size, last_Display, GetDocument(), CTargetWnd::TARGET_TESTWINDOW);
			}
			else
				m_Target.Refresh(GetDocument()->GetGenerator()->m_b16_235,  last_Col + 1, last_Size, last_Display, GetDocument(), CTargetWnd::TARGET_ALL);
		}
		else
			m_Target.Refresh(GetDocument()->GetGenerator()->m_b16_235,  last_minCol, size, m_displayMode, GetDocument(), CTargetWnd::TARGET_ALL);

	}

	if(m_SelectedColor.isValid())
	{
		if (m_displayMode != 2)
			SetLastColor(m_SelectedColor, TRUE);

		SetSelectedColor (m_SelectedColor, TRUE);

		// Retrieve measured white luminance to compute exact delta E, Lab and LCH values
		if (!b_minCol)
			SetSelectedColor (m_SelectedColor, TRUE);

		if ( GetDocument() -> GetMeasure () -> GetOnOffWhite ().isValid() )
			YWhite = GetDocument() -> GetMeasure () -> GetOnOffWhite () [ 1 ]; //onoff white is always grayscale white
		
		if ( GetDocument() -> GetMeasure () -> GetPrimeWhite().isValid() )
			if ((m_displayMode == 1 || (m_displayMode >=5 && m_displayMode <= 11)))
				YWhite = GetDocument() -> GetMeasure () -> GetPrimeWhite () [1]; //use physical Lab coords for display even in shifted diffuse case
		
		Item.strText.Format("%.3f",m_SelectedColor.GetLuminance());
		Item.row = 0;
		m_pSelectedColorGrid->SetItem(&Item);

        if (GetDocument()->m_pSensor->ReadingType() == 2)
            Item.strText.Format("%.4f",m_SelectedColor.GetLuminance() / 10.764);
        else
            Item.strText.Format("%.4f",m_SelectedColor.GetLuminance()*.29188558);
		Item.row = 1;
		m_pSelectedColorGrid->SetItem(&Item);

		aColorTemp = m_SelectedColor.GetXYZValue().GetColorTemp(GetColorReference());
		
		if ( aColorTemp < 1500 )
		{
			Item.strText = _T("< 1500");
			Item.row = 2;
			m_pSelectedColorGrid->SetItem(&Item);
		}
		else if ( aColorTemp > 12000 )
		{
			Item.strText= _T("> 12000");
			Item.row = 2;
			m_pSelectedColorGrid->SetItem(&Item);
		}
		else
		{
			Item.strText.Format ( "%d", aColorTemp );
			Item.row = 2;
			m_pSelectedColorGrid->SetItem(&Item);
		}
		CColorReference  bRef = ((GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4)?ContainerTransportReference(GetColorReference()):(GetColorReference().m_standard == HDTVa || GetColorReference().m_standard == HDTVb)?CColorReference(HDTV):GetColorReference());

        AddColorToGrid(m_SelectedColor.GetXYZValue(), Item, "%.2f");
        AddColorToGrid(m_SelectedColor.GetRGBValue(bRef), Item, "%.2f");
        AddColorToGrid(m_SelectedColor.GetxyYValue(), Item, "%.3f");
        AddColorToGrid(m_SelectedColor.GetxyzValue(), Item, "%.3f");
        AddColorToGrid(m_SelectedColor.GetLabValue(YWhite, bRef), Item, "%.2f");
        AddColorToGrid(m_SelectedColor.GetLCHValue(YWhite, bRef), Item, "%.2f");
        AddColorToGrid(m_SelectedColor.GetLMSValue(1.0, bRef), Item, "%.2f");
	}
	else
	{
		for(i=0;i<21;i++)
		{
			Item.row = i;
			Item.strText="";
			m_pSelectedColorGrid->SetItem(&Item);
		}
	}
	// MV-014: was `m_pSelectedColorGrid->Invalidate();` - SetItem already invalidates
	// only the cells that actually changed (grid-flash fix from prior work), so this
	// whole-grid repaint on top was pure duplication per measurement point.

	if ( m_pInfoWnd )
	{
		switch ( m_infoDisplay )
		{
			case 0:	// Edit
				 break;

			case 1: // target
                if (m_displayMode <= 11)// && m_displayMode != 2)
                {
                    int size=GetDocument()->GetMeasure()->GetGrayScaleSize();
					if (m_displayMode == 1)
						size = 7;
                    if (m_displayMode == 3)
                        size = 101;
                    else if (m_displayMode == 4)
                        size = -1 * GetDocument()->GetMeasure()->GetNearWhiteScaleSize();
				
					if (m_displayMode > 4 && m_displayMode < 12)
						size=GetDocument()->GetMeasure()->GetSaturationSize();

					( ( CTargetWnd * ) m_pInfoWnd ) -> m_pRefColor = & m_SelectedColor;

					if (m_displayMode == 2)
					{
						if (inMeasure)
						{
							( ( CTargetWnd * ) m_pInfoWnd ) -> Refresh (GetDocument()->GetGenerator()->m_b16_235,  last_Col + 1, last_Size, last_Display, GetDocument(), CTargetWnd::TARGET_TARGET);
							( ( CTargetWnd * ) m_pInfoWnd ) -> Refresh (GetDocument()->GetGenerator()->m_b16_235,  last_Col + 1, last_Size, last_Display, GetDocument(), CTargetWnd::TARGET_TESTWINDOW);
						}
						else
							( ( CTargetWnd * ) m_pInfoWnd ) -> Refresh (GetDocument()->GetGenerator()->m_b16_235,  last_Col + 1, last_Size, last_Display, GetDocument(), CTargetWnd::TARGET_ALL);
					}
					else
					{
						if (inMeasure)
		                    ( ( CTargetWnd * ) m_pInfoWnd ) -> Refresh (GetDocument()->GetGenerator()->m_b16_235,  last_minCol - 1, size, m_displayMode, GetDocument(), CTargetWnd::TARGET_TARGET);
						else
		                    ( ( CTargetWnd * ) m_pInfoWnd ) -> Refresh (GetDocument()->GetGenerator()->m_b16_235,  last_minCol, size, m_displayMode, GetDocument(), CTargetWnd::TARGET_TARGET);
					}

                }
				break;

			case 2:	// spectrum
				 ( ( CSpectrumWnd * ) m_pInfoWnd ) -> Refresh ();
				 break;

			case 11: // target
                if (m_displayMode <= 11)// && m_displayMode != 2)
                {
                    int size=GetDocument()->GetMeasure()->GetGrayScaleSize();
					if (m_displayMode == 1)
						size = 7;
                    if (m_displayMode == 3)
                        size = 101;
                    else if (m_displayMode == 4)
                        size = -1 * GetDocument()->GetMeasure()->GetNearWhiteScaleSize();
				
					if (m_displayMode > 4 && m_displayMode < 12)
						size=GetDocument()->GetMeasure()->GetSaturationSize();

					( ( CTargetWnd * ) m_pInfoWnd ) -> m_pRefColor = & m_SelectedColor;

					if (m_displayMode == 2)
					{
						if (inMeasure)
						{
							( ( CTargetWnd * ) m_pInfoWnd ) -> Refresh (GetDocument()->GetGenerator()->m_b16_235,  last_Col + 1, last_Size, last_Display, GetDocument(), CTargetWnd::TARGET_TARGET);
							( ( CTargetWnd * ) m_pInfoWnd ) -> Refresh (GetDocument()->GetGenerator()->m_b16_235,  last_Col + 1, last_Size, last_Display, GetDocument(), CTargetWnd::TARGET_TESTWINDOW);
						}
						else
							( ( CTargetWnd * ) m_pInfoWnd ) -> Refresh (GetDocument()->GetGenerator()->m_b16_235,  last_Col + 1, last_Size, last_Display, GetDocument(), CTargetWnd::TARGET_ALL);
					}
					else
					{
						if (inMeasure)
		                    ( ( CTargetWnd * ) m_pInfoWnd ) -> Refresh (GetDocument()->GetGenerator()->m_b16_235,  last_minCol - 1, size, m_displayMode, GetDocument(), CTargetWnd::TARGET_ALL);
						else
		                    ( ( CTargetWnd * ) m_pInfoWnd ) -> Refresh (GetDocument()->GetGenerator()->m_b16_235,  last_minCol, size, m_displayMode, GetDocument(), CTargetWnd::TARGET_ALL);
					}

                }
				break;
		}
	}
	if (!inMeasure)
	{
		UpdateGrid();
		ColorRGB ref(.5,.5,.5);
		ColorRGB meas(0.5,0.5,0.5);
		CColorReference  bRef = ((GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4)?ContainerTransportReference(GetColorReference()):(GetColorReference().m_standard == HDTVa || GetColorReference().m_standard == HDTVb)?CColorReference(HDTV):GetColorReference());
		CColor White = GetDocument()->GetMeasure()->GetOnOffWhite();
		CColor Black = GetDocument()->GetMeasure()->GetOnOffBlack();
		int mode = GetConfig()->m_GammaOffsetType;
		double tmWhite = TmDiffuseWhiteNits(noDataColor, noDataColor) / 94.37844;
		// Manual generator (DVD) keeps the legacy 105.95640/94.37844 conventions
		// upstream (UpdateGrid/GetItemText), so its comparator math stays legacy.
		BOOL bManualGen = (GetConfig()->GetGeneratorType() == CColorHCFRConfig::enumManual);
		// Mascior-style HDR CC sets keep the legacy * 100 reference scale upstream
		// (UpdateGrid / InitGrid / GetItemText), so the comparator must keep the
		// legacy tmWhite factor for them too.
		BOOL bMasciorCC = ( m_displayMode == 11 && GetConfig()->m_CCMode >= MASCIOR50 && GetConfig()->m_CCMode <= CCMAXHDR );

		double m_meas_rd, m_meas_gd, m_meas_bd, m_ref_rd, m_ref_gd, m_ref_bd;

		if (m_RefColor.isValid())
			m_lastRefColor = m_RefColor;
		else
			m_RefColor = m_lastRefColor; //restore last reference when using freemeasures where minCol = -1

		if (m_SelectedColor.isValid())
		{
			ref = ColorRGB(m_RefColor.GetRGBValue(bRef));
			if (mode == 5)
			{
				double Yref;
				if (m_displayMode==0||m_displayMode==3||m_displayMode==4||m_displayMode==12)
					Yref = GetConfig()->m_useToneMap?((GetConfig()->m_DiffuseL/94.37844) /(GetConfig()->m_TargetMaxL/10000.)):tmWhite/(GetConfig()->m_TargetMaxL/10000.);
				else if (m_displayMode == 1 && !(GetConfig()->m_colorStandard==UHDTV2||GetConfig()->m_colorStandard==UHDTV3||GetConfig()->m_colorStandard==UHDTV4))
					Yref = m_RefWhite * 10000./94.37844;
				else if ((!bManualGen && m_displayMode >= 5 && m_displayMode <= 11 && !bMasciorCC) || m_displayMode == 13)
					// sat/CC/profile references are GetHDRRefScale-scaled (1.0 =
					// tone-mapped diffuse white); without the legacy tmWhite factor
					// this reproduces exactly the code the 105.95640-scaled
					// references displayed (identical with tone mapping off).
					// The Mascior HDR CC sets are excluded: their reference still
					// gets the legacy * 100 upstream (UpdateGrid/InitGrid), so the
					// tmWhite factor must stay for them.
					Yref = m_RefWhite * 10000./94.37844;
				else
					Yref = m_RefWhite * 10000./94.37844*tmWhite;
				m_ref_rd = min(max(ref[0]/Yref,0),1);
				m_ref_gd = min(max(ref[1]/Yref,0),1);
				m_ref_bd = min(max(ref[2]/Yref,0),1);
			}
			else
			{
				if ((GetConfig()->m_colorStandard == HDTVa && m_displayMode == 1))
					m_RefWhite = m_RefWhite * GetConfig()->m_TargetMaxL / m_YWhite;
				m_ref_rd = min(max(ref[0]/m_RefWhite,0),1);
				m_ref_gd = min(max(ref[1]/m_RefWhite,0),1);
				m_ref_bd = min(max(ref[2]/m_RefWhite,0),1);
			}

			if ( mode >= 4 )
			{
				if (mode == 5 || mode == 7)
				{
					m_ref_rd = getL_EOTF(m_ref_rd, noDataColor, noDataColor, 2.4, 0.9, -1*mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL);//,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma,GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
					m_ref_gd = getL_EOTF(m_ref_gd, noDataColor, noDataColor, 2.4, 0.9, -1*mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL);//,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma,GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
					m_ref_bd = getL_EOTF(m_ref_bd, noDataColor, noDataColor, 2.4, 0.9, -1*mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL);//,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma,GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
				}
				else
				{
					m_ref_rd = getL_EOTF((m_ref_rd),White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, -1*mode);//,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
					m_ref_gd = getL_EOTF((m_ref_gd),White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, -1*mode);//,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
					m_ref_bd = getL_EOTF((m_ref_bd),White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, -1*mode);//,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
				}
			}
			else
			{
				m_ref_rd = max(0,min(pow(m_ref_rd,1.0/GetConfig()->m_GammaAvg),1)); //exact video or full scale %
				m_ref_gd = max(0,min(pow(m_ref_gd,1.0/GetConfig()->m_GammaAvg),1));
				m_ref_bd = max(0,min(pow(m_ref_bd,1.0/GetConfig()->m_GammaAvg),1));
			}

			m_ref_rd = m_ref_rd;
			m_ref_gd = m_ref_gd;
			m_ref_bd = m_ref_bd;

			m_ref_r = m_ref_rd;
			m_ref_g = m_ref_gd;
			m_ref_b = m_ref_bd;

		
		}
			trip1.SetString("No\nMeasure\n");				
			ColorRGB meas2;
		 
			if (m_SelectedColor.isValid())
			{
				CColor meas_Color = m_SelectedColor; //for patch color
				CColor meas_Color2 = meas_Color; //for patch triplet
			
				if ( (GetConfig ()->m_dE_gray == 2 || GetConfig ()->m_dE_form == 5) && ((m_displayMode == 0 || m_displayMode == 3 || m_displayMode==4) || (m_displayMode == 2 && isGS)))
				{
					double Y = meas_Color.GetY();
					//set patch luminance = ref luminance
					meas_Color.SetX(meas_Color.GetX()/Y*m_RefColor.GetY()*m_YWhite);
					meas_Color.SetY(m_RefColor.GetY()*m_YWhite);
					meas_Color.SetZ(meas_Color.GetZ()/Y*m_RefColor.GetY()*m_YWhite);
				}

				meas = ColorRGB(meas_Color.GetRGBValue(bRef));
				meas2 = ColorRGB(meas_Color2.GetRGBValue(bRef));

				if (mode == 5)
				{
					double Yref;
					if (m_displayMode==0||m_displayMode==3||m_displayMode==4||m_displayMode==12)
						Yref = GetConfig()->m_useToneMap?10000.*(GetConfig()->m_DiffuseL/94.37844):tmWhite*10000.;
					else if (m_displayMode == 1 && !(GetConfig()->m_colorStandard==UHDTV2||GetConfig()->m_colorStandard==UHDTV3||GetConfig()->m_colorStandard==UHDTV4))
						Yref = m_YWhite * 10000./94.37844;
					else if ((!bManualGen && m_displayMode >= 5 && m_displayMode <= 11 && !bMasciorCC) || m_displayMode == 13)
						// m_YWhite is the unrescaled measured white under the unified
						// convention. The legacy pair was m_YWhite_old * K * tmWhite
						// with m_YWhite_old = m_YWhite / tmWhite, so the faithful
						// replacement simply DROPS the tmWhite factor - dividing by
						// it instead would over-correct by tmWhite^2. Mascior CC is
						// excluded for the same reason as the reference side above.
						Yref = m_YWhite * 10000./94.37844;
					else
						Yref = m_YWhite * 10000./94.37844*tmWhite;
					m_meas_r = min(max(meas[0]/Yref,0),1);
					m_meas_g = min(max(meas[1]/Yref,0),1);
					m_meas_b = min(max(meas[2]/Yref,0),1);
					m_meas_rd = min(max(meas2[0]/Yref,0),1);
					m_meas_gd = min(max(meas2[1]/Yref,0),1);
					m_meas_bd = min(max(meas2[2]/Yref,0),1);
				}
				else
				{
					if ((GetConfig()->m_colorStandard == HDTVa && m_displayMode == 1))
						m_YWhite = GetConfig()->m_TargetMaxL;
					m_meas_r = min(max(meas[0]/m_YWhite,0),1);
					m_meas_g = min(max(meas[1]/m_YWhite,0),1);
					m_meas_b = min(max(meas[2]/m_YWhite,0),1);
					m_meas_rd = min(max(meas2[0]/m_YWhite,0),1);
					m_meas_gd = min(max(meas2[1]/m_YWhite,0),1);
					m_meas_bd = min(max(meas2[2]/m_YWhite,0),1);
				}

				if ( mode >= 4 )
				{
					if (mode == 5 || mode == 7)
					{
						m_meas_r=getL_EOTF(m_meas_r, noDataColor, noDataColor, 2.4, 0.9, -1*mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL);//,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
						m_meas_g=getL_EOTF(m_meas_g, noDataColor, noDataColor, 2.4, 0.9, -1*mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL);//,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
						m_meas_b=getL_EOTF(m_meas_b, noDataColor, noDataColor, 2.4, 0.9, -1*mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL);//,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
						m_meas_rd=getL_EOTF(m_meas_rd, noDataColor, noDataColor, 2.4, 0.9, -1*mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL);//,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
						m_meas_gd=getL_EOTF(m_meas_gd, noDataColor, noDataColor, 2.4, 0.9, -1*mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL);//,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
						m_meas_bd=getL_EOTF(m_meas_bd, noDataColor, noDataColor, 2.4, 0.9, -1*mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL);//,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
					}
					else
					{
						m_meas_r = getL_EOTF(m_meas_r,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, -1*mode);//,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, 1.0, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
						m_meas_g = getL_EOTF(m_meas_g,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, -1*mode);//,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, 1.0, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
						m_meas_b = getL_EOTF(m_meas_b,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, -1*mode);//,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, 1.0, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
						m_meas_rd = getL_EOTF(m_meas_rd,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, -1*mode);//,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, 1.0, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
						m_meas_gd = getL_EOTF(m_meas_gd,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, -1*mode);//,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, 1.0, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
						m_meas_bd = getL_EOTF(m_meas_bd,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, -1*mode);//,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, 1.0, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
					}
				}
				else
				{
					m_meas_r = max(0,min(pow(m_meas_r,1.0/GetConfig()->m_GammaAvg),1)); //exact video or full scale %
					m_meas_g = max(0,min(pow(m_meas_g,1.0/GetConfig()->m_GammaAvg),1));
					m_meas_b = max(0,min(pow(m_meas_b,1.0/GetConfig()->m_GammaAvg),1));
					m_meas_rd = max(0,min(pow(m_meas_rd,1.0/GetConfig()->m_GammaAvg),1)); //exact video or full scale %
					m_meas_gd = max(0,min(pow(m_meas_gd,1.0/GetConfig()->m_GammaAvg),1));
					m_meas_bd = max(0,min(pow(m_meas_bd,1.0/GetConfig()->m_GammaAvg),1));
				}
			
				if (m_SelectedColor.isValid())
				{
					if (GetConfig()->GetProfileInt("GDIGenerator","RGB_16_235",0))
					{
						m_meas_rd = floor(m_meas_rd*219. + 16.5); //round to video levels for label
						m_meas_gd = floor(m_meas_gd*219. + 16.5);
						m_meas_bd = floor(m_meas_bd*219. + 16.5);
					}
					else
					{
						m_meas_rd = floor(m_meas_rd*255. + 0.5); //exact PC levels
						m_meas_gd = floor(m_meas_gd*255. + 0.5);
						m_meas_bd = floor(m_meas_bd*255. + 0.5);
					}
						trip1.Format("%d,%d,%d\nMeasure",(int)m_meas_rd,(int)m_meas_gd,(int)m_meas_bd);
				}

			}
			ColorXYZ measXYZ(ColorRGB(pow(m_meas_r,2.22),pow(m_meas_g,2.22),pow(m_meas_b,2.22)), bRef);
			ColorXYZ measXYZref(ColorRGB(pow(m_ref_r,2.22),pow(m_ref_g,2.22),pow(m_ref_b,2.22)), bRef);
			m_meas_r1 = pow(min(max(ColorRGB(measXYZ,CColorReference(HDTV))[0],0.0),1.0),1./2.22);
			m_meas_g1 = pow(min(max(ColorRGB(measXYZ,CColorReference(HDTV))[1],0.0),1.0),1./2.22);
			m_meas_b1 = pow(min(max(ColorRGB(measXYZ,CColorReference(HDTV))[2],0.0),1.0),1./2.22);
			m_ref_r1 = pow(min(max(ColorRGB(measXYZref,CColorReference(HDTV))[0],0.0),1.0),1./2.22);
			m_ref_g1 = pow(min(max(ColorRGB(measXYZref,CColorReference(HDTV))[1],0.0),1.0),1./2.22);
			m_ref_b1 = pow(min(max(ColorRGB(measXYZref,CColorReference(HDTV))[2],0.0),1.0),1./2.22);

			trip2.SetString("No\nMeasure\n");				
			if (m_SelectedColor.isValid())
			{
				if (GetConfig()->GetProfileInt("GDIGenerator","RGB_16_235",0))
					trip2.Format("%d,%d,%d\nReference",((int)floor((m_ref_rd)*219.+0.5)+16),(int)(floor((m_ref_gd)*219.+0.5)+16),(int)(floor((m_ref_bd)*219.+0.5)+16));
				else
					trip2.Format("%d,%d,%d\nReference",((int)floor((m_ref_rd)*255+0.5)),(int)(floor((m_ref_gd)*255.+0.5)),(int)(floor((m_ref_bd)*255.+0.5)));
			}

		// Owner-drawn split swatch: push each half its fill colour and numeric
		// triplet (labels are fixed per side inside CCompSwatch). No SetWindowText
		// here - the default static repaint it triggers was the source of the old
		// comparator's one-frame flash.
		{
			BOOL bSel = m_SelectedColor.isValid();
			m_Ccomp.SetContent(RGB((int)floor(m_meas_r1*255.+0.5),(int)floor(m_meas_g1*255.+0.5),(int)floor(m_meas_b1*255.+0.5)), bSel, trip1.SpanExcluding("\n"));
			m_Ccomp3.SetContent(RGB((int)floor(m_ref_r1*255.+0.5),(int)floor(m_ref_g1*255.+0.5),(int)floor(m_ref_b1*255.+0.5)), bSel, trip2.SpanExcluding("\n"));
		}
	}
}

void CMainView::InitGrid(bool sizeGrid)
{
	if(m_pGrayScaleGrid==NULL)
		return;

	if(m_displayMode == 13)
		return;		// display profile: the grid is hidden, the pane owns the area

	CDataSetDoc *	pDataRef = GetDataRef();

	if ( pDataRef == GetDocument () )
		pDataRef = NULL;

   	int size;
	int nRows;
	BOOL bHasLuxValues = FALSE;
	BOOL bHasLuxDelta = FALSE;
	BOOL bIRE = GetDocument()->GetMeasure()->m_bIREScaleMode;

	if ( m_displayMode == 0 )
	{
   		size = GetDocument()->GetMeasure()->GetGrayScaleSize();
		if ( pDataRef && pDataRef->GetMeasure()->GetGrayScaleSize() != size )
			pDataRef = NULL;

		if ( pDataRef && pDataRef->GetMeasure()->m_bIREScaleMode != bIRE )
			pDataRef = NULL;

		if ( size )
			bHasLuxValues = GetDocument()->GetMeasure()->GetGray(0).HasLuxValue ();

		bHasLuxDelta = bHasLuxValues;
	}
	else if ( m_displayMode == 1 )
	{
		size = 8;
		bHasLuxValues = GetDocument()->GetMeasure()->GetRedPrimary().HasLuxValue ();
		if ( bHasLuxValues )
		{
			if ( GetDocument()->GetMeasure()->GetOnOffWhite().isValid() )
				bHasLuxDelta = TRUE;
		}
	}
	else if ( m_displayMode == 2 )
	{
		size = GetDocument()->GetMeasure()->GetMeasurementsSize();
		if ( pDataRef && pDataRef->GetMeasure()->GetMeasurementsSize() != size )
			pDataRef = NULL;
	}
	else if ( m_displayMode == 3 )
	{
		size=GetDocument()->GetMeasure()->GetNearBlackScaleSize();
		if ( pDataRef && pDataRef->GetMeasure()->GetNearBlackScaleSize() != size )
			pDataRef = NULL;
		if ( size )
			bHasLuxValues = GetDocument()->GetMeasure()->GetNearBlack(0).HasLuxValue ();
	}
	else if ( m_displayMode == 4 )
	{
		size=GetDocument()->GetMeasure()->GetNearWhiteScaleSize();
		if ( pDataRef && pDataRef->GetMeasure()->GetNearWhiteScaleSize() != size )
			pDataRef = NULL;
		if ( size )
			bHasLuxValues = GetDocument()->GetMeasure()->GetNearWhite(0).HasLuxValue ();
	}
	else if ( m_displayMode > 4 && m_displayMode < 11 )
	{
		size=GetDocument()->GetMeasure()->GetSaturationSize();
		if ( pDataRef && pDataRef->GetMeasure()->GetSaturationSize() != size )
			pDataRef = NULL;
		if ( size )
		{
			switch ( m_displayMode )
			{
				case 5:
					 bHasLuxValues = GetDocument()->GetMeasure()->GetRedSat(0).HasLuxValue ();
					 break;

				case 6:
					 bHasLuxValues = GetDocument()->GetMeasure()->GetGreenSat(0).HasLuxValue ();
					 break;

				case 7:
					 bHasLuxValues = GetDocument()->GetMeasure()->GetBlueSat(0).HasLuxValue ();
					 break;

				case 8:
					 bHasLuxValues = GetDocument()->GetMeasure()->GetYellowSat(0).HasLuxValue ();
					 break;

				case 9:
					 bHasLuxValues = GetDocument()->GetMeasure()->GetCyanSat(0).HasLuxValue ();
					 break;

				case 10:
					 bHasLuxValues = GetDocument()->GetMeasure()->GetMagentaSat(0).HasLuxValue ();
					 break;

			}
		}
	}
	else if ( m_displayMode == 11)
	{
		BOOL isExtPat =( GetConfig()->m_CCMode == USER || GetConfig()->m_CCMode == CM10SAT || GetConfig()->m_CCMode == CM10SAT75 || GetConfig()->m_CCMode == CM5SAT || GetConfig()->m_CCMode == CM5SAT75 || GetConfig()->m_CCMode == CM4SAT || GetConfig()->m_CCMode == CM4SAT75 || GetConfig()->m_CCMode == CM4LUM || GetConfig()->m_CCMode == CM5LUM || GetConfig()->m_CCMode == CM10LUM || GetConfig()->m_CCMode == RANDOM250 || GetConfig()->m_CCMode == RANDOM500 || GetConfig()->m_CCMode == CM6NB || GetConfig()->m_CCMode == CMDNR || GetConfig()->m_CCMode == MASCIOR50);
		isExtPat = (isExtPat || GetConfig()->m_CCMode > 19);
        if (isExtPat)
            size = (GetConfig()->GetCColorsSize());		
        else
            size = (GetConfig()->m_CCMode == CCSG?96:(GetConfig()->m_CCMode == AXIS?71:24));
			if (GetConfig()->m_CCMode==CMS || GetConfig()->m_CCMode==CPS)
				size = 19;
		bHasLuxValues = GetDocument()->GetMeasure()->GetCC24Sat(0).HasLuxValue ();
	}
	else if ( m_displayMode == 12 )
	{
		size = 6;
		pDataRef = NULL;
		bHasLuxValues = GetDocument()->GetMeasure()->GetOnOffWhite().HasLuxValue ();
	}

	if ( m_displayMode == 12 )
		nRows = 3;
	else if ( ! pDataRef )
		nRows = 5;
	else
		nRows = 7;

	if ( m_displayMode <= 1 || (m_displayMode >= 3 && m_displayMode <=11) )
		nRows ++;

	if (m_displayMode == 1 || (m_displayMode >= 5 && m_displayMode <=11) )
		nRows ++;

	if ( bHasLuxValues )
	{
		if ( bHasLuxDelta )
			nRows += 2;
		else
			nRows ++;
	}

	m_pGrayScaleGrid->SetTextColor(FxGetSysColor(COLOR_WINDOWTEXT));
	m_pGrayScaleGrid->SetTextBkColor(FxGetSysColor(COLOR_WINDOW));
	m_pGrayScaleGrid->SetFixedTextColor(FxGetSysColor(COLOR_WINDOWTEXT));
	m_pGrayScaleGrid->SetFixedBkColor(FxGetSysColor(COLOR_3DFACE));
	m_pGrayScaleGrid->SetGridLineColor(GetConfig()->m_darkTheme ? RGB(96,96,100) : RGB(192,192,192));
	m_pGrayScaleGrid->SetFixedRowCount(1);
	m_pGrayScaleGrid->SetFixedColumnCount(1);

    m_pGrayScaleGrid->SetRowCount(nRows + 1);     
    m_pGrayScaleGrid->SetColumnCount(size+1);
	
	m_pGrayScaleGrid->SetFixedColumnSelection(TRUE);
	m_pGrayScaleGrid->SetFixedRowSelection(FALSE);
	m_pGrayScaleGrid->SetTrackFocusCell(TRUE);
	m_pGrayScaleGrid->SetEditable(m_editCheckButton.GetCheck());
	m_pGrayScaleGrid->EnableDragAndDrop(m_editCheckButton.GetCheck());
	m_pGrayScaleGrid->SetDoubleBuffering(TRUE);

	m_pGrayScaleGrid->SetDefCellMargin(3);

	// Set the font to bold
	CFont* pFont = m_pGrayScaleGrid->GetFont();
	LOGFONT lf;
	pFont->GetLogFont(&lf);
	lf.lfWeight=FW_BOLD;
	m_pGrayScaleGrid->SetItemFont(0,0, &lf); // Set the font to bold
	// set column label
	GV_ITEM Item;
	Item.mask = GVIF_TEXT|GVIF_FORMAT;
	Item.row = 0;
	Item.col = 0;
	Item.nFormat = DT_CENTER|DT_WORDBREAK;
	
	switch ( m_displayMode )
	{
		case 0:
			 Item.strText = ( bIRE ? "IRE" : GetConfig()->m_PercentGray );
			 break;
		case 1:
			 Item.strText="Color";
			 break;
		case 2:
		case 3:
		case 4:
			 Item.strText=GetConfig()->m_PercentGray;
			 break;

		case 11:
			 Item.strText="Color Checker";
			 break;
		case 12:
			 Item.strText=" ";
			 break;

		default:
			 Item.strText="% Sat";
			 break;
	}

	m_pGrayScaleGrid->SetItem(&Item);

    int i;

	// set columns labels 
	for(i=0;i<size;i++)
	{
		Item.row = 0;
		Item.col = i+1;
		Item.nFormat = DT_CENTER|DT_WORDBREAK;
		BOOL isExtPat =( GetConfig()->m_CCMode == USER || GetConfig()->m_CCMode == CM10SAT || GetConfig()->m_CCMode == CM10SAT75 || GetConfig()->m_CCMode == CM5SAT || GetConfig()->m_CCMode == CM5SAT75 || GetConfig()->m_CCMode == CM4SAT || GetConfig()->m_CCMode == CM4SAT75 || GetConfig()->m_CCMode == CM4LUM || GetConfig()->m_CCMode == CM5LUM || GetConfig()->m_CCMode == CM10LUM || GetConfig()->m_CCMode == RANDOM250 || GetConfig()->m_CCMode == RANDOM500 || GetConfig()->m_CCMode == CM6NB || GetConfig()->m_CCMode == CMDNR || GetConfig()->m_CCMode == MASCIOR50);
		isExtPat = (isExtPat || GetConfig()->m_CCMode > 19);

		switch ( m_displayMode )
		{
			case 0:
				 if ( bIRE && i==0 )
					Item.strText.Format("%.1f",GetDocument()->GetMeasure()->GetGrayPercent ( i, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235() ));
				 else
					Item.strText.Format("%d", (int) floor(GetDocument()->GetMeasure()->GetGrayPercent ( i, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235() )+0.5) );
				 break;

			case 1:
				 switch ( i )
				 {
					case 0:
						Item.strText.LoadString((GetColorReference().m_standard==CC6)?IDS_CC6a:IDS_RED);
						 break;

					case 1:
						 Item.strText.LoadString((GetColorReference().m_standard==CC6)?IDS_CC6b:IDS_GREEN);
						 break;

					case 2:
						 Item.strText.LoadString((GetColorReference().m_standard==CC6)?IDS_CC6c:IDS_BLUE);
						 break;

					case 3:
						 Item.strText.LoadString((GetColorReference().m_standard==CC6)?IDS_CC6d:IDS_YELLOW);
						 break;

					case 4:
						 Item.strText.LoadString((GetColorReference().m_standard==CC6)?IDS_CC6e:IDS_CYAN);
						 break;

					case 5:
						 Item.strText.LoadString((GetColorReference().m_standard==CC6)?IDS_CC6f:IDS_MAGENTA);
						 break;

					case 6:
						 Item.strText.LoadString(IDS_WHITE);
						 break;

					case 7:
						 Item.strText.LoadString(IDS_BLACK);
						 break;
				 }
				 break;

			case 2:
				 Item.strText.Format("%d",Item.col);
				 break;

			case 3:
				 Item.strText.Format("%d",i*(GetConfig()->m_GammaOffsetType==5?2:1));
				 break;

			case 4:
				 Item.strText.Format("%d",i + GetDocument()->GetMeasure()->m_NearWhiteClipCol - size);
				 break;

			case 11:
                 if (GetConfig()->m_CCMode == CCSG)
                 {
                    Item.strText.SetString(PatName[i]);
                 } 
                 else if (GetConfig()->m_CCMode == CMS)
                 {
                    Item.strText.SetString(PatNameCMS[i]);
                 }
                 else if (GetConfig()->m_CCMode == CPS)
                 {
                    Item.strText.SetString(PatNameCPS[i]);
                 }
                 else if (GetConfig()->m_CCMode == AXIS)
                 {
                    Item.strText.SetString(PatNameAXIS[i]);
                 }
                 else if (isExtPat)
                 {
                     // Pass the name straight through - a user CSV name can exceed any fixed
                     // buffer (the char[50]+sprintf here overran the stack); same class as the
                     // CIEChartView '%'/length fix in this PR.
                     std::string name = GetConfig()->GetCColorsN(i);
                     Item.strText.SetString(name.c_str());
                 }
                 else {
				 switch ( i )
				 {
					 case 0:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_1a:(GetConfig()->m_CCMode == SKIN?IDS_CC_1b:IDS_CC_1));
						break;

					 case 1:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_2a:(GetConfig()->m_CCMode == SKIN?IDS_CC_2b:IDS_CC_2));
						break;

					 case 2:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_3a:(GetConfig()->m_CCMode == SKIN?IDS_CC_3b:IDS_CC_3));
						break;

					 case 3:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_4a:(GetConfig()->m_CCMode == SKIN?IDS_CC_4b:IDS_CC_4));
						break;

					 case 4:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_5a:(GetConfig()->m_CCMode == SKIN?IDS_CC_5b:IDS_CC_5));
						break;

					 case 5:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_6a:(GetConfig()->m_CCMode == SKIN?IDS_CC_6b:IDS_CC_6));
						break;

					 case 6:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_7a:(GetConfig()->m_CCMode == SKIN?IDS_CC_7b:IDS_CC_7));
						break;

					 case 7:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_8a:(GetConfig()->m_CCMode == SKIN?IDS_CC_8b:IDS_CC_8));
						break;

					 case 8:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_9a:(GetConfig()->m_CCMode == SKIN?IDS_CC_9b:IDS_CC_9));
						break;

					 case 9:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_10a:(GetConfig()->m_CCMode == SKIN?IDS_CC_10b:IDS_CC_10));
						break;

					 case 10:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_11a:(GetConfig()->m_CCMode == SKIN?IDS_CC_11b:IDS_CC_11));
						break;

					 case 11:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_12a:(GetConfig()->m_CCMode == SKIN?IDS_CC_12b:IDS_CC_12));
						break;

					 case 12:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_13a:(GetConfig()->m_CCMode == SKIN?IDS_CC_13b:IDS_CC_13));
						break;

					 case 13:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_14a:(GetConfig()->m_CCMode == SKIN?IDS_CC_14b:IDS_CC_14));
						break;

					 case 14:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_15a:(GetConfig()->m_CCMode == SKIN?IDS_CC_15b:IDS_CC_15));
						break;

					 case 15:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_16a:(GetConfig()->m_CCMode == SKIN?IDS_CC_16b:IDS_CC_16));
						break;

					 case 16:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_17a:(GetConfig()->m_CCMode == SKIN?IDS_CC_17b:IDS_CC_17));
						break;

					 case 17:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_18a:(GetConfig()->m_CCMode == SKIN?IDS_CC_18b:IDS_CC_18));
						break;

					 case 18:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_19a:(GetConfig()->m_CCMode == SKIN?IDS_CC_19b:IDS_CC_19));
						break;

					 case 19:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_20a:(GetConfig()->m_CCMode == SKIN?IDS_CC_20b:IDS_CC_20));
						break;

					 case 20:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_21a:(GetConfig()->m_CCMode == SKIN?IDS_CC_21b:IDS_CC_21));
						break;

					 case 21:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_22a:(GetConfig()->m_CCMode == SKIN?IDS_CC_22b:IDS_CC_22));
						break;

					 case 22:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_23a:(GetConfig()->m_CCMode == SKIN?IDS_CC_23b:IDS_CC_23));
						break;

					 case 23:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_24a:(GetConfig()->m_CCMode == SKIN?IDS_CC_24b:IDS_CC_24));
						break;
            		 default:
				        Item.strText.LoadString(IDS_CC_24a);
                 }
                 }
				 break;

			case 12:
				 switch ( i )
				 {
					case 0:
						 Item.strText.LoadString(IDS_BLACK);
						 break;

					case 1:
						 Item.strText.LoadString(IDS_WHITE);
						 break;

					case 2:
						 Item.strText="ANSI 1";
						 break;

					case 3:
						 Item.strText="ANSI 2";
						 break;

					case 4:
					case 5:
						 Item.strText.Empty ();
						 break;
				 }
				 break;

			default:
				 Item.strText.Format("%d",i*100/(size-1));
				 break;
		}

		m_pGrayScaleGrid->SetItem(&Item);
		m_pGrayScaleGrid->SetItemFont(Item.row,Item.col, &lf); // Set the font to bold

		for ( int i2 = 4; i2 <= nRows ; i2++ )
		{
			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, GridBk( (i2&1) ? RGB(240,240,240) : RGB(224,224,224) ) );
			m_pGrayScaleGrid->SetItemState ( i2, i+1, m_pGrayScaleGrid->GetItemState(i2,i+1) | GVIS_READONLY );
        }

		if (m_displayMode < 13)
        {
            int i2=0;
            double step = 120.0 / size;
            m_pGrayScaleGrid->SetItemFgColour(i2, i+1, RGB(10,10,10));
            m_pGrayScaleGrid->SetItemBkColour(i2, i+1, RGB(240,240,240));
            switch (m_displayMode)
            {
            case 0:
    			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(i*step+130,i*step+130,i*step+130) );
                break;
            case 1:
                switch(i+1)
                {
                case 1:
            			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(240,200,200) );
                        break;
                case 2:
            			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(200,240,200) );
                        break;
                case 3:
            			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(200,200,240) );
                        break;
                case 4:
            			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(240,240,200) );
                        break;
                case 5:
            			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(200,240,240) );
                        break;
                case 6:
            			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(240,200,240) );
                        break;
                }
            break;
            case 2:
            case 3:
            case 4:
    			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(i*step+130,i*step+130,i*step+130) );
                break;
            case 5:
    			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(240,240-i*step*2,240-i*step*2) );
                break;
            case 6:
    			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(240-i*step*2,240,240-i*step*2) );
                break;
            case 7:
    			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(240-i*step*2,240-i*step*2,240) );
                break;
            case 8:
    			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(240,240,240-i*step*2) );
                break;
            case 9:
    			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(240-i*step*2,240,240) );
                break;
            case 10:
    			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(240,240-i*step*2,240) );
                break;
            case 11:
                CColor s_clr;
                ColorRGB r_clr;
                double inten;
				GetDocument()->GetMeasure()->GetRefCC24Sat(i, s_clr);
				if (GetConfig()->m_GammaOffsetType == 5 && GetConfig()->m_bHDR100 )
				{
					// Match the dE path's scale (UpdateGrid ~4294): *100 for the
					// Mascior-style HDR CC sets, GetHDRRefScale otherwise (fixed
					// 105.95640 for the legacy manual-generator path) - so the
					// swatch luminance represents the same reference the dE uses.
					double s = ( GetConfig()->m_CCMode >= MASCIOR50 && GetConfig()->m_CCMode <= CCMAXHDR ) ? 100.
							 : ( GetConfig()->GetGeneratorType() == CColorHCFRConfig::enumManual ) ? 105.95640
							 : GetDocument()->GetMeasure()->GetHDRRefScale();
					s_clr.SetX(s_clr.GetX()*s);
					s_clr.SetY(s_clr.GetY()*s);
					s_clr.SetZ(s_clr.GetZ()*s);
				}
                r_clr=s_clr.GetRGBValue(CColorReference(HDTV));
				r_clr[0]=(min(max(r_clr[0],0),1));
				r_clr[1]=(min(max(r_clr[1],0),1));
				r_clr[2]=(min(max(r_clr[2],0),1));
				inten = s_clr.GetLuminance();

	    		m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(pow(r_clr[0],1.0/2.2)*255,pow(r_clr[1],1.0/2.2)*255,pow(r_clr[2],1.0/2.2)*255) );
                
				if (inten < 0.4)
                    m_pGrayScaleGrid->SetItemFgColour(i2, i+1, RGB(240,240,240));
                else
                    m_pGrayScaleGrid->SetItemFgColour(i2, i+1, RGB(10,10,10));
            break;
            }
        }
	}

	if ( size >= 5 )
	{
		Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;

		Item.col = 5;
		for ( i = 0 ; i < 3 + bHasLuxValues; i++ )
		{
			Item.row = i+1;
			if ( m_displayMode == 12 )
			{
				Item.strText = ( i==0 ? "ON/OFF:" : ( i==1 ? "ANSI:" : "" ) );
				m_pGrayScaleGrid->SetItem(&Item);
				m_pGrayScaleGrid->SetItemBkColour ( Item.row,Item.col, GridBk(RGB(224,224,224)) );
//				m_pGrayScaleGrid->SetItemState ( Item.row,Item.col, m_pGrayScaleGrid->GetItemState(Item.row,Item.col) | GVIS_READONLY );
			}
			else if ( i < 3 )
			{
				m_pGrayScaleGrid->SetItemBkColour ( Item.row,Item.col, FxGetSysColor(COLOR_WINDOW) );
//				m_pGrayScaleGrid->SetItemState ( Item.row,Item.col, m_pGrayScaleGrid->GetItemState(Item.row,Item.col) & (~GVIS_READONLY) );
			}
		}

		if ( size >= 6 )
		{
			Item.col = 6;
			Item.strText.Empty ();
			for ( i = 0 ; i < 3 + bHasLuxValues; i++ )
			{
				Item.row = i+1;
				if ( m_displayMode == 12 )
				{
					m_pGrayScaleGrid->SetItem(&Item);
//					m_pGrayScaleGrid->SetItemState ( Item.row,Item.col, m_pGrayScaleGrid->GetItemState(Item.row,Item.col) | GVIS_READONLY );
				}
				else if ( i < 3 )
				{
				m_pGrayScaleGrid->SetItemBkColour ( Item.row,Item.col, FxGetSysColor(COLOR_WINDOW) );
//					m_pGrayScaleGrid->SetItemState ( Item.row,Item.col, m_pGrayScaleGrid->GetItemState(Item.row,Item.col) & (~GVIS_READONLY) );
				}
			}
		}

		Item.nFormat = DT_CENTER|DT_WORDBREAK;
	}

	// Set row labels
	for(i=0;i<nRows;i++)
	{
		Item.row = i+1;
		Item.col = 0;
		m_pGrayScaleGrid->SetItemFont(Item.row,Item.col, &lf); // Set the font to bold
		
		if ( bHasLuxValues && i == nRows - ( 1 + bHasLuxDelta ) )
		{
			if ( GetConfig () -> m_bUseImperialUnits )
				Item.strText = "Ft-cd";
			else
				Item.strText = "Lux";
		}
		else if ( bHasLuxValues && bHasLuxDelta && i == nRows - 1 )
		{
			Item.strText = "delta Y / lux";
		}
		else
			Item.strText = GetGridRowLabel(i);
		
		m_pGrayScaleGrid->SetItem(&Item);
	}

	if (sizeGrid)
	{
		m_pGrayScaleGrid->AutoSizeColumns();
		m_pGrayScaleGrid->ExpandColumnsToFit(FALSE);
		double width = m_pGrayScaleGrid -> GetColumnWidth ( 1 );
		width = max(width, 80);
		if (size < 4)
			width = min(width, 100);
		if (width == 80 || width == 100)
		{
			for ( i = 1 ; i <= size ; i ++ )
				m_pGrayScaleGrid -> SetColumnWidth ( i, width);
		}

		m_pGrayScaleGrid->AutoSizeRows();
		m_pGrayScaleGrid->ExpandRowsToFit(FALSE);
		double height = m_pGrayScaleGrid -> GetRowHeight ( 1 );
		height = max(height, 18);
		if (height == 18)
		{
			for ( i = 1 ; i <= nRows ; i ++ )
				m_pGrayScaleGrid -> SetRowHeight ( i, height);
		}


	}

	if  (GetConfig()->GetProfileDouble("References","Use Black Level",0)) //store/retrieve real black measurements
	{
		if (!m_userBlack)
		{
			m_oldBlackGS = GetDocument()->GetMeasure()->GetGray(0);
			m_oldBlackNB = GetDocument()->GetMeasure()->GetNearBlack(0);
			double Yblack = GetConfig()->GetProfileDouble("References","Manual Black Level",0);
			GetDocument()->GetMeasure()->SetGray(0, ColorXYZ(Yblack*.95047,Yblack,Yblack*1.0883));
			GetDocument()->GetMeasure()->SetNearBlack(0, ColorXYZ(Yblack*.95047,Yblack,Yblack*1.0883)) ;
			GetDocument()->GetMeasure()->SetOnOffBlack(ColorXYZ(Yblack*.95047,Yblack,Yblack*1.0883)) ;
			m_userBlack = TRUE;
		}
	}
	else
	{
		if (m_userBlack)
		{
			GetDocument()->GetMeasure()->SetGray(0, m_oldBlackGS);
			GetDocument()->GetMeasure()->SetNearBlack(0, m_oldBlackNB) ;
			GetDocument()->GetMeasure()->SetOnOffBlack(m_oldBlackGS) ;
			m_userBlack = FALSE;
		}
	}

	if ( (m_displayMode == 0 || m_displayMode == 3 || m_displayMode == 12) && GetConfig()->m_userBlack)
	{
    		m_pGrayScaleGrid->SetItemBkColour ( 1, 1, (GetConfig()->m_darkTheme ? RGB(0,0,0) : RGB(255,218,185)) );
    		m_pGrayScaleGrid->SetItemBkColour ( 2, 1, (GetConfig()->m_darkTheme ? RGB(0,0,0) : RGB(255,218,185)) );
    		m_pGrayScaleGrid->SetItemBkColour ( 3, 1, (GetConfig()->m_darkTheme ? RGB(0,0,0) : RGB(255,218,185)) );
    		m_pGrayScaleGrid->SetItemFgColour ( 1, 1, (GetConfig()->m_darkTheme ? RGB(255,195,0) : RGB(0,10,185)) );
    		m_pGrayScaleGrid->SetItemFgColour ( 2, 1, (GetConfig()->m_darkTheme ? RGB(255,195,0) : RGB(0,10,185)) );
    		m_pGrayScaleGrid->SetItemFgColour ( 3, 1, (GetConfig()->m_darkTheme ? RGB(255,195,0) : RGB(0,10,185)) );
	} else
	{
   			m_pGrayScaleGrid->SetItemBkColour ( 1, 1, GridBk(RGB(255,255,255)) );
   			m_pGrayScaleGrid->SetItemBkColour ( 2, 1, GridBk(RGB(255,255,255)) );
   			m_pGrayScaleGrid->SetItemBkColour ( 3, 1, GridBk(RGB(255,255,255)) );
   			m_pGrayScaleGrid->SetItemFgColour ( 1, 1, GridFg(RGB(0,0,0)) );
   			m_pGrayScaleGrid->SetItemFgColour ( 2, 1, GridFg(RGB(0,0,0)) );
   			m_pGrayScaleGrid->SetItemFgColour ( 3, 1, GridFg(RGB(0,0,0)) );
	}

	OnEditgridCheck();
}

void CMainView::InitSelectedColorGrid()
{
	if(m_pSelectedColorGrid==NULL)
		return;

	int nReadingType = GetDocument()->m_pSensor->ReadingType();
	// Structure/labels depend only on the sensor reading type, which doesn't change during a run.
	// Skip the full rebuild + autosize (a per-update flicker source) when nothing structural changed.
	if ( m_pSelectedColorGrid->GetRowCount() == 24 && m_nSelColorGridReadingType == nReadingType )
		return;
	m_nSelColorGridReadingType = nReadingType;

	m_pSelectedColorGrid->SetTextColor(FxGetSysColor(COLOR_WINDOWTEXT));
	m_pSelectedColorGrid->SetTextBkColor(FxGetSysColor(COLOR_WINDOW));
	m_pSelectedColorGrid->SetFixedTextColor(FxGetSysColor(COLOR_WINDOWTEXT));
	m_pSelectedColorGrid->SetFixedBkColor(FxGetSysColor(COLOR_3DFACE));
	m_pSelectedColorGrid->SetGridLineColor(GetConfig()->m_darkTheme ? RGB(96,96,100) : RGB(192,192,192));
	m_pSelectedColorGrid->SetFixedColumnCount(1);

    m_pSelectedColorGrid->SetRowCount(24);
    m_pSelectedColorGrid->SetColumnCount(2);
	
	m_pSelectedColorGrid->SetFixedColumnSelection(FALSE);
	m_pSelectedColorGrid->SetFixedRowSelection(FALSE);
	m_pSelectedColorGrid->SetTrackFocusCell(TRUE);
	m_pSelectedColorGrid->SetEditable(FALSE);
	m_pSelectedColorGrid->EnableDragAndDrop(FALSE);
	m_pSelectedColorGrid->SetDoubleBuffering(TRUE);	// double-buffer to reduce flicker while live values stream in

	m_pSelectedColorGrid->SetDefCellMargin(3);

	// Set the font to bold
	CFont* pFont = m_pSelectedColorGrid->GetFont();
	LOGFONT lf;
	pFont->GetLogFont(&lf);
	lf.lfWeight=FW_BOLD;

	// Set row labels
	GV_ITEM Item;
	Item.mask = GVIF_TEXT|GVIF_FORMAT;
	Item.nFormat = DT_CENTER|DT_WORDBREAK;

    char * RowLabels [] = { "Y cd/m\xB2", "Y ftL", "T\xB0", "X", "Y", "Z", "R", "G", "B", "x", "y", "Y", "x", "y", "z", "L", "a", "b", "L", "C", "H","L","M","S"};
            
    if (GetDocument()->m_pSensor->ReadingType() == 2)
    {
			RowLabels [0] = "T lux";
	        RowLabels [1] = "Y ft-c";
    }

    for(int i=0;i<24;i++)
	{
		Item.row = i;
		Item.col = 0;
		m_pSelectedColorGrid->SetItemFont(Item.row,Item.col, &lf); // Set the font to bold
		Item.strText=RowLabels[i];
		m_pSelectedColorGrid->SetItem(&Item);

		if ( (i/3)&1 )
			m_pSelectedColorGrid->SetItemBkColour(Item.row,1,GridBk(RGB(224,224,224)));
	}

	m_pSelectedColorGrid->ExpandColumnsToFit(TRUE);
	m_pSelectedColorGrid->AutoSizeColumn(0);
	m_pSelectedColorGrid->ExpandColumnsToFit(TRUE);

    m_pSelectedColorGrid->AutoSizeRows();
}

/////////////////////////////////////////////////////////////////////////////
// CMainView printing

BOOL CMainView::OnPreparePrinting(CPrintInfo* pInfo)
{
 	// default preparation
	return DoPreparePrinting(pInfo);
}

void CMainView::OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo)
{
    if (m_pGrayScaleGrid)             
		m_pGrayScaleGrid->OnBeginPrinting(pDC,pInfo);
}

void CMainView::OnEndPrinting(CDC* pDC, CPrintInfo* pInfo)
{
    if (m_pGrayScaleGrid)             
		m_pGrayScaleGrid->OnEndPrinting(pDC,pInfo);
}

void CMainView::OnPrint(CDC* pDC, CPrintInfo* pInfo)
{
    if (m_pGrayScaleGrid)             
		m_pGrayScaleGrid->OnPrint(pDC,pInfo);
}

/////////////////////////////////////////////////////////////////////////////
// CMainView diagnostics

#ifdef _DEBUG
void CMainView::AssertValid() const
{
	CFormView::AssertValid();
}

void CMainView::Dump(CDumpContext& dc) const
{
	CFormView::Dump(dc);
}

CDataSetDoc* CMainView::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CDataSetDoc)));
	return (CDataSetDoc*)m_pDocument;
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMainView message handlers

void CMainView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	int		nForceMode = -1;
	double	dContrast;
	InitSelectedColorGrid();

	// Keep the parameter dropdowns in sync with document-side changes (scale
	// sizes dialog, stimulus level rebinds). Skipped for realtime per-point
	// hints so sweeps don't pay for combo rebuilds.
	if ( lHint == UPD_EVERYTHING || lHint == UPD_ALLSATURATIONS )
		UpdateParamCombos();
	CFrameWnd * pFrame = (CFrameWnd *)(AfxGetApp()->m_pMainWnd);
//	if (pFrame)
//		pFrame->GetActiveFrame()->ActivateFrame();
	if ( lHint < UPD_REALTIME && lHint != UPD_FREEMEASUREAPPENDED )
		pFrame->OnUpdateFrameMenu(NULL);
	// TODO: add a general option for that ?
	if ( 1 )
	{
		// Adjust grid selection if necessary
		switch ( lHint )
		{
			case UPD_SELECTEDCOLOR:
				RefreshSelection();
				break;
			case UPD_PRIMARIES:
			case UPD_SECONDARIES:
			case UPD_PRIMARIESANDSECONDARIES:
				 if ( m_displayMode != 1 )
					nForceMode = 1;
				 break;

			case UPD_GRAYSCALEANDCOLORS:
				 if ( m_displayMode != 0 && m_displayMode != 1 )
					nForceMode = 0;
				 break;

			case UPD_GRAYSCALE:
				 if ( m_displayMode != 0 )
					nForceMode = 0;
				 break;

			case UPD_NEARBLACK:
				 if ( m_displayMode != 3 )
					nForceMode = 3;
				 break;

			case UPD_NEARWHITE:
				 if ( m_displayMode != 4 )
					nForceMode = 4;
				 break;

			case UPD_REDSAT:
				 if ( m_displayMode != 5 )
					nForceMode = 5;
				 break;

			case UPD_GREENSAT:
				 if ( m_displayMode != 6 )
					nForceMode = 6;
				 break;

			case UPD_BLUESAT:
				 if ( m_displayMode != 7 )
					nForceMode = 7;
				 break;

			case UPD_YELLOWSAT:
				 if ( m_displayMode != 8 )
					nForceMode = 8;
				 break;

			case UPD_CYANSAT:
				 if ( m_displayMode != 9 )
					nForceMode = 9;
				 break;

			case UPD_MAGENTASAT:
				 if ( m_displayMode != 10 )
					nForceMode = 10;
				 break;

			case UPD_CC24SAT:
				 if ( m_displayMode != 11 )
					nForceMode = 11;
				 else
					 UpdateGrid();
				 break;

			case UPD_ALLSATURATIONS:
				 if ( m_displayMode < 5 || m_displayMode > 11 )
					nForceMode = 5;
				 break;

			case UPD_CONTRAST:
				 if ( m_displayMode != 12 )
					nForceMode = 12;
				 break;

			case UPD_DISPLAYPROFILE:
				 if ( m_displayMode != 13 )
					nForceMode = 13;
				 else if ( m_profilePane.GetSafeHwnd () )
					m_profilePane.RefreshState ();
				 break;
		}

		if ( nForceMode >= 0 )
		{
			m_comboMode.SetCurSel ( nForceMode );
			OnSelchangeComboMode();
		}
	}
	CColor	MeasuredColor=noDataColor;

	int	n = GetDocument()->GetMeasure()->GetMeasurementsSize();

	if ( n > 0 )
		MeasuredColor=GetDocument()->GetMeasure()->GetMeasurement(n-1);

	if ( lHint == UPD_FREEMEASUREAPPENDED && pHint == g_pDataDocRunningThread && g_hThread && ! g_bTerminateThread )
	{
		// Optimized version for continuous measures: update only measurement grid
		if (m_displayMode == 0 )
		{
			// Gray/Primary colors may have been updated during free measures
			if ( last_minCol >= 1 && MeasuredColor.isValid() )// && ( MeasuredColor.GetDeltaxy(GetColorReference().GetWhite(), GetColorReference()) < 0.03 || last_minCol == 1 ) )
			{
				GetDocument()->GetMeasure()->SetGray(last_minCol - 1, MeasuredColor);
				if (last_minCol == 1)
					GetDocument()->GetMeasure()->SetOnOffBlack(MeasuredColor);
				if (last_minCol == GetDocument()->GetMeasure()->GetGrayScaleSize ()    )
					GetDocument()->GetMeasure()->SetOnOffWhite(MeasuredColor);
			}
 			
		}
		UpdateGrid();		
		UpdateMeasurementsAfterBkgndMeasure ();
	}
	else if ( lHint >= UPD_REALTIME && lHint != UPD_DISPLAYPROFILE ) //optimized for realtime
	{
		last_minCol = GetDocument()->GetMeasure()->m_currentIndex;
		minCol = last_minCol;

		if (m_displayMode != (lHint - UPD_REALTIME)) //need to change to correct sequence
		{
			m_displayMode = (lHint - UPD_REALTIME);
			m_comboMode.SetCurSel (m_displayMode);
			OnSelchangeComboMode();
			minCol = 1;
			last_minCol = minCol;
		}

		if ( m_displayMode == 13 && m_profilePane.GetSafeHwnd () )
		{
			// per-patch progress during a profile capture; RefreshSelection is
			// grid-bound (gated <= 11), so drive the reference widgets and the
			// desktop test window directly for the on-screen patch
			m_profilePane.OnCaptureProgress ();
			CMeasure * pProfMeasure = GetDocument()->GetMeasure();
			int nProfSize = pProfMeasure->GetProfileMeasureSize();
			if ( nProfSize > 0 )
			{
				// desktop test window shows the patch being DISPLAYED (index cur)...
				int cur = min ( pProfMeasure->m_currentIndex, nProfSize - 1 );
				m_Target.Refresh ( GetDocument()->GetGenerator()->m_b16_235, cur + 1, nProfSize, 13, GetDocument(), CTargetWnd::TARGET_TESTWINDOW );

				// ...while the reference widgets pair with the last MEASURED patch
				// (cur - 1), whose color the selected-measure UI is showing.
				// m_RefColor & co normally come from grid population, which mode 13
				// skips - feed the comparator directly.
				if ( pProfMeasure->m_currentIndex > 0 )
				{
					int done = min ( pProfMeasure->m_currentIndex - 1, nProfSize - 1 );
					CColor profRef;
					pProfMeasure->GetRefProfileSat ( done, profRef );
					CColor w = pProfMeasure->GetPrimeWhite ();
					if ( !w.isValid () )
						w = pProfMeasure->GetOnOffWhite ();
					m_RefColor = profRef;
					m_RefWhite = 1.0;
					m_YWhite = ( w.isValid () && w.GetY () > 0.0 ) ? w.GetY () : 1.0;
					// PQ HDR: same bridge as SelectProfilePatch -- GetRefProfileSat
					// is on the 1.0 = 10000 nits scale; without this the LIVE
					// comparator showed a ~106x-off reference during capture that
					// then "corrected itself" when the patch was clicked afterward.
					// Unified convention: GetHDRRefScale, measured white unrescaled.
					if ( GetConfig()->m_GammaOffsetType == 5 )
					{
						double s = pProfMeasure->GetHDRRefScale();
						m_RefColor.SetX( m_RefColor.GetX() * s );
						m_RefColor.SetY( m_RefColor.GetY() * s );
						m_RefColor.SetZ( m_RefColor.GetZ() * s );
					}
					m_RGBLevels.Refresh ( done + 1, 13, nProfSize );
					m_Target.Refresh ( GetDocument()->GetGenerator()->m_b16_235, done + 1, nProfSize, 13, GetDocument(), CTargetWnd::TARGET_TARGET );
				}
			}
		}

		// Color checker / user sequences store exactly one new patch per realtime
		// hint, so ask UpdateGrid for an incremental pass over just that column
		// (plus any the catch-up window covers) instead of repopulating all N --
		// a large custom sequence otherwise costs O(N^2) grid work per sweep.
		// Single-shot request: UpdateGrid consumes it and falls back to a full
		// rebuild unless its column cache matches the current patch count.
		if ( m_displayMode == 11 && minCol >= 1 && m_pGrayScaleGrid )
			m_nGridIncrCol = minCol;
		RefreshSelection(FALSE); //this will update grid
		m_nGridIncrCol = -1;

		if ( m_pInfoWnd ) //in case colorchecker slot is updated
		{
				m_pInfoWnd -> SetWindowTextA(GetDocument()->GetMeasure()->GetInfoString());
				// The text just came from the document, so the recorded commands
				// describe text that is no longer there: drop them.
				CEditEx * pSummaryEdit = DYNAMIC_DOWNCAST ( CEditEx, m_pInfoWnd );
				if ( pSummaryEdit )
					pSummaryEdit -> EmptyUndoBuffer ();
				m_pInfoWnd -> Invalidate ();
				m_pInfoWnd -> UpdateWindow();			
		}
	}
	else
	{
		// Normal OnUpdate
		if ( !( (lHint >= UPD_PRIMARIES && lHint <= UPD_FREEMEASURES) || lHint == UPD_CC24SAT ) )
			CFormView::OnUpdate(pSender, lHint, pHint);

		if ( m_displayType == HCFR_SENSORRGB_VIEW )
		{
			m_displayType = HCFR_xyY_VIEW;
			CheckDlgButton(IDC_XYZ_RADIO, BST_CHECKED);  
			CheckDlgButton(IDC_SENSORRGB_RADIO, BST_UNCHECKED);  
		}

		GetDlgItem ( IDC_SENSORRGB_RADIO ) -> EnableWindow ( FALSE );


		if ( ( lHint >= UPD_EVERYTHING && lHint <= UPD_FREEMEASURES ) || lHint == UPD_ARRAYSIZES || lHint == UPD_GENERALREFERENCES || lHint == UPD_DATAREFDOC || lHint == UPD_REFERENCEDATA )
		{
			if ( lHint == UPD_EVERYTHING || lHint == UPD_ARRAYSIZES )
			{
				// Structural change (e.g. the grayscale point count changed): rebuild and
				// auto-fit the columns with redraw ENABLED, so the scroll bars and client
				// rect are settled before ExpandColumnsToFit measures the available width.
				// Sizing while redraw was off fit the columns to stale geometry, leaving the
				// last column clipped by the always-present vertical scroll bar. This mirrors
				// the OnSize path, which is why a manual window resize already corrected it.
				InitGrid(true);
				if(m_pGrayScaleGrid)
					UpdateGrid();
				if(m_SelectedColor.isValid())
					RefreshSelection(false,GetDocument()->GetMeasure()->m_binMeasure);
			}
			else
			{
				if (m_pGrayScaleGrid)
					m_pGrayScaleGrid->SetRedraw(FALSE);
				// Suppress intermediate repaints while InitGrid tears the grid down and UpdateGrid refills it
				// (otherwise it blanks to white between the two during a measurement update); repaint once below.
				InitGrid();
				if(m_pGrayScaleGrid)
					UpdateGrid();
				if(m_SelectedColor.isValid())
					RefreshSelection(false,GetDocument()->GetMeasure()->m_binMeasure);
				if (m_pGrayScaleGrid)
				{
					m_pGrayScaleGrid->SetRedraw(TRUE, TRUE);
					m_pGrayScaleGrid->Invalidate(FALSE);
				}
			}
		}
		
		if ( lHint == UPD_FREEMEASUREAPPENDED )
		{
			if ( m_displayMode == 0 )
			{
				// Gray/Primary colors may have been updated during free measures
				if (last_minCol >= 1 && MeasuredColor.isValid()) // ( (MeasuredColor.GetDeltaxy( GetColorReference().GetWhite(), GetColorReference()) < 0.03) || last_minCol == 1 ) )
				{
					GetDocument()->GetMeasure()->SetGray(last_minCol - 1, MeasuredColor);
					if (last_minCol == 1)
						GetDocument()->GetMeasure()->SetOnOffBlack(MeasuredColor);
					if (last_minCol == GetDocument()->GetMeasure()->GetGrayScaleSize ()    )
						GetDocument()->GetMeasure()->SetOnOffWhite(MeasuredColor);
				}
			}
//			UpdateGrid();
			UpdateMeasurementsAfterBkgndMeasure ();
		}

		if ( lHint == UPD_EVERYTHING || lHint == UPD_CONTRAST )
		{
			// TODO: Check this
			dContrast = GetDocument()->GetMeasure()->GetOnOffContrast ();


			dContrast = GetDocument()->GetMeasure()->GetAnsiContrast ();

			double dMinLum;
			dMinLum = GetDocument()->GetMeasure()->GetContrastMinLum ();

			double dMaxLum;
			dMaxLum = GetDocument()->GetMeasure()->GetContrastMaxLum ();
		}

		if ( lHint == UPD_EVERYTHING || lHint == UPD_SENSORCONFIG )
		{
			m_sensorName=GetDocument()->m_pSensor->GetName();
			if (::IsWindow(m_avgLowLightCheck.GetSafeHwnd()))
			{
				CSensor* pAvgS = GetDocument()->m_pSensor;
				BOOL bAvgSup = (pAvgS != NULL && pAvgS->supportsAvg());
				m_avgLowLightCheck.EnableWindow(bAvgSup);
				m_avgLowLightCheck.SetCheck((bAvgSup && pAvgS->getAvgEnabled()) ? BST_CHECKED : BST_UNCHECKED);
			}
		}

		if ( lHint == UPD_EVERYTHING || lHint == UPD_GENERATORCONFIG || lHint == UPD_GRAYSCALE )
		{
			CString dName,tName=GetDocument()->GetGenerator()->GetName(),gName;
			gName.LoadString(IDS_GDIGENERATOR_NAME);
			int d = GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE);
			if ( tName == gName)
			{
				switch (d)
				{
					case 0:
						dName = "Fullscreen";
						break;
					case 3:
						dName = "Overlay";
						break;
					case 2:
						dName = "madVR";
						break;
					case 4:
						dName = "Google Cast";
						break;
					case 5:
						dName = "Window";
						break;
						case 6:
						dName = "PGenerator";
						break;
				}
				m_generatorName.SetString(dName);
			}
			else
				m_generatorName.LoadString(IDS_MANUALDVDGENERATOR_NAME);

			m_testAnsiPatternButton.EnableWindow(GetDocument()->m_pGenerator->CanDisplayAnsiBWRects());
		}

		if ( lHint >= UPD_EVERYTHING && lHint <= UPD_FREEMEASURES )
		{
			// Ki : if current dataset is reference measure, update all views of all documents except current.
			if (GetDataRef() == GetDocument()) 
			{

				CDocEnumerator docEnumerator;
				CDocument* pDoc;
				while ((pDoc=docEnumerator.Next())!=NULL) 
				{
					if (GetDataRef() != pDoc)
						pDoc->UpdateAllViews(NULL, UPD_REFERENCEDATA);
				}
//				RefreshSelection (FALSE);
//				AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
			}
		}

//		if ( lHint == UPD_SELECTEDCOLOR )
//		{
//			RefreshSelection ();
//		}

		// Change background color
		if ( lHint == UPD_EVERYTHING || lHint == UPD_DATAREFDOC )
		{
			if (GetDataRef() == GetDocument()) {

				m_grayScaleGroup.SetHilighted(1);
				m_sensorGroup.SetHilighted(1);
				m_generatorGroup.SetHilighted(1);
				m_datarefGroup.SetHilighted(1);
				m_displayGroup.SetHilighted(1);
				m_paramGroup.SetHilighted(1);
				m_selectGroup.SetHilighted(1);
				m_viewGroup.SetHilighted(1);

				CDocEnumerator docEnumerator;
				CDocument* pDoc;
				while ((pDoc=docEnumerator.Next())!=NULL) {
					if (GetDataRef() != pDoc)
						pDoc->UpdateAllViews(NULL, UPD_REFERENCEDATA);
				}
//				RefreshSelection ();
//				AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
			}
			else 
			{
				if ( GetDocument()->m_pSensor->IsCalibrated() == 1 ) 
				{ 
					m_grayScaleGroup.SetHilighted(2);
					m_sensorGroup.SetHilighted(2);
					m_generatorGroup.SetHilighted(2);
					m_datarefGroup.SetHilighted(2);
					m_displayGroup.SetHilighted(2);
					m_paramGroup.SetHilighted(2);
					m_selectGroup.SetHilighted(2);
					m_viewGroup.SetHilighted(2);
				}
				else
				{
					m_grayScaleGroup.SetHilighted(0);
					m_sensorGroup.SetHilighted(0);
					m_generatorGroup.SetHilighted(0);
					m_datarefGroup.SetHilighted(0);
					m_displayGroup.SetHilighted(0);
					m_paramGroup.SetHilighted(0);
					m_selectGroup.SetHilighted(0);
					m_viewGroup.SetHilighted(0);
				}
			}

			//Update checkbox value
			if (GetDataRef() == GetDocument())
				m_datarefCheckButton = TRUE;
			else
				m_datarefCheckButton = FALSE;
		}
		
		m_AdjustXYZCheckButton.EnableWindow ( GetDocument()->m_pSensor->IsCalibrated () > 0 );
		m_AdjustXYZCheckButton.SetCheck ( GetDocument()->m_pSensor->IsCalibrated () == 1 );

		if ( GetDocument()->m_pSensor->IsCalibrated () == 1 || m_displayType == HCFR_xyz2_VIEW )
		{
			if ( m_editCheckButton.GetCheck () )
			{
				m_editCheckButton.SetCheck(FALSE);
				OnEditgridCheck ();
			}
			m_editCheckButton.EnableWindow(FALSE);
		}
		else
		{
			m_editCheckButton.EnableWindow(TRUE);
		}

		UpdateData(FALSE);

		if (m_bPositionsInit) { LayoutTopRow(); OnSize(0,0,0); }   // re-fit the gen/sensor panes to the new labels
			if (::IsWindow(m_statsBar.GetSafeHwnd())) m_statsBar.Invalidate(FALSE);   // refresh the bar-drawn Edit checkbox (enabled/checked state)
	}
}


CString CMainView::GetItemText(CColor & aMeasure, double YWhite, CColor & aReference, CColor & aRefDocColor, double YWhiteRefDoc, int aComponentNum, int nCol, double Offset, bool isGS)
{
	CString str;
	BOOL isHDR = ( GetConfig()->m_GammaOffsetType == 5 && (m_displayMode == 1 || m_displayMode >= 5 && m_displayMode <= 11) );
	//Special case White redefined on Mascior disk to level 502 50.0% 92.254965 nits
	BOOL DVD = (GetConfig()->GetGeneratorType() == CColorHCFRConfig::enumManual);
	CColorReference  bRef = ((GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4)?ContainerTransportReference(GetColorReference()):(GetColorReference().m_standard == HDTVa || GetColorReference().m_standard == HDTVb)?CColorReference(HDTV):GetColorReference());
	
	if(aMeasure.isValid() || ( aComponentNum == 7 || ( aComponentNum == 5 && ( GetDataRef() == NULL || GetDataRef() == GetDocument () ) ) ))
	{
		if ( aComponentNum < 3 )
		{
			switch(m_displayType)
			{
				case HCFR_SENSORRGB_VIEW:
				case HCFR_XYZ_VIEW:
    					str.Format("%.3f",aMeasure.GetXYZValue()[aComponentNum]);
					break;
				case HCFR_RGB_VIEW:
					str.Format("%.3f",aMeasure.GetRGBValue(bRef)[aComponentNum]);
					break;
				case HCFR_xyz2_VIEW:
					if (aMeasure.GetY() == 0)
						str.Format("%s","nan");
					else
						str.Format("%.4f",aMeasure.GetxyzValue()[aComponentNum]);
					break;
				case HCFR_xyY_VIEW:
                    if (aComponentNum < 2)
						if (aMeasure.GetY() == 0)
							str.Format("%s","nan");
						else
    						str.Format("%.4f",aMeasure.GetxyYValue()[aComponentNum]);
                    else
						if (aMeasure.GetY() == 0)
							str.Format("%s","0");
						else
	    					str.Format("%.3f",aMeasure.GetxyYValue()[aComponentNum]);
					break;
			}
			if ( str == "-99999.990" ) // Printed FX_NODATA value, coming from partially updated noDataColor
				str.Empty ();
		}
		else if ( aComponentNum == 3 )
		{
			COLORREF clr;
			double dE, dL, dC, dH;
			if ( aReference.isValid() )
			{
				if (m_displayMode == 0 || (m_displayMode == 2 && isGS) || m_displayMode == 3 || m_displayMode == 4 )
				{
					if ( nCol > 1 || m_displayMode == 4 || m_displayMode == 2 )
					{
						double Intensity=GetConfig()->GetProfileInt("GDIGenerator","Intensity",100) / 100.;
						str.Format("%.1f",aMeasure. GetDeltaE( YWhite, aReference, 1.0, GetColorReference(), GetConfig()->m_dE_form, true, GetConfig()->m_GammaOffsetType == 5?3:GetConfig()->gw_Weight ) );
       					dE=aMeasure.GetDeltaE ( YWhite, aReference, 1.0, GetColorReference(), GetConfig()->m_dE_form, true, GetConfig()->m_GammaOffsetType == 5?3:GetConfig()->gw_Weight );
       					dL=aMeasure.GetDeltaLCH ( YWhite, aReference, 1.0, GetColorReference(), GetConfig()->m_dE_form, true, GetConfig()->m_GammaOffsetType == 5?3:GetConfig()->gw_Weight, dC, dH );
						dEavg+=(isNan(dE)?dEavg:dE);
						dLavg+=(isNan(dL)?dLavg:dL);
						dCavg+=(isNan(dC)?dCavg:dC);
						dHavg+=(isNan(dH)?dHavg:dH);

						if (nCol == minCol)
						{
							m_RefColor = refColor_for_color_comp; //make sure we use "w/gamma" reference
							m_RefWhite = 1.0;
							m_YWhite = YWhite_for_color_comp;
							m_dE = dE;
						}

						if (dE > dEmax)
                            dEmax = dE;
						clr = GetConfig()->GetDEColor(dE, GetConfig()->m_darkTheme);
						// nCol is -1 for a free measure (see the else below), and the
						// guard above admits mode 2/4 REGARDLESS of nCol - so without
						// this all three calls addressed column -1. GridCtrl returns
						// FALSE on the missing cell, so Release silently skipped the
						// highlight; Debug tripped SetItemFont's ASSERT(pCell).
						if ( nCol >= 1 )
						{
							if (GetConfig()->doHighlight)
								{ m_pGrayScaleGrid->SetItemBkColour(4, nCol, clr); m_pGrayScaleGrid->SetItemFgColour(4, nCol, RGB(0,0,0)); }
							m_pGrayScaleGrid -> SetItemFont ( 4, nCol, m_pGrayScaleGrid->GetItemFont(0,0) ); // Set the font to bold
						}
						dEcnt++;
					}
					else
					{//black or free measure nCol = -1
						if (minCol == -1)
							m_RefColor = noDataColor;
						else if (nCol == 1)
						{
							m_RefColor[0] = 0;
							m_RefColor[1] = 0;
							m_RefColor[2] = 0;
						}

						str.Empty ();
					}
				}
				else
				{
					double dL, dH, dC, RefWhite = 1.0;
					CColorReference cRef = GetColorReference();
					int satsize=GetDocument()->GetMeasure()->GetSaturationSize();
					if ( isHDR )
					{
						bool shiftDiffuse = (abs(GetConfig()->m_DiffuseL-94.0)>0.5);
			            CColor White = GetDocument() -> GetMeasure () -> GetOnOffWhite();
						CColor Black = GetDocument() -> GetMeasure() -> GetOnOffBlack();
						double tmWhite = TmDiffuseWhiteNits(White, Black);
						if (DVD)
						{
							tmWhite = getL_EOTF(0.50, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, 5, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) * 100.0;
							if (m_displayMode == 1)
							{
								if ( (cRef.m_standard == UHDTV2 || cRef.m_standard == HDTV || cRef.m_standard == UHDTV || cRef.m_standard == UHDTV3 || cRef.m_standard == UHDTV4 || nCol == 7) ) //fix for P3/Mascior
									RefWhite = YWhite / (!shiftDiffuse?92.254965:tmWhite);
								else
								{
									RefWhite = YWhite / (tmWhite);
									YWhite = YWhite * 94.37844 / (tmWhite);
								}
							}
							else
							{
								if ( ((cRef.m_standard == UHDTV2 && nCol == satsize ) || cRef.m_standard == HDTV || cRef.m_standard == UHDTV)  && m_displayMode != 11)// && !shiftDiffuse) //fixes skin && nCol == satsize
									RefWhite = YWhite / (tmWhite);
								else
								{
									RefWhite = YWhite / (tmWhite) ;
									YWhite = YWhite * 94.37844 / (tmWhite);
								}
							}
						}
						else
						{
							if (m_displayMode == 1)
							{
								if (cRef.m_standard == UHDTV2 || cRef.m_standard == HDTV || cRef.m_standard == UHDTV || cRef.m_standard == UHDTV3 || cRef.m_standard == UHDTV4 || nCol == 7)
									RefWhite = YWhite / (tmWhite) ;
								else
								{
									RefWhite = YWhite / (tmWhite) ;					
									YWhite = YWhite * 94.37844 / (tmWhite) ;
								}
							}
							else
							{
								if (GetConfig()->m_CCMode >= MASCIOR50 && GetConfig()->m_CCMode <= CCMAXHDR && m_displayMode == 11)
									YWhite = GetDocument()->GetMeasure()->GetGray((GetDocument()->GetMeasure()->GetGrayScaleSize()-1)).GetY() ;
								else
								{
									// Unified HDR convention: the reference is scaled by
									// GetHDRRefScale (UpdateGrid ~4294), so the measurement
									// stays anchored to the measured white as-is - no
									// 94.37844/tmWhite rescale (matches the 3D viewer;
									// identical with tone mapping off).
									RefWhite = YWhite / (tmWhite) ;
								}
							}
						}
					}

					CColorReference  bRef = ((GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4)?ContainerTransportReference(GetColorReference()):(GetColorReference().m_standard == HDTVa || GetColorReference().m_standard == HDTVb)?CColorReference(HDTV):GetColorReference());

					str.Format("%.1f",aMeasure.GetDeltaE ( YWhite, aReference, RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->m_GammaOffsetType == 5?3:GetConfig()->gw_Weight ) );
					dE=aMeasure.GetDeltaE ( YWhite, aReference, RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->m_GammaOffsetType == 5?3:GetConfig()->gw_Weight );
					dL=aMeasure.GetDeltaLCH ( YWhite, aReference, RefWhite, bRef, GetConfig()->m_dE_form, false, GetConfig()->m_GammaOffsetType == 5?3:GetConfig()->gw_Weight, dC, dH );
                    dEvector.push_back(isNan(dE)?dEavg:dE);
                    dLvector.push_back(isNan(dL)?dLavg:dL);
                    dCvector.push_back(isNan(dC)?dCavg:dC);
                    dHvector.push_back(isNan(dH)?dHavg:dH);
                    dEavg+=(isNan(dE)?dEavg:dE);
					dLavg+=(isNan(dL)?dLavg:dL);
					dCavg+=(isNan(dC)?dCavg:dC);
					dHavg+=(isNan(dH)?dHavg:dH);

					// Per-column cache backing the incremental sweep path's summary
					// re-aggregation (UpdateGrid). Valid dE only: a NaN dE (broken
					// reading) stays at the -1 sentinel and is excluded there, where
					// the classic accumulation above pushed a garbage running sum.
					if ( m_displayMode == 11 && nCol >= 1 && nCol <= (int)m_ccDECache.size() && !isNan(dE) )
					{
						m_ccDECache[nCol-1] = dE;
						// a NaN component with a finite dE (degenerate XYZ under some
						// dE forms) must not poison the re-aggregated averages into
						// "nan"; contribute zero for that component instead
						m_ccDLCache[nCol-1] = isNan(dL) ? 0.0 : dL;
						m_ccDCCache[nCol-1] = isNan(dC) ? 0.0 : dC;
						m_ccDHCache[nCol-1] = isNan(dH) ? 0.0 : dH;
					}

					if (nCol == minCol)
					{
						m_RefColor = aReference;
						m_RefWhite = RefWhite;
						m_YWhite = YWhite;
						m_dE = dE;
					}

					if (dE > dEmax)
                        dEmax = dE;
					clr = GetConfig()->GetDEColor(dE, GetConfig()->m_darkTheme);
					// Same out-of-range guard as the grayscale branch above.
					if ( nCol >= 1 )
					{
						if (GetConfig()->doHighlight)
							{ m_pGrayScaleGrid->SetItemBkColour(4, nCol, clr); m_pGrayScaleGrid->SetItemFgColour(4, nCol, RGB(0,0,0)); }
						m_pGrayScaleGrid -> SetItemFont ( 4, nCol, m_pGrayScaleGrid->GetItemFont(0,0) ); // Set the font to bold
					}
					dEcnt++;
				}
			}
			else
				str.Empty ();
		}
		else if ( aComponentNum == 4 )
		{
			if ( aReference.isValid() && (nCol > 1 || ( m_displayMode != 0 && m_displayMode != 3)) )
				str.Format("%.4f",aMeasure.GetDeltaxy ( aReference, bRef) );
			else
				str.Empty ();
		}
		else if ( aComponentNum == 5 && (nCol > 1 || ( m_displayMode != 0 && m_displayMode != 3)) )
		{
			if ( aRefDocColor.isValid() )
			{
//					if (nCol == minCol) this would substitute refdoc for colorcomparator targets, not sure we want to do that
//					{
//						m_RefColor = aRefDocColor;
//						m_RefWhite = YWhiteRefDoc;
//						m_YWhite = YWhite;
//					}
					str.Format("%.1f",aMeasure.GetDeltaE ( YWhite, aRefDocColor, YWhiteRefDoc, bRef, GetConfig()->m_dE_form, m_displayMode == 0 || m_displayMode == 3 || m_displayMode == 4, GetConfig()->gw_Weight ) );
			}
			else
				str.Empty ();
		}
		else if ( aComponentNum == 6 )
		{
			if ( aRefDocColor.isValid() && (nCol > 1 || ( m_displayMode != 0 && m_displayMode !=3)) )
				str.Format("%.4f",aMeasure.GetDeltaxy ( aRefDocColor, bRef) );
			else
				str.Empty ();
		}
		else
			str.Empty ();

		if ( (aComponentNum == 7 || ( aComponentNum == 5 && ( GetDataRef() == NULL || GetDataRef() == GetDocument () ) )) || (aComponentNum == 8 || ( aComponentNum == 6 && ( GetDataRef() == NULL || GetDataRef() == GetDocument () ) )) )
		{
			if ( m_displayMode == 0 || m_displayMode == 3 || m_displayMode == 4 )
			{
				// Display reference Y
				str.Empty();

				int nGrayScaleSize = GetDocument()->GetMeasure()->GetGrayScaleSize ();
				int size=GetDocument()->GetMeasure()->GetNearBlackScaleSize();
				if (m_displayMode == 4)
					size=GetDocument()->GetMeasure()->GetNearWhiteScaleSize();

				if ( nCol >= 1 && nCol <= ((m_displayMode == 0)?nGrayScaleSize:size) )
				{
					CColor White = GetDocument()->GetMeasure()->GetOnOffWhite();
					CColor Black;
					if (m_displayMode != 3)
						Black = GetDocument()->GetMeasure()->GetOnOffBlack();
					else
						Black = GetDocument()->GetMeasure()->GetNearBlack(0);

					if ( White.isValid() && Black.isValid())
					{
						double x, valx, valy;
						double yblack = 0.0;
						BOOL bIRE = GetDocument()->GetMeasure()->m_bIREScaleMode;
						
						if ( GetConfig ()->m_GammaOffsetType == 1 )
							yblack = Black.GetY();
						if (m_displayMode == 0)
							x = GetDocument()->GetMeasure()->GetGrayPercent ( nCol - 1, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235() );
						else if (m_displayMode == 3)
							valx = GrayLevelToGrayProp ( (double)(nCol - 1)*(GetConfig()->m_GammaOffsetType==5?2:1), GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235() );
						else if (m_displayMode == 4)
							valx = GrayLevelToGrayProp ( (double)(nCol - 1 + GetDocument()->GetMeasure()->m_NearWhiteClipCol - size) , GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235() );

						int mode = GetConfig()->m_GammaOffsetType;
						if (GetConfig()->m_colorStandard == sRGB) mode = 99;

						if ( mode >= 4 )
						{
							if (m_displayMode == 0)
								valx = GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());
							
							if (mode == 5)
							{
								valy = getL_EOTF(valx,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) * 100.;
							}
							else
	                            valy = getL_EOTF(valx,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) * White.GetY();

							str.Format ( "%.3f", valy );
						}
						else
						{
							if (m_displayMode == 0)
		    					valx=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235())+Offset)/(1.0+Offset);
	    					valy=pow(valx, GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef));
		    				str.Format ( "%.3f", yblack + ( valy * ( White.GetY () - yblack ) ) );
						}
					}
				}
			}
			else
			{
				// Display primary/secondary/saturations colors delta luminance
				int	    nCol2 = nCol, satsize=GetDocument()->GetMeasure()->GetSaturationSize();
				double  RefLuma [MAX_USER_CC_PATCH_SIZE], sat=double (nCol-1)/ double (satsize-1);
                CColor White = GetDocument() -> GetMeasure () -> GetOnOffWhite();
//	            CColor Black = GetDocument() -> GetMeasure () -> GetGray ( 0 );
	            CColor Black = GetDocument() -> GetMeasure () -> GetOnOffBlack();
				CColor satcolor;
				// Retrieve color luminance coefficients matching actual reference

				switch (m_displayMode)
				{
					case 1: 
						RefLuma [ 0 ] = GetDocument()->GetMeasure()->GetRefPrimary(0).GetLuminance();
						RefLuma [ 1 ] = GetDocument()->GetMeasure()->GetRefPrimary(1).GetLuminance();
						RefLuma [ 2 ] = GetDocument()->GetMeasure()->GetRefPrimary(2).GetLuminance();
						RefLuma [ 3 ] = GetDocument()->GetMeasure()->GetRefSecondary(0).GetLuminance();
						RefLuma [ 4 ] = GetDocument()->GetMeasure()->GetRefSecondary(1).GetLuminance();
						RefLuma [ 5 ] = GetDocument()->GetMeasure()->GetRefSecondary(2).GetLuminance();
						RefLuma [ 6 ] = 1.0;
						break ;
					// The six saturation sweeps differ only in the GetRefSat hue
					// index (displayMode 5..10 -> hue 0..5); one arm keeps the HDR
					// scale in a single place instead of six copies to keep in sync.
					case 5:
					case 6:
					case 7:
					case 8:
					case 9:
					case 10:
						satcolor = GetDocument()->GetMeasure()->GetRefSat(m_displayMode - 5, sat, (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
						if (GetConfig()->m_GammaOffsetType == 5)
		                    RefLuma [nCol - 1] = satcolor.GetLuminance() * (DVD ? 105.95640 : GetDocument()->GetMeasure()->GetHDRRefScale());
						else
		                    RefLuma [nCol - 1] = satcolor.GetLuminance();
						break;
					case 11:
	                        RefLuma [nCol -1] = aReference.GetLuminance();
						break;
				}

				CColor white = GetDocument()->GetMeasure()->GetPrimeWhite();

				if (!white.isValid() && isHDR)
					white = GetDocument()->GetMeasure()->GetGray((GetDocument()->GetMeasure()->GetGrayScaleSize()-1) / 2 );

				if (!white.isValid() || m_displayMode == 0 || m_displayMode == 2 || m_displayMode == 3 || m_displayMode == 4)
					white = GetDocument() -> GetMeasure () ->GetOnOffWhite();

				if ( m_displayMode > 4 && (GetConfig()->m_colorStandard == HDTVa || GetConfig()->m_colorStandard == HDTVb ))
					white = GetDocument() -> GetMeasure () ->GetOnOffWhite();

				//special case check if user has done a primaries run at less than 100%, use grayscale white instead for colorchecker
				if (GetDocument()->GetMeasure()->GetOnOffWhite().isValid())
					if ((GetDocument()->GetMeasure()->GetPrimeWhite()[1] / GetDocument()->GetMeasure()->GetOnOffWhite()[1] < 0.9) && m_displayMode == 11  && GetConfig()->m_GammaOffsetType !=5)
						white = GetDocument() -> GetMeasure () ->GetOnOffWhite();
				
				if ( isHDR )
				{
					bool shiftDiffuse=(abs(GetConfig()->m_DiffuseL-94.0)>0.5);
					double tmWhite = TmDiffuseWhiteNits(White, Black);
					if (DVD)
					{
						tmWhite = getL_EOTF(0.50, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, 5, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) * 100.0;
						if (m_displayMode == 1)
						{
							if (GetColorReference().m_standard == UHDTV || GetColorReference().m_standard == UHDTV2 || GetColorReference().m_standard == HDTV || nCol == 7)
								white.SetY(!shiftDiffuse?92.254965:tmWhite);
							else
								white.SetY(94.37844);
						}
						else
						{
							if ( ( (GetColorReference().m_standard == UHDTV2 && nCol == satsize) || GetColorReference().m_standard == HDTV || GetColorReference().m_standard == UHDTV) && m_displayMode != 11)// && !shiftDiffuse) //&& nCol == (satsize)
								white.SetY(92.25496);
							else
								white.SetY(94.37844);
						}
					}
					else
					{
						if (m_displayMode == 1)
							if (GetColorReference().m_standard == UHDTV2 || GetColorReference().m_standard == HDTV || GetColorReference().m_standard == UHDTV || GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4 || nCol == 7)
								white.SetY(tmWhite);
							else
								white.SetY(94.37844);
						else
							if ((GetConfig()->m_CCMode >= MASCIOR50 && GetConfig()->m_CCMode <= CCMAXHDR) && m_displayMode == 11)
								white.SetY(GetDocument()->GetMeasure()->GetGray((GetDocument()->GetMeasure()->GetGrayScaleSize()-1)).GetY());
							else
								// Unified convention: references are GetHDRRefScale-scaled
								// (1.0 = tone-mapped diffuse white), so normalize the
								// measurement by the same white. The delta-luminance ratio
								// is unchanged (both sides reduce to meas / (ref * 10000)).
								white.SetY(tmWhite);
					}
				}

				if ( (nCol2 < ( (m_displayMode > 11 || m_displayMode < 5) ? 7 : (MAX_USER_CC_PATCH_SIZE+1)) || (nCol2 == 7 && isHDR) ) && white.isValid() && white.GetPreferedLuxValue(GetConfig () -> m_bPreferLuxmeter) > 0.0001 )
    		    {
					if (aMeasure.isValid() && aComponentNum != 8 && aComponentNum != 6 )
					{
						double d = aMeasure.GetPreferedLuxValue(GetConfig () -> m_bPreferLuxmeter) / white.GetPreferedLuxValue(GetConfig () -> m_bPreferLuxmeter);
						if ( fabs ( ( RefLuma [ nCol2 - 1 ] - d ) / RefLuma [ nCol2 - 1 ] ) < 0.001 )
							str = "=";
						else if ( d < RefLuma [ nCol2 - 1 ] )
							str.Format("-%.1f %%", 100.0 * ( RefLuma [ nCol2 - 1 ] - d ) / RefLuma [ nCol2 - 1 ] );
						else
							str.Format("+%.1f %%", 100.0 * ( d - RefLuma [ nCol2 - 1 ] ) / RefLuma [ nCol2 - 1 ] );
					}
					else if (aComponentNum == 8 || aComponentNum == 6)
						str.Format("%.1f",  white.GetPreferedLuxValue(GetConfig () -> m_bPreferLuxmeter) * ( RefLuma [ nCol2 - 1 ] ) );
				}
				else
					str.Empty();
			}
		}
	}
	else
		str.Empty ();

	return str;
}

LPSTR CMainView::GetGridRowLabel(int aComponentNum)
{
	switch(aComponentNum)
	{
		case 0:
			switch(m_displayType)
			{
				case HCFR_SENSORRGB_VIEW:
				case HCFR_XYZ_VIEW:
					return "X";
					break;
				case HCFR_RGB_VIEW:
					switch(GetConfig()->m_colorStandard)
					{
						case SDTV:
							return "R601";
							break;
						case UHDTV:
							return "RDCI-P3";
							break;
						case UHDTV2:
							return "R2020";
							break;
						case UHDTV3:
							return "R2020P3";
							break;
						case UHDTV4:
							return "R2020R709";
							break;
						case HDTV:
							return "R709";
							break;
						case sRGB:
							return "R709";
							break;
						case PALSECAM:
							return "Rpal";
							break;
						case HDTVa:
							return m_displayMode==1?"R709(75%)":"R709";
							break;
						case CC6:
							return m_displayMode==1?"RCC6":"R709";
							break;
						case HDTVb:
							return m_displayMode==1?"R709(OPT-Plasma)":"R709";
							break;
						case CUSTOM:
							return "RCUSTOM";
							break;
						default:
							return "R?";
							break;
					}
				break;
				case HCFR_xyY_VIEW:
				case HCFR_xyz2_VIEW:
					return "x";
					break;
				default:
					return "?";
			}
			break;
		case 1:
			switch(m_displayType)
			{
				case HCFR_SENSORRGB_VIEW:
				case HCFR_XYZ_VIEW:
					return "Y";
					break;
				case HCFR_RGB_VIEW:
					switch(GetConfig()->m_colorStandard)
					{
						case SDTV:
							return "G601";
							break;
						case HDTV:
							return "G709";
							break;
						case UHDTV:
							return "GDCI-P3";
							break;
						case UHDTV2:
							return "G2020";
							break;
						case UHDTV3:
							return "G2020P3";
							break;
						case UHDTV4:
							return "G2020R709";
							break;
						case sRGB:
							return "G709";
							break;
						case PALSECAM:
							return "Gpal";
							break;
						case HDTVa:
							return m_displayMode==1?"G709(75%)":"G709";
							break;
						case CC6:
							return m_displayMode==1?"GCC6":"GCC6";
							break;
						case HDTVb:
							return m_displayMode==1?"G709(OPT-Plasma)":"G709";
							break;
						case CUSTOM:
							return "GCUSTOM";
							break;
						default:
							return "G?";
							break;
					}
				break;
				case HCFR_xyY_VIEW:
				case HCFR_xyz2_VIEW:
					return "y";
					break;
				default:
					return "?";
			}
			break;
		case 2:
			switch(m_displayType)
			{
				case HCFR_SENSORRGB_VIEW:
				case HCFR_XYZ_VIEW:
					return "Z";
					break;

				case HCFR_RGB_VIEW:
					switch(GetConfig()->m_colorStandard)
					{
						case SDTV:
							return "B601";
							break;
						case HDTV:
							return "B709";
							break;
						case UHDTV:
							return "BDCI-P3";
							break;
						case UHDTV2:
							return "B2020";
							break;
						case UHDTV3:
							return "B2020P3";
							break;
						case UHDTV4:
							return "B2020R709";
							break;
						case sRGB:
							return "B709";
							break;
						case PALSECAM:
							return "Bpal";
							break;
						case HDTVa:
							return m_displayMode==1?"B709(75%)":"B709";
							break;
						case CC6:
							return m_displayMode==1?"B709(CC6)":"B709";
							break;
						case HDTVb:
							return m_displayMode==1?"B709(OPT-Plasma)":"B709";
							break;
						case CUSTOM:
							return "BCUSTOM";
							break;
						default:
							return "B?";
							break;
					}
				break;
				case HCFR_xyz2_VIEW:
					return "z";
					break;
				case HCFR_xyY_VIEW:
					return "Y";
					break;
				default:
					return "?";
			}
			break;

		case 3:

			return "Delta E";
			break;

		case 4:
			return "delta xy";
			break;

		case 5:
		 	if ( GetDataRef() && GetDataRef() != GetDocument () )
				return "dE / ref";
			else
				return ( m_displayMode == 0 || m_displayMode == 3 || m_displayMode == 4 ? "Y target" : "delta L" );
			break;

		case 6:
			return (m_displayMode == 0 || m_displayMode == 3 || m_displayMode == 4 ? "xy / ref":GetDataRef() && GetDataRef() != GetDocument ()?"xy / ref":"Y Target");
			break;

		case 7:
			return ( m_displayMode == 0 || m_displayMode == 3 || m_displayMode == 4 ? "Y target" : "delta L" );
			break;

		case 8:
			return  "Y target";
			break;
	}

	return "Undef";
}

void CMainView::UpdateGrid()
{
	// Single-shot incremental request from the realtime sweep path (OnUpdate);
	// consumed here so every other caller keeps full-rebuild semantics.
	int nIncrCol = m_nGridIncrCol;
	m_nGridIncrCol = -1;

	// display profile (mode 13): the grid is hidden and the pane owns that area,
	// so skip the grid population -- but still fall through to the View-pane info
	// line at the end of this function. That line must track reference / EOTF
	// changes; an early return here left it stale (e.g. showing HDR after an
	// HDR->SDR reference switch while the profile view was open).
	if (m_displayMode != 13 && m_pGrayScaleGrid)
	{
		CColor			aColor;
		CColor			refColor = GetColorReference().GetWhite();
		CColor			refDocColor = noDataColor;
		CColor			refLuxColor = noDataColor;
		BOOL			bAddedCol = FALSE;
		BOOL			bSpecialRef = FALSE;
		BOOL			bIRE = GetDocument()->GetMeasure()->m_bIREScaleMode;
		double			YWhiteOnOff = -1.0;
		double			YWhiteGray = -1.0;
		double			YWhitePrime = -1.0;
		double			YWhiteOnOffRefDoc = -1.0;
		double			YWhitePrimeRefDoc = -1.0;
		double			YWhiteGrayRefDoc = -1.0;
		double			YWhite = -1.0;
		double			YWhiteRefDoc = -1.0;
		double			Gamma,Offset = 0.0;
		COLORREF		clrSpecial1=RGB(128,128,128), clrSpecial2=RGB(128,128,128);
		CDataSetDoc *	pDataRef = GetDataRef();
		GV_ITEM Item;
		Item.mask = GVIF_TEXT|GVIF_FORMAT;
		Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
		bool isHDR = GetConfig()->m_GammaOffsetType == 5;
		CColorReference  bRef = ((GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4)?ContainerTransportReference(GetColorReference()):(GetColorReference().m_standard == HDTVa || GetColorReference().m_standard == HDTVb)?CColorReference(HDTV):GetColorReference());
		GetConfig()->WriteProfileInt("MainView","Chart Display",m_displayMode);


		if  (m_userBlack)
		{
			double Yblack = GetConfig()->GetProfileDouble("References","Manual Black Level",0);
			CColor BlackColor = GetColorReference().GetWhite();
			BlackColor[0] = BlackColor[0] * Yblack;
			BlackColor[1] = BlackColor[1] * Yblack;
			BlackColor[2] = BlackColor[2] * Yblack;
			GetDocument()->GetMeasure()->SetGray(0, BlackColor);
			GetDocument()->GetMeasure()->SetNearBlack(0, BlackColor);
			GetDocument()->GetMeasure()->SetOnOffBlack(BlackColor);
		}

		// update values
		int nRows = 5;
		int	nCount;
		BOOL bHasLuxValues = FALSE;
		BOOL bHasLuxDelta = FALSE;
					
		dEavg = 0.0, dEmax=0.0;
		dEcnt = 0; dE10=0.0;
		dLavg = 0.0, dCavg = 0.0; dHavg = 0.0;

		if ( pDataRef == GetDocument () )
			pDataRef = NULL;

		// Retrieve measured white luminance to compute exact delta E, Lab and LCH values
		if ( GetDocument() -> GetMeasure () -> GetOnOffWhite ().isValid() )
			YWhiteOnOff = GetDocument() -> GetMeasure () -> GetOnOffWhite () [ 1 ];
		if ( GetDocument() -> GetMeasure () -> GetPrimeWhite ().isValid() )
			YWhitePrime = GetDocument() -> GetMeasure () -> GetPrimeWhite () [ 1 ];
		else
			YWhitePrime = YWhiteOnOff;

		nCount = GetDocument() -> GetMeasure () -> GetGrayScaleSize ();
		if ( GetDocument() -> GetMeasure () -> GetGray ( nCount - 1 ).isValid() )
			YWhiteGray = GetDocument() -> GetMeasure () -> GetGray ( nCount - 1 ) [ 1 ];

		if (YWhite == -1)
			YWhite = GetConfig()->m_TargetMaxL;
		if (YWhitePrime == -1)
			YWhitePrime = GetConfig()->m_TargetMaxL;
		if (YWhiteOnOff == -1)
			YWhiteOnOff = GetConfig()->m_TargetMaxL;
		if (YWhiteGray == -1)
			YWhiteGray = GetConfig()->m_TargetMaxL;

		if ( pDataRef )
		{
			int refCount;
			if ( pDataRef -> GetMeasure () -> GetOnOffWhite ().isValid() )
				YWhiteOnOffRefDoc = pDataRef -> GetMeasure () -> GetOnOffWhite () [ 1 ];
			if ( pDataRef -> GetMeasure () -> GetPrimeWhite ().isValid() )
				YWhitePrimeRefDoc = pDataRef -> GetMeasure () -> GetPrimeWhite () [ 1 ];

			refCount = pDataRef -> GetMeasure () -> GetGrayScaleSize ();
			if (refCount == nCount)
			{
				nCount = pDataRef -> GetMeasure () -> GetGrayScaleSize ();
				if ( pDataRef -> GetMeasure () -> GetGray ( nCount - 1 ).isValid() )
					YWhiteGrayRefDoc = pDataRef -> GetMeasure () -> GetGray ( nCount - 1 ) [ 1 ];
			}

		}

		// Retrieve gamma and offset in case user has modified
        Gamma = GetConfig()->m_GammaRef;
        GetConfig()->m_GammaAvg = Gamma; //targets can be reference power law or modified for user average gamma, BT.1886 handled separately
        if ( nCount && GetDocument()->GetMeasure()->GetGray(0).isValid() )
            GetDocument()->ComputeGammaAndOffset(&Gamma, &Offset, 1, 1, nCount, false);

        if (GetConfig()->m_useMeasuredGamma)
			GetConfig()->m_GammaAvg = (Gamma<1?2.2:floor((Gamma+.005)*100.)/100.);

        GetConfig()->SetPropertiesSheetValues();

		BOOL isExtPat =( GetConfig()->m_CCMode == USER || GetConfig()->m_CCMode == CM10SAT || GetConfig()->m_CCMode == CM10SAT75 || GetConfig()->m_CCMode == CM5SAT || GetConfig()->m_CCMode == CM5SAT75 || GetConfig()->m_CCMode == CM4SAT || GetConfig()->m_CCMode == CM4SAT75 || GetConfig()->m_CCMode == CM4LUM || GetConfig()->m_CCMode == CM5LUM || GetConfig()->m_CCMode == CM10LUM || GetConfig()->m_CCMode == RANDOM250 || GetConfig()->m_CCMode == RANDOM500 || GetConfig()->m_CCMode == CM6NB || GetConfig()->m_CCMode == CMDNR || GetConfig()->m_CCMode == MASCIOR50);
		isExtPat = (isExtPat || GetConfig()->m_CCMode > 19);

		switch ( m_displayMode )
		{
			case 0:
				 YWhite = YWhiteGray;
				 YWhiteRefDoc = YWhiteGrayRefDoc;
				 nCount = GetDocument()->GetMeasure()->GetGrayScaleSize();
				 if ( pDataRef && pDataRef->GetMeasure()->GetGrayScaleSize() != nCount )
					pDataRef = NULL;

				 if ( pDataRef && pDataRef->GetMeasure()->m_bIREScaleMode != bIRE )
					pDataRef = NULL;

				 if ( nCount )
					bHasLuxValues = GetDocument()->GetMeasure()->GetGray(0).HasLuxValue ();
				 
				 bHasLuxDelta = bHasLuxValues;
				 if ( bHasLuxDelta )
					refLuxColor = GetDocument()->GetMeasure()->GetGray(nCount-1);

				 break;

 			case 1:
  				YWhite = YWhitePrime;
				YWhiteRefDoc = YWhitePrimeRefDoc;
				 nCount = 8;
				 bHasLuxValues = GetDocument()->GetMeasure()->GetRedPrimary().HasLuxValue ();
				 if ( bHasLuxValues )
				 {
					if ( GetDocument()->GetMeasure()->GetOnOffWhite().isValid() )
					{
						bHasLuxDelta = TRUE;
						refLuxColor = GetDocument()->GetMeasure()->GetOnOffWhite();
					}
				 }
				 break;

			case 2:
				 nCount = GetDocument()->GetMeasure()->GetMeasurementsSize();
				 YWhite = YWhiteGray;
				 YWhiteRefDoc = YWhiteGrayRefDoc;
				 if ( pDataRef && pDataRef->GetMeasure()->GetMeasurementsSize() != nCount )
					 pDataRef = NULL;
				 break;

			case 3:
				 YWhite = YWhiteGray;
				 YWhiteRefDoc = YWhiteGrayRefDoc;
				 nCount = GetDocument()->GetMeasure()->GetNearBlackScaleSize();
				 if ( pDataRef && pDataRef->GetMeasure()->GetNearBlackScaleSize() != nCount )
					pDataRef = NULL;

				 if ( nCount )
					bHasLuxValues = GetDocument()->GetMeasure()->GetNearBlack(0).HasLuxValue ();
				 break;

			case 4:
				 YWhite = YWhiteGray;
				 YWhiteRefDoc = YWhiteGrayRefDoc;
				 nCount = GetDocument()->GetMeasure()->GetNearWhiteScaleSize();
				 if ( pDataRef && pDataRef->GetMeasure()->GetNearWhiteScaleSize() != nCount )
					pDataRef = NULL;

				 if ( nCount )
					bHasLuxValues = GetDocument()->GetMeasure()->GetNearWhite(0).HasLuxValue ();
				 break;

			case 12:
				 nCount = 4;
				 nRows = 3;
				 pDataRef = NULL;
				 bHasLuxValues = GetDocument()->GetMeasure()->GetOnOffWhite().HasLuxValue ();
				 break;

			default:
				 bool isSpecial = (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb);
				 YWhite = isSpecial?YWhiteOnOff:YWhitePrime;
				 YWhiteRefDoc = isSpecial?YWhiteOnOffRefDoc:YWhitePrimeRefDoc;

				 //special case check if user has done a less than 100% primaries run and use grayscale white instead for colorchecker
				if (GetDocument()->GetMeasure()->GetOnOffWhite().isValid()&&!isHDR)
				{
					if ((YWhitePrime / YWhiteOnOff < 0.9) && m_displayMode == 11)
					{
						YWhite = YWhiteOnOff;
						YWhiteRefDoc = YWhiteOnOffRefDoc;
					}
				}

				 if (m_displayMode != 11) 
				 {
					 nCount = GetDocument()->GetMeasure()->GetSaturationSize();
					 if ( pDataRef && pDataRef->GetMeasure()->GetSaturationSize() != nCount )
						 pDataRef = NULL;
				 }
				 else
				 {
                     if (isExtPat)
                         nCount = GetConfig()->GetCColorsSize();
                     else
                        nCount = GetConfig()->m_CCMode==CCSG?96:GetConfig()->m_CCMode==CMS||GetConfig()->m_CCMode==CPS?19:(GetConfig()->m_CCMode==AXIS?71:24);
				 }
				 
				 if ( nCount )
				 {
					switch ( m_displayMode )
					{
						case 5:
							 bHasLuxValues = GetDocument()->GetMeasure()->GetRedSat(0).HasLuxValue ();
							 break;

						case 6:
							 bHasLuxValues = GetDocument()->GetMeasure()->GetGreenSat(0).HasLuxValue ();
							 break;

						case 7:
							 bHasLuxValues = GetDocument()->GetMeasure()->GetBlueSat(0).HasLuxValue ();
							 break;

						case 8:
							 bHasLuxValues = GetDocument()->GetMeasure()->GetYellowSat(0).HasLuxValue ();
							 break;

						case 9:
							 bHasLuxValues = GetDocument()->GetMeasure()->GetCyanSat(0).HasLuxValue ();
							 break;

						case 10:
							 bHasLuxValues = GetDocument()->GetMeasure()->GetMagentaSat(0).HasLuxValue ();
							 break;

						case 11:
							 bHasLuxValues = GetDocument()->GetMeasure()->GetCC24Sat(0).HasLuxValue ();
							 break;
					}
				 }
				 break;
		}
		
		if ( pDataRef )
			nRows = 7;

		if ( m_displayMode <= 1 || (m_displayMode >= 3 && m_displayMode <= 11) )
			nRows ++;

		if ( m_displayMode == 1 || (m_displayMode >= 5 && m_displayMode <= 11) )
			nRows ++;

		if ( bHasLuxValues )
		{
			if ( bHasLuxDelta )
				nRows += 2;
			else
				nRows ++;
		}
        
		dEvector.clear();
        dLvector.clear();
        dCvector.clear();
        dHvector.clear();
		isGS = FALSE;
		int nCol = last_minCol - 1, nCnt = 11;
		switch (m_displayMode)
		{
		case (0):
			nCnt = GetDocument()->GetMeasure()->GetGrayScaleSize();			
			last_Col = nCol;
			last_Size = nCnt;
			last_Display = 0;
			break;
		case (2):
			nCol = last_Col;
			nCnt = last_Size;
			break;
		case(3):
			nCnt = 101;
			if (isHDR)
				nCol = (last_minCol - 1) * 2;
			else
				nCol = (last_minCol - 1);
			last_Col = nCol;
			last_Size = nCnt;
			last_Display = 3;
			break;
		case(4):
			nCnt = 101;
			nCol = GetDocument()->GetMeasure()->m_NearWhiteClipCol - GetDocument()->GetMeasure()->GetNearWhiteScaleSize() + nCol;
			last_Col = nCol;
			last_Size = nCnt;
			last_Display = 4;
			break;
		}
        
		CColor White = GetDocument() -> GetMeasure () -> GetOnOffWhite();
	    CColor Black = GetDocument() -> GetMeasure () -> GetOnOffBlack();
		if (!GetConfig()->m_bOverRideTargs && Black.isValid() && White.isValid() && (GetConfig()->m_GammaOffsetType == 5 || GetConfig()->m_GammaOffsetType == 7))
		{
			if (Black.GetY() < White.GetY())
				GetConfig()->m_TargetMinL = Black.GetY()>1e-5?Black.GetY():0.0;
			else
				GetConfig()->m_TargetMinL = 0.0;

			if (White.GetY() > 0)
				GetConfig()->m_TargetMaxL = White.GetY();

			GetConfig()->m_TargetSysGamma = floor( (1.2 + 0.42 * log10(GetConfig()->m_TargetMaxL / 1000.))*100. + 0.5) / 100.0;
		}
					
		YWhite_for_color_comp = YWhite;

		// Incremental pass eligibility: mode 11 with a column cache built by a
		// prior full pass over the SAME patch count. Anything else (first pass,
		// CC set switched, manual edits) takes the full rebuild, which (re)sizes
		// the cache -- the grid analog of the 3D viewer's stale-scene guard.
		int jFirst = 0, jLast = nCount - 1;
		bool bIncrPass = ( nIncrCol >= 1 && nIncrCol <= nCount &&
						   m_displayMode == 11 && (int)m_ccDECache.size() == nCount );
		if ( bIncrPass )
		{
			// Catch-up window: refresh every column measured since the last
			// incremental pass. A sensor/pattern retry steps the sweep index
			// back, so span from the older of (last refreshed, requested).
			jFirst = nIncrCol - 1;
			if ( m_nGridLastRTCol >= 1 && m_nGridLastRTCol <= nCount && m_nGridLastRTCol < nIncrCol )
				jFirst = m_nGridLastRTCol - 1;
			jLast = nIncrCol - 1;
			m_nGridLastRTCol = nIncrCol;
			for ( int j = jFirst ; j <= jLast ; j ++ )
			{
				m_ccDECache[j] = -1.0;
				m_ccDLCache[j] = -1.0;
				m_ccDCCache[j] = -1.0;
				m_ccDHCache[j] = -1.0;
			}
		}
		else if ( m_displayMode == 11 )
		{
			m_ccDECache.assign ( nCount, -1.0 );
			m_ccDLCache.assign ( nCount, -1.0 );
			m_ccDCCache.assign ( nCount, -1.0 );
			m_ccDHCache.assign ( nCount, -1.0 );
			m_nGridLastRTCol = -1;
		}

		for( int j = jFirst ; j <= jLast ; j ++ )
		{
            int i = GetDocument() -> GetMeasure () -> GetGrayScaleSize ();
            ColorxyY tmpColor(GetColorReference().GetWhite());
			double x = ( m_displayMode == 0 )
					   ? GetDocument()->GetMeasure()->GetGrayPercent ( j, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235() )
					   : ArrayIndexToGrayLevel ( j, nCount, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235() );
			int mode = GetConfig()->m_GammaOffsetType;
			CColor White = GetDocument() -> GetMeasure () -> GetOnOffWhite();
			CColor Black = GetDocument() -> GetMeasure () -> GetOnOffBlack();

			switch ( m_displayMode )
			{
				case 0:
                     double valy;
					 aColor = GetDocument()->GetMeasure()->GetGray(j);
					 if ( pDataRef )
					 {
						refDocColor = pDataRef->GetMeasure()->GetGray(j);
					 }
					 // Determine Reference Y luminance for Delta E calculus
					if (GetConfig()->m_colorStandard == sRGB) mode = 99;
					if (  (mode >= 4) )
		            {
				        double valx = GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());
			            valy = getL_EOTF(valx, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
						valy = min(valy, GetConfig()->m_TargetMaxL);
		            }
			        else
			        {
				        double valx=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235())+Offset)/(1.0+Offset);
				        valy=pow(valx, GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef));
						if (mode == 1) //black compensation target
							valy = (Black.GetY() + ( valy * ( YWhite - Black.GetY() ) )) / YWhite;
			        }

					if (mode  == 5)
						tmpColor[2] = valy * 100. / YWhite;
					else
						tmpColor[2] = valy;

					refColor_for_color_comp.SetxyYValue(tmpColor);

					if ( GetConfig ()->m_dE_gray > 0 || GetConfig ()->m_dE_form == 5 )
					{
						// Compute reference Luminance regarding actual offset and reference gamma 
                        // fixed to use correct gamma predicts
                        // and added option to assume perfect gamma

                        if (GetConfig ()->m_dE_gray == 2 || GetConfig ()->m_dE_form == 5 )
								tmpColor[2] = aColor [ 1 ] / YWhite; //perfect gamma

						refColor.SetxyYValue(tmpColor);
					 }
					 else
					 {
	                    YWhite = aColor [ 1 ];
						if ( pDataRef )
							YWhiteRefDoc = refDocColor [ 1 ];
					 }
					 break;

				case 1:
					 if ( j < 3 )
					 {
						aColor = GetDocument()->GetMeasure()->GetPrimary(j);
						refColor = GetDocument()->GetMeasure()->GetRefPrimary(j);
						if ( pDataRef )
							refDocColor = pDataRef->GetMeasure()->GetPrimary(j);
					 }
					 else if ( j < 6 )
					 {
						aColor = GetDocument()->GetMeasure()->GetSecondary(j-3);
						refColor = GetDocument()->GetMeasure()->GetRefSecondary(j-3);
						if ( pDataRef )
							refDocColor = pDataRef->GetMeasure()->GetSecondary(j-3);
					 }
					 else if ( j == 6 )
					 {
						refColor = GetColorReference().GetWhite();
						if (GetDocument()->GetMeasure()->GetPrimeWhite().isValid())
							aColor = GetDocument()->GetMeasure()->GetPrimeWhite();
						YWhite = aColor [ 1 ];

						if (pDataRef)
						{
							refDocColor = pDataRef->GetMeasure()->GetPrimeWhite();
							YWhiteRefDoc = refDocColor [ 1 ];
						}
					 }
					 else if ( j == 7 )
					 {
						aColor = GetDocument()->GetMeasure()->GetOnOffBlack();
						refColor = noDataColor;
						refDocColor = noDataColor;
					 }
					 else
					 {
						ASSERT(0);
						aColor = noDataColor;
						refColor = noDataColor;
						refDocColor = noDataColor;
					 }
					 break;

				case 2:
					 aColor = GetDocument()->GetMeasure()->GetMeasurement(j);					 
					 bSpecialRef = TRUE;
					 //assume white 1st
					 if ( aColor.GetDeltaxy ( GetColorReference().GetWhite(), bRef ) < 0.05 )
 					 {
						bSpecialRef = FALSE;
						refColor = GetColorReference().GetWhite();
						double valy;
						ColorxyY tmpColor(GetColorReference().GetWhite());
						isGS = TRUE;

						if (GetConfig()->m_colorStandard == sRGB) mode = 99;
						if (  (mode >= 4) )
			            {
                            double valx = GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());
                            valy = getL_EOTF(valx, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
			            }
			            else
			            {
				            double valx=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235())+Offset)/(1.0+Offset);
				            valy=pow(valx, GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef));
							if (mode == 1) //black compensation target
								valy = (Black.GetY() + ( valy * ( YWhite - Black.GetY() ) )) / YWhite;
			            }

						if (mode  == 5)
							tmpColor[2] = valy * 100. / YWhite;
						else
							tmpColor[2] = valy;

						refColor_for_color_comp.SetxyYValue(tmpColor);

						if ( GetConfig ()->m_dE_gray > 0 || GetConfig ()->m_dE_form == 5 )
						{
						// Compute reference Luminance regarding actual offset and reference gamma 
                        // fixed to use correct gamma predicts
                        // and added option to assume perfect gamma
                        if (GetConfig ()->m_dE_gray == 2 || GetConfig ()->m_dE_form == 5 )
								tmpColor[2] = aColor [ 1 ] / YWhite; //perfect gamma

						refColor.SetxyYValue(tmpColor);
					 }
					 else
					 {
	                    YWhite = aColor [ 1 ];
					 }
					 }
					 else if ( aColor.GetDeltaxy ( GetDocument()->GetMeasure()->GetRefPrimary(0), bRef ) < 0.05 )
					 {
						refColor = GetDocument()->GetMeasure()->GetRefPrimary(0);
						clrSpecial1 = RGB(255,192,192);
						clrSpecial2 = RGB(255,224,224);
					 }
					 else if ( aColor.GetDeltaxy ( GetDocument()->GetMeasure()->GetRefPrimary(1), bRef ) < 0.05 )
					 {
						refColor = GetDocument()->GetMeasure()->GetRefPrimary(1);
						clrSpecial1 = RGB(192,255,192);
						clrSpecial2 = RGB(224,255,224);
					 }
					 else if ( aColor.GetDeltaxy ( GetDocument()->GetMeasure()->GetRefPrimary(2), bRef ) < 0.05 )
					 {
						refColor = GetDocument()->GetMeasure()->GetRefPrimary(2);
						clrSpecial1 = RGB(192,192,255);
						clrSpecial2 = RGB(224,224,255);
					 }
					 else if ( aColor.GetDeltaxy ( GetDocument()->GetMeasure()->GetRefSecondary(0), bRef ) < 0.05 )
					 {
						refColor = GetDocument()->GetMeasure()->GetRefSecondary(0);
						clrSpecial1 = RGB(255,255,192);
						clrSpecial2 = RGB(255,255,224);
					 }
					 else if ( aColor.GetDeltaxy ( GetDocument()->GetMeasure()->GetRefSecondary(1), bRef ) < 0.05 )
					 {
						refColor = GetDocument()->GetMeasure()->GetRefSecondary(1);
						clrSpecial1 = RGB(192,255,255);
						clrSpecial2 = RGB(224,255,255);
					 }
					 else if ( aColor.GetDeltaxy ( GetDocument()->GetMeasure()->GetRefSecondary(2), bRef ) < 0.05 )
					 {
						refColor = GetDocument()->GetMeasure()->GetRefSecondary(2);
						clrSpecial1 = RGB(255,192,255);
						clrSpecial2 = RGB(255,224,255);
					 }
					 else
						refColor = noDataColor;	// no recognisable target: no dE. Without this
												// the measure falls through to whatever refColor
												// held -- reference white at Y=1.0 on the first
												// column (a meaningless dE ~= 100 - L*), or the
												// PREVIOUS column's primary after that, since
												// refColor is declared outside the column loop.

					 if ( pDataRef )
						refDocColor = pDataRef->GetMeasure()->GetMeasurement(j);
					 else
						refDocColor = noDataColor;
					 break;

				case 3:
					 aColor = GetDocument()->GetMeasure()->GetNearBlack(j);
					 if ( pDataRef )
						refDocColor = pDataRef->GetMeasure()->GetNearBlack(j);
						
					 x = ArrayIndexToGrayLevel ( j * (mode == 5?2:1), 101, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235() );
					 Black = GetDocument() -> GetMeasure () -> GetNearBlack(0);
						
					 if (GetConfig()->m_colorStandard == sRGB) mode = 99;

   					 if (  (mode >= 4) )
			         {
                         double valx = GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());
                         valy = getL_EOTF(valx, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
			         }
			         else
			         {
				         double valx=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235())+Offset)/(1.0+Offset);
				         valy=pow(valx, GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef));
							
						 if (mode == 1) //black compensation target
							valy = (Black.GetY() + ( valy * ( YWhite - Black.GetY() ) )) / YWhite;
			         }

					if (mode  == 5)
						tmpColor[2] = valy * 100. / YWhite;
					else
						tmpColor[2] = valy;

					refColor_for_color_comp.SetxyYValue(tmpColor);

					if ( GetConfig ()->m_dE_gray > 0 || GetConfig ()->m_dE_form == 5 )
					{
						// Compute reference Luminance regarding actual offset and reference gamma 
                        // fixed to use correct gamma predicts
                        // and added option to assume perfect gamma
                        if (GetConfig ()->m_dE_gray == 2 || GetConfig ()->m_dE_form == 5 )
								tmpColor[2] = aColor [ 1 ] / YWhite; //perfect gamma

						refColor.SetxyYValue(tmpColor);
					}
					else
					{
	                    YWhite = aColor [ 1 ];
						if ( pDataRef )
							YWhiteRefDoc = refDocColor [ 1 ];
					}
					break;

				case 4:
					 aColor = GetDocument()->GetMeasure()->GetNearWhite(j);
					 if ( pDataRef )
						refDocColor = pDataRef->GetMeasure()->GetNearWhite(j);

						x = ArrayIndexToGrayLevel ( GetDocument()->GetMeasure()->m_NearWhiteClipCol - nCount + j, 101, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235() );
						if (GetConfig()->m_colorStandard == sRGB) mode = 99;

						if (  (mode >= 4) )
			            {
                            double valx = GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());
                            valy = getL_EOTF(valx, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
			            }
			            else
			            {
				            double valx=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235())+Offset)/(1.0+Offset);
				            valy=pow(valx, GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef));
							if (mode == 1) //black compensation target
								valy = (Black.GetY() + ( valy * ( YWhite - Black.GetY() ) )) / YWhite;
			            }

						if (mode  == 5)
							tmpColor[2] = valy * 100. / YWhite;
						else
							tmpColor[2] = valy;

						refColor_for_color_comp.SetxyYValue(tmpColor);

					 // Determine Reference Y luminance for Delta E calculus
					 if ( GetConfig ()->m_dE_gray > 0 || GetConfig ()->m_dE_form == 5 )
					 {
						// Compute reference Luminance regarding actual offset and reference gamma 
                        // fixed to use correct gamma predicts
                        // and added option to assume perfect gamma
                        if (GetConfig ()->m_dE_gray == 2 || GetConfig ()->m_dE_form == 5 )
								tmpColor[2] = aColor [ 1 ] / YWhite; //perfect gamma

						refColor.SetxyYValue(tmpColor);
					 }
					 else
					 {
	                    YWhite = aColor [ 1 ];
						if ( pDataRef )
							YWhiteRefDoc = refDocColor [ 1 ];
					 }
					 break;

				case 5:
					 aColor = GetDocument()->GetMeasure()->GetRedSat(j);
					 refColor = GetDocument()->GetMeasure()->GetRefSat(0,(double)j/(double)(nCount-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
					 if ( pDataRef )
						refDocColor = pDataRef->GetMeasure()->GetRedSat(j);
					 break;

				case 6:
					 aColor = GetDocument()->GetMeasure()->GetGreenSat(j);
					 refColor = GetDocument()->GetMeasure()->GetRefSat(1,(double)j/(double)(nCount-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
					 if ( pDataRef )
						refDocColor = pDataRef->GetMeasure()->GetGreenSat(j);
					 break;

				case 7:
					 aColor = GetDocument()->GetMeasure()->GetBlueSat(j);
					 refColor = GetDocument()->GetMeasure()->GetRefSat(2,(double)j/(double)(nCount-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
					 if ( pDataRef )
						refDocColor = pDataRef->GetMeasure()->GetBlueSat(j);
					 break;

				case 8:
					 aColor = GetDocument()->GetMeasure()->GetYellowSat(j);
					 refColor = GetDocument()->GetMeasure()->GetRefSat(3,(double)j/(double)(nCount-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
					 if ( pDataRef )
						refDocColor = pDataRef->GetMeasure()->GetYellowSat(j);
					 break;

				case 9:
					 aColor = GetDocument()->GetMeasure()->GetCyanSat(j);
					 refColor = GetDocument()->GetMeasure()->GetRefSat(4,(double)j/(double)(nCount-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
					 if ( pDataRef )
						refDocColor = pDataRef->GetMeasure()->GetCyanSat(j);
					 break;

				case 10:
					 aColor = GetDocument()->GetMeasure()->GetMagentaSat(j);
					 refColor = GetDocument()->GetMeasure()->GetRefSat(5,(double)j/(double)(nCount-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
					 if ( pDataRef )
						refDocColor = pDataRef->GetMeasure()->GetMagentaSat(j);
					 break;

				case 11:
					 aColor = GetDocument()->GetMeasure()->GetCC24Sat(j);
					 GetDocument()->GetMeasure()->GetRefCC24Sat(j, refColor);
					 if ( pDataRef )
						refDocColor = pDataRef->GetMeasure()->GetCC24Sat(j);
					 break;

				case 12:
					 refColor = noDataColor;
					 refDocColor = noDataColor;
					 switch ( j )
					 {
						case 0:
							 aColor = GetDocument()->GetMeasure()->GetOnOffBlack();
							 break;

						case 1:
							 aColor = GetDocument()->GetMeasure()->GetOnOffWhite();
							 break;

						case 2:
							 aColor = GetDocument()->GetMeasure()->GetAnsiBlack();
							 break;

						case 3:
							 aColor = GetDocument()->GetMeasure()->GetAnsiWhite();
							 break;
					 }
			}
			
			if ( (isHDR && m_displayMode <=11 && m_displayMode >= 5) )
			{
				if (GetConfig()->m_CCMode >= MASCIOR50 && GetConfig()->m_CCMode <= CCMAXHDR && m_displayMode == 11)
				{
					refColor.SetX((refColor.GetX() * 100.));
					refColor.SetY((refColor.GetY() * 100.));
					refColor.SetZ((refColor.GetZ() * 100.));
				}
				else
				{
					// Unified HDR rescale: GetHDRRefScale (tone-map aware, matches
					// the 3D viewer; = 105.95640 with tone mapping off). The manual
					// generator (DVD) keeps the legacy fixed scale - its GetItemText
					// white terms still use the 94.37844/tmWhite conventions.
					double s = ( GetConfig()->GetGeneratorType() == CColorHCFRConfig::enumManual )
							 ? 105.95640 : GetDocument()->GetMeasure()->GetHDRRefScale();
					refColor.SetX((refColor.GetX() * s));
					refColor.SetY((refColor.GetY() * s));
					refColor.SetZ((refColor.GetZ() * s));
				}
			}

			for( int i = 0 ; i < nRows ; i ++ )
			{
				Item.row = i+1;
				Item.col = j+1;
				
				if ( bHasLuxValues && i == nRows - ( 1 + bHasLuxDelta ) )
				{
					if ( aColor.isValid() && aColor.HasLuxValue () )
					{
						if ( GetConfig () -> m_bUseImperialUnits )
							Item.strText.Format ( "%.5g", aColor.GetLuxValue () * 0.0929 );
						else
							Item.strText.Format ( "%.5g", aColor.GetLuxValue () );
					}
					else
						Item.strText.Empty ();
				}
				else if ( bHasLuxValues && bHasLuxDelta && i == nRows - 1 )
				{
					if ( aColor.isValid() )
					{
						double dRef = refLuxColor.GetY() / refLuxColor.GetLuxValue ();
						double dColor = aColor.GetY() / aColor.GetLuxValue ();

						if ( fabs ( dRef ) < 0.000000001 )
							Item.strText.Empty ();
						else if ( fabs ( ( dRef - dColor ) / dRef ) < 0.001 )
						{
							if ( j == nCount - ( 1 + m_displayMode ) )
								Item.strText = "Ref";
							else
								Item.strText = "=";
						}
						else if ( dColor < dRef )
							Item.strText.Format("-%.1f %%", 100.0 * ( dRef - dColor ) / dRef );
						else
							Item.strText.Format("+%.1f %%", 100.0 * ( dColor - dRef ) / dRef );
					}
					else
						Item.strText.Empty ();
				}
				else
				{
					Item.strText = GetItemText ( aColor, YWhite, refColor, refDocColor, YWhiteRefDoc, i, j+1, Offset, isGS );
				}
				
				m_pGrayScaleGrid->SetItem(&Item);

				if ( bSpecialRef && i >= 3 )
				{
					m_pGrayScaleGrid->SetItemBkColour ( i+1, j+1, GridBk( i&1 ? clrSpecial1 : clrSpecial2 ) );
				}
			}
		}
		
		// Mode 11 summary stats are re-aggregated from the per-column cache so a
		// narrowed (incremental) pass yields the identical summary a full rebuild
		// would: reset the classic accumulators GetItemText just fed and refill
		// in column order, reproducing the full-loop fill exactly. Plain float
		// adds over N entries -- no colorimetric math, no grid access.
		if ( m_displayMode == 11 && (int)m_ccDECache.size() == nCount )
		{
			dEavg = 0.0; dLavg = 0.0; dCavg = 0.0; dHavg = 0.0;
			dEmax = 0.0; dEcnt = 0; dE10 = 0.0;
			dEvector.clear();
			dLvector.clear();
			dCvector.clear();
			dHvector.clear();
			for ( int j = 0 ; j < nCount ; j ++ )
			{
				if ( m_ccDECache[j] < 0.0 )
					continue;
				dEvector.push_back ( m_ccDECache[j] );
				dLvector.push_back ( m_ccDLCache[j] );
				dCvector.push_back ( m_ccDCCache[j] );
				dHvector.push_back ( m_ccDHCache[j] );
				dEavg += m_ccDECache[j];
				dLavg += m_ccDLCache[j];
				dCavg += m_ccDCCache[j];
				dHavg += m_ccDHCache[j];
				if ( m_ccDECache[j] > dEmax )
					dEmax = m_ccDECache[j];
				dEcnt ++;
			}
		}

		if ( bAddedCol )
		{
			int width = m_pGrayScaleGrid -> GetColumnWidth ( 0 );
			m_pGrayScaleGrid -> SetColumnWidth ( m_pGrayScaleGrid->GetColumnCount() - 1, width * 11 / 10 );
		}

		if ( m_displayMode == 12 )
			UpdateContrastValuesInGrid ();

		
		BOOL bHasMeas = FALSE;
		switch ( m_displayMode )
		{
		case 0: case 3: case 4: bHasMeas = GetDocument()->GetMeasure()->GetGray(0).isValid(); break;
		case 1: bHasMeas = GetDocument()->GetMeasure()->GetRedPrimary().isValid(); break;
		case 5: bHasMeas = GetDocument()->GetMeasure()->GetRedSat(0).isValid(); break;
		case 6: bHasMeas = GetDocument()->GetMeasure()->GetGreenSat(0).isValid(); break;
		case 7: bHasMeas = GetDocument()->GetMeasure()->GetBlueSat(0).isValid(); break;
		case 8: bHasMeas = GetDocument()->GetMeasure()->GetYellowSat(0).isValid(); break;
		case 9: bHasMeas = GetDocument()->GetMeasure()->GetCyanSat(0).isValid(); break;
		case 10: bHasMeas = GetDocument()->GetMeasure()->GetMagentaSat(0).isValid(); break;
		case 11: bHasMeas = GetDocument()->GetMeasure()->GetCC24Sat(0).isValid(); break;
		}
		if ( ! bHasMeas )
		{
			dEavg = 0.0; dLavg = 0.0; dCavg = 0.0; dHavg = 0.0; dEmax = 0.0; dEcnt = 0;
		}
		int dEcntSafe = ( dEcnt > 0 ) ? dEcnt : 1;
		if ( m_displayMode == 0 || m_displayMode == 3 || m_displayMode == 4)
		{
			// Gray scale mode: update group box title
			CString	Msg="", Tmp;

			{
				char	szBuf [ 256 ];

                if (m_displayMode == 0)
                {
    				Tmp.LoadString ( IDS_GAMMAAVERAGE );
	    			Msg += " ( ";
		    		Msg += Tmp;
			    	sprintf ( szBuf, ": %.2f, ", bHasMeas ? Gamma : 0.0 );
				    Msg += szBuf;					
				    Tmp.LoadString ( IDS_CONTRAST );
				    Msg += Tmp;
				    if ( GetDocument()->GetMeasure()->GetGray(0).isValid() && GetDocument()->GetMeasure()->GetGray(0).GetXYZValue()[1] > 0.0001 )
				    {
					    sprintf ( szBuf, ": %.0f:1 )", GetDocument()->GetMeasure()->GetOnOffWhite()[1] / GetDocument()->GetMeasure()->GetGray(0).GetXYZValue()[1] );
					    Msg += szBuf;
				    }
					else if ( GetDocument()->GetMeasure()->GetOnOffWhite().isValid() && GetDocument()->GetMeasure()->GetOnOffWhite()[1] > 0.0 )
					{
					    sprintf ( szBuf, ": %s:1 )", "Infinity" );
					    Msg += szBuf;
					}
				    else
				    {
					    Msg += ": N/A )";
				    }
                }

   			    if ( dEcnt > 0 )
				{
					dEavg_gs = dEavg / dEcntSafe;
					dEmax_gs = dEmax;
				}
				{
					CString dEform;
                    float a=2.0,b=3.0;
					Tmp.LoadString ( IDS_DELTAEAVERAGE );
					Msg += " ( ";
					Msg += Tmp;
					sprintf ( szBuf, ": %.2f [%.2f,%.2f,%.2f] max: %.2f", dEavg / dEcntSafe, dLavg / dEcntSafe, dCavg / dEcntSafe, dHavg / dEcntSafe, dEmax  );
					Msg += szBuf;
					switch (GetConfig()->m_dE_form)
					{
					case 0:
						{
						dEform = " [CIE76(uv)] )";
						a=3.0;
						b=5;
						break;
						}
					case 1:
						{
						dEform = " [CIE76(ab)] )";
						break;
						}
					case 2:
						{
						dEform = " [CIE94] )";
						break;
						}
					case 3:
						{
						dEform = " [CIE2000] )";
						break;
						}
					case 4:
						{
						dEform = " [CMC(1:1)] )";
						break;
						}
					case 5:
						{
						dEform = " [dCIE76(uv)] )";
						a=3.0;
						b=5;
						break;
						}
					case 6:
						{
						dEform = " [dICtCp] )";
						break;
						}
					}
					Msg += dEform;
					dEform = GetConfig()->m_dE_gray==0?" [Relative Y]":(GetConfig ()->m_dE_gray == 1?" [Absolute Y w/gamma]":" [Absolute Y w/o gamma]");
					Msg += dEform;
                    if (GetConfig()->doHighlight)
					    m_grayScaleGroup.SetBorderColor (fxUseCustomColor ? FxGetSysColor(COLOR_BTNFACE) : FxGetSysColor(COLOR_3DSHADOW));
				}
			}

			m_grayScaleGroup.SetText ( Msg );
		} else if ( m_displayMode == 1 )
		{
			CString	Msg="", Tmp;
//			Msg.LoadString ( IDS_SECONDARYCOLORS );
			m_grayScaleGroup.SetText ( Msg );
			{
				char	szBuf [ 256 ];
				CString dEform;
				float a=2.0, b=3;
				Tmp.LoadString ( IDS_DELTAEAVERAGE );
				Msg += " ( ";
				Msg += Tmp;
				sprintf ( szBuf, ": %.2f [%.2f,%.2f,%.2f] max: %.2f", dEavg / dEcntSafe, dLavg / dEcntSafe, dCavg / dEcntSafe, dHavg / dEcntSafe, dEmax  );
				Msg += szBuf;					
					switch (GetConfig()->m_dE_form)
					{
					case 0:
						{
						dEform = " [CIE76(uv)] )";
						a=3.0;
						b=5;
						break;
						}
					case 1:
						{
						dEform = " [CIE76(ab)] )";
						break;
						}
					case 2:
						{
						dEform = " [CIE94] )";
						break;
						}
					case 3:
						{
						dEform = " [CIE2000] )";
						break;
						}
					case 4:
						{
						dEform = " [CMC(1:1)] )";
						break;
						}
					case 5:
						{
						dEform = " [CIE2000] )";
						break;
						}
					case 6:
						{
						dEform = " [dICtCp] )";
						break;
						}
					}
					Msg += dEform;
                    if (GetConfig()->doHighlight)
                        m_grayScaleGroup.SetBorderColor (fxUseCustomColor ? FxGetSysColor(COLOR_BTNFACE) : FxGetSysColor(COLOR_3DSHADOW));
			}
			m_grayScaleGroup.SetText ( Msg );
		} else if ( m_displayMode > 4 && m_displayMode < 11 )
		{
			CString	Msg="", Tmp;;
//			Msg.LoadString ( IDS_SATURATIONCOLORS );
			m_grayScaleGroup.SetText ( Msg );
			{
				if ( dEcnt > 0 )
				switch(m_displayMode)
				{
				case 5:
					dEavg_sr = dEavg / dEcntSafe;
					dEmax_sr = dEmax;
					break;
				case 6:
					dEavg_sg = dEavg / dEcntSafe;
					dEmax_sg = dEmax;
					break;
				case 7:
					dEavg_sb = dEavg / dEcntSafe;
					dEmax_sb = dEmax;
					break;
				case 8:
					dEavg_sy = dEavg / dEcntSafe;
					dEmax_sy = dEmax;
					break;
				case 9:
					dEavg_sc = dEavg / dEcntSafe;
					dEmax_sc = dEmax;
					break;
				case 10:
					dEavg_sm = dEavg / dEcntSafe;
					dEmax_sm = dEmax;
					break;
				}
				char	szBuf [ 256 ];
				CString dEform;
				float a=2.0, b=3;
				Tmp.LoadString ( IDS_DELTAEAVERAGE );
				Msg += " ( ";
				Msg += Tmp;
				sprintf ( szBuf, ": %.2f [%.2f,%.2f,%.2f] max: %.2f", dEavg / dEcntSafe, dLavg / dEcntSafe, dCavg / dEcntSafe, dHavg / dEcntSafe, dEmax  );
				Msg += szBuf;					
					switch (GetConfig()->m_dE_form)
					{
					case 0:
						{
						dEform = " [CIE76(uv)] )";
						a=3.0;
						b=5;
						break;
						}
					case 1:
						{
						dEform = " [CIE76(ab)] )";
						break;
						}
					case 2:
						{
						dEform = " [CIE94] )";
						break;
						}
					case 3:
						{
						dEform = " [CIE2000] )";
						break;
						}
					case 4:
						{
						dEform = " [CMC(1:1)] )";
						break;
						}
					case 5:
						{
						dEform = " [CIE2000] )";
						break;
						}
					case 6:
						{
						dEform = " [dICtCp] )";
						break;
						}
					}
					Msg += dEform;
                    if (GetConfig()->doHighlight) 
					    m_grayScaleGroup.SetBorderColor (fxUseCustomColor ? FxGetSysColor(COLOR_BTNFACE) : FxGetSysColor(COLOR_3DSHADOW));
			}
			m_grayScaleGroup.SetText ( Msg );
		} else if (m_displayMode == 11)
		{
			CString	Msg="", Tmp;
			BOOL isExtPat =( GetConfig()->m_CCMode == USER || GetConfig()->m_CCMode == CM10SAT || GetConfig()->m_CCMode == CM10SAT75 || GetConfig()->m_CCMode == CM5SAT || GetConfig()->m_CCMode == CM5SAT75 || GetConfig()->m_CCMode == CM4SAT || GetConfig()->m_CCMode == CM4SAT75 || GetConfig()->m_CCMode == CM4LUM || GetConfig()->m_CCMode == CM5LUM || GetConfig()->m_CCMode == CM10LUM || GetConfig()->m_CCMode == RANDOM250 || GetConfig()->m_CCMode == RANDOM500 || GetConfig()->m_CCMode == CM6NB || GetConfig()->m_CCMode == CMDNR || GetConfig()->m_CCMode == MASCIOR50);
			isExtPat = (isExtPat || GetConfig()->m_CCMode > 19);
			Msg += (GetConfig()->m_CCMode == GCD?"Classic GCD":(GetConfig()->m_CCMode==MCD?"Classic MCD":(GetConfig()->m_CCMode==SKIN?"Pantone skin tones":(GetConfig()->m_CCMode==CCSG?"CalMan SG":isExtPat?GetConfig()->GetCColorsN(-1).c_str():(GetConfig()->m_CCMode==CMS?"CalMAN SG skin tones":(GetConfig()->m_CCMode==CPS?"ChromaPure skin tones":(GetConfig()->m_CCMode==CMC?"Classic CalMAN":"RGB Luminance Ramps")))))));
			m_grayScaleGroup.SetText ( Msg );
			{
				char	szBuf [ 256 ];
				CString dEform;
				if ( dEcnt > 0 )
				{
					dEavg_cc = dEavg / dEcntSafe;
					dEmax_cc = dEmax;
				}
				float a = 2.0, b = 3;
				Tmp.LoadString ( IDS_DELTAEAVERAGE );
				Msg += " ( ";
				Msg += Tmp;
				if ( dEcnt > 0 && GetConfig()->GetCColorsSize() >= 96 )
                {
                    vector<double>::iterator max;
                    max = max_element( dEvector.begin(), dEvector.end() );
                    double maxv = *max ;
                    int pos = distance(dEvector.begin(), max);
                    std::sort ( dEvector.begin(), dEvector.end() );
                    for ( unsigned i = (int) (dEvector.size() - (dEvector.size() / 10 + 1)); i < dEvector.size(); i++ ) dE10+=(float)dEvector[i];
					if (m_displayMode == 11)
						dE10min = dEvector[(int) (dEvector.size() - (dEvector.size() / 10 + 1))];
 
					dE10 = dE10 / (int) ( (dEvector.size() / 10 + 1) );
                    char aBuf[10];
                    sprintf(aBuf,"Color %d",pos+1);
					dEmax_cc = maxv;

					if (GetConfig()->m_CCMode == CCSG )
        				sprintf ( szBuf, ": %.2f [%.2f,%.2f,%.2f] max: %.2f[%s], worst 10%%: %.2f", dEavg / dEcntSafe, dLavg / dEcntSafe, dCavg / dEcntSafe, dHavg / dEcntSafe, maxv, PatName[pos], dE10 );
                    else
        				sprintf ( szBuf, ": %.2f [%.2f,%.2f,%.2f] max: %.2f[%s], worst 10%%: %.2f", dEavg / dEcntSafe, dLavg / dEcntSafe, dCavg / dEcntSafe, dHavg / dEcntSafe, maxv, aBuf, dE10 );
                }
                else
				{
					dE10min=0.;
    				sprintf ( szBuf, ": %.2f [%.2f,%.2f,%.2f] max: %.2f", dEavg / dEcntSafe, dLavg / dEcntSafe, dCavg / dEcntSafe, dHavg / dEcntSafe, dEmax);
				}
                    dEvector.clear();
                    dLvector.clear();
                    dCvector.clear();
                    dHvector.clear();
				    Msg += szBuf;					
					switch (GetConfig()->m_dE_form)
					{
					case 0:
						{
						dEform = " [CIE76(uv)] )";
						a=3.0;
						b=5;
						break;
						}
					case 1:
						{
						dEform = " [CIE76(ab)] )";
						break;
						}
					case 2:
						{
						dEform = " [CIE94] )";
						break;
						}
					case 3:
						{
						dEform = " [CIE2000] )";
						break;
						}
					case 4:
						{
						dEform = " [CMC(1:1)] )";
						break;
						}
					case 5:
						{
						dEform = " [CIE2000] )";
						break;
						}
					case 6:
						{
						dEform = " [dICtCp] )";
						break;
						}
					}
					Msg += dEform;
                    if (GetConfig()->doHighlight)
					    m_grayScaleGroup.SetBorderColor (fxUseCustomColor ? FxGetSysColor(COLOR_BTNFACE) : FxGetSysColor(COLOR_3DSHADOW));
			}
			m_grayScaleGroup.SetText ( Msg );
		}
	}
		// sRGB overrides m_GammaOffsetType outright (getL_EOTF mode 99), so the
		// stored transfer function must not be reported as the active one -
		// neither the HDR branches nor the SDR switch below describe what the
		// app is actually computing with. Without this the info line names PQ's
		// "HDR10" whenever sRGB is selected while a PQ choice is still stored,
		// and names a power law in the ordinary case, for every sRGB user.
		bool isSRGBtf = ( GetConfig()->m_colorStandard == sRGB );
		bool isHDR = !isSRGBtf && GetConfig()->m_GammaOffsetType == 5;
		double 	BBC_gamma = GetConfig()->m_TargetSysGamma;
		CString dWhitestr;
		double tmWhite = TmDiffuseWhiteNits(noDataColor, noDataColor);
		dWhitestr.Format("%4.1f nits diffuse white", tmWhite);
		
		if (GetConfig()->m_useToneMap)
			dWhitestr += " w/BT.2390 Tonemap";
		CString bbcstr,sdrstr;
		bbcstr.Format("system gamma: %3.2f",BBC_gamma);

		CString CS = GetColorStandardName(GetColorReference().m_standard);
		CString WP = GetColorReference().whiteName;

		switch(GetConfig()->m_GammaOffsetType)
		{
			case 0:
				sdrstr.Format(" SDR, Power law w/gamma = %3.2f", GetConfig()->m_GammaAvg);
			break;
			case 1:
				sdrstr.Format(" SDR, Power law (black compensation) w/gamma = %3.2f", GetConfig()->m_GammaAvg);
			break;
			case 2:
				sdrstr.Format(" SDR, Power law w/Camera gamma = %3.2f", GetConfig()->m_GammaAvg);
			break;
			case 3:
				sdrstr.Format(" SDR, Power law w/Camera gamma = %3.2f", GetConfig()->m_GammaAvg);
			break;
			case 4:
				if (GetConfig()->m_GammaRel == 0)
					sdrstr.SetString(" SDR, BT.1886 w/default gamma");
				else
					sdrstr.Format(" SDR, BT.1886 w/relative gamma = %3.2f", GetConfig()->m_GammaRel);
			break;
			case 6:
				sdrstr.SetString(" SDR, L*");
			break;
		}
		// sRGB's curve is fixed and is none of the cases above, so the switch
		// left sdrstr holding whichever stored transfer function was inactive.
		if (isSRGBtf)
			sdrstr.SetString(" SDR, sRGB");
		m_infoLine = "Color Space: "+CS+", White Point: "+WP+", EOTF: "+(isHDR ? " HDR10, "+ dWhitestr:(!isSRGBtf && GetConfig()->m_GammaOffsetType == 7)?" HLG, "+bbcstr:sdrstr);
		CString t = CTime::GetCurrentTime().Format(" [%H:%M:%S]");
		CWnd * pWnd = GetDlgItem(IDC_INFOLINE);
		CString nMeasures;
		nMeasures.Format(", # of measures: %d",GetDocument()->GetMeasure()->m_NMeasurements);
		pWnd->SetWindowTextA(m_infoLine + nMeasures + t);
		m_tooltip.RemoveAllTools();
		m_tooltip.AddTool(pWnd, m_infoLine + nMeasures + t);
}

void CMainView::UpdateContrastValuesInGrid ()
{
	GV_ITEM Item;

	Item.mask = GVIF_TEXT|GVIF_FORMAT;
	Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;

	Item.col = 6;
	Item.row = 1;

	if ( GetDocument()->GetMeasure()->GetOnOffContrast () > 0.0 )
		Item.strText.Format ( "%.0f:1", GetDocument()->GetMeasure()->GetOnOffContrast () );
	else
		if ( GetDocument()->GetMeasure()->GetOnOffContrast () == -1 )
			Item.strText.Format ("%s","Infinite");
		else
			Item.strText.Format ("%s","undefined");

	m_pGrayScaleGrid->SetItem(&Item);


	Item.row = 2;

	if ( GetDocument()->GetMeasure()->GetAnsiContrast () > 0.0 )
		Item.strText.Format ( "%.0f:1", GetDocument()->GetMeasure()->GetAnsiContrast () );
	else
		if ( GetDocument()->GetMeasure()->GetOnOffContrast () == -1 )
			Item.strText.Format ("%s","Infinite");
		else
			Item.strText.Format ("%s","undefined");

	m_pGrayScaleGrid->SetItem(&Item);
}

void CMainView::OnGrayScaleGridBeginEdit(NMHDR *pNotifyStruct,LRESULT* pResult)
{

	NM_GRIDVIEW *	pItem = (NM_GRIDVIEW*) pNotifyStruct;

    // Check that cell is valid
	if ( pItem->iColumn < 1 || pItem->iRow > 3)
		return;

	// Get document XYZ value
	CColor aColorMeasure;
	CColorReference  bRef = ((GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4)?ContainerTransportReference(GetColorReference()):(GetColorReference().m_standard == HDTVa || GetColorReference().m_standard == HDTVb)?CColorReference(HDTV):GetColorReference());
	
	switch ( m_displayMode )
	{
		case 0:
			 aColorMeasure=GetDocument()->GetMeasure()->GetGray(pItem->iColumn-1);
			 if (pItem->iColumn == GetDocument()->GetMeasure()->GetGrayScaleSize())
				isSelectedWhiteY = TRUE;
			 break;

		case 1:
			 if ( pItem->iColumn < 4 )
				aColorMeasure=GetDocument()->GetMeasure()->GetPrimary(pItem->iColumn-1);
			 else if ( pItem->iColumn < 7 )
				aColorMeasure=GetDocument()->GetMeasure()->GetSecondary(pItem->iColumn-4);
			 else if ( pItem->iColumn == 7 )
			 {
				aColorMeasure=GetDocument()->GetMeasure()->GetPrimeWhite();
				isSelectedWhiteY = TRUE;
			 }
			 else if ( pItem->iColumn == 8 )
				aColorMeasure=GetDocument()->GetMeasure()->GetOnOffBlack();
			 else
			 {
				ASSERT(0);
				aColorMeasure=noDataColor;
			 }
			 break;

		case 2:
			 aColorMeasure=GetDocument()->GetMeasure()->GetMeasurement(pItem->iColumn-1);
			 break;

		case 3:
			 aColorMeasure=GetDocument()->GetMeasure()->GetNearBlack(pItem->iColumn-1);
			 break;

		case 4:
			 aColorMeasure=GetDocument()->GetMeasure()->GetNearWhite(pItem->iColumn-1);
			 break;

		case 5:
			 aColorMeasure=GetDocument()->GetMeasure()->GetRedSat(pItem->iColumn-1);
			 break;

		case 6:
			 aColorMeasure=GetDocument()->GetMeasure()->GetGreenSat(pItem->iColumn-1);
			 break;

		case 7:
			 aColorMeasure=GetDocument()->GetMeasure()->GetBlueSat(pItem->iColumn-1);
			 break;

		case 8:
			 aColorMeasure=GetDocument()->GetMeasure()->GetYellowSat(pItem->iColumn-1);
			 break;

		case 9:
			 aColorMeasure=GetDocument()->GetMeasure()->GetCyanSat(pItem->iColumn-1);
			 break;

		case 10:
			 aColorMeasure=GetDocument()->GetMeasure()->GetMagentaSat(pItem->iColumn-1);
			 break;

		case 11:
			 aColorMeasure=GetDocument()->GetMeasure()->GetCC24Sat(pItem->iColumn-1);
			 break;

		case 12:
			 switch ( pItem->iColumn )
			 {
				case 1:
					 aColorMeasure=GetDocument()->GetMeasure()->GetOnOffBlack();
					 break;

				case 2:
					 aColorMeasure=GetDocument()->GetMeasure()->GetOnOffWhite();
					 break;

				case 3:
					 aColorMeasure=GetDocument()->GetMeasure()->GetAnsiBlack();
					 break;

				case 4:
					 aColorMeasure=GetDocument()->GetMeasure()->GetAnsiWhite();
					 break;
			 }
			 break;
	}
	
	ColorTriplet aColor = ColorXYZ();

	if ( aColorMeasure.isValid() )
	{
		// Get color data from XYZ value 
		switch(m_displayType)
		{
			case HCFR_SENSORRGB_VIEW:
				isSelectedWhiteY = FALSE;
				break;
			case HCFR_XYZ_VIEW:
				aColor=aColorMeasure.GetXYZValue();
				if (isSelectedWhiteY)
				{
					if (pItem->iRow == 2)
						isSelectedWhiteY = TRUE;
					else
						isSelectedWhiteY = FALSE;
				}
				break;
			case HCFR_RGB_VIEW:
				aColor=aColorMeasure.GetRGBValue(bRef);
				isSelectedWhiteY = FALSE;
				break;
			case HCFR_xyY_VIEW:
				aColor=aColorMeasure.GetxyYValue();
				if (isSelectedWhiteY)
				{
					if (pItem->iRow == 3)
						isSelectedWhiteY = TRUE;
					else
						isSelectedWhiteY = FALSE;
				}
				break;
		}
	}

	// Retrieve correct row value
	CString aNewStr;

	if ( aColor.isValid() )
	{
		double aVal;
		aVal = aColor[pItem->iRow-1];

		if ( aVal != FX_NODATA )
			aNewStr.Format ( "%f", aVal );
		else
			isSelectedWhiteY = FALSE;
		m_pGrayScaleGrid->SetItemText(pItem->iRow,pItem->iColumn,aNewStr);
	}
	else
		isSelectedWhiteY = FALSE;
}

void CMainView::OnGrayScaleGridEndEdit(NMHDR *pNotifyStruct,LRESULT* pResult)
{
	LPARAM			lHint = UPD_EVERYTHING;
    NM_GRIDVIEW *	pItem = (NM_GRIDVIEW*) pNotifyStruct;

    // Check that cell is valid
	if ( pItem->iColumn < 1 || pItem->iRow > 3)
		return;

	CString aNewStr=m_pGrayScaleGrid->GetItemText(pItem->iRow,pItem->iColumn);
	aNewStr.Replace(",",".");	// replace decimal separator if necessary
 	double aVal;
	CColorReference  bRef = ((GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4)?ContainerTransportReference(GetColorReference()):(GetColorReference().m_standard == HDTVa || GetColorReference().m_standard == HDTVb)?CColorReference(HDTV):GetColorReference());
	BOOL bAcceptChange = !aNewStr.IsEmpty() && sscanf(aNewStr,"%lf",&aVal) && (m_displayType != HCFR_xyz2_VIEW);
	if(bAcceptChange)	// update value in document
	{
		// Get document XYZ value
		CColor aColorMeasure;
		
		switch ( m_displayMode )
		{
			case 0:
				 aColorMeasure=GetDocument()->GetMeasure()->GetGray(pItem->iColumn-1);
				 break;

			case 1:
				 if ( pItem->iColumn < 4 )
					aColorMeasure=GetDocument()->GetMeasure()->GetPrimary(pItem->iColumn-1);
				 else if ( pItem->iColumn < 7 )
					aColorMeasure=GetDocument()->GetMeasure()->GetSecondary(pItem->iColumn-4);
				 else if ( pItem->iColumn == 7 )
					aColorMeasure=GetDocument()->GetMeasure()->GetPrimeWhite();
				 else if ( pItem->iColumn == 8 )
					aColorMeasure=GetDocument()->GetMeasure()->GetOnOffBlack();
				 else
				 {
					ASSERT(0);
					aColorMeasure=noDataColor;
				 }
				 break;

			case 2:
				 aColorMeasure=GetDocument()->GetMeasure()->GetMeasurement(pItem->iColumn-1);
				 break;

			case 3:
				 aColorMeasure=GetDocument()->GetMeasure()->GetNearBlack(pItem->iColumn-1);
				 break;

			case 4:
				 aColorMeasure=GetDocument()->GetMeasure()->GetNearWhite(pItem->iColumn-1);
				 break;

			case 5:
				 aColorMeasure=GetDocument()->GetMeasure()->GetRedSat(pItem->iColumn-1);
				 break;

			case 6:
				 aColorMeasure=GetDocument()->GetMeasure()->GetGreenSat(pItem->iColumn-1);
				 break;

			case 7:
				 aColorMeasure=GetDocument()->GetMeasure()->GetBlueSat(pItem->iColumn-1);
				 break;

			case 8:
				 aColorMeasure=GetDocument()->GetMeasure()->GetYellowSat(pItem->iColumn-1);
				 break;

			case 9:
				 aColorMeasure=GetDocument()->GetMeasure()->GetCyanSat(pItem->iColumn-1);
				 break;

			case 10:
				 aColorMeasure=GetDocument()->GetMeasure()->GetMagentaSat(pItem->iColumn-1);
				 break;

			case 11:
				 aColorMeasure=GetDocument()->GetMeasure()->GetCC24Sat(pItem->iColumn-1);
				 break;

			case 12:
				 switch ( pItem->iColumn )
				 {
					case 1:
						 aColorMeasure=GetDocument()->GetMeasure()->GetOnOffBlack();
						 break;

					case 2:
						 aColorMeasure=GetDocument()->GetMeasure()->GetOnOffWhite();
						 break;

					case 3:
						 aColorMeasure=GetDocument()->GetMeasure()->GetAnsiBlack();
						 break;

					case 4:
						 aColorMeasure=GetDocument()->GetMeasure()->GetAnsiWhite();
						 break;
				 }
				 break;
		}
		
		if ( !aColorMeasure.isValid() )
		{
			// No color value: build a color from reference white
			aColorMeasure = GetColorReference().GetWhite ();
		}
		//allow user to scale all measurements to new white luminance
		if (isSelectedWhiteY && GetColorApp()->InMeasureMessageBox("Scale all measurements to new white Y?","Scale all",MB_ICONQUESTION | MB_YESNO) == IDYES) 
		{
			double fact = aVal / aColorMeasure.GetY();
			CColor aNewColor;
			int nMeasures = GetDocument()->GetMeasure()->GetGrayScaleSize();
			for (int i = 0; i < nMeasures; i++)
			{
				aNewColor = GetDocument()->GetMeasure()->GetGray(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetGray(i, aNewColor);
				}
			}
			for (int i = 0; i <= 2; i++)
			{
				aNewColor = GetDocument()->GetMeasure()->GetPrimary(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetPrimary(i, aNewColor);
				}

				aNewColor = GetDocument()->GetMeasure()->GetSecondary(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetSecondary(i, aNewColor);
				}
			}
			nMeasures = GetDocument()->GetMeasure()->GetMeasurementsSize();
			for (int i = 0; i < nMeasures; i++)
			{
				aNewColor = GetDocument()->GetMeasure()->GetMeasurement(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetMeasurements(i, aNewColor);
				}
			}
			nMeasures = GetDocument()->GetMeasure()->GetSaturationSize();
			for (int i = 0; i < nMeasures; i++)
			{
				aNewColor = GetDocument()->GetMeasure()->GetRedSat(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetRedSat(i, aNewColor);
				}
				aNewColor = GetDocument()->GetMeasure()->GetBlueSat(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetBlueSat(i, aNewColor);
				}
				aNewColor = GetDocument()->GetMeasure()->GetGreenSat(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetGreenSat(i, aNewColor);
				}
				aNewColor = GetDocument()->GetMeasure()->GetYellowSat(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetYellowSat(i, aNewColor);
				}
				aNewColor = GetDocument()->GetMeasure()->GetCyanSat(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetCyanSat(i, aNewColor);
				}
				aNewColor = GetDocument()->GetMeasure()->GetMagentaSat(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetMagentaSat(i, aNewColor);
				}
			}
			nMeasures = GetDocument()->GetMeasure()->GetCC24MasterSaturationSize();
			for (int i = 0; i < nMeasures; i++)
			{
				aNewColor = GetDocument()->GetMeasure()->GetCC24MasterSat(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetCC24MasterSat(i, aNewColor);
				}
			}
			nMeasures = GetDocument()->GetMeasure()->GetNearBlackScaleSize();
			for (int i = 0; i < nMeasures; i++)
			{
				aNewColor = GetDocument()->GetMeasure()->GetNearBlack(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetNearBlack(i, aNewColor);
				}
			}
			nMeasures = GetDocument()->GetMeasure()->GetNearWhiteScaleSize();
			for (int i = 0; i < nMeasures; i++)
			{
				aNewColor = GetDocument()->GetMeasure()->GetNearWhite(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetNearWhite(i, aNewColor);
				}
			}

			aNewColor = GetDocument()->GetMeasure()->GetPrimeWhite();
			aNewColor.SetX(fact * aNewColor.GetX());
			aNewColor.SetY(fact * aNewColor.GetY());
			aNewColor.SetZ(fact * aNewColor.GetZ());
			GetDocument()->GetMeasure()->SetPrimeWhite(aNewColor);

			aNewColor = GetDocument()->GetMeasure()->GetOnOffWhite();
			aNewColor.SetX(fact * aNewColor.GetX());
			aNewColor.SetY(fact * aNewColor.GetY());
			aNewColor.SetZ(fact * aNewColor.GetZ());
			GetDocument()->GetMeasure()->SetOnOffWhite(aNewColor);

			aNewColor = GetDocument()->GetMeasure()->GetAnsiWhite();
			aNewColor.SetX(fact * aNewColor.GetX());
			aNewColor.SetY(fact * aNewColor.GetY());
			aNewColor.SetZ(fact * aNewColor.GetZ());
			GetDocument()->GetMeasure()->SetAnsiWhite(aNewColor);

			aNewColor = GetDocument()->GetMeasure()->GetAnsiBlack();
			aNewColor.SetX(fact * aNewColor.GetX());
			aNewColor.SetY(fact * aNewColor.GetY());
			aNewColor.SetZ(fact * aNewColor.GetZ());
			GetDocument()->GetMeasure()->SetAnsiBlack(aNewColor);
			isSelectedWhiteY = FALSE;
			lHint = UPD_EVERYTHING;
		} else //normal edit
		{

		int nsize=GetDocument()->GetMeasure()->GetGrayScaleSize();
		ColorTriplet aColor = ColorXYZ();

		// Get color data from XYZ value 
		switch(m_displayType)
		{
			case HCFR_XYZ_VIEW:
				aColor=aColorMeasure.GetXYZValue();
				break;
			case HCFR_SENSORRGB_VIEW:
				aColor=aColorMeasure.GetXYZValue();
				break;
			case HCFR_RGB_VIEW:
				aColor=aColorMeasure.GetRGBValue(bRef);
				break;
			case HCFR_xyY_VIEW:
				aColor=aColorMeasure.GetxyYValue();
				break;
		}

		// change the correct row value
		aColor[pItem->iRow-1]=aVal;

		// Convert back color data to XYZ
		switch(m_displayType)
		{
			case HCFR_XYZ_VIEW:
				aColorMeasure.SetXYZValue(ColorXYZ(aColor));
				break;
			case HCFR_SENSORRGB_VIEW:
				aColorMeasure.SetXYZValue(ColorXYZ(aColor));
				break;
			case HCFR_RGB_VIEW:
				aColorMeasure.SetRGBValue(ColorRGB(aColor), bRef);
				break;
			case HCFR_xyY_VIEW:
				aColorMeasure.SetxyYValue(ColorxyY(aColor));
				break;
		}
		
		// Update document XYZ value
		switch ( m_displayMode )
		{
			case 0:
				 GetDocument()->GetMeasure()->SetGray(pItem->iColumn-1,aColorMeasure);
				 if (pItem->iColumn == 1)
					 GetDocument()->GetMeasure()->SetOnOffBlack(aColorMeasure);
				 if (pItem->iColumn == nsize)
					 GetDocument()->GetMeasure()->SetOnOffWhite(aColorMeasure);
				 lHint = UPD_GRAYSCALE;
				 break;

			case 1:
				 switch ( pItem->iColumn )
				 {
					case 1:
						 GetDocument()->GetMeasure()->SetRedPrimary(aColorMeasure);
						 lHint = UPD_PRIMARIES;
						 break;

					case 2:
						 GetDocument()->GetMeasure()->SetGreenPrimary(aColorMeasure);
						 lHint = UPD_PRIMARIES;
						 break;

					case 3:
						 GetDocument()->GetMeasure()->SetBluePrimary(aColorMeasure);
						 lHint = UPD_PRIMARIES;
						 break;

					case 4:
						 GetDocument()->GetMeasure()->SetYellowSecondary(aColorMeasure);
						 lHint = UPD_SECONDARIES;
						 break;

					case 5:
						 GetDocument()->GetMeasure()->SetCyanSecondary(aColorMeasure);
						 lHint = UPD_SECONDARIES;
						 break;

					case 6:
						 GetDocument()->GetMeasure()->SetMagentaSecondary(aColorMeasure);
						 lHint = UPD_SECONDARIES;
						 break;

					case 7:
						 GetDocument()->GetMeasure()->SetPrimeWhite(aColorMeasure);
						 lHint = UPD_SECONDARIES;
						 break;

					case 8:
						 GetDocument()->GetMeasure()->SetOnOffBlack(aColorMeasure);
						 lHint = UPD_SECONDARIES;
						 break;
				 }
				 break;

			case 2:
				 GetDocument()->GetMeasure()->SetMeasurements(pItem->iColumn-1,aColorMeasure);
				 lHint = UPD_FREEMEASURES;
				 break;

			case 3:
				 GetDocument()->GetMeasure()->SetNearBlack(pItem->iColumn-1,aColorMeasure);
				 lHint = UPD_NEARBLACK;
				 break;

			case 4:
				 GetDocument()->GetMeasure()->SetNearWhite(pItem->iColumn-1,aColorMeasure);
				 lHint = UPD_NEARWHITE;
				 break;

			case 5:
				 GetDocument()->GetMeasure()->SetRedSat(pItem->iColumn-1,aColorMeasure);
				 lHint = UPD_REDSAT;
				 break;

			case 6:
				 GetDocument()->GetMeasure()->SetGreenSat(pItem->iColumn-1,aColorMeasure);
				 lHint = UPD_GREENSAT;
				 break;

			case 7:
				 GetDocument()->GetMeasure()->SetBlueSat(pItem->iColumn-1,aColorMeasure);
				 lHint = UPD_BLUESAT;
				 break;

			case 8:
				 GetDocument()->GetMeasure()->SetYellowSat(pItem->iColumn-1,aColorMeasure);
				 lHint = UPD_YELLOWSAT;
				 break;

			case 9:
				 GetDocument()->GetMeasure()->SetCyanSat(pItem->iColumn-1,aColorMeasure);
				 lHint = UPD_CYANSAT;
				 break;

			case 10:
				 GetDocument()->GetMeasure()->SetMagentaSat(pItem->iColumn-1,aColorMeasure);
				 lHint = UPD_MAGENTASAT;
				 break;

			case 11:
				 GetDocument()->GetMeasure()->SetCC24Sat(pItem->iColumn-1,aColorMeasure);
				 lHint = UPD_CC24SAT;
				 break;

			case 12:
				 switch ( pItem->iColumn )
				 {
					case 1:
						 GetDocument()->GetMeasure()->SetOnOffBlack(aColorMeasure);
						 break;

					case 2:
						 GetDocument()->GetMeasure()->SetOnOffWhite(aColorMeasure);
						 break;

					case 3:
						 GetDocument()->GetMeasure()->SetAnsiBlack(aColorMeasure);
						 break;

					case 4:
						 GetDocument()->GetMeasure()->SetAnsiWhite(aColorMeasure);
						 break;
				 }
				 lHint = UPD_CONTRAST;
				 break;
		}
		}

		GetDocument()->SetModifiedFlag(TRUE);
		GetDocument()->UpdateAllViews(NULL, lHint);	// Called with NULL to update current view too (format correctly values)
		
		switch ( m_displayMode )
		{
			case 0:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetGray(pItem->iColumn-1) );
				 break;

			case 1:
				 if ( pItem->iColumn < 4 )
					SetSelectedColor ( GetDocument()->GetMeasure()->GetPrimary(pItem->iColumn-1) );
				 else if ( pItem->iColumn < 7 )
					SetSelectedColor ( GetDocument()->GetMeasure()->GetSecondary(pItem->iColumn-4) );
				 else if ( pItem->iColumn == 7 )
					SetSelectedColor ( GetDocument()->GetMeasure()->GetPrimeWhite() );
				 else if ( pItem->iColumn == 8 )
					SetSelectedColor ( GetDocument()->GetMeasure()->GetOnOffBlack() );
				 else
				 {
					ASSERT(0);
					SetSelectedColor ( noDataColor );
				 }
				 break;

			case 2:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetMeasurement(pItem->iColumn-1) );
				 break;

			case 3:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetNearBlack(pItem->iColumn-1) );
				 break;

			case 4:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetNearWhite(pItem->iColumn-1) );
				 break;

			case 5:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetRedSat(pItem->iColumn-1) );
				 break;

			case 6:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetGreenSat(pItem->iColumn-1) );
				 break;

			case 7:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetBlueSat(pItem->iColumn-1) );
				 break;

			case 8:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetYellowSat(pItem->iColumn-1) );
				 break;

			case 9:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetCyanSat(pItem->iColumn-1) );
				 break;

			case 10:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetMagentaSat(pItem->iColumn-1) );
				 break;

			case 11:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetCC24Sat(pItem->iColumn-1) );
				 break;

			case 12:
				 switch ( pItem->iColumn )
				 {
					case 1:
						 SetSelectedColor ( GetDocument()->GetMeasure()->GetOnOffBlack() );
						 break;

					case 2:
						 SetSelectedColor ( GetDocument()->GetMeasure()->GetOnOffWhite() );
						 break;

					case 3:
						 SetSelectedColor ( GetDocument()->GetMeasure()->GetAnsiBlack() );
						 break;

					case 4:
						 SetSelectedColor ( GetDocument()->GetMeasure()->GetAnsiWhite() );
						 break;
				 }
				 UpdateContrastValuesInGrid ();
				 break;
		}

//		(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	}

    *pResult = (bAcceptChange)? 0 : -1;
}

void CMainView::OnGrayScaleGridEndSelChange(NMHDR *pNotifyStruct,LRESULT* pResult)
{
	// Clear selection in other grids: only one grid with selection at time
	int maxCol=m_pGrayScaleGrid->GetSelectedCellRange().GetMaxCol();
	int minCol=m_pGrayScaleGrid->GetSelectedCellRange().GetMinCol();
	if( maxCol != minCol || minCol < 1 )
		SetSelectedColor ( noDataColor );	// if more than one column selected => no data selected t avoid confusion
	else
	{
		switch ( m_displayMode )
		{
			case 0:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetGray(minCol-1) );
				 break;

			case 1:
				 if ( minCol < 4 )
					SetSelectedColor ( GetDocument()->GetMeasure()->GetPrimary(minCol-1) );
				 else if ( minCol < 7 )
					SetSelectedColor ( GetDocument()->GetMeasure()->GetSecondary(minCol-4) );
				 else if ( minCol == 7 )
					SetSelectedColor ( GetDocument()->GetMeasure()->GetPrimeWhite() );
				 else if ( minCol == 8 )
					SetSelectedColor ( GetDocument()->GetMeasure()->GetOnOffBlack() );
				 else
				 {
					ASSERT(0);
					SetSelectedColor ( noDataColor );
				 }
				 break;

			case 2:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetMeasurement(minCol-1) );
				 break;

			case 3:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetNearBlack(minCol-1) );
				 break;

			case 4:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetNearWhite(minCol-1) );
				 break;

			case 5:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetRedSat(minCol-1) );
				 break;

			case 6:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetGreenSat(minCol-1) );
				 break;

			case 7:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetBlueSat(minCol-1) );
				 break;

			case 8:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetYellowSat(minCol-1) );
				 break;

			case 9:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetCyanSat(minCol-1) );
				 break;

			case 10:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetMagentaSat(minCol-1) );
				 break;

			case 11:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetCC24Sat(minCol-1) );
				 break;

			case 12:
				 switch ( minCol )
				 {
					case 1:
						 SetSelectedColor ( GetDocument()->GetMeasure()->GetOnOffBlack() );
						 break;

					case 2:
						 SetSelectedColor ( GetDocument()->GetMeasure()->GetOnOffWhite() );
						 break;

					case 3:
						 SetSelectedColor ( GetDocument()->GetMeasure()->GetAnsiBlack() );
						 break;

					case 4:
						 SetSelectedColor ( GetDocument()->GetMeasure()->GetAnsiWhite() );
						 break;
				 }
				 break;
		}

		if (m_editCheckButton.GetCheck()!=BST_CHECKED)
			m_pGrayScaleGrid->SetSelectedRange(1,minCol,3,minCol,FALSE);	// Select entire column
	}
	GetDocument()->UpdateAllViews(this, UPD_SELECTEDCOLOR);
//	(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls

	// Grid -> 3D viewer half of the selection sync: map the selected column back
	// to the scene point's source identity (the inverse of the mapping in
	// C3DColorView::PushSelectionToMainView) and halo it in any live 3D view.
	// A multi-column or row-header selection resolves to nothing and clears it.
	// Nothing below runs without a 3D view: the saturation branch has to sync
	// the stimulus-level store, which is not work a selection should be doing.
	BOOL bHas3DView = FALSE;
	POSITION posFind = GetDocument()->GetFirstViewPosition();
	while ( posFind != NULL && !bHas3DView )
	{
		CView * pView = GetDocument()->GetNextView ( posFind );
		bHas3DView = ( pView != NULL && pView->IsKindOf ( RUNTIME_CLASS ( C3DColorView ) ) );
	}
	if ( !bHas3DView )
		return;

	int srcType = -1, srcA = 0, srcB = 0, srcC = 0;
	if ( maxCol == minCol && minCol >= 1 )
	{
		switch ( m_displayMode )
		{
			case 0:  srcType = C3DColorView::SRC_GRAY;      srcA = minCol - 1; break;

			case 1:  if ( minCol < 4 )
					 {
						srcType = C3DColorView::SRC_PRIMARY;
						srcA = minCol - 1;
					 }
					 else if ( minCol < 7 )
					 {
						srcType = C3DColorView::SRC_SECONDARY;
						srcA = minCol - 4;
					 }
					 break;	// white (7) and black (8) have no scene point

			case 2:  srcType = C3DColorView::SRC_FREE;      srcA = minCol - 1; break;

			case 3:  srcType = C3DColorView::SRC_NEARBLACK; srcA = minCol - 1; break;

			case 4:  srcType = C3DColorView::SRC_NEARWHITE; srcA = minCol - 1; break;

			case 5: case 6: case 7: case 8: case 9: case 10:
				 {
					// The scene holds every measured stimulus level, so the
					// identity also needs the store index of the bound one.
					CMeasure * pSatMeasure = GetDocument()->GetMeasure();
					int nLevels = pSatMeasure->GetSatLevelCount();
					double activeLevel = pSatMeasure->GetActiveSatLevel();
					srcType = C3DColorView::SRC_SAT;
					srcA = minCol - 1;
					srcB = m_displayMode - 5;	// 0=R 1=G 2=B 3=Y 4=C 5=M
					srcC = -1;					// matches nothing if the bound level is absent
					for ( int l = 0 ; l < nLevels ; l ++ )
					{
						// 1e-4 is the store's own notion of "same level"
						// (FindSatLevelIndex); a tighter compare could miss.
						if ( fabs ( pSatMeasure->GetSatLevelAt ( l ) - activeLevel ) < 1e-4 )
						{
							srcC = l;
							break;
						}
					}
				 }
				 break;

			case 11: srcType = C3DColorView::SRC_CC24;      srcA = minCol - 1; break;

			// 12 (contrast) has no scene points; 13 (display profile) has no grid --
			// there the profile pane's inspect drives the viewer (OnProfilePaneAction).
		}
	}

	POSITION pos3D = GetDocument()->GetFirstViewPosition();
	while ( pos3D != NULL )
	{
		CView * pView = GetDocument()->GetNextView ( pos3D );
		if ( pView != NULL && pView->IsKindOf ( RUNTIME_CLASS ( C3DColorView ) ) )
			( (C3DColorView *) pView )->SelectMeasurePoint ( srcType, srcA, srcB, srcC );
	}
}

void CMainView::OnXyzRadio() 
{
	m_editCheckButton.EnableWindow ( ! m_AdjustXYZCheckButton.GetCheck () );
	m_displayType=HCFR_XYZ_VIEW;
	InitGrid();	// to update row labels
	UpdateGrid();
}

void CMainView::OnSensorrgbRadio() 
{
	m_editCheckButton.EnableWindow ( ! m_AdjustXYZCheckButton.GetCheck () );
	m_displayType=HCFR_SENSORRGB_VIEW;
	InitGrid();	// to update row labels
	UpdateGrid();
}

void CMainView::OnRgbRadio() 
{
	m_editCheckButton.EnableWindow ( ! m_AdjustXYZCheckButton.GetCheck () );
	m_displayType=HCFR_RGB_VIEW;
	InitGrid();	// to update row labels
	UpdateGrid();
}

void CMainView::OnXyz2Radio() 
{
	m_editCheckButton.SetCheck ( BST_UNCHECKED );
	m_editCheckButton.EnableWindow ( FALSE );
	if (::IsWindow(m_statsBar.GetSafeHwnd())) m_statsBar.Invalidate(FALSE);   // refresh the bar-drawn Edit checkbox
	m_displayType=HCFR_xyz2_VIEW;
	InitGrid();	// to update row labels
	UpdateGrid();
}

void CMainView::OnxyYRadio() 
{
	m_editCheckButton.EnableWindow ( ! m_AdjustXYZCheckButton.GetCheck () );
	m_displayType=HCFR_xyY_VIEW;
	InitGrid();	// to update row labels
	UpdateGrid();
}

void CMainView::OnEditgridCheck() 
{
	BOOL isEnabled=m_editCheckButton.GetCheck();
	m_pGrayScaleGrid->SetEditable(isEnabled);
	m_pGrayScaleGrid->EnableDragAndDrop(isEnabled);
}

void CMainView::OnSelchangeDisplayType()
{
	int sel = m_comboDisplayType.GetCurSel();
	if (sel < 0) return;
	int newType = (int) m_comboDisplayType.GetItemData(sel);
	m_displayType = newType;

	// Keep the (hidden) radios' checked state in sync for any code that reads them.
	CheckDlgButton(IDC_XYZ_RADIO,       newType == HCFR_XYZ_VIEW       ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_SENSORRGB_RADIO, newType == HCFR_SENSORRGB_VIEW ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_RGB_RADIO,       newType == HCFR_RGB_VIEW       ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_XYZ_RADIO2,      newType == HCFR_xyz2_VIEW      ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_XYY_RADIO,       newType == HCFR_xyY_VIEW       ? BST_CHECKED : BST_UNCHECKED);

	// Mirror the per-radio enable rule for the Edit checkbox (see OnXyz2Radio etc.).
	if (newType == HCFR_xyz2_VIEW)
	{
		m_editCheckButton.SetCheck(BST_UNCHECKED);
		m_editCheckButton.EnableWindow(FALSE);
	}
	else
		m_editCheckButton.EnableWindow(!m_AdjustXYZCheckButton.GetCheck());
	if (::IsWindow(m_statsBar.GetSafeHwnd())) m_statsBar.Invalidate(FALSE);   // refresh the bar-drawn Edit checkbox

	InitGrid();   // update row labels
	UpdateGrid();
}

void CMainView::OnDropdownComboMode()
{
	int n = m_comboMode.GetCount();
	if (n <= 0) return;
	CClientDC dc(this);
	CFont* pf = m_comboMode.GetFont();
	if (!pf) pf = GetFont();
	CFont* pOld = dc.SelectObject(pf);
	int mw = 0;
	for (int i = 0; i < n; i++)
	{
		CString it; m_comboMode.GetLBText(i, it);
		int w = dc.GetTextExtent(it, it.GetLength()).cx;
		if (w > mw) mw = w;
	}
	dc.SelectObject(pOld);
	m_comboMode.SetDroppedWidth(mw + ::GetSystemMetrics(SM_CXVSCROLL) + GetConfig()->Scale(12));
}

// Segment change on the info-pane dE filter: persist and push to the live 3D view.
void CMainView::On3DDEFilterClicked()
{
	GetConfig()->WriteProfileInt("MainView", "ThreeD dE Filter", m_3dDEFilter.GetSel());
	if ( m_infoDisplay == 13 && m_pInfoWnd )
	{
		CView * pView = ( (CSubFrame *) m_pInfoWnd ) -> GetActiveView ();
		if ( pView && pView->IsKindOf ( RUNTIME_CLASS ( C3DColorView ) ) )
			( (C3DColorView *) pView ) -> SetDEFilter ( m_3dDEFilter.GetSel () );
	}
}

void CMainView::OnSelchangeComboMode() 
{
	int	nNewMode = m_comboMode.GetCurSel ();
	CString	Msg, MsgAdd;

	StopBackgroundMeasures ();
	if (m_displayMode == 2 && nNewMode != 2)
	{
		minCol = last_Col + 1;
		last_minCol = last_Col + 1;
		m_SelectedColor = m_LastColor;
	}

	m_displayMode = nNewMode;

	if ( m_displayMode == 12 )
	{
		m_testAnsiPatternButton.ShowWindow ( SW_SHOW );
		m_refs.ShowWindow ( SW_HIDE );
	}
	else
	{
		m_testAnsiPatternButton.ShowWindow ( SW_HIDE );
		// mode 13 hides the whole right column; the pane hosts References itself
		m_refs.ShowWindow ( m_displayMode == 13 ? SW_HIDE : SW_SHOW );
	}
	if ( m_satAllLevelsButton.GetSafeHwnd () )
		m_satAllLevelsButton.ShowWindow ( ( m_displayMode >= 5 && m_displayMode <= 10 ) ? SW_SHOW : SW_HIDE );

	// mode 13 swaps the measures grid AND its satellite chrome (stats bar, value
	// display group, Go/Delete buttons) for the full-width display-profile pane;
	// the pane hosts its own Start/Stop/References controls
	if ( m_profilePane.GetSafeHwnd () )
	{
		BOOL bProfile = ( m_displayMode == 13 );
		int nShow = bProfile ? SW_HIDE : SW_SHOW;

		if ( m_pGrayScaleGrid && m_pGrayScaleGrid->GetSafeHwnd () )
			m_pGrayScaleGrid->ShowWindow ( nShow );
		if ( m_valuesStatic.GetSafeHwnd () )
			m_valuesStatic.ShowWindow ( nShow );	// etched frame behind the grid
		m_grayScaleDeleteButton.ShowWindow ( nShow );
		m_grayScaleButton.ShowWindow ( nShow );
		m_grayScaleGroup.ShowWindow ( nShow );	// pane draws its own titled frame
		if ( m_statsBar.GetSafeHwnd () )
			m_statsBar.ShowWindow ( nShow );
		if ( m_editCheckButton.GetSafeHwnd () )
			m_editCheckButton.ShowWindow ( nShow );
		if ( m_comboDisplayType.GetSafeHwnd () )
			m_comboDisplayType.ShowWindow ( nShow );
		if ( GetDlgItem ( IDC_DISPLAY_GROUP ) )
			GetDlgItem ( IDC_DISPLAY_GROUP )->ShowWindow ( nShow );

		if ( bProfile )
		{
			LayoutProfilePane ();
			m_profilePane.ShowWindow ( SW_SHOW );
			m_profilePane.RefreshState ();
		}
		else
			m_profilePane.ShowWindow ( SW_HIDE );
	}

	MsgAdd.LoadString ( IDS_CTRLCLICK_SIM );

	switch ( m_displayMode )
	{
		case 0:
			 Msg.LoadString ( IDS_GRAYSCALE );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASUREGRAYSCALE );
			 Msg += "\r\n";
			 Msg += MsgAdd;
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetIcon(HCFR_LoadPngHIcon(_T("toolbar"),_T("measure-grayscale"),(fxUseCustomColor!=FALSE),HCFR_ScaleIconPx(24,GetSafeHwnd()),HCFR_ScaleIconPx(24,GetSafeHwnd())),(HICON)NULL);
			 Msg.LoadString ( IDS_DELETEGRAYSCALE );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 1:
			 Msg.LoadString ( IDS_SECONDARYCOLORS );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASURESECONDARIES );
			 Msg += "\r\n";
			 Msg += MsgAdd;
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetIcon(HCFR_LoadPngHIcon(_T("toolbar"),_T("measure-secondaries"),(fxUseCustomColor!=FALSE),HCFR_ScaleIconPx(24,GetSafeHwnd()),HCFR_ScaleIconPx(24,GetSafeHwnd())),(HICON)NULL);
			 Msg.LoadString ( IDS_DELETESECONDARIES );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 2:
			 Msg.LoadString ( IDS_FREEMEASURES );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( GetConfig()->m_bContinuousMeasures?IDS_RUNCONTINUOUS:IDS_ONEMEASURE );
			 Msg += "\r\n";
			 Msg += MsgAdd;
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetIcon(HCFR_LoadPngHIcon(_T("toolbar"),(GetConfig()->m_bContinuousMeasures?_T("measure-continuous"):_T("measure-single")),(fxUseCustomColor!=FALSE),HCFR_ScaleIconPx(24,GetSafeHwnd()),HCFR_ScaleIconPx(24,GetSafeHwnd())),(HICON)NULL);
			 Msg.LoadString ( IDS_DELETEALLMEASURES );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 3:
			 Msg.LoadString ( IDS_NEARBLACK );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASURENEARBLACK );
			 Msg += "\r\n";
			 Msg += MsgAdd;
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetIcon(HCFR_LoadPngHIcon(_T("toolbar"),_T("measure-near-black"),(fxUseCustomColor!=FALSE),HCFR_ScaleIconPx(24,GetSafeHwnd()),HCFR_ScaleIconPx(24,GetSafeHwnd())),(HICON)NULL);
			 Msg.LoadString ( IDS_DELETENEARBLACK );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 4:
			 Msg.LoadString ( IDS_NEARWHITE );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASURENEARWHITE );
			 Msg += "\r\n";
			 Msg += MsgAdd;
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetIcon(HCFR_LoadPngHIcon(_T("toolbar"),_T("measure-near-white"),(fxUseCustomColor!=FALSE),HCFR_ScaleIconPx(24,GetSafeHwnd()),HCFR_ScaleIconPx(24,GetSafeHwnd())),(HICON)NULL);
			 Msg.LoadString ( IDS_DELETENEARWHITE );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 5:
			 Msg.LoadString ( IDS_SATRED );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASURESATRED );
			 Msg += "\r\n";
			 Msg += MsgAdd;
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetIcon(HCFR_LoadPngHIcon(_T("toolbar"),_T("sat-red"),(fxUseCustomColor!=FALSE),HCFR_ScaleIconPx(24,GetSafeHwnd()),HCFR_ScaleIconPx(24,GetSafeHwnd())),(HICON)NULL);
			 Msg.LoadString ( IDS_DELETESATRED );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 6:
			 Msg.LoadString ( IDS_SATGREEN );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASURESATGREEN );
			 Msg += "\r\n";
			 Msg += MsgAdd;
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetIcon(HCFR_LoadPngHIcon(_T("toolbar"),_T("sat-green"),(fxUseCustomColor!=FALSE),HCFR_ScaleIconPx(24,GetSafeHwnd()),HCFR_ScaleIconPx(24,GetSafeHwnd())),(HICON)NULL);
			 Msg.LoadString ( IDS_DELETESATGREEN );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 7:
			 Msg.LoadString ( IDS_SATBLUE );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASURESATBLUE );
			 Msg += "\r\n";
			 Msg += MsgAdd;
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetIcon(HCFR_LoadPngHIcon(_T("toolbar"),_T("sat-blue"),(fxUseCustomColor!=FALSE),HCFR_ScaleIconPx(24,GetSafeHwnd()),HCFR_ScaleIconPx(24,GetSafeHwnd())),(HICON)NULL);
			 Msg.LoadString ( IDS_DELETESATBLUE );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 8:
			 Msg.LoadString ( IDS_SATYELLOW );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASURESATYELLOW );
			 Msg += "\r\n";
			 Msg += MsgAdd;
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetIcon(HCFR_LoadPngHIcon(_T("toolbar"),_T("sat-yellow"),(fxUseCustomColor!=FALSE),HCFR_ScaleIconPx(24,GetSafeHwnd()),HCFR_ScaleIconPx(24,GetSafeHwnd())),(HICON)NULL);
			 Msg.LoadString ( IDS_DELETESATYELLOW );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 9:
			 Msg.LoadString ( IDS_SATCYAN );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASURESATCYAN );
			 Msg += "\r\n";
			 Msg += MsgAdd;
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetIcon(HCFR_LoadPngHIcon(_T("toolbar"),_T("sat-cyan"),(fxUseCustomColor!=FALSE),HCFR_ScaleIconPx(24,GetSafeHwnd()),HCFR_ScaleIconPx(24,GetSafeHwnd())),(HICON)NULL);
			 Msg.LoadString ( IDS_DELETESATCYAN );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 10:
			 Msg.LoadString ( IDS_SATMAGENTA );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASURESATMAGENTA );
			 Msg += "\r\n";
			 Msg += MsgAdd;
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetIcon(HCFR_LoadPngHIcon(_T("toolbar"),_T("sat-magenta"),(fxUseCustomColor!=FALSE),HCFR_ScaleIconPx(24,GetSafeHwnd()),HCFR_ScaleIconPx(24,GetSafeHwnd())),(HICON)NULL);
			 Msg.LoadString ( IDS_DELETESATMAGENTA );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 11:
			 Msg.LoadString ( IDS_SATCC24 );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASURESATCC24 );
			 Msg += "\r\n";
			 Msg += MsgAdd;
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetIcon(HCFR_LoadPngHIcon(_T("toolbar"),_T("sat-colorchecker"),(fxUseCustomColor!=FALSE),HCFR_ScaleIconPx(24,GetSafeHwnd()),HCFR_ScaleIconPx(24,GetSafeHwnd())),(HICON)NULL);
			 Msg.LoadString ( IDS_DELETESATCC24 );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 12:
			 Msg.LoadString ( IDS_CONTRAST );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASURECONTRAST );
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetIcon(HCFR_LoadPngHIcon(_T("toolbar"),_T("measure-contrast"),(fxUseCustomColor!=FALSE),HCFR_ScaleIconPx(24,GetSafeHwnd()),HCFR_ScaleIconPx(24,GetSafeHwnd())),(HICON)NULL);
			 Msg.LoadString ( IDS_DELETECONTRAST );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 13:
			 Msg.LoadString ( IDS_DISPLAYPROFILE );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASUREDISPLAYPROFILE );
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetIcon(HCFR_LoadPngHIcon(_T("toolbar"),_T("sat-colorchecker"),(fxUseCustomColor!=FALSE),HCFR_ScaleIconPx(24,GetSafeHwnd()),HCFR_ScaleIconPx(24,GetSafeHwnd())),(HICON)NULL);
			 Msg.LoadString ( IDS_DELETEDISPLAYPROFILE );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;
	}

	// Reconfigure the dropdowns with painting suppressed on the view AND the two
	// runtime combos, then repaint once. Otherwise ShowWindow / OnSize paint the
	// stimulus combo's transient (tall, pre-collapse) state, flashing a black box
	// below it -- the dialog-template mode combo avoids this by collapsing while the
	// view is still invisible at creation.
	SetRedraw ( FALSE );
	if ( m_comboSteps.GetSafeHwnd () )     m_comboSteps.SetRedraw ( FALSE );
	if ( m_comboStimLevel.GetSafeHwnd () ) m_comboStimLevel.SetRedraw ( FALSE );
	UpdateParamCombos();
	if ( m_bPositionsInit ) { LayoutTopRow(); OnSize(0,0,0); }	// info column starts after this mode's dropdowns
	if ( m_comboSteps.GetSafeHwnd () )     m_comboSteps.SetRedraw ( TRUE );
	if ( m_comboStimLevel.GetSafeHwnd () ) m_comboStimLevel.SetRedraw ( TRUE );
	SetRedraw ( TRUE );
	RedrawWindow ( NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN );

	InitGrid(true);

	if ( m_pGrayScaleGrid)
	{
		if ( m_pGrayScaleGrid->GetSelectedCellRange().IsValid () )
		{
			m_pGrayScaleGrid->SetSelectedRange(-1,-1,-1,-1);
			m_pGrayScaleGrid->SetFocusCell(-1,-1);
			SetSelectedColor ( noDataColor );
//			(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
		}
	}

	if(m_pGrayScaleGrid)
		UpdateGrid();
	if ( IsMeasureSweepActive() )
	{
		if ( IsAllLevelsSweepActive() )
			SetAllLevelsButtonStop ( TRUE );
		else
			SetMeasureButtonStop ( TRUE );
	}
}

BOOL CMainView::CurrentModeSweepHasData()
{
	CMeasure * pMeasure = GetDocument () -> GetMeasure ();
	int i;

	switch ( m_displayMode )
	{
		case 0:
			 for ( i = 0; i < pMeasure->GetGrayScaleSize(); i++ )
				if ( pMeasure->GetGray(i).isValid() )
					return TRUE;
			 break;

		case 3:
			 for ( i = 0; i < pMeasure->GetNearBlackScaleSize(); i++ )
				if ( pMeasure->GetNearBlack(i).isValid() )
					return TRUE;
			 break;

		case 4:
			 for ( i = 0; i < pMeasure->GetNearWhiteScaleSize(); i++ )
				if ( pMeasure->GetNearWhite(i).isValid() )
					return TRUE;
			 break;

		case 5: case 6: case 7: case 8: case 9: case 10:
			 // Changing the saturation step count resizes all six hue sweeps.
			 for ( i = 0; i < pMeasure->GetSaturationSize(); i++ )
			 {
				if ( pMeasure->GetRedSat(i).isValid()    || pMeasure->GetGreenSat(i).isValid()  ||
					 pMeasure->GetBlueSat(i).isValid()   || pMeasure->GetYellowSat(i).isValid() ||
					 pMeasure->GetCyanSat(i).isValid()   || pMeasure->GetMagentaSat(i).isValid() )
					return TRUE;
			 }
			 break;
	}
	return FALSE;
}

void CMainView::UpdateParamCombos()
{
	if ( ! m_comboSteps.GetSafeHwnd () )
		return;

	CMeasure *	pMeasure = GetDocument () -> GetMeasure ();
	BOOL		bShowSteps = FALSE;
	CString		str;

	m_comboSteps.ResetContent ();

	switch ( m_displayMode )
	{
		case 0:		// Grayscale: the preset list (same as the Scale Sizes dialog)
			 {
				const GrayScalePreset *	pPresets = GetGrayScalePresets ();
				int						nPresetCount = GetGrayScalePresetCount ();
				for ( int k = 0; k < nPresetCount; k++ )
				{
					int idx = m_comboSteps.AddString ( pPresets[k].name );
					m_comboSteps.SetItemData ( idx, k );
				}
				CString sCustom; sCustom.LoadString ( IDS_PARAM_CUSTOM );
				int iCustom = m_comboSteps.AddString ( sCustom );
				m_comboSteps.SetItemData ( iCustom, (DWORD_PTR) -1 );
				int nMatch = pMeasure -> GetGrayScalePreset ();
				m_comboSteps.SetCurSel ( nMatch >= 0 ? nMatch : iCustom );
				m_comboSteps.SetDroppedWidth ( GetConfig () -> Scale ( 220 ) );	// preset names are long
				bShowSteps = TRUE;
			 }
			 break;

		case 3: case 4:
		case 5: case 6: case 7: case 8: case 9: case 10:
			 {
				// Near black / near white top out at 10 steps; saturation sweeps go higher.
				static const int nSatChoices  [] = { 2, 4, 5, 8, 10, 12, 16, 20, 25, 32, 50 };
				static const int nNbNwChoices [] = { 2, 4, 5, 6, 8, 10 };
				bool bNbNw = ( m_displayMode == 3 || m_displayMode == 4 );
				const int *	pChoices = bNbNw ? nNbNwChoices : nSatChoices;
				int nChoiceCount = bNbNw ? ( sizeof(nNbNwChoices)/sizeof(nNbNwChoices[0]) )
										 : ( sizeof(nSatChoices)/sizeof(nSatChoices[0]) );
				int nCur;
				if ( m_displayMode == 3 )
					nCur = pMeasure -> GetNearBlackScaleSize () - 1;
				else if ( m_displayMode == 4 )
					nCur = pMeasure -> GetNearWhiteScaleSize () - 1;
				else
					nCur = pMeasure -> GetSaturationSize () - 1;

				BOOL bListed = FALSE;
				for ( int k = 0; k < nChoiceCount; k++ )
				{
					if ( pChoices[k] == nCur )
						bListed = TRUE;
					if ( ! bListed && pChoices[k] > nCur )
					{
						// keep the (custom) current value visible, in sorted order
						str.Format ( _T("%d"), nCur );
						int idx = m_comboSteps.AddString ( str );
						m_comboSteps.SetItemData ( idx, nCur );
						bListed = TRUE;
					}
					str.Format ( _T("%d"), pChoices[k] );
					int idx = m_comboSteps.AddString ( str );
					m_comboSteps.SetItemData ( idx, pChoices[k] );
				}
				for ( int k = 0; k < m_comboSteps.GetCount (); k++ )
					if ( (int) m_comboSteps.GetItemData ( k ) == nCur ) { m_comboSteps.SetCurSel ( k ); break; }
				m_comboSteps.SetDroppedWidth ( GetConfig () -> Scale ( 64 ) );	// bare step numbers; reset any wide width left by grayscale/CC modes
				bShowSteps = TRUE;
			 }
			 break;

		case 11:	// Color Checker: the CC set list (same order/labels as the References dialog)
			 {
				// Names indexed by CCPatterns value (see libHCFR/Color.h); English-only,
				// matching the other runtime-built UI on this branch.
				static const char * const kCCSetNames [] = {
					"GCD classic", "MCD classic", "Pantone skin tones", "CalMAN Classic",
					"CalMAN SG skin tones", "ChromaPure skin tones", "CalMAN SG", "RGB Luminance Axis",
					"CM 4-Point Luminance", "CM 5-Point Luminance", "CM 10-Point Luminance",
					"CM 4-Point Saturation (100AMP)", "CM 4-Point Saturation (75AMP)",
					"CM 5-Point Saturation (100AMP)", "CM 5-Point Saturation (75AMP)",
					"CM 10-Point Saturation (100AMP)", "CM 10-Point Saturation (75AMP)",
					"CM 6-Point Near Black", "CM Dynamic Range (Clipping)", "BT2020HDR_50_50",
					"LG_540_2016", "LG_540_2017", "LG_1000_2017", "LG_4000_2017", "LG UK65xx 20-Point",
					"LG 2018 OLED V1 20-Point", "LG 2018 OLED V2 20-Point", "LG 2018 OLED V3 20-Point",
					"LG 2019 OLED 10-Point", "LG 2019 OLED 22-Point", "LG 2020 OLED 10-Point",
					"LG 2020 OLED 22-Point", "LG 2021 OLED 10-Point", "LG 2021 OLED 22-Point",
					"Random 250", "Random 500", "User defined"
				};
				int nCCMode = (int) GetConfig () -> m_CCMode;
				for ( int k = 0; k < (int)( sizeof(kCCSetNames)/sizeof(kCCSetNames[0]) ); k++ )
				{
					int idx = m_comboSteps.AddString ( kCCSetNames[k] );
					m_comboSteps.SetItemData ( idx, k );
					if ( k == nCCMode )
						m_comboSteps.SetCurSel ( idx );
				}
				m_comboSteps.SetDroppedWidth ( GetConfig () -> Scale ( 240 ) );	// CC set names are long
				bShowSteps = TRUE;
			 }
			 break;
	}

	m_comboSteps.ShowWindow ( bShowSteps ? SW_SHOW : SW_HIDE );

	// Stimulus level: saturation modes only. The list is the configured capture
	// levels plus any level already measured in this document.
	BOOL bShowStim = ( m_displayMode >= 5 && m_displayMode <= 10 );
	if ( bShowStim )
	{
		std::vector<int> pcts;
		GetSatStimLevelPercents ( pcts );	// configured list, plus measured + active below

		// Add stored levels that actually hold measurements. Selecting a level
		// creates an empty store entry, so skipping the empty ones keeps the list
		// to the configured intervals + genuinely-measured off-interval levels.
		int nStored = pMeasure -> GetSatLevelCount ();	// hoisted: each call re-syncs the store
		for ( int s = 0; s < nStored; s++ )
		{
			const CSatLevelSet & set = pMeasure -> GetSatLevelSet ( s );
			bool bHasData = false;
			for ( int c = 0; c < 6 && ! bHasData; c++ )
				for ( size_t i = 0; i < set.sat[c].size () && ! bHasData; i++ )
					if ( set.sat[c][i].isValid () )
						bHasData = true;
			if ( bHasData )
				pcts.push_back ( (int) floor ( pMeasure -> GetSatLevelAt ( s ) * 100.0 + 0.5 ) );
		}

		int nActivePct = (int) floor ( pMeasure -> GetActiveSatLevel () * 100.0 + 0.5 );
		pcts.push_back ( nActivePct );	// the level being viewed always appears

		std::sort ( pcts.begin (), pcts.end () );
		pcts.erase ( std::unique ( pcts.begin (), pcts.end () ), pcts.end () );

		m_comboStimLevel.ResetContent ();
		for ( size_t k = 0; k < pcts.size (); k++ )
		{
			str.Format ( _T("%d%%"), pcts[k] );
			int idx = m_comboStimLevel.AddString ( str );
			m_comboStimLevel.SetItemData ( idx, pcts[k] );
			if ( pcts[k] == nActivePct )
				m_comboStimLevel.SetCurSel ( idx );
		}
		// Level presets: a separator, then Quick/Standard/Fine. Selecting one
		// rewrites the configured SatStimLevels list (see OnSelchangeComboStimLevel).
		m_comboStimLevel.SetItemData ( m_comboStimLevel.AddString ( _T("--------------------") ), (DWORD_PTR) STIM_SEP );
		CString sPreset;
		sPreset.LoadString ( IDS_STIMPRESET_QUICK );    m_comboStimLevel.SetItemData ( m_comboStimLevel.AddString ( sPreset ), (DWORD_PTR) STIM_QUICK );
		sPreset.LoadString ( IDS_STIMPRESET_STANDARD ); m_comboStimLevel.SetItemData ( m_comboStimLevel.AddString ( sPreset ), (DWORD_PTR) STIM_STANDARD );
		sPreset.LoadString ( IDS_STIMPRESET_FINE );     m_comboStimLevel.SetItemData ( m_comboStimLevel.AddString ( sPreset ), (DWORD_PTR) STIM_FINE );
		m_comboStimLevel.SetDroppedWidth ( GetConfig () -> Scale ( 150 ) );	// preset labels are longer than "100%"
	}
	m_comboStimLevel.ShowWindow ( bShowStim ? SW_SHOW : SW_HIDE );

	// Captions under each dropdown. The mode combo is always present; the steps
	// caption reads "Preset" for grayscale (named presets) and "Steps" otherwise.
	if ( m_lblMode.GetSafeHwnd () )
	{
		CString sCap;
		sCap.LoadString ( IDS_PARAM_MODE );     m_lblMode.SetWindowText ( sCap );
		m_lblMode.ShowWindow ( SW_SHOW );
		sCap.LoadString ( m_displayMode == 11 ? IDS_PARAM_CCSET : IDS_PARAM_STEPS );  m_lblSteps.SetWindowText ( sCap );
		m_lblSteps.ShowWindow ( bShowSteps ? SW_SHOW : SW_HIDE );
		sCap.LoadString ( IDS_PARAM_STIMULUS ); m_lblStim.SetWindowText ( sCap );
		m_lblStim.ShowWindow ( bShowStim ? SW_SHOW : SW_HIDE );
	}
}

void CMainView::OnSelchangeComboSteps()
{
	if ( IsMeasureSweepActive () )
	{
		UpdateParamCombos ();	// revert the selection, no changes mid-sweep
		return;
	}

	CMeasure *	pMeasure = GetDocument () -> GetMeasure ();
	int			sel = m_comboSteps.GetCurSel ();
	if ( sel < 0 )
		return;
	int			data = (int) m_comboSteps.GetItemData ( sel );

	if ( m_displayMode == 11 )
	{
		// Color Checker: the dropdown selects the global CC pattern set (m_CCMode),
		// exactly as the References dialog does. Reload the set's colors and refresh.
		if ( data == (int) GetConfig () -> m_CCMode )
			return;
		GetConfig () -> m_CCMode = (CCPatterns) data;
		GetConfig () -> m_referencesPropertiesPage.m_CCMode = data;	// keep the dialog's DDX var in sync
		GuardCCModeOutputRange ( data );	// import-range guard (USER patch list)
		GetConfig () -> WriteProfileInt ( "References", "CCMode", data );
		GetConfig () -> GetCColors ();	// reload the set's target colors / patch count
		GetDocument () -> UpdateAllViews ( NULL, UPD_EVERYTHING );
		return;
	}

	if ( m_displayMode == 0 )
	{
		if ( data < 0 )
		{
			// "Custom...": the Scale Sizes dialog has the free-form step count + IRE mode
			CScaleSizes dlg ( GetDocument () );
			dlg.DoModal ();
			UpdateParamCombos ();
			return;
		}
		if ( data == pMeasure -> GetGrayScalePreset () )
			return;
		CString sMsg, sTitle; sMsg.LoadString ( IDS_CONFIRM_CLEAR_GRAY ); sTitle.LoadString ( IDS_STEPS_TITLE );
		if ( CurrentModeSweepHasData () &&
			 GetColorApp()->InMeasureMessageBox ( sMsg, sTitle, MB_ICONQUESTION | MB_YESNO ) != IDYES )
		{
			UpdateParamCombos ();
			return;
		}
		const GrayScalePreset *	pPresets = GetGrayScalePresets ();
		CArray<double,double>	levels;
		levels.SetSize ( pPresets[data].count );
		for ( int i = 0; i < pPresets[data].count; i++ )
			levels[i] = pPresets[data].levels[i];
		pMeasure -> SetGrayScaleLevels ( levels.GetData (), (int) levels.GetSize () );
		GetConfig()->WriteProfileInt("Scale Sizes","GrayPreset",data);
		GetConfig()->WriteProfileInt("Scale Sizes","GrayCustomN",pPresets[data].count - 1);
		GetConfig()->WriteProfileInt("Scale Sizes","Gray",pPresets[data].count - 1);
	}
	else
	{
		int nCur;
		if ( m_displayMode == 3 )
			nCur = pMeasure -> GetNearBlackScaleSize () - 1;
		else if ( m_displayMode == 4 )
			nCur = pMeasure -> GetNearWhiteScaleSize () - 1;
		else
			nCur = pMeasure -> GetSaturationSize () - 1;
		if ( data == nCur )
			return;

		CString sWarn, sTitle;
		sWarn.LoadString ( m_displayMode == 3 ? IDS_CONFIRM_CLEAR_NB
										      : ( m_displayMode == 4 ? IDS_CONFIRM_CLEAR_NW : IDS_CONFIRM_CLEAR_SAT ) );
		sTitle.LoadString ( IDS_STEPS_TITLE );
		if ( CurrentModeSweepHasData () &&
			 GetColorApp()->InMeasureMessageBox ( sWarn, sTitle, MB_ICONQUESTION | MB_YESNO ) != IDYES )
		{
			UpdateParamCombos ();
			return;
		}

		if ( m_displayMode == 3 )
		{
			pMeasure -> SetNearBlackScaleSize ( data + 1 );
			GetConfig()->WriteProfileInt("Scale Sizes","Near Black",data);
		}
		else if ( m_displayMode == 4 )
		{
			pMeasure -> SetNearWhiteScaleSize ( data + 1 );
			GetConfig()->WriteProfileInt("Scale Sizes","Near White",data);
		}
		else
		{
			pMeasure -> SetSaturationSize ( data + 1 );
			GetConfig()->WriteProfileInt("Scale Sizes","Saturations",data);
		}
	}

	GetDocument () -> SetModifiedFlag ();
	GetDocument () -> UpdateAllViews ( NULL );
	UpdateParamCombos ();
}

void CMainView::OnSelchangeComboStimLevel()
{
	if ( IsMeasureSweepActive () )
	{
		UpdateParamCombos ();	// revert the selection, no level swap mid-sweep
		return;
	}

	int sel = m_comboStimLevel.GetCurSel ();
	if ( sel < 0 )
		return;
	int data = (int) m_comboStimLevel.GetItemData ( sel );

	if ( data >= 1 )	// a level percentage: view/measure at that stimulus level
	{
		// Non-destructive: stores the bound sweeps, then binds the chosen level's
		// set (creating an empty one if it has never been measured). Every view
		// follows via the update broadcast.
		if ( GetDocument () -> GetMeasure () -> BindSatLevel ( (double) data / 100.0 ) )
		{
			GetDocument () -> SetModifiedFlag ();
			GetDocument () -> UpdateAllViews ( NULL );
		}
		return;
	}

	// Preset command: rewrite the configured capture list, then repopulate. This
	// changes which levels are offered / measured by "All stim", not the active
	// (viewed) level. The separator just reverts the selection.
	LPCTSTR pList = NULL;
	switch ( data )
	{
		case STIM_QUICK:    pList = _T("25 50 75 100"); break;
		case STIM_STANDARD: pList = _T("10 20 30 40 50 60 70 80 90 100"); break;
		case STIM_FINE:     pList = _T("5 10 15 20 25 30 35 40 45 50 55 60 65 70 75 80 85 90 95 100"); break;
	}
	if ( ! pList )		// the separator: just revert the selection to the active level
	{
		UpdateParamCombos ();
		return;
	}

	GetConfig () -> WriteProfileString ( "Scale Sizes", "SatStimLevels", pList );

	// If the active level isn't one of the new interval's levels, snap it to the
	// nearest one so it doesn't linger as an off-interval straggler in the list.
	CMeasure *	pMeasure = GetDocument () -> GetMeasure ();
	int			nActive = (int) floor ( pMeasure -> GetActiveSatLevel () * 100.0 + 0.5 );
	std::vector<int> newPcts;
	GetSatStimLevelPercents ( newPcts );	// the list just written
	bool bSnapped = false;
	if ( ! newPcts.empty () && std::find ( newPcts.begin (), newPcts.end (), nActive ) == newPcts.end () )
	{
		int best = newPcts[0], bestD = ( best > nActive ) ? best - nActive : nActive - best;
		for ( size_t i = 1; i < newPcts.size (); i++ )
		{
			int d = ( newPcts[i] > nActive ) ? newPcts[i] - nActive : nActive - newPcts[i];
			if ( d < bestD ) { best = newPcts[i]; bestD = d; }
		}
		if ( pMeasure -> BindSatLevel ( (double) best / 100.0 ) )
			{ GetDocument () -> SetModifiedFlag (); bSnapped = true; }
	}

	if ( bSnapped )
		GetDocument () -> UpdateAllViews ( NULL, UPD_ALLSATURATIONS );	// new list + snapped level, all views
	else
		UpdateParamCombos ();	// just reflect the new list
}

void CMainView::SetMeasureButtonForMode()
{
	if ( m_measureGoCaption.IsEmpty() )
		m_grayScaleButton.GetWindowText ( m_measureGoCaption );
	LPCTSTR name = _T("measure-grayscale");
	UINT    tip  = IDS_MEASUREGRAYSCALE;
	BOOL    bSimHint = TRUE;
	switch ( m_displayMode )
	{
		case 1:  name = _T("measure-secondaries"); tip = IDS_MEASURESECONDARIES; break;
		case 2:  name = GetConfig()->m_bContinuousMeasures ? _T("measure-continuous") : _T("measure-single");
		         tip  = GetConfig()->m_bContinuousMeasures ? IDS_RUNCONTINUOUS : IDS_ONEMEASURE; break;
		case 3:  name = _T("measure-near-black"); tip = IDS_MEASURENEARBLACK; break;
		case 4:  name = _T("measure-near-white"); tip = IDS_MEASURENEARWHITE; break;
		case 5:  name = _T("sat-red"); tip = IDS_MEASURESATRED; break;
		case 6:  name = _T("sat-green"); tip = IDS_MEASURESATGREEN; break;
		case 7:  name = _T("sat-blue"); tip = IDS_MEASURESATBLUE; break;
		case 8:  name = _T("sat-yellow"); tip = IDS_MEASURESATYELLOW; break;
		case 9:  name = _T("sat-cyan"); tip = IDS_MEASURESATCYAN; break;
		case 10: name = _T("sat-magenta"); tip = IDS_MEASURESATMAGENTA; break;
		case 11: name = _T("sat-colorchecker"); tip = IDS_MEASURESATCC24; break;
		case 12: name = _T("measure-contrast"); tip = IDS_MEASURECONTRAST; bSimHint = FALSE; break;
	}
	m_grayScaleButton.SetIcon ( HCFR_LoadPngHIcon ( _T("toolbar"), name, (fxUseCustomColor!=FALSE), HCFR_ScaleIconPx(24,GetSafeHwnd()), HCFR_ScaleIconPx(24,GetSafeHwnd()) ), (HICON)NULL );
	CString sTip; sTip.LoadString ( tip );
	if ( bSimHint )
	{
		CString sSim; sSim.LoadString ( IDS_CTRLCLICK_SIM );
		sTip += _T("\r\n"); sTip += sSim;
	}
	m_grayScaleButton.SetTooltipText ( sTip );
	m_grayScaleButton.SetWindowText ( m_measureGoCaption );
	m_grayScaleButton.SetRoundedBorder ( ButtonBorderColor() );   // d2d2d2, same as the other buttons
}

void CMainView::SetMeasureButtonStop(BOOL bStop)
{
	if ( bStop )
	{
		m_grayScaleButton.SetIcon ( HCFR_LoadPngHIcon ( _T("toolbar"), _T("measure-stop"), (fxUseCustomColor!=FALSE), HCFR_ScaleIconPx(24,GetSafeHwnd()), HCFR_ScaleIconPx(24,GetSafeHwnd()) ), (HICON)NULL );
		CString sStop; sStop.LoadString ( IDS_STOPSWEEP ); m_grayScaleButton.SetTooltipText ( sStop );
		CString sBtn; sBtn.LoadString ( IDS_STOP_BTN ); m_grayScaleButton.SetWindowText ( sBtn );
		m_grayScaleButton.SetRoundedBorder ( RGB(211,47,47) );   // red: measuring (click to stop)
	}
	else
		SetMeasureButtonForMode ();
}

void CMainView::SetAllLevelsButtonStop(BOOL bStop)
{
	if ( !m_satAllLevelsButton.GetSafeHwnd () )
		return;

	if ( bStop )
	{
		m_satAllLevelsButton.SetIcon ( HCFR_LoadPngHIcon ( _T("toolbar"), _T("measure-stop"), (fxUseCustomColor!=FALSE), HCFR_ScaleIconPx(24,GetSafeHwnd()), HCFR_ScaleIconPx(24,GetSafeHwnd()) ), (HICON)NULL );
		CString sStop; sStop.LoadString ( IDS_STOPSWEEP ); m_satAllLevelsButton.SetTooltipText ( sStop );
		CString sBtn; sBtn.LoadString ( IDS_STOP_BTN ); m_satAllLevelsButton.SetWindowText ( sBtn );
		m_satAllLevelsButton.SetRoundedBorder ( RGB(211,47,47) );   // red: measuring (click to stop)
	}
	else
	{
		m_satAllLevelsButton.SetIcon ( HCFR_LoadPngHIcon ( _T("toolbar"), _T("measure-all-sat-stim"), (fxUseCustomColor!=FALSE), HCFR_ScaleIconPx(24,GetSafeHwnd()), HCFR_ScaleIconPx(24,GetSafeHwnd()) ), (HICON)NULL );
		CString sBtn; sBtn.LoadString ( IDS_ALLSTIM_BTN ); m_satAllLevelsButton.SetWindowText ( sBtn );
		CString sTip; sTip.LoadString ( IDS_ALLSTIM_TIP ); m_satAllLevelsButton.SetTooltipText ( sTip );
		m_satAllLevelsButton.SetRoundedBorder ( ButtonBorderColor() );
	}
}

void CMainView::OnMeasureGrayScale()
{
	if ( IsMeasureSweepActive() )
	{
		GetDocument()->GetMeasure()->AbortMeasure();
		return;
	}
	if ( GetKeyState ( VK_CONTROL ) < 0 )
	{
		switch ( m_displayMode )
		{
			case 0:
				 GetDocument()->OnSimGrayscale();
				 break;

			case 1:
				 GetDocument()->OnSimSecondaries();
				 break;

			case 2:
				 GetDocument()->OnSimSingleMeasurement();
				 break;

			case 3:
				 GetDocument()->OnSimNearblack();
				 break;

			case 4:
				 GetDocument()->OnSimNearwhite();
				 break;

			case 5:
				 GetDocument()->OnSimSatRed();
				 break;

			case 6:
				 GetDocument()->OnSimSatGreen();
				 break;

			case 7:
				 GetDocument()->OnSimSatBlue();
				 break;

			case 8:
				 GetDocument()->OnSimSatYellow();
				 break;

			case 9:
				 GetDocument()->OnSimSatCyan();
				 break;

			case 10:
				 GetDocument()->OnSimSatMagenta();
				 break;
		}
	}
	else
	{
		switch ( m_displayMode )
		{
			case 0:
				 GetDocument()->OnMeasureGrayscale();
				 break;

			case 1:
				 GetDocument()->OnMeasureSecondaries();
				 break;

			case 2:
				 if ( GetConfig()->m_bContinuousMeasures )
					GetDocument()->OnContinuousMeasurement();
				 else
					GetDocument()->OnSingleMeasurement();
				 break;

			case 3:
				 GetDocument()->OnMeasureNearblack();
				 break;

			case 4:
				 GetDocument()->OnMeasureNearwhite();
				 break;

			case 5:
				 GetDocument()->OnMeasureSatRed();
				 break;

			case 6:
				 GetDocument()->OnMeasureSatGreen();
				 break;

			case 7:
				 GetDocument()->OnMeasureSatBlue();
				 break;

			case 8:
				 GetDocument()->OnMeasureSatYellow();
				 break;

			case 9:
				 GetDocument()->OnMeasureSatCyan();
				 break;

			case 10:
				 GetDocument()->OnMeasureSatMagenta();
				 break;

			case 11:
				 GetDocument()->OnMeasureSatCC24();
				 break;

			case 12:
				 GetDocument()->OnMeasureContrast();
				 break;

			case 13:
				 StartProfileCapture();
				 break;
		}
	}
}

void CMainView::LayoutProfilePane()
{
	if ( !m_profilePane.GetSafeHwnd () || !m_grayScaleGroup.GetSafeHwnd () )
		return;
	// The measures group only spans the grid band; the Display group + Go/Refs
	// buttons sit in a separate strip to its right. Span the pane across BOTH so
	// no chrome pokes out: group's top-left to the client right edge, matching
	// the full-width top-row panes above.
	CRect rcGroup;
	m_grayScaleGroup.GetWindowRect ( &rcGroup );
	ScreenToClient ( &rcGroup );

	CRect rcClient;
	GetClientRect ( &rcClient );

	int leftInset = rcGroup.left;	// mirror on the right so both margins match
	// pull the right edge in 1px more so the gap to the window edge matches the
	// other top-row panes exactly (right-aligned content follows since CW shrinks)
	CRect rc ( rcGroup.left, rcGroup.top, rcClient.right - leftInset - 1, rcGroup.bottom );
	if ( rc.Width () > 0 && rc.Height () > 0 )
		m_profilePane.MoveWindow ( &rc );
}

void CMainView::StartProfileCapture()
{
	CDataSetDoc * pDoc = GetDocument();
	if ( !pDoc || IsMeasureSweepActive() )
		return;

	// flip the info pane to the 3D viewer so the point cloud fills in live
	if ( m_comboDisplay.GetSafeHwnd () )
	{
		CString str3D;
		str3D.LoadString ( IDS_3DVIEW_NAME );
		int idx = m_comboDisplay.FindStringExact ( -1, str3D );
		if ( idx != CB_ERR && m_comboDisplay.GetCurSel () != idx )
		{
			m_comboDisplay.SetCurSel ( idx );
			OnSelchangeInfoDisplay ();
		}
	}

	// Disable the mode dropdown for the duration of the capture: the pause loop
	// pumps mouse messages (so the pane's Resume/Stop work), which would otherwise
	// let the user switch modes mid-capture and reenter OnSelchangeComboMode.
	if ( m_comboMode.GetSafeHwnd () )
		m_comboMode.EnableWindow ( FALSE );

	m_profilePane.EnterRunning ();
	pDoc->MeasureDisplayProfile ( m_profilePane.GetCubeSize (), m_profilePane.GetGrayExtras (), m_profilePane.GetDriftComp () );
	m_profilePane.LeaveRunning ();

	if ( m_comboMode.GetSafeHwnd () )
		m_comboMode.EnableWindow ( TRUE );
}

void CMainView::OnProfilePaneAction()
{
	CProfilePane::Action act = m_profilePane.GetPendingAction ();
	m_profilePane.ClearPendingAction ();
	CDataSetDoc * pDoc = GetDocument ();
	if ( !pDoc )
		return;
	CMeasure * pMeasure = pDoc->GetMeasure ();

	switch ( act )
	{
		case CProfilePane::PA_START:
			StartProfileCapture ();
			break;

		case CProfilePane::PA_PAUSE:
			// toggles; the capture loop idles between patches while set
			if ( pMeasure && IsMeasureSweepActive () )
			{
				pMeasure->m_bProfilePause = !pMeasure->m_bProfilePause;
				m_profilePane.SetPaused ( pMeasure->m_bProfilePause );
			}
			break;

		case CProfilePane::PA_STOP:
			// the loop breaks at the next patch boundary and keeps partials
			if ( pMeasure && IsMeasureSweepActive () )
			{
				pMeasure->m_bProfilePause = FALSE;
				pMeasure->m_bAbortSweep = TRUE;
			}
			break;

		case CProfilePane::PA_REFS:
			OnRefs ();
			break;

		case CProfilePane::PA_CLEAR:
			if ( pMeasure && pMeasure->HasProfileMeasures () && !IsMeasureSweepActive () )
			{
				CString msg, title;
				msg.LoadString ( IDS_CONFIRMDELETE );
				title.LoadString ( IDS_CALIBRATION );
				if ( MessageBox ( msg, title, MB_YESNO | MB_ICONQUESTION ) == IDYES )
				{
					pMeasure->ClearProfileMeasures ();
					pDoc->SetModifiedFlag ( TRUE );
					pDoc->UpdateAllViews ( NULL, UPD_DISPLAYPROFILE );
				}
			}
			break;

		case CProfilePane::PA_INSPECT:
			if ( m_profilePane.GetInspectIndex () >= 0 )
			{
				int idx = m_profilePane.GetInspectIndex ();
				SelectProfilePatch ( idx );

				// halo the patch in any live 3D view (info pane or full tab)
				POSITION pos = pDoc->GetFirstViewPosition ();
				while ( pos != NULL )
				{
					CView * pView = pDoc->GetNextView ( pos );
					if ( pView != NULL && pView->IsKindOf ( RUNTIME_CLASS ( C3DColorView ) ) )
						( (C3DColorView *)pView )->SelectProfilePoint ( idx );
				}
			}
			break;

		default:
			break;
	}
}

// Load profile patch idx into the selected-color panel AND its reference
// comparator (measured swatch + reference swatch + RGB-levels + target widget).
// Mode 13 has no data grid, so the reference (which grid population normally
// feeds via m_RefColor) must be driven here -- used by both the pane's Delete/
// inspect flow and a click on a profile point in the 3D viewer.
void CMainView::SelectProfilePatch(int idx)
{
	CDataSetDoc * pDoc = GetDocument ();
	CMeasure * pMeasure = pDoc ? pDoc->GetMeasure () : NULL;
	if ( !pMeasure || idx < 0 || idx >= pMeasure->GetProfileMeasureSize () )
		return;

	CColor sel = pMeasure->GetProfileMeasure ( idx );
	CColor profRef;
	pMeasure->GetRefProfileSat ( idx, profRef );
	CColor w = pMeasure->GetPrimeWhite ();
	if ( !w.isValid () )
		w = pMeasure->GetOnOffWhite ();
	m_RefColor = profRef;
	m_RefWhite = 1.0;
	m_YWhite = ( w.isValid () && w.GetY () > 0.0 ) ? w.GetY () : 1.0;

	// PQ HDR: GetRefProfileSat produces the internal HDR-10 scale (1.0 = 10000
	// nits). The grid applies GetHDRRefScale (= 105.95640 with tone mapping
	// off) to CC/sat refs before handing them to the comparator widgets
	// (UpdateGrid, ~line 4294); mode 13 bypasses the grid, so apply the same
	// bridge here. Unified convention: the measured white stays unrescaled.
	if ( GetConfig()->m_GammaOffsetType == 5 )
	{
		double s = pMeasure->GetHDRRefScale();
		m_RefColor.SetX( m_RefColor.GetX() * s );
		m_RefColor.SetY( m_RefColor.GetY() * s );
		m_RefColor.SetZ( m_RefColor.GetZ() * s );
	}

	if ( sel.isValid () )
		SetSelectedColor ( sel );

	m_RGBLevels.Refresh ( idx + 1, 13, pMeasure->GetProfileMeasureSize () );
	m_Target.Refresh ( pDoc->GetGenerator()->m_b16_235, idx + 1, pMeasure->GetProfileMeasureSize (), 13, pDoc, CTargetWnd::TARGET_TARGET );
}

void CMainView::OnDeleteGrayscale()
{
	if ( IsMeasureSweepActive() ) return;
	BOOL	bSelectionOnly = FALSE;
	CString	Msg, Title;
	LPARAM	lHint = UPD_EVERYTHING;

	Msg.LoadString ( IDS_CONFIRMDELETE );
	Title.LoadString ( IDS_CALIBRATION );

	// mode 13 (display profile) has no grid delete button; the pane's own Delete
	// button clears the profile via CProfilePane::PA_CLEAR / OnProfilePaneAction.

	if ( m_displayMode == 2 )
	{
		// Special case: free measurements can be deleted by selection or totally
		if(m_pGrayScaleGrid->GetSelectedCount() == 0)
		{
			// No selection: take all
			Msg.LoadString ( IDS_CONFIRMDELETEALL );
		}
		else
		{
			// Have a selection
			bSelectionOnly = TRUE;
			Msg.LoadString ( IDS_CONFIRMDELETESELECTION );
		}
	}

	if(GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONQUESTION | MB_YESNO) == IDYES)
	{
		int	j;
		CCPatterns cPat = GetConfig()->m_CCMode;
		BOOL isExtPat =( GetConfig()->m_CCMode == USER || GetConfig()->m_CCMode == CM10SAT || GetConfig()->m_CCMode == CM10SAT75 || GetConfig()->m_CCMode == CM5SAT || GetConfig()->m_CCMode == CM5SAT75 || GetConfig()->m_CCMode == CM4SAT || GetConfig()->m_CCMode == CM4SAT75 || GetConfig()->m_CCMode == CM4LUM || GetConfig()->m_CCMode == CM5LUM || GetConfig()->m_CCMode == CM10LUM || GetConfig()->m_CCMode == RANDOM250 || GetConfig()->m_CCMode == RANDOM500 || GetConfig()->m_CCMode == CM6NB || GetConfig()->m_CCMode == CMDNR || GetConfig()->m_CCMode == MASCIOR50);
		isExtPat = (isExtPat || GetConfig()->m_CCMode > 19);
		switch ( m_displayMode )
		{
			case 0:
				 for(j=0;j<GetDocument()->GetMeasure()->GetGrayScaleSize();j++)
					GetDocument()->GetMeasure()->SetGray(j,noDataColor);
				 lHint = UPD_GRAYSCALE;
				 break;

			case 1:
				 GetDocument()->GetMeasure()->SetRedPrimary(noDataColor);
				 GetDocument()->GetMeasure()->SetGreenPrimary(noDataColor);
				 GetDocument()->GetMeasure()->SetBluePrimary(noDataColor);
				 GetDocument()->GetMeasure()->SetYellowSecondary(noDataColor);
				 GetDocument()->GetMeasure()->SetCyanSecondary(noDataColor);
				 GetDocument()->GetMeasure()->SetMagentaSecondary(noDataColor);
				 GetDocument()->GetMeasure()->SetPrimeWhite(noDataColor);
				 GetDocument()->GetMeasure()->SetOnOffBlack(GetDocument()->GetMeasure()->GetOnOffBlack());
				 lHint = UPD_PRIMARIESANDSECONDARIES;
				 break;

			case 2:
				 if ( bSelectionOnly )
				 {
					CCellRange	selRange=m_pGrayScaleGrid->GetSelectedCellRange();
					GetDocument()->GetMeasure()->DeleteMeasurements(selRange.GetMinCol()-1,selRange.GetMaxCol()-selRange.GetMinCol()+1);
				 }
				 else
				 {
					GetDocument()->GetMeasure()->SetMeasurementsSize(0);
				 }
				 lHint = UPD_FREEMEASURES;
				 break;

			case 3:
				 for(j=0;j<GetDocument()->GetMeasure()->GetNearBlackScaleSize();j++)
					GetDocument()->GetMeasure()->SetNearBlack(j,noDataColor);
				 lHint = UPD_NEARBLACK;
				 break;

			case 4:
				 for(j=0;j<GetDocument()->GetMeasure()->GetNearWhiteScaleSize();j++)
					GetDocument()->GetMeasure()->SetNearWhite(j,noDataColor);
				 lHint = UPD_NEARWHITE;
				 break;

			case 5:
				 for(j=0;j<GetDocument()->GetMeasure()->GetSaturationSize();j++)
					GetDocument()->GetMeasure()->SetRedSat(j,noDataColor);
				 lHint = UPD_REDSAT;
				 break;

			case 6:
				 for(j=0;j<GetDocument()->GetMeasure()->GetSaturationSize();j++)
					GetDocument()->GetMeasure()->SetGreenSat(j,noDataColor);
				 lHint = UPD_GREENSAT;
				 break;

			case 7:
				 for(j=0;j<GetDocument()->GetMeasure()->GetSaturationSize();j++)
					GetDocument()->GetMeasure()->SetBlueSat(j,noDataColor);
				 lHint = UPD_BLUESAT;
				 break;

			case 8:
				 for(j=0;j<GetDocument()->GetMeasure()->GetSaturationSize();j++)
					GetDocument()->GetMeasure()->SetYellowSat(j,noDataColor);
				 lHint = UPD_YELLOWSAT;
				 break;

			case 9:
				 for(j=0;j<GetDocument()->GetMeasure()->GetSaturationSize();j++)
					GetDocument()->GetMeasure()->SetCyanSat(j,noDataColor);
				 lHint = UPD_CYANSAT;
				 break;

			case 10:
				 for(j=0;j<GetDocument()->GetMeasure()->GetSaturationSize();j++)
					GetDocument()->GetMeasure()->SetMagentaSat(j,noDataColor);
				 lHint = UPD_MAGENTASAT;
				 break;

			case 11:
                if ( isExtPat )
                {
    				for(j=0;j<GetConfig()->GetCColorsSize() ;j++)
	    				GetDocument()->GetMeasure()->SetCC24Sat(j,noDataColor);
	 				GetConfig()->GetCColors();
					UpdateGrid();
               }
                else
                {
                    for(j=0;j< (cPat == CCSG?96:(cPat == AXIS?71:(cPat == CMS||cPat == CPS?19:24))) ;j++)
	    				GetDocument()->GetMeasure()->SetCC24Sat(j,noDataColor);
                }
				 lHint = UPD_CC24SAT;
				 break;

			case 12:
				 GetDocument()->GetMeasure()->DeleteContrast();
				 lHint = UPD_CONTRAST;
				 break;
		}
		InitGrid();
		UpdateGrid();
		GetDocument()->UpdateAllViews(NULL, lHint);
		GetDocument()->SetModifiedFlag(TRUE);
		if ( m_pGrayScaleGrid->GetSelectedCellRange().IsValid () )
		{
			m_pGrayScaleGrid->SetSelectedRange(-1,-1,-1,-1);
			m_pGrayScaleGrid->SetFocusCell(-1,-1);
			SetSelectedColor ( noDataColor );
//			(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
		}
	}
}


void CMainView::UpdateMeasurementsAfterBkgndMeasure ()
{
	HighlightMeasuringColumn(last_minCol);	// indicate the active column during continuous/background measures
	CColor	MeasuredColor=noDataColor;
	double YWhite = -1;
	double YWhiteRefDoc = -1;
	double			Gamma,Offset = 0.0;
	int nCount = GetDocument()->GetMeasure()->GetGrayScaleSize();
	CColorReference  bRef = ((GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4)?ContainerTransportReference(GetColorReference()):(GetColorReference().m_standard == HDTVa || GetColorReference().m_standard == HDTVb)?CColorReference(HDTV):GetColorReference());

	// Retrieve gamma and offset in case user has modified
    Gamma = GetConfig()->m_GammaRef;
    GetConfig()->m_GammaAvg = Gamma; //targets can be reference power law or modified for user average gamma, BT.1886 handled separately

	if ( nCount && GetDocument()->GetMeasure()->GetGray(0).isValid() )
        GetDocument()->ComputeGammaAndOffset(&Gamma, &Offset, 1, 1, nCount, false);

    if (GetConfig()->m_useMeasuredGamma)
		GetConfig()->m_GammaAvg = (Gamma<1?2.2:floor((Gamma+.005)*100.)/100.);

    GetConfig()->SetPropertiesSheetValues();
	
	if ( GetDocument() -> GetMeasure () -> GetOnOffWhite ().isValid() )
		YWhite = GetDocument() -> GetMeasure () -> GetOnOffWhite () [ 1 ];

	int	n = GetDocument()->GetMeasure()->GetMeasurementsSize();

	if ( n > 0 )
		MeasuredColor=GetDocument()->GetMeasure()->GetMeasurement(n-1);

	int nCol = last_minCol - 1, nCnt = 11;
	switch (m_displayMode)
	{
	case (0):
		nCnt = GetDocument()->GetMeasure()->GetGrayScaleSize();
		nCol = last_minCol - 1;
		last_Col = nCol;
		last_Size = nCnt;
		last_Display = 0;
		break;
	case (2):
		nCnt = last_Size;
		nCol = last_Col;
		break;
	case(3):
		nCnt = 101;
		if (GetConfig()->m_GammaOffsetType == 5)
			nCol = (last_minCol - 1) * 2;
		else
			nCol = (last_minCol - 1);
		last_Col = nCol;
		last_Size = nCnt;
		last_Display = 3;
		break;
	case(4):
		nCnt = 101;
		nCol = GetDocument()->GetMeasure()->m_NearWhiteClipCol - GetDocument()->GetMeasure()->GetNearWhiteScaleSize() + nCol;
		last_Col = nCol;
		last_Size = nCnt;
		last_Display = 4;
		break;
	default:
		last_Display = m_displayMode;

	}
	bool isGS = FALSE;
	if ( m_displayMode == 2 && n > 0 ) //this updates freemeasures grid
	{
		// Update grid only when in measurement mode
		CColor			refColor = GetColorReference().GetWhite();
		BOOL			bAddedCol = FALSE;
		BOOL			bSpecialRef = FALSE;
		COLORREF		clrSpecial1=RGB(128,128,128), clrSpecial2=RGB(128,128,128);
		CDataSetDoc *	pDataRef = GetDataRef();

		if ( pDataRef == GetDocument () )
			pDataRef = NULL;

		if (pDataRef)
		{
			if ( pDataRef -> GetMeasure () -> GetOnOffWhite ().isValid() )
				YWhiteRefDoc = pDataRef -> GetMeasure () -> GetOnOffWhite () [ 1 ];
		}

		GV_ITEM Item;

		if ( pDataRef && pDataRef->GetMeasure()->GetMeasurementsSize() != n )
			pDataRef = NULL;

		if ( m_pGrayScaleGrid->GetColumnCount() - 1 == n)
		{
			// Remove first column and reset column headers
			m_pGrayScaleGrid -> DeleteColumn ( 1 );
			Item.mask = GVIF_TEXT;
			Item.row = 0;
			for ( Item.col = 1; Item.col < m_pGrayScaleGrid->GetColumnCount() ; Item.col ++ )
			{
				Item.strText.Format("%d",Item.col);
				m_pGrayScaleGrid->SetItem(&Item);
			}
		}

		Item.mask = GVIF_TEXT|GVIF_FORMAT;
		Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;

		CString	str;
		str.Format("%d",n);
		m_pGrayScaleGrid -> InsertColumn(str);
		m_pGrayScaleGrid -> SetItemFont ( 0, n, m_pGrayScaleGrid->GetItemFont(0,n-1) ); // Set the font to bold

		m_pGrayScaleGrid->SetItemState ( 4, n, m_pGrayScaleGrid->GetItemState(4,n) | GVIS_READONLY );
		m_pGrayScaleGrid->SetItemBkColour ( 4, n, GridBk(RGB(224,224,224)) );

		m_pGrayScaleGrid->SetItemState ( 5, n, m_pGrayScaleGrid->GetItemState(5,n) | GVIS_READONLY );
		m_pGrayScaleGrid->SetItemBkColour ( 5, n, GridBk(RGB(240,240,240)) );

		if ( pDataRef )
		{
			m_pGrayScaleGrid->SetItemState ( 6, n, m_pGrayScaleGrid->GetItemState(6,n) | GVIS_READONLY );
			m_pGrayScaleGrid->SetItemBkColour ( 6, n, GridBk(RGB(224,224,224)) );

			m_pGrayScaleGrid->SetItemState ( 7, n, m_pGrayScaleGrid->GetItemState(7,n) | GVIS_READONLY );
			m_pGrayScaleGrid->SetItemBkColour ( 7, n, GridBk(RGB(240,240,240)) );
		}

		bSpecialRef = TRUE;

		if ( MeasuredColor.GetDeltaxy ( GetColorReference().GetWhite(), bRef ) < 0.05 )
		{
			bSpecialRef = FALSE;
			double valy;
			ColorxyY tmpColor(GetColorReference().GetWhite());
			isGS = TRUE;

			if ( GetConfig ()->m_dE_gray > 0 || GetConfig ()->m_dE_form == 5 )
			{
			// Compute reference Luminance regarding actual offset and reference gamma 
             // fixed to use correct gamma predicts
             // and added option to assume perfect gamma
				double x = ( nCnt == GetDocument()->GetMeasure()->GetGrayScaleSize() )
						   ? GetDocument()->GetMeasure()->GetGrayPercent ( nCol, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235() )
						   : ArrayIndexToGrayLevel ( nCol, nCnt, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235() );
				CColor White = GetDocument() -> GetMeasure () -> GetOnOffWhite();
//	            CColor Black = GetDocument() -> GetMeasure () -> GetGray ( 0 );
	            CColor Black = GetDocument() -> GetMeasure () -> GetOnOffBlack();
				int mode = GetConfig()->m_GammaOffsetType;
				if (GetConfig()->m_colorStandard == sRGB) mode = 99;
				if (  (mode >= 4) )
			    {
					double valx = GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());
					valy = getL_EOTF
						(valx, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
			    }
			    else
			    {
					double valx=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235())+Offset)/(1.0+Offset);
						valy=pow(valx, GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef));
					if (mode == 1) //black compensation target
						valy = (Black.GetY() + ( valy * ( YWhite - Black.GetY() ) )) / YWhite;
			    }

					if (mode  == 5)
						tmpColor[2] = valy * 100. / YWhite;
					else
						tmpColor[2] = valy;

                    if (GetConfig ()->m_dE_gray == 2 || GetConfig ()->m_dE_form == 5 )
						tmpColor[2] = MeasuredColor [ 1 ] / YWhite; //perfect gamma

					refColor.SetxyYValue(tmpColor);
				}
				else
				{
					YWhite = MeasuredColor [ 1 ];
			}
		}
		else if ( MeasuredColor.GetDeltaxy ( GetDocument()->GetMeasure()->GetRefPrimary(0), bRef ) < 0.05 )
		{
			refColor = GetDocument()->GetMeasure()->GetRefPrimary(0);
			clrSpecial1 = RGB(255,192,192);
			clrSpecial2 = RGB(255,224,224);
		}
		else if ( MeasuredColor.GetDeltaxy ( GetDocument()->GetMeasure()->GetRefPrimary(1), bRef ) < 0.05 )
		{
			refColor = GetDocument()->GetMeasure()->GetRefPrimary(1);
			clrSpecial1 = RGB(192,255,192);
			clrSpecial2 = RGB(224,255,224);
		}
		else if ( MeasuredColor.GetDeltaxy ( GetDocument()->GetMeasure()->GetRefPrimary(2), bRef ) < 0.05 )
		{
			refColor = GetDocument()->GetMeasure()->GetRefPrimary(2);
			clrSpecial1 = RGB(192,192,255);
			clrSpecial2 = RGB(224,224,255);
		}
		else if ( MeasuredColor.GetDeltaxy ( GetDocument()->GetMeasure()->GetRefSecondary(0), bRef ) < 0.05 )
		{
			refColor = GetDocument()->GetMeasure()->GetRefSecondary(0);
			clrSpecial1 = RGB(255,255,192);
			clrSpecial2 = RGB(255,255,224);
		}
		else if ( MeasuredColor.GetDeltaxy ( GetDocument()->GetMeasure()->GetRefSecondary(1), bRef ) < 0.05 )
		{
			refColor = GetDocument()->GetMeasure()->GetRefSecondary(1);
			clrSpecial1 = RGB(192,255,255);
			clrSpecial2 = RGB(224,255,255);
		}
		else if ( MeasuredColor.GetDeltaxy ( GetDocument()->GetMeasure()->GetRefSecondary(2), bRef ) < 0.05 )
		{
			refColor = GetDocument()->GetMeasure()->GetRefSecondary(2);
			clrSpecial1 = RGB(255,192,255);
			clrSpecial2 = RGB(255,224,255);
		}
		else
			refColor = noDataColor;	// no recognisable target: no dE (same as UpdateGrid case 2)

		CColor refDocColor = noDataColor;

		if ( pDataRef )
			refDocColor = pDataRef->GetMeasure()->GetMeasurement(n-1);

		for( int i = 0 ; i < ( pDataRef ? 7 : 5 ) ; i ++ )
		{
			Item.row = i+1;
			Item.col = n;
			Item.strText = GetItemText ( MeasuredColor, YWhite, refColor, refDocColor, YWhiteRefDoc, i, n, 0.0, isGS );
			
			m_pGrayScaleGrid->SetItem(&Item);
			UpdateGrid();

			if ( bSpecialRef && i >= 3 )
			{
				m_pGrayScaleGrid->SetItemBkColour ( i+1, n, GridBk( i&1 ? clrSpecial1 : clrSpecial2 ) );
			}
		}

		ASSERT ( n == m_pGrayScaleGrid->GetColumnCount()-1 );

		int width = m_pGrayScaleGrid -> GetColumnWidth ( 0 );
		m_pGrayScaleGrid -> SetColumnWidth ( n, width * 11 / 10 );

		m_pGrayScaleGrid->SetSelectedRange(1,n,3,n,FALSE);	// select inserted cell
		m_pGrayScaleGrid->EnsureVisible(0,n);
		m_pGrayScaleGrid->RedrawColumn(n);
	}
	else
	{
		HighlightMeasuringColumn(last_minCol);
	}

	SetSelectedColor ( MeasuredColor );
}

void CMainView::InitButtons()
{
	CString	Msg;

	Msg.LoadString ( IDS_CONFIGURESENSOR );
	m_configSensorButton.SetIcon(HCFR_LoadPngHIcon(_T("menu"),_T("configure-sensor"),(fxUseCustomColor!=FALSE),GetConfig()->Scale(16),GetConfig()->Scale(16)),(HICON)NULL);
	m_configSensorButton.SetFont(GetFont());
	m_configSensorButton.EnableBalloonTooltip();
	m_configSensorButton.SetTooltipText(Msg);
	m_configSensorButton.SetColor(CButtonST::BTNST_COLOR_FG_IN,FxGetSysColor(COLOR_MENUTEXT));
	m_configSensorButton.SetColor(CButtonST::BTNST_COLOR_FG_OUT,FxGetSysColor(COLOR_MENUTEXT));
	m_configSensorButton.SetColor(CButtonST::BTNST_COLOR_FG_FOCUS,FxGetSysColor(COLOR_MENUTEXT));
	m_configSensorButton.SetColor(CButtonST::BTNST_COLOR_BK_IN,FxGetMenuBgColor());
	m_configSensorButton.SetColor(CButtonST::BTNST_COLOR_BK_OUT,FxGetMenuBgColor());
	m_configSensorButton.SetColor(CButtonST::BTNST_COLOR_BK_FOCUS,FxGetMenuBgColor());
	m_configSensorButton.OffsetColor(CButtonST::BTNST_COLOR_BK_IN, 30);
	m_configSensorButton.OffsetColor(CButtonST::BTNST_COLOR_FG_IN, 30);
	m_configSensorButton.SetWindowPos(NULL,0,0,24,24,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
	m_configSensorButton.SetWindowText(_T(""));
	{
		CWnd* pAvgLbl = GetDlgItem(IDC_SENSORNAME_STATIC2);
		CWnd* pAvgBtn = GetDlgItem(IDM_CONFIGURE_SENSOR2);
		if (pAvgLbl) pAvgLbl->ShowWindow(SW_HIDE);
		if (pAvgBtn) pAvgBtn->ShowWindow(SW_HIDE);
		if (!::IsWindow(m_avgLowLightCheck.GetSafeHwnd()) && pAvgLbl && pAvgBtn)
		{
			CRect rcAvgL, rcAvgB, rcAvg;
			pAvgLbl->GetWindowRect(&rcAvgL); ScreenToClient(&rcAvgL);
			pAvgBtn->GetWindowRect(&rcAvgB); ScreenToClient(&rcAvgB);
			rcAvg.left = rcAvgL.left < rcAvgB.left ? rcAvgL.left : rcAvgB.left;
			rcAvg.right = rcAvgL.right > rcAvgB.right ? rcAvgL.right : rcAvgB.right;
			CWnd* pAvgCfg = GetDlgItem(IDM_CONFIGURE_SENSOR);
			if (pAvgCfg)
			{
				CRect rcAvgCfg;
				pAvgCfg->GetWindowRect(&rcAvgCfg); ScreenToClient(&rcAvgCfg);
				if (rcAvgCfg.left - 6 > rcAvg.right) rcAvg.right = rcAvgCfg.left - 6;
			}
			rcAvg.top = rcAvgL.top < rcAvgB.top ? rcAvgL.top : rcAvgB.top;
			int hAvg = rcAvgL.Height() > rcAvgB.Height() ? rcAvgL.Height() : rcAvgB.Height();
			rcAvg.bottom = rcAvg.top + hAvg + 2;
			m_avgLowLightCheck.Create(_T(""), WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX, rcAvg, this, IDC_AVG_LOW_LIGHT);
			m_avgLowLightCheck.SetFont(GetFont());
			CString sAvg; sAvg.LoadString(IDS_AVG_LOW_LIGHT);
			m_avgLowLightCheck.SetWindowText(sAvg);
			if (!GetConfig()->m_darkTheme) FxApplyFlatCheck(m_avgLowLightCheck.GetSafeHwnd());
		}
		CSensor* pAvgS = GetDocument() ? GetDocument()->m_pSensor : NULL;
		BOOL bAvgSup = (pAvgS != NULL && pAvgS->supportsAvg());
		if (::IsWindow(m_avgLowLightCheck.GetSafeHwnd()))
		{
			m_avgLowLightCheck.EnableWindow(bAvgSup);
			m_avgLowLightCheck.SetCheck((bAvgSup && pAvgS->getAvgEnabled()) ? BST_CHECKED : BST_UNCHECKED);
			// Re-theme on every InitButtons call: the create-once guard above skips
			// re-theming on a dark<->light switch, which left this checkbox painted with
			// the previous theme's background. FxApplyDarkModeTree adds/removes the dark
			// subclass and resets the window theme; FxApplyFlatCheck restores the flat
			// light owner-draw.
			FxApplyDarkModeTree(m_avgLowLightCheck.GetSafeHwnd(), GetConfig()->m_darkTheme);
			if (!GetConfig()->m_darkTheme) FxApplyFlatCheck(m_avgLowLightCheck.GetSafeHwnd());
			m_avgLowLightCheck.Invalidate(TRUE);
		}
	}
//	m_configSensorButton.DrawTransparent(TRUE);

	Msg.LoadString ( IDS_CONFIGUREGENERATOR );
	m_configGeneratorButton.SetIcon(HCFR_LoadPngHIcon(_T("menu"),_T("configure-generator"),(fxUseCustomColor!=FALSE),GetConfig()->Scale(16),GetConfig()->Scale(16)),(HICON)NULL);
	m_configGeneratorButton.SetFont(GetFont());
	m_configGeneratorButton.EnableBalloonTooltip();
	m_configGeneratorButton.SetTooltipText(Msg);
	m_configGeneratorButton.SetColor(CButtonST::BTNST_COLOR_FG_IN,FxGetSysColor(COLOR_MENUTEXT));
	m_configGeneratorButton.SetColor(CButtonST::BTNST_COLOR_FG_OUT,FxGetSysColor(COLOR_MENUTEXT));
	m_configGeneratorButton.SetColor(CButtonST::BTNST_COLOR_FG_FOCUS,FxGetSysColor(COLOR_MENUTEXT));
	m_configGeneratorButton.SetColor(CButtonST::BTNST_COLOR_BK_IN,FxGetMenuBgColor());
	m_configGeneratorButton.SetColor(CButtonST::BTNST_COLOR_BK_OUT,FxGetMenuBgColor());
	m_configGeneratorButton.SetColor(CButtonST::BTNST_COLOR_BK_FOCUS,FxGetMenuBgColor());
	m_configGeneratorButton.OffsetColor(CButtonST::BTNST_COLOR_BK_IN, 30);
	m_configGeneratorButton.OffsetColor(CButtonST::BTNST_COLOR_FG_IN, 30);
	m_configGeneratorButton.SetWindowPos(NULL,0,0,24,24,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
	m_configGeneratorButton.SetWindowText(_T(""));
//	m_configGeneratorButton.DrawTransparent(TRUE);

	m_grayScaleButton.DrawFlatFocus(FALSE);
	m_grayScaleButton.EnableBalloonTooltip();
	SetMeasureButtonForMode();
	m_grayScaleButton.SetColor(CButtonST::BTNST_COLOR_FG_IN,FxGetSysColor(COLOR_MENUTEXT));
	m_grayScaleButton.SetColor(CButtonST::BTNST_COLOR_FG_OUT,FxGetSysColor(COLOR_MENUTEXT));
	m_grayScaleButton.SetColor(CButtonST::BTNST_COLOR_FG_FOCUS,FxGetSysColor(COLOR_MENUTEXT));
	m_grayScaleButton.SetColor(CButtonST::BTNST_COLOR_BK_IN,FxGetMenuBgColor());
	m_grayScaleButton.SetColor(CButtonST::BTNST_COLOR_BK_OUT,FxGetMenuBgColor());
	m_grayScaleButton.SetColor(CButtonST::BTNST_COLOR_BK_FOCUS,FxGetMenuBgColor());
	m_grayScaleButton.OffsetColor(CButtonST::BTNST_COLOR_BK_IN, 30);
	m_grayScaleButton.OffsetColor(CButtonST::BTNST_COLOR_FG_IN, 30);
//	m_grayScaleButton.DrawTransparent(TRUE);

	Msg.LoadString ( IDS_DELETEGRAYSCALE );
//	m_grayScaleDeleteButton.SetBitmaps(IDB_DELETE_BITMAP,RGB(255,255,255));
	m_grayScaleDeleteButton.SetIcon(IDI_TRASH_ICON,32,32);
	m_grayScaleDeleteButton.SetFont(GetFont());
	m_grayScaleDeleteButton.EnableBalloonTooltip();
	m_grayScaleDeleteButton.SetTooltipText(Msg);
	m_grayScaleDeleteButton.SetColor(CButtonST::BTNST_COLOR_FG_IN,FxGetSysColor(COLOR_MENUTEXT));
	m_grayScaleDeleteButton.SetColor(CButtonST::BTNST_COLOR_FG_OUT,FxGetSysColor(COLOR_MENUTEXT));
	m_grayScaleDeleteButton.SetColor(CButtonST::BTNST_COLOR_FG_FOCUS,FxGetSysColor(COLOR_MENUTEXT));
	m_grayScaleDeleteButton.SetColor(CButtonST::BTNST_COLOR_BK_IN,FxGetMenuBgColor());
	m_grayScaleDeleteButton.SetColor(CButtonST::BTNST_COLOR_BK_OUT,FxGetMenuBgColor());
	m_grayScaleDeleteButton.SetColor(CButtonST::BTNST_COLOR_BK_FOCUS,FxGetMenuBgColor());
	m_grayScaleDeleteButton.OffsetColor(CButtonST::BTNST_COLOR_BK_IN, 30);
	m_grayScaleDeleteButton.OffsetColor(CButtonST::BTNST_COLOR_FG_IN, 30);
//	m_grayScaleDeleteButton.DrawTransparent(TRUE);

	Msg.LoadString ( IDS_DISPLAYANSICONTRAST );
	m_testAnsiPatternButton.SetIcon(HCFR_LoadPngHIcon(_T("toolbar"),_T("measure-ansi"),(fxUseCustomColor!=FALSE),HCFR_ScaleIconPx(24,GetSafeHwnd()),HCFR_ScaleIconPx(24,GetSafeHwnd())),(HICON)NULL);
	m_testAnsiPatternButton.SetFont(GetFont());
	m_testAnsiPatternButton.EnableBalloonTooltip();
	m_testAnsiPatternButton.SetTooltipText(Msg);
	m_testAnsiPatternButton.SetColor(CButtonST::BTNST_COLOR_FG_IN,FxGetSysColor(COLOR_MENUTEXT));
	m_testAnsiPatternButton.SetColor(CButtonST::BTNST_COLOR_FG_OUT,FxGetSysColor(COLOR_MENUTEXT));
	m_testAnsiPatternButton.SetColor(CButtonST::BTNST_COLOR_FG_FOCUS,FxGetSysColor(COLOR_MENUTEXT));
	m_testAnsiPatternButton.SetColor(CButtonST::BTNST_COLOR_BK_IN,FxGetMenuBgColor());
	m_testAnsiPatternButton.SetColor(CButtonST::BTNST_COLOR_BK_OUT,FxGetMenuBgColor());
	m_testAnsiPatternButton.SetColor(CButtonST::BTNST_COLOR_BK_FOCUS,FxGetMenuBgColor());
	m_testAnsiPatternButton.OffsetColor(CButtonST::BTNST_COLOR_BK_IN, 30);
	m_testAnsiPatternButton.OffsetColor(CButtonST::BTNST_COLOR_FG_IN, 30);

	m_refs.SetIcon(HCFR_LoadPngHIcon(_T("toolbar"),_T("references"),(fxUseCustomColor!=FALSE),HCFR_ScaleIconPx(24,GetSafeHwnd()),HCFR_ScaleIconPx(24,GetSafeHwnd())),(HICON)NULL);
	m_grayScaleDeleteButton.SetIcon(HCFR_LoadPngHIcon(_T("toolbar"),_T("delete"),(fxUseCustomColor!=FALSE),HCFR_ScaleIconPx(24,GetSafeHwnd()),HCFR_ScaleIconPx(24,GetSafeHwnd())),(HICON)NULL);
	m_refs.SetFont(GetFont());
	m_refs.EnableBalloonTooltip();
	m_refs.SetTooltipText("Open references menu");
	m_refs.DrawFlatFocus(FALSE);
	m_refs.SetColor(CButtonST::BTNST_COLOR_FG_IN,FxGetSysColor(COLOR_MENUTEXT));
	m_refs.SetColor(CButtonST::BTNST_COLOR_FG_OUT,FxGetSysColor(COLOR_MENUTEXT));
	m_refs.SetColor(CButtonST::BTNST_COLOR_FG_FOCUS,FxGetSysColor(COLOR_MENUTEXT));
	m_refs.SetColor(CButtonST::BTNST_COLOR_BK_IN,FxGetMenuBgColor());
	m_refs.SetColor(CButtonST::BTNST_COLOR_BK_OUT,FxGetMenuBgColor());
	m_refs.SetColor(CButtonST::BTNST_COLOR_BK_FOCUS,FxGetMenuBgColor());
	m_refs.OffsetColor(CButtonST::BTNST_COLOR_BK_IN, 30);
	m_refs.OffsetColor(CButtonST::BTNST_COLOR_FG_IN, 30);

	// "Normal button" look: rounded corners + left-aligned labels on the action buttons.
	m_grayScaleButton.SetRoundedNormal(TRUE);
	m_grayScaleDeleteButton.SetRoundedNormal(TRUE);
	m_refs.SetRoundedNormal(TRUE);
	m_testAnsiPatternButton.SetRoundedNormal(TRUE);
	COLORREF crPanel = FxGetMenuBgColor();   // corner-fill = app bg (buttons float)
	m_grayScaleButton.SetRoundedBg(crPanel);
	m_grayScaleDeleteButton.SetRoundedBg(crPanel);
	m_refs.SetRoundedBg(crPanel);
	m_testAnsiPatternButton.SetRoundedBg(crPanel);
	COLORREF crFace = ButtonFaceColor();
	COLORREF crHover = ButtonHoverColor();
	COLORREF crBdr  = ButtonBorderColor();
	m_grayScaleButton.SetColor(CButtonST::BTNST_COLOR_BK_OUT, crFace);
	m_grayScaleButton.SetColor(CButtonST::BTNST_COLOR_BK_IN, crHover);
	m_grayScaleButton.SetColor(CButtonST::BTNST_COLOR_BK_FOCUS, crFace);
	m_grayScaleDeleteButton.SetColor(CButtonST::BTNST_COLOR_BK_OUT, crFace);
	m_grayScaleDeleteButton.SetColor(CButtonST::BTNST_COLOR_BK_IN, crHover);
	m_grayScaleDeleteButton.SetColor(CButtonST::BTNST_COLOR_BK_FOCUS, crFace);
	m_grayScaleDeleteButton.SetRoundedBorder(crBdr);
	m_refs.SetColor(CButtonST::BTNST_COLOR_BK_OUT, crFace);
	m_refs.SetColor(CButtonST::BTNST_COLOR_BK_IN, crHover);
	m_refs.SetColor(CButtonST::BTNST_COLOR_BK_FOCUS, crFace);
	m_refs.SetRoundedBorder(crBdr);
	m_testAnsiPatternButton.SetColor(CButtonST::BTNST_COLOR_BK_OUT, crFace);
	m_testAnsiPatternButton.SetColor(CButtonST::BTNST_COLOR_BK_IN, crHover);
	m_testAnsiPatternButton.SetColor(CButtonST::BTNST_COLOR_BK_FOCUS, crFace);
	m_testAnsiPatternButton.SetRoundedBorder(crBdr);
	// (m_satAllLevelsButton is created + styled below, after the param dropdowns.)
	if (!GetConfig()->m_darkTheme && ::IsWindow(m_editCheckButton.GetSafeHwnd())) FxApplyFlatCheck(m_editCheckButton.GetSafeHwnd());
	// Header [+]/[-] size buttons replace the up-down spinner. Owner-drawn so the
	// Segoe Fluent Icons glyphs render in this MBCS build; behaviour mirrors the spinner.
	if (GetDlgItem(IDC_SPIN_VIEW)) GetDlgItem(IDC_SPIN_VIEW)->ShowWindow(SW_HIDE);
	{
		m_fluentFont.DeleteObject();
		LOGFONT lf; ZeroMemory(&lf, sizeof(lf));
		lf.lfHeight = -GetConfig()->Scale(12);
		lf.lfWeight = FW_NORMAL;
		lf.lfCharSet = DEFAULT_CHARSET;
		lstrcpyn(lf.lfFaceName, _T("Segoe Fluent Icons"), LF_FACESIZE);
		m_fluentFont.CreateFontIndirect(&lf);
	}
	// (the Edit checkbox is reparented into the stats bar by SetHeaderModel below)
	// (The Go button's border is tinted green/red by SetMeasureButtonForMode/Stop.)


	// (The Display dropdown keeps its own group-box frame; the buttons get their
	// own rounded panel painted in OnEraseBkgnd.)

	// Sensor-name label: ellipsize long meter names (e.g. Xrite i1 Display Pro).
	if (GetDlgItem(IDC_SENSORNAME_STATIC))
		GetDlgItem(IDC_SENSORNAME_STATIC)->ModifyStyle(SS_TYPEMASK, SS_LEFT | SS_ENDELLIPSIS | SS_NOPREFIX, SWP_FRAMECHANGED);

	// Replace the 5 display-type radios with a dropdown (Sensor RGB is always disabled,
	// so it is omitted). Selection drives m_displayType via OnSelchangeDisplayType.
	if (m_comboDisplayType.GetSafeHwnd() == NULL)
	{
		CRect rcDisp; GetDlgItem(IDC_DISPLAY_GROUP)->GetWindowRect(&rcDisp); ScreenToClient(&rcDisp);
		CRect rcCombo(rcDisp.left + GetConfig()->Scale(4), rcDisp.top + GetConfig()->Scale(14), rcDisp.right - GetConfig()->Scale(4), rcDisp.top + GetConfig()->Scale(14) + GetConfig()->Scale(120));
		m_comboDisplayType.Create(WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST, rcCombo, this, IDC_DISPLAYTYPE_COMBO);
		m_comboDisplayType.SetFont(GetFont());
		int iXYZ = m_comboDisplayType.AddString(_T("XYZ")); m_comboDisplayType.SetItemData(iXYZ, HCFR_XYZ_VIEW);
		int iRGB = m_comboDisplayType.AddString(_T("RGB")); m_comboDisplayType.SetItemData(iRGB, HCFR_RGB_VIEW);
		int ixyz = m_comboDisplayType.AddString(_T("xyz")); m_comboDisplayType.SetItemData(ixyz, HCFR_xyz2_VIEW);
		int ixyY = m_comboDisplayType.AddString(_T("xyY")); m_comboDisplayType.SetItemData(ixyY, HCFR_xyY_VIEW);
		for (int ci = 0; ci < m_comboDisplayType.GetCount(); ci++)
			if ((int)m_comboDisplayType.GetItemData(ci) == m_displayType) { m_comboDisplayType.SetCurSel(ci); break; }
		if (m_comboDisplayType.GetCurSel() < 0) m_comboDisplayType.SetCurSel(0);
		m_comboDisplayType.ModifyStyleEx(WS_EX_DLGMODALFRAME, 0, SWP_FRAMECHANGED);

		int radios[] = { IDC_SENSORRGB_RADIO, IDC_RGB_RADIO, IDC_XYZ_RADIO, IDC_XYZ_RADIO2, IDC_XYY_RADIO };
		for (int ri = 0; ri < 5; ri++) if (GetDlgItem(radios[ri])) GetDlgItem(radios[ri])->ShowWindow(SW_HIDE);

		SCtrlInitPos* pCombo = new SCtrlInitPos;
		pCombo->m_hWnd = m_comboDisplayType.GetSafeHwnd();
		::GetWindowRect(pCombo->m_hWnd, &pCombo->m_Rect);
		::ScreenToClient(m_hWnd, (LPPOINT)&pCombo->m_Rect.left);
		::ScreenToClient(m_hWnd, (LPPOINT)&pCombo->m_Rect.right);
		pCombo->m_pLayout = &g_DisplayComboLayout;
		m_CtrlInitPos.AddTail(pCombo);
	}

	// dE filter segments for the 3D viewer, placed right of the info-pane
	// dropdown; hidden unless the 3D viewer occupies the pane.
	if (m_3dDEFilter.GetSafeHwnd() == NULL && GetDlgItem(IDC_INFO_DISPLAY))
	{
		CRect rcInfo; GetDlgItem(IDC_INFO_DISPLAY)->GetWindowRect(&rcInfo); ScreenToClient(&rcInfo);
		CRect rcSeg(rcInfo.right + GetConfig()->Scale(8), rcInfo.top,
					rcInfo.right + GetConfig()->Scale(8) + GetConfig()->Scale(330), rcInfo.bottom);
		m_3dDEFilter.Create(rcSeg, this, IDC_3DVIEW_DE_FILTER);
		m_3dDEFilter.SetSel(GetConfig()->GetProfileInt("MainView", "ThreeD dE Filter", 0));

		SCtrlInitPos* pSeg = new SCtrlInitPos;
		pSeg->m_hWnd = m_3dDEFilter.GetSafeHwnd();
		::GetWindowRect(pSeg->m_hWnd, &pSeg->m_Rect);
		::ScreenToClient(m_hWnd, (LPPOINT)&pSeg->m_Rect.left);
		::ScreenToClient(m_hWnd, (LPPOINT)&pSeg->m_Rect.right);
		pSeg->m_pLayout = &g_3DDEFilterLayout;
		m_CtrlInitPos.AddTail(pSeg);
	}

	// Display-profile pane: occupies the measures-grid rectangle, shown only in
	// mode 13 (grid hidden). Same anchoring as the grid so both resize together.
	if (m_profilePane.GetSafeHwnd() == NULL && m_pGrayScaleGrid && m_pGrayScaleGrid->GetSafeHwnd())
	{
		CRect rcGrid;
		m_pGrayScaleGrid->GetWindowRect(&rcGrid);
		ScreenToClient(&rcGrid);
		m_profilePane.Create(rcGrid, this, IDC_PROFILE_PANE);
		m_profilePane.SetDocument(GetDocument());

		SCtrlInitPos* pPane = new SCtrlInitPos;
		pPane->m_hWnd = m_profilePane.GetSafeHwnd();
		::GetWindowRect(pPane->m_hWnd, &pPane->m_Rect);
		::ScreenToClient(m_hWnd, (LPPOINT)&pPane->m_Rect.left);
		::ScreenToClient(m_hWnd, (LPPOINT)&pPane->m_Rect.right);
		pPane->m_pLayout = &g_ProfilePaneLayout;
		m_CtrlInitPos.AddTail(pPane);
	}

	// Per-mode pattern-parameter dropdowns (steps / stimulus level), positioned by
	// LayoutTopRow under the mode combo; UpdateParamCombos fills and shows them.
	if (m_comboSteps.GetSafeHwnd() == NULL)
	{
		CRect rcInit(0, 0, GetConfig()->Scale(80), GetConfig()->Scale(140));
		m_comboSteps.Create(WS_CHILD|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST, rcInit, this, IDC_PARAMSTEPS_COMBO);
		m_comboSteps.SetFont(GetFont());
		m_comboSteps.ModifyStyleEx(WS_EX_DLGMODALFRAME, 0, SWP_FRAMECHANGED);
		m_comboStimLevel.Create(WS_CHILD|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST, rcInit, this, IDC_STIMLEVEL_COMBO);
		m_comboStimLevel.SetFont(GetFont());
		m_comboStimLevel.ModifyStyleEx(WS_EX_DLGMODALFRAME, 0, SWP_FRAMECHANGED);

		// Caption labels drawn under each dropdown (font set with m_sectionFont below).
		CRect rcLbl(0, 0, GetConfig()->Scale(80), GetConfig()->Scale(14));
		m_lblMode.Create (_T(""), WS_CHILD|SS_CENTER|SS_CENTERIMAGE|SS_NOPREFIX, rcLbl, this, IDC_MODE_LABEL);
		m_lblSteps.Create(_T(""), WS_CHILD|SS_CENTER|SS_CENTERIMAGE|SS_NOPREFIX, rcLbl, this, IDC_PARAMSTEPS_LABEL);
		m_lblStim.Create (_T(""), WS_CHILD|SS_CENTER|SS_CENTERIMAGE|SS_NOPREFIX, rcLbl, this, IDC_STIMLEVEL_LABEL);

		int nParamIds[5] = { IDC_PARAMSTEPS_COMBO, IDC_STIMLEVEL_COMBO, IDC_MODE_LABEL, IDC_PARAMSTEPS_LABEL, IDC_STIMLEVEL_LABEL };
		for (int pi = 0; pi < 5; pi++)
		{
			SCtrlInitPos* pPos = new SCtrlInitPos;
			pPos->m_hWnd = ::GetDlgItem(m_hWnd, nParamIds[pi]);
			::GetWindowRect(pPos->m_hWnd, &pPos->m_Rect);
			::ScreenToClient(m_hWnd, (LPPOINT)&pPos->m_Rect.left);
			::ScreenToClient(m_hWnd, (LPPOINT)&pPos->m_Rect.right);
			pPos->m_pLayout = &g_ParamComboLayout;
			m_CtrlInitPos.AddTail(pPos);
		}
	}

	// "All stim" action button (runtime-created, shown only in saturation modes):
	// measures the current hue's sweep at every configured stimulus level. Styled
	// every InitButtons (theme-responsive) to exactly match the other action buttons.
	if (m_satAllLevelsButton.GetSafeHwnd() == NULL)
	{
		CRect rcBtn(0, 0, GetConfig()->Scale(80), GetConfig()->Scale(25));
		CString sBtn; sBtn.LoadString ( IDS_ALLSTIM_BTN );
		m_satAllLevelsButton.Create(sBtn, WS_CHILD|WS_TABSTOP, rcBtn, this, IDC_MEASURESATALLLEVELS_BUTTON);
		SCtrlInitPos* pBtn = new SCtrlInitPos;
		pBtn->m_hWnd = m_satAllLevelsButton.GetSafeHwnd();
		::GetWindowRect(pBtn->m_hWnd, &pBtn->m_Rect);
		::ScreenToClient(m_hWnd, (LPPOINT)&pBtn->m_Rect.left);
		::ScreenToClient(m_hWnd, (LPPOINT)&pBtn->m_Rect.right);
		pBtn->m_pLayout = &g_ActionBtnLayout;
		m_CtrlInitPos.AddTail(pBtn);
	}
	if (m_satAllLevelsButton.GetSafeHwnd())
	{
		m_satAllLevelsButton.SetIcon(HCFR_LoadPngHIcon(_T("toolbar"),_T("measure-all-sat-stim"),(fxUseCustomColor!=FALSE),HCFR_ScaleIconPx(24,GetSafeHwnd()),HCFR_ScaleIconPx(24,GetSafeHwnd())),(HICON)NULL);
		m_satAllLevelsButton.SetFont(GetFont());
		m_satAllLevelsButton.EnableBalloonTooltip();
		CString sTip; sTip.LoadString ( IDS_ALLSTIM_TIP );
		m_satAllLevelsButton.SetTooltipText(sTip);
		m_satAllLevelsButton.DrawFlatFocus(FALSE);
		m_satAllLevelsButton.SetColor(CButtonST::BTNST_COLOR_FG_IN,FxGetSysColor(COLOR_MENUTEXT));
		m_satAllLevelsButton.SetColor(CButtonST::BTNST_COLOR_FG_OUT,FxGetSysColor(COLOR_MENUTEXT));
		m_satAllLevelsButton.SetColor(CButtonST::BTNST_COLOR_FG_FOCUS,FxGetSysColor(COLOR_MENUTEXT));
		m_satAllLevelsButton.SetColor(CButtonST::BTNST_COLOR_BK_IN,FxGetMenuBgColor());
		m_satAllLevelsButton.SetColor(CButtonST::BTNST_COLOR_BK_OUT,FxGetMenuBgColor());
		m_satAllLevelsButton.SetColor(CButtonST::BTNST_COLOR_BK_FOCUS,FxGetMenuBgColor());
		m_satAllLevelsButton.OffsetColor(CButtonST::BTNST_COLOR_BK_IN, 30);
		m_satAllLevelsButton.OffsetColor(CButtonST::BTNST_COLOR_FG_IN, 30);
		m_satAllLevelsButton.SetRoundedNormal(TRUE);
		m_satAllLevelsButton.SetRoundedBg(FxGetMenuBgColor());
		m_satAllLevelsButton.SetColor(CButtonST::BTNST_COLOR_BK_OUT, ButtonFaceColor());
		m_satAllLevelsButton.SetColor(CButtonST::BTNST_COLOR_BK_IN, ButtonHoverColor());
		m_satAllLevelsButton.SetColor(CButtonST::BTNST_COLOR_BK_FOCUS, ButtonFaceColor());
		m_satAllLevelsButton.SetRoundedBorder(ButtonBorderColor());
	}

	if ( m_displayMode == 12 )
	{
		m_testAnsiPatternButton.ShowWindow ( SW_SHOW );
		m_refs.ShowWindow ( SW_HIDE );
	}
	else
	{
		m_testAnsiPatternButton.ShowWindow ( SW_HIDE );
		m_refs.ShowWindow ( m_displayMode == 13 ? SW_HIDE : SW_SHOW );
	}
	m_satAllLevelsButton.ShowWindow ( ( m_displayMode >= 5 && m_displayMode <= 10 ) ? SW_SHOW : SW_HIDE );
	line_Font.DeleteObject();
	line_Font.CreateFontA(GetConfig()->ScaleFloor(14,17),0,0,0,FW_SEMIBOLD,0,0,0,0,0,0,PROOF_QUALITY,VARIABLE_PITCH,_T("ARIAL"));
	m_refInfo.SetFont(&line_Font);

	// Crisp ClearType Segoe UI for the panel section labels (native GDI statics
	// otherwise render in the default non-ClearType dialog font).
	m_sectionFont.DeleteObject();
	m_sectionFont.CreateFontA(-GetConfig()->Scale(12),0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,VARIABLE_PITCH|FF_SWISS,_T("Segoe UI"));
	if (GetDlgItem(IDC_STATIC_RGBLEVELS)) GetDlgItem(IDC_STATIC_RGBLEVELS)->SetFont(&m_sectionFont);
	if (GetDlgItem(IDC_STATIC_DATA))      GetDlgItem(IDC_STATIC_DATA)->SetFont(&m_sectionFont);
	if (GetDlgItem(IDC_STATIC_TARGET))    GetDlgItem(IDC_STATIC_TARGET)->SetFont(&m_sectionFont);
	m_captionFont.DeleteObject();
	m_captionFont.CreateFontA(-GetConfig()->Scale(11),0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,VARIABLE_PITCH|FF_SWISS,_T("Segoe UI"));
	if (m_lblMode.GetSafeHwnd())  m_lblMode.SetFont(&m_captionFont);
	if (m_lblSteps.GetSafeHwnd()) m_lblSteps.SetFont(&m_captionFont);
	if (m_lblStim.GetSafeHwnd())  m_lblStim.SetFont(&m_captionFont);

    GetDlgItem( IDC_INFOLINE )->SetFont( &line_Font );

	if(GetDlgItem(IDC_MEASUREGRAYSCALE_BUTTON)) GetDlgItem(IDC_MEASUREGRAYSCALE_BUTTON)->ModifyStyleEx(WS_EX_DLGMODALFRAME,0,SWP_FRAMECHANGED);
	if(GetDlgItem(IDC_DELETEGRAYSCALE_BUTTON)) GetDlgItem(IDC_DELETEGRAYSCALE_BUTTON)->ModifyStyleEx(WS_EX_DLGMODALFRAME,0,SWP_FRAMECHANGED);
	if(GetDlgItem(IDC_REFS_BUTTON)) GetDlgItem(IDC_REFS_BUTTON)->ModifyStyleEx(WS_EX_DLGMODALFRAME,0,SWP_FRAMECHANGED);

	if (m_tooltip.GetSafeHwnd() == NULL)
	{
	m_tooltip.Create(this);	
	m_tooltip.SetBehaviour(PPTOOLTIP_CLOSE_LEAVEWND);
	m_tooltip.SetNotify(TRUE);
	m_tooltip.SetBorder(::CreateSolidBrush(RGB(96,96,96)),1,1);
	GetDlgItem( IDC_INFOLINE )->SetWindowTextA(m_infoLine);

	CWnd * pWnd = GetDlgItem(IDC_INFOLINE);
	pWnd = GetDlgItem(IDC_CCOMP3);

	m_tooltip.AddTool(pWnd, m_infoLine);
	m_tooltip.SetColorBk(RGB(238,238,238),RGB(238,238,238));
	m_tooltip.SetEffectBk(CPPDrawManager::EFFECT_SOLID);
	m_tooltip.SetBorder(::CreateSolidBrush(RGB(96,96,96)),1,1);

	m_tooltip.SetFont(&line_Font);
	}
}
void CMainView::InitGroups()
{

	CString groupFontName="Verdana";
	int groupFontSize=8;

	m_grayScaleGroup.SetXPGroupStyle(CXPGroupBox::XPGB_WINDOW);
	m_grayScaleGroup.SetFontName(groupFontName);
	m_grayScaleGroup.SetFontSize(groupFontSize - 1);
	m_grayScaleGroup.SetFontBold(TRUE);
	m_grayScaleGroup.SetAlignment(SS_LEFT);
//	m_grayScaleGroup.SetFontItalic(TRUE);
	m_grayScaleGroup.SetCaptionTextColor(LightenColor(70,FxGetSysColor(COLOR_MENUTEXT)));
	
	m_sensorGroup.SetXPGroupStyle(CXPGroupBox::XPGB_WINDOW);
	m_sensorGroup.SetFontName(groupFontName);
	m_sensorGroup.SetFontSize(groupFontSize);
	m_sensorGroup.SetFontBold(TRUE);
//	m_sensorGroup.SetFontItalic(TRUE);
	m_sensorGroup.SetCaptionTextColor(LightenColor(70,FxGetSysColor(COLOR_MENUTEXT)));
	
	m_generatorGroup.SetXPGroupStyle(CXPGroupBox::XPGB_WINDOW);
	m_generatorGroup.SetFontName(groupFontName);
	m_generatorGroup.SetFontSize(groupFontSize);
	m_generatorGroup.SetFontBold(TRUE);
//	m_generatorGroup.SetFontItalic(TRUE);
	m_generatorGroup.SetCaptionTextColor(LightenColor(70,FxGetSysColor(COLOR_MENUTEXT)));

	m_datarefGroup.SetXPGroupStyle(CXPGroupBox::XPGB_WINDOW);
	m_datarefGroup.SetFontName(groupFontName);
	m_datarefGroup.SetFontSize(groupFontSize);
	m_datarefGroup.SetFontBold(TRUE);
//	m_datarefGroup.SetFontItalic(TRUE);
	m_datarefGroup.SetCaptionTextColor(LightenColor(70,FxGetSysColor(COLOR_MENUTEXT)));

	m_displayGroup.SetXPGroupStyle(CXPGroupBox::XPGB_WINDOW);
	m_displayGroup.SetFontName(groupFontName);
	m_displayGroup.SetFontSize(groupFontSize);
	m_displayGroup.SetFontBold(TRUE);
//	m_displayGroup.SetFontItalic(TRUE);
	m_displayGroup.SetCaptionTextColor(LightenColor(70,FxGetSysColor(COLOR_MENUTEXT)));

	m_paramGroup.SetXPGroupStyle(CXPGroupBox::XPGB_WINDOW);
	m_paramGroup.SetFontName(groupFontName);
	m_paramGroup.SetFontSize(groupFontSize);
	m_paramGroup.SetFontBold(TRUE);
//	m_paramGroup.SetFontItalic(TRUE);
	m_paramGroup.SetCaptionTextColor(LightenColor(70,FxGetSysColor(COLOR_MENUTEXT)));

	m_selectGroup.SetXPGroupStyle(CXPGroupBox::XPGB_WINDOW);
	m_selectGroup.SetFontName(groupFontName);
	m_selectGroup.SetFontSize(groupFontSize);
	m_selectGroup.SetFontBold(TRUE);
//	m_selectGroup.SetFontItalic(TRUE);
	m_selectGroup.SetCaptionTextColor(LightenColor(70,FxGetSysColor(COLOR_MENUTEXT)));

	m_viewGroup.SetXPGroupStyle(CXPGroupBox::XPGB_WINDOW);
	m_viewGroup.SetFontName(groupFontName);
	m_viewGroup.SetFontSize(groupFontSize);
	m_viewGroup.SetFontBold(TRUE);
//	m_viewGroup.SetFontItalic(TRUE);
	m_viewGroup.SetCaptionTextColor(LightenColor(70,FxGetSysColor(COLOR_MENUTEXT)));
	
	CString dName,tName=GetDocument()->GetGenerator()->GetName(),gName;
	gName.LoadString(IDS_GDIGENERATOR_NAME);
	int d = GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE);
	if ( tName == gName )
	{
		switch (d)
		{
		case 0:
			dName = "Fullscreen";
			break;
		case 3:
			dName = "Overlay";
			break;
		case 2:
			dName = "madVR";
			break;
		case 4:
			dName = "Google Cast";
			break;
		case 5:
			dName = "Window";
			break;
			case 6:
			dName = "PGenerator";
			break;
		}
		m_generatorName.SetString(dName);
	} else
		m_generatorName.LoadString(IDS_MANUALDVDGENERATOR_NAME);
}	

// Solid panel colour behind the Display dropdown + action buttons: a few shades
// lighter than the app (menu) background so the group reads as a raised pane and
// the rounded buttons' corners blend into it.
// Action-button palette. Light theme uses the design values; dark theme uses muted equivalents.
static COLORREF ButtonFaceColor()  { return (fxUseCustomColor != FALSE) ? RGB(60,60,62) : RGB(255,255,255); }  // face FFFFFF
static COLORREF ButtonHoverColor() { return (fxUseCustomColor != FALSE) ? RGB(82,82,85) : RGB(242,242,242); }  // hover F2F2F2
static COLORREF ButtonBorderColor(){ return (fxUseCustomColor != FALSE) ? RGB(95,95,98) : RGB(210, 210, 210); }  // D2D2D2 (light) / subtle dark, matches the +/- buttons

BOOL CMainView::OnEraseBkgnd(CDC* pDC) 
{
	CRect windowRect;
	GetClientRect(windowRect );
	if (m_pGrayScaleGrid && m_pGrayScaleGrid->GetSafeHwnd() && (m_pGrayScaleGrid->GetStyle() & WS_VISIBLE))
	{
		CRect _gr; m_pGrayScaleGrid->GetWindowRect(&_gr); ScreenToClient(&_gr); pDC->ExcludeClipRect(&_gr);
	}
	if (m_pSelectedColorGrid && m_pSelectedColorGrid->GetSafeHwnd() && (m_pSelectedColorGrid->GetStyle() & WS_VISIBLE))
	{
		CRect _sr; m_pSelectedColorGrid->GetWindowRect(&_sr); ScreenToClient(&_sr); pDC->ExcludeClipRect(&_sr);
	}

	COLORREF colorTop,colorBottom;
	FxGetMenuBgColors(colorTop,colorBottom);
	pDC->FillSolidRect(windowRect, colorTop);
	{
		// Rounded container behind the action buttons, matching the other panels
		// (Display etc.): same menu-bg fill + border, 6px radius, 3px gap to the buttons.
		// Only VISIBLE buttons count -- Refs/ANSI share a slot, and "All stim" appears
		// in saturation modes -- so the panel wraps exactly the buttons on screen.
		CWnd* bp[5] = { GetDlgItem(IDC_MEASUREGRAYSCALE_BUTTON), GetDlgItem(IDC_MEASURESATALLLEVELS_BUTTON),
		                GetDlgItem(IDC_DELETEGRAYSCALE_BUTTON), GetDlgItem(IDC_REFS_BUTTON),
		                GetDlgItem(IDC_ANSICONTRAST_PATTERN_TEST_BUTTON) };
		CRect panel(0,0,0,0); BOOL gotp = FALSE;
		for (int bpi = 0; bpi < 5; bpi++)
		{
			if (!bp[bpi] || !::IsWindow(bp[bpi]->GetSafeHwnd())) continue;
			if (!(bp[bpi]->GetStyle() & WS_VISIBLE)) continue;
			CRect rc; bp[bpi]->GetWindowRect(&rc); ScreenToClient(&rc);
			if (!gotp) { panel = rc; gotp = TRUE; } else panel |= rc;
		}
		if (gotp)
		{
			panel.InflateRect(GetConfig()->Scale(3), GetConfig()->Scale(3));
			m_rcButtonPanel = panel;
			int rad = GetConfig()->Scale(6);
			COLORREF crCbdr = (fxUseCustomColor != FALSE) ? RGB(64,64,70) : FxGetSysColor(COLOR_3DSHADOW);
			CBrush brP(FxGetMenuBgColor());
			CPen penP(PS_SOLID, 1, crCbdr);
			CBrush* pOldB = pDC->SelectObject(&brP);
			CPen* pOldP = pDC->SelectObject(&penP);
			pDC->RoundRect(panel.left, panel.top, panel.right, panel.bottom, rad*2, rad*2);
			pDC->SelectObject(pOldB);
			pDC->SelectObject(pOldP);
		}
	}
	return true;
}

void CMainView::OnSysColorChange() 
{
	CFormView::OnSysColorChange();
	SetRedraw(FALSE);
	delete m_pBgBrush;
	m_pBgBrush= new CBrush(FxGetMenuBgColor());
	InitButtons();
	InitGroups();
	m_nSelColorGridReadingType = -1;
	InitSelectedColorGrid();
	InitGrid(true);
	if(m_pGrayScaleGrid) UpdateGrid();
	RefreshSelection();
	SetRedraw(TRUE);
	if(m_pGrayScaleGrid) m_pGrayScaleGrid->Invalidate(FALSE);
	if(m_pSelectedColorGrid) m_pSelectedColorGrid->Invalidate(FALSE);
	Invalidate(FALSE);	
}

HBRUSH CMainView::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) 
{
	HBRUSH hbr=CFormView::OnCtlColor(pDC, pWnd, nCtlColor);

	switch(nCtlColor)
	{
		case CTLCOLOR_STATIC:
			pDC->SetBkMode(TRANSPARENT);
			pDC->SetTextColor(FxGetSysColor(COLOR_MENUTEXT));
			return *m_pBgBrush;
			break;
		case CTLCOLOR_EDIT:
			// An edit control repaints incrementally as its text changes and only
			// erases its background on a full repaint. With TRANSPARENT the previous
			// glyphs stay on screen underneath the new ones - inserting a line leaves
			// a copy of the line it pushed down until something forces a redraw - so
			// its text must be drawn opaque, over the colour it is erased with.
			pDC->SetBkMode(OPAQUE);
			pDC->SetBkColor(FxGetMenuBgColor());
			pDC->SetTextColor(FxGetSysColor(COLOR_MENUTEXT));
			return *m_pBgBrush;
		case CTLCOLOR_BTN:
		default:
			pDC->SetBkMode(TRANSPARENT);
			pDC->SetTextColor(FxGetSysColor(COLOR_MENUTEXT));
			return *m_pBgBrush;
	}

	return hbr;
}

LRESULT CMainView::OnCtlColorStatic(WPARAM wParam, LPARAM lParam)
{
	// The colour-comparator swatches (IDC_CCOMP / IDC_CCOMP3) are owner-drawn
	// now (CCompSwatch::OnPaint), so every static gets the plain panel treatment.
	HDC hDC = (HDC)wParam;
	SetBkMode(hDC, TRANSPARENT);
	int nId = ::GetDlgCtrlID((HWND)lParam);
	if ( nId == IDC_MODE_LABEL || nId == IDC_PARAMSTEPS_LABEL || nId == IDC_STIMLEVEL_LABEL )
	{
		// Captions: dimmer than data but still clearly legible (70% text / 30% gray).
		COLORREF t = FxGetSysColor(COLOR_MENUTEXT);
		COLORREF g = FxGetSysColor(COLOR_GRAYTEXT);
		SetTextColor(hDC, RGB( (GetRValue(t)*7+GetRValue(g)*3)/10,
		                       (GetGValue(t)*7+GetGValue(g)*3)/10,
		                       (GetBValue(t)*7+GetBValue(g)*3)/10 ));
	}
	else
		SetTextColor(hDC, FxGetSysColor(COLOR_MENUTEXT));
	return (LRESULT)(m_pBgBrush->GetSafeHandle());
}

void CMainView::InsetInfoWindows()
{
	CWnd * pInfoArr [ 13 ] = { m_pInfoWnd, m_pInfoWnd2, m_pInfoWnd3, m_pInfoWnd4, m_pInfoWnd5, m_pInfoWnd6, m_pInfoWnd7, m_pInfoWnd8, m_pInfoWnd9, m_pInfoWnd10, m_pInfoWnd11, m_pInfoWnd12, m_pInfoWnd13 };
	for ( int i = 0; i < 13; i++ )
	{
		if ( pInfoArr [ i ] != NULL && ::IsWindow ( pInfoArr [ i ] -> m_hWnd ) )
		{
			CRect r;
			pInfoArr [ i ] -> GetWindowRect ( & r );
			ScreenToClient ( & r );
			r.DeflateRect ( 2, 2, 2, 2 );
			pInfoArr [ i ] -> MoveWindow ( & r, FALSE );
		}
	}
}

void CMainView::OnSize(UINT nType, int cx, int cy) 
{
	RECT			Rect;
	RECT			ClientRect;
	HDWP			hwdp;
	SCtrlInitPos *	pCtrlPos;
	POSITION		pos;

	//CFormView::OnSize(nType, cx, cy);
	
	// TODO: Add your message handler code here
	if ( m_bPositionsInit )
	{
		GetClientRect ( & ClientRect );

		hwdp = BeginDeferWindowPos ( m_CtrlInitPos.GetCount () );
		
		pos = m_CtrlInitPos.GetHeadPosition ();
		while ( pos )
		{
			pCtrlPos = (SCtrlInitPos *) m_CtrlInitPos.GetNext ( pos );
			Rect = pCtrlPos -> m_Rect;

			switch ( pCtrlPos -> m_pLayout -> m_LeftMode )
			{
				case LAYOUT_LEFT:
					 // No move
					 break;

				case LAYOUT_RIGHT:
					 Rect.left += ClientRect.right - m_OriginalRect.right;
					 break;
			}

			switch ( pCtrlPos -> m_pLayout -> m_RightMode )
			{
				case LAYOUT_LEFT:
					 // No move
					 break;

				case LAYOUT_RIGHT:
					 Rect.right += ClientRect.right - m_OriginalRect.right;
					 break;
			}

			switch ( pCtrlPos -> m_pLayout -> m_TopMode )
			{
				case LAYOUT_TOP:
					 // No move
					 break;

				case LAYOUT_BOTTOM:
					 Rect.top += ClientRect.bottom - m_OriginalRect.bottom;
					 break;

				case LAYOUT_TOP_OFFSET:
					 Rect.top += m_nSizeOffset;
					 break;
			}

			switch ( pCtrlPos -> m_pLayout -> m_BottomMode )
			{
				case LAYOUT_TOP:
					 // No move
					 break;

				case LAYOUT_BOTTOM:
					 Rect.bottom += ClientRect.bottom - m_OriginalRect.bottom;
					 break;

				case LAYOUT_TOP_OFFSET:
					 Rect.bottom += m_nSizeOffset;
					 break;
			}
			
			hwdp = DeferWindowPos ( hwdp, pCtrlPos -> m_hWnd, NULL, Rect.left, Rect.top, Rect.right - Rect.left, Rect.bottom - Rect.top, SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW );
		}

		EndDeferWindowPos ( hwdp );

		pos = m_CtrlInitPos.GetHeadPosition ();
		while ( pos )
		{
			pCtrlPos = (SCtrlInitPos *) m_CtrlInitPos.GetNext ( pos );
			::InvalidateRect ( pCtrlPos -> m_hWnd, NULL, FALSE );
		}

		if (m_pGrayScaleGrid && m_pGrayScaleGrid->GetSafeHwnd() && (m_pGrayScaleGrid->GetStyle() & WS_VISIBLE))
			m_pGrayScaleGrid->ExpandToFit(FALSE);

		if ( m_pInfoWnd )
		{
			CWnd *	pWnd;

			pWnd = GetDlgItem ( IDC_STATIC_VIEW );
			pWnd -> GetWindowRect ( & Rect );
			ScreenToClient ( & Rect );
			if (m_infoDisplay >= 1)
			{
				if (m_infoDisplay == 9)
				{
					m_pInfoWnd -> SetWindowPos ( pWnd, Rect.left, Rect.top, (Rect.right - Rect.left) / 2, Rect.bottom - Rect.top , SWP_NOACTIVATE );
					m_pInfoWnd5 -> SetWindowPos ( pWnd, Rect.left + (Rect.right - Rect.left) / 2, Rect.top, (Rect.right - Rect.left) / 2, Rect.bottom - Rect.top, SWP_NOACTIVATE );
				}
				else if (m_infoDisplay == 10)
				{
					m_pInfoWnd -> SetWindowPos ( pWnd, Rect.left, Rect.top, (Rect.right - Rect.left) / 2, Rect.bottom - Rect.top , SWP_NOACTIVATE );
					m_pInfoWnd6 -> SetWindowPos ( pWnd, Rect.left + (Rect.right - Rect.left) / 2, Rect.top, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
				}
				else if (m_infoDisplay == 4)
				{
					m_pInfoWnd -> SetWindowPos ( pWnd, Rect.left, Rect.top, (Rect.right - Rect.left) / 2, Rect.bottom - Rect.top , SWP_NOACTIVATE );
					m_pInfoWnd7 -> SetWindowPos ( pWnd, Rect.left + (Rect.right - Rect.left) / 2, Rect.top, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
				}
				else if (m_infoDisplay == 1)
				{
					m_pInfoWnd -> SetWindowPos ( pWnd, Rect.left, Rect.top, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
					m_pInfoWnd8 -> SetWindowPos ( pWnd, Rect.left + (Rect.right - Rect.left) / 2, Rect.top, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
				}
				else if (m_infoDisplay == 3)
				{
					m_pInfoWnd -> SetWindowPos ( pWnd, Rect.left, Rect.top, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
					m_pInfoWnd9 -> SetWindowPos ( pWnd, Rect.left + (Rect.right - Rect.left) / 2, Rect.top, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
				}
				else if (m_infoDisplay == 11)
				{
					m_pInfoWnd -> SetWindowPos ( pWnd, Rect.left, Rect.top, (Rect.right - Rect.left) / 3, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
					m_pInfoWnd10 -> SetWindowPos ( pWnd, Rect.left + (Rect.right - Rect.left) / 3, Rect.top, (Rect.right - Rect.left) / 3, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
					m_pInfoWnd11 -> SetWindowPos ( pWnd, Rect.left + (Rect.right - Rect.left) / 3 * 2, Rect.top, (Rect.right - Rect.left) / 3, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
				}
				else if (m_infoDisplay == 12)
				{
					if (m_pInfoWnd13)
					{
						m_pInfoWnd -> SetWindowPos ( pWnd, Rect.left, Rect.top, (Rect.right - Rect.left) / 3., (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
						m_pInfoWnd12 -> SetWindowPos ( pWnd, Rect.left + (Rect.right - Rect.left) / 3., Rect.top, (Rect.right - Rect.left) / 3., (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
						m_pInfoWnd13 -> SetWindowPos ( pWnd, Rect.left + (Rect.right - Rect.left) / 3. * 2., Rect.top, (Rect.right - Rect.left) / 3., (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
					}
					else if (m_pInfoWnd12)
					{
						m_pInfoWnd -> SetWindowPos ( pWnd, Rect.left, Rect.top, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
						m_pInfoWnd12 -> SetWindowPos ( pWnd, Rect.left + (Rect.right - Rect.left) / 2, Rect.top, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
					}
					else if (m_pInfoWnd)
						m_pInfoWnd -> SetWindowPos ( pWnd, Rect.left, Rect.top, (Rect.right - Rect.left), (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
				}
				else
					m_pInfoWnd -> SetWindowPos ( pWnd, Rect.left, Rect.top, Rect.right - Rect.left, (Rect.bottom - Rect.top), SWP_NOACTIVATE );
			}
			else
			{
				m_pInfoWnd -> SetWindowPos ( pWnd, Rect.left, Rect.top, (Rect.right - Rect.left), (Rect.bottom - Rect.top), SWP_NOACTIVATE );
//				m_pInfoWnd2 -> SetWindowPos ( pWnd, Rect.left + (Rect.right - Rect.left) / 2, Rect.top, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) / 2, SWP_NOACTIVATE );
//				m_pInfoWnd3 -> SetWindowPos ( pWnd, Rect.left, Rect.top + (Rect.bottom - Rect.top) / 2, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) / 2, SWP_NOACTIVATE );
//				m_pInfoWnd4 -> SetWindowPos ( pWnd, Rect.left + (Rect.right - Rect.left) / 2, Rect.top + (Rect.bottom - Rect.top) / 2, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) / 2, SWP_NOACTIVATE );
			}

		}

		InsetInfoWindows ();

		// Gate the target on the room the TARGET itself needs, not the full panel
			// height (the grid/groupbox extend well below the target, so keying off
			// m_InitialWindowSize.y hid the target even when it would fit).
			int nTargetNeeded = m_InitialWindowSize.y;
			if ( m_Target.GetSafeHwnd () )
			{
				CRect tgtRect;
				m_Target.GetWindowRect ( & tgtRect );
				ScreenToClient ( & tgtRect );
				nTargetNeeded = tgtRect.bottom + 4;
			}
			if ( ClientRect.bottom - ClientRect.top < nTargetNeeded + m_nSizeOffset )
			{
				m_Target.ShowWindow ( SW_HIDE );
		}
		else
		{
			if ( ! m_Target.IsWindowVisible () )
				m_Target.ShowWindow ( SW_SHOW );
		}
		
		if ( m_bInSizeMove )
		{
			KillTimer ( SIZEMOVE_TIMER_ID );
			SetTimer ( SIZEMOVE_TIMER_ID, 120, NULL );
		}
		else
		{
			InitGrid(true);
			UpdateGrid();
		}

		if ( m_displayMode == 13 )
			LayoutProfilePane ();	// override the grid-anchored CtrlInitPos rect
	}
}

void CMainView::OnEnterSizeMove()
{
	CFormView::OnEnterSizeMove();
	m_bInSizeMove = TRUE;
}

void CMainView::OnExitSizeMove()
{
	CFormView::OnExitSizeMove();
	m_bInSizeMove = FALSE;
	KillTimer ( SIZEMOVE_TIMER_ID );
	if ( m_bPositionsInit )
	{
		InitGrid(true);
		UpdateGrid();
	}
}

void CMainView::OnTimer(UINT_PTR nIDEvent)
{
	if ( nIDEvent == SIZEMOVE_TIMER_ID )
	{
		KillTimer ( SIZEMOVE_TIMER_ID );
		if ( m_bPositionsInit )
		{
			InitGrid(true);
			UpdateGrid();
		}
		return;
	}
	CFormView::OnTimer(nIDEvent);
}

void CMainView::OnSelchangeInfoDisplay() 
{
	CWnd *					pWnd;
	CRect					Rect;
	CEdit *					pEdit;
	CTargetWnd *			pTargetWnd;
	CSpectrumWnd *			pSpectrumWnd;
	CCreateContext			context;
	CCIEChartView *			pCIEChartView;
	CLuminanceHistoView *	pLuminanceHistoView;
	CNearBlackHistoView *	pNearBlackHistoView;
	CNearWhiteHistoView *	pNearWhiteHistoView;
	CSatLumHistoView *		pSatLumHistoView;
	CSatLumShiftView *		pSatLumShiftView;
	CGammaHistoView	*		pGammaHistoView;
	CRGBHistoView *			pRGBHistoView;
	CColorTempHistoView *	pColorTempHistoView;
	CMeasuresHistoView *	pMeasuresHistoView;
	CSubFrame *				pFrame;

	if ( m_pInfoWnd )
	{
		m_pInfoWnd -> DestroyWindow ();
		if ( m_infoDisplay < 3 || m_infoDisplay == 11 )
			delete m_pInfoWnd;

		m_pInfoWnd = NULL;
	}

	if ( m_pInfoWnd2 ) 
	{
		m_pInfoWnd2->DestroyWindow();
		m_pInfoWnd2 = NULL;
	}

	if ( m_pInfoWnd3 ) 
	{
		m_pInfoWnd3->DestroyWindow();
		m_pInfoWnd3 = NULL;
	}

	if ( m_pInfoWnd4 ) 
	{
		m_pInfoWnd4->DestroyWindow();
		m_pInfoWnd4 = NULL;
	}

	if ( m_pInfoWnd5 ) 
	{
		m_pInfoWnd5->DestroyWindow();
		m_pInfoWnd5 = NULL;
	}

	if ( m_pInfoWnd6 ) 
	{
		m_pInfoWnd6->DestroyWindow();
		m_pInfoWnd6 = NULL;
	}

	if ( m_pInfoWnd7 ) 
	{
		m_pInfoWnd7->DestroyWindow();
		m_pInfoWnd7 = NULL;
	}

	if ( m_pInfoWnd8 ) 
	{
		m_pInfoWnd8->DestroyWindow();
		m_pInfoWnd8 = NULL;
	}

	if ( m_pInfoWnd9 ) 
	{
		m_pInfoWnd9->DestroyWindow();
		m_pInfoWnd9 = NULL;
	}

	if ( m_pInfoWnd10 ) 
	{
		m_pInfoWnd10->DestroyWindow();
		m_pInfoWnd10 = NULL;
	}

	if ( m_pInfoWnd11 ) 
	{
		m_pInfoWnd11->DestroyWindow();
		m_pInfoWnd11 = NULL;
	}

	if ( m_pInfoWnd12 ) 
	{
		m_pInfoWnd12->DestroyWindow();
		m_pInfoWnd12 = NULL;
	}

	if ( m_pInfoWnd13 ) 
	{
		m_pInfoWnd13->DestroyWindow();
		m_pInfoWnd13 = NULL;
	}

	// the dE filter segments only apply to the 3D viewer
	if ( m_3dDEFilter.GetSafeHwnd () )
		m_3dDEFilter.ShowWindow ( SW_HIDE );

	pWnd = GetDlgItem ( IDC_STATIC_VIEW );
	pWnd -> GetWindowRect ( & Rect );
	ScreenToClient ( & Rect );
    int size=GetDocument()->GetMeasure()->GetGrayScaleSize();

	if (!refresh)
		m_infoDisplay = m_comboDisplay.GetCurSel ( );

	if (m_bUpdate)
		GetConfig()->WriteProfileInt("MainView","Info Display",m_infoDisplay);
	
	switch ( m_infoDisplay )
	{
		case 0:
			 pEdit = new CEditEx;

			 pEdit -> Create (WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN, Rect, this, IDC_INFO_VIEW );
			 pEdit -> SetFont ( GetFont () );		 
			 pEdit -> SetWindowText ( GetDocument()->GetMeasure()->GetInfoString() );

			 // The initial fill reaches the control as WM_SETTEXT, which CEditEx
			 // records as an undoable change; without this the first Ctrl+Z would
			 // blank the summary instead of undoing what the user typed.
			 ( (CEditEx *) pEdit ) -> EmptyUndoBuffer ();

			 m_pInfoWnd = pEdit;
			 m_pInfoWnd -> SetWindowPos ( pWnd, Rect.left, Rect.top, (Rect.right - Rect.left), (Rect.bottom - Rect.top), SWP_NOACTIVATE );

/*			 pFrame = new CSubFrame;

			 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			 context.m_pCurrentDoc = GetDocument ();
			 context.m_pCurrentFrame = pFrame;
			 context.m_pLastView = this;
			 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			 context.m_pNewViewClass = RUNTIME_CLASS ( CCIEChartView );

			 pCIEChartView = (CCIEChartView *) context.m_pNewViewClass->CreateObject();
			 pCIEChartView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			 pCIEChartView -> OnInitialUpdate ();
			 pFrame -> SetActiveView ( pCIEChartView, FALSE );

			 m_pInfoWnd2 = pFrame;
			 m_pInfoWnd2 -> SetWindowPos ( pWnd, Rect.left + (Rect.right - Rect.left) / 2, Rect.top, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) / 2, SWP_NOACTIVATE );

			 pFrame = new CSubFrame;

			 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			 context.m_pCurrentDoc = GetDocument ();
			 context.m_pCurrentFrame = pFrame;
			 context.m_pLastView = this;
			 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			 context.m_pNewViewClass = RUNTIME_CLASS ( CLuminanceHistoView );

			 pLuminanceHistoView = (CLuminanceHistoView *) context.m_pNewViewClass->CreateObject();
			 pLuminanceHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			 pLuminanceHistoView -> OnInitialUpdate ();
			 pFrame -> SetActiveView ( pLuminanceHistoView, FALSE );

			 m_pInfoWnd3 = pFrame;
			 m_pInfoWnd3 -> SetWindowPos ( pWnd, Rect.left, Rect.top + (Rect.bottom - Rect.top) / 2, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) / 2, SWP_NOACTIVATE );

			 pFrame = new CSubFrame;

			 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			 context.m_pCurrentDoc = GetDocument ();
			 context.m_pCurrentFrame = pFrame;
			 context.m_pLastView = this;
			 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			 context.m_pNewViewClass = RUNTIME_CLASS ( CRGBHistoView );

			 pRGBHistoView = (CRGBHistoView *) context.m_pNewViewClass->CreateObject();
			 pRGBHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			 pRGBHistoView -> OnInitialUpdate ();
			 pFrame -> SetActiveView ( pRGBHistoView, FALSE );

			 m_pInfoWnd4 = pFrame;
			 m_pInfoWnd4 -> SetWindowPos ( pWnd, Rect.left + (Rect.right - Rect.left) / 2, Rect.top + (Rect.bottom - Rect.top) / 2, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) / 2, SWP_NOACTIVATE );
*/			 break;

		case 1: // target
             pTargetWnd = new CTargetWnd;

			 pTargetWnd -> Create (NULL, NULL, WS_VISIBLE | WS_CHILD, Rect, this, IDC_INFO_VIEW, NULL );
             if (m_displayMode <= 11 )// &&  m_displayMode != 2)
             {
		        if (m_displayMode == 3)
				    size = 101;
				else if (m_displayMode == 4)
				size = -1 * GetDocument()->GetMeasure()->GetNearWhiteScaleSize();

				if (m_displayMode > 4 && m_displayMode < 12)
					size=GetDocument()->GetMeasure()->GetSaturationSize();

				if (m_displayMode == 2)
				{
					pTargetWnd -> m_pRefColor = & m_SelectedColor;
					pTargetWnd -> Refresh (GetDocument()->GetGenerator()->m_b16_235, last_Col + 1, size, last_Display, GetDocument(), CTargetWnd::TARGET_ALL );
				}
				else
				{
					if (m_SelectedColor.isValid())
						pTargetWnd -> m_pRefColor = & m_SelectedColor;
					if (GetDocument())
						pTargetWnd -> Refresh (GetDocument()->GetGenerator()->m_b16_235,  (m_pGrayScaleGrid -> GetSelectedCellRange().IsValid()?m_pGrayScaleGrid -> GetSelectedCellRange().GetMinCol():-1), size, m_displayMode, GetDocument(), CTargetWnd::TARGET_ALL );
				}
             }
			 m_pInfoWnd = pTargetWnd;
			 m_pInfoWnd -> SetWindowPos ( pWnd, Rect.left, Rect.top, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );

			 pFrame = new CSubFrame;

			 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			 context.m_pCurrentDoc = GetDocument ();
			 context.m_pCurrentFrame = pFrame;
			 context.m_pLastView = this;
			 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			 context.m_pNewViewClass = RUNTIME_CLASS ( CCIEChartView );

			 pCIEChartView = (CCIEChartView *) context.m_pNewViewClass->CreateObject();
			 pCIEChartView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			 pCIEChartView -> OnInitialUpdate ();
			 pFrame -> SetActiveView ( pCIEChartView, FALSE );

			 m_pInfoWnd8 = pFrame;
			 m_pInfoWnd8 -> SetWindowPos ( pWnd, Rect.left + (Rect.right - Rect.left) / 2, Rect.top, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
			 break;

		case 2: // spectrum
			 pSpectrumWnd = new CSpectrumWnd;
			
			 pSpectrumWnd -> Create (NULL, NULL, WS_VISIBLE | WS_CHILD, Rect, this, IDC_INFO_VIEW, NULL );
			 pSpectrumWnd -> m_pRefColor = & m_SelectedColor;
			 pSpectrumWnd -> Refresh ();

			 m_pInfoWnd = pSpectrumWnd;
			 break;

		case 3: // luminance
			 pFrame = new CSubFrame;

			 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			 context.m_pCurrentDoc = GetDocument ();
			 context.m_pCurrentFrame = pFrame;
			 context.m_pLastView = this;
			 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			 context.m_pNewViewClass = RUNTIME_CLASS ( CLuminanceHistoView );

			 pLuminanceHistoView = (CLuminanceHistoView *) context.m_pNewViewClass->CreateObject();
			 pLuminanceHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			 pLuminanceHistoView -> OnInitialUpdate ();
			 pFrame -> SetActiveView ( pLuminanceHistoView, FALSE );
			 pFrame -> OnSize ( 0, 0, 0 );

			 m_pInfoWnd = pFrame;
			 m_pInfoWnd -> SetWindowPos ( pWnd, Rect.left, Rect.top, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );

			 pFrame = new CSubFrame;

			 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			 context.m_pCurrentDoc = GetDocument ();
			 context.m_pCurrentFrame = pFrame;
			 context.m_pLastView = this;
			 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			 context.m_pNewViewClass = RUNTIME_CLASS ( CGammaHistoView );

			 pGammaHistoView = (CGammaHistoView *) context.m_pNewViewClass->CreateObject();
			 pGammaHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			 pGammaHistoView -> OnInitialUpdate ();
			 pFrame -> SetActiveView ( pGammaHistoView, FALSE );

			 m_pInfoWnd9 = pFrame;
			 m_pInfoWnd9 -> SetWindowPos ( pWnd, Rect.left + (Rect.right - Rect.left) / 2, Rect.top, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
			 break;

		case 4: // gamma
			 pFrame = new CSubFrame;

			 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			 context.m_pCurrentDoc = GetDocument ();
			 context.m_pCurrentFrame = pFrame;
			 context.m_pLastView = this;
			 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			 context.m_pNewViewClass = RUNTIME_CLASS ( CRGBHistoView );

			 pRGBHistoView = (CRGBHistoView *) context.m_pNewViewClass->CreateObject();
			 pRGBHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			 pRGBHistoView -> OnInitialUpdate ();
			 pFrame -> SetActiveView ( pRGBHistoView, FALSE );

			 pFrame -> OnSize ( 0, 0, 0 );

			 m_pInfoWnd = pFrame;
			 m_pInfoWnd -> SetWindowPos ( pWnd, Rect.left, Rect.top, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );

			 pFrame = new CSubFrame;

			 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			 context.m_pCurrentDoc = GetDocument ();
			 context.m_pCurrentFrame = pFrame;
			 context.m_pLastView = this;
			 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			 context.m_pNewViewClass = RUNTIME_CLASS ( CGammaHistoView );

			 pGammaHistoView = (CGammaHistoView *) context.m_pNewViewClass->CreateObject();
			 pGammaHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			 pGammaHistoView -> OnInitialUpdate ();
			 pFrame -> SetActiveView ( pGammaHistoView, FALSE );

			 m_pInfoWnd7 = pFrame;
			 m_pInfoWnd7 -> SetWindowPos ( pWnd, Rect.left + (Rect.right - Rect.left) / 2, Rect.top, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
			 break;

		case 5: // RGB
			 pFrame = new CSubFrame;

			 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			 context.m_pCurrentDoc = GetDocument ();
			 context.m_pCurrentFrame = pFrame;
			 context.m_pLastView = this;
			 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			 context.m_pNewViewClass = RUNTIME_CLASS ( CRGBHistoView );

			 pRGBHistoView = (CRGBHistoView *) context.m_pNewViewClass->CreateObject();
			 pRGBHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			 pRGBHistoView -> OnInitialUpdate ();
			 pFrame -> SetActiveView ( pRGBHistoView, FALSE );
			 pFrame -> OnSize ( 0, 0, 0 );

			 m_pInfoWnd = pFrame;
			 break;

		case 6: // Color temperature
			 pFrame = new CSubFrame;

			 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			 context.m_pCurrentDoc = GetDocument ();
			 context.m_pCurrentFrame = pFrame;
			 context.m_pLastView = this;
			 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			 context.m_pNewViewClass = RUNTIME_CLASS ( CColorTempHistoView );

			 pColorTempHistoView = (CColorTempHistoView *) context.m_pNewViewClass->CreateObject();
			 pColorTempHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			 pColorTempHistoView -> OnInitialUpdate ();
			 pFrame -> SetActiveView ( pColorTempHistoView, FALSE );
			 pFrame -> OnSize ( 0, 0, 0 );

			 m_pInfoWnd = pFrame;
			 break;

		case 7: // CIE
			 pFrame = new CSubFrame;

			 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			 context.m_pCurrentDoc = GetDocument ();
			 context.m_pCurrentFrame = pFrame;
			 context.m_pLastView = this;
			 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			 context.m_pNewViewClass = RUNTIME_CLASS ( CCIEChartView );

			 pCIEChartView = (CCIEChartView *) context.m_pNewViewClass->CreateObject();
			 pCIEChartView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			 pCIEChartView -> OnInitialUpdate ();
			 pFrame -> SetActiveView ( pCIEChartView, FALSE );
			 pFrame -> OnSize ( 0, 0, 0 );

			 m_pInfoWnd = pFrame;
			 break;

		case 8: // Combined histogram for free measures
			 pFrame = new CSubFrame;

			 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			 context.m_pCurrentDoc = GetDocument ();
			 context.m_pCurrentFrame = pFrame;
			 context.m_pLastView = this;
			 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			 context.m_pNewViewClass = RUNTIME_CLASS ( CMeasuresHistoView );

			 pMeasuresHistoView = (CMeasuresHistoView *) context.m_pNewViewClass->CreateObject();
			 pMeasuresHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			 pMeasuresHistoView -> OnInitialUpdate ();
			 pFrame -> SetActiveView ( pMeasuresHistoView, FALSE );
			 pFrame -> OnSize ( 0, 0, 0 );

			 m_pInfoWnd = pFrame;
			 break;

		case 9: // Nearblack/nearwhite
			 pFrame = new CSubFrame;

			 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			 context.m_pCurrentDoc = GetDocument ();
			 context.m_pCurrentFrame = pFrame;
			 context.m_pLastView = this;
			 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			 context.m_pNewViewClass = RUNTIME_CLASS ( CNearBlackHistoView );

			 pNearBlackHistoView = (CNearBlackHistoView *) context.m_pNewViewClass->CreateObject();
			 pNearBlackHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			 pNearBlackHistoView -> OnInitialUpdate ();
			 pFrame -> SetActiveView ( pNearBlackHistoView, FALSE );
			 pFrame -> OnSize ( 0, 0, 0 );

			 m_pInfoWnd = pFrame;
			 m_pInfoWnd -> SetWindowPos ( pWnd, Rect.left, Rect.top, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
			 
			 pFrame = new CSubFrame;

			 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			 context.m_pCurrentDoc = GetDocument ();
			 context.m_pCurrentFrame = pFrame;
			 context.m_pLastView = this;
			 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			 context.m_pNewViewClass = RUNTIME_CLASS ( CNearWhiteHistoView );

			 pNearWhiteHistoView = (CNearWhiteHistoView *) context.m_pNewViewClass->CreateObject();
			 pNearWhiteHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			 pNearWhiteHistoView -> OnInitialUpdate ();
			 pFrame -> SetActiveView ( pNearWhiteHistoView, FALSE );

			 m_pInfoWnd5 = pFrame;
			 m_pInfoWnd5 -> SetWindowPos ( pWnd, Rect.left + (Rect.right - Rect.left) / 2, Rect.top, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
			 break;

		case 10: // saturations
			 pFrame = new CSubFrame;

			 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			 context.m_pCurrentDoc = GetDocument ();
			 context.m_pCurrentFrame = pFrame;
			 context.m_pLastView = this;
			 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			 context.m_pNewViewClass = RUNTIME_CLASS ( CSatLumHistoView );

			 pSatLumHistoView = (CSatLumHistoView *) context.m_pNewViewClass->CreateObject();
			 pSatLumHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			 pSatLumHistoView -> OnInitialUpdate ();
			 pFrame -> SetActiveView ( pSatLumHistoView, FALSE );
			 pFrame -> OnSize ( 0, 0, 0 );

			 m_pInfoWnd = pFrame;
			 m_pInfoWnd -> SetWindowPos ( pWnd, Rect.left, Rect.top, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
			 
			 pFrame = new CSubFrame;

			 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			 context.m_pCurrentDoc = GetDocument ();
			 context.m_pCurrentFrame = pFrame;
			 context.m_pLastView = this;
			 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			 context.m_pNewViewClass = RUNTIME_CLASS ( CSatLumShiftView );

			 pSatLumShiftView = (CSatLumShiftView *) context.m_pNewViewClass->CreateObject();
			 pSatLumShiftView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			 pSatLumShiftView -> OnInitialUpdate ();
			 pFrame -> SetActiveView ( pSatLumShiftView, FALSE );

			 m_pInfoWnd6 = pFrame;
			 m_pInfoWnd6 -> SetWindowPos ( pWnd, Rect.left + (Rect.right - Rect.left) / 2, Rect.top, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
			 break;

		case 11: //Target - RGB - gamma
			 pTargetWnd = new CTargetWnd;

			 pTargetWnd -> Create (NULL, NULL, WS_VISIBLE | WS_CHILD, Rect, this, IDC_INFO_VIEW, NULL );
             if (m_displayMode <= 11 )// &&  m_displayMode != 2)
             {
		        if (m_displayMode == 3)
				    size = 101;
				else if (m_displayMode == 4)
				size = -1 * GetDocument()->GetMeasure()->GetNearWhiteScaleSize();

				if (m_displayMode > 4 && m_displayMode < 12)
					size=GetDocument()->GetMeasure()->GetSaturationSize();

				if (m_displayMode == 2)
				{
					pTargetWnd -> m_pRefColor = & m_SelectedColor;
					pTargetWnd -> Refresh (GetDocument()->GetGenerator()->m_b16_235, last_Col + 1, size, last_Display, GetDocument(), CTargetWnd::TARGET_ALL );
				}
				else
				{
					if (m_SelectedColor.isValid())
						pTargetWnd -> m_pRefColor = & m_SelectedColor;
					if (GetDocument())
						pTargetWnd -> Refresh (GetDocument()->GetGenerator()->m_b16_235,  (m_pGrayScaleGrid -> GetSelectedCellRange().IsValid()?m_pGrayScaleGrid -> GetSelectedCellRange().GetMinCol():-1), size, m_displayMode, GetDocument(), CTargetWnd::TARGET_ALL );
				}
             }

			 m_pInfoWnd = pTargetWnd;
			 m_pInfoWnd -> SetWindowPos ( pWnd, Rect.left, Rect.top, (Rect.right - Rect.left) / 3, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );

			 pFrame = new CSubFrame;

			 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			 context.m_pCurrentDoc = GetDocument ();
			 context.m_pCurrentFrame = pFrame;
			 context.m_pLastView = this;
			 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			 context.m_pNewViewClass = RUNTIME_CLASS ( CRGBHistoView );

			 pRGBHistoView = (CRGBHistoView *) context.m_pNewViewClass->CreateObject();
			 pRGBHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			 pRGBHistoView -> OnInitialUpdate ();
			 pFrame -> SetActiveView ( pRGBHistoView, FALSE );

			 m_pInfoWnd10 = pFrame;
			 m_pInfoWnd10 -> SetWindowPos ( pWnd, Rect.left + (Rect.right - Rect.left) / 3, Rect.top, (Rect.right - Rect.left) / 3, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );

			 pFrame = new CSubFrame;

			 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			 context.m_pCurrentDoc = GetDocument ();
			 context.m_pCurrentFrame = pFrame;
			 context.m_pLastView = this;
			 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			 context.m_pNewViewClass = RUNTIME_CLASS ( CGammaHistoView );

			 pGammaHistoView = (CGammaHistoView *) context.m_pNewViewClass->CreateObject();
			 pGammaHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			 pGammaHistoView -> OnInitialUpdate ();
			 pFrame -> SetActiveView ( pGammaHistoView, FALSE );

			 m_pInfoWnd11 = pFrame;
			 m_pInfoWnd11 -> SetWindowPos ( pWnd, Rect.left + (Rect.right - Rect.left) / 3 * 2, Rect.top, (Rect.right - Rect.left) / 3, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
			 
			 break;

		case 13: // 3D color viewer
		{
			C3DColorView * p3DColorView;

			pFrame = new CSubFrame;
			pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			context.m_pCurrentDoc = GetDocument ();
			context.m_pCurrentFrame = pFrame;
			context.m_pLastView = this;
			context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			context.m_pNewViewClass = RUNTIME_CLASS ( C3DColorView );

			p3DColorView = (C3DColorView *) context.m_pNewViewClass->CreateObject();
			p3DColorView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			p3DColorView -> OnInitialUpdate ();
			pFrame -> SetActiveView ( p3DColorView, FALSE );
			pFrame -> OnSize ( 0, 0, 0 );

			m_pInfoWnd = pFrame;
			m_pInfoWnd -> SetWindowPos ( pWnd, Rect.left, Rect.top, (Rect.right - Rect.left), (Rect.bottom - Rect.top), SWP_NOACTIVATE );

			if ( m_3dDEFilter.GetSafeHwnd () )
			{
				m_3dDEFilter.ShowWindow ( SW_SHOW );
				p3DColorView -> SetDEFilter ( m_3dDEFilter.GetSel () );
			}
		}
		break;

		case 12: //auto
				CMultiFrame * pActiveFrame = (CMultiFrame *) ( (CMainFrame *) AfxGetMainWnd () ) -> MDIGetActive();
				int c1=0, c2=0, c3 = 0;

				if (pActiveFrame)
				{
					c1 = pActiveFrame->m_nTabbedViewIndex[1];
					c2 = pActiveFrame->m_nTabbedViewIndex[2];
					c3 = pActiveFrame->m_nTabbedViewIndex[3];
				}
				bool g1 = false;
				bool g2 = false;
				bool g3 = false;

				switch (c1)
				{
					case IDS_LUMINANCE:
					 pFrame = new CSubFrame;

					 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

					 context.m_pCurrentDoc = GetDocument ();
					 context.m_pCurrentFrame = pFrame;
					 context.m_pLastView = this;
					 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
					 context.m_pNewViewClass = RUNTIME_CLASS ( CLuminanceHistoView );

					 pLuminanceHistoView = (CLuminanceHistoView *) context.m_pNewViewClass->CreateObject();
					 pLuminanceHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
					 pLuminanceHistoView -> OnInitialUpdate ();
					 pFrame -> SetActiveView ( pLuminanceHistoView, FALSE );
					 pFrame -> OnSize ( 0, 0, 0 );
					 m_pInfoWnd = pFrame;
					 g1 = true;
					break;

					case IDS_GAMMA:
					 pFrame = new CSubFrame;

					 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

					 context.m_pCurrentDoc = GetDocument ();
					 context.m_pCurrentFrame = pFrame;
					 context.m_pLastView = this;
					 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
					 context.m_pNewViewClass = RUNTIME_CLASS ( CGammaHistoView );

					 pGammaHistoView = (CGammaHistoView *) context.m_pNewViewClass->CreateObject();
					 pGammaHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
					 pGammaHistoView -> OnInitialUpdate ();
					 pFrame -> SetActiveView ( pGammaHistoView, FALSE );
					 pFrame -> OnSize ( 0, 0, 0 );
					 m_pInfoWnd = pFrame;
					 g1 = true;
					break;

					case IDS_NEARBLACK:
					 pFrame = new CSubFrame;

					 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

					 context.m_pCurrentDoc = GetDocument ();
					 context.m_pCurrentFrame = pFrame;
					 context.m_pLastView = this;
					 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
					 context.m_pNewViewClass = RUNTIME_CLASS ( CNearBlackHistoView );

					 pNearBlackHistoView = (CNearBlackHistoView *) context.m_pNewViewClass->CreateObject();
					 pNearBlackHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
					 pNearBlackHistoView -> OnInitialUpdate ();
					 pFrame -> SetActiveView ( pNearBlackHistoView, FALSE );
					 pFrame -> OnSize ( 0, 0, 0 );
					 m_pInfoWnd = pFrame;
					 g1 = true;
					break;

					case IDS_NEARWHITE:
					 pFrame = new CSubFrame;

					 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

					 context.m_pCurrentDoc = GetDocument ();
					 context.m_pCurrentFrame = pFrame;
					 context.m_pLastView = this;
					 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
					 context.m_pNewViewClass = RUNTIME_CLASS ( CNearWhiteHistoView );

					 pNearWhiteHistoView = (CNearWhiteHistoView *) context.m_pNewViewClass->CreateObject();
					 pNearWhiteHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
					 pNearWhiteHistoView -> OnInitialUpdate ();
					 pFrame -> SetActiveView ( pNearWhiteHistoView, FALSE );
					 pFrame -> OnSize ( 0, 0, 0 );
					 m_pInfoWnd = pFrame;
					 g1 = true;
					break;

					case IDS_RGBLEVELS:
					 pFrame = new CSubFrame;

					 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

					 context.m_pCurrentDoc = GetDocument ();
					 context.m_pCurrentFrame = pFrame;
					 context.m_pLastView = this;
					 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
					 context.m_pNewViewClass = RUNTIME_CLASS ( CRGBHistoView );

					 pRGBHistoView = (CRGBHistoView *) context.m_pNewViewClass->CreateObject();
					 pRGBHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
					 pRGBHistoView -> OnInitialUpdate ();
					 pFrame -> SetActiveView ( pRGBHistoView, FALSE );
					 pFrame -> OnSize ( 0, 0, 0 );
					 m_pInfoWnd = pFrame;
					 g1 = true;
					break;

					case IDS_COLORTEMP:
					 pFrame = new CSubFrame;

					 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

					 context.m_pCurrentDoc = GetDocument ();
					 context.m_pCurrentFrame = pFrame;
					 context.m_pLastView = this;
					 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
					 context.m_pNewViewClass = RUNTIME_CLASS ( CColorTempHistoView );

					 pColorTempHistoView = (CColorTempHistoView *) context.m_pNewViewClass->CreateObject();
					 pColorTempHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
					 pColorTempHistoView -> OnInitialUpdate ();
					 pFrame -> SetActiveView ( pColorTempHistoView, FALSE );
					 pFrame -> OnSize ( 0, 0, 0 );
					 m_pInfoWnd = pFrame;
					 g1 = true;
					break;

					case IDS_SATLUM:
					 pFrame = new CSubFrame;

					 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

					 context.m_pCurrentDoc = GetDocument ();
					 context.m_pCurrentFrame = pFrame;
					 context.m_pLastView = this;
					 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
					 context.m_pNewViewClass = RUNTIME_CLASS ( CSatLumHistoView );

					 pSatLumHistoView = (CSatLumHistoView *) context.m_pNewViewClass->CreateObject();
					 pSatLumHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
					 pSatLumHistoView -> OnInitialUpdate ();
					 pFrame -> SetActiveView ( pSatLumHistoView, FALSE );
					 pFrame -> OnSize ( 0, 0, 0 );
					 m_pInfoWnd = pFrame;
					 g1 = true;
					break;

					case IDS_SATLUMSHIFT:
					 pFrame = new CSubFrame;

					 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

					 context.m_pCurrentDoc = GetDocument ();
					 context.m_pCurrentFrame = pFrame;
					 context.m_pLastView = this;
					 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
					 context.m_pNewViewClass = RUNTIME_CLASS ( CSatLumShiftView );

					 pSatLumShiftView = (CSatLumShiftView *) context.m_pNewViewClass->CreateObject();
					 pSatLumShiftView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
					 pSatLumShiftView -> OnInitialUpdate ();
					 pFrame -> SetActiveView ( pSatLumShiftView, FALSE );
					 pFrame -> OnSize ( 0, 0, 0 );
					 m_pInfoWnd = pFrame;
					 g1 = true;
					break;

					case IDS_FREEMEASURES:
					 pFrame = new CSubFrame;

					 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

					 context.m_pCurrentDoc = GetDocument ();
					 context.m_pCurrentFrame = pFrame;
					 context.m_pLastView = this;
					 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
					 context.m_pNewViewClass = RUNTIME_CLASS ( CMeasuresHistoView );

					 pMeasuresHistoView = (CMeasuresHistoView *) context.m_pNewViewClass->CreateObject();
					 pMeasuresHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
					 pMeasuresHistoView -> OnInitialUpdate ();
					 pFrame -> SetActiveView ( pMeasuresHistoView, FALSE );
					 pFrame -> OnSize ( 0, 0, 0 );
					 m_pInfoWnd = pFrame;
					 g1 = true;
					break;

					case IDS_CIECHARTVIEW_NAME:
					 pFrame = new CSubFrame;

					 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

					 context.m_pCurrentDoc = GetDocument ();
					 context.m_pCurrentFrame = pFrame;
					 context.m_pLastView = this;
					 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
					 context.m_pNewViewClass = RUNTIME_CLASS ( CCIEChartView );

					 pCIEChartView = (CCIEChartView *) context.m_pNewViewClass->CreateObject();
					 pCIEChartView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
					 pCIEChartView -> OnInitialUpdate ();
					 pFrame -> SetActiveView ( pCIEChartView, FALSE );
					 pFrame -> OnSize ( 0, 0, 0 );
					 m_pInfoWnd = pFrame;
					 g1 = true;
					break;
				}
				
				if (g1)
				{
					switch (c2)
					{
						case IDS_LUMINANCE:
						 pFrame = new CSubFrame;

						 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

						 context.m_pCurrentDoc = GetDocument ();
						 context.m_pCurrentFrame = pFrame;
						 context.m_pLastView = this;
						 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
						 context.m_pNewViewClass = RUNTIME_CLASS ( CLuminanceHistoView );

						 pLuminanceHistoView = (CLuminanceHistoView *) context.m_pNewViewClass->CreateObject();
						 pLuminanceHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
						 pLuminanceHistoView -> OnInitialUpdate ();
						 pFrame -> SetActiveView ( pLuminanceHistoView, FALSE );
						 m_pInfoWnd12 = pFrame;
						 g2 = true;
						break;

						case IDS_GAMMA:
						 pFrame = new CSubFrame;

						 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

						 context.m_pCurrentDoc = GetDocument ();
						 context.m_pCurrentFrame = pFrame;
						 context.m_pLastView = this;
						 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
						 context.m_pNewViewClass = RUNTIME_CLASS ( CGammaHistoView );

						 pGammaHistoView = (CGammaHistoView *) context.m_pNewViewClass->CreateObject();
						 pGammaHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
						 pGammaHistoView -> OnInitialUpdate ();
						 pFrame -> SetActiveView ( pGammaHistoView, FALSE );
						 m_pInfoWnd12 = pFrame;
						 g2 = true;
						break;

					case IDS_NEARBLACK:
					 pFrame = new CSubFrame;

					 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

					 context.m_pCurrentDoc = GetDocument ();
					 context.m_pCurrentFrame = pFrame;
					 context.m_pLastView = this;
					 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
					 context.m_pNewViewClass = RUNTIME_CLASS ( CNearBlackHistoView );

					 pNearBlackHistoView = (CNearBlackHistoView *) context.m_pNewViewClass->CreateObject();
					 pNearBlackHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
					 pNearBlackHistoView -> OnInitialUpdate ();
					 pFrame -> SetActiveView ( pNearBlackHistoView, FALSE );
					 m_pInfoWnd12 = pFrame;
					 g2 = true;
					break;

					case IDS_NEARWHITE:
					 pFrame = new CSubFrame;

					 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

					 context.m_pCurrentDoc = GetDocument ();
					 context.m_pCurrentFrame = pFrame;
					 context.m_pLastView = this;
					 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
					 context.m_pNewViewClass = RUNTIME_CLASS ( CNearWhiteHistoView );

					 pNearWhiteHistoView = (CNearWhiteHistoView *) context.m_pNewViewClass->CreateObject();
					 pNearWhiteHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
					 pNearWhiteHistoView -> OnInitialUpdate ();
					 pFrame -> SetActiveView ( pNearWhiteHistoView, FALSE );
					 m_pInfoWnd12 = pFrame;
					 g2 = true;
					break;

					case IDS_RGBLEVELS:
					 pFrame = new CSubFrame;

					 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

					 context.m_pCurrentDoc = GetDocument ();
					 context.m_pCurrentFrame = pFrame;
					 context.m_pLastView = this;
					 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
					 context.m_pNewViewClass = RUNTIME_CLASS ( CRGBHistoView );

					 pRGBHistoView = (CRGBHistoView *) context.m_pNewViewClass->CreateObject();
					 pRGBHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
					 pRGBHistoView -> OnInitialUpdate ();
					 pFrame -> SetActiveView ( pRGBHistoView, FALSE );
					 m_pInfoWnd12 = pFrame;
					 g2 = true;
					break;

					case IDS_COLORTEMP:
					 pFrame = new CSubFrame;

					 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

					 context.m_pCurrentDoc = GetDocument ();
					 context.m_pCurrentFrame = pFrame;
					 context.m_pLastView = this;
					 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
					 context.m_pNewViewClass = RUNTIME_CLASS ( CColorTempHistoView );

					 pColorTempHistoView = (CColorTempHistoView *) context.m_pNewViewClass->CreateObject();
					 pColorTempHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
					 pColorTempHistoView -> OnInitialUpdate ();
					 pFrame -> SetActiveView ( pColorTempHistoView, FALSE );
					 m_pInfoWnd12 = pFrame;
					 g2 = true;
					break;

					case IDS_SATLUM:
					 pFrame = new CSubFrame;

					 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

					 context.m_pCurrentDoc = GetDocument ();
					 context.m_pCurrentFrame = pFrame;
					 context.m_pLastView = this;
					 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
					 context.m_pNewViewClass = RUNTIME_CLASS ( CSatLumHistoView );

					 pSatLumHistoView = (CSatLumHistoView *) context.m_pNewViewClass->CreateObject();
					 pSatLumHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
					 pSatLumHistoView -> OnInitialUpdate ();
					 pFrame -> SetActiveView ( pSatLumHistoView, FALSE );
					 m_pInfoWnd12 = pFrame;
					 g2 = true;
					break;

					case IDS_SATLUMSHIFT:
					 pFrame = new CSubFrame;

					 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

					 context.m_pCurrentDoc = GetDocument ();
					 context.m_pCurrentFrame = pFrame;
					 context.m_pLastView = this;
					 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
					 context.m_pNewViewClass = RUNTIME_CLASS ( CSatLumShiftView );

					 pSatLumShiftView = (CSatLumShiftView *) context.m_pNewViewClass->CreateObject();
					 pSatLumShiftView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
					 pSatLumShiftView -> OnInitialUpdate ();
					 pFrame -> SetActiveView ( pSatLumShiftView, FALSE );
					 m_pInfoWnd12 = pFrame;
					 g2 = true;
					break;

					case IDS_FREEMEASURES:
					 pFrame = new CSubFrame;

					 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

					 context.m_pCurrentDoc = GetDocument ();
					 context.m_pCurrentFrame = pFrame;
					 context.m_pLastView = this;
					 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
					 context.m_pNewViewClass = RUNTIME_CLASS ( CMeasuresHistoView );

					 pMeasuresHistoView = (CMeasuresHistoView *) context.m_pNewViewClass->CreateObject();
					 pMeasuresHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
					 pMeasuresHistoView -> OnInitialUpdate ();
					 pFrame -> SetActiveView ( pMeasuresHistoView, FALSE );
					 m_pInfoWnd12 = pFrame;
					 g2 = true;
					break;

					case IDS_CIECHARTVIEW_NAME:
					 pFrame = new CSubFrame;

					 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

					 context.m_pCurrentDoc = GetDocument ();
					 context.m_pCurrentFrame = pFrame;
					 context.m_pLastView = this;
					 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
					 context.m_pNewViewClass = RUNTIME_CLASS ( CCIEChartView );

					 pCIEChartView = (CCIEChartView *) context.m_pNewViewClass->CreateObject();
					 pCIEChartView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
					 pCIEChartView -> OnInitialUpdate ();
					 pFrame -> SetActiveView ( pCIEChartView, FALSE );
					 m_pInfoWnd12 = pFrame;
					 g2 = true;
					break;

					}
				}

				if (g2)
				{
					switch (c3)
					{
						case IDS_LUMINANCE:
						 pFrame = new CSubFrame;

						 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

						 context.m_pCurrentDoc = GetDocument ();
						 context.m_pCurrentFrame = pFrame;
						 context.m_pLastView = this;
						 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
						 context.m_pNewViewClass = RUNTIME_CLASS ( CLuminanceHistoView );

						 pLuminanceHistoView = (CLuminanceHistoView *) context.m_pNewViewClass->CreateObject();
						 pLuminanceHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
						 pLuminanceHistoView -> OnInitialUpdate ();
						 pFrame -> SetActiveView ( pLuminanceHistoView, FALSE );
						 m_pInfoWnd13 = pFrame;
						 g3 = true;
						break;

					case IDS_GAMMA:
						 pFrame = new CSubFrame;

						 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

						 context.m_pCurrentDoc = GetDocument ();
						 context.m_pCurrentFrame = pFrame;
						 context.m_pLastView = this;
						 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
						 context.m_pNewViewClass = RUNTIME_CLASS ( CGammaHistoView );

						 pGammaHistoView = (CGammaHistoView *) context.m_pNewViewClass->CreateObject();
						 pGammaHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
						 pGammaHistoView -> OnInitialUpdate ();
						 pFrame -> SetActiveView ( pGammaHistoView, FALSE );
						 m_pInfoWnd13 = pFrame;
						 g3 = true;
						break;

					case IDS_NEARBLACK:
						 pFrame = new CSubFrame;

						 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

						 context.m_pCurrentDoc = GetDocument ();
						 context.m_pCurrentFrame = pFrame;
						 context.m_pLastView = this;
						 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
						 context.m_pNewViewClass = RUNTIME_CLASS ( CNearBlackHistoView );

						 pNearBlackHistoView = (CNearBlackHistoView *) context.m_pNewViewClass->CreateObject();
						 pNearBlackHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
						 pNearBlackHistoView -> OnInitialUpdate ();
						 pFrame -> SetActiveView ( pNearBlackHistoView, FALSE );
						 m_pInfoWnd13 = pFrame;
						 g3 = true;
					break;

					case IDS_NEARWHITE:
						 pFrame = new CSubFrame;

						 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

						 context.m_pCurrentDoc = GetDocument ();
						 context.m_pCurrentFrame = pFrame;
						 context.m_pLastView = this;
						 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
						 context.m_pNewViewClass = RUNTIME_CLASS ( CNearWhiteHistoView );

						 pNearWhiteHistoView = (CNearWhiteHistoView *) context.m_pNewViewClass->CreateObject();
						 pNearWhiteHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
						 pNearWhiteHistoView -> OnInitialUpdate ();
						 pFrame -> SetActiveView ( pNearWhiteHistoView, FALSE );
						 m_pInfoWnd13 = pFrame;
						 g3 = true;
					break;

					case IDS_RGBLEVELS:
						 pFrame = new CSubFrame;

						 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

						 context.m_pCurrentDoc = GetDocument ();
						 context.m_pCurrentFrame = pFrame;
						 context.m_pLastView = this;
						 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
						 context.m_pNewViewClass = RUNTIME_CLASS ( CRGBHistoView );

						 pRGBHistoView = (CRGBHistoView *) context.m_pNewViewClass->CreateObject();
						 pRGBHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
						 pRGBHistoView -> OnInitialUpdate ();
						 pFrame -> SetActiveView ( pRGBHistoView, FALSE );
						 m_pInfoWnd13 = pFrame;
						 g3 = true;
					break;

					case IDS_COLORTEMP:
						 pFrame = new CSubFrame;

						 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

						 context.m_pCurrentDoc = GetDocument ();
						 context.m_pCurrentFrame = pFrame;
						 context.m_pLastView = this;
						 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
						 context.m_pNewViewClass = RUNTIME_CLASS ( CColorTempHistoView );

						 pColorTempHistoView = (CColorTempHistoView *) context.m_pNewViewClass->CreateObject();
						 pColorTempHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
						 pColorTempHistoView -> OnInitialUpdate ();
						 pFrame -> SetActiveView ( pColorTempHistoView, FALSE );
						 m_pInfoWnd13 = pFrame;
						 g3 = true;
					break;

					case IDS_SATLUM:
						 pFrame = new CSubFrame;

						 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

						 context.m_pCurrentDoc = GetDocument ();
						 context.m_pCurrentFrame = pFrame;
						 context.m_pLastView = this;
						 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
						 context.m_pNewViewClass = RUNTIME_CLASS ( CSatLumHistoView );

						 pSatLumHistoView = (CSatLumHistoView *) context.m_pNewViewClass->CreateObject();
						 pSatLumHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
						 pSatLumHistoView -> OnInitialUpdate ();
						 pFrame -> SetActiveView ( pSatLumHistoView, FALSE );
						 m_pInfoWnd13 = pFrame;
						 g3 = true;
					break;

					case IDS_SATLUMSHIFT:
						 pFrame = new CSubFrame;

						 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

						 context.m_pCurrentDoc = GetDocument ();
						 context.m_pCurrentFrame = pFrame;
						 context.m_pLastView = this;
						 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
						 context.m_pNewViewClass = RUNTIME_CLASS ( CSatLumShiftView );

						 pSatLumShiftView = (CSatLumShiftView *) context.m_pNewViewClass->CreateObject();
						 pSatLumShiftView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
						 pSatLumShiftView -> OnInitialUpdate ();
						 pFrame -> SetActiveView ( pSatLumShiftView, FALSE );
						 m_pInfoWnd13 = pFrame;
						 g3 = true;
					break;

					case IDS_FREEMEASURES:
						 pFrame = new CSubFrame;

						 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

						 context.m_pCurrentDoc = GetDocument ();
						 context.m_pCurrentFrame = pFrame;
						 context.m_pLastView = this;
						 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
						 context.m_pNewViewClass = RUNTIME_CLASS ( CMeasuresHistoView );

						 pMeasuresHistoView = (CMeasuresHistoView *) context.m_pNewViewClass->CreateObject();
						 pMeasuresHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
						 pMeasuresHistoView -> OnInitialUpdate ();
						 pFrame -> SetActiveView ( pMeasuresHistoView, FALSE );
						 m_pInfoWnd13 = pFrame;
						 g3 = true;
					break;

					case IDS_CIECHARTVIEW_NAME:
						 pFrame = new CSubFrame;

						 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

						 context.m_pCurrentDoc = GetDocument ();
						 context.m_pCurrentFrame = pFrame;
						 context.m_pLastView = this;
						 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
						 context.m_pNewViewClass = RUNTIME_CLASS ( CCIEChartView );

						 pCIEChartView = (CCIEChartView *) context.m_pNewViewClass->CreateObject();
						 pCIEChartView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
						 pCIEChartView -> OnInitialUpdate ();
						 pFrame -> SetActiveView ( pCIEChartView, FALSE );
						 m_pInfoWnd13 = pFrame;
						 g3 = true;
					break;

					}
				}


				if (g1 && g2 && g3)
				{
					 m_pInfoWnd -> SetWindowPos ( pWnd, Rect.left, Rect.top, (Rect.right - Rect.left) / 3., (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
					 m_pInfoWnd12 -> SetWindowPos ( pWnd, Rect.left + (Rect.right - Rect.left) / 3., Rect.top, (Rect.right - Rect.left) / 3., (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
					 m_pInfoWnd13 -> SetWindowPos ( pWnd, Rect.left + (Rect.right - Rect.left) / 3. * 2., Rect.top, (Rect.right - Rect.left) / 3., (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
				}
				else if (g1 && g2)
				{
					 m_pInfoWnd -> SetWindowPos ( pWnd, Rect.left, Rect.top, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
					 m_pInfoWnd12 -> SetWindowPos ( pWnd, Rect.left + (Rect.right - Rect.left) / 2, Rect.top, (Rect.right - Rect.left) / 2, (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
				} else if (g1)
				{
					 m_pInfoWnd -> SetWindowPos ( pWnd, Rect.left, Rect.top, (Rect.right - Rect.left), (Rect.bottom - Rect.top) , SWP_NOACTIVATE );
				}

			break;

	}

	
	if ( m_pInfoWnd ) 
		m_pInfoWnd -> Invalidate ();

	InsetInfoWindows ();
}

void CMainView::OnChangeInfosEdit() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CFormView::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	if ( m_comboDisplay.GetCurSel () == 0 && m_pInfoWnd )
	{
		CString str;
		m_pInfoWnd -> GetWindowText ( str );
		GetDocument()->GetMeasure()->SetInfoString(str);
		GetDocument()->SetModifiedFlag(TRUE);
	}
}

void CMainView::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_MEASURES, NULL );
}

void CMainView::OnDatarefCheck() 
{
	UpdateData(TRUE);
	UpdateDataRef((IsDlgButtonChecked(IDC_DATAREF_CHECK)==BST_CHECKED), GetDocument());

	AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	
	UpdateData(FALSE);
}

void CMainView::OnInitDefaults()
{
	int rv = GetColorApp()->InMeasureMessageBox("This will reset your preferences to their default values.\nAre you sure?", "Reset prefs", MB_YESNO);
	if (rv == 6)
	{
		CString strlang = GetConfig()->strLang;
		DeleteFile("ColorHCFR.ini");
		GetConfig()->InitDefaults();
		GetConfig()->LoadSettings();
		GetConfig()->ApplySettings(TRUE);
		GetConfig()->WriteProfileString ( "Options", "Language", strlang );
		GetConfig()->WriteProfileInt("GDIGenerator","DisplayMode", DISPLAY_DEFAULT_MODE);
		GetDocument()->GetGenerator()->SetPropertiesSheetValues();
		GetConfig()->SaveSettings();
		GetConfig()->m_bSave = TRUE;
		GetDocument()->SetModifiedFlag(TRUE);
		GetDocument()->UpdateAllViews ( NULL, UPD_EVERYTHING );
		AfxGetMainWnd()->SendMessage(WM_SYSCOLORCHANGE);
	}
}

void CMainView::OnAvgLowLightCheck()
{
	CSensor* pS = GetDocument() ? GetDocument()->m_pSensor : NULL;
	if (pS == NULL || !pS->supportsAvg())
		return;
	bool bOn = (m_avgLowLightCheck.GetCheck() == BST_CHECKED);
	pS->setAvgEnabled(bOn);
}

void CMainView::OnAdjustXYZCheck()
{
	BOOL	bAdjust = m_AdjustXYZCheckButton.GetCheck ();
	Matrix CurrentMatrix = GetDocument ()->m_pSensor->GetSensorMatrix();
	

	if (!bAdjust) //restore uncorrected sensor values
	{
		ASSERT(0);
		GetDocument ()->m_pSensor->SetSensorMatrixMod( CurrentMatrix );
		GetDocument ()->m_pSensor->SetSensorMatrix( Matrix::IdentityMatrix(3) );
		GetDocument ()->m_measure.ApplySensorAdjustmentMatrix( CurrentMatrix.GetInverse() );
		m_AdjustXYZCheckButton.SetCheck(FALSE);
	}
	else  //reapply saved correction matrix
	{
		ASSERT(0);
		GetDocument ()->m_pSensor->SetSensorMatrix( GetDocument ()->m_pSensor->GetSensorMatrixMod() );
		GetDocument ()->m_measure.ApplySensorAdjustmentMatrix(GetDocument ()->m_pSensor->GetSensorMatrixMod() );
		GetDocument ()->m_pSensor->SetSensorMatrixMod( Matrix::IdentityMatrix(3) );
		m_AdjustXYZCheckButton.SetCheck(TRUE);
	}

	if ( m_pGrayScaleGrid->GetSelectedCellRange().IsValid () )
	{
		m_pGrayScaleGrid->SetSelectedRange(-1,-1,-1,-1);
		m_pGrayScaleGrid->SetFocusCell(-1,-1);
		SetSelectedColor ( noDataColor );
		(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	}

	GetDocument()->UpdateAllViews ( NULL, UPD_EVERYTHING );
	AfxGetMainWnd () -> SendMessageToDescendants ( WM_COMMAND, IDM_REFRESH_REFERENCE );
}


void CMainView::OnEditCopy() 
{
	if ( m_pGrayScaleGrid -> m_hWnd == ::GetFocus () )
	{
		NM_GRIDVIEW GridItem;
		CCellRange Selection = m_pGrayScaleGrid -> GetSelectedCellRange();
		int minCol = Selection.GetMinCol();
		int maxCol = Selection.GetMaxCol();
		int minRow = Selection.GetMinRow();
		int maxRow = Selection.GetMaxRow();

		if ( minRow < 1 )
			minRow = 1;
		if ( maxRow > 3 )
			maxRow = 3;
		
		m_pGrayScaleGrid -> SetRedraw(FALSE);

		for ( int nRow = minRow ; nRow <= maxRow ; nRow ++ )
		{
			for ( int nCol = minCol ; nCol <= maxCol ; nCol ++ )
			{
				GridItem.iColumn = nCol;
				GridItem.iRow = nRow;
				OnGrayScaleGridBeginEdit ( (NMHDR *) & GridItem, NULL );
			}
		}


		m_pGrayScaleGrid -> OnEditCopy();
		UpdateGrid();
		m_pGrayScaleGrid -> SetRedraw(TRUE);
	}

	if ( m_comboDisplay.GetCurSel () == 0 && m_pInfoWnd -> m_hWnd == ::GetFocus () )
		m_pInfoWnd -> SendMessage ( WM_COPY );

	if ( m_pGrayScaleGrid -> m_hWnd == ::GetParent(::GetFocus ()) )
		GetFocus () -> SendMessage ( WM_COPY );
}

void CMainView::OnUpdateEditCopy(CCmdUI* pCmdUI) 
{
	if ( m_pGrayScaleGrid -> m_hWnd == ::GetFocus () )
		m_pGrayScaleGrid -> OnUpdateEditCopy(pCmdUI);
}

void CMainView::OnEditCut() 
{
	if ( m_pGrayScaleGrid -> m_hWnd == ::GetFocus () )
	{
		OnEditCopy();
		return;
	}

	if ( m_comboDisplay.GetCurSel () == 0 && m_pInfoWnd -> m_hWnd == ::GetFocus () )
		m_pInfoWnd -> SendMessage ( WM_CUT );

	if ( m_pGrayScaleGrid -> m_hWnd == ::GetParent(::GetFocus ()) )
		GetFocus () -> SendMessage ( WM_CUT );
}

void CMainView::OnUpdateEditCut(CCmdUI* pCmdUI) 
{
	if ( m_pGrayScaleGrid -> m_hWnd == ::GetFocus () )
		m_pGrayScaleGrid -> OnUpdateEditCut(pCmdUI);
}

void CMainView::OnEditPaste() 
{
	if ( m_pGrayScaleGrid -> m_hWnd == ::GetFocus () )
		m_pGrayScaleGrid -> OnEditPaste();

	if ( m_comboDisplay.GetCurSel () == 0 && m_pInfoWnd -> m_hWnd == ::GetFocus () )
		m_pInfoWnd -> SendMessage ( WM_PASTE );

	if ( m_pGrayScaleGrid -> m_hWnd == ::GetParent(::GetFocus ()) )
		GetFocus () -> SendMessage ( WM_PASTE );
}

void CMainView::OnUpdateEditPaste(CCmdUI* pCmdUI) 
{
	if ( m_pGrayScaleGrid -> m_hWnd == ::GetFocus () )
		m_pGrayScaleGrid -> OnUpdateEditPaste(pCmdUI);
}

// The Summary info window is the only editable text this view owns, so the Edit
// menu's Undo (and its Ctrl+Z / Alt+Backspace accelerators) act on it. CEditEx
// keeps its own command history - the edit control's built-in undo is not used.
static CEditEx * GetSummaryEdit ( CWnd * pInfoWnd )
{
	CEditEx * pEdit = DYNAMIC_DOWNCAST ( CEditEx, pInfoWnd );

	if ( pEdit == NULL || ! ::IsWindow ( pEdit -> GetSafeHwnd () ) )
		return NULL;

	// Ctrl+Z and Alt+Backspace are frame accelerators, so ID_EDIT_UNDO is routed to
	// this view whatever holds the focus. The competitor is the grid's in-place cell
	// editor: CInPlaceEdit is a CEdit with its own undo, and it only swallows
	// WM_SYSCHAR in PreTranslateMessage, so Ctrl+Z pressed while editing a cell still
	// reaches the accelerator and would roll back the Summary pane instead - a
	// rollback that goes on to rewrite the document's info string through EN_CHANGE.
	// OnEditCut / OnEditPaste gate on the focus the same way, including the explicit
	// ::GetParent ( ::GetFocus () ) branch for that same in-place editor.
	if ( pEdit -> m_hWnd != ::GetFocus () )
		return NULL;

	return pEdit;
}

void CMainView::OnEditUndo() 
{
	CEditEx * pEdit = GetSummaryEdit ( m_pInfoWnd );

	if ( pEdit )
		pEdit -> Undo ();
}

void CMainView::OnUpdateEditUndo(CCmdUI* pCmdUI) 
{
	CEditEx * pEdit = GetSummaryEdit ( m_pInfoWnd );

	pCmdUI -> Enable ( pEdit != NULL && pEdit -> CanUndo () );
}

void CMainView::UpdateAllGrids() 
{
	InitGrid();
	if(m_pGrayScaleGrid)
		UpdateGrid();
	
	if ( m_pGrayScaleGrid->GetSelectedCellRange().IsValid () )
	{
		m_pGrayScaleGrid->SetSelectedRange(-1,-1,-1,-1);
		m_pGrayScaleGrid->SetFocusCell(-1,-1);
	}
}


// Tool function used during serialization (retrieve current view state: selected grid, unit, information, grid size)
DWORD CMainView::GetUserInfo ()
{
	DWORD	dwSubViewUserInfo = 0;
	if ( m_infoDisplay >= 3 && m_pInfoWnd != NULL && m_infoDisplay != 11 )
	{
		// Info display is a sub view
		dwSubViewUserInfo = ( (CSavingView *) ( ( (CSubFrame *) m_pInfoWnd ) -> GetActiveView () ) ) -> GetUserInfo ();
	}
		
	// m_displayMode: selected grid - between 0 and 12						-> bits 0-5		0-64
	// m_displayType: selected unit - between 0 and 4						-> bits 6-9		0-16
	// m_infoDisplay: selected information screen - between 0 and 8			-> bits 10-15	0-64
	// m_nSizeOffset: window offset (grid added height) - between 0 and 105	-> bits 16-23	0-256
	// dwSubViewUserInfo: sub view parameters use the rest:					-> bits 24-31	0-256

	return ( m_displayMode & 0x003F ) | ( ( m_displayType & 0x000F ) << 6 ) | ( ( m_infoDisplay & 0x003F ) << 10 ) | ( ( m_nSizeOffset & 0x00FF ) << 16 ) | ( ( dwSubViewUserInfo & 0x00FF ) << 24 );
}

void CMainView::SetUserInfo ( DWORD dwUserInfo )
{
	m_dwInitialUserInfo = dwUserInfo;
}

CSubFrame::CSubFrame() : CFrameWnd ()
{
}

IMPLEMENT_DYNAMIC(CSubFrame, CFrameWnd)

BOOL CSubFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if ( ! CFrameWnd::PreCreateWindow ( cs ) )
		return FALSE;
	cs.dwExStyle &= ~( WS_EX_CLIENTEDGE | WS_EX_WINDOWEDGE | WS_EX_STATICEDGE | WS_EX_DLGMODALFRAME );
	cs.style &= ~WS_BORDER;
	return TRUE;
}

BEGIN_MESSAGE_MAP(CSubFrame, CFrameWnd)
	//{{AFX_MSG_MAP(CSubFrame)
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CLuminanceWnd message handlers

void CSubFrame::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	
	// No paint: fully transparent window
}

void CSubFrame::OnSize(UINT nType, int cx, int cy) 
{
	CView *	pView;
	CRect	Rect;

	pView = GetActiveView ();

	if ( pView )
	{
		GetClientRect ( & Rect );
		pView -> SetWindowPos ( NULL, 0, 0, Rect.Width (), Rect.Height (), SWP_NOACTIVATE | SWP_NOZORDER );
	}	
	CFrameWnd::OnSize(nType, cx, cy);
}

BOOL CSubFrame::OnEraseBkgnd(CDC* pDC) 
{

	// No erase at all: fully transparent window
	return true;
}

void CSubFrame::OnDestroy() 
{
	CView *	pView;

	pView = GetActiveView ();
	pView -> DestroyWindow ();

	CFrameWnd::OnDestroy();
}


void CMainView::OnDeltaposSpinView(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
	// TODO: Add your control notification handler code here
	
	if ( pNMUpDown -> iDelta > 0 )
	{
		if ( m_nSizeOffset < 100 )
		{
			m_nSizeOffset += 21;
			( (CMultiFrame *) GetParentFrame () ) -> EnsureMinimumSize ();
			InvalidateRect ( NULL );
			OnSize (0,0,0);
		}
	}
	else
	{
		if ( m_nSizeOffset > -63 )
		{
			m_nSizeOffset -= 21;
			InvalidateRect ( NULL );
			OnSize (0,0,0);
		}
	}

	*pResult = 0;

//	UpdateGrid();
}


void CMainView::OnSizePlus()
{
	if ( m_nSizeOffset < 100 )
	{
		m_nSizeOffset += 21;
		( (CMultiFrame *) GetParentFrame () ) -> EnsureMinimumSize ();
		InvalidateRect ( NULL );
		OnSize ( 0, 0, 0 );
		m_statsBar.Invalidate ( FALSE );
	}
}

void CMainView::OnSizeMinus()
{
	if ( m_nSizeOffset > -63 )
	{
		m_nSizeOffset -= 21;
		InvalidateRect ( NULL );
		OnSize ( 0, 0, 0 );
		m_statsBar.Invalidate ( FALSE );
	}
}

void CMainView::OnAnsiContrastPatternTestButton() 
{
	if ( IsMeasureSweepActive() ) return;
	CString	Msg;

	CGenerator *pGenerator=GetDocument()->GetGenerator();

	pGenerator->Init();
	pGenerator->DisplayAnsiBWRects(FALSE);

	Msg.LoadString ( IDS_CLICKOK );
	GetColorApp()->InMeasureMessageBox(Msg, "ANSI Pattern");
	pGenerator->Release();
}


void CMainView::OnRefs()
{
	if ( IsMeasureSweepActive() ) return;
	GetConfig()->ChangeSettings(1);
}

void CMainView::OnMeasureSatColorAllLevels()
{
	// While a sweep runs this button shows the red "click to stop" state
	// (SetAllLevelsButtonStop), so a click here must abort -- mirror OnMeasureGrayScale.
	if ( IsMeasureSweepActive() )
	{
		GetDocument()->GetMeasure()->AbortMeasure();
		return;
	}
	if ( m_displayMode < 5 || m_displayMode > 10 ) return;   // saturation modes only
	GetDocument()->MeasureSatColorAllLevels( m_displayMode - 5 );   // 0=R..5=M
}

BOOL CMainView::PreTranslateMessage(MSG* pMsg)
{
	m_tooltip.RelayEvent(pMsg);

	// Continuous free-run measurement (case 2's "Run continuous" toggle) is driven by a
	// background thread that has no accelerator wired to it -- Escape falls through untouched,
	// so the Go button is stuck showing its red "click to stop" state. Only act when this view's
	// own document owns the running thread: OnContinuousMeasurement() toggles based on
	// g_pDataDocRunningThread, and calling it for an unrelated document would stop that one and
	// start a new run here instead of just cancelling.
	if ( pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE &&
	     g_pDataDocRunningThread && g_pDataDocRunningThread == GetDocument() )
	{
		GetDocument()->OnContinuousMeasurement();
		return TRUE;
	}

	return CWnd::PreTranslateMessage(pMsg);
}