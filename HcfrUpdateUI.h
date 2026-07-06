// HcfrUpdateUI.h : user-facing glue for the GitHub update checker.
//
// Wraps CHcfrUpdate with MFC/TaskDialog UI: a non-blocking background check
// used at startup, a synchronous "Check for updates" entry point for the menu,
// and the notification/download/install flow. Uses native TaskDialog and the
// shell IProgressDialog, so it needs no dialog resources (important: this app's
// dialogs live in per-language satellite DLLs).
//
#ifndef _HCFRUPDATEUI_H
#define _HCFRUPDATEUI_H

#include "HcfrUpdate.h"

// Posted to the notify window when the background check finds a newer release.
// lParam is a heap-allocated HcfrUpdateInfo* that the handler must delete.
#define WM_HCFR_UPDATE_AVAILABLE (WM_APP + 137)

// Start a non-blocking background check. Respects the ring and skip-version.
// If a newer, non-skipped release is found, posts WM_HCFR_UPDATE_AVAILABLE to
// hNotifyWnd. Safe to call during startup; returns immediately.
void HcfrUpdate_StartBackgroundCheck(HWND hNotifyWnd, int ring,
                                     const CString& skipVersion);

// Show the "update available" TaskDialog for 'info' and carry out the chosen
// action (download & install / skip this version / later). Also honors the
// "check automatically" verification checkbox.
void HcfrUpdate_ShowAvailable(CWnd* pParent, const HcfrUpdateInfo& info);

// Synchronous "Check for updates" for the menu command: queries with a wait
// cursor, then shows either the update dialog or an "up to date" message.
// Ignores the skip-version (an explicit check should always report).
void HcfrUpdate_CheckInteractive(CWnd* pParent, int ring);

#endif // _HCFRUPDATEUI_H
