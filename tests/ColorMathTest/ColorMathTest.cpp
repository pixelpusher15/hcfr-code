// ColorMathTest — 8-bit golden-master regression harness for libHCFR.
// See TEN-BIT-PIPELINE-PLAN.md (repo root), PR 1.
//
// Usage:
//   ColorMathTest gen    [goldenDir]   regenerate golden files (run ONLY on a known-good build)
//   ColorMathTest verify [goldenDir]   compare current libHCFR output against golden files
// goldenDir defaults to "golden" relative to the current directory.
// Exit code 0 = all pass, nonzero = mismatch/failure.
//
// T1 needs no golden file: it carries a frozen copy of the legacy quantizer as an oracle.
// T2/T3/T4/T6 dump deterministic tables and compare them exactly against checked-in goldens.
// T5 (rPI emission) is added by PR 3. T7 (.chc round-trip) needs app-level linkage and is
// deliberately not in this console harness.

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
// T2 — ArrayIndexToGrayLevel over all sizes x three rounding modes.
//////////////////////////////////////////////////////////////////////////
static void RunT2()
{
    printf("T2 grey-level tables...\n");
    std::string out;
    static const int sizes[] = { 2, 5, 6, 11, 12, 16, 21, 25, 101 };
    out += "# ArrayIndexToGrayLevel(index,size,roundDown,b10bit) %.17g\n";
    for (int s = 0; s < sizeof(sizes)/sizeof(sizes[0]); ++s)
    {
        int size = sizes[s];
        for (int mode = 0; mode < 3; ++mode)   // 0=normal, 1=roundDown, 2=10bit
        {
            bool rd  = (mode == 1);
            bool b10 = (mode == 2);
            for (int i = 0; i < size; ++i)
                AppendF(out, "%d,%d,%d,%d,%.17g\n", size, mode, i, 0,
                        ArrayIndexToGrayLevel(i, size, rd, b10));
        }
    }
    HandleTable("T2_graylevels.txt", out);
}

//////////////////////////////////////////////////////////////////////////
// T3 — GrayLevelToGrayProp over a dense level sweep x three rounding modes.
//////////////////////////////////////////////////////////////////////////
static void RunT3()
{
    printf("T3 grey-level props...\n");
    std::string out;
    out += "# GrayLevelToGrayProp(level,roundDown,b10bit) %.17g, level = i*0.25\n";
    for (int mode = 0; mode < 3; ++mode)
    {
        bool rd  = (mode == 1);
        bool b10 = (mode == 2);
        for (int i = 0; i <= 400; ++i)          // 0..100 step 0.25
            AppendF(out, "%d,%d,%.17g\n", mode, i,
                    GrayLevelToGrayProp(i * 0.25, rd, b10));
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
    for (int i = 0; i <= 1000000; ++i)
    {
        double pct = i * 1e-4;
        int lgFull = (int)floor(pct / 100.0 * 255.0 + 0.5);
        lgFull = lgFull < 0 ? 0 : (lgFull > 255 ? 255 : lgFull);
        if (PiPercentToCode(pct, false, 8) != lgFull)
            Fail("T5 full8 pct=%.17g", pct);
        int lgLim = (int)floor(pct / 100.0 * 219.0 + 16.5);
        lgLim = lgLim < 0 ? 0 : (lgLim > 235 ? 235 : lgLim);
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
    if (PiPercentToCode(200.0, true, 10) != 940)   Fail("T5 lim10 clamp high");
    if (PiBackground8ToCode(0.0, true, 10) != 64)    Fail("T5 bglim10 0");
    if (PiBackground8ToCode(255.0, true, 10) != 940) Fail("T5 bglim10 255");
    if (PiBackground8ToCode(255.0, false, 10) != 1023) Fail("T5 bgfull10 255");
    // bits=10 monotonic + limited grid = 877 distinct codes
    for (int r = 0; r < 2; ++r)
    {
        bool lim = (r != 0);
        int prev = -1;
        std::vector<char> seen(1024, 0);
        for (int i = 0; i <= 1000000; ++i)
        {
            int code = PiPercentToCode(i * 1e-4, lim, 10);
            if (code < prev) Fail("T5 10bit non-monotonic range=%d pct=%.17g", r, i * 1e-4);
            prev = code;
            seen[code] = 1;
        }
        int distinct = 0;
        for (int c = 0; c < 1024; ++c) distinct += seen[c];
        if (lim && distinct != 877) Fail("T5 lim10 grid: %d distinct, expected 877", distinct);
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
    RunT8();
    RunT9();

    if (g_failures)
    {
        printf("\n%d FAILURE(S)\n", g_failures);
        return 1;
    }
    printf("\nALL TESTS PASSED (%s mode)\n", g_genMode ? "gen" : "verify");
    return 0;
}
