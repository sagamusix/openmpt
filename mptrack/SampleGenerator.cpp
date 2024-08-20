/*
 * SampleGenerator.cpp
 * -------------------
 * Purpose: Generate samples from math formulas using Lua
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"

#include "SampleGenerator.h"
#include "Reporting.h"
#include "resource.h"
#include "scripting/LuaVM.h"
#include "../soundlib/modsmp_ctrl.h"
#include "../soundlib/AudioCriticalSection.h"
#include "../soundlib/Sndfile.h"

OPENMPT_NAMESPACE_BEGIN

uint32 SampleGenerator::m_sampleFrequency = 48000;
SmpLength SampleGenerator::m_sampleLength = SampleGenerator::m_sampleFrequency;
CString SampleGenerator::m_expression = _T("math.sin(xp * math.pi)");
SampleGenerator::ClipMethod SampleGenerator::m_sampleClipping = SampleGenerator::ClipMethod::Normalize;

namespace
{
	using mix_t = SampleGenerator::mix_t;

	auto clip(mix_t val, mix_t min, mix_t max) { return std::clamp(val, min, max); }
	auto pwm(mix_t pos, mix_t duty, mix_t width) { if (width == 0) return 0.0; else return (std::fmod(pos, width) < ((duty / 100) * width)) ? 1.0 : -1.0; }
	auto sign(mix_t v) { return static_cast<mix_t>(mpt::signum(v)); }
	auto tri(mix_t pos, mix_t width) { if ((int)width == 0) return 0.0; else return std::abs(((int)pos % (int)(width)) - width / 2) / (width / 4) - 1; }
}

SampleGenerator::SampleGenerator()
	: lua{std::make_unique<sol::state>()}
{
	lua->open_libraries(sol::lib::base, sol::lib::math);
	// Setup function callbacks
#define MPT_SOL_BIND(func) lua->set(#func, sol::c_call<decltype(&func), &func>);
	MPT_SOL_BIND(clip);
	MPT_SOL_BIND(pwm);
	MPT_SOL_BIND(sign);
	MPT_SOL_BIND(tri);
#undef MPT_LUA_BIND
	lua->set_function("smp", [this](SmpLength v) { return (v >= 0 && v < m_sampleBuffer.size()) ? m_sampleBuffer[v] : 0; });
	lua->script("function iif(condition, if_true, if_false) if condition then return if_true else return if_false end end");
}


SampleGenerator::~SampleGenerator() {}


// Open the smpgen dialog
bool SampleGenerator::ShowDialog(CWnd *parent)
{
	bool isDone = false, result = false;
	while(!isDone)
	{
		SmpGenDialog dlg(m_sampleFrequency, m_sampleLength, m_sampleClipping, m_expression, parent);
		dlg.DoModal();

		// pressed "OK" button?
		if(dlg.CanApply())
		{
			m_sampleFrequency = dlg.GetFrequency();
			m_sampleLength = dlg.GetLength();
			m_sampleClipping = dlg.GetClipping();
			m_expression = dlg.GetExpression();
			isDone = CanRenderSample();
			if(isDone) isDone = TestExpression();	// show dialog again if the formula can't be parsed.
			result = true;
		} else
		{
			isDone = true; // just quit.
			result = false;
		}
	}
	return result;
}


// Check if the currently select expression can be parsed and executed.
bool SampleGenerator::TestExpression()
{
	try
	{
		lua->script("function sampleGenerator(x, xp, len, lens, freq) return " + mpt::ToCharset(mpt::Charset::UTF8, m_expression) + " end");
		sol::protected_function f = (*lua)["sampleGenerator"];
		auto res = f(0, 0.0, 0, 0.0, m_sampleFrequency);
		if(!res.valid())
		{
			sol::error err = res;
			throw err;
		}
	} catch (const sol::error &e)
	{
		Reporting::Error(e.what(), "Invalid sample generator expression");
		return false;
	}
	return true;
}


// Check if sample parameters are valid.
bool SampleGenerator::CanRenderSample() const
{
	return mpt::is_in_range(m_sampleFrequency, MIN_FREQ, MAX_FREQ)
		&& mpt::is_in_range(m_sampleLength, MIN_LENGTH, MAX_LENGTH);
}


// Actual render loop.
bool SampleGenerator::RenderSample(CSoundFile &sndFile, SAMPLEINDEX smp)
{
	if(!CanRenderSample() || !TestExpression() || (smp < 1) || (smp > sndFile.GetNumSamples())) return false;

	try
	{
		m_sampleBuffer.assign(m_sampleLength, 0);
	} catch(mpt::out_of_memory e)
	{
		mpt::delete_out_of_memory(e);
		return false;
	}

	const SmpLength length = m_sampleLength, freq = m_sampleFrequency;
	const double lengthSec = static_cast<double>(length) / freq;
	mix_t minMax = 0;

	try
	{
		sol::function genFunc = (*lua)["sampleGenerator"];
		for(SmpLength x = 0; x < length; x++)
		{
			mix_t xp = x * 100.0 / length;
			m_sampleBuffer[x] = genFunc(x, xp, length, lengthSec, freq);
			minMax = std::max(minMax, std::abs(m_sampleBuffer[x]));
		}
	} catch (const sol::error &e)
	{
		Reporting::Error(e.what(), "Invalid sample generator expression");
		return false;
	}

	// Set new sample properties
	CriticalSection cs;
	ModSample &sample = sndFile.GetSample(smp);
	sample.nC5Speed = m_sampleFrequency;
	if constexpr (sizeof(sample_t) == 2)
		sample.uFlags.set(CHN_16BIT);
	sample.uFlags.reset(CHN_STEREO | CHN_SUSTAINLOOP | CHN_PINGPONGSUSTAIN);
	sample.nLoopStart = 0;
	sample.nLoopEnd = length;
	sample.nSustainStart = sample.nSustainEnd = 0;
	sample.nLength = length;
	if(lengthSec < 5.0) // arbitrary limit for automatic sample loop (5 seconds)
		sample.uFlags.set(CHN_LOOP);
	else
		sample.uFlags.reset(CHN_LOOP | CHN_PINGPONGLOOP);
	sample.Convert(MOD_TYPE_MPT, sndFile.GetType());

	// Convert sample to 16-bit (or whatever has been specified)
	if(sample.AllocateSample())
	{
		sample_t *pSample = static_cast<sample_t *>(sample.samplev());
		if(minMax == 0)
			minMax = 1;  // avoid division by 0
		for(SmpLength i = 0; i < length; i++)
		{
			auto val = m_sampleBuffer[i];
			switch(m_sampleClipping)
			{
			case ClipMethod::Clip: val = std::clamp(val, -1.0, 1.0); break;
			case ClipMethod::Normalize: val /= minMax; break;
			}

			pSample[i] = static_cast<sample_t>(val * Util::MaxValueOfType(pSample[0]));
		}

		sample.pData.pSample = nullptr;
		sample.ReplaceWaveform(pSample, length, sndFile);
		sample.PrecomputeLoops(sndFile);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
// Sample Generator Dialog implementation


// List of all possible expression for expression menu
static constexpr std::pair<const char*, const char *> menuItems[] =
{
	//------------------------------------
	{ "Variables", "" },
	//------------------------------------
	{ "Current position (sampling point)", "x" },
	{ "Current position (percentage)", "xp" },
	{ "Sample length (frames)", "len" },
	{ "Sample length (seconds)", "lens" },
	{ "Sampling frequency", "freq" },
	//------------------------------------
	{ "Constants", "" },
	//------------------------------------
	{ "Pi", "math.pi" },
	{ "e", "math.exp(1)" },
	//------------------------------------
	{ "Trigonometric functions", "" },
	//------------------------------------
	{ "Sine", "math.sin(x)" },
	{ "Cosine", "math.cos(x)" },
	{ "Tangens", "math.tan(x)" },
	{ "Arcus Sine", "math.asin(x)" },
	{ "Arcus Cosine", "math.acos(x)" },
	{ "Arcus Tangens", "math.atan(x[, y])" },
	//------------------------------------
	{ "Log, Exp, Root", "" },
	//------------------------------------
	{ "Logarithm (base 10)", "math.log10(x)" },
	{ "Natural Logarithm (base e)", "math.log(x)" },
	{ "x^y", "x ^ y" },
	{ "e^x", "math.exp(x)" },
	{ "Square Root", "math.sqrt(x)" },
	//------------------------------------
	{ "Sign and rounding", "" },
	//------------------------------------
	{ "Sign", "sign(x)" },
	{ "Absolute value", "math.abs(x)" },
	{ "Round Down", "math.floor(x)" },
	{ "Round Up", "math.ceil(x)" },
	//------------------------------------
	{ "Sets", "" },
	//------------------------------------
	{ "Minimum", "math.min(x, y, ...)" },
	{ "Maximum", "math.max(x, y, ...)" },
	//{ "Sum", "sum(x, y, ...)" },
	//{ "Mean value", "avg(x, y, ...)" },
	//------------------------------------
	{ "Misc functions", "" },
	//------------------------------------
	{ "Pulse Generator", "pwm(position, duty%, width)" },
	{ "Triangle", "tri(position, width)" },
	{ "Random value between 0 and 1", "math.random()" },
	{ "Random integer value between x and y", "math.random(x, y)" },
	{ "Access previous sampling point", "smp(position)" },
	{ "Clip between values", "clip(value, minclip, maxclip)" },
	{ "If...Then...Else", "iif(condition, true_statement, false_statement)" },
	//------------------------------------
	{ "Operators", "" },
	//------------------------------------
	//{ "Assignment", "x = y" },
	{ "Logical And", "x & y" },
	{ "Logical Or", "x | y" },
	{ "Less or equal", "x <= y" },
	{ "Greater or equal", "x >= y" },
	{ "Not equal", "x != y" },
	{ "Equal", "x == y" },
	{ "Greater than", "x > y" },
	{ "Less than", "x < y" },
	{ "Addition", "x + y" },
	{ "Subtraction", "x - y" },
	{ "Multiplication", "x * y" },
	{ "Division", "x / y" },
	{ "Modulo", "x % y" },
};


BEGIN_MESSAGE_MAP(SmpGenDialog, DialogBase)
	ON_EN_CHANGE(IDC_EDIT_SAMPLE_LENGTH,     &SmpGenDialog::OnSampleLengthChanged)
	ON_EN_CHANGE(IDC_EDIT_SAMPLE_LENGTH_SEC, &SmpGenDialog::OnSampleSecondsChanged)
	ON_EN_CHANGE(IDC_EDIT_SAMPLE_FREQ,       &SmpGenDialog::OnSampleFreqChanged)
	ON_EN_CHANGE(IDC_EDIT_FORMULA,           &SmpGenDialog::OnExpressionChanged)
	ON_COMMAND(IDC_BUTTON_SHOW_EXPRESSIONS,  &SmpGenDialog::OnShowExpressions)
	ON_COMMAND(IDC_BUTTON_SAMPLEGEN_PRESETS, &SmpGenDialog::OnShowPresets)

	ON_COMMAND_RANGE(ID_SAMPLE_GENERATOR_MENU, static_cast<UINT>(ID_SAMPLE_GENERATOR_MENU + std::size(menuItems) - 1), &SmpGenDialog::OnInsertExpression)
	ON_COMMAND_RANGE(ID_SAMPLE_GENERATOR_PRESET_MENU, ID_SAMPLE_GENERATOR_PRESET_MENU + MAX_SAMPLEGEN_PRESETS + 1, &SmpGenDialog::OnSelectPreset)
END_MESSAGE_MAP()


SmpGenDialog::SmpGenDialog(uint32 freq, SmpLength len, SampleGenerator::ClipMethod clipping, const CString &expr, CWnd *parent)
	: DialogBase{IDD_SAMPLE_GENERATOR, parent}
	, m_sampleLength{len}
	, m_sampleFrequency{freq}
	, m_expression{expr}
	, m_sampleClipping{clipping}
{
}



BOOL SmpGenDialog::OnInitDialog()
{
	DialogBase::OnInitDialog();

	m_EditLength.SubclassDlgItem(IDC_EDIT_SAMPLE_LENGTH_SEC, this);
	m_EditLength.AllowNegative(false);
	m_EditLength.AllowFractions(true);

	RecalcParameters(false, true);
	SetDlgItemText(IDC_EDIT_FORMULA, m_expression);

	int check = IDC_RADIO_SMPCLIP1;
	switch(m_sampleClipping)
	{
	case SampleGenerator::ClipMethod::Clip: check = IDC_RADIO_SMPCLIP1; break;
	case SampleGenerator::ClipMethod::Overflow: check = IDC_RADIO_SMPCLIP2; break;
	case SampleGenerator::ClipMethod::Normalize: check = IDC_RADIO_SMPCLIP3; break;
	}
	CheckRadioButton(IDC_RADIO_SMPCLIP1, IDC_RADIO_SMPCLIP3, check);

	if(m_presets.empty())
	{
		CreateDefaultPresets();
	}

	// Create font for "dropdown" button (Marlett system font)
	m_hButtonFont.CreateFont(14, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_DONTCARE, _T("Marlett"));
	GetDlgItem(IDC_BUTTON_SHOW_EXPRESSIONS)->SendMessage(WM_SETFONT, reinterpret_cast<WPARAM>(m_hButtonFont.m_hObject), MAKELPARAM(TRUE, 0));

	return TRUE;
}


void SmpGenDialog::OnOK()
{
	DialogBase::OnOK();
	m_apply = true;

	int check = GetCheckedRadioButton(IDC_RADIO_SMPCLIP1, IDC_RADIO_SMPCLIP3);
	switch(check)
	{
	case IDC_RADIO_SMPCLIP1: m_sampleClipping = SampleGenerator::ClipMethod::Clip; break;
	case IDC_RADIO_SMPCLIP2: m_sampleClipping = SampleGenerator::ClipMethod::Overflow; break;
	case IDC_RADIO_SMPCLIP3: m_sampleClipping = SampleGenerator::ClipMethod::Normalize; break;
	}
}


void SmpGenDialog::OnCancel()
{
	DialogBase::OnCancel();
	m_apply = false;
}


// User changed formula
void SmpGenDialog::OnExpressionChanged()
{
	GetDlgItemText(IDC_EDIT_FORMULA, m_expression);
}


// User changed sample length field
void SmpGenDialog::OnSampleLengthChanged()
{
	int length = GetDlgItemInt(IDC_EDIT_SAMPLE_LENGTH);
	if(length >= SampleGenerator::MIN_LENGTH && length <= SampleGenerator::MAX_LENGTH)
	{
		m_sampleLength = length;
		RecalcParameters(false);
	}
}


// User changed sample length (seconds) field
void SmpGenDialog::OnSampleSecondsChanged()
{
	double seconds = 0.0;
	if(m_EditLength.GetDecimalValue(seconds))
	{
		m_sampleSeconds = seconds;
		RecalcParameters(true);
	}
}


// User changed sample frequency field
void SmpGenDialog::OnSampleFreqChanged()
{
	UINT freq = GetDlgItemInt(IDC_EDIT_SAMPLE_FREQ);
	if(freq >= SampleGenerator::MIN_FREQ && freq <= SampleGenerator::MAX_FREQ)
	{
		m_sampleFrequency = freq;
		RecalcParameters(false);
	}
}


// Show all expressions that can be input
void SmpGenDialog::OnShowExpressions()
{
	HMENU hMenu = ::CreatePopupMenu(), hSubMenu = NULL;
	if(!hMenu) return;

	for(size_t i = 0; i < std::size(menuItems); i++)
	{
		if(!strcmp(menuItems[i].second, ""))
		{
			// add sub menu
			if(hSubMenu != NULL) ::DestroyMenu(hSubMenu);
			hSubMenu = ::CreatePopupMenu();

			AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hSubMenu, mpt::ToWin(mpt::Charset::ASCII, menuItems[i].first).c_str());
		} else
		{
			// add sub menu entry (formula)
			AppendMenu(hSubMenu, MF_STRING, ID_SAMPLE_GENERATOR_MENU + i, mpt::ToWin(mpt::Charset::ASCII, menuItems[i].first).c_str());
		}
	}

	// place popup menu below button
	RECT button;
	GetDlgItem(IDC_BUTTON_SHOW_EXPRESSIONS)->GetWindowRect(&button);
	::TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, button.left, button.bottom, 0, m_hWnd, NULL);
	::DestroyMenu(hMenu);
	::DestroyMenu(hSubMenu);
}


// Show all expression presets
void SmpGenDialog::OnShowPresets()
{
	HMENU hMenu = ::CreatePopupMenu();
	if(!hMenu) return;

	bool prestsExist = false;
	for(size_t i = 0; i < m_presets.size(); i++)
	{
		if(!m_presets[i].expression.IsEmpty())
		{
			AppendMenu(hMenu, MF_STRING, ID_SAMPLE_GENERATOR_PRESET_MENU + i, m_presets[i].description);
			prestsExist = true;
		}
	}
	
	if(prestsExist) AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

	AppendMenu(hMenu, MF_STRING, ID_SAMPLE_GENERATOR_PRESET_MENU + MAX_SAMPLEGEN_PRESETS, _T("Manage..."));

	CString result;
	GetDlgItemText(IDC_EDIT_FORMULA, result);
	if((!result.IsEmpty()) && (m_presets.size() < MAX_SAMPLEGEN_PRESETS))
	{
		AppendMenu(hMenu, MF_STRING, ID_SAMPLE_GENERATOR_PRESET_MENU + MAX_SAMPLEGEN_PRESETS + 1, _T("Add current..."));
	}

	// place popup menu below button
	RECT button;
	GetDlgItem(IDC_BUTTON_SAMPLEGEN_PRESETS)->GetWindowRect(&button);
	::TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, button.left, button.bottom, 0, m_hWnd, NULL);
	::DestroyMenu(hMenu);
}



// Insert expression from context menu
void SmpGenDialog::OnInsertExpression(UINT id)
{
	if((id < ID_SAMPLE_GENERATOR_MENU) || (id >= ID_SAMPLE_GENERATOR_MENU + std::size(menuItems)))
		return;

	m_expression += _T(" ") + mpt::ToCString(mpt::Charset::ASCII, menuItems[id - ID_SAMPLE_GENERATOR_MENU].second);
	SetDlgItemText(IDC_EDIT_FORMULA, m_expression);
}


// Select a preset (or manage them, or add one)
void SmpGenDialog::OnSelectPreset(UINT id)
{
	if((id < ID_SAMPLE_GENERATOR_PRESET_MENU) || (id >= ID_SAMPLE_GENERATOR_PRESET_MENU + MAX_SAMPLEGEN_PRESETS + 2))
		return;

	if(id - ID_SAMPLE_GENERATOR_PRESET_MENU >= MAX_SAMPLEGEN_PRESETS)
	{
		// add...
		if((id - ID_SAMPLE_GENERATOR_PRESET_MENU == MAX_SAMPLEGEN_PRESETS + 1))
		{
			m_presets.emplace_back(m_expression, m_expression);
			// call preset manager now.
		}

		// manage...
		SmpGenPresetDlg dlg(m_presets, this);
		dlg.DoModal();
	} else
	{
		m_expression = m_presets[id - ID_SAMPLE_GENERATOR_PRESET_MENU].expression;
		SetDlgItemText(IDC_EDIT_FORMULA, m_expression);
	}
}


// Update input fields, depending on what has been chagned
void SmpGenDialog::RecalcParameters(bool secondsChanged, bool forceRefresh)
{
	static bool isLocked = false;
	if(isLocked)
		return;
	isLocked = true;  // avoid deadlock

	if(secondsChanged)
	{
		// seconds changed => recalc length
		m_sampleLength = mpt::saturate_round<SmpLength>(m_sampleSeconds * m_sampleFrequency);
		if(m_sampleLength < SampleGenerator::MIN_LENGTH || m_sampleLength > SampleGenerator::MAX_LENGTH) m_sampleLength = SampleGenerator::MAX_LENGTH;
	} else
	{
		// length/freq changed => recalc seconds
		m_sampleSeconds = static_cast<double>(m_sampleLength) / m_sampleFrequency;
	}

	if(secondsChanged || forceRefresh) SetDlgItemInt(IDC_EDIT_SAMPLE_LENGTH, m_sampleLength);
	if(secondsChanged || forceRefresh) SetDlgItemInt(IDC_EDIT_SAMPLE_FREQ, m_sampleFrequency);
	if(!secondsChanged || forceRefresh) m_EditLength.SetDecimalValue(m_sampleSeconds, 10);

	size_t smpSize = m_sampleLength * sizeof(SampleGenerator::sample_t);
	const TCHAR *unit = nullptr;
	if(smpSize < 1024)
	{
		unit = _T("Bytes");
	} else if((smpSize >> 10) < 1024)
	{
		smpSize >>= 10;
		unit = _T("KB");
	} else
	{
		smpSize >>= 20;
		unit = _T("MB");
	}
	SetDlgItemText(IDC_STATIC_SMPSIZE_KB, MPT_TFORMAT("Sample Size: {} {}")(smpSize, unit).c_str());

	isLocked = false;
}


// Create a set of default formla presets
void SmpGenDialog::CreateDefaultPresets()
{
	m_presets.assign({
		{_T("A440"), _T("math.sin(xp * math.pi / 50 * 440 * len / freq)")},
		{_T("Bass Drum"), _T("math.sin(math.sqrt(xp) * math.pi * 10) * math.exp(math.min(0, 15 - xp / 5)) + (math.random() * 2 - 1) * math.exp(-xp)")},
		{_T("Brown Noise (kind of)"), _T("(math.random() - 0.5) * 0.1 + smp(x - 1) * 0.9")},
		{_T("Noisy Saw"), _T("(x % 800) / 800.0 - 0.5 + math.random() * 0.1")},
		{_T("PWM Filter"), _T("pwm(x, 50 + math.sin(xp * math.pi / 100) * 40, 100) + tri(x, 50)")},
		{_T("Fat PWM Pad"), _T("pwm(x, xp, 500) + pwm(x, math.abs(50 - xp), 1000)")},
		{_T("Dual Square"), _T("iif((x % 100) < 50, x % 200, -(x % 200) + 50)")},
		{_T("Noise Hit"), _T("math.exp(-xp) * (math.random() * x - x / 2)")},
		{_T("Laser"), _T("math.sin(xp * math.pi * 100 / xp ^ 2) * 100 / math.sqrt(xp)")},
		{_T("Noisy Laser Hit"), _T("(math.sin(math.sqrt(xp) * 100) + math.random() - 0.5) * math.exp(-xp / 10)")},
		{_T("Twinkle, Twinkle..."), _T("math.sin(x * math.pi * 100 / xp) * 100 / math.sqrt(xp)")},
		{_T("FM Tom"), _T("math.sin(xp * math.pi * 2 + (xp / 5 - 50) ^ 2) * math.exp(-xp / 10)")},
		{_T("FM Warp"), _T("math.sin(math.pi * xp / 2 * (1 + (1 + math.sin(math.pi * xp / 4 * 50)) / 4)) * math.exp(-(xp / 8) * 0.6)")},
		{_T("Super Pulse"), _T("pwm(x * 0.99, 50, 100) + pwm(x * 1.01, 50, 100) + pwm(x * 0.985, 50, 100) + pwm(x * 1.015, 50, 100)")},
		{_T("Flanging Saw"), _T("x % 100 + (x * 0.99) % 100 + (x * 1.01) % 100 - 150")},
		//{_T("Weird Noise"), _T("math.random() * 0.1 + smp(x - math.random() * xp) * 0.9")},
	});
}



//////////////////////////////////////////////////////////////////////////
// Sample Generator Preset Dialog implementation


BEGIN_MESSAGE_MAP(SmpGenPresetDlg, DialogBase)
	ON_COMMAND(IDC_BUTTON_ADD,                   &SmpGenPresetDlg::OnAddPreset)
	ON_COMMAND(IDC_BUTTON_REMOVE,                &SmpGenPresetDlg::OnRemovePreset)
	ON_EN_CHANGE(IDC_EDIT_PRESET_NAME,           &SmpGenPresetDlg::OnTextChanged)
	ON_EN_CHANGE(IDC_EDIT_PRESET_EXPR,           &SmpGenPresetDlg::OnExpressionChanged)
	ON_LBN_SELCHANGE(IDC_LIST_SAMPLEGEN_PRESETS, &SmpGenPresetDlg::OnListSelChange)
END_MESSAGE_MAP()


SmpGenPresetDlg::SmpGenPresetDlg(SmpGenPresets &presets, CWnd *parent)
	: DialogBase(IDD_SAMPLE_GENERATOR_PRESETS, parent)
	, m_presets(presets)
{
}


BOOL SmpGenPresetDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();
	RefreshList();
	return TRUE;
}


void SmpGenPresetDlg::OnOK()
{
	// Remove empty presets
	m_presets.erase(std::remove_if(m_presets.begin(), m_presets.end(), [](const auto &preset) { return preset.expression.IsEmpty(); }), m_presets.end());
	DialogBase::OnOK();
}


void SmpGenPresetDlg::OnListSelChange()
{
	m_currentItem = static_cast<CListBox *>(GetDlgItem(IDC_LIST_SAMPLEGEN_PRESETS))->GetCurSel() + 1;
	if(m_currentItem == 0 || m_currentItem > m_presets.size())
		return;
	const auto &preset = m_presets[m_currentItem - 1];
	SetDlgItemText(IDC_EDIT_PRESET_NAME, preset.description);
	SetDlgItemText(IDC_EDIT_PRESET_EXPR, preset.expression);
}


void SmpGenPresetDlg::OnTextChanged()
{
	if(m_currentItem == 0 || m_currentItem > m_presets.size())
		return;
	CString result;
	GetDlgItemText(IDC_EDIT_PRESET_NAME, result);

	const int item = static_cast<int>(m_currentItem - 1);
	auto &preset = m_presets[item];
	preset.description = result;

	CListBox *clist = static_cast<CListBox *>(GetDlgItem(IDC_LIST_SAMPLEGEN_PRESETS));
	clist->DeleteString(static_cast<UINT>(item));
	clist->InsertString(item, preset.description);
	clist->SetCurSel(item);
}


void SmpGenPresetDlg::OnExpressionChanged()
{
	if(m_currentItem == 0 || m_currentItem > m_presets.size())
		return;
	CString result;
	GetDlgItemText(IDC_EDIT_PRESET_EXPR, result);
	m_presets[m_currentItem - 1].expression = result;

}


void SmpGenPresetDlg::OnAddPreset()
{
	if (m_presets.size() >= MAX_SAMPLEGEN_PRESETS)
	{
		// TODO
		::MessageBeep(MB_ICONWARNING);
		return;
	}
	m_presets.emplace_back(_T("New Preset"), _T(""));
	m_currentItem = m_presets.size();
	RefreshList();
}


void SmpGenPresetDlg::OnRemovePreset()
{
	if(m_currentItem == 0 || m_currentItem > m_presets.size())
		return;
	m_presets.erase(m_presets.begin() + m_currentItem - 1);
	RefreshList();
}


void SmpGenPresetDlg::RefreshList()
{
	CListBox *clist = static_cast<CListBox *>(GetDlgItem(IDC_LIST_SAMPLEGEN_PRESETS));
	clist->SetRedraw(FALSE);  //disable listbox refreshes during fill to avoid flicker
	clist->ResetContent();
	for(const auto &preset : m_presets)
	{
		clist->AddString(preset.description);
	}
	clist->SetRedraw(TRUE);  //re-enable listbox refreshes
	if(m_currentItem == 0 || m_currentItem > m_presets.size())
		m_currentItem = m_presets.size();
	if(m_currentItem != 0)
		clist->SetCurSel(static_cast<int>(m_currentItem - 1));
	OnListSelChange();
}

OPENMPT_NAMESPACE_END
