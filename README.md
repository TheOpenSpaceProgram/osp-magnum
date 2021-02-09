# osp-magnum

![screenshot](screenshot0.png?raw=true "A Debug-rendered vehicle composed of parts flying over a planet.")

***This project is still deep in the development phase***

OpenSpaceProgram is an open source initiative about creating a space flight simulator similar to Kerbal Space Program. Succeeding the urho-osp repository, this attempt features a custom game engine that uses **Modern C++17** with **Magnum Engine** and **EnTT**.

This project aims to keep "raw OSP" self-contained with almost every feature being optional and clearly separated. Using Entity Component System (ECS) architectures and Data-Oriented Design (DoD), this project achieves simplicity, flexibility, low coupling, and excellent performance. DoD is exactly what we need to **avoid spaghetti code** and optimize for a **high part count**.

[More detailed introduction](docs/architecture.md)

[Build instructions](docs/building.md)

If you just want to test out the project so far, then see the **Actions** tab to obtain automated builds for Linux or Windows.
  
## Features  
  
### Core

* ECS Deep Space / Orbit simulator
* Modular ECS Game Engine
* Newton Dynamics Physics Engine integration 
* Simple Asset management
* Interactive Vehicles
* Load Parts from glTF files
* Virtual Wiring System for controlling vehicles (For routable user controls, PID, auto-landing, ...)
* Can extend bulleted lists
  
### Extra
      
* Rockets and RCS
* Rocket exhaust plume effects 
* Ship Resource (Fuel) system
* Icosahedron-based Planet surfaces with Level-of-detail subdivision
    
### Test Application
    
To act as a temporary menu and scenario loader, a console-based test application is implemented. It is **written in simple C++**, making it an excellent start to understanding the rest of the codebase.
  
## Simplified steps for development  

* Step 1: Make a space flight simulator
* Step 2: Bloat it with features

## Random Notes  

* This project isn't intended to be much of a "clone" of Kerbal Space Program. As of now, it's mostly a game engine intended for space flight, with no real gameplay yet.
* As a general-purpose space flight simulator with no reliance on a heavy game engine, this project might have some real applications for aerospace simulation and visualization. Either for education, amature rocketry, or even small aerospace companies.
