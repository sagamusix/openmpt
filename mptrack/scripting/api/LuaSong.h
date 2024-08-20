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
#include "LuaTuning.h"
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

MPT_MAKE_ENUM(Format)
#undef Format

#define MixLevels(X) \
	X(ORIGINAL) \
	X(V1_17RC1) \
	X(V1_17RC2) \
	X(V1_17RC3) \
	X(COMPATIBLE) \
	X(COMPATIBLE_FT2)

MPT_MAKE_ENUM(MixLevels)
#undef MixLevels

#define TempoMode(X) \
	X(CLASSIC) \
	X(ALTERNATIVE) \
	X(MODERN)

MPT_MAKE_ENUM(TempoMode)
#undef TempoMode

struct Song : public Obj
{
	static void Register(sol::table &table)
	{
		table.new_usertype<Song>("Song"
			, sol::meta_function::to_string, &Name
			, sol::meta_function::type, &Type

			, "modified", sol::property(&IsModified)
			, "path", sol::property(&GetPath)
			, "name", sol::property(&GetName, &SetName)
			, "artist", sol::property(&GetArtist, &SetArtist)
			, "channels", &Song::Channels
			, "linear_frequency_slides", sol::property(&GetLinearFrequencySlides, &SetLinearFrequencySlides)
			, "it_old_effects", sol::property(&GetITOldEffects, &SetITOldEffects)
			, "fast_volume_slides", sol::property(&GetFastVolumeSlides, &SetFastVolumeSlides)
			, "amiga_frequency_limits", sol::property(&GetAmigaFrequencyLimits, &SetAmigaFrequencyLimits)
			, "extended_filter_range", sol::property(&GetExtendedFilterRange, &SetExtendedFilterRange)
			, "protracker_mode", sol::property(&GetProtrackerMode, &SetProtrackerMode)
			, "mix_levels", sol::property(&GetMixLevels, &SetMixLevels)
			, "tempo_mode", sol::property(&GetTempoMode, &SetTempoMode)
			, "sample_volume", sol::property(&GetSampleVolume, &SetSampleVolume)
			, "plugin_volume", sol::property(&GetPluginVolume, &SetPluginVolume)
			, "initial_global_volume", sol::property(&GetGlobalVolume, &SetGlobalVolume)
			, "current_global_volume", sol::property(&GetCurrentGlobalVolume, &SetCurrentGlobalVolume)
			, "current_tempo", sol::property(&GetCurrentTempo, &SetCurrentTempo)
			, "current_speed", sol::property(&GetCurrentSpeed, &SetCurrentSpeed)
			, "time_signature", sol::property(&GetTimeSignature, &SetTimeSignature)
			, "approximate_real_bpm", &Song::ApproximateRealBPM
			, "type", sol::property(&GetType, &SetType)
			, "samples", &Song::Samples
			, "instruments", &Song::Instruments
			, "patterns", &Song::Patterns
			, "plugins", &Song::Plugins
			, "sequences", &Song::Sequences
			, "current_sequence", &Song::CurrentSequence
			, "tunings", &Song::Tunings
			, "resampling", sol::property(&GetResampling, &SetResampling)
			, "playback_range_lock", sol::property(&GetPlaybackRangeLock, &SetPlaybackRangeLock)
			, "length", sol::property(&GetLength)
			, "elapsed_time", sol::property(&GetElapsedTime)
			, "is_playing", sol::property(&IsPlaying)
			, "is_active", sol::property(&IsActive)

			, "save", & Song::Save
			, "close", & Song::Close
			//, "load_samples"
			//, "load_instruments"
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
			, "note_name", &Song::GetNoteName
			, "new_window", &Song::NewWindow
			, "set_playback_position_seconds", &Song::SetPlaybackPositionSeconds
			, "set_playback_position", &Song::SetPlaybackPosition
			, "get_playback_time_at", &Song::GetPlaybackTimeAt
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
	TuningList Tunings;

	Song(Script &script, CModDoc &modDoc) noexcept
		: Obj{script, modDoc}
		, Samples{*this}
		, Instruments{*this}
		, Patterns{*this}
		, Plugins{*this}
		, Channels{*this}
		, Sequences{*this}
		, CurrentSequence{*this, SEQUENCEINDEX_INVALID}
		, Tunings{*this}
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

	void Close(sol::optional<bool> force)
	{
		auto modDoc = ModDoc();
		// TODO: Really shouldn't repeat this logic here, but if we don't, we might set
		// the module as not-modified below even though the file isn't going to be closed...
		if(GetActiveWindow() != CMainFrame::GetMainFrame()->m_hWnd)
			return;
		POSITION pos = modDoc->GetFirstViewPosition();
		while(pos != nullptr)
		{
			if(CView *pView = modDoc->GetNextView(pos); pView != nullptr)
			{
				if(force.value_or(false))
					modDoc->SetModified(false);
				pView->PostMessage(WM_COMMAND, ID_FILE_CLOSE);
				break;
			}
		}
	}

	void Focus()
	{
		// TODO: how to deal with open dialogs
		Script::CallInGUIThread([this]()
		{
			if(GetActiveWindow() == CMainFrame::GetMainFrame()->m_hWnd)
				ModDoc()->ActivateWindow();
		});
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

	bool GetLinearFrequencySlides() const
	{
		return SndFile()->m_SongFlags[SONG_LINEARSLIDES];
	}
	void SetLinearFrequencySlides(bool enabled)
	{
		auto sndFile = SndFile();
		if(!sndFile->GetModSpecifications().songFlags[SONG_LINEARSLIDES])
			throw "Linear volume slides are not supported by the current format";
		if(sndFile->m_SongFlags[SONG_LINEARSLIDES] != enabled)
		{
			sndFile->m_SongFlags.set(SONG_LINEARSLIDES, enabled);
			SetModified(sndFile, GeneralHint().General());
		}
	}

	bool GetITOldEffects() const
	{
		return SndFile()->m_SongFlags[SONG_ITOLDEFFECTS];
	}
	void SetITOldEffects(bool enabled)
	{
		auto sndFile = SndFile();
		if(!sndFile->GetModSpecifications().songFlags[SONG_ITOLDEFFECTS])
			throw "IT old effects are not supported by the current format";
		if(sndFile->m_SongFlags[SONG_ITOLDEFFECTS] != enabled)
		{
			sndFile->m_SongFlags.set(SONG_ITOLDEFFECTS, enabled);
			SetModified(sndFile, GeneralHint().General());
		}
	}

	bool GetFastVolumeSlides() const
	{
		return SndFile()->m_SongFlags[SONG_FASTVOLSLIDES];
	}
	void SetFastVolumeSlides(bool enabled)
	{
		auto sndFile = SndFile();
		if(!sndFile->GetModSpecifications().songFlags[SONG_FASTVOLSLIDES])
			throw "Fast volume slides are not supported by the current format";
		if(sndFile->m_SongFlags[SONG_FASTVOLSLIDES] != enabled)
		{
			sndFile->m_SongFlags.set(SONG_FASTVOLSLIDES, enabled);
			SetModified(sndFile, GeneralHint().General());
		}
	}

	bool GetAmigaFrequencyLimits() const
	{
		return SndFile()->m_SongFlags[SONG_AMIGALIMITS];
	}
	void SetAmigaFrequencyLimits(bool enabled)
	{
		auto sndFile = SndFile();
		if(!sndFile->GetModSpecifications().songFlags[SONG_AMIGALIMITS])
			throw "Amiga frequency limits are not supported by the current format";
		if(sndFile->m_SongFlags[SONG_AMIGALIMITS] != enabled)
		{
			sndFile->m_SongFlags.set(SONG_AMIGALIMITS, enabled);
			SetModified(sndFile, GeneralHint().General());
		}
	}

	bool GetExtendedFilterRange() const
	{
		return SndFile()->m_SongFlags[SONG_EXFILTERRANGE];
	}
	void SetExtendedFilterRange(bool enabled)
	{
		auto sndFile = SndFile();
		if(!sndFile->GetModSpecifications().songFlags[SONG_EXFILTERRANGE])
			throw "Extended filter range is not supported by the current format";
		if(sndFile->m_SongFlags[SONG_EXFILTERRANGE] != enabled)
		{
			sndFile->m_SongFlags.set(SONG_EXFILTERRANGE, enabled);
			SetModified(sndFile, GeneralHint().General());
		}
	}

	bool GetProtrackerMode() const
	{
		return SndFile()->m_SongFlags[SONG_PT_MODE];
	}
	void SetProtrackerMode(bool enabled)
	{
		auto sndFile = SndFile();
		if(!sndFile->GetModSpecifications().songFlags[SONG_PT_MODE])
			throw "ProTracker mode is not supported by the current format";
		if(sndFile->m_SongFlags[SONG_PT_MODE] != enabled)
		{
			sndFile->m_SongFlags.set(SONG_PT_MODE, enabled);
			SetModified(sndFile, GeneralHint().General());
		}
	}

	std::string GetMixLevels() const
	{
		switch(SndFile()->GetMixLevels())
		{
		case MixLevels::Original: return EnumMixLevels::ORIGINAL;
		case MixLevels::v1_17RC1: return EnumMixLevels::V1_17RC1;
		case MixLevels::v1_17RC2: return EnumMixLevels::V1_17RC2;
		case MixLevels::v1_17RC3: return EnumMixLevels::V1_17RC3;
		case MixLevels::Compatible: return EnumMixLevels::COMPATIBLE;
		case MixLevels::CompatibleFT2: return EnumMixLevels::COMPATIBLE_FT2;
		case MixLevels::NumMixLevels: break;
		}
		return EnumMixLevels::COMPATIBLE;
	}
	void SetMixLevels(std::string_view levels)
	{
		MixLevels newLevels = MixLevels::Compatible;
		switch(EnumMixLevels::Parse(levels))
		{
		case EnumMixLevels::Enum::ORIGINAL: newLevels = MixLevels::Original; break;
		case EnumMixLevels::Enum::V1_17RC1: newLevels = MixLevels::v1_17RC1; break;
		case EnumMixLevels::Enum::V1_17RC2: newLevels = MixLevels::v1_17RC2; break;
		case EnumMixLevels::Enum::V1_17RC3: newLevels = MixLevels::v1_17RC3; break;
		case EnumMixLevels::Enum::COMPATIBLE: newLevels = MixLevels::Compatible; break;
		case EnumMixLevels::Enum::COMPATIBLE_FT2: newLevels = MixLevels::CompatibleFT2; break;
		case EnumMixLevels::Enum::INVALID_VALUE: throw "Unknown mix levels (must be one of openmpt.Song.mixLevels)";
		}
		auto sndFile = SndFile();
		const MODTYPE modType = sndFile->GetType();
		if(modType == MOD_TYPE_XM)
		{
			if(newLevels != MixLevels::CompatibleFT2 && newLevels != MixLevels::Compatible)
				throw "Only COMPATIBLE and COMPATIBLE_FT2 mix levels are allowed for XM files";
		} else if(modType == MOD_TYPE_MPT)
		{
			if(newLevels != MixLevels::Compatible && newLevels != MixLevels::v1_17RC3)
				throw "Only COMPATIBLE and V1_17RC3 mix levels are allowed for MPTM files";
		} else
		{
			if(newLevels != MixLevels::Compatible)
				throw "Only COMPATIBLE mix levels are allowed for this format";
		}
		
		if(sndFile->GetMixLevels() != newLevels)
		{
			sndFile->SetMixLevels(newLevels);
			SetModified(sndFile, GeneralHint().General());
		}
	}

	std::string GetTempoMode() const
	{
		switch(SndFile()->m_nTempoMode)
		{
		case TempoMode::Classic: return EnumTempoMode::CLASSIC;
		case TempoMode::Alternative: return EnumTempoMode::ALTERNATIVE;
		case TempoMode::Modern: return EnumTempoMode::MODERN;
		default: throw "Unknown tempo mode";
		}
	}
	void SetTempoMode(std::string_view mode)
	{
		TempoMode newMode = TempoMode::Classic;
		switch(EnumTempoMode::Parse(mode))
		{
		case EnumTempoMode::Enum::CLASSIC: newMode = TempoMode::Classic; break;
		case EnumTempoMode::Enum::ALTERNATIVE: newMode = TempoMode::Alternative; break;
		case EnumTempoMode::Enum::MODERN: newMode = TempoMode::Modern; break;
		case EnumTempoMode::Enum::INVALID_VALUE: throw "Unknown tempo mode (must be one of openmpt.Song.tempoMode)";
		}
		auto sndFile = SndFile();
		if(newMode != TempoMode::Classic && sndFile->GetType() != MOD_TYPE_MPT)
			throw "This tempo mode is not supported by the current format";
		if(sndFile->m_nTempoMode != newMode)
		{
			sndFile->m_nTempoMode = newMode;
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

	std::string GetType() const
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

	void SetType(std::string_view type)
	{
		MODTYPE newType = MOD_TYPE_NONE;
		switch(EnumFormat::Parse(type))
		{
		case EnumFormat::Enum::MOD: newType = MOD_TYPE_MOD; break;
		case EnumFormat::Enum::XM: newType = MOD_TYPE_XM; break;
		case EnumFormat::Enum::S3M: newType = MOD_TYPE_S3M; break;
		case EnumFormat::Enum::IT: newType = MOD_TYPE_IT; break;
		case EnumFormat::Enum::MPTM: newType = MOD_TYPE_MPT; break;
		case EnumFormat::Enum::INVALID_VALUE: throw "Unknown type (must be one of openmpt.Song.format)";
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

	std::string GetResampling() const
	{
		return FromResamplingMode(SndFile()->m_nResampling);
	}
	void SetResampling(std::string_view mode)
	{
		auto sndFile = SndFile();
		sndFile->m_nResampling = ToResamplingMode(mode, sndFile->m_SongFlags[SONG_ISAMIGA]);
		SetModified(sndFile, GeneralHint().General());
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

	bool IsPlaying() const
	{
		return ModDoc() == CMainFrame::GetMainFrame()->GetModPlaying();
	}

	bool IsActive() const
	{
		return ModDoc() == Manager::ActiveDoc();
	}

	void Play(sol::optional<int> startOrder, sol::optional<int> startRow)
	{
		Script::CallInGUIThread([this, &startOrder, &startRow]()
		{
			ModDoc()->SetElapsedTime(static_cast<ORDERINDEX>(startOrder.value_or(0)), static_cast<ROWINDEX>(startRow.value_or(0)), true);
			// TODO Mustn't hold lock
			CMainFrame::GetMainFrame()->PlayMod(&m_modDoc);
		});
	}

	void Pause()
	{
		Script::CallInGUIThread([this]()
		{
			// TODO Mustn't hold lock
			CMainFrame::GetMainFrame()->PauseMod(&m_modDoc);
		});
	}

	void Stop()
	{
		Script::CallInGUIThread([this]()
		{
			// TODO Mustn't hold lock
			CMainFrame::GetMainFrame()->StopMod(&m_modDoc);
		});
	}

	double ApproximateRealBPM()
	{
		auto sndFile = SndFile();
		// Ensure proper playback state for BPM calculation like in CModDoc implementation - this is dirty and should be fixed...
		if(CMainFrame::GetMainFrame()->GetModPlaying() != &m_modDoc)
		{
			sndFile->m_PlayState.m_nCurrentRowsPerBeat = sndFile->m_nDefaultRowsPerBeat;
			sndFile->m_PlayState.m_nCurrentRowsPerMeasure = sndFile->m_nDefaultRowsPerMeasure;
		}
		sndFile->RecalculateSamplesPerTick();
		return sndFile->GetCurrentBPM();
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

	bool NewWindow()
	{
		bool result = false;
		Script::CallInGUIThread([this, &result]()
		{
			auto modDoc = ModDoc();
			if(auto *docTemplate = modDoc->GetDocTemplate(); docTemplate != nullptr)
			{
				CFrameWnd *childFrm = docTemplate->CreateNewFrame(modDoc, nullptr);
				if(childFrm != nullptr)
					docTemplate->InitialUpdateFrame(childFrm, modDoc);
				result = childFrm != nullptr;
			}
		});
		return result;
	}

	void SetPlaybackPositionSeconds(double seconds)
	{
		auto sndFile = SndFile();
		auto length = sndFile->GetLength(eNoAdjust, GetLengthTarget(seconds));
		if(!length.empty())
			ModDoc()->SetElapsedTime(length.front().startOrder, length.front().startRow, true);
	}

	void SetPlaybackPosition(ORDERINDEX order, ROWINDEX row)
	{
		ModDoc()->SetElapsedTime(order, row, true);
	}

	double GetPlaybackTimeAt(ORDERINDEX order, ROWINDEX row)
	{
		// TODO optional sequence parameter?
		return SndFile()->GetPlaybackTimeAt(order, row, false, false);
	}

	sol::object GetPlaybackRangeLock() const
	{
		auto sndFile = SndFile();
		if(sndFile->m_lockOrderStart == ORDERINDEX_INVALID)
			return sol::nil;
		sol::state &lua = m_script;
		return lua.create_table_with(
			1, sndFile->m_lockOrderStart,
			2, sndFile->m_lockOrderEnd);
	}

	void SetPlaybackRangeLock(const sol::object &obj)
	{
		auto sndFile = SndFile();
		if(!obj.valid())
		{
			sndFile->m_lockOrderStart = ORDERINDEX_INVALID;
			sndFile->m_lockOrderEnd = ORDERINDEX_INVALID;
		} else if(obj.is<sol::table>())
		{
			sol::table table = obj.as<sol::table>();
			if(table.size() != 2)
				throw "Playback range lock must be a tuple of integers (start, end)";

			sol::object start = table[1];
			sol::object end = table[2];
			if(!start.is<ORDERINDEX>() || !end.is<ORDERINDEX>())
				throw "Playback range lock must be a tuple of integers (start, end)";

			ORDERINDEX startOrder = start.as<ORDERINDEX>();
			ORDERINDEX endOrder = end.as<ORDERINDEX>();
			if(startOrder > endOrder)
				std::swap(startOrder, endOrder);
			sndFile->m_lockOrderStart = startOrder;
			sndFile->m_lockOrderEnd = endOrder;
		} else
		{
			throw "Playback range lock must be nil or a tuple of (start, end) order positions";
		}
		Script::CallInGUIThread([this]()
		{
			ModDoc()->UpdateAllViews(SequenceHint{}.Data());
		});
	}
};

}  // namespace Scripting

OPENMPT_NAMESPACE_END
