/*
* NetworkTypes.cpp
* ----------------
* Purpose: Collaborative Composing implementation
* Notes  : (currently none)
* Authors: Johannes Schultz
* The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
*/


#include "stdafx.h"
#include "NetworkTypes.h"
#include "../common/StringFixer.h"

OPENMPT_NAMESPACE_BEGIN

namespace Networking
{

void PatternEditMsg::Apply(CPattern &pat)
{
	auto mask = commands.begin();
	for(ROWINDEX r = row; r < row + numRows; r++)
	{
		auto m = pat.GetpModCommand(r, channel);
		for(CHANNELINDEX c = channel; c < channel + numChannels; c++, mask++, m++)
		{
			if(mask->mask[ModCommandMask::kNote])
				m->note = mask->m.note;
			if(mask->mask[ModCommandMask::kInstr])
				m->instr = mask->m.instr;
			if(mask->mask[ModCommandMask::kVolCmd])
				m->volcmd = mask->m.volcmd;
			if(mask->mask[ModCommandMask::kVol])
				m->vol = mask->m.vol;
			if(mask->mask[ModCommandMask::kCommand])
				m->command = mask->m.command;
			if(mask->mask[ModCommandMask::kParam])
				m->param = mask->m.param;

			// For sending back new state to all clients
			mask->mask.set();
			mask->m = *m;
		}
	}
	if(nameChanged)
	{
		pat.SetName(name);
	}
	nameChanged = true;
	name = pat.GetName();
}


void SequenceMsg::Apply(ModSequence &order)
{
	order.resize(length);
	if(nameChanged) order.SetName(name);
	nameChanged = true; // For sending back new state to all clients
	name = order.GetName();
	for(ORDERINDEX ord = 0; ord < pat.size(); ord++)
	{
		if(patChanged[ord]) order[ord] = pat[ord];
		patChanged[ord] = true; // For sending back new state to all clients
	}
}


void SamplePropertyEditMsg::Apply(CSoundFile &sndFile, SAMPLEINDEX smp)
{
	ModSample &modSample = sndFile.GetSample(smp);
	//nLength;
	modSample.nLoopStart = sample.nLoopStart;
	modSample.nLoopEnd = sample.nLoopEnd;
	modSample.nSustainStart = sample.nSustainStart;
	modSample.nSustainEnd = sample.nSustainEnd;
	modSample.nC5Speed = sample.nC5Speed;
	modSample.nPan = sample.nPan;
	modSample.nVolume = sample.nVolume;
	modSample.nGlobalVol = sample.nGlobalVol;
	modSample.uFlags = sample.uFlags;
	modSample.RelativeTone = sample.RelativeTone;
	modSample.nFineTune = sample.nFineTune;
	modSample.nVibType = sample.nVibType;
	modSample.nVibSweep = sample.nVibSweep;
	modSample.nVibDepth = sample.nVibDepth;
	modSample.nVibRate = sample.nVibRate;
	modSample.rootNote = sample.rootNote;
	mpt::String::Copy(modSample.filename, sample.filename);
	MemCopy(modSample.cues, sample.cues);
	mpt::String::Copy(sndFile.m_szNames[smp], name);
}


}

OPENMPT_NAMESPACE_END