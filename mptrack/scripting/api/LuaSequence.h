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

namespace Scripting
{

struct Sequence : public ProxyContainer, public Obj
{
	static void Register(sol::table &table)
	{
		table.new_usertype<Sequence>("Sequence"
			, sol::meta_function::to_string, &Name
			, sol::meta_function::type, &Type
			, sol::meta_function::length, &Length
			, sol::meta_function::index, &Index
			, sol::meta_function::new_index, &NewIndex
			, "id", sol::property(&GetSequenceIndexLua)
			, "name", sol::property(&GetName, &SetName)
			//, "get"
			//, "set"
			//, "erase"
			//, "insert"
			);
	}

	Sequence(const Obj &obj, size_t sequence) noexcept : Obj(obj, sequence) { }

	std::string Name() const { return MPT_FORMAT("[openmpt.Song.Sequence object: Sequence {}]")(GetSequenceIndexLua()); }
	static std::string Type() { return "openmpt.Song.Sequence"; }

	SEQUENCEINDEX GetSequenceIndex()
	{
		return (m_index == SEQUENCEINDEX_INVALID) ? SndFile()->Order.GetCurrentSequenceIndex() : static_cast<SEQUENCEINDEX>(m_index);
	}
	SEQUENCEINDEX GetSequenceIndex() const
	{
		return const_cast<Sequence *>(this)->GetSequenceIndex();
	}
	SEQUENCEINDEX GetSequenceIndexLua() const
	{
		return GetSequenceIndex() + 1;
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
		return const_cast<Sequence *>(this)->GetSequence();
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

	ORDERINDEX Length() const
	{
		return GetSequence()->GetLength();
	}

	PATTERNINDEX Index(ORDERINDEX position)
	{
		auto sequence = GetSequence();
		if(position >= MAX_ORDERS)
			throw "Invalid sequence position!";
		if(position >= sequence->GetLength())
			return PATTERNINDEX_INVALID; // TODO nil?
		return sequence->at(position);
	}

	void NewIndex(ORDERINDEX position, PATTERNINDEX value)
	{
		auto sequence = GetSequence();
		if(position >= MAX_ORDERS)
			throw "Invalid sequence position!";
		if(value >= MAX_PATTERNS && value != sequence->GetIgnoreIndex() && value != sequence->GetInvalidPatIndex())
			throw "Invalid pattern index!";
		if(position >= sequence->GetLength())
			sequence->resize(position + 1);
		sequence->at(position) = value;
		SetModified(SndFile(), SequenceHint(static_cast<SEQUENCEINDEX>(GetSequenceIndex())).Data());
	}

	size_t size() override { return GetSequence()->size(); }
	iterator begin() override { return iterator(*this, 0); }
	iterator end() override { return iterator(*this, GetSequence()->size()); }
	value_type operator[] (size_t index) override
	{
		auto sequence = GetSequence();
		if(index < sequence->size())
			return sol::make_object(m_script, sequence->at(index));	// TODO map --- to nil, +++ to whatever?
		else
			return sol::make_object(m_script, sol::nil);
	}
};

struct SequenceList : public ProxyContainer, public Obj
{
	SequenceList(const Obj &obj) noexcept : Obj(obj) { }

	size_t size() override { return SndFile()->Order.GetNumSequences(); }
	iterator begin() override { return iterator(*this, 0); }
	iterator end() override { return iterator(*this, SndFile()->Order.GetNumSequences()); }
	value_type operator[] (size_t index) override
	{
		if(index < size())
			return sol::make_object<Sequence>(m_script, *this, index);
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
struct is_container<Sequence> : std::true_type { };

template <>
struct usertype_container<Sequence>
{
	static int set(lua_State *L)
	{
		auto &self = sol::stack::get<Sequence &>(L, 1);
		const auto key = sol::stack::get<int>(L, 2);
		const auto value = sol::stack::get<int>(L, 3);
		self.NewIndex(static_cast<ORDERINDEX>(key), static_cast<PATTERNINDEX>(value));
		return 0;
	}
	static int size(lua_State *L)
	{
		auto &self = sol::stack::get<Sequence &>(L, 1);
		return sol::stack::push(L, self.size());
	}
	static auto begin(lua_State *, Sequence &that)
	{
		return that.begin();
	}
	static auto end(lua_State *, Sequence &that)
	{
		return that.end();
	}

	// Make the order list 0-based
	static std::ptrdiff_t index_adjustment(lua_State *, Sequence &)
	{
		return 0;
	}
};

}  // namespace sol
