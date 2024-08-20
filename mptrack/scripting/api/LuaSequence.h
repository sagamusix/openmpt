/*
 * LuaSequence.h
 * -------------
 * Purpose: Lua wrappers for CModDoc and contained classes
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once
#include "LuaObject.h"

OPENMPT_NAMESPACE_BEGIN

#define SEQUENCE_IMPL 0

namespace Scripting
{

struct SequenceData : public ProxyContainer<PATTERNINDEX, 0>, public Obj
{
	SequenceData(const Obj &obj, size_t sequence) noexcept
		: Obj{obj, sequence}
	{
	}

	bool operator==(const SequenceData &other) const noexcept { return IsEqual(other); }

	SequenceData &operator=(const SequenceData &other)
	{
		if(*this == other)
			return *this;
		auto sequence = GetSequence();
		auto otherSequence = other.GetSequence();

		static_cast<std::vector<PATTERNINDEX> &>(*sequence) = otherSequence;
		SetModified(sequence, SequenceHint(static_cast<SEQUENCEINDEX>(GetSequenceIndex())).Data());
		return *this;
	}

	SEQUENCEINDEX GetSequenceIndex() const
	{
		return (m_index == SEQUENCEINDEX_INVALID) ? SndFile()->Order.GetCurrentSequenceIndex() : static_cast<SEQUENCEINDEX>(m_index);
	}

	ScopedLock<ModSequence> GetSequence()
	{
		auto sndFile = SndFile();
		SEQUENCEINDEX index = GetSequenceIndex();
		if(index < sndFile->Order.GetNumSequences())
			return make_lock(sndFile->Order(static_cast<SEQUENCEINDEX>(index)), std::move(sndFile));
		throw "Invalid Sequence";
	}
	ScopedLock<const ModSequence> GetSequence() const
	{
		return const_cast<SequenceData *>(this)->GetSequence();
	}

	size_t size() override { return GetSequence()->GetLengthTailTrimmed(); }
	iterator begin() override { return iterator{*this, 0}; }
	iterator end() override { return iterator{*this, size()}; }
	PATTERNINDEX operator[](size_t position) override
	{
		auto sequence = GetSequence();
		if(position >= MAX_ORDERS)
			throw "Invalid sequence position!";
		if(position >= sequence->GetLength())
			return PATTERNINDEX_INVALID;  // TODO nil?
		return sequence->at(position);
	}
	size_t max_size() const noexcept { return MAX_SAMPLES; }

	void push_back(const PATTERNINDEX value)
	{
		static_assert(sol::meta::has_push_back<SequenceData>::value);
		insert(iterator{*this, GetSequence()->GetLengthTailTrimmed()}, value);
	}
	void erase(iterator it)
	{
		static_assert(sol::container_detail::has_erase<SequenceData>::value);
		auto sequence = GetSequence();
		if(it.m_index >= sequence->GetLength())
			return;
		sequence->at(static_cast<ORDERINDEX>(it.m_index)) = PATTERNINDEX_INVALID;
		SetModified(sequence, SequenceHint(static_cast<SEQUENCEINDEX>(GetSequenceIndex())).Data());
	}
	void insert(iterator it, const PATTERNINDEX value)
	{
		static_assert(sol::meta::has_insert_with_iterator<SequenceData>::value);
		auto sequence = GetSequence();
		if(it.m_index >= MAX_ORDERS)
			throw "Invalid sequence position!";
		if(it.m_index >= sequence->GetLengthTailTrimmed() && it.m_index >= SndFile()->GetModSpecifications().ordersMax)
			throw "Cannot add any more sequence items!";
		if(value >= MAX_PATTERNS && value != PATTERNINDEX_SKIP && value != PATTERNINDEX_INVALID)
			throw "Invalid pattern index!";
		if(it.m_index >= sequence->GetLength())
			sequence->resize(static_cast<ORDERINDEX>(it.m_index + 1));
		sequence->at(static_cast<ORDERINDEX>(it.m_index)) = value;
		SetModified(sequence, SequenceHint(static_cast<SEQUENCEINDEX>(GetSequenceIndex())).Data());
	}
};

struct Sequence : public Obj
{
	SequenceData patterns;

	static void Register(sol::table &table)
	{
		table.new_usertype<Sequence>("Sequence"
			, sol::meta_function::to_string, &Name
			, sol::meta_function::type, &Type
			, "id", sol::property(&GetSequenceIndexLua)
			, "name", sol::property(&GetName, &SetName)
			, "initial_tempo", sol::property(&GetTempo, &SetTempo)
			, "initial_speed", sol::property(&GetSpeed, &SetSpeed)
			, "restart_position", sol::property(&GetRestartPos, &SetRestartPos)
			, "patterns", &Sequence::patterns
			//, "get"
			//, "set"
			//, "erase"
			//, "insert"
			);
	}

	Sequence(const Obj &obj, size_t sequence) noexcept : Obj{obj, sequence}, patterns{obj, sequence} { }

	bool operator==(const Sequence &other) const noexcept { return IsEqual(other); }

	Sequence &operator=(const Sequence &other)
	{
		if(*this == other)
			return *this;
		auto sequence = GetSequence();
		auto otherSequence = other.GetSequence();

		*sequence = *otherSequence;
		SetModified(sequence, SequenceHint(static_cast<SEQUENCEINDEX>(GetSequenceIndex())).Names().Data());
		return *this;
	}

	std::string Name() const { return MPT_AFORMAT("[openmpt.Song.Sequence object: Sequence {}]")(GetSequenceIndexLua()); }
	static std::string Type() { return "openmpt.Song.Sequence"; }

	SEQUENCEINDEX GetSequenceIndex() const
	{
		return patterns.GetSequenceIndex();
	}
	SEQUENCEINDEX GetSequenceIndexLua() const
	{
		return GetSequenceIndex() + 1;
	}

	ScopedLock<ModSequence> GetSequence()
	{
		return patterns.GetSequence();
	}
	ScopedLock<const ModSequence> GetSequence() const
	{
		return patterns.GetSequence();
	}

	std::string GetName() const
	{
		return mpt::ToCharset(mpt::Charset::UTF8, GetSequence()->GetName());
	}
	void SetName(const std::string &name)
	{
		auto sndFile = SndFile();
		if(sndFile->GetModSpecifications().sequencesMax > 1)
		{
			GetSequence()->SetName(mpt::ToUnicode(mpt::Charset::UTF8, name));
			SetModified(sndFile, SequenceHint(static_cast<SEQUENCEINDEX>(GetSequenceIndex())).Names());
		}
	}

	// TODO de-duplicate from Song object
	TEMPO DoubleToTempo(double tempo)
	{
		const auto &specs = SndFile()->GetModSpecifications();
		if(!specs.hasFractionalTempo)
			tempo = mpt::round(tempo);
		return TEMPO(Clamp(tempo, specs.tempoMinInt, specs.tempoMaxInt));
	}

	double GetTempo() const
	{
		return GetSequence()->GetDefaultTempo().ToDouble();
	}
	void SetTempo(double tempo)
	{
		auto sndFile = SndFile();
		if(sndFile->GetType() != MOD_TYPE_MOD)
		{
			GetSequence()->SetDefaultTempo(DoubleToTempo(tempo));
			SetModified(sndFile, GeneralHint().General());
		}
	}

	uint32 GetSpeed() const
	{
		return GetSequence()->GetDefaultSpeed();
	}
	void SetSpeed(uint32 speed)
	{
		auto sndFile = SndFile();
		if(sndFile->GetType() != MOD_TYPE_MOD)
		{
			const auto &specs = sndFile->GetModSpecifications();
			GetSequence()->SetDefaultSpeed(Clamp(speed, specs.speedMin, specs.speedMax));
			SetModified(sndFile, GeneralHint().General());
		}
	}

	ORDERINDEX GetRestartPos() const
	{
		return GetSequence()->GetRestartPos();
	}
	void SetRestartPos(ORDERINDEX position)
	{
		auto sequence = GetSequence();
		if(SndFile()->GetModSpecifications().hasRestartPos)
		{
			sequence->SetRestartPos(std::min(position, sequence->GetLengthTailTrimmed()));
			SetModified(sequence, SequenceHint(static_cast<SEQUENCEINDEX>(GetSequenceIndex())).RestartPos());
		}
	}
};

struct SequenceList final : public ProxyContainer<Sequence>, public Obj
{
	SequenceList(const Obj &obj) noexcept : Obj{obj} { }

	SequenceList &operator=(const SequenceList &other)
	{
		auto sndFile = SndFile();
		auto otherSndFile = other.SndFile();
		if(sndFile == otherSndFile)
			return *this;

		sndFile->Order = otherSndFile->Order;

		SetModified(sndFile, SequenceHint().Names().Data());
		return *this;
	}

	size_t size() override { return SndFile()->Order.GetNumSequences(); }
	iterator begin() override { return iterator{*this, 0}; }
	iterator end() override { return iterator{*this, SndFile()->Order.GetNumSequences()}; }
	value_type operator[] (size_t index) override { return Sequence{*this, index}; }
};

}  // namespace Scripting

OPENMPT_NAMESPACE_END

namespace sol
{
using namespace OPENMPT_NAMESPACE::Scripting;

template <>
struct is_container<SequenceData> : std::true_type { };

template <>
struct usertype_container<SequenceData> : UsertypeContainer<SequenceData>
{
};

}  // namespace sol
