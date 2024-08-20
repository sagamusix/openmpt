/*
 * LuaObject.h
 * -----------
 * Purpose: Lua wrappers for CModDoc and contained classes
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include <sol/sol.hpp>

#include "ProxyContainer.h"
#include "ScopedLock.h"

#include "../ScriptManager.h"
#include "../../Mainfrm.h"
#include "../../Moddoc.h"
#include "../../Mptrack.h"
#include "../../WindowMessages.h"
#include "../../../soundlib/mod_specifications.h"
#include "../../../soundlib/modsmp_ctrl.h"
#include "../../../soundlib/plugins/PlugInterface.h"
#include "../../../common/mptStringBuffer.h"


OPENMPT_NAMESPACE_BEGIN

// Create an enum class containing both the enum values and their string names in an array.
// Call MPT_MAKE_ENUM(FooBar) to create namespace EnumFooBar from the contents of an X Macro called FooBar.
// The corresponding Lua enum created by Register() will be called "fooBar".
#define MPT_MAKE_ENUM(name) \
	namespace Enum##name \
	{ \
		name(MPT_X_STR_CONSTANTS) \
		enum class Enum { name(MPT_X_ENUM) INVALID_VALUE }; \
		static constexpr const char* strings[] = { name(MPT_X_ENUM) }; \
		static constexpr Enum Parse(std::string_view value) noexcept \
		{ \
			for(size_t i = 0; i < std::size(strings); i++) \
			{ \
				if(strings[i] == value) \
					return static_cast<Enum>(i); \
			} \
			return Enum::INVALID_VALUE; \
		} \
		static constexpr bool IsValid(std::string_view value) noexcept \
		{ \
			return Parse(value) != Enum::INVALID_VALUE; \
		} \
		void Register(sol::table &table) \
		{ \
			sol::table target = table.create(static_cast<int>(std::size(strings)), 0); \
			for(const auto &str : strings) \
				target.set(str, str); \
			sol::table meta = table.create_with( \
				sol::meta_function::new_index, sol::detail::fail_on_newindex, \
				sol::meta_function::index, target, \
				sol::meta_function::pairs, sol::stack::stack_detail::readonly_pairs, \
				sol::meta_function::length, [](){ return std::size(strings); }); \
			table.create_named(#name, sol::metatable_key, meta); \
		} \
	}
#define MPT_X_ENUM(a) a,
#define MPT_X_STR(a) #a,
#define MPT_X_STR_ENUM(a) { #a, a },
#define MPT_X_STR_CONSTANTS(a) static constexpr char a[] = #a;

namespace Scripting
{

class Script;

struct Obj
{
	Script &m_script;
	CModDoc &m_modDoc;
	const size_t m_index;  // optional, for indexed objects (e.g. a sample)

	Obj(Script &script, CModDoc &modDoc, size_t index = 0) noexcept
		: m_script{script}
		, m_modDoc{modDoc}
		, m_index{index}
	{ }

	Obj(const Obj &obj, size_t index) noexcept
		: m_script{obj.m_script}
		, m_modDoc{obj.m_modDoc}
		, m_index{index}
	{ }

	Obj(const Obj &) noexcept = default;
	Obj(Obj &&) noexcept = default;
	Obj& operator= (const Obj &other) = delete;
	Obj& operator= (Obj &&other) = delete;

	bool IsEqual(const Obj &other) const noexcept { return &m_modDoc == &other.m_modDoc && m_index == other.m_index; }

	void SetModified(UpdateHint hint)
	{
		auto modDoc = ModDoc();
		auto *mainFrm = CMainFrame::GetMainFrame();
		modDoc->SetModified();
		mainFrm->PostMessage(WM_MOD_UPDATEVIEWS, reinterpret_cast<WPARAM>(static_cast<CModDoc *>(modDoc)), hint.AsLPARAM());
	}

	void SetModified(const CriticalSection &, UpdateHint hint)
	{
		// Parameter indicates that the file is already locked and no new lock needs to be obtained
		auto *mainFrm = CMainFrame::GetMainFrame();
		m_modDoc.SetModified();
		mainFrm->PostMessage(WM_MOD_UPDATEVIEWS, reinterpret_cast<WPARAM>(&m_modDoc), hint.AsLPARAM());
	}

	template<typename Tlock>
	static std::string FromSndFileString(const std::string &s, const ScopedLock<Tlock> &sndFile)
	{
		return mpt::ToCharset(mpt::Charset::UTF8, sndFile->GetCharsetInternal(), s);
	}

	template<typename Tlock>
	static std::string ToSndFileString(const std::string &s, const ScopedLock<Tlock> &sndFile)
	{
		return mpt::ToCharset(sndFile->GetCharsetInternal(), mpt::Charset::UTF8, s);
	}

	std::string FromSndFileString(const std::string &s) const
	{
		return FromSndFileString(s, SndFile());
	}

	std::string ToSndFileString(const std::string &s) const
	{
		return ToSndFileString(s, SndFile());
	}

	CModDocLock ModDoc()
	{
		CriticalSection cs;
		if(Manager::DocumentExists(m_modDoc))
			return make_lock(m_modDoc, std::move(cs));
		throw "Song has been closed!";
	}

	CModDocLockConst ModDoc() const
	{
		return const_cast<Obj *>(this)->ModDoc();
	}

	CSoundFileLock SndFile() { return ModDoc(); }
	CSoundFileLockConst SndFile() const { return ModDoc(); }

	operator CModDocLock () { return ModDoc(); }
	operator CModDocLockConst () const { return ModDoc(); }

	operator CSoundFileLock () { return ModDoc(); }
	operator CSoundFileLockConst () const { return ModDoc(); }
};

}  // namespace Scripting

OPENMPT_NAMESPACE_END
