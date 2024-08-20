  
 project "lua"
  uuid "5C2FAD13-3006-4D75-AF2A-8C50A6B533DB"
  language "C++"
  compileas "C++" -- so that lua uses exceptions instead of longjmp
  location ( "%{wks.location}" .. "/ext" )
  mpt_kind "default"
  targetname "openmpt-lua"

  includedirs { "../../include/lua/src" }

  defines { "LUA_USER_H=\"../openmpt.h\"" }

  filter { "configurations:Debug" }
    defines { "LUA_USE_APICHECK" }
  filter { }

  files {
   "../../include/lua/openmpt.c",
   "../../include/lua/openmpt.h",
  }
  files {
   "../../include/lua/src/*.c",
  }
  removefiles { "../../include/lua/src/lua.c", "../../include/lua/src/luac.c" }
  files {
   "../../include/lua/src/*.h",
   "../../include/lua/src/*.hpp",
  }
  files {
     "../../include/lua-sol2/single/include/sol/config.hpp",
     "../../include/lua-sol2/single/include/sol/sol.hpp",
     "../../include/lua-sol2/single/include/sol/forward.hpp",
  }

  filter { "action:vs*" }
    buildoptions { "/wd4334" }
  filter {}

function mpt_use_lua ()
	filter {}
	filter { "action:vs*" }
		includedirs {
			"../../include/lua/src",
			"../../include/lua-sol2/single/include",
		}
	filter { "not action:vs*" }
		externalincludedirs {
			"../../include/lua/src",
			"../../include/lua-sol2/single/include",
		}
	filter {}
	links {
		"lua",
	}
	filter {}
end
