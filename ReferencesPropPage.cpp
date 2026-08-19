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
//	Fran�ois-Xavier CHABOUD
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// ReferencesPropPage.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "ReferencesPropPage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CReferencesPropPage property page

IMPLEMENT_DYNCREATE(CReferencesPropPage, CPropertyPageWithHelp)

CReferencesPropPage::CReferencesPropPage() : CPropertyPageWithHelp(CReferencesPropPage::IDD)
{
	//{{AFX_DATA_INIT(CReferencesPropPage)
	m_whiteTarget = 0;
	m_colorStandard = 1;
	m_CCMode = GCD;
	m_GammaRef = 2.2;
	m_GammaAvg = 2.2;
	m_changeWhiteCheck = FALSE;
	m_userBlack = FALSE;
	m_bOverRideTargs = FALSE;
	m_useToneMap = FALSE;
	m_ManualBlack = 0.0;
	m_useMeasuredGamma = FALSE;
	m_GammaOffsetType = 4;
	m_manualGOffset = 0.099;
    m_manualWhitex = 0.3127;
    m_manualWhitey = 0.3290;
	m_MasterMinL = 0.0;
	m_MasterMaxL = 4000.0;
	m_ContentMaxL = 4000.0;
	m_FrameAvgMaxL = 400.0;
	m_TargetMinL = 0.00;
	m_TargetSysGamma = 1.20;
	m_BT2390_BS = 1.0;
	m_BT2390_WS = 0.0;
	m_BT2390_WS1 = 25;
	m_TargetSysGammaUser = m_TargetSysGamma;
	m_BT2390_BSUser = m_BT2390_BS;
	m_BT2390_WS1User = m_BT2390_WS1;
	m_BT2390_WSUser = m_BT2390_WS;
	m_TargetMinLUser = m_TargetMinL;
	m_TargetMaxL = 120.;
	m_TargetMaxLUser = m_TargetMaxL;
	m_DiffuseL = 94.37844;
	m_DiffuseLUser = m_DiffuseL;
    //}}AFX_DATA_INIT

	m_isModified=FALSE;
	m_pCdm2 = NULL;
	m_pTFGroup = NULL;
	m_pCCGroup = NULL;
	m_pTargetGroup = NULL;
}

CReferencesPropPage::~CReferencesPropPage()
{
}

void CReferencesPropPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPageWithHelp::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CReferencesPropPage)
	DDX_Control(pDX, IDC_EDIT_GAMMA_REF, m_GammaRefEdit);
	DDX_Control(pDX, IDC_EDIT_GAMMA_AVERAGE, m_GammaAvgEdit);
	DDX_Control(pDX, IDC_EDIT_GAMMA_REL, m_GammaRelEdit);
	DDX_Control(pDX, IDC_EDIT_MANUAL_BLACK, m_ManualBlackEdit);
	DDX_Control(pDX, IDC_EDIT_SPLIT, m_SplitEdit);
	DDX_Control(pDX, IDC_WHITETARGET_COMBO, m_whiteTargetCombo);
	DDX_CBIndex(pDX, IDC_WHITETARGET_COMBO, m_whiteTarget);
	DDX_CBIndex(pDX, IDC_COLORREF_COMBO, m_colorStandard);
	DDX_CBIndex(pDX, IDC_CCMODE_COMBO, m_CCMode);
  	DDX_Text(pDX, IDC_EDIT_GAMMA_REF, m_GammaRef);
  	DDX_Text(pDX, IDC_EDIT_GAMMA_REL, m_GammaRel);
  	DDX_Text(pDX, IDC_EDIT_MANUAL_BLACK, m_ManualBlack);
  	DDX_Text(pDX, IDC_EDIT_SPLIT, m_Split);
  	DDX_Text(pDX, IDC_EDIT_GAMMA_AVERAGE, m_GammaAvg);
  	DDX_Text(pDX, IDC_EDIT_DIFFUSE_WHITE, m_DiffuseL);
  	DDX_Control(pDX, IDC_EDIT_DIFFUSE_WHITE, m_DiffuseLCtrl);
	DDV_MinMaxDouble(pDX, m_DiffuseL, 1., 200.);
	DDX_Text(pDX, IDC_EDIT_MASTER_MINL, m_MasterMinL);
	DDX_Control(pDX, IDC_EDIT_MASTER_MINL, m_MasterMinLCtrl);
	DDV_MinMaxDouble(pDX, m_MasterMinL, 0., 0.5);
	DDX_Text(pDX, IDC_EDIT_MASTER_MAXL, m_MasterMaxL);
	DDX_Control(pDX, IDC_EDIT_MASTER_MAXL, m_MasterMaxLCtrl);
	DDV_MinMaxDouble(pDX, m_MasterMaxL, 100., 10000.);
	DDX_Text(pDX, IDC_EDIT_CONTENT_MAXL, m_ContentMaxL);
	DDX_Control(pDX, IDC_EDIT_CONTENT_MAXL, m_ContentMaxLCtrl);
	DDV_MinMaxDouble(pDX, m_ContentMaxL, 100., 10000.);
	DDX_Control(pDX, IDC_EDIT_FRAME_AVG_MAXL, m_FrameAvgMaxLCtrl);
	DDX_Text(pDX, IDC_EDIT_FRAME_AVG_MAXL, m_FrameAvgMaxL);
	DDV_MinMaxDouble(pDX, m_FrameAvgMaxL, 100., 10000.);
  	DDX_Text(pDX, IDC_EDIT_TARGET_MINL, m_TargetMinL);
  	DDX_Control(pDX, IDC_EDIT_TARGET_MINL, m_TargetMinLCtrl);
	DDV_MinMaxDouble(pDX, m_TargetMinL, 0., 100.);
  	DDX_Text(pDX, IDC_EDIT_TARGET_MAXL, m_TargetMaxL);
  	DDX_Control(pDX, IDC_EDIT_TARGET_MAXL, m_TargetMaxLCtrl);
	DDV_MinMaxDouble(pDX, m_TargetMaxL, 1., 10000.);
  	DDX_Text(pDX, IDC_EDIT_TARGET_MAXL2, m_TargetSysGamma);
  	DDX_Control(pDX, IDC_EDIT_TARGET_MAXL2, m_TargetSysGammaCtrl);
  	DDX_Text(pDX, IDC_EDIT_TARGET_MAXL3, m_BT2390_BS);
	DDV_MinMaxDouble(pDX, m_BT2390_BS, 0.1, 3.0);
  	DDX_Control(pDX, IDC_EDIT_TARGET_MAXL3, m_BT2390_BSCtrl);
  	DDX_Text(pDX, IDC_EDIT_TARGET_MAXL4, m_BT2390_WS);
	DDV_MinMaxDouble(pDX, m_BT2390_WS, -4.5, 4.5);
  	DDX_Control(pDX, IDC_EDIT_TARGET_MAXL4, m_BT2390_WSCtrl);
  	DDX_Text(pDX, IDC_EDIT_TARGET_MAXL5, m_BT2390_WS1);
	DDV_MinMaxDouble(pDX, m_BT2390_WS1, 0, 50);
  	DDX_Control(pDX, IDC_EDIT_TARGET_MAXL5, m_BT2390_WS1Ctrl);
	DDV_MinMaxDouble(pDX, m_TargetSysGamma, 0.1, 2.0);
	DDV_MinMaxDouble(pDX, m_GammaRef, 1., 5.);
	DDV_MinMaxDouble(pDX, m_GammaRel, 0., 5.);
	DDV_MinMaxDouble(pDX, m_Split, 0., 100.);
	DDV_MinMaxDouble(pDX, m_ManualBlack, 0., 1.);
	DDX_Check(pDX, IDC_USE_MEASURED_GAMMA, m_useMeasuredGamma);
	DDX_Check(pDX, IDC_USER_BLACK, m_userBlack);
	DDX_Control(pDX, IDC_USER_OVERRIDE_TARGS, m_bOverRideTargsCtrl);
	DDX_Check(pDX, IDC_USER_OVERRIDE_TARGS, m_bOverRideTargs);
	DDX_Control(pDX, IDC_USE_TONEMAP, m_useToneMapCtrl);
	DDX_Check(pDX, IDC_USE_TONEMAP, m_useToneMap);
	DDX_Control(pDX, IDC_USE_MEASURED_GAMMA, m_eMeasuredGamma);
	DDX_Radio(pDX, IDC_GAMMA_OFFSET_RADIO1, m_GammaOffsetType);
	DDX_Text(pDX, IDC_EDIT_MANUAL_GOFFSET, m_manualGOffset);
	DDX_Text(pDX, IDC_WHITE_X, m_manualWhitex);
	DDX_Text(pDX, IDC_WHITE_Y, m_manualWhitey);
	DDX_Text(pDX, IDC_RED_X, m_manualRedx);
	DDX_Text(pDX, IDC_RED_Y, m_manualRedy);
	DDX_Text(pDX, IDC_GREEN_X, m_manualGreenx);
	DDX_Text(pDX, IDC_GREEN_Y, m_manualGreeny);
	DDX_Text(pDX, IDC_BLUE_X, m_manualBluex);
	DDX_Text(pDX, IDC_BLUE_Y, m_manualBluey);
	DDV_MinMaxDouble(pDX, m_manualWhitex, .1, .9);
	DDV_MinMaxDouble(pDX, m_manualWhitey, .1, .9);
	DDV_MinMaxDouble(pDX, m_manualRedx, .1, .9);
	DDV_MinMaxDouble(pDX, m_manualRedy, .1, .9);
	DDV_MinMaxDouble(pDX, m_manualGreenx, .1, .9);
	DDV_MinMaxDouble(pDX, m_manualGreeny, .1, .9);
	DDV_MinMaxDouble(pDX, m_manualBluex, .001, .9);
	DDV_MinMaxDouble(pDX, m_manualBluey, .001, .9);
	DDX_Control(pDX, IDC_WHITE_X, m_manualWhitexedit);
	DDX_Control(pDX, IDC_WHITE_Y, m_manualWhiteyedit);
	DDX_Control(pDX, IDC_RED_X, m_manualRedxedit);
	DDX_Control(pDX, IDC_RED_Y, m_manualRedyedit);
	DDX_Control(pDX, IDC_GREEN_X, m_manualGreenxedit);
	DDX_Control(pDX, IDC_GREEN_Y, m_manualGreenyedit);
	DDX_Control(pDX, IDC_BLUE_X, m_manualBluexedit);
	DDX_Control(pDX, IDC_BLUE_Y, m_manualBlueyedit);
	DDV_MinMaxDouble(pDX, m_manualGOffset, 0., 0.2);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CReferencesPropPage, CPropertyPageWithHelp)
	//{{AFX_MSG_MAP(CReferencesPropPage)
	ON_BN_CLICKED(IDC_CHECK_COLORS, OnCheckColors)
	ON_EN_CHANGE(IDC_EDIT_IRIS_TIME, OnChangeEditIrisTime)
	ON_EN_CHANGE(IDC_EDIT_GAMMA_REF, OnChangeEditGammaRef)
	ON_EN_CHANGE(IDC_EDIT_GAMMA_REL, OnChangeEditGammaRel)
	ON_EN_CHANGE(IDC_EDIT_MANUAL_BLACK, OnChangeEditManualBlack)
	ON_EN_CHANGE(IDC_EDIT_SPLIT, OnChangeEditGammaRel)
	ON_EN_CHANGE(IDC_EDIT_GAMMA_AVERAGE, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_WHITE_X, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_WHITE_Y, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_RED_X, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_RED_Y, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_GREEN_X, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_GREEN_Y, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_BLUE_X, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_BLUE_Y, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_EDIT_DIFFUSE_WHITE, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_EDIT_MASTER_MINL, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_EDIT_MASTER_MAXL, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_EDIT_CONTENT_MAXL, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_EDIT_FRAME_AVG_MAXL, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_EDIT_TARGET_MINL, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_EDIT_TARGET_MAXL, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_EDIT_TARGET_MAXL2, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_EDIT_TARGET_MAXL3, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_EDIT_TARGET_MAXL4, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_EDIT_TARGET_MAXL5, OnChangeEditGammaAvg)
	ON_BN_CLICKED(IDC_USE_MEASURED_GAMMA, OnUseMeasuredGammaCheck)
	ON_BN_CLICKED(IDC_USER_BLACK, OnUserBlackCheck)
	ON_BN_CLICKED(IDC_USER_OVERRIDE_TARGS, OnUserOverRideTargsCheck)
	ON_BN_CLICKED(IDC_USE_TONEMAP, OnUserBlackCheck)
	ON_CBN_SELCHANGE(IDC_COLORREF_COMBO, OnSelchangeColorrefCombo)
	ON_CBN_SELCHANGE(IDC_WHITETARGET_COMBO, OnSelchangeWhiteCombo)
	ON_CBN_SELCHANGE(IDC_CCMODE_COMBO, OnSelchangeCCmodeCombo)
	ON_CBN_SELCHANGE(IDC_TRANSFERFUNC_COMBO, OnSelchangeTransferFuncCombo)
	ON_EN_CHANGE(IDC_EDIT_MANUAL_GOFFSET, OnChangeEditManualGOffset)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CReferencesPropPage message handlers

void CReferencesPropPage::OnCheckColors() 
{
	m_isModified=TRUE;
	SetModified(TRUE);	
}

BOOL CReferencesPropPage::OnApply() 
{
	if (m_TargetMinL >= m_TargetMaxL)
	{
		m_TargetMinL = 0;
		GetConfig()->m_TargetMinL = 0;
		GetColorApp()->InMeasureMessageBox("Minumum Target Luminance must be greater than Maximum, setting to 0.",MB_OK); 
	}

	GetConfig()->	WriteProfileDouble("References","Manual Black Level",m_ManualBlack);
	GetConfig()->	WriteProfileDouble("References","Use Black Level",m_userBlack);
	GetConfig()->	WriteProfileDouble("References","Override Targets",m_bOverRideTargs);
	GetConfig()->	WriteProfileDouble("References","DiffuseLValue",m_DiffuseLUser);
	GetConfig()->	WriteProfileDouble("References","TargetMinLValue",m_TargetMinLUser);
	GetConfig()->	WriteProfileDouble("References","TargetMaxLValue",m_TargetMaxLUser);
	GetConfig()->	WriteProfileDouble("References","TargetSysGamma",m_TargetSysGamma);
	GetConfig()->	WriteProfileDouble("References","BT2390BlackSlopeFactor",m_BT2390_BS);
	GetConfig()->	WriteProfileDouble("References","BT2390WhiteSlopeFactor",m_BT2390_WS);
	GetConfig()->	WriteProfileDouble("References","BT2390WhiteSlopeFactor1",m_BT2390_WS1);
	GetConfig()->	WriteProfileDouble("References","TargetSysGammaUser",m_TargetSysGammaUser);
	if (m_colorStandard == HDTVa || m_colorStandard == HDTVb || m_colorStandard == CC6) 
	{
		m_changeWhiteCheck = FALSE;
        m_whiteTargetCombo.EnableWindow (FALSE);
        m_manualWhitexedit.EnableWindow (FALSE);
        m_manualWhiteyedit.EnableWindow (FALSE);        
		m_whiteTarget=(int)(GetStandardColorReference((ColorStandard)(m_colorStandard)).m_white);	
	} 

    if (m_whiteTarget == DCUST)
    {
        m_manualWhitexedit.EnableWindow (TRUE);
        m_manualWhiteyedit.EnableWindow (TRUE);
    }
    else
    {
        m_manualWhitexedit.EnableWindow (FALSE);
        m_manualWhiteyedit.EnableWindow (FALSE);
    }
    if (m_colorStandard == CUSTOM)
    {
        m_manualRedxedit.EnableWindow (TRUE);
        m_manualRedyedit.EnableWindow (TRUE);
        m_manualGreenxedit.EnableWindow (TRUE);
        m_manualGreenyedit.EnableWindow (TRUE);
        m_manualBluexedit.EnableWindow (TRUE);
        m_manualBlueyedit.EnableWindow (TRUE);
    }
    else
    {
        m_manualRedxedit.EnableWindow (FALSE);
        m_manualRedyedit.EnableWindow (FALSE);
        m_manualGreenxedit.EnableWindow (FALSE);
        m_manualGreenyedit.EnableWindow (FALSE);
        m_manualBluexedit.EnableWindow (FALSE);
        m_manualBlueyedit.EnableWindow (FALSE);
    }
	if (GetConfig ()->m_GammaOffsetType >= 4)
    {
  	  m_GammaRefEdit.EnableWindow (FALSE);
  	  m_eMeasuredGamma.EnableWindow (FALSE);
	  m_useMeasuredGamma = FALSE;
    }
	if (m_userBlack)
		m_ManualBlackEdit.EnableWindow(TRUE);
	else
		m_ManualBlackEdit.EnableWindow(FALSE);

	m_isModified=TRUE;
	GetConfig()->ApplySettings(FALSE);
	m_isModified=FALSE;

	if  ( (m_manualRedx != m_manualRedxold) || (m_manualRedy != m_manualRedyold) || (m_manualBluex != m_manualBluexold)
		|| (m_manualGreenx != m_manualGreenxold) || (m_manualGreeny != m_manualGreenyold) || (m_manualBluey != m_manualBlueyold)
		|| (m_manualWhitex != m_manualWhitexold) || (m_manualWhitey != m_manualWhiteyold) )
		m_bSave = TRUE;

	return CPropertyPageWithHelp::OnApply();
}

void CReferencesPropPage::OnOK() 
{
	GetConfig()->ApplySettings(FALSE);
	if  ( (m_manualRedx != m_manualRedxold) || (m_manualRedy != m_manualRedyold) || (m_manualBluex != m_manualBluexold)
		|| (m_manualGreenx != m_manualGreenxold) || (m_manualGreeny != m_manualGreenyold) || (m_manualBluey != m_manualBlueyold)
		|| (m_manualWhitex != m_manualWhitexold) || (m_manualWhitey != m_manualWhiteyold) )
		m_bSave = TRUE;

	CPropertyPageWithHelp::OnOK();
}


void CReferencesPropPage::OnChangeEditIrisTime() 
{
	m_isModified=TRUE;
	SetModified(TRUE);	
}

BOOL CReferencesPropPage::OnInitDialog() 
{
	CPropertyPageWithHelp::OnInitDialog();

	// TODO: Add extra initialization here
	// The IDC_GAMMA_OFFSET_RADIO* buttons this used to enable/disable here are
	// hidden for good by BuildRuntimeLayout below and replaced by the
	// transfer-function dropdown, so their enable state was unobservable. The
	// SDR-only rule those calls once expressed now lives in
	// PopulateTransferFuncCombo, which drops PQ/HLG from the list outright.
	if (m_colorStandard == sRGB)
	{
		m_GammaRefEdit.EnableWindow (FALSE);
		m_eMeasuredGamma.EnableWindow (FALSE);
 		m_manualGOffset = 0.055;
	}
	m_GammaAvgEdit.EnableWindow(FALSE);
	if (m_colorStandard == HDTVa || m_colorStandard == HDTVb || m_colorStandard == CC6) 
	{
		m_changeWhiteCheck = FALSE;
		m_whiteTarget=(int)(GetStandardColorReference((ColorStandard)(m_colorStandard)).m_white);	}
	else
		m_changeWhiteCheck = (m_whiteTarget!=(int)(GetStandardColorReference((ColorStandard)(m_colorStandard)).m_white));

    if (m_colorStandard == CUSTOM)
    {
        m_manualRedxedit.EnableWindow (TRUE);
        m_manualRedyedit.EnableWindow (TRUE);
        m_manualGreenxedit.EnableWindow (TRUE);
        m_manualGreenyedit.EnableWindow (TRUE);
        m_manualBluexedit.EnableWindow (TRUE);
        m_manualBlueyedit.EnableWindow (TRUE);
    }
    else
    {
        m_manualRedxedit.EnableWindow (FALSE);
        m_manualRedyedit.EnableWindow (FALSE);
        m_manualGreenxedit.EnableWindow (FALSE);
        m_manualGreenyedit.EnableWindow (FALSE);
        m_manualBluexedit.EnableWindow (FALSE);
        m_manualBlueyedit.EnableWindow (FALSE);
    }
	if (m_useMeasuredGamma)
		CheckRadioButton ( IDC_USE_MEASURED_GAMMA, IDC_USE_MEASURED_GAMMA, IDC_USE_MEASURED_GAMMA );
	if (GetConfig ()->m_GammaOffsetType >= 4)
    {
  	  m_GammaRefEdit.EnableWindow (FALSE);
  	  m_eMeasuredGamma.EnableWindow (FALSE);
	  m_useMeasuredGamma = FALSE;
    }

	m_ManualBlackEdit.EnableWindow(m_userBlack);
	m_bSave = GetConfig()->m_bSave;
	m_manualRedxold = m_manualRedx;
	m_manualRedyold = m_manualRedy;
	m_manualBluexold = m_manualBluex;
	m_manualBlueyold = m_manualBluey;
	m_manualGreenxold = m_manualGreenx;
	m_manualGreenyold = m_manualGreeny;
	m_manualWhitexold = m_manualWhitex;
	m_manualWhiteyold = m_manualWhitey;
	m_TargetMinLCtrl.EnableWindow(m_bOverRideTargs);
	m_TargetMaxLCtrl.EnableWindow(m_bOverRideTargs);
	m_TargetSysGammaCtrl.EnableWindow(m_bOverRideTargs);
	m_BT2390_BSCtrl.EnableWindow (m_bOverRideTargs);
	m_BT2390_WSCtrl.EnableWindow (m_bOverRideTargs);
	m_BT2390_WS1Ctrl.EnableWindow (m_bOverRideTargs);
	if (!m_bOverRideTargs)
	{
		m_DiffuseL = 94.37844;
		m_TargetMinL = 0.0;
		m_TargetSysGamma = floor( (1.2 + 0.42 * log10(GetConfig()->m_TargetMaxL / 1000.))*100.+0.5)/100.;
		m_BT2390_BS = 1.0;
		m_BT2390_WS = 0.0;
		m_BT2390_WS1 = 25;
	}
	else
	{
		GetConfig()->m_DiffuseL = m_DiffuseL;
		GetConfig()->m_TargetMinL = m_TargetMinL;
		GetConfig()->m_TargetMaxL = m_TargetMaxL;
	}
	m_DiffuseLCtrl.EnableWindow(m_bOverRideTargs);
	GetConfig()->m_TargetSysGamma = m_TargetSysGamma;
	GetConfig()->m_BT2390_BS = m_BT2390_BS;
	GetConfig()->m_BT2390_WS = m_BT2390_WS;
	GetConfig()->m_BT2390_WS1 = m_BT2390_WS1;
	
	if (m_GammaOffsetType == 5 || m_GammaOffsetType == 7)
	{
		if (m_bOverRideTargs)
		{
			m_TargetMinLCtrl.EnableWindow (TRUE);
  			m_TargetMaxLCtrl.EnableWindow (TRUE);
  			m_DiffuseLCtrl.EnableWindow (TRUE);
			m_TargetSysGammaCtrl.EnableWindow (TRUE);
			m_BT2390_BSCtrl.EnableWindow (TRUE);
			m_BT2390_WSCtrl.EnableWindow (TRUE);
			m_BT2390_WS1Ctrl.EnableWindow (TRUE);
		}
  		m_MasterMinLCtrl.EnableWindow (TRUE);
  		m_MasterMaxLCtrl.EnableWindow (TRUE);
  		m_ContentMaxLCtrl.EnableWindow (TRUE);
  		m_FrameAvgMaxLCtrl.EnableWindow (TRUE);
  		m_bOverRideTargsCtrl.EnableWindow (TRUE);
  		m_useToneMapCtrl.EnableWindow (TRUE);
	}
	else
	{
		m_TargetMinLCtrl.EnableWindow (FALSE);
  		m_TargetMaxLCtrl.EnableWindow (FALSE);
  		m_MasterMinLCtrl.EnableWindow (FALSE);
  		m_MasterMaxLCtrl.EnableWindow (FALSE);
  		m_ContentMaxLCtrl.EnableWindow (FALSE);
  		m_FrameAvgMaxLCtrl.EnableWindow (FALSE);
  		m_bOverRideTargsCtrl.EnableWindow (FALSE);
  		m_useToneMapCtrl.EnableWindow (FALSE);
  		m_DiffuseLCtrl.EnableWindow (FALSE);
		m_TargetSysGammaCtrl.EnableWindow (FALSE);
		m_BT2390_BSCtrl.EnableWindow (FALSE);
		m_BT2390_WSCtrl.EnableWindow (FALSE);
		m_BT2390_WS1Ctrl.EnableWindow (FALSE);
	}
	BuildRuntimeLayout();
	UpdateControlStates();
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CReferencesPropPage::OnChangeEditGammaRef() 
{
	m_isModified=TRUE;
	m_bSave = TRUE;
	SetModified(TRUE);
}

void CReferencesPropPage::OnChangeEditGammaRel() 
{
	m_isModified=TRUE;
	m_bSave = TRUE;
	SetModified(TRUE);
}

void CReferencesPropPage::OnChangeEditManualBlack() 
{
	m_isModified=TRUE;
	SetModified(TRUE);	
	UpdateData(TRUE);
	m_bSave = TRUE;
}

void CReferencesPropPage::OnChangeEditGammaAvg() 
{
	m_isModified=TRUE;
	SetModified(TRUE);
}

void CReferencesPropPage::OnChangeEditManualGOffset() 
{
	m_isModified=TRUE;
	m_bSave = TRUE;
	SetModified(TRUE);
}

UINT CReferencesPropPage::GetHelpId ( LPSTR lpszTopic )
{
	return HID_PREF_REFERENCES;
}

void CReferencesPropPage::OnUseMeasuredGammaCheck() 
{
	m_isModified=TRUE;
	SetModified(TRUE);	
	m_bSave = TRUE;
	UpdateData(TRUE);
	UpdateControlStates();
}

void CReferencesPropPage::OnUserBlackCheck() 
{
	m_isModified=TRUE;
	SetModified(TRUE);	
	UpdateData(TRUE);
	m_bSave = TRUE;
	m_ManualBlackEdit.EnableWindow(m_userBlack);
	UpdateControlStates();
}

void CReferencesPropPage::OnUserOverRideTargsCheck() 
{
	m_isModified=TRUE;
	SetModified(TRUE);	
	UpdateData(TRUE);
	m_bSave = TRUE;
	m_DiffuseLCtrl.EnableWindow(m_bOverRideTargs);
	if (!m_bOverRideTargs)
	{
		m_DiffuseLUser = m_DiffuseL;
		m_DiffuseL = 94.37844;
		m_TargetMinLUser = m_TargetMinL;
		m_TargetMinL = 0.0;
		m_TargetMaxLUser = m_TargetMaxL;
		m_TargetSysGammaUser = m_TargetSysGamma;
		m_TargetSysGamma = floor( (1.2 + 0.42 * log10(GetConfig()->m_TargetMaxL / 1000.))*100.0 + 0.5) / 100.0;
		m_BT2390_BSUser = m_BT2390_BS;
		m_BT2390_WSUser = m_BT2390_WS;
		m_BT2390_WS1User = m_BT2390_WS1;
	}
	else
	{
		m_DiffuseL = m_DiffuseLUser;
		m_TargetMinL = m_TargetMinLUser;
		m_TargetMaxL = m_TargetMaxLUser;
		m_TargetSysGamma = m_TargetSysGammaUser;
		m_BT2390_BS = m_BT2390_BSUser;
		m_BT2390_WS = m_BT2390_WSUser;
		m_BT2390_WS1 = m_BT2390_WS1User;
	}

	GetConfig()->m_DiffuseL = m_DiffuseL;
	m_TargetMinLCtrl.EnableWindow(m_bOverRideTargs);
	m_TargetMaxLCtrl.EnableWindow(m_bOverRideTargs);
	m_TargetSysGammaCtrl.EnableWindow (m_bOverRideTargs);
	m_BT2390_BSCtrl.EnableWindow (m_bOverRideTargs);
	m_BT2390_WSCtrl.EnableWindow (m_bOverRideTargs);
	m_BT2390_WS1Ctrl.EnableWindow (m_bOverRideTargs);
	GetConfig()->m_TargetSysGamma = m_TargetSysGamma;
	GetConfig()->m_BT2390_BS = m_BT2390_BS;
	GetConfig()->m_BT2390_WS = m_BT2390_WS;
	GetConfig()->m_BT2390_WS1 = m_BT2390_WS1;

	UpdateData(FALSE);
	UpdateControlStates();
}

void CReferencesPropPage::OnSelchangeWhiteCombo()
{
	m_isModified=TRUE;
	SetModified(TRUE);
	m_bSave = TRUE;
	UpdateData(TRUE);
	// Keep the internal "white overridden" flag honest now that the dropdown is the
	// only control: TRUE when the chosen white differs from the standard default, so a
	// later color-space change preserves the user's white instead of resetting it.
	m_changeWhiteCheck = (m_whiteTarget != (int)(GetStandardColorReference((ColorStandard)(m_colorStandard)).m_white));
	BOOL enableEditControls = m_whiteTarget == DCUST ? TRUE : FALSE;
	m_manualWhitexedit.EnableWindow (enableEditControls);
	m_manualWhiteyedit.EnableWindow (enableEditControls);
	UpdateColorSpaceValues();
	UpdateData(FALSE);
	UpdateControlStates();
}

void CReferencesPropPage::OnSelchangeColorrefCombo() 
{
	m_isModified=TRUE;
	SetModified(TRUE);
	m_bSave = TRUE;
	UpdateData(TRUE);
	UpdateColorSpaceValues();
	// Radio enable/disable dropped here too - the controls are hidden for good.
	// See OnInitDialog and PopulateTransferFuncCombo.
	if (m_colorStandard == sRGB)
	{
		m_GammaRefEdit.EnableWindow (FALSE);
		m_eMeasuredGamma.EnableWindow (FALSE);
 		m_manualGOffset = 0.055;
	}
	else
	{
		m_GammaRefEdit.EnableWindow (TRUE);
		if (m_GammaOffsetType < 4)
			m_eMeasuredGamma.EnableWindow (TRUE);
		else
		{
			m_eMeasuredGamma.EnableWindow (FALSE);
	 	    m_useMeasuredGamma = FALSE;
		}
		m_manualGOffset = 0.099;
	}
	// SDR-only standards must not keep (or be able to pick) PQ/HLG. This used to
	// disable the PQ/HLG radio buttons, which the runtime-built page hides - see
	// PopulateTransferFuncCombo, which now owns the whole rule and rebuilds the
	// dropdown for the newly chosen standard.
	//
	// The old code also had a bug worth not reproducing: it reset the RADIO,
	// via CheckRadioButton(RADIO1, RADIO1, RADIO1), while assigning
	// m_GammaOffsetType = 4 - and RADIO1 is type 0, not 4 - so the radio and
	// the variable disagreed until something re-synced them.
	PopulateTransferFuncCombo();
	if (m_colorStandard == CC6)
		m_CCMode = GCD;
	// Restore the standard's default white when the user hasn't overridden it,
	// and ALWAYS for the fixed-matrix standards (HDTVa/HDTVb/CC6), locked to D65.
	if(!m_changeWhiteCheck || m_colorStandard == HDTVa || m_colorStandard == HDTVb || m_colorStandard == CC6)
		m_whiteTarget=(int)(GetStandardColorReference((ColorStandard)(m_colorStandard)).m_white);
    if (m_colorStandard == CUSTOM)
    {
        m_manualRedxedit.EnableWindow (TRUE);
        m_manualRedyedit.EnableWindow (TRUE);
        m_manualGreenxedit.EnableWindow (TRUE);
        m_manualGreenyedit.EnableWindow (TRUE);
        m_manualBluexedit.EnableWindow (TRUE);
        m_manualBlueyedit.EnableWindow (TRUE);
    }
	else
	{
        m_manualRedxedit.EnableWindow (FALSE);
        m_manualRedyedit.EnableWindow (FALSE);
        m_manualGreenxedit.EnableWindow (FALSE);
        m_manualGreenyedit.EnableWindow (FALSE);
        m_manualBluexedit.EnableWindow (FALSE);
        m_manualBlueyedit.EnableWindow (FALSE);
	}

	UpdateData(FALSE);	
	UpdateControlStates();
}

void CReferencesPropPage::OnSelchangeCCmodeCombo() 
{
	m_isModified=TRUE;
	m_bSave = TRUE;
	SetModified(TRUE);
	UpdateData(TRUE);
	if (m_colorStandard == CC6)
		m_CCMode = GCD;
	GetConfig()->ApplySettings(false);
	UpdateData(FALSE);
}

void CReferencesPropPage::OnChangeEditGammaOffset() 
{
	m_isModified=TRUE;
	m_bSave = TRUE;
	SetModified(TRUE);
}

void CReferencesPropPage::UpdateColorSpaceValues()
{
	CColorReference colorRef((ColorStandard)m_colorStandard, (WhiteTarget)m_whiteTarget);

	if (m_whiteTarget != DCUST)
	{
		CColor whiteColor(colorRef.GetWhite());
		m_manualWhitex = whiteColor.GetxyYValue()[0];
		m_manualWhitey = whiteColor.GetxyYValue()[1];
	}
	if (m_colorStandard != CUSTOM)
	{
		CColor redColor(colorRef.GetRed());
		m_manualRedx   = redColor.GetxyYValue()[0];
		m_manualRedy   = redColor.GetxyYValue()[1];

		CColor greenColor(colorRef.GetGreen());
		m_manualGreenx = greenColor.GetxyYValue()[0];
		m_manualGreeny = greenColor.GetxyYValue()[1];

		CColor blueColor(colorRef.GetBlue());
		m_manualBluex  = blueColor.GetxyYValue()[0];
		m_manualBluey  = blueColor.GetxyYValue()[1];
	}
}

/////////////////////////////////////////////////////////////////////////////
// Runtime-built layout for the redesigned References page (branch uiFixes).
// The dialog template positions are ignored; every control is repositioned
// here so all 5 localized templates produce one identical layout. Decoration
// (group boxes + labels) is created at runtime with localized IDS_* strings.
//
namespace {

struct DlgMap
{
    HWND h;
    CPoint at(int x, int y) { CRect r(x, y, x + 1, y + 1); ::MapDialogRect(h, &r); return CPoint(r.left, r.top); }
    int w(int n)  { CRect r(0, 0, n, 1); ::MapDialogRect(h, &r); return r.right; }
    int ht(int n) { CRect r(0, 0, 1, n); ::MapDialogRect(h, &r); return r.bottom; }
};

static CString LS(UINT id)
{
    CString s;
    if (id) s.LoadString(id);
    return s;
}

static CStatic* AddText(CWnd* pg, CObArray& all, CObArray* bucket, CFont* font, DlgMap& M,
                        const CString& text, int x, int y, int w, int h, DWORD style = SS_LEFT)
{
    CStatic* p = new CStatic();
    CPoint pt = M.at(x, y);
    p->Create(text, WS_CHILD | WS_VISIBLE | style, CRect(pt.x, pt.y, pt.x + M.w(w), pt.y + M.ht(h)), pg);
    p->SetFont(font);
    all.Add(p);
    if (bucket) bucket->Add(p);
    return p;
}

static CButton* AddGroup(CWnd* pg, CObArray& all, CObArray* bucket, CFont* font, DlgMap& M,
                         UINT ids, int x, int y, int w, int h)
{
    CButton* p = new CButton();
    CPoint pt = M.at(x, y);
    p->Create(LS(ids), WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(pt.x, pt.y, pt.x + M.w(w), pt.y + M.ht(h)), pg, (UINT)IDC_STATIC);
    p->SetFont(font);
    all.Add(p);
    if (bucket) bucket->Add(p);
    return p;
}

static void MoveDlg(CWnd* pg, UINT id, DlgMap& M, int x, int y, int w, int h)
{
    CWnd* c = pg->GetDlgItem(id);
    if (!c) return;
    CPoint pt = M.at(x, y);
    c->MoveWindow(pt.x, pt.y, M.w(w), M.ht(h));
}

static void MoveWnd(CWnd* c, DlgMap& M, int x, int y, int w, int h)
{
    if (!c || !c->GetSafeHwnd()) return;
    CPoint pt = M.at(x, y);
    c->MoveWindow(pt.x, pt.y, M.w(w), M.ht(h));
}

static void ShowIds(CWnd* pg, const UINT* ids, int n, BOOL show)
{
    for (int i = 0; i < n; i++)
    {
        CWnd* c = pg->GetDlgItem(ids[i]);
        if (c) c->ShowWindow(show ? SW_SHOW : SW_HIDE);
    }
}

static void EnableIds(CWnd* pg, const UINT* ids, int n, BOOL en)
{
    for (int i = 0; i < n; i++)
    {
        CWnd* c = pg->GetDlgItem(ids[i]);
        if (c) c->EnableWindow(en);
    }
}

static void ShowBucket(CObArray& a, BOOL show)
{
    for (int i = 0; i < a.GetSize(); i++)
    {
        CWnd* c = (CWnd*)a.GetAt(i);
        if (c && c->GetSafeHwnd()) c->ShowWindow(show ? SW_SHOW : SW_HIDE);
    }
}

// transfer-function dropdown order <-> stored m_GammaOffsetType.
// Ordered SDR-FIRST on purpose: the two HDR entries are the TAIL of the list,
// so a standard that may not select them just gets a shorter list and the
// index -> type mapping below needs no second form. Do not reorder.
static const int kComboToType[6] = { 0, 1, 4, 6, 5, 7 };
static const UINT kComboLabel[6] =
{
	IDS_TF_GAMMA_POWER, IDS_TF_GAMMA_BLACKCOMP, IDS_TF_BT1886,
	IDS_TF_LSTAR, IDS_TF_PQ, IDS_TF_HLG,
};
static const int kComboSDRCount = 4;	// kComboToType entries before the HDR pair

// The color spaces allowed to select PQ/HLG. Taken from the version of
// OnSelchangeColorrefCombo that gated the radio buttons: everything else is an
// SDR standard. HDTVa/HDTVb especially - they are fixed Rec.709/D65 SDR pattern
// conventions (75% and plasma), locked to a D65 white, and HDTVa's 75% white
// anchor has no meaning under PQ.
//
// CUSTOM stays OUT, though it is the one entry whose exclusion looks arguable
// (nothing about user-supplied primaries is inherently SDR). Three reasons to
// leave it: it is absent from /accuracytest's kSpaces entirely, so a CUSTOM HDR
// path would carry no automated coverage; ColorMathTest's T10 excludes it on
// purpose because merely CONSTRUCTING a CUSTOM reference corrupts the global
// primariesRec601 array; and that corruption is a known open defect (MATH-007).
// Offering PQ there would light up an untested HDR path on the one standard
// both harnesses deliberately avoid.
static bool StandardAllowsHDR ( int cs )
{
	return ( cs == UHDTV || cs == UHDTV2 || cs == UHDTV3 || cs == UHDTV4 || cs == HDTV );
}

static int TypeToCombo(int t)
{
    for (int i = 0; i < 6; i++) if (kComboToType[i] == t) return i;
    return 0;
}

static bool IsKnownType(int t)
{
    for (int i = 0; i < 6; i++) if (kComboToType[i] == t) return true;
    return false;
}

static int TypeToRadio(int t)
{
    switch (t)
    {
        case 0: return IDC_GAMMA_OFFSET_RADIO1;
        case 1: return IDC_GAMMA_OFFSET_RADIO2;
        case 4: return IDC_GAMMA_OFFSET_RADIO5;
        case 5: return IDC_GAMMA_OFFSET_RADIO8;
        case 6: return IDC_GAMMA_OFFSET_RADIO7;
        case 7: return IDC_GAMMA_OFFSET_RADIO6;
    }
    return IDC_GAMMA_OFFSET_RADIO1;
}

} // namespace

// Fill the transfer-function dropdown for the ACTIVE color standard, offering
// PQ/HLG only where they are legal, and coerce a stored HDR transfer function
// that the new standard does not allow.
//
// This restriction is original behavior, not a new policy: OnSelchangeColorrefCombo
// used to enforce it by disabling IDC_GAMMA_OFFSET_RADIO6/8/9/10 (HLG/PQ/DV500/
// DV400). The runtime-built page HIDES all ten of those radios and drives the
// transfer function from this dropdown instead, so EnableWindow(FALSE) on a
// hidden radio enforced nothing and the dropdown offered PQ/HLG under every
// standard. The hole was order-dependent - picking the transfer function SECOND
// reached it, because OnSelchangeTransferFuncCombo has no guard of its own -
// which is how HDTVa + PQ became selectable.
// Returns true if it had to CHANGE m_GammaOffsetType to fit the active standard,
// so the caller can mark the page dirty - see BuildRuntimeLayout.
bool CReferencesPropPage::PopulateTransferFuncCombo()
{
	if ( !m_transferFuncCombo.GetSafeHwnd() )
		return false;

	// sRGB mandates its own transfer function: every consumer forces getL_EOTF
	// mode 99 when m_colorStandard == sRGB (~20 sites) and ignores
	// m_GammaOffsetType entirely. Offer exactly that one entry, so the
	// (disabled) dropdown names the curve actually in force instead of
	// whichever transfer function happened to be selected beforehand.
	// m_GammaOffsetType is deliberately left ALONE here - it is unused under
	// sRGB, and preserving it restores the user's choice when they select a
	// standard that honours it again.
	if ( m_colorStandard == sRGB )
	{
		m_transferFuncCombo.ResetContent();
		m_transferFuncCombo.AddString(LS(IDS_TF_SRGB));
		m_transferFuncCombo.SetCurSel(0);
		return false;
	}

	bool bHDROk = StandardAllowsHDR(m_colorStandard);
	int  nWas   = m_GammaOffsetType;

	// Coerce BEFORE repopulating, so the selection below cannot be asked for an
	// entry the list no longer carries. BT.1886 is the fallback the original
	// rule used.
	if ( !bHDROk && ( m_GammaOffsetType == 5 || m_GammaOffsetType == 7 ) )
		m_GammaOffsetType = 4;
	if ( !IsKnownType(m_GammaOffsetType) )
		m_GammaOffsetType = 0;

	int nShown = bHDROk ? 6 : kComboSDRCount;
	m_transferFuncCombo.ResetContent();
	for ( int i = 0 ; i < nShown ; i ++ )
		m_transferFuncCombo.AddString(LS(kComboLabel[i]));
	m_transferFuncCombo.SetCurSel(TypeToCombo(m_GammaOffsetType));

	// The hidden legacy radios are still the DDX_Radio source for
	// m_GammaOffsetType, so they have to track the dropdown - otherwise the next
	// UpdateData(TRUE) reads a stale radio and undoes the coercion above.
	CheckRadioButton(IDC_GAMMA_OFFSET_RADIO1, IDC_GAMMA_OFFSET_RADIO10, TypeToRadio(m_GammaOffsetType));

	return ( m_GammaOffsetType != nWas );
}

void CReferencesPropPage::BuildRuntimeLayout()
{
    for (int i = 0; i < m_dynAll.GetSize(); i++)
    {
        CWnd* c = (CWnd*)m_dynAll.GetAt(i);
        if (c) { if (c->GetSafeHwnd()) c->DestroyWindow(); delete c; }
    }
    m_dynAll.RemoveAll();
    m_bSDRgamma.RemoveAll(); m_bTargetGamma.RemoveAll(); m_bBT1886.RemoveAll();
    m_bLstar.RemoveAll(); m_bHDR.RemoveAll(); m_bPQ.RemoveAll(); m_bHLG.RemoveAll();
    m_bWhiteXY.RemoveAll(); m_bToneSlopes.RemoveAll();
    m_pCdm2 = NULL;
    m_pTFGroup = NULL;
    m_pCCGroup = NULL;
    m_pTargetGroup = NULL;

    DlgMap M; M.h = GetSafeHwnd();
    CFont* font = GetFont();

    for (CWnd* c = GetWindow(GW_CHILD); c != NULL; c = c->GetWindow(GW_HWNDNEXT))
    {
        int id = c->GetDlgCtrlID();
        if (id <= 0 || id == 0xffff) c->ShowWindow(SW_HIDE);
    }

    static const UINT hideIds[] = {
        IDC_GAMMA_OFFSET_RADIO1, IDC_GAMMA_OFFSET_RADIO2, IDC_GAMMA_OFFSET_RADIO3,
        IDC_GAMMA_OFFSET_RADIO4, IDC_GAMMA_OFFSET_RADIO5, IDC_GAMMA_OFFSET_RADIO6,
        IDC_GAMMA_OFFSET_RADIO7, IDC_GAMMA_OFFSET_RADIO8, IDC_GAMMA_OFFSET_RADIO9,
        IDC_GAMMA_OFFSET_RADIO10, IDC_CHANGEWHITE_CHECK, IDC_EDIT_MANUAL_GOFFSET };
    ShowIds(this, hideIds, sizeof(hideIds) / sizeof(hideIds[0]), FALSE);

    // ===== Section 1: Color space =====
    AddGroup(this, m_dynAll, NULL, font, M, IDS_REF_GRP_COLORSPACE, 5, 3, 270, 72);
    AddText(this, m_dynAll, NULL, font, M, LS(IDS_REF_STANDARD), 12, 19, 42, 9);
    MoveDlg(this, IDC_COLORREF_COMBO, M, 54, 17, 96, 90);
    {
        CComboBox* pStd = (CComboBox*)GetDlgItem(IDC_COLORREF_COMBO);
        if (pStd && pStd->GetCount() == 11)
        {
            int sel = m_colorStandard;
            pStd->ResetContent();
            for (int i = 0; i <= CUSTOM; i++) pStd->AddString(GetColorStandardName(i));
            pStd->SetCurSel(sel);
        }
    }
    AddText(this, m_dynAll, NULL, font, M, LS(IDS_REF_WHITEPOINT), 12, 34, 42, 9);
    MoveDlg(this, IDC_WHITETARGET_COMBO, M, 54, 32, 96, 90);
    AddText(this, m_dynAll, &m_bWhiteXY, font, M, "x", 44, 49, 8, 9);
    MoveDlg(this, IDC_WHITE_X, M, 54, 47, 32, 12);
    AddText(this, m_dynAll, &m_bWhiteXY, font, M, "y", 92, 49, 8, 9);
    MoveDlg(this, IDC_WHITE_Y, M, 102, 47, 32, 12);
    AddText(this, m_dynAll, NULL, font, M, "x", 190, 8, 16, 8, SS_CENTER);
    AddText(this, m_dynAll, NULL, font, M, "y", 226, 8, 16, 8, SS_CENTER);
    AddText(this, m_dynAll, NULL, font, M, LS(IDS_REF_RED), 160, 19, 20, 9);
    MoveDlg(this, IDC_RED_X, M, 184, 17, 32, 12);
    MoveDlg(this, IDC_RED_Y, M, 220, 17, 32, 12);
    AddText(this, m_dynAll, NULL, font, M, LS(IDS_REF_GREEN), 160, 34, 20, 9);
    MoveDlg(this, IDC_GREEN_X, M, 184, 32, 32, 12);
    MoveDlg(this, IDC_GREEN_Y, M, 220, 32, 32, 12);
    AddText(this, m_dynAll, NULL, font, M, LS(IDS_REF_BLUE), 160, 49, 20, 9);
    MoveDlg(this, IDC_BLUE_X, M, 184, 47, 32, 12);
    MoveDlg(this, IDC_BLUE_Y, M, 220, 47, 32, 12);

    // ===== Section 2: Transfer function (dropdown on the title line) =====
    m_pTFGroup = AddGroup(this, m_dynAll, NULL, font, M, IDS_REF_GRP_TRANSFERFUNC, 5, 80, 270, 148);
    if (m_transferFuncCombo.GetSafeHwnd()) m_transferFuncCombo.DestroyWindow();
    {
        CPoint cpt = M.at(90, 79);
        m_transferFuncCombo.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            CRect(cpt.x, cpt.y, cpt.x + M.w(115), cpt.y + M.ht(120)), this, IDC_TRANSFERFUNC_COMBO);
        m_transferFuncCombo.SetFont(font);
    }
    // Contents are standard-dependent (PQ/HLG only where legal) and carry the
    // coercion + radio sync this used to do inline.
    //
    // OnInitDialog reaches this, so a config holding a transfer function the
    // active standard does not allow is corrected the moment the page opens.
    // Mark the page dirty when that happens: otherwise the correction is
    // invisible and survives only if the user happens to press OK, while
    // pressing Cancel leaves the page having displayed BT.1886 all along with
    // the config still holding PQ - the invalid pairing this lock exists to
    // remove.
    if (PopulateTransferFuncCombo())
    {
        m_isModified = TRUE;
        m_bSave = TRUE;
        SetModified(TRUE);
    }

    // Override black: common, fixed top-left for every transfer function.
    GetDlgItem(IDC_USER_BLACK)->SetWindowText(LS(IDS_REF_OVERRIDEBLACK));
    {
        // Auto-size the override-black checkbox to its localized text so the value
        // edit + cd/m2 sit right after it with no gap in any language (DE is far
        // longer than EN/FR/ES/IT, so a fixed width can't fit all without gaps).
        CClientDC dc(this);
        CFont* pOldF = dc.SelectObject(font);
        CSize ts = dc.GetTextExtent(LS(IDS_REF_OVERRIDEBLACK));
        dc.SelectObject(pOldF);
        CPoint cbPt = M.at(12, 96);
        int glyph = GetConfig()->Scale(18);            // checkbox box + gap before the text
        int cbW = glyph + ts.cx + M.w(4);
        CWnd* pUB = GetDlgItem(IDC_USER_BLACK);
        if (pUB) pUB->MoveWindow(cbPt.x, cbPt.y, cbW, M.ht(10));
        int editX = cbPt.x + cbW + M.w(4);
        int editW = M.w(26);
        CWnd* pEd = GetDlgItem(IDC_EDIT_MANUAL_BLACK);
        if (pEd) pEd->MoveWindow(editX, M.at(12, 95).y, editW, M.ht(12));
        m_pCdm2 = AddText(this, m_dynAll, NULL, font, M, "cd/m2", 12, 97, 30, 10);
        if (m_pCdm2) m_pCdm2->MoveWindow(editX + editW + M.w(4), M.at(12, 97).y, M.w(30), M.ht(10));
    }

    // Set target values manually: own row (HDR only).
    MoveDlg(this, IDC_USER_OVERRIDE_TARGS, M, 12, 110, 256, 10);
    GetDlgItem(IDC_USER_OVERRIDE_TARGS)->SetWindowText(LS(IDS_REF_SETMANUAL));

    // --- SDR power-law / black compensation: one Gamma box (editable reference,
    //     or locked measured-average when "use measured" is on) ---
    AddText(this, m_dynAll, &m_bSDRgamma, font, M, LS(IDS_REF_GAMMA), 12, 112, 30, 9);
    MoveDlg(this, IDC_EDIT_GAMMA_REF, M, 42, 110, 34, 12);
    MoveDlg(this, IDC_EDIT_GAMMA_AVERAGE, M, 42, 110, 34, 12);
    MoveDlg(this, IDC_USE_MEASURED_GAMMA, M, 42, 126, 180, 10);
    GetDlgItem(IDC_USE_MEASURED_GAMMA)->SetWindowText(LS(IDS_REF_USEMEASGAMMA));

    // --- BT.1886 (inline labels, inputs aligned) ---
    AddText(this, m_dynAll, &m_bBT1886, font, M, LS(IDS_REF_EFFGAMMA50), 12, 112, 110, 9);
    MoveDlg(this, IDC_EDIT_GAMMA_REL, M, 124, 110, 24, 12);
    AddText(this, m_dynAll, &m_bBT1886, font, M, LS(IDS_REF_BLACKOFFSET), 12, 128, 110, 9);
    MoveDlg(this, IDC_EDIT_SPLIT, M, 124, 126, 24, 12);

    // --- L* ---
    AddText(this, m_dynAll, &m_bLstar, font, M, LS(IDS_REF_LSTAR_DESC), 12, 112, 260, 9);

    // --- HDR target group (PQ + HLG) ---
    m_pTargetGroup = AddGroup(this, m_dynAll, &m_bHDR, font, M, IDS_REF_GRP_TARGETDISPLAY, 10, 124, 260, 106);
    AddText(this, m_dynAll, &m_bPQ,  font, M, LS(IDS_REF_DIFFUSEWHITE), 16, 134, 76, 9);
    MoveDlg(this, IDC_EDIT_DIFFUSE_WHITE, M, 16, 145, 44, 12);
    AddText(this, m_dynAll, &m_bHLG, font, M, LS(IDS_REF_HLGSYSGAMMA), 16, 134, 76, 9);
    MoveDlg(this, IDC_EDIT_TARGET_MAXL2, M, 16, 145, 44, 12);
    AddText(this, m_dynAll, &m_bHDR, font, M, LS(IDS_REF_TARGETPEAK), 98, 134, 76, 9);
    MoveDlg(this, IDC_EDIT_TARGET_MAXL, M, 98, 145, 44, 12);
    AddText(this, m_dynAll, &m_bHDR, font, M, LS(IDS_REF_TARGETBLACK), 180, 134, 76, 9);
    MoveDlg(this, IDC_EDIT_TARGET_MINL, M, 180, 145, 44, 12);
    MoveDlg(this, IDC_USE_TONEMAP, M, 16, 164, 180, 10);
    GetDlgItem(IDC_USE_TONEMAP)->SetWindowText(LS(IDS_REF_GRP_TONEMAP));
    AddText(this, m_dynAll, &m_bToneSlopes, font, M, LS(IDS_REF_ROLLOFFKNEE), 16, 178, 118, 9);
    MoveDlg(this, IDC_EDIT_TARGET_MAXL5, M, 16, 188, 22, 12);
    AddText(this, m_dynAll, &m_bToneSlopes, font, M, LS(IDS_REF_BLACKSLOPE), 16, 202, 118, 9);
    MoveDlg(this, IDC_EDIT_TARGET_MAXL3, M, 16, 212, 22, 12);
    AddText(this, m_dynAll, &m_bToneSlopes, font, M, LS(IDS_REF_HIGHLIGHTSLOPE), 140, 178, 116, 9);
    MoveDlg(this, IDC_EDIT_TARGET_MAXL4, M, 140, 188, 22, 12);

    // --- HDR10 signal metadata (PQ only) ---
    AddGroup(this, m_dynAll, &m_bPQ, font, M, IDS_REF_GRP_SIGNALMETA, 10, 234, 260, 66);
    AddText(this, m_dynAll, &m_bPQ, font, M, LS(IDS_REF_MASTERINGMIN), 16, 244, 62, 9);
    MoveDlg(this, IDC_EDIT_MASTER_MINL, M, 16, 254, 44, 12);
    AddText(this, m_dynAll, &m_bPQ, font, M, LS(IDS_REF_MASTERINGMAX), 82, 244, 62, 9);
    MoveDlg(this, IDC_EDIT_MASTER_MAXL, M, 82, 254, 44, 12);
    AddText(this, m_dynAll, &m_bPQ, font, M, LS(IDS_REF_MAXCLL), 148, 244, 60, 9);
    MoveDlg(this, IDC_EDIT_CONTENT_MAXL, M, 148, 254, 44, 12);
    AddText(this, m_dynAll, &m_bPQ, font, M, LS(IDS_REF_MAXFALL), 214, 244, 54, 9);
    MoveDlg(this, IDC_EDIT_FRAME_AVG_MAXL, M, 214, 254, 44, 12);
    AddText(this, m_dynAll, &m_bPQ, font, M, LS(IDS_REF_METANOTE), 16, 268, 248, 26);

    // ===== Section 3: ColorChecker patterns =====
    m_pCCGroup = AddGroup(this, m_dynAll, NULL, font, M, IDS_REF_GRP_CCPATTERNS, 5, 232, 270, 30);
    MoveDlg(this, IDC_CCMODE_COMBO, M, 12, 244, 200, 100);
}

void CReferencesPropPage::UpdateControlStates()
{
    int t = m_GammaOffsetType;
    // sRGB overrides m_GammaOffsetType outright (getL_EOTF mode 99), so NONE of
    // the per-transfer-function parameter panels apply - gate all five here
    // rather than at each use. Without this, arriving at sRGB from BT.1886 left
    // its Rel/Split fields on screen and editable, advertising parameters that
    // no longer feed anything; only the power-law gamma edits were ever guarded.
    BOOL isSRGB    = (m_colorStandard == sRGB);
    BOOL isPower   = !isSRGB && (t == 0 || t == 1);
    BOOL isBT1886  = !isSRGB && (t == 4);
    BOOL isLstar   = !isSRGB && (t == 6);
    BOOL isPQ      = !isSRGB && (t == 5);
    BOOL isHLG     = !isSRGB && (t == 7);
    BOOL isHDR     = (isPQ || isHLG);
    BOOL useMeas   = m_useMeasuredGamma;
    BOOL toneMap   = m_useToneMap;
    BOOL manual    = m_bOverRideTargs;

    DlgMap M; M.h = GetSafeHwnd();

    if (m_transferFuncCombo.GetSafeHwnd()) m_transferFuncCombo.EnableWindow(!isSRGB);

    CWnd* pUB = GetDlgItem(IDC_USER_BLACK);
    if (pUB) pUB->EnableWindow(TRUE);
    m_ManualBlackEdit.EnableWindow(m_userBlack);

    ShowBucket(m_bSDRgamma,   isPower);
    ShowBucket(m_bBT1886,     isBT1886);
    ShowBucket(m_bLstar,      isLstar);
    ShowBucket(m_bHDR,        isHDR);
    ShowBucket(m_bPQ,         isPQ);
    ShowBucket(m_bHLG,        isHLG);
    ShowBucket(m_bToneSlopes, isPQ && toneMap);

    static const UINT gRefId[] = { IDC_EDIT_GAMMA_REF };
    ShowIds(this, gRefId, 1, isPower && !useMeas);
    static const UINT gAvgId[] = { IDC_EDIT_GAMMA_AVERAGE };
    ShowIds(this, gAvgId, 1, isPower && useMeas);
    static const UINT measId[] = { IDC_USE_MEASURED_GAMMA };
    ShowIds(this, measId, 1, isPower);
    // isPower already excludes sRGB (see above), so the old && !isSRGB here is
    // now redundant.
    m_GammaRefEdit.EnableWindow(isPower && !useMeas);
    m_GammaAvgEdit.EnableWindow(FALSE);
    m_eMeasuredGamma.EnableWindow(isPower);

    static const UINT btIds[] = { IDC_EDIT_GAMMA_REL, IDC_EDIT_SPLIT };
    ShowIds(this, btIds, 2, isBT1886);

    static const UINT hdrIds[] = { IDC_EDIT_TARGET_MAXL, IDC_EDIT_TARGET_MINL, IDC_USER_OVERRIDE_TARGS };
    ShowIds(this, hdrIds, 3, isHDR);
    static const UINT pqIds[]  = { IDC_EDIT_DIFFUSE_WHITE, IDC_USE_TONEMAP,
        IDC_EDIT_MASTER_MINL, IDC_EDIT_MASTER_MAXL, IDC_EDIT_CONTENT_MAXL, IDC_EDIT_FRAME_AVG_MAXL };
    ShowIds(this, pqIds, 6, isPQ);
    static const UINT slopeIds[] = { IDC_EDIT_TARGET_MAXL5, IDC_EDIT_TARGET_MAXL3, IDC_EDIT_TARGET_MAXL4 };
    ShowIds(this, slopeIds, 3, isPQ && toneMap);
    static const UINT hlgIds[] = { IDC_EDIT_TARGET_MAXL2 };
    ShowIds(this, hlgIds, 1, isHLG);

    static const UINT diffuseId[]  = { IDC_EDIT_DIFFUSE_WHITE };
    EnableIds(this, diffuseId, 1, isPQ && manual);
    static const UINT tgtId[]      = { IDC_EDIT_TARGET_MAXL, IDC_EDIT_TARGET_MINL };
    EnableIds(this, tgtId, 2, isHDR && manual);
    static const UINT hlgGammaId[] = { IDC_EDIT_TARGET_MAXL2 };
    EnableIds(this, hlgGammaId, 1, isHLG && manual);
    static const UINT slopeEnId[]  = { IDC_EDIT_TARGET_MAXL5, IDC_EDIT_TARGET_MAXL3, IDC_EDIT_TARGET_MAXL4 };
    EnableIds(this, slopeEnId, 3, isPQ && manual && toneMap);
    static const UINT metaId[]     = { IDC_EDIT_MASTER_MINL, IDC_EDIT_MASTER_MAXL, IDC_EDIT_CONTENT_MAXL, IDC_EDIT_FRAME_AVG_MAXL };
    EnableIds(this, metaId, 4, isPQ);
    static const UINT toneChkId[]  = { IDC_USE_TONEMAP };
    EnableIds(this, toneChkId, 1, isPQ);
    static const UINT manualChkId[] = { IDC_USER_OVERRIDE_TARGS };
    EnableIds(this, manualChkId, 1, isHDR);

    // White-point dropdown is enabled directly (the old enabling checkbox is gone).
    // HDTVa/HDTVb/CC6 are fixed-matrix calibration standards locked to D65: grey out
    // the dropdown AND the x/y fields so the UI can't present an editable custom white
    // for a standard that ignores it.
    BOOL whiteLocked = (m_colorStandard == HDTVa || m_colorStandard == HDTVb || m_colorStandard == CC6);
    BOOL customW = (m_whiteTarget == DCUST) && !whiteLocked;
    ShowBucket(m_bWhiteXY, TRUE);
    static const UINT whiteIds[] = { IDC_WHITE_X, IDC_WHITE_Y };
    ShowIds(this, whiteIds, 2, TRUE);
    m_manualWhitexedit.EnableWindow(customW);
    m_manualWhiteyedit.EnableWindow(customW);
    if (m_whiteTargetCombo.GetSafeHwnd())
    {
        m_whiteTargetCombo.EnableWindow(!whiteLocked);
        // Force the dropdown to reflect m_whiteTarget so the (possibly greyed)
        // selection can never lag behind the white actually applied to the reference.
        if (m_whiteTarget >= 0)
            m_whiteTargetCombo.SetCurSel(m_whiteTarget);
    }

    BOOL customP = (m_colorStandard == CUSTOM);
    m_manualRedxedit.EnableWindow(customP);
    m_manualRedyedit.EnableWindow(customP);
    m_manualGreenxedit.EnableWindow(customP);
    m_manualGreenyedit.EnableWindow(customP);
    m_manualBluexedit.EnableWindow(customP);
    m_manualBlueyedit.EnableWindow(customP);

    MoveWnd(m_pTargetGroup, M, 10, 124, 260, isHLG ? 38 : 106);
    int tfBottom;
    if (isPower)        tfBottom = 144;
    else if (isBT1886)  tfBottom = 146;
    else if (isLstar)   tfBottom = 128;
    else if (isHLG)     tfBottom = 168;
    else                tfBottom = 306;
    MoveWnd(m_pTFGroup, M, 5, 80, 270, tfBottom - 80);
    MoveWnd(m_pCCGroup, M, 5, tfBottom + 6, 270, 30);
    MoveDlg(this, IDC_CCMODE_COMBO, M, 12, tfBottom + 18, 200, 100);
}

void CReferencesPropPage::OnSelchangeTransferFuncCombo()
{
    int sel = m_transferFuncCombo.GetCurSel();
    if (sel < 0) return;
    // Under sRGB the list holds the single locked "sRGB" entry, which maps to no
    // kComboToType slot - taking sel 0 there would silently rewrite
    // m_GammaOffsetType to 0 and lose the user's stored choice. The control is
    // disabled in that state, so this is belt and braces.
    if (m_colorStandard == sRGB) return;
    m_GammaOffsetType = kComboToType[sel];
    CheckRadioButton(IDC_GAMMA_OFFSET_RADIO1, IDC_GAMMA_OFFSET_RADIO10, TypeToRadio(m_GammaOffsetType));
    m_isModified = TRUE;
    m_bSave = TRUE;
    SetModified(TRUE);
    UpdateControlStates();
}
