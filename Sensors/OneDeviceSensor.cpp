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
/////////////////////////////////////////////////////////////////////////////

// OneDeviceSensor.cpp: implementation of the COneDeviceSensor class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "OneDeviceSensor.h"
#include "Generator.h"
#include "Signature.h"
#include <math.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

IMPLEMENT_SERIAL(COneDeviceSensor, CSensor, 1) ;


//////////////////////////////////////////////////////////////////////
// Calibration info storage class
//////////////////////////////////////////////////////////////////////

CCalibrationInfo::CCalibrationInfo(Matrix & measures, Matrix & references, CColor & WhiteTest, CColor & WhiteRef, CColor & BlackTest, CColor & BlackRef, CString & Author)
	: m_measures(measures),m_references(references),m_WhiteTest(WhiteTest),m_WhiteRef(WhiteRef),m_BlackTest(BlackTest),m_BlackRef(BlackRef),m_Author(Author)
{
}

CCalibrationInfo::CCalibrationInfo(CCalibrationInfo & other)
	: m_measures(other.m_measures),m_references(other.m_references),m_WhiteTest(other.m_WhiteTest),m_WhiteRef(other.m_WhiteRef),
	m_BlackTest(other.m_BlackTest),m_BlackRef(other.m_BlackRef),m_Author(other.m_Author)
{
}

void CCalibrationInfo::Serialize(CArchive& archive)
{
	if (archive.IsStoring())
	{
		int version=2;

		if ( GetConfig () -> GetProfileInt ( "Debug", "SaveOldCalibrationFile", FALSE ) )
			version = 1;

		archive << version;
		m_measures.Serialize(archive);
		m_references.Serialize(archive);
		m_WhiteTest.Serialize(archive);
		m_WhiteRef.Serialize(archive);
		if ( version > 1 )
		{
			m_BlackTest.Serialize(archive);
			m_BlackRef.Serialize(archive);
			archive << m_Author;
		}
	}
	else
	{
		int version;
		archive >> version;
		if ( version > 2 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		m_measures.Serialize(archive);
		m_references.Serialize(archive);
		m_WhiteTest.Serialize(archive);
		m_WhiteRef.Serialize(archive);
		if ( version > 1 )
		{
			m_BlackTest.Serialize(archive);
			m_BlackRef.Serialize(archive);
			archive >> m_Author;
		}
		else
		{
			m_BlackTest=noDataColor;
			m_BlackRef=noDataColor;
			m_Author.Empty();
		}
	}
}

void CCalibrationInfo::DisplayAdditivity ( Matrix & sensorToXYZMatrix, BOOL bForHCFRSensor )
{
	CString Msg, Title;

	GetAdditivityInfoText ( Msg, sensorToXYZMatrix, bForHCFRSensor );

	Title.LoadString ( IDS_ADDITIVITYRESULTS );
	GetColorApp()->InMeasureMessageBox ( Msg, Title, MB_ICONINFORMATION | MB_OK );
}

void CCalibrationInfo::GetAdditivityInfoText ( CString & strResult, Matrix & sensorToXYZMatrix, BOOL bForHCFRSensor )
{
	CString Msg;
	CString componentName[3];

	componentName [0].LoadString ( IDS_RED );
	componentName [1].LoadString ( IDS_GREEN );
	componentName [2].LoadString ( IDS_BLUE );

	strResult.LoadString ( IDS_ADDITIVITY );
	strResult+="\r\n";

	for ( int i = 0; i < 3 ; i ++ )
	{
		strResult += componentName [ i ];
		double aSum=m_measures(i,0)+m_measures(i,1)+m_measures(i,2);
		CString str;
		Msg.LoadString ( IDS_INSTEADOF );
		str.Format((bForHCFRSensor?" : %.0f %s %.0f ( %.1f%% )\r\n":" : %.3f %s %.3f ( %.1f%% )\r\n"),aSum,(LPCSTR)Msg,m_WhiteTest[i],((aSum - m_WhiteTest[i])/m_WhiteTest[i])*100.0);
		strResult+=str;
	}
	
	if ( m_WhiteTest.isValid() && m_WhiteRef.isValid() )
	{
		CString str;
		CColor ConvertedWhiteTest = ColorXYZ(sensorToXYZMatrix * m_WhiteTest.GetXYZValue());

		strResult += "\r\n";
		Msg.LoadString ( IDS_WHITE );
		strResult += Msg;
		Msg.LoadString ( IDS_INSTEADOF );
		str.Format(" : X=%.3f Y=%.3f Z=%.3f %s X=%.3f Y=%.3f Z=%.3f\r\n",ConvertedWhiteTest[0],ConvertedWhiteTest[1],ConvertedWhiteTest[2],(LPCSTR)Msg,m_WhiteRef[0],m_WhiteRef[1],m_WhiteRef[2]);
		strResult+=str;
		Msg.LoadString ( IDS_DELTAE );
		strResult += Msg;
		str.Format(" : %.1f\r\n",ConvertedWhiteTest.GetDeltaE(-1.0,m_WhiteRef,-1.0, GetColorReference(), 	GetConfig()->m_dE_form, false, GetConfig()->gw_Weight ));
		strResult+=str;
	}

	if ( m_BlackTest.isValid() && m_BlackRef.isValid() )
	{
		CString str;
		CColor ConvertedBlackTest = ColorXYZ(sensorToXYZMatrix * m_BlackTest.GetXYZValue());

		strResult += "\r\n";
		Msg.LoadString ( IDS_BLACK );
		strResult += Msg;
		Msg.LoadString ( IDS_INSTEADOF );
		str.Format(" : X=%.3f Y=%.3f Z=%.3f %s X=%.3f Y=%.3f Z=%.3f\r\n",ConvertedBlackTest[0],ConvertedBlackTest[1],ConvertedBlackTest[2],(LPCSTR)Msg,m_BlackRef[0],m_BlackRef[1],m_BlackRef[2]);
		strResult+=str;
	}

	if ( ! m_Author.IsEmpty () )
	{
		strResult += "----\r\n";
		strResult += m_Author;
		strResult += "\r\n";
	}
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

COneDeviceSensor::COneDeviceSensor()
{
	m_calibrationIRELevel=100.0;

	// Default chromacities are the Rec709 ones
	Matrix chromacities(0.0,3,3);
	chromacities(0,0)=0.640;	// Red chromacities
	chromacities(1,0)=0.330;
	chromacities(2,0)=1.0-chromacities(1,0)-chromacities(0,0);

	chromacities(0,1)=0.300;	// Green chromacities
	chromacities(1,1)=0.600;
	chromacities(2,1)=1.0-chromacities(0,1)-chromacities(1,1);

	chromacities(0,2)=0.150;	// Blue chromacities
	chromacities(1,2)=0.060;
	chromacities(2,2)=1.0-chromacities(0,2)-chromacities(1,2);

	// Default white is D65
	Matrix white(0.0,3,1);
	white(0,0)=0.3127;
	white(1,0)=0.3290;
	white(2,0)=1.0-white(0,0)-white(1,0);

	m_primariesChromacities=chromacities;
	m_whiteChromacity=white;

	m_doBlackCompensation=TRUE;
	m_doVerifyAdditivity=TRUE;
	m_maxAdditivityErrorPercent=15;
	m_showAdditivityResults=TRUE;

	m_pCalibrationInfo = NULL;

	m_pCalibrationPage = & m_oneDevicePropertiesPage;
}

COneDeviceSensor::~COneDeviceSensor()
{
	if ( m_pCalibrationInfo )
	{
		delete m_pCalibrationInfo;
		m_pCalibrationInfo = NULL;
	}
}

void COneDeviceSensor::Copy(CSensor * p)
{
	CSensor::Copy(p);
	
	m_calibrationIRELevel = ((COneDeviceSensor*)p)->m_calibrationIRELevel;
	m_doBlackCompensation = ((COneDeviceSensor*)p)->m_doBlackCompensation;
	m_doVerifyAdditivity = ((COneDeviceSensor*)p)->m_doVerifyAdditivity;
	m_showAdditivityResults = ((COneDeviceSensor*)p)->m_showAdditivityResults;
	m_maxAdditivityErrorPercent = ((COneDeviceSensor*)p)->m_maxAdditivityErrorPercent;

	m_calibrationReferenceName = ((COneDeviceSensor*)p)->m_calibrationReferenceName;
	m_primariesChromacities = ((COneDeviceSensor*)p)->m_primariesChromacities; 
	m_whiteChromacity = ((COneDeviceSensor*)p)->m_whiteChromacity; 

	if ( m_pCalibrationInfo )
	{
		delete m_pCalibrationInfo;
		m_pCalibrationInfo = NULL;
	}

	if ( ((COneDeviceSensor*)p)->m_pCalibrationInfo )
	{
		m_pCalibrationInfo = new CCalibrationInfo (	* ((COneDeviceSensor*)p)->m_pCalibrationInfo );
	}
}

void COneDeviceSensor::SetCalibrationInfo ( CCalibrationInfo * pInfo )
{
	if ( m_pCalibrationInfo )
	{
		delete m_pCalibrationInfo;
		m_pCalibrationInfo = NULL;
	}

	m_pCalibrationInfo = pInfo;
}

void COneDeviceSensor::Serialize(CArchive& archive)
{
	CSensor::Serialize(archive) ;
	if (archive.IsStoring())
	{
		int version=3;
		if ( m_pCalibrationInfo != NULL )
			version = 4;

		if ( GetConfig () -> GetProfileInt ( "Debug", "SaveOldCalibrationFile", FALSE ) )
			version -= 2;

		archive << version;
		archive << m_calibrationIRELevel;
		archive << m_doBlackCompensation;
		archive << m_doVerifyAdditivity;
		archive << m_showAdditivityResults;
		archive << m_maxAdditivityErrorPercent;
		m_primariesChromacities.Serialize(archive);
		m_whiteChromacity.Serialize(archive);
		archive << m_calibrationReferenceName;
		if ( m_pCalibrationInfo != NULL )
		{
			m_pCalibrationInfo->Serialize(archive);
		}
		if ( version >= 3 )
			archive << m_CalibrationFileName;
	}
	else
	{
		int version;
		archive >> version;
		if ( version > 4 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		archive >> m_calibrationIRELevel;
		archive >> m_doBlackCompensation;
		archive >> m_doVerifyAdditivity;
		archive >> m_showAdditivityResults;
		archive >> m_maxAdditivityErrorPercent;
		m_primariesChromacities.Serialize(archive);
		m_whiteChromacity.Serialize(archive);
		archive >> m_calibrationReferenceName;
		
		if ( m_pCalibrationInfo )
		{
			delete m_pCalibrationInfo;
			m_pCalibrationInfo = NULL;
		}
		if ( version == 2 || version == 4 )
		{
			m_pCalibrationInfo = new CCalibrationInfo;
			m_pCalibrationInfo->Serialize(archive);
		}

		if ( version >= 3 )
			archive >> m_CalibrationFileName;
		else
			m_CalibrationFileName.Empty();

	}
}

void COneDeviceSensor::SetPropertiesSheetValues()
{
	CSensor::SetPropertiesSheetValues();

	m_oneDevicePropertiesPage.m_calibrationIRELevel=m_calibrationIRELevel;
	m_oneDevicePropertiesPage.m_doBlackCompensation=m_doBlackCompensation;
	m_oneDevicePropertiesPage.m_doVerifyAdditivity=m_doVerifyAdditivity;
	m_oneDevicePropertiesPage.m_maxAdditivityErrorPercent=m_maxAdditivityErrorPercent;
	m_oneDevicePropertiesPage.m_showAdditivityResults=m_showAdditivityResults;
	m_oneDevicePropertiesPage.m_calibrationReferenceName=m_calibrationReferenceName;
	m_oneDevicePropertiesPage.SetPrimariesMatrix(m_primariesChromacities);
	m_oneDevicePropertiesPage.SetWhiteMatrix(m_whiteChromacity);

	// Build information string inside calibration matrix page
	m_SensorPropertiesPage.m_information.Empty ();

	if ( ! m_CalibrationFileName.IsEmpty () )
	{
		m_SensorPropertiesPage.m_information = m_CalibrationFileName + "\r\n";
	}

	if ( m_pCalibrationInfo )
	{
		CString str;
		m_pCalibrationInfo -> GetAdditivityInfoText ( str, m_sensorToXYZMatrix, FALSE );
		m_SensorPropertiesPage.m_information += str;
	}
}

void COneDeviceSensor::GetPropertiesSheetValues()
{
	CSensor::GetPropertiesSheetValues();

	if(m_calibrationIRELevel!=m_oneDevicePropertiesPage.m_calibrationIRELevel)
	{
		m_calibrationIRELevel=(float)m_oneDevicePropertiesPage.m_calibrationIRELevel;
		SetModifiedFlag(TRUE);
	}

	if(m_doBlackCompensation!=m_oneDevicePropertiesPage.m_doBlackCompensation)
	{
		m_doBlackCompensation=m_oneDevicePropertiesPage.m_doBlackCompensation;
		SetModifiedFlag(TRUE);
	}
	if(m_doVerifyAdditivity!=m_oneDevicePropertiesPage.m_doVerifyAdditivity)
	{
		m_doVerifyAdditivity=m_oneDevicePropertiesPage.m_doVerifyAdditivity;
		SetModifiedFlag(TRUE);
	}
	if(m_maxAdditivityErrorPercent!=m_oneDevicePropertiesPage.m_maxAdditivityErrorPercent)
	{
		m_maxAdditivityErrorPercent=m_oneDevicePropertiesPage.m_maxAdditivityErrorPercent;
		SetModifiedFlag(TRUE);
	}
	if(m_showAdditivityResults!=m_oneDevicePropertiesPage.m_showAdditivityResults)
	{
		m_showAdditivityResults=m_oneDevicePropertiesPage.m_showAdditivityResults;
		SetModifiedFlag(TRUE);
	}
	if(m_calibrationReferenceName!=m_oneDevicePropertiesPage.m_calibrationReferenceName)
	{
		m_calibrationReferenceName=m_oneDevicePropertiesPage.m_calibrationReferenceName;
		SetModifiedFlag(TRUE);
	}
	if(m_primariesChromacities!=m_oneDevicePropertiesPage.GetPrimariesMatrix())
	{
		m_primariesChromacities=m_oneDevicePropertiesPage.GetPrimariesMatrix();
		SetModifiedFlag(TRUE);
	}
	if(m_whiteChromacity!=m_oneDevicePropertiesPage.GetWhiteMatrix())
	{
		m_whiteChromacity=m_oneDevicePropertiesPage.GetWhiteMatrix();
	}
}

void COneDeviceSensor::LoadCalibrationFile(CString & aFileName)
{
	CFile loadFile(aFileName,CFile::modeRead);
	CArchive ar(&loadFile,CArchive::load);
	COneDeviceSensor::Serialize(ar);
	m_CalibrationFileName = loadFile.GetFileTitle ();
}

// Compare two directories as file system locations rather than as strings:
// 8.3 short names, relative forms, subst drives, junctions and UNC shares all
// spell the same folder differently.
static BOOL IsSameDirectory ( LPCSTR lpszDir1, LPCSTR lpszDir2 )
{
	char						szDir [ 2 ] [ MAX_PATH ];
	HANDLE						hDir [ 2 ];
	BY_HANDLE_FILE_INFORMATION	fileInfo [ 2 ];
	BOOL						bHaveInfo = TRUE;
	int							i;

	for ( i = 0; i < 2; i ++ )
	{
		LPCSTR	lpszSrc = ( i == 0 ? lpszDir1 : lpszDir2 );
		DWORD	dwLen;
		int		nLen;

		// A path too long for the buffer leaves it untouched and returns the
		// size it would need. Nothing here can canonicalize such a path, and
		// a truncated copy would make two different folders look identical,
		// so compare the two names exactly as they were given.
		dwLen = GetFullPathName ( lpszSrc, MAX_PATH, szDir [ i ], NULL );
		if ( dwLen == 0 || dwLen >= MAX_PATH )
			return ( _stricmp ( lpszDir1, lpszDir2 ) == 0 );

		// Drop the trailing backslash, except on a root like "C:\"
		nLen = strlen ( szDir [ i ] );
		while ( nLen > 1 && szDir [ i ] [ nLen - 1 ] == '\\' && szDir [ i ] [ nLen - 2 ] != ':' )
			szDir [ i ] [ -- nLen ] = '\0';
	}

	// When both folders exist, compare the volume and file id the file system
	// itself reports: that is immune to every way of spelling a path.
	for ( i = 0; i < 2; i ++ )
	{
		hDir [ i ] = CreateFile ( szDir [ i ], 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL );

		if ( hDir [ i ] == INVALID_HANDLE_VALUE || ! GetFileInformationByHandle ( hDir [ i ], & fileInfo [ i ] ) )
			bHaveInfo = FALSE;
	}

	for ( i = 0; i < 2; i ++ )
	{
		if ( hDir [ i ] != INVALID_HANDLE_VALUE )
			CloseHandle ( hDir [ i ] );
	}

	// FAT32, exFAT and some network redirectors keep no per-file id and report
	// zero for every entry, which would make two unrelated folders on one volume
	// compare equal. Only an id that is actually there says anything; a zero one
	// falls through to the text comparison below.
	if ( bHaveInfo
	  && ( fileInfo [ 0 ].nFileIndexHigh | fileInfo [ 0 ].nFileIndexLow ) != 0
	  && ( fileInfo [ 1 ].nFileIndexHigh | fileInfo [ 1 ].nFileIndexLow ) != 0 )
	{
		return ( fileInfo [ 0 ].dwVolumeSerialNumber == fileInfo [ 1 ].dwVolumeSerialNumber
			  && fileInfo [ 0 ].nFileIndexHigh == fileInfo [ 1 ].nFileIndexHigh
			  && fileInfo [ 0 ].nFileIndexLow == fileInfo [ 1 ].nFileIndexLow );
	}

	// One of them cannot be opened (it does not exist yet, or is not readable):
	// fall back to a text comparison with any short name expanded.
	for ( i = 0; i < 2; i ++ )
	{
		char	szLong [ MAX_PATH ];
		DWORD	dwLen = GetLongPathName ( szDir [ i ], szLong, MAX_PATH );

		if ( dwLen != 0 && dwLen < MAX_PATH )
			lstrcpyn ( szDir [ i ], szLong, MAX_PATH );
	}

	return ( _stricmp ( szDir [ 0 ], szDir [ 1 ] ) == 0 );
}

// Say why a save failed, naming the path and the reason Windows gave. Without
// this the exception unwinds to CWinThread::ProcessWndProcException and the
// user is told only "Command failed".
static void ReportCalibrationSaveFailure ( LPCSTR lpszPath, DWORD dwError )
{
	CString	strWhere ( lpszPath );
	CString	strMsg;
	LPSTR	lpszSysMsg = NULL;

	if ( dwError != 0
	  && FormatMessage ( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
						   NULL, dwError, 0, (LPSTR) & lpszSysMsg, 0, NULL ) != 0
	  && lpszSysMsg != NULL )
	{
		CString	strSysMsg ( lpszSysMsg );

		LocalFree ( lpszSysMsg );
		strSysMsg.TrimRight ( "\r\n" );

		if ( ! strSysMsg.IsEmpty () )
		{
			strWhere += "\n\n";
			strWhere += strSysMsg;
		}
	}

	strMsg.Format ( IDS_THC_SAVE_FAILED, (LPCSTR) strWhere );
	AfxMessageBox ( strMsg, MB_OK | MB_ICONEXCLAMATION );
}

void COneDeviceSensor::SaveCalibrationFile()
{
	BOOL			bContinue = FALSE;
	CString			strPath;
	CString			strSubDir;
	CString			strFileName;
	LPSTR			lpStr;
	DWORD			dwError;
	DWORD			dwAttributes;

	strPath = GetConfig () -> m_ApplicationPath;
	strSubDir = GetStandardSubDir ();
	if ( ! strSubDir.IsEmpty () )
	{
		strPath += strSubDir;
		strPath += '\\';

		// That subdirectory is the only place the sensor selection page looks
		// for training files, so create it before offering to save into it.
		// EnsurePathExists cannot report failure, and under an installation in
		// Program Files a standard user cannot create the folder at all, so ask
		// whether it is there afterwards: carrying on regardless would leave the
		// user in the reopen-until-cancel loop this is meant to end.
		SetLastError ( ERROR_SUCCESS );
		GetConfig () -> EnsurePathExists ( strPath );
		dwError = GetLastError ();

		dwAttributes = GetFileAttributes ( strPath );
		if ( dwAttributes == INVALID_FILE_ATTRIBUTES || ! ( dwAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
		{
			ReportCalibrationSaveFailure ( strPath, dwError );
			return;
		}
	}

	do
	{
		bContinue = FALSE;

		// Name the target folder in lpstrFile and not only in lpstrInitialDir:
		// from Vista on, lpstrInitialDir loses to the shell's last visited
		// folder once the dialog has been used, which would open the dialog
		// somewhere the file cannot be saved.
		CString	strInitialPath = strPath + strFileName;

		CFileDialog fileSaveDialog( FALSE, "thc", (LPCSTR) strInitialPath, OFN_HIDEREADONLY | OFN_NOCHANGEDIR, "Sensor Training File (*.thc)|*.thc||" );
		fileSaveDialog.m_ofn.lpstrInitialDir = (LPCSTR) strPath;

		if(fileSaveDialog.DoModal() == IDOK)
		{
			CString	strChosenPath = fileSaveDialog.GetPathName();
			CString	strChosenDir = strChosenPath.Left ( strChosenPath.ReverseFind ( '\\' ) + 1 );

			if ( IsSameDirectory ( strChosenDir, strPath ) )
			{
				CString	strPreviousName = m_CalibrationFileName;

				// The folder can exist and still refuse the write, which is what a
				// Program Files installation does to a standard user. Say so here
				// rather than letting the exception reach MFC's default handler.
				TRY
				{
					CFile saveFile(strChosenPath,CFile::modeCreate|CFile::modeWrite);
					CArchive ar(&saveFile,CArchive::store);
					CString	strTitle = saveFile.GetFileTitle ();

					// The archive records the correction this sensor was built from,
					// and this file is that correction, so it stores an empty name.
					m_CalibrationFileName.Empty();
					COneDeviceSensor::Serialize(ar);

					// Flush through the archive and the file while the exception can
					// still be caught: both destructors swallow a late write error.
					ar.Close ();
					saveFile.Close ();

					m_CalibrationFileName = strTitle;
				}
				CATCH_ALL ( e )
				{
					DWORD	dwSaveError = 0;

					// Nothing reached the disk, so keep the name the sensor already
					// had: the empty one above was only meant to reach the file.
					m_CalibrationFileName = strPreviousName;

					if ( e -> IsKindOf ( RUNTIME_CLASS ( CFileException ) ) )
						dwSaveError = (DWORD) ( (CFileException *) e ) -> m_lOsError;

					ReportCalibrationSaveFailure ( strChosenPath, dwSaveError );
				}
				END_CATCH_ALL
			}
			else
			{
				// Name the folder in the message: "current directory" on its own
				// leaves the user no way to tell where the file has to go.
				CString Msg;
				Msg.LoadString ( IDS_MUSTSAVEINSUBDIR );
				Msg += "\n\n";
				Msg += strPath;
				AfxMessageBox ( Msg );
				strFileName = fileSaveDialog.GetFileName();
				lpStr = strrchr ( (LPSTR)(LPCSTR) strFileName, '.' );
				if ( lpStr )
					lpStr [ 0 ] = '\0';
				bContinue = TRUE;
			}
		}
	} while ( bContinue );
}

