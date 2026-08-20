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
//	Fran�ois-Xavier CHABOUD
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// GDIGenerator.cpp: implementation of the CGDIGenerator class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "GDIGenerator.h"
#include "madTPG.h"
#include "../PatternDisplay.h"
#include "../libnum/numsup.h"
#include "../libconv/conv.h"
#include "../libccast/ccmdns.h"
#include "../libccast/ccwin.h"
#include "../libccast/ccast.h"
#include "../MainFrm.h"
#include "../Tools/SerialCom.h"
#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#include <winsock2.h>				// raw-TCP transport for the binary UART protocol
#pragma comment(lib, "ws2_32.lib")	// (stdafx defines VC_EXTRALEAN, so windows.h skips winsock.h - no conflict)

#include <string>
#include <vector>
#include <float.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

// Load a UI string from the active language resource (mirrors gdigeneproppage.cpp).
static CString LS(UINT id) { CString s; if (id) s.LoadString(id); return s; }

// we use multimon stubs to
// allow backwards compatibilty and
// this doesn't get defined
#if (WINVER < 0x0500)
#define EDD_GET_DEVICE_INTERFACE_NAME 0x00000001
#endif

BOOL CALLBACK MonitorEnumProc( HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData )
{
	CGDIGenerator *pClass=(CGDIGenerator *)dwData;

	pClass->m_hMonitor[pClass->m_monitorNb]=hMonitor;
	pClass->m_monitorNb++;
	return ( pClass->m_monitorNb < MAX_MONITOR_NB );
}


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IMPLEMENT_SERIAL(CGDIGenerator,CGenerator,1) ;

CGDIGenerator::CGDIGenerator()
{
	m_bBlankingCanceled = FALSE;
	m_displayWindow.m_rectSizePercent=GetConfig()->GetProfileInt("GDIGenerator","SizePercent",10);
	m_displayWindow.m_offsetx = GetConfig()->GetProfileInt("GDIGenerator","XOffset",0);
	m_displayWindow.m_offsety = GetConfig()->GetProfileInt("GDIGenerator","YOffset",0);
	m_displayWindow.m_bgStimPercent=GetConfig()->GetProfileInt("GDIGenerator","bgStimPercent",0);
	m_displayWindow.m_Intensity=GetConfig()->GetProfileInt("GDIGenerator","Intensity",100);
	m_displayWindow.m_busePic=GetConfig()->GetProfileInt("GDIGenerator","USEPIC",0);
	m_displayWindow.m_bdispTrip=GetConfig()->GetProfileInt("GDIGenerator","DISPLAYTRIPLETS",1);
	m_displayWindow.m_brPi_user=GetConfig()->GetProfileInt("GDIGenerator","DISPLAYRPIUSER",0);
	m_displayWindow.m_bLinear=GetConfig()->GetProfileInt("GDIGenerator","LOADLINEAR",1);
	m_rectSizePercent = m_displayWindow.m_rectSizePercent;
	m_bgStimPercent = m_displayWindow.m_bgStimPercent;
	m_offsetx = m_displayWindow.m_offsetx;
	m_offsety = m_displayWindow.m_offsety;
	m_Intensity = m_displayWindow.m_Intensity;
	m_patternDGenerator=NULL;
	m_HdrInterface=NULL;

	m_GDIGenePropertiesPage.m_pGenerator = this;
	GetMonitorList();
	m_activeMonitorNum = m_monitorNb-1;

	m_nDisplayMode = GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE);
	m_b16_235 = GetConfig()->GetProfileInt("GDIGenerator","RGB_16_235",1);
	m_busePic = GetConfig()->GetProfileInt("GDIGenerator","USEPIC",0);
	m_bLinear = GetConfig()->GetProfileInt("GDIGenerator","LOADLINEAR",1);
	m_bHdr10 = GetConfig()->GetProfileInt("GDIGenerator","EnableHDR10",0);
	m_bdispTrip = GetConfig()->GetProfileInt("GDIGenerator","DISPLAYTRIPLETS",1);
	m_brPi_user = GetConfig()->GetProfileInt("GDIGenerator","DISPLAYRPIUSER",0);
	m_b10bitPGen = GetConfig()->GetProfileInt("GDIGenerator","TenBitPGen",0);
	m_b10bitMadvr = GetConfig()->GetProfileInt("GDIGenerator","TenBitMadvr",0);
	m_dvdoComPort = GetConfig()->GetProfileString("GDIGenerator","DvdoComPort","");
	m_dvdoColorSpace = GetConfig()->GetProfileInt("GDIGenerator","DvdoColorSpace",0);
	m_dvdoRange = GetConfig()->GetProfileInt("GDIGenerator","DvdoRange",0);
	m_dvdoOutputFormat = GetConfig()->GetProfileInt("GDIGenerator","DvdoOutputFormat",0);
	m_dvdoPatternCode = GetConfig()->GetProfileInt("GDIGenerator","DvdoPatternCode",0);
	m_muriComPort = GetConfig()->GetProfileString("GDIGenerator","MuriComPort","");
	m_muriIp = GetConfig()->GetProfileString("GDIGenerator","MuriIp","192.168.1.239");
	m_muriUseNetwork = GetConfig()->GetProfileInt("GDIGenerator","MuriUseNetwork",1);
	m_muriTcpPort = GetConfig()->GetProfileInt("GDIGenerator","MuriTcpPort",23);
	m_muriTimingId = GetConfig()->GetProfileInt("GDIGenerator","MuriTimingId",-1);
	m_muriColorSpaceId = GetConfig()->GetProfileInt("GDIGenerator","MuriColorSpaceId",0);
	m_muriPatternId = GetConfig()->GetProfileInt("GDIGenerator","MuriPatternId",-1);
    m_madVR_3d = GetConfig()->GetProfileInt("GDIGenerator","MADVR3D",1);
    m_madVR_vLUT = GetConfig()->GetProfileInt("GDIGenerator","MADVRvLUT",1);
    m_madVR_HDR = GetConfig()->GetProfileInt("GDIGenerator","MADVRHDR",0);
    m_madVR_OSD = GetConfig()->GetProfileInt("GDIGenerator","MADVROSD",0);
	m_displayWindow.SetDisplayMode(m_nDisplayMode);	
//	m_displayWindow.SetDisplayMode();	
	m_bisInited = FALSE;

	CString str;
	str.LoadString(IDS_GDIGENERATOR_PROPERTIES_TITLE);
	m_propertySheet.SetTitle(str); 
	m_propertySheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;

	AddPropertyPage(&m_GDIGenePropertiesPage);
	m_propertySheet.RemovePage(&m_GeneratorPropertiePage);

	str.LoadString(IDS_GDIGENERATOR_NAME);
	SetName(str);
	m_bConnect = FALSE;
}

CGDIGenerator::CGDIGenerator(int nDisplayMode, BOOL b16_235)
{
	// This constructor is used by pattern generator: no screen blanking
	m_bBlankingCanceled = FALSE;
	m_doScreenBlanking = FALSE;
	m_displayWindow.m_rectSizePercent=100;
	m_displayWindow.m_bgStimPercent=0;
	m_displayWindow.m_offsetx = 0;
	m_displayWindow.m_offsety = 0;
	m_displayWindow.m_Intensity=100;
	m_displayWindow.m_busePic=FALSE;
	m_displayWindow.m_brPi_user = FALSE;
	m_displayWindow.m_bdispTrip=FALSE;
	m_displayWindow.m_bLinear=FALSE;
	m_displayWindow.m_bHdr10=FALSE;
	m_HdrInterface=NULL;
	m_nPat = 0;

	m_GDIGenePropertiesPage.m_pGenerator = this;
	GetMonitorList();
	m_activeMonitorNum = m_monitorNb-1;

	m_nDisplayMode = nDisplayMode;
	m_b16_235 = b16_235;
	m_b10bitPGen = GetConfig()->GetProfileInt("GDIGenerator","TenBitPGen",0);
	m_b10bitMadvr = GetConfig()->GetProfileInt("GDIGenerator","TenBitMadvr",0);
	m_dvdoComPort = GetConfig()->GetProfileString("GDIGenerator","DvdoComPort","");
	m_dvdoColorSpace = GetConfig()->GetProfileInt("GDIGenerator","DvdoColorSpace",0);
	m_dvdoRange = GetConfig()->GetProfileInt("GDIGenerator","DvdoRange",0);
	m_dvdoOutputFormat = GetConfig()->GetProfileInt("GDIGenerator","DvdoOutputFormat",0);
	m_dvdoPatternCode = GetConfig()->GetProfileInt("GDIGenerator","DvdoPatternCode",0);
	m_muriComPort = GetConfig()->GetProfileString("GDIGenerator","MuriComPort","");
	m_muriIp = GetConfig()->GetProfileString("GDIGenerator","MuriIp","192.168.1.239");
	m_muriUseNetwork = GetConfig()->GetProfileInt("GDIGenerator","MuriUseNetwork",1);
	m_muriTcpPort = GetConfig()->GetProfileInt("GDIGenerator","MuriTcpPort",23);
	m_muriTimingId = GetConfig()->GetProfileInt("GDIGenerator","MuriTimingId",-1);
	m_muriColorSpaceId = GetConfig()->GetProfileInt("GDIGenerator","MuriColorSpaceId",0);
	m_muriPatternId = GetConfig()->GetProfileInt("GDIGenerator","MuriPatternId",-1);
	m_displayWindow.SetDisplayMode(nDisplayMode);

	CString str;
	str.LoadString(IDS_GDIGENERATOR_PROPERTIES_TITLE);
	m_propertySheet.SetTitle(str); 
	m_propertySheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;

	AddPropertyPage(&m_GDIGenePropertiesPage);
	m_propertySheet.RemovePage(&m_GeneratorPropertiePage);

	str.LoadString(IDS_GDIGENERATOR_NAME);
	SetName(str);
	m_bConnect = FALSE;
	Init();
}

CGDIGenerator::~CGDIGenerator()
{
	if (m_HdrInterface)
	{
		OutputDebugString("Destroy existing HdrInterface");
		delete m_HdrInterface;
	}
} 

void CGDIGenerator::Copy(CGenerator * p)
{
	CGenerator::Copy(p);

	m_activeMonitorNum = ((CGDIGenerator*)p)->m_activeMonitorNum;
	m_nDisplayMode = ((CGDIGenerator*)p)->m_nDisplayMode;

}

void CGDIGenerator::GetMonitorList()
{
	m_monitorNb=0;
	// Get monitors handles and nb 
	EnumDisplayMonitors(NULL,NULL,MonitorEnumProc,(LPARAM)this);
	
	MONITORINFOEX	mi;
	mi.cbSize = sizeof ( mi );

	// Fill monitor string array used for combo box in properties sheet
	m_GDIGenePropertiesPage.m_monitorNameArray.RemoveAll();
	for(UINT i=0; i<m_monitorNb; i++)
	{
		GetMonitorInfo ( m_hMonitor[i], & mi );
		std::string sMonitor = GetMonitorName(&mi);
		m_GDIGenePropertiesPage.m_monitorNameArray.Add(sMonitor.c_str());		
		m_GDIGenePropertiesPage.m_monitorHandle [ i ] = m_hMonitor [ i ];
	}
}

std::string CGDIGenerator::GetMonitorName(const MONITORINFOEX *m) const
{
	std::string sMonitor;
	if (!m)
		return sMonitor;

	// Prefer the OS friendly monitor name (e.g. "DELL U2720Q") from the DisplayConfig
	// API. The legacy EnumDisplayDevices path below only yields the generic driver
	// name ("Generic PnP Monitor") and is kept as a fallback.
	UINT32 numPaths = 0, numModes = 0;
	if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &numPaths, &numModes) == ERROR_SUCCESS)
	{
		std::vector<DISPLAYCONFIG_PATH_INFO> paths(numPaths);
		std::vector<DISPLAYCONFIG_MODE_INFO> modes(numModes);
		if (numPaths && QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &numPaths, paths.data(), &numModes, modes.data(), NULL) == ERROR_SUCCESS)
		{
			for (UINT32 i = 0; i < numPaths; i++)
			{
				// Match this path's GDI source name against the monitor's device name.
				DISPLAYCONFIG_SOURCE_DEVICE_NAME src;
				SecureZeroMemory(&src, sizeof(src));
				src.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
				src.header.size = sizeof(src);
				src.header.adapterId = paths[i].sourceInfo.adapterId;
				src.header.id = paths[i].sourceInfo.id;
				if (DisplayConfigGetDeviceInfo(&src.header) != ERROR_SUCCESS)
					continue;

				char gdiName[CCHDEVICENAME] = { 0 };
				WideCharToMultiByte(CP_ACP, 0, src.viewGdiDeviceName, -1, gdiName, sizeof(gdiName), NULL, NULL);
				if (strcmp(gdiName, m->szDevice) != 0)
					continue;

				// Found it; ask for the EDID friendly name on the target.
				DISPLAYCONFIG_TARGET_DEVICE_NAME tgt;
				SecureZeroMemory(&tgt, sizeof(tgt));
				tgt.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
				tgt.header.size = sizeof(tgt);
				tgt.header.adapterId = paths[i].targetInfo.adapterId;
				tgt.header.id = paths[i].targetInfo.id;
				if (DisplayConfigGetDeviceInfo(&tgt.header) == ERROR_SUCCESS)
				{
					if (tgt.monitorFriendlyDeviceName[0])
					{
						char friendly[64] = { 0 };
						WideCharToMultiByte(CP_ACP, 0, tgt.monitorFriendlyDeviceName, -1, friendly, sizeof(friendly), NULL, NULL);
						sMonitor = friendly;
					}
					else if (tgt.outputTechnology == DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INTERNAL
						|| tgt.outputTechnology == DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EMBEDDED
						|| tgt.outputTechnology == DISPLAYCONFIG_OUTPUT_TECHNOLOGY_UDI_EMBEDDED)
					{
						// Built-in laptop panels usually carry no EDID name; match Windows.
						sMonitor = "Internal Display";
					}
				}
				break;
			}
		}
	}

	if (!sMonitor.empty())
		return sMonitor;

	// Fallback: legacy driver name via EnumDisplayDevices.
	DISPLAY_DEVICE  dd, dm;

	SecureZeroMemory(&dd, sizeof(dd));
	dd.cb = sizeof(dd);

	SecureZeroMemory(&dm, sizeof(dm));
	dm.cb = sizeof(dm);

	for (DWORD numAdapter = 0; EnumDisplayDevices(NULL, numAdapter, &dd, EDD_GET_DEVICE_INTERFACE_NAME); numAdapter++)
	{
		if (!(dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP))
		{
			; // silently ignore as it's disabled or virtual
		}
		else if (strcmp(dd.DeviceName, m->szDevice))
		{
			; // silently ignore as it's not the right monitor
		}
		else if (EnumDisplayDevices(dd.DeviceName, 0, &dm, 0))
		{
			sMonitor = dm.DeviceString;
		}
	}
	return sMonitor;
}

BOOL CGDIGenerator::IsOnOtherMonitor ()
{
	BOOL		bResult = FALSE;
	MONITORINFO	mi;
	RECT		Rect, Rect2;

	if ( m_monitorNb > 1 && m_hMonitor[m_activeMonitorNum] != NULL )
	{
		mi.cbSize = sizeof ( mi );
		GetMonitorInfo ( m_hMonitor[m_activeMonitorNum], & mi );

		AfxGetMainWnd () -> GetWindowRect ( & Rect );
		if ( ! IntersectRect ( & Rect2, & Rect, & mi.rcMonitor ) )
			bResult = TRUE;
	}

	return bResult;
}

void CGDIGenerator::Serialize(CArchive& archive)
{
	CGenerator::Serialize(archive);

	if (archive.IsStoring())
	{
		int version=3;
		archive << version;
		archive << m_displayWindow.m_offsetx;
		archive << m_displayWindow.m_offsety;
		archive << m_displayWindow.m_rectSizePercent;
		archive << m_displayWindow.m_bgStimPercent;
		archive << m_displayWindow.m_Intensity;
		archive << m_activeMonitorNum;
		archive << m_nDisplayMode;
	}
	else
	{
		int version;
		archive >> version;
		if ( version > 3 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		if ( version > 2)
		{
			archive >> m_displayWindow.m_offsetx;
			archive >> m_displayWindow.m_offsety;
		}
		archive >> m_displayWindow.m_rectSizePercent;
		archive >> m_displayWindow.m_bgStimPercent;
		archive >> m_displayWindow.m_Intensity;
		archive >> m_activeMonitorNum;
		if ( version > 1 )
		{
			int m_cDisplayMode = m_nDisplayMode;
			archive >> m_nDisplayMode;
			if (m_nDisplayMode != m_cDisplayMode)
			{
				GetColorApp()->InMeasureMessageBox("Restoring generator setting from save file...", "Generator Change", MB_OK);
				GetConfig()->WriteProfileInt("GDIGenerator","DisplayMode", m_nDisplayMode);
				GetConfig()->RefreshUse10bitLevels();
				SetPropertiesSheetValues();
			}
		}
		else
			m_nDisplayMode = DISPLAY_DEFAULT_MODE;
	}
}

void CGDIGenerator::SetPropertiesSheetValues()
{
	CGenerator::SetPropertiesSheetValues();

	m_GDIGenePropertiesPage.m_doScreenBlanking = m_doScreenBlanking;

	m_GDIGenePropertiesPage.m_rectSizePercent=m_displayWindow.m_rectSizePercent;
	m_GDIGenePropertiesPage.m_bgStimPercent=m_displayWindow.m_bgStimPercent;
	m_GDIGenePropertiesPage.m_Intensity=m_displayWindow.m_Intensity;
	m_GDIGenePropertiesPage.m_offsetx=m_displayWindow.m_offsetx;
	m_GDIGenePropertiesPage.m_offsety=m_displayWindow.m_offsety;
	m_GDIGenePropertiesPage.m_activeMonitorNum=m_activeMonitorNum;
	m_GDIGenePropertiesPage.m_nDisplayMode=m_nDisplayMode;
	m_GDIGenePropertiesPage.m_b16_235=m_b16_235;
	m_GDIGenePropertiesPage.m_busePic=m_busePic;
	m_GDIGenePropertiesPage.m_bLinear=m_bLinear;
	m_GDIGenePropertiesPage.m_bdispTrip=m_bdispTrip;
	m_GDIGenePropertiesPage.m_brPi_user=m_brPi_user;
	m_GDIGenePropertiesPage.m_b10bitPGen=m_b10bitPGen;
	m_GDIGenePropertiesPage.m_b10bitMadvr=m_b10bitMadvr;
	m_GDIGenePropertiesPage.m_madVR_3d=m_madVR_3d;
	m_GDIGenePropertiesPage.m_madVR_vLUT=m_madVR_vLUT;
	m_GDIGenePropertiesPage.m_madVR_HDR=m_madVR_HDR;
	m_GDIGenePropertiesPage.m_madVR_OSD=m_madVR_OSD;
}

void CGDIGenerator::GetPropertiesSheetValues()
{
	m_GeneratorPropertiePage.m_doScreenBlanking = m_GDIGenePropertiesPage.m_doScreenBlanking;
	CGenerator::GetPropertiesSheetValues();

	if( m_displayWindow.m_rectSizePercent != m_GDIGenePropertiesPage.m_rectSizePercent )
	{
		m_displayWindow.m_rectSizePercent=m_GDIGenePropertiesPage.m_rectSizePercent;
		GetConfig()->WriteProfileInt("GDIGenerator","SizePercent",m_displayWindow.m_rectSizePercent);
		SetModifiedFlag(TRUE);
	}

	if( m_displayWindow.m_bgStimPercent != m_GDIGenePropertiesPage.m_bgStimPercent )
	{
		m_displayWindow.m_bgStimPercent=m_GDIGenePropertiesPage.m_bgStimPercent;
		GetConfig()->WriteProfileInt("GDIGenerator","bgStimPercent",m_displayWindow.m_bgStimPercent);
		SetModifiedFlag(TRUE);
	}

	if( m_displayWindow.m_Intensity != m_GDIGenePropertiesPage.m_Intensity )
	{
		m_displayWindow.m_Intensity=m_GDIGenePropertiesPage.m_Intensity;
		GetConfig()->WriteProfileInt("GDIGenerator","Intensity",m_displayWindow.m_Intensity);
		SetModifiedFlag(TRUE);
	}

	if( m_displayWindow.m_offsetx != m_GDIGenePropertiesPage.m_offsetx )
	{
		m_displayWindow.m_offsetx=m_GDIGenePropertiesPage.m_offsetx;
		GetConfig()->WriteProfileInt("GDIGenerator","XOffset",m_displayWindow.m_offsetx);
		SetModifiedFlag(TRUE);
	}

	if( m_displayWindow.m_offsety != m_GDIGenePropertiesPage.m_offsety )
	{
		m_displayWindow.m_offsety=m_GDIGenePropertiesPage.m_offsety;
		GetConfig()->WriteProfileInt("GDIGenerator","YOffset",m_displayWindow.m_offsety);
		SetModifiedFlag(TRUE);
	}

	if( m_activeMonitorNum!=m_GDIGenePropertiesPage.m_activeMonitorNum )
	{
		m_activeMonitorNum=m_GDIGenePropertiesPage.m_activeMonitorNum;
		SetModifiedFlag(TRUE);
	}

	if( m_nDisplayMode!=m_GDIGenePropertiesPage.m_nDisplayMode )
	{
		m_nDisplayMode=m_GDIGenePropertiesPage.m_nDisplayMode;
		GetConfig()->WriteProfileInt("GDIGenerator","DisplayMode",m_nDisplayMode);
		SetModifiedFlag(TRUE);
	}

	if ( m_b16_235!=m_GDIGenePropertiesPage.m_b16_235 )
	{
		m_b16_235=m_GDIGenePropertiesPage.m_b16_235;
		GetConfig()->WriteProfileInt("GDIGenerator","RGB_16_235",m_b16_235);
		SetModifiedFlag(TRUE);
	}

	if ( m_busePic!=m_GDIGenePropertiesPage.m_busePic )
	{
		m_busePic=m_GDIGenePropertiesPage.m_busePic;
		GetConfig()->WriteProfileInt("GDIGenerator","USEPIC",m_busePic);
		SetModifiedFlag(TRUE);
	}

	if ( m_bLinear!=m_GDIGenePropertiesPage.m_bLinear )
	{
		m_bLinear=m_GDIGenePropertiesPage.m_bLinear;
		GetConfig()->WriteProfileInt("GDIGenerator","LOADLINEAR",m_bLinear);
		SetModifiedFlag(TRUE);
	}

	if ( m_bHdr10!=m_GDIGenePropertiesPage.m_bHdr10 )
	{
		m_bHdr10=m_GDIGenePropertiesPage.m_bHdr10;
		GetConfig()->WriteProfileInt("GDIGenerator","EnableHDR10",m_bHdr10);
		SetModifiedFlag(TRUE);
	}

	if ( m_bdispTrip!=m_GDIGenePropertiesPage.m_bdispTrip )
	{
		m_bdispTrip=m_GDIGenePropertiesPage.m_bdispTrip;
		GetConfig()->WriteProfileInt("GDIGenerator","DISPLAYTRIPLETS",m_bdispTrip);
		SetModifiedFlag(TRUE);
	}

	if ( m_brPi_user!=m_GDIGenePropertiesPage.m_brPi_user )
	{
		m_brPi_user=m_GDIGenePropertiesPage.m_brPi_user;
		GetConfig()->WriteProfileInt("GDIGenerator","DISPLAYRPIUSER",m_brPi_user);
		SetModifiedFlag(TRUE);
	}

	if ( m_b10bitPGen!=m_GDIGenePropertiesPage.m_b10bitPGen )
	{
		m_b10bitPGen=m_GDIGenePropertiesPage.m_b10bitPGen;
		GetConfig()->WriteProfileInt("GDIGenerator","TenBitPGen",m_b10bitPGen);
		SetModifiedFlag(TRUE);
	}

	if ( m_b10bitMadvr!=m_GDIGenePropertiesPage.m_b10bitMadvr )
	{
		m_b10bitMadvr=m_GDIGenePropertiesPage.m_b10bitMadvr;
		GetConfig()->WriteProfileInt("GDIGenerator","TenBitMadvr",m_b10bitMadvr);
		SetModifiedFlag(TRUE);
	}

    if ( m_madVR_3d!=m_GDIGenePropertiesPage.m_madVR_3d )
	{
		m_madVR_3d=m_GDIGenePropertiesPage.m_madVR_3d;
		GetConfig()->WriteProfileInt("GDIGenerator","MADVR3D",m_madVR_3d);
		SetModifiedFlag(TRUE);
	}

    if ( m_madVR_vLUT!=m_GDIGenePropertiesPage.m_madVR_vLUT )
	{
		m_madVR_vLUT=m_GDIGenePropertiesPage.m_madVR_vLUT;
		GetConfig()->WriteProfileInt("GDIGenerator","MADVRvLUT",m_madVR_vLUT);
		SetModifiedFlag(TRUE);
	}

    if ( m_madVR_HDR!=m_GDIGenePropertiesPage.m_madVR_HDR )
	{
		m_madVR_HDR=m_GDIGenePropertiesPage.m_madVR_HDR;
		GetConfig()->WriteProfileInt("GDIGenerator","MADVRHDR",m_madVR_HDR);
		SetModifiedFlag(TRUE);
	}

	if ( m_madVR_OSD!=m_GDIGenePropertiesPage.m_madVR_OSD )
	{
		m_madVR_OSD=m_GDIGenePropertiesPage.m_madVR_OSD;
		GetConfig()->WriteProfileInt("GDIGenerator","MADVROSD",m_madVR_OSD);
		SetModifiedFlag(TRUE);
	}

	// DisplayMode / TenBitPGen may have changed above; refresh the cached
	// 10-bit-levels flag so charts/measure pick it up without per-call INI reads.
	GetConfig()->RefreshUse10bitLevels();
}

// Forward declarations for the DVDO serial helpers defined further down (both in
// an anonymous namespace => internal linkage within this translation unit).
namespace { bool DvdoOpen(const CString& comPort, int colorSpace, int range, int outputFormat, CString* fwOut, bool sendSetup); void DvdoClose(); }
namespace { bool MuriConnect(bool useNet, const CString& ip, const CString& com); void MuriDisconnect(); void MuriSetTcpPort(int port); }

BOOL CGDIGenerator::Init(UINT nbMeasure, bool isSpecial)
{
//	GetColorApp()->InMeasureMessageBox( "    ** GDI Generator Init **", "Error", MB_ICONINFORMATION);
	BOOL	bOk, bOnOtherMonitor;
	GetMonitorList();
	m_nDisplayMode =  		GetConfig()->GetProfileInt("GDIGenerator","DisplayMode", DISPLAY_DEFAULT_MODE);
	if (!m_bConnect && m_bLinear) //linear gamma tables
	{
		char arg[255];
		CString str = GetConfig () -> m_ApplicationPath;
		CString str1 = GetConfig () -> m_ApplicationPath;
		str += "\\tools\\dispwin.exe";
		str1 += "\\tools\\current.cal";
		_snprintf(arg, sizeof(arg), " -d%d -s %s", m_activeMonitorNum+1, (LPCTSTR)str1);
		arg[sizeof(arg)-1] = 0;
		ShellExecute(NULL, "open", str, arg, NULL, SW_HIDE);
		Sleep(100);
		sprintf(arg," -d%d -c", m_activeMonitorNum+1);
		ShellExecute(NULL, "open", str, arg, NULL, SW_HIDE);
		m_bConnect = TRUE;
	}

	m_displayWindow.SetDisplayMode(m_nDisplayMode);
	m_displayWindow.SetRGBScale(m_b16_235);
	m_displayWindow.MoveToMonitor(m_hMonitor[m_activeMonitorNum]);

	if (m_nDisplayMode == DISPLAY_GDI || m_nDisplayMode == DISPLAY_OVERLAY || m_nDisplayMode == DISPLAY_GDI_nBG ||
		m_nDisplayMode == DISPLAY_GDI_Hide || m_nDisplayMode == DISPLAY_VMR9 || isSpecial)
	{
		if (!m_HdrInterface)
		{
			OutputDebugString("Create HdrInterface\n");
			m_HdrInterface = GetNewHdrInterface(m_displayWindow.hWnd, m_hMonitor[m_activeMonitorNum]);
		}
		else
		{
			OutputDebugString("Set HdrInterface's hMonitor and hWnd\n");
			m_HdrInterface->SetWindowMonitor(m_displayWindow.hWnd, m_hMonitor[m_activeMonitorNum]);
		}
		if (m_HdrInterface)
		{
			OutputDebugString("HdrInterface exists\n");
			OutputDebugString(m_bHdr10 ? "HDR10 enabled\n":"HDR10 disabled\n");
			LIBHDR_HDR_METADATA_HDR10 metaData = {0};
			if (m_bHdr10)
			{
				// Dump values to debug.
				char buf[1024];
				OutputDebugString("HDR10 Metadata to be used:\n");
				sprintf_s(buf, "Red:   X = %.4f, Y = %.4f\n", GetConfig()->m_manualRedx, GetConfig()->m_manualRedy);
				OutputDebugString(buf);
				sprintf_s(buf, "Green: X = %.4f, Y = %.4f\n", GetConfig()->m_manualGreenx, GetConfig()->m_manualGreeny);
				OutputDebugString(buf);
				sprintf_s(buf, "Blue:  X = %.4f, Y = %.4f\n", GetConfig()->m_manualBluex, GetConfig()->m_manualBluey);
				OutputDebugString(buf);
				sprintf_s(buf, "White: X = %.4f, Y = %.4f\n", GetConfig()->m_manualWhitex, GetConfig()->m_manualWhitey);
				OutputDebugString(buf);
				sprintf_s(buf, "Master Max = %.2f, Master Min = %.4f\n", GetConfig()->m_MasterMaxL, GetConfig()->m_MasterMinL);
				OutputDebugString(buf);
				sprintf_s(buf, "Content Max = %.2f, Frame Avg. Max = %.2f\n", GetConfig()->m_ContentMaxL, GetConfig()->m_FrameAvgMaxL);
				OutputDebugString(buf);

				// Default to DCI/P3 primaries
				metaData.RedPrimary[0] = UINT16(GetConfig()->m_manualRedx * 50000.0);
				metaData.RedPrimary[1] = UINT16(GetConfig()->m_manualRedy * 50000.0);
				metaData.GreenPrimary[0] = UINT16(GetConfig()->m_manualGreenx * 50000.0);
				metaData.GreenPrimary[1] = UINT16(GetConfig()->m_manualGreeny * 50000.0);
				metaData.BluePrimary[0] = UINT16(GetConfig()->m_manualBluex * 50000.0);
				metaData.BluePrimary[1] = UINT16(GetConfig()->m_manualBluey * 50000.0);
				metaData.WhitePoint[0] = UINT16(GetConfig()->m_manualWhitex * 50000.0);
				metaData.WhitePoint[1] = UINT16(GetConfig()->m_manualWhitey * 50000.0);
				// Default luminosity levels.
				metaData.MaxMasteringLuminance = UINT(GetConfig()->m_MasterMaxL * 10000.0);
				metaData.MinMasteringLuminance = UINT(GetConfig()->m_MasterMinL * 10000.0);
				metaData.MaxContentLightLevel = USHORT(GetConfig()->m_ContentMaxL);
				metaData.MaxFrameAverageLightLevel = USHORT(GetConfig()->m_FrameAvgMaxL);
			}
			HDR_STATUS hdrStat = m_HdrInterface->SetHDR10Mode(m_bHdr10, metaData);
			if (SUCCEEDED(hdrStat))
				OutputDebugString("HDR mode switch successful\n");
			else
			{
				char buffer[1024];
				sprintf_s(buffer, "HDR mode switch failed, error number %d\n", (int)hdrStat);
				OutputDebugString(buffer);
			}
		}
		else
			OutputDebugString("HdrInterface doesn't exist\n");
	}

	if (m_nDisplayMode == DISPLAY_GDI || m_nDisplayMode == DISPLAY_GDI_nBG || isSpecial )
		m_displayWindow.ShowWindow(SW_SHOWMAXIMIZED);
	if (m_nDisplayMode == DISPLAY_GDI_Hide && !isSpecial) //to use test colour window instead
		m_displayWindow.ShowWindow(SW_HIDE);
	
	bOnOtherMonitor = IsOnOtherMonitor ();

	BOOL bExternalSurface = ( m_nDisplayMode == DISPLAY_rPI || m_nDisplayMode == DISPLAY_ccast || m_nDisplayMode == DISPLAY_DVDO || m_nDisplayMode == DISPLAY_MURIDEO );

	if ( ! bOnOtherMonitor && ! bExternalSurface )
	{
		// Deactivate blanking when generator window is on the same monitor
		m_bBlankingCanceled = m_doScreenBlanking;
		m_doScreenBlanking = FALSE;
	}

	bOk = CGenerator::Init (nbMeasure );
	m_displayWindow.m_rPiSock = sock;

	// Re-load the DVDO/Murideo transport settings from config: the "... settings..." dialogs
	// write these keys directly, but the generator's copies are set only in the constructor
	// (built once per document, in CreateGenerator), so without this Init would open the
	// constructor-time port/IP and ignore anything the dialog changed this session.
	if ( m_nDisplayMode == DISPLAY_DVDO || m_nDisplayMode == DISPLAY_MURIDEO )
	{
		m_dvdoComPort      = GetConfig()->GetProfileString("GDIGenerator","DvdoComPort","");
		m_dvdoColorSpace   = GetConfig()->GetProfileInt("GDIGenerator","DvdoColorSpace",0);
		m_dvdoRange        = GetConfig()->GetProfileInt("GDIGenerator","DvdoRange",0);
		m_dvdoOutputFormat = GetConfig()->GetProfileInt("GDIGenerator","DvdoOutputFormat",0);
		m_muriComPort      = GetConfig()->GetProfileString("GDIGenerator","MuriComPort","");
		m_muriIp           = GetConfig()->GetProfileString("GDIGenerator","MuriIp","192.168.1.239");
		m_muriUseNetwork   = GetConfig()->GetProfileInt("GDIGenerator","MuriUseNetwork",1);
		m_muriTcpPort      = GetConfig()->GetProfileInt("GDIGenerator","MuriTcpPort",23);
	}

	if ( m_nDisplayMode == DISPLAY_DVDO )
	{
		CString fw;
		// Just open the port for AA patches; do NOT re-send resolution/colour-space here (the
		// "DVDO settings..." dialog's Apply owns those - the device retains them). Re-sending
		// 6C/61 every measurement would needlessly re-lock the display. Mirrors the Murideo.
		BOOL opened = DvdoOpen(m_dvdoComPort, m_dvdoColorSpace, m_dvdoRange, m_dvdoOutputFormat, &fw, false /*no setup*/);
		if ( ! opened )
		{
			if ( ! m_initShowedError )
				GetColorApp()->InMeasureMessageBox("Could not open the DVDO AVLab TPG serial port.\nSelect the correct COM port in the generator options.", "DVDO AVLab TPG", MB_ICONERROR);
			m_initShowedError = TRUE;
		}
		bOk = bOk && opened;	// combine with the base CGenerator::Init result, don't mask its failure
	}

	if ( m_nDisplayMode == DISPLAY_MURIDEO )
	{
		MuriSetTcpPort((m_muriTcpPort > 0) ? m_muriTcpPort : 4001);	// raw-TCP port for colour patches
		BOOL connected = MuriConnect(m_muriUseNetwork != 0, m_muriIp, m_muriComPort);
		if ( ! connected )
		{
			if ( ! m_initShowedError )
				GetColorApp()->InMeasureMessageBox("Could not reach the Murideo Seven-G.\nCheck the IP address (network) or COM port (serial) in the generator options.", "Murideo Seven-G", MB_ICONERROR);
			m_initShowedError = TRUE;
		}
		bOk = bOk && connected;	// combine with the base CGenerator::Init result, don't mask its failure
	}

	if ( ! bOnOtherMonitor )
	{
		GetColorApp() -> SetPatternWindow ( & m_displayWindow );
		GetColorApp() -> EndMeasureCursor ();
	}
	else
	{
		GetColorApp() -> RestoreMeasureCursor ();
	}
	m_bisInited = TRUE;

	return bOk;
}

BOOL CGDIGenerator::DisplayRGBColormadVR( const ColorRGBDisplay& clr, bool first, UINT nPattern )
{
	//init done in generator.cpp 
      int blackLevel, whiteLevel;
      double r, g, b;
      int rT,gT,bT;
      madVR_GetBlackAndWhiteLevel ( &blackLevel, &whiteLevel);
      // if madvr is sending video levels then no dithering and targets are fine, if madvr is sending full (or custom) range then you should dither
      // and again targets will be fine.
      r = (clr[0] / 100. );
      g = (clr[1] / 100. );
      b = (clr[2] / 100. );
	  //What rounded int level will be sent by madVR if dithering is turned off 
      rT = (int) (r * (whiteLevel - blackLevel) + blackLevel + 0.5);
      gT = (int) (g * (whiteLevel - blackLevel) + blackLevel + 0.5);
      bT = (int) (b * (whiteLevel - blackLevel) + blackLevel + 0.5);
      char aBuf[128];
	  madVR_SetDisableOsdButton(!m_madVR_OSD);
	  madVR_SetHdrButton(m_madVR_HDR);
	  if (m_madVR_HDR)
		  madVR_SetHdrMetadata(GetConfig()->m_manualRedx, GetConfig()->m_manualRedy, GetConfig()->m_manualGreenx, GetConfig()->m_manualGreeny, GetConfig()->m_manualBluex, GetConfig()->m_manualBluey, GetConfig()->m_manualWhitex, GetConfig()->m_manualWhitey, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_ContentMaxL, GetConfig()->m_FrameAvgMaxL);
	  CGDIGenerator Cgen;
	  double bgstim = Cgen.m_bgStimPercent / 100.;
	  madVR_SetPatternConfig(Cgen.m_rectSizePercent, int (bgstim * 100), -1, 20);
	  if (first)
	  {
    	  sprintf(aBuf,"%s","Display settling, please wait...");
	      const CString s(aBuf);
	      madVR_SetOsdText(CT2CW(s));
	      if (!madVR_ShowRGB(.75, .75, .75))
		  {
			MessageBox(0, "Test pattern failure.", "Error", MB_ICONERROR);
			return false;
		  }
		  for (int i=0;i<=24;i++)
		  {
			  madVR_ShowRGB(double(i) * 10.0 / 255.0,double(i) * 10.0 / 255.0,double(i) * 10.0 / 255.0);
			  Sleep(33);
		  }
	  }
      if (m_madVR_3d)
    	  sprintf(aBuf,"HCFR is measuring display, pleaset wait...%d:%d:%d[3dlut disabled]",rT,gT,bT);
      else
    	  sprintf(aBuf,"HCFR is measuring display, please wait...%d:%d:%d",rT,gT,bT);
      const CString s2(aBuf);
	  madVR_SetOsdText(CT2CW(s2));

	m_nPat++;
	if ( (m_nPat % GetConfig()->m_ablFreq == 0) && GetConfig()->m_bABL )
	{
		double lvl = m_b16_235 ? 2.19 * GetConfig()->m_ablLevel + 16 : 2.55 * GetConfig()->m_ablLevel;
		//sleep prevention every 40 patterns for longer sequences
		madVR_SetPatternConfig(100, 0, -1, 0);
		if (!madVR_ShowRGB(lvl, lvl, lvl))
		{
			MessageBox(0, "Test pattern failure.", "Error", MB_ICONERROR);
			return false;
		}	 
//		madVR_ShowRGB(.4, .4 , .4);
		Sleep(GetConfig()->m_ablDuration);
		madVR_SetPatternConfig(Cgen.m_rectSizePercent, int (bgstim * 100), -1, 20);
	}

	if (!madVR_ShowRGB(r, g, b))
      {
        MessageBox(0, "Test pattern failure.", "Error", MB_ICONERROR);
		return false;
      } 

	// Sleep 80 ms while dispatching messages to ensure window is really displayed
		MSG		Msg;
		HWND	hEscapeWnd = NULL;
		DWORD	dwWait = GetConfig () -> GetProfileInt ( "Debug", "WaitAfterDisplayPattern", 80 );
		DWORD	dwStart = GetTickCount();
		DWORD	dwNow = dwStart;
		
		// Wait until dwWait time is expired, but ensures all posted messages are treated even if wait time is zero
		while((dwNow - dwStart) < dwWait)
		{
			while(PeekMessage(&Msg, NULL, NULL, NULL, PM_REMOVE))
			{
				if ( ( Msg.message == WM_KEYDOWN || Msg.message == WM_KEYUP ) && Msg.wParam == VK_ESCAPE )
				{
					// Do not treat this message, store it for later use
					hEscapeWnd = Msg.hwnd;
				}
				else
				{
					TranslateMessage ( & Msg );
					DispatchMessage ( & Msg );
				}
				Sleep(0);
			}
			dwNow = GetTickCount();
		}
		if ( hEscapeWnd )
		{
			// Escape key detected and stored during above loop: put it again in message loop to allow detection
			::PostMessage ( hEscapeWnd, WM_KEYDOWN, VK_ESCAPE, NULL );
			::PostMessage ( hEscapeWnd, WM_KEYUP, VK_ESCAPE, NULL );
		}

return TRUE;
}

BOOL CGDIGenerator::DisplayRGBCCast( const ColorRGBDisplay& clr, bool first, UINT nPattern )
{
	//init done in generator.cpp 
    double r, g, b;
	CGDIGenerator Cgen;
	dispwin *ccwin=Cgen.ccwin;
	double bgstim = Cgen.m_bgStimPercent / 100.;
	//Chromecast needs full range RGB
    r = ((clr[0]) / 100. );
	g = ((clr[1]) / 100. );
    b = ((clr[2]) / 100. );

	double R1=0.,G1=0.,B1=0.;
	//subtract window area for APL
	if (Cgen.m_rectSizePercent < 100)
	{
		R1 = max(0,(bgstim - r*Cgen.m_rectSizePercent/100.))/(1-Cgen.m_rectSizePercent/100. );
		G1 = max(0,(bgstim - g*Cgen.m_rectSizePercent/100.))/(1-Cgen.m_rectSizePercent/100. );
		B1 = max(0,(bgstim - b*Cgen.m_rectSizePercent/100.))/(1-Cgen.m_rectSizePercent/100. );
		R1 = min(R1, 1);
		G1 = min(G1, 1);
		B1 = min(B1, 1);
	}

	if (ccwin->height == 0) 
	{
		MessageBox(0, "Test pattern failure.", "Error", MB_ICONERROR);
		return false;
	} 

	if (ccwin->set_bg(ccwin,(R1+G1+B1)/3.) != 0)
	{
		MessageBox(0, "CCast Test pattern failure.", "set_bg", MB_ICONERROR);
		return false;
	} 

	if (first)
	{
		  if (ccwin->set_color(ccwin,0.75,0.75,0.75) != 0)
		  {
	        MessageBox(0, "CCast Test pattern failure.", "set_bg", MB_ICONERROR);
			return false;
		  }
		  for (int i=0;i<=24;i++)
		  {
			  ccwin->set_color(ccwin,double(i) * 10.0 /  255.0,double(i) * 10.0 / 255.0,double(i) * 10.0 / 255.0);
			  Sleep(50);
		  }
	}
	
	m_nPat++;
	if ( (m_nPat % GetConfig()->m_ablFreq == 0) && GetConfig()->m_bABL)
	{
		double lvl = m_b16_235 ? 2.19 * GetConfig()->m_ablLevel + 16 : 2.55 * GetConfig()->m_ablLevel;
		//sleep prevention
		ccwin->set_bg(ccwin,0);
		if (ccwin->set_color(ccwin, lvl, lvl, lvl) != 0 )
		{
	        MessageBox(0, "CCast Test pattern failure.", "set_color", MB_ICONERROR);
			return false;
		}
		Sleep(GetConfig()->m_ablDuration);

		ccwin->set_bg(ccwin,bgstim);
	}

		if (ccwin->set_color(ccwin,r,g,b) != 0 )
		{
	        MessageBox(0, "CCast Test pattern failure.", "set_color", MB_ICONERROR);
			return false;
		} 
	  
	// Sleep 80 ms while dispatching messages to ensure window is really displayed
		MSG		Msg;
		HWND	hEscapeWnd = NULL;
		DWORD	dwWait = GetConfig () -> GetProfileInt ( "Debug", "WaitAfterDisplayPattern", 80 );
		DWORD	dwStart = GetTickCount();
		DWORD	dwNow = dwStart;
		
		// Wait until dwWait time is expired, but ensures all posted messages are treated even if wait time is zero
		while((dwNow - dwStart) < dwWait)
		{
			while(PeekMessage(&Msg, NULL, NULL, NULL, PM_REMOVE))
			{
				if ( ( Msg.message == WM_KEYDOWN || Msg.message == WM_KEYUP ) && Msg.wParam == VK_ESCAPE )
				{
					// Do not treat this message, store it for later use
					hEscapeWnd = Msg.hwnd;
				}
				else
				{
					TranslateMessage ( & Msg );
					DispatchMessage ( & Msg );
				}
				Sleep(0);
			}
			dwNow = GetTickCount();
		}
		if ( hEscapeWnd )
		{
			// Escape key detected and stored during above loop: put it again in message loop to allow detection
			::PostMessage ( hEscapeWnd, WM_KEYDOWN, VK_ESCAPE, NULL );
			::PostMessage ( hEscapeWnd, WM_KEYUP, VK_ESCAPE, NULL );
		}

return TRUE;
}

BOOL CGDIGenerator::DisplayRGBColorrPI( const ColorRGBDisplay& clr, bool first, UINT nPattern )
{
	//init done in generator.cpp 
	int r=0, g=0, b=0;
	char CPat[256];
	CGDIGenerator Cgen;
	double bgstim = Cgen.m_bgStimPercent / 100.;
	double R1=0.,G1=0.,B1=0.;
	//subtract window area for APL
	if (Cgen.m_rectSizePercent < 100)
	{
		R1 = max(0,(bgstim*255 - clr[0]*Cgen.m_rectSizePercent/100.))/(1-Cgen.m_rectSizePercent/100. );
		G1 = max(0,(bgstim*255 - clr[1]*Cgen.m_rectSizePercent/100.))/(1-Cgen.m_rectSizePercent/100. );
		B1 = max(0,(bgstim*255 - clr[2]*Cgen.m_rectSizePercent/100.))/(1-Cgen.m_rectSizePercent/100. );
		R1 = min(R1, 255);
		G1 = min(G1, 255);
		B1 = min(B1, 255);
	}

		int bits = Cgen.m_b10bitPGen ? 10 : 8;
	r = PiPercentToCode ( clr[0], !!m_b16_235, bits );
	g = PiPercentToCode ( clr[1], !!m_b16_235, bits );
	b = PiPercentToCode ( clr[2], !!m_b16_235, bits );
	int bgR = PiBackground8ToCode ( R1, !!m_b16_235, bits );
	int bgG = PiBackground8ToCode ( G1, !!m_b16_235, bits );
	int bgB = PiBackground8ToCode ( B1, !!m_b16_235, bits );

	m_nPat++;

	int x2 = Cgen.m_offsetx;
	int y2 = Cgen.m_offsety;
	if (x2 > 0)
		x2 = min(x2, rPi_xWidth / 2. - pow(Cgen.m_rectSizePercent/100.0,0.5) * rPi_xWidth / 2. );
	else
		x2 = max(x2, -1*(rPi_xWidth / 2. - pow(Cgen.m_rectSizePercent/100.0,0.5) * rPi_xWidth / 2.) );
	if (y2 > 0)
		y2 = min(y2, rPi_yHeight / 2. - pow(Cgen.m_rectSizePercent/100.0,0.5) * rPi_yHeight / 2.);
	else
		y2 = max(y2, -1*(rPi_yHeight / 2. - pow(Cgen.m_rectSizePercent/100.0,0.5) * rPi_yHeight / 2.) );

	if ( (m_nPat % GetConfig()->m_ablFreq == 0) && GetConfig()->m_bABL)
	{
				int abl;
		if (bits == 10)
			abl = PiPercentToCode ( GetConfig()->m_ablLevel, !!m_b16_235, 10 );
		else
			abl = m_b16_235 ? (BYTE)(2.19 * GetConfig()->m_ablLevel + 16) : (BYTE)(2.55 * GetConfig()->m_ablLevel);
		sprintf_s(CPat,"RGB=%s;%d,%d;100;%d,%d,%d;%d,%d,%d;-1,-1", bits == 10 ? "RECTANGLE10bit" : "RECTANGLE", (int)(pow((double)(Cgen.m_rectSizePercent)/100.0,0.5) * rPi_xWidth),(int)(pow((double)(Cgen.m_rectSizePercent)/100.0,0.5) * rPi_yHeight),abl,abl,abl,0,0,0);
		if (sock && CGenerator::_RB8PG_send && CPat[0])
			CGenerator::_RB8PG_send(sock,CPat);
		else
			GetColorApp()->InMeasureMessageBox( "Error communicating with rPI", "Error", MB_ICONINFORMATION);

		Sleep(GetConfig()->m_ablDuration);
	}

				// 10-bit mode always sends the direct RECTANGLE10bit command: the
		// user/triplet template paths stay 8-bit until daemon template
		// support for 10-bit values is confirmed on hardware.
		if (m_brPi_user && bits != 10) //user background
			sprintf_s(CPat, "TESTTEMPLATEDISK:PatternDynamic:%d,%d,%d",r,g,b);
		else if (!m_bdispTrip || bits == 10)
			sprintf_s(CPat,"RGB=%s;%d,%d;100;%d,%d,%d;%d,%d,%d;-1,-1,%d,%d", bits == 10 ? "RECTANGLE10bit" : "RECTANGLE", (int)(pow((double)(Cgen.m_rectSizePercent)/100.0,0.5) * rPi_xWidth),(int)(pow((double)(Cgen.m_rectSizePercent)/100.0,0.5) * rPi_yHeight),r,g,b,bgR,bgG,bgB,x2,y2);
		else
			sprintf_s(CPat, "TESTTEMPLATERAMDISK:HCFR:%d,%d,%d;%d,%d,%d",r,g,b,bgR,bgG,bgB);

	CString debug=_T(CPat);

		if (sock && CGenerator::_RB8PG_send && CPat[0])
			CGenerator::_RB8PG_send(sock,CPat);
		else
			GetColorApp()->InMeasureMessageBox( "Error communicating with rPI", "Error", MB_ICONINFORMATION);

	// Sleep 80 ms while dispatching messages to ensure window is really displayed
		MSG		Msg;
		HWND	hEscapeWnd = NULL;
		DWORD	dwWait = GetConfig () -> GetProfileInt ( "Debug", "WaitAfterDisplayPattern", 80 );
		DWORD	dwStart = GetTickCount();
		DWORD	dwNow = dwStart;
		
		// Wait until dwWait time is expired, but ensures all posted messages are treated even if wait time is zero
		while((dwNow - dwStart) < dwWait)
		{
			while(PeekMessage(&Msg, NULL, NULL, NULL, PM_REMOVE))
			{
				if ( ( Msg.message == WM_KEYDOWN || Msg.message == WM_KEYUP ) && Msg.wParam == VK_ESCAPE )
				{
					// Do not treat this message, store it for later use
					hEscapeWnd = Msg.hwnd;
				}
				else
				{
					TranslateMessage ( & Msg );
					DispatchMessage ( & Msg );
				}
				Sleep(0);
			}
			dwNow = GetTickCount();
		}
		if ( hEscapeWnd )
		{
			// Escape key detected and stored during above loop: put it again in message loop to allow detection
			::PostMessage ( hEscapeWnd, WM_KEYDOWN, VK_ESCAPE, NULL );
			::PostMessage ( hEscapeWnd, WM_KEYUP, VK_ESCAPE, NULL );
		}

return TRUE;

}

// ---------------------------------------------------------------------------
// DVDO AVLab TPG (DISPLAY_DVDO) serial control. Protocol per the AVLab TPG
// User's Guide, Appendix A: "STX '30' DataCount ID NUL <params> NUL ETX" at
// 115200 8N1 over the USB virtual serial port. DataCount is the decimal-ASCII
// byte count of everything from ID through the final NUL.
// ---------------------------------------------------------------------------
namespace {
	CSerialCom	s_dvdoPort;
	bool		s_dvdoOpen  = false;
	bool		s_dvdoArmed = false;		// TPG armed for AA patches? (see the first-patch arm in DisplayRGBColorDVDO)
	CString		s_dvdoDiag;					// human-readable status from the last DvdoOpen
	const int	kDvdoArmPattern = 35;		// 80=35 = "Black" (0 IRE full field, User's Guide page 14): arms the internal TPG with no visible flash

	// params are NUL-separated with a trailing NUL. A single-element vector gives
	// the "ID NUL value NUL" form (used for AA's space-joined value); several
	// elements give AF's "ID (NUL p)* NUL" form.
	std::string DvdoFrame(const char* id, const std::vector<std::string>& params)
	{
		std::string body = id;
		for (size_t i = 0; i < params.size(); ++i) { body += '\0'; body += params[i]; }
		body += '\0';
		// DataCount is UPPER-CASE HEX, ZERO-PADDED to at least 2 digits. This padding is
		// the crux: the firmware's parser reads a fixed 2-char count field, so a single-digit
		// count (e.g. "6") desyncs it and the whole command is silently ignored. That is why
		// only AA/AF ever worked (their counts are >=16 -> already 2 hex digits "16"/"1A"),
		// while 61/80/EA/6C (small counts) appeared "unsupported" - they were mis-framed.
		// Verified on hardware 2026-08-08: "06"/"05" made 61 (resolution) and 80 (patterns) work.
		char cnt[16]; sprintf(cnt, "%02X", (unsigned)body.size());
		std::string pkt;
		pkt += (char)0x02; pkt += "30"; pkt += cnt; pkt += body; pkt += (char)0x03;
		return pkt;
	}

	bool DvdoWrite(const std::string& pkt)
	{
		if (!s_dvdoOpen) return false;
		for (size_t i = 0; i < pkt.size(); ++i)
			if (!s_dvdoPort.WriteByte((BYTE)pkt[i])) return false;
		return true;
	}

	// Every 0x30 command elicits a Response packet (STX "01" .. ETX). The User's Guide 1.01
	// section "Command (30) and Response (01)" states the AVLab ALWAYS responds. If we never
	// read it, the acks back up in the device and, after a resolution change (61), it stops
	// displaying subsequent AA custom patches. Draining the ack after each command keeps the
	// device in sync. Hardware-confirmed 2026-08-08: with the drain, 61 -> AA displays reliably;
	// without it the AA patch blanks. The ack is immediate (~11 ms) - it is NOT a re-lock-done
	// signal - so a separate settle is still used after 61.
	void DvdoDrainResponse()
	{
		if (!s_dvdoOpen) return;
		BYTE b; int guard = 0;
		while (guard++ < 64 && s_dvdoPort.ReadByte(b)) { if (b == 0x03) break; }
	}

	bool DvdoCommand(const char* id, const std::vector<std::string>& params)
	{
		if (!DvdoWrite(DvdoFrame(id, params))) return false;
		FlushFileBuffers(s_dvdoPort.hComm);
		DvdoDrainResponse();
		return true;
	}

	// Type-20 query: STX "20" <2-hex count> <id> NUL ETX. Reply is
	// STX "21" <count> <id> NUL <value> NUL <cksum> ETX; return the <value> between the NULs.
	// (These queries only ever "locked up" the device before because of the single-digit
	// count bug - with the 2-digit fix they answer cleanly.)
	bool DvdoQuery(const char* id, CString& valueOut)
	{
		valueOut.Empty();
		if (!s_dvdoOpen) return false;
		std::string body = id; body += '\0';
		char cnt[8]; sprintf(cnt, "%02X", (unsigned)body.size());
		std::string q; q += (char)0x02; q += "20"; q += cnt; q += body; q += (char)0x03;
		PurgeComm(s_dvdoPort.hComm, PURGE_RXCLEAR);
		s_dvdoPort.SetCommunicationTimeouts(40, 0, 500, 0, 300);	// wait for the reply
		for (size_t i = 0; i < q.size(); ++i) s_dvdoPort.WriteByte((BYTE)q[i]);
		FlushFileBuffers(s_dvdoPort.hComm);
		std::string r; BYTE b; int guard = 0;
		while (guard++ < 128 && s_dvdoPort.ReadByte(b)) { r += (char)b; if (b == 0x03) break; }
		s_dvdoPort.SetCommunicationTimeouts(20, 0, 80, 0, 300);		// restore
		// value is between the 1st and 2nd NUL
		size_t n1 = r.find('\0'); if (n1 == std::string::npos) return false;
		size_t n2 = r.find('\0', n1 + 1); if (n2 == std::string::npos) return false;
		valueOut = CString(r.substr(n1 + 1, n2 - n1 - 1).c_str());
		return true;
	}

	// Device status-value -> resolution name (the query numbering is the AVLab's own internal
	// timing table, NOT the command-61 codes; mapped empirically on hardware 2026-08-08).
	const char* DvdoResNameFromStatus(int v)
	{
		switch (v)
		{
		case 0: return "Auto";           case 5: return "576i";           case 6: return "576p";
		case 7: return "720p50";         case 8: return "1080i50";        case 9: return "1080p50";
		case 10: return "4K50 (4:2:0)";  case 13: return "480i";          case 14: return "480p";
		case 15: return "720p60";        case 16: return "1080i60";       case 17: return "1080p60";
		case 18: return "4K60 (4:2:0)";  case 21: return "1080p24";       case 22: return "4K24 (3840)";
		case 23: return "4K24 (4096)";   case 24: return "1080p25";       case 25: return "4K25";
		case 26: return "1080p30";       case 27: return "4K30";          case 28: return "VGA60";
		case 29: return "SVGA60";        case 30: return "XGA60";         case 31: return "SXGA60";
		default: return "?";
		}
	}

	CString DvdoErrText(DWORD e)
	{
		CString s;
		switch (e)
		{
		case ERROR_FILE_NOT_FOUND: s = _T("port not found (is the DVDO connected?)"); break;
		case ERROR_ACCESS_DENIED:  s = _T("access denied (port already in use by another program?)"); break;
		default: s.Format(_T("Windows error %lu"), (unsigned long)e); break;
		}
		return s;
	}

	bool DvdoOpen(const CString& comPort, int colorSpace, int /*range*/, int outputFormat, CString* fwOut, bool sendSetup)
	{
		if (comPort.IsEmpty()) { s_dvdoDiag = LS(IDS_GEN_NO_COM_SELECTED); return false; }
		if (!s_dvdoOpen)	// REUSE an already-open port; a close-then-immediate-reopen can fail
		{					// on the virtual COM driver (and would drop settings set via Apply).
			if (!s_dvdoPort.OpenPort(comPort))					// OpenPort prepends //./ so COM10+ works
			{
				DWORD e = GetLastError();
				s_dvdoDiag.Format(LS(IDS_GEN_DVDO_OPEN_FAIL), (LPCTSTR)comPort, (LPCTSTR)DvdoErrText(e));
				return false;
			}
			if (!s_dvdoPort.ConfigurePort(115200, 8, FALSE, NOPARITY, ONESTOPBIT) ||
			    !s_dvdoPort.SetCommunicationTimeouts(20, 0, 80, 0, 300))
			{
				// CSerialCom already CloseHandle()s on these failures - do NOT ClosePort again
				// (double-close) and do NOT set s_dvdoOpen, so no write hits a dead handle and
				// DvdoClose stays a no-op.
				s_dvdoDiag.Format(LS(IDS_GEN_DVDO_CONFIG_FAIL), (LPCTSTR)comPort);
				return false;
			}
			// NB: do NOT toggle DTR/RTS - the AVLab TPG locks up (needs a power cycle)
			// when the host asserts them. It receives host bytes without it.
			PurgeComm(s_dvdoPort.hComm, PURGE_RXCLEAR | PURGE_TXCLEAR);
			s_dvdoOpen = true;
			Sleep(150);		// let the USB virtual-COM link settle after open
		}

		// IMPORTANT: do NOT send any firmware/version query here. On the AVLab TPG the
		// type-20 query locks the device up - it stops responding to serial AND to its
		// IR remote until a power cycle - and it never replies anyway. Patches always use
		// AA full-triplet (0-255), which carries the exact RGB and works on F/W 1.01+.
		if (fwOut) fwOut->Empty();
		s_dvdoArmed = false;				// re-arm on the first AA patch of this session (see DisplayRGBColorDVDO)

		s_dvdoDiag.Format(LS(IDS_GEN_DVDO_OPENED), (LPCTSTR)comPort);

		// Only when actually starting output (not during a Detect/Test probe): set the
		// output colour space (6C: 1=RGB, 2=YC444, 3=YC422). AA carries colour space per
		// pattern anyway, so this is mainly for the 80 predefined patterns.
		// NOTE 1: do NOT send EA (Pass-Through mode). Hardware shows EA=0 puts the TPG into
		// pass-through/auto, which kicks it OUT of Test-Patterns mode - so 80 predefined
		// patterns then display nothing (AA still works because it overrides pass-through).
		// The device stays in whatever Pass-Through Mode the user set on its OSD/remote.
		if (sendSetup)
		{
			char cs[8]; sprintf(cs, "%d", colorSpace + 1); DvdoCommand("6C", std::vector<std::string>(1, cs));
			// Output format (command 61): decimal code from kDvdoFormats, verified against the
			// User's Guide 1.01 Appendix A table (1080p60 = 13). Only sent for a specific format
			// (code 0 = "Auto" leaves the device/OSD resolution untouched, so a working display is
			// never blanked). Now safe alongside AA patches because DvdoCommand drains the response
			// ack after every command (see DvdoDrainResponse) - previously the un-read ack backed
			// up and 61 would leave AA blanking. Changing resolution re-locks the HDMI link, so
			// settle before the first patch is sent.
			if (outputFormat > 0)
			{
				char fc[8]; sprintf(fc, "%d", outputFormat); DvdoCommand("61", std::vector<std::string>(1, fc));
				Sleep(GetConfig()->GetProfileInt("Debug", "DvdoFormatSettleMs", 2000));
			}
		}
		return true;
	}

	// Flush any pending TX before closing: closing a COM handle can discard bytes still
	// in the driver's transmit buffer, which drops a fire-and-close command (e.g. a
	// Show-pattern 80). FlushFileBuffers blocks until the bytes are actually sent.
	void DvdoClose() { if (s_dvdoOpen) { FlushFileBuffers(s_dvdoPort.hComm); Sleep(60); s_dvdoPort.ClosePort(); s_dvdoOpen = false; } }

	// Pre-Defined Test Patterns (command 80): value is the pattern code. Requires the port
	// open. No EA/Pass-Through command is ever sent (see the "do NOT send EA" note above),
	// and callers map "off" to 80=35 (full black), never 80=0 - which would disarm the TPG.
	bool DvdoSendPattern(int code)
	{
		if (!s_dvdoOpen) return false;
		char v[8]; sprintf(v, "%d", code);
		return DvdoCommand("80", std::vector<std::string>(1, v));
	}
} // namespace

// Returns TRUE if the port opened. msgOut carries a full human-readable status
// (the AA transport note on success, or the failure reason).
bool CGDIGenerator_DvdoTestConnection(const CString& comPort, int colorSpace, int range, CString& msgOut)
{
	CString fw;
	bool opened = DvdoOpen(comPort, colorSpace, range, 0 /*format n/a for probe*/, &fw, false /*probe only*/);
	msgOut = s_dvdoDiag;
	DvdoClose();
	return opened;
}

// ---------------------------------------------------------------------------
// DVDO built-in test patterns (command 80) and output formats (command 61),
// exposed for the prop-page pickers. Patterns are grouped by category; the codes
// come from Appendix A's pattern/format tables.
// ---------------------------------------------------------------------------
struct DvdoPatEntry { const char* cat; const char* name; int code; };
static const DvdoPatEntry kDvdoPats[] =
{
	{ "Geometry",           "Frame geometry (FRMGEOM)", 1 },
	{ "Geometry",           "Crosshatch coarse",        20 },
	{ "Geometry",           "Crosshatch fine",          21 },
	{ "Geometry",           "Focus",                    22 },
	{ "Sharpness / Motion", "Sharpness",                40 },
	{ "Sharpness / Motion", "EVOT pixel",               3 },
	{ "Sharpness / Motion", "EVOT H/V line",            4 },
	{ "Sharpness / Motion", "EVOT H line",              5 },
	{ "Sharpness / Motion", "Judder",                   6 },
	{ "Sharpness / Motion", "Brightness / Contrast",    2 },
	{ "Color bars",         "8-bar 75%",                7 },
	{ "Color bars",         "8-bar 100%",               8 },
	{ "Color bars",         "Half 7-bar 75%",           24 },
	{ "Color bars",         "Half 7-bar 100%",          25 },
	{ "Color bars",         "Half 8-bar 75%",           26 },
	{ "Color bars",         "Half 8-bar 100%",          27 },
	{ "Windows (IRE)",      "Window 10%",               9 },
	{ "Windows (IRE)",      "Window 20%",               10 },
	{ "Windows (IRE)",      "Window 30%",               11 },
	{ "Windows (IRE)",      "Window 40%",               12 },
	{ "Windows (IRE)",      "Window 50%",               13 },
	{ "Windows (IRE)",      "Window 60%",               14 },
	{ "Windows (IRE)",      "Window 70%",               15 },
	{ "Windows (IRE)",      "Window 80%",               16 },
	{ "Windows (IRE)",      "Window 90%",               17 },
	{ "Windows (IRE)",      "Window 100%",              18 },
	{ "Grayscale / Fields", "Grey ramp",                19 },
	{ "Grayscale / Fields", "Full white (100%)",        28 },
	{ "Grayscale / Fields", "Full black (0 IRE)",       35 },
	{ "Grayscale / Fields", "Half black/white",         23 },
	{ "Solid colors",       "Red 100%",                 29 },
	{ "Solid colors",       "Green 100%",               30 },
	{ "Solid colors",       "Blue 100%",                31 },
	{ "Solid colors",       "Cyan 100%",                32 },
	{ "Solid colors",       "Magenta 100%",             33 },
	{ "Solid colors",       "Yellow 100%",              34 },
	{ "PLUGE",              "White PLUGE 1",            36 },
	{ "PLUGE",              "Black PLUGE 1",            37 },
	{ "PLUGE",              "White PLUGE 2",            38 },
	{ "PLUGE",              "Black PLUGE 2",            39 },
};
static const int kDvdoPatN = sizeof(kDvdoPats) / sizeof(kDvdoPats[0]);

// Distinct category names, in first-seen order.
static int DvdoCatList(const char* out[], int maxN)
{
	int n = 0;
	for (int i = 0; i < kDvdoPatN && n < maxN; ++i)
	{
		bool seen = false;
		for (int j = 0; j < n; ++j) if (strcmp(out[j], kDvdoPats[i].cat) == 0) { seen = true; break; }
		if (!seen) out[n++] = kDvdoPats[i].cat;
	}
	return n;
}

int CGDIGenerator_DvdoCatCount()
{
	const char* cats[16];
	return DvdoCatList(cats, 16);
}

const char* CGDIGenerator_DvdoCatName(int ci)
{
	const char* cats[16];
	int n = DvdoCatList(cats, 16);
	return (ci >= 0 && ci < n) ? cats[ci] : "";
}

// Enumerate the patterns within category ci (in table order).
static int DvdoPatsInCat(int ci, int idxOut[], int maxN)
{
	const char* cats[16];
	int nc = DvdoCatList(cats, 16);
	if (ci < 0 || ci >= nc) return 0;
	const char* cat = cats[ci];
	int n = 0;
	for (int i = 0; i < kDvdoPatN && n < maxN; ++i)
		if (strcmp(kDvdoPats[i].cat, cat) == 0) idxOut[n++] = i;
	return n;
}

int CGDIGenerator_DvdoPatCountInCat(int ci)
{
	int idx[64];
	return DvdoPatsInCat(ci, idx, 64);
}

const char* CGDIGenerator_DvdoPatName(int ci, int pi)
{
	int idx[64];
	int n = DvdoPatsInCat(ci, idx, 64);
	return (pi >= 0 && pi < n) ? kDvdoPats[idx[pi]].name : "";
}

int CGDIGenerator_DvdoPatCode(int ci, int pi)
{
	int idx[64];
	int n = DvdoPatsInCat(ci, idx, 64);
	return (pi >= 0 && pi < n) ? kDvdoPats[idx[pi]].code : 0;
}

// Find the (category, pattern) indices for a stored pattern code; returns false if
// not found. Lets the prop page restore its two dropdowns from the saved code.
bool CGDIGenerator_DvdoFindPattern(int code, int& ciOut, int& piOut)
{
	const char* cats[16];
	int nc = DvdoCatList(cats, 16);
	for (int ci = 0; ci < nc; ++ci)
	{
		int idx[64];
		int n = DvdoPatsInCat(ci, idx, 64);
		for (int pi = 0; pi < n; ++pi)
			if (kDvdoPats[idx[pi]].code == code) { ciOut = ci; piOut = pi; return true; }
	}
	return false;
}

// Open the port (setup format/EA/6C), send pattern code (<0 or 0 = Off), close. The
// TPG keeps displaying the pattern after the port closes.
bool CGDIGenerator_DvdoShowPattern(const CString& comPort, int colorSpace, int outputFormat, int patternCode, CString& msgOut)
{
	// Reuse an already-open port (e.g. left open by a prior Show or a live session) - a
	// close-then-immediate-reopen can fail on the virtual COM driver. Only open fresh if
	// nothing is open yet.
	bool wasOpen = s_dvdoOpen;
	if (!s_dvdoOpen)
	{
		CString fw;
		if (!DvdoOpen(comPort, colorSpace, 0, outputFormat, &fw, true /*send setup*/))
		{
			msgOut = s_dvdoDiag;
			return false;
		}
	}
	// "Patterns off" (code <= 0) maps to 80=35, a full-black field (0% IRE) - NEVER 80=0.
	// On this firmware 80=0 DISARMS the internal TPG: the screen blanks to blue and AA custom
	// patches then no longer render until an 80 pattern re-arms it or a power cycle (hardware-
	// confirmed 2026-08-08). 80=35 shows full black AND keeps the TPG armed, so a following
	// measurement's AA patches still display.
	int code = patternCode > 0 ? patternCode : kDvdoArmPattern;
	// An 80 predefined pattern only displays when the serial Pass-Through Mode is set to
	// "Test Patterns" (AA ignores this and always shows; 80 respects it). Set it via EA
	// before selecting the pattern. EA=0 did not work, so try EA=1 = Test Patterns.
	DvdoCommand("EA", std::vector<std::string>(1, "1"));
	Sleep(120);
	bool ok = DvdoSendPattern(code);
	FlushFileBuffers(s_dvdoPort.hComm);		// push the bytes out; keep the port OPEN so the pattern persists
	s_dvdoArmed = true;						// an 80 pattern leaves the TPG armed for subsequent AA patches
	msgOut.Format(LS(IDS_GEN_DVDO_TEST_OK),
		wasOpen ? LS(IDS_GEN_DVDO_REUSED_PORT) : LS(IDS_GEN_DVDO_OPENED_PORT),
		code, (patternCode > 0 ? _T("") : _T(" (off = full black)")), (LPCTSTR)comPort, ok ? _T("write OK") : _T("WRITE FAILED"));
	return ok;
}

// Apply the output format (command 61) live - used when the user changes the resolution
// in the DVDO settings and clicks OK. DvdoOpen's setup step sends 6C + 61 (with a settle);
// the port is left open (a resolution change persists; the next Init/Show reclaims it).
bool CGDIGenerator_DvdoApplyOutput(const CString& comPort, int colorSpace, int formatCode, CString& msgOut)
{
	CString fw;	// DvdoOpen reuses an already-open port (it does not close it); with sendSetup it re-sends 6C + 61
	if (!DvdoOpen(comPort, colorSpace, 0, formatCode, &fw, true /*send setup -> 6C + 61*/))
	{
		msgOut = s_dvdoDiag;
		return false;
	}
	FlushFileBuffers(s_dvdoPort.hComm);
	msgOut.Format(LS(IDS_GEN_DVDO_FMT_SET), formatCode, (LPCTSTR)comPort);
	return true;
}

// Live status readout for the DVDO panel (PGenerator/Murideo-style, tab-separated label\tvalue).
// Name/Firmware/Resolution are queried live from the device; colour space / format / range come
// from HCFR's settings (the device doesn't report colour space reliably - it negotiates it with
// the sink). csConfig: 0=RGB, 1=YCbCr444, 2=YCbCr422; lim = limited (16-235) range.
bool CGDIGenerator_DvdoQueryReadout(const CString& comPort, int csConfig, bool lim, CString& out)
{
	out.Empty();
	if (comPort.IsEmpty()) return false;
	bool wasOpen = s_dvdoOpen;
	if (!s_dvdoOpen) { CString fw; if (!DvdoOpen(comPort, csConfig, 0, 0 /*don't change format*/, &fw, false /*query only*/)) { out = s_dvdoDiag; return false; } }
	CString name, fwv, resv;
	DvdoQuery("A8", name);
	DvdoQuery("A9", fwv);
	bool haveRes = DvdoQuery("61", resv);
	int resStatus = haveRes ? _ttoi(resv) : -1;
	CString res = haveRes ? CString(DvdoResNameFromStatus(resStatus)) : CString(_T("?"));
	if (!wasOpen) DvdoClose();
	// At 4K/50 and 4K/60 the AVLab forces 4:2:0 chroma subsampling regardless of the RGB/YCbCr
	// setting (User's Guide 1.01: "output the color in RGB, YC 444, YC 422 except in 4K/60 where
	// it is 4:2:0"). Status values 10 = 4K50, 18 = 4K60. Report the actual output so the user
	// knows patches at those resolutions are subsampled (not ideal for colour measurement).
	bool forced420 = (resStatus == 10 || resStatus == 18);
	const TCHAR* fmt = forced420 ? _T("YCbCr 4:2:0 (forced)") : (csConfig == 0) ? _T("RGB") : _T("YCbCr");
	// The AVLab TPG is SDR / BT.709 only (no HDR, no BT.2020), so those are fixed.
	out  = CString(_T("Name\t"))         + (name.IsEmpty() ? _T("?") : (LPCTSTR)name) + _T("\r\n");
	out += CString(_T("COM port\t"))     + comPort + _T("\r\n");
	out += CString(_T("Firmware\t"))     + (fwv.IsEmpty() ? _T("?") : (LPCTSTR)fwv) + _T("\r\n");
	out += CString(_T("Dynamic range\t"))+ CString(_T("SDR")) + _T("\r\n");
	out += CString(_T("Resolution\t"))   + res + _T("\r\n");
	out += CString(_T("Color space\t"))  + CString(_T("BT.709")) + _T("\r\n");
	out += CString(_T("Color format\t")) + fmt + _T("\r\n");
	out += CString(_T("Signal range\t")) + (lim ? _T("Limited (16-235)") : _T("Full (0-255)"));
	return !name.IsEmpty() || haveRes;
}

// Output formats (command 61). Codes are DECIMAL, verified against the DVDO AVLab TPG
// User's Guide 1.01 Appendix A table (page 48-50) and the worked example
// "STX 3 0 6 6 1 NUL 1 3 NUL ETX -> 1080p60" (13). Value 11 is reserved/skipped.
struct DvdoFmtEntry { const char* name; int code; };
static const DvdoFmtEntry kDvdoFormats[] =
{
	{ "Auto",            0 },
	{ "480i",            19 },
	{ "480p",            3 },
	{ "576i",            20 },
	{ "576p",            4 },
	{ "720p50",          5 },
	{ "720p60",          6 },
	{ "1080i50",         7 },
	{ "1080i60",         8 },
	{ "1080p24",         9 },
	{ "1080p25",         10 },
	{ "1080p30",         18 },
	{ "1080p50",         12 },
	{ "1080p60",         13 },
	{ "VGA 60",          14 },
	{ "SVGA 60",         15 },
	{ "XGA 60",          16 },
	{ "SXGA 60",         17 },
	{ "4K24 (3840)",     21 },
	{ "4K24 (4096)",     22 },
	{ "4K25 (3840)",     23 },
	{ "4K30 (3840)",     24 },
	{ "4K50 (4:2:0)",    26 },
	{ "4K60 (4:2:0)",    25 },
};
static const int kDvdoFmtN = sizeof(kDvdoFormats) / sizeof(kDvdoFormats[0]);

int         CGDIGenerator_DvdoFmtCount()          { return kDvdoFmtN; }
const char* CGDIGenerator_DvdoFmtName(int i)      { return (i >= 0 && i < kDvdoFmtN) ? kDvdoFormats[i].name : ""; }
int         CGDIGenerator_DvdoFmtCode(int i)      { return (i >= 0 && i < kDvdoFmtN) ? kDvdoFormats[i].code : 0; }
int         CGDIGenerator_DvdoFmtIndexForCode(int code)
{
	for (int i = 0; i < kDvdoFmtN; ++i) if (kDvdoFormats[i].code == code) return i;
	return 0;	// default to Auto
}

// ===========================================================================
// Murideo Seven-G (DISPLAY_MURIDEO) control.
// PRIMARY transport = HTTP over the network, reverse-engineered from the unit's
// own web UI and VERIFIED against a real SEVEN-G:
//   single (timing/colour-space/...): GET /BtnSendCmd.CGI?button=HEX<catHex>NUM<id>
//   double (pattern):                 GET /AudSendCmd.CGI?button=HEX<catHex>NUM<a>BER<b>
//   IRE window (grayscale patch):     GET /BtnSendCmd.CGI?button=HEXFBNUM<size>IRE<level0-255>
// A trailing "+<cachebuster>" (URL-encoded space + number) mirrors the web UI.
// SECONDARY transport = serial (SENDSINGLE/SENDDOUBLE framing) - UNVERIFIED, kept
// as a best-effort fallback. Colour (non-grey) patches await the RGB-triplet
// command (from a ColourSpace capture).
// ===========================================================================
namespace {
	CSerialCom	s_muriPort;
	bool		s_muriOpen   = false;
	bool		s_muriUseNet = true;			// current transport (set by MuriConnect)
	CString		s_muriIp;
	int			s_muriTcpPort = 23;				// raw-TCP API port (config MuriTcpPort; device default = 23 / telnet)
	CString		s_muriDiag;
	const DWORD	MURI_BAUD = 115200;				// serial placeholder 8N1

	// ---- HTTP transport ----
	// One reusable session. DIRECT (not PRECONFIG) so we never trigger WPAD proxy
	// auto-detection - that's what made each request stall for tens of seconds. Short
	// timeouts keep a bad request from blocking the measurement loop.
	HINTERNET s_muriHInet = NULL;
	HINTERNET MuriInet()
	{
		if (!s_muriHInet)
		{
			s_muriHInet = InternetOpen(_T("HCFR"), INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
			if (s_muriHInet)
			{
				DWORD to = 3000;
				InternetSetOption(s_muriHInet, INTERNET_OPTION_CONNECT_TIMEOUT, &to, sizeof(to));
				InternetSetOption(s_muriHInet, INTERNET_OPTION_SEND_TIMEOUT,    &to, sizeof(to));
				InternetSetOption(s_muriHInet, INTERNET_OPTION_RECEIVE_TIMEOUT, &to, sizeof(to));
			}
		}
		return s_muriHInet;
	}

	bool MuriHttpGet(const char* cgi, const std::string& button)
	{
		if (s_muriIp.IsEmpty()) { s_muriDiag = LS(IDS_GEN_NO_IP_SET); return false; }
		HINTERNET hI = MuriInet();
		if (!hI) { s_muriDiag = LS(IDS_GEN_INTERNETOPEN_FAIL); return false; }
		CString url;
		url.Format(_T("http://%s/%s?button=%s+%u"), (LPCTSTR)s_muriIp, CString(cgi), CString(button.c_str()), (unsigned)GetTickCount());
		HINTERNET hU = InternetOpenUrl(hI, url, NULL, 0,
			INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_UI | INTERNET_FLAG_KEEP_CONNECTION, 0);
		DWORD status = 0, len = sizeof(status), idx = 0;
		bool ok = (hU != NULL);
		if (hU)
		{
			HttpQueryInfo(hU, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &len, &idx);
			// Drain the small response so the connection can be reused.
			char buf[256]; DWORD rd = 0; while (InternetReadFile(hU, buf, sizeof(buf), &rd) && rd) {}
			InternetCloseHandle(hU);
		}
		if (ok) s_muriDiag.Format(_T("HTTP %lu  %s"), (unsigned long)status, (LPCTSTR)url);
		else { DWORD e = GetLastError(); s_muriDiag.Format(LS(IDS_GEN_HTTP_GET_FAIL), (unsigned long)e, (LPCTSTR)url); }
		return ok;
	}

	// GET that returns the response body (for status queries like WebReq.CGI?button=VIDEOGEN).
	bool MuriHttpGetBody(const char* cgi, const std::string& button, CString& bodyOut)
	{
		bodyOut.Empty();
		if (s_muriIp.IsEmpty()) return false;
		HINTERNET hI = MuriInet(); if (!hI) return false;
		CString url;
		url.Format(_T("http://%s/%s?button=%s+%u"), (LPCTSTR)s_muriIp, CString(cgi), CString(button.c_str()), (unsigned)GetTickCount());
		HINTERNET hU = InternetOpenUrl(hI, url, NULL, 0,
			INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_UI | INTERNET_FLAG_KEEP_CONNECTION, 0);
		if (!hU) return false;
		std::string body; char buf[513]; DWORD rd = 0;
		while (InternetReadFile(hU, buf, sizeof(buf) - 1, &rd) && rd) { buf[rd] = 0; body += buf; }
		InternetCloseHandle(hU);
		bodyOut = CString(body.c_str());
		return true;
	}

	// Parse the VIDEOGEN status. The body has a first line
	//   VIDEOGEN=HEX61NUM34&HEX62NUM35&...&3840x2160@60Hz
	// (current state as HEX<cat>NUM<id> echoes + the resolution) followed by
	// human-readable lines (&HDMI &OFF &RGB(0-255) &8Bit &Disable &audio...).
	// We pick fields by CONTENT so field order doesn't matter: resolution (has 'x'
	// and '@'), colour space (contains "RGB"/"YC"), colour depth (ends in "Bit").
	bool MuriStatusSummary(CString& summaryOut)
	{
		CString body;
		if (!MuriHttpGetBody("WebReq.CGI", "VIDEOGEN", body) || body.IsEmpty()) { summaryOut.Empty(); return false; }
		CString res, cs, depth;
		int pos = 0, len = body.GetLength();
		while (pos < len)
		{
			int d = pos;
			while (d < len) { TCHAR c = body[d]; if (c == _T('&') || c == _T('\r') || c == _T('\n')) break; d++; }
			CString t = body.Mid(pos, d - pos); t.TrimLeft(); t.TrimRight();
			int eq = t.Find(_T('='));		// strip a "VIDEOGEN=" prefix on the first token
			if (eq >= 0 && t.Left(eq).CompareNoCase(_T("VIDEOGEN")) == 0) t = t.Mid(eq + 1);
			if (!t.IsEmpty())
			{
				if (res.IsEmpty()   && t.Find(_T('x')) > 0 && t.Find(_T('@')) > 0) res = t;			// e.g. 3840x2160@60Hz
				else if (cs.IsEmpty()    && (t.Find(_T("RGB")) >= 0 || t.Find(_T("YC")) >= 0)) cs = t;	// RGB(0-255) / YC 4:4:4...
				else if (depth.IsEmpty() && t.GetLength() >= 4 && t.Right(3).CompareNoCase(_T("Bit")) == 0) depth = t;	// 8Bit
			}
			pos = d + 1;
		}
		if (res.IsEmpty() && cs.IsEmpty() && depth.IsEmpty()) { summaryOut.Empty(); return false; }
		summaryOut.Empty();
		if (!res.IsEmpty())   summaryOut = res;
		if (!cs.IsEmpty())    { if (!summaryOut.IsEmpty()) summaryOut += _T(", "); summaryOut += cs; }
		if (!depth.IsEmpty()) { if (!summaryOut.IsEmpty()) summaryOut += _T(", "); summaryOut += depth; }
		return true;
	}

	// Multi-line labeled readout (tab-separated label\tvalue) like the PGenerator panel.
	bool MuriStatusReadout(CString& out)
	{
		CString body;
		if (!MuriHttpGetBody("WebReq.CGI", "VIDEOGEN", body) || body.IsEmpty()) { out.Empty(); return false; }
		CString res, cs, depth, output, hdcp, bt2020;
		int dynMode = -1;		// HDR mode from the HEX6f echo: 0=SDR, 1=HDR, 2=HLG
		int pos = 0, len = body.GetLength();
		while (pos < len)
		{
			int d = pos; while (d < len) { TCHAR c = body[d]; if (c == _T('&') || c == _T('\r') || c == _T('\n')) break; d++; }
			CString t = body.Mid(pos, d - pos); t.TrimLeft(); t.TrimRight();
			int eq = t.Find(_T('=')); if (eq >= 0 && t.Left(eq).CompareNoCase(_T("VIDEOGEN")) == 0) t = t.Mid(eq + 1);
			if (!t.IsEmpty())
			{
				if (dynMode < 0 && t.GetLength() >= 6 && t.Left(5).CompareNoCase(_T("HEX6F")) == 0)	// HDR mode command echo (cat 0x6F)
				{
					int np = t.Find(_T("NUM")); if (np < 0) np = t.Find(_T("num"));
					if (np >= 0) dynMode = _ttoi(t.Mid(np + 3));
				}
				else if (res.IsEmpty() && t.Find(_T('@')) >= 0) res = t;		// resolution is the only field with '@'
				else if (cs.IsEmpty() && (t.Find(_T("RGB")) >= 0 || t.Find(_T("YC")) >= 0)) cs = t;
				else if (depth.IsEmpty() && t.GetLength() >= 4 && t.Right(3).CompareNoCase(_T("Bit")) == 0) depth = t;
				else if (output.IsEmpty() && (t.CompareNoCase(_T("HDMI")) == 0 || t.CompareNoCase(_T("DVI")) == 0)) output = t;
				else if (hdcp.IsEmpty() && (t.CompareNoCase(_T("ON")) == 0 || t.CompareNoCase(_T("OFF")) == 0)) hdcp = t;
				else if (bt2020.IsEmpty() && (t.CompareNoCase(_T("Enable")) == 0 || t.CompareNoCase(_T("Disable")) == 0)) bt2020 = t;
			}
			pos = d + 1;
		}
		if (res.IsEmpty() && cs.IsEmpty()) { out.Empty(); return false; }
		CString fmt = (cs.Find(_T("RGB")) >= 0) ? _T("RGB") : (cs.Find(_T("YC")) >= 0 ? _T("YCbCr") : _T("?"));
		CString rng = (cs.Find(_T("0-255")) >= 0) ? _T("Full") : (cs.Find(_T("16-235")) >= 0 ? _T("Limited") : _T("?"));
		// Dynamic range comes from the HDR-mode field (HEX6f): 0=SDR, 1=HDR, 2=HLG.
		CString dyn = (dynMode == 0) ? _T("SDR") : (dynMode == 1) ? _T("HDR") : (dynMode == 2) ? _T("HLG") : _T("?");
		// Colour space = the gamut, from the BT.2020 field (Disable = BT.709, Enable = BT.2020).
		CString colorSpace = (bt2020.CompareNoCase(_T("Enable")) == 0) ? _T("BT.2020") : (bt2020.CompareNoCase(_T("Disable")) == 0) ? _T("BT.709") : _T("?");
		#define MURI_LINE(lbl,val) out += _T(lbl) _T("\t") + (val.IsEmpty()?CString(_T("?")):val) + _T("\r\n")
		out.Empty();
		out += CString(_T("Dynamic range\t")) + dyn + _T("\r\n");
		MURI_LINE("Resolution",   res);
		MURI_LINE("Bit depth",    depth);
		out += CString(_T("Color space\t"))  + colorSpace + _T("\r\n");
		out += CString(_T("Color format\t")) + fmt + _T("\r\n");
		out += CString(_T("Signal range\t")) + rng + _T("\r\n");
		MURI_LINE("Output",       output);
		out += CString(_T("HDCP\t")) + (hdcp.IsEmpty()?CString(_T("?")):hdcp);
		#undef MURI_LINE
		return true;
	}

	// ---- serial transport (binary UART protocol, per official API) ----
	bool MuriSerialOpen(const CString& comPort)
	{
		if (s_muriOpen) return true;
		if (comPort.IsEmpty()) { s_muriDiag = LS(IDS_GEN_NO_COM_SELECTED); return false; }
		if (!s_muriPort.OpenPort(comPort))
		{
			DWORD e = GetLastError();
			s_muriDiag.Format(LS(IDS_GEN_MURI_OPEN_FAIL), (LPCTSTR)comPort, (unsigned long)e);
			return false;
		}
		if (!s_muriPort.ConfigurePort(MURI_BAUD, 8, FALSE, NOPARITY, ONESTOPBIT) ||
		    !s_muriPort.SetCommunicationTimeouts(20, 0, 80, 0, 300))
		{
			// CSerialCom closed the handle on failure - leave s_muriOpen false, don't double-close.
			s_muriDiag.Format(LS(IDS_GEN_MURI_CONFIG_FAIL), (LPCTSTR)comPort, (unsigned long)MURI_BAUD);
			return false;
		}
		PurgeComm(s_muriPort.hComm, PURGE_RXCLEAR | PURGE_TXCLEAR);
		s_muriOpen = true;
		s_muriDiag.Format(LS(IDS_GEN_MURI_OPENED), (LPCTSTR)comPort, (unsigned long)MURI_BAUD);
		return true;
	}
	void MuriClose() { if (s_muriOpen) { FlushFileBuffers(s_muriPort.hComm); Sleep(40); s_muriPort.ClosePort(); s_muriOpen = false; } }

	// ---- binary UART protocol (official SEVEN-G UART API) --------------------
	// Frame:  AA 00 00 <LEN> 00 00 00 <KWlo> <KWhi> <data...> <CKSUM>
	//   LEN   = 5 + data length (3 reserved bytes + 2 keyword bytes + data)
	//   keyword is the command code, low byte first (0x008C=RGB triplet,
	//   0x0061=timing, 0x0062=pattern, 0x0063=colour space, 0x78FB=IRE window)
	//   CKSUM = (0x100 - sum(every byte AA..last data)) & 0xFF  (CheckSum8 2s-comp)
	// Verified against three worked examples in the manual (timing/colourspace,
	// IRE window, and the 15-byte RGB triplet). Header AA = PC->device.
	std::string MuriBuildFrame(int keyword, const std::vector<BYTE>& data)
	{
		std::vector<BYTE> f;
		f.push_back(0xAA); f.push_back(0x00); f.push_back(0x00);
		f.push_back((BYTE)(5 + data.size()));				// LEN
		f.push_back(0x00); f.push_back(0x00); f.push_back(0x00);
		f.push_back((BYTE)(keyword & 0xFF));				// keyword low byte first
		f.push_back((BYTE)((keyword >> 8) & 0xFF));
		f.insert(f.end(), data.begin(), data.end());
		int sum = 0; for (size_t i = 0; i < f.size(); ++i) sum += f[i];
		f.push_back((BYTE)((0x100 - (sum & 0xFF)) & 0xFF));	// 2s-complement checksum
		return std::string((const char*)&f[0], f.size());
	}

	bool MuriSerialWriteRaw(const std::string& bytes)
	{
		if (!s_muriOpen) return false;
		for (size_t i = 0; i < bytes.size(); ++i)
			if (!s_muriPort.WriteByte((BYTE)bytes[i])) return false;
		FlushFileBuffers(s_muriPort.hComm);
		// Same pacing as the TCP path: give the device time to process/render before the
		// next command or the sensor read. Tunable via Debug/MuriSendLingerMs.
		static DWORD lingerMs = (DWORD)GetConfig()->GetProfileInt("Debug", "MuriSendLingerMs", 300);
		Sleep(lingerMs);
		return true;
	}

	// Serial write + read reply (for read commands: EDID, status, connection test). Reads
	// until the stream stops (per the temporarily-widened read timeout) or wantBytes arrive.
	bool MuriSerialXfer(const std::string& sendBytes, std::string& replyOut, size_t wantBytes)
	{
		replyOut.clear();
		if (!s_muriOpen) return false;
		PurgeComm(s_muriPort.hComm, PURGE_RXCLEAR);
		for (size_t i = 0; i < sendBytes.size(); ++i)
			if (!s_muriPort.WriteByte((BYTE)sendBytes[i])) return false;
		FlushFileBuffers(s_muriPort.hComm);
		s_muriPort.SetCommunicationTimeouts(40, 0, 700, 0, 300);	// wait for a possibly-lagging reply
		BYTE b;
		while (replyOut.size() < wantBytes && s_muriPort.ReadByte(b))
			replyOut.push_back((char)b);
		s_muriPort.SetCommunicationTimeouts(20, 0, 80, 0, 300);	// restore send-oriented timeouts
		return !replyOut.empty();
	}

	// Raw-TCP send to the device's API port. Non-blocking connect with a 3s cap so a
	// wrong IP/port can't stall the measurement loop (same rationale as HTTP DIRECT).
	bool MuriTcpSendRaw(const std::string& bytes)
	{
		if (s_muriIp.IsEmpty()) { s_muriDiag = LS(IDS_GEN_NO_IP_SET); return false; }
		static bool wsaInit = false;
		if (!wsaInit) { WSADATA w; if (WSAStartup(MAKEWORD(2, 2), &w) != 0) { s_muriDiag = LS(IDS_GEN_WSASTARTUP_FAIL); return false; } wsaInit = true; }
		SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (s == INVALID_SOCKET) { s_muriDiag = LS(IDS_GEN_SOCKET_FAIL); return false; }
		sockaddr_in a; memset(&a, 0, sizeof(a));
		a.sin_family = AF_INET; a.sin_port = htons((u_short)s_muriTcpPort);
		a.sin_addr.s_addr = inet_addr((LPCSTR)CStringA(s_muriIp));
		u_long nb = 1; ioctlsocket(s, FIONBIO, &nb);				// non-blocking for timed connect
		connect(s, (sockaddr*)&a, sizeof(a));
		fd_set wf; FD_ZERO(&wf); FD_SET(s, &wf);
		timeval tv; tv.tv_sec = 3; tv.tv_usec = 0;
		bool ok = false;
		if (select(0, NULL, &wf, NULL, &tv) > 0)
		{
			int err = 0, el = sizeof(err);
			getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &el);
			if (err == 0)
			{
				nb = 0; ioctlsocket(s, FIONBIO, &nb);
				DWORD to = 3000;
				setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char*)&to, sizeof(to));
				int sent = send(s, bytes.data(), (int)bytes.size(), 0);
				ok = (sent == (int)bytes.size());
				if (ok)
				{
					// Keep the socket open a FIXED spell after sending, then close. The device
					// only applies the command if the connection lingers until it has read and
					// rendered the frame (verified on hardware): an immediate close truncates it.
					// A recv-based wait is unreliable - bytes arrive on port 23 at random times,
					// so recv returns early and the command is randomly dropped/late (seen as the
					// display lagging HCFR by 2-3 patches, differently each run). A fixed settle
					// both guarantees delivery and paces the loop so the read lands on a stable
					// display. Tunable via Debug/MuriSendLingerMs; read once.
					static DWORD lingerMs = (DWORD)GetConfig()->GetProfileInt("Debug", "MuriSendLingerMs", 300);
					Sleep(lingerMs);
				}
			}
		}
		if (ok) s_muriDiag.Format(LS(IDS_GEN_TCP_SENT), (int)bytes.size(), (LPCTSTR)s_muriIp, s_muriTcpPort);
		else    s_muriDiag.Format(LS(IDS_GEN_TCP_SEND_FAIL), (LPCTSTR)s_muriIp, s_muriTcpPort);
		closesocket(s);
		return ok;
	}

	// Like MuriTcpSendRaw but keeps the reply (for read commands such as EDID, which
	// answer with an 0xAB frame carrying the data). Non-blocking connect (3s), then drain
	// the reply until a short read-timeout or 'wantBytes' arrive.
	bool MuriTcpXfer(const std::string& sendBytes, std::string& replyOut, int readTimeoutMs, size_t wantBytes)
	{
		replyOut.clear();
		if (s_muriIp.IsEmpty()) { s_muriDiag = LS(IDS_GEN_NO_IP_SET); return false; }
		static bool wsaInit = false;
		if (!wsaInit) { WSADATA w; if (WSAStartup(MAKEWORD(2, 2), &w) != 0) { s_muriDiag = LS(IDS_GEN_WSASTARTUP_FAIL); return false; } wsaInit = true; }
		SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (s == INVALID_SOCKET) { s_muriDiag = LS(IDS_GEN_SOCKET_FAIL); return false; }
		sockaddr_in a; memset(&a, 0, sizeof(a));
		a.sin_family = AF_INET; a.sin_port = htons((u_short)s_muriTcpPort);
		a.sin_addr.s_addr = inet_addr((LPCSTR)CStringA(s_muriIp));
		u_long nb = 1; ioctlsocket(s, FIONBIO, &nb);
		connect(s, (sockaddr*)&a, sizeof(a));
		fd_set wf; FD_ZERO(&wf); FD_SET(s, &wf);
		timeval tv; tv.tv_sec = 3; tv.tv_usec = 0;
		bool ok = false;
		if (select(0, NULL, &wf, NULL, &tv) > 0)
		{
			int err = 0, el = sizeof(err);
			getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &el);
			if (err == 0)
			{
				nb = 0; ioctlsocket(s, FIONBIO, &nb);
				DWORD to = 3000; setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char*)&to, sizeof(to));
				if (send(s, sendBytes.data(), (int)sendBytes.size(), 0) == (int)sendBytes.size())
				{
					DWORD rto = (DWORD)readTimeoutMs; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&rto, sizeof(rto));
					char rb[600];
					for (;;)
					{
						int n = recv(s, rb, sizeof(rb), 0);
						if (n <= 0) break;
						replyOut.append(rb, n);
						if (replyOut.size() >= wantBytes) break;
					}
					ok = !replyOut.empty();
				}
			}
		}
		closesocket(s);
		return ok;
	}

	// Read the connected sink's EDID (keyword 0xB838). The reply embeds the raw EDID,
	// which always starts with the fixed header 00 FF FF FF FF FF FF 00 - locate that and
	// take up to 256 bytes (base block + first extension). Network transport only.
	bool MuriReadEdidBytes(std::vector<BYTE>& out)
	{
		out.clear();
		std::string frame = MuriBuildFrame(0xB838, std::vector<BYTE>(1, 0x01)), reply;
		bool got = s_muriUseNet ? MuriTcpXfer(frame, reply, 2000, 300) : MuriSerialXfer(frame, reply, 400);
		if (!got) { s_muriDiag = LS(IDS_GEN_NO_REPLY); return false; }
		static const BYTE hdr[8] = { 0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00 };
		size_t idx = std::string::npos;
		for (size_t i = 0; i + 8 <= reply.size(); ++i)
		{
			bool m = true; for (int j = 0; j < 8; ++j) if ((BYTE)reply[i + j] != hdr[j]) { m = false; break; }
			if (m) { idx = i; break; }
		}
		if (idx == std::string::npos) { s_muriDiag = LS(IDS_GEN_NO_EDID); return false; }
		size_t n = min((size_t)256, reply.size() - idx);
		out.assign((const BYTE*)reply.data() + idx, (const BYTE*)reply.data() + idx + n);
		return out.size() >= 128;
	}

	// VIC (CTA-861) -> readable "WxH{p|i} @ R Hz  AR". Unknown VICs print as "VIC n".
	CString MuriVicStr(int vic)
	{
		static const struct { int vic, w, h; char prog; int hz; const char* ar; } t[] = {
			{1,640,480,'p',60,"4:3"},{2,720,480,'p',60,"4:3"},{3,720,480,'p',60,"16:9"},{4,1280,720,'p',60,"16:9"},
			{5,1920,1080,'i',60,"16:9"},{16,1920,1080,'p',60,"16:9"},{17,720,576,'p',50,"4:3"},{18,720,576,'p',50,"16:9"},
			{19,1280,720,'p',50,"16:9"},{20,1920,1080,'i',50,"16:9"},{31,1920,1080,'p',50,"16:9"},{32,1920,1080,'p',24,"16:9"},
			{33,1920,1080,'p',25,"16:9"},{34,1920,1080,'p',30,"16:9"},{93,3840,2160,'p',24,"16:9"},{94,3840,2160,'p',25,"16:9"},
			{95,3840,2160,'p',30,"16:9"},{96,3840,2160,'p',50,"16:9"},{97,3840,2160,'p',60,"16:9"},
		};
		for (int i = 0; i < (int)(sizeof(t) / sizeof(t[0])); ++i)
			if (t[i].vic == vic)
			{
				CString ar(t[i].ar), s;
				s.Format(_T("%dx%d%s @ %d Hz  %s"), t[i].w, t[i].h, t[i].prog == 'i' ? _T("i") : _T("p"), t[i].hz, (LPCTSTR)ar);
				return s;
			}
		CString s; s.Format(_T("VIC %d"), vic); return s;
	}

	// 18-byte Detailed Timing Descriptor -> "WxH @ R Hz"; false if it's a monitor descriptor (pclk 0).
	bool MuriDtdStr(const BYTE* d, CString& out)
	{
		int pclk = (d[0] | (d[1] << 8)) * 10;	// kHz
		if (pclk == 0) return false;
		int ha = d[2] | ((d[4] >> 4) << 8), hb = d[3] | ((d[4] & 0xF) << 8);
		int va = d[5] | ((d[7] >> 4) << 8), vb = d[6] | ((d[7] & 0xF) << 8);
		int ht = ha + hb, vt = va + vb; bool il = (d[17] & 0x80) != 0;
		double hz = (ht && vt) ? (double)pclk * 1000.0 / ((double)ht * vt) : 0;
		if (il) hz *= 2;
		out.Format(_T("%dx%d @ %d Hz"), ha, va, (int)(hz + 0.5));
		return true;
	}

	// Decode the sink EDID into a full General / Video / Audio report (Murideo web-UI parity).
	CString MuriParseEdid(const std::vector<BYTE>& e)
	{
		CString s;
		int v = (e[8] << 8) | e[9];
		TCHAR mfr[4] = { (TCHAR)(((v >> 10) & 31) + 64), (TCHAR)(((v >> 5) & 31) + 64), (TCHAR)((v & 31) + 64), 0 };
		int ver = e[18], rev = e[19];
		CString name;
		for (int off = 54; off <= 108; off += 18)
			if (e[off] == 0 && e[off + 1] == 0 && e[off + 2] == 0 && e[off + 3] == 0xFC && name.IsEmpty())
			{
				char nm[16]; int k = 0;
				for (int j = 5; j < 18; ++j) { char c = (char)e[off + j]; if (c == '\n' || c == 0) break; nm[k++] = c; }
				nm[k] = 0; name = CString(nm); name.TrimRight();
			}
		s += _T("=== GENERAL ===\r\n");
		s.AppendFormat(_T("Manufacturer          %s\r\n"), mfr);
		s.AppendFormat(_T("Product Code          %d\r\n"), e[10] | (e[11] << 8));
		s.AppendFormat(_T("Display Product Name  %s\r\n"), name.IsEmpty() ? _T("-") : (LPCTSTR)name);
		s.AppendFormat(_T("Video Interface       %s\r\n"), (e[20] & 0x80) ? _T("Digital") : _T("Analog"));
		if ((ver > 1 || rev >= 4) && (e[20] & 0x80))
		{
			static const TCHAR* dd[] = { _T("Reserved"),_T("6"),_T("8"),_T("10"),_T("12"),_T("14"),_T("16"),_T("?") };
			s.AppendFormat(_T("Color Bit Depth       %s bpc\r\n"), dd[(e[20] >> 4) & 7]);
		}
		else
			s.AppendFormat(_T("Color Bit Depth       Reserved (EDID %d.%d)\r\n"), ver, rev);
		for (int off = 54; off <= 108; off += 18)
			if (e[off] == 0 && e[off + 1] == 0 && e[off + 2] == 0 && e[off + 3] == 0xFD)
			{
				s.AppendFormat(_T("Vertical Rate         %d-%d Hz\r\n"), e[off + 5], e[off + 6]);
				s.AppendFormat(_T("Horizontal Rate       %d-%d kHz\r\n"), e[off + 7], e[off + 8]);
				s.AppendFormat(_T("Maximum Pixel Clock   %d MHz\r\n"), e[off + 9] * 10);
			}

		std::vector<int> svdVic, svdNat;
		std::vector<CString> hdmi4k, y420, extDtds, sads;
		CString extSupp, deepColor, hdr, colorim, three_d = _T("not supported");
		int nativeCount = 0, dtdOff = 0;
		if (e.size() >= 256 && e[128] == 0x02)		// CEA-861 extension
		{
			const BYTE* x = &e[128];
			dtdOff = x[2];
			if (dtdOff > 127) dtdOff = 127;		// offset within the 128-byte extension; a bogus
												// device value must not push either walk out of bounds
			nativeCount = x[3] & 0x0F;
			if (x[3] & 0x40) extSupp += _T("Base Audio, ");
			if (x[3] & 0x20) extSupp += _T("YCbCr 4:4:4, ");
			if (x[3] & 0x10) extSupp += _T("YCbCr 4:2:2, ");
			int i = 4;
			while (i < dtdOff && i < 128)
			{
				int tag = (x[i] >> 5) & 7, ln = x[i] & 0x1F; const BYTE* body = &x[i + 1];
				if (i + 1 + ln > 128) break;	// block body would run past the 128-byte extension
												// (device-supplied len); stop rather than read e[256+]
				if (tag == 2)			// Short Video Descriptors
				{
					for (int k = 0; k < ln; ++k) { int b = body[k]; if (b >= 129 && b <= 192) { svdVic.push_back(b - 128); svdNat.push_back(1); } else { svdVic.push_back(b & 0x7F); svdNat.push_back(0); } }
				}
				else if (tag == 1)		// Short Audio Descriptors
				{
					for (int k = 0; k + 3 <= ln; k += 3)
					{
						int fmt = (body[k] >> 3) & 0xF, ch = (body[k] & 7) + 1;
						static const TCHAR* fn[] = { _T("?"),_T("LPCM"),_T("AC-3"),_T("MPEG-1"),_T("MP3"),_T("MPEG-2"),_T("AAC"),_T("DTS"),_T("ATRAC"),_T("DSD"),_T("E-AC-3"),_T("DTS-HD"),_T("MLP/TrueHD"),_T("DST"),_T("WMA Pro") };
						CString rates; const int rm[] = { 0x40,0x20,0x10,0x08,0x04,0x02,0x01 }; const TCHAR* rn[] = { _T("192"),_T("176.4"),_T("96"),_T("88.2"),_T("48"),_T("44.1"),_T("32") };
						for (int r = 0; r < 7; ++r) if (body[k + 1] & rm[r]) { if (!rates.IsEmpty()) rates += _T(", "); rates += rn[r]; }
						CString sad;
						sad.AppendFormat(_T("Format                %s\r\n"), (fmt >= 0 && fmt <= 14) ? fn[fmt] : _T("?"));
						sad.AppendFormat(_T("Channels              %d\r\n"), ch);
						sad.AppendFormat(_T("Sample Rates          %s kHz\r\n"), (LPCTSTR)rates);
						if (fmt == 1)
						{
							CString bits; const int bmk[] = { 0x04,0x02,0x01 }; const TCHAR* bn[] = { _T("24"),_T("20"),_T("16") };
							for (int b2 = 0; b2 < 3; ++b2) if (body[k + 2] & bmk[b2]) { if (!bits.IsEmpty()) bits += _T(", "); bits += bn[b2]; }
							sad.AppendFormat(_T("Sample Bits           %s bit\r\n"), (LPCTSTR)bits);
						}
						sads.push_back(sad);
					}
				}
				else if (tag == 3 && ln >= 6 && body[0] == 0x03 && body[1] == 0x0C && body[2] == 0x00)	// HDMI VSDB
				{
					if (body[5] & 0x20) deepColor += _T("36-bit, ");
					if (body[5] & 0x10) deepColor += _T("30-bit, ");
					if (body[5] & 0x08) deepColor += _T("YCbCr444, ");
					if (ln >= 8)
					{
						int lat = body[7], idx = 8;
						if (lat & 0x80) idx += 2;
						if (lat & 0x40) idx += 2;
						if ((lat & 0x20) && ln > idx + 1)		// HDMI_Video_present
						{
							if (body[idx] & 0x80) three_d = _T("supported");
							int hvl = (body[idx + 1] >> 5) & 7; idx += 2;
							static const TCHAR* HV[] = { _T("-"),_T("3840x2160 @ 30 Hz"),_T("3840x2160 @ 25 Hz"),_T("3840x2160 @ 24 Hz"),_T("4096x2160 @ 24 Hz") };
							for (int k = 0; k < hvl && idx + k < ln; ++k)
							{
								int hv = body[idx + k];
								if (hv >= 1 && hv <= 4) hdmi4k.push_back(CString(HV[hv]));
								else { CString t; t.Format(_T("HDMI_VIC %d"), hv); hdmi4k.push_back(t); }
							}
						}
					}
				}
				else if (tag == 7 && ln >= 2)		// extended tag blocks
				{
					int et = body[0];
					if (et == 5)
					{
						if (body[1] & 0x80) colorim += _T("BT.2020 RGB, ");
						if (body[1] & 0x40) colorim += _T("BT.2020 YCC, ");
						if (body[1] & 0x20) colorim += _T("BT.2020 cYCC, ");
					}
					else if (et == 6)
					{
						if (body[1] & 0x01) hdr += _T("SDR, ");
						if (body[1] & 0x02) hdr += _T("HDR, ");
						if (body[1] & 0x04) hdr += _T("HDR10/PQ, ");
						if (body[1] & 0x08) hdr += _T("HLG, ");
					}
					else if (et == 14)		// Y420VDB: direct 4:2:0-only VICs
						for (int k = 1; k < ln; ++k) y420.push_back(MuriVicStr(body[k] & 0x7F));
					else if (et == 15)		// Y420CMDB: bitmap over the SVD list
					{
						if (ln <= 1) { for (size_t k = 0; k < svdVic.size(); ++k) y420.push_back(MuriVicStr(svdVic[k])); }
						else for (int bi = 1; bi < ln; ++bi) for (int bit = 0; bit < 8; ++bit) if (body[bi] & (1 << bit)) { size_t si = (size_t)(bi - 1) * 8 + bit; if (si < svdVic.size()) y420.push_back(MuriVicStr(svdVic[si])); }
					}
				}
				i += 1 + ln;
			}
			// dtdOff == 0 means "no DTDs" in CEA-861; only walk when it is a real offset (>= 4),
			// otherwise the loop would parse the extension header (x[0]=0x02,x[1]=0x03) as a DTD.
			if (dtdOff >= 4)
				for (int j = dtdOff; j + 18 <= 128 && (x[j] || x[j + 1]); j += 18)	// extension DTDs
				{
					CString d; if (MuriDtdStr(&x[j], d)) extDtds.push_back(d);
				}
		}
		#define MURI_TRIM2(cs) if ((cs).GetLength() >= 2 && (cs).Right(2) == _T(", ")) (cs) = (cs).Left((cs).GetLength() - 2)
		MURI_TRIM2(extSupp); MURI_TRIM2(deepColor); MURI_TRIM2(hdr); MURI_TRIM2(colorim);
		#undef MURI_TRIM2
		s.AppendFormat(_T("Native Timings        %d\r\n"), nativeCount);
		s.AppendFormat(_T("Extension Support     %s\r\n"), extSupp.IsEmpty() ? _T("-") : (LPCTSTR)extSupp);
		s.AppendFormat(_T("Deep Color            %s\r\n"), deepColor.IsEmpty() ? _T("-") : (LPCTSTR)deepColor);
		s.AppendFormat(_T("3D Video              %s\r\n"), (LPCTSTR)three_d);
		s.AppendFormat(_T("HDR                   %s\r\n"), hdr.IsEmpty() ? _T("not supported") : (LPCTSTR)(CString(_T("Supported (")) + hdr + _T(")")));
		s.AppendFormat(_T("Colorimetry           %s\r\n"), colorim.IsEmpty() ? _T("-") : (LPCTSTR)colorim);
		if (!hdmi4k.empty()) { s += _T("Extended Resolution (4Kx2K)\r\n"); for (size_t k = 0; k < hdmi4k.size(); ++k) s += _T("  ") + hdmi4k[k] + _T("\r\n"); }
		if (!y420.empty())   { s += _T("YCbCr 4:2:0 (Y420CMDB)\r\n"); for (size_t k = 0; k < y420.size(); ++k) s += _T("  ") + y420[k] + _T("\r\n"); }

		s += _T("\r\n=== VIDEO ===\r\n");
		const TCHAR* dl[] = { _T("Preferred Timing      "), _T("Detailed Timing       ") };
		int di = 0;
		for (int off = 54; off <= 72; off += 18)
		{
			CString d; if (MuriDtdStr(&e[off], d) && di < 2) { s.AppendFormat(_T("%s%s\r\n"), dl[di], (LPCTSTR)d); di++; }
		}
		for (size_t k = 0; k < extDtds.size(); ++k) s.AppendFormat(_T("Ext Detailed Timing %d  %s\r\n"), (int)k + 1, (LPCTSTR)extDtds[k]);
		if (!svdVic.empty())
		{
			s += _T("Short Video Descriptors\r\n");
			for (size_t k = 0; k < svdVic.size(); ++k) s += _T("  ") + MuriVicStr(svdVic[k]) + (svdNat[k] ? CString(_T("  Native")) : CString(_T(""))) + _T("\r\n");
		}

		s += _T("\r\n=== AUDIO ===\r\n");
		if (sads.empty()) s += _T("(none)\r\n");
		else for (size_t k = 0; k < sads.size(); ++k) { if (k) s += _T("\r\n"); s += sads[k]; }
		return s;
	}

	// Send a binary frame over the active transport (serial now; raw-TCP on network).
	bool MuriSendFrame(int keyword, const std::vector<BYTE>& data)
	{
		std::string f = MuriBuildFrame(keyword, data);
		return s_muriUseNet ? MuriTcpSendRaw(f) : MuriSerialWriteRaw(f);
	}

	// 0x008C "10/12bit RGB triplet": foreground + background RGB (16-bit little-endian
	// each), window size %, colour depth (0=8/1=10/2=12 bit), colour space (0=RGB full,
	// 1=RGB limited). This is the arbitrary-colour patch command for calibration.
	bool MuriCmdRgbTriplet(int r, int g, int b, int bgR, int bgG, int bgB, int size, int depth, int csFullLim)
	{
		std::vector<BYTE> d;
		const int v[6] = { r, g, b, bgR, bgG, bgB };
		for (int i = 0; i < 6; ++i) { d.push_back((BYTE)(v[i] & 0xFF)); d.push_back((BYTE)((v[i] >> 8) & 0xFF)); }
		d.push_back((BYTE)size); d.push_back((BYTE)depth); d.push_back((BYTE)csFullLim);
		return MuriSendFrame(0x008C, d);
	}

	// ---- transport-routed commands ----
	// Network path uses the proven HTTP CGI; serial uses the binary UART frame.
	// The 'cat' value is the command keyword (97=0x61 timing, 99=0x63 colourspace, ...).
	bool MuriCmdSingle(int cat, int id)
	{
		if (s_muriUseNet) { char b[64]; sprintf(b, "HEX%02XNUM%d", cat, id); return MuriHttpGet("BtnSendCmd.CGI", b); }
		return MuriSerialWriteRaw(MuriBuildFrame(cat, std::vector<BYTE>(1, (BYTE)id)));
	}
	// Pattern (cat 98=0x62): the 2-byte pattern id is (a | b<<8) - a is the HTTP NUM
	// (low byte), b the HTTP BER (high byte). Both encodings resolve to the same id.
	bool MuriCmdDouble(int cat, int a, int b2)
	{
		if (s_muriUseNet) { char b[64]; sprintf(b, "HEX%02XNUM%dBER%d", cat, a, b2); return MuriHttpGet("AudSendCmd.CGI", b); }
		// Serial carries the pattern as one little-endian 2-byte id = num + bank*256.
		// A bare (BYTE)a would truncate the bank-0 UHD SDR ids 256-347; the bank-1
		// groups store num<256 so [num,bank] already equalled the LE full id - only
		// bank-0 ids above 255 spill into the high byte and were being lost.
		int fullId = a + (b2 << 8);
		std::vector<BYTE> d; d.push_back((BYTE)(fullId & 0xFF)); d.push_back((BYTE)((fullId >> 8) & 0xFF));
		return MuriSerialWriteRaw(MuriBuildFrame(cat, d));
	}
	// IRE window (keyword 0x78FB): grayscale box at level 0-255, size %. HTTP maps it as
	// HEXFBNUM<size>IRE<level>; the binary frame data is [level, size].
	// Select the transport for subsequent commands. For serial, opens the port.
	bool MuriConnect(bool useNet, const CString& ip, const CString& com)
	{
		s_muriUseNet = useNet; s_muriIp = ip;
		if (useNet)
		{
			if (ip.IsEmpty()) { s_muriDiag = LS(IDS_GEN_NO_IP_SET); return false; }
			s_muriDiag.Format(LS(IDS_GEN_NETWORK_MODE), (LPCTSTR)ip);
			return true;		// HTTP is connectionless; nothing to open
		}
		return MuriSerialOpen(com);
	}
	void MuriDisconnect() { MuriClose(); }
	void MuriSetTcpPort(int port) { if (port > 0 && port < 65536) s_muriTcpPort = port; }
}

// --- Preset tables (community 8K codes; unverified for RS-232 SEVEN-G) --------
struct MuriItem { const char* group; const char* name; int id; };

static const MuriItem kMuriTimings[] =
{
	{ "HD",  "720p 60Hz",       12 }, { "HD",  "720p 59.94Hz",    13 }, { "HD",  "1080i 60Hz",      14 },
	{ "HD",  "1080i 59.94Hz",   15 }, { "HD",  "1080p 30Hz",      16 }, { "HD",  "1080p 29.97Hz",   17 },
	{ "HD",  "1080p 24Hz",      18 }, { "HD",  "1080p 23.976Hz",  19 }, { "HD",  "1080p 60Hz",      20 },
	{ "HD",  "1080p 59.94Hz",   21 }, { "HD",  "720p 50Hz",       24 }, { "HD",  "1080i 50Hz",      25 },
	{ "HD",  "1080p 25Hz",      26 },
	{ "UHD", "2160p 30Hz",      28 }, { "UHD", "2160p 29.97Hz",   29 }, { "UHD", "2160p 25Hz",      30 },
	{ "UHD", "2160p 24Hz",      31 }, { "UHD", "2160p 23.98Hz",   32 }, { "UHD", "2160p 60Hz",      34 },
	{ "UHD", "2160p 59.94Hz",   35 }, { "UHD", "2160p 50Hz",      36 }, { "UHD", "2160p 48Hz",     103 },
	{ "UHD", "2160p 47.95Hz",  104 }, { "UHD", "2160p 100Hz",    107 }, { "UHD", "2160p 120Hz",    108 },
	{ "UHD", "2160p 119.88Hz", 109 },
	{ "4K-DCI", "4096x2160 30Hz",     53 }, { "4K-DCI", "4096x2160 29.97Hz",  54 }, { "4K-DCI", "4096x2160 25Hz",     55 },
	{ "4K-DCI", "4096x2160 24Hz",     44 }, { "4K-DCI", "4096x2160 23.976Hz", 56 }, { "4K-DCI", "4096x2160 60Hz",     57 },
	{ "4K-DCI", "4096x2160 59.94Hz",  58 }, { "4K-DCI", "4096x2160 50Hz",     59 },
	{ "2K-DCI", "2048x1080 30Hz",     73 }, { "2K-DCI", "2048x1080 24Hz",     76 }, { "2K-DCI", "2048x1080 60Hz",     78 },
	{ "2K-DCI", "2048x1080 50Hz",     80 },
	{ "8K",  "7680x4320 30Hz",  110 }, { "8K",  "7680x4320 24Hz",  113 }, { "8K",  "7680x4320 60Hz",  115 },
	{ "8K",  "7680x4320 50Hz",  117 },
};
static const int kMuriTimingN = sizeof(kMuriTimings) / sizeof(kMuriTimings[0]);

// Patterns carry a bank (BER): 0 = built-in video banks (FPGA/ISF/DVS/UHD SDR/HD/PVA/
// SPE, ids match the community codes), 1 = stills banks (Spears & Munsil at NUM 200+,
// hardware-anchored). Ids for the higher video groups come from the 8K reference and
// are UNVERIFIED on the SEVEN-G HTTP (FPGA/ISF/DVS-HDR confirmed).
struct MuriPat { const char* group; const char* name; int id; int ber; };
static const MuriPat kMuriPatterns[] =
{
#include "MuriPatterns.inc"
};
static const int kMuriPatternN = sizeof(kMuriPatterns) / sizeof(kMuriPatterns[0]);

struct MuriCs { const char* name; int id; };
static const MuriCs kMuriColorSpaces[] =
{
	{ "RGB (0-255)", 0 }, { "RGB (16-235)", 1 }, { "YC 4:4:4 (16-235)", 2 },
	{ "YC 4:2:2 (16-235)", 3 }, { "YC 4:2:0 (16-235)", 4 },
};
static const int kMuriCsN = sizeof(kMuriColorSpaces) / sizeof(kMuriColorSpaces[0]);

// Generic group/item helpers over a MuriItem table (mirrors the DVDO pattern accessors).
template<class T> static int MuriGroups(const T* t, int n, const char* out[], int maxN)
{
	int c = 0;
	for (int i = 0; i < n && c < maxN; ++i)
	{
		bool seen = false;
		for (int j = 0; j < c; ++j) if (strcmp(out[j], t[i].group) == 0) { seen = true; break; }
		if (!seen) out[c++] = t[i].group;
	}
	return c;
}
template<class T> static int MuriItemsInGroup(const T* t, int n, int gi, int idxOut[], int maxN)
{
	const char* g[24]; int ng = MuriGroups(t, n, g, 24);
	if (gi < 0 || gi >= ng) return 0;
	int c = 0;
	for (int i = 0; i < n && c < maxN; ++i) if (strcmp(t[i].group, g[gi]) == 0) idxOut[c++] = i;
	return c;
}

// --- Externs for the prop page ------------------------------------------------
int         CGDIGenerator_MuriTimingGroups()          { const char* g[16]; return MuriGroups(kMuriTimings, kMuriTimingN, g, 16); }
const char* CGDIGenerator_MuriTimingGroupName(int gi) { const char* g[16]; int n = MuriGroups(kMuriTimings, kMuriTimingN, g, 16); return (gi>=0&&gi<n)?g[gi]:""; }
int         CGDIGenerator_MuriTimingCount(int gi)     { int idx[80]; return MuriItemsInGroup(kMuriTimings, kMuriTimingN, gi, idx, 80); }
const char* CGDIGenerator_MuriTimingName(int gi,int i){ int idx[80]; int n=MuriItemsInGroup(kMuriTimings,kMuriTimingN,gi,idx,80); return (i>=0&&i<n)?kMuriTimings[idx[i]].name:""; }
int         CGDIGenerator_MuriTimingId(int gi,int i)  { int idx[80]; int n=MuriItemsInGroup(kMuriTimings,kMuriTimingN,gi,idx,80); return (i>=0&&i<n)?kMuriTimings[idx[i]].id:-1; }
bool        CGDIGenerator_MuriFindTiming(int id,int& gi,int& ii)
{
	const char* g[16]; int ng=MuriGroups(kMuriTimings,kMuriTimingN,g,16);
	for (int a=0;a<ng;++a){int idx[80];int n=MuriItemsInGroup(kMuriTimings,kMuriTimingN,a,idx,80);for(int b=0;b<n;++b)if(kMuriTimings[idx[b]].id==id){gi=a;ii=b;return true;}}
	return false;
}

int         CGDIGenerator_MuriPatGroups()          { const char* g[24]; return MuriGroups(kMuriPatterns, kMuriPatternN, g, 24); }
const char* CGDIGenerator_MuriPatGroupName(int gi) { const char* g[24]; int n = MuriGroups(kMuriPatterns, kMuriPatternN, g, 24); return (gi>=0&&gi<n)?g[gi]:""; }
int         CGDIGenerator_MuriPatCount(int gi)     { int idx[256]; return MuriItemsInGroup(kMuriPatterns, kMuriPatternN, gi, idx, 256); }
const char* CGDIGenerator_MuriPatName(int gi,int i){ int idx[256]; int n=MuriItemsInGroup(kMuriPatterns,kMuriPatternN,gi,idx,256); return (i>=0&&i<n)?kMuriPatterns[idx[i]].name:""; }
int         CGDIGenerator_MuriPatId(int gi,int i)  { int idx[256]; int n=MuriItemsInGroup(kMuriPatterns,kMuriPatternN,gi,idx,256); return (i>=0&&i<n)?kMuriPatterns[idx[i]].id:-1; }
int         CGDIGenerator_MuriPatBer(int gi,int i) { int idx[256]; int n=MuriItemsInGroup(kMuriPatterns,kMuriPatternN,gi,idx,256); return (i>=0&&i<n)?kMuriPatterns[idx[i]].ber:0; }
bool        CGDIGenerator_MuriFindPat(int id,int ber,int& gi,int& ii)
{
	// Match id AND bank: 111 of the 459 ids collide across bank 0 / bank 1, so id alone would
	// resolve to whichever bank sorts first and restore the wrong group/pattern.
	const char* g[24]; int ng=MuriGroups(kMuriPatterns,kMuriPatternN,g,24);
	for (int a=0;a<ng;++a){int idx[256];int n=MuriItemsInGroup(kMuriPatterns,kMuriPatternN,a,idx,256);for(int b=0;b<n;++b)if(kMuriPatterns[idx[b]].id==id&&kMuriPatterns[idx[b]].ber==ber){gi=a;ii=b;return true;}}
	return false;
}

int         CGDIGenerator_MuriCsCount()       { return kMuriCsN; }
const char* CGDIGenerator_MuriCsName(int i)   { return (i>=0&&i<kMuriCsN)?kMuriColorSpaces[i].name:""; }
int         CGDIGenerator_MuriCsId(int i)     { return (i>=0&&i<kMuriCsN)?kMuriColorSpaces[i].id:0; }
int         CGDIGenerator_MuriCsIndexForId(int id) { for(int i=0;i<kMuriCsN;++i) if(kMuriColorSpaces[i].id==id) return i; return 0; }

// Detect/Test: for network, GET the device root and report the HTTP status; for
// serial, open the port and report.
bool CGDIGenerator_MuriTestConnection(bool useNet, const CString& ip, const CString& comPort, CString& msgOut)
{
	if (useNet)
	{
		s_muriUseNet = true; s_muriIp = ip;
		if (ip.IsEmpty()) { msgOut = LS(IDS_GEN_ENTER_MURI_IP_FIRST); return false; }
		HINTERNET hI = MuriInet();
		CString url; url.Format(_T("http://%s/"), (LPCTSTR)ip);
		HINTERNET hU = hI ? InternetOpenUrl(hI, url, NULL, 0, INTERNET_FLAG_NO_UI | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0) : NULL;
		DWORD st = 0, len = sizeof(st), idx = 0; bool ok = (hU != NULL);
		if (hU) { HttpQueryInfo(hU, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &st, &len, &idx); InternetCloseHandle(hU); }
		if (ok)
		{
			CString sum;
			if (MuriStatusSummary(sum) && !sum.IsEmpty())
				msgOut.Format(LS(IDS_GEN_MURI_CONNECTED_NET), (LPCTSTR)ip, (LPCTSTR)sum);
			else
				msgOut.Format(LS(IDS_GEN_MURI_REACHED_NOSTATUS), (LPCTSTR)ip, (unsigned long)st);
		}
		else { DWORD e = GetLastError(); msgOut.Format(LS(IDS_GEN_MURI_UNREACHABLE), (LPCTSTR)ip, (unsigned long)e); }
		return ok;
	}
	// Serial: open the port, then round-trip a read command (0x8061 read-timing-status)
	// and look for the device's 0xAB reply to confirm it's really the Murideo on this port.
	if (!MuriConnect(false, ip, comPort)) { msgOut = s_muriDiag; return false; }
	std::string reply;
	bool got = MuriSerialXfer(MuriBuildFrame(0x8061, std::vector<BYTE>()), reply, 32);
	bool isMuri = false;
	for (size_t k = 0; k < reply.size(); ++k) if ((BYTE)reply[k] == 0xAB) { isMuri = true; break; }
	if (isMuri)      msgOut.Format(LS(IDS_GEN_MURI_CONNECTED_SERIAL), (LPCTSTR)comPort, (int)reply.size());
	else if (got)    msgOut.Format(LS(IDS_GEN_MURI_WRONG_DEVICE), (LPCTSTR)comPort, (int)reply.size());
	else             msgOut.Format(LS(IDS_GEN_MURI_NO_RESPOND), (LPCTSTR)comPort);
	MuriClose();
	return isMuri;
}

// Apply output settings. Each is skipped when < 0: timing (cat 97), colour space
// (cat 99), BT.2020 gamut (cat 112: 0=BT.709,1=BT.2020), HDR mode (cat 0x6F:
// 0=SDR,1=HDR,2=HLG), colour depth (cat 100: 0=8bit,1=10bit).
bool CGDIGenerator_MuriApplyOutput(bool useNet, const CString& ip, const CString& comPort,
	int timingId, int csId, int bt2020, int hdrMode, int bitDepth, CString& msgOut)
{
	if (!MuriConnect(useNet, ip, comPort)) { msgOut = s_muriDiag; return false; }
	bool ok = true;
	if (timingId >= 0) ok = MuriCmdSingle(97, timingId) && ok;
	if (csId >= 0)     ok = MuriCmdSingle(99, csId) && ok;
	if (bt2020 >= 0)   ok = MuriCmdSingle(112, bt2020) && ok;
	if (hdrMode >= 0)  ok = MuriCmdSingle(111, hdrMode) && ok;
	if (bitDepth >= 0) ok = MuriCmdSingle(100, bitDepth) && ok;
	msgOut.Format(LS(IDS_GEN_MURI_APPLIED), useNet ? _T("HTTP") : _T("serial"), (LPCTSTR)s_muriDiag);
	return ok;
}

// Read the connected sink's EDID and return a parsed summary. Network transport only.
bool CGDIGenerator_MuriReadSinkInfo(bool useNet, const CString& ip, const CString& comPort, int tcpPort, CString& summaryOut)
{
	if (!MuriConnect(useNet, ip, comPort)) { summaryOut = s_muriDiag; return false; }
	if (useNet && tcpPort > 0) MuriSetTcpPort(tcpPort);
	std::vector<BYTE> edid;
	if (!MuriReadEdidBytes(edid)) { summaryOut = LS(IDS_GEN_EDID_READ_FAIL) + s_muriDiag; return false; }
	summaryOut = MuriParseEdid(edid);
	return true;
}

// Show a preset pattern (cat 98, double command): NUM=id, BER=bank (0 video, 1 stills).
bool CGDIGenerator_MuriShowPattern(bool useNet, const CString& ip, const CString& comPort, int patternId, int patternBer, CString& msgOut)
{
	if (!MuriConnect(useNet, ip, comPort)) { msgOut = s_muriDiag; return false; }
	bool ok = MuriCmdDouble(98, patternId, patternBer);
	msgOut.Format(LS(IDS_GEN_MURI_SENT_PATTERN),
		patternId, patternBer, useNet ? _T("HTTP") : _T("serial"), (LPCTSTR)s_muriDiag);
	return ok;
}

// Multi-line labeled readout (label\tvalue lines) for the panel status box.
bool CGDIGenerator_MuriQueryReadout(const CString& ip, CString& readoutOut)
{
	s_muriUseNet = true; s_muriIp = ip;
	return MuriStatusReadout(readoutOut);
}

// timing id -> resolution name (kMuriTimings command-id table); "id N" if unknown.
static CString MuriTimingNameForId(int id)
{
	for (int i = 0; i < kMuriTimingN; ++i)
		if (kMuriTimings[i].id == id) return CString(kMuriTimings[i].name);
	CString s; s.Format(_T("id %d"), id); return s;
}

// Query one read keyword (0x80xx) over the already-open serial port; return its first
// data byte, or -1 on no/short reply. The device answers a read with an 0xAB frame
//   AB 00 00 <LEN> 00 00 00 <kwlo> <kwhi> <data> <cksum>   (data at header + 9)
// and reports the SAME command-id numbering the HTTP VIDEOGEN echo uses - verified on
// hardware: 0x8061 returns the kMuriTimings id (e.g. 20 = 1080p60, matching HEX61NUM20).
static int MuriSerialReadValue(int readKeyword)
{
	std::string reply;
	if (!MuriSerialXfer(MuriBuildFrame(readKeyword, std::vector<BYTE>()), reply, 32)) return -1;
	for (size_t k = 0; k + 10 <= reply.size(); ++k)
		if ((BYTE)reply[k] == 0xAB) return (BYTE)reply[k + 9];
	return -1;
}

// Serial equivalent of MuriStatusReadout: query the individual read keywords and map
// them to the same labelled lines. Assumes the port is open. Fields/labels/order match
// the HTTP readout so the panel looks identical in either transport.
static bool MuriSerialStatusReadout(CString& out)
{
	int timing = MuriSerialReadValue(0x8061);	// cat 97  -> resolution (kMuriTimings id)
	int cs     = MuriSerialReadValue(0x8063);	// cat 99  -> colour space 0..4
	int depth  = MuriSerialReadValue(0x8064);	// cat 100 -> 0=8bit,1=10bit,2=12bit
	int hdcp   = MuriSerialReadValue(0x8065);	// cat 101 -> 0=off,1=on
	int output = MuriSerialReadValue(0x8066);	// cat 102 -> 1=HDMI,0=DVI
	int hdr    = MuriSerialReadValue(0x806F);	// cat 111 -> 0=SDR,1=HDR,2=HLG
	int bt2020 = MuriSerialReadValue(0x8070);	// cat 112 -> 0=BT.709,1=BT.2020
	if (timing < 0 && cs < 0) { out.Empty(); return false; }	// nothing answered

	CString res   = (timing >= 0) ? MuriTimingNameForId(timing) : CString(_T("?"));
	CString dyn   = (hdr == 0) ? _T("SDR") : (hdr == 1) ? _T("HDR") : (hdr == 2) ? _T("HLG") : _T("?");
	CString dep   = (depth == 0) ? _T("8Bit") : (depth == 1) ? _T("10Bit") : (depth == 2) ? _T("12Bit") : _T("?");
	CString gamut = (bt2020 == 0) ? _T("BT.709") : (bt2020 == 1) ? _T("BT.2020") : _T("?");
	CString fmt   = (cs == 0 || cs == 1) ? _T("RGB") : (cs >= 2 && cs <= 4) ? _T("YCbCr") : _T("?");
	CString rng   = (cs == 0) ? _T("Full") : (cs >= 1 && cs <= 4) ? _T("Limited") : _T("?");
	CString outp  = (output == 1) ? _T("HDMI") : (output == 0) ? _T("DVI") : _T("?");
	CString hdcpS = (hdcp == 1) ? _T("ON") : (hdcp == 0) ? _T("OFF") : _T("?");

	out.Empty();
	out += CString(_T("Dynamic range\t")) + dyn   + _T("\r\n");
	out += CString(_T("Resolution\t"))    + res   + _T("\r\n");
	out += CString(_T("Bit depth\t"))     + dep   + _T("\r\n");
	out += CString(_T("Color space\t"))   + gamut + _T("\r\n");
	out += CString(_T("Color format\t"))  + fmt   + _T("\r\n");
	out += CString(_T("Signal range\t"))  + rng   + _T("\r\n");
	out += CString(_T("Output\t"))        + outp  + _T("\r\n");
	out += CString(_T("HDCP\t"))          + hdcpS;
	return true;
}

// Serial status readout: open the port, query the read keywords, close (mirrors the
// Detect lifecycle). Used when the panel is in serial (non-network) transport mode.
bool CGDIGenerator_MuriQueryReadoutSerial(const CString& comPort, CString& readoutOut)
{
	if (!MuriConnect(false, CString(), comPort)) { readoutOut.Empty(); return false; }
	bool ok = MuriSerialStatusReadout(readoutOut);
	MuriClose();
	return ok && !readoutOut.IsEmpty();
}

BOOL CGDIGenerator::DisplayRGBColorDVDO( const ColorRGBDisplay& clr, bool /*first*/, UINT /*nPattern*/ )
{
	if (!s_dvdoOpen) return FALSE;

	// Use the live window value (kept in sync with the prop page + config by
	// GetPropertiesSheetValues); the generator's own m_rectSizePercent is only set
	// once at construction and would send a stale size.
	int win = (int)m_displayWindow.m_rectSizePercent;
	if (win <= 0 || win > 100) win = 100;

	{
		// Arm the internal TPG on the first AA patch of the session. On this firmware the TPG can
		// be left DISARMED - by an 80=0 "patterns off" or the device's own remote - and then AA
		// custom patches silently do NOT render (screen stays blank/blue) until an 80 pattern
		// re-arms it; 6C/61/repeated AA do not (hardware-confirmed 2026-08-08). 80=35 is a
		// full-black field (0% IRE) so this arm is visually invisible, and it is sent back-to-back
		// with the AA below (< 1 frame). Only the first patch pays this; DvdoCommand drains the ack.
		if (!s_dvdoArmed)
		{
			DvdoSendPattern(kDvdoArmPattern);
			s_dvdoArmed = true;
		}
		// AA full triplet: R G B Window BackgroundIRE InputRange OutColourSpace OutRange.
		// Encode R/G/B in HCFR's selected range (m_b16_235): limited -> 16-235, full ->
		// 0-255, exactly matching the main-page stimulus values. InputRange must match the
		// encoding so the device interprets the numbers correctly; OutRange is set the same
		// (the AVLab ignores it on current firmware - actual output range is set on its OSD).
		bool lim = (m_b16_235 != 0);
		int rng = lim ? 0 : 1;		// AA range param: 0 = Limited (16-235), 1 = Full (0-255)
		int r = ColorRGBDisplay::ConvertPercentToBYTE(clr[0], lim);
		int g = ColorRGBDisplay::ConvertPercentToBYTE(clr[1], lim);
		int b = ColorRGBDisplay::ConvertPercentToBYTE(clr[2], lim);
		// APL surround: BackgroundIRE (5th AA param), same scale as R/G/B. Live prop value.
		int bg = ColorRGBDisplay::ConvertPercentToBYTE((double)m_displayWindow.m_bgStimPercent, lim);
		char val[96];
		sprintf(val, "%d %d %d %d %d %d %d %d", r, g, b, win, bg, rng, m_dvdoColorSpace, rng);
		DvdoCommand("AA", std::vector<std::string>(1, val));
	}

	// Courtesy settle wait, mirroring the other external wires.
	DWORD dwWait = GetConfig()->GetProfileInt("Debug","WaitAfterDisplayPattern",80);
	DWORD dwStart = GetTickCount();
	while (GetTickCount() - dwStart < dwWait)
	{
		MSG Msg;
		while (PeekMessage(&Msg, NULL, NULL, NULL, PM_REMOVE)) { TranslateMessage(&Msg); DispatchMessage(&Msg); }
		Sleep(0);
	}
	return TRUE;
}

BOOL CGDIGenerator::DisplayRGBColorMurideo( const ColorRGBDisplay& clr, bool /*first*/, UINT /*nPattern*/ )
{
	// Make sure the transport is selected (Init did this too, but be safe).
	MuriConnect(m_muriUseNetwork != 0, m_muriIp, m_muriComPort);

	// Triplet + bit-depth generation mirror PGenerator (DisplayRGBColorrPI) exactly - that
	// is the reference model. Foreground uses PiPercentToCode; the surround is the APL
	// background (PiBackground8ToCode) so window patches hit the target average level.
	// Bit depth is the page's level depth (GetUse10bitLevels, now Murideo-aware) so the
	// codes we send are byte-for-byte the triplet HCFR displays. depth 0=8bit/1=10bit;
	// colour space 0=RGB full / 1=RGB limited from the range flag.
	bool lim = (m_b16_235 != 0);
	double bgstim = m_displayWindow.m_bgStimPercent / 100.;
	double rect   = (double)m_displayWindow.m_rectSizePercent;
	double R1 = 0., G1 = 0., B1 = 0.;
	if (rect < 100.)		// subtract the window area so the average APL matches bgStim
	{
		R1 = min(255., max(0., (bgstim * 255 - clr[0] * rect / 100.) / (1 - rect / 100.)));
		G1 = min(255., max(0., (bgstim * 255 - clr[1] * rect / 100.) / (1 - rect / 100.)));
		B1 = min(255., max(0., (bgstim * 255 - clr[2] * rect / 100.) / (1 - rect / 100.)));
	}
	int bits = GetConfig()->GetUse10bitLevels() ? 10 : 8;
	int r   = PiPercentToCode(clr[0], lim, bits);
	int g   = PiPercentToCode(clr[1], lim, bits);
	int b   = PiPercentToCode(clr[2], lim, bits);
	int bgR = PiBackground8ToCode(R1, lim, bits);
	int bgG = PiBackground8ToCode(G1, lim, bits);
	int bgB = PiBackground8ToCode(B1, lim, bits);
	int win = (int)m_displayWindow.m_rectSizePercent; if (win <= 0 || win > 100) win = 100;

	MuriCmdRgbTriplet(r, g, b, bgR, bgG, bgB, win, (bits == 10) ? 1 : 0, lim ? 1 : 0);
	return TRUE;
}

BOOL CGDIGenerator::DisplayRGBColor( const ColorRGBDisplay& clr , MeasureType nPatternType , UINT nPatternInfo , BOOL bChangePattern, BOOL bSilentMode)
{
	ColorRGBDisplay p_clr;
	BOOL do_Intensity=false;
	if ( nPatternType == MT_PRIMARY || nPatternType == MT_SECONDARY || nPatternType == MT_SAT_RED || nPatternType == MT_SAT_GREEN || nPatternType == MT_SAT_BLUE || nPatternType == MT_SAT_YELLOW || nPatternType == MT_SAT_CYAN || nPatternType == MT_SAT_MAGENTA || nPatternType == MT_ACTUAL)
		do_Intensity = true;

	p_clr[0] = clr[0] * m_displayWindow.m_Intensity / 100;
	p_clr[1] = clr[1] * m_displayWindow.m_Intensity / 100;
	p_clr[2] = clr[2] * m_displayWindow.m_Intensity / 100;

	//see if we need to reconnect generator
	if (!this->m_bisInited)
		Init();

	if (m_GDIGenePropertiesPage.m_nDisplayMode == DISPLAY_GDI_Hide && nPatternType != MT_SPECIAL && nPatternType != MT_CONTRAST)
	{
		( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.ShowWindow(SW_SHOW);
		( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> EnableWindow (TRUE);
		( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.SetForegroundWindow();
//static pattern
		m_nPat++;
		if ((m_nPat % GetConfig()->m_ablFreq == 0)  && GetConfig()->m_bABL)
		{
			BYTE lvl = m_b16_235 ? (BYTE)(2.19 * GetConfig()->m_ablLevel + 16) : (BYTE)(2.55 * GetConfig()->m_ablLevel);
			WINDOWPLACEMENT wp;
			CRect rect;
			::GetWindowRect ( ::GetDesktopWindow (), & rect );
			( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.GetWindowPlacement(&wp);			
			( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.SetWindowPos(&CWnd::wndTop,0,0,rect.Width(),rect.Height(),SWP_SHOWWINDOW);

			( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) ->m_wndTestColorWnd.m_colorPicker.SetColor ( RGB(lvl, lvl, lvl) );
			
			( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.RedrawWindow ();
			
			Sleep(GetConfig()->m_ablDuration);
			( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.SetWindowPlacement(&wp);			
		}
		( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.SetForegroundWindow();
		( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.RedrawWindow ();
	} else
	{
		if ( m_GDIGenePropertiesPage.m_nDisplayMode == DISPLAY_madVR)
			DisplayRGBColormadVR (do_Intensity?p_clr:clr, GetConfig()->m_isSettling, nPatternInfo);
		else if ( m_GDIGenePropertiesPage.m_nDisplayMode == DISPLAY_ccast)
			DisplayRGBCCast (do_Intensity?p_clr:clr, GetConfig()->m_isSettling, nPatternInfo );
		else if ( m_GDIGenePropertiesPage.m_nDisplayMode == DISPLAY_rPI)
			DisplayRGBColorrPI (do_Intensity?p_clr:clr, GetConfig()->m_isSettling, nPatternInfo );
		else if ( m_GDIGenePropertiesPage.m_nDisplayMode == DISPLAY_DVDO)
			DisplayRGBColorDVDO (do_Intensity?p_clr:clr, GetConfig()->m_isSettling, nPatternInfo );
		else if ( m_GDIGenePropertiesPage.m_nDisplayMode == DISPLAY_MURIDEO)
			DisplayRGBColorMurideo (do_Intensity?p_clr:clr, GetConfig()->m_isSettling, nPatternInfo );
		else
			m_displayWindow.DisplayRGBColor(do_Intensity?p_clr:clr, nPatternInfo);
	}

	return TRUE;
}


BOOL CGDIGenerator::CanDisplayAnsiBWRects()
{
	return ((m_GDIGenePropertiesPage.m_nDisplayMode != DISPLAY_madVR) && (m_GDIGenePropertiesPage.m_nDisplayMode != DISPLAY_DVDO) && (m_GDIGenePropertiesPage.m_nDisplayMode != DISPLAY_MURIDEO));
}

BOOL CGDIGenerator::CanDisplayAnimatedPatterns(BOOL isSpecialty)
{
	if (isSpecialty && m_GDIGenePropertiesPage.m_nDisplayMode != DISPLAY_madVR && m_GDIGenePropertiesPage.m_nDisplayMode != DISPLAY_DVDO && m_GDIGenePropertiesPage.m_nDisplayMode != DISPLAY_MURIDEO)
		return TRUE;

	return ((m_GDIGenePropertiesPage.m_nDisplayMode != DISPLAY_madVR) && (m_GDIGenePropertiesPage.m_nDisplayMode != DISPLAY_ccast) && (m_GDIGenePropertiesPage.m_nDisplayMode != DISPLAY_rPI) && (m_GDIGenePropertiesPage.m_nDisplayMode != DISPLAY_DVDO) && (m_GDIGenePropertiesPage.m_nDisplayMode != DISPLAY_MURIDEO) ? TRUE:FALSE);
}

BOOL CGDIGenerator::DisplayAnsiBWRects(BOOL bInvert)
{
	m_displayWindow.DisplayAnsiBWRects(bInvert);
	return TRUE;
}

BOOL CGDIGenerator::DisplayAnimatedBlack()
{
	m_displayWindow.DisplayAnimatedBlack();
	return TRUE;
}

BOOL CGDIGenerator::DisplayAnimatedWhite()
{
	m_displayWindow.DisplayAnimatedWhite();
	return TRUE;
}

BOOL CGDIGenerator::DisplayGradient()
{
	m_displayWindow.DisplayGradient();
	return TRUE;
}

BOOL CGDIGenerator::DisplayRG()
{
	m_displayWindow.DisplayRG();
	return TRUE;
}
BOOL CGDIGenerator::DisplayRB()
{
	m_displayWindow.DisplayRB();
	return TRUE;
}
BOOL CGDIGenerator::DisplayGB()
{
	m_displayWindow.DisplayGB();
	return TRUE;
}
BOOL CGDIGenerator::DisplayRGd()
{
	m_displayWindow.DisplayRGd();
	return TRUE;
}
BOOL CGDIGenerator::DisplayRBd()
{
	m_displayWindow.DisplayRBd();
	return TRUE;
}
BOOL CGDIGenerator::DisplayGBd()
{
	m_displayWindow.DisplayGBd();
	return TRUE;
}

BOOL CGDIGenerator::DisplayGradient2()
{
	m_displayWindow.DisplayGradient2();
	return TRUE;
}

BOOL CGDIGenerator::DisplayLramp()
{
	m_displayWindow.DisplayLramp();
	return TRUE;
}

BOOL CGDIGenerator::DisplayGranger()
{
	m_displayWindow.DisplayGranger();
	return TRUE;
}

BOOL CGDIGenerator::Display80()
{
	m_displayWindow.Display80();
	return TRUE;
}

BOOL CGDIGenerator::DisplayTV()
{
	m_displayWindow.DisplayTV();
	return TRUE;
}

BOOL CGDIGenerator::DisplayTV2()
{
	m_displayWindow.DisplayTV2();
	return TRUE;
}

BOOL CGDIGenerator::DisplaySpectrum()
{
	m_displayWindow.DisplaySpectrum();
	return TRUE;
}

BOOL CGDIGenerator::DisplaySramp()
{
	m_displayWindow.DisplaySramp();
	return TRUE;
}

BOOL CGDIGenerator::DisplayVSMPTE()
{
	m_displayWindow.DisplayVSMPTE();
	return TRUE;
}

BOOL CGDIGenerator::DisplayEramp()
{
	m_displayWindow.DisplayEramp();
	return TRUE;
}

BOOL CGDIGenerator::DisplayTC0()
{
	m_displayWindow.DisplayTC0();
	return TRUE;
}

BOOL CGDIGenerator::DisplayTC1()
{
	m_displayWindow.DisplayTC1();
	return TRUE;
}

BOOL CGDIGenerator::DisplayTC2()
{
	m_displayWindow.DisplayTC2();
	return TRUE;
}

BOOL CGDIGenerator::DisplayTC3()
{
	m_displayWindow.DisplayTC3();
	return TRUE;
}

BOOL CGDIGenerator::DisplayTC4()
{
	m_displayWindow.DisplayTC4();
	return TRUE;
}

BOOL CGDIGenerator::DisplayTC5()
{
	m_displayWindow.DisplayTC5();
	return TRUE;
}

BOOL CGDIGenerator::DisplayBN()
{
	m_displayWindow.DisplayBN();
	return TRUE;
}

BOOL CGDIGenerator::DisplayDR0()
{
	m_displayWindow.DisplayDR0();
	return TRUE;
}

BOOL CGDIGenerator::DisplayDR1()
{
	m_displayWindow.DisplayDR1();
	return TRUE;
}

BOOL CGDIGenerator::DisplayDR2()
{
	m_displayWindow.DisplayDR2();
	return TRUE;
}


BOOL CGDIGenerator::DisplayAlign()
{
	m_displayWindow.DisplayAlign();
	return TRUE;
}

BOOL CGDIGenerator::DisplayAlign2()
{
	m_displayWindow.DisplayAlign2();
	return TRUE;
}

BOOL CGDIGenerator::DisplayUser1()
{
	m_displayWindow.DisplayUser1();
	return TRUE;
}

BOOL CGDIGenerator::DisplayUser2()
{
	m_displayWindow.DisplayUser2();
	return TRUE;
}

BOOL CGDIGenerator::DisplayUser3()
{
	m_displayWindow.DisplayUser3();
	return TRUE;
}

BOOL CGDIGenerator::DisplayUser4()
{
	m_displayWindow.DisplayUser4();
	return TRUE;
}

BOOL CGDIGenerator::DisplayUser5()
{
	m_displayWindow.DisplayUser5();
	return TRUE;
}

BOOL CGDIGenerator::DisplayUser6()
{
	m_displayWindow.DisplayUser6();
	return TRUE;
}

BOOL CGDIGenerator::DisplaySharp()
{
	if (m_GDIGenePropertiesPage.m_nDisplayMode == DISPLAY_DVDO) { DvdoSendPattern(40 /*Sharpness*/); return TRUE; }
	m_displayWindow.DisplaySharp();
	return TRUE;
}

BOOL CGDIGenerator::DisplayClipH()
{
	m_displayWindow.DisplayClipH();
	return TRUE;
}

BOOL CGDIGenerator::DisplayClipHO()
{
	m_displayWindow.DisplayClipHO();
	return TRUE;
}

BOOL CGDIGenerator::DisplayNBO()
{
	m_displayWindow.DisplayNBO();
	return TRUE;
}

BOOL CGDIGenerator::DisplayClipL()
{
	m_displayWindow.DisplayClipL();
	return TRUE;
}

BOOL CGDIGenerator::DisplayClipLO()
{
	m_displayWindow.DisplayClipLO();
	return TRUE;
}

BOOL CGDIGenerator::DisplayTestimg()
{
	m_displayWindow.DisplayTestimg();
	return TRUE;
}

BOOL CGDIGenerator::DisplayISO12233()
{
	m_displayWindow.DisplayISO12233();
	return TRUE;
}

BOOL CGDIGenerator::DisplayNB()
{
	m_displayWindow.DisplayNB();
	return TRUE;
}

BOOL CGDIGenerator::DisplayBBCHD()
{
	m_displayWindow.DisplayBBCHD();
	return TRUE;
}

BOOL CGDIGenerator::DisplayCROSSl()
{
	m_displayWindow.DisplayCROSSl();
	return TRUE;
}

BOOL CGDIGenerator::DisplayCROSSd()
{
	m_displayWindow.DisplayCROSSd();
	return TRUE;
}

BOOL CGDIGenerator::DisplayPM5644()
{
	m_displayWindow.DisplayPM5644();
	return TRUE;
}

BOOL CGDIGenerator::DisplayZONE()
{
	m_displayWindow.DisplayZONE();
	return TRUE;
}

BOOL CGDIGenerator::DisplayDotPattern( const ColorRGBDisplay& clr , BOOL dot2, UINT nPads)
{
	if (m_GDIGenePropertiesPage.m_nDisplayMode == DISPLAY_DVDO) { DvdoSendPattern(3 /*EVOT pixel*/); return TRUE; }
	m_displayWindow.DisplayDotPattern(clr, dot2, nPads);
	return TRUE;
}

BOOL CGDIGenerator::DisplayHVLinesPattern( const ColorRGBDisplay& clr , BOOL dot2, BOOL vLines)
{
	if (m_GDIGenePropertiesPage.m_nDisplayMode == DISPLAY_DVDO) { DvdoSendPattern(vLines ? 4 /*EVOT H/V line*/ : 5 /*EVOT H line*/); return TRUE; }
	m_displayWindow.DisplayHVLinesPattern(clr, dot2, vLines);
	return TRUE;
}

BOOL CGDIGenerator::DisplayColorLevelPattern( INT clrLevel , BOOL dot2, UINT nPads)
{
	m_displayWindow.DisplayColorLevelPattern(clrLevel, dot2, nPads);
	return TRUE;
}

BOOL CGDIGenerator::DisplayGeomPattern( BOOL dot2, UINT nPads)
{
	if (m_GDIGenePropertiesPage.m_nDisplayMode == DISPLAY_DVDO) { DvdoSendPattern(1 /*FRMGEOM*/); return TRUE; }
	m_displayWindow.DisplayGeomPattern(dot2, nPads);
	return TRUE;
}

BOOL CGDIGenerator::DisplayConvPattern( BOOL dot2, UINT nPads)
{
	if (m_GDIGenePropertiesPage.m_nDisplayMode == DISPLAY_DVDO) { DvdoSendPattern(21 /*Crosshatch fine*/); return TRUE; }
	m_displayWindow.DisplayConvPattern(dot2, nPads);
	return TRUE;
}

BOOL CGDIGenerator::DisplayColorPattern( BOOL dot2)
{
	if (m_GDIGenePropertiesPage.m_nDisplayMode == DISPLAY_DVDO) { DvdoSendPattern(8 /*8-bar 100%*/); return TRUE; }
	m_displayWindow.DisplayColorPattern(dot2);
	return TRUE;
}

BOOL CGDIGenerator::DisplayPatternPicture(HMODULE hInst, UINT nIDResource, BOOL bResizePict)
{
	m_displayWindow.DisplayPatternPicture(hInst,nIDResource,bResizePict);
	return TRUE;
}


BOOL CGDIGenerator::Release(INT nbNext)
{
	GetColorApp() -> SetPatternWindow ( NULL );
	if ( m_nDisplayMode == DISPLAY_DVDO )
		DvdoClose();
	if ( m_nDisplayMode == DISPLAY_MURIDEO )
		MuriDisconnect();
	m_displayWindow.Hide();
	m_displayWindow.SetDisplayMode(GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE));
	m_displayWindow.m_nPat = 0;

	if (m_madVR_HDR && m_GDIGenePropertiesPage.m_nDisplayMode == DISPLAY_madVR)
		  madVR_SetHdrButton(FALSE);

	BOOL bOk = CGenerator::Release(nbNext);
	m_nPat = 0;
	if ( m_bBlankingCanceled )
	{
		m_doScreenBlanking = TRUE;
		m_bBlankingCanceled = FALSE;
	}

	//restore gamma tables
	if (m_bConnect && m_bLinear)
	{
		char arg[255];
		CString str = GetConfig () -> m_ApplicationPath;
		CString str1 = GetConfig () -> m_ApplicationPath;
		str += "\\tools\\dispwin.exe";
		str1 += "\\tools\\current.cal";
		_snprintf(arg, sizeof(arg), " -d%d %s", m_activeMonitorNum+1, (LPCTSTR)str1);
		arg[sizeof(arg)-1] = 0;
		ShellExecute(NULL, "open", str, arg, NULL, SW_HIDE);
		m_bConnect = FALSE;
	}

	return bOk;
}

CString CGDIGenerator::GetActiveDisplayName() 
{ 
	return m_GDIGenePropertiesPage.m_monitorNameArray[m_activeMonitorNum];
}