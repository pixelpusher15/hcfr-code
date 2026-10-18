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
#include "ArchiveExtension.h"
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

// The observer a spectral correction record stores. Argyll's icxOT_default (0,
// "usually CIE_1931_2", libconv/xspect.h) is what the wrapper reports until an Init
// has passed the page's Spectral Type string: a meter first connected lazily
// (checkMeterIsInitialized passes "Default") reports 0, the next Init reports 3 for
// the same physical observer, and Argyll itself treats the two as one observer
// (standardObserver in Tools/spectro/xspect.c: `case icxOT_default: case
// icxOT_CIE_1931_2:`). Recording the canonical value keeps a record derived under
// one from being re-derived, to identical matrices, under the other.
static int RecordedObserver(ArgyllMeterWrapper *meter)
{
    const int obType = meter->getObType();
    return ( obType == 0 /* icxOT_default */ ) ? 3 /* icxOT_CIE_1931_2 */ : obType;
}

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
    m_spectralCacheValid = false;
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
    m_spectralCacheValid = false;

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
        int version=7;
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
        // Written unconditionally: the load has read a meter name for every
        // version > 1, so writing it only when a meter was attached desynced
        // the stream. An empty name matches no meter.
        archive << CString( m_meter ? m_meter->getMeterName().c_str() : "" );

        // Forward-compatible extension trailer (ArchiveExtension.h); empty here.
        CExtensionWriter ext;
        ext.Write(archive);
    }
    else
    {
        int version;
        archive >> version;
        if ( version > 7 )
            throw new CNewerSchemaException ( "CArgyllSensor", version );
        archive >> m_DisplayType;
        archive >> m_ReadingType;
        archive >> m_SpectralType;
        if(version == 1)
        {
            UINT dummy;
            archive >> dummy;
        }
        // Read in the SAME order the store writes: debugMode, HiRes, intTime,
        // Adapt, DisableAIO. The historical load read these in a different order
        // than the store (Adapt/DisableAIO/intTime before debugMode/HiRes), which
        // silently swapped the flag values on every reload. The store order has
        // only ever grown by appending (intTime@v3, Adapt@v4, DisableAIO@v5), so
        // this reads every version's stream correctly.
        archive >> m_debugMode;
        archive >> m_HiRes;
        if ( version > 2)
            archive >> m_intTime;
        if ( version > 3)
            archive >> m_Adapt;
        if ( version > 4)
            archive >> m_DisableAIO;
        if ( version > 5 )
        {
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

        if ( version >= 7 )
        {
            // Skip any field a newer build added to this block.
            CExtensionReader ext(archive);
            BYTE  tag;
            DWORD cb;

            while ( ext.Next(tag, cb) )
                ext.Skip();
            ext.Close();
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
    // spectral-capable colorimeters. A failure must not pass silently: the stored
    // path/description keep telling the UI a correction is active while every
    // reading is uncorrected (a moved/edited file, or a meter without spectral
    // support), so warn once per session and leave the state for the user to
    // re-apply or clear in the sensor properties. Startup itself is never blocked.
    // Meters whose calibration matrices are exposed (the i1d3 family) hold no
    // spectral state while HCFR measures: the correction was derived at Browse and
    // HCFR applies it, so there is nothing to re-apply to the driver.
    if ( !m_spectralCorrectionPath.IsEmpty() && !m_meter->canExposeCalibrationMatrices() )
    {
        bool applied = false;
        try
        {
            if ( m_meter->doesMeterSupportSpectralSamples() )
            {
                // Parse once and cache: Init runs per sweep (and per single
                // measurement via AddMeasurement); the file does not change
                // underneath us. Re-read only when the path changed.
                bool ok = m_spectralCacheValid
                       && m_spectralCachePath == m_spectralCorrectionPath;
                if ( !ok )
                {
                    SpectralSample ss;
                    CString ext = m_spectralCorrectionPath.Right(4); ext.MakeLower();
                    ok = ( ext == ".csv" )
                        ? ss.createFromCorrelationCSV((LPCSTR)m_spectralCorrectionPath)
                        : ss.Read((LPCSTR)m_spectralCorrectionPath);
                    if ( ok )
                    {
                        m_spectralSampleCache = ss;
                        m_spectralCachePath = m_spectralCorrectionPath;
                        m_spectralCacheValid = true;
                    }
                }
                if ( ok )
                {
                    m_meter->loadSpectralSample(m_spectralSampleCache);
                    applied = true;
                }
            }
        }
        catch ( std::logic_error & )
        {
        }
        if ( !applied )
        {
            static BOOL s_warnedSpectralReapply = FALSE;
            if ( !s_warnedSpectralReapply )
            {
                s_warnedSpectralReapply = TRUE;
                CString msg, fmt, title;
                fmt.LoadString ( IDS_SPECTRAL_REAPPLY_FAILED );
                title.LoadString ( IDS_SPECTRAL_TITLE );
                msg.Format ( fmt, (LPCSTR) m_spectralCorrectionPath );
                GetColorApp()->InMeasureMessageBox ( msg, title, MB_OK | MB_ICONWARNING );
            }
        }
        else if ( m_spectral.source != SPECTRAL_SOURCE_FILE )
        {
            // A sample loaded by a build that kept no record: record it now, as
            // ApplySpectralCorrection would have (the driver applies it; identity
            // matrices, driver-corrected basis).
            SpectralCorrectionRecord rec;
            rec.source = SPECTRAL_SOURCE_FILE;
            rec.description = (LPCSTR) m_spectralCorrectionDesc;
            unsigned long long hash = 0;
            if ( !Fnv1a64File((LPCSTR)m_spectralCorrectionPath, hash) )
                hash = 0;
            rec.fileHash = hash;
            rec.observer = RecordedObserver(m_meter);
            rec.meterSerial = m_meter->getSerialNumber();
            rec.basis = FIT_BASIS_DRIVER_CORRECTED;
            SetSpectralRecord(rec);
            if ( GetCalibrationMethod() == CALIB_HCFR_DEFAULT && GetSensorMatrix().IsIdentity() )
            {
                SetCalibrationMethod(CALIB_SPECTRAL_SAMPLE);
                SetFitBasis(rec.basis);
            }
            SetModifiedFlag(TRUE);
        }
    }
    SyncSpectralRecordFromMeter();

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
            ? ss.createFromCorrelationCSV((LPCSTR)filePath)
            : ss.Read((LPCSTR)filePath);
        if ( !ok )
            throw std::logic_error("Could not read the selected spectral correction file.");
    }
    catch ( std::logic_error & e )
    {
        CString sTitle; sTitle.LoadString( IDS_SPECTRAL_TITLE );
        GetColorApp()->InMeasureMessageBox( e.what(), sTitle, MB_OK+MB_ICONHAND );
        return false;
    }

    // The spectral correction replaces a matrix calibration for the values shown.
    // Nothing is lost: every reading keeps its raw (native) value, and the
    // document's recompute from raw follows on the sheet's OK. Cancel aborts the
    // apply entirely (so it really is "before applying").
    if ( IsCorrectionActive() && !UsesSpectralMethod() )
    {
        CString prompt, title;
        prompt.LoadString ( IDS_SPECTRAL_MATRIXCAL_PROMPT );
        title.LoadString ( IDS_SPECTRAL_TITLE );
        int r = GetColorApp()->InMeasureMessageBox( prompt, title,
            MB_OKCANCEL | MB_ICONQUESTION );
        if ( r != IDOK )
            return false;
    }

    // A meter whose matrices are exposed: the driver's own fit turns the sample into
    // a matrix, read out once, and the driver is back to native before any reading
    // (the wrapper restores it on every path). Any other meter keeps the sample
    // loaded and applies it itself.
    const bool exposable = ( m_meter && m_meter->canExposeCalibrationMatrices() );
    Matrix native, current;
    if ( exposable )
    {
        std::string why;
        if ( !m_meter->deriveSpectralCalibration(ss, native, current, why) )
        {
            CString sTitle; sTitle.LoadString( IDS_SPECTRAL_TITLE );
            GetColorApp()->InMeasureMessageBox( why.c_str(), sTitle, MB_OK+MB_ICONHAND );
            return false;
        }
    }
    else
    {
        try
        {
            if ( m_meter && m_meter->doesMeterSupportSpectralSamples() )
                m_meter->loadSpectralSample(ss);
        }
        catch ( std::logic_error & e )
        {
            CString sTitle; sTitle.LoadString( IDS_SPECTRAL_TITLE );
            GetColorApp()->InMeasureMessageBox( e.what(), sTitle, MB_OK+MB_ICONHAND );
            return false;
        }
    }

    // The record. A second Browse simply overwrites it: each derive starts and ends
    // with the driver native (and a driver-applied sample is replaced, not added
    // to), so corrections cannot stack.
    SpectralCorrectionRecord record;
    record.source = SPECTRAL_SOURCE_FILE;
    const char *display = ss.getDisplay();
    const char *descr = ss.getDescription();
    record.description = display ? display : "";
    if ( record.description.empty() && descr != NULL )
        record.description = descr;
    unsigned long long hash = 0;
    if ( !Fnv1a64File((LPCSTR)filePath, hash) )
        hash = 0;   // identifies the file only; not a reason to refuse it
    record.fileHash = hash;
    if ( m_meter )
    {
        record.observer = RecordedObserver(m_meter);
        record.meterSerial = m_meter->getSerialNumber();
    }
    if ( exposable )
    {
        // HCFR applies the correction to the native readings the driver reports.
        Matrix corr, toNative;      // toNative: unused, the driver is native
        std::string why;
        if ( !DeriveSpectralCorrection(native, current, true, corr, toNative, why) )
        {
            CString sTitle; sTitle.LoadString( IDS_SPECTRAL_TITLE );
            GetColorApp()->InMeasureMessageBox( why.c_str(), sTitle, MB_OK+MB_ICONHAND );
            return false;
        }
        record.nativeCal = native;
        record.currentCal = current;
        record.correction = corr;
        record.basis = FIT_BASIS_NATIVE;
    }
    else
    {
        // The driver keeps applying the sample itself: raw is driver-corrected and
        // the method's matrix is identity.
        record.nativeCal = Matrix::IdentityMatrix(3);
        record.currentCal = Matrix::IdentityMatrix(3);
        record.correction = Matrix::IdentityMatrix(3);
        record.basis = FIT_BASIS_DRIVER_CORRECTED;
    }

    m_spectralCorrectionPath = filePath;
    // Show just the panel name in the UI (the derived tech still goes into the
    // ccss metadata); getDescription() would nest the tech in parentheses.
    m_spectralCorrectionDesc = ss.getDisplay();

    // The spectral correction becomes the sensor's method: its single matrix is
    // the record's correction (and any Bodner sub-gamut matrices are dropped).
    SetSpectralRecord(record);
    SetSensorMatrix(record.correction);
    // Deliberately do NOT reset m_sensorToXYZMatrixMod here. OnConfigureSensor saved the
    // pre-configure matrix into it as the baseline for un-applying the old correction from
    // legacy (no-raw) measurements: delta = fullMatrix * SensorMatrixMod.inverse. Clearing it
    // makes that delta identity, so those measurements silently keep the old matrix under
    // the new correction. OnConfigureSensor clears the baseline itself once the recompute
    // is done.
    ClearBodnerMatrices();
    SetCalibrationMethod(CALIB_SPECTRAL_SAMPLE);
    SetFitBasis(record.basis);
    m_spectralCacheValid = false;
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
    m_spectralCacheValid = false;
    CSensor::ClearSpectralCorrection();
}

namespace
{
    // printf-style front end to ArgyllLogMessage (the Argyll debug log)
    void SpectralSyncLog(const char *fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        ArgyllLogMessage("Debug", const_cast<char *>(fmt), args);
        va_end(args);
    }
}

// Runs at the end of every Init (per sweep and per single measurement), for a
// meter whose calibration matrices are exposed (the i1d3 family) only. The driver
// holds no spectral state while HCFR measures, so nothing is checked against it:
// a ccss/ccmx Display Type entry is derived and the driver left on the entry's
// base (native), and a browsed sample is derived again only when the connected
// unit or observer is not the one the record was derived for. Shows no dialog but
// the once-per-session unit notice. Readings already taken keep their values; the
// next Sensor -> Configure OK recomputes them from raw (OnConfigureSensor's
// CorrectionChangedSinceBeginConfigure sees the record or method change, because
// the Display Type is changed through that sheet and this Init runs inside its OK).
void CArgyllSensor::SyncSpectralRecordFromMeter()
{
    if ( m_meter == NULL || !m_meter->canExposeCalibrationMatrices() )
        return;

    const int observer = RecordedObserver(m_meter);
    const std::string serial = m_meter->getSerialNumber();

    if ( m_DisplayType != 0xFFFFFFFF && m_meter->isDisplayTypeCorrected((int)m_DisplayType) )
    {
        // The Display Type entry carries a correction (a ccss or ccmx entry). Its
        // matrix is read out of the driver and the driver measures on the base
        // entry; m_DisplayType keeps the user's choice (persisted, shown in the page).
        Matrix native, current, corr, toNative;
        std::string why;
        int baseIndex = -1;
        if ( !m_meter->deriveDisplayTypeCalibration((int)m_DisplayType, baseIndex, native, current, why)
          || !DeriveSpectralCorrection(native, current, true, corr, toNative, why) )
        {
            SpectralSyncLog("CArgyllSensor: Display Type %d correction not derived: %s", (int)m_DisplayType, why.c_str());
            return;     // leave the record as it is
        }
        try
        {
            m_meter->setDisplayType(baseIndex);
        }
        catch ( std::logic_error & ) {}

        SpectralCorrectionRecord rec;
        rec.source = SPECTRAL_SOURCE_DISPLAY_TYPE;
        try
        {
            const char *text = m_meter->getDisplayTypeText((int)m_DisplayType);
            if ( text != NULL )
                rec.description = text;
        }
        catch ( std::logic_error & ) {}
        rec.fileHash = 0;
        rec.observer = observer;
        rec.meterSerial = serial;
        rec.nativeCal = native;
        rec.currentCal = current;
        rec.correction = corr;
        rec.basis = FIT_BASIS_NATIVE;
        if ( rec != m_spectral )
        {
            // No fit at all: the spectral correction becomes the method.
            const bool noFit = ( GetCalibrationMethod() == CALIB_HCFR_DEFAULT && GetSensorMatrix().IsIdentity() );
            SetSpectralRecord(rec);
            if ( UsesSpectralMethod() )
            {
                SetSensorMatrix(corr);      // another entry while it already is the method
                SetFitBasis(FIT_BASIS_NATIVE);
            }
            else if ( noFit )
            {
                SetSensorMatrix(corr);
                SetCalibrationMethod(CALIB_SPECTRAL_SAMPLE);
                SetFitBasis(FIT_BASIS_NATIVE);
            }
            SetModifiedFlag(TRUE);
        }
        return;
    }

    if ( m_spectral.source == SPECTRAL_SOURCE_DISPLAY_TYPE )
    {
        // Back on a plain display type: the entry's correction is gone.
        CSensor::ClearSpectralCorrection();
    }

    if ( m_spectralCorrectionPath.IsEmpty() )
        return;

    // A browsed sample. Its record holds the matrices; they belong to one unit and
    // one observer, so derive again only when either is not the connected one (a
    // record from a build that kept none, or from a Browse with no meter attached,
    // carries neither). A record whose source is not FILE (a document from an
    // earlier field-test build) is built the same way.
    const bool fileRecord = ( m_spectral.source == SPECTRAL_SOURCE_FILE );
    const bool serialChanged = ( m_spectral.meterSerial != serial );
    if ( fileRecord && !serialChanged && m_spectral.observer == observer )
        return;

    SpectralSample ss;
    Matrix native, current, corr, toNative;
    std::string why;
    bool derived = false;
    try
    {
        CString ext = m_spectralCorrectionPath.Right(4); ext.MakeLower();
        bool ok = ( ext == ".csv" )
            ? ss.createFromCorrelationCSV((LPCSTR)m_spectralCorrectionPath)
            : ss.Read((LPCSTR)m_spectralCorrectionPath);
        if ( !ok )
            why = "the spectral correction file could not be read";
        else
            derived = m_meter->deriveSpectralCalibration(ss, native, current, why)
                   && DeriveSpectralCorrection(native, current, true, corr, toNative, why);
    }
    catch ( std::logic_error & e )
    {
        why = e.what();
        derived = false;
    }
    if ( !derived )
    {
        SpectralSyncLog("CArgyllSensor: spectral correction %s not derived again: %s",
                        (LPCSTR)m_spectralCorrectionPath, why.c_str());
        return;     // leave the record as it is
    }

    if ( fileRecord && serialChanged && !m_spectral.meterSerial.empty() && !serial.empty() )
    {
        static BOOL s_warnedSpectralUnit = FALSE;
        if ( !s_warnedSpectralUnit )
        {
            s_warnedSpectralUnit = TRUE;
            CString msg, fmt, title;
            fmt.LoadString ( IDS_SPECTRAL_UNIT_MISMATCH );
            title.LoadString ( IDS_SPECTRAL_TITLE );
            msg.Format ( fmt, m_spectral.meterSerial.c_str(), serial.c_str() );
            GetColorApp()->InMeasureMessageBox ( msg, title, MB_OK | MB_ICONINFORMATION );
        }
    }

    SpectralCorrectionRecord rec;
    if ( fileRecord )
        rec = m_spectral;
    else
    {
        rec.source = SPECTRAL_SOURCE_FILE;
        rec.description = (LPCSTR) m_spectralCorrectionDesc;
        unsigned long long hash = 0;
        if ( !Fnv1a64File((LPCSTR)m_spectralCorrectionPath, hash) )
            hash = 0;
        rec.fileHash = hash;
    }
    rec.nativeCal = native;
    rec.currentCal = current;
    rec.correction = corr;
    rec.observer = observer;
    rec.meterSerial = serial;
    // Derived from the driver's own fit and applied by HCFR to native readings,
    // whatever the record said (a Browse with no meter attached was labeled
    // driver-corrected).
    rec.basis = FIT_BASIS_NATIVE;
    // No fit at all: the spectral correction becomes the method.
    const bool noFit = ( GetCalibrationMethod() == CALIB_HCFR_DEFAULT && GetSensorMatrix().IsIdentity() );
    SetSpectralRecord(rec);
    if ( UsesSpectralMethod() )
    {
        SetSensorMatrix(corr);
        SetFitBasis(FIT_BASIS_NATIVE);
    }
    else if ( !fileRecord && noFit )
    {
        SetSensorMatrix(corr);
        SetCalibrationMethod(CALIB_SPECTRAL_SAMPLE);
        SetFitBasis(FIT_BASIS_NATIVE);
    }
    SetModifiedFlag(TRUE);
}

bool CArgyllSensor::CorrectionChangedSinceBeginConfigure() const
{
    return CSensor::CorrectionChangedSinceBeginConfigure()
        || m_spectralCorrectionPath != m_cfgSnapSpectralPath
        || m_spectralCorrectionDesc != m_cfgSnapSpectralDesc;
}

void CArgyllSensor::BeginConfigure()
{
    m_spectralCacheValid = false;   // path may change during the sheet
    CSensor::BeginConfigure();
    m_cfgSnapSpectralPath = m_spectralCorrectionPath;
    m_cfgSnapSpectralDesc = m_spectralCorrectionDesc;
}

// A spectral apply (or clear) from the property page committed before the
// sheet closed; a cancelled sheet restores the previous state - members here,
// and the meter itself where connected. Errors pushing to the meter are
// non-fatal: the restored path is re-applied by Init() on every connect.
void CArgyllSensor::CancelConfigure()
{
    if ( m_spectralCorrectionPath != m_cfgSnapSpectralPath )
    {
        // A meter whose matrices are exposed holds no sample (each derive leaves it
        // native): the base below restores the record and the matrix, and there is
        // nothing to re-apply to the driver. Other meters get the previous sample back.
        const bool exposable = ( m_meter && m_meter->canExposeCalibrationMatrices() );
        if ( m_cfgSnapSpectralPath.IsEmpty() )
        {
            if ( m_meter )
            {
                try { m_meter->resetSpectralSample(); }
                catch ( std::logic_error & ) {}
            }
        }
        else if ( !exposable )
        {
            // Same re-apply Init() performs on connect.
            try
            {
                SpectralSample ss;
                CString ext = m_cfgSnapSpectralPath.Right(4); ext.MakeLower();
                bool ok = ( ext == ".csv" )
                    ? ss.createFromCorrelationCSV((LPCSTR)m_cfgSnapSpectralPath)
                    : ss.Read((LPCSTR)m_cfgSnapSpectralPath);
                if ( ok && m_meter && m_meter->doesMeterSupportSpectralSamples() )
                    m_meter->loadSpectralSample(ss);
            }
            catch ( std::logic_error & ) {}
        }
        m_spectralCorrectionPath = m_cfgSnapSpectralPath;
        m_spectralCorrectionDesc = m_cfgSnapSpectralDesc;
    }
    // The base restores the matrices, the method, the record and the fit basis.
    CSensor::CancelConfigure();
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
