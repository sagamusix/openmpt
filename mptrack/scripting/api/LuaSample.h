/*
 * LuaSample.h
 * -----------
 * Purpose: Lua wrappers for CModDoc and contained classes
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once
#include "LuaObject.h"
#include "LuaGlobals.h"
#include "../Script.h"
#include "../../../tracklib/SampleEdit.h"
#include "../../../tracklib/TimeStretchPitchShift.h"
#include "../../../soundlib/OPL.h"

OPENMPT_NAMESPACE_BEGIN

namespace Scripting
{

#define LoopType(X) \
	X(OFF) \
	X(FORWARD) \
	X(PINGPONG)
	//X(REVERSE)

MPT_MAKE_ENUM(LoopType);
#undef LoopType

#define VibratoType(X) \
	X(SINE) \
	X(SQUARE) \
	X(RAMP_UP) \
	X(RAMP_DOWN) \
	X(RANDOM)

MPT_MAKE_ENUM(VibratoType);
#undef VibratoType

#define OplWaveform(X) \
	X(SINE) \
	X(HALF_SINE) \
	X(ABSOLUTE_SINE) \
	X(PULSE_SINE) \
	X(SINE_EVEN_PERIODS) \
	X(ABSOLUTE_SINE_EVEN_PERIODS) \
	X(SQUARE) \
	X(DERIVED_SQUARE)

MPT_MAKE_ENUM(OplWaveform);
#undef OplWaveform

#define InterpolationType(X) \
	X(NEAREST) \
	X(LINEAR) \
	X(CUBIC) \
	X(SINC) \
	X(R8BRAIN) \
	X(DEFAULT)

MPT_MAKE_ENUM(InterpolationType);
#undef InterpolationType

struct SampleLoop : public Obj
{
	const bool isSustainLoop;

	SampleLoop(const Obj &obj, size_t sample, bool sustainLoop) noexcept
		: Obj{obj, sample}
		, isSustainLoop(sustainLoop) {}

	bool operator==(const SampleLoop &other) const noexcept { return IsEqual(other)  && isSustainLoop == other.isSustainLoop; }

	static void Register(sol::table &table)
	{
		table.new_usertype<SampleLoop>("SampleLoop"
			, sol::meta_function::to_string, &Name
			, sol::meta_function::type, &Type
			, "range", sol::property(&GetRange, &SetRange)
			, "type", sol::property(&GetType, &SetType)
			, "crossfade", &Crossfade
			);
	}

	std::string Name() const { return "[openmpt.Song.SampleLoop object]"; }
	static std::string Type() { return "openmpt.Song.SampleLoop"; }

	ScopedLock<ModSample> GetSample()
	{
		auto sndFile = SndFile();
		if(m_index > 0 && m_index <= sndFile->GetNumSamples())
			return make_lock(sndFile->GetSample(static_cast<SAMPLEINDEX>(m_index)), std::move(sndFile));
		throw "Invalid Sample";
	}
	ScopedLock<const ModSample> GetSample() const
	{
		return const_cast<SampleLoop *>(this)->GetSample();
	}

	SampleLoop &operator=(const SampleLoop &other)
	{
		if(*this == other)
			return *this;
		auto sample = GetSample();
		auto otherSample = other.GetSample();

		const SmpLength loopStart = other.isSustainLoop ? otherSample->nSustainStart : otherSample->nLoopStart;
		const SmpLength loopEnd = other.isSustainLoop ? otherSample->nSustainEnd : otherSample->nLoopEnd;
		const bool enable = otherSample->uFlags[other.isSustainLoop ? CHN_SUSTAINLOOP : CHN_LOOP];
		const bool pingpong = otherSample->uFlags[other.isSustainLoop ? CHN_PINGPONGSUSTAIN : CHN_PINGPONGLOOP];
		if(isSustainLoop)
			sample->SetSustainLoop(loopStart, loopEnd, enable, pingpong, SndFile());
		else
			sample->SetLoop(loopStart, loopEnd, enable, pingpong, SndFile());

		SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Info().Data());
		return *this;
	}

	sol::as_table_t<std::vector<SmpLength>> GetRange() const
	{
		auto sample = GetSample();
		return sol::as_table(std::vector<SmpLength>{
		    isSustainLoop ? sample->nSustainStart : sample->nLoopStart,
		    isSustainLoop ? sample->nSustainEnd : sample->nLoopEnd});
	}

	void SetRange(const sol::table &range)
	{
		auto sample = GetSample();
		if(range.size() != 2)
			throw "Invalid range, expected {start, end}";
		SmpLength start = range[1].get_or(SmpLength(0)), end = range[2].get_or(SmpLength(0));
		if(isSustainLoop)
			sample->SetSustainLoop(start, end, sample->uFlags[CHN_SUSTAINLOOP], sample->uFlags[CHN_PINGPONGSUSTAIN], SndFile());
		else
			sample->SetLoop(start, end, sample->uFlags[CHN_LOOP], sample->uFlags[CHN_PINGPONGLOOP], SndFile());
		sample->PrecomputeLoops(SndFile(), CMainFrame::GetMainFrame()->GetSoundFilePlaying() == SndFile());
		SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Info().Data());
	}

	EnumLoopType::values GetType() const
	{
		auto sample = GetSample();
		auto flags = sample->uFlags;
		if(isSustainLoop)
		{
			if(flags.test_all(CHN_SUSTAINLOOP | CHN_PINGPONGSUSTAIN))
				return EnumLoopType::PINGPONG;
			else if(flags[CHN_SUSTAINLOOP])
				return EnumLoopType::FORWARD;
		} else
		{
			if(flags.test_all(CHN_LOOP | CHN_PINGPONGLOOP))
				return EnumLoopType::PINGPONG;
			else if(flags[CHN_LOOP])
				return EnumLoopType::FORWARD;
		}
		return EnumLoopType::OFF;
	}

	void SetType(EnumLoopType::values type)
	{
		if(type >= EnumLoopType::size)
			throw "Invalid loop type";

		auto sample = GetSample();
		auto &flags = sample->uFlags;
		if(isSustainLoop)
		{
			flags.set(CHN_SUSTAINLOOP, type != EnumLoopType::OFF);
			flags.set(CHN_PINGPONGSUSTAIN, type == EnumLoopType::PINGPONG);
		} else
		{
			flags.set(CHN_LOOP, type != EnumLoopType::OFF);
			flags.set(CHN_PINGPONGLOOP, type == EnumLoopType::PINGPONG);
		}
		sample->PrecomputeLoops(SndFile(), CMainFrame::GetMainFrame()->GetSoundFilePlaying() == SndFile());
		SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Info());
	}

	bool Crossfade(double duration, sol::optional<double> fadeLaw, sol::optional<bool> afterLoopFade)
	{
		auto sample = GetSample();
		auto fadeLength = std::min({mpt::saturate_round<SmpLength>(duration * sample->GetSampleRate(SndFile()->GetType())),
		                            sample->nLength,
		                            isSustainLoop ? sample->nSustainStart : sample->nLoopStart,
		                            (isSustainLoop ? sample->nSustainEnd : sample->nLoopEnd) / 2u});
		if(SampleEdit::XFadeSample(sample, fadeLength, mpt::saturate_round<int>(std::clamp(fadeLaw.value_or(0.5), 0.0, 1.0) * 100000.0), afterLoopFade.value_or(true), isSustainLoop, SndFile()))
		{
			SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Data());
			return true;
		}
		return false;
	}
};


struct OPLOperator : public Obj
{
	const uint8 opOffset = 0;

	OPLOperator(const Obj &obj, size_t sample, bool carrier) noexcept
		: Obj{obj, sample}
		, opOffset{carrier ? uint8(1) : uint8(0)} {}

	bool operator==(const OPLOperator &other) const noexcept { return IsEqual(other) && opOffset == other.opOffset; }

	static void Register(sol::table& table)
	{
		table.new_usertype<OPLOperator>("OPLOperator"
			, sol::meta_function::to_string, &Name
			, sol::meta_function::type, &Type

			, "attack", sol::property(&GetAttack, &SetAttack)
			, "decay", sol::property(&GetDecay, &SetDecay)
			, "sustain", sol::property(&GetSustain, &SetSustain)
			, "release", sol::property(&GetRelease, &SetRelease)
			, "hold_sustain", sol::property(&GetHoldSustain, &SetHoldSustain)
			, "volume", sol::property(&GetVolume, &SetVolume)
			, "scale_envelope_with_keys", sol::property(&GetScale, &SetScale)
			, "key_scale_level", sol::property(&GetKSL, &SetKSL)
			, "frequency_multiplier", sol::property(&GetFreqMultiplier, &SetFreqMultipler)
			, "waveform", sol::property(&GetWaveform, &SetWaveform)
			, "vibrato", sol::property(&GetVibrato, &SetVibrato)
			, "tremolo", sol::property(&GetTremolo, &SetTremolo)
			);
	}

	std::string Name() const { return "[openmpt.Song.OPLOperator object]"; }
	static std::string Type() { return "openmpt.Song.OPLOperator"; }

	ScopedLock<ModSample> GetSample()
	{
		auto sndFile = SndFile();
		if(m_index > 0 && m_index <= sndFile->GetNumSamples())
		{
			auto &sample = sndFile->GetSample(static_cast<SAMPLEINDEX>(m_index));
			if(!sample.uFlags[CHN_ADLIB])
				throw "Not an OPL instrument";
			return make_lock(sample, std::move(sndFile));
		}
		throw "Invalid Sample";
	}
	ScopedLock<const ModSample> GetSample() const
	{
		return const_cast<OPLOperator *>(this)->GetSample();
	}

	void SetModified(ScopedLock<ModSample> &sample)
	{
		auto hint = SampleHint(static_cast<SAMPLEINDEX>(m_index)).Data();
		if(sample->uFlags[SMP_KEEPONDISK] && !sample->uFlags[SMP_MODIFIED])
			hint.Names();
		sample->uFlags.set(SMP_MODIFIED);
		Obj::SetModified(sample, hint);
	}

	OPLOperator &operator=(const OPLOperator &other)
	{
		auto sample = GetSample();
		auto otherSample = other.GetSample();
		const auto oldPatch = sample->adlib;
		for(uint8 op = 0; op <= 8; op += 2)
		{
			sample->adlib[op | opOffset] = otherSample->adlib[op | other.opOffset];
		}

		if(oldPatch != sample->adlib)
			SetModified(sample);
		return *this;
	}

	uint8 GetAttack() const
	{
		return 15 - (GetSample()->adlib[4 | opOffset] >> 4);
	}
	void SetAttack(uint8 attack)
	{
		auto sample = GetSample();
		const uint8 op = 4 | opOffset;
		const uint8 oldValue = sample->adlib[op];
		sample->adlib[op] = static_cast<uint8>(((15 - std::min(attack, uint8(15))) << 4) | (sample->adlib[op] & 0x0F));

		if(oldValue != sample->adlib[op])
			SetModified(sample);
	}

	uint8 GetDecay() const
	{
		return 15 - (GetSample()->adlib[4 | opOffset] & 0x0F);
	}
	void SetDecay(uint8 decay)
	{
		auto sample = GetSample();
		const uint8 op = 4 | opOffset;
		const uint8 oldValue = sample->adlib[op];
		sample->adlib[op] = static_cast<uint8>((15 - std::min(decay, uint8(15))) | (sample->adlib[op] & 0xF0));

		if(oldValue != sample->adlib[op])
			SetModified(sample);
	}

	uint8 GetSustain() const
	{
		return 15 - (GetSample()->adlib[6 | opOffset] >> 4);
	}
	void SetSustain(uint8 sustain)
	{
		auto sample = GetSample();
		const uint8 op = 6 | opOffset;
		const uint8 oldValue = sample->adlib[op];
		sample->adlib[op] = static_cast<uint8>(((15 - std::min(sustain, uint8(15))) << 4) | (sample->adlib[op] & 0x0F));

		if(oldValue != sample->adlib[op])
			SetModified(sample);
	}

	uint8 GetRelease() const
	{
		return 15 - (GetSample()->adlib[6 | opOffset] & 0x0F);
	}
	void SetRelease(uint8 release)
	{
		auto sample = GetSample();
		const uint8 op = 6 | opOffset;
		const uint8 oldValue = sample->adlib[op];
		sample->adlib[op] = static_cast<uint8>((15 - std::min(release, uint8(15))) | (sample->adlib[op] & 0xF0));

		if(oldValue != sample->adlib[op])
			SetModified(sample);
	}

	bool GetHoldSustain() const
	{
		return GetSample()->adlib[opOffset] & OPL::SUSTAIN_ON;
	}
	void SetHoldSustain(bool sustain)
	{
		auto sample = GetSample();
		const uint8 oldValue = sample->adlib[opOffset];
		if(sustain)
			sample->adlib[opOffset] |= OPL::SUSTAIN_ON;
		else
			sample->adlib[opOffset] &= ~OPL::SUSTAIN_ON;

		if(oldValue != sample->adlib[opOffset])
			SetModified(sample);
	}

	uint8 GetVolume() const
	{
		return 63 - (GetSample()->adlib[2 | opOffset] & OPL::TOTAL_LEVEL_MASK);
	}
	void SetVolume(uint8 volume)
	{
		auto sample = GetSample();
		const uint8 op = 2 | opOffset;
		const uint8 oldValue = sample->adlib[op];
		sample->adlib[op] = static_cast<uint8>((sample->adlib[op] & ~OPL::TOTAL_LEVEL_MASK) | (63 - std::min(volume, uint8(63))));

		if(oldValue != sample->adlib[op])
			SetModified(sample);
	}

	bool GetScale() const
	{
		return GetSample()->adlib[opOffset] & OPL::KSR;
	}
	void SetScale(bool scale)
	{
		auto sample = GetSample();
		const uint8 oldValue = sample->adlib[opOffset];
		if(scale)
			sample->adlib[opOffset] |= OPL::KSR;
		else
			sample->adlib[opOffset] &= ~OPL::KSR;

		if(oldValue != sample->adlib[opOffset])
			SetModified(sample);
	}

	uint8 GetKSL() const
	{
		static constexpr uint8 KSLFix[4] = { 0, 2, 1, 3 };
		return KSLFix[GetSample()->adlib[2 | opOffset] >> 6];
	}
	void SetKSL(uint8 ksl)
	{
		static constexpr uint8 KSLFix[4] = { 0x00, 0x80, 0x40, 0xC0 };
		auto sample = GetSample();
		const uint8 op = 2 | opOffset;
		const uint8 oldValue = sample->adlib[op];
		sample->adlib[op] = static_cast<uint8>((sample->adlib[op] & OPL::TOTAL_LEVEL_MASK) | KSLFix[ksl & 3]);

		if(oldValue != sample->adlib[op])
			SetModified(sample);
	}

	uint8 GetFreqMultiplier() const
	{
		return GetSample()->adlib[opOffset] & OPL::MULTIPLE_MASK;
	}
	void SetFreqMultipler(uint8 freqMultiplier)
	{
		auto sample = GetSample();
		const uint8 oldValue = sample->adlib[opOffset];
		sample->adlib[opOffset] = static_cast<uint8>((sample->adlib[opOffset] & ~OPL::MULTIPLE_MASK) | std::min(freqMultiplier, uint8(15)));

		if(oldValue != sample->adlib[opOffset])
			SetModified(sample);
	}

	EnumOplWaveform::values GetWaveform() const
	{
		return static_cast<EnumOplWaveform::values>(GetSample()->adlib[8 | opOffset]);
	}
	void SetWaveform(EnumOplWaveform::values waveform)
	{
		auto sample = GetSample();
		const uint8 op = 8 | opOffset;
		const uint8 oldValue = sample->adlib[op];
		sample->adlib[op] = std::min(static_cast<uint8>(waveform), (SndFile()->GetType() == MOD_TYPE_S3M) ? uint8(3) : uint8(7));

		if(oldValue != sample->adlib[op])
			SetModified(sample);
	}

	bool GetVibrato() const
	{
		return GetSample()->adlib[opOffset] & OPL::VIBRATO_ON;
	}
	void SetVibrato(bool vibrato)
	{
		auto sample = GetSample();
		const uint8 oldValue = sample->adlib[opOffset];
		if(vibrato)
			sample->adlib[opOffset] |= OPL::VIBRATO_ON;
		else
			sample->adlib[opOffset] &= ~OPL::VIBRATO_ON;

		if(oldValue != sample->adlib[opOffset])
			SetModified(sample);
	}

	bool GetTremolo() const
	{
		return GetSample()->adlib[opOffset] & OPL::TREMOLO_ON;
	}
	void SetTremolo(bool tremolo)
	{
		auto sample = GetSample();
		const uint8 oldValue = sample->adlib[opOffset];
		if(tremolo)
			sample->adlib[opOffset] |= OPL::TREMOLO_ON;
		else
			sample->adlib[opOffset] &= ~OPL::TREMOLO_ON;

		if(oldValue != sample->adlib[opOffset])
			SetModified(sample);
	}
};

struct OPLData : public Obj
{
	OPLOperator carrier, modulator;

	OPLData(const Obj &obj, size_t sample) noexcept
		: Obj{obj, sample}
		, carrier{obj, sample, true}
		, modulator{obj, sample, false} {}

	bool operator==(const OPLData &other) const noexcept { return IsEqual(other); }

	static void Register(sol::table &table)
	{
		table.new_usertype<OPLData>("OPLData"
			, sol::meta_function::to_string, &Name
			, sol::meta_function::type, &Type
			, "additive", sol::property(&GetAdditive, &SetAdditive)
			, "feedback", sol::property(&GetFeedback, &SetFeedback)
			, "carrier", &OPLData::carrier
			, "modulator", &OPLData::modulator
			);
	}

	std::string Name() const { return "[openmpt.Song.OPLData object]"; }
	static std::string Type() { return "openmpt.Song.OPLData"; }

	ScopedLock<ModSample> GetSample()
	{
		return carrier.GetSample();
	}
	ScopedLock<const ModSample> GetSample() const
	{
		return carrier.GetSample();
	}

	void SetModified(ScopedLock<ModSample> &sample)
	{
		carrier.SetModified(sample);
	}

	OPLData &operator=(const OPLData &other)
	{
		auto sample = GetSample();
		auto otherSample = other.GetSample();
		if(sample->adlib != otherSample->adlib)
		{
			sample->adlib = otherSample->adlib;
			SetModified(sample);
		}
		return *this;
	}

	bool GetAdditive() const
	{
		return GetSample()->adlib[10] & OPL::CONNECTION_BIT;
	}
	void SetAdditive(bool additive)
	{
		auto sample = GetSample();
		auto oldValue = sample->adlib[10];
		if(additive)
			sample->adlib[10] |= OPL::CONNECTION_BIT;
		else
			sample->adlib[10] &= ~OPL::CONNECTION_BIT;
		
		if(oldValue != sample->adlib[10])
			SetModified(sample);
	}

	uint8 GetFeedback() const
	{
		return (GetSample()->adlib[10] & OPL::FEEDBACK_MASK) >> 1;
	}
	void SetFeedback(uint8 feedback)
	{
		auto sample = GetSample();
		auto oldValue = sample->adlib[10];
		sample->adlib[10] = static_cast<uint8>((sample->adlib[10] & ~OPL::FEEDBACK_MASK) | (std::min(feedback, uint8(7)) << 1));
		
		if(oldValue != sample->adlib[10])
			SetModified(sample);
	}
};


struct SampleCuePoints final : public ProxyContainer<SmpLength>, public Obj
{
	static void Register(sol::table &table)
	{
		table.new_usertype<SampleCuePoints>("SampleCuePoints"
			, sol::meta_function::to_string, &Name
			, sol::meta_function::type, &Type
			, sol::meta_function::length, &Length
			, sol::meta_function::index, &Index
			, sol::meta_function::new_index, &NewIndex
		);
	}

	SampleCuePoints(const Obj &obj, size_t sample) noexcept
		: Obj{obj, sample}
	{ }

	bool operator==(const SampleCuePoints &other) const noexcept { return IsEqual(other); }

	std::string Name() const { return "[openmpt.Song.SampleCuePoints object]"; }
	static std::string Type() { return "openmpt.Song.SampleCuePoints"; }

	ScopedLock<ModSample> GetSample()
	{
		auto sndFile = SndFile();
		if(m_index > 0 && m_index <= sndFile->GetNumSamples())
			return make_lock(sndFile->GetSample(static_cast<SAMPLEINDEX>(m_index)), std::move(sndFile));
		throw "Invalid Sample";
	}
	ScopedLock<const ModSample> GetSample() const
	{
		return const_cast<SampleCuePoints *>(this)->GetSample();
	}

	size_t Length() const
	{
		return GetSample()->cues.size();
	}

	SmpLength Index(size_t position)
	{
		auto sample = GetSample();
		if(position < 1 || position > sample->cues.size())
			throw "Invalid cue point!";
		return sample->cues[position - 1u];
	}

	void NewIndex(size_t position, SmpLength value)
	{
		auto sample = GetSample();
		if(position < 1 || position > sample->cues.size())
			throw "Invalid cue point!";
		sample->cues[position - 1u] = value;
		SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Data());
	}

	size_t size() override { return Length(); }
	iterator begin() override { return iterator{*this, 0}; }
	iterator end() override { return iterator{*this, Length()}; }
	value_type operator[](size_t index) override
	{
		return GetSample()->cues[index];
	}
};


struct Sample : public Obj
{
	SampleLoop normalLoop, sustainLoop;
	OPLData oplData;
	SampleCuePoints cuePoints;

	static void Register(sol::table &table)
	{
		table.new_usertype<Sample>("Sample"
			, sol::meta_function::to_string, &Name
			, sol::meta_function::type, &Type
			//, "save"
			//, "load"
			, "id", sol::readonly(&Sample::m_index)
			, "name", sol::property(&GetName, &SetName)
			, "filename", sol::property(&GetFilename, &SetFilename)
			, "path", sol::property(&GetPath)
			, "external", sol::property(&IsExternal)
			, "modified", sol::property(&IsModified)
			, "opl", sol::property(&GetOPL, &SetOPL)
			, "opl_data", &Sample::oplData
			, "length", sol::property(&GetLength, &SetLength)
			, "bits_per_sample", sol::property(&GetBitDepth, &SetBitDepth)
			, "channels", sol::property(&GetChannels, &SetChannels)
			, "loop", &Sample::normalLoop
			, "sustain_loop", &Sample::sustainLoop
			, "frequency", sol::property(&GetFrequency, &SetFrequency)
			, "panning", sol::property(&GetPanning, &SetPanning)
			, "volume", sol::property(&GetVolume, &SetVolume)
			, "global_volume", sol::property(&GetGlobalVolume, &SetGlobalVolume)
			, "cue_points", sol::property(&GetCuePoints, &SetCuePoints)
			, "vibrato_type", sol::property(&GetVibratoType, &SetVibratoType)
			, "vibrato_depth", sol::property(&GetVibratoDepth, &SetVibratoDepth)
			, "vibrato_sweep", sol::property(&GetVibratoSweep, &SetVibratoSweep)
			, "vibrato_rate", sol::property(&GetVibratoRate, &SetVibratoRate)

			, "play", &PlayNote
			, "stop", &StopNote
			, "normalize", &Normalize
			, "remove_dc_offset", &RemoveDCOffset
			, "amplify", &Amplify
			, "change_stereo_separation", &ChangeStereoSeparation
			, "resample", &Resample
			, "reverse", &Reverse
			, "silence", &Silence
			, "invert", &Invert
			, "unsign", &Unsign
			, "stretch_hq", &StretchHQ
			, "stretch_lofi", &StretchLofi
			//, "autotune"
			, "get_data", &GetData
			, "set_data", &SetData
			);
	}

	Sample(const Obj &obj, size_t sample) noexcept
		: Obj{obj, sample}
		, normalLoop{obj, sample, false}
		, sustainLoop{obj, sample, true}
		, oplData{obj, sample}
		, cuePoints{obj, sample}
	{
	}

	bool operator==(const Sample &other) const noexcept { return IsEqual(other); }

	std::string Name() const { return MPT_AFORMAT("[openmpt.Song.Sample object: Sample {}]")(m_index); }
	static std::string Type() { return "openmpt.Song.Sample"; }

	ScopedLock<ModSample> GetSample()
	{
		auto sndFile = SndFile();
		if(m_index > 0 && m_index <= sndFile->GetNumSamples())
			return make_lock(sndFile->GetSample(static_cast<SAMPLEINDEX>(m_index)), std::move(sndFile));
		throw "Invalid Sample";
	}
	ScopedLock<const ModSample> GetSample() const
	{
		return const_cast<Sample *>(this)->GetSample();
	}

	using Obj::SetModified;
	void SetModified(ScopedLock<ModSample> &sample, SampleHint hint, bool waveformModified)
	{
		if(waveformModified)
		{
			if(sample->uFlags[SMP_KEEPONDISK] && !sample->uFlags[SMP_MODIFIED])
				hint.Names();
			sample->uFlags.set(SMP_MODIFIED);
		}
		Obj::SetModified(sample, hint);
	}

	Sample &operator=(const Sample &other)
	{
		if(*this == other)
			return *this;
		auto sample = GetSample();
		auto otherSample = other.GetSample();

		sample->FreeSample();
		*sample = *otherSample;
		sample->CopyWaveform(otherSample);
		sample->uFlags.reset(SMP_KEEPONDISK);

		auto sndFile = SndFile();
		sndFile->m_szNames[m_index] = other.SndFile()->m_szNames[other.m_index];
		sndFile->ResetSamplePath(static_cast<SAMPLEINDEX>(m_index));

		SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Info().Data().Names(), true);
		return *this;
	}

	std::string GetName() const
	{
		auto sndFile = SndFile();
		return FromSndFileString(sndFile->m_szNames[m_index], sndFile);
	}

	void SetName(const std::string &name)
	{
		auto sndFile = SndFile();
		sndFile->m_szNames[m_index] = ToSndFileString(name.substr(0, sndFile->GetModSpecifications().sampleNameLengthMax), sndFile);
		SetModified(sndFile, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Names());
	}

	std::string GetFilename() const
	{
		return FromSndFileString(GetSample()->filename);
	}
	void SetFilename(const std::string &filename)
	{
		auto sndFile = SndFile();
		GetSample()->filename = ToSndFileString(filename.substr(0, sndFile->GetModSpecifications().sampleFilenameLengthMax), sndFile);
		SetModified(sndFile, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Info());
	}

	std::string GetPath() const
	{
		return SndFile()->GetSamplePath(static_cast<SAMPLEINDEX>(m_index)).ToUTF8();
	}

	bool IsExternal() const
	{
		return GetSample()->uFlags[SMP_KEEPONDISK];
	}

	bool IsModified() const
	{
		return GetSample()->uFlags[SMP_MODIFIED];
	}

	bool GetOPL() const
	{
		return GetSample()->uFlags[CHN_ADLIB];
	}
	void SetOPL(bool enableOPL)
	{
		auto sndFile = SndFile();
		auto sample = GetSample();
		if(enableOPL == sample->uFlags[CHN_ADLIB])
			return;
		if(enableOPL && !sndFile->SupportsOPL())
			throw "OPL instruments are not supported by this format";
		
		sample->SetAdlib(enableOPL);
		SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Info().Data().Names(), true);
	}

	SmpLength GetLength() const
	{
		auto sample = GetSample();
		ThrowIfOPL(sample);
		return sample->nLength;
	}
	void SetLength(SmpLength length)
	{
		auto sndFile = SndFile();
		auto sample = GetSample();
		SampleEdit::ResizeSample(sample, length, sndFile);
		SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Info().Data(), true);
	}

	int GetBitDepth() const
	{
		auto sample = GetSample();
		ThrowIfOPL(sample);
		return sample->GetElementarySampleSize() * 8;
	}
	void SetBitDepth(int bits)
	{
		auto sndFile = SndFile();
		auto sample = GetSample();
		ThrowIfOPL(sample);
		switch(bits)
		{
		case 8: SampleEdit::ConvertTo8Bit(sample, sndFile); break;
		case 16: SampleEdit::ConvertTo16Bit(sample, sndFile); break;
		default: throw "Invalid bit depth (must be 8 or 16)";
		}
		SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Info().Data(), true);
	}

	int GetChannels() const
	{
		auto sample = GetSample();
		ThrowIfOPL(sample);
		return sample->GetNumChannels();
	}
	void SetChannels(int channels)
	{
		auto sndFile = SndFile();
		auto sample = GetSample();
		ThrowIfOPL(sample);
		switch(channels)
		{
		case 1: ctrlSmp::ConvertToMono(sample, sndFile, ctrlSmp::mixChannels); break;
		case 2: ctrlSmp::ConvertToStereo(sample, sndFile); break;
		default: throw "Invalid channel count (must be 1 or 2)";
		}
		SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Info().Data(), true);
	}

	// TODO return as pair(transpose,finetune) for MOD/XM?
	uint32 GetFrequency() const
	{
		auto sndFile = SndFile();
		return GetSample()->GetSampleRate(sndFile->GetType());
	}
	void SetFrequency(uint32 frequency)
	{
		auto sample = GetSample();
		sample->nC5Speed = frequency;
		if(SndFile()->UseFinetuneAndTranspose())
			sample->FrequencyToTranspose();
		SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Info());
	}

	sol::optional<double> GetPanning() const
	{
		auto sample = GetSample();
		if(sample->uFlags[CHN_PANNING])
			return GetSample()->nPan / 128.0 - 1.0;
		else
			return {};
	}
	void SetPanning(sol::optional<double> panning)
	{
		auto sample = GetSample();
		if(!panning && SndFile()->GetType() == MOD_TYPE_XM)
			throw "Sample panning cannot be disabled in XM";
		if(panning && SndFile()->GetType() & (MOD_TYPE_MOD | MOD_TYPE_S3M))
			throw "Sample panning is not available in this format";
		sample->uFlags.set(CHN_PANNING, !!panning);
		if(panning)
			sample->nPan = mpt::saturate_round<uint16>(std::clamp(*panning, -1.0, 1.0) * 128.0 + 128.0);
		SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Info());
	}

	double GetVolume() const
	{
		return GetSample()->nVolume / 256.0;
	}
	void SetVolume(double volume)
	{
		auto sample = GetSample();
		sample->nVolume = mpt::saturate_round<uint16>(std::clamp(volume, 0.0, 1.0) * 256.0);
		SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Info());
	}

	double GetGlobalVolume() const
	{
		return GetSample()->nGlobalVol / 64.0;
	}
	void SetGlobalVolume(double volume)
	{
		auto sample = GetSample();
		sample->nGlobalVol = mpt::saturate_round<uint16>(std::clamp(volume, 0.0, 1.0) * 64.0);
		SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Info());
	}

	SampleCuePoints GetCuePoints() const
	{
		auto sample = GetSample();
		ThrowIfOPL(sample);
		return cuePoints;
	}
	void SetCuePoints(const sol::table &cues)
	{
		auto sample = GetSample();
		ThrowIfOPL(sample);
		if(cues.is<SampleCuePoints>() && cuePoints.IsEqual(cues.as<SampleCuePoints>()))
			return;

		if(cues.size() > sample->cues.size())
			throw MPT_AFORMAT("Too many cue points (only {} allowed)")(sample->cues.size());
		sample->RemoveAllCuePoints();
		for(size_t i = 0; i < cues.size(); i++)
		{
			sample->cues[i] = cues[i + 1].get_or(SmpLength(0));
		}
		SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Data());
	}

	EnumVibratoType::values GetVibratoType() const
	{
		auto sample = GetSample();
		switch(sample->nVibType)
		{
		default:
		case VIB_SINE: return EnumVibratoType::SINE;
		case VIB_SQUARE: return EnumVibratoType::SQUARE;
		case VIB_RAMP_UP: return EnumVibratoType::RAMP_UP;
		case VIB_RAMP_DOWN: return EnumVibratoType::RAMP_DOWN;
		case VIB_RANDOM: return EnumVibratoType::RANDOM;
		}
	}
	void SetVibratoType(EnumVibratoType::values type)
	{
		auto sample = GetSample();
		switch(type)
		{
		case EnumVibratoType::SINE: sample->nVibType = VIB_SINE; break;
		case EnumVibratoType::SQUARE: sample->nVibType = VIB_SQUARE; break;
		case EnumVibratoType::RAMP_UP: sample->nVibType = VIB_RAMP_UP; break;
		case EnumVibratoType::RAMP_DOWN: sample->nVibType = VIB_RAMP_DOWN; break;
		case EnumVibratoType::RANDOM: sample->nVibType = VIB_RANDOM; break;
		default: throw "Invalid vibrato type (must be one of openmpt.Song.vibratoType)";
		}
		PropagateAutoVibratoChanges();
	}
	
	int GetVibratoDepth() const
	{
		return GetSample()->nVibDepth;
	}
	void SetVibratoDepth(int depth)
	{
		auto sample = GetSample();
		LimitMax(depth, (SndFile()->GetType() == MOD_TYPE_XM) ? 15 : 32);
		sample->nVibDepth = mpt::saturate_cast<uint8>(depth);
		PropagateAutoVibratoChanges();
	}

	int GetVibratoSweep() const
	{
		return GetSample()->nVibSweep;
	}
	void SetVibratoSweep(int sweep)
	{
		auto sample = GetSample();
		sample->nVibSweep = mpt::saturate_cast<uint8>(sweep);
		PropagateAutoVibratoChanges();
	}
	
	int GetVibratoRate() const
	{
		return GetSample()->nVibRate;
	}
	void SetVibratoRate(int rate)
	{
		auto sample = GetSample();
		LimitMax(rate, (SndFile()->GetType() == MOD_TYPE_XM) ? 63 : 64);
		sample->nVibRate = mpt::saturate_cast<uint8>(rate);
		PropagateAutoVibratoChanges();
	}

	void PropagateAutoVibratoChanges()
	{
		auto sample = GetSample();
		SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Info());

		auto sndFile = SndFile();
		if(!(sndFile->GetType() & MOD_TYPE_XM))
			return;

		auto modDoc = ModDoc();

		for(INSTRUMENTINDEX i = 1; i <= sndFile->GetNumInstruments(); i++)
		{
			if(sndFile->IsSampleReferencedByInstrument(static_cast<SAMPLEINDEX>(m_index), i))
			{
				const auto referencedSamples = sndFile->Instruments[i]->GetSamples();

				// Propagate changes to all samples that belong to this instrument.
				sndFile->PropagateXMAutoVibrato(i, sample->nVibType, sample->nVibSweep, sample->nVibDepth, sample->nVibRate);
				for(auto smp : referencedSamples)
				{
					modDoc->UpdateAllViews(nullptr, SampleHint(smp).Info(), nullptr);
				}
			}
		}
	}

	sol::optional<CHANNELINDEX> PlayNote(int note, sol::optional<double> volume, sol::optional<double> panning)
	{
		if(note < 0 || note > NOTE_MAX - NOTE_MIN)
			return {};

		PlayNoteParam param = PlayNoteParam(static_cast<ModCommand::NOTE>(note + NOTE_MIN)).Sample(static_cast<SAMPLEINDEX>(m_index));
		if(volume)
			param.Volume(mpt::saturate_round<int32>(std::clamp(*volume, 0.0, 1.0) * 256.0));

		if(panning)
			param.Panning(mpt::saturate_round<int32>(std::clamp(*panning, -1.0, 1.0) * 128.0 + 128.0));

		CHANNELINDEX chn;
		Script::CallInGUIThread([this, &param, &chn]() {
			// Mustn't hold lock
			chn = m_modDoc.PlayNote(param);
		});

		if(chn == CHANNELINDEX_INVALID)
			return {};

		return chn;
	}

	void StopNote(int channel)
	{
		Script::CallInGUIThread([this, channel]() {
			// Mustn't hold lock
			m_modDoc.NoteOff(NOTE_NONE, false, INSTRUMENTINDEX_INVALID, static_cast<CHANNELINDEX>(channel));
		});
	}

	bool Normalize(sol::optional<SmpLength> start, sol::optional<SmpLength> end)
	{
		auto sample = GetSample();
		ThrowIfOPL(sample);
		if(SampleEdit::NormalizeSample(sample, start.value_or(0u), end.value_or(sample->nLength), SndFile()))
		{
			SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Data(), true);
			return true;
		}
		return false;
	}

	double RemoveDCOffset(sol::optional<SmpLength> start, sol::optional<SmpLength> end)
	{
		auto sample = GetSample();
		ThrowIfOPL(sample);
		const double offset = SampleEdit::RemoveDCOffset(sample, start.value_or(0u), end.value_or(sample->nLength), SndFile());
		if(offset != 0.0)
			SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Data(), true);
		return offset;
	}

	bool Amplify(double amplifyStart, sol::optional<double> amplifyEnd, sol::optional<SmpLength> start, sol::optional<SmpLength> end, sol::optional<EnumFadelaw::values> fadeLawAPI)
	{
		Fade::Law fadeLaw = Fade::kLinear;
		if(fadeLawAPI)
		{
			if(*fadeLawAPI >= EnumFadelaw::size)
				throw "Invalid fade law (must be one of openmpt.fadeLaw)";

			switch(*fadeLawAPI)
			{
			case EnumFadelaw::LINEAR: fadeLaw = Fade::kLinear; break;
			case EnumFadelaw::EXPONENTIAL: fadeLaw = Fade::kPow; break;
			case EnumFadelaw::SQUARE_ROOT: fadeLaw = Fade::kSqrt; break;
			case EnumFadelaw::LOGARITHMIC: fadeLaw = Fade::kLog; break;
			case EnumFadelaw::QUARTER_SINE: fadeLaw = Fade::kQuarterSine; break;
			case EnumFadelaw::HALF_SINE: fadeLaw = Fade::kHalfSine; break;
			}
		}

		auto sample = GetSample();
		ThrowIfOPL(sample);
		if(SampleEdit::AmplifySample(sample, start.value_or(0u), end.value_or(sample->nLength), amplifyStart, amplifyEnd.value_or(amplifyStart), true, fadeLaw, SndFile()))
		{
			SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Data(), true);
			return true;
		}
		return false;
	}

	bool ChangeStereoSeparation(double factor, sol::optional<SmpLength> start, sol::optional<SmpLength> end)
	{
		auto sample = GetSample();
		ThrowIfOPL(sample);
		if(SampleEdit::StereoSepSample(sample, start.value_or(0u), end.value_or(sample->nLength), factor, SndFile()))
		{
			SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Data(), true);
			return true;
		}
		return false;
	}

	bool Resample(int newRate, EnumInterpolationType::values interpolationType, sol::optional<bool> updateOffsets, sol::optional<bool> updateNotes, sol::optional<SmpLength> start, sol::optional<SmpLength> end)
	{
		auto sndFile = SndFile();
		auto sample = GetSample();
		ThrowIfOPL(sample);

		bool anyPatternUpdates = false;
		auto prepareSampleUndoFunc = [&]()
		{
			//ModDoc()->GetSampleUndo().PrepareUndo(static_cast<SAMPLEINDEX>(m_index), sundo_replace, "Resample");
		};
		auto preparePatternUndoFunc = [&]()
		{
			//ModDoc()->PrepareUndoForAllPatterns(false, "Resample (Adjust Offsets)");
			anyPatternUpdates = true;
		};

		
		ResamplingMode mode = SRCMODE_DEFAULT;
		switch(interpolationType)
		{
		case EnumInterpolationType::DEFAULT:
		case EnumInterpolationType::R8BRAIN:
			break;
		case EnumInterpolationType::NEAREST: mode = SRCMODE_NEAREST; break;
		case EnumInterpolationType::LINEAR: mode = SRCMODE_LINEAR; break;
		case EnumInterpolationType::CUBIC: mode = SRCMODE_CUBIC; break;
		case EnumInterpolationType::SINC: mode = SRCMODE_SINC8LP; break;
		default: throw "Unknown interpolation type";
		}

		if(!SampleEdit::Resample(sample, start.value_or(0u), end.value_or(sample->nLength), mpt::saturate_cast<uint32>(newRate), mode, sndFile, updateOffsets.value_or(false), updateNotes.value_or(false), prepareSampleUndoFunc, preparePatternUndoFunc))
			return false;

		SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Data().Info(), true);
		if(anyPatternUpdates)
			ModDoc()->UpdateAllViews(PatternHint().Data());
		return true;
	}

	bool Reverse(sol::optional<SmpLength> start, sol::optional<SmpLength> end)
	{
		auto sample = GetSample();
		ThrowIfOPL(sample);
		if(SampleEdit::ReverseSample(sample, start.value_or(0u), end.value_or(sample->nLength), SndFile()))
		{
			SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Data(), true);
			return true;
		}
		return false;
	}

	bool Silence(sol::optional<SmpLength> start, sol::optional<SmpLength> end)
	{
		auto sample = GetSample();
		ThrowIfOPL(sample);
		if(SampleEdit::SilenceSample(sample, start.value_or(0u), end.value_or(sample->nLength), SndFile()))
		{
			SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Data(), true);
			return true;
		}
		return false;
	}

	bool Invert(sol::optional<SmpLength> start, sol::optional<SmpLength> end)
	{
		auto sample = GetSample();
		ThrowIfOPL(sample);
		if(SampleEdit::InvertSample(sample, start.value_or(0u), end.value_or(sample->nLength), SndFile()))
		{
			SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Data(), true);
			return true;
		}
		return false;
	}

	bool Unsign(sol::optional<SmpLength> start, sol::optional<SmpLength> end)
	{
		auto sample = GetSample();
		ThrowIfOPL(sample);
		if(SampleEdit::UnsignSample(sample, start.value_or(0u), end.value_or(sample->nLength), SndFile()))
		{
			SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Data(), true);
			return true;
		}
		return false;
	}

	bool StretchHQ(float stretchFactor, sol::optional<float> pitchFactor, sol::optional<SmpLength> start, sol::optional<SmpLength> end)
	{
		auto sample = GetSample();
		const auto updateFunc = [](SmpLength, SmpLength) { return false; };
		const auto prepareSampleUndoFunc = [] { /* TODO */};
		TimeStretchPitchShift::Signalsmith instance(updateFunc, prepareSampleUndoFunc, SndFile(), static_cast<SAMPLEINDEX>(m_index), pitchFactor.value_or(0.0f), stretchFactor, start.value_or(0), end.value_or(sample->nLength));
		return instance.Process() == TimeStretchPitchShift::Result::OK;
	}

	bool StretchLofi(float stretchFactor, sol::optional<float> pitchFactor, sol::optional<int> grainSize, sol::optional<SmpLength> start, sol::optional<SmpLength> end)
	{
		auto sample = GetSample();
		const auto updateFunc = [](SmpLength, SmpLength) { return false; };
		const auto prepareSampleUndoFunc = [] { /* TODO */ };
		TimeStretchPitchShift::LoFi instance(updateFunc, prepareSampleUndoFunc, SndFile(), static_cast<SAMPLEINDEX>(m_index), pitchFactor.value_or(0.0f), stretchFactor, start.value_or(0), end.value_or(sample->nLength), grainSize.value_or(1024));
		return instance.Process() == TimeStretchPitchShift::Result::OK;
	}

	sol::as_table_t<std::vector<double>> GetData() const
	{
		auto sample = GetSample();
		ThrowIfOPL(sample);
		const SmpLength sampleLength = sample->nLength * sample->GetNumChannels();
		std::vector<double> data(sampleLength);
		size_t i = 0;
		switch(sample->GetElementarySampleSize())
		{
			case 1:
				for(const auto v : mpt::as_span(sample->sample8(), sampleLength))
				{
					data[i++] = v * (1.0 / 128.0);
				}
				break;
			case 2:
				for(const auto v : mpt::as_span(sample->sample16(), sampleLength))
				{
					data[i++] = v * (1.0 / 32768.0);
				}
				break;
		}
		return sol::as_table(std::move(data));
	}

	//void SetData(const sol::variadic_args &va)
	void SetData(const sol::table &data)
	{
		auto sndFile = SndFile();
		auto sample = GetSample();
		ThrowIfOPL(sample);
#if 1
		if(data.size() % sample->GetNumChannels())
			throw "Table size is not compatible with number of channels";

		if(data.empty())
		{
			sample->FreeSample();
		} else
		{
			sample->nLength = mpt::saturate_cast<SmpLength>(data.size() / sample->GetNumChannels());
			ctrlChn::ReplaceSample(sndFile, sample, nullptr, 0, FlagSet<ChannelFlags>(), FlagSet<ChannelFlags>());
			if(!sample->AllocateSample())
				throw "Unable to allocate sample!";

			size_t i = 1;
			switch(sample->GetElementarySampleSize())
			{
				case 1:
					for(auto &v : mpt::as_span(sample->sample8(), data.size()))
					{
						v = mpt::saturate_round<int8>(data[i++].get<double>() * 128.0);
					}
					break;
				case 2:
					for(auto &v : mpt::as_span(sample->sample16(), data.size()))
					{
						v = mpt::saturate_round<int16>(data[i++].get<double>() * 32768.0);
					}
					break;
			}
			sample->PrecomputeLoops(sndFile, CMainFrame::GetMainFrame()->GetSoundFilePlaying() == SndFile());
		}
#else
		size_t numChannels = va.size();
		if(numChannels == 0)
		{
			sample->FreeSample();
		} else
		{
			if(numChannels > 2)
			{
				throw "Unsupported channel count!";
			}
			size_t firstSize = va.begin()->get<sol::table>().size();
			if(firstSize > MAX_SAMPLE_LENGTH)
				throw "Sample data too long!";
			for(sol::table channel : va)
			{
				if(channel.size() != firstSize)
					throw "Channels have different lengths!";
			}
			sample->uFlags.set(CHN_SAMPLEFLAGS, numChannels == 2);
			sample->nLength = static_cast<SmpLength>(firstSize);
			ctrlChn::ReplaceSample(sndFile, sample, nullptr, 0, FlagSet<ChannelFlags>(), FlagSet<ChannelFlags>());
			if(!sample->AllocateSample())
				throw "Unable to allocate sample!";

			for(size_t chn = 0; chn < numChannels; chn++)
			{
				try
				{
					sol::table data = va[chn];
					switch(sample->GetElementarySampleSize())
					{
						case 1:
						{
							int8 *dst = sample->sample8() + chn;
							for(const auto &v : data)
							{
								*dst = mpt::saturate_round<int8>(v.second.as<double>() * 128.0);
								dst += numChannels;
							}
							break;
						}
						case 2:
						{
							int16 *dst = sample->sample16() + chn;
							for(const auto &v : data)
							{
								*dst = mpt::saturate_round<int16>(v.second.as<double>() * 32768.0);
								dst += numChannels;
							}
							break;
						}
					}
				} catch(const std::exception &)
				{
					throw;
				}
			}
			sample->PrecomputeLoops(sndFile, CMainFrame::GetMainFrame()->GetSoundFilePlaying() == SndFile());
		}
#endif
		SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Info().Data(), true);
	}

	void ThrowIfOPL(const ModSample &sample) const
	{
		if(sample.uFlags[CHN_ADLIB])
			throw "Cannot call this operation on OPL instruments";
	}
};


struct SampleList final : public ProxyContainer<Sample>, public Obj
{
	SampleList(const Obj &obj) noexcept
		: Obj{ obj } {}

	SampleList& operator=(const SampleList &other)
	{
		auto sndFile = SndFile();
		auto otherSndFile = other.SndFile();
		if (sndFile == otherSndFile)
			return *this;

		for (SAMPLEINDEX smp = 1; smp <= sndFile->GetNumSamples(); smp++)
		{
			sndFile->DestroySample(smp);
		}
		sndFile->m_nSamples = otherSndFile->GetNumSamples();
		for (SAMPLEINDEX smp = 1; smp <= sndFile->GetNumSamples(); smp++)
		{
			sndFile->ReadSampleFromSong(smp, otherSndFile, smp);
		}

		SetModified(sndFile, SampleHint().Info().Data().Names());
		return *this;
	}

	size_t size() override { return SndFile()->GetNumSamples(); }
	iterator begin() override { return iterator{*this, 1}; }
	iterator end() override { return iterator{*this, SndFile()->GetNumSamples() + 1u}; }
	value_type operator[](size_t index) override { return Sample{ *this, static_cast<SAMPLEINDEX>(index) }; }
	size_t max_size() const noexcept { return MAX_SAMPLES; }

	void push_back(const Sample &value)
	{
		static_assert(sol::meta::has_push_back<SampleList>::value);
		auto sndFile = SndFile();
		if(!sndFile->CanAddMoreSamples())
			throw "Cannot add any more samples";
		sndFile->m_nSamples++;
		(*this)[sndFile->GetNumSamples()] = value;
	}
	void erase(iterator it)
	{
		static_assert(sol::container_detail::has_erase<SampleList>::value);
		auto sndFile = SndFile();
		sndFile->DestroySample(static_cast<SAMPLEINDEX>(it.m_index));
		sndFile->m_szNames[static_cast<SAMPLEINDEX>(it.m_index)] = "";
		if(it.m_index == sndFile->GetNumSamples() && sndFile->GetNumSamples() > 1)
		{
			sndFile->m_nSamples--;
		}
		SetModified(sndFile, SampleHint(static_cast<SAMPLEINDEX>(it.m_index)).Info().Data().Names());
	}
	void insert(iterator it, const Sample &value)
	{
		static_assert(sol::meta::has_insert_with_iterator<SampleList>::value);
		auto sndFile = SndFile();
		if(it.m_index > sndFile->GetNumSamples())
		{
			if(it.m_index > sndFile->GetModSpecifications().samplesMax)
				throw "Cannot add any more samples";
			sndFile->m_nSamples = static_cast<SAMPLEINDEX>(it.m_index);
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
struct is_container<SampleList> : std::true_type { };

template <>
struct usertype_container<SampleList> : UsertypeContainer<SampleList> { };

}  // namespace sol
