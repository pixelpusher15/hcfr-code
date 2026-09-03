/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2011 Association Homecinema Francophone.  All rights reserved.
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
//	François-Xavier CHABOUD
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// Measure.h: interface for the CMeasure class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MEASURE_H__4A61BBE7_7779_4FCD_90B5_E9F22517DFBD__INCLUDED_)
#define AFX_MEASURE_H__4A61BBE7_7779_4FCD_90B5_E9F22517DFBD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Color.h"
#include "Sensors\Sensor.h"
#include "Generators\Generator.h"
#include <vector>
BOOL IsMeasureSweepActive();

class CAsyncMeasurer;
// Tone-mapped diffuse-white luminance (nits) for the active HDR-10 target:
// getL_EOTF of the grid-snapped 50.22831% signal, *100. Centralizes the ~400-char
// expression that was copy-pasted across the measure/view/export tmWhite sites.
// Pass noDataColor for White/Black at the sites that do not have a measured
// White/Black (mode-5 PQ ignores them anyway).
double TmDiffuseWhiteNits(const CColor & White, const CColor & Black);

// The complete saturation / color-checker dE normalisation, so that every
// consumer asks CMeasure for it instead of re-deriving it. See
// CMeasure::GetColorDENorm.
//
// Two equivalent ways to spend it, giving the SAME dE:
//   measures grid  measured.GetDeltaE( whiteY, ref * markScale, refWhite, ... )
//   3D viewer      measured.GetDeltaE( whiteY, ref * deScale,   1.0,      ... )
// because deScale == markScale / refWhite by construction. Use markScale for
// anything that also has to PLOT the reference (marker geometry, swatches) so
// the reference lands in the same units as the measurement; use deScale when
// only a dE is wanted.
//
// CAVEAT: that equivalence holds for dE_form 0..5, whose Lab/Luv constructors
// DIVIDE by the white they are handed, so a common factor cancels. dE_form 6
// (dICtCp) does not qualify - ColorXYZ::GetDeltaE case 6 SWAPS the two whites
// and ColorICtCp MULTIPLIES by its white through the non-linear PQ curve, so
// the two forms give different numbers there. Anything that must match the
// measures grid under dICtCp has to spend the grid's (markScale, refWhite)
// form, not deScale.
//
// All four members are 1.0 / measured-white in SDR: nothing rescales there.
//
// SCOPE: the unified (automatic-generator) convention only. The measures grid
// keeps a legacy manual-generator (DVD) carve-out that is deliberately NOT
// modelled here - see CMeasure::GetColorDENorm - so a site that must byte-match
// the grid's on-screen numbers for a disc capture needs its own DVD branch.
struct ColorDENorm
{
	double	whiteY;		// measured white the MEASUREMENT is normalised by
	double	refWhite;	// YWhiteRef to pass alongside a markScale-scaled reference
	double	markScale;	// reference rescale into diffuse-white-relative units
	double	deScale;	// reference rescale for the YWhiteRef = 1.0 form

	// Defaulted so a partially-filled instance degrades to "no rescale" and
	// trips the <= 0.0 white guards consumers already have, rather than feeding
	// indeterminate doubles into GetDeltaE as a divisor.
	ColorDENorm() : whiteY(0.0), refWhite(1.0), markScale(1.0), deScale(1.0) {}
};

#define	DUPLGRAYLEVEL		0
#define	DUPLNEARBLACK		1
#define	DUPLNEARWHITE		2
#define	DUPLPRIMARIESSAT	3
#define	DUPLSECONDARIESSAT	4
#define	DUPLPRIMARIESCOL	5
#define	DUPLSECONDARIESCOL	6
#define	DUPLCONTRAST		7
#define	DUPLINFO			8
#define	DUPLPROFILE			9


#define LUX_NOMEASURE	0
#define LUX_OK			1
#define LUX_CANCELED	2


// ---- Grayscale level presets (shared by CMeasure and the Measure-parameters dialog) ----
// Each preset is an explicit array of nominal IRE percentages (0..100); one is non-uniform.
struct GrayScalePreset
{
	LPCSTR			name;	// dropdown label
	int				count;	// number of IRE points
	const double *	levels;	// 'count' entries, 0..100
};

const GrayScalePreset *	GetGrayScalePresets ();		// fixed table (excludes the "Custom..." entry)
int						GetGrayScalePresetCount ();

// Configured stimulus-capture levels as integer percentages (1..100), parsed from
// the "SatStimLevels" registry list. Shared by the grid dropdown and the
// measure-all-levels commands.
void					GetSatStimLevelPercents ( std::vector<int> & pcts );

#define GRAYSCALE_DEFAULT_PRESET	2	// 11-point (10%) is the default selection


// ---- Multi-level saturation store ----
// One entry per measured stimulus level: the signal-domain amplitude (0..1]
// the sweep patterns were scaled to, plus its six hue sweeps. The active
// level's set is mirrored in the six m_*SatMeasureArray members so every
// existing consumer (grid, charts, dE math) reads it unchanged.
struct CSatLevelSet
{
	double stimLevel;
	std::vector<CColor> sat[6];	// R,G,B,Y,C,M — GetSaturationSize() entries each
};


class CMeasure : public CObject
{
public:
	DECLARE_SERIAL(CMeasure) ;

protected:
	CReferencesPropPage m_referencesPropertiesPage;
	BOOL m_isModified;
	CArray<CColor,CColor> m_primariesArray;
	CArray<CColor,CColor> m_secondariesArray;
	CArray<CColor,CColor> m_grayMeasureArray;
	CArray<double,double> m_grayIRELevelArray;	// explicit nominal IRE % per gray index (empty => legacy even distribution)
	CArray<CColor,CColor> m_measurementsArray;
	CColor m_OnOffBlack;
	CColor m_OnOffWhite;
	CColor m_PrimeWhite;
	CColor m_AnsiBlack;
	CColor m_AnsiWhite;
	CArray<CColor,CColor> m_nearBlackMeasureArray;
	CArray<CColor,CColor> m_nearWhiteMeasureArray;
	CArray<CColor,CColor> m_redSatMeasureArray;
	CArray<CColor,CColor> m_greenSatMeasureArray;
	CArray<CColor,CColor> m_blueSatMeasureArray;
	CArray<CColor,CColor> m_yellowSatMeasureArray;
	CArray<CColor,CColor> m_cyanSatMeasureArray;
	CArray<CColor,CColor> m_magentaSatMeasureArray;
	CArray<CColor,CColor> m_cc24SatMeasureArray;
	CArray<CColor,CColor> m_cc24SatMeasureArray_master;
	std::vector<CSatLevelSet> m_satLevelStore;	// all measured stimulus levels (active mirrored in m_*SatMeasureArray)
	double m_activeSatLevel;	// signal-domain amplitude the bound sweeps were measured at
	// Display profile capture (serialized as version 20, block written only when data exists)
	CArray<CColor,CColor> m_profileMeasureArray;	// dense RGB-cube characterization, generation order
	std::vector<CColor> m_profileDriftAnchors;		// white drift anchors, capture order
	std::vector<int> m_profileDriftAnchorIdx;		// patch index each anchor was measured before
	int m_profileCubeSize;							// N of the N^3 grid (0 = no profile captured)
	BOOL m_profileGrayExtras;						// extra gray-axis / near-black samples appended
	BOOL m_profileDriftComp;						// drift compensation was applied during capture
	double m_profileCaptureSeconds;					// wall-clock duration of the capture
	std::vector<ColorRGBDisplay> m_profileGenCache;	// GetProfilePatchRGB cache (not serialized)
	int m_profileGenCacheKey;						// cubeSize*2+grayExtras the cache was built for
	CString m_infoStr;
	CString m_CCStr;
public:
	BOOL	m_bIREScaleMode;
	BOOL	m_binMeasure;
	volatile BOOL	m_bAbortSweep;
	BOOL	bDisplayRT;
	UINT	m_bpreV10;
	UINT	m_NearWhiteClipCol;
	UINT	m_NMeasurements;

	// Internal data used by background measures threads (not serialized)
public:
	HANDLE					m_hThread;
	HANDLE					m_hEventRun;
	HANDLE					m_hEventDone;
	BOOL					m_bTerminateThread;
	BOOL					m_bErrorOccurred;
	int						m_nBkMeasureStepCount;
	int						m_nBkMeasureStep;
	ColorRGBDisplay			m_clrToMeasure;
	CSensor *				m_pBkMeasureSensor;
	CArray<CColor,int> *	m_pBkMeasuredColor;
	int						m_nbMaxMeasurements;

public:
	CMeasure();
	virtual ~CMeasure();

	virtual void Serialize(CArchive& archive); 

	void StartLuxMeasure ();
	UINT GetLuxMeasure ( double * pValue ); 

	void Copy(CMeasure * p,UINT nId);
	BOOL MeasureGrayScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc);
	BOOL MeasureCC24(CSensor *pSensor, CGenerator *pGenerator);
	BOOL MeasureGrayScaleAndColors(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc);
	CColor GetGray(int i) const;
	void SetGray(int i,const CColor & aColor) {m_grayMeasureArray[i]=aColor; m_isModified=TRUE;} 
	int GetGrayScaleSize() const { return m_grayMeasureArray.GetSize(); }
	void SetGrayScaleSize(int steps);
	void SetGrayScaleLevels(const double * pLevels, int count);	// install explicit (possibly non-uniform) IRE levels
	// is16_235 is REQUIRED - see the note on ArrayIndexToGrayLevel in Color.h.
	double GetGrayPercent(int index, bool bUseRoundDown, bool b10bit, bool is16_235) const;	// IRE % for a gray index
	int GetGrayScalePreset() const;	// matching preset index, or -1 (custom)
	void SetIREScaleMode(BOOL bIRE);
	CColor lastColor, previousColor;
	CColor m_userBlack;
	BOOL m_bOverRideBlack;

	BOOL MeasureNearBlackScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc);
	CColor GetNearBlack(int i) const;
	void SetNearBlack(int i,const CColor & aColor) {m_nearBlackMeasureArray[i]=aColor; m_isModified=TRUE;} 
	int GetNearBlackScaleSize() const { return m_nearBlackMeasureArray.GetSize(); }
	void SetNearBlackScaleSize(int steps);

	BOOL MeasureNearWhiteScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc);
	CColor GetNearWhite(int i) const;
	void SetNearWhite(int i,const CColor & aColor) {m_nearWhiteMeasureArray[i]=aColor; m_isModified=TRUE;} 
	int GetNearWhiteScaleSize() const { return m_nearWhiteMeasureArray.GetSize(); }
	void SetNearWhiteScaleSize(int steps);

	BOOL MeasureRedSatScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc);
	BOOL MeasureGreenSatScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc);
	BOOL MeasureBlueSatScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc);
	BOOL MeasureYellowSatScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc);
	BOOL MeasureCyanSatScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc);
	BOOL MeasureMagentaSatScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc);
	BOOL MeasureCC24SatScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc);
	BOOL MeasureDisplayProfile(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc, int cubeN, BOOL bGrayExtras, BOOL bDriftComp);
	BOOL MeasureAllSaturationScales(CSensor *pSensor, CGenerator *pGenerator,BOOL bPrimaryOnly, CDataSetDoc *pDoc);
	BOOL MeasurePrimarySecondarySaturationScales(CSensor *pSensor, CGenerator *pGenerator,BOOL bPrimaryOnly, CDataSetDoc *pDoc);
	int GetSaturationSize() const { return m_redSatMeasureArray.GetSize(); }
	int GetCC24MasterSaturationSize() const { return m_cc24SatMeasureArray_master.GetSize(); }
	void SetSaturationSize(int steps);
	CColor GetRedSat(int i) const;
	void SetRedSat(int i,const CColor & aColor) {m_redSatMeasureArray[i]=aColor; m_isModified=TRUE; } 
	CColor GetGreenSat(int i) const;
	void SetGreenSat(int i,const CColor & aColor) {m_greenSatMeasureArray[i]=aColor; m_isModified=TRUE; } 
	CColor GetBlueSat(int i) const;
	void SetBlueSat(int i,const CColor & aColor) {m_blueSatMeasureArray[i]=aColor; m_isModified=TRUE; } 
	CColor GetYellowSat(int i) const;
	void SetYellowSat(int i,const CColor & aColor) {m_yellowSatMeasureArray[i]=aColor; m_isModified=TRUE; } 
	CColor GetCyanSat(int i) const;
	void SetCyanSat(int i,const CColor & aColor) {m_cyanSatMeasureArray[i]=aColor; m_isModified=TRUE; } 
	CColor GetMagentaSat(int i) const;
	void SetMagentaSat(int i,const CColor & aColor) {m_magentaSatMeasureArray[i]=aColor; m_isModified=TRUE; } 
	CColor GetCC24Sat(int i);
	void SetCC24Sat(int i,const CColor & aColor) {m_cc24SatMeasureArray[i]=aColor; m_isModified=TRUE; } 
	CColor GetCC24MasterSat(int i) const;
	void SetCC24MasterSat(int i,const CColor & aColor) {m_cc24SatMeasureArray_master[i]=aColor; m_isModified=TRUE; }
	CString GetCCStr() const;

	// Display profile capture
	CColor GetProfileMeasure(int i) const;
	void SetProfileMeasure(int i,const CColor & aColor) {m_profileMeasureArray[i]=aColor; m_isModified=TRUE; }
	int GetProfileMeasureSize() const { return m_profileMeasureArray.GetSize(); }
	BOOL HasProfileMeasures() const { return m_profileCubeSize > 0 && m_profileMeasureArray.GetSize() > 0; }
	int GetProfileCubeSize() const { return m_profileCubeSize; }
	BOOL GetProfileGrayExtras() const { return m_profileGrayExtras; }
	BOOL GetProfileDriftComp() const { return m_profileDriftComp; }
	double GetProfileCaptureSeconds() const { return m_profileCaptureSeconds; }
	int GetProfileDriftAnchorCount() const { return (int)m_profileDriftAnchors.size(); }
	CColor GetProfileDriftAnchor(int i) const { return m_profileDriftAnchors[i]; }
	int GetProfileDriftAnchorIndex(int i) const { return m_profileDriftAnchorIdx[i]; }
	void ClearProfileMeasures();
	ColorRGBDisplay GetProfilePatchRGB(int i);		// patch stimulus, regenerated from metadata (cached)
	void GetRefProfileSat(int i, CColor & ccRef);	// theoretical reference for patch i (grid conventions)
	// Breakdown of a profile patch's error, filled on request beside the dE itself:
	// the unsigned lightness/chroma/hue magnitudes the active m_dE_form weights.
	struct ProfileDEParts
	{
		double dL, dC, dH;
		ProfileDEParts() : dL(0.0), dC(0.0), dH(0.0) {}
	};
	// dE for profile patch i, matching the measures grid (SDR + PQ HDR); -1 to skip.
	// The breakdown lives here rather than in the pane so the white selection and
	// the PQ absolute-nits bridge keep ONE definition; pass NULL to skip its cost.
	double ComputeProfileDEEx(const CColor & measured, int i, ProfileDEParts * pParts);
	double ComputeProfileDE(const CColor & measured, int i) { return ComputeProfileDEEx( measured, i, NULL ); }
	double GetColorDEWhiteY(bool bSpecial, bool bCC, bool bMasciorCC) const;	// the white the grid normalises sat/CC dE by (measured, NOT the theoretical tmWhite)
	ColorDENorm GetColorDENorm(int displayMode) const;	// the whole sat/CC dE normalisation, shared by every consumer
	// Live profile-capture state (not serialized): the profiling pane pauses/observes through these
	volatile BOOL m_bProfilePause;
	double m_profileCurrentDrift;	// last anchor's drift factor minus 1.0

protected:
	bool MeasureProfileDriftAnchor(CAsyncMeasurer & am, CSensor * pSensor, CGenerator * pGenerator, CDataSetDoc * pDoc, int patchIdx, double & firstAnchorY, double & prevFactor, int & prevIdx, BOOL bIgnoreAbort = FALSE);
	void ApplyProfileDriftSegment(int fromIdx, int toIdx, double fFrom, double fTo);
public:

	// Multi-level saturation store (see CSatLevelSet above)
	double GetActiveSatLevel() const { return m_activeSatLevel; }
	void StoreActiveSatLevel();	// mirror the bound m_*SatMeasureArray sweeps into the store
	BOOL BindSatLevel(double level);	// store the active sweeps, then load (or create empty) 'level'
	int GetSatLevelCount();	// syncs the active entry first
	double GetSatLevelAt(int idx);
	const CSatLevelSet & GetSatLevelSet(int idx);

	BOOL MeasurePrimaries(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc);
	CColor GetPrimary(int i) const;
	CColor GetRedPrimary() const;
	CColor GetGreenPrimary() const;
	CColor GetBluePrimary() const;
	void SetRedPrimary(const CColor & aColor) { m_primariesArray[0]=aColor; m_isModified=TRUE; }
	void SetGreenPrimary(const CColor & aColor) { m_primariesArray[1]=aColor; m_isModified=TRUE; }
	void SetBluePrimary(const CColor & aColor) { m_primariesArray[2]=aColor; m_isModified=TRUE; }
	void SetPrimary(int i, const CColor & aColor) { m_primariesArray[i] = aColor; m_isModified=TRUE; }
 
	BOOL MeasureSecondaries(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc);
	CColor GetSecondary(int i) const;
	CColor GetYellowSecondary() const;
	CColor GetCyanSecondary() const;
	CColor GetMagentaSecondary() const;
	void SetYellowSecondary(const CColor & aColor) { m_secondariesArray[0]=aColor; m_isModified=TRUE; }
	void SetCyanSecondary(const CColor & aColor) { m_secondariesArray[1]=aColor; m_isModified=TRUE; }
	void SetMagentaSecondary(const CColor & aColor) { m_secondariesArray[2]=aColor; m_isModified=TRUE; }
	void SetSecondary(int i, const CColor & aColor) { m_secondariesArray[i] = aColor; m_isModified=TRUE; }

	CColor GetAnsiBlack() const;
	CColor GetAnsiWhite() const;
	void SetAnsiBlack(const CColor & aColor) { m_AnsiBlack=aColor; m_isModified=TRUE; }
	void SetAnsiWhite(const CColor & aColor) { m_AnsiWhite=aColor; m_isModified=TRUE; }

	CColor GetOnOffBlack() const;
	CColor GetOnOffWhite() const;
	CColor GetPrimeWhite() const; //white reference for pseudo-color spaces
	double GetHDRRefScale() const; //HDR-10 refs: 1=10000nits -> diffuse-white-relative
	void SetOnOffBlack(const CColor & aColor) { m_OnOffBlack=aColor; m_isModified=TRUE; }
	void SetOnOffWhite(const CColor & aColor) { m_OnOffWhite=aColor; m_isModified=TRUE; }
	void SetPrimeWhite(const CColor & aColor) { m_PrimeWhite=aColor; m_isModified=TRUE; }

	BOOL MeasureContrast(CSensor *pSensor, CGenerator *pGenerator);
	double GetOnOffContrast ();
	double GetAnsiContrast ();
	double GetContrastMinLum ();
	double GetContrastMaxLum ();
	void DeleteContrast ();
	
	BOOL AddMeasurement(CSensor *pSensor, CGenerator *pGenerator, CGenerator::MeasureType MT, int isPrimary, int last_minCol, int m_d);
	CColor GetMeasurement(int i) const;
	void SetMeasurements(int i,const CColor & aColor) {m_measurementsArray[i]=aColor; m_isModified=TRUE; } 
	void AppendMeasurements(const CColor & aColor, int isPrimary, int last_minCol);
	int GetMeasurementsSize() const { return m_measurementsArray.GetSize(); }
	void SetMeasurementsSize(int size) { m_measurementsArray.SetSize(size); m_isModified=TRUE; }
	void DeleteMeasurements(int i,int count) { m_measurementsArray.RemoveAt(i,count); m_isModified=TRUE; }
	void InsertMeasurement(int i, CColor & aColor) { m_measurementsArray.InsertAt(i,aColor); m_isModified=TRUE; }
	void FreeMeasurementAppended(int isPrimary, int last_minCol);

	CString GetInfoString() const { return m_infoStr+m_CCStr; }
	void SetInfoString(CString & aStr) { m_infoStr = aStr; } 

	CColor GetRefPrimary(int i) const;
	CColor GetRefSecondary(int i) const;
	CColor GetRefSat(int i, double sat_percent, bool special, double stimLevel = -1.0) const;	// stimLevel < 0 => active level
	void GetRefCC24Sat(int i, CColor &color) const;

	BOOL IsModified() { return m_isModified; }
	void AbortMeasure() { m_bAbortSweep = TRUE; }

    void ApplySensorAdjustmentMatrix(const Matrix & matrixAdjustment);

	BOOL WaitForDynamicIris ( BOOL bIgnoreEscape = FALSE, CDataSetDoc *pDoc = NULL );
	BOOL CheckBlackOverride (  );
	void UpdateViews ( CDataSetDoc *pDoc = NULL, int Sequence = 0 );
	void UpdateTstWnd ( CDataSetDoc *pDoc, int Sequence );
	int m_currentIndex, m_currentSequence, displaymode;

	HANDLE InitBackgroundMeasures ( CSensor *pSensor, int nSteps );
	BOOL BackgroundMeasureColor ( int nCurStep, const ColorRGBDisplay& aRGBValue );
	void CancelBackgroundMeasures ();
	BOOL ValidateBackgroundGrayScale ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundNearBlack ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundNearWhite ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundPrimaries ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundSecondaries ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundRedSatScale ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundGreenSatScale ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundBlueSatScale ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundYellowSatScale ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundCyanSatScale ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundMagentaSatScale ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundCC24SatScale ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundGrayScaleAndColors ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundSingleMeasurement ( BOOL bUseLuxValues, double * pLuxValues );

};

#endif // !defined(AFX_MEASURE_H__4A61BBE7_7779_4FCD_90B5_E9F22517DFBD__INCLUDED_)
