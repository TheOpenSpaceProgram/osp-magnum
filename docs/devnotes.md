# Random development notes

A list of facts about the codebase in the current state of this commit; not quite practical to put into github issues.

## PSAs

To learn about `src/framework`, read `test/framework/main.cpp`


The pipeline executor in `src/osp/tasks/execute.*` (used by SingleThreadedExecutor in `src/osp/framework/executor.*`) is rather buggy. careful with it!
Likely worth making a new one. Make an executor that runs everything with a simpler and more correct algorithm. This might be slower and involve stuff like 'iterating all pipeline multiple times and seeing which ones can advance' but may be worth it for the correctness or having a ground truth reference for a more optimized one.


What is Sync/Resync???
* Synchronize GPU/rendering resources with whatever is in the scene. This might require GPU stuff for rendering but doesn't do any 'drawing'.
* Read things from the scene, and assign DrawEnts/textures/meshes/shaders to them
* ‘render’ might be skipped (pipeline cancelled), but important sync logic can still run
* Sync usually only iterates things when they change (using dirty flags / list of changes)
* Resync iterates everything. Used when changing scene or renderer.
