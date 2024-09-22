/**
 * Open Space Program
 * Copyright © 2019-2024 Open Space Program Project
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

#include <osp/framework/builder.h>

namespace adera
{

/**
 * @brief Support for Parts, Machines, and Links
 */
extern osp::fw::FeatureDef const ftrParts;


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
 * 1. Tasks write new values to sigFloat.di.sigUpdFloat
 *
 * 2. The "Reduce Signal-Float Nodes" task reads new values from sigFloat.di.sigUpdFloat(s) and writes them
 *    into sigFloat.di.sigValFloat. This changes the input values of connected Machines, marking them dirty.
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
extern osp::fw::FeatureDef const ftrSignalsFloat;



/**
 * @brief Logic and queues for spawning vehicles
 *
 * Note that vehicles don't really exist in the scene, and are just collections
 * of conencted Parts
 */
extern osp::fw::FeatureDef const ftrVehicleSpawn;


extern osp::fw::FeatureDef const ftrVehicleSpawnDraw;

/**
 * @brief Support VehicleBuilder data to be used to spawn vehicles
 */
extern osp::fw::FeatureDef const ftrVehicleSpawnVBData;


} // namespace adera
