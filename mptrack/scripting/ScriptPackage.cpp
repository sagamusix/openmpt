/*
 * ScriptPackage.cpp
 * -----------------
 * Purpose: Lua script package wrapper
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "stdafx.h"
#include "ScriptPackage.h"
#include "../../unarchiver/unzip.h"

#include "mpt/json/json.hpp"


OPENMPT_NAMESPACE_BEGIN

namespace Scripting
{

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ScriptPackage::Manifest, author, description, version, api_version, permissions, entry_point)


ScriptPackage::ScriptPackage(const mpt::PathString &filename)
	: m_archiveName{filename.GetFilename()}
{
	mpt::IO::InputFile file(filename);
	if(!file.IsValid())
		throw std::runtime_error{"Cannot open script archive"};
	
	FileReader f = GetFileReader(file);
	m_archive = std::make_unique<CZipArchive>(f);
	if(!m_archive->IsArchive())
		throw std::runtime_error{"File is not a script archive"};
	
	size_t i = 0;
	for(const auto &entry : *m_archive)
	{
		m_files[entry.name] = i++;
	}
	auto manifestFile = m_files.find(P_("OpenMPT.manifest"));
	if(manifestFile == m_files.end())
		throw std::runtime_error{"File is not a script archive"};
	if(!m_archive->ExtractFile(manifestFile->second))
		throw std::runtime_error{"Cannot extract script manifest"};

	try
	{
		m_manifest = nlohmann::json::parse(mpt::buffer_cast<std::string>(m_archive->GetOutputFile().ReadRawDataAsByteVector())).get<Manifest>();
	} catch(const nlohmann::json::exception &e)
	{
		throw std::runtime_error{std::string{"Invalid script manifest: "} + e.what()};
	}

	lua.add_package_loader(&ScriptPackage::SearchFile);
}


ScriptPackage::~ScriptPackage() { }


sol::protected_function_result ScriptPackage::Run()
{
	m_environment = sol::environment(lua, sol::create, lua.globals());
	
	const auto filename = mpt::PathString::FromUnicode(m_manifest.entry_point.value_or(U_("main.lua")));
	auto file = m_files.find(filename);
	if(file == m_files.end())
		throw std::runtime_error{"Cannot find source file: " + filename.ToUTF8()};
	if(!m_archive->ExtractFile(file->second))
		throw std::runtime_error{"Cannot extract source file: " + filename.ToUTF8()};

	const auto source = mpt::buffer_cast<std::string>(m_archive->GetOutputFile().ReadRawDataAsByteVector());

	return lua.do_file(filename.ToUTF8(), m_environment);
}


int ScriptPackage::SearchFile(lua_State *L)
{
	auto instance = static_cast<ScriptPackage *>(LuaVM::GetInstance(L));
	const auto name = sol::stack::get<std::string>(L);

	auto file = instance->m_files.find(mpt::PathString::FromUTF8(name));
	if(file == instance->m_files.end() || !instance->m_archive->ExtractFile(file->second))
	{
		// Not found
		return 1;
	}

	std::string filename = "@" + instance->m_archiveName.ToUTF8() + "/" + name;  // Prefix with @ for proper stacktrace formatting
	std::string moduleContent = mpt::buffer_cast<std::string>(instance->m_archive->GetOutputFile().ReadRawDataAsByteVector());
	if(luaL_loadbuffer(L, moduleContent.c_str(), moduleContent.size(), filename.c_str()) == LUA_OK)
	{
		lua_pushstring(L, filename.c_str());
		return 2;  // Return open function and file name
	} else
	{
		return luaL_error(L, "error loading module '%s' from file '%s':\n\t%s", lua_tostring(L, 1), filename.c_str(), lua_tostring(L, -1));
	}
}


}  // namespace Scripting

OPENMPT_NAMESPACE_END
