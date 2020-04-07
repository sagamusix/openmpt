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
#include "../../../soundbase/SampleFormatCopy.h"
#include "../../../tracklib/SampleEdit.h"

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

struct SampleLoop : public Obj
{
	bool isSustainLoop;

	SampleLoop(const Obj &obj, size_t sample, bool sustainLoop) noexcept
	    : Obj(obj, sample)
		, isSustainLoop(sustainLoop) {}

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

	std::vector<SmpLength> GetRange() const
	{
		auto sample = GetSample();
		return {
		    isSustainLoop ? sample->nSustainStart : sample->nLoopStart,
		    isSustainLoop ? sample->nSustainEnd : sample->nLoopEnd};
	}

	void SetRange(const sol::table &range)
	{
		auto sample = GetSample();
		if(range.size() != 2)
			throw "Invalid range, expected {start, end}";
		SmpLength start = range[1], end = range[2];
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


struct Sample : public Obj
{
	SampleLoop normalLoop, sustainLoop;

	static void Register(sol::table &table)
	{
		table.new_usertype<Sample>("Sample"
			, sol::meta_function::to_string, &Name, sol::meta_function::type, &Type
			//, "save"
			//, "load"
			//, "external"
			//, "modified" for external samples
			//, "path"
			, "id", sol::readonly(&Sample::m_index)
			, "name", sol::property(&GetName, &SetName)
			, "filename", sol::property(&GetFilename, &SetFilename)
			, "length", sol::property(&GetLength, &SetLength)
			, "bits_per_sample", sol::property(&GetBitDepth, &SetBitDepth)
			, "channels", sol::property(&GetChannels, &SetChannels)
			, "loop", &Sample::normalLoop
			, "sustain_loop", &Sample::sustainLoop
			, "frequency", sol::property(&GetFrequency, &SetFrequency)
			, "panning", sol::property(&GetPanning, &SetPanning)
			, "volume", sol::property(&GetVolume, &SetVolume)
			, "global_volume", sol::property(&GetGlobalVolume, &SetGlobalVolume)
			, "play", &PlayNote
			, "stop", &StopNote
			, "normalize", &Normalize
			, "amplify", &Amplify
			//, "remove_dc_offset"
			, "change_stereo_separation", &ChangeStereoSeparation
			, "resample", &Resample
			, "reverse", &Reverse
			, "silence", &Silence
			, "invert", &Invert
			, "unsign", &Unsign
			//, "autotune"
			, "set_data", &SetData
			);
	}

	Sample(const Obj &obj, size_t sample) noexcept
	    : Obj(obj, sample)
	    , normalLoop(obj, sample, false)
	    , sustainLoop(obj, sample, true)
	{
	}

	std::string Name() const { return MPT_FORMAT("[openmpt.Song.Sample object: Sample {}]")(m_index); }
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

	SmpLength GetLength() const { return GetSample()->nLength; }
	void SetLength(SmpLength length)
	{
		auto sndFile = SndFile();
		SampleEdit::ResizeSample(GetSample(), length, sndFile);
		SetModified(sndFile, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Info().Data());
	}

	int GetBitDepth() const { return GetSample()->GetElementarySampleSize() * 8; }
	void SetBitDepth(int bits)
	{
		auto sndFile = SndFile();
		switch(bits)
		{
		case 8:
			SampleEdit::ConvertTo8Bit(GetSample(), sndFile);
			break;
		case 16:
			SampleEdit::ConvertTo16Bit(GetSample(), sndFile);
			break;
		default:
			throw "Invalid bit depth.";
		}
		SetModified(sndFile, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Info().Data());
	}

	int GetChannels() const { return GetSample()->GetNumChannels(); }
	void SetChannels(int channels)
	{
		auto sndFile = SndFile();
		switch(channels)
		{
		case 1:
			ctrlSmp::ConvertToMono(GetSample(), sndFile, ctrlSmp::mixChannels);
			break;
		case 2:
			ctrlSmp::ConvertToStereo(GetSample(), sndFile);
			break;
		default:
			throw "Invalid channel count.";
		}
		SetModified(sndFile, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Info().Data());
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
			sample->nPan = static_cast<uint16>(Clamp(panning.value(), -1.0, 1.0) * 128.0 + 128.0);
		SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Info());
	}

	double GetVolume() const
	{
		return GetSample()->nVolume / 256.0;
	}
	void SetVolume(double volume)
	{
		auto sample = GetSample();
		sample->nVolume = static_cast<uint16>(Clamp(volume, 0.0, 1.0) * 256.0);
		SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Info());
	}

	double GetGlobalVolume() const
	{
		return GetSample()->nGlobalVol / 64.0;
	}
	void SetGlobalVolume(double volume)
	{
		auto sample = GetSample();
		sample->nGlobalVol = static_cast<uint16>(Clamp(volume, 0.0, 1.0) * 64.0);
		SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Info());
	}

	sol::optional<CHANNELINDEX> PlayNote(int note, sol::optional<double> volume, sol::optional<double> panning)
	{
		if(note < 0 || note > NOTE_MAX - NOTE_MIN)
		{
			return {};
		}
		PlayNoteParam param = PlayNoteParam(static_cast<ModCommand::NOTE>(note + NOTE_MIN)).Sample(static_cast<SAMPLEINDEX>(m_index));
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

	bool Normalize()
	{
		//auto sample = GetSample();
		return false;
	}

	bool Amplify(double amplifyStart, sol::optional<double> amplifyEnd, sol::optional<SmpLength> start, sol::optional<SmpLength> end, sol::optional<EnumFadelaw::values> fadeLawAPI)
	{
		Fade::Law fadeLaw = Fade::kLinear;
		if(fadeLawAPI.has_value())
		{
			if(fadeLawAPI.value() >= EnumFadelaw::size)
				throw "Invalid fade law";

			switch(fadeLawAPI.value())
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
		if(SampleEdit::AmplifySample(sample, start.value_or(0), end.value_or(sample->nLength), amplifyStart, amplifyEnd.value_or(amplifyStart), true, fadeLaw, SndFile()))
		{
			SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Data());
			return true;
		}
		return false;
	}

	bool ChangeStereoSeparation(double factor, sol::optional<SmpLength> start, sol::optional<SmpLength> end)
	{
		auto sample = GetSample();
		if(SampleEdit::StereoSepSample(sample, start.value_or(0u), end.value_or(sample->nLength), factor, SndFile()))
		{
			SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Data());
			return true;
		}
		return false;
	}

	bool Resample(int newRate, const char *interpolationType)
	{
		return false;
	}

	bool Reverse(sol::optional<SmpLength> start, sol::optional<SmpLength> end)
	{
		auto sample = GetSample();
		if(SampleEdit::ReverseSample(sample, start.value_or(0u), end.value_or(sample->nLength), SndFile()))
		{
			SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Data());
			return true;
		}
		return false;
	}

	bool Silence(sol::optional<SmpLength> start, sol::optional<SmpLength> end)
	{
		auto sample = GetSample();
		if(SampleEdit::SilenceSample(sample, start.value_or(0u), end.value_or(sample->nLength), SndFile()))
		{
			SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Data());
			return true;
		}
		return false;
	}

	bool Invert(sol::optional<SmpLength> start, sol::optional<SmpLength> end)
	{
		auto sample = GetSample();
		if(SampleEdit::InvertSample(sample, start.value_or(0u), end.value_or(sample->nLength), SndFile()))
		{
			SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Data());
			return true;
		}
		return false;
	}

	bool Unsign(sol::optional<SmpLength> start, sol::optional<SmpLength> end)
	{
		auto sample = GetSample();
		if(SampleEdit::UnsignSample(sample, start.value_or(0u), end.value_or(sample->nLength), SndFile()))
		{
			SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Data());
			return true;
		}
		return false;
	}

	void SetData(const sol::variadic_args &va)
	{
		auto sndFile = SndFile();
		auto sample = GetSample();
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
					}
					break;
					case 2:
					{
						int16 *dst = sample->sample16() + chn;
						for(const auto &v : data)
						{
							*dst = mpt::saturate_round<int16>(v.second.as<double>() * 32768.0);
							dst += numChannels;
						}
					}
					break;
					}
				} catch(const std::exception &)
				{
					throw;
				}
			}
			sample->PrecomputeLoops(sndFile, CMainFrame::GetMainFrame()->GetSoundFilePlaying() == SndFile());
		}
		SetModified(sample, SampleHint(static_cast<SAMPLEINDEX>(m_index)).Info().Data());
	}

	//SampleFlags uFlags;						// Sample flags (see ChannelFlags enum)
	/*int8   RelativeTone;					// Relative note to middle c (for MOD/XM)
	int8   nFineTune;						// Finetune period (for MOD/XM), -128...127
	uint8  nVibType;						// Auto vibrato type, see VibratoType enum
	uint8  nVibSweep;						// Auto vibrato sweep (i.e. how long it takes until the vibrato effect reaches its full strength)
	uint8  nVibDepth;						// Auto vibrato depth
	uint8  nVibRate;						// Auto vibrato rate (speed)
	uint8  rootNote;						// For multisample import

	SmpLength cues[9];
	adlib
	*/
};

struct SampleList : public ProxyContainer
    , public Obj
{
	SampleList(const Obj &obj) noexcept
	    : Obj(obj) {}

	size_t size() override { return SndFile()->GetNumSamples(); }
	iterator begin() override { return iterator(*this, 1); }
	iterator end() override { return iterator(*this, SndFile()->GetNumSamples() + 1); }
	value_type operator[](size_t index) override
	{
		if(index > 0 && index <= size())
			return sol::make_object<Sample>(m_script, *this, static_cast<SAMPLEINDEX>(index));
		else
			return sol::make_object(m_script, sol::nil);
	}
};

}  // namespace Scripting

OPENMPT_NAMESPACE_END
