/**
 * Open Space Program
 * Copyright © 2019-2021 Open Space Program Project
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

#include "scenarios.h"
#include "enginetest.h"
#include "identifiers.h"

#include "sessions/common.h"
#include "sessions/magnum.h"
#include "sessions/misc.h"
#include "sessions/jolt.h"
#include "sessions/physics.h"
#include "sessions/shapes.h"
#include "sessions/terrain.h"
#include "sessions/universe.h"
#include "sessions/vehicles.h"
#include "sessions/vehicles_machines.h"
#include "sessions/vehicles_prebuilt.h"

#include "MagnumApplication.h"

#include <adera/activescene/vehicles_vb_fn.h>
#include <adera/drawing/CameraController.h>

#include <osp/activescene/basic.h>
#include <osp/core/Resources.h>
#include <osp/tasks/top_utils.h>
#include <osp/util/logging.h>

#include <Magnum/GL/DefaultFramebuffer.h>

#include <algorithm>

#include "scenarios/lander.h"

using namespace adera;
using namespace osp;
using namespace osp::active;

namespace testapp
{

// MaterialIds hints which shaders should be used to draw a DrawEnt
// DrawEnts can be assigned to multiple materials
static constexpr auto   sc_matVisualizer    = draw::MaterialId(0);
static constexpr auto   sc_matFlat          = draw::MaterialId(1);
static constexpr auto   sc_matPhong         = draw::MaterialId(2);
static constexpr int    sc_materialCount    = 4;

static ScenarioMap_t make_scenarios()
{   
    ScenarioMap_t scenarioMap;

    register_stage_enums();

    auto const add_scenario = [&scenarioMap] (std::string_view name, std::string_view desc, SceneSetupFunc_t run)
    {
        scenarioMap.emplace(name, ScenarioOption{desc, run});
    };

    add_scenario("enginetest", "Simple rotating cube scenario without using Pipelines/Tasks",
                 [] (TestApp& rTestApp) -> RendererSetupFunc_t
    {
        // Declares idResources TopDataId variable, obtained from Session m_application.m_data[0].
        //
        // This macro expands to:
        //
        //     auto const [idResources, idMainLoopCtrl] = osp::unpack<2>(rTestApp.m_application.m_data);
        //
        // TopDataIds can be used to access rTestApp.m_topData. The purpose of TopData is to store
        // all data in a safe, type-erased, and addressable manner that can be easily accessed by
        // Tasks. This also allow reserving IDs before instantiation (which cannot be achieved
        // with (smart) pointers alone)
        OSP_DECLARE_GET_DATA_IDS(rTestApp.m_application, TESTAPP_DATA_APPLICATION);
        auto &rResources = top_get<Resources>(rTestApp.m_topData, idResources);

        // Create 1 unnamed session as m_sessions[0], reserving 1 TopDataId as idSceneData
        rTestApp.m_scene.m_sessions.resize(1);
        TopDataId const idSceneData = rTestApp.m_scene.m_sessions[0].acquire_data<1>(rTestApp.m_topData)[0];

        // Create scene, store it in rTestApp.m_topData[idSceneData].
        // enginetest::setup_scene returns an entt::any containing one big EngineTestScene struct
        // containing all the scene data: a spinning cube.
        top_assign<enginetest::EngineTestScene>(rTestApp.m_topData, idSceneData, enginetest::setup_scene(rResources, rTestApp.m_defaultPkg));

        // Called when the MagnumApplication / window is opened, called again if the window is
        // re-opened after its closed. Closing the window completely destructs MagnumApplication
        // and all GPU resources. EngineTestScene will remain untouched in the background.
        RendererSetupFunc_t const setup_renderer = [] (TestApp& rTestApp) -> void
        {
            TopDataId const idSceneData = rTestApp.m_scene.m_sessions[0].m_data[0];
            auto &rScene = top_get<enginetest::EngineTestScene>(rTestApp.m_topData, idSceneData);

            OSP_DECLARE_GET_DATA_IDS(rTestApp.m_magnum,     TESTAPP_DATA_MAGNUM);
            OSP_DECLARE_GET_DATA_IDS(rTestApp.m_windowApp,  TESTAPP_DATA_WINDOW_APP);
            auto &rActiveApp    = top_get< MagnumApplication >      (rTestApp.m_topData, idActiveApp);
            auto &rRenderGl     = top_get< draw::RenderGL >         (rTestApp.m_topData, idRenderGl);
            auto &rUserInput    = top_get< input::UserInputHandler >(rTestApp.m_topData, idUserInput);

            // This creates the renderer actually updates and draws the scene.
            rActiveApp.set_osp_app(enginetest::generate_osp_magnum_app(rScene, rActiveApp, rRenderGl, rUserInput));
        };

        return setup_renderer;
    });

    static constexpr auto sc_gravityForce = Vector3{0.0f, 0.0f, -9.81f};

    add_scenario("physics", "Newton Dynamics integration test scenario",
                 [] (TestApp& rTestApp) -> RendererSetupFunc_t
    {
        #define SCENE_SESSIONS      scene, commonScene, physics, physShapes, droppers, bounds, jolt, joltGravSet, joltGrav, physShapesJolt
        #define RENDERER_SESSIONS   sceneRenderer, magnumScene, cameraCtrl, cameraFree, shVisual, shFlat, shPhong, camThrow, shapeDraw, cursor

        using namespace testapp::scenes;

        auto const  defaultPkg      = rTestApp.m_defaultPkg;
        auto const  application     = rTestApp.m_application;
        auto        & rTopData      = rTestApp.m_topData;

        TopTaskBuilder builder{rTestApp.m_tasks, rTestApp.m_scene.m_edges, rTestApp.m_taskData};

        auto & [SCENE_SESSIONS] = resize_then_unpack<10>(rTestApp.m_scene.m_sessions);

        // Compose together lots of Sessions
        scene           = setup_scene               (builder, rTopData, application);
        commonScene     = setup_common_scene        (builder, rTopData, scene, application, defaultPkg);
        physics         = setup_physics             (builder, rTopData, scene, commonScene);
        physShapes      = setup_phys_shapes         (builder, rTopData, scene, commonScene, physics, sc_matPhong);
        droppers        = setup_droppers            (builder, rTopData, scene, commonScene, physShapes);
        bounds          = setup_bounds              (builder, rTopData, scene, commonScene, physShapes);

        jolt            = setup_jolt              (builder, rTopData, scene, commonScene, physics);
        joltGravSet     = setup_jolt_factors      (builder, rTopData);
        joltGrav        = setup_jolt_force_accel  (builder, rTopData, jolt, joltGravSet, sc_gravityForce);
        physShapesJolt  = setup_phys_shapes_jolt  (builder, rTopData, commonScene, physics, physShapes, jolt, joltGravSet);

        add_floor(rTopData, physShapes, sc_matVisualizer, defaultPkg, 4);

        RendererSetupFunc_t const setup_renderer = [] (TestApp& rTestApp) -> void
        {
            auto const  application     = rTestApp.m_application;
            auto const  windowApp       = rTestApp.m_windowApp;
            auto const  magnum          = rTestApp.m_magnum;
            auto const  defaultPkg      = rTestApp.m_defaultPkg;
            auto        & rTopData      = rTestApp.m_topData;

            TopTaskBuilder builder{rTestApp.m_tasks, rTestApp.m_renderer.m_edges, rTestApp.m_taskData};

            auto & [SCENE_SESSIONS] = unpack<10>(rTestApp.m_scene.m_sessions);
            auto & [RENDERER_SESSIONS] = resize_then_unpack<10>(rTestApp.m_renderer.m_sessions);

            sceneRenderer   = setup_scene_renderer      (builder, rTopData, application, windowApp, commonScene);
            create_materials(rTopData, sceneRenderer, sc_materialCount);

            magnumScene     = setup_magnum_scene        (builder, rTopData, application, windowApp, sceneRenderer, magnum, scene, commonScene);
            cameraCtrl      = setup_camera_ctrl         (builder, rTopData, windowApp, sceneRenderer, magnumScene);
            cameraFree      = setup_camera_free         (builder, rTopData, windowApp, scene, cameraCtrl);
            shVisual        = setup_shader_visualizer   (builder, rTopData, windowApp, sceneRenderer, magnum, magnumScene, sc_matVisualizer);
            shFlat          = setup_shader_flat         (builder, rTopData, windowApp, sceneRenderer, magnum, magnumScene, sc_matFlat);
            shPhong         = setup_shader_phong        (builder, rTopData, windowApp, sceneRenderer, magnum, magnumScene, sc_matPhong);
            camThrow        = setup_thrower             (builder, rTopData, windowApp, cameraCtrl, physShapes);
            shapeDraw       = setup_phys_shapes_draw    (builder, rTopData, windowApp, sceneRenderer, commonScene, physics, physShapes);
            cursor          = setup_cursor              (builder, rTopData, application, sceneRenderer, cameraCtrl, commonScene, sc_matFlat, rTestApp.m_defaultPkg);

            setup_magnum_draw(rTestApp, scene, sceneRenderer, magnumScene);
        };

        #undef SCENE_SESSIONS
        #undef RENDERER_SESSIONS

        return setup_renderer;
    });

    add_scenario("vehicles", "Physics scenario but with Vehicles",
                 [] (TestApp& rTestApp) -> RendererSetupFunc_t
    {
        #define SCENE_SESSIONS      scene, commonScene, physics, physShapes, droppers, bounds, jolt, joltGravSet, joltGrav, physShapesJolt, \
                                    prefabs, parts, vehicleSpawn, signalsFloat, \
                                    vehicleSpawnVB, vehicleSpawnRgd, vehicleSpawnJolt, \
                                    testVehicles, machRocket, machRcsDriver, joltRocketSet, rocketsJolt
        #define RENDERER_SESSIONS   sceneRenderer, magnumScene, cameraCtrl, shVisual, shFlat, shPhong, camThrow, shapeDraw, cursor, \
                                    prefabDraw, vehicleDraw, vehicleCtrl, cameraVehicle, thrustIndicator

        using namespace testapp::scenes;

        auto const  defaultPkg      = rTestApp.m_defaultPkg;
        auto const  application     = rTestApp.m_application;
        auto        & rTopData      = rTestApp.m_topData;

        TopTaskBuilder builder{rTestApp.m_tasks, rTestApp.m_scene.m_edges, rTestApp.m_taskData};

        auto & [SCENE_SESSIONS] = resize_then_unpack<22>(rTestApp.m_scene.m_sessions);

        scene            = setup_scene               (builder, rTopData, application);
        commonScene      = setup_common_scene        (builder, rTopData, scene, application, defaultPkg);
        physics          = setup_physics             (builder, rTopData, scene, commonScene);
        physShapes       = setup_phys_shapes         (builder, rTopData, scene, commonScene, physics, sc_matPhong);
        droppers         = setup_droppers            (builder, rTopData, scene, commonScene, physShapes);
        bounds           = setup_bounds              (builder, rTopData, scene, commonScene, physShapes);

        prefabs          = setup_prefabs             (builder, rTopData, application, scene, commonScene, physics);
        parts            = setup_parts               (builder, rTopData, application, scene);
        signalsFloat     = setup_signals_float       (builder, rTopData, scene, parts);
        vehicleSpawn     = setup_vehicle_spawn       (builder, rTopData, scene);
        vehicleSpawnVB   = setup_vehicle_spawn_vb    (builder, rTopData, application, scene, commonScene, prefabs, parts, vehicleSpawn, signalsFloat);
        testVehicles     = setup_prebuilt_vehicles   (builder, rTopData, application, scene);

        machRocket       = setup_mach_rocket         (builder, rTopData, scene, parts, signalsFloat);
        machRcsDriver    = setup_mach_rcsdriver      (builder, rTopData, scene, parts, signalsFloat);

        jolt             = setup_jolt              (builder, rTopData, scene, commonScene, physics);
        joltGravSet      = setup_jolt_factors      (builder, rTopData);
        joltGrav         = setup_jolt_force_accel  (builder, rTopData, jolt, joltGravSet, sc_gravityForce);
        physShapesJolt   = setup_phys_shapes_jolt  (builder, rTopData, commonScene, physics, physShapes, jolt, joltGravSet);
        vehicleSpawnJolt = setup_vehicle_spawn_jolt(builder, rTopData, application, commonScene, physics, prefabs, parts, vehicleSpawn, jolt);
        joltRocketSet    = setup_jolt_factors      (builder, rTopData);
        rocketsJolt      = setup_rocket_thrust_jolt(builder, rTopData, scene, commonScene, physics, prefabs, parts, signalsFloat, jolt, joltRocketSet);

        OSP_DECLARE_GET_DATA_IDS(vehicleSpawn,   TESTAPP_DATA_VEHICLE_SPAWN);
        OSP_DECLARE_GET_DATA_IDS(vehicleSpawnVB, TESTAPP_DATA_VEHICLE_SPAWN_VB);
        OSP_DECLARE_GET_DATA_IDS(testVehicles,   TESTAPP_DATA_TEST_VEHICLES);

        auto &rVehicleSpawn     = top_get<ACtxVehicleSpawn>     (rTopData, idVehicleSpawn);
        auto &rVehicleSpawnVB   = top_get<ACtxVehicleSpawnVB>   (rTopData, idVehicleSpawnVB);
        auto &rPrebuiltVehicles = top_get<PrebuiltVehicles>     (rTopData, idPrebuiltVehicles);

        for (int i = 0; i < 10; ++i)
        {
            rVehicleSpawn.spawnRequest.push_back(
            {
               .position = {float(i - 2) * 8.0f, 30.0f, 10.0f},
               .velocity = {0.0, 0.0f, 50.0f * float(i)},
               .rotation = {}
            });
            rVehicleSpawnVB.dataVB.push_back(rPrebuiltVehicles[gc_pbvSimpleCommandServiceModule].get());
        }

        add_floor(rTopData, physShapes, sc_matVisualizer, defaultPkg, 4);

        RendererSetupFunc_t const setup_renderer = [] (TestApp& rTestApp)
        {
            auto const  application     = rTestApp.m_application;
            auto const  windowApp       = rTestApp.m_windowApp;
            auto const  magnum          = rTestApp.m_magnum;
            auto const  defaultPkg      = rTestApp.m_defaultPkg;
            auto        & rTopData      = rTestApp.m_topData;

            TopTaskBuilder builder{rTestApp.m_tasks, rTestApp.m_renderer.m_edges, rTestApp.m_taskData};

            auto & [SCENE_SESSIONS] = unpack<22>(rTestApp.m_scene.m_sessions);
            auto & [RENDERER_SESSIONS] = resize_then_unpack<14>(rTestApp.m_renderer.m_sessions);

            sceneRenderer   = setup_scene_renderer      (builder, rTopData, application, windowApp, commonScene);
            create_materials(rTopData, sceneRenderer, sc_materialCount);

            magnumScene     = setup_magnum_scene        (builder, rTopData, application, windowApp, sceneRenderer, magnum, scene, commonScene);
            cameraCtrl      = setup_camera_ctrl         (builder, rTopData, windowApp, sceneRenderer, magnumScene);
            shVisual        = setup_shader_visualizer   (builder, rTopData, windowApp, sceneRenderer, magnum, magnumScene, sc_matVisualizer);
            shFlat          = setup_shader_flat         (builder, rTopData, windowApp, sceneRenderer, magnum, magnumScene, sc_matFlat);
            shPhong         = setup_shader_phong        (builder, rTopData, windowApp, sceneRenderer, magnum, magnumScene, sc_matPhong);
            camThrow        = setup_thrower             (builder, rTopData, windowApp, cameraCtrl, physShapes);
            shapeDraw       = setup_phys_shapes_draw    (builder, rTopData, windowApp, sceneRenderer, commonScene, physics, physShapes);
            cursor          = setup_cursor              (builder, rTopData, application, sceneRenderer, cameraCtrl, commonScene, sc_matFlat, rTestApp.m_defaultPkg);
            prefabDraw      = setup_prefab_draw         (builder, rTopData, application, windowApp, sceneRenderer, commonScene, prefabs, sc_matPhong);
            vehicleDraw     = setup_vehicle_spawn_draw  (builder, rTopData, sceneRenderer, vehicleSpawn);
            vehicleCtrl     = setup_vehicle_control     (builder, rTopData, windowApp, scene, parts, signalsFloat);
            cameraVehicle   = setup_camera_vehicle      (builder, rTopData, windowApp, scene, sceneRenderer, commonScene, physics, parts, cameraCtrl, vehicleCtrl);
            thrustIndicator = setup_thrust_indicators   (builder, rTopData, application, windowApp, commonScene, parts, signalsFloat, sceneRenderer, defaultPkg, sc_matFlat);

            setup_magnum_draw(rTestApp, scene, sceneRenderer, magnumScene);
        };

        #undef SCENE_SESSIONS
        #undef RENDERER_SESSIONS

        return setup_renderer;
    });

    add_scenario("terrain", "Planet terrain mesh test",
                 [] (TestApp& rTestApp) -> RendererSetupFunc_t
    {
        #define SCENE_SESSIONS      scene, commonScene, physics, physShapes, terrain, terrainIco, terrainSubdiv
        #define RENDERER_SESSIONS   sceneRenderer, magnumScene, cameraCtrl, cameraFree, shVisual, shFlat, shPhong, camThrow, shapeDraw, cursor, terrainDraw

        using namespace testapp::scenes;

        auto const  defaultPkg      = rTestApp.m_defaultPkg;
        auto const  application     = rTestApp.m_application;
        auto        & rTopData      = rTestApp.m_topData;

        TopTaskBuilder builder{rTestApp.m_tasks, rTestApp.m_scene.m_edges, rTestApp.m_taskData};

        auto & [SCENE_SESSIONS] = resize_then_unpack<7>(rTestApp.m_scene.m_sessions);

        scene           = setup_scene               (builder, rTopData, application);
        commonScene     = setup_common_scene        (builder, rTopData, scene, application, defaultPkg);
        physics         = setup_physics             (builder, rTopData, scene, commonScene);
        physShapes      = setup_phys_shapes         (builder, rTopData, scene, commonScene, physics, sc_matPhong);
        terrain         = setup_terrain             (builder, rTopData, scene);
        terrainIco      = setup_terrain_icosahedron (builder, rTopData, terrain);
        terrainSubdiv   = setup_terrain_subdiv_dist (builder, rTopData, scene, terrain, terrainIco);

        initialize_ico_terrain(rTopData, terrain, terrainIco, {
            .radius                 = 50.0,
            .height                 = 5.0,
            .skelPrecision          = 10, // 2^10 units = 1024 units = 1 meter
            .skelMaxSubdivLevels    = 7,
            .chunkSubdivLevels      = 4
        });

        RendererSetupFunc_t const setup_renderer = [] (TestApp& rTestApp) -> void
        {
            auto const  application     = rTestApp.m_application;
            auto const  windowApp       = rTestApp.m_windowApp;
            auto const  magnum          = rTestApp.m_magnum;
            auto const  defaultPkg      = rTestApp.m_defaultPkg;
            auto        & rTopData      = rTestApp.m_topData;

            TopTaskBuilder builder{rTestApp.m_tasks, rTestApp.m_renderer.m_edges, rTestApp.m_taskData};

            auto & [SCENE_SESSIONS] = unpack<7>(rTestApp.m_scene.m_sessions);
            auto & [RENDERER_SESSIONS] = resize_then_unpack<11>(rTestApp.m_renderer.m_sessions);

            sceneRenderer   = setup_scene_renderer      (builder, rTopData, application, windowApp, commonScene);
            create_materials(rTopData, sceneRenderer, sc_materialCount);

            magnumScene     = setup_magnum_scene        (builder, rTopData, application, windowApp, sceneRenderer, magnum, scene, commonScene);
            cameraCtrl      = setup_camera_ctrl         (builder, rTopData, windowApp, sceneRenderer, magnumScene);
            cameraFree      = setup_camera_free         (builder, rTopData, windowApp, scene, cameraCtrl);
            shVisual        = setup_shader_visualizer   (builder, rTopData, windowApp, sceneRenderer, magnum, magnumScene, sc_matVisualizer);
            shFlat          = setup_shader_flat         (builder, rTopData, windowApp, sceneRenderer, magnum, magnumScene, sc_matFlat);
            shPhong         = setup_shader_phong        (builder, rTopData, windowApp, sceneRenderer, magnum, magnumScene, sc_matPhong);
            shapeDraw       = setup_phys_shapes_draw    (builder, rTopData, windowApp, sceneRenderer, commonScene, physics, physShapes);
            cursor          = setup_cursor              (builder, rTopData, application, sceneRenderer, cameraCtrl, commonScene, sc_matFlat, rTestApp.m_defaultPkg);
            terrainDraw     = setup_terrain_debug_draw  (builder, rTopData, windowApp, sceneRenderer, cameraCtrl, commonScene, terrain, terrainIco, sc_matFlat);

            OSP_DECLARE_GET_DATA_IDS(cameraCtrl,   TESTAPP_DATA_CAMERA_CTRL);

            auto &rCamCtrl = top_get<ACtxCameraController>(rTopData, idCamCtrl);
            rCamCtrl.m_target = Vector3(0.0f, 0.0f, 50.0f);
            rCamCtrl.m_orbitDistanceMin = 1.0f;
            rCamCtrl.m_moveSpeed = 0.5f;

            setup_magnum_draw(rTestApp, scene, sceneRenderer, magnumScene);
        };

        #undef SCENE_SESSIONS
        #undef RENDERER_SESSIONS

        return setup_renderer;
    });

    add_scenario("universe", "Universe test scenario with very unrealistic planets",
                 [] (TestApp& rTestApp) -> RendererSetupFunc_t
    {
        #define SCENE_SESSIONS      scene, commonScene, physics, physShapes, droppers, bounds, jolt, joltGravSet, joltGrav, physShapesJolt, uniCore, uniScnFrame, uniTestPlanets
        #define RENDERER_SESSIONS   sceneRenderer, magnumScene, cameraCtrl, cameraFree, shVisual, shFlat, shPhong, camThrow, shapeDraw, cursor, planetsDraw

        using namespace testapp::scenes;

        auto const  defaultPkg      = rTestApp.m_defaultPkg;
        auto const  application     = rTestApp.m_application;
        auto        & rTopData      = rTestApp.m_topData;

        TopTaskBuilder builder{rTestApp.m_tasks, rTestApp.m_scene.m_edges, rTestApp.m_taskData};

        auto & [SCENE_SESSIONS] = resize_then_unpack<13>(rTestApp.m_scene.m_sessions);

        // Compose together lots of Sessions
        scene           = setup_scene               (builder, rTopData, application);
        commonScene     = setup_common_scene        (builder, rTopData, scene, application, defaultPkg);
        physics         = setup_physics             (builder, rTopData, scene, commonScene);
        physShapes      = setup_phys_shapes         (builder, rTopData, scene, commonScene, physics, sc_matPhong);
        droppers        = setup_droppers            (builder, rTopData, scene, commonScene, physShapes);
        bounds          = setup_bounds              (builder, rTopData, scene, commonScene, physShapes);

        jolt            = setup_jolt              (builder, rTopData, scene, commonScene, physics);
        joltGravSet     = setup_jolt_factors      (builder, rTopData);
        joltGrav        = setup_jolt_force_accel  (builder, rTopData, jolt, joltGravSet, Vector3{0.0f, 0.0f, -9.81f});
        physShapesJolt  = setup_phys_shapes_jolt  (builder, rTopData, commonScene, physics, physShapes, jolt, joltGravSet);

        auto const tgApp = application.get_pipelines< PlApplication >();

        uniCore         = setup_uni_core            (builder, rTopData, tgApp.mainLoop);
        uniScnFrame     = setup_uni_sceneframe      (builder, rTopData, uniCore);
        uniTestPlanets  = setup_uni_testplanets     (builder, rTopData, uniCore, uniScnFrame);

        add_floor(rTopData, physShapes, sc_matVisualizer, defaultPkg, 0);

        RendererSetupFunc_t const setup_renderer = [] (TestApp& rTestApp)
        {
            auto const  application     = rTestApp.m_application;
            auto const  windowApp       = rTestApp.m_windowApp;
            auto const  magnum          = rTestApp.m_magnum;
            auto const  defaultPkg      = rTestApp.m_defaultPkg;
            auto        & rTopData      = rTestApp.m_topData;

            TopTaskBuilder builder{rTestApp.m_tasks, rTestApp.m_renderer.m_edges, rTestApp.m_taskData};

            auto & [SCENE_SESSIONS] = unpack<13>(rTestApp.m_scene.m_sessions);
            auto & [RENDERER_SESSIONS] = resize_then_unpack<11>(rTestApp.m_renderer.m_sessions);

            sceneRenderer   = setup_scene_renderer      (builder, rTopData, application, windowApp, commonScene);
            create_materials(rTopData, sceneRenderer, sc_materialCount);

            magnumScene     = setup_magnum_scene        (builder, rTopData, application, windowApp, sceneRenderer, magnum, scene, commonScene);
            cameraCtrl      = setup_camera_ctrl         (builder, rTopData, windowApp, sceneRenderer, magnumScene);
            cameraFree      = setup_camera_free         (builder, rTopData, windowApp, scene, cameraCtrl);
            shVisual        = setup_shader_visualizer   (builder, rTopData, windowApp, sceneRenderer, magnum, magnumScene, sc_matVisualizer);
            shFlat          = setup_shader_flat         (builder, rTopData, windowApp, sceneRenderer, magnum, magnumScene, sc_matFlat);
            shPhong         = setup_shader_phong        (builder, rTopData, windowApp, sceneRenderer, magnum, magnumScene, sc_matPhong);
            camThrow        = setup_thrower             (builder, rTopData, windowApp, cameraCtrl, physShapes);
            shapeDraw       = setup_phys_shapes_draw    (builder, rTopData, windowApp, sceneRenderer, commonScene, physics, physShapes);
            cursor          = setup_cursor              (builder, rTopData, application, sceneRenderer, cameraCtrl, commonScene, sc_matFlat, rTestApp.m_defaultPkg);
            planetsDraw     = setup_testplanets_draw    (builder, rTopData, windowApp, sceneRenderer, cameraCtrl, commonScene, uniCore, uniScnFrame, uniTestPlanets, sc_matVisualizer, sc_matFlat);

            setup_magnum_draw(rTestApp, scene, sceneRenderer, magnumScene);
        };

        #undef SCENE_SESSIONS
        #undef RENDERER_SESSIONS

        return setup_renderer;
    });

    using lander::setup_lander_scenario;

    add_scenario("lander", "Lander simulation game", setup_lander_scenario);

    return scenarioMap;
}

ScenarioMap_t const& scenarios()
{
    static ScenarioMap_t s_scenarioMap = make_scenarios();
    return s_scenarioMap;
}

} // namespace testapp


