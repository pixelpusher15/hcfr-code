/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2012 Association Homecinema Francophone.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////
//
//  This file is subject to the terms of the GNU General Public License as
//  published by the Free Software Foundation.  A copy of this license is
//  included with this software distribution in the file COPYING.htm. If you
//  do not have a copy, you may obtain a copy by writing to the Free
//  Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details
/////////////////////////////////////////////////////////////////////////////
//  Author(s):
//  PhoenixF15E
/////////////////////////////////////////////////////////////////////////////

// GrayRamp.cpp: the gray readings a gray graph draws. See GrayRamp.h.

#include "GrayRamp.h"
#include <algorithm>
#include <cmath>
#include <map>

void BuildSweepGrayRamp ( const GraySeriesView & v, GrayRamp * out )
{
	out->source = GRAY_RAMP_SWEEP;
	out->points.clear();
	out->blackIndex = -1;
	out->whiteIndex = -1;
	if ( v.readings == NULL || v.size <= 0 )
		return;
	out->points.resize ( v.size );
	for ( int i = 0; i < v.size; i++ )
	{
		GrayRampPoint & p = out->points[i];
		double drive;
		p.reading = v.readings[i];
		p.level = GraySeriesLevel ( v, i );
		p.prop = GraySeriesProp ( v, i );
		p.nominalProp = GraySeriesSlotProp ( v, i );
		p.stamped = GraySeriesDriveProp ( v, i, & drive );
		p.isBlack = ( i == 0 );
		p.isWhite = ( i == v.size - 1 );
		p.members = 1;
		p.firstRow = i;
	}
	out->blackIndex = 0;
	out->whiteIndex = v.size - 1;
}

bool GrayRampRowCounts ( const CsvPatchRow & row )
{
	if ( row.label.kind != PATCH_KIND_GRAY )
		return false;
	return row.role == PROVENANCE_ROLE_UNKNOWN || row.role == PROVENANCE_ROLE_COLOR
		|| row.role == PROVENANCE_ROLE_VERIFICATION;
}

namespace
{
	// Strict weak order on a reading's value, so a sum does not depend on the
	// order the readings arrive in.
	bool XYZLess ( const CColor * a, const CColor * b )
	{
		const ColorXYZ x = a->GetXYZValue(), y = b->GetXYZValue();
		for ( int k = 0; k < 3; k++ )
			if ( x[k] != y[k] )
				return x[k] < y[k];
		return false;
	}

	bool DoubleLess ( double a, double b ) { return a < b; }

	double SortedMean ( std::vector<double> & v )
	{
		std::sort ( v.begin(), v.end(), DoubleLess );
		double s = 0.0;
		for ( size_t i = 0; i < v.size(); i++ )
			s += v[i];
		return s / (double) v.size();
	}
}

CColor AverageGrayReadings ( const CColor * const * members, int n, const CColor * first )
{
	if ( n <= 0 )
		return noDataColor;
	if ( n == 1 )
		return *members[0];

	std::vector<const CColor *> m ( members, members + n );
	std::sort ( m.begin(), m.end(), XYZLess );

	std::vector<double> xyz[3], raw[3], lux;
	bool allLux = true, allRaw = true, allWire = true;
	const CColorWireSetup wire0 = m[0]->GetWireSetup();
	for ( int i = 0; i < n; i++ )
	{
		const ColorXYZ c = m[i]->GetXYZValue();
		for ( int k = 0; k < 3; k++ )
			xyz[k].push_back ( c[k] );
		if ( m[i]->HasLuxValue() )
			lux.push_back ( m[i]->GetLuxValue() );
		else
			allLux = false;
		if ( m[i]->HasRawXYZValue() )
		{
			const ColorXYZ r = m[i]->GetRawXYZValue();
			for ( int k = 0; k < 3; k++ )
				raw[k].push_back ( r[k] );
		}
		else
			allRaw = false;
		if ( ! m[i]->HasWireSetup() || m[i]->GetWireSetup() != wire0 )
			allWire = false;
	}

	CColor avg ( ColorXYZ ( SortedMean ( xyz[0] ), SortedMean ( xyz[1] ), SortedMean ( xyz[2] ) ) );
	if ( allLux )
		avg.SetLuxValue ( SortedMean ( lux ) );
	if ( allRaw )
		avg.SetRawXYZValue ( ColorXYZ ( SortedMean ( raw[0] ), SortedMean ( raw[1] ), SortedMean ( raw[2] ) ) );
	if ( allWire )
		avg.SetWireSetup ( wire0 );
	const CColor * f = first ? first : members[0];
	if ( f->HasStimulusValue() )
		avg.SetStimulusValue ( f->GetStimulusValue() );
	if ( f->HasProvenance() )
		avg.SetProvenance ( f->GetProvenance() );
	return avg;
}

namespace
{
	struct ListGroup
	{
		std::vector<int> rows;	// in row order
	};

	bool PointLess ( const GrayRampPoint & a, const GrayRampPoint & b )
	{
		if ( a.prop != b.prop )
			return a.prop < b.prop;
		if ( a.nominalProp != b.nominalProp )
			return a.nominalProp < b.nominalProp;
		return a.firstRow < b.firstRow;
	}
}

void BuildListGrayRamp ( const CsvPatchRow * rows, int nRows, const CColor * readings, int nReadings, GrayRamp * out )
{
	out->source = GRAY_RAMP_LIST;
	out->points.clear();
	out->blackIndex = -1;
	out->whiteIndex = -1;
	if ( rows == NULL || readings == NULL )
		return;
	const int n = ( nRows < nReadings ) ? nRows : nReadings;

	// One group per signal: the level the row's (clamped) code decodes to,
	// which equal codes give bit for bit.
	std::map<double, ListGroup> groups;
	for ( int i = 0; i < n; i++ )
	{
		if ( ! GrayRampRowCounts ( rows[i] ) || ! readings[i].isValid() )
			continue;
		groups[rows[i].label.level].rows.push_back ( i );
	}

	for ( std::map<double, ListGroup>::const_iterator it = groups.begin(); it != groups.end(); ++it )
	{
		const std::vector<int> & g = it->second.rows;
		const CsvPatchRow & row0 = rows[g[0]];
		GrayRampPoint p;
		std::vector<const CColor *> members;
		for ( size_t k = 0; k < g.size(); k++ )
			members.push_back ( & readings[g[k]] );
		p.reading = AverageGrayReadings ( & members[0], (int) members.size(), & readings[g[0]] );
		p.members = (int) members.size();
		p.firstRow = g[0];
		p.nominalProp = row0.label.level / 100.0;
		p.isBlack = row0.label.isBlack;
		p.isWhite = row0.label.isWhite;

		// The stamped drive when every reading of the point has the same one.
		double drive = 0.0;
		bool stamped = true;
		for ( size_t k = 0; k < members.size() && stamped; k++ )
		{
			double d;
			if ( ! ReadingWireGrayProp ( *members[k], & d ) )
				stamped = false;
			else if ( k == 0 )
				drive = d;
			else if ( d != drive )
				stamped = false;
		}
		p.stamped = stamped;
		p.prop = stamped ? drive : p.nominalProp;
		p.level = p.prop * 100.0;
		out->points.push_back ( p );
	}

	std::stable_sort ( out->points.begin(), out->points.end(), PointLess );
	for ( int i = 0; i < (int) out->points.size(); i++ )
	{
		if ( out->points[i].isBlack && out->blackIndex < 0 )
			out->blackIndex = i;
		if ( out->points[i].isWhite && out->whiteIndex < 0 )
			out->whiteIndex = i;
	}
}

bool GrayRampWhiteUsable ( const CColor & c )
{
	if ( ! c.isValid() )
		return false;
	const double y = c.GetY();
	return std::isfinite ( y ) && y > 0.0;
}

CColor GrayRampWhite ( const GrayRamp & r, const CColor & fallback )
{
	if ( r.whiteIndex >= 0 && r.whiteIndex < (int) r.points.size() && GrayRampWhiteUsable ( r.points[r.whiteIndex].reading ) )
		return r.points[r.whiteIndex].reading;
	return fallback;
}

CColor GrayRampBlack ( const GrayRamp & r, const CColor & fallback )
{
	if ( r.blackIndex >= 0 && r.blackIndex < (int) r.points.size() )
	{
		const CColor & c = r.points[r.blackIndex].reading;
		if ( c.isValid() && std::isfinite ( c.GetY() ) )
			return c;
	}
	return fallback;
}

GrayRampSource ChooseGrayRampSource ( bool sweepHoldsReading, bool listHasPoint )
{
	if ( sweepHoldsReading )
		return GRAY_RAMP_SWEEP;
	return listHasPoint ? GRAY_RAMP_LIST : GRAY_RAMP_SWEEP;
}

bool GrayRampDraws ( const GrayRamp & r, int i )
{
	if ( i < 0 || i >= (int) r.points.size() )
		return false;
	if ( r.source == GRAY_RAMP_SWEEP )
		return true;
	return r.points[i].prop >= 0.0 && r.points[i].prop <= 1.0;
}
