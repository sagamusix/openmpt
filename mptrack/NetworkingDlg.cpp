/*
 * NetworkingDlg.cpp
 * -----------------
 * Purpose: Collaborative Composing - Connection Dialog
 * Notes  : (currently none)
 * Authors: Johannes Schultz
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "resource.h"
#include "NetworkingDlg.h"
#include "Networking.h"

OPENMPT_NAMESPACE_BEGIN

namespace Networking
{

	
BEGIN_MESSAGE_MAP(NetworkingDlg, CDialog)
	//{{AFX_MSG_MAP(NetworkingDlg)
	//ON_COMMAND(IDC_BUTTON1,	OnStartServer)
	ON_COMMAND(IDC_BUTTON2,	OnConnect)

	//}}AFX_MSG_MAP

END_MESSAGE_MAP()


BOOL NetworkingDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	SetDlgItemText(IDC_EDIT1, _T("localhost:39999"));
	return TRUE;
}


void NetworkingDlg::OnConnect()
{

}


}

OPENMPT_NAMESPACE_END
