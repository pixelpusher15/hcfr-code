# Murideo SIX-G generator - integration notes (WIP, stacked on PR #175)

Branch `six-g-generator`, based on `generators` (PR #175, "Add DVDO AVLab TPG and Murideo
Seven-G pattern generators"). This branch is a **pre-hardware scaffold**: it adds catalog
data and this note only. No C++ is wired yet, so there is nothing to build here until the
hardware bring-up settles the open questions below.

## What the docs already settle (see SIX-*.docx in the source docs)

- **Wire protocol is identical to the SEVEN-G.** `AA 00 00 <LEN> 00 00 00 <KWlo> <KWhi>
  <data...> <CKSUM>`, LEN = 5 + data, keyword low byte first, 2's-complement CheckSum8.
  Verified against the SIX-G manual's own example (`AA 00 00 06 00 00 00 61 00 00 EF`).
  `MuriBuildFrame` in GDIGenerator.cpp produces byte-identical frames.
- **Plastic == metal.** The 8K and 8KM command sets are identical; the doc diffs are only
  spelling. One code path covers both.
- **SIX-G is USB/serial only** (a USB-CDC COM port), no network. The SEVEN's HTTP-CGI and
  raw-TCP transports are unused here; the serial path (`MuriSerialOpen/WriteRaw/Xfer`,
  `MuriBuildFrame`, `MuriReadEdidBytes`) reuses as-is.
- **8-bit patches only.** The SEVEN's 0x008C 10/12-bit triplet does NOT exist on any SIX-G.
  Arbitrary patches use 0x006C (8 byte: R G B bgR bgG bgB Size Move) or 0x007C (15 byte,
  8-bit even/odd + Normal/DV-HDR mode). The device *outputs* 8-16 bit (Deep Color menu),
  but the triplet *input* is 8-bit, so the limit is input granularity (256 codes/channel),
  not near-black crush. => a new 8-bit `MuriCmdRgbTriplet` targeting 0x006C is the main new
  code, replacing the 0x008C 16-bit path.
- **Colour-space enum matches** the SEVEN cat-99 order: 0=RGB full, 1=RGB 16-235, 2=YC444,
  3=YC422, 4=YC420, 5=AUTO (0x0063). Set once; no per-patch re-assert (that trick was
  specific to 0x008C's last byte).
- **Pattern catalog** is in `SixGPatterns.inc`. FPGA (0-35) and ISF (36-47) ids are grounded
  (SEVEN v2.04 hardware-verified + GEN2 manual corroboration). HDR and Dolby Vision ids are
  unknown and marked -1.

## Open questions for the hardware session (rig answers these)

Rig: `<scratchpad>/sixg_patternrig/` - `python rig.py --port COMx`, then open the page it
prints. It sends 0x0062 pattern ids with Prev/Next, reads back 0x8062, sends an 0x006C test
triplet, and lets you record the observed pattern per id and export a corrected .inc block.

1. Confirm FPGA ids 0-35 and ISF ids 36-47 match the panel (spot-check a few, esp. id 8
   Magenta, id 12 Vertical Split, id 35 Moving Ball).
2. Find N (highest valid pattern id) and how the device answers an out-of-range id.
3. Capture the HDR (Black/White/Color Clipping, Indian Market) and Dolby Vision (FHD/UHD)
   ids - the four/two -1 rows.
4. Does 0x8062 read-back reliably return the current id (auto-detect feasible)?
5. Does the 0x006C 8-bit triplet render correct color + APL/window (sanity for the patch path)?

## After hardware

- Fill in `SixGPatterns.inc` ids from the rig export.
- Add the SIX-G patch path (0x006C 8-bit builder) + settings, mirroring the SEVEN's serial
  path minus HTTP/TCP. Integration approach (new DISPLAY_ mode vs a Seven/Six variant toggle
  on the existing Murideo page) is an open decision - the mode route touches resource.h and
  all five .rc satellites, so it needs the radio-order + warning-diff checks per CLAUDE.local.
