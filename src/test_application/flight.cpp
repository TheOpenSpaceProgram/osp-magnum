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

#include "CameraController.h"

#include <osp/Active/ActiveScene.h>
#include <osp/Active/SysHierarchy.h>
#include <osp/Active/SysRender.h>
#include <osp/Active/SysPhysics.h>
#include <osp/Active/SysVehicle.h>
#include <osp/Active/SysVehicleSync.h>
#include <osp/Active/SysWire.h>
#include <osp/Active/SysForceFields.h>
#include <osp/Active/SysAreaAssociate.h>

#include <osp/CoordinateSpaces/CartesianSimple.h>

#include <osp/Satellites/SatActiveArea.h>
#include <osp/Satellites/SatVehicle.h>

#include <osp/logging.h>

#include <adera/Machines/Container.h>
#include <adera/Machines/RCSController.h>
#include <adera/Machines/Rocket.h>
#include <adera/Machines/UserControl.h>

#include <adera/Shaders/PlumeShader.h>

#include <adera/wiretypes.h>

#include <adera/SysExhaustPlume.h>
#include <adera/ShipResources.h>

#include <planet-a/Active/SysPlanetA.h>
#include <planet-a/Satellites/SatPlanet.h>

#include <osp/Shaders/Phong.h>
#include <osp/Shaders/MeshVisualizer.h>
#include <osp/Shaders/FullscreenTriShader.h>
#include <Magnum/Shaders/MeshVisualizerGL.h>

#include <newtondynamics_physics/SysNewton.h>

using namespace testapp;


// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

using osp::universe::Universe;
using osp::universe::Satellite;

using osp::universe::SatActiveArea;
using osp::universe::SatVehicle;

using osp::universe::UCompInCoordspace;
using osp::universe::UCompActiveArea;

using osp::universe::coordspace_index_t;

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

using planeta::universe::SatPlanet;

using osp::Vector2;
using osp::Vector3;
using osp::Quaternion;
using osp::Matrix3;
using osp::Matrix4;

void load_shaders(ActiveScene& rScene);

void update_scene(ActiveScene& rScene);

void setup_wiring(ActiveScene& rScene);

Satellite active_area_create(
        osp::OSPApplication& rOspApp, Universe &rUni,
        coordspace_index_t targetCoordspace);

void active_area_destroy(
        osp::OSPApplication& rOspApp, Universe &rUni, Satellite areaSat);

void testapp::test_flight(
        std::unique_ptr<ActiveApplication>& pMagnumApp, osp::OSPApplication& rOspApp,
        Universe &rUni, universe_update_t& rUniUpd, ActiveApplication::Arguments args)
{

    // Create the application, and its draw function
    pMagnumApp = std::make_unique<ActiveApplication>(
            args, rOspApp,
            [&rUniUpd, &rUni] (ActiveApplication& rMagnumApp)
    {
        // Update the universe each frame
        // This likely wouldn't be here in the future
        rUniUpd(rUni);

        rMagnumApp.update_scenes(); // Update scenes each frame
        rMagnumApp.draw_scenes(); // Draw each frame of course
    });

    // Configure the controls
    config_controls(*pMagnumApp);

    // Create an ActiveScene
    ActiveScene& rScene = pMagnumApp->scene_create("Area 1", &update_scene);

    // Setup hierarchy, initialize root entity
    SysHierarchy::setup(rScene);

    // Setup wiring
    setup_wiring(rScene);

    // create a Satellite with an ActiveArea, then link it to the scene
    Satellite areaSat = active_area_create(rOspApp, rUni, 0);
    rUniUpd(rUni);
    osp::active::SysAreaAssociate::connect(rScene, rUni, areaSat);


    // Setup sync states used by scene systems to sync with the universe
    rScene.get_registry().set<osp::active::SyncVehicles>();
    rScene.get_registry().set<planeta::active::SyncPlanets>();

    // Setup generic physics interface
    rScene.get_registry().set<osp::active::ACtxPhysics>();

    // Setup Newton dynamics physics
    ospnewton::SysNewton::setup(rScene);

    // workaround: update the scene right away to initialize physics world
    //             needed by planets for now
    ospnewton::SysNewton::update_world(rScene);

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

    // Add the camera controller to the camera. This adds controls
    rScene.reg_emplace<ACompCameraController>(
                camera, pMagnumApp->get_input_handler());

    // Configure default rendering system
    osp::active::SysRender::setup_context(rScene.get_context_resources());
    osp::active::SysRender::setup_forward_renderer(rScene);

    // Load shaders
    load_shaders(rScene);

    // Connect camera to rendering system; set up with basic 3D renderer
    rScene.reg_emplace<ACompRenderingAgent>(camera, SysRender::get_default_rendertarget(rScene));
    rScene.reg_emplace<ACompPerspective3DView>(camera, camera);
    rScene.reg_emplace<ACompRenderer>(camera);

    // Starts the main loop. This function is blocking, and will only return
    // once the window is closed. See ActiveApplication::drawEvent
    pMagnumApp->exec();

    // Window has been closed

    OSP_LOG_INFO("Closed Magnum Application");

    rUniUpd(rUni); // make sure universe is in a valid state

    active_area_destroy(rOspApp, rUni, areaSat); // Disconnect ActiveArea
    rUniUpd(rUni);

    rUni.get_reg().destroy(areaSat);

    // Release Newton resources
    ospnewton::SysNewton::destroy(rScene);

    // destruct the application, this closes the window
    pMagnumApp.reset();
}

void update_scene(osp::active::ActiveScene& rScene)
{
    using namespace osp::active;
    using namespace adera::active;
    using namespace adera::active::machines;
    using namespace planeta::active;

    SysAreaAssociate::update_consume(rScene);

    SysAreaAssociate::update_translate(rScene);
    ospnewton::SysNewton::update_translate(rScene);

    // Activate or deactivate nearby planets
    SysPlanetA::update_activate(rScene);

    // Activate or deactivate nearby vehicles
    SysVehicleSync::update_universe_sync(rScene);

    SysCameraController::update_area(rScene);

    // Construct components of vehicles. These should be parallelizable
    SysMachineContainer::update_construct(rScene);
    SysMachineRCSController::update_construct(rScene);
    SysMachineRocket::update_construct(rScene);
    SysMachineUserControl::update_construct(rScene);
    SysSignal<adera::wire::Percent>::signal_update_construct(rScene);
    SysSignal<adera::wire::AttitudeControl>::signal_update_construct(rScene);

    // Plume construction is a bit hacky, and depend on Rockets
    SysExhaustPlume::update_construct(rScene);

    // CameraController may tamper with vehicles for debugging reasons
    SysCameraController::update_vehicle(rScene);

    // Update changed vehicles
    SysVehicle::update_vehicle_modification(rScene);

    // Write controls into selected vehicle
    SysCameraController::update_controls(rScene);

    // UserControl reads possibly changed values written by CameraController
    SysMachineUserControl::update_sensor(rScene);

    // Assign shaders to newly created entities
    SysRender::update_drawfunc_assign(rScene);

    // Update wires
    SysWire::update_wire(rScene);

    // Rockets apply thrust
    SysMachineRocket::update_physics(rScene);

    // Apply gravity forces
    SysFFGravity::update_force(rScene);

    // Planets update geometry
    SysPlanetA::update_geometry(rScene);

    // Containers update mass
    SysMachineContainer::update_containers(rScene);

    // ** Physics update **
    ospnewton::SysNewton::update_world(rScene);

    // Update rocket plume effects
    SysExhaustPlume::update_plumes(rScene);

    // Move the camera
    // TODO: move this into a drawing function, since interpolation in the
    //       future may mean multiple frames drawn between physics frames
    SysCameraController::update_view(rScene);

    // Add ACompDelete to descendents of hierarchy entities with ACompDelete
    SysHierarchy::update_delete(rScene);

    // Delete entities from RenderGroups
    SysRender::update_drawfunc_delete(rScene);

    // Delete entities with ACompDelete
    ActiveScene::update_delete(rScene);
}

void setup_wiring(ActiveScene& rScene)
{
    using namespace osp::active;
    using namespace adera::active::machines;

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

}

Satellite active_area_create(
        osp::OSPApplication& rOspApp, Universe &rUni,
        coordspace_index_t targetIndex)
{
    using namespace osp::universe;

    // create a Satellite
    Satellite areaSat = rUni.sat_create();

    // assign sat as an ActiveArea
    auto &rArea = rUni.get_reg().emplace<UCompActiveArea>(areaSat);

    // captured satellites will be put into the target coordspace when released
    rArea.m_releaseSpace = targetIndex;

    // Create the "ActiveArea Domain" Coordinte Space
    // This is a CoordinateSpace that acts like a layer overtop of the target
    // CoordinateSpace. The ActiveArea is free to roam around in this space
    // unaffected by the target coordspace's trajectory function.
    {
        // Make the Domain CoordinateSpace identical to the target CoordinateSpace
        CoordinateSpace const &targetCoord = rUni.coordspace_get(targetIndex);

        // Make the Domain CoordinateSpace identical to the target CoordinateSpace
        auto const& [domainIndex, rDomainCoord] = rUni.coordspace_create(targetCoord.m_parentSat);
        rUni.coordspace_update_depth(domainIndex);
        rDomainCoord.m_pow2scale = targetCoord.m_pow2scale;

        rDomainCoord.m_data.emplace<CoordspaceCartesianSimple>();
        auto *pDomainData = entt::any_cast<CoordspaceCartesianSimple>(&rDomainCoord.m_data);

        // Add ActiveArea to the Domain Coordinate space
        rDomainCoord.add(areaSat, {}, {});
        rUni.coordspace_update_sats(domainIndex);
        pDomainData->update_exchange(rUni, rDomainCoord, *pDomainData);
        pDomainData->update_views(rDomainCoord, *pDomainData);
    }

    // Create the "ActiveArea Capture" CoordinateSpace
    // This is a coordinate space for Satellites captured inside of the
    // ActiveArea and can be modified in the ActiveScene, such as Vehicles.
    {
        auto const& [captureIndex, rCaptureSpace] = rUni.coordspace_create(areaSat);
        rUni.coordspace_update_depth(captureIndex);
        rCaptureSpace.m_data.emplace<CoordspaceCartesianSimple>();

        // Make the ActiveArea aware of the capture space's existance
        rArea.m_captureSpace = captureIndex;
    }

    return areaSat;
}

void active_area_destroy(
        osp::OSPApplication& rOspApp, Universe &rUni, Satellite areaSat)
{
    using namespace osp::universe;

    auto &rArea = rUni.get_reg().get<UCompActiveArea>(areaSat);
    CoordinateSpace &rRelease = rUni.coordspace_get(rArea.m_releaseSpace);
    CoordinateSpace &rCapture = rUni.coordspace_get(rArea.m_captureSpace);
    auto viewSats = rCapture.ccomp_view<CCompSat>();
    auto viewPos = rCapture.ccomp_view_tuple<CCompX, CCompY, CCompZ>();

    CoordspaceTransform transform = rUni.coordspace_transform(rCapture, rRelease).value();

    for (uint32_t i = 0; i < viewSats.size(); i ++)
    {

        auto const posLocal = make_from_ccomp<Vector3g>(*viewPos, i);
        Vector3g pos = transform(posLocal);
        rCapture.remove(i);
        rRelease.add(viewSats[i], pos, {});
    }

}

void load_shaders(osp::active::ActiveScene& rScene)
{
    using namespace osp::active;
    using osp::shader::Phong;
    using osp::shader::MeshVisualizer;

    using adera::shader::PlumeShader;
    using adera::active::MaterialPlume;

    auto &rResources = rScene.get_context_resources();
    auto &rGroups = rScene.get_registry().ctx<ACtxRenderGroups>();

    using osp::DependRes;

    DependRes<Phong> phongTex = rResources.add<Phong>(
                "textured",Phong{Phong::Flag::DiffuseTexture});
    DependRes<Phong> phongNoTex = rResources.add<Phong>("notexture", Phong{});

    DependRes<PlumeShader> plume = rResources.add<PlumeShader>("plume_shader");

    DependRes<MeshVisualizer> visual = rResources.add<MeshVisualizer>(
            "mesh_vis_shader", MeshVisualizer{
                MeshVisualizer::Flag::Wireframe
                 | MeshVisualizer::Flag::NormalDirection});

    rGroups.resize_to_fit<MaterialCommon, MaterialPlume, MaterialTerrain>();

    // Use Phong shader for common materials
    rGroups.m_groups["fwd_opaque"].set_assigner<MaterialCommon>(
            Phong::gen_assign_phong_opaque( phongNoTex.operator->(),
                                            phongTex.operator->() ));

    // Use Plume shader for plume materials
    rGroups.m_groups["fwd_transparent"].set_assigner<MaterialPlume>(
            PlumeShader::gen_assign_plume( plume.operator->() ));

    // Use MeshVisualizer for terrain materials
    MeshVisualizer *pVisual = visual.operator->();
    rGroups.m_groups["fwd_opaque"].set_assigner<MaterialTerrain>(
        [pVisual] (ActiveScene& rScene, RenderGroup::Storage_t& rStorage,
                   RenderGroup::ArrayView_t entities)
    {
        for (ActiveEnt ent : entities)
        {
            rStorage.emplace(ent,EntityToDraw{&MeshVisualizer::draw_entity,
                                              pVisual});
        }
    });
}
