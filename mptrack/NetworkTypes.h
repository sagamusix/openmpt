/*
 * NetworkTypes.h
 * --------------
 * Purpose: Collaborative Composing implementation
 * Notes  : (currently none)
 * Authors: Johannes Schultz
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include <cereal/cereal.hpp>
#include <cereal/types/bitset.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/archives/binary.hpp>

#include "Sndfile.h"

OPENMPT_NAMESPACE_BEGIN

namespace Networking
{

struct NetworkMessage
{
	char type[4];

	NetworkMessage(const char(&id)[5] = "\0\0\0\0")
	{
		memcpy(type, id, 4);
	}

	bool operator== (const char(&other)[5]) { return !memcmp(type, other, 4); }

	template<class Archive>
	void serialize(Archive &archive)
	{
		archive(type);
	}
};

struct DocumentInfo
{
	std::string name;
	uint64 id;
	int32 collaborators;
	int32 maxCollaboratos;
	int32 spectators;
	int32 maxSpectators;
	bool password;

	template<class Archive>
	void serialize(Archive &archive)
	{
		archive(name, id, collaborators, maxCollaboratos, spectators, maxSpectators, password);
	}
};


struct WelcomeMsg// : public NetworkMessage
{
	std::string version;
	std::vector<DocumentInfo> documents;

	//WelcomeMsg() : NetworkMessage("LIST") { }

	template<class Archive>
	void serialize(Archive &archive)
	{
		archive(version, documents);
	}
};


struct JoinMsg// : public NetworkMessage
{
	uint64 id;
	std::string password;
	int32 accessType;

	//JoinMsg() : NetworkMessage("JOIN") { }

	template<class Archive>
	void serialize(Archive &archive)
	{
		archive(id, password, accessType);
	}
};


struct SetCursorPosMsg
{
	uint32 sequence, order, pattern, row, channel, column;

	template<class Archive>
	void serialize(Archive &archive)
	{
		archive(sequence, order, pattern, row, channel, column);
	}
};


struct ModCommandMask
{
	enum
	{
		kNote,
		kInstr,
		kVolCmd,
		kVol,
		kCommand,
		kParam,
		kNumColumns
	};

	ModCommand m;
	std::bitset<kNumColumns> mask;

	ModCommandMask(ModCommand mc = ModCommand::Empty()) : m(mc)
	{
		STATIC_ASSERT(sizeof(ModCommand) == kNumColumns);
	}

	template<class Archive>
	void serialize(Archive &archive)
	{
		archive(m, mask);
	}
};


struct PatternEditMsg
{
	uint32 pattern, row, channel, numRows, numChannels;
	std::vector<ModCommandMask> commands;

	PatternEditMsg(PATTERNINDEX p = 0, ROWINDEX r = 0, CHANNELINDEX c = 0, ROWINDEX rc = 0, CHANNELINDEX cc = 0)
		: pattern(p)
		, row(r)
		, channel(c)
		, numRows(rc)
		, numChannels(cc)
	{ }

	template<class Archive>
	void serialize(Archive &archive)
	{
		archive(pattern, row, channel, numRows, numChannels, commands);
	}
};

}


template<class Archive>
void CSoundFile::serializeCommon(Archive &archive)
{
	archive(m_nType, m_ContainerType, m_nChannels, m_nSamples, m_nInstruments, m_nDefaultSpeed, m_nDefaultGlobalVolume, m_nDefaultTempo,
		m_SongFlags, m_nDefaultRowsPerBeat, m_nDefaultRowsPerMeasure, m_nTempoMode,
#ifdef MODPLUG_TRACKER
		//m_lockRowStart, m_lockRowEnd,
		//m_lockOrderStart, m_lockOrderEnd,
#endif // MODPLUG_TRACKER
		m_nSamplePreAmp, m_nVSTiVolume, m_tempoSwing, m_nMinPeriod, m_nMaxPeriod, m_nResampling,
		//m_nRepeatCount,
		m_nMaxOrderPosition, ChnSettings, Patterns, Order, Samples, //Instruments,
		m_MidiCfg, m_MixPlugins,
		m_szNames, m_dwCreatedWithVersion, m_dwLastSavedWithVersion, m_playBehaviour,
		m_nMixLevels,
		m_songName, m_songArtist, m_songMessage, m_madeWithTracker, m_FileHistory, m_samplePaths
	);
}

template<class Archive>
void CSoundFile::save(Archive &archive) const
{
	const_cast<CSoundFile *>(this)->serializeCommon(archive);
	for(INSTRUMENTINDEX i = 1; i <= GetNumInstruments(); i++)
	{
		if(Instruments[i])
			archive(*Instruments[i]);
		else
			archive(ModInstrument());
	}
}
template<class Archive>
void CSoundFile::load(Archive &archive)
{
	serializeCommon(archive);
	SetModSpecsPointer(m_pModSpecs, GetType());
	SetMixLevels(m_nMixLevels);
	for(INSTRUMENTINDEX i = 1; i <= GetNumInstruments(); i++)
	{
		ModInstrument *ins = AllocateInstrument(i);
		if(ins)
		{
			archive(*ins);
		} else
		{
			ModInstrument temp;
			archive(temp);
		}
	}
	for(auto &plug : m_MixPlugins)
	{
		CreateMixPluginProc(plug, *this);
		if(plug.pMixPlugin)
		{
			plug.pMixPlugin->RestoreAllParameters(plug.defaultProgram);
		}
	}
}

OPENMPT_NAMESPACE_END

namespace cereal
{

template <class Archive>
TEMPO::store_t save_minimal(Archive const &, TEMPO const &f)
{
	return f.GetRaw();
}
template <class Archive>
void load_minimal(Archive const &, TEMPO &f, TEMPO::store_t const &value)
{
	f.SetRaw(value);
}


template <class Archive, typename T, typename Tstore = typename enum_value_type<T>::store_type>
void save(Archive &archive, FlagSet<T, Tstore> const &f)
{
	archive(f.GetRaw());
}
template <class Archive, typename T, typename Tstore = typename enum_value_type<T>::store_type>
void load(Archive &archive, FlagSet<T, Tstore> &f)
{
	Tstore value;
	archive(value);
	f.SetRaw(value);
}


template <class Archive, typename T>
void save(Archive &archive, Enum<T> const &e)
{
	typename Enum<T>::store_type value = e;
	archive(value);
}
template <class Archive, typename T>
void load(Archive &archive, Enum<T> &e)
{
	typename Enum<T>::store_type value = e;
	archive(value);
	e = static_cast<T>(value);
}


template <class Archive>
std::string save_minimal(Archive const &, mpt::PathString const &p)
{
	return p.ToUTF8();
}
template <class Archive>
void load_minimal(Archive const &, mpt::PathString &f, std::string const &value)
{
	f = mpt::PathString::FromUTF8(value);
}


template <class Archive, typename T, typename Tendian>
typename packed<T, Tendian>::base_type save_minimal(Archive const &, packed<T, Tendian> const &p)
{
	return p;
}
template <class Archive, typename T, typename Tendian>
void load_minimal(Archive const &, packed<T, Tendian> &p, typename packed<T, Tendian>::base_type const &value)
{
	p = value;
}


template<class Archive>
void serialize(Archive &archive, ModChannelSettings &c)
{
	archive(c.dwFlags, c.nPan, c.nVolume, c.nMixPlugin, c.szName);
}


template<class Archive>
void serialize(Archive &archive, ModSample &s)
{
	archive(s.nLength, s.nLoopStart, s.nLoopEnd, s.nSustainStart, s.nSustainEnd,
		s.nC5Speed, s.nPan, s.nVolume, s.nGlobalVol, s.uFlags, s.RelativeTone, s.nFineTune,
		s.nVibType, s.nVibSweep, s.nVibDepth, s.nVibRate, s.rootNote, s.filename, s.cues[9]);
	// TODO: Sample data (probably best done separately)
}


template<class Archive>
void serialize(Archive &archive, ModInstrument &i)
{
	archive(i.dwFlags, i.nFadeOut, i.nGlobalVol, i.nPan, i.nVolRampUp,
		i.wMidiBank, i.nMidiProgram, i.nMidiChannel, i.nMidiDrumKey, i.midiPWD,
		i.nDNA, i.nDCT, i.nDNA, i.nPanSwing, i.nVolSwing, i.nIFC, i.nIFR, i.nPPS, i.nPPC, i.nMixPlug,
		i.nCutSwing, i.nResSwing, i.nFilterMode, i.nPluginVelocityHandling, i.nPluginVolumeHandling,
		i.pitchToTempoLock, i.nResampling, i.VolEnv, i.PanEnv, i.PitchEnv, i.NoteMap, i.Keyboard, i.name, i.filename);
	// TODO: Tuning
}


template <class Archive>
struct specialize<Archive, ModSequence, cereal::specialization::member_serialize> {};


template<class Archive>
struct specialize<Archive, InstrumentEnvelope, cereal::specialization::non_member_serialize> {};

template<class Archive>
void serialize(Archive &archive, InstrumentEnvelope &i)
{
	archive(static_cast<std::vector<EnvelopeNode> &>(i), i.dwFlags, i.nLoopStart, i.nLoopEnd, i.nSustainStart, i.nSustainEnd, i.nReleaseNode);
}


template<class Archive>
void serialize(Archive &archive, EnvelopeNode &e)
{
	archive(e.tick, e.value);
}


template<class Archive>
void serialize(Archive &archive, tm &t)
{
	archive(t.tm_sec, t.tm_min, t.tm_hour, t.tm_mday, t.tm_mon, t.tm_year, t.tm_wday, t.tm_yday, t.tm_isdst);
}


template<class Archive>
void serialize(Archive &archive, FileHistory &h)
{
	archive(h.loadDate, h.openTime);
}


template<class Archive>
void serialize(Archive &archive, MIDIMacroConfig &m)
{
	archive(m.szMidiGlb, m.szMidiSFXExt, m.szMidiZXXExt);
}


template<class Archive>
void serialize(Archive &archive, SNDMIXPLUGININFO &i)
{
	archive(i.dwPluginId1, i.dwPluginId2, i.routingFlags, i.mixMode, i.gain, i.reserved, i.dwOutputRouting, i.dwReserved, i.szName, i.szLibraryName);
}


template<class Archive>
void serialize(Archive &archive, SNDMIXPLUGIN &p)
{
	archive(p.pluginData, p.Info, p.fDryRatio, p.defaultProgram /*, p.editorX, p.editorY*/);
}


template<class Archive>
void serialize(Archive &archive, ModCommand &m)
{
	archive(m.note, m.instr, m.volcmd, m.vol, m.command, m.param);
}


template<class Archive>
void save(Archive &archive, CPatternContainer const &patterns)
{
	archive(make_size_tag(static_cast<size_type>(patterns.Size())));
	for(const auto &pat : patterns)
	{
		archive(pat);
	}
}
template<class Archive>
void load(Archive &archive, CPatternContainer &patterns)
{
	size_type numPatterns;
	archive(make_size_tag(numPatterns));

	patterns.ResizeArray(static_cast<PATTERNINDEX>(numPatterns));
	for(auto &pat : patterns)
	{
		archive(pat);
	}
}


template<class Archive>
void save(Archive &archive, ModSequenceSet const &sequences)
{
	archive(make_size_tag(static_cast<size_type>(sequences.GetNumSequences())));
	for(const auto &seq : sequences)
	{
		archive(seq);
	}
	archive(sequences.GetCurrentSequenceIndex());
}
template<class Archive>
void load(Archive &archive, ModSequenceSet &sequences)
{
	size_type numSequences;
	archive(make_size_tag(numSequences));

	sequences.Initialize();
	for(SEQUENCEINDEX i = 0; i < numSequences; i++)
	{
		if(i > 0)
			sequences.AddSequence(false);
		archive(sequences(i));
	}
	SEQUENCEINDEX currentSeq = 0;
	archive(currentSeq);
	sequences.SetSequence(currentSeq);
}


}
