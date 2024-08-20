/*
 * LuaPlugin.h
 * -----------
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

struct PluginParameter : public Obj
{
	const PlugParamIndex m_paramIndex;

	static void Register(sol::table &table)
	{
		table.new_usertype<PluginParameter>("PluginParameter"
			, sol::meta_function::to_string, &Name
			, sol::meta_function::type, &Type
			, "id", sol::readonly(&PluginParameter::m_index)
			, "name", sol::property(&GetName)
			, "value", sol::property(&GetValue, &SetValue)
			);
	}

	PluginParameter(const Obj &obj, size_t plugin, size_t param) noexcept : Obj{obj, plugin}, m_paramIndex{static_cast<PlugParamIndex>(param)} { }

	bool operator==(const PluginParameter &other) const noexcept { return IsEqual(other) && m_paramIndex == other.m_paramIndex; }

	PluginParameter &operator=(const PluginParameter &other)
	{
		if(*this == other)
			return *this;
		SetValue(other.GetValue());
		return *this;
	}

	std::string Name() const { return MPT_AFORMAT("[openmpt.Song.PluginParameter object: Parameter {} of Plugin Slot {}]")(m_paramIndex, m_index + 1); }
	static std::string Type() { return "openmpt.Song.PluginParameter"; }

	ScopedLock<IMixPlugin> GetPlugin()
	{
		auto sndFile = SndFile();
		if(m_index <= MAX_MIXPLUGINS && sndFile->m_MixPlugins[m_index].pMixPlugin != nullptr)
			return make_lock(*sndFile->m_MixPlugins[m_index].pMixPlugin, std::move(sndFile));
		throw "No plugin loaded into this slot";
	}
	ScopedLock<const IMixPlugin> GetPlugin() const
	{
		return const_cast<PluginParameter *>(this)->GetPlugin();
	}

	std::string GetName() const
	{
		return mpt::ToCharset(mpt::Charset::UTF8, const_cast<PluginParameter *>(this)->GetPlugin()->GetParamName(m_paramIndex));
	}

	PlugParamValue GetValue() const
	{
		return const_cast<PluginParameter *>(this)->GetPlugin()->GetParameter(m_paramIndex);
	}
	void SetValue(PlugParamValue value)
	{
		auto plugin = GetPlugin();
		plugin->SetParameter(m_paramIndex, value);
		SetModified(plugin, PluginHint(static_cast<PLUGINDEX>(m_index)).Parameter());
	}
};

struct PluginParamList final : public ProxyContainer<PluginParameter, 0>, public Obj
{
	PluginParamList(const Obj &obj, size_t plugin) noexcept : Obj{obj, plugin} { }

	size_t GetNumParameters()
	{
		auto sndFile = SndFile();
		if(m_index < MAX_MIXPLUGINS && sndFile->m_MixPlugins[m_index].pMixPlugin != nullptr)
			return sndFile->m_MixPlugins[m_index].pMixPlugin->GetNumParameters();
		else
			return 0;
	}

	size_t size() override { return GetNumParameters(); }
	iterator begin() override { return iterator{*this, 0}; }
	iterator end() override { return iterator{*this, GetNumParameters()}; }
	value_type operator[] (size_t index) override { return PluginParameter{*this, m_index, index}; }
};

struct Plugin : public Obj
{
	PluginParamList Parameters;

	static void Register(sol::table &table)
	{
		table.new_usertype<Plugin>("Plugin"
			, sol::meta_function::to_string, &Name
			, sol::meta_function::type, &Type
			//, "create"
			//, "load_preset"
			//, "save_preset"
			, "id", sol::property(&GetSlot)
			, "name", sol::property(&GetName, &SetName)
			, "library_name", sol::property(&GetLibraryName)
			, "is_loaded", sol::property(&IsLoaded)
			, "parameters", &Plugin::Parameters
			//, "toggle_editor"
			, "master", sol::property(&GetMaster, &SetMaster)
			, "dry_mix", sol::property(&GetDryMix, &SetDryMix)
			, "expanded_mix", sol::property(&GetExpandedMix, &SetExpandedMix)
			, "auto_suspend", sol::property(&GetAutoSuspend, &SetAutoSuspend)
			, "bypass", sol::property(&GetBypass, &SetBypass)
			, "dry_wet_ratio", sol::property(&GetDryWet, &SetDryWet)
			//, "mixmode"
			, "gain", sol::property(&GetGain, &SetGain)
			, "output", sol::property(&GetOutput, &SetOutput)
			//, "midi_command"
			);
	}

	Plugin(const Obj &obj, size_t plugin) noexcept : Obj{obj, plugin}, Parameters{obj, plugin} { }

	bool operator==(const Plugin &other) const noexcept { return IsEqual(other); }

	std::string Name() const { return MPT_AFORMAT("[openmpt.Song.Plugin object: Slot {}]")(m_index + 1); }
	static std::string Type() { return "openmpt.Song.Plugin"; }

	size_t GetSlot() const { return m_index + 1; }

	ScopedLock<SNDMIXPLUGIN> GetPlugin()
	{
		auto sndFile = SndFile();
		if(m_index < MAX_MIXPLUGINS)
			return make_lock(sndFile->m_MixPlugins[m_index], std::move(sndFile));
		throw "Invalid Plugin";
	}
	ScopedLock<const SNDMIXPLUGIN> GetPlugin() const
	{
		return const_cast<Plugin *>(this)->GetPlugin();
	}

	static PLUGINDEX IndexFromObject(sol::object plugin, const CSoundFileLock &sndFile)
	{
		if(plugin.is<int>())
		{
			int i = plugin.as<int>();
			if(i > 0 && i <= MAX_MIXPLUGINS)
				return static_cast<PLUGINDEX>(i);
			else
				return 0;
		} else if(plugin.is<Plugin>())
		{
			PLUGINDEX i = static_cast<PLUGINDEX>(plugin.as<Plugin>().m_index);
			if(i < MAX_MIXPLUGINS && sndFile->m_MixPlugins[i].pMixPlugin != nullptr)
				return i + 1u;
			else
				return 0;
		} else if(!plugin.valid())
		{
			return 0;
		} else
		{
			throw "Invalid parameter type (index or plugin object expected)";
		}
	}

	std::string GetName() const
	{
		return FromSndFileString(GetPlugin()->Info.szName);
	}
	void SetName(const std::string &name)
	{
		auto sndFile = SndFile();
		GetPlugin()->Info.szName = ToSndFileString(name, sndFile);
		SetModified(sndFile, PluginHint(static_cast<PLUGINDEX>(m_index)).Names());
	}

	std::string GetLibraryName() const { return GetPlugin()->Info.szLibraryName; }

	bool IsLoaded() const { return GetPlugin()->pMixPlugin != nullptr; }

	bool GetMaster() const
	{
		return GetPlugin()->IsMasterEffect();
	}
	void SetMaster(bool master)
	{
		auto plugin = GetPlugin();
		plugin->SetMasterEffect(master);
		SetModified(plugin, PluginHint(static_cast<PLUGINDEX>(m_index)).Info());
	}

	bool GetDryMix() const
	{
		return GetPlugin()->IsDryMix();
	}
	void SetDryMix(bool dryMix)
	{
		auto plugin = GetPlugin();
		plugin->SetDryMix(dryMix);
		SetModified(plugin, PluginHint(static_cast<PLUGINDEX>(m_index)).Info());
	}

	bool GetExpandedMix() const
	{
		return GetPlugin()->IsExpandedMix();
	}
	void SetExpandedMix(bool expand)
	{
		auto plugin = GetPlugin();
		plugin->SetExpandedMix(expand);
		SetModified(plugin, PluginHint(static_cast<PLUGINDEX>(m_index)).Info());
	}

	bool GetAutoSuspend() const
	{
		return GetPlugin()->IsAutoSuspendable();
	}
	void SetAutoSuspend(bool autoSuspend)
	{
		auto plugin = GetPlugin();
		plugin->SetAutoSuspend(autoSuspend);
		SetModified(plugin, PluginHint(static_cast<PLUGINDEX>(m_index)).Info());
	}

	bool GetBypass() const
	{
		return GetPlugin()->IsBypassed();
	}
	void SetBypass(bool bypass)
	{
		Script::CallInGUIThread([this, bypass]()
		{
			auto plugin = GetPlugin();
			plugin->SetBypass(bypass);
			SetModified(plugin, PluginHint(static_cast<PLUGINDEX>(m_index)).Info());
		});
	}

	float GetDryWet() const
	{
		return GetPlugin()->fDryRatio;
	}
	void SetDryWet(float dryRatio)
	{
		auto plugin = GetPlugin();
		Limit(dryRatio, 0.0f, 1.0f);
		if(dryRatio != GetDryWet())
		{
			plugin->fDryRatio = dryRatio;
			SetModified(plugin, PluginHint(static_cast<PLUGINDEX>(m_index)).Info());
		}
	}

	double GetGain() const
	{
		return GetPlugin()->GetGain() * 0.1;
	}
	void SetGain(double gain)
	{
		auto plugin = GetPlugin();
		if(gain != GetGain())
		{
			plugin->SetGain(mpt::saturate_round<uint8>(Clamp(gain, 0.1, 8.0) * 10.0));
			SetModified(plugin, PluginHint(static_cast<PLUGINDEX>(m_index)).Info());
		}
	}

	sol::optional<Plugin> GetOutput() const
	{
		auto plugin = GetPlugin();
		if(plugin->IsOutputToMaster())
			return {};
		else
			return Plugin{*const_cast<Plugin *>(this), plugin->GetOutputPlugin()};
	}
	void SetOutput(sol::object output)
	{
		auto plugin = GetPlugin();
		PLUGINDEX outIndex = IndexFromObject(output, SndFile());
		if(outIndex > m_index + 1u)
			plugin->SetOutputPlugin(outIndex - 1u);
		else
			plugin->SetOutputToMaster();
		SetModified(plugin, PluginHint(static_cast<PLUGINDEX>(m_index)).Info());
	}
};

struct PluginList final : public ProxyContainer<Plugin>, public Obj
{
	PluginList(const Obj &obj) noexcept : Obj{obj} { }

	size_t size() override { return MAX_MIXPLUGINS; }
	iterator begin() override { return iterator{*this, 0}; }
	iterator end() override { return iterator{*this, MAX_MIXPLUGINS}; }
	value_type operator[] (size_t index) override { return Plugin{*this, index}; }
};

}  // namespace Scripting

OPENMPT_NAMESPACE_END

namespace sol
{
using namespace OPENMPT_NAMESPACE::Scripting;

template <>
struct is_container<PluginParamList> : std::true_type { };

template <>
struct usertype_container<PluginParamList>
{
	static int size(lua_State *L)
	{
		auto &self = sol::stack::get<PluginParamList &>(L, 1);
		return sol::stack::push(L, self.size());
	}
	static auto begin(lua_State *, PluginParamList &that)
	{
		return that.begin();
	}
	static auto end(lua_State *, PluginParamList &that)
	{
		return that.end();
	}

	// Make the parameter list 0-based
	static std::ptrdiff_t index_adjustment(lua_State *, PluginParamList &)
	{
		return 0;
	}
};

}  // namespace sol
