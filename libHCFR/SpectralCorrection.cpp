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

// SpectralCorrection.cpp: see SpectralCorrection.h.

#ifdef LIBHCFR_HAS_MFC
#include "stdafx.h"
#endif

#include "SpectralCorrection.h"
#include <cmath>
#include <cstdio>
#include <float.h>

static bool Is3x3(const Matrix& m)
{
	return m.GetRows() == 3 && m.GetColumns() == 3;
}

static bool AllFinite(const Matrix& m)
{
	for ( int i = 0; i < 3; i++ )
		for ( int j = 0; j < 3; j++ )
			if ( !_finite(m(i, j)) )
				return false;
	return true;
}

static double MaxAbsEntry(const Matrix& m)
{
	double mx = 0.0;
	for ( int i = 0; i < 3; i++ )
		for ( int j = 0; j < 3; j++ )
			if ( fabs(m(i, j)) > mx )
				mx = fabs(m(i, j));
	return mx;
}

bool SpectralMatricesEqual(const Matrix& a, const Matrix& b)
{
	if ( !Is3x3(a) || !Is3x3(b) )
		return false;
	double scale = MaxAbsEntry(a);
	double tol = kSpectralMatrixTol * ( scale > 0.0 ? scale : 1.0 );
	for ( int i = 0; i < 3; i++ )
		for ( int j = 0; j < 3; j++ )
			if ( fabs(a(i, j) - b(i, j)) > tol )
				return false;
	return true;
}

static bool IsSingular(const Matrix& m)
{
	double scale = MaxAbsEntry(m);
	if ( scale <= 0.0 )
		return true;
	return fabs(m.Determinant()) <= kSpectralMatrixTol * scale * scale * scale;
}

bool DeriveSpectralCorrection(const Matrix& nativeCal, const Matrix& currentCal, bool requireDistinct,
                              Matrix& outCorrection, Matrix& outToNative, std::string& why)
{
	outCorrection = Matrix::IdentityMatrix(3);
	outToNative   = Matrix::IdentityMatrix(3);
	why.clear();

	if ( !Is3x3(nativeCal) || !Is3x3(currentCal) )
	{
		why = "calibration matrices must be 3x3";
		return false;
	}
	if ( !AllFinite(nativeCal) || !AllFinite(currentCal) )
	{
		why = "calibration matrix holds a non-finite entry";
		return false;
	}
	if ( IsSingular(nativeCal) )
	{
		why = "the native calibration matrix is singular";
		return false;
	}
	if ( IsSingular(currentCal) )
	{
		why = "the current calibration matrix is singular";
		return false;
	}
	if ( SpectralMatricesEqual(nativeCal, currentCal) )
	{
		if ( requireDistinct )
		{
			why = "the spectral sample did not change the meter's calibration matrix";
			return false;
		}
		return true;	// identity outputs
	}

	outCorrection = currentCal * nativeCal.GetInverse();
	outToNative   = nativeCal * currentCal.GetInverse();
	return true;
}

unsigned long long Fnv1a64(const void* data, size_t n)
{
	const unsigned char* p = (const unsigned char*)data;
	unsigned long long h = 0xcbf29ce484222325ULL;
	for ( size_t i = 0; i < n; i++ )
	{
		h ^= (unsigned long long)p[i];
		h *= 0x100000001b3ULL;
	}
	return h;
}

bool Fnv1a64File(const char* path, unsigned long long& outHash)
{
	outHash = 0;
	FILE* f = fopen(path, "rb");
	if ( f == NULL )
		return false;
	unsigned long long h = 0xcbf29ce484222325ULL;
	unsigned char buf[65536];
	size_t n;
	while ( ( n = fread(buf, 1, sizeof(buf), f) ) > 0 )
	{
		for ( size_t i = 0; i < n; i++ )
		{
			h ^= (unsigned long long)buf[i];
			h *= 0x100000001b3ULL;
		}
	}
	fclose(f);
	outHash = h;
	return true;
}

SpectralCorrectionRecord::SpectralCorrectionRecord()
{
	Clear();
}

void SpectralCorrectionRecord::Clear()
{
	source      = SPECTRAL_SOURCE_NONE;
	description.clear();
	fileHash    = 0;
	observer    = -1;
	meterSerial.clear();
	nativeCal   = Matrix::IdentityMatrix(3);
	currentCal  = Matrix::IdentityMatrix(3);
	correction  = Matrix::IdentityMatrix(3);
	basis       = FIT_BASIS_UNKNOWN;
}

bool SpectralCorrectionRecord::operator==(const SpectralCorrectionRecord& o) const
{
	return source      == o.source
	    && description == o.description
	    && fileHash    == o.fileHash
	    && observer    == o.observer
	    && meterSerial == o.meterSerial
	    && nativeCal   == o.nativeCal
	    && currentCal  == o.currentCal
	    && correction  == o.correction
	    && basis       == o.basis;
}

#ifdef LIBHCFR_HAS_MFC
void SpectralCorrectionRecord::Serialize(CArchive& ar)
{
	if ( ar.IsStoring() )
	{
		ar << (int) source;
		ar << CString(description.c_str());
		ar << (DWORD)( fileHash & 0xFFFFFFFFULL );
		ar << (DWORD)( fileHash >> 32 );
		ar << (int) observer;
		ar << CString(meterSerial.c_str());
		nativeCal.Serialize(ar);
		currentCal.Serialize(ar);
		correction.Serialize(ar);
		ar << (int) basis;
	}
	else
	{
		CString s;
		DWORD lo, hi;
		ar >> source;
		ar >> s; description = (LPCSTR) s;
		ar >> lo;
		ar >> hi;
		fileHash = ( (unsigned long long) hi << 32 ) | (unsigned long long) lo;
		ar >> observer;
		ar >> s; meterSerial = (LPCSTR) s;
		nativeCal.Serialize(ar);
		currentCal.Serialize(ar);
		correction.Serialize(ar);
		ar >> basis;
	}
}
#endif
