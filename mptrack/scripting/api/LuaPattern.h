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

MPT_MAKE_ENUM(Transpose)
#undef Transpose

struct PatternCell : public Obj
{
	const ROWINDEX m_row;
	const CHANNELINDEX m_channel;

	// TODO PC events, probably separate type
	static void Register(sol::table &table)
	{
		table.new_usertype<PatternCell>("Cell"
			, sol::meta_function::to_string, &Name
			, sol::meta_function::type, &Type

			, "pattern", sol::readonly(&PatternCell::m_index)
			, "row", sol::readonly(&PatternCell::m_row)
			, "channel", sol::readonly(&PatternCell::m_channel)
			, "note", sol::property(&GetNote, &SetNote)
			, "instrument", sol::property(&GetInstrument, &SetInstrument)
			, "volume_effect", sol::property(&GetVolCmd, &SetVolCmd)
			, "volume_param", sol::property(&GetVol, &SetVol)
			, "effect", sol::property(&GetCommand, &SetCommand)
			, "param", sol::property(&GetParam, &SetParam)
			);
	}

	PatternCell(const Obj &obj, size_t pattern, ROWINDEX row, CHANNELINDEX channel) noexcept : Obj{obj, pattern}, m_row{row}, m_channel{channel} { }

	bool operator==(const PatternCell &other) const noexcept { return IsEqual(other) && std::tie(m_row, m_channel) == std::tie(other.m_row, other.m_channel); }

	PatternCell &operator=(const PatternCell &other)
	{
		auto cell = GetCell();
		auto otherCell = other.GetCell();
		if(cell == otherCell || *cell == *otherCell)
			return *this;

		*cell = *otherCell;
		SetModified(cell, RowHint(m_row));
		return *this;
	}

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
		if(m_channel < 1 || m_channel > pattern.GetNumChannels())
			throw "Invalid Channel";
		return make_lock(*pattern.GetpModCommand(m_row, m_channel - 1u), std::move(sndFile));
	}
	ScopedLock<const ModCommand> GetCell() const
	{
		return const_cast<PatternCell *>(this)->GetCell();
	}

	ModCommand::NOTE GetNote() const
	{
		return GetCell()->note;
	}
	void SetNote(ModCommand::NOTE note)
	{
		auto sndFile = SndFile();
		if(!sndFile->GetModSpecifications().HasNote(note))
			return;
		GetCell()->note = note;
		SetModified(sndFile, RowHint(m_row));
	}

	ModCommand::INSTR GetInstrument() const
	{
		return GetCell()->instr;
	}
	void SetInstrument(ModCommand::INSTR instr)
	{
		auto cell = GetCell();
		cell->instr = instr;
		SetModified(cell, RowHint(m_row));
	}

	ModCommand::VOLCMD GetVolCmd() const
	{
		return GetCell()->volcmd;
	}
	void SetVolCmd(ModCommand::VOLCMD volcmd)
	{
		auto sndFile = SndFile();
		if(!sndFile->GetModSpecifications().HasVolCommand(volcmd))
			return;

		GetCell()->volcmd = volcmd;
		SetModified(sndFile, RowHint(m_row));
	}

	ModCommand::VOL GetVol() const
	{
		return GetCell()->vol;
	}
	void SetVol(ModCommand::VOL vol)
	{
		auto cell = GetCell();
		cell->vol = vol;
		SetModified(cell, RowHint(m_row));
	}

	ModCommand::COMMAND GetCommand() const
	{
		return GetCell()->command;
	}
	void SetCommand(ModCommand::COMMAND command)
	{
		auto sndFile = SndFile();
		if(!sndFile->GetModSpecifications().HasCommand(command))
			return;

		GetCell()->command = command;
		SetModified(sndFile, RowHint(m_row));
	}

	ModCommand::PARAM GetParam() const
	{
		return GetCell()->param;
	}
	void SetParam(ModCommand::PARAM param)
	{
		auto cell = GetCell();
		cell->param = param;
		SetModified(cell, RowHint(m_row));
	}
};

struct PatternRow final : public ProxyContainer<PatternCell>, public Obj
{
	const ROWINDEX m_row;

	PatternRow(const Obj &obj, size_t pattern, size_t row) noexcept
		: Obj{obj, pattern}
		, m_row{static_cast<ROWINDEX>(row)} {}

	bool operator==(const PatternRow &other) const noexcept { return IsEqual(other) && m_row == other.m_row; }

	PatternRow &operator=(const PatternRow &other)
	{
		if(*this == other)
			return *this;
		auto pattern = GetPattern();
		auto otherPattern = other.GetPattern();

		if(pattern->GetNumChannels() != otherPattern->GetNumChannels())
			throw "Number of channels does not match between patterns";

		for(CHANNELINDEX chn = 0; chn < pattern->GetNumChannels(); chn++)
		{
			*pattern->GetpModCommand(m_row, chn) = *otherPattern->GetpModCommand(other.m_row, chn);
		}
		SetModified(pattern, RowHint(m_row));
		return *this;
	}

	ScopedLock<CPattern> GetPattern()
	{
		auto sndFile = SndFile();
		if(m_index >= sndFile->Patterns.GetNumPatterns())
			throw "Invalid Pattern";
		auto &pattern = sndFile->Patterns[static_cast<PATTERNINDEX>(m_index)];
		if(!pattern.IsValidRow(m_row))
			throw "Invalid Row";
		return make_lock(pattern, std::move(sndFile));
	}
	ScopedLock<const CPattern> GetPattern() const
	{
		return const_cast<PatternRow *>(this)->GetPattern();
	}

	size_t size() override { return GetPattern()->GetNumChannels(); }
	iterator begin() override { return iterator{*this, 0}; }
	iterator end() override { return iterator{*this, GetPattern()->GetNumChannels()}; }
	value_type operator[](size_t index) override
	{
		auto pattern = GetPattern();
		return PatternCell{*this, m_index, m_row, static_cast<CHANNELINDEX>(index + 1u)};
	}
};

struct PatternData final : public ProxyContainer<PatternRow, 0>, public Obj
{
	PatternData(const Obj &obj, size_t pattern) noexcept : Obj{obj, pattern} { }

	bool operator==(const PatternData &other) const noexcept { return IsEqual(other); }

	PatternData &operator=(const PatternData& other)
	{
		if(*this == other)
			return *this;
		auto pattern = GetPattern();
		auto otherPattern = other.GetPattern();

		if(pattern->GetNumChannels() != otherPattern->GetNumChannels())
			throw "Number of channels does not match between patterns";

		pattern->Resize(otherPattern->GetNumRows(), false);
		auto data = otherPattern->GetData();
		pattern->SetData(std::move(data));
		SetModified(pattern, PatternHint(static_cast<PATTERNINDEX>(m_index)).Data());
		return *this;
	}

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

	size_t size() override { return GetPattern()->GetNumRows(); }
	iterator begin() override { return iterator{*this, 0}; }
	iterator end() override { return iterator{*this, GetPattern()->GetNumRows()}; }
	value_type operator[] (size_t index) override
	{
		auto pattern = GetPattern();
		return PatternRow{*this, m_index, static_cast<ROWINDEX>(index)};
	}

	void push_back(const PatternRow &source)
	{
		auto sndFile = SndFile();
		auto pattern = GetPattern();
		if(pattern->GetNumRows() >= sndFile->GetModSpecifications().patternRowsMax)
			throw "Cannot add any more rows";
		pattern->Resize(pattern->GetNumRows() + 1);
		(*this)[pattern->GetNumRows()] = source;
	}
};

struct Pattern
{
	PatternData commands;
	TimeSignature timeSignature;

	static void Register(sol::table &table)
	{
		table.new_usertype<Pattern>("Pattern"
			, sol::meta_function::to_string, &Pattern::Name
			, sol::meta_function::type, &Pattern::Type

			, "index", sol::property(&Pattern::GetIndex)
			, "name", sol::property(&Pattern::GetName, &Pattern::SetName)
			, "rows", sol::property(&Pattern::GetNumRows, &Pattern::SetNumRows)
			, "data", &Pattern::commands
			, "time_signature", sol::property(&Pattern::GetTimeSignature, &Pattern::SetTimeSignature)
			, "color", sol::property(&Pattern::GetColor, &Pattern::SetColor)

			, "play", &Pattern::Play
			, "delete", &Pattern::Delete
			, "transpose", &Pattern::Transpose
			);
	}

	Pattern(const Obj &obj, size_t pattern) noexcept : commands(obj, pattern), timeSignature(obj, pattern) { }

	bool operator==(const Pattern &other) const noexcept { return commands.IsEqual(other.commands); }

	std::string Name() const { return MPT_AFORMAT("[openmpt.Song.Pattern object: Pattern {}]")(commands.m_index); }
	static std::string Type() { return "openmpt.Song.Pattern"; }

	size_t GetIndex() const { return commands.m_index; }

	ScopedLock<CPattern> GetPattern() { return commands.GetPattern(); }
	ScopedLock<const CPattern> GetPattern() const { return commands.GetPattern(); }

	void SetModified(const CriticalSection &cs, UpdateHint hint) { commands.SetModified(cs, hint); }

	Pattern operator=(const Pattern &other)
	{
		if(*this == other)
			return *this;
		auto pattern = GetPattern();
		auto otherPattern = other.GetPattern();

		*pattern = *otherPattern;
		SetModified(pattern, PatternHint(static_cast<PATTERNINDEX>(commands.m_index)).Data().Names());
		return *this;
	}

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

	sol::optional<std::vector<float>> GetColor() const
	{
		auto color = GetPattern()->GetColor();
		if(color == CPattern::INVALID_COLOR)
			return {};
		else
			return std::vector<float>{GetRValue(color) / 255.0f, GetGValue(color) / 255.0f, GetBValue(color) / 255.0f};
	}
	void SetColor(sol::object color)
	{
		auto pattern = GetPattern();
		if(color.is<sol::table>())
		{
			sol::table colorTable = color;
			if(colorTable.size() != 3)
				throw "Invalid table size (must be 3)";
			pattern->SetColor(RGB(mpt::saturate_round<uint8>(colorTable[0].get<float>() * 255.0f),
				mpt::saturate_round<uint8>(colorTable[1].get<float>() * 255.0f),
				mpt::saturate_round<uint8>(colorTable[2].get<float>() * 255.0f)));
		} else if(!color.valid())
		{
			pattern->SetColor(CPattern::INVALID_COLOR);
		} else
		{
			throw "Invalid type (must be table or nil)";
		}
		SetModified(pattern, SequenceHint(SEQUENCEINDEX_INVALID).Data());
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
		auto pattern = GetPattern();  // Just to make sure it's a valid pattern
		sndFile->Patterns.Remove(static_cast<PATTERNINDEX>(commands.m_index));
	}

	void Transpose(sol::object steps)
	{
		EnumTranspose::Enum transposeType = EnumTranspose::Enum::INVALID_VALUE;
		int numSteps = 0;
		if(steps.is<std::string_view>())
		{
			transposeType = EnumTranspose::Parse(steps.as<std::string_view>());
			if(transposeType == EnumTranspose::Enum::INVALID_VALUE)
				throw "Invalid transpose value (must be one of openmpt.Song.transpose or integer)";
		} else if (steps.is<int>())
		{
			numSteps = steps.as<int>();
		} else
		{
			throw "Invalid transpose value (must be one of openmpt.Song.transpose or integer)";
		}

		auto sndFile = commands.SndFile();
		int noteMin = sndFile->GetModSpecifications().noteMin, noteMax = sndFile->GetModSpecifications().noteMax;
		bool modified = false;
		for(auto &m : *GetPattern())
		{
			if(m.IsNote())
			{
				int step = numSteps;
				if(transposeType != EnumTranspose::Enum::INVALID_VALUE)
				{
					step = commands.ModDoc()->GetInstrumentGroupSize(m.instr);
					if(transposeType == EnumTranspose::Enum::OCTAVE_DOWN)
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
			SetModified(sndFile, PatternHint(static_cast<PATTERNINDEX>(commands.m_index)).Data());
	}

	void Play(sol::optional<int> startRow, sol::optional<int> endRow)
	{
		Script::CallInGUIThread([this, startRow, endRow]()
		{
			auto sndFile = commands.SndFile();

			// Set up row playback lock if both startRow and endRow are provided, otherwise we just start playing from the startRow
			if(startRow && endRow)
			{
				sndFile->m_lockRowStart = static_cast<ROWINDEX>(startRow.value());
				sndFile->m_lockRowEnd = static_cast<ROWINDEX>(endRow.value());
			} else
			{
				sndFile->m_lockRowStart = sndFile->m_lockRowEnd = ROWINDEX_INVALID;
			}

			sndFile->LoopPattern(static_cast<PATTERNINDEX>(commands.m_index), static_cast<ROWINDEX>(startRow.value_or(0)));
			// TODO Mustn't hold lock
			CMainFrame::GetMainFrame()->PlayMod(commands.ModDoc());
		});
	}
};

struct PatternList final : public ProxyContainer<Pattern, 0>, public Obj
{
	PatternList(const Obj &obj) noexcept : Obj{obj} { }

	PatternList operator=(const PatternList &other)
	{
		auto sndFile = SndFile();
		auto otherSndFile = other.SndFile();
		if(sndFile == otherSndFile)
			return *this;
		if(sndFile->GetNumChannels() != otherSndFile->GetNumChannels())
			throw "Number of channels does not match between patterns";

		sndFile->Patterns = otherSndFile->Patterns;
		SetModified(sndFile, PatternHint().Data().Names());
		return *this;
	}

	size_t size() override { return SndFile()->Patterns.GetNumPatterns(); }
	iterator begin() override { return iterator{*this, 0}; }
	iterator end() override { return iterator{*this, SndFile()->Patterns.GetNumPatterns()}; }
	value_type operator[] (size_t index) override { return Pattern{*this, index}; }

	void push_back(const Pattern &source)
	{
		static_assert(sol::meta::has_push_back<PatternList>::value);
		auto sndFile = SndFile();
		if(sndFile->Patterns.GetNumPatterns() >= sndFile->GetModSpecifications().patternsMax)
			throw "Cannot add any more patterns";
		PATTERNINDEX newIndex = sndFile->Patterns.GetNumPatterns();
		sndFile->Patterns.ResizeArray(newIndex + 1);
		(*this)[newIndex] = source;
	}
	void erase(iterator it)
	{
		static_assert(sol::container_detail::has_erase<PatternList>::value);
		auto sndFile = SndFile();
		if(it.m_index >= sndFile->Patterns.GetNumPatterns())
			return;
		sndFile->Patterns.Remove(static_cast<PATTERNINDEX>(it.m_index));
		SetModified(sndFile, PatternHint(static_cast<PATTERNINDEX>(it.m_index)).Data().Names());
	}
	void insert(iterator it, const Pattern &value)
	{
		static_assert(sol::meta::has_insert_with_iterator<PatternList>::value);
		auto sndFile = SndFile();
		if(it.m_index >= sndFile->Patterns.Size())
		{
			if(it.m_index >= sndFile->GetModSpecifications().patternsMax)
				throw "Cannot add any more pattern";
			sndFile->Patterns.Insert(static_cast<PATTERNINDEX>(it.m_index), value.GetNumRows());
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
struct is_container<PatternList> : std::true_type { };

template <>
struct usertype_container<PatternList> : UsertypeContainer<PatternList> { };

template <>
struct is_container<PatternData> : std::true_type { };

template <>
struct usertype_container<PatternData>
{
	static int size(lua_State *L)
	{
		auto &self = sol::stack::get<PatternData &>(L, 1);
		return sol::stack::push(L, self.size());
	}
	static auto begin(lua_State *, PatternData &that)
	{
		return that.begin();
	}
	static auto end(lua_State *, PatternData &that)
	{
		return that.end();
	}

	// Make the pattern rows 0-based
	static std::ptrdiff_t index_adjustment(lua_State *, PatternData &)
	{
		return 0;
	}
};

}  // namespace sol
