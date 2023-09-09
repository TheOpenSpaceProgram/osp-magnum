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

namespace testapp::scenes
{

void create_materials(
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         sceneRenderer,
        int                         count);

void add_floor(
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         shapeSpawn,
        osp::active::MaterialId     material,
        osp::PkgId                  pkg);

/**
 * @brief Create CameraController connected to an app's UserInputHandler
 */
osp::Session setup_camera_ctrl(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         windowApp,
        osp::Session const&         sceneRenderer,
        osp::Session const&         magnumScene);

/**
 * @brief Adds free cam controls to a CameraController
 */
osp::Session setup_camera_free(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         windowApp,
        osp::Session const&         scene,
        osp::Session const&         camera);

/**
 * @brief Throws spheres when pressing space
 */
osp::Session setup_thrower(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         windowApp,
        osp::Session const&         cameraCtrl,
        osp::Session const&         shapeSpawn);

/**
 * @brief Spawn blocks every 2 seconds and spheres every 1 second
 */
osp::Session setup_droppers(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         scene,
        osp::Session const&         commonScene,
        osp::Session const&         shapeSpawn);

/**
 * @brief Entity set to delete entities under Z = -10, added to spawned shapes
 */
osp::Session setup_bounds(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         scene,
        osp::Session const&         commonScene,
        osp::Session const&         shapeSpawn);


}
