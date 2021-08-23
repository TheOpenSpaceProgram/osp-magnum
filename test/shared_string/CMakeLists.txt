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
PROJECT(test_shared_string CXX)

ADD_EXECUTABLE(${PROJECT_NAME} EXCLUDE_FROM_ALL)
ADD_DEPENDENCIES(test ${PROJECT_NAME})

FILE(GLOB H_FILES   "*.h")
FILE(GLOB CPP_FILES "*.cpp")
TARGET_SOURCES(${PROJECT_NAME} PRIVATE ${H_FILES} ${CPP_FILES})

TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src/")

TARGET_LINK_LIBRARIES(${PROJECT_NAME} gtest_main)

GTEST_DISCOVER_TESTS(${PROJECT_NAME})

ADD_TEST(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
