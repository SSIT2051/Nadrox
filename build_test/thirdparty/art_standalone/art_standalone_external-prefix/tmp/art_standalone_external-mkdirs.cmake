# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/ssit/Projects/Nadrox/build_test/thirdparty/art_standalone/src")
  file(MAKE_DIRECTORY "/home/ssit/Projects/Nadrox/build_test/thirdparty/art_standalone/src")
endif()
file(MAKE_DIRECTORY
  "/home/ssit/Projects/Nadrox/build_test/thirdparty/art_standalone/src"
  "/home/ssit/Projects/Nadrox/build_test/thirdparty/art_standalone/art_standalone_external-prefix"
  "/home/ssit/Projects/Nadrox/build_test/thirdparty/art_standalone/art_standalone_external-prefix/tmp"
  "/home/ssit/Projects/Nadrox/build_test/thirdparty/art_standalone/art_standalone_external-prefix/src/art_standalone_external-stamp"
  "/home/ssit/Projects/Nadrox/build_test/thirdparty/art_standalone/art_standalone_external-prefix/src"
  "/home/ssit/Projects/Nadrox/build_test/thirdparty/art_standalone/art_standalone_external-prefix/src/art_standalone_external-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/ssit/Projects/Nadrox/build_test/thirdparty/art_standalone/art_standalone_external-prefix/src/art_standalone_external-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/ssit/Projects/Nadrox/build_test/thirdparty/art_standalone/art_standalone_external-prefix/src/art_standalone_external-stamp${cfgdir}") # cfgdir has leading slash
endif()
