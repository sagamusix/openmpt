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
#include "dlg_misc.h"
#include "Mptrack.h"
#include "Moddoc.h"
#include "Mainfrm.h"
#include "Childfrm.h"
#include "View_tre.h"
#include "../common/version.h"
#include "../soundlib/plugins/PlugInterface.h"
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
	ON_COMMAND(IDC_BUTTON2,	OnConnect)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST1, OnSelectDocument)
	ON_MESSAGE(WM_USER + 100, OnOpenDocument)
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

	m_client = std::make_shared<RemoteCollabClient>(mpt::ToCharset(mpt::CharsetUTF8, server), mpt::ToString(port), dialogInstance);
	if(!m_client->Connect())
	{
		Reporting::Error(_T("Unable to connect to ") + server);
		m_client = nullptr;
		return;
	}
	std::ostringstream ss;
	cereal::BinaryOutputArchive ar(ss);
	ar(ListMsg);
	m_client->Write(ss.str());
}


void NetworkingDlg::OnSelectDocument(NMHDR *pNMHDR, LRESULT *pResult)
{
	int item = reinterpret_cast<NM_LISTVIEW *>(pNMHDR)->iItem;
	if(item < 0)
		return;
	DocumentInfo *doc = reinterpret_cast<DocumentInfo *>(m_List.GetItemData(item));
	if(doc != nullptr)
	{
		JoinMsg join;
		join.id = doc->id;
		join.accessType = 0;	// Collaborator
		join.password = "";
		if(doc->password)
		{
			CInputDlg dlg(this, _T("Password required for joining this collaboration session:"), _T(""));
			if(dlg.DoModal() != IDOK)
				return;
			join.password = mpt::ToCharset(mpt::CharsetUTF8, dlg.resultAsString);
		}

		std::ostringstream ss;
		cereal::BinaryOutputArchive ar(ss);
		ar(ConnectMsg);
		ar(join);
		m_client->Write(ss.str());
	}
	*pResult = 0;
}


void NetworkingDlg::Receive(std::shared_ptr<CollabConnection>, std::stringstream &msg)
{
	cereal::BinaryInputArchive inArchive(msg);
	NetworkMessage type;
	inArchive >> type;
	if(type == ListMsg)
	{
		WelcomeMsg welcome;
		inArchive >> welcome;

		if(welcome.version != MptVersion::str)
		{
			Reporting::Error(mpt::format("The server is running a different version of OpenMPT. You must be using the same version as the server (%1).")(welcome.version), "Collaboration Server");
			return;
		}

		m_List.SetRedraw(FALSE);
		m_List.DeleteAllItems();
		m_docs = std::move(welcome.documents);
		for(const auto &doc : m_docs)
		{
			int insertAt = m_List.InsertItem(m_List.GetItemCount(), mpt::ToCString(mpt::CharsetUTF8, doc.name));
			if(insertAt == -1)
				continue;
			m_List.SetItemText(insertAt, 1, mpt::format(_T("%1/%2"))(doc.collaborators, doc.maxCollaboratos).c_str());
			m_List.SetItemText(insertAt, 2, mpt::format(_T("%1/%2"))(doc.spectators, doc.maxSpectators).c_str());
			m_List.SetItemText(insertAt, 3, doc.password ? _T("Yes") : _T("No"));
			m_List.SetItemData(insertAt, reinterpret_cast<DWORD_PTR>(&doc));
		}
		m_List.SetRedraw(TRUE);
	} else if(type == DocNotFoundMsg)
	{
		Reporting::Error("The document you wanted to join was not found.");
	} else if(type == WrongPasswordMsg)
	{
		Reporting::Error("The password was incorrect.");
	} else if(type == NoMoreClientsMsg)
	{
		Reporting::Error("No more clients can join this shared song.");
	} else if(type == ConnectOKMsg)
	{
		// Need to do this in GUI thread
		SendMessage(WM_USER + 100, reinterpret_cast<WPARAM>(&inArchive));
	}
}


LRESULT NetworkingDlg::OnOpenDocument(WPARAM wParam, LPARAM /*lParam*/)
{
	cereal::BinaryInputArchive &inArchive = (*reinterpret_cast<cereal::BinaryInputArchive *>(wParam));

	auto pTemplate = theApp.GetModDocTemplate();
	auto modDoc = static_cast<CModDoc *>(pTemplate->CreateNewDocument());
	modDoc->OnNewDocument();
	CSoundFile &sndFile = modDoc->GetrSoundFile();
	inArchive >> sndFile;
	std::string title;
	inArchive >> title;
	modDoc->SetTitle(mpt::ToCString(mpt::CharsetUTF8, title));
	// TODO Tunings and samples
	auto pChildFrm = static_cast<CChildFrame *>(pTemplate->CreateNewFrame(modDoc, nullptr));
	if(pChildFrm != nullptr)
	{
		pTemplate->InitialUpdateFrame(pChildFrm, modDoc);
	}
	CMainFrame::GetMainFrame()->GetUpperTreeview()->AddDocument(*modDoc);
	m_client->SetListener(modDoc->m_listener);
	modDoc->m_collabClient = std::move(m_client);
	m_List.DeleteAllItems();
	OnOK();
	return 0;
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

	m_ModDoc.m_collabClient = collabServer->AddDocument(m_ModDoc, collaborators, spectators, mpt::ToUnicode(password));;
}



}

OPENMPT_NAMESPACE_END
