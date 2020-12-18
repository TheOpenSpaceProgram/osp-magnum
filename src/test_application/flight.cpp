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

#include "flight.h"

#include "DebugObject.h"

#include <osp/Active/ActiveScene.h>
#include <osp/Active/SysDebugRender.h>
#include <osp/Active/SysVehicle.h>
#include <osp/Active/SysForceFields.h>
#include <osp/Active/SysAreaAssociate.h>

#include <adera/Machines/UserControl.h>
#include <adera/Machines/Rocket.h>

#include <planet-a/Active/SysPlanetA.h>
#include <planet-a/Satellites/SatPlanet.h>

using osp::Vector2;

using osp::Vector3;
using osp::Quaternion;
using osp::Matrix3;
using osp::Matrix4;

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

using osp::universe::Universe;
using osp::universe::Satellite;
using osp::universe::SatActiveArea;

using osp::universe::UCompActiveArea;

using osp::active::ActiveEnt;
using osp::active::ACompTransform;
using osp::active::ACompCamera;
using osp::active::ACompFloatingOrigin;

using adera::active::machines::SysMachineUserControl;
using adera::active::machines::SysMachineRocket;

using planeta::universe::SatPlanet;


void test_flight(std::unique_ptr<OSPMagnum>& magnumApp,
                 osp::OSPApplication& ospApp, OSPMagnum::Arguments args)
{

    // Get needed variables
    Universe &uni = ospApp.get_universe();
    SatActiveArea *satAA = static_cast<osp::universe::SatActiveArea*>(
            uni.sat_type_find("Vehicle")->second.get());
    SatPlanet *satPlanet = static_cast<planeta::universe::SatPlanet*>(
            uni.sat_type_find("Planet")->second.get());

    // Create the application
    magnumApp = std::make_unique<OSPMagnum>(args, ospApp);

    // Configure the controls
    config_controls(*magnumApp);

    // Create an ActiveScene
    osp::active::ActiveScene& scene = magnumApp->scene_add("Area 1");

    // Register dynamic systems for that scene

    auto &sysPhysics = scene.dynamic_system_add<osp::active::SysPhysics>(
                "Physics");
    auto &sysWire = scene.dynamic_system_add<osp::active::SysWire>(
                "Wire");
    auto &sysDebugRender = scene.dynamic_system_add<osp::active::SysDebugRender>(
                "DebugRender");
    auto &sysArea = scene.dynamic_system_add<osp::active::SysAreaAssociate>(
                "AreaAssociate", uni);
    auto &sysVehicle = scene.dynamic_system_add<osp::active::SysVehicle>(
                "Vehicle");
    auto &sysPlanet = scene.dynamic_system_add<planeta::active::SysPlanetA>(
                "Planet");
    auto &sysGravity = scene.dynamic_system_add<osp::active::SysFFGravity>(
                "FFGravity");

    // Register machines for that scene
    scene.system_machine_add<SysMachineUserControl>("UserControl",
            magnumApp->get_input_handler());
    scene.system_machine_add<SysMachineRocket>("Rocket");

    // Make active areas load vehicles and planets
    sysArea.activator_add(satAA, sysVehicle);
    sysArea.activator_add(satPlanet, sysPlanet);

    // create a Satellite with an ActiveArea
    Satellite areaSat = uni.sat_create();

    // assign sat as an ActiveArea
    UCompActiveArea &area = satAA->add_get_ucomp(areaSat);

    // Link ActiveArea to scene using the AreaAssociate
    sysArea.connect(areaSat);

    // Add a camera to the scene

    // Create the camera entity
    ActiveEnt camera = scene.hier_create_child(scene.hier_get_root(),
                                                       "Camera");
    auto &cameraTransform = scene.reg_emplace<ACompTransform>(camera);
    auto &cameraComp = scene.get_registry().emplace<ACompCamera>(camera);

    cameraTransform.m_transform = Matrix4::translation(Vector3(0, 0, 25));
    scene.reg_emplace<ACompFloatingOrigin>(camera);

    cameraComp.m_viewport
            = Vector2(Magnum::GL::defaultFramebuffer.viewport().size());
    cameraComp.m_far = 4096.0f;
    cameraComp.m_near = 0.125f;
    cameraComp.m_fov = 45.0_degf;

    cameraComp.calculate_projection();

    // Add the debug camera controller to the scene. This adds controls
    auto camObj = std::make_unique<DebugCameraController>(scene, camera);

    // Add a ACompDebugObject to camera to manage camObj's lifetime
    scene.reg_emplace<ACompDebugObject>(camera, std::move(camObj));

    // Starts the game loop. This function is blocking, and will only return
    // when the window is  closed. See OSPMagnum::drawEvent
    magnumApp->exec();

    // Close button has been pressed

    std::cout << "Magnum Application closed\n";

    // Disconnect ActiveArea
    sysArea.disconnect();

    // workaround: wipe mesh resources because they're specific to the
    // opengl context
    ospApp.debug_get_packges()[0].clear<Magnum::GL::Mesh>();
    ospApp.debug_get_packges()[0].clear<Magnum::GL::Texture2D>();

    // destruct the application, this closes the window
    magnumApp.reset();
}
