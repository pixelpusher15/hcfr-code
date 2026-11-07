/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2008 Association Homecinema Francophone.  All rights reserved.
// Copyright (c) 2012-2015 Hcfr project.  All rights reserved.
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

// ArgyllSensor.h: interface for the ArgyllSensor class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ARGYLLSENSOR_H__1493C213_6A02_44C5_8EB7_55B469092E14__INCLUDED_)
#define AFX_ARGYLLSENSOR_H__1493C213_6A02_44C5_8EB7_55B469092E14__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "OneDeviceSensor.h"
#include "Argyllsensorproppage.h"
#include "ArgyllMeterWrapper.h"

#include "../Tools/ArgyllMeterWrapper/SpectralSample.h"

class CArgyllSensor : public COneDeviceSensor  
{
public:
    DECLARE_SERIAL(CArgyllSensor) ;

protected:
    CArgyllSensorPropPage m_ArgyllSensorPropertiesPage;
    static bool m_debugMode;

public:
    UINT		m_DisplayType;
    UINT		m_ReadingType;
    CString		m_SpectralType;
    UINT        m_intTime;
    BOOL		m_HiRes;
    BOOL        m_Adapt;
    BOOL        m_DisableAIO;   // Disable Rev. B AIO measurement mode (i1d3 / ColorMunki Display)
    CString     m_spectralCorrectionPath;   // canonical .ccss loaded onto the meter (empty = none)
    CString     m_spectralCorrectionDesc;   // description shown in the UI
    bool        m_displayTypeFromIni;       // m_DisplayType was read from the INI (meter ctor), not a .chc or Copy
    CString     m_cfgSnapSpectralPath;      // BeginConfigure/CancelConfigure snapshot
    CString     m_cfgSnapSpectralDesc;
    // The device settings GetPropertiesSheetValues commits (members and registry) -
    // the page's Calibrate button commits them mid-sheet - so a cancelled sheet puts
    // them back too. Valid only after a BeginConfigure.
    bool        m_cfgSnapDeviceValid;
    UINT        m_cfgSnapDisplayType;
    UINT        m_cfgSnapReadingType;
    CString     m_cfgSnapSpectralType;
    UINT        m_cfgSnapIntTime;
    BOOL        m_cfgSnapHiRes;
    BOOL        m_cfgSnapAdapt;
    BOOL        m_cfgSnapDisableAIO;
    bool        m_cfgSnapDebugMode;
    // Parsed-spectral-sample cache so Init does not re-read the file per
    // sweep/measurement; invalidated whenever the correction state changes.
    SpectralSample m_spectralSampleCache;
    bool        m_spectralCacheValid;
    CString     m_spectralCachePath;
private:
    ArgyllMeterWrapper* m_meter;
    SpectralSampleFiles *m_spectralSamples;
    // The base entry the last successful Display Type derive left selected, and the
    // entry it belongs to (-1 / 0xFFFFFFFF: none yet). Lets Init measure on the base
    // without deriving again while the record still matches (see Init).
    int         m_dtBaseIndex;
    UINT        m_dtBaseFor;
    // True only while GetPropertiesSheetValues re-inits for a Display Type the user
    // changed in the sheet: tells a Display Type CHANGE from a document load.
    bool        m_displayTypeChangedInSheet;
    // Set by Init: the Display Type record's entry (matched by its text) is no
    // longer in the meter's list. The record stays applied, the driver measures on
    // a plain entry, and Sync leaves the Display Type alone. Re-evaluated every
    // Init, so a Display Type chosen in the sheet clears it.
    bool        m_displayTypeRecordEntryMissing;

public:
    CArgyllSensor();
    CArgyllSensor(ArgyllMeterWrapper* meter);
    virtual ~CArgyllSensor();

    // Overriden functions from CSensor
    virtual void Copy(CSensor * p);    
    virtual void Serialize(CArchive& archive); 

    virtual BOOL Init( BOOL bForSimultaneousMeasures );
    virtual BOOL Release();

    virtual void SetPropertiesSheetValues();
    virtual void GetPropertiesSheetValues();

    virtual LPCSTR GetStandardSubDir ()    { return "Etalon_Argyll"; }

    void Calibrate();

    // Spectral correction (restores the 2012 ccss apply path, extended to .csv).
    // Loads a .ccss or ColourSpace .csv onto the meter, derives the correction
    // from the driver's two calibration matrices and makes it the sensor's
    // method (CALIB_SPECTRAL_SAMPLE): HCFR applies it to native readings like
    // any single-matrix method. Meters whose matrices are not exposed keep the
    // driver applying the sample (identity record, driver-corrected basis).
    bool ApplySpectralCorrection(const CString& filePath);
    virtual void ClearSpectralCorrection();
    CString GetSpectralCorrectionDesc() const { return m_spectralCorrectionDesc; }
    CString GetSpectralCorrectionPath() const { return m_spectralCorrectionPath; }
    // True, after telling the user why, when Display Type list entry `displayType`
    // carries a ccss or ccmx itself: a browsed file would be replaced by the entry's
    // correction at the next Init, so Browse is refused while it is selected.
    bool RefuseSpectralBrowse(UINT displayType);
    // Extend the base snapshot with the spectral state, which a Browse in the
    // property page commits (device + members) before the sheet closes.
    virtual void BeginConfigure();
    virtual bool CorrectionChangedSinceBeginConfigure() const;
    virtual void CancelConfigure();
    bool MeterSupportsSpectralSamples();

    virtual void GetUniqueIdentifier( CString & strId );
    static bool isInDebugMode() {return m_debugMode;}
    virtual bool isValid() const {return (m_meter != 0);}
    virtual int ReadingType() const {return m_ReadingType;}
    virtual CString SpectralType() const {return m_SpectralType;}
    void FillDisplayTypeCombo(CComboBox& comboToFill);
    void FillSpectralTypeCombo(CComboBox& comboToFill);
    virtual bool isColorimeter() const;
    virtual bool supportsAvg() const;
    virtual void setAvgEnabled(bool bOn);
    virtual bool getAvgEnabled() const;
    virtual bool isRefresh() const;

private:
    virtual CColor MeasureColorInternal(const ColorRGBDisplay& aRGBValue, int displaymode = 0);
    // After Init, for a meter whose calibration matrices are exposed: derives a
    // ccss/ccmx Display Type entry (leaving the driver on its base entry) or
    // re-derives a browsed sample for another unit/observer (see the .cpp).
    void SyncSpectralRecordFromMeter();
    // A .ccss through SpectralSample::Read or a correlation .csv through
    // createFromCorrelationCSV, by extension. False with the reason in `why`.
    static bool ReadSpectralFile(const CString& path, SpectralSample& ss, std::string& why);
    // The FILE-source fields of a record: source, description (the sample's display,
    // else its description, else m_spectralCorrectionDesc), file hash, observer and
    // meter serial. The caller sets the matrices and the basis.
    void MakeFileRecord(const CString& path, const SpectralSample* ss, SpectralCorrectionRecord& rec);
    // The Display Type entry's text ("" when the meter cannot say).
    std::string DisplayTypeText(UINT index);
    // The list index whose text is `text`; -1 when absent or `text` is empty.
    int FindDisplayTypeByText(const std::string& text);
    // The record is the selected corrected entry's, for this unit and observer, and
    // the base it was derived on is known: nothing to derive again.
    bool DisplayTypeRecordIsCurrent();
};

#endif // !defined(AFX_ARGYLLSENSOR_H__1493C213_6A02_44C5_8EB7_55B469092E14__INCLUDED_)
