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

namespace testapp { struct VehicleData; }

namespace testapp::scenes
{

using MachTypeToEvt_t = std::vector<osp::TagId>;

/**
 * @brief Support for Parts and their Machines
 */
osp::Session setup_parts(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         scnCommon,
        osp::TopDataId const        idResources);

/**
 * @brief Float Signal Links, allowing Machines to pass floats to each other
 */
osp::Session setup_signals_float(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         scnCommon,
        osp::Session const&         parts);

/**
 * @brief 'Rockets' that print values to the console
 */
osp::Session setup_mach_rocket(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         scnCommon,
        osp::Session const&         parts,
        osp::Session const&         signalsFloat);

/**
 * @brief Logic and queues for spawning vehicles
 *
 * Note that vehicles don't really exist in the scene, and are just collections
 * of conencted Parts
 */
osp::Session setup_vehicle_spawn(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         scnCommon);

/**
 * @brief Support VehicleBuilder data to be used to spawn vehicles
 */
osp::Session setup_vehicle_spawn_vb(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         scnCommon,
        osp::Session const&         prefabs,
        osp::Session const&         parts,
        osp::Session const&         vehicleSpawn,
        osp::TopDataId const        idResources);

/**
 * @brief Build "Test Vehicle" data, so they can be spawned
 */
osp::Session setup_test_vehicles(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         scnCommon,
        osp::TopDataId const        idResources);

/**
 * @brief Controls to select and control a UserControl Machine
 */
osp::Session setup_vehicle_control(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         scnCommon,
        osp::Session const&         parts,
        osp::Session const&         signalsFloat,
        osp::Session const&         app);

/**
 * @brief Camera which can free cam or follow a selected vehicle
 */
osp::Session setup_camera_vehicle(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         app,
        osp::Session const&         scnCommon,
        osp::Session const&         parts,
        osp::Session const&         physics,
        osp::Session const&         camera,
        osp::Session const&         vehicleControl);


}
