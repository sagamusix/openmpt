/*
 * CScriptableEdit.cpp
 * -------------------
 * Purpose: An edit UI item whose contents can be automatically evaluated as a Lua expression.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "CScriptableEdit.h"
#include "Reporting.h"
#include "scripting/LuaVM.h"

OPENMPT_NAMESPACE_BEGIN

BEGIN_MESSAGE_MAP(CScriptableEdit, CEdit)
	ON_WM_CHAR()
	ON_MESSAGE(WM_PASTE, &CScriptableEdit::OnPaste)
END_MESSAGE_MAP()


void CScriptableEdit::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	bool handled = false;
	CDecimalSupport<CScriptableEdit>::OnChar(0, nChar, 0, handled);
	if(!handled)
		CEdit::OnChar(nChar, nRepCnt, nFlags);
}


LPARAM CScriptableEdit::OnPaste(WPARAM wParam, LPARAM lParam)
{
	bool handled = false;
	CDecimalSupport<CScriptableEdit>::OnPaste(0, wParam, lParam, handled);
	if(!handled)
		return CEdit::DefWindowProc(WM_PASTE, wParam, lParam);
	else
		return 0;
}


template<>
std::optional<double> CScriptableEdit::GetValue()
{
	try
	{
		Scripting::LuaVM lua;
		return lua.Eval<double>("return " + mpt::ToCharset(mpt::Charset::UTF8, GetExpression()));
	} catch(const Scripting::LuaVM::Error &e)
	{
		Reporting::Error(e.what(), "Invalid expression");
		SetFocus();
	}
	return std::nullopt;
}

OPENMPT_NAMESPACE_END
