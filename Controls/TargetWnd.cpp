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

// TargetWnd.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "TargetWnd.h"
#include "Color.h"
#include "MainFrm.h"
#include "DataSetDoc.h"
#include "MainView.h"
#include "fxcolor.h"
#include <math.h>
#include <vector>
#include <algorithm>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTargetWnd

// Live instances, so a dE-tolerance change made on one target widget can
// repaint the others (e.g. the mini target in the main view).
static std::vector<CTargetWnd *> s_targetWnds;

CTargetWnd::CTargetWnd()
{
	s_targetWnds.push_back(this);
	m_deltax=0.0;
	m_deltay=0.0;
	m_deltaE=-1.0;
	m_clr = RGB(0,0,0);
	m_pRefColor = NULL;
    m_pDocument = NULL;
	centerXYZ = GetColorReference().GetWhite();
	nR = 0;
	nG = 0;
	nB = 0;
	m_trailCenter = centerXYZ;
	m_trailCol = -999;
	m_trailMode = -999;
	m_pBgBitmap = NULL;
	m_bgCx = -1;
	m_bgCy = -1;
	m_bgDark = -1;
	m_bgTol = -1.0;
}

CTargetWnd::~CTargetWnd()
{
	s_targetWnds.erase(std::remove(s_targetWnds.begin(), s_targetWnds.end(), this), s_targetWnds.end());
	delete m_pBgBitmap;
}

void CTargetWnd::Refresh(BOOL m_b16_235, int minCol, int nSize, int m_DisplayMode, CDataSetDoc * pDoc, int target)
{	
	if (m_pRefColor)
	{
		bool DVD = (GetConfig()->GetGeneratorType() == CColorHCFRConfig::enumManual);
		bool isUHDTV3 = GetConfig()->m_colorStandard == UHDTV3;
		bool isUHDTV4 = GetConfig()->m_colorStandard == UHDTV4;
		bool isHDR10 = GetConfig()->m_GammaOffsetType == 5;
		bool isHLG = GetConfig()->m_GammaOffsetType == 7;

		if (minCol > 0)
		{
			int		y1 = 0, y2 = 0, y3 = 0;
			double	x1, x2, x3, p1, p2, p3, z1, z2, z3;
			ColorRGBDisplay	GenColors [MAX_USER_CC_PATCH_SIZE + 10];
		
			if ((m_DisplayMode == 0 || m_DisplayMode == 2 || m_DisplayMode == 3 || m_DisplayMode == 4 ))
			{
				centerXYZ = GetColorReference().GetWhite();
	            if (nSize > 0)
				{
					if (m_DisplayMode == 0)
						// Grayscale: honor the explicit (possibly non-uniform) IRE levels,
						// matching what DisplayGray() actually shows/measures.
						p1 = pDoc->GetMeasure()->GetGrayPercent(minCol - 1, GetConfig()->m_bUseRoundDown, GetConfig()->m_bUse10bit) / 100.0;
					else if (isHDR10 && m_DisplayMode == 3)
						p1 = (double)(minCol - 1) * 2. / (double)(nSize-1);
					else
						p1 = (double)(minCol-1) / (double)(nSize-1);
				}
			    else
    				p1 =(double)(pDoc->GetMeasure()->m_NearWhiteClipCol+nSize+minCol-1) / (double)(100.);

				//fix 255->235 rounding errors that the generator will create
				x1 =  (int)floor(p1 * 255.0 + 0.5);
				y1 =  (int)floor(p1 * 219.0 + 0.5);
				z1= y1 - x1 / 255. * 219.;
				x1 += floor( z1 + 0.5);

				nR=(int)x1;
				nG=(int)x1;
				nB=(int)x1;
 				m_clr = RGB(nR,nG,nB);
			}
			else if (m_DisplayMode > 4 && m_DisplayMode < 12) //saturations
			{
				switch (m_DisplayMode)
				{
					case 5:
					centerXYZ =  pDoc->GetMeasure()->GetRefSat(0, double(minCol-1) / double(nSize - 1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb)).GetXYZValue();
					GenerateSaturationColors (GetColorReference(), GenColors, nSize, true, false, false, GetConfig()->m_GammaOffsetType);				
					p1=GenColors[minCol-1][0] / 100.;
					p2=GenColors[minCol-1][1] / 100.;
					p3=GenColors[minCol-1][2] / 100.;
					break;
				case 6:
					centerXYZ =  pDoc->GetMeasure()->GetRefSat(1, double(minCol-1) / double(nSize - 1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb)).GetXYZValue();
					GenerateSaturationColors (GetColorReference(), GenColors, nSize, false, true, false, GetConfig()->m_GammaOffsetType);				
					p1=GenColors[minCol-1][0] / 100.;
					p2=GenColors[minCol-1][1] / 100.;
					p3=GenColors[minCol-1][2] / 100.;
					break;
				case 7:
					centerXYZ =  pDoc->GetMeasure()->GetRefSat(2, double(minCol-1) / double(nSize - 1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb)).GetXYZValue();
					GenerateSaturationColors (GetColorReference(), GenColors, nSize, false, false, true,  GetConfig()->m_GammaOffsetType);				
					p1=GenColors[minCol-1][0] / 100.;
					p2=GenColors[minCol-1][1] / 100.;
					p3=GenColors[minCol-1][2] / 100.;
					break;
				case 8:
					centerXYZ =  pDoc->GetMeasure()->GetRefSat(3, double(minCol-1) / double(nSize-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb)).GetXYZValue();
					GenerateSaturationColors (GetColorReference(), GenColors, nSize, true, true, false, GetConfig()->m_GammaOffsetType);				
					p1=GenColors[minCol-1][0] / 100.;
					p2=GenColors[minCol-1][1] / 100.;
					p3=GenColors[minCol-1][2] / 100.;
					break;
				case 9:
					centerXYZ =  pDoc->GetMeasure()->GetRefSat(4, double(minCol-1) / double(nSize-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb)).GetXYZValue();
					GenerateSaturationColors (GetColorReference(), GenColors, nSize, false, true, true, GetConfig()->m_GammaOffsetType);				
					p1=GenColors[minCol-1][0] / 100.;
					p2=GenColors[minCol-1][1] / 100.;
					p3=GenColors[minCol-1][2] / 100.;
					break;
				case 10:
					centerXYZ =  pDoc->GetMeasure()->GetRefSat(5, double(minCol-1) / double(nSize-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb)).GetXYZValue();
					GenerateSaturationColors (GetColorReference(), GenColors, nSize, true, false, true, GetConfig()->m_GammaOffsetType);				
					p1=GenColors[minCol-1][0] / 100.;
					p2=GenColors[minCol-1][1] / 100.;
					p3=GenColors[minCol-1][2] / 100.;
					break;
				case 11:
				{
					CColor clr;
					pDoc->GetMeasure()->GetRefCC24Sat(minCol - 1, clr);
					centerXYZ = clr.GetXYZValue();
				}
					GenerateCC24Colors (GetColorReference(), GenColors, GetConfig()->m_CCMode, GetConfig()->m_GammaOffsetType);
					if (GetConfig()->m_CCMode == MCD)
					{
						if (minCol >= 7)
							minCol -= 6;
						else
							switch (minCol)
							{
								case 1:
									minCol = 24;
									break;
								case 2:
									minCol = 23;
									break;
								case 3:
									minCol = 22;
									break;
								case 4:
									minCol = 21;
									break;
								case 5:
									minCol = 20;
									break;
								case 6:
									minCol = 19;
									break;
							}
					}
					p1=GenColors[minCol-1][0] / 100.;
					p2=GenColors[minCol-1][1] / 100.;
					p3=GenColors[minCol-1][2] / 100.;
					break;
				default:
					p1=0.1;
					p2=0.2;
					p3=0.3;
				}

				x1 =  (int)floor(p1 * 255.0 + 0.5);
				x2 =  (int)floor(p2 * 255.0 + 0.5);
				x3 =  (int)floor(p3 * 255.0 + 0.5);
				y1 =  (int)floor(p1 * 219.0 + 0.5);
				y2 =  (int)floor(p2 * 219.0 + 0.5);
				y3 =  (int)floor(p3 * 219.0 + 0.5);
				z1 = y1 - x1 / 255. * 219.;
				z2 = y2 - x2 / 255. * 219.;
				z3 = y3 - x3 / 255. * 219.;
				x1 += floor( z1 + 0.5);
				x2 += floor( z2 + 0.5);
				x3 += floor( z3 + 0.5);

				nR=(int)x1;
				nG=(int)x2;
				nB=(int)x3;
				m_clr = RGB(nR,nG,nB);
			}
			else if ( m_DisplayMode == 1 && minCol == 1 )
			{
				centerXYZ = GetColorReference().GetRed();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
						nR = 174;
						nG = 51;
						nB = 51;
						m_clr = RGB(nR,nG,nB);
						break;
					case UHDTV3:
							nR = isHDR10?121:isHLG?242:225;
							nG = isHDR10?61:isHLG?82:64;
							nB = 0;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?116:isHLG?233:207;
							nG = isHDR10?68:isHLG?115:77;
							nB = isHDR10?44:isHLG?0:40;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 204;
							nG = 26;
							nB = 26;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = isHDR10?128:255;
							nG = 0;
							nB = 0;
   						m_clr = RGB(192,0,0);
						break;
				}
			}
			else if ( m_DisplayMode == 1 && minCol == 2)
			{
				centerXYZ = GetColorReference().GetGreen();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
							nR = 71;
							nG = 186;
							nB = 71;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV3:
							nR = isHDR10?90:isHLG?177:123;
							nG = isHDR10?127:isHLG?252:248;
							nB = isHDR10?45:isHLG?0:41;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?101:isHLG?202:155;
							nG = isHDR10?126:isHLG?251:246;
							nB = isHDR10?72:isHLG?131:85;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 77;
							nG = 204;
							nB = 77;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = 0;
							nG = isHDR10?128:255;
							nB = 0;
	   				  m_clr = RGB(0,192,0);
					  break;
				}
			}
			else if ( m_DisplayMode == 1 && minCol == 3)
			{
				centerXYZ = GetColorReference().GetBlue();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
							nR = 49;
							nG = 49;
							nB = 128;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV3:
							nR = isHDR10?62:isHLG?86:65;
							nG = isHDR10?41:isHLG?0:35;
							nB = isHDR10?128:isHLG?254:253;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?59:isHLG?77:62;
							nG = isHDR10?40:isHLG?0:34;
							nB = isHDR10?126:isHLG?250:242;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 128;
							nG = 128;
							nB = 204;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = 0;
							nG = 0;
							nB = isHDR10?128:255;
	   				  m_clr = RGB(0,0,192);
					  break;
				}
			}
			else if ( m_DisplayMode == 1 && minCol == 4)
			{
				centerXYZ = GetColorReference().GetYellow();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
							nR = 189;
							nG = 189;
							nB = 85;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV3:
							nR = isHDR10?127:isHLG?253:249;
							nG = isHDR10?128:isHLG?254:254;
							nB = isHDR10?44:isHLG?0:40;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?127:isHLG?253:250;
							nG = isHDR10?128:isHLG?254:254;
							nB = isHDR10?76:isHLG?141:92;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 204;
							nG = 204;
							nB = 26;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = isHDR10?128:255;
							nG = isHDR10?128:255;
							nB = 0;
	   				  m_clr = RGB(192,192,0);
					  break;
				}
			}
			else if ( m_DisplayMode == 1 && minCol == 5)
			{
				centerXYZ = GetColorReference().GetCyan();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
							nR = 92;
							nG = 186;
							nB = 186;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV3:
							nR = isHDR10?94:isHLG?188:135;
							nG = isHDR10?127:isHLG?253:249;
							nB = isHDR10?128:isHLG?255:255;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?104:isHLG?208:163;
							nG = isHDR10?126:isHLG?252:247;
							nB = isHDR10?128:isHLG?254:253;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 26;
							nG = 204;
							nB = 204;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = 0;
							nG = isHDR10?128:255;
							nB = isHDR10?128:255;
	   				  m_clr = RGB(0,192,192);
					  break;
				}
			}
			else if ( m_DisplayMode == 1 && minCol == 6)
			{
				centerXYZ = GetColorReference().GetMagenta();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
							nR = 164;
							nG = 74;
							nB = 164;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV3:
							nR = isHDR10?122:isHLG?245:231;
							nG = isHDR10?65:isHLG?103:71;
							nB = isHDR10?128:isHLG?254:253;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?118:isHLG?236:213;
							nG = isHDR10?71:isHLG?125:82;
							nB = isHDR10?126:isHLG?251:245;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 204;
							nG = 27;
							nB = 204;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = isHDR10?128:255;
							nG = 0;
							nB = isHDR10?128:255;
	   				  m_clr = RGB(192,0,192);
					  break;
				}
			}
			else if ( m_DisplayMode == 1 && minCol == 7 )
			{
				centerXYZ =  GetColorReference().GetWhite();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
							nR = 191;
							nG = 191;
							nB = 191;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV3:
							nR = isHDR10?128:255;
							nG = isHDR10?128:255;
							nB = isHDR10?128:255;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?128:255;
							nG = isHDR10?128:255;
							nB = isHDR10?128:255;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 255;
							nG = 255;
							nB = 255;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = isHDR10?128:255;
							nG = isHDR10?128:255;
							nB = isHDR10?128:255;
	   				  m_clr = RGB(255,255,255);
					  break;
				}
			}    //update RGB
		} //mincol > 0
		else if ( m_DisplayMode == 1 && m_pRefColor->GetDeltaxy(GetColorReference().GetRed(),GetColorReference()) < 0.05 && DVD ) 
			{
				centerXYZ = GetColorReference().GetRed();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
						nR = 174;
						nG = 51;
						nB = 51;
						m_clr = RGB(nR,nG,nB);
						break;
					case UHDTV3:
							nR = isHDR10?121:isHLG?242:225;
							nG = isHDR10?61:isHLG?82:64;
							nB = 0;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?116:isHLG?233:207;
							nG = isHDR10?68:isHLG?115:77;
							nB = isHDR10?44:isHLG?0:40;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 204;
							nG = 26;
							nB = 26;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = isHDR10?128:255;
							nG = 0;
							nB = 0;
   						m_clr = RGB(192,0,0);
						break;
				}
			}
			else if ( m_DisplayMode == 1 && m_pRefColor->GetDeltaxy(GetColorReference().GetGreen(),GetColorReference()) < 0.05 && GetConfig()->m_bDetectPrimaries && DVD )
			{
				centerXYZ = GetColorReference().GetGreen();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
							nR = 71;
							nG = 186;
							nB = 71;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV3:
							nR = isHDR10?90:isHLG?177:123;
							nG = isHDR10?127:isHLG?252:248;
							nB = isHDR10?45:isHLG?0:41;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?101:isHLG?202:155;
							nG = isHDR10?126:isHLG?251:246;
							nB = isHDR10?72:isHLG?131:85;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 77;
							nG = 204;
							nB = 77;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = 0;
							nG = isHDR10?128:255;
							nB = 0;
	   				  m_clr = RGB(0,192,0);
					  break;
				}
			}
			else if ( m_DisplayMode == 1 && m_pRefColor->GetDeltaxy(GetColorReference().GetBlue(),GetColorReference()) < 0.05 && GetConfig()->m_bDetectPrimaries && DVD )
			{
				centerXYZ = GetColorReference().GetBlue();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
							nR = 49;
							nG = 49;
							nB = 128;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV3:
							nR = isHDR10?62:isHLG?86:65;
							nG = isHDR10?41:isHLG?0:35;
							nB = isHDR10?128:isHLG?254:253;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?59:isHLG?77:62;
							nG = isHDR10?40:isHLG?0:34;
							nB = isHDR10?126:isHLG?250:242;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 128;
							nG = 128;
							nB = 204;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = 0;
							nG = 0;
							nB = isHDR10?128:255;
	   				  m_clr = RGB(0,0,192);
					  break;
				}
			}
			else if ( m_DisplayMode == 1 && m_pRefColor->GetDeltaxy(GetColorReference().GetYellow(),GetColorReference()) < 0.05 && GetConfig()->m_bDetectPrimaries && DVD )
			{
				centerXYZ = GetColorReference().GetYellow();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
							nR = 189;
							nG = 189;
							nB = 85;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV3:
							nR = isHDR10?127:isHLG?253:249;
							nG = isHDR10?128:isHLG?254:254;
							nB = isHDR10?44:isHLG?0:40;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?127:isHLG?253:250;
							nG = isHDR10?128:isHLG?254:254;
							nB = isHDR10?76:isHLG?141:92;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 204;
							nG = 204;
							nB = 26;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = isHDR10?128:255;
							nG = isHDR10?128:255;
							nB = 0;
	   				  m_clr = RGB(192,192,0);
					  break;
				}
			}
			else if ( m_DisplayMode == 1 && m_pRefColor->GetDeltaxy(GetColorReference().GetCyan(),GetColorReference()) < 0.05 && GetConfig()->m_bDetectPrimaries && DVD )
			{
				centerXYZ = GetColorReference().GetCyan();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
							nR = 92;
							nG = 186;
							nB = 186;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV3:
							nR = isHDR10?94:isHLG?188:135;
							nG = isHDR10?127:isHLG?253:249;
							nB = isHDR10?128:isHLG?255:255;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?104:isHLG?208:163;
							nG = isHDR10?126:isHLG?252:247;
							nB = isHDR10?128:isHLG?254:253;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 26;
							nG = 204;
							nB = 204;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = 0;
							nG = isHDR10?128:255;
							nB = isHDR10?128:255;
	   				  m_clr = RGB(0,192,192);
					  break;
				}
			}
			else if ( m_DisplayMode == 1 && m_pRefColor->GetDeltaxy(GetColorReference().GetMagenta(),GetColorReference()) < 0.05 && GetConfig()->m_bDetectPrimaries && DVD )
			{
				centerXYZ = GetColorReference().GetMagenta();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
							nR = 164;
							nG = 74;
							nB = 164;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV3:
							nR = isHDR10?122:isHLG?245:231;
							nG = isHDR10?65:isHLG?103:71;
							nB = isHDR10?128:isHLG?254:253;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?118:isHLG?236:213;
							nG = isHDR10?71:isHLG?125:82;
							nB = isHDR10?126:isHLG?251:245;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 204;
							nG = 26;
							nB = 204;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = isHDR10?128:255;
							nG = 0;
							nB = isHDR10?128:255;
	   				  m_clr = RGB(192,0,192);
					  break;
				}
			}
			else if ( m_DisplayMode == 1 && m_pRefColor->GetDeltaxy(GetColorReference().GetWhite(),GetColorReference()) < 0.05 && GetConfig()->m_bDetectPrimaries && DVD )
			{
				centerXYZ =  GetColorReference().GetWhite();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
							nR = 255;
							nG = 255;
							nB = 255;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV3:
							nR = isHDR10?128:255;
							nG = isHDR10?128:255;
							nB = isHDR10?128:255;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?128:255;
							nG = isHDR10?128:255;
							nB = isHDR10?128:255;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 191;
							nG = 191;
							nB = 191;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = isHDR10?128:255;
							nG = isHDR10?128:255;
							nB = isHDR10?128:255;
	   				  m_clr = RGB(255,255,255);
					  break;
				}
			}

		//Update test window for display when selected
		BOOL		bDisplayColor = GetConfig () -> m_bDisplayTestColors;

		if (bDisplayColor && target <= TARGET_ALL)
		{
			( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) ->m_wndTestColorWnd.m_colorPicker.SetColor ( RGB(nR,nG,nB) );
			( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) ->m_wndTestColorWnd.RedrawWindow ();
		}
		
		if (target >= TARGET_ALL)
		{
			//update RGB

			ColorxyY aColor = m_pRefColor -> GetxyYValue();
			ColorxyY centerxyY(centerXYZ);

			m_deltax = (aColor[0]-centerxyY[0])/centerxyY[0];
			m_deltay = (aColor[1]-centerxyY[1])/centerxyY[1];

			UpdateDeltaE();

			// Trail bookkeeping: a new target (patch, mode or reference change)
			// starts a fresh trail; a moved reading appends to it.
			if ( m_trailCol != minCol || m_trailMode != m_DisplayMode ||
				 fabs(m_trailCenter[0]-centerXYZ[0]) > 1e-9 ||
				 fabs(m_trailCenter[1]-centerXYZ[1]) > 1e-9 ||
				 fabs(m_trailCenter[2]-centerXYZ[2]) > 1e-9 )
			{
				m_trail.clear();
				m_trailCol = minCol;
				m_trailMode = m_DisplayMode;
				m_trailCenter = centerXYZ;
			}

			STrailPoint pt;
			pt.angle = atan2(m_deltay, m_deltax);
			pt.radius = RingRadius(m_deltaE);
			if ( m_trail.empty() || fabs(m_trail.back().angle - pt.angle) > 1e-6 ||
				 fabs(m_trail.back().radius - pt.radius) > 1e-6 )
			{
				m_trail.push_back(pt);
				while ( m_trail.size() > 5 )	// current read + up to 4 predecessors
					m_trail.pop_front();
			}

			Invalidate(TRUE);
		}
	} //have valid m_prefcolor
}


BEGIN_MESSAGE_MAP(CTargetWnd, CWnd)
	//{{AFX_MSG_MAP(CTargetWnd)
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_WHATS_THIS, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CTargetWnd message handlers

// Display name of the configured colour-difference formula (the combo in the
// advanced settings). "Recommended" computes CIE2000 for colour patches and
// CIE76uv for grayscale, so it is shown as the pair.
static const WCHAR * DeltaEFormulaName()
{
	switch ( GetConfig()->m_dE_form )
	{
		case 0: return L"CIE76uv";
		case 1: return L"CIE76ab";
		case 2: return L"CIE94";
		case 3: return L"CIE2000";
		case 4: return L"CMC";
		case 5: return L"CIE2000/76uv";
		case 6: return L"dICtCp";
	}
	return L"";
}

// Draws a pill chip anchored by its bottom-right corner and returns its width
// (so a row of chips can grow leftward from the corner).
static Gdiplus::REAL DrawChip(Gdiplus::Graphics & g, const Gdiplus::Font & font, const WCHAR * text,
					 Gdiplus::REAL right, Gdiplus::REAL bottom,
					 const Gdiplus::Color & fill, const Gdiplus::Color & border, const Gdiplus::Color & textClr)
{
	Gdiplus::RectF bounds;
	g.MeasureString(text, -1, &font, Gdiplus::PointF(0.0f, 0.0f), &bounds);
	Gdiplus::REAL padX = font.GetSize() * 0.55f;
	Gdiplus::REAL padY = font.GetSize() * 0.24f;
	Gdiplus::REAL w = bounds.Width + 2.0f * padX;
	Gdiplus::REAL h = bounds.Height + 2.0f * padY;
	Gdiplus::REAL x = right - w;
	Gdiplus::REAL y = bottom - h;
	Gdiplus::REAL r = h / 2.0f;	// pill
	Gdiplus::GraphicsPath path;
	path.AddArc(x, y, 2.0f * r, 2.0f * r, 90.0f, 180.0f);
	path.AddArc(x + w - 2.0f * r, y, 2.0f * r, 2.0f * r, 270.0f, 180.0f);
	path.CloseFigure();
	Gdiplus::SolidBrush fillBrush(fill);
	g.FillPath(&fillBrush, &path);
	Gdiplus::Pen borderPen(border, 1.0f);
	g.DrawPath(&borderPen, &path);
	Gdiplus::SolidBrush textBrush(textClr);
	g.DrawString(text, -1, &font, Gdiplus::PointF(x + padX, y + padY), &textBrush);
	return w;
}

void CTargetWnd::OnPaint()
{
	CPaintDC dc(this); // device context for painting
    CHMemDC pDC(&dc);

	CRect rect;
	GetClientRect(&rect);

	// windows problem with "negative" client height. Can occur when mainframe is really small
	if ( rect.bottom == 32767 )
		rect.bottom = 0;
	if ( rect.Width() <= 0 || rect.Height() <= 0 )
		return;

	FxEnsureGdiplus();
	BOOL bDark = GetConfig()->m_darkTheme;

	if ( !m_pBgBitmap || m_bgCx != rect.Width() || m_bgCy != rect.Height() || m_bgDark != bDark ||
		 m_bgTol != GetConfig()->m_dE_tolerance )
		RebuildBackground(rect, bDark);

	Gdiplus::Graphics g(pDC->GetSafeHdc());
	g.DrawImage(m_pBgBitmap, 0, 0, rect.Width(), rect.Height());

	if ( m_pRefColor == NULL || !m_pRefColor->isValid() )
		return;		// Draw nothing more

	g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

	double cx, cy, R;
	GetGeometry(rect, cx, cy, R);

	// Current read on the ring scale: direction = xy cast direction,
	// distance = dE.
	double curAngle = atan2(m_deltay, m_deltax);
	double curFrac = RingRadius(m_deltaE);
	Gdiplus::REAL curX = (Gdiplus::REAL)(cx + cos(curAngle) * curFrac * R);
	Gdiplus::REAL curY = (Gdiplus::REAL)(cy - sin(curAngle) * curFrac * R);

	// Fading trail of the previous reads, linked into the current dot
	// (the last stored point is the current read).
	int nTrail = (int)m_trail.size();
	if ( nTrail > 1 )
	{
		BYTE trailGray = bDark ? 210 : 80;
		Gdiplus::Pen linkPen(Gdiplus::Color(110, trailGray, trailGray, trailGray), 1.0f);
		linkPen.SetDashStyle(Gdiplus::DashStyleDash);
		Gdiplus::REAL trailR = (Gdiplus::REAL)max(2.5, 0.022 * R);
		Gdiplus::REAL prevX = 0.0f, prevY = 0.0f;
		for ( int i = 0; i < nTrail; i++ )
		{
			Gdiplus::REAL px = (Gdiplus::REAL)(cx + cos(m_trail[i].angle) * m_trail[i].radius * R);
			Gdiplus::REAL py = (Gdiplus::REAL)(cy - sin(m_trail[i].angle) * m_trail[i].radius * R);
			if ( i > 0 )
				g.DrawLine(&linkPen, prevX, prevY, px, py);
			if ( i < nTrail - 1 )
			{
				int alpha = 60 + 110 * (i + 1) / nTrail;
				Gdiplus::SolidBrush dotBrush(Gdiplus::Color((BYTE)alpha, trailGray, trailGray, trailGray));
				g.FillEllipse(&dotBrush, px - trailR, py - trailR, 2.0f * trailR, 2.0f * trailR);
			}
			prevX = px;
			prevY = py;
		}
	}

	// Measured dot, filled with the measured colour
	Gdiplus::REAL dotR = (Gdiplus::REAL)max(4.0, 0.055 * R);
	Gdiplus::Color fill(255, GetRValue(m_clr), GetGValue(m_clr), GetBValue(m_clr));
	Gdiplus::Color edge = bDark ? Gdiplus::Color(255, 240, 240, 240) : Gdiplus::Color(255, 50, 50, 50);
	if ( m_deltaE > 10.0 )
	{
		// off scale: hollow marker pinned just outside the outer ring
		Gdiplus::Pen fillPen(fill, (Gdiplus::REAL)max(2.0, dotR * 0.45));
		g.DrawEllipse(&fillPen, curX - dotR, curY - dotR, 2.0f * dotR, 2.0f * dotR);
		Gdiplus::Pen edgePen(edge, 1.0f);
		g.DrawEllipse(&edgePen, curX - dotR - 1.0f, curY - dotR - 1.0f, 2.0f * dotR + 2.0f, 2.0f * dotR + 2.0f);
	}
	else
	{
		Gdiplus::SolidBrush dotBrush(fill);
		g.FillEllipse(&dotBrush, curX - dotR, curY - dotR, 2.0f * dotR, 2.0f * dotR);
		Gdiplus::Pen edgePen(edge, 1.5f);
		g.DrawEllipse(&edgePen, curX - dotR, curY - dotR, 2.0f * dotR, 2.0f * dotR);
	}

	// Large widget only: stat chips bottom-right (dE tinted with the data
	// grid's highlight colours, xy offsets beside it) and the ring formula
	// note bottom-left.
	BOOL compact = min(rect.Width(), rect.Height()) < 220;
	if ( !compact )
	{
		g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
		Gdiplus::REAL chipFontPx = (Gdiplus::REAL)max(12.0, 0.052 * R + 1.0);
		Gdiplus::Font chipFont(L"Segoe UI", chipFontPx, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
		const Gdiplus::REAL pad = 8.0f;

		// Same colours and thresholds as the measures grid dE row (CIE76uv is
		// judged at 3/5, every other formula at 2/3), black text like the grid.
		bool is76uv = (GetConfig()->m_dE_form == 0);
		double tGood = is76uv ? 3.0 : 2.0;
		double tWarn = is76uv ? 5.0 : 3.0;
		COLORREF gridClr = bDark ? (m_deltaE < tGood ? RGB(98,187,78)   : (m_deltaE < tWarn ? RGB(206,188,71)  : RGB(232,84,84)))
								 : (m_deltaE < tGood ? RGB(175,255,175) : (m_deltaE < tWarn ? RGB(255,255,175) : RGB(255,175,175)));
		Gdiplus::Color chipFill(255, GetRValue(gridClr), GetGValue(gridClr), GetBValue(gridClr));
		Gdiplus::Color chipBorder(255, GetRValue(gridClr) * 70 / 100, GetGValue(gridClr) * 70 / 100, GetBValue(gridClr) * 70 / 100);
		Gdiplus::Color chipText(255, 0, 0, 0);

		WCHAR buf[64];
		Gdiplus::REAL xRight = (Gdiplus::REAL)rect.Width() - pad;
		Gdiplus::REAL yBottom = (Gdiplus::REAL)rect.Height() - pad;
		swprintf_s(buf, 64, L"dE %.1f", m_deltaE);
		Gdiplus::REAL wChip = DrawChip(g, chipFont, buf, xRight, yBottom, chipFill, chipBorder, chipText);

		Gdiplus::Color nFill   = bDark ? Gdiplus::Color(255, 42, 42, 42)    : Gdiplus::Color(255, 255, 255, 255);
		Gdiplus::Color nBorder = bDark ? Gdiplus::Color(255, 72, 72, 72)    : Gdiplus::Color(255, 205, 207, 213);
		Gdiplus::Color nText   = bDark ? Gdiplus::Color(255, 215, 215, 215) : Gdiplus::Color(255, 70, 74, 80);
		swprintf_s(buf, 64, L"dx %+.1f%%   dy %+.1f%%", m_deltax * 100.0, m_deltay * 100.0);
		DrawChip(g, chipFont, buf, xRight - wChip - 6.0f, yBottom, nFill, nBorder, nText);

		// ring formula note, plain muted text (localized prefix)
		Gdiplus::REAL noteFontPx = (Gdiplus::REAL)max(11.0, 0.055 * R);
		Gdiplus::Font noteFont(L"Segoe UI", noteFontPx, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
		CStringW noteFmt;
		if ( !noteFmt.LoadString(IDS_TARGET_RINGS) )
			noteFmt = L"Rings: dE %s";
		CStringW note;
		note.Format(noteFmt, DeltaEFormulaName());
		Gdiplus::RectF noteBounds;
		g.MeasureString(note, -1, &noteFont, Gdiplus::PointF(0.0f, 0.0f), &noteBounds);
		Gdiplus::SolidBrush noteBrush(bDark ? Gdiplus::Color(190, 205, 205, 205) : Gdiplus::Color(200, 95, 99, 106));
		g.DrawString(note, -1, &noteFont, Gdiplus::PointF(pad, yBottom - noteBounds.Height), &noteBrush);
	}
}

// Ring anchors: dE 1/3/5/10 sit at 25/50/75/100% of the scale radius,
// piecewise linear in between, pinned just outside the rim when off scale.
double CTargetWnd::RingRadius(double dE)
{
	if ( dE < 0.0 )
		return 0.0;
	if ( dE <= 1.0 )
		return 0.25 * dE;
	if ( dE <= 3.0 )
		return 0.25 + 0.25 * (dE - 1.0) / 2.0;
	if ( dE <= 5.0 )
		return 0.50 + 0.25 * (dE - 3.0) / 2.0;
	if ( dE <= 10.0 )
		return 0.75 + 0.25 * (dE - 5.0) / 5.0;
	return 1.04;
}

void CTargetWnd::GetGeometry(const CRect & rect, double & cx, double & cy, double & R) const
{
	cx = rect.Width() / 2.0;
	cy = rect.Height() / 2.0;
	double margin = max(8.0, 0.05 * min(rect.Width(), rect.Height()));
	R = min(rect.Width(), rect.Height()) / 2.0 - margin;
	if ( R < 10.0 )
		R = 10.0;
}

// The dE shown is the same number as the RGB-levels dE bar (full configured
// formula including the HDR handling), read from the sibling widget - both
// targets are CMainView children and the levels widget refreshes first. When
// the sibling has none (it zeroes dE for the first grayscale column), fall
// back to the plain formula against the current target.
void CTargetWnd::UpdateDeltaE()
{
	m_deltaE = -1.0;
	CWnd * pParent = GetParent();
	if ( pParent && pParent->IsKindOf(RUNTIME_CLASS(CMainView)) )
	{
		float dE = ((CMainView *)pParent)->m_RGBLevels.m_dEValue;
		if ( dE > 0.0f )
			m_deltaE = dE;
	}
	if ( m_deltaE < 0.0 && m_pRefColor && m_pRefColor->isValid() )
		m_deltaE = m_pRefColor->GetDeltaE(CColor(centerXYZ));
	if ( m_deltaE < 0.0 )
		m_deltaE = 0.0;
}

static void HueToRGB(double hue, double v, int & r, int & g, int & b)	// HSV -> RGB with S = 1, hue in [0,360)
{
	double h6 = hue / 60.0;
	int i = (int)h6;
	double f = h6 - i;
	double q = v * (1.0 - f);
	double t = v * f;
	double rd, gd, bd;
	switch ( i )
	{
		case 0:  rd = v;   gd = t;   bd = 0.0; break;
		case 1:  rd = q;   gd = v;   bd = 0.0; break;
		case 2:  rd = 0.0; gd = v;   bd = t;   break;
		case 3:  rd = 0.0; gd = q;   bd = v;   break;
		case 4:  rd = t;   gd = 0.0; bd = v;   break;
		default: rd = v;   gd = 0.0; bd = q;   break;
	}
	r = (int)(rd * 255.0 + 0.5);
	g = (int)(gd * 255.0 + 0.5);
	b = (int)(bd * 255.0 + 0.5);
}

void CTargetWnd::RebuildBackground(const CRect & rect, BOOL bDark)	// Cached: panel, hue wheel, rings, labels
{
	int w = rect.Width();
	int h = rect.Height();

	delete m_pBgBitmap;
	// DEBUG_NEW's placement form doesn't match Gdiplus::GdiplusBase::operator new
#ifdef _DEBUG
#undef new
#endif
	m_pBgBitmap = new Gdiplus::Bitmap(w, h, PixelFormat32bppPARGB);
#ifdef _DEBUG
#define new DEBUG_NEW
#endif
	m_bgTol = GetConfig()->m_dE_tolerance;
	m_bgCx = w;
	m_bgCy = h;
	m_bgDark = bDark;

	double cx, cy, R;
	GetGeometry(rect, cx, cy, R);

	// Panel gradient behind everything (matches the RGB level widget), with
	// the hue wheel blended in inside the circle: direction from center =
	// hue of the cast, neutral at the target, saturating toward the rim.
	int bgTop = bDark ? 26 : 250;
	int bgBot = bDark ? 12 : 235;
	double wheelAlpha = bDark ? 0.45 : 0.35;
	double wheelV = bDark ? 0.80 : 0.85;

	Gdiplus::BitmapData bd;
	Gdiplus::Rect lockRect(0, 0, w, h);
	if ( m_pBgBitmap->LockBits(&lockRect, Gdiplus::ImageLockModeWrite, PixelFormat32bppPARGB, &bd) == Gdiplus::Ok )
	{
		for ( int y = 0; y < h; y++ )
		{
			DWORD * row = (DWORD *)((BYTE *)bd.Scan0 + y * bd.Stride);
			int v = bgTop - (h > 1 ? (bgTop - bgBot) * y / (h - 1) : 0);
			int bgR = v, bgG = v, bgB = bDark ? v : min(255, v + 2);
			for ( int x = 0; x < w; x++ )
			{
				int r = bgR, gg = bgG, b = bgB;
				double fx = x - cx;
				double fy = cy - y;
				double rr = sqrt(fx * fx + fy * fy) / R;
				if ( rr <= 1.0 )
				{
					double a = atan2(fy, fx) * 180.0 / PI;
					if ( a < 0.0 )
						a += 360.0;
					// green up, red right, magenta down, cyan left (the old
					// widget's axis convention)
					double hue;
					if ( a < 90.0 )
						hue = a * (120.0 / 90.0);
					else if ( a < 180.0 )
						hue = 120.0 + (a - 90.0) * (60.0 / 90.0);
					else if ( a < 270.0 )
						hue = 180.0 + (a - 180.0) * (120.0 / 90.0);
					else
						hue = 300.0 + (a - 270.0) * (60.0 / 90.0);
					int hr, hg, hb;
					HueToRGB(hue, wheelV, hr, hg, hb);
					double alpha = wheelAlpha * (rr - 0.12) / 0.88;
					if ( alpha < 0.0 )
						alpha = 0.0;
					double aaEdge = (1.0 - rr) * R / 1.5;	// anti-alias the rim
					if ( aaEdge < 1.0 )
						alpha *= max(0.0, aaEdge);
					r  = (int)(r  + (hr - r ) * alpha);
					gg = (int)(gg + (hg - gg) * alpha);
					b  = (int)(b  + (hb - b ) * alpha);
				}
				row[x] = 0xFF000000 | (r << 16) | (gg << 8) | b;
			}
		}
		m_pBgBitmap->UnlockBits(&bd);
	}

	Gdiplus::Graphics gb(m_pBgBitmap);

	// panel border, crisp (no AA yet)
	Gdiplus::Pen borderPen(bDark ? Gdiplus::Color(255, 56, 56, 56) : Gdiplus::Color(255, 205, 207, 213), 1.0f);
	gb.DrawRectangle(&borderPen, 0, 0, w - 1, h - 1);

	gb.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	gb.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

	BOOL compact = min(w, h) < 220;

	// crosshair axes, under the rings
	Gdiplus::Pen axisPen(bDark ? Gdiplus::Color(70, 235, 235, 235) : Gdiplus::Color(60, 30, 30, 30), 1.0f);
	gb.DrawLine(&axisPen, (Gdiplus::REAL)(cx - R), (Gdiplus::REAL)cy, (Gdiplus::REAL)(cx + R), (Gdiplus::REAL)cy);
	gb.DrawLine(&axisPen, (Gdiplus::REAL)cx, (Gdiplus::REAL)(cy - R), (Gdiplus::REAL)cx, (Gdiplus::REAL)(cy + R));

	// dE scale rings at 1/3/5/10; the dashed tolerance ring is drawn on top
	// at the configured tolerance
	Gdiplus::Pen ringPen(bDark ? Gdiplus::Color(140, 130, 130, 130) : Gdiplus::Color(150, 165, 169, 178), 1.0f);
	static const double ringFrac[4] = { 0.25, 0.50, 0.75, 1.00 };
	for ( int i = 0; i < 4; i++ )
	{
		double rr = ringFrac[i] * R;
		gb.DrawEllipse(&ringPen, (Gdiplus::REAL)(cx - rr), (Gdiplus::REAL)(cy - rr), (Gdiplus::REAL)(2.0 * rr), (Gdiplus::REAL)(2.0 * rr));
	}

	Gdiplus::Pen tolPen(bDark ? Gdiplus::Color(255, 47, 191, 143) : Gdiplus::Color(255, 29, 158, 117), 1.4f);
	tolPen.SetDashStyle(Gdiplus::DashStyleDash);
	double tolR = RingRadius(m_bgTol) * R;
	gb.DrawEllipse(&tolPen, (Gdiplus::REAL)(cx - tolR), (Gdiplus::REAL)(cy - tolR), (Gdiplus::REAL)(2.0 * tolR), (Gdiplus::REAL)(2.0 * tolR));

	// center marker
	double ctrR = max(2.0, 0.014 * R);
	Gdiplus::SolidBrush ctrBrush(bDark ? Gdiplus::Color(255, 235, 235, 235) : Gdiplus::Color(255, 40, 40, 40));
	gb.FillEllipse(&ctrBrush, (Gdiplus::REAL)(cx - ctrR), (Gdiplus::REAL)(cy - ctrR), (Gdiplus::REAL)(2.0 * ctrR), (Gdiplus::REAL)(2.0 * ctrR));

	// dE ring labels along the upper-right diagonal (dropped in the small pane)
	if ( !compact )
	{
		gb.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
		Gdiplus::REAL fontPx = (Gdiplus::REAL)max(11.0, 0.068 * R);
		Gdiplus::Font labelFont(L"Segoe UI", fontPx, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
		Gdiplus::SolidBrush labelBrush(bDark ? Gdiplus::Color(210, 205, 205, 205) : Gdiplus::Color(220, 95, 99, 106));
		static const double labelFrac[4] = { 0.25, 0.50, 0.75, 1.00 };
		static const WCHAR * labelText[4] = { L"1", L"3", L"5", L"10" };
		const double diag = 0.70710678;
		for ( int i = 0; i < 4; i++ )
		{
			Gdiplus::PointF pos((Gdiplus::REAL)(cx + labelFrac[i] * R * diag + 2.0),
								(Gdiplus::REAL)(cy - labelFrac[i] * R * diag - fontPx - 2.0));
			gb.DrawString(labelText[i], -1, &labelFont, pos, &labelBrush);
		}
	}
}

void CTargetWnd::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
	Invalidate(FALSE);	// background rebuild happens lazily in OnPaint
}

BOOL CTargetWnd::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;	
}

void CTargetWnd::OnContextMenu(CWnd* pWnd, CPoint point)
{
	// load and display popup menu, extended with the dE tolerance presets
	// (the tolerance is also configurable in the advanced settings)
	static const double presets[] = { 0.5, 1.0, 1.5, 2.0, 3.0 };
	const int nPresets = sizeof(presets) / sizeof(presets[0]);

	CNewMenu menu;
	menu.LoadMenu(IDR_WHATS_THIS);
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup);

	CString tolLabel;
	if ( !tolLabel.LoadString(IDS_TARGET_TOLERANCE) )
		tolLabel = "dE tolerance";
	pPopup->AppendMenu(MF_SEPARATOR);
	for ( int i = 0; i < nPresets; i++ )
	{
		CString item;
		item.Format("%s %g", (LPCSTR)tolLabel, presets[i]);
		UINT flags = MF_STRING | ( fabs(GetConfig()->m_dE_tolerance - presets[i]) < 1e-6 ? MF_CHECKED : 0 );
		pPopup->AppendMenu(flags, ID_TARGET_TOL_FIRST + i, item);
	}

	int cmd = pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL | TPM_RETURNCMD,
		point.x, point.y, this);
	if ( cmd >= ID_TARGET_TOL_FIRST && cmd <= ID_TARGET_TOL_LAST )
	{
		GetConfig()->m_dE_tolerance = presets[cmd - ID_TARGET_TOL_FIRST];
		GetConfig()->WriteProfileDouble("Advanced","dE_tolerance",GetConfig()->m_dE_tolerance);
		// Repaint every live target widget, not just this one -- the mini
		// target in the main view shares the same tolerance ring.
		for ( size_t i = 0; i < s_targetWnds.size(); i++ )
			if ( ::IsWindow(s_targetWnds[i]->m_hWnd) )
				s_targetWnds[i]->Invalidate(FALSE);
	}
	else if ( cmd != 0 )
		SendMessage(WM_COMMAND, cmd);
}

void CTargetWnd::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_CTRL_TARGET, NULL );
}

