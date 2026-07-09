// PngIconLoader.cpp - alpha-blended PNG toolbar icons for CNewToolBar.
// Decodes per-command PNGs with CxImage and builds an ILC_COLOR32 image list.

#include "stdafx.h"
#include <afxres.h>
#include "resource.h"
#include "ximage.h"
#include "PngIconLoader.h"
#include "ColorHCFR.h"   // GetConfig() / CColorHCFRConfig::ScaleFloor

// Scale a base (96-dpi) icon size to the current DPI, never below the base. One
// place so toolbar + menu icon sizing tracks the rest of the UI (no duplicated
// GetDpiForWindow logic in the toolbar/menu classes).
int HCFR_ScaleIconPx(int basePx, HWND hWnd)
{
  return GetConfig() ? GetConfig()->ScaleFloor(basePx, basePx, hWnd) : basePx;
}

struct HCFR_IconMap { UINT nCmd; LPCTSTR pszFile; };

// Command id -> base filename (without extension). One PNG per command lives in
// res\images\toolbar\{light,dark}\<file>.png. Commands not listed here keep the
// legacy bitmap toolbar (graceful fallback).
static const HCFR_IconMap g_iconMap[] =
{
  // Main toolbar
  { ID_FILE_NEW,                          _T("file-new") },
  { ID_FILE_OPEN,                         _T("file-open") },
  { ID_FILE_SAVE,                         _T("file-save") },
  { ID_EDIT_CUT,                          _T("edit-cut") },
  { ID_EDIT_COPY,                         _T("edit-copy") },
  { ID_EDIT_PASTE,                        _T("edit-paste") },
  { ID_FILE_PRINT,                        _T("file-print") },
  { ID_APP_ABOUT,                         _T("app-about") },
  // Views toolbar
  { IDM_VIEW_DATASET,                     _T("view-dataset") },
  { IDM_VIEW_LUMINANCEHISTO,              _T("view-luminance") },
  { IDM_VIEW_GAMMAHISTO,                  _T("view-gamma") },
  { IDM_VIEW_RGBHISTO,                    _T("view-rgb-levels") },
  { IDM_VIEW_COLORTEMPHISTO,              _T("view-colortemp") },
  { IDM_VIEW_CIECHART,                    _T("view-cie-chart") },
  { IDM_VIEW_NEARBLACKHISTO,              _T("view-near-black") },
  { IDM_VIEW_NEARWHITEHISTO,              _T("view-near-white") },
  { IDM_VIEW_SATLUMHISTO,                 _T("view-saturation") },
  { IDM_VIEW_SATLUMSHIFT,                 _T("view-saturation-shift") },
  { IDM_VIEW_MEASURESCOMBO,               _T("view-summary") },
  { IDM_VIEW_3DCOLOR,                     _T("view-cie-chart") },   // reuses the CIE icon for now
  // Measures toolbar
  { IDM_MEASURE_GRAYSCALE,                _T("measure-grayscale") },
  { IDM_MEASURE_PRIMARIES,                _T("measure-primaries") },
  { IDM_MEASURE_SECONDARIES,              _T("measure-secondaries") },
  { IDM_MEASURE_GRAYSCALE_COLORS,         _T("measure-grayscale-colors") },
  { IDM_SINGLE_MEASUREMENT,               _T("measure-single") },
  { IDM_CONTINUOUS_MEASUREMENT,           _T("measure-continuous") },
  // Measures-Ex toolbar
  { IDM_MEASURE_NEARBLACK,                _T("measure-near-black") },
  { IDM_MEASURE_NEARWHITE,                _T("measure-near-white") },
  { IDM_MEASURE_SAT_PRIMARIES_SECONDARIES, _T("measure-sat-primaries-secondaries") },
  { IDM_MEASURE_SAT_ALL,                  _T("measure-sat-all") },
  { ID_MEASURES_FULLTILTBOOGIE,           _T("measure-complete") },
  // Measures-Sat toolbar
  { IDM_MEASURE_SAT_RED,                  _T("sat-red") },
  { IDM_MEASURE_SAT_GREEN,                _T("sat-green") },
  { IDM_MEASURE_SAT_BLUE,                 _T("sat-blue") },
  { IDM_MEASURE_SAT_YELLOW,               _T("sat-yellow") },
  { IDM_MEASURE_SAT_CYAN,                 _T("sat-cyan") },
  { IDM_MEASURE_SAT_MAGENTA,              _T("sat-magenta") },
  { IDM_MEASURE_SAT_CC24,                 _T("sat-colorchecker") },
  // Menu-only commands (no toolbar button). Sim_* reuse the measure glyphs.
  { IDM_CONFIGURE_SENSOR,                 _T("configure-sensor") },
  { IDM_CONFIGURE_GENERATOR,              _T("configure-generator") },
  { IDM_MEASURE_CONTRAST,                 _T("measure-contrast") },
  { IDM_MEASURE_SAT_PRIMARIES,            _T("measure-sat-primaries-secondaries") },
  { IDM_SIM_GRAYSCALE,                    _T("measure-grayscale") },
  { IDM_SIM_PRIMARIES,                    _T("measure-primaries") },
  { IDM_SIM_SECONDARIES,                  _T("measure-secondaries") },
  { IDM_SIM_GRAYSCALE_AND_COLORS,         _T("measure-grayscale-colors") },
  { IDM_SIM_NEARBLACK,                    _T("measure-near-black") },
  { IDM_SIM_NEARWHITE,                    _T("measure-near-white") },
  { IDM_SIM_SINGLEMEASURE,                _T("measure-single") },
  { IDM_SIM_SAT_RED,                      _T("sat-red") },
  { IDM_SIM_SAT_GREEN,                    _T("sat-green") },
  { IDM_SIM_SAT_BLUE,                     _T("sat-blue") },
  { IDM_SIM_SAT_YELLOW,                   _T("sat-yellow") },
  { IDM_SIM_SAT_CYAN,                     _T("sat-cyan") },
  { IDM_SIM_SAT_MAGENTA,                  _T("sat-magenta") },
};

// Menu icon order: every command that should show a menu icon. Built into the
// menu image list in this order; m_IDs is filled to match.
static const UINT g_menuOrder[] =
{
  ID_FILE_NEW, ID_FILE_OPEN, ID_FILE_SAVE, ID_EDIT_CUT, ID_EDIT_COPY, ID_EDIT_PASTE,
  ID_FILE_PRINT, ID_APP_ABOUT,
  IDM_VIEW_DATASET, IDM_VIEW_LUMINANCEHISTO, IDM_VIEW_GAMMAHISTO, IDM_VIEW_RGBHISTO,
  IDM_VIEW_COLORTEMPHISTO, IDM_VIEW_CIECHART, IDM_VIEW_NEARBLACKHISTO, IDM_VIEW_NEARWHITEHISTO,
  IDM_VIEW_SATLUMHISTO, IDM_VIEW_SATLUMSHIFT, IDM_VIEW_MEASURESCOMBO, IDM_VIEW_3DCOLOR,
  IDM_CONFIGURE_SENSOR, IDM_CONFIGURE_GENERATOR,
  IDM_MEASURE_GRAYSCALE, IDM_MEASURE_PRIMARIES, IDM_MEASURE_SECONDARIES, IDM_MEASURE_GRAYSCALE_COLORS,
  IDM_MEASURE_NEARBLACK, IDM_MEASURE_NEARWHITE, IDM_SINGLE_MEASUREMENT, IDM_CONTINUOUS_MEASUREMENT,
  IDM_SIM_GRAYSCALE, IDM_SIM_PRIMARIES, IDM_SIM_SECONDARIES, IDM_SIM_GRAYSCALE_AND_COLORS,
  IDM_SIM_NEARBLACK, IDM_SIM_NEARWHITE, IDM_SIM_SINGLEMEASURE,
  IDM_MEASURE_SAT_RED, IDM_MEASURE_SAT_GREEN, IDM_MEASURE_SAT_BLUE, IDM_MEASURE_SAT_YELLOW,
  IDM_MEASURE_SAT_CYAN, IDM_MEASURE_SAT_MAGENTA, IDM_MEASURE_SAT_CC24,
  IDM_MEASURE_SAT_PRIMARIES, IDM_MEASURE_SAT_PRIMARIES_SECONDARIES, IDM_MEASURE_SAT_ALL,
  IDM_SIM_SAT_RED, IDM_SIM_SAT_GREEN, IDM_SIM_SAT_BLUE, IDM_SIM_SAT_YELLOW,
  IDM_SIM_SAT_CYAN, IDM_SIM_SAT_MAGENTA,
  IDM_MEASURE_CONTRAST, ID_MEASURES_FULLTILTBOOGIE,
};

static CString ExeDir()
{
  TCHAR buf[MAX_PATH];
  buf[0] = 0;
  ::GetModuleFileName(NULL, buf, MAX_PATH);
  CString s(buf);
  int i = s.ReverseFind(_T('\\'));
  if (i >= 0)
    s = s.Left(i + 1);
  return s;
}

static bool FileThere(const CString& p)
{
  DWORD a = ::GetFileAttributes(p);
  return (a != INVALID_FILE_ATTRIBUTES) && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static CString IconFileName(UINT nCmd)
{
  for (int i = 0; i < (int)(sizeof(g_iconMap) / sizeof(g_iconMap[0])); i++)
    if (g_iconMap[i].nCmd == nCmd)
      return CString(g_iconMap[i].pszFile);
  return CString();
}

// Decode one PNG and premultiply it (alpha scaled by alphaPct) into cell
// cellIndex of a 32bpp top-down BGRA strip.
static void BlitPngCell(BYTE* pBase, int stride, int cellIndex, int cx, int cy, const CString& file, int alphaPct)
{
  if (file.IsEmpty())
    return;
  CxImage img;
  img.Load((LPCTSTR)file, CXIMAGE_FORMAT_PNG);
  if (!img.IsValid())
    return;
  if ((int)img.GetWidth() != cx || (int)img.GetHeight() != cy)
    img.Resample2(cx, cy, CxImage::IM_BICUBIC2, CxImage::OM_TRANSPARENT);   // area-averaged scale, transparent edge overflow
  int iw = (int)img.GetWidth();
  int ih = (int)img.GetHeight();
  bool hasAlpha = img.AlphaIsValid();
  int ox = (cx - iw) / 2;
  int oy = (cy - ih) / 2;
  int cellX = cellIndex * cx;
  for (int vy = 0; vy < ih; vy++)
  {
    int dy = oy + vy;
    if (dy < 0 || dy >= cy)
      continue;
    int sy = ih - 1 - vy;
    BYTE* pRow = pBase + (size_t)dy * stride;
    for (int x = 0; x < iw; x++)
    {
      int dx = ox + x;
      if (dx < 0 || dx >= cx)
        continue;
      RGBQUAD c = img.GetPixelColor(x, sy, false);
      int a = hasAlpha ? img.AlphaGet(x, sy) : 255;
      a = a * alphaPct / 100;
      BYTE* p = pRow + (size_t)(cellX + dx) * 4;
      p[0] = (BYTE)((c.rgbBlue  * a) / 255);
      p[1] = (BYTE)((c.rgbGreen * a) / 255);
      p[2] = (BYTE)((c.rgbRed   * a) / 255);
      p[3] = (BYTE)a;
    }
  }
}

// Create a 32bpp top-down DIB section of (cells * cx) x cy, zero-filled.
static HBITMAP CreateStripDib(int cells, int cx, int cy, void** ppBits)
{
  BITMAPINFO bi;
  ZeroMemory(&bi, sizeof(bi));
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = cx * cells;
  bi.bmiHeader.biHeight = -cy;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  *ppBits = NULL;
  HDC hdc = ::GetDC(NULL);
  HBITMAP hbm = ::CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, ppBits, NULL, 0);
  ::ReleaseDC(NULL, hdc);
  if (hbm && *ppBits)
    ZeroMemory(*ppBits, (size_t)cx * cells * 4 * cy);
  return hbm;
}

// High-DPI variants. A glyph ships as an unsuffixed 1x base plus larger
// masters (1.5x/2x/3x). The resolver picks the smallest variant >= the
// requested scale and the caller resamples it to the exact cell size, so we
// downscale (crisp) and never upscale within the available range.
struct HCFR_FactorDef { double mult; LPCTSTR suffix; };
static const HCFR_FactorDef g_factors[] =
{
  { 1.0, _T("")     },   // unsuffixed base
  { 1.25, _T("1.25x") }, // exact match at 125% DPI (add res\images\<set>\<theme>\<name>@1.25x.png)
  { 1.5, _T("1.5x") },
  { 2.0, _T("2x")   },
  { 3.0, _T("3x")   },
};

// Build the path for one set/theme/name/suffix. Tolerates the '@' separator
// being present or absent (some exports drop it on the 1.5x files).
static CString BuildIconPath(LPCTSTR pszSet, bool bDark, LPCTSTR pszName, LPCTSTR pszSuffix)
{
  CString base = ExeDir() + _T("res\\images\\") + pszSet + _T("\\")
               + (bDark ? _T("dark\\") : _T("light\\"));
  if (pszSuffix == NULL || pszSuffix[0] == 0)
  {
    CString p = base + pszName + _T(".png");
    return FileThere(p) ? p : CString();
  }
  CString withAt = base + pszName + _T("@") + pszSuffix + _T(".png");
  if (FileThere(withAt))
    return withAt;
  CString plain = base + pszName + pszSuffix + _T(".png");
  if (FileThere(plain))
    return plain;
  return CString();
}

// Best existing variant for one theme: smallest factor >= want first (downscale
// to the cell), then any larger, then fall back to smaller (last resort upscale).
static CString ResolveForTheme(LPCTSTR pszSet, bool bDark, LPCTSTR pszName, double want)
{
  const int n = (int)(sizeof(g_factors) / sizeof(g_factors[0]));
  int start = -1;
  for (int i = 0; i < n; i++)
    if (g_factors[i].mult + 1e-6 >= want) { start = i; break; }

  if (start >= 0)
  {
    for (int i = start; i < n; i++)
    {
      CString p = BuildIconPath(pszSet, bDark, pszName, g_factors[i].suffix);
      if (!p.IsEmpty()) return p;
    }
    for (int i = start - 1; i >= 0; i--)
    {
      CString p = BuildIconPath(pszSet, bDark, pszName, g_factors[i].suffix);
      if (!p.IsEmpty()) return p;
    }
  }
  else
  {
    for (int i = n - 1; i >= 0; i--)
    {
      CString p = BuildIconPath(pszSet, bDark, pszName, g_factors[i].suffix);
      if (!p.IsEmpty()) return p;
    }
  }
  return CString();
}

// Resolve a themed icon for a target cell size. basePx is the set's 1x pixel
// size (32 toolbar, 16 menu). Prefers the active theme, falls back to the other.
static CString ResolveBySize(LPCTSTR pszSet, bool bDark, LPCTSTR pszName, int targetPx, int basePx)
{
  if (pszName == NULL || pszName[0] == 0)
    return CString();
  double want = (basePx > 0) ? (double)targetPx / (double)basePx : 1.0;
  CString p = ResolveForTheme(pszSet, bDark, pszName, want);
  if (p.IsEmpty())
    p = ResolveForTheme(pszSet, !bDark, pszName, want);
  return p;
}

CString HCFR_ResolveToolbarIcon(UINT nCmdId, bool bDark, int targetPx)
{
  return ResolveBySize(_T("toolbar"), bDark, IconFileName(nCmdId), targetPx, 32);
}

CString HCFR_ResolveToolbarIconByName(LPCTSTR pszName, bool bDark, int targetPx)
{
  return ResolveBySize(_T("toolbar"), bDark, pszName, targetPx, 32);
}

HICON HCFR_LoadPngHIcon(LPCTSTR pszSet, LPCTSTR pszName, bool bDark, int w, int h)
{
  if (w <= 0 || h <= 0)
    return NULL;

  int basePx = (lstrcmpi(pszSet, _T("menu")) == 0) ? 16 : 32;
  CString path = ResolveBySize(pszSet, bDark, pszName, w, basePx);
  if (path.IsEmpty())
    return NULL;

  CxImage img;
  img.Load((LPCTSTR)path, CXIMAGE_FORMAT_PNG);
  if (!img.IsValid())
    return NULL;
  if ((int)img.GetWidth() != w || (int)img.GetHeight() != h)
    img.Resample2(w, h, CxImage::IM_BICUBIC2, CxImage::OM_TRANSPARENT);
  bool hasAlpha = img.AlphaIsValid();

  // 32bpp straight-alpha top-down color DIB for the icon
  BITMAPV5HEADER bi;
  ZeroMemory(&bi, sizeof(bi));
  bi.bV5Size = sizeof(BITMAPV5HEADER);
  bi.bV5Width = w;
  bi.bV5Height = -h;
  bi.bV5Planes = 1;
  bi.bV5BitCount = 32;
  bi.bV5Compression = BI_BITFIELDS;
  bi.bV5RedMask   = 0x00FF0000;
  bi.bV5GreenMask = 0x0000FF00;
  bi.bV5BlueMask  = 0x000000FF;
  bi.bV5AlphaMask = 0xFF000000;

  void* pBits = NULL;
  HDC hdc = ::GetDC(NULL);
  HBITMAP hColor = ::CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &pBits, NULL, 0);
  ::ReleaseDC(NULL, hdc);
  if (!hColor || !pBits)
  {
    if (hColor) ::DeleteObject(hColor);
    return NULL;
  }

  BYTE* pBase = (BYTE*)pBits;
  for (int vy = 0; vy < h; vy++)
  {
    int sy = h - 1 - vy;
    BYTE* pRow = pBase + (size_t)vy * w * 4;
    for (int x = 0; x < w; x++)
    {
      RGBQUAD c = img.GetPixelColor(x, sy, false);
      BYTE a = hasAlpha ? img.AlphaGet(x, sy) : 255;
      BYTE* q = pRow + (size_t)x * 4;
      q[0] = c.rgbBlue;   // straight (non-premultiplied) BGRA for icons
      q[1] = c.rgbGreen;
      q[2] = c.rgbRed;
      q[3] = a;
    }
  }

  int maskBytes = ((w + 15) / 16) * 2 * h;
  BYTE* pMask = new BYTE[maskBytes];
  ZeroMemory(pMask, maskBytes);           // all-0 mask -> opaque, alpha does the work
  HBITMAP hMask = ::CreateBitmap(w, h, 1, 1, pMask);
  delete[] pMask;

  ICONINFO ii;
  ZeroMemory(&ii, sizeof(ii));
  ii.fIcon = TRUE;
  ii.hbmColor = hColor;
  ii.hbmMask = hMask;
  HICON hIcon = ::CreateIconIndirect(&ii);

  ::DeleteObject(hColor);
  ::DeleteObject(hMask);
  return hIcon;
}

HIMAGELIST HCFR_BuildMenuIconList(bool bDark, int cx, int cy, CArray<UINT, UINT&>& outIds)
{
  outIds.RemoveAll();
  CStringArray files;
  for (int i = 0; i < (int)(sizeof(g_menuOrder) / sizeof(g_menuOrder[0])); i++)
  {
    UINT nId = g_menuOrder[i];
    CString f = ResolveBySize(_T("menu"), bDark, IconFileName(nId), cx, 16);
    if (f.IsEmpty())
      continue;
    files.Add(f);
    outIds.Add(nId);
  }
  int n = (int)files.GetSize();
  if (n <= 0 || cx <= 0 || cy <= 0)
    return NULL;

  // CNewMenu expects MENU_ICONS(=3) images per command: normal, hot, disabled.
  int cells = n * 3;
  void* pBits = NULL;
  HBITMAP hbm = CreateStripDib(cells, cx, cy, &pBits);
  if (!hbm || !pBits)
  {
    if (hbm) ::DeleteObject(hbm);
    return NULL;
  }
  int stride = cx * cells * 4;
  for (int k = 0; k < n; k++)
  {
    BlitPngCell((BYTE*)pBits, stride, k * 3 + 0, cx, cy, files[k], 100);
    BlitPngCell((BYTE*)pBits, stride, k * 3 + 1, cx, cy, files[k], 100);
    BlitPngCell((BYTE*)pBits, stride, k * 3 + 2, cx, cy, files[k], 42);
  }
  HIMAGELIST himl = ImageList_Create(cx, cy, ILC_COLOR32, cells, 1);
  if (himl)
    ImageList_Add(himl, hbm, NULL);
  ::DeleteObject(hbm);
  return himl;
}

HIMAGELIST HCFR_BuildPngImageList(const CStringArray& files, int cx, int cy, bool bDisabled)
{
  int n = (int)files.GetSize();
  if (n <= 0 || cx <= 0 || cy <= 0)
    return NULL;

  BITMAPINFO bi;
  ZeroMemory(&bi, sizeof(bi));
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = cx * n;
  bi.bmiHeader.biHeight = -cy;          // top-down
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;

  void* pBits = NULL;
  HDC hdc = ::GetDC(NULL);
  HBITMAP hbm = ::CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &pBits, NULL, 0);
  ::ReleaseDC(NULL, hdc);
  if (!hbm || !pBits)
  {
    if (hbm) ::DeleteObject(hbm);
    return NULL;
  }

  int stride = cx * n * 4;
  ZeroMemory(pBits, (size_t)stride * cy);   // fully transparent
  BYTE* pBase = (BYTE*)pBits;

  for (int i = 0; i < n; i++)
  {
    CString f = files[i];
    if (f.IsEmpty())
      continue;

    CxImage img;
    img.Load((LPCTSTR)f, CXIMAGE_FORMAT_PNG);
    if (!img.IsValid())
      continue;
    if ((int)img.GetWidth() != cx || (int)img.GetHeight() != cy)
      img.Resample2(cx, cy, CxImage::IM_BICUBIC2, CxImage::OM_TRANSPARENT);   // area-averaged scale, transparent edge overflow

    int iw = (int)img.GetWidth();
    int ih = (int)img.GetHeight();
    bool hasAlpha = img.AlphaIsValid();
    int ox = (cx - iw) / 2;
    int oy = (cy - ih) / 2;
    int cellX = i * cx;

    for (int vy = 0; vy < ih; vy++)
    {
      int dy = oy + vy;
      if (dy < 0 || dy >= cy)
        continue;
      int sy = ih - 1 - vy;             // CxImage is bottom-up
      BYTE* pRow = pBase + (size_t)dy * stride;

      for (int x = 0; x < iw; x++)
      {
        int dx = ox + x;
        if (dx < 0 || dx >= cx)
          continue;

        RGBQUAD c = img.GetPixelColor(x, sy, false);
        BYTE a = hasAlpha ? img.AlphaGet(x, sy) : 255;
        if (bDisabled)
          a = (BYTE)((a * 42) / 100);

        BYTE* p = pRow + (size_t)(cellX + dx) * 4;
        p[0] = (BYTE)((c.rgbBlue  * a) / 255);   // premultiplied BGRA
        p[1] = (BYTE)((c.rgbGreen * a) / 255);
        p[2] = (BYTE)((c.rgbRed   * a) / 255);
        p[3] = a;
      }
    }
  }

  HIMAGELIST himl = ImageList_Create(cx, cy, ILC_COLOR32, n, 1);
  if (himl)
    ImageList_Add(himl, hbm, NULL);
  ::DeleteObject(hbm);
  return himl;
}
