# osp-magnum

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

See wiki for more technical details

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

## Random Notes
* This project might be codenamed 'adera'. the name of the street the 49 UBC
  bus was at while the project files were first created.
* If there are problems with DPI scaling, then run with command line arguments
  `--magnum-dpi-scaling 1.0`
* Some commit messages are kind of verbose, and can be used to help fill a wiki
* This project isn't intended to be much of "clone" of Kerbal Space Program.
  As of now, this project is mostly a game engine intended for space flight,
  with no gameplay stuff yet.
