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
#include "osp/CommonPhysics.h"
#include <Magnum/Math/Vector4.h>

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
 * Rigid body with the Newton stuff
 */
struct ACompNwtBody : public phys::DataRigidBody
{
    constexpr ACompNwtBody() = default;
    ACompNwtBody(ACompNwtBody&& move) noexcept;
    ACompNwtBody& operator=(ACompNwtBody&& move) noexcept;

    NewtonBody *m_body{nullptr};
    ActiveEnt m_entity{entt::null};
    //ActiveScene &m_scene;
};

struct ACompRigidbodyAncestor
{
    ActiveEnt m_ancestor{entt::null};
    Matrix4 m_relTransform{};
};

struct ACompCollisionShape
{
    NewtonCollision *m_collision{nullptr};
    phys::ECollisionShape m_shape{phys::ECollisionShape::NONE};
};

using ACompRigidBody_t = ACompNwtBody;

using ACompPhysicsWorld_t = ACompNwtWorld;

class SysNewton : public IDynamicSystem
{

public:

    static inline std::string smc_name = "NewtonPhysics";

    SysNewton(ActiveScene &scene);
    SysNewton(SysNewton const& copy) = delete;
    SysNewton(SysNewton&& move) = delete;

    ~SysNewton();
    
    void update_world(ActiveScene& rScene);

    static ACompNwtWorld* try_get_physics_world(ActiveScene &rScene);

    /**
     * Used to find which rigid body an entity belongs to. This will keep
     * looking up the tree of parents until it finds a rigid body.
     *
     * @param rScene     [in] ActiveScene containing ent
     * @param ent        [in] ActiveEnt with ACompHierarchy and rigidbody ancestor
     *
     * @return Pair of {level-1 entity, pointer to ACompNwtBody found}. If
     *         hierarchy error, then {entt:null, nullptr}. If no ACompNwtBody
     *         component is found, then {level-1 entity, nullptr}
     */
    static std::pair<ActiveEnt, ACompNwtBody*> find_rigidbody_ancestor(
            ActiveScene& rScene, ActiveEnt ent);

    /**
     * Finds the transformation of an entity relative to its rigidbody ancestor
     * 
     * Identical to find_rigidbody_ancestor(), except returns the transformation
     * between rigidbody ancestor and the specified entity.
     * 
     * @param rScene  [in] ActiveScene containing ent
     * @param ent     [in] ActiveEnt with ACompHierarchy and rigidbody ancestor
     * 
     * @return A Matrix4 representing the transformation
     */
    static Matrix4 find_transform_rel_rigidbody_ancestor(
        ActiveScene& rScene, ActiveEnt ent);

    /**
     * Helper function for a SysMachine to access a parent rigidbody
     *
     * Used by machines which influence the rigidbody to which they're attached.
     * This function takes a child entity and attempts to retrieve the rigidbody
     * ancestor of the machine. The function makes use of the
     * ACompRigidbodyAncestor component; if the specified entity lacks this
     * component, one is added to it. The component is then used to store the
     * result of find_rigidbody_ancestor() (the entity which owns the rigidbody)
     * so that it can be easily accessed later.
     * 
     * @param rScene          [in] The scene to search
     * @param childEntity     [in] An entity whose rigidbody ancestor is sought
     * 
     * @return Pair of {pointer to found ACompNwtBody, pointer to ACompTransform
     *         of the ACompNwtBody entity}. If either component can't be found,
     *         returns {nullptr, nullptr}
     */
    static ACompRigidbodyAncestor* try_get_or_find_rigidbody_ancestor(
        ActiveScene& rScene, ActiveEnt childEntity);

    // most of these are self-explanatory
    static void body_apply_force(ACompRigidBody_t& body, Vector3 force) noexcept;
    static void body_apply_accel(ACompRigidBody_t& body, Vector3 accel) noexcept;
    static void body_apply_torque(ACompRigidBody_t& body, Vector3 torque) noexcept;

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

    /**
    * Recursively compute the center of mass of a hierarchy subtree
    * 
    * Takes in a root entity and recurses through its children. Entities which
    * possess an ACompMass component are used to compute a center of mass for
    * the entire subtree, treating it as a system of point masses.
    * 
    * @param rScene           [in] ActiveScene containing relevant scene data
    * @param root             [in] Entity at the root of the hierarchy subtree
    * @param currentTransform [in] Transformation of root with respect to the
    *                              initial call's root
    * 
    * @return A 4-vector containing xyz=CoM, w=total mass
    */
    static Magnum::Vector4 compute_body_CoM(ActiveScene& rScene,
        ActiveEnt root, Matrix4 currentTransform = Matrix4());

    /**
     * Query Newton for the center of mass of the specified rigidbody
     * 
     * @param body [in] ACompNwtBody containing a pointer to a NewtonBody
     * 
     * @return a 3-vector representing the CoM offset from the body origin
     */
    static Vector3 get_rigidbody_CoM(ACompNwtBody const& body);

    /**
     * Compute the volume of a part
     *
     * Traverses the immediate children of the specified entity and sums the
     * volumes of any detected collision volumes. Cannot account for overlapping
     * collider volumes.
     * 
     * @param rScene [in] ActiveScene containing relevant scene data
     * @param part   [in] The part
     * 
     * @return The part's collider volume
     */
    static float compute_part_volume(ActiveScene& rScene, ActiveEnt part);

    /**
     * Compute the moment of inertia of a massive entity
     *
     * Searches the immediate children of the specified entity and computes
     * its moment of inertia. The entity must have child nodes containing
     * ACompCollisionShape components to compute its physical extent.
     * Assumes mass is evenly distributed over collision volume and that the
     * part's center of mass lies at its root origin.
     * 
     * @param rScene [in] ActiveScene containing relevant scene data
     * @param part   [in] The entity with an ACompMass
     * 
     * @return The inertia tensor of the part about its origin
     */
    static Matrix3 compute_mass_inertia(ActiveScene& rScene, ActiveEnt part);

    /**
     * Compute the moment of inertia of a rigid body
     *
     * Searches the child nodes of the root and computes the total moment of
     * inertia of the body. Does not perform recursion, as vehicles do not yet
     * have nested hierarchies.
     * 
     * @param rScene       [in] ActiveScene containing relevant scene data
     * @param root         [in] The root entity of the rigid body
     * @param centerOfMass [in] The center of mass of the rigid body
     * 
     * @return The inertia tensor of the rigid body about its center of mass
     */
    static Matrix3 compute_body_inertia(ActiveScene& rScene, ActiveEnt root,
        Vector3 centerOfMass, Matrix4 currentTransform);

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

    shape.m_shape = phys::ECollisionShape::TERRAIN;
    shape.m_collision = tree;
}

using SysPhysics_t = SysNewton;

}

