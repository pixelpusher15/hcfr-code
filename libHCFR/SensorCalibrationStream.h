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

// SensorCalibrationStream.h: the versioned calibration fields of a CSensor
// stream - everything CSensor::Serialize writes after the sensor matrix.
//
// CSensor itself is an application class (it needs the configuration, the
// property pages and the generators), so it cannot be linked into the unit
// tests. Its on-disk layout can: this codec owns the version number, the
// fields and every refusal, and CSensor::Serialize only copies its members
// in and out. The legacy version-1 method derivation stays in CSensor,
// because it reads the user's configuration.

#ifndef SENSORCALIBRATIONSTREAM_H_INCLUDED
#define SENSORCALIBRATIONSTREAM_H_INCLUDED

#include "libHCFR_Config.h"

#ifdef LIBHCFR_HAS_MFC

#include <afx.h>
#include <time.h>
#include "Color.h"
#include "SpectralCorrection.h"

// The version every non-legacy CSensor stream is written with. Version 4
// is the time followed by the extension trailer; every field after the
// time is a tag (ArchiveTags.h, CSensorTag). Versions 2 and 3 were
// unreleased development layouts and are refused.
static const int kSensorStreamVersion = 4;

struct SensorCalibrationFields
{
	SensorCalibrationFields();

	time_t	calibrationTime;
	int		method;				// CalibrationMatrixMethod
	bool	rawlessEmbody;		// RawlessReadingsState::Get()
	Matrix	bodnerRaw[3];		// meaningful only when method == CALIB_BODNER_THREEMATRIX
	Matrix	bodnerCal[3];
	int		fitBasis;			// SpectralFitBasis of the method's matrices; UNKNOWN when absent
	SpectralCorrectionRecord spectral;	// source NONE when absent
};

// Writes the block version, then the fields that version carries. Version 1
// (the Debug\SaveOldCalibrationFile downgrade) carries the time only; any
// other value must be kSensorStreamVersion.
void StoreSensorCalibration(CArchive& ar, const SensorCalibrationFields& f, int version);

// Reads the block version and its fields into f and returns the version.
// For version 1 only calibrationTime is set; the caller derives the method
// and the raw-less state. fitBasis and spectral are always set: to the
// tags' values, or to their absent defaults (UNKNOWN, no record).
// Throws CNewerSchemaException for a version this build refuses, and
// CArchiveException for a malformed stream. f is the caller's scratch: on a
// throw it may hold part of the stream, so copy it out only on return.
int LoadSensorCalibration(CArchive& ar, SensorCalibrationFields& f);

#endif // LIBHCFR_HAS_MFC

#endif // SENSORCALIBRATIONSTREAM_H_INCLUDED
