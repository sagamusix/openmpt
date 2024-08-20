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
#include "../../../soundlib/plugins/PluginManager.h"

OPENMPT_NAMESPACE_BEGIN

namespace Scripting
{

#define PluginMixMode(X) \
	X(DEFAULT) \
	X(WET_SUBTRACT) \
	X(DRY_SUBTRACT) \
	X(MIX_SUBTRACT) \
	X(MIDDLE_SUBTRACT) \
	X(BALANCE) \
	X(INSTRUMENT)

MPT_MAKE_ENUM(PluginMixMode)
#undef PluginMixMode

struct PluginParameter : public Obj
{
	const PlugParamIndex m_paramIndex;

	static void Register(sol::table &table)
	{
		table.new_usertype<PluginParameter>("PluginParameter"
			, sol::meta_function::to_string, &Name
			, sol::meta_function::type, &Type

			, "index", sol::readonly(&PluginParameter::m_index)
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
		if(m_index < MAX_MIXPLUGINS && sndFile->m_MixPlugins[m_index].pMixPlugin != nullptr)
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
		// TODO only mark as modified if format supports plugins
		SetModified(plugin, PluginHint(static_cast<PLUGINDEX>(m_index + 1)).Parameter());
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

			, "index", sol::property(&GetSlot)
			, "name", sol::property(&GetName, &SetName)
			, "library_name", sol::property(&GetLibraryName)
			, "is_loaded", sol::property(&IsLoaded)
			, "parameters", &Plugin::Parameters
			, "master", sol::property(&GetMaster, &SetMaster)
			, "dry_mix", sol::property(&GetDryMix, &SetDryMix)
			, "expanded_mix", sol::property(&GetExpandedMix, &SetExpandedMix)
			, "auto_suspend", sol::property(&GetAutoSuspend, &SetAutoSuspend)
			, "bypass", sol::property(&GetBypass, &SetBypass)
			, "dry_wet_ratio", sol::property(&GetDryWet, &SetDryWet)
			, "mix_mode", sol::property(&GetMixMode, &SetMixMode)
			, "gain", sol::property(&GetGain, &SetGain)
			, "output", sol::property(&GetOutput, &SetOutput)

			, "create", & Plugin::Create
			//, "load_preset"
			//, "save_preset"
			, "midi_command", &Plugin::MidiCommand
			, "toggle_editor", &Plugin::ToggleEditor
		);
	}

	Plugin(const Obj &obj, size_t plugin) noexcept : Obj{obj, plugin}, Parameters{obj, plugin} { }

	bool operator==(const Plugin &other) const noexcept { return IsEqual(other); }

	void SetModified(const CriticalSection &, PluginHint hint)
	{
		auto modDoc = ModDoc();
		auto *mainFrm = CMainFrame::GetMainFrame();
		if(modDoc->GetSoundFile().GetModSpecifications().supportsPlugins)
			modDoc->SetModified();
		mainFrm->PostMessage(WM_MOD_UPDATEVIEWS, reinterpret_cast<WPARAM>(static_cast<CModDoc *>(modDoc)), hint.AsLPARAM());
	}

	std::string Name() const { return MPT_AFORMAT("[openmpt.Song.Plugin object: Slot {}]")(m_index + 1u); }
	static std::string Type() { return "openmpt.Song.Plugin"; }

	size_t GetSlot() const { return m_index + 1u; }

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
		SetModified(sndFile, PluginHint(static_cast<PLUGINDEX>(m_index + 1)).Names());
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
		SetModified(plugin, PluginHint(static_cast<PLUGINDEX>(m_index + 1)).Info());
	}

	bool GetDryMix() const
	{
		return GetPlugin()->IsDryMix();
	}
	void SetDryMix(bool dryMix)
	{
		auto plugin = GetPlugin();
		plugin->SetDryMix(dryMix);
		SetModified(plugin, PluginHint(static_cast<PLUGINDEX>(m_index + 1)).Info());
	}

	bool GetExpandedMix() const
	{
		return GetPlugin()->IsExpandedMix();
	}
	void SetExpandedMix(bool expand)
	{
		auto plugin = GetPlugin();
		plugin->SetExpandedMix(expand);
		SetModified(plugin, PluginHint(static_cast<PLUGINDEX>(m_index + 1)).Info());
	}

	bool GetAutoSuspend() const
	{
		return GetPlugin()->IsAutoSuspendable();
	}
	void SetAutoSuspend(bool autoSuspend)
	{
		auto plugin = GetPlugin();
		plugin->SetAutoSuspend(autoSuspend);
		SetModified(plugin, PluginHint(static_cast<PLUGINDEX>(m_index + 1)).Info());
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
			SetModified(plugin, PluginHint(static_cast<PLUGINDEX>(m_index + 1)).Info());
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
			SetModified(plugin, PluginHint(static_cast<PLUGINDEX>(m_index + 1)).Info());
		}
	}

	std::string GetMixMode() const
	{
		switch(GetPlugin()->GetMixMode())
		{
		case PluginMixMode::Default: return EnumPluginMixMode::DEFAULT;
		case PluginMixMode::WetSubtract: return EnumPluginMixMode::WET_SUBTRACT;
		case PluginMixMode::DrySubtract: return EnumPluginMixMode::DRY_SUBTRACT;
		case PluginMixMode::MixSubtract: return EnumPluginMixMode::MIX_SUBTRACT;
		case PluginMixMode::MiddleSubtract: return EnumPluginMixMode::MIDDLE_SUBTRACT;
		case PluginMixMode::LRBalance: return EnumPluginMixMode::BALANCE;
		case PluginMixMode::Instrument: return EnumPluginMixMode::INSTRUMENT;
		}
		return EnumPluginMixMode::DEFAULT;
	}
	void SetMixMode(std::string_view mixMode)
	{
		auto plugin = GetPlugin();
		switch(EnumPluginMixMode::Parse(mixMode))
		{
		case EnumPluginMixMode::Enum::DEFAULT: plugin->SetMixMode(PluginMixMode::Default); break;
		case EnumPluginMixMode::Enum::WET_SUBTRACT: plugin->SetMixMode(PluginMixMode::WetSubtract); break;
		case EnumPluginMixMode::Enum::DRY_SUBTRACT: plugin->SetMixMode(PluginMixMode::DrySubtract); break;
		case EnumPluginMixMode::Enum::MIX_SUBTRACT: plugin->SetMixMode(PluginMixMode::MixSubtract); break;
		case EnumPluginMixMode::Enum::MIDDLE_SUBTRACT: plugin->SetMixMode(PluginMixMode::MiddleSubtract); break;
		case EnumPluginMixMode::Enum::BALANCE: plugin->SetMixMode(PluginMixMode::LRBalance); break;
		case EnumPluginMixMode::Enum::INSTRUMENT: plugin->SetMixMode(PluginMixMode::Instrument); break;
		case EnumPluginMixMode::Enum::INVALID_VALUE: throw "Invalid plugin mix mode value";
		}
		SetModified(plugin, PluginHint(static_cast<PLUGINDEX>(m_index + 1)).Info());
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
			SetModified(plugin, PluginHint(static_cast<PLUGINDEX>(m_index + 1)).Info());
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
		SetModified(plugin, PluginHint(static_cast<PLUGINDEX>(m_index + 1)).Info());
	}

	bool MidiCommand(const sol::table &midiData)
	{
		auto plugin = GetPlugin();
		if(plugin->pMixPlugin == nullptr)
			return false;

		std::vector<std::byte> data;
		data.resize(midiData.size());
		for(size_t i = 0; i < data.size(); i++)
		{
			data[i] = mpt::byte_cast<std::byte>(midiData[i + 1].get<uint8>());
		}
		return plugin->pMixPlugin->MidiSend(data);
	}

	void ToggleEditor()
	{
		auto plugin = GetPlugin();
		if(plugin->pMixPlugin == nullptr)
			return;
		plugin->pMixPlugin->ToggleEditor();
	}

	bool Create(sol::table pluginInfo)
	{
		bool ok = false;
		Script::CallInGUIThread([this, pluginInfo, &ok]()
		{
			CriticalSection cs;
			auto sndFile = SndFile();
			auto plugin = GetPlugin();

			if(plugin->pMixPlugin)
			{
				plugin->pMixPlugin->Release();
				mpt::reconstruct(*plugin);
			}

			std::string id = pluginInfo["id"];
			if(id.size() != 8 + 1 + 8 && id.size() != 8 + 1 + 8 + 1 + 8)
				throw "Invalid Plugin ID";

			plugin->Info.dwPluginId1 = mpt::parse_hex<uint32>(id.substr(0, 8));
			plugin->Info.dwPluginId2 = mpt::parse_hex<uint32>(id.substr(9, 8));
			if(id.size() == 8 + 1 + 8 + 1 + 8)
				plugin->Info.shellPluginID = mpt::parse_hex<uint32>(id.substr(18, 8));
			const std::string libName = pluginInfo["library_name"];
			if(libName.empty())
				throw "Plugin library name is missing";
			plugin->Info.szLibraryName = libName;
			plugin->Info.szName = ToSndFileString(libName, sndFile);

			ok = theApp.GetPluginManager()->CreateMixPlugin(plugin, *sndFile);
			if(ok)
				SetModified(sndFile, PluginHint(static_cast<PLUGINDEX>(m_index + 1)).Names().Info());
		});
		return ok;
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
