# osp-magnum
![Windows](https://github.com/TheOpenSpaceProgram/osp-magnum/actions/workflows/windows.yml/badge.svg)
![Linux](https://github.com/TheOpenSpaceProgram/osp-magnum/actions/workflows/linux.yml/badge.svg)
![Mac OS X](https://github.com/TheOpenSpaceProgram/osp-magnum/actions/workflows/macos.yml/badge.svg)

![screenshot](screenshot0.png?raw=true "A Debug-rendered vehicle composed of parts flying over a planet.")

***This project is still deep in the pre-release development phase***

OpenSpaceProgram is an open source initiative with the goal of creating a space flight simulator inspired by Kerbal Space Program. This project also works as a general-purpose library for space games and simulations with very large universes and multiple planetary systems.

Written in modern C++17, this project mainly features a custom game engine and a universe/orbit simulator, both relying on [EnTT](https://github.com/skypjack/entt/) and [Magnum](https://github.com/mosra/magnum). The universe and game engine are synchronized for a seamless spaceflight experience from a planet's surface to deep space.

By taking advantage of Entity Component System (ECS) architectures and Data-Oriented Design, this project achieves simplicity, flexibility, low coupling, and excellent performance. With these techniques in action, we can easily avoid spaghetti code and optimize for a high part count.

## Features

### Core

* Universe
  * Support for Universes that use a hierarchical coordinate system, allowing sizes that can be described as:
    * Big. Really big. You just won't believe how vastly, hugely, mindbogglingly big it is. I mean, you may think it's a long way down the road to the chemist's, but that's just peanuts.
  * Arbitrary sized integers for coordinates within a particular layer of the hierarchy
    * We use 64-bit integrer coordinates by default, but we're interested to see what people try out!
  * Optimal data layout to support pluggable oribial mechanics algorithms, which includes
    * Patched conics simulations -- like Kerbal Space Program's gameplay
    * N-Body simulations -- like the KSP mod 'Principia'
    * Build your own simulation logic!
* "ActiveScene" Game Engine
  * Scene Graph
  * Configurable multipass Renderer
  * Straightforward interface for integrating any physics engine
    * Out of the box we integrate with Newton Dynamics 3.14c.
    * PRs to support other physics engines welcome!!!
  * Wiring/Connection System
    * Resource flow between ship components
      * Fuel
      * Cargo
      * Life support
      * Tell us about your ideas!
    * Virtual control systems for vehicles
      * routable user inputs
      * PID
      * auto-landing
      * Write your own!
* Asset management
  * Comes out of the box with glTF as a part model format
  * Plugin-able ship part loader allowing arbitrary format support
* Extendable Bulleted List system to briefly present implemented features

### Extra

* *Newton Dynamics* Physics Engine integration
* Rockets, RCS, and Fuel tanks
* Rocket exhaust plume effects 
* Planet terrain, Icosahedron-based tessellation

### Test Application

To act as a temporary menu and scenario loader, a console-based test application is implemented. It is **written in simple C++**, making it an excellent start to understanding the rest of the codebase.

## Building

These are simplified build instructions for an Ubuntu computer. Windows and Mac OS X build instructions are slightly different.

See our [GitHub Actions](https://github.com/TheOpenSpaceProgram/osp-magnum/tree/master/.github/workflows) for examples on how to get building for your environment

Install some dependencies, configure the build, build the build!

```bash
sudo apt install -y build-essential git cmake libsdl2-dev
git clone --depth=1 --recurse-submodules --shallow-submodules https://github.com/TheOpenSpaceProgram/osp-magnum.git osp-magnum
mkdir build-osp-magnum
cmake -B build-osp-magnum -S osp-magnum -DCMAKE_BUILD_TYPE=Release
cmake --build build-osp-magnum --parallel --config Release --target osp-magnum
```

Run the unit tests!

```bash
cmake --build build-osp-magnum --parallel --config Release --target compile-tests
ctest --schedule-random --progress --output-on-failure --parallel --no-tests error --build-config Release --test-dir build-osp-magnum/test
```

Run the game!

```bash
cd build-osp-magnum/Release
./osp-magnum
```

If you just want to test out the project so far, then see the [Actions](https://github.com/TheOpenSpaceProgram/osp-magnum/actions) tab on GitHub to obtain automated builds for Linux or Windows.

## Controls
These controls will seem familiar if you have played Kerbal Space Program!

#### Navigating
```bash
V - Switch game mode

ArrowUp    - Camera look up
ArrowDown  - Camera look down
ArrowLeft  - Camera look left
ArrowRight - Camera look right

RightMouse - Camera orbit

W - Camera forward
S - Camera backwards
A - Camera left
D - Camera right

Q - Camera up
E - Camera down
```

#### Flight
```bash
S - Vehicle pitch up
W - Vehicle pitch down

A - Vehicle yaw left
D - Vehicle yaw right

Q - Vehicle roll left
E - Vehicle roll right

Z - Vehicle thrust max
X - Vehicle thrust min
LShift - Vehicle thrust increment
LCtrl  - Vehicle thrust decrement

LCtrl+C | LShift+A - Vehicle self destruct
```

#### Misc
```bash
Space - Debug Throw
LCtrl+1 - Debug Planet Update
```

## Contributing

Our development team is very small right now. We need more crew to help to launch this project to its first release.

Join our [Discord Server](https://discord.gg/7xFsKRg) for the latest discussions. You *don't* need to be a professional C++ developer to be involved or help! Graphics, sounds, game design, and scientific accuracy are important to this project too.

Checkout [Architecture.md](docs/architecture.md) to get started with learning the codebase. Feel free to ask questions (even, and especially, the stupid ones); this will greatly help with documentation.

## Simplified development roadmap

* Step 1: Make a space flight simulator
* Step 2: Bloat it with features
* Step 3: ???
* Step 4: Fun!

## Random Notes
* This project might be codenamed 'adera'. the name of the street the 49 UBC bus was at while the project files were first created.
* If there are problems with DPI scaling, then run with command line arguments `--magnum-dpi-scaling 1.0`
* As a general-purpose space flight simulator with no reliance on a heavy game engine, this project might have some real applications for aerospace simulation and visualization, such as:
  * Education
  * Amateur rocketry
  * Small aerospace companies
* This project isn't intended to be much of a "clone" of Kerbal Space Program. As of now, it's mostly a game engine intended for space flight, with no real gameplay yet.
