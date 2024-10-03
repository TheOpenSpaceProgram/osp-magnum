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

# TODO put that in the right place. 

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads)
if(NOT Threads_FOUND)
    message(FATAL_ERROR "Can't find a thread library to use!")
endif()

#compile as position-independant to avoid link errors on static link
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(adera)
add_subdirectory(adera_app)
add_subdirectory(osp)
add_subdirectory(planet-a)
add_subdirectory(ospjolt)
add_subdirectory(osp_drawing_gl)
add_subdirectory(adera_drawing_gl)

if(OSP_BUILD_TESTAPP)
    add_subdirectory(testapp)
endif()

if(OSP_BUILD_GDEXTENSION)
    add_subdirectory(gdextension)
endif()

