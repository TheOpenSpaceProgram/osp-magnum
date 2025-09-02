# Universe

This document describes the workings of OSP 'universe'.

See directory `src/osp/universe`

## Introduction

Making a self-contained n-body or patched conics orbit simulator can be a challenge, but still fairly straightforward. Then consider...

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


## Basic usage

OSP Universe simply provides means to link together multiple simple 'simulations'. Simulations can be written with regular C++, without much dependencies on OSP:

```cpp
struct SimpleNBodySim
{
    struct Satellite // Let's call objects in space 'satellites'
    {
        std::int64_t    posX;
        std::int64_t    posY;
        std::int64_t    posZ;
        double          velX;
        double          velY;
        double          velZ;
        double          mass;
        std::int32_t    id;
    };
    
    std::vector<Satellite> m_satellites;
    
    // ... do whatever you want! 
}
```

Simulations can be interfaced with OSP universe non-intrusively (without modifying the original simulation code).

* From `UCtxComponentTypes`, use `ComponentTypeId`s to keep track common datatypes used by different simulations.
  * xyz position int64, xyz position double, velocity, radius, ...
* From `UCtxSatellites`, use `SatelliteId`s to identify individual satellites, included as part of simulation data.
  * SatelliteId can track an instance as it's re-arranged within buffers of a simulation, or even be split or transferred across different simulations.
    * Satellite data can be split across different simulations as simulations only need a few components at a time.
      * For example: N-body point masses don't need rotation, best if it's handled by a separate simulator.
  * In the example above, we can use `id` as the SatelliteId; it just needs 32-bits of user data, or just write it intrusively and use SatelliteId directly.
    * If you want to be really non-intrusive, manage SatelliteId arrays as part of the interface code.
* From `UCtxCoordSpaces`, use **Coordinate Spaces** (Coordspace, Cospace, `CoSpaceId`) to create a tree of coordinate spaces (frames of reference) to orient positions and rotations of large groups of satellites relative to each other.
  * Cospaces are parented to other cospaces, but there's an option to parent to a specific Satellite.
    * Has options to enable parent rotation.
      * For example: a planet's 'in-orbit' cospace ignores rotation, but 'surface' coordspace for landed satellites uses rotation.
  * Supports varying powers-of-two precision for int64 positions to support very large universes.
    * For example: 1024 units = 1 meter for positions in a solar system, 1 unit = 16 meters for positions in a galaxy.
* From `UCtxSimulations`, create a `SimulationId` to help keep track of time and who owns what.
  * 'Simulations' as an instance are given a 'timeBehindBy' value. There's technically no 'global time' and update interval can vary across different simulations.
  * Updating the universe can just be looping through every SimulationId and knocking timeBehindBy up by a delta time. Simulation update interval can just poll timeBehindBy, and advance forward if it falls behind too much.
* From `UCtxDataAccessors`, create `DataAccessor`s to expose buffers of the simulation as components accessible to the rest of the universe.
  * Basically just a bunch of pointers, strides, and datatypes (`ComponentTypeId`)
  * If your simulation uses multiple array, just use multiple accessors.
    * If your simulation doesn't use arrays, then wtf are you doing.
  * Must contain a `CoSpaceId` if position or rotation data is specified in the accessor.
* From `UCtxDataSources`, send a `DataSourceChange` to specify which DataAccessors contain the components of each Satellite.
  * Internally Satellites are assigned a `DataSourceId`, created for each unique set of (component types, `DataAccessorId`).
    * ie. Satellites that get positions from accessor A and rotations from accessor B are assigned a common DataSource. Satellites that get positions from accessor A but rotations from accessor C are assigned a common DataSource, but different from the previous one.

    
OSP Universe does NOT...

* Provide orbital mechanics nor other math
* Call update functions on simulations


## Transferring satellites

for transferring
* `UCtxStolenSatellites`
* `UCtxIntakes`
* `UCtxTransferBuffers`


TODO
 
## Notes

* Part of designing this system is just 'treat it like it's gpu buffers'
