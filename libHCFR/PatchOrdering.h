/////////////////////////////////////////////////////////////////////////////
//
//  This file is subject to the terms of the GNU General Public License as
//  published by the Free Software Foundation.  A copy of this license is
//  included with this software distribution in the file COPYING.htm. If you
//  do not have a copy, you may obtain a copy by writing to the Free
//  Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  This software is distributed in the hope that it is useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details
/////////////////////////////////////////////////////////////////////////////

// PatchOrdering - the ordering stage: constant-windowed-load sequencing
// with drift anchors. MFC-free, libHCFR.
//
// A pure function from one PatchSequence to another: Order() permutes the
// characterization rows so that every rolling window of W seconds of
// estimated dwell presents (nearly) the same average panel load, inserts
// preroll and anchor rows, and never mutates its input. No randomness, no
// hidden state: the same input and parameters give the same output byte
// for byte. The objective and the models are original implementations of
// published principles (balanced-partition scheduling; the constant-load
// ordering objective described publicly by its authors on AVS); nothing
// is ported.
//
// Load model. Every row gets a scalar load L in arbitrary units (only
// ratios matter). Inputs: the row's components gamma-decoded to linear
// r,g,b in 0..1 (percent/100, clamped to 0..1 first, raised to `gamma`),
// the window area fraction `area`, and the background's own load:
//     L_row = area * L_fg(r,g,b) + (1 - area) * backgroundLoad
//   luminance:  L_fg = shareR*r + shareG*g + shareB*b   (Rec.709 shares by
//               default: 0.2126, 0.7152, 0.0722)
//   wrgb-power: w = min(r,g,b);  L_fg = kw*w + kr*(r-w) + kg*(g-w) + kb*(b-w)
//               (the white sub-pixel carries the common component, the
//               colored emitters the remainder, each with a power factor)
// LoadModel::Parse reads the LOAD-MODEL directive text ("luminance
// [gamma=<g>]" or "wrgb-power [kw=<g>] [kr=<g>] [kg=<g>] [kb=<g>]
// [gamma=<g>]", tokens in any order, unknown tokens refused) and Format
// writes the canonical form: the kind, then for wrgb-power kw kr kg kb,
// then gamma, each as "key=%g", single spaces, e.g.
// "wrgb-power kw=1 kr=1.1 kg=1 kb=1.35 gamma=2.2" / "luminance gamma=2.2".
//
// Dwell. Heat is load times time, so every row carries an estimated dwell
//     t = settle + integration * max(average, 1) + hold + latency
//         + settleAfterStep * max(0, L_prev - L_row)
// where settle / integration / average / hold come from the row's own
// PatchTiming when set, else from its group's, else from `defaults` (a
// -1 in `defaults` counts as 0), L_prev is the load of the row before it
// in the sequence (no predecessor: no term), and an 'I' row (never read)
// has t = hold + latency only.
//
// Objective. Let Lbar be the dwell-weighted mean load over every row the
// window statistic counts. A window is a maximal run of consecutive
// counted rows starting at each row whose summed dwell stays <= W (always
// at least the starting row). J = max over windows of
// | sum(L*t)/sum(t) - Lbar |. Rows of role 'A' and 'P' are NOT counted
// (they are the measurement of J, not part of it); every other role is.
//
// Order(). Input: a sequence with no 'A' or 'P' rows (anchors and preroll
// are what Order adds; an input carrying them is refused). Output, in this
// order: `prerollCount` copies of the preroll definition (role 'P'), then
// the anchor block (one row per anchor spec, in spec order, role 'A'),
// then the ordered body with an anchor block inserted after every
// `every`-th body row (each anchor's own `every`; when several fall on the
// same row they go in spec order), then the anchor block again at the
// tail. Anchor and preroll ids must be identities of definitions present
// in the input (any role); the inserted rows repeat that definition's
// R,G,B and name. Body ordering: rows are sorted by L*t descending
// (ties by identity ascending), seeded by alternating the two ends of
// that list (largest, smallest, next largest, ...), then refined by
// deterministic pairwise swaps that reduce J (the window with the largest
// excursion exchanges its most-offending row with the row elsewhere that
// reduces J most; stop when a full pass makes no improvement or after
// 4 * n swaps). An 'I' row placed by hand keeps its input predecessor:
// wherever that predecessor lands, the 'I' row follows it immediately (it
// is not sorted on its own). With `maxStep` >= 0 the result must also
// satisfy |L_i - L_{i+1}| <= maxStep on every adjacent pair of body rows;
// when no arrangement found within the swap budget satisfies it, Order
// refuses with reason "max-step cannot be satisfied". Multiset guarantee:
// the body is a permutation of the input rows - same identities, roles,
// R,G,B, names, groups and timings, nothing added or lost.
//
// DriftScore. Peak-to-peak of the anchor readings divided by their mean:
// the run's drift as a fraction (0.0025 = 0.25%). Returns -1 for fewer
// than two readings or a non-positive mean.
//
// Error convention: bool-returning operations leave their outputs
// untouched on refusal and set `reason` (one lowercase line).

#ifndef PATCHORDERING_H
#define PATCHORDERING_H

#include "PatchSequence.h"
#include "PatchSequenceFile.h"

#include <string>
#include <vector>

struct LoadModel
{
    enum Kind { LOAD_LUMINANCE = 0, LOAD_WRGB_POWER = 1 };

    Kind   kind;
    double gamma;                   // default 2.2
    double shareR, shareG, shareB;  // luminance shares, default Rec.709
    double kw, kr, kg, kb;          // wrgb-power factors, default 1, 1.1, 1, 1.35

    LoadModel();

    // Directive text <-> model (see the header comment). Parse refuses an
    // unknown kind or token, a non-finite or non-positive gamma, or a
    // negative factor; `out` is untouched on refusal.
    static bool Parse(const std::string& text, LoadModel& out);
    std::string Format() const;

    // L_fg for one row's components in stimulus percent.
    double Load(const double rgbPct[3]) const;
};

struct OrderingParams
{
    LoadModel   model;
    double      area;               // window area fraction 0..1; default 0.1
    double      backgroundLoad;     // L of the background; default 0
    double      windowSeconds;      // W; default 45
    double      latency;            // per-row overhead; default 0.4
    double      settleAfterStep;    // seconds per unit load drop; -1 none
    double      maxStep;            // -1 none
    double      targetLoad;         // reserved (-1 = the set's own mean)
    PatchTiming defaults;           // run defaults for dwell
    std::vector<PatchGroupSpec>  groups;
    std::vector<PatchAnchorSpec> anchors;
    int         prerollId;          // -1 none
    int         prerollCount;       // 0 none

    OrderingParams();
};

class PatchOrdering
{
public:
    // The load and dwell of row `index` (see the header comment); -1 on a
    // bad index. Dwell looks at the row before `index` in `seq` for the
    // settle-after-step term.
    static double RowLoad(const PatchSequence& seq, int index, const OrderingParams& p);
    static double Dwell(const PatchSequence& seq, int index, const OrderingParams& p);

    // J for `seq` as it stands; -1 when no row is counted or W <= 0.
    static double WindowExcursion(const PatchSequence& seq, const OrderingParams& p);

    // The ordering stage. Refuses (out untouched) an empty input, an input
    // with 'A' or 'P' rows, an anchor or preroll id that is not a
    // definition in the input, a spec with every < 1 or count < 0, W <= 0,
    // area outside 0..1, or a negative latency.
    static bool Order(const PatchSequence& in, const OrderingParams& p,
                      PatchSequence& out, std::string& reason);

    static double DriftScore(const std::vector<double>& anchorY);
};

#endif // PATCHORDERING_H
