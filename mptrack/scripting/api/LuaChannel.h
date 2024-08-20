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
#include "LuaPlugin.h"

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
			, "volume", sol::property(&GetVolume, &SetVolume)
			, "panning", sol::property(&GetPanning, &SetPanning)
			, "surround", sol::property(&GetSurround, &SetSurround)
			, "plugin", sol::property(&GetPlugin, &SetPlugin)
			);
	}

	Channel(const Obj &obj, size_t channel) : Obj{obj, channel} { }

	bool operator==(const Channel &other) const noexcept { return IsEqual(other); }

	Channel &operator=(const Channel &other)
	{
		if(*this == other)
			return *this;
		auto chn = GetChannel();
		auto otherChn = other.GetChannel();

		*chn = *otherChn;
		SetModified(chn, GeneralHint(static_cast<CHANNELINDEX>(m_index - 1)).Channels());
		return *this;
	}

	std::string Name() const { return MPT_AFORMAT("[openmpt.Song.Channel object: Channel {}]")(m_index); }
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

	double GetVolume() const
	{
		return GetChannel()->nVolume / 64.0;
	}
	void SetVolume(double volume)
	{
		auto sndFile = SndFile();
		if(sndFile->GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT))
		{
			GetChannel()->nVolume = std::min(mpt::saturate_round<uint8>(volume * 64.0), uint8(64));
			SetModified(sndFile, GeneralHint(static_cast<CHANNELINDEX>(m_index - 1)).Channels());
		} else
		{
			throw "Channel volume is not available in this format";
		}
	}

	double GetPanning() const
	{
		return GetChannel()->nPan / 256.0;
	}
	void SetPanning(double panning)
	{
		auto sndFile = SndFile();
		if(sndFile->GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT | MOD_TYPE_S3M))
		{
			GetChannel()->nPan = std::min(mpt::saturate_round<uint16>(panning * 256.0), uint16(256));
			SetModified(sndFile, GeneralHint(static_cast<CHANNELINDEX>(m_index - 1)).Channels());
		} else
		{
			throw "Channel panning is not available in this format";
		}
	}

	bool GetSurround() const
	{
		return GetChannel()->dwFlags[CHN_SURROUND];
	}
	void SetSurround(bool surround)
	{
		auto sndFile = SndFile();
		if(sndFile->GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT))
		{
			GetChannel()->dwFlags.set(CHN_SURROUND, surround);
			if(surround)
				GetChannel()->nPan = 128;
			SetModified(sndFile, GeneralHint(static_cast<CHANNELINDEX>(m_index - 1)).Channels());
		} else
		{
			throw "Channel surround is not available in this format";
		}
	}

	sol::optional<Plugin> GetPlugin() const
	{
		auto plugin = GetChannel()->nMixPlugin;
		if(!plugin)
			return {};
		else
			return Plugin{*const_cast<Channel *>(this), plugin - 1u};
	}
	void SetPlugin(sol::object plugin)
	{
		auto channel = GetChannel();
		channel->nMixPlugin = Plugin::IndexFromObject(plugin, SndFile());
		SetModified(channel, PluginHint(static_cast<PLUGINDEX>(m_index)).Info());
	}
};

struct ChannelList final : public ProxyContainer<Channel>, public Obj
{
	ChannelList(const Obj &obj) noexcept : Obj{obj} { }

	size_t size() override { return SndFile()->GetNumChannels(); }
	iterator begin() override { return iterator{*this, 1}; }
	iterator end() override { return iterator{*this, SndFile()->GetNumChannels() + 1u}; }
	value_type operator[] (size_t index) override { return Channel{*this, index}; }
};

}  // namespace Scripting

OPENMPT_NAMESPACE_END
