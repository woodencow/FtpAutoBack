# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/24289/ftpsrv/build/switch/_deps/minini-src")
  file(MAKE_DIRECTORY "/home/24289/ftpsrv/build/switch/_deps/minini-src")
endif()
file(MAKE_DIRECTORY
  "/home/24289/ftpsrv/build/switch/_deps/minini-build"
  "/home/24289/ftpsrv/build/switch/_deps/minini-subbuild/minini-populate-prefix"
  "/home/24289/ftpsrv/build/switch/_deps/minini-subbuild/minini-populate-prefix/tmp"
  "/home/24289/ftpsrv/build/switch/_deps/minini-subbuild/minini-populate-prefix/src/minini-populate-stamp"
  "/home/24289/ftpsrv/build/switch/_deps/minini-subbuild/minini-populate-prefix/src"
  "/home/24289/ftpsrv/build/switch/_deps/minini-subbuild/minini-populate-prefix/src/minini-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/24289/ftpsrv/build/switch/_deps/minini-subbuild/minini-populate-prefix/src/minini-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/24289/ftpsrv/build/switch/_deps/minini-subbuild/minini-populate-prefix/src/minini-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
