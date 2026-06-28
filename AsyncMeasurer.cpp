// AsyncMeasurer.cpp : implementation of CAsyncMeasurer (see AsyncMeasurer.h)

#include "StdAfx.h"
#include "ColorHCFR.h"   // CPropertyPageWithHelp + resource.h (prereqs for Sensor.h's header chain)
#include "AsyncMeasurer.h"
#include "Sensors\Sensor.h"
#include "CrashDump.h"
#include <iostream>
#include <cstdlib>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CAsyncMeasurer::CAsyncMeasurer()
	: m_pSensor(NULL), m_hThread(NULL), m_hEventRun(NULL), m_hEventDone(NULL),
	  m_bTerminate(FALSE), m_displaymode(0)
{
}

CAsyncMeasurer::~CAsyncMeasurer()
{
	Stop();
}

BOOL CAsyncMeasurer::Start(CSensor * pSensor)
{
	if (m_hThread)
		return TRUE;            // already running
	if (!pSensor)
		return FALSE;

	m_pSensor    = pSensor;
	m_bTerminate = FALSE;

	// Manual-reset, initially non-signaled events.
	m_hEventRun  = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hEventDone = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!m_hEventRun || !m_hEventDone)
	{
		Stop();
		return FALSE;
	}

	DWORD dwId = 0;
	m_hThread = CreateThread(NULL, 0, ThreadProc, this, 0, &dwId);
	if (!m_hThread)
	{
		Stop();
		return FALSE;
	}
	return TRUE;
}

BOOL CAsyncMeasurer::MeasurePumped(const ColorRGBDisplay & aRGBValue, CColor & out, int displaymode)
{
	if (!m_hThread)
		return FALSE;

	// Hand the request to the worker.
	ResetEvent(m_hEventDone);
	m_request    = aRGBValue;
	m_displaymode = displaymode;
	SetEvent(m_hEventRun);

	// Block here until the measure completes, but run a message pump so the window stays
	// genuinely interactive during the integration: hover/hot-tracking, scrolling, repaints,
	// and the Stop button all work. The 200 ms wake keeps the queue serviced even during a
	// long, input-free dark-patch integration so the window is never flagged "(Not Responding)".
	for (;;)
	{
		DWORD wr = MsgWaitForMultipleObjects(1, &m_hEventDone, FALSE, 200, QS_ALLINPUT);
		if (wr == WAIT_OBJECT_0)
			break;             // measurement done

		// Pump everything EXCEPT two ranges, by peeking around the keyboard range:
		//  - keyboard (WM_KEYFIRST..WM_KEYLAST) is left queued so the sweep's own
		//    post-read PeekMessage still sees an ESC press and aborts, exactly as in the
		//    old synchronous path;
		//  - queued WM_COMMAND (menu / accelerator / posted toolbar commands) is dropped
		//    so nothing can run mid-sweep and disturb the measurement (e.g. opening
		//    References and changing the gamut, or exporting a half-finished sweep).
		// The Stop button is unaffected: a button click delivers WM_COMMAND via a
		// synchronous SendMessage during WM_LBUTTONUP, not as a queued message, so it is
		// not filtered (and the action buttons that could interfere are guarded by
		// IsMeasureSweepActive() in their handlers).
		MSG msg;
		while (PeekMessage(&msg, NULL, 0, WM_KEYFIRST - 1, PM_REMOVE)
		    || PeekMessage(&msg, NULL, WM_KEYLAST + 1, 0xFFFFFFFF, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				// App is shutting down (unexpected mid-sweep, since close is vetoed while
				// measuring). Requeue the quit, let the in-flight read finish so the worker
				// is idle, then return; the sweep unwinds and the quit is handled after.
				PostQuitMessage((int) msg.wParam);
				WaitForSingleObject(m_hEventDone, INFINITE);
				out = m_result;
				return TRUE;
			}
			if (msg.message == WM_COMMAND)
				continue;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	out = m_result;
	return TRUE;
}

void CAsyncMeasurer::Stop()
{
	if (m_hThread)
	{
		m_bTerminate = TRUE;
		if (m_hEventRun)
			SetEvent(m_hEventRun);     // wake the worker so it sees m_bTerminate
		WaitForSingleObject(m_hThread, INFINITE);
		CloseHandle(m_hThread);
		m_hThread = NULL;
	}
	if (m_hEventRun)
	{
		CloseHandle(m_hEventRun);
		m_hEventRun = NULL;
	}
	if (m_hEventDone)
	{
		CloseHandle(m_hEventDone);
		m_hEventDone = NULL;
	}
	m_pSensor = NULL;
}

DWORD WINAPI CAsyncMeasurer::ThreadProc(LPVOID pParam)
{
	((CAsyncMeasurer *) pParam)->Run();
	return 0;
}

void CAsyncMeasurer::Run()
{
	CrashDump useInThisThread;
	// The CRT keeps a PER-THREAD rand() seed that starts at 1 on every new thread. Since a
	// fresh worker thread is created for each sweep, rand() would otherwise replay the exact
	// same sequence every run -- making the simulated sensor's offset/gain error simulation
	// produce identical "random" results on the 2nd+ run. Seed per-thread so each sweep varies,
	// matching the old single-UI-thread behaviour where rand() advanced continuously.
	srand((unsigned)(GetTickCount() ^ GetCurrentThreadId()));

	for (;;)
	{
		WaitForSingleObject(m_hEventRun, INFINITE);
		ResetEvent(m_hEventRun);

		if (m_bTerminate)
			break;

		// The try/catch MUST be inside the loop and m_hEventDone MUST be signaled on every
		// path: if MeasureColor() throws (e.g. a meter I/O error) and we don't signal, the
		// UI-thread pump in MeasurePumped() waits forever and the sweep hangs unrecoverably.
		// On failure we report an invalid reading so the sweep takes its normal error path.
		try
		{
			// The same call the sweep would make directly on the UI thread:
			// CSensor::MeasureGray() merely forwards to MeasureColor() of a gray.
			m_result = m_pSensor->MeasureColor(m_request, m_displaymode);
		}
		catch (std::exception & e)
		{
			std::cerr << "Exception in async measure thread : " << e.what() << std::endl;
			m_result = noDataColor;
			if (m_pSensor)
				m_pSensor->SetMeasureValidity(FALSE);
		}
		catch (...)
		{
			std::cerr << "Unexpected exception in async measure thread" << std::endl;
			m_result = noDataColor;
			if (m_pSensor)
				m_pSensor->SetMeasureValidity(FALSE);
		}

		SetEvent(m_hEventDone);
	}
}
