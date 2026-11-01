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
	: calibrationTime(0), method(CALIB_HCFR_DEFAULT), rawlessEmbody(true),
	  fitBasis(FIT_BASIS_UNKNOWN)
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

	// v4: everything after the time is a tag in the extension trailer, the
	// last thing in the block. The method, the fit basis and the raw-less
	// state always; the six Bodner matrices only when that method is in force;
	// the spectral correction record only when one is present.
	CExtensionWriter ext;

	ext.BeginTag ( ArchiveTag::SENSOR_CALIBRATION_METHOD ) << f.method;
	ext.EndTag ();

	if ( f.method == CALIB_BODNER_THREEMATRIX )
	{
		CArchive & value = ext.BeginTag ( ArchiveTag::SENSOR_BODNER_MATRICES );
		int k;
		for ( k = 0; k < 3; k++ )
		{
			Matrix raw = f.bodnerRaw[k];	// Matrix::Serialize is not const
			raw.Serialize(value);
		}
		for ( k = 0; k < 3; k++ )
		{
			Matrix cal = f.bodnerCal[k];
			cal.Serialize(value);
		}
		ext.EndTag ();
	}

	ext.BeginTag ( ArchiveTag::SENSOR_FIT_BASIS ) << f.fitBasis;
	ext.EndTag ();

	if ( f.spectral.IsPresent() )
	{
		SpectralCorrectionRecord rec = f.spectral;	// Serialize is not const
		rec.Serialize ( ext.BeginTag ( ArchiveTag::SENSOR_SPECTRAL_CORRECTION ) );
		ext.EndTag ();
	}

	ext.BeginTag ( ArchiveTag::SENSOR_RAWLESS_EMBODY ) << (BOOL) ( f.rawlessEmbody ? TRUE : FALSE );
	ext.EndTag ();

	ext.Write ( ar );
}

int LoadSensorCalibration(CArchive& ar, SensorCalibrationFields& f)
{
	int version;
	ar >> version;
	if ( version > kSensorStreamVersion )
		throw new CNewerSchemaException ( "CSensor", version );
	if ( version == 2 || version == 3 )
	{
		// Two unreleased development layouts, neither in any release: v2 was
		// the first Bodner layout, v3 the #165 branch's inline method and
		// raw-less state. Neither has a trailer, so reading either as v4 would
		// take a field for the trailer length and desync unpredictably. They
		// are refused outright: regenerate the file with this build.
		throw new CNewerSchemaException ( "CSensor", version, true );
	}
	if ( version < 1 )
		AfxThrowArchiveException ( CArchiveException::badSchema );
	ar >> f.calibrationTime;
	// Neither is in a version-1 block, and either tag may be absent from a
	// version-4 one (PR-A wrote neither): basis UNKNOWN, no record.
	f.fitBasis = FIT_BASIS_UNKNOWN;
	f.spectral.Clear();

	if ( version == kSensorStreamVersion )
	{
		CExtensionReader ext(ar);
		BYTE  tag;
		DWORD cb;

		bool bSawBodnerMatrices = false;
		bool bSawRawless = false;
		f.method = CALIB_HCFR_DEFAULT;		// when the tag is absent
		while ( ext.Next(tag, cb) )
		{
			switch ( tag )
			{
				case ArchiveTag::SENSOR_CALIBRATION_METHOD:
				{
					// Unknown tags are forward-compatible; unknown values inside a
					// known tag are not. A method this build does not know would
					// silently run through the wrong correction, so it is refused
					// as a file from a newer HCFR. The block version is one this
					// build reads, so the exception names the feature instead;
					// the message must not quote a version as the reason.
					int method;
					ext.Value() >> method;
					if ( method < CALIB_CLASSIC_NIST || method >= CALIB_METHOD_COUNT )
						throw new CNewerSchemaException ( "CSensor", version, false, "calibration method" );
					f.method = method;
					break;
				}

				case ArchiveTag::SENSOR_BODNER_MATRICES:
				{
					CArchive & value = ext.Value();
					bSawBodnerMatrices = true;
					int k;
					for ( k = 0; k < 3; k++ )
						f.bodnerRaw[k].Serialize(value);
					for ( k = 0; k < 3; k++ )
						f.bodnerCal[k].Serialize(value);
					break;
				}

				case ArchiveTag::SENSOR_FIT_BASIS:
				{
					// Same rule as the method: a basis this build does not know
					// would compose (or not) the wrong matrices.
					int basis;
					ext.Value() >> basis;
					if ( basis < FIT_BASIS_UNKNOWN || basis > FIT_BASIS_DRIVER_CORRECTED )
						throw new CNewerSchemaException ( "CSensor", version, false, "fit basis" );
					f.fitBasis = basis;
					break;
				}

				case ArchiveTag::SENSOR_SPECTRAL_CORRECTION:
				{
					// A written record always names its source and its basis; a
					// value outside the known ones comes from a newer HCFR. Source
					// NONE is not one of those: the writer emits this tag only for a
					// present record, so NONE here is corruption.
					f.spectral.Serialize ( ext.Value() );
					if ( f.spectral.source == SPECTRAL_SOURCE_NONE )
						AfxThrowArchiveException ( CArchiveException::badSchema );
					if ( f.spectral.source < SPECTRAL_SOURCE_FILE || f.spectral.source > SPECTRAL_SOURCE_DISPLAY_TYPE )
						throw new CNewerSchemaException ( "CSensor", version, false, "spectral correction source" );
					if ( f.spectral.basis < FIT_BASIS_NATIVE || f.spectral.basis > FIT_BASIS_DRIVER_CORRECTED )
						throw new CNewerSchemaException ( "CSensor", version, false, "spectral correction basis" );
					break;
				}

				case ArchiveTag::SENSOR_RAWLESS_EMBODY:
				{
					BOOL bEmbody;
					ext.Value() >> bEmbody;
					f.rawlessEmbody = ( bEmbody != FALSE );
					bSawRawless = true;
					break;
				}

				default:
					ext.Skip();
					break;
			}
		}
		ext.Close();

		// The Bodner method is meaningless without its six matrices: loading
		// it anyway would recalibrate every reading through identity
		// sub-gamuts and report success.
		if ( f.method == CALIB_BODNER_THREEMATRIX && ! bSawBodnerMatrices )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		// Same reason for the spectral method: its matrix is the record's
		// correction, and without the record nothing says what it corrects.
		if ( f.method == CALIB_SPECTRAL_SAMPLE && ! f.spectral.IsPresent() )
			AfxThrowArchiveException ( CArchiveException::badSchema );

		// No raw-less state stored: a Bodner sensor's raw-less readings are
		// stranded (its single matrix is identity), a matrix sensor's embody
		// its matrix - the rule the #165 branch applied to its own v2 files.
		if ( ! bSawRawless )
			f.rawlessEmbody = ( f.method != CALIB_BODNER_THREEMATRIX );
	}
	return version;
}

#endif // LIBHCFR_HAS_MFC
