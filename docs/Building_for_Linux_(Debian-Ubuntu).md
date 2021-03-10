To build this project do the following steps:
1. Install cmake and (lib)**sdl2**(-dev) and git
> > `sudo apt-get install cmake libsdl2-dev git`

2. Download the repo & cd into it
> > `git clone --recurse-submodules https://github.com/TheOpenSpaceProgram/osp-magnum.git`

> > `cd osp-magnum`
3. Run Cmake script & compile:
> > `cmake -DCMAKE_BUILD_TYPE=Debug -B build`

> > `cmake --build build --parallel --config Debug`
4. Run OSP:
> > `cd build/bin`

> > `./osp-magnum`
