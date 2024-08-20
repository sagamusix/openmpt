/*
 * CScriptableEdit.h
 * -----------------
 * Purpose: An edit UI item whose contents can be automatically evaluated as a Lua expression.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mpt/base/saturate_round.hpp"
#include "CDecimalSupport.h"

OPENMPT_NAMESPACE_BEGIN

class CScriptableEdit : public CEdit, public CDecimalSupport<CScriptableEdit>
{
public:
	CScriptableEdit() { AllowExpressions(true); }

	template<typename T>
	std::optional<T> GetValue()
	{
		const auto value = GetValue<double>();
		if(!value)
			return std::nullopt;

		if constexpr(std::numeric_limits<T>::is_integer)
			return mpt::saturate_round<T>(*value);
		else
			return static_cast<T>(*value);
	}

	template<>
	std::optional<double> GetValue();

protected:
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg LPARAM OnPaste(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
