# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/nariakiiwatani/Works/HokuyoHub/proj/external/urg_library/release/urg_library-1.2.7")
  file(MAKE_DIRECTORY "/Users/nariakiiwatani/Works/HokuyoHub/proj/external/urg_library/release/urg_library-1.2.7")
endif()
file(MAKE_DIRECTORY
  "/Users/nariakiiwatani/Works/HokuyoHub/proj/build/darwin-arm64/urg_library_proj-prefix/src/urg_library_proj-build"
  "/Users/nariakiiwatani/Works/HokuyoHub/proj/build/darwin-arm64/urg_library_proj-prefix"
  "/Users/nariakiiwatani/Works/HokuyoHub/proj/build/darwin-arm64/urg_library_proj-prefix/tmp"
  "/Users/nariakiiwatani/Works/HokuyoHub/proj/build/darwin-arm64/urg_library_proj-prefix/src/urg_library_proj-stamp"
  "/Users/nariakiiwatani/Works/HokuyoHub/proj/build/darwin-arm64/urg_library_proj-prefix/src"
  "/Users/nariakiiwatani/Works/HokuyoHub/proj/build/darwin-arm64/urg_library_proj-prefix/src/urg_library_proj-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/nariakiiwatani/Works/HokuyoHub/proj/build/darwin-arm64/urg_library_proj-prefix/src/urg_library_proj-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/nariakiiwatani/Works/HokuyoHub/proj/build/darwin-arm64/urg_library_proj-prefix/src/urg_library_proj-stamp${cfgdir}") # cfgdir has leading slash
endif()
