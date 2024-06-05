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

##
# Mark this directory and all scoped subdirectories as "SYSTEM" directories to try to squash warnings
# This property should mark all targets created in this directory and subdirs as third party code.
##
set_directory_properties(PROPERTIES SYSTEM TRUE)

##
# SDL
##

SET(INSTALL_GTEST    OFF CACHE BOOL "" FORCE)
SET(BUILD_TESTING    OFF CACHE BOOL "" FORCE)
SET(BUILD_DEPRECATED OFF CACHE BOOL "" FORCE)
SET(BUILD_GMOCK      OFF CACHE BOOL "" FORCE)
SET(BUILD_STATIC     ON  CACHE BOOL "" FORCE) # Applies to more than one third party...

if(NOT OSP_USE_SYSTEM_SDL)
  # This is the most minimal set of features which still make Sdl2Application
  # work. If you need something from these, remove the setting. The SDL_AUDIO
  # option (and SDL_DLOPEN) should not be needed either as Magnum doesn't use it,
  # but if it's disabled it causes linker errors. Needs further investigation.
  SET(SDL_ATOMIC            OFF CACHE BOOL "" FORCE)
  SET(SDL_CPUINFO           OFF CACHE BOOL "" FORCE)
  SET(SDL_FILESYSTEM        OFF CACHE BOOL "" FORCE)
  SET(SDL_HAPTIC            OFF CACHE BOOL "" FORCE)
  SET(SDL_LOCALE            OFF CACHE BOOL "" FORCE)
  SET(SDL_POWER             OFF CACHE BOOL "" FORCE)
  SET(SDL_RENDER            OFF CACHE BOOL "" FORCE)
  SET(SDL_SENSOR            OFF CACHE BOOL "" FORCE)
  SET(SDL_TIMERS            OFF CACHE BOOL "" FORCE)
  SET(SDL_ALTIVEC           OFF CACHE BOOL "" FORCE) # POWERPC exclusive, not supported by osp-magnum
  SET(SDL_3DNOW             OFF CACHE BOOL "" FORCE) # Old 32bit x86 chips only, not supported by osp-magnum.
  SET(SDL_MMX               OFF CACHE BOOL "" FORCE) # Old. Superceeded by SSE1, not supported by osp-magnum.
  SET(SDL_ASSEMBLY          OFF CACHE BOOL "" FORCE) # Presumably not actually needed. Lets try more for portability than speed...

  ##
  # SDL Audio backend settings
  ##
  SET(SDL_AUDIO             ON  CACHE BOOL "" FORCE)

  # Enabled audio settings
  SET(SDL_PULSEAUDIO        ON  CACHE BOOL "" FORCE) # Most linux systems use pulseaudio
  SET(SDL_PULSEAUDIO_SHARED OFF CACHE BOOL "" FORCE) # Don't need dynamic lib support for pulse, yet

  # Disabled audio settings
  SET(SDL_PIPEWIRE          OFF CACHE BOOL "" FORCE)
  SET(SDL_PIPEWIRE_SHARED   OFF CACHE BOOL "" FORCE)
  SET(SDL_ALSA              OFF CACHE BOOL "" FORCE)
  SET(SDL_ALSA_SHARED       OFF CACHE BOOL "" FORCE)
  SET(SDL_JACK              OFF CACHE BOOL "" FORCE)
  SET(SDL_JACK_SHARED       OFF CACHE BOOL "" FORCE)
  SET(SDL_DISKAUDIO         OFF CACHE BOOL "" FORCE)
  SET(SDL_DISKAUDIO_SHARED  OFF CACHE BOOL "" FORCE)
  SET(SDL_SNDIO             OFF CACHE BOOL "" FORCE)
  SET(SDL_SNDIO_SHARED      OFF CACHE BOOL "" FORCE)
  SET(SDL_DUMMYAUDIO        OFF CACHE BOOL "" FORCE)
  SET(SDL_DUMMYAUDIO_SHARED OFF CACHE BOOL "" FORCE)
  SET(SDL_ESD               OFF CACHE BOOL "" FORCE)
  SET(SDL_ESD_SHARED        OFF CACHE BOOL "" FORCE)
  SET(SDL_ARTS              OFF CACHE BOOL "" FORCE)
  SET(SDL_ARTS_SHARED       OFF CACHE BOOL "" FORCE)
  SET(SDL_NAS               OFF CACHE BOOL "" FORCE)
  SET(SDL_NAS_SHARED        OFF CACHE BOOL "" FORCE)
  SET(SDL_OSS               OFF CACHE BOOL "" FORCE)
  SET(SDL_OSS_SHARED        OFF CACHE BOOL "" FORCE)
  SET(SDL_WASAPI            OFF CACHE BOOL "" FORCE) # Windows Audio Session API : Is this really needed?

  ##
  # SDL Video backend settings
  ##
  SET(SDL_VIDEO              ON  CACHE BOOL "" FORCE)

  # Enabled video settings
  SET(SDL_OPENGL             ON  CACHE BOOL "" FORCE)

  # Disabled video settings
  SET(SDL_DUMMYVIDEO         OFF CACHE BOOL "" FORCE)
  SET(SDL_VULKAN             OFF CACHE BOOL "" FORCE) # Vulkan is cool and all, but one project at a time.
  SET(SDL_OPENGLES           OFF CACHE BOOL "" FORCE)
  SET(SDL_DIRECTFB           OFF CACHE BOOL "" FORCE)
  SET(SDL_DIRECTX            OFF CACHE BOOL "" FORCE) # DirectX is windows specific. Lets stick with multiplatform.
  SET(SDL_RENDER_D3D         OFF CACHE BOOL "" FORCE) # Ancient DirectX?
  if(NOT APPLE)
    SET(SDL_COCOA              OFF CACHE BOOL "" FORCE) # Cocoa is mac specific. Lets stick with multiplatform.
    SET(SDL_METAL              OFF CACHE BOOL "" FORCE) # Metal is mac specific. Lets stick with multiplatform.
    SET(SDL_RENDER_METAL       OFF CACHE BOOL "" FORCE) # Metal is mac specific. Lets stick with multiplatform.
  endif()
  SET(SDL_RPI                OFF CACHE BOOL "" FORCE) # Raspberry PI is not multiplatform. It supports OpenGL.
  SET(SDL_WAYLAND            OFF CACHE BOOL "" FORCE)
  SET(SDL_WAYLAND_QT_TOUCH   OFF CACHE BOOL "" FORCE)
  SET(SDL_KMSDRM             OFF CACHE BOOL "" FORCE)

  SET(SDL_FILE               ON  CACHE BOOL "" FORCE) # Needed for Mac OSX
  SET(SDL_LOADSO             ON  CACHE BOOL "" FORCE)

  SET(SDL_JOYSTICK           ON  CACHE BOOL "" FORCE)
  SET(SDL_VIRTUAL_JOYSTICK   ON  CACHE BOOL "" FORCE)

  SET(LIBSAMPLERATE          ON  CACHE BOOL "" FORCE)
  SET(LIBSAMPLERATE_SHARED   ON  CACHE BOOL "" FORCE)

  SET(SDL_EVENTS             ON  CACHE BOOL "" FORCE)
  SET(SDL_THREADS            ON  CACHE BOOL "" FORCE)

  # This assumes you want to have SDL as a static library. If not, set SDL_STATIC
  # to OFF instead.
  SET(SDL_DLOPEN ON  CACHE BOOL "" FORCE)
  SET(SDL_SHARED ON  CACHE BOOL "" FORCE)
  SET(SDL_STATIC OFF CACHE BOOL "" FORCE)

  SET(SDL2_DISABLE_SDL2MAIN  ON CACHE BOOL "" FORCE)
  SET(SDL2_DISABLE_INSTALL   ON CACHE BOOL "" FORCE)
  SET(SDL2_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)

  # Copied from https://github.com/libsdl-org/SDL/commit/7774de0fe364fa997d58f2faa2199054d37bedf6
  #
  # CMAKE_PREFIX_PATH and CMAKE_INSTALL_FULL_BINDIR can be a non-absolute path
  # when a master-project does e.g. `set(CMAKE_INSTALL_PREFIX "libs/SDL2" CACHE PATH "prefix" FORCE)`.
  if(NOT IS_ABSOLUTE "${CMAKE_INSTALL_PREFIX}")
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_PREFIX}")
  endif()
  if(NOT IS_ABSOLUTE "${CMAKE_INSTALL_FULL_BINDIR}")
    set(CMAKE_INSTALL_FULL_BINDIR "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_FULL_BINDIR}")
  endif()

  ADD_SUBDIRECTORY(SDL2 EXCLUDE_FROM_ALL)

  # Work around macos-13 erroring on -Werror,-Wdeclaration-after-statement
  check_c_compiler_flag	(-Wno-error=declaration-after-statement HAVE_GCC_WNO_ERROR_DECLARATION_AFTER_STATEMENT)
  if(HAVE_GCC_WNO_ERROR_DECLARATION_AFTER_STATEMENT)
    target_compile_options(sdl-build-options INTERFACE "-Wno-error=declaration-after-statement")
  endif()

endif() # NOT OSP_USE_SYSTEM_SDL

# Corrade explicitly says IFUNC does not work with sanitizers
IF(OSP_BUILD_SANATIZER)
    SET(CORRADE_CPU_USE_IFUNC  OFF CACHE BOOL "" FORCE)
ELSE()
    SET(CORRADE_CPU_USE_IFUNC  ON CACHE BOOL "" FORCE)
ENDIF()

SET(CORRADE_BUILD_CPU_RUNTIME_DISPATCH OFF CACHE BOOL "" FORCE) # ON causes clang with LTO (enabled for Release) to fail
SET(CORRADE_BUILD_DEPRECATED   ON  CACHE BOOL "" FORCE)
SET(CORRADE_BUILD_STATIC       ON  CACHE BOOL "" FORCE)
SET(CORRADE_BUILD_TESTS        OFF CACHE BOOL "" FORCE)
SET(CORRADE_WITH_INTERCONNECT  OFF CACHE BOOL "" FORCE)
SET(CORRADE_WITH_MAIN          ON  CACHE BOOL "" FORCE)
SET(CORRADE_WITH_PLUGINMANAGER ON  CACHE BOOL "" FORCE)
SET(CORRADE_WITH_RC            OFF CACHE BOOL "" FORCE)
SET(CORRADE_WITH_TESTSUITE     OFF CACHE BOOL "" FORCE)
SET(CORRADE_WITH_UTILITY       OFF CACHE BOOL "" FORCE)
ADD_SUBDIRECTORY(corrade EXCLUDE_FROM_ALL)

SET(MAGNUM_BUILD_DEPRECATED      ON  CACHE BOOL "" FORCE)
SET(MAGNUM_BUILD_PLUGINS_STATIC  ON  CACHE BOOL "" FORCE)
SET(MAGNUM_BUILD_STATIC          ON  CACHE BOOL "" FORCE)
SET(MAGNUM_BUILD_TESTS           OFF CACHE BOOL "" FORCE)
SET(MAGNUM_WITH_ANYIMAGEIMPORTER ON  CACHE BOOL "" FORCE)
SET(MAGNUM_WITH_DEBUGTOOLS       OFF CACHE BOOL "" FORCE)
SET(MAGNUM_WITH_MESHTOOLS        ON  CACHE BOOL "" FORCE)
SET(MAGNUM_WITH_SCENEGRAPH       OFF CACHE BOOL "" FORCE)
SET(MAGNUM_WITH_SCENETOOLS       OFF CACHE BOOL "" FORCE)
SET(MAGNUM_WITH_SDL2APPLICATION  ON  CACHE BOOL "" FORCE)
SET(MAGNUM_WITH_SHADERS          ON  CACHE BOOL "" FORCE)
SET(MAGNUM_WITH_SHADERTOOLS      OFF CACHE BOOL "" FORCE)
SET(MAGNUM_WITH_TESTSUITE        OFF CACHE BOOL "" FORCE)
SET(MAGNUM_WITH_TEXT             OFF CACHE BOOL "" FORCE)
ADD_SUBDIRECTORY(magnum EXCLUDE_FROM_ALL)

SET(MAGNUM_WITH_TINYGLTFIMPORTER ON CACHE BOOL "" FORCE)
SET(MAGNUM_WITH_STBIMAGEIMPORTER ON CACHE BOOL "" FORCE)
ADD_SUBDIRECTORY(magnum-plugins EXCLUDE_FROM_ALL)

SET(NEWTON_BUILD_SANDBOX_DEMOS       OFF CACHE BOOL "" FORCE)
SET(NEWTON_BUILD_SHARED_LIBS         ON  CACHE BOOL "" FORCE)
SET(NEWTON_USE_PLUGIN_DLL            OFF CACHE BOOL "" FORCE)
SET(NEWTON_WITH_AVX_PLUGIN           OFF CACHE BOOL "" FORCE)
SET(NEWTON_WITH_REFERENCE_GPU_PLUGIN OFF CACHE BOOL "" FORCE)
ADD_SUBDIRECTORY(newton-dynamics/newton-3.14/ EXCLUDE_FROM_ALL)

# Newton sets CMAKE_ARCHIVE_OUTPUT_DIRECTORY and CMAKE_LIBRARY_OUTPUT_DIRECTORY
# for UNIX, so manually set each target to our desired location
SET_TARGET_PROPERTIES(dAnimation dContainers dCustomJoints dMath dModel dNewton dScene dVehicle dgCore dgPhysics newton PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
)

ADD_SUBDIRECTORY(entt EXCLUDE_FROM_ALL)

# toml11 3.8.0 added a check for this global variable, which they should not have.
# https://github.com/ToruNiina/toml11/issues/241
SET(CMAKE_CXX_STANDARD 20)
SET(toml11_INSTALL OFF CACHE BOOL "" FORCE)
ADD_SUBDIRECTORY(toml11 EXCLUDE_FROM_ALL)

SET(SPDLOG_ENABLE_PCH ON CACHE BOOL "" FORCE)
# Not yet supported by github runners
# SET(SPDLOG_USE_STD_FORMAT ON CACHE BOOL "" FORCE)
ADD_SUBDIRECTORY(spdlog EXCLUDE_FROM_ALL)
ADD_SUBDIRECTORY(longeronpp EXCLUDE_FROM_ALL)

# For Windows: Prevent overriding the parent project's compiler/linker settings
SET(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
ADD_SUBDIRECTORY(googletest EXCLUDE_FROM_ALL)

SET(FT_DISABLE_ZLIB        ON CACHE BOOL "" FORCE)
SET(FT_DISABLE_BZIP2       ON CACHE BOOL "" FORCE)
SET(FT_DISABLE_PNG         ON CACHE BOOL "" FORCE)
SET(FT_DISABLE_HARFBUZZ    ON CACHE BOOL "" FORCE)
SET(FT_DISABLE_BROTLI      ON CACHE BOOL "" FORCE)
SET(SKIP_INSTALL_ALL       ON CACHE BOOL "" FORCE)
SET(SKIP_INSTALL_LIBRARIES ON CACHE BOOL "" FORCE)
SET(BUILD_SHARED_LIBS      ON CACHE BOOL "" FORCE)
ADD_SUBDIRECTORY(freetype EXCLUDE_FROM_ALL)

INSTALL(TARGETS freetype
        EXPORT RmlUiTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
       )

SET(RML_SKIP_INSTALL ON CACHE BOOL "" FORCE)
SET(CUSTOM_CONFIGURATION ON CACHE BOOL "" FORCE)
#ADD_SUBDIRECTORY(RmlUi EXCLUDE_FROM_ALL)
SET(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
ADD_SUBDIRECTORY(JoltPhysics/Build EXCLUDE_FROM_ALL)
