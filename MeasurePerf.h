#pragma once
// -----------------------------------------------------------------------------
// Shared hook for measurement performance logging (baseline profiling).
//
// The per-sweep accumulator and the summary-line writer live in Measure.cpp.
// This header lets the generator translation unit contribute the "display"
// (patch-generation) phase time to the active sweep. All measurement timing
// runs on the UI thread (only the sensor read is off-thread), so the shared
// state needs no locking.
// -----------------------------------------------------------------------------
#include <windows.h>

// Defined in Measure.cpp. Records the wall-clock duration (ms) of one display /
// patch-generation call into the active sweep. No-op when perf logging is
// disabled ([Debug] PerfLog=0) or no sweep is running.
void HcfrPerfRecordDisplay(double ms);

// RAII: times an entire display call -- covering every return path -- and hands
// the duration to HcfrPerfRecordDisplay. Place one instance at the top of a
// generator's DisplayRGBColor implementation.
struct PerfDisplayScope
{
    LARGE_INTEGER t0, fr;
    PerfDisplayScope()
    {
        QueryPerformanceFrequency(&fr);
        QueryPerformanceCounter(&t0);
    }
    ~PerfDisplayScope()
    {
        LARGE_INTEGER t1;
        QueryPerformanceCounter(&t1);
        HcfrPerfRecordDisplay(1000.0 * (double)(t1.QuadPart - t0.QuadPart) / (double)fr.QuadPart);
    }
};
