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

// NOT YET IMPLEMENTED: red-baseline stubs so the T16 oracles link and fail.
// The behavioral contract lives in LutOps.h.

#include "LutOps.h"

ShaperCurve::ShaperCurve()
{
    for (int c = 0; c < 3; ++c)
    {
        m_domainMin[c] = 0.0;
        m_domainMax[c] = 1.0;
    }
}

bool ShaperCurve::SetChannel(int, const double*, int, double, double)
{
    return false;
}

bool ShaperCurve::IsValid() const
{
    return false;
}

int ShaperCurve::Count(int) const
{
    return 0;
}

bool ShaperCurve::GetDomain(int, double&, double&) const
{
    return false;
}

bool ShaperCurve::GetSample(int, int, double&) const
{
    return false;
}

bool ShaperCurve::Evaluate(const double[3], double out[3]) const
{
    out[0] = out[1] = out[2] = 0.0;
    return false;
}

bool ShaperCurve::EvaluateInverse(const double[3], double out[3]) const
{
    out[0] = out[1] = out[2] = 0.0;
    return false;
}

bool ComposeCube(const CubeLUT&, const CubeLUT&, int, CubeLUT&, std::string* err)
{
    if (err)
        *err = "not implemented";
    return false;
}

bool ResampleCube(const CubeLUT&, int, CubeLUT&, std::string* err)
{
    if (err)
        *err = "not implemented";
    return false;
}

bool ExtractNeutralShaper(const CubeLUT&, ShaperCurve&, std::string* err)
{
    if (err)
        *err = "not implemented";
    return false;
}

bool SplitShaperCube(const CubeLUT&, int, ShaperCurve&, CubeLUT&, std::string* err)
{
    if (err)
        *err = "not implemented";
    return false;
}
