##
# Open Space Program
# Copyright © 2019-2024 Open Space Program Project
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

find_package(Magnum REQUIRED
    GL
    Shaders
    MeshTools
    Primitives
    Trade
    Sdl2Application
    AnyImageImporter
    )

find_package(MagnumPlugins REQUIRED
    TinyGltfImporter
    StbImageImporter)

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads)
if(NOT Threads_FOUND)
    message(FATAL_ERROR "Can't find a thread library to use!")
endif()


add_library(osp-magnum-deps INTERFACE)
add_custom_target(compile-osp-magnum-deps)

# TODO: This list shouldn't be required, but there appears to be a bug in cmake where
# the add_custom_target() command, pointing to an INTERFACE dependency, doesn't
# result in the dependencies of the interface building.
# See: https://gitlab.kitware.com/cmake/cmake/-/issues/23569
SET(OSP_MAGNUM_DEPS_LIBS
            Threads::Threads
            EnTT::EnTT
            Corrade::Main
            Magnum::Application
            Magnum::GL
            Magnum::Magnum
            Magnum::MeshTools
            Magnum::Primitives
            Magnum::Shaders
            Magnum::Trade
            Magnum::AnyImageImporter
            MagnumPlugins::TinyGltfImporter
            MagnumPlugins::StbImageImporter
            toml11
            spdlog
            longeron
            OpenSpaceProgram
            Adera
            AderaGL
            OspGL
            OspJolt
            PlanetA
            OspTestApp)
target_link_libraries(osp-magnum-deps INTERFACE ${OSP_MAGNUM_DEPS_LIBS})
add_dependencies(compile-osp-magnum-deps ${OSP_MAGNUM_DEPS_LIBS})

add_executable(osp-magnum)

target_compile_features(osp-magnum PUBLIC cxx_std_20)

target_include_directories(osp-magnum PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Link it to the interface library that describes the dependencies
target_link_libraries(osp-magnum PRIVATE osp-magnum-deps)

# Gather paths to OSP headers and sources
file(GLOB_RECURSE CPP_FILES CONFIGURE_DEPENDS *.cpp)
file(GLOB_RECURSE H_FILES CONFIGURE_DEPENDS *.h)
target_sources(osp-magnum PRIVATE "${CPP_FILES}" "${H_FILES}")

# Enforce conformance mode for osp-magnum
target_compile_options(osp-magnum PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/permissive->)

set_target_properties(osp-magnum PROPERTIES
    EXPORT_COMPILE_COMMANDS TRUE
    INSTALL_RPATH "$ORIGIN/lib"
    C_EXTENSIONS OFF
    C_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
)

add_custom_target(run-osp-magnum
		  COMMAND $<TARGET_FILE:osp-magnum>
		  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>)
