# ColorHCFR 4.0.0.0 — Release Notes

A major release rolling up instrument support, color-science, and a full UI/UX overhaul. (Supersedes the internal 3.5.5 work, which is included below.)

## Instrument & Color Engine
- **Updated ArgyllCMS to V3.5.0** (instrument drivers + color math), improving meter compatibility and reliability. Removed dead vendored code.
- **Custom white point for Rec2020/P3 containers** — UHDTV3 (P3-in-Rec2020) and UHDTV4 (Rec709-in-Rec2020) now fully support a custom white across every view (grayscale, primaries, secondaries, saturation sweeps, ColorChecker) in both SDR and HDR. D65 results are unchanged.
- **Disable Rev. B AIO mode for i1Display Pro** — new checkbox in the Argyll meter properties to turn off the i1d3 all-in-one measurement path.
- **"Avg low light" checkbox** replaces the old (buggy) Sensor-pane "Adaptive" button, and the setting now correctly persists across reconnects.

## Measurements
- **Grayscale level presets** — the step-count box is now a preset dropdown (5/6/11/12/16/21/25-point) plus a Custom field, with explicit per-point IRE levels enabling non-uniform near-black detail. Older `.chc` files still load.
- **New measurement summary header** — gamma, contrast, average dE, and luminance mode shown as rounded stat chips, always visible (with 0.00 placeholders before readings), with saturation-sweep avg/max now displayed correctly.

## Theme & Appearance
- **New Light/Dark theme** — a flat, VS-style dark recolor of the entire app (chrome, controls, grids, dialogs, menus), switchable on the Appearance page. Numerous dark-mode polish fixes (menu separators, borders, status-bar, submenu arrows, graph backgrounds).
- **Rebrand** — new HCFR logo, multi-size app icon (everywhere, including the `.chc` document and installer), updated splash screen, and refreshed toolbar/menu icons (themed Fluent PNGs that live-refresh on theme switch).

## UI Redesigns
- **References page** redesigned for a cleaner gamut / transfer-function layout.
- **Generator settings dialog** redesigned — the cluttered 6-way "Display mode" radio group is replaced by a single **"Pattern output" dropdown** that shows only the selected backend's options (Desktop, madVR/madTPG, Google Cast, PGenerator). Fixed a pre-existing Google Cast device-selection bug.

## Performance & Stability
- **Eliminated measurement flashing** — the data grids, group-box frames, and window no longer flicker during or after a measurement sweep.
- **Installer fixes** — now creates a Start Menu shortcut (so HCFR shows up in Start menu search), plus themed icons, splash, and English installer UI.
