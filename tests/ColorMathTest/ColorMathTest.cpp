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
// T14 asserts the CsvPercentToCode inverse round-trips every code the decode distinguishes,
// T19 asserts the #193 archive extension trailer: round trip, unknown-tag skip,
// legacy bytes, rejections, the primitives, append-only values, and the legacy store,
// T22 asserts the test-color window paints each patch's own code, and T41 asserts the
// saturation-tracking hue/luminance errors and anchor transform. T44 writes a synthetic
// set with the .ti3 export writer and reads it back through the in-tree CGATS reader.
// T45 asserts that the #256 spectral-correction sync ends the same whichever comes
// first, a measurement or the sensor sheet, and pins its rules.
// The .chc round-trip originally planned for the T7
// slot needs app-level linkage and is deliberately still not in this console harness.

#include <afx.h>
#include "../../libHCFR/Color.h"
#include "../../libHCFR/ArchiveExtension.h"
#include "../../libHCFR/ArchiveTags.h"
#include "../../libHCFR/SpectralCorrection.h"
#include <limits>
#include "../../libHCFR/SaturationTracking.h"
#include "../../libHCFR/Ti3Export.h"
#include "../../Tools/spectro/cgats.h"

#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <direct.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utime.h>

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
// T14 - CsvPercentToCode inverse oracle (pure assertion, no golden file).
// CsvPercentToCode must be the exact round-trip inverse of CsvCodeToPercent
// over every code the decode can distinguish: the whole grid for FULL, the
// legal span (black..white) for LEGAL and the no-header legacy default (whose
// decode clamps codes outside it), and black..maxCode for EXTENDED (whose
// super-white percentages stay linear on the legal slope). 8-bit and 10-bit.
//////////////////////////////////////////////////////////////////////////
static void RunT14()
{
    printf("T14 CsvPercentToCode inverse oracle...\n");
    static const int bitsList[] = { 8, 10 };
    static const int ranges[]   = { CSV_RANGE_FULL, CSV_RANGE_LEGAL, CSV_RANGE_EXTENDED, CSV_RANGE_NONE };
    for (int b = 0; b < 2; ++b)
    {
        const int bits    = bitsList[b];
        const int maxCode = (1 << bits) - 1;
        const int black   = 16  << (bits - 8);
        const int white   = 235 << (bits - 8);
        for (int r = 0; r < 4; ++r)
        {
            const int range = ranges[r];
            int lo = (range == CSV_RANGE_FULL) ? 0 : black;
            int hi = (range == CSV_RANGE_FULL || range == CSV_RANGE_EXTENDED) ? maxCode : white;
            for (int code = lo; code <= hi; ++code)
            {
                int back = CsvPercentToCode(CsvCodeToPercent(code, bits, range), bits, range);
                if (back != code)
                    Fail("T14 bits=%d range=%d code=%d back=%d", bits, range, code, back);
            }
        }
        // clamped ends: out-of-span codes decode to the span edge and must map back onto it
        if (CsvPercentToCode(CsvCodeToPercent(0, bits, CSV_RANGE_LEGAL), bits, CSV_RANGE_LEGAL) != black)
            Fail("T14 bits=%d legal sub-black clamp", bits);
        if (CsvPercentToCode(CsvCodeToPercent(maxCode, bits, CSV_RANGE_LEGAL), bits, CSV_RANGE_LEGAL) != white)
            Fail("T14 bits=%d legal super-white clamp", bits);
    }
}

//////////////////////////////////////////////////////////////////////////
// T19 - archive extension trailer oracle (issue #193).
//
// Every versioned block in a .chc / .thc stream now ends with a length-
// prefixed, tagged extension region (libHCFR/ArchiveExtension.h). CColor is
// the one block that lives in libHCFR, so it is the block this console
// harness can pin end to end:
//   (a) a color carrying every optional field (provenance with a role
//       included) round-trips exactly, and the stream's version is base + 100;
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
//       without a TIMESTAMP or PROVENANCE tag has none; a PROVENANCE value
//       that ends after its five original fields reads with role UNKNOWN,
//       one with the role reads it, and one with a further int reads the
//       role and drops the rest - also when the fields fill the archive
//       buffer exactly - and one with 3 stray bytes after the name reads
//       with role UNKNOWN;
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
        if (c.HasProvenance())    Fail("T19 %s: unexpected provenance", what);
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

    static void ExpectProvenance(const char* what, const CColor& c, int patch, int seq, int bits,
                                 int range, const std::string& name, int role)
    {
        if (!c.HasProvenance())
        {
            Fail("T19 %s: provenance missing", what);
            return;
        }
        CColorProvenance p = c.GetProvenance();
        if (p.patchNumber != patch || p.sequenceIndex != seq || p.srcBits != bits || p.srcRange != range)
            Fail("T19 %s: provenance %d/%d/%d/%d, expected %d/%d/%d/%d", what,
                 p.patchNumber, p.sequenceIndex, p.srcBits, p.srcRange, patch, seq, bits, range);
        if (p.name != name)
            Fail("T19 %s: provenance name of %u chars, expected %u chars", what,
                 (unsigned)p.name.size(), (unsigned)name.size());
        if (p.role != role)
            Fail("T19 %s: provenance role %d, expected %d", what, p.role, role);
    }

    // CArchive's ANSI CString layout: a BYTE length below 255, else 0xFF and
    // a WORD length (below 0xFFFE), then the characters.
    static void PutCString(Bytes& b, const std::string& s)
    {
        if (s.size() < 255)
            PutByte(b, (BYTE)s.size());
        else
        {
            PutByte(b, 0xFF);
            WORD n = (WORD)s.size();
            PutRaw(b, &n, 2);
        }
        PutRaw(b, s.data(), s.size());
    }

    // A PROVENANCE value: the five fields csv-provenance first wrote, then
    // `extra` trailing ints (the role first).
    static Entry ProvenanceEntry(const std::string& name, const std::vector<int>& extra)
    {
        Entry e;
        e.tag = (BYTE)ArchiveTag::COLOR_PROVENANCE;
        PutInt(e.value, 42);
        PutInt(e.value, 7);
        PutInt(e.value, 10);
        PutInt(e.value, 1);
        PutCString(e.value, name);
        for (size_t i = 0; i < extra.size(); ++i)
            PutInt(e.value, extra[i]);
        return e;
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
        CColorProvenance prov;
        prov.patchNumber = 42;
        prov.sequenceIndex = 7;
        prov.srcBits = 10;
        prov.srcRange = 1;
        prov.name = "Skin tone (light)";
        prov.role = PROVENANCE_ROLE_ANCHOR;
        src.SetProvenance(prov);

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
        ExpectProvenance("(a)", dst, 42, 7, 10, 1, "Skin tone (light)", PROVENANCE_ROLE_ANCHOR);

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
                if (c.HasProvenance())
                    Fail("T19 (f) no provenance: HasProvenance() is true");
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

        // PROVENANCE (tag 3) with (i) the five original fields only, (ii) the
        // role, (iii) the role and one more int a newer build appended, each
        // followed by RAW_XYZ; (iv) and (v) repeat (i) and (ii) with a 4077-
        // character name, so the five fields end exactly at CArchive's 4096-
        // byte buffer (16 + 3 + 4077): a role check that watched the buffer
        // instead of the value would find it empty and drop (v)'s role.
        {
            struct Case { const char* name; size_t nameLen; int nExtra; int role; };
            static const Case cases[] = {
                { "(f) provenance, five fields",        17,   0, PROVENANCE_ROLE_UNKNOWN },
                { "(f) provenance with role",           17,   1, PROVENANCE_ROLE_VERIFICATION },
                { "(f) provenance with role and more",  17,   2, PROVENANCE_ROLE_VERIFICATION },
                { "(f) long-name provenance, five",     4077, 0, PROVENANCE_ROLE_UNKNOWN },
                { "(f) long-name provenance with role", 4077, 1, PROVENANCE_ROLE_VERIFICATION },
            };
            for (int i = 0; i < (int)(sizeof cases / sizeof cases[0]); ++i)
            {
                std::string name = (cases[i].nameLen == 17) ? std::string("Skin tone (light)")
                                                            : std::string(cases[i].nameLen, 'n');
                std::vector<int> extra;
                if (cases[i].nExtra >= 1) extra.push_back(PROVENANCE_ROLE_VERIFICATION);
                if (cases[i].nExtra >= 2) extra.push_back(77);
                std::vector<Entry> entries;
                entries.push_back(ProvenanceEntry(name, extra));
                entries.push_back(raw);
                Bytes b;
                PutColor101(b, 0.25, 0.5, 0.75, entries);
                CColor c;
                CException* e = Load(b, &c, 1);
                if (e)
                    FailUnexpected(cases[i].name, e);
                else
                {
                    ExpectProvenance(cases[i].name, c, 42, 7, 10, 1, name, cases[i].role);
                    ExpectRaw(cases[i].name, c, 0.2, 0.4, 0.6);
                }
            }
        }

        // The five fields and then 3 stray bytes, too few for a role: the
        // role reads as UNKNOWN, the bytes are discarded with the value, and
        // RAW_XYZ after it still decodes.
        {
            Entry prov = ProvenanceEntry("Skin tone (light)", std::vector<int>());
            for (int i = 0; i < 3; ++i)
                prov.value.push_back(0xA5);
            std::vector<Entry> entries;
            entries.push_back(prov);
            entries.push_back(raw);
            Bytes b;
            PutColor101(b, 0.25, 0.5, 0.75, entries);
            CColor c;
            CException* e = Load(b, &c, 1);
            if (e)
                FailUnexpected("(f) provenance with 3 trailing bytes", e);
            else
            {
                ExpectProvenance("(f) provenance with 3 trailing bytes", c, 42, 7, 10, 1, "Skin tone (light)", PROVENANCE_ROLE_UNKNOWN);
                ExpectRaw("(f) provenance with 3 trailing bytes", c, 0.2, 0.4, 0.6);
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

    // (g) The matrix-calibration refusal (PR #237 review R1). A record the driver
    // applies itself (basis DRIVER_CORRECTED) means the meter reports corrected
    // readings: a matrix fitted on them would be applied to native readings once
    // the record is cleared, so every matrix calibration entry point refuses. Every
    // source x basis cell, stated independently: refused only for a present record
    // with basis DRIVER_CORRECTED. The absent record carries DRIVER_CORRECTED as
    // its basis, so a predicate that ignored IsPresent() fails here.
    static void CheckMatrixCalibrationAllowed()
    {
        const int sources[] = { SPECTRAL_SOURCE_NONE, SPECTRAL_SOURCE_FILE, SPECTRAL_SOURCE_DISPLAY_TYPE };
        const int bases[]   = { FIT_BASIS_UNKNOWN, FIT_BASIS_NATIVE, FIT_BASIS_DRIVER_CORRECTED };
        int cells = 0, refused = 0;
        for (int si = 0; si < 3; ++si)
            for (int bi = 0; bi < 3; ++bi)
            {
                SpectralCorrectionRecord rec;
                rec.source = sources[si];
                rec.basis  = bases[bi];
                const bool want = !( sources[si] != SPECTRAL_SOURCE_NONE && bases[bi] == FIT_BASIS_DRIVER_CORRECTED );
                const bool got  = MatrixCalibrationAllowed(rec);
                if (got != want)
                    Fail("T32 (g) source %d basis %d: MatrixCalibrationAllowed %d, want %d",
                         sources[si], bases[bi], (int) got, (int) want);
                if (!got)
                    ++refused;
                ++cells;
            }
        if (cells != 9 || refused != 2)
            Fail("T32 (g) table has %d cells and %d refusals, want 9 and 2", cells, refused);
        // A default-constructed record (no spectral correction at all) allows it,
        // and so does a cleared driver-corrected one.
        SpectralCorrectionRecord none;
        if (!MatrixCalibrationAllowed(none))
            Fail("T32 (g) a default record refuses a matrix calibration");
        SpectralCorrectionRecord cleared;
        cleared.source = SPECTRAL_SOURCE_FILE;
        cleared.basis  = FIT_BASIS_DRIVER_CORRECTED;
        cleared.Clear();
        if (!MatrixCalibrationAllowed(cleared))
            Fail("T32 (g) a cleared driver-corrected record still refuses a matrix calibration");
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
    T32Spectral::CheckMatrixCalibrationAllowed();
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

//////////////////////////////////////////////////////////////////////////
// T41 - saturation-tracking oracle (pure assertion, no golden file). The
// "Saturation - shifts" view's hue error, luminance error and anchor-preview
// transform (libHCFR/SaturationTracking). Synthetic points about a D65-like
// white in u'v': a known rotation must read back as that many degrees, wrapped
// to -180..180; the anchor transform must put the anchor point itself on its
// target (that is what the knob settings are chosen for); a point with no
// error must come back unchanged; an anchor with no defined hue (target on the
// white) must be the identity.
//////////////////////////////////////////////////////////////////////////
namespace T41
{
    const double kUw = 0.1978, kVw = 0.4683;    // u'v' of the synthetic white
    const double kPi = 3.14159265358979323846;

    // A point at polar (r, deg) about the white, as XYZ with luminance Y
    static void Polar(double r, double deg, double Y, double xyz[3])
    {
        double u = kUw + r * cos(deg * kPi / 180.0);
        double v = kVw + r * sin(deg * kPi / 180.0);
        double X, Yo, Z;
        SatTrackUvYToXYZ(u, v, Y, X, Yo, Z);
        xyz[0] = X; xyz[1] = Yo; xyz[2] = Z;
    }

    static void Uv(const double xyz[3], double & u, double & v)
    {
        SatTrackXYZToUv(xyz[0], xyz[1], xyz[2], u, v);
    }
}

static void RunT41()
{
    printf("T41 saturation-tracking oracle...\n");
    using namespace T41;
    int checks = 0;

    // 1. Hue error of a known rotation, both signs and across the wrap
    static const double rotations[] = { 5.0, -5.0, 0.0, 37.5, 175.0, 190.0, -190.0 };
    static const double expected[]  = { 5.0, -5.0, 0.0, 37.5, 175.0, -170.0, 170.0 };
    for (int i = 0; i < _countof(rotations); i++)
    {
        const double base = 20.0;                   // target direction, degrees
        double t[3], m[3], ut, vt, um, vm;
        Polar(0.08, base, 30.0, t);
        Polar(0.08, base + rotations[i], 30.0, m);
        Uv(t, ut, vt);
        Uv(m, um, vm);
        double h = SatTrackHueErrorDeg(um, vm, ut, vt, kUw, kVw);
        if (fabs(h - expected[i]) > 1e-9)
            Fail("T41 rotation %g: hue error %.12f, want %.12f +/- 1e-9", rotations[i], h, expected[i]);
        checks++;
    }
    // A point on the white has no hue: 0 by definition
    if (SatTrackHueErrorDeg(kUw, kVw, kUw + 0.05, kVw, kUw, kVw) != 0.0)
        Fail("T41 measured on the white: hue error not 0");
    if (SatTrackHueErrorDeg(kUw + 0.05, kVw, kUw, kVw, kUw, kVw) != 0.0)
        Fail("T41 target on the white: hue error not 0");
    checks += 2;

    // 2. Radial and luminance errors of known ratios
    {
        double e = SatTrackRadialErrorPct(kUw + 0.066, kVw, kUw + 0.06, kVw, kUw, kVw);
        if (fabs(e - 10.0) > 1e-9)
            Fail("T41 radial 0.066/0.060: %.12f, want 10 +/- 1e-9", e);
        double l = SatTrackLuminanceErrorPct(33.0, 30.0);
        if (fabs(l - 10.0) > 1e-9)
            Fail("T41 luminance 33/30: %.12f, want 10 +/- 1e-9", l);
        if (SatTrackLuminanceErrorPct(5.0, 0.0) != 0.0)
            Fail("T41 luminance against a zero target: not 0");
        checks += 3;
    }

    // 3. The anchor point lands on its target, and the anchor reads the
    //    errors that were put in
    static const struct { double dHue, kSat, kLum; } errs[] =
    {
        { 7.0, 1.12, 0.93 }, { -4.5, 0.85, 1.08 }, { 0.0, 1.0, 1.0 }, { 30.0, 1.5, 0.5 }
    };
    for (int i = 0; i < _countof(errs); i++)
    {
        double t[3], m[3], out[3];
        Polar(0.07, 112.0, 25.0, t);
        Polar(0.07 * errs[i].kSat, 112.0 + errs[i].dHue, 25.0 * errs[i].kLum, m);
        SatTrackAnchor a = SatTrackComputeAnchor(m, t, kUw, kVw);
        if (!a.valid)
        {
            Fail("T41 anchor case %d: not valid", i);
            continue;
        }
        if (fabs(a.hueDeg - errs[i].dHue) > 1e-9
            || fabs(a.satPct - 100.0 * (errs[i].kSat - 1.0)) > 1e-9
            || fabs(a.lumPct - 100.0 * (errs[i].kLum - 1.0)) > 1e-9)
            Fail("T41 anchor case %d: errors H %.12f S %.12f L %.12f, want %g %g %g (+/- 1e-9)", i,
                 a.hueDeg, a.satPct, a.lumPct, errs[i].dHue, 100.0 * (errs[i].kSat - 1.0), 100.0 * (errs[i].kLum - 1.0));
        if (!SatTrackApplyAnchor(a, m, out))
            Fail("T41 anchor case %d: apply failed", i);
        double uo, vo, ut, vt;
        Uv(out, uo, vo);
        Uv(t, ut, vt);
        if (fabs(uo - ut) > 1e-9 || fabs(vo - vt) > 1e-9 || fabs(out[1] - t[1]) > 1e-9)
            Fail("T41 anchor case %d: anchor point lands at u'v'Y (%.12f %.12f %.12f), target (%.12f %.12f %.12f) +/- 1e-9",
                 i, uo, vo, out[1], ut, vt, t[1]);
        checks += 2;
    }

    // 4. A point with zero error is unchanged: an anchor that measured no
    //    error (anchor point == target) moves no other point either
    {
        double t[3], p[3], q[3], out[3];
        Polar(0.07, 250.0, 12.0, t);
        SatTrackAnchor a = SatTrackComputeAnchor(t, t, kUw, kVw);
        if (!a.valid || a.hueDeg != 0.0 || a.satPct != 0.0 || a.lumPct != 0.0)
            Fail("T41 zero-error anchor: valid %d H %.17g S %.17g L %.17g, want valid and exactly 0",
                 (int)a.valid, a.hueDeg, a.satPct, a.lumPct);
        Polar(0.03, 247.0, 4.0, p);
        Polar(0.11, 255.0, 20.0, q);
        const double * pts[3] = { t, p, q };
        for (int k = 0; k < 3; k++)
        {
            SatTrackApplyAnchor(a, pts[k], out);
            for (int c = 0; c < 3; c++)
                if (fabs(out[c] - pts[k][c]) > 1e-9)
                    Fail("T41 zero-error anchor moved point %d component %d: %.12f -> %.12f (+/- 1e-9)",
                         k, c, pts[k][c], out[c]);
            checks++;
        }
    }

    // 5. No defined hue at the anchor (target on the white): the identity, exactly
    {
        double w[3], m[3], out[3];
        Polar(0.0, 0.0, 50.0, w);
        Polar(0.05, 10.0, 40.0, m);
        SatTrackAnchor a = SatTrackComputeAnchor(m, w, kUw, kVw);
        if (a.valid)
            Fail("T41 target on the white: anchor reported valid");
        SatTrackApplyAnchor(a, m, out);
        if (out[0] != m[0] || out[1] != m[1] || out[2] != m[2])
            Fail("T41 invalid anchor is not the identity");
        checks += 2;
    }

    // 6. A correction that pushes a point past the spectrum locus (Z < 0) is
    //    refused, and the output is the unchanged input. Yellow-like point at
    //    u'v' (0.2027, 0.5662), Z ~ 3% of Y (a measured BT.2020 yellow), with
    //    an anchor that reads 10 percent undersaturated: scaling its radius by
    //    1.1 puts 12 - 3u - 20v below 0
    {
        double t[3], m[3], y[3], out[3];
        Polar(0.08, 90.0, 30.0, t);
        Polar(0.08 / 1.1, 90.0, 30.0, m);           // anchor: 1/1.1 of the target radius
        SatTrackAnchor a = SatTrackComputeAnchor(m, t, kUw, kVw);
        double X, Yo, Z;
        SatTrackUvYToXYZ(0.2027, 0.5662, 81.6, X, Yo, Z);
        y[0] = X; y[1] = Yo; y[2] = Z;
        if (!(Z > 0.0))
            Fail("T41 locus case: synthetic yellow has Z %.6f, want > 0", Z);
        bool ok = SatTrackApplyAnchor(a, y, out);
        if (ok)
            Fail("T41 locus case: correction past the locus was accepted");
        if (out[0] != y[0] || out[1] != y[1] || out[2] != y[2])
            Fail("T41 locus case: refused correction did not return the input");
        checks += 3;
    }

    // 7. An anchor ON the locus is accepted: a target with Z exactly 0 (the
    //    BT.2020 red corner, xy 0.708 0.292) and a measured point that is also
    //    on the locus (Z = 0, as a simulated sensor reports a pure red) at
    //    another x and Y. The anchor point must land on the target; its Z comes
    //    back as round-off of either sign (about a quarter of this 8 x 5 x 5
    //    grid goes negative, at -1e-14 or so), and that must not refuse it.
    {
        int refused = 0, cases = 0;
        for (int iy = 0; iy < 8; iy++)
        for (int im = 0; im < 5; im++)
        for (int ix = 0; ix < 5; ix++)
        {
            const double ty = 3.0 + 2.5 * iy;
            double t[3] = { ty * 0.708 / 0.292, ty, 0.0 };
            double ut, vt, out[3];
            Uv(t, ut, vt);
            const double my = ty * (0.80 + 0.05 * im), mx = 0.708 + 0.01 * ix;
            double m[3] = { my * mx / (1.0 - mx), my, 0.0 };
            SatTrackAnchor a = SatTrackComputeAnchor(m, t, kUw, kVw);
            cases++;
            if (!a.valid || !SatTrackApplyAnchor(a, m, out))
            {
                refused++;
                continue;
            }
            double uo, vo;
            Uv(out, uo, vo);
            if (fabs(uo - ut) > 1e-9 || fabs(vo - vt) > 1e-9 || fabs(out[1] - ty) > 1e-9 || out[2] < 0.0 || out[2] > 1e-9 * ty)
                Fail("T41 locus target Y %g: lands at u'v'Y (%.12f %.12f %.12f) Z %.3g, target (%.12f %.12f %.12f) Z 0",
                     ty, uo, vo, out[1], out[2], ut, vt, ty);
        }
        if (refused)
            Fail("T41 locus target: %d of %d anchor points refused", refused, cases);

        // Deterministic form of the same round-off: a valid, zero-error anchor
        // applied to a locus color whose Z is -1e-13 * Y (what the u'v' round
        // trip can return for Z = 0) is accepted with Z clamped to exactly 0;
        // at -1e-6 * Y, far beyond round-off, it is refused
        const double ty = 26.27;
        double t[3] = { ty * 0.708 / 0.292, ty, 0.0 };
        SatTrackAnchor a = SatTrackComputeAnchor(t, t, kUw, kVw);
        double y1[3] = { t[0], ty, -1e-13 * ty }, y2[3] = { t[0], ty, -1e-6 * ty }, out[3] = { 0.0, 0.0, 0.0 };
        if (!a.valid || !SatTrackApplyAnchor(a, y1, out) || out[2] != 0.0 || fabs(out[1] - ty) > 1e-9)
            Fail("T41 locus round-off: Z = -1e-13 Y not accepted and clamped (valid %d, Z out %.3g)", (int)a.valid, out[2]);
        if (SatTrackApplyAnchor(a, y2, out))
            Fail("T41 locus round-off: Z = -1e-6 Y accepted");
        cases += 2;
        checks += cases;
    }

    printf("  %d saturation-tracking checks\n", checks);
}

// T42 - user patch set definition helpers (pure assertion, no golden file).
// CsvPatchCodesMatch decides whether re-opening a patch list may keep the
// readings of the set of that name: only when every row still drives the same
// code. CsvSetNameFromPath names the set from the file.
static void RunT42()
{
    printf("T42 user patch set definition helpers...\n");

    std::vector<CsvPatchRow> a(3);
    for (int i = 0; i < 3; i++)
    {
        a[i].patch = i + 1;
        a[i].r = 16 + i; a[i].g = 32 + i; a[i].b = 64 + i;
        a[i].name = "row";
    }
    std::vector<CsvPatchRow> b(a);
    if (!CsvPatchCodesMatch(a, b))
        Fail("T42 identical rows: not a match");
    b[1].name = "renamed";
    b[1].patch = 99;
    if (!CsvPatchCodesMatch(a, b))
        Fail("T42 a new label on the same codes: not a match");
    for (int c = 0; c < 3; c++)
    {
        std::vector<CsvPatchRow> d(a);
        (c == 0 ? d[2].r : c == 1 ? d[2].g : d[2].b) += 1;
        if (CsvPatchCodesMatch(a, d))
            Fail("T42 last row, channel %d one code off: reported a match", c);
    }
    std::vector<CsvPatchRow> shorter(a.begin(), a.begin() + 2);
    if (CsvPatchCodesMatch(a, shorter))
        Fail("T42 different row counts: reported a match");

    static const struct { const char * path; const char * name; } kNames[] =
    {
        { "C:\\lists\\ramp.csv",     "ramp" },
        { "C:\\lists\\ramp.CSV",     "ramp" },
        { "C:\\lists\\Ramp.Csv",     "Ramp" },
        { "ramp.csv",                    "ramp" },
        { "C:\\lists\\ramp.csv.bak", "ramp.csv.bak" },
        { "C:\\a.csv\\ramp",         "ramp" },
        { "C:\\lists\\ramp",         "ramp" },
    };
    for (size_t i = 0; i < sizeof(kNames) / sizeof(kNames[0]); i++)
    {
        std::string got = CsvSetNameFromPath(kNames[i].path);
        if (got != kNames[i].name)
            Fail("T42 name of %s: '%s', want '%s'", kNames[i].path, got.c_str(), kNames[i].name);
    }
}

//////////////////////////////////////////////////////////////////////////
// T44 - CGATS CTI3 (.ti3) export round trip.
// Writes synthetic tables with WriteTi3File (libHCFR/Ti3Export, the writer the
// app's Export > Measures to ti3 file uses) and reads them back through the
// in-tree CGATS reader (Tools/spectro/cgats.c, linked from the spectro library).
// Header keywords, table type, field names and types, and every number are
// compared. The library writes reals at 6 significant digits, so the exact
// cases use values that print exactly at that precision (dyadic fractions of
// at most 6 significant digits, white Y = 200 so the 100/Y_white scale is 0.5);
// one case states the 6-digit bound for values that do not.
//////////////////////////////////////////////////////////////////////////
namespace T44
{
    // white (absolute cd/m2) and three readings with a gap in SAMPLE_ID (slot 3, 4 skipped)
    static const double kWhite[3] = { 190.0, 200.0, 217.5 };
    static const int    kIds[3]   = { 1, 2, 5 };
    static const double kRgb[3][3] = { { 100.0, 100.0, 100.0 }, { 50.0, 25.0, 0.0 }, { 6.25, 12.5, 75.0 } };
    static const double kXyz[3][3] = { { 190.0, 200.0, 217.5 }, { 41.25, 30.5, 8.75 }, { 12.125, 9.5, 60.25 } };
    static const double kSpec[3][4] = { { 2.5, 4.0, 8.25, 1.125 }, { 0.5, 1.75, 3.0, 0.25 }, { 6.0, 0.125, 0.0, 9.5 } };

    static Ti3Table Make(bool withSpectra)
    {
        Ti3Table t;
        t.descriptor = "T44 synthetic set";
        t.originator = "ColorMathTest";
        for (int c = 0; c < 3; c++)
            t.whiteXYZ[c] = kWhite[c];
        t.specBands = 4;
        t.specStartNm = 380.0;
        t.specEndNm = 680.0;
        for (int r = 0; r < 3; r++)
        {
            Ti3Reading rd;
            rd.sampleId = kIds[r];
            for (int c = 0; c < 3; c++)
            {
                rd.rgb[c] = kRgb[r][c];
                rd.xyz[c] = kXyz[r][c];
            }
            if (withSpectra || r != 1)  // without spectra: the middle reading lacks one
                rd.spectrum.assign(kSpec[r], kSpec[r] + 4);
            t.readings.push_back(rd);
        }
        return t;
    }

    static std::string TempPath(const char* name)
    {
        char dir[MAX_PATH];
        DWORD n = GetTempPathA(MAX_PATH, dir);
        return std::string((n > 0 && n < MAX_PATH) ? dir : ".\\") + name;
    }

    static const char* Kw(cgats* p, const char* k)
    {
        int i = p->find_kword(p, 0, k);
        return (i >= 0) ? p->t[0].kdata[i] : NULL;
    }

    static void CheckKw(cgats* p, const char* tag, const char* k, const char* want)
    {
        const char* got = Kw(p, k);
        if (want == NULL)
        {
            if (got != NULL)
                Fail("T44 %s: keyword %s present ('%s'), want absent", tag, k, got);
        }
        else if (got == NULL || strcmp(got, want) != 0)
            Fail("T44 %s: keyword %s = '%s', want '%s'", tag, k, got ? got : "(absent)", want);
    }

    // Read path back; returns NULL (after a Fail) when the reader refuses it.
    static cgats* ReadBack(const char* tag, const std::string& path)
    {
        cgats* p = new_cgats();
        if (p == NULL)
        {
            Fail("T44 %s: new_cgats failed", tag);
            return NULL;
        }
        p->add_other(p, "CTI3");
        if (p->read_name(p, path.c_str()) < 0)
        {
            Fail("T44 %s: CGATS reader refused %s: %s", tag, path.c_str(), p->e.m);
            p->del(p);
            return NULL;
        }
        return p;
    }

    static void CheckRoundTrip(bool withSpectra)
    {
        const char* tag = withSpectra ? "spectral" : "no-spectra";
        Ti3Table t = Make(withSpectra);
        std::string path = TempPath(withSpectra ? "HCFR_T44_spectral.ti3" : "HCFR_T44_plain.ti3");
        std::string err;
        if (!WriteTi3File(path.c_str(), t, err))
        {
            Fail("T44 %s: WriteTi3File failed: %s", tag, err.c_str());
            return;
        }
        cgats* p = ReadBack(tag, path);
        if (p == NULL)
            return;

        if (p->ntables != 1)
            Fail("T44 %s: %d tables, want 1", tag, p->ntables);
        if (p->t[0].tt != tt_other || p->t[0].oi < 0 || p->t[0].oi >= p->nothers
         || strcmp(p->others[p->t[0].oi], "CTI3") != 0)
            Fail("T44 %s: table type is not CTI3", tag);

        CheckKw(p, tag, "DESCRIPTOR", "T44 synthetic set");
        CheckKw(p, tag, "ORIGINATOR", "ColorMathTest");
        CheckKw(p, tag, "DEVICE_CLASS", "DISPLAY");
        CheckKw(p, tag, "COLOR_REP", "RGB_XYZ");
        CheckKw(p, tag, "NORMALIZED_TO_Y_100", "YES");
        CheckKw(p, tag, "LUMINANCE_XYZ_CDM2", "190.000000 200.000000 217.500000");
        CheckKw(p, tag, "INSTRUMENT_TYPE_SPECTRAL", withSpectra ? "YES" : "NO");
        CheckKw(p, tag, "SPECTRAL_BANDS", withSpectra ? "4" : NULL);
        CheckKw(p, tag, "SPECTRAL_START_NM", withSpectra ? "380.000000" : NULL);
        CheckKw(p, tag, "SPECTRAL_END_NM", withSpectra ? "680.000000" : NULL);
        if (Kw(p, "CREATED") == NULL)
            Fail("T44 %s: no CREATED keyword", tag);
        // the luminance keyword parses back to the absolute white exactly
        const char* lum = Kw(p, "LUMINANCE_XYZ_CDM2");
        double lx = 0, ly = 0, lz = 0;
        if (lum == NULL || sscanf(lum, "%lf %lf %lf", &lx, &ly, &lz) != 3
         || lx != kWhite[0] || ly != kWhite[1] || lz != kWhite[2])
            Fail("T44 %s: LUMINANCE_XYZ_CDM2 does not parse back to the white", tag);

        static const char* kNames[11] = { "SAMPLE_ID", "RGB_R", "RGB_G", "RGB_B", "XYZ_X", "XYZ_Y", "XYZ_Z",
                                          "SPEC_380", "SPEC_480", "SPEC_580", "SPEC_680" };
        const int nf = withSpectra ? 11 : 7;
        if (p->t[0].nfields != nf)
            Fail("T44 %s: %d fields, want %d", tag, p->t[0].nfields, nf);
        else
            for (int f = 0; f < nf; f++)
            {
                if (strcmp(p->t[0].fsym[f], kNames[f]) != 0)
                    Fail("T44 %s: field %d is %s, want %s", tag, f, p->t[0].fsym[f], kNames[f]);
                data_type want = (f == 0) ? nqcs_t : r_t;
                if (p->t[0].ftype[f] != want)
                    Fail("T44 %s: field %s has type %d, want %d", tag, kNames[f], (int)p->t[0].ftype[f], (int)want);
            }

        if (p->t[0].nsets != 3)
            Fail("T44 %s: %d sets, want 3", tag, p->t[0].nsets);
        else if (p->t[0].nfields == nf)
        {
            const double scale = 100.0 / kWhite[1];
            for (int r = 0; r < 3; r++)
            {
                char id[16];
                sprintf(id, "%d", kIds[r]);
                if (p->t[0].ftype[0] == nqcs_t && strcmp((char*)p->t[0].fdata[r][0], id) != 0)
                    Fail("T44 %s: set %d SAMPLE_ID '%s', want '%s'", tag, r, (char*)p->t[0].fdata[r][0], id);
                for (int f = 1; f < nf; f++)
                {
                    if (p->t[0].ftype[f] != r_t)
                        continue;
                    double got = *(double*)p->t[0].fdata[r][f];
                    double want = (f <= 3) ? kRgb[r][f - 1]
                                : (f <= 6) ? kXyz[r][f - 4] * scale
                                :            kSpec[r][f - 7] * scale;
                    if (got != want)  // exact: every value prints exactly at 6 significant digits
                        Fail("T44 %s: set %d %s = %.17g, want %.17g", tag, r, kNames[f], got, want);
                }
            }
        }
        p->del(p);
        remove(path.c_str());
    }

    // Values that do not print exactly come back within the library's
    // 6-significant-digit rounding: |back - v| <= 5e-6 * |v|.
    static void CheckPrecisionBound()
    {
        Ti3Table t;
        t.whiteXYZ[0] = 87.123456; t.whiteXYZ[1] = 93.7; t.whiteXYZ[2] = 101.987654;
        Ti3Reading rd;
        rd.sampleId = 7;
        rd.rgb[0] = 100.0 / 3.0; rd.rgb[1] = 512.0 * 100.0 / 1023.0; rd.rgb[2] = 0.0123456789;
        rd.xyz[0] = 12.3456789; rd.xyz[1] = 0.0987654321; rd.xyz[2] = 87.654321;
        t.readings.push_back(rd);
        std::string path = TempPath("HCFR_T44_precision.ti3"), err;
        if (!WriteTi3File(path.c_str(), t, err))
        {
            Fail("T44 precision: WriteTi3File failed: %s", err.c_str());
            return;
        }
        cgats* p = ReadBack("precision", path);
        if (p == NULL)
            return;
        if (p->t[0].nsets != 1 || p->t[0].nfields != 7)
            Fail("T44 precision: %d sets x %d fields, want 1 x 7", p->t[0].nsets, p->t[0].nfields);
        else
            for (int f = 1; f < 7; f++)
            {
                double want = (f <= 3) ? rd.rgb[f - 1] : rd.xyz[f - 4] * (100.0 / t.whiteXYZ[1]);
                double got = (p->t[0].ftype[f] == r_t) ? *(double*)p->t[0].fdata[0][f] : -1.0;
                if (fabs(got - want) > 5e-6 * fabs(want))
                    Fail("T44 precision: field %d = %.17g, want %.17g within 5e-6 relative", f, got, want);
            }
        p->del(p);
        remove(path.c_str());
    }

    static void CheckRefusals()
    {
        std::string err;
        Ti3Table t = Make(true);
        t.whiteXYZ[1] = 0.0;
        if (WriteTi3File(TempPath("HCFR_T44_refused.ti3").c_str(), t, err) || err.empty())
            Fail("T44 white Y = 0: written, want refused with a message");
        t = Make(true);
        t.readings.clear();
        if (WriteTi3File(TempPath("HCFR_T44_refused.ti3").c_str(), t, err) || err.empty())
            Fail("T44 no readings: written, want refused with a message");
        t = Make(true);
        if (WriteTi3File(TempPath("HCFR_T44_no_such_dir\\x.ti3").c_str(), t, err) || err.empty())
            Fail("T44 unwritable path: reported success, want the library's error");
        remove(TempPath("HCFR_T44_refused.ti3").c_str());

        // spectral layout rules: whole-nm bands only, every reading on the layout
        t = Make(true);
        if (!Ti3HasSpectralColumns(t))
            Fail("T44 380-680 nm, 4 bands, all readings: no spectral columns");
        t.specEndNm = 685.0;   // 101.667 nm steps
        if (Ti3HasSpectralColumns(t))
            Fail("T44 fractional-nm bands: spectral columns, want none");
        t = Make(true);
        t.readings[2].spectrum.pop_back();
        if (Ti3HasSpectralColumns(t))
            Fail("T44 one spectrum short a band: spectral columns, want none");
        if (Ti3SpectralFieldName(Make(true), 0) != "SPEC_380" || Ti3SpectralFieldName(Make(true), 3) != "SPEC_680")
            Fail("T44 spectral field names");
        const double w[3] = { 95.0471, 100.0, 108.883 };
        if (Ti3LuminanceKeyword(w) != "95.047100 100.000000 108.883000")
            Fail("T44 LUMINANCE_XYZ_CDM2 format: '%s'", Ti3LuminanceKeyword(w).c_str());
    }
}

static void RunT44()
{
    printf("T44 CGATS .ti3 export round trip...\n");
    T44::CheckRoundTrip(true);
    T44::CheckRoundTrip(false);
    T44::CheckPrecisionBound();
    T44::CheckRefusals();
}

//////////////////////////////////////////////////////////////////////////
// T46 - the per-reading wire-setup stamp (CColor trailer tag
// COLOR_WIRE_SETUP = 6, libHCFR/WireSetup.h) and the display record it is
// taken from. Pure assertion; no golden file. Every expectation is exact:
// the stamp is integers and a bool, so there is no tolerance anywhere.
//   (a) every field survives Serialize store -> load;
//   (b) a shorter value (an older writer) loads with the defaults after the
//       ints it holds, and a longer one (a newer writer) loads the nine it
//       knows; realDrive is true only for exactly 1;
//   (c) a reading without the entry loads as unknown (no stamp, defaults);
//   (d) the entry's bytes are pinned, and a load + re-save is byte-exact;
//   (e) copy, assignment and Clear keep the stamp deep and separate, and the
//       4.1 legacy store drops it;
//   (f) WireDisplayRecord: a stamp is real only for exactly the recorded
//       stimulus with nothing displayed since.
//////////////////////////////////////////////////////////////////////////
namespace T46Wire
{
    using T19Ext::Bytes;
    using T19Ext::Entry;

    static CColorWireSetup Sample()
    {
        CColorWireSetup s;
        s.realDrive = true;
        s.wireBits = 10;
        s.encodingRange = WIRE_RANGE_LIMITED;
        s.deviceRange = WIRE_RANGE_FULL;
        s.generatorId = WIRE_GENERATOR_GDI;
        s.outputMode = 6;
        s.wireCode[0] = 64;
        s.wireCode[1] = 502;
        s.wireCode[2] = 940;
        return s;
    }

    static void Describe(char* buf, size_t n, const CColorWireSetup& s)
    {
        _snprintf_s(buf, n, _TRUNCATE, "{%d, %d, %d, %d, %d, %d, %d/%d/%d}",
                    s.realDrive ? 1 : 0, s.wireBits, s.encodingRange, s.deviceRange,
                    s.generatorId, s.outputMode, s.wireCode[0], s.wireCode[1], s.wireCode[2]);
    }

    static void ExpectSetup(const char* what, const CColorWireSetup& got, const CColorWireSetup& want)
    {
        if (got != want)
        {
            char g[160], e[160];
            Describe(g, sizeof(g), got);
            Describe(e, sizeof(e), want);
            Fail("T46 %s: stamp %s, expected %s", what, g, e);
        }
    }

    // A 101 color, XYZ (0.25, 0.5, 0.75), whose trailer holds one tag-6 entry
    // made of `ints`, then `extraBytes` stray bytes.
    static Bytes ColorWithWireValue(const std::vector<int>& ints, int extraBytes)
    {
        Entry e;
        e.tag = (BYTE)ArchiveTag::COLOR_WIRE_SETUP;
        for (size_t i = 0; i < ints.size(); ++i)
            T19Ext::PutInt(e.value, ints[i]);
        for (int i = 0; i < extraBytes; ++i)
            e.value.push_back((unsigned char)(0xA0 + i));
        std::vector<Entry> entries(1, e);
        Bytes b;
        T19Ext::PutColor101(b, 0.25, 0.5, 0.75, entries);
        return b;
    }

    static bool LoadOne(const char* what, const Bytes& b, CColor& out)
    {
        CException* e = T19Ext::Load(b, &out, 1);
        if (e)
        {
            int cause = -1;
            if (e->IsKindOf(RUNTIME_CLASS(CArchiveException)))
                cause = ((CArchiveException*)e)->m_cause;
            Fail("T46 %s: load threw %s (archive cause %d)", what,
                 e->GetRuntimeClass()->m_lpszClassName, cause);
            e->Delete();
            return false;
        }
        return true;
    }

    // What a generator records: Sample()'s setup with all three codes = code.
    static void Display(double r, double g, double b, int code, unsigned long* pSerial)
    {
        const double stim[3] = { r, g, b };
        CColorWireSetup s = Sample();
        s.realDrive = false;
        s.wireCode[0] = s.wireCode[1] = s.wireCode[2] = code;
        WireDisplayRecord::Record(stim, s);
        if (pSerial)
            WireDisplayRecord::StampFor(stim, pSerial);
    }
}

static void RunT46()
{
    printf("T46 per-reading wire-setup stamp (tag 6)...\n");
    using namespace T46Wire;
    const CColorWireSetup kDefault;
    const CColorWireSetup kSample = Sample();

    // the defaults the reader fills in, as ArchiveTags.h states them
    if (kDefault.realDrive || kDefault.wireBits != 0 || kDefault.encodingRange != -1
        || kDefault.deviceRange != -1 || kDefault.generatorId != 0 || kDefault.outputMode != -1
        || kDefault.wireCode[0] != -1 || kDefault.wireCode[1] != -1 || kDefault.wireCode[2] != -1)
        Fail("T46 defaults: not (false, 0, -1, -1, 0, -1, -1, -1, -1)");

    // (a) round trip of every field, with realDrive true and false
    for (int k = 0; k < 2; ++k)
    {
        CColor c(0.25, 0.5, 0.75);
        CColorWireSetup s = kSample;
        if (k == 1)
        {
            s.realDrive = false;
            s.wireBits = 8;
            s.encodingRange = WIRE_RANGE_FULL;
            s.deviceRange = WIRE_RANGE_UNKNOWN;
            s.outputMode = 0;
            s.wireCode[0] = 0; s.wireCode[1] = 128; s.wireCode[2] = 255;
        }
        c.SetWireSetup(s);
        Bytes b = T19Ext::Store(c);
        CColor back;
        if (LoadOne("(a) round trip", b, back))
        {
            if (!back.HasWireSetup())
                Fail("T46 (a) round trip %d: no stamp after load", k);
            ExpectSetup(k ? "(a) round trip, realDrive false" : "(a) round trip, realDrive true",
                        back.GetWireSetup(), s);
        }
    }

    // (b) shorter values: n ints, every n from 0 to 9; then longer ones
    {
        const int full[9] = { 1, 10, 1, 0, 1, 6, 64, 502, 940 };
        for (int n = 0; n <= 9; ++n)
        {
            std::vector<int> ints(full, full + n);
            CColor back;
            char what[64];
            _snprintf_s(what, sizeof(what), _TRUNCATE, "(b) a value of %d ints", n);
            if (!LoadOne(what, ColorWithWireValue(ints, 0), back))
                continue;
            if (!back.HasWireSetup())
                Fail("T46 %s: no stamp after load", what);
            CColorWireSetup want = kDefault;
            if (n > 0) want.realDrive = true;
            if (n > 1) want.wireBits = 10;
            if (n > 2) want.encodingRange = 1;
            if (n > 3) want.deviceRange = 0;
            if (n > 4) want.generatorId = 1;
            if (n > 5) want.outputMode = 6;
            if (n > 6) want.wireCode[0] = 64;
            if (n > 7) want.wireCode[1] = 502;
            if (n > 8) want.wireCode[2] = 940;
            ExpectSetup(what, back.GetWireSetup(), want);
        }
        // fields a newer writer appends are skipped; a partial int is not an int
        static const int kExtra[] = { 2, 4, 7 };
        for (int j = 0; j < 3; ++j)
        {
            std::vector<int> ints(full, full + 9);
            CColor back;
            char what[64];
            _snprintf_s(what, sizeof(what), _TRUNCATE, "(b) nine ints then %d more bytes", kExtra[j]);
            if (LoadOne(what, ColorWithWireValue(ints, kExtra[j]), back))
                ExpectSetup(what, back.GetWireSetup(), kSample);
        }
        {
            std::vector<int> ints(full, full + 3);
            CColor back;
            if (LoadOne("(b) three ints then 3 bytes", ColorWithWireValue(ints, 3), back))
            {
                CColorWireSetup want = kDefault;
                want.realDrive = true; want.wireBits = 10; want.encodingRange = 1;
                ExpectSetup("(b) three ints then 3 bytes", back.GetWireSetup(), want);
            }
        }
        static const int kNotOne[] = { 0, 2, -1, 0x7FFFFFFF };
        for (int j = 0; j < 4; ++j)
        {
            std::vector<int> ints(full, full + 9);
            ints[0] = kNotOne[j];
            CColor back;
            if (LoadOne("(b) realDrive not 1", ColorWithWireValue(ints, 0), back)
                && back.GetWireSetup().realDrive)
                Fail("T46 (b) a stored realDrive of %d read as true", kNotOne[j]);
        }
    }

    // (c) no stamp: a stored color without one, and a hand-built trailer
    {
        CColor c(0.25, 0.5, 0.75);
        c.SetStimulusValue(ColorRGBDisplay(50.0, 50.0, 50.0));
        CColor back;
        back.SetWireSetup(kSample);		// a load replaces it
        if (LoadOne("(c) stored without a stamp", T19Ext::Store(c), back))
        {
            if (back.HasWireSetup())
                Fail("T46 (c) stored without a stamp: the load kept a stamp");
            ExpectSetup("(c) stored without a stamp", back.GetWireSetup(), kDefault);
        }
        Bytes b;
        T19Ext::PutColor101(b, 0.25, 0.5, 0.75, std::vector<Entry>());
        CColor bare;
        if (LoadOne("(c) empty trailer", b, bare) && bare.HasWireSetup())
            Fail("T46 (c) empty trailer: a stamp appeared");
        CColor fresh;
        if (fresh.HasWireSetup())
            Fail("T46 (c) a new CColor carries a stamp");
    }

    // (d) the bytes: a color whose only optional field is the stamp stores
    // exactly version 101, XYZ, then one 41-byte entry (tag, cb = 36, 9 ints),
    // and a load + re-save gives the same bytes
    {
        CColor c(0.25, 0.5, 0.75);
        c.SetWireSetup(kSample);
        Bytes got = T19Ext::Store(c);
        const int ints[9] = { 1, 10, 1, 0, 1, 6, 64, 502, 940 };
        Bytes want = ColorWithWireValue(std::vector<int>(ints, ints + 9), 0);
        if (got != want)
            Fail("T46 (d) stored bytes: %u bytes, expected the %u-byte pinned layout",
                 (unsigned)got.size(), (unsigned)want.size());
        CColor back;
        if (LoadOne("(d) re-save", got, back) && T19Ext::Store(back) != got)
            Fail("T46 (d) load + re-save is not byte-exact");

        // with every other optional field present too
        CColor full(0.25, 0.5, 0.75);
        full.SetRawXYZValue(ColorXYZ(1.0, 2.0, 3.0));
        full.SetStimulusValue(ColorRGBDisplay(10.0, 20.0, 30.0));
        full.SetTimestamp(1789000000.5);
        CColorProvenance p;
        p.patchNumber = 3; p.sequenceIndex = 2; p.srcBits = 10; p.srcRange = 1;
        p.name = "gray 50"; p.role = PROVENANCE_ROLE_COLOR;
        full.SetProvenance(p);
        full.SetWireSetup(kSample);
        Bytes b1 = T19Ext::Store(full);
        CColor back2;
        if (LoadOne("(d) re-save with every field", b1, back2))
        {
            ExpectSetup("(d) re-save with every field", back2.GetWireSetup(), kSample);
            if (T19Ext::Store(back2) != b1)
                Fail("T46 (d) load + re-save with every field is not byte-exact");
        }
    }

    // (e) copies are deep; Clear and ClearSpectrumLux drop the stamp; the
    // legacy (4.1) store writes no trailer, so the stamp is dropped there
    {
        CColor a(0.25, 0.5, 0.75);
        a.SetWireSetup(kSample);
        CColor b(a);
        CColor c;
        c = a;
        a.ClearWireSetup();
        if (a.HasWireSetup())
            Fail("T46 (e) ClearWireSetup left a stamp");
        ExpectSetup("(e) copy constructor", b.GetWireSetup(), kSample);
        ExpectSetup("(e) assignment", c.GetWireSetup(), kSample);
        c = a;
        if (c.HasWireSetup())
            Fail("T46 (e) assigning a color without a stamp kept the old one");
        b.ClearSpectrumLux();
        if (b.HasWireSetup())
            Fail("T46 (e) ClearSpectrumLux left the stamp");

        CColor plain(0.25, 0.5, 0.75), stamped(0.25, 0.5, 0.75);
        stamped.SetWireSetup(kSample);
        Bytes lp, ls;
        {
            CColorLegacyStoreScope legacy;
            lp = T19Ext::Store(plain);
            ls = T19Ext::Store(stamped);
        }
        if (lp != ls)
            Fail("T46 (e) the legacy store wrote the stamp (%u bytes against %u)",
                 (unsigned)ls.size(), (unsigned)lp.size());
    }

    // (f) the display record
    {
        unsigned long serial = 0;
        const double gray[3] = { 50.0, 50.0, 50.0 };
        WireDisplayRecord::Invalidate();
        ExpectSetup("(f) no record", WireDisplayRecord::StampFor(gray, &serial), kDefault);

        Display(50.0, 50.0, 50.0, 128, NULL);
        CColorWireSetup want = kSample;
        want.wireCode[0] = want.wireCode[1] = want.wireCode[2] = 128;
        ExpectSetup("(f) recorded stimulus", WireDisplayRecord::StampFor(gray, &serial), want);
        if (!WireDisplayRecord::StillCurrent(serial))
            Fail("T46 (f) nothing displayed, yet the record is not current");

        // exactly this stimulus: one channel one ulp away is not it
        const double offByUlp[3] = { 50.0, 50.0, nextafter(50.0, 51.0) };
        ExpectSetup("(f) one ulp off", WireDisplayRecord::StampFor(offByUlp, NULL), kDefault);
        const double placeholder[3] = { 0.5, 0.5, 0.5 };
        ExpectSetup("(f) the 0.5% placeholder", WireDisplayRecord::StampFor(placeholder, NULL), kDefault);

        // a display during the read moves the serial, even back to the same patch
        WireDisplayRecord::StampFor(gray, &serial);
        Display(50.0, 50.0, 50.0, 128, NULL);
        if (WireDisplayRecord::StillCurrent(serial))
            Fail("T46 (f) a display during the read left the read's serial current");
        WireDisplayRecord::StampFor(gray, &serial);
        WireDisplayRecord::Invalidate();
        if (WireDisplayRecord::StillCurrent(serial))
            Fail("T46 (f) an invalidation during the read left the serial current");
        ExpectSetup("(f) after Invalidate", WireDisplayRecord::StampFor(gray, NULL), kDefault);

        // the surface check: the recorded codes on the recorded surface keep the
        // record, other codes there drop it, another surface is ignored
        Display(50.0, 50.0, 50.0, 128, &serial);
        const int same[3] = { 128, 128, 128 }, other[3] = { 127, 128, 128 };
        WireDisplayRecord::NoteSurfacePaint(6, same);
        if (!WireDisplayRecord::StillCurrent(serial) || !WireDisplayRecord::StampFor(gray, NULL).realDrive)
            Fail("T46 (f) a repaint with the recorded codes dropped the record");
        WireDisplayRecord::NoteSurfacePaint(5, other);
        if (!WireDisplayRecord::StampFor(gray, NULL).realDrive)
            Fail("T46 (f) a repaint of another surface dropped the record");
        WireDisplayRecord::NoteSurfacePaint(6, other);
        if (WireDisplayRecord::StampFor(gray, NULL).realDrive)
            Fail("T46 (f) a repaint with other codes on the recorded surface kept the record");

        // Intensity re-key: only from the displayed stimulus, and only when the
        // codes on the wire are the requested stimulus's own
        const double dimmed[3] = { 50.0, 50.0, nextafter(50.0, 49.0) };
        const int c128[3] = { 128, 128, 128 }, c127[3] = { 128, 128, 127 };
        Display(dimmed[0], dimmed[1], dimmed[2], 128, NULL);
        WireDisplayRecord::RekeyToRequested(gray, gray, c128);		// not the displayed stimulus
        if (WireDisplayRecord::StampFor(gray, NULL).realDrive)
            Fail("T46 (f) re-key from a stimulus that was not displayed");
        WireDisplayRecord::RekeyToRequested(dimmed, gray, c127);	// other codes
        if (WireDisplayRecord::StampFor(gray, NULL).realDrive)
            Fail("T46 (f) re-key with codes that differ");
        WireDisplayRecord::RekeyToRequested(dimmed, gray, c128);
        if (!WireDisplayRecord::StampFor(gray, NULL).realDrive)
            Fail("T46 (f) re-key with the requested stimulus's own codes did not take");
        if (WireDisplayRecord::StampFor(dimmed, NULL).realDrive)
            Fail("T46 (f) the re-keyed record still answers for the displayed stimulus");
        Display(dimmed[0], dimmed[1], dimmed[2], -1, NULL);			// codes unknown
        WireDisplayRecord::RekeyToRequested(dimmed, gray, c128);
        if (WireDisplayRecord::StampFor(gray, NULL).realDrive)
            Fail("T46 (f) re-key of a record without codes");

        // Peek, and a recorder's realDrive is never stored
        double seen[3];
        CColorWireSetup peeked;
        if (!WireDisplayRecord::Peek(seen, &peeked) || seen[2] != dimmed[2] || peeked.realDrive)
            Fail("T46 (f) Peek: not the current record, or realDrive stored as true");
        WireDisplayRecord::Invalidate();
        if (WireDisplayRecord::Peek(seen, &peeked))
            Fail("T46 (f) Peek found a record after Invalidate");
    }
}

// =============================================================================
// T43 - where an existing gray reading sits on a gray-level axis (libHCFR
// ReadingWireGrayProp / ReadingGrayProp and the GraySeries
// accessor core behind CMeasure::GetGrayReadingLevel / Prop). Pure assertions,
// no golden files. Every comparison is exact (==): the rule has no tolerance,
// and the fallback must be bit for bit the value the describing sites computed
// before the stamp existed.
// =============================================================================
namespace T43Drive
{
    // A reading of a gray stimulus `level` (%) with a wire stamp.
    static CColor Stamped(double level, bool realDrive, int bits, int range, int code,
                          int deviceRange = WIRE_RANGE_UNKNOWN)
    {
        CColor c(0.3, 0.4, 0.5);
        c.SetStimulusValue(ColorRGBDisplay(level));
        CColorWireSetup w;
        w.realDrive = realDrive;
        w.wireBits = bits;
        w.encodingRange = range;
        w.deviceRange = deviceRange;
        w.generatorId = WIRE_GENERATOR_GDI;
        w.outputMode = 0;
        w.wireCode[0] = w.wireCode[1] = w.wireCode[2] = code;
        c.SetWireSetup(w);
        return c;
    }

    static double Black(int bits, int range)
    {
        return range == WIRE_RANGE_LIMITED ? (bits == 10 ? 64.0 : 16.0) : 0.0;
    }
    static double Grid(int bits, int range)
    {
        return bits == 10 ? (range == WIRE_RANGE_LIMITED ? 876.0 : 1023.0)
                          : (range == WIRE_RANGE_LIMITED ? 219.0 : 255.0);
    }
    // The code a sweep sends for nominal level n on a grid (round to nearest).
    static int CodeFor(int n, int bits, int range)
    {
        return (int)(floor(n / 100.0 * Grid(bits, range) + 0.5) + Black(bits, range));
    }

    // r as the only reading of a near-white series whose one slot is nominal n
    // (clip column n + 1): GraySeriesLevel / Prop of index 0 is the accessor's
    // level for r, and GraySeriesSlotLevel the slot it falls back to.
    static GraySeriesView OneSlot(const CColor* r, int n, bool rd, bool b10, bool lim)
    {
        GraySeriesView v;
        v.series = GRAY_SERIES_NEAR_WHITE; v.readings = r; v.size = 1;
        v.nominalLevels = NULL; v.nominalCount = 0; v.nearWhiteClipCol = n + 1; v.pqNearBlack = false;
        v.bUseRoundDown = rd; v.b10bit = b10; v.is16_235 = lim;
        return v;
    }

    // Expect the reading at exactly `wantProp` (stamped drive taken) under the
    // current grid flags, whatever slot level the site passes.
    static void ExpectDrive(const char* what, const CColor& r, double slotLevel,
                            bool rd, bool b10, bool lim, double wantProp)
    {
        double p = -1.0;
        if (!ReadingWireGrayProp(r, &p))
            Fail("T43 %s: stamped drive not taken", what);
        else if (p != wantProp)
            Fail("T43 %s: drive %.17g, expected %.17g", what, p, wantProp);
        const double lvl = GraySeriesLevel(OneSlot(&r, 50, rd, b10, lim), 0);
        if (lvl != wantProp * 100.0)
            Fail("T43 %s: level %.17g, expected %.17g", what, lvl, wantProp * 100.0);
        const double prop = ReadingGrayProp(r, slotLevel, rd, b10, lim);
        if (prop != wantProp)
            Fail("T43 %s: prop %.17g, expected %.17g", what, prop, wantProp);
    }

    // Expect the fallback: the slot level and GrayLevelToGrayProp of it.
    static void ExpectFallback(const char* what, const CColor& r, double slotLevel,
                               bool rd, bool b10, bool lim)
    {
        double p = -1.0;
        if (ReadingWireGrayProp(r, &p))
            Fail("T43 %s: a drive (%.17g) was taken; expected the fallback", what, p);
        // the series accessor, with its one slot at the caller's nominal level,
        // must give back the caller's independently computed slotLevel
        const GraySeriesView one = OneSlot(&r, (int)floor(slotLevel + 0.5), rd, b10, lim);
        const double lvl = GraySeriesLevel(one, 0);
        if (lvl != slotLevel)
            Fail("T43 %s: level %.17g, expected the caller's slot %.17g", what, lvl, slotLevel);
        if (GraySeriesProp(one, 0) != GrayLevelToGrayProp(slotLevel, rd, b10, lim))
            Fail("T43 %s: series prop is not the caller's slot's", what);
        const double want = GrayLevelToGrayProp(slotLevel, rd, b10, lim);
        const double prop = ReadingGrayProp(r, slotLevel, rd, b10, lim);
        if (prop != want)
            Fail("T43 %s: prop %.17g, expected the slot's %.17g", what, prop, want);
    }
}

static void RunT43()
{
    printf("T43 gray reading level: stamped drive or the slot (wire stamp)...\n");
    using namespace T43Drive;
    const int F = WIRE_RANGE_FULL, L = WIRE_RANGE_LIMITED;

    // (a) Dominic's case: near white 95..99% measured on an 8-bit FULL wire
    //     (codes 242/245/247/250/252) and viewed with LIMITED selected. Each
    //     reading sits at its own code/255, never at the limited grid's
    //     94.98/95.89/96.80/98.17/99.09%.
    {
        const int codes[5] = { 242, 245, 247, 250, 252 };
        const double shown[5] = { 94.90, 96.08, 96.86, 98.04, 98.82 };	// 2 decimals, as the brief states them
        for (int k = 0; k < 5; ++k)
        {
            const int n = 95 + k;
            if (CodeFor(n, 8, F) != codes[k])
                Fail("T43 (a) code for %d%% on 8-bit full: %d, expected %d", n, CodeFor(n, 8, F), codes[k]);
            const double stim = ArrayIndexToGrayLevel(n, 101, false, false, false);	// what the full-range sweep drove
            const CColor r = Stamped(stim, true, 8, F, codes[k]);
            const double slotLimited = ArrayIndexToGrayLevel(n, 101, false, false, true);
            char what[64];
            _snprintf_s(what, sizeof(what), _TRUNCATE, "(a) full 8-bit code %d viewed limited", codes[k]);
            ExpectDrive(what, r, slotLimited, false, false, true, codes[k] / 255.0);
            const double lvl = GraySeriesLevel(OneSlot(&r, n, false, false, true), 0);
            if (floor(lvl * 100.0 + 0.5) / 100.0 != shown[k])
                Fail("T43 %s: %.4f%% does not read %.2f%%", what, lvl, shown[k]);
            if (lvl == slotLimited)
                Fail("T43 %s: placed on the limited grid (%.4f%%)", what, slotLimited);
        }
    }

    // (b) the reverse: 8-bit LIMITED codes viewed with FULL selected, and the
    //     10-bit pairs both ways. Decoded on the stamp's own encoding.
    {
        struct Case { int bits, range; bool viewB10, viewLim; const char* name; };
        const Case cases[] = {
            {  8, L, false, false, "8-bit limited viewed full" },
            { 10, F, true,  true,  "10-bit full viewed limited" },
            { 10, L, true,  false, "10-bit limited viewed full" },
            {  8, F, true,  true,  "8-bit full viewed 10-bit limited" },
            { 10, L, false, false, "10-bit limited viewed 8-bit full" },
        };
        for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c)
            for (int n = 95; n <= 99; ++n)
            {
                const Case& k = cases[c];
                const int code = CodeFor(n, k.bits, k.range);
                const CColor r = Stamped(ArrayIndexToGrayLevel(n, 101, false, k.bits == 10, k.range == L),
                                         true, k.bits, k.range, code);
                const double slot = ArrayIndexToGrayLevel(n, 101, false, k.viewB10, k.viewLim);
                const double want = (code - Black(k.bits, k.range)) / Grid(k.bits, k.range);
                char what[96];
                _snprintf_s(what, sizeof(what), _TRUNCATE, "(b) %s, %d%% code %d", k.name, n, code);
                ExpectDrive(what, r, slot, false, k.viewB10, k.viewLim, want);
            }
        // the limited reading named in Color.cpp's comment
        ExpectDrive("(b) limited code 224 viewed full", Stamped(208.0 / 219.0 * 100.0, true, 8, L, 224),
                    95.0, false, false, false, 208.0 / 219.0);
    }

    // (c) Kyle's limited setup: HCFR limited, TV limited, PGenerator output FULL.
    //     The stamp says encodingRange limited, deviceRange full; the level is
    //     decoded on encodingRange, (code - 16) / 219, never code / 255.
    {
        const CColor r = Stamped(208.0 / 219.0 * 100.0, true, 8, L, 224, F);
        ExpectDrive("(c) HCFR limited / PGen full, viewed limited", r, 95.0, false, false, true, 208.0 / 219.0);
        ExpectDrive("(c) HCFR limited / PGen full, viewed full", r, 95.0, false, false, false, 208.0 / 219.0);
        double p = 0.0;
        if (ReadingWireGrayProp(r, &p) && p == 224.0 / 255.0)
            Fail("T43 (c) decoded on deviceRange");
        const CColor r10 = Stamped(832.0 / 876.0 * 100.0, true, 10, L, 896, F);
        ExpectDrive("(c) 10-bit HCFR limited / PGen full", r10, 95.0, false, true, true, 832.0 / 876.0);
    }

    // (d) no stamp falls back: a bare reading, one with a stimulus but no
    //     stamp (a file written between the stimulus and the stamp), noDataColor.
    for (int lim = 0; lim < 2; ++lim)
        for (int b10 = 0; b10 < 2; ++b10)
        {
            const double slot = ArrayIndexToGrayLevel(30, 101, false, b10 != 0, lim != 0);
            CColor bare(0.3, 0.4, 0.5);
            ExpectFallback("(d) no stamp, no stimulus", bare, slot, false, b10 != 0, lim != 0);
            CColor stim(0.3, 0.4, 0.5);
            stim.SetStimulusValue(ColorRGBDisplay(31.0));
            ExpectFallback("(d) a stimulus, no stamp", stim, slot, false, b10 != 0, lim != 0);
            ExpectFallback("(d) noDataColor", noDataColor, slot, false, b10 != 0, lim != 0);
        }

    // (e) realDrive false falls back: the 0.5% and 50.196% (128/255)
    //     placeholders written into a 30% slot - as CSensor stamps them (the
    //     defaults) and with stale code fields beside realDrive false.
    {
        const double slot = ArrayIndexToGrayLevel(30, 101, false, false, true);
        CColor p05(0.3, 0.4, 0.5);
        p05.SetStimulusValue(ColorRGBDisplay(0.5));
        p05.SetWireSetup(CColorWireSetup());
        ExpectFallback("(e) 0.5% placeholder, default stamp", p05, slot, false, false, true);
        CColor p50(0.3, 0.4, 0.5);
        p50.SetStimulusValue(ColorRGBDisplay(128.0 / 255.0 * 100.0));
        p50.SetWireSetup(CColorWireSetup());
        ExpectFallback("(e) 50.196% placeholder, default stamp", p50, slot, false, false, true);
        ExpectFallback("(e) 50.196% realDrive false with codes",
                       Stamped(128.0 / 255.0 * 100.0, false, 8, F, 128), slot, false, false, true);
        ExpectFallback("(e) 0.5% realDrive false, no codes",
                       Stamped(0.5, false, 0, WIRE_RANGE_UNKNOWN, -1), slot, false, false, true);
    }

    // (f) a real drive with unequal channels is not a gray patch: falls back.
    {
        const double slot = ArrayIndexToGrayLevel(30, 101, false, false, false);
        CColor s(0.3, 0.4, 0.5);
        s.SetStimulusValue(ColorRGBDisplay(30.0, 30.0, 31.0));
        CColorWireSetup w;
        w.realDrive = true; w.wireBits = 8; w.encodingRange = F;
        w.wireCode[0] = w.wireCode[1] = w.wireCode[2] = 77;
        s.SetWireSetup(w);
        ExpectFallback("(f) unequal stimulus, equal codes", s, slot, false, false, false);

        CColor nocode(0.3, 0.4, 0.5);
        nocode.SetStimulusValue(ColorRGBDisplay(30.0, 31.0, 30.0));
        CColorWireSetup wn;
        wn.realDrive = true;
        nocode.SetWireSetup(wn);
        ExpectFallback("(f) unequal stimulus, no codes", nocode, slot, false, false, false);

        CColor c = Stamped(30.0, true, 8, F, 77);
        CColorWireSetup wc = c.GetWireSetup();
        wc.wireCode[2] = 78;
        c.SetWireSetup(wc);
        ExpectFallback("(f) equal stimulus, unequal codes", c, slot, false, false, false);
        wc.wireCode[2] = 77; wc.wireCode[1] = -1;
        c.SetWireSetup(wc);
        ExpectFallback("(f) codes partly unknown", c, slot, false, false, false);
    }

    // (g) a real drive without codes (madVR, Chromecast, overlay): the stimulus
    //     fraction itself. Codes that cannot be decoded: falls back.
    {
        CColor m(0.3, 0.4, 0.5);
        m.SetStimulusValue(ColorRGBDisplay(30.0));
        CColorWireSetup w;
        w.realDrive = true; w.outputMode = 2;
        m.SetWireSetup(w);
        ExpectDrive("(g) madVR, no codes", m, 29.0, false, false, true, 30.0 / 100.0);
        const double slot = ArrayIndexToGrayLevel(30, 101, false, false, false);
        ExpectFallback("(g) codes with unknown bits", Stamped(30.0, true, 0, F, 77), slot, false, false, false);
        ExpectFallback("(g) codes with unknown range", Stamped(30.0, true, 8, WIRE_RANGE_UNKNOWN, 77), slot, false, false, false);
        ExpectFallback("(g) codes with 12 bits", Stamped(30.0, true, 12, F, 1229), slot, false, false, false);
    }

    // (h) on the setup it was measured on, a stamped reading is described bit
    //     for bit as before: every nominal 0..100 on all four grids.
    {
        int bad = 0;
        for (int b = 0; b < 2; ++b)
            for (int g = 0; g < 2; ++g)
                for (int n = 0; n <= 100; ++n)
                {
                    const bool b10 = b != 0, lim = g != 0;
                    const int bits = b10 ? 10 : 8, range = lim ? L : F;
                    const double slot = ArrayIndexToGrayLevel(n, 101, false, b10, lim);
                    const CColor r = Stamped(slot, true, bits, range, CodeFor(n, bits, range));
                    if (GraySeriesLevel(OneSlot(&r, n, false, b10, lim), 0) != slot
                        || ReadingGrayProp(r, slot, false, b10, lim) != GrayLevelToGrayProp(slot, false, b10, lim))
                    {
                        if (bad++ < 5)
                            Fail("T43 (h) %d%% on %d-bit %s: not the grid's value bit for bit", n, bits, lim ? "limited" : "full");
                    }
                }
        if (bad > 5)
            Fail("T43 (h) %d mismatches in all", bad);
    }

    // (i) the two slot forms the sites used are one value: ArrayIndexToGrayLevel
    //     ( n, 101 ) == GrayLevelToGrayProp ( n ) * 100, every n, grid and
    //     rounding (the near-black/white charts used the second form).
    {
        int bad = 0;
        for (int rd = 0; rd < 2; ++rd)
            for (int b = 0; b < 2; ++b)
                for (int g = 0; g < 2; ++g)
                    for (int n = 0; n <= 101; ++n)
                        if (ArrayIndexToGrayLevel(n, 101, rd != 0, b != 0, g != 0)
                            != GrayLevelToGrayProp((double)n, rd != 0, b != 0, g != 0) * 100.)
                            ++bad;
        if (bad)
            Fail("T43 (i) %d slot levels differ between the two forms", bad);
    }

    // (j) the accessor core pairs slot i with reading i, and slot i is the
    //     sweep's level for that series.
    {
        // near white, 5 slots, clip column 99 (not 101), 8-bit full wire viewed
        // limited; reading 2 has no stamp, reading 3 a placeholder.
        CColor nw[5];
        const int clip = 99, size = 5;
        for (int i = 0; i < size; ++i)
        {
            const int n = clip - size + i;
            nw[i] = Stamped(ArrayIndexToGrayLevel(n, 101, false, false, false), true, 8, F, CodeFor(n, 8, F));
        }
        nw[2] = CColor(0.3, 0.4, 0.5);
        nw[3] = Stamped(0.5, false, 0, WIRE_RANGE_UNKNOWN, -1);
        GraySeriesView v;
        v.series = GRAY_SERIES_NEAR_WHITE; v.readings = nw; v.size = size;
        v.nominalLevels = NULL; v.nominalCount = 0; v.nearWhiteClipCol = clip; v.pqNearBlack = false;
        v.bUseRoundDown = false; v.b10bit = false; v.is16_235 = true;
        for (int i = 0; i < size; ++i)
        {
            const int n = clip - size + i;
            const double slot = ArrayIndexToGrayLevel(n, 101, false, false, true);
            if (GraySeriesSlotLevel(v, i) != slot)
                Fail("T43 (j) near white slot %d: %.17g, expected %d%% on the grid (%.17g)", i, GraySeriesSlotLevel(v, i), n, slot);
            const bool stamped = (i != 2 && i != 3);
            const double wantProp = stamped ? CodeFor(n, 8, F) / 255.0 : GrayLevelToGrayProp(slot, false, false, true);
            const double wantLvl = stamped ? CodeFor(n, 8, F) / 255.0 * 100.0 : slot;
            double p;
            if (GraySeriesDriveProp(v, i, &p) != stamped)
                Fail("T43 (j) near white %d: drive %s, expected %s", i, stamped ? "absent" : "taken", stamped ? "taken" : "absent");
            if (GraySeriesLevel(v, i) != wantLvl)
                Fail("T43 (j) near white %d: level %.17g, expected %.17g", i, GraySeriesLevel(v, i), wantLvl);
            if (GraySeriesProp(v, i) != wantProp)
                Fail("T43 (j) near white %d: prop %.17g, expected %.17g", i, GraySeriesProp(v, i), wantProp);
        }
        double p;
        if (GraySeriesDriveProp(v, -1, &p) || GraySeriesDriveProp(v, size, &p))
            Fail("T43 (j) a drive outside the series");

        // near black in PQ: slot i is 2i%, reading i its own drive.
        CColor nb[4];
        for (int i = 0; i < 4; ++i)
            nb[i] = Stamped(ArrayIndexToGrayLevel(2 * i, 101, false, true, false), true, 10, F, CodeFor(2 * i, 10, F));
        nb[1] = CColor(0.3, 0.4, 0.5);
        GraySeriesView b = v;
        b.series = GRAY_SERIES_NEAR_BLACK; b.readings = nb; b.size = 4; b.pqNearBlack = true;
        b.b10bit = true; b.is16_235 = true;
        for (int i = 0; i < 4; ++i)
        {
            const double slot = ArrayIndexToGrayLevel(2 * i, 101, false, true, true);
            if (GraySeriesSlotLevel(b, i) != slot)
                Fail("T43 (j) PQ near black slot %d: not %d%%", i, 2 * i);
            const double want = (i == 1) ? slot : CodeFor(2 * i, 10, F) / 1023.0 * 100.0;
            if (GraySeriesLevel(b, i) != want)
                Fail("T43 (j) PQ near black %d: level %.17g, expected %.17g", i, GraySeriesLevel(b, i), want);
        }
        b.pqNearBlack = false;
        if (GraySeriesSlotLevel(b, 3) != ArrayIndexToGrayLevel(3, 101, false, true, true))
            Fail("T43 (j) SDR near black slot 3: not 3%%");

        // grayscale with nominal levels per slot, and the even ramp without them.
        const double nominal[5] = { 0.0, 10.0, 30.0, 75.0, 100.0 };
        CColor gs[5];
        for (int i = 0; i < 5; ++i)
            gs[i] = CColor(0.3, 0.4, 0.5);
        gs[3] = Stamped(GrayLevelToGrayProp(75.0, false, false, false) * 100.0, true, 8, F, CodeFor(75, 8, F));
        GraySeriesView g = v;
        g.series = GRAY_SERIES_GRAYSCALE; g.readings = gs; g.size = 5;
        g.nominalLevels = nominal; g.nominalCount = 5; g.b10bit = false; g.is16_235 = true;
        for (int i = 0; i < 5; ++i)
        {
            const double slot = GrayLevelToGrayProp(nominal[i], false, false, true) * 100.0;
            if (GraySeriesSlotLevel(g, i) != slot || GrayScaleSlotLevel(i, 5, nominal, 5, false, false, true) != slot)
                Fail("T43 (j) grayscale slot %d: not the nominal %.0f%% on the grid", i, nominal[i]);
            const double want = (i == 3) ? CodeFor(75, 8, F) / 255.0 * 100.0 : slot;
            if (GraySeriesLevel(g, i) != want)
                Fail("T43 (j) grayscale %d: level %.17g, expected %.17g", i, GraySeriesLevel(g, i), want);
        }
        g.nominalCount = 4;		// a count mismatch: the even ramp, as GetGrayPercent
        for (int i = 0; i < 5; ++i)
            if (GraySeriesSlotLevel(g, i) != ArrayIndexToGrayLevel(i, 5, false, false, true))
                Fail("T43 (j) grayscale slot %d without nominal levels: not the even ramp", i);
        g.nominalLevels = NULL; g.nominalCount = 5;
        if (GraySeriesSlotLevel(g, 2) != ArrayIndexToGrayLevel(2, 5, false, false, true))
            Fail("T43 (j) grayscale slot 2 with no nominal array: not the even ramp");
    }
    // (k) GraySeriesSlotProp, the grid's reference-Y cells' slot (GetItemText):
    //     bit for bit main's GrayLevelToGrayProp ( (double)(i)*(PQ?2:1) ) for near
    //     black and GrayLevelToGrayProp ( (double)(i + clip - size) ) for near white,
    //     on all four grids, with and without round-down.
    {
        int bad = 0;
        CColor nb[51], nw[11];
        for (int rd = 0; rd < 2; ++rd)
            for (int b = 0; b < 2; ++b)
                for (int g = 0; g < 2; ++g)
                    for (int pq = 0; pq < 2; ++pq)
                    {
                        GraySeriesView vb = OneSlot(nb, 0, rd != 0, b != 0, g != 0);
                        vb.series = GRAY_SERIES_NEAR_BLACK; vb.size = 51; vb.pqNearBlack = (pq != 0);
                        for (int i = 0; i < 51; ++i)
                            if (GraySeriesSlotProp(vb, i) != GrayLevelToGrayProp((double)i * (pq ? 2 : 1), rd != 0, b != 0, g != 0)) ++bad;
                        const int clips[2] = { 101, 76 };
                        for (int c = 0; c < 2; ++c)
                        {
                            GraySeriesView vw = vb;
                            vw.series = GRAY_SERIES_NEAR_WHITE; vw.readings = nw; vw.size = 11; vw.nearWhiteClipCol = clips[c];
                            for (int i = 0; i < 11; ++i)
                                if (GraySeriesSlotProp(vw, i) != GrayLevelToGrayProp((double)(i + clips[c] - 11), rd != 0, b != 0, g != 0)) ++bad;
                        }
                    }
        if (bad)
            Fail("T43 (k) %d GraySeriesSlotProp values differ from main's single-truncation expressions", bad);
        // and under round-down it differs from GraySeriesProp's fallback where main's
        // two truncations differ (219 grid, near black 5%: 10/219 against 9/219)
        GraySeriesView vb = OneSlot(nb, 0, true, false, true);
        vb.series = GRAY_SERIES_NEAR_BLACK; vb.size = 51;
        if (GraySeriesSlotProp(vb, 5) != 10.0 / 219.0 || GraySeriesProp(vb, 5) != 9.0 / 219.0)
            Fail("T43 (k) round-down 5%% on 219: slot prop %.17g (want 10/219), series prop %.17g (want 9/219)",
                 GraySeriesSlotProp(vb, 5), GraySeriesProp(vb, 5));
    }
}

//////////////////////////////////////////////////////////////////////////
// T45 - issue #256: order independence of the spectral-correction sync (pure
// assertion, no golden file).
//
// The property: what a session ends with - the readings, the record, the path, the
// raw-less state, what the driver applies - must not depend on whether the first
// Init of the session is a measurement's or the sensor sheet's. The
// CSensor/CArgyllSensor glue needs MFC and a meter, so the harness below is a model
// of it that calls the libHCFR rules the app calls (EffectiveSensorMatrix,
// ReadingsCarryUnrecordedDriverCorrection, EffectiveChangedWithoutWalk,
// SpectralRecordFromPathFile, SpectralRecordApplied, SpectralClearAllowed,
// PlainDisplayTypeEntry, RawlessReadingsState) in the app's order:
//   Init         - CArgyllSensor::Init: the start (connectAndStartMeter, which can
//                  fail and let go of the meter), then ApplyCorrectionStep: the Display Type lookup by
//                  text, the entry the driver is put on, the conversion for a meter
//                  whose matrices are not exposed, the path rule, the re-apply,
//                  SyncSpectralRecordFromMeter's branches (R5 included),
//                  EnforceRecordApplied, the booking outside the sheet.
//   Sheet        - CDataSetDoc::OnConfigureSensor: BeginConfigure (the
//                  measurement-start Init, before the page is filled; a failed one
//                  refuses the sheet), the walk baseline, the page's steps (Display
//                  Type, Spectral Type, Calibrate, Clear - enabled as the page last
//                  refreshed it), OK or Cancel, and the walk of the stored readings.
// Every scenario of the #256 reviews runs in three orders - measurement first,
// sheet first, and a Calibrate-then-Cancel sheet first - with OK, Cancel and Clear,
// and the orders must end identical. Each scenario also runs in main's order (no
// Init at BeginConfigure, nothing booked at a measurement, none of the rules), where
// the reviews say it differs or breaks an invariant: the harness asserts that it
// does, so it is shown to detect the defect. (a) is that table; (b)-(h) pin the
// rules as tables. Readings are column vectors (the correction acts first).
//////////////////////////////////////////////////////////////////////////
namespace T45Order
{
    static Matrix Vec(double x, double y, double z)
    {
        Matrix v(0.0, 3, 1);
        v(0,0) = x; v(1,0) = y; v(2,0) = z;
        return v;
    }

    static Matrix I3() { return Matrix::IdentityMatrix(3); }

    // A deterministic, diagonally dominant correction for (source, unit, observer):
    // what the driver's own fit gives for a ccss/ccmx entry or a browsed file on one
    // unit under one observer. Different arguments give different matrices.
    static Matrix CorrFor(const std::string& source, const std::string& unit, int observer)
    {
        const std::string key = source + "|" + unit + "|" + std::to_string(observer);
        unsigned long long h = Fnv1a64(key.data(), key.size());
        Matrix m = I3();
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
            {
                h = h * 6364136223846793005ULL + 1442695040888963407ULL;
                const double u = (double)((h >> 33) & 0xFFFF) / 65535.0 - 0.5;
                m(i,j) += ( i == j ? 0.08 : 0.03 ) * u;
            }
        return m;
    }

    static Matrix Fcmm()   { return T32Spectral::M3(0.98, 0.03, -0.01,  0.02, 1.01, 0.005,  -0.004, 0.012, 1.05); }
    static Matrix Native() { return Vec(41.2, 43.7, 47.9); }

    // hasBase: the driver finds the entry's base (deriveDisplayTypeCalibration
    // gets past its list checks).
    struct Entry { std::string text; bool corrected; bool hasBase; };
    // content "" = not on disk; parses false = read and hashed, but not a sample.
    struct DiskFile { std::string path; std::string content; bool parses; };

    // The meter and the disk.
    struct World
    {
        bool exposed;                   // canExposeCalibrationMatrices (the i1d3)
        bool holdsSamples;              // doesMeterSupportSpectralSamples (inst2_ccss)
        bool startFails;                // connectAndStartMeter fails (meter unplugged)
        int failObs;                    // ... fails for this Spectral Type only (-1 none)
        int throwEntry;                 // setDisplayType throws for this entry (-1 none):
                                        // the DTP94's set_disp_type is a device command
        bool wrapperDead;               // another document's failed start deleted the
                                        // shared wrapper's driver
        bool needsCalibration;          // a reading asks for a manual calibration
        bool calFails;                  // ... and the calibration always fails
        bool unplugInSheet;             // the meter is unplugged while the sheet is open
        bool calFailsOnce;              // the first calibration fails, the next ones succeed
        std::string serial;
        std::vector<Entry> list;
        std::vector<DiskFile> disk;
        World() : exposed(true), holdsSamples(true), startFails(false), failObs(-1), throwEntry(-1),
                  wrapperDead(false), needsCalibration(false), calFails(false), unplugInSheet(false), calFailsOnce(false), serial("A") {}
        const DiskFile* File(const std::string& path) const
        {
            for (size_t i = 0; i < disk.size(); ++i)
                if (disk[i].path == path && !disk[i].content.empty())
                    return &disk[i];
            return NULL;
        }
        int Find(const std::string& text) const
        {
            for (size_t i = 0; i < list.size(); ++i)
                if (list[i].text == text)
                    return (int)i;
            return -1;
        }
        int Plain(int stored) const
        {
            std::vector<bool> corrected;
            for (size_t i = 0; i < list.size(); ++i)
                corrected.push_back(list[i].corrected);
            return PlainDisplayTypeEntry(stored, corrected);
        }
    };

    static unsigned long long HashOf(const std::string& content)
    {
        return Fnv1a64(content.data(), content.size());
    }

    // CSensor + CArgyllSensor correction state, the settings the page commits, and
    // the driver state that matters here.
    struct Sensor
    {
        int method, fitBasis;
        SpectralCorrectionRecord rec;
        Matrix matrix;
        std::string path;
        RawlessReadingsState rawless;
        int displayType, observer;          // members (committed settings)
        int driverObserver;                 // what the driver was last started with
        int driverEntry;                    // the list entry the driver is on (-1 none)
        int dtBaseFor;                      // m_dtBaseFor: the entry the last derive succeeded for
        std::string driverSample;           // what the driver applies itself ("" none)
        bool cacheValid; std::string cachePath, cacheContent;
        bool pathCheckedValid; std::string pathCheckedPath; unsigned long long pathCheckedHash;
        bool pathDropPending;
        bool entryMissing;                  // m_displayTypeRecordEntryMissing
        bool meterGone;                     // a failed Init set m_meter = 0
        bool inSheet;
        bool uiClear;                       // the Clear button as the page last refreshed it
        bool uiBrowse;                      // the Browse button as the page last refreshed it
        bool uiCalibrate;                   // the Calibrate button as the page last refreshed it
        bool hasRow;                        // the page built its spectral row (status, Browse, Clear)
        bool measureValid;                  // m_isMeasureValid: persists between readings
        SpectralCorrectionRecord recAtLoss; // the record when the meter was lost
        Sensor() : method(CALIB_HCFR_DEFAULT), fitBasis(FIT_BASIS_NATIVE), matrix(I3()), displayType(0),
                   observer(3), driverObserver(3), driverEntry(-1), dtBaseFor(-1), cacheValid(false), pathCheckedValid(false),
                   pathCheckedHash(0), pathDropPending(false), entryMissing(false), meterGone(false),
                   inSheet(false), uiClear(false), uiBrowse(false), uiCalibrate(false), hasRow(false), measureValid(true) {}
        Matrix Effective() const { return EffectiveSensorMatrix(method, fitBasis, rec, matrix); }
        bool UsesSpectral() const { return method == CALIB_SPECTRAL_SAMPLE; }
    };

    // One raw-less reading (no raw value: 4.2.2 and earlier, or edited) and,
    // optionally, one raw-carrying reading. unrecorded: the correction the readings
    // carry that no record describes (what the 4.2.2 driver applied).
    struct Doc
    {
        Sensor s;
        Matrix rawless;
        bool hasRaw;
        Matrix raw, rawValue;
        Matrix unrecorded;
        std::vector<int> clearAllowed;      // what the page's Clear did, each time it was pressed
        bool crashed;                       // main: a lost meter dereferenced, or a throw uncaught
        int messages, maxMessages;          // messages in the current user action, and the most
        bool refusedSilently;               // the sheet refused with no message
        bool silentClick;                   // an enabled button clicked, nothing done, nothing shown
        bool settingsDroppedSilently;       // settings confirmed with OK, not kept, not told
        bool loopedForever;                 // a reading that retried a failing calibration
        int iniObserver;                    // the INI's Spectral Type (new documents, a restart)
        int notices;                        // NOT_KEPT / REJECTED notices shown
        bool silentRevert;                  // a commit reverted settings and said nothing
        bool pageStale;                     // the page shows settings the sensor does not have
        bool calFailedOnce;
        Doc() : rawless(Native()), hasRaw(false), raw(Native()), rawValue(Native()), unrecorded(I3()),
                crashed(false), messages(0), maxMessages(0), refusedSilently(false), silentClick(false),
                settingsDroppedSilently(false), loopedForever(false), iniObserver(3),
                notices(0), silentRevert(false), pageStale(false), calFailedOnce(false) {}
    };

    // The order a session runs in. Branch: the step at BeginConfigure on a started
    // driver (else owed to the first change in the sheet), the booking outside the
    // sheet, the rules (the path rule, SpectralRecordApplied, R5, the Clear rule, the
    // refresh after Calibrate). Main: none (origin/main before #256).
    struct Structure { bool presync, books, rules; const char* name; };
    static const Structure kBranch = { true,  true,  true,  "branch" };
    // MeasureColorInternal stops when Calibrate fails (r6#2).
    static const bool kStopOnFailedCalibration = true;
    // RefreshSpectralStatus sets Calibrate before its early return for a page with no
    // spectral row (r7#3).
    static const bool kCalibrateRefreshBeforeRow = true;
    // A failed reading says so: SetMeasureValidity(FALSE) (r7#1).
    static const bool kFailedReadingInvalid = true;
    // CommitPageSettings writes the INI only after the start succeeds and puts the
    // old settings back when it fails (r7#2).
    static const bool kIniAfterStart = true;
    // Each reading starts valid (MeasureColorInternal's SetMeasureValidity(TRUE)) (r8#2).
    static const bool kValidityReset = true;
    // Ignore after an invalid reading stores noDataColor, not the clamped reading (r8#1).
    static const bool kIgnoreStoresNoData = true;
    // A commit that reverts settings says so, and the page shows the sensor's settings
    // again (r8#3).
    static const bool kNoticeOnRevert = true;
    static const bool kResyncPage = true;
    static const Structure kMain   = { false, false, false, "main" };

    // CSensor::ClearSpectralCorrection
    static void ClearRecord(Sensor& s)
    {
        s.rec.Clear();
        if (s.UsesSpectral())
        {
            s.matrix = I3();
            s.method = CALIB_HCFR_DEFAULT;
            s.fitBasis = FIT_BASIS_NATIVE;
        }
    }

    // CArgyllSensor::ClearSpectralCorrection
    static void ClearArgyll(Sensor& s, const World& w)
    {
        if (!w.exposed)
            s.driverSample = ( s.driverEntry >= 0 && w.list[s.driverEntry].corrected )
                           ? "entry:" + w.list[s.driverEntry].text : "";
        s.path.clear();
        s.cacheValid = false;
        ClearRecord(s);
    }

    // DropPathNotRecordFile
    static void DropPath(Sensor& s, const World& w)
    {
        s.path.clear();
        s.cacheValid = false;
        s.pathCheckedValid = false;
        s.pathDropPending = true;
        if (!w.exposed)
            s.driverSample = ( s.driverEntry >= 0 && w.list[s.driverEntry].corrected )
                           ? "entry:" + w.list[s.driverEntry].text : "";
    }

    // KeepPathIfRecordFile
    static bool KeepPath(Sensor& s, const World& w, const Structure& st, bool hashed, unsigned long long hash)
    {
        if (!st.rules || SpectralRecordFromPathFile(s.rec, hashed, hash))
        {
            if (hashed)
            {
                s.pathCheckedPath = s.path;
                s.pathCheckedHash = hash;
                s.pathCheckedValid = true;
            }
            return true;
        }
        DropPath(s, w);
        return false;
    }

    // MakeFileRecord's identity fields
    static SpectralCorrectionRecord FileRecord(const Sensor& s, const World& w, const std::string& content)
    {
        SpectralCorrectionRecord r;
        r.source = SPECTRAL_SOURCE_FILE;
        r.description = content;
        r.fileHash = HashOf(content);
        r.observer = s.driverObserver;
        r.meterSerial = w.serial;
        return r;
    }

    // SyncSpectralRecordFromMeter (the R3 question answered OK).
    static void Sync(Sensor& s, const World& w, const Structure& st, bool dtChanged)
    {
        if (!w.exposed || s.entryMissing)
            return;
        const int obs = s.driverObserver;
        const Entry& e = w.list[s.displayType];
        if (e.corrected)
        {
            if (s.rec.source == SPECTRAL_SOURCE_DISPLAY_TYPE && s.rec.description == e.text
             && s.rec.meterSerial == w.serial && s.rec.observer == obs && s.dtBaseFor == s.displayType)
            {
                s.driverEntry = w.Plain(-1);                // DisplayTypeRecordIsCurrent: the base
                return;
            }
            if (!e.hasBase)
            {
                // The derive stops before its list checks. R5 (rules): the driver is
                // put on a plain entry; main left it on the corrected entry.
                if (st.rules)
                    s.driverEntry = w.Plain(s.displayType);
                if (s.rec.source == SPECTRAL_SOURCE_DISPLAY_TYPE && s.rec.description != e.text)
                    ClearRecord(s);
                return;
            }
            s.driverEntry = w.Plain(-1);                    // the derive leaves the base selected
            s.dtBaseFor = s.displayType;
            SpectralCorrectionRecord r;
            r.source = SPECTRAL_SOURCE_DISPLAY_TYPE;
            r.description = e.text;
            r.observer = obs;
            r.meterSerial = w.serial;
            r.correction = CorrFor("entry:" + e.text, w.serial, obs);
            r.basis = FIT_BASIS_NATIVE;
            if (r != s.rec)
            {
                const bool noFit = ( s.method == CALIB_HCFR_DEFAULT && s.matrix.IsIdentity() );
                bool makeMethod = noFit;
                if (!s.UsesSpectral() && dtChanged)
                    makeMethod = true;
                s.rec = r;
                if (s.UsesSpectral() || makeMethod)
                {
                    s.matrix = r.correction;
                    s.method = CALIB_SPECTRAL_SAMPLE;
                    s.fitBasis = FIT_BASIS_NATIVE;
                }
            }
            return;
        }
        if (s.rec.source == SPECTRAL_SOURCE_DISPLAY_TYPE)
        {
            if (dtChanged)
                ClearRecord(s);
            else if (s.path.empty())
                return;
        }
        if (s.path.empty())
            return;
        const bool fileRecord = ( s.rec.source == SPECTRAL_SOURCE_FILE );
        if (fileRecord && s.rec.meterSerial == w.serial && s.rec.observer == obs)
            return;
        const DiskFile* f = w.File(s.path);
        const bool hashed = ( f != NULL );
        if (!KeepPath(s, w, st, hashed, hashed ? HashOf(f->content) : 0))
            return;
        if (!hashed || !f->parses)
            return;
        SpectralCorrectionRecord r = FileRecord(s, w, f->content);
        r.correction = CorrFor("file:" + f->content, w.serial, obs);
        r.basis = FIT_BASIS_NATIVE;
        const bool noFit = ( s.method == CALIB_HCFR_DEFAULT && s.matrix.IsIdentity() );
        s.rec = r;
        if (s.UsesSpectral() || ( !fileRecord && noFit ))
        {
            s.matrix = r.correction;
            s.method = CALIB_SPECTRAL_SAMPLE;
            s.fitBasis = FIT_BASIS_NATIVE;
        }
    }

    // ReapplyPathSample (also CancelConfigure's way back to the snapshot's path)
    static void Reapply(Sensor& s, const World& w, const Structure& st)
    {
        if (!w.holdsSamples)
            return;                                         // the re-apply warning; nothing loaded
        bool ok = s.cacheValid && s.cachePath == s.path;
        bool dropped = false;
        if (!ok)
        {
            const DiskFile* f = w.File(s.path);
            const bool hashed = ( f != NULL );
            if (!KeepPath(s, w, st, hashed, hashed ? HashOf(f->content) : 0))
                dropped = true;
            else if (hashed && f->parses)
            {
                s.cacheContent = f->content;
                s.cachePath = s.path;
                s.cacheValid = true;
                ok = true;
            }
        }
        if (ok)
            s.driverSample = s.cacheContent;
        if (!dropped && ok && s.rec.source != SPECTRAL_SOURCE_FILE)
        {
            SpectralCorrectionRecord r = FileRecord(s, w, s.cacheContent);
            r.basis = FIT_BASIS_DRIVER_CORRECTED;
            s.rec = r;
            if (s.method == CALIB_HCFR_DEFAULT && s.matrix.IsIdentity())
            {
                s.method = CALIB_SPECTRAL_SAMPLE;
                s.fitBasis = r.basis;
            }
        }
    }

    // EnforceRecordApplied
    static void Enforce(Sensor& s, const World& w, const Structure& st, bool driverAppliedRecord)
    {
        s.pathDropPending = false;
        if (!st.rules)
            return;
        std::string entry;
        if (!w.exposed && s.displayType >= 0 && w.list[s.displayType].corrected)
            entry = w.list[s.displayType].text;
        if (SpectralRecordApplied(s.rec, w.exposed, w.holdsSamples, !s.path.empty(), entry))
            return;
        const bool wasMethod = s.UsesSpectral();
        ClearRecord(s);
        if (driverAppliedRecord && wasMethod)
            s.fitBasis = FIT_BASIS_DRIVER_CORRECTED;
    }

    // CArgyllSensor::ApplyCorrectionStep
    static bool Step(Doc& d, const World& w, const Structure& st, bool dtChanged)
    {
        Sensor& s = d.s;
        const Matrix before = s.Effective();
        const bool through = ReadingsCarryUnrecordedDriverCorrection(s.rec, s.fitBasis);
        const bool driverAppliedRecord = s.rec.IsPresent() && s.rec.basis != FIT_BASIS_NATIVE;
        // The Display Type record anchored to its entry's text (exposed meter).
        s.entryMissing = false;
        int plain = -1;
        if (w.exposed && s.rec.source == SPECTRAL_SOURCE_DISPLAY_TYPE && !dtChanged
         && w.list[s.displayType].text != s.rec.description)
        {
            const int ix = w.Find(s.rec.description);
            if (ix >= 0)
            {
                s.displayType = ix;
                s.dtBaseFor = -1;
            }
            else
            {
                s.entryMissing = true;
                plain = w.Plain(s.displayType);
                if (plain >= 0)
                    s.displayType = plain;
            }
        }
        // The entry the driver is put on (setDisplayType). It throws for the entry the
        // device rejects: the step stops here, after the repair and the lookup.
        if (( s.entryMissing ? plain : s.displayType ) == w.throwEntry)
            return false;
        s.driverEntry = s.entryMissing ? plain : s.displayType;
        if (!w.exposed)
            s.driverSample = w.list[s.displayType].corrected ? "entry:" + w.list[s.displayType].text : "";
        // The conversion for a meter whose matrices are not exposed.
        if (!w.exposed && s.rec.IsPresent() && s.rec.basis == FIT_BASIS_NATIVE)
        {
            s.rec.correction = I3();
            s.rec.basis = FIT_BASIS_DRIVER_CORRECTED;
            if (s.UsesSpectral())
            {
                s.matrix = I3();
                s.fitBasis = FIT_BASIS_DRIVER_CORRECTED;
            }
        }
        // CheckPathAgainstRecord
        if (st.rules && !s.path.empty() && s.rec.source == SPECTRAL_SOURCE_FILE && s.rec.fileHash != 0
         && !( s.pathCheckedValid && s.pathCheckedPath == s.path && s.pathCheckedHash == s.rec.fileHash ))
        {
            const DiskFile* f = w.File(s.path);
            KeepPath(s, w, st, f != NULL, f ? HashOf(f->content) : 0);
        }
        if (!s.path.empty() && !w.exposed)
            Reapply(s, w, st);
        Sync(s, w, st, dtChanged);
        Enforce(s, w, st, driverAppliedRecord);
        if (st.books && !s.inSheet)
            EffectiveChangedWithoutWalk(s.rawless, before, s.Effective(), through);
        return true;
    }

    static void Message(Doc& d)
    {
        ++d.messages;
        if (d.messages > d.maxMessages)
            d.maxMessages = d.messages;
    }

    // CArgyllSensor::Init: the start, then the step.
    // Main: no NULL guard, no isMeterStillValid, no catch - a lost meter or a throw
    // ends in a crash.
    // CArgyllSensor::InitReporting
    enum InitResult { INIT_OK, INIT_FAILED_ANNOUNCED, INIT_FAILED_SILENT };
    static int InitR(Doc& d, const World& w, const Structure& st, bool dtChanged)
    {
        Sensor& s = d.s;
        if (s.meterGone || w.wrapperDead)
        {
            if (!st.rules)
                d.crashed = true;
            if (!s.meterGone)
                s.recAtLoss = s.rec;
            s.meterGone = true;                     // the NULL guard / isMeterStillValid
            return INIT_FAILED_SILENT;              // nothing shown
        }
        if (w.startFails || s.observer == w.failObs || ( w.unplugInSheet && s.inSheet ))
        {
            s.recAtLoss = s.rec;
            s.meterGone = true;                     // m_meter = 0 (the driver is deleted)
            s.driverSample.clear();
            Message(d);                             // the wrapper's box or HCFR's, once
            return INIT_FAILED_ANNOUNCED;
        }
        s.driverObserver = s.observer;
        if (!Step(d, w, st, dtChanged))
        {
            if (!st.rules)
                d.crashed = true;                   // the throw escapes the handler
            Message(d);                             // the wrapper's box (errorBoxShown)
            return INIT_FAILED_ANNOUNCED;
        }
        return INIT_OK;
    }
    static bool Init(Doc& d, const World& w, const Structure& st, bool dtChanged)
    {
        return InitR(d, w, st, dtChanged) == INIT_OK;
    }

    // CArgyllSensor::Calibrate: false when it did not calibrate. inSheet: from the
    // page's button (main: a throw there is uncaught); else from a measurement
    // (main: the throw ends the measurement).
    static bool Calibrate(Doc& d, const World& w, const Structure& st, bool fromPage)
    {
        Sensor& s = d.s;
        if (st.rules && s.meterGone)
            return false;
        if (!Init(d, w, st, false))
        {
            if (!st.rules && !d.crashed)
                Message(d);                         // main: "Meter failed init check"
            return false;
        }
        if (w.calFails || ( w.calFailsOnce && !d.calFailedOnce ))
        {
            d.calFailedOnce = true;
            Message(d);                             // the wrapper's "Calibration Failed!"
            if (!st.rules && fromPage)
                d.crashed = true;                   // main: uncaught in the handler
            return false;
        }
        return true;
    }

    // The CSensor + CArgyllSensor snapshot, and CancelConfigure.
    struct Snap { Sensor s; };
    static void Cancel(Sensor& s, const World& w, const Structure& st, const Snap& snap, bool reapply)
    {
        bool restoreSample = false;
        if (s.path != snap.s.path)
        {
            if (snap.s.path.empty())
                s.driverSample.clear();
            s.path = snap.s.path;
            s.cacheValid = false;
            restoreSample = reapply && !s.path.empty() && !w.exposed;
        }
        s.displayType = snap.s.displayType;
        s.observer = snap.s.observer;
        s.method = snap.s.method;
        s.matrix = snap.s.matrix;
        s.rec = snap.s.rec;
        s.fitBasis = snap.s.fitBasis;
        if (restoreSample)
        {
            const bool driverAppliedRecord = s.rec.IsPresent() && s.rec.basis != FIT_BASIS_NATIVE;
            if (st.rules)
                Reapply(s, w, st);
            else
            {
                const DiskFile* f = w.File(s.path);
                if (f && f->parses)
                    s.driverSample = f->content;
            }
            Enforce(s, w, st, driverAppliedRecord);
        }
    }

    static bool CorrectionChanged(const Sensor& s, const Snap& snap)
    {
        return s.method != snap.s.method || s.matrix != snap.s.matrix || s.fitBasis != snap.s.fitBasis
            || s.rec != snap.s.rec || s.path != snap.s.path;
    }

    enum StepKind { SET_DT, SET_OBS, CALIBRATE, CLEAR, BROWSE };
    struct Step_ { StepKind kind; int value; };
    enum Outcome { OK, CANCEL };

    // SpectralClearAllowedOnPage; main: a FILE record or a stored path.
    static bool ClearAllowed(const Sensor& s, const World& w, const Structure& st)
    {
        if (!st.rules)
            return s.rec.source == SPECTRAL_SOURCE_FILE || !s.path.empty();
        const bool entrySelected = s.rec.source == SPECTRAL_SOURCE_DISPLAY_TYPE
                                && w.list[s.displayType].corrected && w.list[s.displayType].text == s.rec.description;
        return SpectralClearAllowed(s.rec, !s.path.empty(), true, entrySelected);
    }

    // RefreshSpectralStatus
    static void Refresh(Sensor& s, const World& w, const Structure& st)
    {
        if (st.rules && kCalibrateRefreshBeforeRow)
            s.uiCalibrate = !s.meterGone;                   // HasMeter, every page
        if (!s.hasRow)
            return;                                         // no spectral row: nothing more
        if (st.rules && !kCalibrateRefreshBeforeRow)
            s.uiCalibrate = !s.meterGone;
        s.uiClear = ClearAllowed(s, w, st);
        if (st.rules)
            s.uiBrowse = !s.meterGone && w.holdsSamples;    // MeterSupportsSpectralSamples
    }

    // CArgyllSensor::ApplySpectralCorrection (the R3 question answered OK), when the
    // Browse button is enabled and the selected Display Type does not refuse it.
    static void Browse(Doc& d, const World& w, const Structure& st, int pageDT, int file)
    {
        Sensor& s = d.s;
        if (!s.uiBrowse || w.list[pageDT].corrected)
            return;
        const DiskFile& f = w.disk[file];
        if (f.content.empty() || !f.parses)
            return;
        SpectralCorrectionRecord r;
        r.source = SPECTRAL_SOURCE_FILE;
        r.description = f.content;
        r.fileHash = HashOf(f.content);
        r.observer = s.driverObserver;
        r.meterSerial = s.meterGone ? "" : w.serial;
        if (!s.meterGone && w.exposed)
        {
            r.correction = CorrFor("file:" + f.content, w.serial, s.driverObserver);
            r.basis = FIT_BASIS_NATIVE;
        }
        else
        {
            if (!s.meterGone && w.holdsSamples)
                s.driverSample = f.content;         // loadSpectralSample
            r.basis = FIT_BASIS_DRIVER_CORRECTED;   // identity: the driver applies it
        }
        s.path = f.path;
        s.rec = r;
        s.matrix = r.correction;
        s.method = CALIB_SPECTRAL_SAMPLE;
        s.fitBasis = r.basis;
        s.cacheValid = false;
        if (st.rules)
        {
            // Seeded from the bytes just read (ApplySpectralCorrection).
            s.cacheValid = true; s.cachePath = f.path; s.cacheContent = f.content;
            s.pathCheckedValid = true; s.pathCheckedPath = f.path; s.pathCheckedHash = r.fileHash;
        }
    }

    // GetPropertiesSheetValues: a changed setting is committed and re-inits.
    // CArgyllSensor::CommitPageSettings
    static int Commit(Doc& d, const World& w, const Structure& st, int& pageDT, int& pageObs)
    {
        const bool changed = ( pageDT != d.s.displayType || pageObs != d.s.observer );
        if (d.s.meterGone)
        {
            if (!st.rules)
                d.crashed = true;                   // main: m_meter->getMeterName()
            else if (changed)
                Message(d);                         // IDS_ARGYLL_SETTINGS_NOT_KEPT
            if (changed && d.messages == 0 && !d.crashed)
                d.settingsDroppedSilently = true;
            return ( st.rules && changed ) ? INIT_FAILED_ANNOUNCED : INIT_FAILED_SILENT;
        }
        if (!changed)
            return INIT_OK;
        const bool dtChanged = ( pageDT != d.s.displayType );
        const int oldDT = d.s.displayType, oldObs = d.s.observer;
        d.s.displayType = pageDT;
        d.s.observer = pageObs;
        const bool iniFirst = !( st.rules && kIniAfterStart );
        if (iniFirst)
            d.iniObserver = pageObs;                // main: written before the start
        const int r = InitR(d, w, st, dtChanged);
        if (!iniFirst)
        {
            if (r == INIT_OK)
                d.iniObserver = d.s.observer;
            else
            {
                d.s.displayType = oldDT;            // not kept
                d.s.observer = oldObs;
                if (kNoticeOnRevert)
                    ++d.notices;                    // IDS_ARGYLL_SETTINGS_REJECTED(_STOPPED)
                else
                    d.silentRevert = true;
                if (kResyncPage)
                {
                    pageDT = d.s.displayType;       // the page shows the sensor's settings
                    pageObs = d.s.observer;
                }
                else
                    d.pageStale = true;
            }
        }
        return r;
    }

    // CDataSetDoc::OnConfigureSensor
    static void Sheet(Doc& d, const World& w, const Structure& st, const std::vector<Step_>& steps, Outcome out)
    {
        Sensor& s = d.s;
        // CArgyllSensor::BeginConfigure: the measurement-start Init, then Configure,
        // which refuses the sheet when there is no meter, with a message every time
        // (the start's own, or IDS_ARGYLL_METER_UNAVAILABLE). A start that fails with
        // the meter still held opens the sheet with no step applied.
        d.messages = 0;
        if (st.presync)
        {
            const int begin = InitR(d, w, st, false);
            if (s.meterGone)
            {
                // Configure: IDS_ARGYLL_METER_UNAVAILABLE unless the start announced it.
                if (begin != INIT_FAILED_ANNOUNCED)
                    Message(d);
                if (d.messages == 0)
                    d.refusedSilently = true;
                return;                             // OnConfigureSensor: as a Cancel
            }
        }
        else if (s.meterGone || w.startFails || w.wrapperDead)
        {
            // Main: SetPropertiesSheetValues and the page fill reach the meter (a
            // lost one is dereferenced; an unstartable one throws from
            // checkMeterIsInitialized, uncaught).
            d.crashed = true;
            return;
        }
        Snap snap; snap.s = s;
        Matrix mod = s.Effective();                 // SetSensorMatrixMod
        s.inSheet = true;
        // OnInitDialog: the spectral row for a meter that takes samples or a sensor
        // with a correction; the Calibrate button on every page, enabled.
        s.hasRow = w.holdsSamples || s.rec.IsPresent();
        s.uiCalibrate = true;
        s.uiClear = false;
        s.uiBrowse = false;
        if (s.hasRow)
        {
            Refresh(s, w, st);
            s.uiBrowse = !s.meterGone && w.holdsSamples;
        }
        int pageDT = s.displayType, pageObs = s.observer;
        for (size_t i = 0; i < steps.size(); ++i)
        {
            switch (steps[i].kind)
            {
            case SET_DT:  pageDT = steps[i].value; break;
            case SET_OBS: pageObs = steps[i].value; break;
            case BROWSE:
                d.messages = 0;
                Browse(d, w, st, pageDT, steps[i].value);
                break;
            case CALIBRATE:                         // OnCalibrate, when the button is enabled
            {
                if (!s.uiCalibrate)
                    break;
                d.messages = 0;
                const int r = Commit(d, w, st, pageDT, pageObs);
                bool calibrated = false;
                if (!st.rules || r == INIT_OK)      // main calibrated whatever the commit did
                    calibrated = Calibrate(d, w, st, true);
                if (!calibrated && d.messages == 0 && !d.crashed)
                    d.silentClick = true;
                if (st.rules)
                    Refresh(s, w, st);              // main did not refresh here
                break;
            }
            case CLEAR:                             // OnSpectralClear, when the button is enabled
            {
                d.clearAllowed.push_back(s.uiClear ? 1 : 0);
                if (s.uiClear)
                {
                    // CancelConfigureBeforeClear: the snapshot's sample is not put
                    // back into the driver (main: CancelConfigure re-applied it).
                    Cancel(s, w, st, snap, !st.rules);
                    ClearArgyll(s, w);
                    Refresh(s, w, st);
                }
                break;
            }
            }
        }
        if (out == CANCEL)
            Cancel(s, w, st, snap, true);
        else
        {
            d.messages = 0;
            Commit(d, w, st, pageDT, pageObs);       // the OK
        }
        s.inSheet = false;
        if (out == OK && CorrectionChanged(s, snap))
        {
            const Matrix full = s.Effective();
            int nSkipped = 0;
            if (s.rawless.ComposeOntoRawless())
                d.rawless = full * mod.GetInverse() * d.rawless;
            else
                nSkipped = 1;
            s.rawless.SingleMatrixWalked(nSkipped);
            if (d.hasRaw)
                d.rawValue = full * d.raw;
        }
    }

    // One reading (MeasureColorInternal): IsMeasureValid afterwards. Main: a failed
    // calibration threw, and AsyncMeasurer's catch set the reading invalid.
    static bool Reading(Doc& d, const World& w, const Structure& st)
    {
        Sensor& s = d.s;
        if (st.rules && kValidityReset)
            s.measureValid = true;                  // each reading starts valid
        if (s.meterGone)
        {
            if (!st.rules)
            {
                d.crashed = true;                   // main: Init dereferences m_meter
                return false;
            }
            if (kFailedReadingInvalid)
                s.measureValid = false;
            return s.measureValid;
        }
        if (!w.needsCalibration)
            return s.measureValid;
        if (Calibrate(d, w, st, false))
            return s.measureValid;
        if (!st.rules || kFailedReadingInvalid)
            s.measureValid = false;                 // main: AsyncMeasurer's catch
        return s.measureValid;
    }

    // A sweep of nPatches (Measure.cpp's loop): the measurement start, then each
    // patch; an invalid reading shows the Abort/Retry/Ignore box ("An error
    // occurred:" + the error string) and stores nothing; the answer is `answer`
    // (0 abort, 1 retry once then abort, 2 ignore).
    struct SweepResult { int stored, prompts, messages, fakeStored; };
    static SweepResult Sweep(Doc& d, const World& w, const Structure& st, int nPatches, int answer)
    {
        SweepResult r = { 0, 0, 0, 0 };
        d.messages = 0;
        if (!Init(d, w, st, false))
        {
            r.messages = d.messages;                // IDS_ERRINITSENSOR follows (pre-existing)
            return r;
        }
        int retries = 0;
        for (int i = 0; i < nPatches; ++i)
        {
            if (!Reading(d, w, st))
            {
                ++r.prompts;
                if (answer == 0 || ( answer == 1 && retries > 0 ))
                    break;                          // Abort
                if (answer == 1)
                {
                    ++retries;
                    --i;                            // Retry
                }
                // Ignore: the slot holds noDataColor (DropIgnoredReading), not the
                // clamped reading.
                if (!kIgnoreStoresNoData)
                    ++r.fakeStored;
                continue;
            }
            ++r.stored;
        }
        r.messages = d.messages;
        return r;
    }

    static void Measure(Doc& d, const World& w, const Structure& st)
    {
        d.messages = 0;
        if (!Init(d, w, st, false))
            return;                                 // a lost meter: noDataColor
        if (!w.needsCalibration)
            return;
        // MeasureColorInternal: takeReading asks for a manual calibration until one
        // succeeds. A failed Calibrate ends the reading with no data (main: the
        // throw left the reading). Bounded here at 20 so a loop is visible.
        for (int k = 0; k < 20; ++k)
        {
            const bool ok = Calibrate(d, w, st, false);
            if (ok)
                return;
            if (!st.rules)
                return;                             // main: the throw ended it
            if (kStopOnFailedCalibration)
                return;
        }
        d.loopedForever = true;
    }

    static bool NearVec(const Matrix& a, const Matrix& b, double rel)
    {
        double scale = 0.0;
        for (int i = 0; i < a.GetRows(); ++i)
            for (int j = 0; j < a.GetColumns(); ++j)
                scale = fmax(scale, fmax(fabs(a(i,j)), fabs(b(i,j))));
        for (int i = 0; i < a.GetRows(); ++i)
            for (int j = 0; j < a.GetColumns(); ++j)
                if (fabs(a(i,j) - b(i,j)) > rel * scale)
                    return false;
        return true;
    }

    // What differs between two orders' end states ("" when nothing).
    static std::string Diff(const Doc& a, const Doc& b)
    {
        std::string out;
        if (!NearVec(a.rawless, b.rawless, 1e-12)) out += " readings";
        if (a.hasRaw && !NearVec(a.rawValue, b.rawValue, 1e-12)) out += " raw-carrying";
        if (a.s.rec != b.s.rec) out += " record";
        if (a.s.path != b.s.path) out += " path";
        if (a.s.rawless.Get() != b.s.rawless.Get()) out += " raw-less-state";
        if (a.s.method != b.s.method || a.s.fitBasis != b.s.fitBasis || !NearVec(a.s.matrix, b.s.matrix, 1e-12))
            out += " method";
        if (a.s.driverSample != b.s.driverSample || a.s.driverEntry != b.s.driverEntry) out += " driver";
        if (a.s.displayType != b.s.displayType) out += " display-type";
        return out;
    }

    struct Scenario
    {
        const char* id;                 // the finding it reproduces
        std::string what;
        World w;
        Doc d;
        std::vector<Step_> steps;
        Outcome out;
        bool mainDiffers;               // the reviews' claim: main's order differs
        bool mainViolates;              // the reviews' claim: main breaks an invariant below
        std::string wantRecord;         // the record that must stand ("": no check)
        int wantDisplayType;            // the Display Type the user's OK must leave (-1: no check)
        bool clearSeen;                 // the page's Clear must have been enabled once (else no check)
        bool unrecordedTruth;           // the readings keep a correction no derive can express (R5)
        Scenario() : out(OK), mainDiffers(false), mainViolates(false), wantDisplayType(-1), clearSeen(false), unrecordedTruth(false) {}
    };

    // Measurement first: a measurement's Init, the sheet, the next measurement.
    // Sheet first: the sheet, the next measurement. Calibrate-Cancel first: a sheet
    // in which Calibrate ran and that was cancelled, then the sheet, the next
    // measurement (the driver is started, by a sheet, before the sheet under test).
    static std::string RunOrders(const Scenario& sc, const Structure& st, Doc ends[3])
    {
        std::vector<Step_> calibrate(1);
        calibrate[0].kind = CALIBRATE; calibrate[0].value = 0;
        ends[0] = sc.d;
        Measure(ends[0], sc.w, st);
        Sheet(ends[0], sc.w, st, sc.steps, sc.out);
        Measure(ends[0], sc.w, st);
        ends[1] = sc.d;
        Sheet(ends[1], sc.w, st, sc.steps, sc.out);
        Measure(ends[1], sc.w, st);
        ends[2] = sc.d;
        Sheet(ends[2], sc.w, st, calibrate, CANCEL);
        ends[2].clearAllowed.clear();
        Sheet(ends[2], sc.w, st, sc.steps, sc.out);
        Measure(ends[2], sc.w, st);
        std::string out;
        const std::string d1 = Diff(ends[0], ends[1]), d2 = Diff(ends[0], ends[2]);
        if (!d1.empty()) out += " [sheet first:" + d1 + "]";
        if (!d2.empty()) out += " [calibrate-cancel first:" + d2 + "]";
        if (ends[0].clearAllowed != ends[1].clearAllowed || ends[0].clearAllowed != ends[2].clearAllowed)
            out += " [clear-enabled]";
        return out;
    }

    // The i1d3 list: two plain entries and three ccss/ccmx entries, plus an entry
    // "N" whose base the driver does not find.
    static World I1d3(const char* serial)
    {
        World w;
        w.exposed = true;
        w.serial = serial;
        Entry list[] = { { "Non-Refresh", false, true }, { "Refresh", false, true }, { "E", true, true },
                         { "E2", true, true }, { "L", true, true }, { "N", true, false } };
        w.list.assign(list, list + 6);
        DiskFile disk[] = { { "F.ccss", "F1", true }, { "G.ccss", "G1", true }, { "Fr.ccss", "F2", true },
                            { "Bad.ccss", "BAD", false } };
        w.disk.assign(disk, disk + 4);
        return w;
    }

    // A Spyder: its driver takes samples, and lists the same installed entries.
    static World Spyder()
    {
        World w = I1d3("S");
        w.exposed = false;
        return w;
    }

    static std::vector<Step_> Steps(int n, ...)
    {
        std::vector<Step_> v;
        va_list ap;
        va_start(ap, n);
        for (int i = 0; i < n; ++i)
        {
            Step_ s;
            s.kind = (StepKind)va_arg(ap, int);
            s.value = va_arg(ap, int);
            v.push_back(s);
        }
        va_end(ap);
        return v;
    }

    enum { SAME = 0, DIFFERS = 1 };
    enum { HOLDS = 0, VIOLATES = 1 };
    static Scenario& Add(std::vector<Scenario>& v, const char* id, const std::string& what, const World& w, const Doc& d,
                         const std::vector<Step_>& steps, Outcome out, int mainDiffers, int mainViolates)
    {
        Scenario sc;
        sc.id = id; sc.what = what; sc.w = w; sc.d = d; sc.steps = steps; sc.out = out;
        sc.mainDiffers = ( mainDiffers == DIFFERS ); sc.mainViolates = ( mainViolates == VIOLATES );
        v.push_back(sc);
        return v.back();
    }

    static SpectralCorrectionRecord DtRecord(const char* entry, const char* unit, int obs)
    {
        SpectralCorrectionRecord r;
        r.source = SPECTRAL_SOURCE_DISPLAY_TYPE;
        r.description = entry;
        r.observer = obs;
        r.meterSerial = unit;
        r.correction = CorrFor(std::string("entry:") + entry, unit, obs);
        r.basis = FIT_BASIS_NATIVE;
        return r;
    }

    static SpectralCorrectionRecord FileRec(const char* content, const char* unit, int obs, bool driver)
    {
        SpectralCorrectionRecord r;
        r.source = SPECTRAL_SOURCE_FILE;
        r.description = content;
        r.fileHash = HashOf(content);
        r.observer = obs;
        r.meterSerial = unit;
        r.correction = driver ? I3() : CorrFor(std::string("file:") + content, unit, obs);
        r.basis = driver ? FIT_BASIS_DRIVER_CORRECTED : FIT_BASIS_NATIVE;
        return r;
    }

    // A 4.2.2.0 document (v1 sensor stream: method DEFAULT, basis UNKNOWN, no record)
    // whose readings the i1d3 driver corrected with `through` (an entry or a file).
    static Doc V1Doc(const Matrix& fit, int displayType, const std::string& through)
    {
        Doc d;
        d.s.method = CALIB_HCFR_DEFAULT;
        d.s.fitBasis = FIT_BASIS_UNKNOWN;
        d.s.matrix = fit;
        d.s.displayType = displayType;
        d.unrecorded = CorrFor(through, "A", 3);
        d.rawless = fit * d.unrecorded * Native();
        return d;
    }

    // A document whose sensor uses `rec` as the method. For a driver-applied record
    // the model's native reading is the one the driver corrected.
    static Doc SpectralDoc(const SpectralCorrectionRecord& rec, int displayType, const char* path)
    {
        Doc d;
        d.s.method = CALIB_SPECTRAL_SAMPLE;
        d.s.rec = rec;
        d.s.matrix = rec.correction;
        d.s.fitBasis = rec.basis;
        d.s.displayType = displayType;
        d.s.path = path;
        if (rec.basis == FIT_BASIS_DRIVER_CORRECTED)
            d.s.driverSample = rec.source == SPECTRAL_SOURCE_FILE ? rec.description : "entry:" + rec.description;
        d.rawless = rec.correction * Native();
        return d;
    }

    static std::vector<Scenario> Scenarios()
    {
        std::vector<Scenario> v;
        const World a = I1d3("A"), b = I1d3("B"), sp = Spyder();
        const Matrix fits[2] = { I3(), Fcmm() };
        const char* fitName[2] = { "no fit", "FCMM fit" };
        for (int f = 0; f < 2; ++f)
        {
            // R2: a 4.2.2.0 document whose readings the driver corrected with its
            // entry E. The first Init inside the sheet is a setting change at OK or
            // the page's Calibrate; with neither, main runs no Init in the sheet.
            const Doc v1e = V1Doc(fits[f], 2, "entry:E");
            const std::string w1 = std::string("4.2.2 doc on entry E, ") + fitName[f];
            Add(v, "R2", w1 + ", OK",                         a, v1e, Steps(0), OK, SAME, HOLDS);
            Add(v, "R2", w1 + ", Calibrate, OK",              a, v1e, Steps(1, CALIBRATE, 0), OK, DIFFERS, HOLDS);
            Add(v, "R2", w1 + ", Calibrate, Cancel",          a, v1e, Steps(1, CALIBRATE, 0), CANCEL, SAME, HOLDS);
            Add(v, "R2", w1 + ", Spectral Type changed, OK",  a, v1e, Steps(1, SET_OBS, 4), OK, DIFFERS, HOLDS);
            Add(v, "R2", w1 + ", Display Type -> E2, OK",     a, v1e, Steps(1, SET_DT, 3), OK, DIFFERS, HOLDS);
            Add(v, "r2#3", w1 + ", Display Type -> plain, OK", a, v1e, Steps(1, SET_DT, 0), OK, DIFFERS, HOLDS);
            // r1#3: no record, a browsed sample the driver applied, the page's Clear.
            Doc v1f = V1Doc(fits[f], 0, "file:F1");
            v1f.s.path = "F.ccss";
            const std::string w3 = std::string("no record, sample F applied by the driver, ") + fitName[f];
            Add(v, "r1#3", w3 + ", Clear, OK",     a, v1f, Steps(1, CLEAR, 0), OK, DIFFERS, HOLDS).clearSeen = true;
            Add(v, "r1#3", w3 + ", Clear, Cancel", a, v1f, Steps(1, CLEAR, 0), CANCEL, SAME, HOLDS).clearSeen = true;
            Add(v, "r1#3", w3 + ", OK",            a, v1f, Steps(0), OK, SAME, HOLDS);
            // r2#4: entry E selected and applied by the driver, a stored path too.
            Doc v1ep = v1e;
            v1ep.s.path = "F.ccss";
            const std::string w4 = std::string("no record, entry E applied by the driver, path F, ") + fitName[f];
            Add(v, "r2#4", w4 + ", Clear, OK", a, v1ep, Steps(1, CLEAR, 0), OK, DIFFERS, HOLDS);
            // r2#3: entry L applied by the driver, path F, the user leaves L for a plain entry.
            Doc v1l = V1Doc(fits[f], 4, "entry:L");
            v1l.s.path = "F.ccss";
            const std::string w5 = std::string("no record, entry L applied by the driver, path F, ") + fitName[f];
            Add(v, "r2#3", w5 + ", Display Type -> plain, OK", a, v1l, Steps(1, SET_DT, 0), OK, DIFFERS, HOLDS);
            // r1#8: raw XYZ the driver had already corrected (a field-test build
            // document), re-expressed by an Init in the sheet.
            Doc ft = V1Doc(fits[f], 0, "file:F1");
            ft.s.path = "F.ccss";
            ft.hasRaw = true;
            ft.raw = CorrFor("file:F1", "A", 3) * Native();
            ft.rawValue = fits[f] * ft.raw;
            const std::string w8 = std::string("raw already driver-corrected, path F, ") + fitName[f];
            Add(v, "r1#8", w8 + ", Calibrate, OK", a, ft, Steps(1, CALIBRATE, 0), OK, DIFFERS, HOLDS);
            // R5 / gap: the entry the readings went through has no base entry (the
            // derive stops before selecting anything). Main leaves the driver on it.
            const Doc v1n = V1Doc(fits[f], 5, "entry:N");
            const std::string w9 = std::string("4.2.2 doc on entry N (no base entry), ") + fitName[f];
            Add(v, "R5", w9 + ", Calibrate, OK", a, v1n, Steps(1, CALIBRATE, 0), OK, SAME, VIOLATES).unrecordedTruth = true;
        }
        // R5 with a record for that same entry: main applies it twice (driver and HCFR).
        Doc dtN = SpectralDoc(DtRecord("N", "A", 3), 5, "");
        Add(v, "R5", "entry N record, no base entry, Calibrate, OK", a, dtN, Steps(1, CALIBRATE, 0), OK, SAME, VIOLATES);
        // Gap: the record's entry moved to another index (an installed file added),
        // and the record's entry gone from the list. Main: Calibrate's Init moves the
        // Display Type under the page, and OK commits the stale index as a choice.
        World moved = a;
        std::swap(moved.list[2], moved.list[3]);            // E now at 3, E2 at 2
        const Doc dtE = SpectralDoc(DtRecord("E", "A", 3), 2, "");
        Add(v, "gap", "entry E record, E moved to index 3, Calibrate, OK", moved, dtE, Steps(1, CALIBRATE, 0), OK, DIFFERS, HOLDS);
        World gone = a;
        gone.list[2].text = "X";
        Add(v, "gap", "entry E record, E gone from the list, Calibrate, OK", gone, dtE, Steps(1, CALIBRATE, 0), OK, DIFFERS, HOLDS);
        // Gap: a .ccss that hashes but does not parse, as the stored path.
        Doc bad = V1Doc(I3(), 0, "file:F1");
        bad.s.path = "Bad.ccss";
        Add(v, "gap", "no record, path to a file that does not parse, Calibrate, OK", a, bad, Steps(1, CALIBRATE, 0), OK, SAME, HOLDS).unrecordedTruth = true;
        Doc badSpy = SpectralDoc(SpectralCorrectionRecord(), 0, "Bad.ccss");
        badSpy.s.method = CALIB_HCFR_DEFAULT;
        Add(v, "gap", "Spyder, no record, path to a file that does not parse, Calibrate, OK", sp, badSpy, Steps(1, CALIBRATE, 0), OK, SAME, HOLDS);
        // r1#1 / r2#5: unit B connected; the record was derived on unit A.
        const Doc fileA = SpectralDoc(FileRec("F1", "A", 3, false), 0, "F.ccss");
        Add(v, "r1#1", "unit B, FILE record from unit A, plain -> plain, OK", b, fileA, Steps(1, SET_DT, 1), OK, DIFFERS, HOLDS);
        Add(v, "r1#1", "unit B, FILE record from unit A, OK", b, fileA, Steps(0), OK, SAME, HOLDS);
        Add(v, "r1#1", "unit B, FILE record from unit A, Calibrate, Cancel", b, fileA, Steps(1, CALIBRATE, 0), CANCEL, SAME, HOLDS);
        Add(v, "r2#5", "unit B, FILE record from unit A, -> entry E2, OK", b, fileA, Steps(1, SET_DT, 3), OK, DIFFERS, HOLDS);
        const Doc dtA = SpectralDoc(DtRecord("E", "A", 3), 2, "");
        Add(v, "r2#5", "unit B, entry E record from unit A, -> entry E2, OK", b, dtA, Steps(1, SET_DT, 3), OK, DIFFERS, HOLDS);
        Add(v, "r2#5", "unit B, entry E record from unit A, OK", b, dtA, Steps(0), OK, SAME, HOLDS);
        // r2#1: the Spectral Type changed with Calibrate, then Cancel or OK.
        const Doc fileOwn = SpectralDoc(FileRec("F1", "A", 3, false), 0, "F.ccss");
        Add(v, "r2#1", "FILE record, Spectral Type changed, Calibrate, Cancel", a, fileOwn, Steps(2, SET_OBS, 4, CALIBRATE, 0), CANCEL, SAME, HOLDS);
        Add(v, "r2#1", "FILE record, Spectral Type changed, Calibrate, OK", a, fileOwn, Steps(2, SET_OBS, 4, CALIBRATE, 0), OK, SAME, HOLDS);
        // r3#6: Calibrate commits a Display Type that changes what Clear may do; the
        // page's Clear acts on the state after it.
        Add(v, "r3#6", "FILE record, Display Type -> E, Calibrate, Clear, OK", a, fileOwn, Steps(3, SET_DT, 2, CALIBRATE, 0, CLEAR, 0), OK, SAME, HOLDS);
        // r1#2 / r1#5 / r4#4: a Spyder given an i1d3 document's Display Type record.
        // Its own list has an entry at that index, and the driver applies it: the
        // record's own entry (applied), another corrected entry, or a plain one.
        const Doc dtSpy = SpectralDoc(DtRecord("E", "A", 3), 2, "");
        Add(v, "r1#5", "Spyder, i1d3 entry E record, E at that index, OK", sp, dtSpy, Steps(0), OK, SAME, HOLDS);
        Add(v, "r1#2", "Spyder, i1d3 entry E record, E at that index, Calibrate, Clear, OK", sp, dtSpy, Steps(2, CALIBRATE, 0, CLEAR, 0), OK, DIFFERS, HOLDS);
        Add(v, "r1#2", "Spyder, i1d3 entry E record, E at that index, -> plain, OK", sp, dtSpy, Steps(1, SET_DT, 0), OK, DIFFERS, VIOLATES);
        World spOther = sp;
        spOther.list[2].text = "E9";
        Add(v, "r4#4", "Spyder, i1d3 entry E record, another entry at that index, OK", spOther, dtSpy, Steps(0), OK, SAME, VIOLATES);
        World spPlain = sp;
        spPlain.list[2].corrected = false;
        Add(v, "r1#5", "Spyder, i1d3 entry E record, a plain entry at that index, Clear, OK", spPlain, dtSpy, Steps(1, CLEAR, 0), OK, SAME, VIOLATES);
        // R4 on the i1d3: a FILE record from G (a correction file loaded after the
        // Browse of F). Same unit (main: the path names another file than the record)
        // and another unit (main: F derived again in G's place).
        const Doc yOnF = SpectralDoc(FileRec("G1", "A", 3, false), 0, "F.ccss");
        Add(v, "R4", "record from G, path F, same unit, OK", a, yOnF, Steps(0), OK, SAME, VIOLATES).wantRecord = "G1";
        Add(v, "R4", "record from G, path F, unit B, OK", b, yOnF, Steps(0), OK, SAME, VIOLATES).wantRecord = "G1";
        // r1#6 / r2#2: a Spyder whose path names another file than its record's (a
        // record from G; F rewritten in place after its Browse). Main loads it.
        const Doc spyG = SpectralDoc(FileRec("G1", "S", 3, true), 0, "F.ccss");
        Add(v, "r1#6", "Spyder, record from G, path F, OK", sp, spyG, Steps(0), OK, SAME, VIOLATES);
        const Doc spyR = SpectralDoc(FileRec("F1", "S", 3, true), 0, "Fr.ccss");
        Add(v, "r2#2", "Spyder, path file rewritten after its Browse, OK", sp, spyR, Steps(0), OK, SAME, VIOLATES);
        Add(v, "r2#2", "Spyder, path file rewritten, Calibrate, Cancel", sp, spyR, Steps(1, CALIBRATE, 0), CANCEL, SAME, VIOLATES);
        // r3#3: a meter that takes no sample (a DTP94, a Spyder3) given a Spyder
        // document's FILE record and its path.
        World dtp = sp;
        dtp.holdsSamples = false;
        const Doc spyF = SpectralDoc(FileRec("F1", "S", 3, true), 0, "F.ccss");
        Add(v, "r3#3", "meter without samples, Spyder's FILE record, its path, OK", dtp, spyF, Steps(0), OK, SAME, VIOLATES);
        // r4: a start that fails (the meter unplugged): the sheet is refused cleanly, in
        // every order; main dereferences the lost meter or lets the throw escape.
        World dead = a;
        dead.startFails = true;
        Add(v, "r4", "i1d3 start fails, 4.2.2 doc on entry E, Calibrate, OK", dead, V1Doc(I3(), 2, "entry:E"), Steps(1, CALIBRATE, 0), OK, SAME, VIOLATES).unrecordedTruth = true;
        World deadSpy = sp;
        deadSpy.startFails = true;
        Add(v, "r4", "Spyder start fails, its own browsed file, Clear, OK", deadSpy, SpectralDoc(FileRec("F1", "S", 3, true), 0, "F.ccss"), Steps(1, CLEAR, 0), OK, SAME, VIOLATES);
        // r5#1: a start that fails with the meter still held (the DTP94 rejects the stored
        // Display Type) opens the sheet with no step applied; the user picks another
        // Display Type and the OK's commit Init runs with it. The same change made after
        // a measurement in that state ends the same.
        World dtpThrow = sp;
        dtpThrow.holdsSamples = false;
        dtpThrow.throwEntry = 2;
        Scenario& t1 = Add(v, "r5#1", "DTP94 rejects the stored Display Type, -> plain, OK", dtpThrow, V1Doc(I3(), 2, "entry:E"), Steps(1, SET_DT, 0), OK, SAME, VIOLATES);
        t1.unrecordedTruth = true; t1.wantDisplayType = 0;
        Add(v, "r5#1", "DTP94 rejects the stored Display Type, Calibrate, Cancel", dtpThrow, V1Doc(I3(), 2, "entry:E"), Steps(1, CALIBRATE, 0), CANCEL, SAME, VIOLATES).unrecordedTruth = true;
        World i1Throw = a;
        i1Throw.throwEntry = 2;
        Scenario& t3 = Add(v, "r5#1", "i1d3, 4.2.2 doc on E, the select of E throws, -> plain, Calibrate, OK", i1Throw, V1Doc(Fcmm(), 2, "entry:E"), Steps(2, SET_DT, 0, CALIBRATE, 0), OK, SAME, VIOLATES);
        t3.unrecordedTruth = true; t3.wantDisplayType = 0;
        // r5#6: a setting changed that still fails, then Calibrate: one message.
        Add(v, "r5#6", "DTP94 rejects the stored Display Type, Spectral Type changed, Calibrate, Cancel", dtpThrow, V1Doc(I3(), 2, "entry:E"), Steps(2, SET_OBS, 4, CALIBRATE, 0), CANCEL, SAME, VIOLATES).unrecordedTruth = true;
        // r5#7: a throw after the step moved the Display Type (the record's entry found
        // at another index, and that index rejected): the partial step is the same in
        // both orders.
        World movedThrow = moved;
        movedThrow.throwEntry = 3;
        Add(v, "r5#7", "entry E record, E moved to 3, the select of 3 throws, -> plain, OK", movedThrow, dtE, Steps(1, SET_DT, 0), OK, SAME, VIOLATES).wantDisplayType = 0;
        // r5#2: two documents on one meter; the other one's failed start deleted the
        // shared driver. This one's sheet is refused with a message, no dereference.
        World dead2 = a;
        dead2.wrapperDead = true;
        Add(v, "r5#2", "another document's failed start deleted the driver, OK", dead2, fileOwn, Steps(0), OK, SAME, VIOLATES);
        // r5#4: an in-sheet start that fails for the Spectral Type lets go of the meter;
        // Browse is refreshed with the rest and cannot install a record nothing applies.
        World obsFails = a;
        obsFails.failObs = 4;
        Add(v, "r5#4", "Spectral Type the meter fails on, Calibrate, Browse G, OK", obsFails, fileOwn, Steps(3, SET_OBS, 4, CALIBRATE, 0, BROWSE, 1), OK, DIFFERS, VIOLATES);
        // r6#2: a meter whose calibration always fails (an i1Pro whose white tile keeps
        // failing): a reading that asks for it ends with no data and one message, the
        // page's Calibrate shows one message.
        World calBad = a;
        calBad.needsCalibration = true;
        calBad.calFails = true;
        Add(v, "r6#2", "calibration always fails, Calibrate, OK", calBad, fileOwn, Steps(1, CALIBRATE, 0), OK, SAME, VIOLATES);
        // r6#6: after the meter is lost in the sheet, Calibrate is disabled, and settings
        // changed and confirmed with OK are told not kept.
        Add(v, "r6#6", "Spectral Type the meter fails on, Calibrate, Calibrate again, OK", obsFails, fileOwn, Steps(3, SET_OBS, 4, CALIBRATE, 0, CALIBRATE, 0), OK, DIFFERS, VIOLATES);
        Add(v, "r6#6", "Spectral Type the meter fails on, Calibrate, Display Type changed, OK", obsFails, fileOwn, Steps(3, SET_OBS, 4, CALIBRATE, 0, SET_DT, 1), OK, DIFFERS, VIOLATES);
        // r7#3: an i1Pro-like meter (no samples) with no correction has no spectral
        // row; after the meter is lost, Calibrate is disabled all the same.
        World i1pro = obsFails;
        i1pro.exposed = false;
        i1pro.holdsSamples = false;
        Add(v, "r7#3", "no spectral row, Spectral Type the meter fails on, Calibrate, Calibrate again, OK", i1pro, Doc(), Steps(3, SET_OBS, 4, CALIBRATE, 0, CALIBRATE, 0), OK, DIFFERS, VIOLATES);
        // r7#3: the same page, with the meter unplugged while the sheet is open (no
        // setting changed): the second Calibrate is disabled, not a silent click.
        World unplug = i1pro;
        unplug.failObs = -1;
        unplug.unplugInSheet = true;
        Add(v, "r7#3", "no spectral row, unplugged in the sheet, Calibrate, Calibrate again, OK", unplug, Doc(), Steps(2, CALIBRATE, 0, CALIBRATE, 0), OK, DIFFERS, VIOLATES);
        // r7#2: the rejected Spectral Type, then Cancel: neither the INI nor the
        // document keeps it.
        Add(v, "r7#2", "Spectral Type the meter fails on, Calibrate, Cancel", obsFails, fileOwn, Steps(2, SET_OBS, 4, CALIBRATE, 0), CANCEL, DIFFERS, VIOLATES);
        // A Spyder's own browsed file, and an i1d3's: nothing changes in either order.
        Add(v, "-", "Spyder, its own browsed file, Calibrate, OK", sp, spyF, Steps(1, CALIBRATE, 0), OK, SAME, HOLDS);
        Add(v, "-", "i1d3, its own browsed file, Calibrate, OK", a, fileOwn, Steps(1, CALIBRATE, 0), OK, SAME, HOLDS);
        return v;
    }

    // The invariants every end state must hold; "" when it does, else what breaks.
    static std::string Violations(const Scenario& sc, const Doc& e)
    {
        std::string out;
        // The second invariant: no record says a correction is in force that nothing
        // applies - and what applies it is the record's own: its file's sample, or its
        // own Display Type entry (the driver of a meter whose matrices are not exposed).
        // With no meter nothing is measured and nothing applies anything: the record
        // is the document's, kept as it was (a failed start changes nothing).
        if (!e.s.meterGone && e.s.rec.IsPresent() && e.s.rec.basis != FIT_BASIS_NATIVE)
        {
            const std::string want = e.s.rec.source == SPECTRAL_SOURCE_FILE ? e.s.rec.description
                                                                            : "entry:" + e.s.rec.description;
            if (sc.w.exposed || e.s.driverSample != want)
                out += " unapplied-record";
        }
        // A meter whose matrices are exposed measures native: the driver is never left
        // on a corrected entry (R5).
        if (sc.w.exposed && e.s.driverEntry >= 0 && sc.w.list[e.s.driverEntry].corrected)
            out += " driver-on-corrected-entry";
        // A FILE record and a readable path name the same file (the R4 rule).
        const DiskFile* f = sc.w.File(e.s.path);
        if (e.s.rec.source == SPECTRAL_SOURCE_FILE && e.s.rec.fileHash != 0 && f != NULL
         && HashOf(f->content) != e.s.rec.fileHash)
            out += " path-not-record-file";
        // The record the readings went through stands (R4).
        if (!sc.wantRecord.empty() && e.s.rec.description != sc.wantRecord)
            out += " record-replaced";
        // The setting the user changed in the sheet is applied (r5#1: a failed start
        // must not lock the user out of the sheet that changes it).
        if (sc.wantDisplayType >= 0 && e.s.displayType != sc.wantDisplayType)
            out += " setting-not-applied";
        // Removing a record never forgets a correction the readings went through: a
        // driver-applied record that is gone leaves the readings marked as carrying an
        // unrecorded driver correction (r3#2).
        if (sc.d.s.rec.IsPresent() && sc.d.s.rec.basis == FIT_BASIS_DRIVER_CORRECTED && !e.s.rec.IsPresent()
         && !ReadingsCarryUnrecordedDriverCorrection(e.s.rec, e.s.fitBasis))
            out += " forgot-driver-correction";
        // Nothing dereferences a lost meter and no throw escapes (r4, r5).
        if (e.crashed)
            out += " crash";
        // With no meter no record can come to say a correction is in force (r5#4: Browse
        // after the meter was lost would install one).
        if (e.s.meterGone && e.s.rec.IsPresent() && e.s.rec != e.s.recAtLoss)
            out += " record-installed-without-meter";
        // One message per failure per user action; a refused sheet always says why (r5).
        if (e.maxMessages > 1)
            out += " several-messages";
        if (e.refusedSilently)
            out += " refused-silently";
        if (e.silentClick)
            out += " silent-click";
        if (e.settingsDroppedSilently)
            out += " settings-dropped-silently";
        if (e.loopedForever)
            out += " calibration-loop";
        if (e.silentRevert)
            out += " silent-revert";
        if (e.pageStale)
            out += " page-stale";
        // A setting the meter rejected is kept nowhere: a restart starts the meter
        // with the INI's settings (a new document) and the document's (r7#2).
        if (sc.w.failObs >= 0 && ( e.iniObserver == sc.w.failObs || e.s.observer == sc.w.failObs ))
            out += " rejected-setting-kept";
        // A start that fails changes nothing: the sheet is refused, the next
        // measurement gets no data, and the correction state is the document's.
        if (sc.w.startFails && ( e.s.rec != sc.d.s.rec || e.s.path != sc.d.s.path || e.s.method != sc.d.s.method
                              || !NearVec(e.rawless, sc.d.rawless, 0.0) || e.s.rawless.Get() != sc.d.s.rawless.Get() ))
            out += " failed-start-changed-state";
        return out;
    }

    // (i) r7#1: a sweep on a meter whose calibration always fails stores no reading,
    // shows its Abort/Retry/Ignore box once per failing patch it is asked about, and
    // stops on Abort; a continuous run ends at the first invalid reading.
    static void CheckSweep()
    {
        World w = I1d3("A");
        w.needsCalibration = true;
        w.calFails = true;
        struct { int answer, wantPrompts, wantMessages; const char* what; } rows[] = {
            { 0, 1, 1, "Abort at the first box" },
            { 1, 2, 2, "Retry once, then Abort" },
            { 2, 21, 21, "Ignore every box" },
        };
        for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
        {
            Doc d;
            const SweepResult r = Sweep(d, w, kBranch, 21, rows[i].answer);
            if (r.stored != 0 || r.fakeStored != 0)
                Fail("T45 (i) %s: %d readings and %d clamped readings stored from a failed calibration",
                     rows[i].what, r.stored, r.fakeStored);
            if (r.prompts != rows[i].wantPrompts)
                Fail("T45 (i) %s: %d Abort/Retry/Ignore boxes, want %d", rows[i].what, r.prompts, rows[i].wantPrompts);
            if (r.messages != rows[i].wantMessages)
                Fail("T45 (i) %s: %d calibration-failure boxes, want %d", rows[i].what, r.messages, rows[i].wantMessages);
        }
        // The continuous thread (datasetdoc.cpp): it ends at the first invalid reading.
        {
            Doc d;
            int readings = 0, posted = 0;
            Init(d, w, kBranch, false);
            for (int k = 0; k < 50; ++k)
            {
                ++readings;
                if (!Reading(d, w, kBranch))
                    break;                          // "Error: close thread"
                ++posted;
            }
            if (posted != 0 || readings != 1)
                Fail("T45 (i) continuous: %d readings, %d posted, want 1 and 0", readings, posted);
        }
        // r8#2: the first calibration fails, Retry succeeds: the reading after the
        // recovery is stored, and so is every later patch (the flag starts each
        // reading valid again).
        {
            World once = I1d3("A");
            once.needsCalibration = true;
            once.calFailsOnce = true;
            Doc d;
            const SweepResult r = Sweep(d, once, kBranch, 21, 1);
            if (r.stored != 21 || r.prompts != 1)
                Fail("T45 (i) fail, Retry, recover: %d stored, %d boxes, want 21 and 1", r.stored, r.prompts);
        }
        // A meter that calibrates: every patch stored, no box.
        {
            World ok = I1d3("A");
            ok.needsCalibration = true;
            Doc d;
            const SweepResult r = Sweep(d, ok, kBranch, 21, 0);
            if (r.stored != 21 || r.prompts != 0)
                Fail("T45 (i) a calibrating meter: %d stored, %d boxes, want 21 and 0", r.stored, r.prompts);
        }
    }

    // (a) the order-independence table.
    static void CheckOrders()
    {
        const std::vector<Scenario> v = Scenarios();
        int nMainDiffers = 0, nMainViolates = 0;
        printf("T45 (a) %d scenarios in three orders (measurement first, sheet first, calibrate-cancel first),\n"
               "        then the invariants:\n", (int)v.size());
        for (size_t i = 0; i < v.size(); ++i)
        {
            const Scenario& sc = v[i];
            Doc be[3], me[3];
            const std::string branch = RunOrders(sc, kBranch, be);
            const std::string onMain = RunOrders(sc, kMain, me);
            std::string mainBroken, branchBroken;
            for (int k = 0; k < 3; ++k)
            {
                mainBroken += Violations(sc, me[k]);
                branchBroken += Violations(sc, be[k]);
            }
            printf("  %-5s %-70s main:%s%s%s | branch:%s%s%s\n", sc.id, sc.what.c_str(),
                   onMain.empty() ? " same" : onMain.c_str(), mainBroken.empty() ? "" : " /", mainBroken.c_str(),
                   branch.empty() ? " same" : branch.c_str(), branchBroken.empty() ? "" : " /", branchBroken.c_str());
            if (!branch.empty())
                Fail("T45 (a) %s %s: the orders differ:%s", sc.id, sc.what.c_str(), branch.c_str());
            if (!branchBroken.empty())
                Fail("T45 (a) %s %s: invariant broken:%s", sc.id, sc.what.c_str(), branchBroken.c_str());
            if (sc.mainDiffers != !onMain.empty())
                Fail("T45 (a) %s %s: main's order %s, the review says it %s", sc.id, sc.what.c_str(),
                     onMain.empty() ? "does not differ" : "differs", sc.mainDiffers ? "does" : "does not");
            if (sc.mainViolates != !mainBroken.empty())
                Fail("T45 (a) %s %s: main %s an invariant, the review says it %s", sc.id, sc.what.c_str(),
                     mainBroken.empty() ? "holds" : "breaks", sc.mainViolates ? "breaks one" : "holds them");
            if (sc.clearSeen && ( be[0].clearAllowed.empty() || be[0].clearAllowed[0] != 1 ))
                Fail("T45 (a) %s %s: the page's Clear was not enabled", sc.id, sc.what.c_str());
            nMainDiffers += onMain.empty() ? 0 : 1;
            nMainViolates += mainBroken.empty() ? 0 : 1;
            for (int k = 0; k < 3; ++k)
            {
                const Doc& e = be[k];
                // Readings marked as embodying the matrix do: a composable raw-less
                // reading is Effective * native, through the correction the readings
                // carry unrecorded where no derive can express it (a 4.2.2 entry
                // without a base, R5). A reading another unit's or
                // another meter's correction went through must be stranded.
                const Matrix u = ( sc.unrecordedTruth && ReadingsCarryUnrecordedDriverCorrection(e.s.rec, e.s.fitBasis) )
                               ? e.unrecorded : I3();
                if (e.s.rawless.ComposeOntoRawless() && !NearVec(e.rawless, e.s.Effective() * u * Native(), 1e-12))
                    Fail("T45 (a) %s %s, order %d: raw-less reading is not Effective * native while marked composable",
                         sc.id, sc.what.c_str(), k);
            }
        }
        printf("T45 (a): main differs in %d of %d scenarios and breaks an invariant in %d\n",
               nMainDiffers, (int)v.size(), nMainViolates);
    }

    static SpectralCorrectionRecord Rec(int source, int basis, unsigned long long hash)
    {
        SpectralCorrectionRecord r;
        r.source = source;
        r.description = "X";
        r.basis = basis;
        r.fileHash = hash;
        return r;
    }

    // (b) ReadingsCarryUnrecordedDriverCorrection
    static void CheckUnrecorded()
    {
        const SpectralCorrectionRecord none, file = Rec(SPECTRAL_SOURCE_FILE, FIT_BASIS_NATIVE, 1),
                                       dt = Rec(SPECTRAL_SOURCE_DISPLAY_TYPE, FIT_BASIS_NATIVE, 0);
        struct { const SpectralCorrectionRecord* rec; int basis; bool want; const char* what; } rows[] = {
            { &none, FIT_BASIS_UNKNOWN,          true,  "no record, UNKNOWN" },
            { &none, FIT_BASIS_DRIVER_CORRECTED, true,  "no record, DRIVER_CORRECTED" },
            { &none, FIT_BASIS_NATIVE,           false, "no record, NATIVE" },
            { &file, FIT_BASIS_UNKNOWN,          false, "FILE record, UNKNOWN" },
            { &dt,   FIT_BASIS_DRIVER_CORRECTED, false, "Display Type record, DRIVER_CORRECTED" },
        };
        for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
            if (ReadingsCarryUnrecordedDriverCorrection(*rows[i].rec, rows[i].basis) != rows[i].want)
                Fail("T45 (b) %s: got %d", rows[i].what, (int)!rows[i].want);
    }

    // (c) EffectiveChangedWithoutWalk: exact equality is no change; a change the
    // readings already went through leaves them composable; any other strands them,
    // and a strand is never undone by a later call.
    static void CheckBooking()
    {
        const Matrix c = CorrFor("entry:E", "A", 3), c2 = CorrFor("entry:E", "B", 3);
        struct { Matrix before, after; bool through; bool want; const char* what; } rows[] = {
            { c,    c,  false, true,  "unchanged" },
            { I3(), c,  true,  true,  "re-expressed (the readings went through it)" },
            { c,    c2, false, false, "another unit's correction" },
            { c,    I3(), false, false, "the conversion for a meter whose matrices are not exposed" },
        };
        for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
        {
            RawlessReadingsState st;
            EffectiveChangedWithoutWalk(st, rows[i].before, rows[i].after, rows[i].through);
            if (st.ComposeOntoRawless() != rows[i].want)
                Fail("T45 (c) %s: composable %d, want %d", rows[i].what, (int)st.ComposeOntoRawless(), (int)rows[i].want);
        }
        RawlessReadingsState stranded;
        stranded.MatrixSetWithoutWalk();
        EffectiveChangedWithoutWalk(stranded, I3(), c, true);
        if (stranded.ComposeOntoRawless())
            Fail("T45 (c) a re-expression re-armed stranded readings");
    }

    // (d) SpectralRecordFromPathFile (R4)
    static void CheckPathRule()
    {
        const unsigned long long ha = HashOf("F1"), hb = HashOf("G1");
        if (ha == hb)
            Fail("T45 (d) the two files hash alike");
        const SpectralCorrectionRecord file = Rec(SPECTRAL_SOURCE_FILE, FIT_BASIS_NATIVE, ha),
                                       unhashed = Rec(SPECTRAL_SOURCE_FILE, FIT_BASIS_NATIVE, 0),
                                       dt = Rec(SPECTRAL_SOURCE_DISPLAY_TYPE, FIT_BASIS_NATIVE, 0), none;
        struct { const SpectralCorrectionRecord* rec; bool hashed; unsigned long long h; bool want; const char* what; } rows[] = {
            { &file,     true,  ha, true,  "FILE record, its own file" },
            { &file,     true,  hb, false, "FILE record, another file at the path" },
            { &file,     false, 0,  true,  "FILE record, path unreadable" },
            { &unhashed, true,  hb, true,  "FILE record without a hash" },
            { &dt,       true,  hb, true,  "Display Type record" },
            { &none,     true,  hb, true,  "no record" },
        };
        for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
            if (SpectralRecordFromPathFile(*rows[i].rec, rows[i].hashed, rows[i].h) != rows[i].want)
                Fail("T45 (d) %s: got %d", rows[i].what, (int)!rows[i].want);
    }

    // (e) SpectralRecordApplied (the second invariant; r3#3, r3#4)
    static void CheckApplied()
    {
        const SpectralCorrectionRecord none, native = Rec(SPECTRAL_SOURCE_DISPLAY_TYPE, FIT_BASIS_NATIVE, 0),
                                       driver = Rec(SPECTRAL_SOURCE_FILE, FIT_BASIS_DRIVER_CORRECTED, 1),
                                       driverDt = Rec(SPECTRAL_SOURCE_DISPLAY_TYPE, FIT_BASIS_DRIVER_CORRECTED, 0);
        // Rec() describes a record "X".
        struct { const SpectralCorrectionRecord* rec; bool exposed, samples, path; const char* entry; bool want; const char* what; } rows[] = {
            { &none,     true,  true,  false, "",  true,  "no record" },
            { &native,   true,  true,  false, "",  true,  "NATIVE record, i1d3 (HCFR applies it)" },
            { &native,   false, true,  false, "",  true,  "NATIVE record, Spyder (before the conversion)" },
            { &driver,   false, true,  true,  "",  true,  "FILE record, Spyder with its path (the re-apply)" },
            { &driver,   false, true,  false, "",  false, "FILE record, Spyder, path dropped (r2#2)" },
            { &driver,   false, false, true,  "",  false, "FILE record, a meter without samples, with a path (r3#3)" },
            { &driver,   true,  true,  true,  "",  false, "DRIVER_CORRECTED on an i1d3 (holds no sample)" },
            { &driverDt, false, true,  false, "X", true,  "Display Type record, its entry selected on a Spyder (r3#4)" },
            { &driverDt, false, false, false, "X", true,  "the same on a meter without samples (a ccmx entry)" },
            { &driverDt, false, true,  false, "Y", false, "Display Type record, another entry selected (r3#4)" },
            { &driverDt, false, true,  false, "",  false, "Display Type record, a plain entry selected (r1#5)" },
            { &driverDt, false, true,  true,  "X", false, "Display Type record under a path's sample" },
            { &driverDt, true,  true,  false, "X", false, "DRIVER_CORRECTED Display Type record on an i1d3" },
        };
        for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
            if (SpectralRecordApplied(*rows[i].rec, rows[i].exposed, rows[i].samples, rows[i].path, rows[i].entry) != rows[i].want)
                Fail("T45 (e) %s: got %d", rows[i].what, (int)!rows[i].want);
    }

    // (f) SpectralClearAllowed (the Spyder clear gap; r1#9; r2#4)
    static void CheckClearRule()
    {
        const SpectralCorrectionRecord none, file = Rec(SPECTRAL_SOURCE_FILE, FIT_BASIS_NATIVE, 1),
                                       dt = Rec(SPECTRAL_SOURCE_DISPLAY_TYPE, FIT_BASIS_NATIVE, 0);
        struct { const SpectralCorrectionRecord* rec; bool path, resyncs, selected, want; const char* what; } rows[] = {
            { &none, false, true,  false, false, "nothing" },
            { &none, true,  true,  false, true,  "a stored path, no record" },
            { &file, false, true,  false, true,  "FILE record, i1d3" },
            { &file, false, false, false, true,  "FILE record, Spyder" },
            { &dt,   false, true,  true,  false, "Display Type record, its entry selected, i1d3 (the list removes it)" },
            { &dt,   true,  true,  true,  false, "the same with a stored path (the next Init derives it again)" },
            { &dt,   false, true,  false, true,  "Display Type record, entry not selected (gone, or a plain type)" },
            { &dt,   false, false, false, true,  "Display Type record, a meter that never resyncs, or none" },
        };
        for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
            if (SpectralClearAllowed(*rows[i].rec, rows[i].path, rows[i].resyncs, rows[i].selected) != rows[i].want)
                Fail("T45 (f) %s: got %d", rows[i].what, (int)!rows[i].want);
    }

    // (g) PlainDisplayTypeEntry (R5)
    static void CheckPlainEntry()
    {
        std::vector<bool> list;
        list.push_back(false); list.push_back(false); list.push_back(true); list.push_back(true);
        struct { int stored, want; const char* what; } rows[] = {
            { 1,  1, "stored plain entry" },
            { 2,  0, "stored corrected entry" },
            { 7,  0, "stored index past the end" },
            { -1, 0, "no stored entry" },
        };
        for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
        {
            const int got = PlainDisplayTypeEntry(rows[i].stored, list);
            if (got != rows[i].want)
                Fail("T45 (g) %s: got %d want %d", rows[i].what, got, rows[i].want);
        }
        std::vector<bool> lateFirst;
        lateFirst.push_back(true); lateFirst.push_back(false);
        if (PlainDisplayTypeEntry(0, lateFirst) != 1)
            Fail("T45 (g) the first plain entry is not entry 0");
        std::vector<bool> allCorrected(3, true), empty;
        if (PlainDisplayTypeEntry(0, allCorrected) != -1 || PlainDisplayTypeEntry(0, empty) != -1)
            Fail("T45 (g) a list with no plain entry");
    }

    // (h) r2#6: identity is the content. A rewrite with the same size and the same
    // modification time (what robocopy/xcopy preserve) is another file to the rule,
    // because the bytes are hashed when they are used.
    static void CheckContentIdentity()
    {
        const char* tmp = "t45_identity.tmp";
        FILE* f = fopen(tmp, "wb");
        if (!f)
        {
            Fail("T45 (h) cannot create %s", tmp);
            return;
        }
        fwrite("CCSS-A", 1, 6, f);
        fclose(f);
        struct _stat64 st0;
        unsigned long long h0 = 0, h1 = 0;
        if (_stat64(tmp, &st0) != 0 || !Fnv1a64File(tmp, h0))
        {
            Fail("T45 (h) cannot read %s back", tmp);
            remove(tmp);
            return;
        }
        SpectralCorrectionRecord rec = Rec(SPECTRAL_SOURCE_FILE, FIT_BASIS_NATIVE, h0);
        f = fopen(tmp, "wb");
        if (f)
        {
            fwrite("CCSS-B", 1, 6, f);
            fclose(f);
        }
        struct __utimbuf64 keep;
        keep.actime = st0.st_atime;
        keep.modtime = st0.st_mtime;
        struct _stat64 st1;
        const bool same = ( _utime64(tmp, &keep) == 0 && _stat64(tmp, &st1) == 0
                         && st1.st_size == st0.st_size && st1.st_mtime == st0.st_mtime );
        if (!same)
            Fail("T45 (h) the rewrite did not keep the size and the time");
        if (!Fnv1a64File(tmp, h1) || h1 == h0)
            Fail("T45 (h) the rewritten file hashes as before: %016llx", h1);
        if (SpectralRecordFromPathFile(rec, true, h1))
            Fail("T45 (h) the rewritten file passes the path rule as the record's file");
        remove(tmp);
    }
}

static void RunT45()
{
    T45Order::CheckOrders();
    T45Order::CheckUnrecorded();
    T45Order::CheckBooking();
    T45Order::CheckPathRule();
    T45Order::CheckApplied();
    T45Order::CheckClearRule();
    T45Order::CheckPlainEntry();
    T45Order::CheckContentIdentity();
    T45Order::CheckSweep();
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
    RunT14();
    RunT19();
    RunT32();
    RunT22();
    RunT41();
    RunT42();
    RunT43();
    RunT44();
    RunT45();
    RunT46();

    if (g_failures)
    {
        printf("\n%d FAILURE(S)\n", g_failures);
        return 1;
    }
    printf("\nALL TESTS PASSED (%s mode)\n", g_genMode ? "gen" : "verify");
    return 0;
}
