##
# Open Space Program
# Copyright © 2019-2021 Open Space Program Project
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
include(ProcessorCount)
ProcessorCount(MACHINE_PARALLEL_LEVEL)
set(CMAKE_CTEST_ARGUMENTS --schedule-random --progress --output-on-failure --parallel=${MACHINE_PARLLEL_LEVEL} --no-tests error)

include(GoogleTest)

# Target to force any dependencies of tests (aka google test) to be compiled
add_library(test-deps INTERFACE)
add_custom_target(compile-test-deps)

set(TEST_DEPS_LIBRARIES gtest_main longeron EnTT::EnTT Magnum::Magnum)
target_link_libraries(test-deps INTERFACE ${TEST_DEPS_LIBRARIES})
add_dependencies(compile-test-deps ${TEST_DEPS_LIBRARIES})

# Target to force the unit tests to be compiled without being executed
add_custom_target(compile-tests)

function(ADD_TEST_DIRECTORY NAME)
    add_executable(${NAME} EXCLUDE_FROM_ALL)
    add_dependencies(compile-tests ${NAME})

	target_compile_features(${NAME} PUBLIC cxx_std_20)

    file(GLOB H_FILES   CONFIGURE_DEPENDS "*.h")
    file(GLOB CPP_FILES CONFIGURE_DEPENDS "*.cpp")
    target_sources(${NAME} PRIVATE ${H_FILES} ${CPP_FILES})

    target_include_directories(${NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src/")

    target_link_libraries(${NAME} PRIVATE test-deps)
    set_target_properties(${NAME} PROPERTIES EXPORT_COMPILE_COMMANDS TRUE)

    add_test(NAME ${NAME} COMMAND ${NAME})

    # Something is wrong with this function. It's executing prior to the program being compiled
    # which causes it to say it can't execute the program. Which, I think we can all agree,
    # is an obvious consequence...
    #gtest_discover_tests(${NAME})
endfunction()

ADD_SUBDIRECTORY(resources)
ADD_SUBDIRECTORY(string_concat)
ADD_SUBDIRECTORY(shared_string)
ADD_SUBDIRECTORY(universe)
ADD_SUBDIRECTORY(tasks)
ADD_SUBDIRECTORY(framework)

