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

// SensorCalibrationStream.cpp: the CSensor calibration-field codec.
// Layout and contract are documented in SensorCalibrationStream.h.

#include "SensorCalibrationStream.h"

#ifdef LIBHCFR_HAS_MFC

#include "ArchiveExtension.h"
#include "ArchiveTags.h"

SensorCalibrationFields::SensorCalibrationFields()
	: calibrationTime(0), method(CALIB_HCFR_DEFAULT), rawlessEmbody(true)
{
	for ( int k = 0; k < 3; k++ )
	{
		bodnerRaw[k] = Matrix::IdentityMatrix(3);
		bodnerCal[k] = Matrix::IdentityMatrix(3);
	}
}

void StoreSensorCalibration(CArchive& ar, const SensorCalibrationFields& f, int version)
{
	ASSERT(version == 1 || version == kSensorStreamVersion);

	ar << version;
	ar << f.calibrationTime;
	if ( version == 1 )
		return;

	// v3: the method, the raw-less state and (Bodner only) the six sub-gamut
	// matrices, then the extension trailer as the last thing in the block.
	ar << f.method;
	BOOL bRawlessEmbody = f.rawlessEmbody ? TRUE : FALSE;
	ar << bRawlessEmbody;
	if ( f.method == CALIB_BODNER_THREEMATRIX )
	{
		for ( int k = 0; k < 3; k++ )
		{
			Matrix raw = f.bodnerRaw[k];	// Matrix::Serialize is not const
			Matrix cal = f.bodnerCal[k];
			raw.Serialize(ar);
			cal.Serialize(ar);
		}
	}

	CExtensionWriter ext;
	ext.Write ( ar );
}

int LoadSensorCalibration(CArchive& ar, SensorCalibrationFields& f)
{
	int version;
	ar >> version;
	if ( version > 3 )
		throw new CNewerSchemaException ( "CSensor", version );
	if ( version == 2 )
	{
		// The unreleased Bodner layout. Parsing it as v3 would read its
		// raw-less state from the first matrix and the trailer length from
		// inside the matrices, desyncing unpredictably, so it is refused
		// outright: regenerate the file with this build.
		throw new CNewerSchemaException ( "CSensor", version, true );
	}
	if ( version < 1 )
		AfxThrowArchiveException ( CArchiveException::badSchema );
	ar >> f.calibrationTime;

	if ( version == 3 )
	{
		// Unknown tags are forward-compatible; unknown values inside a
		// known field are not. A method this build does not know would
		// silently run through the wrong correction, so it is refused as a
		// file from a newer HCFR.
		int method;
		ar >> method;
		if ( method < CALIB_CLASSIC_NIST || method > CALIB_FCMM_NO_LUM )
			throw new CNewerSchemaException ( "CSensor", method, false );
		f.method = method;

		BOOL bRawlessEmbody;
		ar >> bRawlessEmbody;
		f.rawlessEmbody = ( bRawlessEmbody != FALSE );

		if ( f.method == CALIB_BODNER_THREEMATRIX )
		{
			for ( int k = 0; k < 3; k++ )
			{
				f.bodnerRaw[k].Serialize(ar);
				f.bodnerCal[k].Serialize(ar);
			}
		}

		// Extension trailer, the last thing in the block. No tag is
		// written here yet; any a newer build adds is skipped by length.
		CExtensionReader ext(ar);
		BYTE  tag;
		DWORD cb;
		while ( ext.Next(tag, cb) )
			ext.Skip();
		ext.Close();
	}
	return version;
}

#endif // LIBHCFR_HAS_MFC
