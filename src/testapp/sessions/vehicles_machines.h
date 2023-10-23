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

namespace testapp::scenes
{

/**
 * @brief Controls to select and control a UserControl Machine
 */
osp::Session setup_vehicle_control(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         windowApp,
        osp::Session const&         scene,
        osp::Session const&         parts,
        osp::Session const&         signalsFloat);

/**
 * @brief Camera which can free cam or follow a selected vehicle
 */
osp::Session setup_camera_vehicle(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         windowApp,
        osp::Session const&         scene,
        osp::Session const&         sceneRenderer,
        osp::Session const&         commonScene,
        osp::Session const&         physics,
        osp::Session const&         parts,
        osp::Session const&         cameraCtrl,
        osp::Session const&         vehicleCtrl);

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

} // namespace testapp::scenes
