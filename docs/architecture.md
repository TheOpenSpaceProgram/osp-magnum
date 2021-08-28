# Architecture

The core of OSP-Magnum is best described as a library that provides a set of functions and data structures intended to support a space flight simulator. The user is responsible for implementing their own application on the top level, allowing for full control over every feature.

OSP-Magnum provides two distinct worlds the application can manage:
* **Universe**: A space simulator that can handle massive world sizes with multiple coordinate spaces. Objects in space are referred to as **Satellites**.
* **ActiveScene**: A typical game engine that can support physics, rendering, etc..

The ActiveScene can be synchronized to represent a small area of the Universe, where in-game representations of nearby Satellites can be added to the scene. This area is referred to as the **ActiveArea** and can follow the user around to seamlessly explore the Universe while also avoiding floating point precision errors.

For performance and flexibility for such a project, we adhere to few techniques that may seem unconventional:
* Data-oriented Design
* Entity Component System

## Data-oriented Design (DoD)

Data-oriented Design is an emerging approach to software architecture, used extensively in OSP-Magnum. By focusing on creating a memory layout for efficient access, DoD greatly reduces complexity and achieves high performance. As counterintuitive as it may seem, this potentially results in (or at least encourages) a greater degree of flexibility compared to class-based OOP.

DoD is not a new idea, but it is incredibly unconventional to most programmers (especially ones adept to OOP). It will be useful to read about or listen to a few presentations before getting started.

See [data-oriented-design](https://github.com/dbartolini/data-oriented-design) for plenty of resources.

Important note: When going down the rabbit hole, *Data-oriented programming* and *Data-driven design* is not what you're looking for. *Correct* resources on DoD usually mention the CPU cache on systems-level languages.

*tl;dr: RAM sucks at random access and use arrays everywhere because they're fast!*

### DoD in practice

DoD leads to:

* **Seprated Data and Functions**. This idea is also favored by functional programming. Most classes are only responsible for storing data and their methods only modify their own members. Most operations are free functions that only have a few responsibilities and little side effects.
* **Structure of Arrays**. Aside from efficiency, data can be organized based on how it's operated on. Data for a single 'object' can be represented across multiple arrays, and each of these arrays can be passed to functions that can run simultaneously. This also leads to a natural separation of concerns.
* **Extensive use of contiguous containers**. Dealing with streams of data is the common case. Contiguous containers offer best performance with O(1) access and perfect cache locality. Can't really say `std::vector` is the best, but it is heavily used.
* **No runtime Polymorphism**. We don't need polymorphism. Instead, prefer a more true form of message passing through shared variables. Two completely different systems can communicate using queues that act as interfaces for example.
* **Less Abstractions**. This is not a bad thing, nor does it mean everything is written in assembly. Along with separate data and functions, the computer has separate RAM and CPU (Objects aren't real!). By following a style that more resembles the computer's architecture, less (unnecessary) abstractions are needed to glue together the 'perfect' interface.

Note that this doesn't mean everything has to be ultra-optimized. It's perfectly acceptable to prefer a more convenient option for paths that aren't performance critical, such as operations called only once per frame or once during setup.

## Entity Component System (ECS)

ECS is a solution to representing a world that consists of many 'things' that may have common properties with each other. This is perfect for many games and simulations, and is more versatile and performant than class-based inheritance. Universe and ActiveScene both use ECS through the [EnTT](https://github.com/skypjack/entt/) library.

Confusingly, ECS is not "a system of entities and components" that many mainstream game engines already follow. ECS is better stated as "Entities, Components, and Systems:" three separate concepts that come together to create an elegant architecture.

* **Entity**: A simple unique integer ID linked to the existance of a 'thing'.
* **Component**: An (optional) property of an entity. Usually a struct stored in a contiguous container that associates it to an Entity.
* **System**: A function that operates on entities that have a specific set of components. Best described as 'rules' that determine how some components change over time.

Entities are still just integers, but are treated as **compositions** of different types of components. Components represent what an entity **has or is made of**, which determines what operations can be done to it by systems. The components themselves don't describe any behaviour.

As a simple example: any entity with a *Mass component* is affected by the *Gravity system*, any entity with the *Legs component* affected by the *Walk system*. *Mass* or *Legs* don't directly imply any behaviour; it's the world's job to add meaning to them.

Components **never inherit each other**, and all common properties are split into separate components.

As another example, a Planet can be represented by having a Mass, Radius, Atmosphere, and Solid surface component. A Star can share the same Mass, Radius, and Atmosphere, but with a Luminance and Plasma surface component.

### Encapsulation?

Encapsulation is not needed, since it is 100% certain which components a system will read from and write to. Systems are very much analagous to real-world laws of the physics. The real world won't tell a falling apple to update; it's not the apple's responsibility to move itself. The world doesn't care that the apple is even an apple; gravity will affect any object with mass regardless of what it is.

### Updating the world

Most mainstream game engines update the world per-object like this:

```
void world_update(world) {
    for (each object in world) {
        object.update()
    }
}

```

An ECS engine will more resemble this:

```
void world_update(world) {
    gravity_system(world);
    move_system(world);
}

void gravity_system(world) {
    for (each entity with mass and velocity) {
        // accelerate the masses together
    }
}

void move_system(world) {
    for (each entity with velocity and position) {
        position += velocity
    }
}

...
```

Systems are called in a specific order to update the entire world to the next frame. Some systems may depend on each other, and other systems can run in parallel.

The EnTT library provides many features making iteration convenient.

### Responsibility of Systems

Ideally, components are raw data that don't describe any behaviour. However, it's acceptable for them to have constructors, destructors, member functions, and RAII members as long as components are only modified by very specific systems. This includes creating and destroying components. Many systems reading the same component is okay.

EnTT allows polymorphically accessing components, to allow for deleting, copying, and listing all components an entity has. These features should never be used outside of debugging, since they potentially modify components and calls functions outside of their respective systems.

In short, **components must only be modified by a few select systems**. There's a few other practical reasons why:

* Components may need to be synced with external references: pointers, GPU resourses, or physics engine objects. Some of these require specific threads.
* Multiple Systems that modify different components can run in parallel without mutexes.

There's a few important operations that need to be addressed however:

* **Deleting entire entities**: Deleting an entity means modifying every component container the entity is associated with. Entities can be marked for deletion with a component, or external queue. 'Deleter' Systems can then delete their respective components near the end of the frame.
* **Copying entire entities**: It may seem convenient to iterate all of an entity's components and copy them into another entity, but this breaks the rules. Copies of an entity should be created by the same system as the original entity. A dedicated 'mark for copy' and copy systems may be practical depending on the case.

### A solution to all world state?

Entities and Components are powerful, but cannot solve every single storage use case alone. EnTT specifically, only allows one component of a unique type per entity, and only one container of these components (pool) per world (registry). One case may require a variable number of the same component for an entity, or the problem may not be well suited for entities and components at all.

EnTT features customizability and future versions can be more versitile, but either way, ECS is best when combined with other data structures besides just components. For example, queues can be used to pass messages between systems.

## Universe rundown

Universe is designed to support many different kinds of Satellites: Planets, Vehicles, Stars, etc... This can be achieved efficiently through Entities and Components, where Satellites are Entities. When considering keeping groups of Satellites in an N-body simulation, patched conics simulation, or just landed on a planet, it also becomes apparent that some weird scheme is needed to position Satellites.

See [test_application/universes/simple.cpp](https://github.com/TheOpenSpaceProgram/osp-magnum/blob/master/src/test_application/universes/simple.cpp) for how a simple universe can be setup.

### Components

Since Universe uses ECS, Satellites are compositions of Components. Universe Components are simple structs prefixed with **UComp**.

Not much is written yet. **UCompVehicle** contains data for vehicles, **UCompPlanet** contains data for planets, and these can be assigned to Satellites.

### Coordinates

Dealing with space means ridiculously long distances and big numbers. Storing positions in floating point is not ideal, since they lose precision the further the value is from zero. Integers don't suffer from this issue and also run consistently across different processors.

Therefore, signed 64-bit integers was chosen as space coordinates. They are also paired with a power-of-2 scale factor to correlate them to meters, where 2^x unit = 1 meter. A scale factor of 10 means that 1024 units = 1 meter.

*Note: multiple scale factors are not yet implemented, all coordinates are fixed to 1024 units = 1 meter.*

### Coordinate Spaces

A bunch of satellites in an N-body simulation have nothing to do with a bunch of satellites landed on a planet. The planet also rotates, which moves everything landed on it too. This calls upon the need for CoordinateSpaces.

CooridnateSpaces groups satellites together to be part of the same reference frame, centered around a parent Satellite. A good example is a planet owning a CooridnateSpace for its landed Satellites, which means CoordinateSpaces should be optionally rotatable (not yet implemented). A patched conics implementation may want a CoordinateSpace around every orbit-able Satellite.

Satellites can be handed off between CoordinateSpaces. This won't be discusseed in detail here; but in short, the process relies on a few queues.

On a technical level, CoordinateSpaces store contiguous buffers of Satellites integer IDs, positions, and other additional data like velocity that may be needed to calculate trajectories. CoordinateSpaces are free to store data any way they want. This allows them to be specialized for whichever operations are performed on them (such as N-body). Their buffers are exposed through **read-only** [strided array views](https://doc.magnum.graphics/corrade/classCorrade_1_1Containers_1_1StridedArrayView.html) to be accessed externally. These are referred to as **CComp** for coordinate components. All CoordinateSpaces must output CCompX, CCompY, and CCompZ as a common interface, which are Cartesian 64-bit signed integers. 

X, Y, and Z are separated, because SIMD computations favor each to be on their own buffer.

CoordinateSpaces are stored in the Universe and are addressable with an index.

Two important Universe components are used to track CoordinateSpace data:
* **UCompInCoordspace**: Stores the index to which CoordinateSpace a Satellite is part of
* **UCompCoordspaceIndex**: Stores the Satellite's index inside the CoordnateSpace, used to access CComps.

See [CartesianSimple.h](https://github.com/TheOpenSpaceProgram/osp-magnum/blob/master/src/osp/CoordinateSpaces/CartesianSimple.h) for how CoordinateSpace data can be stored

See [coordinates.h](https://github.com/TheOpenSpaceProgram/osp-magnum/blob/master/src/osp/coordinates.h) for the CoordinateSpaces themselves

### ActiveArea

The ActiveArea is the name given to a small area in the Universe synchronized with the game engine. This is represented as a Satellite with a **UCompActiveArea**. UCompActiveArea stores queues to communicate with the game engine. The game engine can tell the ActiveArea where to move, allowing the user to explore and interact with the universe in-game. This also elegantly solves the floating origin problem.

ActiveArea keeps track of and informs the game engine on Satellites that have entered and exited the area, by determining which ones meet certain conditions to make them visible (such as being nearby). The game engine is responsible for reading the Satellite data, and loading an in-game representation.

For example, if the ActiveArea goes near a planet, it will notify the game engine, which will read the Satellite's data and maybe load terrain and an atmosphere. If a vehicle enters the ActiveArea, then the game engine will read the data and create a physical Vehicle.

ActiveArea also relies on a few of its own CoordinateSpaces to be able to move around freely and also 'capture' other Satellites. This won't be discussed here in detail.

### Universe updates

Universe on its own simply provides data storage. It is the user application's responsibility to pass pieces of Universe into update functions.

Not much is written yet. Coordinate spaces are intended to be passed to update functions to step them through time.

## ActiveScene rundown

ActiveScene stores the state of a 3D game scene, and comes with almost absolutely nothing on its own. All features are 'add-ons' in the form of ECS system functions that need the right components setup in the scene, best explained individually. It does not provide its own window API or anything like that; this is the responsibility of the user's application.

ActiveScene does have a draw function, but it will probably be moved elsewhere in the future.

ECS components intended for the ActiveScene are prefixed with **AComp** for "active component."

See [flight.cpp](https://github.com/TheOpenSpaceProgram/osp-magnum/blob/master/src/test_application/flight.cpp) for how an ActiveScene is setup and updated

### Scene Graph Hierarchy

todo

### Synchronizing with Universe

todo

### Renderer

So far, the test application updates and renders the scene in the same loop. This will likely not be the case in the future.

todo

## Resources and Packages

todo

## Directories

* **`src/osp`**: Core Features with Universe, ActiveScene, and resource management
* **`src/adera`**: Game-specific fun stuff: Rockets, Plumes, Fuel tanks, etc...
* **`src/planet-a`**: Planet UComps, LOD terrain for ActiveScene
* **`src/newtondynamics_physics`**: [Newton Dynamics 3.14](http://newtondynamics.com) physics intergration for ActiveScene
* **`src/test_application`**: Runnable application, see below

### Test Application

The test application is an executable designed to demonstrate many features of osp-magnum. It stores a single universe and its update function. Different universes (created in-code) can be loaded in through a rather stupid command-line shell in its [main.cpp](https://github.com/TheOpenSpaceProgram/osp-magnum/blob/master/src/test_application/main.cpp).

Running the "flight" command runs the `test_flight` function in [flight.cpp](https://github.com/TheOpenSpaceProgram/osp-magnum/blob/master/src/test_application/flight.cpp), which opens a windowed Magnum application that displays an ActiveScene linked to the Universe.

Since the test application is written in simple C++ (less weird and not engine-level), it makes an excellent start to understanding the rest of the codebase.