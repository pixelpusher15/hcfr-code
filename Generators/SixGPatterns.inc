// SixGPatterns.inc - Murideo SIX-G (GEN2, 8K metal "SGH20") built-in pattern catalog.
//
// SCAFFOLD - PRE-HARDWARE. Every id below is a CANDIDATE, not a confirmed wire value.
// Nothing #includes this file yet; it is the seed of the SIX-G generator integration and
// exists so the hardware bring-up session only has to fill in / confirm the id column.
//
// Row format matches the SEVEN-G table (Generators/GDIGenerator.cpp): { group, name, id, ber }.
// The SIX-G addresses a built-in pattern with the 1-byte command 0x0062 (Change Pattern),
// so the wire id is a plain byte (no BER bank composition - ber stays 0). Read it back with
// 0x8062. See the SIX-G UART command doc, Table-5, and the GEN2 8K manual, menu 02.
//
// Provenance / confidence, by group:
//   FPGA 0-35   - HIGH. The SEVEN-G v2.04 spec's FPGA ids (hardware-verified, in
//                 MuriPatterns.inc) and the GEN2 8K manual's front-panel FPGA menu (02-1)
//                 agree position-for-position. Names below follow the MANUAL (it corrects
//                 three stale names in the SEVEN inc: id 8 Magenta not "Cyan", id 12
//                 "Vertical Split" not "Vertical Screen", id 35 "Moving Ball" not "Ramp").
//   ISF  36-47  - MEDIUM. Same 12 patterns (3 UHD ISF + 9 HD ISF) as the SEVEN's ISF block
//                 at ids 36-47; confirm the ids and the manual's spelling on hardware.
//   HDR         - UNKNOWN. The manual lists 3 HDR test patterns + 1 reference image; it
//                 gives no ids. id = -1 until read off hardware.
//   Dolby Vision- UNKNOWN. Manual lists 2; no ids. id = -1 until hardware.
//
// VERIFY ON HARDWARE (rig: scratchpad sixg_patternrig): sweep 0x0062 0..N, read back 0x8062,
// and record what the panel actually shows. Do NOT trust the old SGH20 V1.11 Table-5 ordering
// - that is an earlier firmware and disagrees with this GEN2 set.

// ---- FPGA (0x0062 ids 0-35; candidate, corroborated) ----
	{ "FPGA", "100% Color Bars",    0, 0 },
	{ "FPGA", "75% Color Bars",     1, 0 },
	{ "FPGA", "8 Step Gray Bars",   2, 0 },
	{ "FPGA", "16 Step Gray Bars",  3, 0 },
	{ "FPGA", "Red Screen",         4, 0 },
	{ "FPGA", "Green Screen",       5, 0 },
	{ "FPGA", "Blue Screen",        6, 0 },
	{ "FPGA", "Cyan Screen",        7, 0 },
	{ "FPGA", "Magenta Screen",     8, 0 },
	{ "FPGA", "Yellow Screen",      9, 0 },
	{ "FPGA", "Black Screen",      10, 0 },
	{ "FPGA", "White Screen",      11, 0 },
	{ "FPGA", "Vertical Split",    12, 0 },
	{ "FPGA", "Horizontal Split",  13, 0 },
	{ "FPGA", "Multiburst Ver",    14, 0 },
	{ "FPGA", "Multiburst Hor",    15, 0 },
	{ "FPGA", "Quarter Block 1",   16, 0 },
	{ "FPGA", "Quarter Block 2",   17, 0 },
	{ "FPGA", "Alternate W.B.",    18, 0 },
	{ "FPGA", "RGB CMY Ramps",     19, 0 },
	{ "FPGA", "Black Pluge",       20, 0 },
	{ "FPGA", "White Pluge",       21, 0 },
	{ "FPGA", "Still Gray Ramps 1",22, 0 },
	{ "FPGA", "Still Gray Ramps 2",23, 0 },
	{ "FPGA", "SMPTE Bars",        24, 0 },
	{ "FPGA", "Border Lines",      25, 0 },
	{ "FPGA", "Window",            26, 0 },
	{ "FPGA", "3D Boxes",          27, 0 },
	{ "FPGA", "Lines V. Scroll",   28, 0 },
	{ "FPGA", "Lines H. Scroll",   29, 0 },
	{ "FPGA", "A/V Sync",          30, 0 },
	{ "FPGA", "Gray Ramp",         31, 0 },
	{ "FPGA", "Red Ramp",          32, 0 },
	{ "FPGA", "Green Ramp",        33, 0 },
	{ "FPGA", "Blue Ramp",         34, 0 },
	{ "FPGA", "Moving Ball",       35, 0 },

// ---- ISF (0x0062 ids 36-47; candidate, confirm on HW) ----
	{ "ISF", "Black Pluge UHD",    37, 0 },  // manual: UHD ISF (2160p30)
	{ "ISF", "White Pluge UHD",    36, 0 },
	{ "ISF", "Geometry UHD",       38, 0 },
	{ "ISF", "Black Pluge HD",     40, 0 },  // manual: HD ISF (1080p60)
	{ "ISF", "White Pluge HD",     39, 0 },
	{ "ISF", "Geometry 178 HD",    41, 0 },
	{ "ISF", "Geometry 240 HD",    42, 0 },
	{ "ISF", "Cone Gradiants",     46, 0 },
	{ "ISF", "ISF Color Girls",    43, 0 },
	{ "ISF", "PD Family",          44, 0 },
	{ "ISF", "ISF Dob",            47, 0 },
	{ "ISF", "Red Blue MTB",       45, 0 },

// ---- HDR (native 1920x1080@60 4:2:2 12bit 1000nits; ids UNKNOWN - read on HW) ----
	{ "HDR", "Black Clipping",     -1, 0 },
	{ "HDR", "White Clipping",     -1, 0 },
	{ "HDR", "Color Clipping",     -1, 0 },
	{ "HDR", "Indian Market (ref)",-1, 0 },

// ---- Dolby Vision (ids UNKNOWN - read on HW) ----
	{ "Dolby Vision", "Dolby Vision FHD", -1, 0 },
	{ "Dolby Vision", "Dolby Vision UHD", -1, 0 },
