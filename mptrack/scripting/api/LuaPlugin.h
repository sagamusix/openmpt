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
	PlugParamIndex m_paramIndex;

	static void Register(sol::table &table)
	{
		table.new_usertype<PluginParameter>("PluginParameter"
			, sol::meta_function::to_string, &Name
			, sol::meta_function::type, &Type
			, "name", sol::property(&GetName)
			, "value", sol::property(&GetValue, &SetValue)
			);
	}

	PluginParameter(const Obj &obj, size_t plugin, size_t param) noexcept : Obj(obj, plugin), m_paramIndex(static_cast<PlugParamIndex>(param)) { }

	std::string Name() const { return MPT_FORMAT("[openmpt.Song.PluginParameter object: Parameter {} of Plugin Slot {}]")(m_paramIndex, m_index + 1); }
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

struct PluginParamList : public ProxyContainer, public Obj
{
	PluginParamList(const Obj &obj, size_t plugin) noexcept : Obj(obj, plugin) { }

	size_t GetNumParameters()
	{
		auto sndFile = SndFile();
		if(m_index < MAX_MIXPLUGINS && sndFile->m_MixPlugins[m_index].pMixPlugin != nullptr)
			return sndFile->m_MixPlugins[m_index].pMixPlugin->GetNumParameters();
		else
			return 0;
	}

	size_t size() override { return GetNumParameters(); }
	iterator begin() override { return iterator(*this, 0); }
	iterator end() override { return iterator(*this, GetNumParameters()); }
	value_type operator[] (size_t index) override
	{
		if(index < size())
			return sol::make_object<PluginParameter>(m_script, *this, m_index, index);
		else
			return sol::make_object(m_script, sol::nil);
	}
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
			//, "drymix"
			, "bypass", sol::property(&GetBypass, &SetBypass)
			//, "expand"
			, "dry_wet_ratio", sol::property(&GetDryWet, &SetDryWet)
			//, "mixmode"
			, "gain", sol::property(&GetGain, &SetGain)
			, "output", sol::property(&GetOutput, &SetOutput)
			//, "midi_command"
			);
	}

	Plugin(const Obj &obj, size_t plugin) noexcept : Obj(obj, plugin), Parameters(obj, plugin) { }

	std::string Name() const { return MPT_FORMAT("[openmpt.Song.Plugin object: Slot {}]")(m_index + 1); }
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
			plugin->SetGain(mpt::saturate_round<uint8>(Clamp(gain, 0.1, 8.0) * 10));
			SetModified(plugin, PluginHint(static_cast<PLUGINDEX>(m_index)).Info());
		}
	}

	sol::object GetOutput() const
	{
		auto plugin = GetPlugin();
		if(plugin->IsOutputToMaster())
			return sol::nil;
		else
			return sol::make_object<Plugin>(m_script, *const_cast<Plugin *>(this), plugin->GetOutputPlugin());
	}
	void SetOutput(sol::object output)
	{
		auto plugin = GetPlugin();
		if(output.is<PLUGINDEX>())
		{
			PLUGINDEX i = std::max(output.as<PLUGINDEX>(), PLUGINDEX(1)) - 1u;
			if(i > m_index && i < MAX_MIXPLUGINS && SndFile()->m_MixPlugins[i].pMixPlugin != nullptr)
				plugin->SetOutputPlugin(i);
			else
				plugin->SetOutputToMaster();
		} else if(output.is<Plugin>())
		{
			PLUGINDEX i = static_cast<PLUGINDEX>(output.as<Plugin>().m_index);
			if(i > m_index && i < MAX_MIXPLUGINS && SndFile()->m_MixPlugins[i].pMixPlugin != nullptr)
				plugin->SetOutputPlugin(i);
			else
				plugin->SetOutputToMaster();
		} else if(!output.valid())
		{
			plugin->SetOutputToMaster();
		} else
		{
			throw "Invalid parameter type (index or plugin object expected)";
		}
		SetModified(plugin, PluginHint(static_cast<PLUGINDEX>(m_index)).Info());
	}
};

struct PluginList : public ProxyContainer, public Obj
{
	PluginList(const Obj &obj) noexcept : Obj(obj) { }

	size_t size() override { return MAX_MIXPLUGINS; }
	iterator begin() override { return iterator(*this, 0); }
	iterator end() override { return iterator(*this, MAX_MIXPLUGINS); }
	value_type operator[] (size_t index) override
	{
		if(index < size())
			return sol::make_object<Plugin>(m_script, *this, index);
		else
			return sol::make_object(m_script, sol::nil);
	}
};

}

OPENMPT_NAMESPACE_END
