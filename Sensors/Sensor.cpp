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
	for ( int k = 0; k < 3; k++ )
	{
		m_bodnerRawMatrix[k]=IdentityMatrix(3);
		m_bodnerCalMatrix[k]=IdentityMatrix(3);
	}

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
	m_calibrationTime = p->m_calibrationTime;
	m_calibrationMethod = p->m_calibrationMethod;
	for ( int k = 0; k < 3; k++ )
	{
		m_bodnerRawMatrix[k] = p->m_bodnerRawMatrix[k];
		m_bodnerCalMatrix[k] = p->m_bodnerCalMatrix[k];
	}
	m_name = p->m_name;
}

void CSensor::Serialize(CArchive& archive)
{
	CObject::Serialize(archive);
	m_sensorToXYZMatrix.Serialize(archive);
	if (archive.IsStoring())
	{
		int version=2;
		archive << version;
		archive << m_calibrationTime;
		archive << m_calibrationMethod;
		if ( m_calibrationMethod == CALIB_BODNER_THREEMATRIX )
		{
			for ( int k = 0; k < 3; k++ )
			{
				m_bodnerRawMatrix[k].Serialize(archive);
				m_bodnerCalMatrix[k].Serialize(archive);
			}
		}
	}
	else
	{
		int version;
		archive >> version;
		if ( version > 2 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		archive >> m_calibrationTime;

		if ( version >= 2 )
		{
			archive >> m_calibrationMethod;
			if ( m_calibrationMethod == CALIB_BODNER_THREEMATRIX )
			{
				for ( int k = 0; k < 3; k++ )
				{
					m_bodnerRawMatrix[k].Serialize(archive);
					m_bodnerCalMatrix[k].Serialize(archive);
				}
			}
		}
		else
		{
			// Pre-existing sensor files predate the method field. They were
			// calibrated under the legacy "UseOnlyPrimaries" checkbox, so derive
			// the method from the same profile key the config migration uses -
			// hardcoding HCFR_DEFAULT made every old NIST-calibrated .chc trip
			// the method-sync prompt on export, inviting a wrong rewrite.
			// The legacy key is only trustworthy before the migration: once the
			// user picks a method in the new dropdown, SaveSettings rewrites
			// UseOnlyPrimaries from that selection (downgrade safety), so it no
			// longer describes what old files were calibrated with.
			m_calibrationMethod = ( GetConfig()->GetProfileInt("Advanced","CalibrationMethod",-1) == -1
								 &&  GetConfig()->GetProfileInt("Advanced","UseOnlyPrimaries",0) )
									? CALIB_CLASSIC_NIST : CALIB_HCFR_DEFAULT;
		}
	}
}

void CSensor::BeginConfigure()
{
	m_cfgSnapMatrix   = m_sensorToXYZMatrix;
	m_cfgSnapMethod   = m_calibrationMethod;
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
}

void CSensor::SetPropertiesSheetValues()
{
	m_SensorPropertiesPage.SetMatrix(m_sensorToXYZMatrix);
	m_SensorPropertiesPage.m_calibrationDate=GetCalibrationTime().Format("%#c");
}

void CSensor::GetPropertiesSheetValues()
{
	if(	m_sensorToXYZMatrix != m_SensorPropertiesPage.GetMatrix() )
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

	if ( m_calibrationMethod == CALIB_BODNER_THREEMATRIX )
		result.SetXYZValue(SelectAndApplyBodnerMatrix(result.GetXYZValue(), m_bodnerRawMatrix, m_bodnerCalMatrix));
	else
		result.applyAdjustmentMatrix(m_sensorToXYZMatrix);

	if ( bMeasureOk )
		GetColorApp()->PlayMeasureSound();

    return result;
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
