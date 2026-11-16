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

// WireSetup.h: the per-reading wire-setup stamp (CColor tag COLOR_WIRE_SETUP)
// and the process-wide record of what the generator last put on the wire.
//
// A reading's stimulus (COLOR_STIMULUS) is the RGB percent the measuring code
// asked for. On its own it cannot say whether that patch was on the screen for
// the reading, or how it was encoded on the wire. The stamp records those facts
// at measure time, so a consumer never has to infer them:
//
//   realDrive      true only when a generator recorded that it displayed
//                  exactly this stimulus, and nothing else was displayed
//                  between that record and the end of the reading.
//   wireBits       bits per channel of the codes HCFR sent (8, 10); 0 = unknown.
//   encodingRange  the mapping HCFR applied from percent to codes
//                  (WIRE_RANGE_*); unknown when HCFR applied none (madVR,
//                  Chromecast take a 0..1 fraction and quantize themselves).
//   deviceRange    the generator device's own output range, when the device
//                  reports one for this reading; unknown otherwise.
//   generatorId    which HCFR generator recorded the display (WIRE_GENERATOR_*).
//   outputMode     that generator's output mode (the DisplayMode int of the
//                  GDI generator: 0 GDI, 1 overlay, 2 madVR, 3 GDI no
//                  background, 4 Chromecast, 5 test-color window, 6 rPI /
//                  PGenerator, 7 VMR9); -1 = unknown.
//   wireCode[3]    the integer R, G, B codes sent after quantization, when HCFR
//                  computed them; -1 = unknown.
//
// Every field other than realDrive is meaningful only when realDrive is true:
// without a display record the stamp carries the defaults. A reading with no
// stamp at all (every file written before this field) is unknown.
//
// MFC-free: the CArchive encoding lives in Color.cpp beside the other CColor
// trailer entries.

#ifndef WIRESETUP_H_INCLUDED
#define WIRESETUP_H_INCLUDED

// Values are stored; never renumber. Same numbering as CSV_RANGE_* (Color.h).
enum CColorWireRange
{
	WIRE_RANGE_UNKNOWN = -1,
	WIRE_RANGE_FULL    = 0,	// 0-255 / 0-1023
	WIRE_RANGE_LIMITED = 1	// 16-235 / 64-940
};

// Which generator recorded the display. Values are stored; never renumber.
enum CColorWireGenerator
{
	WIRE_GENERATOR_UNKNOWN = 0,
	WIRE_GENERATOR_GDI     = 1	// CGDIGenerator, every DisplayMode
};

struct CColorWireSetup
{
	bool realDrive;
	int  wireBits;
	int  encodingRange;		// CColorWireRange
	int  deviceRange;		// CColorWireRange
	int  generatorId;		// CColorWireGenerator
	int  outputMode;
	int  wireCode[3];

	CColorWireSetup()
		: realDrive(false), wireBits(0), encodingRange(WIRE_RANGE_UNKNOWN),
		  deviceRange(WIRE_RANGE_UNKNOWN), generatorId(WIRE_GENERATOR_UNKNOWN),
		  outputMode(-1)
	{
		wireCode[0] = wireCode[1] = wireCode[2] = -1;
	}

	bool operator==(const CColorWireSetup& o) const
	{
		return realDrive == o.realDrive && wireBits == o.wireBits
			&& encodingRange == o.encodingRange && deviceRange == o.deviceRange
			&& generatorId == o.generatorId && outputMode == o.outputMode
			&& wireCode[0] == o.wireCode[0] && wireCode[1] == o.wireCode[1]
			&& wireCode[2] == o.wireCode[2];
	}
	bool operator!=(const CColorWireSetup& o) const { return !(*this == o); }
};

// The record a generator leaves when it has put a patch on the wire, and the
// stamp a sensor read takes from it. Thread-safe: generators record on the UI
// or continuous-measure thread, sensors read on the async measurer and the
// simultaneous-measure threads.
//
// Every change - a record, an invalidation - advances a serial. A read takes
// its stamp and the serial before it starts and keeps realDrive only if the
// serial is unchanged when it ends, so a pattern, a recalibration screen or any
// other display that lands during the read turns the stamp back to unknown.
namespace WireDisplayRecord
{
	// The generator displayed `stimulus` (RGB percent, exactly the triplet it was
	// handed) with this wire setup. setup.realDrive is ignored.
	void Record(const double stimulus[3], const CColorWireSetup& setup);

	// Anything else reached the screen, or the generator was initialized or
	// released: no earlier record holds any more.
	void Invalidate();

	// A surface repainted itself with these codes. Invalidates the record when it
	// is the surface the record was made on (outputMode) and the codes differ.
	void NoteSurfacePaint(int outputMode, const int shownCode[3]);

	// The current record, when there is one: its stimulus and setup.
	bool Peek(double stimulus[3], CColorWireSetup* pSetup);

	// The generator was asked for `requested` but displayed `displayed` (window
	// Intensity scales a patch by Intensity / 100, which even at 100 can move a
	// percent by one ulp). When the record is for exactly `displayed`, carries
	// codes, and those codes equal `requestedCodes` - the codes `requested` maps to
	// on the same wire - the patch on the wire is exactly the requested one, and
	// the record is re-keyed to `requested`. Otherwise nothing changes, and a read
	// of `requested` finds no record for it.
	void RekeyToRequested(const double displayed[3], const double requested[3], const int requestedCodes[3]);

	// The stamp for a read of `stimulus` starting now: the recorded setup with
	// realDrive = true when a record exists for exactly this stimulus (each
	// channel equal as a double; no tolerance), else the defaults. *pSerial
	// receives the serial to pass to StillCurrent when the read ends.
	CColorWireSetup StampFor(const double stimulus[3], unsigned long* pSerial);

	// True when no record or invalidation happened since StampFor gave `serial`.
	bool StillCurrent(unsigned long serial);
}

#endif // WIRESETUP_H_INCLUDED
