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

#include <osp/tasks/tasks.h>
#include <osp/core/keyed_vector.h>
#include <osp/core/resourcetypes.h>
#include <osp/tasks/top_execute.h>
#include <osp/tasks/top_session.h>
#include <osp/util/logging.h>

#include <entt/core/any.hpp>

#include <osp/core/math_types.h>
#include <osp/tasks/builder.h>
#include <osp/tasks/top_tasks.h>

#include <ospjolt/forcefactors.h>

#include <longeron/id_management/registry_stl.hpp>

namespace testapp::scenes
{

/**
 * @brief Jolt physics integration
 */
osp::Session setup_jolt(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         scene,
        osp::Session const&         commonScene,
        osp::Session const&         physics);

/**
 * @brief Create a single empty force factor bitset
 *
 * This is a simple bitset that can be assigned to a rigid body to set which
 * functions contribute to its force and torque
 */
osp::Session setup_jolt_factors(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData);

/**
 * @brief Setup constant acceleration force, add to a force factor bitset
 */
osp::Session setup_jolt_force_accel(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         jolt,
        osp::Session const&         joltFactors,
        osp::Vector3                accel);

/**
 * @brief Support for Shape Spawner physics using Jolt Physics
 */
osp::Session setup_phys_shapes_jolt(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         commonScene,
        osp::Session const&         physics,
        osp::Session const&         physShapes,
        osp::Session const&         jolt,
        osp::Session const&         joltFactors);

/**
 * @brief Support for Vehicle physics using Jolt Physics
 */
osp::Session setup_vehicle_spawn_jolt(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         application,
        osp::Session const&         commonScene,
        osp::Session const&         physics,
        osp::Session const&         prefabs,
        osp::Session const&         parts,
        osp::Session const&         vehicleSpawn,
        osp::Session const&         jolt);

/**
 * @brief Add thrust forces to Magic Rockets from setup_mach_rocket
 */
osp::Session setup_rocket_thrust_jolt(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         scene,
        osp::Session const&         commonScene,
        osp::Session const&         physics,
        osp::Session const&         prefabs,
        osp::Session const&         parts,
        osp::Session const&         signalsFloat,
        osp::Session const&         jolt,
        osp::Session const&         joltFactors);

} // namespace testapp::scenes
