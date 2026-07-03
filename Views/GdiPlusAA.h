/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2026 Association Homecinema Francophone.  All rights reserved.
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

// GdiPlusAA.h : shared GDI+ helpers for anti-aliased chart rendering
//
#pragma once

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

// Initialise GDI+ once (process lifetime); never shut down, the OS reclaims it.
inline void EnsureGdiplus()
{
	static ULONG_PTR s_token = 0;
	if ( s_token == 0 )
	{
		Gdiplus::GdiplusStartupInput gdipInput;
		Gdiplus::GdiplusStartup(&s_token, &gdipInput, NULL);
	}
}

inline Gdiplus::Color GpColor(COLORREF clr, BYTE alpha = 255)
{
	return Gdiplus::Color(alpha, GetRValue(clr), GetGValue(clr), GetBValue(clr));
}

// GDI+ ignores the DC's logical window/viewport origins (e.g. the one CHMemDC
// sets for partial repaints), so mirror them onto the Graphics transform.
inline void GpApplyDCOrigin(Gdiplus::Graphics & g, CDC *pDC)
{
	CPoint wo = pDC->GetWindowOrg();
	CPoint vo = pDC->GetViewportOrg();
	if ( wo.x != vo.x || wo.y != vo.y )
		g.TranslateTransform((float)(vo.x - wo.x), (float)(vo.y - wo.y));
}

// Chip padding as a fraction of the font size (horizontal, vertical).
static const float STATCHIP_PADX_FRAC = 0.55f;
static const float STATCHIP_PADY_FRAC = 0.24f;

// Size of a stat chip (pill) for the given text, padding included.
inline Gdiplus::SizeF MeasureStatChip(Gdiplus::Graphics & g, const Gdiplus::Font & font, const WCHAR * text)
{
	Gdiplus::RectF bounds;
	g.MeasureString(text, -1, &font, Gdiplus::PointF(0.0f, 0.0f), &bounds);
	return Gdiplus::SizeF(bounds.Width + 2.0f * font.GetSize() * STATCHIP_PADX_FRAC,
						  bounds.Height + 2.0f * font.GetSize() * STATCHIP_PADY_FRAC);
}

// Draws a pill stat chip anchored by its bottom-right corner and returns its
// width (so a row of chips can grow leftward from the corner). Shared by the
// target widget and the CIE chart coverage readout.
inline Gdiplus::REAL DrawStatChip(Gdiplus::Graphics & g, const Gdiplus::Font & font, const WCHAR * text,
					 Gdiplus::REAL right, Gdiplus::REAL bottom,
					 const Gdiplus::Color & fill, const Gdiplus::Color & border, const Gdiplus::Color & textClr)
{
	Gdiplus::RectF bounds;
	g.MeasureString(text, -1, &font, Gdiplus::PointF(0.0f, 0.0f), &bounds);
	Gdiplus::REAL padX = font.GetSize() * STATCHIP_PADX_FRAC;
	Gdiplus::REAL padY = font.GetSize() * STATCHIP_PADY_FRAC;
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

// Anti-aliased drop-in for the CDC MoveTo/LineTo pattern used by the charts.
// Wraps a Graphics + Pen over the target DC and keeps the current position.
class CAAPolyline
{
public:
	CAAPolyline(CDC *pDC, float fWidth, COLORREF clr, int nPenStyle = PS_SOLID)
		: m_g((EnsureGdiplus(), pDC->GetSafeHdc()))
		, m_pen(GpColor(clr), fWidth)
	{
		// Dotted/dashed HAIRLINES stay un-antialiased: AA smears the 1px dots
		// across pixel rows instead of keeping them on their line. Wider
		// dashed strokes (e.g. the CIE reference gamut outline) do get AA.
		// Note: keep the default PixelOffsetMode -- it renders integer
		// coordinates on the same pixels as GDI; PixelOffsetModeHalf lands
		// half to a full pixel up-left of GDI-drawn grid lines.
		m_g.SetSmoothingMode(nPenStyle == PS_SOLID || fWidth > 1.5f
		                     ? Gdiplus::SmoothingModeAntiAlias
		                     : Gdiplus::SmoothingModeNone);
		GpApplyDCOrigin(m_g, pDC);
		switch ( nPenStyle )
		{
			case PS_DASH:    m_pen.SetDashStyle(Gdiplus::DashStyleDash); break;
			case PS_DOT:     m_pen.SetDashStyle(Gdiplus::DashStyleDot); break;
			case PS_DASHDOT: m_pen.SetDashStyle(Gdiplus::DashStyleDashDot); break;
			default: // PS_SOLID
				m_pen.SetStartCap(Gdiplus::LineCapRound);
				m_pen.SetEndCap(Gdiplus::LineCapRound);
				m_pen.SetLineJoin(Gdiplus::LineJoinRound);
				break;
		}
	}

	void MoveTo(CPoint pt)
	{
		m_cur = Gdiplus::PointF((float)pt.x, (float)pt.y);
	}

	void LineTo(CPoint pt)
	{
		Gdiplus::PointF next((float)pt.x, (float)pt.y);
		m_g.DrawLine(&m_pen, m_cur, next);
		m_cur = next;
	}

	Gdiplus::Graphics & Graphics() { return m_g; }
	Gdiplus::Pen & Pen() { return m_pen; }

private:
	Gdiplus::Graphics m_g;
	Gdiplus::Pen m_pen;
	Gdiplus::PointF m_cur;
};
