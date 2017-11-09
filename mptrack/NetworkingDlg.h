/*
 * NetworkingDlg.h
 * ---------------
 * Purpose: Collaborative Composing - Connection Dialog
 * Notes  : (currently none)
 * Authors: Johannes Schultz
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "Networking.h"
#include "NetworkTypes.h"
#include "CListCtrl.h"
#include <unordered_map>

OPENMPT_NAMESPACE_BEGIN

class CModDoc;

namespace Networking
{

class NetworkingDlg : public CDialog, public Listener
{
	CListCtrlEx m_List;
	CButton m_ButtonCollaborator, m_ButtonSpectator;
	std::vector<DocumentInfo> m_docs;
	std::shared_ptr<RemoteCollabClient> m_client;
	int m_selectedItem = 0;

public:
	static void Show(CWnd *parent);

protected:
	void DoDataExchange(CDataExchange* pDX) override;
	BOOL OnInitDialog() override;
	void PostNcDestroy() override;

	//}}AFX_VIRTUAL

	afx_msg void OnConnect();
	afx_msg void OnJoinCollaborator() { Join(true); }
	afx_msg void OnJoinSpectator() { Join(false); }
	afx_msg void OnSelectDocument(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg LRESULT OnOpenDocument(WPARAM wParam, LPARAM lParam);

	void Join(bool collaborator);
	bool Receive(std::shared_ptr<CollabConnection>, std::stringstream &msg) override;

	DECLARE_MESSAGE_MAP()
};


class SharingDlg : public CDialog
{
	CSpinButtonCtrl m_CollaboratorsSpin, m_SpectatorsSpin;
	CModDoc &m_ModDoc;

public:
	SharingDlg(CWnd *parent, CModDoc &modDoc)
		: CDialog(IDD_SHAREMODULE, parent)
		, m_ModDoc(modDoc)
	{ }

protected:
	void DoDataExchange(CDataExchange* pDX) override;
	BOOL OnInitDialog() override;
	void OnOK() override;
};


class ChatDlg : public CDialog
{
	CImageList m_Icons;
	CEdit m_History, m_Input;
	CListCtrl m_Users;
	CListBox m_Annotations, m_ActionLog;
	CTabCtrl m_Tabs;
	CModDoc &m_ModDoc;
	std::map<ClientID, int> m_UserToIcon;
	std::map<ClientID, mpt::tstring> m_LastUserAction;
	int m_iconSize;

public:
	ChatDlg(CModDoc &modDoc);
	~ChatDlg();

	void AddMessage(const mpt::ustring &sender, const mpt::ustring &message);
	void AddAction(ClientID sender, const mpt::tstring &message);
	void Update();
	ClientID GetFollowUser();

protected:
	void DoDataExchange(CDataExchange* pDX) override;
	void OnOK() override;
	
	afx_msg void OnGotoAnnotation();
	afx_msg LRESULT OnUpdate(WPARAM /*wParam*/, LPARAM /*lParam*/);
	afx_msg LRESULT OnAddAction(WPARAM id, LPARAM /*lParam*/);
	afx_msg void OnTabSelchange(NMHDR *pNMHDR, LRESULT *pResult);

	DECLARE_MESSAGE_MAP()
};

}

OPENMPT_NAMESPACE_END
