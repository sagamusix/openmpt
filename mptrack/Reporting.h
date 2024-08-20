/*
 * Reporting.h
 * -----------
 * Purpose: A class for showing notifications, prompts, etc...
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include <vector>

OPENMPT_NAMESPACE_BEGIN


struct TaskDlg
{
	// When assigning a custom command ID instead of IDOK/IDCANCEL/..., use this base offset
	static constexpr int CUSTOM_ID_BASE = 200;

	struct Button
	{
		CString text;
		UINT commandId;
	};

	struct Result
	{
		UINT buttonId;
		bool verificationChecked;
	};

	Result DoModal(const CWnd *parent = nullptr) const;

	static bool ModernTaskDialogSupported();

	TaskDlg &WindowTitle(CString title) { windowTitle = std::move(title); return *this; }
	TaskDlg &Headline(CString text) { headline = std::move(text); return *this; }
	TaskDlg &Description(CString text) { description = std::move(text); return *this; }
	TaskDlg &AddButton(CString text, UINT commandID) { buttons.push_back({std::move(text), commandID}); return *this; }
	TaskDlg &Buttons(std::vector<Button> buttonList) { buttons = std::move(buttonList); return *this; }
	TaskDlg &DefaultButton(int commandID) { defaultButton = commandID; return *this; }
	TaskDlg &Icon(UINT iconType) { icon = iconType; return *this; }
	TaskDlg &VerificationText(CString text) { verificationText = std::move(text); return *this; }
	TaskDlg &VerificationChecked(bool checked) { verificationChecked = checked; return *this; }
	TaskDlg &UseBigButtons(bool useBigButtons) { bigButtons = useBigButtons; return *this; }

protected:
	CString windowTitle;
	CString headline;
	CString description;
	CString verificationText;
	std::vector<Button> buttons;
	int defaultButton = 0;
	UINT icon = 0;
	bool verificationChecked = false;
	bool bigButtons = false;
};

enum ConfirmAnswer
{
	cnfYes,
	cnfNo,
	cnfCancel,
};


enum RetryAnswer
{
	rtyRetry,
	rtyCancel,
};


namespace Reporting
{
	// Show a simple notification
	void Notification(const AnyStringLocale &text, const CWnd *parent = nullptr);
	void Notification(const AnyStringLocale &text, const AnyStringLocale &caption, const CWnd *parent = nullptr);

	// Show a simple information
	void Information(const AnyStringLocale &text, const CWnd *parent = nullptr);
	void Information(const AnyStringLocale &text, const AnyStringLocale &caption, const CWnd *parent = nullptr);

	// Show a simple warning
	void Warning(const AnyStringLocale &text, const CWnd *parent = nullptr);
	void Warning(const AnyStringLocale &text, const AnyStringLocale &caption, const CWnd *parent = nullptr);

	// Show an error box.
	void Error(const AnyStringLocale &text, const CWnd *parent = nullptr);
	void Error(const AnyStringLocale &text, const AnyStringLocale &caption, const CWnd *parent = nullptr);

	// Simplified version of the above
	void Message(LogLevel level, const AnyStringLocale &text, const CWnd *parent = nullptr);
	void Message(LogLevel level, const AnyStringLocale &text, const AnyStringLocale &caption, const CWnd *parent = nullptr);

	// Show a confirmation dialog.
	ConfirmAnswer Confirm(const AnyStringLocale &text, bool showCancel = false, bool defaultNo = false, const CWnd *parent = nullptr);
	ConfirmAnswer Confirm(const AnyStringLocale &text, const AnyStringLocale &caption, bool showCancel = false, bool defaultNo = false, const CWnd *parent = nullptr);
	// work-around string literals for caption decaying to bool and catching the wrong overload instead of converting to a string.
	inline ConfirmAnswer Confirm(const AnyStringLocale &text, const char *caption, bool showCancel = false, bool defaultNo = false, const CWnd *parent = nullptr) { return Confirm(text, AnyStringLocale(caption), showCancel, defaultNo, parent); }
	inline ConfirmAnswer Confirm(const AnyStringLocale &text, const wchar_t *caption, bool showCancel = false, bool defaultNo = false, const CWnd *parent = nullptr) { return Confirm(text, AnyStringLocale(caption), showCancel, defaultNo, parent); }
	inline ConfirmAnswer Confirm(const AnyStringLocale &text, const CString &caption, bool showCancel = false, bool defaultNo = false, const CWnd *parent = nullptr) { return Confirm(text, AnyStringLocale(caption), showCancel, defaultNo, parent); }

	// Show a confirmation dialog.
	RetryAnswer RetryCancel(const AnyStringLocale &text, const CWnd *parent = nullptr);
	RetryAnswer RetryCancel(const AnyStringLocale &text, const AnyStringLocale &caption, const CWnd *parent = nullptr);
};


OPENMPT_NAMESPACE_END
