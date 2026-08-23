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
    const double kNearZeroSignal = 0.01;   // at or below: counts as channel off
    const double kMinFitSignal   = 0.02;   // regression uses samples above this
    const double kMaxFitSignal   = 0.999;  // and below this (peak pins the matrix)
    const double kGammaMin       = 0.5;
    const double kGammaMax       = 5.0;
    const double kDefaultGamma   = 2.2;

    double Norm3(const double v[3])
    {
        return sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    }

    // 3x3 inverse via adjugate. Returns false when |det| is degenerate
    // relative to the matrix magnitude.
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
        if (fabs(det) <= 1e-12 * scale * scale * scale || det == 0.0)
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

    // Weighted linear regression of y on x: returns slope, or fallback when
    // fewer than two distinct x values carry weight.
    double WeightedSlope(const std::vector<double>& x, const std::vector<double>& y,
                         const std::vector<double>& w, double fallback)
    {
        double sw = 0.0, sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
        for (size_t i = 0; i < x.size(); ++i)
        {
            sw  += w[i];
            sx  += w[i] * x[i];
            sy  += w[i] * y[i];
            sxx += w[i] * x[i] * x[i];
            sxy += w[i] * x[i] * y[i];
        }
        if (sw <= 0.0)
            return fallback;
        double denom = sxx - sx * sx / sw;
        if (denom <= 1e-12)
            return fallback;
        return (sxy - sx * sy / sw) / denom;
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
}

bool DisplayModel::Fit(const std::vector<DisplayModelSample>& samples)
{
    m_valid = false;
    memset(&m_report, 0, sizeof(m_report));

    // ---- Pass 1: black offset = weighted mean XYZ of all-channels-off samples.
    double blackW = 0.0, blackSum[3] = { 0.0, 0.0, 0.0 };
    int usable = 0;
    for (size_t i = 0; i < samples.size(); ++i)
    {
        const DisplayModelSample& s = samples[i];
        if (s.weight <= 0.0)
            continue;
        ++usable;
        if (s.rgb[0] <= kNearZeroSignal && s.rgb[1] <= kNearZeroSignal &&
            s.rgb[2] <= kNearZeroSignal)
        {
            blackW += s.weight;
            for (int k = 0; k < 3; ++k)
                blackSum[k] += s.weight * s.XYZ[k];
        }
    }
    if (usable < 8)
        return false;
    for (int k = 0; k < 3; ++k)
        m_black[k] = (blackW > 0.0) ? blackSum[k] / blackW : 0.0;

    // ---- Pass 2: per-channel gamma from single-channel samples, gray fallback.
    // Above black, a single-channel drive gives (XYZ - black) = column_c * s^gamma,
    // so ln|XYZ - black| is linear in ln s with slope gamma. Grays obey the same
    // relation only when all channels share a gamma, so they are the fallback
    // pool, not the primary one.
    std::vector<double> grayX, grayY, grayW;
    for (int c = 0; c < 3; ++c)
    {
        std::vector<double> lx, ly, lw;
        for (size_t i = 0; i < samples.size(); ++i)
        {
            const DisplayModelSample& s = samples[i];
            if (s.weight <= 0.0)
                continue;
            int o1 = (c + 1) % 3, o2 = (c + 2) % 3;
            bool single = s.rgb[o1] <= kNearZeroSignal && s.rgb[o2] <= kNearZeroSignal &&
                          s.rgb[c] > kMinFitSignal && s.rgb[c] < kMaxFitSignal;
            bool gray = c == 0 &&   // collect grays once
                        fabs(s.rgb[0] - s.rgb[1]) <= 1e-9 && fabs(s.rgb[1] - s.rgb[2]) <= 1e-9 &&
                        s.rgb[0] > kMinFitSignal && s.rgb[0] < kMaxFitSignal;
            double delta[3];
            for (int k = 0; k < 3; ++k)
                delta[k] = s.XYZ[k] - m_black[k];
            double mag = Norm3(delta);
            if (mag <= 0.0)
                continue;
            if (single)
            {
                lx.push_back(log(s.rgb[c]));
                ly.push_back(log(mag));
                lw.push_back(s.weight);
            }
            if (gray)
            {
                grayX.push_back(log(s.rgb[0]));
                grayY.push_back(log(mag));
                grayW.push_back(s.weight);
            }
        }
        m_gamma[c] = (lx.size() >= 2)
            ? WeightedSlope(lx, ly, lw, kDefaultGamma)
            : 0.0;   // resolved from grays below
    }
    double grayGamma = WeightedSlope(grayX, grayY, grayW, kDefaultGamma);
    for (int c = 0; c < 3; ++c)
    {
        if (m_gamma[c] == 0.0)
            m_gamma[c] = grayGamma;
        if (m_gamma[c] < kGammaMin) m_gamma[c] = kGammaMin;
        if (m_gamma[c] > kGammaMax) m_gamma[c] = kGammaMax;
    }

    // ---- Pass 3: matrix by weighted least squares over ALL usable samples.
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
            L[k] = (s.rgb[k] <= 0.0) ? 0.0 : pow(s.rgb[k], m_gamma[k]);
            delta[k] = s.XYZ[k] - m_black[k];
        }
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
            {
                A[r][c] += s.weight * delta[r] * L[c];
                B[r][c] += s.weight * L[r] * L[c];
            }
    }
    double Binv[3][3];
    if (!Invert3x3(B, Binv))
        return false;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
        {
            m_M[r][c] = 0.0;
            for (int k = 0; k < 3; ++k)
                m_M[r][c] += A[r][k] * Binv[k][c];
        }
    if (!Invert3x3(m_M, m_Minv))
        return false;

    // ---- Residual report over the samples the fit saw.
    m_valid = true;   // SignalToXYZ needs the fitted state
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
        m_report.gamma[c] = m_gamma[c];
    m_report.valid = true;
    return true;
}

void DisplayModel::SignalToXYZ(const double rgb[3], double XYZ[3]) const
{
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
