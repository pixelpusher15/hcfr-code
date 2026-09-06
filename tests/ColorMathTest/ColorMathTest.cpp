// ColorMathTest — 8-bit golden-master regression harness for libHCFR.
// See TEN-BIT-PIPELINE-PLAN.md (repo root), PR 1.
//
// Usage:
//   ColorMathTest gen    [goldenDir]   regenerate golden files (run ONLY on a known-good build)
//   ColorMathTest verify [goldenDir]   compare current libHCFR output against golden files
// goldenDir defaults to "golden" relative to the current directory.
// Exit code 0 = all pass, nonzero = mismatch/failure.
//
// T2/T3/T4/T6 dump deterministic tables and compare them exactly against checked-in goldens.
// Everything else is a self-contained oracle needing no golden file: T1 carries a frozen
// copy of the legacy quantizer, T5/T7/T8/T9 assert quantizer and generator invariants,
// T10 asserts gamut-basis consistency, T11 asserts BT.2390 tone-map monotonicity,
// T12 asserts the BT.2390 inverse LUT round-trips and never faults on an empty
// table, T17 asserts the PatchSequence generator and its CSV writer (round-tripped
// through the real ReadCsvPatchRows importer), T20 pins the .hps patch-sequence
// file dialect, its CSV imports/export, identities, roles, timing, repeats, the
// extended span and the three inset/power/repeat primitives, and T21 pins the
// ordering stage (load models, dwell, the window objective, Order, drift score).
// T13-T16, T18 and T19 are deliberately skipped here, held by in-flight branches:
// #183 display-model T13, csv-provenance T14, #185/#186 cube-lut T15/T16,
// wtw-reference T18, the #193 schema trailer T19. Run hcfr-bench\tnumbers.py
// before assigning any new T number.
// The .chc round-trip originally planned for the T7
// slot needs app-level linkage and is deliberately still not in this console harness.

#include <afx.h>
#include "../../libHCFR/Color.h"
#include "../../libHCFR/PatchSequence.h"
#include "../../libHCFR/PatchSequenceFile.h"
#include "../../libHCFR/PatchOrdering.h"

#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <algorithm>
#include <ctime>
#include <cstring>
#include <string>
#include <vector>
#include <direct.h>

static int g_failures = 0;

static void Fail(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("FAIL: ");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
    ++g_failures;
}

//////////////////////////////////////////////////////////////////////////
// T1 — quantizer oracle.
// Frozen verbatim copy of ColorRGBDisplay::ConvertPercentToBYTE as of the
// PR-1 baseline (libHCFR/Color.cpp:1037-1067). DO NOT UPDATE this copy when
// the live function is refactored — it IS the 8-bit contract.
//////////////////////////////////////////////////////////////////////////
static BYTE Legacy_ConvertPercentToBYTE(double percent, bool is16_235)
{
    double coef;
    double offset;

    if (is16_235)
    {
        coef = (235.0 - 16.0) / 100.0;
        offset = 16.0;
    }
    else
    {
        coef = 255.0 / 100.0;
        offset = 0.0;
    }

    int result((int)floor(offset + percent * coef + 0.5));

    if(result < 0)
    {
        return 0;
    }
    else if(result > 255)
    {
        return 255;
    }
    else
    {
        return (BYTE)result;
    }
}

static void CheckT1Value(double pct, bool r)
{
    BYTE expected = Legacy_ConvertPercentToBYTE(pct, r);
    BYTE actual = ColorRGBDisplay::ConvertPercentToBYTE(pct, r);
    if (expected != actual)
        Fail("T1 pct=%.17g range=%d: legacy=%d live=%d", pct, (int)r, (int)expected, (int)actual);
}

static void RunT1()
{
    printf("T1 quantizer oracle...\n");
    for (int r = 0; r < 2; ++r)
    {
        bool lim = (r != 0);
        // dense sweep 0..100 step 1e-4 (1,000,001 points)
        for (int i = 0; i <= 1000000; ++i)
            CheckT1Value(i * 1e-4, lim);
        // exact 8-bit grid points, both conventions
        for (int c = 0; c <= 255; ++c)
        {
            CheckT1Value(c / 2.55, lim);
            CheckT1Value(c * 100.0 / 219.0, lim);
        }
        // clamping / out-of-range
        static const double edges[] = { -100.0, -5.0, -0.0001, 0.0, 100.0, 100.0001, 105.0, 200.0 };
        for (int e = 0; e < sizeof(edges)/sizeof(edges[0]); ++e)
            CheckT1Value(edges[e], lim);
    }
}

//////////////////////////////////////////////////////////////////////////
// T1b — ConvertPercentToCode (added by PR 2 of the 10-bit plan):
// bits=8 must equal the frozen legacy oracle at every input; bits=10 is
// checked structurally (endpoints, clamps, monotonicity, limited grid).
//////////////////////////////////////////////////////////////////////////
static void RunT1b()
{
    printf("T1b ConvertPercentToCode...\n");
    for (int r = 0; r < 2; ++r)
    {
        bool lim = (r != 0);
        // bits=8 == legacy oracle, dense sweep + grid + clamps
        for (int i = 0; i <= 1000000; ++i)
        {
            double pct = i * 1e-4;
            int code = ColorRGBDisplay::ConvertPercentToCode(pct, lim, 8);
            BYTE expected = Legacy_ConvertPercentToBYTE(pct, lim);
            if (code != (int)expected)
                Fail("T1b8 pct=%.17g range=%d: legacy=%d code=%d", pct, r, (int)expected, code);
        }
        static const double edges[] = { -100.0, -5.0, 0.0, 100.0, 105.0, 200.0 };
        for (int e = 0; e < sizeof(edges)/sizeof(edges[0]); ++e)
        {
            int code = ColorRGBDisplay::ConvertPercentToCode(edges[e], lim, 8);
            if (code != (int)Legacy_ConvertPercentToBYTE(edges[e], lim))
                Fail("T1b8 edge pct=%.17g range=%d", edges[e], r);
        }

        // bits=10: monotonic non-decreasing over the sweep, codes in range
        int prev = -1;
        for (int i = 0; i <= 1000000; ++i)
        {
            int code = ColorRGBDisplay::ConvertPercentToCode(i * 1e-4, lim, 10);
            if (code < prev)
                Fail("T1b10 non-monotonic at pct=%.17g range=%d (%d -> %d)", i * 1e-4, r, prev, code);
            if (code < 0 || code > 1023)
                Fail("T1b10 out of range at pct=%.17g range=%d: %d", i * 1e-4, r, code);
            prev = code;
        }
    }

    // bits=10 endpoints
    if (ColorRGBDisplay::ConvertPercentToCode(0.0, false, 10) != 0)    Fail("T1b10 full 0%% != 0");
    if (ColorRGBDisplay::ConvertPercentToCode(100.0, false, 10) != 1023) Fail("T1b10 full 100%% != 1023");
    if (ColorRGBDisplay::ConvertPercentToCode(0.0, true, 10) != 64)    Fail("T1b10 limited 0%% != 64");
    if (ColorRGBDisplay::ConvertPercentToCode(100.0, true, 10) != 940) Fail("T1b10 limited 100%% != 940");
    // bits=10 clamps (full-range convention: clamp to 0..maxCode)
    if (ColorRGBDisplay::ConvertPercentToCode(-5.0, false, 10) != 0)     Fail("T1b10 full clamp low");
    if (ColorRGBDisplay::ConvertPercentToCode(200.0, false, 10) != 1023) Fail("T1b10 full clamp high");

    // limited 10-bit grid: exactly 877 distinct codes (64..940) over 0..100%
    {
        std::vector<char> seen(1024, 0);
        for (int i = 0; i <= 1000000; ++i)
            seen[ColorRGBDisplay::ConvertPercentToCode(i * 1e-4, true, 10)] = 1;
        int distinct = 0;
        for (int c = 0; c < 1024; ++c) distinct += seen[c];
        if (distinct != 877)
            Fail("T1b10 limited grid: %d distinct codes, expected 877", distinct);
        if (!seen[64] || !seen[940] || seen[63] || seen[941])
            Fail("T1b10 limited grid bounds wrong");
    }
}

//////////////////////////////////////////////////////////////////////////
// Golden-file plumbing. Tables are built as strings (LF line endings,
// %.17g doubles) and either written (gen) or compared exactly (verify).
//////////////////////////////////////////////////////////////////////////
static bool g_genMode = false;
static std::string g_goldenDir;

static void HandleTable(const char* name, const std::string& content)
{
    std::string path = g_goldenDir + "\\" + name;
    if (g_genMode)
    {
        FILE* f = fopen(path.c_str(), "wb");
        if (!f) { Fail("%s: cannot write %s", name, path.c_str()); return; }
        fwrite(content.data(), 1, content.size(), f);
        fclose(f);
        printf("wrote %s (%u bytes)\n", path.c_str(), (unsigned)content.size());
        return;
    }
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) { Fail("%s: missing golden file %s (run 'gen' on a known-good build)", name, path.c_str()); return; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<char> buf(len > 0 ? (size_t)len : 1);
    size_t got = fread(&buf[0], 1, (size_t)len, f);
    fclose(f);
    if ((long)got != len || (size_t)len != content.size() ||
        (len > 0 && memcmp(&buf[0], content.data(), (size_t)len) != 0))
    {
        // locate the first differing line for a useful message
        size_t line = 1, i = 0, n = content.size() < (size_t)len ? content.size() : (size_t)len;
        while (i < n && buf[i] == content[(unsigned)i]) { if (content[(unsigned)i] == '\n') ++line; ++i; }
        Fail("%s: mismatch vs golden at line %u (golden %ld bytes, current %u bytes)",
             name, (unsigned)line, len, (unsigned)content.size());
    }
    else
        printf("%s OK\n", name);
}

static void AppendF(std::string& s, const char* fmt, ...)
{
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(line, sizeof(line) - 1, fmt, ap);
    va_end(ap);
    line[sizeof(line) - 1] = 0;
    s += line;
}

//////////////////////////////////////////////////////////////////////////
// T2 — ArrayIndexToGrayLevel over all sizes x three rounding modes x both
// wire ranges. The range column selects the native code grid (219 limited /
// 255 full at 8 bits, 876 / 1023 at 10) exactly as SnapToVideoGrid does; the
// lim=1 rows are the historic values and must never move.
//////////////////////////////////////////////////////////////////////////
static void RunT2()
{
    printf("T2 grey-level tables...\n");
    std::string out;
    static const int sizes[] = { 2, 5, 6, 11, 12, 16, 21, 25, 101 };
    out += "# ArrayIndexToGrayLevel: size,mode,is16_235,index,%.17g\n";
    for (int s = 0; s < sizeof(sizes)/sizeof(sizes[0]); ++s)
    {
        int size = sizes[s];
        for (int mode = 0; mode < 3; ++mode)   // 0=normal, 1=roundDown, 2=10bit
        {
            bool rd  = (mode == 1);
            bool b10 = (mode == 2);
            for (int lim = 1; lim >= 0; --lim)
                for (int i = 0; i < size; ++i)
                    AppendF(out, "%d,%d,%d,%d,%.17g\n", size, mode, lim, i,
                            ArrayIndexToGrayLevel(i, size, rd, b10, lim != 0));
        }
    }
    HandleTable("T2_graylevels.txt", out);
}

//////////////////////////////////////////////////////////////////////////
// T3 — GrayLevelToGrayProp over a dense level sweep x three rounding modes x
// both wire ranges (see T2 on the range column).
//////////////////////////////////////////////////////////////////////////
static void RunT3()
{
    printf("T3 grey-level props...\n");
    std::string out;
    out += "# GrayLevelToGrayProp: mode,is16_235,i,%.17g, level = i*0.25\n";
    for (int mode = 0; mode < 3; ++mode)
    {
        bool rd  = (mode == 1);
        bool b10 = (mode == 2);
        for (int lim = 1; lim >= 0; --lim)
            for (int i = 0; i <= 400; ++i)      // 0..100 step 0.25
                AppendF(out, "%d,%d,%d,%.17g\n", mode, lim, i,
                        GrayLevelToGrayProp(i * 0.25, rd, b10, lim != 0));
    }
    HandleTable("T3_graylevelprop.txt", out);
}

//////////////////////////////////////////////////////////////////////////
// T4 — pattern-generation freeze: GenerateSaturationColors and the
// hardcoded (non-CSV) GenerateCC24Colors modes.
//////////////////////////////////////////////////////////////////////////
static void DumpTriplets(std::string& out, const ColorRGBDisplay* c, int n)
{
    for (int i = 0; i < n; ++i)
        AppendF(out, "%d,%.17g,%.17g,%.17g\n", i, c[i][0], c[i][1], c[i][2]);
}

static void RunT4()
{
    printf("T4 pattern generation...\n");
    std::string out;
    static const ColorStandard stds[] = { HDTV, UHDTV, UHDTV2, UHDTV3, UHDTV4 };
    static const bool combos[6][3] = {
        {true,false,false},{false,true,false},{false,false,true},
        {true,true,false},{false,true,true},{true,false,true} };
    static const int steps[] = { 4, 8, 10 };
    static const int eotfs[] = { 0, 5, 7 };

    out += "# GenerateSaturationColors(std,combo,steps,eotf) then triplets %.17g\n";
    for (int s = 0; s < sizeof(stds)/sizeof(stds[0]); ++s)
    {
        CColorReference ref(stds[s]);
        for (int c = 0; c < 6; ++c)
            for (int st = 0; st < sizeof(steps)/sizeof(steps[0]); ++st)
                for (int e = 0; e < sizeof(eotfs)/sizeof(eotfs[0]); ++e)
                {
                    std::vector<ColorRGBDisplay> buf(steps[st]);
                    GenerateSaturationColors(ref, &buf[0], steps[st],
                        combos[c][0], combos[c][1], combos[c][2], eotfs[e]);
                    AppendF(out, "SAT,%d,%d,%d,%d\n", (int)stds[s], c, steps[st], eotfs[e]);
                    DumpTriplets(out, &buf[0], steps[st]);
                }
    }

    out += "# GenerateCC24Colors(std,ccmode,eotf) hardcoded modes only\n";
    static const int ccmodes[]  = { GCD, MCD, CCSG, AXIS };
    static const int ccsizes[]  = { 24, 24, 96, 71 };	// AXIS = black + 7 ramps x 10
    // UHDTV3/UHDTV4 freeze the SDR inner->transport pseudo-space remap.
    static const ColorStandard ccstds[] = { HDTV, UHDTV2, UHDTV3, UHDTV4 };
    for (int s = 0; s < sizeof(ccstds)/sizeof(ccstds[0]); ++s)
    {
        CColorReference ref(ccstds[s]);
        for (int m = 0; m < sizeof(ccmodes)/sizeof(ccmodes[0]); ++m)
            for (int e = 0; e < 2; ++e)
            {
                int eotf = e ? 5 : 0;
                std::vector<ColorRGBDisplay> buf(96);
                bool ok = GenerateCC24Colors(ref, &buf[0], ccmodes[m], eotf);
                AppendF(out, "CC,%d,%d,%d,%d\n", (int)ccstds[s], ccmodes[m], eotf, ok ? 1 : 0);
                DumpTriplets(out, &buf[0], ccsizes[m]);
            }
    }

    // Full-range 8-bit variants (b10bit=false, is16_235=false): freezes the
    // native 255-grid quantization (PC levels), which the frozen limited rows
    // above do NOT exercise. Tags: SAT8F / CC8F.
    out += "# GenerateSaturationColors 8-bit full (std,combo,steps,eotf) then triplets %.17g\n";
    for (int s = 0; s < sizeof(stds)/sizeof(stds[0]); ++s)
    {
        CColorReference ref(stds[s]);
        for (int c = 0; c < 6; ++c)
            for (int st = 0; st < sizeof(steps)/sizeof(steps[0]); ++st)
                for (int e = 0; e < sizeof(eotfs)/sizeof(eotfs[0]); ++e)
                {
                    std::vector<ColorRGBDisplay> buf(steps[st]);
                    GenerateSaturationColors(ref, &buf[0], steps[st],
                        combos[c][0], combos[c][1], combos[c][2], eotfs[e], 1.0, false, false);
                    AppendF(out, "SAT8F,%d,%d,%d,%d\n", (int)stds[s], c, steps[st], eotfs[e]);
                    DumpTriplets(out, &buf[0], steps[st]);
                }
    }
    out += "# GenerateCC24Colors 8-bit full (std,ccmode,eotf) hardcoded modes only\n";
    for (int s = 0; s < sizeof(ccstds)/sizeof(ccstds[0]); ++s)
    {
        CColorReference ref(ccstds[s]);
        for (int m = 0; m < sizeof(ccmodes)/sizeof(ccmodes[0]); ++m)
            for (int e = 0; e < 2; ++e)
            {
                int eotf = e ? 5 : 0;
                std::vector<ColorRGBDisplay> buf(96);
                bool ok = GenerateCC24Colors(ref, &buf[0], ccmodes[m], eotf, false, false);
                AppendF(out, "CC8F,%d,%d,%d,%d\n", (int)ccstds[s], ccmodes[m], eotf, ok ? 1 : 0);
                DumpTriplets(out, &buf[0], ccsizes[m]);
            }
    }

    // Native 10-bit grid variants (b10bit=true), for BOTH output ranges:
    // limited/16-235 (876 grid) and full/PC (1023 grid). Freezes the range-aware
    // single-rounding native quantization; appended so the 8-bit rows stay
    // byte-identical. Tags: SAT10/CC10 (limited), SAT10F/CC10F (full).
    for (int rng = 0; rng < 2; ++rng)
    {
        bool lim = (rng == 0);
        const char* satTag = lim ? "SAT10" : "SAT10F";
        const char* ccTag  = lim ? "CC10"  : "CC10F";
        AppendF(out, "# GenerateSaturationColors 10-bit %s (std,combo,steps,eotf) then triplets %%.17g\n", lim ? "limited" : "full");
        for (int s = 0; s < sizeof(stds)/sizeof(stds[0]); ++s)
        {
            CColorReference ref(stds[s]);
            for (int c = 0; c < 6; ++c)
                for (int st = 0; st < sizeof(steps)/sizeof(steps[0]); ++st)
                    for (int e = 0; e < sizeof(eotfs)/sizeof(eotfs[0]); ++e)
                    {
                        std::vector<ColorRGBDisplay> buf(steps[st]);
                        GenerateSaturationColors(ref, &buf[0], steps[st],
                            combos[c][0], combos[c][1], combos[c][2], eotfs[e], 1.0, true, lim);
                        AppendF(out, "%s,%d,%d,%d,%d\n", satTag, (int)stds[s], c, steps[st], eotfs[e]);
                        DumpTriplets(out, &buf[0], steps[st]);
                    }
        }

        AppendF(out, "# GenerateCC24Colors 10-bit %s (std,ccmode,eotf) hardcoded modes only\n", lim ? "limited" : "full");
        for (int s = 0; s < sizeof(ccstds)/sizeof(ccstds[0]); ++s)
        {
            CColorReference ref(ccstds[s]);
            for (int m = 0; m < sizeof(ccmodes)/sizeof(ccmodes[0]); ++m)
                for (int e = 0; e < 2; ++e)
                {
                    int eotf = e ? 5 : 0;
                    std::vector<ColorRGBDisplay> buf(96);
                    bool ok = GenerateCC24Colors(ref, &buf[0], ccmodes[m], eotf, true, lim);
                    AppendF(out, "%s,%d,%d,%d,%d\n", ccTag, (int)ccstds[s], ccmodes[m], eotf, ok ? 1 : 0);
                    DumpTriplets(out, &buf[0], ccsizes[m]);
                }
        }
    }
    HandleTable("T4_patterns.txt", out);
}

//////////////////////////////////////////////////////////////////////////
// T5 — rPI (PGenerator) emission quantizers (added by PR 3): bits=8 must
// reproduce the legacy DisplayRGBColorrPI wire math exactly; bits=10 is
// checked structurally.
//////////////////////////////////////////////////////////////////////////
static void RunT5()
{
    printf("T5 rPI emission quantizers...\n");
    // bits=8 patch == frozen legacy formulas
    // sweep 0..120% so super-white (>100%) is covered: limited now clamps at the
    // code max (255), not legal white (235); full is unchanged.
    for (int i = 0; i <= 1200000; ++i)
    {
        double pct = i * 1e-4;
        int lgFull = (int)floor(pct / 100.0 * 255.0 + 0.5);
        lgFull = lgFull < 0 ? 0 : (lgFull > 255 ? 255 : lgFull);
        if (PiPercentToCode(pct, false, 8) != lgFull)
            Fail("T5 full8 pct=%.17g", pct);
        int lgLim = (int)floor(pct / 100.0 * 219.0 + 16.5);
        lgLim = lgLim < 0 ? 0 : (lgLim > 255 ? 255 : lgLim);
        if (PiPercentToCode(pct, true, 8) != lgLim)
            Fail("T5 lim8 pct=%.17g", pct);
    }
    // bits=8 background == frozen legacy formulas (v in the 0..255 domain)
    for (int i = 0; i <= 255000; ++i)
    {
        double v = i * 1e-3;
        if (PiBackground8ToCode(v, false, 8) != (int)v)
            Fail("T5 bgfull8 v=%.17g", v);
        if (PiBackground8ToCode(v, true, 8) != (int)floor(v / 255.0 * 219.0 + 16.5))
            Fail("T5 bglim8 v=%.17g", v);
    }
    // bits=10 endpoints and clamps
    if (PiPercentToCode(0.0, false, 10) != 0)      Fail("T5 full10 0%%");
    if (PiPercentToCode(100.0, false, 10) != 1023) Fail("T5 full10 100%%");
    if (PiPercentToCode(0.0, true, 10) != 64)      Fail("T5 lim10 0%%");
    if (PiPercentToCode(100.0, true, 10) != 940)   Fail("T5 lim10 100%%");
    if (PiPercentToCode(-5.0, false, 10) != 0)     Fail("T5 full10 clamp low");
    if (PiPercentToCode(200.0, false, 10) != 1023) Fail("T5 full10 clamp high");
    if (PiPercentToCode(200.0, true, 10) != 1023)  Fail("T5 lim10 clamp high");
    if (PiBackground8ToCode(0.0, true, 10) != 64)    Fail("T5 bglim10 0");
    if (PiBackground8ToCode(255.0, true, 10) != 940) Fail("T5 bglim10 255");
    if (PiBackground8ToCode(255.0, false, 10) != 1023) Fail("T5 bgfull10 255");
    // bits=10 monotonic + limited grid = 877 distinct codes
    for (int r = 0; r < 2; ++r)
    {
        bool lim = (r != 0);
        int prev = -1;
        std::vector<char> seen(1024, 0);
        for (int i = 0; i <= 1200000; ++i)
        {
            int code = PiPercentToCode(i * 1e-4, lim, 10);
            if (code < prev) Fail("T5 10bit non-monotonic range=%d pct=%.17g", r, i * 1e-4);
            prev = code;
            seen[code] = 1;
        }
        int distinct = 0;
        for (int c = 0; c < 1024; ++c) distinct += seen[c];
        if (lim && distinct != 960) Fail("T5 lim10 grid: %d distinct, expected 960", distinct);
        if (!lim && distinct != 1024) Fail("T5 full10 grid: %d distinct, expected 1024", distinct);
    }
}

//////////////////////////////////////////////////////////////////////////
// T6 — GetColorRef COLORREF packing.
//////////////////////////////////////////////////////////////////////////
static void RunT6()
{
    printf("T6 COLORREF packing...\n");
    std::string out;
    out += "# GetColorRef(r%%,g%%,b%%,is16_235) -> 0xXXXXXXXX\n";
    static const double pcts[] = { 0.0, 1.0, 10.5, 25.3, 50.0, 62.1, 75.5, 99.0, 100.0 };
    const int n = sizeof(pcts)/sizeof(pcts[0]);
    for (int r = 0; r < 2; ++r)
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
            {
                ColorRGBDisplay c(pcts[i], pcts[j], pcts[(i + j) % n]);
                AppendF(out, "%d,%.17g,%.17g,%.17g,0x%08lX\n", r,
                        c[0], c[1], c[2], (unsigned long)c.GetColorRef(r != 0));
            }
    HandleTable("T6_colorref.txt", out);
}

//////////////////////////////////////////////////////////////////////////
// T7 — GenerateProfileColors (display-profiling cube patch set). The whole
// feature relies on this being DETERMINISTIC: a saved .chc stores only the
// cube size + gray-extras flag and regenerates the exact patch list on load,
// and the reference / export paths index it by position. If the ordering or
// count ever changes, saved profiles silently mis-map. Locks the exact counts
// (which also drive the pane's preset labels) and the ordering contract. No
// golden file: all values are asserted inline.
//////////////////////////////////////////////////////////////////////////
static void RunT7()
{
    printf("T7 GenerateProfileColors determinism...\n");
    // { cube N, count WITH gray/near-black extras } — must match the pane presets
    static const int cases[5][2] = { {5,150}, {9,754}, {11,1350}, {17,4938}, {21,9270} };
    for (int c = 0; c < 5; ++c)
    {
        int N = cases[c][0], expExt = cases[c][1];
        int plain = GenerateProfileColors(NULL, 0, N, false);
        if (plain != N*N*N) Fail("T7 cube %d plain count %d != %d", N, plain, N*N*N);
        int ext = GenerateProfileColors(NULL, 0, N, true);
        if (ext != expExt) Fail("T7 cube %d extras count %d != %d", N, ext, expExt);

        std::vector<ColorRGBDisplay> a(ext), b(ext);
        int fa = GenerateProfileColors(&a[0], ext, N, true);
        int fb = GenerateProfileColors(&b[0], ext, N, true);
        if (fa != ext || fb != ext) Fail("T7 cube %d fill count mismatch (%d/%d)", N, fa, fb);
        for (int i = 0; i < ext; ++i)
            if (a[i][0] != b[i][0] || a[i][1] != b[i][1] || a[i][2] != b[i][2])
            { Fail("T7 cube %d non-deterministic at index %d", N, i); break; }
    }

    // ordering: r slowest, b fastest; first = black, last cube node = white
    std::vector<ColorRGBDisplay> g(125);
    GenerateProfileColors(&g[0], 125, 5, false);
    if (g[0][0] != 0.0 || g[0][1] != 0.0 || g[0][2] != 0.0) Fail("T7 patch 0 not black");
    if (g[1][0] != 0.0 || g[1][1] != 0.0 || fabs(g[1][2] - 25.0) > 1e-9) Fail("T7 patch 1 not (0,0,25)");
    if (fabs(g[124][0] - 100.0) > 1e-9 || fabs(g[124][2] - 100.0) > 1e-9) Fail("T7 last cube node not white");

    // bad cube sizes rejected; buffer overflow caught
    if (GenerateProfileColors(NULL, 0, 1, false)  != -1) Fail("T7 cube size 1 not rejected");
    if (GenerateProfileColors(NULL, 0, 22, false) != -1) Fail("T7 cube size 22 not rejected");
    std::vector<ColorRGBDisplay> tiny(10);
    if (GenerateProfileColors(&tiny[0], 10, 5, false) != -1) Fail("T7 buffer overflow not caught");
}

// T8 — SnapToVideoGrid oracle: the shared patch/reference/sensor grid
// quantizer must hit the exact native grid for every (bit depth, range)
// combo: 219 / 255 (8-bit limited/full), 876 / 1023 (10-bit limited/full).
// 8-bit limited must equal the historical floor(v*219+0.5)/219 form exactly.
//////////////////////////////////////////////////////////////////////////
static void RunT8()
{
    printf("T8 SnapToVideoGrid oracle...\n");
    static const struct { bool b10, lim; double grid; } combos[] = {
        { false, true,  219.  }, { false, false, 255.  },
        { true,  true,  876.  }, { true,  false, 1023. },
    };
    for (int c = 0; c < 4; ++c)
    {
        const double grid = combos[c].grid;
        for (int i = 0; i <= 1000000; ++i)      // v = 0..1 step 1e-6
        {
            double v = i * 1e-6;
            double got = SnapToVideoGrid(v, combos[c].b10, combos[c].lim);
            // contract includes the 1e-9 tie-breaker: exact half-code ties
            // round up deterministically (see SnapToVideoGrid in Color.cpp)
            double want = floor(v * grid + 0.5 + 1e-9) / grid;
            if (got != want)
                Fail("T8 b10=%d lim=%d v=%.17g got=%.17g want=%.17g",
                     combos[c].b10, combos[c].lim, v, got, want);
            // result must sit on an integer code of its grid (allow last-ULP
            // noise from the /grid*grid round trip; a wrong grid is off by ~0.2+)
            double code = got * grid;
            if (fabs(code - floor(code + 0.5)) > 1e-6)
                Fail("T8 off-grid b10=%d lim=%d v=%.17g code=%.17g",
                     combos[c].b10, combos[c].lim, v, code);
        }
        // legacy byte-identity spot check for 8-bit limited
        if (!combos[c].b10 && combos[c].lim)
            for (int i = 0; i <= 1000; ++i)
            {
                double v = i * 1e-3;
                if (SnapToVideoGrid(v, false, true) != floor((v * 219.) + 0.5) / 219.)
                    Fail("T8 legacy219 v=%.17g", v);
            }
    }
}

//////////////////////////////////////////////////////////////////////////
// T9 — quantizer equivalence oracle (pure assertion, no golden file).
// Freezes two invariants that let the generators, the shared SnapToVideoGrid
// signal/reference quantizer, and the rPI wire emitter all agree on a single
// code for every (bit depth, range) combo: 219/255 (8-bit limited/full),
// 876/1023 (10-bit limited/full).
//   1. Generator grid-form floor(v*grid+0.5+1e-9) selects the SAME integer
//      code as SnapToVideoGrid(v)*grid for a dense deterministic sweep of v:
//      every code center k/grid, every half-code tie (k+0.5)/grid, that tie
//      +/- 1e-12 and +/- 2e-10 of dust, and a golden-ratio low-discrepancy
//      sweep. For 8-bit limited the historical /2.19 percent form is also
//      shown to be the same code (k/2.19/100 == k/219 to 1e-12).
//   2. Emitter stability: an on-grid signal snapped by SnapToVideoGrid passes
//      through PiPercentToCode without moving a code — code == k + black
//      offset (16/0/64/0 for lim8/full8/lim10/full10).
//////////////////////////////////////////////////////////////////////////
static void RunT9()
{
    printf("T9 quantizer equivalence oracle...\n");
    static const struct { bool b10, lim; double grid; int bits; int offset; } combos[] = {
        { false, true,  219.,  8,  16 }, { false, false, 255.,  8,  0 },
        { true,  true,  876.,  10, 64 }, { true,  false, 1023., 10, 0 },
    };
    for (int c = 0; c < 4; ++c)
    {
        const double grid   = combos[c].grid;
        const bool   b10    = combos[c].b10;
        const bool   lim    = combos[c].lim;
        const int    bits   = combos[c].bits;
        const int    offset = combos[c].offset;
        const int    gi     = (int)grid;

        // ---- Part 1: generator grid-form code == SnapToVideoGrid code.
        // Each closure input asserts the two integer codes are identical, and
        // (8-bit limited only) the /2.19 percent form is the same code.
        struct Local {
            static void checkCode(double v, double grid, bool b10, bool lim)
            {
                int genCode  = (int)floor(v * grid + 0.5 + 1e-9);
                int snapCode = (int)floor(SnapToVideoGrid(v, b10, lim) * grid + 0.5);
                if (genCode != snapCode)
                    Fail("T9 code mismatch grid=%.0f v=%.17g gen=%d snap=%d",
                         grid, v, genCode, snapCode);
                if (!b10 && lim)   // /2.19 percent form: same code as k/219
                {
                    double a = floor(v * 219. + 0.5 + 1e-9) / 2.19 / 100.;
                    double b = SnapToVideoGrid(v, false, true);
                    if (fabs(a - b) > 1e-12)
                        Fail("T9 pct-form grid=219 v=%.17g /2.19/100=%.17g snap=%.17g",
                             v, a, b);
                }
            }
        };
        for (int k = 0; k <= gi; ++k)
        {
            Local::checkCode((double)k / grid, grid, b10, lim);        // code center
            double tie = ((double)k + 0.5) / grid;                     // half-code tie
            Local::checkCode(tie,          grid, b10, lim);
            Local::checkCode(tie + 1e-12,  grid, b10, lim);
            Local::checkCode(tie - 1e-12,  grid, b10, lim);
            Local::checkCode(tie + 2e-10,  grid, b10, lim);
            Local::checkCode(tie - 2e-10,  grid, b10, lim);
        }
        for (int n = 1; n <= 2000; ++n)                               // golden-ratio sweep
            Local::checkCode(fmod(0.6180339887498949 * n, 1.0), grid, b10, lim);

        // ---- Part 2: emitter stability for on-grid inputs.
        for (int k = 0; k <= gi; ++k)
        {
            double p = SnapToVideoGrid((double)k / grid, b10, lim) * 100.;
            int code = PiPercentToCode(p, lim, bits);
            if (code != k + offset)
                Fail("T9 emitter grid=%.0f k=%d p=%.17g: PiPercentToCode=%d expected=%d",
                     grid, k, p, code, k + offset);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
// T10 — gamut-basis consistency oracle (pure assertion, no golden file).
//
// Closes a real gap: dE-based tests are blind to which BASIS a consumer plots
// against, because a point can be drawn in completely the wrong place while its
// dE reads exactly 0.000. The 3D viewer built its gamut solid straight from
// CColorReference::GetRed/Green/Blue, but HDTVa/HDTVb do not carry a gamut
// there - HDTVa holds the 75% saturation/amplitude PATCH chromaticities and
// HDTVb the plasma-optimized color-checker ones, both far inside Rec.709. So
// the solid was a small desaturated triangle while the references themselves
// are manufactured in real Rec.709 by SetRGBValue's substitution, and a perfect
// simulated display plotted its own targets outside the solid at dE 0.00.
//
// The invariant: the basis a consumer uses for GEOMETRY must be the same basis
// the references were BUILT in. Two legs, over every standard in the enum
// except CUSTOM (see the carve-outs below):
//   1. Round trip. SetRGBValue(rgb, S) is how the app manufactures a reference;
//      decoding that XYZ in SpecialModeGamutReference(S) must return rgb. Any
//      standard whose geometry basis disagrees with its construction basis
//      lands outside the unit cube here. Verified by mutation: reverting the
//      helper to a plain `return active` fails 600 assertions - 240 on HDTVa
//      (red decodes to 1.267,-0.072,-0.071 instead of 1,0,0), 240 on HDTVb,
//      120 on CC6, and none at all on the other eight standards.
//   2. Helper agreement. SpecialModeGamutReference must reproduce GetRGBValue's
//      own inline substitution exactly, so the two cannot drift apart.
//
// Each standard runs under three white targets, and that is load-bearing rather
// than thoroughness for its own sake: the special modes' construction basis is a
// FIXED Rec.709/D65 (SetRGBValue builds CColorReference(HDTV) unconditionally),
// so a helper that carried the active white over would pass every default-white
// case and fail 270 assertions the moment a non-D65 white is selected. The first
// version of this helper did exactly that.
//
// SCOPE: this pins the helper's CONTRACT, not its application. Nothing here can
// see C3DColorView - if the viewer stopped calling SpecialModeGamutReference, T10
// would stay green. That call site is funnelled through a single
// SceneGamutReference() in Color3DView.cpp to keep the blast radius of such a
// regression to one line; the display itself is still only checked on screen.
//
// CUSTOM is deliberately absent: CColorReference's CUSTOM branch writes its
// primaries THROUGH the default `primaries` pointer, which still aims at the
// global primariesRec601 array, so merely constructing one corrupts SDTV for
// the rest of the process. Constructing it here would poison later tests.
// CC6 (the unused enum slot) runs leg 1 only: GetRGBValue's substitution
// predicate omits CC6 while SetRGBValue's and GetDeltaE's include it, a latent
// inconsistency in dead-but-present code that leg 2 would trip over.
//////////////////////////////////////////////////////////////////////////
static void RunT10()
{
    printf("T10 gamut-basis consistency oracle...\n");
    static const struct { ColorStandard cs; const char* name; } stds[] = {
        { PALSECAM, "PALSECAM" }, { SDTV,   "SDTV"   }, { HDTV,   "HDTV"   },
        { HDTVa,    "HDTVa"    }, { HDTVb,  "HDTVb"  }, { sRGB,   "sRGB"   },
        { UHDTV,    "UHDTV"    }, { UHDTV2, "UHDTV2" }, { UHDTV3, "UHDTV3" },
        { UHDTV4,   "UHDTV4"   }, { CC6,    "CC6"    },
    };
    // cube corners, face centres, grey, and a few off-axis points
    static const double rgbs[][3] = {
        {0,0,0}, {1,0,0}, {0,1,0}, {0,0,1}, {1,1,0}, {0,1,1}, {1,0,1}, {1,1,1},
        {0.5,0,0}, {0,0.5,0}, {0,0,0.5}, {0.5,0.5,0.5},
        {0.75,0.25,0.10}, {0.10,0.75,0.25}, {0.25,0.10,0.75},
    };
    // White targets matter: the special standards' construction basis is a FIXED
    // Rec.709/D65 (SetRGBValue builds CColorReference(HDTV) unconditionally), so a
    // geometry basis that tracked the active white instead would break leg 1 for
    // every non-D65 white while passing under the default.
    enum { W_DEFAULT, W_NAMED, W_CUSTOM, W_COUNT };
    static const char * wName[W_COUNT] = { "D65", "D75", "custom" };

    const int nStd = (int)(sizeof(stds) / sizeof(stds[0]));
    const int nRGB = (int)(sizeof(rgbs) / sizeof(rgbs[0]));

    for (int s = 0; s < nStd; ++s)
    for (int w = 0; w < W_COUNT; ++w)
    {
        CColorReference ref =
            (w == W_NAMED)  ? CColorReference(stds[s].cs, D75) :
            (w == W_CUSTOM) ? CColorReference(stds[s].cs, DCUST, -1.0, " modified",
                                              ColorXYZ(ColorxyY(0.2900, 0.3000)))
                            : CColorReference(stds[s].cs);
        CColorReference gref = SpecialModeGamutReference(ref);
        char tag[48];
        _snprintf(tag, sizeof(tag) - 1, "%s/%s", stds[s].name, wName[w]);
        tag[sizeof(tag) - 1] = 0;

        // Ordinary standards must come back untouched - the helper is only ever
        // allowed to act on the special ones.
        bool special = (stds[s].cs == HDTVa || stds[s].cs == HDTVb || stds[s].cs == CC6);
        if (!special)
            for (int k = 0; k < 3; ++k)
                if (fabs(ref.GetWhite()[k] - gref.GetWhite()[k]) > 1e-12)
                    Fail("T10 %s: helper altered a non-special reference, white component "
                         "%d: %.17g -> %.17g", tag, k, ref.GetWhite()[k], gref.GetWhite()[k]);

        for (int i = 0; i < nRGB; ++i)
        {
            ColorRGB rgb(rgbs[i][0], rgbs[i][1], rgbs[i][2]);
            CColor c;
            c.SetRGBValue(rgb, ref);

            // ---- Leg 1: construction basis == geometry basis.
            ColorRGB back(c.GetXYZValue(), gref);
            for (int k = 0; k < 3; ++k)
                if (fabs(back[k] - rgb[k]) > 1e-9)
                    Fail("T10 %s rgb=(%.2f,%.2f,%.2f): reference is outside the plotted "
                         "gamut basis, channel %d came back %.6f (expected %.6f)",
                         tag, rgb[0], rgb[1], rgb[2], k, back[k], rgb[k]);

            // ---- Leg 2: helper == GetRGBValue's own inline substitution.
            if (stds[s].cs == CC6)
                continue;
            ColorRGB viaGetter = c.GetRGBValue(ref);
            for (int k = 0; k < 3; ++k)
                if (fabs(viaGetter[k] - back[k]) > 1e-9)
                    Fail("T10 %s rgb=(%.2f,%.2f,%.2f): SpecialModeGamutReference disagrees "
                         "with GetRGBValue, channel %d: %.17g vs %.17g",
                         tag, rgb[0], rgb[1], rgb[2], k, back[k], viaGetter[k]);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
// T11 - BT.2390 tone-map monotonicity oracle (pure assertion, no golden file).
//
// Closes the near-black half of the tone-mapping curve, which no dE-based
// harness can see: /accuracytest runs every tone-map combo with MasterMinL and
// TargetMinL pinned to 0, and a reference that is wrong in exactly the same way
// as the modelled sensor still reads dE 0.000.
//
// The bug this pins: BT2390ToneMap normalises the signal against the mastering
// display's black (E1 = (E - PQ(MinML)) / d), runs the knee, and then only
// applies BT.2390's black lift when E2 landed inside [0,1]. Anything that fell
// out of that window dropped to the raw, UN-tone-mapped PQ curve, so near-black
// got no lift at all while the highlights were still rolled off - Y target at 0%
// (which returns the display black target directly) could come out ABOVE Y
// target at 1-5%. Two inputs land in that dead zone: a signal below the
// mastering black (negative E1, so negative E2), and a target peak below a third
// of the mastering range (negative KS, so the knee itself returns a negative
// E2). The E2 floor in Color.cpp covers both.
//
// The invariant is the weakest thing that catches it and stays true of any sane
// EOTF: an electro-optical transfer function must be non-decreasing in the
// signal. Verified by mutation - dropping the E2 floor in Color.cpp fails 2880
// of the 4320 configurations swept here (and 54 of T12's 72). Four more
// mutations of the same block are killed from this test: writing the floor as
// max(E2, 0.0) fails 240 (the NaN at TargetMaxL == MasterMaxL == 10000 becomes
// 0, so full white reads as black), clamping E1's top end fails 600 on the peak
// check below, dropping the d > 0 guard fails 10 of the 18 degenerate configs,
// and flooring E1 rather than E2 - the narrower fix, which covers only the
// sub-black half - still fails 92.
//
// The grid deliberately mixes MasterMinL above and below TargetMinL: the
// sub-black dead zone only opens when the mastering black is nonzero, and
// BT2390ToneMap's own `if (m_MinML > m_MinTL) m_MinML = m_MinTL` clamp shrinks
// it again from the other side.
//
// Where the grid deliberately does NOT go. The invariant below is stated
// absolutely, but it is only swept over a band where it currently holds: the
// black lift's exponent p = min(1/b, 4*BS) falls under 1 when Black Slope drops
// below 0.25, or when b exceeds 1 from a high target black against a low
// diffuse white. p < 1 makes the lift's slope diverge as E2 -> 1 and turns the
// curve over MID-SCALE - a third non-monotonic band, distinct from both
// near-black mechanisms above, pre-existing and untouched by the E2 floor (225
// of 5760 UI-reachable configurations, every one at BS = 0.1, and identical on
// main). BS is pinned to {1.0, 2.0} and diffuse to {94.37844, 203} to stay
// clear of both entrances; widen either and T11 becomes a test of that band
// instead of this fix. (203 is BT.2408's reference white and sits just above
// the References page's own DiffuseL cap of 200, so it reaches libHCFR only
// through an INI - a separate question from what this test covers.)
//////////////////////////////////////////////////////////////////////////
static void RunT11()
{
    printf("T11 BT.2390 tone-map monotonicity oracle...\n");

    static const double minMLs[] = { 0.0, 0.0001, 0.001, 0.005, 0.05, 0.5 };
    static const double maxMLs[] = { 1000.0, 4000.0, 10000.0 };
    static const double minTLs[] = { 0.0, 0.005, 0.05, 0.1, 0.3 };
    // Three of these columns exist for a specific regime, not for coverage:
    //  - 2000 is above the 1000-nit mastering peak, the EXPANSION region (a
    //    display brighter than the disc), where a clamp on E1's top end
    //    flat-lines the whole top of the curve.
    //  - 30 is low enough against these mastering peaks to drive
    //    KS = 1.5*maxL - 0.5 NEGATIVE, which lets the Hermite knee return a
    //    negative E2 at signal 0 and, before the E2 floor, dropped the bottom
    //    of the curve to raw PQ - the same 0%-above-1% inversion from a second
    //    mechanism. Without this column every config here has KS > 0.
    //  - 10000 equals a mastering peak AND is the top of the PQ range, the one
    //    place E1, KS and maxL all land on exactly 1 at full signal, so the knee
    //    runs with T = 0/0 and E2 comes out NaN. That NaN has to keep falling
    //    through to raw PQ; a floor written as max(E2, 0.0) answers 0 for it and
    //    full white reads as black. 1000 is the same equality lower down, where
    //    PQ(MasterMaxL) < 1 keeps E1 away from the singular point.
    static const double maxTLs[] = { 30.0, 120.0, 400.0, 1000.0, 2000.0, 10000.0 };
    // Both pinned clear of the p < 1 band described above, not for coverage.
    static const double diffuses[] = { 94.37844, 203.0 };
    static const double bFacts[] = { 1.0, 2.0 };          // m_BT2390_BS
    static const double e2Facts[] = { 0.0, 1.0 };         // m_BT2390_WS

    // Sample on the inverse LUT's own 1/1024 grid rather than a coarser one, so
    // the oracle's resolution is not the thing that decides what it can see.
    const int steps = 1024;
    int configs = 0;

    for (int a = 0; a < _countof(minMLs); a++)
    for (int b = 0; b < _countof(maxMLs); b++)
    for (int c = 0; c < _countof(minTLs); c++)
    for (int d = 0; d < _countof(maxTLs); d++)
    for (int e = 0; e < _countof(diffuses); e++)
    for (int f = 0; f < _countof(bFacts); f++)
    for (int g = 0; g < _countof(e2Facts); g++)
    {
        configs++;
        double prev = -1.0;
        for (int i = 0; i <= steps; i++)
        {
            const double valx = (double)i / steps;
            const double y = getL_EOTF(valx, noDataColor, noDataColor, 0.0, 0.0, 5,
                                       diffuses[e], minMLs[a], maxMLs[b],
                                       minTLs[c], maxTLs[d], true, false, 1.2,
                                       bFacts[f], e2Facts[g], 25.0) * 100.0;
            if (y < prev - 1e-9)
            {
                Fail("T11 MasterMin=%g MasterMax=%g TargetMin=%g TargetMax=%g "
                     "diffuse=%g BS=%g WS=%g: tone-mapped PQ target falls from "
                     "%.6f to %.6f cd/m2 at signal %.4f",
                     minMLs[a], maxMLs[b], minTLs[c], maxTLs[d], diffuses[e],
                     bFacts[f], e2Facts[g], prev, y, valx);
                break;      // one report per configuration
            }
            prev = y;
        }

        // Peak reach. Monotonicity alone cannot see a curve that goes FLAT
        // early - that is still non-decreasing - so pin the endpoint too: full
        // signal must land on the display's peak, whatever the mastering peak
        // was. Clamping E1's top end broke exactly this, pinning everything
        // above the mastering peak to PQ(MasterMaxL): a 1000-nit-mastered disc
        // on a 2000-nit target returned 1000 here instead of 2000, flat across
        // the whole top of the grid, with T11 none the wiser.
        const double peak = getL_EOTF(1.0, noDataColor, noDataColor, 0.0, 0.0, 5,
                                      diffuses[e], minMLs[a], maxMLs[b],
                                      minTLs[c], maxTLs[d], true, false, 1.2,
                                      bFacts[f], e2Facts[g], 25.0) * 100.0;
        if (fabs(peak - maxTLs[d]) > 1e-6 * maxTLs[d])
            Fail("T11 MasterMin=%g MasterMax=%g TargetMin=%g TargetMax=%g "
                 "diffuse=%g BS=%g WS=%g: full signal reaches %.4f cd/m2, "
                 "expected the display peak %.4f",
                 minMLs[a], maxMLs[b], minTLs[c], maxTLs[d], diffuses[e],
                 bFacts[f], e2Facts[g], peak, maxTLs[d]);
    }
    printf("  %d tone-map configurations swept\n", configs);

    // Degenerate mastering metadata. MasterMaxL = 0 makes the PQ span d zero or
    // negative, so E1, minL and maxL all come out +/-inf or NaN. The References
    // page cannot produce it - it validates MasterMaxL to [100, 10000] and
    // MasterMinL to [0, 0.5], and the smallest d over the corners of all four
    // luminance fields is 0.0733 - so it arrives from a hand-edited INI, which
    // is read back without re-validation, or from a direct caller of the public
    // getL_EOTF. The E2 floor has to stay OFF for those: an infinite
    // b turns the black lift into a NaN, and min(NaN, cap) resolves to the full
    // target peak, so a signal of 1e-7 returned the whole 1000-nit target peak
    // while 1e-5 next door returned 4e-9. Sampled on a sub-grid ladder because
    // that is where it lives - the coarsest failing signal is well under 1/1024 -
    // and starting above 0, since x = 0 takes the early return at the top of
    // getL_EOTF and is the separate, pre-existing 0%-above-1% case this does not
    // touch.
    {
        static const double degenMinMLs[] = { 0.0, 0.05, 0.5 };
        static const double degenMinTLs[] = { 0.0, 0.005, 0.05 };
        static const double degenMaxTLs[] = { 120.0, 1000.0 };
        static const double ladder[] = { 1e-9, 1e-8, 1e-7, 1e-6, 1e-5, 1e-4, 1e-3 };
        int degen = 0;
        for (int a = 0; a < _countof(degenMinMLs); a++)
        for (int c = 0; c < _countof(degenMinTLs); c++)
        for (int d = 0; d < _countof(degenMaxTLs); d++)
        {
            degen++;
            double prev = -1.0;
            for (int i = 0; i < _countof(ladder); i++)
            {
                const double y = getL_EOTF(ladder[i], noDataColor, noDataColor, 0.0, 0.0, 5,
                                           94.37844, degenMinMLs[a], 0.0,
                                           degenMinTLs[c], degenMaxTLs[d], true, false, 1.2,
                                           1.0, 0.0, 25.0) * 100.0;
                if (y < prev - 1e-12)
                {
                    Fail("T11 degenerate MasterMin=%g MasterMax=0 TargetMin=%g TargetMax=%g: "
                         "target falls from %.9f to %.9f cd/m2 at signal %g",
                         degenMinMLs[a], degenMinTLs[c], degenMaxTLs[d], prev, y, ladder[i]);
                    break;
                }
                prev = y;
            }
        }
        printf("  %d degenerate-metadata configurations swept\n", degen);
    }
}

//////////////////////////////////////////////////////////////////////////
// T12 - BT.2390 inverse-LUT oracle (pure assertion, no golden file).
//
// T11 only reaches the FORWARD direction: its getL_EOTF call passes a positive
// mode, so case -10 - the LUT build and the nearest-neighbour search over it -
// had no coverage at all. This closes that. The per-signal math is now one
// shared BT2390ToneMap, so both oracles pin the same curve rather than two
// copies of it, but everything around it on the inverse side is only reachable
// from here, and it gets two things wrong on its own:
//
// 1. MATH-008, the empty-LUT fault. case -10 used to build the table by
//    re-entering getL_EOTF with mode 5 and cBT2390 = TRUE, which meant the
//    build had to survive the guards at the top of that function and the
//    `ToneMap && mode == 5` promotion to case 10. Two ways through without a
//    table: a valx of 0 tripped the `valx == 0 && mode > 4` early return and
//    came back BEFORE the cBT2390 branch ran, and a caller arriving at mode
//    -10 with ToneMap false skipped the promotion entirely. Either way the
//    vectors held whatever they held - empty on the first such call - and the
//    nearest-neighbour search read BT2390y[0] out of bounds. It went unnoticed
//    because the dead tmWhite call in the forward branch happened to leave
//    exactly one entry behind on every forward tone-mapped call; deleting that
//    dead code (same commit) exposed the fault, so this test is what keeps it
//    dead. case -10 now calls BuildBT2390Table directly, past both guards.
//    Verified by mutation: deleting that build call aborts this test with
//    "vector subscript out of range" on the first configuration.
//
// 2. Round trip, stated in LUMINANCE: forward(inverse(y)) == y. Signal ->
//    signal is the wrong invariant here, because the tone curve is not
//    injective - the clamp deliberately maps every signal below the mastering
//    black onto the black target, and the peak cap flattens the highlights the
//    same way, so several signals share one luminance and the inverse is
//    legitimately free to return any of them. What must hold is that whichever
//    it returns lands back on the luminance asked for. The LUT stores
//    nits/10000 while mode 5 returns nits/100, hence the /100 bridge below.
//////////////////////////////////////////////////////////////////////////
static void RunT12()
{
    printf("T12 BT.2390 inverse-LUT oracle...\n");

    static const double minMLs[] = { 0.0, 0.005, 0.05 };
    static const double maxMLs[] = { 1000.0, 4000.0 };
    static const double minTLs[] = { 0.0, 0.005, 0.05, 0.1 };
    // 30 against a 1000/4000-nit master is the KS < 0 regime, where the forward
    // curve is deliberately flat across the bottom - the inverse is then free to
    // return any signal in that run, which is exactly why the round trip below is
    // stated in luminance.
    static const double maxTLs[] = { 30.0, 120.0, 1000.0 };

    int configs = 0;

    for (int a = 0; a < _countof(minMLs); a++)
    for (int b = 0; b < _countof(maxMLs); b++)
    for (int c = 0; c < _countof(minTLs); c++)
    for (int d = 0; d < _countof(maxTLs); d++)
    {
        configs++;

        // (1) MATH-008 guard. Deliberately the FIRST getL_EOTF call for this
        // configuration, so the LUT is stale or empty exactly as it would be
        // when something inverts before anything has run the forward curve.
        const double zero = getL_EOTF(0.0, noDataColor, noDataColor, 0.0, 0.0, -5,
                                      94.37844, minMLs[a], maxMLs[b], minTLs[c], maxTLs[d],
                                      true, false, 1.2, 1.0, 0.0, 25.0);
        if (zero != 0.0)
            Fail("T12 MasterMin=%g MasterMax=%g TargetMin=%g TargetMax=%g: inverse of 0 "
                 "returned %.6f, expected 0 (the signal that produces black)",
                 minMLs[a], maxMLs[b], minTLs[c], maxTLs[d], zero);

        // (2) forward -> inverse round trip over near black
        for (int i = 0; i <= 40; i++)
        {
            const double sig = (double)i / 1024.0;
            const double raw = getL_EOTF(sig, noDataColor, noDataColor, 0.0, 0.0, 5,
                                         94.37844, minMLs[a], maxMLs[b], minTLs[c], maxTLs[d],
                                         true, false, 1.2, 1.0, 0.0, 25.0);
            const double y    = raw / 100.0;                  // LUT units, nits/10000
            const double back = getL_EOTF(y, noDataColor, noDataColor, 0.0, 0.0, -5,
                                          94.37844, minMLs[a], maxMLs[b], minTLs[c], maxTLs[d],
                                          true, false, 1.2, 1.0, 0.0, 25.0);
            const double y2   = getL_EOTF(back, noDataColor, noDataColor, 0.0, 0.0, 5,
                                          94.37844, minMLs[a], maxMLs[b], minTLs[c], maxTLs[d],
                                          true, false, 1.2, 1.0, 0.0, 25.0) / 100.0;
            // sig sits on the LUT's own grid, so the search should land on an
            // exact table entry; only floating-point dust is allowed.
            const double tol = 1e-12 + 1e-6 * fabs(y);
            if (fabs(y2 - y) > tol)
            {
                Fail("T12 MasterMin=%g MasterMax=%g TargetMin=%g TargetMax=%g: signal %.6f "
                     "-> %.8f -> inverse %.6f -> %.8f (off by %.3g, tolerance %.3g)",
                     minMLs[a], maxMLs[b], minTLs[c], maxTLs[d], sig, y, back, y2,
                     fabs(y2 - y), tol);
                break;      // one report per configuration
            }
        }
    }
    // (3) The old diffuse-white fast path. Building the table used to short to a
    // SINGLE entry whenever the signal handed to the builder landed within 5e-4
    // of 0.5022283, leaving the nearest-neighbour search one candidate and making
    // it hand back its own input as the answer. The window is a real luminance -
    // the LUT is in nits/10000, so it is ~5022 cd/m2, inside a 10000-nit target -
    // and it was reachable from the near-white autoscale in Measure.cpp, where the
    // inverted value is a free-ranging measured luminance. Walk straight across it.
    {
        const double lo = 0.5022283 - 8e-4, hi = 0.5022283 + 8e-4;
        for (int i = 0; i <= 16; i++)
        {
            const double y = lo + (hi - lo) * i / 16.0;
            const double sig = getL_EOTF(y, noDataColor, noDataColor, 0.0, 0.0, -5,
                                         94.37844, 0.0, 10000.0, 0.0, 10000.0,
                                         true, false, 1.2, 1.0, 0.0, 25.0);
            const double y2 = getL_EOTF(sig, noDataColor, noDataColor, 0.0, 0.0, 5,
                                        94.37844, 0.0, 10000.0, 0.0, 10000.0,
                                        true, false, 1.2, 1.0, 0.0, 25.0) / 100.0;
            // one LUT step is 1/1024 in signal; allow the luminance that spans
            if (fabs(y2 - y) > 0.02 * y)
                Fail("T12 fast-path window: inverse of %.7f returned signal %.6f, "
                     "which forwards to %.7f (off by %.3g) - one-entry table?",
                     y, sig, y2, fabs(y2 - y));
        }
    }

    printf("  %d inverse configurations swept\n", configs);
}

//////////////////////////////////////////////////////////////////////////
// T17 — PatchSequence generator + CSV writer oracle (pure assertion, no
// golden file). Every primitive is a pure function of its arguments, so
// values are pinned exactly (the uniform grid uses the endpoint-exact
// lo*(1-u)+hi*u form and the pinned levels below are FP-exact); the CSV
// writer is pinned by an inline expected-bytes oracle plus a genuine
// integration round trip through the REAL importer (ReadCsvPatchRows +
// CsvCodeToPercent from Color.cpp) at every bits/range combination, with
// the reconstruction error bounded by half a code step.
//////////////////////////////////////////////////////////////////////////

namespace T17Seq
{
    static bool SameSequences(const PatchSequence& a, const PatchSequence& b)
    {
        if (a.Count() != b.Count())
            return false;
        for (int i = 0; i < a.Count(); ++i)
        {
            double ra[3], rb[3];
            std::string na, nb;
            if (!a.GetPatch(i, ra, &na) || !b.GetPatch(i, rb, &nb))
                return false;
            for (int k = 0; k < 3; ++k)
                if (ra[k] != rb[k])
                    return false;
            if (na != nb)
                return false;
        }
        return true;
    }

    // Half a code step in percent for a bits/range encoding - the writer's
    // documented round-trip bound.
    static double HalfStepPct(int bits, int range)
    {
        double span;
        if (range == CSV_RANGE_FULL)
            span = (double)((1 << bits) - 1);
        else
            span = (235.0 - 16.0) * (1 << (bits - 8));
        return 50.0 / span;
    }

    static const char* kTmpCsv = "t17_roundtrip.csv";
}

static void RunT17()
{
    printf("T17 PatchSequence generator + CSV writer oracle...\n");
    using namespace T17Seq;

    // AddPatch validation: bad values and names refused, state preserved.
    {
        PatchSequence s;
        if (s.Count() != 0)
            Fail("T17 fresh sequence not empty");
        if (s.AddPatch(sqrt(-1.0), 0, 0, "x"))
            Fail("T17 AddPatch accepted NaN");
        if (s.AddPatch(-0.001, 0, 0, "x") || s.AddPatch(0, 100.001, 0, "x"))
            Fail("T17 AddPatch accepted out-of-range percent");
        if (s.AddPatch(50, 50, 50, "bad,name"))
            Fail("T17 AddPatch accepted a comma in the name");
        if (s.AddPatch(50, 50, 50, "bad\nname"))
            Fail("T17 AddPatch accepted a newline in the name");
        // The importer trims edge whitespace from names, so names that
        // would not round-trip byte-for-byte are refused up front.
        if (s.AddPatch(50, 50, 50, " padded") || s.AddPatch(50, 50, 50, "padded ")
            || s.AddPatch(50, 50, 50, "  "))
            Fail("T17 AddPatch accepted edge whitespace in the name");
        if (s.AddPatch(50, 50, 50, "tab\tname") || s.AddPatch(50, 50, 50, "nul\x01"))
            Fail("T17 AddPatch accepted a non-printable name byte");
        if (s.AddPatch(50, 50, 50, "caf\xC3\xA9"))
            Fail("T17 AddPatch accepted a non-ASCII name byte");
        if (s.Count() != 0)
            Fail("T17 refused AddPatch changed the sequence");
        // Negative zero is stored as +0.0 (one bit pattern per value, and
        // no "-0" in generated names).
        if (!s.AddPatch(-0.0, 0.0, 100.0, "negzero"))
            Fail("T17 AddPatch refused -0.0");
        else
        {
            double rgb[3];
            if (!s.GetPatch(0, rgb, 0))
                Fail("T17 negzero GetPatch failed");
            else if (memcmp(&rgb[0], &rgb[1], sizeof(double)) != 0)
                Fail("T17 -0.0 not normalized to +0.0");
            s.Clear();
        }
        if (!s.AddPatch(0, 0, 0, "Black") || !s.AddPatch(100, 100, 100, "White"))
            Fail("T17 AddPatch refused valid patches");
        double rgb[3];
        std::string nm;
        if (!s.GetPatch(1, rgb, &nm) || rgb[0] != 100.0 || nm != "White")
            Fail("T17 GetPatch(1) wrong");
        if (!s.GetPatch(0, rgb, 0))
            Fail("T17 GetPatch with null name refused");
        if (s.GetPatch(2, rgb, &nm) || s.GetPatch(-1, rgb, &nm))
            Fail("T17 out-of-range GetPatch succeeded");
    }

    // Gray ramp: exact levels on the endpoint-exact grid, neutral values,
    // "%g" names; refusals.
    {
        PatchSequence s;
        if (!s.AddGrayRamp(5, 0.0, 100.0))
            Fail("T17 AddGrayRamp(5,0,100) refused");
        const double want[5] = { 0.0, 25.0, 50.0, 75.0, 100.0 };
        const char* wantName[5] =
            { "Gray 0", "Gray 25", "Gray 50", "Gray 75", "Gray 100" };
        for (int i = 0; i < 5; ++i)
        {
            double rgb[3];
            std::string nm;
            if (!s.GetPatch(i, rgb, &nm))
                { Fail("T17 gray GetPatch(%d) failed", i); continue; }
            if (rgb[0] != want[i] || rgb[1] != want[i] || rgb[2] != want[i])
                Fail("T17 gray level %d = (%.17g,%.17g,%.17g) want %.17g",
                     i, rgb[0], rgb[1], rgb[2], want[i]);
            if (nm != wantName[i])
                Fail("T17 gray name %d = \"%s\" want \"%s\"",
                     i, nm.c_str(), wantName[i]);
        }
        // Non-0/100 endpoints are still exact (10..90 by 5 -> 10,30,50,70,90).
        PatchSequence t;
        if (!t.AddGrayRamp(5, 10.0, 90.0))
            Fail("T17 AddGrayRamp(5,10,90) refused");
        const double want2[5] = { 10.0, 30.0, 50.0, 70.0, 90.0 };
        for (int i = 0; i < 5; ++i)
        {
            double rgb[3];
            if (!t.GetPatch(i, rgb, 0))
                { Fail("T17 gray2 GetPatch(%d) failed", i); continue; }
            if (rgb[0] != want2[i])
                Fail("T17 gray2 level %d = %.17g want %.17g", i, rgb[0], want2[i]);
        }
        if (t.AddGrayRamp(1, 0, 100) || t.AddGrayRamp(3, 50, 50)
            || t.AddGrayRamp(3, 80, 20) || t.AddGrayRamp(3, -1, 50)
            || t.AddGrayRamp(3, 0, sqrt(-1.0)))
            Fail("T17 AddGrayRamp accepted invalid arguments");
        if (t.Count() != 5)
            Fail("T17 refused AddGrayRamp changed the sequence");
    }

    // Channel ramps: swept channel on the grid, others exactly 0, channel
    // words in the names; bad channel refused.
    {
        PatchSequence s;
        if (!s.AddChannelRamp(0, 3, 0.0, 100.0)
            || !s.AddChannelRamp(1, 3, 0.0, 100.0)
            || !s.AddChannelRamp(2, 3, 0.0, 100.0))
            Fail("T17 AddChannelRamp refused valid arguments");
        if (s.Count() != 9)
            Fail("T17 channel ramps count %d want 9", s.Count());
        const char* words[3] = { "Red", "Green", "Blue" };
        for (int c = 0; c < 3; ++c)
            for (int i = 0; i < 3; ++i)
            {
                double rgb[3];
                std::string nm;
                if (!s.GetPatch(c * 3 + i, rgb, &nm))
                    { Fail("T17 channel GetPatch failed"); continue; }
                double lvl = i * 50.0;
                for (int k = 0; k < 3; ++k)
                {
                    double wantV = (k == c) ? lvl : 0.0;
                    if (rgb[k] != wantV)
                        Fail("T17 channel %d step %d comp %d = %.17g want %.17g",
                             c, i, k, rgb[k], wantV);
                }
                if (nm.compare(0, strlen(words[c]), words[c]) != 0)
                    Fail("T17 channel %d name \"%s\" lacks \"%s\"",
                         c, nm.c_str(), words[c]);
            }
        if (s.AddChannelRamp(3, 3, 0, 100) || s.AddChannelRamp(-1, 3, 0, 100))
            Fail("T17 AddChannelRamp accepted a bad channel");
    }

    // Cube grid: n^3 patches, red slowest / blue fastest, exact node
    // values; shell = grid minus interior, in grid order, name-identical.
    {
        PatchSequence g;
        if (!g.AddCubeGrid(3))
            Fail("T17 AddCubeGrid(3) refused");
        if (g.Count() != 27)
            Fail("T17 cube grid count %d want 27", g.Count());
        int idx = 0;
        for (int r = 0; r < 3; ++r)
            for (int gg = 0; gg < 3; ++gg)
                for (int b = 0; b < 3; ++b, ++idx)
                {
                    double rgb[3];
                    if (!g.GetPatch(idx, rgb, 0))
                        { Fail("T17 cube GetPatch(%d) failed", idx); continue; }
                    if (rgb[0] != r * 50.0 || rgb[1] != gg * 50.0
                        || rgb[2] != b * 50.0)
                        Fail("T17 cube node %d = (%.17g,%.17g,%.17g) want "
                             "(%g,%g,%g)", idx, rgb[0], rgb[1], rgb[2],
                             r * 50.0, gg * 50.0, b * 50.0);
                }
        if (g.AddCubeGrid(1) || g.AddCubeGrid(34))
            Fail("T17 AddCubeGrid accepted a bad size");

        PatchSequence grid4, shell4;
        if (!grid4.AddCubeGrid(4) || !shell4.AddCubeShell(4))
            Fail("T17 size-4 grid/shell refused");
        if (shell4.Count() != 64 - 8)
            Fail("T17 shell count %d want 56", shell4.Count());
        int si = 0;
        for (int i = 0; i < grid4.Count(); ++i)
        {
            double rgb[3];
            std::string nm;
            if (!grid4.GetPatch(i, rgb, &nm))
                { Fail("T17 grid4 GetPatch failed"); continue; }
            bool surface = false;
            for (int k = 0; k < 3; ++k)
                if (rgb[k] == 0.0 || rgb[k] == 100.0)
                    surface = true;
            if (!surface)
                continue;                       // interior: shell skips it
            double srgb[3];
            std::string snm;
            if (si >= shell4.Count() || !shell4.GetPatch(si, srgb, &snm))
                { Fail("T17 shell ran short at grid node %d", i); break; }
            for (int k = 0; k < 3; ++k)
                if (srgb[k] != rgb[k])
                    Fail("T17 shell[%d] differs from surface grid node %d "
                         "comp %d", si, i, k);
            if (snm != nm)
                Fail("T17 shell[%d] name \"%s\" want \"%s\"",
                     si, snm.c_str(), nm.c_str());
            ++si;
        }
        if (si != shell4.Count())
            Fail("T17 shell has %d extra patches", shell4.Count() - si);
    }

    // Deduplicate: gray ramp then cube grid - the three shared neutral
    // nodes vanish, first occurrence (the gray name) wins, order stable.
    {
        PatchSequence s;
        if (!s.AddGrayRamp(5, 0.0, 100.0) || !s.AddCubeGrid(3))
            Fail("T17 dedupe setup refused");
        int removed = s.Deduplicate();
        if (removed != 3)
            Fail("T17 Deduplicate removed %d want 3", removed);
        if (s.Count() != 5 + 27 - 3)
            Fail("T17 dedupe count %d want 29", s.Count());
        double rgb[3];
        std::string nm;
        if (!s.GetPatch(0, rgb, &nm) || nm != "Gray 0")
            Fail("T17 dedupe did not keep the first occurrence (got \"%s\")",
                 nm.c_str());
        // (50,50,50) must survive exactly once, named by the gray ramp.
        int hits = 0;
        std::string hitName;
        for (int i = 0; i < s.Count(); ++i)
            if (s.GetPatch(i, rgb, &nm)
                && rgb[0] == 50.0 && rgb[1] == 50.0 && rgb[2] == 50.0)
                { ++hits; hitName = nm; }
        if (hits != 1 || hitName != "Gray 50")
            Fail("T17 dedupe neutral 50: %d hits, name \"%s\"",
                 hits, hitName.c_str());
    }

    // Determinism: identical builds are identical, bitwise and by name.
    {
        PatchSequence a, b;
        for (int t = 0; t < 2; ++t)
        {
            PatchSequence& s = (t == 0) ? a : b;
            s.AddGrayRamp(9, 0.0, 100.0);
            s.AddChannelRamp(1, 4, 5.0, 95.0);
            s.AddCubeShell(3);
            s.AddPatch(12.5, 33.25, 90.0, "Dark Skin");
            s.Deduplicate();
        }
        if (!SameSequences(a, b))
            Fail("T17 identical builds produced different sequences");
    }

    // Exact expected-bytes oracle for the writer (8-bit LEGAL: 0% -> 16,
    // 25% -> 71, 100% -> 235, matching the shipped CM patch lists), and
    // the empty-name row form.
    {
        PatchSequence s;
        s.AddPatch(0, 0, 0, "Black");
        s.AddPatch(100, 100, 100, "White");
        s.AddPatch(25, 0, 0, "");
        std::string out = "sentinel";
        if (!s.WriteCsv(out, 8, CSV_RANGE_LEGAL))
            Fail("T17 WriteCsv refused a valid sequence: %s",
                 s.LastError().c_str());
        else
        {
            const char* want =
                "# BITDEPTH 8, RANGE LEGAL\n"
                "0,16,16,16,Black\n"
                "1,235,235,235,White\n"
                "2,71,16,16\n";
            if (out != want)
                Fail("T17 WriteCsv bytes differ:\n---got---\n%s---want---\n%s",
                     out.c_str(), want);
            if (!s.LastError().empty())
                Fail("T17 successful WriteCsv left LastError set");
        }
        // FULL rounds against the full code range: 50% of 255 -> 128.
        PatchSequence f;
        f.AddPatch(50, 0, 100, "");
        if (!f.WriteCsv(out, 8, CSV_RANGE_FULL))
            Fail("T17 WriteCsv FULL refused");
        else if (out != "# BITDEPTH 8, RANGE FULL\n0,128,0,255\n")
            Fail("T17 WriteCsv FULL bytes differ:\n%s", out.c_str());
    }

    // Writer refusals: empty sequence, bad bits, bad range - output
    // untouched, LastError set.
    {
        PatchSequence empty, one;
        one.AddPatch(50, 50, 50, "x");
        std::string out = "keep";
        if (empty.WriteCsv(out, 8, CSV_RANGE_LEGAL))
            Fail("T17 WriteCsv accepted an empty sequence");
        if (one.WriteCsv(out, 9, CSV_RANGE_LEGAL))
            Fail("T17 WriteCsv accepted bits=9");
        if (one.WriteCsv(out, 8, CSV_RANGE_NONE))
            Fail("T17 WriteCsv accepted CSV_RANGE_NONE");
        if (out != "keep")
            Fail("T17 refused WriteCsv touched the output");
        if (one.LastError().empty())
            Fail("T17 refused WriteCsv left LastError empty");
        if (one.WriteCsvFile("no_such_dir_t17\\x.csv", 8, CSV_RANGE_LEGAL))
            Fail("T17 WriteCsvFile accepted an unwritable path");
    }

    // Integration round trip through the REAL importer at every encoding:
    // count, 0-based patch numbers, and names preserved; every percent
    // reconstructed within half a code step. The file bytes must equal the
    // WriteCsv string (binary write - no CRLF translation).
    {
        PatchSequence s;
        s.AddGrayRamp(9, 0.0, 100.0);           // 12.5% steps: quantization real
        s.AddChannelRamp(0, 4, 5.0, 95.0);
        s.AddChannelRamp(2, 3, 0.0, 100.0);
        s.AddPatch(12.5, 33.25, 90.0, "Dark Skin");
        const int bitsList[2] = { 8, 10 };
        const int rangeList[3] =
            { CSV_RANGE_FULL, CSV_RANGE_LEGAL, CSV_RANGE_EXTENDED };
        for (int bi = 0; bi < 2; ++bi)
            for (int ri = 0; ri < 3; ++ri)
            {
                int bits = bitsList[bi], range = rangeList[ri];
                std::string text;
                if (!s.WriteCsv(text, bits, range))
                    { Fail("T17 rt WriteCsv(%d,%d) refused", bits, range); continue; }
                if (!s.WriteCsvFile(kTmpCsv, bits, range))
                    { Fail("T17 rt WriteCsvFile(%d,%d) refused", bits, range); continue; }
                {
                    // File bytes == string (binary write).
                    FILE* f = fopen(kTmpCsv, "rb");
                    if (!f)
                        Fail("T17 rt cannot reopen %s", kTmpCsv);
                    else
                    {
                        std::string bytes;
                        char buf[512];
                        size_t got;
                        while ((got = fread(buf, 1, sizeof(buf), f)) > 0)
                            bytes.append(buf, got);
                        fclose(f);
                        if (bytes != text)
                            Fail("T17 rt file bytes differ from WriteCsv "
                                 "(%d,%d)", bits, range);
                    }
                }
                std::vector<CsvPatchRow> rows;
                int gotBits = 0, gotRange = -9;
                int n = ReadCsvPatchRows(CString(kTmpCsv), rows, 10000,
                                         &gotBits, &gotRange);
                if (n != s.Count())
                    { Fail("T17 rt (%d,%d) importer read %d of %d rows",
                           bits, range, n, s.Count()); continue; }
                if (gotBits != bits || gotRange != range)
                    Fail("T17 rt (%d,%d) importer saw header bits=%d range=%d",
                         bits, range, gotBits, gotRange);
                double bound = HalfStepPct(bits, range) + 1e-9;
                for (int i = 0; i < n; ++i)
                {
                    double rgb[3];
                    std::string nm;
                    if (!s.GetPatch(i, rgb, &nm))
                        { Fail("T17 rt GetPatch(%d) failed", i); continue; }
                    if (rows[i].patch != i)
                        Fail("T17 rt (%d,%d) row %d has patch number %d",
                             bits, range, i, rows[i].patch);
                    if (rows[i].name != nm)
                        Fail("T17 rt (%d,%d) row %d name \"%s\" want \"%s\"",
                             bits, range, i, rows[i].name.c_str(), nm.c_str());
                    int codes[3] = { rows[i].r, rows[i].g, rows[i].b };
                    for (int k = 0; k < 3; ++k)
                    {
                        double back = CsvCodeToPercent(codes[k], bits, range);
                        if (fabs(back - rgb[k]) > bound)
                            Fail("T17 rt (%d,%d) row %d comp %d: %.17g back "
                                 "%.17g (bound %.3g)", bits, range, i, k,
                                 rgb[k], back, bound);
                    }
                }
            }
        remove(kTmpCsv);
    }
}

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// T20 — patch sequence file oracle (pure assertion, no golden file).
// Pins the .hps dialect byte-for-byte for a small sequence (the format IS
// the deliverable), the two CSV imports, the CSV export, identity /
// role / timing round trips, the repeat rules, the extended span, the
// three new placement primitives, forward compatibility (unknown keys and
// columns ignored, a newer major version refused), and the row cap. Every
// expectation is stated exactly; nothing here is a tolerance.
//////////////////////////////////////////////////////////////////////////

namespace T20File
{
    static const char* kTmpHps  = "t20_roundtrip.hps";
    static const char* kTmpCsv  = "t20_import.csv";

    static bool WriteTextFile(const char* path, const std::string& text)
    {
        FILE* f = fopen(path, "wb");
        if (!f)
            return false;
        size_t got = fwrite(text.data(), 1, text.size(), f);
        fclose(f);
        return got == text.size();
    }

    static bool ReadTextFile(const char* path, std::string& text)
    {
        FILE* f = fopen(path, "rb");
        if (!f)
            return false;
        char buf[4096];
        text.clear();
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
            text.append(buf, n);
        fclose(f);
        return true;
    }

    // The pinned example: three definitions, one anchor repeat, one
    // interstitial, one timing override, one group. 10-bit LEGAL.
    static bool BuildPinned(PatchSequence& s)
    {
        bool ok = true;
        ok = ok && s.AddRow(1, 50.0, 50.0, 50.0, "Gray 50", 'A');
        ok = ok && s.AddRow(2, 100.0, 0.0, 0.0, "Red 100", 'C');
        ok = ok && s.AddRow(0, 0.0, 0.0, 0.0, "Background", 'I');
        // 1% is not on the 10-bit legal grid (64 + 8.76 = 72.76); the pinned
        // row stores exactly what code 73 decodes to, so the exact R,G,B
        // comparison after a parse does real work.
        const double gray1 = CsvCodeToPercent(73, 10, CSV_RANGE_LEGAL);
        ok = ok && s.AddRow(3, gray1, gray1, gray1, "Gray 1", 'C');
        ok = ok && s.AddRow(1, 50.0, 50.0, 50.0, "Gray 50", 'A');
        if (!ok)
            return false;
        PatchTiming t;
        t.hold = 8.0;
        ok = ok && s.SetTiming(2, t);
        PatchTiming d;
        d.settle = 0.0;
        d.average = 10;
        ok = ok && s.SetTiming(3, d);
        ok = ok && s.SetGroup(3, "dark");
        ok = ok && s.SetGroup(1, "bright");
        return ok;
    }

    static void FillPinnedHeader(PatchSequenceHeader& h)
    {
        h.bits = 10;
        h.range = CSV_RANGE_LEGAL;
        h.sequenceId = "7a1c9e2e-3b0f-4d61-9f2a-0c5e8b4d1a77";
        h.name = "Pinned example";
        h.created = "2026-09-06T18:14:03Z";
        h.generator = "HCFR 3.5.3.11";
        h.sourceKind = "recipe";
        h.required.push_back("panel-record");
        h.panelRecord = "a80j-2026-09-06";
        h.steps.push_back("AddGrayRamp 21 0 100");
        h.steps.push_back("Deduplicate");
        h.geometryArea = 10;
        h.geometryBackground = 64;
        PatchGroupSpec dark;
        dark.name = "dark";
        dark.timing.integration = 2.0;
        dark.timing.average = 5;
        dark.timing.settle = 1.5;
        h.groups.push_back(dark);
        PatchGroupSpec bright;
        bright.name = "bright";
        bright.timing.integration = 0.2;
        bright.timing.settle = 0.8;
        bright.readAt = "plateau";
        h.groups.push_back(bright);
        h.order = "constant-load";
        h.targetLoad = 0.031;
        h.loadModel = "wrgb-power kw=1.00 kr=1.10 kg=1.00 kb=1.35 gamma=2.2";
        h.window = 38.0;
        h.dwellLatency = 0.4;
        h.maxStep = 0.05;
        h.settleAfterStep = 2.5;
        h.ablLimit = 25.0;
        PatchAnchorSpec a;
        a.id = 1;
        a.every = 100;
        h.anchors.push_back(a);
        h.prerollId = 1;
        h.prerollCount = 0;
    }

    // Codes at 10-bit legal: 64 + pct * 8.76, nearest integer.
    static const char* kPinnedText =
        "# HCFR-SEQUENCE 1\n"
        "# BITDEPTH 10, RANGE LEGAL\n"
        "# SEQUENCE-ID 7a1c9e2e-3b0f-4d61-9f2a-0c5e8b4d1a77\n"
        "# NAME \"Pinned example\"\n"
        "# CREATED 2026-09-06T18:14:03Z\n"
        "# GENERATOR \"HCFR 3.5.3.11\"\n"
        "# SOURCE recipe\n"
        "# REQUIRES panel-record\n"
        "# PANEL-RECORD a80j-2026-09-06\n"
        "# STEP AddGrayRamp 21 0 100\n"
        "# STEP Deduplicate\n"
        "# PATCH-GEOMETRY area=10 background=64\n"
        "# GROUP dark integration=2 average=5 settle=1.5\n"
        "# GROUP bright integration=0.2 settle=0.8 readat=plateau\n"
        "# ORDER constant-load\n"
        "# TARGET-LOAD 0.031\n"
        "# LOAD-MODEL wrgb-power kw=1.00 kr=1.10 kg=1.00 kb=1.35 gamma=2.2\n"
        "# WINDOW 38\n"
        "# DWELL latency=0.4\n"
        "# MAX-STEP 0.05\n"
        "# SETTLE-AFTER-STEP 2.5\n"
        "# ABL-LIMIT apl=25\n"
        "# ANCHOR 1 every=100\n"
        "# PREROLL 1 count=0\n"
        "# COLUMNS PatchNumber,R,G,B,Name,Role,Group,Settle,Hold,Integration,Average\n"
        "1,502,502,502,Gray 50,A,,,,,\n"
        "2,940,64,64,Red 100,C,bright,,,,\n"
        "0,64,64,64,Background,I,,,8,,\n"
        "3,73,73,73,Gray 1,C,dark,0,,,10\n"
        "1,502,502,502,Gray 50,A,,,,,\n";

    static bool SameRows(const PatchSequence& a, const PatchSequence& b, const char* what)
    {
        if (a.Count() != b.Count())
        {
            Fail("T20 %s: count %d want %d", what, b.Count(), a.Count());
            return false;
        }
        bool same = true;
        for (int i = 0; i < a.Count(); ++i)
        {
            double ra[3], rb[3];
            std::string na, nb, ga, gb;
            PatchTiming ta, tb;
            a.GetPatch(i, ra, &na); b.GetPatch(i, rb, &nb);
            a.GetGroup(i, ga);      b.GetGroup(i, gb);
            a.GetTiming(i, ta);     b.GetTiming(i, tb);
            if (a.GetIdentity(i) != b.GetIdentity(i) || a.GetRole(i) != b.GetRole(i)
                || ra[0] != rb[0] || ra[1] != rb[1] || ra[2] != rb[2]
                || na != nb || ga != gb
                || ta.settle != tb.settle || ta.hold != tb.hold
                || ta.integration != tb.integration || ta.average != tb.average)
            {
                Fail("T20 %s: row %d differs (id %d/%d role %c/%c name '%s'/'%s' group '%s'/'%s')",
                     what, i, a.GetIdentity(i), b.GetIdentity(i), a.GetRole(i) ? a.GetRole(i) : '?', b.GetRole(i) ? b.GetRole(i) : '?',
                     na.c_str(), nb.c_str(), ga.c_str(), gb.c_str());
                same = false;
            }
        }
        return same;
    }
}

static void RunT20()
{
    printf("T20 patch sequence file oracle...\n");
    using namespace T20File;

    // (1) Identity, role, group, timing on the builder; AddRow rules.
    {
        PatchSequence s;
        if (!s.AddPatch(0, 0, 0, "Black") || !s.AddPatch(100, 100, 100, "White"))
            Fail("T20 AddPatch refused");
        if (s.GetIdentity(0) != 0 || s.GetIdentity(1) != 1)
            Fail("T20 fresh identities not 0,1 (got %d,%d)", s.GetIdentity(0), s.GetIdentity(1));
        if (s.GetRole(0) != 'C' || s.GetRole(1) != 'C')
            Fail("T20 default role not C");
        if (s.GetIdentity(2) != -1 || s.GetRole(2) != '\0')
            Fail("T20 out-of-range identity/role not -1/NUL");
        std::string g;
        PatchTiming t;
        if (!s.GetGroup(0, g) || !g.empty() || !s.GetTiming(0, t)
            || t.settle != -1.0 || t.hold != -1.0 || t.integration != -1.0 || t.average != -1)
            Fail("T20 default group/timing not empty/-1");
        if (s.GetGroup(5, g) || s.GetTiming(5, t))
            Fail("T20 out-of-range GetGroup/GetTiming succeeded");

        if (!s.SetRole(1, 'A') || s.GetRole(1) != 'A')
            Fail("T20 SetRole A refused or not stored");
        if (s.SetRole(1, 'Q') || s.SetRole(1, 'c') || s.SetRole(9, 'C'))
            Fail("T20 SetRole accepted a bad role or index");
        if (s.GetRole(1) != 'A')
            Fail("T20 refused SetRole changed the role");
        if (!s.SetGroup(0, "dark") || !s.GetGroup(0, g) || g != "dark")
            Fail("T20 SetGroup refused or not stored");
        if (s.SetGroup(0, "two words") || s.SetGroup(0, "a,b") || s.SetGroup(0, " x") || s.SetGroup(7, "x"))
            Fail("T20 SetGroup accepted a bad group or index");
        if (!s.GetGroup(0, g) || g != "dark")
            Fail("T20 refused SetGroup changed the group");
        if (!s.SetGroup(0, ""))
            Fail("T20 SetGroup refused clearing to empty");
        PatchTiming ok;
        ok.settle = 0.0; ok.hold = 8.0; ok.integration = 2.0; ok.average = 10;
        if (!s.SetTiming(0, ok) || !s.GetTiming(0, t)
            || t.settle != 0.0 || t.hold != 8.0 || t.integration != 2.0 || t.average != 10)
            Fail("T20 SetTiming refused or not stored");
        PatchTiming bad;
        bad.settle = -2.0;
        if (s.SetTiming(0, bad))
            Fail("T20 SetTiming accepted settle -2");
        bad = PatchTiming();
        bad.average = 0;
        if (s.SetTiming(0, bad))
            Fail("T20 SetTiming accepted average 0");
        bad = PatchTiming();
        bad.hold = sqrt(-1.0);
        if (s.SetTiming(0, bad))
            Fail("T20 SetTiming accepted a NaN hold");
        if (!s.GetTiming(0, t) || t.hold != 8.0)
            Fail("T20 refused SetTiming changed the timing");

        // AddRow: explicit identity and role; repeats must repeat.
        if (!s.AddRow(7, 25, 25, 25, "Gray 25", 'V'))
            Fail("T20 AddRow refused a fresh identity");
        if (s.GetIdentity(2) != 7 || s.GetRole(2) != 'V')
            Fail("T20 AddRow identity/role not stored");
        if (!s.AddPatch(75, 75, 75, "Gray 75") || s.GetIdentity(3) != 8)
            Fail("T20 fresh identity after AddRow(7) is not 8 (got %d)", s.GetIdentity(3));
        if (!s.AddRow(7, 25, 25, 25, "Gray 25 again", 'C'))
            Fail("T20 AddRow refused a faithful repeat");
        if (s.AddRow(7, 25, 25, 26, "Gray 25 wrong", 'C'))
            Fail("T20 AddRow accepted a repeat with different rgb");
        if (s.AddRow(-1, 0, 0, 0, "", 'C') || s.AddRow(9, 0, 0, 0, "", 'Z')
            || s.AddRow(9, 0, 0, 101, "", 'C') || s.AddRow(9, 0, 0, 0, "bad,name", 'C'))
            Fail("T20 AddRow accepted a bad id/role/component/name");
        if (s.Count() != 5)
            Fail("T20 refused AddRow changed the count (%d)", s.Count());
        std::string nm;
        double rgb[3];
        if (!s.GetPatch(4, rgb, &nm) || nm != "Gray 25 again" || rgb[0] != 25.0)
            Fail("T20 repeat row not stored as given");
    }

    // (2) Deduplicate: definitions only, role C only, renumbered, repeats follow.
    {
        PatchSequence s;
        s.AddPatch(0, 0, 0, "Black");          // id 0
        s.AddPatch(50, 50, 50, "Gray 50");     // id 1
        s.AddPatch(0, 0, 0, "Black dup");      // id 2, duplicate of 0
        s.AddRow(2, 0, 0, 0, "Black dup rep", 'C');  // repeat of the duplicate
        s.AddRow(1, 50, 50, 50, "Gray 50", 'A');     // anchor repeat of 1
        s.AddRow(9, 0, 0, 0, "Black anchor", 'A');   // role A definition, equal rgb: never removed
        s.AddPatch(100, 100, 100, "White");    // id 10
        int removed = s.Deduplicate();
        if (removed != 1)
            Fail("T20 Deduplicate removed %d want 1", removed);
        if (s.Count() != 6)
            Fail("T20 Deduplicate count %d want 6", s.Count());
        // Surviving definitions in order: 0 Black, 1 Gray 50, (rep), (anchor rep), 9 Black anchor, 10 White
        // -> renumbered 0, 1, -, -, 2, 3; the repeat of the removed duplicate now refers to 0.
        const int wantId[6] = { 0, 1, 0, 1, 2, 3 };
        const char wantRole[6] = { 'C', 'C', 'C', 'A', 'A', 'C' };
        for (int i = 0; i < 6; ++i)
            if (s.GetIdentity(i) != wantId[i] || s.GetRole(i) != wantRole[i])
                Fail("T20 Deduplicate row %d id %d role %c want %d %c", i,
                     s.GetIdentity(i), s.GetRole(i) ? s.GetRole(i) : '?', wantId[i], wantRole[i]);
        std::string nm;
        double rgb[3];
        if (!s.GetPatch(2, rgb, &nm) || nm != "Black dup rep")
            Fail("T20 Deduplicate dropped the repeat of a removed duplicate");
        // Deduplicate with no duplicates is a no-op that still renumbers contiguously.
        PatchSequence g;
        g.AddRow(5, 0, 0, 0, "a", 'C');
        g.AddRow(9, 1, 1, 1, "b", 'C');
        if (g.Deduplicate() != 0 || g.GetIdentity(0) != 0 || g.GetIdentity(1) != 1)
            Fail("T20 Deduplicate did not renumber a gap-free-able sequence");
    }

    // (3) Extended span.
    {
        PatchSequence s;
        if (s.IsExtended())
            Fail("T20 new sequence is extended");
        if (s.AddPatch(105, 0, 0, "wtw"))
            Fail("T20 AddPatch accepted 105 while not extended");
        if (!s.SetExtended(true) || !s.IsExtended())
            Fail("T20 SetExtended(true) refused");
        if (!s.AddPatch(109.47, 0, 0, "wtw") || !s.AddPatch(-7.3, 0, 0, "btb"))
            Fail("T20 extended AddPatch refused an in-span value");
        if (s.AddPatch(109.48, 0, 0, "over") || s.AddPatch(-7.31, 0, 0, "under"))
            Fail("T20 extended AddPatch accepted an out-of-span value");
        if (s.SetExtended(false))
            Fail("T20 SetExtended(false) accepted with out-of-span entries present");
        if (!s.IsExtended() || s.Count() != 2)
            Fail("T20 refused SetExtended(false) changed state");
        std::string out;
        if (s.WriteCsv(out, 10, CSV_RANGE_LEGAL))
            Fail("T20 WriteCsv LEGAL accepted out-of-span components");
        if (!s.WriteCsv(out, 10, CSV_RANGE_EXTENDED))
            Fail("T20 WriteCsv EXTENDED refused: %s", s.LastError().c_str());
        else if (out != "# BITDEPTH 10, RANGE EXTENDED\n0,1023,64,64,wtw\n1,0,64,64,btb\n")
            Fail("T20 WriteCsv EXTENDED bytes:\n%s", out.c_str());
        PatchSequence e;
        if (!e.SetExtended(true) || !e.AddPatch(50, 50, 50, "mid") || !e.SetExtended(false) || e.IsExtended())
            Fail("T20 SetExtended(false) refused with every entry in 0..100");
    }

    // (4) New placement primitives.
    {
        PatchSequence s;
        if (!s.AddGrayPowerRamp(5, 0.0, 10.0, 2.0))
            Fail("T20 AddGrayPowerRamp refused");
        else
        {
            // u = 0, .25, .5, .75, 1 -> 0, 0.625, 2.5, 5.625, 10 (exact in binary)
            const double want[5] = { 0.0, 0.625, 2.5, 5.625, 10.0 };
            const char* wantName[5] = { "Gray 0", "Gray 0.625", "Gray 2.5", "Gray 5.625", "Gray 10" };
            for (int i = 0; i < 5; ++i)
            {
                double rgb[3];
                std::string nm;
                s.GetPatch(i, rgb, &nm);
                if (rgb[0] != want[i] || rgb[1] != want[i] || rgb[2] != want[i] || nm != wantName[i])
                    Fail("T20 power ramp level %d = %.17g '%s' want %.17g '%s'", i, rgb[0], nm.c_str(), want[i], wantName[i]);
                if (s.GetIdentity(i) != i || s.GetRole(i) != 'C')
                    Fail("T20 power ramp identity/role wrong at %d", i);
            }
        }
        if (s.AddGrayPowerRamp(5, 0.0, 10.0, 0.0) || s.AddGrayPowerRamp(5, 0.0, 10.0, -1.0)
            || s.AddGrayPowerRamp(5, 0.0, 10.0, sqrt(-1.0)) || s.AddGrayPowerRamp(1, 0.0, 10.0, 2.0)
            || s.AddGrayPowerRamp(5, 10.0, 0.0, 2.0))
            Fail("T20 AddGrayPowerRamp accepted a bad argument");
        if (s.Count() != 5)
            Fail("T20 refused AddGrayPowerRamp changed the count");
        // Endpoint-exact with an exponent that would not be through pow: 1/3 grid at exponent 3.
        PatchSequence p;
        p.AddGrayPowerRamp(4, 20.0, 80.0, 3.0);
        double rgb[3];
        if (!p.GetPatch(0, rgb, 0) || rgb[0] != 20.0) Fail("T20 power ramp first level not exactly lo");
        if (!p.GetPatch(3, rgb, 0) || rgb[0] != 80.0) Fail("T20 power ramp last level not exactly hi");

        PatchSequence i;
        if (!i.AddShellInset(3, 5.0))
            Fail("T20 AddShellInset refused");
        else
        {
            if (i.Count() != 26)
                Fail("T20 AddShellInset(3) count %d want 26", i.Count());
            bool bad = false;
            for (int k = 0; k < i.Count(); ++k)
            {
                std::string nm;
                i.GetPatch(k, rgb, &nm);
                bool face = false;
                for (int c = 0; c < 3; ++c)
                {
                    if (rgb[c] < 5.0 || rgb[c] > 95.0) bad = true;
                    if (rgb[c] == 5.0 || rgb[c] == 95.0) face = true;
                }
                if (!face) bad = true;
                if (nm.compare(0, 6, "Inset ") != 0) bad = true;
            }
            if (bad)
                Fail("T20 AddShellInset nodes not in [5,95] with a face component / bad name");
            i.GetPatch(0, rgb, 0);
            if (rgb[0] != 5.0 || rgb[1] != 5.0 || rgb[2] != 5.0)
                Fail("T20 AddShellInset first node not 5/5/5");
            i.GetPatch(25, rgb, 0);
            if (rgb[0] != 95.0 || rgb[1] != 95.0 || rgb[2] != 95.0)
                Fail("T20 AddShellInset last node not 95/95/95");
        }
        if (i.AddShellInset(3, 0.0) || i.AddShellInset(3, 50.0) || i.AddShellInset(1, 5.0) || i.AddShellInset(34, 5.0))
            Fail("T20 AddShellInset accepted a bad argument");

        PatchSequence r;
        r.AddGrayRamp(9, 0.0, 100.0);          // ids 0..8, role C
        r.AddRow(20, 50, 0, 0, "Red half", 'A');   // not a C definition
        if (!r.AddRepeats(3))
            Fail("T20 AddRepeats refused");
        else
        {
            // k = floor(9/3) = 3 -> definitions 0, 3, 6 in identity order.
            if (r.Count() != 13)
                Fail("T20 AddRepeats count %d want 13", r.Count());
            const int wantId[3] = { 0, 3, 6 };
            for (int k = 0; k < 3; ++k)
            {
                std::string nm;
                r.GetPatch(10 + k, rgb, &nm);
                if (r.GetIdentity(10 + k) != wantId[k] || r.GetRole(10 + k) != 'C'
                    || rgb[0] != wantId[k] * 12.5 || nm != std::string("Gray ") + (k == 0 ? "0" : k == 1 ? "37.5" : "75") + " repeat")
                    Fail("T20 AddRepeats row %d: id %d name '%s'", k, r.GetIdentity(10 + k), nm.c_str());
            }
        }
        if (r.AddRepeats(0) || r.AddRepeats(10))
            Fail("T20 AddRepeats accepted count 0 or > definitions");
        PatchSequence e;
        e.AddPatch(0, 0, 0, "");
        if (!e.AddRepeats(1))
            Fail("T20 AddRepeats(1) on one definition refused");
        else
        {
            std::string nm;
            e.GetPatch(1, rgb, &nm);
            if (nm != "repeat")
                Fail("T20 AddRepeats empty-name form '%s' want 'repeat'", nm.c_str());
        }
    }

    // (5) .hps format pinned byte-for-byte; parse reproduces rows and header.
    {
        PatchSequence s;
        PatchSequenceHeader h;
        FillPinnedHeader(h);
        if (!BuildPinned(s))
            Fail("T20 pinned sequence could not be built");
        PatchSequenceFile f;
        std::string out;
        if (!f.FormatHps(out, s, h))
            Fail("T20 FormatHps refused: %s", f.LastError().c_str());
        else if (out != kPinnedText)
            Fail("T20 FormatHps bytes differ:\n---got---\n%s---want---\n%s", out.c_str(), kPinnedText);

        PatchSequence back;
        PatchSequenceHeader hb;
        if (!f.ParseHps(kPinnedText, back, hb))
            Fail("T20 ParseHps refused the pinned text: %s", f.LastError().c_str());
        else
        {
            SameRows(s, back, "pinned parse");
            if (hb.version != 1 || hb.bits != 10 || hb.range != CSV_RANGE_LEGAL
                || hb.sequenceId != h.sequenceId || hb.name != h.name || hb.created != h.created
                || hb.generator != h.generator || hb.sourceKind != "recipe"
                || hb.required.size() != 1 || hb.required[0] != "panel-record"
                || hb.panelRecord != h.panelRecord
                || hb.steps.size() != 2 || hb.steps[0] != h.steps[0] || hb.steps[1] != h.steps[1]
                || hb.geometryArea != 10 || hb.geometryBackground != 64
                || hb.groups.size() != 2 || hb.groups[0].name != "dark"
                || hb.groups[0].timing.integration != 2.0 || hb.groups[0].timing.average != 5
                || hb.groups[0].timing.settle != 1.5 || hb.groups[0].timing.hold != -1.0
                || hb.groups[1].name != "bright" || hb.groups[1].readAt != "plateau"
                || hb.groups[1].timing.integration != 0.2 || hb.groups[1].timing.average != -1
                || hb.order != "constant-load" || hb.targetLoad != 0.031
                || hb.loadModel != h.loadModel || hb.window != 38.0 || hb.dwellLatency != 0.4
                || hb.maxStep != 0.05 || hb.settleAfterStep != 2.5 || hb.ablLimit != 25.0
                || hb.anchors.size() != 1 || hb.anchors[0].id != 1 || hb.anchors[0].every != 100
                || hb.prerollId != 1 || hb.prerollCount != 0
                || !hb.displayProfile.empty() || !hb.target.empty() || !hb.readAt.empty())
                Fail("T20 ParseHps header differs from the pinned header");
            // Second format is byte-identical to the first.
            std::string again;
            if (!f.FormatHps(again, back, hb) || again != kPinnedText)
                Fail("T20 FormatHps of the parsed sequence is not byte-identical");
        }

        // File round trip through Write / Read.
        remove(kTmpHps);
        if (!f.Write(kTmpHps, s, h))
            Fail("T20 Write refused: %s", f.LastError().c_str());
        else
        {
            std::string disk;
            if (!ReadTextFile(kTmpHps, disk) || disk != kPinnedText)
                Fail("T20 written file bytes differ from FormatHps");
            PatchSequence r;
            PatchSequenceHeader hr;
            PatchSequenceFile::Dialect d = PatchSequenceFile::DIALECT_CSV_LEGACY;
            if (!f.Read(kTmpHps, r, hr, &d))
                Fail("T20 Read of the written file refused: %s", f.LastError().c_str());
            else
            {
                if (d != PatchSequenceFile::DIALECT_HPS)
                    Fail("T20 Read dialect %d want HPS", (int)d);
                SameRows(s, r, "file read");
            }
            remove(kTmpHps);
        }
        if (f.Write("no_such_dir_t20\\x.hps", s, h))
            Fail("T20 Write accepted an unwritable path");
        PatchSequence empty;
        if (f.FormatHps(out, empty, h))
            Fail("T20 FormatHps accepted an empty sequence");
    }

    // (6) Forward compatibility and refusals on parse.
    {
        PatchSequenceFile f;
        PatchSequence s;
        PatchSequenceHeader h;
        std::string text = kPinnedText;
        // Unknown directive, an unknown trailing column, a comment after data.
        std::string mod = text;
        size_t p = mod.find("# COLUMNS");
        mod.insert(p, "# FUTURE-KEY something 42\n");
        p = mod.find("Average\n");
        mod.insert(p + 7, ",Extra");
        const std::string rowOld = "2,940,64,64,Red 100,C,bright,,,,\n";
        mod.replace(mod.find(rowOld), rowOld.size(), "2,940,64,64,Red 100,C,bright,,,,,zzz\n");
        mod += "# a trailing comment\n";
        if (!f.ParseHps(mod, s, h))
            Fail("T20 ParseHps refused unknown key/column/comment: %s", f.LastError().c_str());
        else
        {
            std::string g;
            s.GetGroup(1, g);
            if (s.Count() != 5 || s.GetRole(0) != 'A' || g != "bright")
                Fail("T20 ParseHps with unknowns changed the rows (count %d group '%s')", s.Count(), g.c_str());
        }
        std::string v2 = text;
        v2.replace(0, 18, "# HCFR-SEQUENCE 2\n");
        if (f.ParseHps(v2, s, h))
            Fail("T20 ParseHps accepted major version 2");
        std::string mism = text + "3,74,73,73,Gray 1 wrong,C,,,,,\n";
        if (f.ParseHps(mism, s, h))
            Fail("T20 ParseHps accepted a repeat with different codes");
        std::string noline1 = text.substr(18);
        if (f.ParseHps(noline1, s, h))
            Fail("T20 ParseHps accepted text without the HCFR-SEQUENCE line");
        // Row cap: 10001 rows refused, 10000 accepted.
        std::string big = "# HCFR-SEQUENCE 1\n# BITDEPTH 8, RANGE LEGAL\n# COLUMNS PatchNumber,R,G,B,Name,Role,Group,Settle,Hold,Integration,Average\n";
        char row[64];
        for (int i = 0; i < 10001; ++i)
        {
            snprintf(row, sizeof(row), "%d,%d,%d,%d,,C,,,,,\n", i, 16 + (i % 220), 16, 16);
            big += row;
        }
        if (f.ParseHps(big, s, h))
            Fail("T20 ParseHps accepted 10001 rows");
        size_t last = big.rfind("10000,");
        big.erase(last);
        if (!f.ParseHps(big, s, h) || s.Count() != 10000)
            Fail("T20 ParseHps refused 10000 rows: %s", f.LastError().c_str());
        PatchSequence over;
        for (int i = 0; i < 10001; ++i)
            over.AddPatch((i % 101), 0, 0, "");
        std::string out;
        if (f.FormatHps(out, over, h))
            Fail("T20 FormatHps accepted 10001 rows");
    }

    // (7) CSV imports: legacy and headered.
    {
        PatchSequenceFile f;
        PatchSequence s;
        PatchSequenceHeader h;
        PatchSequenceFile::Dialect d;
        // Legacy: no header, R,G,B,name, 8-bit legal, identity = row index.
        WriteTextFile(kTmpCsv, "16,16,16,0 Black\n235,235,235,100 White\n126,126,126\n");
        if (!f.Read(kTmpCsv, s, h, &d))
            Fail("T20 legacy CSV read refused: %s", f.LastError().c_str());
        else
        {
            if (d != PatchSequenceFile::DIALECT_CSV_LEGACY)
                Fail("T20 legacy CSV dialect %d", (int)d);
            if (s.Count() != 3 || s.GetIdentity(2) != 2 || s.GetRole(1) != 'C')
                Fail("T20 legacy CSV rows/identities/roles wrong");
            double rgb[3];
            std::string nm;
            s.GetPatch(1, rgb, &nm);
            if (rgb[0] != 100.0 || nm != "100 White")
                Fail("T20 legacy CSV white = %.17g '%s'", rgb[0], nm.c_str());
            s.GetPatch(2, rgb, &nm);
            if (!nm.empty() || fabs(rgb[0] - CsvCodeToPercent(126, 8, CSV_RANGE_LEGAL)) != 0.0)
                Fail("T20 legacy CSV row 3 wrong");
            if (h.bits != 8 || h.range != CSV_RANGE_LEGAL || h.sourceKind != "csv"
                || h.sourcePath != kTmpCsv || !h.sequenceId.empty() || !h.steps.empty())
                Fail("T20 legacy CSV header wrong (bits %d range %d kind '%s')", h.bits, h.range, h.sourceKind.c_str());
        }
        // Headered, 10-bit EXTENDED with a super-white code and explicit identities.
        WriteTextFile(kTmpCsv, "# BITDEPTH 10, RANGE EXTENDED\n5,64,64,64,Black\n9,1023,1023,1023,Super white\n2,502,502,502\n");
        if (!f.Read(kTmpCsv, s, h, &d))
            Fail("T20 headered CSV read refused: %s", f.LastError().c_str());
        else
        {
            if (d != PatchSequenceFile::DIALECT_CSV_HEADERED)
                Fail("T20 headered CSV dialect %d", (int)d);
            if (s.Count() != 3 || s.GetIdentity(0) != 5 || s.GetIdentity(1) != 9 || s.GetIdentity(2) != 2)
                Fail("T20 headered CSV identities not 5,9,2");
            if (!s.IsExtended())
                Fail("T20 EXTENDED CSV did not switch the sequence to extended");
            double rgb[3];
            s.GetPatch(1, rgb, 0);
            if (rgb[0] != CsvCodeToPercent(1023, 10, CSV_RANGE_EXTENDED))
                Fail("T20 super-white code clamped on import (%.17g)", rgb[0]);
            if (h.bits != 10 || h.range != CSV_RANGE_EXTENDED)
                Fail("T20 headered CSV header bits/range wrong");
        }
        remove(kTmpCsv);
        if (f.Read("no_such_file_t20.csv", s, h, &d))
            Fail("T20 Read accepted a missing file");
    }

    // (8) CSV export: orders and role filtering.
    {
        PatchSequence s;
        BuildPinned(s);
        PatchSequenceFile f;
        std::string out;
        if (!f.ExportCsv(out, s, 10, CSV_RANGE_LEGAL, PatchSequenceFile::EXPORT_MEASUREMENT, false))
            Fail("T20 ExportCsv measurement refused: %s", f.LastError().c_str());
        else if (out != "# BITDEPTH 10, RANGE LEGAL\n2,940,64,64,Red 100\n3,73,73,73,Gray 1\n")
            Fail("T20 ExportCsv measurement bytes:\n%s", out.c_str());
        if (!f.ExportCsv(out, s, 10, CSV_RANGE_LEGAL, PatchSequenceFile::EXPORT_MEASUREMENT, true))
            Fail("T20 ExportCsv with roles refused");
        else if (out != "# BITDEPTH 10, RANGE LEGAL\n1,502,502,502,Gray 50\n2,940,64,64,Red 100\n0,64,64,64,Background\n3,73,73,73,Gray 1\n1,502,502,502,Gray 50\n")
            Fail("T20 ExportCsv with roles bytes:\n%s", out.c_str());
        if (!f.ExportCsv(out, s, 10, CSV_RANGE_LEGAL, PatchSequenceFile::EXPORT_DEFINITION, true))
            Fail("T20 ExportCsv definition refused");
        else if (out != "# BITDEPTH 10, RANGE LEGAL\n0,64,64,64,Background\n1,502,502,502,Gray 50\n2,940,64,64,Red 100\n3,73,73,73,Gray 1\n")
            Fail("T20 ExportCsv definition bytes:\n%s", out.c_str());
        // The definition export re-imports as the definition set.
        if (f.ExportCsvFile(kTmpCsv, s, 10, CSV_RANGE_LEGAL, PatchSequenceFile::EXPORT_DEFINITION, true))
        {
            PatchSequence back;
            PatchSequenceHeader hb;
            if (!f.Read(kTmpCsv, back, hb, 0) || back.Count() != 4
                || back.GetIdentity(0) != 0 || back.GetIdentity(3) != 3)
                Fail("T20 definition export did not re-import as 4 definitions");
            remove(kTmpCsv);
        }
        else
            Fail("T20 ExportCsvFile refused: %s", f.LastError().c_str());
        PatchSequence empty;
        if (f.ExportCsv(out, empty, 10, CSV_RANGE_LEGAL, PatchSequenceFile::EXPORT_MEASUREMENT, true))
            Fail("T20 ExportCsv accepted an empty sequence");
        // Everything filtered out is refused too, output untouched.
        PatchSequence onlyAnchors;
        onlyAnchors.AddRow(1, 50, 50, 50, "Gray 50", 'A');
        std::string keep = "sentinel";
        if (f.ExportCsv(keep, onlyAnchors, 8, CSV_RANGE_LEGAL, PatchSequenceFile::EXPORT_MEASUREMENT, false))
            Fail("T20 ExportCsv accepted a sequence with no exportable rows");
        if (keep != "sentinel")
            Fail("T20 refused ExportCsv touched the output");
    }

    // (9) Review round: contract tightenings from /code-review high.
    {
        // Role predicates are the one home of role knowledge.
        const char roles[6] = { 'C', 'A', 'P', 'V', 'I', 'X' };
        for (int k = 0; k < 6; ++k)
        {
            char r = roles[k];
            if (PatchSequence::RoleIsMeasured(r) != (r != 'I'))
                Fail("T20 RoleIsMeasured(%c) wrong", r);
            if (PatchSequence::RoleIsFitted(r) != (r == 'C' || r == 'V'))
                Fail("T20 RoleIsFitted(%c) wrong", r);
            if (PatchSequence::RoleIsExportable(r) != (r == 'C' || r == 'V'))
                Fail("T20 RoleIsExportable(%c) wrong", r);
        }
        if (PatchSequence::RoleIsMeasured('Q') || PatchSequence::RoleIsFitted('c') || PatchSequence::RoleIsExportable('\0'))
            Fail("T20 role predicates accepted a non-role");
        if (!PatchSequence::ValidGroupName("dark") || PatchSequence::ValidGroupName("") || PatchSequence::ValidGroupName("near black")
            || PatchSequence::ValidGroupName("a,b") || PatchSequence::ValidGroupName("caf\xC3\xA9"))
            Fail("T20 ValidGroupName wrong");

        // Identity cap.
        PatchSequence c;
        if (!c.AddRow(PatchSequence::kMaxIdentity, 0, 0, 0, "top", 'C'))
            Fail("T20 AddRow refused kMaxIdentity");
        if (c.AddRow(PatchSequence::kMaxIdentity + 1, 0, 0, 0, "over", 'C'))
            Fail("T20 AddRow accepted kMaxIdentity + 1");
        if (c.Count() != 1)
            Fail("T20 refused AddRow over the cap changed the count");

        // Deduplicate reports its renumbering; the header follows it.
        PatchSequence s;
        s.AddPatch(0, 0, 0, "Black");          // id 0
        s.AddPatch(50, 50, 50, "Gray 50");     // id 1
        s.AddPatch(0, 0, 0, "Black dup");      // id 2, duplicate of 0
        s.AddRow(2, 0, 0, 0, "Black dup rep", 'C');
        s.AddRow(1, 50, 50, 50, "Gray 50", 'A');
        s.AddRow(9, 0, 0, 0, "Black anchor", 'A');
        s.AddPatch(100, 100, 100, "White");    // id 10
        std::vector< std::pair<int, int> > map;
        if (s.Deduplicate(&map) != 1)
            Fail("T20 Deduplicate(&map) removed count wrong");
        const int wantOld[5] = { 0, 1, 2, 9, 10 };
        const int wantNew[5] = { 0, 1, 0, 2, 3 };
        if (map.size() != 5)
            Fail("T20 renumbering has %d pairs want 5", (int)map.size());
        else
            for (int k = 0; k < 5; ++k)
                if (map[k].first != wantOld[k] || map[k].second != wantNew[k])
                    Fail("T20 renumbering pair %d = (%d,%d) want (%d,%d)", k,
                         map[k].first, map[k].second, wantOld[k], wantNew[k]);
        PatchSequenceHeader h;
        PatchAnchorSpec a9, a2, a42;
        a9.id = 9; a9.every = 100;
        a2.id = 2; a2.every = 50;
        a42.id = 42; a42.every = 10;
        h.anchors.push_back(a9);
        h.anchors.push_back(a2);
        h.anchors.push_back(a42);
        h.prerollId = 10;
        h.RemapIdentities(map);
        if (h.anchors.size() != 3 || h.anchors[0].id != 2 || h.anchors[1].id != 0 || h.anchors[2].id != -1
            || h.anchors[0].every != 100 || h.prerollId != 3)
            Fail("T20 RemapIdentities: anchors %d,%d,%d preroll %d want 2,0,-1 preroll 3",
                 h.anchors.size() > 2 ? h.anchors[0].id : -9, h.anchors.size() > 2 ? h.anchors[1].id : -9,
                 h.anchors.size() > 2 ? h.anchors[2].id : -9, h.prerollId);
        PatchSequenceHeader unset;
        unset.RemapIdentities(map);
        if (unset.prerollId != -1)
            Fail("T20 RemapIdentities changed an unset prerollId");

        // Swap is a real exchange.
        PatchSequence x, y;
        x.AddPatch(10, 10, 10, "x");
        y.AddPatch(20, 20, 20, "y1");
        y.AddPatch(30, 30, 30, "y2");
        x.Swap(y);
        std::string nm;
        double rgb[3];
        if (x.Count() != 2 || y.Count() != 1 || !y.GetPatch(0, rgb, &nm) || nm != "x" || rgb[0] != 10.0)
            Fail("T20 Swap did not exchange contents");

        // Representability: one rule for every writer.
        if (!CsvPercentRepresentable(100.0, 8, CSV_RANGE_LEGAL) || !CsvPercentRepresentable(0.0, 10, CSV_RANGE_FULL)
            || CsvPercentRepresentable(100.01, 8, CSV_RANGE_LEGAL) || CsvPercentRepresentable(-0.5, 10, CSV_RANGE_FULL)
            || !CsvPercentRepresentable(109.13, 8, CSV_RANGE_EXTENDED) || CsvPercentRepresentable(109.47, 8, CSV_RANGE_EXTENDED)
            || !CsvPercentRepresentable(109.47, 10, CSV_RANGE_EXTENDED) || CsvPercentRepresentable(sqrt(-1.0), 8, CSV_RANGE_LEGAL))
            Fail("T20 CsvPercentRepresentable wrong");
        if (CsvCodeFromPercent(50.0, 10, CSV_RANGE_LEGAL) != 502 || CsvCodeFromPercent(100.0, 8, CSV_RANGE_FULL) != 255
            || CsvCodeFromPercent(109.47, 10, CSV_RANGE_EXTENDED) != 1023 || CsvCodeFromPercent(-7.3, 10, CSV_RANGE_EXTENDED) != 0)
            Fail("T20 CsvCodeFromPercent wrong");
        if (std::string(CsvRangeWord(CSV_RANGE_FULL)) != "FULL" || std::string(CsvRangeWord(CSV_RANGE_EXTENDED)) != "EXTENDED"
            || std::string(CsvRangeWord(CSV_RANGE_NONE)) != "" || CsvFormatHeaderLine(10, CSV_RANGE_LEGAL) != "# BITDEPTH 10, RANGE LEGAL\n")
            Fail("T20 CsvRangeWord / CsvFormatHeaderLine wrong");
        int b = 0, r = -5;
        if (!ParseCsvFormatDirective("# BITDEPTH 10, RANGE extended", b, r) || b != 10 || r != CSV_RANGE_EXTENDED)
            Fail("T20 ParseCsvFormatDirective basic (%d,%d)", b, r);
        b = 0; r = -5;
        if (!ParseCsvFormatDirective("bitdepth=8 range:legal", b, r) || b != 8 || r != CSV_RANGE_LEGAL)
            Fail("T20 ParseCsvFormatDirective separators (%d,%d)", b, r);
        b = 0; r = -5;
        if (ParseCsvFormatDirective("# full sweep of illegal codes", b, r) || b != 0 || r != -5)
            Fail("T20 ParseCsvFormatDirective tripped on prose");
        if (ParseCsvFormatDirective("# BITDEPTH 12", b, r) || b != 0)
            Fail("T20 ParseCsvFormatDirective accepted 12 bits");

        PatchSequence e;
        e.SetExtended(true);
        e.AddPatch(109.47, 0, 0, "wtw");
        std::string out;
        if (e.WriteCsv(out, 8, CSV_RANGE_EXTENDED))
            Fail("T20 WriteCsv 8-bit EXTENDED accepted 109.47 (clamps to 255)");
        if (!e.WriteCsv(out, 10, CSV_RANGE_EXTENDED))
            Fail("T20 WriteCsv 10-bit EXTENDED refused 109.47: %s", e.LastError().c_str());
        PatchSequenceFile f;
        PatchSequenceHeader hx;
        hx.bits = 10; hx.range = CSV_RANGE_LEGAL;
        if (f.FormatHps(out, e, hx))
            Fail("T20 FormatHps LEGAL accepted an out-of-span component");
        hx.range = CSV_RANGE_EXTENDED; hx.bits = 8;
        if (f.FormatHps(out, e, hx))
            Fail("T20 FormatHps 8-bit EXTENDED accepted 109.47");
        hx.bits = 10;
        if (!f.FormatHps(out, e, hx))
            Fail("T20 FormatHps 10-bit EXTENDED refused 109.47: %s", f.LastError().c_str());

        // Writer refuses what would corrupt the file or flip the CSV-only reader.
        PatchSequence p;
        BuildPinned(p);
        PatchSequenceHeader hp;
        FillPinnedHeader(hp);
        std::string keep = "sentinel";
        hp.name = "Range extended near-white";
        if (f.FormatHps(keep, p, hp))
            Fail("T20 FormatHps accepted a NAME containing RANGE");
        hp.name = "bitdepth 10 set";
        if (f.FormatHps(keep, p, hp))
            Fail("T20 FormatHps accepted a NAME containing BITDEPTH");
        hp.name = "quote\"inside";
        if (f.FormatHps(keep, p, hp))
            Fail("T20 FormatHps accepted a NAME containing a quote");
        hp.name = "two\nlines";
        if (f.FormatHps(keep, p, hp))
            Fail("T20 FormatHps accepted a NAME containing LF");
        hp.name = "ok";
        hp.steps.push_back("AddRangeThing");
        if (f.FormatHps(keep, p, hp))
            Fail("T20 FormatHps accepted a STEP containing RANGE");
        hp.steps.pop_back();
        hp.loadModel = "wrgb\rpower";
        if (f.FormatHps(keep, p, hp))
            Fail("T20 FormatHps accepted a LOAD-MODEL containing CR");
        hp.loadModel = "luminance";
        PatchGroupSpec bad;
        bad.name = "near black";
        hp.groups.push_back(bad);
        if (f.FormatHps(keep, p, hp))
            Fail("T20 FormatHps accepted a group name with a space");
        hp.groups.back().name = "";
        if (f.FormatHps(keep, p, hp))
            Fail("T20 FormatHps accepted an empty group name");
        hp.groups.pop_back();
        hp.version = 2;
        if (f.FormatHps(keep, p, hp))
            Fail("T20 FormatHps accepted version 2");
        hp.version = 0;
        if (f.FormatHps(keep, p, hp))
            Fail("T20 FormatHps accepted version 0");
        hp.version = 1;
        if (keep != "sentinel")
            Fail("T20 a refused FormatHps touched the output");
        if (!f.FormatHps(keep, p, hp))
            Fail("T20 FormatHps refused the repaired header: %s", f.LastError().c_str());

        // Reader tolerance: BOM, leading blank line, leading whitespace.
        PatchSequence q;
        PatchSequenceHeader hq;
        std::string bom = std::string("\xEF\xBB\xBF") + kPinnedText;
        if (!f.ParseHps(bom, q, hq) || q.Count() != 5 || hq.version != 1)
            Fail("T20 ParseHps refused a BOM-prefixed file: %s", f.LastError().c_str());
        std::string blank = std::string("\n\n") + kPinnedText;
        if (!f.ParseHps(blank, q, hq) || q.Count() != 5)
            Fail("T20 ParseHps refused leading blank lines: %s", f.LastError().c_str());
        std::string indented = std::string("   ") + kPinnedText;
        if (!f.ParseHps(indented, q, hq) || q.Count() != 5)
            Fail("T20 ParseHps refused an indented first line: %s", f.LastError().c_str());
        std::string crlf;
        for (const char* pc = kPinnedText; *pc; ++pc)
        {
            if (*pc == '\n') crlf += '\r';
            crlf += *pc;
        }
        if (!f.ParseHps(crlf, q, hq) || q.Count() != 5)
            Fail("T20 ParseHps refused CRLF input: %s", f.LastError().c_str());
        remove(kTmpHps);
        if (!WriteTextFile(kTmpHps, bom))
            Fail("T20 cannot write the BOM fixture");
        else
        {
            PatchSequenceFile::Dialect d = PatchSequenceFile::DIALECT_CSV_LEGACY;
            if (!f.Read(kTmpHps, q, hq, &d) || d != PatchSequenceFile::DIALECT_HPS)
                Fail("T20 Read of a BOM-prefixed .hps not HPS: %s", f.LastError().c_str());
            remove(kTmpHps);
        }

        // Codes truncate to int in both dialects; the row number is named on refusal.
        std::string frac = kPinnedText;
        const std::string rowOld = "3,73,73,73,Gray 1,C,dark,0,,,10\n";
        frac.replace(frac.find(rowOld), rowOld.size(), "3,73.9,73.9,73.9,Gray 1,C,dark,0,,,10\n");
        if (!f.ParseHps(frac, q, hq))
            Fail("T20 ParseHps refused fractional codes: %s", f.LastError().c_str());
        else
        {
            q.GetPatch(3, rgb, 0);
            if (rgb[0] != CsvCodeToPercent(73, 10, CSV_RANGE_LEGAL))
                Fail("T20 fractional code not truncated (%.17g)", rgb[0]);
        }
        std::string mism = std::string(kPinnedText) + "3,74,73,73,Gray 1 wrong,C,,,,,\n";
        if (f.ParseHps(mism, q, hq))
            Fail("T20 ParseHps accepted a mismatched repeat");
        else if (f.LastError().find("row 6") == std::string::npos)
            Fail("T20 refusal does not name the row: '%s'", f.LastError().c_str());

        // CSV import: cap refused; unusable patch numbers fall back to row indices.
        std::string big = "# BITDEPTH 8, RANGE LEGAL\n";
        char row[48];
        for (int i = 0; i < 10001; ++i)
        {
            snprintf(row, sizeof(row), "%d,%d,16,16\n", i, 16 + (i % 220));
            big += row;
        }
        WriteTextFile(kTmpCsv, big);
        PatchSequenceFile::Dialect d;
        if (f.Read(kTmpCsv, q, hq, &d))
            Fail("T20 CSV import accepted 10001 rows");
        WriteTextFile(kTmpCsv, "# BITDEPTH 8, RANGE LEGAL\n7,16,16,16,Black\n7,235,235,235,White\n3,126,126,126\n");
        if (!f.Read(kTmpCsv, q, hq, &d))
            Fail("T20 CSV import refused duplicated patch numbers: %s", f.LastError().c_str());
        else if (q.Count() != 3 || q.GetIdentity(0) != 0 || q.GetIdentity(1) != 1 || q.GetIdentity(2) != 2 || !hq.identitiesFromRows)
            Fail("T20 CSV duplicate patch numbers did not fall back to row indices");
        WriteTextFile(kTmpCsv, "# BITDEPTH 8, RANGE LEGAL\nP1,16,16,16,Black\nP2,235,235,235,White\n");
        if (!f.Read(kTmpCsv, q, hq, &d))
            Fail("T20 CSV import refused non-numeric patch numbers: %s", f.LastError().c_str());
        else if (q.Count() != 2 || q.GetIdentity(1) != 1 || !hq.identitiesFromRows)
            Fail("T20 CSV non-numeric patch numbers did not fall back to row indices");
        WriteTextFile(kTmpCsv, "# BITDEPTH 8, RANGE LEGAL\n5,16,16,16,Black\n9,235,235,235,White\n");
        if (!f.Read(kTmpCsv, q, hq, &d) || q.GetIdentity(0) != 5 || q.GetIdentity(1) != 9 || hq.identitiesFromRows)
            Fail("T20 CSV usable patch numbers were not kept as identities");
        remove(kTmpCsv);
    }
}

//////////////////////////////////////////////////////////////////////////
// T21 — ordering stage oracle (pure assertion, no golden file). Pins the
// load models and their directive text, the dwell estimate, the window
// objective, and Order(): multiset preservation, the anchor/preroll
// layout, hand-placed interstitials keeping their predecessor, the
// max-step constraint, dwell weighting changing the result, the bound
// (ordered J strictly below generation-order J), determinism, refusals,
// and the drift score. Load values are pinned by closed form; the bound
// is relative, so no reference implementation is needed.
//////////////////////////////////////////////////////////////////////////

namespace T21Order
{
    static bool Near(double a, double b, double tol)
    {
        return fabs(a - b) <= tol;
    }

    // A body that needs ordering: three neutral ramps of 11 (0..100), the
    // 3-lattice shell, and the six 100% primaries/secondaries. 33+26+6.
    static void BuildBody(PatchSequence& s)
    {
        s.AddGrayRamp(11, 0.0, 100.0);
        s.AddGrayRamp(11, 0.0, 100.0);
        s.AddGrayRamp(11, 0.0, 100.0);
        s.AddCubeShell(3);
        s.AddPatch(100, 0, 0, "Red");
        s.AddPatch(0, 100, 0, "Green");
        s.AddPatch(0, 0, 100, "Blue");
        s.AddPatch(0, 100, 100, "Cyan");
        s.AddPatch(100, 0, 100, "Magenta");
        s.AddPatch(100, 100, 0, "Yellow");
    }

    // Sorted (identity, r, g, b, role, name) tuples of the body rows.
    static void BodyKeys(const PatchSequence& s, std::vector<std::string>& keys)
    {
        keys.clear();
        for (int i = 0; i < s.Count(); ++i)
        {
            char r = s.GetRole(i);
            if (r == 'A' || r == 'P')
                continue;
            double rgb[3];
            std::string nm, g;
            PatchTiming t;
            s.GetPatch(i, rgb, &nm);
            s.GetGroup(i, g);
            s.GetTiming(i, t);
            char buf[256];
            snprintf(buf, sizeof(buf), "%d|%.17g|%.17g|%.17g|%c|%s|%s|%g|%g|%g|%d",
                     s.GetIdentity(i), rgb[0], rgb[1], rgb[2], r, nm.c_str(), g.c_str(),
                     t.settle, t.hold, t.integration, t.average);
            keys.push_back(buf);
        }
        std::sort(keys.begin(), keys.end());
    }

    static bool SameOrder(const PatchSequence& a, const PatchSequence& b)
    {
        if (a.Count() != b.Count())
            return false;
        for (int i = 0; i < a.Count(); ++i)
        {
            double ra[3], rb[3];
            a.GetPatch(i, ra, 0);
            b.GetPatch(i, rb, 0);
            if (a.GetIdentity(i) != b.GetIdentity(i) || a.GetRole(i) != b.GetRole(i)
                || ra[0] != rb[0] || ra[1] != rb[1] || ra[2] != rb[2])
                return false;
        }
        return true;
    }
}

static void RunT21()
{
    printf("T21 ordering stage oracle...\n");
    using namespace T21Order;

    // (1) Load models: closed-form values, directive text round trip.
    {
        LoadModel m;
        if (m.kind != LoadModel::LOAD_LUMINANCE || m.gamma != 2.2 || m.kw != 1.0 || m.kr != 1.1 || m.kg != 1.0 || m.kb != 1.35)
            Fail("T21 LoadModel defaults wrong");
        const double white[3] = { 100, 100, 100 };
        const double half[3] = { 50, 50, 50 };
        const double red[3] = { 100, 0, 0 };
        const double over[3] = { 109.4, 0, 0 };
        if (!Near(m.Load(white), 1.0, 1e-12))
            Fail("T21 luminance white = %.17g want 1", m.Load(white));
        if (!Near(m.Load(half), pow(0.5, 2.2), 1e-12))
            Fail("T21 luminance 50%% = %.17g want %.17g", m.Load(half), pow(0.5, 2.2));
        if (!Near(m.Load(red), 0.2126, 1e-12))
            Fail("T21 luminance red = %.17g want 0.2126", m.Load(red));
        if (!Near(m.Load(over), 0.2126, 1e-12))
            Fail("T21 luminance clamps super-white before decoding (%.17g)", m.Load(over));
        m.kind = LoadModel::LOAD_WRGB_POWER;
        if (!Near(m.Load(white), 1.0, 1e-12))
            Fail("T21 wrgb white = %.17g want kw = 1", m.Load(white));
        if (!Near(m.Load(red), 1.1, 1e-12))
            Fail("T21 wrgb red = %.17g want kr = 1.1", m.Load(red));
        const double mix[3] = { 100, 50, 0 };     // w = 0, r = 1, g = 0.5^2.2
        if (!Near(m.Load(mix), 1.1 + 1.0 * pow(0.5, 2.2), 1e-12))
            Fail("T21 wrgb mix = %.17g", m.Load(mix));
        if (m.Format() != "wrgb-power kw=1 kr=1.1 kg=1 kb=1.35 gamma=2.2")
            Fail("T21 wrgb Format '%s'", m.Format().c_str());
        LoadModel lum;
        if (lum.Format() != "luminance gamma=2.2")
            Fail("T21 luminance Format '%s'", lum.Format().c_str());
        LoadModel p;
        if (!LoadModel::Parse("wrgb-power kw=1.00 kr=1.10 kg=1.00 kb=1.35 gamma=2.2", p)
            || p.kind != LoadModel::LOAD_WRGB_POWER || p.kw != 1.0 || p.kr != 1.1 || p.kb != 1.35 || p.gamma != 2.2)
            Fail("T21 Parse wrgb-power failed");
        if (!LoadModel::Parse("luminance", p) || p.kind != LoadModel::LOAD_LUMINANCE || p.gamma != 2.2)
            Fail("T21 Parse bare luminance failed");
        if (!LoadModel::Parse("luminance gamma=2.4", p) || p.gamma != 2.4)
            Fail("T21 Parse luminance gamma failed");
        if (!LoadModel::Parse("wrgb-power gamma=2 kb=2", p) || p.kb != 2.0 || p.gamma != 2.0 || p.kw != 1.0)
            Fail("T21 Parse tokens in any order failed");
        LoadModel keep = p;
        if (LoadModel::Parse("qd-oled", p) || LoadModel::Parse("luminance gamma=0", p) || LoadModel::Parse("luminance foo=1", p)
            || LoadModel::Parse("wrgb-power kr=-1", p) || LoadModel::Parse("", p))
            Fail("T21 Parse accepted a bad model");
        if (p.kind != keep.kind || p.kb != keep.kb || p.gamma != keep.gamma)
            Fail("T21 refused Parse changed the model");
        LoadModel rt;
        LoadModel::Parse(m.Format(), rt);
        if (rt.Format() != m.Format())
            Fail("T21 Format/Parse round trip differs");
    }

    // (2) RowLoad and Dwell.
    {
        PatchSequence s;
        s.AddPatch(100, 100, 100, "White");            // 0
        s.AddPatch(0, 0, 0, "Black");                  // 1
        s.AddRow(2, 0, 0, 0, "Background", 'I');       // 2, hold 8
        s.AddPatch(50, 50, 50, "Gray 50");             // 3, group dark, settle override 0
        PatchTiming t;
        t.hold = 8.0;
        s.SetTiming(2, t);
        s.SetGroup(3, "dark");
        PatchTiming d;
        d.settle = 0.0;
        s.SetTiming(3, d);
        OrderingParams p;
        p.area = 0.1;
        p.backgroundLoad = 0.0;
        p.latency = 0.4;
        p.defaults.settle = 0.3;
        p.defaults.integration = 0.5;
        p.defaults.average = 1;
        PatchGroupSpec dark;
        dark.name = "dark";
        dark.timing.integration = 2.0;
        dark.timing.average = 5;
        dark.timing.settle = 1.5;
        p.groups.push_back(dark);
        if (!Near(PatchOrdering::RowLoad(s, 0, p), 0.1, 1e-12))
            Fail("T21 RowLoad white = %.17g want 0.1", PatchOrdering::RowLoad(s, 0, p));
        if (!Near(PatchOrdering::RowLoad(s, 1, p), 0.0, 1e-12))
            Fail("T21 RowLoad black = %.17g want 0", PatchOrdering::RowLoad(s, 1, p));
        p.backgroundLoad = 0.5;
        if (!Near(PatchOrdering::RowLoad(s, 1, p), 0.45, 1e-12))
            Fail("T21 RowLoad with background = %.17g want 0.45", PatchOrdering::RowLoad(s, 1, p));
        p.backgroundLoad = 0.0;
        if (PatchOrdering::RowLoad(s, 9, p) != -1.0 || PatchOrdering::Dwell(s, -1, p) != -1.0)
            Fail("T21 RowLoad/Dwell bad index not -1");
        // white: defaults 0.3 + 0.5*1 + 0 + 0.4 = 1.2
        if (!Near(PatchOrdering::Dwell(s, 0, p), 1.2, 1e-12))
            Fail("T21 Dwell white = %.17g want 1.2", PatchOrdering::Dwell(s, 0, p));
        // black after white, no settle-after-step: 1.2
        if (!Near(PatchOrdering::Dwell(s, 1, p), 1.2, 1e-12))
            Fail("T21 Dwell black = %.17g want 1.2", PatchOrdering::Dwell(s, 1, p));
        p.settleAfterStep = 2.5;              // drop 0.1 -> 0.0 adds 0.25
        if (!Near(PatchOrdering::Dwell(s, 1, p), 1.45, 1e-12))
            Fail("T21 Dwell black with settle-after-step = %.17g want 1.45", PatchOrdering::Dwell(s, 1, p));
        if (!Near(PatchOrdering::Dwell(s, 0, p), 1.2, 1e-12))
            Fail("T21 Dwell of the first row has no predecessor term");
        // interstitial: hold 8 + latency 0.4
        if (!Near(PatchOrdering::Dwell(s, 2, p), 8.4, 1e-12))
            Fail("T21 Dwell interstitial = %.17g want 8.4", PatchOrdering::Dwell(s, 2, p));
        // gray 50 in group dark with settle override 0: 0 + 2*5 + 0 + 0.4 = 10.4 (load rises, no step term)
        if (!Near(PatchOrdering::Dwell(s, 3, p), 10.4, 1e-12))
            Fail("T21 Dwell dark row = %.17g want 10.4", PatchOrdering::Dwell(s, 3, p));
        p.defaults = PatchTiming();           // all -1 count as 0: white = 0 + 0*1 + 0 + 0.4
        if (!Near(PatchOrdering::Dwell(s, 0, p), 0.4, 1e-12))
            Fail("T21 Dwell with unset defaults = %.17g want 0.4", PatchOrdering::Dwell(s, 0, p));
    }

    // (3) WindowExcursion on a hand-made sequence.
    {
        PatchSequence s;
        s.AddPatch(100, 100, 100, "W");      // L 0.1
        s.AddPatch(0, 0, 0, "K");            // L 0
        s.AddPatch(100, 100, 100, "W");
        s.AddPatch(0, 0, 0, "K");
        OrderingParams p;
        p.latency = 1.0;                     // every dwell = 1
        p.windowSeconds = 1.0;               // windows of one row: J = |L - Lbar| max = 0.05
        if (!Near(PatchOrdering::WindowExcursion(s, p), 0.05, 1e-12))
            Fail("T21 J single-row windows = %.17g want 0.05", PatchOrdering::WindowExcursion(s, p));
        p.windowSeconds = 2.0;               // pairs: every window mean = 0.05 = Lbar -> 0; last window is one row: 0.05
        if (!Near(PatchOrdering::WindowExcursion(s, p), 0.05, 1e-12))
            Fail("T21 J two-row windows = %.17g want 0.05 (the tail window)", PatchOrdering::WindowExcursion(s, p));
        s.AddRow(0, 100, 100, 100, "W", 'A');   // anchors are not counted
        s.AddRow(0, 100, 100, 100, "W", 'A');
        if (!Near(PatchOrdering::WindowExcursion(s, p), 0.05, 1e-12))
            Fail("T21 anchors changed J (%.17g)", PatchOrdering::WindowExcursion(s, p));
        p.windowSeconds = 0.0;
        if (PatchOrdering::WindowExcursion(s, p) != -1.0)
            Fail("T21 J with W <= 0 not -1");
        PatchSequence a;
        a.AddRow(0, 50, 50, 50, "g", 'A');
        p.windowSeconds = 1.0;
        if (PatchOrdering::WindowExcursion(a, p) != -1.0)
            Fail("T21 J with nothing counted not -1");
    }

    // (4) Order: multiset, layout, bound, determinism, interstitial, max-step, weighting, refusals.
    {
        PatchSequence body;
        BuildBody(body);
        const int n = body.Count();                  // 65
        double rgb[3];
        std::string nm;
        OrderingParams p;
        p.windowSeconds = 6.0;
        p.latency = 1.0;                             // uniform dwell 1 s -> windows of 6 rows
        PatchAnchorSpec a;
        a.id = 5;                                    // Gray 50 of the first ramp
        a.every = 10;
        p.anchors.push_back(a);
        p.prerollId = 0;                             // Gray 0
        p.prerollCount = 2;
        PatchSequence out;
        std::string reason;
        if (!PatchOrdering::Order(body, p, out, reason))
            Fail("T21 Order refused: %s", reason.c_str());
        else
        {
            // Layout: 2 preroll, anchor, body with an anchor after every 10 body rows, anchor at the tail.
            int expected = 2 + 1 + n + (n / 10) + 1;
            if (out.Count() != expected)
                Fail("T21 Order count %d want %d", out.Count(), expected);
            if (out.GetRole(0) != 'P' || out.GetRole(1) != 'P' || out.GetIdentity(0) != 0 || out.GetRole(2) != 'A' || out.GetIdentity(2) != 5)
                Fail("T21 Order head layout wrong (%c %c %c)", out.GetRole(0), out.GetRole(1), out.GetRole(2));
            if (out.GetRole(out.Count() - 1) != 'A' || out.GetIdentity(out.Count() - 1) != 5)
                Fail("T21 Order tail anchor missing");
            out.GetPatch(2, rgb, &nm);
            if (rgb[0] != 50.0 || nm != "Gray 50")
                Fail("T21 anchor row does not repeat its definition");
            // Every anchor sits after a multiple of 10 body rows.
            int bodySeen = 0, anchorsSeen = 0;
            bool layoutOk = true;
            for (int i = 3; i < out.Count(); ++i)
            {
                char r = out.GetRole(i);
                if (r == 'A')
                {
                    ++anchorsSeen;
                    if (i != out.Count() - 1 && bodySeen % 10 != 0)
                        layoutOk = false;
                }
                else if (r == 'P')
                    layoutOk = false;
                else
                    ++bodySeen;
            }
            if (!layoutOk || bodySeen != n || anchorsSeen != n / 10 + 1)
                Fail("T21 anchor placement wrong (body %d anchors %d)", bodySeen, anchorsSeen);
            // Multiset.
            std::vector<std::string> kin, kout;
            BodyKeys(body, kin);
            BodyKeys(out, kout);
            if (kin != kout)
                Fail("T21 Order changed the body multiset");
            // Bound: strictly better than generation order, and not worse than half of it.
            double jGen = PatchOrdering::WindowExcursion(body, p);
            double jOut = PatchOrdering::WindowExcursion(out, p);
            if (!(jOut < jGen) || !(jOut <= 0.5 * jGen))
                Fail("T21 ordered J %.6g not below half of generation-order J %.6g", jOut, jGen);
            // Determinism.
            PatchSequence again;
            if (!PatchOrdering::Order(body, p, again, reason) || !SameOrder(out, again))
                Fail("T21 Order is not deterministic");
            // A different input order of the same rows gives the same result
            // (the sort is total: L*t then identity).
            PatchSequence shuffled;
            for (int i = n - 1; i >= 0; --i)
            {
                body.GetPatch(i, rgb, &nm);
                shuffled.AddRow(body.GetIdentity(i), rgb[0], rgb[1], rgb[2], nm, body.GetRole(i));
            }
            PatchSequence fromShuffled;
            if (!PatchOrdering::Order(shuffled, p, fromShuffled, reason) || !SameOrder(out, fromShuffled))
                Fail("T21 Order depends on the input order");
        }

        // Interstitial keeps its predecessor.
        PatchSequence withI;
        BuildBody(withI);
        withI.AddRow(200, 0, 0, 0, "Cooler", 'I');    // predecessor = Yellow (the last body row)
        PatchTiming th;
        th.hold = 5.0;
        withI.SetTiming(withI.Count() - 1, th);
        PatchSequence outI;
        if (!PatchOrdering::Order(withI, p, outI, reason))
            Fail("T21 Order with an interstitial refused: %s", reason.c_str());
        else
        {
            int at = -1;
            for (int i = 0; i < outI.Count(); ++i)
                if (outI.GetRole(i) == 'I')
                    at = i;
            std::string prev;
            double prgb[3];
            if (at >= 1)
                outI.GetPatch(at - 1, prgb, &prev);
            if (at < 1 || prev != "Yellow")
                Fail("T21 interstitial did not follow its predecessor (at %d, prev '%s')", at, prev.c_str());
            PatchTiming got;
            outI.GetTiming(at, got);
            if (got.hold != 5.0)
                Fail("T21 interstitial lost its timing");
        }

        // Max-step: satisfied on every adjacent body pair; impossible -> refused.
        OrderingParams ps = p;
        ps.maxStep = 0.03;
        PatchSequence outS;
        if (!PatchOrdering::Order(body, ps, outS, reason))
            Fail("T21 Order with max-step 0.03 refused: %s", reason.c_str());
        else
        {
            double prevL = -1.0;
            bool ok = true;
            for (int i = 0; i < outS.Count(); ++i)
            {
                char r = outS.GetRole(i);
                if (r == 'A' || r == 'P')
                    continue;
                double L = PatchOrdering::RowLoad(outS, i, ps);
                if (prevL >= 0.0 && fabs(L - prevL) > 0.03 + 1e-12)
                    ok = false;
                prevL = L;
            }
            if (!ok)
                Fail("T21 max-step violated");
        }
        PatchSequence twoTone;
        twoTone.AddPatch(100, 100, 100, "W");
        twoTone.AddPatch(0, 0, 0, "K");
        twoTone.AddPatch(100, 100, 100, "W2");
        OrderingParams pt;
        pt.maxStep = 0.01;                          // 0.1 vs 0 can never be adjacent within 0.01
        std::string sentinel = "untouched";
        PatchSequence outT;
        outT.AddPatch(1, 1, 1, "sentinel");
        if (PatchOrdering::Order(twoTone, pt, outT, reason))
            Fail("T21 Order accepted an impossible max-step");
        else if (reason != "max-step cannot be satisfied" || outT.Count() != 1)
            Fail("T21 impossible max-step: reason '%s', out count %d", reason.c_str(), outT.Count());

        // Dwell weighting changes the order: dark rows dwelling 30x.
        PatchSequence heavy;
        BuildBody(heavy);
        OrderingParams pw = p;
        PatchGroupSpec dark;
        dark.name = "dark";
        dark.timing.integration = 30.0;
        pw.groups.push_back(dark);
        for (int i = 0; i < heavy.Count(); ++i)
        {
            heavy.GetPatch(i, rgb, &nm);
            if (rgb[0] <= 10.0 && rgb[1] <= 10.0 && rgb[2] <= 10.0)
                heavy.SetGroup(i, "dark");
        }
        PatchSequence outW, outU;
        if (!PatchOrdering::Order(heavy, pw, outW, reason) || !PatchOrdering::Order(heavy, p, outU, reason))
            Fail("T21 weighted / unweighted Order refused: %s", reason.c_str());
        else
        {
            if (SameOrder(outW, outU))
                Fail("T21 dwell weighting did not change the order");
            double jW = PatchOrdering::WindowExcursion(outW, pw);
            double jU = PatchOrdering::WindowExcursion(outU, pw);   // the unweighted order judged under real dwell
            if (!(jW <= jU))
                Fail("T21 weighted order J %.6g worse than unweighted order's %.6g under real dwell", jW, jU);
        }

        // Refusals leave the output untouched.
        PatchSequence keep;
        keep.AddPatch(1, 1, 1, "sentinel");
        PatchSequence empty;
        if (PatchOrdering::Order(empty, p, keep, reason))
            Fail("T21 Order accepted an empty input");
        PatchSequence hasAnchor;
        BuildBody(hasAnchor);
        hasAnchor.AddRow(5, 50, 50, 50, "Gray 50", 'A');
        if (PatchOrdering::Order(hasAnchor, p, keep, reason))
            Fail("T21 Order accepted an input with an anchor row");
        OrderingParams bad = p;
        bad.anchors[0].id = 999;
        if (PatchOrdering::Order(body, bad, keep, reason))
            Fail("T21 Order accepted an unknown anchor id");
        bad = p;
        bad.anchors[0].every = 0;
        if (PatchOrdering::Order(body, bad, keep, reason))
            Fail("T21 Order accepted every = 0");
        bad = p;
        bad.windowSeconds = 0.0;
        if (PatchOrdering::Order(body, bad, keep, reason))
            Fail("T21 Order accepted W = 0");
        bad = p;
        bad.area = 1.5;
        if (PatchOrdering::Order(body, bad, keep, reason))
            Fail("T21 Order accepted area > 1");
        bad = p;
        bad.prerollCount = -1;
        if (PatchOrdering::Order(body, bad, keep, reason))
            Fail("T21 Order accepted a negative preroll count");
        bad = p;
        bad.prerollId = 999;
        if (PatchOrdering::Order(body, bad, keep, reason))
            Fail("T21 Order accepted an unknown preroll id");
        if (keep.Count() != 1)
            Fail("T21 a refused Order touched the output");
        // No anchors, no preroll: the body alone.
        OrderingParams plain;
        plain.latency = 1.0;
        plain.windowSeconds = 6.0;
        PatchSequence outPlain;
        if (!PatchOrdering::Order(body, plain, outPlain, reason) || outPlain.Count() != n)
            Fail("T21 Order without anchors changed the count");
    }

    // (5) Drift score.
    {
        std::vector<double> y;
        y.push_back(100.0); y.push_back(101.0); y.push_back(99.0);
        if (!Near(PatchOrdering::DriftScore(y), 0.02, 1e-12))
            Fail("T21 DriftScore = %.17g want 0.02", PatchOrdering::DriftScore(y));
        y.resize(1);
        if (PatchOrdering::DriftScore(y) != -1.0)
            Fail("T21 DriftScore of one reading not -1");
        y.clear();
        y.push_back(0.0); y.push_back(0.0);
        if (PatchOrdering::DriftScore(y) != -1.0)
            Fail("T21 DriftScore with zero mean not -1");
        y.clear();
        y.push_back(5.0); y.push_back(5.0);
        if (PatchOrdering::DriftScore(y) != 0.0)
            Fail("T21 DriftScore of a flat trace not 0");
    }

    // (6) Review round: contract tightenings from /code-review high.
    {
        std::string reason;
        double rgb[3];
        std::string nm;

        // PatchTiming::Resolved and PatchSequence::DefinitionIndex.
        {
            PatchTiming row, grp, def;
            row.settle = 0.0;
            grp.settle = 1.5; grp.integration = 2.0; grp.average = 5;
            def.settle = 0.3; def.integration = 0.5; def.average = 1; def.hold = 0.0;
            PatchTiming r = PatchTiming::Resolved(row, grp, def);
            if (r.settle != 0.0 || r.integration != 2.0 || r.average != 5 || r.hold != 0.0)
                Fail("T21 Resolved precedence wrong (%g %g %d %g)", r.settle, r.integration, r.average, r.hold);
            PatchTiming none;
            r = PatchTiming::Resolved(none, none, none);
            if (r.settle != 0.0 || r.hold != 0.0 || r.integration != 0.0 || r.average != 0)
                Fail("T21 Resolved of all-unset is not zero");
            if (!PatchTiming::IsSet(1.0) || PatchTiming::IsSet(-1.0))
                Fail("T21 PatchTiming::IsSet wrong");
            PatchSequence s;
            s.AddRow(7, 1, 1, 1, "a", 'C');
            s.AddRow(3, 2, 2, 2, "b", 'C');
            s.AddRow(7, 1, 1, 1, "a rep", 'C');
            if (s.DefinitionIndex(7) != 0 || s.DefinitionIndex(3) != 1 || s.DefinitionIndex(9) != -1)
                Fail("T21 DefinitionIndex wrong (%d %d %d)", s.DefinitionIndex(7), s.DefinitionIndex(3), s.DefinitionIndex(9));
        }

        // Anchor and preroll rows inherit the definition's group and timing.
        {
            PatchSequence body;
            BuildBody(body);
            body.SetGroup(1, "dark");                 // Gray 10 of the first ramp, id 1
            PatchTiming t;
            t.average = 10;
            body.SetTiming(1, t);
            OrderingParams p;
            p.latency = 1.0; p.windowSeconds = 6.0;
            PatchGroupSpec dark;
            dark.name = "dark"; dark.timing.integration = 2.0;
            p.groups.push_back(dark);
            PatchAnchorSpec a;
            a.id = 1; a.every = 20;
            p.anchors.push_back(a);
            p.prerollId = 1; p.prerollCount = 1;
            PatchSequence out;
            if (!PatchOrdering::Order(body, p, out, reason))
                Fail("T21 inherit: Order refused: %s", reason.c_str());
            else
            {
                std::string g;
                PatchTiming got;
                out.GetGroup(0, g); out.GetTiming(0, got);
                if (out.GetRole(0) != 'P' || g != "dark" || got.average != 10)
                    Fail("T21 preroll row lost group/timing ('%s', avg %d)", g.c_str(), got.average);
                out.GetGroup(1, g); out.GetTiming(1, got);
                if (out.GetRole(1) != 'A' || g != "dark" || got.average != 10)
                    Fail("T21 anchor row lost group/timing ('%s', avg %d)", g.c_str(), got.average);
            }
        }

        // Anchor cadence counts characterization rows only; a block follows the chain.
        {
            PatchSequence s;
            s.AddPatch(10, 10, 10, "a");                       // identity 0
            s.AddRow(100, 0, 0, 0, "i1", 'I');
            s.AddRow(101, 0, 0, 0, "i2", 'I');
            s.AddPatch(20, 20, 20, "b");                       // identity 102 (fresh ids continue past 101)
            s.AddPatch(30, 30, 30, "c");                       // identity 103
            OrderingParams p;
            p.latency = 1.0; p.windowSeconds = 100.0;
            PatchAnchorSpec a;
            a.id = s.GetIdentity(4); a.every = 1;              // "c"
            p.anchors.push_back(a);
            PatchSequence out;
            if (!PatchOrdering::Order(s, p, out, reason))
                Fail("T21 cadence: Order refused: %s", reason.c_str());
            else
            {
                // head anchor, then for each of the 3 C rows: its chain then an anchor; the tail anchor
                // coincides with the last per-row anchor, so exactly one block ends the sequence.
                if (out.Count() != 1 + 5 + 3)
                    Fail("T21 cadence count %d want 9", out.Count());
                int cSeen = 0, aSeen = 0;
                bool ok = true;
                for (int i = 1; i < out.Count(); ++i)
                {
                    char r = out.GetRole(i);
                    if (r == 'A') { ++aSeen; if (aSeen != cSeen) ok = false; }
                    else if (r == 'C') { if (aSeen != cSeen) ok = false; ++cSeen; }
                    else if (r == 'I') { if (aSeen != cSeen - 1) ok = false; }   // still inside a's chain
                }
                if (!ok || cSeen != 3 || aSeen != 3)
                    Fail("T21 cadence layout wrong (C %d A %d)", cSeen, aSeen);
                // the 'I' rows still follow "a" directly
                int at = -1;
                for (int i = 0; i < out.Count(); ++i)
                    if (out.GetRole(i) == 'C') { out.GetPatch(i, rgb, &nm); if (nm == "a") at = i; }
                std::string n1, n2;
                if (at < 0 || at + 2 >= out.Count()) Fail("T21 cadence: chain head not found");
                else
                {
                    out.GetPatch(at + 1, rgb, &n1); out.GetPatch(at + 2, rgb, &n2);
                    if (n1 != "i1" || n2 != "i2")
                        Fail("T21 cadence: interstitials split from their chain ('%s','%s')", n1.c_str(), n2.c_str());
                }
            }
        }

        // Max-step over chain boundaries only; intra-chain steps are the input's.
        {
            // A = [0.5 C, 0.0 I], B = [1.0], C = [0.0]; maxStep 0.5 -> B, A, C satisfies it.
            PatchSequence s;
            OrderingParams p;
            p.area = 1.0; p.latency = 1.0; p.windowSeconds = 100.0; p.model.gamma = 1.0;
            s.AddPatch(50, 50, 50, "A");
            s.AddRow(100, 0, 0, 0, "A-cooler", 'I');
            s.AddPatch(100, 100, 100, "B");
            s.AddPatch(0, 0, 0, "C");
            p.maxStep = 0.5;
            PatchSequence out;
            if (!PatchOrdering::Order(s, p, out, reason))
                Fail("T21 chain max-step: refused although B,A,C satisfies it: %s", reason.c_str());
            else
            {
                double prevExit = -1.0; bool ok = true;
                for (int i = 0; i < out.Count(); ++i)
                {
                    char r = out.GetRole(i);
                    double L = PatchOrdering::RowLoad(out, i, p);
                    if (r == 'I') { prevExit = L; continue; }
                    if (prevExit >= 0.0 && fabs(L - prevExit) > 0.5 + 1e-12) ok = false;
                    prevExit = L;
                }
                if (!ok) Fail("T21 chain max-step violated across a chain boundary");
            }
            // An intra-chain step above the limit is refused up front with its own reason.
            p.maxStep = 0.3;
            PatchSequence keep;
            keep.AddPatch(1, 1, 1, "sentinel");
            if (PatchOrdering::Order(s, p, keep, reason))
                Fail("T21 intra-chain step above max-step accepted");
            else if (reason != "interstitial step exceeds max-step" || keep.Count() != 1)
                Fail("T21 intra-chain refusal: reason '%s' count %d", reason.c_str(), keep.Count());
        }

        // Leading interstitial refused; preroll count without an id refused; non-finite params refused.
        {
            PatchSequence lead;
            lead.AddRow(9, 0, 0, 0, "lead", 'I');
            lead.AddPatch(50, 50, 50, "x");
            OrderingParams p;
            p.latency = 1.0; p.windowSeconds = 6.0;
            PatchSequence keep;
            keep.AddPatch(1, 1, 1, "sentinel");
            if (PatchOrdering::Order(lead, p, keep, reason))
                Fail("T21 leading interstitial accepted");
            PatchSequence body;
            BuildBody(body);
            OrderingParams bad = p;
            bad.prerollCount = 2;                         // prerollId left at -1
            if (PatchOrdering::Order(body, bad, keep, reason))
                Fail("T21 preroll count without an id accepted");
            else if (reason.find("preroll") == std::string::npos)
                Fail("T21 preroll refusal reason does not name preroll: '%s'", reason.c_str());
            bad = p; bad.backgroundLoad = sqrt(-1.0);
            if (PatchOrdering::Order(body, bad, keep, reason)) Fail("T21 NaN backgroundLoad accepted");
            bad = p; bad.windowSeconds = sqrt(-1.0);
            if (PatchOrdering::Order(body, bad, keep, reason)) Fail("T21 NaN window accepted");
            bad = p; bad.model.gamma = sqrt(-1.0);
            if (PatchOrdering::Order(body, bad, keep, reason)) Fail("T21 NaN gamma accepted");
            bad = p; bad.model.kb = -1.0;
            if (PatchOrdering::Order(body, bad, keep, reason)) Fail("T21 negative factor accepted");
            bad = p; bad.defaults.settle = -0.5;
            if (PatchOrdering::Order(body, bad, keep, reason)) Fail("T21 negative default settle accepted");
            bad = p;
            PatchGroupSpec g; g.name = "dark"; g.timing.integration = sqrt(-1.0);
            bad.groups.push_back(g);
            if (PatchOrdering::Order(body, bad, keep, reason)) Fail("T21 NaN group timing accepted");
            bad = p; bad.settleAfterStep = sqrt(-1.0);
            if (PatchOrdering::Order(body, bad, keep, reason)) Fail("T21 NaN settle-after-step accepted");
            if (keep.Count() != 1) Fail("T21 a refused Order (review round) touched the output");
            LoadModel m;
            if (!m.Valid()) Fail("T21 default LoadModel not Valid");
            m.kr = sqrt(-1.0);
            if (m.Valid()) Fail("T21 LoadModel::Valid accepted NaN");
        }

        // Repeats: a total order - reversed input gives the same names per position.
        {
            PatchSequence body;
            BuildBody(body);
            body.AddRepeats(5);
            const int n = body.Count();
            PatchSequence rev;
            for (int i = n - 1; i >= 0; --i)
            {
                body.GetPatch(i, rgb, &nm);
                rev.AddRow(body.GetIdentity(i), rgb[0], rgb[1], rgb[2], nm, body.GetRole(i));
            }
            OrderingParams p;
            p.latency = 1.0; p.windowSeconds = 6.0;
            PatchSequence o1, o2;
            if (!PatchOrdering::Order(body, p, o1, reason) || !PatchOrdering::Order(rev, p, o2, reason))
                Fail("T21 repeats: Order refused: %s", reason.c_str());
            else
            {
                bool same = (o1.Count() == o2.Count());
                for (int i = 0; same && i < o1.Count(); ++i)
                {
                    std::string a, b;
                    o1.GetPatch(i, rgb, &a); o2.GetPatch(i, rgb, &b);
                    if (a != b || o1.GetIdentity(i) != o2.GetIdentity(i)) same = false;
                }
                if (!same) Fail("T21 repeats break input-order independence");
            }
        }

        // Predecessor rule: the row after an anchor charges settle-after-step against the previous counted row.
        {
            PatchSequence s;
            s.AddPatch(100, 100, 100, "W");          // load 0.1 (area 0.1)
            s.AddRow(5, 50, 50, 50, "anchor", 'A');  // not counted
            s.AddPatch(0, 0, 0, "K");                // drop 0.1 from W, not from the anchor
            OrderingParams p;
            p.latency = 0.4; p.settleAfterStep = 2.5;
            if (!Near(PatchOrdering::Dwell(s, 2, p), 0.4 + 0.25, 1e-12))
                Fail("T21 predecessor rule: dwell %.17g want 0.65", PatchOrdering::Dwell(s, 2, p));
        }

        // targetLoad replaces the set mean as Lbar.
        {
            PatchSequence s;
            s.AddPatch(100, 100, 100, "W");
            s.AddPatch(0, 0, 0, "K");
            OrderingParams p;
            p.latency = 1.0; p.windowSeconds = 1.0; p.targetLoad = 0.1;
            if (!Near(PatchOrdering::WindowExcursion(s, p), 0.1, 1e-12))
                Fail("T21 targetLoad J = %.17g want 0.1", PatchOrdering::WindowExcursion(s, p));
        }

        // Scale: a 3,735-row body orders within a wall-clock bound (guards against the cubic refine).
        {
            PatchSequence big;
            big.AddCubeGrid(13);                     // 2197
            big.AddCubeShell(17);                    // 1538
            OrderingParams p;
            p.latency = 1.0; p.windowSeconds = 40.0;
            PatchAnchorSpec a; a.id = 0; a.every = 100;
            p.anchors.push_back(a);
            clock_t t0 = clock();
            PatchSequence out;
            bool ok = PatchOrdering::Order(big, p, out, reason);
            double secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
            if (!ok)
                Fail("T21 scale: Order refused: %s", reason.c_str());
            else if (out.Count() < big.Count())
                Fail("T21 scale: rows lost");
            if (secs > 45.0)
                Fail("T21 scale: Order took %.1f s (bound 45 s)", secs);
            printf("    (T21 scale: %d rows ordered in %.1f s)\n", big.Count(), secs);
        }
    }
}

int main(int argc, char* argv[])
{
    setvbuf(stdout, NULL, _IONBF, 0);   // unbuffered, so a hung oracle still leaves its FAIL lines behind
    const char* mode = (argc > 1) ? argv[1] : "verify";
    g_goldenDir = (argc > 2) ? argv[2] : "golden";
    if (strcmp(mode, "gen") == 0)
        g_genMode = true;
    else if (strcmp(mode, "verify") != 0)
    {
        printf("usage: ColorMathTest [gen|verify] [goldenDir]\n");
        return 2;
    }
    if (g_genMode)
        _mkdir(g_goldenDir.c_str());

    RunT1();
    RunT1b();
    RunT2();
    RunT3();
    RunT4();
    RunT5();
    RunT6();
    RunT7();
    RunT8();
    RunT9();
    RunT10();
    RunT11();
    RunT12();
    RunT17();   // T13-T16, T18, T19 held by in-flight branches; see the header comment
    RunT20();
    RunT21();

    if (g_failures)
    {
        printf("\n%d FAILURE(S)\n", g_failures);
        return 1;
    }
    printf("\nALL TESTS PASSED (%s mode)\n", g_genMode ? "gen" : "verify");
    return 0;
}
