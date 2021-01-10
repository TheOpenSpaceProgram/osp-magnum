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

SET(INSTALL_GTEST    OFF CACHE BOOL "" FORCE)
SET(BUILD_TESTING    OFF CACHE BOOL "" FORCE)
SET(BUILD_DEPRECATED OFF CACHE BOOL "" FORCE)
SET(BUILD_GMOCK      OFF CACHE BOOL "" FORCE)
SET(BUILD_STATIC     ON  CACHE BOOL "" FORCE)

ADD_SUBDIRECTORY(corrade EXCLUDE_FROM_ALL)

SET(BUILD_PLUGINS_STATIC  ON CACHE BOOL "" FORCE)
SET(WITH_SDL2APPLICATION  ON CACHE BOOL "" FORCE)
SET(WITH_ANYIMAGEIMPORTER ON CACHE BOOL "" FORCE)
ADD_SUBDIRECTORY(magnum EXCLUDE_FROM_ALL)

SET(WITH_TINYGLTFIMPORTER ON CACHE BOOL "" FORCE)
SET(WITH_STBIMAGEIMPORTER ON CACHE BOOL "" FORCE)
ADD_SUBDIRECTORY(magnum-plugins EXCLUDE_FROM_ALL)

SET(NEWTON_BUILD_SANDBOX_DEMOS OFF CACHE BOOL "" FORCE)
SET(NEWTON_BUILD_SHARED_LIBS   ON  CACHE BOOL "" FORCE)
ADD_SUBDIRECTORY(newton-dynamics)

ADD_SUBDIRECTORY(entt)
