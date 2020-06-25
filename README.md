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
* **Universe:**: Manages the large universe and its Satellites. This part deals
  with orbits and other interactions between Satellites.
* **Active:** Anything related to the game scene and engine, such as Physics,
  Machines, Rendering, etc...
* **Application:** Manages resources, and pretty much everything else

### Differences with Technical.md
* Instead of just inheritance, Satellites are non-virtual classes with a
  "SatelliteObject" member for different types, which is virtual. This allows
  Satellites to be contiguous in memory.

### Differences with urho-osp
* Uses more modern C++ (C++17, ECS, data-oriented and stuff)
* Less dependent on engine
* Less spaghetti
* Compiles faster

### Execution
So far, a small CLI is implemented in main.cpp for development purposes. This
is not a permanent solution.

When executed, it creates a `Universe`, does some
configuring, and starts listening for commands through stdin.

When the CLI gets the *start* command, it creates an `OSPMagnum`, loads a bunch
of resources if not done so already, then creates a `SatActiveArea` for
`OSPMagnum` to view the universe through.

### Application classes

* **OSPMagnum** (Magnum Application)
  * Manages an OpenGL context
  * can use several different OpenGL bindings supported by Magnum.
  * Listens to user input, and feeds events into a UserInputHandler

* **UserInputHandler**
  * Configurable interface that can be fed button input events from any source
    to map to specific controls.
  * Supports binding controls to multiple buttons and combinations
  * Used by MachineUserControl

### Universe classes

* **Universe**
  * Holds a vector of all the Satellites in existance
  * not much is implemented yet.

* **Satellite**
  * See Technical.md
  * Has a position, can be loaded, etc...
  * Has a unique_ptr to a SatelliteObject to give it functionality

* **SatelliteObject**
  * Base class that adds functionality to a Satellite
  * Identifiable via virtual functions
  * Not movable nor copyable

### SatelliteObject-derived classes

* **SatActiveArea**
  * todo

* **SatVehicle**
  * Depends on a `BlueprintVehicle` from a `Package`

* **SatPlanet**
  * todo

### Active classes

* **ActiveScene**
  * The game engine scene
  * Has a `SysNewton` and `SysWire` as members
  * Stores a map that maps strings to `AbstractSysMachine`s
  * Calls update functions for pretty much everything

* **SysNewton**
  * Handles Physics using Newton Dynamics

* **SysWire**
  * Deals with wiring: communication between Machines. read below
  * Has a vector of `DependentOutput`s

* **Machine**
  * A base class for Machines, see below
  * Uses virtual functions for Wiring only
  * Instances are managed by a corresponding SysMachine

* **AbstractSysMachine**
  * Base class for SysMachine
  * Used to polymorphically access to a SysMachine

* **SysMachine<T>**
  * Stores a vector of Machines specified by template
  * Implements update functions that usually involve iterating the vector
  * Has functions to instantiate or delete Machines

ECS Components:

* **CompCamera**
* **CompTransform**
* **CompHierarchy**
* **CompCollider**
* **CompDrawableDebug**
* **CompMachine**
* **CompNewtonBody**

### Machine classes

Derived SysMachines use CRTP for inheritance. SysMachine uses another template
parameter to declare the Machine.

```cpp

class MachineDerived;

// Declare the system
class SysMachineDerived : public SysMachine<SysMachineDerived, MachineDerived>
{
    // stuff
};

// Declare the machine data
class MachineDerived : public Machine
{
    friend SysMachineDerived;
    // stuff
};

```

Since all instances of the same Machine are contiguous in a vector, one can
easily determine if an abstract Machine is of a specific type by checking if
its memory address is within the vector's data.

**Machines:**

* **MachineUserControl**
* **SysMachineUserControl**

* **MachineRocket**
* **SysMachineRocket**

### Wiring classes

TODO: this may need some discussion. Also, ignore anything related on Dependent
Outputs since it isn't implemented, and isn't useful nor important yet.

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

### Resource classes

There can be multiple ways to load different assets, such as from disk,
network, compressed archive, or from other existing resources. Because of this,
Packages only store loaded data of specified types, how they're loaded is
up to the application.

this system still needs work, and it may be useful to have some sort of
"request load" function for resources.

**Managing Resources:**

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

**Loading and creating Resources:**

* **SturdyImporter**
  * Uses a Magnum `TinyGltfImporter` to open GLTF files
  * Outputs `PrototypePart`s to a `Package`

**Main resources used in packages:**

* **PrototypePart**
  * Has a vector of `PrototypeObjects`s and a vector of `PrototypeMachines`s
  * Usually loaded from a sturdy file

* **BlueprintVehicle**
  * Has a vector of `BlueprintPart`s, a vector of `Blueprint`s
  * This is the thing that players get to build

TODO: magnum stuff

**Structs used by resources:**

* **PrototypeObject**
  * Owned by a PrototypePart
  * Describes a game engine object: drawable, collider, etc...

* **PrototypeMachine**
  * Owned by a PrototypePart
  * Describes a type of machine
  * Contains default configs

* **BlueprintPart**
  * Owned by a BlueprintVehicle
  * Describes a specific instance of a `PrototypePart`
  * Has a transformation

* **BlueprintMachine**
  * Owned by a BlueprintPart
  * Specific configuration of a machine

* **BlueprintWire**
  * Owned by a BlueprintVehicle
  * Describes a wire connection between two `BlueprintPart`s


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
* Update user controls `UserInputHandler::update_controls()`
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
* If there are problems with DPI scaling, then run with command line arguments
  `--magnum-dpi-scaling 1.0`
* Some commit messages are kind of verbose, and can be used to help fill a wiki
