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
// clock, no hidden state: the same input and parameters give the same
// output byte for byte.
//
// Provenance. The objective (equalize the average panel load over every
// rolling window) was described publicly by the authors of a commercial
// sequencer on AVS Forum and is used here as prior art only; nothing is
// ported. The algorithm is built from two textbook pieces: the seed is
// greedy list scheduling in the sense of R. L. Graham, "Bounds on
// Multiprocessing Timing Anomalies", SIAM J. Appl. Math. 17(2), 1969
// (largest items first, balanced by alternating ends); the refinement is
// pairwise-exchange local search as introduced by G. A. Croes, "A Method
// for Solving Traveling-Salesman Problems", Operations Research 6(6),
// 1958. The max-step fallback rests on an elementary fact stated where it
// is used: for single-row chains, sorting by load minimizes the largest
// adjacent difference, because any order must still cross every gap
// between consecutive sorted values.
//
// Load model. Every row gets a scalar load L in arbitrary units (only
// ratios matter). Inputs: the row's components gamma-decoded to linear
// r,g,b in 0..1 (percent/100, clamped to 0..1 first, raised to `gamma`),
// the window area fraction `area`, and the background's own load:
//     L_row = area * L_fg(r,g,b) + (1 - area) * backgroundLoad
//   luminance:  L_fg = shareR*r + shareG*g + shareB*b   (Rec.709 shares by
//               default: 0.2126, 0.7152, 0.0722 - the exact BT.709 values;
//               CColorReference carries rounded ones for a different use)
//   wrgb-power: w = min(r,g,b);  L_fg = kw*w + kr*(r-w) + kg*(g-w) + kb*(b-w)
//               (the white sub-pixel carries the common component, the
//               colored emitters the remainder, each with a power factor)
// LoadModel::Parse reads the LOAD-MODEL directive text ("luminance
// [gamma=<g>]" or "wrgb-power [kw=<g>] [kr=<g>] [kg=<g>] [kb=<g>]
// [gamma=<g>]", tokens in any order, unknown tokens refused, using the
// same key=value grammar as every other .hps directive) and Format writes
// the canonical form: the kind, then for wrgb-power kw kr kg kb, then
// gamma, each as "key=%g", single spaces, e.g.
// "wrgb-power kw=1 kr=1.1 kg=1 kb=1.35 gamma=2.2" / "luminance gamma=2.2".
//
// Dwell. Heat is load times time, so every row carries an estimated dwell
//     t = settle + integration * max(average, 1) + hold + latency
//         + settleAfterStep * max(0, L_prev - L_row)
// where settle / integration / average / hold are PatchTiming::Resolved
// (the row's own value when set, else its group's, else `defaults`, an
// unset default counting as 0), L_prev is the load of the nearest
// preceding COUNTED row (see Objective; none: no term), and an 'I' row
// (never read) has t = hold + latency only. There is one implementation of
// this formula, used by Dwell, WindowExcursion and Order alike.
//
// Objective. Counted rows are every row whose role is not 'A' or 'P'
// (anchors and preroll are the measurement of J, not part of it, and are
// skipped for the predecessor rule too). Let Lbar be the dwell-weighted
// mean load over the counted rows, or `targetLoad` when it is >= 0 (the
// equilibrium a bundle of sets shares). A window is a maximal run of
// consecutive counted rows starting at each counted row whose summed
// dwell stays <= W (always at least the starting row). J = max over
// windows of | sum(L*t)/sum(t) - Lbar |. J and its worst window come from
// one walk, used by WindowExcursion and by the refinement.
//
// Chains. An 'I' row placed by hand is attached to its input predecessor
// and moves with it: a CHAIN is one non-'I' row followed by the 'I' rows
// that follow it in the input. A leading 'I' row (no predecessor) is
// refused. A chain's key is its aggregate: lt = sum over its rows of
// L*t_base (t_base = the dwell without the settle-after-step term), and
// its entry load (first row) and exit load (last row) bound the steps to
// its neighbors.
//
// Order(). Input: a sequence with no 'A' or 'P' rows (anchors and preroll
// are what Order adds; an input carrying them is refused). Output, in this
// order: `prerollCount` copies of the preroll definition (role 'P'), then
// the anchor block (one row per anchor spec, in spec order, role 'A'),
// then the ordered body with an anchor block after every `every`-th
// CHARACTERIZATION row (each anchor's own `every`; 'I' rows do not count;
// the block follows that row's whole chain; several specs falling on one
// row go in spec order), then the anchor block again at the tail - unless
// the cadence already ended the run with a full block, since two identical
// blocks back to back measure nothing extra. Anchor
// and preroll ids must be identities of definitions present in the input
// (any role, found through PatchSequence::DefinitionIndex); an emitted
// anchor or preroll row repeats that definition's R,G,B, name, group and
// timing. Body ordering: chains are sorted by lt descending with a TOTAL
// tie-break - identity, then name, then group, then settle, hold,
// integration, average of the head row (two chains equal in all of these
// are interchangeable, so the result is the same either way) - seeded by
// alternating the two ends of that list (largest, smallest, next largest,
// ...), then refined by deterministic pairwise swaps that reduce J: the
// worst window's most-offending chain (largest |L - Lbar| weighted by
// dwell) is tried against a bounded candidate set - the kCandidates chains
// nearest in lt to the value that would move the window mean to Lbar -
// and the best strict improvement is taken; stop when no candidate
// improves, or after min(4n, kMaxSwaps) swaps. Per-row loads and base
// dwells are computed once; a candidate is scored in O(n) with no
// allocation, so ordering a 10,000-row set completes in seconds.
// With `maxStep` >= 0 the result must satisfy |L_exit - L_entry| <=
// maxStep between every pair of adjacent chains (anchor and preroll rows
// are not part of the check). A step inside a chain is the input's own
// and is refused up front when it exceeds maxStep ("interstitial step
// exceeds max-step"). The search tries, in order: the refined arrangement
// with swaps restricted to those that keep the constraint satisfied, the
// chains sorted ascending by entry load, and descending; when none
// satisfies it, Order refuses with "max-step cannot be satisfied".
// Multiset guarantee: the body is a permutation of the input rows - same
// identities, roles, R,G,B, names, groups and timings, nothing added or
// lost.
//
// DriftScore. Peak-to-peak of the anchor readings divided by their mean:
// the run's drift as a fraction (0.0025 = 0.25%). Returns -1 for fewer
// than two readings or a non-positive mean.
//
// Refusals (Order; out untouched, `reason` one lowercase line): an empty
// input; an input with 'A' or 'P' rows; a leading 'I' row; an anchor or
// preroll id that is not a definition in the input; prerollCount > 0 with
// prerollId < 0; a spec with every < 1 or a negative prerollCount; W <= 0;
// area outside 0..1; a negative latency; any non-finite parameter (W,
// area, latency, backgroundLoad, maxStep, settleAfterStep, targetLoad,
// the model's gamma and factors, every timing field of `defaults` and of
// every group); a timing field below 0 other than -1; an intra-chain step
// above maxStep.

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
    // negative or non-finite factor; `out` is untouched on refusal.
    static bool Parse(const std::string& text, LoadModel& out);
    std::string Format() const;

    // Every field finite, gamma > 0, factors >= 0.
    bool Valid() const;

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
    double      targetLoad;         // -1 = the set's own mean, else Lbar
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
    static const int kCandidates = 64;
    static const int kMaxSwaps   = 4000;

    // The load and dwell of row `index` (see the header comment); -1 on a
    // bad index. Dwell's settle-after-step term uses the nearest preceding
    // counted row of `seq`.
    static double RowLoad(const PatchSequence& seq, int index, const OrderingParams& p);
    static double Dwell(const PatchSequence& seq, int index, const OrderingParams& p);

    // J for `seq` as it stands; -1 when no row is counted or W <= 0.
    static double WindowExcursion(const PatchSequence& seq, const OrderingParams& p);

    // The ordering stage (see the header comment and the refusal list).
    static bool Order(const PatchSequence& in, const OrderingParams& p,
                      PatchSequence& out, std::string& reason);

    static double DriftScore(const std::vector<double>& anchorY);
};

#endif // PATCHORDERING_H
