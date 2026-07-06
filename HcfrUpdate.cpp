// HcfrUpdate.cpp : GitHub Releases based update checker. See HcfrUpdate.h.
//

#include "stdafx.h"
#include "HcfrUpdate.h"
#include "MiniJson.h"
#include "VersionInfoFromFile.h"

#include <winhttp.h>
#include <vector>
#include <ctype.h>    // tolower
#include <stdlib.h>   // atoi, atof
#include <stdio.h>    // fopen

#pragma comment(lib, "winhttp")

// Default repository the shipping build checks for updates.
static const char* const kDefaultOwner = "pixelpusher15";
static const char* const kDefaultRepo  = "hcfr-code";

// ---------------------------------------------------------------------------
// small helpers
// ---------------------------------------------------------------------------

// Convert a UTF-8 std::string (as produced by the JSON parser) to an ANSI
// CString on the active code page. Non-representable characters become '?'.
static CString Utf8ToCString(const std::string& utf8)
{
    if (utf8.empty())
        return CString();

    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), NULL, 0);
    if (wlen <= 0)
        return CString();

    std::vector<wchar_t> wbuf(wlen);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &wbuf[0], wlen);

    int alen = WideCharToMultiByte(CP_ACP, 0, &wbuf[0], wlen, NULL, 0, NULL, NULL);
    if (alen <= 0)
        return CString();

    std::vector<char> abuf(alen);
    WideCharToMultiByte(CP_ACP, 0, &wbuf[0], wlen, &abuf[0], alen, NULL, NULL);
    return CString(&abuf[0], alen);
}

// Pick the installer asset from a release's "assets" array. The project ships
// an .msi (e.g. "HCFR-4.1.0.0.msi"), so prefer the first .msi; otherwise fall
// back to the first .exe. Returns false if the release has no installer.
static bool PickInstallerAsset(const minijson::Value& assets,
                               CString& outUrl, CString& outName)
{
    int firstMsi = -1;
    int firstAny = -1;
    for (size_t i = 0; i < assets.size(); ++i)
    {
        std::string lower = assets.at(i).getString("name");
        for (size_t k = 0; k < lower.size(); ++k)
            lower[k] = (char)tolower((unsigned char)lower[k]);

        bool isMsi = lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".msi") == 0;
        bool isExe = lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".exe") == 0;
        if (!isMsi && !isExe)
            continue;

        if (isMsi && firstMsi < 0)
            firstMsi = (int)i;
        if (firstAny < 0)
            firstAny = (int)i;
    }

    int pick = (firstMsi >= 0) ? firstMsi : firstAny;
    if (pick < 0)
        return false;

    outUrl  = Utf8ToCString(assets.at(pick).getString("browser_download_url"));
    outName = Utf8ToCString(assets.at(pick).getString("name"));
    return true;
}

// ---------------------------------------------------------------------------
// CHcfrUpdate
// ---------------------------------------------------------------------------

CHcfrUpdate::CHcfrUpdate()
    : m_owner(kDefaultOwner)
    , m_repo(kDefaultRepo)
{
}

CString CHcfrUpdate::GetRunningVersion()
{
    VersionInfoFromFile verInfo;
    std::string ver;
    if (verInfo.getProductVersion(ver))
        return Utf8ToCString(ver);
    return CString();
}

CString CHcfrUpdate::NormalizeVersion(const CString& tag)
{
    CString s = tag;
    s.TrimLeft();
    s.TrimRight();

    // Drop a leading 'v'/'V'.
    if (!s.IsEmpty() && (s[0] == 'v' || s[0] == 'V'))
        s = s.Mid(1);

    // Keep only the leading dotted-digit portion (stop at '-', '+', space, ...).
    CString out;
    for (int i = 0; i < s.GetLength(); ++i)
    {
        char c = (char)s[i];
        if ((c >= '0' && c <= '9') || c == '.')
            out += c;
        else
            break;
    }
    return out;
}

int CHcfrUpdate::CompareVersions(const CString& a, const CString& b)
{
    CString na = NormalizeVersion(a);
    CString nb = NormalizeVersion(b);

    // Compare up to 4 dotted components; missing components count as 0.
    // strtoul (not atoi) so a huge component saturates to ULONG_MAX instead of
    // overflowing a signed int to a negative value and inverting the compare.
    for (int i = 0; i < 4; ++i)
    {
        unsigned long ca = 0, cb = 0;
        if (!na.IsEmpty())
        {
            int dot = na.Find('.');
            ca = strtoul((LPCSTR)(dot < 0 ? na : na.Left(dot)), NULL, 10);
            na = dot < 0 ? CString() : na.Mid(dot + 1);
        }
        if (!nb.IsEmpty())
        {
            int dot = nb.Find('.');
            cb = strtoul((LPCSTR)(dot < 0 ? nb : nb.Left(dot)), NULL, 10);
            nb = dot < 0 ? CString() : nb.Mid(dot + 1);
        }
        if (ca != cb)
            return ca < cb ? -1 : 1;
    }
    return 0;
}

bool CHcfrUpdate::IsNewer(const CString& remoteTag, const CString& localVer)
{
    return CompareVersions(remoteTag, localVer) > 0;
}

int CHcfrUpdate::QueryLatest(int ring, HcfrUpdateInfo& info)
{
    CString url;
    url.Format("https://api.github.com/repos/%s/%s/releases?per_page=30",
               (LPCSTR)m_owner, (LPCSTR)m_repo);

    std::string body;
    if (!HttpTransfer(url, /*apiHeaders*/true, &body, NULL, NULL, NULL))
        return -1;   // m_lastError already set

    minijson::Value root;
    std::string err;
    if (!minijson::Parser::parse(body, root, err) || !root.isArray())
    {
        SetError(CString("Could not parse update information."));
        return -1;
    }

    // Walk all releases, keep the newest that fits the selected ring and has a
    // usable installer asset. GitHub returns newest-first, but we compare
    // versions explicitly rather than trust ordering.
    bool found = false;
    HcfrUpdateInfo best;
    CString bestVer;

    for (size_t i = 0; i < root.size(); ++i)
    {
        const minijson::Value& rel = root.at(i);
        if (rel.getBool("draft"))
            continue;

        bool pre = rel.getBool("prerelease");
        if (ring == HCFR_RING_STABLE && pre)
            continue;   // stable ring ignores pre-releases

        CString tag = Utf8ToCString(rel.getString("tag_name"));
        CString ver = NormalizeVersion(tag);
        if (ver.IsEmpty())
            continue;

        CString assetUrl, assetName;
        if (!PickInstallerAsset(rel.get("assets"), assetUrl, assetName))
            continue;   // no installer attached - skip

        if (!found || CompareVersions(ver, bestVer) > 0)
        {
            best.version      = ver;
            best.tag          = tag;
            best.name         = Utf8ToCString(rel.getString("name"));
            best.notes        = Utf8ToCString(rel.getString("body"));
            best.htmlUrl      = Utf8ToCString(rel.getString("html_url"));
            best.assetUrl     = assetUrl;
            best.assetName    = assetName;
            best.isPrerelease = pre;
            bestVer           = ver;
            found             = true;
        }
    }

    if (!found)
        return 0;

    info = best;
    return 1;
}

bool CHcfrUpdate::DownloadFile(const CString& url, const CString& destPath,
                               ProgressFn cb, void* ctx)
{
    FILE* f = fopen(destPath, "wb");
    if (!f)
    {
        SetError(CString("Could not open download file for writing."));
        return false;
    }

    bool ok = HttpTransfer(url, /*apiHeaders*/false, NULL, f, cb, ctx);

    fclose(f);
    if (!ok)
        DeleteFile(destPath);
    return ok;
}

// ---------------------------------------------------------------------------
// WinHTTP transfer core
// ---------------------------------------------------------------------------

bool CHcfrUpdate::HttpTransfer(const CString& url, bool apiHeaders,
                               std::string* pBody, FILE* destFile,
                               ProgressFn cb, void* ctx)
{
    CStringW wurl(url);

    URL_COMPONENTS uc;
    ZeroMemory(&uc, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256]  = {0};
    wchar_t path[2048] = {0};
    wchar_t extra[1024] = {0};
    uc.lpszHostName     = host;  uc.dwHostNameLength  = _countof(host);
    uc.lpszUrlPath      = path;  uc.dwUrlPathLength   = _countof(path);
    uc.lpszExtraInfo    = extra; uc.dwExtraInfoLength = _countof(extra);

    if (!WinHttpCrackUrl(wurl, 0, 0, &uc))
    {
        SetError(CString("Malformed update URL."));
        return false;
    }

    CStringW fullPath(path);
    fullPath += extra;   // reattach query string
    bool https = (uc.nScheme == INTERNET_SCHEME_HTTPS);

    HINTERNET hSession = WinHttpOpen(L"HCFR-Updater/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
    {
        SetError(CString("Could not initialize network session."));
        return false;
    }

    // Reasonable timeouts so a stalled server never hangs the caller.
    WinHttpSetTimeouts(hSession, 15000, 15000, 20000, 30000);

    // Declared before the first 'goto cleanup' so no jump crosses an
    // initialization (MSVC C2362).
    bool             result = false;
    __int64          total  = -1;
    unsigned __int64 done   = 0;
    HINTERNET hConnect = WinHttpConnect(hSession, host, uc.nPort, 0);
    HINTERNET hRequest = NULL;

    if (!hConnect)
    {
        SetError(CString("Could not connect to update server."));
        goto cleanup;
    }

    hRequest = WinHttpOpenRequest(hConnect, L"GET", fullPath,
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        https ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest)
    {
        SetError(CString("Could not create update request."));
        goto cleanup;
    }

    if (apiHeaders)
    {
        WinHttpAddRequestHeaders(hRequest,
            L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n",
            (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hRequest, NULL))
    {
        SetError(CString("Could not reach the update server."));
        goto cleanup;
    }

    // Check HTTP status.
    {
        DWORD status = 0, slen = sizeof(status);
        WinHttpQueryHeaders(hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &slen, WINHTTP_NO_HEADER_INDEX);
        if (status < 200 || status >= 300)
        {
            CString e;
            e.Format("Update server returned HTTP %u.", status);
            SetError(e);
            goto cleanup;
        }
    }

    // Optional content length for progress reporting.
    {
        DWORD cl = 0, cllen = sizeof(cl);
        if (WinHttpQueryHeaders(hRequest,
                WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &cl, &cllen, WINHTTP_NO_HEADER_INDEX))
            total = (__int64)cl;
    }

    // Read loop.
    for (;;)
    {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &avail))
        {
            SetError(CString("Network read error."));
            goto cleanup;
        }
        if (avail == 0)
            break;

        std::vector<char> buf(avail);
        DWORD read = 0;
        if (!WinHttpReadData(hRequest, &buf[0], avail, &read))
        {
            SetError(CString("Network read error."));
            goto cleanup;
        }
        if (read == 0)
            break;

        if (pBody)
            pBody->append(&buf[0], read);
        if (destFile && fwrite(&buf[0], 1, read, destFile) != read)
        {
            SetError(CString("Could not write download to disk."));
            goto cleanup;
        }

        done += read;
        if (cb && !cb(ctx, done, total))
        {
            SetError(CString("Download cancelled."));
            goto cleanup;
        }
    }

    result = true;

cleanup:
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);
    return result;
}
