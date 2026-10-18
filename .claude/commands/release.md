---
description: Cut a ColorHCFR release — diff since last tag, draft notes, build MSI, upload to GitHub, write the AVSForum post
---

Drive a full ColorHCFR release with as little manual work for the user as possible. The user
should only have to: (1) approve the release notes, (2) say release-vs-prerelease. Do everything
else, and at the end give them an exact, short list of anything only they can do.

Environment:
- Toolchain example: **Visual Studio 18 Community** (adjust paths to your install), with the
  MSVC **v142** toolset and MFC components installed (every `.vcxproj` uses `PlatformToolset v142`).
  Console build binary: `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\devenv.com`
- Solution `ColorHCFR_VS2019.sln`, active config **`Release|Win32`** (Win32 only; no x64).
- MSI project `ColorHCFR_VS2019.vdproj` is in the solution and is set to build in
  `Release|Win32`. Building it needs the "Microsoft Visual Studio Installer Projects" extension.
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

## 2. Bump the version (on a branch → PR, never on `main`)
- **`ColorHCFR.rc`** (~line 73) and **all six satellite `.rc` files**
  (`CHCFR21_ENGLISH/FRANCAIS/DEUTSCH/ESPANOL/ITALIAN/PATTERNS.rc`) each have a VERSIONINFO block
  with four fields to bump: `FILEVERSION`, `PRODUCTVERSION`, and the `"FileVersion"` /
  `"ProductVersion"` string values.
- **`FILEVERSION` / `PRODUCTVERSION` must use commas** (`4,2,0,0`), never dots. The resource
  compiler reads `4.2.0.0` as the single number 4 and writes a binary (fixed) version of
  `4.0.0.0` — this is why 4.1.0.0 shipped 7 of 8 files stamped `4.0.0.0`. The string values stay
  dotted (`"4.2.0.0"`).
- The non-English satellites are **Latin-1** — edit them byte-safely, e.g. from Git Bash:
  `LC_ALL=C sed -i -E 's/^( (FILE|PRODUCT)VERSION) 4,1,0,0(\r?)$/\1 4,2,0,0\3/' <file>`
  (the `\r?` keeps the pattern working on CRLF files). Then check `git diff` shows only the
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
- **Merge the bump PR before building.** Build and tag from the merged `main` commit, so the
  release tag points at a commit that contains the bump.

## 3. Draft release notes  → WAIT for approval
- Group the changes into user-facing themes (Fixed / Added / Changed). Plain language, not commit
  subjects. If this is the first release since the installer fixes, mention them: the 4.1.0.0
  MSI omitted `CHCFR21_ITALIAN.dll` (Italian unavailable to MSI installs) and packaged
  FRANCAIS/PATTERNS twice. Commit `e34ccb41` restored Italian; PR #201 removed the duplicates,
  so the MSI now ships each language DLL once and is ~80 MB smaller.
- Show the notes to the user. **Do not proceed until they approve.** Revise as asked.

## 4. Build
Run from the repo root, on the merged `main` commit from step 2. `devenv.com` is the console
build variant (blocks until done; `devenv.exe` would return immediately).
- **From Git Bash, prefix every devenv command with `MSYS_NO_PATHCONV=1`.** Otherwise Git Bash
  rewrites `/Build` and `/Project` into Windows paths (`C:/Program Files/Git/Build`); devenv then
  builds nothing, exits 0 with no output, and leaves the IDE open behind a hidden
  "files could not be found" dialog. cmd and PowerShell don't need this.
- **One-time per machine/user:** run `DisableOutOfProcBuild.exe` from
  `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\VSI\DisableOutOfProcBuild\`
  (cd there first). It sets the registry key that avoids
  `ERROR: An error occurred while validating. HRESULT = '8000000A'` on command-line `.vdproj` builds.
- Full solution (all 6 resource `.vcxproj`s, so every satellite DLL is current, plus the MSI,
  which the solution builds in `Release|Win32`):
  `"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\devenv.com" ColorHCFR_VS2019.sln /Build "Release|Win32"`
- If the MSI didn't come out of that, build it explicitly:
  `"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\devenv.com" ColorHCFR_VS2019.sln /Project ColorHCFR_VS2019.vdproj /Build "Release|Win32"`
- If devenv returns at once or sits with no output, check `Get-Process devenv | Select MainWindowTitle`.
  An open IDE window means the arguments were mangled (see above) or VS is showing a dialog:
  stop that process and fix the command rather than clicking through the IDE.
- A good build ends with `========== Build: N succeeded, 0 failed` and contains no
  `WARNING: Two or more objects have the same target location` lines (those mean a file is
  packaged twice).
- If devenv reports the project type is unsupported, the Installer Projects extension isn't
  installed — tell the user to install it (or build the MSI from the IDE) and stop.
- Report the produced `.msi` path and confirm all 6 `CHCFR21_*.dll` (5 languages + PATTERNS)
  are inside it **exactly once each** (the step 4b read loop lists them).

## 4b. Check the MSI File table versions
Windows Installer decides whether to overwrite a file on upgrade by comparing the `File` table's
`Version` with the installed file's **binary (fixed) version** — not the `"FileVersion"` string.
The `.vdproj` build copies each file's fixed version into the table, so the two only disagree if
step 2 used dotted `FILEVERSION` lines.
- Check each built HCFR binary's fixed version (Release output folder):
  ```powershell
  Get-ChildItem Release\ColorHCFR.exe, Release\CHCFR21_*.dll | ForEach-Object {
    $v = $_.VersionInfo
    '{0}  fixed={1}.{2}.{3}.{4}  string={5}' -f $_.Name, $v.FileMajorPart, $v.FileMinorPart, $v.FileBuildPart, $v.FilePrivatePart, $v.FileVersion
  }
  ```
  Every `fixed=` must equal the release version. If one doesn't, fix that file's `.rc`
  (comma format), rebuild, and check again — **don't patch the MSI to hide it**, because a table
  version higher than the file's real fixed version breaks upgrade file replacement.
- Then read the MSI's `File` table with the WindowsInstaller COM API and confirm each HCFR row
  (`ColorHCFR.exe`, `CHCFR21_*.dll`; `FileName` is `SHORT~1|LongName`) matches:
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
- Fallback only, if a row is wrong even though the binary's fixed version is right: in a **new
  PowerShell process** (so the read-only handle above is released), open the database with mode
  `1` (transact), run `UPDATE File SET Version='<ver>' WHERE File='<key>'` for each wrong row,
  set a **new PackageCode** (Summary Information property 9 = a fresh `{GUID}`: get
  `SummaryInformation` with update count 1, set `Property(9)`, call `Persist`), then `Commit`.
  Changing the MSI's contents without a new PackageCode breaks Windows Installer's package
  caching. Re-run the read loop and show the user before/after.

## 5. Upload  → ASK release vs prerelease
- Ask the user: **stable release or prerelease?** (Auto-update Stable ring only picks up
  non-prerelease; the two rings are Stable and pre-release. 4.2 is planned as Stable.)
- Create the GitHub release on the fork with the approved notes and the `.msi` asset, tagging the
  exact commit that was built:
  `gh release create v<ver> <path-to-msi> --repo pixelpusher15/hcfr-code --target <built-commit-sha> --title "v<ver>" --notes-file <notes> [--prerelease]`
- Tag matches the version. Verify the release URL points at `pixelpusher15/hcfr-code`.

## 6. AVSForum post
- Write a ready-to-paste AVSForum post: what's new in user terms, the download link to the release,
  and any install notes. Keep it forum-toned, not changelog-toned. One paragraph per line — no
  hard-wrapped prose, since it will be pasted into a web form.

## 7. Hand off
- Give the user a short, explicit list of what remains that only they can do (e.g. "paste the
  AVSForum post", "flip prerelease→stable when ready to promote"). If nothing remains, say so.
