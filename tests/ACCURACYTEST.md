# /accuracytest — in-app reference == wire == sensor-model matrix

Automates the manual simulated-sensor sweeps used to validate color-math
changes: for every meaningful configuration combination it verifies that the
app's **references** (CMeasure `GetRefPrimary` / `GetRefSecondary` /
`GetRefSat` / `GetRefCC24Sat` and the grayscale EOTF target) model the **wire**
(the exact patch codes the generators emit) exactly, by pushing those codes
through `CSimulatedSensor` (error injection off) and asserting dE ~ 0 with the
**same normalization the measures grid uses** (`CMainView::UpdateGrid` +
`GetItemText`: the unified HDR `GetHDRRefScale` reference rescale
(= `105.95640` with tone mapping off), the `tmWhite`-based `RefWhite`, the
per-view YWhite source — gray-ramp top, PrimeWhite, or OnOff white).

## Relation to `tests\ColorMathTest`

They are complementary layers — run **both** after any color-math change:

| Harness | Layer | How it checks |
|---|---|---|
| `ColorMathTest.exe verify` | pure libHCFR (patch generators, quantizers, EOTF round trips) | byte-exact golden files |
| `ColorHCFR.exe /accuracytest` | app-coupled reference layer (CMeasure GetRef*, grid dE conventions) | dE ~ 0 invariant vs the simulated sensor |

The GetRef* functions are coupled to `GetConfig()` and to measured state
(gray-array top, OnOffBlack, PrimeWhite), so ColorMathTest cannot reach them.
`/accuracytest` never asserts absolute golden values — it asserts the
*invariant* that a perfect display model measures its own reference.

## Running

```
Debug\ColorHCFR.exe /accuracytest [report.txt]
echo %ERRORLEVEL%     rem 0 = pass, 1 = failures, 2 = cannot write report
```

Headless: no windows, no generators, no real measure loops; the run takes
seconds and writes `accuracytest_report.txt` (or the given path) in the
current directory. When started from a console it also prints progress and
the summary line.

Truly headless and config-safe, so it is safe to run in CI on a fresh machine.
The hook sets `CColorHCFRConfig::s_bHeadless` **before** constructing the
config, so the constructor never reads or writes the user's real ini, never
migrates/seeds `%APPDATA%\color`, and never pops the modal language/help
pickers (which would otherwise hang a headless run on a box that has never
launched HCFR interactively). All profile traffic lands on a throwaway scratch
ini in `%TEMP%`, and the process exits via `ExitProcess` before any
settings-saving teardown.

## The matrix

Full cross product where meaningful (~800 combos):

- **Grid (bit depth x range)**: 8-bit limited (219), 8-bit full (255),
  10-bit limited (876), 10-bit full (1023) — via the cached
  `GetUse10bitLevels()` / `GetRGB16_235()` flags.
- **Color space**: HDTV (Rec.709), sRGB, UHDTV (P3), UHDTV2 (BT.2020),
  UHDTV3 (P3-in-2020), UHDTV4 (709-in-2020), HDTVa (75%), HDTVb (plasma).
  CC6 is marked unused in the enum and is not UI-selectable — excluded.
- **Transfer function**: power 2.2, BT.1886, L*, PQ (tone map OFF and ON /
  BT.2390), HLG. sRGB-the-EOTF is exercised through the sRGB color space
  (every consumer forces mode 99 for it), so that space runs one entry.
  Mode 1 (SDR black compensation) only reshapes the gray target and mode 8
  is not UI-reachable — excluded.
- **White point**: D65, DCI white, and manual xy 0.3067/0.3180 (`DCUST`).
  The custom whites were never hand-tested; the known-fail table below
  documents where the references are still D65-bound.
- **Window intensity**: 100% always; 90% additionally for the SDR transfer
  functions (the generator UI disables Intensity for PQ/HLG). Intensity is
  deliberately NOT modeled by the references — it dims the measured white
  anchor equally and cancels in the white-relative dE. The 90% combos test
  that *cancellation*: the harness dims exactly the patches GDIGenerator
  dims (MT_PRIMARY / MT_SECONDARY / MT_SAT_*, **including** the PrimeWhite
  anchor; grayscale MT_IRE and CC patches are not dimmed).

Patch families per combo, built with the same construction code the measure
loops use, in the real measurement order (grayscale first — OnOffWhite/Black
come from the ramp ends and the HLG/BT.1886 reference decodes use the
measured gray top as White; then primaries, which set PrimeWhite):

- `gray` — 11-point ramp via `GetGrayPercent` (black column carries no dE in
  the grid and is skipped the same way here).
- `prim` — primaries + secondaries + white row, mirroring `MeasurePrimaries`'
  GenColors (incl. the UHDTV3/4 `ContainerPrimaryLinear` chain and the
  HDTVa/b HDR re-encode).
- `sat100` / `sat75` — six saturation sweeps at 100% and 75% stimulus via
  `GenerateSaturationColors`.
- `ccGCD` / `ccAXIS` — color-checker sets GCD (24) and AXIS (71 patches,
  indices 0..70) via `GenerateCC24Colors`. AXIS covers the clipped-ramp-top
  cases in PQ: patches above `m_TargetMaxL` must STILL read dE ~ 0 because
  the reference models the same per-channel clip.
- `convPV` — pane-vs-viewer dE **convention equality** (PQ mode-5 combos
  only; `-1.000` = not run). The wire-model families read dE ~ 0 under *any*
  self-consistent normalization, so they cannot see a convention split. This
  family perturbs each sat/CC measurement (fixed asymmetric XYZ gains, a few
  dE of luminance + chroma error) and asserts the measures-grid formula and
  the 3D-viewer formula produce the **same** dE. It guards three axes that
  drifted apart historically: the HDR reference **rescale**, the **white**
  the dE normalizes by (`CMeasure::GetColorDEWhiteY` — the measured white, not
  the theoretical `TmDiffuseWhiteNits`), and the **dE evaluation space**
  (transport space for the UHDTV3/4 pseudo-spaces). Because it also
  recomputes the white through the shared helper, it fails if that helper
  drifts from the grid's own YWhite selection. The ideal sensor's prime white equals
  `TmDiffuseWhiteNits` exactly, so matched conventions agree to machine
  precision — hence a dedicated **0.005** tolerance: the legacy fixed
  `105.95640` pane rescale reads 0.015–0.023 here (small in absolute dE
  because a common normalizer shift largely cancels inside a dE
  difference), which the default 0.05 would let back in.
  **Known blind spot:** the ideal sensor's measured white lands exactly on
  `TmDiffuseWhiteNits`, so a *white-source* divergence is invisible to this
  harness — the viewer's old theoretical-white normalization passed convPV
  while reading ~1% off the grid on a real display whose white missed target
  by 3.5% (found by hand, grid 8.80 vs viewer 8.72). The guard against that
  class is structural, not numerical: both sides now call
  `CMeasure::GetColorDEWhiteY`, so there is one definition to drift.
  Tone-map-ON combos run at `TargetMaxL = 300` nits — BT.2390's
  knee (`KS = 1.5*PQ(Lmax) - 0.5`) then falls below the 50.23%
  diffuse-white code, so tone mapping actually compresses diffuse white and
  a convention split becomes visible (at 700 nits the knee sits ~245 nits
  and diffuse white passes through untouched). HDTVa/b are excluded: the
  grid normalizes their sat/CC dE to the measured ON/OFF (peak) white while
  the viewer uses the tone-mapped diffuse white — a separate legacy
  divergence.

## Reading the report

One line per combo with the worst dE per family; `*` marks over-tolerance
(FAIL), `#` marks over-tolerance but a documented KNOWN-FAIL *within its
ceiling*. A detail block lists every failing family with its worst patch, and
known-fail entries print their code-level reason.

Known-fails **do not mask regressions**. Each `kKnownFails` entry carries a
`ceiling`: a matching combo is only downgraded to KNOWN-FAIL while its worst dE
stays at or below that ceiling. If the known gap *grows* past the ceiling the
line is reported as `FAIL(>ceil)` and the run fails — a widening known gap is a
regression the entry must not swallow. Ceilings sit above the first-full-run
worst per class with headroom; the broad full-range-gray entry is deliberately
tight (2.0 vs an observed 0.39) so it cannot absorb a real grayscale/EOTF
regression.

A known-fail entry that never fires anywhere in a run is **stale** (a combo was
dropped, or the issue was fixed and the entry now sits ready to mask a new
nearby regression). Stale entries print a warning **and fail the run** (nonzero
exit) — remove them from `kKnownFails` in `AccuracyTest.cpp`. The exit code is
nonzero if there is any real FAIL **or** any stale entry.

Tolerances: **0.05** for exact-model combos (SnapToVideoGrid's 1e-9
tie-breaker means no extra slack is needed). The Intensity-90% combos get
**0.9** (power law) / **1.5** (BT.1886, L*, sRGB): the cancellation is exact
only for an ideal power law — the dimmed code snaps to a different grid point
than the undimmed one, and non-power EOTFs break the ratio identity — so a
bounded, level-dependent residual is expected behavior, not a modeling error
(measured ceilings ~0.8 and ~1.2 across the full matrix; worst on 8-bit dark
saturation steps).

Harness dE settings (fixed, independent of the user's config): `dE_form=3`
(CIE2000), `dE_gray=1` (gamma-predicted gray luminance target — the app's
default `dE_form=5`/`dE_gray=2` substitutes the *measured* luminance into the
gray reference, which would blind the test to EOTF regressions), `gw_Weight=0`,
`m_TargetMinL=0` (the references target ideal black; the sensor's nonzero
floor is a display property, not a wire property).

## Fixes that came out of the first full run

The first matrix run surfaced three reference-vs-wire gaps that were fixed in
`Measure.cpp` (the reference must model the wire):

- `GetRefCC24Sat` had no branch for mode 99 (the sRGB standard): CC
  references were left unquantized and 2.22-decoded while the sensor/gray
  targets decode with the sRGB curve (up to ~5 dE on dark AXIS steps).
- `GetRefSat` applied the `YLuma * tmWhite / 94.37844` 100%-saturation
  convention even at reduced stimulus, where the quantize path opens and the
  generator encodes the plain K-luma color — the reference landed a code away
  (up to ~1.2 dE). Now gated on `stimLevel >= 1`.
- `WireModeledPrimaryReference` re-encoded the analog HDTVa/b colors while
  the wire sends 2-decimal-rounded hardcoded tables (~0.4 dE); it now decodes
  the actual wire codes (in HDTV space, HDTVa normalized to its 75% white
  anchor).

## Known failures

Kept in `kKnownFails` in `AccuracyTest.cpp`, each with the code-level reason
and (where relevant) a grid filter. An entry that never fires across a whole
run is reported as STALE. Current entries (expected, pre-existing):

- **UHDTV3/UHDTV4 with custom whites** (`prim`, `sat*`): `GetRefSat`'s sweep
  endpoints are hardcoded D65 xy tables (`p3Ref`/`p3sRef`/`rRef`/`rsRef`,
  Measure.cpp ~6955-6977), while the wire patches follow the active white via
  `ContainerPrimaryLinear`.
- **HDTVa/HDTVb with custom whites** (`gray`, `prim`, `sat*`): the wire
  tables, `pRef`/`sRef` endpoints, the simulated sensor's decode space and
  the dE space are all fixed Rec.709/D65 constructions, while gray/sat
  reference targets follow the active white. (CC passes — both sides share
  the decode chain.)
- **HDTVa/HDTVb primaries under PQ/HLG**: `WireModeledPrimaryReference`
  deliberately keeps the legacy ANALOG reference for modes 5/7 while the
  wire re-encodes the 75% tables with the active EOTF (dE ~70 on blue).
- **HDTVa/HDTVb PQ `sat75` on 8-bit limited only**: the PQ 50% anchor is
  exactly code 110/219, so 0.75 stim = an exact half-code (82.5); generator
  and reference reach it through different matrix round trips whose sub-1e-9
  dust splits the tie (constant dE 0.74 at yellow).
- **`gray` on the full-range grids**: the gray pipeline (`GetGrayPercent` /
  `ArrayIndexToGrayLevel` / `GrayLevelToGrayProp`) has no range parameter —
  ramp codes and references live on the 219/876 limited grids and the
  full-range re-snap to 255/1023 is unmodeled (≤ ~0.4 dE, worst at the PQ
  8-bit knee). Fixing requires a range parameter through libHCFR, which
  moves ColorMathTest T2/T3 goldens — out of scope here.

## Coverage gaps — what this harness structurally cannot see

Known-fails above are *modeling* gaps the harness detects and tolerates. The
gaps below are different and more dangerous: configurations and code paths the
harness never reaches, so a real dE bug in them reads **green**. Every dE bug
found by hand during the 2026-07 HDR rescale unification lived in one of these.

**The root limitation.** The core assertion is "a perfect display measures its
own reference, dE ~ 0". That invariant holds under *any* self-consistent
normalization, so it cannot distinguish two conventions that disagree — it only
catches a reference that stops modeling the wire. The `convPV` family exists to
cover exactly that hole (perturb the measurement, require two formulas to
agree), and the gaps below are mostly places `convPV`'s reach stops.

1. **The ideal sensor's white is exactly `TmDiffuseWhiteNits`.** The 50.22831%
   wire code and the helper's snapped `0.5022283` land on the same grid code, so
   measured white == theoretical white *by construction*. Any divergence in
   which white a consumer normalizes by is therefore invisible. This is how the
   3D viewer shipped a ~1% dE offset vs the grid that only appears on a display
   whose white misses target (found by hand: grid 8.80 vs viewer 8.72).
   *Needs:* a combo axis that perturbs the measured white away from the
   theoretical one (a gain error injected into the white patches only).
2. **The manual generator (DVD) is never configured.** `RunAccuracyTest` sets
   `enumAutomatic` unconditionally, so every `if (DVD)` carve-out — in
   `GetItemText`, `UpdateGrid`, `InitGrid`, `RGBLevelWnd`, and the Export
   blocks — is untested. Two real bugs hid there.
3. **Mascior HDR CC sets are never run.** `RunCC` is only called with `GCD` and
   `AXIS`; the whole `m_CCMode in [MASCIOR50..CCMAXHDR]` family (its `* 100`
   reference scale and grayscale-top white) has zero coverage in the harness,
   in `GetColorDEWhiteY`, and in the viewer.
4. **`bSpecial` (HDTVa/HDTVb) is excluded from `convPV`.** The exclusion
   predates the white unification and its stated rationale is now stale — the
   viewer passes `satSpecial` today, so these could be covered.
5. **Whites are always present.** The run order is always gray → primaries →
   sat/CC, so `PrimeWhite`/`OnOffWhite` are always valid and the grid's
   `m_TargetMaxL` fallback path is never exercised.
6. **The display/comparator layer is not modeled at all.** `CMainView::OnUpdate`'s
   `Yref` formulas, the RGB-levels bars and the target widget do dE-adjacent
   math on `m_RefColor`/`m_RefWhite`/`m_YWhite`. A wrong factor there is
   user-visible (swatches disagree while dE reads 0) and completely invisible
   here.
7. **`convPV` compares two harness-local copies.** It *emulates* the viewer's
   formula rather than calling it, so changing `C3DColorView::BuildScene`
   without editing `AccuracyTest.cpp` still reads 0.000. Only the white is
   genuinely shared (`CMeasure::GetColorDEWhiteY`); the reference scale and the
   dE evaluation space are re-derived. Extracting the whole normalization into
   one `CMeasure` helper that the grid, the viewer and the harness all call
   would make the lock real.
8. **Export is never exercised.** The PDF/CSV/spreadsheet dE paths duplicate the
   grid's normalization and have already drifted from it.
9. **Hard-clip coverage is tone-map-OFF only.** Tone-map-ON combos run at
   `TargetMaxL = 300`, where BT.2390 rolls off instead of clipping, so the
   "clipped patches still read dE ~ 0" invariant is only checked on the
   tone-map-OFF half of the matrix.

Reference result (2026-07-13, first full run after the fixes):
`708 combos: 311 pass, 0 FAIL, 397 known-fail`, ColorMathTest verify green
with no golden movement.
