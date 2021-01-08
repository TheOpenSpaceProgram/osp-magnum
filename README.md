# osp-magnum

![screenshot](screenshot0.png?raw=true "Mess of vehicles and some tiny planets")

OpenSpaceProgram is an open source initiative about creating a space flight
simulator similar to Kerbal Space Program. This attempt uses Magnum Engine,
and succeeds the urho-osp repository.

This project aims to keep "raw OSP" self-contained with as little dependencies
as possible. Stuff like the code that deals with the Universe and orbits will
be clearly separated from the active game engine physics stuff. Features that
seem important, such as rocket engines and planets, can be removed easily.
In short, nothing will be glued together and the project is modular.

Magnum isn't really much of an engine, and this project is pretty much an
entirely new custom game engine.

Build instructions and more details can be found in the
[Wiki](https://github.com/TheOpenSpaceProgram/osp-magnum/wiki/).

## Nice Engine Features
* Uses modern **C++17**
* Uses **entt** for an Entity Component System (this means it's fast, google it)
* Uses **Newton Dynamics** for physics
* Include dependencies are pretty clean (files only include what they need, and
  there are no circular includes)
* Compiles pretty fast
* Can extend bulleted lists

## Things to implement (roughly in order):
* ~~Hello World, tests, and working build system~~
* ~~Simple console interface for streamlining~~
* ~~Minimal Universe of Satellites~~
* ~~Showing glTF sturdy models~~
* ~~ActiveAreas that load nearby satellites~~
* ~~Intergrate Newton Dynamics~~
* ~~Controllable debug crafts that can fly around~~
* Terrain maybe?
* Scripting or dynamic plugin interface?

## Simplified steps for development
* Step 1: Make a space flight simulator
* Step 2: Bloat it with features

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

## Random Notes
* This project might be codenamed 'adera'. the name of the street the 49 UBC
  bus was at while the project files were first created.
* If there are problems with DPI scaling, then run with command line arguments
  `--magnum-dpi-scaling 1.0`
* Some commit messages are kind of verbose, and can be used to help fill a wiki
* This project isn't intended to be much of "clone" of Kerbal Space Program.
  As of now, this project is mostly a game engine intended for space flight,
  with no gameplay stuff yet.