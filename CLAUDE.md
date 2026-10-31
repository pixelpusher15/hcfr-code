# HCFR / ColorHCFR — working rules

ColorHCFR is a Windows MFC app. Solution: `ColorHCFR_VS2019.sln` (the name is historical; it
builds with newer Visual Studio as long as the MSVC **v142** toolset and MFC components are
installed — every `.vcxproj` uses `PlatformToolset v142`). Installer/MSI: `ColorHCFR_VS2019.vdproj` (a VS
*deployment* project — builds only via `devenv`, not MSBuild). No CI; all builds are local.

## Source control — never break these
- **Never commit to `main`.** All work goes on a branch → PR.
- **This repo is a fork; all work targets `pixelpusher15/hcfr-code`.** Never push to, open PRs
  against, or otherwise touch the upstream repository it was forked from. Bare `gh pr create`
  defaults to upstream, so always pass `--repo pixelpusher15/hcfr-code`, and verify the PR URL
  before merging.
- `.claude/hooks/guard-git.ps1` (wired in `.claude/settings.json`) enforces the above on
  `git`/`gh` calls from the Bash and PowerShell tools. A block from it is a real rule, not a
  glitch — fix the command, don't bypass.
- Personal, machine-specific rules go in `CLAUDE.local.md` (gitignored), not in this file.
- **GitHub Releases are the source of truth for release notes** — there is no in-repo notes file.

## Building
- **Build the whole solution, not single projects.** The 5 language satellite DLLs
  (`CHCFR21_ENGLISH/FRANCAIS/DEUTSCH/ESPANOL/ITALIAN`) + `CHCFR21_PATTERNS` are separate
  `.vcxproj`s. Building only `ColorHCFR` leaves some languages stale/inactive. A full-solution
  build is the only thing that guarantees every language is current.
- Use **MSBuild** for code builds. Example path (VS 18 Community — adjust to your install):
  `"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" ColorHCFR_VS2019.sln -p:Configuration=Debug -p:Platform=Win32 -m -v:m`
  Win32 only; there is no x64 config.
- `devenv.com` is only needed for the `.vdproj` MSI (`/release`); MSBuild is faster for code.
- **Git Bash rewrites any argument that starts with `/` into a Windows path** (`/Build` becomes
  `C:/Program Files/Git/Build`). With devenv, that means no build at all: it opens the IDE behind
  a hidden "files could not be found" dialog and looks hung. From Git Bash, prefix devenv with
  `MSYS_NO_PATHCONV=1`, or run it from cmd/PowerShell. The same trap is why the app's harness
  switch is passed as `-accuracytest`, not `/accuracytest`.

## Before merging any PR
Run all of these and report the results in the PR:
1. **Full-solution build** (above), 0 errors.
2. **Accuracy harness:** from Git Bash, `./Debug/ColorHCFR.exe -accuracytest` (from cmd:
   `Debug\ColorHCFR.exe -accuracytest`) — pass = **834 combos, 0 FAIL, exit code 0**. Use the
   `-accuracytest` form in Git Bash: MSYS rewrites a leading `/` into a path, which launches the
   GUI with a garbage argument instead of running the test. The current pass/known-fail baseline
   is in `tests/ACCURACYTEST.md` (the `Reference result` line); a shift in the known-fail count
   needs explaining, but `0 FAIL` + exit 0 is the gate.
3. **ColorMathTest** is **not** in the solution, so build it separately after the full-solution
   build: `MSBuild tests\ColorMathTest\ColorMathTest.vcxproj -p:Configuration=Debug -p:Platform=Win32 -m`.
   Then run it from `tests/ColorMathTest/` (golden files resolve relative to the CWD):
   `./Debug/ColorMathTest/ColorMathTest.exe verify` — exit 0 = pass. Never run `gen` unless the
   baseline is being moved on purpose (say so in the PR).
4. **An independent review pass** of the diff (e.g. `/code-review`, or a fresh session that did
   not write the change) before merge.

## Localization — always, without being asked
- **Every new user-facing string must be localized into all 5 languages** in the same change.
  Add it to each `CHCFR21_<LANG>.rc` with the same resource ID and a real translation.
  Do not ship an English-only string and do not ask permission first — just do the translations.
  Being a non-native speaker is not a reason to skip it; get it done and note any low-confidence
  translation so a human can eyeball it.

## Known issues
- Open bugs and code-review findings are tracked as **GitHub issues on
  `pixelpusher15/hcfr-code`**, labeled `type:*` and `sev:*`. When editing code that an open issue
  touches, mention the issue and ask whether to address it in the same change.

## Byte-sensitive files
- The `.rc` satellite files (`CHCFR21_FRANCAIS/DEUTSCH/ESPANOL/ITALIAN`) are per-language
  Latin-1 with hundreds of intentional accented bytes. `CHCFR21_PATTERNS.rc` is Latin-1 too (one
  accented byte, in its VERSIONINFO `ProductName`); only `CHCFR21_ENGLISH.rc` is pure ASCII. **Do not re-encode them.** When Edit/Write
  would round-trip them through UTF-8, edit byte-safely (PowerShell byte splice) or via targeted
  resource edits, then confirm with `git diff` that only the intended lines changed.
- **Many `.cpp/.h` files are still Latin-1**, not UTF-8 — roughly half, mostly the "François"
  author credit in the header comment, and `ColorHCFR.cpp` has one inside a string literal (the
  About text). **Grep a file for non-ASCII bytes before editing it**; if it has any, edit
  byte-safely like the `.rc` files, or the save will re-encode it to UTF-8.
- New and touched code should be **pure ASCII**. Use `\xNN` escapes for any needed high-byte
  string literals (e.g. `"Y cd/m\xB2"`, `"T\xB0"`); keep comments ASCII.
- `macSources/**` is Mac Obj-C, **not** in any Windows `.vcxproj` — ignore for Windows edits.

## Releases
- Use the `/release` command — it drives the whole flow (diff since last tag → notes → version
  bump → MSI → approve → upload → AVSForum post). See `.claude/commands/release.md`.
