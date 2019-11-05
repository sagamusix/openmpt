// TODO add header

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "AccessibleControls.h"
#include "CListCtrl.h"
#include "Globals.h"
#include "PluginComboBox.h"
#include "../soundlib/plugins/PluginStructs.h"

OPENMPT_NAMESPACE_BEGIN

class IMixPlugin;

class CCtrlPlugins : public CModControlDlg
{
protected:
	PluginComboBox m_CbnPlugin;
	CComboBox m_CbnParam, m_CbnOutput;
	CSliderCtrl m_sbValue, m_sbDryRatio;
	CListCtrlEx m_ChannelList;
	CComboBox m_CbnPreset, m_CbnSpecialMixProcessing;
	CSliderCtrl m_sbWetDry;
	CButton m_BtnSelect, m_BtnEdit;
	AccessibleButton m_prevPluginButton, m_nextPluginButton;
	CSpinButtonCtrl m_SpinMixGain;

	PlugParamIndex m_nCurrentParam = 0;
	PLUGINDEX m_nCurrentPlugin = 0;

	enum { AdjustPattern = true, NoPatternAdjust = false };

public:
	CCtrlPlugins(CModControlView &parent, CModDoc &document);

	CRuntimeClass *GetAssociatedViewClass() override;

public:
	//{{AFX_VIRTUAL(CViewGlobals)
	Setting<LONG> &GetSplitPosRef() override;
	BOOL OnInitDialog() override;
	void DoDataExchange(CDataExchange* pDX) override;
	void UpdateView(UpdateHint hint, CObject *pObj = nullptr) override;
	LRESULT OnModCtrlMsg(WPARAM wParam, LPARAM lParam) override;
	void OnActivatePage(LPARAM lParam) override;
	void RecalcLayout() override {}
	CString GetToolTipText(UINT id, HWND hwnd) const override;
	//}}AFX_VIRTUAL

protected:
	int GetDlgItemIntEx(UINT nID);
	void BuildEmptySlotList(std::vector<PLUGINDEX> &emptySlots);
	bool MovePlug(PLUGINDEX src, PLUGINDEX dest, bool bAdjustPat = AdjustPattern);

	IMixPlugin *GetCurrentPlugin() const;

	void FillPluginProgramBox(int32 firstProg, int32 lastProg);
	void UpdateDryWetDisplay();

protected:
	void SetModified(PluginHint hint = PluginHint().Info());

	//{{AFX_MSG(CViewGlobals)
	afx_msg void OnPluginChanged();
	afx_msg void OnPluginNameChanged();
	afx_msg void OnFillParamCombo();
	afx_msg LRESULT OnParamAutomated(WPARAM plugin, LPARAM param);
	afx_msg LRESULT OnDryWetRatioChangedFromPlayer(WPARAM plugin, LPARAM);
	afx_msg void OnParamChanged();
	afx_msg void OnFocusParam();
	afx_msg void OnFillProgramCombo();
	afx_msg void OnProgramChanged();
	afx_msg void OnLoadParam();
	afx_msg void OnSaveParam();
	afx_msg void OnSelectPlugin();
	afx_msg void OnRemovePlugin();
	afx_msg void OnSetParameter();
	afx_msg void OnEditPlugin();
	afx_msg void OnMixModeChanged();
	afx_msg void OnBypassChanged();
	afx_msg void OnDryMixChanged();
	afx_msg void OnMovePlugToSlot();
	afx_msg void OnInsertSlot();
	afx_msg void OnClonePlug();
	afx_msg void OnWetDryExpandChanged();
	afx_msg void OnAutoSuspendChanged();
	afx_msg void OnSpecialMixProcessingChanged();
	afx_msg void OnOutputRoutingChanged();
	afx_msg void OnPrevPlugin();
	afx_msg void OnNextPlugin();
	afx_msg void OnDeactivatePage();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
