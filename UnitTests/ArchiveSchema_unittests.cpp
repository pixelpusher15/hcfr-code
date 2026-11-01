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

// ArchiveSchema_unittests.cpp: the CSensor calibration stream (issue #193).
//
// CSensor is an application class and cannot be linked here, so these cases
// drive the codec it delegates to (libHCFR/SensorCalibrationStream): every
// byte CSensor::Serialize writes after the sensor matrix. Hand-built streams
// pin the refusals, so a layout this build must not parse is proven refused
// rather than read. The trailer writer's size cap is pinned here too.

#include "Color.h"
#include "ArchiveExtension.h"
#include "ArchiveTags.h"
#include "SensorCalibrationStream.h"
#include <vector>
#include <cppunit/config/SourcePrefix.h>
#include <cppunit/extensions/HelperMacros.h>

#define THIS_TEST_CASE ArchiveSchemaTestCase

namespace
{
	// What a load did.
	enum Outcome { LOADED, NEWER, UNRELEASED, BAD_SCHEMA, END_OF_FILE, OTHER_ERROR };

	struct LoadResult
	{
		Outcome					outcome;
		int						version;		// returned by the codec when LOADED
		int						exVersion;		// CNewerSchemaException::m_version
		CString					exFeature;		// CNewerSchemaException::m_feature
		SensorCalibrationFields	fields;
		int						sentinel;		// the int that follows the block, when asked for
	};

	// A storing archive over a growing CMemFile; Bytes() closes it.
	class StreamBuilder
	{
	public:
		StreamBuilder() : m_ar(&m_mem, CArchive::store) {}
		CArchive& Ar() { return m_ar; }
		std::vector<BYTE> Bytes()
		{
			m_ar.Close();
			std::vector<BYTE> out;
			ULONGLONG n = m_mem.GetLength();
			BYTE* p = m_mem.Detach();
			if ( n > 0 )
				out.assign(p, p + (size_t) n);
			free(p);
			return out;
		}
	private:
		CMemFile m_mem;
		CArchive m_ar;
	};

	LoadResult Load(const std::vector<BYTE>& bytes, bool bReadSentinel)
	{
		LoadResult r;
		r.outcome = OTHER_ERROR;
		r.version = -1;
		r.exVersion = -1;
		r.sentinel = 0;

		std::vector<BYTE> copy(bytes);
		if ( copy.empty() )
			copy.push_back(0);
		CMemFile mem(&copy[0], (UINT) bytes.size());
		CArchive ar(&mem, CArchive::load);
		try
		{
			r.version = LoadSensorCalibration(ar, r.fields);
			if ( bReadSentinel )
				ar >> r.sentinel;
			r.outcome = LOADED;
			ar.Close();
		}
		catch ( CNewerSchemaException* e )
		{
			r.outcome = e->m_unreleased ? UNRELEASED : NEWER;
			r.exVersion = e->m_version;
			r.exFeature = e->m_feature;
			e->Delete();
			ar.Abort();
		}
		catch ( CArchiveException* e )
		{
			r.outcome = ( e->m_cause == CArchiveException::badSchema ) ? BAD_SCHEMA
			          : ( e->m_cause == CArchiveException::endOfFile ) ? END_OF_FILE : OTHER_ERROR;
			e->Delete();
			ar.Abort();
		}
		catch ( CException* e )
		{
			e->Delete();
			ar.Abort();
		}
		mem.Detach();
		return r;
	}

	Matrix Distinct(double seed)
	{
		Matrix m(0.0, 3, 3);
		for ( int i = 0; i < 3; i++ )
			for ( int j = 0; j < 3; j++ )
				m(i, j) = seed + 0.125 * (3 * i + j);
		return m;
	}

	bool SameMatrix(const Matrix& a, const Matrix& b)
	{
		if ( a.GetRows() != b.GetRows() || a.GetColumns() != b.GetColumns() )
			return false;
		for ( int i = 0; i < a.GetRows(); i++ )
			for ( int j = 0; j < a.GetColumns(); j++ )
				if ( a(i, j) != b(i, j) )
					return false;
		return true;
	}

	SensorCalibrationFields BodnerFields()
	{
		SensorCalibrationFields f;
		f.calibrationTime = 1789000000;
		f.method = CALIB_BODNER_THREEMATRIX;
		f.rawlessEmbody = false;
		for ( int k = 0; k < 3; k++ )
		{
			f.bodnerRaw[k] = Distinct(1.0 + k);
			f.bodnerCal[k] = Distinct(10.0 + k);
		}
		return f;
	}

	// A spectral correction record with every field away from its default,
	// so a field the codec drops cannot pass for one it kept. The hash has
	// both 32-bit halves set.
	SpectralCorrectionRecord SpectralRecord(int source, int basis)
	{
		SpectralCorrectionRecord rec;
		rec.source = source;
		rec.description = ( source == SPECTRAL_SOURCE_FILE ) ? "WLED PFS Phosphor (Example)" : "LCD (White LED)";
		rec.fileHash = ( source == SPECTRAL_SOURCE_FILE ) ? 0xCBF29CE484222325ULL : 0;
		rec.observer = 2;
		rec.meterSerial = "J010000";
		rec.nativeCal = Distinct(20.0);
		rec.currentCal = Distinct(30.0);
		rec.correction = Distinct(40.0);
		rec.basis = basis;
		return rec;
	}

	// A version-4 block written by hand: the version, the time, then a
	// trailer holding only the tags asked for. kOmit leaves METHOD out,
	// rawless < 0 leaves RAWLESS out, fitBasis == kOmit leaves FIT_BASIS out,
	// and a record whose source is NONE leaves SPECTRAL_CORRECTION out (its
	// payload is written as given, valid or not). With the defaults this is
	// what PR-A's writer produced: no tag 4 or 5.
	const int kOmit = -100000;
	std::vector<BYTE> HandV4(int method, bool bMatrices, int rawless, bool bUnknownFirst, int sentinel,
	                         int fitBasis = kOmit, const SpectralCorrectionRecord& spectral = SpectralCorrectionRecord())
	{
		StreamBuilder s;
		s.Ar() << (int) 4;
		s.Ar() << (time_t) 1789000000;
		CExtensionWriter ext;
		if ( bUnknownFirst )
		{
			CArchive& v = ext.BeginTag(200);
			for ( int i = 0; i < 37; i++ )
				v << (BYTE) 0xA5;
			ext.EndTag();
		}
		if ( method != kOmit )
		{
			ext.BeginTag(ArchiveTag::SENSOR_CALIBRATION_METHOD) << method;
			ext.EndTag();
		}
		if ( bMatrices )
		{
			CArchive& v = ext.BeginTag(ArchiveTag::SENSOR_BODNER_MATRICES);
			int k;
			for ( k = 0; k < 3; k++ ) { Matrix m = Distinct(1.0 + k); m.Serialize(v); }
			for ( k = 0; k < 3; k++ ) { Matrix m = Distinct(10.0 + k); m.Serialize(v); }
			ext.EndTag();
		}
		if ( fitBasis != kOmit )
		{
			ext.BeginTag(ArchiveTag::SENSOR_FIT_BASIS) << fitBasis;
			ext.EndTag();
		}
		if ( spectral.source != SPECTRAL_SOURCE_NONE )
		{
			SpectralCorrectionRecord rec = spectral;
			rec.Serialize(ext.BeginTag(ArchiveTag::SENSOR_SPECTRAL_CORRECTION));
			ext.EndTag();
		}
		if ( rawless >= 0 )
		{
			ext.BeginTag(ArchiveTag::SENSOR_RAWLESS_EMBODY) << (BOOL) rawless;
			ext.EndTag();
		}
		ext.Write(s.Ar());
		s.Ar() << sentinel;
		return s.Bytes();
	}
}

class THIS_TEST_CASE : public CPPUNIT_NS::TestFixture
{
	CPPUNIT_TEST_SUITE(THIS_TEST_CASE);
	CPPUNIT_TEST( SensorCurrentVersionRoundTripBodner );
	CPPUNIT_TEST( SensorCurrentVersionRoundTripMatrixMethods );
	CPPUNIT_TEST( SensorRawlessTagAbsentDefaults );
	CPPUNIT_TEST( SensorUnknownTagSkipped );
	CPPUNIT_TEST( SensorV2AndV3Refused );
	CPPUNIT_TEST( SensorBranchV3BlobRefusedNotParsed );
	CPPUNIT_TEST( SensorUnknownMethodRefused );
	CPPUNIT_TEST( SensorBodnerWithoutMatricesRefused );
	CPPUNIT_TEST( SensorNewerVersionRefused );
	CPPUNIT_TEST( SensorLegacyV1Bytes );
	CPPUNIT_TEST( ExtensionWriterCap );
	CPPUNIT_TEST( SensorSpectralRoundTrip );
	CPPUNIT_TEST( SensorFitBasisAndRecordRoundTrip );
	CPPUNIT_TEST( SensorPraV4StreamLoadsWithSpectralDefaults );
	CPPUNIT_TEST( SensorUnknownFitBasisRefused );
	CPPUNIT_TEST( SensorUnknownRecordValuesRefused );
	CPPUNIT_TEST( SensorSpectralMethodWithoutRecordRefused );
	CPPUNIT_TEST( SensorRecordWithSourceNoneIsCorrupt );
	CPPUNIT_TEST_SUITE_END();

public:
	void setUp()
	{
	}

protected:

	void SensorCurrentVersionRoundTripBodner()
	{
		SensorCalibrationFields in = BodnerFields();
		StreamBuilder s;
		StoreSensorCalibration(s.Ar(), in, kSensorStreamVersion);
		s.Ar() << (int) 0x5EAD;
		std::vector<BYTE> bytes = s.Bytes();

		CPPUNIT_ASSERT( bytes.size() >= 4 );
		CPPUNIT_ASSERT_EQUAL( 4, *(int*) &bytes[0] );		// written as version 4

		LoadResult r = Load(bytes, true);
		CPPUNIT_ASSERT_EQUAL( (int) LOADED, (int) r.outcome );
		CPPUNIT_ASSERT_EQUAL( 4, r.version );
		CPPUNIT_ASSERT( r.fields.calibrationTime == in.calibrationTime );
		CPPUNIT_ASSERT_EQUAL( (int) CALIB_BODNER_THREEMATRIX, r.fields.method );
		CPPUNIT_ASSERT( r.fields.rawlessEmbody == false );
		for ( int k = 0; k < 3; k++ )
		{
			CPPUNIT_ASSERT( SameMatrix(in.bodnerRaw[k], r.fields.bodnerRaw[k]) );
			CPPUNIT_ASSERT( SameMatrix(in.bodnerCal[k], r.fields.bodnerCal[k]) );
		}
		CPPUNIT_ASSERT_EQUAL( 0x5EAD, r.sentinel );			// the block ended where it said
	}

	void SensorCurrentVersionRoundTripMatrixMethods()
	{
		const int methods[3] = { CALIB_CLASSIC_NIST, CALIB_HCFR_DEFAULT, CALIB_FCMM_NO_LUM };
		for ( int m = 0; m < 3; m++ )
		{
			for ( int embody = 0; embody < 2; embody++ )
			{
				SensorCalibrationFields in;
				in.calibrationTime = 1700000000 + m;
				in.method = methods[m];
				in.rawlessEmbody = ( embody != 0 );
				StreamBuilder s;
				StoreSensorCalibration(s.Ar(), in, kSensorStreamVersion);
				s.Ar() << (int) 0x5EAD;
				LoadResult r = Load(s.Bytes(), true);
				CPPUNIT_ASSERT_EQUAL( (int) LOADED, (int) r.outcome );
				CPPUNIT_ASSERT_EQUAL( 4, r.version );
				CPPUNIT_ASSERT_EQUAL( methods[m], r.fields.method );
				CPPUNIT_ASSERT( r.fields.rawlessEmbody == in.rawlessEmbody );
				CPPUNIT_ASSERT( r.fields.calibrationTime == in.calibrationTime );
				CPPUNIT_ASSERT_EQUAL( 0x5EAD, r.sentinel );
			}
		}
	}

	void SensorRawlessTagAbsentDefaults()
	{
		// No RAWLESS tag: a matrix method's readings embody its matrix, a
		// Bodner sensor's do not (its single matrix is identity).
		LoadResult r = Load(HandV4(CALIB_HCFR_DEFAULT, false, -1, false, 77), true);
		CPPUNIT_ASSERT_EQUAL( (int) LOADED, (int) r.outcome );
		CPPUNIT_ASSERT( r.fields.rawlessEmbody == true );
		CPPUNIT_ASSERT_EQUAL( 77, r.sentinel );

		r = Load(HandV4(CALIB_BODNER_THREEMATRIX, true, -1, false, 78), true);
		CPPUNIT_ASSERT_EQUAL( (int) LOADED, (int) r.outcome );
		CPPUNIT_ASSERT( r.fields.rawlessEmbody == false );
		CPPUNIT_ASSERT( SameMatrix(Distinct(2.0), r.fields.bodnerRaw[1]) );
		CPPUNIT_ASSERT( SameMatrix(Distinct(12.0), r.fields.bodnerCal[2]) );
		CPPUNIT_ASSERT_EQUAL( 78, r.sentinel );

		// Present: the tag wins over the default.
		r = Load(HandV4(CALIB_HCFR_DEFAULT, false, FALSE, false, 79), true);
		CPPUNIT_ASSERT_EQUAL( (int) LOADED, (int) r.outcome );
		CPPUNIT_ASSERT( r.fields.rawlessEmbody == false );

		// No METHOD tag at all: the default method.
		r = Load(HandV4(kOmit, false, -1, false, 80), true);
		CPPUNIT_ASSERT_EQUAL( (int) LOADED, (int) r.outcome );
		CPPUNIT_ASSERT_EQUAL( (int) CALIB_HCFR_DEFAULT, r.fields.method );
		CPPUNIT_ASSERT( r.fields.rawlessEmbody == true );
		CPPUNIT_ASSERT_EQUAL( 80, r.sentinel );
	}

	void SensorUnknownTagSkipped()
	{
		LoadResult r = Load(HandV4(CALIB_FCMM_NO_LUM, false, TRUE, true, 81), true);
		CPPUNIT_ASSERT_EQUAL( (int) LOADED, (int) r.outcome );
		CPPUNIT_ASSERT_EQUAL( (int) CALIB_FCMM_NO_LUM, r.fields.method );
		CPPUNIT_ASSERT_EQUAL( 81, r.sentinel );
	}

	void SensorV2AndV3Refused()
	{
		for ( int v = 2; v <= 3; v++ )
		{
			// Whatever follows the version, it is never looked at.
			StreamBuilder s;
			s.Ar() << v;
			s.Ar() << (time_t) 1789000000;
			for ( int i = 0; i < 64; i++ )
				s.Ar() << (int) 0;
			LoadResult r = Load(s.Bytes(), false);
			CPPUNIT_ASSERT_EQUAL( (int) UNRELEASED, (int) r.outcome );
			CPPUNIT_ASSERT_EQUAL( v, r.exVersion );
		}
	}

	void SensorBranchV3BlobRefusedNotParsed()
	{
		// What a build of the #165 branch wrote: version 3, time, int method,
		// BOOL raw-less state, no trailer - then the next block's bytes (a
		// COneDeviceSensor version 5 here). Read as any trailer layout, the
		// next block would be swallowed; it must be refused instead.
		StreamBuilder s;
		s.Ar() << (int) 3;
		s.Ar() << (time_t) 1789000000;
		s.Ar() << (int) CALIB_HCFR_DEFAULT;
		s.Ar() << (BOOL) TRUE;
		s.Ar() << (int) 5;
		LoadResult r = Load(s.Bytes(), false);
		CPPUNIT_ASSERT_EQUAL( (int) UNRELEASED, (int) r.outcome );
		CPPUNIT_ASSERT_EQUAL( 3, r.exVersion );
	}

	void SensorUnknownMethodRefused()
	{
		// A method this build does not know is a file from a newer HCFR. The
		// block version (4) is one this build reads, so the refusal names the
		// feature, never the method value as a version.
		const int bad[3] = { -1, CALIB_METHOD_COUNT, 99 };
		for ( int i = 0; i < 3; i++ )
		{
			LoadResult r = Load(HandV4(bad[i], false, TRUE, false, 0), false);
			CPPUNIT_ASSERT_EQUAL( (int) NEWER, (int) r.outcome );
			CPPUNIT_ASSERT_EQUAL( 4, r.exVersion );
			CPPUNIT_ASSERT( r.exFeature == "calibration method" );
		}
		// The last known method still loads, with what that method requires:
		// Bodner its matrices, the spectral method its record.
		const bool bLastIsSpectral = ( CALIB_METHOD_COUNT - 1 == CALIB_SPECTRAL_SAMPLE );
		LoadResult r = Load(HandV4(CALIB_METHOD_COUNT - 1, CALIB_METHOD_COUNT - 1 == CALIB_BODNER_THREEMATRIX, TRUE, false, 0,
		                           bLastIsSpectral ? (int) FIT_BASIS_NATIVE : kOmit,
		                           bLastIsSpectral ? SpectralRecord(SPECTRAL_SOURCE_FILE, FIT_BASIS_NATIVE) : SpectralCorrectionRecord()), false);
		CPPUNIT_ASSERT_EQUAL( (int) LOADED, (int) r.outcome );
	}

	void SensorBodnerWithoutMatricesRefused()
	{
		LoadResult r = Load(HandV4(CALIB_BODNER_THREEMATRIX, false, FALSE, false, 0), false);
		CPPUNIT_ASSERT_EQUAL( (int) BAD_SCHEMA, (int) r.outcome );
	}

	void SensorNewerVersionRefused()
	{
		StreamBuilder s;
		s.Ar() << (int) 5;
		s.Ar() << (time_t) 1789000000;
		LoadResult r = Load(s.Bytes(), false);
		CPPUNIT_ASSERT_EQUAL( (int) NEWER, (int) r.outcome );
		CPPUNIT_ASSERT_EQUAL( 5, r.exVersion );
		CPPUNIT_ASSERT( r.exFeature.IsEmpty() );	// a version refusal names no feature
	}

	void SensorLegacyV1Bytes()
	{
		// The SaveOldCalibrationFile store: the version and the time, nothing
		// else, exactly as pre-method builds read it.
		SensorCalibrationFields in = BodnerFields();
		in.calibrationTime = 0x01020304;
		StreamBuilder s;
		StoreSensorCalibration(s.Ar(), in, 1);
		std::vector<BYTE> bytes = s.Bytes();
		const BYTE expected[] = { 1, 0, 0, 0, 0x04, 0x03, 0x02, 0x01 };
		CPPUNIT_ASSERT_EQUAL( sizeof expected, bytes.size() );
		for ( size_t i = 0; i < sizeof expected; i++ )
			CPPUNIT_ASSERT_EQUAL( (int) expected[i], (int) bytes[i] );

		LoadResult r = Load(bytes, false);
		CPPUNIT_ASSERT_EQUAL( (int) LOADED, (int) r.outcome );
		CPPUNIT_ASSERT_EQUAL( 1, r.version );
		CPPUNIT_ASSERT( r.fields.calibrationTime == in.calibrationTime );
	}

	// Peak memory is about 200 MB (the 64 MB source vector, the tag's
	// CMemFile and the writer's copy of the value, then the same again for
	// the at-the-cap half). Deliberate: the cap is only reachable that way.
	void ExtensionWriterCap()
	{
		// A trailer the reader would refuse (cbExtension > kMaxExtensionBytes)
		// must never be written: Write throws badSchema before emitting a byte.
		CExtensionWriter ext;
		CArchive& v = ext.BeginTag(1);
		std::vector<BYTE> big(kMaxExtensionBytes, 0x5A);
		v.Write(&big[0], (UINT) big.size());
		ext.EndTag();
		std::vector<BYTE>().swap(big);

		CMemFile mem;
		CArchive ar(&mem, CArchive::store);
		Outcome o = LOADED;
		try
		{
			ext.Write(ar);
		}
		catch ( CArchiveException* e )
		{
			o = ( e->m_cause == CArchiveException::badSchema ) ? BAD_SCHEMA : OTHER_ERROR;
			e->Delete();
		}
		ar.Flush();
		CPPUNIT_ASSERT_EQUAL( (int) BAD_SCHEMA, (int) o );
		CPPUNIT_ASSERT( mem.GetLength() == 0 );
		ar.Abort();

		// At the cap exactly, it is written.
		CExtensionWriter ok;
		CArchive& w = ok.BeginTag(1);
		std::vector<BYTE> fits(kMaxExtensionBytes - 5, 0x5A);
		w.Write(&fits[0], (UINT) fits.size());
		ok.EndTag();
		std::vector<BYTE>().swap(fits);
		CMemFile mem2;
		CArchive ar2(&mem2, CArchive::store);
		ok.Write(ar2);
		ar2.Close();
		CPPUNIT_ASSERT( mem2.GetLength() == 4 + (ULONGLONG) kMaxExtensionBytes );
	}

	// Tags 4 (SENSOR_FIT_BASIS) and 5 (SENSOR_SPECTRAL_CORRECTION), the
	// ccss-as-correction fields.

	void SensorSpectralRoundTrip()
	{
		// The spectral method: its record (tag 5) and the basis (tag 4) come
		// back field for field, and the block ends where it said.
		const int sources[2] = { SPECTRAL_SOURCE_FILE, SPECTRAL_SOURCE_DISPLAY_TYPE };
		for ( int i = 0; i < 2; i++ )
		{
			SensorCalibrationFields in;
			in.calibrationTime = 1789000100 + i;
			in.method = CALIB_SPECTRAL_SAMPLE;
			in.rawlessEmbody = false;
			in.fitBasis = FIT_BASIS_NATIVE;
			in.spectral = SpectralRecord(sources[i], FIT_BASIS_NATIVE);
			StreamBuilder s;
			StoreSensorCalibration(s.Ar(), in, kSensorStreamVersion);
			s.Ar() << (int) 0x5EAD;
			LoadResult r = Load(s.Bytes(), true);
			CPPUNIT_ASSERT_EQUAL( (int) LOADED, (int) r.outcome );
			CPPUNIT_ASSERT_EQUAL( 4, r.version );
			CPPUNIT_ASSERT_EQUAL( (int) CALIB_SPECTRAL_SAMPLE, r.fields.method );
			CPPUNIT_ASSERT_EQUAL( (int) FIT_BASIS_NATIVE, r.fields.fitBasis );
			CPPUNIT_ASSERT( r.fields.spectral.IsPresent() );
			CPPUNIT_ASSERT( r.fields.spectral == in.spectral );
			CPPUNIT_ASSERT( r.fields.rawlessEmbody == false );
			CPPUNIT_ASSERT_EQUAL( 0x5EAD, r.sentinel );
		}
	}

	void SensorFitBasisAndRecordRoundTrip()
	{
		// Every basis under a matrix method and under Bodner, with and
		// without a (dormant) record: tag 4 is always written, tag 5 only
		// when a record is present, and Bodner's matrices still come back.
		const int bases[3] = { FIT_BASIS_UNKNOWN, FIT_BASIS_NATIVE, FIT_BASIS_DRIVER_CORRECTED };
		for ( int bodner = 0; bodner < 2; bodner++ )
		for ( int b = 0; b < 3; b++ )
		for ( int withRecord = 0; withRecord < 2; withRecord++ )
		{
			SensorCalibrationFields in = bodner ? BodnerFields() : SensorCalibrationFields();
			if ( !bodner )
			{
				in.calibrationTime = 1789000200;
				in.method = CALIB_FCMM_NO_LUM;
			}
			in.fitBasis = bases[b];
			if ( withRecord )
				in.spectral = SpectralRecord(SPECTRAL_SOURCE_FILE, FIT_BASIS_DRIVER_CORRECTED);
			StreamBuilder s;
			StoreSensorCalibration(s.Ar(), in, kSensorStreamVersion);
			s.Ar() << (int) 0x5EAD;
			LoadResult r = Load(s.Bytes(), true);
			CPPUNIT_ASSERT_EQUAL( (int) LOADED, (int) r.outcome );
			CPPUNIT_ASSERT_EQUAL( in.method, r.fields.method );
			CPPUNIT_ASSERT_EQUAL( bases[b], r.fields.fitBasis );
			CPPUNIT_ASSERT( r.fields.spectral.IsPresent() == ( withRecord != 0 ) );
			CPPUNIT_ASSERT( r.fields.spectral == in.spectral );
			CPPUNIT_ASSERT( r.fields.rawlessEmbody == in.rawlessEmbody );
			for ( int k = 0; bodner && k < 3; k++ )
			{
				CPPUNIT_ASSERT( SameMatrix(in.bodnerRaw[k], r.fields.bodnerRaw[k]) );
				CPPUNIT_ASSERT( SameMatrix(in.bodnerCal[k], r.fields.bodnerCal[k]) );
			}
			CPPUNIT_ASSERT_EQUAL( 0x5EAD, r.sentinel );
		}
	}

	void SensorPraV4StreamLoadsWithSpectralDefaults()
	{
		// A v4 block as PR-A wrote it (tags 1, 2, 6 only): basis UNKNOWN (what
		// every build before the basis field did) and no record. Loaded into
		// scratch that already holds a basis and a record, so the defaults are
		// proven set by the codec, not inherited from a fresh struct.
		const int methods[4] = { CALIB_CLASSIC_NIST, CALIB_HCFR_DEFAULT, CALIB_BODNER_THREEMATRIX, CALIB_FCMM_NO_LUM };
		for ( int m = 0; m < 4; m++ )
		{
			std::vector<BYTE> bytes = HandV4(methods[m], methods[m] == CALIB_BODNER_THREEMATRIX, TRUE, false, 90 + m);
			CMemFile mem(&bytes[0], (UINT) bytes.size());
			CArchive ar(&mem, CArchive::load);
			SensorCalibrationFields f;
			f.fitBasis = FIT_BASIS_NATIVE;
			f.spectral = SpectralRecord(SPECTRAL_SOURCE_FILE, FIT_BASIS_NATIVE);
			int version = LoadSensorCalibration(ar, f);
			int sentinel = 0;
			ar >> sentinel;
			ar.Close();
			mem.Detach();
			CPPUNIT_ASSERT_EQUAL( 4, version );
			CPPUNIT_ASSERT_EQUAL( methods[m], f.method );
			CPPUNIT_ASSERT_EQUAL( (int) FIT_BASIS_UNKNOWN, f.fitBasis );
			CPPUNIT_ASSERT( ! f.spectral.IsPresent() );
			CPPUNIT_ASSERT( f.spectral == SpectralCorrectionRecord() );
			CPPUNIT_ASSERT_EQUAL( 90 + m, sentinel );
		}

		// A version-1 block carries neither, even when the fields it was
		// stored from held both: same defaults, again into dirty scratch.
		StreamBuilder s;
		SensorCalibrationFields in;
		in.fitBasis = FIT_BASIS_NATIVE;
		in.spectral = SpectralRecord(SPECTRAL_SOURCE_FILE, FIT_BASIS_NATIVE);
		StoreSensorCalibration(s.Ar(), in, 1);
		std::vector<BYTE> v1 = s.Bytes();
		CMemFile mem(&v1[0], (UINT) v1.size());
		CArchive ar(&mem, CArchive::load);
		SensorCalibrationFields f = in;
		int version = LoadSensorCalibration(ar, f);
		ar.Close();
		mem.Detach();
		CPPUNIT_ASSERT_EQUAL( 1, version );
		CPPUNIT_ASSERT_EQUAL( (int) FIT_BASIS_UNKNOWN, f.fitBasis );
		CPPUNIT_ASSERT( ! f.spectral.IsPresent() );
	}

	void SensorUnknownFitBasisRefused()
	{
		// A basis this build does not know would compose (or not) the wrong
		// matrices: refused as a newer file, naming the feature.
		const int bad[3] = { -1, FIT_BASIS_DRIVER_CORRECTED + 1, 99 };
		for ( int i = 0; i < 3; i++ )
		{
			LoadResult r = Load(HandV4(CALIB_HCFR_DEFAULT, false, TRUE, false, 0, bad[i]), false);
			CPPUNIT_ASSERT_EQUAL( (int) NEWER, (int) r.outcome );
			CPPUNIT_ASSERT_EQUAL( 4, r.exVersion );
			CPPUNIT_ASSERT( r.exFeature == "fit basis" );
		}
		// Every known basis loads.
		for ( int b = FIT_BASIS_UNKNOWN; b <= FIT_BASIS_DRIVER_CORRECTED; b++ )
		{
			LoadResult r = Load(HandV4(CALIB_HCFR_DEFAULT, false, TRUE, false, 0, b), false);
			CPPUNIT_ASSERT_EQUAL( (int) LOADED, (int) r.outcome );
			CPPUNIT_ASSERT_EQUAL( b, r.fields.fitBasis );
		}
	}

	void SensorUnknownRecordValuesRefused()
	{
		// A written record always names a known source and a known basis.
		const int badSource[2] = { SPECTRAL_SOURCE_DISPLAY_TYPE + 1, -1 };
		for ( int i = 0; i < 2; i++ )
		{
			SpectralCorrectionRecord rec = SpectralRecord(SPECTRAL_SOURCE_FILE, FIT_BASIS_NATIVE);
			rec.source = badSource[i];
			LoadResult r = Load(HandV4(CALIB_HCFR_DEFAULT, false, TRUE, false, 0, FIT_BASIS_NATIVE, rec), false);
			CPPUNIT_ASSERT_EQUAL( (int) NEWER, (int) r.outcome );
			CPPUNIT_ASSERT_EQUAL( 4, r.exVersion );
			CPPUNIT_ASSERT( r.exFeature == "spectral correction source" );
		}
		const int badBasis[3] = { FIT_BASIS_UNKNOWN, FIT_BASIS_DRIVER_CORRECTED + 1, -1 };
		for ( int i = 0; i < 3; i++ )
		{
			LoadResult r = Load(HandV4(CALIB_HCFR_DEFAULT, false, TRUE, false, 0, FIT_BASIS_NATIVE,
			                           SpectralRecord(SPECTRAL_SOURCE_FILE, badBasis[i])), false);
			CPPUNIT_ASSERT_EQUAL( (int) NEWER, (int) r.outcome );
			CPPUNIT_ASSERT_EQUAL( 4, r.exVersion );
			CPPUNIT_ASSERT( r.exFeature == "spectral correction basis" );
		}
	}

	void SensorRecordWithSourceNoneIsCorrupt()
	{
		// The writer emits tag 5 only for a present record, so a tag-5 payload
		// naming source NONE was never written by any HCFR: corruption
		// (badSchema), not a file from a newer version.
		StreamBuilder s;
		s.Ar() << (int) 4;
		s.Ar() << (time_t) 1789000000;
		CExtensionWriter ext;
		ext.BeginTag(ArchiveTag::SENSOR_CALIBRATION_METHOD) << (int) CALIB_HCFR_DEFAULT;
		ext.EndTag();
		SpectralCorrectionRecord rec = SpectralRecord(SPECTRAL_SOURCE_FILE, FIT_BASIS_NATIVE);
		rec.source = SPECTRAL_SOURCE_NONE;
		rec.Serialize(ext.BeginTag(ArchiveTag::SENSOR_SPECTRAL_CORRECTION));
		ext.EndTag();
		ext.Write(s.Ar());
		LoadResult r = Load(s.Bytes(), false);
		CPPUNIT_ASSERT_EQUAL( (int) BAD_SCHEMA, (int) r.outcome );
	}

	void SensorSpectralMethodWithoutRecordRefused()
	{
		// The spectral method's matrix is the record's correction; without the
		// record nothing says what it corrects. With it, the block loads.
		LoadResult r = Load(HandV4(CALIB_SPECTRAL_SAMPLE, false, FALSE, false, 0, FIT_BASIS_NATIVE), false);
		CPPUNIT_ASSERT_EQUAL( (int) BAD_SCHEMA, (int) r.outcome );
		r = Load(HandV4(CALIB_SPECTRAL_SAMPLE, false, FALSE, false, 0, FIT_BASIS_NATIVE,
		                SpectralRecord(SPECTRAL_SOURCE_DISPLAY_TYPE, FIT_BASIS_NATIVE)), false);
		CPPUNIT_ASSERT_EQUAL( (int) LOADED, (int) r.outcome );
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION( THIS_TEST_CASE );
