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

#include "DisplayModel.h"

#include <cmath>
#include <cstring>

namespace
{
    const double kZeroSignal   = 1e-9;    // at or below: channel is truly off
    const double kMinFitSignal = 0.02;    // gamma regression uses samples above this
    const double kMaxFitSignal = 0.999;   // and below this (peak still pins the matrix)
    const double kSignalTol    = 1e-9;    // contract tolerance on the [0..1] stimulus range
    const double kGrayTol      = 1e-6;    // gray classification: absorbs per-channel
                                          // quantization jitter (10/12-bit rounding);
                                          // safe to be loose - grays are only ever the
                                          // fallback pool, never preferred over ramps
    const double kGammaMin     = 0.5;
    const double kGammaMax     = 5.0;
    const double kDefaultGamma = 2.2;

    bool Finite(double v)
    {
        return std::isfinite(v);
    }

    double Norm3(const double v[3])
    {
        return sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    }

    // 3x3 inverse via adjugate. Returns false when |det| is degenerate
    // relative to the matrix magnitude, or not finite.
    bool Invert3x3(const double a[3][3], double out[3][3])
    {
        double c00 = a[1][1] * a[2][2] - a[1][2] * a[2][1];
        double c01 = a[1][2] * a[2][0] - a[1][0] * a[2][2];
        double c02 = a[1][0] * a[2][1] - a[1][1] * a[2][0];
        double det = a[0][0] * c00 + a[0][1] * c01 + a[0][2] * c02;

        double scale = 0.0;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                scale += fabs(a[i][j]);
        if (!Finite(det) || fabs(det) <= 1e-12 * scale * scale * scale)
            return false;

        out[0][0] = c00 / det;
        out[1][0] = c01 / det;
        out[2][0] = c02 / det;
        out[0][1] = (a[0][2] * a[2][1] - a[0][1] * a[2][2]) / det;
        out[1][1] = (a[0][0] * a[2][2] - a[0][2] * a[2][0]) / det;
        out[2][1] = (a[0][1] * a[2][0] - a[0][0] * a[2][1]) / det;
        out[0][2] = (a[0][1] * a[1][2] - a[0][2] * a[1][1]) / det;
        out[1][2] = (a[0][2] * a[1][0] - a[0][0] * a[1][2]) / det;
        out[2][2] = (a[0][0] * a[1][1] - a[0][1] * a[1][0]) / det;
        return true;
    }

    // Streaming weighted linear regression of y on x. The degeneracy test is
    // relative to the accumulated weight, so uniformly rescaling all weights
    // cannot change whether the slope resolves (the weight contract is
    // "arbitrary-scale relative confidence").
    struct SlopeAccum
    {
        int    n;
        double sw, sx, sy, sxx, sxy;

        SlopeAccum() : n(0), sw(0.0), sx(0.0), sy(0.0), sxx(0.0), sxy(0.0) {}

        void Add(double x, double y, double w)
        {
            ++n;
            sw  += w;
            sx  += w * x;
            sy  += w * y;
            sxx += w * x * x;
            sxy += w * x * y;
        }

        bool Slope(double* out) const
        {
            if (n < 2 || sw <= 0.0)
                return false;
            double denom = sxx - sx * sx / sw;   // sw * weighted Var(x)
            if (!Finite(denom) || denom <= 1e-12 * sw)
                return false;
            *out = (sxy - sx * sy / sw) / denom;
            return Finite(*out);
        }
    };

    double ClampGamma(double g)
    {
        if (g < kGammaMin) return kGammaMin;
        if (g > kGammaMax) return kGammaMax;
        return g;
    }
}

DisplayModel::DisplayModel()
{
    memset(m_M, 0, sizeof(m_M));
    memset(m_Minv, 0, sizeof(m_Minv));
    memset(m_black, 0, sizeof(m_black));
    m_gamma[0] = m_gamma[1] = m_gamma[2] = kDefaultGamma;
    m_valid = false;
    memset(&m_report, 0, sizeof(m_report));
    // Never claim measured provenance before a fit: FITTED is enum value 0.
    for (int c = 0; c < 3; ++c)
        m_report.gammaSource[c] = DM_PARAM_ASSUMED;
    m_report.blackSource = DM_PARAM_ASSUMED;
}

bool DisplayModel::Fit(const std::vector<DisplayModelSample>& samples)
{
    // The fit computes into locals and commits to members only on success, so
    // a failed (re-)fit leaves the previous model fully intact.

    // ---- Contract validation + black offset in one pass.
    // Black offset = weighted mean XYZ of the truly-black samples (all
    // channels off); a lit near-black patch must not be pooled into flare.
    double blackW = 0.0, blackSum[3] = { 0.0, 0.0, 0.0 };
    int usable = 0;
    for (size_t i = 0; i < samples.size(); ++i)
    {
        const DisplayModelSample& s = samples[i];
        if (!Finite(s.weight) || s.weight < 0.0)
            return false;
        if (s.weight == 0.0)
            continue;
        for (int k = 0; k < 3; ++k)
        {
            if (!Finite(s.rgb[k]) || !Finite(s.XYZ[k]))
                return false;
            if (s.rgb[k] < -kSignalTol || s.rgb[k] > 1.0 + kSignalTol)
                return false;
        }
        ++usable;
        if (s.rgb[0] <= kZeroSignal && s.rgb[1] <= kZeroSignal &&
            s.rgb[2] <= kZeroSignal)
        {
            blackW += s.weight;
            for (int k = 0; k < 3; ++k)
                blackSum[k] += s.weight * s.XYZ[k];
        }
    }
    if (usable < 8)
        return false;

    double black[3];
    for (int k = 0; k < 3; ++k)
        black[k] = (blackW > 0.0) ? blackSum[k] / blackW : 0.0;
    DisplayModelParamSource blackSource =
        (blackW > 0.0) ? DM_PARAM_FITTED : DM_PARAM_ASSUMED;

    // ---- Gamma per channel, classified in a single pass over the samples.
    // Above black, a single-channel drive gives (XYZ - black) = column_c * s^gamma,
    // so ln|XYZ - black| is linear in ln s with slope gamma. Grays obey the same
    // relation only when all channels share a gamma, so they are the fallback
    // pool, not the primary one.
    SlopeAccum channelPool[3], grayPool;
    for (size_t i = 0; i < samples.size(); ++i)
    {
        const DisplayModelSample& s = samples[i];
        if (s.weight <= 0.0)
            continue;
        double delta[3];
        for (int k = 0; k < 3; ++k)
            delta[k] = s.XYZ[k] - black[k];
        double mag = Norm3(delta);
        if (mag <= 0.0)
            continue;
        for (int c = 0; c < 3; ++c)
        {
            int o1 = (c + 1) % 3, o2 = (c + 2) % 3;
            if (s.rgb[o1] <= kZeroSignal && s.rgb[o2] <= kZeroSignal &&
                s.rgb[c] > kMinFitSignal && s.rgb[c] < kMaxFitSignal)
                channelPool[c].Add(log(s.rgb[c]), log(mag), s.weight);
        }
        if (fabs(s.rgb[0] - s.rgb[1]) <= kGrayTol &&
            fabs(s.rgb[1] - s.rgb[2]) <= kGrayTol &&
            s.rgb[0] > kMinFitSignal && s.rgb[0] < kMaxFitSignal)
            grayPool.Add(log(s.rgb[0]), log(mag), s.weight);
    }

    double graySlope = 0.0;
    bool grayResolved = grayPool.Slope(&graySlope);

    double gamma[3];
    DisplayModelParamSource gammaSource[3];
    for (int c = 0; c < 3; ++c)
    {
        double slope = 0.0;
        if (channelPool[c].Slope(&slope))
        {
            gamma[c] = ClampGamma(slope);
            gammaSource[c] = DM_PARAM_FITTED;
        }
        else if (grayResolved)
        {
            gamma[c] = ClampGamma(graySlope);
            gammaSource[c] = DM_PARAM_FROM_GRAYS;
        }
        else
        {
            gamma[c] = kDefaultGamma;
            gammaSource[c] = DM_PARAM_ASSUMED;
        }
    }

    // ---- Matrix by weighted least squares over ALL usable samples.
    // (XYZ - black) ~= M * L, L_c = rgb_c^gamma_c.
    // M = A * B^-1 with A = sum w * (XYZ-black) * L^T, B = sum w * L * L^T.
    double A[3][3] = { { 0 } }, B[3][3] = { { 0 } };
    for (size_t i = 0; i < samples.size(); ++i)
    {
        const DisplayModelSample& s = samples[i];
        if (s.weight <= 0.0)
            continue;
        double L[3], delta[3];
        for (int k = 0; k < 3; ++k)
        {
            L[k] = (s.rgb[k] <= 0.0) ? 0.0 : pow(s.rgb[k], gamma[k]);
            delta[k] = s.XYZ[k] - black[k];
        }
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
            {
                A[r][c] += s.weight * delta[r] * L[c];
                B[r][c] += s.weight * L[r] * L[c];
            }
    }
    double Binv[3][3], M[3][3], Minv[3][3];
    if (!Invert3x3(B, Binv))
        return false;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
        {
            M[r][c] = 0.0;
            for (int k = 0; k < 3; ++k)
                M[r][c] += A[r][k] * Binv[k][c];
        }
    if (!Invert3x3(M, Minv))
        return false;

    // ---- Success: commit the fitted state, then report residuals through
    // the same forward path callers will use.
    memcpy(m_M, M, sizeof(m_M));
    memcpy(m_Minv, Minv, sizeof(m_Minv));
    memcpy(m_black, black, sizeof(m_black));
    memcpy(m_gamma, gamma, sizeof(m_gamma));
    m_valid = true;

    double sw = 0.0, swr2 = 0.0, maxR = 0.0;
    for (size_t i = 0; i < samples.size(); ++i)
    {
        const DisplayModelSample& s = samples[i];
        if (s.weight <= 0.0)
            continue;
        double pred[3], resid[3];
        SignalToXYZ(s.rgb, pred);
        for (int k = 0; k < 3; ++k)
            resid[k] = s.XYZ[k] - pred[k];
        double r = Norm3(resid);
        sw += s.weight;
        swr2 += s.weight * r * r;
        if (r > maxR)
            maxR = r;
    }
    m_report.samplesUsed = usable;
    m_report.rmsXYZ = (sw > 0.0) ? sqrt(swr2 / sw) : 0.0;
    m_report.maxXYZ = maxR;
    for (int c = 0; c < 3; ++c)
        m_report.gammaSource[c] = gammaSource[c];
    m_report.blackSource = blackSource;
    return true;
}

void DisplayModel::SignalToXYZ(const double rgb[3], double XYZ[3]) const
{
    if (!m_valid)
    {
        XYZ[0] = XYZ[1] = XYZ[2] = 0.0;
        return;
    }
    double L[3];
    for (int k = 0; k < 3; ++k)
    {
        double s = rgb[k];
        if (s < 0.0) s = 0.0;
        if (s > 1.0) s = 1.0;
        L[k] = (s <= 0.0) ? 0.0 : pow(s, m_gamma[k]);
    }
    for (int r = 0; r < 3; ++r)
    {
        XYZ[r] = m_black[r];
        for (int c = 0; c < 3; ++c)
            XYZ[r] += m_M[r][c] * L[c];
    }
}

bool DisplayModel::XYZToSignal(const double XYZ[3], double rgb[3]) const
{
    if (!m_valid)
    {
        rgb[0] = rgb[1] = rgb[2] = 0.0;
        return false;
    }
    const double kClipTolerance = 1e-9;
    double delta[3], L[3];
    for (int k = 0; k < 3; ++k)
        delta[k] = XYZ[k] - m_black[k];
    bool inGamut = true;
    for (int r = 0; r < 3; ++r)
    {
        L[r] = 0.0;
        for (int c = 0; c < 3; ++c)
            L[r] += m_Minv[r][c] * delta[c];
        if (L[r] < -kClipTolerance || L[r] > 1.0 + kClipTolerance)
            inGamut = false;
        if (L[r] < 0.0) L[r] = 0.0;
        if (L[r] > 1.0) L[r] = 1.0;
        rgb[r] = (L[r] <= 0.0) ? 0.0 : pow(L[r], 1.0 / m_gamma[r]);
    }
    return inGamut;
}
