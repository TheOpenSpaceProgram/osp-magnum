/**
 * Open Space Program
 * Copyright © 2019-2021 Open Space Program Project
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

#include <string>

namespace osp::active
{

/**
 * @brief Component for transformation (in meters)
 */
struct ACompTransform
{
    osp::Matrix4 m_transform;
};

// TODO: this scheme of controlled and mutable likely isn't the best, maybe
//       consider other options

/**
 * @brief Indicates that an entity's ACompTransform is owned by some specific
 *        system, and shouldn't be modified freely.
 *
 * This can be used by a physics or animation system, which may set the
 * transform each frame.
 */
struct ACompTransformControlled { };

/**
 * @brief Allows mutation for entities with ACompTransformControlled, as long as
 *        a dirty flag is set.
 */
struct ACompTransformMutable{ bool m_dirty{false}; };

/**
 * @brief The ACompFloatingOrigin struct
 */
struct ACompFloatingOrigin { };

/**
 * @brief Simple name component
 */
struct ACompName
{
    std::string m_name;
};

/**
 * @brief Places an entity in a hierarchy
 *
 * Stores entity IDs for parent, both siblings, and first child if present
 */
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

/**
 * @brief Component that represents a camera
 */
struct ACompCamera
{
    float m_near;
    float m_far;
    Deg m_fov;
    Vector2 m_viewport;

    Matrix4 m_projection;
    Matrix4 m_inverse;

    void calculate_projection() noexcept
    {
        m_projection = osp::Matrix4::perspectiveProjection(
                m_fov, m_viewport.x() / m_viewport.y(),
                m_near, m_far);
    }
};

/**
 * @brief Storage for basic components
 */
struct ACtxBasic
{
    acomp_storage_t<ACompTransform>             m_transform;
    acomp_storage_t<ACompTransformControlled>   m_transformControlled;
    acomp_storage_t<ACompTransformMutable>      m_transformMutable;
    acomp_storage_t<ACompFloatingOrigin>        m_floatingOrigin;
    acomp_storage_t<ACompName>                  m_name;
    acomp_storage_t<ACompHierarchy>             m_hierarchy;
    acomp_storage_t<ACompCamera>                m_camera;
};

template<typename IT_T>
void update_delete_basic(ACtxBasic &rCtxBasic, IT_T first, IT_T last)
{
    rCtxBasic.m_floatingOrigin  .remove(first, last);
    rCtxBasic.m_name            .remove(first, last);
    rCtxBasic.m_hierarchy       .remove(first, last);
    rCtxBasic.m_camera          .remove(first, last);

    while (first != last)
    {
        ActiveEnt const ent = *first;

        if (rCtxBasic.m_transform.contains(ent))
        {
            rCtxBasic.m_transform           .remove(ent);
            rCtxBasic.m_transformControlled .remove(ent);
            rCtxBasic.m_transformMutable    .remove(ent);

        }

        ++first;
    }
}

} // namespace osp::active
