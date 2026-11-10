/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2011 Association Homecinema Francophone.  All rights reserved.
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
//	François-Xavier CHABOUD
/////////////////////////////////////////////////////////////////////////////

// Sensor.cpp: implementation of the CSensor class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "Sensor.h"
#include "Generator.h"
#include "SensorCalibrationStream.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

IMPLEMENT_SERIAL(CSensor, CObject, 1) ;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSensor::CSensor()
{
	m_isModified=FALSE;
	m_isMeasureValid=TRUE;
	m_sensorToXYZMatrix=IdentityMatrix(3);
	m_sensorToXYZMatrixMod=IdentityMatrix(3);
	m_calibrationMethod=CALIB_HCFR_DEFAULT;
	m_fitBasis=FIT_BASIS_NATIVE;		// a fresh sensor has no legacy to honor; m_spectral starts empty
	m_inSensorSheet=false;
	for ( int k = 0; k < 3; k++ )
	{
		m_bodnerRawMatrix[k]=IdentityMatrix(3);
		m_bodnerCalMatrix[k]=IdentityMatrix(3);
	}
	UpdateBodnerInverseCache();

	m_calibrationTime=0;

	m_PropertySheetTitle = IDS_SENSOR_PROPERTIES_TITLE;

	m_pDevicePage = NULL;
	m_pCalibrationPage = NULL;

	SetName("Not defined");  // Needs to be set for real sensors
}

CSensor::~CSensor()
{

}

void CSensor::Copy(CSensor * p)
{
	m_errorString = p->m_errorString;
	m_isMeasureValid = p->m_isMeasureValid;
	m_sensorToXYZMatrix = p->m_sensorToXYZMatrix;
	m_sensorToXYZMatrixMod = p->m_sensorToXYZMatrixMod;
	m_calibrationTime = p->m_calibrationTime;
	m_calibrationMethod = p->m_calibrationMethod;
	m_spectral = p->m_spectral;
	m_fitBasis = p->m_fitBasis;
	for ( int k = 0; k < 3; k++ )
	{
		m_bodnerRawMatrix[k] = p->m_bodnerRawMatrix[k];
		m_bodnerCalMatrix[k] = p->m_bodnerCalMatrix[k];
	}
	UpdateBodnerInverseCache();
	m_rawlessReadings = p->m_rawlessReadings;
	m_name = p->m_name;
}

void CSensor::UpdateBodnerInverseCache()
{
	for ( int k = 0; k < 3; k++ )
	{
		m_bodnerRawInvertible[k] = ( m_bodnerRawMatrix[k].Determinant() != 0.0 );
		m_bodnerRawInverse[k] = m_bodnerRawInvertible[k]
			? m_bodnerRawMatrix[k].GetInverse() : IdentityMatrix(3);
	}
}

void CSensor::Serialize(CArchive& archive)
{
	CObject::Serialize(archive);
	m_sensorToXYZMatrix.Serialize(archive);
	if (archive.IsStoring())
	{
		// Honor the Debug\SaveOldCalibrationFile downgrade switch (see
		// COneDeviceSensor::Serialize): a v1 stream omits the method field and
		// loads in pre-method builds; the v1 loader's frozen-key derivation
		// recovers the method on re-import. Only the DEFAULT method is written
		// as v1: any other method (Bodner, three-color, FCMM-no-lum) would
		// silently change identity through the v1 round-trip. Document saves
		// under the key also produce v1 sensor blocks - harmless for the
		// default method, and the document header gates old builds anyway.
		// A v1 stream cannot say that the raw-less readings are stranded either
		// (it would reload as "they embody the matrix" and the next matrix
		// recalibration would compose onto them), so that state also forces
		// the current version.
		//
		// A v1 stream carries no fit basis and no spectral correction record
		// either, and reloads as basis UNKNOWN with no record. So v1 is also
		// written only when that reload is exact: no record, and a basis that is
		// not NATIVE already (UNKNOWN and DRIVER_CORRECTED behave alike). A NATIVE
		// fit reloaded as UNKNOWN would compose the next record's correction onto
		// a matrix fitted on native readings - a double correction - and a
		// dropped record loses what the matrix may be composed on.
		//
		// The layout of every version lives in libHCFR's
		// SensorCalibrationStream, where the unit tests can reach it.
		int version = ( m_calibrationMethod == CALIB_HCFR_DEFAULT
		             && ! m_spectral.IsPresent()
		             && m_fitBasis != FIT_BASIS_NATIVE
		             && m_rawlessReadings.ComposeOntoRawless()
		             && GetConfig () -> GetProfileInt ( "Debug", "SaveOldCalibrationFile", FALSE ) )
						? 1 : kSensorStreamVersion;

		SensorCalibrationFields fields;
		fields.calibrationTime = m_calibrationTime;
		fields.method = m_calibrationMethod;
		fields.rawlessEmbody = m_rawlessReadings.Get();
		fields.fitBasis = m_fitBasis;
		fields.spectral = m_spectral;
		for ( int k = 0; k < 3; k++ )
		{
			fields.bodnerRaw[k] = m_bodnerRawMatrix[k];
			fields.bodnerCal[k] = m_bodnerCalMatrix[k];
		}
		StoreSensorCalibration ( archive, fields, version );
	}
	else
	{
		SensorCalibrationFields fields;
		int version = LoadSensorCalibration ( archive, fields );

		m_calibrationTime = fields.calibrationTime;
		// Absent tags, and every v1 stream, leave the codec's defaults: basis
		// UNKNOWN (treated as driver-corrected) and no spectral correction record.
		m_fitBasis = fields.fitBasis;
		m_spectral = fields.spectral;
		if ( version != 1 )
		{
			m_calibrationMethod = fields.method;
			m_rawlessReadings.Set( fields.rawlessEmbody );
			if ( m_calibrationMethod == CALIB_BODNER_THREEMATRIX )
			{
				for ( int k = 0; k < 3; k++ )
				{
					m_bodnerRawMatrix[k] = fields.bodnerRaw[k];
					m_bodnerCalMatrix[k] = fields.bodnerCal[k];
				}
			}
		}
		else
		{
			// Pre-existing sensor files predate the method field. The legacy HCFR
			// correction is NIST four color / FCMM with luminance scaling
			// (CALIB_HCFR_DEFAULT): ComputeConversionMatrix's primary path, and
			// the fresh-document default. The three-color RGB / ASTM E1455-92
			// path (CALIB_CLASSIC_NIST - the enum name is a holdover) was only a
			// FALLBACK: taken automatically when no valid white was measured, or
			// forced by the "Do not use white in matrix calculus (R1.x)" checkbox
			// (the UseOnlyPrimaries key). So derive the method from that key: set
			// -> three-color, clear -> FCMM with luminance scaling. (The no-white auto-fallback cohort
			// is undetectable from any key; those rare files load as FCMM
			// with luminance scaling.)
			// The live legacy key is rewritten by SaveSettings on every save
			// (downgrade safety), so consult the copy frozen at migration time
			// (ColorHCFRConfig.cpp) instead. A profile migrated before the
			// frozen key existed has lost the information; those files load as
			// FCMM with luminance scaling, matching prior behavior.
			int legacyOnly = GetConfig()->GetProfileInt("Advanced","LegacyUseOnlyPrimaries",-1);
			if ( legacyOnly == -1 )
				legacyOnly = ( GetConfig()->GetProfileInt("Advanced","CalibrationMethod",-1) == -1 )
					? GetConfig()->GetProfileInt("Advanced","UseOnlyPrimaries",0) : 0;
			m_calibrationMethod = legacyOnly ? CALIB_CLASSIC_NIST : CALIB_HCFR_DEFAULT;
			// Written by a single-matrix build: every reading embodies the stored matrix.
			m_rawlessReadings.Set( true );
		}
		// Loaded (Bodner) or defaulted matrices either way: refresh the
		// sub-gamut inverse cache for this deserialized state.
		UpdateBodnerInverseCache();
	}
}

void CSensor::BeginConfigure()
{
	m_cfgSnapMatrix   = m_sensorToXYZMatrix;
	m_cfgSnapMethod   = m_calibrationMethod;
	m_cfgSnapSpectral = m_spectral;
	m_cfgSnapFitBasis = m_fitBasis;
	for ( int k = 0; k < 3; k++ )
	{
		m_cfgSnapBodnerRaw[k] = m_bodnerRawMatrix[k];
		m_cfgSnapBodnerCal[k] = m_bodnerCalMatrix[k];
	}
}

// Restores everything BeginConfigure snapshotted. Used when Configure()'s
// property sheet is cancelled: the spectral Browse commits its changes
// immediately, so cancelling the sheet has to put the previous correction
// back rather than trusting nothing happened.
void CSensor::CancelConfigure()
{
	m_sensorToXYZMatrix = m_cfgSnapMatrix;
	m_calibrationMethod = m_cfgSnapMethod;
	m_spectral          = m_cfgSnapSpectral;
	m_fitBasis          = m_cfgSnapFitBasis;
	// m_isModified is deliberately NOT restored: the Argyll page's Calibrate
	// button commits device settings (reading/display type, adapt, ...) and
	// re-inits the meter mid-dialog, and those legitimately survive a Cancel.
	// Forcing the flag back would hide them. With the correction state
	// restored above, the caller's IsModified recompute degenerates to a
	// harmless re-apply of the unchanged correction.

	for ( int k = 0; k < 3; k++ )
	{
		m_bodnerRawMatrix[k] = m_cfgSnapBodnerRaw[k];
		m_bodnerCalMatrix[k] = m_cfgSnapBodnerCal[k];
	}
	UpdateBodnerInverseCache();
}

// Whether the correction state differs from the BeginConfigure snapshot -
// i.e. whether this sheet session actually changed the correction (as
// opposed to device settings, which also set the modified flag).
bool CSensor::CorrectionChangedSinceBeginConfigure() const
{
	if ( m_calibrationMethod != m_cfgSnapMethod )
		return true;
	if ( m_sensorToXYZMatrix != m_cfgSnapMatrix )
		return true;
	if ( m_fitBasis != m_cfgSnapFitBasis || m_spectral != m_cfgSnapSpectral )
		return true;
	for ( int k = 0; k < 3; k++ )
		if ( m_bodnerRawMatrix[k] != m_cfgSnapBodnerRaw[k]
		  || m_bodnerCalMatrix[k] != m_cfgSnapBodnerCal[k] )
			return true;
	return false;
}

void CSensor::SetPropertiesSheetValues()
{
	m_SensorPropertiesPage.SetMatrix(m_sensorToXYZMatrix);
	m_SensorPropertiesPage.m_bMatrixEditable = ( m_calibrationMethod != CALIB_BODNER_THREEMATRIX );
	m_sheetMatrixShown = m_sensorToXYZMatrix;
	m_SensorPropertiesPage.m_calibrationDate=GetCalibrationTime().Format("%#c");
}

void CSensor::GetPropertiesSheetValues()
{
	// Compare against what the page was SHOWN, not against the live matrix. Code can
	// change m_sensorToXYZMatrix while the sheet is open - loading a spectral .ccss on
	// the Argyll page replaces it with the spectral correction - and
	// m_SensorPropertiesPage still holds the value we put there in
	// SetPropertiesSheetValues. Testing the live matrix makes that stale page value look
	// like an edit, so OK silently restored the matrix the .ccss had just retired and
	// every later reading was corrected twice, in the meter and again here.
	//
	// Only a real grid edit moves the page away from m_sheetMatrixShown, and that still
	// wins - a matrix typed in by hand while a spectral correction is loaded is the user
	// asking for it, and ConfirmMatrixCalOverSpectral covers the calibration paths.
	if(	m_SensorPropertiesPage.GetMatrix() != m_sheetMatrixShown )
	{
		m_sensorToXYZMatrix=m_SensorPropertiesPage.GetMatrix();
		SetModifiedFlag(TRUE);
	}
}

BOOL CSensor::Init( BOOL bForSimultaneousMeasures )
{
	return TRUE;
}

BOOL CSensor::Release()
{
	
	return TRUE;
}

CColor CSensor::MeasureColor(const ColorRGBDisplay& aRGBValue, int displaymode)
{
	// The wire-setup stamp (WireSetup.h): what the generator recorded putting on
	// the wire, taken before the read when that record is for exactly this
	// stimulus, and kept only if nothing else reached the screen until the read
	// ends. A read with no such record - a placeholder stimulus, a manual or
	// external generator - is stamped realDrive = false with every field unknown.
	const double wireStim[3] = { aRGBValue[0], aRGBValue[1], aRGBValue[2] };
	unsigned long wireSerial = 0;
	CColorWireSetup wire = WireDisplayRecord::StampFor(wireStim, &wireSerial);

	CColor result;
	if (this->GetName() == "Simulated sensor")
	    result = MeasureColorInternal(aRGBValue, displaymode);
	else
		result = MeasureColorInternal(aRGBValue);
	
	// A failed read comes back as noDataColor, which the clamps below would turn
	// into a valid-looking color, so decide success before they run.
	BOOL bMeasureOk = ( IsMeasureValid() && result.isValid() );
	// A failed read stays noDataColor, as it did when AsyncMeasurer's catch returned
	// it (4.2.2): the sweeps test IsMeasureValid, and a slot that keeps the reading
	// on Abort or Ignore then holds an invalid placeholder the grid and charts
	// recognize, not a valid-looking black of 1e-8.
	if ( ! bMeasureOk )
		return noDataColor;

	result.SetX(max(result.GetX(),0.00000001));
	result.SetY(max(result.GetY(),0.00000001));
	result.SetZ(max(result.GetZ(),0.00000001));

	result.SetRawXYZValue(result.GetXYZValue());
	// Capture the drive stimulus that produced this reading, alongside the raw
	// XYZ, so an export or a later recompute can report the exact signal sent to
	// the display instead of reconstructing it from the pattern generator.
	result.SetStimulusValue(aRGBValue);
	// And the wall-clock time of the reading, in seconds since the Unix epoch with
	// a sub-second fraction. Time, not a sweep index: drift anchors and the
	// step-response fit have to be plotted against the clock, and an index says
	// nothing about the pauses between readings. FILETIME counts 100 ns ticks
	// since 1601-01-01; 116444736000000000 of them precede 1970-01-01.
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	ULONGLONG t = ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
	result.SetTimestamp((double)(t - 116444736000000000ULL) / 1e7);
	if ( ! WireDisplayRecord::StillCurrent(wireSerial) )
		wire = CColorWireSetup();
	result.SetWireSetup(wire);

	if ( m_calibrationMethod == CALIB_BODNER_THREEMATRIX )
		result.SetXYZValue(SelectAndApplyBodnerMatrixInv(result.GetXYZValue(), m_bodnerRawInverse, m_bodnerRawInvertible, m_bodnerCalMatrix));
	else
		result.applyAdjustmentMatrix(GetEffectiveMatrix());

	if ( bMeasureOk )
		GetColorApp()->PlayMeasureSound();

    return result;
}

// Why the document's recompute baselines (OnConfigureSensor's SensorMatrixMod and
// full matrix, the oldMatrix of ComputeAdjustmentMatrix and of the manual fit,
// OnLoadCalibrationFile's re-apply) use this rather than GetSensorMatrix(): every
// stored reading is GetEffectiveMatrix() * raw. When a matrix fitted before the
// basis field is composed on a spectral correction, GetSensorMatrix() alone is not
// what the readings went through, and a delta or a recompute built on it would
// drop the spectral correction from some readings and keep it on others.
// The dispatch itself lives in libHCFR (EffectiveSensorMatrix), where T32 pins it.
Matrix CSensor::GetEffectiveMatrix() const
{
	return EffectiveSensorMatrix ( m_calibrationMethod, m_fitBasis, m_spectral, m_sensorToXYZMatrix );
}

void CSensor::EffectiveChangedWithoutWalk(const Matrix& before, bool readingsAlreadyThroughIt)
{
	::EffectiveChangedWithoutWalk ( m_rawlessReadings, before, GetEffectiveMatrix(), readingsAlreadyThroughIt );
}

void CSensor::ClearSpectralCorrection()
{
	m_spectral.Clear();
	if ( UsesSpectralMethod() )
	{
		// The method was the spectral correction itself: nothing is left to apply.
		SetSensorMatrix(Matrix::IdentityMatrix(3));
		SetCalibrationMethod(CALIB_HCFR_DEFAULT);
		m_fitBasis = FIT_BASIS_NATIVE;
	}
	SetModifiedFlag(TRUE);
}

void CSensor::SpectralSupersededByMatrixFit()
{
	// Nothing to do without a record, or when what was just installed is itself
	// the spectral method (a correction file that carries one).
	if ( ! m_spectral.IsPresent() || UsesSpectralMethod() )
		return;
	// A record whose correction the driver applies (its matrices are not exposed)
	// would stack under the new matrix: clear it, the sample with it. A native
	// record stays, dormant: raw is native and the matrix was fitted on raw.
	if ( m_spectral.basis != FIT_BASIS_NATIVE )
		ClearSpectralCorrection();
}

CColor CSensor::MeasureGray(double aLevel)
{
	// by default use pure virtual DisplayRGBColor function
	return MeasureColor(ColorRGBDisplay(aLevel)); 
}

BOOL CSensor::Configure()
{
	CString	str;
	CPropertySheetWithHelp propertySheet;

	str.LoadString(m_PropertySheetTitle);
	propertySheet.SetTitle(str);
	propertySheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;

	if ( m_pDevicePage )
		propertySheet.AddPage ( m_pDevicePage );

	propertySheet.AddPage ( & m_SensorPropertiesPage );
	propertySheet.SetActivePage(0);
	SetPropertiesSheetValues();
	// Every Init from here to the end of the OK (the page's Calibrate button, a
	// setting committed by GetPropertiesSheetValues) is inside the sheet.
	m_inSensorSheet = true;
	int result=propertySheet.DoModal();
	if(result == IDOK)
		GetPropertiesSheetValues();
	m_inSensorSheet = false;

	return result==IDOK;
}
