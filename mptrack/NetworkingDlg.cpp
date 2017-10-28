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
	ON_COMMAND(IDC_BUTTON1, OnJoinCollaborator)
	ON_COMMAND(IDC_BUTTON2,	OnConnect)
	ON_COMMAND(IDC_BUTTON3, OnJoinSpectator)
	ON_NOTIFY(NM_CLICK, IDC_LIST1, OnSelectDocument)
	ON_MESSAGE(WM_USER + 100, OnOpenDocument)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()



void NetworkingDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(NetworkingDlg)
	DDX_Control(pDX, IDC_LIST1, m_List);
	DDX_Control(pDX, IDC_BUTTON1, m_ButtonCollaborator);
	DDX_Control(pDX, IDC_BUTTON3, m_ButtonSpectator);
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

	m_ButtonCollaborator.EnableWindow(FALSE);
	m_ButtonSpectator.EnableWindow(FALSE);

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
	m_ButtonCollaborator.EnableWindow(item < 0 ? FALSE : TRUE);
	m_ButtonSpectator.EnableWindow(item < 0 ? FALSE : TRUE);
	m_selectedItem = item;
	*pResult = 0;
}


void NetworkingDlg::Join(bool collaborator)
{
	int item = m_selectedItem;
	if(item >= m_List.GetItemCount())
		return;
	auto doc = reinterpret_cast<const DocumentInfo *>(m_List.GetItemData(item));
	if(doc != nullptr)
	{
		JoinMsg join;
		join.id = doc->id;
		join.userName = mpt::ToCharset(mpt::CharsetUTF8, TrackerSettings::Instance().defaultArtist);
		join.accessType = collaborator ? JoinMsg::ACCESS_COLLABORATOR : JoinMsg::ACCESS_SPECTATOR;
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
}


bool NetworkingDlg::Receive(std::shared_ptr<CollabConnection>, std::stringstream &msg)
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
			return false;
		}

		m_ButtonCollaborator.EnableWindow(FALSE);
		m_ButtonSpectator.EnableWindow(FALSE);
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
	return true;
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
	
	uint32 numPositions;
	inArchive >> numPositions;
	for(uint32 i = 0; i < numPositions; i++)
	{
		ClientID id;
		SetCursorPosMsg msg;
		inArchive >> id;
		inArchive >> msg;
		modDoc->m_collabEditPositions[id] = { msg.sequence, msg.order, msg.pattern, msg.row, msg.channel, msg.column };
	}

	uint32 numAnnotations;
	inArchive >> numAnnotations;
	for(uint32 i = 0; i < numAnnotations; i++)
	{
		AnnotationMsg msg;
		inArchive >> msg;
		CModDoc::NetworkAnnotationPos pos{ msg.pattern, msg.row, msg.channel, msg.column };
		modDoc->m_collabAnnotations[pos] = mpt::ToUnicode(mpt::CharsetUTF8, msg.message);
	}

	uint32 numNames;
	inArchive >> numNames;
	for(uint32 i = 0; i < numNames; i++)
	{
		ClientID id;
		decltype(CollabConnection::m_userName) name;
		inArchive >> id;
		inArchive >> name;
		modDoc->m_collabNames[id] = name;
	}


	m_client->SetListener(modDoc->m_listener);
	modDoc->m_collabClient = std::move(m_client);
	// TODO Tunings
	auto pChildFrm = static_cast<CChildFrame *>(pTemplate->CreateNewFrame(modDoc, nullptr));
	if(pChildFrm != nullptr)
	{
		pTemplate->InitialUpdateFrame(pChildFrm, modDoc);
	}
	CMainFrame::GetMainFrame()->GetUpperTreeview()->AddDocument(*modDoc);
	m_List.DeleteAllItems();
	modDoc->m_chatDlg = mpt::make_unique<Networking::ChatDlg>(*modDoc);
	OnOK();
	return 0;
}


void SharingDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(SharingDlg)
	DDX_Control(pDX, IDC_SPIN1, m_CollaboratorsSpin);
	DDX_Control(pDX, IDC_SPIN2, m_SpectatorsSpin);
	//}}AFX_DATA_MAP
}


BOOL SharingDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	m_CollaboratorsSpin.SetRange(0, MAX_CLIENTS);
	m_SpectatorsSpin.SetRange(0, MAX_CLIENTS);
	NetworkedDocument doc;
	if(collabServer)
	{
		doc = collabServer->GetDocument(m_ModDoc);
	}
	SetDlgItemInt(IDC_EDIT1, doc.m_maxCollaborators);
	SetDlgItemInt(IDC_EDIT2, doc.m_maxSpectators);
	SetDlgItemText(IDC_EDIT3, mpt::ToCString(doc.m_password));

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
		VLDEnable();
		collabServer = std::make_shared<Networking::CollabServer>();
		collabServer->StartAccept();
	}

	auto newConnection = collabServer->AddDocument(m_ModDoc, collaborators, spectators, mpt::ToUnicode(password));
	// May be nullptr if we just update the document
	if(newConnection)
	{
		m_ModDoc.m_collabClient = newConnection;
	}
	if(m_ModDoc.m_chatDlg == nullptr)
	{
		m_ModDoc.m_chatDlg = mpt::make_unique<Networking::ChatDlg>(m_ModDoc);
	}
}


BEGIN_MESSAGE_MAP(ChatDlg, CDialog)
	//{{AFX_MSG_MAP(ChatDlg)
	ON_LBN_SELCHANGE(IDC_LIST2, OnGotoAnnotation)
	ON_MESSAGE(WM_USER + 101, OnUpdate)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


void ChatDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(ChatDlg)
	DDX_Control(pDX, IDC_EDIT1, m_History);
	DDX_Control(pDX, IDC_EDIT2, m_Input);
	DDX_Control(pDX, IDC_LIST1, m_Users);
	DDX_Control(pDX, IDC_LIST2, m_Annotations);
	//}}AFX_DATA_MAP
}


ChatDlg::ChatDlg(CModDoc &modDoc)
	: m_ModDoc(modDoc)
{
	Create(IDD_NETWORKCHAT, CMainFrame::GetMainFrame());
	ShowWindow(SW_SHOW);
	Update();
}


ChatDlg::~ChatDlg()
{
	DestroyWindow();
}


void ChatDlg::OnOK()
{
	// Send message
	CString message;
	m_Input.GetWindowText(message);
	if(message.IsEmpty())
	{
		return;
	}
	mpt::ustring umessage = mpt::ToUnicode(message);
	if(auto client = m_ModDoc.m_collabClient)
	{
		std::ostringstream ss;
		cereal::BinaryOutputArchive ar(ss);
		ar(ChatMsg);
		ar(umessage);
		client->Write(ss.str());
		m_Input.SetWindowText(_T(""));
	}
}


void ChatDlg::AddMessage(const mpt::ustring &sender, const mpt::ustring message)
{
	auto len = m_History.GetWindowTextLength();
	m_History.SetSel(len, len);
	m_History.ReplaceSel(mpt::ToCString(MPT_ULITERAL("<") + sender + MPT_ULITERAL("> ") + message + MPT_ULITERAL("\r\n")));
}


void ChatDlg::Update()
{
	PostMessage(WM_USER + 101, 0, 0);
}


LRESULT ChatDlg::OnUpdate(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	m_Users.SetRedraw(FALSE);
	m_Users.ResetContent();
	for(const auto &user : m_ModDoc.m_collabNames)
	{
		m_Users.AddString(mpt::ToCString(user.second));
	}
	m_Users.SetRedraw(TRUE);

	m_Annotations.SetRedraw(FALSE);
	m_Annotations.ResetContent();
	for(const auto &anno : m_ModDoc.m_collabAnnotations)
	{
		m_Annotations.AddString(mpt::cformat(_T("Pattern %1, row %2, channel %3: "))(anno.first.pattern, anno.first.row, anno.first.channel + 1) + mpt::ToCString(anno.second));
	}
	m_Annotations.SetRedraw(TRUE);
	return 0;
}


void ChatDlg::OnGotoAnnotation()
{
	int sel = m_Annotations.GetCurSel();
	if(sel >= 0 && static_cast<size_t>(sel) < m_ModDoc.m_collabAnnotations.size())
	{
		auto pos = m_ModDoc.m_collabAnnotations.cbegin();
		std::advance(pos, sel);
		m_ModDoc.ActivateView(IDD_CONTROL_PATTERNS, pos->first.pattern);
	}
}


}

OPENMPT_NAMESPACE_END
