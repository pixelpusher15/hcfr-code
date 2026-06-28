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

// Generator.cpp: implementation of the CGenerator class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "GDIGenerator.h"
#include "Color.h"
#include "madTPG.h"
#include "Generator.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IMPLEMENT_SERIAL(CGenerator, CObject, 1) ;
dispwin *dw;
typedef int (__stdcall *RB8PG_send)(SOCKET sock,const char *message);
typedef int (__stdcall *RB8PG_close)(SOCKET sock);
typedef SOCKET (__stdcall *RB8PG_connect)(const char *server_addr);
typedef char * (__stdcall *RB8PG_discovery)();

CGenerator::CGenerator()
{
	m_isModified=FALSE;
	m_doScreenBlanking=GetConfig()->GetProfileInt("Generator","Blanking",0);
	m_rectSizePercent=GetConfig()->GetProfileInt("GDIGenerator","SizePercent",10);
	m_ccastIp = 0;
	sock = NULL;
	m_initShowedError = FALSE;
	rPi_xWidth = 1980;
	rPi_yHeight = 1080;
	rPi_memSize  = 0;
	AddPropertyPage(&m_GeneratorPropertiePage);

	CString str;
	str.LoadString(IDS_GENERATOR_PROPERTIES_TITLE);
	m_propertySheet.SetTitle(str);

	SetName("Not defined");  // Needs to be set for real generators
	m_blankingWindow.m_bDisableCursorHiding = TRUE;
	m_blankingWindow.m_bBlankScreen = TRUE;
	ccwin = dw;
}

CGenerator::~CGenerator()
{

}
void CGenerator::Copy(CGenerator * p)
{
	m_doScreenBlanking = p->m_doScreenBlanking;
	m_name = p->m_name;
	m_b16_235 = p->m_b16_235;
	m_busePic = p->m_busePic;
	m_bdispTrip = p->m_bdispTrip;
	m_bLinear = p->m_bLinear;
	m_brPi_user = p->m_brPi_user;
}

void CGenerator::Serialize(CArchive& archive)
{
	CObject::Serialize(archive) ;
	if (archive.IsStoring())
	{
		int version=6;
		archive << version;
		archive << m_doScreenBlanking;
		archive << m_b16_235;
		archive << m_busePic;
		archive << m_bdispTrip;
		archive << m_bLinear;
		archive << m_brPi_user;
	}
	else
	{
		int version;
		archive >> version;
		if ( version > 6 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		archive >> m_doScreenBlanking;
		if ( version > 1 )
			archive >> m_b16_235;
		if ( version > 2 )
			archive >> m_busePic;
		if ( version > 3 )
			archive >> m_bdispTrip;
		if ( version > 4 )
			archive >> m_bLinear;
		if ( version > 5 )
			archive >>	m_brPi_user;
	}
}

void CGenerator::SetPropertiesSheetValues()
{
	m_GeneratorPropertiePage.m_doScreenBlanking=m_doScreenBlanking;
}

void CGenerator::GetPropertiesSheetValues()
{
	if( m_doScreenBlanking != m_GeneratorPropertiePage.m_doScreenBlanking )
	{
		m_doScreenBlanking=m_GeneratorPropertiePage.m_doScreenBlanking;
		GetConfig()->WriteProfileInt("Generator","Blanking",m_doScreenBlanking);
		SetModifiedFlag(TRUE);
	}
}

void CGenerator::AddPropertyPage(CPropertyPageWithHelp *apPage)
{
	m_propertySheet.AddPage(apPage);
}


BOOL CGenerator::Configure()
{
	SetPropertiesSheetValues();
	m_propertySheet.SetActivePage(1);
	int result=m_propertySheet.DoModal();
	if(result == IDOK)
		GetPropertiesSheetValues();

	return result==IDOK;
}

static CString PgenParseVal(const char* resp, const char* name)
{
	if (!resp) return CString();
	int len = 0;
	while (resp[len] != 0 && (unsigned char)resp[len] >= 0x20) len++;
	CString r(resp, len);
	r.TrimLeft(); r.TrimRight();
	CString key(name); key += ":";
	int idx = r.Find(key);
	if (idx >= 0) { CString v = r.Mid(idx + key.GetLength()); v.TrimLeft(); v.TrimRight(); return v; }
	if (r.Left(3) == "OK:") { CString v = r.Mid(3); v.TrimLeft(); v.TrimRight(); return v; }
	int c = r.ReverseFind(':');
	if (c >= 0) { CString v = r.Mid(c + 1); v.TrimLeft(); v.TrimRight(); return v; }
	return r;
}

static char* PgSafeDisc(RB8PG_discovery f) { __try { return f(); } __except(EXCEPTION_EXECUTE_HANDLER) { return NULL; } }
static SOCKET PgSafeConn(RB8PG_connect f, char* ip) { __try { return f(ip); } __except(EXCEPTION_EXECUTE_HANDLER) { return (SOCKET)0; } }
static char* PgSafeGet(RB8PG_get f, SOCKET s, LPCSTR c) { __try { return f(s, c); } __except(EXCEPTION_EXECUTE_HANDLER) { return NULL; } }
static void PgSafeClose(RB8PG_close f, SOCKET s) { __try { f(s); } __except(EXCEPTION_EXECUTE_HANDLER) {} }
static CStringA PgSend(SOCKET s, LPCSTR cmd)
{
	CStringA out;
	if (!s) return out;
	CStringA frame(cmd);
	char term[2]; term[0] = 2; term[1] = 13;
	frame += CStringA(term, 2);
	if (send(s, (LPCSTR)frame, frame.GetLength(), 0) == SOCKET_ERROR) return out;
	DWORD tmo = 1000;
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tmo, sizeof(tmo));
	char buf[4096];
	for (int it = 0; it < 512 && out.Find((char)2) < 0; it++)
	{
		int n = recv(s, buf, sizeof(buf), 0);
		if (n <= 0) break;
		out += CStringA(buf, n);
	}
	int t = out.Find((char)2); if (t >= 0) out = out.Left(t);
	return out;
}

static HINSTANCE g_pgenLib = NULL;
static HINSTANCE PgenLib() { if (!g_pgenLib) g_pgenLib = LoadLibrary("RB8PGenerator.dll"); return g_pgenLib; }

static char g_pgenIp[64] = {0};
static char* PgDiscoverIP(RB8PG_discovery disc)
{
	if (g_pgenIp[0]) return g_pgenIp;
	char* ip = NULL;
	for (int i = 0; i < 3; i++) { ip = PgSafeDisc(disc); if (ip && strlen(ip) > 5) break; Sleep(150); }
	if (ip && strlen(ip) > 5) { strncpy(g_pgenIp, ip, 63); g_pgenIp[63] = 0; return g_pgenIp; }
	return NULL;
}

static SOCKET g_pgenSock = (SOCKET)0;
static SOCKET PgGetConn(RB8PG_connect conn, char* ip) { if (!g_pgenSock) g_pgenSock = PgSafeConn(conn, ip); return g_pgenSock; }
static void PgDropConn(RB8PG_close clsf) { if (g_pgenSock) { PgSafeClose(clsf, g_pgenSock); g_pgenSock = (SOCKET)0; } }
static SOCKET PgLiveConn(RB8PG_connect conn, RB8PG_close clsf, RB8PG_get getf, char* ip)
{
	for (int attempt = 0; attempt < 2; attempt++)
	{
		SOCKET s = PgGetConn(conn, ip);
		if (!s) return (SOCKET)0;
		CStringA v = PgSend(s, "CMD:GET_PGENERATOR_VERSION");
		if (v.Find("OK") >= 0) return s;
		PgDropConn(clsf);
	}
	return (SOCKET)0;
}

BOOL CGenerator::QueryPGeneratorInfo(CStringArray& vals, CString& err)
{
	err = _T("");
	vals.RemoveAll();
	for (int i = 0; i < 9; i++) vals.Add(_T("-"));

	HINSTANCE hLib = PgenLib();
	if (!hLib) { err.LoadString(IDS_PGEN_ST_NODLL); return FALSE; }
	RB8PG_discovery disc = (RB8PG_discovery)GetProcAddress(hLib, "RB8PG_discovery@0");
	RB8PG_connect   conn = (RB8PG_connect)GetProcAddress(hLib, "RB8PG_connect@4");
	RB8PG_get       getf = (RB8PG_get)GetProcAddress(hLib, "RB8PG_get@8");
	RB8PG_close     clsf = (RB8PG_close)GetProcAddress(hLib, "RB8PG_close@4");
	if (!disc || !conn || !getf || !clsf) { err.LoadString(IDS_PGEN_ST_NOENTRY); return FALSE; }

	char* ip = PgDiscoverIP(disc);
	if (!ip || strlen(ip) <= 5) { err.LoadString(IDS_PGEN_ST_NOTFOUND); return FALSE; }
	CString ipStr(ip);

	SOCKET s = PgLiveConn(conn, clsf, getf, ip);
	if (!s) { g_pgenIp[0] = 0; err.LoadString(IDS_PGEN_ST_NOCONNECT); return FALSE; }

	CString ver   = PgenParseVal(PgSend(s, "CMD:GET_PGENERATOR_VERSION"), "GET_PGENERATOR_VERSION");
	CString mode  = PgenParseVal(PgSend(s, "CMD:GET_MODE"), "GET_MODE");
	CString res   = PgenParseVal(PgSend(s, "CMD:GET_RESOLUTION"), "GET_RESOLUTION");
	CString fmt   = PgenParseVal(PgSend(s, "CMD:GET_PGENERATOR_CONF_COLOR_FORMAT"), "GET_PGENERATOR_CONF_COLOR_FORMAT");
	CString bpc   = PgenParseVal(PgSend(s, "CMD:GET_PGENERATOR_CONF_MAX_BPC"), "GET_PGENERATOR_CONF_MAX_BPC");
	CString outr  = PgenParseVal(PgSend(s, "CMD:GET_OUTPUT_RANGE"), "GET_OUTPUT_RANGE");
	CString quant = PgenParseVal(PgSend(s, "CMD:GET_PGENERATOR_CONF_RGB_QUANT_RANGE"), "GET_PGENERATOR_CONF_RGB_QUANT_RANGE");
	CString colm  = PgenParseVal(PgSend(s, "CMD:GET_PGENERATOR_CONF_COLORIMETRY"), "GET_PGENERATOR_CONF_COLORIMETRY");
	CString isHdr = PgenParseVal(PgSend(s, "CMD:GET_PGENERATOR_CONF_IS_HDR"), "GET_PGENERATOR_CONF_IS_HDR");
	CString isDov = PgenParseVal(PgSend(s, "CMD:GET_PGENERATOR_CONF_IS_LL_DOVI"), "GET_PGENERATOR_CONF_IS_LL_DOVI");
	CString host  = PgenParseVal(PgSend(s, "CMD:GET_HOSTNAME"), "GET_HOSTNAME");

	

	CString status = _T("SDR");
	if (isDov == "1") status = _T("Dolby Vision");
	else if (isHdr == "1") status = _T("HDR");

	CString resval = res;
	int hz = mode.Find("Hz");
	if (hz > 0)
	{
		int st = hz; while (st > 0 && mode[st - 1] != ' ') st--;
		CString rate = mode.Mid(st, hz - st + 2);
		resval = res.IsEmpty() ? rate : (res + _T(" @ ") + rate);
	}

	CString bd = bpc;
	if (!bd.IsEmpty()) bd += _T("-bit");

	CString cf = fmt;
	if (fmt == "0") cf = _T("RGB");
	else if (fmt == "1") cf = _T("YCbCr 4:4:4");
	else if (fmt == "2") cf = _T("YCbCr 4:2:2");

	CString cs = colm;
	if (colm == "0") cs = _T("Default");
	else if (colm == "2") cs = _T("BT.709 (YCC)");
	else if (colm == "9") cs = _T("BT.2020 (RGB)");

	CString range;
	if (quant == "1") range = _T("Limited");
	else if (quant == "2") range = _T("Full");
	else
	{
		CString ol = outr; ol.MakeLower();
		if (ol.Find("full") >= 0) range = _T("Full");
		else if (ol.Find("limit") >= 0) range = _T("Limited");
		else range = quant.IsEmpty() ? outr : quant;
	}

	if (host.IsEmpty()) host = _T("-");

	vals[0] = host;
	vals[1] = ipStr;
	vals[2] = ver;
	vals[3] = status;
	vals[4] = resval;
	vals[5] = bd;
	vals[6] = cs;
	vals[7] = cf;
	vals[8] = range;
	return TRUE;
}


BOOL CGenerator::ApplyPGeneratorConf(const CStringArray& cmds)
{
	if (cmds.GetSize() == 0) return TRUE;
	InvalidatePGenCache();
	HINSTANCE hLib = PgenLib();
	if (!hLib) return FALSE;
	RB8PG_discovery disc = (RB8PG_discovery)GetProcAddress(hLib, "RB8PG_discovery@0");
	RB8PG_connect   conn = (RB8PG_connect)GetProcAddress(hLib, "RB8PG_connect@4");
	RB8PG_get       getf = (RB8PG_get)GetProcAddress(hLib, "RB8PG_get@8");
	RB8PG_close     clsf = (RB8PG_close)GetProcAddress(hLib, "RB8PG_close@4");
	if (!disc || !conn || !getf || !clsf) { return FALSE; }

	char* ip = PgDiscoverIP(disc);
	if (!ip || strlen(ip) <= 5) { return FALSE; }

	SOCKET s = PgLiveConn(conn, clsf, getf, ip);
	if (!s) { g_pgenIp[0] = 0; return FALSE; }

	for (int i = 0; i < cmds.GetSize(); i++)
		PgSend(s, (LPCTSTR)cmds[i]);
	Sleep(150);
	PgSend(s, "RESTARTPGENERATOR:");
	PgDropConn(clsf);

	
	return TRUE;
}

// Decodes the base64 body of a PGenerator GET_MODES_AVAILABLE reply.
// Wire framing: OK: prefix + base64 payload, terminated by the STX (0x02) frame byte.
static CStringA PgenB64Decode(const CStringA& in)
{
	signed char rev[256];
	for (int i = 0; i < 256; i++) rev[i] = -1;
	const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	for (int i = 0; i < 64; i++) rev[(unsigned char)tbl[i]] = (signed char)i;
	CStringA out;
	int val = 0, bits = 0;
	for (int i = 0; i < in.GetLength(); i++)
	{
		int d = rev[(unsigned char)in[i]];
		if (d < 0) continue;
		val = (val << 6) | d;
		bits += 6;
		if (bits >= 8) { bits -= 8; out += (char)((val >> bits) & 0xFF); }
	}
	return out;
}

static BOOL g_pgenModeCacheValid = FALSE;
static CStringArray g_cacheLabels;
static CArray<int,int> g_cacheIds;
static int g_cacheCurMode = -1;
static PGenSettings g_cacheSt;
void CGenerator::InvalidatePGenCache() { g_pgenModeCacheValid = FALSE; }

int CGenerator::QueryPGeneratorModes(CStringArray& labels, CArray<int,int>& ids, PGenSettings& st)
{
	labels.RemoveAll();
	ids.RemoveAll();
	if (g_pgenModeCacheValid) { labels.Copy(g_cacheLabels); ids.Copy(g_cacheIds); st = g_cacheSt; return g_cacheCurMode; }
	st.valid = FALSE;
	int curId = -1;
	HINSTANCE hLib = PgenLib();
	if (!hLib) return -1;
	RB8PG_discovery disc = (RB8PG_discovery)GetProcAddress(hLib, "RB8PG_discovery@0");
	RB8PG_connect   conn = (RB8PG_connect)GetProcAddress(hLib, "RB8PG_connect@4");
	RB8PG_get       getf = (RB8PG_get)GetProcAddress(hLib, "RB8PG_get@8");
	RB8PG_close     clsf = (RB8PG_close)GetProcAddress(hLib, "RB8PG_close@4");
	if (!disc || !conn || !getf || !clsf) { return -1; }

	char* ip = PgDiscoverIP(disc);
	if (!ip || strlen(ip) <= 5) { return -1; }

	SOCKET s = PgLiveConn(conn, clsf, getf, ip);
	if (!s) { g_pgenIp[0] = 0; return -1; }

	PgSend(s, "CMD:GET_PGENERATOR_VERSION");

	CString cur(PgSend(s, "CMD:GET_MODE"));

	st.colorFormat = atoi((LPCTSTR)PgenParseVal(PgSend(s, "CMD:GET_PGENERATOR_CONF_COLOR_FORMAT"), "GET_PGENERATOR_CONF_COLOR_FORMAT"));
	st.quantRange  = atoi((LPCTSTR)PgenParseVal(PgSend(s, "CMD:GET_PGENERATOR_CONF_RGB_QUANT_RANGE"), "GET_PGENERATOR_CONF_RGB_QUANT_RANGE"));
	st.bitDepth    = atoi((LPCTSTR)PgenParseVal(PgSend(s, "CMD:GET_PGENERATOR_CONF_MAX_BPC"), "GET_PGENERATOR_CONF_MAX_BPC"));
	st.colorimetry = atoi((LPCTSTR)PgenParseVal(PgSend(s, "CMD:GET_PGENERATOR_CONF_COLORIMETRY"), "GET_PGENERATOR_CONF_COLORIMETRY"));
	st.isHdr       = atoi((LPCTSTR)PgenParseVal(PgSend(s, "CMD:GET_PGENERATOR_CONF_IS_HDR"), "GET_PGENERATOR_CONF_IS_HDR"));
	st.isLLDovi    = atoi((LPCTSTR)PgenParseVal(PgSend(s, "CMD:GET_PGENERATOR_CONF_IS_LL_DOVI"), "GET_PGENERATOR_CONF_IS_LL_DOVI"));
	st.eotf        = atoi((LPCTSTR)PgenParseVal(PgSend(s, "CMD:GET_PGENERATOR_CONF_EOTF"), "GET_PGENERATOR_CONF_EOTF"));
	st.primaries   = atoi((LPCTSTR)PgenParseVal(PgSend(s, "CMD:GET_PGENERATOR_CONF_PRIMARIES"), "GET_PGENERATOR_CONF_PRIMARIES"));
	st.doviMode    = atoi((LPCTSTR)PgenParseVal(PgSend(s, "CMD:GET_PGENERATOR_CONF_DV_MAP_MODE"), "GET_PGENERATOR_CONF_DV_MAP_MODE"));
	st.maxLuma     = atoi((LPCTSTR)PgenParseVal(PgSend(s, "CMD:GET_PGENERATOR_CONF_MAX_LUMA"), "GET_PGENERATOR_CONF_MAX_LUMA"));
	st.minLuma     = atoi((LPCTSTR)PgenParseVal(PgSend(s, "CMD:GET_PGENERATOR_CONF_MIN_LUMA"), "GET_PGENERATOR_CONF_MIN_LUMA"));
	st.maxCll      = atoi((LPCTSTR)PgenParseVal(PgSend(s, "CMD:GET_PGENERATOR_CONF_MAX_CLL"), "GET_PGENERATOR_CONF_MAX_CLL"));
	st.maxFall     = atoi((LPCTSTR)PgenParseVal(PgSend(s, "CMD:GET_PGENERATOR_CONF_MAX_FALL"), "GET_PGENERATOR_CONF_MAX_FALL"));
	st.valid = TRUE;

	// modes list last: the recv-drain can leave the socket mid-frame, so do it
	// after the single-frame conf reads above.
	CStringA rawA = PgSend(s, "CMD:GET_MODES_AVAILABLE");
	{ int ok = rawA.Find("OK:"); if (ok >= 0) rawA = rawA.Mid(ok + 3); }

	{ int t = cur.Find('\x02'); if (t >= 0) cur = cur.Left(t); }
	{ int c = cur.Find(':'); int b = cur.Find('['); if (c >= 0 && (b < 0 || c < b)) cur = cur.Mid(c + 1); }
	curId = atoi(cur);

	CString raw(PgenB64Decode(rawA));

	int pos = 0;
	while ((pos = raw.Find('[', pos)) >= 0)
	{
		int idStart = pos;
		while (idStart > 0 && raw[idStart - 1] >= '0' && raw[idStart - 1] <= '9') idStart--;
		int close = raw.Find(']', pos);
		if (close < 0) break;
		if (idStart < pos)
		{
			int id = atoi(raw.Mid(idStart, pos - idStart));
			CString desc = raw.Mid(pos + 1, close - pos - 1);
			CString lbl = desc;
			int sp = desc.Find(' ');
			if (sp > 0)
			{
				CString wh = desc.Left(sp);
				int hz = desc.Find("Hz");
				CString rate;
				if (hz > 0) { int stt = hz; while (stt > 0 && desc[stt - 1] != ' ') stt--; rate = desc.Mid(stt, hz - stt + 2); }
				lbl = rate.IsEmpty() ? wh : (wh + _T(" @ ") + rate);
			}
			labels.Add(lbl);
			ids.Add(id);
		}
		pos = close + 1;
	}
	if (st.valid) { g_cacheLabels.Copy(labels); g_cacheIds.Copy(ids); g_cacheSt = st; g_cacheCurMode = curId; g_pgenModeCacheValid = TRUE; }
	return curId;
}

BOOL CGenerator::SendPGeneratorCommand(LPCSTR cmd)
{
	InvalidatePGenCache();
	HINSTANCE hLib = PgenLib();
	if (!hLib) return FALSE;
	RB8PG_discovery disc = (RB8PG_discovery)GetProcAddress(hLib, "RB8PG_discovery@0");
	RB8PG_connect   conn = (RB8PG_connect)GetProcAddress(hLib, "RB8PG_connect@4");
	RB8PG_get       getf = (RB8PG_get)GetProcAddress(hLib, "RB8PG_get@8");
	RB8PG_close     clsf = (RB8PG_close)GetProcAddress(hLib, "RB8PG_close@4");
	if (!disc || !conn || !getf || !clsf) { return FALSE; }

	char* ip = PgDiscoverIP(disc);
	if (!ip || strlen(ip) <= 5) { return FALSE; }

	SOCKET s = PgLiveConn(conn, clsf, getf, ip);
	if (!s) { g_pgenIp[0] = 0; return FALSE; }

	PgSend(s, cmd);
	Sleep(150);
	PgDropConn(clsf);

	
	return TRUE;
}

BOOL CGenerator::Init(UINT nbMeasure, bool isSpecial)
{
	nMeasureNumber = nbMeasure; 
	m_initShowedError = FALSE;
	CGDIGenerator Cgen;
	CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);
	BOOL madVR_Found;
	const char *	m_piIP = "";
	if (m_name != str)
	{
		if (Cgen.m_nDisplayMode == DISPLAY_rPI)
		{
			int x2 = Cgen.m_offsetx;
			int y2 = Cgen.m_offsety;
			if (sock)
			{
				CStringA _vp = PgSend(sock, "CMD:GET_PGENERATOR_VERSION");
				DWORD _rt = 0; setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&_rt, sizeof(_rt));
				if (_vp.Find("OK") < 0)
				{
					PgSafeClose(_RB8PG_close, sock);
					sock = (SOCKET)0;
				}
			}
			if (!sock) //initialization
			{
				hInstLibrary = LoadLibrary("RB8PGenerator.dll");
				if (hInstLibrary)
				{
					_RB8PG_discovery = (RB8PG_discovery)GetProcAddress(hInstLibrary, "RB8PG_discovery@0");
					_RB8PG_connect = (RB8PG_connect)GetProcAddress(hInstLibrary, "RB8PG_connect@4");
					_RB8PG_get = (RB8PG_get)GetProcAddress(hInstLibrary, "RB8PG_get@8");
					_RB8PG_send = (RB8PG_send)GetProcAddress(hInstLibrary, "RB8PG_send@8");
					_RB8PG_close = (RB8PG_close)GetProcAddress(hInstLibrary, "RB8PG_close@4");

					if (_RB8PG_discovery)
					{
						for (int i = 0; i < 2; i++)
						{
							m_piIP = _RB8PG_discovery();
							if (m_piIP && strlen(m_piIP) > 5)
								break;
						}
					}
								
					if(m_piIP && strlen(m_piIP) > 5)
					{
						CString cs = m_piIP;
						if (_RB8PG_connect)
							sock = _RB8PG_connect(m_piIP);
						else
						{
							m_initShowedError = TRUE, GetColorApp()->InMeasureMessageBox( "Error connecting with PGenerator: "+cs, "Error", MB_ICONINFORMATION);
							return false;
						}

						if (!sock)
						{
							m_initShowedError = TRUE, GetColorApp()->InMeasureMessageBox( "    ** PGenerator not found on the network **", "Error", MB_ICONERROR);
							return false;
						}
						{
							if (_RB8PG_send)
							{
								pi_Res = _RB8PG_get(sock,"CMD:GET_RESOLUTION");
							
								CString cs1(pi_Res);
								CString cs2,cs3,xW,yH;
								AfxExtractSubString(cs2, cs1, 0, ':');
								if (cs2 != "OK")
								{
									m_initShowedError = TRUE, GetColorApp()->InMeasureMessageBox( "Failed to get PGenerator resolution", "GET_RESOLUTION", MB_ICONINFORMATION);
									return false;
								}
								AfxExtractSubString(cs3, cs1, 1, ':');
								AfxExtractSubString(xW, cs3, 0, 'x');
							
								int xsize = xW.GetLength();
								yH = cs3.Mid(xsize+1);

								rPi_xWidth = atoi(xW);
								rPi_yHeight = atoi(yH);

								pi_Res = _RB8PG_get(sock,"CMD:GET_GPU_MEMORY");
								CString cs4(pi_Res),cs5,cs6;
								AfxExtractSubString(cs5, cs4, 0, ':');
								if (cs5 != "OK")
								{
									m_initShowedError = TRUE, GetColorApp()->InMeasureMessageBox( "Failed to get PGenerator GPU memory size", "GET_GPU_MEMORY", MB_ICONINFORMATION);
									return false;
								}
								cs6=cs4.Mid(3);
								cs6.Remove('M');
								rPi_memSize = atoi(cs6);
								GetConfig()->WriteProfileInt("GDIGenerator", "rPiGPU", rPi_memSize);
								GetConfig()->WriteProfileInt("GDIGenerator", "rPiWidth", rPi_xWidth);
								GetConfig()->WriteProfileInt("GDIGenerator", "rPiHeight", rPi_yHeight);
								CString msg;
								msg.Format("RGB=TEXT;12,0;100;16,128,128;0,0,0;100,300;Initializing PGenerator at: "+cs+" Res [%dx%d], GPU Mem [%dM]",rPi_xWidth, rPi_yHeight,rPi_memSize);
								_RB8PG_send(sock,msg);
								Sleep(3000);
								if (rPi_memSize >= 192)
								{
									_RB8PG_send(sock,"RGB=IMAGE;1920,1080;100;255,255,255;0,0,0;-1,-1;/var/lib/PGenerator/images-HCFR/gbramp.png");
									Sleep(1000);
								}
								if (m_bdispTrip)
								{
									CGDIGenerator Cgen;
									double bgstim = Cgen.m_bgStimPercent / 100.;
									int rb,gb,bb;

									if (m_b16_235)
									{
										rb = floor(bgstim * 219.0 + 16.5);
										gb = floor(bgstim * 219.0 + 16.5);
										bb = floor(bgstim * 219.0 + 16.5);
										rb=min(max(rb,0),235);
										gb=min(max(gb,0),235);
										bb=min(max(bb,0),235);
									}
									else
									{
										rb = floor(bgstim * 255.0 + 0.5);
										gb = floor(bgstim * 255.0 + 0.5);
										bb = floor(bgstim * 255.0 + 0.5);
										rb=min(max(rb,0),255);
										gb=min(max(gb,0),255);
										bb=min(max(bb,0),255);
									}

									if (x2 > 0)
										x2 = min(x2, rPi_xWidth / 2. - pow(Cgen.m_rectSizePercent/100.0,0.5) * rPi_xWidth / 2. );
									else
										x2 = max(x2, -1*(rPi_xWidth / 2. - pow(Cgen.m_rectSizePercent/100.0,0.5) * rPi_xWidth / 2.) );
									if (y2 > 0)
										y2 = min(y2, rPi_yHeight / 2. - pow(Cgen.m_rectSizePercent/100.0,0.5) * rPi_yHeight / 2.);
									else
										y2 = max(y2, -1*(rPi_yHeight / 2. - pow(Cgen.m_rectSizePercent/100.0,0.5) * rPi_yHeight / 2.) );

									CString templ;
									int x1 = (int)(pow((double)(Cgen.m_rectSizePercent)/100.0,0.5) * rPi_xWidth);
									int y1 = (int)(pow((double)(Cgen.m_rectSizePercent)/100.0,0.5) * rPi_yHeight);
									double t_fact = rPi_xWidth / 1920.; 
									templ.Format("SETCONF:HCFR:TEMPLATERAMDISK:DRAW=TEXT\nDIM=18,0\nRESOLUTION=100\nRGB=20,128,128\nBG=DYNAMIC\n" \
										"POSITION=%d,20\nTEXT=RGB Triplet $RGB\nEND=1\n" \
										"DRAW=RECTANGLE\nDIM=%d,%d\nRESOLUTION=100\n" \
										"RGB=DYNAMIC\nBG=-1,-1,-1\nPOSITION=-1,-1,%d,%d\nEND=1",rPi_xWidth / 2 - int(175 * t_fact),x1,y1,x2,y2);
//									templ.Format("SETCONF:HCFR:TEMPLATERAMDISK:DRAW=TEXT\nDIM=18,0\nRESOLUTION=100\nRGB=20,128,128\nBG=20,40,60\n" \
//										"POSITION=%d,20\nTEXT=RGB Triplet $RGB\nEND=1\n" \
//										"DRAW=RECTANGLE\nDIM=%d,%d\nRESOLUTION=100\n" \
//										"RGB=DYNAMIC\nBG=DYNAMIC\nPOSITION=-1,-1,%d,%d\nEND=1",rPi_xWidth / 2 - int(175 * t_fact),x1,y1,x2,y2);
							
									CGenerator::_RB8PG_send(sock,templ);
								}
							}
							else
							{
								m_initShowedError = TRUE, GetColorApp()->InMeasureMessageBox( "Error communicating with PGenerator", "Error", MB_ICONINFORMATION);
								return false;
							}
						}
					}
					else
					{
						m_initShowedError = TRUE, GetColorApp()->InMeasureMessageBox( "    ** PGenerator not found on the network **", "Error", MB_ICONERROR);
						OutputDebugString("    ** RB8PG_discovery failed **");
						return false;
					}			
				}
				else
				{
					m_initShowedError = TRUE, GetColorApp()->InMeasureMessageBox( "    ** RB8PGenerator.dll not found **", "Error", MB_ICONERROR);
					OutputDebugString("    ** Load_dll failed **");
					return false;
				}
			}
			else //in case template needs updating
			{
				if (m_bdispTrip || x2 !=0 || y2 != 0)
				{
					CGDIGenerator Cgen;
					double bgstim = Cgen.m_bgStimPercent / 100.;
					int rb,gb,bb;
					if (m_b16_235)
					{
						rb = floor(bgstim * 219.0 + 16.5);
						gb = floor(bgstim * 219.0 + 16.5);
						bb = floor(bgstim * 219.0 + 16.5);
						rb=min(max(rb,0),235);
						gb=min(max(gb,0),235);
						bb=min(max(bb,0),235);
					}
					else
					{
						rb = floor(bgstim * 255.0 + 0.5);
						gb = floor(bgstim * 255.0 + 0.5);
						bb = floor(bgstim * 255.0 + 0.5);
						rb=min(max(rb,0),255);
						gb=min(max(gb,0),255);
						bb=min(max(bb,0),255);
					}
					CString templ;
					int x1 = (int)(pow((double)(Cgen.m_rectSizePercent) / 100.0, 0.5) * rPi_xWidth);
					int y1 = (int)(pow((double)(Cgen.m_rectSizePercent) / 100.0, 0.5) * rPi_yHeight);

					if (x2 > 0)
						x2 = (int) min(x2, rPi_xWidth / 2. - pow(Cgen.m_rectSizePercent/100.0,0.5) * rPi_xWidth / 2.);
					else
						x2 = (int) max(x2, -1*(rPi_xWidth / 2. - pow(Cgen.m_rectSizePercent/100.0,0.5) * rPi_xWidth / 2. )); // Omardris
					if (y2 > 0)
						y2 = (int) min(y2, rPi_yHeight / 2. - pow(Cgen.m_rectSizePercent/100.0,0.5) * rPi_yHeight / 2.);
					else
						y2 = (int) max(y2, -1*(rPi_yHeight / 2. - pow(Cgen.m_rectSizePercent/100.0,0.5) * rPi_yHeight / 2.));

					double t_fact = rPi_xWidth / 1920.; 
					templ.Format("SETCONF:HCFR:TEMPLATERAMDISK:DRAW=TEXT\nDIM=18,0\nRESOLUTION=100\nRGB=20,128,128\nBG=DYNAMIC\n" \
						"POSITION=%d,20\nTEXT=RGB Triplet $RGB\nEND=1\n" \
						"DRAW=RECTANGLE\nDIM=%d,%d\nRESOLUTION=100\n" \
						"RGB=DYNAMIC\nBG=-1,-1,-1\nPOSITION=-1,-1,%d,%d\nEND=1",rPi_xWidth / 2 - int(175 * t_fact),x1,y1,x2,y2);
//					templ.Format("SETCONF:HCFR:TEMPLATERAMDISK:DRAW=TEXT\nDIM=18,0\nRESOLUTION=100\nRGB=20,128,128\nBG=20,40,60\n" \
//						"POSITION=%d,20\nTEXT=RGB Triplet $RGB\nEND=1\n" \
//						"DRAW=RECTANGLE\nDIM=%d,%d\nRESOLUTION=100\n" \
//						"RGB=DYNAMIC\nBG=DYNAMIC\nPOSITION=-1,-1,%d,%d\nEND=1",rPi_xWidth / 2 - int(175 * t_fact),x1,y1,x2,y2);
				
					CGenerator::_RB8PG_send(sock,templ);
				}
			}
		}
		else if (Cgen.m_nDisplayMode == DISPLAY_ccast)
		{
			OutputDebugString("CGenerator::Init");
			m_ccastIp = GetConfig()->GetProfileInt("GDIGenerator", "CCastIp", 0);
			CGoogleCastWrapper GCast;
			GCast.RefreshList();
			if (GCast.getCount() == 0)
			{
				m_initShowedError = TRUE, GetColorApp()->InMeasureMessageBox( "    ** No ChromeCasts found **", "Error", MB_ICONERROR);
				OutputDebugString("    ** No ChromeCasts found **");
				return false;
			} else 
			{
				const ccast_id *id = m_ccastIp ? GCast.getCcastByIp(m_ccastIp) : GCast[0];
				if (id == NULL && (id = GCast[0]) == NULL)
				{
					m_initShowedError = TRUE, GetColorApp()->InMeasureMessageBox( "    ** Error discovering ChromeCasts **", "Error", MB_ICONERROR);
					OutputDebugString("    ** Error discovering ChromeCasts **");
					return false;
				}
				else 
				{
					OutputDebugString("Casting to: ");OutputDebugString(id->name);
					double rx = sqrt( double( (double)Cgen.m_rectSizePercent / 100.));
					dw = new_ccwin((ccast_id *)id, 1000.0 * rx  , 565.0 * rx, 0.0, 0.0, 0, 0.1234);
					if (dw == NULL) 
					{
						m_initShowedError = TRUE, GetColorApp()->InMeasureMessageBox( id->name, "new_ccwin failed!", MB_ICONERROR);
						OutputDebugString("new_ccwin failed! ");OutputDebugString(id->name);
						return -1;
					} 
					ccwin = dw;				
				}
			}
		} else if (Cgen.m_nDisplayMode == DISPLAY_madVR)
		{
			if (madVR_IsAvailable())
			{
				int nSettling=GetConfig()->m_isSettling?26:0;
				madVR_Found = madVR_Connect(CM_ConnectToLocalInstance, CM_ConnectToLanInstance, CM_StartLocalInstance  );
				if (m_madVR_vLUT)
					madVR_SetDeviceGammaRamp(NULL);
				if (m_madVR_3d)
					madVR_Disable3dlut();
				if (m_madVR_OSD)
					madVR_ShowProgressBar(nMeasureNumber + nSettling);
				madVR_SetHdrButton(m_madVR_HDR);
				if (m_madVR_HDR)
					madVR_SetHdrMetadata(GetConfig()->m_manualRedx, GetConfig()->m_manualRedy, GetConfig()->m_manualGreenx, GetConfig()->m_manualGreeny, GetConfig()->m_manualBluex, GetConfig()->m_manualBluey, GetConfig()->m_manualWhitex, GetConfig()->m_manualWhitey, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_ContentMaxL, GetConfig()->m_FrameAvgMaxL);
			}
			else
			{
				m_initShowedError = TRUE, GetColorApp()->InMeasureMessageBox( "madVR dll not found, is madVR installed?", "Error", MB_ICONERROR);
				return false;
			}
		} else
		{
			CGenerator *	m_pGenerator;
			CGenerator *	 t1;			
			m_pGenerator=new CGDIGenerator();
			t1 = m_pGenerator;
		}
	}

	if(m_doScreenBlanking)
	{
		m_blankingWindow.DisplayRGBColor(ColorRGBDisplay(0.0), TRUE);	// show black screen
		Sleep(50);
		m_blankingWindow.DisplayRGBColor(ColorRGBDisplay(0.0), TRUE);	// show black screen again to be sure task bar is hidden
	}
	
	GetColorApp() -> BeginMeasureCursor ();

	return TRUE;
}

BOOL CGenerator::DisplayRGBColorrPI(const ColorRGBDisplay& aRGBColor, MeasureType nPatternType, UINT nPatternInfo )
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayRGBColormadVR(const ColorRGBDisplay& aRGBColor, MeasureType nPatternType, UINT nPatternInfo )
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayRGBCCast(const ColorRGBDisplay& aRGBColor, MeasureType nPatternType, UINT nPatternInfo )
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayRGBColor(const ColorRGBDisplay& aRGBColor, MeasureType nPatternType, UINT nPatternInfo,  BOOL bChangePattern,BOOL bSilentMode)
{
	return TRUE;	  // need to be overriden
}


BOOL CGenerator::DisplayAnsiBWRects(BOOL bInvert)
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayAnimatedBlack()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayAnimatedWhite()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayGradient()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayRG()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayRB()
{
	return TRUE;	  // need to be overriden
}
BOOL CGenerator::DisplayGB()
{
	return TRUE;	  // need to be overriden
}
BOOL CGenerator::DisplayRGd()
{
	return TRUE;	  // need to be overriden
}
BOOL CGenerator::DisplayRBd()
{
	return TRUE;	  // need to be overriden
}
BOOL CGenerator::DisplayGBd()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayGradient2()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayLramp()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayGranger()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::Display80()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTV()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTV2()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplaySpectrum()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplaySramp()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayVSMPTE()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayEramp()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTC0()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTC1()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTC2()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTC3()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTC4()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTC5()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayBN()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayDR0()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayDR1()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayDR2()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayAlign()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayAlign2()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayUser1()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayUser2()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayUser3()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayUser4()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayUser5()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayUser6()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplaySharp()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayClipH()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayClipHO()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayNBO()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayClipL()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayClipLO()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTestimg()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayISO12233()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayNB()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayBBCHD()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayCROSSl()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayCROSSd()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayPM5644()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayZONE()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::CanDisplayAnsiBWRects()
{
	return FALSE;	  // need to be overriden if display AnsiBWRects is implemented
}

BOOL CGenerator::DisplayGray(double aLevel, MeasureType nPatternType , BOOL bChangePattern)
{
	return DisplayRGBColor(ColorRGBDisplay(aLevel), nPatternType ,bChangePattern); 
}


BOOL CGenerator::Release(INT nbNext)
{
	CGDIGenerator Cgen;

	if (Cgen.m_nDisplayMode == DISPLAY_madVR)
	{
	  if (madVR_IsAvailable())
	    madVR_Disconnect();
	} else if (Cgen.m_nDisplayMode == DISPLAY_ccast && dw)
		dw->del(dw);

	if (sock && Cgen.m_nDisplayMode == DISPLAY_rPI)
	{
			if (_RB8PG_send)
			{
				CString msg;
				if (nbNext == -1)
				{
					msg.Format("RGB=TEXT;14,0;100;16,128,128;0,0,0;100,300;End of sequence");
					_RB8PG_send(sock,msg);
					Sleep(2000);
				}
				_RB8PG_send(sock,"TESTTEMPLATE:PatternDynamic:0,0,0");
			}
			else
				m_initShowedError = TRUE, GetColorApp()->InMeasureMessageBox( "Error communicating with PGenerator", "Error", MB_ICONINFORMATION);
	}

	if (sock && Cgen.m_nDisplayMode != DISPLAY_rPI && hInstLibrary) //disconnect only after generator change
	{

			if (_RB8PG_send)
			{
				CString msg;
				msg.Format("RGB=TEXT;14,0;100;16,128,128;0,0,0;100,300;Disconnecting from PGenerator");
				_RB8PG_send(sock,msg);
				Sleep(3000);
				_RB8PG_send(sock,"TESTTEMPLATE:PatternDynamic:0,0,0");
			}
			else
				m_initShowedError = TRUE, GetColorApp()->InMeasureMessageBox( "Error communicating with PGenerator", "Error", MB_ICONINFORMATION);

			if (_RB8PG_close)
				_RB8PG_close(sock);
			else
				m_initShowedError = TRUE, GetColorApp()->InMeasureMessageBox( "Error communicating with PGenerator", "Error", MB_ICONINFORMATION);

			sock = NULL;
			GetConfig()->WriteProfileInt("GDIGenerator", "rPiGPU", 0);
			FreeLibrary(hInstLibrary);
	}

	if(m_doScreenBlanking)
		m_blankingWindow.Hide();

	return TRUE;
}

BOOL CGenerator::ChangePatternSeries()
{
	return TRUE;
}
