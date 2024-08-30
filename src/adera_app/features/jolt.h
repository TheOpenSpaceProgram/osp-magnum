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

#include <osp/framework/builder.h>

#include <osp/core/math_types.h>

#include <ospjolt/forcefactors.h>


namespace adera
{

struct ACtxConstAccel
{
    struct Force
    {
        osp::Vector3 vec;
        std::uint8_t factorIndex;
    };

    std::vector<Force> forces;
};

/**
 * @brief Jolt physics integration
 */
extern osp::fw::FeatureDef const ftrJolt;

ospjolt::ForceFactors_t add_constant_acceleration(
        osp::Vector3                forceVec,
        osp::fw::Framework          &rFW,
        osp::fw::ContextId          sceneCtx);

void set_phys_shape_factors(
        ospjolt::ForceFactors_t     factors,
        osp::fw::Framework          &rFW,
        osp::fw::ContextId          sceneCtx);

void set_vehicle_default_factors(
        ospjolt::ForceFactors_t     factors,
        osp::fw::Framework          &rFW,
        osp::fw::ContextId          sceneCtx);

/**
 * @brief Setup constant acceleration force
 */
extern osp::fw::FeatureDef const ftrJoltConstAccel;

/**
 * @brief Support for Shape Spawner physics using Jolt Physics
 */
extern osp::fw::FeatureDef const ftrPhysicsShapesJolt;

/**
 * @brief Support for Vehicle physics using Jolt Physics
 */
extern osp::fw::FeatureDef const ftrVehicleSpawnJolt;

/**
 * @brief Add thrust forces to Magic Rockets from setup_mach_rocket
 */
extern osp::fw::FeatureDef const ftrRocketThrustJolt;

} // namespace adera

