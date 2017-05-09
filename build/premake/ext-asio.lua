  
 project "asio"
  uuid "3A818D8E-9101-4539-A3A0-65F441D063F3"
  language "C++"
  location ( "../../build/" .. mpt_projectpathname .. "/ext" )
  mpt_projectname = "asio"
  dofile "../../build/premake/premake-defaults-LIB.lua"
  dofile "../../build/premake/premake-defaults.lua"
  dofile "../../build/premake/premake-defaults-winver.lua"
  targetname "openmpt-asio"
  includedirs { "../../include/asio/include" }
	filter {}
	filter { "action:vs*" }
		characterset "Unicode"
	filter {}
  defines {
   "UNICODE",
   "_UNICODE",
   "ASIO_SEPARATE_COMPILATION",
   "ASIO_STANDALONE",
   --"ASIO_ENABLE_HANDLER_TRACKING",
  }
  files {
   "../../include/asio/include/**.hpp",
  }
  files {
   "../../include/asio/src/asio.cpp",
   --"../../include/asio/src/asio_ssl.cpp",
  }
