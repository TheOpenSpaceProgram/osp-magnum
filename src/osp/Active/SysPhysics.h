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

#include "physics.h"

namespace osp::active
{

class SysPhysics
{
public:
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
    static std::pair<ActiveEnt, ACompRigidBody*> find_rigidbody_ancestor(
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
    static void body_apply_force(ACompRigidBody& body, Vector3 force) noexcept;
    static void body_apply_accel(ACompRigidBody& body, Vector3 accel) noexcept;
    static void body_apply_torque(ACompRigidBody& body, Vector3 torque) noexcept;

    enum EIncludeRootMass { Ignore, Include };
    /**
    * Recursively compute the center of mass of a hierarchy subtree
    *
    * Takes in a root entity and recurses through its children. Entities which
    * possess an ACompMass component are used to compute a center of mass for
    * the entire subtree, treating it as a system of point masses. By default,
    * the root entity's mass is not included; to include it, the optional
    * includeRootMass argument can be set to 'true'.
    *
    * @template CHECK_ROOT_MASS Include or exclude the mass of the root entity
    * being passed to the function
    *
    * @param rScene           [in] ActiveScene containing relevant scene data
    * @param root             [in] Entity at the root of the hierarchy subtree
    * @param includeRootMass  [in] Set to true if the root entity's mass should
    *                              be included in the calculation
    *
    * @return A 4-vector containing xyz=CoM, w=total mass
    */
    template <EIncludeRootMass INCLUDE_ROOT_MASS=EIncludeRootMass::Ignore>
    static Vector4 compute_hier_CoM(ActiveScene& rScene, ActiveEnt root);

    /**
     * Compute the moment of inertia of a rigid body
     *
     * Searches the child nodes of the root and computes the total moment of
     * inertia of the body. To contribute to the inertia of the rigidbody, child
     * entities must posses both an ACompMass and ACompShape component, so that
     * the mass distribution of the entity may be calculated.
     *
     * @param rScene       [in] ActiveScene containing relevant scene data
     * @param root         [in] The root entity of the rigid body
     *
     * @return The inertia tensor of the rigid body about its center of mass, and
     *         a 4-vector containing xyz=CoM, w=total mass
     */
    static std::pair<Matrix3, Magnum::Vector4> compute_hier_inertia(ActiveScene& rScene,
        ActiveEnt entity);


};


}
