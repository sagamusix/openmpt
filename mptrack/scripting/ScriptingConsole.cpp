#include "stdafx.h"
#include "ScriptingConsole.h"
#include "Script.h"
#include "../resource.h"
#include "../FileDialog.h"
#include "../Reporting.h"

OPENMPT_NAMESPACE_BEGIN

namespace Scripting
{

BOOL CHistoryEdit::PreTranslateMessage(MSG *pMsg)
{
	if(pMsg->message == WM_KEYDOWN)
	{
		switch(pMsg->wParam)
		{
		case VK_UP:
			if(m_index > 0)
			{
				m_index--;
				SetWindowText(m_history[m_index]);
				SetSel(GetWindowTextLength(), GetWindowTextLength());
			}
			return TRUE;
		case VK_DOWN:
			m_index++;
			if(m_index < m_history.size())
			{
				SetWindowText(m_history[m_index]);
				SetSel(GetWindowTextLength(), GetWindowTextLength());
			} else
			{
				m_index = m_history.size();
				SetWindowText(_T(""));
			}
			return TRUE;
		case VK_RETURN:
			// TODO: Ctrl+Enter = Multiline?
			{
				CString text;
				GetWindowText(text);
				//if(m_index >= m_history.size() || m_history[m_index] != text)
				{
					if(m_history.empty() || m_history.back() != text)
					{
						m_history.push_back(std::move(text));
					}
					m_index = m_history.size();
				}
			}
			break;
		}
	}
	return CEdit::PreTranslateMessage(pMsg);
}

BEGIN_MESSAGE_MAP(Console, ResizableDialog)
	ON_COMMAND(IDC_BUTTON1, OnLoadScript)
	ON_COMMAND(IDC_BUTTON2, OnStopScript)
END_MESSAGE_MAP()


Console::Console(CWnd *parent)
	: ResizableDialog(IDD_SCRIPTING_CONSOLE, parent)
	, m_script(std::make_unique<Script>())
{
	Create(IDD_SCRIPTING_CONSOLE, parent);
	sol::state &lua = *m_script;
	lua.set_function("print", [this](const sol::variadic_args &va)
	{
		CString text;
		bool first = true;
		for(const auto &a : va)
		{
			if(first)
				first = false;
			else
				text += _T("\t");
			text += mpt::ToCString(mpt::Charset::UTF8, m_script->ToString(a.get<sol::object>()));
		}
		text += _T("\n");
		AddToHistory(text);
	});
}


void Console::DoDataExchange(CDataExchange *pDX)
{
	ResizableDialog::DoDataExchange(pDX);
	//DDX_Control(pDX, IDC_EDIT1, m_input);
	DDX_Control(pDX, IDC_EDIT2, m_history);
}


BOOL Console::OnInitDialog()
{
	ResizableDialog::OnInitDialog();

	m_input.SubclassDlgItem(IDC_EDIT1, this);

	CFont *font = m_history.GetFont();
	LOGFONT lf;
	font->GetLogFont(&lf);
	_tcscpy(lf.lfFaceName, _T("Consolas"));
	m_font.CreateFontIndirect(&lf);
	m_input.SetFont(&m_font);
	m_history.SetFont(&m_font);

	m_input.SetFocus();

	ShowWindow(SW_SHOW);
	return TRUE;
}


void Console::OnOK()
{
	CString scriptText;
	m_input.GetWindowText(scriptText);
	if(!scriptText.IsEmpty())
	{
		m_input.SetWindowText(_T(""));

		AddToHistory(_T("> ") + scriptText + _T("\n"));

		//m_script->RequestTermination();
		if(m_thread.joinable()) m_thread.join();
		m_thread = std::thread([this, scriptText]()
		{
			sol::state &lua = *m_script;
			auto res = lua.do_string(mpt::ToCharset(mpt::Charset::UTF8, scriptText), m_script->m_environment, "@[console input]");
			if(!res.valid())
			{
				sol::error err = res;
				AddToHistory(_T(">>> ") + mpt::ToCString(mpt::Charset::UTF8, err.what()) + _T("\n"));
			}
		});
	}
}


void Console::PostNcDestroy()
{
	m_script->RequestTermination();
	if(m_thread.joinable()) m_thread.join();
	ResizableDialog::PostNcDestroy();
}


void Console::OnLoadScript()
{
	FileDialog dlg = OpenFileDialog()
		.DefaultExtension("lua")
		.ExtensionFilter("Lua Scripts|*.lua||")
		.WorkingDirectory(MPT_PATHSTRING("S:/VC++/"));
	if(!dlg.Show(this)) return;

	//m_script->RequestTermination();
	auto path{ dlg.GetFirstFile() };
	if(m_thread.joinable()) m_thread.join();
	m_thread = std::thread([this, path]()
	{
		sol::state &lua = *m_script;
		m_script->m_environment = sol::environment(lua, sol::create, lua.globals());
		lua["package"]["path"] = path.GetPath().ToUTF8() + "?.lua;" + lua["package"]["path"].get<std::string>();
		auto res = lua.do_file(path.ToUTF8(), m_script->m_environment);
		if(res.valid())
		{
			try
			{
				m_script->OnInit();
			} catch(const Script::Error &e)
			{
				AddToHistory(_T(">>> ") + mpt::ToCString(mpt::Charset::UTF8, e.what()) + _T("\n"));
			}
		} else
		{
			sol::error err = res;
			AddToHistory(_T(">>> ") + mpt::ToCString(mpt::Charset::UTF8, err.what()) + _T("\n"));
		}
	});
}


void Console::OnStopScript()
{
	m_script->RequestTermination();
}


void Console::AddToHistory(CString text)
{
	text.Replace(_T("\n"), _T("\r\n"));
	auto length = m_history.GetWindowTextLength();
	m_history.SetSel(length, length, FALSE);
	m_history.ReplaceSel(text);
}


}

OPENMPT_NAMESPACE_END
