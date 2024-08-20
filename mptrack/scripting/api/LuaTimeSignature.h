/*
 * LuaTimeSignature.h
 * ------------------
 * Purpose: Lua wrappers for CModDoc and contained classes
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once
#include "LuaObject.h"

OPENMPT_NAMESPACE_BEGIN

namespace Scripting
{

static sol::table MakeTimeSignature(sol::state &lua, ROWINDEX rpb, ROWINDEX rpm, const TempoSwing &swing)
{
	std::vector<double> swingD;
	swingD.resize(swing.size());
	for(size_t i = 0; i < swing.size(); i++)
	{
		swingD[i] = swing[i] / static_cast<double>(TempoSwing::Unity);
	}

	sol::table t = lua.create_table_with(
		  "rows_per_beat", rpb
		, "rows_per_measure", rpm
		, "swing", sol::nil);
	if(!swingD.empty())
		t["swing"] = sol::as_table(std::move(swingD));
	return t;
}

static std::tuple<ROWINDEX, ROWINDEX, TempoSwing> ParseTimeSignature(sol::object obj, bool allowEmptySignature)
{
	if(obj.is<sol::table>())
	{
		auto t = obj.as<sol::table>();
		ROWINDEX rpb = t.get_or<ROWINDEX>("rows_per_beat", 0);
		rpb = t.get_or<ROWINDEX>(1, 0);
		ROWINDEX rpm = t.get_or<ROWINDEX>("rows_per_measure", 0);
		std::vector<double> swingD;
		if(t["swing"].get_type() == sol::type::table)
			swingD = t.get<std::vector<double>>("swing");

		TempoSwing swing;
		swing.resize(swingD.size());
		for(size_t i = 0; i < swing.size(); i++)
		{
			swing[i] = mpt::saturate_round<TempoSwing::value_type>(swingD[i] * TempoSwing::Unity);
		}
		swing.Normalize();

		if(rpb == rpm && rpb == 0 && allowEmptySignature)
			throw "Invalid time signature";
		if(rpb > rpm || (!swingD.empty() && swingD.size() != rpb))
			throw "Invalid time signature";
		else
			return std::make_tuple(rpb, rpm, swing);
	} else if(obj == sol::nil)
	{
		return std::make_tuple(0, 0, TempoSwing());
	} else
	{
		throw "Invalid parameter, expected openmpt.Song.TimeSignature";
	}
}

struct TimeSignature : public Obj
{
	static const size_t GLOBAL = std::numeric_limits<size_t>::max();

	static void Register(sol::table &table)
	{
		table.new_usertype<TimeSignature>("TimeSignature"
			, sol::meta_function::to_string, &Name
			, sol::meta_function::type, &Type
			, "rows_per_beat", sol::property(&GetRowsPerBeat, &SetRowsPerBeat)
			//, "rows_per_measure", &TimeSignature::rpm
			//, "swing", &TimeSignature::swing
			);
	}

	TimeSignature(const Obj &obj, size_t pattern = GLOBAL) noexcept : Obj{obj, pattern} { }

	std::string Name() const { return "[openmpt.Song.TimeSignature object]"; }
	static std::string Type() { return "openmpt.Song.TimeSignature"; }

	TimeSignature &operator=(const TimeSignature &other)
	{
		auto sndFile = SndFile();
		if(m_index == GLOBAL)
		{
			sndFile->m_nDefaultRowsPerBeat = other.GetRowsPerBeat();
			sndFile->m_nDefaultRowsPerMeasure = other.GetRowsPerMeasure();
			//sndFile->m_tempoSwing = ...
		} else if (sndFile->Patterns.IsValidPat(static_cast<PATTERNINDEX>(m_index)))
		{
			// ...
		} else
		{
			throw "Invalid Pattern";
		}
		return *this;
	}

	ROWINDEX GetRowsPerBeat() const
	{
		auto sndFile = SndFile();
		if(m_index == GLOBAL)
			return sndFile->m_nDefaultRowsPerBeat;
		else if(sndFile->Patterns.IsValidPat(static_cast<PATTERNINDEX>(m_index)))
			return sndFile->Patterns[static_cast<PATTERNINDEX>(m_index)].GetRowsPerBeat();
		else
			throw "Invalid Pattern";
	}
	void SetRowsPerBeat(ROWINDEX rows)
	{
		// TODO Sanitize
		auto sndFile = SndFile();
		if(m_index == GLOBAL)
			sndFile->m_nDefaultRowsPerBeat = rows;
		else if(sndFile->Patterns.IsValidPat(static_cast<PATTERNINDEX>(m_index)))
			sndFile->Patterns[static_cast<PATTERNINDEX>(m_index)].SetSignature(rows, sndFile->Patterns[static_cast<PATTERNINDEX>(m_index)].GetRowsPerMeasure());
		else
			throw "Invalid Pattern";
	}

	ROWINDEX GetRowsPerMeasure() const
	{
		auto sndFile = SndFile();
		if(m_index == GLOBAL)
			return sndFile->m_nDefaultRowsPerMeasure;
		else if(sndFile->Patterns.IsValidPat(static_cast<PATTERNINDEX>(m_index)))
			return sndFile->Patterns[static_cast<PATTERNINDEX>(m_index)].GetRowsPerMeasure();
		else
			throw "Invalid Pattern";
	}
	void SetRowsPerMeasure(ROWINDEX rows)
	{
		// TODO Sanitize
		auto sndFile = SndFile();
		if(m_index == GLOBAL)
			sndFile->m_nDefaultRowsPerBeat = rows;
		else if(sndFile->Patterns.IsValidPat(static_cast<PATTERNINDEX>(m_index)))
			sndFile->Patterns[static_cast<PATTERNINDEX>(m_index)].SetSignature(sndFile->Patterns[static_cast<PATTERNINDEX>(m_index)].GetRowsPerBeat(), rows);
		else
			throw "Invalid Pattern";
	}
};

}  // namespace Scripting

OPENMPT_NAMESPACE_END
