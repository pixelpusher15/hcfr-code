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

// RED-BASELINE STUB: contract-only placeholder so the T15 oracles compile
// and fail. The real implementation replaces this file entirely; see
// CubeLUT.h for the contract.

#include "CubeLUT.h"

CubeLUT::CubeLUT()
    : m_size(0)
{
    m_domainMin[0] = m_domainMin[1] = m_domainMin[2] = 0.0;
    m_domainMax[0] = m_domainMax[1] = m_domainMax[2] = 1.0;
}

bool CubeLUT::Create(int size)
{
    (void)size;
    return false;
}

bool CubeLUT::SetEntry(int r, int g, int b, const double rgb[3])
{
    (void)r; (void)g; (void)b; (void)rgb;
    return false;
}

bool CubeLUT::GetEntry(int r, int g, int b, double rgb[3]) const
{
    (void)r; (void)g; (void)b;
    rgb[0] = rgb[1] = rgb[2] = 0.0;
    return false;
}

bool CubeLUT::SetTitle(const std::string& title)
{
    (void)title;
    return false;
}

bool CubeLUT::SetDomain(const double dmin[3], const double dmax[3])
{
    (void)dmin; (void)dmax;
    return false;
}

void CubeLUT::GetDomain(double dmin[3], double dmax[3]) const
{
    for (int k = 0; k < 3; ++k)
    {
        dmin[k] = m_domainMin[k];
        dmax[k] = m_domainMax[k];
    }
}

bool CubeLUT::WriteToString(std::string& out) const
{
    out.clear();
    m_lastError = "not implemented";
    return false;
}

bool CubeLUT::ReadFromString(const std::string& text)
{
    (void)text;
    m_lastError = "not implemented";
    return false;
}

bool CubeLUT::WriteFile(const char* path) const
{
    (void)path;
    m_lastError = "not implemented";
    return false;
}

bool CubeLUT::ReadFile(const char* path)
{
    (void)path;
    m_lastError = "not implemented";
    return false;
}
