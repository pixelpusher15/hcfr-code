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

// UserPatchSetStream.h: the user patch set collection (multi-set picker) and
// its on-disk form, the CMeasure trailer tag MEASURE_USER_PATCH_SETS.
//
// CMeasure is an application class and cannot be linked into the unit tests,
// so the set structure and the codec for its tag value live here, and
// CMeasure::Serialize only hands its collection in and out.
//
// Tag value layout (ArchiveTags.h, CMeasureTag 1). All ints are 32-bit, and
// strings are CArchive CStrings:
//
//     int     setCount             >= 0
//     int     activeIndex          -1 (none) or 0..setCount-1
//     per set, setCount times:
//       CString sequenceId         the sequence file's identity; "" for a patch-list CSV
//       CString name               display name
//       CString srcPath            the file it was loaded from (informational)
//       int     bits               declared bit depth of the definition codes
//       int     range              CSV_RANGE_* of the definition codes
//       int     rowCount           0..MAX_USER_CC_PATCH_SIZE
//       per row: int patch, int r, int g, int b, CString name, int role
//       int     measureCount       0..MAX_USER_CC_PATCH_SIZE
//       int     storedCount        0..measureCount
//       per stored measure: int index (strictly ascending, < measureCount),
//                           then the CColor (CColor::Serialize, with its own trailer)
//
// Measures that are not stored are unmeasured (noDataColor). The writer emits
// the tag only when the collection is non-empty; an absent tag means no sets.
// The value has no per-set length, so a field added later cannot go inside a
// set: it goes after the last set, or in a new tag. Every count is checked
// against the bytes the value still holds, and any value outside its range
// above is a corrupt stream (CArchiveException::badSchema).

#ifndef USERPATCHSETSTREAM_H_INCLUDED
#define USERPATCHSETSTREAM_H_INCLUDED

#include "libHCFR_Config.h"
#include "Color.h"
#include <string>
#include <vector>

// A self-contained user patch set: the definition read from a patch-list CSV
// plus its own measured results, so the .chc is the record even after the
// source file changes or disappears. The document owns a collection of these
// (CMeasure::m_userPatchSets). The legacy USER band of the master array stays
// the live storage for whichever set is active ("band-as-window"), so the
// sweep, stamp, reapply and display paths need no per-set routing; the set's
// own copy is refreshed on switch and save.
struct CUserPatchSet
{
	std::string sequenceId;         // sequence-file identity; "" for a patch-list CSV
	std::string name;               // display name = source file base name
	std::string srcPath;            // original CSV path (informational; the set is self-contained)
	int bits;                       // declared bit depth (8/10)
	int range;                      // CSV_RANGE_* of the definition codes
	std::vector<CsvPatchRow> rows;  // per-patch definition: patch#, r/g/b codes, name, role
	std::vector<CColor> measures;   // measured results, one per row (noDataColor = unmeasured)
	CUserPatchSet() : bits(8), range(-1) {}
};

#ifdef LIBHCFR_HAS_MFC

#include <afx.h>

// Writes the tag value described above.
void StoreUserPatchSets(CArchive& ar, const std::vector<CUserPatchSet>& sets, int activeIndex);

// Reads the tag value into sets and activeIndex. ar must be a loading
// archive over a memory file, as CExtensionReader::Value() returns, because
// the counts are checked against the bytes it still holds. Throws
// CArchiveException for a malformed value; sets and activeIndex are the
// caller's scratch and may hold part of the value after a throw.
void LoadUserPatchSets(CArchive& ar, std::vector<CUserPatchSet>& sets, int& activeIndex);

// The whole CMeasure version-22 extension trailer, the last thing in the
// block: cbExtension, then the MEASURE_USER_PATCH_SETS tag when sets is not
// empty. Throws CArchiveException::badSchema, writing nothing, when the
// trailer would exceed kMaxExtensionBytes (ArchiveExtension.h).
void StoreMeasureTrailer(CArchive& ar, const std::vector<CUserPatchSet>& sets, int activeIndex);

// Reads the CMeasure trailer. Without the MEASURE_USER_PATCH_SETS tag, sets
// is empty and activeIndex -1; tags this build does not know are skipped.
// Same scratch rule as LoadUserPatchSets.
void LoadMeasureTrailer(CArchive& ar, std::vector<CUserPatchSet>& sets, int& activeIndex);

#endif // LIBHCFR_HAS_MFC

#endif // USERPATCHSETSTREAM_H_INCLUDED
