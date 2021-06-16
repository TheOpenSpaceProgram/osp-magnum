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
 * Generic Rigid Body physics data
 *
 * TODO: possibly split into more components when the time comes
 */
struct ACompRigidBody
{
    // Modify these
    Vector3 m_inertia{1, 1, 1};
    Vector3 m_netForce{0, 0, 0};
    Vector3 m_netTorque{0, 0, 0};

    float m_mass{1.0f};
    Vector3 m_velocity{0, 0, 0};
    Vector3 m_rotVelocity{0, 0, 0};
    Vector3 m_centerOfMassOffset{0, 0, 0};

    bool m_colliderDirty{false}; // set true if collider is modified
    bool m_inertiaDirty{false}; // set true if rigidbody is modified
};

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
    phys::ECollisionShape m_shape{phys::ECollisionShape::NONE};
};

/**
 * For entities that are solid colliders for rigid bodies. Relies on ACompShape
 */
struct ACompSolidCollider { };

}
