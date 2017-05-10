/*
 * NetworkingDlg.h
 * ---------------
 * Purpose: Collaborative Composing - Connection Dialog
 * Notes  : (currently none)
 * Authors: Johannes Schultz
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

OPENMPT_NAMESPACE_BEGIN

namespace Networking
{

class NetworkingDlg : public CDialog
{
public:
	NetworkingDlg(CWnd *parent)
		: CDialog(IDD_NETWORKING, parent)
	{ }

protected:
	//{{AFX_VIRTUAL(CModTypeDlg)
	//virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	//virtual void OnOK();

	//}}AFX_VIRTUAL

	afx_msg void OnConnect();

	DECLARE_MESSAGE_MAP()
};

}

OPENMPT_NAMESPACE_END
