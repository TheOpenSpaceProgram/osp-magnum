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
#include <osp/Active/SysHierarchy.h>
#include <osp/Active/SysRender.h>
#include <osp/Active/SysPhysics.h>
#include <osp/Active/SysVehicle.h>
#include <osp/Active/SysWire.h>
#include <osp/Active/SysForceFields.h>
#include <osp/Active/SysAreaAssociate.h>

#include <osp/Satellites/SatVehicle.h>

#include <adera/Machines/Container.h>
#include <adera/Machines/RCSController.h>
#include <adera/Machines/Rocket.h>
#include <adera/Machines/UserControl.h>

#include <adera/wiretypes.h>

#include <adera/SysExhaustPlume.h>
#include <adera/ShipResources.h>

#include <planet-a/Active/SysPlanetA.h>
#include <planet-a/Satellites/SatPlanet.h>

#include <osp/Shaders/Phong.h>
#include <osp/Shaders/FullscreenTriShader.h>
#include <Magnum/Shaders/MeshVisualizer.h>

#include <osp/Active/SysNewton.h>

using namespace testapp;


// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

using osp::universe::Universe;
using osp::universe::Satellite;
using osp::universe::SatActiveArea;
using osp::universe::SatVehicle;

using osp::universe::UCompActiveArea;

using osp::active::ActiveEnt;
using osp::active::ActiveScene;
using osp::active::ACompTransform;
using osp::active::ACompCamera;
using osp::active::ACompFloatingOrigin;

using osp::active::SysHierarchy;
using osp::active::SysRender;
using osp::active::ACompDrawTransform;
using osp::active::ACompRenderTarget;
using osp::active::ACompRenderingAgent;
using osp::active::ACompPerspective3DView;
using osp::active::ACompRenderer;

using osp::active::SysWire;
using osp::active::SysSignal;
using osp::active::ACompWire;
using osp::active::ACompWireNodes;

using adera::active::machines::SysMachineUserControl;
using adera::active::machines::SysMachineRocket;
using adera::active::machines::SysMachineRCSController;
using adera::active::machines::SysMachineContainer;

using planeta::universe::SatPlanet;

using osp::Vector2;
using osp::Vector3;
using osp::Quaternion;
using osp::Matrix3;
using osp::Matrix4;

void load_shaders(osp::active::ActiveScene& rScene);

void testapp::test_flight(std::unique_ptr<OSPMagnum>& pMagnumApp,
                 osp::OSPApplication& rOspApp, OSPMagnum::Arguments args)
{

    // Get Universe
    Universe &rUni = rOspApp.get_universe();

    // Create the application
    pMagnumApp = std::make_unique<OSPMagnum>(args, rOspApp);

    // Configure the controls
    config_controls(*pMagnumApp);

    // Create an ActiveScene
    ActiveScene& rScene = pMagnumApp->scene_create("Area 1");

    // Setup hierarchy, initialize root entity
    SysHierarchy::setup(rScene);

    // Add system functions needed for flight scene
    osp::active::SysNewton::add_functions(rScene);
    osp::active::SysAreaAssociate::add_functions(rScene);
    osp::active::SysVehicle::add_functions(rScene);
    osp::active::SysFFGravity::add_functions(rScene);

    adera::active::SysExhaustPlume::add_functions(rScene);

    planeta::active::SysPlanetA::add_functions(rScene);
  
    // Setup Machine systems
    SysMachineContainer::add_functions(rScene);
    SysMachineRCSController::add_functions(rScene);
    SysMachineRocket::add_functions(rScene);
    SysMachineUserControl::add_functions(rScene);

    // Add user controls for SysMachineUserControl
    // TODO: Eventually restructure MachineUserControl to instead have controls
    //       as members that can be written to instead of listening directly
    //       to a UserInputHandler
    rScene.reg_emplace<adera::active::machines::ACompUserControl>(
                rScene.hier_get_root(), pMagnumApp->get_input_handler());

    // ##### Setup Wiring #####

    // Add ACompWire to scene with update functions for passing Percent and
    // AttitudeControl values between Machines
    SysWire::setup_default(
            rScene, 5,
            {&SysMachineRocket::update_calculate,
             &SysMachineRCSController::update_calculate},
            {&SysSignal<adera::wire::Percent>::signal_update_nodes,
             &SysSignal<adera::wire::AttitudeControl>::signal_update_nodes});

    // Add scene components for storing 'Nodes' used for wiring
    rScene.reg_emplace< ACompWireNodes<adera::wire::AttitudeControl> >(rScene.hier_get_root());
    rScene.reg_emplace< ACompWireNodes<adera::wire::Percent> >(rScene.hier_get_root());

    SysWire::add_functions(rScene);

    // Add functions to wire vehicles when they're added to the scene
    rScene.debug_update_add(
            rScene.get_update_order(),
            "wire_percent_construct", "vehicle_activate", "vehicle_modification",
            &SysSignal<adera::wire::Percent>::signal_update_construct);
    rScene.debug_update_add(
            rScene.get_update_order(),
            "wire_attctrl_construct", "vehicle_activate", "vehicle_modification",
            &SysSignal<adera::wire::AttitudeControl>::signal_update_construct);

    // create a Satellite with an ActiveArea
    Satellite areaSat = rUni.sat_create();

    // assign sat as an ActiveArea
    UCompActiveArea &area = SatActiveArea::add_active_area(rUni, areaSat);

    // Link ActiveArea to scene using the AreaAssociate
    osp::active::SysAreaAssociate::connect(rScene, rUni, areaSat);

    // Add default-constructed physics world to scene
    rScene.reg_emplace<osp::active::ACompNwtWorld>(rScene.hier_get_root());

    // ##### Add a camera to the scene #####

    // Create the camera entity
    ActiveEnt camera = SysHierarchy::create_child(
                rScene, rScene.hier_get_root(), "Camera");

    // Configure camera transformation
    auto &cameraTransform = rScene.reg_emplace<ACompTransform>(camera);
    rScene.reg_emplace<ACompDrawTransform>(camera);
    cameraTransform.m_transform = Matrix4::translation(Vector3(0, 0, 25));
    rScene.reg_emplace<ACompFloatingOrigin>(camera);

    // Configure camera component, projection
    auto &cameraComp = rScene.reg_emplace<ACompCamera>(camera);
    cameraComp.m_viewport
            = Vector2(Magnum::GL::defaultFramebuffer.viewport().size());
    cameraComp.m_far = 1u << 24;
    cameraComp.m_near = 1.0f;
    cameraComp.m_fov = 45.0_degf;

    cameraComp.calculate_projection();

    // Add the debug camera controller to the scene. This adds controls
    auto camObj = std::make_unique<DebugCameraController>(
                rScene, camera, pMagnumApp->get_input_handler());

    // Add a ACompDebugObject to camera to manage camObj's lifetime
    rScene.reg_emplace<ACompDebugObject>(camera, std::move(camObj));

    // Configure default rendering system
    osp::active::SysRender::setup(rScene);
    load_shaders(rScene);

    // Connect camera to rendering system; set up with basic 3D renderer
    rScene.reg_emplace<ACompRenderingAgent>(camera, SysRender::get_default_rendertarget(rScene));
    rScene.reg_emplace<ACompPerspective3DView>(camera, camera);
    rScene.reg_emplace<ACompRenderer>(camera);


    // Starts the game loop. This function is blocking, and will only return
    // when the window is  closed. See OSPMagnum::drawEvent
    pMagnumApp->exec();

    // Close button has been pressed

    SPDLOG_LOGGER_INFO(rOspApp.get_logger(),
                       "Closed Magnum Application");

    // Disconnect ActiveArea
    osp::active::SysAreaAssociate::disconnect(rScene);

    // destruct the application, this closes the window
    pMagnumApp.reset();
}

void load_shaders(osp::active::ActiveScene& rScene)
{
    using adera::shader::PlumeShader;
    using Magnum::Shaders::MeshVisualizer3D;
    auto& resources = rScene.get_context_resources();

    resources.add<PlumeShader>("plume_shader");

    resources.add<MeshVisualizer3D>("mesh_vis_shader",
        MeshVisualizer3D{MeshVisualizer3D::Flag::Wireframe
        | MeshVisualizer3D::Flag::NormalDirection});
}
