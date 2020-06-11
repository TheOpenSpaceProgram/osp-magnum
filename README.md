# osp-magnum

Open Space Program implemented 'properly' and uses Magnum Engine, succeeding
the urho-osp repository. It's much more type safe, and the code is nicer.

The key feature here, is that the "raw OSP" is self-contained with as little
dependencies as possible. Planned features like UI and Scripting, will only be
interfaces to the underlying OSP system.

Some details is in the following document (with a few differences), and some
ideas from the urho-osp wiki.

https://github.com/TheOpenSpaceProgram/OpenSpaceProgram/blob/master/docs/Technical.md

## Things to implement (roughly in order):
* ~~Hello World, tests, and working build system~~
* ~~Simple console interface for streamlining~~
* ~~Minimal Universe of Satellites~~
* ~~Showing glTF sturdy models~~
* ~~ActiveAreas that load nearby satellites~~
* ~~Intergrate Newton Dynamics~~
* Controllable debug crafts that can fly around
* Terrain maybe?
* Scripting?

## Building
Build using CMake, prefer not using a GUI for options to be set properly.

FIXME: `cmake .` has to be run twice to properly configure

```bash
cmake .
make
```

### Dependencies

Dependencies are included as submodules in the 3rdparty folder. Magnum is
configures to use SDL2, which should be automatically found by CMake. It can
be changed to another backend like GLFW in src/types.h by changing the
Application include path.

```cpp
// Can be changed to <Magnum/Platform/GlfwApplication.h>
#include <Magnum/Platform/Sdl2Application.h>
```

## Some Technical details

There is three main sides to the program:
* **Universe:**: Manages the large universe and its Satellites
* **Active:** Uses the game engine and Active Areas to interact with universe
* **Application:** Manages resources, and pretty much everything else

### Differences with Technical.md
* Instead of just inheritance, Satellites are non-virtual classes with a
  "SatelliteObject" member for different types, which is virtual. This allows
  Satellites to be contiguous in memory.

### Differences with urho-osp
* Uses more modern C++ (ECS, data-oriented and stuff)
* Less dependent on engine
* Less spaghetti
* Compiles faster

### Execution
So far, a small CLI is implemented in main.cpp for development purposes. This
is not a permanent solution. When executed, it creates a `Universe` and starts
listening for commands through stdin. When it gets the *start* command, it
creates an `OSPMagnum` and a `SatActiveArea` to interact with the `Universe`.

### Class descriptions:

**Core:**

* **OSPMagnum** (Magnum Application)
  * Manages an OpenGL context
  * can use several different OpenGL bindings supported by Magnum.
  * can be bound to a `SatActiveArea` to render and pass inputs to it.

* **Universe**
  * Holds a vector of all the Satellites in existance
  * not much is implemented yet.

**Resource Management:**

* **Package**
  * Has a prefix, name, but mostly not yet implemented
  * Stores vectors of `Resource<T>` for every class it supports
  * Classes stored are addressable via a string
  * Does not do any file operations or loading by itself, only stores
  * see comments Package.h for more details
  * tl;dr: resources are planned to use paths like "pkgA:rocketenginemesh"

* **Resource<T>**
  * Keeps a T as a member.
  * Tracks dependencies as a LinkedList of `ResDependency<T>`s

* **ResDependency<T>**
  * Points to a Resource<T>
  * Is a LinkedList item, and Resource<T> is the LinkedList
  * Can be safely moved around or de-allocated without worry

* **SturdyImporter**
  * Uses a Magnum `TinyGltfImporter` to open GLTF files
  * Outputs `PartPrototype`s to a `Package`

**Resources**

Try to keep struct-like resources easily accesible, serializable, or almost
plain-old-data

* **PartPrototype**
  * Has a vector of `ObjectPrototype`s

* **ObjectPrototype**
  * Describes a game engine object: drawable, collider, etc...

* **PartBlueprint**
  * Describes a specific instance of a `PartPrototype`

* **VehicleBlueprint**
  * An arrangement of `PartBlueprint`s


**Satellites**

* **SatActiveArea**
  * todo

* **SatVehicle**
  * todo

**TODO: new ECS stuff**

### Wiring System

TODO: this may need some discussion

See Technical.md for reasons why this is needed. Here, it's implemented
something like this:

**WireElement**
* Base class for Machines
* Has functions to list available WireInputs and WireOutputs
* Inherited by Machine

**WireOutput**
* Member of a WireElement class
* Has a name for identification
* Stores a supported wire value
* Value can be dependent on multiple WireInputs

**WireInput**
* Member of a WireElement class
* Has a name for identification
* Points to a WireOutput to read it's value

**Dependent/Independent WireOutputs**

Most WireOutputs are expected to be independent of any WireInputs, such as
values from user input or sensor-like data from the physics world.

The only 'Dependent WireOutputs' will be things like logic gates, where a
*change in an input will immediately modify the output*. This means a value
might have to propagate through multiple WireElements to reach its destination.
Most importantly, WireElements have to be updated in a specific order, or
multiple times in a single frame for a preferrable instantaneous response.

To deal with this, WireElements can have a propagate_output(WireOutput*)
function, that can be used to update the value of a specific WireOutput. Note
that WireElements can have multiple Dependent Outputs, so propagations have to
be called per-output. The Wire System keeps a list of Dependent Outputs that
are sorted by closest->furthest from an independent output.

Wire-related step per frame:
1. Machine 'Sensor' Update - Update values from physics world and user input
2. Propagate Update - Propagate independent WireOutputs in order
3. Machine Physics Update - Machines respond to up-to-date wire values
4. Physics update, render, wait for next frame.

```
Example:

Each [In  x  Out] is a WireElement. A, B, and C have dependent outputs, and
just copy their input value into the output for simplicity.

       Independent                                              Destination
            V                                                        V
[UserCtrl  Out] -> [In  A  Out] -> [In  B  Out] -> [In  C  Out] -> [In  Rocket]
                            0               1               2

A's out is closest to UserCtrl's out so is propagated first, B and C come next.
Each propagation, the up-to-date data gets passed along until it gets to C.
On Machine Physics Update, Rocket can now read the up-to-date value from C.

```

TODO: some details on how to actually sort it

This is kind of complicated but in theory should even work for sequential logic
like flip flops.

### Update order

The internal main loop is handled by Magnum, which calls OSPMagnum::drawEvent()

Independent render and physics threads isn't implemented yet.

**Render Frame (OSPMagnum::drawEvent())**
* Call a physics update
* Draw ActiveArea ( m_area->draw_gl() )
  * Update ActiveScene hierarchy `ActiveScene::update_hierarchy_transforms()`
    * Calculate world transformation
    * Interpolate physics (not yet implemented)
  * Draw ActiveScene  `ActiveScene::draw()`
    * System Loop through drawables
* Swap screen buffer

**Update Physics**
*.Update user controls `UserInputHandler::update_controls()`
* Update ActiveArea `ActiveArea::update_physics()`
  * Scan for nearby Satellites
  * Request floating origin translation
  * Call ActiveScene physics update `ActiveScene::update_physics()`
    * SysMachines Sensor Update
    * Wire Propagate Update
    * SysMachines Physics Update
    * Physics Engine Update `SysNewton::update`
      * Updates Newton Dynamics, whatever happens there
* Clear user controls

## Random Notes
* This project might be codenamed 'adera'. the name of the street the 49 UBC
  bus was at while the project files were first created.
