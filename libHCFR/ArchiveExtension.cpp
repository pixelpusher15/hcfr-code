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

// ArchiveExtension.cpp: the extension trailer primitives (issue #193).
// Wire format and usage are documented in ArchiveExtension.h.

#include "ArchiveExtension.h"

#ifdef LIBHCFR_HAS_MFC

#include <algorithm>
#include <exception>

/////////////////////////////////////////////////////////////////////////////
// CNewerSchemaException

IMPLEMENT_DYNAMIC(CNewerSchemaException, CArchiveException)

CNewerSchemaException::CNewerSchemaException(LPCSTR block, int version, bool unreleased)
	: CArchiveException(CArchiveException::badSchema, NULL),
	  m_block(block), m_version(version), m_unreleased(unreleased)
{
}

/////////////////////////////////////////////////////////////////////////////
// SkipArchiveBytes

void SkipArchiveBytes(CArchive& ar, DWORD n)
{
	BYTE buf[256];

	while ( n > 0 )
	{
		UINT chunk = ( n < sizeof buf ) ? (UINT) n : (UINT) sizeof buf;
		UINT got = ar.Read(buf, chunk);
		if ( got < chunk )
			AfxThrowArchiveException(CArchiveException::endOfFile, NULL);
		n -= chunk;
	}
}

/////////////////////////////////////////////////////////////////////////////
// CExtensionWriter

CExtensionWriter::CExtensionWriter() : m_pMem(NULL), m_pArchive(NULL), m_openTag(0)
{
}

CExtensionWriter::~CExtensionWriter()
{
	// A tag left open is a programming error; drop it rather than leak. While
	// an exception unwinds through the caller (a value's Serialize threw
	// between BeginTag and EndTag) it is not one, so do not assert then.
	ASSERT(m_openTag == 0 || std::uncaught_exception());
	if ( m_pArchive )
	{
		m_pArchive->Abort();
		delete m_pArchive;
	}
	delete m_pMem;
}

CArchive& CExtensionWriter::BeginTag(BYTE tag)
{
	ASSERT(tag != 0 && tag != 255);		// reserved, never emitted
	ASSERT(m_openTag == 0);				// EndTag the previous one first
#ifdef _DEBUG
	for ( size_t i = 0; i < m_entries.size(); i++ )
		ASSERT(m_entries[i].tag != tag);	// a tag appears at most once per trailer
#endif

	m_pMem = new CMemFile;
	m_pArchive = new CArchive(m_pMem, CArchive::store);
	m_openTag = tag;
	return *m_pArchive;
}

void CExtensionWriter::EndTag()
{
	ASSERT(m_openTag != 0);

	// An unflushed CArchive still holds bytes in its own buffer and the
	// CMemFile is short: flush and close before measuring.
	m_pArchive->Flush();
	m_pArchive->Close();
	delete m_pArchive;
	m_pArchive = NULL;

	Entry entry;
	entry.tag = m_openTag;
	ULONGLONG cb = m_pMem->GetLength();
	BYTE* pData = m_pMem->Detach();
	if ( cb > 0 )
		entry.value.assign(pData, pData + (size_t) cb);
	free(pData);						// CMemFile allocates with malloc
	delete m_pMem;
	m_pMem = NULL;

	m_entries.push_back(entry);
	m_openTag = 0;
}

static bool EntryTagLess(const std::pair<BYTE, size_t>& a, const std::pair<BYTE, size_t>& b)
{
	return a.first < b.first;
}

void CExtensionWriter::Write(CArchive& ar)
{
	ASSERT(m_openTag == 0);

	// Ascending tag order on the wire, whatever order the caller used.
	std::vector< std::pair<BYTE, size_t> > order;
	for ( size_t i = 0; i < m_entries.size(); i++ )
		order.push_back(std::make_pair(m_entries[i].tag, i));
	std::sort(order.begin(), order.end(), EntryTagLess);

	// Never emit what CExtensionReader refuses: a region over
	// kMaxExtensionBytes is badSchema on load, so it is badSchema here, thrown
	// before a single byte reaches the archive. Summed in 64 bits so a huge
	// value cannot wrap the DWORD and slip under the cap.
	ULONGLONG total = 0;
	for ( size_t i = 0; i < m_entries.size(); i++ )
		total += 1 + 4 + (ULONGLONG) m_entries[i].value.size();
	if ( total > kMaxExtensionBytes )
		AfxThrowArchiveException(CArchiveException::badSchema, NULL);

	ar << (DWORD) total;
	for ( size_t i = 0; i < order.size(); i++ )
	{
		const Entry& e = m_entries[order[i].second];
		ar << (BYTE) e.tag;
		ar << (DWORD) e.value.size();
		if ( !e.value.empty() )
			ar.Write(&e.value[0], (UINT) e.value.size());
	}
}

/////////////////////////////////////////////////////////////////////////////
// CExtensionReader

CExtensionReader::CExtensionReader(CArchive& ar)
	: m_ar(ar), m_cbExtension(0), m_consumed(0), m_cbCurrent(0), m_pending(false), m_closed(false),
	  m_pBuffer(NULL), m_pMem(NULL), m_pValue(NULL)
{
	m_ar >> m_cbExtension;

	// A corrupt length must fail as a schema error, not as a multi-gigabyte
	// allocation in Value() (a CMemoryException) or a long skip toward EOF.
	if ( m_cbExtension > kMaxExtensionBytes )
		AfxThrowArchiveException(CArchiveException::badSchema, NULL);
}

CExtensionReader::~CExtensionReader()
{
	// No Close() here: a destructor must not throw, and Close() can. A
	// reader that was never closed leaves the stream wherever it was.
	ReleaseValue();
}

void CExtensionReader::ReleaseValue()
{
	if ( m_pValue )
	{
		m_pValue->Abort();
		delete m_pValue;
		m_pValue = NULL;
	}
	if ( m_pMem )
	{
		m_pMem->Detach();
		delete m_pMem;
		m_pMem = NULL;
	}
	delete [] m_pBuffer;
	m_pBuffer = NULL;
}

// An entry whose value was neither read nor skipped is skipped now.
void CExtensionReader::ConsumePending()
{
	if ( m_pending )
		Skip();
}

bool CExtensionReader::Next(BYTE& tag, DWORD& cbValue)
{
	ASSERT(!m_closed);
	ConsumePending();
	ReleaseValue();

	if ( m_consumed >= m_cbExtension )
		return false;

	// An entry header that does not fit, or a value that overruns the region,
	// is a malformed trailer: a schema error, not a short stream.
	if ( m_cbExtension - m_consumed < 5 )
		AfxThrowArchiveException(CArchiveException::badSchema, NULL);

	m_ar >> tag;
	m_ar >> cbValue;
	m_consumed += 5;
	if ( cbValue > m_cbExtension - m_consumed )
		AfxThrowArchiveException(CArchiveException::badSchema, NULL);

	m_cbCurrent = cbValue;
	m_pending = true;
	return true;
}

CArchive& CExtensionReader::Value()
{
	ASSERT(m_pending);
	ReleaseValue();

	// Read the whole value first, so a short stream throws before the caller
	// has attached anything to the object being loaded.
	m_pBuffer = new BYTE [ m_cbCurrent ? m_cbCurrent : 1 ];
	if ( m_cbCurrent > 0 )
	{
		UINT got = m_ar.Read(m_pBuffer, m_cbCurrent);
		if ( got < m_cbCurrent )
			AfxThrowArchiveException(CArchiveException::endOfFile, NULL);
	}
	m_consumed += m_cbCurrent;
	m_pending = false;

	m_pMem = new CMemFile;
	m_pMem->Attach(m_pBuffer, m_cbCurrent);
	m_pValue = new CArchive(m_pMem, CArchive::load);
	return *m_pValue;
}

void CExtensionReader::Skip()
{
	ASSERT(m_pending);
	m_pending = false;
	SkipArchiveBytes(m_ar, m_cbCurrent);
	m_consumed += m_cbCurrent;
}

void CExtensionReader::Close()
{
	if ( m_closed )
		return;
	ConsumePending();
	ReleaseValue();
	if ( m_consumed < m_cbExtension )
		SkipArchiveBytes(m_ar, m_cbExtension - m_consumed);
	m_consumed = m_cbExtension;
	m_closed = true;
}

#endif // LIBHCFR_HAS_MFC
