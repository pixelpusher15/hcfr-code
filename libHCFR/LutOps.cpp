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

// LUT lattice algebra: typed composition, resampling, neutral-axis shaper
// extraction, and the shaper+cube split. The behavioral contract lives in
// LutOps.h; this file is the implementation of that contract.
//
// Mathematics used here is elementary and standard: uniform-knot
// piecewise-linear interpolation and its exact inverse (a strictly
// increasing PWL curve is invertible by swapping the (position, value)
// roles of each segment), and lattice resampling by evaluating the source
// map at the destination node positions. The 3D interpolation itself is
// CubeLUT's tetrahedral evaluator (see CubeLUT.h for its citation).
// Independent implementation; no code from any other project.

#include "LutOps.h"

#include <math.h>
#include <new>
#include <stdio.h>
#include <utility>

namespace
{
    inline bool Finite(double v)
    {
        return CubeLUT::IsFiniteValue(v);
    }

    inline bool FiniteTriple(const double v[3])
    {
        return Finite(v[0]) && Finite(v[1]) && Finite(v[2]);
    }

    // Knot/node position for (possibly fractional) index of count knots
    // spanning [dmin, dmax]: mathematically dmin + index/(count-1) *
    // (dmax - dmin), written in the symmetric interpolation form
    // dmin*(1-u) + dmax*u, which reproduces both endpoints exactly (u = 0
    // gives dmin, u = 1 gives dmax, with no dependence on the rounding of
    // the span). Interior positions carry ordinary last-place rounding, and
    // mapping one back through an evaluator's (x - dmin)/span inverse can
    // land an ulp inside the neighboring cell - which is why bit-exactness
    // is only ever promised where it is achieved structurally (verbatim
    // copies, GetEntry), never through round-tripped arithmetic. This one
    // helper defines the knot mapping for every operation AND for
    // ShaperCurve's inverse, so no two sites can drift apart.
    inline double NodePosition(double dmin, double dmax, double index,
                               int count)
    {
        const double u = index / (double)(count - 1);
        return dmin * (1.0 - u) + dmax * u;
    }

    inline void SetErr(std::string* err, const char* text)
    {
        if (err)
            *err = text;
    }

    inline void ClearErr(std::string* err)
    {
        if (err)
            err->clear();
    }

    // The size bounds live on CubeLUT; the message is derived from them so
    // it can never drift from the actual policy.
    void SetSizeErr(std::string* err)
    {
        if (err)
        {
            char buf[64];
            buf[0] = '\0';
            _snprintf(buf, sizeof(buf) - 1,
                      "lattice size must be between %d and %d",
                      CubeLUT::kMinSize, CubeLUT::kMaxSize);
            buf[sizeof(buf) - 1] = '\0';
            *err = buf;
        }
    }

    // Builds the K = lut.Size() diagonal samples of channel c and checks
    // strict monotonicity. Returns false (samples still filled as far as it
    // got) when the diagonal is not strictly increasing.
    bool DiagonalSamples(const CubeLUT& lut, int channel,
                         std::vector<double>& samples)
    {
        const int k = lut.Size();
        samples.clear();
        samples.reserve(k);
        for (int i = 0; i < k; ++i)
        {
            double v[3];
            if (!lut.GetEntry(i, i, i, v))
                return false;
            if (!Finite(v[channel]))
                return false;
            if (i > 0 && v[channel] <= samples[i - 1])
                return false;
            samples.push_back(v[channel]);
        }
        return true;
    }

    const char* ChannelName(int c)
    {
        return (c == 0) ? "red" : ((c == 1) ? "green" : "blue");
    }
}

//---------------------------------------------------------------- ShaperCurve

ShaperCurve::ShaperCurve()
{
    for (int c = 0; c < 3; ++c)
    {
        m_domainMin[c] = 0.0;
        m_domainMax[c] = 1.0;
    }
}

bool ShaperCurve::SetChannel(int channel, const double* samples, int count,
                             double dmin, double dmax)
{
    // Validate everything before touching the channel: a rejected call must
    // leave the previous curve exactly as it was.
    if (channel < 0 || channel > 2)
        return false;
    if (samples == 0 || count < 2)
        return false;
    if (!CubeLUT::ValidDomainComponent(dmin, dmax))
        return false;
    for (int i = 0; i < count; ++i)
    {
        if (!Finite(samples[i]))
            return false;
        if (i > 0 && !(samples[i] > samples[i - 1]))
            return false;
    }

    m_samples[channel].assign(samples, samples + count);
    m_domainMin[channel] = dmin;
    m_domainMax[channel] = dmax;
    return true;
}

bool ShaperCurve::IsValid() const
{
    for (int c = 0; c < 3; ++c)
        if (m_samples[c].size() < 2)
            return false;
    return true;
}

int ShaperCurve::Count(int channel) const
{
    if (channel < 0 || channel > 2)
        return 0;
    return (int)m_samples[channel].size();
}

bool ShaperCurve::GetDomain(int channel, double& dmin, double& dmax) const
{
    if (channel < 0 || channel > 2 || m_samples[channel].size() < 2)
        return false;
    dmin = m_domainMin[channel];
    dmax = m_domainMax[channel];
    return true;
}

bool ShaperCurve::GetSample(int channel, int index, double& value) const
{
    if (channel < 0 || channel > 2 || m_samples[channel].size() < 2)
        return false;
    if (index < 0 || index >= (int)m_samples[channel].size())
        return false;
    value = m_samples[channel][index];
    return true;
}

bool ShaperCurve::Evaluate(const double in[3], double out[3]) const
{
    if (!IsValid() || !FiniteTriple(in))
    {
        out[0] = out[1] = out[2] = 0.0;
        return false;
    }

    // Compute into locals so in and out may alias.
    double result[3];
    for (int c = 0; c < 3; ++c)
    {
        const std::vector<double>& s = m_samples[c];
        const int count = (int)s.size();
        const double dmin = m_domainMin[c];
        const double dmax = m_domainMax[c];
        const double x = in[c];

        if (x <= dmin)
        {
            result[c] = s[0];
            continue;
        }
        if (x >= dmax)
        {
            result[c] = s[count - 1];
            continue;
        }

        // Uniform knots: the segment index is the floor of the scaled
        // position, clamped to the last segment for safety at the top end.
        double t = (x - dmin) / (dmax - dmin) * (count - 1);
        int i = (int)floor(t);
        if (i < 0)
            i = 0;
        if (i > count - 2)
            i = count - 2;
        double frac = t - i;
        result[c] = s[i] + frac * (s[i + 1] - s[i]);
    }

    out[0] = result[0];
    out[1] = result[1];
    out[2] = result[2];
    return true;
}

bool ShaperCurve::EvaluateInverse(const double in[3], double out[3]) const
{
    if (!IsValid() || !FiniteTriple(in))
    {
        out[0] = out[1] = out[2] = 0.0;
        return false;
    }

    double result[3];
    for (int c = 0; c < 3; ++c)
    {
        const std::vector<double>& s = m_samples[c];
        const int count = (int)s.size();
        const double dmin = m_domainMin[c];
        const double dmax = m_domainMax[c];
        const double v = in[c];

        if (v <= s[0])
        {
            result[c] = dmin;
            continue;
        }
        if (v >= s[count - 1])
        {
            result[c] = dmax;
            continue;
        }

        // Samples are strictly increasing: binary search for the segment
        // with s[i] <= v < s[i+1].
        int lo = 0;
        int hi = count - 1;
        while (hi - lo > 1)
        {
            int mid = lo + (hi - lo) / 2;
            if (s[mid] <= v)
                lo = mid;
            else
                hi = mid;
        }
        double frac = (v - s[lo]) / (s[lo + 1] - s[lo]);
        // The shared NodePosition helper (fractional index) defines the
        // knot mapping here exactly as it does for every lattice fill.
        result[c] = NodePosition(dmin, dmax, lo + frac, count);
    }

    out[0] = result[0];
    out[1] = result[1];
    out[2] = result[2];
    return true;
}

//---------------------------------------------------------------- ComposeCube

bool ComposeCube(const CubeLUT& first, const CubeLUT& second, int outSize,
                 CubeLUT& out, std::string* err)
{
    if (!first.IsValid())
    {
        SetErr(err, "first lut is not valid");
        return false;
    }
    if (!second.IsValid())
    {
        SetErr(err, "second lut is not valid");
        return false;
    }
    if (!CubeLUT::ValidSize(outSize))
    {
        SetSizeErr(err);
        return false;
    }
    if (!CompatibleSignalTypes(first.Contract().output,
                               second.Contract().input))
    {
        SetErr(err, "first output signal does not match second input signal");
        return false;
    }

    // Capture everything needed from the inputs before any write, so out may
    // alias either of them.
    double dmin[3], dmax[3];
    first.GetDomain(dmin, dmax);
    LutContract stamp;
    stamp.input = first.Contract().input;
    stamp.output = second.Contract().output;

    try
    {
        CubeLUT result;
        if (!result.Create(outSize))
        {
            SetErr(err, "could not allocate the result lattice");
            return false;
        }
        if (!result.SetDomain(dmin, dmax))
        {
            SetErr(err, "first lut has an unusable domain");
            return false;
        }

        // One node-position array per axis: bit-identical positions for
        // every node, computed once instead of per lattice entry.
        std::vector<double> nodeR(outSize), nodeG(outSize), nodeB(outSize);
        for (int i = 0; i < outSize; ++i)
        {
            nodeR[i] = NodePosition(dmin[0], dmax[0], i, outSize);
            nodeG[i] = NodePosition(dmin[1], dmax[1], i, outSize);
            nodeB[i] = NodePosition(dmin[2], dmax[2], i, outSize);
        }

        for (int b = 0; b < outSize; ++b)
            for (int g = 0; g < outSize; ++g)
                for (int r = 0; r < outSize; ++r)
                {
                    double p[3] = { nodeR[r], nodeG[g], nodeB[b] };
                    double mid[3], v[3];
                    if (!first.Evaluate(p, mid) || !second.Evaluate(mid, v)
                        || !result.SetEntry(r, g, b, v))
                    {
                        SetErr(err, "composition produced an unusable value");
                        return false;
                    }
                }

        if (!result.SetContract(stamp))
        {
            SetErr(err, "result contract is not valid");
            return false;
        }

        out = std::move(result);    // noexcept commit: out is never torn
    }
    catch (std::bad_alloc&)
    {
        SetErr(err, "out of memory building the result lattice");
        return false;
    }
    ClearErr(err);
    return true;
}

//--------------------------------------------------------------- ResampleCube

bool ResampleCube(const CubeLUT& src, int newSize, CubeLUT& out,
                  std::string* err)
{
    if (!src.IsValid())
    {
        SetErr(err, "source lut is not valid");
        return false;
    }
    if (!CubeLUT::ValidSize(newSize))
    {
        SetSizeErr(err);
        return false;
    }

    double dmin[3], dmax[3];
    src.GetDomain(dmin, dmax);
    LutContract stamp = src.Contract();

    try
    {
        // Same size: a verbatim copy of the lattice. This is what makes the
        // header's exactness guarantee unconditional - round-tripping every
        // node through the evaluator can pick up last-place rounding for
        // some sizes and domains, a copy cannot.
        if (newSize == src.Size())
        {
            CubeLUT result = src;
            if (!result.SetTitle(std::string()))
            {
                SetErr(err, "could not clear the result title");
                return false;
            }
            out = std::move(result);
            ClearErr(err);
            return true;
        }

        CubeLUT result;
        if (!result.Create(newSize))
        {
            SetErr(err, "could not allocate the result lattice");
            return false;
        }
        if (!result.SetDomain(dmin, dmax))
        {
            SetErr(err, "source lut has an unusable domain");
            return false;
        }

        std::vector<double> nodeR(newSize), nodeG(newSize), nodeB(newSize);
        for (int i = 0; i < newSize; ++i)
        {
            nodeR[i] = NodePosition(dmin[0], dmax[0], i, newSize);
            nodeG[i] = NodePosition(dmin[1], dmax[1], i, newSize);
            nodeB[i] = NodePosition(dmin[2], dmax[2], i, newSize);
        }

        for (int b = 0; b < newSize; ++b)
            for (int g = 0; g < newSize; ++g)
                for (int r = 0; r < newSize; ++r)
                {
                    double p[3] = { nodeR[r], nodeG[g], nodeB[b] };
                    double v[3];
                    if (!src.Evaluate(p, v) || !result.SetEntry(r, g, b, v))
                    {
                        SetErr(err, "resampling produced an unusable value");
                        return false;
                    }
                }

        if (!result.SetContract(stamp))
        {
            SetErr(err, "source contract is not valid");
            return false;
        }

        out = std::move(result);    // noexcept commit: out is never torn
    }
    catch (std::bad_alloc&)
    {
        SetErr(err, "out of memory building the result lattice");
        return false;
    }
    ClearErr(err);
    return true;
}

//-------------------------------------------------------- ExtractNeutralShaper

bool ExtractNeutralShaper(const CubeLUT& lut, ShaperCurve& shaper,
                          std::string* err)
{
    if (!lut.IsValid())
    {
        SetErr(err, "lut is not valid");
        return false;
    }

    double dmin[3], dmax[3];
    lut.GetDomain(dmin, dmax);

    try
    {
        ShaperCurve result;
        std::vector<double> samples;
        for (int c = 0; c < 3; ++c)
        {
            if (!DiagonalSamples(lut, c, samples))
            {
                std::string reason =
                    "neutral axis is not strictly increasing in the ";
                reason += ChannelName(c);
                reason += " channel";
                if (err)
                    *err = reason;
                return false;
            }
            if (!result.SetChannel(c, &samples[0], (int)samples.size(),
                                   dmin[c], dmax[c]))
            {
                std::string reason = "could not build the ";
                reason += ChannelName(c);
                reason += " shaper channel";
                if (err)
                    *err = reason;
                return false;
            }
        }

        shaper = std::move(result);     // noexcept commit
    }
    catch (std::bad_alloc&)
    {
        SetErr(err, "out of memory building the shaper");
        return false;
    }
    ClearErr(err);
    return true;
}

//----------------------------------------------------------- SplitShaperCube

bool SplitShaperCube(const CubeLUT& src, int cubeSize, ShaperCurve& shaper,
                     CubeLUT& cube, std::string* err)
{
    if (!src.IsValid())
    {
        SetErr(err, "source lut is not valid");
        return false;
    }
    if (!CubeLUT::ValidSize(cubeSize))
    {
        SetSizeErr(err);
        return false;
    }

    try
    {
        // Build the shaper to the side: a refusal below must leave BOTH
        // outputs untouched.
        ShaperCurve localShaper;
        if (!ExtractNeutralShaper(src, localShaper, err))
            return false;

        // The residual cube lives over the shaper's VALUE RANGE, so its
        // input domain is exactly what the shaper can emit.
        double cmin[3], cmax[3];
        for (int c = 0; c < 3; ++c)
        {
            const int k = localShaper.Count(c);
            if (!localShaper.GetSample(c, 0, cmin[c])
                || !localShaper.GetSample(c, k - 1, cmax[c]))
            {
                SetErr(err, "could not read the shaper value range");
                return false;
            }
        }

        LutContract stamp;
        stamp.input = LutSignalType();  // the intermediate point is untyped
        stamp.output = src.Contract().output;

        CubeLUT result;
        if (!result.Create(cubeSize))
        {
            SetErr(err, "could not allocate the residual cube");
            return false;
        }
        if (!result.SetDomain(cmin, cmax))
        {
            SetErr(err, "shaper value range is not a usable cube domain");
            return false;
        }

        // The shaper inverse is separable per channel and each axis takes
        // only cubeSize distinct node values, so invert once per axis index
        // (cubeSize inversions) instead of once per lattice entry
        // (cubeSize^3 of them). The assembled t is bit identical to the
        // per-entry computation it replaces.
        std::vector<double> invR(cubeSize), invG(cubeSize), invB(cubeSize);
        for (int i = 0; i < cubeSize; ++i)
        {
            double y[3] = {
                NodePosition(cmin[0], cmax[0], i, cubeSize),
                NodePosition(cmin[1], cmax[1], i, cubeSize),
                NodePosition(cmin[2], cmax[2], i, cubeSize),
            };
            double t[3];
            if (!localShaper.EvaluateInverse(y, t))
            {
                SetErr(err, "split produced an unusable value");
                return false;
            }
            invR[i] = t[0];
            invG[i] = t[1];
            invB[i] = t[2];
        }

        for (int b = 0; b < cubeSize; ++b)
            for (int g = 0; g < cubeSize; ++g)
                for (int r = 0; r < cubeSize; ++r)
                {
                    double t[3] = { invR[r], invG[g], invB[b] };
                    double v[3];
                    if (!src.Evaluate(t, v) || !result.SetEntry(r, g, b, v))
                    {
                        SetErr(err, "split produced an unusable value");
                        return false;
                    }
                }

        if (!result.SetContract(stamp))
        {
            SetErr(err, "residual cube contract is not valid");
            return false;
        }

        // Both commits are noexcept moves, so the pair lands atomically:
        // neither output can be torn or half-updated.
        shaper = std::move(localShaper);
        cube = std::move(result);
    }
    catch (std::bad_alloc&)
    {
        SetErr(err, "out of memory building the residual cube");
        return false;
    }
    ClearErr(err);
    return true;
}
