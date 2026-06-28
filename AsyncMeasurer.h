// AsyncMeasurer.h : run a blocking sensor read on a worker thread so the UI can pump
//
// Part of the measurement / UI decoupling work (see MEASUREMENT-UI-DECOUPLING-PLAN.md,
// Phase 1). A calibrated sweep keeps its own generator/sensor orchestration on the UI
// thread, but delegates the one slow call -- CSensor::MeasureColor() -- to this helper,
// which runs it on a dedicated worker thread while the UI thread pumps WM_PAINT. That
// removes the per-point "(Not Responding)" freeze and lets charts repaint live during a
// sweep, without moving any view/dialog code off the UI thread.
//
// The sensor MUST already be Init()'d by the caller on the UI thread; this class never
// Init/Release's it. Opening the device on the UI thread and reading it from a worker is
// exactly the pattern the shipping simultaneous-measure path already uses
// (CMeasure::InitBackgroundMeasures / BkGndMeasureThreadFunc), so the device thread-safety
// profile is unchanged.
//
// ASCII-clean by design (safe for the Edit tool).

#pragma once

#include <windows.h>
#include "Color.h"

class CSensor;

class CAsyncMeasurer
{
public:
	CAsyncMeasurer();
	~CAsyncMeasurer();

	// Owns raw thread/event HANDLEs -- non-copyable (a copy would double-close them).
	CAsyncMeasurer(const CAsyncMeasurer &) = delete;
	CAsyncMeasurer & operator=(const CAsyncMeasurer &) = delete;

	// Spin up the worker bound to an already-initialised sensor. Returns FALSE on
	// failure; callers should then fall back to a direct synchronous MeasureColor().
	BOOL Start(CSensor * pSensor);

	// True once Start() has succeeded and Stop() has not yet run.
	BOOL IsRunning() const { return m_hThread != NULL; }

	// Measure aRGBValue on the worker and block the CALLING (UI) thread until it
	// completes, pumping WM_PAINT so the window stays responsive and charts repaint.
	// The result is returned in 'out'. Measurement validity is reported exactly as a
	// direct call would: query the sensor's IsMeasureValid()/GetErrorString() afterwards.
	// Returns FALSE only if the worker is not running (caller should fall back).
	BOOL MeasurePumped(const ColorRGBDisplay & aRGBValue, CColor & out);

	// Stop and join the worker, close handles. Safe to call repeatedly. Called
	// automatically by the destructor, so early returns from a sweep clean up via RAII.
	void Stop();

private:
	static DWORD WINAPI ThreadProc(LPVOID pParam);
	void Run();

	CSensor *        m_pSensor;
	HANDLE           m_hThread;
	HANDLE           m_hEventRun;   // UI -> worker: a request is pending
	HANDLE           m_hEventDone;  // worker -> UI: result is ready
	volatile BOOL    m_bTerminate;
	ColorRGBDisplay  m_request;     // written by UI before SetEvent(run)
	CColor           m_result;      // written by worker before SetEvent(done)
};
