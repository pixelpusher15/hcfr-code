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

// PatchOrdering implementation - the behavioral contract lives in
// PatchOrdering.h. Plain C++ and CRT only (no MFC), and deterministic by
// construction: no randomness, no clock, and every sum is taken in
// sequence order, so the same input and parameters give the same output
// byte for byte.

#include "PatchOrdering.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

namespace
{
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

    // "key=value", both non-empty.
    bool SplitKeyValue(const std::string& token, std::string& key, std::string& value)
    {
        size_t eq = token.find('=');
        if (eq == std::string::npos || eq == 0 || eq + 1 >= token.size())
            return false;
        key = token.substr(0, eq);
        value = token.substr(eq + 1);
        return true;
    }

    // The whole text must be one finite number - "1.0x" is a refusal, not 1.
    bool ParseReal(const std::string& text, double& out)
    {
        if (text.empty())
            return false;
        char* end = 0;
        double v = strtod(text.c_str(), &end);
        if (end != text.c_str() + text.size())
            return false;
        if (!_finite(v))
            return false;
        out = v;
        return true;
    }

    // A -1 means "not set" at every level; an unset default counts as 0.
    double ResolveReal(double rowV, double groupV, double defaultV)
    {
        if (rowV != -1.0)
            return rowV;
        if (groupV != -1.0)
            return groupV;
        return (defaultV != -1.0) ? defaultV : 0.0;
    }

    int ResolveInt(int rowV, int groupV, int defaultV)
    {
        if (rowV != -1)
            return rowV;
        if (groupV != -1)
            return groupV;
        return (defaultV != -1) ? defaultV : 0;
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

    // Dwell without the settle-after-step term - everything that depends
    // only on the row itself, so it can be used as a sort key before the
    // row has a predecessor.
    double BaseDwell(const PatchSequence& seq, int index, const OrderingParams& p)
    {
        std::string group;
        PatchTiming row;
        seq.GetGroup(index, group);
        seq.GetTiming(index, row);
        const PatchGroupSpec* g = FindGroup(p, group);
        PatchTiming none;                   // all -1 when the row has no group

        const PatchTiming& gt = g ? g->timing : none;
        double hold = ResolveReal(row.hold, gt.hold, p.defaults.hold);
        if (seq.GetRole(index) == 'I')
            return hold + p.latency;        // shown, never read

        double settle = ResolveReal(row.settle, gt.settle, p.defaults.settle);
        double integration = ResolveReal(row.integration, gt.integration,
                                         p.defaults.integration);
        int average = ResolveInt(row.average, gt.average, p.defaults.average);
        if (average < 1)
            average = 1;
        return settle + integration * (double)average + hold + p.latency;
    }

    // J over one arrangement, given each row's load and dwell in order.
    // Windows are maximal runs of consecutive rows whose summed dwell
    // stays <= W, one starting at every row (always at least that row);
    // the two-pointer walk is exact because every dwell is non-negative.
    double ExcursionOver(const std::vector<double>& L,
                         const std::vector<double>& T, double W)
    {
        size_t n = L.size();
        if (n == 0 || W <= 0.0)
            return -1.0;

        std::vector<double> pT(n + 1, 0.0), pLT(n + 1, 0.0);
        for (size_t i = 0; i < n; ++i)
        {
            pT[i + 1] = pT[i] + T[i];
            pLT[i + 1] = pLT[i] + L[i] * T[i];
        }
        if (pT[n] <= 0.0)
            return -1.0;
        double mean = pLT[n] / pT[n];

        double worst = 0.0;
        size_t e = 0;
        for (size_t s = 0; s < n; ++s)
        {
            if (e < s)
                e = s;
            while (e + 1 < n && pT[e + 2] - pT[s] <= W)
                ++e;
            while (e > s && pT[e + 1] - pT[s] > W)
                --e;
            double wT = pT[e + 1] - pT[s];
            double m;
            if (wT > 0.0)
                m = (pLT[e + 1] - pLT[s]) / wT;
            else
            {
                double sum = 0.0;           // degenerate: every dwell zero
                for (size_t k = s; k <= e; ++k)
                    sum += L[k];
                m = sum / (double)(e - s + 1);
            }
            double d = fabs(m - mean);
            if (d > worst)
                worst = d;
        }
        return worst;
    }

    // ---- Order's working model -------------------------------------------
    //
    // A CHAIN is one body row plus the hand-placed 'I' rows that followed
    // it in the input: the unit that moves as a whole, so an interstitial
    // always keeps the predecessor it was written against.
    struct Chain
    {
        std::vector<int> rows;              // input indices, in input order
        double lt;                          // sort key: load * base dwell
        double load;                        // sort key for the max-step pass
        int    id;                          // tie-break: identity ascending
    };

    struct ByLoadTimeDesc
    {
        bool operator()(const Chain& a, const Chain& b) const
        {
            if (a.lt != b.lt)
                return a.lt > b.lt;
            return a.id < b.id;
        }
    };

    struct ByLoadAsc
    {
        bool operator()(const Chain& a, const Chain& b) const
        {
            if (a.load != b.load)
                return a.load < b.load;
            return a.id < b.id;
        }
    };

    // The arrangement, its flattened rows, and the loads/dwells that go
    // with them. `order` holds chain indices.
    struct Arrangement
    {
        const PatchSequence*  in;
        const OrderingParams* p;
        const std::vector<Chain>* chains;

        std::vector<size_t> order;
        std::vector<int>    rows;           // flattened input indices
        std::vector<size_t> chainAt;        // flattened row -> position in order
        std::vector<double> L, T;

        void Flatten()
        {
            rows.clear();
            chainAt.clear();
            for (size_t k = 0; k < order.size(); ++k)
            {
                const Chain& c = (*chains)[order[k]];
                for (size_t r = 0; r < c.rows.size(); ++r)
                {
                    rows.push_back(c.rows[r]);
                    chainAt.push_back(k);
                }
            }
            L.resize(rows.size());
            T.resize(rows.size());
            for (size_t i = 0; i < rows.size(); ++i)
                L[i] = PatchOrdering::RowLoad(*in, rows[i], *p);
            for (size_t i = 0; i < rows.size(); ++i)
            {
                double t = BaseDwell(*in, rows[i], *p);
                if (in->GetRole(rows[i]) != 'I' && p->settleAfterStep >= 0.0 && i > 0)
                {
                    double drop = L[i - 1] - L[i];
                    if (drop > 0.0)
                        t += p->settleAfterStep * drop;
                }
                T[i] = t;
            }
        }

        double J()
        {
            Flatten();
            return ExcursionOver(L, T, p->windowSeconds);
        }

        bool MaxStepOk(double maxStep)
        {
            Flatten();
            for (size_t i = 0; i + 1 < L.size(); ++i)
                if (fabs(L[i] - L[i + 1]) > maxStep)
                    return false;
            return true;
        }
    };

    // Deterministic pairwise refinement: the window with the largest
    // excursion hands its most-offending row to whichever chain elsewhere
    // reduces J most. The best arrangement seen is kept, so refinement can
    // never leave the seed worse than it found it.
    void Refine(Arrangement& a, double maxStep)
    {
        double curJ = a.J();
        if (curJ < 0.0)
            return;
        std::vector<size_t> best = a.order;
        double bestJ = curJ;

        const size_t m = a.order.size();
        const size_t budget = 4 * a.rows.size();
        for (size_t swaps = 0; swaps < budget; ++swaps)
        {
            a.Flatten();
            const size_t n = a.rows.size();
            if (n == 0)
                break;
            std::vector<double> pT(n + 1, 0.0), pLT(n + 1, 0.0);
            for (size_t i = 0; i < n; ++i)
            {
                pT[i + 1] = pT[i] + a.T[i];
                pLT[i + 1] = pLT[i] + a.L[i] * a.T[i];
            }
            if (pT[n] <= 0.0)
                break;
            double mean = pLT[n] / pT[n];

            // The worst window, and inside it the row pulling hardest in
            // the direction of the excursion.
            size_t ws = 0, we = 0;
            double wm = mean, worst = -1.0;
            size_t e = 0;
            for (size_t s = 0; s < n; ++s)
            {
                if (e < s)
                    e = s;
                while (e + 1 < n && pT[e + 2] - pT[s] <= a.p->windowSeconds)
                    ++e;
                while (e > s && pT[e + 1] - pT[s] > a.p->windowSeconds)
                    --e;
                double wT = pT[e + 1] - pT[s];
                if (wT <= 0.0)
                    continue;
                double mm = (pLT[e + 1] - pLT[s]) / wT;
                double d = fabs(mm - mean);
                if (d > worst)
                {
                    worst = d;
                    ws = s;
                    we = e;
                    wm = mm;
                }
            }
            if (worst <= 0.0)
                break;

            size_t offender = ws;
            for (size_t k = ws + 1; k <= we; ++k)
            {
                if (wm > mean ? (a.L[k] > a.L[offender]) : (a.L[k] < a.L[offender]))
                    offender = k;
            }
            size_t ci = a.chainAt[offender];

            // Every chain that lies wholly inside the window is not
            // "elsewhere": exchanging within a window cannot move its mean.
            std::vector<bool> inside(m, true);
            for (size_t i = 0; i < n; ++i)
                if (i < ws || i > we)
                    inside[a.chainAt[i]] = false;

            size_t pick = m;
            double pickJ = curJ;
            for (size_t cj = 0; cj < m; ++cj)
            {
                if (cj == ci || inside[cj])
                    continue;
                std::swap(a.order[ci], a.order[cj]);
                bool ok = (maxStep < 0.0) || a.MaxStepOk(maxStep);
                double j2 = ok ? a.J() : 0.0;
                std::swap(a.order[ci], a.order[cj]);
                if (ok && j2 < pickJ - 1e-15)
                {
                    pickJ = j2;
                    pick = cj;
                }
            }
            if (pick == m)
                break;                      // a full pass found no improvement

            std::swap(a.order[ci], a.order[pick]);
            curJ = pickJ;
            if (curJ < bestJ)
            {
                bestJ = curJ;
                best = a.order;
            }
        }
        a.order = best;
    }

    // The first row carrying `id`, whatever its role - the definition the
    // anchor and preroll rows repeat.
    int DefinitionOf(const PatchSequence& seq, int id)
    {
        for (int i = 0; i < seq.Count(); ++i)
            if (seq.GetIdentity(i) == id)
                return i;
        return -1;
    }

    bool EmitRow(PatchSequence& res, const PatchSequence& in, int index,
                 char role, bool copyMetadata)
    {
        double rgb[3];
        std::string name;
        in.GetPatch(index, rgb, &name);
        if (!res.AddRow(in.GetIdentity(index), rgb[0], rgb[1], rgb[2], name, role))
            return false;
        if (!copyMetadata)
            return true;
        int at = res.Count() - 1;
        std::string group;
        PatchTiming timing;
        in.GetGroup(index, group);
        in.GetTiming(index, timing);
        return res.SetGroup(at, group) && res.SetTiming(at, timing);
    }

    bool EmitAnchorBlock(PatchSequence& res, const PatchSequence& in,
                         const std::vector<int>& anchorDef)
    {
        for (size_t i = 0; i < anchorDef.size(); ++i)
            if (!EmitRow(res, in, anchorDef[i], 'A', false))
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
        if (!_finite(v))
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
    if (seq.GetRole(index) != 'I' && p.settleAfterStep >= 0.0 && index > 0)
    {
        // Only a DROP in load costs settling time; a rise does not.
        double drop = RowLoad(seq, index - 1, p) - RowLoad(seq, index, p);
        if (drop > 0.0)
            t += p.settleAfterStep * drop;
    }
    return t;
}

double PatchOrdering::WindowExcursion(const PatchSequence& seq,
                                      const OrderingParams& p)
{
    if (p.windowSeconds <= 0.0)
        return -1.0;

    // Anchors and preroll are the measurement of J, not part of it.
    std::vector<double> L, T;
    for (int i = 0; i < seq.Count(); ++i)
    {
        char role = seq.GetRole(i);
        if (role == 'A' || role == 'P')
            continue;
        L.push_back(RowLoad(seq, i, p));
        T.push_back(Dwell(seq, i, p));
    }
    return ExcursionOver(L, T, p.windowSeconds);
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
    if (p.windowSeconds <= 0.0)
    {
        reason = "window seconds must be positive";
        return false;
    }
    if (!_finite(p.area) || p.area < 0.0 || p.area > 1.0)
    {
        reason = "area outside 0..1";
        return false;
    }
    if (!_finite(p.latency) || p.latency < 0.0)
    {
        reason = "negative latency";
        return false;
    }
    if (p.prerollCount < 0)
    {
        reason = "negative preroll count";
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
        anchorDef[i] = DefinitionOf(in, p.anchors[i].id);
        if (anchorDef[i] < 0)
        {
            reason = "unknown anchor identity";
            return false;
        }
    }
    int prerollDef = -1;
    if (p.prerollId >= 0)
    {
        prerollDef = DefinitionOf(in, p.prerollId);
        if (prerollDef < 0)
        {
            reason = "unknown preroll identity";
            return false;
        }
    }

    // ---- chains: a body row plus the interstitials written after it -----
    std::vector<Chain> chains;
    for (int i = 0; i < in.Count(); ++i)
    {
        if (in.GetRole(i) == 'I' && !chains.empty())
        {
            chains.back().rows.push_back(i);
            continue;
        }
        Chain c;
        c.rows.push_back(i);
        c.load = RowLoad(in, i, p);
        c.lt = c.load * BaseDwell(in, i, p);
        c.id = in.GetIdentity(i);
        chains.push_back(c);
    }

    // ---- seed: alternate the two ends of the load*time ordering ---------
    std::vector<Chain> sorted = chains;
    std::stable_sort(sorted.begin(), sorted.end(), ByLoadTimeDesc());

    Arrangement arr;
    arr.in = &in;
    arr.p = &p;
    arr.chains = &sorted;
    {
        size_t lo = 0, hi = sorted.size();
        while (lo < hi)
        {
            arr.order.push_back(lo);
            --hi;
            if (lo != hi)
                arr.order.push_back(hi);
            ++lo;
        }
    }

    Refine(arr, -1.0);

    // ---- the max-step constraint ----------------------------------------
    if (p.maxStep >= 0.0 && !arr.MaxStepOk(p.maxStep))
    {
        // Walking the loads in order minimizes the largest adjacent step
        // over every permutation (any order must still cross each gap in
        // the sorted values), so if this arrangement violates the limit,
        // no arrangement satisfies it.
        std::vector<Chain> byLoad = chains;
        std::stable_sort(byLoad.begin(), byLoad.end(), ByLoadAsc());
        sorted = byLoad;
        arr.order.clear();
        for (size_t k = 0; k < sorted.size(); ++k)
            arr.order.push_back(k);
        if (!arr.MaxStepOk(p.maxStep))
        {
            reason = "max-step cannot be satisfied";
            return false;
        }
        Refine(arr, p.maxStep);             // improve J without losing it
    }

    // ---- assemble --------------------------------------------------------
    PatchSequence res;
    if (in.IsExtended() && !res.SetExtended(true))
    {
        reason = "cannot switch the sequence to extended";
        return false;
    }

    for (int k = 0; k < p.prerollCount; ++k)
        if (!EmitRow(res, in, prerollDef, 'P', false))
        {
            reason = "cannot emit the preroll row";
            return false;
        }
    if (!EmitAnchorBlock(res, in, anchorDef))
    {
        reason = "cannot emit the anchor block";
        return false;
    }

    int bodySeen = 0;
    for (size_t k = 0; k < arr.order.size(); ++k)
    {
        const Chain& c = sorted[arr.order[k]];
        for (size_t r = 0; r < c.rows.size(); ++r)
            if (!EmitRow(res, in, c.rows[r], in.GetRole(c.rows[r]), true))
            {
                reason = "cannot emit a body row";
                return false;
            }
        int before = bodySeen;
        bodySeen += (int)c.rows.size();
        // An anchor block after every `every`-th body row - never inside a
        // chain, so an interstitial keeps the row it follows.
        for (size_t i = 0; i < p.anchors.size(); ++i)
        {
            int every = p.anchors[i].every;
            if (bodySeen / every > before / every)
                if (!EmitRow(res, in, anchorDef[i], 'A', false))
                {
                    reason = "cannot emit the anchor block";
                    return false;
                }
        }
    }

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
