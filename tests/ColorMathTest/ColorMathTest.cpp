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
// T12 asserts the BT.2390 inverse LUT round-trips and never faults on an empty table,
// T19 asserts the #193 archive extension trailer: round trip, unknown-tag skip,
// legacy bytes, rejections, the primitives, append-only values, and the legacy store,
// and T22 asserts the test-color window paints each patch's own code.
// The .chc round-trip originally planned for the T7
// slot needs app-level linkage and is deliberately still not in this console harness.

#include <afx.h>
#include "../../libHCFR/Color.h"
#include "../../libHCFR/ArchiveExtension.h"
#include "../../libHCFR/ArchiveTags.h"
#include "../../libHCFR/SpectralCorrection.h"
#include <limits>

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
// T22 - test-color window oracle (pure assertion, no golden file): the code
// the window paints for a patch (TestWindowShownCode of TestWindowPickerCode)
// must be the patch's own 8-bit code in the range, the code the references
// snap to. The picker holds full-range codes and the window maps them to
// 16-235 for a limited generator; quantizing in full range first is a code
// off for ~1 in 5 levels.
//////////////////////////////////////////////////////////////////////////
static void RunT22()
{
    printf("T22 test-color window oracle...\n");
    for (int r = 0; r < 2; ++r)
    {
        bool lim = (r != 0);
        for (int i = -50000; i <= 1050000; ++i)
        {
            double pct = i * 1e-4;
            int want = ColorRGBDisplay::ConvertPercentToCode(pct, lim, 8);
            if (lim)
                want = want < 16 ? 16 : (want > 235 ? 235 : want);
            int picker = ColorRGBDisplay::TestWindowPickerCode(pct, lim);
            if (picker < 0 || picker > 255)
                Fail("T22 pct=%.17g range=%d: picker code %d out of 0..255", pct, r, picker);
            int shown = ColorRGBDisplay::TestWindowShownCode(picker, lim);
            if (shown != want)
                Fail("T22 pct=%.17g range=%d: window shows %d, patch code %d", pct, r, shown, want);
        }
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
//    table: a valx of 0 tripped the `valx == 0` PQ early return and
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
// T19 - archive extension trailer oracle (issue #193).
//
// Every versioned block in a .chc / .thc stream now ends with a length-
// prefixed, tagged extension region (libHCFR/ArchiveExtension.h). CColor is
// the one block that lives in libHCFR, so it is the block this console
// harness can pin end to end:
//   (a) a color carrying every optional field round-trips exactly, and the
//       stream's version is base + 100;
//   (b) a trailer with an unknown tag is skipped by byte count and the NEXT
//       color in the stream still decodes - the whole point of the design;
//   (c) legacy streams captured from the pre-trailer writer (bases 1/3/5/6,
//       with the two 92-byte dead "ignore" matrices) still load, and so do
//       hand-built base-2/4 streams (int bandwidth) which the writer never
//       emitted but real old documents contain;
//   (d) Bodner-era tens-digit versions (10..99) and versions from a newer
//       build (>= 200) are rejected as CNewerSchemaException, bases outside
//       1..6 as badSchema, and a truncated or malformed trailer throws
//       without changing the target color at all (it is loaded into a
//       temporary and assigned only on success);
//   (e) the writer and reader primitives themselves: ordering, the empty
//       trailer, exact value extents, and discard-on-Close;
//   (f) append-only within a tag: a known value longer than this build
//       reads is consumed whole and the next tag still decodes; a color
//       without a TIMESTAMP tag has none;
//   (g) the legacy store (CColorLegacyStoreScope, used by the
//       SaveOldCalibrationFile writer) emits exactly the (c) goldens, so a
//       file written for 4.1 is readable there without a 4.1 build.
// Pure assertion; no golden file. All expected bytes are pinned inline.
//////////////////////////////////////////////////////////////////////////
namespace T19Ext
{
    typedef std::vector<unsigned char> Bytes;

    // --- little-endian builders, matching what CArchive writes ---
    static void PutRaw(Bytes& b, const void* p, size_t n)
    {
        const unsigned char* q = (const unsigned char*)p;
        b.insert(b.end(), q, q + n);
    }
    static void PutInt(Bytes& b, int v)      { PutRaw(b, &v, 4); }
    static void PutDword(Bytes& b, DWORD v)  { PutRaw(b, &v, 4); }
    static void PutByte(Bytes& b, BYTE v)    { b.push_back(v); }
    static void PutDouble(Bytes& b, double v){ PutRaw(b, &v, 8); }

    // Matrix::Serialize layout for a 3x1: two headers, version 1, columns, rows,
    // then the elements by column (44 bytes).
    static void PutMatrix3x1(Bytes& b, double x, double y, double z)
    {
        PutInt(b, 0x434d6174);
        PutInt(b, 0x72697843);
        PutInt(b, 1);
        PutInt(b, 1);
        PutInt(b, 3);
        PutDouble(b, x);
        PutDouble(b, y);
        PutDouble(b, z);
    }

    struct Entry
    {
        BYTE  tag;
        Bytes value;
    };

    // One CColor at version 101 (base 1 + trailer): version, XYZ, then the
    // trailer holding `entries` in the order given.
    static void PutColor101(Bytes& b, double x, double y, double z, const std::vector<Entry>& entries)
    {
        PutInt(b, 101);
        PutMatrix3x1(b, x, y, z);
        DWORD cb = 0;
        for (size_t i = 0; i < entries.size(); ++i)
            cb += 5 + (DWORD)entries[i].value.size();
        PutDword(b, cb);
        for (size_t i = 0; i < entries.size(); ++i)
        {
            PutByte(b, entries[i].tag);
            PutDword(b, (DWORD)entries[i].value.size());
            PutRaw(b, entries[i].value.empty() ? NULL : &entries[i].value[0], entries[i].value.size());
        }
    }

    static Bytes Store(CColor& c)
    {
        CMemFile mem;
        {
            CArchive ar(&mem, CArchive::store);
            c.Serialize(ar);
            ar.Flush();
            ar.Close();
        }
        size_t n = (size_t)mem.GetLength();
        BYTE* p = mem.Detach();
        Bytes out(p, p + n);
        free(p);
        return out;
    }

    // Loads `count` colors back to back from one stream. Returns the exception
    // that stopped the load (caller must Delete it), or NULL when all loaded.
    static CException* Load(const Bytes& b, CColor* colors, int count)
    {
        size_t n = b.size();
        BYTE* buf = (BYTE*)malloc(n ? n : 1);
        if (n)
            memcpy(buf, &b[0], n);
        CMemFile mem;
        mem.Attach(buf, (UINT)n);
        CException* caught = NULL;
        {
            CArchive ar(&mem, CArchive::load);
            try
            {
                for (int i = 0; i < count; ++i)
                    colors[i].Serialize(ar);
            }
            catch (CException* e)
            {
                caught = e;
                ar.Abort();
            }
        }
        mem.Detach();
        free(buf);
        return caught;
    }

    static void ExpectXYZ(const char* what, const CColor& c, double x, double y, double z)
    {
        if (c.GetX() != x || c.GetY() != y || c.GetZ() != z)
            Fail("T19 %s: XYZ = (%.17g, %.17g, %.17g), expected (%.17g, %.17g, %.17g)",
                 what, c.GetX(), c.GetY(), c.GetZ(), x, y, z);
    }

    static void ExpectNoOptional(const char* what, const CColor& c)
    {
        if (c.HasSpectrum())      Fail("T19 %s: unexpected spectrum", what);
        if (c.HasLuxValue())      Fail("T19 %s: unexpected lux value", what);
        if (c.HasRawXYZValue())   Fail("T19 %s: unexpected raw XYZ", what);
        if (c.HasStimulusValue()) Fail("T19 %s: unexpected stimulus", what);
        if (c.HasTimestamp())     Fail("T19 %s: unexpected timestamp", what);
    }

    static void ExpectSpectrum(const char* what, const CColor& c, int rows, int wlMin, int wlMax, double bw, const double* samples)
    {
        if (!c.HasSpectrum())
        {
            Fail("T19 %s: spectrum missing", what);
            return;
        }
        CSpectrum s = c.GetSpectrum();
        if (s.GetRows() != rows || s.m_WaveLengthMin != wlMin || s.m_WaveLengthMax != wlMax || s.m_BandWidth != bw)
            Fail("T19 %s: spectrum header %d/%d/%d/%.17g, expected %d/%d/%d/%.17g",
                 what, s.GetRows(), s.m_WaveLengthMin, s.m_WaveLengthMax, s.m_BandWidth, rows, wlMin, wlMax, bw);
        else
            for (int i = 0; i < rows; ++i)
                if (s[i] != samples[i])
                    Fail("T19 %s: spectrum[%d] = %.17g, expected %.17g", what, i, s[i], samples[i]);
    }

    static void ExpectLux(const char* what, const CColor& c, double lux)
    {
        if (!c.HasLuxValue())
            Fail("T19 %s: lux missing", what);
        else if (c.GetLuxValue() != lux)
            Fail("T19 %s: lux = %.17g, expected %.17g", what, c.GetLuxValue(), lux);
    }

    static void ExpectRaw(const char* what, const CColor& c, double x, double y, double z)
    {
        if (!c.HasRawXYZValue())
        {
            Fail("T19 %s: raw XYZ missing", what);
            return;
        }
        ColorXYZ r = c.GetRawXYZValue();
        if (r[0] != x || r[1] != y || r[2] != z)
            Fail("T19 %s: raw XYZ = (%.17g, %.17g, %.17g), expected (%.17g, %.17g, %.17g)",
                 what, r[0], r[1], r[2], x, y, z);
    }

    static void ExpectStimulus(const char* what, const CColor& c, double r, double g, double b)
    {
        if (!c.HasStimulusValue())
        {
            Fail("T19 %s: stimulus missing", what);
            return;
        }
        ColorRGBDisplay s = c.GetStimulusValue();
        if (s[0] != r || s[1] != g || s[2] != b)
            Fail("T19 %s: stimulus = (%.17g, %.17g, %.17g), expected (%.17g, %.17g, %.17g)",
                 what, s[0], s[1], s[2], r, g, b);
    }

    // Bit-exact: the timestamp is a double on the wire, so any rounding (a
    // float, whole seconds) is a defect, not noise.
    static void ExpectTimestamp(const char* what, const CColor& c, double t)
    {
        if (!c.HasTimestamp())
        {
            Fail("T19 %s: timestamp missing", what);
            return;
        }
        double got = c.GetTimestamp();
        if (memcmp(&got, &t, sizeof t) != 0)
            Fail("T19 %s: timestamp = %.17g, expected %.17g bit-exact", what, got, t);
    }

    static void ExpectBytes(const char* what, const Bytes& got, const unsigned char* want, size_t n)
    {
        if (got.size() != n)
        {
            Fail("T19 %s: %u bytes, expected %u", what, (unsigned)got.size(), (unsigned)n);
            return;
        }
        for (size_t i = 0; i < n; ++i)
            if (got[i] != want[i])
            {
                Fail("T19 %s: byte %u = 0x%02X, expected 0x%02X", what, (unsigned)i, got[i], want[i]);
                return;
            }
    }

    // Reports and frees an exception the caller did not expect. Named by
    // runtime class and archive cause, not GetErrorMessage: that loads an
    // MFC string resource this console harness does not carry, and the
    // missing string is an ASSERT (abort) in a Debug build.
    static void FailUnexpected(const char* what, CException* e)
    {
        int cause = -1;
        if (e->IsKindOf(RUNTIME_CLASS(CArchiveException)))
            cause = ((CArchiveException*)e)->m_cause;
        Fail("T19 %s: unexpected exception %s (archive cause %d)", what,
             e->GetRuntimeClass()->m_lpszClassName, cause);
        e->Delete();
    }

    static const double kSamples[3] = { 0.1, 0.2, 0.3 };
    static const double kTimestamp = 1789000000.123456;

    //////////////////////////////////////////////////////////////////////
    // (c) Legacy streams, captured 2026-09-05 from the pre-trailer
    // CColor::Serialize on the stand-in base (Bodner 6326187c + #191),
    // inputs XYZ = (0.25, 0.5, 0.75), lux = 123.456, spectrum
    // CSpectrum(3, 380, 780, 200.0) with samples (0.1, 0.2, 0.3).
    // Layout: int base; XYZ (44); two 3x3 "ignore" matrices (92 each);
    // [spectrum: rows, min, max, double bandwidth, Matrix (44)]; [double lux].
    //////////////////////////////////////////////////////////////////////
    static const unsigned char kGoldenBase1[232] = {
        0x01,0x00,0x00,0x00,0x74,0x61,0x4D,0x43,0x43,0x78,0x69,0x72,0x01,0x00,0x00,0x00,
        0x01,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xD0,0x3F,
        0x00,0x00,0x00,0x00,0x00,0x00,0xE0,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0xE8,0x3F,
        0x74,0x61,0x4D,0x43,0x43,0x78,0x69,0x72,0x01,0x00,0x00,0x00,0x03,0x00,0x00,0x00,
        0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x74,0x61,0x4D,0x43,
        0x43,0x78,0x69,0x72,0x01,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x03,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    };
    static const unsigned char kGoldenBase3[240] = {
        0x03,0x00,0x00,0x00,0x74,0x61,0x4D,0x43,0x43,0x78,0x69,0x72,0x01,0x00,0x00,0x00,
        0x01,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xD0,0x3F,
        0x00,0x00,0x00,0x00,0x00,0x00,0xE0,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0xE8,0x3F,
        0x74,0x61,0x4D,0x43,0x43,0x78,0x69,0x72,0x01,0x00,0x00,0x00,0x03,0x00,0x00,0x00,
        0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x74,0x61,0x4D,0x43,
        0x43,0x78,0x69,0x72,0x01,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x03,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x77,0xBE,0x9F,0x1A,0x2F,0xDD,0x5E,0x40,
    };
    static const unsigned char kGoldenBase5[300] = {
        0x05,0x00,0x00,0x00,0x74,0x61,0x4D,0x43,0x43,0x78,0x69,0x72,0x01,0x00,0x00,0x00,
        0x01,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xD0,0x3F,
        0x00,0x00,0x00,0x00,0x00,0x00,0xE0,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0xE8,0x3F,
        0x74,0x61,0x4D,0x43,0x43,0x78,0x69,0x72,0x01,0x00,0x00,0x00,0x03,0x00,0x00,0x00,
        0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x74,0x61,0x4D,0x43,
        0x43,0x78,0x69,0x72,0x01,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x03,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x7C,0x01,0x00,0x00,
        0x0C,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x69,0x40,0x01,0x00,0x00,0x00,
        0x74,0x61,0x4D,0x43,0x43,0x78,0x69,0x72,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,
        0x03,0x00,0x00,0x00,0x9A,0x99,0x99,0x99,0x99,0x99,0xB9,0x3F,0x9A,0x99,0x99,0x99,
        0x99,0x99,0xC9,0x3F,0x33,0x33,0x33,0x33,0x33,0x33,0xD3,0x3F,
    };
    static const unsigned char kGoldenBase6[308] = {
        0x06,0x00,0x00,0x00,0x74,0x61,0x4D,0x43,0x43,0x78,0x69,0x72,0x01,0x00,0x00,0x00,
        0x01,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xD0,0x3F,
        0x00,0x00,0x00,0x00,0x00,0x00,0xE0,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0xE8,0x3F,
        0x74,0x61,0x4D,0x43,0x43,0x78,0x69,0x72,0x01,0x00,0x00,0x00,0x03,0x00,0x00,0x00,
        0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x74,0x61,0x4D,0x43,
        0x43,0x78,0x69,0x72,0x01,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x03,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x7C,0x01,0x00,0x00,
        0x0C,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x69,0x40,0x01,0x00,0x00,0x00,
        0x74,0x61,0x4D,0x43,0x43,0x78,0x69,0x72,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,
        0x03,0x00,0x00,0x00,0x9A,0x99,0x99,0x99,0x99,0x99,0xB9,0x3F,0x9A,0x99,0x99,0x99,
        0x99,0x99,0xC9,0x3F,0x33,0x33,0x33,0x33,0x33,0x33,0xD3,0x3F,0x77,0xBE,0x9F,0x1A,
        0x2F,0xDD,0x5E,0x40,
    };

    // Offsets inside the base-5/6 goldens: the spectrum header starts right
    // after the 232-byte legacy prefix, and its double bandwidth sits after
    // rows/min/max.
    static const size_t kLegacyPrefix = 4 + 44 + 92 + 92;
    static const size_t kBandwidthOff = kLegacyPrefix + 12;

    // A base-2 (from base 5) or base-4 (from base 6) stream: the same layout
    // with the bandwidth stored as an int, which is what old documents hold.
    static Bytes MakeIntBandwidth(const unsigned char* golden, size_t n, int newBase)
    {
        Bytes b(golden, golden + kBandwidthOff);
        b[0] = (unsigned char)newBase;
        PutInt(b, 200);
        b.insert(b.end(), golden + kBandwidthOff + 8, golden + n);
        return b;
    }

    static void CheckRoundTrip()
    {
        CColor src(0.25, 0.5, 0.75);
        double samples[3] = { 0.1, 0.2, 0.3 };
        CSpectrum spec(3, 380, 780, 200.0, samples);
        src.SetSpectrum(spec);
        src.SetLuxValue(123.456);
        src.SetRawXYZValue(ColorXYZ(0.2, 0.4, 0.6));
        src.SetStimulusValue(ColorRGBDisplay(10.0, 20.0, 30.0));
        src.SetTimestamp(kTimestamp);

        Bytes b = Store(src);
        if (b.size() < 4)
        {
            Fail("T19 (a): stored %u bytes", (unsigned)b.size());
            return;
        }
        int version;
        memcpy(&version, &b[0], 4);
        if (version != 106)
            Fail("T19 (a): stream version %d, expected 106 (base 6 + 100)", version);

        CColor dst;
        CException* e = Load(b, &dst, 1);
        if (e)
        {
            FailUnexpected("(a) round trip", e);
            return;
        }
        ExpectXYZ("(a)", dst, 0.25, 0.5, 0.75);
        ExpectSpectrum("(a)", dst, 3, 380, 780, 200.0, kSamples);
        ExpectLux("(a)", dst, 123.456);
        ExpectRaw("(a)", dst, 0.2, 0.4, 0.6);
        ExpectStimulus("(a)", dst, 10.0, 20.0, 30.0);
        ExpectTimestamp("(a)", dst, kTimestamp);

        // The base-only color (no optional field) must carry an empty trailer,
        // never no trailer: pin its exact bytes so a "write it only when
        // non-empty" regression shows here, not only in (b).
        CColor plain(0.25, 0.5, 0.75);
        Bytes p = Store(plain);
        Bytes want;
        PutColor101(want, 0.25, 0.5, 0.75, std::vector<Entry>());
        ExpectBytes("(a) base-only stream", p, want.empty() ? NULL : &want[0], want.size());
    }

    static void CheckForwardCompat()
    {
        Entry unknown;
        unknown.tag = 200;
        unknown.value.assign(37, 0xA5);
        Entry raw;
        raw.tag = 1;
        PutMatrix3x1(raw.value, 0.2, 0.4, 0.6);

        std::vector<Entry> second;
        second.push_back(raw);

        // Three shapes for the first color's trailer: unknown only, known then
        // unknown, unknown then known. The second color is always the same.
        for (int shape = 0; shape < 3; ++shape)
        {
            std::vector<Entry> first;
            if (shape == 0)      { first.push_back(unknown); }
            else if (shape == 1) { first.push_back(raw); first.push_back(unknown); }
            else                 { first.push_back(unknown); first.push_back(raw); }

            Bytes b;
            PutColor101(b, 0.25, 0.5, 0.75, first);
            PutColor101(b, 0.11, 0.22, 0.33, second);

            char what[64];
            sprintf(what, "(b) shape %d", shape);

            CColor c[2];
            CException* e = Load(b, c, 2);
            if (e)
            {
                FailUnexpected(what, e);
                continue;
            }
            ExpectXYZ(what, c[0], 0.25, 0.5, 0.75);
            if (shape == 0)
                ExpectNoOptional(what, c[0]);
            else
                ExpectRaw(what, c[0], 0.2, 0.4, 0.6);
            if (c[0].HasStimulusValue() || c[0].HasSpectrum() || c[0].HasLuxValue())
                Fail("T19 %s: first color grew a field it was never given", what);

            ExpectXYZ(what, c[1], 0.11, 0.22, 0.33);
            ExpectRaw(what, c[1], 0.2, 0.4, 0.6);
            if (c[1].HasStimulusValue() || c[1].HasSpectrum() || c[1].HasLuxValue())
                Fail("T19 %s: second color grew a field it was never given", what);
        }
    }

    static void CheckLegacy()
    {
        struct Case
        {
            const char*          name;
            const unsigned char* bytes;
            size_t               size;
            bool                 spectrum;
            bool                 lux;
        };
        static const Case cases[] = {
            { "(c) base 1", kGoldenBase1, sizeof kGoldenBase1, false, false },
            { "(c) base 3", kGoldenBase3, sizeof kGoldenBase3, false, true  },
            { "(c) base 5", kGoldenBase5, sizeof kGoldenBase5, true,  false },
            { "(c) base 6", kGoldenBase6, sizeof kGoldenBase6, true,  true  },
        };
        for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i)
        {
            Bytes b(cases[i].bytes, cases[i].bytes + cases[i].size);
            CColor c;
            CException* e = Load(b, &c, 1);
            if (e)
            {
                FailUnexpected(cases[i].name, e);
                continue;
            }
            ExpectXYZ(cases[i].name, c, 0.25, 0.5, 0.75);
            if (cases[i].spectrum)
                ExpectSpectrum(cases[i].name, c, 3, 380, 780, 200.0, kSamples);
            else if (c.HasSpectrum())
                Fail("T19 %s: unexpected spectrum", cases[i].name);
            if (cases[i].lux)
                ExpectLux(cases[i].name, c, 123.456);
            else if (c.HasLuxValue())
                Fail("T19 %s: unexpected lux", cases[i].name);
            if (c.HasRawXYZValue() || c.HasStimulusValue())
                Fail("T19 %s: legacy stream produced a trailer field", cases[i].name);
        }

        // Bases 2 and 4: int bandwidth. The writer never emitted these, real
        // old documents contain them, and the version gate must be 1..6, not
        // the writer's {1,3,5,6}.
        {
            Bytes b2 = MakeIntBandwidth(kGoldenBase5, sizeof kGoldenBase5, 2);
            CColor c;
            CException* e = Load(b2, &c, 1);
            if (e)
                FailUnexpected("(c) base 2", e);
            else
            {
                ExpectXYZ("(c) base 2", c, 0.25, 0.5, 0.75);
                ExpectSpectrum("(c) base 2", c, 3, 380, 780, 200.0, kSamples);
                if (c.HasLuxValue())
                    Fail("T19 (c) base 2: unexpected lux");
            }
        }
        {
            Bytes b4 = MakeIntBandwidth(kGoldenBase6, sizeof kGoldenBase6, 4);
            CColor c;
            CException* e = Load(b4, &c, 1);
            if (e)
                FailUnexpected("(c) base 4", e);
            else
            {
                ExpectXYZ("(c) base 4", c, 0.25, 0.5, 0.75);
                ExpectSpectrum("(c) base 4", c, 3, 380, 780, 200.0, kSamples);
                ExpectLux("(c) base 4", c, 123.456);
            }
        }
    }

    static void ExpectNewerSchema(const char* what, CException* e, int version, bool unreleased)
    {
        if (!e)
        {
            Fail("T19 %s: loaded without an exception", what);
            return;
        }
        if (!e->IsKindOf(RUNTIME_CLASS(CNewerSchemaException)))
        {
            FailUnexpected(what, e);
            return;
        }
        CNewerSchemaException* n = (CNewerSchemaException*)e;
        if (n->m_block != "CColor")
            Fail("T19 %s: block '%s', expected 'CColor'", what, (LPCSTR)n->m_block);
        if (n->m_version != version)
            Fail("T19 %s: version %d, expected %d", what, n->m_version, version);
        if (n->m_unreleased != unreleased)
            Fail("T19 %s: unreleased = %d, expected %d", what, (int)n->m_unreleased, (int)unreleased);
        if (n->m_cause != CArchiveException::badSchema)
            Fail("T19 %s: m_cause %d, expected badSchema so unaware handlers still see a schema error", what, n->m_cause);
        e->Delete();
    }

    static void ExpectArchiveCause(const char* what, CException* e, int cause)
    {
        if (!e)
        {
            Fail("T19 %s: loaded without an exception", what);
            return;
        }
        if (e->IsKindOf(RUNTIME_CLASS(CNewerSchemaException)))
        {
            Fail("T19 %s: got CNewerSchemaException, expected a plain archive error", what);
            e->Delete();
            return;
        }
        if (!e->IsKindOf(RUNTIME_CLASS(CArchiveException)))
        {
            FailUnexpected(what, e);
            return;
        }
        int got = ((CArchiveException*)e)->m_cause;
        if (got != cause)
            Fail("T19 %s: archive cause %d, expected %d", what, got, cause);
        e->Delete();
    }

    // The target of a refused load: every field differs from what the
    // rejected streams carry (XYZ (0.25, 0.5, 0.75), and a raw XYZ when they
    // get that far), and it holds the optional fields a careless load would
    // reset before reading the trailer.
    static void MakeTarget(CColor& c)
    {
        c = CColor(0.9, 0.8, 0.7);
        c.SetRawXYZValue(ColorXYZ(0.3, 0.2, 0.1));
        c.SetStimulusValue(ColorRGBDisplay(40.0, 50.0, 60.0));
        c.SetLuxValue(77.5);
    }

    static void ExpectTargetIntact(const char* what, const CColor& c)
    {
        ExpectXYZ(what, c, 0.9, 0.8, 0.7);
        ExpectRaw(what, c, 0.3, 0.2, 0.1);
        ExpectStimulus(what, c, 40.0, 50.0, 60.0);
        ExpectLux(what, c, 77.5);
        if (c.HasSpectrum())
            Fail("T19 %s: target grew a spectrum", what);
        if (c.HasTimestamp())
            Fail("T19 %s: target grew a timestamp", what);
    }

    static void CheckRejections()
    {
        // 10 and 99 are the edges of the unreleased tens-digit range.
        static const int unreleased[] = { 10, 13, 46, 99 };
        for (int i = 0; i < 4; ++i)
        {
            Bytes b;
            PutInt(b, unreleased[i]);
            CColor c;
            char what[64];
            sprintf(what, "(d) version %d", unreleased[i]);
            ExpectNewerSchema(what, Load(b, &c, 1), unreleased[i], true);
        }
        // 200 is the first version from a newer build; it must not fall
        // through to the base check and become a plain badSchema.
        static const int newer[] = { 200, 250 };
        for (int i = 0; i < 2; ++i)
        {
            Bytes b;
            PutInt(b, newer[i]);
            CColor c;
            char what[64];
            sprintf(what, "(d) version %d", newer[i]);
            ExpectNewerSchema(what, Load(b, &c, 1), newer[i], false);
        }
        // Bases 0 and 7 at the trailer version: not a known layout, and not
        // a newer one either.
        static const int badBase[] = { 100, 107 };
        for (int i = 0; i < 2; ++i)
        {
            Bytes b;
            PutInt(b, badBase[i]);
            CColor c;
            char what[64];
            sprintf(what, "(d) version %d", badBase[i]);
            ExpectArchiveCause(what, Load(b, &c, 1), CArchiveException::badSchema);
        }

        // Truncated trailer: cbExtension claims 1000 bytes, 10 are present
        // (tag 1, cbValue 44, then 5 bytes of the 44). The stream's XYZ differs
        // from the target's and the target carries raw, stimulus and lux, so
        // "nothing changed" is a real assertion: XYZ is read before the
        // trailer, and a load that reset the optional fields up front would
        // lose them.
        {
            Bytes b;
            PutInt(b, 101);
            PutMatrix3x1(b, 0.25, 0.5, 0.75);
            PutDword(b, 1000);
            PutByte(b, 1);
            PutDword(b, 44);
            for (int i = 0; i < 5; ++i)
                PutByte(b, 0x00);
            CColor c;
            MakeTarget(c);
            ExpectArchiveCause("(d) truncated trailer", Load(b, &c, 1), CArchiveException::endOfFile);
            ExpectTargetIntact("(d) truncated trailer", c);
        }

        // Malformed trailer: an entry that does not fit inside cbExtension is a
        // schema error, distinct from the stream simply ending early.
        {
            Bytes b;
            PutInt(b, 101);
            PutMatrix3x1(b, 0.25, 0.5, 0.75);
            PutDword(b, 7);
            PutByte(b, 1);
            PutDword(b, 44);
            for (int i = 0; i < 2 + 44; ++i)
                PutByte(b, 0x00);
            CColor c;
            MakeTarget(c);
            ExpectArchiveCause("(d) malformed trailer", Load(b, &c, 1), CArchiveException::badSchema);
            ExpectTargetIntact("(d) malformed trailer", c);
        }

        // Malformed known value: a well-formed trailer whose RAW_XYZ value is
        // 7 bytes, too short for the 44-byte matrix. The value is read whole,
        // the matrix runs off its end (endOfFile), and the target is untouched.
        {
            Entry raw;
            raw.tag = (BYTE)ArchiveTag::COLOR_RAW_XYZ;
            for (int i = 0; i < 7; ++i)
                raw.value.push_back(0x00);
            std::vector<Entry> entries;
            entries.push_back(raw);
            Bytes b;
            PutColor101(b, 0.25, 0.5, 0.75, entries);
            CColor c;
            MakeTarget(c);
            ExpectArchiveCause("(d) short RAW_XYZ value", Load(b, &c, 1), CArchiveException::endOfFile);
            ExpectTargetIntact("(d) short RAW_XYZ value", c);
        }

        // A corrupt cbExtension (near 4 GB) is a schema error at once, never
        // an allocation or a long skip toward the end of the file.
        {
            Bytes b;
            PutInt(b, 101);
            PutMatrix3x1(b, 0.25, 0.5, 0.75);
            PutDword(b, 0xFFFFFFF0);
            CColor c;
            MakeTarget(c);
            ExpectArchiveCause("(d) oversized cbExtension", Load(b, &c, 1), CArchiveException::badSchema);
            ExpectTargetIntact("(d) oversized cbExtension", c);
        }
    }

    //////////////////////////////////////////////////////////////////////
    // (f) Append-only within a tag, and the timestamp tag on the wire.
    //////////////////////////////////////////////////////////////////////
    static void CheckAppendOnly()
    {
        Entry raw;
        raw.tag = (BYTE)ArchiveTag::COLOR_RAW_XYZ;
        PutMatrix3x1(raw.value, 0.2, 0.4, 0.6);

        // No TIMESTAMP tag: none on the loaded color.
        {
            std::vector<Entry> entries;
            entries.push_back(raw);
            Bytes b;
            PutColor101(b, 0.25, 0.5, 0.75, entries);
            CColor c;
            CException* e = Load(b, &c, 1);
            if (e)
                FailUnexpected("(f) no timestamp", e);
            else
            {
                ExpectRaw("(f) no timestamp", c, 0.2, 0.4, 0.6);
                if (c.HasTimestamp())
                    Fail("T19 (f) no timestamp: HasTimestamp() is true");
            }
        }

        // TIMESTAMP (tag 5) placed before RAW_XYZ: both decode.
        {
            Entry ts;
            ts.tag = (BYTE)ArchiveTag::COLOR_TIMESTAMP;
            PutDouble(ts.value, kTimestamp);
            std::vector<Entry> entries;
            entries.push_back(ts);
            entries.push_back(raw);
            Bytes b;
            PutColor101(b, 0.25, 0.5, 0.75, entries);
            CColor c;
            CException* e = Load(b, &c, 1);
            if (e)
                FailUnexpected("(f) timestamp then raw", e);
            else
            {
                ExpectTimestamp("(f) timestamp then raw", c, kTimestamp);
                ExpectRaw("(f) timestamp then raw", c, 0.2, 0.4, 0.6);
            }
        }

        // A KNOWN tag whose value is longer than this build reads: RAW_XYZ
        // carries its 44 bytes plus 8 of 0xA5 a newer build appended. The
        // extra bytes are discarded with the value and the STIMULUS tag after
        // it still decodes.
        {
            Entry longRaw = raw;
            for (int i = 0; i < 8; ++i)
                longRaw.value.push_back(0xA5);
            Entry stim;
            stim.tag = (BYTE)ArchiveTag::COLOR_STIMULUS;
            PutMatrix3x1(stim.value, 10.0, 20.0, 30.0);
            std::vector<Entry> entries;
            entries.push_back(longRaw);
            entries.push_back(stim);
            Bytes b;
            PutColor101(b, 0.25, 0.5, 0.75, entries);
            CColor c;
            CException* e = Load(b, &c, 1);
            if (e)
                FailUnexpected("(f) long RAW_XYZ value", e);
            else
            {
                ExpectXYZ("(f) long RAW_XYZ value", c, 0.25, 0.5, 0.75);
                ExpectRaw("(f) long RAW_XYZ value", c, 0.2, 0.4, 0.6);
                ExpectStimulus("(f) long RAW_XYZ value", c, 10.0, 20.0, 30.0);
            }
        }
    }

    //////////////////////////////////////////////////////////////////////
    // (g) The legacy store. The SaveOldCalibrationFile writer stores the
    // colors of a v1 CCalibrationInfo under CColorLegacyStoreScope; the
    // bytes must be exactly what the pre-trailer writer emitted, i.e. the
    // (c) goldens, which is the proof a 4.1 build can read them. Each color
    // also carries raw XYZ, stimulus and a timestamp, which have no legacy
    // encoding: they must be dropped, not leak into the stream.
    //////////////////////////////////////////////////////////////////////
    static void CheckLegacyStore()
    {
        struct Case
        {
            const char*          name;
            const unsigned char* golden;
            size_t               size;
            int                  base;
            bool                 spectrum;
            bool                 lux;
        };
        static const Case cases[] = {
            { "(g) base 1", kGoldenBase1, sizeof kGoldenBase1, 1, false, false },
            { "(g) base 3", kGoldenBase3, sizeof kGoldenBase3, 3, false, true  },
            { "(g) base 5", kGoldenBase5, sizeof kGoldenBase5, 5, true,  false },
            { "(g) base 6", kGoldenBase6, sizeof kGoldenBase6, 6, true,  true  },
        };
        for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i)
        {
            CColor c(0.25, 0.5, 0.75);
            if (cases[i].spectrum)
            {
                double samples[3] = { 0.1, 0.2, 0.3 };
                CSpectrum spec(3, 380, 780, 200.0, samples);
                c.SetSpectrum(spec);
            }
            if (cases[i].lux)
                c.SetLuxValue(123.456);
            c.SetRawXYZValue(ColorXYZ(0.2, 0.4, 0.6));
            c.SetStimulusValue(ColorRGBDisplay(10.0, 20.0, 30.0));
            c.SetTimestamp(kTimestamp);

            Bytes legacy;
            try
            {
                CColorLegacyStoreScope scope;
                legacy = Store(c);
            }
            catch (CException* e)
            {
                FailUnexpected(cases[i].name, e);
                continue;
            }
            ExpectBytes(cases[i].name, legacy, cases[i].golden, cases[i].size);
            if (CColor::s_bStoreLegacy)
                Fail("T19 %s: the scope did not restore s_bStoreLegacy", cases[i].name);

            // The same color stored outside the scope is the current layout:
            // the scope, and only the scope, is what switches.
            Bytes current = Store(c);
            int version = 0;
            if (current.size() >= 4)
                memcpy(&version, &current[0], 4);
            if (version != cases[i].base + 100)
                Fail("T19 %s: stored without the scope as version %d, expected %d",
                     cases[i].name, version, cases[i].base + 100);
        }
    }

    static Bytes WriteTrailer(CExtensionWriter& w)
    {
        CMemFile mem;
        {
            CArchive ar(&mem, CArchive::store);
            w.Write(ar);
            ar.Flush();
            ar.Close();
        }
        size_t n = (size_t)mem.GetLength();
        BYTE* p = mem.Detach();
        Bytes out(p, p + n);
        free(p);
        return out;
    }

    static void CheckPrimitives()
    {
        // Writer: tags begun as 3, 1, 2 come out as 1, 2, 3.
        try
        {
            CExtensionWriter w;
            w.BeginTag(3) << (BYTE)0x33;
            w.EndTag();
            w.BeginTag(1) << (int)0x11111111;
            w.EndTag();
            w.BeginTag(2) << (WORD)0x2222;
            w.EndTag();
            Bytes got = WriteTrailer(w);
            static const unsigned char want[] = {
                22,0,0,0,
                1, 4,0,0,0, 0x11,0x11,0x11,0x11,
                2, 2,0,0,0, 0x22,0x22,
                3, 1,0,0,0, 0x33,
            };
            ExpectBytes("(e) writer ordering", got, want, sizeof want);
        }
        catch (CException* e)
        {
            FailUnexpected("(e) writer ordering", e);
        }

        // Writer: nothing begun -> exactly four zero bytes.
        try
        {
            CExtensionWriter w;
            Bytes got = WriteTrailer(w);
            static const unsigned char want[] = { 0,0,0,0 };
            ExpectBytes("(e) empty writer", got, want, sizeof want);
        }
        catch (CException* e)
        {
            FailUnexpected("(e) empty writer", e);
        }

        // Reader: a value of 7 bytes exposes exactly 7, the eighth read throws,
        // and the entry after it is still reachable. Then an entry never asked
        // for is discarded by Close() so the stream continues where it should.
        {
            Bytes b;
            PutDword(b, 12 + 8);
            PutByte(b, 7);
            PutDword(b, 7);
            for (int i = 1; i <= 7; ++i)
                PutByte(b, (BYTE)i);
            PutByte(b, 9);
            PutDword(b, 3);
            PutByte(b, 0xAA); PutByte(b, 0xBB); PutByte(b, 0xCC);
            PutInt(b, 0x5A5A5A5A);				// the next block's first field

            size_t n = b.size();
            BYTE* buf = (BYTE*)malloc(n);
            memcpy(buf, &b[0], n);
            CMemFile mem;
            mem.Attach(buf, (UINT)n);
            {
                CArchive ar(&mem, CArchive::load);
                try
                {
                    CExtensionReader r(ar);
                    BYTE tag = 0;
                    DWORD cb = 0;
                    if (!r.Next(tag, cb) || tag != 7 || cb != 7)
                        Fail("T19 (e) reader: first entry tag %d cb %u, expected 7/7", tag, (unsigned)cb);
                    else
                    {
                        CArchive& v = r.Value();
                        for (int i = 1; i <= 7; ++i)
                        {
                            BYTE x;
                            v >> x;
                            if (x != i)
                                Fail("T19 (e) reader: value byte %d = %d", i, x);
                        }
                        bool threw = false;
                        try
                        {
                            BYTE extra;
                            v >> extra;
                        }
                        catch (CArchiveException* ex)
                        {
                            threw = (ex->m_cause == CArchiveException::endOfFile);
                            ex->Delete();
                        }
                        if (!threw)
                            Fail("T19 (e) reader: an eighth byte was readable from a 7-byte value");
                    }
                    if (!r.Next(tag, cb) || tag != 9 || cb != 3)
                        Fail("T19 (e) reader: second entry tag %d cb %u, expected 9/3", tag, (unsigned)cb);
                    r.Close();							// never asked for the second value
                    int next = 0;
                    ar >> next;
                    if (next != 0x5A5A5A5A)
                        Fail("T19 (e) reader: after Close the stream is at 0x%08X, expected 0x5A5A5A5A", next);
                }
                catch (CException* e)
                {
                    FailUnexpected("(e) reader", e);
                }
                ar.Abort();
            }
            mem.Detach();
            free(buf);
        }

        // SkipArchiveBytes: exact count is fine; one more than the stream holds
        // throws endOfFile rather than returning short.
        {
            static const unsigned char six[] = { 1,2,3,4,5,6 };
            for (int pass = 0; pass < 2; ++pass)
            {
                BYTE* buf = (BYTE*)malloc(sizeof six);
                memcpy(buf, six, sizeof six);
                CMemFile mem;
                mem.Attach(buf, sizeof six);
                {
                    CArchive ar(&mem, CArchive::load);
                    bool threw = false;
                    try
                    {
                        SkipArchiveBytes(ar, pass == 0 ? 6 : 7);
                    }
                    catch (CArchiveException* ex)
                    {
                        threw = (ex->m_cause == CArchiveException::endOfFile);
                        ex->Delete();
                    }
                    catch (CException* e)
                    {
                        FailUnexpected("(e) skip", e);
                        threw = true;
                    }
                    if (pass == 0 && threw)
                        Fail("T19 (e) skip: skipping exactly the available bytes threw");
                    if (pass == 1 && !threw)
                        Fail("T19 (e) skip: skipping past the end did not throw endOfFile");
                    ar.Abort();
                }
                mem.Detach();
                free(buf);
            }
        }
        // Close() without iterating: the whole region is discarded when it is
        // there, and a region claiming more than the stream holds is an
        // endOfFile, never a silent short read. This is the one path the
        // CColor loop cannot reach (it iterates to the end), so pin it here.
        for (int pass = 0; pass < 2; ++pass)
        {
            Bytes b;
            if (pass == 0)
            {
                PutDword(b, 8);
                PutByte(b, 5);
                PutDword(b, 3);
                PutByte(b, 1); PutByte(b, 2); PutByte(b, 3);
                PutInt(b, 0x5A5A5A5A);
            }
            else
            {
                PutDword(b, 20);
                for (int i = 0; i < 10; ++i)
                    PutByte(b, 0x00);
            }
            size_t n = b.size();
            BYTE* buf = (BYTE*)malloc(n);
            memcpy(buf, &b[0], n);
            CMemFile mem;
            mem.Attach(buf, (UINT)n);
            {
                CArchive ar(&mem, CArchive::load);
                bool threw = false;
                try
                {
                    CExtensionReader r(ar);
                    r.Close();
                    if (pass == 0)
                    {
                        int next = 0;
                        ar >> next;
                        if (next != 0x5A5A5A5A)
                            Fail("T19 (e) close-only: stream at 0x%08X after Close, expected 0x5A5A5A5A", next);
                    }
                }
                catch (CArchiveException* ex)
                {
                    threw = (ex->m_cause == CArchiveException::endOfFile);
                    ex->Delete();
                }
                catch (CException* e)
                {
                    FailUnexpected("(e) close-only", e);
                    threw = true;
                }
                if (pass == 0 && threw)
                    Fail("T19 (e) close-only: Close over a complete region threw");
                if (pass == 1 && !threw)
                    Fail("T19 (e) close-only: Close over a short region did not throw endOfFile");
                ar.Abort();
            }
            mem.Detach();
            free(buf);
        }
    }
}

static void RunT19()
{
    printf("T19 archive extension trailer oracle...\n");
    T19Ext::CheckRoundTrip();
    T19Ext::CheckForwardCompat();
    T19Ext::CheckLegacy();
    T19Ext::CheckRejections();
    T19Ext::CheckPrimitives();
    T19Ext::CheckAppendOnly();
    T19Ext::CheckLegacyStore();
}

//////////////////////////////////////////////////////////////////////////
// T32 — spectral correction (ccss) as a correction method
// (libHCFR/SpectralCorrection.h): the derivation and its exact
// interconversion, the refusals, the file hash, the record's trailer payload,
// and the persisted numbers. Pure assertions, no goldens.
//////////////////////////////////////////////////////////////////////////
namespace T32Spectral
{
    static Matrix M3(double a00, double a01, double a02,
                     double a10, double a11, double a12,
                     double a20, double a21, double a22)
    {
        Matrix m(0.0, 3, 3);
        m(0,0)=a00; m(0,1)=a01; m(0,2)=a02;
        m(1,0)=a10; m(1,1)=a11; m(1,2)=a12;
        m(2,0)=a20; m(2,1)=a21; m(2,2)=a22;
        return m;
    }

    // Relative to the larger magnitude; exact when both are zero.
    static bool NearRel(double a, double b, double rel)
    {
        double scale = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
        if (scale == 0.0)
            return true;
        return fabs(a - b) <= rel * scale;
    }

    // Every entry within rel * (largest |entry| of want): a matrix tolerance, so an
    // exact zero off the diagonal of a product is allowed its 1e-17 of round-off.
    static void CheckMatrix(const char* what, const Matrix& got, const Matrix& want, double rel)
    {
        if (got.GetRows() != 3 || got.GetColumns() != 3)
        {
            Fail("T32 %s: not 3x3", what);
            return;
        }
        double scale = 0.0;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                if (fabs(want(i,j)) > scale)
                    scale = fabs(want(i,j));
        if (scale == 0.0)
        {
            Fail("T32 %s: zero expectation", what);
            return;
        }
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                if (fabs(got(i,j) - want(i,j)) > rel * scale)
                    Fail("T32 %s [%d][%d]: got %.17g want %.17g (rel %g of %g)", what, i, j, got(i,j), want(i,j), rel, scale);
    }

    // An i1d3-like native fit (XYZ per sensor count, small numbers) and a
    // display-specific multiplier D standing in for what a ccss changes, so
    // the expected correction is known exactly: current = D * native.
    static Matrix Native()  { return M3(0.0152, 0.0031, 0.0007,  0.0049, 0.0198, 0.0012,  0.0003, 0.0027, 0.0251); }
    static Matrix Delta()   { return M3(1.03, -0.02, 0.01,  -0.01, 1.02, 0.0,  0.02, 0.01, 0.97); }
    static Matrix Current() { return Delta() * Native(); }

    // (a) The derivation and the exact interconversion it promises.
    static void CheckDerivation()
    {
        Matrix corr, toNative;
        std::string why;
        bool ok = DeriveSpectralCorrection(Native(), Current(), true, corr, toNative, why);
        if (!ok)
        {
            Fail("T32 (a) derivation refused: %s", why.c_str());
            return;
        }
        if (!why.empty())
            Fail("T32 (a) why not empty on success: %s", why.c_str());
        CheckMatrix("(a) correction == D", corr, Delta(), 1e-10);
        CheckMatrix("(a) correction*native == current", corr * Native(), Current(), 1e-10);
        CheckMatrix("(a) toNative*current == native", toNative * Current(), Native(), 1e-10);
        CheckMatrix("(a) correction*toNative == I", corr * toNative, Matrix::IdentityMatrix(3), 1e-10);

        // On a reading: the driver's corrected XYZ, undone to native by the
        // wrapper, re-corrected by HCFR, is the driver's XYZ again, and the
        // native value equals the native fit applied to the same sensor triple.
        Matrix rgb(0.0, 3, 1);
        rgb(0,0) = 123456.7; rgb(1,0) = 98765.4; rgb(2,0) = 45678.9;
        Matrix xyzCur       = Current() * rgb;
        Matrix xyzNat       = toNative * xyzCur;
        Matrix xyzNatDirect = Native() * rgb;
        Matrix xyzBack      = corr * xyzNat;
        for (int i = 0; i < 3; ++i)
        {
            if (!NearRel(xyzNat(i,0), xyzNatDirect(i,0), 1e-10))
                Fail("T32 (a) native[%d]: via undo %.17g, direct %.17g", i, xyzNat(i,0), xyzNatDirect(i,0));
            if (!NearRel(xyzBack(i,0), xyzCur(i,0), 1e-10))
                Fail("T32 (a) round trip[%d]: %.17g vs %.17g", i, xyzBack(i,0), xyzCur(i,0));
        }
    }

    static void ExpectRefused(const char* what, const Matrix& n, const Matrix& c, bool distinct)
    {
        Matrix corr = Delta(), toNative = Delta();   // must be reset to identity by a refusal
        std::string why;
        bool ok = DeriveSpectralCorrection(n, c, distinct, corr, toNative, why);
        if (ok)
            Fail("T32 (b) %s: accepted", what);
        if (why.empty())
            Fail("T32 (b) %s: no reason given", what);
        if (!corr.IsIdentity() || !toNative.IsIdentity())
            Fail("T32 (b) %s: outputs not reset to identity", what);
    }

    // (b) Refusals, and the equality tolerance pinned from both sides.
    static void CheckRejections()
    {
        Matrix bad = Current();
        bad(1,1) = std::numeric_limits<double>::quiet_NaN();
        ExpectRefused("NaN entry", Native(), bad, true);
        Matrix inf = Native();
        inf(0,2) = std::numeric_limits<double>::infinity();
        ExpectRefused("Inf entry", inf, Current(), true);
        Matrix sing = Native();
        for (int j = 0; j < 3; ++j)
            sing(2,j) = sing(1,j);   // two equal rows
        ExpectRefused("singular native", sing, Current(), true);
        ExpectRefused("singular current", Native(), sing, true);
        ExpectRefused("zero matrix", Matrix(0.0, 3, 3), Current(), true);
        ExpectRefused("not 3x3", Matrix(1.0, 2, 2), Current(), true);
        ExpectRefused("equal, distinct required", Native(), Native(), true);
        // A one-part-in-1e12 nudge is still "equal" (tolerance 1e-9 relative).
        Matrix nudged = Native();
        nudged(0,0) *= (1.0 + 1e-12);
        ExpectRefused("equal within tolerance, distinct required", Native(), nudged, true);

        // Equal without the requirement: accepted, identity outputs, no reason.
        Matrix corr = Delta(), toNative = Delta();
        std::string why;
        if (!DeriveSpectralCorrection(Native(), Native(), false, corr, toNative, why))
            Fail("T32 (b) equal, distinct not required: refused: %s", why.c_str());
        if (!corr.IsIdentity() || !toNative.IsIdentity())
            Fail("T32 (b) equal, distinct not required: outputs not identity");
        if (!why.empty())
            Fail("T32 (b) equal, distinct not required: why = %s", why.c_str());

        // A 0.1% scaled copy is distinct: the tolerance is on the entries.
        if (!DeriveSpectralCorrection(Native(), Native() * 1.001, true, corr, toNative, why))
            Fail("T32 (b) 0.1%% scaled copy refused as equal: %s", why.c_str());
        CheckMatrix("(b) scaled correction", corr, Matrix::IdentityMatrix(3) * 1.001, 1e-10);

        if (!SpectralMatricesEqual(Native(), nudged))
            Fail("T32 (b) SpectralMatricesEqual: 1e-12 nudge not equal");
        if (SpectralMatricesEqual(Native(), Native() * 1.001))
            Fail("T32 (b) SpectralMatricesEqual: 0.1%% scale reported equal");
        if (SpectralMatricesEqual(Native(), Matrix(1.0, 2, 2)))
            Fail("T32 (b) SpectralMatricesEqual: a 2x2 reported equal");
    }

    // (c) FNV-1a 64 against the published vectors, and the file variant.
    static void CheckHash()
    {
        unsigned long long h0 = Fnv1a64("", 0);
        if (h0 != 0xcbf29ce484222325ULL)
            Fail("T32 (c) fnv1a64(\"\") = %016llx", h0);
        unsigned long long ha = Fnv1a64("a", 1);
        if (ha != 0xaf63dc4c8601ec8cULL)
            Fail("T32 (c) fnv1a64(\"a\") = %016llx", ha);
        unsigned long long hf = Fnv1a64("foobar", 6);
        if (hf != 0x85944171f73967e8ULL)
            Fail("T32 (c) fnv1a64(\"foobar\") = %016llx", hf);

        const char* tmp = "t32_hash_probe.tmp";
        FILE* f = fopen(tmp, "wb");
        if (!f)
            Fail("T32 (c) cannot create %s", tmp);
        else
        {
            fwrite("foobar", 1, 6, f);
            fclose(f);
            unsigned long long hfile = 0;
            if (!Fnv1a64File(tmp, hfile))
                Fail("T32 (c) Fnv1a64File refused an existing file");
            if (hfile != 0x85944171f73967e8ULL)
                Fail("T32 (c) Fnv1a64File = %016llx", hfile);
            remove(tmp);
        }
        unsigned long long hmiss = 1;
        if (Fnv1a64File("t32_no_such_file.tmp", hmiss))
            Fail("T32 (c) Fnv1a64File accepted a missing file");
        if (hmiss != 0)
            Fail("T32 (c) Fnv1a64File left hash %016llx for a missing file", hmiss);
    }

    // (d) The record: defaults, equality, and its trailer payload byte for byte.
    static void CheckRecord()
    {
        SpectralCorrectionRecord d;
        if (d.IsPresent())
            Fail("T32 (d) default record present");
        if (d.source != SPECTRAL_SOURCE_NONE || d.fileHash != 0 || d.observer != -1
            || !d.description.empty() || !d.meterSerial.empty())
            Fail("T32 (d) default scalar fields");
        if (!d.nativeCal.IsIdentity() || !d.currentCal.IsIdentity() || !d.correction.IsIdentity())
            Fail("T32 (d) default matrices not identity");
        if (d.basis != FIT_BASIS_UNKNOWN)
            Fail("T32 (d) default basis %d", d.basis);

        SpectralCorrectionRecord r;
        r.source      = SPECTRAL_SOURCE_FILE;
        r.description = "LG OLED C9 (WOLED)";     // 18 chars
        r.fileHash    = 0x85944171f73967e8ULL;
        r.observer    = 3;
        r.meterSerial = "I1D3-12345";             // 10 chars
        r.nativeCal   = Native();
        r.currentCal  = Current();
        r.correction  = Delta();
        r.basis       = FIT_BASIS_NATIVE;
        if (!r.IsPresent())
            Fail("T32 (d) filled record not present");
        if (!(r == r) || r != r)
            Fail("T32 (d) equality not reflexive");
        SpectralCorrectionRecord r2 = r;
        r2.fileHash ^= 1;
        if (r2 == r)
            Fail("T32 (d) equality ignores the hash");
        r2 = r;
        r2.correction(0,0) += 1e-15;
        if (r2 == r)
            Fail("T32 (d) equality is not exact on matrices");
        r2 = r;
        r2.basis = FIT_BASIS_DRIVER_CORRECTED;
        if (r2 == r)
            Fail("T32 (d) equality ignores the basis");

        // Through the trailer exactly as the SensorCalibrationStream codec writes it: one tag.
        // Payload = int 4 + CString(1+18) + DWORD 4 + DWORD 4 + int 4 + CString(1+10)
        //         + 3 x Matrix::Serialize 3x3 (8+4+4+4+72 = 92) + int 4 = 326;
        // region = DWORD cb 4 + tag 1 + DWORD cbValue 4 + 326 = 335.
        CMemFile mem;
        {
            CArchive ar(&mem, CArchive::store);
            CExtensionWriter ext;
            r.Serialize(ext.BeginTag(ArchiveTag::SENSOR_SPECTRAL_CORRECTION));
            ext.EndTag();
            ext.Write(ar);
            ar.Flush();
            ar.Close();
        }
        ULONGLONG len = mem.GetLength();
        if (len != 335)
            Fail("T32 (d) trailer bytes %llu, want 335", len);

        mem.SeekToBegin();
        SpectralCorrectionRecord back;
        bool sawTag = false;
        {
            CArchive ar(&mem, CArchive::load);
            CExtensionReader ext(ar);
            BYTE tag; DWORD cb;
            while (ext.Next(tag, cb))
            {
                if (tag == ArchiveTag::SENSOR_SPECTRAL_CORRECTION)
                {
                    sawTag = true;
                    if (cb != 326)
                        Fail("T32 (d) cbValue %lu, want 326", (unsigned long)cb);
                    back.Serialize(ext.Value());
                }
                else
                    ext.Skip();
            }
            ext.Close();
            ar.Close();
        }
        if (!sawTag)
            Fail("T32 (d) tag SENSOR_SPECTRAL_CORRECTION not found");
        if (back != r)
            Fail("T32 (d) record did not round-trip");
        if (back.description != r.description || back.meterSerial != r.meterSerial)
            Fail("T32 (d) strings: '%s' / '%s'", back.description.c_str(), back.meterSerial.c_str());
        if (back.fileHash != r.fileHash)
            Fail("T32 (d) hash %016llx", back.fileHash);
        if (back.basis != FIT_BASIS_NATIVE)
            Fail("T32 (d) basis %d", back.basis);

        // A truncated payload is a short read (an exception), never a silent default.
        {
            CMemFile shortMem;
            {
                CArchive ar(&shortMem, CArchive::store);
                ar << (int)SPECTRAL_SOURCE_FILE;
                ar << CString("x");
                ar.Flush();
                ar.Close();
            }
            shortMem.SeekToBegin();
            SpectralCorrectionRecord s;
            bool threw = false;
            CArchive ar(&shortMem, CArchive::load);
            TRY
            {
                s.Serialize(ar);
            }
            CATCH_ALL(e)
            {
                threw = true;
                e->Delete();
            }
            END_CATCH_ALL
            ar.Abort();
            if (!threw)
                Fail("T32 (d) truncated payload loaded without an exception");
        }
    }

    // (e) The persisted numbers: the method value, the tag numbers, the enums.
    static void CheckPersistedNumbers()
    {
        if ((int)CALIB_SPECTRAL_SAMPLE != 4)
            Fail("T32 (e) CALIB_SPECTRAL_SAMPLE = %d, want 4", (int)CALIB_SPECTRAL_SAMPLE);
        if ((int)ArchiveTag::SENSOR_FIT_BASIS != 4)
            Fail("T32 (e) SENSOR_FIT_BASIS = %d, want 4", (int)ArchiveTag::SENSOR_FIT_BASIS);
        if ((int)ArchiveTag::SENSOR_SPECTRAL_CORRECTION != 5)
            Fail("T32 (e) SENSOR_SPECTRAL_CORRECTION = %d, want 5", (int)ArchiveTag::SENSOR_SPECTRAL_CORRECTION);
        if (FIT_BASIS_UNKNOWN != 0 || FIT_BASIS_NATIVE != 1 || FIT_BASIS_DRIVER_CORRECTED != 2)
            Fail("T32 (e) SpectralFitBasis values moved");
        if (SPECTRAL_SOURCE_NONE != 0 || SPECTRAL_SOURCE_FILE != 1 || SPECTRAL_SOURCE_DISPLAY_TYPE != 2)
            Fail("T32 (e) SpectralCorrectionSource values moved");
    }

    // (f) The sensor's dispatch, EffectiveSensorMatrix, over the whole
    // (method, basis, record present) table. The method's matrix M and the
    // record's correction D do not commute, so a composition in the wrong
    // order (D * M) fails here, not only a missing or an extra one. The
    // absent record still carries D in its correction field, so a dispatch
    // that ignored IsPresent() would compose and fail too.
    static Matrix MethodMatrix() { return M3(1.05, 0.02, -0.01,  0.03, 0.98, 0.01,  -0.02, 0.04, 1.01); }

    static void CheckDispatch()
    {
        const Matrix M = MethodMatrix();
        const Matrix D = Delta();
        // The canary's premise: the two orders really differ (well beyond 1e-12).
        Matrix md = M * D, dm = D * M;
        double maxDiff = 0.0;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                if (fabs(md(i,j) - dm(i,j)) > maxDiff)
                    maxDiff = fabs(md(i,j) - dm(i,j));
        if (maxDiff < 1e-4)
            Fail("T32 (f) M and D commute (max diff %g): the order would not be pinned", maxDiff);

        SpectralCorrectionRecord present;
        present.source     = SPECTRAL_SOURCE_FILE;
        present.correction = D;
        present.basis      = FIT_BASIS_NATIVE;
        SpectralCorrectionRecord absent;      // source NONE, but a non-identity correction
        absent.correction  = D;

        const int methods[] = { CALIB_CLASSIC_NIST, CALIB_HCFR_DEFAULT, CALIB_BODNER_THREEMATRIX,
                                CALIB_FCMM_NO_LUM, CALIB_SPECTRAL_SAMPLE };
        const int bases[]   = { FIT_BASIS_UNKNOWN, FIT_BASIS_NATIVE, FIT_BASIS_DRIVER_CORRECTED };
        int cells = 0;
        for (int mi = 0; mi < 5; ++mi)
            for (int bi = 0; bi < 3; ++bi)
                for (int p = 0; p < 2; ++p)
                {
                    const int method = methods[mi], basis = bases[bi];
                    const SpectralCorrectionRecord& rec = p ? present : absent;
                    // The table, stated independently of the implementation:
                    //   Bodner                                  -> identity (callers branch on it first)
                    //   spectral method                         -> M (M IS the correction: never composed)
                    //   other, basis != NATIVE, record present  -> M * D
                    //   otherwise                               -> M
                    Matrix want;
                    const char* rule;
                    if (method == CALIB_BODNER_THREEMATRIX)                          { want = Matrix::IdentityMatrix(3); rule = "identity"; }
                    else if (method == CALIB_SPECTRAL_SAMPLE)                        { want = M;     rule = "M"; }
                    else if (basis != FIT_BASIS_NATIVE && p)                         { want = M * D; rule = "M*D"; }
                    else                                                             { want = M;     rule = "M"; }

                    Matrix got = EffectiveSensorMatrix(method, basis, rec, M);
                    char what[96];
                    sprintf_s(what, "(f) method %d basis %d present %d -> %s", method, basis, p, rule);
                    // Same arithmetic on both sides: 1e-14 relative is round-off only.
                    CheckMatrix(what, got, want, 1e-14);
                    ++cells;
                }
        if (cells != 30)
            Fail("T32 (f) table has %d cells, want 30", cells);

        // The record's own basis does not drive the dispatch: the method's basis does.
        SpectralCorrectionRecord drv = present;
        drv.basis = FIT_BASIS_DRIVER_CORRECTED;
        CheckMatrix("(f) native method basis, driver-corrected record -> M",
                    EffectiveSensorMatrix(CALIB_HCFR_DEFAULT, FIT_BASIS_NATIVE, drv, M), M, 1e-14);
    }
}

static void RunT32()
{
    printf("T32 spectral correction (ccss as a method) oracle...\n");
    T32Spectral::CheckDerivation();
    T32Spectral::CheckRejections();
    T32Spectral::CheckHash();
    T32Spectral::CheckRecord();
    T32Spectral::CheckPersistedNumbers();
    T32Spectral::CheckDispatch();
}

//////////////////////////////////////////////////////////////////////////
// T13 - HLG black oracle (pure assertion, no golden file). At a signal of 0
// HLG returns its OOTF black lift, m_MinTL / White.Y (BT.2100: F_D = L_B at
// E' = 0). It must be exact there, finite for a system gamma below 1 (the
// OOTF would take pow(0, gamma - 1)), independent of PQ's tone-mapping
// switch, continuous with the curve just above 0, and a negative signal
// must land on the same value. The accuracy harness pins TargetMinL to 0
// and skips gray 0%, so this is the only test that reaches it.
//////////////////////////////////////////////////////////////////////////
static void RunT13()
{
    printf("T13 HLG black oracle...\n");

    static const double whites[] = { 100.0, 227.66, 1000.0 };
    static const double minTLs[] = { 0.0, 0.005, 0.05 };
    static const double gammas[] = { 0.93, 1.0, 1.2, 1.4 };

    int cases = 0;
    for (int w = 0; w < _countof(whites); w++)
    for (int m = 0; m < _countof(minTLs); m++)
    for (int g = 0; g < _countof(gammas); g++)
    for (int tm = 0; tm < 2; tm++)
    {
        const double Y = whites[w];
        CColor White(ColorXYZ(0.9505 * Y, Y, 1.089 * Y));
        const double want = minTLs[m] / Y;
        const bool toneMap = (tm != 0);

        const double at0 = getL_EOTF(0.0, White, noDataColor, 0.0, 0.0, 7, 100.0, 0.0, 1000.0,
                                     minTLs[m], Y, toneMap, false, gammas[g], 0.0, 0.0, 0.0);
        const double neg = getL_EOTF(-0.1, White, noDataColor, 0.0, 0.0, 7, 100.0, 0.0, 1000.0,
                                     minTLs[m], Y, toneMap, false, gammas[g], 0.0, 0.0, 0.0);
        const double above = getL_EOTF(1e-6, White, noDataColor, 0.0, 0.0, 7, 100.0, 0.0, 1000.0,
                                       minTLs[m], Y, toneMap, false, gammas[g], 0.0, 0.0, 0.0);

        if (!(at0 == at0) || at0 != want)
            Fail("T13 white=%g minTL=%g gamma=%g tm=%d: f(0)=%.17g, want %.17g", Y, minTLs[m], gammas[g], tm, at0, want);
        if (!(neg == neg) || neg != want)
            Fail("T13 white=%g minTL=%g gamma=%g tm=%d: f(-0.1)=%.17g, want %.17g", Y, minTLs[m], gammas[g], tm, neg, want);
        if (!(above == above) || fabs(above - at0) > 1e-9)
            Fail("T13 white=%g minTL=%g gamma=%g tm=%d: f(1e-6)=%.17g not continuous with f(0)=%.17g", Y, minTLs[m], gammas[g], tm, above, at0);
        cases++;
    }
    printf("  %d HLG black cases checked\n", cases);
}

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
    RunT13();
    RunT19();
    RunT32();
    RunT22();

    if (g_failures)
    {
        printf("\n%d FAILURE(S)\n", g_failures);
        return 1;
    }
    printf("\nALL TESTS PASSED (%s mode)\n", g_genMode ? "gen" : "verify");
    return 0;
}
