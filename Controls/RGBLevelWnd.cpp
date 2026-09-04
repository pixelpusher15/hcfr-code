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
/////////////////////////////////////////////////////////////////////////////

// RGBLevelWnd.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "DataSetDoc.h"
#include "MainView.h"
#include "RGBLevelWnd.h"
#include "Color.h"
#include "fxcolor.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CRGBLevelWnd

void RGBTOHSV(double R, double G, double B, double& H, double& S, double& V)
{
	double var_R = ( R );                     //RGB values = From 0 to 1
	double var_G = ( G );
	double var_B = ( B );

	double  var_Min = min(min( var_R, var_G), var_B );    //Min. value of RGB
	double  var_Max = max(max( var_R, var_G), var_B );    //Max. value of RGB
	double  del_Max = var_Max - var_Min ;            //Delta RGB value

	V = var_Max;

	if ( del_Max == 0 )                     //This is a gray, no chroma...
	{
	   H = 0;                                //HSL results = From 0 to 1
	   S = 0;
	}
	else                                    //Chromatic data...
	{
       double alpha = 0.5 * (2 * var_R - var_G - var_B);
       double beta = pow(3., 0.5) / 2 * (var_G - var_B);
       S = pow(pow(alpha,2)+pow(beta,2),2);
       H = (atan2(beta,alpha) * 180 / PI + 180) / 360.;
	}
}

CRGBLevelWnd::CRGBLevelWnd()
{
	m_pRefColor = NULL;
	m_pDocument = NULL;
	m_bLumaMode = FALSE;
	m_bHasReference = TRUE;
	m_redValue = 0.;
	m_greenValue = 0.;
	m_blueValue = 0.;
	m_dEValue = 0.;
}

CRGBLevelWnd::~CRGBLevelWnd()
{
}

void CRGBLevelWnd::Refresh(int minCol, int m_displayMode, int nSize)
{
    
	BOOL bWasLumaMode = m_bLumaMode;
	// aReference is a member and the ladder below does not claim every column, so
	// a column it misses used to be read against whatever the PREVIOUS selection
	// left behind. Track whether this call actually found a target instead of
	// letting a stale one stand in for one.
	m_bHasReference = TRUE;
    double cx,cy,cz,cxref,cyref,czref;
	CColorReference cRef= (GetColorReference());
	int satsize=m_pDocument->GetMeasure()->GetSaturationSize();
	BOOL DVD = (GetConfig()->GetGeneratorType() == CColorHCFRConfig::enumManual);

	if ( m_pRefColor)
	{
		if (minCol > 0)
		{

			if (m_displayMode == 13)	// display profile patch
			{
				m_bLumaMode = TRUE;
				m_pDocument->GetMeasure()->GetRefProfileSat(minCol-1, aReference);
			}
			else if (m_displayMode == 11)
			{
				m_bLumaMode = TRUE;
				m_pDocument->GetMeasure()->GetRefCC24Sat(minCol-1, aReference);
			}
			else if (m_displayMode == 5)
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefSat(0, double(minCol-1) / double(nSize -1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
			}
			else if (m_displayMode == 6)
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefSat(1, double(minCol-1) / double(nSize -1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
			}
			else if (m_displayMode == 7)
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefSat(2, double(minCol-1) / double(nSize -1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
			}
			else if (m_displayMode == 8)
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefSat(3, double(minCol-1) / double(nSize -1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
			}
			else if (m_displayMode == 9)
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefSat(4, double(minCol-1) / double(nSize -1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
			}
			else if (m_displayMode == 10)
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefSat(5, double(minCol-1) / double(nSize -1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
			}
			// Black, the last column of Primaries and Secondaries. No branch used to
			// claim it and there is no final else, so it fell through holding the
			// previous column's target: pick Magenta then Black and the pane read
			// 100% green, Cyan then Black 100% red - always the complement of the
			// last selection, because where the stale target's channel is zero the
			// level maths correctly answers "on target". The maths is not the bug;
			// the target is.
			//
			// The measures grid already settled what this column's target is - case
			// 1, j == 7 sets refColor = noDataColor and prints no dE, since black's
			// target on a page that judges every other column with luminance
			// included is simply no light. Match it, and say so rather than
			// printing a 0.0 that looks like a reading. The xyY readout beside the
			// bars is computed from the measurement and is unaffected.
			else if ( m_displayMode == 1 && minCol == 8 )
			{
				m_bLumaMode = TRUE;
				m_bHasReference = FALSE;
				aReference = noDataColor;
			}
			else if ( m_displayMode == 1 && (minCol == 1 || m_pRefColor->GetDeltaxy ( m_pDocument->GetMeasure()->GetRefPrimary(0), cRef ) < 0.05))
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefPrimary(0);
			}
			else if ( m_displayMode == 1 && (minCol == 2 || m_pRefColor->GetDeltaxy ( m_pDocument->GetMeasure()->GetRefPrimary(1), cRef ) < 0.05))
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefPrimary(1);
			}
			else if ( m_displayMode == 1 && (minCol == 3 || m_pRefColor->GetDeltaxy ( m_pDocument->GetMeasure()->GetRefPrimary(2), cRef ) < 0.05))
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefPrimary(2);
			}
			else if ( m_displayMode == 1 && (minCol == 4 || m_pRefColor->GetDeltaxy ( m_pDocument->GetMeasure()->GetRefSecondary(0), cRef ) < 0.05))
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefSecondary(0);
			}
			else if ( m_displayMode == 1 && (minCol == 5 || m_pRefColor->GetDeltaxy ( m_pDocument->GetMeasure()->GetRefSecondary(1), cRef ) < 0.05))
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefSecondary(1);
			}
			else if ( m_displayMode == 1 && (minCol == 6 || m_pRefColor->GetDeltaxy ( m_pDocument->GetMeasure()->GetRefSecondary(2), cRef ) < 0.05))
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefSecondary(2);
			}
			//look for white and disable if detect for primaries/secondaries is not selected
			else if ( m_displayMode == 0 || (m_displayMode == 1 && minCol == 7) || m_displayMode == 3 || m_displayMode == 4 || m_pRefColor->GetDeltaxy ( cRef.GetWhite(), cRef ) < 0.05)
			{
				m_bLumaMode = (m_displayMode==1)?TRUE:FALSE;
				aReference = cRef.GetWhite();
			}
		} //mincol > 0
		else //autodetect if mincol <= 0 (measuring) and this is primaries or grayscale page
		{
			if ( m_displayMode == 1 && (m_pRefColor->GetDeltaxy ( m_pDocument->GetMeasure()->GetRefPrimary(0), cRef ) < 0.05 && GetConfig()->m_bDetectPrimaries))
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefPrimary(0);
			}
			else if ( m_displayMode == 1 && (m_pRefColor->GetDeltaxy ( m_pDocument->GetMeasure()->GetRefPrimary(1), cRef ) < 0.05) && GetConfig()->m_bDetectPrimaries)
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefPrimary(1);
			}
			else if ( m_displayMode == 1 && (m_pRefColor->GetDeltaxy ( m_pDocument->GetMeasure()->GetRefPrimary(2), cRef) < 0.05) && GetConfig()->m_bDetectPrimaries)
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefPrimary(2);
			}
			else if ( m_displayMode == 1 && (m_pRefColor->GetDeltaxy ( m_pDocument->GetMeasure()->GetRefSecondary(0), cRef ) < 0.05) && GetConfig()->m_bDetectPrimaries)
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefSecondary(0);
			}
			else if ( m_displayMode == 1 && (m_pRefColor->GetDeltaxy ( m_pDocument->GetMeasure()->GetRefSecondary(1), cRef ) < 0.05) && GetConfig()->m_bDetectPrimaries)
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefSecondary(1);
			}
			else if ( m_displayMode == 1 && (m_pRefColor->GetDeltaxy ( m_pDocument->GetMeasure()->GetRefSecondary(2), cRef ) < 0.05) && GetConfig()->m_bDetectPrimaries)
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefSecondary(2);
			}
			else if ( (m_displayMode == 1 || m_displayMode == 0) && m_pRefColor->GetDeltaxy ( GetColorReference().GetWhite(), cRef ) < 0.05 && GetConfig()->m_bDetectPrimaries)
			{
				m_bLumaMode = FALSE;
				aReference = cRef.GetWhite();
			}
		} 

		// Neither ladder claims every column, and a call that claims none leaves
		// aReference holding the previous selection's. The reference-relative
		// branch below - the only one that reads it - then skips silently, so the
		// level members keep the last selection's numbers while the pane still
		// draws them as data. That is how the black column's own case escaped: pick
		// Black in Primaries (aReference = noDataColor, levels zeroed), then let a
		// measuring refresh arrive on the minCol <= 0 path with detect-primaries
		// off - all seven of its branches are gated on it and there is no final
		// else - and the pane paints three enabled bars reading 0.0%, the
		// reading-shaped zero this column exists to avoid. Placed after the ladders
		// so m_bLumaMode is final; the HDR rescale below runs only for display
		// modes whose ladder always claims a target. The other branch computes its
		// levels without aReference, so it is not gated on one.
		if ( m_bLumaMode && !aReference.isValid() )
			m_bHasReference = FALSE;

		BOOL isHDR = ( GetConfig()->m_GammaOffsetType == 5 && (m_displayMode == 1 || m_displayMode >= 5 && m_displayMode <= 11 || m_displayMode == 13) );
		CColor white = m_pDocument->GetMeasure()->GetPrimeWhite();

		if (!white.isValid() && isHDR)
			white = m_pDocument->GetMeasure()->GetGray((m_pDocument->GetMeasure()->GetGrayScaleSize()-1) / 2 );

		if (!white.isValid() || m_displayMode == 0 || m_displayMode == 2 || m_displayMode == 3 || m_displayMode == 4)
			white = m_pDocument -> GetMeasure () ->GetOnOffWhite();
		if ( m_displayMode > 4 && (GetConfig()->m_colorStandard == HDTVa || GetConfig()->m_colorStandard == HDTVb))
			white = m_pDocument -> GetMeasure () ->GetOnOffWhite();

		//special case check if user has done a less than 100% primaries run and use grayscale white instead for colorchecker
		if (m_pDocument->GetMeasure()->GetOnOffWhite().isValid())
			if ((m_pDocument->GetMeasure()->GetPrimeWhite()[1] / m_pDocument->GetMeasure()->GetOnOffWhite()[1] < 0.9) && (m_displayMode == 11 || m_displayMode == 13)  && GetConfig()->m_GammaOffsetType != 5)
				white = m_pDocument -> GetMeasure () ->GetOnOffWhite();
		
    	int nCount = m_pDocument -> GetMeasure () -> GetGrayScaleSize ();
        double YWhite = white.GetY();
		double tmWhite = TmDiffuseWhiteNits(noDataColor, noDataColor);
		// The DVD branches below REASSIGN tmWhite to the manual-generator 0.50-code
		// white (~92.25). The unified paths (mode 13) pair with a reference scaled
		// by GetHDRRefScale(), which is built on the true tone-mapped diffuse
		// white - so they must normalize by THAT, whichever branch ran first.
		// Clamped exactly like GetHDRRefScale: this value is used as a divisor.
		const double tmWhiteRef = ( tmWhite > 0.0 ) ? tmWhite : 94.37844;

			if ( (GetConfig()->m_GammaOffsetType == 5 && (m_displayMode <=11 && m_displayMode >= 5 || m_displayMode == 13)) )
			{
				if (GetConfig()->m_CCMode >= MASCIOR50 && GetConfig()->m_CCMode <= CCMAXHDR && m_displayMode == 11)
				{
					aReference.SetX((aReference.GetX() * 100.));
					aReference.SetY((aReference.GetY() * 100.));
					aReference.SetZ((aReference.GetZ() * 100.));
				}
				else
				{
					// Unified HDR rescale (matches UpdateGrid ~4294 and the 3D
					// viewer): GetHDRRefScale, = 105.95640 with tone mapping off.
					// The manual generator (DVD) keeps the legacy fixed scale;
					// profile mode 13 is always unified.
					double s = ( DVD && m_displayMode != 13 ) ? 105.95640
							 : m_pDocument->GetMeasure()->GetHDRRefScale();
					aReference.SetX((aReference.GetX() * s));
					aReference.SetY((aReference.GetY() * s));
					aReference.SetZ((aReference.GetZ() * s));
				}
			}

        if (!m_bLumaMode)
        {
            ColorxyY tmpColor(GetColorReference().GetWhite());
		    // Determine Reference Y luminance for Delta E calculus
			if ( GetConfig ()->m_dE_gray > 0 || GetConfig ()->m_dE_form == 5 )
			{
				// Compute reference Luminance regarding actual offset and reference gamma 
                // fixed to use correct gamma predicts
                // and added option to assume perfect gamma
					double x;
					switch (m_displayMode)
					{
						int Count;
						case 3:
						x = ArrayIndexToGrayLevel ( (minCol - 1)*(GetConfig()->m_GammaOffsetType==5?2:1), 101, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235() );
						break;
						case 4:
						Count = m_pDocument -> GetMeasure()->GetNearWhiteScaleSize();
						x = ArrayIndexToGrayLevel ( m_pDocument->GetMeasure()->m_NearWhiteClipCol - Count + (minCol - 1), 101, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235() );
						break;
						default:
						x = m_pDocument->GetMeasure()->GetGrayPercent ( minCol - 1, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235() );
					}
					double valy, Gamma, Offset;
                    Gamma = GetConfig()->m_GammaRef;
                    GetConfig()->m_GammaAvg = Gamma;
                    m_pDocument->ComputeGammaAndOffset(&Gamma, &Offset, 1, 1, nCount, false);
                    if (GetConfig()->m_useMeasuredGamma)
						GetConfig()->m_GammaAvg = (Gamma<1?2.2:floor((Gamma+.005)*100.)/100.);
                    GetConfig()->SetPropertiesSheetValues();
            		CColor White = m_pDocument -> GetMeasure () -> GetGray ( nCount - 1 );
//	                CColor Black = m_pDocument -> GetMeasure () -> GetGray ( 0 );
	                CColor Black = m_pDocument -> GetMeasure () -> GetOnOffBlack();
					int mode = GetConfig()->m_GammaOffsetType;
					if (GetConfig()->m_colorStandard == sRGB) mode = 99;
					if ( mode >= 4 )
			        {
						double valx = GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235());
						valy = getL_EOTF(valx, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
//						valy = min(valy, GetConfig()->m_TargetMaxL);
			        }
			        else
			        {
				        double valx=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig()->GetUse10bitLevels(), GetConfig()->GetRGB16_235())+Offset)/(1.0+Offset);
				        valy=pow(valx, GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef));
						if (mode == 1) //black compensation target
							valy = (Black.GetY() + ( valy * ( YWhite - Black.GetY() ) )) / YWhite;
			        }

					if ( mode == 5)
						tmpColor[2] = valy * 100. / YWhite;
					else
						tmpColor[2] = valy;
                    if (GetConfig ()->m_dE_gray == 2 || GetConfig ()->m_dE_form == 5 )
			            tmpColor[2] = m_pRefColor->GetY() / YWhite; //perfect gamma
			
					aReference.SetxyYValue(tmpColor);

			}

			//RGB plots now include luminance offset when grayscale dE handling includes it
			double fact;
			ColorxyY aColor = m_pRefColor -> GetxyYValue();
			if ( GetConfig ()->m_dE_gray == 0 )
		    {
						// Use actual gray luminance as correct reference (absolute)
                           YWhite = m_pRefColor->GetY();
						   fact = 1.0;
			}
			else
				fact = aColor[2] / (tmpColor[2] * white.GetY());

            ColorXYZ normColor;

            // Column 1 of the grayscale and near-black pages is black, and it was
            // excluded outright: selecting 0% read 0.0% on all three bars while the
            // balance chart beside it plots that very patch. Let it through on the
            // same terms the chart uses (rgbhistoview.cpp) - the w/gamma
            // normalisation divides by a target luminance that is exactly 0 at
            // black, so only the chromaticity-only settings survive there, and the
            // patch needs light in it for its xy to mean anything. Free measures
            // reach here with minCol 1 meaning something else, and carrying a stale
            // m_displayMode - MainView's case 2 assigns none of last_Col, last_Size
            // or last_Display - so one selected after a grayscale or near-black
            // column arrives labelled as that page and does take this clause. What
            // it then reports agrees with the other free-measure columns, so it is
            // left that way.
            if(aColor[1] > 0.0 && (minCol != 1 || m_displayMode == 4
                || ((m_displayMode == 0 || m_displayMode == 3) && GetConfig()->m_dE_gray != 1 && aColor[2] > 0.0)))
            {
                normColor[0]=(aColor[0]/aColor[1])*fact;
                normColor[1]=1.0*fact;
                normColor[2]=((1.0-(aColor[0]+aColor[1]))/aColor[1])*fact;
            }
            else
            {
                normColor[0]=0.0;
                normColor[1]=0.0;
                normColor[2]=0.0;
            }

            ColorRGB normColorRGB(normColor, cRef);
                        
            m_redValue=(float)(normColorRGB[0]*100.0);
            m_greenValue=(float)(normColorRGB[1]*100.0);
            m_blueValue=(float)(normColorRGB[2]*100.0);
        }
		else if (aReference.isValid())
		{
            ColorXYZ aColor=m_pRefColor->GetXYZValue(), refColor=aReference.GetXYZValue() ;
			
			m_redValue=100.;
			m_greenValue=100.;
			m_blueValue=100.;

            if ( white.isValid() )
            {

				if ( isHDR )
				{
					bool shiftDiffuse=(abs(GetConfig()->m_DiffuseL-94.0)>0.5);
					if (DVD)
					{
						tmWhite = getL_EOTF(0.50, noDataColor, noDataColor, GetConfig()->m_GammaRel, GetConfig()->m_Split, 5, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) * 100.0;
						if (m_displayMode == 1)
						{
							if (GetColorReference().m_standard == UHDTV || GetColorReference().m_standard == UHDTV2 || GetColorReference().m_standard == HDTV || minCol == 7)
								white.SetY(!shiftDiffuse?92.254965:tmWhite);
							else
								white.SetY(94.37844);
						}
						else
						{
							if ( ( (GetColorReference().m_standard == UHDTV2 && minCol == satsize) || GetColorReference().m_standard == HDTV || GetColorReference().m_standard == UHDTV) && m_displayMode != 11 && m_displayMode != 13)// && !shiftDiffuse) //&& nCol == (satsize)
								white.SetY(92.25496);
							else if (m_displayMode == 13)
								// profile mode is always on the unified GetHDRRefScale
								// convention, even with the manual generator
								white.SetY(tmWhiteRef);
							else
								white.SetY(94.37844);
						}
					}
					else
					{
						if (m_displayMode == 1)
							if (GetColorReference().m_standard == UHDTV2 || GetColorReference().m_standard == HDTV || GetColorReference().m_standard == UHDTV || minCol == 7)
								white.SetY(tmWhite);
							else
								white.SetY(94.37844);
						else
							if ((GetConfig()->m_CCMode >= MASCIOR50 && GetConfig()->m_CCMode <= CCMAXHDR) && m_displayMode == 11)
								white.SetY(m_pDocument->GetMeasure()->GetGray((m_pDocument->GetMeasure()->GetGrayScaleSize()-1)).GetY());
							else
								// Unified convention: the reference above is
								// GetHDRRefScale-scaled (1.0 = tone-mapped diffuse
								// white), so normalize the measurement by the same
								// white (identical with tone mapping off).
								white.SetY(tmWhiteRef);
					}
				}
				aColor[0]=aColor[0]/white.GetY();
				aColor[1]=aColor[1]/white.GetY();
				aColor[2]=aColor[2]/white.GetY();

				ColorRGB aColorRGB(aColor, cRef);
				ColorRGB refColorRGB(refColor, cRef);

				cx = aColorRGB[0];
				cy = aColorRGB[1];
				cz = aColorRGB[2];
				cxref = refColorRGB[0];
				cyref = refColorRGB[1];
				czref = refColorRGB[2];
				// RGB or HSV vector differences for CMS, V = luminance
				if (GetConfig() -> m_useHSV)
				{
					RGBTOHSV(aColorRGB[0],aColorRGB[1],aColorRGB[2],cx,cy,cz);
					RGBTOHSV(refColorRGB[0],refColorRGB[1],refColorRGB[2],cxref,cyref,czref);
					czref = refColor[1]; //set V to luminance
					cz = aColor[1];
				}
				if (cxref > .01)
					m_redValue=(float)(100.-(cxref-cx)/cxref*100.0);
				else
					m_redValue=(abs(cxref-cx)<0.3)?(float)(100.-(cxref-cx)*100.0):(float)(100.-(cxref+1.0-cx)*100.0);
				if (cyref > .01)
					m_greenValue=(float)(100.-(cyref-cy)/cyref*100.0);
				else
					m_greenValue=(float)(100.-(cyref-cy)*100.0);
				if (czref > .01)
					m_blueValue=(float)(100.-(czref-cz)/czref*100.0);
				else
					m_blueValue=(float)(100.-(czref-cz)*100.0);                        
			}
		}

		double RefWhite = 1.0;
		if ( isHDR )
		{
			bool shiftDiffuse = (abs(GetConfig()->m_DiffuseL-94.0)>0.5);
			if (DVD)
			{
				if (m_displayMode == 1)
				{
						tmWhite = getL_EOTF(0.50, noDataColor, noDataColor, GetConfig()->m_GammaRel, GetConfig()->m_Split, 5, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) * 100.0;								
						if ( (cRef.m_standard == UHDTV2 || cRef.m_standard == HDTV || cRef.m_standard == UHDTV || minCol == 7) ) //fix for P3/Mascior
							RefWhite = YWhite / (!shiftDiffuse?92.254965:tmWhite);
				else
				{
						RefWhite = YWhite / (tmWhite);
						YWhite = YWhite * 94.37844 / (tmWhite);
				}
			}
			else
			{
				if ( ((cRef.m_standard == UHDTV2 && minCol == satsize ) || cRef.m_standard == HDTV || cRef.m_standard == UHDTV)  && m_displayMode != 11 && m_displayMode != 13)// && !shiftDiffuse) //fixes skin && nCol == satsize
					RefWhite = YWhite / (tmWhite);
				else if (m_displayMode == 13)
					// profile mode: unified convention even with the manual
					// generator (reference is GetHDRRefScale-scaled), measured
					// white unrescaled. tmWhiteRef, NOT tmWhite: the DVD branch
					// above clobbered tmWhite with the 0.50-code white, which
					// would leave a fixed ~2.3% bias against the reference.
					RefWhite = YWhite / (tmWhiteRef);
				else
				{
					RefWhite = YWhite / (tmWhite) ;
					YWhite = YWhite * 94.37844 / (tmWhite);
				}
			}
		}
		else
		{
			if (m_displayMode == 1)
			{
				if (cRef.m_standard == UHDTV2 || cRef.m_standard == HDTV || cRef.m_standard == UHDTV || minCol == 7)
					RefWhite = YWhite / (tmWhite) ;
				else
				{
					RefWhite = YWhite / (tmWhite) ;
					YWhite = YWhite * 94.37844 / (tmWhite) ;
				}
			}
			else
			{
				if (GetConfig()->m_CCMode >= MASCIOR50 && GetConfig()->m_CCMode <= CCMAXHDR && m_displayMode == 11)
					YWhite = m_pDocument->GetMeasure()->GetGray((m_pDocument->GetMeasure()->GetGrayScaleSize()-1)).GetY() ;
				else
				{
					// Unified HDR convention (matches the measures grid and the
					// 3D viewer): the reference is GetHDRRefScale-scaled, so the
					// measured white stays unrescaled - no 94.37844/tmWhite
					// adjust (identical with tone mapping off).
					RefWhite = YWhite / (tmWhite) ;
				}
			}
			}
		}
		m_dEValue = aReference.isValid()?float(m_pRefColor->GetDeltaE(YWhite, aReference, RefWhite, cRef, GetConfig()->m_dE_form, !m_bLumaMode,  GetConfig()->gw_Weight )):0 ;
    } //have valid m_prefcolor
	else
	{
		m_dEValue = 0.;
		//m_pRef
	}

	// Black keeps its dE where the normalisation makes it a chromaticity error,
	// which is the rule the balance chart and the grayscale grid cell already use:
	// m_dE_gray 2 / dE_form 5 set the target's luminance to the measured one, so
	// what is left is "is my black tinted". Under the others the target at black
	// is Y = 0 and the number would be an error against an ideal zero, so it stays
	// suppressed - as it does for a black with no light to have a chromaticity.
	BOOL bFirstCol = ( minCol == 1 && !m_bLumaMode && m_displayMode != 4 );
	BOOL bBlackDEMeaningful = ( bFirstCol
								&& ( m_displayMode == 0 || m_displayMode == 3 )
								&& ( GetConfig()->m_dE_gray == 2 || GetConfig()->m_dE_form == 5 )
								&& m_pRefColor != NULL && m_pRefColor->GetY() > 0.0 );

	if (m_dEValue > 40 || ( bFirstCol && !bBlackDEMeaningful ) ) m_dEValue = 0.;

	// With no target neither branch above ran, so the level members still hold the
	// previous column's numbers. OnDraw does not print them, but it does scale the
	// well off the largest of them - clear them so the empty wells are drawn the
	// same way every time.
	if ( !m_bHasReference )
		m_redValue = m_greenValue = m_blueValue = 0.f;

	Invalidate(FALSE);

	CString title;
	if (title.LoadString(m_bLumaMode ? (GetConfig()->m_useHSV?IDS_LCHLEVELS:IDS_RGBLEVELS) : IDS_RGBLEVELS))
		((CMainView *)GetParent())->m_RGBLevelsLabel.SetWindowText ((LPCSTR)title);
} 


BEGIN_MESSAGE_MAP(CRGBLevelWnd, CWnd)
	//{{AFX_MSG_MAP(CRGBLevelWnd)
	ON_WM_PAINT()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_WHATS_THIS, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CRGBLevelWnd message handlers


// Rounded-rectangle path (all four corners) for the level tracks and bars.
static void AddRoundRectPath(Gdiplus::GraphicsPath& p, float x, float y, float w, float h, float r)
{
	if ( r * 2.0f > w ) r = w * 0.5f;
	if ( r * 2.0f > h ) r = h * 0.5f;
	if ( r <= 0.0f ) { p.AddRectangle(Gdiplus::RectF(x, y, w, h)); return; }
	p.AddArc(x, y, r*2.0f, r*2.0f, 180.0f, 90.0f);
	p.AddArc(x+w-r*2.0f, y, r*2.0f, r*2.0f, 270.0f, 90.0f);
	p.AddArc(x+w-r*2.0f, y+h-r*2.0f, r*2.0f, r*2.0f, 0.0f, 90.0f);
	p.AddArc(x, y+h-r*2.0f, r*2.0f, r*2.0f, 90.0f, 90.0f);
	p.CloseFigure();
}

void CRGBLevelWnd::OnPaint()
{
	CPaintDC dc(this); // device context for painting
	CHMemDC pDC(&dc);

	CRect rect;
	GetClientRect(&rect);
	if ( rect.Width() <= 0 || rect.Height() <= 0 )
		return;

	BOOL bDark = GetConfig()->m_darkTheme;
	int dpiY = pDC->GetDeviceCaps(LOGPIXELSY);
	int dpiX = pDC->GetDeviceCaps(LOGPIXELSX);

	// Flat panel surface matching the app theme so the bars read as part of the
	// Selected color panel rather than a separate inset chart.
	COLORREF panelBg = FxGetMenuBgColor();
	pDC->FillSolidRect(&rect, panelBg);

	COLORREF trackClr  = bDark ? RGB(40,40,40)    : RGB(208,208,208);
	COLORREF valueClr  = bDark ? RGB(242,242,244) : RGB(35,35,40);
	COLORREF letterClr = bDark ? RGB(148,148,154) : RGB(112,114,120);
	// Bright on dark / dark on light so the reference line reads over both the
	// empty track and a bright filled bar.
	COLORREF dashClr   = bDark ? RGB(228,228,232) : RGB(64,64,68);

	// Every bar in this pane is reference-relative, so with no target there is
	// nothing here to draw - see the black column in Refresh.
	BOOL hasData = (m_pRefColor != NULL && m_pRefColor->isValid() && m_bHasReference);

	// Layout: four rounded tracks with the value and channel labels below them.
	float margin    = (float) MulDiv(4, dpiY, 96);
	float gap       = (float) MulDiv(9, dpiX, 96);
	float rad       = (float) MulDiv(5, dpiY, 96);
	float valuePx   = (float) MulDiv(12, dpiY, 96);
	float letterPx  = (float) MulDiv(11, dpiY, 96);
	float labelZone = valuePx + letterPx + (float) MulDiv(8, dpiY, 96);	// +2px so the nudged letter row isn't clipped

	float trackTop = margin;
	float trackBot = (float) rect.Height() - labelZone;
	float trackH   = trackBot - trackTop;
	float hmargin  = (float) MulDiv(8, dpiX, 96);   // left room so wide values (e.g. 129.8%) are not clipped
	float colW     = ((float) rect.Width() - hmargin - 3.0f*gap) / 4.0f;
	if ( trackH <= 8.0f || colW <= 8.0f )
		return;

	// 0-200% scale puts 100% exactly halfway up the track; if a channel exceeds
	// 200% the scale stretches so the tallest bar still fits. The dE track uses
	// its own 0-10 scale; its dashed reference line sits at the tolerance limit.
	float maxVal  = max(m_redValue, max(m_greenValue, m_blueValue));
	float yScale  = (maxVal < 200.0f) ? trackH / 200.0f : trackH / maxVal;
	float dEScale = trackH / 10.0f;		// full dE bar height = dE 10

	// dE tolerance bands (shared with the grid and target widget); the dashed
	// reference line marks the tolerance (yellow->red fail) limit, per preset.
	double deGood, deWarn;
	GetConfig()->GetDEThresholds(deGood, deWarn);

	float vals[4] = { m_redValue, m_greenValue, m_blueValue, (float) m_dEValue };
	COLORREF clrs[4];
	if ( m_bLumaMode && GetConfig()->m_useHSV )
	{
		clrs[0] = RGB(0,125,125); clrs[1] = RGB(125,0,125); clrs[2] = RGB(125,125,0);
	}
	else
	{
		clrs[0] = RGB(215,60,60); clrs[1] = RGB(65,190,80); clrs[2] = RGB(66,109,218); // B = #426DDA
	}
	// dE bar colours from the spec: green #83FF61, yellow #E7FAA3, red #D67C6A.
	clrs[3] = (m_dEValue < deGood) ? RGB(131,255,97)
	        : (m_dEValue < deWarn) ? RGB(231,250,163)
	                               : RGB(214,124,106);

	EnsureGdiplus();
	Gdiplus::Graphics g(pDC->GetSafeHdc());
	g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
	g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

	// Inset "well" styling (Figma spec): track fill + subtle dark border + a 1px
	// white bottom highlight; the fill bars get a subtle dark border of their own.
	Gdiplus::SolidBrush trackBrush(Gdiplus::Color(255, GetRValue(trackClr), GetGValue(trackClr), GetBValue(trackClr)));
	Gdiplus::Pen trackBorderPen(bDark ? Gdiplus::Color(34,255,255,255) : Gdiplus::Color(26,0,0,0), 1.0f);
	Gdiplus::Pen hlPen(bDark ? Gdiplus::Color(26,255,255,255) : Gdiplus::Color(115,255,255,255), 1.0f);
	Gdiplus::Pen barBorderPen(bDark ? Gdiplus::Color(70,0,0,0) : Gdiplus::Color(51,0,0,0), 1.0f);
	Gdiplus::Pen dashPen(Gdiplus::Color(205, GetRValue(dashClr), GetGValue(dashClr), GetBValue(dashClr)), 1.0f);
	float dashes[2] = { 4.0f, 4.0f };
	dashPen.SetDashPattern(dashes, 2);

	Gdiplus::Font valueFont(L"Segoe UI", valuePx, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	Gdiplus::Font letterFont(L"Segoe UI", letterPx, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	Gdiplus::SolidBrush valueBrush(Gdiplus::Color(255, GetRValue(valueClr), GetGValue(valueClr), GetBValue(valueClr)));
	Gdiplus::SolidBrush letterBrush(Gdiplus::Color(255, GetRValue(letterClr), GetGValue(letterClr), GetBValue(letterClr)));
	Gdiplus::StringFormat fmt(Gdiplus::StringFormatFlagsNoWrap | Gdiplus::StringFormatFlagsNoClip);
	fmt.SetAlignment(Gdiplus::StringAlignmentCenter);

	static const WCHAR* letters[4] = { L"R", L"G", L"B", L"\x0394" L"E" };

	for ( int i = 0; i < 4; i++ )
	{
		float x = hmargin + i * (colW + gap);

		// 1px white bottom highlight just under the track (the "0 1px 0" inset shadow).
		g.DrawLine(&hlPen, x + rad, trackBot + 1.0f, x + colW - rad, trackBot + 1.0f);

		Gdiplus::GraphicsPath track;
		AddRoundRectPath(track, x, trackTop, colW, trackH, rad);
		g.FillPath(&trackBrush, &track);
		g.DrawPath(&trackBorderPen, &track);

		// 100% for the channel bars, the dE tolerance (fail limit) for the dE
		// bar -- moves with the preset. Drawn on TOP of the bar below.
		float dashY = (i < 3) ? (trackBot - 100.0f * yScale) : (trackBot - (float)deWarn * dEScale);

		if ( hasData )
		{
			float v = (i < 3) ? vals[i] * yScale : vals[i] * dEScale;
			if ( v > trackH ) v = trackH;
			if ( v > 1.0f )
			{
				Gdiplus::GraphicsPath bar;
				AddRoundRectPath(bar, x, trackBot - v, colW, v, rad);
				Gdiplus::SolidBrush barBrush(Gdiplus::Color(255, GetRValue(clrs[i]), GetGValue(clrs[i]), GetBValue(clrs[i])));
				g.FillPath(&barBrush, &bar);
				g.DrawPath(&barBorderPen, &bar);
			}

			WCHAR wval[24];
			if ( i < 3 )
				swprintf_s(wval, L"%.1f%%", vals[i]);
			else
				swprintf_s(wval, L"%.1f", vals[i]);
			Gdiplus::RectF vr(x - gap, trackBot + (float) MulDiv(3, dpiY, 96), colW + 2.0f*gap, valuePx + 4.0f);
			g.DrawString(wval, -1, &valueFont, vr, &fmt, &valueBrush);
		}

		// Dashed reference line, over the bar and slightly translucent.
		g.DrawLine(&dashPen, x + 2.0f, dashY, x + colW, dashY);

		// letter row (R/G/B/dE) nudged 2px lower than the value row above it
		Gdiplus::RectF lr(x - gap*0.5f, trackBot + (float) MulDiv(6, dpiY, 96) + valuePx, colW + gap, letterPx + 4.0f);
		g.DrawString(letters[i], -1, &letterFont, lr, &fmt, &letterBrush);
	}
}

// EnsureGdiplus() now comes inline from Views/GdiPlusAA.h (via RGBLevelWnd.h),
// shared with the chart controls -- no local definition needed here.

void CRGBLevelWnd::OnContextMenu(CWnd* pWnd, CPoint point)
{
	// load and display popup menu
	CNewMenu menu;
	menu.LoadMenu(IDR_WHATS_THIS);
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup);
	
	pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL,
		point.x, point.y, this);
}

void CRGBLevelWnd::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_CTRL_RGBLEVELS, NULL );
}

