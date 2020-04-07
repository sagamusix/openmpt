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
			, "tuning", sol::property(&GetTuning, &SetTuning)
			, "play", &PlayNote
			, "stop", &StopNote
			);
	}

	Instrument(const Obj &obj, size_t instr) noexcept
	    : Obj(obj, instr) {}

	std::string Name() const { return MPT_FORMAT("[openmpt.Song.Instrument object: Instrument {}]")(m_index); }
	static std::string Type() { return "openmpt.Song.Instrument"; }

	ScopedLock<ModInstrument> GetInstrument()
	{
		auto sndFile = SndFile();
		if(m_index > 0 && m_index <= sndFile->GetNumInstruments() && sndFile->Instruments[m_index] != nullptr)
			return make_lock(*sndFile->Instruments[m_index], std::move(sndFile));
		throw "Invalid Instrument";
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
			instr->nPan = static_cast<uint16>(Clamp(panning.value(), -1.0, 1.0) * 128.0 + 128.0);
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	double GetGlobalVolume() const
	{
		return GetInstrument()->nGlobalVol / 64.0;
	}
	void SetGlobalVolume(double volume)
	{
		auto instr = GetInstrument();
		instr->nGlobalVol = static_cast<uint16>(Clamp(volume, 0.0, 1.0) * 64.0);
		SetModified(instr, InstrumentHint(static_cast<INSTRUMENTINDEX>(m_index)).Info());
	}

	sol::as_table_t<std::vector<sol::object>> GetSamples() const
	{
		const auto samples = GetInstrument()->GetSamples();
		std::vector<sol::object> objects;
		objects.reserve(samples.size());
		for(auto s : samples)
		{
			objects.push_back(sol::make_object<Sample>(m_script, *this, s));
		}
		return sol::as_table(objects);
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
};

struct InstrumentList : public ProxyContainer
    , public Obj
{
	InstrumentList(const Obj &obj) noexcept
	    : Obj(obj) {}

	size_t size() override { return SndFile()->GetNumInstruments(); }
	iterator begin() override { return iterator(*this, 1); }
	iterator end() override { return iterator(*this, SndFile()->GetNumInstruments() + 1); }
	value_type operator[](size_t index) override
	{
		auto sndFile = SndFile();
		if(index > 0 && index <= sndFile->GetNumInstruments() && sndFile->Instruments[index] != nullptr)
			return sol::make_object<Instrument>(m_script, *this, index);
		else
			return sol::make_object(m_script, sol::nil);
	}
};

}  // namespace Scripting

OPENMPT_NAMESPACE_END
