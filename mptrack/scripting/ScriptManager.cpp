/*
 * ScriptManager.cpp
 * -----------------
 * Purpose: Handles instances of scripting VMs and script packages.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "ScriptManager.h"
#include "Script.h"
#include "../Mptrack.h"
#include "../ModDocTemplate.h"

OPENMPT_NAMESPACE_BEGIN

namespace Scripting
{

Manager::Manager() { }

Manager& Manager::GetManager()
{
	static Manager instance;
	return instance;
}


/*void Manager::AddScript(const mpt::PathString &file)
{
	auto script = std::make_shared<Script>();

	sol::state &lua = *script;
	auto res = lua.do_file(file.ToLocale());
	if(res.valid())
	{
		script->OnInit();
	} else
	{
		sol::error err = res;
		OutputDebugStringA("\nEXCEPTION____________________________________\n");
		OutputDebugStringA(err.what());
		OutputDebugStringA("\nEXCEPTION____________________________________\n");
	}

	m_scripts.push_back(std::move(script));
}*/


void Manager::AddScript(Script *script)
{
	//MPT_LOCK_GUARD<mpt::mutex> lock(m_mutex);	// always accessed in GUI thread
	m_scripts.push_back(script);
}


void Manager::RemoveScript(Script *script)
{
	//MPT_LOCK_GUARD<mpt::mutex> lock(m_mutex);	// always accessed in GUI thread
	auto it = std::find(m_scripts.begin(), m_scripts.end(), script);
	if(it != m_scripts.end())
	{
		m_scripts.erase(it);
	}
}


bool Manager::DocumentExists(CModDoc &modDoc)
{
	return theApp.GetModDocTemplate()->DocumentExists(&modDoc);
}


std::unordered_set<CModDoc *> Manager::GetDocuments()
{
	return theApp.GetModDocTemplate()->GetDocuments();
}


CModDoc *Manager::ActiveDoc()
{
	return theApp.GetModDocTemplate()->ActiveDoc();
}


void Manager::OnShortcut(int id)
{
	//MPT_LOCK_GUARD<mpt::mutex> lock(m_mutex);	// always accessed in GUI thread
	for(auto &s : m_scripts)
	{
		// TODO: Script is now executed in GUI thread, so could cause GUI to freeze.
		// Should a shortcut spawn a new thread?
		s->OnShortcut(id);
	}
}


void Manager::OnMidiMessage(uint32 &midiMessage)
{
	for(auto &s : m_scripts)
	{
		s->OnMidiMessage(midiMessage);
	}
}


#define CallLuaFunction(functionName, ...) \
	for(auto &s : m_scripts) \
	{ \
		if(s->functionName.valid()) \
		{ \
			MPT_UNREFERENCED_PARAMETER(modDoc); \
			auto ret = s->functionName(/*Song(*s, modDoc),*/ __VA_ARGS__); \
			if(!ret.valid()) \
			{ \
				sol::error err = ret; \
				/*s->GetLog()  << "Exception occurred in " # functionName ": " << err.what() << std::endl;*/ \
				/*luaL_traceback(it, ...);*/ \
			} \
		} \
	}

void Manager::OnNewSong(CModDoc &modDoc)
{
	CallLuaFunction(newSongCallback);
}


void Manager::OnNewPattern(CModDoc &modDoc, PATTERNINDEX index, ORDERINDEX order)
{
	CallLuaFunction(newPatternCallback, index, order);
}


void Manager::OnNewSample(CModDoc &modDoc, SAMPLEINDEX index)
{
	CallLuaFunction(newSampleCallback, index);
}


void Manager::OnNewInstrument(CModDoc &modDoc, INSTRUMENTINDEX index)
{
	CallLuaFunction(newInstrumentCallback, index);
}


void Manager::OnNewPlugin(CModDoc &modDoc, PLUGINDEX index)
{
	CallLuaFunction(newPluginCallback, index);
}


void Manager::OnTick(CModDoc &modDoc)
{
	// TODO this must not happen on the mixer thread
	CallLuaFunction(tickCallback);
}

void Manager::OnRow(CModDoc &modDoc)
{
	CallLuaFunction(rowCallback);
}

void Manager::OnPattern(CModDoc &modDoc)
{
	CallLuaFunction(patternCallback);
}

}  // namespace Scripting

OPENMPT_NAMESPACE_END
