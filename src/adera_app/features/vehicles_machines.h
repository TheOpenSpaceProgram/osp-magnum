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
 * @brief Links for Magic Rockets
 *
 * This only sets up links and does not apply forces, see setup_rocket_thrust_newton
 */
extern osp::fw::FeatureDef const ftrMachMagicRockets;


/**
 * @brief Links for RCS Drivers, which output thrust levels given pitch/yaw/roll controls
 */
extern osp::fw::FeatureDef const ftrMachRCSDriver;


/**
 * @brief Controls to select and control a UserControl Machine
 */
extern osp::fw::FeatureDef const ftrVehicleControl;


/**
 * @brief Camera which can free cam or follow a selected vehicle
 */
extern osp::fw::FeatureDef const ftrVehicleCamera;


/**
 * @brief Red indicators over Magic Rockets
 */
extern osp::fw::FeatureDef const ftrMagicRocketThrustIndicator;


} // namespace adera
