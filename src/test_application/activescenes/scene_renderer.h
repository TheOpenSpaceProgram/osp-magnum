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

#include <osp/Active/activetypes.h>

#include "../MagnumApplication.h"


namespace testapp::scenes
{

osp::Session setup_window_app(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         application);

osp::Session setup_scene_renderer(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         application,
        osp::Session const&         windowApp,
        osp::Session const&         commonScene);

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
        osp::active::MaterialId     materialId = lgrn::id_null<osp::active::MaterialId>());



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
        osp::active::MaterialId     materialId = lgrn::id_null<osp::active::MaterialId>());

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
        osp::active::MaterialId     materialId = lgrn::id_null<osp::active::MaterialId>());

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

/**
 * @brief Wireframe cube over the camera controller's target
 */
osp::Session setup_cursor(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         magnum,
        osp::Session const&         commonScene,
        osp::Session const&         scnRender,
        osp::Session const&         cameraCtrl,
        osp::Session const&         shFlat,
        osp::TopDataId const        idResources,
        osp::PkgId const            pkg);

/**
 * @brief Draw universe, specifically designed for setup_uni_test_planets
 */
osp::Session setup_uni_test_planets_renderer(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         magnum,
        osp::Session const&         scnRender,
        osp::Session const&         commonScene,
        osp::Session const&         cameraCtrl,
        osp::Session const&         visualizer,
        osp::Session const&         uniCore,
        osp::Session const&         uniScnFrame,
        osp::Session const&         uniTestPlanets);

}
