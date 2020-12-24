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

find_package(Corrade REQUIRED Main)
find_package(Magnum REQUIRED
    GL
    Shaders
    MeshTools
    Primitives
    #SceneGraph
    Trade
    Sdl2Application
    AnyImageImporter
    )

find_package(MagnumPlugins REQUIRED
    TinyGltfImporter
    StbImageImporter)

find_package(Threads)

# Gather paths to OSP headers and sources
file (GLOB_RECURSE CPP_FILES *.cpp)
file (GLOB_RECURSE H_FILES *.h)
set (SOURCE_FILES ${CPP_FILES} ${H_FILES})

add_executable(osp-magnum ${CPP_FILES})

target_include_directories(osp-magnum PRIVATE .)

# Include Newton Dynamics to the project

# TODO: find a better way to do this.
#       Somehow Newton needs some defines for headers to work properly.
#       This section was based on Newton Dynamic's CMakeLists.txt
if(UNIX)
    if (BUILD_64)
        target_compile_definitions(osp-magnum PRIVATE -D_POSIX_VER_64)
    else (BUILD_64)
        target_compile_definitions(osp-magnum PRIVATE -D_POSIX_VER)
    endif (BUILD_64)

elseif(MSVC)
    if(WIN32)
        if(CMAKE_CL_64)
            target_compile_definitions(osp-magnum PRIVATE -D_WIN_64_VER)
        else()
            target_compile_definitions(osp-magnum PRIVATE -D_WIN_32_VER)
        endif()
    else()
        target_compile_definitions(osp-magnum PRIVATE -D_ARM_VER)
    endif()

elseif(MINGW)
    if(CMAKE_CL_64)
        target_compile_definitions(osp-magnum PRIVATE -D_MINGW_64_VER)
        target_compile_definitions(osp-magnum PRIVATE -D_WIN_64_VER)
    else()
        target_compile_definitions(osp-magnum PRIVATE -D_MINGW_32_VER)
        target_compile_definitions(osp-magnum PRIVATE -D_WIN_32_VER)
    endif()

endif()

# Include ENTT (header only lib)
target_include_directories(osp-magnum PRIVATE ../3rdparty/entt/src)

# Put executable in the bin folder
set_target_properties(osp-magnum PROPERTIES RUNTIME_OUTPUT_DIRECTORY ../bin)

target_link_libraries(osp-magnum PRIVATE
    ${CMAKE_THREAD_LIBS_INIT}
    EnTT::EnTT
    Corrade::Main
    Magnum::Application
    Magnum::GL
    Magnum::Magnum
    Magnum::MeshTools
    Magnum::Primitives
    #Magnum::SceneGraph
    Magnum::Shaders
    Magnum::Trade
    Magnum::AnyImageImporter
    MagnumPlugins::TinyGltfImporter
    MagnumPlugins::StbImageImporter
    dNewton dScene dModel dVehicle
    )

find_program(iwyu_path NAMES include-what-you-use iwyu)
if(iwyu_path)
    set_property(TARGET osp-magnum
                 PROPERTY
                 CXX_INCLUDE_WHAT_YOU_USE ${iwyu_path}
                 -Xiwyu
                 --mapping_file=${CMAKE_SOURCE_DIR}/iwyu.imp)
endif()

find_program(tidy_path NAMES clang-tidy)
if(tidy_path)
    set_property(TARGET osp-magnum
                 PROPERTY
                 CXX_CLANG_TIDY ${tidy_path}
                 --checks="clang-diagnostic-*,clang-analyzer-*,bugprone-*,performance-*,readability-*,modernize-*,-modernize-use-trailing-return-type,-modernize-use-auto")
endif()

FILE (COPY "${CMAKE_SOURCE_DIR}/bin/OSPData/adera/" DESTINATION "${CMAKE_BINARY_DIR}/bin/OSPData/adera")
