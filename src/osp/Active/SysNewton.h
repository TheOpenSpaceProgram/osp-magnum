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

#include <cstdint>

#include "../Resource/PrototypePart.h"

#include "../types.h"
#include "activetypes.h"

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

/**
 * Generic rigid body state
 */
struct DataRigidBody
{
    // Modify these
    Vector3 m_intertia{1, 1, 1};
    Vector3 m_netForce{0, 0, 0};
    Vector3 m_netTorque{0, 0, 0};

    float m_mass{1.0f};
    Vector3 m_velocity{0, 0, 0};
    Vector3 m_rotVelocity{0, 0, 0};

    bool m_colliderDirty{false}; // set true if collider is modified
};

/**
 * Rigid body with the Newton stuff
 */
struct ACompNwtBody : public DataRigidBody
{
    constexpr ACompNwtBody() = default;
    ACompNwtBody(ACompNwtBody&& move) noexcept;
    ACompNwtBody& operator=(ACompNwtBody&& move) noexcept;

    NewtonBody *m_body{nullptr};
    ActiveEnt m_entity{entt::null};
    //ActiveScene &m_scene;
};

struct ACompCollisionShape
{
    NewtonCollision *m_collision{nullptr};
    ECollisionShape m_shape{ECollisionShape::NONE};
};

using ACompRigidBody_t = ACompNwtBody;

using ACompPhysicsWorld_t = ACompNwtWorld;

class SysNewton : public IDynamicSystem
{

public:

    static const std::string smc_name;

    SysNewton(ActiveScene &scene);
    SysNewton(SysNewton const& copy) = delete;
    SysNewton(SysNewton&& move) = delete;

    ~SysNewton();
    
    void update_world(ActiveScene& rScene);

    constexpr ActiveScene& get_scene() noexcept { return m_scene; }

    static ACompNwtWorld* try_get_physics_world(ActiveScene &rScene);

    /**
     * Used to find which rigid body an entity belongs to. This will keep
     * looking up the tree of parents until it finds a rigid body.
     *
     * @param rScene [in] ActiveScene containing ent
     * @param ent    [in] ActiveEnt with ACompHierarchy and rigid body ancestor
     *
     * @return Pair of {level-1 entity, pointer to ACompNwtBody found}. If
     *         hierarchy error, then {entt:null, nullptr}. If no ACompNwtBody
     *         component is found, then {level-1 entity, nullptr}
     */
    static std::pair<ActiveEnt, ACompNwtBody*> find_rigidbody_ancestor(
            ActiveScene& rScene, ActiveEnt ent);

    // most of these are self-explanatory
    static void body_apply_force(ACompRigidBody_t& body, Vector3 force) noexcept;
    static void body_apply_accel(ACompRigidBody_t& body, Vector3 accel) noexcept;
    static void body_apply_torque(ACompRigidBody_t& body, Vector3 force) noexcept;

    /**
     * Create a Newton TreeCollision from a mesh using those weird triangle
     * mesh iterators.
     * @param shape Shape component to store NewtonCollision* into
     * @param start
     * @param end
     */
    template<class TRIANGLE_IT_T>
    void shape_create_tri_mesh_static(ACompCollisionShape& shape,
                                      TRIANGLE_IT_T const& start,
                                      TRIANGLE_IT_T const& end);

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
    static void create_body(ActiveScene& rScene, ActiveEnt entity,
                            NewtonWorld const* nwtWorld);

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

    ActiveScene& m_scene;

    UpdateOrderHandle_t m_updatePhysicsWorld;
};

template<class TRIANGLE_IT_T>
void SysNewton::shape_create_tri_mesh_static(ACompCollisionShape &shape,
                                             TRIANGLE_IT_T const& start,
                                             TRIANGLE_IT_T const& end)
{
    // TODO: this is actually horrendously slow and WILL cause issues later on.
    //       Tree collisions aren't made for real-time loading. Consider
    //       manually hacking up serialized data instead of add face, or use
    //       Newton's dgAABBPolygonSoup stuff directly

    ACompNwtWorld* nwtWorldComp = try_get_physics_world(m_scene);
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

    shape.m_shape = ECollisionShape::TERRAIN;
    shape.m_collision = tree;
}

using SysPhysics_t = SysNewton;

}

