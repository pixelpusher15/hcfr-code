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

// Color3DView.h : rotatable/zoomable 3D viewer of measured color data.
//
// The measured point cloud, the target gamut solid, and the CIE chromaticity
// floor are all rasterized in software into a 32-bit DIB (a per-pixel z-buffer
// gives correct occlusion); GDI+ is used only for the wireframe/axes/labels on
// top. This scales to many thousands of points (the intended source for display
// profiling / 3D-LUT generation) at interactive rotation rates.
//
// Because the camera projection is orthographic, the gamut faces and the CIE
// tongue floor map to screen affinely, so the floor is a plain affine texture
// map rather than a perspective one.
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_COLOR3DVIEW_H__6F2A1B54_3D0E_4C77_9A21_9B7C0E3D5A10__INCLUDED_)
#define AFX_COLOR3DVIEW_H__6F2A1B54_3D0E_4C77_9A21_9B7C0E3D5A10__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "DocTempl.h"
#include <vector>
#include <string>

class CDataSetDoc;
class CColor;
class CColorReference;
class ColorXYZ;

class C3DColorView : public CSavingView
{
protected:
	C3DColorView();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(C3DColorView)

public:
	// Selectable plot space. The gamut solid and CIE floor share this notion.
	enum PlotSpace { SPACE_XYY = 0, SPACE_LAB = 1, SPACE_RGB = 2, SPACE_COUNT = 3 };

	CDataSetDoc * GetDocument() const { return (CDataSetDoc *) CView::GetDocument (); }

	virtual DWORD	GetUserInfo ();
	virtual void	SetUserInfo ( DWORD dwUserInfo );

	virtual void OnInitialUpdate();

	// dE display filter driven by the info-pane segments: 0 = show all,
	// 1 = hide points below the "good" threshold, 2 = below "warn"
	// (thresholds read live from the configured tolerance preset).
	void SetDEFilter(int filter);

protected:
	virtual void OnDraw(CDC* pDC);
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	virtual ~C3DColorView();

	// ---- scene: measured points in model space (roughly a unit cube) ----
	// Which CMeasure array a point came from (so a click can re-fetch the
	// original CColor, spectrum included, and push it to the main view).
	enum PointSource { SRC_GRAY = 0, SRC_NEARBLACK, SRC_NEARWHITE, SRC_PRIMARY,
					   SRC_SECONDARY, SRC_SAT, SRC_CC24, SRC_FREE };

	struct ScenePoint
	{
		float mx, my, mz;     // measured position
		float tx, ty, tz;     // target position (valid when hasTarget)
		float dE;             // delta E vs target (HCFR's configured formula)
		bool  hasTarget;
		DWORD trueColor;      // target/patch-colour swatch (heat colour is
							  // derived at render time from live thresholds)
		// inspection data (click-to-inspect readout)
		float mcx, mcy, mYr;  // measured chromaticity x,y + Y ratio (white=1)
		float tcx, tcy, tYr;  // target chromaticity x,y + Y ratio
		std::wstring label;   // e.g. "Gray 40%", "Red primary", "CC #7"
		BYTE  srcType;        // PointSource
		short srcA;           // index within the source array
		short srcB;           // SRC_SAT: hue 0-5 (R,G,B,Y,C,M)
	};
	std::vector<ScenePoint> m_points;
	bool m_sceneDirty;
	void BuildScene();
	// dETarget/ywForDE feed GetDeltaE with the grid's conventions; markerTarget
	// (relative to white=1) is where the tail/ring is drawn. Pass noDataColor
	// twice for measurements without a reference.
	void AppendMeasure(const CColor & c, double whiteY, CColorReference & ref,
					   const CColor & dETarget, const CColor & markerTarget,
					   bool isGS, double ywForDE, const wchar_t * label,
					   int srcType, int srcA, int srcB = 0);
	void PushSelectionToMainView(const ScenePoint & S);
	void ToModel(const ColorXYZ & xyz, double whiteY, CColorReference & ref,
				 double & mx, double & my, double & mz) const;

	// ---- target gamut solid, tessellated so edges/faces follow the real,
	//      curved gamut boundary in the current space (not straight cube edges) ----
	std::vector<float> m_edgeV;   // 12 edges * (N+1) verts * 3         (model coords)
	std::vector<float> m_faceV;   // 6 faces  * (N+1)*(N+1) verts * 3   (model coords)
	float m_baseV[3][3];          // xyY only: gamut-triangle base rim (R,G,B at floor)
	bool  m_baseValid;
	bool  m_gamutValid;
	void  BuildGamut(CColorReference & ref);

	// ---- CIE 1931 tongue floor texture (xyY mode) ----
	HDC     m_texDC;
	HBITMAP m_texBmp, m_texOld;
	DWORD * m_texBits;
	int     m_texW, m_texH;
	int     m_texSig;         // color-standard signature the texture was built for
	void    EnsureTongueTexture(CColorReference & ref);
	void    FreeTongueTexture();

	// ---- camera / view state ----
	int    m_space;
	double m_yaw, m_pitch, m_zoom;
	double m_panX, m_panY;        // screen-space pan offset (shift + left-drag)
	bool   m_showGamut, m_showFloor, m_shadeGamut;
	enum PointColorMode { PTCOLOR_DE = 0, PTCOLOR_TARGET = 1, PTCOLOR_PLAIN = 2 };
	int    m_pointColor;          // one of PointColorMode
	int    m_deFilter;            // see SetDEFilter
	bool   m_showTails;           // draw target cross + tail to each measured point
	bool   m_bDragging;
	CPoint m_lastMouse;
	CPoint m_downPos;             // to tell a click (select) from a drag (rotate)
	int    m_selected;            // index into m_points, -1 = none

	// ---- backbuffer: a top-down 32-bit DIB section + its memory DC ----
	HDC     m_memDC;
	HBITMAP m_memBmp, m_oldBmp;
	DWORD * m_bits;
	int     m_bw, m_bh;
	std::vector<float> m_zbuf;
	bool EnsureBackbuffer(int w, int h);
	void FreeBackbuffer();

	// ---- transient per-frame camera basis (set at the top of Render) ----
	double m_cx, m_cyc, m_scale, m_ry, m_rsy, m_rp, m_rsp;
	void ProjectModel(double mx, double my, double mz,
					  double & sx, double & sy, double & depth) const;
	// Points render as shaded orbs: a per-radius kernel of diffuse/specular
	// factors is precomputed once, so the per-pixel cost stays a multiply.
	struct OrbPx { short dx, dy; float shade; BYTE spec; };
	std::vector<OrbPx> m_orbKernel;
	int  m_orbKernelR;
	void BuildOrbKernel(int radius);
	void SplatOrb(int px, int py, float depth, DWORD color);
	void BlendPixel(int x, int y, float z, DWORD color, double alpha);              // z-tested, no z write
	void WuLine(double x0, double y0, float z0, double x1, double y1, float z1,
				DWORD color, double alpha);                                          // anti-aliased tail
	void SplatCross(int cx, int cy, float z, DWORD color, int arm);                  // target marker
	void RasterTriFlat(const double v[3][3], DWORD color, int alpha);   // translucent, z-test no write
	void RasterTriTex (const double v[3][3], const double uv[3][2]);    // opaque floor, z-test + write

	void Render(const CRect& rc);
	void SetSpace(int space);

	//{{AFX_MSG(C3DColorView)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}

#endif // !defined(AFX_COLOR3DVIEW_H__...)
