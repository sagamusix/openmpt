#include "stdafx.h"
#include "ScriptingConsole.h"
#include "Script.h"
#include "ScriptPackage.h"
#include "../resource.h"
#include "../FileDialog.h"
#include "../Reporting.h"

OPENMPT_NAMESPACE_BEGIN

namespace Scripting
{


static std::set<std::string> GetMembers(sol::table obj)
{
	sol::table metaTable = obj[sol::metatable_key];

	std::set<std::string> retVal;
	std::set<std::string> keys;
	for(const auto &pair : metaTable)
	{
		if(pair.second.get_type() == sol::type::lightuserdata)
			continue;
		keys.insert(pair.first.as<std::string>());
	}
	for(const auto &pair : obj.pairs())
	{
		if(pair.first.is<std::string>())
			keys.insert(pair.first.as<std::string>());
	}
	std::string str;
	const std::array<const char *, 4> reservedKeys{sol::detail::base_class_check_key(), sol::detail::base_class_cast_key(), reinterpret_cast<const char *>(sol::detail::base_class_index_propogation_key()), reinterpret_cast<const char *>(sol::detail::base_class_new_index_propogation_key())};
	for(const auto &key : keys)
	{
		if(key.empty())
			continue;
		if(key.length() > 1 && key[0] == '_')
			continue;
		if(mpt::contains(reservedKeys, key))
			continue;
		if(!obj[key].valid())
			continue;

		sol::object valueObj = obj[key];
		const sol::type type = valueObj.get_type();
		if(type != sol::type::string && type != sol::type::number && type != sol::type::boolean && type != sol::type::userdata && type != sol::type::table && type != sol::type::function)
			continue;

		retVal.insert(key);
	}

	return retVal;
}


void CHistoryEdit::DoAutoComplete()
{
	if(!m_script)
		return;
	int start = 0, end = 0;
	GetSel(start, end);
	if(start != end || start < 1)
		return;

	CString text;
	GetWindowText(text);

	CString context = text.Left(start), suggestionStart = context;
	int splitPoint = -1, startPoint = 0;
	for(int i = start - 1; i >= 0 && splitPoint == -1; i--)
	{
		const auto ch = context[i];
		switch(ch)
		{
		case _T('.'):
		case _T(':'):
			suggestionStart = context.Mid(i + 1);
			splitPoint = i;
			break;
		case _T(' '):
		case _T('\t'):
		case _T(','):
		case _T('('):
		case _T('['):
		case _T('{'):
			suggestionStart = context.Mid(i + 1);
			splitPoint = startPoint = i;
			break;
		}
	}
	if(splitPoint == -1)
		splitPoint = start;

	for(int i = splitPoint - 1; i >= 0 && startPoint == 0; i--)
	{
		const auto ch = context[i];
		switch(ch)
		{
			case _T(' '):
			case _T('\t'):
			case _T(','):
			case _T('('):
			case _T('['):
			case _T('{'):
				startPoint = i;
				break;
		}
	}

	context = _T("");//context.Mid(startPoint, splitPoint - startPoint);

	sol::state_view lua{*m_script};
	sol::object obj;
	if(context.IsEmpty())
		obj = lua.globals();
	else
		obj = lua[mpt::ToCharset(mpt::Charset::UTF8, context)];
	auto suggestion = GetMembers(obj);
	Reporting::Information(_T("context: ") + context + _T("\r\nsugg: ") + suggestionStart);
	const auto sugSubstr = mpt::ToCharset(mpt::Charset::UTF8, suggestionStart);
	std::vector<std::string> possibilities;
	for(const auto sug : suggestion)
	{
		if(sugSubstr.empty())
			possibilities.push_back(sug);
		if(sug.size() > sugSubstr.size() && sug.substr(sugSubstr.size()) == sugSubstr)
			possibilities.push_back(sug);
	}
	if(possibilities.size() == 1)
	{
		text.Insert(start, mpt::ToCString(mpt::Charset::UTF8, possibilities.front()));
		SetWindowText(text);
		const int newSel = static_cast<int>(start + possibilities.front().size() - sugSubstr.size());
		SetSel(newSel, newSel);
	} else
	{
		std::string possibilitiesStr;
		for(const auto &p: possibilities)
			possibilitiesStr += "\r\n" + p;
		Reporting::Information("possibilities:" + possibilitiesStr);
	}
}


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
			case VK_TAB:
				DoAutoComplete();
				return TRUE;
		}
	}
	return CEdit::PreTranslateMessage(pMsg);
}

BEGIN_MESSAGE_MAP(Console, ResizableDialog)
	ON_COMMAND(IDC_BUTTON1, OnLoadScript)
	ON_COMMAND(IDC_BUTTON2, OnStopScript)
END_MESSAGE_MAP()


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

Console::Console(CWnd *parent)
	: ResizableDialog{IDD_SCRIPTING_CONSOLE, parent}
	, m_script{std::make_unique<Script>()}
{
	Create(IDD_SCRIPTING_CONSOLE, parent);
	m_input.SetScript(m_script.get());

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

#if 1
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
}

Console::~Console() {}


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
		.ExtensionFilter("Lua Scripts|*.lua||");
	if(!dlg.Show(this)) return;

	//m_script->RequestTermination();
	const auto path = dlg.GetFirstFile();
	if(m_thread.joinable())
		m_thread.join();
	m_thread = std::thread([this, path]()
	{
		try
		{
			ScriptPackage package{path};  // TODO
			package.Run();
		}
		catch(const std::exception &e)
		{
			Reporting::Error(e.what());
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

}  // namespace Scripting

OPENMPT_NAMESPACE_END
