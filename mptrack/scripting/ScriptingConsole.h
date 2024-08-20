#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../ResizableDialog.h"

#include <thread>
#include <atomic>

OPENMPT_NAMESPACE_BEGIN

namespace Scripting
{

class Script;

class CHistoryEdit : public CEdit
{
	std::vector<CString> m_history;
	size_t m_index = 0;
	Script *m_script = nullptr;

public:
	BOOL PreTranslateMessage(MSG *pMsg) override;
	void SetScript(Script *script) { m_script = script; }

protected:
	void DoAutoComplete();
};


class Console : public ResizableDialog
{
protected:
	CHistoryEdit m_input;
	CEdit m_history;
	CFont m_font;

	std::unique_ptr<Script> m_script;
	std::thread m_thread;
	std::atomic<bool> m_readMore;

public:
	Console(CWnd *parent);
	~Console();

protected:
	BOOL OnInitDialog() override;
	void DoDataExchange(CDataExchange *pDX) override;
	void PostNcDestroy() override;

	void OnOK() override;
	afx_msg void OnLoadScript();
	afx_msg void OnStopScript();

	void AddToHistory(CString text);

	DECLARE_MESSAGE_MAP()
};

}  // namespace Scripting

OPENMPT_NAMESPACE_END
