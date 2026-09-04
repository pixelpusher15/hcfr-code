///////////////////////////////////////////////////////////////////////////////
// GamutName.h: the short label for a reference gamut.
//
// A property of the CColorReference rather than of any one chart, but it writes
// wide strings through the secure CRT, so it lives on the Windows side instead
// of in libHCFR. Shared by the CIE chart's coverage chips and the 3D viewer's
// gamut-volume chips, so the two always name the same gamut the same way -- and
// so neither has to include the other's view header to ask.
///////////////////////////////////////////////////////////////////////////////

#ifndef GAMUTNAME_H_INCLUDED
#define GAMUTNAME_H_INCLUDED

#include "Color.h"
#include <wchar.h>

// Short names for the mainstream gamuts; everything else (incl. the
// reduced-primary HDTVa/HDTVb pseudo-gamuts) uses its own reference name so the
// label always matches the gamut actually drawn.
inline void GamutShortName(const CColorReference & ref, wchar_t * out, size_t cch)
{
	switch (ref.m_standard)
	{
		case HDTV: case UHDTV4:  wcscpy_s(out, cch, L"Rec.709");  break;
		case UHDTV: case UHDTV3: wcscpy_s(out, cch, L"DCI-P3");   break;
		case UHDTV2:             wcscpy_s(out, cch, L"Rec.2020"); break;
		default:
			swprintf_s(out, cch, L"%S", ref.GetName().c_str());
			break;
	}
}

#endif // GAMUTNAME_H_INCLUDED
