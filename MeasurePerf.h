/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2026 Association Homecinema Francophone.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////
//
//  This file is subject to the terms of the GNU General Public License as
//  published by the Free Software Foundation.  A copy of this license is
//  included with this software distribution in the file COPYING.htm. If you
//  do not have a copy, you may obtain a copy by writing to the Free
//  Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details
/////////////////////////////////////////////////////////////////////////////

#pragma once
// -----------------------------------------------------------------------------
// Shared hook for measurement performance logging (baseline profiling).
//
// The per-sweep accumulator and the summary-line writer live in Measure.cpp.
// This header lets the generator translation unit contribute the "display"
// (patch-generation) phase time to the active sweep. Sweep timing runs on the
// UI thread (only the sensor read is off-thread), so the shared state needs no
// locking. The free-run worker also calls DisplayRGBColor, but no sweep is in
// progress then, so HcfrPerfRecordDisplay returns without touching that state.
// -----------------------------------------------------------------------------
#include <windows.h>

// Defined in Measure.cpp. Records the wall-clock duration (ms) of one display /
// patch-generation call into the active sweep. No-op when perf logging is
// disabled ([Debug] PerfLog, default off) or no sweep is running.
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
