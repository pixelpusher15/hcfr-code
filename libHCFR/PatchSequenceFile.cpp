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

// NOT YET IMPLEMENTED: red-baseline stubs so the T20 oracles link and fail.
// The behavioral contract lives in PatchSequenceFile.h.

#include "PatchSequenceFile.h"

PatchSequenceHeader::PatchSequenceHeader()
    : version(1), bits(8), range(1),
      geometryArea(-1), geometryBackground(-1),
      targetLoad(-1.0), window(-1.0), dwellLatency(-1.0), maxStep(-1.0),
      settleAfterStep(-1.0), ablLimit(-1.0),
      prerollId(-1), prerollCount(-1)
{
}

bool PatchSequenceFile::Read(const char*, PatchSequence&, PatchSequenceHeader&, Dialect*)
{
    m_lastError = "not implemented";
    return false;
}

bool PatchSequenceFile::ParseHps(const std::string&, PatchSequence&, PatchSequenceHeader&)
{
    m_lastError = "not implemented";
    return false;
}

bool PatchSequenceFile::FormatHps(std::string&, const PatchSequence&, const PatchSequenceHeader&)
{
    m_lastError = "not implemented";
    return false;
}

bool PatchSequenceFile::Write(const char*, const PatchSequence&, const PatchSequenceHeader&)
{
    m_lastError = "not implemented";
    return false;
}

bool PatchSequenceFile::ExportCsv(std::string&, const PatchSequence&, int, int, ExportOrder, bool)
{
    m_lastError = "not implemented";
    return false;
}

bool PatchSequenceFile::ExportCsvFile(const char*, const PatchSequence&, int, int, ExportOrder, bool)
{
    m_lastError = "not implemented";
    return false;
}
