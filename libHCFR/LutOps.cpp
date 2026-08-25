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

#include <float.h>
#include <math.h>

namespace
{
    inline bool Finite(double v)
    {
        return _finite(v) != 0;
    }

    inline bool FiniteTriple(const double v[3])
    {
        return Finite(v[0]) && Finite(v[1]) && Finite(v[2]);
    }

    // Lattice node position for index i of count nodes spanning
    // [dmin, dmax]: mathematically dmin + i/(count-1) * (dmax - dmin),
    // written in the symmetric interpolation form
    // dmin*(1-u) + dmax*u. The symmetric form reproduces BOTH endpoints
    // exactly (u = 0 gives dmin, u = 1 gives dmax, with no dependence on
    // the rounding of the span) and keeps the interior nodes on the value
    // the evaluator's own inverse mapping (x - dmin) / span recovers, which
    // is what makes a same-size resample reproduce the source lattice.
    // One helper for every operation, so a node computed by one is bit
    // identical to the node computed by another.
    inline double NodePosition(double dmin, double dmax, int index, int count)
    {
        const double u = index / (double)(count - 1);
        return dmin * (1.0 - u) + dmax * u;
    }

    inline bool ValidLatticeSize(int size)
    {
        return size >= 2 && size <= 256;
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

    // Copies one lattice's worth of nodes from 'source domain' into 'dest',
    // where the value at each node comes from the caller. Kept inline in the
    // operations instead (each has a different node map), so nothing here.

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
    if (!Finite(dmin) || !Finite(dmax) || !(dmin < dmax)
        || !Finite(dmax - dmin))
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
        // Same node formula as Evaluate, so the round trip is exact to
        // rounding at the knots.
        result[c] = dmin + (lo + frac) / (double)(count - 1) * (dmax - dmin);
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
    if (!ValidLatticeSize(outSize))
    {
        SetErr(err, "output size must be between 2 and 256");
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

    for (int b = 0; b < outSize; ++b)
        for (int g = 0; g < outSize; ++g)
            for (int r = 0; r < outSize; ++r)
            {
                double p[3] = {
                    NodePosition(dmin[0], dmax[0], r, outSize),
                    NodePosition(dmin[1], dmax[1], g, outSize),
                    NodePosition(dmin[2], dmax[2], b, outSize),
                };
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

    out = result;
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
    if (!ValidLatticeSize(newSize))
    {
        SetErr(err, "new size must be between 2 and 256");
        return false;
    }

    double dmin[3], dmax[3];
    src.GetDomain(dmin, dmax);
    LutContract stamp = src.Contract();

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

    for (int b = 0; b < newSize; ++b)
        for (int g = 0; g < newSize; ++g)
            for (int r = 0; r < newSize; ++r)
            {
                double p[3] = {
                    NodePosition(dmin[0], dmax[0], r, newSize),
                    NodePosition(dmin[1], dmax[1], g, newSize),
                    NodePosition(dmin[2], dmax[2], b, newSize),
                };
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

    out = result;
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

    ShaperCurve result;
    std::vector<double> samples;
    for (int c = 0; c < 3; ++c)
    {
        if (!DiagonalSamples(lut, c, samples))
        {
            std::string reason = "neutral axis is not strictly increasing in the ";
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

    shaper = result;
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
    if (!ValidLatticeSize(cubeSize))
    {
        SetErr(err, "cube size must be between 2 and 256");
        return false;
    }

    // Build the shaper to the side: a refusal below must leave BOTH outputs
    // untouched.
    ShaperCurve localShaper;
    if (!ExtractNeutralShaper(src, localShaper, err))
        return false;

    // The residual cube lives over the shaper's VALUE RANGE, so its input
    // domain is exactly what the shaper can emit.
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
    stamp.input = LutSignalType();          // the intermediate point is untyped
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

    for (int b = 0; b < cubeSize; ++b)
        for (int g = 0; g < cubeSize; ++g)
            for (int r = 0; r < cubeSize; ++r)
            {
                double y[3] = {
                    NodePosition(cmin[0], cmax[0], r, cubeSize),
                    NodePosition(cmin[1], cmax[1], g, cubeSize),
                    NodePosition(cmin[2], cmax[2], b, cubeSize),
                };
                double t[3], v[3];
                if (!localShaper.EvaluateInverse(y, t) || !src.Evaluate(t, v)
                    || !result.SetEntry(r, g, b, v))
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

    shaper = localShaper;
    cube = result;
    ClearErr(err);
    return true;
}
