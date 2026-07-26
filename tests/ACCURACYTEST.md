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

It also runs a second, **differential** kind of check — the `conv*` families —
that asks whether two *consumers* (the measures grid and the 3D viewer) agree
about a dE convention. The dE ~ 0 invariant structurally cannot answer that; see
[The convention families](#the-convention-families-convpv--convpvw--convnw).

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

Headless: no windows, no generators, no real measure loops. The run takes a
few minutes (~5-6 on a current desktop; it evaluates on the order of a million
`GetDeltaE` calls) and writes `accuracytest_report.txt` (or the given path) in
the current directory. When started from a console it also prints per-EOTF
progress and the summary line.

Truly headless and config-safe, so it is safe to run in CI on a fresh machine.
The hook sets `CColorHCFRConfig::s_bHeadless` **before** constructing the
config, so the constructor never reads or writes the user's real ini, never
migrates/seeds `%APPDATA%\color`, and never pops the modal language/help
pickers (which would otherwise hang a headless run on a box that has never
launched HCFR interactively). All profile traffic lands on a throwaway scratch
ini in `%TEMP%`, and the process exits via `ExitProcess` before any
settings-saving teardown.

## The matrix

Full cross product where meaningful (876 combos):

- **Grid (bit depth x range)**: 8-bit limited (219), 8-bit full (255),
  10-bit limited (876), 10-bit full (1023) — via the cached
  `GetUse10bitLevels()` / `GetRGB16_235()` flags.
- **Color space**: HDTV (Rec.709), sRGB, UHDTV (P3), UHDTV2 (BT.2020),
  UHDTV3 (P3-in-2020), UHDTV4 (709-in-2020), HDTVa (75%), HDTVb (plasma).
  CC6 is marked unused in the enum and is not UI-selectable — excluded.
- **Transfer function**: power 2.2, BT.1886, L*, PQ, PQ + BT.2390 tone mapping
  at **two** display targets (`TargetMaxL` 300 and 700 nits), HLG.
  sRGB-the-EOTF is exercised through the sRGB color space (every consumer
  forces mode 99 for it), so that space runs one entry. Mode 1 (SDR black
  compensation) only reshapes the gray target and mode 8 is not UI-reachable —
  excluded.
  The two tone-mapped rows sit on opposite sides of BT.2390's knee
  (`KS = 1.5*PQ(Lmax) - 0.5`): at 300 nits the knee falls **below** the 50.23%
  diffuse-white code, so tone mapping actually compresses diffuse white and the
  HDR reference-rescale / white-anchor conventions get real coverage; at 700
  nits diffuse white passes through untouched and the roll-off instead lands
  among the AXIS patches that hard-clip with tone mapping off. Before the
  700-nit row existed, the "patches above `m_TargetMaxL` still read dE ~ 0"
  invariant was only ever checked with tone mapping **off**.
- **White point**: D65, DCI white, and manual xy 0.3067/0.3180 (`DCUST`).
  The custom whites were never hand-tested; the known-fail table below
  documents where the references are still D65-bound.
- **Generator**: automatic (GDI) always; **manual (DVD)** additionally for the
  three PQ rows at D65. Every `if (DVD)` carve-out in `GetItemText` /
  `UpdateGrid` sits inside the mode-5 HDR block, and the carve-outs swap a
  diffuse-white *constant* rather than a chromaticity, so PQ-at-D65 reaches all
  of them without tripling the matrix.
  DVD combos score **only** the `conv*` columns; the wire families print
  `-1.000`. HCFR emits no patches at all with a manual generator — the "wire" is
  the disc the user plays, and the DVD conventions are built for the Mascior
  disc's 50.0% / 92.254965-nit white rather than the 50.22831% code the
  simulated sensor is fed, so `reference == wire` has no meaning there. The
  families still *run* (they bootstrap the measured whites and collect the
  convention samples), they are just not scored.
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

### The convention families (`convPV` / `convPVw` / `convNW`)

The wire-model families above read dE ~ 0 under *any* self-consistent
normalization, so they cannot see two **consumers** disagreeing about a
convention — only a reference that stopped modeling the wire. Every dE bug that
reached a user during the 2026-07 HDR rescale unification was of the first kind
and left all 708 combos green.

These three families close that hole with a **differential** check instead of a
zero check: perturb the measurement (fixed asymmetric XYZ gains, a few dE of
luminance + chroma error), then require the measures-grid formula and the
3D-viewer formula to produce the **same** dE. They guard the three axes that
drifted apart historically — the HDR reference **rescale**, the **white** the dE
normalizes by, and the **dE evaluation space** (transport space for the UHDTV3/4
pseudo-spaces) — and because the viewer side calls `CMeasure::GetColorDEWhiteY`
while the grid side is fed the harness's own `UpdateGrid` emulation, they also
fail if that shared helper drifts from the grid's YWhite selection.

They run on **every** combo, SDR included (in SDR the check reduces to that
white-selection lock, which is where the CC "primaries run below 90% stimulus"
fallback lives), and now include HDTVa/HDTVb — the old exclusion described a
viewer that no longer exists, and re-enabling it turned out to need only that
`bSpecial` be threaded through to `GetColorDEWhiteY`.

- `convPV` — nominal measured state.
- `convPVw` — the same comparison with the three stored whites (prime, ON/OFF,
  grayscale top) scaled **off target and off each other** (0.965 / 0.982 /
  0.973). This is the family that would have caught the shipped bug. With the
  ideal sensor every candidate white — prime, ON/OFF, gray top, theoretical
  `TmDiffuseWhiteNits` — has the *same* value by construction (the 50.22831%
  wire code and the helper's snapped `0.5022283` reach the same grid code on all
  four grids), so a consumer normalizing by the wrong one is invisible; that is
  exactly how the viewer shipped a ~1% offset against the grid, visible only on
  a display whose white misses target (found by hand: grid 8.80 vs viewer 8.72).
  The perturbation is applied to the **stored** whites, not to the sensor: both
  consumers read the same `CMeasure`, so a differential check only needs the
  state to differ, and three *different* factors make the whites mutually
  distinguishable. A real display cannot have its grayscale top disagree with
  its ON/OFF white — that is deliberate, and it is why this family asserts only
  the differential, never dE ~ 0.
- `convNW` — a saturation sweep on a pristine `CMeasure`: **no** grayscale and
  **no** primaries, so both whites are invalid and the grid falls through to its
  `m_TargetMaxL` fallback (MainView.cpp ~3699-3706), which the always-ordered
  gray → primaries → sat/CC matrix never reaches. A user who measures a
  saturation sweep first lands exactly here. The wire-model dE is meaningless in
  this state (the references are normalized by a white that was never measured),
  so this family also asserts only the differential — and that nothing produces
  a NaN, which is what an unguarded divide by a missing white looks like.

All three share a dedicated **0.005** tolerance. Matched conventions agree to
machine precision (identical `GetDeltaE` arguments), while the divergences they
exist to catch are small in *absolute* dE because a common normalizer shift
largely cancels inside a dE difference: the legacy fixed `105.95640` pane
rescale reads 0.015–0.023, and the pre-unification theoretical-white viewer
reads 0.046 on `convPVw`. The default 0.05 tolerance would let **both** back in.

Verified non-vacuous by mutation: restoring the pre-unification viewer white
(`TmDiffuseWhiteNits` instead of `GetColorDEWhiteY`) fails 42 combos on
`convPVw` and 42 on `convNW`, where `convPV` alone catches only 12 — and 18 of
those are the HDTVa/b combos the family used to skip entirely.

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
tie-breaker means no extra slack is needed), **0.005** for the three
convention families (see above). The Intensity-90% combos get
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
- **Manual generator (DVD), all `conv*` families**: a real, newly-detected
  **grid-vs-3D-viewer divergence**, not a modeling gap — the first thing the DVD
  axis found. `GetItemText` keeps the legacy DVD conventions for mode 5 (the
  Mascior disc's 92.254965-nit white instead of `TmDiffuseWhiteNits`, the fixed
  `105.95640` rescale instead of `GetHDRRefScale`, and in its second sub-branch
  an extra `YWhite * 94.37844 / tmWhite`), while `C3DColorView::BuildScene` has
  **no** manual-generator branch and always uses the unified pair. A user
  measuring from a disc therefore sees one dE in the pane and another in the 3D
  viewer. Two magnitudes, matching the two sub-branches: 0.49–0.70 where the
  measured white is left alone (saturation modes for HDTV/UHDTV, and UHDTV2's
  last saturation column), 0.02–0.07 where the `94.37844/tmWhite` rescale very
  nearly cancels the reference offset. Ceilings are split (1.5 / 0.3) so a
  regression in the near-cancelling half cannot hide under the other's headroom.
  Recorded rather than fixed because the 2026-07 unification *deliberately* left
  DVD legacy, and closing it means choosing which convention wins — a
  user-visible number change on a legacy path. The structural fix is coverage
  gap 7's shared `CMeasure` normalization helper: if the viewer, Export and
  RGBLevelWnd all ask `CMeasure` for the normalization, the DVD carve-out lives
  in exactly one place and this divergence cannot exist.
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
catches a reference that stops modeling the wire. The `conv*` families exist to
cover exactly that hole (perturb the measurement, require two formulas to
agree), and the gaps below are mostly places their reach stops.

### Closed

1. ~~**The ideal sensor's white is exactly `TmDiffuseWhiteNits`.**~~ Closed by
   `convPVw`, which scales the three stored whites off target and off each
   other. Mutation-verified: the pre-unification viewer white fails 42 combos
   there that `convPV` alone lets through.
4. ~~**`bSpecial` (HDTVa/HDTVb) is excluded from `convPV`.**~~ Closed — the
   exclusion needed only `bSpecial` threaded through to `GetColorDEWhiteY`.
   Those 18 combos now carry the largest signal in the mutation test (3.4 dE).
5. ~~**Whites are always present.**~~ Closed by `convNW`.
9. ~~**Hard-clip coverage is tone-map-OFF only.**~~ Closed by the second
   tone-mapped EOTF row at `TargetMaxL = 700`.
2. ~~**The manual generator (DVD) is never configured.**~~ Closed by the
   generator axis — and it immediately found a real grid-vs-3D-viewer
   divergence, now the DVD known-fail entry above. The `if (DVD)` carve-outs in
   `InitGrid`, `RGBLevelWnd` and Export are still not *directly* covered: they
   duplicate the same normalization, which is gap 7's problem, not a separate
   axis.

### Still open

3. **Mascior HDR CC sets are never run.** `RunCC` is only called with `GCD` and
   `AXIS`; the whole `m_CCMode in [MASCIOR50..CCMAXHDR]` family (its `* 100`
   reference scale and grayscale-top white) has zero coverage in the harness,
   in `GetColorDEWhiteY`, and in the viewer. Every set in that range reads its
   patch list from a CSV in `%APPDATA%\color\`, so any coverage here has to be
   gated on the file loading and cannot be assumed present in CI.
6. **The display/comparator layer is not modeled at all.** `CMainView::OnUpdate`'s
   `Yref` formulas, the RGB-levels bars and the target widget do dE-adjacent
   math on `m_RefColor`/`m_RefWhite`/`m_YWhite`. A wrong factor there is
   user-visible (swatches disagree while dE reads 0) and completely invisible
   here. This is MFC view code; extracting the pure math into a testable
   function is the prerequisite.
7. **The `conv*` families compare two harness-local copies.** They *emulate* the
   viewer's formula rather than calling it, so changing `C3DColorView::BuildScene`
   without editing `AccuracyTest.cpp` still reads 0.000. Only the white is
   genuinely shared (`CMeasure::GetColorDEWhiteY`); the reference scale and the
   dE evaluation space are re-derived. Extracting the whole normalization into
   one `CMeasure` helper that the grid, the viewer, Export and the harness all
   call would make the lock real.
8. **Export is never exercised.** The PDF/CSV/spreadsheet dE paths duplicate the
   grid's normalization and have already drifted from it. Gap 7's shared helper
   is the better fix than an export family: if Export calls it, there is nothing
   left to diverge.

Reference result (2026-07-25): `876 combos: 333 pass, 0 FAIL, 543 known-fail`,
ColorMathTest verify green with no golden movement. (Was `708 combos: 311 pass,
0 FAIL, 397 known-fail` before this work: +84 tone-mapped 700-nit combos and +84
manual-generator combos, the latter all landing in the new DVD known-fail.
On the GDI half the three convention families read 0.000 across the whole
matrix.)
