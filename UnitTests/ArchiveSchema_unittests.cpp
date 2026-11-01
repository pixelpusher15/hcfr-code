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

	// A version-4 block written by hand: the version, the time, then a
	// trailer holding only the tags asked for. kOmit leaves METHOD out,
	// rawless < 0 leaves RAWLESS out.
	const int kOmit = -100000;
	std::vector<BYTE> HandV4(int method, bool bMatrices, int rawless, bool bUnknownFirst, int sentinel)
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
		// A method this build does not know is a file from a newer HCFR,
		// reported with the block version, not the method value.
		const int bad[3] = { -1, CALIB_METHOD_COUNT, 99 };
		for ( int i = 0; i < 3; i++ )
		{
			LoadResult r = Load(HandV4(bad[i], false, TRUE, false, 0), false);
			CPPUNIT_ASSERT_EQUAL( (int) NEWER, (int) r.outcome );
			CPPUNIT_ASSERT_EQUAL( 4, r.exVersion );
		}
		// The last known method still loads.
		LoadResult r = Load(HandV4(CALIB_METHOD_COUNT - 1, CALIB_METHOD_COUNT - 1 == CALIB_BODNER_THREEMATRIX, TRUE, false, 0), false);
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
};

CPPUNIT_TEST_SUITE_REGISTRATION( THIS_TEST_CASE );
