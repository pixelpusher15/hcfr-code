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

// ArchiveExtension.cpp: RED BASELINE. The contract in ArchiveExtension.h is
// declared; nothing behind it exists yet. Every primitive says so by throwing,
// so the T19 oracle fails until the real implementation lands.

#include "ArchiveExtension.h"

#ifdef LIBHCFR_HAS_MFC

IMPLEMENT_DYNAMIC(CNewerSchemaException, CArchiveException)

CNewerSchemaException::CNewerSchemaException(LPCSTR block, int version, bool unreleased)
	: CArchiveException(CArchiveException::badSchema, NULL),
	  m_block(block), m_version(version), m_unreleased(unreleased)
{
}

static void NotImplemented()
{
	AfxThrowNotSupportedException();
}

void SkipArchiveBytes(CArchive& /*ar*/, DWORD /*n*/)
{
	NotImplemented();
}

CExtensionWriter::CExtensionWriter() : m_pMem(NULL), m_pArchive(NULL), m_openTag(0)
{
}

CExtensionWriter::~CExtensionWriter()
{
}

CArchive& CExtensionWriter::BeginTag(BYTE /*tag*/)
{
	NotImplemented();
	return *m_pArchive;
}

void CExtensionWriter::EndTag()
{
	NotImplemented();
}

void CExtensionWriter::Write(CArchive& /*ar*/)
{
	NotImplemented();
}

CExtensionReader::CExtensionReader(CArchive& ar)
	: m_ar(ar), m_cbExtension(0), m_consumed(0), m_cbCurrent(0), m_pending(false), m_closed(false),
	  m_pBuffer(NULL), m_pMem(NULL), m_pValue(NULL)
{
	NotImplemented();
}

CExtensionReader::~CExtensionReader()
{
}

bool CExtensionReader::Next(BYTE& /*tag*/, DWORD& /*cbValue*/)
{
	NotImplemented();
	return false;
}

CArchive& CExtensionReader::Value()
{
	NotImplemented();
	return *m_pValue;
}

void CExtensionReader::Skip()
{
	NotImplemented();
}

void CExtensionReader::Close()
{
	NotImplemented();
}

void CExtensionReader::ReleaseValue()
{
}

void CExtensionReader::ConsumePending()
{
}

#endif // LIBHCFR_HAS_MFC
