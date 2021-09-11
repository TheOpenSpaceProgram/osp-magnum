# Architecture

The core of OSP-Magnum is best described as a library that provides a set of functions and data structures intended to support a space flight simulator. The user's top-level application has full control over every feature and their main loop. We provide a Test Application to act as an out-of-the-box game.

OSP-Magnum provides two distinct worlds the application can manage:
* **Universe**: A space simulator that can handle massive world sizes with multiple coordinate spaces. Objects in space are referred to as **Satellites**.
* **ActiveScene**: A typical game engine that can support physics, rendering, etc..

The ActiveScene can be synchronized to represent a small area of the Universe, where in-game representations of nearby Satellites can be added to the scene. This area is referred to as the **ActiveArea** and can follow the user around to seamlessly explore the Universe while also avoiding floating point precision errors.

Multiple ActiveAreas and ActiveScenes are allowed to exist simultaneously. ActiveAreas can also be connected to an entirely different game engine if needed, aside from just ActiveScene.

For performance and flexibility for such a project, we adhere to few techniques that may seem unconventional:
* Data-oriented Design
* Entity Component System

## Data-oriented Design (DoD)

Data-oriented Design is an emerging approach to software architecture, used extensively in OSP-Magnum. By focusing on creating a memory layout for efficient access, DoD greatly reduces complexity and achieves high performance. As counterintuitive as it may seem, this potentially results in (or at least encourages) a greater degree of flexibility compared to class-based OOP.

DoD is not a new idea, but it is incredibly unconventional to most programmers (especially ones adept with OOP). It will be useful to read about or listen to a few presentations before getting started.

See [data-oriented-design](https://github.com/dbartolini/data-oriented-design) for plenty of resources.

Important note: When going down the rabbit hole, *Data-oriented programming* and *Data-driven design* is not what you're looking for. *Correct* resources on DoD usually mention the CPU cache on systems-level languages.

*tl;dr: RAM sucks at random access and use arrays everywhere because they're fast!*

### DoD in practice

DoD leads to:

* **Separated Data and Functions**. This idea is also favored by functional programming. Most classes are only responsible for storing data, and their methods only modify their own members. Most operations are free functions that only have a few responsibilities and few side effects.
* **Structure of Arrays**. To represent a group of common objects, their properties can each be stored in individual arrays. For example, a separate position array, and velocity array for a particle system. A function that only reads position won’t need to jump over extraneous data (e.g. velocity).  This leads to improved memory performance and a natural separation of concerns.
* **Extensive use of contiguous containers**. Dealing with streams of data is the common case. Contiguous containers offer best performance with O(1) access and perfect cache locality. Can't really say `std::vector` is the best, but it is heavily used.
* **No runtime Polymorphism**. We don't need polymorphism. Instead, prefer a more true form of message passing through shared variables. For example, two completely different systems can communicate using queues that act as interfaces.
* **Less Abstractions**. This is not a bad thing, nor does it mean everything is written in assembly. Along with separate data and functions, the computer has separate RAM and CPU (Objects aren't real!). By following a style that more resembles the computer's architecture, less (unnecessary) abstractions are needed to glue together the 'perfect' interface.

Note that this doesn't mean everything has to be ultra-optimized. It's perfectly acceptable to prefer a more convenient option for paths that aren't performance critical, such as operations called only once per frame or once during setup.

## Entity Component System (ECS)

See [ecs-faq](https://github.com/SanderMertens/ecs-faq)

### Motivation

Video games are special in that the systems that make a game world "tick" are incredibly varied. Physics, graphics, sounds, menus, AI, and dialogue are just a few examples of the vast spectrum of logic that governs how the game behaves.
What's more, nearly every entity in the game world expresses a different combination of these behaviors.
The player character has a graphical appearance, makes sound, carries weapons, is affected by physics, and is controlled by the player, while an AI character shares most of these same properties, but with an AI algorithm responsible for its behavior, rather than the player's inputs.

A waypoint marker floating over an objective has a graphical appearance and a position in space, yet is not affected by physics.
A trigger which detects that the player has reached the objective is nearly identical, but is invisible and thus lacks a graphical appearance.

This broad spectrum of combinations of behaviors indicates that game objects are best represented by **composition**.
Rather than attempting to force game objects into a hierarchy of behaviors (e.g. using inheritance to organize "universal" properties that we *assume* will be shared), we would like to make no assumptions, and instead freely attach any behavior to any object.

Furthermore, we would like to **decouple** systems from one another, wherever possible.
The physics logic and rendering logic for an object both may rely on the object's position to do their job, but they need not know any more.
Decoupling unrelated systems from one another keeps them focused, and easier to work with.

### Solution

These problems are addressed by the **Entity Component System** (ECS) paradigm. Here's how it works:

Everything in your game is an **Entity**. The player is an entity, the terrain is an entity, a projectile in flight is an entity, and so on.
Under the hood, an entity is represented by nothing more than a unique integer ID.

We may attach **Components** to entities.
A component represents some behavior or property that the entity possesses, such as a "Position," "Color," "Health," and so on. Instead of inheritance, all common properties between entities are made into their own components.
Under the hood, each component type is a `struct` which stores the state data relevant to its function (e.g. a 3D vector representing position, a mesh resource to be drawn, etc.).

**Systems** are responsible for providing the behaviors promised by the components. Systems only process entities which possess the relevant components.
A physics system would process all entities with a "position" and "velocity" component.
A graphics system might operate on the "position" and a "mesh" component.
Entities with a "Mass" component would be handled by the "Gravity" system.
Any entities which don't possess the right components are simply ignored, and for the entities which *are* processed, any components unrelated to the system are ignored.

This arrangement provides us the decoupling we desired; if we want to add a new behavior to a game object, say, to make it flash colors, we simply create a new function that accesses the "Color" component of any entity which has one, and modifies its color. No other information about the entity is relevant, and the presence of any one component doesn't imply anything about the entity beyond that component's narrow purpose.

## ECS Implementation

In OSP, ECS powers the `Universe` and `ActiveScene` using the [EnTT](https://github.com/skypjack/entt/) library.

EnTT is a *sparse set* ECS, where all instances of each unique component type is kept in a single contiguous container (pools), relying on sparse sets to associate them with entities. Other implementations of ECS exist, such as Archetypes, but those aren't used in OSP.

### Encapsulation?

Encapsulation is not needed, since it is 100% certain which components a system will read from and write to. Systems are very much analogous to the real-world laws of physics. The real world won't tell a falling apple to update; it's not the apple's responsibility to move itself. The world doesn't care that the apple is even an apple; gravity will affect any object with mass regardless of what it is.

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
    gravity_system(world); // Apply forces first
    move_system(world); // Add velocities to positions
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

The EnTT library provides many features making iteration of entities that have a specific set of components convenient.

### Responsibility of Systems

It is the sole responsibility of Systems to mutate component state data.
Ideally, components are raw data (plain-old-data structs) with no behaviors.
However, it's acceptable for components to have constructors, destructors, member functions, and RAII members as long as components are only modified by very specific systems. This includes creating and destroying components.

EnTT allows polymorphically accessing components, to allow for deleting, copying, and listing all components an entity has. These features should never be used outside of debugging, since they potentially modify components or call functions outside of the appropriate system.

Many systems *reading* the same component is okay.
However, **components must only be *modified* by a few select systems**.
There are several practical reasons why:

* Components may need to be synced with external references: pointers, GPU resources, or physics engine objects. Some of these resources must be executed on a specific thread.
* Systems that don't touch the same components can run in parallel without thread safety hazards, which would require costly synchronization and mutual exclusion.

In particular, there are several important operations worth noting:

* **Deleting entire entities**: Deleting an entity means modifying every component container the entity is associated with. Entities can be marked for deletion with a component, or external queue. 'Deleter' Systems can then delete their respective components near the end of the frame.
* **Copying entire entities**: It may seem convenient to iterate all of an entity's components and copy them into another entity, but this breaks the rules. Copies of an entity should be created by the same system as the original entity. A dedicated 'mark for copy' and copy systems may be practical depending on the case.

### A solution to all world state?

Entities and Components are powerful, but cannot solve every single storage use case alone. EnTT, for instance, only allows one component of a unique type per entity, and only one container of these components (pool) per world (registry). One may imagine a situation requiring a variable number of the same component for an entity. The problem may simply not be well suited for entities and components to begin with.

EnTT features customizability and future versions may be more versatile, but either way, ECS is best when combined with other data structures besides just components, such as using queues to pass messages between systems.

## Universe rundown

Universe is designed to support many different kinds of Satellites: Stars, Planets, Moons, Moons of Moons, Asteroids, Stations, Landers, Rovers, and so on. This can be achieved efficiently through Entities and Components, where Satellites are Entities. When considering keeping groups of Satellites in an N-body simulation, patched conics simulation, or just landed on a planet, it also becomes apparent that some weird scheme is needed to position Satellites.

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

CooridnateSpaces groups satellites together to be part of the same reference frame, centered around a parent Satellite. A good example is a planet owning a CoordinateSpace for its landed Satellites, which means CoordinateSpaces should be optionally rotatable (not yet implemented). A patched conics implementation may want a CoordinateSpace around every orbit-able Satellite.

Satellites can be handed off between CoordinateSpaces. This won't be discussed in detail here; but in short, the process relies on a few queues.

On a technical level, CoordinateSpaces store contiguous buffers of Satellites integer IDs, positions, and other additional data like velocity that may be needed to calculate trajectories. CoordinateSpaces are free to store data any way they want. This allows them to be specialized for whichever operations are performed on them (such as N-body). Their buffers are exposed through **read-only** [strided array views](https://doc.magnum.graphics/corrade/classCorrade_1_1Containers_1_1StridedArrayView.html) to be accessed externally. These are referred to as **CComp** for coordinate components. All CoordinateSpaces must output CCompX, CCompY, and CCompZ as a common interface, which are Cartesian 64-bit signed integers. 

X, Y, and Z coordinates are stored in separate arrays, because SIMD computations favor a Struct of Arrays data layout.

CoordinateSpaces are stored in the Universe and are addressable with an index.

Two important Universe components are used to track CoordinateSpace data:
* **UCompInCoordspace**: Stores the index to which CoordinateSpace a Satellite is part of
* **UCompCoordspaceIndex**: Stores the Satellite's index inside the CoordinateSpace, used to access CComps.

See [CartesianSimple.h](https://github.com/TheOpenSpaceProgram/osp-magnum/blob/master/src/osp/CoordinateSpaces/CartesianSimple.h) for how CoordinateSpace data can be stored

See [coordinates.h](https://github.com/TheOpenSpaceProgram/osp-magnum/blob/master/src/osp/coordinates.h) for the CoordinateSpaces themselves

### ActiveArea

The ActiveArea is the name given to a small area in the Universe synchronized with the game engine. This is represented as a Satellite with a **UCompActiveArea**. UCompActiveArea stores queues to communicate with the game engine. The game engine can tell the ActiveArea where to move, allowing the user to explore and interact with the universe in-game. This also elegantly solves the floating origin problem.

ActiveArea keeps track of and informs the game engine about Satellites that have entered and exited the area, by determining which ones meet certain conditions to make them visible (such as being nearby). The game engine is responsible for reading the Satellite data, and loading an in-game representation.

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

TODO

## Resources and Packages

No final code is written yet.

TODO

## Directories

* **`src/osp`**: Core Features with Universe, ActiveScene, and resource management
* **`src/adera`**: Game-specific fun stuff: Rockets, Plumes, Fuel tanks, etc...
* **`src/planet-a`**: Planet UComps, LOD terrain for ActiveScene
* **`src/newtondynamics_physics`**: [Newton Dynamics 3.14](http://newtondynamics.com) physics integration for ActiveScene
* **`src/test_application`**: Runnable application, see below

### Test Application

The test application is an executable designed to demonstrate many features of osp-magnum. It stores a single universe and its update function. Different universes (created in-code) can be loaded in through a rather stupid command-line shell in its [main.cpp](https://github.com/TheOpenSpaceProgram/osp-magnum/blob/master/src/test_application/main.cpp).

Running the "flight" command runs the `test_flight` function in [flight.cpp](https://github.com/TheOpenSpaceProgram/osp-magnum/blob/master/src/test_application/flight.cpp), which opens a windowed Magnum application that displays an ActiveScene linked to the Universe.

Since the test application is written in simple C++ (less weird and not engine-level), it makes an excellent start to understanding the rest of the codebase.
