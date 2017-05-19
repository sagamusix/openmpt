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
#include "CListCtrl.h"

OPENMPT_NAMESPACE_BEGIN

namespace Networking
{

class NetworkingDlg : public CDialog, public Listener
{
	CListCtrlEx m_List;

public:
	static void Show(CWnd *parent);

protected:
	//{{AFX_VIRTUAL(CModTypeDlg)
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void PostNcDestroy();
	//virtual void OnOK();

	//}}AFX_VIRTUAL

	afx_msg void OnConnect();
	afx_msg void OnSelectDocument(NMHDR *pNMHDR, LRESULT *pResult);

	void Receive(const std::string &msg) override;

	DECLARE_MESSAGE_MAP()
};

}

OPENMPT_NAMESPACE_END
