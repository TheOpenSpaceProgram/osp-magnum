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
* Intergrate Newton Dynamics, and make a SatDebugPlanet as some flat plane to
  test things on
* Crafts that fire thrust
* Terrain maybe?

## Building
Build using CMake, prefer not using a GUI for options to be set properly.

FIXME: `cmake .` has to be run twice to properly configure

```bash
cmake .
make
```

### Dependencies

Dependencies are included as submodules in the 3rdparty folder. Magnum is
configures to use SDL2, which can be changed in src/types.h by changing the
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
* Uses more modern C++
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

**Active Magnum Features**

* **FtrNewtonBody**
  * todo

* **FtrVehicle**
  * todo

## Random Notes
* This project might be codenamed 'adera'. the name of the street the 49 UBC
  bus was at while the project files were first created.
* Maybe separate SatActiveArea into different Policies
