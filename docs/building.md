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
1. Install cmake and (lib)**sdl2**(-dev) and git
> > `sudo apt-get install cmake libsdl2-dev git`
 
2. Download the repo & cd into it

>Make sure submodules are up to date.

>>```bash
>># clone with --recurse-submodules
>>git clone --recurse-submodules https://github.com/TheOpenSpaceProgram/osp-magnum.git```

>or init them if you forgot to do so

>>```bash
>>cd ./osp-magnum
>>git submodule init
>>git submodule update
>>```

3. Build using CMake, make sure to build into a separate directory or it will not configure.

>FIXME: `cmake .` might have to be run twice to properly configure

>Remove the --parallel flag if you don't want 100% of your CPU to be used.

>>```bash
>>cmake -DCMAKE_BUILD_TYPE=Debug -B build
>>cmake --build build --parallel --config Debug
>>```

This will build the executable in `build/bin/osp-magnum`

### Running

Executable is built into `build/bin/osp-magnum`. The executable's working directory should be the `bin` directory (not `build/bin`), so file paths can correctly find the `OSPData` directory.

```bash
cd bin
../build/bin/osp-magnum
```

If there are problems with DPI scaling, then run with command line arguments `--magnum-dpi-scaling 1.0`
    
  
## Compiling for Windows on Visual Studio
You will need: cmake, Visual Studio  

* Clone the repo with `https://github.com/TheOpenSpaceProgram/osp-magnum`
* Open the repo `cd osp-magnum`
* Get submodules `git clone --recursive --shallow-submodules https://github.com/TheOpenSpaceProgram/osp-magnum`
* Create the build folder `mkdir build && cd build`
* Download [SDL2 visual studio development binaries](https://www.libsdl.org/release/SDL2-devel-2.0.12-VC.zip), and extract it into the build folder
* Run the command `SET SDL2_DIR=SDL2-2.0.12 && cmake -DCMAKE_PREFIX_PATH=%SDL2_DIR% -DSDL2_LIBRARY_RELEASE=%SDL2_DIR%/lib/x64/*.lib -DSDL2_INCLUDE_DIR=%SDL2_DIR%/include ..` to create project files
* Open the solution `OSP-MAGNUM.sln` in file explorer, and visual studio should open up
* Shift-control-B to build for debug.
  
### Running with Visual Studio

Running with VS takes a few steps to get working. VS by default will spread the binaries across several folders, separating Release and Debug builds, and will place the OSP binaries in a different place than the engine binaries. If the application crashes on startup, some .dll files may need to be copied to the OSP binaries (it will specify which one, e.g. newton.dll), and if the application runs but the game data is not being imported (e.g. no models), the VS working directory (`osp-magnum Properties > Configuration Properties > Debugging > Working Directory`) may need to be specified to point to the `/bin` folder so that the game can find the data files.
    

## Using GLFW instead of SDL2

It's possible to use GLFW instead; but as of now, requires modifying files.
This can be automated later.

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
