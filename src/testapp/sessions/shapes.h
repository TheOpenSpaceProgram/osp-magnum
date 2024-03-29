/**
 * Open Space Program
 * Copyright © 2019-2023 Open Space Program Project
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

#include <osp/activescene/basic.h>
#include <osp/activescene/physics.h>
#include <osp/drawing/drawing.h>

namespace testapp::scenes
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
    osp::draw::MaterialId           m_materialId;
};

void add_floor(
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         physShapes,
        osp::draw::MaterialId       material,
        osp::PkgId                  pkg,
        int                         size);


/**
 * @brief Queues and logic for spawning physics shapes
 */
osp::Session setup_phys_shapes(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         scene,
        osp::Session const&         commonScene,
        osp::Session const&         physics,
        osp::draw::MaterialId       materialId);

osp::Session setup_phys_shapes_draw(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         windowApp,
        osp::Session const&         sceneRenderer,
        osp::Session const&         commonScene,
        osp::Session const&         physics,
        osp::Session const&         physShapes);

/**
 * @brief Throws spheres when pressing space
 */
osp::Session setup_thrower(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         windowApp,
        osp::Session const&         cameraCtrl,
        osp::Session const&         physShapes);

/**
 * @brief Spawn blocks every 2 seconds and spheres every 1 second
 */
osp::Session setup_droppers(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         scene,
        osp::Session const&         commonScene,
        osp::Session const&         physShapes);

/**
 * @brief Entity set to delete entities under Z = -10, added to spawned shapes
 */
osp::Session setup_bounds(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         scene,
        osp::Session const&         commonScene,
        osp::Session const&         physShapes);


}

