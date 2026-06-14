#ifndef HCFR_PNG_ICON_LOADER_H
#define HCFR_PNG_ICON_LOADER_H
#pragma once

#include <afxtempl.h>

// Toolbar PNG icon support (alpha-blended, Light/Dark themed).
// Icons live on disk next to the exe, under res\images\toolbar\{light,dark}\,
// one 32x32 PNG per command, named by command (see g_iconMap in the .cpp).

// Resolve the icon file for a command id and theme. Returns the full path to an
// existing PNG, preferring the active theme folder and falling back to the other.
// Returns an empty CString when the command has no mapping or no file exists.
CString HCFR_ResolveToolbarIcon(UINT nCmdId, bool bDark);

// Load a themed PNG (res\images\<set>\<theme>\<name>.png, with fallback to the
// other theme) as an alpha HICON scaled to w x h. Returns NULL if not found.
// Caller passes it to CButtonST::SetIcon, which takes ownership.
HICON HCFR_LoadPngHIcon(LPCTSTR pszSet, LPCTSTR pszName, bool bDark, int w, int h);

// Build a 32bpp premultiplied-alpha image list (ILC_COLOR32) from the given full
// PNG paths, one cell per entry sized cx*cy. When bDisabled is true the icons are
// faded for the disabled state. Returns NULL on failure. Caller owns the handle.
HIMAGELIST HCFR_BuildPngImageList(const CStringArray& files, int cx, int cy, bool bDisabled);

// Build the menu icon image list (ILC_COLOR32) for the active theme and fill
// outIds with the matching command ids (parallel to image order). Returns NULL
// if no icons resolved. Used to drive CNewMenuIcons (m_IconsList + m_IDs).
HIMAGELIST HCFR_BuildMenuIconList(bool bDark, int cx, int cy, CArray<UINT, UINT&>& outIds);

#endif // HCFR_PNG_ICON_LOADER_H
