/*
 * ScriptPackage.h
 * ---------------
 * Purpose: Lua script package wrapper
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "Script.h"

OPENMPT_NAMESPACE_BEGIN

class IArchive;

namespace Scripting
{

class ScriptPackage : public Script
{
public:
	struct Manifest
	{
		mpt::ustring author;
		mpt::ustring description;
		mpt::ustring version;
		mpt::ustring api_version;
		std::set<mpt::ustring> permissions;
		std::optional<mpt::ustring> entry_point;
	};

	ScriptPackage(const mpt::PathString &filename);
	ScriptPackage(Script &&) = delete;
	ScriptPackage(const Script &) = delete;
	~ScriptPackage();

	sol::protected_function_result Run();

private:
	static int SearchFile(lua_State *L);

	mpt::PathString m_archiveName;
	std::unique_ptr<IArchive> m_archive;
	std::map<mpt::PathString, size_t> m_files;
	Manifest m_manifest;
};

}  // namespace Scripting

OPENMPT_NAMESPACE_END
