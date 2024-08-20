/*
 * LuaTuning.h
 * -----------
 * Purpose: Lua wrappers for CModDoc and contained classes
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once
#include "LuaObject.h"
#include "../../../soundlib/tuning.h"
#include "../../../soundlib/tuningcollection.h"

OPENMPT_NAMESPACE_BEGIN

namespace Scripting
{

	using NOTEINDEXTYPE = OPENMPT_NAMESPACE::Tuning::NOTEINDEXTYPE;
	using UNOTEINDEXTYPE = OPENMPT_NAMESPACE::Tuning::UNOTEINDEXTYPE;
	using USTEPINDEXTYPE = OPENMPT_NAMESPACE::Tuning::USTEPINDEXTYPE;
	using RATIOTYPE = OPENMPT_NAMESPACE::Tuning::RATIOTYPE;

#define TuningType(X) \
	X(GENERAL) \
	X(GROUPGEOMETRIC) \
	X(GEOMETRIC)

MPT_MAKE_ENUM(TuningType)
#undef TuningType

struct TuningRatioTable final : public ProxyContainer<RATIOTYPE>, public Obj
{
	TuningRatioTable(const Obj &obj, size_t tuningIndex) noexcept
		: Obj{obj, tuningIndex} {}

	bool operator==(const TuningRatioTable &other) const noexcept { return IsEqual(other); }

	static void Register(sol::table &table)
	{
		table.new_usertype<TuningRatioTable>("TuningRatioTable"
			, sol::meta_function::to_string, &TuningRatioTable::Name
			, sol::meta_function::type, &TuningRatioTable::Type
			, sol::meta_function::length, &TuningRatioTable::Length
			, sol::meta_function::index, &TuningRatioTable::Index
			, sol::meta_function::new_index, &TuningRatioTable::NewIndex
		);
	}

	std::string Name() const { return "[openmpt.Song.Tuning.RatioTable object]"; }
	static std::string Type() { return "openmpt.Song.Tuning.RatioTable"; }

	ScopedLock<const CTuning> GetTuning() const
	{
		auto sndFile = SndFile();
		auto &collection = sndFile->GetTuneSpecificTunings();
		if(m_index < collection.GetNumTunings() && collection.GetTuning(m_index) != nullptr)
			return make_lock(*collection.GetTuning(m_index), std::move(sndFile));
		throw "Invalid tuning";
	}

	size_t Length() const
	{
		auto tuning = GetTuning();
		auto range = tuning->GetNoteRange();
		return range.last - range.first + 1;
	}

	RATIOTYPE Index(int note) const
	{
		auto tuning = GetTuning();
		if(!tuning->IsValidNote(static_cast<NOTEINDEXTYPE>(note)))
			throw "Invalid note index for this tuning";
		return tuning->GetRatio(static_cast<NOTEINDEXTYPE>(note));
	}

	void NewIndex(int note, RATIOTYPE ratio)
	{
		// TODO Tuning ratios are read-only from Lua for now
		// Should only be writable for generic tunings
		throw "Tuning ratio table is read-only";
	}

	size_t size() override { return Length(); }
	iterator begin() override { return iterator{*this, 0}; }
	iterator end() override { return iterator{*this, Length()}; }
	value_type operator[](size_t index) override
	{
		auto tuning = GetTuning();
		auto range = tuning->GetNoteRange();
		auto note = mpt::saturate_cast<NOTEINDEXTYPE>(range.first + index);
		return tuning->GetRatio(note);
	}

	void erase(iterator it)
	{
		static_assert(sol::container_detail::has_erase<TuningRatioTable>::value);
		throw "Cannot erase from tuning ratio table - table size is fixed";
	}

	void insert(iterator it, const value_type &value)
	{
		static_assert(sol::meta::has_insert_with_iterator<TuningRatioTable>::value);
		throw "Cannot insert into tuning ratio table - table size is fixed";
	}
};

struct Tuning : public Obj
{
	TuningRatioTable m_ratioTable;

	static void Register(sol::table &table)
	{
		table.new_usertype<Tuning>("Tuning"
			, sol::meta_function::to_string, &Tuning::Name
			, sol::meta_function::type, &Tuning::Type

			, "name", sol::property(&Tuning::GetName, &Tuning::SetName)
			, "type", sol::property(&Tuning::GetType)
			, "group_size", sol::property(&Tuning::GetGroupSize)
			, "group_ratio", sol::property(&Tuning::GetGroupRatio)
			, "fine_step_count", sol::property(&Tuning::GetFineStepCount)
			, "note_range", sol::property(&Tuning::GetNoteRange)
			, "ratios", &Tuning::m_ratioTable

			, "get_ratio", &Tuning::GetRatio
			, "get_note_name", &Tuning::GetNoteName
			);
	}

	Tuning(const Obj &obj, size_t tuningIndex) noexcept
		: Obj{obj, tuningIndex}
		, m_ratioTable{obj, tuningIndex} {}

	bool operator==(const Tuning &other) const noexcept { return IsEqual(other); }

	std::string Name() const { return "[openmpt.Song.Tuning object]"; }
	static std::string Type() { return "openmpt.Song.Tuning"; }

	ScopedLock<CTuning> GetTuning()
	{
		auto sndFile = SndFile();
		auto &collection = sndFile->GetTuneSpecificTunings();
		if(m_index < collection.GetNumTunings() && collection.GetTuning(m_index) != nullptr)
			return make_lock(*collection.GetTuning(m_index), std::move(sndFile));
		throw "Invalid tuning";
	}
	ScopedLock<const CTuning> GetTuning() const
	{
		return const_cast<Tuning *>(this)->GetTuning();
	}

	std::string GetName() const
	{
		auto tuning = GetTuning();
		return mpt::ToCharset(mpt::Charset::UTF8, tuning->GetName());
	}
	void SetName(const std::string &name)
	{
		auto tuning = GetTuning();
		tuning->SetName(mpt::ToUnicode(mpt::Charset::UTF8, name));
		SetModified(tuning, GeneralHint().Tunings());
	}

	std::string GetType() const
	{
		switch(GetTuning()->GetType())
		{
		case OPENMPT_NAMESPACE::Tuning::Type::GENERAL: return EnumTuningType::GENERAL;
		case OPENMPT_NAMESPACE::Tuning::Type::GROUPGEOMETRIC: return EnumTuningType::GROUPGEOMETRIC;
		case OPENMPT_NAMESPACE::Tuning::Type::GEOMETRIC: return EnumTuningType::GEOMETRIC;
		}
		return EnumTuningType::GENERAL;
	}

	int GetGroupSize() const
	{
		auto tuning = GetTuning();
		return tuning->GetGroupSize();
	}

	double GetGroupRatio() const
	{
		auto tuning = GetTuning();
		return tuning->GetGroupRatio();
	}

	int GetFineStepCount() const
	{
		auto tuning = GetTuning();
		return tuning->GetFineStepCount();
	}

	sol::as_table_t<std::vector<int>> GetNoteRange() const
	{
		auto tuning = GetTuning();
		auto range = tuning->GetNoteRange();
		return sol::as_table(std::vector<int>{range.first, range.last});
	}

	double GetRatio(int note, sol::optional<int> finesteps) const
	{
		auto tuning = GetTuning();
		if(!tuning->IsValidNote(static_cast<NOTEINDEXTYPE>(note)))
			throw "Invalid note index for this tuning";
		
		if(finesteps.has_value())
			return tuning->GetRatio(static_cast<NOTEINDEXTYPE>(note), static_cast<OPENMPT_NAMESPACE::Tuning::STEPINDEXTYPE>(finesteps.value()));
		else
			return tuning->GetRatio(static_cast<NOTEINDEXTYPE>(note));
	}

	std::string GetNoteName(int note, sol::optional<bool> addOctave) const
	{
		auto tuning = GetTuning();
		if(!tuning->IsValidNote(static_cast<NOTEINDEXTYPE>(note)))
			throw "Invalid note index for this tuning";
		return mpt::ToCharset(mpt::Charset::UTF8, tuning->GetNoteName(static_cast<NOTEINDEXTYPE>(note), addOctave.value_or(true)));
	}
};

struct TuningList final : public ProxyContainer<Tuning>, public Obj
{
	static void Register(sol::table &table)
	{
		table.new_usertype<TuningList>("TuningList"
			, sol::meta_function::to_string, &TuningList::Name
			, sol::meta_function::type, &TuningList::Type
			, sol::meta_function::length, &TuningList::size
			, sol::meta_function::index, &TuningList::Index
			, "find_by_name", &TuningList::FindByName
			, "create_general", &TuningList::CreateGeneral
			, "create_group_geometric", &TuningList::CreateGroupGeometric
			, "create_geometric", &TuningList::CreateGeometric
		);
	}

	TuningList(const Obj &obj) noexcept
		: Obj{obj} {}

	bool operator==(const TuningList &other) const noexcept { return IsEqual(other); }

	std::string Name() const { return "[openmpt.Song.TuningList object]"; }
	static std::string Type() { return "openmpt.Song.TuningList"; }

	ScopedLock<OPENMPT_NAMESPACE::Tuning::CTuningCollection> GetTuningCollection()
	{
		auto sndFile = SndFile();
		return make_lock(sndFile->GetTuneSpecificTunings(), std::move(sndFile));
	}

	ScopedLock<const OPENMPT_NAMESPACE::Tuning::CTuningCollection> GetTuningCollection() const
	{
		auto sndFile = SndFile();
		return make_lock(sndFile->GetTuneSpecificTunings(), std::move(sndFile));
	}

	Tuning Index(size_t index)
	{
		return Tuning{*this, index};
	}

	size_t size() override
	{
		return GetTuningCollection()->GetNumTunings();
	}
	
	iterator begin() override { return iterator{*this, 0}; }
	iterator end() override { return iterator{*this, size()}; }
	
	value_type operator[](size_t index) override
	{
		return Tuning{*this, index};
	}

	sol::optional<Tuning> FindByName(const std::string &name) const
	{
		auto collection = GetTuningCollection();
		const CTuning *tuning = collection->GetTuning(mpt::ToUnicode(mpt::Charset::UTF8, name));
		if(!tuning)
			return {};
		
		// TODO
		for(size_t i = 0; i < collection->GetNumTunings(); i++)
		{
			if(collection->GetTuning(i) == tuning)
				return Tuning{*const_cast<TuningList*>(this), i};
		}
		return {};
	}

	sol::optional<Tuning> CreateGeneral(const std::string &name, sol::optional<sol::table> ratios)
	{
		auto sndFile = SndFile();
		if(sndFile->GetType() != MOD_TYPE_MPT)
			throw "Tunings can only be added to MPTM format songs";
		
		auto tuning = CTuning::CreateGeneral(mpt::ToUnicode(mpt::Charset::UTF8, name));
		if(!tuning)
			return {};
		
		auto collection = GetTuningCollection();
		CTuning *addedTuning = collection->AddTuning(std::move(tuning));
		if(!addedTuning)
			return {};

		if(ratios)
		{
			const sol::table &ratioTable = ratios.value();
			const size_t numNotes = std::min(static_cast<size_t>(addedTuning->GetGroupSize()), ratioTable.size());
			for(size_t i = 0; i < numNotes; i++)
			{
				addedTuning->SetRatio(static_cast<NOTEINDEXTYPE>(i), ratioTable[i + 1].get_or(RATIOTYPE(1.0)));
			}
		}

		SetModified(collection, GeneralHint().Tunings());
		return Tuning{*this, collection->GetNumTunings() - 1};
	}

	sol::optional<Tuning> CreateGroupGeometric(const std::string &name, int groupSize, double groupRatio, int fineStepCount, sol::optional<sol::table> ratios)
	{
		auto sndFile = SndFile();
		if(sndFile->GetType() != MOD_TYPE_MPT)
			throw "Tunings can only be added to MPTM format songs";
		
		if(groupRatio <= 0.0 || fineStepCount < 0)
			return {};
		
		std::unique_ptr<CTuning> tuning;
		
		// TODO: Use two overloads instead?
		if(ratios.has_value())
		{
			// Create with custom ratios table
			const sol::table &ratioTable = ratios.value();
			if(ratioTable.empty())
				return {};
			
			std::vector<RATIOTYPE> ratioVec;
			ratioVec.resize(ratioTable.size());
			for(size_t i = 0; i < ratioTable.size(); i++)
			{
				ratioVec[i] = ratioTable[i + 1].get_or(RATIOTYPE(1.0));
			}
			
			tuning = CTuning::CreateGroupGeometric(
				mpt::ToUnicode(mpt::Charset::UTF8, name),
				std::move(ratioVec),
				static_cast<RATIOTYPE>(groupRatio),
				static_cast<USTEPINDEXTYPE>(fineStepCount));
		} else
		{
			// Create with group size
			if(groupSize <= 0)
				return {};
			
			tuning = CTuning::CreateGroupGeometric(
				mpt::ToUnicode(mpt::Charset::UTF8, name),
				static_cast<UNOTEINDEXTYPE>(groupSize),
				static_cast<RATIOTYPE>(groupRatio),
				static_cast<USTEPINDEXTYPE>(fineStepCount));
		}
		
		if(!tuning)
			return {};
		
		auto collection = GetTuningCollection();
		const CTuning *addedTuning = collection->AddTuning(std::move(tuning));
		if(!addedTuning)
			return {};
		
		SetModified(collection, GeneralHint().Tunings());
		return Tuning{*this, collection->GetNumTunings() - 1};
	}

	sol::optional<Tuning> CreateGeometric(const std::string &name, int groupSize, double groupRatio, int fineStepCount)
	{
		auto sndFile = SndFile();
		if(sndFile->GetType() != MOD_TYPE_MPT)
			throw "Tunings can only be added to MPTM format songs";
		
		if(groupSize <= 0 || groupRatio <= 0.0 || fineStepCount < 0)
			return {};
		
		auto tuning = CTuning::CreateGeometric(
			mpt::ToUnicode(mpt::Charset::UTF8, name),
			static_cast<UNOTEINDEXTYPE>(groupSize),
			static_cast<RATIOTYPE>(groupRatio),
			static_cast<USTEPINDEXTYPE>(fineStepCount));
		if(!tuning)
			return {};
		
		auto collection = GetTuningCollection();
		const CTuning *addedTuning = collection->AddTuning(std::move(tuning));
		if(!addedTuning)
			return {};
		
		SetModified(collection, GeneralHint().Tunings());
		return Tuning{*this, collection->GetNumTunings() - 1};
	}

	void erase(iterator it)
	{
		static_assert(sol::container_detail::has_erase<TuningList>::value);
		auto collection = GetTuningCollection();
		if(it.m_index < collection->GetNumTunings())
		{
			collection->Remove(it.m_index);
			SetModified(collection, GeneralHint().Tunings());
		}
	}

	void insert(iterator it, const Tuning &srcTuning)
	{
		throw "Tunings must be added through dedicated creation functions";
#if 0
		static_assert(sol::meta::has_insert_with_iterator<TuningList>::value);
		auto sndFile = SndFile();
		if(sndFile->GetType() != MOD_TYPE_MPT)
			throw "Tunings can only be added to MPTM format songs";
		
		auto collection = GetTuningCollection();
		if(collection->GetNumTunings() >= OPENMPT_NAMESPACE::Tuning::CTuningCollection::s_nMaxTuningCount)
			throw "Cannot add any more tunings";
		
		// TODO: Create real copy of provided tuning
		// TODO this will invalidate any other Lua tuning objects
		auto tuning = CTuning::CreateGeneral(srcTuning.GetTuning()->GetName());
		if(!tuning)
			throw "Failed to create tuning";
		
		const CTuning *addedTuning = collection->AddTuning(std::move(tuning));
		if(!addedTuning)
			throw "Failed to add tuning to collection";
		
		SetModified(collection, GeneralHint().Tunings());
#endif
	}
};

}  // namespace Scripting

OPENMPT_NAMESPACE_END

namespace sol
{
using namespace OPENMPT_NAMESPACE::Scripting;

template <>
struct is_container<TuningRatioTable> : std::true_type { };

template <>
struct usertype_container<TuningRatioTable> : UsertypeContainer<TuningRatioTable> { };

template <>
struct is_container<TuningList> : std::true_type { };

template <>
struct usertype_container<TuningList> : UsertypeContainer<TuningList> { };

}  // namespace sol
