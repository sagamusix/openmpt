/*
 * LuaVM.h
 * -------
 * Purpose: Wrapper class for a Lua VM instance.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#ifdef _DEBUG
#define SOL_ALL_SAFETIES_ON 1
#endif
//#define SOL_STRINGS_ARE_NUMBERS 1
//#define SOL_NO_CHECK_NUMBER_PRECISION 1
//#define SOL_NO_COMPAT 1
#define SOL_SAFE_STACK_CHECK 1
#define SOL_NO_LUA_HPP 1
#define SOL_USING_CXX_LUA 1
#include <sol/sol.hpp>
#include <atomic>
#include <functional>


OPENMPT_NAMESPACE_BEGIN

namespace Scripting
{

class LuaVM
{
protected:

	sol::state lua;
	std::atomic<bool> m_terminate = false;
	std::function<void()> m_idleFunc;

public:

	using Error = sol::error;

	// Create new VM
	LuaVM()
	{
		GetInstance(lua) = this;
		lua.open_libraries();

		// Hook for stopping scripts that are stuck
		lua_sethook(lua, Hook, LUA_MASKCOUNT, 500);
	}

	LuaVM(LuaVM &&) = delete;
	LuaVM(const LuaVM &) = delete;

	operator sol::state&() { return lua; }
	operator lua_State*() { return lua; }

	static LuaVM*& GetInstance(lua_State *L)
	{
		static_assert(LUA_EXTRASPACE >= sizeof(void *));
		return *static_cast<LuaVM **>(lua_getextraspace(L));
	}

	static void Hook(lua_State *L, lua_Debug *)
	{
		LuaVM *vm = GetInstance(L);
		if(vm != nullptr)
		{
			if(vm->m_idleFunc)
				vm->m_idleFunc();
			bool expected = true;
			if(vm->m_terminate.compare_exchange_strong(expected, false))
				luaL_error(L, "Terminated by user.");
		}
	}

	std::string ToString(sol::object o) const
	{
		return lua["tostring"](o);
	}

	// Evaluate a Lua expression
	// May throw LuaVM::Error.
	template<typename T>
	T Eval(const sol::string_view &evalScript)
	{
		auto res = lua.do_string(evalScript);
		if(!res.valid())
		{
			sol::error err = res;
			throw Error(err.what());
		}
		sol::optional<T> value = res;
		if(!value)
		{
			throw Error("Incompatible expression type");
		}
		return value.value();
	}

	void RequestTermination()
	{
		m_terminate = true;
		// TODO wait on m_terminate
	}

	void SetIdleFunction(std::function<void()> idleFunc)
	{
		m_idleFunc = std::move(idleFunc);
	}
};

}  // namespace Scripting

OPENMPT_NAMESPACE_END
