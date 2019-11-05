// TODO verify CronoX3 5.1, Redux output, 5.1 Master Limiter
// TODO maybe use a list (with icons) instead of dropdown for plugin slots
// TODO NO_PLUGINS guards?
// TODO when done, do SVN move again

#include "stdafx.h"
#include "Ctrl_plugins.h"
#include "AbstractVstEditor.h"
#include "Childfrm.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "MoveFXSlotDialog.h"
#include "Reporting.h"
#include "resource.h"
#include "SelectPluginDialog.h"
#include "TrackerSettings.h"
#include "View_plugins.h"
#include "WindowMessages.h"
#include "../common/mptStringBuffer.h"
#include "../soundlib/mod_specifications.h"
#include "../soundlib/plugins/PlugInterface.h"

OPENMPT_NAMESPACE_BEGIN

BEGIN_MESSAGE_MAP(CCtrlPlugins, CModControlDlg)
	//{{AFX_MSG_MAP(CCtrlPlugins)
	ON_WM_HSCROLL()
	ON_WM_VSCROLL()
	ON_WM_DESTROY()

	ON_WM_ACTIVATE()
	ON_COMMAND(IDC_CHECK9,       &CCtrlPlugins::OnMixModeChanged)
	ON_COMMAND(IDC_CHECK10,      &CCtrlPlugins::OnBypassChanged)
	ON_COMMAND(IDC_CHECK11,      &CCtrlPlugins::OnDryMixChanged)
	ON_COMMAND(IDC_BUTTON1,      &CCtrlPlugins::OnSelectPlugin)
	ON_COMMAND(IDC_DELPLUGIN,    &CCtrlPlugins::OnRemovePlugin)
	ON_COMMAND(IDC_BUTTON2,      &CCtrlPlugins::OnEditPlugin)
	ON_COMMAND(IDC_BUTTON4,      &CCtrlPlugins::OnNextPlugin)
	ON_COMMAND(IDC_BUTTON5,      &CCtrlPlugins::OnPrevPlugin)
	ON_COMMAND(IDC_MOVEFXSLOT,   &CCtrlPlugins::OnMovePlugToSlot)
	ON_COMMAND(IDC_INSERTFXSLOT, &CCtrlPlugins::OnInsertSlot)
	ON_COMMAND(IDC_CLONEPLUG,    &CCtrlPlugins::OnClonePlug)

	ON_COMMAND(IDC_BUTTON6, &CCtrlPlugins::OnLoadParam)
	ON_COMMAND(IDC_BUTTON8, &CCtrlPlugins::OnSaveParam)

	ON_EN_UPDATE(IDC_EDIT13,     &CCtrlPlugins::OnPluginNameChanged)
	ON_EN_UPDATE(IDC_EDIT14,     &CCtrlPlugins::OnSetParameter)
	ON_EN_SETFOCUS(IDC_EDIT14,   &CCtrlPlugins::OnFocusParam)
	ON_EN_KILLFOCUS(IDC_EDIT14,  &CCtrlPlugins::OnParamChanged)
	ON_CBN_SELCHANGE(IDC_COMBO5, &CCtrlPlugins::OnPluginChanged)

	ON_CBN_SELCHANGE(IDC_COMBO6, &CCtrlPlugins::OnParamChanged)
	ON_CBN_SETFOCUS(IDC_COMBO6,  &CCtrlPlugins::OnFillParamCombo)

	ON_CBN_SELCHANGE(IDC_COMBO7, &CCtrlPlugins::OnOutputRoutingChanged)

	ON_CBN_SELCHANGE(IDC_COMBO8, &CCtrlPlugins::OnProgramChanged)
	ON_CBN_SETFOCUS(IDC_COMBO8,  &CCtrlPlugins::OnFillProgramCombo)

	ON_COMMAND(IDC_CHECK12, &CCtrlPlugins::OnWetDryExpandChanged)
	ON_COMMAND(IDC_CHECK13, &CCtrlPlugins::OnAutoSuspendChanged)
	ON_CBN_SELCHANGE(IDC_COMBO9, &CCtrlPlugins::OnSpecialMixProcessingChanged)

	ON_MESSAGE(WM_MOD_PLUGPARAMAUTOMATE,        &CCtrlPlugins::OnParamAutomated)
	ON_MESSAGE(WM_MOD_PLUGINDRYWETRATIOCHANGED, &CCtrlPlugins::OnDryWetRatioChangedFromPlayer)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


void CCtrlPlugins::DoDataExchange(CDataExchange* pDX)
{
	CModControlDlg::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CCtrlPlugins)
	DDX_Control(pDX, IDC_LIST1,  m_ChannelList);
	DDX_Control(pDX, IDC_COMBO5, m_CbnPlugin);
	DDX_Control(pDX, IDC_COMBO6, m_CbnParam);
	DDX_Control(pDX, IDC_COMBO7, m_CbnOutput);

	DDX_Control(pDX, IDC_COMBO8, m_CbnPreset);
	DDX_Control(pDX, IDC_COMBO9, m_CbnSpecialMixProcessing);
	DDX_Control(pDX, IDC_SPIN10, m_SpinMixGain);

	DDX_Control(pDX, IDC_SLIDER9,  m_sbValue);
	DDX_Control(pDX, IDC_SLIDER10, m_sbDryRatio);
	DDX_Control(pDX, IDC_BUTTON1,  m_BtnSelect);
	DDX_Control(pDX, IDC_BUTTON2,  m_BtnEdit);
	DDX_Control(pDX, IDC_BUTTON4,  m_nextPluginButton);
	DDX_Control(pDX, IDC_BUTTON5,  m_prevPluginButton);
	//}}AFX_DATA_MAP
}


CCtrlPlugins::CCtrlPlugins(CModControlView &parent, CModDoc &document) : CModControlDlg{parent, document}
{
	m_prevPluginButton.SetAccessibleText(_T("Previous Plugin"));
	m_nextPluginButton.SetAccessibleText(_T("Next Plugin"));
	m_nLockCount = 1;
}


CRuntimeClass *CCtrlPlugins::GetAssociatedViewClass()
{
	return RUNTIME_CLASS(CViewPlugins);
}


BOOL CCtrlPlugins::OnInitDialog()
{
	CModControlDlg::OnInitDialog();

	m_sbValue.SetPos(0);
	m_sbValue.SetRange(0, 100);

	m_sbValue.SetPos(0);
	m_sbValue.SetRange(0, 100);

	m_CbnSpecialMixProcessing.AddString(_T("Default"));
	m_CbnSpecialMixProcessing.AddString(_T("Wet Subtract"));
	m_CbnSpecialMixProcessing.AddString(_T("Dry Subtract"));
	m_CbnSpecialMixProcessing.AddString(_T("Mix Subtract"));
	m_CbnSpecialMixProcessing.AddString(_T("Middle Subtract"));
	m_CbnSpecialMixProcessing.AddString(_T("LR Balance"));
	m_CbnSpecialMixProcessing.AddString(_T("Instrument"));
	m_SpinMixGain.SetRange(0, 80);
	m_SpinMixGain.SetPos(10);
	SetDlgItemText(IDC_EDIT16, _T("Gain: x1.0"));

	const CListCtrlEx::Header headers[] =
	{
		{ _T("Output"),		50,		LVCFMT_LEFT },
		{ _T("To Plugin"),	120,	LVCFMT_LEFT },
		{ _T("Input"),		50,		LVCFMT_LEFT },
	};
	m_ChannelList.SetHeaders(headers);
	m_ChannelList.SetExtendedStyle(m_ChannelList.GetExtendedStyle() | LVS_EX_FULLROWSELECT);

	UpdateView(UpdateHint().ModType());
	OnParamChanged();
	m_nLockCount = 0;

	return TRUE;
}


void CCtrlPlugins::OnActivatePage(LPARAM lParam)
{
	// Initial Update
	if(!m_initialized)
		UpdateView(UpdateHint().ModType());
	CChildFrame *pFrame = static_cast<CChildFrame *>(GetParentFrame());
	if ((pFrame) && (m_hWndView)) PostViewMessage(VIEWMSG_LOADSTATE, reinterpret_cast<LPARAM>(&pFrame->GetPluginViewState()));
	SwitchToView();

	// Combo boxes randomly disappear without this... why?
	Invalidate();
	CModControlDlg::OnActivatePage(lParam);
}


void CCtrlPlugins::OnDeactivatePage()
{
	CChildFrame *pFrame = static_cast<CChildFrame *>(GetParentFrame());
	if((pFrame) && (m_hWndView)) SendViewMessage(VIEWMSG_SAVESTATE, reinterpret_cast<LPARAM>(&pFrame->GetPluginViewState()));
	CModControlDlg::OnDeactivatePage();
}


Setting<LONG> &CCtrlPlugins::GetSplitPosRef() { return TrackerSettings::Instance().glPluginWindowHeight; }


int CCtrlPlugins::GetDlgItemIntEx(UINT nID)
{
	CString s;
	GetDlgItemText(nID, s);
	if(s.GetLength() < 1 || s[0] < _T('0') || s[0] > _T('9')) return -1;
	return _ttoi(s);
}


void CCtrlPlugins::UpdateView(UpdateHint hint, CObject *pObj)
{
	if(pObj == this)
		return;
	if(m_nCurrentPlugin >= MAX_MIXPLUGINS)
		m_nCurrentPlugin = 0;

	const PluginHint plugHint = hint.ToType<PluginHint>();
	if(!plugHint.GetType()[HINT_MODTYPE | HINT_MIXPLUGINS | HINT_PLUGINNAMES | HINT_PLUGINPARAM])
		return;

	const PLUGINDEX updatePlug =  plugHint.GetPlugin();
	const bool updateThisPlug = updatePlug == 0 || (updatePlug - 1) == m_nCurrentPlugin;

	// Update plugin names
	if(plugHint.GetType()[HINT_MODTYPE | HINT_PLUGINNAMES])
	{
		m_CbnPlugin.Update(PluginComboBox::Config{PluginComboBox::ShowEmptySlots | PluginComboBox::ShowLibraryNames}.Hint(hint, pObj).CurrentSelection(m_nCurrentPlugin), m_sndFile);
		if(updateThisPlug)
			SetDlgItemText(IDC_EDIT13, mpt::ToWin(m_sndFile.m_MixPlugins[m_nCurrentPlugin].GetName()).c_str());
	}

	CString s;
	const SNDMIXPLUGIN &plugin = m_sndFile.m_MixPlugins[m_nCurrentPlugin];
	const bool updateWholePluginView = plugHint.GetType()[HINT_MODTYPE | HINT_MIXPLUGINS] && updateThisPlug;
	if(updateWholePluginView)
	{
		CheckDlgButton(IDC_CHECK9, plugin.IsMasterEffect() ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(IDC_CHECK10, plugin.IsBypassed() ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(IDC_CHECK11, plugin.IsDryMix() ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(IDC_CHECK13, plugin.IsAutoSuspendable() ? BST_CHECKED : BST_UNCHECKED);
		IMixPlugin *pPlugin = plugin.pMixPlugin;
		m_BtnEdit.EnableWindow((pPlugin != nullptr && (pPlugin->HasEditor() || pPlugin->GetNumVisibleParameters())) ? TRUE : FALSE);
		GetDlgItem(IDC_MOVEFXSLOT)->EnableWindow((pPlugin) ? TRUE : FALSE);
		GetDlgItem(IDC_INSERTFXSLOT)->EnableWindow((pPlugin) ? TRUE : FALSE);
		GetDlgItem(IDC_CLONEPLUG)->EnableWindow((pPlugin) ? TRUE : FALSE);
		GetDlgItem(IDC_DELPLUGIN)->EnableWindow((plugin.IsValidPlugin() || !plugin.Info.szLibraryName.empty() || !plugin.Info.szName.empty()) ? TRUE : FALSE);
		UpdateDryWetDisplay();

		m_CbnSpecialMixProcessing.SetCurSel(static_cast<int>(plugin.GetMixMode()));
		CheckDlgButton(IDC_CHECK12, plugin.IsExpandedMix() ? BST_CHECKED : BST_UNCHECKED);
		int gain = plugin.GetGain();
		if(gain == 0) gain = 10;
		s.Format(_T("Gain: x%u.%u"), gain / 10u, gain % 10u);
		SetDlgItemText(IDC_EDIT16, s);
		m_SpinMixGain.SetPos(gain);

		m_ChannelList.DeleteAllItems();
		for(size_t i = 0; i < plugin.outputs.size(); i++)
		{
			// TODO: nicer names, e.g. "Left 1, Right 1, Left 2, ..."
			m_ChannelList.InsertItem(i, mpt::cfmt::val(plugin.outputs[i].outChannel + 1));
			m_ChannelList.SetItemText(i, 2, mpt::cfmt::val(plugin.outputs[i].inChannel + 1));
			const PLUGINDEX outPlug = plugin.outputs[i].plugin;
			if(outPlug < MAX_MIXPLUGINS)
			{
				m_ChannelList.SetItemText(i, 1, mpt::ToWin(m_sndFile.m_MixPlugins[outPlug].GetName()).c_str());
			} else if(outPlug == PLUGINDEX_INVALID)
			{
				m_ChannelList.SetItemText(i, 1, _T("Master"));
			}
		}

		if (pPlugin)
		{
			const PlugParamIndex nParams = pPlugin->GetNumVisibleParameters();
			m_CbnParam.SetRedraw(FALSE);
			m_CbnParam.ResetContent();
			if (m_nCurrentParam >= nParams) m_nCurrentParam = 0;

			if(nParams)
			{
				m_CbnParam.SetItemData(m_CbnParam.AddString(pPlugin->GetFormattedParamName(m_nCurrentParam)), m_nCurrentParam);
			}

			m_CbnParam.SetCurSel(0);
			m_CbnParam.SetRedraw(TRUE);
			OnParamChanged();

			// Input / Output type
			int in = pPlugin->GetNumInputChannels(), out = pPlugin->GetNumOutputChannels();
			switch(in)
			{
			case 0:s = _T("No input"); break;
			case 1:s = _T("Mono-In"); break;
			case 2:s = _T("Stereo-In"); break;
			default:s = _T("Multi-In"); break;
			}
			s += _T(", ");
			switch(out)
			{
			case 0:s += _T("No output"); break;
			case 1:s += _T("Mono-Out"); break;
			case 2:s += _T("Stereo-Out"); break;
			default:s += _T("Multi-Out"); break;
			}

			// For now, only display the "current" preset.
			// This prevents the program from hanging when switching between plugin slots or
			// switching to the general tab and the first plugin in the list has a lot of presets.
			// Some plugins like Synth1 have so many presets that this *does* indeed make a difference,
			// even on fairly modern CPUs. The rest of the presets are just added when the combo box
			// gets the focus, i.e. just when they're needed.
			int32 currentProg = pPlugin->GetCurrentProgram();
			FillPluginProgramBox(currentProg, currentProg);
			m_CbnPreset.SetCurSel(0);

			m_sbValue.EnableWindow(TRUE);
			m_sbDryRatio.EnableWindow(TRUE);
			GetDlgItem(IDC_EDIT14)->EnableWindow(TRUE);
		} else
		{
			s.Empty();
			if (m_CbnParam.GetCount() > 0) m_CbnParam.ResetContent();
			m_nCurrentParam = 0;

			m_CbnPreset.SetRedraw(FALSE);
			m_CbnPreset.ResetContent();
			m_CbnPreset.SetItemData(m_CbnPreset.AddString(_T("none")), 0);
			m_CbnPreset.SetRedraw(TRUE);
			m_CbnPreset.SetCurSel(0);
			m_sbValue.EnableWindow(FALSE);
			m_sbDryRatio.EnableWindow(FALSE);
			GetDlgItem(IDC_EDIT14)->EnableWindow(FALSE);
		}
		SetDlgItemText(IDC_TEXT6, s);
	}

	if(updateWholePluginView || updatePlug > m_nCurrentPlugin)
	{
		int insertAt = 1;
		m_CbnOutput.SetRedraw(FALSE);
		if(updateWholePluginView)
		{
			m_CbnOutput.ResetContent();
			m_CbnOutput.SetItemData(m_CbnOutput.AddString(_T("Default")), 0);

			for(PLUGINDEX i = m_nCurrentPlugin + 1; i < MAX_MIXPLUGINS; i++)
			{
				if(!m_sndFile.m_MixPlugins[i].IsValidPlugin())
				{
					m_CbnOutput.SetItemData(m_CbnOutput.AddString(_T("New Plugin...")), 1);
					insertAt = 2;
					break;
				}
			}
		} else
		{
			const DWORD_PTR changedPlugin = 0x80 + (updatePlug - 1);
			const int items               = m_CbnOutput.GetCount();
			for(insertAt = 1; insertAt < items; insertAt++)
			{
				DWORD_PTR thisPlugin = m_CbnOutput.GetItemData(insertAt);
				if(thisPlugin == changedPlugin)
					m_CbnOutput.DeleteString(insertAt);
				if(thisPlugin >= changedPlugin)
					break;
			}
		}

		int outputSel = plugin.IsOutputToMaster() ? 0 : -1;
		for(PLUGINDEX iOut = m_nCurrentPlugin + 1; iOut < MAX_MIXPLUGINS; iOut++)
		{
			if(!updateWholePluginView && (iOut + 1) != updatePlug)
				continue;
			auto &outPlug = m_sndFile.m_MixPlugins[iOut];
			if(outPlug.IsValidPlugin())
			{
				const auto name = outPlug.GetName(), libName = outPlug.GetLibraryName();
				s.Format(_T("FX%d: "), iOut + 1);
				s += mpt::ToCString(name.empty() ? libName : name);
				if(!name.empty() && libName != name)
				{
					s += _T(" (");
					s += mpt::ToCString(libName);
					s += _T(")");
				}

				insertAt = m_CbnOutput.InsertString(insertAt, s);
				m_CbnOutput.SetItemData(insertAt, 0x80 + iOut);
				if(!plugin.IsOutputToMaster() && (plugin.GetOutputPlugin() == iOut))
				{
					outputSel = insertAt;
				}
				insertAt++;
			}
		}
		m_CbnOutput.SetRedraw(TRUE);
		if(outputSel >= 0)
			m_CbnOutput.SetCurSel(outputSel);
	}
	if(plugHint.GetType()[HINT_PLUGINPARAM] && updatePlug)
	{
		OnParamChanged();
	}

	m_CbnPlugin.Invalidate(FALSE);
	m_CbnParam.Invalidate(FALSE);
	m_CbnPreset.Invalidate(FALSE);
	m_CbnSpecialMixProcessing.Invalidate(FALSE);
	m_CbnOutput.Invalidate(FALSE);
}


IMixPlugin *CCtrlPlugins::GetCurrentPlugin() const
{
	if(m_nCurrentPlugin >= MAX_MIXPLUGINS)
	{
		return nullptr;
	}

	return m_sndFile.m_MixPlugins[m_nCurrentPlugin].pMixPlugin;
}


void CCtrlPlugins::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	CModControlDlg::OnHScroll(nSBCode, nPos, pScrollBar);

	if (!IsLocked())
	{
		LockControls();

		if ((pScrollBar) && (pScrollBar->m_hWnd == m_sbDryRatio.m_hWnd))
		{
			int n = 100 - m_sbDryRatio.GetPos();
			if ((n >= 0) && (n <= 100) && (m_nCurrentPlugin < MAX_MIXPLUGINS))
			{
				SNDMIXPLUGIN &plugin = m_sndFile.m_MixPlugins[m_nCurrentPlugin];

				if(plugin.pMixPlugin)
				{
					plugin.fDryRatio = static_cast<float>(n) / 100.0f;
					SetModified();
				}
				UpdateDryWetDisplay();
			}
		}

		UnlockControls();

		if ((pScrollBar) && (pScrollBar->m_hWnd == m_sbValue.m_hWnd))
		{
			int n = (short int)m_sbValue.GetPos();
			if ((n >= 0) && (n <= 100) && (m_nCurrentPlugin < MAX_MIXPLUGINS))
			{
				IMixPlugin *pPlugin = GetCurrentPlugin();
				if(pPlugin != nullptr)
				{
					const PlugParamIndex nParams = pPlugin->GetNumVisibleParameters();
					if(m_nCurrentParam < nParams)
					{
						if (nSBCode == SB_THUMBPOSITION || nSBCode == SB_THUMBTRACK || nSBCode == SB_ENDSCROLL)
						{
							pPlugin->SetScaledUIParam(m_nCurrentParam, 0.01f * n);
							OnParamChanged();
							SetModified();
						}
					}
				}
			}
		}
	}
}


void CCtrlPlugins::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if(m_nCurrentPlugin >= MAX_MIXPLUGINS) return;

	if(nSBCode != SB_ENDSCROLL && pScrollBar && pScrollBar == (CScrollBar*)&m_SpinMixGain)
	{

		SNDMIXPLUGIN &plugin = m_sndFile.m_MixPlugins[m_nCurrentPlugin];

		if(plugin.pMixPlugin)
		{
			uint8 gain = (uint8)nPos;
			if(gain == 0) gain = 1;

			plugin.SetGain(gain);

			TCHAR s[32];
			_stprintf(s, _T("Gain: x%u.%u"), gain / 10u, gain % 10u);
			SetDlgItemText(IDC_EDIT16, s);
			SetModified();
		}
	}

	CModControlDlg::OnVScroll(nSBCode, nPos, pScrollBar);
}


void CCtrlPlugins::OnPluginNameChanged()
{
	if(m_nCurrentPlugin < MAX_MIXPLUGINS)
	{
		SNDMIXPLUGIN &plugin = m_sndFile.m_MixPlugins[m_nCurrentPlugin];

		CString s;
		GetDlgItemText(IDC_EDIT13, s);
		if(s != mpt::ToCString(plugin.GetName()))
		{
			plugin.Info.szName = mpt::ToCharset(mpt::Charset::Locale, s);
			const auto updateHint = PluginHint(m_nCurrentPlugin + 1).Names();
			SetModified(updateHint);

			IMixPlugin *pPlugin = plugin.pMixPlugin;
			if(pPlugin != nullptr && pPlugin->GetEditor() != nullptr)
			{
				pPlugin->GetEditor()->SetTitle();
			}
			m_CbnPlugin.Update(PluginComboBox::Config{ updateHint }, m_sndFile);
		}
	}
}


void CCtrlPlugins::OnPrevPlugin()
{
	if(m_nCurrentPlugin > 0)
	{
		m_nCurrentPlugin--;
		UpdateView(PluginHint(m_nCurrentPlugin + 1).Info().Names());
		SendViewMessage(VIEWMSG_SETCURRENTPLUGIN, m_nCurrentPlugin);
	}
}


void CCtrlPlugins::OnNextPlugin()
{
	if(m_nCurrentPlugin < MAX_MIXPLUGINS - 1)
	{
		m_nCurrentPlugin++;
		UpdateView(PluginHint(m_nCurrentPlugin + 1).Info().Names());
		SendViewMessage(VIEWMSG_SETCURRENTPLUGIN, m_nCurrentPlugin);
	}
}


void CCtrlPlugins::OnPluginChanged()
{
	const PLUGINDEX plugin = m_CbnPlugin.GetSelectionPlugin().value_or(PLUGINDEX_INVALID);
	if(plugin != PLUGINDEX_INVALID)
	{
		m_nCurrentPlugin = plugin;
		UpdateView(PluginHint(m_nCurrentPlugin + 1).Info().Names());
		SendViewMessage(VIEWMSG_SETCURRENTPLUGIN, m_nCurrentPlugin);
	}
	m_CbnPreset.SetCurSel(0);
}


void CCtrlPlugins::OnSelectPlugin()
{
#ifndef NO_PLUGINS
	if(m_nCurrentPlugin < MAX_MIXPLUGINS)
	{
		CSelectPluginDlg dlg(&m_modDoc, m_nCurrentPlugin, this);
		if (dlg.DoModal() == IDOK)
		{
			SetModified();
		}
		OnPluginChanged();
		OnParamChanged();
	}
#endif // NO_PLUGINS
}


void CCtrlPlugins::OnRemovePlugin()
{
#ifndef NO_PLUGINS
	if(m_nCurrentPlugin < MAX_MIXPLUGINS && Reporting::Confirm(MPT_UFORMAT("Remove plugin FX{}: {}?")(m_nCurrentPlugin + 1, m_sndFile.m_MixPlugins[m_nCurrentPlugin].GetName()), false, true) == cnfYes)
	{
		if(m_modDoc.RemovePlugin(m_nCurrentPlugin))
		{
			OnPluginChanged();
			OnParamChanged();
		}
	}
#endif  // NO_PLUGINS
}


LRESULT CCtrlPlugins::OnParamAutomated(WPARAM plugin, LPARAM param)
{
	if(plugin == m_nCurrentPlugin && static_cast<PlugParamIndex>(param) == m_nCurrentParam)
	{
		OnParamChanged();
	}
	return 0;
}


LRESULT CCtrlPlugins::OnDryWetRatioChangedFromPlayer(WPARAM plugin, LPARAM)
{
	if(plugin == m_nCurrentPlugin)
	{
		UpdateDryWetDisplay();
	}
	return 0;
}


void CCtrlPlugins::OnParamChanged()
{
	PlugParamIndex cursel = static_cast<PlugParamIndex>(m_CbnParam.GetItemData(m_CbnParam.GetCurSel()));

	IMixPlugin *pPlugin = GetCurrentPlugin();
	if(pPlugin != nullptr && cursel != static_cast<PlugParamIndex>(CB_ERR))
	{
		const PlugParamIndex nParams = pPlugin->GetNumVisibleParameters();
		if(cursel < nParams) m_nCurrentParam = cursel;
		if(m_nCurrentParam < nParams)
		{
			const auto value = pPlugin->GetScaledUIParam(m_nCurrentParam);
			int intValue = mpt::saturate_round<int>(value * 100.0f);
			LockControls();
			if(GetFocus() != GetDlgItem(IDC_EDIT14))
			{
				CString s = pPlugin->GetFormattedParamValue(m_nCurrentParam).Trim();
				if(s.IsEmpty())
				{
					s.Format(_T("%f"), value);
				}
				SetDlgItemText(IDC_EDIT14, s);
			}
			m_sbValue.SetPos(intValue);
			UnlockControls();
			return;
		}
	}
	SetDlgItemText(IDC_EDIT14, _T(""));
	m_sbValue.SetPos(0);
}


// When focussing the parameter value, show its real value to edit
void CCtrlPlugins::OnFocusParam()
{
	IMixPlugin *pPlugin = GetCurrentPlugin();
	if(pPlugin != nullptr)
	{
		const PlugParamIndex nParams = pPlugin->GetNumVisibleParameters();
		if(m_nCurrentParam < nParams)
		{
			TCHAR s[32];
			float fValue = pPlugin->GetScaledUIParam(m_nCurrentParam);
			_stprintf(s, _T("%f"), fValue);
			LockControls();
			SetDlgItemText(IDC_EDIT14, s);
			UnlockControls();
		}
	}
}


void CCtrlPlugins::OnProgramChanged()
{
	int32 curProg = m_CbnPreset.GetItemData(m_CbnPreset.GetCurSel());

	IMixPlugin *pPlugin = GetCurrentPlugin();

	if(pPlugin != nullptr)
	{
		const int32 numProgs = pPlugin->GetNumPrograms();
		if(curProg <= numProgs)
		{
			pPlugin->SetCurrentProgram(curProg);
			// Update parameter display
			OnParamChanged();

			SetModified();
		}
	}
}


void CCtrlPlugins::OnLoadParam()
{
	IMixPlugin *pPlugin = GetCurrentPlugin();
	if(pPlugin != nullptr && pPlugin->LoadProgram())
	{
		int32 currentProg = pPlugin->GetCurrentProgram();
		FillPluginProgramBox(currentProg, currentProg);
		m_CbnPreset.SetCurSel(0);
	}
}

void CCtrlPlugins::OnSaveParam()
{
	IMixPlugin *pPlugin = GetCurrentPlugin();
	if(pPlugin != nullptr)
	{
		pPlugin->SaveProgram();
	}
}


void CCtrlPlugins::OnSetParameter()
{
	if(m_nCurrentPlugin >= MAX_MIXPLUGINS || IsLocked()) return;
	IMixPlugin *pPlugin = GetCurrentPlugin();

	if(pPlugin != nullptr)
	{
		const PlugParamIndex nParams = pPlugin->GetNumVisibleParameters();
		TCHAR s[32];
		GetDlgItemText(IDC_EDIT14, s, static_cast<int>(std::size(s)));
		if ((m_nCurrentParam < nParams) && (s[0]))
		{
			float fValue = (float)_tstof(s);
			pPlugin->SetScaledUIParam(m_nCurrentParam, fValue);
			OnParamChanged();
			SetModified();
		}
	}
}


void CCtrlPlugins::UpdateDryWetDisplay()
{
	SNDMIXPLUGIN &plugin = m_sndFile.m_MixPlugins[m_nCurrentPlugin];
	float wetRatio = 1.0f - plugin.fDryRatio, dryRatio = plugin.fDryRatio;
	m_sbDryRatio.SetPos(mpt::saturate_round<int>(wetRatio * 100));
	if(plugin.IsExpandedMix())
	{
		wetRatio = 2.0f * wetRatio - 1.0f;
		dryRatio = -wetRatio;
	}
	int wetInt = mpt::saturate_round<int>(wetRatio * 100), dryInt = mpt::saturate_round<int>(dryRatio * 100);
	SetDlgItemText(IDC_STATIC8, MPT_TFORMAT("{}% wet, {}% dry")(wetInt, dryInt).c_str());
}


void CCtrlPlugins::OnMixModeChanged()
{
	if(m_nCurrentPlugin >= MAX_MIXPLUGINS) return;
	m_sndFile.m_MixPlugins[m_nCurrentPlugin].SetMasterEffect(IsDlgButtonChecked(IDC_CHECK9) != BST_UNCHECKED);
	SetModified();
}


void CCtrlPlugins::OnBypassChanged()
{
	if(m_nCurrentPlugin >= MAX_MIXPLUGINS)
		return;

	auto &mixPlugs = m_sndFile.m_MixPlugins;
	auto &currentPlug = m_sndFile.m_MixPlugins[m_nCurrentPlugin];
	bool bypass = IsDlgButtonChecked(IDC_CHECK10) != BST_UNCHECKED;
	const bool bypassOthers = CInputHandler::ShiftPressed();
	const bool bypassOnlyMasterPlugs = CInputHandler::CtrlPressed();
	if(bypassOthers || bypassOnlyMasterPlugs)
	{
		CheckDlgButton(IDC_CHECK10, currentPlug.IsBypassed() ? BST_CHECKED : BST_UNCHECKED);
		uint8 state = 0;
		for(const auto &plug : mixPlugs)
		{
			if(!plug.pMixPlugin || &plug == &currentPlug)
				continue;
			if(!plug.IsMasterEffect() && bypassOnlyMasterPlugs)
				continue;
			if(plug.IsBypassed())
				state |= 1;
			else
				state |= 2;
			if(state == 3)
				break;
		}
		if(!state)
			return;

		bypass = (state != 1);
		for(auto &plug : mixPlugs)
		{
			if(!plug.pMixPlugin || &plug == &currentPlug)
				continue;
			if(!plug.IsMasterEffect() && bypassOnlyMasterPlugs)
				continue;
			if(plug.IsBypassed() == bypass)
				continue;
			plug.SetBypass(bypass);
			m_modDoc.UpdateAllViews(nullptr, PluginHint(plug.pMixPlugin->GetSlot() + 1).Info(), this);
		}
		if(m_sndFile.GetModSpecifications().supportsPlugins)
			m_modDoc.SetModified();
	} else
	{
		currentPlug.SetBypass(bypass);
		SetModified();
	}
}


void CCtrlPlugins::OnWetDryExpandChanged()
{
	if(m_nCurrentPlugin >= MAX_MIXPLUGINS) return;
	m_sndFile.m_MixPlugins[m_nCurrentPlugin].SetExpandedMix(IsDlgButtonChecked(IDC_CHECK12) != BST_UNCHECKED);
	UpdateDryWetDisplay();
	SetModified();
}


void CCtrlPlugins::OnAutoSuspendChanged()
{
	if(m_nCurrentPlugin >= MAX_MIXPLUGINS)
		return;

	m_sndFile.m_MixPlugins[m_nCurrentPlugin].SetAutoSuspend(IsDlgButtonChecked(IDC_CHECK13) != BST_UNCHECKED);
	SetModified();
}


void CCtrlPlugins::OnSpecialMixProcessingChanged()
{
	if(m_nCurrentPlugin >= MAX_MIXPLUGINS) return;
	m_sndFile.m_MixPlugins[m_nCurrentPlugin].SetMixMode(static_cast<PluginMixMode>(m_CbnSpecialMixProcessing.GetCurSel()));
	SetModified();
}


void CCtrlPlugins::OnDryMixChanged()
{
	if(m_nCurrentPlugin >= MAX_MIXPLUGINS) return;
	m_sndFile.m_MixPlugins[m_nCurrentPlugin].SetDryMix(IsDlgButtonChecked(IDC_CHECK11) != BST_UNCHECKED);
	SetModified();
}


void CCtrlPlugins::OnEditPlugin()
{
	if(m_nCurrentPlugin >= MAX_MIXPLUGINS) return;
	m_modDoc.TogglePluginEditor(m_nCurrentPlugin, CInputHandler::ShiftPressed());
	return;
}


void CCtrlPlugins::OnOutputRoutingChanged()
{
	if(m_nCurrentPlugin >= MAX_MIXPLUGINS) return;
	SNDMIXPLUGIN &plugin = m_sndFile.m_MixPlugins[m_nCurrentPlugin];
	DWORD_PTR nroute = m_CbnOutput.GetItemData(m_CbnOutput.GetCurSel());

	if(nroute == 1)
	{
		// Add new plugin
		for(PLUGINDEX i = m_nCurrentPlugin + 1; i < MAX_MIXPLUGINS; i++)
		{
			if(!m_sndFile.m_MixPlugins[i].IsValidPlugin())
			{
				CSelectPluginDlg dlg(&m_modDoc, i, this);
				if(dlg.DoModal() == IDOK)
				{
					m_modDoc.UpdateAllViews(nullptr, PluginHint(m_nCurrentPlugin).Info());
					nroute = 0x80 + i;
					m_nCurrentPlugin = i;
					m_CbnPlugin.SetSelection(i);
					OnPluginChanged();
				}
				break;
			}
		}
	}

	if(!nroute)
		plugin.SetOutputToMaster();
	else
		plugin.SetOutputPlugin(static_cast<PLUGINDEX>(nroute - 0x80));

	SetModified();
}


LRESULT CCtrlPlugins::OnModCtrlMsg(WPARAM wParam, LPARAM lParam)
{
	switch(wParam)
	{
	case CTRLMSG_PLUG_SETCURRENT:
		m_CbnPlugin.SetSelection(static_cast<PLUGINDEX>(lParam));
		OnPluginChanged();
		break;

	default:
		return CModControlDlg::OnModCtrlMsg(wParam, lParam);
	}
	return 0;
}


void CCtrlPlugins::OnMovePlugToSlot()
{
	if(GetCurrentPlugin() == nullptr)
	{
		return;
	}

	// If any plugin routes its output to the current plugin, we shouldn't try to move it before that plugin...
	PLUGINDEX defaultIndex = 0;
	for(PLUGINDEX i = 0; i < m_nCurrentPlugin; i++)
	{
		if(m_sndFile.m_MixPlugins[i].GetOutputPlugin() == m_nCurrentPlugin)
		{
			defaultIndex = i + 1;
		}
	}

	std::vector<PLUGINDEX> emptySlots;
	BuildEmptySlotList(emptySlots);

	CMoveFXSlotDialog dlg(this, m_nCurrentPlugin, emptySlots, defaultIndex, false, !m_sndFile.m_MixPlugins[m_nCurrentPlugin].IsOutputToMaster());

	if(dlg.DoModal() == IDOK)
	{
		size_t toIndex = dlg.GetSlotIndex();
		do
		{
			const SNDMIXPLUGIN &curPlugin = m_sndFile.m_MixPlugins[m_nCurrentPlugin];
			SNDMIXPLUGIN &newPlugin = m_sndFile.m_MixPlugins[emptySlots[toIndex]];
			const PLUGINDEX nextPlugin = curPlugin.GetOutputPlugin();

			MovePlug(m_nCurrentPlugin, emptySlots[toIndex]);

			if(nextPlugin == PLUGINDEX_INVALID || toIndex == emptySlots.size() - 1)
			{
				break;
			}

			m_nCurrentPlugin = nextPlugin;

			if(dlg.DoMoveChain())
			{
				toIndex++;
				newPlugin.SetOutputPlugin(emptySlots[toIndex]);
			}
		} while(dlg.DoMoveChain());

		m_CbnPlugin.SetSelection(dlg.GetSlot());
		OnPluginChanged();
		m_modDoc.UpdateAllViews(nullptr, PluginHint().Names(), this);
	}
}


// Functor for adjusting plug indexes in modcommands. Adjusts all instrument column values in
// range [m_nInstrMin, m_nInstrMax] by m_nDiff.
struct PlugIndexModifier
{
	PlugIndexModifier(PLUGINDEX nMin, PLUGINDEX nMax, int nDiff) :
		m_nDiff(nDiff), m_nInstrMin(nMin), m_nInstrMax(nMax) {}
	void operator()(ModCommand& m)
	{
		if (m.IsInstrPlug() && m.instr >= m_nInstrMin && m.instr <= m_nInstrMax)
			m.instr = static_cast<ModCommand::INSTR>(m.instr + m_nDiff);
	}
	int m_nDiff;
	ModCommand::INSTR m_nInstrMin;
	ModCommand::INSTR m_nInstrMax;
};


bool CCtrlPlugins::MovePlug(PLUGINDEX src, PLUGINDEX dest, bool bAdjustPat)
{
	if (src == dest)
		return false;

	BeginWaitCursor();

	CriticalSection cs;

	// Move plug data
	m_sndFile.m_MixPlugins[dest] = std::move(m_sndFile.m_MixPlugins[src]);
	mpt::reconstruct(m_sndFile.m_MixPlugins[src]);

	m_sndFile.GetMIDIMapper().MovePlugin(src, dest);

	//Prevent plug from pointing backwards.
	if(!m_sndFile.m_MixPlugins[dest].IsOutputToMaster())
	{
		PLUGINDEX nOutput = m_sndFile.m_MixPlugins[dest].GetOutputPlugin();
		if (nOutput <= dest && nOutput != PLUGINDEX_INVALID)
		{
			m_sndFile.m_MixPlugins[dest].SetOutputToMaster();
		}
	}

	// Update current plug
	IMixPlugin *pPlugin = m_sndFile.m_MixPlugins[dest].pMixPlugin;
	if(pPlugin != nullptr)
	{
		pPlugin->SetSlot(dest);
		if(pPlugin->GetEditor() != nullptr)
		{
			pPlugin->GetEditor()->SetTitle();
		}
	}

	// Update all other plugs' outputs
	for (PLUGINDEX nPlug = 0; nPlug < src; nPlug++)
	{
		if(!m_sndFile.m_MixPlugins[nPlug].IsOutputToMaster())
		{
			if(m_sndFile.m_MixPlugins[nPlug].GetOutputPlugin() == src)
			{
				m_sndFile.m_MixPlugins[nPlug].SetOutputPlugin(dest);
			}
		}
	}
	// Update channels
	for (CHANNELINDEX nChn = 0; nChn < m_sndFile.GetNumChannels(); nChn++)
	{
		if (m_sndFile.ChnSettings[nChn].nMixPlugin == src + 1u)
		{
			m_sndFile.ChnSettings[nChn].nMixPlugin = dest + 1u;
		}
	}

	// Update instruments
	for (INSTRUMENTINDEX nIns = 1; nIns <= m_sndFile.GetNumInstruments(); nIns++)
	{
		if (m_sndFile.Instruments[nIns] && (m_sndFile.Instruments[nIns]->nMixPlug == src + 1))
		{
			m_sndFile.Instruments[nIns]->nMixPlug = dest + 1u;
		}
	}

	// Update MODCOMMANDs so that they won't be referring to old indexes (e.g. with NOTE_PC).
	if (bAdjustPat && m_sndFile.GetModSpecifications().HasNote(NOTE_PC))
		m_sndFile.Patterns.ForEachModCommand(PlugIndexModifier(src + 1, src + 1, int(dest) - int(src)));

	cs.Leave();

	SetModified();

	EndWaitCursor();

	return true;
}


void CCtrlPlugins::BuildEmptySlotList(std::vector<PLUGINDEX> &emptySlots)
{
	emptySlots.clear();

	for(PLUGINDEX nSlot = 0; nSlot < MAX_MIXPLUGINS; nSlot++)
	{
		if(m_sndFile.m_MixPlugins[nSlot].pMixPlugin == nullptr)
		{
			emptySlots.push_back(nSlot);
		}
	}
	return;
}

void CCtrlPlugins::OnInsertSlot()
{
	CString prompt;
	prompt.Format(_T("Insert empty slot before slot FX%u?"), m_nCurrentPlugin + 1);

	// If last plugin slot is occupied, move it so that the plugin is not lost.
	// This could certainly be improved...
	bool moveLastPlug = false;

	if(m_sndFile.m_MixPlugins[MAX_MIXPLUGINS - 1].pMixPlugin)
	{
		if(m_sndFile.m_MixPlugins[MAX_MIXPLUGINS - 2].pMixPlugin == nullptr)
		{
			moveLastPlug = true;
		} else
		{
			prompt.Append(_T("\nWarning: plugin data in last slot will be lost."));
		}
	}
	if(Reporting::Confirm(prompt) == cnfYes)
	{

		// Delete last plug...
		if(m_sndFile.m_MixPlugins[MAX_MIXPLUGINS - 1].pMixPlugin)
		{
			if(moveLastPlug)
			{
				MovePlug(MAX_MIXPLUGINS - 1, MAX_MIXPLUGINS - 2, true);
			} else
			{
				m_sndFile.m_MixPlugins[MAX_MIXPLUGINS - 1].Destroy();
				MemsetZero(m_sndFile.m_MixPlugins[MAX_MIXPLUGINS - 1].Info);
			}
		}

		// Update MODCOMMANDs so that they won't be referring to old indexes (e.g. with NOTE_PC).
		if(m_sndFile.GetModSpecifications().HasNote(NOTE_PC))
			m_sndFile.Patterns.ForEachModCommand(PlugIndexModifier(m_nCurrentPlugin + 1, MAX_MIXPLUGINS - 1, 1));


		for(PLUGINDEX nSlot = MAX_MIXPLUGINS - 1; nSlot > m_nCurrentPlugin; nSlot--)
		{
			if(m_sndFile.m_MixPlugins[nSlot-1].pMixPlugin)
			{
				MovePlug(nSlot - 1, nSlot, NoPatternAdjust);
			}
		}

		m_CbnPlugin.SetSelection(m_nCurrentPlugin);
		OnPluginChanged();
		m_modDoc.UpdateAllViews(nullptr, PluginHint().Names());

		SetModified();
	}
}


void CCtrlPlugins::OnClonePlug()
{
	if(GetCurrentPlugin() == nullptr)
	{
		return;
	}

	std::vector<PLUGINDEX> emptySlots;
	BuildEmptySlotList(emptySlots);

	CMoveFXSlotDialog dlg(this, m_nCurrentPlugin, emptySlots, 0, true, !m_sndFile.m_MixPlugins[m_nCurrentPlugin].IsOutputToMaster());

	if(dlg.DoModal() == IDOK)
	{
		size_t toIndex = dlg.GetSlotIndex();
		do
		{
			const SNDMIXPLUGIN &curPlugin = m_sndFile.m_MixPlugins[m_nCurrentPlugin];
			SNDMIXPLUGIN &newPlugin = m_sndFile.m_MixPlugins[emptySlots[toIndex]];

			m_modDoc.ClonePlugin(newPlugin, curPlugin);
			IMixPlugin *mixPlug = newPlugin.pMixPlugin;
			if(mixPlug != nullptr && mixPlug->IsInstrument() && m_modDoc.HasInstrumentForPlugin(emptySlots[toIndex]) == INSTRUMENTINDEX_INVALID)
			{
				m_modDoc.InsertInstrumentForPlugin(emptySlots[toIndex]);
			}

			if(curPlugin.IsOutputToMaster() || toIndex == emptySlots.size() - 1)
			{
				break;
			}

			m_nCurrentPlugin = curPlugin.GetOutputPlugin();

			if(dlg.DoMoveChain())
			{
				toIndex++;
				newPlugin.SetOutputPlugin(emptySlots[toIndex]);
			}
		} while(dlg.DoMoveChain());

		m_CbnPlugin.SetSelection(dlg.GetSlot());
		OnPluginChanged();
		m_modDoc.UpdateAllViews(nullptr, PluginHint().Names());

		SetModified();
	}
}


// The plugin param box is only filled when it gets the focus (done here).
void CCtrlPlugins::OnFillParamCombo()
{
	// no need to fill it again.
	if(m_CbnParam.GetCount() > 1)
		return;

	IMixPlugin *pPlugin = GetCurrentPlugin();
	if(pPlugin == nullptr) return;

	const PlugParamIndex nParams = pPlugin->GetNumVisibleParameters();
	m_CbnParam.SetRedraw(FALSE);
	m_CbnParam.ResetContent();

	AddPluginParameternamesToCombobox(m_CbnParam, *pPlugin);

	if (m_nCurrentParam >= nParams) m_nCurrentParam = 0;
	m_CbnParam.SetCurSel(m_nCurrentParam);
	m_CbnParam.SetRedraw(TRUE);
	m_CbnParam.Invalidate(FALSE);
}


// The preset box is only filled when it gets the focus (done here).
void CCtrlPlugins::OnFillProgramCombo()
{
	// no need to fill it again.
	if(m_CbnPreset.GetCount() > 1)
		return;

	IMixPlugin *pPlugin = GetCurrentPlugin();
	if(pPlugin == nullptr) return;

	FillPluginProgramBox(0, pPlugin->GetNumPrograms() - 1);
	m_CbnPreset.SetCurSel(pPlugin->GetCurrentProgram());
}


void CCtrlPlugins::FillPluginProgramBox(int32 firstProg, int32 lastProg)
{
	IMixPlugin *pPlugin = GetCurrentPlugin();

	m_CbnPreset.SetRedraw(FALSE);
	m_CbnPreset.ResetContent();

	pPlugin->CacheProgramNames(firstProg, lastProg + 1);
	for (int32 i = firstProg; i <= lastProg; i++)
	{
		m_CbnPreset.SetItemData(m_CbnPreset.AddString(pPlugin->GetFormattedProgramName(i)), i);
	}

	m_CbnPreset.SetRedraw(TRUE);
	m_CbnPreset.Invalidate(FALSE);
}


void CCtrlPlugins::SetModified(PluginHint hint)
{
	if(m_sndFile.GetModSpecifications().supportsPlugins)
		m_modDoc.SetModified();

	m_modDoc.UpdateAllViews(nullptr, hint.SetData(m_nCurrentPlugin + 1u), /*updateAll ? nullptr : */this);
}


CString CCtrlPlugins::GetToolTipText(UINT id, HWND) const
{
	CString s;
	switch(id)
	{
	case IDC_EDIT16:
		{
			const auto gain = m_sndFile.m_MixPlugins[m_nCurrentPlugin].GetGain();
			s = CModDoc::LinearToDecibelsString(gain ? gain : 10, 10.0);
		}
	case IDC_BUTTON5:
		s = _T("Previous Plugin");
	case IDC_BUTTON4:
		s = _T("Next Plugin");
	}
	return s;
}


OPENMPT_NAMESPACE_END
