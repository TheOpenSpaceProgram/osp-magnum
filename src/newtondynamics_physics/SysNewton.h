/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#pragma once

#include <osp/Active/physics.h>      // for ACompShape
#include <osp/Active/ActiveScene.h>  // for ActiveScene
#include <osp/Active/activetypes.h>  // for ActiveEnt, ActiveReg_t, basic_sp...

#include <osp/types.h>               // for Vector3, Matrix4
#include <osp/CommonPhysics.h>       // for ECollisionShape, ECollisionShape...

// IWYU pragma: no_include <cstdint>
// IWYU pragma: no_include <stdint.h>
// IWYU pragma: no_include <type_traits>

class NewtonBody;
class NewtonWorld;
class NewtonCollision;

namespace ospnewton
{



class SysNewton
{

public:

    static void setup(osp::active::ActiveScene& rScene);

    static void destroy(osp::active::ActiveScene& rScene);
    
    static void update_translate(osp::active::ActiveScene& rScene);

    static void update_world(osp::active::ActiveScene& rScene);

    static NewtonWorld const* debug_get_world();

    /**
     * Create a Newton TreeCollision from a mesh using those weird triangle
     * mesh iterators.
     * @param shape Shape component to store NewtonCollision* into
     * @param start
     * @param end
     */
    template<class TRIANGLE_IT_T>
    static void shape_create_tri_mesh_static(
            osp::active::ActiveScene& rScene, osp::active::ACompShape &rShape,
            osp::active::ActiveEnt chunkEnt,
            TRIANGLE_IT_T const& start, TRIANGLE_IT_T const& end);

private:
    /**
     * Search descendents for collider components and add NewtonCollisions to a
     * vector. Make sure NewtonCompoundCollisionBeginAddRemove(rCompound) is
     * called first.
     *
     * @param rScene    [in] ActiveScene containing entity and physics world
     * @param ent       [in] Entity containing colliders, and recurse into children
     * @param transform [in] Total transform from Hierarchy
     * @param nwtWorld  [in] Newton world from scene
     * @param rCompound [out] Compound collision to add new colliders to
     */
    static void find_colliders_recurse(
            osp::active::ActiveScene& rScene, osp::active::ActiveEnt ent,
            osp::Matrix4 const &transform,
            NewtonWorld const* pNwtWorld, NewtonCollision *rCompound);

    /**
     * @brief Create Newton bodies and colliders for entities with ACompPhysBody
     *
     * @param rScene    [ref] ActiveScene containing entity and physics world
     * @param entity    [in] Entity containing ACompNwtBody
     * @param pNwtWorld [in] Newton physics world
     */
    static void create_body(
            osp::active::ActiveScene& rScene, osp::active::ActiveEnt ent,
            NewtonWorld const* pNwtWorld);

    /**
     * Update the inertia properties of a rigid body
     *
     * Given an existing rigid body, computes and updates the mass matrix and
     * center of mass. Entirely self contained, calls the other inertia
     * functions in this class.
     * @param entity [in] The rigid body to update
     */
    static void compute_rigidbody_inertia(
            osp::active::ActiveScene& rScene, osp::active::ActiveEnt entity);

};

}

