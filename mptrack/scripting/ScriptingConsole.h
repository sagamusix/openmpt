#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../ResizableDialog.h"

#include <thread>
#include <atomic>

OPENMPT_NAMESPACE_BEGIN

namespace Scripting
{

class Script;

enum class MemberType : uint8
{
	Member,
	Function,
	SelfFunction,
};


class CHistoryEdit : public CEdit
{
public:
	BOOL PreTranslateMessage(MSG *pMsg) override;
	void SetScript(Script *script) { m_script = script; }

protected:
	void DoAutoComplete(bool next);
	void ApplySuggestion(const CString &text);

	std::vector<CString> m_history;
	size_t m_historyIndex = 0;
	Script *m_script = nullptr;

	std::vector<std::pair<CString, MemberType>> m_completions;
	CString m_lastCompletionText;
	size_t m_completionIndex = 0;
	int m_replaceStart = -1;
	int m_replaceEnd = -1;
	int m_lastCursorPos = -1;
	bool m_globalContext = false;
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
	void OnCancel() override { OnStopScript(); }
	
	afx_msg void OnClose();
	afx_msg void OnLoadScript();
	afx_msg void OnStopScript();

	void AddToHistory(CString text);
	void SetRunning(bool running);

	DECLARE_MESSAGE_MAP()
};

}  // namespace Scripting

OPENMPT_NAMESPACE_END
