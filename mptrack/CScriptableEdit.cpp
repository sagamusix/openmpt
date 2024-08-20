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

template<>
std::optional<double> CScriptableEdit::GetValue()
{
	try
	{
		CString value;
		GetWindowText(value);
		Scripting::LuaVM lua;
		return lua.Eval<double>("return " + mpt::ToCharset(mpt::Charset::UTF8, value));
	} catch(const Scripting::LuaVM::Error &e)
	{
		Reporting::Error(e.what(), "Invalid expression");
		SetFocus();
	}
	return std::nullopt;
}

OPENMPT_NAMESPACE_END
