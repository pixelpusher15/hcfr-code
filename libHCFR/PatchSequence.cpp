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

// NOT YET IMPLEMENTED: red-baseline stubs so the T17 oracles link and fail.
// The behavioral contract lives in PatchSequence.h.

#include "PatchSequence.h"

PatchSequence::PatchSequence()
{
}

int PatchSequence::Count() const
{
    return 0;
}

bool PatchSequence::GetPatch(int, double[3], std::string*) const
{
    return false;
}

bool PatchSequence::AddPatch(double, double, double, const std::string&)
{
    return false;
}

bool PatchSequence::AddGrayRamp(int, double, double)
{
    return false;
}

bool PatchSequence::AddChannelRamp(int, int, double, double)
{
    return false;
}

bool PatchSequence::AddCubeGrid(int)
{
    return false;
}

bool PatchSequence::AddCubeShell(int)
{
    return false;
}

int PatchSequence::Deduplicate()
{
    return 0;
}

void PatchSequence::Clear()
{
}

bool PatchSequence::WriteCsv(std::string&, int, int) const
{
    m_lastError = "not implemented";
    return false;
}

bool PatchSequence::WriteCsvFile(const char*, int, int) const
{
    m_lastError = "not implemented";
    return false;
}
