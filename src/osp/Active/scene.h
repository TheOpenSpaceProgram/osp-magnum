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

#include "../types.h"
#include "activetypes.h"

#include <entt/entity/entity.hpp>

namespace osp::active
{

/**
 * Component for transformation (in meters)
 */
struct ACompTransform
{
    //Matrix4 m_transformPrev;
    osp::Matrix4 m_transform;
    //Matrix4 m_transformWorld;
    //bool m_enableFloatingOrigin;

    // For when transform is controlled by a specific system.
    // Examples of this behaviour:
    // * Entities with ACompRigidBody are controlled by SysPhysics, transform
    //   is updated each frame
    //bool m_controlled{false};

    // if this is true, then transform can be modified, as long as
    // m_transformDirty is set afterwards
    //bool m_mutable{true};
    //bool m_transformDirty{false};
};

struct ACompTransformControlled { };

struct ACompTransformMutable{ bool m_dirty{false}; };

//struct ACompTransformDirty{ };

/**
 * Added to an entity to mark it for deletion
 */
struct ACompDelete{ };

struct ACompName
{
    ACompName(std::string name) : m_name(name) { }
    std::string m_name;
};

struct ACompHierarchy
{
    unsigned m_level{0}; // 0 for root entity, 1 for root's child, etc...
    ActiveEnt m_parent{entt::null};
    ActiveEnt m_siblingNext{entt::null};
    ActiveEnt m_siblingPrev{entt::null};

    // as a parent
    unsigned m_childCount{0};
    ActiveEnt m_childFirst{entt::null};
};

// Stores the mass of entities
struct ACompMass
{
    float m_mass;
};

/**
 * Component that represents a camera
 */
struct ACompCamera
{
    float m_near, m_far;
    Magnum::Deg m_fov;
    Vector2 m_viewport;

    Matrix4 m_projection;
    Matrix4 m_inverse;

    void calculate_projection();
};

}
