/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2011 Association Homecinema Francophone.  All rights reserved.
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

// ExportOptionsDialog.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "ExportOptionsDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CExportOptionsDialog dialog


CExportOptionsDialog::CExportOptionsDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CExportOptionsDialog::IDD, pParent)
{
	//{{AFX_DATA_INIT(CExportOptionsDialog)
	m_bExportRaw = TRUE;
	m_bExportStimulus = TRUE;
	//}}AFX_DATA_INIT
}


void CExportOptionsDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CExportOptionsDialog)
	DDX_Check(pDX, IDC_CHECK_EXPORT_RAW, m_bExportRaw);
	DDX_Check(pDX, IDC_CHECK_EXPORT_STIMULUS, m_bExportStimulus);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CExportOptionsDialog, CDialog)
	//{{AFX_MSG_MAP(CExportOptionsDialog)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
