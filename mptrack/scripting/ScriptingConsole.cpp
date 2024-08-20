#include "stdafx.h"
#include "ScriptingConsole.h"
#include "Script.h"
#include "ScriptPackage.h"
#include "../resource.h"
#include "../FileDialog.h"

OPENMPT_NAMESPACE_BEGIN

namespace Scripting
{


static bool ValidateMemberName(const std::string_view key)
{
	if(key.empty())
		return false;
	// Meta stuff
	if(key.length() > 1 && key[0] == '_')
		return false;
	// Internal type stuff from sol2 in the global namespace
	if(key.starts_with("sol."))
		return false;
	const std::array<const char *, 4> reservedKeys{sol::detail::base_class_check_key(), sol::detail::base_class_cast_key(), reinterpret_cast<const char *>(sol::detail::base_class_index_propogation_key()), reinterpret_cast<const char *>(sol::detail::base_class_new_index_propogation_key())};
	if(mpt::contains(reservedKeys, key))
		return false;
	return true;
}


static void AddMember(const std::string_view key, sol::object obj, std::map<std::string, MemberType> &result)
{
	if(!ValidateMemberName(key))
		return;

	sol::object valueObj;
	if(obj.get_type() == sol::type::table)
	{
		sol::table table = obj;
		valueObj = table[key];
	} else if(obj.get_type() == sol::type::userdata)
	{
		sol::userdata userdata = obj;
		valueObj = userdata[key];
	} else
	{
		return;
	}

	const sol::type luaType = valueObj.get_type();
	if(luaType != sol::type::string && luaType != sol::type::number && luaType != sol::type::boolean && luaType != sol::type::userdata && luaType != sol::type::table && luaType != sol::type::function)
		return;

	MemberType type = MemberType::Member;
	if(luaType == sol::type::function)
	{
		// Crude approximation that only works with our internal data types
		if(obj.get_type() == sol::type::userdata)
			type = MemberType::SelfFunction;
		else
			type = MemberType::Function;
	}

	result.insert(std::make_pair(std::string{key}, type));
}


static std::map<std::string, MemberType> GetMembers(sol::object obj)
{
	std::map<std::string, MemberType> result;

	sol::object metaTableObj;
	if(obj.get_type() == sol::type::userdata)
		metaTableObj = obj.as<sol::userdata>()[sol::metatable_key];
	else
		metaTableObj = obj.as<sol::table>()[sol::metatable_key];

	if(metaTableObj.valid() && metaTableObj.get_type() == sol::type::table)
	{
		sol::table metaTable = metaTableObj;
		for(const auto &pair : metaTable)
		{
			if(pair.first.is<std::string_view>())
				AddMember(pair.first.as<std::string_view>(), obj, result);
		}

		// This is for objects like openmpt.app
		sol::object indexObj = metaTable["__index"];
		if(indexObj.valid() && indexObj.get_type() == sol::type::table)
		{
			sol::table indexTable = indexObj;
			for(const auto &pair : indexTable)
			{
				if(pair.first.is<std::string>())
					AddMember(pair.first.as<std::string_view>(), obj, result);
			}
		}
	}

	if(obj.get_type() == sol::type::table)
	{
		sol::table table = obj;
		for(const auto &pair : table)
		{
			if(pair.first.is<std::string_view>())
				AddMember(pair.first.as<std::string_view>(), obj, result);
		}

		// This is for objects like openmpt.Song, which define our Song usertype but are also a namespace for enums and such.
		if(metaTableObj.valid() && metaTableObj.get_type() == sol::type::table)
		{
			sol::table metatable = metaTableObj;
			auto utb = sol::u_detail::maybe_get_usertype_storage_base(metatable.lua_state(), metatable.push());
			if(utb)
			{
				for(const auto &pair : utb->string_keys)
				{
					if(!ValidateMemberName(pair.first))
						continue;
					MemberType type = MemberType::Member;
					if(const auto &ics = pair.second; ics.index == ics.new_index && ics.index != nullptr)
						type = MemberType::SelfFunction;
					result.insert(std::make_pair(std::string{pair.first}, type));
				}
			}
			metatable.pop();
		}
	}
	return result;
}


static constexpr bool IsSpecialLuaChar(const TCHAR ch) noexcept
{
	switch(ch)
	{
	case _T('\t'):
	case _T('\r'):
	case _T('\n'):
	case _T(' '):
	case _T('"'):
	case _T('\''):
	case _T(','):
	case _T(';'):
	case _T('('):
	case _T('['):
	case _T('{'):
	case _T('<'):
	case _T('>'):
	case _T('='):
	case _T('#'):
	case _T('+'):
	case _T('-'):
	case _T('/'):
	case _T('%'):
	case _T('^'):
	case _T('~'):
		return true;
	default:
		return false;
	}
}


void CHistoryEdit::DoAutoComplete(bool next)
{
	if(!m_script)
		return;

	int start = 0, end = 0;
	GetSel(start, end);
	if(start != end || start < 0)
		return;

	CString text;
	GetWindowText(text);

	// If cursor moved or text changed since last completion, start anew
	if(!m_completions.empty() && (m_lastCursorPos != start || m_lastCompletionText != text))
		m_completions.clear();

	// If we already gathered completions, just cycle through them
	if(!m_completions.empty())
	{
		if(next)
			m_completionIndex = (m_completionIndex + 1) % m_completions.size();
		else
			m_completionIndex = (m_completionIndex ? m_completionIndex : m_completions.size()) - 1;
		ApplySuggestion(text);
		return;
	}

	CString context = text.Left(start), suggestionStart = context;
	int splitPoint = -1, startPoint = 0;

	// Find split point (left of cursor)
	for(int i = start - 1; i >= 0 && splitPoint == -1; i--)
	{
		switch(context[i])
		{
		case _T('.'):
		case _T(':'):
			suggestionStart = context.Mid(i + 1);
			splitPoint = i;
			break;
		default:
			if(IsSpecialLuaChar(context[i]))
			{
				suggestionStart = context.Mid(i + 1);
				splitPoint = startPoint = i + 1;
			}
			break;
		}
	}
	if(splitPoint == -1)
	{
		suggestionStart = context;
		splitPoint = 0;
		startPoint = 0;
	}

	for(int i = splitPoint - 1; i >= 0 && startPoint == 0; i--)
	{
		if(IsSpecialLuaChar(context[i]))
			startPoint = i + 1;
	}

	// If cursor is in the middle of a word, also consider characters to the right of the cursor
	int rightEnd = start;
	for(int i = start; i < text.GetLength(); i++)
	{
		if(_tcschr(_T("\t\r\n\" '.:,;()[]{}<>=#+-*/%^~"), text[i]))
			break;
		rightEnd = i + 1;
	}

	context = context.Mid(startPoint, splitPoint - startPoint);
	m_globalContext = context.IsEmpty();

	// Gather suggestions for first autocomplete
	sol::state_view lua{*m_script};
	sol::object obj;
	if(m_globalContext)
	{
		//obj = lua.globals();
		obj = m_script->m_environment;
	} else
	{
		try
		{
			sol::environment env{lua, sol::create, m_script->m_environment};
			obj = lua.script("return " + mpt::ToCharset(mpt::Charset::UTF8, context), env);
		} catch(...)
		{
			return;
		}
	}

	if(!obj.valid() || (obj.get_type() != sol::type::table && obj.get_type() != sol::type::userdata))
		return;

	auto members = GetMembers(obj);
	if(m_globalContext)
	{
		static constexpr const char *LuaKeywords[] =
		{
			"and ", "break ", "do ", "else ", "elseif ", "end ",
			"false ", "for ", "function ", "global ", "if ",
			"in ", "local ", "nil ", "not ", "or ", "repeat ",
			"return ", "then ", "true ", "until ", "while "
		};
		for(const auto &keyword : LuaKeywords)
		{
			members.insert(std::make_pair(std::string{keyword}, MemberType::Member));
		}
	}

	const CString matchStr = suggestionStart + text.Mid(start, rightEnd - start);
	for(const auto &[m, type] : members)
	{
		CString member = mpt::ToCString(mpt::Charset::UTF8, m);
		if(matchStr.IsEmpty() || (member.GetLength() >= matchStr.GetLength() && member.Left(matchStr.GetLength()) == matchStr))
		{
			if(type != MemberType::Member)
				member += _T("()");
			m_completions.emplace_back(std::move(member), type);
		}
	}

	if(m_completions.empty())
		return;

	// Replace the entire word (left + right parts)
	m_replaceStart = start - suggestionStart.GetLength();
	m_replaceEnd = rightEnd;
	m_completionIndex = 0;

	ApplySuggestion(text);
}


void CHistoryEdit::ApplySuggestion(const CString &text)
{
	const auto &[completion, type] = m_completions[m_completionIndex];
	m_lastCompletionText = text.Left(m_replaceStart) + completion + text.Mid(m_replaceEnd);

	// Convert . to : or vice versa based on whether suggestion is a self function
	if(m_replaceStart > 0)
	{
		const TCHAR prevChar = m_lastCompletionText[m_replaceStart - 1];
		if(type == MemberType::SelfFunction && prevChar == _T('.'))
			m_lastCompletionText.SetAt(m_replaceStart - 1, _T(':'));
		else if(type != MemberType::SelfFunction && prevChar == _T(':'))
			m_lastCompletionText.SetAt(m_replaceStart - 1, _T('.'));
	}

	SetWindowText(m_lastCompletionText);

	m_replaceEnd = m_replaceStart + completion.GetLength();
	// If suggestion is a function, place cursor between parentheses
	m_lastCursorPos = m_replaceEnd;
	if(type != MemberType::Member)
		m_lastCursorPos--;
	SetSel(m_lastCursorPos, m_lastCursorPos);
}


BOOL CHistoryEdit::PreTranslateMessage(MSG *pMsg)
{
	if(pMsg->message == WM_KEYDOWN)
	{
		switch(pMsg->wParam)
		{
		case VK_UP:
			if(m_historyIndex > 0)
			{
				m_historyIndex--;
				SetWindowText(m_history[m_historyIndex]);
				SetSel(GetWindowTextLength(), GetWindowTextLength());
			}
			m_completions.clear();
			return TRUE;
		case VK_DOWN:
			m_historyIndex++;
			if(m_historyIndex < m_history.size())
			{
				SetWindowText(m_history[m_historyIndex]);
				SetSel(GetWindowTextLength(), GetWindowTextLength());
			} else
			{
				::MessageBeep(MB_ICONWARNING);
			}
			m_completions.clear();
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
					m_historyIndex = m_history.size();
				}
			}
			m_completions.clear();
			break;
		case VK_TAB:
			DoAutoComplete(!(GetAsyncKeyState(VK_SHIFT) & 0x8000));
			return TRUE;
		}
	} else if(pMsg->message == WM_CHAR)
	{
		// Listen to EN_CHANGE instead?
		m_completions.clear();
	}
	return CEdit::PreTranslateMessage(pMsg);
}


static std::string FormatUserType(sol::table obj, Script &script, int indent = 0)
{
	sol::table metaTable = obj[sol::metatable_key];

#if 1
	std::set<std::variant<std::string, int>> keys;
	for(const auto &pair : metaTable)
	{
		if(pair.second.get_type() == sol::type::lightuserdata)
			continue;
		keys.insert(pair.first.as<std::string>());
	}
	// todo for 0-based containers, the first entry is lost
	// todo crashes for non-containers https://github.com/ThePhD/sol2/issues/1453
	for(const auto &pair : obj.pairs())
	{
		if(pair.first.is<std::string>())
			keys.insert(pair.first.as<std::string>());
		else if(pair.first.is<int>())
			keys.insert(pair.first.as<int>());
	}
	std::string str;
	const std::array<const char *, 4> reservedKeys{sol::detail::base_class_check_key(), sol::detail::base_class_cast_key(), reinterpret_cast<const char *>(sol::detail::base_class_index_propogation_key()), reinterpret_cast<const char *>(sol::detail::base_class_new_index_propogation_key())};
	for(const auto &keyObj : keys)
	{
		std::string key, value;
		sol::object valueObj;
		if(std::holds_alternative<std::string>(keyObj))
		{
			key = std::get<std::string>(keyObj);
			if(key.empty())
				continue;
			if(key.length() > 1 && key[0] == '_')
				continue;
			if(mpt::contains(reservedKeys, key))
				continue;
			if(!obj[key].valid())
				continue;
			valueObj = obj[key];
		} else //if(keyObj.is<int>())
		{
			int keyInt = std::get<int>(keyObj);
			key = mpt::afmt::val(keyInt);
			valueObj = obj[keyInt];
		}

		const sol::type type = valueObj.get_type();
		//if(type == sol::type::none || type == sol::type::function)
		//	continue;
		if(type != sol::type::string && type != sol::type::number && type != sol::type::boolean && type != sol::type::userdata && type != sol::type::table)
			continue;

		if(type == sol::type::userdata /* || type == sol::type::table*/)
			value = FormatUserType(valueObj, script, indent + 1);
		else
			value = script.ToString(valueObj);

		str += std::string(indent, '\t') + key + " = " + value + "\n";
	}

#else
	std::map<std::string, std::string> members;
	for(const auto &pair : metaTable)
	{
		std::string value;
		members.insert(std::make_pair(std::move(key), std::move(value)));
	}
	//if(metaTable[sol::meta_function::pairs].valid())
	for(const auto &[key, valueObj] : obj.pairs())
	{
		const sol::type type = valueObj.get_type();
		//if(type == sol::type::none || type == sol::type::function)
		//	continue;
		if(type != sol::type::string && type != sol::type::number && type != sol::type::boolean && type != sol::type::userdata && type != sol::type::table)
			continue;

		OutputDebugStringA((script.ToString(key) + "...\n").c_str());
		std::string value;
		if(type == sol::type::userdata /* || type == sol::type::table*/)
			value = FormatUserType(valueObj, script, indent + 1);
		else
			value = script.ToString(valueObj);

		members.insert(std::make_pair(script.ToString(key), value));
	}
	std::string str;
	for(const auto &[key, value] : members)
	{
		str += std::string(indent, '\t') + key + " = " + value + "\n";
	}
#endif
	return str;
}

BEGIN_MESSAGE_MAP(Console, ResizableDialog)
	ON_WM_CLOSE()

	ON_COMMAND(IDC_BUTTON1, &Console::OnLoadScript)
	ON_COMMAND(IDC_BUTTON2, &Console::OnStopScript)
END_MESSAGE_MAP()


Console::Console(CWnd *parent)
	: ResizableDialog{IDD_SCRIPTING_CONSOLE, parent}
	, m_script{std::make_unique<Script>()}
{
	m_input.SetScript(m_script.get());

	m_script->SetIdleFunction([]()
	{
		MSG msg;
		while(::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
	});

	sol::state &lua = *m_script;
	lua.set_function("print", [this](const sol::variadic_args &va)
	{
		CString text;
		bool first = true;
		for(const auto &arg : va)
		{
			if(first)
				first = false;
			else
				text += _T("\t");

#if 0
			std::string str;
			if(arg.get_type() == sol::type::userdata)
				str = FormatUserType(arg.as<sol::table>(), *m_script);
			else
				str = m_script->ToString(arg.get<sol::object>());
#else
			auto str = m_script->ToString(arg.get<sol::object>());
#endif
			str = mpt::replace(str, std::string(1, '\0'), std::string("\\0"));
			text += mpt::ToCString(mpt::Charset::UTF8, str);
		}
		text += _T("\n");
		AddToHistory(text);
	});

	Create(IDD_SCRIPTING_CONSOLE, parent);
}

Console::~Console() {}


void Console::OnClose()
{
	SetRunning(false);  // Reactivate main window if necessary
	DestroyWindow();
}


void Console::DoDataExchange(CDataExchange *pDX)
{
	ResizableDialog::DoDataExchange(pDX);
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
	m_history.SetLimitText(0);  // Remove default 32k text limit

	sol::state &lua = *m_script;
	lua.do_string(R"(print("> Welcome to )" LUA_RELEASE R"(.")
		print("> More information on the OpenMPT scripting API can be obtained from api.lua.\n"))", m_script->m_environment);

	SetRunning(false);

	ShowWindow(SW_SHOW);
	return FALSE;
}


void Console::OnOK()
{
	CString scriptText;
	m_input.GetWindowText(scriptText);
	if(!scriptText.IsEmpty())
	{
		m_input.SetWindowText(_T(""));
		SetRunning(true);

		AddToHistory(_T("> ") + scriptText + _T("\n"));

		//m_script->RequestTermination();
		if(m_thread.joinable()) m_thread.join();
		//m_thread = std::thread([this, scriptText]()
		{
			sol::state &lua = *m_script;
			auto res = lua.do_string(mpt::ToCharset(mpt::Charset::UTF8, scriptText), m_script->m_environment, "@[console input]");
			if(!res.valid())
			{
				sol::error err = res;
				AddToHistory(_T(">>> ") + mpt::ToCString(mpt::Charset::UTF8, err.what()) + _T("\n"));
			}
		}
		//);
		SetRunning(false);
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
	FileDialog dlg = OpenFileDialog{}
		.DefaultExtension("lua")
		.ExtensionFilter("Lua Scripts|*.lua||");
	if(!dlg.Show(this)) return;

	SetRunning(true);
	//m_script->RequestTermination();
	const auto path = dlg.GetFirstFile();
	if(m_thread.joinable())
		m_thread.join();
	//m_thread = std::thread([this, path]()
	{
		try
		{
			ScriptPackage package{path};  // TODO use m_script environment
			package.Run();
			return;
		} catch(const std::exception &e)
		{
			AddToHistory(_T(">>> ") + mpt::ToCString(mpt::Charset::UTF8, e.what()) + _T("\n"));
			AddToHistory(_T(">>> Retrying as normal Lua file\n"));
		}
		sol::state &lua = *m_script;
		m_script->m_environment = sol::environment(lua, sol::create, lua.globals());
		lua["package"]["path"] = path.GetDirectoryWithDrive().ToUTF8() + LUA_PATH_MARK ".lua" LUA_PATH_SEP + lua["package"]["path"].get_or<std::string>({});
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
			try
			{
				sol::error err = res;
				AddToHistory(_T(">>> ") + mpt::ToCString(mpt::Charset::UTF8, err.what()) + _T("\n"));
			} catch(sol::error &e)
			{
				AddToHistory(_T(">>> Errorception!\n"));
				AddToHistory(_T(">>> ") + mpt::ToCString(mpt::Charset::UTF8, e.what()) + _T("\n"));
			}
		}
	}
	//);
	SetRunning(false);
}


void Console::OnStopScript()
{
	//if(isRunning)
	m_script->RequestTermination();
	//else
	//m_input.SetWindowText(_T(""));
}


void Console::AddToHistory(CString text)
{
	text.Replace(_T("\n"), _T("\r\n"));
	auto length = m_history.GetWindowTextLength();
	m_history.SetSel(length, length, FALSE);
	m_history.ReplaceSel(text);
}


void Console::SetRunning(bool running)
{
	GetParent()->EnableWindow(running ? FALSE : TRUE);
	m_input.EnableWindow(running ? FALSE : TRUE);
	GetDlgItem(IDC_BUTTON1)->EnableWindow(running ? FALSE : TRUE);
	GetDlgItem(IDC_BUTTON2)->EnableWindow(running ? TRUE : FALSE);
	if(running)
		GetDlgItem(IDC_BUTTON2)->SetFocus();
	else
		m_input.SetFocus();
}


}  // namespace Scripting

OPENMPT_NAMESPACE_END
