##
# Open Space Program
# Copyright © 2019-2020 Open Space Program Project
#
# MIT License
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
##

# Set CMake minimum version and CMake policy
CMAKE_MINIMUM_REQUIRED(VERSION 3.12.4)

# Enable link time optimization
CMAKE_POLICY(SET CMP0069 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

# Add the module search directory
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/modules/")

IF(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows" AND NOT CMAKE_SYSTEM_VERSION)
    MESSAGE("- CMAKE_SYSTEM_VERSION is not set. This setting controls which version of the Windows SDK to use. If it is not set, then OSP-MAGNUM will automatically select 10.0.18362.0.")
    MESSAGE("- OSP-MAGNUMs CMake configuration automatically sets this value because Microsoft has decided to not repair Windows SDK versions from 10.0.17763.0 or older to work with C++17")
    MESSAGE("- So we must set this value to an SDK version higher than that, or you'll get compile failures. However, CMake does not have any mechanism to automatically detect the available")
    MESSAGE("- SDK versions and select an appropriate one to use out of what's available. And the only way to find out what SDK version will be used by default is to enable the C or CXX languages")
    MESSAGE("- But once those languages are enabled, the SDK version can no longer be changed from what CMake selected. As a result, we have no choice but to set this version if one isn't supplied")
    MESSAGE("- If your system has a newer version of the SDK, then pass the -DCMAKE_SYSTEM_VERSION=\"Version String Goes Here\" parameter to CMake during the configuration stage.")
    SET(CMAKE_SYSTEM_VERSION "10.0.18362.0" CACHE INTERNAL "" FORCE)
ENDIF()

# Set project name
PROJECT(OSP-MAGNUM CXX)

# Setup things for the test subdir below
# Has to be in toplevel CMakeLists.txt or ctest gets mad.
enable_testing()

OPTION(OSP_BUILD_SANATIZER          "Build with the address sanatizer" OFF)
OPTION(OSP_WARNINGS_ARE_ERRORS      "Build with the flag -Werror" OFF)
OPTION(OSP_ENABLE_COMPILER_WARNINGS "Build with the majority of compiler warnings enabled" OFF)
OPTION(OSP_ENABLE_IWYU              "Build with warnings from IWYU turned on" OFF)
OPTION(OSP_ENABLE_CLANG_TIDY        "Build with warnings from clang-tidy turned on" OFF)
OPTION(OSP_USE_SYSTEM_SDL           "Build with SDL that you provide if turned on, compiles SDL if turned off. Off by default" OFF)

# If the environment has these set, pull them into proper variables.
SET(CLANG_COMPILE_FLAGS ${CLANG_COMPILE_FLAGS})
SET(GCC_COMPILE_FLAGS   ${GCC_COMPILE_FLAGS})

# -fno-omit-frame-pointer significantly helps with debugging of the application if built with optimizations
if(NOT MSVC)
    add_compile_options(-fno-omit-frame-pointer)
endif()

# Compiler warnings can help find problems in code.
IF(OSP_ENABLE_COMPILER_WARNINGS)
  # Enable additional warnings.
  add_compile_options(-Wall -Wextra)

  # These are hand selected warnings that are enabled by -Wextra that tend to be non-issues, so disable them explicitly for sake of avoiding build noise.
  LIST(APPEND CLANG_COMPILE_FLAGS -Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-padded -Wno-undefined-func-template -Wno-unused-template)
  LIST(APPEND CLANG_COMPILE_FLAGS -Wno-deprecated -Wno-documentation -Wno-documentation-deprecated-sync -Wno-documentation-unknown-command -Wno-abstract-vbase-init)
  LIST(APPEND CLANG_COMPILE_FLAGS -Wno-switch-enum -Wno-covered-switch-default -Wno-missing-prototypes -Wno-sign-conversion -Wno-float-conversion -Wno-shorten-64-to-32)
  LIST(APPEND CLANG_COMPILE_FLAGS -Wno-shadow -Wno-shadow-field-in-constructor -Wshadow-uncaptured-local -Wno-inconsistent-missing-destructor-override)
  LIST(APPEND CLANG_COMPILE_FLAGS -Wno-global-constructors -Wno-exit-time-destructors  -Wno-weak-vtables -Wno-undef -Wno-disabled-macro-expansion)

  # This relatively rare warning can be emitted because of third party code, and is basically not our problem.
  LIST(APPEND GCC_COMPILE_FLAGS -Wno-psabi)
ENDIF() # OSP_ENABLE_COMPILER_EARNINGS

# Compiler warnings can help find problems in code.
IF(OSP_WARNINGS_ARE_ERRORS)
  add_compile_options(-Werror)
ENDIF() # OSP_WARNINGS_ARE_ERRORS

# The sanatizers provide compile time code instrumentation that drastically improve the ability of programmars to find bugs.
IF(OSP_BUILD_SANATIZER)
  add_link_options(-fstack-protector-all -fsanitize=address,bounds,enum,leak,pointer-compare,pointer-subtract -fsanitize-address-use-after-scope)
  add_compile_options(-fstack-protector-all -fsanitize=address,bounds,enum,leak,pointer-compare,pointer-subtract -fsanitize-address-use-after-scope)
ENDIF() # OSP_BUILD_SANATIZER

IF(CLANG_COMPILE_FLAGS AND CMAKE_C_COMPILER_ID MATCHES "Clang")
  add_compile_options("$<$<COMPILE_LANGUAGE:C>:${CLANG_COMPILE_FLAGS}>")
ENDIF()
IF(CLANG_COMPILE_FLAGS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:${CLANG_COMPILE_FLAGS}>")
ENDIF()

IF(GCC_COMPILE_FLAGS AND CMAKE_C_COMPILER_ID MATCHES "GNU")
  add_compile_options("$<$<COMPILE_LANGUAGE:C>:${GCC_COMPILE_FLAGS}>")
ENDIF()
IF(GCC_COMPILE_FLAGS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:${GCC_COMPILE_FLAGS}>")
ENDIF()

# In debug builds, disable optimizations (just in case they were enabled by a previous flag, or env variable)
# and mark the debug symbols to be compressed, which can be a huge saving in terms of file size.
IF(MSVC)
ELSE()
  add_compile_options($<$<CONFIG:Debug>:-O0>)
  add_compile_options($<$<CONFIG:Debug>:-gz>)
ENDIF()

IF(CMAKE_BUILD_TYPE MATCHES "Debug")
# Don't enable LTO in debug builds.
ELSE()
  INCLUDE(CheckIPOSupported)
  CHECK_IPO_SUPPORTED(RESULT lto_supported OUTPUT error)
  IF( lto_supported )
    MESSAGE(STATUS "IPO / LTO enabled")
    SET(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    SET(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG FALSE)
  ELSE()
    MESSAGE(STATUS "IPO / LTO not supported: <${error}>")
  ENDIF()
ENDIF()

# Force Runtime output directory into Debug or Release folder
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG> CACHE PATH "")
IF(MSVC)
  # Put shared libraries in the same directory as the executable on windows
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG> CACHE PATH "")
  # Force multiprocessor compilation for all projects
  add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)
  # Force dynamic runtime library to be in debug mode in debug mode.
  add_compile_options($<$<CONFIG:Debug>:$<$<CXX_COMPILER_ID:MSVC>:/MDd>>)
  # Don't generate a blah.manifest file for each build target.
  add_link_options(LINKER:/MANIFEST:NO)
ELSEIF(UNIX)
  # Put shared libraries in ./lib, and use RPATH
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/lib CACHE PATH "")
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
  # Make sure that newer toolchains, like ubuntu 22.04, don't
  # use runpath, and use rpath instead.
  IF(NOT APPLE)
    add_link_options(LINKER:--disable-new-dtags)
  ENDIF()
ENDIF()

# Process subdirectory full of third-party libraries
ADD_SUBDIRECTORY(3rdparty)

# Also process the src subdirectory, which describes the source code for the project.
ADD_SUBDIRECTORY(src)

# Unit Tests
ADD_SUBDIRECTORY(test)

# Set OSP as default startup project in Visual Studio
set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT osp-magnum)
# Set execution directory of osp-magnum so that we don't have to copy the files
set_property(TARGET osp-magnum PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
