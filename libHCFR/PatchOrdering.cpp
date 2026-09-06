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

// NOT YET IMPLEMENTED: red-baseline stubs so the T21 oracles link and fail.
// The behavioral contract lives in PatchOrdering.h.

#include "PatchOrdering.h"

LoadModel::LoadModel()
    : kind(LOAD_LUMINANCE), gamma(2.2),
      shareR(0.2126), shareG(0.7152), shareB(0.0722),
      kw(1.0), kr(1.1), kg(1.0), kb(1.35)
{
}

bool LoadModel::Parse(const std::string&, LoadModel&)
{
    return false;
}

std::string LoadModel::Format() const
{
    return std::string();
}

double LoadModel::Load(const double[3]) const
{
    return -1.0;
}

OrderingParams::OrderingParams()
    : area(0.1), backgroundLoad(0.0), windowSeconds(45.0), latency(0.4),
      settleAfterStep(-1.0), maxStep(-1.0), targetLoad(-1.0),
      prerollId(-1), prerollCount(0)
{
}

double PatchOrdering::RowLoad(const PatchSequence&, int, const OrderingParams&)
{
    return -1.0;
}

double PatchOrdering::Dwell(const PatchSequence&, int, const OrderingParams&)
{
    return -1.0;
}

double PatchOrdering::WindowExcursion(const PatchSequence&, const OrderingParams&)
{
    return -1.0;
}

bool PatchOrdering::Order(const PatchSequence&, const OrderingParams&,
                          PatchSequence&, std::string& reason)
{
    reason = "not implemented";
    return false;
}

double PatchOrdering::DriftScore(const std::vector<double>&)
{
    return -1.0;
}
