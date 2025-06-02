# Universe

This document describes the workings of OSP 'universe', used to create a coherent and complex universe simulation by interfacing together and splitting data between multiple simpler simulations.

See directory `src/osp/universe`
 

## Introduction


Objects in space (planets, vehicles, asteroids, etc...) are referred to as `Satellite`s.


Making a self-contained n-body or patched conics orbit simulator can be a challenge, but is not impossible. Then consider...

* Satellites move in different ways
  * Landed on a planet (locked in position)
  * In space (n-body orbit, patched conics orbit, fixed path)
  * Controlled by physics engine scene
* Different types of satellites behave differently
  * Can be orbited? significant mass?
  * Has a solid surface to land on?
  * Requires active updating? (e.g. battery drain)
* How to handle satellites landed on planets?
  * Planet rotation must be considered
* How to handle transferring satellites between coordinate spaces / spheres of influence?

The 'software engineering' side of this problem becomes more complicated than orbit math alone. The bad way to go about this would be to make one really big and complex spaghetti universe simulator that just handles every possible case. The OSP way is to make an interface to compose together and unify many simpler simulations.

OSP Universe...

* Manages satellite instance
* Handles multiple simulations
* Allows splitting satellite data across multiple simulations
  * i.e. Position & velocity can be handled by simulation A, while rotation is handled by simulation B
* Provides means of transferring satellites between simulations
* Provides built-in 'components' common across different simulations: (position, velocity, mass, radius)
* Defines a tree of Coordinate Spaces to orient simulations relative to each other

OSP Universe does NOT...

* Provide orbital mechanics or other math (N-body, patched conics)
* Call update functions on simulations

Simulations can be made independent from most of universe, and are free to be structured in many possible ways.

```cpp
struct SimpleNBodySim
{
    struct SatData
    {
        Vector3g        position;
        Vector3d        velocity;
        Vector3d        accel;
        float           mass;
        SatelliteId     id;
    };
    
    std::vector<SatData> m_data;
    
    // ... do whatever you want! 
}
```

To glue a simulation to OSP Universe...

* Assign and keep track of a SatelliteId to each instance/satellite in the simulation
* Create DataAccessor to expose components (position, velocity, etc...)
  * DataAccessors should allow for many different storage layouts.
  * Supports AoS, SoA, multiple buffers, ...



 
## Notes

* Part of designing this system is just 'treat it like it's gpu buffers'
