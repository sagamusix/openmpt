/*
 * Reporting.cpp
 * -------------
 * Purpose: A class for showing notifications, prompts, etc...
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "Reporting.h"
#include "Mainfrm.h"

#if MPT_WINNT_AT_LEAST(MPT_WIN_VISTA) && defined(UNICODE)
#include "Mptrack.h"
#include <afxtaskdialog.h>
#endif

OPENMPT_NAMESPACE_BEGIN


static inline UINT LogLevelToIcon(LogLevel level)
{
	switch(level)
	{
	case LogDebug: return 0;
	case LogNotification: return 0;
	case LogInformation: return MB_ICONINFORMATION;
	case LogWarning: return MB_ICONWARNING;
	case LogError: return MB_ICONERROR;
	}
	return 0;
}


static CString GetTitle()
{
	return MAINFRAME_TITLE;
}


static CString FillEmptyCaption(const CString &caption, LogLevel level)
{
	CString result = caption;
	if(result.IsEmpty())
	{
		result = GetTitle() + _T(" - ");
		switch(level)
		{
		case LogDebug: result += _T("Debug"); break;
		case LogNotification: result += _T("Notification"); break;
		case LogInformation: result += _T("Information"); break;
		case LogWarning: result += _T("Warning"); break;
		case LogError: result += _T("Error"); break;
		}
	}
	return result;
}


static CString FillEmptyCaption(const CString &caption)
{
	CString result = caption;
	if(result.IsEmpty())
	{
		result = GetTitle();
	}
	return result;
}


static const TCHAR *GetButtonString(UINT id)
{
	mpt::Library user32{mpt::LibraryPath::System(P_("user32"))};
	using PMB_GETSTRING = LPCWSTR(WINAPI *)(UINT);
	PMB_GETSTRING MB_GetString = nullptr;
	user32.Bind(MB_GetString, "MB_GetString");
	if(MB_GetString)
		return MB_GetString(id - 1);

	switch(id)
	{
	case IDOK: return _T("&OK");
	case IDCANCEL: return _T("&Cancel");
	case IDABORT: return _T("&Abort");
	case IDRETRY: return _T("&Retry");
	case IDIGNORE: return _T("&Ignore");
	case IDYES: return _T("&Yes");
	case IDNO: return _T("&No");
	case IDCLOSE: return _T("&Close");
	case IDHELP: return _T("Help");
	case IDTRYAGAIN: return _T("&Try Again");
	case IDCONTINUE: return _T("&Continue");
	default: return _T("");
	}
}


bool TaskDlg::ModernTaskDialogSupported()
{
#if MPT_WINNT_AT_LEAST(MPT_WIN_VISTA) && defined(UNICODE)
	return CTaskDialog::IsSupported()
		&& !(mpt::OS::Windows::IsWine() && theApp.GetWineVersion()->Version().IsBefore(mpt::OS::Wine::Version(3, 13, 0)));
#else
	return false;
#endif
}


TaskDlg::Result TaskDlg::DoModal(const CWnd *parent) const
{
	if(!parent)
		parent = CMainFrame::GetActiveWindow();

#if MPT_WINNT_AT_LEAST(MPT_WIN_VISTA) && defined(UNICODE)
	if(ModernTaskDialogSupported())
	{
		CTaskDialog taskDialog{
			description,
			headline,
			windowTitle.IsEmpty() ? GetTitle() : windowTitle,
			buttons.empty() ? TDCBF_OK_BUTTON : 0,
			bigButtons ? TDF_ENABLE_HYPERLINKS | TDF_USE_COMMAND_LINKS : 0};

		if(icon == MB_ICONINFORMATION)
			taskDialog.SetMainIcon(TD_INFORMATION_ICON);
		else if(icon == MB_ICONWARNING)
			taskDialog.SetMainIcon(TD_WARNING_ICON);
		else if(icon == MB_ICONERROR)
			taskDialog.SetMainIcon(TD_ERROR_ICON);
		else if(icon == MB_ICONQUESTION)
			taskDialog.SetMainIcon(TD_INFORMATION_ICON);  // theApp.LoadStandardIcon(IDI_QUESTION) - this icon hasn't been updated since Vista, and shouldn't be used according to guidelines anyway...

		for(const auto &button : buttons)
		{
			taskDialog.AddCommandControl(button.commandId, button.text);
		}

		if(!verificationText.IsEmpty())
		{
			taskDialog.SetVerificationCheckboxText(verificationText);
			taskDialog.SetVerificationCheckbox(verificationChecked);
		}

		if(defaultButton)
			taskDialog.SetDefaultCommandControl(defaultButton);

		UINT result = static_cast<UINT>(taskDialog.DoModal(parent->GetSafeHwnd()));
		return {result, !verificationText.IsEmpty() ? !!taskDialog.GetVerificationCheckboxState() : false};
	}
#endif

	// Fallback using regular MessageBox
	UINT flags = MB_OK;
	if(buttons.size() <= 1)
	{
		flags = MB_OK;
	} else if(buttons.size() == 2)
	{
		uint32 hasButton = 0;
		for(const auto &button : buttons)
		{
			hasButton |= (1 << button.commandId);
		}
		if(hasButton == ((1 << IDOK) | (1 << IDCANCEL)))
			flags = MB_OKCANCEL;
		else if(hasButton == ((1 << IDYES) | (1 << IDNO)))
			flags = MB_YESNO;
		else if(hasButton == ((1 << IDRETRY) | (1 << IDCANCEL)))
			flags = MB_RETRYCANCEL;
		else
			flags = MB_OKCANCEL;
	} else if(buttons.size() == 3)
	{
		flags = MB_YESNOCANCEL;
	}

	if(defaultButton != IDOK)
	{
		if(defaultButton == IDCANCEL)
			flags |= MB_DEFBUTTON3;
		else if(defaultButton == IDNO)
			flags |= MB_DEFBUTTON2;
	}

	CString messageText = description;
	if(!headline.IsEmpty())
		messageText = headline + _T("\n\n") + messageText;

	// Workaround MessageBox text length limitation: Better show a truncated string than no message at all.
	if(messageText.GetLength() > uint16_max)
	{
		messageText.Truncate(uint16_max);
		messageText.SetAt(uint16_max - 1, _T('.'));
		messageText.SetAt(uint16_max - 2, _T('.'));
		messageText.SetAt(uint16_max - 3, _T('.'));
	}
	UINT result = ::MessageBox(parent->GetSafeHwnd(), messageText, windowTitle.IsEmpty() ? GetTitle() : windowTitle, flags | icon);
	return {result, false};
}


void Reporting::Notification(const AnyStringLocale &text, const CWnd *parent)
{
	TaskDlg{}
		.Description(mpt::ToCString(text))
		.WindowTitle(FillEmptyCaption({}, LogNotification))
		.AddButton(GetButtonString(IDOK), IDOK)
		.DoModal(parent);
}
void Reporting::Notification(const AnyStringLocale &text, const AnyStringLocale &caption, const CWnd *parent)
{
	TaskDlg{}
		.Description(mpt::ToCString(text))
		.WindowTitle(FillEmptyCaption(mpt::ToCString(caption), LogNotification))
		.AddButton(GetButtonString(IDOK), IDOK)
		.DoModal(parent);
}


void Reporting::Information(const AnyStringLocale &text, const CWnd *parent)
{
	TaskDlg{}
		.Description(mpt::ToCString(text))
		.WindowTitle(FillEmptyCaption({}, LogInformation))
		.AddButton(GetButtonString(IDOK), IDOK)
		.Icon(MB_ICONINFORMATION)
		.DoModal(parent);
}
void Reporting::Information(const AnyStringLocale &text, const AnyStringLocale &caption, const CWnd *parent)
{
	TaskDlg{}
		.Description(mpt::ToCString(text))
		.WindowTitle(FillEmptyCaption(mpt::ToCString(caption), LogInformation))
		.AddButton(GetButtonString(IDOK), IDOK)
		.Icon(MB_ICONINFORMATION)
		.DoModal(parent);
}


void Reporting::Warning(const AnyStringLocale &text, const CWnd *parent)
{
	TaskDlg{}
		.Description(mpt::ToCString(text))
		.WindowTitle(FillEmptyCaption({}, LogWarning))
		.AddButton(GetButtonString(IDOK), IDOK)
		.Icon(MB_ICONWARNING)
		.DoModal(parent);
}
void Reporting::Warning(const AnyStringLocale &text, const AnyStringLocale &caption, const CWnd *parent)
{
	TaskDlg{}
		.Description(mpt::ToCString(text))
		.WindowTitle(FillEmptyCaption(mpt::ToCString(caption), LogWarning))
		.AddButton(GetButtonString(IDOK), IDOK)
		.Icon(MB_ICONWARNING)
		.DoModal(parent);
}


void Reporting::Error(const AnyStringLocale &text, const CWnd *parent)
{
	TaskDlg{}
		.Description(mpt::ToCString(text))
		.WindowTitle(FillEmptyCaption({}, LogError))
		.AddButton(GetButtonString(IDOK), IDOK)
		.Icon(MB_ICONERROR)
		.DoModal(parent);
}
void Reporting::Error(const AnyStringLocale &text, const AnyStringLocale &caption, const CWnd *parent)
{
	TaskDlg{}
		.Description(mpt::ToCString(text))
		.WindowTitle(FillEmptyCaption(mpt::ToCString(caption), LogError))
		.AddButton(GetButtonString(IDOK), IDOK)
		.Icon(MB_ICONERROR)
		.DoModal(parent);
}


void Reporting::Message(LogLevel level, const AnyStringLocale &text, const CWnd *parent)
{
	TaskDlg{}
		.Description(mpt::ToCString(text))
		.WindowTitle(FillEmptyCaption({}, level))
		.AddButton(GetButtonString(IDOK), IDOK)
		.Icon(LogLevelToIcon(level))
		.DoModal(parent);
}
void Reporting::Message(LogLevel level, const AnyStringLocale &text, const AnyStringLocale &caption, const CWnd *parent)
{
	TaskDlg{}
		.Description(mpt::ToCString(text))
		.WindowTitle(FillEmptyCaption(mpt::ToCString(caption), level))
		.AddButton(GetButtonString(IDOK), IDOK)
		.Icon(LogLevelToIcon(level))
		.DoModal(parent);
}


ConfirmAnswer Reporting::Confirm(const AnyStringLocale &text, bool showCancel, bool defaultNo, const CWnd *parent)
{
	return Confirm(text, GetTitle() + _T(" - Confirmation"), showCancel, defaultNo, parent);
}
ConfirmAnswer Reporting::Confirm(const AnyStringLocale &text, const AnyStringLocale &caption, bool showCancel, bool defaultNo, const CWnd *parent)
{
	std::vector<TaskDlg::Button> buttonList =
	{
		{GetButtonString(IDYES), IDYES},
		{GetButtonString(IDNO), IDNO}
	};
	if(showCancel)
		buttonList.push_back({GetButtonString(IDCANCEL), IDCANCEL});

	TaskDlg params = TaskDlg{}
		.Description(mpt::ToCString(text))
		.WindowTitle(FillEmptyCaption(mpt::ToCString(caption)))
		.Buttons(buttonList)
		.Icon(MB_ICONQUESTION);

	if(defaultNo)
		params.DefaultButton(IDNO);

	auto result = params.DoModal(parent).buttonId;
	switch(result)
	{
	case IDYES:
		return cnfYes;
	case IDNO:
		return cnfNo;
	default:
	case IDCANCEL:
		return cnfCancel;
	}
}
RetryAnswer Reporting::RetryCancel(const AnyStringLocale &text, const CWnd *parent)
{
	return RetryCancel(text, GetTitle(), parent);
}
RetryAnswer Reporting::RetryCancel(const AnyStringLocale &text, const AnyStringLocale &caption, const CWnd *parent)
{
	auto result = TaskDlg{}
		.Description(mpt::ToCString(text))
		.WindowTitle(FillEmptyCaption(mpt::ToCString(caption)))
		.Buttons({{GetButtonString(IDRETRY), IDRETRY}, {GetButtonString(IDCANCEL), IDCANCEL}})
		.DoModal(parent).buttonId;
	return (result == IDRETRY) ? rtyRetry : rtyCancel;
}


OPENMPT_NAMESPACE_END
