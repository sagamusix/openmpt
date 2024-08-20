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
#include "../Mptrack.h"
#include "../InputHandler.h"
#include "../FileDialog.h"
#include "../Reporting.h"
#include "../TrackerSettings.h"
#include "../../soundlib/plugins/PluginManager.h"

#include <sapi.h>

#if MPT_WINNT_AT_LEAST(MPT_WIN_VISTA) && defined(UNICODE)
#include <afxtaskdialog.h>
#endif

OPENMPT_NAMESPACE_BEGIN

namespace Scripting
{

// Helper function to build file dialog filter string from Lua table
static CString BuildFileFilterString(const sol::optional<sol::table> &filters)
{
	CString filterString;
	if(!filters)
		return filterString;

	for(const auto &entry : *filters)
	{
		if(!entry.second.is<sol::table>())
			continue;
		sol::table filterEntry = entry.second.as<sol::table>();
		std::string pattern = filterEntry.get_or<std::string>("filter", "");
		if(pattern.empty())
			continue;
		if(!filterString.IsEmpty())
			filterString += _T("|");
		std::string description = filterEntry.get_or<std::string>("description", pattern);
		filterString += mpt::ToCString(mpt::Charset::UTF8, description + "|" + pattern);
	}
	if(!filterString.IsEmpty())
		filterString += _T("||");
	return filterString;
}

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

MPT_MAKE_ENUM(Callbacks)
#undef Callbacks

void Script::RegisterCallback(std::string_view callbackType, sol::protected_function callback)
{
	switch(EnumCallbacks::Parse(callbackType))
	{
	case EnumCallbacks::Enum::NEWSONG:
		newSongCallback = callback;
		break;
	case EnumCallbacks::Enum::NEWPATTERN:
		newPatternCallback = callback;
		break;
	case EnumCallbacks::Enum::NEWSAMPLE:
		newSampleCallback = callback;
		break;
	case EnumCallbacks::Enum::NEWINSTRUMENT:
		newInstrumentCallback = callback;
		break;
	case EnumCallbacks::Enum::NEWPLUGIN:
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

		, "new_song", sol::as_function([this](sol::optional<std::string_view> format)
		{
			MODTYPE newType = MOD_TYPE_NONE;
			if(format)
			{
				switch(EnumFormat::Parse(*format))
				{
				case EnumFormat::Enum::MOD: newType = MOD_TYPE_MOD; break;
				case EnumFormat::Enum::XM: newType = MOD_TYPE_XM; break;
				case EnumFormat::Enum::S3M: newType = MOD_TYPE_S3M; break;
				case EnumFormat::Enum::IT: newType = MOD_TYPE_IT; break;
				case EnumFormat::Enum::MPTM: newType = MOD_TYPE_MPT; break;
				case EnumFormat::Enum::INVALID_VALUE: throw "Unknown type (must be one of openmpt.Song.format)";
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
		, "playing_song", sol::as_function([this]()
		{
			// TODO: Does it make sense to turn into property so that setting the song is possible too?
			CModDoc *doc = CMainFrame::GetMainFrame()->GetModPlaying();
			if (doc != nullptr && Manager::DocumentExists(*doc))
				return sol::make_object(lua, Song(*this, *doc));
			else
				return sol::make_object(lua, sol::nil);
		})
	);

	openmpt.new_enum("util"
		, "accessibility_announce", sol::as_function([this](std::string_view text, sol::optional<double> rate)
		{
			if(!m_sapiController)
				CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_INPROC_SERVER, IID_ISpVoice, reinterpret_cast<void **>(&m_sapiController));
			if(m_sapiController)
			{
				DWORD flags = SPF_ASYNC | SPF_IS_NOT_XML;
				m_sapiController->SetRate(mpt::saturate_round<long>(std::clamp(rate.value_or(0.0), -1.0, 1.0) * 10.0));
				m_sapiController->Speak(mpt::ToWide(mpt::Charset::UTF8, text).c_str(), flags, nullptr);
			}
		})
		, "dump", lua.do_string(
#include "api/LuaDump.h"
			, "=openmpt.util.dump")
		, "eval", sol::as_function(&Script::Eval<sol::object>, this)

		, "linear_to_db", sol::as_function([](double lin) { return 20.0 * std::log10(lin); })
		, "db_to_linear", sol::as_function([](double dB) { return std::pow(10.0, dB / 20.0); })

		, "cutoff_to_frequency", sol::as_function([](const Song &song, double cutoff) { return song.SndFile()->CutOffToFrequency(mpt::saturate_round<uint32>(cutoff * 127.0)); })
		, "frequency_to_cutoff", sol::as_function([](const Song &song, double frequency) { return song.SndFile()->FrequencyToCutOff(frequency) / 127.0; })

		, "transpose_to_frequency", sol::as_function([](int transpose, int finetune) { return ModSample::TransposeToFrequency(transpose, finetune); })
		, "frequency_to_transpose", sol::as_function([](sol::this_state state, uint32 frequency)
		{
			sol::state_view lua{state};
			const auto [transpose, finetune] = ModSample::FrequencyToTranspose(frequency);
			sol::table table = lua.create_table();
			table["transpose"] = transpose;
			table["finetune"] = finetune;
			return table;
		})

		, "fft", sol::as_function([](sol::table real, sol::optional<sol::table> imaginary) { Script::FFT(real, imaginary, false); })
		, "ifft", sol::as_function([](sol::table real, sol::optional<sol::table> imaginary) { Script::FFT(real, imaginary, true); })
	);

	openmpt.new_enum("app"
		, "API_VERSION", Scripting::API_VERSION
		, "OPENMPT_VERSION", mpt::ToCharset(mpt::Charset::UTF8, Version::Current().ToUString())
		, "recent_songs", sol::as_function([](sol::this_state state)
		{
			const auto &mruFiles = TrackerSettings::Instance().mruFiles;
			sol::state_view lua{state};
			sol::table files = lua.create_table(static_cast<int>(mruFiles.size()), 0);
			for(const auto &file : mruFiles)
				files.add(file.ToUTF8());
			return files;
		})
		, "template_songs", sol::as_function([](sol::this_state state)
		{
			const auto &templateFiles = CMainFrame::GetMainFrame()->GetTemplateModulePaths();
			sol::state_view lua{state};
			sol::table files = lua.create_table(static_cast<int>(templateFiles.size()), 0);
			for(const auto &file : templateFiles)
				files.add(file.ToUTF8());
			return files;
		})
		, "example_songs", sol::as_function([](sol::this_state state)
		{
			const auto &exampleFiles = CMainFrame::GetMainFrame()->GetExampleModulePaths();
			sol::state_view lua{state};
			sol::table files = lua.create_table(static_cast<int>(exampleFiles.size()), 0);
			for(const auto &file : exampleFiles)
				files.add(file.ToUTF8());
			return files;
		})
		, "load", sol::as_function([this](const sol::variadic_args &va)
		{
			sol::table songs = lua.create_table(static_cast<int>(va.size()), 0);
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
			sol::table songs = lua.create_table(static_cast<int>(docs.size()), 0);
			for(const auto &doc : docs)
			{
				songs.add(Song(*this, *doc));
			}
			return songs;
		})
		, "path", sol::property([]() { return theApp.GetInstallPath().ToUTF8(); })
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
						{ "APPS", VK_APPS },         { "SLEEP", VK_SLEEP },
						{ "NUM*", VK_MULTIPLY },     { "NUM+", VK_ADD },
						//{ "", VK_SEPARATOR },
						{ "NUM-", VK_SUBTRACT },     { "NUM.", VK_DECIMAL },
						{ "NUM/", VK_DIVIDE },
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
		, "register_midi_filter", sol::as_function([this](sol::protected_function func)
		{
			m_nextMidiFilterID++;
			m_midiFilters[m_nextMidiFilterID] = func;
			return m_nextMidiFilterID;
		})
		, "remove_midi_filter", sol::as_function([this](int id)
		{
			const auto it = m_midiFilters.find(id);
			if(it == m_midiFilters.end())
				return false;
			m_midiFilters.erase(it);
			return true;
		})
		, "set_timer", sol::as_function([](sol::protected_function func, double duration) -> sol::optional<UINT_PTR>
		{
			UINT durationMS = mpt::saturate_round<UINT>(std::clamp(duration * 1000.0, double(USER_TIMER_MINIMUM), double(USER_TIMER_MAXIMUM)));
			UINT_PTR id = ::SetTimer(nullptr, 0, durationMS, &Script::TimerProc);
			if(!id)
				return sol::nullopt;
			Script::s_timers[id] = func;
			return id;
		})
		, "cancel_timer", sol::as_function([](sol::this_state state, UINT_PTR id)
		{
			if(auto it = Script::s_timers.find(id); it != Script::s_timers.end())
			{
				if(it->second.lua_state() == state)
				{
					::KillTimer(nullptr, id);
					Script::s_timers.erase(it);
					return true;
				}
			}
			return false;
		})
		, "show_message", sol::as_function([](sol::this_state state, const sol::variadic_args &va)
		{
			sol::state_view lua{state};
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
		, "task_dialog", sol::as_function([](sol::this_state state, sol::table config) -> sol::table
		{
#if MPT_WINNT_AT_LEAST(MPT_WIN_VISTA) && defined(UNICODE)
			CString title = mpt::ToCString(mpt::Charset::UTF8, config.get_or<std::string>("title", ""));
			CString description = mpt::ToCString(mpt::Charset::UTF8, config.get_or<std::string>("description", ""));
			sol::optional<sol::table> choices = config.get<sol::optional<sol::table>>("choices");
			sol::optional<sol::table> buttons = config.get<sol::optional<sol::table>>("buttons");
			sol::optional<std::string> footer = config.get<sol::optional<std::string>>("footer");
			sol::optional<sol::table> expansion = config.get<sol::optional<sol::table>>("expansion");
			sol::optional<std::string> verification = config.get<sol::optional<std::string>>("verification");
			sol::optional<int> defaultChoice = config.get<sol::optional<int>>("default_choice");
			sol::optional<int> defaultRadio = config.get<sol::optional<int>>("default_radio");

			TASKDIALOG_COMMON_BUTTON_FLAGS commonButtons = 0;
			if(buttons)
			{
				for(const auto &entry : *buttons)
				{
					std::string buttonType = entry.second.as<std::string>();
					if(buttonType == "ok")
						commonButtons |= TDCBF_OK_BUTTON;
					else if(buttonType == "yes")
						commonButtons |= TDCBF_YES_BUTTON;
					else if(buttonType == "no")
						commonButtons |= TDCBF_NO_BUTTON;
					else if(buttonType == "cancel")
						commonButtons |= TDCBF_CANCEL_BUTTON;
					else if(buttonType == "retry")
						commonButtons |= TDCBF_RETRY_BUTTON;
					else if(buttonType == "close")
						commonButtons |= TDCBF_CLOSE_BUTTON;
				}
			}
			if(commonButtons == 0)
				commonButtons = TDCBF_OK_BUTTON;

			CTaskDialog dlg{title, description, title, commonButtons};

			if(choices)
			{
				for(const auto &entry : *choices)
				{
					if(entry.second.is<sol::table>())
					{
						sol::table choice = entry.second.as<sol::table>();
						std::string text = choice.get_or<std::string>("text", "");
						std::string type = choice.get_or<std::string>("type", "button");
						bool enabled = choice.get_or("enabled", true);

						if(text.empty())
							continue;
						int id = choice.get_or("id", static_cast<int>(entry.first.as<int>()));
						if(type == "radio")
							dlg.AddRadioButton(200 + id, mpt::ToCString(mpt::Charset::UTF8, text), enabled);
						else
							dlg.AddCommandControl(200 + id, mpt::ToCString(mpt::Charset::UTF8, text), enabled);
					} else if(entry.second.is<std::string>())
					{
						std::string text = entry.second.as<std::string>();
						int id = static_cast<int>(entry.first.as<int>());
						dlg.AddCommandControl(200 + id, mpt::ToCString(mpt::Charset::UTF8, text));
					}
				}
			}

			// Set footer text
			if(footer)
				dlg.SetFooterText(mpt::ToCString(mpt::Charset::UTF8, *footer));

			// Set expansion area
			if(expansion)
			{
				CString expandedInfo = mpt::ToCString(mpt::Charset::UTF8, expansion->get_or<std::string>("info", ""));
				CString collapsedLabel = mpt::ToCString(mpt::Charset::UTF8, expansion->get_or<std::string>("collapsed_label", ""));
				CString expandedLabel = mpt::ToCString(mpt::Charset::UTF8, expansion->get_or<std::string>("expanded_label", ""));
				bool expandedByDefault = expansion->get_or("expanded", false);

				if(!expandedInfo.IsEmpty())
				{
					dlg.SetExpansionArea(
						expandedInfo,
						collapsedLabel.IsEmpty() ? nullptr : collapsedLabel,
						expandedLabel.IsEmpty() ? nullptr : expandedLabel);
					if(expandedByDefault)
						dlg.SetOptions(dlg.GetOptions() | TDF_EXPANDED_BY_DEFAULT);
				}
			}

			if(verification)
				dlg.SetVerificationCheckboxText(mpt::ToCString(mpt::Charset::UTF8, *verification));

			if(defaultChoice)
				dlg.SetDefaultCommandControl(200 + *defaultChoice);
			if(defaultRadio)
				dlg.SetDefaultRadioButton(200 + *defaultRadio);

			INT_PTR dlgResult = IDOK;
			CallInGUIThread([&dlgResult, &dlg]()
			{
				dlgResult = dlg.DoModal();
			});

			sol::state_view lua{state};
			sol::table result = lua.create_table();
			// TODO this is not good
			if(dlgResult == IDOK || dlgResult == IDYES || dlgResult == IDNO || dlgResult == IDCANCEL || dlgResult == IDRETRY || dlgResult == IDCLOSE)
				result["button"] = sol::make_object(lua, dlgResult);
			else if(dlgResult >= 200)
				result["button"] = sol::make_object(lua, dlgResult - 200);
			else
				result["button"] = sol::make_object(lua, sol::nil);

			result["radio"] = sol::make_object(lua, dlg.GetSelectedRadioButtonID() - 200);
			result["checked"] = sol::make_object(lua, dlg.GetVerificationCheckboxState());

			return result;
#else
			sol::state_view lua{state};
			sol::table result = lua.create_table();
			result["button"] = sol::make_object(lua, sol::nil);
			result["radio"] = sol::make_object(lua, sol::nil);
			result["checked"] = sol::make_object(lua, false);
			return result;
#endif
		})
		, "browse_for_folder", sol::as_function([](sol::optional<std::string> title, sol::optional<std::string> initialPath)
		{
			BrowseForFolder dlg(mpt::PathString::FromUTF8(initialPath.value_or(std::string{})), mpt::ToCString(mpt::Charset::UTF8, title.value_or(std::string{})));
			if(dlg.Show())
				return dlg.GetDirectory().ToUTF8();
			return std::string{};
		})
		, "file_load_dialog", sol::as_function([this](sol::optional<std::string> title, sol::optional<sol::table> filters, sol::optional<std::string> initialPath, sol::optional<bool> allowMultiple)
		{
			OpenFileDialog dlg{mpt::ToCString(mpt::Charset::UTF8, title.value_or(""))};
			if(allowMultiple.value_or(false))
				dlg.AllowMultiSelect();
			if(initialPath)
				dlg.WorkingDirectory(mpt::PathString::FromUTF8(*initialPath));
			const auto filterString = BuildFileFilterString(filters);
			if(!filterString.IsEmpty())
				dlg.ExtensionFilter(filterString);
			sol::table results = lua.create_table();
			if(dlg.Show())
			{
				for(const auto &path : dlg.GetFilenames())
				{
					std::string pathStr = path.ToUTF8();
					results.add(pathStr);
					m_allowReadFiles.insert(std::move(pathStr));
				}
			}
			return results;
		})
		, "file_save_dialog", sol::as_function([this](sol::optional<std::string> title, sol::optional<sol::table> filters, sol::optional<std::string> initialPath)
		{
			SaveFileDialog dlg{mpt::ToCString(mpt::Charset::UTF8, title.value_or(""))};
			if(initialPath)
				dlg.WorkingDirectory(mpt::PathString::FromUTF8(*initialPath));
			const auto filterString = BuildFileFilterString(filters);
			if(!filterString.IsEmpty())
				dlg.ExtensionFilter(filterString);
			if(dlg.Show())
			{
				std::string pathStr = dlg.GetFirstFile().ToUTF8();
				m_allowWriteFiles.insert(pathStr);
				return pathStr;
			}
			return std::string{};
		})
		, "get_state", sol::as_function(&Script::GetState, this)
		, "set_state", sol::as_function(&Script::SetState, this)
		, "delete_state", sol::as_function(&Script::DeleteState, this)
		, "check_permission", sol::as_function([this](sol::object permissionObj)
		{
			if(permissionObj.is<std::string>())
			{
				return m_permissions.count(permissionObj.as<std::string>()) > 0;
			} else if(permissionObj.is<sol::table>())
			{
				sol::table permissions = permissionObj.as<sol::table>();
				for(const auto &[_, permission] : permissions)
				{
					if(m_permissions.count(permission.as<std::string>()) == 0)
						return false;
				}
				return true;
			} else
			{
				throw "Parameter must be a string or table of strings";
			}
		})
		, "request_permission", sol::as_function([this](sol::object permissionObj)
		{
			return RequestPermissions(permissionObj);
		})
		, "midi_devices", sol::as_function([this]()
		{
			sol::table table = lua.create_table(0, 2);
			CallInGUIThread([&]()
			{
				UINT numDevs = midiInGetNumDevs();
				sol::table inputs = lua.create_table(static_cast<int>(numDevs), 0);
				for(UINT i = 0; i < numDevs; i++)
				{
					MIDIINCAPS mic;
					mic.szPname[0] = 0;
					if(midiInGetDevCaps(i, &mic, sizeof(mic)) == MMSYSERR_NOERROR)
					{
						sol::table device = lua.create_table(0, 3);
						device["id"] = i + 1;
						auto name = mpt::String::ReadCStringBuf(mic.szPname);
						device["name"] = mpt::ToCharset(mpt::Charset::UTF8, name);
						device["friendly_name"] = mpt::ToCharset(mpt::Charset::UTF8, theApp.GetFriendlyMIDIPortName(name, true, false));
						//device["active"] = ...
						inputs.add(device);
					}
				}

				numDevs = midiOutGetNumDevs();
				sol::table outputs = lua.create_table(static_cast<int>(numDevs), 0);
				for(UINT i = 0; i < numDevs; i++)
				{
					MIDIOUTCAPS moc;
					moc.szPname[0] = 0;
					if(midiOutGetDevCaps(i, &moc, sizeof(moc)) == MMSYSERR_NOERROR)
					{
						sol::table device = lua.create_table(0, 3);
						device["id"] = i + 1;
						auto name = mpt::String::ReadCStringBuf(moc.szPname);
						device["name"] = mpt::ToCharset(mpt::Charset::UTF8, name);
						device["friendly_name"] = mpt::ToCharset(mpt::Charset::UTF8, theApp.GetFriendlyMIDIPortName(name, false, false));
						//device["active"] = ...
						outputs.add(device);
					}
				}

				table["inputs"] = inputs;
				table["outputs"] = outputs;
			});
			return table;
		})
		// TODO: have a namespace plugins, with a list of plugins and the possibility to register/unregister?
		, "registered_plugins", sol::as_function([this]()
		{
			sol::table plugins = lua.create_table();
			for(const auto &p : *theApp.GetPluginManager())
			{
				VSTPluginLib &plug = *p;
				sol::table info = lua.create_table();
				std::string pluginId = mpt::afmt::HEX0<8>(static_cast<uint32>(plug.pluginId1)) + "." + mpt::afmt::HEX0<8>(static_cast<uint32>(plug.pluginId2));
				if(plug.shellPluginID)
					pluginId += "." + mpt::afmt::HEX0<8>(plug.pluginId2);
				info["library_name"] = plug.libraryName.ToUTF8();
				info["path"] = plug.dllPath.ToUTF8();
				info["id"] = std::move(pluginId);
				info["is_instrument"] = !!plug.isInstrument;
				info["is_builtin"] = plug.isBuiltIn;
				//info["category"] = static_cast<int>(plug.category);  //TODO
				info["vendor"] = mpt::ToCharset(mpt::Charset::UTF8, plug.vendor);
				info["tags"] = mpt::ToCharset(mpt::Charset::UTF8, plug.tags);
				plugins.add(info);
			}
			return plugins;
		})
		// TODO doesn't work with new_enum
		, "metronome_enabled", sol::property([]() { return TrackerSettings::Instance().metronomeEnabled.Get(); }, [](bool enabled) { TrackerSettings::Instance().metronomeEnabled = enabled; theApp.PostMessageToAllViews(WM_MOD_CTRLMSG, CTRLMSG_PAT_UPDATE_TOOLBAR); })
		, "metronome_volume", sol::property([]() { return TrackerSettings::Instance().metronomeVolume.Get(); }, [](float volume) { TrackerSettings::Instance().metronomeVolume = std::clamp(volume, -48.0f, 0.0f); })
		// this should probably require a permission to read/write
		//, "settings"
		);

	// Random tools to add:
	// - ShellExecute via theApp.OpenFile

	// Pattern, sample, instrument clipboard (read/write)

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
	OPLData::Register(song);
	OPLOperator::Register(song);
	SampleLoop::Register(song);
	SampleCuePoints::Register(song);
	Sample::Register(song);
	Instrument::Register(song);
	Envelope::Register(song);
	EnvelopePoint::Register(song);
	NoteMapEntry::Register(song);
	Plugin::Register(song);
	PluginParameter::Register(song);
	PatternCell::Register(song);
	Pattern::Register(song);
	Sequence::Register(song);
	Channel::Register(song);
	ChannelList::Register(song);
	Tuning::Register(song);
	TuningRatioTable::Register(song);
	TuningList::Register(song);

	EnumCallbacks::Register(openmpt);
	EnumFadeLaw::Register(openmpt);
	EnumFormat::Register(song);
	EnumMixLevels::Register(song);
	EnumTempoMode::Register(song);
	EnumLoopType::Register(song);
	EnumVibratoType::Register(song);
	EnumOPLWaveform::Register(song);
	EnumInterpolationType::Register(song);
	EnumNewNoteAction::Register(song);
	EnumDuplicateCheckType::Register(song);
	EnumDuplicateNoteAction::Register(song);
	EnumFilterMode::Register(song);
	EnumResamplingMode::Register(song);
	EnumPluginVolumeHandling::Register(song);
	EnumPluginMixMode::Register(song);
	EnumTranspose::Register(song);
	EnumTuningType::Register(song);

	return openmpt;
}


void Script::TimerProc(HWND, UINT, UINT_PTR id, DWORD)
{
	const auto it = s_timers.find(id);
	if(it == s_timers.end())
		return;
	::KillTimer(nullptr, id);
	auto f = s_timers.extract(it).mapped();

	sol::state_view lua{f.lua_state()};
	// Stopgap solution: Setting a timer repeatedly will create many new stacks that eventually get garbage-collected.
	// Apparently this can sometimes happen right before the sol::thread destructor runs.
	// We should probably just have one pre-created thread for each type of callback and reuse it.
	lua.collect_garbage();
	auto thread = sol::thread::create(lua);
	sol::protected_function func{thread.state(), f};
	func(id);
}


}  // namespace Scripting

OPENMPT_NAMESPACE_END
