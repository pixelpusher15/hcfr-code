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

// UserPatchSetStream.cpp: the MEASURE_USER_PATCH_SETS tag value codec.
// Layout and contract are documented in UserPatchSetStream.h.

#include "UserPatchSetStream.h"

#ifdef LIBHCFR_HAS_MFC

#include "ArchiveExtension.h"
#include "ArchiveTags.h"
#include <limits.h>

namespace
{
	// The fewest bytes a set, a row and a stored measure can take on the
	// wire: three empty CStrings (one length byte each) and five ints; four
	// ints, an empty CString and an int; the index and a CColor's version.
	// A count that could not fit in what is left of the value is corrupt and
	// is refused before anything is allocated for it.
	const ULONGLONG kMinSetBytes     = 3 + 5 * 4;
	const ULONGLONG kMinRowBytes     = 4 * 4 + 1 + 4;
	const ULONGLONG kMinMeasureBytes = 4 + 4;

	// Bytes the loading archive has not yet handed out. Flush gives the
	// archive's unread buffer back to the file, so the file position then
	// counts exactly what has been consumed.
	ULONGLONG Remaining(CArchive& ar)
	{
		ar.Flush();
		CFile* f = ar.GetFile();
		return f->GetLength() - f->GetPosition();
	}

	void Corrupt()
	{
		AfxThrowArchiveException(CArchiveException::badSchema, NULL);
	}

	void ReadCount(CArchive& ar, int& n, int maxCount, ULONGLONG minBytesEach)
	{
		ar >> n;
		if ( n < 0 || n > maxCount || (ULONGLONG) n * minBytesEach > Remaining(ar) )
			Corrupt();
	}

	std::string ReadString(CArchive& ar)
	{
		CString s;
		ar >> s;
		return std::string((LPCTSTR) s);
	}
}

void StoreUserPatchSets(CArchive& ar, const std::vector<CUserPatchSet>& sets, int activeIndex)
{
	ar << (int) sets.size();
	ar << activeIndex;
	for ( size_t s = 0; s < sets.size(); s++ )
	{
		const CUserPatchSet& set = sets[s];
		ar << CString(set.sequenceId.c_str());
		ar << CString(set.name.c_str());
		ar << CString(set.srcPath.c_str());
		ar << set.bits;
		ar << set.range;

		ar << (int) set.rows.size();
		for ( size_t i = 0; i < set.rows.size(); i++ )
		{
			const CsvPatchRow& row = set.rows[i];
			ar << row.patch << row.r << row.g << row.b;
			ar << CString(row.name.c_str());
			ar << row.role;
		}

		int nStored = 0;
		for ( size_t i = 0; i < set.measures.size(); i++ )
			if ( set.measures[i].isValid() )
				nStored++;
		ar << (int) set.measures.size();
		ar << nStored;
		for ( size_t i = 0; i < set.measures.size(); i++ )
		{
			if ( set.measures[i].isValid() )
			{
				ar << (int) i;
				CColor c(set.measures[i]);	// CColor::Serialize is not const
				c.Serialize(ar);
			}
		}
	}
}

void LoadUserPatchSets(CArchive& ar, std::vector<CUserPatchSet>& sets, int& activeIndex)
{
	int nSets = 0;
	ReadCount(ar, nSets, INT_MAX, kMinSetBytes);
	ar >> activeIndex;
	if ( activeIndex < -1 || activeIndex >= nSets )
		Corrupt();

	sets.clear();
	sets.resize(nSets);
	for ( int s = 0; s < nSets; s++ )
	{
		CUserPatchSet& set = sets[s];
		set.sequenceId = ReadString(ar);
		set.name = ReadString(ar);
		set.srcPath = ReadString(ar);
		ar >> set.bits;
		ar >> set.range;

		int nRows = 0;
		ReadCount(ar, nRows, MAX_USER_CC_PATCH_SIZE, kMinRowBytes);
		set.rows.resize(nRows);
		for ( int i = 0; i < nRows; i++ )
		{
			CsvPatchRow& row = set.rows[i];
			ar >> row.patch >> row.r >> row.g >> row.b;
			row.name = ReadString(ar);
			ar >> row.role;
		}

		int nMeasures = 0;
		ar >> nMeasures;
		if ( nMeasures < 0 || nMeasures > MAX_USER_CC_PATCH_SIZE )
			Corrupt();
		int nStored = 0;
		ReadCount(ar, nStored, nMeasures, kMinMeasureBytes);
		set.measures.assign(nMeasures, noDataColor);
		int previous = -1;
		for ( int k = 0; k < nStored; k++ )
		{
			int index;
			ar >> index;
			if ( index <= previous || index >= nMeasures )
				Corrupt();
			previous = index;
			set.measures[index].Serialize(ar);
		}
	}
}

void StoreMeasureTrailer(CArchive& ar, const std::vector<CUserPatchSet>& sets, int activeIndex)
{
	CExtensionWriter ext;
	if ( ! sets.empty() )
	{
		StoreUserPatchSets(ext.BeginTag(ArchiveTag::MEASURE_USER_PATCH_SETS), sets, activeIndex);
		ext.EndTag();
	}
	ext.Write(ar);
}

void LoadMeasureTrailer(CArchive& ar, std::vector<CUserPatchSet>& sets, int& activeIndex)
{
	sets.clear();
	activeIndex = -1;

	CExtensionReader ext(ar);
	BYTE  tag;
	DWORD cb;
	while ( ext.Next(tag, cb) )
	{
		switch ( tag )
		{
			case ArchiveTag::MEASURE_USER_PATCH_SETS:
				LoadUserPatchSets(ext.Value(), sets, activeIndex);
				break;
			default:
				ext.Skip();
				break;
		}
	}
	ext.Close();
}

#endif // LIBHCFR_HAS_MFC
