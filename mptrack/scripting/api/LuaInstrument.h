/*
 * LuaInstrument.h
 * ---------------
 * Purpose: Lua wrappers for CModDoc and contained classes
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once
#include "LuaObject.h"
#include "LuaSample.h"

#include "../../../soundlib/tuning.h"
#include "../../../soundlib/tuningcollection.h"
#include "../../../common/FileReader.h"
#include "mpt/io_file/inputfile.hpp"

OPENMPT_NAMESPACE_BEGIN

namespace Scripting
{

#define NewNoteAction(X) \
	X(NOTE_CUT) \
	X(CONTINUE) \
	X(NOTE_OFF) \
	X(NOTE_FADE)

MPT_MAKE_ENUM(NewNoteAction)
#undef NewNoteAction

#define DuplicateCheckType(X) \
	X(NONE) \
	X(NOTE) \
	X(SAMPLE) \
	X(INSTRUMENT) \
	X(PLUGIN)

MPT_MAKE_ENUM(DuplicateCheckType)
#undef DuplicateCheckType

#define DuplicateNoteAction(X) \
	X(NOTE_CUT) \
	X(NOTE_OFF) \
	X(NOTE_FADE)

MPT_MAKE_ENUM(DuplicateNoteAction)
#undef DuplicateNoteAction

#define FilterMode(X) \
	X(UNCHANGED) \
	X(LOWPASS) \
	X(HIGHPASS)

MPT_MAKE_ENUM(FilterMode)
#undef FilterMode

#define ResamplingMode(X) \
	X(NEAREST) \
	X(LINEAR) \
	X(CUBIC_SPLINE) \
	X(SINC_WITH_LOWPASS) \
	X(SINC) \
	X(AMIGA) \
	X(DEFAULT)

MPT_MAKE_ENUM(ResamplingMode)
#undef ResamplingMode

#undef IGNORE
#define PluginVolumeHandling(X) \
	X(MIDI_VOLUME) \
	X(DRY_WET_RATIO) \
	X(IGNORE)

MPT_MAKE_ENUM(PluginVolumeHandling)
#undef PluginVolumeHandling

inline ResamplingMode ToResamplingMode(std::string_view value, const bool allowAmiga)
{
	switch(EnumResamplingMode::Parse(value))
	{
	case EnumResamplingMode::Enum::NEAREST: return SRCMODE_NEAREST;
	case EnumResamplingMode::Enum::LINEAR: return SRCMODE_LINEAR;
	case EnumResamplingMode::Enum::CUBIC_SPLINE: return SRCMODE_CUBIC;
	case EnumResamplingMode::Enum::SINC_WITH_LOWPASS: return SRCMODE_SINC8LP;
	case EnumResamplingMode::Enum::SINC: return SRCMODE_SINC8;
	case EnumResamplingMode::Enum::DEFAULT: return SRCMODE_DEFAULT;
	case EnumResamplingMode::Enum::AMIGA:
		if(allowAmiga)
			return SRCMODE_AMIGA;
		[[fallthrough]];
	default: throw "Invalid resampling mode (must be one of openmpt.Song.resamplingMode)";
	}
}

inline std::string FromResamplingMode(ResamplingMode value)
{
	switch(value)
	{
	case SRCMODE_NEAREST: return EnumResamplingMode::NEAREST;
	case SRCMODE_LINEAR: return EnumResamplingMode::LINEAR;
	case SRCMODE_CUBIC: return EnumResamplingMode::CUBIC_SPLINE;
	case SRCMODE_SINC8LP: return EnumResamplingMode::SINC_WITH_LOWPASS;
	case SRCMODE_SINC8: return EnumResamplingMode::SINC;
	case SRCMODE_DEFAULT: return EnumResamplingMode::DEFAULT;
	case SRCMODE_AMIGA: return EnumResamplingMode::AMIGA;
	}
	return EnumResamplingMode::DEFAULT;
}


struct EnvelopePoint : public Obj
{
	const EnvelopeType m_type;
	const size_t m_pointIndex;

	static const char* EnvTypeToString(EnvelopeType type)
	{
		switch(type)
		{
		case ENV_VOLUME: return "Volume";
		case ENV_PANNING: return "Panning";
		case ENV_PITCH: return "Pitch";
		default: return "";
		}
	}

	static void Register(sol::table &table)
	{
		table.new_usertype<EnvelopePoint>("EnvelopePoint"
			, sol::meta_function::to_string, &Name
			, sol::meta_function::type, &Type
			, "tick", sol::property(&GetTick, &SetTick)
			, "value", sol::property(&GetValue, &SetValue)
			);
	}

	EnvelopePoint(const Obj &obj, size_t instr, EnvelopeType type, size_t pointIndex) noexcept
		: Obj{obj, instr}
		, m_type{type}
		, m_pointIndex{pointIndex} {}

	bool operator==(const EnvelopePoint &other) const noexcept { return IsEqual(other) && std::tie(m_type, m_pointIndex) == std::tie(other.m_type, other.m_pointIndex); }

	EnvelopePoint &operator=(const EnvelopePoint &other)
	{
		if (*this == other)
			return *this;
		auto node = GetEnvelopePoint();
		auto otherNode = other.GetEnvelopePoint();

		*node = *otherNode;
		SndFile()->Instruments[m_index]->GetEnvelope(m_type).Sanitize();

		SetModified(node, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Envelope());
		return *this;
	}

	std::string Name() const { return MPT_AFORMAT("[openmpt.Song.EnvelopePoint object: Instrument {} {} Envelope Point {}]")(m_index, EnvTypeToString(m_type), m_pointIndex); }
	static std::string Type() { return "openmpt.Song.EnvelopePoint"; }

	ScopedLock<EnvelopeNode> GetEnvelopePoint()
	{
		auto sndFile = SndFile();
		if(m_index < 1 || m_index > sndFile->GetNumInstruments() || sndFile->Instruments[m_index] == nullptr)
			throw "Invalid instrument";
		auto &env = sndFile->Instruments[m_index]->GetEnvelope(m_type);
		if(m_pointIndex >= env.size())
			throw "Invalid envelope point";
		return make_lock(env[m_pointIndex], std::move(sndFile));
	}
	ScopedLock<const EnvelopeNode> GetEnvelopePoint() const
	{
		return const_cast<EnvelopePoint *>(this)->GetEnvelopePoint();
	}

	uint32 GetTick() const
	{
		return GetEnvelopePoint()->tick;
	}
	void SetTick(uint32 tick)
	{
		auto node = GetEnvelopePoint();
		node->tick = mpt::saturate_cast<EnvelopeNode::tick_t>(tick);
		SndFile()->Instruments[m_index]->GetEnvelope(m_type).Sanitize();

		SetModified(node, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Envelope());
	}

	float GetValue() const
	{
		auto node = GetEnvelopePoint();
		auto &env = SndFile()->Instruments[m_index]->GetEnvelope(m_type);

		float value = node->value;
		if(m_type == ENV_VOLUME || (m_type == ENV_PITCH && env.dwFlags[ENV_FILTER]))
			return value / ENVELOPE_MAX;
		else
			return (value - ENVELOPE_MID) / (ENVELOPE_MAX * 0.5f);
	}
	void SetValue(float value)
	{
		auto node = GetEnvelopePoint();
		auto &env = SndFile()->Instruments[m_index]->GetEnvelope(m_type);

		if(m_type == ENV_VOLUME || (m_type == ENV_PITCH && env.dwFlags[ENV_FILTER]))
			value *= ENVELOPE_MAX;
		else
			value = (value * (ENVELOPE_MAX * 0.5f)) * ENVELOPE_MID;
		Limit(value, ENVELOPE_MIN, ENVELOPE_MAX);
		node->value = mpt::saturate_round<EnvelopeNode::value_t>(value);

		SndFile()->Instruments[m_index]->GetEnvelope(m_type).Sanitize();

		SetModified(node, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Envelope());
	}
};

struct EnvelopeData : public ProxyContainer<EnvelopePoint>, public Obj
{
	const EnvelopeType m_type;

	EnvelopeData(const Obj &obj, size_t instr, EnvelopeType type) noexcept
		: Obj{obj, instr}
		, m_type{type}
	{
	}

	bool operator==(const EnvelopeData &other) const noexcept { return IsEqual(other) && m_type == other.m_type; }

	EnvelopeData &operator=(const EnvelopeData &other)
	{
		if(*this == other)
			return *this;
		auto env = GetEnvelope();
		auto otherEnv = other.GetEnvelope();

		*static_cast<std::vector<EnvelopeNode> *>(env) = *otherEnv;
		SetModified(env, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Envelope());
		return *this;
	}

	ScopedLock<InstrumentEnvelope> GetEnvelope()
	{
		auto sndFile = SndFile();
		if(m_index > 0 && m_index <= sndFile->GetNumInstruments() && sndFile->Instruments[m_index] != nullptr)
			return make_lock(sndFile->Instruments[m_index]->GetEnvelope(m_type), std::move(sndFile));
		throw "Invalid instrument";
	}
	ScopedLock<const InstrumentEnvelope> GetEnvelope() const
	{
		return const_cast<EnvelopeData *>(this)->GetEnvelope();
	}

	size_t size() override { return GetEnvelope()->size(); }
	iterator begin() override { return iterator{*this, 0}; }
	iterator end() override { return iterator{*this, size()}; }
	value_type operator[](size_t index) override { return EnvelopePoint{*this, m_index, m_type, index}; }
	size_t max_size() const noexcept { return MAX_ENVPOINTS; }

	void push_back(const EnvelopePoint &value)
	{
		static_assert(sol::meta::has_push_back<EnvelopeData>::value);
		auto env = GetEnvelope();
		if(env->size() >= SndFile()->GetModSpecifications().envelopePointsMax)
			throw "Cannot add any more envelope points";
		env->push_back(value.GetEnvelopePoint());
		env->Sanitize();
		SetModified(env, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Envelope());
	}
	void erase(iterator it)
	{
		static_assert(sol::container_detail::has_erase<EnvelopeData>::value);
		auto env = GetEnvelope();
		if(it.m_index < env->size())
		{
			env->erase(env->begin() + it.m_index);
			env->Sanitize();
			SetModified(env, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Envelope());
		}
	}
	void insert(iterator it, const EnvelopePoint &value)
	{
		static_assert(sol::meta::has_insert_with_iterator<EnvelopeData>::value);
		auto env = GetEnvelope();
		if(it.m_index >= env->size())
		{
			if(env->size() >= SndFile()->GetModSpecifications().envelopePointsMax)
				throw "Cannot add any more envelope points";
			env->resize(it.m_index + 1u);
		}
		(*this)[it.m_index] = value;
	}
};


struct Envelope : public Obj
{
	EnvelopeData envelopeData;

	static void Register(sol::table &table)
	{
		table.new_usertype<Envelope>("Envelope"
			, sol::meta_function::to_string, &Name
			, sol::meta_function::type, &Type

			, "enabled", sol::property(&GetEnabled, &SetEnabled)
			, "carry", sol::property(&GetCarry, &SetCarry)
			, "points", &Envelope::envelopeData
			, "loop_enabled", sol::property(&GetLoopEnabled, &SetLoopEnabled)
			, "loop_start", sol::property(&GetLoopStart, &SetLoopStart)
			, "loop_end", sol::property(&GetLoopEnd, &SetLoopEnd)
			, "sustain_loop_enabled", sol::property(&GetSustainLoopEnabled, &SetSustainLoopEnabled)
			, "sustain_loop_start", sol::property(&GetSustainLoopStart, &SetSustainLoopStart)
			, "sustain_loop_end", sol::property(&GetSustainLoopEnd, &SetSustainLoopEnd)
			, "release_node", sol::property(&GetReleaseNode, &SetReleaseNode)
			);
	}

	Envelope(const Obj &obj, size_t instr, EnvelopeType type) noexcept
		: Obj{obj, instr}
		, envelopeData{obj, instr, type} {}

	bool operator==(const Envelope &other) const noexcept { return envelopeData == other.envelopeData; }

	Envelope &operator=(const Envelope &other)
	{
		if(*this == other)
			return *this;
		auto env = GetEnvelope();
		auto otherEnv = other.GetEnvelope();

		*env = *otherEnv;
		if(envelopeData.m_type != ENV_VOLUME)
			env->nReleaseNode = ENV_RELEASE_NODE_UNSET;
		if(envelopeData.m_type != ENV_PITCH)
			env->dwFlags.reset(ENV_FILTER);
		SetModified(env, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Envelope());
		return *this;
	}

	std::string Name() const { return MPT_AFORMAT("[openmpt.Song.Envelope object: Instrument {} {} Envelope]")(m_index, EnvelopePoint::EnvTypeToString(envelopeData.m_type)); }
	static std::string Type() { return "openmpt.Song.Envelope"; }

	ScopedLock<InstrumentEnvelope> GetEnvelope() { return envelopeData.GetEnvelope(); }
	ScopedLock<const InstrumentEnvelope> GetEnvelope() const { return envelopeData.GetEnvelope(); }

	bool GetEnabled() const
	{
		return GetEnvelope()->dwFlags[ENV_ENABLED];
	}
	void SetEnabled(bool enabled)
	{
		auto envelope = GetEnvelope();
		envelope->dwFlags.set(ENV_ENABLED, enabled);
		SetModified(envelope, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Envelope());
	}

	bool GetCarry() const
	{
		return GetEnvelope()->dwFlags[ENV_CARRY];
	}
	void SetCarry(bool enabled)
	{
		auto envelope = GetEnvelope();
		if(SndFile()->GetType() == MOD_TYPE_XM)
			throw "Envelope carry is not supported by this format";
		envelope->dwFlags.set(ENV_CARRY, enabled);
		SetModified(envelope, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Envelope());
	}

	bool GetLoopEnabled() const
	{
		return GetEnvelope()->dwFlags[ENV_LOOP];
	}
	void SetLoopEnabled(bool enabled)
	{
		auto envelope = GetEnvelope();
		envelope->dwFlags.set(ENV_LOOP, enabled);
		SetModified(envelope, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Envelope());
	}

	uint32 GetLoopStart() const
	{
		return GetEnvelope()->nLoopStart + 1;
	}
	void SetLoopStart(uint32 point)
	{
		auto envelope = GetEnvelope();
		envelope->nLoopStart = mpt::saturate_cast<uint8>(point ? point - 1 : 0);
		envelope->Sanitize();
		SetModified(envelope, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Envelope());
	}

	uint32 GetLoopEnd() const
	{
		return GetEnvelope()->nLoopEnd + 1;
	}
	void SetLoopEnd(uint32 point)
	{
		auto envelope = GetEnvelope();
		envelope->nLoopEnd = mpt::saturate_cast<uint8>(point ? point - 1 : 0);
		envelope->Sanitize();
		SetModified(envelope, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Envelope());
	}

	bool GetSustainLoopEnabled() const
	{
		return GetEnvelope()->dwFlags[ENV_SUSTAIN];
	}
	void SetSustainLoopEnabled(bool enabled)
	{
		auto envelope = GetEnvelope();
		envelope->dwFlags.set(ENV_SUSTAIN, enabled);
		SetModified(envelope, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Envelope());
	}

	uint32 GetSustainLoopStart() const
	{
		return GetEnvelope()->nSustainStart + 1;
	}
	void SetSustainLoopStart(uint32 point)
	{
		auto envelope = GetEnvelope();
		envelope->nSustainStart = mpt::saturate_cast<uint8>(point ? point - 1 : 0);
		if(SndFile()->GetType() == MOD_TYPE_XM)
			envelope->nSustainEnd = envelope->nSustainStart;
		envelope->Sanitize();
		SetModified(envelope, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Envelope());
	}

	uint32 GetSustainLoopEnd() const
	{
		return GetEnvelope()->nSustainEnd + 1;
	}
	void SetSustainLoopEnd(uint32 point)
	{
		auto envelope = GetEnvelope();
		envelope->nSustainEnd = mpt::saturate_cast<uint8>(point ? point - 1 : 0);
		if(SndFile()->GetType() == MOD_TYPE_XM)
			envelope->nSustainStart = envelope->nSustainEnd;
		envelope->Sanitize();
		SetModified(envelope, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Envelope());
	}

	sol::optional<uint32> GetReleaseNode() const
	{
		auto envelope = GetEnvelope();
		if(envelopeData.m_type != ENV_VOLUME)
			return {};
		if(envelope->nReleaseNode == ENV_RELEASE_NODE_UNSET)
			return {};
		return envelope->nReleaseNode + 1;
	}
	void SetReleaseNode(sol::optional<uint32> node)
	{
		auto envelope = GetEnvelope();
		if(node && SndFile()->GetType() == MOD_TYPE_MPT)
			throw "Release node is not supported in this format";
		if(envelopeData.m_type != ENV_VOLUME)
			throw "Release node is only supported for volume envelopes";
		envelope->nReleaseNode = node ? mpt::saturate_cast<uint8>(std::max(node.value(), uint32(1)) - 1) : ENV_RELEASE_NODE_UNSET;
		envelope->Sanitize();
		SetModified(envelope, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Envelope());
	}
};


struct NoteMapEntry : public Obj
{
	const size_t m_noteIndex;

	static void Register(sol::table &table)
	{
		table.new_usertype<NoteMapEntry>("NoteMapEntry"
			, sol::meta_function::to_string, &Name
			, sol::meta_function::type, &Type

			, "note", sol::property(&GetNote, &SetNote)
			, "sample", sol::property(&GetSample, &SetSample)
			);
	}

	NoteMapEntry(const Obj &obj, size_t instr, size_t noteIndex) noexcept
		: Obj{obj, instr}
		, m_noteIndex{noteIndex} {}

	bool operator==(const NoteMapEntry &other) const noexcept { return IsEqual(other) && m_noteIndex == other.m_noteIndex; }

	NoteMapEntry &operator=(const NoteMapEntry &other)
	{
		if(*this == other)
			return *this;
		auto instr = GetInstrument();
		auto otherInstr = other.GetInstrument();

		*instr = *otherInstr;
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info().Envelope().Names());
		return *this;
	}

	std::string Name() const { return MPT_AFORMAT("[openmpt.Song.NoteMapEntry object: Instrument {}]")(m_index); }
	static std::string Type() { return "openmpt.Song.NoteMapEntry"; }

	ScopedLock<ModInstrument> GetInstrument()
	{
		auto sndFile = SndFile();
		if(m_index > 0 && m_index <= sndFile->GetNumInstruments() && sndFile->Instruments[m_index] != nullptr)
			return make_lock(*sndFile->Instruments[m_index], std::move(sndFile));
		throw "Invalid instrument";
	}
	ScopedLock<const ModInstrument> GetInstrument() const
	{
		return const_cast<NoteMapEntry *>(this)->GetInstrument();
	}

	ModCommand::NOTE GetNote() const
	{
		auto instr = GetInstrument();
		return instr->NoteMap[m_index - 1];
	}
	void SetNote(ModCommand::NOTE note)
	{
		if(note < 1 || note > NOTE_MAX - NOTE_MIN + 1)
			return;
		auto instr = GetInstrument();
		instr->NoteMap[m_index - 1] = note;
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	Sample GetSample() const
	{
		auto instr = GetInstrument();
		return Sample{*this, instr->Keyboard[m_index - 1]};
	}
	void SetSample(sol::object sample)
	{
		auto instr = GetInstrument();
		SAMPLEINDEX smp = 0;
		if(sample.is<Sample>())
			smp = static_cast<SAMPLEINDEX>(sample.as<Sample>().m_index);
		else if(sample.is<SAMPLEINDEX>())
			smp = sample.as<SAMPLEINDEX>();
		else if(!sample.valid())
			smp = 0;
		else
			throw "Invalid type (index or sample object expected)";
		
		if(smp > MAX_SAMPLES)
			smp = 0;
		
		if(smp != instr->Keyboard[m_index - 1])
		{
			instr->Keyboard[m_index - 1] = smp;
			SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
		}
	}
};


struct Instrument : public Obj
{
	static void Register(sol::table &table)
	{
		table.new_usertype<Instrument>("Instrument"
			, sol::meta_function::to_string, &Name
			, sol::meta_function::type, &Type

			, "index", sol::readonly(&Instrument::m_index)
			, "name", sol::property(&GetName, &SetName)
			, "filename", sol::property(&GetFilename, &SetFilename)
			, "panning", sol::property(&GetPanning, &SetPanning)
			, "global_volume", sol::property(&GetGlobalVolume, &SetGlobalVolume)
			, "samples", sol::property(&GetSamples)
			, "volume_envelope", sol::property(&GetVolumeEnvelope, &SetVolumeEnvelope)
			, "panning_envelope", sol::property(&GetPanningEnvelope, &SetPanningEnvelope)
			, "pitch_envelope", sol::property(&GetPitchEnvelope, &SetPitchEnvelope)
			, "pitch_tempo_lock", sol::property(&GetPitchTempoLock, &SetPitchTempoLock)
			, "fadeout", sol::property(&GetFadeout, &SetFadeout)
			, "pitch_pan_separation", sol::property(&GetPitchPanSeparation, &SetPitchPanSeparation)
			, "pitch_pan_separation_center", sol::property(&GetPitchPanSeparationCenter, &SetPitchPanSeparationCenter)
			, "ramp_in", sol::property(&GetRampIn, &SetRampIn)
			, "resampling", sol::property(&GetResampling, &SetResampling)
			, "volume_random_variation", sol::property(&GetVolumeRandomVariation, &SetVolumeRandomVariation)
			, "panning_random_variation", sol::property(&GetPanningRandomVariation, &SetPanningRandomVariation)
			, "cutoff_random_variation", sol::property(&GetCutoffRandomVariation, &SetCutoffRandomVariation)
			, "resonance_random_variation", sol::property(&GetResonanceRandomVariation, &SetResonanceRandomVariation)
			, "filter_cutoff", sol::property(&GetFilterCutoff, &SetFilterCutoff)
			, "filter_resonance", sol::property(&GetFilterResonance, &SetFilterResonance)
			, "filter_mode", sol::property(&GetFilterMode, &SetFilterMode)
			, "new_note_action", sol::property(&GetNewNoteAction, &SetNewNoteAction)
			, "duplicate_note_check", sol::property(&GetDuplicateNoteCheck, &SetDuplicateNoteCheck)
			, "duplicate_note_action", sol::property(&GetDuplicateNoteAction, &SetDuplicateNoteAction)
			, "plugin", sol::property(&GetPlugin, &SetPlugin)
			, "midi_channel", sol::property(&GetMidiChannel, &SetMidiChannel)
			, "midi_program", sol::property(&GetMidiProgram, &SetMidiProgram)
			, "midi_bank", sol::property(&GetMidiBank, &SetMidiBank)
			, "pitch_bend_range", sol::property(&GetPitchBendRange, &SetPitchBendRange)
			, "volume_command_handling", sol::property(&GetVolumeCommandHandling, &SetVolumeCommandHandling)
			, "volume_commands_with_notes_are_velocities", sol::property(&GetVolumeCommandsWithNotesAreVelocities, &SetVolumeCommandsWithNotesAreVelocities)
			, "tuning", sol::property(&GetTuning, &SetTuning)
			, "note_map", sol::property(&GetNoteMap, &SetNoteMap)

			, "load", &Instrument::Load
			, "play", &Instrument::PlayNote
			, "stop", &Instrument::StopNote
			// etc.
			);
	}

	Instrument(const Obj &obj, size_t instr) noexcept
		: Obj{obj, instr} {}

	bool operator==(const Instrument &other) const noexcept { return IsEqual(other); }

	Instrument &operator=(const Instrument &other)
	{
		if(*this == other)
			return *this;
		auto instr = GetInstrument();
		auto otherInstr = other.GetInstrument();

		*instr = *otherInstr;
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info().Envelope().Names());
		return *this;
	}

	std::string Name() const { return MPT_AFORMAT("[openmpt.Song.Instrument object: Instrument {}]")(m_index); }
	static std::string Type() { return "openmpt.Song.Instrument"; }

	ScopedLock<ModInstrument> GetInstrument()
	{
		auto sndFile = SndFile();
		if(m_index > 0 && m_index <= sndFile->GetNumInstruments() && sndFile->Instruments[m_index] != nullptr)
			return make_lock(*sndFile->Instruments[m_index], std::move(sndFile));
		throw "Invalid instrument";
	}
	ScopedLock<const ModInstrument> GetInstrument() const
	{
		return const_cast<Instrument *>(this)->GetInstrument();
	}

	std::string GetName() const
	{
		return FromSndFileString(GetInstrument()->name);
	}
	void SetName(const std::string &name)
	{
		auto sndFile = SndFile();
		GetInstrument()->name = ToSndFileString(name.substr(0, sndFile->GetModSpecifications().instrNameLengthMax), sndFile);
		SetModified(sndFile, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	std::string GetFilename() const
	{
		return FromSndFileString(GetInstrument()->filename);
	}
	void SetFilename(const std::string &filename)
	{
		auto sndFile = SndFile();
		GetInstrument()->filename = ToSndFileString(filename.substr(0, sndFile->GetModSpecifications().instrFilenameLengthMax), sndFile);
		SetModified(sndFile, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	sol::optional<double> GetPanning() const
	{
		auto instr = GetInstrument();
		if(instr->dwFlags[INS_SETPANNING])
			return GetInstrument()->nPan / 128.0 - 1.0;
		else
			return {};
	}
	void SetPanning(sol::optional<double> panning)
	{
		auto instr = GetInstrument();
		instr->dwFlags.set(INS_SETPANNING, !!panning);
		if(panning)
			instr->nPan = mpt::saturate_round<uint16>(std::clamp(panning.value(), -1.0, 1.0) * 128.0 + 128.0);
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	double GetGlobalVolume() const
	{
		return GetInstrument()->nGlobalVol / 64.0;
	}
	void SetGlobalVolume(double volume)
	{
		auto instr = GetInstrument();
		instr->nGlobalVol = mpt::saturate_round<uint16>(std::clamp(volume, 0.0, 1.0) * 64.0);
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	Envelope GetVolumeEnvelope() const
	{
		return Envelope{*this, m_index, ENV_VOLUME};
	}
	void SetVolumeEnvelope(const Envelope &env)
	{
		GetVolumeEnvelope() = env;
	}

	Envelope GetPanningEnvelope() const
	{
		return Envelope{*this, m_index, ENV_PANNING};
	}
	void SetPanningEnvelope(const Envelope &env)
	{
		GetPanningEnvelope() = env;
	}

	sol::optional<Envelope> GetPitchEnvelope() const
	{
		if(SndFile()->GetType() == MOD_TYPE_XM)
			return {};
		return Envelope{*this, m_index, ENV_PITCH};
	}
	void SetPitchEnvelope(const Envelope &env)
	{
		auto pitchEnv = GetPitchEnvelope();
		if(pitchEnv)
			*pitchEnv = env;
		else
			throw "Envelope type is not supported by this format";
	}

	sol::optional<double> GetPitchTempoLock() const
	{
		auto instr = GetInstrument();
		if(instr->pitchToTempoLock.GetRaw() > 0)
			return instr->pitchToTempoLock.ToDouble();
		else
			return {};
	}
	void SetPitchTempoLock(sol::optional<double> ptt)
	{
		auto instr = GetInstrument();
		if(SndFile()->GetType() != MOD_TYPE_MPT)
			throw "Feature is not supported by this format";
		instr->pitchToTempoLock = TEMPO(ptt.value_or(0.0));
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	sol::optional<double> GetFilterCutoff() const
	{
		auto instr = GetInstrument();
		if(instr->IsCutoffEnabled())
			return instr->GetCutoff() / 127.0;
		else
			return {};
	}
	void SetFilterCutoff(sol::optional<double> cutoff)
	{
		auto instr = GetInstrument();
		if(SndFile()->GetType() == MOD_TYPE_XM)
			throw "Feature is not supported by this format";
		if(cutoff)
			instr->SetCutoff(mpt::saturate_round<uint8>(*cutoff * 127.0), true);
		else
			instr->SetCutoff(instr->GetCutoff(), false);
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	sol::optional<double> GetFilterResonance() const
	{
		auto instr = GetInstrument();
		if(instr->IsResonanceEnabled())
			return instr->GetResonance() / 127.0;
		else
			return {};
	}
	void SetFilterResonance(sol::optional<double> resonance)
	{
		auto instr = GetInstrument();
		if(SndFile()->GetType() == MOD_TYPE_XM)
			throw "Feature is not supported by this format";
		if(resonance)
			instr->SetResonance(mpt::saturate_round<uint8>(*resonance * 127.0), true);
		else
			instr->SetResonance(instr->GetResonance(), false);
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	sol::as_table_t<std::vector<Sample>> GetSamples() const
	{
		const auto sndFile = SndFile();
		const auto samples = GetInstrument()->GetSamples();
		std::vector<Sample> objects;
		objects.reserve(samples.size());
		for(auto s : samples)
		{
			if(s <= sndFile->GetNumSamples())
				objects.push_back(Sample{*this, s});
		}
		return sol::as_table(std::move(objects));
	}

	sol::as_table_t<std::vector<NoteMapEntry>> GetNoteMap() const
	{
		const auto sndFile = SndFile();
		std::vector<NoteMapEntry> objects;
		objects.reserve(NOTE_MAX - NOTE_MIN + 1);
		for(size_t i= 0; i < NOTE_MAX - NOTE_MIN + 1; i++)
		{
			objects.push_back(NoteMapEntry{*this, m_index, i});
		}
		return sol::as_table(std::move(objects));
	}
	void SetNoteMap(sol::table notes)
	{
		auto instr = GetInstrument();
		instr->ResetNoteMap();
		instr->AssignSample(0);
		for(const auto n : notes)
		{
			if(n.second.is<NoteMapEntry>())
			{
				const auto entry = n.second.as<NoteMapEntry>();
				instr->Keyboard[entry.m_index] = entry.GetInstrument()->Keyboard[entry.m_index];
				instr->NoteMap[entry.m_index] = entry.GetInstrument()->NoteMap[entry.m_index];
			} else if (n.second.is<sol::table>())
			{
				const auto entry = n.second.as<sol::table>();
				instr->Keyboard[n.first.as<int>()] = entry["sample"].get_or(SAMPLEINDEX(0));
				instr->NoteMap[n.first.as<int>()] = Clamp(entry["note"].get_or(NOTE_NONE), NOTE_MIN, NOTE_MAX);
			}
		}
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	sol::object GetTuning() const
	{
		// TODO use tuning objects
		auto instr = GetInstrument();
		if(instr->pTuning == nullptr)
			return sol::make_object(m_script, sol::nil);
		else
			return sol::make_object(m_script, mpt::ToCharset(mpt::Charset::UTF8, instr->pTuning->GetName()));
	}
	void SetTuning(const std::string &tuningName)
	{
		auto instr = GetInstrument();
		if(tuningName.empty())
		{
			instr->pTuning = nullptr;
			SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
			return;
		}
		const auto uName = mpt::ToUnicode(mpt::Charset::UTF8, tuningName);
		for(const auto &tuning : SndFile()->GetTuneSpecificTunings())
		{
			if(tuning->GetName() == uName)
			{
				instr->pTuning = tuning.get();
				SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
				return;
			}
		}
		throw "Unknown tuning!";
	}

	sol::optional<CHANNELINDEX> PlayNote(int note, sol::optional<double> volume, sol::optional<double> panning)
	{
		if(note < 0 || note > NOTE_MAX - NOTE_MIN)
			return {};

		PlayNoteParam param = PlayNoteParam(static_cast<ModCommand::NOTE>(note + NOTE_MIN)).Instrument(static_cast<INSTRUMENTINDEX>(m_index));
		if(volume)
			param.Volume(mpt::saturate_round<int32>(Clamp(volume.value(), 0.0, 1.0) * 256.0));
		if(panning)
			param.Panning(mpt::saturate_round<int32>(Clamp(panning.value(), -1.0, 1.0) * 128.0 + 128.0));

		CHANNELINDEX chn;
		Script::CallInGUIThread([this, &param, &chn]()
		{
			// Mustn't hold lock
			chn = m_modDoc.PlayNote(param);
		});
		if(chn == CHANNELINDEX_INVALID)
		{
			return {};
		}
		return chn;
	}

	void StopNote(int channel)
	{
		Script::CallInGUIThread([this, channel]()
		{
			// Mustn't hold lock
			m_modDoc.NoteOff(NOTE_NONE, false, INSTRUMENTINDEX_INVALID, static_cast<CHANNELINDEX>(channel));
		});
	}

	int32 GetFadeout() const
	{
		return GetInstrument()->nFadeOut;
	}
	void SetFadeout(int32 fadeout)
	{
		auto instr = GetInstrument();
		const bool extendedFadeoutRange = !(SndFile()->GetType() & MOD_TYPE_IT);
		instr->nFadeOut = static_cast<uint16>(std::clamp(fadeout, 0, extendedFadeoutRange ? 32767 : 8192));
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	int8 GetPitchPanSeparation() const
	{
		return GetInstrument()->nPPS;
	}
	void SetPitchPanSeparation(int8 pps)
	{
		auto instr = GetInstrument();
		if(SndFile()->GetType() == MOD_TYPE_XM)
			throw "Feature is not supported by this format";
		instr->nPPS = std::clamp(pps, int8(-32), int8(32));
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	uint8 GetPitchPanSeparationCenter() const
	{
		return GetInstrument()->nPPC + NOTE_MIN;
	}
	void SetPitchPanSeparationCenter(uint8 ppc)
	{
		auto instr = GetInstrument();
		if(SndFile()->GetType() == MOD_TYPE_XM)
			throw "Feature is not supported by this format";
		instr->nPPC = std::clamp(ppc, uint8(NOTE_MIN), uint8(NOTE_MAX)) - NOTE_MIN;
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	int GetRampIn() const
	{
		return GetInstrument()->nVolRampUp;
	}
	void SetRampIn(int rampUp)
	{
		auto instr = GetInstrument();
		if(SndFile()->GetType() != MOD_TYPE_MPT)
			throw "Feature is not supported by this format";
		instr->nVolRampUp = static_cast<uint16>(std::clamp(rampUp, 0, 2000));
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	std::string GetResampling() const
	{
		return FromResamplingMode(GetInstrument()->resampling);
	}
	void SetResampling(std::string_view mode)
	{
		auto instr = GetInstrument();
		if(SndFile()->GetType() != MOD_TYPE_MPT)
			throw "Feature is not supported by this format";
		instr->resampling = ToResamplingMode(mode, false);
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	double GetVolumeRandomVariation() const
	{
		return GetInstrument()->nVolSwing / 100.0;
	}
	void SetVolumeRandomVariation(double swing)
	{
		auto instr = GetInstrument();
		instr->nVolSwing = mpt::saturate_round<uint8>(std::clamp(swing, 0.0, 1.0) * 100.0);
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	double GetPanningRandomVariation() const
	{
		return GetInstrument()->nPanSwing / 64.0;
	}
	void SetPanningRandomVariation(double swing)
	{
		auto instr = GetInstrument();
		instr->nPanSwing = mpt::saturate_round<uint8>(std::clamp(swing, 0.0, 1.0) * 64.0);
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	double GetCutoffRandomVariation() const
	{
		return GetInstrument()->nCutSwing / 64.0;
	}
	void SetCutoffRandomVariation(double swing)
	{
		auto instr = GetInstrument();
		instr->nCutSwing = mpt::saturate_round<uint8>(std::clamp(swing, 0.0, 1.0) * 64.0);
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	double GetResonanceRandomVariation() const
	{
		return GetInstrument()->nResSwing / 64.0;
	}
	void SetResonanceRandomVariation(double swing)
	{
		auto instr = GetInstrument();
		instr->nResSwing = mpt::saturate_round<uint8>(std::clamp(swing, 0.0, 1.0) * 64.0);
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	std::string GetFilterMode() const
	{
		switch(GetInstrument()->filterMode)
		{
		case FilterMode::Unchanged: return EnumFilterMode::UNCHANGED;
		case FilterMode::LowPass: return EnumFilterMode::LOWPASS;
		case FilterMode::HighPass: return EnumFilterMode::HIGHPASS;
		}
		return EnumFilterMode::UNCHANGED;
	}
	void SetFilterMode(std::string_view mode)
	{
		auto instr = GetInstrument();
		if(SndFile()->GetType() == MOD_TYPE_XM)
			throw "Feature is not supported by this format";
		switch(EnumFilterMode::Parse(mode))
		{
		case EnumFilterMode::Enum::UNCHANGED: instr->filterMode = FilterMode::Unchanged; break;
		case EnumFilterMode::Enum::LOWPASS: instr->filterMode = FilterMode::LowPass; break;
		case EnumFilterMode::Enum::HIGHPASS: instr->filterMode = FilterMode::HighPass; break;
		case EnumFilterMode::Enum::INVALID_VALUE: throw "Invalid filter mode";
		}
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	std::string GetNewNoteAction() const
	{
		switch(GetInstrument()->nNNA)
		{
		case NewNoteAction::NoteCut: return EnumNewNoteAction::NOTE_CUT;
		case NewNoteAction::Continue: return EnumNewNoteAction::CONTINUE;
		case NewNoteAction::NoteOff: return EnumNewNoteAction::NOTE_OFF;
		case NewNoteAction::NoteFade: return EnumNewNoteAction::NOTE_FADE;
		}
		return EnumNewNoteAction::NOTE_CUT;
	}
	void SetNewNoteAction(std::string_view nna)
	{
		auto instr = GetInstrument();
		if(SndFile()->GetType() == MOD_TYPE_XM)
			throw "Feature is not supported by this format";
		switch(EnumNewNoteAction::Parse(nna))
		{
		case EnumNewNoteAction::Enum::NOTE_CUT: instr->nNNA = NewNoteAction::NoteCut; break;
		case EnumNewNoteAction::Enum::CONTINUE: instr->nNNA = NewNoteAction::Continue; break;
		case EnumNewNoteAction::Enum::NOTE_OFF: instr->nNNA = NewNoteAction::NoteOff; break;
		case EnumNewNoteAction::Enum::NOTE_FADE: instr->nNNA = NewNoteAction::NoteFade; break;
		case EnumNewNoteAction::Enum::INVALID_VALUE: throw "Invalid NNA value";
		}
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	std::string GetDuplicateNoteCheck() const
	{
		switch(GetInstrument()->nDCT)
		{
		case DuplicateCheckType::None: return EnumDuplicateCheckType::NONE;
		case DuplicateCheckType::Note: return EnumDuplicateCheckType::NOTE;
		case DuplicateCheckType::Sample: return EnumDuplicateCheckType::SAMPLE;
		case DuplicateCheckType::Instrument: return EnumDuplicateCheckType::INSTRUMENT;
		case DuplicateCheckType::Plugin: return EnumDuplicateCheckType::PLUGIN;
		}
		return EnumDuplicateCheckType::NONE;
	}
	void SetDuplicateNoteCheck(std::string_view dct)
	{
		auto instr = GetInstrument();
		if(SndFile()->GetType() == MOD_TYPE_XM)
			throw "Feature is not supported by this format";
		switch(EnumDuplicateCheckType::Parse(dct))
		{
		case EnumDuplicateCheckType::Enum::NONE: instr->nDCT = DuplicateCheckType::None; break;
		case EnumDuplicateCheckType::Enum::NOTE: instr->nDCT = DuplicateCheckType::Note; break;
		case EnumDuplicateCheckType::Enum::SAMPLE: instr->nDCT = DuplicateCheckType::Sample; break;
		case EnumDuplicateCheckType::Enum::INSTRUMENT: instr->nDCT = DuplicateCheckType::Instrument; break;
		case EnumDuplicateCheckType::Enum::PLUGIN: instr->nDCT = DuplicateCheckType::Plugin; break;
		case EnumDuplicateCheckType::Enum::INVALID_VALUE: throw "Invalid DCT value";
		}
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	std::string GetDuplicateNoteAction() const
	{
		switch(GetInstrument()->nDNA)
		{
		case DuplicateNoteAction::NoteCut: return EnumDuplicateNoteAction::NOTE_CUT;
		case DuplicateNoteAction::NoteOff: return EnumDuplicateNoteAction::NOTE_OFF;
		case DuplicateNoteAction::NoteFade: return EnumDuplicateNoteAction::NOTE_FADE;
		}
		return EnumDuplicateNoteAction::NOTE_CUT;
	}
	void SetDuplicateNoteAction(std::string_view dna)
	{
		auto instr = GetInstrument();
		if(SndFile()->GetType() == MOD_TYPE_XM)
			throw "Feature is not supported by this format";
		switch(EnumDuplicateNoteAction::Parse(dna))
		{
		case EnumDuplicateNoteAction::Enum::NOTE_CUT: instr->nDNA = DuplicateNoteAction::NoteCut; break;
		case EnumDuplicateNoteAction::Enum::NOTE_OFF: instr->nDNA = DuplicateNoteAction::NoteOff; break;
		case EnumDuplicateNoteAction::Enum::NOTE_FADE: instr->nDNA = DuplicateNoteAction::NoteFade; break;
		case EnumDuplicateNoteAction::Enum::INVALID_VALUE: throw "Invalid DNA value";
		}
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	sol::optional<uint32> GetPlugin() const
	{
		auto instr = GetInstrument();
		if(instr->nMixPlug == 0)
			return {};
		return instr->nMixPlug;
	}
	void SetPlugin(sol::optional<uint32> plugin)
	{
		auto instr = GetInstrument();
		if(plugin && plugin <= MAX_MIXPLUGINS)
			instr->nMixPlug = static_cast<PLUGINDEX>(plugin.value());
		else
			instr->nMixPlug = 0;
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	sol::optional<uint32> GetMidiChannel() const
	{
		auto instr = GetInstrument();
		if(instr->nMidiChannel == 0)
			return {};
		return instr->nMidiChannel;
	}
	void SetMidiChannel(sol::optional<int> channel)
	{
		auto instr = GetInstrument();
		if(channel && channel >= MidiFirstChannel && channel <= MidiMappedChannel)
			instr->nMidiChannel = static_cast<uint8>(*channel);
		else
			instr->nMidiChannel = 0;
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	sol::optional<uint32> GetMidiProgram() const
	{
		auto instr = GetInstrument();
		if(instr->nMidiProgram == 0)
			return {};
		return instr->nMidiProgram;
	}
	void SetMidiProgram(sol::optional<uint32> program)
	{
		auto instr = GetInstrument();
		if(program)
			instr->nMidiProgram = mpt::saturate_cast<uint8>(std::clamp(program.value(), 1u, 128u));
		else
			instr->nMidiProgram = 0;
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	sol::optional<uint32> GetMidiBank() const
	{
		auto instr = GetInstrument();
		if(instr->wMidiBank == 0)
			return {};
		return instr->wMidiBank;
	}
	void SetMidiBank(sol::optional<uint32> bank)
	{
		auto instr = GetInstrument();
		if(bank)
			instr->wMidiBank = mpt::saturate_cast<uint16>(std::clamp(bank.value(), 1u, 16384u));
		else
			instr->wMidiBank = 0;
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	int8 GetPitchBendRange() const
	{
		return GetInstrument()->midiPWD;
	}
	void SetPitchBendRange(int8 range)
	{
		auto instr = GetInstrument();
		instr->midiPWD = range;
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	std::string GetVolumeCommandHandling() const
	{
		switch(GetInstrument()->pluginVolumeHandling)
		{
		case PLUGIN_VOLUMEHANDLING_MIDI: return EnumPluginVolumeHandling::MIDI_VOLUME;
		case PLUGIN_VOLUMEHANDLING_DRYWET: return EnumPluginVolumeHandling::DRY_WET_RATIO;
		case PLUGIN_VOLUMEHANDLING_CUSTOM:
		case PLUGIN_VOLUMEHANDLING_MAX:
		case PLUGIN_VOLUMEHANDLING_IGNORE: return EnumPluginVolumeHandling::IGNORE;
		}
		return EnumPluginVolumeHandling::IGNORE;
	}
	void SetVolumeCommandHandling(std::string_view handling)
	{
		auto instr = GetInstrument();
		switch(EnumPluginVolumeHandling::Parse(handling))
		{
		case EnumPluginVolumeHandling::Enum::MIDI_VOLUME: instr->pluginVolumeHandling = PLUGIN_VOLUMEHANDLING_MIDI; break;
		case EnumPluginVolumeHandling::Enum::DRY_WET_RATIO: instr->pluginVolumeHandling = PLUGIN_VOLUMEHANDLING_DRYWET; break;
		case EnumPluginVolumeHandling::Enum::IGNORE: instr->pluginVolumeHandling = PLUGIN_VOLUMEHANDLING_IGNORE; break;
		case EnumPluginVolumeHandling::Enum::INVALID_VALUE: throw "Invalid volume handling value";
		}
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	bool GetVolumeCommandsWithNotesAreVelocities() const
	{
		return GetInstrument()->pluginVelocityHandling == PLUGIN_VELOCITYHANDLING_CHANNEL;
	}
	void SetVolumeCommandsWithNotesAreVelocities(bool velocities)
	{
		auto instr = GetInstrument();
		instr->pluginVelocityHandling = velocities ? PLUGIN_VELOCITYHANDLING_CHANNEL : PLUGIN_VELOCITYHANDLING_VOLUME;
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	bool Load(const std::string &filename)
	{
		mpt::IO::InputFile f{mpt::PathString::FromUTF8(filename)};
		if(!f.IsValid())
			return false;
		FileReader file = GetFileReader(f);
		if(!file.IsValid())
			return false;

		auto sndFile = SndFile();
		bool result = sndFile->ReadInstrumentFromFile(static_cast<INSTRUMENTINDEX>(m_index), file, false);
		if(result)
			SetModified(sndFile, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info().Envelope().Names());
		return result;
	}
};

struct InstrumentList final : public ProxyContainer<Instrument>, public Obj
{
	InstrumentList(const Obj &obj) noexcept
		: Obj{obj} {}

	InstrumentList &operator=(const InstrumentList &other)
	{
		auto sndFile = SndFile();
		auto otherSndFile = other.SndFile();
		if(sndFile == otherSndFile)
			return *this;

		for(INSTRUMENTINDEX ins = otherSndFile->GetNumInstruments() + 1; ins <= sndFile->GetNumInstruments(); ins++)
		{
			sndFile->DestroyInstrument(ins, doNoDeleteAssociatedSamples);
		}
		sndFile->m_nInstruments = otherSndFile->GetNumInstruments();
		for(INSTRUMENTINDEX ins = 1; ins <= sndFile->GetNumInstruments(); ins++)
		{
			if(otherSndFile->Instruments[ins])
			{
				ModInstrument *instr = sndFile->AllocateInstrument(ins);
				if(instr)
				{
					*instr = *otherSndFile->Instruments[ins];
					if(instr->pTuning && sndFile != otherSndFile)
						instr->pTuning = sndFile->GetTuneSpecificTunings().AddTuning(std::make_unique<CTuning>(*instr->pTuning));
				}
			} else if(sndFile->Instruments[ins])
			{
				*sndFile->Instruments[ins] = ModInstrument{};
			}
		}

		SetModified(sndFile, InstrumentHint().Info().Envelope().Names());
		return *this;
	}

	size_t size() override { return SndFile()->GetNumInstruments(); }
	iterator begin() override { return iterator{*this, 1}; }
	iterator end() override { return iterator{*this, SndFile()->GetNumInstruments() + 1u}; }
	value_type operator[](size_t index) override { return Instrument{*this, index}; }
	size_t max_size() const noexcept { return MAX_INSTRUMENTS; }

	void push_back(const Instrument &value)
	{
		static_assert(sol::meta::has_push_back<SampleList>::value);
		auto sndFile = SndFile();
		if(!sndFile->CanAddMoreInstruments())
			throw "Cannot add any more instruments";
		sndFile->AllocateInstrument(sndFile->GetNumInstruments() + 1);
		(*this)[sndFile->GetNumInstruments()] = value;
	}
	void erase(iterator it)
	{
		static_assert(sol::container_detail::has_erase<InstrumentList>::value);
		auto sndFile = SndFile();
		sndFile->DestroyInstrument(static_cast<INSTRUMENTINDEX>(it.m_index), doNoDeleteAssociatedSamples);
		if(it.m_index == sndFile->GetNumInstruments())
		{
			sndFile->m_nInstruments--;
		}
		SetModified(sndFile, InstrumentHint(static_cast<INSTRUMENTINDEX>(it.m_index)).Info().Envelope().Names());
	}
	void insert(iterator it, const Instrument &value)
	{
		static_assert(sol::meta::has_insert_with_iterator<InstrumentList>::value);
		auto sndFile = SndFile();
		if(it.m_index > sndFile->GetNumInstruments())
		{
			if(it.m_index > sndFile->GetModSpecifications().instrumentsMax)
				throw "Cannot add any more instruments";
			sndFile->m_nInstruments = static_cast<INSTRUMENTINDEX>(it.m_index);
		}
		(*this)[it.m_index] = value;
	}
};

}  // namespace Scripting

OPENMPT_NAMESPACE_END

namespace sol
{
using namespace OPENMPT_NAMESPACE::Scripting;

template <>
struct is_container<EnvelopeData> : std::true_type { };

template <>
struct usertype_container<EnvelopeData> : UsertypeContainer<EnvelopeData> { };

template <>
struct is_container<InstrumentList> : std::true_type { };

template <>
struct usertype_container<InstrumentList> : UsertypeContainer<InstrumentList> { };

}  // namespace sol
