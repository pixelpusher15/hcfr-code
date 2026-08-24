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

// CubeLUT: a 3D lookup-table lattice with reader/writer for the .cube text
// format (Adobe Cube LUT Specification 1.0), the interchange format read by
// madVR, Resolve, and most calibration pipelines. Independent implementation
// from the published format description; no code from any other project.
//
// Lattice model: size N (2..256) cells per axis, N^3 entries, each entry an
// RGB triple of doubles (any finite value; .cube values may be negative or
// exceed 1). Entry (r,g,b) with indices 0..N-1 represents the output for
// input (domainMin + r/(N-1)*(domainMax-domainMin), ...) per component.
//
// File format contract:
// - Writer emits LF-terminated lines: optional TITLE "..." (only when the
//   title is nonempty), LUT_3D_SIZE N, DOMAIN_MIN a b c, DOMAIN_MAX a b c
//   (both always written), then N^3 data lines "R G B". The RED index
//   varies fastest, then green, then blue (the .cube standard ordering).
//   Numbers print with %.17g so a read-back reproduces every double
//   exactly, and always with '.' as the decimal separator regardless of
//   the global C locale.
// - Reader accepts LF or CRLF, blank lines, and '#' comments anywhere;
//   keyword lines TITLE "quoted string" (no embedded quote), DOMAIN_MIN,
//   DOMAIN_MAX, LUT_3D_SIZE in any order but all before the first data
//   line; then exactly N^3 data lines of 3 finite numbers ('.' decimal
//   separator only, standard C float syntax). Anything else is an error:
//   unknown or duplicate keywords, LUT_1D_SIZE (1D LUTs are not supported
//   here), a missing or out-of-range size, non-numeric or non-finite data,
//   too few or too many data lines, an unquoted title, or a domain with
//   min >= max in any component.
// - Any failed Read/Create/Set leaves the object exactly as it was (the
//   last good state, or the never-loaded invalid state), and a failed
//   Read/Write records a one-line reason in LastError().

#ifndef CUBELUT_H
#define CUBELUT_H

#include <string>
#include <vector>

class CubeLUT
{
public:
    CubeLUT();

    // Allocate an N^3 identity lattice (2 <= size <= 256): entry (r,g,b) =
    // domain min + index/(N-1) * (domain span), using the DEFAULT domain
    // 0..1, which Create also resets along with the title. Returns false
    // (state preserved) when size is out of range.
    bool Create(int size);

    bool IsValid() const { return m_size != 0; }
    int  Size() const    { return m_size; }     // 0 when not valid

    // Lattice entries; indices 0..size-1 per axis. Both return false (and
    // change/write nothing) on an invalid object or out-of-range index;
    // SetEntry also rejects non-finite values.
    bool SetEntry(int r, int g, int b, const double rgb[3]);
    bool GetEntry(int r, int g, int b, double rgb[3]) const;

    // Title: written as TITLE "..." when nonempty. Rejects (returns false,
    // keeps the old title) a title containing '"', CR, or LF.
    bool SetTitle(const std::string& title);
    const std::string& Title() const { return m_title; }

    // Domain: every component finite and max > min per component, else
    // false with the old domain kept. Valid whether or not a lattice is
    // loaded (Create resets it to 0..1).
    bool SetDomain(const double dmin[3], const double dmax[3]);
    void GetDomain(double dmin[3], double dmax[3]) const;

    // Apply the LUT: map an input color through the lattice by tetrahedral
    // interpolation - the classic 6-tetrahedron decomposition of each
    // lattice cell along its main diagonal, where the cube is split by the
    // ordering of the three cell-local fractions and the output is the
    // barycentric blend along the path c000 -> (largest axis) -> (largest+
    // middle axes) -> c111 with weights (1-max, max-mid, mid-min, min).
    // (Interpolation technique analyzed in Kasson & Plouffe, "An Analysis
    // of Selected Computer Interchange Color Spaces", ACM Trans. Graphics
    // 11(4), 1992; long-standard in color LUT hardware and ICC pipelines.)
    //
    // Input is in DOMAIN coordinates: each component is mapped through
    // (in - domainMin) / (domainMax - domainMin) and clamped to the domain
    // box first, so out-of-domain inputs evaluate at the nearest box face.
    // Exact at every lattice node, exact for a lattice sampled from an
    // affine function, and continuous across cell and tetrahedron
    // boundaries. Returns false (with out zeroed) only on an object that
    // is not valid or a non-finite input component.
    bool Evaluate(const double in[3], double out[3]) const;

    // Serialization per the format contract above. The Write pair returns
    // false on an invalid object (or an unwritable path); the Read pair
    // returns false on any format violation with the previous state kept.
    // LastError() describes the most recent Read/Write failure and is
    // cleared by a successful Read/Write.
    bool WriteToString(std::string& out) const;
    bool ReadFromString(const std::string& text);
    bool WriteFile(const char* path) const;
    bool ReadFile(const char* path);
    const std::string& LastError() const { return m_lastError; }

private:
    // Entry (r,g,b) lives at ((b*N + g)*N + r)*3 - red fastest, matching
    // the file ordering so data lines stream straight through.
    int m_size;                     // 0 = invalid
    std::vector<double> m_data;     // size^3 * 3
    std::string m_title;
    double m_domainMin[3];
    double m_domainMax[3];
    mutable std::string m_lastError;
};

#endif // CUBELUT_H
