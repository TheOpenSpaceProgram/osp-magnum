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
    Trade
    )

SET(DEPS_LIBS
            EnTT::EnTT
            Corrade::Main
            Magnum::Magnum
            Magnum::Trade
            longeron
            spdlog
            OpenSpaceProgram
            Jolt)

add_library(OspJolt STATIC)


target_compile_features(OspJolt PUBLIC cxx_std_20)

# Link it to the interface library that describes the dependencies
target_link_libraries(OspJolt PUBLIC ${DEPS_LIBS})

# Gather paths to OSP headers and sources
file(GLOB_RECURSE CPP_FILES CONFIGURE_DEPENDS *.cpp)
file(GLOB_RECURSE H_FILES CONFIGURE_DEPENDS *.h)
target_sources(OspJolt PRIVATE "${CPP_FILES}" "${H_FILES}")

# Enforce conformance mode for OspJolt
target_compile_options(OspJolt PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/permissive->)