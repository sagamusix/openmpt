/*
 * LuaGlobals.h
 * ------------
 * Purpose: Lua wrappers for CModDoc and contained classes
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once
#include "LuaObject.h"

OPENMPT_NAMESPACE_BEGIN

namespace Scripting
{

#define FadeLaw(X) \
	X(LINEAR) \
	X(EXPONENTIAL) \
	X(SQUARE_ROOT) \
	X(LOGARITHMIC) \
	X(QUARTER_SINE) \
	X(HALF_SINE)

MPT_MAKE_ENUM(FadeLaw)
#undef FadeLaw

}  // namespace Scripting

OPENMPT_NAMESPACE_END
