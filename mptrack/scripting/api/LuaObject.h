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
// Call MPT_MAKE_ENUM(Foo) to create namespace EnumFoo from the contents of an X Macro called Foo.
// The corresponding Lua enum created by Register() will be called "foo".
#define MPT_MAKE_ENUM(name) \
	namespace Enum##name \
	{ \
		enum values { name(MPT_X_ENUM) }; \
		constexpr size_t size = std::size({ name(MPT_X_ENUM) }); \
		/*constexpr char *strings[] = { name(MPT_X_STR) };*/ \
		void Register(sol::table &table) \
		{ \
			char luaName[] = #name; \
			if(luaName[0] >= 'A' && luaName[0] <= 'Z') luaName[0] += 'a' - 'A'; \
			table.new_enum<values>(luaName/*, sol::meta_function::to_string, []() { return "[Enum " #name "]"; }*/, { name(MPT_X_STR_ENUM) } ); \
		} \
	};
#define MPT_X_ENUM(a) a,
#define MPT_X_STR(a) #a,
#define MPT_X_STR_ENUM(a) { #a, a },

namespace Scripting
{

class Script;

struct Obj
{
	Script &m_script;
	CModDoc &m_modDoc;
	const size_t m_index;  // optional, for indexed objects (e.g. a sample)

	Obj(Script &script, CModDoc &modDoc, size_t index = 0) noexcept
		: m_script(script)
		, m_modDoc(modDoc)
		, m_index(index)
	{ }

	Obj(const Obj &obj, size_t index) noexcept
		: m_script(obj.m_script)
		, m_modDoc(obj.m_modDoc)
		, m_index(index)
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
