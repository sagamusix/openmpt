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

			, "index", sol::readonly(&Channel::m_index)
			, "name", sol::property(&GetName, &SetName)
			, "volume", sol::property(&GetVolume, &SetVolume)
			, "panning", sol::property(&GetPanning, &SetPanning)
			, "surround", sol::property(&GetSurround, &SetSurround)
			, "plugin", sol::property(&GetPlugin, &SetPlugin)
			, "color", sol::property(&GetColor, &SetColor)

			, "solo", &Channel::Solo
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

	sol::optional<std::vector<float>> GetColor() const
	{
		auto color = GetChannel()->color;
		if(color == ModChannelSettings::INVALID_COLOR)
			return {};
		else
			return std::vector<float>{GetRValue(color) / 255.0f, GetGValue(color) / 255.0f, GetBValue(color) / 255.0f};
	}
	void SetColor(sol::object color)
	{
		auto channel = GetChannel();
		if(color.is<sol::table>())
		{
			sol::table colorTable = color;
			if(colorTable.size() != 3)
				throw "Invalid table size (must be 3)";
			channel->color = RGB(mpt::saturate_round<uint8>(colorTable[0].get<float>() * 255.0f),
				mpt::saturate_round<uint8>(colorTable[1].get<float>() * 255.0f),
				mpt::saturate_round<uint8>(colorTable[2].get<float>() * 255.0f));
		} else if(!color.valid())
		{
			channel->color = ModChannelSettings::INVALID_COLOR;
		} else
		{
			throw "Invalid type (must be table or nil)";
		}
		SetModified(channel, GeneralHint(static_cast<CHANNELINDEX>(m_index - 1)).Channels());
	}

	void Solo()
	{
		Script::CallInGUIThread([this]()
		{
			auto modDoc = ModDoc();
			const CHANNELINDEX numChannels = modDoc->GetNumChannels();
			const CHANNELINDEX soloChannel = static_cast<CHANNELINDEX>(m_index - 1);
			for(CHANNELINDEX chn = 0; chn < numChannels; chn++)
			{
				modDoc->MuteChannel(chn, chn != soloChannel);
			}
		});
	}
};

struct ChannelList final : public ProxyContainer<Channel>, public Obj
{
	static void Register(sol::table &table)
	{
		table.new_usertype<ChannelList>("ChannelList"
			, sol::meta_function::to_string, &ChannelList::Name
			, sol::meta_function::type, &ChannelList::Type
			, sol::meta_function::length, &ChannelList::size
			, sol::meta_function::index, &ChannelList::Index
			, "rearrange", &ChannelList::RearrangeChannels
		);
	}

	ChannelList(const Obj &obj) noexcept : Obj{obj} { }

	std::string Name() const { return "[openmpt.Song.ChannelList object]"; }
	static std::string Type() { return "openmpt.Song.ChannelList"; }

	Channel Index(size_t index) { return Channel{*this, index}; }

	size_t size() override { return SndFile()->GetNumChannels(); }
	iterator begin() override { return iterator{*this, 1}; }
	iterator end() override { return iterator{*this, SndFile()->GetNumChannels() + 1u}; }
	value_type operator[] (size_t index) override { return Channel{*this, index}; }

	bool RearrangeChannels(const sol::table &newOrder)
	{
		auto modDoc = ModDoc();

		std::vector<CHANNELINDEX> channelOrder;
		channelOrder.reserve(newOrder.size());
		for(size_t i = 1; i <= newOrder.size(); i++)
		{
			sol::object entry = newOrder[i];
			if(!entry.valid())
			{
				channelOrder.push_back(CHANNELINDEX_INVALID);
			} else if(entry.is<int>())
			{
				const int channelIndex = entry.as<int>();
				if(channelIndex >= 1 && channelIndex <= modDoc->GetNumChannels())
					channelOrder.push_back(static_cast<CHANNELINDEX>(channelIndex - 1));
				else
					throw MPT_AFORMAT("Invalid channel index: {}")(channelIndex);
			} else
			{
				throw "Channel order table must contain integers or nil";
			}
		}

		bool result = modDoc->ReArrangeChannels(channelOrder, true) != CHANNELINDEX_INVALID;
		if(result)
			SetModified(GeneralHint().ModType());
		return result;
	}
};

}  // namespace Scripting

OPENMPT_NAMESPACE_END
