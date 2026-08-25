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

// LUT lattice algebra: the operations that build and take apart CubeLUT
// assets - typed composition, resampling, and the shaper+cube split
// (mathematical removal of the neutral-axis 1D response, never point
// deletion). Independent implementation from the underlying mathematics;
// no code from any other project.
//
// Error-reporting convention: every operation returns bool. On refusal the
// OUTPUT objects are left exactly as they were, and when the caller passes
// a non-null err string a one-line lowercase reason is written to it; a
// successful call clears err. Output objects may alias input objects
// (results are computed to the side and committed only on success, by
// noexcept moves - so an output is never torn). Allocation failure on a
// large lattice reports the same way (false + err), never as an escaping
// exception.

#ifndef LUTOPS_H
#define LUTOPS_H

#include <string>
#include <vector>
#include "CubeLUT.h"
#include "LutContract.h"

// ShaperCurve: three per-channel 1D curves (R, G, B), each a strictly
// increasing sequence of samples placed uniformly over that channel's own
// domain [dmin, dmax]. Evaluation is piecewise-linear between samples;
// because every channel is strictly increasing the piecewise-linear inverse
// exists and is exact (the inverse of a PWL curve is the PWL curve through
// the swapped (value, position) pairs - EvaluateInverse(Evaluate(x)) == x
// up to rounding, and vice versa within the value range).
//
// The curve carries no LutContract of its own yet: a shaper split from a
// CubeLUT conceptually inherits the source LUT's input signal, and typed
// 1D stages arrive when 1D LUTs become standalone assets.
class ShaperCurve
{
public:
    ShaperCurve();

    // Install one channel (0=R, 1=G, 2=B): count >= 2 samples, every sample
    // finite and STRICTLY increasing, placed uniformly over [dmin, dmax]
    // with dmin < dmax and a finite span. Returns false (channel unchanged)
    // on a bad channel index, count < 2, any non-finite sample, any
    // sample[i+1] <= sample[i], or a bad domain.
    bool SetChannel(int channel, const double* samples, int count,
                    double dmin, double dmax);

    // True only when all three channels have been installed.
    bool IsValid() const;

    // Per-channel introspection. Count returns 0 (and the getters return
    // false, writing nothing) for a bad or not-yet-installed channel;
    // GetSample also for an out-of-range index.
    int  Count(int channel) const;
    bool GetDomain(int channel, double& dmin, double& dmax) const;
    bool GetSample(int channel, int index, double& value) const;

    // Piecewise-linear evaluation per channel. Input components are clamped
    // to the channel's domain first (so out-of-domain inputs evaluate at
    // the nearest end). Returns false with out zeroed on an invalid object
    // or a non-finite input component. in and out may alias.
    bool Evaluate(const double in[3], double out[3]) const;

    // Exact piecewise-linear inverse per channel: maps a VALUE back to the
    // position that produces it. Input components are clamped to the
    // channel's value range [first sample, last sample] first. Returns
    // false with out zeroed on an invalid object or a non-finite input
    // component. in and out may alias.
    bool EvaluateInverse(const double in[3], double out[3]) const;

private:
    // Channel c: m_samples[c] (empty = not installed) over
    // [m_domainMin[c], m_domainMax[c]].
    std::vector<double> m_samples[3];
    double m_domainMin[3];
    double m_domainMax[3];
};

// out = second(first(x)), materialized as an outSize^3 lattice: entry at
// lattice node p (in FIRST's domain, node convention dmin + i/(N-1)*span
// per component, computed in the endpoint-exact symmetric form
// dmin*(1-u) + dmax*u; interior nodes carry ordinary last-place rounding)
// is second.Evaluate(first.Evaluate(p)). The result keeps
// first's domain and an empty title. Values of first that fall outside
// second's domain are clamped by second's evaluator (the documented
// CubeLUT::Evaluate domain-box clamp) - the contract gate below is the
// intended guard against composing stages that do not belong together.
//
// Contract gate: refuses (nothing written) unless first's OUTPUT signal
// type is compatible with second's INPUT signal type (every field equal or
// unspecified on either side; see LutContract.h). On success the result is
// stamped with input = first's input signal and output = second's output
// signal, exactly as given (no field merging).
//
// Also refuses when either LUT is invalid or outSize is outside 2..256.
// out may alias first or second.
bool ComposeCube(const CubeLUT& first, const CubeLUT& second, int outSize,
                 CubeLUT& out, std::string* err);

// out = src re-sampled through its own evaluator on a newSize^3 lattice
// over src's domain: entry at node p is src.Evaluate(p). Keeps src's
// domain and contract; the title starts empty. A same-size resample is a
// verbatim copy of the lattice, so it reproduces every entry of src
// exactly for any size and domain (round-tripping node positions through
// the evaluator could pick up last-place rounding; a copy cannot).
// Refuses (nothing written) on an invalid src or newSize outside 2..256.
// out may alias src.
bool ResampleCube(const CubeLUT& src, int newSize, CubeLUT& out,
                  std::string* err);

// Extracts the neutral-axis response of lut into shaper: channel c gets
// K = lut.Size() samples where sample i is EXACTLY the lattice diagonal
// entry (i,i,i) component c (identical to evaluating at the diagonal node,
// by node exactness), over the domain [dmin_c, dmax_c] of lut. Refuses
// (shaper untouched) when lut is invalid or any channel's diagonal is not
// strictly increasing (the reason names the offending channel).
bool ExtractNeutralShaper(const CubeLUT& lut, ShaperCurve& shaper,
                          std::string* err);

// Shaper+cube split: factors src into src == cube AFTER shaper (the shaper
// runs first, per-channel; the cube then absorbs everything the 1D neutral
// response cannot express). shaper is ExtractNeutralShaper(src); cube is
// the residual src composed with the shaper's exact inverse,
//
//     cube(y) = src.Evaluate(shaper.EvaluateInverse(y)),
//
// materialized as a cubeSize^3 lattice over the shaper's VALUE RANGE:
// cube's domain component c is [shaper sample 0, shaper sample K-1] of
// channel c (so the cube's input domain is exactly what the shaper can
// emit). Consequences the caller may rely on, each up to last-place
// rounding of the chained evaluations: cube(shaper(x)) reproduces src(x);
// along the shaper's image of the gray diagonal the cube is the identity
// (the neutral response lives entirely in the shaper); and for a src that
// is purely per-channel 1D the whole cube collapses to the identity
// lattice.
//
// Contracts: cube is stamped with an UNSPECIFIED input signal (the
// intermediate point has no standard name) and src's output signal.
//
// Refuses (both outputs untouched) when src is invalid, cubeSize is
// outside 2..256, or the neutral axis is not strictly increasing in every
// channel.
bool SplitShaperCube(const CubeLUT& src, int cubeSize, ShaperCurve& shaper,
                     CubeLUT& cube, std::string* err);

#endif // LUTOPS_H
