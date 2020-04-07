/*
 * LuaChannel.h
 * ------------
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

struct Channel : public Obj
{
	static void Register(sol::table &table)
	{
		table.new_usertype<Channel>("Channel"
			, sol::meta_function::to_string, &Name
			, sol::meta_function::type, &Type
			, "id", sol::readonly(&Channel::m_index)
			, "name", sol::property(&GetName, &SetName)
			);
	}

	Channel(const Obj &obj, size_t channel) : Obj(obj, channel) { }

	std::string Name() const { return MPT_FORMAT("[openmpt.Song.Channel object: Channel {}]")(m_index); }
	static std::string Type() { return "openmpt.Song.Channel"; }

	ScopedLock<ModChannelSettings> GetChannel()
	{
		auto sndFile = SndFile();
		if(m_index > 0 && m_index <= sndFile->GetNumChannels())
			return make_lock(sndFile->ChnSettings[m_index - 1], std::move(sndFile));
		throw "Invalid Channel";
	}
	ScopedLock<const ModChannelSettings> GetChannel() const
	{
		return const_cast<Channel *>(this)->GetChannel();
	}

	std::string GetName() const
	{
		return FromSndFileString(GetChannel()->szName);
	}
	void SetName(const std::string &name)
	{
		auto sndFile = SndFile();
		if(sndFile->GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT | MOD_TYPE_XM))
		{
			GetChannel()->szName = ToSndFileString(name);
			SetModified(sndFile, GeneralHint(static_cast<CHANNELINDEX>(m_index - 1)).Channels());
		}
	}
};

struct ChannelList : public ProxyContainer, public Obj
{
	ChannelList(const Obj &obj) noexcept : Obj(obj) { }

	size_t size() override { return SndFile()->GetNumChannels(); }
	iterator begin() override { return iterator(*this, 1); }
	iterator end() override { return iterator(*this, SndFile()->GetNumChannels() + 1); }
	value_type operator[] (size_t index) override
	{
		if(index > 0 && index <= size())
			return sol::make_object<Channel>(m_script, *this, index);
		else
			return sol::make_object(m_script, sol::nil);
	}
};

}

OPENMPT_NAMESPACE_END
