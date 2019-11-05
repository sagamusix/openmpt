/*
 * view_gen.cpp
 * ------------
 * Purpose: General tab, lower panel.
 * Notes  : (currently none)
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "Mptrack.h"
#include "Mainfrm.h"
#include "InputHandler.h"
#include "Childfrm.h"
#include "Moddoc.h"
#include "Globals.h"
#include "Ctrl_gen.h"
#include "View_gen.h"
#include "EffectVis.h"
#include "ChannelManagerDlg.h"
#include "../soundlib/mod_specifications.h"
#include "../common/mptStringBuffer.h"

// This is used for retrieving the correct background colour for the
// frames on the general tab when using WinXP Luna or Vista/Win7 Aero.
#include <uxtheme.h>


OPENMPT_NAMESPACE_BEGIN


IMPLEMENT_SERIAL(CViewGlobals, CFormView, 0)

BEGIN_MESSAGE_MAP(CViewGlobals, CFormView)
	//{{AFX_MSG_MAP(CViewGlobals)
	ON_WM_SIZE()
	ON_WM_HSCROLL()
	ON_WM_DESTROY()
	
	ON_MESSAGE(WM_MOD_MDIACTIVATE,   &CViewGlobals::OnMDIDeactivate)
	ON_MESSAGE(WM_MOD_MDIDEACTIVATE, &CViewGlobals::OnMDIDeactivate)

	ON_COMMAND(IDC_CHECK1,		&CViewGlobals::OnMute1)
	ON_COMMAND(IDC_CHECK3,		&CViewGlobals::OnMute2)
	ON_COMMAND(IDC_CHECK5,		&CViewGlobals::OnMute3)
	ON_COMMAND(IDC_CHECK7,		&CViewGlobals::OnMute4)
	ON_COMMAND(IDC_CHECK2,		&CViewGlobals::OnSurround1)
	ON_COMMAND(IDC_CHECK4,		&CViewGlobals::OnSurround2)
	ON_COMMAND(IDC_CHECK6,		&CViewGlobals::OnSurround3)
	ON_COMMAND(IDC_CHECK8,		&CViewGlobals::OnSurround4)
	ON_COMMAND(IDC_BUTTON9,		&CViewGlobals::OnEditColor1)
	ON_COMMAND(IDC_BUTTON10,	&CViewGlobals::OnEditColor2)
	ON_COMMAND(IDC_BUTTON11,	&CViewGlobals::OnEditColor3)
	ON_COMMAND(IDC_BUTTON12,	&CViewGlobals::OnEditColor4)

	ON_EN_UPDATE(IDC_EDIT1,		&CViewGlobals::OnEditVol1)
	ON_EN_UPDATE(IDC_EDIT3,		&CViewGlobals::OnEditVol2)
	ON_EN_UPDATE(IDC_EDIT5,		&CViewGlobals::OnEditVol3)
	ON_EN_UPDATE(IDC_EDIT7,		&CViewGlobals::OnEditVol4)
	ON_EN_UPDATE(IDC_EDIT2,		&CViewGlobals::OnEditPan1)
	ON_EN_UPDATE(IDC_EDIT4,		&CViewGlobals::OnEditPan2)
	ON_EN_UPDATE(IDC_EDIT6,		&CViewGlobals::OnEditPan3)
	ON_EN_UPDATE(IDC_EDIT8,		&CViewGlobals::OnEditPan4)
	ON_EN_UPDATE(IDC_EDIT9,		&CViewGlobals::OnEditName1)
	ON_EN_UPDATE(IDC_EDIT10,	&CViewGlobals::OnEditName2)
	ON_EN_UPDATE(IDC_EDIT11,	&CViewGlobals::OnEditName3)
	ON_EN_UPDATE(IDC_EDIT12,	&CViewGlobals::OnEditName4)
	ON_CBN_SELCHANGE(IDC_COMBO1, &CViewGlobals::OnFx1Changed)
	ON_CBN_SELCHANGE(IDC_COMBO2, &CViewGlobals::OnFx2Changed)
	ON_CBN_SELCHANGE(IDC_COMBO3, &CViewGlobals::OnFx3Changed)
	ON_CBN_SELCHANGE(IDC_COMBO4, &CViewGlobals::OnFx4Changed)
	ON_NOTIFY(TCN_SELCHANGE, IDC_TABCTRL1,	&CViewGlobals::OnTabSelchange)
	ON_MESSAGE(WM_MOD_UNLOCKCONTROLS,		&CViewGlobals::OnUnlockControls)
	ON_MESSAGE(WM_MOD_VIEWMSG,	&CViewGlobals::OnModViewMsg)
	ON_MESSAGE(WM_MOD_MIDIMSG,	&CViewGlobals::OnMidiMsg)

	ON_COMMAND(ID_EDIT_UNDO, &CViewGlobals::OnEditUndo)
	ON_COMMAND(ID_EDIT_REDO, &CViewGlobals::OnEditRedo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, &CViewGlobals::OnUpdateUndo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_REDO, &CViewGlobals::OnUpdateRedo)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


void CViewGlobals::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CViewGlobals)
	DDX_Control(pDX, IDC_TABCTRL1,	m_TabCtrl);
	DDX_Control(pDX, IDC_COMBO1,	m_CbnEffects[0]);
	DDX_Control(pDX, IDC_COMBO2,	m_CbnEffects[1]);
	DDX_Control(pDX, IDC_COMBO3,	m_CbnEffects[2]);
	DDX_Control(pDX, IDC_COMBO4,	m_CbnEffects[3]);
	DDX_Control(pDX, IDC_SLIDER1,	m_sbVolume[0]);
	DDX_Control(pDX, IDC_SLIDER2,	m_sbPan[0]);
	DDX_Control(pDX, IDC_SLIDER3,	m_sbVolume[1]);
	DDX_Control(pDX, IDC_SLIDER4,	m_sbPan[1]);
	DDX_Control(pDX, IDC_SLIDER5,	m_sbVolume[2]);
	DDX_Control(pDX, IDC_SLIDER6,	m_sbPan[2]);
	DDX_Control(pDX, IDC_SLIDER7,	m_sbVolume[3]);
	DDX_Control(pDX, IDC_SLIDER8,	m_sbPan[3]);
	DDX_Control(pDX, IDC_SPIN1,		m_spinVolume[0]);
	DDX_Control(pDX, IDC_SPIN2,		m_spinPan[0]);
	DDX_Control(pDX, IDC_SPIN3,		m_spinVolume[1]);
	DDX_Control(pDX, IDC_SPIN4,		m_spinPan[1]);
	DDX_Control(pDX, IDC_SPIN5,		m_spinVolume[2]);
	DDX_Control(pDX, IDC_SPIN6,		m_spinPan[2]);
	DDX_Control(pDX, IDC_SPIN7,		m_spinVolume[3]);
	DDX_Control(pDX, IDC_SPIN8,		m_spinPan[3]);
	//}}AFX_DATA_MAP
}

void CViewGlobals::OnInitialUpdate()
{
	int nMapMode = MM_TEXT;
	SIZE sizeTotal, sizePage, sizeLine;

	m_nActiveTab = CHANNELINDEX(-1);
	CFormView::OnInitialUpdate();

	CChildFrame *pFrame = (CChildFrame *)GetParentFrame();
	if(pFrame != nullptr)
	{
		GENERALVIEWSTATE &generalState = pFrame->GetGeneralViewState();
		if (generalState.initialized)
		{
			m_TabCtrl.SetCurSel(generalState.nTab);
			m_nActiveTab = generalState.nTab;
		}
	}
	GetDeviceScrollSizes(nMapMode, sizeTotal, sizePage, sizeLine);
	m_rcClient.SetRect(0, 0, sizeTotal.cx, sizeTotal.cy);
	RecalcLayout();

	// Initializing scroll ranges
	for(int ichn = 0; ichn < CHANNELS_IN_TAB; ichn++)
	{
		// Color select
		m_channelColor[ichn].SubclassDlgItem(IDC_BUTTON9 + ichn, this);
		// Volume Slider
		m_sbVolume[ichn].SetRange(0, 64);
		m_sbVolume[ichn].SetTicFreq(8);
		// Pan Slider
		m_sbPan[ichn].SetRange(0, 64);
		m_sbPan[ichn].SetTicFreq(8);
		// Volume Spin
		m_spinVolume[ichn].SetRange(0, 64);
		// Pan Spin
		m_spinPan[ichn].SetRange(0, 256);
	}

	UpdateView(UpdateHint().ModType());
	m_nLockCount = 0;

	// Use tab background color rather than regular dialog background color (required for Aero/etc. where they are not the same color)
	EnableThemeDialogTexture(m_hWnd, ETDT_ENABLETAB);
}


void CViewGlobals::OnDestroy()
{
	CChildFrame *pFrame = (CChildFrame *)GetParentFrame();
	if(pFrame != nullptr)
	{
		GENERALVIEWSTATE &generalState = pFrame->GetGeneralViewState();
		generalState.initialized = true;
		generalState.nTab = m_nActiveTab;
	}
	CFormView::OnDestroy();
}


LRESULT CViewGlobals::OnMDIDeactivate(WPARAM, LPARAM)
{
	// Create new undo point if we switch to / from other window
	m_lastEdit = CHANNELINDEX_INVALID;
	return 0;
}


LRESULT CViewGlobals::OnMidiMsg(WPARAM midiData_, LPARAM)
{
	uint32 midiData = static_cast<uint32>(midiData_);
	// Handle MIDI messages assigned to shortcuts
	CInputHandler *ih = CMainFrame::GetInputHandler();
	ih->HandleMIDIMessage(kCtxViewGeneral, midiData) != kcNull
		|| ih->HandleMIDIMessage(kCtxAllContexts, midiData) != kcNull;
	return 1;
}


void CViewGlobals::RecalcLayout()
{
	if (m_TabCtrl.m_hWnd != NULL)
	{
		CRect rect;
		GetClientRect(&rect);
		if (rect.right < m_rcClient.right) rect.right = m_rcClient.right;
		if (rect.bottom < m_rcClient.bottom) rect.bottom = m_rcClient.bottom;
		m_TabCtrl.SetWindowPos(NULL, 0,0, rect.right, rect.bottom, SWP_NOZORDER|SWP_NOMOVE);
	}
}


int CViewGlobals::GetDlgItemIntEx(UINT nID)
{
	CString s;
	GetDlgItemText(nID, s);
	if(s.GetLength() < 1 || s[0] < _T('0') || s[0] > _T('9')) return -1;
	return _ttoi(s);
}


void CViewGlobals::OnSize(UINT nType, int cx, int cy)
{
	CFormView::OnSize(nType, cx, cy);
	if (((nType == SIZE_RESTORED) || (nType == SIZE_MAXIMIZED)) && (cx > 0) && (cy > 0) && (m_hWnd))
	{
		RecalcLayout();
	}
}


void CViewGlobals::OnUpdate(CView *pView, LPARAM lHint, CObject *pHint)
{
	if (pView != this) UpdateView(UpdateHint::FromLPARAM(lHint), pHint);
}


void CViewGlobals::UpdateView(UpdateHint hint, CObject *pObject)
{
	const CModDoc *pModDoc = GetDocument();
	int nTabCount, nTabIndex;

	if (!pModDoc || pObject == this) return;
	const CSoundFile &sndFile = pModDoc->GetSoundFile();
	const GeneralHint genHint = hint.ToType<GeneralHint>();
	const PluginHint plugHint = hint.ToType<PluginHint>();
	const bool updatePlugNames = plugHint.GetType()[HINT_PLUGINNAMES];
	if (!genHint.GetType()[HINT_MODTYPE | HINT_MODCHANNELS] && !updatePlugNames)
	{
		return;
	}
	FlagSet<HintType> hintType = hint.GetType();
	const auto updateChannel = genHint.GetChannel();
	const int updateTab = (updateChannel < sndFile.GetNumChannels()) ? (updateChannel / CHANNELS_IN_TAB) : m_nActiveTab;
	if(genHint.GetType()[HINT_MODCHANNELS] && updateTab != m_nActiveTab)
		return;

	CString s;
	nTabCount = (sndFile.m_nChannels + (CHANNELS_IN_TAB - 1)) / CHANNELS_IN_TAB;
	if (nTabCount != m_TabCtrl.GetItemCount())
	{
		UINT nOldSel = m_TabCtrl.GetCurSel();
		if (!m_TabCtrl.GetItemCount()) nOldSel = m_nActiveTab;
		m_TabCtrl.SetRedraw(FALSE);
		m_TabCtrl.DeleteAllItems();
		for (int iItem=0; iItem<nTabCount; iItem++)
		{
			const int lastItem = std::min(iItem * CHANNELS_IN_TAB + CHANNELS_IN_TAB, static_cast<int>(MAX_BASECHANNELS));
			s = MPT_CFORMAT("{} - {}")(iItem * CHANNELS_IN_TAB + 1, lastItem);
			TC_ITEM tci;
			tci.mask = TCIF_TEXT | TCIF_PARAM;
			tci.pszText = const_cast<TCHAR *>(s.GetString());
			tci.lParam = iItem * CHANNELS_IN_TAB;
			m_TabCtrl.InsertItem(iItem, &tci);
		}
		if (nOldSel >= (UINT)nTabCount) nOldSel = 0;

		m_TabCtrl.SetRedraw(TRUE);
		m_TabCtrl.SetCurSel(nOldSel);

		InvalidateRect(NULL, FALSE);
	}
	nTabIndex = m_TabCtrl.GetCurSel();
	if ((nTabIndex < 0) || (nTabIndex >= nTabCount)) return; // ???

	const bool updateTabs = m_nActiveTab != nTabIndex || genHint.GetType()[HINT_MODTYPE | HINT_MODCHANNELS];
	if(updateTabs)
	{
		LockControls();
		m_nActiveTab = static_cast<CHANNELINDEX>(nTabIndex);
		for (CHANNELINDEX ichn = 0; ichn < CHANNELS_IN_TAB; ichn++)
		{
			const CHANNELINDEX nChn = m_nActiveTab * CHANNELS_IN_TAB + ichn;
			const BOOL bEnable = (nChn < sndFile.GetNumChannels()) ? TRUE : FALSE;
			if(nChn < MAX_BASECHANNELS)
			{
				const auto &chnSettings = sndFile.ChnSettings[nChn];
				// Text
				if(bEnable)
					s = MPT_CFORMAT("Channel {}")(nChn + 1);
				else
					s = _T("");
				SetDlgItemText(IDC_TEXT1 + ichn, s);
				// Channel color
				m_channelColor[ichn].SetColor(chnSettings.color);
				m_channelColor[ichn].EnableWindow(bEnable);
				// Mute
				CheckDlgButton(IDC_CHECK1 + ichn * 2, chnSettings.dwFlags[CHN_MUTE] ? TRUE : FALSE);
				// Surround
				CheckDlgButton(IDC_CHECK2 + ichn * 2, chnSettings.dwFlags[CHN_SURROUND] ? TRUE : FALSE);
				// Volume
				int vol = chnSettings.nVolume;
				m_sbVolume[ichn].SetPos(vol);
				m_sbVolume[ichn].Invalidate(FALSE);
				SetDlgItemInt(IDC_EDIT1+ichn*2, vol);
				// Pan
				int pan = chnSettings.nPan;
				m_sbPan[ichn].SetPos(pan/4);
				m_sbPan[ichn].Invalidate(FALSE);
				SetDlgItemInt(IDC_EDIT2+ichn*2, pan);

				// Channel name
				s = mpt::ToCString(sndFile.GetCharsetInternal(), chnSettings.szName);
				SetDlgItemText(IDC_EDIT9 + ichn, s);
				((CEdit*)(GetDlgItem(IDC_EDIT9 + ichn)))->LimitText(MAX_CHANNELNAME - 1);
			}
			else
				SetDlgItemText(IDC_TEXT1 + ichn, _T(""));

			// Enable/Disable controls for this channel
			BOOL bIT = ((bEnable) && (sndFile.m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT)));
			GetDlgItem(IDC_CHECK1 + ichn * 2)->EnableWindow(bEnable);
			GetDlgItem(IDC_CHECK2 + ichn * 2)->EnableWindow(bIT);

			m_sbVolume[ichn].EnableWindow(bIT);
			m_spinVolume[ichn].EnableWindow(bIT);

			m_sbPan[ichn].EnableWindow(bEnable && !(sndFile.GetType() & (MOD_TYPE_XM|MOD_TYPE_MOD)));
			m_spinPan[ichn].EnableWindow(bEnable && !(sndFile.GetType() & (MOD_TYPE_XM|MOD_TYPE_MOD)));
			GetDlgItem(IDC_EDIT1 + ichn * 2)->EnableWindow(bIT);	// channel vol
			GetDlgItem(IDC_EDIT2 + ichn * 2)->EnableWindow(bEnable && !(sndFile.GetType() & (MOD_TYPE_XM|MOD_TYPE_MOD)));	// channel pan
			GetDlgItem(IDC_EDIT9 + ichn)->EnableWindow(((bEnable) && (sndFile.m_nType & (MOD_TYPE_XM|MOD_TYPE_IT|MOD_TYPE_MPT))));	// channel name
			m_CbnEffects[ichn].EnableWindow(bEnable & (sndFile.GetModSpecifications().supportsPlugins ? TRUE : FALSE));
		}
		UnlockControls();
	}

	// Update plugin names
	if (updateTabs || updatePlugNames)
	{
		const PLUGINDEX updatePlugin = plugHint.GetPlugin() ? (plugHint.GetPlugin() - 1u) : PLUGINDEX_INVALID;
		for(CHANNELINDEX ichn = 0; ichn < CHANNELS_IN_TAB; ichn++)
		{
			const CHANNELINDEX nChn = m_nActiveTab * CHANNELS_IN_TAB + ichn;
			if(nChn < MAX_BASECHANNELS)
			{
				m_CbnEffects[ichn].Update(sndFile, PluginChannel(sndFile.ChnSettings[nChn].nMixPlugin, sndFile.ChnSettings[nChn].pluginChannel), PluginComboBox::ShowNoPlugin | PluginComboBox::ShowInputs, updatePlugin);
				m_CbnEffects[ichn].Invalidate(FALSE);
			}
		}
	}
}


void CViewGlobals::OnTabSelchange(NMHDR*, LRESULT* pResult)
{
	UpdateView(GeneralHint().Channels());
	if (pResult) *pResult = 0;
}


void CViewGlobals::OnUpdateUndo(CCmdUI *pCmdUI)
{
	CModDoc *pModDoc = GetDocument();
	if((pCmdUI) && (pModDoc))
	{
		pCmdUI->Enable(pModDoc->GetPatternUndo().CanUndoChannelSettings());
		pCmdUI->SetText(CMainFrame::GetInputHandler()->GetKeyTextFromCommand(kcEditUndo, _T("Undo ") + pModDoc->GetPatternUndo().GetUndoName()));
	}
}


void CViewGlobals::OnUpdateRedo(CCmdUI *pCmdUI)
{
	CModDoc *pModDoc = GetDocument();
	if((pCmdUI) && (pModDoc))
	{
		pCmdUI->Enable(pModDoc->GetPatternUndo().CanRedoChannelSettings());
		pCmdUI->SetText(CMainFrame::GetInputHandler()->GetKeyTextFromCommand(kcEditRedo, _T("Redo ") + pModDoc->GetPatternUndo().GetRedoName()));
	}
}


void CViewGlobals::OnEditUndo()
{
	UndoRedo(true);
}


void CViewGlobals::OnEditRedo()
{
	UndoRedo(false);
}


void CViewGlobals::UndoRedo(bool undo)
{
	CModDoc *pModDoc = GetDocument();
	if(!pModDoc)
		return;
	if(undo && pModDoc->GetPatternUndo().CanUndoChannelSettings())
		pModDoc->GetPatternUndo().Undo();
	else if(!undo && pModDoc->GetPatternUndo().CanRedoChannelSettings())
		pModDoc->GetPatternUndo().Redo();
}


void CViewGlobals::PrepareUndo(CHANNELINDEX chnMod4)
{
	if(m_lastEdit != chnMod4)
	{
		// Backup old channel settings through pattern undo.
		m_lastEdit = chnMod4;
		const CHANNELINDEX chn = static_cast<CHANNELINDEX>(m_nActiveTab * CHANNELS_IN_TAB) + chnMod4;
		GetDocument()->GetPatternUndo().PrepareChannelUndo(chn, 1, "Channel Settings");
	}
}


void CViewGlobals::OnEditColor(const CHANNELINDEX chnMod4)
{
	auto *modDoc = GetDocument();
	auto &sndFile = modDoc->GetSoundFile();
	const CHANNELINDEX chn = static_cast<CHANNELINDEX>(m_nActiveTab * CHANNELS_IN_TAB) + chnMod4;
	if(auto color = m_channelColor[chnMod4].PickColor(sndFile, chn); color.has_value())
	{
		PrepareUndo(chnMod4);
		sndFile.ChnSettings[chn].color = *color;
		modDoc->SetModified();
		modDoc->UpdateAllViews(nullptr, GeneralHint(chn).Channels());
	}
}


void CViewGlobals::OnEditColor1() { OnEditColor(0); }
void CViewGlobals::OnEditColor2() { OnEditColor(1); }
void CViewGlobals::OnEditColor3() { OnEditColor(2); }
void CViewGlobals::OnEditColor4() { OnEditColor(3); }


void CViewGlobals::OnMute(const CHANNELINDEX chnMod4, const UINT itemID)
{
	CModDoc *pModDoc = GetDocument();

	if (pModDoc)
	{
		const bool b = (IsDlgButtonChecked(itemID) != FALSE);
		const CHANNELINDEX nChn = (CHANNELINDEX)(m_nActiveTab * CHANNELS_IN_TAB) + chnMod4;
		pModDoc->MuteChannel(nChn, b);
		pModDoc->UpdateAllViews(this, GeneralHint(nChn).Channels());
	}
}

void CViewGlobals::OnMute1() {OnMute(0, IDC_CHECK1);}
void CViewGlobals::OnMute2() {OnMute(1, IDC_CHECK3);}
void CViewGlobals::OnMute3() {OnMute(2, IDC_CHECK5);}
void CViewGlobals::OnMute4() {OnMute(3, IDC_CHECK7);}


void CViewGlobals::OnSurround(const CHANNELINDEX chnMod4, const UINT itemID)
{
	CModDoc *pModDoc = GetDocument();

	if (pModDoc)
	{
		PrepareUndo(chnMod4);
		const bool b = (IsDlgButtonChecked(itemID) != FALSE);
		const CHANNELINDEX nChn = (CHANNELINDEX)(m_nActiveTab * CHANNELS_IN_TAB) + chnMod4;
		pModDoc->SurroundChannel(nChn, b);
		pModDoc->UpdateAllViews(nullptr, GeneralHint(nChn).Channels());
	}
}

void CViewGlobals::OnSurround1() {OnSurround(0, IDC_CHECK2);}
void CViewGlobals::OnSurround2() {OnSurround(1, IDC_CHECK4);}
void CViewGlobals::OnSurround3() {OnSurround(2, IDC_CHECK6);}
void CViewGlobals::OnSurround4() {OnSurround(3, IDC_CHECK8);}

void CViewGlobals::OnEditVol(const CHANNELINDEX chnMod4, const UINT itemID)
{
	CModDoc *pModDoc = GetDocument();
	const CHANNELINDEX nChn = (CHANNELINDEX)(m_nActiveTab * CHANNELS_IN_TAB) + chnMod4;
	const int vol = GetDlgItemIntEx(itemID);
	if ((pModDoc) && (vol >= 0) && (vol <= 64) && (!m_nLockCount))
	{
		PrepareUndo(chnMod4);
		if (pModDoc->SetChannelGlobalVolume(nChn, static_cast<uint16>(vol)))
		{
			m_sbVolume[chnMod4].SetPos(vol);
			pModDoc->UpdateAllViews(this, GeneralHint(nChn).Channels());
		}
	}
}

void CViewGlobals::OnEditVol1() {OnEditVol(0, IDC_EDIT1);}
void CViewGlobals::OnEditVol2() {OnEditVol(1, IDC_EDIT3);}
void CViewGlobals::OnEditVol3() {OnEditVol(2, IDC_EDIT5);}
void CViewGlobals::OnEditVol4() {OnEditVol(3, IDC_EDIT7);}


void CViewGlobals::OnEditPan(const CHANNELINDEX chnMod4, const UINT itemID)
{
	CModDoc *pModDoc = GetDocument();
	const CHANNELINDEX nChn = (CHANNELINDEX)(m_nActiveTab * CHANNELS_IN_TAB) + chnMod4;
	const int pan = GetDlgItemIntEx(itemID);
	if ((pModDoc) && (pan >= 0) && (pan <= 256) && (!m_nLockCount))
	{
		PrepareUndo(chnMod4);
		if (pModDoc->SetChannelDefaultPan(nChn, static_cast<uint16>(pan)))
		{
			m_sbPan[chnMod4].SetPos(pan / 4);
			pModDoc->UpdateAllViews(this, GeneralHint(nChn).Channels());
			// Surround is forced off when changing pan, so uncheck the checkbox.
			CheckDlgButton(IDC_CHECK2 + chnMod4 * 2, BST_UNCHECKED);
		}
	}
}


void CViewGlobals::OnEditPan1() {OnEditPan(0, IDC_EDIT2);}
void CViewGlobals::OnEditPan2() {OnEditPan(1, IDC_EDIT4);}
void CViewGlobals::OnEditPan3() {OnEditPan(2, IDC_EDIT6);}
void CViewGlobals::OnEditPan4() {OnEditPan(3, IDC_EDIT8);}


void CViewGlobals::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	CModDoc *pModDoc;
	CHANNELINDEX nChn;

	CFormView::OnHScroll(nSBCode, nPos, pScrollBar);

	pModDoc = GetDocument();
	nChn = (CHANNELINDEX)(m_nActiveTab * CHANNELS_IN_TAB);
	if ((pModDoc) && (!IsLocked()) && (nChn < MAX_BASECHANNELS))
	{
		BOOL bUpdate = FALSE;
		short int pos;

		LockControls();
		const CHANNELINDEX nLoopLimit = std::min(static_cast<CHANNELINDEX>(CHANNELS_IN_TAB), static_cast<CHANNELINDEX>(pModDoc->GetSoundFile().GetNumChannels() - nChn));
		for (CHANNELINDEX iCh = 0; iCh < nLoopLimit; iCh++)
		{
			if(pScrollBar == (CScrollBar *) &m_sbVolume[iCh])
			{
				// Volume sliders
				pos = (short int)m_sbVolume[iCh].GetPos();
				if ((pos >= 0) && (pos <= 64))
				{
					PrepareUndo(iCh);
					if (pModDoc->SetChannelGlobalVolume(nChn + iCh, pos))
					{
						SetDlgItemInt(IDC_EDIT1 + iCh * 2, pos);
						bUpdate = TRUE;
					}
				}
			} else if(pScrollBar == (CScrollBar *) &m_sbPan[iCh])
			{
				// Pan sliders
				pos = (short int)m_sbPan[iCh].GetPos();
				if(pos >= 0 && pos <= 64 && (static_cast<uint16>(pos) != pModDoc->GetSoundFile().ChnSettings[nChn+iCh].nPan / 4u))
				{
					PrepareUndo(iCh);
					if (pModDoc->SetChannelDefaultPan(nChn + iCh, pos * 4))
					{
						SetDlgItemInt(IDC_EDIT2 + iCh * 2, pos * 4);
						CheckDlgButton(IDC_CHECK2 + iCh * 2, BST_UNCHECKED);
						bUpdate = TRUE;
					}
				}
			}
		}


		if (bUpdate) pModDoc->UpdateAllViews(this, GeneralHint(nChn).Channels());
		UnlockControls();
	}
}


void CViewGlobals::OnEditName(const CHANNELINDEX chnMod4, const UINT itemID)
{
	CModDoc *pModDoc = GetDocument();

	if ((pModDoc) && (!m_nLockCount))
	{
		CSoundFile &sndFile = pModDoc->GetSoundFile();
		const CHANNELINDEX nChn = m_nActiveTab * CHANNELS_IN_TAB + chnMod4;
		CString tmp;
		GetDlgItemText(itemID, tmp);
		const std::string s = mpt::ToCharset(sndFile.GetCharsetInternal(), tmp);
		if ((sndFile.GetType() & (MOD_TYPE_XM|MOD_TYPE_IT|MOD_TYPE_MPT)) && (nChn < sndFile.GetNumChannels()) && (s != sndFile.ChnSettings[nChn].szName))
		{
			PrepareUndo(chnMod4);
			sndFile.ChnSettings[nChn].szName = s;
			pModDoc->SetModified();
			pModDoc->UpdateAllViews(this, GeneralHint(nChn).Channels());
		}
	}
}
void CViewGlobals::OnEditName1() {OnEditName(0, IDC_EDIT9);}
void CViewGlobals::OnEditName2() {OnEditName(1, IDC_EDIT10);}
void CViewGlobals::OnEditName3() {OnEditName(2, IDC_EDIT11);}
void CViewGlobals::OnEditName4() {OnEditName(3, IDC_EDIT12);}


void CViewGlobals::OnFxChanged(const CHANNELINDEX chnMod4)
{
	CModDoc *pModDoc = GetDocument();

	if (pModDoc)
	{
		CSoundFile &sndFile = pModDoc->GetSoundFile();
		CHANNELINDEX nChn = m_nActiveTab * CHANNELS_IN_TAB + chnMod4;
		PluginChannel nfx = m_CbnEffects[chnMod4].GetSelection();
		if (nfx.plugin != PLUGINDEX_INVALID && nChn < sndFile.GetNumChannels()
			&& (sndFile.ChnSettings[nChn].nMixPlugin != nfx.plugin || sndFile.ChnSettings[nChn].pluginChannel != nfx.channel))
		{
			PrepareUndo(chnMod4);
			sndFile.ChnSettings[nChn].nMixPlugin = nfx.plugin;
			sndFile.ChnSettings[nChn].pluginChannel = nfx.channel;
			if(sndFile.GetModSpecifications().supportsPlugins)
				pModDoc->SetModified();
			pModDoc->UpdateAllViews(this, GeneralHint(nChn).Channels());
		}
	}
}


void CViewGlobals::OnFx1Changed() {OnFxChanged(0);}
void CViewGlobals::OnFx2Changed() {OnFxChanged(1);}
void CViewGlobals::OnFx3Changed() {OnFxChanged(2);}
void CViewGlobals::OnFx4Changed() {OnFxChanged(3);}


LRESULT CViewGlobals::OnModViewMsg(WPARAM wParam, LPARAM /*lParam*/)
{
	switch(wParam)
	{
		case VIEWMSG_SETFOCUS:
		case VIEWMSG_SETACTIVE:
			GetParentFrame()->SetActiveView(this);
			SetFocus();
			return 0;
		default:
			return 0;
	}
}


OPENMPT_NAMESPACE_END
