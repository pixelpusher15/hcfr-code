// MeasureSound.cpp : implementation of CMeasureSoundPlayer
//
// See MeasureSound.h for why this exists instead of a PlaySound() call.

#include "stdafx.h"
#include "MeasureSound.h"

#pragma comment(lib, "winmm.lib")

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

CMeasureSoundPlayer::CMeasureSoundPlayer()
{
	InitializeCriticalSection(&m_cs);
	m_hWaveOut = NULL;
	m_bPrepared = FALSE;
	m_nLoadedResource = 0;
	m_pData = NULL;
	m_dwDataSize = 0;
	ZeroMemory(&m_format, sizeof(m_format));
	ZeroMemory(&m_header, sizeof(m_header));
}

CMeasureSoundPlayer::~CMeasureSoundPlayer()
{
	Close();
	DeleteCriticalSection(&m_cs);
}

// Walks the RIFF chunks of a WAVE resource for its 'fmt ' and 'data' chunks.
// The waves shipped in res\ are plain 16-bit PCM, but nothing here assumes that
// beyond requiring a format the wave mapper will accept.
BOOL CMeasureSoundPlayer::LoadWave(UINT nResource)
{
	if (m_nLoadedResource == nResource && m_pData)
		return TRUE;

	HMODULE hModule = AfxGetInstanceHandle();
	HRSRC hRsrc = ::FindResource(hModule, MAKEINTRESOURCE(nResource), "WAVE");
	if (!hRsrc)
		return FALSE;

	DWORD dwLen = ::SizeofResource(hModule, hRsrc);
	HGLOBAL hRes = ::LoadResource(hModule, hRsrc);
	const BYTE * pRes = hRes ? (const BYTE *) ::LockResource(hRes) : NULL;
	if (!pRes || dwLen < 12)
		return FALSE;
	if (memcmp(pRes, "RIFF", 4) != 0 || memcmp(pRes + 8, "WAVE", 4) != 0)
		return FALSE;

	WAVEFORMATEX format;
	ZeroMemory(&format, sizeof(format));
	const BYTE * pPcm = NULL;
	DWORD dwPcmSize = 0;
	BOOL bHaveFormat = FALSE;

	// Chunks are { id[4], size[4], payload } with the payload padded to an even
	// length; the pad byte is not counted in size.
	DWORD dwPos = 12;
	while (dwPos + 8 <= dwLen)
	{
		DWORD dwChunkSize;
		memcpy(&dwChunkSize, pRes + dwPos + 4, 4);
		const BYTE * pPayload = pRes + dwPos + 8;
		if (dwChunkSize > dwLen - (dwPos + 8))
			break;

		if (memcmp(pRes + dwPos, "fmt ", 4) == 0)
		{
			DWORD dwCopy = min(dwChunkSize, (DWORD) sizeof(format));
			memcpy(&format, pPayload, dwCopy);
			bHaveFormat = TRUE;
		}
		else if (memcmp(pRes + dwPos, "data", 4) == 0)
		{
			pPcm = pPayload;
			dwPcmSize = dwChunkSize;
		}

		dwPos += 8 + dwChunkSize + (dwChunkSize & 1);
	}

	if (!bHaveFormat || !pPcm || dwPcmSize == 0)
		return FALSE;

	// waveOutPrepareHeader page-locks the buffer, so give it heap memory of our
	// own rather than the resource image. MFC's operator new THROWS: letting that
	// escape would unwind out of CSensor::MeasureColor into the measure worker's
	// catch, which reports an invalid reading -- i.e. a failed audio allocation
	// would fail the patch. Swallow it and go quiet instead.
	BYTE * pCopy = NULL;
	try
	{
		pCopy = new BYTE [ dwPcmSize ];
	}
	catch (CMemoryException * e)
	{
		e->Delete();
		return FALSE;
	}
	catch (...)
	{
		return FALSE;
	}

	memcpy(pCopy, pPcm, dwPcmSize);

	delete [] m_pData;
	m_pData = pCopy;
	m_dwDataSize = dwPcmSize;
	m_format = format;
	m_format.cbSize = 0;			// no extra format bytes are copied above
	m_nLoadedResource = nResource;
	return TRUE;
}

BOOL CMeasureSoundPlayer::OpenDevice()
{
	if (m_hWaveOut)
		return TRUE;

	// CALLBACK_NULL: nothing needs to be notified when a cue finishes. The next
	// Play() resets the device anyway, and Close() resets before closing.
	MMRESULT mr = ::waveOutOpen(&m_hWaveOut, WAVE_MAPPER, &m_format, 0, 0, CALLBACK_NULL);
	if (mr != MMSYSERR_NOERROR)
	{
		m_hWaveOut = NULL;
		return FALSE;
	}
	return TRUE;
}

void CMeasureSoundPlayer::Play(UINT nResource)
{
	EnterCriticalSection(&m_cs);

	UINT nPrevious = m_nLoadedResource;
	if (LoadWave(nResource))
	{
		// A different wave may want a different device format. In practice all
		// three cues share one, so this reopen never fires during a sweep.
		if (m_hWaveOut && nPrevious != nResource)
			CloseDevice();

		if (OpenDevice())
		{
			if (m_bPrepared)
			{
				::waveOutReset(m_hWaveOut);		// also marks the buffer done
				::waveOutUnprepareHeader(m_hWaveOut, &m_header, sizeof(m_header));
				m_bPrepared = FALSE;
			}

			ZeroMemory(&m_header, sizeof(m_header));
			m_header.lpData = (LPSTR) m_pData;
			m_header.dwBufferLength = m_dwDataSize;

			if (::waveOutPrepareHeader(m_hWaveOut, &m_header, sizeof(m_header)) == MMSYSERR_NOERROR)
			{
				m_bPrepared = TRUE;
				::waveOutWrite(m_hWaveOut, &m_header, sizeof(m_header));
			}
		}
	}

	LeaveCriticalSection(&m_cs);
}

void CMeasureSoundPlayer::CloseDevice()
{
	if (m_hWaveOut)
	{
		::waveOutReset(m_hWaveOut);
		if (m_bPrepared)
		{
			::waveOutUnprepareHeader(m_hWaveOut, &m_header, sizeof(m_header));
			m_bPrepared = FALSE;
		}
		::waveOutClose(m_hWaveOut);
		m_hWaveOut = NULL;
	}
}

void CMeasureSoundPlayer::Close()
{
	EnterCriticalSection(&m_cs);

	CloseDevice();

	delete [] m_pData;
	m_pData = NULL;
	m_dwDataSize = 0;
	m_nLoadedResource = 0;

	LeaveCriticalSection(&m_cs);
}
