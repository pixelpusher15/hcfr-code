// HcfrUpdateUI.cpp : see HcfrUpdateUI.h.
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "HcfrUpdate.h"
#include "HcfrUpdateUI.h"

#include <commctrl.h>   // TaskDialogIndirect
#include <shlobj.h>     // IProgressDialog
#include <process.h>    // _beginthreadex

#pragma comment(lib, "comctl32")
#pragma comment(lib, "ole32")

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

// TaskDialog command-button IDs (kept clear of IDOK/IDCANCEL).
enum { BTN_DOWNLOAD = 1001, BTN_SKIP = 1002, BTN_LATER = 1003 };

// Load a localized WIDE string from the active language DLL. The resource-free
// TaskDialog / IProgressDialog need PCWSTR; string-table resources are always
// UTF-16 in the binary, so LoadStringW returns correctly-decoded text
// regardless of the app's MBCS build. (cchBufferMax 0 -> read-only resource ptr.)
static CStringW LoadW(UINT id)
{
    LPWSTR p = NULL;
    int len = ::LoadStringW(AfxGetResourceHandle(), id, (LPWSTR)&p, 0);
    return len > 0 ? CStringW(p, len) : CStringW();
}

// Shared state between the UI thread and the download worker thread. The
// download runs off the UI thread so the main window stays responsive; the UI
// thread owns the (apartment-threaded) progress dialog and reads counters here.
struct DownloadState
{
    CString       url;
    CString       dest;
    volatile LONG done;    // bytes so far (progress only; 32-bit for atomic reads on x86)
    volatile LONG total;   // -1 until known
    volatile LONG cancel;  // set by UI thread when user cancels
    bool          ok;      // written by worker before it exits
    CString       error;
};

// Runs on the worker thread; forwards cancel/progress to/from DownloadState.
// done/total are stored as 32-bit LONG so the UI thread's reads are atomic on
// x86 (a 64-bit volatile load there is two ops and can tear). Installers are
// well under 2 GiB, so the narrowing is harmless.
static bool DownloadWorkerProgress(void* ctx, unsigned __int64 done, __int64 total)
{
    DownloadState* s = (DownloadState*) ctx;
    s->done  = (LONG) done;
    s->total = (LONG) total;
    return ::InterlockedCompareExchange(&s->cancel, 0, 0) == 0;
}

static unsigned __stdcall DownloadWorker(void* p)
{
    DownloadState* s = (DownloadState*) p;
    CHcfrUpdate upd;
    s->ok = upd.DownloadFile(s->url, s->dest, DownloadWorkerProgress, s);
    if (!s->ok)
        s->error = upd.GetLastErrorText();
    return 0;
}

// Guards against starting a second download re-entrantly (the message pump in
// DownloadAndInstall dispatches input, so the user could trigger another one).
static bool s_downloadInProgress = false;
struct DownloadBusyGuard
{
    DownloadBusyGuard()  { s_downloadInProgress = true; }
    ~DownloadBusyGuard() { s_downloadInProgress = false; }
};

// Download the installer asset (off-thread, with a progress dialog), then offer
// to run it.
static void DownloadAndInstall(CWnd* pParent, const HcfrUpdateInfo& info)
{
    if (info.assetUrl.IsEmpty())
    {
        CString m; m.LoadString(IDS_UPDLG_NO_INSTALLER);
        AfxMessageBox(m, MB_OK | MB_ICONWARNING);
        return;
    }
    if (s_downloadInProgress)
    {
        CString m; m.LoadString(IDS_UPDLG_IN_PROGRESS);
        AfxMessageBox(m, MB_OK | MB_ICONINFORMATION);
        return;
    }
    DownloadBusyGuard busy;

    // Destination in the per-user temp directory.
    char tmpDir[MAX_PATH] = {0};
    DWORD tlen = GetTempPathA(MAX_PATH, tmpDir);
    if (tlen == 0 || tlen >= MAX_PATH)
    {
        CString m; m.LoadString(IDS_UPDLG_NO_TEMP);
        AfxMessageBox(m, MB_OK | MB_ICONWARNING);
        return;
    }
    // Use only the file-name part of the server-supplied asset name so a crafted
    // name (e.g. containing "..\\") cannot redirect the write out of the temp dir.
    CString safeName = info.assetName;
    int sep = safeName.ReverseFind('\\');
    if (sep >= 0) safeName = safeName.Mid(sep + 1);
    sep = safeName.ReverseFind('/');
    if (sep >= 0) safeName = safeName.Mid(sep + 1);
    if (safeName.IsEmpty())
        safeName = _T("HCFRSetup.exe");
    CString dest = tmpDir;
    dest += safeName;

    DownloadState st;
    st.url    = info.assetUrl;
    st.dest   = dest;
    st.done   = 0;
    st.total  = -1;
    st.cancel = 0;
    st.ok     = false;

    HRESULT hrCo = CoInitialize(NULL);

    IProgressDialog* pd = NULL;
    CoCreateInstance(CLSID_ProgressDialog, NULL, CLSCTX_INPROC_SERVER,
                     IID_IProgressDialog, (void**)&pd);
    if (pd)
    {
        pd->SetTitle(LoadW(IDS_UPDLG_DOWNLOADING_TITLE));
        CStringW verW(info.version), line;
        line.Format(LoadW(IDS_UPDLG_DOWNLOADING_LINE), (LPCWSTR)verW);
        pd->SetLine(1, line, FALSE, NULL);
        pd->StartProgressDialog(pParent ? pParent->GetSafeHwnd() : NULL, NULL,
                                PROGDLG_AUTOTIME | PROGDLG_NOMINIMIZE, NULL);
    }

    unsigned tid = 0;
    HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, DownloadWorker, &st, 0, &tid);
    if (!hThread)
    {
        if (pd) { pd->StopProgressDialog(); pd->Release(); }
        if (SUCCEEDED(hrCo)) CoUninitialize();
        CString m; m.LoadString(IDS_UPDLG_CANT_START);
        AfxMessageBox(m, MB_OK | MB_ICONWARNING);
        return;
    }

    // Pump messages while the worker downloads so the app stays responsive and
    // the progress dialog updates.
    bool quitting = false;
    for (;;)
    {
        DWORD w = ::MsgWaitForMultipleObjects(1, &hThread, FALSE, 100, QS_ALLINPUT);

        MSG msg;
        while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                // App is shutting down: cancel the download, re-post the quit so
                // the outer message loop still sees it, and stop pumping.
                ::InterlockedExchange(&st.cancel, 1);
                ::PostQuitMessage((int) msg.wParam);
                quitting = true;
                break;
            }
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
        if (pd)
        {
            if (pd->HasUserCancelled())
                ::InterlockedExchange(&st.cancel, 1);
            if (st.total > 0)
                pd->SetProgress64((ULONGLONG)(ULONG)st.done, (ULONGLONG)(ULONG)st.total);
        }
        // Break on worker completion, on shutdown, or on a wait failure (a bad
        // handle must not spin the loop forever).
        if (quitting || w == WAIT_OBJECT_0 || w == WAIT_FAILED)
            break;
    }
    ::WaitForSingleObject(hThread, INFINITE);
    ::CloseHandle(hThread);

    bool cancelled = ::InterlockedCompareExchange(&st.cancel, 0, 0) != 0;
    if (pd) { pd->StopProgressDialog(); pd->Release(); pd = NULL; }
    if (SUCCEEDED(hrCo)) CoUninitialize();

    if (!st.ok)
    {
        if (!cancelled)
        {
            CString fmt; fmt.LoadString(IDS_UPDLG_DOWNLOAD_FAILED);
            CString m; m.Format(fmt, (LPCSTR)st.error);
            AfxMessageBox(m, MB_OK | MB_ICONWARNING);
        }
        return;   // cancelled -> stay quiet
    }

    CString askInstall; askInstall.LoadString(IDS_UPDLG_DOWNLOAD_OK);
    if (AfxMessageBox(askInstall, MB_OKCANCEL | MB_ICONINFORMATION) == IDOK)
    {
        // Only close the app if the installer actually launched (ShellExecute
        // returns <=32 on failure) - otherwise the user would be left with a
        // closing app and no installer.
        HINSTANCE hInst = ShellExecute(NULL, _T("open"), dest, NULL, NULL, SW_SHOWNORMAL);
        if ((INT_PTR)hInst <= 32)
        {
            CString fmt; fmt.LoadString(IDS_UPDLG_CANT_LAUNCH);
            CString m; m.Format(fmt, (LPCSTR)dest);
            AfxMessageBox(m, MB_OK | MB_ICONWARNING);
            return;
        }
        if (AfxGetMainWnd())
            AfxGetMainWnd()->PostMessage(WM_CLOSE);
    }
}

// ---------------------------------------------------------------------------
// notification dialog
// ---------------------------------------------------------------------------

void HcfrUpdate_ShowAvailable(CWnd* pParent, const HcfrUpdateInfo& info)
{
    CString running = CHcfrUpdate::GetRunningVersion();

    // Localized strings, kept alive for the whole TaskDialogIndirect call.
    CStringW sTitle   = LoadW(IDS_UPDLG_TITLE);
    CStringW sInstr   = LoadW(IDS_UPDLG_AVAILABLE);
    CStringW sBtnDl   = LoadW(IDS_UPDLG_BTN_DOWNLOAD);
    CStringW sBtnSkip = LoadW(IDS_UPDLG_BTN_SKIP);
    CStringW sBtnLtr  = LoadW(IDS_UPDLG_BTN_LATER);
    CStringW sHide    = LoadW(IDS_UPDLG_HIDE_NOTES);
    CStringW sShow    = LoadW(IDS_UPDLG_SHOW_NOTES);
    CStringW sAuto    = LoadW(IDS_UPDLG_AUTOCHECK);

    CStringW runningW(running), verW(info.version);
    CStringW suffixW = info.isPrerelease ? LoadW(IDS_UPDLG_PRERELEASE) : CStringW();
    CStringW wContent;
    wContent.Format(LoadW(IDS_UPDLG_CONTENT), (LPCWSTR)runningW, (LPCWSTR)verW, (LPCWSTR)suffixW);

    CStringW wNotes(info.notes);
    if (wNotes.GetLength() > 2000)
        wNotes = wNotes.Left(2000) + L"\r\n...";

    TASKDIALOG_BUTTON btns[3];
    btns[0].nButtonID = BTN_DOWNLOAD; btns[0].pszButtonText = sBtnDl;
    btns[1].nButtonID = BTN_SKIP;     btns[1].pszButtonText = sBtnSkip;
    btns[2].nButtonID = BTN_LATER;    btns[2].pszButtonText = sBtnLtr;

    TASKDIALOGCONFIG tc;
    ZeroMemory(&tc, sizeof(tc));
    tc.cbSize = sizeof(tc);
    tc.hwndParent = pParent ? pParent->GetSafeHwnd() : NULL;
    tc.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ALLOW_DIALOG_CANCELLATION
               | TDF_EXPAND_FOOTER_AREA | TDF_POSITION_RELATIVE_TO_WINDOW;
    tc.pszWindowTitle     = sTitle;
    tc.pszMainIcon        = TD_INFORMATION_ICON;
    tc.pszMainInstruction = sInstr;
    tc.pszContent         = wContent;
    if (!wNotes.IsEmpty())
    {
        tc.pszExpandedInformation = wNotes;
        tc.pszExpandedControlText = sHide;
        tc.pszCollapsedControlText = sShow;
    }
    tc.pButtons       = btns;
    tc.cButtons       = 3;
    tc.nDefaultButton = BTN_DOWNLOAD;

    BOOL autoCheck = GetConfig()->m_doUpdateCheck;
    tc.pszVerificationText = sAuto;
    if (autoCheck)
        tc.dwFlags |= TDF_VERIFICATION_FLAG_CHECKED;

    int  nButton = 0;
    BOOL bVerify = FALSE;
    if (FAILED(TaskDialogIndirect(&tc, &nButton, NULL, &bVerify)))
        return;

    // Apply the "check automatically" toggle if the user changed it.
    if ((bVerify != 0) != (GetConfig()->m_doUpdateCheck != 0))
    {
        GetConfig()->m_doUpdateCheck = bVerify ? TRUE : FALSE;
        GetConfig()->SaveSettings();
    }

    if (nButton == BTN_DOWNLOAD)
    {
        DownloadAndInstall(pParent, info);
    }
    else if (nButton == BTN_SKIP)
    {
        GetConfig()->m_updateSkipVersion = info.version;
        GetConfig()->SaveSettings();
    }
    // BTN_LATER / cancel: do nothing - we'll check again next time.
}

// ---------------------------------------------------------------------------
// manual check (menu)
// ---------------------------------------------------------------------------

void HcfrUpdate_CheckInteractive(CWnd* pParent, int ring)
{
    CWaitCursor wait;

    CHcfrUpdate upd;
    HcfrUpdateInfo info;
    int r = upd.QueryLatest(ring, info);

    if (r < 0)
    {
        CString fmt; fmt.LoadString(IDS_UPDLG_CHECK_FAILED);
        CString m; m.Format(fmt, (LPCSTR)upd.GetLastErrorText());
        AfxMessageBox(m, MB_OK | MB_ICONWARNING);
        return;
    }

    CString running = CHcfrUpdate::GetRunningVersion();
    if (r == 1 && CHcfrUpdate::IsNewer(info.tag, running))
    {
        HcfrUpdate_ShowAvailable(pParent, info);
    }
    else
    {
        CStringW runningW(running), content;
        content.Format(LoadW(IDS_UPDLG_UPTODATE_CONTENT), (LPCWSTR)runningW);
        CStringW title = LoadW(IDS_UPDLG_TITLE), instr = LoadW(IDS_UPDLG_UPTODATE);
        TaskDialog(pParent ? pParent->GetSafeHwnd() : NULL, NULL,
                   title, instr, content,
                   TDCBF_OK_BUTTON, TD_INFORMATION_ICON, NULL);
    }
}

// ---------------------------------------------------------------------------
// background check
// ---------------------------------------------------------------------------

struct BgCheckCtx
{
    HWND    hWnd;
    int     ring;
    CString skip;
};

static unsigned __stdcall BgCheckThread(void* p)
{
    BgCheckCtx* c = (BgCheckCtx*) p;

    CHcfrUpdate upd;
    HcfrUpdateInfo info;
    int r = upd.QueryLatest(c->ring, info);

    if (r == 1)
    {
        CString running = CHcfrUpdate::GetRunningVersion();
        bool newer   = CHcfrUpdate::IsNewer(info.tag, running);
        bool skipped = (!c->skip.IsEmpty() && c->skip.CompareNoCase(info.version) == 0);
        if (newer && !skipped && ::IsWindow(c->hWnd))
        {
            HcfrUpdateInfo* pInfo = new HcfrUpdateInfo(info);
            if (!::PostMessage(c->hWnd, WM_HCFR_UPDATE_AVAILABLE, 0, (LPARAM)pInfo))
                delete pInfo;
        }
    }

    delete c;
    return 0;
}

void HcfrUpdate_StartBackgroundCheck(HWND hNotifyWnd, int ring,
                                     const CString& skipVersion)
{
    BgCheckCtx* c = new BgCheckCtx;
    c->hWnd = hNotifyWnd;
    c->ring = ring;
    c->skip = skipVersion;

    unsigned tid = 0;
    HANDLE h = (HANDLE)_beginthreadex(NULL, 0, BgCheckThread, c, 0, &tid);
    if (h)
        CloseHandle(h);   // fire and forget
    else
        delete c;
}
