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

#include <osp/activescene/basic.h>
#include <osp/activescene/physics.h>
#include <osp/drawing/drawing.h>

#include <longeron/id_management/registry_stl.hpp>

#include <string_view>

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

struct ACtxShapeSpawner
{
    osp::active::ActiveEntSet_t     m_ownedEnts;

    std::vector<SpawnShape>         m_spawnRequest;
    osp::active::ActiveEntVec_t     m_ents;
    osp::draw::MaterialId           m_materialId;
};

/**
 * @brief Physical properties for entities and generic Physics interface
 *
 * Independent of whichever physics engine is used
 */
osp::Session setup_physics(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         scene,
        osp::Session const&         commonScene);

/**
 * @brief Queues and logic for spawning physics shapes
 */
osp::Session setup_shape_spawn(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         scene,
        osp::Session const&         commonScene,
        osp::Session const&         physics,
        osp::draw::MaterialId       materialId);

osp::Session setup_shape_spawn_draw(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         windowApp,
        osp::Session const&         sceneRenderer,
        osp::Session const&         commonScene,
        osp::Session const&         physics,
        osp::Session const&         shapeSpawn);

/**
 * @brief Queues and logic for spawning Prefab resources
 */
osp::Session setup_prefabs(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         commonScene,
        osp::Session const&         physics,
        osp::Session const&         material,
        osp::TopDataId              idResources);

} // namespace testapp::scenes
