---
description: Cut a ColorHCFR release — diff since last tag, draft notes, build MSI, upload to GitHub, write the AVSForum post
---

Drive a full ColorHCFR release with as little manual work for the user as possible. The user
should only have to: (1) approve the release notes, (2) say release-vs-prerelease. Do everything
else, and at the end give them an exact, short list of anything only they can do.

Environment:
- Toolchain example: **Visual Studio 18 Community** (adjust paths to your install). Console build
  binary: `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\devenv.com`
- Solution `ColorHCFR_VS2019.sln`, active config **`Release|Win32`** (Win32 only; no x64).
- MSI project `ColorHCFR_VS2019.vdproj` is in the solution (build it explicitly via `/Project`).
  Building it needs the "Microsoft Visual Studio Installer Projects" extension.
- `gh` must be authenticated as an account with write access to `pixelpusher15/hcfr-code`.
  Every `gh` call passes `--repo pixelpusher15/hcfr-code` — never the upstream repo.
- No CI — all builds are local.

Release channel: 4.1.0.0 shipped as a pre-release. **4.2 is planned as the first full
(non-pre-release) Stable release** — the auto-updater's Stable ring only picks up
non-prerelease GitHub releases.

Follow these steps in order. Stop and wait where it says WAIT.

## 1. Determine scope
- Latest release tag: `git describe --tags --abbrev=0`.
- Collect everything since then: `git log <lasttag>..HEAD --no-merges` plus merged PRs
  (`gh pr list --repo pixelpusher15/hcfr-code --state merged --search "merged:>=<date>"`).
- Propose the new version number (semantic bump from the last tag). Confirm it with the user.
- Confirm `main` is green on the pre-merge checks in `CLAUDE.md` (full-solution build,
  `-accuracytest`, ColorMathTest verify). Don't release a tree that hasn't passed them.

## 2. Bump the version (do it on a branch → PR, never on `main`)
- **`ColorHCFR.rc`** (~line 73): `FILEVERSION` / `PRODUCTVERSION` and the `"FileVersion"` /
  `"ProductVersion"` string values.
- **All six satellite `.rc` files** — `CHCFR21_ENGLISH/FRANCAIS/DEUTSCH/ESPANOL/ITALIAN/PATTERNS.rc`
  — each has its own VERSIONINFO block with the same four fields. Note the format split:
  `CHCFR21_DEUTSCH.rc` and `CHCFR21_ITALIAN.rc` write `FILEVERSION 4,1,0,0` (commas); the others
  use dots. Keep each file's existing format. The string values are dotted in all six.
  The non-English satellites are **Latin-1** — edit them byte-safely (e.g. `sed -i` on the exact
  version lines from Git Bash, which does not re-encode), then check `git diff` shows only the
  version lines changed.
- **`ColorHCFR_VS2019.vdproj`** (Product section near the end of the file):
  - `ProductCode` — **new GUID** every release.
  - `PackageCode` — **new GUID** every release.
  - `ProductVersion` — bump (3-part, e.g. `8:4.2.0`).
  - `OutputFilename` (~line 672) — `Release\\Installer\\HCFR-<ver>.msi`.
  - **Never change `UpgradeCode`.** It is what lets a new MSI upgrade an installed one.
  Generate GUIDs with `powershell -NoProfile -Command "([guid]::NewGuid()).ToString().ToUpper()"`.
- Grep afterwards to confirm no stale old-version string remains in any of the 7 `.rc` files
  or the `.vdproj`.

## 3. Draft release notes  → WAIT for approval
- Group the changes into user-facing themes (Fixed / Added / Changed). Plain language, not commit
  subjects. Note the known installer quirk if still unresolved (MSI historically omitted
  `CHCFR21_ITALIAN.dll` and duplicated FRANCAIS/PATTERNS — check whether this release fixes it).
- Show the notes to the user. **Do not proceed until they approve.** Revise as asked.

## 4. Build
Run from the repo root. `devenv.com` is the console build variant (blocks until done; `devenv.exe`
would return immediately).
- **One-time per machine/user:** run `DisableOutOfProcBuild.exe` from
  `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\VSI\DisableOutOfProcBuild\`
  (cd there first). It sets the registry key that avoids
  `ERROR: An error occurred while validating. HRESULT = '8000000A'` on command-line `.vdproj` builds.
- Full solution (compiles all 6 language `.vcxproj` so every satellite DLL is current):
  `"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\devenv.com" ColorHCFR_VS2019.sln /Build "Release|Win32"`
- MSI (deployment projects are usually excluded from the solution build, so build it explicitly):
  `"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\devenv.com" ColorHCFR_VS2019.sln /Project ColorHCFR_VS2019.vdproj /Build "Release|Win32"`
- If devenv sits with no output for minutes, it has probably opened a hidden GUI dialog
  (check `Get-Process devenv | Select MainWindowTitle`); kill it and resolve the dialog in the IDE.
- If devenv reports the project type is unsupported, the Installer Projects extension isn't
  installed — tell the user to install it (or build the MSI from the IDE) and stop.
- Report the produced `.msi` path and confirm all 5 `CHCFR21_*.dll` are inside it.

## 4b. Check and patch the MSI File table versions
The `.vdproj` build can bake **stale file versions** into the MSI's `File` table (4.1.0.0
shipped with `4.0.0.0` recorded for 7 of 8 HCFR files). Windows Installer uses those versions
to decide whether to overwrite files on upgrade, so stale values can leave old DLLs installed.
- Read the `File` table with the WindowsInstaller COM API and compare each HCFR row
  (`ColorHCFR.exe`, `CHCFR21_*.dll`; `FileName` is `SHORT~1|LongName`) against the built
  binary's `(Get-Item <file>).VersionInfo.FileVersion`:
  ```powershell
  $msi  = (Resolve-Path 'Release\Installer\HCFR-<ver>.msi').Path
  $inst = New-Object -ComObject WindowsInstaller.Installer
  $db   = $inst.GetType().InvokeMember('OpenDatabase','InvokeMethod',$null,$inst,@($msi,0))
  $view = $db.GetType().InvokeMember('OpenView','InvokeMethod',$null,$db,@('SELECT File, FileName, Version FROM File'))
  $view.GetType().InvokeMember('Execute','InvokeMethod',$null,$view,$null)
  while ($r = $view.GetType().InvokeMember('Fetch','InvokeMethod',$null,$view,$null)) {
    $get = { param($i) $r.GetType().InvokeMember('StringData','GetProperty',$null,$r,@($i)) }
    '{0}  {1}  {2}' -f (& $get 1), (& $get 2), (& $get 3)
  }
  ```
- If any HCFR row is stale: reopen with mode `1` (transact), run
  `UPDATE File SET Version='<ver>' WHERE File='<key>'` for each stale row, then set a **new
  PackageCode** (Summary Information property 9 = a fresh `{GUID}`; get it via
  `SummaryInformation` with update count 1, set `Property(9)`, call `Persist`), and `Commit`
  the database. Changing the MSI's contents without a new PackageCode breaks Windows Installer's
  package caching.
- Re-run the read loop and show the user the before/after versions. Don't upload until every
  HCFR row matches its binary.

## 5. Upload  → ASK release vs prerelease
- Ask the user: **stable release or prerelease?** (Auto-update Stable ring only picks up
  non-prerelease; the two rings are Stable and pre-release. 4.2 is planned as Stable.)
- Create the GitHub release on the fork with the approved notes and the `.msi` asset:
  `gh release create v<ver> <path-to-msi> --repo pixelpusher15/hcfr-code --title "v<ver>" --notes-file <notes> [--prerelease]`
- Tag matches the version. Verify the release URL points at `pixelpusher15/hcfr-code`.

## 6. AVSForum post
- Write a ready-to-paste AVSForum post: what's new in user terms, the download link to the release,
  and any install notes. Keep it forum-toned, not changelog-toned. One paragraph per line — no
  hard-wrapped prose, since it will be pasted into a web form.

## 7. Hand off
- Give the user a short, explicit list of what remains that only they can do (e.g. "paste the
  AVSForum post", "flip prerelease→stable when ready to promote"). If nothing remains, say so.
