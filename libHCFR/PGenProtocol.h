/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2012 Association Homecinema Francophone.  All rights reserved.
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
//  Author(s):
//  PhoenixF15E
/////////////////////////////////////////////////////////////////////////////

// PGenProtocol.h: the decisions HCFR takes on the PGenerator's port-85 replies,
// kept free of sockets and MFC so tests/ColorMathTest can check them (T53).
// Generators/Generator.cpp and gdigeneproppage.cpp do the talking; every reply
// string below has already had its frame byte stripped by PgSend.
//
// Renderer state: GET_PGENERATOR_IS_EXECUTED answers with the renderer's PID
// ("...Pid...") while it runs and "Not executed" while it is down or
// restarting. Firmware without the command answers something else. An empty
// reply is a timeout: the daemon was busy or the frame arrived late, which says
// nothing about the renderer or the firmware.
//
// Apply sends its conf batch and RESTARTPGENERATOR only into a running
// renderer: a batch and restart sent while the generator was still restarting
// hung the 1.6 daemon until a power cycle (2026-11-05).

#ifndef PGENPROTOCOL_H_INCLUDED
#define PGENPROTOCOL_H_INCLUDED

#include <string.h>

namespace PGenProtocol
{
	// Legal black on the wire. Under Dolby Vision the 1.6 renderer shows code 0 as
	// a full red field (it is below DV black; measured 2026-11-05), so every black
	// HCFR itself sends is legal black there: 16 for an 8-bit command, 64 for a
	// 10-bit one. Outside DV black is 0, as it always was.
	inline int LegalBlackCode(bool dolbyVision, int bits)
	{
		if (!dolbyVision) return 0;
		return (bits == 10) ? 64 : 16;
	}

	// The Dolby Vision tunnel on 1.6 is limited range only: full-range codes would
	// put 0..15 below DV black. A measurement in full range under DV is refused at
	// Init rather than rewritten patch by patch.
	inline bool RangeAllowedForDolbyVision(bool dolbyVision, bool limitedRange)
	{
		return !dolbyVision || limitedRange;
	}

	enum ExecReply { EXEC_EMPTY = 0, EXEC_PID, EXEC_NOT_EXECUTED, EXEC_OTHER };

	inline ExecReply ClassifyExec(const char* r)
	{
		if (!r) return EXEC_EMPTY;
		const char* p = r;
		while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
		if (!*p) return EXEC_EMPTY;
		if (strstr(r, "Pid")) return EXEC_PID;
		if (strstr(r, "Not executed")) return EXEC_NOT_EXECUTED;
		return EXEC_OTHER;
	}

	// A reply that shows the firmware knows GET_PGENERATOR_IS_EXECUTED. Only such a
	// reply may mark the firmware as PID-reporting; nothing ever unmarks it.
	inline bool ConfirmsPidReporting(ExecReply c)
	{
		return c == EXEC_PID || c == EXEC_NOT_EXECUTED;
	}

	// RESTARTPGENERATOR refused (PGenerator-Plus answers ERR:Refused or
	// OK:ERR:Refused). Anything else, including no reply, is not a refusal.
	inline bool RestartRefused(const char* r)
	{
		if (!r) return false;
		const char* p = r;
		while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
		return strstr(p, "Refused") != NULL || strncmp(p, "ERR", 3) == 0 || strstr(p, "OK:ERR") != NULL;
	}

	// Before the batch: what one probe of the renderer means. probesDone counts
	// the probe just answered (1 for the first).
	enum PreRestart
	{
		PRE_PROBE_AGAIN = 0,	// empty (or garbled, on known firmware): ask again
		PRE_SEND,				// renderer up, PID known: send, restart, wait for a new PID
		PRE_WAIT_FOR_RENDERER,	// renderer down or restarting: wait for a PID before sending
		PRE_SEND_LEGACY,		// firmware without the command: send, fixed grace period
		PRE_SEND_UNVERIFIED		// never answered, firmware unknown: send, then poll
	};
	inline PreRestart PreRestartStep(ExecReply c, int probesDone, int maxProbes, bool firmwareReportsPid)
	{
		if (c == EXEC_PID) return PRE_SEND;
		if (c == EXEC_NOT_EXECUTED) return PRE_WAIT_FOR_RENDERER;
		if (c == EXEC_OTHER && !firmwareReportsPid) return PRE_SEND_LEGACY;
		if (probesDone < maxProbes) return PRE_PROBE_AGAIN;
		return firmwareReportsPid ? PRE_WAIT_FOR_RENDERER : PRE_SEND_UNVERIFIED;
	}

	// While waiting for the renderer: what one probe means. pidIsNew is true when
	// the reply differs from the PID read before the restart (a reply from the
	// not-yet-killed renderer must not pass). timedOut is the caller's deadline.
	enum PostRestart
	{
		POST_KEEP_POLLING = 0,
		POST_UP,				// a new renderer PID
		POST_DOWN,				// PID-reporting firmware, no new PID by the deadline
		POST_UP_UNVERIFIED		// firmware that cannot tell: assume up, as before
	};
	inline PostRestart PostRestartStep(ExecReply c, bool pidIsNew, bool timedOut, bool firmwareReportsPid)
	{
		if (c == EXEC_PID && pidIsNew) return POST_UP;
		if (c == EXEC_OTHER && !firmwareReportsPid) return POST_UP_UNVERIFIED;
		if (!timedOut) return POST_KEEP_POLLING;
		return firmwareReportsPid ? POST_DOWN : POST_UP_UNVERIFIED;
	}

	// A conf value as an integer, or `unknown` when the reply carried none (an
	// empty or bare "OK:" reply, which atoi would read as 0). Digits only, with
	// an optional leading minus; surrounding blanks are already trimmed.
	inline int ConfIntOr(const char* v, int unknown)
	{
		if (!v) return unknown;
		const char* p = v;
		if (*p == '-') p++;
		if (!*p) return unknown;
		int n = 0;
		for (; *p; p++)
		{
			if (*p < '0' || *p > '9') return unknown;
			if (n > 100000000) return unknown;
			n = n * 10 + (*p - '0');
		}
		return (*v == '-') ? -n : n;
	}

	// DV state from the two conf reads (values as PgenParseVal returns them). DV is
	// on only with BOTH flags; a value that is not exactly "0" or "1" means the
	// read failed, and a failed read keeps the last known state (black would go
	// out as 0, a red field, if a lost reply turned a known DV state off).
	inline bool DoviStateFromConf(const char* stdDovi, const char* llDovi, bool previous)
	{
		int a = ConfIntOr(stdDovi, -1), b = ConfIntOr(llDovi, -1);
		if ((a != 0 && a != 1) || (b != 0 && b != 1)) return previous;
		return a == 1 && b == 1;
	}

	// The settings dialog's DoVi mode combo: 0 Verify (map mode 1), 1 Calibrate
	// (2), 2 Perceptual (0). An unknown map mode (-1) shows as Verify, the
	// behavior before Perceptual existed.
	inline int DoviModeToComboIndex(int mapMode)
	{
		if (mapMode == 2) return 1;
		if (mapMode == 0) return 2;
		return 0;
	}
	inline int ComboIndexToDoviMode(int sel)
	{
		if (sel == 1) return 2;
		if (sel == 2) return 0;
		return 1;
	}
}

#endif
