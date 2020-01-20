# Set CMake minimum version and CMake policy required by UrhoCommon module
CMAKE_MINIMUM_REQUIRED(VERSION 3.2.3)
IF(COMMAND cmake_policy)
    # Libraries linked via full path no longer produce linker search paths
    CMAKE_POLICY(SET CMP0003 NEW)
    # INTERFACE_LINK_LIBRARIES defines the link interface
    CMAKE_POLICY(SET CMP0022 NEW)
    # Disallow use of the LOCATION target property - so we set to OLD as we still need it
    CMAKE_POLICY(SET CMP0026 OLD)
    # MACOSX_RPATH is enabled by default
    CMAKE_POLICY(SET CMP0042 NEW)
    # Honor the visibility properties for SHARED target types only
    CMAKE_POLICY(SET CMP0063 OLD)
ENDIF()

# Set project name
PROJECT(OSP-MAGNUM CXX)

OPTION(OSP_BUILD_SANATIZER          "Build with the address sanatizer" OFF)
OPTION(OSP_WARNINGS_ARE_ERRORS      "Build with the flag -Werror" OFF)
OPTION(OSP_ENABLE_COMPILER_WARNINGS "Build with the majority of compiler warnings enabled" OFF)

# Define target name
SET(TARGET_NAME OSP-MAGNUM)

# Set the explicit standard revision for C and C++ code.
SET(CMAKE_C_STANDARD 11)
SET(CMAKE_C_EXTENSIONS OFF)
SET(CMAKE_C_STANDARD_REQUIRED ON)

SET(CMAKE_CXX_STANDARD 14)
SET(CMAKE_CXX_EXTENSIONS OFF)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

# If the environment has this set we don't want to clear it.
SET(CLANG_COMPILE_FLAGS "${CLANG_COMPILE_FLAGS}")
SET(GCC_COMPILE_FLAGS   "${GCC_COMPILE_FLAGS}")

# Compiler warnings can help find problems in code.
IF(OSP_ENABLE_COMPILER_WARNINGS)
  # Enable additional warnings.
  # -fno-omit-frame-pointer significantly helps with debugging of the application.
  SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wall -Wextra -fno-omit-frame-pointer")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -fno-omit-frame-pointer")

  # Urho3D has unused parameters everywhere. This just makes too much noise.
  SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wno-unused-parameter")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")

  # Urho3D has poor type aliasing safety everywhere. This just makes too much noise.
  SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wno-strict-aliasing")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-aliasing")

  # These are hand selected warnings that are enabled by -Wextra that tend to be non-issues, so disable them explicitly for sake of avoiding build noise.
  SET(CLANG_COMPILE_FLAGS "${CLANG_COMPILE_FLAGS} -Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-padded -Wno-undefined-func-template -Wno-unused-template")
  SET(CLANG_COMPILE_FLAGS "${CLANG_COMPILE_FLAGS} -Wno-deprecated -Wno-documentation -Wno-documentation-deprecated-sync -Wno-documentation-unknown-command -Wno-abstract-vbase-init")
  SET(CLANG_COMPILE_FLAGS "${CLANG_COMPILE_FLAGS} -Wno-switch-enum -Wno-covered-switch-default -Wno-missing-prototypes -Wno-sign-conversion -Wno-float-conversion -Wno-shorten-64-to-32")
  SET(CLANG_COMPILE_FLAGS "${CLANG_COMPILE_FLAGS} -Wno-shadow -Wno-shadow-field-in-constructor -Wshadow-uncaptured-local -Wno-inconsistent-missing-destructor-override")
  SET(CLANG_COMPILE_FLAGS "${CLANG_COMPILE_FLAGS} -Wno-global-constructors -Wno-exit-time-destructors  -Wno-weak-vtables -Wno-undef -Wno-disabled-macro-expansion")

  # This relatively rare warning can be emitted because of third party code, and is basically not our problem.
  SET(GCC_COMPILE_FLAGS "${GCC_COMPILE_FLAGS} -Wno-psabi")
ENDIF() # OSP_ENABLE_COMPILER_EARNINGS

# Compiler warnings can help find problems in code.
IF(OSP_WARNINGS_ARE_ERRORS)
  SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Werror")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
ENDIF() # OSP_WARNINGS_ARE_ERRORS

# The sanatizers provide compile time code instrumentation that drastically improve the ability of programmars to find bugs.
IF(OSP_BUILD_SANATIZER)
  SET(GCC_COMPILE_FLAGS "${GCC_COMPILE_FLAGS} -fsanitize=address -fsanitize-address-use-after-scope -fsanitize=pointer-compare -fsanitize=pointer-subtract -fsanitize=leak")
  SET(GCC_COMPILE_FLAGS "${GCC_COMPILE_FLAGS} -fstack-protector-all -fsanitize=bounds -fsanitize=enum")
ENDIF() # OSP_BUILD_SANATIZER

IF(CMAKE_C_COMPILER_ID MATCHES "Clang")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CLANG_COMPILE_FLAGS}")
ENDIF()
IF(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CLANG_COMPILE_FLAGS}")
ENDIF()

IF(CMAKE_C_COMPILER_ID MATCHES "GNU")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${GCC_COMPILE_FLAGS}")
ENDIF()
IF(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_COMPILE_FLAGS}")
ENDIF()

# In debug builds, disable optimizations (just in case they were enabled by a previous flag, or env variable)
# and mark the debug symbols to be compressed, which can be a huge saving in terms of file size.
SET(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}   -gz -O0")
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -gz -O0")

IF(CMAKE_BUILD_TYPE MATCHES "Debug")
# Don't enable LTO in debug builds.
ELSE()
  CMAKE_POLICY(SET CMP0069 NEW)
  INCLUDE(CheckIPOSupported)
  CHECK_IPO_SUPPORTED(RESULT lto_supported OUTPUT error)

  IF( lto_supported )
    MESSAGE(STATUS "IPO / LTO enabled")
    SET(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
  ELSE()
    MESSAGE(STATUS "IPO / LTO not supported: <${error}>")
  ENDIF()
ENDIF()

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/modules/" ${CMAKE_MODULE_PATH})

# Process subdirectory full of third-party libraries
ADD_SUBDIRECTORY(3rdparty)

# Also process the src subdirectory, which describes the source code for the project.
ADD_SUBDIRECTORY(src)

