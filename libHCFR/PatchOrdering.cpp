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

// PatchOrdering implementation - the behavioral contract, and the
// provenance of the algorithm, live in PatchOrdering.h. Plain C++ and CRT
// only (no MFC), and deterministic by construction: no randomness, no
// clock, and every sum taken in sequence order, so the same input and
// parameters give the same output byte for byte.
//
// Cost. Per-row loads and base dwells are computed once into arrays;
// laying out an arrangement is a gather over those arrays plus the one
// order-dependent settle-after-step term; J and its worst window come
// from a single walk over prefix sums into buffers allocated once. A
// refinement step scores at most kCandidates arrangements, so ordering a
// set of several thousand rows is seconds of work, not minutes.

#include "PatchOrdering.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    // The .hps key=value token grammar. PatchSequenceFile.cpp has the same
    // helpers, but in its own anonymous namespace and therefore not
    // reachable from here; exporting them would mean new public API in
    // PatchSequenceFile.h, so this file keeps the one local copy.
    void SplitTokens(const std::string& s, std::vector<std::string>& out)
    {
        out.clear();
        size_t i = 0;
        while (i < s.size())
        {
            while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
                ++i;
            size_t b = i;
            while (i < s.size() && s[i] != ' ' && s[i] != '\t')
                ++i;
            if (i > b)
                out.push_back(s.substr(b, i - b));
        }
    }

    bool SplitKeyValue(const std::string& token, std::string& key, std::string& value)
    {
        size_t eq = token.find('=');
        if (eq == std::string::npos || eq == 0 || eq + 1 >= token.size())
            return false;
        key = token.substr(0, eq);
        value = token.substr(eq + 1);
        return true;
    }

    // The whole text must be one finite number - "1.0x" is a refusal.
    bool ParseReal(const std::string& text, double& out)
    {
        if (text.empty())
            return false;
        char* end = 0;
        double v = strtod(text.c_str(), &end);
        if (end != text.c_str() + text.size() || !_finite(v))
            return false;
        out = v;
        return true;
    }

    bool Finite(double v)
    {
        return _finite(v) != 0;
    }

    // A timing field is finite and either set to something non-negative or
    // left at the -1 sentinel.
    bool TimingFieldOk(double v)
    {
        return Finite(v) && (v >= 0.0 || !PatchTiming::IsSet(v));
    }

    bool TimingOk(const PatchTiming& t)
    {
        if (!TimingFieldOk(t.settle) || !TimingFieldOk(t.hold) || !TimingFieldOk(t.integration))
            return false;
        return t.average >= 0 || !PatchTiming::IsSet(t.average);
    }

    const PatchGroupSpec* FindGroup(const OrderingParams& p, const std::string& name)
    {
        if (name.empty())
            return 0;
        for (size_t i = 0; i < p.groups.size(); ++i)
            if (p.groups[i].name == name)
                return &p.groups[i];
        return 0;
    }

    bool IsCounted(char role)
    {
        return role != 'A' && role != 'P';
    }

    // THE dwell formula, without the order-dependent settle-after-step
    // term. Dwell, WindowExcursion and Order all reach the estimate
    // through here, so there is one implementation of it.
    double BaseDwell(const PatchSequence& seq, int index, const OrderingParams& p)
    {
        std::string group;
        PatchTiming row;
        seq.GetGroup(index, group);
        seq.GetTiming(index, row);
        const PatchGroupSpec* g = FindGroup(p, group);
        PatchTiming none;
        PatchTiming r = PatchTiming::Resolved(row, g ? g->timing : none, p.defaults);

        if (seq.GetRole(index) == 'I')
            return r.hold + p.latency;      // shown, never read
        int average = (r.average < 1) ? 1 : r.average;
        return r.settle + r.integration * (double)average + r.hold + p.latency;
    }

    // J and its worst window from ONE walk over the prefix sums. Windows
    // are maximal runs of consecutive counted rows whose summed dwell
    // stays <= W, one per row (always at least that row); the two-pointer
    // never has to step back because every dwell is non-negative.
    // With `bound` >= 0 the walk stops as soon as it can no longer beat
    // it, which is all a rejected candidate needs.
    double WalkWindows(const double* pT, const double* pLT, const double* fl,
                       size_t n, double W, double Lbar, double bound,
                       size_t* wsOut, size_t* weOut, double* wmOut)
    {
        double worst = 0.0;
        size_t bs = 0, be = 0;
        double bm = Lbar;
        size_t e = 0;
        for (size_t s = 0; s < n; ++s)
        {
            if (e < s)
                e = s;
            while (e + 1 < n && pT[e + 2] - pT[s] <= W)
                ++e;
            double wT = pT[e + 1] - pT[s];
            double m = (wT > 0.0) ? ((pLT[e + 1] - pLT[s]) / wT) : fl[s];
            double d = m - Lbar;
            if (d < 0.0)
                d = -d;
            if (d > worst)
            {
                worst = d;
                bs = s;
                be = e;
                bm = m;
            }
            if (bound >= 0.0 && worst >= bound)
                return worst;               // cannot win; stop early
        }
        if (wsOut) *wsOut = bs;
        if (weOut) *weOut = be;
        if (wmOut) *wmOut = bm;
        return worst;
    }

    // ---- Order's working model ------------------------------------------
    //
    // A CHAIN is one non-'I' row plus the 'I' rows that follow it in the
    // input: the unit that moves as a whole, so an interstitial always
    // keeps the row it was written against.
    struct Chain
    {
        int    start;                       // into Work::chainRows
        int    count;
        double lt;                          // sum of L * t_base over the chain
        double entry, exit;                 // loads bounding it
        int    charRows;                    // role-'C' rows in the chain
        // The total tie-break, from the head row.
        int         id;
        std::string name;
        std::string group;
        PatchTiming timing;
    };

    struct ChainLess
    {
        const std::vector<Chain>* c;
        explicit ChainLess(const std::vector<Chain>* v) : c(v) {}
        bool operator()(size_t x, size_t y) const
        {
            const Chain& a = (*c)[x];
            const Chain& b = (*c)[y];
            if (a.lt != b.lt)            return a.lt > b.lt;     // heaviest first
            if (a.id != b.id)            return a.id < b.id;
            if (a.name != b.name)        return a.name < b.name;
            if (a.group != b.group)      return a.group < b.group;
            if (a.timing.settle != b.timing.settle)           return a.timing.settle < b.timing.settle;
            if (a.timing.hold != b.timing.hold)               return a.timing.hold < b.timing.hold;
            if (a.timing.integration != b.timing.integration) return a.timing.integration < b.timing.integration;
            return a.timing.average < b.timing.average;
        }
    };

    struct ByEntryAsc
    {
        const std::vector<Chain>* c;
        explicit ByEntryAsc(const std::vector<Chain>* v) : c(v) {}
        bool operator()(size_t x, size_t y) const
        {
            const Chain& a = (*c)[x];
            const Chain& b = (*c)[y];
            if (a.entry != b.entry) return a.entry < b.entry;
            return ChainLess(c)(x, y);
        }
    };

    // Everything Order needs, allocated once.
    struct Work
    {
        const PatchSequence*  in;
        const OrderingParams* p;

        std::vector<double> rowL, rowT;     // per input row, computed once
        std::vector<char>   rowStep;        // row charges settle-after-step

        std::vector<int>    chainRows;      // input indices, chains back to back
        std::vector<Chain>  chains;
        std::vector<size_t> sorted;         // chain indices, heaviest first
        std::vector<double> sortedLt;       // their lt, for the candidate search

        std::vector<size_t> order;          // position -> index into `sorted`
        std::vector<size_t> posOf;          // index into `sorted` -> position

        std::vector<double> fl, ft, pT, pLT;
        std::vector<size_t> flPos;          // flattened row -> position
        size_t n;                           // body rows

        bool   fixedLbar;
        double Lbar;

        void Flatten()
        {
            const double sas = p->settleAfterStep;
            const bool step = (sas >= 0.0);
            double* L = &fl[0];
            double* T = &ft[0];
            size_t* P = &flPos[0];
            const double* rL = &rowL[0];
            const double* rT = &rowT[0];
            const char*   rS = &rowStep[0];
            const int*    cr = &chainRows[0];

            size_t k = 0;
            for (size_t pos = 0; pos < order.size(); ++pos)
            {
                const Chain& c = chains[sorted[order[pos]]];
                for (int r = 0; r < c.count; ++r)
                {
                    int row = cr[c.start + r];
                    double load = rL[row];
                    double t = rT[row];
                    if (step && k > 0 && rS[row])
                    {
                        // Every body row is counted, so the nearest
                        // preceding counted row is simply the one before.
                        double drop = L[k - 1] - load;
                        if (drop > 0.0)
                            t += sas * drop;
                    }
                    L[k] = load;
                    T[k] = t;
                    P[k] = pos;
                    ++k;
                }
            }

            double* a = &pT[0];
            double* b = &pLT[0];
            a[0] = 0.0;
            b[0] = 0.0;
            for (size_t i = 0; i < n; ++i)
            {
                a[i + 1] = a[i] + T[i];
                b[i + 1] = b[i] + L[i] * T[i];
            }
            Lbar = fixedLbar ? p->targetLoad
                             : ((a[n] > 0.0) ? (b[n] / a[n]) : 0.0);
        }

        double J(double bound, size_t* ws, size_t* we, double* wm)
        {
            Flatten();
            return WalkWindows(&pT[0], &pLT[0], &fl[0], n, p->windowSeconds,
                               Lbar, bound, ws, we, wm);
        }

        // Only chain boundaries: a step inside a chain is the input's own
        // and was refused up front. Anchors and preroll are not part of it.
        bool MaxStepOk(double maxStep) const
        {
            for (size_t pos = 0; pos + 1 < order.size(); ++pos)
            {
                double a = chains[sorted[order[pos]]].exit;
                double b = chains[sorted[order[pos + 1]]].entry;
                if (fabs(a - b) > maxStep)
                    return false;
            }
            return true;
        }

        void SetOrder(const std::vector<size_t>& o)
        {
            order = o;
            for (size_t i = 0; i < order.size(); ++i)
                posOf[order[i]] = i;
        }
    };

    // Pairwise-exchange local search (Croes): the worst window's most
    // offending chain is offered to the chains whose weight is nearest the
    // one that would pull that window's mean onto Lbar, and the best
    // strict improvement is taken. Strict improvement only, so the loop is
    // monotone and the arrangement it ends on is the best it saw.
    void Refine(Work& w, double maxStep)
    {
        const size_t m = w.order.size();
        if (m < 2)
            return;
        size_t budget = 4 * w.n;
        if (budget > (size_t)PatchOrdering::kMaxSwaps)
            budget = (size_t)PatchOrdering::kMaxSwaps;

        std::vector<size_t> candidates;
        candidates.reserve((size_t)PatchOrdering::kCandidates);

        for (size_t swaps = 0; swaps < budget; ++swaps)
        {
            size_t ws = 0, we = 0;
            double wm = 0.0;
            double curJ = w.J(-1.0, &ws, &we, &wm);
            if (curJ <= 0.0)
                break;

            // The chain pulling hardest inside that window.
            const double Lbar = w.Lbar;
            size_t offender = ws;
            double worstPull = -1.0;
            for (size_t k = ws; k <= we; ++k)
            {
                double pull = fabs(w.fl[k] - Lbar) * w.ft[k];
                if (pull > worstPull)
                {
                    worstPull = pull;
                    offender = k;
                }
            }
            size_t ci = w.flPos[offender];
            size_t offChain = w.order[ci];

            // The weight that would move this window's mean onto Lbar.
            double wT = w.pT[we + 1] - w.pT[ws];
            double targetLt = w.chains[w.sorted[offChain]].lt + wT * (Lbar - wm);

            // The kCandidates chains nearest that weight. `sortedLt` is
            // descending, so a binary search plus an outward walk gives
            // them in order of increasing distance, ties to the lower
            // index - deterministic without a sort.
            candidates.clear();
            {
                size_t lo = 0, hi = m;
                while (lo < hi)
                {
                    size_t mid = lo + (hi - lo) / 2;
                    if (w.sortedLt[mid] > targetLt)
                        lo = mid + 1;
                    else
                        hi = mid;
                }
                long left = (long)lo - 1;
                size_t right = lo;
                while (candidates.size() < (size_t)PatchOrdering::kCandidates
                       && (left >= 0 || right < m))
                {
                    double dl = (left >= 0) ? (w.sortedLt[(size_t)left] - targetLt) : DBL_MAX;
                    double dr = (right < m) ? (targetLt - w.sortedLt[right]) : DBL_MAX;
                    size_t take;
                    if (dl <= dr)
                    {
                        take = (size_t)left;
                        --left;
                    }
                    else
                    {
                        take = right;
                        ++right;
                    }
                    if (take != offChain)
                        candidates.push_back(take);
                }
            }

            size_t pick = m;
            double pickJ = curJ;
            for (size_t k = 0; k < candidates.size(); ++k)
            {
                size_t cj = w.posOf[candidates[k]];
                if (cj == ci)
                    continue;
                std::swap(w.order[ci], w.order[cj]);
                bool ok = (maxStep < 0.0) || w.MaxStepOk(maxStep);
                double j2 = ok ? w.J(pickJ, 0, 0, 0) : 0.0;
                std::swap(w.order[ci], w.order[cj]);
                if (ok && j2 < pickJ - 1e-15)
                {
                    pickJ = j2;
                    pick = cj;
                }
            }
            if (pick == m)
                break;                      // no candidate improves

            std::swap(w.order[ci], w.order[pick]);
            w.posOf[w.order[ci]] = ci;
            w.posOf[w.order[pick]] = pick;
        }
    }

    // An emitted row repeats everything the input row carries.
    bool EmitRow(PatchSequence& res, const PatchSequence& in, int index, char role)
    {
        double rgb[3];
        std::string name, group;
        PatchTiming timing;
        in.GetPatch(index, rgb, &name);
        in.GetGroup(index, group);
        in.GetTiming(index, timing);
        if (!res.AddRow(in.GetIdentity(index), rgb[0], rgb[1], rgb[2], name, role))
            return false;
        int at = res.Count() - 1;
        return res.SetGroup(at, group) && res.SetTiming(at, timing);
    }

    bool EmitAnchorBlock(PatchSequence& res, const PatchSequence& in,
                         const std::vector<int>& anchorDef)
    {
        for (size_t i = 0; i < anchorDef.size(); ++i)
            if (!EmitRow(res, in, anchorDef[i], 'A'))
                return false;
        return true;
    }
}

LoadModel::LoadModel()
    : kind(LOAD_LUMINANCE), gamma(2.2),
      shareR(0.2126), shareG(0.7152), shareB(0.0722),
      kw(1.0), kr(1.1), kg(1.0), kb(1.35)
{
}

bool LoadModel::Valid() const
{
    if (!Finite(gamma) || gamma <= 0.0)
        return false;
    if (!Finite(shareR) || !Finite(shareG) || !Finite(shareB))
        return false;
    if (!Finite(kw) || !Finite(kr) || !Finite(kg) || !Finite(kb))
        return false;
    return kw >= 0.0 && kr >= 0.0 && kg >= 0.0 && kb >= 0.0;
}

bool LoadModel::Parse(const std::string& text, LoadModel& out)
{
    std::vector<std::string> tok;
    SplitTokens(text, tok);
    if (tok.empty())
        return false;

    LoadModel m;                            // the defaults, then the tokens
    if (tok[0] == "luminance")
        m.kind = LOAD_LUMINANCE;
    else if (tok[0] == "wrgb-power")
        m.kind = LOAD_WRGB_POWER;
    else
        return false;

    for (size_t i = 1; i < tok.size(); ++i)
    {
        std::string key, value;
        double v = 0.0;
        if (!SplitKeyValue(tok[i], key, value) || !ParseReal(value, v))
            return false;
        if (key == "gamma")
        {
            if (v <= 0.0)
                return false;
            m.gamma = v;
        }
        else if (m.kind == LOAD_WRGB_POWER
                 && (key == "kw" || key == "kr" || key == "kg" || key == "kb"))
        {
            if (v < 0.0)
                return false;
            if (key == "kw")      m.kw = v;
            else if (key == "kr") m.kr = v;
            else if (key == "kg") m.kg = v;
            else                  m.kb = v;
        }
        else
            return false;                   // an unknown token is a refusal
    }

    if (!m.Valid())
        return false;
    out = m;
    return true;
}

std::string LoadModel::Format() const
{
    char buf[256];
    if (kind == LOAD_WRGB_POWER)
        snprintf(buf, sizeof(buf),
                 "wrgb-power kw=%g kr=%g kg=%g kb=%g gamma=%g",
                 kw, kr, kg, kb, gamma);
    else
        snprintf(buf, sizeof(buf), "luminance gamma=%g", gamma);
    return std::string(buf);
}

double LoadModel::Load(const double rgbPct[3]) const
{
    // Clamped to the displayable range BEFORE decoding: a super-white
    // stimulus still only drives the panel to white.
    double c[3];
    for (int k = 0; k < 3; ++k)
    {
        double v = rgbPct[k];
        if (!Finite(v))
            v = 0.0;
        if (v < 0.0)
            v = 0.0;
        else if (v > 100.0)
            v = 100.0;
        c[k] = pow(v / 100.0, gamma);
    }

    if (kind == LOAD_WRGB_POWER)
    {
        // The white sub-pixel carries the common component, the colored
        // emitters the remainder.
        double w = c[0];
        if (c[1] < w) w = c[1];
        if (c[2] < w) w = c[2];
        return kw * w + kr * (c[0] - w) + kg * (c[1] - w) + kb * (c[2] - w);
    }
    return shareR * c[0] + shareG * c[1] + shareB * c[2];
}

OrderingParams::OrderingParams()
    : area(0.1), backgroundLoad(0.0), windowSeconds(45.0), latency(0.4),
      settleAfterStep(-1.0), maxStep(-1.0), targetLoad(-1.0),
      prerollId(-1), prerollCount(0)
{
}

double PatchOrdering::RowLoad(const PatchSequence& seq, int index,
                              const OrderingParams& p)
{
    double rgb[3];
    if (!seq.GetPatch(index, rgb, 0))
        return -1.0;
    return p.area * p.model.Load(rgb) + (1.0 - p.area) * p.backgroundLoad;
}

double PatchOrdering::Dwell(const PatchSequence& seq, int index,
                            const OrderingParams& p)
{
    if (index < 0 || index >= seq.Count())
        return -1.0;
    double t = BaseDwell(seq, index, p);
    if (seq.GetRole(index) != 'I' && p.settleAfterStep >= 0.0)
    {
        // The predecessor is the nearest preceding COUNTED row: an anchor
        // or preroll row is the measurement, not part of the run.
        int prev = index - 1;
        while (prev >= 0 && !IsCounted(seq.GetRole(prev)))
            --prev;
        if (prev >= 0)
        {
            double drop = RowLoad(seq, prev, p) - RowLoad(seq, index, p);
            if (drop > 0.0)
                t += p.settleAfterStep * drop;
        }
    }
    return t;
}

double PatchOrdering::WindowExcursion(const PatchSequence& seq,
                                      const OrderingParams& p)
{
    if (!Finite(p.windowSeconds) || p.windowSeconds <= 0.0)
        return -1.0;

    std::vector<double> L, T;
    for (int i = 0; i < seq.Count(); ++i)
    {
        if (!IsCounted(seq.GetRole(i)))
            continue;                       // the measurement, not the run
        L.push_back(RowLoad(seq, i, p));
        T.push_back(Dwell(seq, i, p));
    }
    size_t n = L.size();
    if (n == 0)
        return -1.0;

    std::vector<double> pT(n + 1, 0.0), pLT(n + 1, 0.0);
    for (size_t i = 0; i < n; ++i)
    {
        pT[i + 1] = pT[i] + T[i];
        pLT[i + 1] = pLT[i] + L[i] * T[i];
    }
    bool fixed = (p.targetLoad >= 0.0);
    if (!fixed && pT[n] <= 0.0)
        return -1.0;
    double Lbar = fixed ? p.targetLoad : (pLT[n] / pT[n]);
    return WalkWindows(&pT[0], &pLT[0], &L[0], n, p.windowSeconds, Lbar,
                       -1.0, 0, 0, 0);
}

bool PatchOrdering::Order(const PatchSequence& in, const OrderingParams& p,
                          PatchSequence& out, std::string& reason)
{
    // ---- every refusal before the output is touched ---------------------
    if (in.Count() <= 0)
    {
        reason = "empty patch sequence";
        return false;
    }
    for (int i = 0; i < in.Count(); ++i)
    {
        char role = in.GetRole(i);
        if (role == 'A' || role == 'P')
        {
            reason = "input already carries anchor or preroll rows";
            return false;
        }
    }
    if (in.GetRole(0) == 'I')
    {
        reason = "leading interstitial row has no predecessor";
        return false;
    }

    if (!p.model.Valid())
    {
        reason = "invalid load model";
        return false;
    }
    if (!Finite(p.windowSeconds) || !Finite(p.area) || !Finite(p.latency)
        || !Finite(p.backgroundLoad) || !Finite(p.maxStep)
        || !Finite(p.settleAfterStep) || !Finite(p.targetLoad))
    {
        reason = "non-finite ordering parameter";
        return false;
    }
    if (!TimingOk(p.defaults))
    {
        reason = "invalid default timing";
        return false;
    }
    for (size_t i = 0; i < p.groups.size(); ++i)
        if (!TimingOk(p.groups[i].timing))
        {
            reason = "invalid group timing";
            return false;
        }
    if (p.windowSeconds <= 0.0)
    {
        reason = "window seconds must be positive";
        return false;
    }
    if (p.area < 0.0 || p.area > 1.0)
    {
        reason = "area outside 0..1";
        return false;
    }
    if (p.latency < 0.0)
    {
        reason = "negative latency";
        return false;
    }
    if (p.prerollCount < 0)
    {
        reason = "negative preroll count";
        return false;
    }
    if (p.prerollCount > 0 && p.prerollId < 0)
    {
        reason = "preroll count without a preroll identity";
        return false;
    }

    std::vector<int> anchorDef(p.anchors.size(), -1);
    for (size_t i = 0; i < p.anchors.size(); ++i)
    {
        if (p.anchors[i].every < 1)
        {
            reason = "anchor interval must be at least one";
            return false;
        }
        anchorDef[i] = in.DefinitionIndex(p.anchors[i].id);
        if (anchorDef[i] < 0)
        {
            reason = "unknown anchor identity";
            return false;
        }
    }
    int prerollDef = -1;
    if (p.prerollId >= 0)
    {
        prerollDef = in.DefinitionIndex(p.prerollId);
        if (prerollDef < 0)
        {
            reason = "unknown preroll identity";
            return false;
        }
    }

    // ---- per-row loads and base dwells, computed once --------------------
    Work w;
    w.in = &in;
    w.p = &p;
    const size_t rows = (size_t)in.Count();
    w.rowL.resize(rows);
    w.rowT.resize(rows);
    w.rowStep.resize(rows);
    for (size_t i = 0; i < rows; ++i)
    {
        w.rowL[i] = RowLoad(in, (int)i, p);
        w.rowT[i] = BaseDwell(in, (int)i, p);
        w.rowStep[i] = (char)(in.GetRole((int)i) != 'I' ? 1 : 0);
    }

    // ---- chains ----------------------------------------------------------
    w.chainRows.reserve(rows);
    for (int i = 0; i < in.Count(); ++i)
    {
        if (in.GetRole(i) == 'I' && !w.chains.empty())
        {
            Chain& c = w.chains.back();
            ++c.count;
            c.exit = w.rowL[(size_t)i];
            c.lt += w.rowL[(size_t)i] * w.rowT[(size_t)i];
            w.chainRows.push_back(i);
            continue;
        }
        Chain c;
        c.start = (int)w.chainRows.size();
        c.count = 1;
        c.lt = w.rowL[(size_t)i] * w.rowT[(size_t)i];
        c.entry = w.rowL[(size_t)i];
        c.exit = w.rowL[(size_t)i];
        c.charRows = (in.GetRole(i) == 'C') ? 1 : 0;
        c.id = in.GetIdentity(i);
        double rgb[3];
        in.GetPatch(i, rgb, &c.name);
        in.GetGroup(i, c.group);
        in.GetTiming(i, c.timing);
        w.chainRows.push_back(i);
        w.chains.push_back(c);
    }

    // An intra-chain step is the input's own arrangement, not something
    // ordering can fix, so it is refused up front with its own reason.
    if (p.maxStep >= 0.0)
    {
        for (size_t c = 0; c < w.chains.size(); ++c)
            for (int r = 1; r < w.chains[c].count; ++r)
            {
                int a = w.chainRows[(size_t)(w.chains[c].start + r - 1)];
                int b = w.chainRows[(size_t)(w.chains[c].start + r)];
                if (fabs(w.rowL[(size_t)a] - w.rowL[(size_t)b]) > p.maxStep)
                {
                    reason = "interstitial step exceeds max-step";
                    return false;
                }
            }
    }

    // ---- seed: alternate the two ends of the load*time ordering ----------
    const size_t m = w.chains.size();
    w.sorted.resize(m);
    for (size_t i = 0; i < m; ++i)
        w.sorted[i] = i;
    std::sort(w.sorted.begin(), w.sorted.end(), ChainLess(&w.chains));
    w.sortedLt.resize(m);
    for (size_t i = 0; i < m; ++i)
        w.sortedLt[i] = w.chains[w.sorted[i]].lt;

    w.n = rows;
    w.fl.resize(rows);
    w.ft.resize(rows);
    w.flPos.resize(rows);
    w.pT.resize(rows + 1);
    w.pLT.resize(rows + 1);
    w.posOf.resize(m);
    w.fixedLbar = (p.targetLoad >= 0.0);

    std::vector<size_t> seed;
    seed.reserve(m);
    {
        size_t lo = 0, hi = m;
        while (lo < hi)
        {
            seed.push_back(lo);
            --hi;
            if (lo != hi)
                seed.push_back(hi);
            ++lo;
        }
    }
    w.SetOrder(seed);

    Refine(w, -1.0);

    // ---- the max-step constraint ----------------------------------------
    if (p.maxStep >= 0.0 && !w.MaxStepOk(p.maxStep))
    {
        // For single-row chains, walking the loads in order minimizes the
        // largest adjacent difference (any order must still cross every
        // gap between consecutive sorted values), so the two monotone
        // arrangements are the ones worth trying after the refinement.
        std::vector<size_t> byEntry(m);
        for (size_t i = 0; i < m; ++i)
            byEntry[i] = w.sorted[i];
        std::sort(byEntry.begin(), byEntry.end(), ByEntryAsc(&w.chains));
        std::vector<size_t> posInSorted(m);      // chain index -> its rank
        for (size_t k = 0; k < m; ++k)
            posInSorted[w.sorted[k]] = k;
        std::vector<size_t> ascending(m), descending(m);
        for (size_t i = 0; i < m; ++i)
        {
            size_t at = posInSorted[byEntry[i]];
            ascending[i] = at;
            descending[m - 1 - i] = at;
        }

        bool done = false;
        w.SetOrder(ascending);
        if (w.MaxStepOk(p.maxStep))
            done = true;
        if (!done)
        {
            w.SetOrder(descending);
            if (w.MaxStepOk(p.maxStep))
                done = true;
        }
        if (!done)
        {
            reason = "max-step cannot be satisfied";
            return false;
        }
        Refine(w, p.maxStep);               // improve J without losing it
    }

    // ---- assemble --------------------------------------------------------
    PatchSequence res;
    if (in.IsExtended() && !res.SetExtended(true))
    {
        reason = "cannot switch the sequence to extended";
        return false;
    }

    for (int k = 0; k < p.prerollCount; ++k)
        if (!EmitRow(res, in, prerollDef, 'P'))
        {
            reason = "cannot emit the preroll row";
            return false;
        }
    if (!EmitAnchorBlock(res, in, anchorDef))
    {
        reason = "cannot emit the anchor block";
        return false;
    }

    int charSeen = 0;
    size_t firedOnLast = 0;
    for (size_t pos = 0; pos < w.order.size(); ++pos)
    {
        const Chain& c = w.chains[w.sorted[w.order[pos]]];
        for (int r = 0; r < c.count; ++r)
        {
            int row = w.chainRows[(size_t)(c.start + r)];
            if (!EmitRow(res, in, row, in.GetRole(row)))
            {
                reason = "cannot emit a body row";
                return false;
            }
        }
        // The cadence counts CHARACTERIZATION rows, and a block follows
        // the whole chain, so an interstitial is never split from its row.
        int before = charSeen;
        charSeen += c.charRows;
        firedOnLast = 0;
        for (size_t i = 0; i < p.anchors.size(); ++i)
        {
            int every = p.anchors[i].every;
            if (charSeen / every > before / every)
            {
                if (!EmitRow(res, in, anchorDef[i], 'A'))
                {
                    reason = "cannot emit the anchor block";
                    return false;
                }
                ++firedOnLast;
            }
        }
    }

    // The tail block, unless the cadence already ended the run with a full
    // one - two identical blocks back to back measure nothing extra.
    if (!(!anchorDef.empty() && firedOnLast == anchorDef.size()))
        if (!EmitAnchorBlock(res, in, anchorDef))
        {
            reason = "cannot emit the anchor block";
            return false;
        }

    out.Swap(res);
    reason.clear();
    return true;
}

double PatchOrdering::DriftScore(const std::vector<double>& anchorY)
{
    if (anchorY.size() < 2)
        return -1.0;
    double lo = anchorY[0], hi = anchorY[0], sum = 0.0;
    for (size_t i = 0; i < anchorY.size(); ++i)
    {
        if (anchorY[i] < lo) lo = anchorY[i];
        if (anchorY[i] > hi) hi = anchorY[i];
        sum += anchorY[i];
    }
    double mean = sum / (double)anchorY.size();
    if (mean <= 0.0)
        return -1.0;
    return (hi - lo) / mean;
}
