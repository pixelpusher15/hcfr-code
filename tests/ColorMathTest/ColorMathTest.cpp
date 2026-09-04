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
// and T12 asserts the BT.2390 inverse LUT round-trips and never faults on an empty table. The .chc round-trip originally planned for the T7
// slot needs app-level linkage and is deliberately still not in this console harness.

#include <afx.h>
#include "../../libHCFR/Color.h"

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
    RunT11();
    RunT12();

    if (g_failures)
    {
        printf("\n%d FAILURE(S)\n", g_failures);
        return 1;
    }
    printf("\nALL TESTS PASSED (%s mode)\n", g_genMode ? "gen" : "verify");
    return 0;
}
