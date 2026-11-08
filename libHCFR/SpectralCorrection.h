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
#include <vector>

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

// The single matrix a sensor's readings go through for every method but Bodner
// (CSensor::GetEffectiveMatrix forwards here; T32 pins the whole table):
//   calibrationMethod == CALIB_BODNER_THREEMATRIX         -> identity (the caller
//                                                            branches on Bodner first)
//   calibrationMethod == CALIB_SPECTRAL_SAMPLE            -> sensorToXYZ, never composed:
//                                                            that matrix IS rec.correction
//   fitBasis != FIT_BASIS_NATIVE and rec.IsPresent()      -> sensorToXYZ * rec.correction
//                                                            (a matrix fitted on driver-
//                                                            corrected readings, as 4.1 did)
//   otherwise                                             -> sensorToXYZ
// fitBasis is the sensor's (SpectralFitBasis of the method's matrices), not
// rec.basis. Readings are column vectors, so the correction acts first.
Matrix EffectiveSensorMatrix(int calibrationMethod, int fitBasis, const SpectralCorrectionRecord& rec,
                             const Matrix& sensorToXYZ);

// Whether a matrix calibration (a manual, simultaneous or existing-reference fit,
// or a correction file) may be made while the sensor holds this record. False only
// for a present record the driver applies itself (basis DRIVER_CORRECTED): the
// meter then reports readings that already carry the spectral correction, so a
// matrix would be fitted on corrected readings and, once the record and its sample
// are cleared, applied to native ones. The calibration is refused instead (the
// user clears the spectral correction first). A NATIVE record allows it: raw is
// native and the record stays dormant under the matrix. T32 pins the table.
bool MatrixCalibrationAllowed(const SpectralCorrectionRecord& rec);

class RawlessReadingsState;		// Color.h

// Whether a sensor's stored readings went through a spectral correction the driver
// applied that no record describes: there is no record, and the method's matrices
// expect driver-corrected readings (fitBasis not NATIVE). That is a v1 sensor
// stream - HCFR 4.2.2 and earlier, where an i1d3 on a ccss/ccmx Display Type entry
// had the driver apply the entry - or a document from a build that loaded a browsed
// sample into the i1d3 driver and kept no record. Deriving that same correction now
// changes how those readings are expressed (HCFR applies it to native readings),
// not what they went through. T45 pins the table.
bool ReadingsCarryUnrecordedDriverCorrection(const SpectralCorrectionRecord& rec, int fitBasis);

// A change of the effective single matrix (EffectiveSensorMatrix) that a sensor's
// Init made by itself, outside the sensor sheet, where no walk of the stored
// readings follows. `before` and `after` are the effective matrix around the Init's
// correction step:
//   before == after (exactly)  -> nothing.
//   readingsAlreadyThroughIt   -> the raw-less readings already went through what
//     `after` applies (ReadingsCarryUnrecordedDriverCorrection before the change):
//     they embody the new matrix, and the state stands.
//   otherwise                  -> they went through `before`, which the sensor no
//     longer has: state.MatrixSetWithoutWalk(). They keep their values and every
//     later walk skips them, as after DDE SetSensorMatrix or Change Sensor.
// Inside the sensor sheet nothing is booked: the sheet's walk (OK) or its snapshot
// (Cancel) accounts for every change made there. The sheet runs the measurement-start
// Init before it takes its snapshot (CArgyllSensor::BeginConfigure), so the sheet
// sees only the user's changes, whichever came first. T45 pins both orders.
void EffectiveChangedWithoutWalk(RawlessReadingsState& state, const Matrix& before, const Matrix& after,
                                 bool readingsAlreadyThroughIt);

// The one rule for a FILE record and the correction file at a sensor's stored path:
// the path derives the record again (another unit or observer), or the driver
// re-applies the path's sample (a meter whose matrices are not exposed), only when
// the path's file is the file the record was made from. "The file" is its content:
// pathHash is the Fnv1a64 of the bytes read from the path when they are about to be
// used, never a cached value (pathHashed false: the file could not be read). False
// only for a FILE record with a known hash (non-zero) and a read file that hashes to
// another value - the record came from another file (a correction file or a document
// loaded after the Browse), or the file was rewritten after its Browse. The record
// then stands as far as anything applies it (SpectralRecordApplied), and the path,
// which no longer names the correction in use, is dropped. Every other case cannot
// be compared and is true. T45 pins the table.
bool SpectralRecordFromPathFile(const SpectralCorrectionRecord& rec, bool pathHashed, unsigned long long pathHash);

// The second invariant: a record never says a correction is in force when nothing
// applies one. True when something applies `rec` on this meter:
//   no record       -> true (nothing is claimed);
//   basis NATIVE    -> true: HCFR applies the record's matrices itself
//                      (EffectiveSensorMatrix), on any meter;
//   any other basis -> the record says the driver applies the sample. Only a driver
//                      whose matrices are not exposed holds one, and only from a
//                      stored path (Init's re-apply): true only for
//                      !matricesExposed && hasPath.
// The test is structural. A stored path whose file cannot be loaded this time is
// reported by the re-apply warning and tried again at the next Init; its record
// stays. CArgyllSensor::EnforceRecordApplied is the one place that acts on a false
// answer (it removes the record and says so), at the end of every Init's correction
// step: every record transition a meter makes happens inside that step, and a record
// that arrives by a load reaches it before the first reading. T45 pins the table.
bool SpectralRecordApplied(const SpectralCorrectionRecord& rec, bool matricesExposed, bool hasPath);

// Whether the sensor page's Clear may remove the spectral correction: whenever
// there is a record or a stored path, except a Display Type record on a meter
// whose list resyncs it (displayTypeListResyncs: the i1d3) with the record's own
// entry selected (recordEntrySelected). There choosing another Display Type is what
// removes it, and a Clear would be undone by the next Init, which derives the
// selected entry again - after the Clear's walk had taken the correction off the
// readings, so the readings and the sensor would no longer agree (a stored path
// does not change that: the selected entry takes precedence over it). Every other
// Display Type record has no other way out: a meter that never resyncs, no meter,
// an empty list, a record whose entry is not the one selected (the entry is gone
// and Init moved to a plain one, or the record came with a document onto a plain
// type) - there choosing the entry already shown is no change. T45 pins the table.
bool SpectralClearAllowed(const SpectralCorrectionRecord& rec, bool hasPath, bool displayTypeListResyncs,
                          bool recordEntrySelected);

// The plain Display Type entry (neither ccss nor ccmx) to measure on when an entry's
// own correction cannot be used: `stored` when it is in the list and plain, else the
// first plain entry; -1 when the list has none. corrected[i]: entry i carries a ccss
// or a ccmx. The i1d3 list starts with the driver's static (plain) table.
int PlainDisplayTypeEntry(int stored, const std::vector<bool>& corrected);

#endif // SPECTRALCORRECTION_H_INCLUDED
