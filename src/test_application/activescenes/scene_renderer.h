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

#include "../ActiveApplication.h"


namespace testapp::scenes
{


osp::Session setup_magnum_application(
        Builder_t&                      rBuilder,
        osp::ArrayView<entt::any>       topData,
        osp::Tags&                      rTags,
        osp::TopDataId const            idResources,
        ActiveApplication::Arguments    args);

/**
 * @brief Magnum-powered OpenGL Renderer
 */
osp::Session setup_scene_renderer(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         magnum,
        osp::Session const&         scene,
        osp::TopDataId              idResources);

/**
 * @brief Assign a material from setup_material to use Magnum MeshVisualizer
 */
osp::Session setup_shader_visualizer(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         magnum,
        osp::Session const&         scene,
        osp::Session const&         scnRender,
        osp::Session const&         material);


osp::Session setup_shader_flat(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         magnum,
        osp::Session const&         scene,
        osp::Session const&         scnRender,
        osp::Session const&         material);


osp::Session setup_shader_phong(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         magnum,
        osp::Session const&         scene,
        osp::Session const&         scnRender,
        osp::Session const&         material);

osp::Session setup_cursor(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         magnum,
        osp::Session const&         scnCommon,
        osp::Session const&         scnRender,
        osp::Session const&         cameraCtrl,
        osp::Session const&         shFlat,
        osp::TopDataId const        idResources,
        osp::PkgId const            pkg);

osp::Session setup_uni_test_planets_renderer(
        Builder_t&                  rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Tags&                  rTags,
        osp::Session const&         magnum,
        osp::Session const&         scnRender,
        osp::Session const&         scnCommon,
        osp::Session const&         cameraCtrl,
        osp::Session const&         visualizer,
        osp::Session const&         uniCore,
        osp::Session const&         uniScnFrame,
        osp::Session const&         uniTestPlanets);

}
