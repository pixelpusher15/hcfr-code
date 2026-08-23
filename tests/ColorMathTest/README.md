# ColorMathTest — 8-bit golden-master regression harness

Guards the 8-bit signal path while the 10-bit work lands. Part of the plan in
`TEN-BIT-PIPELINE-PLAN.md` (repo root, local-only doc); PR 1 of that plan.

## What it tests
- **T1** — `ColorRGBDisplay::ConvertPercentToBYTE` against a **frozen verbatim copy** of the
  legacy implementation carried inside the test (no golden file). This is the core 8-bit
  contract; ~1M-point sweep per range plus grid and clamp edges. **Never update the frozen
  copy** — if T1 fails, the live quantizer changed behavior.
- **T2/T3** — `ArrayIndexToGrayLevel` / `GrayLevelToGrayProp` tables for all grayscale sizes,
  all three rounding modes (normal / round-down / 10-bit disc) **and both wire ranges**, vs
  golden files. The `is16_235` column selects the native code grid (219 limited / 255 full at
  8 bits, 876 / 1023 at 10) exactly as `SnapToVideoGrid` does. The `is16_235=1` rows carry the
  historic values: every value is byte-identical to the pre-range goldens, though the lines
  themselves are not (the row layout gained the range column and T2 dropped a trailing zero
  field). If one of those values moves, the limited-range gray path changed and that is a
  regression, not a re-baseline.
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
- **T5** — rPI emission quantizers (`PiPercentToCode` / `PiBackground8ToCode`) across all
  four grids: endpoints, clamping, monotonicity, and distinct-code counts. No golden file.
- **T7** — `GenerateProfileColors` determinism: plain and extra patch counts per cube size.
  No golden file. (This slot was originally reserved for a `.chc` round-trip, which needs
  app-level linkage and is still not in this console harness.)
- **T9** — quantizer equivalence oracle (no golden file): the generator grid form, the
  shared `SnapToVideoGrid`, and the rPI emitter must all select the same integer code for
  every (bit depth, range) combo, including half-code ties plus/minus dust.
- **T10** — gamut-basis consistency oracle (no golden file): for every standard in the
  enum except `CUSTOM`, under three white targets, the basis a consumer plots GEOMETRY in
  must be the basis its references were BUILT in. Catches the class dE tests are blind to
  — a point drawn in the wrong place while its dE reads exactly 0.000 — which is how the
  3D viewer came to draw HDTVa/HDTVb solids from their 75%/plasma PATCH chromaticities
  instead of Rec.709. `CUSTOM` is excluded because constructing one corrupts the global
  Rec.601 primaries (see the comment above `RunT10`); `CC6` skips the second leg only.
  Note this pins the shared helper, **not** any view's use of it — nothing here links MFC.
- **T13** — `DisplayModel` v1 recovery oracle (no golden file): a synthetic display with
  known per-channel gammas, primary matrix, and black offset must be recovered exactly
  from noise-free samples; weight semantics (a weight-0 sample must leave matrix, gammas,
  black, and `samplesUsed` unchanged; a heavy corrupt sample must pull the fit; uniform
  weight rescaling must change nothing); inverse round-trip and out-of-gamut flag/clamp;
  input-contract rejection (non-finite fields, negative weight, stimulus outside [0,1]);
  failed re-fit preserves the previous model; unfitted-model transforms refuse;
  parameter-provenance flags (fitted / gray-derived / assumed) for gammas and black,
  including quantization-jittered gray ramps staying in the gray pool. **T11/T12 are
  deliberately skipped**: claimed by in-flight branches (PR #178's BT.2390 oracles;
  csv-provenance/wtw-reference renumber on rebase) — check open branches before
  assigning any new T number.

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
