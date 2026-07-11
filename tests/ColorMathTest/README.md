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
  and the hardcoded `GenerateCC24Colors` modes (GCD/MCD/CCSG), vs golden files. Emitted for
  every native grid the generators can quantize to: 8-bit limited (`SAT`/`CC` rows — the
  frozen legacy path, byte-identical forever), 8-bit full (`SAT8F`/`CC8F`, 255 grid), 10-bit
  limited (`SAT10`/`CC10`, 876 grid), and 10-bit full (`SAT10F`/`CC10F`, 1023 grid). T4
  catches accidental drift (e.g. a stray edit in byte-sensitive `Color.cpp`). Note `CC*`
  SDR (eotf=0) rows equal the hardcoded ColorChecker tables regardless of grid — only the
  HDR-recalc path re-quantizes; the SDR tables are fixed 8-bit reference values.
- **T8** — `SnapToVideoGrid` oracle (no golden file): the shared patch/reference/sensor
  quantizer must hit the exact native grid for all four (bit depth, range) combos —
  219/255/876/1023 — and its 8-bit limited form must equal the historical
  `floor(v*219+0.5)/219` exactly.
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
