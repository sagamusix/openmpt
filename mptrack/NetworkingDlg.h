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
	std::vector<DocumentInfo> m_docs;
	std::shared_ptr<CollabClient> m_client;

public:
	static void Show(CWnd *parent);

protected:
	void DoDataExchange(CDataExchange* pDX) override;
	BOOL OnInitDialog() override;
	void PostNcDestroy() override;

	//}}AFX_VIRTUAL

	afx_msg void OnConnect();
	afx_msg void OnSelectDocument(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg LRESULT OnOpenDocument(WPARAM wParam, LPARAM lParam);

	void Receive(CollabConnection *, const std::string &msg) override;

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

}

OPENMPT_NAMESPACE_END
