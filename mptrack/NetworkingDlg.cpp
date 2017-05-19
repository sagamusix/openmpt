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
#include "NetworkTypes.h"
#include "../common/version.h"
#include "Reporting.h"

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
	ON_NOTIFY(NM_DBLCLK, IDC_LIST1, OnSelectDocument)

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
	m_List.SetExtendedStyle(m_List.GetExtendedStyle() | LVS_EX_FULLROWSELECT);

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


void NetworkingDlg::OnSelectDocument(NMHDR *pNMHDR, LRESULT *pResult)
{
	int index = reinterpret_cast<NM_LISTVIEW *>(pNMHDR)->iItem;
	if(index >= 0)
	{

	}
	*pResult = 0;
}


void NetworkingDlg::Receive(const std::string &msg)
{
	std::istringstream ss(msg);
	WelcomeMsg welcome;
	cereal::BinaryInputArchive inArchive(ss);
	inArchive >> welcome;

	if(welcome.version != MptVersion::str)
	{
		Reporting::Error(mpt::String::Print("The server is running a different version of OpenMPT. You must be using the same version as the server (%1).", welcome.version), "Collaboration Server");
		return;
	}

	m_List.SetRedraw(FALSE);
	m_List.DeleteAllItems();
	for(auto &doc : welcome.documents)
	{
		int insertAt = m_List.InsertItem(m_List.GetItemCount(), mpt::ToCString(mpt::CharsetUTF8, doc.name));
		if(insertAt == -1)
			continue;
		m_List.SetItemText(insertAt, 1, mpt::String::Print(_T("%1/%2"), doc.collaborators, doc.maxCollaboratos).c_str());
		m_List.SetItemData(insertAt, doc.id);	// TODO
	}
	m_List.SetRedraw(TRUE);
}


}

OPENMPT_NAMESPACE_END
