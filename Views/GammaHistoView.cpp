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
//	François-Xavier CHABOUD
//	Georges GALLERAND
//	Benoit SEGUIN
/////////////////////////////////////////////////////////////////////////////

// GammaHistoView.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "DataSetDoc.h"
#include "DocTempl.h"
#include "GammaHistoView.h"
#include "math.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

double Y_to_L( double val );

/////////////////////////////////////////////////////////////////////////////
// CGammaGrapher

CGammaGrapher::CGammaGrapher ()
{
	CString	Msg;

	Msg.LoadString ( IDS_GAMMA );
	m_luminanceLogGraphID = m_graphCtrl.AddGraph(RGB(255,255,0),(LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_GAMMAREFERENCE );
	m_refLogGraphID = m_graphCtrl.AddGraph(RGB(230,230,230),(LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_GAMMADATAREF );
	m_luminanceDataRefLogGraphID = m_graphCtrl.AddGraph(RGB(255,255,0),(LPSTR)(LPCSTR)Msg,1,PS_DOT); //Ki
	Msg += " (lux)";
	m_luxmeterDataRefLogGraphID = m_graphCtrl.AddGraph(RGB(255,128,0),(LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_GAMMA );
	Msg += " (lux)";
	m_luxmeterLogGraphID = m_graphCtrl.AddGraph(RGB(255,128,0),(LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_GAMMARED );
	m_redLumLogGraphID = m_graphCtrl.AddGraph(RGB(255,0,0),(LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_GAMMAGREEN );
	m_greenLumLogGraphID = m_graphCtrl.AddGraph(RGB(0,255,0),(LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_GAMMABLUE );
	m_blueLumLogGraphID = m_graphCtrl.AddGraph(RGB(70,70,255),(LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_GAMMAAVERAGE );
	m_avgLogGraphID = m_graphCtrl.AddGraph(RGB(0,255,255),(LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg += " (lux)";
	m_luxmeterAvgLogGraphID = m_graphCtrl.AddGraph(RGB(128,0,255),(LPSTR)(LPCSTR)Msg,1,PS_DOT);
	
	Msg.LoadString ( IDS_GAMMAREDDATAREF );
	m_redLumDataRefLogGraphID = m_graphCtrl.AddGraph(RGB(255,0,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT); //Ki
	Msg.LoadString ( IDS_GAMMAGREENDATAREF );
	m_greenLumDataRefLogGraphID = m_graphCtrl.AddGraph(RGB(0,255,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT); //Ki
	Msg.LoadString ( IDS_GAMMABLUEDATAREF );
	m_blueLumDataRefLogGraphID = m_graphCtrl.AddGraph(RGB(70,70,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT); //Ki
	
	m_graphCtrl.SetXAxisProps((LPSTR)(LPCSTR)GetConfig()->m_PercentGray, 10, 0, 100);
	bool isHDR = (GetConfig()->m_GammaOffsetType == 5 || GetConfig()->m_GammaOffsetType == 7);

	m_graphCtrl.SetYAxisProps(isHDR?"cd m-2":"", isHDR?1:0.1, isHDR?-100:1, isHDR?100:4);
	m_graphCtrl.SetScale(0,100,isHDR?-20:1,isHDR?20:3);
	m_graphCtrl.ReadSettings("Gamma Histo");

	m_showReference=GetConfig()->GetProfileInt("Gamma Histo","Show Reference",TRUE);
	m_showAverage=GetConfig()->GetProfileInt("Gamma Histo","Show Average",TRUE);
	m_showYLum=GetConfig()->GetProfileInt("Gamma Histo","Show Y lum",TRUE);
	m_showRedLum=GetConfig()->GetProfileInt("Gamma Histo","Show Red",FALSE);
	m_showGreenLum=GetConfig()->GetProfileInt("Gamma Histo","Show Green",FALSE);
	m_showBlueLum=GetConfig()->GetProfileInt("Gamma Histo","Show Blue",FALSE);
	m_showDataRef=GetConfig()->GetProfileInt("Gamma Histo","Show Reference Data",TRUE);	//Ki
}


// Gamma is plotted as log(y)/log(x), which is 0/0 at reference white - which is
// why the reference curve used to stop one grayscale step short of 100%. The
// limit does exist: it is the local slope of the EOTF at white, and for a pure
// power law it is exactly the gamma itself (log(x^g)/log(x) == g for any x), so
// evaluating the top point a hair below white recovers it instead of dropping
// the point. Only the math is nudged - the point is still plotted at its real
// 100% stimulus. HDR modes plot an absolute luminance delta rather than a log
// ratio, so they are left alone; so is the MEASURED curve, where Y(100%) is the
// white it is normalised against, making the ratio exactly 1 and the limit
// unrecoverable from a single sample.
static double NudgeOffWhite(double valx, bool isHDR)
{
	return ( !isHDR && valx >= 1.0 ) ? 1.0 - 1e-4 : valx;
}

// The same step off the other end of the scale, so the curve can be evaluated
// at black instead of starting at the first patch above it. HDR is untouched -
// its reference pass already starts at point 0 and feeds m_yref_abs, so nudging
// would move an HDR baseline.
static const double kBlackNudge = 1e-4;

static double NudgeOffBlack(double valx, bool isHDR)
{
	return ( !isHDR && valx <= 0.0 ) ? kBlackNudge : valx;
}

// The target EOTF at stimulus x (0..100): *pValY is its relative luminance and
// *pGamma the log(y)/log(x) the chart plots. Returns FALSE where that gamma has
// no value (x or y at zero, or a denominator of log(1)). Single definition,
// because the measured trace needs the same number at reference white as the
// reference curve does.
//
// White and Black belong to the document being plotted and are passed in rather
// than fetched here: modes >= 4 feed them to getL_EOTF, so the value differs per
// document. Passing by const reference also saves one CColor copy per call, and
// CColor copies deep-copy the attached spectrum - but only one of the two: this
// hands them straight to getL_EOTF, which still takes White and Black BY VALUE,
// so a spectrum copy per grayscale point remains. Removing that one means
// changing the signature in libHCFR/Color.h and every call site in the tree,
// which is a wider change than this file.
static BOOL ReferenceGammaAt(const CColor & White, const CColor & Black, double x, double GammaOffset, double *pGamma, double *pValY, bool bNudgeBlack = true)
{
	int		mode = GetConfig()->m_GammaOffsetType;
	bool	isHDR = (mode == 5 || mode == 7);
	double	valx, valy;

	if (GetConfig()->m_colorStandard == sRGB) mode = 99;

	if ( mode >= 4 )
	{
		valx = NudgeOffWhite(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235()), isHDR);
		if (bNudgeBlack) valx = NudgeOffBlack(valx, isHDR);
		if (mode == 5)
			valy = getL_EOTF(valx, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) / 100.;
		else
			valy = getL_EOTF(valx, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
	}
	else
	{
		valx = NudgeOffWhite((GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235())+GammaOffset)/(1.0+GammaOffset), isHDR);
		if (bNudgeBlack) valx = NudgeOffBlack(valx, isHDR);
		valy = pow(valx, GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef));
	}

	*pValY = valy;

	// valx == 1.0 is the log(1) denominator, not just an undefined numerator:
	// the nudge keeps SDR off it, but HDR skips the nudge, and dividing there
	// would hand back an infinity that looks like a valid gamma.
	if ( !(valy > 0 && valx > 0 && valx != 1.0 && valy != 1.0) )
	{
		*pGamma = 0.0;
		return FALSE;
	}

	*pGamma = log(valy)/log(valx);
	return TRUE;
}

// The value the scale carries at BLACK.
//
// This chart plots gamma relative to the display's own black and white, and the
// transfer function is anchored at both of those by construction: 0% stimulus
// puts out the display's black, 100% puts out its white. Both endpoints are
// therefore 0/0, and neither carries a measurement - they carry the anchor.
//
// The two ends are NOT symmetric, and it is worth being exact about that. At
// white the limit is the target's exponent and it is exact: log(x^g)/log(x) == g
// for any x, so evaluating a hair below white recovers g whatever the nudge. At
// black no such clean limit exists for every target, and this function does not
// pretend to find one.
//
// What it does is take out the black-elevation term. The raw log(y)/log(x)
// divides by white without removing black: a real black is a finite fraction of
// white while log(x) runs to -infinity, so the ratio collapses toward 0 (~0.8 at
// 1655:1) and lands off the bottom of any gamma axis, on every display ever
// made. Removing the target's own floor first - (y(e) - y(0)) / (1 - y(0)) -
// leaves the SHAPE of the target curve near black instead of its contrast ratio.
//
// What that shape evaluates to depends on the target:
//
//   Pure power (modes < 4, and any mode >= 4 target of the form
//   y = y0 + (1-y0)*x^g): the removal reduces the curve to x^g exactly, so the
//   answer is g, independent of e. This is the case the "gamma at black is 2.2
//   for a 2.2 display" intuition is true for, and only this one.
//
//   BT.1886 with split > 0: getL_EOTF case 4 is
//   y = (a*(x+b)^g + minL*(1-split/100)) / (maxL + minL*(1-split/100)), and
//   a*b^g = offset = split/100 * minL, so y(0) = minL / (maxL +
//   minL*(1-split/100)) - the target's floor IS the measured black. At the
//   default Split of 100 that is exactly minL/maxL and the curve reduces to
//   a*(x+b)^g / maxL; at a lower split the floor sits slightly below minL/maxL
//   and the endpoint slightly above, so the value is Split-dependent too (1.361
//   at split 100, 1.403 at split 50, on the panel below). Either way
//   y(e) - y(0) is LINEAR in e, so log/log tends to 1, not to g, and the value
//   depends on e. At the e below, a 1220:1 panel on a g 2.4 target at the
//   default split reads 1.361 (1.240 at e = 1e-6). That is not the exponent -
//   and BT.1886 does not reach its exponent anywhere: the same reference curve
//   runs 1.455 at 1%, 2.001 at 10%, 2.225 at 50%, 2.276 at white. Only
//   split = 0 returns a flat 2.4.
//
//   sRGB, L*: y(0) == 0, so there is no floor and the removal is arithmetically
//   a no-op. Their linear toe makes y proportional to x near black, so this is
//   again the target's own local log/log slope - 1.239 for L* at the e below -
//   and again e-dependent.
//
// So on every target but the pure powers this is the target curve SAMPLED at e,
// not a limit; e is what fixes the sampled value, and the white endpoint has no
// equivalent freedom. That is acceptable because of what the chart needs from
// the point: on all three it lands on the reference curve drawn beside it and
// continues it, which bench measurement confirms is also where the measured
// trace extrapolates. It is an anchor, not a reading.
static BOOL ReferenceGammaAtBlack(const CColor & White, const CColor & Black, double GammaOffset, double *pGamma)
{
	double	gNudged, valyNudged, gZero, valyZero;

	if ( !ReferenceGammaAt(White, Black, 0.0, GammaOffset, &gNudged, &valyNudged) )
		return FALSE;

	// The target's luminance at a stimulus of literally zero: its floor, if it
	// has one. Return value ignored on purpose - a target with no floor gives
	// y(0) = 0 and fails the log guard, which is exactly the "nothing to remove"
	// case handled below.
	ReferenceGammaAt(White, Black, 0.0, GammaOffset, &gZero, &valyZero, false);

	double	num = valyNudged - valyZero;
	double	den = 1.0 - valyZero;

	// Nothing to remove: a pure power target, or an offset target whose stimulus
	// at 0% is already nonzero so the nudge never fired. The plain value is the
	// exponent already.
	if ( !(num > 0.0 && den > 0.0) )
	{
		*pGamma = gNudged;
		return TRUE;
	}

	*pGamma = log(num/den)/log(kBlackNudge);
	return TRUE;
}

void CGammaGrapher::UpdateGraph ( CDataSetDoc * pDoc )
{
	BOOL	bDataPresent = FALSE;
	BOOL	bIRE = pDoc->GetMeasure()->m_bIREScaleMode;
	double GammaOffset,GammaOpt,RefGammaOffset,RefGammaOpt,LuxGammaOffset,LuxGammaOpt,RefLuxGammaOffset,RefLuxGammaOpt;
	m_yref_abs.clear();

	bool isHDR = (GetConfig()->m_GammaOffsetType == 5 || GetConfig()->m_GammaOffsetType == 7);
	m_graphCtrl.SetXAxisProps(bIRE?"IRE":(LPSTR)(LPCSTR)GetConfig()->m_PercentGray, 10, 0, 100);
	m_graphCtrl.SetYAxisProps(isHDR?"cd m-2":"", isHDR?1:0.1, isHDR?-100:1, isHDR?100:4);
	CString strGamma;
	strGamma.LoadString ( isHDR ? IDS_GRAPH_DELTALUMREFMEAS : IDS_GRAPH_REFMEASGAMMA );
	m_graphCtrl.m_graphArray[2].m_Title=strGamma;
	strGamma.LoadString ( isHDR ? IDS_GRAPH_DELTALUMREF : IDS_GAMMAREFERENCE );
	m_graphCtrl.m_graphArray[1].m_Title=strGamma;
	strGamma.LoadString ( isHDR ? IDS_GRAPH_DELTALUM : IDS_GAMMA );
	m_graphCtrl.m_graphArray[0].m_Title=strGamma;
	strGamma.LoadString ( isHDR ? IDS_GRAPH_DELTALUM : IDS_GRAPH_EOTFGAMMA );
	m_graphCtrl.m_graphArray[0].p_Title=strGamma;

	CDataSetDoc *pDataRef = GetDataRef();
	int size=pDoc->GetMeasure()->GetGrayScaleSize();

	if ( pDataRef )
	{
		// Check if data reference is comparable
		if ( pDataRef->GetMeasure()->GetGrayScaleSize() != size || pDataRef->GetMeasure()->m_bIREScaleMode != bIRE )
		{
			// Cannot use data reference
			pDataRef = NULL;
		}
	}

	if (pDoc->GetMeasure()->GetGray(0).isValid())
		bDataPresent = TRUE;
	if (pDataRef && !pDataRef->GetMeasure()->GetGray(0).isValid())
	    	pDataRef = NULL;

	m_graphCtrl.ClearGraph(m_refLogGraphID);
	m_graphCtrl.ClearGraph(m_avgLogGraphID);
	m_graphCtrl.ClearGraph(m_luxmeterAvgLogGraphID);

	
	// Compute offset
	pDoc->ComputeGammaAndOffset(&GammaOpt, &GammaOffset, 1,1,size, false);
	pDoc->ComputeGammaAndOffset(&LuxGammaOpt, &LuxGammaOffset, 2,1,size, false);

//	GammaOpt = floor(GammaOpt * 100) / 100;

	if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
	{
		// The comparison document's own fit: these offsets are handed to
		// AddPointtoLumGraph for the pDataRef traces, so fitting them on pDoc
		// described the wrong document (rgbhistoview.cpp does it on pDataRef).
		pDataRef->ComputeGammaAndOffset(&RefGammaOpt, &RefGammaOffset, 1,1,size,false);
		pDataRef->ComputeGammaAndOffset(&RefLuxGammaOpt, &RefLuxGammaOffset, 2,1,size,false);
	}

	if (m_showReference && m_refLogGraphID != -1 && size > 0 || (m_refLogGraphID != -1 && (GetConfig()->m_GammaOffsetType == 5 || GetConfig()->m_GammaOffsetType == 7 )))
	{	
		// Both ends of the log scale are singular - log(0) at black, log(1) at
		// white - and NudgeOffBlack/NudgeOffWhite are what let the curve be
		// evaluated there, so the loop now runs the full grayscale in both modes.
		// That also satisfies HIST-002 on its own: the HDR delta path indexes
		// m_yref_abs by grayscale point, and every point pushes exactly one slot,
		// so index and point stay in step without a placeholder.

		// Fetched once for the whole curve, not once per point - see ReferenceGammaAt.
		CColor RefWhite = pDoc -> GetMeasure () -> GetOnOffWhite();
		CColor RefBlack = pDoc -> GetMeasure () -> GetOnOffBlack();

		for (int i=0; i<size; i++)
		{
			double x, gamma, valy;
			x = pDoc->GetMeasure()->GetGrayPercent ( i, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235() );
			// Point 0 takes the anchor value, so the reference curve and the
			// measured traces meet at the left edge the way they do at white.
			BOOL bHasGamma = ( i == 0 && !isHDR )
				? ( ReferenceGammaAt(RefWhite, RefBlack, x, GammaOffset, &gamma, &valy)
					&& ReferenceGammaAtBlack(RefWhite, RefBlack, GammaOffset, &gamma) )
				: ReferenceGammaAt(RefWhite, RefBlack, x, GammaOffset, &gamma, &valy);

			m_yref_abs.push_back(valy);

			if (isHDR)
				m_graphCtrl.AddPoint(m_refLogGraphID, x, 0);
			else if (bHasGamma)
				m_graphCtrl.AddPoint(m_refLogGraphID, x, gamma);
		}
	}
	

	if (m_showAverage && m_avgLogGraphID != -1 && size > 0 && bDataPresent && !(GetConfig()->m_GammaOffsetType == 5 || GetConfig()->m_GammaOffsetType == 7 ))
	{	
		// Le calcul de la moyenne des gamma et la représentation en échelle log 
		// ne se fait plus avec l'échelle des x = % de blanc mais avec la formule : 
		// (x + offset) / (1+offset) 
		for (int i=0; i<size; i++)
			m_graphCtrl.AddPoint(m_avgLogGraphID, pDoc->GetMeasure()->GetGrayPercent ( i, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235() ), GammaOpt);
	}

	if ( GetConfig () -> m_nLuminanceCurveMode == 2 )
	{
		if (m_showAverage && m_luxmeterAvgLogGraphID != -1 && size > 0 && bDataPresent && pDoc->GetMeasure()->GetGray(0).HasLuxValue() )
		{	
			// Le calcul de la moyenne des gamma et la représentation en échelle log 
			// ne se fait plus avec l'échelle des x = % de blanc mais avec la formule : 
			// (x + offset) / (1+offset) 
			for (int i=0; i<size; i++)
				m_graphCtrl.AddPoint(m_luxmeterAvgLogGraphID, pDoc->GetMeasure()->GetGrayPercent ( i, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235() ), LuxGammaOpt);
		}
	}

	m_graphCtrl.ClearGraph(m_redLumLogGraphID);
	m_graphCtrl.ClearGraph(m_redLumDataRefLogGraphID); //Ki
	if (m_showRedLum && m_redLumLogGraphID != -1 && size > 0 && !(GetConfig()->m_GammaOffsetType == 5 || GetConfig()->m_GammaOffsetType == 7 ))
	{
		for (int i=0; i<size; i++)
		{
			if (bDataPresent)
				AddPointtoLumGraph(0,0,size,i,GammaOffset,pDoc,m_redLumLogGraphID,bIRE);
			if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
				AddPointtoLumGraph(0,0,size,i,RefGammaOffset,pDataRef,m_redLumDataRefLogGraphID,bIRE);
		}
	}

	m_graphCtrl.ClearGraph(m_greenLumLogGraphID);
	m_graphCtrl.ClearGraph(m_greenLumDataRefLogGraphID); //Ki
	if (m_showGreenLum && m_greenLumLogGraphID != -1 && size > 0 && !(GetConfig()->m_GammaOffsetType == 5 || GetConfig()->m_GammaOffsetType == 7 ))
	{
		for (int i=0; i<size; i++)
		{
			if (bDataPresent)
				AddPointtoLumGraph(0,1,size,i,GammaOffset,pDoc,m_greenLumLogGraphID,bIRE);
			if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
				AddPointtoLumGraph(0,1,size,i,RefGammaOffset,pDataRef,m_greenLumDataRefLogGraphID,bIRE);
		}
	}

	m_graphCtrl.ClearGraph(m_blueLumLogGraphID);
	m_graphCtrl.ClearGraph(m_blueLumDataRefLogGraphID); //Ki
	if (m_showBlueLum && m_blueLumLogGraphID != -1 && size > 0 && !(GetConfig()->m_GammaOffsetType == 5 || GetConfig()->m_GammaOffsetType == 7 )) 
	{
		for (int i=0; i<size; i++) 
		{
			if (bDataPresent)
				AddPointtoLumGraph(0,2,size,i,GammaOffset,pDoc,m_blueLumLogGraphID,bIRE);
			if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
				AddPointtoLumGraph(0,2,size,i,RefGammaOffset,pDataRef,m_blueLumDataRefLogGraphID,bIRE);
		}
	}

	m_graphCtrl.ClearGraph(m_luminanceLogGraphID);
	m_graphCtrl.ClearGraph(m_luminanceDataRefLogGraphID); //Ki
	if (m_showYLum && m_luminanceLogGraphID != -1 && size > 0)	
	{
		for (int i=0; i<size; i++) 
		{
			if (bDataPresent)
				AddPointtoLumGraph(1,1,size,i,GammaOffset,pDoc,m_luminanceLogGraphID,bIRE);
			if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
				AddPointtoLumGraph(1,1,size,i,RefGammaOffset,pDataRef,m_luminanceDataRefLogGraphID,bIRE);
		}
	}

	m_graphCtrl.ClearGraph(m_luxmeterLogGraphID);
	m_graphCtrl.ClearGraph(m_luxmeterDataRefLogGraphID);
	if ( GetConfig () -> m_nLuminanceCurveMode == 2 )
	{
		if (m_showYLum && m_luxmeterLogGraphID != -1 && size > 0)	
		{
			for (int i=0; i<size; i++) 
			{
				if (bDataPresent)
				{
					if (pDoc->GetMeasure()->GetGray(0).HasLuxValue())
						AddPointtoLumGraph(2,1,size,i,LuxGammaOffset,pDoc,m_luxmeterLogGraphID,bIRE);
				}
				
				if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
				{
					if (pDataRef->GetMeasure()->GetGray(0).HasLuxValue())
						AddPointtoLumGraph(2,1,size,i,RefLuxGammaOffset,pDataRef,m_luxmeterDataRefLogGraphID,bIRE);
				}
			}
		}
	}
}

/*
ColorSpace : 0 : RGB, 1 : Luminance or Lux, 2 : Lux
ColorIndex : 0 : R, 1 : G, 2 : B
Size : Total number of points
PointIndex : Point index on graph
*pDataSet : pointer on data set to display
GraphID : graph ID for drawing
LogGraphID : graph ID for log drawing, -1 if no Log drawing
*/

void CGammaGrapher::AddPointtoLumGraph(int ColorSpace,int ColorIndex,int Size,int PointIndex,double GammaOffset,CDataSetDoc *pDataSet,long GraphID, BOOL bIRE)
{
	double blacklvl, whitelvl;
	double colorlevel;
	char	szBuf [ 64 ];
	LPCSTR	lpMsg = NULL;

	if (pDataSet->GetMeasure()->GetGray(PointIndex).isValid())
	{
		pDataSet->GetMeasure()->SetGray(Size-1, pDataSet->GetMeasure()->GetOnOffWhite());
	if (ColorSpace == 0) 
	{
		blacklvl=pDataSet->GetMeasure()->GetGray(0).GetRGBValue((GetColorReference()))[ColorIndex];
		whitelvl=pDataSet->GetMeasure()->GetGray(Size-1).GetRGBValue((GetColorReference()))[ColorIndex];
		colorlevel=pDataSet->GetMeasure()->GetGray(PointIndex).GetRGBValue((GetColorReference()))[ColorIndex];
		if (GetConfig()->m_GammaOffsetType == 5)
			whitelvl =  10000.;
	}
	else if (ColorSpace == 1) 
	{
		blacklvl=pDataSet->GetMeasure()->GetGray(0).GetLuxOrLumaValue(GetConfig () -> m_nLuminanceCurveMode);
		whitelvl=pDataSet->GetMeasure()->GetGray(Size-1).GetLuxOrLumaValue(GetConfig () -> m_nLuminanceCurveMode);
		colorlevel=pDataSet->GetMeasure()->GetGray(PointIndex).GetLuxOrLumaValue(GetConfig () -> m_nLuminanceCurveMode);
		if (GetConfig()->m_GammaOffsetType == 5)
			whitelvl =  10000.;
	}
	else 
	{
		blacklvl=pDataSet->GetMeasure()->GetGray(0).GetLuxValue();
		whitelvl=pDataSet->GetMeasure()->GetGray(Size-1).GetLuxValue();
		colorlevel=pDataSet->GetMeasure()->GetGray(PointIndex).GetLuxValue();
	}

	if(GetConfig() -> m_GammaOffsetType == 1 )
	{
		colorlevel-=blacklvl;
		whitelvl-=blacklvl;
		blacklvl = 0;
	}

	if ( pDataSet->GetMeasure()->GetGray(PointIndex).HasLuxValue () )
	{
		if ( GetConfig () ->m_bUseImperialUnits )
			sprintf ( szBuf, "%.5g Ft-cd", pDataSet->GetMeasure()->GetGray(PointIndex).GetLuxValue() * 0.0929 );
		else
			sprintf ( szBuf, "%.5g Lux", pDataSet->GetMeasure()->GetGray(PointIndex).GetLuxValue() );
		
		lpMsg = szBuf;
	}
	
	// Le calcul de la moyenne des gamma et la représentation en échelle log 
	// ne se fait plus avec l'échelle des x = % de blanc mais avec la formule : 
	// (x + offset) / (1+offset) 

	if (GraphID != -1)
	{
		bool isHDR = (GetConfig()->m_GammaOffsetType == 5 || GetConfig()->m_GammaOffsetType == 7);

		double x = pDataSet->GetMeasure()->GetGrayPercent ( PointIndex, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235() );

		double valxprime=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235())+GammaOffset)/(1.0+GammaOffset);

		if (isHDR)
			m_graphCtrl.AddPoint(GraphID, x, colorlevel - whitelvl * (PointIndex >= 0 && PointIndex < (int)m_yref_abs.size() ? m_yref_abs[PointIndex] : 0.0), lpMsg);
		else if (PointIndex == 0)
		{
			// Black, the anchor at the other end - see ReferenceGammaAtBlack.
			// 0% stimulus puts out the display's black by construction, exactly
			// as 100% puts out its white, so the point sits on the target for
			// the same reason the white one does. The raw log ratio here would
			// be ~0.8, which is a black-elevation figure wearing a gamma label
			// and off the bottom of the axis on every display.
			//
			// A raised black still shows: it lands on the 5% and 10% points,
			// where the stimulus is big enough for the ratio to mean something.
			double gammaBlack;
			if ( ReferenceGammaAtBlack(pDataSet->GetMeasure()->GetOnOffWhite(), pDataSet->GetMeasure()->GetOnOffBlack(), GammaOffset, &gammaBlack) )
				m_graphCtrl.AddPoint(GraphID, x, gammaBlack, lpMsg);
		}
		else if (PointIndex == Size-1 && colorlevel > 0.0001)
		{
			// The top patch IS the white every other point is divided by, so its
			// own log(y)/log(x) is 0/0 and the trace used to stop one patch short
			// of the axis end. Its relative luminance and the target's are both
			// 1.0 there - the deviation at white is identically zero for any
			// display - so the point belongs on the target value, which is what
			// the Luminance view shows at 100% too. The tooltip still carries the
			// patch's own measured values.
			//
			// The target comes from THIS trace's document: in modes >= 4 it is a
			// function of that document's own white and black. Same colorlevel
			// guard as the points below - a top patch that read no light gets no
			// point rather than one sitting exactly on target.
			double gammaWhite, valyWhite;
			if ( ReferenceGammaAt(pDataSet->GetMeasure()->GetOnOffWhite(), pDataSet->GetMeasure()->GetOnOffBlack(), x, GammaOffset, &gammaWhite, &valyWhite) )
				m_graphCtrl.AddPoint(GraphID, x, gammaWhite, lpMsg);
		}
		else if (colorlevel > 0.0001)	// log scale is not valid for negative values
			m_graphCtrl.AddPoint(GraphID, x, log((colorlevel)/whitelvl)/log(valxprime), lpMsg);
	}
	}
}


/////////////////////////////////////////////////////////////////////////////
// CGammaHistoView

IMPLEMENT_DYNCREATE(CGammaHistoView, CSavingView)

CGammaHistoView::CGammaHistoView()
	: CSavingView()
{
	//{{AFX_DATA_INIT(CGammaHistoView)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}
 
CGammaHistoView::~CGammaHistoView()
{
}
 
BEGIN_MESSAGE_MAP(CGammaHistoView, CSavingView)
	//{{AFX_MSG_MAP(CGammaHistoView)
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_LUM_GRAPH_BLUELUM, OnLumGraphBlueLum)
	ON_COMMAND(IDM_LUM_GRAPH_GREENLUM, OnLumGraphGreenLum)
	ON_COMMAND(IDM_LUM_GRAPH_REDLUM, OnLumGraphRedLum)
	ON_COMMAND(IDM_LUM_GRAPH_SHOWREF, OnLumGraphShowRef)
	ON_COMMAND(IDM_LUM_GRAPH_SHOW_AVG, OnLumGraphShowAvg)
	ON_COMMAND(IDM_LUM_GRAPH_DATAREF, OnLumGraphShowDataRef)	//Ki
	ON_COMMAND(IDM_LUM_GRAPH_YLUM, OnLumGraphYLum)
	ON_COMMAND(IDM_GRAPH_SETTINGS, OnGraphSettings)
	ON_COMMAND(IDM_GRAPH_X_SCALE_FIT, OnGraphXScaleFit)
	ON_COMMAND(IDM_GRAPH_X_ZOOM_IN, OnGraphXZoomIn)
	ON_COMMAND(IDM_GRAPH_X_ZOOM_OUT, OnGraphXZoomOut)
	ON_COMMAND(IDM_GRAPH_X_SHIFT_LEFT, OnGraphXShiftLeft)
	ON_COMMAND(IDM_GRAPH_X_SHIFT_RIGHT, OnGraphXShiftRight)
	ON_COMMAND(IDM_GRAPH_X_SCALE1, OnGraphXScale1)
	ON_COMMAND(IDM_GRAPH_X_SCALE2, OnGraphXScale2)
	ON_COMMAND(IDM_GRAPH_Y_SHIFT_BOTTOM, OnGraphYShiftBottom)
	ON_COMMAND(IDM_GRAPH_Y_SHIFT_TOP, OnGraphYShiftTop)
	ON_COMMAND(IDM_GRAPH_Y_ZOOM_IN, OnGraphYZoomIn)
	ON_COMMAND(IDM_GRAPH_Y_ZOOM_OUT, OnGraphYZoomOut)
	ON_COMMAND(IDM_GAMMA_GRAPH_Y_SCALE1, OnGammaGraphYScale1)
	ON_COMMAND(IDM_GRAPH_SCALE_FIT, OnGraphScaleFit)
	ON_COMMAND(IDM_GRAPH_Y_SCALE_FIT, OnGraphYScaleFit)
	ON_COMMAND(IDM_GRAPH_SCALE_CUSTOM, OnGraphScaleCustom)
	ON_COMMAND(IDM_GRAPH_SAVE, OnGraphSave)
	ON_COMMAND(IDM_HELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGammaHistoView diagnostics

#ifdef _DEBUG
void CGammaHistoView::AssertValid() const
{
	CSavingView::AssertValid();
}

void CGammaHistoView::Dump(CDumpContext& dc) const
{
	CSavingView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CGammaHistoView message handlers

void CGammaHistoView::OnInitialUpdate() 
{
	CSavingView::OnInitialUpdate();

	CRect rect;
	GetClientRect(&rect);	// fill entire window

	m_Grapher.m_graphCtrl.Create(_T("Graph Window"), rect, this, IDC_LUMINANCEHISTOLOG_GRAPH);

	OnUpdate(NULL,UPD_EVERYTHING,NULL);
}

void CGammaHistoView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
	// Update this view only when we are concerned
	if ( lHint == UPD_EVERYTHING || lHint == UPD_GRAYSCALEANDCOLORS || lHint == UPD_GRAYSCALE || lHint == UPD_DATAREFDOC || lHint == UPD_REFERENCEDATA  || lHint == UPD_FREEMEASUREAPPENDED || (lHint >= UPD_REALTIME && lHint != UPD_DISPLAYPROFILE && lHint != UPD_REALTIME + 13))
	{
		m_Grapher.UpdateGraph ( GetDocument () );
		RedrawWindow( NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW );
	}
}

DWORD CGammaHistoView::GetUserInfo ()
{
	return	( ( m_Grapher.m_showReference	& 0x0001 )	<< 0 )
		  + ( ( m_Grapher.m_showAverage		& 0x0001 )	<< 1 )
		  + ( ( m_Grapher.m_showYLum		& 0x0001 )	<< 2 )
		  + ( ( m_Grapher.m_showRedLum		& 0x0001 )	<< 3 )
		  + ( ( m_Grapher.m_showGreenLum	& 0x0001 )	<< 4 )
		  + ( ( m_Grapher.m_showBlueLum		& 0x0001 )	<< 5 )
		  + ( ( m_Grapher.m_showDataRef		& 0x0001 )	<< 6 );
}

void CGammaHistoView::SetUserInfo ( DWORD dwUserInfo )
{
	m_Grapher.m_showReference	= ( dwUserInfo >> 0 ) & 0x0001;
	m_Grapher.m_showAverage		= ( dwUserInfo >> 1 ) & 0x0001;
	m_Grapher.m_showYLum		= ( dwUserInfo >> 2 ) & 0x0001;
	m_Grapher.m_showRedLum		= ( dwUserInfo >> 3 ) & 0x0001;
	m_Grapher.m_showGreenLum	= ( dwUserInfo >> 4 ) & 0x0001;
	m_Grapher.m_showBlueLum		= ( dwUserInfo >> 5 ) & 0x0001;
	m_Grapher.m_showDataRef		= ( dwUserInfo >> 6 ) & 0x0001;
}

void CGammaHistoView::OnSize(UINT nType, int cx, int cy) 
{
	CSavingView::OnSize(nType, cx, cy);
	
	if(IsWindow(m_Grapher.m_graphCtrl.m_hWnd))
		m_Grapher.m_graphCtrl.MoveWindow(0,0,cx,cy);
}

void CGammaHistoView::OnDraw(CDC* pDC) 
{
	// TODO: Add your specialized code here and/or call the base class	
}

BOOL CGammaHistoView::OnEraseBkgnd(CDC* pDC) 
{
	return TRUE;
}


void CGammaHistoView::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	// load and display popup menu
	CNewMenu menu;
	menu.LoadMenu(IDR_GAMMA_GRAPH_MENU);
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup);
	
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_SHOWREF, m_Grapher.m_showReference ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
 	pPopup->CheckMenuItem(IDM_LUM_GRAPH_DATAREF, m_Grapher.m_showDataRef ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND); //Ki
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_YLUM, m_Grapher.m_showYLum ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);

	if (GetConfig()->m_GammaOffsetType == 5 || GetConfig()->m_GammaOffsetType == 7)
	{
		pPopup->EnableMenuItem(IDM_LUM_GRAPH_SHOW_AVG, MF_DISABLED | MF_GRAYED);
		pPopup->EnableMenuItem(IDM_LUM_GRAPH_REDLUM, MF_DISABLED | MF_GRAYED);
		pPopup->EnableMenuItem(IDM_LUM_GRAPH_GREENLUM, MF_DISABLED | MF_GRAYED);
		pPopup->EnableMenuItem(IDM_LUM_GRAPH_BLUELUM, MF_DISABLED | MF_GRAYED);
	} else
	{
		pPopup->CheckMenuItem(IDM_LUM_GRAPH_SHOW_AVG, m_Grapher.m_showAverage ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
		pPopup->CheckMenuItem(IDM_LUM_GRAPH_REDLUM, m_Grapher.m_showRedLum ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
		pPopup->CheckMenuItem(IDM_LUM_GRAPH_GREENLUM, m_Grapher.m_showGreenLum ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
		pPopup->CheckMenuItem(IDM_LUM_GRAPH_BLUELUM, m_Grapher.m_showBlueLum ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
	}

	pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL, point.x, point.y, this);
}

void CGammaHistoView::OnLumGraphShowRef() 
{
	m_Grapher.m_showReference = !m_Grapher.m_showReference;
	GetConfig()->WriteProfileInt("Gamma Histo","Show Reference",m_Grapher.m_showReference);
	OnUpdate(NULL,NULL,NULL);
}

void CGammaHistoView::OnLumGraphShowDataRef()  //Ki
{
	m_Grapher.m_showDataRef = !m_Grapher.m_showDataRef;
	GetConfig()->WriteProfileInt("Gamma Histo","Show Reference Data",m_Grapher.m_showDataRef);
	OnUpdate(NULL,NULL,NULL);
}

void CGammaHistoView::OnLumGraphShowAvg() 
{
	m_Grapher.m_showAverage = !m_Grapher.m_showAverage;
	GetConfig()->WriteProfileInt("Gamma Histo","Show Average",m_Grapher.m_showAverage);
	OnUpdate(NULL,NULL,NULL);
}

void CGammaHistoView::OnLumGraphYLum() 
{
	m_Grapher.m_showYLum = !m_Grapher.m_showYLum;
	GetConfig()->WriteProfileInt("Gamma Histo","Show Y lum",m_Grapher.m_showYLum);
	OnUpdate(NULL,NULL,NULL);
}

void CGammaHistoView::OnLumGraphRedLum() 
{
	m_Grapher.m_showRedLum = !m_Grapher.m_showRedLum;
	GetConfig()->WriteProfileInt("Gamma Histo","Show Red",m_Grapher.m_showRedLum);
	OnUpdate(NULL,NULL,NULL);
}

void CGammaHistoView::OnLumGraphGreenLum() 
{
	m_Grapher.m_showGreenLum = !m_Grapher.m_showGreenLum;
	GetConfig()->WriteProfileInt("Gamma Histo","Show Green",m_Grapher.m_showGreenLum);
	OnUpdate(NULL,NULL,NULL);
}

void CGammaHistoView::OnLumGraphBlueLum() 
{
	m_Grapher.m_showBlueLum = !m_Grapher.m_showBlueLum;
	GetConfig()->WriteProfileInt("Gamma Histo","Show Blue",m_Grapher.m_showBlueLum);
	OnUpdate(NULL,NULL,NULL);
}

void CGammaHistoView::OnGraphSave() 
{
	m_Grapher.m_graphCtrl.SaveGraphs();
}

void CGammaHistoView::OnGraphSettings() 
{
	// Add log graphs to first graph control to allow setting change 
	m_Grapher.m_graphCtrl.ChangeSettings();

	OnUpdate(NULL,NULL,NULL);
}

void CGammaHistoView::OnGraphScaleCustom() 
{
	m_Grapher.m_graphCtrl.ChangeScale();
	m_Grapher.m_graphCtrl.WriteSettings("Gamma Histo");
}

void CGammaHistoView::OnGraphScaleFit() 
{
	m_Grapher.m_graphCtrl.FitXScale(TRUE);
	bool isHDR = (GetConfig()->m_GammaOffsetType == 5 || GetConfig()->m_GammaOffsetType == 7);
	m_Grapher.m_graphCtrl.FitYScale(TRUE,isHDR?1:0.1,true);
	m_Grapher.m_graphCtrl.WriteSettings("Gamma Histo");
	Invalidate(FALSE);
}

void CGammaHistoView::OnGammaGraphYScale1() 
{
	bool isHDR = (GetConfig()->m_GammaOffsetType == 5 || GetConfig()->m_GammaOffsetType == 7);
	m_Grapher.m_graphCtrl.SetYScale(isHDR?-10:1,isHDR?10:3);
	m_Grapher.m_graphCtrl.SetYAxisProps(isHDR?" cd m-2 ":"", isHDR?1:0.1, isHDR?-100:1, isHDR?100:4);
	m_Grapher.m_graphCtrl.WriteSettings("Gamma Histo");
	Invalidate(FALSE);
}

void CGammaHistoView::OnGraphYScaleFit() 
{
	bool isHDR = (GetConfig()->m_GammaOffsetType == 5 || GetConfig()->m_GammaOffsetType == 7);
	m_Grapher.m_graphCtrl.FitYScale(TRUE,isHDR?1:0.1, true);
	m_Grapher.m_graphCtrl.SetYAxisProps(isHDR?" cd m-2 ":"", isHDR?1:0.1, isHDR?-100:1, isHDR?100:4);
	m_Grapher.m_graphCtrl.WriteSettings("Gamma Histo");
	Invalidate(FALSE);
}

void CGammaHistoView::OnGraphYShiftBottom() 
{
	m_Grapher.m_graphCtrl.ShiftYScale(0.1);
	Invalidate(FALSE);
}

void CGammaHistoView::OnGraphYShiftTop() 
{
	m_Grapher.m_graphCtrl.ShiftYScale(-0.1);
	Invalidate(FALSE);
}

void CGammaHistoView::OnGraphYZoomIn() 
{
	m_Grapher.m_graphCtrl.GrowYScale(0.1,-0.1);
	Invalidate(FALSE);
}

void CGammaHistoView::OnGraphYZoomOut() 
{
	m_Grapher.m_graphCtrl.GrowYScale(-0.1,0.1);
	Invalidate(FALSE);
}

void CGammaHistoView::OnGraphXScale1() 
{
	m_Grapher.m_graphCtrl.SetXScale(0,100);
	m_Grapher.m_graphCtrl.WriteSettings("Gamma Histo");
	Invalidate(FALSE);
}

void CGammaHistoView::OnGraphXScale2() 
{
	m_Grapher.m_graphCtrl.SetXScale(20,100);
	m_Grapher.m_graphCtrl.WriteSettings("Gamma Histo");
	Invalidate(FALSE);
}

void CGammaHistoView::OnGraphXScaleFit() 
{
	m_Grapher.m_graphCtrl.FitXScale(TRUE);
	m_Grapher.m_graphCtrl.WriteSettings("Gamma Histo");
	Invalidate(FALSE);
}

void CGammaHistoView::OnGraphXZoomIn() 
{
	m_Grapher.m_graphCtrl.GrowXScale(+10,-10);
	Invalidate(FALSE);
}

void CGammaHistoView::OnGraphXZoomOut() 
{
	m_Grapher.m_graphCtrl.GrowXScale(-10,+10);
	Invalidate(FALSE);
}

void CGammaHistoView::OnGraphXShiftLeft() 
{
	m_Grapher.m_graphCtrl.ShiftXScale(-10);
	Invalidate(FALSE);
}

void CGammaHistoView::OnGraphXShiftRight() 
{
	m_Grapher.m_graphCtrl.ShiftXScale(+10);
	Invalidate(FALSE);
}


void CGammaHistoView::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_GAMMA, NULL );
}

