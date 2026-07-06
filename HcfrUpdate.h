// HcfrUpdate.h : GitHub Releases based update checker.
//
// Replaces the defunct FTP/SHA1 CWebUpdate. Talks to the GitHub REST API over
// HTTPS (WinHTTP), understands "stable" vs "pre-release" update rings, compares
// the running version against the newest applicable release, and can download
// the installer asset.
//
#ifndef _HCFRUPDATE_H
#define _HCFRUPDATE_H

#include <string>

// Update rings the user can subscribe to.
enum HcfrUpdateRing
{
    HCFR_RING_STABLE     = 0,   // released, non-prerelease builds only
    HCFR_RING_PRERELEASE = 1    // newest build, including pre-releases
};

// Result of a successful query.
struct HcfrUpdateInfo
{
    CString version;        // normalized numeric version, e.g. "4.2.0"
    CString tag;            // raw tag_name, e.g. "v4.2.0-beta1"
    CString name;           // release title
    CString notes;          // release body / notes (ANSI)
    CString htmlUrl;        // release page on github.com
    CString assetUrl;       // browser_download_url of the installer asset
    CString assetName;      // installer file name, e.g. "HCFRSetup.exe"
    bool    isPrerelease;

    HcfrUpdateInfo() : isPrerelease(false) {}
};

class CHcfrUpdate
{
public:
    CHcfrUpdate();

    // Repo to query. Defaults to the shipping repo (see constructor).
    void SetRepo(LPCSTR owner, LPCSTR repo) { m_owner = owner; m_repo = repo; }

    // Query GitHub for the newest release applicable to 'ring'.
    //   1  = a release was found            (info populated)
    //   0  = query OK but no usable release (e.g. no assets)
    //  -1  = network / parse / HTTP error   (see GetLastErrorText)
    int QueryLatest(int ring, HcfrUpdateInfo& info);

    // Download 'url' to 'destPath'. Optional progress callback receives bytes
    // downloaded so far and the total (or -1 when the server omits a length).
    // Return false from the callback to abort. Returns true on success.
    typedef bool (*ProgressFn)(void* ctx, unsigned __int64 done, __int64 total);
    bool DownloadFile(const CString& url, const CString& destPath,
                      ProgressFn cb = NULL, void* ctx = NULL);

    CString GetLastErrorText() const { return m_lastError; }

    // --- version helpers ---------------------------------------------------

    // Running app version from the exe's version resource, e.g. "4.1.0.0".
    static CString GetRunningVersion();

    // Strip a leading 'v' and any "-suffix"/"+build", leaving dotted digits.
    static CString NormalizeVersion(const CString& tag);

    // Compare dotted numeric versions. >0 if a>b, 0 if equal, <0 if a<b.
    static int CompareVersions(const CString& a, const CString& b);

    // True if 'remoteTag' represents a strictly newer version than 'localVer'.
    static bool IsNewer(const CString& remoteTag, const CString& localVer);

private:
    // Core WinHTTP transfer. If 'pBody' is non-NULL the response is appended to
    // it; if 'destFile' is non-NULL it is streamed to that FILE*. Sends the
    // GitHub API headers when 'apiHeaders' is true.
    bool HttpTransfer(const CString& url, bool apiHeaders,
                      std::string* pBody, FILE* destFile,
                      ProgressFn cb, void* ctx);

    void SetError(const CString& text) { m_lastError = text; }

    CString m_owner;
    CString m_repo;
    CString m_lastError;
};

#endif // _HCFRUPDATE_H
