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

// ArchiveExtension.h: the length-prefixed extension trailer that ends every
// versioned block in a .chc / .thc stream (issue #193).
//
// CArchive cannot seek, but it can read and discard by byte count. So each
// block, once at its trailer version, ends with:
//
//     DWORD  cbExtension          byte count of everything that follows; 0 = empty
//       entry, repeated until cbExtension bytes are consumed:
//         BYTE   tag              1..254 (0 and 255 reserved; never emitted)
//         DWORD  cbValue          byte count of the value
//         BYTE   value[cbValue]
//
// All integers are little-endian exactly as CArchive writes them. A reader
// that meets a tag it does not know skips cbValue bytes and carries on, so a
// newer build can add fields without ever again bumping a block version.
// Entries are emitted in ascending tag order; a tag appears at most once per
// trailer; readers must not rely on either. Tag numbers live in ArchiveTags.h.
//
// Values are built with an ordinary storing CArchive over a CMemFile, so
// Matrix::Serialize, ar << CString and friends are reused verbatim: there is
// no second encoding.
//
// Truncation is an error. A trailer or value that claims more bytes than the
// stream holds throws CArchiveException::endOfFile; a trailer whose entries
// do not fit inside cbExtension throws CArchiveException::badSchema.

#ifndef ARCHIVEEXTENSION_H_INCLUDED
#define ARCHIVEEXTENSION_H_INCLUDED

#include "libHCFR_Config.h"

#ifdef LIBHCFR_HAS_MFC

#include <afx.h>
#include <vector>

// Read-and-discard n bytes from a loading archive. Throws
// CArchiveException::endOfFile if fewer than n are available. This is the
// only place in the tree that calls CArchive::Read; keep it that way.
void SkipArchiveBytes(CArchive& ar, DWORD n);

// Collects tagged entries, then emits DWORD cb + entries. Usage:
//
//     CExtensionWriter ext;
//     ext.BeginTag(COLOR_RAW_XYZ); m_pRaw->Serialize(ext.Archive()); ext.EndTag();
//     ext.Write(archive);              // always, even when no tag was begun
//
// BeginTag returns the storing archive for the new value; the caller writes
// with << / Serialize and then calls EndTag. Write emits the whole trailer
// and must be called exactly once, after the last EndTag.
class CExtensionWriter
{
public:
	CExtensionWriter();
	~CExtensionWriter();

	CArchive& BeginTag(BYTE tag);
	void      EndTag();
	void      Write(CArchive& ar);

private:
	struct Entry
	{
		BYTE              tag;
		std::vector<BYTE> value;
	};

	std::vector<Entry> m_entries;
	CMemFile*          m_pMem;			// value under construction
	CArchive*          m_pArchive;		// storing archive over m_pMem
	BYTE               m_openTag;		// 0 when no tag is open

	CExtensionWriter(const CExtensionWriter&);
	CExtensionWriter& operator=(const CExtensionWriter&);
};

// Reads DWORD cb, then iterates entries. Usage:
//
//     CExtensionReader ext(archive);   // reads cbExtension immediately
//     BYTE tag; DWORD cb;
//     while ( ext.Next(tag, cb) )
//         switch ( tag ) { case ...: ... ext.Value() ...; break; default: ext.Skip(); }
//     ext.Close();                     // discards whatever was not asked for
//
// Value() reads exactly cb bytes and returns a loading archive over them,
// owned by the reader until the next Next() or Close(); reading past the end
// of that archive throws endOfFile like any other short read. An entry whose
// value was neither read nor skipped is skipped by the next Next() or by
// Close().
class CExtensionReader
{
public:
	explicit CExtensionReader(CArchive& ar);
	~CExtensionReader();

	bool      Next(BYTE& tag, DWORD& cbValue);
	CArchive& Value();
	void      Skip();
	void      Close();

private:
	void ReleaseValue();
	void ConsumePending();

	CArchive& m_ar;
	DWORD     m_cbExtension;		// as read from the stream
	DWORD     m_consumed;			// bytes of the region consumed so far
	DWORD     m_cbCurrent;			// cbValue of the current entry
	bool      m_pending;			// current entry's value not yet read or skipped
	bool      m_closed;

	BYTE*     m_pBuffer;			// current value, when Value() was called
	CMemFile* m_pMem;
	CArchive* m_pValue;

	CExtensionReader(const CExtensionReader&);
	CExtensionReader& operator=(const CExtensionReader&);
};

// Thrown by a block reader when the stream's block version is one this build
// does not know: either newer than this build (a file written by a newer
// HCFR), or an unreleased development layout that is rejected rather than
// parsed (m_unreleased). Carries facts only: libHCFR has no string resources,
// so each application surface composes its own message. m_cause stays
// badSchema so a surface that does not recognise the subclass behaves as it
// always has.
class CNewerSchemaException : public CArchiveException
{
	DECLARE_DYNAMIC(CNewerSchemaException)
public:
	CNewerSchemaException(LPCSTR block, int version, bool unreleased = false);

	CString m_block;
	int     m_version;
	bool    m_unreleased;
};

#endif // LIBHCFR_HAS_MFC

#endif // ARCHIVEEXTENSION_H_INCLUDED
