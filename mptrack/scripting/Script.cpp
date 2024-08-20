/*
 * Script.cpp
 * ----------
 * Purpose: Lua environment wrapper
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
 
/*
TODOs:
- "script packages" consisting of several files, using our CustomSearcher function
  - should contain a manifest (contains name, version, required permissions, etc)
- A plugin that can execute Lua code on Process and MIDI calls
- make things "observable" rather than having specific callbacks for everything
- can we somehow make settings exposed?
  some generally useful settings could be the recording quantization amount, or the recording state
- Lua can't do copy assignments, as everything is a reference. we maybe need "assign" functions to copy e.g. a sample from one slot to another, or maybe even between modules
- We need to PrepareUndo() a lot of stuff.
*/

#include "stdafx.h"
#include "Script.h"
#include "ScriptManager.h"
#include "../InputHandler.h"
#include "../Mainfrm.h"
#include "../Mptrack.h"
#include "../WindowMessages.h"
#include "../FolderScanner.h"
#include "../../common/version.h"
#include "../../common/misc_util.h"
#include "../../soundlib/Snd_defs.h"
#include "../../soundlib/MIDIEvents.h"
#include "../../soundlib/TinyFFT.h"

OPENMPT_NAMESPACE_BEGIN

namespace Scripting
{

static void UnavailableFunction()
{
	throw "This function is not available.";
}

static void LockedFunction()
{
	throw "The script package must request the appropriate permissions to use this function.";
}

static int CustomSearcher(lua_State *L)
{
	//const char *name = luaL_checklstring(L, 1, nullptr);
	const auto name = sol::stack::get<sol::string_view>(L);

	if(name == "foo")
	{
		//auto vm = LuaVM::GetInstance(L);
		const char *filename = "@Internal/foo.lua";  // Prefix with @ for proper stacktrace formatting
		std::string moduleContent = "local M = {} function M.bar() return \"bar\" end function M.baz() return asf.gegw end return M";

		if(!moduleContent.empty() && moduleContent[0] == LUA_SIGNATURE[0])
			return luaL_error(L, "error loading module '%s' from file '%s':\n\tBytecode is not permitted.", lua_tostring(L, 1), filename);

		if(luaL_loadbuffer(L, moduleContent.c_str(), moduleContent.size(), filename) == LUA_OK)
		{
			lua_pushstring(L, filename);
			return 2; // Return open function and file name
		} else
		{
			return luaL_error(L, "error loading module '%s' from file '%s':\n\t%s",
				lua_tostring(L, 1), filename, lua_tostring(L, -1));
		}
	} else
	{
		// Not found
		return 1;
	}
}


Script::Script()
	: m_environment{lua, sol::create, lua.globals()}
{
	//lua["print"]("Welcome to " LUA_RELEASE);
#if 1
	lua.add_package_loader(CustomSearcher);
#elif 2
	sol::table searchers = lua["package"]["searchers"];
	searchers.add(CustomSearcher);
#else
	sol::table searchers = lua["package"]["searchers"];
	searchers.add(sol::as_function([this](const sol::string_view &module)
	{
		if(module == "foo")
		{
			return sol::make_object(lua, lua.load("local M = {} function M.bar() return \"bar\" end function M.baz() return asf.gegw end return M", "@Internal/foo.bar").get<sol::object>());
		} else
		{
			return sol::make_object(lua, "\n\tno such module in script package");
		}
	}));
#endif
	// Remove package from cache: package.loaded.mymodule = nil
	//sol::table loaded = lua["package"]["loaded"];
	lua["package"]["path"] = sol::nil;
	lua["package"]["cpath"] = sol::nil;

#if 0
	//sol::environment m_environment(lua, sol::create, lua.globals());
	//m_environment["openmpt"] = RegisterEnvironment();	// lua.create_table_with(sol::metatable_key, openmpt);
	m_environment[sol::metatable_key][sol::meta_function::index] = m_environment;
	m_environment[sol::metatable_key][sol::meta_function::new_index] = sol::as_function([this](sol::table table, const char *name, sol::object object)
	{
		if(!strcmp(name, "openmpt"))
			throw "You cannot modify this namespace.";
		table.raw_set(name, object);
	});
#else
	// Protect our openmpt namespace from being overwritten by the user
	sol::table environment = lua.create_table_with("openmpt", RegisterEnvironment());
	sol::table metatable = lua.create_table();
	metatable[sol::meta_function::index] = environment;
	metatable[sol::meta_function::new_index] = sol::as_function([](sol::table table, const char *name, sol::object object)
	{
		if(!strcmp(name, "openmpt"))
			throw "You cannot modify this namespace.";
		table.raw_set(name, object);
	});
	lua.globals()[sol::metatable_key] = metatable;
#endif
	
	lua.script("function iif(condition, if_true, if_false) if condition then return if_true else return if_false end end", "=iif");
	SetupOSLibrary({});
	SetupStringLibrary();


	//lua["string"]["dump"].set_function(UnavailableFunction);
	lua["load"].set_function(UnavailableFunction);

	// TODO: translate filenames from UTF8, disallow loading binary blobs (also for "load" function)
	lua["dofile"].set_function(UnavailableFunction);
	lua["loadfile"].set_function(UnavailableFunction);

	// http://lua.2524044.n2.nabble.com/Suggestion-quot-usertype-quot-function-td7580056.html
	//lua.script("function type(v) local ty = getmetatable(v).__type return ty and \"_\" ..ty or type(v) end"/*, "=openmpt.Type"*/);

	// TODO more stuff from https://luau-lang.org/sandbox ?

	/*sol::table openmpt = lua["openmpt"];
	MakeReadOnly(openmpt);
	sol::table song = lua["openmpt"]["Song"];
		MakeReadOnly(song);*/

	Manager::GetManager().AddScript(this);
}


static sol::table DirOrFilenames(sol::this_state state, const std::string &path, const std::string &filter, FlagSet<FolderScanner::ScanType> type)
{
	sol::state_view lua(state);
	FolderScanner scanner(mpt::PathString::FromUTF8(path), type, mpt::PathString::FromUTF8(filter));
	sol::table paths = lua.create_table();
	mpt::PathString foundPath;
	while(scanner.Next(foundPath))
	{
		paths.add(foundPath.ToUTF8());
	}
	return paths;
}


void Script::SetupOSLibrary(const std::set<std::string> &permissions)
{
	// TODO: Script manifest should request availability of individual os functions (or the whole OS table?)
	sol::table os = lua["os"];
	os["exit"].set_function(UnavailableFunction);
	os["dirnames"].set_function([](sol::this_state state, const std::string &path)
	{
		return DirOrFilenames(state, path, "*.*", FolderScanner::kOnlyDirectories);
	});
	os["filenames"].set_function([](sol::this_state state, const std::string &path, sol::optional<std::string> filter)
	{
		return DirOrFilenames(state, path, filter.value_or("*.*"), FolderScanner::kOnlyFiles);
	});
	os["tmpname"].set_function([this]()
	{
		m_tempFiles.push_back(mpt::TemporaryPathname{MPT_PATHSTRING("openmpt_scripting")}.GetPathname());
		return m_tempFiles.back().ToUTF8();
	});
#ifdef MPT_OS_WINDOWS // wchar fixups
	os["execute"].set_function([](lua_State *L) -> int
	{
		const char *cmd = luaL_optstring(L, 1, NULL);
		int stat = _wsystem(cmd ? mpt::ToWide(mpt::Charset::UTF8, cmd).c_str() : nullptr);
		if(cmd != nullptr)
		{
			return luaL_execresult(L, stat);
		} else
		{
			lua_pushboolean(L, stat); // true if there is a shell
			return 1;
		}
	});
	os["getenv"].set_function([](const char *varname)
	{
		return mpt::ToCharset(mpt::Charset::UTF8, _wgetenv(mpt::ToWide(mpt::Charset::UTF8, varname).c_str()));
	});
	os["mkdir"].set_function([](const std::string &path)
	{
		return _wmkdir(mpt::ToWin(mpt::Charset::UTF8, path).c_str()) != FALSE;
	});
	os["remove"].set_function([](const char *path)
	{
		return _wremove(mpt::ToWide(mpt::Charset::UTF8, path).c_str());
	});
	os["rename"].set_function([](const char *oldname, const char *newname)
	{
		return _wrename(mpt::ToWide(mpt::Charset::UTF8, oldname).c_str(), mpt::ToWide(mpt::Charset::UTF8, newname).c_str());
	});
#endif

	static constexpr const char *functions[] = {"execute", "getenv", "remove", "rename", "tmpname", "dirnames", "filenames", "mkdir"};
	for(const auto function : functions)
	{
		if(!permissions.count(std::string("os.") + function))
			os[function].set_function(LockedFunction);
	}
}


void Script::SetupStringLibrary()
{
	sol::table string = lua["string"];
	string["upper"].set_function([](const std::string &str)
	{
		return mpt::ToCharset(mpt::Charset::UTF8, mpt::ToUpperCase(mpt::ToUnicode(mpt::Charset::UTF8, str)));
	});
	string["lower"].set_function([](const std::string &str)
	{
		return mpt::ToCharset(mpt::Charset::UTF8, mpt::ToLowerCase(mpt::ToUnicode(mpt::Charset::UTF8, str)));
	});
	string["reverse"].set_function([](const std::string &str)
	{
		auto reversed = mpt::ToUnicode(mpt::Charset::UTF8, str);
		std::reverse(reversed.begin(), reversed.end());
		return mpt::ToCharset(mpt::Charset::UTF8, reversed);
	});
}


Script::~Script()
{
	// TODO Call optional shutdown function here

	auto *ih = CMainFrame::GetMainFrame()->GetInputHandler();
	bool anyShortcuts = false;
	for(const auto &sc : m_shortcuts)
	{
		anyShortcuts |= ih->m_activeCommandSet->RemoveScriptable(static_cast<CommandID>(sc.first));
	}
	if(anyShortcuts)
	{
		ih->RegenerateCommandSet();
	}
	Manager::GetManager().RemoveScript(this);
	// TODO: Files can be still open at this point!
	for(const auto &path : m_tempFiles)
	{
		DeleteFileW(mpt::support_long_path(path.AsNative()).c_str());
	}
	for(const auto &[section, key] : m_temporaryStates)
	{
		DeleteState(section, key);
	}
}


void Script::OnInit()
{
	sol::object table = m_environment["OPENMPT_SCRIPT"];
	if(table.is<sol::table>())
	{
		double apiVersion = table.as<sol::table>()["api_version"].get_or(-1.0);
		if(apiVersion > Scripting::API_VERSION)
			throw Error(MPT_AFORMAT("This script expects API version {}. OpenMPT's API version is {}.")(apiVersion, Scripting::API_VERSION));
		else if(apiVersion < 0.0)
			throw Error("OPENMPT_SCRIPT.api_version is missing!");
	} else
	{
		throw Error("OPENMPT_SCRIPT table is missing!");
	}
}


void Script::OnShortcut(int id)
{
	const auto sc = m_shortcuts.find(id);
	if(sc != m_shortcuts.end())
	{
		auto thread = sol::thread::create(lua);
		sol::protected_function func(thread.state(), sol::ref_index(sc->second.registry_index()));
		func(id);
	}
}


void Script::OnMidiMessage(uint32 &midiMessage)
{
	for(auto &[id, filterFunc] : m_midiFilters)
	{
		auto thread = sol::thread::create(lua);
		sol::protected_function func(thread.state(), sol::ref_index(filterFunc.registry_index()));
		std::vector<uint8> midiBytes(std::min(MIDIEvents::GetEventLength(static_cast<uint8>(midiMessage)), static_cast<uint8>(sizeof(midiMessage))));
		memcpy(midiBytes.data(), &midiMessage, midiBytes.size());
		auto result = func(midiBytes, id, "MIDI Device Name");  // TODO
		if(result.valid())
		{
			sol::optional<std::vector<uint8>> value = result;
			if(value)
			{
				midiMessage = 0;
				memcpy(&midiMessage, value->data(), std::min(value->size(), sizeof(midiMessage)));
			}
		}
	}
}


void Script::FFT(sol::table &real, sol::optional<sol::table> &imaginary, bool ifft)
{
	if(imaginary && real.size() != imaginary->size())
		throw "Real and imaginary arrays need to have the same length";
	if(!mpt::has_single_bit(real.size()))
		throw "Arrays size needs to be a power of 2";

	std::vector<std::complex<double>> input(real.size());
	if(imaginary)
	{
		for(size_t i = 0; i < input.size(); i++)
			input[i] = {real.get_or(i + 1, 0.0), imaginary->get_or(i + 1, 0.0)};
	} else
	{
		for(size_t i = 0; i < input.size(); i++)
			input[i] = {real.get_or(i + 1, 0.0), 0.0};
	}

	TinyFFT tinyFFT{static_cast<uint32>(mpt::bit_width(input.size()) - 1)};
	if(ifft)
		tinyFFT.IFFT(input);
	else
		tinyFFT.FFT(input);
	if(ifft)
		tinyFFT.Normalize(input);

	for(size_t i = 0; i < input.size(); i++)
		real[i + 1] = input[i].real();
	if(imaginary)
	{
		for(size_t i = 0; i < input.size(); i++)
			(*imaginary)[i + 1] = input[i].imag();
	}
}


std::string Script::GetState(const std::string &section, const std::string &key, const sol::optional<std::string> &defaultValue)
{
	mpt::ustring value;
	CallInGUIThread([&]
	{
		value = theApp.GetSettings().Read(U_("Script.") + mpt::ToUnicode(mpt::Charset::UTF8, section), mpt::ToUnicode(mpt::Charset::UTF8, key), mpt::ToUnicode(mpt::Charset::UTF8, defaultValue.value_or(std::string{})));
	});
	return mpt::ToCharset(mpt::Charset::UTF8, value);
}


void Script::SetState(const std::string &section, const std::string &key, const std::string &value, const sol::optional<bool> &persist)
{
	CallInGUIThread([&]
	{
		theApp.GetSettings().Write(U_("Script.") + mpt::ToUnicode(mpt::Charset::UTF8, section), mpt::ToUnicode(mpt::Charset::UTF8, key), mpt::ToUnicode(mpt::Charset::UTF8, value));
		if(persist.value_or(false))
			m_temporaryStates.erase(std::make_pair(section, key));
		else
			m_temporaryStates.insert(std::make_pair(section, key));
	});
}


void Script::DeleteState(const std::string &section, const std::string &key)
{
	CallInGUIThread([&]
	{
		m_temporaryStates.erase(std::make_pair(section, key));
		theApp.GetSettings().Forget(U_("Script.") + mpt::ToUnicode(mpt::Charset::UTF8, section), mpt::ToUnicode(mpt::Charset::UTF8, key));
	});
}


void Script::CallInGUIThreadImpl(std::function<void()> &&func)
{
	CMainFrame::GetMainFrame()->SendMessage(WM_MOD_SCRIPTCALL, reinterpret_cast<WPARAM>(&func));
}


void Script::MakeReadOnly(sol::table &table)
{
	sol::table metatable = lua.create_table();
	metatable[sol::meta_function::index] = table;
	metatable[sol::meta_function::new_index] = sol::as_function([]()
	{
		throw "You cannot modify this namespace.";
	});
	table[sol::metatable_key] = metatable;
	/*
	sol::table metatable = create_with(
		sol::meta_function::index, table,
		sol::meta_function::new_index, sol::as_function([]()
		{
			throw "You cannot modify this namespace.";
		}));
	return lua.create_named(name, sol::metatable_key, metatable); */
}

}  // namespace Scripting

OPENMPT_NAMESPACE_END
