/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2012 Association Homecinema Francophone.  All rights reserved.
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
//  PhoenixF15E
/////////////////////////////////////////////////////////////////////////////

// MeterCalibrationRule.h: when the Argyll sensor page's "Calibrate meter" button
// has something to do, and when the i1Pro white tile question is a real choice.
// Header only, no driver types: ArgyllMeterWrapper.cpp checks the two driver
// constants below against Tools/spectro/inst.h at compile time.
//
// What a Calibrate does. The page's Calibrate runs an Init (which also resets
// the wrapper's pending condition) and then asks the driver for
// inst_calt_available. Every driver's calibrate() translates that request into
// "available & inst_calt_n_dfrble_mask" and returns inst_ok at once when the
// result is empty ("Nothing todo": dtp20/22/41/51/92, ex1, i1d3, i1disp,
// i1pro_imp, i1pro3_imp, kleink10, munki_imp, smcube, specbos, spyd2, spydX,
// spydX2, ss); a driver with no calibrate() (colorhug, huey, hcfr) answers
// inst_unsupported and lists nothing (inst.c). The wrapper then says "No
// calibrations needed." So a Calibrate does something exactly when the
// driver's current mode lists a NON-DEFERRABLE available calibration.
// Deferrable ones (inst_calt_ref_freq, inst_calt_emis_int_time) are never done
// by the button: the drivers do them at the first reading (i1d3.c,
// i1d3_read_sample, "Attempt a refresh display frame rate calibration if
// needed").
//
// Which page selections can change that answer. get_n_a_cals reports for the
// CURRENT mode only, and there is no query for another mode, so for a
// selection the page has not committed the answer is unknown:
// - Reading Type sets the instrument mode (connectAndStartMeter: spot, tele,
//   ambient). It changes the answer: spydX/spydX2 list inst_calt_emis_offset
//   only when not in ambient mode.
// - Hi-Res (i1Pro family) changes the i1Pro's lists (i1pro_imp_get_n_a_cals,
//   the high resolution reflective fine calibration).
// - Display Type changes only the refresh mode (i1d3, i1disp, spyd2, specbos
//   refrmode, set from the display type's entry), and the refresh mode gates only
//   inst_calt_ref_freq, which is deferrable: no driver's non-deferrable list
//   depends on it. Spectral Type, integration time, Disable AIO and Debug do
//   not enter any get_n_a_cals.
// So the button is enabled whenever the page's Reading Type or Hi-Res differs
// from the committed one (the Calibrate commits them, Inits, and the driver
// decides), and otherwise follows the driver's answer for the mode it is in -
// provided that mode is this sensor's: the wrapper is shared by every document
// on the meter, and a start for another document can set another mode.
// No meter: disabled, as before (#256); a meter whose driver is gone, or a
// query that fails: enabled, and the click says what happened.

#if !defined(METERCALIBRATIONRULE_H_INCLUDED)
#define METERCALIBRATIONRULE_H_INCLUDED

namespace MeterCalibrationRule
{
    // inst_calt_n_dfrble_mask: the calibrations an inst_calt_available request
    // performs (Tools/spectro/inst.h, inst_cal_type).
    const unsigned int NonDeferrableMask = 0x0000fff0u;
    // inst_calt_em_dark: emissive dark calibration.
    const unsigned int EmissiveDark      = 0x00000400u;

    enum Availability
    {
        CAL_UNKNOWN = -1,   // no answer from the driver
        CAL_NONE    = 0,    // a Calibrate in this mode does nothing
        CAL_SOME    = 1     // a Calibrate in this mode calibrates
    };

    // The driver's answer for its current mode: queryOk is get_n_a_cals'
    // success, supportsCalibration the wrapper's capability test (without it
    // CArgyllSensor::CalibrateDevice stops with "No Cals found"), available the
    // available mask get_n_a_cals returned.
    inline Availability FromDriver(bool queryOk, bool supportsCalibration, unsigned int available)
    {
        if ( !queryOk )
            return CAL_UNKNOWN;
        if ( !supportsCalibration )
            return CAL_NONE;
        return ( available & NonDeferrableMask ) != 0 ? CAL_SOME : CAL_NONE;
    }

    // Whether the page's Calibrate button is enabled. pageReadingType/pageHiRes
    // are the page's current selections, sensorReadingType/sensorHiRes the
    // committed ones; driverModeIsSensors is false when another start has set
    // the driver's mode since this sensor's last Init; live is FromDriver's
    // answer for the driver's current mode.
    inline bool CalibrateEnabled(bool hasMeter,
                                 unsigned int pageReadingType, bool pageHiRes,
                                 unsigned int sensorReadingType, bool sensorHiRes,
                                 bool driverModeIsSensors, Availability live)
    {
        if ( !hasMeter )
            return false;
        if ( pageReadingType != sensorReadingType || pageHiRes != sensorHiRes )
            return true;
        if ( !driverModeIsSensors )
            return true;
        return live != CAL_NONE;
    }

    // Whether "Skip the white tile calibration?" is a real choice: an i1Pro
    // family meter, Hi-Res off (Hi-Res always takes what the driver asks for),
    // the driver's first condition for an inst_calt_available calibration is
    // the white tile (inst_calc_man_ref_white), and an emissive dark
    // calibration is available on its own, which is what skipping it does.
    inline bool AskSkipWhiteTile(bool i1ProFamily, bool hiRes,
                                 bool driverAsksWhiteTile, unsigned int available)
    {
        return i1ProFamily && !hiRes && driverAsksWhiteTile
            && ( available & EmissiveDark ) != 0;
    }
}

#endif // !defined(METERCALIBRATIONRULE_H_INCLUDED)
