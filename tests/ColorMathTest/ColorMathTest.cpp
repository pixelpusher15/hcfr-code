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
// T10 asserts gamut-basis consistency, T15 asserts the CubeLUT .cube format contract
// and its tetrahedral evaluator, and T16 asserts the LUT lattice algebra (typed
// compose/resample, ShaperCurve, and the shaper+cube split).
// T11-T14 are deliberately skipped here, reserved for in-flight branches: PR #178 claims
// T11/T12 (BT.2390 oracles); the csv-provenance and wtw-reference branches carry their
// own T11/T12 copies TODAY and renumber when they rebase (T14 is held for that
// renumbering; T16 is this branch's second slot per the next-free-number rule, so the
// next free number is T17); the display-model series owns T13. Check open branches
// before assigning any new T number.
// The .chc round-trip originally planned for the T7 slot needs app-level linkage and is
// deliberately still not in this console harness.

#include <afx.h>
#include "../../libHCFR/Color.h"
#include "../../libHCFR/CubeLUT.h"
#include "../../libHCFR/LutOps.h"

#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <cstdlib>
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
                        // The tagged first components are integers, so this
                        // atof never sees a decimal separator and is safe
                        // under any LC_NUMERIC.
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

        // A leading UTF-8 BOM (Notepad, some LUT exporters) must not reject
        // an otherwise valid file.
        std::string bommed = std::string("\xEF\xBB\xBF") + minimal;
        CubeLUT bl;
        if (!bl.ReadFromString(bommed))
            Fail("T15 BOM-prefixed fixture rejected: %s", bl.LastError().c_str());
        else if (bl.Size() != 2)
            Fail("T15 BOM-prefixed fixture size %d expected 2", bl.Size());

        // Resolve/IRIDAS LUT_3D_INPUT_RANGE shorthand maps onto the domain.
        const char* ranged =
            "LUT_3D_SIZE 2\n"
            "LUT_3D_INPUT_RANGE 0.0 2.0\n"
            "0 0 0\n" "1 0 0\n" "0 1 0\n" "1 1 0\n"
            "0 0 1\n" "1 0 1\n" "0 1 1\n" "1 1 1\n";
        CubeLUT rl;
        if (!rl.ReadFromString(ranged))
            Fail("T15 LUT_3D_INPUT_RANGE fixture rejected: %s", rl.LastError().c_str());
        else
        {
            double dmin[3], dmax[3];
            rl.GetDomain(dmin, dmax);
            for (int k = 0; k < 3; ++k)
                if (dmin[k] != 0.0 || dmax[k] != 2.0)
                    Fail("T15 LUT_3D_INPUT_RANGE domain wrong (comp %d)", k);
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
            { "truncated size-256 header", "LUT_3D_SIZE 256\n" },
            { "overflowing domain span", "LUT_3D_SIZE 2\nDOMAIN_MIN -1e308 0 0\n"
                                 "DOMAIN_MAX 1e308 1 1\n"
                                 "0 0 0\n0 0 0\n0 0 0\n0 0 0\n"
                                 "0 0 0\n0 0 0\n0 0 0\n0 0 0\n" },
            { "duplicate input range", "LUT_3D_SIZE 2\nLUT_3D_INPUT_RANGE 0 1\n"
                                 "LUT_3D_INPUT_RANGE 0 2\n" },
            { "input range + domain conflict", "LUT_3D_SIZE 2\nLUT_3D_INPUT_RANGE 0 1\n"
                                 "DOMAIN_MIN 0 0 0\n" },
            { "inverted input range", "LUT_3D_SIZE 2\nLUT_3D_INPUT_RANGE 1 0\n" },
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
        // Finite endpoints whose span overflows would make Evaluate divide
        // by +inf and return a constant; the domain must be rejected whole.
        double huge0[3] = { -1e308, 0.0, 0.0 }, huge1[3] = { 1e308, 1.0, 1.0 };
        if (lut.SetDomain(huge0, huge1))
            Fail("T15 SetDomain accepted an overflowing span");
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
        // Any comma-decimal locale exercises the assertion; try a few names
        // so the oracle does not fail on hosts missing one locale pack.
        static const char* commaLocales[] = {
            "French_France.1252", "fr-FR", "German_Germany.1252", "de-DE",
        };
        const char* active = 0;
        for (size_t i = 0; i < sizeof(commaLocales) / sizeof(commaLocales[0]); ++i)
            if (setlocale(LC_NUMERIC, commaLocales[i]))
                { active = commaLocales[i]; break; }
        if (!active)
            Fail("T15 could not activate any comma-decimal locale for the test");
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
        if (!lut.Evaluate(outside, a) || !lut.Evaluate(clamped, b))
            Fail("T15 interp clamp Evaluate failed");
        else
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
                if (!lut.Evaluate(lo, a) || !lut.Evaluate(hi, b))
                    { Fail("T15 interp face Evaluate failed"); continue; }
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
            if (!lut.Evaluate(lo, a) || !lut.Evaluate(hi, b))
                { Fail("T15 interp diagonal Evaluate failed"); continue; }
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
        if (!lut.Evaluate(in, a) || !lut.Evaluate(in, b))
            Fail("T15 interp determinism Evaluate failed");
        else
            for (int k = 0; k < 3; ++k)
                if (a[k] != b[k])
                    Fail("T15 interp nondeterministic (comp %d)", k);
    }

    // In-place evaluation: in and out may alias per the contract.
    {
        CubeLUT lut;
        lut.Create(5);
        T15Cube::FillLattice(lut, 62831u);
        double p[3] = { 0.31, 0.77, 0.52 };
        double want[3];
        if (!lut.Evaluate(p, want))
            Fail("T15 interp aliasing reference Evaluate failed");
        double inout[3] = { 0.31, 0.77, 0.52 };
        if (!lut.Evaluate(inout, inout))
            Fail("T15 interp in-place Evaluate failed");
        for (int k = 0; k < 3; ++k)
            if (inout[k] != want[k])
                Fail("T15 interp in-place evaluation differs from two-buffer (comp %d)", k);
    }
}

//////////////////////////////////////////////////////////////////////////
// T16 — LUT lattice algebra oracle (pure assertion, no golden file).
// Every pinned property is a theorem of the specified constructions, so the
// tolerances are rounding-level: composition and resampling are DEFINED as
// lattice sampling through the public evaluators (node-definition oracles),
// affine lattices make whole chains exact everywhere, a piecewise-linear
// shaper has an exact piecewise-linear inverse, and a purely per-channel
// source collapses the split's residual cube to the identity. Contract
// type-checking (LutContract.h) is pinned by a compatibility table plus
// refusal/stamping through ComposeCube.
//////////////////////////////////////////////////////////////////////////

namespace T16Ops
{
    using T15Cube::Lcg;

    // Affine mix with unit row sums and nonneg coefficients: the neutral
    // axis of kM*f(x)+kOff is strictly increasing whenever every f is.
    static const double kM[3][3] = {
        { 0.80, 0.15, 0.05 },
        { 0.10, 0.75, 0.15 },
        { 0.05, 0.20, 0.75 },
    };
    static const double kOff[3] = { 0.02, -0.03, 0.05 };

    static void AffineB(const double in[3], double out[3])
    {
        for (int r = 0; r < 3; ++r)
            out[r] = kOff[r] + kM[r][0] * in[0] + kM[r][1] * in[1] + kM[r][2] * in[2];
    }

    static void FillAffineB(CubeLUT& lut)
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
                    AffineB(pos, v);
                    if (!lut.SetEntry(r, g, b, v))
                        Fail("T16 FillAffineB SetEntry(%d,%d,%d) failed", r, g, b);
                }
    }

    static const double kGamma[3] = { 1.8, 2.2, 2.6 };

    // Separable per-channel power lattice over the default 0..1 domain.
    static void FillGamma(CubeLUT& lut)
    {
        int n = lut.Size();
        for (int b = 0; b < n; ++b)
            for (int g = 0; g < n; ++g)
                for (int r = 0; r < n; ++r)
                {
                    double v[3] = {
                        pow(r / (double)(n - 1), kGamma[0]),
                        pow(g / (double)(n - 1), kGamma[1]),
                        pow(b / (double)(n - 1), kGamma[2]),
                    };
                    if (!lut.SetEntry(r, g, b, v))
                        Fail("T16 FillGamma SetEntry(%d,%d,%d) failed", r, g, b);
                }
    }

    // Non-separable but neutral-monotone: kM * gamma(x) + kOff.
    static void FillMixedGamma(CubeLUT& lut)
    {
        int n = lut.Size();
        for (int b = 0; b < n; ++b)
            for (int g = 0; g < n; ++g)
                for (int r = 0; r < n; ++r)
                {
                    double f[3] = {
                        pow(r / (double)(n - 1), kGamma[0]),
                        pow(g / (double)(n - 1), kGamma[1]),
                        pow(b / (double)(n - 1), kGamma[2]),
                    };
                    double v[3];
                    AffineB(f, v);
                    if (!lut.SetEntry(r, g, b, v))
                        Fail("T16 FillMixedGamma SetEntry(%d,%d,%d) failed", r, g, b);
                }
    }

    static LutSignalType Sig(LutSignalType::ColorSpace cs,
                             LutSignalType::Transfer tf,
                             LutSignalType::Range rg)
    {
        LutSignalType s;
        s.colorSpace = cs;
        s.transfer = tf;
        s.range = rg;
        return s;
    }

    // A sentinel output object: refused operations must leave it untouched.
    static void MakeSentinel(CubeLUT& lut)
    {
        if (!lut.Create(2))
            Fail("T16 sentinel Create failed");
        double v[3] = { 0.125, 0.25, 0.375 };
        if (!lut.SetEntry(1, 0, 1, v))
            Fail("T16 sentinel SetEntry failed");
        if (!lut.SetTitle("keep me"))
            Fail("T16 sentinel SetTitle failed");
    }

    static bool SentinelIntact(const CubeLUT& lut)
    {
        if (lut.Size() != 2 || lut.Title() != "keep me")
            return false;
        double v[3];
        if (!lut.GetEntry(1, 0, 1, v))
            return false;
        return v[0] == 0.125 && v[1] == 0.25 && v[2] == 0.375;
    }
}

static void RunT16()
{
    printf("T16 LUT lattice algebra oracle...\n");
    using namespace T16Ops;
    using T15Cube::SameLattice;

    //---------------------------------------------------------------- contracts
    // Signal-type validity and the wildcard compatibility rule.
    {
        LutSignalType u;    // default: fully unspecified
        if (!ValidSignalType(u))
            Fail("T16 contract: default signal type not valid");
        LutSignalType a = Sig(LutSignalType::CS_BT709, LutSignalType::TF_GAMMA22,
                              LutSignalType::RANGE_FULL);
        LutSignalType b = Sig(LutSignalType::CS_BT2020, LutSignalType::TF_GAMMA22,
                              LutSignalType::RANGE_FULL);
        if (!ValidSignalType(a) || !ValidSignalType(b))
            Fail("T16 contract: specified signal types not valid");
        if (!CompatibleSignalTypes(a, a))
            Fail("T16 contract: identical types not compatible");
        if (CompatibleSignalTypes(a, b))
            Fail("T16 contract: differing color spaces reported compatible");
        if (!CompatibleSignalTypes(a, u) || !CompatibleSignalTypes(u, b))
            Fail("T16 contract: unspecified side did not act as a wildcard");
        LutSignalType partial = u;
        partial.range = LutSignalType::RANGE_VIDEO;
        if (CompatibleSignalTypes(a, partial))
            Fail("T16 contract: range clash reported compatible");
        LutSignalType bad = a;
        bad.colorSpace = (LutSignalType::ColorSpace)99;
        if (ValidSignalType(bad))
            Fail("T16 contract: out-of-range color space reported valid");
        LutContract c;
        if (!ValidContract(c))
            Fail("T16 contract: default contract not valid");
        c.output = bad;
        if (ValidContract(c))
            Fail("T16 contract: contract with a bad field reported valid");
    }

    // The contract rides the CubeLUT: set/reject/reset semantics.
    {
        CubeLUT lut;
        lut.Create(2);
        LutContract c;
        c.input = Sig(LutSignalType::CS_BT709, LutSignalType::TF_GAMMA22,
                      LutSignalType::RANGE_FULL);
        c.output = Sig(LutSignalType::CS_BT2020, LutSignalType::TF_PQ,
                       LutSignalType::RANGE_VIDEO);
        if (!lut.SetContract(c))
            Fail("T16 contract: SetContract rejected a valid contract");
        if (!SameSignalType(lut.Contract().input, c.input)
            || !SameSignalType(lut.Contract().output, c.output))
            Fail("T16 contract: stored contract differs from the one set");
        LutContract bad = c;
        bad.input.transfer = (LutSignalType::Transfer)77;
        if (lut.SetContract(bad))
            Fail("T16 contract: SetContract accepted an out-of-range transfer");
        if (!SameSignalType(lut.Contract().input, c.input))
            Fail("T16 contract: rejected SetContract changed the contract");

        // Create resets to fully unspecified.
        lut.Create(3);
        if (!SameSignalType(lut.Contract().input, LutSignalType())
            || !SameSignalType(lut.Contract().output, LutSignalType()))
            Fail("T16 contract: Create did not reset the contract");

        // A successful Read resets (the format carries no contract), a
        // failed Read preserves.
        lut.SetContract(c);
        if (!lut.ReadFromString("LUT_3D_SIZE 2\n"
                                "0 0 0\n1 0 0\n0 1 0\n1 1 0\n"
                                "0 0 1\n1 0 1\n0 1 1\n1 1 1\n"))
            Fail("T16 contract: valid .cube read failed: %s", lut.LastError().c_str());
        if (!SameSignalType(lut.Contract().input, LutSignalType())
            || !SameSignalType(lut.Contract().output, LutSignalType()))
            Fail("T16 contract: successful Read did not reset the contract");
        lut.SetContract(c);
        if (lut.ReadFromString("LUT_3D_SIZE 2\nnot a number\n"))
            Fail("T16 contract: malformed .cube read succeeded");
        if (!SameSignalType(lut.Contract().input, c.input)
            || !SameSignalType(lut.Contract().output, c.output))
            Fail("T16 contract: failed Read did not preserve the contract");

        // Copies carry the contract.
        CubeLUT copy = lut;
        if (!SameSignalType(copy.Contract().input, c.input)
            || !SameSignalType(copy.Contract().output, c.output))
            Fail("T16 contract: copy did not carry the contract");
    }

    //---------------------------------------------------------------- ShaperCurve
    // Installation rejections; validity only when all three channels exist.
    {
        ShaperCurve s;
        const double up[4] = { 0.0, 0.25, 0.5, 1.0 };
        if (s.IsValid())
            Fail("T16 shaper: fresh curve reported valid");
        if (s.SetChannel(-1, up, 4, 0.0, 1.0) || s.SetChannel(3, up, 4, 0.0, 1.0))
            Fail("T16 shaper: bad channel index accepted");
        if (s.SetChannel(0, up, 1, 0.0, 1.0))
            Fail("T16 shaper: single-sample channel accepted");
        const double flat[3] = { 0.0, 0.5, 0.5 };
        if (s.SetChannel(0, flat, 3, 0.0, 1.0))
            Fail("T16 shaper: non-strictly-increasing samples accepted");
        const double down[3] = { 0.0, 0.6, 0.4 };
        if (s.SetChannel(0, down, 3, 0.0, 1.0))
            Fail("T16 shaper: decreasing samples accepted");
        double nf[3] = { 0.0, sqrt(-1.0), 1.0 };
        if (s.SetChannel(0, nf, 3, 0.0, 1.0))
            Fail("T16 shaper: non-finite sample accepted");
        if (s.SetChannel(0, up, 4, 1.0, 1.0) || s.SetChannel(0, up, 4, 2.0, 1.0))
            Fail("T16 shaper: bad domain accepted");
        if (s.SetChannel(0, up, 4, 0.0, sqrt(-1.0)))
            Fail("T16 shaper: non-finite domain accepted");

        if (!s.SetChannel(0, up, 4, 0.0, 1.0))
            Fail("T16 shaper: valid channel 0 rejected");
        if (s.IsValid())
            Fail("T16 shaper: valid with only one channel installed");
        if (!s.SetChannel(1, up, 4, 0.0, 1.0) || !s.SetChannel(2, up, 4, 0.0, 1.0))
            Fail("T16 shaper: valid channels 1/2 rejected");
        if (!s.IsValid())
            Fail("T16 shaper: not valid with all three channels installed");

        // A rejected re-install keeps the old channel.
        if (s.SetChannel(1, down, 3, 0.0, 1.0))
            Fail("T16 shaper: decreasing re-install accepted");
        if (s.Count(1) != 4)
            Fail("T16 shaper: rejected re-install changed the channel (count %d)",
                 s.Count(1));

        // Introspection.
        double lo = 9, hi = 9, v = 9;
        if (!s.GetDomain(1, lo, hi) || lo != 0.0 || hi != 1.0)
            Fail("T16 shaper: GetDomain wrong (%g..%g)", lo, hi);
        if (!s.GetSample(2, 1, v) || v != 0.25)
            Fail("T16 shaper: GetSample wrong (%g)", v);
        if (s.GetSample(2, 4, v) || s.GetSample(2, -1, v))
            Fail("T16 shaper: out-of-range GetSample succeeded");
    }

    // Piecewise-linear evaluation: exact against the hand formula, with
    // per-channel domains and end clamping; invalid/non-finite reject with
    // zeroed output; in-place evaluation allowed.
    {
        ShaperCurve s;
        // Channel 0: t^2 sampled at 5 uniform points on [0,1].
        const double sq[5] = { 0.0, 0.0625, 0.25, 0.5625, 1.0 };
        // Channel 1: over [-0.5, 1.5].
        const double c1[3] = { -1.0, 0.0, 3.0 };
        // Channel 2: over [0.25, 0.75].
        const double c2[4] = { 2.0, 2.5, 4.0, 8.0 };
        {
            ShaperCurve fresh;
            double in[3] = { 0.5, 0.5, 0.5 }, out[3] = { 9, 9, 9 };
            if (fresh.Evaluate(in, out))
                Fail("T16 shaper: Evaluate succeeded on an invalid curve");
            if (out[0] != 0.0 || out[1] != 0.0 || out[2] != 0.0)
                Fail("T16 shaper: invalid Evaluate did not zero the output");
            if (fresh.EvaluateInverse(in, out))
                Fail("T16 shaper: EvaluateInverse succeeded on an invalid curve");
        }
        if (!s.SetChannel(0, sq, 5, 0.0, 1.0)
            || !s.SetChannel(1, c1, 3, -0.5, 1.5)
            || !s.SetChannel(2, c2, 4, 0.25, 0.75))
            Fail("T16 shaper: PWL install failed");

        // Hand-computed probes. ch0 x=0.3: segment [0.25,0.5], f = 0.0625 +
        // (0.3-0.25)/0.25 * (0.25-0.0625) = 0.1. ch1 x=0.25: segment
        // [-0.5,0.5] -> f = -1 + 0.75*1.0... samples uniform over the
        // channel domain: nodes -0.5, 0.5, 1.5; x=0.25 -> -1 + 0.75/1.0 *
        // (0 - -1) = -0.25. ch2 x=0.5: nodes 0.25, 0.41666.., 0.58333..,
        // 0.75; x=0.5 lies mid-segment 1 -> 2.5 + 0.5*(4.0-2.5) = 3.25.
        double in[3] = { 0.3, 0.25, 0.5 }, out[3];
        if (!s.Evaluate(in, out))
            Fail("T16 shaper: PWL Evaluate failed");
        else
        {
            const double want[3] = { 0.1, -0.25, 3.25 };
            for (int k = 0; k < 3; ++k)
                if (fabs(out[k] - want[k]) > 1e-12)
                    Fail("T16 shaper: PWL comp %d = %.17g want %.17g", k, out[k], want[k]);
        }

        // End clamping on both sides, per channel.
        double lowIn[3] = { -2.0, -2.0, 0.0 }, hiIn[3] = { 2.0, 2.0, 2.0 };
        double lowOut[3], hiOut[3];
        if (!s.Evaluate(lowIn, lowOut) || !s.Evaluate(hiIn, hiOut))
            Fail("T16 shaper: clamped Evaluate failed");
        else
        {
            if (lowOut[0] != 0.0 || lowOut[1] != -1.0 || lowOut[2] != 2.0)
                Fail("T16 shaper: low clamp wrong (%g,%g,%g)",
                     lowOut[0], lowOut[1], lowOut[2]);
            if (hiOut[0] != 1.0 || hiOut[1] != 3.0 || hiOut[2] != 8.0)
                Fail("T16 shaper: high clamp wrong (%g,%g,%g)",
                     hiOut[0], hiOut[1], hiOut[2]);
        }

        // Non-finite input rejects with zeroed output.
        double bad[3] = { 0.5, sqrt(-1.0), 0.5 };
        out[0] = out[1] = out[2] = 9.0;
        if (s.Evaluate(bad, out))
            Fail("T16 shaper: Evaluate accepted a NaN input");
        if (out[0] != 0.0 || out[1] != 0.0 || out[2] != 0.0)
            Fail("T16 shaper: NaN Evaluate did not zero the output");

        // In-place evaluation.
        double inout[3] = { 0.3, 0.25, 0.5 }, want2[3];
        if (!s.Evaluate(inout, want2)) { /* checked above */ }
        double alias[3] = { 0.3, 0.25, 0.5 };
        if (!s.Evaluate(alias, alias))
            Fail("T16 shaper: in-place Evaluate failed");
        for (int k = 0; k < 3; ++k)
            if (alias[k] != want2[k])
                Fail("T16 shaper: in-place Evaluate differs (comp %d)", k);
    }

    // The piecewise-linear inverse is exact: round trips both ways at many
    // probes on random strictly-increasing curves; values outside the range
    // clamp to the curve ends.
    {
        ShaperCurve s;
        Lcg g(16161u);
        double first[3], last[3];
        for (int c = 0; c < 3; ++c)
        {
            double samples[9];
            double acc = g.Next01() - 0.5;
            for (int i = 0; i < 9; ++i)
            {
                samples[i] = acc;
                acc += 0.05 + g.Next01();      // strictly increasing steps
            }
            first[c] = samples[0];
            last[c] = samples[8];
            if (!s.SetChannel(c, samples, 9, -0.25, 1.25))
                Fail("T16 shaper: inverse-test install failed (channel %d)", c);
        }
        for (int i = 0; i < 200; ++i)
        {
            double x[3], y[3], back[3];
            for (int c = 0; c < 3; ++c)
                x[c] = -0.25 + g.Next01() * 1.5;
            if (!s.Evaluate(x, y) || !s.EvaluateInverse(y, back))
                { Fail("T16 shaper: inverse round trip failed at probe %d", i); break; }
            for (int c = 0; c < 3; ++c)
                if (fabs(back[c] - x[c]) > 1e-12)
                    Fail("T16 shaper: inverse(forward) off at probe %d ch %d: %.3g",
                         i, c, fabs(back[c] - x[c]));
            // And the other direction, from a value inside the range.
            double v[3], t[3], fwd[3];
            for (int c = 0; c < 3; ++c)
                v[c] = first[c] + g.Next01() * (last[c] - first[c]);
            if (!s.EvaluateInverse(v, t) || !s.Evaluate(t, fwd))
                { Fail("T16 shaper: forward(inverse) failed at probe %d", i); break; }
            for (int c = 0; c < 3; ++c)
                if (fabs(fwd[c] - v[c]) > 1e-12)
                    Fail("T16 shaper: forward(inverse) off at probe %d ch %d: %.3g",
                         i, c, fabs(fwd[c] - v[c]));
        }
        // Out-of-range values clamp to the curve ends.
        double below[3] = { first[0] - 5.0, first[1] - 5.0, first[2] - 5.0 };
        double above[3] = { last[0] + 5.0, last[1] + 5.0, last[2] + 5.0 };
        double tb[3], ta[3];
        if (!s.EvaluateInverse(below, tb) || !s.EvaluateInverse(above, ta))
            Fail("T16 shaper: clamped inverse failed");
        else
            for (int c = 0; c < 3; ++c)
            {
                if (tb[c] != -0.25)
                    Fail("T16 shaper: below-range inverse ch %d = %.17g want -0.25",
                         c, tb[c]);
                if (ta[c] != 1.25)
                    Fail("T16 shaper: above-range inverse ch %d = %.17g want 1.25",
                         c, ta[c]);
            }
    }

    //---------------------------------------------------------------- ComposeCube
    // Node definition: every entry of the result IS second(first(node)),
    // for arbitrary lattices, through the public evaluators.
    {
        CubeLUT a, b, out;
        a.Create(4);
        T15Cube::FillLattice(a, 24680u);
        b.Create(3);
        T15Cube::FillLattice(b, 13579u);
        std::string err = "junk";
        if (!ComposeCube(a, b, 5, out, &err))
            Fail("T16 compose: node-definition compose refused: %s", err.c_str());
        else
        {
            if (!err.empty())
                Fail("T16 compose: success did not clear err (\"%s\")", err.c_str());
            if (out.Size() != 5)
                Fail("T16 compose: wrong result size %d", out.Size());
            if (!out.Title().empty())
                Fail("T16 compose: result title not empty");
            for (int bb = 0; bb < 5; ++bb)
                for (int gg = 0; gg < 5; ++gg)
                    for (int rr = 0; rr < 5; ++rr)
                    {
                        double p[3] = { rr / 4.0, gg / 4.0, bb / 4.0 };
                        double mid[3], want[3], got[3];
                        if (!a.Evaluate(p, mid) || !b.Evaluate(mid, want)
                            || !out.GetEntry(rr, gg, bb, got))
                            { Fail("T16 compose: node-definition eval failed"); continue; }
                        for (int k = 0; k < 3; ++k)
                            if (fabs(got[k] - want[k]) > 1e-13)
                                Fail("T16 compose: node (%d,%d,%d) comp %d off by %.3g",
                                     rr, gg, bb, k, fabs(got[k] - want[k]));
                    }
        }
    }

    // Affine o affine is exact EVERYWHERE (both stages affine, second's
    // domain covers first's output range so nothing clamps), and the result
    // keeps first's domain.
    {
        CubeLUT a, b, out;
        a.Create(4);
        T15Interp::FillAffine(a);
        b.Create(3);
        double bmin[3] = { -1.0, -1.0, -1.0 }, bmax[3] = { 2.0, 2.0, 2.0 };
        if (!b.SetDomain(bmin, bmax))
            Fail("T16 compose: SetDomain for affine B failed");
        FillAffineB(b);
        std::string err;
        if (!ComposeCube(a, b, 7, out, &err))
            Fail("T16 compose: affine compose refused: %s", err.c_str());
        else
        {
            double dmin[3], dmax[3];
            out.GetDomain(dmin, dmax);
            for (int k = 0; k < 3; ++k)
                if (dmin[k] != 0.0 || dmax[k] != 1.0)
                    Fail("T16 compose: result domain not first's (comp %d: %g..%g)",
                         k, dmin[k], dmax[k]);
            Lcg g(11223u);
            for (int i = 0; i < 200; ++i)
            {
                double x[3] = { g.Next01(), g.Next01(), g.Next01() };
                double mid[3], want[3], got[3];
                T15Interp::Affine(x, mid);
                AffineB(mid, want);
                if (!out.Evaluate(x, got))
                    { Fail("T16 compose: affine result Evaluate failed"); break; }
                for (int k = 0; k < 3; ++k)
                    if (fabs(got[k] - want[k]) > 1e-11)
                        Fail("T16 compose: affine probe %d comp %d off by %.3g",
                             i, k, fabs(got[k] - want[k]));
            }
        }
    }

    // Separable-gamma first stage, affine second, result sampled on the
    // SAME node set as the first stage: exact everywhere against the
    // evaluator chain (per cell the chain is affine, and the cells align).
    {
        CubeLUT a, b, out;
        a.Create(9);
        FillGamma(a);
        b.Create(3);
        FillAffineB(b);     // gamma outputs stay inside b's default 0..1 domain
        std::string err;
        if (!ComposeCube(a, b, 9, out, &err))
            Fail("T16 compose: aligned compose refused: %s", err.c_str());
        else
        {
            Lcg g(44556u);
            for (int i = 0; i < 200; ++i)
            {
                double x[3] = { g.Next01(), g.Next01(), g.Next01() };
                double mid[3], want[3], got[3];
                if (!a.Evaluate(x, mid) || !b.Evaluate(mid, want)
                    || !out.Evaluate(x, got))
                    { Fail("T16 compose: aligned eval failed"); break; }
                for (int k = 0; k < 3; ++k)
                    if (fabs(got[k] - want[k]) > 1e-11)
                        Fail("T16 compose: aligned probe %d comp %d off by %.3g",
                             i, k, fabs(got[k] - want[k]));
            }
        }
    }

    // Contract gate: incompatible insertion point refuses and leaves the
    // output untouched; compatible one stamps input/output through.
    {
        CubeLUT a, b, out;
        a.Create(2);
        b.Create(2);
        LutContract ca, cb;
        ca.input = Sig(LutSignalType::CS_BT709, LutSignalType::TF_GAMMA22,
                       LutSignalType::RANGE_FULL);
        ca.output = Sig(LutSignalType::CS_BT2020, LutSignalType::TF_LINEAR,
                        LutSignalType::RANGE_FULL);
        cb.input = Sig(LutSignalType::CS_BT709, LutSignalType::TF_UNSPECIFIED,
                       LutSignalType::RANGE_UNSPECIFIED);   // clashes with ca.output
        cb.output = Sig(LutSignalType::CS_P3D65, LutSignalType::TF_PQ,
                        LutSignalType::RANGE_VIDEO);
        a.SetContract(ca);
        b.SetContract(cb);
        MakeSentinel(out);
        std::string err;
        if (ComposeCube(a, b, 4, out, &err))
            Fail("T16 compose: incompatible contracts composed");
        else
        {
            if (err.empty())
                Fail("T16 compose: contract refusal left err empty");
            if (!SentinelIntact(out))
                Fail("T16 compose: contract refusal touched the output");
        }
        cb.input.colorSpace = LutSignalType::CS_BT2020;     // now compatible
        b.SetContract(cb);
        if (!ComposeCube(a, b, 4, out, &err))
            Fail("T16 compose: compatible contracts refused: %s", err.c_str());
        else
        {
            if (!SameSignalType(out.Contract().input, ca.input))
                Fail("T16 compose: result input signal not first's input");
            if (!SameSignalType(out.Contract().output, cb.output))
                Fail("T16 compose: result output signal not second's output");
        }
    }

    // Refusals: bad output size, invalid inputs - output untouched.
    {
        CubeLUT a, b, out;
        a.Create(3);
        b.Create(3);
        MakeSentinel(out);
        std::string err;
        if (ComposeCube(a, b, 1, out, &err) || ComposeCube(a, b, 257, out, &err))
            Fail("T16 compose: out-of-range outSize accepted");
        CubeLUT invalid;
        if (ComposeCube(invalid, b, 4, out, &err))
            Fail("T16 compose: invalid first accepted");
        if (ComposeCube(a, invalid, 4, out, &err))
            Fail("T16 compose: invalid second accepted");
        if (!SentinelIntact(out))
            Fail("T16 compose: a refusal touched the output");
        // err works as nullptr too.
        if (ComposeCube(invalid, b, 4, out, 0))
            Fail("T16 compose: invalid first accepted (null err)");
    }

    // Aliasing: out may be first or second; both match the fresh-output
    // result exactly.
    {
        CubeLUT a, b, fresh;
        a.Create(4);
        T15Cube::FillLattice(a, 97531u);
        b.Create(3);
        T15Cube::FillLattice(b, 86420u);
        std::string err;
        if (!ComposeCube(a, b, 4, fresh, &err))
            Fail("T16 compose: alias reference compose refused: %s", err.c_str());
        CubeLUT aliasA = a;
        if (!ComposeCube(aliasA, b, 4, aliasA, &err))
            Fail("T16 compose: out==first compose refused: %s", err.c_str());
        else if (!SameLattice(aliasA, fresh))
            Fail("T16 compose: out==first result differs");
        CubeLUT aliasB = b;
        if (!ComposeCube(a, aliasB, 4, aliasB, &err))
            Fail("T16 compose: out==second compose refused: %s", err.c_str());
        else if (!SameLattice(aliasB, fresh))
            Fail("T16 compose: out==second result differs");
    }

    //---------------------------------------------------------------- ResampleCube
    {
        // Same-size resample reproduces every entry exactly (node
        // exactness), keeps domain and contract, empties the title.
        CubeLUT src, out;
        src.Create(4);
        T15Cube::FillLattice(src, 55555u);
        double smin[3] = { -0.5, 0.0, 0.25 }, smax[3] = { 1.5, 2.0, 0.75 };
        if (!src.SetDomain(smin, smax))
            Fail("T16 resample: SetDomain failed");
        LutContract c;
        c.input = Sig(LutSignalType::CS_BT709, LutSignalType::TF_SRGB,
                      LutSignalType::RANGE_FULL);
        c.output = Sig(LutSignalType::CS_BT709, LutSignalType::TF_LINEAR,
                       LutSignalType::RANGE_FULL);
        src.SetContract(c);
        src.SetTitle("source title");
        std::string err = "junk";
        if (!ResampleCube(src, 4, out, &err))
            Fail("T16 resample: same-size resample refused: %s", err.c_str());
        else
        {
            if (!err.empty())
                Fail("T16 resample: success did not clear err");
            if (!SameLattice(out, src))
                Fail("T16 resample: same-size resample changed entries");
            double dmin[3], dmax[3];
            out.GetDomain(dmin, dmax);
            for (int k = 0; k < 3; ++k)
                if (dmin[k] != smin[k] || dmax[k] != smax[k])
                    Fail("T16 resample: domain not kept (comp %d)", k);
            if (!SameSignalType(out.Contract().input, c.input)
                || !SameSignalType(out.Contract().output, c.output))
                Fail("T16 resample: contract not kept");
            if (!out.Title().empty())
                Fail("T16 resample: result title not empty");
        }

        // Upsampling an affine lattice is exact against the analytic map.
        CubeLUT aff, up;
        aff.Create(5);
        T15Interp::FillAffine(aff);
        if (!ResampleCube(aff, 9, up, &err))
            Fail("T16 resample: upsample refused: %s", err.c_str());
        else
        {
            Lcg g(31313u);
            for (int i = 0; i < 200; ++i)
            {
                double x[3] = { g.Next01(), g.Next01(), g.Next01() };
                double want[3], got[3];
                T15Interp::Affine(x, want);
                if (!up.Evaluate(x, got))
                    { Fail("T16 resample: upsample Evaluate failed"); break; }
                for (int k = 0; k < 3; ++k)
                    if (fabs(got[k] - want[k]) > 1e-12)
                        Fail("T16 resample: upsample probe %d comp %d off by %.3g",
                             i, k, fabs(got[k] - want[k]));
            }
        }

        // Refusals leave the output untouched; aliasing works.
        CubeLUT sent;
        MakeSentinel(sent);
        CubeLUT invalid;
        if (ResampleCube(invalid, 4, sent, &err) || ResampleCube(src, 1, sent, &err)
            || ResampleCube(src, 257, sent, &err))
            Fail("T16 resample: a refusal succeeded");
        if (!SentinelIntact(sent))
            Fail("T16 resample: a refusal touched the output");
        CubeLUT aliased = src;
        if (!ResampleCube(aliased, 4, aliased, &err))
            Fail("T16 resample: in-place resample refused: %s", err.c_str());
        else if (!SameLattice(aliased, src))
            Fail("T16 resample: in-place resample changed entries");
    }

    //---------------------------------------------------------------- extraction
    {
        // The shaper is EXACTLY the lattice diagonal, over the LUT's domain.
        CubeLUT lut;
        lut.Create(4);
        T15Cube::FillLattice(lut, 20406u);
        double dmin[3] = { -0.5, 0.0, 0.25 }, dmax[3] = { 1.5, 1.0, 0.75 };
        if (!lut.SetDomain(dmin, dmax))
            Fail("T16 extract: SetDomain failed");
        double diag[4][3];
        for (int i = 0; i < 4; ++i)
        {
            diag[i][0] = 0.1 + 0.3 * i;
            diag[i][1] = -0.2 + 0.5 * i;
            diag[i][2] = 0.05 + 0.25 * i;
            if (!lut.SetEntry(i, i, i, diag[i]))
                Fail("T16 extract: diagonal SetEntry failed");
        }
        ShaperCurve s;
        std::string err = "junk";
        if (!ExtractNeutralShaper(lut, s, &err))
            Fail("T16 extract: extraction refused: %s", err.c_str());
        else
        {
            if (!err.empty())
                Fail("T16 extract: success did not clear err");
            if (!s.IsValid())
                Fail("T16 extract: extracted shaper not valid");
            for (int c = 0; c < 3; ++c)
            {
                if (s.Count(c) != 4)
                    Fail("T16 extract: channel %d count %d want 4", c, s.Count(c));
                double lo, hi;
                if (!s.GetDomain(c, lo, hi) || lo != dmin[c] || hi != dmax[c])
                    Fail("T16 extract: channel %d domain wrong", c);
                for (int i = 0; i < 4; ++i)
                {
                    double v;
                    if (!s.GetSample(c, i, v) || v != diag[i][c])
                        Fail("T16 extract: channel %d sample %d not the diagonal entry",
                             c, i);
                }
            }
        }

        // A non-monotone diagonal refuses and leaves an existing shaper
        // untouched.
        double dip[3] = { diag[2][0], diag[1][1] - 0.01, diag[2][2] };
        if (!lut.SetEntry(2, 2, 2, dip))
            Fail("T16 extract: dip SetEntry failed");
        err.clear();
        if (ExtractNeutralShaper(lut, s, &err))
            Fail("T16 extract: non-monotone green diagonal accepted");
        else
        {
            if (err.empty())
                Fail("T16 extract: refusal left err empty");
            double v;
            if (!s.IsValid() || !s.GetSample(1, 2, v) || v != diag[2][1])
                Fail("T16 extract: refusal touched the existing shaper");
        }
        CubeLUT invalid;
        if (ExtractNeutralShaper(invalid, s, &err))
            Fail("T16 extract: invalid LUT accepted");
    }

    //---------------------------------------------------------------- split
    // A purely per-channel source: the whole neutral response moves into
    // the shaper and the residual cube collapses to the identity lattice
    // over the shaper's value range (here 0..1, since the gammas fix the
    // endpoints).
    {
        CubeLUT src;
        src.Create(9);
        FillGamma(src);
        ShaperCurve s;
        CubeLUT cube;
        std::string err;
        if (!SplitShaperCube(src, 7, s, cube, &err))
            Fail("T16 split: separable split refused: %s", err.c_str());
        else
        {
            for (int c = 0; c < 3; ++c)
            {
                if (s.Count(c) != 9)
                    Fail("T16 split: shaper channel %d count %d want 9", c, s.Count(c));
                for (int i = 0; i < 9; ++i)
                {
                    double v, want = pow(i / 8.0, kGamma[c]);
                    if (!s.GetSample(c, i, v) || fabs(v - want) > 1e-15)
                        Fail("T16 split: shaper channel %d sample %d off", c, i);
                }
            }
            double dmin[3], dmax[3];
            cube.GetDomain(dmin, dmax);
            for (int c = 0; c < 3; ++c)
                if (fabs(dmin[c] - 0.0) > 0.0 || fabs(dmax[c] - 1.0) > 0.0)
                    Fail("T16 split: separable cube domain comp %d = %g..%g want 0..1",
                         c, dmin[c], dmax[c]);
            if (cube.Size() != 7)
                Fail("T16 split: cube size %d want 7", cube.Size());
            for (int bb = 0; bb < 7; ++bb)
                for (int gg = 0; gg < 7; ++gg)
                    for (int rr = 0; rr < 7; ++rr)
                    {
                        double want[3] = { rr / 6.0, gg / 6.0, bb / 6.0 };
                        double got[3];
                        if (!cube.GetEntry(rr, gg, bb, got))
                            { Fail("T16 split: identity GetEntry failed"); continue; }
                        for (int k = 0; k < 3; ++k)
                            if (fabs(got[k] - want[k]) > 1e-12)
                                Fail("T16 split: residual cube not identity at "
                                     "(%d,%d,%d) comp %d: off by %.3g",
                                     rr, gg, bb, k, fabs(got[k] - want[k]));
                    }
        }
    }

    // Affine source: shaper and residual are both exact, so the
    // recomposition cube(shaper(x)) reproduces src(x) everywhere,
    // including far off the neutral axis.
    {
        CubeLUT src;
        src.Create(5);
        FillAffineB(src);
        LutContract c;
        c.input = Sig(LutSignalType::CS_BT709, LutSignalType::TF_GAMMA22,
                      LutSignalType::RANGE_FULL);
        c.output = Sig(LutSignalType::CS_BT2020, LutSignalType::TF_PQ,
                       LutSignalType::RANGE_VIDEO);
        src.SetContract(c);
        ShaperCurve s;
        CubeLUT cube;
        std::string err;
        if (!SplitShaperCube(src, 4, s, cube, &err))
            Fail("T16 split: affine split refused: %s", err.c_str());
        else
        {
            // Cube domain = the shaper's value range, exactly.
            double dmin[3], dmax[3];
            cube.GetDomain(dmin, dmax);
            for (int ch = 0; ch < 3; ++ch)
            {
                double lo, hi;
                if (!s.GetSample(ch, 0, lo) || !s.GetSample(ch, s.Count(ch) - 1, hi))
                    { Fail("T16 split: shaper range read failed"); continue; }
                if (dmin[ch] != lo || dmax[ch] != hi)
                    Fail("T16 split: cube domain comp %d = %.17g..%.17g want "
                         "%.17g..%.17g", ch, dmin[ch], dmax[ch], lo, hi);
            }
            // Contracts: unspecified input, source's output.
            if (!SameSignalType(cube.Contract().input, LutSignalType()))
                Fail("T16 split: cube input signal not unspecified");
            if (!SameSignalType(cube.Contract().output, c.output))
                Fail("T16 split: cube output signal not source's output");
            // Recomposition.
            Lcg g(77441u);
            for (int i = 0; i < 200; ++i)
            {
                double x[3] = { g.Next01(), g.Next01(), g.Next01() };
                double mid[3], got[3], want[3];
                AffineB(x, want);
                if (!s.Evaluate(x, mid) || !cube.Evaluate(mid, got))
                    { Fail("T16 split: recomposition eval failed"); break; }
                for (int k = 0; k < 3; ++k)
                    if (fabs(got[k] - want[k]) > 1e-10)
                        Fail("T16 split: recomposition probe %d comp %d off by %.3g",
                             i, k, fabs(got[k] - want[k]));
            }
        }
    }

    // General (non-separable) source: every residual-cube entry IS
    // src(shaperInverse(node)) through the public evaluators - the node
    // definition of the split.
    {
        CubeLUT src;
        src.Create(7);
        FillMixedGamma(src);
        ShaperCurve s;
        CubeLUT cube;
        std::string err;
        if (!SplitShaperCube(src, 5, s, cube, &err))
            Fail("T16 split: mixed split refused: %s", err.c_str());
        else
        {
            double dmin[3], dmax[3];
            cube.GetDomain(dmin, dmax);
            for (int bb = 0; bb < 5; ++bb)
                for (int gg = 0; gg < 5; ++gg)
                    for (int rr = 0; rr < 5; ++rr)
                    {
                        double y[3] = {
                            dmin[0] + rr / 4.0 * (dmax[0] - dmin[0]),
                            dmin[1] + gg / 4.0 * (dmax[1] - dmin[1]),
                            dmin[2] + bb / 4.0 * (dmax[2] - dmin[2]),
                        };
                        double t[3], want[3], got[3];
                        if (!s.EvaluateInverse(y, t) || !src.Evaluate(t, want)
                            || !cube.GetEntry(rr, gg, bb, got))
                            { Fail("T16 split: node-definition eval failed"); continue; }
                        for (int k = 0; k < 3; ++k)
                            if (fabs(got[k] - want[k]) > 1e-13)
                                Fail("T16 split: cube node (%d,%d,%d) comp %d off "
                                     "by %.3g", rr, gg, bb, k, fabs(got[k] - want[k]));
                    }
        }
    }

    // Refusals: non-monotone neutral axis, bad cube size, invalid source -
    // both outputs untouched.
    {
        CubeLUT src;
        src.Create(4);
        T15Cube::FillLattice(src, 20406u);
        double hi0[3] = { 0.5, 0.5, 0.5 }, lo1[3] = { 0.1, 0.1, 0.1 };
        if (!src.SetEntry(0, 0, 0, hi0) || !src.SetEntry(1, 1, 1, lo1))
            Fail("T16 split: refusal-test diagonal SetEntry failed");
        ShaperCurve s;
        const double keep[2] = { 0.25, 0.75 };
        for (int c = 0; c < 3; ++c)
            if (!s.SetChannel(c, keep, 2, 0.0, 1.0))
                Fail("T16 split: refusal-test shaper install failed");
        CubeLUT sent;
        MakeSentinel(sent);
        std::string err;
        if (SplitShaperCube(src, 4, s, sent, &err))
            Fail("T16 split: non-monotone source accepted");
        else
        {
            if (err.empty())
                Fail("T16 split: refusal left err empty");
            double v;
            if (!s.IsValid() || s.Count(0) != 2 || !s.GetSample(0, 0, v) || v != 0.25)
                Fail("T16 split: refusal touched the shaper");
            if (!SentinelIntact(sent))
                Fail("T16 split: refusal touched the cube");
        }
        CubeLUT good;
        good.Create(3);         // identity: monotone diagonal
        if (SplitShaperCube(good, 1, s, sent, &err)
            || SplitShaperCube(good, 257, s, sent, &err))
            Fail("T16 split: out-of-range cubeSize accepted");
        CubeLUT invalid;
        if (SplitShaperCube(invalid, 4, s, sent, &err))
            Fail("T16 split: invalid source accepted");
        if (!SentinelIntact(sent))
            Fail("T16 split: a size/validity refusal touched the cube");
    }

    // Determinism: composing twice gives bit-identical lattices.
    {
        CubeLUT a, b, o1, o2;
        a.Create(4);
        T15Cube::FillLattice(a, 31415u);
        b.Create(4);
        T15Cube::FillLattice(b, 27182u);
        std::string err;
        if (!ComposeCube(a, b, 5, o1, &err) || !ComposeCube(a, b, 5, o2, &err))
            Fail("T16 determinism: compose failed: %s", err.c_str());
        else if (!SameLattice(o1, o2))
            Fail("T16 determinism: repeated compose differs");
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
    RunT15();   // T11-T14 reserved for in-flight branches; see the header comment
    RunT15Interp();
    RunT16();

    if (g_failures)
    {
        printf("\n%d FAILURE(S)\n", g_failures);
        return 1;
    }
    printf("\nALL TESTS PASSED (%s mode)\n", g_genMode ? "gen" : "verify");
    return 0;
}
