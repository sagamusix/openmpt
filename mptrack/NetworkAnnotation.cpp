#include "stdafx.h"
#include "NetworkAnnotation.h"
#include "Networking.h"
#include "NetworkTypes.h"
#include "Moddoc.h"
#include "resource.h"

namespace Networking
{

BEGIN_MESSAGE_MAP(AnnotationEditor, CDialog)
	//{{AFX_MSG_MAP(NetworkingDlg)
	ON_EN_CHANGE(IDC_EDIT1, OnEditMessage)
	ON_WM_ACTIVATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


AnnotationEditor::AnnotationEditor(CWnd *parent, CModDoc &modDoc)
	: m_modDoc(modDoc)
{
	Create(IDD_ANNOTATION, parent);
}


void AnnotationEditor::Show(CPoint position, PATTERNINDEX pat, ROWINDEX row, CHANNELINDEX chn, uint32 column)
{
	m_pattern = pat;
	m_row = row;
	m_channel = chn;
	m_column = column;

	CModDoc::NetworkAnnotationPos pos{ m_pattern, m_row, m_channel, m_column };
	SetDlgItemText(IDC_EDIT1, mpt::ToCString(m_modDoc.m_collabAnnotations[pos]));
	
	// Center window around annotated point
	CRect rect;
	GetWindowRect(rect);
	rect.MoveToXY(position.x - rect.Width() / 2, position.y - rect.Height() / 2);
	MoveWindow(rect);

	ShowWindow(SW_SHOW);
}


void AnnotationEditor::OnActivate(UINT nState, CWnd *pWndOther, BOOL bMinimized)
{
	CDialog::OnActivate(nState, pWndOther, bMinimized);
	if(nState == WA_INACTIVE)
	{
		ShowWindow(SW_HIDE);
	}
}


void AnnotationEditor::OnEditMessage()
{
	if(m_modDoc.m_collabClient)
	{
		CString text;
		GetDlgItemText(IDC_EDIT1, text);

		AnnotationMsg msg{ m_pattern, m_row, m_channel, m_column, mpt::ToCharset(mpt::CharsetUTF8, text) };

		std::ostringstream ss;
		cereal::BinaryOutputArchive ar(ss);
		ar(Networking::SendAnnotationMsg);
		ar(msg);
		m_modDoc.m_collabClient->Write(ss.str());
	}
}

}
