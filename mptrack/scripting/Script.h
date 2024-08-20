/*
 * Script.h
 * --------
 * Purpose: Lua environment wrapper
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "LuaVM.h"

#include <set>

OPENMPT_NAMESPACE_BEGIN

namespace Scripting
{

inline const double API_VERSION = 1.0;

class Script : public LuaVM
{
	std::unordered_map<int, sol::protected_function> m_shortcuts;
	std::unordered_map<int, sol::protected_function> m_midiFilters;
	std::vector<mpt::PathString> m_tempFiles;
	std::set<std::pair<std::string, std::string>> m_temporaryStates;
	int m_nextMidiFilterID = 0;

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
	void OnMidiMessage(uint32 &midiMessage);

	std::string GetState(const std::string &section, const std::string &key, const sol::optional<std::string> &defaultValue);
	void SetState(const std::string &section, const std::string &key, const std::string &value, const sol::optional<bool> &persist);
	void DeleteState(const std::string &section, const std::string &key);

	template<typename Tfunc>
	static void CallInGUIThread(Tfunc &&func)
	{
		CallInGUIThreadImpl(std::forward<std::function<void()>>(func));
	}

private:
	sol::table RegisterEnvironment();
	void SetupOSLibrary(const std::set<std::string> &permissions);
	void SetupStringLibrary();
	void MakeReadOnly(sol::table &table);

	static void FFT(sol::table &real, sol::optional<sol::table> &imaginary, bool ifft);

	static void CallInGUIThreadImpl(std::function<void()> &&func);
};

}  // namespace Scripting

OPENMPT_NAMESPACE_END
