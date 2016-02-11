workspace "radix-sort"
  configurations { "debug", "release" }
  platforms { "x86", "x64" }
  location "build"

  includedirs "include"
  flags "staticruntime"

  filter "configurations:debug"
    flags "symbols"

  filter "configurations:release"
    defines "NDEBUG"
    optimize "full"

  filter "platforms:x86"
    architecture "x32"
    targetsuffix "-x86"

  filter "platforms:x64"
    architecture "x64"
    targetsuffix "-x64"

project "radix-sort"
  kind "staticlib"
  language "c++"

  files { "sources/**.cc"
        , "include/**.hh"
        , "include/**.h"
        }

project "radix-sort.tests"
  kind "consoleapp"
  language "c++"
  links { "radix-sort" }

  files { "tests/**.cc"
        , "tests/**.hh"
        }
