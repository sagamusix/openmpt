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
	uint64 id;
	std::string password;
	int32 accessType;

	template<class Archive>
	void serialize(Archive &archive)
	{
		archive(id, password, accessType);
	}
};

}

namespace cereal
{

template <class Archive>
TEMPO::store_t save_minimal(Archive const &, TEMPO const &f)
{
	return f.GetRaw();
}
template <class Archive, typename T, typename Tstore = typename enum_value_type<T>::store_type>
void load_minimal(Archive const &, TEMPO &f, TEMPO::store_t const &value)
{
	f.SetRaw(value);
}


template <class Archive, typename T, typename Tstore = typename enum_value_type<T>::store_type>
Tstore save_minimal(Archive const &, FlagSet<T, Tstore> const &f)
{
	return f.GetRaw();
}
template <class Archive, typename T, typename Tstore = typename enum_value_type<T>::store_type>
void load_minimal(Archive const &, FlagSet<T, Tstore> &f, Tstore const &value)
{
	f.SetRaw(value);
}


template <class Archive, typename T>
typename enum_value_type<T>::store_type save_minimal(Archive const &, Enum<T> const &f)
{
	return f;
}
template <class Archive, typename T>
void load_minimal(Archive const &, Enum<T> &f, typename enum_value_type<T>::store_type const &value)
{
	f = value;
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
	archive(i.dwPluginId1, i.dwPluginId2, i.routingFlags, i.mixMode, i.gain, i.reserved, i.dwOutputRouting, i.dwReserved[4], i.szName[32], i.szLibraryName[64]);
}


template<class Archive>
void serialize(Archive &archive, SNDMIXPLUGIN &p)
{
	archive(p.pluginData, p.Info, p.fDryRatio, p.defaultProgram, /*p.editorX, p.editorY*/);
}

}

OPENMPT_NAMESPACE_END
