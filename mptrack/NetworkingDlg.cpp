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
		{ _T("Module"),			140, LVCFMT_LEFT },
		{ _T("Collaborators"),	90, LVCFMT_LEFT },
		{ _T("Spectators"),		90, LVCFMT_LEFT },
		{ _T("Password"),		80, LVCFMT_LEFT },
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
		uint64 itemID = m_itemID[index];
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
		m_List.SetItemText(insertAt, 2, mpt::String::Print(_T("%1/%2"), doc.spectators, doc.maxSpectators).c_str());
		m_List.SetItemText(insertAt, 3, doc.password ? _T("Yes") : _T("No"));
		m_itemID[insertAt] = doc.id;
	}
	m_List.SetRedraw(TRUE);
}


void SharingDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CModTypeDlg)
	DDX_Control(pDX, IDC_SPIN1, m_CollaboratorsSpin);
	DDX_Control(pDX, IDC_SPIN2, m_SpectatorsSpin);
	//}}AFX_DATA_MAP
}


BOOL SharingDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	m_CollaboratorsSpin.SetRange(0, 99);
	m_SpectatorsSpin.SetRange(0, 99);
	SetDlgItemInt(IDC_EDIT1, 1);
	SetDlgItemInt(IDC_EDIT2, 0);
	return TRUE;
}


void SharingDlg::OnOK()
{
	CDialog::OnOK();
	int collaborators = GetDlgItemInt(IDC_EDIT1), spectators = GetDlgItemInt(IDC_EDIT2);
	CString password;
	GetDlgItemText(IDC_EDIT3, password);
	if(collabServer == nullptr)
	{
		collabServer = std::make_shared<Networking::CollabServer>();
		collabServer->StartAccept();
	}

	collabServer->AddDocument(m_ModDoc, collaborators, spectators, mpt::ToUnicode(password));
}



}

OPENMPT_NAMESPACE_END
