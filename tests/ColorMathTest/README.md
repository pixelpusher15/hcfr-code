# ColorMathTest — 8-bit golden-master regression harness

Guards the 8-bit signal path while the 10-bit work lands. Part of the plan in
`TEN-BIT-PIPELINE-PLAN.md` (repo root, local-only doc); PR 1 of that plan.

## What it tests
- **T1** — `ColorRGBDisplay::ConvertPercentToBYTE` against a **frozen verbatim copy** of the
  legacy implementation carried inside the test (no golden file). This is the core 8-bit
  contract; ~1M-point sweep per range plus grid and clamp edges. **Never update the frozen
  copy** — if T1 fails, the live quantizer changed behavior.
- **T2/T3** — `ArrayIndexToGrayLevel` / `GrayLevelToGrayProp` tables for all grayscale sizes
  and all three rounding modes (normal / round-down / 10-bit disc), vs golden files.
- **T4** — `GenerateSaturationColors` (5 standards x 6 color combos x 3 sizes x 3 EOTF modes)
  and the hardcoded `GenerateCC24Colors` modes (GCD/MCD/CCSG), vs golden files. Emitted twice:
  the 8-bit path (`SAT`/`CC` rows, `b10bit=false`) and the native 10-bit path (`SAT10`/`CC10`
  rows, `b10bit=true`, quantized to the 219*4 grid). The 8-bit rows are frozen and must not
  drift; T4 catches accidental changes (e.g. a stray edit in byte-sensitive `Color.cpp`). Note
  `CC10` SDR rows equal their `CC` counterparts because the hardcoded ColorChecker tables are
  fixed 8-bit reference values (only the HDR-recalc path is regenerated on the 10-bit grid).
- **T6** — `GetColorRef` COLORREF packing, vs golden file.
- **T5** (rPI emission function) arrives with PR 3. **T7** (.chc round-trip) needs app-level
  linkage and is not in this console harness.

## Build & run
Build `libHCFR` first, then this project (Debug|Win32 only):
```
MSBuild ColorHCFR_VS2019.sln -t:libHCFR -p:Configuration=Debug -p:Platform=Win32 -m
MSBuild tests\ColorMathTest\ColorMathTest.vcxproj -p:Configuration=Debug -p:Platform=Win32 -m
```
Run from `tests\ColorMathTest\` (golden dir is resolved relative to the CWD):
```
Debug\ColorMathTest\ColorMathTest.exe verify     # the normal regression run; exit 0 = pass
Debug\ColorMathTest\ColorMathTest.exe gen        # REGENERATES golden files - known-good builds only
```
Run `verify` after every build of every PR in the 10-bit series. Only run `gen` when the
baseline is intentionally being moved (and say so in the PR description).

The golden files are byte-exact LF text (`%.17g` doubles) pinned `-text` in `.gitattributes`;
do not let an editor or git touch their line endings.
