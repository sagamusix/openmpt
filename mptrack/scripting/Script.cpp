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
- We need to PrepareUndo() a lot of stuff, or erase the undo history alternatively.
*/

#include "stdafx.h"
#include "Script.h"
#include "ScriptManager.h"
#include "../InputHandler.h"
#include "../Mainfrm.h"
#include "../Mptrack.h"
#include "../Reporting.h"
#include "../WindowMessages.h"
#include "../FolderScanner.h"
#include "../../common/version.h"
#include "../../common/misc_util.h"
#include "../../common/mptFileTemporary.h"
#include "../../soundlib/Snd_defs.h"
#include "../../soundlib/MIDIEvents.h"
#include "../../soundlib/TinyFFT.h"

#include <sapi.h>

OPENMPT_NAMESPACE_BEGIN

namespace Scripting
{

static constexpr std::pair<const char*, const char*> ScriptPermissions[] =
{
	{"allow_write_all_files", "Write to ANY file on your computer"},
	{"allow_read_all_files", "Read ANY file on your computer"},
	{"debug", "Use Lua debugging facilities (may cause instability or crashes)"},
	{"package.loadlib", "Load arbitrary DLL libraries on your computer"},
	{"io.popen", "Run arbitrary programs on your computer and read their output"},
	{"io.tmpfile", "Write to temporary files"},
	{"os.dirnames", "Create directory listings of arbitrary folders on your computer"},
	{"os.filenames", "Create file listing of arbitrary folders on your computer"},
	{"os.tmpname", "Write to temporary files"},
	{"os.execute", "Run arbitrary programs on your computer"},
	{"os.getenv", "Read the process environment"},
	{"os.mkdir", "Create new directory anywhere on your computer"},
	{"os.remove", "Delete ANY files on your computer"},
	{"os.rename", "Rename ANY files on your computer"},
};

std::unordered_map<UINT_PTR, sol::protected_function> Script::s_timers;

static void UnavailableFunction()
{
	throw "This function is not available.";
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

#if 0
	//sol::environment m_environment(lua, sol::create, lua.globals());
	m_environment["openmpt"] = RegisterEnvironment();	// lua.create_table_with(sol::metatable_key, openmpt);
	m_environment[sol::metatable_key][sol::meta_function::index] = m_environment;
	m_environment[sol::metatable_key][sol::meta_function::new_index] = sol::as_function([this](sol::table table, const char *name, sol::object object)
	{
		if(!strcmp(name, "openmpt"))
			throw "You cannot modify this namespace.";
		table.raw_set(name, object);
	});
#elif 1
	lua["openmpt"] = RegisterEnvironment();
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
	HardenLuaPackages();
	SetupStringLibrary();

	// http://lua.2524044.n2.nabble.com/Suggestion-quot-usertype-quot-function-td7580056.html
	//lua.script("function type(v) local ty = getmetatable(v).__type return ty and \"_\" ..ty or type(v) end"/*, "=openmpt.Type"*/);

	// TODO more stuff from https://luau-lang.org/sandbox ?

	/*sol::table openmpt = lua["openmpt"];
	MakeReadOnly(openmpt);
	sol::table song = lua["openmpt"]["Song"];
		MakeReadOnly(song);*/

	Manager::GetManager().AddScript(this);
}


static sol::table DirOrFilenames(sol::state_view state, const std::string &path, const std::string &filter, FlagSet<FolderScanner::ScanType> type)
{
	sol::state_view lua{state};
	FolderScanner scanner(mpt::PathString::FromUTF8(path), type, mpt::PathString::FromUTF8(filter));
	sol::table paths = lua.create_table();
	mpt::PathString foundPath;
	while(scanner.Next(foundPath))
	{
		paths.add(foundPath.ToUTF8());
	}
	return paths;
}


void Script::CheckOpenFile(const std::string &filename, bool openForWrite) const
{
	if(openForWrite && !m_allowWriteFiles.contains(filename) && !m_permissions.count("allow_write_all_files"))
		throw "You are not allowed to open this file for writing.";
	else if(!m_allowReadFiles.contains(filename) && !m_allowWriteFiles.contains(filename) && !m_permissions.count("allow_read_all_files") && !m_permissions.count("allow_write_all_files"))
		throw "You are not allowed to open this file for reading.";
}


void Script::HardenLuaPackages()
{
	if(!m_permissions.count("debug"))
		lua["debug"] = sol::nil;

	//lua["string"]["dump"].set_function(UnavailableFunction);
	lua["load"].set_function(UnavailableFunction);

	// TODO: translate filenames from UTF8, disallow loading binary blobs (also for "load" function)
	lua["dofile"].set_function(UnavailableFunction);
	lua["loadfile"].set_function(UnavailableFunction);

	// Remove package from cache: package.loaded.mymodule = nil
	//sol::table loaded = lua["package"]["loaded"];
	lua["package"]["path"] = sol::nil;
	lua["package"]["cpath"] = sol::nil;

	sol::function originalLoadlib = lua["package"]["loadlib"];
	lua["package"]["loadlib"] = sol::as_function([this, originalLoadlib](sol::variadic_args args)
	{
		if(!m_permissions.count("package.loadlib"))
			throw "'package.loadlib' permission required";
		return originalLoadlib(args);
	});

	// Hijack io functions for file access and permission validation
	sol::table io = lua["io"];
	
	sol::function originalOpenFunc = io["open"];
	io["open"] = sol::as_function([this, originalOpenFunc](const std::string &filename, sol::optional<std::string_view> mode) -> sol::object
	{
		CheckOpenFile(filename, mode && mode->find('w') != std::string_view::npos);
		sol::object result = mode ? originalOpenFunc(filename, *mode) : originalOpenFunc(filename);
		if(result.valid())
			m_openFiles.push_back(result);

		return result;
	});

	sol::function originalInputFunc = io["input"];
	io["input"] = sol::as_function([this, originalInputFunc](sol::optional<std::string> filename) -> sol::object
	{
		sol::object result;
		if(filename)
		{
			CheckOpenFile(*filename, false);
			result = originalInputFunc(*filename);
			if(result.valid())
				m_openFiles.push_back(result);
		} else
		{
			result = originalInputFunc();
		}
		return result;
	});

	sol::function originalOutputFunc = io["output"];
	io["output"] = sol::as_function([this, originalOutputFunc](sol::optional<std::string> filename) -> sol::object
	{
		sol::object result;
		if(filename)
		{
			CheckOpenFile(*filename, true);
			result = originalOutputFunc(*filename);
			if(result.valid())
				m_openFiles.push_back(result);
		} else
		{
			result = originalOutputFunc();
		}
		return result;
	});

	sol::function originalLinesFunc = io["lines"];
	io["lines"] = sol::as_function([this, originalLinesFunc](sol::optional<std::string> filename, sol::variadic_args va) -> sol::object
	{
		if(filename)
		{
			CheckOpenFile(*filename, false);
			return originalLinesFunc(*filename, va);
		} else
		{
			return originalLinesFunc(va);
		}
	});

	sol::function originalPopenFunc = io["popen"];
	io["popen"] = sol::as_function([this, originalPopenFunc](const std::string &prog, sol::variadic_args va)
	{
		if(!m_permissions.count("io.popen"))
			throw "'io.popen' permission required";

		return originalPopenFunc(prog, va);
	});

	io["tmpfile"] = sol::as_function([this]()
	{
		if(!m_permissions.count("io.tmpfile"))
			throw "'io.tmpfile' permission required";

		mpt::PathString path = mpt::TemporaryPathname{P_("openmpt_scripting")}.GetPathname();
		std::string pathU8 = path.ToUTF8();

		sol::function openFunc = lua["io"]["open"];
		sol::object result = openFunc(pathU8, "w+b");
		if(result.valid())
			m_openFiles.push_back(result);

		m_allowWriteFiles.insert(std::move(pathU8));
		m_tempFiles.insert(std::move(path));

		return result;
	});

	// Remove dangerous functions and guard others by permission checks
	sol::table os = lua["os"];

	os["exit"].set_function(UnavailableFunction);

	// Custom
	os["dirnames"] = sol::as_function([this](const std::string &path) -> sol::object
	{
		if(!m_permissions.count("os.dirnames"))
			throw "'os.dirnames' permission required";
		return DirOrFilenames(lua, path, "*.*", FolderScanner::kOnlyDirectories);
	});

	// Custom
	os["filenames"] = sol::as_function([this](const std::string &path, sol::optional<std::string> filter) -> sol::object
	{
		if(!m_permissions.count("os.filenames"))
			throw "'os.filenames' permission required";
		return DirOrFilenames(lua, path, filter.value_or("*.*"), FolderScanner::kOnlyFiles);
	});

	os["tmpname"] = sol::as_function([this]()
	{
		if(!m_permissions.count("os.tmpname"))
			throw "'os.tmpname' permission required";
		mpt::PathString path = mpt::TemporaryPathname{P_("openmpt_scripting")}.GetPathname();
		std::string pathU8 = path.ToUTF8();
		m_allowReadFiles.insert(pathU8);
		m_allowWriteFiles.insert(pathU8);
		m_tempFiles.insert(std::move(path));
		return pathU8;
	});

#ifdef MPT_OS_WINDOWS // wchar fixups
	os["execute"] = sol::as_function([this](lua_State *L) -> int
	{
		if(!m_permissions.count("os.execute"))
			throw "'os.execute' permission required";
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

	os["getenv"] = sol::as_function([this](std::string_view varname)
	{
		if(!m_permissions.count("os.getenv"))
			throw "'os.getenv' permission required";
		return mpt::ToCharset(mpt::Charset::UTF8, _wgetenv(mpt::ToWide(mpt::Charset::UTF8, varname).c_str()));
	});

	// Custom
	os["mkdir"] = sol::as_function([this](std::string_view path)
	{
		if(!m_permissions.count("os.mkdir"))
			throw "'os.mkdir' permission required";
		return _wmkdir(mpt::ToWin(mpt::Charset::UTF8, path).c_str()) != FALSE;
	});

	os["remove"] = sol::as_function([this](std::string_view path)
	{
		const auto pathW = mpt::ToWide(mpt::Charset::UTF8, path);
		if(!m_tempFiles.count(pathW) && !m_permissions.count("os.remove"))
			throw "'os.remove' permission required";
		return luaL_fileresult(lua, _wremove(pathW.c_str()), path.data());
	});

	os["rename"] = sol::as_function([this](std::string_view oldname, std::string_view newname)
	{
		if(!m_permissions.count("os.rename"))
			throw "'os.rename' permission required";
		return luaL_fileresult(lua, _wrename(mpt::ToWide(mpt::Charset::UTF8, oldname).c_str(), mpt::ToWide(mpt::Charset::UTF8, newname).c_str()), nullptr);
	});
#else
	// Non-Windows versions still need permission checks
	static constexpr const char *functions[] = {"execute", "getenv", "remove", "rename"};
	for(const char *funcName : functions)
	{
		sol::function origFunc = os[funcName];
		if(!origFunc.valid())
			continue;
		os[funcName] = sol::as_function([this, funcName, originalFunc](sol::variadic_args args) -> sol::object
		{
			auto permissionName = std::string{"os."} + funcName;
			if(!m_permissions.count(permissionName))
				throw "Permission to use this function is required";
			return originalFunc(args);
		});
	}
#endif
}


void Script::SetupStringLibrary()
{
	sol::table string = lua["string"];
	// Enforce proper Unicode behaviour on these functions
	string["upper"].set_function([](const std::string &str)
	{
		return mpt::ToCharset(mpt::Charset::UTF8, mpt::ToUpperCaseLocale(mpt::ToUnicode(mpt::Charset::UTF8, str)));
	});
	string["lower"].set_function([](const std::string &str)
	{
		return mpt::ToCharset(mpt::Charset::UTF8, mpt::ToLowerCaseLocale(mpt::ToUnicode(mpt::Charset::UTF8, str)));
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

	std::erase_if(s_timers, [this](auto it)
	{
		if(GetInstance(it.second.lua_state()) == this)
		{
			::KillTimer(nullptr, it.first);
			return true;
		}
		return false;
	});

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

	// Close any open files before trying to delete temporary files
	for(auto &fileObj : m_openFiles)
	{
		if(!fileObj.is<sol::table>())
			continue;
		sol::table file = fileObj.as<sol::table>();
		sol::protected_function closeFunc = file["close"];
		if(closeFunc.valid())
			closeFunc(file);
	}
	m_openFiles.clear();

	for(const auto &path : m_tempFiles)
	{
		DeleteFileW(mpt::support_long_path(path.AsNative()).c_str());
	}
	while(!m_temporaryStates.empty())
	{
		const auto &[section, key] = *m_temporaryStates.begin();
		DeleteState(section, key);
	}

	if(m_sapiController)
		m_sapiController->Release();
}


void Script::OnInit()
{
	sol::object table = m_environment["OPENMPT_SCRIPT"];
	if(table.is<sol::table>())
	{
		sol::table manifest = table.as<sol::table>();
		double apiVersion = manifest["api_version"].get_or(-1.0);
		if(apiVersion > Scripting::API_VERSION)
			throw Error(MPT_AFORMAT("This script expects API version {}. OpenMPT's API version is {}.")(apiVersion, Scripting::API_VERSION));
		else if(apiVersion < 0.0)
			throw Error("OPENMPT_SCRIPT.api_version is missing!");

		if(sol::object permissions = manifest["permissions"]; permissions.valid())
			RequestPermissions(permissions);
	} else
	{
		throw Error("OPENMPT_SCRIPT table is missing!");
	}
}


void Script::OnShortcut(int id)
{
	const auto sc = m_shortcuts.find(id);
	if(sc == m_shortcuts.end())
		return;
	auto thread = sol::thread::create(sc->second.lua_state());
	sol::protected_function func(thread.state(), sc->second.registry_index());
	func(id);
}


void Script::OnMidiMessage(uint32 &midiMessage)
{
	for(auto &[id, filterFunc] : m_midiFilters)
	{
		auto thread = sol::thread::create(lua);
		sol::protected_function func(thread.state(), filterFunc.registry_index());
		std::vector<uint8> midiBytes(std::min(MIDIEvents::GetEventLength(static_cast<uint8>(midiMessage)), static_cast<uint8>(sizeof(midiMessage))));
		memcpy(midiBytes.data(), &midiMessage, midiBytes.size());
		auto result = func(midiBytes, "MIDI Device Name", id);  // TODO
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
		theApp.GetSettings().Forget(U_("Script.") + mpt::ToUnicode(mpt::Charset::UTF8, section), mpt::ToUnicode(mpt::Charset::UTF8, key));
		m_temporaryStates.erase(std::make_pair(section, key));
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


bool Script::RequestPermissions(sol::object permissionObj)
{
	std::set<std::string> requestedPermissions;
	if(permissionObj.is<std::string>())
	{
		requestedPermissions.insert(permissionObj.as<std::string>());
	} else if(permissionObj.is<sol::table>())
	{
		sol::table permissionsTable = permissionObj.as<sol::table>();
		for(const auto &[_, permission] : permissionObj.as<sol::table>())
		{
			requestedPermissions.insert(permission.as<std::string>());
		}
	} else
	{
		throw "Parameter must be a string or table of strings";
	}

	return RequestPermissions(requestedPermissions);
}


bool Script::RequestPermissions(const std::set<std::string> &requestedPermissions)
{
	const std::map<std::string_view, std::string_view> validPermissions{std::begin(ScriptPermissions), std::end(ScriptPermissions)};

	std::set<std::string> missingPermissions;
	std::string permissionList;
	for(const auto &permission : requestedPermissions)
	{
		auto it = validPermissions.find(permission);
		if(it == validPermissions.end())
			continue;
		// Don't ask again if the user already denied once
		if(m_deniedPermissions.count(permission))
			return false;
		if(!m_permissions.contains(permission))
		{
			missingPermissions.insert(permission);
			permissionList += "\n- " + std::string{it->second};
		}
	}

	if(missingPermissions.empty())
		return true;

	CString message = MPT_CFORMAT("The script is requesting permission to:\n{}\n\nWould you like to grant {}?")(mpt::ToCString(mpt::Charset::UTF8, permissionList), missingPermissions.size() == 1 ? _T("this permission") : _T("these permissions"));
	auto result = Reporting::Confirm(message, _T("Script Permission Request"), true, true);
	if(result != cnfYes)
	{
		m_deniedPermissions.insert(missingPermissions.begin(), missingPermissions.end());
		if(result == cnfCancel)
			throw "Terminated by user.";
		return false;
	}
	for(const auto &permission : missingPermissions)
	{
		m_permissions.insert(permission);
	}
	// Implicitly granted
	if(missingPermissions.contains("allow_write_all_files"))
		m_permissions.insert("allow_read_all_files");
	return true;
}


}  // namespace Scripting

OPENMPT_NAMESPACE_END
