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
#include <cereal/types/map.hpp>
#include <cereal/archives/binary.hpp>

#include "../soundlib/Sndfile.h"

OPENMPT_NAMESPACE_BEGIN

namespace Networking
{

// Create a network message ID
constexpr uint32 NetMsg(const char(&id)[5])
{
	return static_cast<uint32>(
		  (static_cast<uint8>(id[3]) << 24)
		| (static_cast<uint8>(id[2]) << 16)
		| (static_cast<uint8>(id[1]) << 8)
		| (static_cast<uint8>(id[0]) << 0));
}

// Network message IDs
enum NetworkMessage : uint32
{
	ListMsg = NetMsg("LIST"),	// Get a list of shared documents from the server

	ConnectMsg = NetMsg("CONN"),	// Join a document
	ConnectOKMsg = NetMsg("!OK!"),	// It is okay to join the document
	DocNotFoundMsg = NetMsg("404!"),	// Unknown document
	WrongPasswordMsg = NetMsg("403!"),	// Invalid password
	NoMoreClientsMsg = NetMsg("FULL"),	// No more clients can join this document

	UserJoinedMsg = NetMsg("USRJ"),	// A user joins a shared document
	UserQuitMsg = NetMsg("USRQ"),	// A user leaves a shared document

	EnvelopeTransactionMsg = NetMsg("ENTR"),	// Modification of a single envelope of a single instrument
	InstrumentTransactionMsg = NetMsg("INTR"),	// Modification of a single instrument

	SamplePropertyTransactionMsg = NetMsg("SATR"),	// Modification of sample propreties of a single sample
	SampleDataTransactionMsg = NetMsg("SDTR"),	// Modification of a single sample's data

	PatternTransactionMsg = NetMsg("PATR"),	// Modification of a single pattern
	PatternResizeMsg = NetMsg("PSTR"),	// Resizing of a single pattern
	
	EditPosMsg = NetMsg("EDPS"),	// Tell other clients about current edit position
	KeyCommandMsg = NetMsg("KEYC"),	// Send specific key command (e.g. play song)

	SequenceTransactionMsg = NetMsg("SQTR"),	// Modification of an order list (sequence)

	ChannelSettingsMsg = NetMsg("CHST"),	// Modification of channel settings
	GlobalSettingsMsg = NetMsg("GLOB"),	// Modification of global settings
	RearrangeChannelsMsg = NetMsg("REAR"),	// Rearrange/add/remove channels

	PluginDataTransactionMsg = NetMsg("PLTR"),	// Automation of a single plugin

	TuningTransactionMsg = NetMsg("TUNI"),	// Re-send tuning cllection

	ReturnValTransactionMsg = NetMsg("RETV"),	// Message with return type (one of the following messages)

	InsertPatternMsg = NetMsg("INPA"),	// Request to insert a new pattern
	InsertSampleMsg = NetMsg("INSA"),	// Request to insert a new sample
	InsertInstrumentMsg = NetMsg("ININ"),	// Request to insert a new instrument
	ConvertInstrumentsMsg = NetMsg("CNVI"),	// Request to convert all samples to instruments
	ExpandOrShrinkPatternMsg = NetMsg("EXPP"),	// Request to shrink or expand a pattern

	SendAnnotationMsg = NetMsg("ANNO"),	// Add/change/remove an annoation at a given pattern position
	PatternLockMsg = NetMsg("LOCK"),	// Add/remove a pattern lock

	ChatMsg = NetMsg("CHAT"),	// Send a chat message

	QuitMsg = NetMsg("QUIT"),	// Close connection
};


// "Optional" value that knows if it has been modified.
template<typename T>
struct opt
{
protected:
	T m_value;
	bool m_set = false;

public:
	opt() : m_value() {}
	opt(const opt<T> &) = default;
	opt(opt<T> &&) = default;
	opt<T>& operator=(const T &v) { m_value = v; m_set = true; return *this; }
	opt<T>& operator=(T &&v) { m_value = std::move(v); m_set = true; return *this; }

	explicit operator T() const { return m_value; }
	bool operator==(const T &other) const { return m_value == other; }
	bool operator==(const opt<T> &other) const { return m_value == other.m_value; }
	bool operator!=(const T &other) const { return m_value != other; }
	bool operator!=(const opt<T> &other) const { return m_value != other.m_value; }
	
	T get() const { return m_value; }
	bool is_set() const { return m_set; }
	void set_if(T &other) { if(m_set) other = m_value; }

	template<class Archive>
	void serialize(Archive &archive)
	{
		archive(m_value, m_set);
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


struct WelcomeMsg
{
	std::string version;
	std::vector<DocumentInfo> documents;

	template<class Archive>
	void serialize(Archive &archive)
	{
		archive(version, documents);
	}
};


struct JoinMsg
{
	static const uint32 ACCESS_COLLABORATOR = 0;
	static const uint32 ACCESS_SPECTATOR = 1;

	uint64 id;
	std::string userName;
	std::string password;
	uint32 accessType;

	template<class Archive>
	void serialize(Archive &archive)
	{
		archive(id, userName, password, accessType);
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
	PATTERNINDEX pattern;
	ROWINDEX row;
	CHANNELINDEX channel;
	ROWINDEX numRows;
	CHANNELINDEX numChannels;
	std::vector<ModCommandMask> commands;
	std::string name;
	bool nameChanged;

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
		archive(pattern, row, channel, numRows, numChannels, commands, name, nameChanged);
	}

	void Apply(CPattern &pattern);
};


struct SequenceMsg
{
	SEQUENCEINDEX seq;
	std::string name;
	std::vector<PATTERNINDEX> pat;
	std::vector<bool> patChanged;
	ORDERINDEX length;
	bool nameChanged;

	template<class Archive>
	void serialize(Archive &archive)
	{
		archive(seq, nameChanged, name, length, pat, patChanged);
	}

	void Apply(ModSequence &order);
};


struct SamplePropertyEditMsg
{
	SAMPLEINDEX id;
	ModSample sample;
	char name[MAX_SAMPLENAME];

	template<class Archive>
	void serialize(Archive &archive)
	{
		archive(id, sample, name);
	}

	void Apply(CSoundFile &sndFile, SAMPLEINDEX smp);
};


struct InstrumentEditMsg
{
	INSTRUMENTINDEX id;
	ModInstrument instr;
	uint32 tuningID;

	template<class Archive>
	void serialize(Archive &archive)
	{
		archive(id, instr, tuningID);
	}
};


struct PluginParam
{
	PlugParamIndex param;
	PlugParamValue value;

	template<class Archive>
	void serialize(Archive &archive)
	{
		archive(param, value);
	}
};


struct PluginEditMsg
{
	PLUGINDEX plugin;
	std::vector<PluginParam> params;
	std::vector<mpt::byte> chunk;

	template<class Archive>
	void serialize(Archive &archive)
	{
		archive(plugin, params, chunk);
	}
};


struct AnnotationMsg
{
	uint32 pattern, row, channel, column;
	std::string message;

	template<class Archive>
	void serialize(Archive &archive)
	{
		archive(pattern, row, channel, column, message);
	}
};


struct GlobalEditMsg
{
	opt<std::string> name, artist;
	opt<TEMPO> tempo;
	opt<uint32> speed;
	opt<uint32> globalVol;
	opt<uint32> sampleVol;
	opt<uint32> pluginVol;
	opt<uint32> resampling;
	opt<uint32> mixLevels;
	opt<uint32> flags;
	opt<uint32> tempoMode;
	opt<ROWINDEX> rpb;
	opt<ROWINDEX> rpm;
	opt<TempoSwing> swing;

	template<class Archive>
	void serialize(Archive &archive)
	{
		archive(name, artist, tempo, speed, globalVol, sampleVol, pluginVol, resampling, mixLevels, flags, tempoMode, rpm, rpm, swing);
	}
};


std::string SerializeTunings(const CSoundFile &sndFile);
void DeserializeTunings(CSoundFile &sndFile, const std::string &s);

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
	archive(Networking::SerializeTunings(*this));
	for(SAMPLEINDEX i = 1; i <= GetNumSamples(); i++)
	{
		/*cereal::size_type size = Samples[i].GetSampleSizeInBytes();
		archive(cereal::make_size_tag(size));
		archive(cereal::binary_data(Samples[i].pSample8, static_cast<size_t>(size)));*/
		if(Samples[i].nLength)
		{
			archive(cereal::binary_data(Samples[i].pSample8, Samples[i].GetSampleSizeInBytes()));
		}
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
	std::string tunings;
	archive(tunings);
	Networking::DeserializeTunings(*this, tunings);
	for(SAMPLEINDEX i = 1; i <= GetNumSamples(); i++)
	{
		//cereal::size_type size;
		//archive(cereal::make_size_tag(size));
		if(Samples[i].nLength && Samples[i].AllocateSample())
		{
			//archive(cereal::binary_data(Samples[i].pSample8, static_cast<size_t>(size)));
			archive(cereal::binary_data(Samples[i].pSample8, Samples[i].GetSampleSizeInBytes()));
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
