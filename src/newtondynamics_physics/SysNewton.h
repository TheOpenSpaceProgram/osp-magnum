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

#include "osp/CommonPhysics.h"
#include "physics.h"

#include "ActiveScene.h"

#include <cstdint>

class NewtonBody;
class NewtonCollision;
class NewtonWorld;

namespace osp::active
{

class ActiveScene;

/**
 * Component that stores the NewtonWorld, only added to the root node.
 */
struct ACompNwtWorld
{
    NewtonWorld *m_nwtWorld{nullptr};
    float m_timeStep{1.0f / 60.0f};
};

struct ACompNwtBody
{
    constexpr ACompNwtBody(NewtonBody const* body) noexcept
     : m_body(body) { }

    constexpr NewtonBody const* body() const noexcept { return m_body; }

private:
    NewtonBody const *m_body;
};

/**
 * Stores a handle to a NewtonCollision object
 */
struct ACompNwtCollider
{
    constexpr ACompNwtCollider(NewtonCollision const* collision) noexcept
     : m_collision(collision) { }

    constexpr NewtonCollision const* collision() const noexcept
    { return m_collision; }

private:
    NewtonCollision const *m_collision;
};

class SysNewton
{

public:

    static void add_functions(ActiveScene &scene);
    
    static void update_world(ActiveScene& rScene);

    static ACompNwtWorld* try_get_physics_world(ActiveScene &rScene);

    /**
     * Create a Newton TreeCollision from a mesh using those weird triangle
     * mesh iterators.
     * @param shape Shape component to store NewtonCollision* into
     * @param start
     * @param end
     */
    template<class TRIANGLE_IT_T>
    static void shape_create_tri_mesh_static(
            ActiveScene& rScene, ACompShape &rShape, ActiveEnt chunkEnt,
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
    static void find_colliders_recurse(ActiveScene& rScene, ActiveEnt ent,
                                       Matrix4 const &transform,
                                       NewtonWorld const* nwtWorld,
                                       NewtonCollision *rCompound);

    /**
     * Scan children of specified rigid body entity for ACompCollisionShapes,
     * then combine it all into a single compound collision
     *
     * @param rScene   [in] ActiveScene containing entity and physics world
     * @param entity   [in] Entity containing ACompNwtBody
     * @param nwtWorld [in] Newton physics world
     */
    static void create_body(ActiveScene& rScene, ActiveEnt ent,
                            NewtonWorld const* nwtWorld);

    /**
     * Update the inertia properties of a rigid body
     *
     * Given an existing rigid body, computes and updates the mass matrix and
     * center of mass. Entirely self contained, calls the other inertia
     * functions in this class.
     * @param entity [in] The rigid body to update
     */
    static void compute_rigidbody_inertia(ActiveScene& rScene, ActiveEnt entity);


    static void on_body_destruct(ActiveReg_t& reg, ActiveEnt ent);
    static void on_shape_destruct(ActiveReg_t& reg, ActiveEnt ent);
    static void on_world_destruct(ActiveReg_t& reg, ActiveEnt ent);

    static NewtonCollision* newton_create_tree_collision(
            const NewtonWorld *newtonWorld, int shapeId);
    static void newton_tree_collision_add_face(
            const NewtonCollision* treeCollision, int vertexCount,
            const float* vertexPtr, int strideInBytes, int faceAttribute);
    static void newton_tree_collision_begin_build(
            const NewtonCollision* treeCollision);
    static void newton_tree_collision_end_build(
            const NewtonCollision* treeCollision,  int optimize);
};

template<class TRIANGLE_IT_T>
void SysNewton::shape_create_tri_mesh_static(
        ActiveScene& rScene, ACompShape& rShape, ActiveEnt chunkEnt,
        TRIANGLE_IT_T const& start, TRIANGLE_IT_T const& end)
{
    // TODO: this is actually horrendously slow and WILL cause issues later on.
    //       Tree collisions aren't made for real-time loading. Consider
    //       manually hacking up serialized data instead of add face, or use
    //       Newton's dgAABBPolygonSoup stuff directly

    ACompNwtWorld* nwtWorldComp = try_get_physics_world(rScene);
    NewtonCollision* tree = newton_create_tree_collision(nwtWorldComp->m_nwtWorld, 0);

    newton_tree_collision_begin_build(tree);

    Vector3 triangle[3];

    for (TRIANGLE_IT_T it = start; it != end; it += 3)
    {
        triangle[0] = *reinterpret_cast<Vector3 const*>((it + 0).position());
        triangle[1] = *reinterpret_cast<Vector3 const*>((it + 1).position());
        triangle[2] = *reinterpret_cast<Vector3 const*>((it + 2).position());

        newton_tree_collision_add_face(tree, 3,
                                       reinterpret_cast<float*>(triangle),
                                       sizeof(float) * 3, 0);

    }

    newton_tree_collision_end_build(tree, 2);

    rShape.m_shape = phys::ECollisionShape::TERRAIN;
    rScene.reg_emplace<ACompNwtCollider>(chunkEnt, tree);
}

}

