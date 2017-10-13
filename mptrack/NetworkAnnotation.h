#pragma once

#include "Snd_defs.h"

class CModDoc;
namespace Networking
{

class AnnotationEditor : public CDialog
{
	CModDoc &m_modDoc;
	uint32 m_pattern = 0;
	uint32 m_row = 0;
	uint32 m_channel = 0;
	uint32 m_column = 0;

public:
	AnnotationEditor(CWnd *parent, CModDoc &modDoc);
	void Show(CPoint position, PATTERNINDEX pat, ROWINDEX row, CHANNELINDEX chn, uint32 column);

protected:
	afx_msg void OnActivate(UINT nState, CWnd *pWndOther, BOOL bMinimized);
	afx_msg void OnEditMessage();

	DECLARE_MESSAGE_MAP()
};

}
