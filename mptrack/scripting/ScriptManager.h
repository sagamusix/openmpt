/*
 * ScriptManager.h
 * ---------------
 * Purpose: Handles instances of scripting VMs and script packages.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../../misc/mptMutex.h"
#include "../../soundlib/Snd_defs.h"
#include <unordered_set>

OPENMPT_NAMESPACE_BEGIN

class CModDoc;

namespace Scripting
{

class Script;

class Manager
{
	friend class Script;

protected:
	std::vector<Script *> m_scripts;

	Manager();

	void AddScript(Script *script);
	void RemoveScript(Script *script);

public:
	static Manager& GetManager();

	//void AddScript(const mpt::PathString &file);
	
	static bool DocumentExists(CModDoc &modDoc);
	static std::unordered_set<CModDoc *> GetDocuments();
	static CModDoc *ActiveDoc();

	void OnShortcut(int id);
	void OnMidiMessage(uint32 &midiMessage);

	// Events

	// Song has just been created
	void OnNewSong(CModDoc &modDoc);
	// New pattern has been created
	void OnNewPattern(CModDoc &modDoc, PATTERNINDEX index, ORDERINDEX order);
	// New sample has been created
	void OnNewSample(CModDoc &modDoc, SAMPLEINDEX index);
	// New instrument has been created
	void OnNewInstrument(CModDoc &modDoc, INSTRUMENTINDEX index);
	// New plugins has been created
	void OnNewPlugin(CModDoc &modDoc, PLUGINDEX index);
	// Next tick is being processed
	void OnTick(CModDoc &modDoc);
	// Next row is being processed
	void OnRow(CModDoc &modDoc);
	// Next pattern is being processed
	void OnPattern(CModDoc &modDoc);
};

}  // namespace Scripting

OPENMPT_NAMESPACE_END
