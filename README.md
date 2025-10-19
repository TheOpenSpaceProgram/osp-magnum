# osp-magnum
![Windows](https://github.com/TheOpenSpaceProgram/osp-magnum/actions/workflows/windows.yml/badge.svg)
![Linux](https://github.com/TheOpenSpaceProgram/osp-magnum/actions/workflows/linux.yml/badge.svg)
![Mac OS X](https://github.com/TheOpenSpaceProgram/osp-magnum/actions/workflows/macos.yml/badge.svg)

![screenshot](screenshot0.png?raw=true "A Debug-rendered vehicle composed of parts flying over a planet.")

OpenSpaceProgram is an open source initiative with the goal of creating a realistic spacecraft building game inspired by Kerbal Space Program.

[[Website](https://openspaceprogram.org/)]  [[Discord Server](https://discord.gg/7xFsKRg)] [[IRC](ircs://irc.libera.chat:6697/#openspaceprogram)]

Putting gameplay details aside (art, story, progression, etc...), a game of this nature requires a massive and complicated universe simulation with intricate vehicles, rigid-body physics, orbits, and literally rocket science. To avoid inevitably creating an unmaintainable buggy mess, we need a strong foundation built with considerable amounts of engineering and passion.

***Introducing OSP-Magnum***

Goals:

* Fast, multi-threaded, and runs on everything
* Powerful API with mods / plugins
* Components reusable in other similar projects

After heavy iteration, it became apparent that it's impractical to build over an off-the-shelf game engine. Major components of OSP-Magnum (vehicles, planet terrain, code utilities, etc...) are designed as separate libraries that are usable on their own, independent of any game engine.

## Features

* C++20
* Data-oriented design
  * This basically means common data for multiple 'things' are often represented as big arrays instead of individual classes/objects
  * Best utilizes the CPU you paid for, instead of being wasted by memory bottlenecks
  * Simpler code, easier to multithread, and no overcomplicated inheritance hierarchy spaghetti
  * Might be Entity-component-system???
* Scene Graph
* Jolt Physics integration
* Vehicles
  * Part model format uses Standard glTF
  * 'Link System' - Parts communicate like components in a logic circuit simulator. (PID control anything, differential thrust, auto-landing... Too many possibilities!)
* Planet terrain
  * Icosahedron-based tessellation. Manifold (Hole-free) mesh; allows vertex shaders to cleanly displace vertices.
* Universe
  * Stitches together many simpler simulations into a common interface
    * Currently supports fixed circular paths and simple n-body. More sophisticated orbit simulators can be added easily.
    * Handles transferring objects and splitting data between simulations
  * Nested coordinate spaces with 64-bit int/fixed-point coordinates with variable precision
    * Use '65536 units = 1 meter' for a planet, and '1 unit = 1 meter' for a galaxy, for example
* Powerful Framework
  * Designed to elegantly handle tough cases like floating origin translations; generally making sure all code runs in the correct order
  * Immune to 'NullReferenceException-like' logic bugs common in conventional game engines. Frequent null-checking not required
  * Extremely composable. Easy to swap out major components, such as the renderer or physics engine
* Runnable Test application demo
  * OpenGL renderer using Magnum
  
Notes:

* No multi-threading is implemented yet
  * Code is structured in a way that is easy to *efficiently* multithread in the future 
  * Don't underestimate how fast a single thread can be. This won't be a problem for a while

## Roadmap

1. Lunar Lander Minimum-Viable-Product demo. Orbits, terrain, solar system, and fairly complex prebuilt lander / service module.
2. Sandbox demo. Edit vehicles directly, with ways to save/load vehicles; no separate 'vehicle assembly' UI yet. Can walk characters around on EVA.
3. ???

Making this into an actual 'game' will practically be a separate project.

### Planned major features and changes:

* See https://github.com/TheOpenSpaceProgram/osp-magnum/issues?q=is%3Aissue+is%3Aopen+label%3Aplanned

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

Run the testapp!

```bash
cd build-osp-magnum/Release
./osp-magnum
```

This will bring up the "OSP-Magnum Temporary Debug CLI." From here you can enter commands to run a scenario, like `vehicles`.

If you just want to try the demo without building, then see the [Actions](https://github.com/TheOpenSpaceProgram/osp-magnum/actions) tab on GitHub to obtain automated builds for Linux or Windows.

## Contributing

Our development team is very small right now. We need more crew to help to launch this project to its first release.

This project has many different aspects. It's best if you're interested in working on a specific feature. Aerodynamics? An orbit simulator? UI? Graphics? Sounds? Rocket engines? Wheels? Trains??? Maybe just weird C++?

We are at a point where many different features can be worked on simultaneously; but of course many issues block others. The sad reality is: we need to write code to do fun things (but make it good!!!).

You *don't* need to be a professional C++ developer to be involved or help! Art style, music, game design, scientific accuracy, and having lots of space nerds around are important to this project too!

Feel free to ask questions (even, and especially, the stupid ones) on Github, Discord, or IRC.

### Resources

* [Architecture.md](docs/architecture.md)
* [Framework Unit test / Tutorial](test/framework/main.cpp)
* [1 hour 30 minute long technical introduction video](https://www.youtube.com/watch?v=vdUllp9-E6k)
* [Informal project history from BDFL's perspective](https://gist.github.com/Capital-Asterisk/a22c81ffff1bf20d5023bdd40909d31d)

## Random Notes
* This project might be codenamed 'adera'. the name of the street the 49 UBC bus was at while the project files were first created.
* If there are problems with DPI scaling, then run with command line arguments `--magnum-dpi-scaling 1.0`
* As a general-purpose space flight simulator with no reliance on a heavy game engine, this project might have some real applications for aerospace simulation and visualization, such as:
  * Education
  * Amateur rocketry
  * Small aerospace companies
* This project isn't intended to be much of a "clone" of Kerbal Space Program. As of now, it's mostly a game engine intended for space flight, with no real gameplay yet.
