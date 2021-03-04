# Building  
    
### Dependencies

Programs used for Building:

* **cmake** 3.4 or greater
* a C++17 compiler 

This project relies on Magnum Engine. Which needs SDL2.

See Option A: https://doc.magnum.graphics/magnum/getting-started.html#getting-started-setup-subproject

Other dependencies are included as submodules in the 3rdparty folder.


## Building on Linux
To build this project do the following steps:

1\. Install cmake, (lib)**sdl2**(-dev) and git:
`sudo apt-get install cmake libsdl2-dev git`
 
2\. Download the repo and `cd` into it

Make sure submodules are up to date:

```bash
# clone with --recurse-submodules
git clone --recurse-submodules https://github.com/TheOpenSpaceProgram/osp-magnum.git
```

or init them if you forgot to do so:

```bash
cd ./osp-magnum
git submodule init
git submodule update
```

3\. Build using CMake, make sure to build into a separate directory or it will not configure.

FIXME: `cmake .` might have to be run twice to properly configure

Remove the --parallel flag if you don't want 100% of your CPU to be used.

```bash
cmake -DCMAKE_BUILD_TYPE=Debug -B build
cmake --build build --parallel --config Debug
```

This will build the executable in `build/bin/osp-magnum`

### Running

The executable is built into `build/bin/osp-magnum`. The executable's working directory should
be the `/bin` directory (not `build/bin`), so file paths can correctly find the `OSPData`
directory. The program thus should be run with:

```bash
cd bin
../build/bin/osp-magnum
```

If there are problems with DPI scaling, run with command line arguments `--magnum-dpi-scaling 1.0`

## Compiling for Windows on Visual Studio
You will need: cmake, Visual Studio

1\. Clone the repository and submodules, and create a CMake build folder:

```bash
git clone --recursive --shallow-submodules https://github.com/TheOpenSpaceProgram/osp-magnum
cd osp-magnum
mkdir build
```

2\. Choose a graphics library to serve as the platform for Magnum. SDL2 is the
default, but GLFW can be used as well (see [below](#using-glfw-instead-of-sdl2)). To use SDL2, download [SDL2 visual studio development binaries](https://www.libsdl.org/release/SDL2-devel-2.0.12-VC.zip), and extract it into the build folder

3\. In the build folder, use the command `SET SDL2_DIR=SDL2-2.0.12 && cmake -DCMAKE_PREFIX_PATH=%SDL2_DIR% -DSDL2_LIBRARY_RELEASE=%SDL2_DIR%/lib/x64/*.lib -DSDL2_INCLUDE_DIR=%SDL2_DIR%/include ..` to create project files

Alternatively, the CMake GUI can be used to set these parameters and build while
avoiding the windows terminal environment.

4\. Open the solution `OSP-MAGNUM.sln` in file explorer or in CMake GUI, and Visual Studio should open the project

5\. Set `osp-magnum` as the "Startup Project" in the Solution Explorer

6\. Build the project

By default, VS will separate the build binaries into several folders in order to
allow for multiple build configurations to exist concurrently without colliding.
OSP is configured to place its .dll files in these folders; if there is a crash
on program launch, check to ensure that there are Magnum and Newton Dynamics
binaries in the appropriate build folders.

Similarly, the CMake configuration should generate a project with the working
directory set to `/bin`, but if game assets are not being found, check to ensure
that the working directory (`osp-magnum Properties > Configuration Properties >
Debugging > Working Directory`) points to the repository root `/bin` directory.

## Using GLFW instead of SDL2

It's possible to use GLFW instead of SDL2. As of now, this requires modifying
files. This can be automated later.

In src/OSPMagnum.h

```cpp
// change this
#include <Magnum/Platform/Sdl2Application.h>

// to this
#include <Magnum/Platform/GlfwApplication.h>
```

In 3rdparty/CMakeLists.txt

```cmake
# change this
SET(WITH_SDL2APPLICATION ON CACHE BOOL "" FORCE)

# to this
SET(WITH_GLFWAPPLICATION ON CACHE BOOL "" FORCE)
```

In src/CMakeLists.txt

```cmake
# change this
find_package(Magnum REQUIRED
    # ...
    Sdl2Application
    # ...
    )

# to this
find_package(Magnum REQUIRED
    # ...
    GlfwApplication
    # ...
    )
```

In CMake, set the paths to your GLFW binary library and include directory, and then build the application.
