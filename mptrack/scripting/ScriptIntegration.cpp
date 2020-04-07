/*
 * ScriptIntegration.cpp
 * ---------------------
 * Purpose: Lua wrappers for CModDoc and contained classes
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "ScriptManager.h"
#include "Script.h"
#include "api/LuaSong.h"

#include "../Mainfrm.h"
#include "../InputHandler.h"
#include "../FileDialog.h"
#include "../../soundlib/plugins/PluginManager.h"

OPENMPT_NAMESPACE_BEGIN

namespace Scripting
{

struct Midi
{

};

#define Callbacks(X) \
	X(NEWSONG) \
	X(NEWPATTERN) \
	X(NEWSAMPLE) \
	X(NEWINSTRUMENT) \
	X(NEWPLUGIN) \
	X(SYNC_MIXTIME_TICK) \
	X(SYNC_MIXTIME_ROW) \
	X(SYNC_MIXTIME_PATTERN) \
	X(SYNC_PLAYTIME_TICK) \
	X(SYNC_PLAYTIME_ROW) \
	X(SYNC_PLAYTIME_PATTERN)

MPT_MAKE_ENUM(Callbacks);
#undef Callbacks

void Script::RegisterCallback(int cbType, sol::protected_function callback)
{
	switch(cbType)
	{
	case EnumCallbacks::NEWSONG:
		newSongCallback = callback;
		break;
	case EnumCallbacks::NEWPATTERN:
		newPatternCallback = callback;
		break;
	case EnumCallbacks::NEWSAMPLE:
		newSampleCallback = callback;
		break;
	case EnumCallbacks::NEWINSTRUMENT:
		newInstrumentCallback = callback;
		break;
	case EnumCallbacks::NEWPLUGIN:
		newPluginCallback = callback;
		break;
	default:
		throw "Unknown callback type!";
		break;
	}
}


sol::table Script::RegisterEnvironment()
{
	// TODO: sol::readonly http://sol2.readthedocs.io/en/latest/api/readonly.html where required
	sol::table openmpt = lua.create_table();
	openmpt.set(
		  "register_callback", sol::as_function(&Script::RegisterCallback, this)

		, "new_song", sol::as_function([this](sol::optional<EnumFormat::values> format)
		{
			MODTYPE newType = MOD_TYPE_NONE;
			if(format)
			{
				switch(format.value())
				{
				case EnumFormat::MOD: newType = MOD_TYPE_MOD; break;
				case EnumFormat::XM: newType = MOD_TYPE_XM; break;
				case EnumFormat::S3M: newType = MOD_TYPE_S3M; break;
				case EnumFormat::IT: newType = MOD_TYPE_IT; break;
				case EnumFormat::MPTM: newType = MOD_TYPE_MPT; break;
				default: return sol::make_object(lua, sol::nil);	// or throw "Unknown Format (must be an openmpt.Song.format value)"
				}
			}
			CModDoc *doc = nullptr;
			CallInGUIThread([newType, &doc]()
			{
				doc = theApp.NewDocument(newType);
			});
			if(doc != nullptr)
				return sol::make_object(lua, Song(*this, *doc));
			else
				return sol::make_object(lua, sol::nil);
		})

		// TODO move to app?
		, "active_song", sol::as_function([this]()
		{
			// TODO: Does it make sense to turn into property so that setting the song is possible too?
			auto doc = Manager::ActiveDoc();
			if(doc != nullptr && Manager::DocumentExists(*doc))
				return sol::make_object(lua, Song(*this, *doc));
			else
				return sol::make_object(lua, sol::nil);
		})
	);

	openmpt.new_enum("util"
		, "lin2db", sol::as_function([](double lin) { return 20.0 * std::log10(lin); })
		, "db2lin", sol::as_function([](double dB) { return std::pow(10.0, dB / 20.0); })
		//  "cutoff2frequency", sol::as_function([](double cutoff) { }) // TODO depends on filter settings
		//, "fft",
		//, "ifft",
		, "eval", sol::as_function(&Script::Eval<sol::object>, this)
		, "dump", lua.do_string(
#include "api/LuaDump.h"
			, "=openmpt.util.dump")	// https://stackoverflow.com/a/27028488/1530347
	);

	openmpt.new_enum("app"
		, "API_VERSION", Scripting::API_VERSION
		, "OPENMPT_VERSION", mpt::ToCharset(mpt::Charset::UTF8, Version::Current().ToUString())
		, "recent_songs", sol::as_function([](sol::this_state state)
		{
			sol::state_view lua(state);
			sol::table files = lua.create_table();
			for(const auto &file : TrackerSettings::Instance().mruFiles)
				files.add(file.ToUTF8());
			return files;
		})
		, "load", sol::as_function([this](const sol::variadic_args &va)
		{
			sol::table songs = lua.create_table();
			CallInGUIThread([this, &va, &songs]()
			{
				for(const auto &obj : va)
				{
					if(obj.is<std::string>())
					{
						CModDoc *doc = static_cast<CModDoc *>(theApp.OpenDocumentFile(mpt::ToCString(mpt::Charset::UTF8, obj.as<std::string>())));
						if(doc != nullptr)
						{
							songs.add(Song(*this, *doc));
						}
					}
				}
			});
			return songs;
		})
		// TODO: Find song by name or filename?
		, "open_songs", sol::as_function([this]()
		{
			const auto docs = Manager::GetDocuments();
			sol::table songs = lua.create_table();
			//songs.reserve(docs.size());
			for(const auto &doc : docs)
			{
				songs.add(Song(*this, *doc));
			}
			return songs;
		})
		, "register_shortcut", sol::as_function([this](sol::protected_function func, const sol::variadic_args &va, const std::string &)
		{
			auto *ih = CMainFrame::GetMainFrame()->GetInputHandler();
			auto &commandSet = ih->m_activeCommandSet;
			KeyCombination kc;
			for(const auto &obj : va)
			{
				if(obj.is<std::string>())
				{
					std::string key = mpt::ToUpperCaseAscii(obj.as<std::string>());
					static constexpr std::pair<const char *, Modifiers> ModifierKeys[] =
					{
						{ "SHIFT", ModShift }, { "CTRL", ModCtrl },
						{ "ALT", ModAlt },     { "WIN", ModWin },
						{ "CC", ModMidi },     { "RSHIFT", ModRShift },
						{ "RCTRL", ModRCtrl }, { "RALT", ModRAlt },
					};
					for(const auto &m : ModifierKeys)
					{
						if(key == m.first)
						{
							kc.AddModifier(m.second);
							break;
						}
					}

					static constexpr std::pair<const char *, int> Keys[] =
					{
						{ "CLEAR", VK_CLEAR },    { "RETURN", VK_RETURN },
						//{ "SHIFT", VK_SHIFT }, //{ "CTRL", VK_CONTROL },
						{ "MENU", VK_MENU },      { "PAUSE", VK_PAUSE },
						{ "CAPITAL", VK_CAPITAL },

						{ "KANA", VK_KANA },      { "HANGUL", VK_HANGUL },
						{ "JUNJA", VK_JUNJA },    { "FINAL", VK_FINAL },
						{ "HANJA", VK_HANJA },    { "KANJI", VK_KANJI },

						{ "ESC", VK_ESCAPE },

						{ "CONVERT", VK_CONVERT }, { "NONCONVERT", VK_NONCONVERT },
						{ "ACCEPT", VK_ACCEPT },   { "MODECHANGE", VK_MODECHANGE },

						{ " ", VK_SPACE },           { "PREV", VK_PRIOR },
						{ "NEXT", VK_NEXT },         { "END", VK_END },
						{ "HOME", VK_HOME },         { "LEFT", VK_LEFT },
						{ "UP", VK_UP },             { "RIGHT", VK_RIGHT },
						{ "DOWN", VK_DOWN },         { "SELECT", VK_SELECT },
						{ "PRINT", VK_PRINT },       { "EXECUTE", VK_EXECUTE },
						{ "SNAPSHOT", VK_SNAPSHOT }, { "INSERT", VK_INSERT },
						{ "DELETE", VK_DELETE },     { "HELP", VK_HELP },
						/*
						* VK_0 - VK_9 are the same as ASCII '0' - '9' (0x30 - 0x39)
						* 0x40 : unassigned
						* VK_A - VK_Z are the same as ASCII 'A' - 'Z' (0x41 - 0x5A)
						*/
						//{ "LWIN", VK_LWIN },
						//{ "RWIN", VK_RWIN },
						{ "APPS", VK_APPS },
						{ "SLEEP", VK_SLEEP },
						/*{ "NUM0", VK_NUMPAD0 },
						{ "NUM1", VK_NUMPAD1 },
						{ "NUM2", VK_NUMPAD2 },
						{ "NUM3", VK_NUMPAD3 },
						{ "NUM4", VK_NUMPAD4 },
						{ "NUM5", VK_NUMPAD5 },
						{ "NUM6", VK_NUMPAD6 },
						{ "NUM7", VK_NUMPAD7 },
						{ "NUM8", VK_NUMPAD8 },
						{ "NUM9", VK_NUMPAD9 },*/
						{ "NUM*", VK_MULTIPLY },
						{ "NUM+", VK_ADD },
						{ "", VK_SEPARATOR },
						{ "NUM-", VK_SUBTRACT },
						{ "NUM.", VK_DECIMAL },
						{ "NUM/", VK_DIVIDE },
						/*{ "F1", VK_F1 },
						{ "F2", VK_F2 },
						{ "F3", VK_F3 },
						{ "F4", VK_F4 },
						{ "F5", VK_F5 },
						{ "F6", VK_F6 },
						{ "F7", VK_F7 },
						{ "F8", VK_F8 },
						{ "F9", VK_F9 },
						{ "F10", VK_F10 },
						{ "F11", VK_F11 },
						{ "F12", VK_F12 },
						{ "F13", VK_F13 },
						{ "F14", VK_F14 },
						{ "F15", VK_F15 },
						{ "F16", VK_F16 },
						{ "F17", VK_F17 },
						{ "F18", VK_F18 },
						{ "F19", VK_F19 },
						{ "F20", VK_F20 },
						{ "F21", VK_F21 },
						{ "F22", VK_F22 },
						{ "F23", VK_F23 },
						{ "F24", VK_F24 },*/
					};
					if(key.size() == 1 && (key[0] >= 'A' && key[0] <= 'Z') || (key[0] >= '0' && key[0] <= '9'))
					{
						kc.KeyCode(key[0]);
					} else if(key.size() == 2 && key[0] == 'F' && key[1] >= '0' && key[1] <= '9')
					{
						kc.KeyCode(VK_F1 + key[1]);
					} else if(key.size() == 3 && key[0] == 'F' && key[1] == '1' && key[2] >= '0' && key[2] <= '9')
					{
						kc.KeyCode(VK_F10 + key[2]);
					} else if(key.size() == 3 && key[0] == 'F' && key[1] == '2' && key[2] >= '0' && key[2] <= '4')
					{
						kc.KeyCode(VK_F20 + key[2]);
					} else if(key.size() == 4 && key.substr(0, 3) == "NUM" && key[3] >= '0' && key[3] <= '9')
					{
						kc.KeyCode(VK_NUMPAD0 + key[3]);
					} else for(const auto &k : Keys)
					{
						if(key == k.first)
						{
							kc.KeyCode(k.second);
							break;
						}
					}
				}
			}
			kc.EventType(kKeyEventDown);

			CommandID newCmd = commandSet->AddScriptable(kc);
			if(newCmd != kcNull)
			{
				ih->RegenerateCommandSet();
				m_shortcuts[newCmd] = func;
				return newCmd;
			} else
			{
				throw "Cannot register any more shortcuts!";
			}
		})
		, "remove_shortcut", sol::as_function([this](CommandID id)
		{
			m_shortcuts.erase(id);
			auto *ih = CMainFrame::GetMainFrame()->GetInputHandler();
			bool result = ih->m_activeCommandSet->RemoveScriptable(id);
			ih->RegenerateCommandSet();
			return result;
		})
		, "show_message", sol::as_function([](sol::this_state state, const sol::variadic_args &va)
		{
			sol::state_view lua(state);
			sol::protected_function tostring = lua["tostring"];
			std::string text;
			bool first = true;
			for(const auto &a : va)
			{
				if(first)
					first = false;
				else
					text += "\t";
				text += tostring(a.get<sol::object>());
			}
			Reporting::Information(mpt::ToUnicode(mpt::Charset::UTF8, text), "Scripting");
		})
		, "browse_for_folder", sol::as_function([](sol::optional<std::string> title, sol::optional<std::string> initialPath)
		{
			BrowseForFolder dlg(mpt::PathString::FromUTF8(initialPath.value_or(std::string{})), mpt::ToCString(mpt::Charset::UTF8, title.value_or(std::string{})));
			if(dlg.Show())
				return dlg.GetDirectory().ToUTF8();
			return std::string{};
		})
		, "file_load_dialog", sol::as_function([this](sol::optional<std::string> initialPath, sol::optional<bool> allowMultiple)
		{
			OpenFileDialog dlg;
			if(allowMultiple.value_or(false))
				dlg.AllowMultiSelect();
			if(initialPath)
				dlg.WorkingDirectory(mpt::PathString::FromUTF8(*initialPath));
			sol::table results = lua.create_table();
			if(dlg.Show())
			{
				for(const auto &path : dlg.GetFilenames())
				{
					results.add(path.ToUTF8());
				}
			}
			return results;
		})
		, "file_save_dialog", sol::as_function([](sol::optional<std::string> initialPath)
		{
			SaveFileDialog dlg;
			if(initialPath)
				dlg.WorkingDirectory(mpt::PathString::FromUTF8(*initialPath));
			if(dlg.Show())
				return dlg.GetFirstFile().ToUTF8();
			return std::string{};
		})
		// TODO: have a namespace plugins, with a list of plugins and the possibility to register/unregister?
		, "registered_plugins", sol::as_function([]
		{
			// TODO: What should this contain? Library name, path, ID 1 + ID 2, category, tags, vendor, isInstrument
			std::vector<std::string> plugins;
			const auto &pluginManager = *theApp.GetPluginManager();
			plugins.reserve(pluginManager.size());
			for(const auto &plug : pluginManager)
			{
				plugins.push_back(plug->dllPath.ToUTF8());
			}
			return plugins;
		})
	);

	// Random tools to add:
	// - ShellExecute via theApp.OpenFile

	/*lua.create_named_table(sol::metatable_key
		, sol::meta_function::index, static_cast<sol::table>(lua)
		, sol::meta_function::new_index, sol::as_function([this](const char *name, sol::object object)
	{
		if(strcmp(name, "openmpt"))
			lua[name] = object;
		else
			throw "Don't shoot yourself in the foot!";
	}));*/

	// TODO: Maybe add an FFI package, e.g. https://github.com/mascarenhas/alien or https://github.com/jmckaskill/luaffi

	Song::Register(openmpt);
	sol::table song = openmpt["Song"];
	TimeSignature::Register(song);
	SampleLoop::Register(song);
	Sample::Register(song);
	Instrument::Register(song);
	Plugin::Register(song);
	PluginParameter::Register(song);
	PatternCell::Register(song);
	Pattern::Register(song);
	Sequence::Register(song);
	Channel::Register(song);

	EnumCallbacks::Register(openmpt);
	EnumFadelaw::Register(openmpt);
	EnumFormat::Register(song);
	EnumLoopType::Register(song);
	EnumTranspose::Register(song);

	return openmpt;
}


}

OPENMPT_NAMESPACE_END
