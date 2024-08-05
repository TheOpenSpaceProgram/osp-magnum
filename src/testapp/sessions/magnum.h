#if 0
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
#include "../MagnumApplication.h"

#include <osp/activescene/basic.h>
#include <osp/drawing/drawing.h>

namespace testapp::scenes
{

osp::Session setup_magnum(
        osp::TopTaskBuilder&            rBuilder,
        osp::ArrayView<entt::any>       topData,
        osp::Session const&             application,
        osp::Session const&             windowApp,
        MagnumApplication::Arguments    args);

/**
 * @brief stuff needed to render a scene using Magnum
 */
osp::Session setup_magnum_scene(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         application,
        osp::Session const&         windowApp,
        osp::Session const&         sceneRenderer,
        osp::Session const&         magnum,
        osp::Session const&         scene,
        osp::Session const&         commonScene);

/**
 * @brief Magnum MeshVisualizer shader and optional material for drawing ActiveEnts with it
 */
osp::Session setup_shader_visualizer(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         windowApp,
        osp::Session const&         sceneRenderer,
        osp::Session const&         magnum,
        osp::Session const&         magnumScene,
        osp::draw::MaterialId       materialId = lgrn::id_null<osp::draw::MaterialId>());

/**
 * @brief Magnum Flat shader and optional material for drawing ActiveEnts with it
 */
osp::Session setup_shader_flat(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         windowApp,
        osp::Session const&         sceneRenderer,
        osp::Session const&         magnum,
        osp::Session const&         magnumScene,
        osp::draw::MaterialId       materialId = lgrn::id_null<osp::draw::MaterialId>());

/**
 * @brief Magnum Phong shader and optional material for drawing ActiveEnts with it
 */
osp::Session setup_shader_phong(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         windowApp,
        osp::Session const&         sceneRenderer,
        osp::Session const&         magnum,
        osp::Session const&         magnumScene,
        osp::draw::MaterialId       materialId = lgrn::id_null<osp::draw::MaterialId>());


osp::Session setup_terrain_draw_magnum(
        osp::TopTaskBuilder         &rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session          const &windowApp,
        osp::Session          const &sceneRenderer,
        osp::Session          const &magnum,
        osp::Session          const &magnumScene,
        osp::Session          const &terrain);
}
#endif
