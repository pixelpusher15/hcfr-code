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

// WireSetup.cpp: the process-wide display record behind the wire-setup stamp.
// See WireSetup.h.

#include "WireSetup.h"
#include "CriticalSection.h"
#include "LockWhileInScope.h"

namespace
{
	struct DisplayRecord
	{
		bool            valid;
		double          stimulus[3];
		CColorWireSetup setup;
		unsigned long   serial;
		DisplayRecord() : valid(false), serial(0) { stimulus[0] = stimulus[1] = stimulus[2] = 0.0; }
	};

	// Function-local statics: constructed on first use, so a generator or a
	// sensor in another translation unit's static initializer still finds them.
	CriticalSection& Lock()
	{
		static CriticalSection s_lock;
		return s_lock;
	}
	DisplayRecord& State()
	{
		static DisplayRecord s_record;
		return s_record;
	}
}

namespace WireDisplayRecord
{
	void Record(const double stimulus[3], const CColorWireSetup& setup)
	{
		CLockWhileInScope guard(Lock());
		DisplayRecord& r = State();
		r.valid = true;
		r.stimulus[0] = stimulus[0];
		r.stimulus[1] = stimulus[1];
		r.stimulus[2] = stimulus[2];
		r.setup = setup;
		r.setup.realDrive = false;
		r.serial++;
	}

	void Invalidate()
	{
		CLockWhileInScope guard(Lock());
		DisplayRecord& r = State();
		r.valid = false;
		r.serial++;
	}

	void NoteSurfacePaint(int outputMode, const int shownCode[3])
	{
		CLockWhileInScope guard(Lock());
		DisplayRecord& r = State();
		if ( ! r.valid || r.setup.outputMode != outputMode )
			return;
		if ( r.setup.wireCode[0] == shownCode[0] && r.setup.wireCode[1] == shownCode[1]
			&& r.setup.wireCode[2] == shownCode[2] )
			return;
		r.valid = false;
		r.serial++;
	}

	bool Peek(double stimulus[3], CColorWireSetup* pSetup)
	{
		CLockWhileInScope guard(Lock());
		const DisplayRecord& r = State();
		if ( ! r.valid )
			return false;
		stimulus[0] = r.stimulus[0];
		stimulus[1] = r.stimulus[1];
		stimulus[2] = r.stimulus[2];
		if ( pSetup )
			*pSetup = r.setup;
		return true;
	}

	void RekeyToRequested(const double displayed[3], const double requested[3], const int requestedCodes[3])
	{
		CLockWhileInScope guard(Lock());
		DisplayRecord& r = State();
		if ( ! r.valid || r.stimulus[0] != displayed[0] || r.stimulus[1] != displayed[1]
			|| r.stimulus[2] != displayed[2] )
			return;
		for ( int c = 0; c < 3; c ++ )
			if ( r.setup.wireCode[c] < 0 || r.setup.wireCode[c] != requestedCodes[c] )
				return;
		r.stimulus[0] = requested[0];
		r.stimulus[1] = requested[1];
		r.stimulus[2] = requested[2];
		r.serial++;
	}

	CColorWireSetup StampFor(const double stimulus[3], unsigned long* pSerial)
	{
		CLockWhileInScope guard(Lock());
		const DisplayRecord& r = State();
		if ( pSerial )
			*pSerial = r.serial;
		CColorWireSetup out;
		if ( r.valid && r.stimulus[0] == stimulus[0] && r.stimulus[1] == stimulus[1]
			&& r.stimulus[2] == stimulus[2] )
		{
			out = r.setup;
			out.realDrive = true;
		}
		return out;
	}

	bool StillCurrent(unsigned long serial)
	{
		CLockWhileInScope guard(Lock());
		return State().serial == serial;
	}
}
