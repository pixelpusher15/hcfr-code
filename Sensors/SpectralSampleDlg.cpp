/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2008 Association Homecinema Francophone.  All rights reserved.
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
//	Ian C
/////////////////////////////////////////////////////////////////////////////

// SpectralSampleDlg.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#include "stdafx.h"
#include "SpectralSampleDlg.h"
#include "SpectralSample.h"
#include <string>

// CSpectralSampleDlg dialog

IMPLEMENT_DYNAMIC(CSpectralSampleDlg, CDialog)

CSpectralSampleDlg::CSpectralSampleDlg(CString displayName, CWnd* pParent /*=NULL*/)
	: CDialog(CSpectralSampleDlg::IDD, pParent)
{
	m_displayName = displayName;
}

CSpectralSampleDlg::~CSpectralSampleDlg()
{
}

void CSpectralSampleDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_DISPLAY_NAME, m_displayName);
	DDX_Control(pDX, IDC_DISPLAY_NAME, m_displayNameCtrl);
	DDX_CBString(pDX, IDC_DISPLAY_TECH, m_displayTech);
	DDX_Control(pDX, IDC_DISPLAY_TECH, m_displayTechCtrl);
}


BEGIN_MESSAGE_MAP(CSpectralSampleDlg, CDialog)
END_MESSAGE_MAP()


// CSpectralSampleDlg message handlers


BOOL CSpectralSampleDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	// Only Argyll's own technology IDs are offered: any other text is written to the
	// file as TECHNOLOGY "Unknown"
	std::vector<std::string> techs = SpectralSample::getDisplayTechnologies();
	// The box is narrower than the longest IDs ("Projector RGBCMY Filter Wheel"), so widen
	// the drop-down list to fit them
	CClientDC dc(&m_displayTechCtrl);
	CFont* pOldFont = dc.SelectObject(m_displayTechCtrl.GetFont());
	int listWidth = m_displayTechCtrl.GetDroppedWidth();
	for (size_t i = 0; i < techs.size(); i++)
	{
		m_displayTechCtrl.AddString(techs[i].c_str());
		listWidth = max(listWidth, (int)dc.GetTextExtent(techs[i].c_str()).cx
			+ GetSystemMetrics(SM_CXVSCROLL) + 2 * GetSystemMetrics(SM_CXEDGE));
	}
	dc.SelectObject(pOldFont);
	m_displayTechCtrl.SetDroppedWidth(listWidth);

	m_displayTechCtrl.SetFocus();
	m_displayNameCtrl.SetSel(-1, 0, FALSE);
	return FALSE;  
}

void CSpectralSampleDlg::OnCancel() 
{
	// TODO: Add extra cleanup here
	
	CDialog::OnCancel();
}

void CSpectralSampleDlg::OnOK() 
{
	UpdateData(TRUE);

	if (m_displayName.IsEmpty() || m_displayTech.IsEmpty())
	{
		CString	Title, errorMessage;
		Title.LoadString(IDS_SPECTRAL_SAMPLE);
		errorMessage.LoadString(IDS_SPECTRAL_SAMPLE_NEED_FIELDS);
		GetColorApp()->InMeasureMessageBox(errorMessage, Title, MB_OK | MB_ICONHAND);
	}
	else
	{
		CDialog::OnOK();
	}
}

