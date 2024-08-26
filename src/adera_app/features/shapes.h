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

#include <osp/activescene/basic.h>
#include <osp/activescene/physics.h>
#include <osp/drawing/drawing.h>

namespace adera
{

struct SpawnShape
{
    osp::Vector3    m_position;
    osp::Vector3    m_velocity;
    osp::Vector3    m_size;
    float           m_mass;
    osp::EShape     m_shape;
};

struct ACtxPhysShapes
{
    osp::active::ActiveEntSet_t     ownedEnts;

    std::vector<SpawnShape>         m_spawnRequest;
    osp::active::ActiveEntVec_t     m_ents;
};

void add_floor(
        osp::fw::Framework          &rFW,
        osp::fw::ContextId          sceneCtx,
        osp::PkgId                  pkg,
        int                         size);


/**
 * @brief Queues and logic for spawning physics shapes
 */
extern osp::fw::FeatureDef const ftrPhysicsShapes;


extern osp::fw::FeatureDef const ftrPhysicsShapesDraw;


/**
 * @brief Throws spheres when pressing space
 */
extern osp::fw::FeatureDef const ftrThrower;


/**
 * @brief Spawn blocks every 2 seconds and spheres every 1 second
 */
extern osp::fw::FeatureDef const ftrDroppers;


/**
 * @brief Entity set to delete entities under Z = -10, added to spawned shapes
 */
extern osp::fw::FeatureDef const ftrBounds;



}
