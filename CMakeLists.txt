# Copyright 2024 - 2026 Khalil Estell and the libhal contributors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.21)

project(libexcept LANGUAGES CXX)

# No dependency on libhal-cmake-util: this file is meant to also work as a
# plain FetchContent/add_subdirectory dependency for people outside the
# Conan+libhal toolchain, so it only assumes vanilla CMake plus libhal-util
# (a real source dependency of the runtime, not build tooling).
if(NOT TARGET libhal::util)
  find_package(libhal-util REQUIRED)
endif()

set(SOURCE_LIST
    runtime/armv7/exception.cpp
    runtime/armv7/libunwind_wrappers.cpp
)

# Built as an OBJECT library rather than STATIC. A static archive only pulls
# in the member .o files the linker can see are referenced, and nothing
# visibly references the __wrap_* symbols in exception.cpp until the linker
# performs --wrap substitution - so without forcing the issue, the archive
# member never gets linked in at all. That used to be solved by having
# consumers pass -Wl,--whole-archive around libexcept.a. An OBJECT library's
# .o files are instead placed on the link line unconditionally by CMake
# whenever something links against it, so the same problem doesn't occur and
# --whole-archive is no longer needed. The Conan package reuses this same
# property directly: it ships the loose .o files and lists them as plain
# link flags instead of installing a CMake package config for them (see
# conanfile.py).
add_library(libexcept OBJECT ${SOURCE_LIST})
add_library(libexcept::libexcept ALIAS libexcept)

target_include_directories(libexcept PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(libexcept PUBLIC libhal::util)
target_compile_features(libexcept PUBLIC cxx_std_20)

target_compile_options(libexcept PRIVATE
  -g -Werror -Wall -Wextra -Wshadow -fexceptions -fno-rtti
  -save-temps=obj
  # exception.cpp's ARM unwinder uses a computed-goto jump table (GNU label-
  # as-value extension), which Clang warns on by default even without
  # -pedantic.
  $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-gnu-label-as-value>
)

include(GNUInstallDirs)
install(TARGETS libexcept OBJECTS DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Only build tests when this is the top-level project, not when pulled in
# via FetchContent/add_subdirectory by someone else.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND NOT CMAKE_CROSSCOMPILING)
  find_package(ut REQUIRED)

  add_executable(test_main tests/main.test.cpp)
  target_link_libraries(test_main PRIVATE libexcept::libexcept Boost::ut)
  target_compile_features(test_main PRIVATE cxx_std_20)

  add_executable(test_new tests/new.test.cpp)
  target_link_libraries(test_new PRIVATE libexcept::libexcept Boost::ut)
  target_compile_features(test_new PRIVATE cxx_std_20)

  enable_testing()
  add_test(NAME test_main COMMAND test_main)
  add_test(NAME test_new COMMAND test_new)
endif()
