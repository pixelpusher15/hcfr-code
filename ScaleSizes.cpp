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
//  Author(s):
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// ScaleSizes.cpp : implementation file
//

#include "stdafx.h"
#include "colorhcfr.h"
#include "DataSetDoc.h"
#include "Measure.h"
#include "ScaleSizes.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CScaleSizes dialog


CScaleSizes::CScaleSizes(CDataSetDoc * pDoc, CWnd* pParent /*=NULL*/)
	: CDialog(CScaleSizes::IDD, pParent)
{
	//{{AFX_DATA_INIT(CScaleSizes)
	m_NbNearBlack = 0;
	m_NbNearWhite = 0;
	m_NbSat = 0;
	//}}AFX_DATA_INIT

	m_pDoc = pDoc;
	m_NbNearBlack = m_pDoc -> GetMeasure () -> GetNearBlackScaleSize () - 1;
	m_NbNearWhite = m_pDoc -> GetMeasure () -> GetNearWhiteScaleSize () - 1;
	m_NbSat = m_pDoc -> GetMeasure () -> GetSaturationSize () - 1;
	m_bIRE = m_pDoc -> GetMeasure () -> m_bIREScaleMode;
}


void CScaleSizes::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CScaleSizes)
	DDX_Control(pDX, IDC_COMBO_GRAYS, m_ComboGrays);
	DDX_Control(pDX, IDC_COMBO_EDIT_GRAYS, m_EditGrays);
	DDX_Text(pDX, IDC_EDIT_NEARBLACK, m_NbNearBlack);
	DDV_MinMaxInt(pDX, m_NbNearBlack, 2, 50);
	DDX_Text(pDX, IDC_EDIT_NEARWHITE, m_NbNearWhite);
	DDV_MinMaxInt(pDX, m_NbNearWhite, 2, 50);
	DDX_Text(pDX, IDC_EDIT_SAT, m_NbSat);
	DDV_MinMaxInt(pDX, m_NbSat, 2, 50);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CScaleSizes, CDialog)
	//{{AFX_MSG_MAP(CScaleSizes)
	ON_CBN_SELCHANGE(IDC_COMBO_GRAYS, OnSelChangeGrays)
	ON_BN_CLICKED(IDHELP, OnHelp)
	ON_WM_HELPINFO()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CScaleSizes message handlers

BOOL CScaleSizes::OnInitDialog()
{
	CString	str;

	CDialog::OnInitDialog();

	// Populate the preset dropdown, then append the "Custom..." entry.
	const GrayScalePreset *	pPresets = GetGrayScalePresets();
	int						nPresetCount = GetGrayScalePresetCount();
	for (int k = 0; k < nPresetCount; k++)
		m_ComboGrays.AddString ( pPresets[k].name );
	m_ComboGrays.AddString ( "Custom..." );	// last item == custom
	m_ComboGrays.SetDroppedWidth ( 220 );	// keep the longer preset names readable

	// Select the preset matching the current measure, else "Custom..." with its N.
	int nMatch = m_pDoc -> GetMeasure () -> GetGrayScalePreset ();
	m_ComboGrays.SetCurSel ( nMatch >= 0 ? nMatch : nPresetCount );

	str.Format ( "%d", m_pDoc -> GetMeasure () -> GetGrayScaleSize () - 1 );	// step count for Custom
	m_EditGrays.SetWindowText ( str );

	UpdateGrayControls ();

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CScaleSizes::UpdateGrayControls()
{
	BOOL bCustom = ( m_ComboGrays.GetCurSel () == GetGrayScalePresetCount () );	// "Custom..." is the last item
	m_EditGrays.ShowWindow ( bCustom ? SW_SHOW : SW_HIDE );
	m_EditGrays.EnableWindow ( bCustom );
}

void CScaleSizes::OnSelChangeGrays()
{
	UpdateGrayControls ();
}

void CScaleSizes::OnOK()
{
	if ( ! UpdateData ( TRUE ) )	// pull + validate near black/white/sat (DDV)
		return;

	const GrayScalePreset *	pPresets = GetGrayScalePresets();
	int						nPresetCount = GetGrayScalePresetCount();
	int						sel = m_ComboGrays.GetCurSel ();

	CArray<double,double>	levels;
	int						nCustomSteps = 0;	// stored for cross-version "Gray" / custom-N

	if ( sel >= 0 && sel < nPresetCount )
	{
		// Preset: copy its explicit level array.
		levels.SetSize ( pPresets[sel].count );
		for (int i = 0; i < pPresets[sel].count; i++)
			levels[i] = pPresets[sel].levels[i];
		nCustomSteps = pPresets[sel].count - 1;
	}
	else
	{
		// Custom: N evenly-spaced steps 0..100 -> N+1 points.
		CString	str;
		m_EditGrays.GetWindowText ( str );
		int N = atoi ( (LPCSTR) str );
		if ( N < 4 || N > 100 )
		{
			AfxMessageBox ( "Please enter a custom number of grayscale steps between 4 and 100." );
			m_EditGrays.SetFocus ();
			return;	// keep the dialog open
		}
		levels.SetSize ( N + 1 );
		for (int i = 0; i <= N; i++)
			levels[i] = i * 100.0 / N;
		nCustomSteps = N;
	}

	m_pDoc -> GetMeasure () -> SetIREScaleMode ( m_bIRE );
	m_pDoc -> GetMeasure () -> SetGrayScaleLevels ( levels.GetData (), (int)levels.GetSize () );
	m_pDoc -> GetMeasure () -> SetNearBlackScaleSize ( m_NbNearBlack + 1 );
	m_pDoc -> GetMeasure () -> SetNearWhiteScaleSize ( m_NbNearWhite + 1 );
	m_pDoc -> GetMeasure () -> SetSaturationSize ( m_NbSat + 1 );

	GetConfig()->WriteProfileInt("References","IRELevels",m_bIRE);
	GetConfig()->WriteProfileInt("Scale Sizes","GrayPreset",( sel >= 0 && sel < nPresetCount ) ? sel : -1);
	GetConfig()->WriteProfileInt("Scale Sizes","GrayCustomN",nCustomSteps);
	GetConfig()->WriteProfileInt("Scale Sizes","Gray",(int)levels.GetSize() - 1);	// legacy/cross-version
	GetConfig()->WriteProfileInt("Scale Sizes","Near Black",m_NbNearBlack);
	GetConfig()->WriteProfileInt("Scale Sizes","Near White",m_NbNearWhite);
	GetConfig()->WriteProfileInt("Scale Sizes","Saturations",m_NbSat);

	m_pDoc -> UpdateAllViews ( NULL );

	CDialog::OnOK();
}

void CScaleSizes::OnCancel()
{
	CDialog::OnCancel();
}

void CScaleSizes::OnHelp()
{
	GetConfig () -> DisplayHelp ( HID_SCALESIZES, NULL );
}

BOOL CScaleSizes::OnHelpInfo(HELPINFO* pHelpInfo)
{
	OnHelp ();
	return TRUE;
}
