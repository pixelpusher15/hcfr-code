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

// FailedReadingRule.h: a failed reading never becomes data, and the user always
// knows. Header only, no MFC or driver types, so ColorMathTest (T50) checks the
// same functions the app calls.
//
// A reading fails when the sensor says so (CSensor::MeasureColor: the driver
// flagged it, or the XYZ is not finite) - IsMeasureValid() is FALSE, the reading
// is noDataColor and the sensor's error string says why. Every sweep then shows
// its Abort / Retry / Ignore box. What happens to the document:
//
// - A sweep (Measure.cpp, every slot it fills: gray scale, near black, near
//   white, the saturations, primaries and secondaries, the color checker, the
//   display profile) stores a reading only after it passed. The reading lands
//   in the sweep's buffer first; KeepPassedReading replaces a failed one with
//   what the slot already held (PreviousReading: a constructor placeholder is
//   not a reading, so it counts as never measured), so neither the live write
//   during the sweep nor the commit at its end can store it. Abort, and Ignore
//   ("skip this patch, keep what the document had"), leave the slot's previous
//   reading in place. A kept slot changes nothing: the live write during the
//   sweep happens only for a reading that passed, and the commit skips the
//   slot. KeptSlots remembers which slots were kept, and the commit neither writes them nor
//   stamps them (lux, provenance) nor derives anything from them (the on/off
//   white and black, the prime white, their "measured" flags, the color
//   checker's master band): those come only from slots that passed.
// - A free measure has no slot to keep: Retry reads again, Ignore appends
//   nothing, Abort leaves (FreeMeasureOutcome).
// - The gamma fit never takes an invalid black where it reads one: only the
//   relative offset (type 1) does (GammaFitNoBlack: no fit, gamma 1.0 and the
//   configured offset); every other type fits normally (MeanPointGamma). A
//   chart draws when any slot holds a reading, leaving out each invalid point,
//   instead of hiding everything because slot 0 is invalid (AnyHeldReading).
//   A fit with no point that has light is no fit either (MeanPointGamma). The
//   user's black override holds a reading whatever its value
//   (HoldsBlackReading).
// - A read that asks for calibration right after the read loop's Calibrate
//   attempt is a failed reading, never another round of calibration
//   (AfterCalibration): its reason says whether a calibration ran and did not
//   take, or the driver had no calibration to run.

#if !defined(FAILEDREADINGRULE_H_INCLUDED)
#define FAILEDREADINGRULE_H_INCLUDED

#include <vector>

namespace FailedReadingRule
{
    // The user's answer to the Abort / Retry / Ignore box a failed reading raises.
    enum Answer
    {
        ANSWER_ABORT,
        ANSWER_RETRY,
        ANSWER_IGNORE
    };

    // What a slot holds as a sweep's previous reading: the slot itself, or
    // nothing (noDataColor) when it holds a constructor placeholder
    // (isPlaceholder; the app passes CMeasure::IsConstructorPlaceholder). A new
    // document's top gray slot holds a valid-looking nominal white that no
    // meter measured; keeping it on a failed patch would fabricate a reading.
    template <class Reading, class IsPlaceholder>
    inline const Reading & PreviousReading(const Reading & slot, const Reading & nothing, IsPlaceholder isPlaceholder)
    {
        return isPlaceholder(slot) ? nothing : slot;
    }

    // A sweep slot. reading is the sweep's buffer entry for the patch just read,
    // previous what the document holds in the slot that patch fills (through
    // PreviousReading), passed the sensor's verdict (IsMeasureValid). A passed
    // reading stays; a failed one is replaced by previous. Returns passed.
    // A kept slot is skipped by the commit (KeptSlots).
    template <class Reading>
    inline bool KeepPassedReading(Reading & reading, const Reading & previous, bool passed)
    {
        if ( !passed )
            reading = previous;
        return passed;
    }

    // Which buffer entries of a sweep hold a kept previous reading. The sweep
    // notes every read (a Retry that passes clears the note), and its commit
    // skips every kept entry: not written, not lux- or provenance-stamped, not
    // reset when the luxmeter dropped out, and nothing derived from it. The
    // slot keeps the document's reading exactly as it was.
    class KeptSlots
    {
    public:
        explicit KeptSlots(int n) : m_kept(n > 0 ? n : 0, false) {}

        // Returns passed, so a call wraps KeepPassedReading and gates the live write.
        bool Note(int idx, bool passed)
        {
            if ( idx >= 0 && idx < (int)m_kept.size() )
                m_kept[idx] = !passed;
            return passed;
        }

        bool IsKept(int idx) const
        {
            return idx >= 0 && idx < (int)m_kept.size() && m_kept[idx];
        }


    private:
        std::vector<bool> m_kept;
    };

    // A free measure (CMeasure::AddMeasurement): what to do with a reading.
    enum FreeMeasureStep
    {
        FREE_APPEND,            // the reading passed: append it
        FREE_READ_AGAIN,        // Retry: read the same patch again
        FREE_APPEND_NOTHING     // Ignore or Abort: append nothing and stop
    };

    inline FreeMeasureStep FreeMeasureOutcome(bool passed, Answer answer)
    {
        if ( passed )
            return FREE_APPEND;
        return answer == ANSWER_RETRY ? FREE_READ_AGAIN : FREE_APPEND_NOTHING;
    }

    // CDataSetDoc::ComputeGammaAndOffset's gamma when it fits nothing.
    const double NoFitGamma = 1.0;

    // The offset ComputeGammaAndOffset uses for a gamma offset type: set by the
    // configuration, never fitted, so it does not depend on the black. Type 2
    // (camera gamma) 0.055 for sRGB, else 0.099; type 3 the manual offset; every
    // other type 0. Every return of ComputeGammaAndOffset carries it.
    inline double ConfiguredGammaOffset(int offsetType, bool sRGB, double manualOffset)
    {
        if ( offsetType == 2 )
            return sRGB ? 0.055 : 0.099;
        if ( offsetType == 3 )
            return manualOffset;
        return 0.0;
    }

    // Whether the fit reads the black: only the relative offset (type 1)
    // subtracts it from the white and from every point. The power law (types
    // 0, 2, 3) never reads it, and the EOTF targets (4..7, BT.1886) go through
    // getL_EOTF, which takes its own stand-in (0.012) for an invalid black.
    inline bool FitReadsBlack(int offsetType)
    {
        return offsetType == 1;
    }

    // An invalid black never enters the fit that reads it (FX_NODATA would be a
    // black of -99999.99). True when the black is invalid and the offset type
    // reads it, with gamma NoFitGamma and offset the configured one; false, with
    // gamma and offset untouched, when the fit may go ahead.
    inline bool GammaFitNoBlack(bool blackValid, int offsetType, double configuredOffset, double * gamma, double * offset)
    {
        if ( blackValid || !FitReadsBlack(offsetType) )
            return false;
        *gamma = NoFitGamma;
        *offset = configuredOffset;
        return true;
    }

    // What a caller of ComputeGammaAndOffset takes as the gamma: the fit when
    // there is one, else its own fallback (m_GammaRef for the measured-gamma
    // target), never the no-fit NoFitGamma.
    inline double FittedGammaOr(bool fitted, double fitGamma, double fallback)
    {
        return fitted ? fitGamma : fallback;
    }

    // A reading a target or the fit may take: valid, and not a constructor
    // placeholder (isPlaceholder; the app passes
    // CMeasure::IsConstructorPlaceholder). The fit and every black-compensation
    // target test the black they read with this one predicate.
    template <class Reading, class IsPlaceholder>
    inline bool HoldsReading(const Reading & c, IsPlaceholder isPlaceholder)
    {
        return c.isValid() && !isPlaceholder(c);
    }

    // The black a target or the fit reads: HoldsReading, except that the user's
    // black override ("Use Black Level", userBlackOn) is a reading by decision,
    // whatever its value - at 0 cd/m2 (an OLED) it has the all-zero black
    // placeholder's value, and a rawless reading of exactly 0 in a pre-raw
    // document cannot be told from that placeholder either.
    template <class Reading, class IsPlaceholder>
    inline bool HoldsBlackReading(const Reading & black, bool userBlackOn, IsPlaceholder isPlaceholder)
    {
        return black.isValid() && ( userBlackOn || !isPlaceholder(black) );
    }

    // The fit itself (ComputeGammaAndOffset): the mean of pointGamma(i) over the
    // points 1 .. size-3 that have light (lum[i] > 0), in *mean (1 point at least
    // in the divisor, as the original loop had it). Returns whether any point
    // had light: none (every mid slot failed or dark) is no fit.
    template <class PointGamma>
    inline bool MeanPointGamma(int size, const double * lum, PointGamma pointGamma, double * mean)
    {
        int nb = 0;
        double avg = 0;
        for ( int i = 1; i < size - 2; i++ )
        {
            if ( lum[i] > 0.0 )
            {
                avg += pointGamma(i);
                nb ++;
            }
        }
        *mean = avg / ( nb ? nb : 1 );
        return nb > 0;
    }

    // Whether any of the n slots get(0) .. get(n-1) holds a reading: valid, and
    // not a constructor placeholder (isPlaceholder; the app passes
    // CMeasure::IsConstructorPlaceholder, since a new document's top gray slot
    // holds a valid-looking nominal white). A chart draws when this holds and leaves
    // out each invalid point, instead of drawing only when slot 0 is valid.
    template <class Get, class IsPlaceholder>
    inline bool AnyHeldReading(int n, Get get, IsPlaceholder isPlaceholder)
    {
        for ( int i = 0; i < n; i++ )
        {
            if ( HoldsReading(get(i), isPlaceholder) )
                return true;
        }
        return false;
    }

    // The Argyll read loop (CArgyllSensor::MeasureColorInternal): the Calibrate
    // attempt it made between the previous read of this reading and this one.
    enum CalibrationAttempt
    {
        CAL_ATTEMPT_NONE,           // no Calibrate since the previous read
        CAL_ATTEMPT_RAN,            // a calibration ran and reported success
        CAL_ATTEMPT_RAN_NOTHING     // reported success, but the driver had no
                                    // calibration to run (empty instructions)
    };

    enum ReadAfterCalibration
    {
        READ_GOES_ON,               // no attempt to answer for: calibrate if this
                                    // read asks, else the reading stands
        READ_FAILS_STILL_NEEDS,     // IDS_ARGYLL_STILL_NEEDS_CALIBRATION
        READ_FAILS_UNAVAILABLE      // IDS_ARGYLL_CALIBRATION_UNAVAILABLE
    };

    // The attempt the read loop carries to its next read: only a read that came
    // back (READY) clears it. A position prompt between a Calibrate and an
    // asking read does not, so a meter that alternates the two cannot make the
    // loop calibrate without bound.
    inline CalibrationAttempt AttemptAfterRead(CalibrationAttempt attempt, bool readReady)
    {
        return readReady ? CAL_ATTEMPT_NONE : attempt;
    }

    // A read that asks for calibration right after an attempt of either kind
    // ends the reading through the normal Abort / Retry / Ignore box, with the
    // reason that matches the attempt; each Retry allows one more attempt. No
    // case calibrates again silently.
    inline ReadAfterCalibration AfterCalibration(CalibrationAttempt attempt, bool readNeedsCalibration)
    {
        if ( !readNeedsCalibration || attempt == CAL_ATTEMPT_NONE )
            return READ_GOES_ON;
        return attempt == CAL_ATTEMPT_RAN ? READ_FAILS_STILL_NEEDS : READ_FAILS_UNAVAILABLE;
    }
}

#endif // !defined(FAILEDREADINGRULE_H_INCLUDED)
