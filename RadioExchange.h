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

// RadioExchange.h: exchange a radio-button group by control ID.
//
// MFC's DDX_Radio does NOT identify a radio button by its control ID. It walks
// the dialog template from the WS_GROUP control and stores the button's ORDINAL
// POSITION within that group (atlmfc/src/mfc/dlgdata.cpp). That is safe in an
// application with one copy of each dialog. HCFR is not one: every dialog exists
// five times over, once per language satellite (CHCFR21_*.rc), and those copies
// are maintained by hand.
//
// When two satellites list the same buttons in a different order, the same
// stored value means two different settings. That is not hypothetical - the
// German and French copies of IDD_REFERENCE_PROP_PAGE listed the PQ and HLG
// radios in the opposite order from the other three, so a German or French user
// who selected PQ (SMPTE ST 2084) got HLG written to the configuration, while
// English, Italian and Spanish worked correctly. Nothing warned: each template
// is individually valid, the compiler cannot see across resource files, and
// MFC's only guard is a Debug-only ASSERT that this case does not trip.
//
// DDX_RadioByID stores the same value DDX_Radio did - the index into the caller's
// ID list - so persisted settings keep their meaning. What changes is that the
// index is resolved against an explicit list of control IDs written in the code
// rather than against whatever order a translator's resource editor happened to
// leave behind. List the IDs in the order the ENGLISH template declares them,
// which is the order that defines every value already written to an INI.
//
// After touching any .rc file, confirm the five satellites still list each
// dialog's radio buttons in the same order: diff each dialog's radio-group ID
// sequence against CHCFR21_ENGLISH.rc. The ID lists below assume it.

#if !defined(AFX_RADIOEXCHANGE_H__INCLUDED_)
#define AFX_RADIOEXCHANGE_H__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// Exchange one group of radio buttons, identified by control ID.
//
// On save, value becomes the index of the checked button within pIDs. If no
// button is checked, value is LEFT ALONE - unlike DDX_Radio, which wrote -1 into
// the member and so could quietly destroy a valid setting.
// On load, the button at pIDs[value] is checked and the others are cleared.
template<int N>
inline void DDX_RadioByID(CDataExchange* pDX, int& value, const UINT (&pIDs)[N])
{
	ASSERT(pDX != NULL);

	// Match DDX_Radio: the group's first control is the one a failed validation
	// puts the focus on.
	pDX->PrepareCtrl(pIDs[0]);

	CWnd* pDlgWnd = pDX->m_pDlgWnd;
	if (pDlgWnd == NULL)
		return;

	if (pDX->m_bSaveAndValidate)
	{
		for (int i = 0; i < N; i++)
		{
			CButton* pButton = (CButton*)pDlgWnd->GetDlgItem(pIDs[i]);
			ASSERT(pButton != NULL);	// a listed ID missing from this satellite's template
			if (pButton != NULL && pButton->GetCheck() == BST_CHECKED)
			{
				value = i;
				return;
			}
		}
	}
	else
	{
		for (int i = 0; i < N; i++)
		{
			CButton* pButton = (CButton*)pDlgWnd->GetDlgItem(pIDs[i]);
			ASSERT(pButton != NULL);
			if (pButton != NULL)
				pButton->SetCheck((i == value) ? BST_CHECKED : BST_UNCHECKED);
		}
	}
}

#endif // !defined(AFX_RADIOEXCHANGE_H__INCLUDED_)
