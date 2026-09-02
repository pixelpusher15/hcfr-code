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
//	Benoit SEGUIN
/////////////////////////////////////////////////////////////////////////////

// RGBHistoView.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "DataSetDoc.h"
#include "DocTempl.h"
#include "RGBHistoView.h"

#include <math.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CRGBGrapher

CRGBGrapher::CRGBGrapher ()
{
	CString		Msg;

	m_showReference=GetConfig()->GetProfileInt("RGB Histo","Show Reference",TRUE);
	m_showDeltaE=GetConfig()->GetProfileInt("RGB Histo","Show Delta E",TRUE);
	m_showDataRef=GetConfig()->GetProfileInt("RGB Histo","Show Reference Data",TRUE);	//Ki

	Msg.LoadString ( IDS_RGBREFERENCE );
	m_refGraphID = m_graphCtrl.AddGraph(RGB(230,230,230), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_RGBLEVELRED );
	m_redGraphID = m_graphCtrl.AddGraph(RGB(255,0,0), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_RGBLEVELGREEN );
	m_greenGraphID = m_graphCtrl.AddGraph(RGB(0,255,0), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_RGBLEVELBLUE );
	m_blueGraphID = m_graphCtrl.AddGraph(RGB(70,70,255), (LPSTR)(LPCSTR)Msg);
	
	Msg.LoadString ( IDS_RGBLEVELREDDATAREF );
	m_redDataRefGraphID = m_graphCtrl.AddGraph(RGB(255,0,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT);	//Ki
	Msg.LoadString ( IDS_RGBLEVELGREENDATAREF );
	m_greenDataRefGraphID = m_graphCtrl.AddGraph(RGB(0,255,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT);	//Ki
	Msg.LoadString ( IDS_RGBLEVELBLUEDATAREF );
	m_blueDataRefGraphID = m_graphCtrl.AddGraph(RGB(70,70,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT);	//Ki
	
	m_graphCtrl.SetScale(0,100,50,150);
	m_graphCtrl.SetXAxisProps((LPSTR)(LPCSTR)GetConfig()->m_PercentGray, 10, 0, 100);
	m_graphCtrl.SetYAxisProps("%", 10, 0, 400);

	Msg.LoadString ( IDS_DELTAE );
	m_deltaEGraphID = m_graphCtrl2.AddGraph(RGB(255,0,255), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_DELTAEDATAREF );
	m_deltaEDataRefGraphID = m_graphCtrl2.AddGraph(RGB(255,0,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT); // Ki
	Msg.LoadString ( IDS_DELTAEBETWEENDATAANDREF );
	m_deltaEBetweenGraphID = m_graphCtrl2.AddGraph(RGB(255,192,128), (LPSTR)(LPCSTR)Msg);
	
	m_graphCtrl2.SetScale(0,100,0,10);
	m_graphCtrl2.SetXAxisProps((LPSTR)(LPCSTR)GetConfig()->m_PercentGray, 10, 0, 100);
	m_graphCtrl2.SetYAxisProps("", 2, 0, 40);

	m_scaleYrgb = 0;
	m_scaleYdeltaE = 0;

	m_graphCtrl.ReadSettings("RGB Histo");
	m_graphCtrl2.ReadSettings("RGB Histo2");
}

void CRGBGrapher::UpdateGraph ( CDataSetDoc * pDoc )
{
	BOOL	bIRE = pDoc->GetMeasure()->m_bIREScaleMode;
	double	YWhite, YWhiteRefDoc;
	double	Gamma, Offset, OffsetRef;

	m_graphCtrl.SetXAxisProps(bIRE?"IRE":(LPSTR)(LPCSTR)GetConfig()->m_PercentGray, 10, 0, 100);
	m_graphCtrl2.SetXAxisProps(bIRE?"IRE":(LPSTR)(LPCSTR)GetConfig()->m_PercentGray, 10, 0, 100);

	m_graphCtrl.ClearGraph(m_refGraphID);
	if (m_showReference)
	{	
		m_graphCtrl.AddPoint(m_refGraphID, bIRE ? 7.5 : 0, 100);
		m_graphCtrl.AddPoint(m_refGraphID, 100, 100);
	}

	m_graphCtrl.ClearGraph(m_redGraphID);
	m_graphCtrl.ClearGraph(m_greenGraphID);
	m_graphCtrl.ClearGraph(m_blueGraphID);

	m_graphCtrl.ClearGraph(m_redDataRefGraphID); //Ki
	m_graphCtrl.ClearGraph(m_greenDataRefGraphID); //Ki
	m_graphCtrl.ClearGraph(m_blueDataRefGraphID); //Ki

	m_graphCtrl2.ClearGraph(m_deltaEGraphID);
	m_graphCtrl2.ClearGraph(m_deltaEDataRefGraphID); //Ki
	m_graphCtrl2.ClearGraph(m_deltaEBetweenGraphID);
	
	if(IsWindow(m_graphCtrl2.m_hWnd))
		m_graphCtrl2.ShowWindow(m_showDeltaE ? SW_SHOW : SW_HIDE);

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
	
	if (pDataRef && !pDataRef->GetMeasure()->GetGray(0).isValid())
		pDataRef = NULL;

	if (pDoc->GetMeasure()->GetGray(0).isValid())
	{
		// Retrieve gamma and offset
		CColor			refColor = GetColorReference().GetWhite();

		if ( size )
			pDoc->ComputeGammaAndOffset(&Gamma, &Offset, 3, 1, size,false);

		YWhite = pDoc->GetMeasure()->GetOnOffWhite()[1];

		// From 0: the black patch's BALANCE is a real reading - see the guard on
		// the plot below for the two conditions that make it one.
		for (int i=0; i<size; i++)
		{
			double x = pDoc->GetMeasure()->GetGrayPercent ( i, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235() );
            double valy;
			ColorxyY aColor=pDoc->GetMeasure()->GetGray(i).GetxyYValue();
            ColorxyY tmpColor(GetColorReference().GetWhite());

			// Determine Reference Y luminance for Delta E calculus
			if ( GetConfig ()->m_dE_gray > 0 || GetConfig ()-> m_dE_form == 5 )
			{
            	CColor White = pDoc -> GetMeasure () -> GetOnOffWhite();
	           	CColor Black = pDoc -> GetMeasure () -> GetOnOffBlack();
				int mode = GetConfig()->m_GammaOffsetType;
				if (GetConfig()->m_colorStandard == sRGB) mode = 99;
				if ( mode >= 4 )
		        {
			        double valx = GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());
                    valy = getL_EOTF(valx, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
//					valy = min(valy, GetConfig()->m_TargetMaxL);
		        }
		        else
		        {
			        double valx=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235())+Offset)/(1.0+Offset);
			        valy=pow(valx, GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef))+Offset;
					if (mode == 1) //black compensation target
						valy = (Black.GetY() + ( valy * ( YWhite - Black.GetY() ) )) / YWhite;
		        }

				if (mode == 5)
					tmpColor[2] = valy * 100. / YWhite;
				else
					tmpColor[2] = valy;
                if (GetConfig ()->m_dE_gray == 2 || GetConfig ()->m_dE_form == 5 )
                    tmpColor[2] = aColor [ 2 ] / YWhite;

				refColor.SetxyYValue(tmpColor);
			}
			//RGB plots now include luminance offset when grayscale dE handling includes it
			// dE_gray 0 normalises each patch against its OWN luminance, so the dE
			// it reports is chromaticity only. That per-patch value stays a LOCAL:
			// written back into YWhite it is loop-carried, and the reference block at
			// the top of the NEXT iteration would divide by the previous patch's
			// luminance instead of the display white - a black that reads 0, with the
			// loop now starting there, gives inf.
			//
			// Defensive, not a live fix: that block only runs for dE_form 5, and the
			// preferences dialog forces dE_gray to 2 whenever dE_form is 5 and greys
			// the combo out (AdvancedPropPage.cpp OnApply/OnSetActive, there since
			// 3.1.0.4), so the pair is unreachable from the UI. AccuracyTest.cpp does
			// set m_dE_gray directly though, and MainView (case 0) and Export.cpp both
			// keep YWhite intact - this brings the outlier into line.
			double fact;
			double YWhitePatch = YWhite;
			if ( GetConfig ()->m_dE_gray == 0 )
			{
				// Use actual gray luminance as correct reference (absolute)
				YWhitePatch = aColor [ 2 ];
				fact = 1.0;
			}
			else
				fact = aColor[2] / (tmpColor[2] * pDoc->GetMeasure()->GetOnOffWhite()[1]);

			// HIST-011: aColor[1] is the CIE y chromaticity, and it is the divisor for
			// both X and Z below. A degenerate black reading - y == 0 while Y is still
			// above zero - made both terms inf, and the guard tested Y, not y, so the
			// point was plotted and took the chart scale with it. Latent while the loop
			// started at 1; point 0 is exactly where a meter hands back a degenerate
			// chromaticity. Built inside the guard so the division does not happen at
			// all when there is nothing to plot.
			//
			// Point 0 needs two more things the others do not. The w/gamma
			// normalisation (m_dE_gray == 1) divides by the TARGET luminance,
			// which is exactly 0 at black - inf, or NaN when the measurement is
			// zero too; only the chromaticity-only normalisations survive there.
			// And black itself has to have light in it: with Y == 0 the xy is a
			// 0/0 fallback that would plot as a plausible-looking tint reading
			// out of nothing.
			if (aColor.isValid() && aColor[1] > 0.0 && ( i > 0 || ( GetConfig()->m_dE_gray != 1 && aColor[2] > 0.0 ) ))
			{
				ColorXYZ aMeasure(aColor[0]/aColor[1] * fact, fact, (1.0-(aColor[0]+aColor[1]))/aColor[1] * fact);
				ColorRGB normColor(aMeasure, GetColorReference());
				m_graphCtrl.AddPoint(m_redGraphID, x, normColor[0]*100.0);
				m_graphCtrl.AddPoint(m_greenGraphID, x, normColor[1]*100.0);
				m_graphCtrl.AddPoint(m_blueGraphID, x, normColor[2]*100.0);
			}

			// Black gets a dE only where the normalisation makes it mean
			// something. m_dE_gray == 2 (and dE_form 5) set the target's
			// luminance to the MEASURED one a few lines up, so what is left is
			// the chromaticity error - "is my black tinted" - which is the same
			// quantity CalMAN prints at 0% and why its number lands under 1
			// instead of being an error against an ideal zero. The other
			// normalisations leave the target at Y=0 there. Y > 0 because a
			// patch with no light has no chromaticity to be wrong about.
			if(m_showDeltaE && ( i > 0 || ( ( GetConfig()->m_dE_gray == 2 || GetConfig()->m_dE_form == 5 ) && aColor[2] > 0.0 ) ))
			{
				if (aColor.isValid())
				{
					CString str;
					ColorLab aColorLab = pDoc->GetMeasure()->GetGray(i).GetLabValue(YWhitePatch, GetColorReference());
					str.Format("L*a*b*:%.2f %.2f %.2f",aColorLab[0],aColorLab[1],aColorLab[2]);
					m_graphCtrl2.AddPoint(m_deltaEGraphID, x, pDoc->GetMeasure()->GetGray(i).GetDeltaE(YWhitePatch, refColor, 1.0, GetColorReference(), GetConfig()->m_dE_form, true, GetConfig()->gw_Weight), str);
				}
			}
		}
	}

	if((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc)) 
	{
		BOOL			bMainDocHasColors = (pDoc->GetMeasure()->GetGray(0).isValid());
		CColor			refColor = GetColorReference().GetWhite();

		if ( size && pDataRef->GetMeasure()->GetGray(0).isValid() )
			pDataRef->ComputeGammaAndOffset(&Gamma, &OffsetRef, 3, 1, size,false);

		YWhiteRefDoc = pDataRef->GetMeasure()->GetOnOffWhite()[1];
		ColorxyY tmpColor(GetColorReference().GetWhite());

		// From 0, same as the primary document above.
		for (int i=0; i<size; i++)
		{
			ColorxyY aColor, aColor2;
		    double x = pDataRef->GetMeasure()->GetGrayPercent ( i, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235() );
            double valy;			
			
			aColor=pDataRef->GetMeasure()->GetGray(i).GetxyYValue();
			if ( bMainDocHasColors )
				aColor2=pDoc->GetMeasure()->GetGray(i).GetxyYValue();

			// Determine Reference Y luminance for Delta E calculus
			if ( GetConfig ()->m_dE_gray > 0 || GetConfig ()->m_dE_form == 5 )
			{
            	CColor White = pDataRef -> GetMeasure () -> GetOnOffWhite();
	           	CColor Black = pDataRef -> GetMeasure () -> GetOnOffBlack();
				int mode = GetConfig()->m_GammaOffsetType;
				if (GetConfig()->m_colorStandard == sRGB) mode = 99;
				if ( mode >= 4 )
		        {
			        double valx = GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());
                    valy = getL_EOTF(valx, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
//					valy = min(valy, GetConfig()->m_TargetMaxL);
		        }
		        else
		        {
			        // HIST-007: OffsetRef is what ComputeGammaAndOffset fitted on
			        // pDataRef a few lines up. Offset belongs to the primary document,
			        // so the comparison curve was built from the wrong fit - and
			        // OffsetRef was computed and then never read.
			        double valx=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235())+OffsetRef)/(1.0+OffsetRef);
			        valy=pow(valx, GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef))+OffsetRef;
					if (mode == 1) //black compensation target
						valy = (Black.GetY() + ( valy * ( YWhiteRefDoc - Black.GetY() ) )) / YWhiteRefDoc;
		        }

				if (mode == 5)
					tmpColor[2] = valy * 100. / YWhiteRefDoc;
				else
					tmpColor[2] = valy;
                if (GetConfig ()->m_dE_gray == 2 || GetConfig ()->m_dE_form == 5 )
                    tmpColor[2] = aColor [ 2 ] / YWhiteRefDoc;

				refColor.SetxyYValue(tmpColor);
			}
			//RGB plots now include luminance offset when grayscale dE handling includes it
			// Same per-patch locals as the primary loop above - see the comment
			// there. YWhite / YWhiteRefDoc stay the documents' own white.
			double fact;
			double YWhiteRefPatch = YWhiteRefDoc;
			if ( GetConfig ()->m_dE_gray == 0 )
			{
				// Use actual gray luminance as correct reference (absolute)
				YWhiteRefPatch = aColor [ 2 ];
				fact = 1.0;
			}
			else
				fact = aColor[2] / (tmpColor[2] * pDataRef->GetMeasure()->GetOnOffWhite()[1]);

			double YWhitePatch;
			if ( !bMainDocHasColors )
				YWhitePatch = YWhiteRefPatch;
			else if ( GetConfig ()->m_dE_gray == 0 )
				YWhitePatch = aColor2 [ 2 ];
			else
				YWhitePatch = YWhite;

			// HIST-011: aColor[1] is the CIE y chromaticity, and it is the divisor for
			// both X and Z below. A degenerate black reading - y == 0 while Y is still
			// above zero - made both terms inf, and the guard tested Y, not y, so the
			// point was plotted and took the chart scale with it. Latent while the loop
			// started at 1; point 0 is exactly where a meter hands back a degenerate
			// chromaticity. Built inside the guard so the division does not happen at
			// all when there is nothing to plot.
			if (aColor.isValid() && aColor[1] > 0.0 && ( i > 0 || ( GetConfig()->m_dE_gray != 1 && aColor[2] > 0.0 ) ))
			{
				ColorXYZ aMeasure(aColor[0]/aColor[1] * fact, fact, (1.0-(aColor[0]+aColor[1]))/aColor[1] * fact);
				ColorRGB normColor(aMeasure, GetColorReference());
				m_graphCtrl.AddPoint(m_redDataRefGraphID, x, normColor[0]*100.0);
				m_graphCtrl.AddPoint(m_greenDataRefGraphID, x, normColor[1]*100.0);
				m_graphCtrl.AddPoint(m_blueDataRefGraphID, x, normColor[2]*100.0);
			}

			if(m_showDeltaE && ( i > 0 || ( ( GetConfig()->m_dE_gray == 2 || GetConfig()->m_dE_form == 5 ) && aColor[2] > 0.0 ) ))
			{
				if (aColor.isValid())
				{
					CString str;
					ColorLab aColorLab = pDataRef->GetMeasure()->GetGray(i).GetLabValue(YWhiteRefPatch, GetColorReference());
					str.Format("L*a*b*:%.2f %.2f %.2f",aColorLab[0],aColorLab[1],aColorLab[2]);
					m_graphCtrl2.AddPoint(m_deltaEDataRefGraphID, x, pDataRef->GetMeasure()->GetGray(i).GetDeltaE(YWhiteRefPatch, refColor, 1.0, GetColorReference(), GetConfig()->m_dE_form, true, GetConfig()->gw_Weight ), str);
				}
				
				// The enclosing guard checks the COMPARISON document's black
				// (aColor[2]); this point normalises with the PRIMARY document's,
				// because m_dE_gray == 0 makes YWhitePatch aColor2[2] - the black
				// patch's own luminance, and point 0 is the one place on the ramp
				// where that is legitimately zero.
				//
				// Not a divide by zero: ColorXYZ::GetDeltaE clamps a non-positive
				// YWhite to 120 and YWhiteRef to 1.0 before it builds any Lab. That
				// is the reason to suppress the point, not to keep it - it gets
				// drawn, as a dE against a white nobody chose. Reachable at point 0
				// with dE_form 5 + dE_gray 0: the pair is blocked in
				// AdvancedPropPage, but Measure.cpp restores both straight out of a
				// loaded .chc.
				//
				// Point 0 only. Under every other m_dE_gray both divisors are
				// document-wide (GetOnOffWhite), so testing them there would drop
				// the whole trace for a grayscale typed into the grid rather than
				// measured - SetGray writes only m_grayMeasureArray, leaving
				// m_OnOffWhite at noDataColor - and that is a change to every point,
				// not to black.
				if (bMainDocHasColors && aColor.isValid()
					&& ( i > 0 || ( YWhitePatch > 0.0 && YWhiteRefPatch > 0.0 ) ))
						m_graphCtrl2.AddPoint(m_deltaEBetweenGraphID, x, pDoc->GetMeasure()->GetGray(i).GetDeltaE(YWhitePatch,pDataRef->GetMeasure()->GetGray(i),YWhiteRefPatch, GetColorReference(), GetConfig()->m_dE_form, true, GetConfig()->gw_Weight)); //Ki
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// CRGBHistoView

IMPLEMENT_DYNCREATE(CRGBHistoView, CSavingView)

CRGBHistoView::CRGBHistoView()
	: CSavingView()
{
	//{{AFX_DATA_INIT(CRGBHistoView)
	//}}AFX_DATA_INIT
}

CRGBHistoView::~CRGBHistoView()
{
}

BEGIN_MESSAGE_MAP(CRGBHistoView, CSavingView)
	//{{AFX_MSG_MAP(CRGBHistoView)
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_RGB_GRAPH_REFERENCE, OnRgbGraphReference)
	ON_COMMAND(IDM_RGB_GRAPH_DELTAE, OnRgbGraphDeltaE)
	ON_COMMAND(IDM_RGB_GRAPH_GAMMA, OnRgbGraphGamma)
	ON_COMMAND(IDM_RGB_GRAPH_DATAREF, OnRgbGraphDataRef)	//Ki
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
	ON_COMMAND(IDM_RGB_GRAPH_Y_SCALE1, OnRGBGraphYScale1)
	ON_COMMAND(IDM_RGB_GRAPH_Y_SCALE2, OnRGBGraphYScale2)
	ON_COMMAND(IDM_RGB_GRAPH_Y_SCALE3, OnRGBGraphYScale3)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SCALE1, OnDeltaEGraphYScale1)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SCALE2, OnDeltaEGraphYScale2)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SCALE3, OnDeltaEGraphYScale3)
	ON_COMMAND(IDM_GRAPH_SCALE_FIT, OnGraphScaleFit)
	ON_COMMAND(IDM_GRAPH_Y_SCALE_FIT, OnGraphYScaleFit)
	ON_COMMAND(IDM_GRAPH_SCALE_CUSTOM, OnGraphScaleCustom)
	ON_COMMAND(IDM_GRAPH_SAVE, OnGraphSave)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SCALE_FIT, OnDeltaEGraphYScaleFit)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SHIFT_BOTTOM, OnDeltaEGraphYShiftBottom)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SHIFT_TOP, OnDeltaEGraphYShiftTop)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_ZOOM_IN, OnDeltaEGraphYZoomIn)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_ZOOM_OUT, OnDeltaEGraphYZoomOut)
	ON_COMMAND(IDM_DELTAE_GRAPH_SCALE_CUSTOM, OnDeltaEGraphScaleCustom)
	ON_COMMAND(IDM_HELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CRGBHistoView diagnostics

#ifdef _DEBUG
void CRGBHistoView::AssertValid() const
{
	CSavingView::AssertValid();
}
 
void CRGBHistoView::Dump(CDumpContext& dc) const
{
	CSavingView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CRGBHistoView message handlers

void CRGBHistoView::OnInitialUpdate() 
{
	CSavingView::OnInitialUpdate();

	CRect rect;
	GetClientRect(&rect);	// fill entire window

	m_Grapher.m_graphCtrl.Create(_T("Graph Window"), rect, this, IDC_RGBHISTO_GRAPH);
	m_Grapher.m_graphCtrl2.Create(_T("Graph2 Window"), rect, this, IDC_RGBHISTO_GRAPH2);

	OnSize(SIZE_RESTORED,rect.Width(),rect.Height());	// to size graph according to m_showDeltaE

	OnUpdate(NULL,UPD_EVERYTHING,NULL);
}

void CRGBHistoView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
	// Update this view only when we are concerned
	if ( lHint == UPD_EVERYTHING || lHint == UPD_GRAYSCALEANDCOLORS || lHint == UPD_GRAYSCALE || lHint == UPD_DATAREFDOC || lHint == UPD_REFERENCEDATA || lHint == UPD_ARRAYSIZES || lHint == UPD_GENERALREFERENCES || lHint == UPD_SENSORCONFIG || lHint == UPD_FREEMEASUREAPPENDED || (lHint >= UPD_REALTIME && lHint != UPD_DISPLAYPROFILE && lHint != UPD_REALTIME + 13))
	{
		m_Grapher.UpdateGraph ( GetDocument () );
		// Keep the two stacked charts' x-axes aligned so their vertical gridlines line up:
		// the delta-E chart (graphCtrl2) can auto-fit to a narrower valid x-range than the
		// RGB balance chart stacked above it -- mirror its x-range onto the RGB balance one.
		m_Grapher.m_graphCtrl2.m_minX      = m_Grapher.m_graphCtrl.m_minX;
		m_Grapher.m_graphCtrl2.m_maxX      = m_Grapher.m_graphCtrl.m_maxX;
		m_Grapher.m_graphCtrl2.m_xScale    = m_Grapher.m_graphCtrl.m_xScale;
		m_Grapher.m_graphCtrl2.m_xAxisStep = m_Grapher.m_graphCtrl.m_xAxisStep;
		RedrawWindow( NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW );
	}
}

DWORD CRGBHistoView::GetUserInfo ()
{
	return	( ( m_Grapher.m_showReference	& 0x0001 )	<< 0 )
		  + ( ( m_Grapher.m_showDeltaE		& 0x0001 )	<< 1 )
		  + ( ( m_Grapher.m_showDataRef		& 0x0001 )	<< 2 )
		  + ( ( m_Grapher.m_scaleYrgb		& 0x0003 )	<< 3 )  // 2 bits
		  + ( ( m_Grapher.m_scaleYdeltaE	& 0x0003 )	<< 5 ); // 2 bits
}

void CRGBHistoView::SetUserInfo ( DWORD dwUserInfo )
{
	m_Grapher.m_showReference	= ( dwUserInfo >> 0 ) & 0x0001;
	m_Grapher.m_showDeltaE		= ( dwUserInfo >> 1 ) & 0x0001;
	m_Grapher.m_showDataRef		= ( dwUserInfo >> 2 ) & 0x0001;
	m_Grapher.m_scaleYrgb		= ( dwUserInfo >> 3 ) & 0x0003;	// 2 bits
	m_Grapher.m_scaleYdeltaE	= ( dwUserInfo >> 5 ) & 0x0003;	// 2 bits

	switch ( m_Grapher.m_scaleYrgb )
	{
		case 0:
			 m_Grapher.m_graphCtrl.SetYScale(50,150);
			 m_Grapher.m_graphCtrl.SetYAxisProps("%", 10, 0, 400);
			 break;

		case 1:
			 m_Grapher.m_graphCtrl.SetYScale(0,200);
		 	 m_Grapher.m_graphCtrl.SetYAxisProps("%", 20, 0, 400);
			 break;

		case 2:
			 m_Grapher.m_graphCtrl.SetYScale(80,120);
			 m_Grapher.m_graphCtrl.SetYAxisProps("%", 5, 0, 400);
			 break;
	}

	switch ( m_Grapher.m_scaleYdeltaE )
	{
		case 0:
			 m_Grapher.m_graphCtrl2.SetYScale(0,10);
			 m_Grapher.m_graphCtrl2.SetYAxisProps("", 1, 0, 40);
			 break;

		case 1:
			 m_Grapher.m_graphCtrl2.SetYScale(0,20);
		 	 m_Grapher.m_graphCtrl2.SetYAxisProps("", 2, 0, 40);
			 break;

		case 2:
			 m_Grapher.m_graphCtrl2.SetYScale(0,5);
			 m_Grapher.m_graphCtrl2.SetYAxisProps("", 1, 0, 40);
			 break;
	}
}

void CRGBHistoView::OnSize(UINT nType, int cx, int cy) 
{
	CSavingView::OnSize(nType, cx, cy);
	
	if(m_Grapher.m_showDeltaE)
	{
		if(IsWindow(m_Grapher.m_graphCtrl.m_hWnd))
			m_Grapher.m_graphCtrl.MoveWindow(0,0,cx,cy/2);
		if(IsWindow(m_Grapher.m_graphCtrl2.m_hWnd))
			m_Grapher.m_graphCtrl2.MoveWindow(0,cy/2,cx,cy/2);
	}
	else
		if(IsWindow(m_Grapher.m_graphCtrl.m_hWnd))
			m_Grapher.m_graphCtrl.MoveWindow(0,0,cx,cy);
}

void CRGBHistoView::OnDraw(CDC* pDC) 
{
	CString strTitle;
	strTitle.LoadString ( GetConfig()->m_dE_gray == 1 ? IDS_GRAPH_GSBALGAMMA : IDS_GRAPH_GSBALNOGAMMA );
	m_Grapher.m_graphCtrl.m_graphArray[0].p_Title=strTitle;
}

BOOL CRGBHistoView::OnEraseBkgnd(CDC* pDC) 
{
	return TRUE;
}

void CRGBHistoView::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	// load and display popup menu
	CNewMenu menu;
	menu.LoadMenu(IDR_RGB_GRAPH_MENU);
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup);

    pPopup->CheckMenuItem(IDM_RGB_GRAPH_REFERENCE, m_Grapher.m_showReference ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_RGB_GRAPH_DELTAE, m_Grapher.m_showDeltaE ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
	pPopup->CheckMenuItem(IDM_RGB_GRAPH_GAMMA, GetConfig()->m_dE_gray == 1 ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
	pPopup->CheckMenuItem(IDM_RGB_GRAPH_DATAREF, m_Grapher.m_showDataRef ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND); //Ki

	pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL,
		point.x, point.y, this);
}

void CRGBHistoView::OnRgbGraphReference() 
{
	m_Grapher.m_showReference = !m_Grapher.m_showReference;
	GetConfig()->WriteProfileInt("RGB Histo","Show Reference",m_Grapher.m_showReference);
	OnUpdate(NULL,NULL,NULL);
}

void CRGBHistoView::OnRgbGraphDeltaE() 
{
	m_Grapher.m_showDeltaE = !m_Grapher.m_showDeltaE;
	GetConfig()->WriteProfileInt("RGB Histo","Show Delta E",m_Grapher.m_showDeltaE);

	CRect rect;
	GetClientRect(&rect);
	OnSize(SIZE_RESTORED,rect.Width(),rect.Height());	// to size graph according to m_showDeltaE

	OnUpdate(NULL,NULL,NULL);
}

void CRGBHistoView::OnRgbGraphGamma() 
{
	if (GetConfig()->m_dE_form == 5)
	{
		GetConfig()->m_dE_form = 0;
		GetConfig()->WriteProfileInt("Advanced","dE_form",0);
	}

	if (GetConfig()->m_dE_gray == 1)
	{
		GetConfig()->WriteProfileInt("Advanced","dE_gray",2);
		GetConfig()->m_dE_gray = 2;
	}
	else
	{
		GetConfig()->WriteProfileInt("Advanced","dE_gray",1);
		GetConfig()->m_dE_gray = 1;
	}

	GetDocument()->UpdateAllViews(NULL, UPD_GRAYSCALE, NULL);
	GetDocument()->UpdateAllViews(NULL, UPD_SELECTEDCOLOR, NULL);
}

void CRGBHistoView::OnRgbGraphDataRef()  //Ki
{
	m_Grapher.m_showDataRef = !m_Grapher.m_showDataRef;
	GetConfig()->WriteProfileInt("RGB Histo","Show Reference Data",m_Grapher.m_showDataRef);
	OnUpdate(NULL,NULL,NULL);
}

void CRGBHistoView::OnGraphSettings() 
{
	// Add delta E graph to first graph control to allow setting change 
	int tmpGraphID=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_deltaEGraphID]);
	int tmpGraphID2=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_deltaEDataRefGraphID]);
	int tmpGraphID3=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_deltaEBetweenGraphID]);

	m_Grapher.m_graphCtrl.ClearGraph(tmpGraphID);
	m_Grapher.m_graphCtrl.ClearGraph(tmpGraphID2);
	m_Grapher.m_graphCtrl.ClearGraph(tmpGraphID3);

	m_Grapher.m_graphCtrl.ChangeSettings();

	// Update graph2 setting according to graph and values of tmpGraphID
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID,m_Grapher.m_deltaEGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID2,m_Grapher.m_deltaEDataRefGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID3,m_Grapher.m_deltaEBetweenGraphID);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID3);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID2);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID);

	OnUpdate(NULL,NULL,NULL);
}

void CRGBHistoView::OnGraphSave() 
{
	if(m_Grapher.m_showDeltaE)
		m_Grapher.m_graphCtrl.SaveGraphs(&m_Grapher.m_graphCtrl2);
	else
		m_Grapher.m_graphCtrl.SaveGraphs();
}

void CRGBHistoView::OnGraphScaleCustom() 
{
	m_Grapher.m_graphCtrl.ChangeScale();
	m_Grapher.m_scaleYrgb = 0;
	m_Grapher.m_graphCtrl.WriteSettings("RGB Histo");
	m_Grapher.m_graphCtrl2.WriteSettings("RGB Histo2");
}

void CRGBHistoView::OnGraphScaleFit() 
{
	OnGraphXScaleFit();
	OnGraphYScaleFit();
	OnDeltaEGraphYScaleFit();
	m_Grapher.m_graphCtrl.WriteSettings("RGB Histo");
	m_Grapher.m_graphCtrl2.WriteSettings("RGB Histo2");
}

void CRGBHistoView::OnRGBGraphYScale1() 
{
	m_Grapher.m_graphCtrl.SetYScale(0,200);
	m_Grapher.m_scaleYrgb = 1;
	m_Grapher.m_graphCtrl.SetYAxisProps("%", 20, 0, 400);
	m_Grapher.m_graphCtrl.WriteSettings("RGB Histo");
	m_Grapher.m_graphCtrl2.WriteSettings("RGB Histo2");
	Invalidate(FALSE);
}

void CRGBHistoView::OnRGBGraphYScale2() 
{
	m_Grapher.m_graphCtrl.SetYScale(50,150);
	m_Grapher.m_scaleYrgb = 0;
	m_Grapher.m_graphCtrl.SetYAxisProps("%", 10, 0, 400);
	m_Grapher.m_graphCtrl.WriteSettings("RGB Histo");
	m_Grapher.m_graphCtrl2.WriteSettings("RGB Histo2");
	Invalidate(FALSE);
}

void CRGBHistoView::OnRGBGraphYScale3() 
{
	m_Grapher.m_graphCtrl.SetYScale(80,120);
	m_Grapher.m_scaleYrgb = 2;
	m_Grapher.m_graphCtrl.SetYAxisProps("%", 5, 0, 400);
	m_Grapher.m_graphCtrl.WriteSettings("RGB Histo");
	m_Grapher.m_graphCtrl2.WriteSettings("RGB Histo2");
	Invalidate(FALSE);
}

void CRGBHistoView::OnDeltaEGraphYScale1() 
{
	m_Grapher.m_graphCtrl2.SetYScale(0,20);
	m_Grapher.m_scaleYdeltaE = 1;
	m_Grapher.m_graphCtrl2.SetYAxisProps("", 2, 0, 40);
	m_Grapher.m_graphCtrl.WriteSettings("RGB Histo");
	m_Grapher.m_graphCtrl2.WriteSettings("RGB Histo2");
	Invalidate(FALSE);
}

void CRGBHistoView::OnDeltaEGraphYScale2() 
{
	m_Grapher.m_graphCtrl2.SetYScale(0,5);
	m_Grapher.m_scaleYdeltaE = 2;
	m_Grapher.m_graphCtrl2.SetYAxisProps("", 1, 0, 40);
	m_Grapher.m_graphCtrl.WriteSettings("RGB Histo");
	m_Grapher.m_graphCtrl2.WriteSettings("RGB Histo2");
	Invalidate(FALSE);
}

void CRGBHistoView::OnDeltaEGraphYScale3() 
{
	m_Grapher.m_graphCtrl2.SetYScale(0,10);
	m_Grapher.m_scaleYdeltaE = 0;
	m_Grapher.m_graphCtrl2.SetYAxisProps("", 1, 0, 40);
	m_Grapher.m_graphCtrl.WriteSettings("RGB Histo");
	m_Grapher.m_graphCtrl2.WriteSettings("RGB Histo2");
	Invalidate(FALSE);
}

void CRGBHistoView::OnDeltaEGraphYScaleFit() 
{
	m_Grapher.m_graphCtrl2.FitYScale(TRUE,1);
	m_Grapher.m_scaleYdeltaE = 0;
	m_Grapher.m_graphCtrl2.m_yAxisStep=(m_Grapher.m_graphCtrl2.m_maxY-m_Grapher.m_graphCtrl2.m_minY)/10;
	m_Grapher.m_graphCtrl.WriteSettings("RGB Histo");
	m_Grapher.m_graphCtrl2.WriteSettings("RGB Histo2");
	Invalidate(FALSE);
}

void CRGBHistoView::OnDeltaEGraphYShiftBottom() 
{
	m_Grapher.m_graphCtrl2.ShiftYScale(1);
	m_Grapher.m_scaleYdeltaE = 0;
	Invalidate(FALSE);
}

void CRGBHistoView::OnDeltaEGraphYShiftTop() 
{
	m_Grapher.m_graphCtrl2.ShiftYScale(-1);
	m_Grapher.m_scaleYdeltaE = 0;
	Invalidate(FALSE);
}

void CRGBHistoView::OnDeltaEGraphYZoomIn() 
{
	m_Grapher.m_graphCtrl2.GrowYScale(0,-4);
	m_Grapher.m_scaleYdeltaE = 0;
	Invalidate(FALSE);
}

void CRGBHistoView::OnDeltaEGraphYZoomOut() 
{
	m_Grapher.m_graphCtrl2.GrowYScale(0,+4);
	m_Grapher.m_scaleYdeltaE = 0;
	Invalidate(FALSE);
}

void CRGBHistoView::OnDeltaEGraphScaleCustom() 
{
	m_Grapher.m_graphCtrl2.ChangeScale();
	m_Grapher.m_scaleYdeltaE = 0;
	m_Grapher.m_graphCtrl.WriteSettings("RGB Histo");
	m_Grapher.m_graphCtrl2.WriteSettings("RGB Histo2");
}

void CRGBHistoView::OnGraphYScaleFit() 
{
	m_Grapher.m_graphCtrl.FitYScale(TRUE);
	m_Grapher.m_scaleYrgb = 0;
	Invalidate(FALSE);
}

void CRGBHistoView::OnGraphYShiftBottom() 
{
	m_Grapher.m_graphCtrl.ShiftYScale(10);
	m_Grapher.m_scaleYrgb = 0;
	Invalidate(FALSE);
}

void CRGBHistoView::OnGraphYShiftTop() 
{
	m_Grapher.m_graphCtrl.ShiftYScale(-10);
	m_Grapher.m_scaleYrgb = 0;
	Invalidate(FALSE);
}

void CRGBHistoView::OnGraphYZoomIn() 
{
	m_Grapher.m_graphCtrl.GrowYScale(+10,-10);
	m_Grapher.m_scaleYrgb = 0;
	Invalidate(FALSE);
}

void CRGBHistoView::OnGraphYZoomOut() 
{
	m_Grapher.m_graphCtrl.GrowYScale(-10,+10);
	m_Grapher.m_scaleYrgb = 0;
	Invalidate(FALSE);
}

void CRGBHistoView::OnGraphXScale1() 
{
	m_Grapher.m_graphCtrl.SetXScale(0,100);
	m_Grapher.m_graphCtrl2.SetXScale(0,100);
	Invalidate(FALSE);
}

void CRGBHistoView::OnGraphXScale2() 
{
	m_Grapher.m_graphCtrl.SetXScale(20,100);
	m_Grapher.m_graphCtrl2.SetXScale(20,100);
	Invalidate(FALSE);
}

void CRGBHistoView::OnGraphXScaleFit() 
{
	m_Grapher.m_graphCtrl.FitXScale(TRUE);
	m_Grapher.m_graphCtrl2.FitXScale(TRUE);
	Invalidate(FALSE);
}

void CRGBHistoView::OnGraphXZoomIn() 
{
	m_Grapher.m_graphCtrl.GrowXScale(+10,-10);
	m_Grapher.m_graphCtrl2.GrowXScale(+10,-10);
	Invalidate(FALSE);
}

void CRGBHistoView::OnGraphXZoomOut() 
{
	m_Grapher.m_graphCtrl.GrowXScale(-10,+10);
	m_Grapher.m_graphCtrl2.GrowXScale(-10,+10);
	Invalidate(FALSE);
}

void CRGBHistoView::OnGraphXShiftLeft() 
{
	m_Grapher.m_graphCtrl.ShiftXScale(-10);
	m_Grapher.m_graphCtrl2.ShiftXScale(-10);
	Invalidate(FALSE);
}

void CRGBHistoView::OnGraphXShiftRight() 
{
	m_Grapher.m_graphCtrl.ShiftXScale(+10);
	m_Grapher.m_graphCtrl2.ShiftXScale(+10);
	Invalidate(FALSE);
}


void CRGBHistoView::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_RGBLEVELS, NULL );
}
