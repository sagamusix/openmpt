/*
 * LuaSong.h
 * ---------
 * Purpose: Lua wrappers for CModDoc and contained classes
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once
#include "LuaObject.h"
#include "LuaChannel.h"
#include "LuaInstrument.h"
#include "LuaPattern.h"
#include "LuaPlugin.h"
#include "LuaSample.h"
#include "LuaSequence.h"
#include "../../soundlib/Tables.h"

OPENMPT_NAMESPACE_BEGIN

namespace Scripting
{

#define Format(X) \
	X(MOD) \
	X(XM) \
	X(S3M) \
	X(IT) \
	X(MPTM)

MPT_MAKE_ENUM(Format);
#undef Format

struct Song : public Obj
{
	static void Register(sol::table &table)
	{
		table.new_usertype<Song>("Song"
			, sol::meta_function::to_string, &Name
			, sol::meta_function::type, &Type
			, "save", &Save
			, "close", &Close
			//, "load_samples"
			//, "load_instruments"
			, "modified", sol::property(&IsModified)
			, "path", sol::property(&GetPath)
			, "name", sol::property(&GetName, &SetName)
			, "artist", sol::property(&GetArtist, &SetArtist)
			, "channels", &Song::Channels
			, "sample_volume", sol::property(&GetSampleVolume, &SetSampleVolume)
			, "plugin_volume", sol::property(&GetPluginVolume, &SetPluginVolume)
			, "initial_global_volume", sol::property(&GetGlobalVolume, &SetGlobalVolume)
			, "current_global_volume", sol::property(&GetCurrentGlobalVolume, &SetCurrentGlobalVolume)
			, "current_tempo", sol::property(&GetCurrentTempo, &SetCurrentTempo)
			, "current_speed", sol::property(&GetCurrentSpeed, &SetCurrentSpeed)
			, "time_signature", sol::property(&GetTimeSignature, &SetTimeSignature)
			, "type", sol::property(&GetType, &SetType)
			, "samples", &Song::Samples
			, "instruments", &Song::Instruments
			, "patterns", &Song::Patterns
			, "plugins", &Song::Plugins
			, "sequences", &Song::Sequences
			, "current_sequence", &Song::CurrentSequence
			, "focus", &Song::Focus
			, "play", &Song::Play
			//, "play_song_from", &Song::PlaySongFrom
			//, "play_pattern_from" -- shouldn't this be a pattern function?
			, "pause", &Song::Pause
			, "stop", &Song::Stop
			//, "observe", [](const Song &/*song*/, sol::object /*obj*/, sol::protected_function /*func*/) {}
			// pattern cursor position (also as notifiable event?)
			// current sample
			// current instrument
			, "length", sol::property(&GetLength)
			, "elapsed_time", sol::property(&GetElapsedTime)
			, "note_name", sol::as_function(&GetNoteName)
			// export (to streaming file or render_to_sample)
			);
	}

	SampleList Samples;
	InstrumentList Instruments;
	PatternList Patterns;
	PluginList Plugins;
	ChannelList Channels;
	SequenceList Sequences;
	Sequence CurrentSequence;

	Song(Script &script, CModDoc &modDoc) noexcept
		: Obj{script, modDoc}
		, Samples{*this}
		, Instruments{*this}
		, Patterns{*this}
		, Plugins{*this}
		, Channels{*this}
		, Sequences{*this}
		, CurrentSequence{*this, SEQUENCEINDEX_INVALID}
	{ }

	bool operator==(const Song &other) const noexcept { return IsEqual(other); }

	std::string Name() const { return "[openmpt.Song object: " + GetName() + "]"; }
	static std::string Type() { return "openmpt.Song"; }

	void Save(const sol::optional<std::string> &name)
	{
		Script::CallInGUIThread([this, &name]()
		{
			auto modDoc = ModDoc();
			if(!name || name.value().empty())
				modDoc->SaveModified();
			else
				modDoc->OnSaveDocument(mpt::PathString::FromUTF8(name.value()));
		});
	}

	void Close()
	{
		// Should document that this can fail if there is an active dialog (doesn't matter if modal).
		// TODO: Not a good idea, we hold the lock at this point and Close() will stop audio if this module is playing (which requires to acquire the lock)
		ModDoc()->PostMessageToAllViews(WM_COMMAND, ID_FILE_CLOSE);
		//Script::CallInGUIThread([this]() { ModDoc()->SafeFileClose(); });
	}

	void Focus()
	{
		// TODO: how to deal with open dialogs
		Script::CallInGUIThread([this]() { ModDoc()->ActivateWindow(); });
	}

	std::string GetPath() const
	{
		return ModDoc()->GetPathNameMpt().ToUTF8();
	}

	bool IsModified() const
	{
		return ModDoc()->IsModified();
	}

	int GetNumChannels() const
	{
		return ModDoc()->GetNumChannels();
	}
	void SetNumChannels(int num)
	{
		Script::CallInGUIThread([this, num]() { ModDoc()->ChangeNumChannels(static_cast<CHANNELINDEX>(num), false); });
	}

	std::string GetName() const
	{
		auto sndFile = SndFile();
		return FromSndFileString(sndFile->m_songName, sndFile);
	}
	void SetName(const std::string &name)
	{
		auto sndFile = SndFile();
		SndFile()->m_songName = ToSndFileString(name.substr(0, sndFile->GetModSpecifications().modNameLengthMax), sndFile);
		SetModified(sndFile, GeneralHint().General());
	}

	std::string GetArtist() const
	{
		return mpt::ToCharset(mpt::Charset::UTF8, SndFile()->m_songArtist);
	}
	void SetArtist(const std::string &artist)
	{
		auto sndFile = SndFile();
		if(sndFile->GetModSpecifications().hasArtistName)
		{
			sndFile->m_songArtist = mpt::ToUnicode(mpt::Charset::UTF8, artist);
			SetModified(sndFile, GeneralHint().General());
		}
	}

	double GetSampleVolume()
	{
		return SndFile()->m_nSamplePreAmp;  // TODO convert to dB or at least map 1.0 to 0dB?
	}
	void SetSampleVolume(double volume)
	{
		auto sndFile = SndFile();
		sndFile->m_nSamplePreAmp = mpt::saturate_round<uint32>(Clamp(volume, 0.0, 2000.0));  // TODO convert from dB or at least map 1.0 to 0dB?
		SetModified(sndFile, GeneralHint().General());
	}

	double GetPluginVolume()
	{
		return SndFile()->m_nVSTiVolume; // TODO convert to dB or at least map 1.0 to 0dB?
	}
	void SetPluginVolume(double volume)
	{
		auto sndFile = SndFile();
		sndFile->m_nVSTiVolume = mpt::saturate_round<uint32>(Clamp(volume, 0.0, 2000.0));  // TODO convert from dB or at least map 1.0 to 0dB?
		SetModified(sndFile, GeneralHint().General());
	}

	double GetGlobalVolume() const
	{
		return SndFile()->m_nDefaultGlobalVolume / 256.0;
	}
	void SetGlobalVolume(double volume)
	{
		auto sndFile = SndFile();
		sndFile->m_nDefaultGlobalVolume = mpt::saturate_round<uint32>(Clamp(volume, 0.0, 1.0) * 256.0);
		SetModified(sndFile, GeneralHint().General());
	}

	// TODO: "current" stuff should probably go to "playstate" sub struct?
	double GetCurrentGlobalVolume() const
	{
		return SndFile()->m_PlayState.m_nGlobalVolume / 256.0;
	}
	void SetCurrentGlobalVolume(double volume)
	{
		SndFile()->m_PlayState.m_nGlobalVolume = mpt::saturate_round<uint32>(Clamp(volume, 0.0, 1.0) * 256.0);
	}

	TEMPO DoubleToTempo(double tempo)
	{
		const auto &specs = SndFile()->GetModSpecifications();
		if(!specs.hasFractionalTempo)
			tempo = mpt::round(tempo);
		return TEMPO(Clamp(tempo, specs.tempoMinInt, specs.tempoMaxInt));
	}

	// TODO: "current" stuff should probably go to "playstate" sub struct?
	double GetCurrentTempo() const
	{
		return SndFile()->m_PlayState.m_nMusicTempo.ToDouble();
	}
	void SetCurrentTempo(double tempo)
	{
		SndFile()->m_PlayState.m_nMusicTempo = DoubleToTempo(tempo);
	}

	// TODO: "current" stuff should probably go to "playstate" sub struct?
	uint32 GetCurrentSpeed() const
	{
		return SndFile()->m_PlayState.m_nMusicSpeed;
	}
	void SetCurrentSpeed(uint32 speed)
	{
		SndFile()->m_PlayState.m_nMusicSpeed = Clamp(speed, 1u, 255u);
	}

	sol::object GetTimeSignature() const
	{
		auto sndFile = SndFile();
		return MakeTimeSignature(m_script, sndFile->m_nDefaultRowsPerBeat, sndFile->m_nDefaultRowsPerMeasure, sndFile->m_tempoSwing);
	}
	void SetTimeSignature(const sol::object &obj)
	{
		auto sndFile = SndFile();
		auto [rpb, rpm, swing] = ParseTimeSignature(obj, false);
		if(sndFile->m_nTempoMode != TempoMode::Modern)
			swing.clear();

		sndFile->m_nDefaultRowsPerBeat = rpb;
		sndFile->m_nDefaultRowsPerMeasure = rpm;
		sndFile->m_tempoSwing = std::move(swing);

		SetModified(sndFile, PatternHint().Data().ModType());
	}

	EnumFormat::values GetType() const
	{
		switch(SndFile()->GetType())
		{
		case MOD_TYPE_MOD: return EnumFormat::MOD;
		case MOD_TYPE_XM: return EnumFormat::XM;
		case MOD_TYPE_S3M: return EnumFormat::S3M;
		case MOD_TYPE_IT: return EnumFormat::IT;
		case MOD_TYPE_MPT: return EnumFormat::MPTM;
		default: throw "Unknown type";
		}
	}

	void SetType(EnumFormat::values type)
	{
		MODTYPE newType = MOD_TYPE_NONE;
		switch (type)
		{
		case EnumFormat::MOD: newType = MOD_TYPE_MOD; break;
		case EnumFormat::XM: newType = MOD_TYPE_XM; break;
		case EnumFormat::S3M: newType = MOD_TYPE_S3M; break;
		case EnumFormat::IT: newType = MOD_TYPE_IT; break;
		case EnumFormat::MPTM: newType = MOD_TYPE_MPT; break;
		default: throw "Unknown type (must be one of openmpt.Song.format)";
		}
		Script::CallInGUIThread([this, newType]()
		{
			auto modDoc = ModDoc();
			// TODO this may be interactive
			if (modDoc->ChangeModType(newType))
			{
				SetModified(modDoc, UpdateHint().ModType());
			}
		});
	}

	sol::table GetLength() const
	{
		auto sndFile = SndFile();
		sol::state &lua = m_script;
		sol::table songTable = lua.create_table();
		for(SEQUENCEINDEX seq = 0; seq < sndFile->Order.GetNumSequences(); seq++)
		{
			auto length = const_cast<CSoundFile &>(*sndFile).GetLength(eNoAdjust, GetLengthTarget(true).StartPos(seq, 0, 0));
			for(const auto &l : length)
			{
				songTable.add(lua.create_table_with(
					"duration", l.duration,
					"start_sequence", Sequence{*this, seq},
					"start_order", l.startOrder,
					"start_row", l.startRow
				));
			}
		}
		return songTable;
	}

	double GetElapsedTime() const
	{
		auto sndFile = SndFile();
		return static_cast<double>(sndFile->m_PlayState.m_lTotalSampleCount) / sndFile->GetSampleRate();
	}

	void Play(sol::optional<int> startOrder, sol::optional<int> startRow)
	{
		Script::CallInGUIThread([this, &startOrder, &startRow]()
		{
			ModDoc()->SetElapsedTime(static_cast<ORDERINDEX>(startOrder.value_or(0)), static_cast<ROWINDEX>(startRow.value_or(0)), true);
			// Mustn't hold lock
			CMainFrame::GetMainFrame()->PlayMod(&m_modDoc);
		});
	}

	void Pause()
	{
		Script::CallInGUIThread([this]()
		{
			// Mustn't hold lock
			CMainFrame::GetMainFrame()->PauseMod(&m_modDoc);
		});
	}

	void Stop()
	{
		Script::CallInGUIThread([this]()
		{
			// Mustn't hold lock
			CMainFrame::GetMainFrame()->StopMod(&m_modDoc);
		});
	}

	std::string GetNoteName(ModCommand::NOTE note, sol::object instr, sol::optional<bool> useFlats) const
	{
		auto sndFile = SndFile();
		INSTRUMENTINDEX ins = 0;
		if(instr)
		{
			if(instr.is<INSTRUMENTINDEX>())
				ins = instr.as<INSTRUMENTINDEX>();
			else if(instr.is<Instrument>())
				ins = static_cast<INSTRUMENTINDEX>(instr.as<Instrument>().m_index);
			else
				throw "Invalid instrument type (index or instrument object expected)";
		}
		const NoteName *noteNames = nullptr;
		if(useFlats)
			noteNames = *useFlats ? NoteNamesFlat : NoteNamesSharp;
		return mpt::ToCharset(mpt::Charset::UTF8, sndFile->GetNoteName(note, ins, noteNames));
	}
};

}  // namespace Scripting

OPENMPT_NAMESPACE_END
