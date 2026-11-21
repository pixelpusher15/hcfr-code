/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2012 Association Homecinema Francophone.  All rights reserved.
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
//  PhoenixF15E
/////////////////////////////////////////////////////////////////////////////

// PatchLabel.h: what color a patch-list row is, from its R, G, B codes and
// the list's bit depth and range - never from the row's name.
//
// The label is separate from the provenance role: the role says what a patch
// is for in a run (color, anchor, warm-up, verification); the label says what
// color it is. Pure C++, no MFC. The functions that take a CsvPatchRow or a
// CColor are declared in Color.h, which includes this header.
//
// Rules, applied to the codes the row actually drives (see ClassifyPatch):
//
//   Gray        R = G = B. The gray at the bottom of the nominal range is the
//               black, the gray at the top is the white. Black is gray, never
//               a primary at level 0. White is also the 0% end of every
//               saturation line; a later task finds it there by query, not by
//               a second label.
//   Primary     one channel at a level, the other two at black.
//   Secondary   two channels equal at a level, the third at black.
//   Saturation  on a primary's or secondary's line toward white: the two
//               lower channels equal (a primary's line) or the two upper
//               channels equal (a secondary's line), with the lower value
//               above black and below the upper one.
//   Other       everything else, including any row with three different codes.
//
// Extended range: a code below black or above white keeps its kind; its level
// is then under 0% or over 100%.

#if !defined(PATCHLABEL_H_INCLUDED_)
#define PATCHLABEL_H_INCLUDED_

// Values are going to be stored (a later task saves the label in the
// document): never renumber.
enum PatchLabelKind
{
	PATCH_KIND_UNLABELED  = -1,	// not computed: no definition behind it
	PATCH_KIND_OTHER      = 0,
	PATCH_KIND_GRAY       = 1,
	PATCH_KIND_PRIMARY    = 2,
	PATCH_KIND_SECONDARY  = 3,
	PATCH_KIND_SATURATION = 4
};

// Values are going to be stored: never renumber.
enum PatchLabelColor
{
	PATCH_COLOR_NONE    = -1,	// gray, other, unlabeled
	PATCH_COLOR_RED     = 0,
	PATCH_COLOR_GREEN   = 1,
	PATCH_COLOR_BLUE    = 2,
	PATCH_COLOR_YELLOW  = 3,
	PATCH_COLOR_CYAN    = 4,
	PATCH_COLOR_MAGENTA = 5
};

struct PatchLabel
{
	int    kind;          // PatchLabelKind
	int    color;         // PatchLabelColor: the primary or secondary of a
	                      // primary, secondary or saturation step
	double level;         // percent of the nominal range (CsvCodeToPercent):
	                      // gray = its code; primary = the lit channel;
	                      // secondary = the lit pair; saturation step = the
	                      // upper channel(s). 0 for other and unlabeled.
	double desaturation;  // saturation steps only, in the CODE domain: the
	                      // lower channel's percent over the upper channel's,
	                      // 0 < d < 1 (toward 0 = the full color, toward 1 =
	                      // gray). Not a perceptual saturation percentage.
	                      // 0 for every other kind.
	bool   isBlack;       // a gray at exactly 0%
	bool   isWhite;       // a gray at exactly 100%
	PatchLabel() : kind(PATCH_KIND_UNLABELED), color(PATCH_COLOR_NONE), level(0.0),
	               desaturation(0.0), isBlack(false), isWhite(false) {}
};

// Classifies one row. bits is 8 or 10 (any other value gives an UNLABELED
// label); range is a CSV_RANGE_* value (Color.h). CSV_RANGE_NONE (no header)
// is treated exactly as the loader and CsvCodeToPercent treat it: legal-range
// codes (16..235 at 8 bits, 64..940 at 10) clamped to black..white. A
// headerless file is 8-bit; a header with BITDEPTH and no RANGE keeps its
// depth. Under LEGAL and NONE the classification is made on the clamped codes,
// which are what the row drives: a code below black is black, above white is
// white. FULL and EXTENDED classify the codes as written.
PatchLabel ClassifyPatch ( int r, int g, int b, int bits, int range );

#endif // PATCHLABEL_H_INCLUDED_
