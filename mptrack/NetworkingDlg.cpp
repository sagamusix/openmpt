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
#include <picojson/picojson.h>

OPENMPT_NAMESPACE_BEGIN

namespace Networking
{

std::shared_ptr<NetworkingDlg> dialogInstance;

void NetworkingDlg::Show(CWnd *parent)
{
	if(dialogInstance == nullptr)
	{
		dialogInstance = std::make_shared<NetworkingDlg>();
		dialogInstance->Create(IDD_NETWORKING, parent);
	}
	dialogInstance->ShowWindow(SW_SHOW);
	dialogInstance->BringWindowToTop();
}



BEGIN_MESSAGE_MAP(NetworkingDlg, CDialog)
	//{{AFX_MSG_MAP(NetworkingDlg)
	//ON_COMMAND(IDC_BUTTON1,	OnStartServer)
	ON_COMMAND(IDC_BUTTON2,	OnConnect)

	//}}AFX_MSG_MAP

END_MESSAGE_MAP()



void NetworkingDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CModTypeDlg)
	DDX_Control(pDX, IDC_LIST1, m_List);
	//}}AFX_DATA_MAP
}


void NetworkingDlg::PostNcDestroy()
{
	CDialog::PostNcDestroy();
	dialogInstance = nullptr;
}


BOOL NetworkingDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	SetDlgItemText(IDC_EDIT1, _T("localhost"));
	SetDlgItemInt(IDC_EDIT2, DEFAULT_PORT);

	const CListCtrlEx::Header headers[] =
	{
		{ _T("Module"),			310, LVCFMT_LEFT },
		{ _T("Collaborators"),	90, LVCFMT_LEFT },
	};
	m_List.SetHeaders(headers);

	return TRUE;
}


void NetworkingDlg::OnConnect()
{
	CString server;
	GetDlgItemText(IDC_EDIT1, server);
	auto port = GetDlgItemInt(IDC_EDIT2);
	if(port == 0)
		port = DEFAULT_PORT;

	collabClients.push_back(std::make_shared<CollabClient>(mpt::ToCharset(mpt::CharsetUTF8, server), mpt::ToString(port), dialogInstance));
	collabClients.back()->Connect();
}


void NetworkingDlg::Receive(const std::string &msg)
{
	picojson::value root;
	if(picojson::parse(root, msg).empty() && root.is<picojson::object>())
	{
		auto rootObj = root.get<picojson::object>();
		if(rootObj["modules"].is<picojson::array>())
		{
			auto modules = rootObj["modules"].get<picojson::array>();
			for(auto &m : modules)
			{
				auto module = m.get<picojson::object>();
			}
		}
	}
}


}

OPENMPT_NAMESPACE_END
