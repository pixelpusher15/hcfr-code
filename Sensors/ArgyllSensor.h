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
    BOOL        m_spectralApplyLeaveMeasures; // transient: last apply asked to leave measures mixed (not strip to raw)
    CString     m_cfgSnapSpectralPath;      // BeginConfigure/CancelConfigure snapshot
    CString     m_cfgSnapSpectralDesc;
    // Parsed-spectral-sample cache so Init does not re-read the file per
    // sweep/measurement; invalidated whenever the correction state changes.
    SpectralSample m_spectralSampleCache;
    bool        m_spectralCacheValid;
    CString     m_spectralCachePath;
private:
    ArgyllMeterWrapper* m_meter;
    SpectralSampleFiles *m_spectralSamples;

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
    // Loads a .ccss or ColourSpace .csv onto the meter so Argyll applies a
    // display-specific correction to every read. Forces the HCFR-side matrix to
    // identity so the two correction regimes don't stack.
    bool ApplySpectralCorrection(const CString& filePath);
    virtual void ClearSpectralCorrection();
    virtual BOOL TakePendingSpectralLeaveMeasures();
    virtual BOOL HasSpectralCorrection() const { return !m_spectralCorrectionPath.IsEmpty(); }
    CString GetSpectralCorrectionDesc() const { return m_spectralCorrectionDesc; }
    // Extend the base snapshot with the spectral state, which a Browse in the
    // property page commits (device + members) before the sheet closes.
    virtual void BeginConfigure();
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
};

#endif // !defined(AFX_ARGYLLSENSOR_H__1493C213_6A02_44C5_8EB7_55B469092E14__INCLUDED_)
