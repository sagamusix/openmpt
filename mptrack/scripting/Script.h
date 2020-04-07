/*
 * Script.h
 * --------
 * Purpose: Lua environment wrapper
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "LuaVM.h"

OPENMPT_NAMESPACE_BEGIN

namespace Scripting
{

inline const double API_VERSION = 1.0;

class Script : public LuaVM
{
	std::unordered_map<int, sol::protected_function> m_shortcuts;
	std::vector<mpt::PathString> m_tempFiles;

public:
	sol::protected_function newSongCallback, newPatternCallback, newSampleCallback, newInstrumentCallback, newPluginCallback, tickCallback, rowCallback, patternCallback;
	sol::environment m_environment;

public:
	Script();
	Script(Script &&) = delete;
	Script(const Script &) = delete;

	~Script();

	// Events
	void RegisterCallback(int which, sol::protected_function callback);

	// Script has just been loaded
	void OnInit();

	void OnShortcut(int id);

	template<typename Tfunc>
	static void CallInGUIThread(Tfunc &&func)
	{
		CallInGUIThreadImpl(std::forward<std::function<void()>>(func));
	}

private:
	sol::table RegisterEnvironment();
	void SetupOSLibrary();
	void MakeReadOnly(sol::table &table);

	static void CallInGUIThreadImpl(std::function<void()> &&func);
};

}

OPENMPT_NAMESPACE_END
