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
#include "ArchiveExtension.h"
#include "ArchiveTags.h"

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
		//
		// v3 (issue #193) is the v1 payload followed by a tagged extension
		// trailer (ArchiveTags.h): the method as SENSOR_CALIBRATION_METHOD, and
		// the six Bodner sub-gamut matrices as SENSOR_BODNER_MATRICES only when
		// that method is in force, the fit basis as SENSOR_FIT_BASIS, and the
		// spectral correction record as SENSOR_SPECTRAL_CORRECTION only when one
		// is present. v2 was the unreleased Bodner layout and is retired, never
		// written again. The v1 stream carries none of these: it is written only
		// for the DEFAULT method, and the record is dropped there by design.
		int version = ( m_calibrationMethod == CALIB_HCFR_DEFAULT
		             && GetConfig () -> GetProfileInt ( "Debug", "SaveOldCalibrationFile", FALSE ) )
						? 1 : 3;
		archive << version;
		archive << m_calibrationTime;
		if ( version == 3 )
		{
			CExtensionWriter ext;

			ext.BeginTag ( ArchiveTag::SENSOR_CALIBRATION_METHOD ) << (int) m_calibrationMethod;
			ext.EndTag ();

			if ( m_calibrationMethod == CALIB_BODNER_THREEMATRIX )
			{
				CArchive & value = ext.BeginTag ( ArchiveTag::SENSOR_BODNER_MATRICES );
				int k;
				for ( k = 0; k < 3; k++ )
					m_bodnerRawMatrix[k].Serialize(value);
				for ( k = 0; k < 3; k++ )
					m_bodnerCalMatrix[k].Serialize(value);
				ext.EndTag ();
			}

			ext.BeginTag ( ArchiveTag::SENSOR_FIT_BASIS ) << (int) m_fitBasis;
			ext.EndTag ();

			if ( m_spectral.IsPresent() )
			{
				m_spectral.Serialize ( ext.BeginTag ( ArchiveTag::SENSOR_SPECTRAL_CORRECTION ) );
				ext.EndTag ();
			}

			ext.Write ( archive );
		}
	}
	else
	{
		int version;
		archive >> version;
		if ( version > 3 )
			throw new CNewerSchemaException ( "CSensor", version );
		if ( version == 2 )
		{
			// The unreleased Bodner layout. Parsing it as v3 would read its
			// method field as the trailer length and desync unpredictably, so
			// it is refused outright: regenerate the file with this build.
			throw new CNewerSchemaException ( "CSensor", version, true );
		}
		if ( version < 1 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		archive >> m_calibrationTime;

		if ( version == 3 )
		{
			CExtensionReader ext(archive);
			BYTE  tag;
			DWORD cb;

			bool bSawBodnerMatrices = false;
			m_calibrationMethod = CALIB_HCFR_DEFAULT;		// when the tag is absent
			m_fitBasis = FIT_BASIS_UNKNOWN;					// a file from before the basis field
			m_spectral.Clear();								// no spectral correction recorded
			while ( ext.Next(tag, cb) )
			{
				switch ( tag )
				{
					case ArchiveTag::SENSOR_CALIBRATION_METHOD:
					{
						// Unknown tags are forward-compatible; unknown values inside a
						// known tag are not. A method this build does not know would
						// silently run through the wrong correction, so it is refused
						// as a file from a newer HCFR.
						int method;
						ext.Value() >> method;
						if ( method < CALIB_CLASSIC_NIST || method > CALIB_SPECTRAL_SAMPLE )
							throw new CNewerSchemaException ( "CSensor", method, false );
						m_calibrationMethod = method;
						break;
					}

					case ArchiveTag::SENSOR_BODNER_MATRICES:
					{
						CArchive & value = ext.Value();
						bSawBodnerMatrices = true;
						int k;
						for ( k = 0; k < 3; k++ )
							m_bodnerRawMatrix[k].Serialize(value);
						for ( k = 0; k < 3; k++ )
							m_bodnerCalMatrix[k].Serialize(value);
						break;
					}

					case ArchiveTag::SENSOR_FIT_BASIS:
					{
						// Same rule as the method: a basis this build does not know
						// would compose (or not) the wrong matrices.
						int basis;
						ext.Value() >> basis;
						if ( basis < FIT_BASIS_UNKNOWN || basis > FIT_BASIS_DRIVER_CORRECTED )
							throw new CNewerSchemaException ( "CSensor", basis, false );
						m_fitBasis = basis;
						break;
					}

					case ArchiveTag::SENSOR_SPECTRAL_CORRECTION:
					{
						// A written record always names its source and its basis; a
						// value outside the known ones comes from a newer HCFR.
						m_spectral.Serialize ( ext.Value() );
						if ( m_spectral.source < SPECTRAL_SOURCE_FILE || m_spectral.source > SPECTRAL_SOURCE_DISPLAY_TYPE )
							throw new CNewerSchemaException ( "CSensor", m_spectral.source, false );
						if ( m_spectral.basis < FIT_BASIS_NATIVE || m_spectral.basis > FIT_BASIS_DRIVER_CORRECTED )
							throw new CNewerSchemaException ( "CSensor", m_spectral.basis, false );
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
			if ( m_calibrationMethod == CALIB_BODNER_THREEMATRIX && ! bSawBodnerMatrices )
				AfxThrowArchiveException ( CArchiveException::badSchema );
			// Same reason for the spectral method: its matrix is the record's
			// correction, and without the record nothing says what it corrects.
			if ( UsesSpectralMethod() && ! m_spectral.IsPresent() )
				AfxThrowArchiveException ( CArchiveException::badSchema );
		}
		else
		{
			// No trailer: no basis (UNKNOWN, treated as driver-corrected) and no
			// spectral correction record.
			m_fitBasis = FIT_BASIS_UNKNOWN;
			m_spectral.Clear();

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
		}
		// Loaded (v3 Bodner) or defaulted matrices either way: refresh the
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
	CColor result;
	if (this->GetName() == "Simulated sensor")
	    result = MeasureColorInternal(aRGBValue, displaymode);
	else
		result = MeasureColorInternal(aRGBValue);
	
	// A failed read comes back as noDataColor, which the clamps below would turn
	// into a valid-looking color, so decide success before they run.
	BOOL bMeasureOk = ( IsMeasureValid() && result.isValid() );

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
Matrix CSensor::GetEffectiveMatrix() const
{
	if ( m_calibrationMethod == CALIB_BODNER_THREEMATRIX )
		return Matrix::IdentityMatrix(3);
	// The spectral method's matrix IS the record's correction: never compose it twice.
	if ( ! UsesSpectralMethod() && m_fitBasis != FIT_BASIS_NATIVE && m_spectral.IsPresent() )
		return m_sensorToXYZMatrix * m_spectral.correction;
	return m_sensorToXYZMatrix;
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
	int result=propertySheet.DoModal();
	if(result == IDOK)
		GetPropertiesSheetValues();

	return result==IDOK;
}
