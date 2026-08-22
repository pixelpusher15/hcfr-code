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
//	Francois-Xavier CHABOUD
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// Measure.cpp: implementation of the CMeasure class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ColorHCFR.h"
#include "MainFrm.h"
#include "Measure.h"
#include "AsyncMeasurer.h"
#include "Generator.h"
#include "LuxScaleAdvisor.h"
#include "DataSetDoc.h"
#include "Views\MainView.h"

#include <math.h>
#include <sstream>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#define PATTERN_SIZE (RANDOM250 * 100)

//////////////////////////////////////////////////////////////////////
// Grayscale level presets
//////////////////////////////////////////////////////////////////////

static const double s_grayLevels5[]  = { 0,25,50,75,100 };
static const double s_grayLevels6[]  = { 0,20,40,60,80,100 };
static const double s_grayLevels11[] = { 0,10,20,30,40,50,60,70,80,90,100 };
static const double s_grayLevels12[] = { 0,5,10,20,30,40,50,60,70,80,90,100 };	// non-uniform: 5% near-black
static const double s_grayLevels16[] = { 0,1,2,3,4,5,10,20,30,40,50,60,70,80,90,100 };	// 12-point + 1-4 IRE
static const double s_grayLevels21[] = { 0,5,10,15,20,25,30,35,40,45,50,55,60,65,70,75,80,85,90,95,100 };
static const double s_grayLevels25[] = { 0,1,2,3,4,5,10,15,20,25,30,35,40,45,50,55,60,65,70,75,80,85,90,95,100 };	// 21-point + 1-4 IRE

static const GrayScalePreset s_grayPresets[] =
{
	{ "5-point (25%)",                  5,  s_grayLevels5  },
	{ "6-point (20%)",                  6,  s_grayLevels6  },
	{ "11-point (10%)",                11,  s_grayLevels11 },
	{ "12-point (5% near-black)",      12,  s_grayLevels12 },
	{ "16-point (1-5% near black)",    16,  s_grayLevels16 },
	{ "21-point (5%)",                 21,  s_grayLevels21 },
	{ "25-point (1-5% near black)",    25,  s_grayLevels25 },
};

const GrayScalePreset * GetGrayScalePresets ()    { return s_grayPresets; }
int                     GetGrayScalePresetCount () { return sizeof(s_grayPresets) / sizeof(s_grayPresets[0]); }

// Fill 'levels' with a uniform ramp of 'nPoints' nominal IRE percentages (0..100).
// A Custom selection of N steps maps to nPoints = N+1.
static void FillUniformGrayLevels(CArray<double,double> & levels, int nPoints)
{
	levels.SetSize(nPoints);
	for (int i = 0; i < nPoints; i++)
		levels[i] = ( nPoints > 1 ) ? ( i * 100.0 / (nPoints - 1) ) : 0.0;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

static volatile BOOL g_bMeasureSweepActive = FALSE;
BOOL IsMeasureSweepActive() { return g_bMeasureSweepActive; }
namespace {
struct SweepActiveGuard
{
    BOOL m_owned;
    CMeasure * m_pMeasure;
    explicit SweepActiveGuard(CMeasure * p) : m_owned(!g_bMeasureSweepActive), m_pMeasure(p)
    {
        if (m_owned) { g_bMeasureSweepActive = TRUE; p->m_bAbortSweep = FALSE; }
    }
    // Clearing m_binMeasure here covers every early return (ESC cancel, sensor
    // abort, init failure); success paths still clear it explicitly before
    // their final UpdateViews so views repaint with the flag already down.
    ~SweepActiveGuard()
    {
        if (m_owned)
        {
            m_pMeasure->m_binMeasure = FALSE;
            g_bMeasureSweepActive = FALSE;
        }
    }
    BOOL Owned() const { return m_owned; }
};
}

static CColor PumpedRead(CAsyncMeasurer & am, CSensor * pSensor, const ColorRGBDisplay & rgb, int displaymode = 0)
{
	CColor c;
	if (am.IsRunning())
		am.MeasurePumped(rgb, c, displaymode);
	else
		c = pSensor->MeasureColor(rgb, displaymode);
	return c;
}

// NOTE on the window Intensity setting: GDI-family generators scale
// primary/secondary/saturation patches (INCLUDING the white anchor) by
// Intensity at emission. Because every dE/delta-L in the grids is normalised
// to the MEASURED white, and a power-law gamma satisfies
// (I*s)^g / (I*w)^g == (s/w)^g, the dimming cancels in the ratio - so neither
// the sensor read nor the references model Intensity. (Modeling it on either
// side alone shows up as a uniform ~1/I^g delta-L error.) Intensity is also
// disabled by the generator UI for HDR (GammaOffsetType 5/7), where absolute
// targets would break the cancellation.

double TmDiffuseWhiteNits(const CColor & White, const CColor & Black)
{
	return getL_EOTF(SnapToVideoGrid(0.5022283, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235()), White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, 5, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL, GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) * 100.0;
}

IMPLEMENT_SERIAL(CMeasure, CObject, 1)

CMeasure::CMeasure()
{
	m_NMeasurements = 0;
	m_isModified = FALSE;
	m_bpreV10 = 0;
	m_binMeasure = FALSE;
	m_bAbortSweep = FALSE;
	bDisplayRT = TRUE;
	m_primariesArray.SetSize(3);
	m_secondariesArray.SetSize(3);
	{
		// Grayscale levels: prefer an explicit preset index saved in the registry;
		// otherwise apply the legacy mapping of the saved step count
		// (4->5-point, 10->11-point, 20->21-point, any other N -> Custom with N steps).
		CArray<double,double> levels;
		const GrayScalePreset * pPresets = GetGrayScalePresets();
		int nPresetCount = GetGrayScalePresetCount();
		int nPreset = GetConfig()->GetProfileInt("Scale Sizes","GrayPreset",-99);
		if ( nPreset >= 0 && nPreset < nPresetCount )
		{
			levels.SetSize(pPresets[nPreset].count);
			for (int i = 0; i < pPresets[nPreset].count; i++)
				levels[i] = pPresets[nPreset].levels[i];
		}
		else
		{
			int nGray = GetConfig()->GetProfileInt("Scale Sizes","Gray",10);	// legacy step count (points-1)
			int nLegacy = ( nGray == 4 ) ? 0 : ( nGray == 10 ) ? 2 : ( nGray == 20 ) ? 4 : -1;
			if ( nLegacy >= 0 )
			{
				levels.SetSize(pPresets[nLegacy].count);
				for (int i = 0; i < pPresets[nLegacy].count; i++)
					levels[i] = pPresets[nLegacy].levels[i];
			}
			else
				FillUniformGrayLevels(levels, nGray + 1);	// Custom N steps -> N+1 points
		}
		m_grayMeasureArray.SetSize(levels.GetSize());
		m_grayIRELevelArray.Copy(levels);
	}
	m_nearBlackMeasureArray.SetSize( GetConfig()->GetProfileInt("Scale Sizes","Near Black",4)+1);
	m_nearWhiteMeasureArray.SetSize(GetConfig()->GetProfileInt("Scale Sizes","Near White",4)+1);
	m_redSatMeasureArray.SetSize(GetConfig()->GetProfileInt("Scale Sizes","Saturations",4)+1);
	m_greenSatMeasureArray.SetSize(GetConfig()->GetProfileInt("Scale Sizes","Saturations",4)+1);
	m_blueSatMeasureArray.SetSize(GetConfig()->GetProfileInt("Scale Sizes","Saturations",4)+1);
	m_yellowSatMeasureArray.SetSize(GetConfig()->GetProfileInt("Scale Sizes","Saturations",4)+1);
	m_cyanSatMeasureArray.SetSize(GetConfig()->GetProfileInt("Scale Sizes","Saturations",4)+1);
	m_magentaSatMeasureArray.SetSize(GetConfig()->GetProfileInt("Scale Sizes","Saturations",4)+1);

	//need to limit size to current cc size needs or compress saved file
    m_cc24SatMeasureArray.SetSize(MAX_USER_CC_PATCH_SIZE);
	m_cc24SatMeasureArray_master.SetSize(5 * MAX_USER_CC_PATCH_SIZE);

	m_activeSatLevel = 1.0;

	ClearProfileMeasures();
	m_bProfilePause = FALSE;
	m_profileCurrentDrift = 0.0;

	m_primariesArray[0]=m_primariesArray[1]=m_primariesArray[2]=noDataColor;
	m_secondariesArray[0]=m_secondariesArray[1]=m_secondariesArray[2]=noDataColor;

	for(int i=0;i<m_grayMeasureArray.GetSize();i++)	// Init default values: by default m_grayMeasureArray init to D65, Y=1
		m_grayMeasureArray[i]=GetPrimary(0);	

	m_grayMeasureArray[m_grayMeasureArray.GetSize()-1].SetXYZValue(ColorXYZ(GetConfig()->m_TargetMaxL*0.95047,GetConfig()->m_TargetMaxL,GetConfig()->m_TargetMaxL*1.08883));

	for(int i=0;i<m_nearBlackMeasureArray.GetSize();i++)
		m_nearBlackMeasureArray[i]=noDataColor;	

	for(int  i=0;i<m_nearWhiteMeasureArray.GetSize();i++)
		m_nearWhiteMeasureArray[i]=noDataColor;	

	for(int  i=0;i<m_redSatMeasureArray.GetSize();i++)
	{
		m_redSatMeasureArray[i]=noDataColor;
		m_greenSatMeasureArray[i]=noDataColor;
		m_blueSatMeasureArray[i]=noDataColor;
		m_yellowSatMeasureArray[i]=noDataColor;
		m_cyanSatMeasureArray[i]=noDataColor;
		m_magentaSatMeasureArray[i]=noDataColor;
	}	
	for ( int i=0;i<m_cc24SatMeasureArray.GetSize();i++ )	m_cc24SatMeasureArray[i]=noDataColor;
	for ( int i=0;i<m_cc24SatMeasureArray_master.GetSize();i++ )	m_cc24SatMeasureArray_master[i]=noDataColor;
//pre-load typical display values
	m_OnOffWhite.SetXYZValue(ColorXYZ(95.047,GetConfig()->m_TargetMaxL,108.883));
	m_PrimeWhite.SetXYZValue(ColorXYZ(GetConfig()->m_TargetMaxL*0.95047,GetConfig()->m_TargetMaxL,GetConfig()->m_TargetMaxL*1.08883));
	m_OnOffBlack.SetXYZValue(ColorXYZ(GetConfig()->m_TargetMinL*0.95047,GetConfig()->m_TargetMinL,GetConfig()->m_TargetMinL*1.08833));
	m_AnsiBlack=m_AnsiWhite=noDataColor;
	m_CCStr = (CString)"";
	SetInfoString((CString)"Calibration by: \r\nDisplay: \r\nNote: \r\n");
	m_currentIndex = 0;
	m_bIREScaleMode = GetConfig()->GetProfileInt("References","IRELevels",FALSE);

	m_hThread = NULL;
	m_hEventRun = NULL;
	m_hEventDone = NULL;
	m_bTerminateThread = FALSE;
	m_bErrorOccurred = FALSE;
	m_nBkMeasureStep = 0;
	m_clrToMeasure = ColorRGBDisplay(0.0);
	m_nBkMeasureStepCount = 0;
	m_pBkMeasureSensor = NULL;
	m_pBkMeasuredColor = NULL;

	m_nbMaxMeasurements = GetConfig()->GetProfileInt("General","MaxMeasurements",2500);
	if ( m_nbMaxMeasurements < 100 )
		m_nbMaxMeasurements = 100;
	else if ( m_nbMaxMeasurements > 30000 )
		m_nbMaxMeasurements = 30000;

	m_bOverRideBlack = GetConfig()->GetProfileDouble("References","Use Black Level",0);
	double YBlack = GetConfig()->GetProfileDouble("References","Manual Black Level",0);
	m_userBlack = CColor(ColorXYZ(YBlack*.95047,YBlack,YBlack*1.0883));
	m_NearWhiteClipCol = 101;
}

CMeasure::~CMeasure()
{

}

void CMeasure::Copy(CMeasure * p,UINT nId)
{
	switch (nId)
	{
		case DUPLGRAYLEVEL:		// Gray scale measure
			m_grayMeasureArray.SetSize(p->m_grayMeasureArray.GetSize());
			m_grayIRELevelArray.Copy(p->m_grayIRELevelArray);
			m_bIREScaleMode=p->m_bIREScaleMode;
			m_OnOffWhite=p->m_OnOffWhite;
			for(int i=0;i<m_grayMeasureArray.GetSize();i++)
    			m_grayMeasureArray[i]=p->m_grayMeasureArray[i];
			break;

		case DUPLNEARBLACK:		// Near black measure
			m_nearBlackMeasureArray.SetSize(p->m_nearBlackMeasureArray.GetSize());
			for(int i=0;i<m_nearBlackMeasureArray.GetSize();i++)
	    		m_nearBlackMeasureArray[i]=p->m_nearBlackMeasureArray[i];
			break;

		case DUPLNEARWHITE:		// Near white measure
			m_nearWhiteMeasureArray.SetSize(p->m_nearWhiteMeasureArray.GetSize());
			for(int i=0;i<m_nearWhiteMeasureArray.GetSize();i++)
		    	m_nearWhiteMeasureArray[i]=p->m_nearWhiteMeasureArray[i];
			break;

		case DUPLPRIMARIESSAT:		// Primaries saturation measure
			m_redSatMeasureArray.SetSize(p->m_redSatMeasureArray.GetSize());
			m_greenSatMeasureArray.SetSize(p->m_greenSatMeasureArray.GetSize());
			m_blueSatMeasureArray.SetSize(p->m_blueSatMeasureArray.GetSize());
			m_yellowSatMeasureArray.SetSize(p->m_yellowSatMeasureArray.GetSize());
			m_cyanSatMeasureArray.SetSize(p->m_cyanSatMeasureArray.GetSize());
			m_magentaSatMeasureArray.SetSize(p->m_magentaSatMeasureArray.GetSize());
			m_cc24SatMeasureArray.SetSize(p->m_cc24SatMeasureArray.GetSize());
			m_cc24SatMeasureArray_master.SetSize(p->m_cc24SatMeasureArray_master.GetSize());
			for(int i=0;i<m_redSatMeasureArray.GetSize();i++)
			{
				m_redSatMeasureArray[i]=p->m_redSatMeasureArray[i];
				m_greenSatMeasureArray[i]=p->m_greenSatMeasureArray[i];
				m_blueSatMeasureArray[i]=p->m_blueSatMeasureArray[i];
			}
			for(int i=0;i<m_cc24SatMeasureArray.GetSize();i++)
				m_cc24SatMeasureArray[i]=p->m_cc24SatMeasureArray[i];
			for(int i=0;i<m_cc24SatMeasureArray_master.GetSize();i++)
				m_cc24SatMeasureArray_master[i]=p->m_cc24SatMeasureArray_master[i];
			break;

		case DUPLSECONDARIESSAT:		// Secondaries saturation measure
			m_redSatMeasureArray.SetSize(p->m_redSatMeasureArray.GetSize());
			m_greenSatMeasureArray.SetSize(p->m_greenSatMeasureArray.GetSize());
			m_blueSatMeasureArray.SetSize(p->m_blueSatMeasureArray.GetSize());
			m_yellowSatMeasureArray.SetSize(p->m_yellowSatMeasureArray.GetSize());
			m_cyanSatMeasureArray.SetSize(p->m_cyanSatMeasureArray.GetSize());
			m_magentaSatMeasureArray.SetSize(p->m_magentaSatMeasureArray.GetSize());
			m_cc24SatMeasureArray.SetSize(p->m_cc24SatMeasureArray.GetSize());
			m_cc24SatMeasureArray_master.SetSize(p->m_cc24SatMeasureArray_master.GetSize());
			for(int i=0;i<m_yellowSatMeasureArray.GetSize();i++)
			{
				m_yellowSatMeasureArray[i]=p->m_yellowSatMeasureArray[i];
				m_cyanSatMeasureArray[i]=p->m_cyanSatMeasureArray[i];
				m_magentaSatMeasureArray[i]=p->m_magentaSatMeasureArray[i];
			}
			for(int i=0;i<m_cc24SatMeasureArray.GetSize();i++)
				m_cc24SatMeasureArray[i]=p->m_cc24SatMeasureArray[i];
			for(int i=0;i<m_cc24SatMeasureArray_master.GetSize();i++)
				m_cc24SatMeasureArray_master[i]=p->m_cc24SatMeasureArray_master[i];
			break;

		case DUPLPRIMARIESCOL:		// Primaries measure
			for(int i=0;i<m_primariesArray.GetSize();i++)
				m_primariesArray[i]=p->m_primariesArray[i];	
				m_PrimeWhite = p->m_PrimeWhite;
			break;

		case DUPLSECONDARIESCOL:		// Secondaries measure
			for(int i=0;i<m_secondariesArray.GetSize();i++)
				m_secondariesArray[i]=p->m_secondariesArray[i];
				m_PrimeWhite = p->m_PrimeWhite;
			break;

		case DUPLCONTRAST:		// Contrast measure
			m_OnOffBlack=p->m_OnOffBlack;
			m_OnOffWhite=p->m_OnOffWhite;
			m_AnsiBlack=p->m_AnsiBlack;
			m_AnsiBlack=p->m_AnsiBlack;
			break;

		case DUPLINFO:		// Info
			m_infoStr=p->m_infoStr;
			break;

		case DUPLPROFILE:	// Display profile (whole capture + metadata + drift anchors)
			m_profileCubeSize      = p->m_profileCubeSize;
			m_profileGrayExtras    = p->m_profileGrayExtras;
			m_profileDriftComp     = p->m_profileDriftComp;
			m_profileCaptureSeconds= p->m_profileCaptureSeconds;
			m_profileMeasureArray.Copy(p->m_profileMeasureArray);
			m_profileDriftAnchors  = p->m_profileDriftAnchors;
			m_profileDriftAnchorIdx= p->m_profileDriftAnchorIdx;
			m_profileGenCacheKey   = -1;	// force regen of the stimulus cache
			break;

		default:
			break;
	}
}

void CMeasure::Serialize(CArchive& ar)
{
	CObject::Serialize(ar) ;
	CColor MarkerColor(0.123,0.456,0.789);

	if (ar.IsStoring())
	{
		// Version 20 only when a display profile exists: documents without one keep
		// version 19 so they stay readable by older builds.
	    int version = HasProfileMeasures() ? 20 : 19;
		ar << version;

		StoreActiveSatLevel();	// capture the bound sweeps before writing the store

		ar << GetConfig()->m_BT2390_BS;
		ar << GetConfig()->m_BT2390_WS;
		ar << GetConfig()->m_BT2390_WS1;

		ar << GetConfig()->m_TargetSysGamma;

		ar << GetConfig()->m_MasterMinL;
		ar << GetConfig()->m_MasterMaxL;
		ar << GetConfig()->m_TargetMinL;
		ar << GetConfig()->m_TargetMaxL;
		ar << GetConfig()->m_ContentMaxL;
		ar << GetConfig()->m_FrameAvgMaxL;
		ar << GetConfig()->m_useToneMap;
		ar << GetConfig()->m_bOverRideTargs;
		ar << GetConfig()->m_DiffuseL;

		ar << m_NearWhiteClipCol;
		ar << (int)GetConfig()->m_whiteTarget; //new in 12
		ar << (int)GetConfig()->m_CCMode;
		ar << (int)GetConfig()->m_colorStandard;
		ar << GetConfig()->m_dE_form;
		ar << GetConfig()->m_dE_gray;
		ar << GetConfig()->gw_Weight;
		ar << GetConfig()->m_GammaOffsetType;
		ar << GetConfig()->m_GammaRef;
		ar << GetConfig()->m_GammaRel;
		ar << GetConfig()->m_Split;
		ar << GetConfig()->m_manualWhitex;
		ar << GetConfig()->m_manualWhitey;
		ar << GetConfig()->m_useMeasuredGamma;
		ar << GetConfig()->m_manualBluex;
		ar << GetConfig()->m_manualRedx;
		ar << GetConfig()->m_manualGreenx;
		ar << GetConfig()->m_manualBluey;
		ar << GetConfig()->m_manualRedy;
		ar << GetConfig()->m_manualGreeny;

		ar << m_bOverRideBlack; //new in 11
		m_userBlack.Serialize(ar);

		ar << m_grayMeasureArray.GetSize();
		for(int i=0;i<m_grayMeasureArray.GetSize();i++)
			m_grayMeasureArray[i].Serialize(ar);

		// Version 18: explicit grayscale IRE levels (supports non-uniform presets)
		ar << m_grayIRELevelArray.GetSize();
		for(int i=0;i<m_grayIRELevelArray.GetSize();i++)
			ar << m_grayIRELevelArray[i];

		// Version 3: near black and near white added
		ar << m_nearBlackMeasureArray.GetSize();
		for(int i=0;i<m_nearBlackMeasureArray.GetSize();i++)
			m_nearBlackMeasureArray[i].Serialize(ar);

		ar << m_nearWhiteMeasureArray.GetSize();
		for(int i=0;i<m_nearWhiteMeasureArray.GetSize();i++)
			m_nearWhiteMeasureArray[i].Serialize(ar);

		// Version 2: color saturation added
		ar << m_redSatMeasureArray.GetSize();
		for(int i=0;i<m_redSatMeasureArray.GetSize();i++)
			m_redSatMeasureArray[i].Serialize(ar);

		ar << m_greenSatMeasureArray.GetSize();
		for(int i=0;i<m_greenSatMeasureArray.GetSize();i++)
			m_greenSatMeasureArray[i].Serialize(ar);

		ar << m_blueSatMeasureArray.GetSize();
		for(int i=0;i<m_blueSatMeasureArray.GetSize();i++)
			m_blueSatMeasureArray[i].Serialize(ar);

		ar << m_yellowSatMeasureArray.GetSize();
		for(int i=0;i<m_yellowSatMeasureArray.GetSize();i++)
			m_yellowSatMeasureArray[i].Serialize(ar);

		ar << m_cyanSatMeasureArray.GetSize();
		for(int i=0;i<m_cyanSatMeasureArray.GetSize();i++)
			m_cyanSatMeasureArray[i].Serialize(ar);

		ar << m_magentaSatMeasureArray.GetSize();
		for(int i=0;i<m_magentaSatMeasureArray.GetSize();i++)
			m_magentaSatMeasureArray[i].Serialize(ar);

		//write end marker to limit storage size to real data limits version 13
		ar << m_cc24SatMeasureArray.GetSize();
		for(int i=0;i<m_cc24SatMeasureArray.GetSize();i++)
		{
			if (m_cc24SatMeasureArray[i].isValid())
			{
				m_cc24SatMeasureArray[i].Serialize(ar);
				ar << i;
			}
		}
		MarkerColor.Serialize(ar);

		ar << m_cc24SatMeasureArray_master.GetSize();
		for(int i=0;i<m_cc24SatMeasureArray_master.GetSize();i++)
		{
			if (m_cc24SatMeasureArray_master[i].isValid())
			{
				m_cc24SatMeasureArray_master[i].Serialize(ar);
				ar << i;
			}
		}
		MarkerColor.Serialize(ar);

		// Version 1 again
		ar << m_measurementsArray.GetSize();
		for(int i=0;i<m_measurementsArray.GetSize();i++)
			m_measurementsArray[i].Serialize(ar);

		m_primariesArray[0].Serialize(ar);
		m_primariesArray[1].Serialize(ar);
		m_primariesArray[2].Serialize(ar);

		m_secondariesArray[0].Serialize(ar);
		m_secondariesArray[1].Serialize(ar);
		m_secondariesArray[2].Serialize(ar);

		m_OnOffBlack.Serialize(ar);
		m_OnOffWhite.Serialize(ar);
		m_AnsiBlack.Serialize(ar);
		m_AnsiWhite.Serialize(ar);
//version 9
		m_PrimeWhite.Serialize(ar);

		ar << m_infoStr;

		ar << m_bIREScaleMode;

		// Version 19: multi-level saturation store
		ar << m_activeSatLevel;
		ar << (int) m_satLevelStore.size();
		for ( size_t s = 0; s < m_satLevelStore.size(); s++ )
		{
			ar << m_satLevelStore[s].stimLevel;
			for ( int c = 0; c < 6; c++ )
			{
				ar << (int) m_satLevelStore[s].sat[c].size();
				for ( size_t i = 0; i < m_satLevelStore[s].sat[c].size(); i++ )
					m_satLevelStore[s].sat[c][i].Serialize(ar);
			}
		}

		// Version 20: display profile capture (see version selection above)
		if ( HasProfileMeasures() )
		{
			ar << m_profileCubeSize;
			ar << m_profileGrayExtras;
			ar << m_profileDriftComp;
			ar << m_profileCaptureSeconds;

			ar << (int) m_profileMeasureArray.GetSize();	// loader reads an int; match the sibling counts + x64 width
			for(int i=0;i<m_profileMeasureArray.GetSize();i++)
			{
				if (m_profileMeasureArray[i].isValid())
				{
					m_profileMeasureArray[i].Serialize(ar);
					ar << i;
				}
			}
			MarkerColor.Serialize(ar);

			ar << (int) m_profileDriftAnchors.size();
			for ( size_t i = 0; i < m_profileDriftAnchors.size(); i++ )
			{
				ar << m_profileDriftAnchorIdx[i];
				m_profileDriftAnchors[i].Serialize(ar);
			}
		}
	}
	else
	{
	    int version;
		ar >> version;


		if ( version > 20 )
			AfxThrowArchiveException ( CArchiveException::badSchema );


		if ( version > 16)
		{
			ar >> GetConfig()->m_BT2390_BS;
			ar >> GetConfig()->m_BT2390_WS;
			ar >> GetConfig()->m_BT2390_WS1;
		}

		if ( version > 15)
			ar >> GetConfig()->m_TargetSysGamma;

		if ( version > 14 )
		{
			ar >> GetConfig()->m_MasterMinL;
			ar >> GetConfig()->m_MasterMaxL;
			ar >> GetConfig()->m_TargetMinL;
			ar >> GetConfig()->m_TargetMaxL;
			ar >> GetConfig()->m_ContentMaxL;
			ar >> GetConfig()->m_FrameAvgMaxL;
			ar >> GetConfig()->m_useToneMap;
			ar >> GetConfig()->m_bOverRideTargs;
			ar >> GetConfig()->m_DiffuseL;
		}

		if ( version > 13 )
			ar >> m_NearWhiteClipCol;

		if ( version > 11)
		{
			BOOL over = FALSE;
			int in[8];
			double in2[11];

			for (int i=0;i<7;i++)
				ar >> in[i];

			for (int i=0;i<5;i++)
				ar >> in2[i];

			ar >> in[7];

			for (int i=5;i<11;i++)
				ar >> in2[i];

			if ( in[0] != (int)GetConfig()->m_whiteTarget ||
					 in[1] != (int)GetConfig()->m_CCMode ||
					 in[2] != (int)GetConfig()->m_colorStandard ||
					 in[3] != GetConfig()->m_dE_form ||
					 in[4] != GetConfig()->m_dE_gray ||
					 in[5] != GetConfig()->gw_Weight ||
					 in[6] != GetConfig()->m_GammaOffsetType || 
					 in2[0] != GetConfig()->m_GammaRef ||
					 in2[1] != GetConfig()->m_GammaRel ||
					 in2[2] != GetConfig()->m_Split ||
					 in2[3] != GetConfig()->m_manualWhitex || 
					 in2[4] != GetConfig()->m_manualWhitey || 
					 in[7] != GetConfig()->m_useMeasuredGamma ||
					 in2[5] != GetConfig()->m_manualBluex ||
					 in2[6] != GetConfig()->m_manualRedx ||
					 in2[7] != GetConfig()->m_manualGreenx ||
					 in2[8] != GetConfig()->m_manualBluey ||
					 in2[9] != GetConfig()->m_manualRedy ||
					 in2[10] != GetConfig()->m_manualGreeny )
				over = TRUE;

			if (over)
			{
				CString msg;
				msg.SetString("Preferences in saved file differ from active preferences, overwrite?");
				if (GetColorApp()->InMeasureMessageBox(msg,"Warning", MB_YESNO | MB_ICONWARNING) != IDYES )
					over = FALSE;
			}

			if (over)
			{
				GetConfig()->m_whiteTarget = WhiteTarget(in[0]);
				GetConfig()->m_CCMode = CCPatterns(in[1]);
				GetConfig()->m_colorStandard = ColorStandard(in[2]);
				if (GetColorApp()->m_pColorReference)
					delete GetColorApp()->m_pColorReference;
				if (WhiteTarget(in[0]) == DCUST && in[2] != CUSTOM) //Custom White only
				{
					ColorxyY whitecolor = ColorxyY(in2[3], in2[4]);
					GetColorApp()->m_pColorReference = new CColorReference(ColorStandard(in[2]), WhiteTarget(in[0]), -1, " modified", ColorXYZ(whitecolor));
				}
				else if (in[2] == CUSTOM && in[0] != DCUST) 	//Custom Color only
				{
					ColorxyY redcolor = ColorxyY(in2[6], in2[9]);
					ColorxyY greencolor = ColorxyY(in2[7], in2[10]);
					ColorxyY bluecolor = ColorxyY(in2[5], in2[8]);
					GetColorApp()->m_pColorReference = new CColorReference(ColorStandard(in[2]), WhiteTarget(in[0]), -1, " modified", GetColorApp()->m_pColorReference->GetWhite(), redcolor, greencolor, bluecolor);
				}
				else if (in[2] == CUSTOM && in[0] == DCUST)	//Both
				{
					ColorxyY whitecolor = ColorxyY(in2[3], in2[4]);
					ColorxyY redcolor = ColorxyY(in2[6], in2[9]);
					ColorxyY greencolor = ColorxyY(in2[7], in2[10]);
					ColorxyY bluecolor = ColorxyY(in2[5], in2[8]);
					GetColorApp()->m_pColorReference = new CColorReference(ColorStandard(in[2]), WhiteTarget(in[0]), -1, " modified", ColorXYZ(whitecolor), redcolor, greencolor, bluecolor);
				}
				else
					GetColorApp()->m_pColorReference = new CColorReference(ColorStandard(in[2]), WhiteTarget(in[0]), in[6]);
				GetConfig()->m_dE_form = in[3];
				GetConfig()->m_dE_gray = in[4];
				GetConfig()->gw_Weight = in[5];
				GetConfig()->m_GammaOffsetType = in[6];

				GetConfig()->m_GammaRef = in2[0];
				GetConfig()->m_GammaRel = in2[1];
				GetConfig()->m_Split = in2[2];
				GetConfig()->m_manualWhitex = in2[3];
				GetConfig()->m_manualWhitey = in2[4];

				GetConfig()->m_useMeasuredGamma = in[7];

				GetConfig()->m_manualBluex = in2[5];
				GetConfig()->m_manualRedx = in2[6];
				GetConfig()->m_manualGreenx = in2[7];
				GetConfig()->m_manualBluey = in2[8];
				GetConfig()->m_manualRedy = in2[9];
				GetConfig()->m_manualGreeny = in2[10];
			}

		}

		if (version > 10)
		{
			ar >> m_bOverRideBlack;
			m_userBlack.Serialize(ar);
		}

		int size, gsize;

		ar >> gsize;
		m_grayMeasureArray.SetSize(gsize);
		for(int i=0;i<m_grayMeasureArray.GetSize();i++)
			m_grayMeasureArray[i].Serialize(ar);

		// Version 18: explicit grayscale IRE levels (else fall back to even distribution)
		m_grayIRELevelArray.RemoveAll();
		if ( version > 17 )
		{
			int lsize;
			ar >> lsize;
			m_grayIRELevelArray.SetSize(lsize);
			for(int i=0;i<m_grayIRELevelArray.GetSize();i++)
				ar >> m_grayIRELevelArray[i];
		}

		if ( version > 2 )
		{
			ar >> size;
			m_nearBlackMeasureArray.SetSize(size);
			for(int i=0;i<m_nearBlackMeasureArray.GetSize();i++)
				m_nearBlackMeasureArray[i].Serialize(ar);

			ar >> size;
			m_nearWhiteMeasureArray.SetSize(size);
			for(int i=0;i<m_nearWhiteMeasureArray.GetSize();i++)
				m_nearWhiteMeasureArray[i].Serialize(ar);
		}
		else
		{
			m_nearBlackMeasureArray.SetSize(5);
			m_nearWhiteMeasureArray.SetSize(5);
			for(int i=0;i<m_nearBlackMeasureArray.GetSize();i++)
			{
				m_nearBlackMeasureArray[i]=noDataColor;
				m_nearWhiteMeasureArray[i]=noDataColor;
			}
		}

		if ( version > 1 )
		{
			ar >> size;
			m_redSatMeasureArray.SetSize(size);
			for(int i=0;i<m_redSatMeasureArray.GetSize();i++)
				m_redSatMeasureArray[i].Serialize(ar);

			ar >> size;
			m_greenSatMeasureArray.SetSize(size);
			for(int i=0;i<m_greenSatMeasureArray.GetSize();i++)
				m_greenSatMeasureArray[i].Serialize(ar);

			ar >> size;
			m_blueSatMeasureArray.SetSize(size);
			for(int i=0;i<m_blueSatMeasureArray.GetSize();i++)
				m_blueSatMeasureArray[i].Serialize(ar);

			ar >> size;
			m_yellowSatMeasureArray.SetSize(size);
			for(int i=0;i<m_yellowSatMeasureArray.GetSize();i++)
				m_yellowSatMeasureArray[i].Serialize(ar);

			ar >> size;
			m_cyanSatMeasureArray.SetSize(size);
			for(int i=0;i<m_cyanSatMeasureArray.GetSize();i++)
				m_cyanSatMeasureArray[i].Serialize(ar);

			ar >> size;
			m_magentaSatMeasureArray.SetSize(size);
			for(int i=0;i<m_magentaSatMeasureArray.GetSize();i++)
				m_magentaSatMeasureArray[i].Serialize(ar);

			if ( version >= 8 )
			{
				ar >> size;
				m_cc24SatMeasureArray.SetSize(max(size, MAX_USER_CC_PATCH_SIZE));
				if ( version <= 12)
				{
					for(int i=0;i<size;i++)
						m_cc24SatMeasureArray[i].Serialize(ar);
				} else
				{
					for(int i=0;i<size;i++)
					{
						CColor inColor;
						int j = 0;
						inColor.Serialize(ar);
						if (inColor.GetX() == 0.123 && inColor.GetY() == 0.456 && inColor.GetZ() == 0.789)
							break;
						else
						{
							ar >> j;
							m_cc24SatMeasureArray[j] = inColor;
						}
					}
				}
				if ( version >= 10)
				{
					ar >> size;
					m_cc24SatMeasureArray_master.SetSize(max(size, 5 * MAX_USER_CC_PATCH_SIZE));
					if (version <= 12)
					{
						for(int i=0;i<size;i++)
							m_cc24SatMeasureArray_master[i].Serialize(ar);
						m_CCStr=GetCCStr();
					}
					else //version for smaller save files (doesn't save/restore nodata values)
					{
						for(int i=0;i<size;i++)
						{
							CColor inColor;
							int j = 0;
							inColor.Serialize(ar);
							if (inColor.GetX() == 0.123 && inColor.GetY() == 0.456 && inColor.GetZ() == 0.789)
								break;
							else
							{
								ar >> j;
								m_cc24SatMeasureArray_master[j] = inColor;
							}
							m_CCStr=GetCCStr();
						}
					}
				}
				else
					m_bpreV10 = 1;
			}
			else
				m_bpreV10 = 1;			
		}
		else
		{
			m_redSatMeasureArray.SetSize(5);
			m_greenSatMeasureArray.SetSize(5);
			m_blueSatMeasureArray.SetSize(5);
			m_yellowSatMeasureArray.SetSize(5);
			m_cyanSatMeasureArray.SetSize(5);
			m_magentaSatMeasureArray.SetSize(5);
			m_cc24SatMeasureArray.SetSize(RANDOM250);
			for(int i=0;i<m_cc24SatMeasureArray.GetSize();i++) 
				m_cc24SatMeasureArray[i]=noDataColor;
			for(int i=0;i<m_redSatMeasureArray.GetSize();i++)
			{
				m_redSatMeasureArray[i]=noDataColor;
				m_greenSatMeasureArray[i]=noDataColor;
				m_blueSatMeasureArray[i]=noDataColor;
				m_yellowSatMeasureArray[i]=noDataColor;
				m_cyanSatMeasureArray[i]=noDataColor;
				m_magentaSatMeasureArray[i]=noDataColor;
			}
		}
		
		ar >> size;
		m_measurementsArray.SetSize(size);
		for(int i=0;i<m_measurementsArray.GetSize();i++)
			m_measurementsArray[i].Serialize(ar);

		m_primariesArray[0].Serialize(ar);
		m_primariesArray[1].Serialize(ar);
		m_primariesArray[2].Serialize(ar);

		m_secondariesArray[0].Serialize(ar);
		m_secondariesArray[1].Serialize(ar);
		m_secondariesArray[2].Serialize(ar);

		m_OnOffBlack.Serialize(ar);
		if (!m_OnOffBlack.isValid() && gsize > 0)
			m_OnOffBlack = 	m_grayMeasureArray[0];
		m_OnOffWhite.Serialize(ar);
		m_AnsiBlack.Serialize(ar);
		m_AnsiWhite.Serialize(ar);
		if ( version > 8 )
			m_PrimeWhite.Serialize(ar);
		else
		{
			m_PrimeWhite = m_OnOffWhite;
			if (gsize > 0)
				m_OnOffWhite = m_grayMeasureArray[gsize-1];
		}

		ar >> m_infoStr;
		if (m_infoStr.Find("\n") < 1)
			SetInfoString((CString)"Calibration by: \r\nDisplay: \r\nNote: \r\n");

		if ( version > 4 && version < 7 )
		{
            BOOL bUseAdjustmentMatrix;
            Matrix XYZAdjustmentMatrix;
            CString XYZAdjustmentComment;
			ar >> bUseAdjustmentMatrix;
			XYZAdjustmentMatrix.Serialize(ar);
			ar >> XYZAdjustmentComment;
		}

		if ( version > 5 )
			ar >> m_bIREScaleMode;
		else
			m_bIREScaleMode = FALSE;

		// Version 19: multi-level saturation store
		m_satLevelStore.clear();
		m_activeSatLevel = 1.0;
		if ( version > 18 )
		{
			int nLevels;
			ar >> m_activeSatLevel;
			ar >> nLevels;
			m_satLevelStore.resize ( nLevels );
			for ( int s = 0; s < nLevels; s++ )
			{
				ar >> m_satLevelStore[s].stimLevel;
				for ( int c = 0; c < 6; c++ )
				{
					int nColors;
					ar >> nColors;
					m_satLevelStore[s].sat[c].resize ( nColors );
					for ( int i = 0; i < nColors; i++ )
						m_satLevelStore[s].sat[c][i].Serialize(ar);
				}
			}
		}

		// Version 20: display profile capture
		ClearProfileMeasures();
		if ( version > 19 )
		{
			ar >> m_profileCubeSize;
			ar >> m_profileGrayExtras;
			ar >> m_profileDriftComp;
			ar >> m_profileCaptureSeconds;

			ar >> size;
			// a count outside what the app can ever write means the stream is
			// corrupt: abort the load (standard archive error) instead of
			// attempting a multi-GB SetSize from a garbage value
			if ( size < 0 || size > MAX_USER_CC_PATCH_SIZE )
				AfxThrowArchiveException ( CArchiveException::badIndex, NULL );
			m_profileMeasureArray.SetSize(size);
			for(int i=0;i<size;i++)
				m_profileMeasureArray[i]=noDataColor;
			// size+1 iterations: a fully-valid array writes size pairs and THEN the
			// end marker, which must still be consumed to keep the stream in sync
			for(int i=0;i<size+1;i++)
			{
				CColor inColor;
				int j = 0;
				inColor.Serialize(ar);
				if (inColor.GetX() == 0.123 && inColor.GetY() == 0.456 && inColor.GetZ() == 0.789)
					break;
				else
				{
					ar >> j;
					// index comes straight from the archive; guard against a
					// corrupt/hand-edited file writing out of bounds
					if ( j >= 0 && j < size )
						m_profileMeasureArray[j] = inColor;
				}
			}

			int nAnchors;
			ar >> nAnchors;
			// anchors are written every kProfileAnchorInterval patches, so the
			// legit ceiling is tiny; same corrupt-stream guard as above
			if ( nAnchors < 0 || nAnchors > 1024 )
				AfxThrowArchiveException ( CArchiveException::badIndex, NULL );
			m_profileDriftAnchors.resize(nAnchors);
			m_profileDriftAnchorIdx.resize(nAnchors);
			for ( int i = 0; i < nAnchors; i++ )
			{
				ar >> m_profileDriftAnchorIdx[i];
				m_profileDriftAnchors[i].Serialize(ar);
			}
		}
		StoreActiveSatLevel();	// seed/sync the active entry from the bound sweeps

	}
	m_isModified = FALSE;
}

CColor CMeasure::GetProfileMeasure(int i) const
{
	ASSERT(i >= 0 && i < m_profileMeasureArray.GetSize());
	return m_profileMeasureArray[i];
}

void CMeasure::ClearProfileMeasures()
{
	m_profileMeasureArray.SetSize(0);
	m_profileDriftAnchors.clear();
	m_profileDriftAnchorIdx.clear();
	m_profileCubeSize = 0;
	m_profileGrayExtras = FALSE;
	m_profileDriftComp = FALSE;
	m_profileCaptureSeconds = 0.0;
	m_profileGenCache.clear();
	m_profileGenCacheKey = -1;
}

ColorRGBDisplay CMeasure::GetProfilePatchRGB(int i)
{
	int key = m_profileCubeSize * 2 + ( m_profileGrayExtras ? 1 : 0 );
	if ( key != m_profileGenCacheKey )
	{
		int n = GenerateProfileColors ( NULL, 0, m_profileCubeSize, m_profileGrayExtras != FALSE );
		m_profileGenCache.assign ( ( n > 0 ) ? n : 0, ColorRGBDisplay(0.0) );
		if ( n > 0 )
			GenerateProfileColors ( &m_profileGenCache[0], n, m_profileCubeSize, m_profileGrayExtras != FALSE );
		m_profileGenCacheKey = key;
	}
	if ( i < 0 || i >= (int)m_profileGenCache.size() )
		return ColorRGBDisplay(0.0);
	return m_profileGenCache[i];
}

// Theoretical reference for profile patch i: the same signal-path model the
// grid applies to color-checker patches (GetRefCC24Sat's generic branch) --
// gamma-decode, XYZ round-trip through the working color space, clamp, 8-bit
// video quantization, then the configured gamma/EOTF -- sourced from the
// generated patch stimulus instead of a CC table.
void CMeasure::GetRefProfileSat(int i, CColor & ccRef)
{
	ColorRGBDisplay rgbd = GetProfilePatchRGB ( i );
	CColorReference cRef = GetColorReference();
	CColor White = GetGray ( GetGrayScaleSize() - 1 );
	CColor Black = GetOnOffBlack();
	double gamma = GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef);
	CColor tempColor;
	int mode = GetConfig()->m_GammaOffsetType;
	if (GetConfig()->m_colorStandard == sRGB) mode = 99;

	// Snap the reference onto the SAME native video grid the generators and the
	// (simulated) sensor use, so patch == reference == sensor -> 0 dE. Formerly a
	// hardcoded floor(v*219+0.5)/219 (8-bit limited only); use SnapToVideoGrid so
	// 10-bit / full-range configs land on 876 / 1023 / 255 instead of 219.
	bool b10 = GetConfig()->GetUse10bitLevels();
	bool lim = GetConfig()->GetRGB16_235();

	// Linearize the drive percents. The profile is a NON-RECALC set (the cube is
	// displayed as-is, modulo the pseudo-space remap below): in modes 5/7 the
	// percents are EOTF-encoded signals, so linearize with the pure signal EOTF
	// exactly as GetRefCC24Sat's non-recalc pseudo-space branch and
	// RemapProfileToTransport do -- PQ with m_TargetMaxL = 10000 passed explicitly
	// (the display's tone clip belongs to the decode below, never to the signal
	// round trip; /100 puts it on the 1.0 = 10000 nits scale the -5 encoder
	// expects), HLG via the display-independent inverse OETF. Using pow(2.22)
	// here in PQ blew the reference luminance up to peak on every mid patch.
	double r, g, b;
	if ( mode == 5 || mode == 7 )
	{
		double sr = rgbd[0] / 100., sg = rgbd[1] / 100., sb = rgbd[2] / 100.;
		if ( mode == 7 )
		{
			r = HLG_SignalToScene(sr);
			g = HLG_SignalToScene(sg);
			b = HLG_SignalToScene(sb);
		}
		else
		{
			r = (sr <= 0.0) ? 0.0 : getL_EOTF(sr, noDataColor, noDataColor, 0.0, 0.0, 5, 94.37844, 0.0, 4000.0, 0.0, 10000.0) / 100.;
			g = (sg <= 0.0) ? 0.0 : getL_EOTF(sg, noDataColor, noDataColor, 0.0, 0.0, 5, 94.37844, 0.0, 4000.0, 0.0, 10000.0) / 100.;
			b = (sb <= 0.0) ? 0.0 : getL_EOTF(sb, noDataColor, noDataColor, 0.0, 0.0, 5, 94.37844, 0.0, 4000.0, 0.0, 10000.0) / 100.;
		}
	}
	else
	{
		r = pow(rgbd[0]/100.,2.22);
		g = pow(rgbd[1]/100.,2.22);
		b = pow(rgbd[2]/100.,2.22);
	}

	// UHDTV3/4 (P3-in-2020): define the patch in INNER (content, e.g. P3) space,
	// then read it back as the TRANSPORT (BT.2020) wire encoding -- the exact
	// GetRefCC24Sat model. The capture remaps the DISPLAYED cube the same way
	// (RemapProfileToTransport), so measured == reference and both land inside the
	// inner (P3) gamut. For plain (non-container) standards the round trip is an
	// identity, so this reduces to the raw-wire model.
	tempColor.SetRGBValue(ColorRGB(r,g,b), (GetColorReference().m_standard==UHDTV3||GetColorReference().m_standard==UHDTV4)?ContainerInnerReference(GetColorReference()):cRef);
	ColorRGB aRGBColor = tempColor.GetRGBValue((GetColorReference().m_standard==UHDTV3||GetColorReference().m_standard==UHDTV4)?ContainerTransportReference(GetColorReference()):cRef);
	r = aRGBColor[0];
	g = aRGBColor[1];
	b = aRGBColor[2];

	// same NaN guard as GetRefCC24Sat: round-trip can go fractionally negative
	if (r < 0.) r = 0.;
	if (g < 0.) g = 0.;
	if (b < 0.) b = 0.;

	double qr,qg,qb;
	if (mode == 5 || mode == 7)
	{
		// r,g,b are the inner->transport-remapped LINEAR values (SET inner / GET
		// transport above), so PQ/HLG-encode them to the wire signal, snap on the
		// native grid, then decode -- the identical chain as GetRefCC24Sat's
		// (non-rawWire) HDR branch. The capture applies the matching remap+encode
		// (RemapProfileToTransport), so reference == displayed signal.
		qr = getL_EOTF(r,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, -1*mode);
		qg = getL_EOTF(g,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, -1*mode);
		qb = getL_EOTF(b,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, -1*mode);
		qr = SnapToVideoGrid( qr, b10, lim );
		qg = SnapToVideoGrid( qg, b10, lim );
		qb = SnapToVideoGrid( qb, b10, lim );
		r = getL_EOTF(qr,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) / (mode==5?100.:1.0);
		g = getL_EOTF(qg,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) / (mode==5?100.:1.0);
		b = getL_EOTF(qb,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) / (mode==5?100.:1.0);
	}
	// mode 99 (sRGB standard) must take this branch too: without it the sRGB
	// reference stayed 2.22-decoded and UNQUANTIZED while the wire/sensor are
	// sRGB-decoded and grid-snapped -- same omission GetRefCC24Sat fixed
	// (see its "|| mode == 99" and comment; worst on dark patches).
	if ( mode == 6 || mode == 4 || mode == 8 || mode == 99 )
	{
		qr = (r==0)?0:pow(r, 1.0 / 2.22);
		qg = (g==0)?0:pow(g, 1.0 / 2.22);
		qb = (b==0)?0:pow(b, 1.0 / 2.22);
		qr = SnapToVideoGrid( qr, b10, lim );
		qg = SnapToVideoGrid( qg, b10, lim );
		qb = SnapToVideoGrid( qb, b10, lim );
		r=(r<=0||r>=1)?min(max(r,0),1):getL_EOTF(qr,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode);
		g=(g<=0||g>=1)?min(max(g,0),1):getL_EOTF(qg,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode);
		b=(b<=0||b>=1)?min(max(b,0),1):getL_EOTF(qb,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode);
	}
	else if ( mode < 4 )
	{
		qr = (r==0)?0:pow(r, 1.0 / 2.22);
		qg = (g==0)?0:pow(g, 1.0 / 2.22);
		qb = (b==0)?0:pow(b, 1.0 / 2.22);
		qr = SnapToVideoGrid( qr, b10, lim );
		qg = SnapToVideoGrid( qg, b10, lim );
		qb = SnapToVideoGrid( qb, b10, lim );
		r=(qr<=0||qr>=1)?min(max(qr,0),1):pow(qr, gamma);
		g=(qg<=0||qg>=1)?min(max(qg,0),1):pow(qg, gamma);
		b=(qb<=0||qb>=1)?min(max(qb,0),1):pow(qb, gamma);
	}

	ccRef.ClearSpectrumLux();
	ccRef.SetRGBValue(ColorRGB(r,g,b),(GetColorReference().m_standard==UHDTV3||GetColorReference().m_standard==UHDTV4)?ContainerTransportReference(GetColorReference()):cRef);
}

// dE for a measured profile patch against its theoretical reference, using the
// SAME conventions as the measures grid (CMainView::GetItemText, CC24 branch):
// bRef standard selection, gw_Weight, and -- crucially -- the PQ-HDR bridge
// (mode 5) that scales the normalized reference to absolute nits and adjusts
// YWhite/RefWhite via tmWhite. SDR (mode != 5) keeps the plain relative path.
// Returns -1.0 to skip (invalid / blackish / non-finite).
//
// pdL/pdC/pdH optionally receive the component split of the SAME dE. They are
// filled here, sharing the reference and white derived below, because a caller
// that re-derived them would have to duplicate the white-selection chain and the
// mode-5 nits bridge -- exactly the divergence GetColorDENorm was introduced to
// stop. Pass NULL (via the ComputeProfileDE wrapper) to skip the extra work.
double CMeasure::ComputeProfileDEEx(const CColor & c, int i, ProfileDEParts * pParts)
{
	// Reset the breakdown up front: every skip path below returns -1.0 early, and
	// a caller that folded an untouched component into a running sum would be
	// accumulating whatever the struct happened to hold.
	if ( pParts )
		*pParts = ProfileDEParts();

	if ( !c.isValid() )
		return -1.0;
	ColorXYZ xyz = c.GetXYZValue();

	// White reference: the SHARED chain, not a copy of it. This used to re-derive
	// prime-white-then-on/off-then-sub-90% by hand and, in doing so, had dropped
	// the special-standard (HDTVa/HDTVb) branch that every other consumer of a
	// profile patch applies - RGBLevelWnd gates it on `m_displayMode > 4`, which
	// includes profile mode 13 (RGBLevelWnd.cpp ~220). The dE printed for a patch
	// and the RGB-levels bars drawn beside it therefore normalised by DIFFERENT
	// whites under those two standards.
	//
	// bCC = true because mode 13 shares the color-checker white chain: the
	// sub-90%-stimulus fallback is gated on `m_displayMode == 11 || 13` in
	// RGBLevelWnd, not on 11 alone.
	//
	// The call is guarded rather than unconditional because GetColorDEWhiteY ends
	// in the grid's m_TargetMaxL fallback, which would pre-empt the profile-cube
	// fallback below - the one thing this function must NOT inherit from the grid,
	// since a standalone profile capture has no grayscale or primaries run at all.
	CColor prime = GetPrimeWhite();
	CColor onoff = GetOnOffWhite();
	bool bSpecial = ( GetConfig()->m_colorStandard == HDTVa || GetConfig()->m_colorStandard == HDTVb );
	double ywForDE = 0.0;
	if ( ( prime.isValid() && prime.GetY() > 0.0 ) || ( onoff.isValid() && onoff.GetY() > 0.0 ) )
		ywForDE = GetColorDEWhiteY( bSpecial, true, false );

	// Standalone capture (no grayscale/white measured): fall back to the measured
	// white cube node (stimulus 100/100/100 = last grid patch) so dE still shows.
	if ( ywForDE <= 0.0 && m_profileCubeSize >= 2 )
	{
		int wi = m_profileCubeSize * m_profileCubeSize * m_profileCubeSize - 1;
		if ( wi >= 0 && wi < m_profileMeasureArray.GetSize() &&
			 m_profileMeasureArray[wi].isValid() && m_profileMeasureArray[wi].GetY() > 0.0 )
			ywForDE = m_profileMeasureArray[wi].GetY();
	}

	// (near-)black has no defined chromaticity; a chroma dE against it is bogus
	if ( ( xyz[0] + xyz[1] + xyz[2] ) < 1e-6 || ywForDE <= 0.0 )
		return -1.0;

	CColor refC;
	GetRefProfileSat( i, refC );
	if ( !refC.isValid() )
		return -1.0;

	CColorReference cRef = GetColorReference();
	CColorReference bRef = ( cRef.m_standard == UHDTV3 || cRef.m_standard == UHDTV4 ) ? ContainerTransportReference( cRef )
						 : ( cRef.m_standard == HDTVa  || cRef.m_standard == HDTVb  ) ? CColorReference( HDTV )
						 : cRef;
	int mode = GetConfig()->m_GammaOffsetType;
	int gw = ( mode == 5 ) ? 3 : GetConfig()->gw_Weight;

	double YWhite = ywForDE, RefWhite = 1.0;
	if ( mode == 5 )	// PQ HDR: match the grid's absolute-nits bridge
	{
		// Same chain as the grid's CC24 dE (GetItemText): reference rescaled
		// from the 1.0 = 10000 nits convention by GetHDRRefScale (the unified,
		// tone-map-aware form the 3D viewer also uses; = 105.95640 with tone
		// mapping off), measurement anchored to the measured white as-is.
		double tmWhite = TmDiffuseWhiteNits( GetOnOffWhite(), GetOnOffBlack() );
		if ( tmWhite > 0.0 )
		{
			// 10000/tmWhite IS GetHDRRefScale() here (mode-5 getL_EOTF ignores
			// White/Black), and the guard above already covers its <= 0 case -
			// calling it would re-run the PQ + BT.2390 chain on every patch, and
			// this is per-patch code (a 21-cube is 9261 of them).
			double s = 10000. / tmWhite;
			refC.SetX( refC.GetX() * s );
			refC.SetY( refC.GetY() * s );
			refC.SetZ( refC.GetZ() * s );
			RefWhite = YWhite / tmWhite;
		}
	}

	double dE = c.GetDeltaE( YWhite, refC, RefWhite, bRef, GetConfig()->m_dE_form, false, gw );
	if ( !( dE == dE ) || dE < 0.0 || dE > 1.0e6 )	// NaN / negative / absurd
		return -1.0;

	// Component split on request. GetDeltaLCH redoes the Lab/Luv conversion, so
	// this is gated: the per-patch hot paths (3D viewer rebuild, Export) pass NULL
	// and pay nothing. The costly part -- GetRefProfileSat above -- is shared.
	//
	// Caveat for callers, NOT correctable here: dChrom/dHue are only genuinely
	// chroma and hue for dE_form 2..5. Form 0 (CIE76uv) fills them with |du|/|dv|
	// and form 1 (CIE76ab) with |da|/|db|, so a UI labelling them "chroma"/"hue"
	// under those forms would be lying; combine them into a single colour term
	// instead. Form 6 (ICtCp) carries its own 240x scaling.
	if ( pParts )
	{
		double dC = 0.0, dH = 0.0;
		double dL = c.GetDeltaLCH( YWhite, refC, RefWhite, bRef, GetConfig()->m_dE_form, false, gw, dC, dH );
		// same shape of guard as the total: a non-finite component fed into a
		// running sum-of-squares would poison every later statistic
		if ( !( dL == dL ) || dL < 0.0 || dL > 1.0e6 ) dL = 0.0;
		if ( !( dC == dC ) || dC < 0.0 || dC > 1.0e6 ) dC = 0.0;
		if ( !( dH == dH ) || dH < 0.0 || dH > 1.0e6 ) dH = 0.0;
		pParts->dL = dL; pParts->dC = dC; pParts->dH = dH;
	}
	return dE;
}

void CMeasure::SetGrayScaleSize(int steps)
{
	int OldSize = m_grayMeasureArray.GetSize ();

	m_grayMeasureArray.SetSize(steps);

	if ( steps != OldSize )
	{
		// Purge all actual results
		for(int i=0;i<m_grayMeasureArray.GetSize();i++)	// Init default values: by default m_grayMeasureArray init to D65, Y=1
		{
			m_grayMeasureArray[i]=noDataColor;
		}

		// A genuine resize drops any explicit (preset) level set; fall back to the
		// legacy even distribution computed live by ArrayIndexToGrayLevel().
		// A same-size call (e.g. from the background measure validators) leaves an
		// explicit, possibly non-uniform, level set intact.
		m_grayIRELevelArray.RemoveAll();
	}
}

void CMeasure::SetGrayScaleLevels(const double * pLevels, int count)
{
	int OldSize = m_grayMeasureArray.GetSize ();

	// Purge measured results only when the effective measurement points move
	// (count changed, or any level differs from the current effective level).
	bool bChanged = ( count != OldSize );
	if ( ! bChanged )
	{
		for (int i = 0; i < count && ! bChanged; i++)
		{
			double cur = ( m_grayIRELevelArray.GetSize() == count )
						 ? m_grayIRELevelArray[i]
						 : ( ( count > 1 ) ? ( i * 100.0 / (count - 1) ) : 0.0 );	// implied uniform ramp
			if ( fabs(cur - pLevels[i]) > 1e-9 )
				bChanged = true;
		}
	}

	m_grayMeasureArray.SetSize(count);
	m_grayIRELevelArray.SetSize(count);
	for (int i = 0; i < count; i++)
		m_grayIRELevelArray[i] = pLevels[i];

	if ( bChanged )
	{
		for (int i = 0; i < count; i++)
			m_grayMeasureArray[i] = noDataColor;
	}
}

double CMeasure::GetGrayPercent(int index, bool bUseRoundDown, bool b10bit, bool is16_235) const
{
	int size = m_grayMeasureArray.GetSize ();

	if ( m_grayIRELevelArray.GetSize() == size && index >= 0 && index < size )
	{
		// Snap the stored nominal IRE % to the active output grid. The stored
		// levels are nominal percentages, never codes, so this is the one place
		// the grid is applied to them - GrayLevelToGrayProp is the same snap
		// expressed as a 0..1 proportion, so route through it and there is a
		// single definition of the grid rounding.
		// For a uniform level set this reproduces ArrayIndexToGrayLevel() exactly.
		//
		// SAVED DOCUMENTS: m_grayIRELevelArray serializes the NOMINAL levels, so
		// nothing on disk is rewritten by the grid choice - the snap is applied
		// live, at display time, from the CURRENT config. A .chc saved while the
		// wire was full-range therefore reloads with 255-grid IRE labels (10.196%
		// where the limited grid reads 10.046%), and the same document opened on
		// a limited-range setup reads the limited labels. That is the same
		// already-established behavior as toggling the 10-bit checkbox, and it is
		// the correct one: the label must describe the grid the user is measuring
		// on now, not the one the file happened to be captured on. The stored
		// measurements are untouched either way.
		return GrayLevelToGrayProp ( m_grayIRELevelArray[index], bUseRoundDown, b10bit, is16_235 ) * 100.0;
	}

	return ArrayIndexToGrayLevel ( index, size, bUseRoundDown, b10bit, is16_235 );
}

int CMeasure::GetGrayScalePreset() const
{
	const GrayScalePreset *	pPresets = GetGrayScalePresets();
	int						nPresetCount = GetGrayScalePresetCount();
	int						size = m_grayMeasureArray.GetSize ();
	bool					bHasExplicit = ( m_grayIRELevelArray.GetSize() == size );

	for (int k = 0; k < nPresetCount; k++)
	{
		if ( pPresets[k].count != size )
			continue;

		bool match = true;
		for (int i = 0; i < size && match; i++)
		{
			// Compare against stored nominal levels, or the implied uniform ramp.
			double level = bHasExplicit ? m_grayIRELevelArray[i]
										: ( ( size > 1 ) ? ( i * 100.0 / (size - 1) ) : 0.0 );
			if ( fabs(level - pPresets[k].levels[i]) > 1e-9 )
				match = false;
		}
		if ( match )
			return k;
	}
	return -1;	// custom
}

void CMeasure::SetIREScaleMode(BOOL bIRE)
{
	if ( bIRE != m_bIREScaleMode )
	{
		m_bIREScaleMode = bIRE;

		// Purge all actual results
		for(int i=0;i<m_grayMeasureArray.GetSize();i++)	// Init default values: by default m_grayMeasureArray init to D65, Y=1
		{
			m_grayMeasureArray[i]=noDataColor;	
		}
	}
}

void CMeasure::SetNearBlackScaleSize(int steps)
{
	int OldSize = m_nearBlackMeasureArray.GetSize ();

	m_nearBlackMeasureArray.SetSize(steps);

	if ( steps != OldSize )
	{
		// Purge all actual results
		for(int i=0;i<m_nearBlackMeasureArray.GetSize();i++)
		{
			m_nearBlackMeasureArray[i]=noDataColor;	
		}
	}
}

void CMeasure::SetNearWhiteScaleSize(int steps)
{
	int OldSize = m_nearWhiteMeasureArray.GetSize ();

	m_nearWhiteMeasureArray.SetSize(steps);

	if ( steps != OldSize )
	{
		// Purge all actual results
		for(int i=0;i<m_nearWhiteMeasureArray.GetSize();i++)
		{
			m_nearWhiteMeasureArray[i]=noDataColor;	
		}
	}
}

void CMeasure::SetSaturationSize(int steps)
{
	int OldSize = m_redSatMeasureArray.GetSize ();

	m_redSatMeasureArray.SetSize(steps);
	m_greenSatMeasureArray.SetSize(steps);
	m_blueSatMeasureArray.SetSize(steps);
	m_yellowSatMeasureArray.SetSize(steps);
	m_cyanSatMeasureArray.SetSize(steps);
	m_magentaSatMeasureArray.SetSize(steps);

	if ( steps != OldSize )
	{
		// Purge all actual results
		for(int i=0;i<m_redSatMeasureArray.GetSize();i++)	// Init default values
		{
			m_redSatMeasureArray[i]=noDataColor;
			m_greenSatMeasureArray[i]=noDataColor;
			m_blueSatMeasureArray[i]=noDataColor;
			m_yellowSatMeasureArray[i]=noDataColor;
			m_cyanSatMeasureArray[i]=noDataColor;
			m_magentaSatMeasureArray[i]=noDataColor;
		}

		// The step count changed, so every stored level's sweeps are stale too
		for ( size_t s = 0; s < m_satLevelStore.size(); s++ )
			for ( int c = 0; c < 6; c++ )
				m_satLevelStore[s].sat[c].assign ( steps, noDataColor );
	}
}

// ---- Multi-level saturation store --------------------------------------

static int FindSatLevelIndex ( const std::vector<CSatLevelSet> & store, double level )
{
	for ( size_t s = 0; s < store.size(); s++ )
		if ( fabs ( store[s].stimLevel - level ) < 1e-4 )
			return (int) s;
	return -1;
}

void CMeasure::StoreActiveSatLevel()
{
	int idx = FindSatLevelIndex ( m_satLevelStore, m_activeSatLevel );
	if ( idx < 0 )
	{
		CSatLevelSet emptySet;
		emptySet.stimLevel = m_activeSatLevel;

		// keep the store sorted by level so dropdowns list naturally
		idx = 0;
		while ( idx < (int) m_satLevelStore.size () && m_satLevelStore[idx].stimLevel < m_activeSatLevel )
			idx ++;
		m_satLevelStore.insert ( m_satLevelStore.begin () + idx, emptySet );
	}

	CSatLevelSet & set = m_satLevelStore[idx];
	const CArray<CColor,CColor> * pArrays[6] =
		{ &m_redSatMeasureArray, &m_greenSatMeasureArray, &m_blueSatMeasureArray,
		  &m_yellowSatMeasureArray, &m_cyanSatMeasureArray, &m_magentaSatMeasureArray };
	for ( int c = 0; c < 6; c++ )
	{
		set.sat[c].resize ( pArrays[c]->GetSize () );
		for ( int i = 0; i < pArrays[c]->GetSize (); i++ )
			set.sat[c][i] = pArrays[c]->GetAt ( i );
	}
}

BOOL CMeasure::BindSatLevel(double level)
{
	if ( fabs ( level - m_activeSatLevel ) < 1e-4 )
		return FALSE;

	StoreActiveSatLevel ();

	int nSize = m_redSatMeasureArray.GetSize ();
	CArray<CColor,CColor> * pArrays[6] =
		{ &m_redSatMeasureArray, &m_greenSatMeasureArray, &m_blueSatMeasureArray,
		  &m_yellowSatMeasureArray, &m_cyanSatMeasureArray, &m_magentaSatMeasureArray };

	int idx = FindSatLevelIndex ( m_satLevelStore, level );
	for ( int c = 0; c < 6; c++ )
	{
		for ( int i = 0; i < nSize; i++ )
		{
			if ( idx >= 0 && i < (int) m_satLevelStore[idx].sat[c].size () )
				pArrays[c]->SetAt ( i, m_satLevelStore[idx].sat[c][i] );
			else
				pArrays[c]->SetAt ( i, noDataColor );
		}
	}

	m_activeSatLevel = level;
	StoreActiveSatLevel ();	// materialize the entry (and heal a size mismatch)
	m_isModified = TRUE;
	return TRUE;
}

int CMeasure::GetSatLevelCount()
{
	StoreActiveSatLevel ();
	return (int) m_satLevelStore.size ();
}

double CMeasure::GetSatLevelAt(int idx)
{
	return m_satLevelStore[idx].stimLevel;
}

const CSatLevelSet & CMeasure::GetSatLevelSet(int idx)
{
	// Callers iterate by index after GetSatLevelCount() (which syncs the active
	// entry), so no per-call StoreActiveSatLevel is needed here -- and re-storing
	// mid-iteration could reallocate the vector and invalidate a returned reference.
	return m_satLevelStore[idx];
}

void GetSatStimLevelPercents ( std::vector<int> & pcts )
{
	pcts.clear ();
	CString strLevels = GetConfig () -> GetProfileString ( "Scale Sizes", "SatStimLevels", "25 50 75 100" );
	LPCSTR p = (LPCSTR) strLevels;
	while ( *p )
	{
		char * pEnd;
		long v = strtol ( p, &pEnd, 10 );
		if ( pEnd == p ) { p ++; continue; }
		if ( v >= 1 && v <= 100 )
			pcts.push_back ( (int) v );
		p = pEnd;
	}
}


void CMeasure::StartLuxMeasure ()
{
	GetColorApp () -> BeginLuxMeasure ();
}

UINT CMeasure::GetLuxMeasure ( double * pValue )
{
	UINT	nRet = LUX_NOMEASURE;
	UINT	nLuxRetCode;
	BOOL	bContinue;
	double	dLuxValue;
	CString	Msg, Title;
	CLuxScaleAdvisor *	pDlg = NULL;

	do
	{
		bContinue = FALSE;
		nLuxRetCode = GetColorApp () -> GetLuxMeasure ( & dLuxValue );
		switch ( nLuxRetCode )
		{
			case LUXMETER_OK:
				 * pValue = dLuxValue;
				 nRet = LUX_OK;
				 break;
			
			case LUXMETER_NOT_RUNNING:
				 * pValue = 0.0;
				 nRet = LUX_NOMEASURE;
				 break;

			case LUXMETER_SCALE_TOO_HIGH:
			case LUXMETER_SCALE_TOO_LOW:
				 // Request scale adjustment
				 bContinue = TRUE;
				 if ( pDlg == NULL )
				 {
					// Create advisor dialog
					pDlg = new CLuxScaleAdvisor;
					pDlg -> Create ( pDlg -> IDD, NULL );
					pDlg -> UpdateWindow ();
				 }
				 else
				 {
					// Test if advisor button has been clicked
					if ( pDlg -> m_bCancel )
					{
						 * pValue = 0.0;
						 nRet = LUX_CANCELED;
						 bContinue = FALSE;
					}
					else if ( pDlg -> m_bContinue )
					{
						 * pValue = 0.0;
						 nRet = LUX_NOMEASURE;
						 bContinue = FALSE;
					}
				 }
				 break;

			case LUXMETER_SCALE_TOO_HIGH_MIN:
			case LUXMETER_SCALE_TOO_LOW_MAX:
				 Title.LoadString ( IDS_ERROR );
				 Msg.LoadString ( IDS_LUXMETER_OUTOFRANGE );
				 GetColorApp()->InMeasureMessageBox(Msg,Title,MB_OK | MB_ICONINFORMATION);
				 * pValue = 0.0;
				 nRet = LUX_NOMEASURE;
				 break;

			case LUXMETER_NEW_SCALE_OK:
				 bContinue = TRUE;
				 if ( pDlg )
				 {
					// Advisor dialog has been opened: close it
					pDlg -> DestroyWindow ();
					delete pDlg;
					pDlg = NULL;
					GetColorApp()->BringPatternWindowToTop ();
				 }
				 
				 // Reset measures
				 * pValue = 0.0;
				 WaitForDynamicIris ( TRUE );
				 GetColorApp () -> BeginLuxMeasure ();
				 break;
		}

		if ( bContinue )
		{
			// Sleep 50 ms while dispatching messages to the advisor dialog
			Sleep(50);
			
			MSG	Msg;
			while(PeekMessage(&Msg, NULL, NULL, NULL, PM_REMOVE))
			{
				TranslateMessage( &Msg );
				DispatchMessage( &Msg );
			}
		}
	} while ( bContinue );

	if ( pDlg )
	{
		// This case should not occur
		ASSERT ( 0 );

		// Advisor dialog has been opened: close it
		pDlg -> DestroyWindow ();
		delete pDlg;
		pDlg = NULL;
		GetColorApp()->BringPatternWindowToTop ();
		GetColorApp () -> BeginLuxMeasure ();
	}

	return nRet;
}
bool doSettling = FALSE;

BOOL CMeasure::MeasureGrayScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	SweepActiveGuard _sweepGuard(this);
	if (!_sweepGuard.Owned()) return FALSE;
	MSG		Msg;
	BOOL	bEscape;
	BOOL	bPatternRetry = FALSE;
	BOOL	bRetry = FALSE;
	int		size=m_grayMeasureArray.GetSize();
	CString	strMsg, Title;
	double	dLuxValue;

	CArray<CColor,int> measuredColor;
//	CColor previousColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

	measuredColor.SetSize(size);
	measuredLux.SetSize(size);

	if(pGenerator->Init(size) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		if (pGenerator->m_initShowedError) return FALSE;
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pGenerator->CanDisplayScale ( CGenerator::MT_IRE, size ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
	CAsyncMeasurer asyncMeasure;
	asyncMeasure.Start(pSensor);
	
	m_binMeasure = TRUE;
	m_currentIndex = 0;
	for(int i=(CheckBlackOverride()?1:0);i<size;i++)
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;

		UpdateViews(pDoc, 0);
		
		if (!i && (GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE) == DISPLAY_GDI_Hide) )
			UpdateTstWnd(pDoc, -1);
		if( pGenerator->DisplayGray(GetGrayPercent ( i, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235()),CGenerator::MT_IRE ,!bRetry))
		{
			UpdateTstWnd(pDoc, i);
			bEscape = WaitForDynamicIris (FALSE, pDoc);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i] = PumpedRead(asyncMeasure, pSensor, ColorRGBDisplay(GetGrayPercent ( i, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235())));

				m_grayMeasureArray[i] = measuredColor[i];
				
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			if ( bUseLuxValues && ! bEscape && i == 0 )
			{
				int		nNbLoops = 0;
				BOOL	bContinue;
				
				// Measuring black: ask and reask luxmeter value until it stabilizes
				do
				{
					bContinue = FALSE;
					nNbLoops ++;
					
					StartLuxMeasure ();
					
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 if ( dLuxValue < measuredLux[0] )
							 {
								measuredLux[0] = dLuxValue;
								bContinue = TRUE;
							 }
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				} while ( bContinue && nNbLoops < 10 );
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}
			if ( m_bAbortSweep ) bEscape = TRUE;

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;
				lastColor = measuredColor[i];
	
				if(i != 0)
				{
					if (!pGenerator->HasPatternChanged(CGenerator::MT_IRE,previousColor,lastColor))
					{
						i--;
						bPatternRetry = TRUE;
					}
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}
	GetConfig()->m_isSettling = doSettling;

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		GetColorApp()->InMeasureMessageBox(pGenerator->GetRetryMessage(), NULL, MB_OK | MB_ICONWARNING);

	for(int i=0;i<size;i++)
	{
		if (!i && m_bOverRideBlack)
			m_grayMeasureArray[i] = m_userBlack;
		else
			m_grayMeasureArray[i] = measuredColor[i];

		if ( bUseLuxValues )
			m_grayMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_grayMeasureArray[i].ResetLuxValue ();
	}

	m_OnOffWhite = measuredColor[size-1];
	if (m_bOverRideBlack)
		m_OnOffBlack = m_userBlack;
	else		
		m_OnOffBlack = measuredColor[0];

	if ( bUseLuxValues )
	{
		m_OnOffWhite.SetLuxValue ( measuredLux[size-1] );
		m_OnOffBlack.SetLuxValue ( measuredLux[0] );
	}
	else
	{
		m_OnOffWhite.ResetLuxValue ();
		m_OnOffBlack.ResetLuxValue ();
	}

	m_binMeasure = FALSE;
	UpdateViews(pDoc, 0);
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureGrayScaleAndColors(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	SweepActiveGuard _sweepGuard(this);
	if (!_sweepGuard.Owned()) return FALSE;
	MSG		Msg;
	BOOL	bEscape;
	BOOL	bPatternRetry = FALSE;
	BOOL	bRetry = FALSE, isSpecial = FALSE;
	int		size=m_grayMeasureArray.GetSize();
	CString	strMsg, Title;
	double	dLuxValue;

	CArray<CColor,int> measuredColor;
//	CColor previousColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

	measuredColor.SetSize(size+6+GetConfig()->m_BWColorsToAdd);
	measuredLux.SetSize(size+6+GetConfig()->m_BWColorsToAdd);

	if(pGenerator->Init(size+6+GetConfig()->m_BWColorsToAdd) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		if (pGenerator->m_initShowedError) return FALSE;
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pGenerator->CanDisplayGrayAndColorsSeries() != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
	CAsyncMeasurer asyncMeasure;
	asyncMeasure.Start(pSensor);

	m_binMeasure = TRUE;
	m_currentIndex = 0;
	for(int i=(CheckBlackOverride()?1:0);i<size;i++)
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;
		
		UpdateViews(pDoc, 0);

		if (!i && GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE) == DISPLAY_GDI_Hide)
			UpdateTstWnd(pDoc, -1);

		if( pGenerator->DisplayGray(GetGrayPercent ( i, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235()),CGenerator::MT_IRE ,!bRetry))
		{
			UpdateTstWnd(pDoc, i);
			bEscape = WaitForDynamicIris (FALSE, pDoc);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i] = PumpedRead(asyncMeasure, pSensor, ColorRGBDisplay(GetGrayPercent ( i, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235() )));
				m_grayMeasureArray[i] = measuredColor[i];
				
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			if ( bUseLuxValues && ! bEscape && i == 0 )
			{
				int		nNbLoops = 0;
				BOOL	bContinue;
				
				// Measuring black: ask and reask luxmeter value until it stabilizes
				do
				{
					bContinue = FALSE;
					nNbLoops ++;
					
					StartLuxMeasure ();
					
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 if ( dLuxValue < measuredLux[0] )
							 {
								measuredLux[0] = dLuxValue;
								bContinue = TRUE;
							 }
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				} while ( bContinue && nNbLoops < 10 );
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}
			if ( m_bAbortSweep ) bEscape = TRUE;

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release(size - 1 - i);
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[i];
		
				if(i != 0)
				{
					if (!pGenerator->HasPatternChanged(CGenerator::MT_IRE,previousColor,lastColor))
					{
						i--;
						bPatternRetry = TRUE;
					}
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}
	
	// Generator init to change pattern series if necessary
	if ( !pGenerator->ChangePatternSeries())
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);
	int mode = GetConfig()->m_GammaOffsetType;

	double primaryIRELevel=100.0;	
	if (mode == 5)
	{
		primaryIRELevel = 50.22831;
	//Special Case white for Mascior's HDR disk
		if(pGenerator->GetName() == str && (GetColorReference().m_standard == UHDTV4 || GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV2 || GetColorReference().m_standard == UHDTV || GetColorReference().m_standard == HDTV))
			primaryIRELevel = 50.00;
	}
	// Measure primary and secondary colors
	ColorRGBDisplay	GenColors [ 8 ] = 
								{	
									ColorRGBDisplay(primaryIRELevel,0,0),
									ColorRGBDisplay(0,primaryIRELevel,0),
									ColorRGBDisplay(0,0,primaryIRELevel),
									ColorRGBDisplay(primaryIRELevel,primaryIRELevel,0),
									ColorRGBDisplay(0,primaryIRELevel,primaryIRELevel),
									ColorRGBDisplay(primaryIRELevel,0,primaryIRELevel),
									ColorRGBDisplay(primaryIRELevel,primaryIRELevel,primaryIRELevel),
									ColorRGBDisplay(0,0,0)
								};
	if (GetColorReference().m_standard == HDTVb)
	{
			GenColors [ 0 ] = ColorRGBDisplay(79.9087,10.0457,10.0457); 
			GenColors [ 1 ] = ColorRGBDisplay(30.137,79.9087,30.137); 
			GenColors [ 2 ] = ColorRGBDisplay(50.2283,50.2283,79.9087); 
			GenColors [ 3 ] = ColorRGBDisplay(79.9087,79.9087,10.0457);
			GenColors [ 4 ] = ColorRGBDisplay(10.0457,79.9087,79.9087);
			GenColors [ 5 ] = ColorRGBDisplay(79.9087,10.0457,79.9087);
			isSpecial = TRUE;
	}
	else if (GetColorReference().m_standard == HDTVa) //75%
	{ 
		GenColors [ 0 ] = ColorRGBDisplay(68.04,20.09,20.09);
		GenColors [ 1 ] = ColorRGBDisplay(27.85,73.06,27.85);
		GenColors [ 2 ] = ColorRGBDisplay(19.18,19.18,50.22);
		GenColors [ 3 ] = ColorRGBDisplay(73.9726,73.9726,33.3333);
		GenColors [ 4 ] = ColorRGBDisplay(36.07,73.06,73.06);
		GenColors [ 5 ] = ColorRGBDisplay(64.3836,29.2237,64.3836);
		GenColors [ 6 ] = ColorRGBDisplay(75.0,75.0,75.0);
		isSpecial = TRUE;
	}
	else if ( GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4 ) //P3/Rec709 in BT.2020
	{
		// GenColors[0..5] are computed from ContainerPrimaryLinear below
		// (all transfer functions).
		if (!(mode == 5 || mode == 7))
			isSpecial = TRUE;

		GenColors [ 6 ] = ColorRGBDisplay(primaryIRELevel,primaryIRELevel,primaryIRELevel);
		GenColors [ 7 ] = ColorRGBDisplay(0,0,0);
	}

	// Pseudo color spaces: build the primary/secondary patches from the inner
	// primaries mapped into the transport container, encoded with the ACTIVE
	// transfer function (2.22 SDR, PQ, HLG OETF) - the same chain GetRefSat
	// models. Left unquantized: the wire and the reference each snap ONCE to
	// the active grid and land on the same code. (The old hardcoded HDR tables
	// were pre-quantized on the 8-bit LIMITED grid - double-quantizing them
	// onto the full/10-bit grids left patches a code off the reference - and
	// predate the transport-space references entirely, which is why HLG read
	// several dE.)
	if (GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4)
	{
		CColor tmW = CMeasure::GetGray ( CMeasure::GetGrayScaleSize() - 1 );	// same White/Black GetRefSat's encode uses
		CColor tmB = CMeasure::GetOnOffBlack();
		for (int ci = 0; ci < 6; ci++)
		{
			ColorRGB clin = ContainerPrimaryLinear(GetColorReference(), ci);
			for (int ck = 0; ck < 3; ck++)
			{
				double cv = clin[ck];
				if (mode == 5)
					cv = (cv <= 0.0) ? 0.0 : getL_EOTF(cv / 105.95640, tmW, tmB, GetConfig()->m_GammaRel, GetConfig()->m_Split, -5);
				else if (mode == 7)
					cv = (cv <= 0.0) ? 0.0 : getL_EOTF(cv, tmW, tmB, GetConfig()->m_GammaRel, GetConfig()->m_Split, -7);
				else
					cv = (cv <= 0.0 || cv >= 1.0) ? min(max(cv, 0.0), 1.0) : pow(cv, 1.0 / 2.22);
				GenColors[ci][ck] = min(max(cv, 0.0), 1.0) * 100.0;
			}
		}
	}

	if ( (mode == 5 || mode  == 7) && isSpecial)
	{
		for (int i=0;i<=5;i++)
		{
			GenColors[i][0]= 100. * getL_EOTF(pow(GenColors[i][0] / 100.,2.22), noDataColor, noDataColor,0,0,-1*mode);
			GenColors[i][1]= 100. * getL_EOTF(pow(GenColors[i][1] / 100.,2.22), noDataColor, noDataColor,0,0,-1*mode);
			GenColors[i][2]= 100. * getL_EOTF(pow(GenColors[i][2] / 100.,2.22), noDataColor, noDataColor,0,0,-1*mode);
		}
	}

	m_currentIndex = 0;
	for (int i = 0; i < 6 + GetConfig()->m_BWColorsToAdd ; i ++ )
	{
		UpdateViews(pDoc, 1);
		if (!i && GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE) == DISPLAY_GDI_Hide)
			UpdateTstWnd(pDoc, -1);

		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_SECONDARY,i,TRUE,TRUE) )
		{
			UpdateTstWnd(pDoc, i);
			bEscape = WaitForDynamicIris (FALSE, pDoc);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[size+i] = PumpedRead(asyncMeasure, pSensor, GenColors[i], displaymode);
				if (i<3)
					m_primariesArray[i] = measuredColor[size+i];
				if (i>=3&&i<6)
					m_secondariesArray[i-3] = measuredColor[size+i];
				
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[size+i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}
			if ( m_bAbortSweep ) bEscape = TRUE;

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[size+i];
	
				if (!pGenerator->HasPatternChanged(CGenerator::MT_SECONDARY,previousColor,lastColor))
				{
					i--;
					bPatternRetry = TRUE;
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	for(int i=0;i<size;i++)
	{
		if (!i && m_bOverRideBlack)
			m_grayMeasureArray[i] = m_userBlack;
		else
			m_grayMeasureArray[i] = measuredColor[i];

		if ( bUseLuxValues )
			m_grayMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_grayMeasureArray[i].ResetLuxValue ();
	}

	for(int i=0;i<3;i++)
	{
		m_primariesArray[i] = measuredColor[i+size];

		if ( bUseLuxValues )
			m_primariesArray[i].SetLuxValue ( measuredLux[i+size] );
		else
			m_primariesArray[i].ResetLuxValue ();
	}

	for(int i=0;i<3;i++)
	{
		m_secondariesArray[i] = measuredColor[i+size+3];

		if ( bUseLuxValues )
			m_secondariesArray[i].SetLuxValue ( measuredLux[i+size+3] );
		else
			m_secondariesArray[i].ResetLuxValue ();
	}

	m_OnOffWhite = measuredColor[size-1];
	if (m_bOverRideBlack)
		m_OnOffBlack = m_userBlack;
	else
		m_OnOffBlack = measuredColor[0];

	if ( bUseLuxValues )
	{
		m_OnOffWhite.SetLuxValue ( measuredLux[size-1] );
		m_OnOffBlack.SetLuxValue ( measuredLux[0] );
	}
	else
	{
		m_OnOffWhite.ResetLuxValue ();
//		m_OnOffBlack.ResetLuxValue ();
	}

	if ( GetConfig () -> m_BWColorsToAdd > 0 )
	{
		m_PrimeWhite = measuredColor[size+6];                
		if ( bUseLuxValues )
			m_PrimeWhite.SetLuxValue ( measuredLux[size+6] );
		else
			m_PrimeWhite.ResetLuxValue ();
	}
	GetConfig()->m_isSettling = doSettling;
		
	m_binMeasure = FALSE;
	UpdateViews(pDoc, 1);
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureNearBlackScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	SweepActiveGuard _sweepGuard(this);
	if (!_sweepGuard.Owned()) return FALSE;
	MSG		Msg;
	BOOL	bEscape;
	BOOL	bPatternRetry = FALSE;
	BOOL	bRetry = FALSE;
	int		size=m_nearBlackMeasureArray.GetSize();
	CString	strMsg, Title;
	double	dLuxValue;

	CArray<CColor,int> measuredColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

	measuredColor.SetSize(size);
	measuredLux.SetSize(size);

	if(pGenerator->Init(size) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		if (pGenerator->m_initShowedError) return FALSE;
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pGenerator->CanDisplayScale ( CGenerator::MT_NEARBLACK, size ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
	CAsyncMeasurer asyncMeasure;
	asyncMeasure.Start(pSensor);


	m_binMeasure = TRUE;
	m_currentIndex = 0;
	for(int i=(CheckBlackOverride()?1:0);i<size;i++)
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;

		UpdateViews(pDoc, 3);
		if (!i && GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE) == DISPLAY_GDI_Hide)
			UpdateTstWnd(pDoc, -1);

		int nCol = i * (GetConfig()->m_GammaOffsetType == 5 ? 2 : 1);
		if( pGenerator->DisplayGray((ArrayIndexToGrayLevel ( nCol, 101, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235())),CGenerator::MT_NEARBLACK,!bRetry) )
		{
			UpdateTstWnd(pDoc, nCol);
			bEscape = WaitForDynamicIris (FALSE, pDoc);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i] = PumpedRead(asyncMeasure, pSensor, ColorRGBDisplay(ArrayIndexToGrayLevel ( nCol, 101, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235())));
				m_nearBlackMeasureArray[i] = measuredColor[i];
				
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			if ( bUseLuxValues && ! bEscape && i == 0 )
			{
				int		nNbLoops = 0;
				BOOL	bContinue;
				
				// Measuring black: ask and reask luxmeter value until it stabilizes
				do
				{
					bContinue = FALSE;
					nNbLoops ++;
					
					StartLuxMeasure ();
					
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 if ( dLuxValue < measuredLux[0] )
							 {
								measuredLux[0] = dLuxValue;
								bContinue = TRUE;
							 }
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				} while ( bContinue && nNbLoops < 10 );
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}
			if ( m_bAbortSweep ) bEscape = TRUE;

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[i];
	
				if(i != 0)
				{
					if (!pGenerator->HasPatternChanged(CGenerator::MT_NEARBLACK,previousColor,lastColor))
					{
						i--;
						bPatternRetry = TRUE;
					}
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	for(int i=0;i<size;i++)
	{
		if (!i && m_bOverRideBlack)
			m_nearBlackMeasureArray[i] = m_userBlack;
		else
			m_nearBlackMeasureArray[i] = measuredColor[i];
		if ( bUseLuxValues )
			m_nearBlackMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_nearBlackMeasureArray[i].ResetLuxValue ();
	}
	GetConfig()->m_isSettling = doSettling;

	m_binMeasure = FALSE;
	UpdateViews(pDoc, 3);
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureNearWhiteScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	SweepActiveGuard _sweepGuard(this);
	if (!_sweepGuard.Owned()) return FALSE;
	MSG		Msg;
	BOOL	bEscape;
	BOOL	bPatternRetry = FALSE;
	BOOL	bRetry = FALSE;
	int		size=m_nearWhiteMeasureArray.GetSize();
	CString	strMsg, Title;
	double	dLuxValue;
	double	YMax = 10000.;

	if (m_OnOffWhite.GetY() > 0)
		YMax = m_OnOffWhite.GetY();

	CArray<CColor,int> measuredColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

	measuredColor.SetSize(size);
	measuredLux.SetSize(size);

	if(pGenerator->Init(size) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		if (pGenerator->m_initShowedError) return FALSE;
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pGenerator->CanDisplayScale ( CGenerator::MT_NEARWHITE, size ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}
	
	CString pName = pSensor->GetName();
	if(pSensor->Init(pName == "Simulated sensor"?TRUE:FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
	CAsyncMeasurer asyncMeasure;
	asyncMeasure.Start(pSensor);

	m_binMeasure = TRUE;
	m_currentIndex = 0;
	m_currentSequence = 4;
	for(int i=0;i<size;i++)
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;

		UpdateViews(pDoc, 4);
		if (!i && GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE) == DISPLAY_GDI_Hide)
			UpdateTstWnd(pDoc, -1);

		//Autoscale range for clipped white in HDR mode
		double tmWhite = TmDiffuseWhiteNits(noDataColor, noDataColor) / 94.37844;

		double PMax = getL_EOTF(YMax / 10000. / tmWhite, noDataColor, noDataColor, 0, 0, -5, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL,  GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);

		if (GetConfig()->m_GammaOffsetType == 5)
			m_NearWhiteClipCol = int(101*PMax) + 1;
		else
			m_NearWhiteClipCol = 101;

		if( pGenerator->DisplayGray( (ArrayIndexToGrayLevel ( m_NearWhiteClipCol - size + i, 101, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235()) ),CGenerator::MT_NEARWHITE,!bRetry ) )
		{
			UpdateTstWnd(pDoc, i);
			bEscape = WaitForDynamicIris (FALSE, pDoc);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i] = PumpedRead(asyncMeasure, pSensor, ColorRGBDisplay(ArrayIndexToGrayLevel ( m_NearWhiteClipCol - size+i, 101, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235())));
				m_nearWhiteMeasureArray[i] = measuredColor[i];
				
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}
			if ( m_bAbortSweep ) bEscape = TRUE;

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[i];

				if(i != 0)
				{
					if (!pGenerator->HasPatternChanged(CGenerator::MT_NEARWHITE,previousColor,lastColor))
					{
						i--;
						bPatternRetry = TRUE;
					}
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	for(int i=0;i<size;i++)
	{
		m_nearWhiteMeasureArray[i] = measuredColor[i];
		if ( bUseLuxValues )
			m_nearWhiteMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_nearWhiteMeasureArray[i].ResetLuxValue ();
	}
	GetConfig()->m_isSettling = doSettling;

	m_binMeasure = FALSE;
	UpdateViews(pDoc, 4);
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureRedSatScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	SweepActiveGuard _sweepGuard(this);
	if (!_sweepGuard.Owned()) return FALSE;
	MSG			Msg;
	BOOL		bEscape;
	BOOL		bPatternRetry = FALSE;
	BOOL		bRetry = FALSE;
	int			size = GetSaturationSize ();
	CString		strMsg, Title;
	ColorRGBDisplay	GenColors [ 256 ];
	double		dLuxValue;

	CArray<CColor,int> measuredColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

	measuredColor.SetSize(size);
	measuredLux.SetSize(size);

	if(pGenerator->Init(size) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		if (pGenerator->m_initShowedError) return FALSE;
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pGenerator->CanDisplayScale ( CGenerator::MT_SAT_RED, size ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
	CAsyncMeasurer asyncMeasure;
	asyncMeasure.Start(pSensor);

	// Generate saturation colors for red
	GenerateSaturationColors (GetColorReference(), GenColors,size, true, false, false, GetConfig()->m_GammaOffsetType, m_activeSatLevel, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());
	CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);

	m_binMeasure = TRUE;
	m_currentIndex = 0;
    for(int i=((GetConfig()->m_CCMode == MCD && pGenerator->GetName() == str && GetConfig()->m_GammaOffsetType != 5 )?1:0);i<size;i++)
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;
		
		UpdateViews(pDoc, 5);

		if (!i && GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE) == DISPLAY_GDI_Hide)
			UpdateTstWnd(pDoc, -1);
		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_SAT_RED,100*i/(size - 1),!bRetry))
		{
			UpdateTstWnd(pDoc, i);
			bEscape = WaitForDynamicIris (FALSE, pDoc);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i] = PumpedRead(asyncMeasure, pSensor, GenColors[i], displaymode);
				m_redSatMeasureArray[i] = measuredColor[i];
				
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}
			if ( m_bAbortSweep ) bEscape = TRUE;

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[i];
	
				if(i != 0)
				{
					if (!pGenerator->HasPatternChanged(CGenerator::MT_SAT_RED,previousColor,lastColor))
					{
						i--;
						bPatternRetry = TRUE;
					}
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	for(int i=0;i<size;i++)
	{
		m_redSatMeasureArray[i] = measuredColor[i];
		if ( bUseLuxValues )
			m_redSatMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_redSatMeasureArray[i].ResetLuxValue ();
	}
	GetConfig()->m_isSettling = doSettling;

	m_binMeasure = FALSE;
	UpdateViews(pDoc, 5);
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureGreenSatScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	SweepActiveGuard _sweepGuard(this);
	if (!_sweepGuard.Owned()) return FALSE;
	MSG			Msg;
	BOOL		bEscape;
	BOOL		bPatternRetry = FALSE;
	BOOL		bRetry = FALSE;
	int			size = GetSaturationSize ();
	CString		strMsg, Title;
	ColorRGBDisplay	GenColors [ 256 ];
	double		dLuxValue;

	CArray<CColor,int> measuredColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

	measuredColor.SetSize(size);
	measuredLux.SetSize(size);

	if(pGenerator->Init(size) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		if (pGenerator->m_initShowedError) return FALSE;
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pGenerator->CanDisplayScale ( CGenerator::MT_SAT_GREEN, size ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
	CAsyncMeasurer asyncMeasure;
	asyncMeasure.Start(pSensor);
	// Generate saturation colors for green
	GenerateSaturationColors (GetColorReference(), GenColors,size, false, true, false, GetConfig()->m_GammaOffsetType, m_activeSatLevel, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());
	CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);

	m_binMeasure = TRUE;
	m_currentIndex = 0;
	for(int i=((GetConfig()->m_CCMode == MCD && pGenerator->GetName() == str && GetConfig()->m_GammaOffsetType != 5)?1:0);i<size;i++)
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;
		UpdateViews(pDoc, 6);

		if (!i && GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE) == DISPLAY_GDI_Hide)
			UpdateTstWnd(pDoc, -1);

		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_SAT_GREEN,100*i/(size - 1),!bRetry) )
		{
			UpdateTstWnd(pDoc, i);
			bEscape = WaitForDynamicIris (FALSE, pDoc);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i] = PumpedRead(asyncMeasure, pSensor, GenColors[i], displaymode);
				m_greenSatMeasureArray[i] = measuredColor[i];
				
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}
			if ( m_bAbortSweep ) bEscape = TRUE;

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[i];
	
				if(i != 0)
				{
					if (!pGenerator->HasPatternChanged(CGenerator::MT_SAT_GREEN,previousColor,lastColor))
					{
						i--;
						bPatternRetry = TRUE;
					}
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	for(int i=0;i<size;i++)
	{
		m_greenSatMeasureArray[i] = measuredColor[i];
		if ( bUseLuxValues )
			m_greenSatMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_greenSatMeasureArray[i].ResetLuxValue ();
	}
	GetConfig()->m_isSettling = doSettling;

	m_binMeasure = FALSE;
	UpdateViews(pDoc, 6);
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureBlueSatScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	SweepActiveGuard _sweepGuard(this);
	if (!_sweepGuard.Owned()) return FALSE;
	MSG			Msg;
	BOOL		bEscape;
	BOOL		bPatternRetry = FALSE;
	BOOL		bRetry = FALSE;
	int			size = GetSaturationSize ();
	CString		strMsg, Title;
	ColorRGBDisplay	GenColors [ 256 ];
	double		dLuxValue;

	CArray<CColor,int> measuredColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

	measuredColor.SetSize(size);
	measuredLux.SetSize(size);

	if(pGenerator->Init(size) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		if (pGenerator->m_initShowedError) return FALSE;
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pGenerator->CanDisplayScale ( CGenerator::MT_SAT_BLUE, size ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
	CAsyncMeasurer asyncMeasure;
	asyncMeasure.Start(pSensor);

	// Generate saturation colors for blue
		GenerateSaturationColors (GetColorReference(), GenColors,size, false, false, true, GetConfig()->m_GammaOffsetType, m_activeSatLevel, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());
	CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);

	m_binMeasure = TRUE;
	m_currentIndex = 0;
    for(int i=((GetConfig()->m_CCMode == MCD && pGenerator->GetName() == str && GetConfig()->m_GammaOffsetType != 5)?1:0);i<size;i++)
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;

		UpdateViews(pDoc, 7);

		if (!i && GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE) == DISPLAY_GDI_Hide)
			UpdateTstWnd(pDoc, -1);

		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_SAT_BLUE,100*i/(size - 1),!bRetry))
		{
			UpdateTstWnd(pDoc, i);
			bEscape = WaitForDynamicIris (FALSE, pDoc);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i] = PumpedRead(asyncMeasure, pSensor, GenColors[i], displaymode);
				m_blueSatMeasureArray[i] = measuredColor[i];
				
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}
			if ( m_bAbortSweep ) bEscape = TRUE;

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[i];
	
				if(i != 0)
				{
					if (!pGenerator->HasPatternChanged(CGenerator::MT_SAT_BLUE,previousColor,lastColor))
					{
						i--;
						bPatternRetry = TRUE;
					}
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	for(int i=0;i<size;i++)
	{
		m_blueSatMeasureArray[i] = measuredColor[i];
		if ( bUseLuxValues )
			m_blueSatMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_blueSatMeasureArray[i].ResetLuxValue ();
	}
	GetConfig()->m_isSettling = doSettling;

	m_binMeasure = FALSE;
	UpdateViews(pDoc, 7);
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureYellowSatScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	SweepActiveGuard _sweepGuard(this);
	if (!_sweepGuard.Owned()) return FALSE;
	MSG			Msg;
	BOOL		bEscape;
	BOOL		bPatternRetry = FALSE;
	BOOL		bRetry = FALSE;
	int			size = GetSaturationSize ();
	CString		strMsg, Title;
	ColorRGBDisplay	GenColors [ 256 ];
	double		dLuxValue;

	CArray<CColor,int> measuredColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

	measuredColor.SetSize(size);
	measuredLux.SetSize(size);

	if(pGenerator->Init(size) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		if (pGenerator->m_initShowedError) return FALSE;
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pGenerator->CanDisplayScale ( CGenerator::MT_SAT_YELLOW, size ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
	CAsyncMeasurer asyncMeasure;
	asyncMeasure.Start(pSensor);

	// Generate saturation colors for yellow
	GenerateSaturationColors (GetColorReference(), GenColors,size, true, true, false, GetConfig()->m_GammaOffsetType, m_activeSatLevel, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());
	CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);

	m_binMeasure = TRUE;
	m_currentIndex = 0;
    for(int i=((GetConfig()->m_CCMode == MCD && pGenerator->GetName() == str && GetConfig()->m_GammaOffsetType != 5)?1:0);i<size;i++)
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;

		UpdateViews(pDoc, 8);

		if (!i && GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE) == DISPLAY_GDI_Hide)
			UpdateTstWnd(pDoc, -1);
		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_SAT_YELLOW,100*i/(size - 1),!bRetry))
		{
			UpdateTstWnd(pDoc, i);
			bEscape = WaitForDynamicIris (FALSE, pDoc);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i] = PumpedRead(asyncMeasure, pSensor, GenColors[i], displaymode);
				m_yellowSatMeasureArray[i] = measuredColor[i];
				
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}
			if ( m_bAbortSweep ) bEscape = TRUE;

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[i];
	
				if(i != 0)
				{
					if (!pGenerator->HasPatternChanged(CGenerator::MT_SAT_YELLOW,previousColor,lastColor))
					{
						i--;
						bPatternRetry = TRUE;
					}
				}
			}

		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	for(int i=0;i<size;i++)
	{
		m_yellowSatMeasureArray[i] = measuredColor[i];
		if ( bUseLuxValues )
			m_yellowSatMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_yellowSatMeasureArray[i].ResetLuxValue ();
	}
	GetConfig()->m_isSettling = doSettling;

	m_binMeasure = FALSE;
	UpdateViews(pDoc, 8);
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureCyanSatScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	SweepActiveGuard _sweepGuard(this);
	if (!_sweepGuard.Owned()) return FALSE;
	MSG			Msg;
	BOOL		bEscape;
	BOOL		bPatternRetry = FALSE;
	BOOL		bRetry = FALSE;
	int			size = GetSaturationSize ();
	CString		strMsg, Title;
	ColorRGBDisplay	GenColors [ 256 ];
	double		dLuxValue;

	CArray<CColor,int> measuredColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

	measuredColor.SetSize(size);
	measuredLux.SetSize(size);

	if(pGenerator->Init(size) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		if (pGenerator->m_initShowedError) return FALSE;
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pGenerator->CanDisplayScale ( CGenerator::MT_SAT_CYAN, size ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
	CAsyncMeasurer asyncMeasure;
	asyncMeasure.Start(pSensor);

	// Generate saturation colors for cyan
	GenerateSaturationColors (GetColorReference(), GenColors,size, false, true, true, GetConfig()->m_GammaOffsetType, m_activeSatLevel, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());
	CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);

	m_binMeasure = TRUE;
	m_currentIndex = 0;
    for(int i=((GetConfig()->m_CCMode == MCD && pGenerator->GetName() == str && GetConfig()->m_GammaOffsetType != 5)?1:0);i<size;i++)
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;

		UpdateViews(pDoc, 9);

		if (!i && GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE) == DISPLAY_GDI_Hide)
			UpdateTstWnd(pDoc, -1);

		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_SAT_CYAN,100*i/(size - 1),!bRetry))
		{
			UpdateTstWnd(pDoc, i);
			bEscape = WaitForDynamicIris (FALSE, pDoc);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i] = PumpedRead(asyncMeasure, pSensor, GenColors[i], displaymode);
				m_cyanSatMeasureArray[i] = measuredColor[i];
				
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}
			if ( m_bAbortSweep ) bEscape = TRUE;

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[i];
	
				if(i != 0)
				{
					if (!pGenerator->HasPatternChanged(CGenerator::MT_SAT_CYAN,previousColor,lastColor))
					{
						i--;
						bPatternRetry = TRUE;
					}
				}
			}

		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	for(int i=0;i<size;i++)
	{
		m_cyanSatMeasureArray[i] = measuredColor[i];
		if ( bUseLuxValues )
			m_cyanSatMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_cyanSatMeasureArray[i].ResetLuxValue ();
	}
	GetConfig()->m_isSettling = doSettling;

	m_binMeasure = FALSE;
	UpdateViews(pDoc, 9);
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureMagentaSatScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	SweepActiveGuard _sweepGuard(this);
	if (!_sweepGuard.Owned()) return FALSE;
	MSG			Msg;
	BOOL		bEscape;
	BOOL		bPatternRetry = FALSE;
	BOOL		bRetry = FALSE;
	int			size = GetSaturationSize ();
	CString		strMsg, Title;
	ColorRGBDisplay	GenColors [ 256 ];
	double		dLuxValue;

	CArray<CColor,int> measuredColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

	measuredColor.SetSize(size);
	measuredLux.SetSize(size);

	if(pGenerator->Init(size) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		if (pGenerator->m_initShowedError) return FALSE;
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pGenerator->CanDisplayScale ( CGenerator::MT_SAT_MAGENTA, size ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
	CAsyncMeasurer asyncMeasure;
	asyncMeasure.Start(pSensor);

	// Generate saturation colors for magenta
	GenerateSaturationColors (GetColorReference(), GenColors,size, true, false, true, GetConfig()->m_GammaOffsetType, m_activeSatLevel, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());
	CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);

	m_binMeasure = TRUE;
	m_currentIndex = 0;
    for(int i=((GetConfig()->m_CCMode == MCD && pGenerator->GetName() == str && GetConfig()->m_GammaOffsetType != 5)?1:0);i<size;i++)
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;

		UpdateViews(pDoc, 10);

		if (!i && GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE) == DISPLAY_GDI_Hide)
			UpdateTstWnd(pDoc, -1);

		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_SAT_MAGENTA,100*i/(size - 1) ,!bRetry))
		{
			UpdateTstWnd(pDoc, i);
			bEscape = WaitForDynamicIris (FALSE, pDoc);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i] = PumpedRead(asyncMeasure, pSensor, GenColors[i], displaymode);
				m_magentaSatMeasureArray[i] = measuredColor[i];
				
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}
			if ( m_bAbortSweep ) bEscape = TRUE;

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[i];
	
				if(i != 0)
				{
					if (!pGenerator->HasPatternChanged(CGenerator::MT_SAT_MAGENTA,previousColor,lastColor))
					{
						i--;
						bPatternRetry = TRUE;
					}
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	for(int i=0;i<size;i++)
	{
		m_magentaSatMeasureArray[i] = measuredColor[i];
		if ( bUseLuxValues )
			m_magentaSatMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_magentaSatMeasureArray[i].ResetLuxValue ();
	}
	GetConfig()->m_isSettling = doSettling;

	m_binMeasure = FALSE;
	UpdateViews(pDoc, 10);
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureCC24SatScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	SweepActiveGuard _sweepGuard(this);
	if (!_sweepGuard.Owned()) return FALSE;
	MSG			Msg;
	BOOL		bEscape;
	BOOL		bPatternRetry = FALSE;
	BOOL		bRetry = FALSE;
	CCPatterns	ccPat = GetConfig()->m_CCMode;
	int			size = ccPat == CCSG?96:(ccPat == CMS || ccPat ==CPS)?19:(ccPat==AXIS?71:24);
	CString		strMsg, Title;
	ColorRGBDisplay	GenColors [MAX_USER_CC_PATCH_SIZE + 10];
	double		dLuxValue;
	BOOL isExtPat =( GetConfig()->m_CCMode == USER || GetConfig()->m_CCMode == CM10SAT || GetConfig()->m_CCMode == CM10SAT75 || GetConfig()->m_CCMode == CM5SAT || GetConfig()->m_CCMode == CM5SAT75 || GetConfig()->m_CCMode == CM4SAT || GetConfig()->m_CCMode == CM4SAT75 || GetConfig()->m_CCMode == CM4LUM || GetConfig()->m_CCMode == CM5LUM || GetConfig()->m_CCMode == CM10LUM || GetConfig()->m_CCMode == RANDOM250 || GetConfig()->m_CCMode == RANDOM500 || GetConfig()->m_CCMode == CM6NB || GetConfig()->m_CCMode == CMDNR || GetConfig()->m_CCMode == MASCIOR50);
	isExtPat = (isExtPat || GetConfig()->m_CCMode > 19);

    if (isExtPat) size = GetConfig()->GetCColorsSize();

    CArray<CColor,int> measuredColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

	measuredColor.SetSize(size);
	measuredLux.SetSize(size);

	if(pGenerator->Init(size) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		if (pGenerator->m_initShowedError) return FALSE;
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}
	// Generate saturation colors for color checker
	CGenerator::MeasureType nPattern;
	switch (GetConfig()->m_CCMode)
	{
	case GCD:
		 nPattern=CGenerator::MT_SAT_CC24_GCD;
		 break;
	case MCD:
		 nPattern=CGenerator::MT_SAT_CC24_MCD;		
		 break;
	case CMC:
		 nPattern=CGenerator::MT_SAT_CC24_CMC;		
		 break;
	case CMS:
		 nPattern=CGenerator::MT_SAT_CC24_CMS;		
		 break;
	case CPS:
		 nPattern=CGenerator::MT_SAT_CC24_CPS;		
		 break;
	case SKIN:
		 nPattern=CGenerator::MT_SAT_CC24_SKIN;
		 break;
	case AXIS:
		 nPattern=CGenerator::MT_SAT_CC24_AXIS;
		 break;
	case CCSG:
		 nPattern=CGenerator::MT_SAT_CC24_CCSG;
		 break;
	default:
		 nPattern=CGenerator::MT_SAT_CC24_USER;
		 break;
	}

	if(pGenerator->CanDisplayScale ( nPattern, size ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
	CAsyncMeasurer asyncMeasure;
	asyncMeasure.Start(pSensor);
	CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);

    if(pGenerator->GetName() == str&&( (GetConfig()->m_CCMode==USER && size > 100) ) )
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		strMsg.Append(" not a supported DVD sequence.");
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}

    if (!GenerateCC24Colors (GetColorReference(), GenColors, GetConfig()->m_CCMode, GetConfig()->m_GammaOffsetType, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235()))
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
	for (int i=0;i<MAX_USER_CC_PATCH_SIZE;i++)
		m_cc24SatMeasureArray[i] = noDataColor;
	m_binMeasure = TRUE;
	m_currentIndex = 0;
	for(int i=0;i<size;i++)
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;

		UpdateViews(pDoc, 11);

		if (!i && GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE) == DISPLAY_GDI_Hide)
			UpdateTstWnd(pDoc, -1);
		if( pGenerator->DisplayRGBColor(GenColors[i], nPattern , i, !bRetry))
		{
			UpdateTstWnd(pDoc, i);
			bEscape = WaitForDynamicIris (FALSE, pDoc);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();
				if (GetConfig()->m_CCMode != MCD)
				{
					measuredColor[i] = PumpedRead(asyncMeasure, pSensor, GenColors[i], displaymode);
				}
				else
				{
					if (i < 18)
						measuredColor[i+6] = PumpedRead(asyncMeasure, pSensor, GenColors[i], displaymode);
					else
						measuredColor[23-i] = PumpedRead(asyncMeasure, pSensor, GenColors[i], displaymode);
				}

				m_cc24SatMeasureArray[i] = measuredColor[i];

				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}
			if ( m_bAbortSweep ) bEscape = TRUE;

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[i];
	
				if(i != 0)
				{
					if (!pGenerator->HasPatternChanged(nPattern,previousColor,lastColor))
					{
						i--;
						bPatternRetry = TRUE;
					}
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	for(int i=0;i<size;i++)
	{
		m_cc24SatMeasureArray[i] = measuredColor[i];
		if ( bUseLuxValues )
			m_cc24SatMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_cc24SatMeasureArray[i].ResetLuxValue ();
	}

	GetConfig()->m_isSettling = doSettling;

	int iCC=GetConfig()->m_CCMode;
	if (iCC < RANDOM250)
		for (int i=0+100*iCC;i<100*(iCC+1);i++)
				m_cc24SatMeasureArray_master[i] = m_cc24SatMeasureArray[i-iCC*100];
	else if (iCC == RANDOM250)
		for (int i=PATTERN_SIZE;i<PATTERN_SIZE+250;i++)
				m_cc24SatMeasureArray_master[i] = m_cc24SatMeasureArray[i-PATTERN_SIZE];
	else if (iCC == RANDOM500)
		for (int i=PATTERN_SIZE+250;i<PATTERN_SIZE+250+500;i++)
				m_cc24SatMeasureArray_master[i] = m_cc24SatMeasureArray[i-(PATTERN_SIZE+250)];
	else if (iCC == USER)
		for (int i=PATTERN_SIZE+250+500;i<PATTERN_SIZE+250+500+MAX_USER_CC_PATCH_SIZE;i++)
				m_cc24SatMeasureArray_master[i] = m_cc24SatMeasureArray[i-(PATTERN_SIZE+250+500)];

	m_binMeasure = FALSE;
	UpdateViews(pDoc, 11);
	m_isModified=TRUE;
	m_CCStr=GetCCStr();
	return TRUE;
}

static const int kProfileAnchorInterval = 64;	// patches between drift-compensation white anchors

// Rescale the XYZ of profile patches [fromIdx, toIdx) by the reciprocal of a
// drift factor interpolated linearly from fFrom (at the previous anchor) to
// fTo (at the anchor just measured). Luminance-only correction: all three
// components share the factor, so chromaticity is preserved. Spectral data,
// when present, is intentionally left raw.
void CMeasure::ApplyProfileDriftSegment(int fromIdx, int toIdx, double fFrom, double fTo)
{
	for ( int j = fromIdx; j < toIdx && j < m_profileMeasureArray.GetSize(); j++ )
	{
		if ( ! m_profileMeasureArray[j].isValid() )
			continue;
		double t = (double)( j - fromIdx + 1 ) / (double)( toIdx - fromIdx );
		double f = fFrom + t * ( fTo - fFrom );
		if ( f <= 0.0 )
			continue;
		// Scalar drift compensation: write it through the stored raw as well
		// (a scalar commutes with the correction matrix), so the raw stays in
		// step with the corrected value it is supposed to reproduce.
		m_profileMeasureArray[j].ScaleXYZ ( 1.0 / f );
	}
}

// Measure a full-white drift anchor before patch patchIdx. A valid anchor
// closes the previous segment (retroactive correction) and becomes the new
// segment start; an invalid sensor read is skipped rather than aborting an
// hours-long capture. Returns false only when the generator itself fails.
bool CMeasure::MeasureProfileDriftAnchor(CAsyncMeasurer & am, CSensor * pSensor, CGenerator * pGenerator, CDataSetDoc * pDoc, int patchIdx, double & firstAnchorY, double & prevFactor, int & prevIdx)
{
	ColorRGBDisplay whiteRGB ( 100.0, 100.0, 100.0 );
	if ( ! pGenerator->DisplayRGBColor ( whiteRGB, CGenerator::MT_SAT_CC24_USER, patchIdx, TRUE ) )
		return false;
	if ( WaitForDynamicIris ( FALSE, pDoc ) )
		m_bAbortSweep = TRUE;
	CColor anchor = PumpedRead ( am, pSensor, whiteRGB, displaymode );
	if ( ! pSensor->IsMeasureValid() || ! anchor.isValid() || anchor.GetY() <= 0.0 )
		return true;

	if ( firstAnchorY <= 0.0 )
	{
		firstAnchorY = anchor.GetY();
		prevFactor = 1.0;
		prevIdx = patchIdx;
	}
	else
	{
		double f = anchor.GetY() / firstAnchorY;
		ApplyProfileDriftSegment ( prevIdx, patchIdx, prevFactor, f );
		prevFactor = f;
		prevIdx = patchIdx;
		m_profileCurrentDrift = f - 1.0;
	}
	m_profileDriftAnchors.push_back ( anchor );
	m_profileDriftAnchorIdx.push_back ( patchIdx );
	return true;
}

BOOL CMeasure::MeasureDisplayProfile(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc, int cubeN, BOOL bGrayExtras, BOOL bDriftComp)
{
	SweepActiveGuard _sweepGuard(this);
	if (!_sweepGuard.Owned()) return FALSE;
	MSG			Msg;
	BOOL		bEscape = FALSE;
	BOOL		bPatternRetry = FALSE;
	BOOL		bRetry = FALSE;
	CString		strMsg, Title;
	double		dLuxValue;

	int size = GenerateProfileColors ( NULL, 0, cubeN, bGrayExtras != FALSE );
	if ( size <= 0 || size > MAX_USER_CC_PATCH_SIZE )
		return FALSE;

	std::vector<ColorRGBDisplay> GenColors ( size );
	if ( GenerateProfileColors ( &GenColors[0], size, cubeN, bGrayExtras != FALSE ) != size )
		return FALSE;

	// UHDTV3/4 (e.g. "P3 in Rec.2020"): GenerateProfileColors is container-agnostic,
	// so remap the DISPLAYED patches inner->transport here -- exactly as
	// GenerateCC24Colors/GenerateSaturationColors do -- so the wire carries the P3
	// content through the 2020 container and the sensor recovers P3 colors. The
	// references (GetProfilePatchRGB/GetRefProfileSat) stay on the raw inner cube,
	// mirroring the CC24 inner-target model, so measured == reference in P3 space.
	RemapProfileToTransport ( &GenColors[0], size, GetColorReference(),
							  GetConfig()->m_GammaOffsetType,
							  GetConfig()->GetUse10bitLevels() != FALSE,
							  GetConfig()->GetRGB16_235() != FALSE );

	BOOL	bUseLuxValues = TRUE;

	if(pGenerator->Init(size) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		if (pGenerator->m_initShowedError) return FALSE;
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	CGenerator::MeasureType nPattern = CGenerator::MT_SAT_CC24_USER;

	if(pGenerator->CanDisplayScale ( nPattern, size ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
	CAsyncMeasurer asyncMeasure;
	asyncMeasure.Start(pSensor);

	// a new capture replaces the document's profile
	ClearProfileMeasures();
	m_profileCubeSize = cubeN;
	m_profileGrayExtras = bGrayExtras;
	m_profileDriftComp = bDriftComp;
	m_profileMeasureArray.SetSize(size);
	for (int i=0;i<size;i++)
		m_profileMeasureArray[i] = noDataColor;
	m_bProfilePause = FALSE;
	m_profileCurrentDrift = 0.0;

	double	firstAnchorY = 0.0;
	double	prevAnchorFactor = 1.0;
	int		prevAnchorIdx = 0;
	DWORD	startTick = GetTickCount();
	int		nDone = 0;

	m_binMeasure = TRUE;
	m_currentIndex = 0;

	// Self-contained white/black reference. A profile inherently drives its own
	// 0/0/0 and 100/100/100 cube corners, so measure them up front and publish
	// the app-wide On/Off white+black -- the user can come straight in and start a
	// profile with no separate grayscale/contrast pass first, because every
	// white-relative consumer (ComputeProfileDE, the 3D viewer, the RGB-levels
	// widget) reads GetOnOffWhite/GetOnOffBlack. Measured only when not already
	// present, so an existing contrast/grayscale run is never overwritten.
	if ( ! m_bAbortSweep && ( ! m_OnOffWhite.isValid() || m_OnOffWhite.GetY() <= 0.0 ) )
	{
		ColorRGBDisplay whRGB ( 100.0, 100.0, 100.0 );
		if ( pGenerator->DisplayRGBColor ( whRGB, nPattern, 0, TRUE ) )
		{
			if ( WaitForDynamicIris ( FALSE, pDoc ) )
				m_bAbortSweep = TRUE;
			CColor wh = PumpedRead ( asyncMeasure, pSensor, whRGB, displaymode );
			if ( pSensor->IsMeasureValid() && wh.isValid() && wh.GetY() > 0.0 )
				m_OnOffWhite = wh;
		}
	}
	if ( ! m_bAbortSweep && ! m_OnOffBlack.isValid() )
	{
		ColorRGBDisplay bkRGB ( 0.0, 0.0, 0.0 );
		if ( pGenerator->DisplayRGBColor ( bkRGB, nPattern, 0, TRUE ) )
		{
			if ( WaitForDynamicIris ( FALSE, pDoc ) )
				m_bAbortSweep = TRUE;
			CColor bk = PumpedRead ( asyncMeasure, pSensor, bkRGB, displaymode );
			if ( pSensor->IsMeasureValid() && bk.isValid() )
				m_OnOffBlack = bk;
		}
	}

	for(int i=0;i<size;i++)
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;

		// pane-driven pause: idle between patches. We must pump mouse + paint so
		// the pane's Resume/Stop buttons (which notify via synchronous SendMessage
		// from their own OnLButtonUp) work and repaint -- but we DROP queued
		// WM_COMMAND / WM_SYSCOMMAND / WM_CLOSE so a menu/accelerator can't reenter
		// OnSelchangeComboMode or tear the document down mid-capture (the mode combo
		// itself is disabled by StartProfileCapture). Sweep guard keeps
		// IsMeasureSweepActive() TRUE throughout.
		while ( m_bProfilePause && ! m_bAbortSweep )
		{
			while ( PeekMessage ( & Msg, NULL, 0, 0, PM_REMOVE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					m_bAbortSweep = TRUE;
				if ( Msg.message == WM_COMMAND || Msg.message == WM_SYSCOMMAND || Msg.message == WM_CLOSE )
					continue;	// don't let it reenter the capture / free the doc
				TranslateMessage ( & Msg );
				DispatchMessage ( & Msg );
			}
			Sleep(50);
		}
		if ( m_bAbortSweep )
			break;


		// white drift anchor at capture start and every kProfileAnchorInterval patches
		if ( bDriftComp && ( i % kProfileAnchorInterval ) == 0 && ! bRetry )
		{
			if ( ! MeasureProfileDriftAnchor ( asyncMeasure, pSensor, pGenerator, pDoc, i, firstAnchorY, prevAnchorFactor, prevAnchorIdx ) )
			{
				pSensor->Release();
				pGenerator->Release();
				ClearProfileMeasures();
				return FALSE;
			}
			if ( m_bAbortSweep )
				break;
		}

		m_currentIndex = i;
		UpdateViews(pDoc, 13);

		if (!i && GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE) == DISPLAY_GDI_Hide)
			UpdateTstWnd(pDoc, -1);
		if( pGenerator->DisplayRGBColor(GenColors[i], nPattern , i, !bRetry))
		{
			bEscape = WaitForDynamicIris (FALSE, pDoc);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				CColor measured = PumpedRead(asyncMeasure, pSensor, GenColors[i], displaymode);

				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measured.SetLuxValue ( dLuxValue );
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
				if ( ! bUseLuxValues )
					measured.ResetLuxValue ();

				m_profileMeasureArray[i] = measured;
				nDone = i + 1;
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}
			if ( m_bAbortSweep ) bEscape = TRUE;

			if ( bEscape )
				break;		// Stop / ESC keeps the partial capture

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
					break;	// keep the partial capture
				if(result == IDRETRY)
				{
					m_profileMeasureArray[i] = noDataColor;
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;
				lastColor = m_profileMeasureArray[i];

				if(i != 0)
				{
					if (!pGenerator->HasPatternChanged(nPattern,previousColor,lastColor))
					{
						i--;
						bPatternRetry = TRUE;
					}
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			ClearProfileMeasures();
			return FALSE;
		}
	}

	// final drift anchor closes the last open segment (also after Stop)
	if ( bDriftComp && firstAnchorY > 0.0 && nDone > prevAnchorIdx )
		MeasureProfileDriftAnchor ( asyncMeasure, pSensor, pGenerator, pDoc, nDone, firstAnchorY, prevAnchorFactor, prevAnchorIdx );

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	GetConfig()->m_isSettling = doSettling;
	m_profileCaptureSeconds = (GetTickCount() - startTick) / 1000.0;
	m_profileDriftComp = bDriftComp && m_profileDriftAnchors.size() >= 2;
	m_binMeasure = FALSE;
	m_bProfilePause = FALSE;
	m_currentIndex = nDone;

	if ( nDone == 0 )
	{
		ClearProfileMeasures();
		UpdateViews(pDoc, 13);
		return FALSE;
	}

	UpdateViews(pDoc, 13);
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureAllSaturationScales(CSensor *pSensor, CGenerator *pGenerator, BOOL bPrimaryOnly, CDataSetDoc *pDoc)
{
	SweepActiveGuard _sweepGuard(this);
	if (!_sweepGuard.Owned()) return FALSE;
	int			i, j;
	MSG			Msg;
	BOOL		bEscape;
	BOOL		bPatternRetry = FALSE;
	BOOL		bRetry = FALSE;
	int			size = GetSaturationSize ();
	CCPatterns	ccPat = GetConfig()->m_CCMode;
	int			ccSize = ccPat == CCSG?96:(ccPat == CMS || ccPat == CPS)?19:(ccPat==AXIS?71:24);
	CString		strMsg, Title;
	std::vector<ColorRGBDisplay> GenColors ( size * 6 + MAX_USER_CC_PATCH_SIZE );

	double		dLuxValue;
	CGenerator::MeasureType nPattern;
	switch (GetConfig()->m_CCMode)
	{
	case MCD:
		 nPattern=CGenerator::MT_SAT_CC24_MCD;
		 break;
	case GCD:
		 nPattern=CGenerator::MT_SAT_CC24_GCD;		
		 break;
	case CMC:
		 nPattern=CGenerator::MT_SAT_CC24_CMC;		
		 break;
	case CMS:
		 nPattern=CGenerator::MT_SAT_CC24_CMS;		
		 break;
	case CPS:
		 nPattern=CGenerator::MT_SAT_CC24_CPS;		
		 break;
	case SKIN:
		 nPattern=CGenerator::MT_SAT_CC24_SKIN;
         break;
	case AXIS:
		 nPattern=CGenerator::MT_SAT_CC24_AXIS;
         break;
	case CCSG:
		 nPattern=CGenerator::MT_SAT_CC24_CCSG;		
         break;
	default:
		 nPattern=CGenerator::MT_SAT_CC24_CCSG;		
         ccSize = GetConfig()->GetCColorsSize();
		 break;
	}
	
	CGenerator::MeasureType	SaturationType [ 7 ] =
							{
								CGenerator::MT_SAT_RED,
								CGenerator::MT_SAT_GREEN,
								CGenerator::MT_SAT_BLUE,
								CGenerator::MT_SAT_YELLOW,
								CGenerator::MT_SAT_CYAN,
								CGenerator::MT_SAT_MAGENTA,
								nPattern
							};

	CArray<CColor,int> measuredColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

    measuredColor.SetSize(size*6+(ccSize));
	measuredLux.SetSize(size*6+(ccSize));

	if(pGenerator->Init(size*(bPrimaryOnly?3:6) + ccSize) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		if (pGenerator->m_initShowedError) return FALSE;
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pGenerator->CanDisplayScale ( CGenerator::MT_SAT_ALL, ccSize ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
	CAsyncMeasurer asyncMeasure;
	asyncMeasure.Start(pSensor);

	CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);
	if(pGenerator->GetName() == str&&( (GetConfig()->m_CCMode==USER && ccSize > 100) ))
	{		
		Title.LoadString ( IDS_ERROR );
		if (pGenerator->m_initShowedError) return FALSE;
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		strMsg.Append(" not a supported DVD sequence.");
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}


	// Generate saturations for all colors
	GenerateSaturationColors (GetColorReference(), &GenColors[0], size, true, false, false, GetConfig()->m_GammaOffsetType, m_activeSatLevel, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());			// Red
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 1 ], size, false, true, false, GetConfig()->m_GammaOffsetType, m_activeSatLevel, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());	// Green
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 2 ], size, false, false, true, GetConfig()->m_GammaOffsetType, m_activeSatLevel, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());	// Blue
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 3 ], size, true, true, false, GetConfig()->m_GammaOffsetType, m_activeSatLevel, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());	// Yellow
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 4 ], size, false, true, true, GetConfig()->m_GammaOffsetType, m_activeSatLevel, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());	// Cyan
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 5 ], size, true, false, true, GetConfig()->m_GammaOffsetType, m_activeSatLevel, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());	// Magenta

	if (!GenerateCC24Colors (GetColorReference(), & GenColors [ size * 6 ], GetConfig()->m_CCMode, GetConfig()->m_GammaOffsetType, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235())) //color checker
	{		
		Title.LoadString ( IDS_ERROR );
		if (pGenerator->m_initShowedError) return FALSE;
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
	for (i=0;i<MAX_USER_CC_PATCH_SIZE;i++)
	m_cc24SatMeasureArray[i] = noDataColor;

	m_binMeasure = TRUE;
	for ( j = 0 ; j < ( bPrimaryOnly ? 3 : 7 ) ; j ++ )
	{
		m_currentIndex = 0;
		if (j>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;
		if (!j && GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE) == DISPLAY_GDI_Hide)
			UpdateTstWnd(pDoc, -1);

		for ( i = 0 ; i < ( j == 6 ? ccSize : size ) ; i ++ )
		{
			if (bPrimaryOnly)
			{
				if (j < 3)
					UpdateViews(pDoc, j+5);
				else
					UpdateViews(pDoc, 11);
			}
			else
			{
				if (j < 6)
					UpdateViews(pDoc, j+5);
				else
					UpdateViews(pDoc, 11);
			}

			if( pGenerator->DisplayRGBColor(GenColors[(j*size)+i],SaturationType[j],(j == 6 ? i:100*i/(size - 1)),!bRetry,(j>0)) )
			{
				UpdateTstWnd(pDoc, i);
				bEscape = WaitForDynamicIris (FALSE, pDoc);
				bRetry = FALSE;

				if ( ! bEscape )
				{
					if ( bUseLuxValues )
						StartLuxMeasure ();

					measuredColor[(j*size)+i] = PumpedRead(asyncMeasure, pSensor, GenColors[(j*size)+i], displaymode);
					if ((i+j*size)<size)
						m_redSatMeasureArray[i] = measuredColor[j*size+i];
					if ((i+j*size)<2*size&&(i+j*size)>=size)
						m_greenSatMeasureArray[i] = measuredColor[j*size+i];
					if ((i+j*size)<3*size&&(i+j*size)>=2*size)
						m_blueSatMeasureArray[i] = measuredColor[j*size+i];
					if (!bPrimaryOnly)
					{
						if ((i+j*size)<4*size&&(i+j*size)>=3*size)
							m_yellowSatMeasureArray[i] = measuredColor[j*size+i];
						if ((i+j*size)<5*size&&(i+j*size)>=4*size)
							m_cyanSatMeasureArray[i] = measuredColor[j*size+i];
						if ((i+j*size)<6*size&&(i+j*size)>=5*size)
							m_magentaSatMeasureArray[i] = measuredColor[j*size+i];
						if ((i+j*size)>=6*size)
							m_cc24SatMeasureArray[i] = measuredColor[j*size+i];
					} else
					{
						if ((i+j*size)>=3*size)
							m_cc24SatMeasureArray[i] = measuredColor[j*size+i];
					}
					if ( bUseLuxValues )
					{
						switch ( GetLuxMeasure ( & dLuxValue ) )
						{
							case LUX_NOMEASURE:
								 bUseLuxValues = FALSE;
								 break;

							case LUX_OK:
								 measuredLux[(j*size)+i] = dLuxValue;
								 break;

							case LUX_CANCELED:
								 bEscape = TRUE;
								 break;
						}
					}
				}

				while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
				{
					if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
						bEscape = TRUE;
				}
				if ( m_bAbortSweep ) bEscape = TRUE;

				if ( bEscape )
				{
					pSensor->Release();
					pGenerator->Release(size - i - 1);
					strMsg.LoadString ( IDS_MEASURESCANCELED );
					GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
					return FALSE;
				}

				if(!pSensor->IsMeasureValid())
				{
					Title.LoadString ( IDS_ERROR );
					strMsg.LoadString ( IDS_ANERROROCCURED );
					int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
					if(result == IDABORT)
					{
						pSensor->Release();
						pGenerator->Release();
						return FALSE;
					}
					if(result == IDRETRY)
					{
						i--;
						bRetry = TRUE;
					}
				}
				else
				{
					previousColor = lastColor;			
					lastColor = measuredColor[(j*size)+i];

					if(i != 0)
					{
						if (!pGenerator->HasPatternChanged(SaturationType[j],previousColor,lastColor))
						{
							i--;
							bPatternRetry = TRUE;
						}
					}
				}
			}
			else
			{
				pSensor->Release();
				pGenerator->Release();
				return FALSE;
			}
		}
		// Generator init to change pattern series if necessary
		if (j < ( bPrimaryOnly ? 3 : 6 ) - 1)
		{
			if ( !pGenerator->ChangePatternSeries())
			{
				pSensor->Release();
				pGenerator->Release();
				return FALSE;
			}
		}

	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	for ( i = 0 ; i < size ; i ++ )
	{
		m_redSatMeasureArray[i] = measuredColor[i];

		m_greenSatMeasureArray[i] = measuredColor[size+i];

		m_blueSatMeasureArray[i] = measuredColor[(2*size)+i];

		if ( bUseLuxValues )
		{
			m_redSatMeasureArray[i].SetLuxValue ( measuredLux[i] );
			m_greenSatMeasureArray[i].SetLuxValue ( measuredLux[size+i] );
			m_blueSatMeasureArray[i].SetLuxValue ( measuredLux[(2*size)+i] );
		}
		else
		{
			m_redSatMeasureArray[i].ResetLuxValue ();
			m_greenSatMeasureArray[i].ResetLuxValue ();
			m_blueSatMeasureArray[i].ResetLuxValue ();
		}

		if ( ! bPrimaryOnly )
		{
			m_yellowSatMeasureArray[i] = measuredColor[(3*size)+i];

			m_cyanSatMeasureArray[i] = measuredColor[(4*size)+i];

			m_magentaSatMeasureArray[i] = measuredColor[(5*size)+i];

			m_cc24SatMeasureArray[i] = measuredColor[(6*size)+i];

			if ( bUseLuxValues )
			{
				m_yellowSatMeasureArray[i].SetLuxValue ( measuredLux[(3*size)+i] );
				m_cyanSatMeasureArray[i].SetLuxValue ( measuredLux[(4*size)+i] );
				m_magentaSatMeasureArray[i].SetLuxValue ( measuredLux[(5*size)+i] );
				m_cc24SatMeasureArray[i].SetLuxValue ( measuredLux[(6*size)+i] );
			}
			else
			{
				m_yellowSatMeasureArray[i].ResetLuxValue ();
				m_cyanSatMeasureArray[i].ResetLuxValue ();
				m_magentaSatMeasureArray[i].ResetLuxValue ();
				m_cc24SatMeasureArray[i].ResetLuxValue ();
			}
		}
	}
	for ( i = 0 ; i < ccSize ; i ++ )
	{
		if ( ! bPrimaryOnly )
		{
				if (GetConfig()->m_CCMode != MCD)
				{
					m_cc24SatMeasureArray[i] = measuredColor[(6*size)+i];
				}
				else
				{
					if (i < 18)
					    m_cc24SatMeasureArray[i+6] = measuredColor[(6*size)+i];
					else
						m_cc24SatMeasureArray[23-i] = measuredColor[(6*size)+i];
				}

			if ( bUseLuxValues )
			{
				if (GetConfig()->m_CCMode != MCD)
				{
					m_cc24SatMeasureArray[i].SetLuxValue ( measuredLux[(6*size)+i] );
				}
				else
				{
					if (i < 18)
						m_cc24SatMeasureArray[i+6].SetLuxValue ( measuredLux[(6*size)+i] );
					else
						m_cc24SatMeasureArray[23-i].SetLuxValue ( measuredLux[(6*size)+i] );
				}
			}
			else
			{
				m_cc24SatMeasureArray[i].ResetLuxValue ();
			}
		}
	}

	GetConfig()->m_isSettling = doSettling;
	int iCC=GetConfig()->m_CCMode;
	if (iCC < RANDOM250)
		for (int i=0+100*iCC;i<100*(iCC+1);i++)
				m_cc24SatMeasureArray_master[i] = m_cc24SatMeasureArray[i-iCC*100];
	else if (iCC == RANDOM250)
		for (int i=PATTERN_SIZE;i<PATTERN_SIZE+250;i++)
				m_cc24SatMeasureArray_master[i] = m_cc24SatMeasureArray[i-PATTERN_SIZE];
	else if (iCC == RANDOM500)
		for (int i=PATTERN_SIZE+250;i<PATTERN_SIZE+250+500;i++)
				m_cc24SatMeasureArray_master[i] = m_cc24SatMeasureArray[i-(PATTERN_SIZE+250)];
	else if (iCC == USER)
		for (int i=PATTERN_SIZE+250+500;i<PATTERN_SIZE+250+500+MAX_USER_CC_PATCH_SIZE;i++)
				m_cc24SatMeasureArray_master[i] = m_cc24SatMeasureArray[i-(PATTERN_SIZE+250+500)];

	m_binMeasure = FALSE;
	UpdateViews(pDoc, 11);
	m_isModified=TRUE;
	m_CCStr=GetCCStr();
	return TRUE;
}

BOOL CMeasure::MeasurePrimarySecondarySaturationScales(CSensor *pSensor, CGenerator *pGenerator, BOOL bPrimaryOnly, CDataSetDoc *pDoc)
{
	SweepActiveGuard _sweepGuard(this);
	if (!_sweepGuard.Owned()) return FALSE;
	int			i, j;
	MSG			Msg;
	BOOL		bEscape;
	BOOL		bPatternRetry = FALSE;
	BOOL		bRetry = FALSE;
	int			size = GetSaturationSize ();
	CString		strMsg, Title;
	ColorRGBDisplay	GenColors [ 6 * 256 ];

	double		dLuxValue;
	
	CGenerator::MeasureType	SaturationType [ 6 ] =
							{
								CGenerator::MT_SAT_RED,
								CGenerator::MT_SAT_GREEN,
								CGenerator::MT_SAT_BLUE,
								CGenerator::MT_SAT_YELLOW,
								CGenerator::MT_SAT_CYAN,
								CGenerator::MT_SAT_MAGENTA
							};

	CArray<CColor,int> measuredColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

	measuredColor.SetSize(size*6);
	measuredLux.SetSize(size*6);

	if(pGenerator->Init(size*(bPrimaryOnly?3:6)) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		if (pGenerator->m_initShowedError) return FALSE;
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pGenerator->CanDisplayScale ( CGenerator::MT_SAT_ALL, size ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
	CAsyncMeasurer asyncMeasure;
	asyncMeasure.Start(pSensor);


	// Generate saturations for all colors
	GenerateSaturationColors (GetColorReference(), GenColors, size, true, false, false, GetConfig()->m_GammaOffsetType, m_activeSatLevel, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());				// Red
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 1 ], size, false, true, false, GetConfig()->m_GammaOffsetType, m_activeSatLevel, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());	// Green
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 2 ], size, false, false, true, GetConfig()->m_GammaOffsetType, m_activeSatLevel, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());	// Blue
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 3 ], size, true, true, false, GetConfig()->m_GammaOffsetType, m_activeSatLevel, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());	// Yellow
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 4 ], size, false, true, true, GetConfig()->m_GammaOffsetType, m_activeSatLevel, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());	// Cyan
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 5 ], size, true, false, true, GetConfig()->m_GammaOffsetType, m_activeSatLevel, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());	// Magenta

	m_binMeasure = TRUE;
	for ( j = 0 ; j < ( bPrimaryOnly ? 3 : 6 ) ; j ++ )
	{
		m_currentIndex = 0;
		for ( i = 0 ; i < size  ; i ++ )
		{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;

			UpdateViews(pDoc, j+5);
			if (!i && GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE) == DISPLAY_GDI_Hide)
				UpdateTstWnd(pDoc, -1);

			if( pGenerator->DisplayRGBColor(GenColors[(j*size)+i],SaturationType[j],100*i/(size - 1),!bRetry,(j>0)) )
			{
				UpdateTstWnd(pDoc, i);
				bEscape = WaitForDynamicIris (FALSE, pDoc);
				bRetry = FALSE;

				if ( ! bEscape )
				{
					if ( bUseLuxValues )
						StartLuxMeasure ();

					measuredColor[(j*size)+i] = PumpedRead(asyncMeasure, pSensor, GenColors[(j*size)+i], displaymode);
					if ((i+j*size)<size)
						m_redSatMeasureArray[i] = measuredColor[i];
					if ((i+j*size)<size*2&&(i+j*size)>=size)
						m_greenSatMeasureArray[i] = measuredColor[size+i];
					if ((i+j*size)<size*3&&(i+j*size)>=2*size)
						m_blueSatMeasureArray[i] = measuredColor[2*size+i];
					if (!bPrimaryOnly)
					{
						if ((i+j*size)<size*4&&(i+j*size)>=3*size)
							m_yellowSatMeasureArray[i] = measuredColor[3*size+i];
						if ((i+j*size)<size*5&&(i+j*size)>=4*size)
							m_cyanSatMeasureArray[i] = measuredColor[4*size+i];
						if ((i+j*size)<size*6&&(i+j*size)>=5*size)
							m_magentaSatMeasureArray[i] = measuredColor[5*size+i];
					}
					
					if ( bUseLuxValues )
					{
						switch ( GetLuxMeasure ( & dLuxValue ) )
						{
							case LUX_NOMEASURE:
								 bUseLuxValues = FALSE;
								 break;

							case LUX_OK:
								 measuredLux[(j*size)+i] = dLuxValue;
								 break;

							case LUX_CANCELED:
								 bEscape = TRUE;
								 break;
						}
					}
				}

				while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
				{
					if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
						bEscape = TRUE;
				}
				if ( m_bAbortSweep ) bEscape = TRUE;

				if ( bEscape )
				{
					pSensor->Release();
					pGenerator->Release(size - i - 1);
					strMsg.LoadString ( IDS_MEASURESCANCELED );
					GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
					return FALSE;
				}

				if(!pSensor->IsMeasureValid())
				{
					Title.LoadString ( IDS_ERROR );
					strMsg.LoadString ( IDS_ANERROROCCURED );
					int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
					if(result == IDABORT)
					{
						pSensor->Release();
						pGenerator->Release();
						return FALSE;
					}
					if(result == IDRETRY)
					{
						i--;
						bRetry = TRUE;
					}
				}
				else
				{
					previousColor = lastColor;			
					lastColor = measuredColor[(j*size)+i];
		
					if(i != 0)
					{
						if (!pGenerator->HasPatternChanged(SaturationType[j],previousColor,lastColor))
						{
							i--;
							bPatternRetry = TRUE;
						}
					}
				}
			}
			else
			{
				pSensor->Release();
				pGenerator->Release();
				return FALSE;
			}
		}
		// Generator init to change pattern series if necessary
		if (j < ( bPrimaryOnly ? 3 : 6 ) - 1)
		{
			if ( !pGenerator->ChangePatternSeries())
			{
				pSensor->Release();
				pGenerator->Release();
				return FALSE;
			}
		}

	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	for ( i = 0 ; i < size ; i ++ )
	{
		m_redSatMeasureArray[i] = measuredColor[i];

		m_greenSatMeasureArray[i] = measuredColor[size+i];

		m_blueSatMeasureArray[i] = measuredColor[(2*size)+i];

		if ( bUseLuxValues )
		{
			m_redSatMeasureArray[i].SetLuxValue ( measuredLux[i] );
			m_greenSatMeasureArray[i].SetLuxValue ( measuredLux[size+i] );
			m_blueSatMeasureArray[i].SetLuxValue ( measuredLux[(2*size)+i] );
		}
		else
		{
			m_redSatMeasureArray[i].ResetLuxValue ();
			m_greenSatMeasureArray[i].ResetLuxValue ();
			m_blueSatMeasureArray[i].ResetLuxValue ();
		}

		if ( ! bPrimaryOnly )
		{
			m_yellowSatMeasureArray[i] = measuredColor[(3*size)+i];

			m_cyanSatMeasureArray[i] = measuredColor[(4*size)+i];

			m_magentaSatMeasureArray[i] = measuredColor[(5*size)+i];

			if ( bUseLuxValues )
			{
				m_yellowSatMeasureArray[i].SetLuxValue ( measuredLux[(3*size)+i] );
				m_cyanSatMeasureArray[i].SetLuxValue ( measuredLux[(4*size)+i] );
				m_magentaSatMeasureArray[i].SetLuxValue ( measuredLux[(5*size)+i] );
			}
			else
			{
				m_yellowSatMeasureArray[i].ResetLuxValue ();
				m_cyanSatMeasureArray[i].ResetLuxValue ();
				m_magentaSatMeasureArray[i].ResetLuxValue ();
			}
		}
	}
	GetConfig()->m_isSettling = doSettling;

	m_binMeasure = FALSE;
	UpdateViews(pDoc, 10);
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasurePrimaries(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	SweepActiveGuard _sweepGuard(this);
	if (!_sweepGuard.Owned()) return FALSE;
	int		i;
	MSG		Msg;
	BOOL	bEscape;
	BOOL	bPatternRetry = FALSE;
	BOOL	bRetry = FALSE, isSpecial = FALSE;
	CColor	measuredColor[5];
	CString	strMsg, Title;
	double	dLuxValue;

	BOOL	bUseLuxValues = TRUE;
	double	measuredLux[5];

	if(pGenerator->Init(3 + GetConfig () -> m_BWColorsToAdd) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		if (pGenerator->m_initShowedError) return FALSE;
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
	CAsyncMeasurer asyncMeasure;
	asyncMeasure.Start(pSensor);

	CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);

	if(pGenerator->GetName() == str&&GetConfig()->m_colorStandard == HDTVb)
	{		
		Title.LoadString ( IDS_ERROR );
		if (pGenerator->m_initShowedError) return FALSE;
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		strMsg.Append(" not a supported DVD sequence.");
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}


	// Measure primary and secondary colors
	double		primaryIRELevel=100.0;
	int mode = GetConfig()->m_GammaOffsetType;

	if (mode == 5)
	{
		primaryIRELevel = 50.22831;
	//Special Case white for Mascior's HDR disk
		if(pGenerator->GetName() == str && (GetColorReference().m_standard == UHDTV4 || GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV2 || GetColorReference().m_standard == UHDTV || GetColorReference().m_standard == HDTV ))
			primaryIRELevel = 50.00;
	}

	ColorRGBDisplay	GenColors [ 5 ] = 
								{	
									ColorRGBDisplay(primaryIRELevel,0,0),
									ColorRGBDisplay(0,primaryIRELevel,0),
									ColorRGBDisplay(0,0,primaryIRELevel),
									ColorRGBDisplay(primaryIRELevel,primaryIRELevel,primaryIRELevel),
									ColorRGBDisplay(0,0,0)
								};

	if ( GetColorReference().m_standard == HDTVb )
	{
			GenColors [ 0 ] = ColorRGBDisplay(79.9087,10.0457,10.0457); 
			GenColors [ 1 ] = ColorRGBDisplay(30.137,79.9087,30.137); 
			GenColors [ 2 ] = ColorRGBDisplay(50.2283,50.2283,79.9087); 
			GenColors [ 3 ] = ColorRGBDisplay(primaryIRELevel,primaryIRELevel,primaryIRELevel);
			GenColors [ 4 ] = ColorRGBDisplay(0,0,0);
			isSpecial = TRUE;
	}
	else if ( GetColorReference().m_standard == HDTVa ) //75%
	{ 
		GenColors [ 0 ] = ColorRGBDisplay(68.04,20.09,20.09);
		GenColors [ 1 ] = ColorRGBDisplay(27.85,73.06,27.85);
		GenColors [ 2 ] = ColorRGBDisplay(19.18,19.18,50.22);
		GenColors [ 3 ] = ColorRGBDisplay(75.0,75.0,75.0);
		GenColors [ 4 ] = ColorRGBDisplay(0,0,0);
		isSpecial = TRUE;	
	}
	else if ( GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4 ) //P3/Rec709 in BT.2020
	{
		// GenColors[0..2] are computed from ContainerPrimaryLinear below
		// (all transfer functions).
		if (!(mode == 5 || mode == 7))
			isSpecial = TRUE;

		GenColors [ 3 ] = ColorRGBDisplay(primaryIRELevel,primaryIRELevel,primaryIRELevel);
		GenColors [ 4 ] = ColorRGBDisplay(0,0,0);
	}

	// Pseudo color spaces: build the primary patches from the inner primaries
	// mapped into the transport container, encoded with the ACTIVE transfer
	// function - the same chain GetRefSat models; see the identical block in
	// MeasureGrayScaleAndColors for the full rationale.
	if (GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4)
	{
		CColor tmW = CMeasure::GetGray ( CMeasure::GetGrayScaleSize() - 1 );
		CColor tmB = CMeasure::GetOnOffBlack();
		for (int ci = 0; ci < 3; ci++)
		{
			ColorRGB clin = ContainerPrimaryLinear(GetColorReference(), ci);
			for (int ck = 0; ck < 3; ck++)
			{
				double cv = clin[ck];
				if (mode == 5)
					cv = (cv <= 0.0) ? 0.0 : getL_EOTF(cv / 105.95640, tmW, tmB, GetConfig()->m_GammaRel, GetConfig()->m_Split, -5);
				else if (mode == 7)
					cv = (cv <= 0.0) ? 0.0 : getL_EOTF(cv, tmW, tmB, GetConfig()->m_GammaRel, GetConfig()->m_Split, -7);
				else
					cv = (cv <= 0.0 || cv >= 1.0) ? min(max(cv, 0.0), 1.0) : pow(cv, 1.0 / 2.22);
				GenColors[ci][ck] = min(max(cv, 0.0), 1.0) * 100.0;
			}
		}
	}

	if ( (mode == 5 || mode  == 7) && isSpecial)
	{
		for (i=0;i<=2;i++)
		{
			GenColors[i][0]= 100. * getL_EOTF(pow(GenColors[i][0] / 100.,2.22), noDataColor, noDataColor,0,0,-1*mode);
			GenColors[i][1]= 100. * getL_EOTF(pow(GenColors[i][1] / 100.,2.22), noDataColor, noDataColor,0,0,-1*mode);
			GenColors[i][2]= 100. * getL_EOTF(pow(GenColors[i][2] / 100.,2.22), noDataColor, noDataColor,0,0,-1*mode);
		}
	}

	m_binMeasure = TRUE;
	m_currentIndex = 0;
	for ( i = 0; i < ( 3 + GetConfig () -> m_BWColorsToAdd ) ; i ++ )
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;

//		if (i<3)
//			m_currentIndex = i;
//		else
//			m_currentIndex = i;

		UpdateViews(pDoc, 1);
		if (!i && GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE) == DISPLAY_GDI_Hide)
			UpdateTstWnd(pDoc, -1);

		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_PRIMARY, i) )
		{
			UpdateTstWnd(pDoc, i<3?i:i+3);
			bEscape = WaitForDynamicIris (FALSE, pDoc);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i] = PumpedRead(asyncMeasure, pSensor, GenColors[i], displaymode);
				if (i < 3)
					m_primariesArray[i] = measuredColor[i];
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}
			if ( m_bAbortSweep ) bEscape = TRUE;

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[i];
	
				if (!pGenerator->HasPatternChanged(CGenerator::MT_PRIMARY,previousColor,lastColor))
				{
					i--;
					bPatternRetry = TRUE;
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}

	for(int i=0;i<3;i++)
	{
		m_primariesArray[i] = measuredColor[i];
		if ( bUseLuxValues )
			m_primariesArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_primariesArray[i].ResetLuxValue ();
	}

	if ( GetConfig () -> m_BWColorsToAdd > 0 )
	{
		m_PrimeWhite = measuredColor[3];                
		if ( bUseLuxValues )
			m_PrimeWhite.SetLuxValue ( measuredLux[3] );
		else
			m_PrimeWhite.ResetLuxValue ();
	}
	else
	{
		m_PrimeWhite=noDataColor;
	}

	if ( GetConfig () -> m_BWColorsToAdd > 1 )
	{
		if (m_bOverRideBlack)
			m_OnOffBlack = m_userBlack;
		else
			m_OnOffBlack = measuredColor[4];                
		if ( bUseLuxValues )
			m_OnOffBlack.SetLuxValue ( measuredLux[4] );
		else
			m_OnOffBlack.ResetLuxValue ();
	}
	else
	{
//		m_OnOffBlack=noDataColor;
	}
	GetConfig()->m_isSettling = doSettling;

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	m_binMeasure = FALSE;
	UpdateViews(pDoc, 1);
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureSecondaries(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	SweepActiveGuard _sweepGuard(this);
	if (!_sweepGuard.Owned()) return FALSE;
	int		i;
	MSG		Msg;
	BOOL	bEscape;
	BOOL	bPatternRetry = FALSE;
	BOOL	bRetry = FALSE, isSpecial = FALSE;
	CColor	measuredColor[8];
	CString	strMsg, Title;
	double	dLuxValue;

	BOOL	bUseLuxValues = TRUE;
	double	measuredLux[8];

	if(pGenerator->Init(6+GetConfig()->m_BWColorsToAdd) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		if (pGenerator->m_initShowedError) return FALSE;
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
	CAsyncMeasurer asyncMeasure;
	asyncMeasure.Start(pSensor);
		CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);
	// Measure primary and secondary colors
	double		primaryIRELevel=100.0;	
	int mode = GetConfig()->m_GammaOffsetType;

	if (mode == 5)
	{
		primaryIRELevel = 50.22831;
	//Special Case white for Mascior's HDR disk
		if(pGenerator->GetName() == str && (GetColorReference().m_standard == UHDTV4 || GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV2 || GetColorReference().m_standard == UHDTV || GetColorReference().m_standard == HDTV))
			primaryIRELevel = 50.00;
	}

	ColorRGBDisplay	GenColors [ 8 ] = 
								{	
									ColorRGBDisplay(primaryIRELevel,0,0),
									ColorRGBDisplay(0,primaryIRELevel,0),
									ColorRGBDisplay(0,0,primaryIRELevel),
									ColorRGBDisplay(primaryIRELevel,primaryIRELevel,0),
									ColorRGBDisplay(0,primaryIRELevel,primaryIRELevel),
									ColorRGBDisplay(primaryIRELevel,0,primaryIRELevel),
									ColorRGBDisplay(primaryIRELevel,primaryIRELevel,primaryIRELevel),
									ColorRGBDisplay(0,0,0)
								};
	if (GetColorReference().m_standard == HDTVb)
	{
			GenColors [ 0 ] = ColorRGBDisplay(79.9087,10.0457,10.0457); 
			GenColors [ 1 ] = ColorRGBDisplay(30.137,79.9087,30.137); 
			GenColors [ 2 ] = ColorRGBDisplay(50.2283,50.2283,79.9087); 
			GenColors [ 3 ] = ColorRGBDisplay(79.9087,79.9087,10.0457);
			GenColors [ 4 ] = ColorRGBDisplay(10.0457,79.9087,79.9087);
			GenColors [ 5 ] = ColorRGBDisplay(79.9087,10.0457,79.9087);
			GenColors [ 6 ] = ColorRGBDisplay(primaryIRELevel,primaryIRELevel,primaryIRELevel);
			GenColors [ 7 ] = ColorRGBDisplay(0,0,0);
			isSpecial = TRUE;
	}
	else if (GetColorReference().m_standard == HDTVa) //75%
	{ 
		GenColors [ 0 ] = ColorRGBDisplay(68.04,20.09,20.09);
		GenColors [ 1 ] = ColorRGBDisplay(27.85,73.06,27.85);
		GenColors [ 2 ] = ColorRGBDisplay(19.18,19.18,50.22);
		GenColors [ 3 ] = ColorRGBDisplay(73.9726,73.9726,33.3333);
		GenColors [ 4 ] = ColorRGBDisplay(36.07,73.06,73.06);
		GenColors [ 5 ] = ColorRGBDisplay(64.3836,29.2237,64.3836);
		GenColors [ 6 ] = ColorRGBDisplay(75.0,75.0,75.0);
		GenColors [ 7 ] = ColorRGBDisplay(0,0,0);	
		isSpecial = TRUE;
	}
	else if ( GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4 ) //P3/Rec709 in BT.2020
	{
		// GenColors[0..5] are computed from ContainerPrimaryLinear below
		// (all transfer functions).
		if (!(mode == 5 || mode == 7))
			isSpecial = TRUE;

		GenColors [ 6 ] = ColorRGBDisplay(primaryIRELevel,primaryIRELevel,primaryIRELevel);
		GenColors [ 7 ] = ColorRGBDisplay(0,0,0);
	}

	// Pseudo color spaces: build the primary/secondary patches from the inner
	// primaries mapped into the transport container, encoded with the ACTIVE
	// transfer function - the same chain GetRefSat models; see the identical
	// block in MeasureGrayScaleAndColors for the full rationale.
	if (GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4)
	{
		CColor tmW = CMeasure::GetGray ( CMeasure::GetGrayScaleSize() - 1 );
		CColor tmB = CMeasure::GetOnOffBlack();
		for (int ci = 0; ci < 6; ci++)
		{
			ColorRGB clin = ContainerPrimaryLinear(GetColorReference(), ci);
			for (int ck = 0; ck < 3; ck++)
			{
				double cv = clin[ck];
				if (mode == 5)
					cv = (cv <= 0.0) ? 0.0 : getL_EOTF(cv / 105.95640, tmW, tmB, GetConfig()->m_GammaRel, GetConfig()->m_Split, -5);
				else if (mode == 7)
					cv = (cv <= 0.0) ? 0.0 : getL_EOTF(cv, tmW, tmB, GetConfig()->m_GammaRel, GetConfig()->m_Split, -7);
				else
					cv = (cv <= 0.0 || cv >= 1.0) ? min(max(cv, 0.0), 1.0) : pow(cv, 1.0 / 2.22);
				GenColors[ci][ck] = min(max(cv, 0.0), 1.0) * 100.0;
			}
		}
	}

	if ( (mode == 5 || mode  == 7) && isSpecial)
	{
		for (i=0;i<=5;i++)
		{
			GenColors[i][0]= 100. * getL_EOTF(pow(GenColors[i][0] / 100.,2.22), noDataColor, noDataColor,0,0,-1*mode);
			GenColors[i][1]= 100. * getL_EOTF(pow(GenColors[i][1] / 100.,2.22), noDataColor, noDataColor,0,0,-1*mode);
			GenColors[i][2]= 100. * getL_EOTF(pow(GenColors[i][2] / 100.,2.22), noDataColor, noDataColor,0,0,-1*mode);
		}
	}

	m_binMeasure = TRUE;
	m_currentIndex = 0;
	for ( i = 0; i < ( 6 + GetConfig () -> m_BWColorsToAdd ); i ++ )
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;

		UpdateViews(pDoc, 1);
		if (!i && GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE) == DISPLAY_GDI_Hide)
			UpdateTstWnd(pDoc, -1);

		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_SECONDARY, i) )
		{
			UpdateTstWnd(pDoc, i);
			bEscape = WaitForDynamicIris (FALSE, pDoc);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i] = PumpedRead(asyncMeasure, pSensor, GenColors[i], displaymode);
				
				if (i<3)
					m_primariesArray[i] = measuredColor[i];
				if (i>=3&&i<6)
					m_secondariesArray[i-3] = measuredColor[i];
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}
			if ( m_bAbortSweep ) bEscape = TRUE;

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[i];
	
				if (!pGenerator->HasPatternChanged(CGenerator::MT_SECONDARY,previousColor,lastColor))
				{
					i--;
					bPatternRetry = TRUE;
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}
	for(int i=0;i<3;i++)
	{
		m_primariesArray[i] = measuredColor[i];
		if ( bUseLuxValues )
			m_primariesArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_primariesArray[i].ResetLuxValue ();
	}

	for(int i=0;i<3;i++)
	{
		m_secondariesArray[i] = measuredColor[i+3];
		if ( bUseLuxValues )
			m_secondariesArray[i].SetLuxValue ( measuredLux[i+3] );
		else
			m_secondariesArray[i].ResetLuxValue ();
	}

	if ( GetConfig () -> m_BWColorsToAdd > 0 )
	{
		m_PrimeWhite = measuredColor[6];                
		if ( bUseLuxValues )
			m_PrimeWhite.SetLuxValue ( measuredLux[6] );
		else
			m_PrimeWhite.ResetLuxValue ();
	}
	else
	{
		m_PrimeWhite=noDataColor;
	}

	if ( GetConfig () -> m_BWColorsToAdd > 1 )
	{
		if (m_bOverRideBlack)
			m_OnOffBlack = m_userBlack;
		else
			m_OnOffBlack = measuredColor[7];
		if ( bUseLuxValues )
			m_OnOffBlack.SetLuxValue ( measuredLux[7] );
		else
			m_OnOffBlack.ResetLuxValue ();
	}
	else
	{
//		m_OnOffBlack=noDataColor;
	}
	GetConfig()->m_isSettling = doSettling;

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	m_binMeasure = FALSE;
	UpdateViews(pDoc, 1);
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureContrast(CSensor *pSensor, CGenerator *pGenerator)
{
	SweepActiveGuard _sweepGuard(this);
	if (!_sweepGuard.Owned()) return FALSE;
	int		i;
	MSG		Msg;
	BOOL	bEscape;
	BOOL	bPatternRetry = FALSE;
	BOOL	bRetry = FALSE;
	CString	strMsg, Title;
	double	dLuxValue;

	BOOL	bUseLuxValues = TRUE;
	double	measuredLux[4];

	
	if(pGenerator->Init(4, TRUE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		if (pGenerator->m_initShowedError) return FALSE;
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
	CAsyncMeasurer asyncMeasure;
	asyncMeasure.Start(pSensor);

	double BlackIRELevel=0.0;	
	double WhiteIRELevel=100.0;	
	double NearBlackIRELevel=10.0;	
	double NearWhiteIRELevel=90.0;	
	
	CColor measure;
	// Measure black for on/off contrast, uses GDI if detached window is selected
	m_binMeasure = TRUE;
	m_currentIndex = 0;

	doSettling = GetConfig()->m_isSettling;
	if (GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE) == DISPLAY_GDI_Hide)
	{
			( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.ShowWindow(SW_HIDE);
			( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> EnableWindow ( FALSE );
	}

	if (!CheckBlackOverride())
	{
		for ( i = 0; i < 1 ; i ++ )
		{
			if( pGenerator->DisplayGray(BlackIRELevel,CGenerator::MT_CONTRAST,!bRetry ) )
			{
				bEscape = WaitForDynamicIris (FALSE);
				bRetry = FALSE;

				if ( ! bEscape )
				{
					if ( bUseLuxValues )
						StartLuxMeasure ();

						measure = PumpedRead(asyncMeasure, pSensor, ColorRGBDisplay(BlackIRELevel));
				
					if ( bUseLuxValues )
					{
						switch ( GetLuxMeasure ( & dLuxValue ) )
						{
							case LUX_NOMEASURE:
								 bUseLuxValues = FALSE;
								 break;

							case LUX_OK:
								 measuredLux[0] = dLuxValue;
								 break;

							case LUX_CANCELED:
								 bEscape = TRUE;
								 break;
						}
					}
				}

				if ( bUseLuxValues && ! bEscape )
				{
					int		nNbLoops = 0;
					BOOL	bContinue;
				
					// Measuring black: ask and reask luxmeter value until it stabilizes
					do
					{
						bContinue = FALSE;
						nNbLoops ++;
					
						StartLuxMeasure ();
					
						switch ( GetLuxMeasure ( & dLuxValue ) )
						{
							case LUX_NOMEASURE:
								 bUseLuxValues = FALSE;
								 break;

							case LUX_OK:
								 if ( dLuxValue < measuredLux[0] )
								 {
									measuredLux[0] = dLuxValue;
									bContinue = TRUE;
								 }
								 break;

							case LUX_CANCELED:
								 bEscape = TRUE;
								 break;
						}
					} while ( bContinue && nNbLoops < 10 );
				}

				while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
				{
					if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
						bEscape = TRUE;
				}
				if ( m_bAbortSweep ) bEscape = TRUE;

				if ( bEscape )
				{
					pSensor->Release();
					pGenerator->Release();
					strMsg.LoadString ( IDS_MEASURESCANCELED );
					GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
					return FALSE;
				}

				if(!pSensor->IsMeasureValid())
				{
					Title.LoadString ( IDS_ERROR );
					strMsg.LoadString ( IDS_ANERROROCCURED );
					int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
					if(result == IDABORT)
					{
						pSensor->Release();
						pGenerator->Release();
						return FALSE;
					}
					if(result == IDRETRY)
					{
						i--;
						bRetry = TRUE;
					}
				}
				else
				{
						m_OnOffBlack = measure;
				}
			}
			else
			{
				pSensor->Release();
				pGenerator->Release();
				return FALSE;
			}
		}
		GetConfig()->m_isSettling = FALSE;
	} else
		m_OnOffBlack = m_userBlack;

	// Measure white for on/off contrast
	for ( i = 0; i < 1 ; i ++ )
	{
		if( pGenerator->DisplayGray(WhiteIRELevel,CGenerator::MT_CONTRAST,!bRetry ))
		{
			bEscape = WaitForDynamicIris ();
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measure = PumpedRead(asyncMeasure, pSensor, ColorRGBDisplay(WhiteIRELevel));
				
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[1] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}
			if ( m_bAbortSweep ) bEscape = TRUE;

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				lastColor = measure;
	
				if (!pGenerator->HasPatternChanged(CGenerator::MT_CONTRAST,m_OnOffBlack,lastColor))
				{
					i--;
					bPatternRetry = TRUE;
				}
				else
				{
					m_OnOffWhite = measure;

					if ( bUseLuxValues )
					{
						m_OnOffBlack.SetLuxValue ( measuredLux[0] );
						m_OnOffWhite.SetLuxValue ( measuredLux[1] );
					}
					else
					{
						m_OnOffBlack.ResetLuxValue ();
						m_OnOffWhite.ResetLuxValue ();
					}
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}
	GetConfig()->m_isSettling = FALSE;
		
	if(pGenerator->CanDisplayAnsiBWRects())
	{
		// Measure a color for ansi contrast (black or white, don't know)
		for ( i = 0; i < 1 ; i ++ )
		{
			if( pGenerator->DisplayAnsiBWRects(FALSE) )
			{
				bEscape = WaitForDynamicIris ();

				if ( ! bEscape )
				{
					if ( bUseLuxValues )
						StartLuxMeasure ();
						measure = PumpedRead(asyncMeasure, pSensor, ColorRGBDisplay(NearBlackIRELevel));	// Assume Black
					
					if ( bUseLuxValues )
					{
						switch ( GetLuxMeasure ( & dLuxValue ) )
						{
							case LUX_NOMEASURE:
								 bUseLuxValues = FALSE;
								 break;

							case LUX_OK:
								 measuredLux[2] = dLuxValue;
								 break;

							case LUX_CANCELED:
								 bEscape = TRUE;
								 break;
						}
					}
				}

				if ( bUseLuxValues && ! bEscape )
				{
					int		nNbLoops = 0;
					BOOL	bContinue;
					
					// Measuring black: ask and reask luxmeter value until it stabilizes
					do
					{
						bContinue = FALSE;
						nNbLoops ++;
						
						StartLuxMeasure ();
						
						switch ( GetLuxMeasure ( & dLuxValue ) )
						{
							case LUX_NOMEASURE:
								 bUseLuxValues = FALSE;
								 break;

							case LUX_OK:
								 if ( dLuxValue < measuredLux[2] )
								 {
									measuredLux[2] = dLuxValue;
									bContinue = TRUE;
								 }
								 break;

							case LUX_CANCELED:
								 bEscape = TRUE;
								 break;
						}
					} while ( bContinue && nNbLoops < 10 );
				}

				while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
				{
					if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
						bEscape = TRUE;
				}
				if ( m_bAbortSweep ) bEscape = TRUE;

				if ( bEscape )
				{
					pSensor->Release();
					pGenerator->Release();
					strMsg.LoadString ( IDS_MEASURESCANCELED );
					GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
					return FALSE;
				}

				if(!pSensor->IsMeasureValid())
				{
					Title.LoadString ( IDS_ERROR );
					strMsg.LoadString ( IDS_ANERROROCCURED );
					int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
						if(result == IDABORT)
						{
							pSensor->Release();
							pGenerator->Release();
							return FALSE;
						}
						if(result == IDRETRY)
							i--;
				}
				else
				{
					m_AnsiBlack = measure;
				}
			}
			else
			{
				pSensor->Release();
				pGenerator->Release();

				return FALSE;
			}
		}
		
		// Measure the other color for ansi contrast (black or white, don't know)
		for ( i = 0; i < 1 ; i ++ )
		{
			if( pGenerator->DisplayAnsiBWRects(TRUE) )
			{
				bEscape = WaitForDynamicIris ();

				if ( bUseLuxValues )
					StartLuxMeasure ();

				measure = PumpedRead(asyncMeasure, pSensor, ColorRGBDisplay(NearWhiteIRELevel));	// Assume White
				
				pGenerator->DisplayGray(0, CGenerator::MT_NEARBLACK, FALSE); //flush ccast

				bEscape = FALSE;
			
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[3] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}

				if ( bUseLuxValues && ! bEscape )
				{
					int		nNbLoops = 0;
					BOOL	bContinue;
					
					// Measuring black: ask and reask luxmeter value until it stabilizes
					do
					{
						bContinue = FALSE;
						nNbLoops ++;
						
						StartLuxMeasure ();
						
						switch ( GetLuxMeasure ( & dLuxValue ) )
						{
							case LUX_NOMEASURE:
								 bUseLuxValues = FALSE;
								 break;

							case LUX_OK:
								 if ( dLuxValue < measuredLux[3] )
								 {
									measuredLux[3] = dLuxValue;
									bContinue = TRUE;
								 }
								 break;

							case LUX_CANCELED:
								 bEscape = TRUE;
								 break;
						}
					} while ( bContinue && nNbLoops < 10 );
				}

				while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
				{
					if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
						bEscape = TRUE;
				}
				if ( m_bAbortSweep ) bEscape = TRUE;

				if ( bEscape )
				{
					pSensor->Release();
					pGenerator->Release();
					strMsg.LoadString ( IDS_MEASURESCANCELED );
					GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
					return FALSE;
				}

				if(!pSensor->IsMeasureValid())
				{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
					if(result == IDABORT)
					{
						pSensor->Release();
						pGenerator->Release();
						return FALSE;
					}
					if(result == IDRETRY)
						i--;
				}
				else
				{
					m_AnsiWhite = measure;

					if ( bUseLuxValues )
					{
						m_AnsiBlack.SetLuxValue ( measuredLux[2] );
						m_AnsiWhite.SetLuxValue ( measuredLux[3] );
					}
					else
					{
						m_AnsiBlack.ResetLuxValue ();
						m_AnsiWhite.ResetLuxValue ();
					}
				}
			}
			else
			{
				pSensor->Release();
				pGenerator->Release();
				return FALSE;
			}
		}

		if ( m_AnsiBlack.GetPreferedLuxValue (GetConfig () -> m_bPreferLuxmeter) > m_AnsiWhite.GetPreferedLuxValue (GetConfig () -> m_bPreferLuxmeter) )
		{
			// Exchange colors
			measure = m_AnsiBlack;
			m_AnsiBlack = m_AnsiWhite;
			m_AnsiWhite = measure;
		}
	}
	
	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	if (GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE) == DISPLAY_GDI_Hide)
	{
			( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.ShowWindow(SW_SHOW);
			( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> EnableWindow ( TRUE );
			( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.SetForegroundWindow();
	}

	GetConfig()->m_isSettling = doSettling;
	m_binMeasure = FALSE;
	m_isModified=TRUE;
	return TRUE;
}

double CMeasure::GetOnOffContrast ()
{
	double	black = m_OnOffBlack.GetPreferedLuxValue (GetConfig () -> m_bPreferLuxmeter);
	double	white = m_OnOffWhite.GetPreferedLuxValue (GetConfig () -> m_bPreferLuxmeter);
	
	if ( black > 0.0 && white > black )
		return ( white / black );
	else
		if (black == 0)
			return -1.0;
		else
			return -2.0;
}

double CMeasure::GetAnsiContrast ()
{
	double	black = m_AnsiBlack.GetPreferedLuxValue (GetConfig () -> m_bPreferLuxmeter);
	double	white = m_AnsiWhite.GetPreferedLuxValue (GetConfig () -> m_bPreferLuxmeter);
	
	if ( black > 0.000001 && white > black )
		return ( white / black );
	else
		if (black == 0)
			return -1.0;
		else
			return -2.0;
}

double CMeasure::GetContrastMinLum ()
{
	double	black = m_OnOffBlack.GetPreferedLuxValue (GetConfig () -> m_bPreferLuxmeter);
	if ( black > 0.000001 )
		return black;
	else
		return -1.0;
}

double CMeasure::GetContrastMaxLum ()
{
	double	white = m_OnOffWhite.GetPreferedLuxValue (GetConfig () -> m_bPreferLuxmeter);

	if ( white > 0.000001 )
		return white;
	else
		return -1.0;
}

void CMeasure::DeleteContrast ()
{
	m_AnsiBlack = noDataColor;
	m_AnsiWhite = noDataColor;

	m_isModified=TRUE; 
}

BOOL CMeasure::AddMeasurement(CSensor *pSensor, CGenerator *pGenerator,  CGenerator::MeasureType MT, int isPrimary, int last_minCol, int m_d)
{
	BOOL		bDisplayColor = GetConfig () -> m_bDisplayTestColors;
	BOOL		bOk;
	COLORREF	clr;
	CColor		measuredColor;
	CString	Msg, Title;

	if ( bDisplayColor )
	{
		if(pGenerator->Init() != TRUE)
		{
			Title.LoadString ( IDS_ERROR );
			Msg.LoadString ( IDS_ERRINITGENERATOR );
			GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONERROR | MB_OK);
			return FALSE;
		}
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		Msg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONERROR | MB_OK);
		if ( bDisplayColor )
			pGenerator->Release(-99);
		return FALSE;
	}

	if ( bDisplayColor ) 
	{
		// Display test color
		clr = ( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) ->m_wndTestColorWnd.m_colorPicker.GetColor ();
		clr &= 0x00FFFFFF;
		bOk = pGenerator->DisplayRGBColor(ColorRGBDisplay(clr), MT);
		if ( bOk )
			WaitForDynamicIris ( TRUE );
	}
	else
	{
        // don't know the color to measure so default value to mid gray...
		clr = RGB(128,128,128);
		bOk = TRUE;
	}

	if ( bOk )
	{
		measuredColor=pSensor->MeasureColor(ColorRGBDisplay(clr), m_d);
		if(!pSensor->IsMeasureValid())
		{
			Title.LoadString ( IDS_ERROR );
			Msg.LoadString ( IDS_ANERROROCCURED );
			int result=GetColorApp()->InMeasureMessageBox(Msg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
			if(result == IDABORT)
			{
				pSensor->Release();
				if ( bDisplayColor )
					pGenerator->Release(-99);
				return FALSE;
			}
		}
	}
	else
	{
		pSensor->Release();
		if ( bDisplayColor )
			pGenerator->Release(-99);
		return FALSE;
	}

	CColor measurement;
	measurement = measuredColor;
	m_measurementsArray.InsertAt(m_measurementsArray.GetSize(),measurement);
	m_isModified=TRUE;	
	FreeMeasurementAppended(isPrimary, last_minCol);

	pSensor->Release();
	if ( bDisplayColor )
		pGenerator->Release(-99);

	return TRUE;
}

// Recalibrates a single stored measurement. When the measurement retains its
// original raw (uncorrected) sensor reading, fullMatrix is applied to that raw
// value directly; otherwise (legacy data recorded before raw-value capture was
// added) fall back to composing deltaMatrix onto the already-corrected value,
// matching this function's previous behaviour. deltaMatrix and fullMatrix must
// be equivalent (deltaMatrix = fullMatrix * inverse(previous full matrix)) -
// passing the same matrix for both is fine when there is no meaningful "delta"
// (e.g. a freshly loaded calibration file replacing everything wholesale).
static void ReapplyAdjustmentMatrix(CColor& color, const Matrix& deltaMatrix, const Matrix& fullMatrix)
{
	if ( color.HasRawXYZValue() )
		color.SetXYZValue(ColorXYZ(fullMatrix * color.GetRawXYZValue()));
	else
		color.applyAdjustmentMatrix(deltaMatrix);
}

// Recalibrates a single stored measurement under Bodner's per-sub-gamut method.
// Requires the raw sensor reading (there is no meaningful delta-compose fallback
// for a per-sub-gamut method); returns false only when the slot actually holds a
// measurement (isValid()) but has no raw value (legacy data). Slots that were
// simply never measured (still noDataColor) are left alone and don't count as a
// recalibration failure.
static bool ReapplyBodnerMatrix(CColor& color, const Matrix rawMatrix[3], const Matrix calMatrix[3])
{
	if ( !color.isValid() )
		return true;

	if ( !color.HasRawXYZValue() )
		return false;

	color.SetXYZValue(SelectAndApplyBodnerMatrix(color.GetRawXYZValue(), rawMatrix, calMatrix));
	return true;
}

void CMeasure::ApplySensorAdjustmentMatrix(const Matrix& deltaMatrix, const Matrix& fullMatrix)
{
	for(int i=0;i<m_grayMeasureArray.GetSize();i++)  // Preserve sensor values
	{
				if (!i && m_bOverRideBlack)
					m_grayMeasureArray[i] = m_userBlack;
				else
					ReapplyAdjustmentMatrix(m_grayMeasureArray[i], deltaMatrix, fullMatrix);
	}

	for(int i=0;i<m_nearBlackMeasureArray.GetSize();i++)  // Preserve sensor values
	{
		ReapplyAdjustmentMatrix(m_nearBlackMeasureArray[i], deltaMatrix, fullMatrix);
	}
	for(int i=0;i<m_nearWhiteMeasureArray.GetSize();i++)  // Preserve sensor values
	{
		ReapplyAdjustmentMatrix(m_nearWhiteMeasureArray[i], deltaMatrix, fullMatrix);
	}
	for(int i=0;i<m_redSatMeasureArray.GetSize();i++)  // Preserve sensor values
	{
		ReapplyAdjustmentMatrix(m_redSatMeasureArray[i], deltaMatrix, fullMatrix);
	}
	for(int i=0;i<m_greenSatMeasureArray.GetSize();i++)  // Preserve sensor values
	{
		ReapplyAdjustmentMatrix(m_greenSatMeasureArray[i], deltaMatrix, fullMatrix);
	}
	for(int i=0;i<m_blueSatMeasureArray.GetSize();i++)  // Preserve sensor values
	{
		ReapplyAdjustmentMatrix(m_blueSatMeasureArray[i], deltaMatrix, fullMatrix);
	}
	for(int i=0;i<m_yellowSatMeasureArray.GetSize();i++)  // Preserve sensor values
	{
		ReapplyAdjustmentMatrix(m_yellowSatMeasureArray[i], deltaMatrix, fullMatrix);
	}
	for(int i=0;i<m_cyanSatMeasureArray.GetSize();i++)  // Preserve sensor values
	{
		ReapplyAdjustmentMatrix(m_cyanSatMeasureArray[i], deltaMatrix, fullMatrix);
	}
	for(int i=0;i<m_magentaSatMeasureArray.GetSize();i++)  // Preserve sensor values
	{
		ReapplyAdjustmentMatrix(m_magentaSatMeasureArray[i], deltaMatrix, fullMatrix);
	}
	for(int i=0;i<m_cc24SatMeasureArray.GetSize();i++)  // Preserve sensor values
	{
		ReapplyAdjustmentMatrix(m_cc24SatMeasureArray[i], deltaMatrix, fullMatrix);
	}
	for(int i=0;i<m_cc24SatMeasureArray_master.GetSize();i++)  // Preserve sensor values
	{
		ReapplyAdjustmentMatrix(m_cc24SatMeasureArray_master[i], deltaMatrix, fullMatrix);
	}
	for(int i=0;i<3;i++)
	{
		ReapplyAdjustmentMatrix(m_primariesArray[i], deltaMatrix, fullMatrix);
	}
	for(int i=0;i<3;i++)
	{
		ReapplyAdjustmentMatrix(m_secondariesArray[i], deltaMatrix, fullMatrix);
	}

	if (!m_bOverRideBlack)
		ReapplyAdjustmentMatrix(m_OnOffBlack, deltaMatrix, fullMatrix);

	ReapplyAdjustmentMatrix(m_OnOffWhite, deltaMatrix, fullMatrix);

	ReapplyAdjustmentMatrix(m_PrimeWhite, deltaMatrix, fullMatrix);

	ReapplyAdjustmentMatrix(m_AnsiBlack, deltaMatrix, fullMatrix);

	ReapplyAdjustmentMatrix(m_AnsiWhite, deltaMatrix, fullMatrix);
}

int CMeasure::ApplySensorBodnerRecalibration(const Matrix rawMatrix[3], const Matrix calMatrix[3])
{
	int nSkipped = 0;
	#define BODNER_REAPPLY(color) if (!ReapplyBodnerMatrix((color), rawMatrix, calMatrix)) nSkipped++;

	for(int i=0;i<m_grayMeasureArray.GetSize();i++)  // Preserve sensor values
	{
				if (!i && m_bOverRideBlack)
					m_grayMeasureArray[i] = m_userBlack;
				else
					BODNER_REAPPLY(m_grayMeasureArray[i]);
	}

	for(int i=0;i<m_nearBlackMeasureArray.GetSize();i++)
		BODNER_REAPPLY(m_nearBlackMeasureArray[i]);
	for(int i=0;i<m_nearWhiteMeasureArray.GetSize();i++)
		BODNER_REAPPLY(m_nearWhiteMeasureArray[i]);
	for(int i=0;i<m_redSatMeasureArray.GetSize();i++)
		BODNER_REAPPLY(m_redSatMeasureArray[i]);
	for(int i=0;i<m_greenSatMeasureArray.GetSize();i++)
		BODNER_REAPPLY(m_greenSatMeasureArray[i]);
	for(int i=0;i<m_blueSatMeasureArray.GetSize();i++)
		BODNER_REAPPLY(m_blueSatMeasureArray[i]);
	for(int i=0;i<m_yellowSatMeasureArray.GetSize();i++)
		BODNER_REAPPLY(m_yellowSatMeasureArray[i]);
	for(int i=0;i<m_cyanSatMeasureArray.GetSize();i++)
		BODNER_REAPPLY(m_cyanSatMeasureArray[i]);
	for(int i=0;i<m_magentaSatMeasureArray.GetSize();i++)
		BODNER_REAPPLY(m_magentaSatMeasureArray[i]);
	for(int i=0;i<m_cc24SatMeasureArray.GetSize();i++)
		BODNER_REAPPLY(m_cc24SatMeasureArray[i]);
	for(int i=0;i<m_cc24SatMeasureArray_master.GetSize();i++)
		BODNER_REAPPLY(m_cc24SatMeasureArray_master[i]);
	for(int i=0;i<3;i++)
		BODNER_REAPPLY(m_primariesArray[i]);
	for(int i=0;i<3;i++)
		BODNER_REAPPLY(m_secondariesArray[i]);

	if (!m_bOverRideBlack)
		BODNER_REAPPLY(m_OnOffBlack);

	BODNER_REAPPLY(m_OnOffWhite);
	BODNER_REAPPLY(m_PrimeWhite);
	BODNER_REAPPLY(m_AnsiBlack);
	BODNER_REAPPLY(m_AnsiWhite);

	#undef BODNER_REAPPLY
	return nSkipped;
}

BOOL CMeasure::WaitForDynamicIris ( BOOL bIgnoreEscape, CDataSetDoc *pDoc )
{
	BOOL bEscape = FALSE;
	int nLatencyTime = GetConfig()->m_latencyTime;
	UINT nLoopTime = 10;
	m_NMeasurements++;

	if (nLatencyTime < 0) 
		nLoopTime = -1 * nLatencyTime;
	else
	{
		nLoopTime = nLatencyTime;
		if (pDoc)
			if (pDoc->GetSensor()->GetName() == "Simulated sensor")
				nLoopTime = 10;
	}

	if ( nLoopTime > 0 )
	{
		// Sleep nLatencyTime ms while dispatching messages
		MSG	Msg;
		DWORD dwStart = GetTickCount();
		DWORD dwNow = dwStart;

		while((!bEscape) && ((dwNow - dwStart) < nLoopTime + 100))
		{
			Sleep(0);
			while(PeekMessage(&Msg, NULL, NULL, NULL, TRUE ))
			{
				if (!bIgnoreEscape )
				{
					if ( Msg.message == WM_KEYDOWN )
					{
						if ( Msg.wParam == VK_ESCAPE )
							bEscape = TRUE;
					}
				}

				TranslateMessage ( & Msg );
				DispatchMessage ( & Msg );
			}
			dwNow = GetTickCount();
		}
	}

	if ( GetConfig () -> m_bLatencyBeep && ! bEscape )
		MessageBeep (-1);

	return bEscape;
}
BOOL CMeasure::CheckBlackOverride ( )
{
	m_bOverRideBlack = GetConfig()->GetProfileDouble("References","Use Black Level",0);
	double YBlack = GetConfig()->GetProfileDouble("References","Manual Black Level",0);
	m_userBlack = CColor(ColorXYZ(YBlack*.95047,YBlack,YBlack*1.0883));
	return m_bOverRideBlack;
}

void CMeasure::UpdateViews ( CDataSetDoc *pDoc, int Sequence )
{
	if (pDoc )
	{
		POSITION pos = pDoc -> GetFirstViewPosition ();
		CView *pView = pDoc->GetNextView(pos);
		if (!m_currentIndex)
			((CMainView*)pView)->SetSelectedColor(noDataColor, TRUE);
		else
			((CMainView*)pView)->SetSelectedColor(lastColor, TRUE);

		if (GetConfig()->bDisplayRT)
		{
			pDoc ->SetModifiedFlag(TRUE);
			pDoc ->UpdateAllViews(NULL, UPD_REALTIME + Sequence);
		}
		else if ( ((CMainView*)pView)->m_displayMode != Sequence )
		{
			pDoc ->UpdateAllViews(NULL, UPD_REALTIME + Sequence);
		}
	}

}

void CMeasure::UpdateTstWnd (CDataSetDoc *pDoc, int i )
{
	if (i == -1)
	{
		GetConfig()->WriteProfileInt("GDIGenerator","DisplayMode", DISPLAY_GDI_Hide);
		( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.ShowWindow(SW_SHOW);
		( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> EnableWindow ( TRUE );
		( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.SetForegroundWindow();
		if (GetConfig()->m_isSettling && GetConfig()->bDisplayRT)
		{
			for (int j=0;j<255;j+=3)
			{
				( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) ->m_wndTestColorWnd.m_colorPicker.SetColor ( RGB(j,j,j) );
				( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) ->m_wndTestColorWnd.RedrawWindow();
				Sleep(55);
			}
		}
	}
	else
	{
		POSITION pos = pDoc->GetFirstViewPosition ();
		CView *pView = pDoc->GetNextView(pos);
		((CMainView*)pView)->m_Target.Refresh(pDoc->GetGenerator()->m_b16_235,  i+1, ((CMainView*)pView)->target_Size, ((CMainView*)pView)->m_displayMode, pDoc, CTargetWnd::TARGET_TESTWINDOW);
		((CMainView*)pView)->minCol = i+1;
		((CMainView*)pView)->last_minCol = i;
		m_currentIndex = i+1;
		((CMainView*)pView)->HighlightMeasuringColumn(i+1);
		displaymode = ((CMainView*)pView)->m_displayMode;
	}
}

static DWORD WINAPI BkGndMeasureThreadFunc ( LPVOID lpParameter )
{
    CrashDump useInThisThread;
    try
    {
	    CMeasure *		pMeasure = (CMeasure *) lpParameter;
	    CSensor *		pSensor = pMeasure -> m_pBkMeasureSensor;	// Assume sensor is initialized
    	
	    do
	    {
		    // Wait for execution request
		    WaitForSingleObject ( pMeasure -> m_hEventRun, INFINITE );
		    ResetEvent ( pMeasure -> m_hEventRun );

		    if ( pMeasure -> m_bTerminateThread )
		    {
			    // Exit thread
			    break;
		    }

		    // Perform one measure
		    ( * pMeasure -> m_pBkMeasuredColor ) [ pMeasure -> m_nBkMeasureStep ] = pSensor -> MeasureColor ( pMeasure -> m_clrToMeasure );

		    if ( ! pSensor -> IsMeasureValid () )
		    {
			    // Register error
			    pMeasure -> m_bErrorOccurred = TRUE;
		    }

		    // Indicate measure is done
		    SetEvent ( pMeasure -> m_hEventDone );

	    } while ( TRUE );

	    // Release sensor
	    pSensor -> Release ();
    }
    catch(std::exception& e)
    {
        std::cerr << "Exception in measurement thread : " << e.what() << std::endl;
    }
    catch(...)
    {
        std::cerr << "Unexpected Exception in measurement thread" << std::endl;
    }
	return 0;
}

HANDLE CMeasure::InitBackgroundMeasures ( CSensor *pSensor, int nSteps )
{
	DWORD	dw;

	m_bTerminateThread = FALSE;
	m_bErrorOccurred = FALSE;
	m_nBkMeasureStep = 0;
	m_clrToMeasure = ColorRGBDisplay(0.0);
	m_hEventRun = NULL;
	m_hEventDone = NULL;
	
	// Initialise sensor
	if ( pSensor -> Init (TRUE) )
	{
		m_nBkMeasureStepCount = nSteps;
		m_pBkMeasureSensor = pSensor;

		m_pBkMeasuredColor = new CArray<CColor,int>;
		m_pBkMeasuredColor -> SetSize ( m_nBkMeasureStepCount );

		// Create manual event in non signaled state
		m_hEventRun = CreateEvent ( NULL, TRUE, FALSE, NULL );

		// Create manual event in non signaled state
		m_hEventDone = CreateEvent ( NULL, TRUE, FALSE, NULL );

		if ( m_hEventRun && m_hEventDone )
		{
            ResetEvent ( m_hEventDone );
            ResetEvent ( m_hEventRun );
            m_hThread = CreateThread ( NULL, 0, BkGndMeasureThreadFunc, this, 0, & dw );

			if ( ! m_hThread )
			{
				CloseHandle ( m_hEventDone );
				CloseHandle ( m_hEventRun );
				m_hEventDone = NULL;
				m_hEventRun = NULL;
			}
		}

		if ( ! m_hThread )
		{
			delete m_pBkMeasuredColor;
			m_pBkMeasuredColor = NULL;
			m_pBkMeasureSensor = NULL;

			pSensor -> Release ();
		}
	}

	// Return NULL when initialization failed, or the event to wait for after measure request
	return m_hEventDone;
}

BOOL CMeasure::BackgroundMeasureColor ( int nCurStep, const ColorRGBDisplay& aRGBValue )
{
	if ( m_hThread )
	{
		// Reset event indicating that a measure is ready
		ResetEvent ( m_hEventDone );
		ResetEvent ( m_hEventRun );
		
		// Request a new background measure
		m_nBkMeasureStep = nCurStep;
		m_clrToMeasure = aRGBValue;
		
		SetEvent ( m_hEventRun );
		return TRUE;
	}

	return FALSE;
}

void CMeasure::CancelBackgroundMeasures ()
{
	if ( m_hThread )
	{
		// Terminate thread
		m_bTerminateThread = TRUE;
		SetEvent ( m_hEventRun );

		// Close thread
		WaitForSingleObject ( m_hThread, INFINITE );
		CloseHandle ( m_hThread );
		m_hThread = NULL;

		// Close events
		CloseHandle ( m_hEventDone );
		CloseHandle ( m_hEventRun );
		m_hEventDone = NULL;
		m_hEventRun = NULL;

		// Delete temporary list of measures
		delete m_pBkMeasuredColor;
		m_pBkMeasuredColor = NULL;

		m_pBkMeasureSensor = NULL;
	}
}

BOOL CMeasure::ValidateBackgroundGrayScale ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		SetGrayScaleSize(m_nBkMeasureStepCount);
		for ( int i = 0; i < m_nBkMeasureStepCount ; i++ )
		{
			if (!i && m_bOverRideBlack)
				m_grayMeasureArray[i] = m_userBlack;
			else
				m_grayMeasureArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_grayMeasureArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_grayMeasureArray[i].ResetLuxValue ();
		}
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

BOOL CMeasure::ValidateBackgroundSingleMeasurement ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;
				
		CColor measurement;

		measurement = (*m_pBkMeasuredColor)[0];

		if ( bUseLuxValues )
			measurement.SetLuxValue ( pLuxValues[0] );
		else
			measurement.ResetLuxValue ();

		m_measurementsArray.InsertAt(m_measurementsArray.GetSize(),measurement);
		
		FreeMeasurementAppended (0, 0);
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();
	Sleep(abs(GetConfig()->m_latencyTime));
	return bOk;
}

void CMeasure::FreeMeasurementAppended(int isPrimary, int last_minCol)
{
	//update grid
	int	n = GetMeasurementsSize();
	CColor LastMeasure;
	if ( GetConfig () -> m_bDetectPrimaries  && last_minCol < 1 )
	{
		if ( n > 0 )
		{
			LastMeasure=GetMeasurement(n-1);

			if ( LastMeasure.GetDeltaxy ( GetRefPrimary(0), GetColorReference()) < 0.03 )
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
				SetRedPrimary ( m_measurementsArray[n-1] );
			}
			else if ( LastMeasure.GetDeltaxy ( GetRefPrimary(1), GetColorReference() ) < 0.03 )
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
				SetGreenPrimary ( m_measurementsArray[n-1] );
			}
			else if ( LastMeasure.GetDeltaxy ( GetRefPrimary(2), GetColorReference() ) < 0.03 )
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
				SetBluePrimary ( m_measurementsArray[n-1] );
			}
			else if ( LastMeasure.GetDeltaxy (GetRefSecondary(0), GetColorReference() ) < 0.03)
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
				SetYellowSecondary ( m_measurementsArray[n-1] );
			}
			else if ( LastMeasure.GetDeltaxy ( GetRefSecondary(1), GetColorReference() ) < 0.03)
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
				SetCyanSecondary ( m_measurementsArray[n-1] );
			}
			else if ( LastMeasure.GetDeltaxy ( GetRefSecondary(2), GetColorReference() ) < 0.03)
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
				SetMagentaSecondary ( m_measurementsArray[n-1] );
			}
			else if ( LastMeasure.GetDeltaxy ( GetColorReference().GetWhite(), GetColorReference() ) < 0.03 )
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
//				SetPrimeWhite ( m_measurementsArray[n-1] ); //do not futz with white reference during measures
			}
		}
	} 
	else if (n > 0) 
	{
		LastMeasure = GetMeasurement(n-1);
		switch (isPrimary)
		{
		case 1:
			switch (last_minCol)
			{
				case 1:
				SetRedPrimary ( m_measurementsArray[n-1] );
				break;
				case 2:
				SetGreenPrimary ( m_measurementsArray[n-1] );
				break;
				case 3:
				SetBluePrimary ( m_measurementsArray[n-1] );
				break;
				case 4:
				SetYellowSecondary ( m_measurementsArray[n-1] );
				break;
				case 5:
				SetCyanSecondary ( m_measurementsArray[n-1] );
				break;
				case 6:
				SetMagentaSecondary ( m_measurementsArray[n-1] );
				break;
				case 7:
				SetPrimeWhite ( m_measurementsArray[n-1] );
				break;
				case 8:
				m_OnOffBlack = m_measurementsArray[n-1];
				break;
			}
			break;
		case 3:
			SetNearBlack(last_minCol - 1, m_measurementsArray[n-1]);
			break;
		case 4:
			SetNearWhite(last_minCol - 1, m_measurementsArray[n-1]);
			break;
		case 5:
			SetRedSat(last_minCol - 1, m_measurementsArray[n-1]);
			break;
		case 6:
			SetGreenSat(last_minCol - 1, m_measurementsArray[n-1]);
			break;
		case 7:
			SetBlueSat(last_minCol - 1, m_measurementsArray[n-1]);
			break;
		case 8:
			SetYellowSat(last_minCol - 1, m_measurementsArray[n-1]);
			break;
		case 9:
			SetCyanSat(last_minCol - 1, m_measurementsArray[n-1]);
			break;
		case 10:
			SetMagentaSat(last_minCol - 1, m_measurementsArray[n-1]);
			break;
		case 11: //color checker
			SetCC24Sat(last_minCol - 1, m_measurementsArray[n-1]);
			int iCC=GetConfig()->m_CCMode, i;
			if (iCC < RANDOM250)
			{
				i = 0 + 100*iCC + last_minCol - 1;
						m_cc24SatMeasureArray_master[i] = m_measurementsArray[n-1];
			}
			else if (iCC == RANDOM250)
			{
				i = PATTERN_SIZE + last_minCol -1;
						m_cc24SatMeasureArray_master[i] = m_measurementsArray[n-1];
			}
			else if (iCC == RANDOM500)
			{
				i = PATTERN_SIZE + 250 + last_minCol -1;
						m_cc24SatMeasureArray_master[i] = m_measurementsArray[n-1];
			}
			else if (iCC == USER) 
			{
				i = PATTERN_SIZE + 250 + 500 + last_minCol - 1;
						m_cc24SatMeasureArray_master[i] = m_measurementsArray[n-1];
			}
			break;
		}
		m_isModified=TRUE;
	}
	Sleep(abs(GetConfig()->m_latencyTime));
}

BOOL CMeasure::ValidateBackgroundNearBlack ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		SetNearBlackScaleSize(m_nBkMeasureStepCount);
		for ( int i = 0; i < m_nBkMeasureStepCount ; i++ )
		{
			if (!i && m_bOverRideBlack)
				m_nearBlackMeasureArray[i] = m_userBlack;
			else
				m_nearBlackMeasureArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_nearBlackMeasureArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_nearBlackMeasureArray[i].ResetLuxValue ();
		}
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

BOOL CMeasure::ValidateBackgroundNearWhite ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		SetNearWhiteScaleSize(m_nBkMeasureStepCount);
		for ( int i = 0; i < m_nBkMeasureStepCount ; i++ )
		{
			m_nearWhiteMeasureArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_nearWhiteMeasureArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_nearWhiteMeasureArray[i].ResetLuxValue ();
		}
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

BOOL CMeasure::ValidateBackgroundPrimaries ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		ASSERT ( m_nBkMeasureStepCount == 3 || m_nBkMeasureStepCount == 4 || m_nBkMeasureStepCount == 5 );
		for ( int i = 0; i < 3 ; i++ )
		{
			m_primariesArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_primariesArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_primariesArray[i].ResetLuxValue ();
		}
		if ( m_nBkMeasureStepCount >= 4 )
		{
			// Store reference white for primaries
			m_PrimeWhite = (*m_pBkMeasuredColor)[3];

			if ( bUseLuxValues )
				m_PrimeWhite.SetLuxValue ( pLuxValues[3] );
			else
				m_PrimeWhite.ResetLuxValue ();
		}
		else
			m_PrimeWhite = noDataColor;

		if ( m_nBkMeasureStepCount >= 5 )
		{
			if (m_bOverRideBlack)
				m_OnOffBlack = m_userBlack;
			else
				m_OnOffBlack = (*m_pBkMeasuredColor)[4];

			if ( bUseLuxValues )
				m_OnOffBlack.SetLuxValue ( pLuxValues[4] );
			else
				m_OnOffBlack.ResetLuxValue ();
		}
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

BOOL CMeasure::ValidateBackgroundSecondaries ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		ASSERT ( m_nBkMeasureStepCount == 6 || m_nBkMeasureStepCount == 7 || m_nBkMeasureStepCount == 8 );
		for ( int i = 0; i < 3 ; i++ )
		{
			m_primariesArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_primariesArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_primariesArray[i].ResetLuxValue ();
		}
		for (int i = 0; i < 3 ; i++ )
		{
			m_secondariesArray[i] = (*m_pBkMeasuredColor)[i+3];

			if ( bUseLuxValues )
				m_secondariesArray[i].SetLuxValue ( pLuxValues[i+3] );
			else
				m_secondariesArray[i].ResetLuxValue ();
		}

		if ( m_nBkMeasureStepCount >= 7 )
		{
			// Store reference white for primaries
			m_PrimeWhite = (*m_pBkMeasuredColor)[6];

			if ( bUseLuxValues )
				m_PrimeWhite.SetLuxValue ( pLuxValues[6] );
			else
				m_PrimeWhite.ResetLuxValue ();
		}
		else
			m_PrimeWhite = noDataColor;

		if ( m_nBkMeasureStepCount >= 8 )
		{
			if (m_bOverRideBlack)
				m_OnOffBlack = m_userBlack;				
			else
				m_OnOffBlack = (*m_pBkMeasuredColor)[7];

			if ( bUseLuxValues )
				m_OnOffBlack.SetLuxValue ( pLuxValues[7] );
			else
				m_OnOffBlack.ResetLuxValue ();
		}
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

BOOL CMeasure::ValidateBackgroundGrayScaleAndColors ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		SetGrayScaleSize(m_nBkMeasureStepCount-6);
		for ( int i = 0; i < m_nBkMeasureStepCount-6 ; i++ )
		{
			if (!i && m_bOverRideBlack)
				m_grayMeasureArray[i] = m_userBlack;
			else
				m_grayMeasureArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_grayMeasureArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_grayMeasureArray[i].ResetLuxValue ();
		}
		for (int i = 0; i < 3 ; i++ )
		{
			m_primariesArray[i] = (*m_pBkMeasuredColor)[m_nBkMeasureStepCount-6+i];

			if ( bUseLuxValues )
				m_primariesArray[i].SetLuxValue ( pLuxValues[m_nBkMeasureStepCount-6+i] );
			else
				m_primariesArray[i].ResetLuxValue ();
		}
		for (int i = 0; i < 3 ; i++ )
		{
			m_secondariesArray[i] = (*m_pBkMeasuredColor)[m_nBkMeasureStepCount-3+i];

			if ( bUseLuxValues )
				m_secondariesArray[i].SetLuxValue ( pLuxValues[m_nBkMeasureStepCount-3+i] );
			else
				m_secondariesArray[i].ResetLuxValue ();
		}

		m_PrimeWhite = (*m_pBkMeasuredColor)[m_nBkMeasureStepCount-7];
		if (m_bOverRideBlack)
			m_OnOffBlack = m_userBlack;
		else
			m_OnOffBlack = (*m_pBkMeasuredColor)[0];

		if ( bUseLuxValues )
		{
			m_PrimeWhite.SetLuxValue ( pLuxValues[m_nBkMeasureStepCount-7] );
			m_OnOffBlack.SetLuxValue ( pLuxValues[0] );
		}
		else
		{
			m_PrimeWhite.ResetLuxValue ();
//			m_OnOffBlack.ResetLuxValue ();
		}
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

BOOL CMeasure::ValidateBackgroundRedSatScale ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		SetSaturationSize(m_nBkMeasureStepCount);
		for ( int i = 0; i < m_nBkMeasureStepCount ; i++ )
		{
			m_redSatMeasureArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_redSatMeasureArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_redSatMeasureArray[i].ResetLuxValue ();
		}
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

BOOL CMeasure::ValidateBackgroundGreenSatScale ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		SetSaturationSize(m_nBkMeasureStepCount);
		for ( int i = 0; i < m_nBkMeasureStepCount ; i++ )
		{
			m_greenSatMeasureArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_greenSatMeasureArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_greenSatMeasureArray[i].ResetLuxValue ();
		}
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

BOOL CMeasure::ValidateBackgroundBlueSatScale ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		SetSaturationSize(m_nBkMeasureStepCount);
		for ( int i = 0; i < m_nBkMeasureStepCount ; i++ )
		{
			m_blueSatMeasureArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_blueSatMeasureArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_blueSatMeasureArray[i].ResetLuxValue ();
		}
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

BOOL CMeasure::ValidateBackgroundYellowSatScale ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		SetSaturationSize(m_nBkMeasureStepCount);
		for ( int i = 0; i < m_nBkMeasureStepCount ; i++ )
		{
			m_yellowSatMeasureArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_yellowSatMeasureArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_yellowSatMeasureArray[i].ResetLuxValue ();
		}
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

BOOL CMeasure::ValidateBackgroundCyanSatScale ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		SetSaturationSize(m_nBkMeasureStepCount);
		for ( int i = 0; i < m_nBkMeasureStepCount ; i++ )
		{
			m_cyanSatMeasureArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_cyanSatMeasureArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_cyanSatMeasureArray[i].ResetLuxValue ();
		}
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

BOOL CMeasure::ValidateBackgroundMagentaSatScale ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		SetSaturationSize(m_nBkMeasureStepCount);
		for ( int i = 0; i < m_nBkMeasureStepCount ; i++ )
		{
			m_magentaSatMeasureArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_magentaSatMeasureArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_magentaSatMeasureArray[i].ResetLuxValue ();
		}
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

BOOL CMeasure::ValidateBackgroundCC24SatScale ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

//		SetSaturationSize(m_nBkMeasureStepCount);
		for ( int i = 0; i<m_cc24SatMeasureArray.GetSize() ; i++ )
		{
			m_cc24SatMeasureArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_cc24SatMeasureArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_cc24SatMeasureArray[i].ResetLuxValue ();
		}
		int iCC=GetConfig()->m_CCMode;
		if (iCC < RANDOM250)
			for (int i=0+100*iCC;i<100*(iCC+1);i++)
					m_cc24SatMeasureArray_master[i] = m_cc24SatMeasureArray[i-iCC*100];
		else if (iCC == RANDOM250) 
			for (int i=PATTERN_SIZE;i<PATTERN_SIZE+250;i++)
					m_cc24SatMeasureArray_master[i] = m_cc24SatMeasureArray[i-PATTERN_SIZE];
		else if (iCC == RANDOM500) 
			for (int i=PATTERN_SIZE+250;i<PATTERN_SIZE+250+500;i++)
					m_cc24SatMeasureArray_master[i] = m_cc24SatMeasureArray[i-(PATTERN_SIZE+250)];
		else if (iCC == USER) 
			for (int i=PATTERN_SIZE+250+500;i<PATTERN_SIZE+250+500+MAX_USER_CC_PATCH_SIZE;i++)
					m_cc24SatMeasureArray_master[i] = m_cc24SatMeasureArray[i-(PATTERN_SIZE+250+500)];
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

CColor CMeasure::GetGray(int i) const 
{
	return m_grayMeasureArray[i]; 
} 

CColor CMeasure::GetNearBlack(int i) const 
{ 
	return m_nearBlackMeasureArray[i]; 
} 

CColor CMeasure::GetNearWhite(int i) const 
{ 
	return m_nearWhiteMeasureArray[i]; 
} 

CColor CMeasure::GetRedSat(int i) const 
{ 
	return m_redSatMeasureArray[i]; 
} 

CColor CMeasure::GetGreenSat(int i) const 
{ 
	return m_greenSatMeasureArray[i]; 
} 

CColor CMeasure::GetBlueSat(int i) const 
{ 
	return m_blueSatMeasureArray[i]; 
} 

CColor CMeasure::GetYellowSat(int i) const 
{ 
	return m_yellowSatMeasureArray[i]; 
} 

CColor CMeasure::GetCyanSat(int i) const 
{ 
	return m_cyanSatMeasureArray[i]; 
} 

CColor CMeasure::GetMagentaSat(int i) const 
{ 
	return m_magentaSatMeasureArray[i]; 
} 

CColor CMeasure::GetCC24MasterSat(int i) const 
{ 
	return m_cc24SatMeasureArray_master[i]; 
} 

CColor CMeasure::GetCC24Sat(int i) 
{ 
	int iCC=GetConfig()->m_CCMode;

	if (m_binMeasure)
		return m_cc24SatMeasureArray[i]; 

	if (m_bpreV10 > 0)
	{
		if (i == 0 && m_bpreV10 == 1)
		{
			CString msg;
			BOOL isExtPat =( iCC == USER || iCC == CM10SAT || iCC == CM10SAT75 || iCC == CM5SAT || iCC == CM5SAT75 || iCC == CM4SAT || iCC == CM4SAT75 || iCC == CM4LUM || iCC == CM5LUM || iCC == CM10LUM || iCC == RANDOM250 || iCC == RANDOM500 || iCC == CM6NB || iCC == CMDNR || GetConfig()->m_CCMode == MASCIOR50);
			isExtPat = (isExtPat || GetConfig()->m_CCMode > 19);

			msg.SetString("File contains old style colorchecker data, load into slot ");
			msg+=(iCC == GCD?"Classic GCD":(GetConfig()->m_CCMode==MCD?"Classic MCD":(GetConfig()->m_CCMode==SKIN?"Pantone skin tones":(GetConfig()->m_CCMode==CCSG?"CalMan SG":isExtPat?GetConfig()->GetCColorsN(-1).c_str():(GetConfig()->m_CCMode==CMS?"CalMAN SG skin tones":(GetConfig()->m_CCMode==CPS?"ChromaPure skin tones":(GetConfig()->m_CCMode==CMC?"Classic CalMAN":"RGB Luminance Ramps")))))));
			msg+="?\r\n\r\nClick no if this is not the correct series and choose another from advanced preferences.";
			if (GetColorApp()->InMeasureMessageBox(msg,"Pre 3.3.0 file format dialog",MB_YESNO | MB_ICONQUESTION) == IDYES)			
			{
				CDataSetDoc *	pCurrentDocument = NULL;
				CMDIChildWnd *  pMDIFrameWnd = ( ( CMDIFrameWnd * ) AfxGetMainWnd () ) -> MDIGetActive ();

				pCurrentDocument = (CDataSetDoc *) pMDIFrameWnd->GetActiveDocument();
				for (int j = 0;j < m_cc24SatMeasureArray.GetSize();j++)
					m_cc24SatMeasureArray_master[j + (iCC<=RANDOM250?(iCC * 100):(iCC==RANDOM500?PATTERN_SIZE+250:PATTERN_SIZE+250+500))] = m_cc24SatMeasureArray[j];
				if (GetColorApp()->InMeasureMessageBox("Save to new  file?","Pre 3.3.0 file format save dialog",MB_YESNO | MB_ICONQUESTION) == IDYES)
				{
					CFileDialog fileSaveDialog ( FALSE, ".chc", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "HCFR Save File (*.chc)|*.chc||" );
					if ( fileSaveDialog.DoModal () == IDOK )
					{
						pCurrentDocument->OnSaveDocument(fileSaveDialog.GetPathName());
					}
				}
				m_bpreV10 = 0;
				pCurrentDocument->SetModifiedFlag();
//				pCurrentDocument->UpdateAllViews();
			}
			else
				m_bpreV10++;
		}
		else
			m_bpreV10++;

		if (m_bpreV10 == 5)
			m_bpreV10 = 1;

		return m_cc24SatMeasureArray[i]; 
	}

	return m_cc24SatMeasureArray_master[i + (iCC<=RANDOM250?(iCC * 100):(iCC==RANDOM500?PATTERN_SIZE+250:PATTERN_SIZE+250+500))];  //index increments by 100 until slot 19 (then 250 & 500 & user MAX_USER_CC_PATCH_SIZE & 2020_50)
} 

CString CMeasure::GetCCStr() const
{
	CString mStr("Color Checker sweeps active:\r\n");
	char *sweeps[37]={"GCD Classic \r\n","MCD Classic \r\n","Pantone Skin \r\n",
		"CalMAN Classic \r\n",
		"CalMAN Skin \r\n",
		"Chromapure Skin \r\n",
		"CalMAN SG \r\n",
		"CalMAN 10 pt Lum. \r\n",
		"CalMAN 4 pt Lum. \r\n",
		"CalMAN 5 pt Lum. \r\n",
		"CalMAN 10 pt Lum. \r\n",
		"CalMAN 4 pt Sat.(100AMP) \r\n",
		"CalMAN 4 pt Sat.(75AMP) \r\n",
		"CalMAN 5 pt Sat.(100AMP) \r\n",
		"CalMAN 5 pt Sat.(75AMP) \r\n",
		"CalMAN 10 pt Sat.(100AMP) \r\n",
		"CalMAN 10 pt Sat.(75AMP) \r\n",
		"CalMAN near black \r\n",
		"CalMan dynamic range \r\n",
		"BT2020_50 HDR\r\n",
		"LG_540_2016 HDR\r\n",
		"LG_540_2017 HDR\r\n",
		"LG_1000_2017 HDR\r\n",
		"LG_4000_2017 HDR\r\n",
		"LG_UK65xx_2018 HDR\r\n",
		"LG_OLED_V1_2018 HDR\r\n",
		"LG_OLED_V2_2018 HDR\r\n",
		"LG_OLED_V3_2018 HDR\r\n",
		"LG_OLED_10P_2019 HDR\r\n",
		"LG_OLED_22P_2019 HDR\r\n",
		"LG_OLED_10P_2020 HDR\r\n",
		"LG_OLED_22P_2020 HDR\r\n",
		"LG_OLED_10P_2021 HDR\r\n",
		"LG_OLED_22P_2021 HDR\r\n",
		"Random 250 \r\n",
		"Random 500 \r\n",
		"User\r\n"
	};
	for (int i=0;i<=USER;i++)
	{
		if (i<RANDOM250)
			if (m_cc24SatMeasureArray_master[i*100].isValid())
				mStr+=sweeps[i];
		if (i==RANDOM250)
			if (m_cc24SatMeasureArray_master[2300+100].isValid())
				mStr+=sweeps[i];
		if (i==RANDOM500)
			if (m_cc24SatMeasureArray_master[2300+100+250].isValid())
				mStr+=sweeps[i];
		if (i==USER)
			if (m_cc24SatMeasureArray_master[2300+100+250+500].isValid())
				mStr+=sweeps[i];
	}
	return mStr+="\r\n";
}

CColor CMeasure::GetPrimary(int i) const 
{ 
	return m_primariesArray[i]; 
} 

CColor CMeasure::GetRedPrimary() const 
{ 
	return GetPrimary(0);
} 

CColor CMeasure::GetGreenPrimary() const 
{ 
	return GetPrimary(1);
} 

CColor CMeasure::GetBluePrimary() const 
{ 
	return GetPrimary(2);
} 

CColor CMeasure::GetSecondary(int i) const 
{ 
	return m_secondariesArray[i]; 
} 

CColor CMeasure::GetYellowSecondary() const 
{ 
	return GetSecondary(0);
} 

CColor CMeasure::GetCyanSecondary() const 
{ 
	return GetSecondary(1);
} 

CColor CMeasure::GetMagentaSecondary() const 
{ 
	return GetSecondary(2);
} 

CColor CMeasure::GetAnsiBlack() const 
{ 
	CColor clr;

	clr = m_AnsiBlack; 

	return clr;
} 

CColor CMeasure::GetAnsiWhite() const 
{ 
	CColor clr;

	clr = m_AnsiWhite; 

	return clr;
} 

CColor CMeasure::GetOnOffBlack() const 
{ 
	CColor clr;

	clr = m_OnOffBlack; 

	return clr;
} 

CColor CMeasure::GetOnOffWhite() const 
{ 
	CColor clr;

	clr = m_OnOffWhite; 

	return clr;
} 

CColor CMeasure::GetPrimeWhite() const
{
	CColor clr;

	clr = m_PrimeWhite;

	return clr;
}

// Scale from the internal HDR-10 reference convention (1.0 = 10000 nits, the
// scale GetRefSat/GetRefCC24Sat produce in mode 5) to the diffuse-white-
// relative convention their consumers normalise with (RefWhite = YWhite /
// tmWhite). With tone mapping the diffuse white is compressed, so the correct
// factor is 10000 / TONE-MAPPED white; without tone mapping this reduces to
// the legacy 105.95640 (= 10000 / 94.37844).
double CMeasure::GetHDRRefScale() const
{
	// mode-5 getL_EOTF is absolute PQ - it ignores White/Black entirely - so
	// pass noDataColor like the other ~13 tmWhite sites. (Reading
	// GetGray(GetGrayScaleSize()-1) here would be an unchecked, unguarded
	// CArray index into a possibly-empty gray array.)
	double tmWhite = TmDiffuseWhiteNits(noDataColor, noDataColor);
	if ( tmWhite <= 0.0 )
		tmWhite = 94.37844;
	return 10000. / tmWhite;
}

// The white the measures grid normalises saturation / color-checker dE by
// (UpdateGrid's YWhite source + GetItemText's HDR block): the MEASURED prime
// white, the ON/OFF white for the special 75%/plasma standards, or the
// grayscale top for the Mascior-style HDR CC sets. The <90% fallback (a
// primaries run made at reduced stimulus) is SDR- and COLOR-CHECKER-only in
// the grid, hence bCC.
//
// Consumers must normalise dE by THIS white, not by the theoretical
// TmDiffuseWhiteNits: both are self-consistent (a perfect measurement reads
// dE 0 either way, which is why the /accuracytest invariant cannot tell them
// apart), but they diverge as soon as the display's white misses its target -
// dE scales roughly with (YWhite / tmWhite)^(1/3), so a 3.5%-low white shifted
// the 3D viewer ~1% away from the grid before this was shared.
double CMeasure::GetColorDEWhiteY(bool bSpecial, bool bCC, bool bMasciorCC) const
{
	BOOL isHDR = ( GetConfig()->m_GammaOffsetType == 5 );

	// The > 0.0 test matters as much as isValid(): ColorTriplet::isValid only
	// rejects values below -1.0 (it is a FX_NODATA sentinel check), so a gray
	// top that measured - or was typed into the grid as - zero luminance is
	// "valid" with GetY() == 0. Returning it would skip the m_TargetMaxL
	// fallback below and hand every consumer a zero dE white, which the 3D
	// viewer turns into a 100x reference/marker mismatch and /accuracytest into
	// a 999 convention failure.
	if ( isHDR && bCC && bMasciorCC )
	{
		int n = GetGrayScaleSize();
		if ( n > 0 && GetGray(n - 1).isValid() && GetGray(n - 1).GetY() > 0.0 )
			return GetGray(n - 1).GetY();
	}

	CColor prime = GetPrimeWhite();
	CColor onoff = GetOnOffWhite();
	double yPrime = prime.isValid() ? prime.GetY() : 0.0;
	double yOnOff = onoff.isValid() ? onoff.GetY() : 0.0;

	// UpdateGrid's chain: prime white, falling back to on/off white, then to
	// m_TargetMaxL when neither was measured (MainView.cpp ~3708-3722). The
	// special standards read the on/off white directly and do NOT fall back to
	// prime - the grid goes straight from a missing on/off white to TargetMaxL.
	double y = bSpecial ? yOnOff : ( yPrime > 0.0 ? yPrime : yOnOff );
	if ( bCC && onoff.isValid() && !isHDR && yOnOff > 0.0 && yPrime / yOnOff < 0.9 )
		y = yOnOff;

	if ( y <= 0.0 )
		y = GetConfig()->m_TargetMaxL;
	return y;
}

// The WHOLE saturation / color-checker dE normalisation in one place: the dE
// white, the reference rescale, and the YWhiteRef that pairs with it.
// displayMode follows CMainView: 5..10 saturation sweeps, 11 color checker.
//
// This exists because the normalisation had been re-derived at roughly a dozen
// sites - the measures grid, the 3D viewer, three Export blocks, RGBLevelWnd,
// SatLumShiftView, the CIE tooltip and the /accuracytest harness - and they had
// already drifted apart twice: the viewer normalised by the THEORETICAL
// tone-mapped white where the grid uses the MEASURED one, and Export lost the
// grid's manual-generator carve-out. A dE ~ 0 self-test cannot see that class of
// split (it holds under any self-consistent normalisation), so the guard has to
// be structural: one definition, no copies.
//
// SCOPE: the unified (automatic-generator) convention. The measures grid keeps a
// legacy carve-out for the MANUAL generator - the Mascior disc's 92.254965-nit
// white, a fixed 105.95640 rescale, and an extra YWhite * 94.37844 / tmWhite -
// which is deliberately NOT modelled here, for two reasons. It is legacy the
// 2026-07 unification chose to leave alone, and it is not even expressible as a
// per-family normalisation: one of its branches keys off the patch's COLUMN
// (UHDTV2's last saturation step), so it cannot be hoisted out of a patch loop
// the way this can. Sites that must byte-match the grid's on-screen numbers for
// a disc capture (Export) therefore keep an explicit, commented DVD branch. The
// resulting grid-vs-viewer split is tracked by the manual-generator entries in
// /accuracytest's kKnownFails.
ColorDENorm CMeasure::GetColorDENorm(int displayMode) const
{
	// 5..10 saturation, 11 color checker. Nothing else is modelled: the grid
	// normalises grayscale (0..4), primaries (1) and the profile cube (13)
	// differently, and silently handing one of those the SATURATION convention
	// would double-scale its reference (GetRefPrimary already applies
	// GetHDRRefScale internally). Assert rather than guess.
	ASSERT( displayMode >= 5 && displayMode <= 11 );

	CColorHCFRConfig * cfg = GetConfig();
	bool bCC      = ( displayMode == 11 );
	bool bSpecial = ( cfg->m_colorStandard == HDTVa || cfg->m_colorStandard == HDTVb );
	bool mascior  = ( bCC && IsMasciorCC(cfg->m_CCMode) );

	ColorDENorm n;
	n.whiteY    = GetColorDEWhiteY(bSpecial, bCC, mascior);
	n.refWhite  = 1.0;
	n.markScale = 1.0;
	n.deScale   = 1.0;

	// SDR: references are already white-relative and the measured white does all
	// the work, so every scale stays 1.0.
	if ( cfg->m_GammaOffsetType != 5 )
		return n;

	// Mascior-style HDR CC sets keep their own convention: references land on the
	// HDR-100 scale and are normalised against the grayscale top (which
	// GetColorDEWhiteY already returned), with YWhiteRef 1.0.
	if ( mascior )
	{
		n.markScale = 100.;
		n.deScale   = 100.;
		return n;
	}

	// HDR-10: GetRefSat/GetRefCC24Sat produce the 1.0 = 10000 nits convention.
	// markScale brings that to diffuse-white-relative (tone-map aware, = the
	// legacy 105.95640 with tone mapping off); refWhite then expresses the
	// measured white against the same diffuse white.
	// ONE tmWhite for all three members, and it comes from GetHDRRefScale so that
	// its "<= 0 -> 94.37844" clamp applies to every one of them: refWhite is
	// whiteY / tmWhite, and tmWhite == 10000 / markScale by construction.
	// Recomputing TmDiffuseWhiteNits separately here (which this used to do) both
	// dropped that clamp on refWhite alone - making deScale != markScale /
	// refWhite in exactly the degenerate case the clamp exists for, i.e. the two
	// documented spellings of this struct disagreeing - and paid a second full
	// PQ/BT.2390 evaluation plus four CColor deep copies per call, for a number
	// that is only equal to markScale's because mode-5 getL_EOTF happens to
	// ignore White/Black.
	n.markScale = GetHDRRefScale();
	n.refWhite  = ( n.whiteY > 0.0 ) ? n.whiteY * n.markScale / 10000. : 1.0;
	// deScale == markScale / refWhite in every branch, including whiteY <= 0
	// (where refWhite is 1.0 and deScale falls back to markScale). Computed
	// directly so a zero white cannot become a division by zero here instead of
	// inside GetDeltaE.
	n.deScale   = ( n.whiteY > 0.0 ) ? 10000. / n.whiteY : n.markScale;
	return n;
}

CColor CMeasure::GetMeasurement(int i) const
{
	return m_measurementsArray[i]; 
} 

void CMeasure::AppendMeasurements(const CColor & aColor, int isPrimary, int last_minCol) 
{
	if (m_measurementsArray.GetSize() >= m_nbMaxMeasurements )
		m_measurementsArray.RemoveAt(0,1); 
	
	// Using a pointer here is a workaround for a VC++ 6.0 bug, which uses Matrix copy constructor instead of CColor one when using a const CColor ref.
	CColor * pColor = (CColor *) & aColor;
	m_measurementsArray.InsertAt(m_measurementsArray.GetSize(),*pColor); 

	m_isModified=TRUE; 

	FreeMeasurementAppended(isPrimary, last_minCol); 
}

// The EXACT percent triplets MeasurePrimaries sends for the special
// standards (R,G,B,Y,C,M) - the reference must decode these actual wire
// codes: re-encoding the analog color lands up to half a code away from the
// 2-decimal-rounded tables (~0.4 dE). Keep byte-identical to the GenColors
// tables in MeasurePrimaries.
static const double kHDTVaWireCodes[6][3] =
{
	{ 68.04,20.09,20.09 }, { 27.85,73.06,27.85 }, { 19.18,19.18,50.22 },
	{ 73.9726,73.9726,33.3333 }, { 36.07,73.06,73.06 }, { 64.3836,29.2237,64.3836 },
};
static const double kHDTVbWireCodes[6][3] =
{
	{ 79.9087,10.0457,10.0457 }, { 30.137,79.9087,30.137 }, { 50.2283,50.2283,79.9087 },
	{ 79.9087,79.9087,10.0457 }, { 10.0457,79.9087,79.9087 }, { 79.9087,10.0457,79.9087 },
};

// Shared by GetRefPrimary/GetRefSecondary: turn the analog primary/secondary
// XYZ into the reference the wire actually produces. Plain standards send
// pure 0/100% codes - exact on every grid and under any gamma - so the analog
// color IS the wire-exact reference. The special 75%-style standards send
// fractional codes that must be grid-quantized and decoded: HDTVa/b decode
// the actual hardcoded wire tables (in the HDTV space the sensor/dE path
// uses); CC6 (no wire table) keeps the analog encode->quantize->decode
// chain. 'idx' is the patch index (0-2 primaries, 3-5 secondaries).
// HDR modes 5/7 keep the analog reference (patch levels are recalculated at
// measure time; legacy behavior). Window Intensity is deliberately NOT
// modeled - it dims the measured white anchor equally and cancels in the
// white-relative dE (see the note above TmDiffuseWhiteNits).
static CColor WireModeledPrimaryReference ( const CMeasure & measure, const ColorXYZ & xyz, const CColorReference & cRef, int idx )
{
	int mode = GetConfig()->m_GammaOffsetType;
	if (GetConfig()->m_colorStandard == sRGB) mode = 99;
	bool isSpecial = ( cRef.m_standard == HDTVa || cRef.m_standard == HDTVb || cRef.m_standard == CC6 );

	if ( !isSpecial || mode == 5 || mode == 7 )
		return xyz;

	CColor White, Black;
	if ( measure.GetGray(0).isValid() )
	{
		White = measure.GetGray ( measure.GetGrayScaleSize() - 1 );
		Black = measure.GetOnOffBlack();
	}
	double gamma = GetConfig()->m_useMeasuredGamma ? GetConfig()->m_GammaAvg : GetConfig()->m_GammaRef;
	bool b10 = GetConfig()->GetUse10bitLevels();
	bool lim = GetConfig()->GetRGB16_235();

	const double (*pWire)[3] = ( cRef.m_standard == HDTVa ) ? kHDTVaWireCodes
							 : ( cRef.m_standard == HDTVb ) ? kHDTVbWireCodes : NULL;

	CColor aColor;
	ColorRGB rgb;
	if ( !pWire )
	{
		aColor.SetXYZValue ( xyz );
		rgb = aColor.GetRGBValue ( cRef );
	}
	for ( int ch = 0 ; ch < 3 ; ch ++ )
	{
		double q;
		if ( pWire )
			q = pWire[idx][ch] / 100.0;
		else
		{
			q = min ( max ( rgb[ch], 0.0 ), 1.0 );
			q = ( q <= 0.0 || q >= 1.0 ) ? q : pow ( q, 1.0 / 2.22 );
		}
		q = SnapToVideoGrid ( q, b10, lim );
		if ( mode >= 4 )
			rgb[ch] = ( q <= 0.0 || q >= 1.0 ) ? q : getL_EOTF ( q, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode );
		else
			rgb[ch] = ( q <= 0.0 || q >= 1.0 ) ? q : pow ( q, gamma );
	}
	// HDTVa/b wire codes are HDTV-space signals: the sensor and the dE path
	// both operate in plain HDTV for the special modes.
	aColor.SetRGBValue ( rgb, pWire ? CColorReference(HDTV) : cRef );
	ColorXYZ out = aColor.GetXYZValue();

	// HDTVa's white anchor is the 75% patch (MeasurePrimaries GenColors[6] =
	// 75/75/75), and the primaries dE normalizes the measurement to that
	// PrimeWhite while the reference is normalized to 1.0 - so express the
	// reference relative to the decoded 75% white, exactly as the sensor's
	// 75% patch relates to the measured white. (HDTVb's white patch is 100%:
	// no rescale.)
	if ( cRef.m_standard == HDTVa )
	{
		double qw = SnapToVideoGrid ( 0.75, b10, lim );
		double w = ( mode >= 4 ) ? getL_EOTF ( qw, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode )
								 : pow ( qw, gamma );
		if ( w > 0.0 )
		{
			out[0] /= w;
			out[1] /= w;
			out[2] /= w;
		}
	}
	return out;
}

CColor CMeasure::GetRefPrimary(int i) const
{
	// UHDTV3/4 pseudo-spaces: the wire carries transport-encoded (BT.2020),
	// grid-quantized codes; the fully saturated sweep point GetRefSat(i, 1.0)
	// models that exactly (encode -> SnapToVideoGrid -> decode in the transport
	// space), while the analog primary does not. stimLevel forced to 1.0:
	// primaries are measured at full level regardless of the bound sweep level.
	if ( GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4 )
	{
		CColor ref = GetRefSat(i, 1.0, false, 1.0);
		// HDR-10: GetRefSat returns the sat-grid convention (1.0 = 10000 nits);
		// primaries consumers expect the diffuse-white-relative convention.
		// The scale is 10000 / tone-mapped white (= the legacy 105.95640 with
		// tone mapping off) - the primaries view normalises references by
		// RefWhite = YWhite / tmWhite without the sat grid's YWhite rescale.
		if ( GetConfig()->m_GammaOffsetType == 5 )
		{
			double s = GetHDRRefScale();
			ref.SetX(ref.GetX() * s);
			ref.SetY(ref.GetY() * s);
			ref.SetZ(ref.GetZ() * s);
		}
		return ref;
	}

	CColorReference cRef = GetColorReference();
	switch ( i )
	{
		case 0:	// red
			return WireModeledPrimaryReference ( *this, ColorXYZ(cRef.GetRed()), cRef, 0 );

		case 1:	// green
			return WireModeledPrimaryReference ( *this, ColorXYZ(cRef.GetGreen()), cRef, 1 );

		case 2:	// blue
			return WireModeledPrimaryReference ( *this, ColorXYZ(cRef.GetBlue()), cRef, 2 );
	}

	// Cannot execute this if "i" is OK.
	ASSERT(0);
	return noDataColor;
}

CColor CMeasure::GetRefSecondary(int i) const
{
	// UHDTV3/4: model the wire via the fully saturated sweep point - see
	// GetRefPrimary.
	if ( GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4 )
	{
		CColor ref = GetRefSat(i + 3, 1.0, false, 1.0);
		// HDR-10 convention rescale - see GetRefPrimary.
		if ( GetConfig()->m_GammaOffsetType == 5 )
		{
			double s = GetHDRRefScale();
			ref.SetX(ref.GetX() * s);
			ref.SetY(ref.GetY() * s);
			ref.SetZ(ref.GetZ() * s);
		}
		return ref;
	}

	CColorReference cRef = GetColorReference();
	switch ( i )
	{
		case 0:	// Yellow
			return WireModeledPrimaryReference ( *this, ColorXYZ(cRef.GetYellow()), cRef, 3 );

		case 1:	// Cyan
			return WireModeledPrimaryReference ( *this, ColorXYZ(cRef.GetCyan()), cRef, 4 );

		case 2:	// Magenta
			return WireModeledPrimaryReference ( *this, ColorXYZ(cRef.GetMagenta()), cRef, 5 );
	}

	// Cannot execute this if "i" is OK.
	ASSERT(0);
	return noDataColor;
}

CColor CMeasure::GetRefSat(int i, double sat_ratio, bool special, double stimLevel) const
{
	CColor	refColor;

	if ( stimLevel < 0.0 )
		stimLevel = m_activeSatLevel;	// default: track the bound sweep level
	ColorxyY	refWhite(GetColorReference().GetWhite());
	double	x, y;
	double	xstart = refWhite[0];
	double	ystart = refWhite[1];
	double	YLuma=1.0;
	// Window Intensity is deliberately NOT modeled here: it dims the measured
	// white anchor equally, so it cancels in the white-relative dE (see the
	// note above TmDiffuseWhiteNits).
	int mode = GetConfig()->m_GammaOffsetType;

	GetConfig()->m_bHDR100 = FALSE;
	
	CColor pRef[3];
	pRef[0].SetxyYValue(ColorxyY(0.6400, 0.3300));
	pRef[1].SetxyYValue(ColorxyY(0.3000, 0.6000));
	pRef[2].SetxyYValue(ColorxyY(0.1500, 0.0600));
	CColor sRef[3];
	sRef[0].SetxyYValue(ColorxyY(0.419314,0.505251));
	sRef[1].SetxyYValue(ColorxyY(0.224650,0.328741));
	sRef[2].SetxyYValue(ColorxyY(0.320913, 0.154177));
	int m_cRef=GetColorReference().m_standard;
	// One basis for the endpoint below AND the YLuma switch further down.
	// Both used to build this reference independently, which cost an extra
	// matrix inversion + secondary solve on every call - and GetRefSat runs
	// per hue per saturation step on the CIE-chart and 3D-viewer redraw paths.
	const CColorReference basis = special ? CColorReference(HDTV)
										  : ContainerInnerReference(GetColorReference());
	//display rec709 sat points in special colorspace modes
	if (!special)
	{
		if (m_cRef == UHDTV3 || m_cRef == UHDTV4)
		{
			// UHDTV3/UHDTV4 are containers: the sweep runs from the active
			// white out to the INNER gamut's corner (P3 inside 2020, Rec.709
			// inside 2020). Take that corner from ContainerInnerReference -
			// the same reference GenerateSaturationColors builds the wire
			// patches in (its cRef) and the same one ContainerPrimaryLinear
			// indexes for the primaries wire - so reference and wire follow
			// one white. Note the sat wire derives its corner as the primary
			// SUM, ColorXYZ(ColorRGB(1,1,0), cRef), while this reads
			// UpdateSecondary's line intersection: equal by construction
			// (R+G = white-B lies on both lines), but two formulas - a change
			// to either must keep them in step.
			// These endpoints used to be hardcoded xy tables
			// (p3Ref/p3sRef/rRef/rsRef) evaluated at D65: the primaries are
			// white-independent so they were merely duplicated, but the
			// secondaries are white-point MIXTURES (see UpdateSecondary) and
			// so were wrong under any custom white.
			//
			// GetRefPrimary/GetRefSecondary route these two standards BACK
			// through here (GetRefSat(i, 1.0)), so this must not call them.
			switch (i)
			{
				case 0:	refColor.SetXYZValue(basis.GetRed());		break;
				case 1:	refColor.SetXYZValue(basis.GetGreen());		break;
				case 2:	refColor.SetXYZValue(basis.GetBlue());		break;
				case 3:	refColor.SetXYZValue(basis.GetYellow());	break;
				case 4:	refColor.SetXYZValue(basis.GetCyan());		break;
				case 5:	refColor.SetXYZValue(basis.GetMagenta());	break;
				// Fail the way GetRefPrimary/GetRefSecondary do rather than
				// returning a plausible-looking chromaticity for a bad index.
				default: ASSERT(0); return noDataColor;
			}
		}
		else
		{
			if ( i < 3 )
				refColor = GetRefPrimary(i);
			else
				refColor = GetRefSecondary(i-3);
		}
	}
	else
	{
		if ( i < 3 )
			refColor = pRef[i];
		else
			refColor = sRef[i-3];
	}

	switch (i)
	{
		case 0:
			YLuma = basis.GetRedReferenceLuma(true);
			break;
		case 1:
			YLuma = basis.GetGreenReferenceLuma(true);
			break;
		case 2:
			YLuma = basis.GetBlueReferenceLuma(true);
			break;
		case 3:
			YLuma = basis.GetYellowReferenceLuma(true);
			break;
		case 4:
			YLuma = basis.GetCyanReferenceLuma(true);
			break;
		case 5:
			YLuma = basis.GetMagentaReferenceLuma(true);
			break;
	}
	
	double	xend = refColor.GetxyYValue()[0];
	double	yend = refColor.GetxyYValue()[1];

	x = xstart + ( (xend - xstart) * sat_ratio );
	y = ystart + ( (yend - ystart) * sat_ratio );

	CColor	aColor;
    CColor White = CMeasure::GetGray ( CMeasure::GetGrayScaleSize() - 1 );
//	CColor Black = CMeasure::GetGray ( 0 );
	CColor Black = CMeasure::GetOnOffBlack();
    double gamma=GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef);

	double tmWhite = TmDiffuseWhiteNits(White, Black);

	// 100%-saturation luma convention for the analog (unquantized) HDR-10
	// reference. Full-stimulus only: at reduced stimLevel the quantize gate
	// below opens and the wire (GenerateSaturationColors) encodes the plain
	// K-luma color - carrying this scale into that encode lands the
	// reference up to a code away from the signal.
	if (mode == 5 && sat_ratio == 1 && stimLevel >= 1.0 && GetConfig()->m_colorStandard != UHDTV3 && GetConfig()->m_colorStandard != UHDTV4)
		YLuma = YLuma * tmWhite / 94.37844;

	aColor.SetxyYValue (x, y, YLuma);

	ColorRGB rgb;

	if (mode == 5)
	{
		aColor.SetX(aColor.GetX()/105.95640);
		aColor.SetY(aColor.GetY()/105.95640);
		aColor.SetZ(aColor.GetZ()/105.95640);
		GetConfig()->m_bHDR100 = TRUE;
	}

	// UHDTV3/4: the wire carries the TRANSPORT (BT.2020) encoding - the patch
	// generators quantize the transport triplet - so the reference must encode,
	// quantize and decode in the transport space too, or the two sides land up
	// to a code apart per channel.
	if (!special)
		rgb = aColor.GetRGBValue (((m_cRef == UHDTV3||m_cRef == UHDTV4)?ContainerTransportReference(GetColorReference()):GetColorReference()));
	else
		rgb=aColor.GetRGBValue(CColorReference(HDTV));

	double r=rgb[0],g=rgb[1],b=rgb[2];
	double qr,qg,qb;
	bool b10 = GetConfig()->GetUse10bitLevels();
	bool lim = GetConfig()->GetRGB16_235();

	if (stimLevel < 1.0 || sat_ratio < 1 || (sat_ratio == 1 && (GetConfig()->m_GammaOffsetType != 5 || GetConfig()->m_colorStandard == UHDTV3 || GetConfig()->m_colorStandard == UHDTV4)) ) // adjust references locations for difference between target gamma and 2.2
	{
		if (GetConfig()->m_colorStandard == sRGB) mode = 99;
		if ( mode >= 4 )
		{
			if (mode == 5 || mode == 7)
			{
				qr = getL_EOTF(r,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, -1*mode) * stimLevel;
				qg = getL_EOTF(g,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, -1*mode) * stimLevel;
				qb = getL_EOTF(b,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, -1*mode) * stimLevel;
				qr = SnapToVideoGrid( qr, b10, lim );
				qg = SnapToVideoGrid( qg, b10, lim );
				qb = SnapToVideoGrid( qb, b10, lim );

				r = getL_EOTF(qr,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) / (mode==5?100.:1.0);
				g = getL_EOTF(qg,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) / (mode==5?100.:1.0);
				b = getL_EOTF(qb,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) / (mode==5?100.:1.0);
			}
			else
			{
				qr = ((r<=0||r>=1)?min(max(r,0),1):pow(r, 1.0 / 2.22)) * stimLevel;
				qg = ((g<=0||g>=1)?min(max(g,0),1):pow(g, 1.0 / 2.22)) * stimLevel;
				qb = ((b<=0||b>=1)?min(max(b,0),1):pow(b, 1.0 / 2.22)) * stimLevel;
				qr = SnapToVideoGrid( qr, b10, lim );
				qg = SnapToVideoGrid( qg, b10, lim );
				qb = SnapToVideoGrid( qb, b10, lim );
			    r = getL_EOTF(qr,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode);
			    g = getL_EOTF(qg,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode);
			    b = getL_EOTF(qb,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode);
			}
		}
		else
		{
			qr = ((r<=0||r>=1)?min(max(r,0),1):pow(r, 1.0 / 2.22)) * stimLevel;
			qg = ((g<=0||g>=1)?min(max(g,0),1):pow(g, 1.0 / 2.22)) * stimLevel;
			qb = ((b<=0||b>=1)?min(max(b,0),1):pow(b, 1.0 / 2.22)) * stimLevel;
			qr = SnapToVideoGrid( qr, b10, lim );
			qg = SnapToVideoGrid( qg, b10, lim );
			qb = SnapToVideoGrid( qb, b10, lim );
			r=(qr<=0||qr>=1)?min(max(qr,0),1):pow(qr,gamma);
			g=(qg<=0||qg>=1)?min(max(qg,0),1):pow(qg,gamma);
			b=(qb<=0||qb>=1)?min(max(qb,0),1):pow(qb,gamma);
		}

	if (!special)
			aColor.SetRGBValue (ColorRGB(r,g,b), ((m_cRef == UHDTV3||m_cRef == UHDTV4)?ContainerTransportReference(GetColorReference()):GetColorReference()));
		else
			aColor.SetRGBValue (ColorRGB(r,g,b), CColorReference(HDTV));	
	}

	return aColor;
}

ColorRGB RGB[MAX_USER_CC_PATCH_SIZE];

void CMeasure::GetRefCC24Sat(int i, CColor& ccRef) const
{
    CColorReference cRef = GetColorReference();
	bool const_XYZ = FALSE;
	GetConfig()->m_bHDR100 = FALSE;

	switch (GetConfig()->m_CCMode)
	{
//GCD
    	case GCD:
		{
			const_XYZ = TRUE;
			GetConfig()->m_bHDR100 = TRUE;
            RGB[0] = ColorRGB(0, 0, 0);
			RGB[1] = ColorRGB(.6210,.6210,.6210) ;
			RGB[2] = ColorRGB(.7306,.7306,.7306) ;
			RGB[3] = ColorRGB(.8219,.8219,.8219) ;
			RGB[4] = ColorRGB(.8995,.8995,.8995) ;
			RGB[5] = ColorRGB(1.0,1.0,1.0) ;
			RGB[6] = ColorRGB(.452,.3196,.2603);
			RGB[7] = ColorRGB(.758,.589,.5114);
			RGB[8] = ColorRGB(.3699,.4795,.6119);
			RGB[9] = ColorRGB(.3516,.4201,.2603);
			RGB[10] = ColorRGB(.5114,.5023,.6895);
			RGB[11] = ColorRGB(.3881,.7397,.6621);
			RGB[12] = ColorRGB(.8493,.4703,.1598);
			RGB[13] = ColorRGB(.2922,.3607,.6393);
			RGB[14] = ColorRGB(.7580,.3288,.3790);
			RGB[15] = ColorRGB(.3607,.2420,.4201);
			RGB[16] = ColorRGB(.6210,.7306,.2511);
			RGB[17] = ColorRGB(.8995,.6301,.1781);
			RGB[18] = ColorRGB(.2009,.2420,.5890);
			RGB[19] = ColorRGB(.2785,.5799,.2785);
			RGB[20] = ColorRGB(.6895,.1918,.2283);
			RGB[21] = ColorRGB(.9315,.7808,.1279);
			RGB[22] = ColorRGB(.7306,.3288,.5708);
			RGB[23] = ColorRGB(  0.0, .5205,.6393);
		break;
		}
//MCD
    	case MCD:
		{
			const_XYZ = TRUE;
			GetConfig()->m_bHDR100 = TRUE;
			RGB[0] = ColorRGB(.21,.2055,.21);
			RGB[1] = ColorRGB(.3288,.3288,.3288);
			RGB[2] = ColorRGB(.4749,.4749,.4703);
			RGB[3] = ColorRGB(.6256,.6256,.6256);
			RGB[4] = ColorRGB(.7854,.7854,.7808);
			RGB[5] = ColorRGB(.9498,.9452,.9269);
			RGB[6] = ColorRGB(.4474,.3151,.2603);
			RGB[7] = ColorRGB(.7580,.5845,.5068);
			RGB[8] = ColorRGB(.3699,.4795,.6073);
			RGB[9] = ColorRGB(.3470,.4247,.2648);
			RGB[10] = ColorRGB(.5068,.5022,.6849);
			RGB[11] = ColorRGB(.3927,.7397,.6621);
			RGB[12] = ColorRGB(.8447,.4749,.1644);
			RGB[13] = ColorRGB(.2877,.3562,.6438);
			RGB[14] = ColorRGB(.7580,.3333,.3836);
			RGB[15] = ColorRGB(.3607,.2420,.4201);
			RGB[16] = ColorRGB(.6210,.7306,.2466);
			RGB[17] = ColorRGB(.8995,.6347,.1826);
			RGB[18] = ColorRGB(.1963,.2420,.5936);
			RGB[19] = ColorRGB(.2831,.5799,.2785);
			RGB[20] = ColorRGB(.6895,.1918,.2283);
			RGB[21] = ColorRGB(.9315,.7808,.1279);
			RGB[22] = ColorRGB(.7261,.3242,.5753);
			RGB[23] = ColorRGB(.1187,.5160,.5982);
		break;
		}
		//CalMAN classic steps
	    case CMC:
		{
			const_XYZ = TRUE;
			GetConfig()->m_bHDR100 = TRUE;
			RGB[0] = ColorRGB(1, 1, 1 );
			RGB[1] = ColorRGB(0.8995, 0.8995, 0.8995 );
			RGB[2] = ColorRGB(0.8329, 0.8329, 0.8329 );
			RGB[3] = ColorRGB(0.7306, 0.7306, 0.7306 );
			RGB[4] = ColorRGB(0.6210, 0.6210, 0.6210 );
			RGB[5] = ColorRGB(0.0, 0.0, 0.0 );
			RGB[6] = ColorRGB(0.4521,	0.3196,	0.2603);
			RGB[7] = ColorRGB(0.7580,	0.5890,	0.5114);
			RGB[8] = ColorRGB(  0.3699,	0.4795,	0.6119);
			RGB[9] = ColorRGB(  0.3516,	0.4201,	0.2603);
			RGB[10] = ColorRGB(  0.5114,	0.5023,	0.6895);
			RGB[11] = ColorRGB(  0.3881,	0.7397,	0.6621);
			RGB[12] = ColorRGB(  0.8493,	0.4703,	0.1598);
			RGB[13] = ColorRGB(  0.2922,	0.3607,	0.6393);
			RGB[14] = ColorRGB(  0.7580,	0.3288,	0.3790);
			RGB[15] = ColorRGB(  0.3607,	0.2420,	0.4201);
			RGB[16] = ColorRGB(  0.6210,	0.7306,	0.2511);
			RGB[17] = ColorRGB(  0.8995,	0.6301,	0.1781);
			RGB[18] = ColorRGB(  0.2009,	0.2420,	0.5890);
			RGB[19] = ColorRGB(  0.2785,	0.5799,	0.2785);
			RGB[20] = ColorRGB(  0.6895,	0.1918,	0.2283);
			RGB[21] = ColorRGB(  0.9315,	0.7808,	0.1279);
			RGB[22] = ColorRGB(  0.7306,	0.3288,	0.5708);
			RGB[23] = ColorRGB(  0.0000,	0.5205,	0.6393);
		break;		
        }
	    case CMS:
		{
			const_XYZ = TRUE;
			GetConfig()->m_bHDR100 = TRUE;
			RGB[0] = ColorRGB(1.0000,	1.0000,	1.0000);
			RGB[1] = ColorRGB(0.0000,	0.0000,	0.0000);
			RGB[2] = ColorRGB(0.4384,	0.2511,	0.1507);
			RGB[3] = ColorRGB(0.7991,	0.5388,	0.4018);
			RGB[4] = ColorRGB(1.0000,	0.7808,	0.5982);
			RGB[5] = ColorRGB(1.0000,	0.7808,	0.6712);
			RGB[6] = ColorRGB(0.9680,	0.6712,	0.4886);
			RGB[7] = ColorRGB(0.7808,	0.5479,	0.3607);
			RGB[8] = ColorRGB( 0.5616,	0.3607,	0.2009);
			RGB[9] = ColorRGB(0.8082,	0.5890,	0.4521);
			RGB[10] = ColorRGB( 0.6301,	0.3379,	0.1279);
			RGB[11] = ColorRGB( 0.8402,	0.5205,	0.3607);
			RGB[12] = ColorRGB( 0.8219,	0.5388,	0.4110);
			RGB[13] = ColorRGB( 0.9817,	0.5982,	0.4521);
			RGB[14] = ColorRGB( 0.7808,	0.5616,	0.4201);
			RGB[15] = ColorRGB( 0.7900,	0.5479,	0.4201);
			RGB[16] = ColorRGB( 0.7991,	0.5616,	0.4110);
			RGB[17] = ColorRGB( 0.4795,	0.2922,	0.1507);
			RGB[18] = ColorRGB( 0.8493,	0.5479,	0.3699);
		break;		
        }
	    case CPS:
		{
			const_XYZ = TRUE;
			GetConfig()->m_bHDR100 = TRUE;
			RGB[0] = ColorRGB(1, 1, 1 );
			RGB[1] = ColorRGB(0.8447,	0.4932,	0.3425);
			RGB[2] = ColorRGB(0.7900,	0.5525,	0.4840);
			RGB[3] = ColorRGB(0.9361,	0.6758,	0.5799);
			RGB[4] = ColorRGB(0.9452,	0.6119,	0.5297);
			RGB[5] = ColorRGB(0.7534,	0.5616,	0.4292);
			RGB[6] = ColorRGB(0.7489,	0.5708,	0.4932);
			RGB[7] = ColorRGB(0.5525,	0.3653,	0.2466);
			RGB[8] = ColorRGB(0.7626,	0.5662,	0.4977);
			RGB[9] = ColorRGB(0.7580,	0.5890,	0.5114);
			RGB[10] = ColorRGB(0.7717,	0.5662,	0.4886);
			RGB[11] = ColorRGB(0.6210,	0.3425,	0.1781);
			RGB[12] = ColorRGB(0.4703,	0.2968,	0.1872);
			RGB[13] = ColorRGB(0.8174,	0.5297,	0.4247);
			RGB[14] = ColorRGB(0.8265,	0.5616,	0.4384);
			RGB[15] = ColorRGB(0.9315,	0.6849,	0.4840);
			RGB[16] = ColorRGB(0.8174,	0.6027,	0.4247);
			RGB[17] = ColorRGB(0.4521,	0.3196,	0.2648);
			RGB[18] = ColorRGB(0.7626,	0.5890,	0.5114);
		break;		
        }
        //Pantone skin set
	    case SKIN:
		{
			const_XYZ = TRUE;
			GetConfig()->m_bHDR100 = TRUE;
			RGB[0] = ColorRGB(1,0.876712329,0.767123288);
			RGB[1] = ColorRGB(0.940639269,0.835616438,0.744292237);
			RGB[2] = ColorRGB(0.931506849,0.808219178,0.703196347);
			RGB[3] = ColorRGB(0.881278539,0.721461187,0.598173516);
			RGB[4] = ColorRGB(0.899543379,0.762557078,0.598173516);
			RGB[5] = ColorRGB(1,0.863013699,0.698630137);
			RGB[6] = ColorRGB(0.899543379,0.721461187,0.561643836);
			RGB[7] = ColorRGB(0.899543379,0.625570776,0.452054795);
			RGB[8] = ColorRGB(0.904109589,0.621004566,0.429223744);
			RGB[9] = ColorRGB(0.858447489,0.566210046,0.397260274);
			RGB[10] = ColorRGB(0.808219178,0.589041096,0.484018265);
			RGB[11] = ColorRGB(0.776255708,0.470319635,0.337899543);
			RGB[12] = ColorRGB(0.730593607,0.424657534,0.287671233);
			RGB[13] = ColorRGB(0.648401826,0.447488584,0.342465753);
			RGB[14] = ColorRGB(0.940639269,0.785388128,0.789954338);
			RGB[15] = ColorRGB(0.867579909,0.657534247,0.625570776);
			RGB[16] = ColorRGB(0.726027397,0.484018265,0.429223744);
			RGB[17] = ColorRGB(0.657534247,0.456621005,0.424657534);
			RGB[18] = ColorRGB(0.680365297,0.392694064,0.319634703);
			RGB[19] = ColorRGB(0.360730594,0.219178082,0.210045662);
			RGB[20] = ColorRGB(0.794520548,0.515981735,0.260273973);
			RGB[21] = ColorRGB(0.739726027,0.447488584,0.237442922);
			RGB[22] = ColorRGB(0.438356164,0.255707763,0.223744292);
			RGB[23] = ColorRGB(0.639269406,0.525114155,0.415525114);
            break;
        }
		case AXIS:
		{
			int j;
			RGB[0] = ColorRGB(0,0,0);
			for (j=0;j<10;j++) {RGB[j+1] = ColorRGB((j+1) * 0.1,(j+1) * 0.1,(j+1) * 0.1);}
			for (j=0;j<10;j++) {RGB[j+11] = ColorRGB((j+1) * 0.1,0.0,0.0);}
			for (j=0;j<10;j++) {RGB[j+21] = ColorRGB(0.0, (j+1) * 0.1,0.0);}
			for (j=0;j<10;j++) {RGB[j+31] = ColorRGB(0.0, 0.0, (j+1) * 0.1);}
			for (j=0;j<10;j++) {RGB[j+61] = ColorRGB((j+1) * 0.1, (j+1) * 0.1, 0.0);}
			for (j=0;j<10;j++) {RGB[j+41] = ColorRGB(0.0, (j+1) * 0.1, (j+1) * 0.1);}
			for (j=0;j<10;j++) {RGB[j+51] = ColorRGB((j+1) * 0.1, 0.0, (j+1) * 0.1);}
            break;
        }
        //Color checker SG 96 colors
		case CCSG:
		{
			const_XYZ = TRUE;
			GetConfig()->m_bHDR100 = TRUE;
            RGB[0] = ColorRGB(1,1,1);
            RGB[1] = ColorRGB(0.872146119,0.872146119,0.872146119);
            RGB[2] = ColorRGB(0.771689498,0.771689498,0.771689498);
            RGB[3] = ColorRGB(0.721461187,0.721461187,0.721461187);
            RGB[4] = ColorRGB(0.671232877,0.671232877,0.671232877);
            RGB[5] = ColorRGB(0.611872146,0.611872146,0.611872146);
            RGB[6] = ColorRGB(0.561643836,0.561643836,0.561643836);
            RGB[7] = ColorRGB(0.461187215,0.461187215,0.461187215);
            RGB[8] = ColorRGB(0.420091324,0.420091324,0.420091324);
            RGB[9] = ColorRGB(0.369863014,0.369863014,0.369863014);
            RGB[10] = ColorRGB(0.328767123,0.328767123,0.328767123);
            RGB[11] = ColorRGB(0.292237443,0.292237443,0.292237443);
            RGB[12] = ColorRGB(0.210045662,0.210045662,0.210045662);
            RGB[13] = ColorRGB(0.168949772,0.168949772,0.168949772);
            RGB[14] = ColorRGB(0,0,0);
            RGB[15] = ColorRGB(0.570776256,0.109589041,0.328767123);
            RGB[16] = ColorRGB(0.292237443,0.178082192,0.278538813);
            RGB[17] = ColorRGB(0.849315068,0.821917808,0.757990868);
            RGB[18] = ColorRGB(0.438356164,0.251141553,0.150684932);
            RGB[19] = ColorRGB(0.799086758,0.538812785,0.401826484);
            RGB[20] = ColorRGB(0.351598174,0.438356164,0.511415525);
            RGB[21] = ColorRGB(0.328767123,0.378995434,0.118721461);
            RGB[22] = ColorRGB(0.502283105,0.461187215,0.570776256);
            RGB[23] = ColorRGB(0.420091324,0.707762557,0.561643836);
            RGB[24] = ColorRGB(1,0.780821918,0.598173516);
            RGB[25] = ColorRGB(0.388127854,0.109589041,0.159817352);
            RGB[26] = ColorRGB(0.748858447,0.118721461,0.292237443);
            RGB[27] = ColorRGB(0.739726027,0.502283105,0.611872146);
            RGB[28] = ColorRGB(0.429223744,0.351598174,0.538812785);
            RGB[29] = ColorRGB(0.99086758,0.780821918,0.707762557);
            RGB[30] = ColorRGB(0.908675799,0.438356164,0);
            RGB[31] = ColorRGB(0.242009132,0.310502283,0.561643836);
            RGB[32] = ColorRGB(0.780821918,0.251141553,0.269406393);
            RGB[33] = ColorRGB(0.319634703,0.150684932,0.319634703);
            RGB[34] = ColorRGB(0.662100457,0.707762557,0);
            RGB[35] = ColorRGB(0.917808219,0.589041096,0);
            RGB[36] = ColorRGB(0.840182648,0.908675799,0.707762557);
            RGB[37] = ColorRGB(0.799086758,0,0.082191781);
            RGB[38] = ColorRGB(0.337899543,0.127853881,0.210045662);
            RGB[39] = ColorRGB(0.461187215,0.118721461,0.438356164);
            RGB[40] = ColorRGB(0,0.210045662,0.351598174);
            RGB[41] = ColorRGB(0.739726027,0.858447489,0.721461187);
            RGB[42] = ColorRGB(0.050228311,0.168949772,0.461187215);
            RGB[43] = ColorRGB(0.260273973,0.547945205,0.159817352);
            RGB[44] = ColorRGB(0.698630137,0,0.100456621);
            RGB[45] = ColorRGB(0.96803653,0.748858447,0);
            RGB[46] = ColorRGB(0.757990868,0.260273973,0.479452055);
            RGB[47] = ColorRGB(0,0.502283105,0.547945205);
            RGB[48] = ColorRGB(0.908675799,0.799086758,0.748858447);
            RGB[49] = ColorRGB(0.840182648,0.461187215,0.470319635);
            RGB[50] = ColorRGB(0.748858447,0,0.150684932);
            RGB[51] = ColorRGB(0,0.488584475,0.680365297);
            RGB[52] = ColorRGB(0.328767123,0.598173516,0.680365297);
            RGB[53] = ColorRGB(1,0.780821918,0.671232877);
            RGB[54] = ColorRGB(0.748858447,0.840182648,0.757990868);
            RGB[55] = ColorRGB(0.872146119,0.461187215,0.401826484);
            RGB[56] = ColorRGB(0.940639269,0.200913242,0.150684932);
            RGB[57] = ColorRGB(0.191780822,0.630136986,0.648401826);
            RGB[58] = ColorRGB(0,0.228310502,0.269406393);
            RGB[59] = ColorRGB(0.858447489,0.831050228,0.520547945);
            RGB[60] = ColorRGB(1,0.401826484,0);
            RGB[61] = ColorRGB(1,0.630136986,0);
            RGB[62] = ColorRGB(0,0.228310502,0.200913242);
            RGB[63] = ColorRGB(0.461187215,0.579908676,0.689497717);
            RGB[64] = ColorRGB(0.858447489,0.479452055,0.278538813);
            RGB[65] = ColorRGB(0.96803653,0.671232877,0.488584475);
            RGB[66] = ColorRGB(0.780821918,0.547945205,0.360730594);
            RGB[67] = ColorRGB(0.561643836,0.360730594,0.200913242);
            RGB[68] = ColorRGB(0.808219178,0.589041096,0.452054795);
            RGB[69] = ColorRGB(0.630136986,0.337899543,0.127853881);
            RGB[70] = ColorRGB(0.840182648,0.520547945,0.360730594);
            RGB[71] = ColorRGB(0.780821918,0.698630137,0);
            RGB[72] = ColorRGB(1,0.748858447,0);
            RGB[73] = ColorRGB(0,0.630136986,0.561643836);
            RGB[74] = ColorRGB(0,0.547945205,0.461187215);
            RGB[75] = ColorRGB(0.821917808,0.538812785,0.410958904);
            RGB[76] = ColorRGB(0.98173516,0.598173516,0.452054795);
            RGB[77] = ColorRGB(0.780821918,0.561643836,0.420091324);
            RGB[78] = ColorRGB(0.789954338,0.547945205,0.420091324);
            RGB[79] = ColorRGB(0.799086758,0.561643836,0.410958904);
            RGB[80] = ColorRGB(0.479452055,0.292237443,0.150684932);
            RGB[81] = ColorRGB(0.849315068,0.547945205,0.369863014);
            RGB[82] = ColorRGB(0.721461187,0.561643836,0.091324201);
            RGB[83] = ColorRGB(0.730593607,0.698630137,0);
            RGB[84] = ColorRGB(0.251141553,0.210045662,0.141552511);
            RGB[85] = ColorRGB(0.351598174,0.630136986,0.351598174);
            RGB[86] = ColorRGB(0,0.538812785,0.310502283);
            RGB[87] = ColorRGB(0.118721461,0.251141553,0.159817352);
            RGB[88] = ColorRGB(0.228310502,0.630136986,0.429223744);
            RGB[89] = ColorRGB(0.479452055,0.611872146,0.219178082);
            RGB[90] = ColorRGB(0.200913242,0.538812785,0.100456621);
            RGB[91] = ColorRGB(0.292237443,0.662100457,0.159817352);
            RGB[92] = ColorRGB(0.789954338,0.520547945,0.168949772);
            RGB[93] = ColorRGB(0.621004566,0.589041096,0.100456621);
            RGB[94] = ColorRGB(0.648401826,0.730593607,0);
            RGB[95] = ColorRGB(0.301369863,0.168949772,0.100456621);
            break;
        } 
        //Custom color checker as default
		default:
			RGB [ i ] = GetConfig()->GetCColorsT(i);
			if (GetConfig()->m_CCMode >= MASCIOR50 && GetConfig()->m_CCMode <= CCMAXHDR)
				GetConfig()->m_bHDR100 = TRUE;
			break;
    } 

    CColor White = CMeasure::GetGray ( CMeasure::GetGrayScaleSize() - 1 );
//	CColor Black = CMeasure::GetGray ( 0 );
	CColor Black = CMeasure::GetOnOffBlack();
    double gamma=GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef);
	CColor tempColor;
	int mode = GetConfig()->m_GammaOffsetType;
	if (GetConfig()->m_colorStandard == sRGB) mode = 99;
	
	
	// Sets the generator does NOT recalc for HDR (m_bRecalc = FALSE in
	// GenerateCC24Colors: AXIS, luminance/near-black/clipping CSVs, random and
	// user sets). In modes 5/7 their stored triplets are EOTF-encoded signals,
	// not SDR-2.22 ones: plain standards send them to the wire verbatim (model
	// the raw signal below), pseudo-spaces remap them inner->transport with the
	// active EOTF (mirror GenerateCC24Colors' chain, incl. the 1=100nits ->
	// 1=10000nits /100 rescale between getL_EOTF(+5) and the -5 encoder).
	int ccm = GetConfig()->m_CCMode;
	bool nonRecalcSet = ( ccm == AXIS || ccm == CM4LUM || ccm == CM5LUM ||
		ccm == CM10LUM || ccm == CM6NB || ccm == CMDNR ||
		ccm == RANDOM250 || ccm == RANDOM500 || ccm == USER );
	bool pseudoSpace = ( GetColorReference().m_standard == UHDTV3 || GetColorReference().m_standard == UHDTV4 );
	bool rawWireHDR = nonRecalcSet && !pseudoSpace && ( mode == 5 || mode == 7 );

	// These paths produce references on the HDR-100 scale (1.0 = 10000 nits
	// after the /100 decode); tell the consumers (grid header swatches, 3D
	// viewer) to apply their * 100 display rescale.
	if ( nonRecalcSet && mode == 5 )
		GetConfig()->m_bHDR100 = TRUE;

	double r,g,b;
	if ( nonRecalcSet && pseudoSpace && ( mode == 5 || mode == 7 ) )
	{
		// mirror GenerateCC24Colors: pure PQ decode with m_TargetMaxL = 10000
		// passed explicitly (the 700-nit default would tone-clip the
		// linearization - clipping belongs to the display model, never to the
		// signal round trip); HLG linearizes to SCENE light via the
		// display-independent inverse OETF.
		if ( mode == 7 )
		{
			r = HLG_SignalToScene(RGB[i][0]);
			g = HLG_SignalToScene(RGB[i][1]);
			b = HLG_SignalToScene(RGB[i][2]);
		}
		else
		{
			r = (RGB[i][0] <= 0.0) ? 0.0 : getL_EOTF(RGB[i][0], noDataColor, noDataColor, 0.0, 0.0, 5, 94.37844, 0.0, 4000.0, 0.0, 10000.0) / 100.;
			g = (RGB[i][1] <= 0.0) ? 0.0 : getL_EOTF(RGB[i][1], noDataColor, noDataColor, 0.0, 0.0, 5, 94.37844, 0.0, 4000.0, 0.0, 10000.0) / 100.;
			b = (RGB[i][2] <= 0.0) ? 0.0 : getL_EOTF(RGB[i][2], noDataColor, noDataColor, 0.0, 0.0, 5, 94.37844, 0.0, 4000.0, 0.0, 10000.0) / 100.;
		}
	}
	else
	{
		r = pow(RGB[i][0],2.22), g = pow(RGB[i][1],2.22), b = pow(RGB[i][2],2.22);
	}

	if (GetConfig()->m_CCMode >= MASCIOR50 && GetConfig()->m_CCMode <= CCMAXHDR)
	{
		r=RGB[i][0],g=RGB[i][1],b=RGB[i][2];
		double PeakWhite = 700; //default 2017 LGs
		
		if (White.isValid())
			PeakWhite = White.GetY();

		switch (GetConfig()->m_CCMode) //Special HDR sequences
		{
			case MASCIOR50:
				{
					r = getL_EOTF(r,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) / 100.;
					g = getL_EOTF(g,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) / 100.;
					b = getL_EOTF(b,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) / 100.;
				}
			break;
			case LG54016: 
				{
					double levels[21] = {0.0,0.0033,0.0059,0.0168,0.0418,0.0636,0.0941,0.1138,
					0.1356,0.1632,0.1936,0.2319,0.2741,0.3271,0.3856,0.4588,0.5394,0.6403,0.7514,0.8905,1.0};
					r = levels[i] / 100., g=levels[i] / 100., b=levels[i] / 100.;
				break;
				}
			case LG54017: //2017 specified as PQ L normalized to 10000.
				{
					double levels1[21] = {0.0,0.00041568721,0.0010963512,0.0025700175,0.0036186520,0.0051448577,0.0059534422,0.0068068908,0.0080930827,0.0095057554,0.010926765,0.012793139,0.014811335,0.016962938,0.019784122,0.023048520,0.026571914,0.030322742,0.034906582,0.040533540,PeakWhite/10000.};
					r = levels1[i] * 10000. / PeakWhite / 100., g=levels1[i] * 10000. / PeakWhite / 100., b=levels1[i] * 10000. / PeakWhite / 100.;
				break;
				}
			case LG100017:
				{
					double levels2[21] = {0,0.00068780816,0.0015811979,0.0033531274,0.0047793624,0.0067374526,0.0080930827,0.0093174258,0.011035500,0.012668243,0.014811335,0.016799930,0.019408523,0.022187297,0.025104679,0.028928159,0.032997985,0.038327360,0.044075792,0.050192776,PeakWhite/10000.};
					r = levels2[i] * 10000. / PeakWhite / 100., g=levels2[i] * 10000. / PeakWhite / 100., b=levels2[i] * 10000. / PeakWhite / 100.;
				break;
				}
			case LG400017:
				{
					double levels3[21] = {0.,0.00090633254,0.0021676269,0.0046300845,0.0067374526,0.0092245709,0.010819026,0.012793139,0.014956038,0.017127426,0.020360505,0.023048520,0.027079017,0.031190355,0.036578259,0.042072311,0.048815980,0.055573426,0.063244199,0.063358607,PeakWhite/10000.};
					r = levels3[i] * 10000. / PeakWhite /  100., g=levels3[i] * 10000. / PeakWhite / 100., b=levels3[i] * 10000. / PeakWhite / 100.;
				break;
				}
			default:
				{
					r = getL_EOTF(r, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL, GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) / PeakWhite;
					g = getL_EOTF(g, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL, GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) / PeakWhite;
					b = getL_EOTF(b, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL, GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) / PeakWhite;
				}
			break;
		}
	}
	else
	{

		// linearize R'G'B'
		if (const_XYZ && (mode == 5 || mode == 7))
		{
			tempColor.SetRGBValue(ColorRGB(r,g,b), CColorReference((GetConfig()->m_CCMode == MASCIOR50)?UHDTV:HDTV));
			if (mode == 5)
			{
				tempColor.SetX(tempColor.GetX() / 105.95640 ); //50% reference for HDR-10
				tempColor.SetY(tempColor.GetY() / 105.95640 );
				tempColor.SetZ(tempColor.GetZ() / 105.95640 );
			}
		}
		else
			tempColor.SetRGBValue(ColorRGB(r,g,b), (GetColorReference().m_standard==UHDTV3||GetColorReference().m_standard==UHDTV4)?ContainerInnerReference(GetColorReference()):cRef);

		// UHDTV3/4: GenerateCC24Colors remaps the patch inner->transport and the
		// wire carries the transport (BT.2020) encoding, so model the wire here:
		// convert to transport RGB, then encode/quantize/decode in that space.
		ColorRGB aRGBColor = tempColor.GetRGBValue((GetColorReference().m_standard==UHDTV3||GetColorReference().m_standard==UHDTV4)?ContainerTransportReference(GetColorReference()):cRef);
		r = aRGBColor[0];
		g = aRGBColor[1];
		b = aRGBColor[2];

		// Pure-primary patches (100% saturations) can round-trip through XYZ to a
		// tiny negative channel value; pow()/EOTF below turn that into NaN, which
		// invalidates the reference (blank dE, garbage Y target). Clamp to zero.
		if (r < 0.) r = 0.;
		if (g < 0.) g = 0.;
		if (b < 0.) b = 0.;

		double qr,qg,qb;
		bool b10 = GetConfig()->GetUse10bitLevels();
		bool lim = GetConfig()->GetRGB16_235();
		if (mode == 5 || mode == 7)
		{
			// rawWireHDR (plain-standard non-recalc sets): the stored triplet
			// IS the wire signal - snap it and EOTF-decode, instead of the
			// SDR-2.22 -> EOTF re-encode chain that models the recalc'd sets.
			// (Pseudo-space non-recalc sets took the EOTF linearize above and
			// flow through the normal inner->transport remap.)
			if (rawWireHDR)
			{
				qr = RGB[i][0];
				qg = RGB[i][1];
				qb = RGB[i][2];
			}
			else
			{
				qr = getL_EOTF(r,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, -1*mode);
				qg = getL_EOTF(g,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, -1*mode);
				qb = getL_EOTF(b,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, -1*mode);
			}
			qr = SnapToVideoGrid( qr, b10, lim );
			qg = SnapToVideoGrid( qg, b10, lim );
			qb = SnapToVideoGrid( qb, b10, lim );
			r = getL_EOTF(qr,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) / (mode==5?100.:1.0);
			g = getL_EOTF(qg,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) / (mode==5?100.:1.0);
			b = getL_EOTF(qb,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) / (mode==5?100.:1.0);
		}
		// mode 99 (sRGB standard) belongs here too: the wire quantizes the
		// 2.22-encoded signal and the sensor/gray targets decode it with the
		// sRGB curve via getL_EOTF(99) - same chain GetRefSat models. It
		// previously fell through every branch, leaving CC references
		// unquantized AND 2.22-decoded (up to ~5 dE on dark AXIS steps).
		if ( mode == 6 || mode == 4 || mode == 8 || mode == 99 )
		{
			qr = (r==0)?0:pow(r, 1.0 / 2.22);
			qg = (g==0)?0:pow(g, 1.0 / 2.22);
			qb = (b==0)?0:pow(b, 1.0 / 2.22);
			qr = SnapToVideoGrid( qr, b10, lim );
			qg = SnapToVideoGrid( qg, b10, lim );
			qb = SnapToVideoGrid( qb, b10, lim );
			r=(r<=0||r>=1)?min(max(r,0),1):getL_EOTF(qr,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode);
			g=(g<=0||g>=1)?min(max(g,0),1):getL_EOTF(qg,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode);
			b=(b<=0||b>=1)?min(max(b,0),1):getL_EOTF(qb,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode);
		}
		else if ( mode < 4 )
		{
			qr = (r==0)?0:pow(r, 1.0 / 2.22);
			qg = (g==0)?0:pow(g, 1.0 / 2.22);
			qb = (b==0)?0:pow(b, 1.0 / 2.22);
			qr = SnapToVideoGrid( qr, b10, lim );
			qg = SnapToVideoGrid( qg, b10, lim );
			qb = SnapToVideoGrid( qb, b10, lim );
			r=(qr<=0||qr>=1)?min(max(qr,0),1):pow(qr, gamma);
			g=(qg<=0||qg>=1)?min(max(qg,0),1):pow(qg, gamma);
			b=(qb<=0||qb>=1)?min(max(qb,0),1):pow(qb, gamma);
		}

	}

	ccRef.ClearSpectrumLux();
	ccRef.SetRGBValue(ColorRGB(r,g,b),(GetColorReference().m_standard==UHDTV3||GetColorReference().m_standard==UHDTV4)?ContainerTransportReference(GetColorReference()):cRef);

	//Special case White redefined on Mascior disk to level 502 50.0% 92.254965 nits
	bool DVD = (GetConfig()->GetGeneratorType() == CColorHCFRConfig::enumManual);
	double level = (abs(GetConfig()->m_DiffuseL-94.0)<0.5?92.254965:GetConfig()->m_DiffuseL) / 10000.;
	if (!i && GetConfig()->m_CCMode == CPS && mode == 5 && DVD)
		ccRef.SetRGBValue(ColorRGB(level,level,level),(GetColorReference().m_standard==UHDTV3||GetColorReference().m_standard==UHDTV4)?ContainerTransportReference(GetColorReference()):cRef);
}