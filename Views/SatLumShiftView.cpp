/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2011 Association Homecinema Francophone.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////
//
//  This file is subject to the terms of the GNU General Public License as
//  published by the Free Software Foundation.  A copy of this license is
//  included with this software distribution in the file COPYING.htm. If you
//  do not have a copy, you may obtain a copy by writing to the Free
//  Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//2.22
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details
/////////////////////////////////////////////////////////////////////////////
//  Author(s):
//	François-Xavier CHABOUD
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// SatLumShiftView.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "DataSetDoc.h"
#include "DocTempl.h"
#include "SatLumShiftView.h"
#include "SaturationTracking.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// CSatLumShiftGrapher

CSatLumShiftGrapher::CSatLumShiftGrapher ()
{
	CString		Msg;

	Msg.LoadString ( IDS_PERCENTSATRED );
	m_redSatGraphID = m_graphCtrl.AddGraph(RGB(255,0,0), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_PERCENTSATGREEN );
	m_greenSatGraphID = m_graphCtrl.AddGraph(RGB(0,255,0), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_PERCENTSATBLUE );
	m_blueSatGraphID = m_graphCtrl.AddGraph(RGB(70,70,255), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_PERCENTSATYELLOW );
	m_yellowSatGraphID = m_graphCtrl.AddGraph(RGB(255,255,0), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_PERCENTSATCYAN );
	m_cyanSatGraphID = m_graphCtrl.AddGraph(RGB(0,255,255), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_PERCENTSATMAGENTA );
	m_magentaSatGraphID = m_graphCtrl.AddGraph(RGB(255,0,255), (LPSTR)(LPCSTR)Msg);

	Msg.LoadString ( IDS_PERCENTSATREDDATAREF );
	m_redSatDataRefGraphID = m_graphCtrl.AddGraph(RGB(255,0,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_PERCENTSATGREENDATAREF );
	m_greenSatDataRefGraphID = m_graphCtrl.AddGraph(RGB(0,255,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_PERCENTSATBLUEDATAREF );
	m_blueSatDataRefGraphID = m_graphCtrl.AddGraph(RGB(70,70,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_PERCENTSATYELLOWDATAREF );
	m_yellowSatDataRefGraphID = m_graphCtrl.AddGraph(RGB(255,255,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_PERCENTSATCYANDATAREF );
	m_cyanSatDataRefGraphID = m_graphCtrl.AddGraph(RGB(0,255,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_PERCENTSATMAGENTADATAREF );
	m_magentaSatDataRefGraphID = m_graphCtrl.AddGraph(RGB(255,0,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT);

	Msg.LoadString ( IDS_REFERENCE );
	m_refGraphID = m_graphCtrl.AddGraph(RGB(230,230,230), (LPSTR)(LPCSTR)Msg,1,PS_DOT);

	// Measured-sweep overlays for the anchor preview. Added after every older
	// series so ReadSettings, which stops at the first series it has no entry
	// for, keeps reading the older ones from an existing INI.
	static const UINT satTitle[6] = { IDS_PERCENTSATRED, IDS_PERCENTSATGREEN, IDS_PERCENTSATBLUE, IDS_PERCENTSATYELLOW, IDS_PERCENTSATCYAN, IDS_PERCENTSATMAGENTA };
	static const UINT deTitle[6] = { IDS_DELTAERED, IDS_DELTAEGREEN, IDS_DELTAEBLUE, IDS_DELTAEYELLOW, IDS_DELTAECYAN, IDS_DELTAEMAGENTA };
	static const COLORREF hueColor[6] = { RGB(255,0,0), RGB(0,255,0), RGB(70,70,255), RGB(255,255,0), RGB(0,255,255), RGB(255,0,255) };
	CString measFmt;
	measFmt.LoadString ( IDS_SATSHIFT_MEASURED );
	for ( int h = 0; h < 6; h++ )
	{
		CString name, title;
		name.LoadString ( satTitle[h] );
		title.Format ( measFmt, (LPCSTR) name );
		m_measSatGraphID[h] = m_graphCtrl.AddGraph(hueColor[h], (LPSTR)(LPCSTR)title,1,PS_DOT);
	}
	
	m_graphCtrl.SetXAxisProps("% Sat", 10, 0, 100);
	m_graphCtrl.SetYAxisProps("%", 5, -100, 100);
	m_graphCtrl.SetScale(0,100,-20,20);
	m_graphCtrl.ReadSettings("Saturation Shift Sat");

	Msg.LoadString ( IDS_DELTAERED );
	m_redColorGraphID = m_graphCtrl2.AddGraph(RGB(255,0,0), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_DELTAEGREEN );
	m_greenColorGraphID = m_graphCtrl2.AddGraph(RGB(0,255,0), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_DELTAEBLUE );
	m_blueColorGraphID = m_graphCtrl2.AddGraph(RGB(70,70,255), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_DELTAEYELLOW );
	m_yellowColorGraphID = m_graphCtrl2.AddGraph(RGB(255,255,0), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_DELTAECYAN );
	m_cyanColorGraphID = m_graphCtrl2.AddGraph(RGB(0,255,255), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_DELTAEMAGENTA );
	m_magentaColorGraphID = m_graphCtrl2.AddGraph(RGB(255,0,255), (LPSTR)(LPCSTR)Msg);

	Msg.LoadString ( IDS_DELTAEREDDATAREF );
	m_redColorDataRefGraphID = m_graphCtrl2.AddGraph(RGB(255,0,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_DELTAEGREENDATAREF );
	m_greenColorDataRefGraphID = m_graphCtrl2.AddGraph(RGB(0,255,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_DELTAEBLUEDATAREF );
	m_blueColorDataRefGraphID = m_graphCtrl2.AddGraph(RGB(70,70,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_DELTAEYELLOWDATAREF );
	m_yellowColorDataRefGraphID = m_graphCtrl2.AddGraph(RGB(255,255,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_DELTAECYANDATAREF );
	m_cyanColorDataRefGraphID = m_graphCtrl2.AddGraph(RGB(0,255,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_DELTAEMAGENTADATAREF );
	m_magentaColorDataRefGraphID = m_graphCtrl2.AddGraph(RGB(255,0,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT);

	for ( int h = 0; h < 6; h++ )
	{
		CString name, title;
		name.LoadString ( deTitle[h] );
		title.Format ( measFmt, (LPCSTR) name );
		m_measColorGraphID[h] = m_graphCtrl2.AddGraph(hueColor[h], (LPSTR)(LPCSTR)title,1,PS_DOT);
	}

	m_graphCtrl2.SetXAxisProps("% Sat", 10, 0, 100);
	m_graphCtrl2.SetYAxisProps("", 2, 0, 20);
	m_graphCtrl2.SetScale(0,100,0,20);
	m_graphCtrl2.ReadSettings("Saturation Shift Color");

	m_showReference=GetConfig()->GetProfileInt("Saturation Shift","Show Reference",TRUE);
	m_showRed=GetConfig()->GetProfileInt("Saturation Shift","Show Red",TRUE);
	m_showGreen=GetConfig()->GetProfileInt("Saturation Shift","Show Green",TRUE);
	m_showBlue=GetConfig()->GetProfileInt("Saturation Shift","Show Blue",TRUE);
	m_showYellow=GetConfig()->GetProfileInt("Saturation Shift","Show Yellow",TRUE);
	m_showCyan=GetConfig()->GetProfileInt("Saturation Shift","Show Cyan",TRUE);
	m_showMagenta=GetConfig()->GetProfileInt("Saturation Shift","Show Magenta",TRUE);
	m_showDataRef=GetConfig()->GetProfileInt("Saturation Shift","Show Reference Data",TRUE);

	m_anchor = SATSHIFT_ANCHOR_NONE;
	m_quantity = GetConfig()->GetProfileInt("Saturation Shift","Quantity",SATSHIFT_QTY_SAT);
	if ( m_quantity < SATSHIFT_QTY_SAT || m_quantity > SATSHIFT_QTY_LUM )
		m_quantity = SATSHIFT_QTY_SAT;
	// No saved scale is applied: open on this quantity's narrow preset, as
	// the Quantity menu and a document's saved quantity do.
	ApplyQuantityScale ();
}

// Y unit strings must outlive the graph control (it keeps the pointer)
static char g_unitPercent[] = "%";
static char g_unitDegree[] = "\xB0";

void CSatLumShiftGrapher::SetQuantityLabels ()
{
	UINT nTitle = IDS_GRAPH_SATSHIFT_SAT;
	m_graphCtrl.m_pYUnitStr = g_unitPercent;
	if ( m_quantity == SATSHIFT_QTY_HUE )
	{
		nTitle = IDS_GRAPH_SATSHIFT_HUE;
		m_graphCtrl.m_pYUnitStr = g_unitDegree;
	}
	else if ( m_quantity == SATSHIFT_QTY_LUM )
		nTitle = IDS_GRAPH_SATSHIFT_LUM;

	CString strTitle;
	strTitle.LoadString ( nTitle );
	m_graphCtrl.m_graphArray[0].p_Title=strTitle;
}

void CSatLumShiftGrapher::ApplyQuantityScale ()
{
	// The narrow preset of each quantity (the second Scale menu entry)
	if ( m_quantity == SATSHIFT_QTY_HUE )
	{
		m_graphCtrl.SetYAxisProps(g_unitDegree, 2, -180, 180);
		m_graphCtrl.SetYScale(-10,10);
	}
	else
	{
		m_graphCtrl.SetYAxisProps(g_unitPercent, 5, -100, 100);
		m_graphCtrl.SetYScale(-20,20);
	}
	SetQuantityLabels ();
}

void CSatLumShiftGrapher::UpdateGraph ( CDataSetDoc * pDoc )
{
	CDataSetDoc *pDataRef = GetDataRef();
	int size=pDoc->GetMeasure()->GetSaturationSize();

	if ( pDataRef )
	{
		// Check if data reference is comparable
		if ( pDataRef->GetMeasure()->GetSaturationSize() != size )
		{
			// Cannot use data reference
			pDataRef = NULL;
		}
	}

	if(IsWindow(m_graphCtrl.m_hWnd))
		m_graphCtrl.ShowWindow(SW_SHOW);
	if(IsWindow(m_graphCtrl2.m_hWnd))
		m_graphCtrl2.ShowWindow(SW_SHOW);

	m_graphCtrl.ClearGraph(m_refGraphID);
	
	if (m_showReference && m_refGraphID != -1 && size > 0)
	{	
		m_graphCtrl.AddPoint(m_refGraphID, 0, 0.0);
		m_graphCtrl.AddPoint(m_refGraphID, 100, 0.0);
	}

	// GetRefSat's hue order: 0=R 1=G 2=B 3=Y 4=C 5=M
	const struct { BOOL bShow; long satID, colorID, satRefID, colorRefID; } hues[6] =
	{
		{ m_showRed,     m_redSatGraphID,     m_redColorGraphID,     m_redSatDataRefGraphID,     m_redColorDataRefGraphID },
		{ m_showGreen,   m_greenSatGraphID,   m_greenColorGraphID,   m_greenSatDataRefGraphID,   m_greenColorDataRefGraphID },
		{ m_showBlue,    m_blueSatGraphID,    m_blueColorGraphID,    m_blueSatDataRefGraphID,    m_blueColorDataRefGraphID },
		{ m_showYellow,  m_yellowSatGraphID,  m_yellowColorGraphID,  m_yellowSatDataRefGraphID,  m_yellowColorDataRefGraphID },
		{ m_showCyan,    m_cyanSatGraphID,    m_cyanColorGraphID,    m_cyanSatDataRefGraphID,    m_cyanColorDataRefGraphID },
		{ m_showMagenta, m_magentaSatGraphID, m_magentaColorGraphID, m_magentaSatDataRefGraphID, m_magentaColorDataRefGraphID },
	};
	bool bDataRef = ( m_showDataRef && pDataRef != NULL && pDataRef != pDoc );

	// Anchor preview: the document's curves become the prediction and the
	// comparison document, if shown, stays as measured. The measured sweep is
	// drawn under the prediction only when the comparison is not, so a dotted
	// curve always means one thing.
	static const int anchorPercent[4] = { 0, 50, 75, 100 };
	int anchor = ( m_anchor > SATSHIFT_ANCHOR_NONE && m_anchor <= SATSHIFT_ANCHOR_100 ) ? anchorPercent[m_anchor] : 0;
	bool bMeasOverlay = ( anchor > 0 && ! bDataRef );
	static const UINT hueName[6] = { IDS_RED, IDS_GREEN, IDS_BLUE, IDS_YELLOW, IDS_CYAN, IDS_MAGENTA };
	CString strAvg, strAt100, strMax;
	if ( anchor > 0 )
	{
		CString fmt;
		fmt.LoadString ( IDS_SATSHIFT_TBL_AVG );
		strAvg.Format ( fmt, anchor );
		strAt100.LoadString ( IDS_SATSHIFT_TBL_AT100 );
		strMax.LoadString ( IDS_SATSHIFT_TBL_MAX );
	}
	m_graphCtrl.m_overlayRows.RemoveAll();

	for ( int h = 0; h < 6; h++ )
	{
		m_graphCtrl.ClearGraph(hues[h].satID);
		m_graphCtrl2.ClearGraph(hues[h].colorID);
		m_graphCtrl.ClearGraph(hues[h].satRefID);
		m_graphCtrl2.ClearGraph(hues[h].colorRefID);
		m_graphCtrl.ClearGraph(m_measSatGraphID[h]);
		m_graphCtrl2.ClearGraph(m_measColorGraphID[h]);
		if ( ! hues[h].bShow )
			continue;
		AnchorRow row;
		PlotHue ( pDoc, h, size, hues[h].satID, hues[h].colorID, anchor,
				  bMeasOverlay ? m_measSatGraphID[h] : -1, bMeasOverlay ? m_measColorGraphID[h] : -1, & row );
		if ( bDataRef )
			PlotHue ( pDataRef, h, size, hues[h].satRefID, hues[h].colorRefID );

		// One table row per shown color. H S L are the knob offsets the anchor
		// implies, i.e. the negated anchor errors.
		if ( anchor > 0 && row.valid )
		{
			CString name, line, last;
			name.LoadString ( hueName[h] );
			// "-": the last step's prediction left the spectrum locus
			if ( row.hasLast )
				last.Format ( "%.1f", row.lastDE );
			else
				last = "-";
			// Round before signing, so an offset under 0.05 prints +0.0, not -0.0
			double hKnob = floor ( -row.hueDeg * 10.0 + 0.5 ) / 10.0;
			if ( hKnob == 0.0 )
				hKnob = 0.0;
			line.Format ( "%s\t%s\t%.1f\t%s\t%s\t%s\t%.1f\tH\t%+.1f\tS\t%+d\tL\t%+d",
						  (LPCSTR) name, (LPCSTR) strAvg, row.avgDE, (LPCSTR) strAt100, (LPCSTR) last,
						  (LPCSTR) strMax, row.maxDE, hKnob,
						  (int) floor ( -row.satPct + 0.5 ), (int) floor ( -row.lumPct + 0.5 ) );
			m_graphCtrl.m_overlayRows.Add ( line );
		}
	}
}

static CColor GetSatMeasure ( CMeasure * pMeasure, int hue, int i )
{
	switch ( hue )
	{
		case 0:	return pMeasure->GetRedSat(i);
		case 1:	return pMeasure->GetGreenSat(i);
		case 2:	return pMeasure->GetBlueSat(i);
		case 3:	return pMeasure->GetYellowSat(i);
		case 4:	return pMeasure->GetCyanSat(i);
		default: return pMeasure->GetMagentaSat(i);
	}
}

// What PlotHue needs to evaluate one step, shared by the measured and the
// predicted colors so both go through exactly the same arithmetic.
struct SatShiftStepCtx
{
	int						quantity;
	double					xstart, ystart, dx, dy;		// 0% -> 100% reference line in xy
	double					uWhite, vWhite;				// 0% reference in u'v'
	double					toMeas;						// markScale-scaled reference -> measurement units
	ColorDENorm				norm;
	const CColorReference *	pRef;
	int						dE_form;
	int						gw;
};

static void EvalSatShiftStep ( const CColor & c, const CColor & ref, const SatShiftStepCtx & k, double & q, double & deltaE )
{
	ColorXYZ r = ref.GetXYZValue();
	ColorXYZ rScaled ( r[0] * k.norm.markScale, r[1] * k.norm.markScale, r[2] * k.norm.markScale );

	if ( k.quantity == SATSHIFT_QTY_HUE )
	{
		ColorXYZ cx = c.GetXYZValue();
		double um, vm, ut, vt;
		SatTrackXYZToUv ( cx[0], cx[1], cx[2], um, vm );
		SatTrackXYZToUv ( r[0], r[1], r[2], ut, vt );
		q = SatTrackHueErrorDeg ( um, vm, ut, vt, k.uWhite, k.vWhite );
	}
	else if ( k.quantity == SATSHIFT_QTY_LUM )
	{
		q = SatTrackLuminanceErrorPct ( c.GetY(), rScaled[1] * k.toMeas );
	}
	else
	{
		// Saturation shift: project the measured xy onto the 0% -> 100% line
		// and express its distance from the target along that line in percent
		ColorxyY m = c.GetxyYValue();
		ColorxyY t = ref.GetxyYValue();
		double kk = ( ( m[0] - k.xstart ) + ( k.ystart - m[1] ) * ( k.dx / k.dy ) ) / ( ( k.dx * k.dx / k.dy ) + k.dy );
		double xpoint = m[0] - kk * k.dy;
		q = ( xpoint - t[0] ) / k.dx * 100.0;
	}

	deltaE = c.GetDeltaE ( k.norm.whiteY, rScaled, k.norm.refWhite, *k.pRef, k.dE_form, false, k.gw );
}

// Plot one hue's saturation sweep from pSrc (the document, or the comparison
// document): the m_quantity value on m_graphCtrl and the dE on m_graphCtrl2.
// Each document is graded against its OWN references and whites.
//
// The references come from CMeasure::GetRefSat at the document's active
// stimulus level - the same call the measures grid and the 3D viewer make - and
// the dE normalisation from CMeasure::GetColorDENorm, the shared form of what
// the grid computes inline, so a patch reads the same dE here as in the grid
// (automatic generator; like GetColorDENorm, this does not model the grid's
// manual-generator carve-out).
// This view used to re-derive the reference itself, and that copy had drifted:
// it modelled P3/709-in-2020 patches in the inner space instead of the BT.2020
// transport they are sent in, which bends the hue under per-channel PQ tone
// mapping. The saturation axis is likewise this level's own 0% -> 100%
// reference: at reduced stimulus in PQ the 100% point's chromaticity moves, so
// a fixed full-stimulus endpoint showed a perfect patch as shifted.
//
// The three quantities:
//   saturation shift  the projection above, percent of the 0% -> 100% line;
//   hue error         SatTrackHueErrorDeg in u'v' about the 0% reference;
//   luminance error   measured Y against the reference Y in the units the dE
//                     compares them in: GetDeltaE divides the measurement by
//                     whiteY and the markScale-scaled reference by refWhite,
//                     so the target in measurement units is
//                     rScaled.Y * whiteY / refWhite (with GetDeltaE's own
//                     <= 0 fallbacks), in SDR and HDR alike.
//
// anchor (percent, 0 = none): the step nearest that saturation fixes a
// first-order CMS correction (SatTrackComputeAnchor; see SaturationTracking.h
// for what the model is and is not), every step is replaced by its corrected
// color (SatTrackApplyAnchor) and BOTH panes are recomputed from it through
// EvalSatShiftStep, so the dE pane shows the real dE of the predicted color.
// measSatID / measColorID, when not -1, receive the uncorrected sweep; pRow
// receives the table figures.
void CSatLumShiftGrapher::PlotHue ( CDataSetDoc * pSrc, int hue, int size, long satID, long colorID,
								   int anchor, long measSatID, long measColorID, AnchorRow * pRow )
{
	if ( pRow )
		pRow->valid = false;

	CMeasure * pMeasure = pSrc->GetMeasure();
	if ( satID == -1 || size <= 0 || ! GetSatMeasure ( pMeasure, hue, 0 ).isValid() )
		return;

	CColorHCFRConfig * cfg = GetConfig();
	bool bSpecial = ( cfg->m_colorStandard == HDTVa || cfg->m_colorStandard == HDTVb );
	const ColorDENorm norm = pMeasure->GetColorDENorm ( 5 + hue );
	// dE space and gray-world weight exactly as GetItemText's saturation branch
	CColorReference bRef = GetColorReference();
	if ( bRef.m_standard == UHDTV3 || bRef.m_standard == UHDTV4 )
		bRef = ContainerTransportReference ( bRef );
	else if ( bSpecial )
		bRef = CColorReference ( HDTV );
	int gw = ( cfg->m_GammaOffsetType == 5 ) ? 3 : cfg->gw_Weight;

	CColor white = pMeasure->GetRefSat ( hue, 0.0, bSpecial );
	ColorxyY start = white.GetxyYValue();
	ColorxyY end = pMeasure->GetRefSat ( hue, 1.0, bSpecial ).GetxyYValue();

	SatShiftStepCtx k;
	k.quantity = m_quantity;
	k.xstart = start[0];
	k.ystart = start[1];
	k.dx = end[0] - k.xstart;
	k.dy = end[1] - k.ystart;
	ColorXYZ w = white.GetXYZValue();
	SatTrackXYZToUv ( w[0], w[1], w[2], k.uWhite, k.vWhite );
	k.toMeas = ( norm.whiteY > 0.0 ? norm.whiteY : 120. ) / ( norm.refWhite > 0.0 ? norm.refWhite : 1.0 );
	k.norm = norm;
	k.pRef = & bRef;
	k.dE_form = cfg->m_dE_form;
	k.gw = gw;

	SatTrackAnchor anc;
	if ( anchor > 0 && size > 1 )
	{
		int iA = (int) floor ( anchor / 100.0 * ( size - 1 ) + 0.5 );
		CColor mA = GetSatMeasure ( pMeasure, hue, iA );
		CColor rA = pMeasure->GetRefSat ( hue, (double) iA / (double) ( size - 1 ), bSpecial );
		if ( mA.isValid() && rA.isValid() )
		{
			ColorXYZ m = mA.GetXYZValue();
			ColorXYZ r = rA.GetXYZValue();
			double s = norm.markScale * k.toMeas;
			double measXYZ[3] = { m[0], m[1], m[2] };
			double targetXYZ[3] = { r[0] * s, r[1] * s, r[2] * s };
			anc = SatTrackComputeAnchor ( measXYZ, targetXYZ, k.uWhite, k.vWhite );
		}
	}

	double sumDE = 0.0, maxDE = 0.0, lastDE = 0.0;
	int nAvg = 0;
	bool bLast = false;
	for ( int i = 0; i < size; i++ )
	{
		CColor meas = GetSatMeasure ( pMeasure, hue, i );
		CColor ref = pMeasure->GetRefSat ( hue, (double) i / (double) ( size - 1 ), bSpecial );
		if ( ! meas.isValid() || ! ref.isValid() )
			continue;

		double q, deltaE;
		if ( ! anc.valid )
		{
			EvalSatShiftStep ( meas, ref, k, q, deltaE );
			m_graphCtrl.AddPoint(satID, i*100/(size-1), q);
			m_graphCtrl2.AddPoint(colorID, i*100/(size-1), deltaE);
			continue;
		}

		ColorXYZ m = meas.GetXYZValue();
		double inXYZ[3] = { m[0], m[1], m[2] }, outXYZ[3];
		// No prediction for a step the correction pushes past the spectrum
		// locus (see SatTrackApplyAnchor): it gets no point and no say in the
		// table, rather than a meaningless dE
		if ( SatTrackApplyAnchor ( anc, inXYZ, outXYZ ) )
		{
			CColor pred ( ColorXYZ ( outXYZ[0], outXYZ[1], outXYZ[2] ) );

			EvalSatShiftStep ( pred, ref, k, q, deltaE );
			m_graphCtrl.AddPoint(satID, i*100/(size-1), q);
			m_graphCtrl2.AddPoint(colorID, i*100/(size-1), deltaE);

			if ( i > 0 && i * 100 <= anchor * ( size - 1 ) )
			{
				sumDE += deltaE;
				nAvg++;
			}
			if ( deltaE > maxDE )
				maxDE = deltaE;
			if ( i == size - 1 )
			{
				lastDE = deltaE;
				bLast = true;
			}
		}

		if ( measSatID != -1 )
		{
			double qm, deltaEm;
			EvalSatShiftStep ( meas, ref, k, qm, deltaEm );
			m_graphCtrl.AddPoint(measSatID, i*100/(size-1), qm);
			m_graphCtrl2.AddPoint(measColorID, i*100/(size-1), deltaEm);
		}
	}

	if ( pRow && anc.valid && nAvg > 0 )
	{
		pRow->valid = true;
		pRow->avgDE = sumDE / nAvg;
		pRow->hasLast = bLast;
		pRow->lastDE = lastDE;
		pRow->maxDE = maxDE;
		pRow->hueDeg = anc.hueDeg;
		pRow->satPct = anc.satPct;
		pRow->lumPct = anc.lumPct;
	}
}

/////////////////////////////////////////////////////////////////////////////
// CSatLumShiftView

IMPLEMENT_DYNCREATE(CSatLumShiftView, CSavingView)

CSatLumShiftView::CSatLumShiftView()
	: CSavingView()
{
	//{{AFX_DATA_INIT(CSatLumShiftView)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	m_Grapher.m_anchor = GetConfig()->GetProfileInt("Saturation Shift","Anchor",SATSHIFT_ANCHOR_NONE);
	if ( m_Grapher.m_anchor < SATSHIFT_ANCHOR_NONE || m_Grapher.m_anchor > SATSHIFT_ANCHOR_100 )
		m_Grapher.m_anchor = SATSHIFT_ANCHOR_NONE;
}
 
CSatLumShiftView::~CSatLumShiftView()
{
}
 
BEGIN_MESSAGE_MAP(CSatLumShiftView, CSavingView)
	//{{AFX_MSG_MAP(CSatLumShiftView)
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_LUM_GRAPH_SHOWREF, OnSatGraphShowRef)
	ON_COMMAND(IDM_LUM_GRAPH_REDSHIFT, OnSatGraphRed)
	ON_COMMAND(IDM_LUM_GRAPH_GREENSHIFT, OnSatGraphGreen)
	ON_COMMAND(IDM_LUM_GRAPH_BLUESHIFT, OnSatGraphBlue)
	ON_COMMAND(IDM_LUM_GRAPH_YELLOWSHIFT, OnSatGraphYellow)
	ON_COMMAND(IDM_LUM_GRAPH_CYANSHIFT, OnSatGraphCyan)
	ON_COMMAND(IDM_LUM_GRAPH_MAGENTASHIFT, OnSatGraphMagenta)
	ON_COMMAND(IDM_LUM_GRAPH_DATAREF, OnSatGraphShowDataRef)
	ON_COMMAND(IDM_GRAPH_SETTINGS, OnGraphSettings)
	ON_COMMAND(IDM_GRAPH_SAVE, OnGraphSave)
	ON_COMMAND(IDM_GRAPH_SCALE_FIT, OnGraphScaleFit)
	ON_COMMAND(IDM_LUMINANCE_GRAPH_Y_SCALE1, OnLuminanceGraphYScale1)
	ON_COMMAND(IDM_LUMINANCE_GRAPH_Y_SCALE2, OnLuminanceGraphYScale2)
	ON_COMMAND(IDM_GRAPH_Y_SHIFT_BOTTOM, OnGraphYShiftBottom)
	ON_COMMAND(IDM_GRAPH_Y_SHIFT_TOP, OnGraphYShiftTop)
	ON_COMMAND(IDM_GRAPH_Y_ZOOM_IN, OnGraphYZoomIn)
	ON_COMMAND(IDM_GRAPH_Y_ZOOM_OUT, OnGraphYZoomOut)
	ON_COMMAND(IDM_GRAPH_Y_SCALE_FIT, OnGraphYScaleFit)
	ON_COMMAND(IDM_GRAPH_SCALE_CUSTOM, OnGraphScaleCustom)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SCALE1, OnDeltaEGraphYScale1)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SCALE2, OnDeltaEGraphYScale2)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SCALE_FIT, OnDeltaEGraphYScaleFit)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SHIFT_BOTTOM, OnDeltaEGraphYShiftBottom)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SHIFT_TOP, OnDeltaEGraphYShiftTop)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_ZOOM_IN, OnDeltaEGraphYZoomIn)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_ZOOM_OUT, OnDeltaEGraphYZoomOut)
	ON_COMMAND(IDM_DELTAE_GRAPH_SCALE_CUSTOM, OnDeltaEGraphScaleCustom)
	ON_COMMAND(IDM_HELP, OnHelp)
	//}}AFX_MSG_MAP
	ON_COMMAND_RANGE(IDM_SATSHIFT_QTY_SAT, IDM_SATSHIFT_QTY_LUM, OnSatShiftQuantity)
	ON_COMMAND_RANGE(IDM_SATSHIFT_ANCHOR_NONE, IDM_SATSHIFT_ANCHOR_100, OnSatShiftAnchor)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSatLumShiftView diagnostics

#ifdef _DEBUG
void CSatLumShiftView::AssertValid() const
{
	CSavingView::AssertValid();
}

void CSatLumShiftView::Dump(CDumpContext& dc) const
{
	CSavingView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CSatLumShiftView message handlers

void CSatLumShiftView::OnInitialUpdate() 
{
	CSavingView::OnInitialUpdate();

	CRect rect;
	GetClientRect(&rect);	// fill entire window

	m_Grapher.m_graphCtrl.Create(_T("Graph Window"), rect, this, IDC_LUMINANCEHISTO_GRAPH);
	m_Grapher.m_graphCtrl2.Create(_T("Graph Window"), rect, this, IDC_LUMINANCEHISTOLOG_GRAPH);

	OnSize(SIZE_RESTORED,rect.Width(),rect.Height());	// to size graph according to m_showDeltaE
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
	// Do nothing when not concerned
	switch ( lHint )
	{
//		case UPD_GRAYSCALE: //used as catchall for preferences change
		case UPD_NEARBLACK:
		case UPD_NEARWHITE:
		case UPD_CONTRAST:
		case UPD_FREEMEASURES:
		case UPD_GENERATORCONFIG:
		case UPD_SELECTEDCOLOR:
			 return;
	}

	m_Grapher.UpdateGraph ( GetDocument () );

	RedrawWindow( NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW );
}

// User info bits: 0 reference line, 1-6 the six colors (R G B Y C M), 7 the
// comparison document, 8-9 the top-pane quantity (SATSHIFT_QTY_*), 10-11 the
// anchor (SATSHIFT_ANCHOR_*). Bits 12-31 are free. A document saved before
// bits 8-11 existed carries zeros there: saturation shift, no anchor.
DWORD CSatLumShiftView::GetUserInfo ()
{
	return	( ( m_Grapher.m_showReference	& 0x0001 )	<< 0 )
		  + ( ( m_Grapher.m_showRed			& 0x0001 )	<< 1 )
		  + ( ( m_Grapher.m_showGreen		& 0x0001 )	<< 2 )
		  + ( ( m_Grapher.m_showBlue		& 0x0001 )	<< 3 )
		  + ( ( m_Grapher.m_showYellow		& 0x0001 )	<< 4 )
		  + ( ( m_Grapher.m_showCyan		& 0x0001 )	<< 5 )
		  + ( ( m_Grapher.m_showMagenta		& 0x0001 )	<< 6 )
		  + ( ( m_Grapher.m_showDataRef		& 0x0001 )	<< 7 )
		  + ( ( m_Grapher.m_quantity		& 0x0003 )	<< 8 )
		  + ( ( m_Grapher.m_anchor			& 0x0003 )	<< 10 );
}

void CSatLumShiftView::SetUserInfo ( DWORD dwUserInfo )
{
	m_Grapher.m_showReference	= ( dwUserInfo >> 0 ) & 0x0001;
	m_Grapher.m_showRed			= ( dwUserInfo >> 1 ) & 0x0001;
	m_Grapher.m_showGreen		= ( dwUserInfo >> 2 ) & 0x0001;
	m_Grapher.m_showBlue		= ( dwUserInfo >> 3 ) & 0x0001;
	m_Grapher.m_showYellow		= ( dwUserInfo >> 4 ) & 0x0001;
	m_Grapher.m_showCyan		= ( dwUserInfo >> 5 ) & 0x0001;
	m_Grapher.m_showMagenta		= ( dwUserInfo >> 6 ) & 0x0001;
	m_Grapher.m_showDataRef		= ( dwUserInfo >> 7 ) & 0x0001;

	int quantity = ( dwUserInfo >> 8 ) & 0x0003;
	if ( quantity > SATSHIFT_QTY_LUM )
		quantity = SATSHIFT_QTY_SAT;
	if ( quantity != m_Grapher.m_quantity )
	{
		// Per-document state: the INI keeps the user's last menu choice
		m_Grapher.m_quantity = quantity;
		m_Grapher.ApplyQuantityScale ();
	}
	m_Grapher.m_anchor = ( dwUserInfo >> 10 ) & 0x0003;
}

void CSatLumShiftView::OnSize(UINT nType, int cx, int cy) 
{
	CSavingView::OnSize(nType, cx, cy);
	
	if(IsWindow(m_Grapher.m_graphCtrl.m_hWnd))
		m_Grapher.m_graphCtrl.MoveWindow(0,0,cx,cy/2);
	if(IsWindow(m_Grapher.m_graphCtrl2.m_hWnd))
		m_Grapher.m_graphCtrl2.MoveWindow(0,cy/2,cx,cy/2);
}

void CSatLumShiftView::OnDraw(CDC* pDC) 
{
	// TODO: Add your specialized code here and/or call the base class
}

BOOL CSatLumShiftView::OnEraseBkgnd(CDC* pDC) 
{
	return TRUE;
}


void CSatLumShiftView::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	// load and display popup menu
	CNewMenu menu;
	menu.LoadMenu(IDR_SATLUMSHIFT_GRAPH_MENU);
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup);
	
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_SHOWREF, m_Grapher.m_showReference ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
 	pPopup->CheckMenuItem(IDM_LUM_GRAPH_DATAREF, m_Grapher.m_showDataRef ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND); //Ki
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_REDSHIFT, m_Grapher.m_showRed ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_GREENSHIFT, m_Grapher.m_showGreen ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_BLUESHIFT, m_Grapher.m_showBlue ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_YELLOWSHIFT, m_Grapher.m_showYellow ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_CYANSHIFT, m_Grapher.m_showCyan ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_MAGENTASHIFT, m_Grapher.m_showMagenta ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);

	// One of three quantities, one of four anchors (MF_BYCOMMAND reaches the
	// Anchor submenu)
	for ( int q = SATSHIFT_QTY_SAT; q <= SATSHIFT_QTY_LUM; q++ )
		pPopup->CheckMenuItem(IDM_SATSHIFT_QTY_SAT + q, MF_BYCOMMAND | ( m_Grapher.m_quantity == q ? MF_CHECKED : MF_UNCHECKED ));
	for ( int a = SATSHIFT_ANCHOR_NONE; a <= SATSHIFT_ANCHOR_100; a++ )
		pPopup->CheckMenuItem(IDM_SATSHIFT_ANCHOR_NONE + a, MF_BYCOMMAND | ( m_Grapher.m_anchor == a ? MF_CHECKED : MF_UNCHECKED ));

	// The hue error's two presets share the saturation presets' commands;
	// their labels are numbers and a degree sign, the same in every language
	if ( m_Grapher.m_quantity == SATSHIFT_QTY_HUE )
	{
		menu.SetMenuText(IDM_LUMINANCE_GRAPH_Y_SCALE1, "-30\xB0 / +30\xB0", MF_BYCOMMAND);
		menu.SetMenuText(IDM_LUMINANCE_GRAPH_Y_SCALE2, "-10\xB0 / +10\xB0", MF_BYCOMMAND);
	}

	pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL, point.x, point.y, this);
}

void CSatLumShiftView::OnSatGraphShowRef() 
{
	m_Grapher.m_showReference = !m_Grapher.m_showReference;
	GetConfig()->WriteProfileInt("Saturation Shift","Show Reference",m_Grapher.m_showReference);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnSatGraphRed() 
{
	m_Grapher.m_showRed = !m_Grapher.m_showRed;
	GetConfig()->WriteProfileInt("Saturation Shift","Show Red",m_Grapher.m_showRed);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnSatGraphGreen() 
{
	m_Grapher.m_showGreen = !m_Grapher.m_showGreen;
	GetConfig()->WriteProfileInt("Saturation Shift","Show Green",m_Grapher.m_showGreen);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnSatGraphBlue() 
{
	m_Grapher.m_showBlue = !m_Grapher.m_showBlue;
	GetConfig()->WriteProfileInt("Saturation Shift","Show Blue",m_Grapher.m_showBlue);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnSatGraphYellow() 
{
	m_Grapher.m_showYellow = !m_Grapher.m_showYellow;
	GetConfig()->WriteProfileInt("Saturation Shift","Show Yellow",m_Grapher.m_showYellow);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnSatGraphCyan() 
{
	m_Grapher.m_showCyan = !m_Grapher.m_showCyan;
	GetConfig()->WriteProfileInt("Saturation Shift","Show Cyan",m_Grapher.m_showCyan);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnSatGraphMagenta() 
{
	m_Grapher.m_showMagenta = !m_Grapher.m_showMagenta;
	GetConfig()->WriteProfileInt("Saturation Shift","Show Magenta",m_Grapher.m_showMagenta);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnSatGraphShowDataRef()
{
	m_Grapher.m_showDataRef = !m_Grapher.m_showDataRef;
	GetConfig()->WriteProfileInt("Saturation Shift","Show Reference Data",m_Grapher.m_showDataRef);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnSatShiftQuantity(UINT nID)
{
	int quantity = (int) ( nID - IDM_SATSHIFT_QTY_SAT );
	if ( quantity == m_Grapher.m_quantity )
		return;
	m_Grapher.m_quantity = quantity;
	GetConfig()->WriteProfileInt("Saturation Shift","Quantity",quantity);
	m_Grapher.ApplyQuantityScale ();
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnSatShiftAnchor(UINT nID)
{
	int anchor = (int) ( nID - IDM_SATSHIFT_ANCHOR_NONE );
	if ( anchor == m_Grapher.m_anchor )
		return;
	m_Grapher.m_anchor = anchor;
	GetConfig()->WriteProfileInt("Saturation Shift","Anchor",anchor);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnGraphSave() 
{
	m_Grapher.m_graphCtrl.SaveGraphs(&m_Grapher.m_graphCtrl2);
}

void CSatLumShiftView::OnGraphSettings() 
{
	// Add delta E graph to first graph control to allow setting change 
	int tmpGraphID1=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_redColorGraphID]);
	int tmpGraphID2=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_greenColorGraphID]);
	int tmpGraphID3=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_blueColorGraphID]);
	int tmpGraphID4=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_yellowColorGraphID]);
	int tmpGraphID5=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_cyanColorGraphID]);
	int tmpGraphID6=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_magentaColorGraphID]);
	int tmpGraphID7=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_redColorDataRefGraphID]);
	int tmpGraphID8=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_greenColorDataRefGraphID]);
	int tmpGraphID9=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_blueColorDataRefGraphID]);
	int tmpGraphID10=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_yellowColorDataRefGraphID]);
	int tmpGraphID11=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_cyanColorDataRefGraphID]);
	int tmpGraphID12=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_magentaColorDataRefGraphID]);
	int tmpMeasID[6];
	for ( int h = 0; h < 6; h++ )
	{
		tmpMeasID[h]=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_measColorGraphID[h]]);
		m_Grapher.m_graphCtrl.ClearGraph ( tmpMeasID[h] );
	}

	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID1 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID2 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID3 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID4 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID5 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID6 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID7 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID8 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID9 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID10 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID11 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID12 );

	m_Grapher.m_graphCtrl.ChangeSettings();

	// Update m_Grapher.m_graphCtrl2 setting according to graph and values of tmpGraphIDs
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID1,m_Grapher.m_redColorGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID2,m_Grapher.m_greenColorGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID3,m_Grapher.m_blueColorGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID4,m_Grapher.m_yellowColorGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID5,m_Grapher.m_cyanColorGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID6,m_Grapher.m_magentaColorGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID7,m_Grapher.m_redColorDataRefGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID8,m_Grapher.m_greenColorDataRefGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID9,m_Grapher.m_blueColorDataRefGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID10,m_Grapher.m_yellowColorDataRefGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID11,m_Grapher.m_cyanColorDataRefGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID12,m_Grapher.m_magentaColorDataRefGraphID);
	for ( int h = 0; h < 6; h++ )
		m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpMeasID[h],m_Grapher.m_measColorGraphID[h]);

	for ( int h = 5; h >= 0; h-- )
		m_Grapher.m_graphCtrl.RemoveGraph(tmpMeasID[h]);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID12);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID11);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID10);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID9);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID8);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID7);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID6);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID5);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID4);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID3);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID2);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID1);

	// Parameters... saves itself: series colors, gradient, PDF User Scales
	m_Grapher.m_graphCtrl.WriteSettings("Saturation Shift Sat");
	m_Grapher.m_graphCtrl2.WriteSettings("Saturation Shift Color");

	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnGraphScaleFit() 
{
	m_Grapher.m_graphCtrl.FitXScale(TRUE);
	if ( m_Grapher.m_quantity == SATSHIFT_QTY_HUE )
		m_Grapher.m_graphCtrl.SetYAxisProps(m_Grapher.m_graphCtrl.m_pYUnitStr, 2, -180, 180);
	else
		m_Grapher.m_graphCtrl.SetYAxisProps(m_Grapher.m_graphCtrl.m_pYUnitStr, 5, -100, 100);
	m_Grapher.m_graphCtrl.FitYScale(TRUE,10);
	OnDeltaEGraphYScaleFit();
}

void CSatLumShiftView::OnLuminanceGraphYScale1() 
{
	if ( m_Grapher.m_quantity == SATSHIFT_QTY_HUE )
	{
		m_Grapher.m_graphCtrl.SetYScale(-30,30);
		m_Grapher.m_graphCtrl.SetYAxisProps(m_Grapher.m_graphCtrl.m_pYUnitStr, 5, -180, 180);
	}
	else
	{
		m_Grapher.m_graphCtrl.SetYScale(-100,100);
		m_Grapher.m_graphCtrl.SetYAxisProps(m_Grapher.m_graphCtrl.m_pYUnitStr, 20, -100, 100);
	}
	Invalidate(FALSE);
}

void CSatLumShiftView::OnLuminanceGraphYScale2() 
{
	if ( m_Grapher.m_quantity == SATSHIFT_QTY_HUE )
	{
		m_Grapher.m_graphCtrl.SetYScale(-10,10);
		m_Grapher.m_graphCtrl.SetYAxisProps(m_Grapher.m_graphCtrl.m_pYUnitStr, 2, -180, 180);
	}
	else
	{
		m_Grapher.m_graphCtrl.SetYScale(-20,20);
		m_Grapher.m_graphCtrl.SetYAxisProps(m_Grapher.m_graphCtrl.m_pYUnitStr, 5, -100, 100);
	}
	Invalidate(FALSE);
}

void CSatLumShiftView::OnGraphYScaleFit() 
{
	m_Grapher.m_graphCtrl.FitYScale(TRUE);
	Invalidate(FALSE);
}

void CSatLumShiftView::OnGraphYShiftBottom() 
{
	m_Grapher.m_graphCtrl.ShiftYScale(10);
	Invalidate(FALSE);
}

void CSatLumShiftView::OnGraphYShiftTop() 
{
	m_Grapher.m_graphCtrl.ShiftYScale(-10);
	Invalidate(FALSE);
}

void CSatLumShiftView::OnGraphYZoomIn() 
{
	m_Grapher.m_graphCtrl.GrowYScale(0,-10);
	Invalidate(FALSE);
}

void CSatLumShiftView::OnGraphYZoomOut() 
{
	m_Grapher.m_graphCtrl.GrowYScale(0,10);
	Invalidate(FALSE);
}

void CSatLumShiftView::OnGraphScaleCustom() 
{
	m_Grapher.m_graphCtrl.ChangeScale();
}

void CSatLumShiftView::OnDeltaEGraphYScale1() 
{
	m_Grapher.m_graphCtrl2.SetYScale(0,20);
	m_Grapher.m_graphCtrl2.SetYAxisProps("", 2, 0, 40);
	Invalidate(FALSE);
}

void CSatLumShiftView::OnDeltaEGraphYScale2() 
{
	m_Grapher.m_graphCtrl2.SetYScale(0,5);
	m_Grapher.m_graphCtrl2.SetYAxisProps("", 1, 0, 40);
	Invalidate(FALSE);
}

void CSatLumShiftView::OnDeltaEGraphYScaleFit() 
{
	m_Grapher.m_graphCtrl2.FitYScale(TRUE,1);
	m_Grapher.m_graphCtrl2.m_yAxisStep=(m_Grapher.m_graphCtrl2.m_maxY-m_Grapher.m_graphCtrl2.m_minY)/10;
	Invalidate(FALSE);
}

void CSatLumShiftView::OnDeltaEGraphYShiftBottom() 
{
	m_Grapher.m_graphCtrl2.ShiftYScale(1);
	Invalidate(FALSE);
}

void CSatLumShiftView::OnDeltaEGraphYShiftTop() 
{
	m_Grapher.m_graphCtrl2.ShiftYScale(-1);
	Invalidate(FALSE);
}

void CSatLumShiftView::OnDeltaEGraphYZoomIn() 
{
	m_Grapher.m_graphCtrl2.GrowYScale(0,-4);
	Invalidate(FALSE);
}

void CSatLumShiftView::OnDeltaEGraphYZoomOut() 
{
	m_Grapher.m_graphCtrl2.GrowYScale(0,+4);
	Invalidate(FALSE);
}

void CSatLumShiftView::OnDeltaEGraphScaleCustom() 
{
	m_Grapher.m_graphCtrl2.ChangeScale();
}

void CSatLumShiftView::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_SATSHIFT, NULL );
}

