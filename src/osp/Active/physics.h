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

#include "../CommonPhysics.h"
#include "activetypes.h"

namespace osp::active
{

/**
 * @brief The ACtxPhysics struct
 */
struct ACtxPhysics
{
    Vector3 m_originTranslate;

    // Input queues
    std::vector<ActiveEnt> m_colliderDirty;
    std::vector<ActiveEnt> m_inertiaDirty;
    std::vector< std::pair<ActiveEnt, Vector3> > m_setVelocity;
};

/**
 * @brief Synchronizes an entity with a physics engine body
 */
struct ACompPhysBody
{ };

/**
 * @brief Adds rigid body dynamics to entities with ACompPhysBody
 */
struct ACompPhysDynamic
{
    Vector3 m_centerOfMassOffset{0, 0, 0};
    Vector3 m_inertia{1, 1, 1};
    float m_totalMass;
};

/**
 * @brief Read-only Linear velocity for entities with ACompPhysDynamic
 */
struct ACompPhysLinearVel : Vector3 { };

/**
 * @brief Read-only Angular velocity for entities with ACompPhysDynamic
 */
struct ACompPhysAngularVel : Vector3 { };

/**
 * @brief Applies force to a dynamic physics entity
 */
struct ACompPhysNetForce : Vector3 { };

/**
 * @brief Applies torque to a dynamic physics entity
 */
struct ACompPhysNetTorque : Vector3 { };

/**
 * @brief Keeps track of which rigid body an entity belongs to
*/
struct ACompRigidbodyAncestor
{
    ActiveEnt m_ancestor{entt::null};
    Matrix4 m_relTransform{};
};

/**
 * Represents the shape of an entity
 */
struct ACompShape
{
    phys::EShape m_shape{phys::EShape::None};
};

/**
 * For entities that are solid colliders for rigid bodies. Relies on ACompShape
 */
struct ACompSolidCollider { };

}
