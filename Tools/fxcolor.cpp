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
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"        // Standard windows header file

#ifndef COLOR_MENUBAR				// copied from NewMenu.cpp
#define COLOR_MENUBAR       30
#endif

#include "fxcolor.h"
#include <commctrl.h>
#include <math.h>
#include <stdio.h>
#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")
#ifndef BP_RADIOBUTTON
#define BP_RADIOBUTTON 2
#endif
#ifndef BP_CHECKBOX
#define BP_CHECKBOX 3
#endif 


BOOL fxDrawMenuBorder = TRUE;
BOOL fxUseCustomColor = FALSE;

COLORREF fxColorWindow;
COLORREF fxColorMenu;
COLORREF fxColorSelection;
COLORREF fxColorText;

//
//	Copied from uxtheme.h
//  If you have this new header, then delete these and
//  #include <uxtheme.h> instead!
//
#define ETDT_DISABLE        0x00000001
#define ETDT_ENABLE         0x00000002
#define ETDT_USETABTEXTURE  0x00000004
#define ETDT_ENABLETAB      (ETDT_ENABLE  | ETDT_USETABTEXTURE)

//---- flags to control theming within an app ----
#ifndef STAP_ALLOW_NONCLIENT
#define STAP_ALLOW_NONCLIENT    (1 << 0)
#define STAP_ALLOW_CONTROLS     (1 << 1)
#define STAP_ALLOW_WEBCONTENT   (1 << 2)
#endif

// 
typedef HRESULT (WINAPI * EnableThemeDialogTextureFunct) (HWND, DWORD);
typedef HRESULT (WINAPI * SetWindowThemeFunct)(HWND hwnd, LPCWSTR pszSubAppName, LPCWSTR pszSubIdList);
typedef void (WINAPI *SetThemeAppPropertiesFunct)(DWORD dwFlags);

//
//	Try to call EnableThemeDialogTexture, if uxtheme.dll is present
//
BOOL FxEnableThemeDialogTexture(HWND hwnd, BOOL bEnable)
{
	HMODULE hUXTheme;
	EnableThemeDialogTextureFunct fnEnableThemeDialogTexture;

	hUXTheme = LoadLibrary(_T("uxtheme.dll"));

	if(hUXTheme)
	{
		fnEnableThemeDialogTexture = 
			(EnableThemeDialogTextureFunct)GetProcAddress(hUXTheme, "EnableThemeDialogTexture");

		if(fnEnableThemeDialogTexture)
		{
			if(bEnable)
				fnEnableThemeDialogTexture(hwnd, ETDT_ENABLETAB);
			else
				fnEnableThemeDialogTexture(hwnd, ETDT_DISABLE);

			
			FreeLibrary(hUXTheme);
			return TRUE;
		}
		else
		{
			// Failed to locate API!
			FreeLibrary(hUXTheme);
			return FALSE;
		}
	}
	else
	{
		// Not running under XP? Just fail gracefully
		return FALSE;
	}
}

BOOL FxEnableWindowTheme(HWND hwnd, BOOL bEnable)
{
	HMODULE hUXTheme;
	SetWindowThemeFunct fnSetWindowThemeFunct;

	hUXTheme = LoadLibrary(_T("uxtheme.dll"));

	if(hUXTheme)
	{
		fnSetWindowThemeFunct = 
			(SetWindowThemeFunct)GetProcAddress(hUXTheme, "SetWindowTheme");

		if(fnSetWindowThemeFunct)
		{
			if(bEnable)
				fnSetWindowThemeFunct(hwnd, NULL, NULL);
			else
				fnSetWindowThemeFunct(hwnd, L" ", L" ");

			
			FreeLibrary(hUXTheme);
			return TRUE;
		}
		else
		{
			// Failed to locate API!
			FreeLibrary(hUXTheme);
			return FALSE;
		}
	}
	else
	{
		// Not running under XP? Just fail gracefully
		return FALSE;
	}
}

BOOL FxEnableControlsTheme(BOOL bEnable)
{
	HMODULE hUXTheme;
	SetThemeAppPropertiesFunct fnSetThemeAppProperties;

	hUXTheme = LoadLibrary(_T("uxtheme.dll"));

	if(hUXTheme)
	{
		fnSetThemeAppProperties = 
			(SetThemeAppPropertiesFunct)GetProcAddress(hUXTheme, "SetThemeAppProperties");

		if(fnSetThemeAppProperties)
		{
			if(bEnable)
				fnSetThemeAppProperties(STAP_ALLOW_NONCLIENT | STAP_ALLOW_CONTROLS | STAP_ALLOW_WEBCONTENT);
			else
				fnSetThemeAppProperties(STAP_ALLOW_WEBCONTENT);
			
			FreeLibrary(hUXTheme);
			return TRUE;
		}
		else
		{
			// Failed to locate API!
			FreeLibrary(hUXTheme);
			return FALSE;
		}
	}
	else
	{
		// Not running under XP? Just fail gracefully
		return FALSE;
	}
}

void SetFxColors(COLORREF aColorWindow,COLORREF aColorMenu,COLORREF aColorSelection,COLORREF aColorText)
{
	fxColorWindow=aColorWindow;
	fxColorMenu=aColorMenu;
	fxColorSelection=aColorSelection;
	fxColorText=aColorText;

	FxEnableControlsTheme(TRUE);
}	
		
COLORREF FxGetWindowColor()
{
	if(fxColorWindow == CLR_DEFAULT)
		return GetSysColor(COLOR_WINDOW);
	else
		return fxColorWindow;
}

COLORREF FxGetMenuColor()
{
	if(fxColorMenu == CLR_DEFAULT)
	{
	  UINT nMenuDrawMode = CNewMenu::GetMenuDrawMode();
	  switch(nMenuDrawMode)
	  {
		  case CNewMenu::STYLE_XP_2003 :
		  case CNewMenu::STYLE_XP_2003_NOBORDER :
//			return 0x00e55400;
//			return GetSysColor(COLOR_ACTIVECAPTION);
//			return MidColor(GetSysColor(COLOR_WINDOW),GetSysColor(COLOR_ACTIVECAPTION));
			return LightenColor(155,GetSysColor(COLOR_ACTIVECAPTION));

		  case CNewMenu::STYLE_XP :
		  case CNewMenu::STYLE_XP_NOBORDER :
		  case CNewMenu::STYLE_ICY:
		  case CNewMenu::STYLE_ICY_NOBORDER:
		  case CNewMenu::STYLE_ORIGINAL :
		  case CNewMenu::STYLE_ORIGINAL_NOBORDER :
		  case CNewMenu::STYLE_COLORFUL :
		  case CNewMenu::STYLE_COLORFUL_NOBORDER :
			return GetSysColor(COLOR_3DFACE);

		  default:
			return GetSysColor(COLOR_MENU);
	  }
	}
	else
		return fxColorMenu;
}

COLORREF FxGetSelectionColor()
{
	if(fxColorSelection == CLR_DEFAULT)
	{
	  UINT nMenuDrawMode = CNewMenu::GetMenuDrawMode();
	  switch(nMenuDrawMode)
	  {
		case CNewMenu::STYLE_XP_2003 :
		case CNewMenu::STYLE_XP_2003_NOBORDER :
		  return RGB(255,238,194);  // Orange of office 2003 by default
		  break;
		default:
		  return GetSysColor(COLOR_HIGHLIGHT);
	  }
	}
	else
		return fxColorSelection;
}

COLORREF FxGetTextColor()
{
	if(fxColorText == CLR_DEFAULT)
		return GetSysColor(COLOR_MENUTEXT);
	else
		return fxColorText;
}

COLORREF FxGetSysColor(int nIndex)
{
	if(fxUseCustomColor)
		switch(nIndex)
	{
	    case COLOR_MENUBAR:
		case COLOR_MENU:
		case COLOR_GRADIENTACTIVECAPTION:
			return FxGetMenuColor();
			break;
		case COLOR_3DFACE:
			return FxGetMenuColor();
			break;
		case COLOR_INACTIVECAPTION:
			return DarkenColor(70,FxSaturateColor(150,FxGetMenuColor()));
			break;
		case COLOR_GRADIENTINACTIVECAPTION:
			return LightenColor(40,FxSaturateColor(150,FxGetMenuColor()));
			break;
		case COLOR_ACTIVECAPTION:
			return FxGetMenuColor();
			break;
		case COLOR_WINDOW:
			return FxGetWindowColor();
			break;
		case COLOR_HIGHLIGHT:
			return FxGetSelectionColor();
			break;
		case COLOR_BTNHIGHLIGHT:
			return DarkenColor(50,FxSaturateColor(200,FxGetSelectionColor()));
			break;
		case COLOR_HIGHLIGHTTEXT:
			return FxSaturateColor(150,FxGetTextColor());
			break;
		case COLOR_BTNSHADOW:
		case COLOR_3DDKSHADOW:
//			return DarkenColor(200,FxGetMenuColor());
			break;
		case COLOR_3DLIGHT:
//			return LightenColor(50,FxGetMenuColor());
			break;
		case COLOR_MENUTEXT:
		case COLOR_BTNTEXT:
			return FxGetTextColor();
			break;
		case COLOR_GRAYTEXT:
			if (fxUseCustomColor) return RGB(120,120,120); return LightenColor(110,GrayColor(FxGetTextColor()));
			break;
		case COLOR_INACTIVECAPTIONTEXT:
		case COLOR_WINDOWTEXT:
			return FxGetTextColor();
			break;
		default:
			return GetSysColor(nIndex);
	}
    return GetSysColor(nIndex);
}

HRESULT FxLocalGetThemeColor(HANDLE hTheme,
    int iPartId,
    int iStateId,
    int iColorId,
    COLORREF *pColor)
{
  switch(iColorId)
  {
	case 3821:	// color caption
		*pColor=FxGetMenuColor();
		return TRUE;
		break;
	case 3805:	// color window
		*pColor=FxGetWindowColor();
		return TRUE;
		break;
  }
  return FALSE;
}

void FxGetMenuBarColors(COLORREF& colorTop, COLORREF& colorBottom)
{
  UINT nMenuDrawMode = CNewMenu::GetMenuDrawMode();
  COLORREF colorBack;

  switch(nMenuDrawMode)
  {
	  case CNewMenu::STYLE_XP_2003 :
	  case CNewMenu::STYLE_XP_2003_NOBORDER :
		CNewMenu::GetMenuBarColor2003(colorTop,colorBottom);
		return;

	  case CNewMenu::STYLE_ICY:
	  case CNewMenu::STYLE_ICY_NOBORDER:
		colorBack=CNewMenu::GetMenuColor();
		colorTop=LightenColor(30,colorBack);
		colorBottom=DarkenColor(30,colorBack);
		return;

	  case CNewMenu::STYLE_XP:
	  case CNewMenu::STYLE_XP_NOBORDER:
  		colorTop=colorBottom=CNewMenu::GetMenuBarColorXP();
		return;

	  default:
		colorTop=colorBottom=FxGetSysColor(COLOR_WINDOW);
		return;
  }
}

COLORREF FxGetMenuBgColor()
{
  UINT nMenuDrawMode = CNewMenu::GetMenuDrawMode();

  switch(nMenuDrawMode)
  {
	  case CNewMenu::STYLE_XP_2003 :
	  case CNewMenu::STYLE_XP_2003_NOBORDER :
	  case CNewMenu::STYLE_ICY:
	  case CNewMenu::STYLE_ICY_NOBORDER:
	  case CNewMenu::STYLE_XP:
	  case CNewMenu::STYLE_XP_NOBORDER:
	  case CNewMenu::STYLE_COLORFUL:
	  case CNewMenu::STYLE_COLORFUL_NOBORDER:
		return DarkenColor(20,FxGetSysColor(COLOR_WINDOW));

	  case CNewMenu::STYLE_ORIGINAL:
	  case CNewMenu::STYLE_ORIGINAL_NOBORDER:
		  return FxGetSysColor(COLOR_3DFACE);

	  default:
		return FxGetSysColor(COLOR_WINDOW);
  }
}

void FxGetMenuBgColors(COLORREF& colorTop, COLORREF& colorBottom)
{
  UINT nMenuDrawMode = CNewMenu::GetMenuDrawMode();

  COLORREF colorBg=FxGetMenuBgColor();

  switch(nMenuDrawMode)
  {
	  case CNewMenu::STYLE_XP_2003 :
	  case CNewMenu::STYLE_XP_2003_NOBORDER :
		  colorTop=DarkenColor(128,colorBg);
		  colorBottom=LightenColor(128,colorTop);
		  return;

	  case CNewMenu::STYLE_ICY:
	  case CNewMenu::STYLE_ICY_NOBORDER:
		  colorTop=DarkenColor(128,colorBg);
		  colorBottom=LightenColor(128,colorTop);
		  return;

	  case CNewMenu::STYLE_XP:
	  case CNewMenu::STYLE_XP_NOBORDER:
		  colorTop=DarkenColor(128,colorBg);
		  colorBottom=LightenColor(128,colorTop);
		  return;

	  default:
		  colorTop=colorBottom=colorBg;
		  return;
  }
}

void RGBtoHSV(double R, double G, double B, double& H, double& S, double& V)
{
	double  var_R = ( R / 255.0 );                     //RGB values = From 0 to 255
	double var_G = ( G / 255.0 );
	double var_B = ( B / 255.0 );

	double  var_Min = min(min( var_R, var_G), var_B );    //Min. value of RGB
	double  var_Max = max(max( var_R, var_G), var_B );    //Max. value of RGB
	double  del_Max = var_Max - var_Min ;            //Delta RGB value

	V = var_Max;

	if ( del_Max == 0 )                     //This is a gray, no chroma...
	{
	   H = 0;                                //HSL results = From 0 to 1
	   S = 0;
	}
	else                                    //Chromatic data...
	{
	   S = del_Max / var_Max;

	   double  del_R = ( ( ( var_Max - var_R ) / 6.0 ) + ( del_Max / 2.0 ) ) / del_Max;
	   double  del_G = ( ( ( var_Max - var_G ) / 6.0 ) + ( del_Max / 2.0 ) ) / del_Max;
	   double  del_B = ( ( ( var_Max - var_B ) / 6.0 ) + ( del_Max / 2.0 ) ) / del_Max;

	   if      ( var_R == var_Max ) H = del_B - del_G;
	   else if ( var_G == var_Max ) H = ( 1.0 / 3.0 ) + del_R - del_B;
	   else if ( var_B == var_Max ) H = ( 2.0 / 3.0 ) + del_G - del_R;

	   if ( H < 0 ) H += 1.0;
	   if ( H > 1 ) H -= 1.0;
	}
}

void HSVtoRGB(double H, double S, double V, double& R, double& G, double& B)
{
	if ( S == 0 )                       //HSL values = From 0 to 1
	{
	   R = V * 255.0;                      //RGB results = From 0 to 255
	   G = V * 255.0;
	   B = V * 255.0;
	}
	else
	{
	   double var_h = H * 6;
	   double var_i = floor( var_h );             //Or ... var_i = floor( var_h )
	   double var_1 = V * ( 1 - S );
	   double var_2 = V * ( 1 - S * ( var_h - var_i ) );
	   double var_3 = V * ( 1 - S * ( 1 - ( var_h - var_i ) ) );

	   double var_r,var_g,var_b;

	   if      ( var_i == 0 ) { var_r = V     ; var_g = var_3 ; var_b = var_1; }
	   else if ( var_i == 1 ) { var_r = var_2 ; var_g = V     ; var_b = var_1; }
	   else if ( var_i == 2 ) { var_r = var_1 ; var_g = V     ; var_b = var_3; }
	   else if ( var_i == 3 ) { var_r = var_1 ; var_g = var_2 ; var_b = V;     }
	   else if ( var_i == 4 ) { var_r = var_3 ; var_g = var_1 ; var_b = V;     }
	   else                   { var_r = V     ; var_g = var_1 ; var_b = var_2; }

	   R = var_r * 255.0;                  //RGB results = From 0 to 255
	   G = var_g * 255.0;
	   B = var_b * 255.0;
	} 
}

void RGBtoHSL(double R, double G, double B, double& H, double& S, double& L)
{
    double minval = min(R, min(G, B));
    double maxval = max(R, max(G, B));
    double mdiff  = double(maxval) - double(minval);
    double msum   = double(maxval) + double(minval);
   
    L = msum / 510.0f;

    if (maxval == minval) 
    {
      S = 0.0f;
      H = 0.0f; 
    }   
    else 
    { 
      double rnorm = (maxval - R  ) / mdiff;      
      double gnorm = (maxval - G) / mdiff;
      double bnorm = (maxval - B ) / mdiff;   

      S = (L <= 0.5f) ? (mdiff / msum) : (mdiff / (510.0f - msum));

      if (R   == maxval) H = 60.0f * (6.0f + bnorm - gnorm);
      if (G == maxval) H = 60.0f * (2.0f + rnorm - bnorm);
      if (B  == maxval) H = 60.0f * (4.0f + gnorm - rnorm);
      if (H > 360.0f) H = H - 360.0f;
	}
}

double ToRGB1(double rm1, double rm2, double rh)
{
  if      (rh > 360.0f) rh -= 360.0f;
  else if (rh <   0.0f) rh += 360.0f;
 
  if      (rh <  60.0f) rm1 = rm1 + (rm2 - rm1) * rh / 60.0f;   
  else if (rh < 180.0f) rm1 = rm2;
  else if (rh < 240.0f) rm1 = rm1 + (rm2 - rm1) * (240.0f - rh) / 60.0f;      
                   
  return (rm1 * 255);
}

void HSLtoRGB(double H, double S, double L, double& R, double& G, double& B)
{
    if (S == 0.0) // Grauton, einfacher Fall
    {
      R = G = B = unsigned char(L * 255.0);
    }
    else
    {
      double rm1, rm2;
         
      if (L <= 0.5f) rm2 = L + L * S;  
      else                     rm2 = L + S - L * S;
      rm1 = 2.0f * L - rm2;   
      R   = ToRGB1(rm1, rm2, H + 120.0f);   
      G = ToRGB1(rm1, rm2, H);
      B  = ToRGB1(rm1, rm2, H - 120.0f);
    }
}

COLORREF FxSaturateColor(int aSatPercent,COLORREF aColor)
{
	double h,s,l;
	double r,g,b;

	RGBtoHSV(GetRValue(aColor),GetGValue(aColor),GetBValue(aColor),h,s,l);
//	RGBtoHSL(GetRValue(aColor),GetGValue(aColor),GetBValue(aColor),h,s,l);
	double s1=s * aSatPercent / 100.0;
	if(s1 < 1.0)
		s=s1;
	HSVtoRGB(h,s,l,r,g,b);
//	HSLtoRGB(h,s,l,r,g,b);
	return RGB(r,g,b);
}

void ApplyDarkTitleBar(HWND hWnd, BOOL bDark)
{
	if (hWnd == NULL)
		return;
	HMODULE hDwm = LoadLibrary(_T("dwmapi.dll"));
	if (hDwm == NULL)
		return;
	typedef HRESULT (WINAPI *DwmSetWindowAttributeFunc)(HWND, DWORD, LPCVOID, DWORD);
	DwmSetWindowAttributeFunc pSet = (DwmSetWindowAttributeFunc)GetProcAddress(hDwm, "DwmSetWindowAttribute");
	if (pSet != NULL)
	{
		BOOL v = bDark;
		if (pSet(hWnd, 20, &v, sizeof(v)) != S_OK)
			pSet(hWnd, 19, &v, sizeof(v));
	}
	FreeLibrary(hDwm);
}

void FxEnableDarkMode(BOOL bDark)
{
    HMODULE hUx = LoadLibrary(_T("uxtheme.dll"));
    if (hUx == NULL) return;
    typedef int (WINAPI *SetPreferredAppModeFn)(int);
    SetPreferredAppModeFn pSetMode = (SetPreferredAppModeFn)GetProcAddress(hUx, MAKEINTRESOURCEA(135));
    if (pSetMode) pSetMode(bDark ? 1 : 0);
    typedef void (WINAPI *RefreshFn)(); RefreshFn pRefresh = (RefreshFn)GetProcAddress(hUx, MAKEINTRESOURCEA(104)); if (pRefresh) pRefresh();
    typedef void (WINAPI *FlushMenuThemesFn)();
    FlushMenuThemesFn pFlush = (FlushMenuThemesFn)GetProcAddress(hUx, MAKEINTRESOURCEA(136));
    if (pFlush) pFlush();
    FreeLibrary(hUx);
}
static LRESULT CALLBACK FxCheckSubclass(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR uId, DWORD_PTR dwRef)
{
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_MOUSEMOVE) { if (!GetProp(hWnd, _T("fxh"))) { SetProp(hWnd, _T("fxh"), (HANDLE)1); TRACKMOUSEEVENT t; t.cbSize=sizeof(t); t.dwFlags=TME_LEAVE; t.hwndTrack=hWnd; t.dwHoverTime=0; TrackMouseEvent(&t); InvalidateRect(hWnd, NULL, FALSE); } return DefSubclassProc(hWnd, msg, wp, lp); }
    if (msg == WM_MOUSELEAVE) { RemoveProp(hWnd, _T("fxh")); InvalidateRect(hWnd, NULL, FALSE); return DefSubclassProc(hWnd, msg, wp, lp); }
    if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS) { InvalidateRect(hWnd, NULL, FALSE); return DefSubclassProc(hWnd, msg, wp, lp); }
    if (msg != WM_PAINT) return DefSubclassProc(hWnd, msg, wp, lp);
    PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps); RECT rc; GetClientRect(hWnd, &rc);
    HBRUSH hbg = (HBRUSH)::SendMessage(GetParent(hWnd), WM_CTLCOLORSTATIC, (WPARAM)hdc, (LPARAM)hWnd); if (hbg) FillRect(hdc, &rc, hbg);
    LONG st = GetWindowLong(hWnd, GWL_STYLE) & BS_TYPEMASK; BOOL isRadio = (st == BS_RADIOBUTTON || st == BS_AUTORADIOBUTTON); BOOL checked = (::SendMessage(hWnd, BM_GETCHECK, 0, 0) == BST_CHECKED); BOOL hot = (GetProp(hWnd, _T("fxh")) != NULL);
    int h = rc.bottom - rc.top; int sz = h - 8; if (sz < 11) sz = 11; if (sz > 15) sz = 15; int top = rc.top + (h - sz) / 2; int left = rc.left + 1;
    COLORREF fg = FxGetTextColor(); HPEN pen = CreatePen(PS_SOLID, hot ? 2 : 1, fg); HGDIOBJ oldPen = SelectObject(hdc, pen); HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    if (isRadio) { Ellipse(hdc, left, top, left + sz, top + sz); if (checked) { HBRUSH db = CreateSolidBrush(fg); HGDIOBJ ob = SelectObject(hdc, db); Ellipse(hdc, left + 4, top + 4, left + sz - 4, top + sz - 4); SelectObject(hdc, ob); DeleteObject(db); } }
    else { RoundRect(hdc, left, top, left + sz, top + sz, 4, 4); if (checked) { MoveToEx(hdc, left + 3, top + sz / 2, NULL); LineTo(hdc, left + sz / 2 - 1, top + sz - 4); LineTo(hdc, left + sz - 3, top + 3); } }
    SelectObject(hdc, oldPen); SelectObject(hdc, oldBr); DeleteObject(pen);
    WCHAR buf[256]; buf[0] = 0; GetWindowTextW(hWnd, buf, 255);
    RECT tr; tr.left = left + sz + 5; tr.top = rc.top; tr.right = rc.right; tr.bottom = rc.bottom;
    SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, fg);
    HFONT hf = (HFONT)::SendMessage(hWnd, WM_GETFONT, 0, 0); HGDIOBJ oldF = hf ? SelectObject(hdc, hf) : NULL;
    DrawTextW(hdc, buf, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE); if (oldF) SelectObject(hdc, oldF); if (GetFocus() == hWnd) DrawFocusRect(hdc, &tr);
    EndPaint(hWnd, &ps); return 0;
}
static void FxSubclassCheckRadio(HWND hWnd, BOOL bDark)
{
    if (bDark) SetWindowSubclass(hWnd, FxCheckSubclass, 1, 0); else RemoveWindowSubclass(hWnd, FxCheckSubclass, 1);
    InvalidateRect(hWnd, NULL, TRUE);
}
static LRESULT CALLBACK FxRadioTextSubclass(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR uId, DWORD_PTR dwRef)
{
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_MOUSEMOVE) { if (!GetProp(hWnd, _T("fxrh"))) { SetProp(hWnd, _T("fxrh"), (HANDLE)1); TRACKMOUSEEVENT tme; tme.cbSize=sizeof(tme); tme.dwFlags=TME_LEAVE; tme.hwndTrack=hWnd; tme.dwHoverTime=0; TrackMouseEvent(&tme); InvalidateRect(hWnd, NULL, FALSE); } return DefSubclassProc(hWnd, msg, wp, lp); }
    if (msg == WM_MOUSELEAVE) { RemoveProp(hWnd, _T("fxrh")); InvalidateRect(hWnd, NULL, FALSE); return DefSubclassProc(hWnd, msg, wp, lp); }
    if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS) { InvalidateRect(hWnd, NULL, FALSE); return DefSubclassProc(hWnd, msg, wp, lp); }
    if (msg == WM_NCDESTROY) { RemoveWindowSubclass(hWnd, FxRadioTextSubclass, 2); RemoveProp(hWnd, _T("fxrh")); return DefSubclassProc(hWnd, msg, wp, lp); }
    if (msg != WM_PAINT) return DefSubclassProc(hWnd, msg, wp, lp);
    PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps); RECT rc; GetClientRect(hWnd, &rc);
    HBRUSH hbg = (HBRUSH)::SendMessage(GetParent(hWnd), WM_CTLCOLORSTATIC, (WPARAM)hdc, (LPARAM)hWnd); if (hbg) FillRect(hdc, &rc, hbg);
    BOOL enabled = IsWindowEnabled(hWnd); LRESULT chk = ::SendMessage(hWnd, BM_GETCHECK, 0, 0); BOOL hot = (GetProp(hWnd, _T("fxrh")) != NULL);
    LONG bs = GetWindowLong(hWnd, GWL_STYLE) & BS_TYPEMASK; BOOL isRadio = (bs == BS_RADIOBUTTON || bs == BS_AUTORADIOBUTTON); int part = isRadio ? BP_RADIOBUTTON : BP_CHECKBOX;
    int base = (chk == BST_CHECKED) ? 5 : ((chk == BST_INDETERMINATE) ? 9 : 1); int state = base + (!enabled ? 3 : (hot ? 1 : 0));
    SIZE gsz; gsz.cx = 13; gsz.cy = 13;
    HTHEME hT = OpenThemeData(hWnd, L"Button");
    if (hT) { GetThemePartSize(hT, hdc, part, state, NULL, TS_TRUE, &gsz); }
    int gy = rc.top + (rc.bottom - rc.top - gsz.cy) / 2; if (gy < rc.top) gy = rc.top;
    RECT g; g.left = rc.left; g.top = gy; g.right = rc.left + gsz.cx; g.bottom = gy + gsz.cy;
    if (hT) { DrawThemeBackground(hT, hdc, part, state, &g, NULL); CloseThemeData(hT); }
    WCHAR buf[256]; buf[0] = 0; GetWindowTextW(hWnd, buf, 255);
    RECT tr; tr.left = g.right + 5; tr.top = rc.top; tr.right = rc.right; tr.bottom = rc.bottom;
    SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, enabled ? FxGetTextColor() : FxGetSysColor(COLOR_GRAYTEXT));
    HFONT hf = (HFONT)::SendMessage(hWnd, WM_GETFONT, 0, 0); HGDIOBJ oldF = hf ? SelectObject(hdc, hf) : NULL;
    DrawTextW(hdc, buf, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE); if (oldF) SelectObject(hdc, oldF);
    if (GetFocus() == hWnd) { RECT cr = tr; DrawTextW(hdc, buf, -1, &cr, DT_CALCRECT | DT_SINGLELINE); RECT fr = tr; fr.right = tr.left + (cr.right - cr.left) + 2; DrawFocusRect(hdc, &fr); }
    EndPaint(hWnd, &ps); return 0;
}
static void FxSubclassRadio(HWND hWnd, BOOL bDark)
{
    if (bDark) SetWindowSubclass(hWnd, FxRadioTextSubclass, 2, 0); else RemoveWindowSubclass(hWnd, FxRadioTextSubclass, 2);
    InvalidateRect(hWnd, NULL, TRUE);
}
static void FxThemeOneWindow(HWND hWnd, BOOL bDark)
{
    if (bDark) { LONG_PTR fxstrip = WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE; HWND fxpar = GetParent(hWnd); if (fxpar) { WCHAR fxpc[32]; fxpc[0]=0; GetClassNameW(fxpar, fxpc, 31); if (wcscmp(fxpc, L"MDIClient") == 0) fxstrip |= WS_EX_WINDOWEDGE; } LONG_PTR ex = GetWindowLongPtr(hWnd, GWL_EXSTYLE); LONG_PTR ex2 = ex & ~(LONG_PTR)fxstrip; if (ex2 != ex) { SetWindowLongPtr(hWnd, GWL_EXSTYLE, ex2); SetWindowPos(hWnd, NULL, 0,0,0,0, SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE); } }
    
    HMODULE hUx = LoadLibrary(_T("uxtheme.dll"));
    if (hUx == NULL) return;
    typedef bool (WINAPI *AllowDarkModeForWindowFn)(HWND, BOOL);
    AllowDarkModeForWindowFn pAllow = (AllowDarkModeForWindowFn)GetProcAddress(hUx, MAKEINTRESOURCEA(133));
    if (pAllow) pAllow(hWnd, bDark);
    typedef HRESULT (WINAPI *SetWindowThemeFn)(HWND, LPCWSTR, LPCWSTR);
    SetWindowThemeFn pSetTheme = (SetWindowThemeFn)GetProcAddress(hUx, "SetWindowTheme");
    LPCWSTR fxTheme = bDark ? L"DarkMode_Explorer" : NULL; LPCWSTR fxSubId = NULL;
    WCHAR fxcls[64]; fxcls[0]=0; GetClassNameW(hWnd, fxcls, 63);
    BOOL fxIsRadio = FALSE; if (wcscmp(fxcls, L"Button") == 0) { LONG bt = GetWindowLong(hWnd, GWL_STYLE) & BS_TYPEMASK; fxIsRadio = (bt == BS_RADIOBUTTON || bt == BS_AUTORADIOBUTTON || bt == BS_CHECKBOX || bt == BS_AUTOCHECKBOX || bt == BS_3STATE || bt == BS_AUTO3STATE); }
    if (bDark && wcsstr(fxcls, L"ComboBox") != NULL) fxTheme = L"DarkMode_CFD";
    if (pSetTheme) pSetTheme(hWnd, fxTheme, fxSubId);
    if (fxIsRadio) FxSubclassRadio(hWnd, bDark);
    { WCHAR fxc3[16]; fxc3[0]=0; GetClassNameW(hWnd, fxc3, 15); if (bDark && wcscmp(fxc3, L"ComboBox") == 0) { int cs = (int)::SendMessage(hWnd, CB_GETCURSEL, 0, 0); ::SendMessage(hWnd, CB_SETCURSEL, cs, 0); ::InvalidateRect(hWnd, NULL, TRUE); } }
    FreeLibrary(hUx);
}
static BOOL CALLBACK FxDarkChildProc(HWND hChild, LPARAM lParam)
{
    FxThemeOneWindow(hChild, (BOOL)lParam);
    return TRUE;
}
void FxApplyDarkModeTree(HWND hRoot, BOOL bDark)
{
    if (hRoot == NULL) return;
    FxThemeOneWindow(hRoot, bDark);
    EnumChildWindows(hRoot, FxDarkChildProc, (LPARAM)bDark);
    RedrawWindow(hRoot, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_ERASE);
}
