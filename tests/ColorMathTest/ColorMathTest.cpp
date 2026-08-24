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
// T10 asserts gamut-basis consistency, and T15 asserts the CubeLUT .cube format contract
// and its tetrahedral evaluator.
// T11-T14 are deliberately skipped here: they are claimed by in-flight branches (PR #178's
// BT.2390 oracles = T11/T12, the display-model series = T13, csv-provenance's CSV oracle
// renumbers to T14 on rebase) - check open branches before assigning any new T number.
// The .chc round-trip originally planned for the T7 slot needs app-level linkage and is
// deliberately still not in this console harness.

#include <afx.h>
#include "../../libHCFR/Color.h"
#include "../../libHCFR/CubeLUT.h"

#include <cstdio>
#include <cstdarg>
#include <cmath>
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
//////////////////////////////////////////////////////////////////////////
// T15 — CubeLUT .cube format oracle (pure assertion, no golden file).
// Pins the Adobe Cube 1.0 contract stated in CubeLUT.h: identity creation,
// exact write/read round-trip (%.17g), red-fastest data ordering verified
// against the emitted text, a hand-authored fixture with CRLF/comments/
// blank lines, strict rejection of malformed input with bit-identical
// keep-state, locale independence of both writer and reader, and the
// file-path I/O leg.
//////////////////////////////////////////////////////////////////////////

#include <clocale>

namespace T15Cube
{
    // 32-bit LCG (Numerical Recipes constants), deterministic everywhere.
    struct Lcg
    {
        unsigned int s;
        explicit Lcg(unsigned int seed) : s(seed) {}
        double Next01()
        {
            s = 1664525u * s + 1013904223u;
            return s / 4294967296.0;
        }
    };

    // Fills a size^3 lattice with deterministic values spanning negatives
    // and values above 1 (both legal in .cube).
    static void FillLattice(CubeLUT& lut, unsigned int seed)
    {
        Lcg g(seed);
        int n = lut.Size();
        for (int b = 0; b < n; ++b)
            for (int gg = 0; gg < n; ++gg)
                for (int r = 0; r < n; ++r)
                {
                    double v[3] = { g.Next01() * 4.0 - 1.0,
                                    g.Next01() * 4.0 - 1.0,
                                    g.Next01() * 4.0 - 1.0 };
                    if (!lut.SetEntry(r, gg, b, v))
                        Fail("T15 SetEntry(%d,%d,%d) rejected a finite value", r, gg, b);
                }
    }

    static bool SameLattice(const CubeLUT& a, const CubeLUT& b)
    {
        if (a.Size() != b.Size())
            return false;
        int n = a.Size();
        for (int bb = 0; bb < n; ++bb)
            for (int gg = 0; gg < n; ++gg)
                for (int r = 0; r < n; ++r)
                {
                    double va[3], vb[3];
                    if (!a.GetEntry(r, gg, bb, va) || !b.GetEntry(r, gg, bb, vb))
                        return false;
                    for (int k = 0; k < 3; ++k)
                        if (va[k] != vb[k])
                            return false;
                }
        return true;
    }
}

static void RunT15()
{
    printf("T15 CubeLUT .cube format oracle...\n");
    using namespace T15Cube;

    // Identity creation: default domain, exact node values.
    {
        CubeLUT lut;
        if (!lut.Create(3) || !lut.IsValid() || lut.Size() != 3)
            Fail("T15 Create(3) failed");
        else
        {
            for (int b = 0; b < 3; ++b)
                for (int g = 0; g < 3; ++g)
                    for (int r = 0; r < 3; ++r)
                    {
                        double v[3];
                        if (!lut.GetEntry(r, g, b, v))
                            { Fail("T15 GetEntry(%d,%d,%d) failed", r, g, b); continue; }
                        double e[3] = { r / 2.0, g / 2.0, b / 2.0 };
                        for (int k = 0; k < 3; ++k)
                            if (v[k] != e[k])
                                Fail("T15 identity entry (%d,%d,%d) comp %d = %.17g expected %.17g",
                                     r, g, b, k, v[k], e[k]);
                    }
            double dmin[3], dmax[3];
            lut.GetDomain(dmin, dmax);
            for (int k = 0; k < 3; ++k)
                if (dmin[k] != 0.0 || dmax[k] != 1.0)
                    Fail("T15 Create left a non-default domain (comp %d)", k);
            if (!lut.Title().empty())
                Fail("T15 Create left a non-empty title");
        }
        if (lut.Create(1))
            Fail("T15 Create accepted size 1");
        if (lut.Create(257))
            Fail("T15 Create accepted size 257");
        if (lut.Size() != 3)
            Fail("T15 failed Create changed the lattice (size %d)", lut.Size());
    }

    // Exact round-trip through the string form, with title and custom domain.
    {
        CubeLUT lut;
        if (!lut.Create(5))
            Fail("T15 Create(5) failed");
        else
        {
            FillLattice(lut, 24680u);
            if (!lut.SetTitle("HCFR round-trip fixture"))
                Fail("T15 SetTitle rejected a plain title");
            double dmin[3] = { -0.125, 0.0, -1.0 };
            double dmax[3] = { 1.25, 1.0, 2.0 };
            if (!lut.SetDomain(dmin, dmax))
                Fail("T15 SetDomain rejected a valid domain");

            std::string text;
            if (!lut.WriteToString(text) || text.empty())
                Fail("T15 WriteToString failed on a valid lattice");
            else
            {
                // Deterministic writer.
                std::string text2;
                if (!lut.WriteToString(text2) || text2 != text)
                    Fail("T15 writer is not deterministic");

                CubeLUT back;
                if (!back.ReadFromString(text))
                    Fail("T15 ReadFromString rejected our own output: %s",
                         back.LastError().c_str());
                else
                {
                    if (back.Title() != lut.Title())
                        Fail("T15 title did not round-trip");
                    double rmin[3], rmax[3];
                    back.GetDomain(rmin, rmax);
                    for (int k = 0; k < 3; ++k)
                        if (rmin[k] != dmin[k] || rmax[k] != dmax[k])
                            Fail("T15 domain did not round-trip (comp %d)", k);
                    if (!SameLattice(lut, back))
                        Fail("T15 lattice did not round-trip exactly");
                }
            }
        }
    }

    // Data ordering pinned against the emitted text: red varies fastest.
    {
        CubeLUT lut;
        if (!lut.Create(2))
            Fail("T15 Create(2) failed");
        else
        {
            for (int b = 0; b < 2; ++b)
                for (int g = 0; g < 2; ++g)
                    for (int r = 0; r < 2; ++r)
                    {
                        double v[3] = { r + 10.0 * g + 100.0 * b, 0.5, -0.5 };
                        lut.SetEntry(r, g, b, v);
                    }
            std::string text;
            if (!lut.WriteToString(text))
                Fail("T15 ordering write failed");
            else
            {
                // Collect data lines: lines whose first non-space char is a
                // digit, '-', or '.'.
                std::vector<std::string> data;
                size_t pos = 0;
                while (pos <= text.size())
                {
                    size_t nl = text.find('\n', pos);
                    std::string line = text.substr(pos, (nl == std::string::npos ?
                                                         text.size() : nl) - pos);
                    pos = (nl == std::string::npos) ? text.size() + 1 : nl + 1;
                    size_t i = line.find_first_not_of(" \t\r");
                    if (i == std::string::npos)
                        continue;
                    char c = line[i];
                    if ((c >= '0' && c <= '9') || c == '-' || c == '.')
                        data.push_back(line);
                }
                if (data.size() != 8)
                    Fail("T15 expected 8 data lines, found %d", (int)data.size());
                else
                    for (int d = 0; d < 8; ++d)
                    {
                        double expect = (d % 2) + 10.0 * ((d / 2) % 2) + 100.0 * (d / 4);
                        double got = atof(data[(size_t)d].c_str());
                        if (got != expect)
                            Fail("T15 data line %d starts with %.17g expected %.17g "
                                 "(red must vary fastest)", d, got, expect);
                    }
            }
        }
    }

    // Hand-authored fixture: CRLF, comments, blank lines, keywords after
    // one another in a different order than we write them.
    {
        const char* fixture =
            "# hand-authored fixture\r\n"
            "\r\n"
            "LUT_3D_SIZE 2\r\n"
            "TITLE \"fixture with spaces\"\r\n"
            "# domain comes after the size here\r\n"
            "DOMAIN_MIN 0 0 0\r\n"
            "DOMAIN_MAX 1 1 2\r\n"
            "\r\n"
            "0 0 0\r\n"
            "0.25 0 0\r\n"
            "0 0.5 0\r\n"
            "0.25 0.5 0\r\n"
            "0 0 1.5\r\n"
            "0.25 0 1.5\r\n"
            "0 0.5 1.5\r\n"
            "0.25 0.5 1.5\r\n";
        CubeLUT lut;
        if (!lut.ReadFromString(fixture))
            Fail("T15 fixture rejected: %s", lut.LastError().c_str());
        else
        {
            if (lut.Size() != 2)
                Fail("T15 fixture size %d expected 2", lut.Size());
            if (lut.Title() != "fixture with spaces")
                Fail("T15 fixture title '%s'", lut.Title().c_str());
            double dmin[3], dmax[3];
            lut.GetDomain(dmin, dmax);
            if (dmax[2] != 2.0)
                Fail("T15 fixture domain max b = %.17g expected 2", dmax[2]);
            double v[3];
            if (!lut.GetEntry(1, 0, 0, v) || v[0] != 0.25 || v[1] != 0.0 || v[2] != 0.0)
                Fail("T15 fixture entry (1,0,0) wrong");
            if (!lut.GetEntry(0, 1, 1, v) || v[0] != 0.0 || v[1] != 0.5 || v[2] != 1.5)
                Fail("T15 fixture entry (0,1,1) wrong");
        }

        // Minimal fixture without title/domain: defaults apply.
        const char* minimal =
            "LUT_3D_SIZE 2\n"
            "0 0 0\n" "1 0 0\n" "0 1 0\n" "1 1 0\n"
            "0 0 1\n" "1 0 1\n" "0 1 1\n" "1 1 1\n";
        CubeLUT ml;
        if (!ml.ReadFromString(minimal))
            Fail("T15 minimal fixture rejected: %s", ml.LastError().c_str());
        else
        {
            if (!ml.Title().empty())
                Fail("T15 minimal fixture grew a title");
            double dmin[3], dmax[3];
            ml.GetDomain(dmin, dmax);
            for (int k = 0; k < 3; ++k)
                if (dmin[k] != 0.0 || dmax[k] != 1.0)
                    Fail("T15 minimal fixture domain not default (comp %d)", k);
        }
    }

    // Malformed input: every case must be rejected, and a rejected read
    // must leave a previously loaded object bit-identical.
    {
        static const struct { const char* what; const char* text; } bad[] = {
            { "size 1",          "LUT_3D_SIZE 1\n0 0 0\n" },
            { "size 257",        "LUT_3D_SIZE 257\n" },
            { "size 0",          "LUT_3D_SIZE 0\n" },
            { "negative size",   "LUT_3D_SIZE -3\n" },
            { "non-numeric size","LUT_3D_SIZE abc\n" },
            { "missing size",    "TITLE \"x\"\n0 0 0\n" },
            { "1D LUT",          "LUT_1D_SIZE 10\n" },
            { "unknown keyword", "FOO 1\nLUT_3D_SIZE 2\n" },
            { "duplicate size",  "LUT_3D_SIZE 2\nLUT_3D_SIZE 2\n" },
            { "too few rows",    "LUT_3D_SIZE 2\n0 0 0\n1 1 1\n" },
            { "too many rows",   "LUT_3D_SIZE 2\n0 0 0\n0 0 0\n0 0 0\n0 0 0\n"
                                 "0 0 0\n0 0 0\n0 0 0\n0 0 0\n0 0 0\n" },
            { "2-number row",    "LUT_3D_SIZE 2\n0 0\n0 0 0\n0 0 0\n0 0 0\n"
                                 "0 0 0\n0 0 0\n0 0 0\n0 0 0\n" },
            { "junk in row",     "LUT_3D_SIZE 2\n0 zero 0\n0 0 0\n0 0 0\n0 0 0\n"
                                 "0 0 0\n0 0 0\n0 0 0\n0 0 0\n" },
            { "nan in row",      "LUT_3D_SIZE 2\nnan 0 0\n0 0 0\n0 0 0\n0 0 0\n"
                                 "0 0 0\n0 0 0\n0 0 0\n0 0 0\n" },
            { "inf in row",      "LUT_3D_SIZE 2\ninf 0 0\n0 0 0\n0 0 0\n0 0 0\n"
                                 "0 0 0\n0 0 0\n0 0 0\n0 0 0\n" },
            { "comma decimal",   "LUT_3D_SIZE 2\n0,5 0 0\n0 0 0\n0 0 0\n0 0 0\n"
                                 "0 0 0\n0 0 0\n0 0 0\n0 0 0\n" },
            { "domain min>=max", "LUT_3D_SIZE 2\nDOMAIN_MIN 0 0 1\nDOMAIN_MAX 1 1 1\n"
                                 "0 0 0\n0 0 0\n0 0 0\n0 0 0\n"
                                 "0 0 0\n0 0 0\n0 0 0\n0 0 0\n" },
            { "unquoted title",  "TITLE naked\nLUT_3D_SIZE 2\n0 0 0\n0 0 0\n0 0 0\n"
                                 "0 0 0\n0 0 0\n0 0 0\n0 0 0\n0 0 0\n" },
            { "keyword after data", "LUT_3D_SIZE 2\n0 0 0\nDOMAIN_MIN 0 0 0\n0 0 0\n"
                                 "0 0 0\n0 0 0\n0 0 0\n0 0 0\n0 0 0\n0 0 0\n" },
            { "data before size","0 0 0\nLUT_3D_SIZE 2\n" },
            { "empty input",     "" },
        };

        CubeLUT keeper;
        keeper.Create(2);
        FillLattice(keeper, 13579u);
        keeper.SetTitle("survivor");
        std::string before;
        keeper.WriteToString(before);

        for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
        {
            CubeLUT fresh;
            if (fresh.ReadFromString(bad[i].text))
                Fail("T15 reader accepted %s", bad[i].what);
            else if (fresh.IsValid())
                Fail("T15 rejected read (%s) left a fresh object valid", bad[i].what);
            if (fresh.LastError().empty())
                Fail("T15 rejected read (%s) set no LastError", bad[i].what);

            CubeLUT k2;
            k2.Create(2);
            FillLattice(k2, 13579u);
            k2.SetTitle("survivor");
            if (k2.ReadFromString(bad[i].text))
                Fail("T15 loaded reader accepted %s", bad[i].what);
            std::string after;
            if (!k2.WriteToString(after) || after != before)
                Fail("T15 rejected read (%s) changed a loaded object", bad[i].what);
        }
    }

    // Entry and metadata contract violations.
    {
        CubeLUT lut;
        lut.Create(2);
        double v[3] = { 0.0, 0.0, 0.0 };
        if (lut.SetEntry(2, 0, 0, v) || lut.SetEntry(-1, 0, 0, v))
            Fail("T15 SetEntry accepted an out-of-range index");
        double nanv[3] = { sqrt(-1.0), 0.0, 0.0 };
        if (lut.SetEntry(0, 0, 0, nanv))
            Fail("T15 SetEntry accepted a NaN value");
        double probe[3];
        lut.GetEntry(0, 0, 0, probe);
        if (probe[0] != 0.0)
            Fail("T15 rejected SetEntry modified the entry");
        if (lut.SetTitle("bad \" quote"))
            Fail("T15 SetTitle accepted an embedded quote");
        if (lut.SetTitle("bad\nnewline"))
            Fail("T15 SetTitle accepted an embedded newline");
        double dmin[3] = { 0.0, 0.0, 0.0 }, dmax[3] = { 1.0, 0.0, 1.0 };
        if (lut.SetDomain(dmin, dmax))
            Fail("T15 SetDomain accepted max <= min");
        CubeLUT fresh;
        std::string out;
        if (fresh.WriteToString(out))
            Fail("T15 WriteToString succeeded on an invalid object");
    }

    // Locale independence: with a comma-decimal locale active the writer
    // must still emit '.' and the reader must still parse '.'.
    {
        const char* prev = setlocale(LC_NUMERIC, NULL);
        std::string saved = prev ? prev : "C";
        if (!setlocale(LC_NUMERIC, "French_France.1252"))
            Fail("T15 could not activate the French locale for the test");
        else
        {
            CubeLUT lut;
            lut.Create(2);
            FillLattice(lut, 11111u);
            std::string text;
            if (!lut.WriteToString(text))
                Fail("T15 locale write failed");
            else if (text.find(',') != std::string::npos)
                Fail("T15 writer used ',' as a decimal separator under a comma locale");
            else
            {
                CubeLUT back;
                if (!back.ReadFromString(text) || !SameLattice(lut, back))
                    Fail("T15 locale round-trip failed");
            }
        }
        setlocale(LC_NUMERIC, saved.c_str());
    }

    // File-path leg: write to a temp file, read it back, clean up.
    {
        CubeLUT lut;
        lut.Create(4);
        FillLattice(lut, 97531u);
        const char* path = "T15_tmp.cube";
        if (!lut.WriteFile(path))
            Fail("T15 WriteFile failed: %s", lut.LastError().c_str());
        else
        {
            CubeLUT back;
            if (!back.ReadFile(path))
                Fail("T15 ReadFile failed: %s", back.LastError().c_str());
            else if (!SameLattice(lut, back))
                Fail("T15 file round-trip lattice mismatch");
            remove(path);
        }
        CubeLUT nf;
        if (nf.ReadFile("T15_no_such_file.cube"))
            Fail("T15 ReadFile succeeded on a missing file");
        if (lut.WriteFile("no_such_dir_T15\\x\\y.cube"))
            Fail("T15 WriteFile succeeded on an unwritable path");
    }
}

//////////////////////////////////////////////////////////////////////////
// T15 interp — CubeLUT tetrahedral evaluator oracle (pure assertion).
// The pinned properties are theorems of tetrahedral interpolation, so the
// tolerances are rounding-level: exact at nodes, exact everywhere for a
// lattice sampled from an affine function, the 6-tetrahedron barycentric
// weight table on indicator lattices (which trilinear interpolation fails,
// so the technique itself is pinned, not just its boundary behavior),
// continuity across cell faces and diagonal planes, domain mapping,
// clamping, and rejection of invalid/non-finite evaluation.
//////////////////////////////////////////////////////////////////////////

namespace T15Interp
{
    // Fixed affine map out = A*in + c, arbitrary but nonsingular.
    static const double kA[3][3] = {
        {  0.7,  0.2, -0.1 },
        { -0.3,  1.1,  0.4 },
        {  0.5, -0.6,  0.9 },
    };
    static const double kC[3] = { 0.25, -0.5, 0.125 };

    static void Affine(const double in[3], double out[3])
    {
        for (int r = 0; r < 3; ++r)
            out[r] = kC[r] + kA[r][0] * in[0] + kA[r][1] * in[1] + kA[r][2] * in[2];
    }

    // Fills every entry with the affine image of that node's DOMAIN position.
    static void FillAffine(CubeLUT& lut)
    {
        int n = lut.Size();
        double dmin[3], dmax[3];
        lut.GetDomain(dmin, dmax);
        for (int b = 0; b < n; ++b)
            for (int g = 0; g < n; ++g)
                for (int r = 0; r < n; ++r)
                {
                    double pos[3] = {
                        dmin[0] + r / (double)(n - 1) * (dmax[0] - dmin[0]),
                        dmin[1] + g / (double)(n - 1) * (dmax[1] - dmin[1]),
                        dmin[2] + b / (double)(n - 1) * (dmax[2] - dmin[2]),
                    };
                    double v[3];
                    Affine(pos, v);
                    if (!lut.SetEntry(r, g, b, v))
                        Fail("T15 interp FillAffine SetEntry(%d,%d,%d) failed", r, g, b);
                }
    }
}

static void RunT15Interp()
{
    printf("T15 CubeLUT tetrahedral evaluator oracle...\n");
    using namespace T15Interp;
    using T15Cube::Lcg;

    // Invalid object and non-finite input reject with zeroed output.
    {
        CubeLUT fresh;
        double in[3] = { 0.5, 0.5, 0.5 }, out[3] = { 9, 9, 9 };
        if (fresh.Evaluate(in, out))
            Fail("T15 interp Evaluate succeeded on an invalid object");
        if (out[0] != 0.0 || out[1] != 0.0 || out[2] != 0.0)
            Fail("T15 interp invalid Evaluate did not zero the output");
        CubeLUT lut;
        lut.Create(2);
        double bad[3] = { sqrt(-1.0), 0.5, 0.5 };
        out[0] = out[1] = out[2] = 9.0;
        if (lut.Evaluate(bad, out))
            Fail("T15 interp Evaluate accepted a NaN input");
        if (out[0] != 0.0 || out[1] != 0.0 || out[2] != 0.0)
            Fail("T15 interp NaN Evaluate did not zero the output");
    }

    // Identity lattice evaluates to the identity everywhere in the domain.
    {
        CubeLUT lut;
        lut.Create(5);
        Lcg g(52413u);
        for (int i = 0; i < 200; ++i)
        {
            double in[3] = { g.Next01(), g.Next01(), g.Next01() }, out[3];
            if (!lut.Evaluate(in, out))
                { Fail("T15 interp identity Evaluate failed"); break; }
            for (int k = 0; k < 3; ++k)
                if (fabs(out[k] - in[k]) > 1e-12)
                    Fail("T15 interp identity off at probe %d comp %d: %.3g",
                         i, k, fabs(out[k] - in[k]));
        }
    }

    // A lattice sampled from an affine function reproduces it EXACTLY
    // everywhere, not just at nodes - the defining property of barycentric-
    // linear interpolation.
    {
        CubeLUT lut;
        lut.Create(4);
        FillAffine(lut);
        Lcg g(90125u);
        for (int i = 0; i < 200; ++i)
        {
            double in[3] = { g.Next01(), g.Next01(), g.Next01() };
            double out[3], want[3];
            if (!lut.Evaluate(in, out))
                { Fail("T15 interp affine Evaluate failed"); break; }
            Affine(in, want);
            for (int k = 0; k < 3; ++k)
                if (fabs(out[k] - want[k]) > 1e-12)
                    Fail("T15 interp affine off at probe %d comp %d: %.3g",
                         i, k, fabs(out[k] - want[k]));
        }
    }

    // Exact at every node of a non-affine (random) lattice.
    {
        CubeLUT lut;
        lut.Create(4);
        T15Cube::FillLattice(lut, 86420u);
        for (int b = 0; b < 4; ++b)
            for (int g = 0; g < 4; ++g)
                for (int r = 0; r < 4; ++r)
                {
                    double in[3] = { r / 3.0, g / 3.0, b / 3.0 };
                    double out[3], want[3];
                    lut.GetEntry(r, g, b, want);
                    if (!lut.Evaluate(in, out))
                        { Fail("T15 interp node Evaluate failed"); continue; }
                    for (int k = 0; k < 3; ++k)
                        if (fabs(out[k] - want[k]) > 1e-12)
                            Fail("T15 interp node (%d,%d,%d) comp %d off by %.3g",
                                 r, g, b, k, fabs(out[k] - want[k]));
                }
    }

    // The 6-tetrahedron weight table itself, pinned on indicator lattices.
    // For cell fractions f with distinct values and hi/mid/lo the axis
    // order, the only corners with weight are c000 (1-f[hi]), the corner
    // with a 1 on the hi axis (f[hi]-f[mid]), the corner with 1s on hi+mid
    // (f[mid]-f[lo]), and c111 (f[lo]). Trilinear interpolation fails this.
    {
        static const double perms[6][3] = {
            { 0.6, 0.3, 0.1 }, { 0.6, 0.1, 0.3 },
            { 0.3, 0.6, 0.1 }, { 0.1, 0.6, 0.3 },
            { 0.3, 0.1, 0.6 }, { 0.1, 0.3, 0.6 },
        };
        for (int p = 0; p < 6; ++p)
        {
            const double* f = perms[p];
            int hi = 0, lo = 0;
            for (int k = 1; k < 3; ++k)
            {
                if (f[k] > f[hi]) hi = k;
                if (f[k] < f[lo]) lo = k;
            }
            int mid = 3 - hi - lo;
            for (int corner = 0; corner < 8; ++corner)
            {
                int bits[3] = { corner & 1, (corner >> 1) & 1, (corner >> 2) & 1 };
                CubeLUT lut;
                lut.Create(2);
                // zero everything, then the indicator
                double z[3] = { 0.0, 0.0, 0.0 };
                for (int c = 0; c < 8; ++c)
                    lut.SetEntry(c & 1, (c >> 1) & 1, (c >> 2) & 1, z);
                double one[3] = { 1.0, 0.0, 0.0 };
                lut.SetEntry(bits[0], bits[1], bits[2], one);

                double want;
                int setCount = bits[0] + bits[1] + bits[2];
                if (setCount == 0)
                    want = 1.0 - f[hi];
                else if (setCount == 3)
                    want = f[lo];
                else if (setCount == 1 && bits[hi] == 1)
                    want = f[hi] - f[mid];
                else if (setCount == 2 && bits[hi] == 1 && bits[mid] == 1)
                    want = f[mid] - f[lo];
                else
                    want = 0.0;

                double out[3];
                if (!lut.Evaluate(f, out))
                    { Fail("T15 interp weight-table Evaluate failed"); continue; }
                if (fabs(out[0] - want) > 1e-12)
                    Fail("T15 interp weight perm %d corner %d = %.17g expected %.17g "
                         "(tetrahedral, not trilinear)", p, corner, out[0], want);
            }
        }
    }

    // Custom domain: the same affine exactness must hold when the lattice
    // spans a non-default box, and out-of-domain inputs clamp to the face.
    {
        CubeLUT lut;
        lut.Create(3);
        double dmin[3] = { -1.0, 0.0, -0.5 }, dmax[3] = { 3.0, 2.0, 0.5 };
        if (!lut.SetDomain(dmin, dmax))
            Fail("T15 interp SetDomain failed");
        FillAffine(lut);
        Lcg g(19283u);
        for (int i = 0; i < 100; ++i)
        {
            double in[3], out[3], want[3];
            for (int k = 0; k < 3; ++k)
                in[k] = dmin[k] + g.Next01() * (dmax[k] - dmin[k]);
            if (!lut.Evaluate(in, out))
                { Fail("T15 interp domain Evaluate failed"); break; }
            Affine(in, want);
            for (int k = 0; k < 3; ++k)
                if (fabs(out[k] - want[k]) > 1e-12)
                    Fail("T15 interp domain probe %d comp %d off by %.3g",
                         i, k, fabs(out[k] - want[k]));
        }
        double outside[3] = { 5.0, -1.0, 0.2 };
        double clamped[3] = { 3.0,  0.0, 0.2 };
        double a[3], b[3];
        lut.Evaluate(outside, a);
        lut.Evaluate(clamped, b);
        for (int k = 0; k < 3; ++k)
            if (a[k] != b[k])
                Fail("T15 interp out-of-domain input did not clamp (comp %d)", k);
    }

    // Continuity across cell faces and across the diagonal planes where
    // the fraction ordering (and thus the tetrahedron) changes.
    {
        CubeLUT lut;
        lut.Create(5);
        T15Cube::FillLattice(lut, 75319u);
        const double eps = 1e-9;
        // cell faces: x = k/4 crossed along each axis
        for (int axis = 0; axis < 3; ++axis)
            for (int k = 1; k < 4; ++k)
            {
                double lo[3] = { 0.37, 0.61, 0.23 };
                double hi[3] = { 0.37, 0.61, 0.23 };
                lo[axis] = k / 4.0 - eps;
                hi[axis] = k / 4.0 + eps;
                double a[3], b[3];
                lut.Evaluate(lo, a);
                lut.Evaluate(hi, b);
                for (int c = 0; c < 3; ++c)
                    if (fabs(a[c] - b[c]) > 1e-6)
                        Fail("T15 interp discontinuity across face axis %d k=%d: %.3g",
                             axis, k, fabs(a[c] - b[c]));
            }
        // diagonal planes: equal fractions inside one cell, straddled by
        // nudging one axis in each direction
        static const double base[3][3] = {
            { 0.30, 0.30, 0.10 },   // fr == fg
            { 0.30, 0.10, 0.30 },   // fr == fb
            { 0.10, 0.30, 0.30 },   // fg == fb
        };
        for (int t = 0; t < 3; ++t)
        {
            int axis = (t == 0) ? 0 : (t == 1) ? 0 : 1;   // one of the tied axes
            double lo[3], hi[3];
            for (int c = 0; c < 3; ++c)
            {
                lo[c] = base[t][c];
                hi[c] = base[t][c];
            }
            lo[axis] -= eps;
            hi[axis] += eps;
            double a[3], b[3];
            lut.Evaluate(lo, a);
            lut.Evaluate(hi, b);
            for (int c = 0; c < 3; ++c)
                if (fabs(a[c] - b[c]) > 1e-6)
                    Fail("T15 interp discontinuity across diagonal %d: %.3g",
                         t, fabs(a[c] - b[c]));
        }
    }

    // Deterministic.
    {
        CubeLUT lut;
        lut.Create(4);
        T15Cube::FillLattice(lut, 31415u);
        double in[3] = { 0.31, 0.77, 0.52 }, a[3], b[3];
        lut.Evaluate(in, a);
        lut.Evaluate(in, b);
        for (int k = 0; k < 3; ++k)
            if (a[k] != b[k])
                Fail("T15 interp nondeterministic (comp %d)", k);
    }
}

//////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
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
    RunT15();   // T11-T14 are claimed by in-flight branches; see the header
    RunT15Interp();

    if (g_failures)
    {
        printf("\n%d FAILURE(S)\n", g_failures);
        return 1;
    }
    printf("\nALL TESTS PASSED (%s mode)\n", g_genMode ? "gen" : "verify");
    return 0;
}
