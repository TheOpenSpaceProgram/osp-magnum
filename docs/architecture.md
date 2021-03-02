# Introduction to osp-magnum

Let's build a spaceflight simulator! Now where do we start???  
  
## Data-Oriented Design / ECS  

Instead of structuring a program around objects communicating, Data-Oriented Design (DoD) is about structuring a program to efficiently represent data, and how that data is transformed from input to output. This leads to simpler code and an optimized memory layout to reduce cache misses. DoD is not a new idea, but is very unconventional to most. It will be useful to read about or listen to a few presentations before getting started.

See [data-oriented-design](https://github.com/dbartolini/data-oriented-design) for plenty of resources.

See [ecs.md](ecs.md) for a more osp-magnum specific description.

## How to represent a universe
  
The universe is really big. As of February 2021, Voyager 1 is about 22.7 billion km away from the sun. No game engine is suited for simulating distances this vast, noting issues like floating point precision. This means the large universe must be handled as a **separate orbital mechanics simulator** outside of the game engine. The game scene the user interacts with will only be a representation of a small area in this universe, simulating only nearby objects.

With this idea in mind, osp-magnum implements two distinct simulated worlds:

### Universe: space and orbits

The Universe, a representation of deep space centered around the `osp::universe::Universe` class. Using EnTT ECS, objects in space are known as ``Satellite`s, which are **compositions** of UComp structs (Universe Component).

In a theoretical more complete osp-magnum, a planet can be a composition of `UCompPosition`, `UCompRotation`, `UCompPlanetSurface`, and `UCompAtmosphere`.  
  
  A star can be a composition of `UCompPosition`, `UCompRotation`, `UCompAtmosphere`, and `UCompLightSource`.

Signed 64-bit integers are used to represent positions, where 1024 units is equal to 1 meter.  This gives objects in the universe a max distance of ( 2^64 )/1024 meters, 18,014 billion kilometers, or 1.9 light years. This is more than enough for a solar system, but there are plans to adjust this precision later for galactic scales.

So far, the Universe **only stores states**, and orbits (Trajectories) are not yet implemented.

See `src/test_application/universes/simple.cpp` for an example of how to create a simple universe.

### ActiveScene: the a game engine

The ActiveScene, a general-purpose ECS game engine centered around the `osp::active::ActiveScene` class. It handles things like rigid body physics, vehicles, terrain, and can be rendered. This is what the player interacts with.  
    
ActiveScene also uses EnTT to store a world of `ActiveEnt`s (Active Entity), which are **compositions** of AComp structs (Active Component). These structs only store state, and don't describe any behaviours.

An `ActiveEnt` with a `ACompCamera` can be used as a camera, `ACompVehicle` stores info on a vehicle, `ACompFloatingOrigi	n` marks it as floating origin translatable, etc...

AComps **only store state,** no behaviours. A newly initialized ActiveScene only features a scene graph hierarchy, and nothing else. Kind of like Arch Linux, all other features must be added to it manually (as ECS system functions) to add behaviours to the components.
  
See `src/test_application/flight.cpp`for an example of how an ActiveScene can be configured as a flight scene connected to the universe

## Directories

### src/test_application

To act as a temporary menu and scenario loader, a console-based test application is implemented. It is **written in simple C++**, making it an excellent start to understanding the rest of the codebase.

### src/osp

Core features, such as the Universe, ActiveScene, Vehicles, Asset management, Newton Dynamics Physics engine integration, and the architecture to interface everything together.

### src/adera

Contains features that are game-specific, and shouldn't be part of the core, such as Rockets, RCS controllers and exhaust plumes. It is named after the street the bus was on while this project was first created.

### src/planet-a

Implements planetary surfaces by subdividing an icosahedron.  
 
