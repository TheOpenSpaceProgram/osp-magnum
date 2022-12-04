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

#include <osp/tasks/tasks.h>
#include <osp/tasks/top_tasks.h>
#include <osp/tasks/top_execute.h>
#include <osp/tasks/top_session.h>
#include <osp/tasks/builder.h>
#include <osp/types.h>

#include <newtondynamics_physics/factors.h>

#include <longeron/id_management/registry_stl.hpp>

namespace testapp::scenes
{

/**
 * @brief Newton Dynamics physics integration
 */
osp::Session setup_newton(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         scnCommon,
        osp::Session const&         physics);

osp::Session setup_newton_force_set(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags);

osp::Session setup_newton_force_accel(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         newton,
        osp::Session const&         nwtForce,
        osp::Vector3                accel);

osp::Session setup_shape_spawn_newton(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         scnCommon,
        osp::Session const&         physics,
        osp::Session const&         shapeSpawn,
        osp::Session const&         newton,
        osp::Session const&         nwtForces);

osp::Session setup_vehicle_spawn_newton(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         scnCommon,
        osp::Session const&         physics,
        osp::Session const&         prefabs,
        osp::Session const&         parts,
        osp::Session const&         vehicleSpawn,
        osp::Session const&         newton,
        osp::TopDataId const        idResources);


} // namespace testapp::scenes
