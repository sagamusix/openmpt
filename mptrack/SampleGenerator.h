/*
 * SampleGenerator.h
 * -----------------
 * Purpose: Generate samples from math formulas using Lua
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../soundlib/Snd_defs.h"
#include "CDecimalSupport.h"
#include "DialogBase.h"

namespace sol { class state; }

OPENMPT_NAMESPACE_BEGIN

class CSoundFile;

class SampleGenerator
{
public:
	enum class ClipMethod
	{
		Clip,
		Overflow,
		Normalize,
	};


	static constexpr SmpLength MIN_LENGTH = 1;
	static constexpr SmpLength MAX_LENGTH = MAX_SAMPLE_LENGTH;

	static constexpr uint32 MIN_FREQ = 1;
	static constexpr uint32 MAX_FREQ = 192000; // MAX_SAMPLE_RATE
	
	// Target sample size
	using sample_t = int16;
	using mix_t = double;

protected:
	// Sample parameters
	static uint32 m_sampleFrequency;
	static SmpLength m_sampleLength;
	static CString m_expression;
	static ClipMethod m_sampleClipping;

	// Rendering helper variables (they're here for the callback functions)
	std::vector<mix_t> m_sampleBuffer;

	// Scripting object for parsing the expression
	std::unique_ptr<sol::state> lua;

public:
	bool ShowDialog(CWnd *parent);
	bool TestExpression();
	bool CanRenderSample() const;
	bool RenderSample(CSoundFile &sndFile, SAMPLEINDEX nSample);

	SampleGenerator();
	~SampleGenerator();
};


//////////////////////////////////////////////////////////////////////////
// Sample Generator Formula Preset implementation


struct SampleGenExpression
{
	CString description;  // e.g. "Pulse"
	CString expression;   // e.g. "pwm(x,y,z)" - empty if this is a sub menu

	SampleGenExpression(const CString &desc, const CString &expr) : description(desc), expression(expr) { }
};

static constexpr int MAX_SAMPLEGEN_PRESETS = 100;


//////////////////////////////////////////////////////////////////////////
// Sample Generator Dialog implementation


using SmpGenPresets = std::vector<SampleGenExpression>;

class SmpGenDialog: public DialogBase
{
protected:
	// Sample parameters
	double m_sampleSeconds = 0.0;
	SmpLength m_sampleLength;
	uint32 m_sampleFrequency;
	CString m_expression;
	SampleGenerator::ClipMethod m_sampleClipping;
	// pressed "OK"?
	bool m_apply = false;
	// preset slots
	SmpGenPresets m_presets;

	CFont m_hButtonFont; // "Marlett" font for "dropdown" button
	CNumberEdit m_EditLength;

	void RecalcParameters(bool secondsChanged, bool forceRefresh = false);

	// function presets
	void CreateDefaultPresets();

public:
	const int GetFrequency() const noexcept { return m_sampleFrequency; };
	const int GetLength() const noexcept { return m_sampleLength; };
	const SampleGenerator::ClipMethod GetClipping() const noexcept { return m_sampleClipping; }
	const CString GetExpression() const { return m_expression; };
	bool CanApply() const noexcept { return m_apply; };
	
	SmpGenDialog(uint32 freq, SmpLength len, SampleGenerator::ClipMethod clipping, const CString &expr, CWnd *parent);

protected:
	BOOL OnInitDialog() override;
	void OnOK() override;
	void OnCancel() override;

	afx_msg void OnSampleLengthChanged();
	afx_msg void OnSampleSecondsChanged();
	afx_msg void OnSampleFreqChanged();
	afx_msg void OnExpressionChanged();
	afx_msg void OnShowExpressions();
	afx_msg void OnShowPresets();
	afx_msg void OnInsertExpression(UINT id);
	afx_msg void OnSelectPreset(UINT id);

	DECLARE_MESSAGE_MAP()
};


//////////////////////////////////////////////////////////////////////////
// Sample Generator Preset Dialog implementation


class SmpGenPresetDlg: public DialogBase
{
protected:
	SmpGenPresets &m_presets;
	size_t m_currentItem = 0;  // 0 = no selection, first item is actually 1!

	void RefreshList();

public:
	SmpGenPresetDlg(SmpGenPresets &presets, CWnd *parent);

protected:
	BOOL OnInitDialog() override;
	void OnOK() override;

	afx_msg void OnListSelChange();

	afx_msg void OnTextChanged();
	afx_msg void OnExpressionChanged();

	afx_msg void OnAddPreset();
	afx_msg void OnRemovePreset();

	DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
