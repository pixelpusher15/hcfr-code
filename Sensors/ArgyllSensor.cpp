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

// The stored spectral correction file could not be used: once per session (Init
// runs per sweep), from the re-apply path of a meter that keeps the sample and from
// the re-derive path of one whose matrices are exposed alike.
static void WarnSpectralReapplyFailedOnce(const CString& path)
{
    static BOOL s_warnedSpectralReapply = FALSE;
    if ( s_warnedSpectralReapply )
        return;
    s_warnedSpectralReapply = TRUE;
    CString msg, fmt, title;
    fmt.LoadString ( IDS_SPECTRAL_REAPPLY_FAILED );
    title.LoadString ( IDS_SPECTRAL_TITLE );
    msg.Format ( fmt, (LPCSTR) path );
    GetColorApp()->InMeasureMessageBox ( msg, title, MB_OK | MB_ICONWARNING );
}

// A Display Type record whose entry is not the one in use (a plain entry is
// selected, or the record's entry is gone from the list): it stays applied, and the
// user is told once per session.
static void NoticeLoadedDisplayTypeOnce(const std::string& description)
{
    static BOOL s_noticedLoadedDisplayType = FALSE;
    if ( s_noticedLoadedDisplayType )
        return;
    s_noticedLoadedDisplayType = TRUE;
    CString msg, fmt, title;
    fmt.LoadString ( IDS_SPECTRAL_LOADED_DISPTYPE_NOTICE );
    title.LoadString ( IDS_SPECTRAL_TITLE );
    msg.Format ( fmt, description.c_str() );
    GetColorApp()->InMeasureMessageBox ( msg, title, MB_OK | MB_ICONINFORMATION );
}

// The stored correction file is gone while its record, complete for the connected
// unit, keeps applying: told once per session.
static void NoticeSpectralFileMovedOnce(const CString& path)
{
    static BOOL s_noticedSpectralFileMoved = FALSE;
    if ( s_noticedSpectralFileMoved )
        return;
    s_noticedSpectralFileMoved = TRUE;
    CString msg, fmt, title;
    fmt.LoadString ( IDS_SPECTRAL_FILE_MOVED_NOTICE );
    title.LoadString ( IDS_SPECTRAL_TITLE );
    msg.Format ( fmt, (LPCSTR) path );
    GetColorApp()->InMeasureMessageBox ( msg, title, MB_OK | MB_ICONINFORMATION );
}

// A FILE-source record derived for another unit that cannot be derived again for
// the connected one (no correction file, or one that cannot be read): it stays in
// use unchanged, and the user is told once per session. Only when both serials are
// known: an empty one says nothing about which unit the matrices belong to.
static void NoticeForeignFileRecordOnce(const SpectralCorrectionRecord& rec, const std::string& connectedSerial)
{
    if ( rec.source != SPECTRAL_SOURCE_FILE || rec.meterSerial.empty() || connectedSerial.empty()
      || rec.meterSerial == connectedSerial )
        return;
    static BOOL s_noticedForeignRecord = FALSE;
    if ( s_noticedForeignRecord )
        return;
    s_noticedForeignRecord = TRUE;
    CString msg, fmt, title;
    fmt.LoadString ( IDS_SPECTRAL_FOREIGN_RECORD_NOTICE );
    title.LoadString ( IDS_SPECTRAL_TITLE );
    msg.Format ( fmt, rec.description.c_str(), rec.meterSerial.c_str() );
    GetColorApp()->InMeasureMessageBox ( msg, title, MB_OK | MB_ICONINFORMATION );
}

// The stored correction file is not the file the record in use was made from (the
// R4 rule, SpectralRecordFromPathFile): the path is dropped, once-per-session notice.
static void NoticePathNotRecordFileOnce(const CString& path, const std::string& description)
{
    static BOOL s_noticedPathNotRecordFile = FALSE;
    if ( s_noticedPathNotRecordFile )
        return;
    s_noticedPathNotRecordFile = TRUE;
    CString msg, fmt, title;
    fmt.LoadString ( IDS_SPECTRAL_PATH_NOT_RECORD_FILE );
    title.LoadString ( IDS_SPECTRAL_TITLE );
    msg.Format ( fmt, (LPCSTR) path, description.c_str() );
    GetColorApp()->InMeasureMessageBox ( msg, title, MB_OK | MB_ICONINFORMATION );
}

// A record nothing applies on this meter was removed (EnforceRecordApplied). One
// message per event, worded for what the meter can do; each wording has its own
// once-per-session flag (one shown must not silence another, which says something
// different):
//   IDS_SPECTRAL_RECORD_NOT_APPLIED_NOSAMPLES  the meter takes no spectral sample
//                                              (Browse is disabled there);
//   IDS_SPECTRAL_RECORD_NOT_APPLIED_ENTRY      the driver applies another Display
//                                              Type entry itself (arg2);
//   IDS_SPECTRAL_PATH_NOT_RECORD_FILE_REMOVED  the path rule dropped the path in the
//                                              same step (arg2: the path) - instead
//                                              of that notice and this one in a row;
//   IDS_SPECTRAL_RECORD_NOT_APPLIED            otherwise.
static void WarnRecordNotAppliedOnce(UINT id, const std::string& description, const CString& arg2)
{
    static BOOL s_warned[4] = { FALSE, FALSE, FALSE, FALSE };
    const int slot = ( id == IDS_SPECTRAL_RECORD_NOT_APPLIED_NOSAMPLES ) ? 1
                   : ( id == IDS_SPECTRAL_RECORD_NOT_APPLIED_ENTRY ) ? 2
                   : ( id == IDS_SPECTRAL_PATH_NOT_RECORD_FILE_REMOVED ) ? 3 : 0;
    if ( s_warned[slot] )
        return;
    s_warned[slot] = TRUE;
    CString msg, fmt, title;
    fmt.LoadString ( id );
    title.LoadString ( IDS_SPECTRAL_TITLE );
    if ( id == IDS_SPECTRAL_PATH_NOT_RECORD_FILE_REMOVED )
        msg.Format ( fmt, (LPCSTR) arg2, description.c_str() );
    else if ( id == IDS_SPECTRAL_RECORD_NOT_APPLIED_ENTRY )
        msg.Format ( fmt, description.c_str(), (LPCSTR) arg2 );
    else
        msg.Format ( fmt, description.c_str() );
    GetColorApp()->InMeasureMessageBox ( msg, title, MB_OK | MB_ICONWARNING );
}

// One message per failure in the sensor sheet. The wrapper shows its own box, with
// the driver's reason, before some of its failures, and says so
// (ArgyllMeterWrapper::errorBoxShown); for those HCFR shows nothing more. Every
// other failure reaching Init or Calibrate is shown by HCFR, once.
static void ShowUnlessWrapperDid(ArgyllMeterWrapper* meter, const char* what, const char* title)
{
    if ( meter == NULL || !meter->errorBoxShown() )
        GetColorApp()->InMeasureMessageBox( what, title, MB_OK+MB_ICONHAND);
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
    m_displayTypeFromIni = false;
    m_spectralCacheValid = false;
    m_dtBaseIndex = -1;
    m_dtBaseFor = 0xFFFFFFFF;
    m_displayTypeChangedInSheet = false;
    m_cfgSnapDeviceValid = false;
    m_displayTypeRecordEntryMissing = false;
    m_pathCheckedValid = false;
    m_pathCheckedHash = 0;
    m_spectralCacheHash = 0;
    m_pathDropPending = false;
    m_beginInit = INIT_OK;
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
    m_displayTypeFromIni = true;
    m_spectralCacheValid = false;
    m_dtBaseIndex = -1;
    m_dtBaseFor = 0xFFFFFFFF;
    m_displayTypeChangedInSheet = false;
    m_cfgSnapDeviceValid = false;
    m_displayTypeRecordEntryMissing = false;
    m_pathCheckedValid = false;
    m_pathCheckedHash = 0;
    m_spectralCacheHash = 0;
    m_pathDropPending = false;
    m_beginInit = INIT_OK;

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
    m_displayTypeFromIni = false;   // the source document's value, not the INI's
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
    CommitPageSettings();
}

// GetPropertiesSheetValues, with the result of the Init a changed setting runs
// (INIT_OK when nothing needed one).
CArgyllSensor::InitResult CArgyllSensor::CommitPageSettings()
{
    COneDeviceSensor::GetPropertiesSheetValues();
    // An Init in this sheet failed and let go of the meter: there is nothing to
    // commit the device settings to (and the registry keys are per meter name).
    // Settings the user changed since are not kept, and the user is told so.
    if ( m_meter == NULL )
    {
        const CArgyllSensorPropPage& pg = m_ArgyllSensorPropertiesPage;
        if ( m_ReadingType != (UINT)pg.m_ReadingType || m_SpectralType != pg.m_SpectralType
          || ( pg.m_DisplayType >= 0 && m_DisplayType != (UINT)pg.m_DisplayType ) || m_HiRes != pg.m_HiRes
          || m_intTime != (UINT)pg.m_intTime || m_Adapt != pg.m_Adapt || m_DisableAIO != pg.m_DisableAIO )
        {
            CString msg, title;
            msg.LoadString ( IDS_ARGYLL_SETTINGS_NOT_KEPT );
            title.LoadString ( IDS_ARGYLLSENSOR_PROPERTIES_TITLE );
            GetColorApp()->InMeasureMessageBox ( msg, title, MB_OK | MB_ICONWARNING );
        }
        return INIT_FAILED_SILENT;
    }
    InitResult result = INIT_OK;
    std::string meterName(m_meter->getMeterName());

    // An empty Display Type combo (an i1d3 with its diffuser over the sensor lists
    // no types) reads back as CB_ERR, -1. That is "nothing to choose from", not a
    // choice: keep the type the sensor has instead of storing -1, which Init then
    // skips and which reached the INI and every later session.
    if ( m_ArgyllSensorPropertiesPage.m_DisplayType < 0 )
        m_ArgyllSensorPropertiesPage.m_DisplayType = m_DisplayType;

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
        // A Display Type chosen in the sheet: the Init below may drop a Display Type
        // correction with it (SyncSpectralRecordFromMeter); a load never does.
        const bool displayTypeChanged = ( m_DisplayType != m_ArgyllSensorPropertiesPage.m_DisplayType );
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
        m_displayTypeChangedInSheet = displayTypeChanged;
        result = InitReporting(TRUE);
        m_displayTypeChangedInSheet = false;
    }
    return result;
}

BOOL CArgyllSensor::Init( BOOL bForSimultaneousMeasures )
{
    return InitReporting(bForSimultaneousMeasures) == INIT_OK;
}

// Init, saying what happened: INIT_OK; INIT_FAILED_ANNOUNCED, a failure the user
// has been told about (by the wrapper's own box or by HCFR's, once); or
// INIT_FAILED_SILENT, no meter to start and nothing shown - the caller says so.
CArgyllSensor::InitResult CArgyllSensor::InitReporting( BOOL bForSimultaneousMeasures )
{
    // No meter: a failed Init before this one let go of it (below). Nothing to
    // start, and every call past here would dereference it.
    if ( m_meter == NULL )
        return INIT_FAILED_SILENT;
    // The wrapper is shared by every document on the meter, and a failed start in
    // any of them deletes its driver (connectAndStartMeter, every failure path): the
    // wrapper then holds no driver, and this sensor has no meter either.
    if ( !m_meter->isMeterStillValid() )
    {
        m_meter = 0;
        return INIT_FAILED_SILENT;
    }
    m_meter->clearErrorBoxShown();
    // A std::logic_error from the wrapper or the driver (a Display Type select that
    // fails: on the DTP94 it is a device command) is a failed Init: shown once, and
    // announced. The sensor keeps its meter: the driver started and stays usable.
    // No caller - an MFC command handler, the sheet's BeginConfigure, a measurement
    // start - catches it, and uncaught it terminates the application.
    try
    {
        return InitBody(bForSimultaneousMeasures) ? INIT_OK : INIT_FAILED_ANNOUNCED;
    }
    catch ( std::logic_error & e )
    {
        ShowUnlessWrapperDid(m_meter, e.what(), "Argyll Meter");
        return INIT_FAILED_ANNOUNCED;
    }
}

BOOL CArgyllSensor::InitBody( BOOL bForSimultaneousMeasures )
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
        ShowUnlessWrapperDid(m_meter, errorDescription.c_str(), "Argyll Meter");
        // Every failure path of the start deletes the wrapper's driver.
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
    ApplyCorrectionStep();

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

// Init's correction step: everything Init does to the spectral correction state
// once the driver is started - the Display Type repair and lookup, the conversion
// for a meter whose matrices are not exposed, the path rule, the re-apply,
// SyncSpectralRecordFromMeter, EnforceRecordApplied, and the booking. Init runs it
// at every measurement start and at the sensor sheet's BeginConfigure, before the
// snapshot and the page fill - so the sheet starts from the state a measurement
// would have left, and its walk and Cancel see only what the user changes. A
// change it makes outside the sheet has no walk after it and is booked for the
// raw-less readings at the end (EffectiveChangedWithoutWalk); inside the sheet the
// sheet accounts for it. Its Display Type select is a device command on some meters
// (the DTP94's set_disp_type writes to the instrument, dtp92.c); a failure there
// throws, and Init turns it into a failed Init.
void CArgyllSensor::ApplyCorrectionStep()
{
    // A -1 display type (stored by builds that let an empty combo through, in the
    // INI or in a document) means none was ever chosen: use the driver's default
    // rather than skipping setDisplayType, which left whatever the driver last
    // had - a cleared ccss included - in place. An index past the end of the
    // current list is the same case: the list includes installed ccss/ccmx
    // entries, so a stored index goes stale when files are removed or a .chc
    // comes from a machine with more of them, and setDisplayType ignores it
    // silently (ArgyllMeterWrapper.cpp) - leaving whatever another document on
    // the same meter selected live. With no list (count 0) nothing can be checked.
    // The default goes back to the INI only when the value came from the INI:
    // a document's stored type (.chc, Copy) must not replace the type the user
    // chose for new documents.
    {
        int nDisplayTypes = 0;
        try
        {
            nDisplayTypes = m_meter->getNumberOfDisplayTypes();
        }
        catch ( std::logic_error & ) {}
        if ( m_DisplayType == 0xFFFFFFFF
          || ( nDisplayTypes > 0 && m_DisplayType >= (UINT)nDisplayTypes ) )
        {
            try
            {
                m_DisplayType = m_meter->getDefaultDisplayType();
                if ( m_displayTypeFromIni )
                    GetConfig()->WriteProfileInt(m_meter->getMeterName().c_str(), "DisplayType", m_DisplayType );
            }
            catch ( std::logic_error & ) {}
        }
    }

    const Matrix effectiveBefore = GetEffectiveMatrix();
    const bool throughDriver = ReadingsCarryUnrecordedDriverCorrection(m_spectral, GetFitBasis());
    // What the readings so far went through inside the driver (EnforceRecordApplied
    // must not forget it if it removes the record).
    const bool driverAppliedRecord = m_spectral.IsPresent() && m_spectral.basis != FIT_BASIS_NATIVE;

    // A Display Type record is anchored to its entry's TEXT, not to the persisted
    // index: Argyll's list numbers installed ccss/ccmx files in the order it finds
    // them, so one file added or removed shifts every index after it, and the stored
    // index can name another entry. Unless the user chose the Display Type in the
    // sheet (that choice replaces the record), the entry is looked up by the
    // record's text. Found: m_DisplayType follows it. Gone: the record keeps
    // applying, the driver measures on a plain entry, and Sync leaves the Display
    // Type alone (m_displayTypeRecordEntryMissing). Either way the member follows
    // and the registry only when the value came from it (m_displayTypeFromIni): a
    // document's stored Display Type never rewrites the one the user chose for
    // new documents, the same rule as the -1 repair above.
    m_displayTypeRecordEntryMissing = false;
    int plainEntry = -1;
    if ( m_meter->canExposeCalibrationMatrices() && m_spectral.source == SPECTRAL_SOURCE_DISPLAY_TYPE
      && !m_displayTypeChangedInSheet && m_DisplayType != 0xFFFFFFFF
      && DisplayTypeText(m_DisplayType) != m_spectral.description )
    {
        const int ix = FindDisplayTypeByText(m_spectral.description);
        if ( ix >= 0 )
        {
            m_DisplayType = (UINT)ix;
            m_dtBaseIndex = -1;     // the base belongs to the entry it was derived for
            if ( m_displayTypeFromIni )
                GetConfig()->WriteProfileInt(m_meter->getMeterName().c_str(), "DisplayType", m_DisplayType );
            SetModifiedFlag(TRUE);
        }
        else
        {
            m_displayTypeRecordEntryMissing = true;
            // The stored entry when it is plain (the user's own choice); else the
            // first entry carrying neither a ccss nor a ccmx. The wrapper exposes no
            // inst_dtflags_default test, but inst_creat_disptype_list copies the
            // driver's static table first, and the i1d3's default ("Non-Refresh
            // display", inst_dtflags_default) is entry 0 of it - so this is entry 0.
            // A stored index past the end of the list (the list shrank when the file
            // went) names nothing and is not plain.
            plainEntry = FindPlainDisplayType();
            // The Display Type follows the driver, so the page shows the entry the
            // readings are taken on, not the stale index, and so does the next session
            // when the value came from the INI (see above). The record is still
            // anchored to its text: should the file come back, the lookup above finds
            // it again.
            if ( plainEntry >= 0 && (UINT)plainEntry != m_DisplayType )
            {
                m_DisplayType = (UINT)plainEntry;
                if ( m_displayTypeFromIni )
                    GetConfig()->WriteProfileInt(m_meter->getMeterName().c_str(), "DisplayType", m_DisplayType );
                SetModifiedFlag(TRUE);
            }
            NoticeLoadedDisplayTypeOnce ( m_spectral.description );
        }
    }

    if(m_DisplayType != 0xFFFFFFFF)
    {
        // A ccss/ccmx entry whose correction is already recorded for this unit and
        // observer is not derived again (SyncSpectralRecordFromMeter skips it): select
        // the base entry that derive measured on directly, so the driver measures
        // native. Otherwise the user's entry; a corrected one is derived below and
        // the derive leaves its base selected.
        if ( m_displayTypeRecordEntryMissing )
        {
            if ( plainEntry >= 0 )
                m_meter->setDisplayType(plainEntry);
        }
        else
            m_meter->setDisplayType( DisplayTypeRecordIsCurrent() ? m_dtBaseIndex : (int)m_DisplayType );
    }

    // A record derived on a meter whose matrices are exposed (basis NATIVE: HCFR
    // applies its correction to native readings) met by a meter whose matrices are
    // not - a document from an i1d3 opened with a Spyder. This driver applies a
    // correction itself (the path's sample, re-applied below, or the corrected entry
    // selected above in its own Display Type list), so HCFR applying the record's
    // correction as well would correct twice, and those matrices belong to another
    // probe's curves anyway. Record it the way ApplySpectralCorrection does for such
    // a meter: identity matrices, driver-corrected basis. Whether the driver then
    // applies this record's own correction - its file's sample, or its own entry at
    // the selected index - is EnforceRecordApplied's question at the end of this
    // step (SpectralRecordApplied); a record it does not apply is removed there.
    if ( !m_meter->canExposeCalibrationMatrices() && m_spectral.IsPresent()
      && m_spectral.basis == FIT_BASIS_NATIVE )
    {
        SpectralCorrectionRecord rec = m_spectral;
        rec.nativeCal = Matrix::IdentityMatrix(3);
        rec.currentCal = Matrix::IdentityMatrix(3);
        rec.correction = Matrix::IdentityMatrix(3);
        rec.basis = FIT_BASIS_DRIVER_CORRECTED;
        SetSpectralRecord(rec);
        if ( UsesSpectralMethod() )
        {
            SetSensorMatrix(Matrix::IdentityMatrix(3));
            SetFitBasis(FIT_BASIS_DRIVER_CORRECTED);
        }
        SetModifiedFlag(TRUE);
    }

    // The path rule (SpectralRecordFromPathFile), checked once whenever the path or
    // the record changed - a correction file or a document loaded after the Browse
    // replaces the record and leaves the path. The file is checked again wherever its
    // bytes are read to be used (the re-apply below, Sync's re-derive).
    CheckPathAgainstRecord();

    // Re-apply a stored sample to a driver that holds one (ReapplyPathSample).
    if ( !m_spectralCorrectionPath.IsEmpty() && !m_meter->canExposeCalibrationMatrices() )
        ReapplyPathSample();
    SyncSpectralRecordFromMeter();
    EnforceRecordApplied(driverAppliedRecord);
    if ( !InSensorSheet() )
        CSensor::EffectiveChangedWithoutWalk(effectiveBefore, throughDriver);
}

// Re-apply a loaded spectral (ccss/CSV) correction so it survives reconnects
// and restarts (restores the apply path dropped in 2014), for a driver that holds
// the sample itself (its matrices are not exposed). A failure must not pass
// silently: warn once per session and leave the state for the user to re-apply
// or clear in the sensor properties. Startup itself is never blocked.
// The file is read once per path (cached: Init runs per sweep and per single
// measurement; Browse, Clear and a path change invalidate the cache), and the
// bytes read are the bytes checked against the record (KeepPathIfRecordFile) and
// parsed: the driver is never given a sample from another file under this record.
// Also CancelConfigure's way back to the snapshot's path.
void CArgyllSensor::ReapplyPathSample()
{
    bool applied = false;
    bool dropped = false;
    try
    {
        if ( m_meter->doesMeterSupportSpectralSamples() )
        {
            bool ok = m_spectralCacheValid
                   && m_spectralCachePath == m_spectralCorrectionPath;
            if ( !ok )
            {
                SpectralSample ss;
                std::string why;
                bool hashed = false;
                unsigned long long hash = 0;
                const bool parsed = ReadPathFile(m_spectralCorrectionPath, ss, hashed, hash, why);
                if ( !KeepPathIfRecordFile(hashed, hash) )
                    dropped = true;
                else if ( parsed )
                {
                    m_spectralSampleCache = ss;
                    m_spectralCachePath = m_spectralCorrectionPath;
                    m_spectralCacheHash = hash;
                    m_spectralCacheValid = true;
                    ok = true;
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
    if ( dropped )
    {
        // DropPathNotRecordFile left the notice to EnforceRecordApplied, which
        // removes the record that nothing applies now and says so once.
    }
    else if ( !applied )
    {
        WarnSpectralReapplyFailedOnce ( m_spectralCorrectionPath );
    }
    else if ( m_spectral.source != SPECTRAL_SOURCE_FILE )
    {
        // A sample loaded by a build that kept no record: record it now, as
        // ApplySpectralCorrection would have (the driver applies it; identity
        // matrices, driver-corrected basis).
        SpectralCorrectionRecord rec;
        MakeFileRecord(m_spectralCorrectionPath, &m_spectralSampleCache, rec, &m_spectralCacheHash);
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

    // Read (and validate) the file first, so we don't prompt then fail. One read:
    // the record's hash is the hash of the bytes parsed (ReadPathFile).
    SpectralSample ss;
    unsigned long long fileHash = 0;
    bool fileHashed = false;
    {
        std::string why;
        if ( !ReadPathFile(filePath, ss, fileHashed, fileHash, why) )
        {
            CString sTitle; sTitle.LoadString( IDS_SPECTRAL_TITLE );
            GetColorApp()->InMeasureMessageBox( why.c_str(), sTitle, MB_OK+MB_ICONHAND );
            return false;
        }
    }

    // The spectral correction replaces a matrix calibration for the values shown.
    // Nothing is lost: every reading keeps its raw (native) value, and the
    // document's recompute from raw follows on the sheet's OK. Cancel aborts the
    // apply entirely (so it really is "before applying").
    // A meter whose matrices are not exposed is told what happens there instead:
    // its driver applies the sample to new readings only, and the recompute turns
    // every stored reading into identity * raw, its raw being the uncorrected
    // reading - the matrix calibration is gone from it and the spectral correction
    // never reaches it (IDS_SPECTRAL_MATRIXCAL_PROMPT_DRIVER).
    const bool exposable = ( m_meter && m_meter->canExposeCalibrationMatrices() );
    if ( IsCorrectionActive() && !UsesSpectralMethod() )
    {
        CString prompt, title;
        prompt.LoadString ( exposable ? IDS_SPECTRAL_MATRIXCAL_PROMPT : IDS_SPECTRAL_MATRIXCAL_PROMPT_DRIVER );
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
    Matrix native, current, corr;
    if ( exposable )
    {
        // HCFR applies the correction to the native readings the driver reports.
        Matrix toNative;      // unused: the driver is native
        std::string why;
        if ( !m_meter->deriveSpectralCalibration(ss, native, current, why)
          || !DeriveSpectralCorrection(native, current, true, corr, toNative, why) )
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
    m_spectralCorrectionPath = filePath;
    // Show just the panel name in the UI (the derived tech still goes into the
    // ccss metadata); getDescription() would nest the tech in parentheses.
    m_spectralCorrectionDesc = ss.getDisplay();

    // The record. A second Browse simply overwrites it: each derive starts and ends
    // with the driver native (and a driver-applied sample is replaced, not added
    // to), so corrections cannot stack.
    SpectralCorrectionRecord record;
    MakeFileRecord(filePath, &ss, record, fileHashed ? &fileHash : NULL);
    if ( exposable )
    {
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
    // The bytes just read are the path's sample and its hash: the next Init's path
    // rule and re-apply use them instead of reading the file twice more.
    m_spectralSampleCache = ss;
    m_spectralCachePath = filePath;
    m_spectralCacheHash = fileHash;
    m_spectralCacheValid = fileHashed;
    m_pathCheckedPath = filePath;
    m_pathCheckedHash = fileHash;
    m_pathCheckedValid = fileHashed;
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

// One read of the file at `path`: the bytes are hashed (hashed/hash: the file could
// be read) and those same bytes are parsed (the return value): a correlation .csv
// or a .ccss, by extension. The path rule (KeepPathIfRecordFile) is then applied to the
// hash of exactly what is used.
bool CArgyllSensor::ReadPathFile(const CString& path, SpectralSample& ss, bool& hashed,
                                 unsigned long long& hash, std::string& why)
{
    why.clear();
    hashed = false;
    hash = 0;
    std::vector<unsigned char> bytes;
    FILE *f = fopen((LPCSTR)path, "rb");
    if ( f == NULL )
    {
        why = "Could not open the spectral correction file.";
        return false;
    }
    unsigned char chunk[65536];
    size_t n;
    while ( ( n = fread(chunk, 1, sizeof(chunk), f) ) > 0 )
        bytes.insert(bytes.end(), chunk, chunk + n);
    const bool readError = ( ferror(f) != 0 );
    fclose(f);
    if ( readError )
    {
        why = "Could not read the spectral correction file.";
        return false;
    }
    hashed = true;
    hash = Fnv1a64(bytes.empty() ? NULL : &bytes[0], bytes.size());
    try
    {
        CString ext = path.Right(4); ext.MakeLower();
        bool ok = false;
        if ( ext == ".csv" )
            ok = ss.createFromCorrelationCSVText(std::string(bytes.begin(), bytes.end()), (LPCSTR)path);
        else
            ok = !bytes.empty() && ss.ReadBuffer(&bytes[0], bytes.size(), (LPCSTR)path);
        if ( !ok )
            why = "Could not read the selected spectral correction file.";
        return ok;
    }
    catch ( std::logic_error & e )
    {
        why = e.what();
        return false;
    }
}

void CArgyllSensor::MakeFileRecord(const CString& path, const SpectralSample* ss, SpectralCorrectionRecord& rec,
                                   const unsigned long long* knownHash)
{
    rec.source = SPECTRAL_SOURCE_FILE;
    rec.description.clear();
    if ( ss != NULL )
    {
        const char *display = ss->getDisplay();
        const char *descr = ss->getDescription();
        if ( display != NULL )
            rec.description = display;
        if ( rec.description.empty() && descr != NULL )
            rec.description = descr;
    }
    if ( rec.description.empty() )
        rec.description = (LPCSTR) m_spectralCorrectionDesc;
    unsigned long long hash = 0;
    if ( knownHash != NULL )
        hash = *knownHash;  // the caller hashed the bytes it just read
    else if ( !Fnv1a64File((LPCSTR)path, hash) )
        hash = 0;   // identifies the file only; not a reason to refuse it
    rec.fileHash = hash;
    rec.observer = m_meter ? RecordedObserver(m_meter) : -1;
    rec.meterSerial = m_meter ? m_meter->getSerialNumber() : std::string();
}

std::string CArgyllSensor::DisplayTypeText(UINT index)
{
    if ( m_meter == NULL || index == 0xFFFFFFFF )
        return std::string();
    try
    {
        const char *text = m_meter->getDisplayTypeText((int)index);
        return text != NULL ? std::string(text) : std::string();
    }
    catch ( std::logic_error & )
    {
        return std::string();
    }
}

int CArgyllSensor::FindDisplayTypeByText(const std::string& text)
{
    if ( m_meter == NULL || text.empty() )
        return -1;
    int n = 0;
    try { n = m_meter->getNumberOfDisplayTypes(); }
    catch ( std::logic_error & ) { return -1; }
    for ( int i = 0; i < n; ++i )
        if ( DisplayTypeText((UINT)i) == text )
            return i;
    return -1;
}

int CArgyllSensor::FindPlainDisplayType()
{
    if ( m_meter == NULL )
        return -1;
    int n = 0;
    try { n = m_meter->getNumberOfDisplayTypes(); }
    catch ( std::logic_error & ) { return -1; }
    std::vector<bool> corrected( n > 0 ? n : 0 );
    for ( int i = 0; i < n; ++i )
        corrected[i] = m_meter->isDisplayTypeCorrected(i);
    const int stored = ( m_DisplayType == 0xFFFFFFFF ) ? -1 : (int)m_DisplayType;
    return PlainDisplayTypeEntry(stored, corrected);
}

bool CArgyllSensor::SpectralClearAllowedOnPage()
{
    // The selected Display Type governs a Display Type record on any meter: on an
    // i1d3 Sync derives it from the entry, and on a meter whose matrices are not
    // exposed the driver applies the entry and EnforceRecordApplied removes the
    // record once another is chosen. The record's own entry is the selected one:
    // only then does choosing another Display Type remove it (an empty list, a
    // vanished entry, or a record loaded onto a plain type leave nothing to choose
    // that is a change).
    const bool listResyncs = ( m_meter != NULL );
    bool entrySelected = false;
    if ( listResyncs && m_spectral.source == SPECTRAL_SOURCE_DISPLAY_TYPE && m_DisplayType != 0xFFFFFFFF )
    {
        try
        {
            entrySelected = m_meter->isDisplayTypeCorrected((int)m_DisplayType)
                         && DisplayTypeText(m_DisplayType) == m_spectral.description;
        }
        catch ( std::logic_error & ) {}
    }
    return SpectralClearAllowed(m_spectral, !m_spectralCorrectionPath.IsEmpty(), listResyncs, entrySelected);
}

// The path rule (SpectralRecordFromPathFile) against bytes just hashed from the
// stored path. Kept: true, and the pair (path, hash) is remembered so the
// change-only check does not hash the same file again. Not the record's file: the
// path is dropped (DropPathNotRecordFile) and false.
bool CArgyllSensor::KeepPathIfRecordFile(bool hashed, unsigned long long hash)
{
    if ( SpectralRecordFromPathFile(m_spectral, hashed, hash) )
    {
        if ( hashed )
        {
            m_pathCheckedPath = m_spectralCorrectionPath;
            m_pathCheckedHash = hash;
            m_pathCheckedValid = true;
        }
        return true;
    }
    DropPathNotRecordFile();
    return false;
}

// The rule once per (path, record) pair: a FILE record with a known hash whose
// path was not yet checked against it. Hashes the file then, and only then.
bool CArgyllSensor::CheckPathAgainstRecord()
{
    if ( m_spectralCorrectionPath.IsEmpty() || m_spectral.source != SPECTRAL_SOURCE_FILE || m_spectral.fileHash == 0 )
        return true;
    if ( m_pathCheckedValid && m_pathCheckedPath == m_spectralCorrectionPath && m_pathCheckedHash == m_spectral.fileHash )
        return true;
    unsigned long long hash = 0;
    const bool hashed = Fnv1a64File((LPCSTR)m_spectralCorrectionPath, hash);
    return KeepPathIfRecordFile(hashed, hash);
}

// The stored path names another file than the record's: the record is what the
// readings went through and stands as far as anything applies it (on an i1d3 HCFR
// applies its matrices; on a meter that needs the driver to hold the sample nothing
// does, and EnforceRecordApplied removes it at the end of Init's correction step).
// The path, which no longer names the correction in use, is dropped, with whatever
// sample the driver holds from it. The one drop, for Sync and the re-apply alike.
void CArgyllSensor::DropPathNotRecordFile()
{
    SpectralSyncLog("CArgyllSensor: %s is not the file the spectral correction record (%s) was made from; the path is dropped",
                    (LPCSTR)m_spectralCorrectionPath, m_spectral.description.c_str());
    // Told at the end of the step (EnforceRecordApplied), in one message with what
    // becomes of the record.
    m_pathDropPending = true;
    m_pathDroppedPath = m_spectralCorrectionPath;
    m_spectralCorrectionPath.Empty();
    m_spectralCorrectionDesc.Empty();
    m_spectralCacheValid = false;
    m_pathCheckedValid = false;
    if ( !m_meter->canExposeCalibrationMatrices() )
    {
        try { m_meter->resetSpectralSample(); }
        catch ( std::logic_error & ) {}
    }
    SetModifiedFlag(TRUE);
}

// The second invariant (SpectralRecordApplied): a record never says a correction
// is in force when nothing applies one. This is the one place that enforces it, at
// the end of Init's correction step, which every record transition a meter makes
// goes through (the conversion for a meter whose matrices are not exposed, a path
// dropped by the path rule, Sync) and which every record that arrives by a load
// reaches before the first reading. A record that fails it is removed - with it
// the method when the record was the method (CSensor::ClearSpectralCorrection) - so
// the record, the status line and MatrixCalibrationAllowed all say what is true:
// nothing corrects the readings. The stored path, if any, is left for Sync to derive
// from again. The readings already taken keep their values (the bookkeeping after
// this step strands the raw-less ones when the effective matrix moved).
void CArgyllSensor::EnforceRecordApplied(bool driverAppliedRecord)
{
    const bool exposed = m_meter->canExposeCalibrationMatrices();
    const bool holdsSamples = MeterSupportsSpectralSamples();
    const bool hasPath = !m_spectralCorrectionPath.IsEmpty();
    // A meter whose matrices are not exposed applies a corrected Display Type entry
    // selected in its own list itself (Init's setDisplayType; Sync runs only for an
    // exposed meter): its text, or "" when the selection is plain.
    std::string driverEntry;
    if ( !exposed && m_DisplayType != 0xFFFFFFFF )
    {
        try
        {
            if ( m_meter->isDisplayTypeCorrected((int)m_DisplayType) )
                driverEntry = DisplayTypeText(m_DisplayType);
        }
        catch ( std::logic_error & ) {}
    }
    const bool pathDropped = m_pathDropPending;
    const CString droppedPath = m_pathDroppedPath;
    m_pathDropPending = false;
    m_pathDroppedPath.Empty();
    if ( SpectralRecordApplied(m_spectral, exposed, holdsSamples, hasPath, driverEntry) )
    {
        if ( pathDropped )
            NoticePathNotRecordFileOnce ( droppedPath, m_spectral.description );
        return;
    }
    const std::string description = m_spectral.description;
    const bool wasMethod = UsesSpectralMethod();
    SpectralSyncLog("CArgyllSensor: spectral correction %s is applied by nothing on this meter; removed",
                    description.c_str());
    CSensor::ClearSpectralCorrection();
    // Removing a record never forgets a correction the readings went through. A
    // record the driver applied (basis not NATIVE when this step began) left that
    // correction in every reading taken under it, raw values included; with the
    // record gone, ReadingsCarryUnrecordedDriverCorrection must still say so, and
    // that is the fit basis (the base Clear reset it to NATIVE with the method).
    if ( driverAppliedRecord && wasMethod )
        SetFitBasis(FIT_BASIS_DRIVER_CORRECTED);
    UINT id = IDS_SPECTRAL_RECORD_NOT_APPLIED;
    CString arg2;
    if ( !exposed && !holdsSamples )
        id = IDS_SPECTRAL_RECORD_NOT_APPLIED_NOSAMPLES;
    else if ( !driverEntry.empty() )
    {
        id = IDS_SPECTRAL_RECORD_NOT_APPLIED_ENTRY;
        arg2 = driverEntry.c_str();
    }
    else if ( pathDropped )
    {
        id = IDS_SPECTRAL_PATH_NOT_RECORD_FILE_REMOVED;
        arg2 = droppedPath;
    }
    WarnRecordNotAppliedOnce ( id, description, arg2 );
}

bool CArgyllSensor::DisplayTypeRecordIsCurrent()
{
    if ( m_meter == NULL || !m_meter->canExposeCalibrationMatrices() || m_DisplayType == 0xFFFFFFFF )
        return false;
    if ( m_spectral.source != SPECTRAL_SOURCE_DISPLAY_TYPE || m_dtBaseIndex < 0 || m_dtBaseFor != m_DisplayType )
        return false;
    if ( !m_meter->isDisplayTypeCorrected((int)m_DisplayType) )
        return false;
    return m_spectral.description == DisplayTypeText(m_DisplayType)
        && m_spectral.meterSerial == m_meter->getSerialNumber()
        && m_spectral.observer == RecordedObserver(m_meter);
}

bool CArgyllSensor::RefuseSpectralBrowse(UINT displayType)
{
    bool corrected = false;
    if ( m_meter != NULL && displayType != 0xFFFFFFFF )
    {
        try { corrected = m_meter->isDisplayTypeCorrected((int)displayType); }
        catch ( std::logic_error & ) {}
    }
    if ( !corrected )
        return false;
    CString msg, title;
    msg.LoadString ( IDS_SPECTRAL_BROWSE_REFUSED_DISPTYPE );
    title.LoadString ( IDS_SPECTRAL_TITLE );
    GetColorApp()->InMeasureMessageBox ( msg, title, MB_OK | MB_ICONINFORMATION );
    return true;
}

// Runs at the end of every Init (per sweep and per single measurement), for a
// meter whose calibration matrices are exposed (the i1d3 family) only. The driver
// holds no spectral state while HCFR measures, so nothing is checked against it:
// a ccss/ccmx Display Type entry is derived and the driver left on the entry's
// base (native), and a browsed sample is derived again only when the connected
// unit or observer is not the one the record was derived for. Shows no dialog but
// the once-per-session notices, and Browse's question when a ccss/ccmx Display Type
// entry is chosen in the sheet over a matrix calibration (see the Display Type
// branch). Readings already taken keep their values. What Sync changes by itself
// happens at a measurement start or at the sheet's BeginConfigure, both outside the
// sheet, and Init books it for the raw-less readings; what it does in the sheet
// follows a setting the user changed there, and the sheet's OK recomputes the
// readings from raw for it (OnConfigureSensor's CorrectionChangedSinceBeginConfigure
// sees the record or method change) or its Cancel restores the snapshot.
void CArgyllSensor::SyncSpectralRecordFromMeter()
{
    if ( m_meter == NULL || !m_meter->canExposeCalibrationMatrices() )
        return;
    // The record's Display Type entry is gone from the list (Init): nothing to
    // derive it from, and the entry at the stale index is another one. It stays.
    if ( m_displayTypeRecordEntryMissing )
        return;

    const int observer = RecordedObserver(m_meter);
    const std::string serial = m_meter->getSerialNumber();

    if ( m_DisplayType != 0xFFFFFFFF && m_meter->isDisplayTypeCorrected((int)m_DisplayType) )
    {
        // The Display Type entry carries a correction (a ccss or ccmx entry). Its
        // matrix is read out of the driver and the driver measures on the base
        // entry; m_DisplayType keeps the user's choice (persisted, shown in the page).
        // Already recorded for this entry, unit and observer: Init has selected the
        // base entry directly, and a derive would produce the same record again.
        if ( DisplayTypeRecordIsCurrent() )
            return;

        const std::string entryText = DisplayTypeText(m_DisplayType);
        Matrix native, current, corr, toNative;
        std::string why;
        int baseIndex = -1;
        if ( !m_meter->deriveDisplayTypeCalibration((int)m_DisplayType, baseIndex, native, current, why)
          || !DeriveSpectralCorrection(native, current, true, corr, toNative, why) )
        {
            SpectralSyncLog("CArgyllSensor: Display Type %d correction not derived: %s", (int)m_DisplayType, why.c_str());
            // The driver measures native on every exit path (R5). Past its list
            // checks the derive has left the entry's base selected (baseIndex is
            // set), on success and failure alike. Before them (the entry is not in
            // the driver's list, or has no base entry) it selected nothing, and the
            // driver is still on the corrected entry Init selected, applying its
            // ccss/ccmx itself: under a record for the same entry HCFR would apply it
            // a second time, and a fit would be stamped NATIVE on corrected readings.
            // A plain entry is selected instead (the i1d3 list always starts with the
            // driver's plain built-ins, so there is one).
            bool measuresNative = ( baseIndex >= 0 );
            if ( baseIndex < 0 )
            {
                const int plain = FindPlainDisplayType();
                if ( plain >= 0 )
                {
                    m_meter->setDisplayType(plain);
                    measuresNative = true;
                }
            }
            // Another entry's correction must not stay on this entry's readings: a
            // Display Type record for a different entry is removed, and the user is
            // told why once per session. A browsed file's record stays as it is, and
            // so does one for this same entry (HCFR applies it, once). With no record
            // nothing corrects the readings, and the user is told that instead. Each
            // notice has its own once-per-session flag: one shown must not silence
            // the other, which says something different.
            static BOOL s_warnedDisplayTypeFailed = FALSE;
            static BOOL s_warnedDisplayTypeNotApplied = FALSE;
            UINT warning = 0;
            BOOL * warned = NULL;
            if ( m_spectral.source == SPECTRAL_SOURCE_DISPLAY_TYPE && m_spectral.description != entryText )
            {
                CSensor::ClearSpectralCorrection();
                warning = IDS_SPECTRAL_DISPTYPE_FAILED;
                warned = &s_warnedDisplayTypeFailed;
            }
            else if ( !m_spectral.IsPresent() && measuresNative )
            {
                warning = IDS_SPECTRAL_DISPTYPE_NOT_APPLIED;
                warned = &s_warnedDisplayTypeNotApplied;
            }
            if ( warned != NULL && !*warned )
            {
                *warned = TRUE;
                CString msg, fmt, title;
                fmt.LoadString ( warning );
                title.LoadString ( IDS_SPECTRAL_TITLE );
                msg.Format ( fmt, why.c_str() );
                GetColorApp()->InMeasureMessageBox ( msg, title, MB_OK | MB_ICONWARNING );
            }
            return;
        }
        // No setDisplayType(baseIndex) here: deriveDisplayTypeCalibration has already
        // selected the base entry (set_disptype(base), m_displayType = base) on every
        // path past its list checks. Remember it, so the next Init selects it directly.
        m_dtBaseIndex = baseIndex;
        m_dtBaseFor = m_DisplayType;

        SpectralCorrectionRecord rec;
        rec.source = SPECTRAL_SOURCE_DISPLAY_TYPE;
        rec.description = entryText;
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
            // A matrix calibration in force (a fit, a correction file, Bodner) and
            // the entry chosen in the sheet just now: Browse's question over a
            // matrix (ApplySpectralCorrection), with its outcomes. OK makes the
            // entry's correction the method, the Bodner sub-gamut matrices dropped,
            // and the sheet's OK recomputes the readings from raw. Cancel keeps the
            // matrix calibration and the values shown; the entry's correction is
            // recorded, not applied (the status line says so), which is what
            // matches the entry the driver now measures on. Any other derive (a
            // document's entry, another unit) asks nothing and records it the same
            // way: Init runs at every measurement start.
            // Chosen with no correction live, it becomes the method unasked, as a
            // Browse does.
            bool makeMethod = noFit;
            if ( !UsesSpectralMethod() && m_displayTypeChangedInSheet )
            {
                makeMethod = true;
                if ( IsCorrectionActive() )
                {
                    CString prompt, title;
                    prompt.LoadString ( IDS_SPECTRAL_MATRIXCAL_PROMPT );
                    title.LoadString ( IDS_SPECTRAL_TITLE );
                    makeMethod = ( GetColorApp()->InMeasureMessageBox( prompt, title,
                        MB_OKCANCEL | MB_ICONQUESTION ) == IDOK );
                }
            }
            SetSpectralRecord(rec);
            if ( UsesSpectralMethod() )
            {
                SetSensorMatrix(corr);      // another entry while it already is the method
                SetFitBasis(FIT_BASIS_NATIVE);
            }
            else if ( makeMethod )
            {
                if ( !noFit )
                    ClearBodnerMatrices();  // as Browse: the method's matrix is the correction
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
        if ( m_displayTypeChangedInSheet )
        {
            // The user chose a plain Display Type in the sheet: the entry's
            // correction goes with the entry.
            CSensor::ClearSpectralCorrection();
        }
        else if ( m_spectralCorrectionPath.IsEmpty() )
        {
            // A Display Type record that arrived with a document or a correction file
            // while the local Display Type is plain: there is no entry here to derive
            // it from, and dropping it would silently change what the document's
            // readings went through. It stays and applies until the Display Type is
            // changed in the sheet; the user is told once per session.
            NoticeLoadedDisplayTypeOnce ( m_spectral.description );
            return;
        }
    }

    if ( m_spectralCorrectionPath.IsEmpty() )
    {
        // A FILE record that came with a document or a correction file, with no
        // file here to derive it from: it keeps applying; if it was derived for
        // another unit, the user is told.
        NoticeForeignFileRecordOnce ( m_spectral, serial );
        return;
    }

    // A browsed sample. Its record holds the matrices; they belong to one unit and
    // one observer, so derive again only when either is not the connected one (a
    // record from a build that kept none, or from a Browse with no meter attached,
    // carries neither). A record whose source is not FILE (a document from an
    // earlier field-test build) is built the same way.
    const bool fileRecord = ( m_spectral.source == SPECTRAL_SOURCE_FILE );
    const bool serialChanged = ( m_spectral.meterSerial != serial );
    if ( fileRecord && !serialChanged && m_spectral.observer == observer )
    {
        // Complete for this unit: used without deriving it again. Say so once if the
        // file is gone, so a moved .ccss is not found out only on another meter.
        if ( ::GetFileAttributes((LPCSTR)m_spectralCorrectionPath) == INVALID_FILE_ATTRIBUTES )
            NoticeSpectralFileMovedOnce ( m_spectralCorrectionPath );
        return;
    }

    // The path rule on the bytes about to be derived (R4): a FILE record is derived
    // again only from the file it was made from. Another file at the path leaves the
    // record standing - from here on it is a FILE record with no file here, as
    // above, and another unit's is told so - and IDS_SPECTRAL_UNIT_MISMATCH below
    // ("derived again") is only ever shown for the record's own file.
    SpectralSample ss;
    Matrix native, current, corr, toNative;
    std::string why;
    unsigned long long pathHash = 0;
    bool hashed = false;
    const bool read = ReadPathFile(m_spectralCorrectionPath, ss, hashed, pathHash, why);
    if ( !KeepPathIfRecordFile(hashed, pathHash) )
        return;     // told at EnforceRecordApplied; the next Init's no-file branch tells the rest
    bool derived = false;
    if ( read )
    {
        try
        {
            derived = m_meter->deriveSpectralCalibration(ss, native, current, why)
                   && DeriveSpectralCorrection(native, current, true, corr, toNative, why);
        }
        catch ( std::logic_error & e )
        {
            why = e.what();
            derived = false;
        }
    }
    if ( !derived )
    {
        SpectralSyncLog("CArgyllSensor: spectral correction %s not derived again: %s",
                        (LPCSTR)m_spectralCorrectionPath, why.c_str());
        // A moved or unreadable file with no record left to apply: every reading is
        // uncorrected while the path still names a correction. Same once-per-session
        // warning as a meter that keeps the sample (Init's re-apply).
        if ( !read && !m_spectral.IsPresent() )
            WarnSpectralReapplyFailedOnce ( m_spectralCorrectionPath );
        // Another unit's FILE record and a file that cannot be read: it keeps
        // applying, and the user is told it belongs to another meter.
        else if ( !read )
            NoticeForeignFileRecordOnce ( m_spectral, serial );
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

    // Description and hash come from the file just read, never from the record it
    // replaces (which may have come from another file through a document).
    SpectralCorrectionRecord rec;
    MakeFileRecord(m_spectralCorrectionPath, &ss, rec, hashed ? &pathHash : NULL);
    rec.nativeCal = native;
    rec.currentCal = current;
    rec.correction = corr;
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

// The structural rule for order independence (#256). Init's correction step can
// change the correction by itself (a document's Display Type entry derived for the
// first time, a FILE record derived again for another unit or observer, the
// conversion for a meter whose matrices are not exposed, a stale path dropped).
// Outside the sheet nothing walks the readings after it, so Init books the change
// for the raw-less readings (EffectiveChangedWithoutWalk); inside the sheet, the
// sheet's OK would walk it as if the user had made it, and its Cancel and the page's
// Clear would undo it. So the measurement-start Init runs here, with that
// bookkeeping, before the snapshot and before the page is filled
// (SetPropertiesSheetValues, in Configure): the page, the snapshot and the walk
// baseline all hold its result, every sequence in the sheet starts from the state
// a measurement would have left, and the sheet's walk and Cancel see only the
// user's changes - the same result whichever came first, a measurement or the
// sheet. An Init in the sheet then changes the correction only for a setting the
// user changed there.
// This is the start the page itself would make: on main, filling the page already
// starts an unstarted meter (SetPropertiesSheetValues and the combo fill reach
// checkMeterIsInitialized -> connectAndStartMeter). Here it is the start a
// measurement makes, with this sensor's settings, and its failure is handled:
//   - the start fails and the sensor still holds its meter (a std::logic_error,
//     the DTP94 rejecting the stored Display Type: the driver started and stays
//     usable): the sheet OPENS with no correction step applied. That is what a
//     measurement in the same state does too - its Init fails at the same point -
//     so both orders start from the same state, and the sheet is the place to
//     change the setting that failed. The partial step a throw leaves behind is
//     the one a measurement's Init leaves (the step is deterministic and throws at
//     the same call): only the Display Type repair and the record's lookup by text
//     precede the first Display Type select, and neither moves the effective
//     matrix, so nothing is owed to the booking. The page fill starts nothing
//     more: the driver is initialized, and checkMeterIsInitialized finds it so.
//     The OK's commit Init runs with the setting the user changed.
//   - the sensor has no meter (none was found, a failed start deleted the
//     driver, another document's failed start deleted the shared one): the sheet
//     is refused with a message every time (Configure).
void CArgyllSensor::BeginConfigure()
{
    m_beginInit = InitReporting(FALSE);
    CSensor::BeginConfigure();
    m_cfgSnapSpectralPath = m_spectralCorrectionPath;
    m_cfgSnapSpectralDesc = m_spectralCorrectionDesc;
    m_cfgSnapDisplayType  = m_DisplayType;
    m_cfgSnapReadingType  = m_ReadingType;
    m_cfgSnapSpectralType = m_SpectralType;
    m_cfgSnapIntTime      = m_intTime;
    m_cfgSnapHiRes        = m_HiRes;
    m_cfgSnapAdapt        = m_Adapt;
    m_cfgSnapDisableAIO   = m_DisableAIO;
    m_cfgSnapDebugMode    = m_debugMode;
    m_cfgSnapDeviceValid  = true;
}

// The page reads the meter throughout (SetPropertiesSheetValues, the combo fills):
// with no meter there is no sheet to show, and the caller treats it as a Cancel.
// The user is told every time: by the start that just failed (INIT_FAILED_ANNOUNCED),
// or by this message whenever nothing was shown (HCFR detects meters once per
// session, so the remedy is a restart).
BOOL CArgyllSensor::Configure()
{
    if ( m_meter == NULL )
    {
        if ( m_beginInit != INIT_FAILED_ANNOUNCED )
        {
            CString msg, title;
            msg.LoadString ( IDS_ARGYLL_METER_UNAVAILABLE );
            title.LoadString ( IDS_ARGYLLSENSOR_PROPERTIES_TITLE );
            GetColorApp()->InMeasureMessageBox ( msg, title, MB_OK | MB_ICONWARNING );
        }
        return FALSE;
    }
    return COneDeviceSensor::Configure();
}

// A spectral apply (or clear) from the property page committed before the
// sheet closed; a cancelled sheet restores the previous state - members here,
// and the meter itself where connected. Errors pushing to the meter are
// non-fatal: the restored path is re-applied by Init() on every connect.
void CArgyllSensor::CancelConfigure()
{
    RestoreSnapshot(true);
}

// The page's Clear: back to the sheet's opening state, then everything is cleared,
// so the snapshot's sample is not put back into the driver first - its re-apply
// could only warn about a path the Clear discards, and use up that warning's
// once-per-session flag.
void CArgyllSensor::CancelConfigureBeforeClear()
{
    RestoreSnapshot(false);
}

void CArgyllSensor::RestoreSnapshot(bool reapplyDriverSample)
{
    // The snapshot's sample goes back into the driver the way Init's re-apply puts
    // it there (ReapplyPathSample, the path rule included), once the record it is
    // checked against is restored too (the base, below).
    bool restoreDriverSample = false;
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
        m_spectralCorrectionPath = m_cfgSnapSpectralPath;
        m_spectralCorrectionDesc = m_cfgSnapSpectralDesc;
        m_spectralCacheValid = false;
        restoreDriverSample = ( reapplyDriverSample && !m_cfgSnapSpectralPath.IsEmpty() && !exposable && m_meter != NULL );
    }

    // The device settings the page's Calibrate button committed mid-sheet
    // (GetPropertiesSheetValues: members and registry). Restored with the record,
    // or the next Init re-selects a cancelled Display Type and derives its record
    // with no recompute. Each registry key is written back only when it changed;
    // the driver picks the restored settings up at the next Init, as it would
    // after OK.
    if ( m_cfgSnapDeviceValid )
    {
        const bool haveMeter = ( m_meter != NULL );
        std::string meterName;
        if ( haveMeter )
            meterName = m_meter->getMeterName();
        if ( m_debugMode != m_cfgSnapDebugMode )
        {
            m_debugMode = m_cfgSnapDebugMode;
            if ( haveMeter )
                GetConfig()->WriteProfileInt(meterName.c_str(), "DebugMode", m_debugMode?1:0);
        }
        if ( m_ReadingType != m_cfgSnapReadingType )
        {
            m_ReadingType = m_cfgSnapReadingType;
            if ( haveMeter )
                GetConfig()->WriteProfileInt(meterName.c_str(), "ReadingType", m_ReadingType );
        }
        if ( m_SpectralType != m_cfgSnapSpectralType )
        {
            m_SpectralType = m_cfgSnapSpectralType;
            if ( haveMeter )
                GetConfig()->WriteProfileString(meterName.c_str(), "SpectralType", m_SpectralType );
        }
        if ( m_intTime != m_cfgSnapIntTime )
        {
            m_intTime = m_cfgSnapIntTime;
            if ( haveMeter )
                GetConfig()->WriteProfileInt(meterName.c_str(), "IntTime", m_intTime );
        }
        if ( m_DisplayType != m_cfgSnapDisplayType )
        {
            m_DisplayType = m_cfgSnapDisplayType;
            if ( haveMeter )
                GetConfig()->WriteProfileInt(meterName.c_str(), "DisplayType", m_DisplayType );
        }
        if ( m_HiRes != m_cfgSnapHiRes )
        {
            m_HiRes = m_cfgSnapHiRes;
            if ( haveMeter )
                GetConfig()->WriteProfileInt(meterName.c_str(), "HiRes", m_HiRes );
        }
        if ( m_Adapt != m_cfgSnapAdapt )
        {
            m_Adapt = m_cfgSnapAdapt;
            if ( haveMeter )
                GetConfig()->WriteProfileInt(meterName.c_str(), "Adapt", m_Adapt );
        }
        if ( m_DisableAIO != m_cfgSnapDisableAIO )
        {
            m_DisableAIO = m_cfgSnapDisableAIO;
            if ( haveMeter )
                GetConfig()->WriteProfileInt(meterName.c_str(), "DisableAIO", m_DisableAIO );
        }
    }

    // The base restores the matrices, the method, the record and the fit basis.
    CSensor::CancelConfigure();
    if ( restoreDriverSample )
    {
        const bool driverAppliedRecord = m_spectral.IsPresent() && m_spectral.basis != FIT_BASIS_NATIVE;
        ReapplyPathSample();
        // A path the rule dropped here leaves a record nothing applies: the one
        // place that removes it, as at the end of Init's correction step.
        EnforceRecordApplied(driverAppliedRecord);
    }
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
        // A calibration that fails ends the reading: retrying it would loop through
        // the same modal boxes for as long as it keeps failing, and a Calibrate whose
        // Init let go of the meter leaves nothing to read from.
        if(state == ArgyllMeterWrapper::NEEDS_MANUAL_CALIBRATION)
        {
            if ( !Calibrate() || m_meter == NULL )
                return noDataColor;
        }
        if(state == ArgyllMeterWrapper::INCORRECT_POSITION)
        {
            GetColorApp()->InMeasureMessageBox( m_meter->getIncorrectPositionInstructions().c_str(), "Incorrect Position", MB_OK+MB_ICONHAND);
        }
    }
    return m_meter->getLastReading();
}

bool CArgyllSensor::Calibrate()
{
    // No meter: an earlier Init failed, let go of it and said why (the page's
    // Calibrate button is disabled then). Nothing to calibrate.
    if ( m_meter == NULL )
        return false;
    // A failed Init has said why (Init, or the wrapper): no second message.
    if ( !Init(FALSE) )
        return false;
    // The wrapper's calibrate and getCalibrationInstructions throw on a device
    // failure; Calibrate runs from the page's button (an MFC command handler) and
    // from a measurement, neither of which catches it. Shown once.
    try
    {
        m_meter->clearErrorBoxShown();
        return CalibrateDevice();
    }
    catch ( std::logic_error & e )
    {
        ShowUnlessWrapperDid(m_meter, e.what(), "Calibration");
        return false;
    }
}

namespace
{
    // Hides the full-screen test color window CalibrateDevice shows for the 80%
    // white patch, on every way out of it (a calibrate that throws included).
    struct TestWindowGuard
    {
        bool shown;
        TestWindowGuard() : shown(false) {}
        ~TestWindowGuard()
        {
            if ( shown )
                ( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.ShowWindow(SW_HIDE);
        }
    };
}

bool CArgyllSensor::CalibrateDevice()
{
    if(!m_meter->doesMeterSupportCalibration())
    {
        GetColorApp()->InMeasureMessageBox( "No calibration capabilities are defined for this probe.","No Cals found",MB_OK);
        return false;
    }

    ArgyllMeterWrapper::eMeterState state(ArgyllMeterWrapper::NEEDS_MANUAL_CALIBRATION);
    while(state != ArgyllMeterWrapper::READY)
    {
		TestWindowGuard testWindow;
        std::string meterInstructions(m_meter->getCalibrationInstructions(m_HiRes != 0));
        if(meterInstructions.empty())
        {
            break;
        }
		if (meterInstructions == "Provide an 80% or greater white test patch" && GetConfig()->GetGeneratorType() == CColorHCFRConfig::enumAutomatic)
		{
			int display_mode  = GetConfig()->GetProfileInt("GDIGenerator","DisplayMode", DISPLAY_DEFAULT_MODE);
			if (display_mode == DISPLAY_ccast || display_mode == DISPLAY_rPI)
			{
				// Only this arm drives the generator (Display80); the else arm paints
				// m_wndTestColorWnd directly, so allocating before the branch leaked one
				// CGDIGenerator per calibration on every other display mode.
				CGDIGenerator *m_pGenerator = new CGDIGenerator(display_mode, false);
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
				( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.SetTestColor(RGB(204,204,204));
				( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> EnableWindow ( TRUE );
				( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.SetForegroundWindow();
				testWindow.shown = true;
				GetColorApp()->InMeasureMessageBox( "Place test window and meter on monitor to be calibrated.", "Calibration Instructions", MB_OK);
			}
		} else
	        GetColorApp()->InMeasureMessageBox( meterInstructions.c_str(), "Calibration Instructions", MB_OK);

		state = m_meter->calibrate();

        if(state == ArgyllMeterWrapper::INCORRECT_POSITION)
        {
            GetColorApp()->InMeasureMessageBox( m_meter->getIncorrectPositionInstructions().c_str(), "Incorrect Position", MB_OK+MB_ICONHAND);
        }
    }
    GetColorApp()->InMeasureMessageBox( "Device is now calibrated.  If the device requires it return to the correct measurement position.", "Calibration Complete", MB_OK);
    return true;
}

void CArgyllSensor::GetUniqueIdentifier( CString & strId )
{
    strId = m_meter->getMeterName().c_str();
}

void CArgyllSensor::FillDisplayTypeCombo(CComboBox& comboToFill)
{
    // No meter: an Init in the sheet failed and let go of it. Nothing to list.
    if ( m_meter == NULL )
        return;

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
    if ( m_meter == NULL )
        return;
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
        // The log path is a pointer into the config set by the sensor ctor and
        // the file may be unopenable (read-only install dir, locked); either
        // way drop the line instead of handing NULL to fprintf (#60).
        FILE *logFile = (m_logFile != NULL) ? fopen( m_logFile, "a" ) : NULL;
        if (logFile == NULL)
            return;
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
