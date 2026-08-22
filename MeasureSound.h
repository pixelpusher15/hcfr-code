// MeasureSound.h : plays the short cue that confirms a successful sensor read
//
// PlaySound() cannot do this job. It opens and closes the wave device on every
// call, and that start-up latency is longer than the gap between patches in a
// sweep (measured at ~220 ms on a grayscale run), so each call cancelled the
// previous cue before it ever became audible and only the last cue of a sweep
// was heard. This player keeps one device open for the life of the app, so a
// cue starts immediately and every measure is heard.
//
// ASCII-clean by design (safe for the Edit tool).

#pragma once

#include <windows.h>
#include <mmsystem.h>

class CMeasureSoundPlayer
{
public:
	CMeasureSoundPlayer();
	~CMeasureSoundPlayer();

	// Non-copyable: owns a device handle, a heap buffer and a prepared header.
	CMeasureSoundPlayer(const CMeasureSoundPlayer &) = delete;
	CMeasureSoundPlayer & operator=(const CMeasureSoundPlayer &) = delete;

	// nResource names a WAVE resource in the executable. Safe to call from the
	// async measure worker thread as well as the UI thread. A cue that is still
	// playing is cut short so the new one lands on time: when measures come
	// faster than a cue is long it is the tail that is lost, never the attack.
	//
	// Never throws and never fails a measurement. The caller is deep inside
	// CSensor::MeasureColor, where an escaping exception would be caught by the
	// measure worker and reported as an invalid reading -- so a sound problem
	// would corrupt a sweep. Any failure here is silent, in both senses.
	void Play(UINT nResource);

	// Release the device AND the cached wave. Idempotent, and called from the
	// destructor, but the app calls it from ExitInstance() so teardown does not
	// depend on the order static objects are destroyed -- and so the buffer is
	// freed before MFC dumps its leak report.
	void Close();

private:
	// Copies the resource's PCM payload and format into m_pData/m_format,
	// replacing whatever was cached before. Caches by resource id, so a sweep
	// re-uses the copy it already made. Only ever called with the header already
	// unprepared -- see StopAndUnprepare.
	BOOL LoadWave(UINT nResource);
	BOOL OpenDevice();

	// Stops the cue in flight and hands back the header's page-lock on m_pData.
	// waveOutPrepareHeader locks that buffer and keeps it locked until the
	// matching unprepare, so this has to run before anything frees, replaces or
	// re-writes m_pData. Leaves the header zeroed and the device open.
	void StopAndUnprepare();

	// Device only, keeping the cached wave. Play() needs this to reopen for a
	// wave in a different format: Close() would free the very buffer that the
	// waveOutWrite() straight afterwards is about to play.
	void CloseDevice();

	CRITICAL_SECTION m_cs;
	HWAVEOUT         m_hWaveOut;
	WAVEFORMATEX     m_format;          // format m_hWaveOut is open with
	WAVEHDR          m_header;
	BOOL             m_bPrepared;
	UINT             m_nLoadedResource; // which wave m_pData holds, 0 for none
	BYTE *           m_pData;
	DWORD            m_dwDataSize;
};
