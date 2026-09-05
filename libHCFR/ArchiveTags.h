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

// ArchiveTags.h: the central registry of extension-trailer tags (issue #193).
//
// Every versioned block in a .chc / .thc stream ends with a length-prefixed
// extension region (see ArchiveExtension.h). Inside it, each optional field is
// a tagged entry, and the tag numbers are allocated HERE, per block, so that
// two branches cannot claim the same number for different fields the way the
// tens-digit CColor flags once collided.
//
// Rules:
//   - Never renumber. Never reuse a retired number. Append only.
//   - Tags are per block: COLOR_RAW_XYZ = 1 and SENSOR_CALIBRATION_METHOD = 1
//     are unrelated.
//   - 0 and 255 are reserved and never written.
//   - A number listed as reserved (no writer or reader yet) is still taken.

#ifndef ARCHIVETAGS_H_INCLUDED
#define ARCHIVETAGS_H_INCLUDED

namespace ArchiveTag
{
	// CSensor (Sensors/Sensor.cpp), trailer from block version 3.
	enum CSensorTag
	{
		SENSOR_CALIBRATION_METHOD = 1,	// int: CalibrationMatrixMethod
		SENSOR_BODNER_MATRICES    = 2,	// 6 x Matrix::Serialize: raw[0..2] then cal[0..2]
		SENSOR_CALIBRATION_PAIRS  = 3	// reserved (Stage 2): raw / reference reading pairs
	};

	// COneDeviceSensor (Sensors/OneDeviceSensor.cpp), trailer from block version 5.
	// All reserved for Stage 2 (identity); Stage 1 writes an empty trailer.
	enum COneDeviceSensorTag
	{
		ODS_COLORIMETER          = 1,
		ODS_DISPLAY              = 2,
		ODS_REFERENCE_INSTRUMENT = 3,
		ODS_PRODUCER             = 4,
		ODS_NOTES                = 5
	};

	// CColor (libHCFR/Color.cpp), trailer from block version 100 + base.
	enum CColorTag
	{
		COLOR_RAW_XYZ    = 1,	// Matrix::Serialize (3x1): uncorrected sensor reading
		COLOR_STIMULUS   = 2,	// Matrix::Serialize (3x1): drive stimulus, RGB percent
		COLOR_PROVENANCE = 3,	// reserved for csv-provenance: 4 x int, CString
		COLOR_CONTEXT    = 4,	// reserved (Stage 2): int index into DOC_CONTEXTS
		COLOR_TIMESTAMP  = 5	// reserved (Stage 2): double, seconds since the Unix epoch
	};

	// CMeasure (Measure.cpp), trailer from block version 22.
	enum CMeasureTag
	{
		MEASURE_USER_PATCH_SETS = 1	// reserved for csv-provenance
	};

	// The .chc document header (datasetdoc.cpp), trailer from file version 5.
	enum DocumentTag
	{
		DOC_PRODUCER = 1,	// reserved (Stage 2)
		DOC_DISPLAY  = 2,	// reserved (Stage 2)
		// 3 is retired: it was MEASURING_INSTRUMENT, superseded by row 0 of
		// DOC_CONTEXTS. Never reuse it.
		DOC_CONTEXTS = 4	// reserved (Stage 2): measurement-context table
	};

	// Blocks with no tags yet still write a trailer; it simply has cb = 0.
	// Stage 1 writes and reads ONLY CSensorTag and CColorTag entries.
}

#endif
