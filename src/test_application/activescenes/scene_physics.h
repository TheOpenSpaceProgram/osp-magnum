/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
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

#include "scenarios.h"

#include <osp/Active/drawing.h>
#include <osp/Active/physics.h>

#include <longeron/id_management/registry_stl.hpp>

#include <string_view>

namespace testapp::scenes
{

struct SpawnShape
{
    osp::Vector3 m_position;
    osp::Vector3 m_velocity;
    osp::Vector3 m_size;
    float m_mass;
    osp::phys::EShape m_shape;
};

using SpawnerVec_t = std::vector<SpawnShape>;

/**
 * @brief Physical properties for entities and generic Physics interface
 *
 * Independent of whichever physics engine is used
 */
osp::Session setup_physics(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         scnCommon);

/**
 * @brief Newton Dynamics physics integration
 */
osp::Session setup_newton_physics(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         scnCommon,
        osp::Session const&         physics);

/**
 * @brief Queues and logic for spawning physics shapes
 */
osp::Session setup_shape_spawn(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         scnCommon,
        osp::Session const&         physics,
        osp::Session const&         material);

/**
 * @brief Queues and logic for spawning Prefab resources
 */
osp::Session setup_prefabs(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         scnCommon,
        osp::Session const&         physics,
        osp::Session const&         material,
        osp::TopDataId              idResources);

/**
 * @brief Entity set to apply 9.81m/s^2 acceleration, added to spawned shapes
 */
osp::Session setup_gravity(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         scnCommon,
        osp::Session const&         physics,
        osp::Session const&         shapeSpawn);

/**
 * @brief Entity set to delete entities under Z = -10, added to spawned shapes
 */
osp::Session setup_bounds(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         scnCommon,
        osp::Session const&         physics,
        osp::Session const&         shapeSpawn);


} // namespace testapp::scenes
