/*
 * LuaPattern.h
 * ------------
 * Purpose: Lua wrappers for CModDoc and contained classes
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once
#include "LuaObject.h"
#include "LuaTimeSignature.h"

OPENMPT_NAMESPACE_BEGIN

namespace Scripting
{

#define Transpose(X) \
	X(OCTAVE_DOWN) \
	X(OCTAVE_UP)

MPT_MAKE_ENUM(Transpose);
#undef Transpose


struct PatternCell : public Obj
{
	ROWINDEX m_row;
	CHANNELINDEX m_channel;

	// TODO PC events, probably separate type
	static void Register(sol::table &table)
	{
		table.new_usertype<PatternCell>("Cell"
			, sol::meta_function::to_string, &Name
			, sol::meta_function::type, &Type
			, "row", sol::readonly(&PatternCell::m_row)
			, "channel", sol::readonly(&PatternCell::m_channel)	// TODO make 1-based!
			, "note", sol::property(&GetNote, &SetNote)
			, "instrument", sol::property(&GetInstrument, &SetInstrument)
			, "volume_effect", sol::property(&GetVolCmd, &SetVolCmd)
			, "volume_param", sol::property(&GetVol, &SetVol)
			, "effect", sol::property(&GetCommand, &SetCommand)
			, "param", sol::property(&GetParam, &SetParam)
			);
	}

	PatternCell(const Obj &obj, size_t pattern, ROWINDEX row, CHANNELINDEX channel) noexcept : Obj(obj, pattern), m_row(row), m_channel(channel) { }

	std::string Name() const { return "[Pattern Cell Object]"; }
	static std::string Type() { return "openmpt.Song.Pattern.Cell"; }

	ScopedLock<ModCommand> GetCell()
	{
		CriticalSection cs;
		auto sndFile = SndFile();
		if(!sndFile->Patterns.IsValidPat(static_cast<PATTERNINDEX>(m_index)))
			throw "Invalid Pattern";
		auto &pattern = sndFile->Patterns[static_cast<PATTERNINDEX>(m_index)];
		if(!pattern.IsValidRow(m_row))
			throw "Invalid Row";
		if(m_channel > pattern.GetNumChannels())
			throw "Invalid Channel";
		return make_lock(*pattern.GetpModCommand(m_row, m_channel), std::move(sndFile));
	}
	ScopedLock<const ModCommand> GetCell() const
	{
		return const_cast<PatternCell *>(this)->GetCell();
	}

	ModCommand::NOTE GetNote() const { return GetCell()->note; }
	void SetNote(ModCommand::NOTE note) { GetCell()->note = note; SetModified(RowHint(static_cast<ROWINDEX>(m_row))); }

	ModCommand::INSTR GetInstrument() const { return GetCell()->instr; }
	void SetInstrument(ModCommand::INSTR instr) { GetCell()->instr = instr; SetModified(RowHint(static_cast<ROWINDEX>(m_row))); }

	ModCommand::VOLCMD GetVolCmd() const { return GetCell()->volcmd; }
	void SetVolCmd(ModCommand::VOLCMD volcmd) { GetCell()->volcmd = volcmd; SetModified(RowHint(static_cast<ROWINDEX>(m_row))); }

	ModCommand::VOL GetVol() const { return GetCell()->vol; }
	void SetVol(ModCommand::VOL vol) { GetCell()->vol = vol; SetModified(RowHint(static_cast<ROWINDEX>(m_row))); }

	ModCommand::COMMAND GetCommand() const { return GetCell()->command; }
	void SetCommand(ModCommand::COMMAND command) { GetCell()->command = command; SetModified(RowHint(static_cast<ROWINDEX>(m_row))); }

	ModCommand::PARAM GetParam() const { return GetCell()->param; }
	void SetParam(ModCommand::PARAM param) { GetCell()->param = param; SetModified(RowHint(static_cast<ROWINDEX>(m_row))); }
};

struct PatternData : public ProxyContainer, public Obj
{
	PatternData(const Obj &obj, size_t pattern) noexcept : Obj(obj, pattern) { }

	ScopedLock<CPattern> GetPattern()
	{
		auto sndFile = SndFile();
		if(m_index < sndFile->Patterns.GetNumPatterns())
			return make_lock(sndFile->Patterns[static_cast<PATTERNINDEX>(m_index)], std::move(sndFile));
		throw "Invalid Pattern";
	}
	ScopedLock<const CPattern> GetPattern() const
	{
		return const_cast<PatternData *>(this)->GetPattern();
	}

	size_t size() override { return GetPattern()->GetData().size(); }
	iterator begin() override { return iterator(*this, 0); }
	iterator end() override { return iterator(*this, GetPattern()->GetData().size()); }
	value_type operator[] (size_t index) override
	{
		auto pattern = GetPattern();
		if(index < pattern->GetData().size())
			return sol::make_object<PatternCell>(m_script, *this, m_index, static_cast<ROWINDEX>(index / pattern->GetNumChannels()), static_cast<CHANNELINDEX>(index % pattern->GetNumChannels()));
		else
			return sol::make_object(m_script, sol::nil);
	}
};

struct Pattern
{
	PatternData commands;
	TimeSignature timeSignature;

	static void Register(sol::table &table)
	{
		table.new_usertype<Pattern>("Pattern"
			, sol::meta_function::to_string, &Name
			, sol::meta_function::type, &Type
			//, "id", sol::readonly(&Pattern::commands.m_index)
			, "name", sol::property(&GetName, &SetName)
			, "rows", sol::property(&GetNumRows, &SetNumRows)
			, "data", &Pattern::commands
			//, "time_signature", &Pattern::timeSignature
			, "time_signature", sol::property(&GetTimeSignature, &SetTimeSignature)
			//, "play"
			, "delete", &Delete
			, "transpose", &Transpose
			);
	}

	Pattern(const Obj &obj, size_t pattern) noexcept : commands(obj, pattern), timeSignature(obj, pattern) { }

	std::string Name() const { return MPT_FORMAT("[openmpt.Song.Pattern object: Pattern {}]")(commands.m_index); }
	static std::string Type() { return "openmpt.Song.Pattern"; }

	ScopedLock<CPattern> GetPattern() { return commands.GetPattern(); }
	ScopedLock<const CPattern> GetPattern() const { return commands.GetPattern(); }

	void SetModified(const CriticalSection &cs, UpdateHint hint) { commands.SetModified(cs, hint); }

	std::string GetName() const
	{
		return commands.FromSndFileString(GetPattern()->GetName());
	}
	void SetName(const std::string &name)
	{
		auto sndFile = commands.SndFile();
		if(sndFile->GetModSpecifications().hasPatternNames)
		{
			GetPattern()->SetName(commands.ToSndFileString(name).substr(0, MAX_PATTERNNAME));
			SetModified(sndFile, PatternHint(static_cast<PATTERNINDEX>(commands.m_index)).Names());
		}
	}

	sol::object GetTimeSignature() const
	{
		auto pattern = GetPattern();
		if(pattern->GetOverrideSignature())
			return MakeTimeSignature(commands.m_script, pattern->GetRowsPerBeat(), pattern->GetRowsPerMeasure(), pattern->GetTempoSwing());
		else
			return sol::make_object(commands.m_script, sol::nil);
	}
	void SetTimeSignature(const sol::object &obj)
	{
		auto pattern = GetPattern();
		auto [rpb, rpm, swing] = ParseTimeSignature(obj, true);
		if(pattern->GetSoundFile().m_nTempoMode != TempoMode::Modern)
			swing.clear();

		if(rpb == 0 && rpm == 0)
			pattern->RemoveSignature();
		else
			pattern->SetSignature(rpb, rpm);

		pattern->SetTempoSwing(std::move(swing));

		SetModified(pattern, PatternHint(static_cast<PATTERNINDEX>(commands.m_index)).Data());
	}

	int GetNumRows() const
	{
		return GetPattern()->GetNumRows();
	}
	void SetNumRows(int rows)
	{
		auto pattern = GetPattern();
		if(pattern->Resize(rows, true))
			SetModified(pattern, PatternHint(static_cast<PATTERNINDEX>(commands.m_index)).Data());
	}

	void Delete()
	{
		auto sndFile = commands.SndFile();
		auto pattern = GetPattern();	// Just to make sure it's a valid pattern
		sndFile->Patterns.Remove(static_cast<PATTERNINDEX>(commands.m_index));
	}

	void Transpose(int steps)
	{
		auto sndFile = commands.SndFile();
		int noteMin = sndFile->GetModSpecifications().noteMin, noteMax = sndFile->GetModSpecifications().noteMax;
		bool modified = false;
		for(auto &m : *GetPattern())
		{
			if(m.IsNote())
			{
				int step = steps;
				if(steps == EnumTranspose::OCTAVE_DOWN || steps == EnumTranspose::OCTAVE_UP)
				{
					step = commands.ModDoc()->GetInstrumentGroupSize(m.instr);
					if(steps == EnumTranspose::OCTAVE_DOWN)
						step = -step;
				}
				auto newNote = static_cast<ModCommand::NOTE>(Clamp(m.note + step, noteMin, noteMax));
				if(m.note != newNote)
				{
					m.note = newNote;
					modified = true;
				}
			}
		}
		if(modified)
		{
			SetModified(sndFile, PatternHint(static_cast<PATTERNINDEX>(commands.m_index)).Data());
		}
	}
};

struct PatternList : public ProxyContainer, public Obj
{
	PatternList(const Obj &obj) noexcept : Obj(obj) { }

	size_t size() override { return SndFile()->Patterns.GetNumPatterns(); }
	iterator begin() override { return iterator(*this, 0); }
	iterator end() override { return iterator(*this, SndFile()->Patterns.GetNumPatterns()); }
	value_type operator[] (size_t index) override
	{
		if(index < size())
			return sol::make_object<Pattern>(m_script, *this, index);
		else
			return sol::make_object(m_script, sol::nil);
	}
};

}

OPENMPT_NAMESPACE_END

namespace sol
{
using namespace OPENMPT_NAMESPACE::Scripting;

template <>
struct is_container<PatternList> : std::true_type { };

template <>
struct usertype_container<PatternList>
{
	static int size(lua_State *L)
	{
		auto &self = sol::stack::get<PatternList &>(L, 1);
		return sol::stack::push(L, self.size());
	}
	static auto begin(lua_State *, PatternList &that)
	{
		return that.begin();
	}
	static auto end(lua_State *, PatternList &that)
	{
		return that.end();
	}

	// Make the pattern table 0-based
	static int index_adjustment(lua_State *, PatternList &)
	{
		return 0;
	}
};

}  // namespace sol
