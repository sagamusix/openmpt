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
#include "../soundlib/tuning.h"
#include "../soundlib/tuningcollection.h"

OPENMPT_NAMESPACE_BEGIN

namespace Networking
{

std::string SerializeTunings(const CSoundFile &sndFile)
{
	std::ostringstream ss;
	cereal::BinaryOutputArchive ar(ss);
	{
		std::ostringstream tss;
		sndFile.GetTuneSpecificTunings().Serialize(tss, "Tune specific");
		ar(tss.str());
	}
	for(INSTRUMENTINDEX i = 1; i <= sndFile.GetNumInstruments(); i++)
	{
		if(sndFile.Instruments[i] && sndFile.Instruments[i]->pTuning)
			ar(sndFile.Instruments[i]->pTuning->GetName());
		else
			ar(std::string());
	}
	return ss.str();
}


void DeserializeTunings(CSoundFile &sndFile, const std::string &s)
{
	std::istringstream is(s);
	cereal::BinaryInputArchive ar(is);
	auto &tunings = sndFile.GetTuneSpecificTunings();
	tunings.RemoveAll();
	{
		std::string ts;
		ar(ts);
		std::istringstream tss(ts);
		std::string name;
		tunings.Deserialize(tss, name);
	}
	for(INSTRUMENTINDEX i = 1; i <= sndFile.GetNumInstruments(); i++)
	{
		std::string tuning;
		ar(tuning);
		if(sndFile.Instruments[i] && !tuning.empty())
		{
			for(const auto &tun : tunings)
			{
				if(tun->GetName() == tuning)
				{
					sndFile.Instruments[i]->pTuning = tun.get();
					break;
				}
			}
		}
	}
}


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
	memcpy(&modSample.cues, &sample.cues, sizeof(sample.cues));
	mpt::String::Copy(sndFile.m_szNames[smp], name);
}


}

OPENMPT_NAMESPACE_END