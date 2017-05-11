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
	SetDlgItemText(IDC_EDIT1, mpt::ToCString(mpt::String::Print(MPT_ULITERAL("%1:%2"), MPT_ULITERAL("localhost"), DEFAULT_PORT)));
	return TRUE;
}


void NetworkingDlg::OnConnect()
{
	CString s;
	mpt::ustring server, port;
	GetDlgItemText(IDC_EDIT1, s);
	if(s.Find(_T(':')) == -1)
	{
		server = mpt::ToUnicode(s);
		port = mpt::ToUString(DEFAULT_PORT);
	} else
	{
		auto split = mpt::String::Split<mpt::ustring>(mpt::ToUnicode(s), MPT_USTRING(":"));
		server = split[0];
		port = split[1];
	}
	new CollabClient(mpt::ToCharset(mpt::CharsetUTF8, server), mpt::ToCharset(mpt::CharsetUTF8, port));
}


}

OPENMPT_NAMESPACE_END
