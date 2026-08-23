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

// DisplayModel: forward model of a display fitted from weighted characterization
// samples. Version 1 is the matrix + per-channel-shaper tier: a per-channel
// power-law shaper (gamma), an additive black offset (flare), and a 3x3 matrix
// mapping linearized RGB to CIE XYZ. The fit interface takes (stimulus, value,
// weight) tuples so later scattered-data correction layers slot in behind the
// same API without plumbing changes.
//
// Units: stimulus RGB is normalized signal in [0..1] per channel (after any
// range decode). XYZ is in whatever consistent luminance unit the samples use.

#ifndef DISPLAYMODEL_H
#define DISPLAYMODEL_H

#include <vector>

// One characterization sample. weight >= 0; weight 0 excludes the sample.
// For samples with weight > 0, Fit requires every field finite and each
// stimulus channel within [0..1]; a violating sample rejects the whole fit
// rather than being silently dropped or misused. Measured XYZ may be
// slightly negative (instrument noise near black) - only finiteness is
// required of it.
struct DisplayModelSample
{
    double rgb[3];    // stimulus, normalized signal 0..1
    double XYZ[3];    // measured CIE XYZ
    double weight;    // relative confidence (e.g. 1/variance); 0 = ignore
};

// How each fitted parameter group was determined. A parameter the sample set
// could not constrain is filled with a documented assumption and flagged, so
// callers can always tell a measured value from an assumed one.
enum DisplayModelParamSource
{
    DM_PARAM_FITTED = 0,     // determined from qualifying samples
    DM_PARAM_FROM_GRAYS = 1, // gamma only: derived from the gray-ramp pool
    DM_PARAM_ASSUMED = 2     // default used (gamma 2.2, black zero)
};

struct DisplayModelReport
{
    int    samplesUsed;   // samples with weight > 0 that entered the fit
    double rmsXYZ;        // weighted RMS of residual vector magnitude, XYZ units
    double maxXYZ;        // largest residual magnitude among used samples
    DisplayModelParamSource gammaSource[3];
    DisplayModelParamSource blackSource;
};

class DisplayModel
{
public:
    DisplayModel();

    // Weighted fit. Returns false when the sample set violates the sample
    // contract above or cannot constrain the matrix: fewer than 8 usable
    // samples, or a singular linearized-RGB moment matrix. On failure the
    // model keeps its previous state (the last successful fit, or the
    // never-fitted invalid state). Gammas the set cannot constrain do not
    // fail the fit; they fall back (single-channel ramps, then grays, then
    // the 2.2 default) and the report's gammaSource says which happened.
    bool Fit(const std::vector<DisplayModelSample>& samples);

    bool IsValid() const { return m_valid; }

    // Forward transform: normalized signal -> predicted XYZ.
    // On a model that is not valid, writes all zeros.
    void SignalToXYZ(const double rgb[3], double XYZ[3]) const;

    // Inverse transform: XYZ -> normalized signal. Returns false when the
    // target lies outside the model gamut (result is clamped to [0..1]),
    // and always returns false (with zeros) on a model that is not valid.
    bool XYZToSignal(const double XYZ[3], double rgb[3]) const;

    // Meaningful only while IsValid(); reflects the last successful fit.
    const DisplayModelReport& Report() const { return m_report; }

    // Fitted parameters, exposed for inspection and serialization.
    // Meaningful only while IsValid(); reflect the last successful fit.
    const double* MatrixRow(int row) const { return m_M[row]; }     // row 0..2
    const double* BlackXYZ() const         { return m_black; }
    double Gamma(int channel) const        { return m_gamma[channel]; }

private:
    double m_M[3][3];      // linearized RGB -> (XYZ - black)
    double m_Minv[3][3];
    double m_black[3];     // XYZ measured at black (flare), additive
    double m_gamma[3];
    bool   m_valid;
    DisplayModelReport m_report;
};

#endif // DISPLAYMODEL_H
