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

// BlackOverrideRule.h: which black goes back into a document when the main
// view's black-override swap ends. Header only.
//
// The swap. While References > "Override black" is ticked, CMainView::InitGrid
// takes the document's gray 0%, near-black 0% and on/off black out into view
// members and shows the override value in their place; unticking it writes the
// members back, and a save writes them into the file (CDataSetDoc::Serialize)
// while the open document keeps showing the override.
//
// The rule. A slot gets its captured value back, except when the capture was
// taken before the slot's series held any data - the capture is empty and the
// series was empty - and the series holds data now: a sweep run under the
// override filled the series (and wrote the override black into slot 0) after
// the capture. Writing the empty capture back then would blank the 0% slot of
// a measured series and collapse its analysis, so the slot keeps what the
// sweep put there. "Data" is a reading in the CMeasure::HoldsReading sense (a
// valid color that is not a constructor placeholder); the app decides it, this
// header only combines the answers.

#if !defined(BLACKOVERRIDERULE_H_INCLUDED)
#define BLACKOVERRIDERULE_H_INCLUDED

namespace BlackOverrideRule
{
    // At capture: the capture predates the series' data when neither the
    // captured slot nor any other slot of its series holds a reading.
    inline bool CapturePredatesData(bool capturedHoldsReading, bool seriesHoldsReading)
    {
        return !capturedHoldsReading && !seriesHoldsReading;
    }

    // At restore (untick, or a save): whether the captured value is written
    // back into the slot. False only for a capture that predates the data of
    // a series that holds data now; the slot then keeps its current value.
    inline bool RestoresCapture(bool capturePredatesData, bool seriesHoldsReadingNow)
    {
        return !(capturePredatesData && seriesHoldsReadingNow);
    }
}

#endif // BLACKOVERRIDERULE_H_INCLUDED
