##
# Open Space Program
# Copyright © 2024-2025 Open Space Program Project
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

ADD_LIBRARY( osp-gdextension SHARED)

set( LIB_ARCH ${CMAKE_SYSTEM_PROCESSOR} )
if ( APPLE )
    set( LIB_ARCH "universal" )
endif()

set( LIB_DIR "lib/${CMAKE_SYSTEM_NAME}-${LIB_ARCH}" )
set( BUILD_OUTPUT_DIR "${CMAKE_BINARY_DIR}/$<CONFIG>/" )

set_target_properties( osp-gdextension
    PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN true
        RUNTIME_OUTPUT_DIRECTORY "${BUILD_OUTPUT_DIR}/${LIB_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${BUILD_OUTPUT_DIR}/${LIB_DIR}"
)


# Copy over additional files from the support_files directory
add_custom_command(
    TARGET osp-gdextension POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/support_files"
            ${BUILD_OUTPUT_DIR}
)

file(GLOB_RECURSE CPP_FILES CONFIGURE_DEPENDS *.cpp)
file(GLOB_RECURSE H_FILES CONFIGURE_DEPENDS *.h)
target_sources(osp-gdextension PRIVATE "${CPP_FILES}" "${H_FILES}")

target_include_directories(osp-gdextension PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(osp-gdextension PRIVATE godot-cpp OpenSpaceProgram Adera PlanetA OspJolt OspTestApp)

target_compile_options(osp-gdextension PRIVATE "-static")


# Install library, extension file, and support files in ${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}
set( INSTALL_DIR "${CMAKE_INSTALL_PREFIX}" )

add_subdirectory( templates )

install( TARGETS osp-gdextension
LIBRARY
DESTINATION "${INSTALL_DIR}/osp-gdextension/${LIB_DIR}"
RUNTIME
DESTINATION "${INSTALL_DIR}/osp-gdextension/${LIB_DIR}"
)

# Copy over support files
install( DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/support_files/"
DESTINATION "${INSTALL_DIR}/osp-gdextension/"
PATTERN ".*" EXCLUDE
)

install( DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../bin/OSPData"
DESTINATION ${INSTALL_DIR}
PATTERN ".*" EXCLUDE
)