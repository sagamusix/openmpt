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

#include "../../soundlib/tuning.h"
#include "../../soundlib/tuningcollection.h"

OPENMPT_NAMESPACE_BEGIN

namespace Scripting
{

struct EnvelopePoint : public Obj
{
	const EnvelopeType m_type;
	const size_t m_pointIndex;
	
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

	// TODO add env type / point index to Name
	std::string Name() const { return MPT_AFORMAT("[openmpt.Song.EnvelopePoint object: Instrument {}]")(m_index); }
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
			/*, "loop_enabled"
			, "loop_start"
			, "loop_end"
			, "sustain_loop_enabled"
			, "sustain_loop_start"
			, "sustain_loop_end"
			, "release_node"*/
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

	// TODO add env type to Name
	std::string Name() const { return MPT_AFORMAT("[openmpt.Song.Envelope object: Instrument {}]")(m_index); }
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
			, "id", sol::readonly(&Instrument::m_index)
			, "name", sol::property(&GetName, &SetName)
			, "filename", sol::property(&GetFilename, &SetFilename)
			, "panning", sol::property(&GetPanning, &SetPanning)
			, "global_volume", sol::property(&GetGlobalVolume, &SetGlobalVolume)
			, "samples", sol::property(&GetSamples)
			, "volume_envelope", sol::property(&GetVolumeEnvelope, &SetVolumeEnvelope)
			, "panning_envelope", sol::property(&GetPanningEnvelope, &SetPanningEnvelope)
			, "pitch_envelope", sol::property(&GetPitchEnvelope, &SetPitchEnvelope)
			, "pitch_tempo_lock", sol::property(&GetPitchTempoLock, &SetPitchTempoLock)
			//, "pitch_pan_separation"
			//, "pitch_pan_separation_center"
			//, "ramp_in"
			//, "resampling"
			//, "volume_random_variation"
			//, "panning_random_variation"
			//, "cutoff_random_variation"
			//, "resonance_random_variation"
			, "filter_cutoff", sol::property(&GetFilterCutoff, &SetFilterCutoff)
			, "filter_resonance", sol::property(&GetFilterResonance, &SetFilterResonance)
			//, "filter_mode"
			//, "new_note_action"
			//, "duplicate_note_check"
			//, "duplicate_note_action"
			//, "plugin"
			//, "midi_channel"
			//, "midi_program"
			//, "midi_bank"
			//, "pitch_bend_range"
			//, "volume_command_handling"
			//, "volume_commands_with_notes_are_velocities"
			, "tuning", sol::property(&GetTuning, &SetTuning)
			, "note_map", sol::property(&GetNoteMap, &SetNoteMap)
			, "play", &PlayNote
			, "stop", &StopNote
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
		{
			return {};
		}
		PlayNoteParam param = PlayNoteParam(static_cast<ModCommand::NOTE>(note + NOTE_MIN)).Instrument(static_cast<INSTRUMENTINDEX>(m_index));
		if(volume)
		{
			param.Volume(mpt::saturate_round<int32>(Clamp(volume.value(), 0.0, 1.0) * 256.0));
		}
		if(panning)
		{
			param.Panning(mpt::saturate_round<int32>(Clamp(panning.value(), -1.0, 1.0) * 128.0 + 128.0));
		}
		CHANNELINDEX chn;
		Script::CallInGUIThread([this, &param, &chn]() {
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
		Script::CallInGUIThread([this, channel]() {
			// Mustn't hold lock
			m_modDoc.NoteOff(NOTE_NONE, false, INSTRUMENTINDEX_INVALID, static_cast<CHANNELINDEX>(channel));
		});
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
