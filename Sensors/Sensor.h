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

// Sensor.h: interface for the CSensor class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SENSOR_H__FD0761AA_CBEC_4A38_8A67_ADB0963FBAE4__INCLUDED_)
#define AFX_SENSOR_H__FD0761AA_CBEC_4A38_8A67_ADB0963FBAE4__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Color.h"
#include "SensorPropPage.h"

class CGenerator;

class CSensor: public CObject   
{

public:
	DECLARE_SERIAL(CSensor) ;

protected:
	BOOL m_isModified;
	CString m_errorString;
	BOOL m_isMeasureValid;
	Matrix m_sensorToXYZMatrix;
    Matrix m_sensorToXYZMatrixMod;
	int m_calibrationMethod;					// CalibrationMatrixMethod: which method produced the matrices below
	Matrix m_bodnerRawMatrix[3];				// Bodner sub-gamut raw-primary matrices (rgw/gbw/rbw), only valid when m_calibrationMethod == CALIB_BODNER_THREEMATRIX
	Matrix m_bodnerCalMatrix[3];				// Bodner sub-gamut calibration matrices (rgw/gbw/rbw)
	// Cached inverses of m_bodnerRawMatrix (maintained by
	// UpdateBodnerInverseCache): the sub-gamut selection needs them for
	// every reading, and Gauss-Jordan per reading is measurably slow.
	Matrix m_bodnerRawInverse[3];
	bool   m_bodnerRawInvertible[3];
	// Configure() snapshot (BeginConfigure/CancelConfigure)
	Matrix m_cfgSnapMatrix;
	Matrix m_cfgSnapBodnerRaw[3], m_cfgSnapBodnerCal[3];
	int  m_cfgSnapMethod;
	// The matrix SetPropertiesSheetValues put into m_SensorPropertiesPage. Kept so
	// GetPropertiesSheetValues can tell a user grid edit from a value the sheet was
	// merely shown - see the comment there.
	Matrix m_sheetMatrixShown;
	time_t m_calibrationTime;
	int		m_PropertySheetTitle;
	CSensorPropPage m_SensorPropertiesPage;
	CString m_name;

	CPropertyPageWithHelp * m_pDevicePage;
	CPropertyPageWithHelp * m_pCalibrationPage;

public:
	CSensor();
	virtual ~CSensor();
	virtual	void Copy(CSensor * p);

	virtual void Serialize(CArchive& archive); 

	virtual BOOL Init( BOOL bForSimultaneousMeasures );
	CColor MeasureColor(const ColorRGBDisplay& aRGBValue, int displaymode = 0);
	virtual CColor MeasureGray(double aIRELevel);
	virtual BOOL Release();

	//virtual BOOL CalibrateSensor(CGenerator *apGenerator);
	//virtual BOOL CalibrateSensor(Matrix & measures, Matrix & references, CColor & WhiteTest, CColor & WhiteRef, CColor & BlackTest, CColor & BlackRef);
	virtual void LoadCalibrationFile(CString & aFileName) { return ; }
	virtual void SaveCalibrationFile() { return ; }

	void SetSensorMatrix(Matrix aMatrix) { m_sensorToXYZMatrix=aMatrix; m_calibrationTime=time(NULL);}
	void SetSensorMatrixMod(Matrix aMatrix) { m_sensorToXYZMatrixMod=aMatrix; m_calibrationTime=time(NULL);}
	Matrix GetSensorMatrix() {return m_sensorToXYZMatrix; }
	Matrix GetSensorMatrixMod() {return m_sensorToXYZMatrixMod; }

	void SetCalibrationMethod(int aMethod) { m_calibrationMethod=aMethod; }
	int GetCalibrationMethod() const { return m_calibrationMethod; }
	void UpdateBodnerInverseCache();
	virtual bool CorrectionChangedSinceBeginConfigure() const;
	void SetBodnerMatrices(const Matrix aRawMatrix[3], const Matrix aCalMatrix[3])
	{
		for ( int k = 0; k < 3; k++ )
		{
			m_bodnerRawMatrix[k] = aRawMatrix[k];
			m_bodnerCalMatrix[k] = aCalMatrix[k];
		}
		UpdateBodnerInverseCache();
		m_calibrationTime=time(NULL);
	}
	void ClearBodnerMatrices()
	{
		for ( int k = 0; k < 3; k++ )
		{
			m_bodnerRawMatrix[k] = Matrix::IdentityMatrix(3);
			m_bodnerCalMatrix[k] = Matrix::IdentityMatrix(3);
		}
		UpdateBodnerInverseCache();
	}
	const Matrix * GetBodnerRawMatrices() const { return m_bodnerRawMatrix; }
	const Matrix * GetBodnerCalMatrices() const { return m_bodnerCalMatrix; }

	virtual BOOL IsMeasureValid() {return m_isMeasureValid; }
	virtual void SetMeasureValidity(BOOL isValid) { m_isMeasureValid=isValid; }
	virtual void SetErrorString(CString aString) { m_errorString=aString; }
	virtual CString GetErrorString() { return m_errorString; }

	virtual void SetPropertiesSheetValues();
	virtual void GetPropertiesSheetValues();
	virtual BOOL Configure();

	virtual BOOL IsModified() { return m_isModified; }
	virtual void SetModifiedFlag( BOOL bModified ) { m_isModified = bModified; }

	// Snapshot/restore of the correction state around Configure(). Needed because
	// the Argyll spectral Browse commits immediately (device sample + matrices +
	// modified flag) - a cancelled sheet must restore all of it. Derived classes
	// extend both (see CArgyllSensor).
	virtual void BeginConfigure();
	virtual void CancelConfigure();

	virtual LPCSTR GetStandardSubDir ()	{ return ""; }

	// Spectral (ccss/CSV) correction, applied inside the meter driver. Only
	// Argyll spectral-capable colorimeters override these; the base is a no-op.
	// Used to keep the spectral and HCFR-matrix correction regimes mutually
	// exclusive (avoid double-correction).
	virtual BOOL HasSpectralCorrection() const { return FALSE; }
	virtual void ClearSpectralCorrection() {}
	// Read-and-reset: TRUE when the last spectral-correction apply asked to LEAVE
	// existing measurements as-is (mixed) rather than strip them back to raw.
	virtual BOOL TakePendingSpectralLeaveMeasures() { return FALSE; }

	CTime GetCalibrationTime() { return CTime(m_calibrationTime); }
	int IsCalibrated()
	{
		if (m_sensorToXYZMatrix.IsIdentity () && m_sensorToXYZMatrixMod.IsIdentity ())
			return 0;
		else
			if (!m_sensorToXYZMatrix.IsIdentity ())
				return 1;
		return 2;
	}

	// True when a correction is actually being applied to every MeasureColor reading:
	// a live non-identity sensor matrix (RGB/FCMM), OR the Bodner sub-gamut matrices.
	// IsCalibrated() alone returns 0 for a Bodner sensor (its m_sensorToXYZMatrix is
	// identity - the correction lives in m_bodnerRawMatrix[]), so UI gates that ask
	// "is a correction live?" must use this to see the Bodner case. Mirrors the
	// bHasCorrection test in datasetdoc.cpp.
	bool IsCorrectionActive()
	{
		return ( IsCalibrated() == 1 ) || ( m_calibrationMethod == CALIB_BODNER_THREEMATRIX );
	}
	
	
	CString GetName() { return m_name; }
	void SetName(CString aStr) { m_name=aStr; } 

	// returns unique sensor identifier (for simultaneous measures: cannot use twice the same sensor on two documents)
	virtual void GetUniqueIdentifier( CString & strId ) { strId = m_name; }

    virtual bool isValid() const {return true;}
	virtual BOOL HasSpectrumCapabilities ( int * pNbBands, int * pMinWaveLength, int * pMaxWaveLength, double * pBandWidth ) { return FALSE; }
    virtual bool isColorimeter() const { return true; }
    virtual int ReadingType() const {return 0;}
    virtual bool supportsAvg() const { return false; }
    virtual void setAvgEnabled(bool) {}
    virtual bool getAvgEnabled() const { return false; }
private:
    virtual CColor MeasureColorInternal(const ColorRGBDisplay& aRGBValue, int displaymode = 0) { return noDataColor;};
};

#endif // !defined(AFX_SENSOR_H__FD0761AA_CBEC_4A38_8A67_ADB0963FBAE4__INCLUDED_)
