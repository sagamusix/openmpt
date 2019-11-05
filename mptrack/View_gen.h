/*
 * view_gen.h
 * ----------
 * Purpose: General tab, lower panel.
 * Notes  : (currently none)
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ColorPickerButton.h"
#include "PluginComboBox.h"
#include "PluginComboBox.h"
#include "UpdateHints.h"

OPENMPT_NAMESPACE_BEGIN

//Note: Changing this won't increase the number of tabs in general view. Most
//of the code use plain number 4.
#define CHANNELS_IN_TAB	4

class CModDoc;
class IMixPlugin;

class CViewGlobals: public CFormView
{
protected:
	CRect m_rcClient;
	CTabCtrl m_TabCtrl;
	PluginComboBox m_CbnEffects[CHANNELS_IN_TAB];
	CSliderCtrl m_sbVolume[CHANNELS_IN_TAB], m_sbPan[CHANNELS_IN_TAB];
	ColorPickerButton m_channelColor[CHANNELS_IN_TAB];	CSpinButtonCtrl m_spinVolume[CHANNELS_IN_TAB], m_spinPan[CHANNELS_IN_TAB];
	int m_nLockCount = 1;
	CHANNELINDEX m_nActiveTab = 0;
	CHANNELINDEX m_lastEdit = CHANNELINDEX_INVALID;

protected:
	CViewGlobals();
	DECLARE_SERIAL(CViewGlobals)

public:
	CModDoc* GetDocument() const;
	void RecalcLayout();
	void LockControls() { m_nLockCount++; }
	void UnlockControls();
	bool IsLocked() const noexcept { return (m_nLockCount > 0); }
	int GetDlgItemIntEx(UINT nID);

public:
	//{{AFX_VIRTUAL(CViewGlobals)
	void OnInitialUpdate() override;
	void DoDataExchange(CDataExchange *pDX) override;
	void OnUpdate(CView *pSender, LPARAM lHint, CObject *pHint) override;
	INT_PTR OnToolHitTest(CPoint point, TOOLINFO *pTI) const override;

	void UpdateView(UpdateHint hint, CObject *pObj = nullptr);
	LRESULT OnModViewMsg(WPARAM, LPARAM);
	LRESULT OnMidiMsg(WPARAM midiData, LPARAM);

private:
	void PrepareUndo(CHANNELINDEX chnMod4);
	void UndoRedo(bool undo);

	void OnEditColor(const CHANNELINDEX chnMod4);
	void OnMute(const CHANNELINDEX chnMod4, const UINT itemID);
	void OnSurround(const CHANNELINDEX chnMod4, const UINT itemID);
	void OnEditVol(const CHANNELINDEX chnMod4, const UINT itemID);
	void OnEditPan(const CHANNELINDEX chnMod4, const UINT itemID);
	void OnEditName(const CHANNELINDEX chnMod4, const UINT itemID);
	void OnFxChanged(const CHANNELINDEX chnMod4);

protected:
	//{{AFX_MSG(CViewGlobals)
	afx_msg void OnEditUndo();
	afx_msg void OnEditRedo();
	afx_msg void OnUpdateUndo(CCmdUI *pCmdUI);
	afx_msg void OnUpdateRedo(CCmdUI *pCmdUI);

	afx_msg void OnEditColor1();
	afx_msg void OnEditColor2();
	afx_msg void OnEditColor3();
	afx_msg void OnEditColor4();
	afx_msg void OnMute1();
	afx_msg void OnMute2();
	afx_msg void OnMute3();
	afx_msg void OnMute4();
	afx_msg void OnSurround1();
	afx_msg void OnSurround2();
	afx_msg void OnSurround3();
	afx_msg void OnSurround4();
	afx_msg void OnEditVol1();
	afx_msg void OnEditVol2();
	afx_msg void OnEditVol3();
	afx_msg void OnEditVol4();
	afx_msg void OnEditPan1();
	afx_msg void OnEditPan2();
	afx_msg void OnEditPan3();
	afx_msg void OnEditPan4();
	afx_msg void OnEditName1();
	afx_msg void OnEditName2();
	afx_msg void OnEditName3();
	afx_msg void OnEditName4();
	afx_msg void OnFx1Changed();
	afx_msg void OnFx2Changed();
	afx_msg void OnFx3Changed();
	afx_msg void OnFx4Changed();
	LRESULT OnDryWetRatioChangedFromPlayer(WPARAM plugin, LPARAM);

	afx_msg void OnDestroy();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnTabSelchange(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg LRESULT OnMDIDeactivate(WPARAM, LPARAM);
	afx_msg LRESULT OnUnlockControls(WPARAM, LPARAM) { if (m_nLockCount > 0) m_nLockCount--; return 0; }
	afx_msg BOOL OnToolTipText(UINT, NMHDR *pNMHDR, LRESULT *pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


OPENMPT_NAMESPACE_END
