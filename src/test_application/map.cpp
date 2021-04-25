/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
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

#include "map.h"
#include <memory>
#include "DebugMapCamera.h"
#include <osp/types.h>
#include <osp/Universe.h>
#include <adera/SysMap.h>
#include <osp/Active/SysRender.h>

using osp::universe::Universe;
using osp::active::ActiveScene;
using osp::active::ActiveEnt;
using osp::active::ACompTransform;
using osp::active::ACompCamera;
using osp::Matrix4;
using osp::Vector3;
using osp::Vector2;
using osp::active::SysRender;
using osp::active::ACompRenderingAgent;
using osp::active::ACompRenderTarget;
using osp::active::ACompPerspective3DView;
using osp::active::ACompRenderer;
// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

void testapp::test_map(std::unique_ptr<OSPMagnum>& pMagnumApp,
    osp::OSPApplication & rOspApp, OSPMagnum::Arguments args)
{
    // Get needed variables
    Universe &rUni = rOspApp.get_universe();

    // Create the application
    pMagnumApp = std::make_unique<OSPMagnum>(args, rOspApp);

    // Configure the controls
    config_controls(*pMagnumApp);

    // Create an ActiveScene
    ActiveScene& rScene = pMagnumApp->scene_create("Map Screen");

    // Add systems
    adera::active::SysMap::add_functions(rScene);
    adera::active::SysMap::setup(rScene);
    osp::active::SysRender::setup(rScene);

    // Camera
    ActiveEnt camera = rScene.hier_create_child(rScene.hier_get_root(), "Camera");
    auto& cameraTransform = rScene.reg_emplace<ACompTransform>(camera);
    auto& cameraComp = rScene.reg_emplace<ACompCamera>(camera);
    rScene.reg_emplace<ACompRenderingAgent>(camera, SysRender::get_default_rendertarget(rScene));
    rScene.reg_emplace<ACompPerspective3DView>(camera, camera);
    rScene.reg_emplace<ACompRenderer>(camera, "map");

    cameraTransform.m_transform = Matrix4::translation(Vector3(0, 0, 2000.0f));
    cameraComp.m_viewport = Vector2(Magnum::GL::defaultFramebuffer.viewport().size());
    cameraComp.m_far = 1e8f;
    cameraComp.m_near = 1.0f;
    cameraComp.m_fov = 45.0_degf;
    cameraComp.calculate_projection();

    // Camera controller
    auto camObj = std::make_unique<DebugMapCameraController>(rScene, camera);

    rScene.reg_emplace<ACompDebugObject>(camera, std::move(camObj));

    // Start the game loop
    pMagnumApp->exec();

    // Close button was pressed
    std::cout << "Magnum Application closed\n";

    // Destruct application
    pMagnumApp.reset();
}
