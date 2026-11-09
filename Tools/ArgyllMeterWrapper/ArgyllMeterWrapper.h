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
    // the driver's default display type index (0 when it lists none)
    int getDefaultDisplayType();
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
    // True when one of this wrapper's own error boxes (init_inst, setting the observer
    // or the integration time, Set display type error, Calibration Failed!) was shown
    // since clearErrorBoxShown for a failure it then returns or throws: that failure
    // has been told to the user, and the caller shows no second message for it. A box
    // with no failure after it (the inst_wrong_setup one) does not set it.
    bool errorBoxShown() const { return m_errorBoxShown; }
    void clearErrorBoxShown() { m_errorBoxShown = false; }
    // The instrument serial number ("" when the driver has none).
    std::string getSerialNumber() const;
    // Derives the correction a spectral sample would make, using the driver's own fit:
    // loads the sample, reads ccmat * emis_cal (currentCal), restores the current display
    // type (which drops the sample and refits the native matrix), reads it again
    // (nativeCal). Leaves the driver NATIVE. False (driver untouched or restored, why set)
    // when not exposable, when either read is not in the state it must be in (sample
    // present after load; no sample and unity ccmat after restore), or when the two are
    // equal (SpectralMatricesEqual): the sample did not change the matrix.
    bool deriveSpectralCalibration(const SpectralSample& sample, Matrix& nativeCal, Matrix& currentCal, std::string& why);
    // Same for a ccss/ccmx entry of the display-type list (index as the list numbers it):
    // selects the entry, reads currentCal, selects its BASE entry, reads nativeCal, and
    // leaves the base entry selected. The base of a ccmx entry is the list entry whose
    // cbid == entry.cc_cbid; of a ccss entry the entry with the same refresh flag and
    // flags carrying neither ccss nor ccmx. False as above, or when the entry is a plain one.
    bool deriveDisplayTypeCalibration(int displayTypeIndex, int& baseIndex, Matrix& nativeCal, Matrix& currentCal, std::string& why);
    // True when list entry `index` carries a ccss or a ccmx (public inst_disptypesel flags).
    bool isDisplayTypeCorrected(int index);

private:
    void checkMeterIsInitialized();
    // Reads the i1d3 driver's emission calibration: cal = ccmat * emis_cal, whether a
    // spectral sample is in force, whether ccmat is unity. False when not exposable.
    bool readCalibration(Matrix& cal, bool& sampleLoaded, bool& ccmatUnity);
    _inst* m_meter;
    int m_displayType;
    int m_meterType;
    eReadingType m_readingType;
    CColor m_lastReading;
    int m_nextCalibration;
    BOOL dark_only;
    char m_calibrationMessage[200];    
    bool m_Adapt;
    bool m_errorBoxShown;
    
    std::string m_SampleDescription;
};

extern void ArgyllLogMessage(const char* messageType, char *fmt, va_list& args);


#endif
