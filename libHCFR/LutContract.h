/////////////////////////////////////////////////////////////////////////////
//
//  This file is subject to the terms of the GNU General Public License as
//  published by the Free Software Foundation.  A copy of this license is
//  included with this software distribution in the file COPYING.htm. If you
//  do not have a copy, you may obtain a copy by writing to the Free
//  Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  This software is distributed in the hope that it is useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details
/////////////////////////////////////////////////////////////////////////////

// LutContract: the typed insertion-point contract a LUT carries - what the
// signal IS at the LUT's input and at its output (color space, transfer
// characteristic, quantization range). Composition of two LUTs type-checks
// against these instead of relying on documentation: an output that cannot
// feed the next stage's input refuses to compose. UNSPECIFIED fields act as
// wildcards so untyped legacy assets keep working while typed ones are
// protected.
//
// The contract is in-memory metadata only for now: the .cube interchange
// format has no field for it, so Read/Write do not carry it and a freshly
// read LUT is untyped until the caller stamps it.

#ifndef LUTCONTRACT_H
#define LUTCONTRACT_H

// What the signal is at one point in the chain. Every enum starts with an
// UNSPECIFIED value (0) meaning "not stated": it matches anything during
// composition type-checks.
struct LutSignalType
{
    enum ColorSpace
    {
        CS_UNSPECIFIED = 0,
        CS_BT709,
        CS_P3D65,
        CS_BT2020,
    };
    enum Transfer
    {
        TF_UNSPECIFIED = 0,
        TF_LINEAR,
        TF_GAMMA22,
        TF_GAMMA24,
        TF_BT1886,
        TF_SRGB,
        TF_PQ,
        TF_HLG,
    };
    enum Range
    {
        RANGE_UNSPECIFIED = 0,
        RANGE_FULL,
        RANGE_VIDEO,
    };

    ColorSpace colorSpace;
    Transfer   transfer;
    Range      range;

    LutSignalType()
        : colorSpace(CS_UNSPECIFIED)
        , transfer(TF_UNSPECIFIED)
        , range(RANGE_UNSPECIFIED)
    {
    }
};

// Every field inside its enum's declared value set.
inline bool ValidSignalType(const LutSignalType& s)
{
    return s.colorSpace >= LutSignalType::CS_UNSPECIFIED
        && s.colorSpace <= LutSignalType::CS_BT2020
        && s.transfer   >= LutSignalType::TF_UNSPECIFIED
        && s.transfer   <= LutSignalType::TF_HLG
        && s.range      >= LutSignalType::RANGE_UNSPECIFIED
        && s.range      <= LutSignalType::RANGE_VIDEO;
}

// Two signal types can meet at an insertion point when every field agrees
// or at least one side leaves it unspecified.
inline bool CompatibleSignalTypes(const LutSignalType& a, const LutSignalType& b)
{
    return (a.colorSpace == b.colorSpace
            || a.colorSpace == LutSignalType::CS_UNSPECIFIED
            || b.colorSpace == LutSignalType::CS_UNSPECIFIED)
        && (a.transfer == b.transfer
            || a.transfer == LutSignalType::TF_UNSPECIFIED
            || b.transfer == LutSignalType::TF_UNSPECIFIED)
        && (a.range == b.range
            || a.range == LutSignalType::RANGE_UNSPECIFIED
            || b.range == LutSignalType::RANGE_UNSPECIFIED);
}

inline bool SameSignalType(const LutSignalType& a, const LutSignalType& b)
{
    return a.colorSpace == b.colorSpace
        && a.transfer == b.transfer
        && a.range == b.range;
}

// The LUT-level contract: the signal expected at the input and the signal
// produced at the output. A default-constructed contract is fully
// unspecified (an untyped LUT).
struct LutContract
{
    LutSignalType input;
    LutSignalType output;
};

inline bool ValidContract(const LutContract& c)
{
    return ValidSignalType(c.input) && ValidSignalType(c.output);
}

#endif // LUTCONTRACT_H
