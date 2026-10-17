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

// SpectralCorrection.h: a colorimeter's spectral-sample (ccss) correction as an
// ordinary HCFR correction method.
//
// A filter colorimeter reports one triple per reading, projected through its
// own sensitivity curves. The driver turns that triple into XYZ with a 3x3
// emission calibration matrix it fits from those curves: either from the
// curves alone (the OEM default, "native") or from a set of display spectra
// (a loaded ccss). Native and ccss-corrected readings are therefore two linear
// images of the same triple, and with both matrices known they convert into
// each other exactly:
//
//     XYZ_native = nativeCal * rgb      XYZ_corrected = currentCal * rgb
//     correction = currentCal * nativeCal^-1      (HCFR applies this to native)
//     toNative   = nativeCal  * currentCal^-1     (the wrapper applies this to
//                                                   the driver's output)
//
// So HCFR stores raw = native for every reading, keeps the correction beside
// the other methods (FCMM, Bodner), and switching between them is a recompute
// from raw. Nothing is applied inside the driver that HCFR does not own.
// Design note: hcfr-bench\release-4.3\ccss-as-correction-design.md.
//
// No MFC in the math; the record's Serialize is MFC-guarded because it is the
// extension-trailer payload (ArchiveTags.h, SENSOR_SPECTRAL_CORRECTION).

#ifndef SPECTRALCORRECTION_H_INCLUDED
#define SPECTRALCORRECTION_H_INCLUDED

#include "libHCFR_Config.h"
#include "Matrix.h"
#include <string>

// Which readings a correction's matrices were fitted on, and so expect as input.
// Persisted (SENSOR_FIT_BASIS tag): never renumber.
enum SpectralFitBasis
{
	FIT_BASIS_UNKNOWN          = 0,	// tag absent: a file from before this field. Treated as DRIVER_CORRECTED,
									// which is what every build before this one did.
	FIT_BASIS_NATIVE           = 1,	// fitted on native readings: the probe under its default matrix
	FIT_BASIS_DRIVER_CORRECTED = 2	// fitted on readings the driver had already corrected with a ccss
};

// Where a sensor's spectral correction came from. Persisted: never renumber.
enum SpectralCorrectionSource
{
	SPECTRAL_SOURCE_NONE         = 0,
	SPECTRAL_SOURCE_FILE         = 1,	// a .ccss or correlation .csv browsed in the sensor page
	SPECTRAL_SOURCE_DISPLAY_TYPE = 2	// a ccss or ccmx entry chosen in Argyll's Display Type list
};

// Relative tolerance for "these two 3x3 matrices are the same" and for the
// singularity test: |det| <= kSpectralMatrixTol * (largest |entry|)^3.
static const double kSpectralMatrixTol = 1e-9;

// Derives the HCFR-side correction and the wrapper-side undo from the driver's
// two emission calibration matrices (see the header comment). Returns false,
// leaves both outputs identity and says why in `why` when:
//   - either matrix is not 3x3, or holds a non-finite entry;
//   - either matrix is singular (tolerance above);
//   - requireDistinct and the two are equal within kSpectralMatrixTol - the
//     driver was asked for a spectral sample and its matrix did not change,
//     which is the "sample silently dropped" failure the design guards against.
// With requireDistinct false and equal inputs, succeeds with identity outputs.
bool DeriveSpectralCorrection(const Matrix& nativeCal, const Matrix& currentCal, bool requireDistinct,
                              Matrix& outCorrection, Matrix& outToNative, std::string& why);

// True when a and b are both 3x3 and equal within kSpectralMatrixTol relative
// to the largest |entry| of a.
bool SpectralMatricesEqual(const Matrix& a, const Matrix& b);

// FNV-1a, 64-bit (Fowler / Noll / Vo; offset 0xcbf29ce484222325, prime
// 0x100000001b3). Identifies the correction file that was loaded.
unsigned long long Fnv1a64(const void* data, size_t n);
// Hashes a whole file; false when it cannot be opened.
bool Fnv1a64File(const char* path, unsigned long long& outHash);

// Everything a document needs to say which spectral correction a sensor
// carries, for which unit, and how to re-derive it. Lives on CSensor so a
// correction file (.thc) carries it too.
struct SpectralCorrectionRecord
{
	int                source;		// SpectralCorrectionSource; NONE = no record
	std::string        description;	// the ccss display/tech text, or the Display Type label
	unsigned long long fileHash;	// Fnv1a64 of the file bytes; 0 for a Display Type entry
	int                observer;	// the Argyll observer (icxObserverType) in force when derived; -1 unknown
	std::string        meterSerial;	// the unit the matrices belong to; "" when the driver has none
	Matrix             nativeCal;	// 3x3, the probe's default emission calibration
	Matrix             currentCal;	// 3x3, the emission calibration under the sample (ccmx folded in)
	Matrix             correction;	// 3x3 = currentCal * nativeCal^-1, what HCFR applies to native
	int                basis;		// SpectralFitBasis of `correction`: NATIVE when the wrapper divides
									// the driver's correction back out; DRIVER_CORRECTED when it cannot
									// (a driver whose matrices are not exposed) - then correction is
									// identity and the driver keeps applying the sample itself.

	SpectralCorrectionRecord();		// NONE, "", 0, -1, "", three identities, UNKNOWN
	void Clear();
	bool IsPresent() const { return source != SPECTRAL_SOURCE_NONE; }

	// Field-wise; matrices compared exactly (a record is copied, never recomputed).
	bool operator==(const SpectralCorrectionRecord& o) const;
	bool operator!=(const SpectralCorrectionRecord& o) const { return !(*this == o); }

#ifdef LIBHCFR_HAS_MFC
	// The SENSOR_SPECTRAL_CORRECTION tag payload, both directions. Append-only:
	//   int source, CString description, DWORD hashLow, DWORD hashHigh,
	//   int observer, CString meterSerial, Matrix nativeCal, Matrix currentCal,
	//   Matrix correction, int basis.
	// A loading archive that ends early throws like any other short read.
	void Serialize(CArchive& ar);
#endif
};

#endif // SPECTRALCORRECTION_H_INCLUDED
