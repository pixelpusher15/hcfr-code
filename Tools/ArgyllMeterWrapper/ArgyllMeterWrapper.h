/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2012 HCFR Project.  All rights reserved.
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
//    John Adcock
//    Ian C
/////////////////////////////////////////////////////////////////////////////

// ArgyllMeterWrapper.h: Wrapper for the argyll spectro library.
//
//////////////////////////////////////////////////////////////////////

#if !defined(ARGYLLWAPPER_HEADER_INCLUDED)
#define ARGYLLWAPPER_HEADER_INCLUDED

#include "libHCFR/Color.h"
#include <string>
#include <vector>
#include <stdarg.h>


#include "SpectralSampleFiles.h" 

struct _inst;

class ArgyllMeterWrapper
{
public:
    typedef enum
    {
        DISPLAY,
        TELEPHOTO,
        AMBIENT
    } eReadingType;

    typedef enum
    {
        READY,
        NEEDS_MANUAL_CALIBRATION,
        INCORRECT_POSITION,
    } eMeterState;

    /// ArgyllMeterWrapper constructor
    /// Create a USB meter object
    ArgyllMeterWrapper(_inst* meter);
    ~ArgyllMeterWrapper();

    /// initialize the meter
    /// returns true on success
    bool connectAndStartMeter(std::string& errorDescription, eReadingType readingType, CString spectralType, bool debugmode, double intTime, bool refresh);

    /// see if the meter supports calibration
    bool doesMeterSupportCalibration();

    // try and do a reading
    // the client application will need to handle the
    // possible non success conditions
    eMeterState takeReading(CString SpectralType);

    CColor getLastReading() const;

    int getNumberOfDisplayTypes();
    const char* getDisplayTypeText(int displayModeIndex);
    int getDisplayType() const;
    int getReadingType() const;
    void setDisplayType(int displayMode);
    int m_obType;

    // Determine if the meter supports spectral sample files
    bool doesMeterSupportSpectralSamples();

    //Set the observer type used for meter->XYZ calcs, spectral devices only, return true if changed
    bool setObType(CString SpectralType);
    void setObType(int obTypeInt);
    const char* getObTypeText(int obTypeInt);
    int getNumberOfObTypes();
    int getObType();

	// Load the supplied spectral sample
    bool loadSpectralSample(const SpectralSample& sample);

	// return current spectral sample identified by a textual description
    const std::string& currentSpectralSampleDescription();

    // Remove any spectral sample
    void resetSpectralSample();

    // calibrate the meter
    // this should be called 
    // after the user has been told what to do
    // and presumably followed them
    // return true on success
    // false if the display is in wrong state or for 
    /// anything else oddd
    eMeterState calibrate();

    /// if either take reading or calibrate returns 
    /// NEEDS_MANUAL_CALIBRATION then call this to find out
    /// what the user needs to do next
    std::string getCalibrationInstructions(bool HiRes);

    /// if either take reading or calibrate returns 
    /// INCORRECT_POSITION then call this to find out
    /// what the user needs to do next
    std::string getIncorrectPositionInstructions();

    /// Enable/Disable high resolution mode on i1Pro
    void setHiResMode(bool enableHiRes);

    /// Set low-light averaging on/off explicitly
    void setAdapt(bool bOn) { m_Adapt = bOn; }

    /// Current low-light averaging state
    bool getAdapt() const { return m_Adapt != 0; }

    /// Enable/Disable Rev. B AIO measurement mode (i1d3 family only)
    void setDisableAIO(bool disable);

    /// Does device support high resolution mode on i1Pro
    bool doesSupportHiRes() const;

    /// get the name of the meter given the meter
    std::string getMeterName() const;

    // do the meter objects point to the same underlying meter
    // not expected to be useful to client code but harmless
    bool isMeterStillValid() const;

    typedef std::vector<ArgyllMeterWrapper*> ArgyllMeterWrappers;

    // get a list of pointer to meters, the life time of the
    // pointers is handled at a global level and there is no need to free
    // or delete the returned objects
    static ArgyllMeterWrappers getDetectedMeters(std::string& errorMessage);

    // is the meter a colorimeter rather than a spectrometer
    virtual bool isColorimeter();

    // is the meter in refresh mode
    virtual bool isRefresh();

    // Emission calibration matrices of the driver. i1d3 family only - the one bundled
    // driver whose struct the wrapper reads (see setDisableAIO). False for every other meter.
    bool canExposeCalibrationMatrices() const;
    // nativeCal: the fit from the probe's own sensitivity curves (no sample, unity ccmat).
    // currentCal: ccmat * emis_cal as the driver applies it now. Both 3x3.
    // False when not exposable or no snapshot has been taken yet.
    bool getCalibrationMatrices(Matrix& nativeCal, Matrix& currentCal) const;
    // True when currentCal differs from nativeCal (SpectralMatricesEqual): the driver is
    // applying a correction the wrapper is dividing back out of every reading.
    bool hasDriverCorrection() const;
    // The instrument serial number ("" when the driver has none).
    std::string getSerialNumber() const;

private:
    void checkMeterIsInitialized();
    // Re-reads the i1d3 driver's emission calibration (see the .cpp). Returns true
    // when the driver was in its native state at this call (no spectral sample,
    // unity ccmat), i.e. when m_nativeCal was (re)captured.
    bool refreshCalibrationSnapshot();
    _inst* m_meter;
    int m_displayType;
    int m_meterType;
    eReadingType m_readingType;
    CColor m_lastReading;
    int m_nextCalibration;
    BOOL dark_only;
    char m_calibrationMessage[200];    
    bool m_Adapt;
    
    std::string m_SampleDescription;

    // i1d3 emission calibration snapshot (refreshCalibrationSnapshot).
    Matrix m_nativeCal;     // the driver's default fit, captured only in its native state
    Matrix m_currentCal;    // ccmat * emis_cal as the driver applies it now
    Matrix m_toNative;      // nativeCal * currentCal^-1, applied to every display reading
    bool m_nativeKnown;
    bool m_currentKnown;
    int m_nativeObType;     // the driver's observer when m_nativeCal was captured: the
                            // default fit depends on it, so a later observer change
                            // outside the native state makes the capture stale
};

extern void ArgyllLogMessage(const char* messageType, char *fmt, va_list& args);


#endif
