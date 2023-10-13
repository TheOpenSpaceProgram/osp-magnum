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

#include "../scenarios.h"

namespace testapp::scenes
{

/**
 * @brief Support for Parts, Machines, and Links
 */
osp::Session setup_parts(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         application,
        osp::Session const&         scene);

/**
 * @brief Float Signal Links, allowing Machines to pass floats to each other
 *
 * Setup:
 * * Each machine type provides an update event tag in idMachEvtTags.
 *   eg: tgMhRocketEvt and tgMhRcsDriverEvt
 *
 *
 * Passing values:
 *
 * 1. Tasks write new values to idSigUpdFloat
 *
 * 2. The "Reduce Signal-Float Nodes" task reads new values from idSigUpdFloat(s) and writes them
 *    into idSigValFloat. This changes the input values of connected Machines, marking them dirty.
 *    Tags for each unique dirty machine type is added to rMachUpdEnqueue.
 *    Other 'reduce node' tasks could be running in parallel here.
 *
 * 3. The "Enqueue Machine & Node update tasks" task from setup_parts runs, and enqueues machine
 *    tasks from rMachUpdEnqueue as well as every tgNodeUpdEvt task, including
 *    "Reduce Signal-Float Nodes".
 *
 * 4. Repeat until nothing is dirty
 *
 *
 * This seemingly complex scheme allows different node types to interoperate seamlessly.
 *
 * eg. A float signal can trigger a fuel valve that triggers a pressure sensor which outputs
 *     another float signal, all running within a single frame.
 */
osp::Session setup_signals_float(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         commonScene,
        osp::Session const&         parts);

/**
 * @brief Links for Magic Rockets
 *
 * This only sets up links and does not apply forces, see setup_rocket_thrust_newton
 */
osp::Session setup_mach_rocket(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         commonScene,
        osp::Session const&         parts,
        osp::Session const&         signalsFloat);

/**
 * @brief Links for RCS Drivers, which output thrust levels given pitch/yaw/roll controls
 */
osp::Session setup_mach_rcsdriver(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         commonScene,
        osp::Session const&         parts,
        osp::Session const&         signalsFloat);

/**
 * @brief Logic and queues for spawning vehicles
 *
 * Note that vehicles don't really exist in the scene, and are just collections
 * of conencted Parts
 */
osp::Session setup_vehicle_spawn(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         scene);

osp::Session setup_vehicle_spawn_draw(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         sceneRenderer,
        osp::Session const&         vehicleSpawn);

/**
 * @brief Support VehicleBuilder data to be used to spawn vehicles
 */
osp::Session setup_vehicle_spawn_vb(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         application,
        osp::Session const&         scene,
        osp::Session const&         commonScene,
        osp::Session const&         prefabs,
        osp::Session const&         parts,
        osp::Session const&         vehicleSpawn,
        osp::Session const&         signalsFloat);

/**
 * @brief Controls to select and control a UserControl Machine
 */
osp::Session setup_vehicle_control(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         commonScene,
        osp::Session const&         parts,
        osp::Session const&         signalsFloat,
        osp::Session const&         app);

/**
 * @brief Camera which can free cam or follow a selected vehicle
 */
osp::Session setup_camera_vehicle(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         app,
        osp::Session const&         commonScene,
        osp::Session const&         parts,
        osp::Session const&         physics,
        osp::Session const&         camera,
        osp::Session const&         vehicleControl);


/**
 * @brief Red indicators over Magic Rockets
 */
osp::Session setup_thrust_indicators(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         magnum,
        osp::Session const&         commonScene,
        osp::Session const&         parts,
        osp::Session const&         signalsFloat,
        osp::Session const&         scnRender,
        osp::Session const&         cameraCtrl,
        osp::Session const&         shFlat,
        osp::TopDataId const        idResources,
        osp::PkgId const            pkg);

}
