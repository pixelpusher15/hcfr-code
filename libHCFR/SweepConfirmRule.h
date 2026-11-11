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

// SweepConfirmRule.h: when a sweep start asks "...? Warning: actual data will
// be lost...". Header only; CMeasure::SweepOverwritesData builds the slot table
// from its arrays, the sweep-start handlers in datasetdoc.cpp pass the mask of
// the sweep they start.
//
// The rule. Advanced > "Confirm measures" is the master switch: off, no sweep
// asks. On, a sweep asks only when a slot it is about to overwrite holds a
// reading. A sweep's mask names every slot it writes, not only the series it
// is named after: a gray scale also replaces the on/off contrast pair, the
// primaries sweep replaces the primaries' white reference (and the on/off
// black when two B/W patches are added), so a contrast measured on its own is
// still protected when a gray scale runs after it.
//
// A reading is a valid color that is not a constructor placeholder. A new
// document is not all noDataColor: CMeasure's constructor pre-loads the top
// gray slot, the on/off white and black and the primaries' white with nominal
// values so the charts have something to draw, and those pass isValid(). The
// table carries, for the gray, the two whites and the on/off black, the test
// that recognizes a placeholder (CMeasure::IsConstructorPlaceholder: either
// raw-less constructor white, the on/off black one) and leaves a white out
// when its white-measured flag is clear (Measure.h: "anything that needs to
// know whether white was ever MEASURED has to ask this, not the color"). The
// whites also test the color: a version 8 or older file loads the gray-top
// placeholder into the on/off white, and the flag recovery there
// (IsPlaceholderWhite with the on/off signature) does not recognize it. The
// gray top can hold the on/off placeholder too: the luminance and gamma
// graphers copy the on/off white there (CMeasure::SweepOverwritesData).
//
// The masks below are a hand-maintained copy of what the sweep bodies assign.
// ColorMathTest T52 compares them with a second hand-typed list of the same
// writes, each line cited to Measure.cpp: two hand-maintained copies that
// catch each other drifting, not a check against the code itself. A write
// added to a sweep body without updating both still goes unseen; the
// structural fix (each sweep declaring its mask beside its writes) is backlog.

#if !defined(SWEEPCONFIRMRULE_H_INCLUDED)
#define SWEEPCONFIRMRULE_H_INCLUDED

#include "Color.h"

namespace SweepConfirmRule
{
    // The slots a sweep can write, one bit each.
    enum Series
    {
        S_GRAY          = 1u << 0,
        S_NEARBLACK     = 1u << 1,
        S_NEARWHITE     = 1u << 2,
        S_PRIMARIES     = 1u << 3,
        S_SECONDARIES   = 1u << 4,
        S_PRIMEWHITE    = 1u << 5,     // the primaries' white reference
        S_ONOFFBLACK    = 1u << 6,
        S_ONOFFWHITE    = 1u << 7,
        S_ANSI          = 1u << 8,     // ANSI contrast black and white
        S_SAT_RED       = 1u << 9,
        S_SAT_GREEN     = 1u << 10,
        S_SAT_BLUE      = 1u << 11,
        S_SAT_YELLOW    = 1u << 12,
        S_SAT_CYAN      = 1u << 13,
        S_SAT_MAGENTA   = 1u << 14,
        S_CC24          = 1u << 15     // the active color checker set's band
    };

    // What each sweep writes (Measure.cpp, the CMeasure::Measure* sweeps and,
    // for the simultaneous variants, the ValidateBackground* functions).
    const unsigned int PrimarySaturations = S_SAT_RED | S_SAT_GREEN | S_SAT_BLUE;
    const unsigned int AllSixSaturations  = PrimarySaturations | S_SAT_YELLOW | S_SAT_CYAN | S_SAT_MAGENTA;
    const unsigned int AllSaturations     = AllSixSaturations | S_CC24;
    const unsigned int GrayScale          = S_GRAY | S_ONOFFBLACK | S_ONOFFWHITE;
    // MeasureGrayScaleAndColors writes the primaries' white reference only when
    // a B/W patch is added (General > "B/W colors to add" above 0).
    inline unsigned int GrayScaleAndColors(int bwColorsToAdd)
    {
        return S_GRAY | S_PRIMARIES | S_SECONDARIES | S_ONOFFBLACK | S_ONOFFWHITE
             | ( bwColorsToAdd > 0 ? (unsigned int) S_PRIMEWHITE : 0u );
    }
    // Simultaneous variants: ValidateBackgroundGrayScale writes the grays only;
    // ValidateBackgroundGrayScaleAndColors leaves the on/off white alone and
    // writes the white reference whatever the B/W patch count (unconditional).
    const unsigned int SimGrayScale          = S_GRAY;
    const unsigned int SimGrayScaleAndColors = S_GRAY | S_PRIMARIES | S_SECONDARIES | S_PRIMEWHITE | S_ONOFFBLACK;

    // The primaries and secondaries sweeps write the white reference always
    // (noDataColor when no B/W patch is added) and the on/off black when the
    // second B/W patch is added (General > "B/W colors to add" above 1).
    inline unsigned int Primaries(int bwColorsToAdd)
    {
        return S_PRIMARIES | S_PRIMEWHITE | ( bwColorsToAdd > 1 ? (unsigned int) S_ONOFFBLACK : 0u );
    }
    inline unsigned int Secondaries(int bwColorsToAdd)
    {
        return Primaries ( bwColorsToAdd ) | S_SECONDARIES;
    }

    // MeasureContrast writes the ANSI pair only when the generator can show
    // the ANSI black/white rectangles (CGenerator::CanDisplayAnsiBWRects).
    inline unsigned int Contrast(bool ansiRects)
    {
        return S_ONOFFBLACK | S_ONOFFWHITE | ( ansiRects ? (unsigned int) S_ANSI : 0u );
    }

    // The full run (OnMeasureFullTiltBoogie): gray scale and colors, near
    // black, near white, every saturation and the color checker, contrast.
    inline unsigned int FullRun(int bwColorsToAdd, bool ansiRects)
    {
        return GrayScaleAndColors ( bwColorsToAdd ) | S_NEARBLACK | S_NEARWHITE | AllSaturations | Contrast ( ansiRects );
    }

    // Recognizes a series' constructor placeholder; NULL when it has none.
    typedef bool (*PlaceholderTest)(const CColor & c);

    // One series' slots as the document holds them.
    struct SeriesSlots
    {
        unsigned int     series;        // one Series bit
        const CColor *   slots;
        int              count;
        PlaceholderTest  isPlaceholder;
        // The black override's synthetic blacks, or NULL: a first slot that
        // IsSyntheticBlack matches against either is not a reading.
        const CColor *   syntheticFirst;
        const CColor *   syntheticFirstLive;
    };

    inline bool IsReading(const CColor & c, PlaceholderTest isPlaceholder)
    {
        return c.isValid() && ! ( isPlaceholder && isPlaceholder ( c ) );
    }

    // The black override ([References] "Use Black Level"): every sweep writes
    // the user's black (CMeasure::m_userBlack), a synthetic value and not a
    // reading, into the first gray and near black slot and into the on/off
    // black (Measure.cpp 2392, 2737, 3011, 7618, 7816, 7989; 2416, 2785, 6082,
    // 6294, 6489, 7896, 7959, 8020). Its Z ratio (1.0883) is not the
    // constructor placeholder's (1.08833), so no placeholder test catches it.
    // It is recognized by value: no raw value (a meter reading carries raw)
    // and Y exactly the user's black Y. X and Z are not compared: the app
    // re-derives them with other white-point constants after the copy (traced
    // on an empty document: m_userBlack (0.0475235, 0.05, 0.054415), the slots
    // (0.047523464475027963, 0.05, 0.054448524048047453)). A reading with a raw
    // value, or with another Y, is data and still asks. The one case this
    // misjudges - a raw-less measured black in a pre-raw document whose Y
    // equals the override Y exactly - is negligible. CMeasure::GetGrayScaleBlack
    // uses this same test for the document's override black.
    inline bool IsSyntheticBlack(const CColor & c, const CColor & userBlack)
    {
        if ( !c.isValid() || !userBlack.isValid() || c.HasRawXYZValue() )
            return false;
        return c.GetXYZValue()[1] == userBlack.GetXYZValue()[1];
    }

    inline bool SlotsHoldData(const SeriesSlots & s)
    {
        if ( s.slots == NULL )
            return false;
        for ( int i = 0; i < s.count; i++ )
        {
            if ( i == 0 && ( ( s.syntheticFirst && IsSyntheticBlack ( s.slots[0], *s.syntheticFirst ) )
                          || ( s.syntheticFirstLive && IsSyntheticBlack ( s.slots[0], *s.syntheticFirstLive ) ) ) )
                continue;
            if ( IsReading ( s.slots[i], s.isPlaceholder ) )
                return true;
        }
        return false;
    }

    // Whether a sweep writing 'sweep' would overwrite at least one reading:
    // any table entry whose series the sweep writes, holding any reading.
    inline bool HoldsData(unsigned int sweep, const SeriesSlots * table, int n)
    {
        for ( int k = 0; k < n; k++ )
            if ( ( table[k].series & sweep ) != 0 && SlotsHoldData ( table[k] ) )
                return true;
        return false;
    }

    // Whether the sweep-start confirmation is shown.
    inline bool AskBeforeSweep(bool confirmMeasures, bool overwritesData)
    {
        return confirmMeasures && overwritesData;
    }
}

#endif // !defined(SWEEPCONFIRMRULE_H_INCLUDED)
