# osp-magnum

![screenshot](screenshot0.png?raw=true "A Debug-rendered vehicle composed of parts flying over a planet.")

***This project is still deep in the pre-release development phase***

OpenSpaceProgram is an open source initiative with the goal of creating a space flight simulator inspired by Kerbal Space Program. This project also works as a general-purpose library for space games and simulations with very large universes and multiple planetary systems.

Written in C++17, this project mainly features a custom game engine and a universe/orbit simulator, both relying on [EnTT](https://github.com/skypjack/entt/) and [Magnum](https://github.com/mosra/magnum). The universe and game engine are synchronized for a seamless spaceflight experience from a planet's surface to deep space.

By taking advantage of Entity Component System (ECS) architectures and Data-Oriented Design, this project achieves simplicity, flexibility, low coupling, and excellent performance. With these techniques in action, we can easily avoid spaghetti code and optimize for a high part count.

## Features

### Core

* Universe
  * Support for 'infinite' universe sizes using hierarchical Coordinate Spaces
  * Optimal data layout to support fast N-body and patched conics simulations
  * 64-bit integers for coordinates
* "ActiveScene" Game Engine
  * Scene Graph
  * Straightforward interface for integrating any physics engine
  * Configurable multipass Renderer
  * Wiring/Connection System
    * Virtual control systems for vehicles (routable user inputs, PID, auto-landing, ...)
    * Resource flow
* Asset management
  * glTF Part Model format
* Extendable Bulleted List system to briefly present implemented features

### Extra

* *Newton Dynamics* Physics Engine integration
* Rockets, RCS, and Fuel tanks
* Rocket exhaust plume effects 
* Planet terrain Icosahedron-based  tessellation

### Test Application

To act as a temporary menu and scenario loader, a console-based test application is implemented. It is **written in simple C++**, making it an excellent start to understanding the rest of the codebase.

## Building

[Build instructions](docs/building.md)

If you just want to test out the project so far, then see the **Actions** tab to obtain automated builds for Linux or Windows.

## Contributing

Our development team is very small right now. We definitely need more crew to help launch this project to its first release. Join our [Discord Server](https://discord.gg/7xFsKRg) for the latest discussions. You don't need to be a professional C++ developer to be involved. Graphics, sounds, game design, and scientific accuracy are important to this project too.

Check out [Architecture.md](docs/architecture.md) to get started with learning the codebase. Feel free to ask questions (even the stupid ones); this will greatly help with documentation.

## Simplified steps for development

* Step 1: Make a space flight simulator
* Step 2: Bloat it with features

## Random Notes
* This project might be codenamed 'adera'. the name of the street the 49 UBC
  bus was at while the project files were first created.
* If there are problems with DPI scaling, then run with command line arguments
  `--magnum-dpi-scaling 1.0`
* As a general-purpose space flight simulator with no reliance on a heavy game engine, this project might have some real applications for aerospace simulation and visualization. Either for education, amateur rocketry, or even small aerospace companies.
* This project isn't intended to be much of a "clone" of Kerbal Space Program. As of now, it's mostly a game engine intended for space flight, with no real gameplay yet.