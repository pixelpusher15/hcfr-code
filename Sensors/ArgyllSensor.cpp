/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2008 Association Homecinema Francophone.  All rights reserved.
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
//      Georges GALLERAND
//      John Adcock
//      Ian C
/////////////////////////////////////////////////////////////////////////////

// ArgyllSensor.cpp: implementation of the Argyll class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "ArgyllSensor.h"
#include "SpectralSampleFiles.h"
#include "SpectralSample.h"
#include "../Generators/GDIGenerator.h"
#include "../MainFrm.h"
#include <math.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

bool CArgyllSensor::m_debugMode = false;
char *m_logFile; 

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IMPLEMENT_SERIAL(CArgyllSensor, COneDeviceSensor, 1) ;

CArgyllSensor::CArgyllSensor() :
    m_DisplayType(0),
    m_ReadingType(0),
    m_SpectralType("Default"),
    m_intTime(1),
    m_meter(0),
    m_HiRes(0),
    m_Adapt(0),
    m_DisableAIO(0)
{
    m_spectralApplyLeaveMeasures = FALSE;
    m_ArgyllSensorPropertiesPage.m_pSensor = this;

    m_pDevicePage = & m_ArgyllSensorPropertiesPage;  // Add Argyll settings page to property sheet

    m_PropertySheetTitle = IDS_ARGYLLSENSOR_PROPERTIES_TITLE;

    SetName("Argyll Meter");
	m_logFile = GetConfig () -> m_logFileName;
    // Retrieve the list of installed ccss files to display
	
    try
    {
        m_spectralSamples = new SpectralSampleFiles;		
    }
    catch (std::logic_error& e)
    {
        GetColorApp()->InMeasureMessageBox( e.what(), "Argyll Meter", MB_OK+MB_ICONHAND);
    }
}

CArgyllSensor::CArgyllSensor(ArgyllMeterWrapper* meter) :
    m_DisplayType(0),
    m_ReadingType(0),
    m_meter(meter)
{
    std::string meterName(m_meter->getMeterName());
    m_DisplayType = GetConfig()->GetProfileInt(meterName.c_str(), "DisplayType", 0);
    m_ReadingType = GetConfig()->GetProfileInt(meterName.c_str(), "ReadingType", 0);
    m_SpectralType = GetConfig()->GetProfileString(meterName.c_str(), "SpectralType", "Default");
    m_intTime = GetConfig()->GetProfileInt(meterName.c_str(), "IntTime", 1);
    m_debugMode = GetConfig()->GetProfileInt(meterName.c_str(), "DebugMode", 0) != 0;
    m_HiRes = GetConfig()->GetProfileInt(meterName.c_str(), "HiRes", 0);

    m_Adapt = GetConfig()->GetProfileInt(meterName.c_str(), "Adapt", 0);
    m_DisableAIO = GetConfig()->GetProfileInt(meterName.c_str(), "DisableAIO", 0);
    m_spectralApplyLeaveMeasures = FALSE;

    m_ArgyllSensorPropertiesPage.m_pSensor = this;

    m_pDevicePage = & m_ArgyllSensorPropertiesPage;  // Add Argyll settings page to property sheet

    m_PropertySheetTitle = IDS_ARGYLLSENSOR_PROPERTIES_TITLE;

    SetName(CString(meterName.c_str()));
	m_logFile = GetConfig () -> m_logFileName;

    // Retrieve the list of installed ccss files to display

    try
    {
        m_spectralSamples = new SpectralSampleFiles;		
    }
    catch (std::logic_error& e)
    {
        GetColorApp()->InMeasureMessageBox( e.what(), "Argyll Meter", MB_OK+MB_ICONHAND);
    }
}

CArgyllSensor::~CArgyllSensor()
{
    // we don't own the meter don't delete it

    delete m_spectralSamples;

}

void CArgyllSensor::Copy(CSensor * p)
{
    COneDeviceSensor::Copy(p);

    m_DisplayType = ((CArgyllSensor*)p)->m_DisplayType;
    m_ReadingType = ((CArgyllSensor*)p)->m_ReadingType;
    m_SpectralType = ((CArgyllSensor*)p)->m_SpectralType;
    m_intTime = ((CArgyllSensor*)p)->m_intTime;
    m_HiRes = ((CArgyllSensor*)p)->m_HiRes;
    m_Adapt = ((CArgyllSensor*)p)->m_Adapt;
    m_DisableAIO = ((CArgyllSensor*)p)->m_DisableAIO;
    m_spectralCorrectionPath = ((CArgyllSensor*)p)->m_spectralCorrectionPath;
    m_spectralCorrectionDesc = ((CArgyllSensor*)p)->m_spectralCorrectionDesc;


    if(m_meter >= 0)
    {
        m_meter = ((CArgyllSensor*)p)->m_meter;
        SetName(CString(m_meter->getMeterName().c_str()));
    }

    if(m_spectralSamples)
    {
        *m_spectralSamples = *(((CArgyllSensor*)p)->m_spectralSamples);
    }
}

void CArgyllSensor::Serialize(CArchive& archive)
{
    COneDeviceSensor::Serialize(archive) ;

    if (archive.IsStoring())
    {
        int version=6;
        archive << version;
        archive << m_DisplayType;
        archive << m_ReadingType;
        archive << m_SpectralType;
        archive << m_debugMode;
        archive << m_HiRes;
        archive << m_intTime;
        archive << m_Adapt;
        archive << m_DisableAIO;
        archive << m_spectralCorrectionPath;
        archive << m_spectralCorrectionDesc;
        if(m_meter)
        {
            archive << CString(m_meter->getMeterName().c_str());
        }
    }
    else
    {
        int version;
        archive >> version;
        if ( version > 6 )
            AfxThrowArchiveException ( CArchiveException::badSchema );
        archive >> m_DisplayType;
        archive >> m_ReadingType;
        archive >> m_SpectralType;
        if ( version > 3)
            archive >> m_Adapt;
        if ( version > 4)
            archive >> m_DisableAIO;
        if ( version > 2)
            archive >> m_intTime;
        if(version == 1)
        {
            UINT dummy;
            archive >> dummy;
        }
        archive >> m_debugMode;
        archive >> m_HiRes;
        if ( version > 5 )
        {
            // Written right after DisableAIO on store; the store/load field
            // orders differ but stay byte-aligned (BOOL/UINT are both 4 bytes),
            // so these two CStrings sit at the same stream position either way.
            archive >> m_spectralCorrectionPath;
            archive >> m_spectralCorrectionDesc;
        }

        std::string errorMessage;
        ArgyllMeterWrapper::ArgyllMeterWrappers meters = ArgyllMeterWrapper::getDetectedMeters(errorMessage);
        if(version > 1)
        {
            // try and open the same meter we were saved with
            // otherwise exit so that we get the simulated meter
            CString meterName;
            archive >> meterName;
            for(size_t i(0); i < meters.size(); ++i)
            {
			    GetColorApp()->InMeasureMessageBox( meterName, "Argyll Meter used during this session...", MB_OK+MB_ICONHAND);
                if(meters[i]->getMeterName().c_str() == meterName)
                {
                    m_meter = meters[i];
                    SetName(CString(m_meter->getMeterName().c_str()));
                }
            }
        }
        else
        {
            // if we don't yet have a meter
            // open whatever the first meter is
            // if we leave here with nothing then 
            // we should ge replaced by the simulated meter
            // in the higher up object
            if(meters.size() > 0)
            {
                m_meter = meters[0];
                SetName(CString(m_meter->getMeterName().c_str()));
            }
        }
    }
}

void CArgyllSensor::SetPropertiesSheetValues()
{
    COneDeviceSensor::SetPropertiesSheetValues();

    m_ArgyllSensorPropertiesPage.m_DisplayType=m_DisplayType;
    m_ArgyllSensorPropertiesPage.m_ReadingType=m_ReadingType;
    m_ArgyllSensorPropertiesPage.m_SpectralType=m_SpectralType;
    m_ArgyllSensorPropertiesPage.m_intTime=m_intTime;
    m_ArgyllSensorPropertiesPage.m_DebugMode=m_debugMode;
    m_ArgyllSensorPropertiesPage.m_HiResCheckBoxEnabled = m_meter->doesSupportHiRes();
    m_ArgyllSensorPropertiesPage.m_obTypeEnabled = (m_meter->doesMeterSupportSpectralSamples() || !m_meter->isColorimeter());
    m_ArgyllSensorPropertiesPage.m_intTimeEnabled = (m_meter->getMeterName() == "X-Rite i1 DisplayPro, ColorMunki Display");
    m_ArgyllSensorPropertiesPage.m_HiRes=m_HiRes;
    m_ArgyllSensorPropertiesPage.m_Adapt=m_Adapt;
    m_ArgyllSensorPropertiesPage.m_DisableAIO=m_DisableAIO;
    // Rev. B AIO only applies to the i1d3 / ColorMunki Display colorimeter family.
    m_ArgyllSensorPropertiesPage.m_AIOEnabled =
        (m_meter->getMeterName() == "X-Rite i1 DisplayPro, ColorMunki Display");
    m_ArgyllSensorPropertiesPage.m_MeterName = m_meter->getMeterName().c_str();
}

void CArgyllSensor::GetPropertiesSheetValues()
{
    COneDeviceSensor::GetPropertiesSheetValues();
    std::string meterName(m_meter->getMeterName());

    if(m_debugMode != !!m_ArgyllSensorPropertiesPage.m_DebugMode) 
    {
        SetModifiedFlag(TRUE);
        m_debugMode = m_ArgyllSensorPropertiesPage.m_DebugMode != 0;
        GetConfig()->WriteProfileInt(meterName.c_str(), "DebugMode", m_debugMode?1:0);
    }

    if(m_ReadingType != m_ArgyllSensorPropertiesPage.m_ReadingType ||
        m_SpectralType != m_ArgyllSensorPropertiesPage.m_SpectralType ||
        m_DisplayType != m_ArgyllSensorPropertiesPage.m_DisplayType ||
        m_HiRes != m_ArgyllSensorPropertiesPage.m_HiRes ||
        m_intTime != m_ArgyllSensorPropertiesPage.m_intTime ||
		m_Adapt != m_ArgyllSensorPropertiesPage.m_Adapt ||
		m_DisableAIO != m_ArgyllSensorPropertiesPage.m_DisableAIO)
    {
        SetModifiedFlag(TRUE);
        m_ReadingType=m_ArgyllSensorPropertiesPage.m_ReadingType;
        m_SpectralType=m_ArgyllSensorPropertiesPage.m_SpectralType;
        m_DisplayType=m_ArgyllSensorPropertiesPage.m_DisplayType;
        m_HiRes = m_ArgyllSensorPropertiesPage.m_HiRes;
        m_intTime=m_ArgyllSensorPropertiesPage.m_intTime;
        m_Adapt=m_ArgyllSensorPropertiesPage.m_Adapt;
        m_DisableAIO=m_ArgyllSensorPropertiesPage.m_DisableAIO;

        GetConfig()->WriteProfileInt(meterName.c_str(), "ReadingType", m_ReadingType );
        GetConfig()->WriteProfileString(meterName.c_str(), "SpectralType", m_SpectralType );
        GetConfig()->WriteProfileInt(meterName.c_str(), "IntTime", m_intTime );
        GetConfig()->WriteProfileInt(meterName.c_str(), "DisplayType", m_DisplayType );
        GetConfig()->WriteProfileInt(meterName.c_str(), "HiRes", m_HiRes );
        GetConfig()->WriteProfileInt(meterName.c_str(), "Adapt", m_Adapt );
        GetConfig()->WriteProfileInt(meterName.c_str(), "DisableAIO", m_DisableAIO );
        Init(TRUE);
    }
}

BOOL CArgyllSensor::Init( BOOL bForSimultaneousMeasures )
{
    std::string errorDescription;
    double i_time=0.0;

    switch (m_intTime)
    {
        case 1:
            i_time = 0.50;
            break;
        case 2:
            i_time = 0.30;
            break;
        case 3:
            i_time = 0.40;
            break;
        case 4:
            i_time = 0.60;
            break;
        case 5:
            i_time = 0.80;
            break;
        case 6:
            i_time = 1.0;
            break;
    }

    if(!m_meter->connectAndStartMeter(errorDescription, (ArgyllMeterWrapper::eReadingType)m_ReadingType, m_SpectralType, CArgyllSensor::isInDebugMode(), i_time, ((m_DisplayType == 1) || CArgyllSensor::isRefresh()) && !(m_DisplayType == 0)  ) )
    {
        GetColorApp()->InMeasureMessageBox( errorDescription.c_str(), "Argyll Meter", MB_OK+MB_ICONHAND);
        m_meter = 0;
        return FALSE;
    }
    m_meter->setHiResMode(!!m_HiRes);
    // Apply the Rev. B AIO setting. The meter is initialised by now, so this
    // overrides whatever init_inst() selected and takes effect immediately on a
    // settings change (Init() is re-run from GetPropertiesSheetValues), with no
    // re-init or restart needed. No-op for non-i1d3 meters.
    m_meter->setDisableAIO(!!m_DisableAIO);
    // Re-apply the low-light averaging setting so it survives reconnects /
    // restarts (the wrapper resets it on each new meter object).
    m_meter->setAdapt(!!m_Adapt);
    if(m_DisplayType != 0xFFFFFFFF)
    {
        m_meter->setDisplayType(m_DisplayType);
    }

    // Re-apply a loaded spectral (ccss/CSV) correction so it survives reconnects
    // and restarts (restores the apply path dropped in 2014). Only meaningful for
    // spectral-capable colorimeters; failures are swallowed so a stale/missing
    // file never blocks startup.
    if ( !m_spectralCorrectionPath.IsEmpty() )
    {
        try
        {
            if ( m_meter->doesMeterSupportSpectralSamples() )
            {
                SpectralSample ss;
                CString ext = m_spectralCorrectionPath.Right(4); ext.MakeLower();
                bool ok = ( ext == ".csv" )
                    ? ss.createFromColourSpaceCSV((LPCSTR)m_spectralCorrectionPath)
                    : ss.Read((LPCSTR)m_spectralCorrectionPath);
                if ( ok )
                    m_meter->loadSpectralSample(ss);
            }
        }
        catch ( std::logic_error & )
        {
        }
    }

    //Alert user if in ambient/lux mode
    if (bForSimultaneousMeasures)
    {
        if (m_meter->getReadingType() == 2)
            GetColorApp()->InMeasureMessageBox( "Ambient mode set, values will be reported in LUX", "Argyll Meter set-up", MB_OK);
        if (m_meter->getReadingType() != m_ReadingType)
        {
            char s1 [128];
            sprintf(s1, "Reading mode not available, defaulting to %s",m_meter->getReadingType()==0?"DISPLAY":(m_meter->getReadingType()==1?"TELEPHOTO":"AMBIENT"));
            GetColorApp()->InMeasureMessageBox( s1, "Argyll Meter set-up", MB_OK);
            m_ReadingType = m_meter->getReadingType();
        }   
    }
    return TRUE;
}

BOOL CArgyllSensor::Release()
{
    return CSensor::Release();
}

bool CArgyllSensor::MeterSupportsSpectralSamples()
{
    if ( !m_meter )
        return false;
    try
    {
        return m_meter->doesMeterSupportSpectralSamples();
    }
    catch ( std::logic_error & )
    {
        return false;
    }
}

bool CArgyllSensor::ApplySpectralCorrection(const CString& filePath)
{
    if ( filePath.IsEmpty() )
        return false;

    // Read (and validate) the file first, so we don't prompt then fail.
    SpectralSample ss;
    try
    {
        CString ext = filePath.Right(4); ext.MakeLower();
        bool ok = ( ext == ".csv" )
            ? ss.createFromColourSpaceCSV((LPCSTR)filePath)
            : ss.Read((LPCSTR)filePath);
        if ( !ok )
            throw std::logic_error("Could not read the selected spectral correction file.");
    }
    catch ( std::logic_error & e )
    {
        GetColorApp()->InMeasureMessageBox( e.what(), "Spectral correction", MB_OK+MB_ICONHAND );
        return false;
    }

    // Confirm before discarding an active matrix calibration. Existing
    // measurements were taken through that matrix; let the user strip them back
    // to raw or keep them as-is (mixed with the new spectral-corrected reads).
    // Cancel aborts the apply entirely (so it really is "before applying").
    m_spectralApplyLeaveMeasures = FALSE;
    bool hasMatrixCal = !GetSensorMatrix().IsIdentity()
                     || GetCalibrationMethod() == CALIB_BODNER_THREEMATRIX;
    if ( hasMatrixCal )
    {
        int r = GetColorApp()->InMeasureMessageBox(
            "This sensor has a matrix calibration. Applying a spectral correction "
            "will make it the sole correction for new readings.\n\n"
            "Existing measurements were taken with the matrix calibration - strip "
            "them back to raw (uncorrected) sensor values?\n\n"
            "Yes - strip existing measurements to raw\n"
            "No - leave them as they are (mixed with new spectral readings)\n"
            "Cancel - do not apply the spectral correction",
            "Spectral correction", MB_YESNOCANCEL | MB_ICONQUESTION );
        if ( r == IDCANCEL )
            return false;
        m_spectralApplyLeaveMeasures = ( r == IDNO );
    }

    // Load onto the meter (Argyll driver) if connected and capable.
    try
    {
        if ( m_meter && m_meter->doesMeterSupportSpectralSamples() )
            m_meter->loadSpectralSample(ss);
    }
    catch ( std::logic_error & e )
    {
        GetColorApp()->InMeasureMessageBox( e.what(), "Spectral correction", MB_OK+MB_ICONHAND );
        return false;
    }

    m_spectralCorrectionPath = filePath;
    // Show just the panel name in the UI (the derived tech still goes into the
    // ccss metadata); getDescription() would nest the tech in parentheses.
    m_spectralCorrectionDesc = ss.getDisplay();

    // The spectral correction is applied inside the Argyll driver, so make it the
    // sole correction: force HCFR's own sensor matrix to identity (and drop any
    // Bodner sub-gamut matrices) so the two regimes can't double-correct.
    SetSensorMatrix(Matrix::IdentityMatrix(3));
    SetSensorMatrixMod(Matrix::IdentityMatrix(3));
    ClearBodnerMatrices();
    SetCalibrationMethod(CALIB_HCFR_DEFAULT);
    SetModifiedFlag(TRUE);
    return true;
}

void CArgyllSensor::ClearSpectralCorrection()
{
    if ( m_meter )
    {
        try { m_meter->resetSpectralSample(); }
        catch ( std::logic_error & ) {}
    }
    m_spectralCorrectionPath.Empty();
    m_spectralCorrectionDesc.Empty();
    SetModifiedFlag(TRUE);
}

BOOL CArgyllSensor::TakePendingSpectralLeaveMeasures()
{
    BOOL leave = m_spectralApplyLeaveMeasures;
    m_spectralApplyLeaveMeasures = FALSE;
    return leave;
}

CColor CArgyllSensor::MeasureColorInternal(const ColorRGBDisplay& aRGBValue, int displaymode)
{
    if(!m_meter) if(!Init(FALSE)) return noDataColor;
    ArgyllMeterWrapper::eMeterState state(ArgyllMeterWrapper::NEEDS_MANUAL_CALIBRATION);
    while(state != ArgyllMeterWrapper::READY)
    {
        try
        {
            state = m_meter->takeReading(m_SpectralType);
        }
        catch(std::logic_error&)
        {
            return noDataColor;
        }
        if(state == ArgyllMeterWrapper::NEEDS_MANUAL_CALIBRATION)
        {
            Calibrate();
        }
        if(state == ArgyllMeterWrapper::INCORRECT_POSITION)
        {
            GetColorApp()->InMeasureMessageBox( m_meter->getIncorrectPositionInstructions().c_str(), "Incorrect Position", MB_OK+MB_ICONHAND);
        }
    }
    return m_meter->getLastReading();
}

void CArgyllSensor::Calibrate()
{
    if(!Init(FALSE)) {
        GetColorApp()->InMeasureMessageBox( "Meter failed init check","Meter initialization error",MB_OK);
        return;
    }

    if(!m_meter->doesMeterSupportCalibration()) 
    {
        GetColorApp()->InMeasureMessageBox( "No calibration capabilities are defined for this probe.","No Cals found",MB_OK);
        return;
    }

    ArgyllMeterWrapper::eMeterState state(ArgyllMeterWrapper::NEEDS_MANUAL_CALIBRATION);
    while(state != ArgyllMeterWrapper::READY)
    {
		bool b_TestWindow = false;
        std::string meterInstructions(m_meter->getCalibrationInstructions(m_HiRes != 0));
        if(meterInstructions.empty())
        {
            break;
        }
		if (meterInstructions == "Provide an 80% or greater white test patch" && GetConfig()->GetGeneratorType() == CColorHCFRConfig::enumAutomatic)
		{
			CGDIGenerator *m_pGenerator;
			int display_mode  = GetConfig()->GetProfileInt("GDIGenerator","DisplayMode", DISPLAY_DEFAULT_MODE);
			m_pGenerator = new CGDIGenerator(display_mode, false);
			if (display_mode == DISPLAY_ccast || display_mode == DISPLAY_rPI)
			{
				if (display_mode == DISPLAY_ccast)
					GetColorApp()->InMeasureMessageBox( "Place test window and meter on monitor connected to your ChromeCast device.", "Calibration Instructions", MB_OK);
				else
					GetColorApp()->InMeasureMessageBox( "Place test window and meter on monitor connected to your rPi device.", "Calibration Instructions", MB_OK);

				m_pGenerator->Display80();
				delete m_pGenerator;
			}
			else
			{
				( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.ShowWindow(SW_SHOW);
				( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.m_colorPicker.SetColor(RGB(204,204,204));
				( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> EnableWindow ( TRUE );
				( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.SetForegroundWindow();
				b_TestWindow = true;
				GetColorApp()->InMeasureMessageBox( "Place test window and meter on monitor to be calibrated.", "Calibration Instructions", MB_OK);
			}
		} else
	        GetColorApp()->InMeasureMessageBox( meterInstructions.c_str(), "Calibration Instructions", MB_OK);

		state = m_meter->calibrate();
		if (b_TestWindow)
			( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.ShowWindow(SW_HIDE);

        if(state == ArgyllMeterWrapper::INCORRECT_POSITION)
        {
            GetColorApp()->InMeasureMessageBox( m_meter->getIncorrectPositionInstructions().c_str(), "Incorrect Position", MB_OK+MB_ICONHAND);
        }
    }
    GetColorApp()->InMeasureMessageBox( "Device is now calibrated.  If the device requires it return to the correct measurement position.", "Calibration Complete", MB_OK);
}

void CArgyllSensor::GetUniqueIdentifier( CString & strId )
{
    strId = m_meter->getMeterName().c_str();
}

void CArgyllSensor::FillDisplayTypeCombo(CComboBox& comboToFill)
{

    int numDisplayTypes(m_meter->getNumberOfDisplayTypes());

    if(numDisplayTypes > 1)
    {
        for(int i(0); i < numDisplayTypes; ++i)
        {
            comboToFill.AddString(m_meter->getDisplayTypeText(i));
        }
    }
}

void CArgyllSensor::FillSpectralTypeCombo(CComboBox& comboToFill)
{
    int numObTypes(m_meter->getNumberOfObTypes());

    comboToFill.ResetContent();
    for (int i(0); i < m_meter->getNumberOfObTypes(); ++i)
    {
        comboToFill.AddString(m_meter->getObTypeText(i));
    }
}

// very basic logging and error handling to override
// the standard argyll verion
// should use whatever log library we end up with
void ArgyllLogMessage(const char* messageType, char *fmt, va_list& args)
{
    if(CArgyllSensor::isInDebugMode())
    {
        FILE *logFile = fopen( m_logFile, "a" );
        fprintf(logFile,"Argyll %s - ", messageType);
        vfprintf(logFile, fmt, args);
        va_end(args);
        fprintf(logFile,"\n");
        fclose(logFile);
    }
}

bool CArgyllSensor::isColorimeter() const
{
    return m_meter->isColorimeter();
}

bool CArgyllSensor::supportsAvg() const
{
    // The low-light averager is a software loop in the wrapper, available for
    // every Argyll meter once one is connected.
    return (m_meter != 0);
}

void CArgyllSensor::setAvgEnabled(bool bOn)
{
    m_Adapt = bOn ? 1 : 0;
    if (m_meter)
    {
        m_meter->setAdapt(!!m_Adapt);
        GetConfig()->WriteProfileInt(m_meter->getMeterName().c_str(), "Adapt", m_Adapt);
    }
}

bool CArgyllSensor::getAvgEnabled() const
{
    return !!m_Adapt;
}

bool CArgyllSensor::isRefresh() const
{
    return m_meter->isRefresh();
}
