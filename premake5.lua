workspace "parallel"
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

project "parallel-gl"
  kind "staticlib"
  language "c++"

  files { "sources/gl/**.cc"
        , "include/parallel/gl/**.hh"
        , "include/GL/**.h"
        }

project "parallel-amp"
  kind "staticlib"
  language "c++"

  files { "sources/amp/**.cc"
        , "include/parallel/amp/**.hh"
        }

project "parallel-tests"
  kind "consoleapp"
  language "c++"
  links { "parallel-gl" }

  files { "tests/**.cc"
        , "tests/**.hh"
        }
