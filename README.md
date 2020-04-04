# osp-magnum

Open Space Program implemented 'properly' and uses Magnum Engine, succeeding
the urho-osp repository. It's much more type safe, and the code looks nicer.

The key feature here, is that the "raw OSP" is self contained with as little
dependencies as possible. Planned features like UI and Scripting, will only be
interfaces to the underlying OSP system.

Some details is in the following document (with a few differences), and and
some ideas from the urho-osp wiki.

https://github.com/TheOpenSpaceProgram/OpenSpaceProgram/blob/master/docs/Technical.md

## Things to implement (roughly in order):
* ~~Hello World, tests, and working build system~~
* ~~Simple console interface for streamlining~~
* ~~Minimal Universe of Satellites~~
* ~~Showing glTF sturdy models~~
* ActiveAreas that load nearby satellites
* Intergrate Newton Dynamics, and make a SatDebugPlanet as some flat plane to
  test things on
* Crafts that fire thrust
* Terrain maybe?

## Notes
* Instead of just inheritance described in Technical.md, Satellites are just
  normal classes with a virtual "SatelliteObject" for different types
* this project might be codenamed 'adera'. the name of the street the bus was
  at while the project files were first created.
