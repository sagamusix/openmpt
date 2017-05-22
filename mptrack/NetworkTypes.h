/*
 * NetworkTypes.h
 * --------------
 * Purpose: Collaborative Composing implementation
 * Notes  : (currently none)
 * Authors: Johannes Schultz
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include <cereal/cereal.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/archives/binary.hpp>

OPENMPT_NAMESPACE_BEGIN

namespace Networking
{

struct DocumentInfo
{
	std::string name;
	int64 id;
	int32 collaborators;
	int32 maxCollaboratos;
	int32 spectators;
	int32 maxSpectators;
	bool password;

	template<class Archive>
	void serialize(Archive &archive)
	{
		archive(name, id, collaborators, maxCollaboratos, spectators, maxSpectators, password);
	}
};


struct WelcomeMsg
{
	std::string version;
	std::vector<DocumentInfo> documents;

	template<class Archive>
	void serialize(Archive &archive)
	{
		archive(version, documents);
	}
};


/*template<class Archive>
void serialize(Archive &archive, ModCommand &m)
{
	archive(m.note, m.instr, m.volcmd, m.vol, m.command, m.param);
}*/

}

OPENMPT_NAMESPACE_END
