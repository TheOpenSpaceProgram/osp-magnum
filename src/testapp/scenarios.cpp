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

#include <adera_app/feature_interfaces.h>
#include <adera_app/features/common.h>
#include <adera_app/features/misc.h>
#include <adera_app/features/jolt.h>
#include <adera_app/features/physics.h>
#include <adera_app/features/shapes.h>
#include <adera_app/features/terrain.h>
#include <adera_app/features/universe.h>
#include <adera_app/features/vehicles.h>
#include <adera_app/features/vehicles_machines.h>
#include <adera_app/features/vehicles_prebuilt.h>

#include <adera_app/application.h>
#include <adera/activescene/vehicles_vb_fn.h>
#include <adera/drawing/CameraController.h>

#include <osp/activescene/basic.h>
#include <osp/util/logging.h>
#include <osp/framework/framework.h>

using namespace adera;
using namespace osp;
using namespace osp::fw;
using namespace osp::active;

using namespace ftr_inter;

namespace testapp
{

// MaterialIds hints which shaders should be used to draw a DrawEnt
// DrawEnts can be assigned to multiple materials
static constexpr auto   sc_matVisualizer    = draw::MaterialId(0);
static constexpr auto   sc_matFlat          = draw::MaterialId(1);
static constexpr auto   sc_matPhong         = draw::MaterialId(2);
static constexpr int    sc_materialCount    = 4;

/**
 * Enginetest itself doesn't depend on the framework, but we need to store it somewhere.
 */
FeatureDef const ftrEngineTest = feature_def("EngineTest", [] (FeatureBuilder& rFB, Implement<FIEngineTest> engineTest, DependOn<FIMainApp> mainApp, entt::any data)
{
    auto &rResources = rFB.data_get<Resources>(mainApp.di.resources);
    rFB.data(engineTest.di.bigStruct) = enginetest::setup_scene(rResources, entt::any_cast<PkgId>(data));
});

static ScenarioMap_t make_scenarios()
{   
    ScenarioMap_t scenarioMap;


    auto const add_scenario = [&scenarioMap] (ScenarioOption scenario)
    {
        scenarioMap.emplace(scenario.name, scenario);
    };

    add_scenario({
        .name        = "enginetest",
        .brief       = "Simple rotating cube scenario without using framework",
        .description = "Uses a single large struct to store a simple OSP scene. This demonstrates "
                       "how OSP components are usable on their own as separate types and "
                       "functions, independent of any particular framework.\n"
                       "Controls:\n"
                       "* [WASD]            - Move camera\n"
                       "* [QE]              - Move camera up/down\n"
                       "* [Drag MouseRight] - Orbit camera\n",
        .loadFunc = [] (TestApp& rTestApp)
    {
        auto        &rFw      = rTestApp.m_framework;
        auto  const mainApp   = rFw.get_interface<FIMainApp>  (rTestApp.m_mainContext);
        auto        &rAppCtxs = rFw.data_get<adera::AppContexts&>(mainApp.di.appContexts);


        rAppCtxs.scene  = rFw.m_contextIds.create();
        ContextBuilder  sceneCB { rAppCtxs.scene,  {rTestApp.m_mainContext}, rFw };
        sceneCB.add_feature(ftrEngineTest, rTestApp.m_defaultPkg);
        LGRN_ASSERTM(sceneCB.m_errors.empty(), "Error adding engine test feature");
        ContextBuilder::finalize(std::move(sceneCB));
    }});

    static constexpr auto sc_gravityForce = Vector3{0.0f, 0.0f, -9.81f};

    add_scenario({
        .name        = "physics",
        .brief       = "Jolt Physics engine integration test",
        .description = "Controls:\n"
                       "* [WASD]            - Move camera\n"
                       "* [QE]              - Move camera up/down\n"
                       "* [Drag MouseRight] - Orbit camera\n"
                       "* [Space]           - Throw spheres\n",
        .loadFunc = [] (TestApp& rTestApp)
    {
        auto        &rFW      = rTestApp.m_framework;
        auto  const mainApp   = rFW.get_interface<FIMainApp>  (rTestApp.m_mainContext);
        auto        &rAppCtxs = rFW.data_get<adera::AppContexts&>(mainApp.di.appContexts);

        rAppCtxs.scene = rFW.m_contextIds.create();
        ContextBuilder  sceneCB { rAppCtxs.scene, {rTestApp.m_mainContext}, rFW };
        sceneCB.add_feature(ftrScene);
        sceneCB.add_feature(ftrCommonScene, rTestApp.m_defaultPkg);
        sceneCB.add_feature(ftrPhysics);
        sceneCB.add_feature(ftrPhysicsShapes, osp::draw::MaterialId{0});
        sceneCB.add_feature(ftrDroppers);
        sceneCB.add_feature(ftrBounds);

        sceneCB.add_feature(ftrJolt);
        sceneCB.add_feature(ftrJoltForces);
        sceneCB.add_feature(ftrJoltAccel, sc_gravityForce);
        sceneCB.add_feature(ftrPhysicsShapesJolt);

        ContextBuilder::finalize(std::move(sceneCB));

        add_floor(rFW, rAppCtxs.scene, osp::draw::MaterialId{0}, rTestApp.m_defaultPkg, 4);
    }});

     #if 0


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

        scene            = setup_scene               (builder, rapplication);
        commonScene      = setup_common_scene        (builder, rscene, application, defaultPkg);
        physics          = setup_physics             (builder, rscene, commonScene);
        physShapes       = setup_phys_shapes         (builder, rscene, commonScene, physics, sc_matPhong);
        droppers         = setup_droppers            (builder, rscene, commonScene, physShapes);
        bounds           = setup_bounds              (builder, rscene, commonScene, physShapes);

        prefabs          = setup_prefabs             (builder, rapplication, scene, commonScene, physics);
        parts            = setup_parts               (builder, rapplication, scene);
        signalsFloat     = setup_signals_float       (builder, rscene, parts);
        vehicleSpawn     = setup_vehicle_spawn       (builder, rscene);
        vehicleSpawnVB   = setup_vehicle_spawn_vb    (builder, rapplication, scene, commonScene, prefabs, parts, vehicleSpawn, signalsFloat);
        testVehicles     = setup_prebuilt_vehicles   (builder, rapplication, scene);

        machRocket       = setup_mach_rocket         (builder, rscene, parts, signalsFloat);
        machRcsDriver    = setup_mach_rcsdriver      (builder, rscene, parts, signalsFloat);

        jolt             = setup_jolt              (builder, rscene, commonScene, physics);
        joltGravSet      = setup_jolt_factors      (builder, rTopData);
        joltGrav         = setup_jolt_force_accel  (builder, rjolt, joltGravSet, sc_gravityForce);
        physShapesJolt   = setup_phys_shapes_jolt  (builder, rcommonScene, physics, physShapes, jolt, joltGravSet);
        vehicleSpawnJolt = setup_vehicle_spawn_jolt(builder, rapplication, commonScene, physics, prefabs, parts, vehicleSpawn, jolt);
        joltRocketSet    = setup_jolt_factors      (builder, rTopData);
        rocketsJolt      = setup_rocket_thrust_jolt(builder, rscene, commonScene, physics, prefabs, parts, signalsFloat, jolt, joltRocketSet);

        OSP_DECLARE_GET_DATA_IDS(vehicleSpawn,   TESTAPP_DATA_VEHICLE_SPAWN);
        OSP_DECLARE_GET_DATA_IDS(vehicleSpawnVB, TESTAPP_DATA_VEHICLE_SPAWN_VB);
        OSP_DECLARE_GET_DATA_IDS(testVehicles,   TESTAPP_DATA_TEST_VEHICLES);

        auto &rVehicleSpawn     = rFB.data_get<ACtxVehicleSpawn>     (rvhclSpawn.di.vehicleSpawn);
        auto &rVehicleSpawnVB   = rFB.data_get<ACtxVehicleSpawnVB>   (rvhclSpawn.di.vehicleSpawnVB);
        auto &rPrebuiltVehicles = rFB.data_get<PrebuiltVehicles>     (ridPrebuiltVehicles);

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

        add_floor(rphysShapes, sc_matVisualizer, defaultPkg, 4);

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

            sceneRenderer   = setup_scene_renderer      (builder, rapplication, windowApp, commonScene);
            create_materials(rsceneRenderer, sc_materialCount);

            magnumScene     = setup_magnum_scene        (builder, rapplication, windowApp, sceneRenderer, magnum, scene, commonScene);
            cameraCtrl      = setup_camera_ctrl         (builder, rwindowApp, sceneRenderer, magnumScene);
            shVisual        = setup_shader_visualizer   (builder, rwindowApp, sceneRenderer, magnum, magnumScene, sc_matVisualizer);
            shFlat          = setup_shader_flat         (builder, rwindowApp, sceneRenderer, magnum, magnumScene, sc_matFlat);
            shPhong         = setup_shader_phong        (builder, rwindowApp, sceneRenderer, magnum, magnumScene, sc_matPhong);
            camThrow        = setup_thrower             (builder, rwindowApp, cameraCtrl, physShapes);
            shapeDraw       = setup_phys_shapes_draw    (builder, rwindowApp, sceneRenderer, commonScene, physics, physShapes);
            cursor          = setup_cursor              (builder, rapplication, sceneRenderer, cameraCtrl, commonScene, sc_matFlat, rTestApp.m_defaultPkg);
            prefabDraw      = setup_prefab_draw         (builder, rapplication, windowApp, sceneRenderer, commonScene, prefabs, sc_matPhong);
            vehicleDraw     = setup_vehicle_spawn_draw  (builder, rsceneRenderer, vehicleSpawn);
            vehicleCtrl     = setup_vehicle_control     (builder, rwindowApp, scene, parts, signalsFloat);
            cameraVehicle   = setup_camera_vehicle      (builder, rwindowApp, scene, sceneRenderer, commonScene, physics, parts, cameraCtrl, vehicleCtrl);
            thrustIndicator = setup_thrust_indicators   (builder, rapplication, windowApp, commonScene, parts, signalsFloat, sceneRenderer, defaultPkg, sc_matFlat);

            setup_magnum_draw(rTestApp, scene, sceneRenderer, magnumScene);
        };

        #undef SCENE_SESSIONS
        #undef RENDERER_SESSIONS

        return setup_renderer;
    });

    add_scenario("terrain", "Planet terrain mesh test (Earth-sized planet)",
                 [] (TestApp& rTestApp) -> RendererSetupFunc_t
    {
        #define SCENE_SESSIONS      scene, commonScene, physics, physShapes, terrain, terrainIco, terrainSubdiv
        #define RENDERER_SESSIONS   sceneRenderer, magnumScene, cameraCtrl, shVisual, shFlat, shPhong, camThrow, shapeDraw, cursor, terrainDraw, terrainDrawGL

        using namespace testapp::scenes;

        auto const  defaultPkg      = rTestApp.m_defaultPkg;
        auto const  application     = rTestApp.m_application;
        auto        & rTopData      = rTestApp.m_topData;

        TopTaskBuilder builder{rTestApp.m_tasks, rTestApp.m_scene.m_edges, rTestApp.m_taskData};

        auto & [SCENE_SESSIONS] = resize_then_unpack<7>(rTestApp.m_scene.m_sessions);

        scene           = setup_scene               (builder, rapplication);
        commonScene     = setup_common_scene        (builder, rscene, application, defaultPkg);
        physics         = setup_physics             (builder, rscene, commonScene);
        physShapes      = setup_phys_shapes         (builder, rscene, commonScene, physics, sc_matPhong);
        terrain         = setup_terrain             (builder, rscene, commonScene);
        terrainIco      = setup_terrain_icosahedron (builder, rterrain);
        terrainSubdiv   = setup_terrain_subdiv_dist (builder, rscene, terrain, terrainIco);

        OSP_DECLARE_GET_DATA_IDS(terrain,    TESTAPP_DATA_TERRAIN);
        auto &rTerrain = rFB.data_get<ACtxTerrain>(rterrain.di.terrain);
        auto &rTerrainFrame = rFB.data_get<ACtxTerrainFrame>(rterrain.di.terrainFrame);

        constexpr std::uint64_t c_earthRadius = 6371000;

        initialize_ico_terrain(rterrain, terrainIco, {
            .radius                 = double(c_earthRadius),
            .height                 = 20000.0,   // Height between Mariana Trench and Mount Everest
            .skelPrecision          = 10,        // 2^10 units = 1024 units = 1 meter
            .skelMaxSubdivLevels    = 19,
            .chunkSubdivLevels      = 4
        });

        // Set scene position relative to planet to be just on the surface
        rTerrainFrame.position = Vector3l{0,0,c_earthRadius} * 1024;

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

            sceneRenderer   = setup_scene_renderer      (builder, rapplication, windowApp, commonScene);
            create_materials(rsceneRenderer, sc_materialCount);

            magnumScene     = setup_magnum_scene        (builder, rapplication, windowApp, sceneRenderer, magnum, scene, commonScene);
            cameraCtrl      = setup_camera_ctrl         (builder, rwindowApp, sceneRenderer, magnumScene);
            shVisual        = setup_shader_visualizer   (builder, rwindowApp, sceneRenderer, magnum, magnumScene, sc_matVisualizer);
            shFlat          = setup_shader_flat         (builder, rwindowApp, sceneRenderer, magnum, magnumScene, sc_matFlat);
            shPhong         = setup_shader_phong        (builder, rwindowApp, sceneRenderer, magnum, magnumScene, sc_matPhong);
            shapeDraw       = setup_phys_shapes_draw    (builder, rwindowApp, sceneRenderer, commonScene, physics, physShapes);
            cursor          = setup_cursor              (builder, rapplication, sceneRenderer, cameraCtrl, commonScene, sc_matFlat, rTestApp.m_defaultPkg);
            terrainDraw     = setup_terrain_debug_draw  (builder, rscene, sceneRenderer, cameraCtrl, commonScene, terrain, terrainIco, sc_matVisualizer);
            terrainDrawGL   = setup_terrain_draw_magnum (builder, rwindowApp, sceneRenderer, magnum, magnumScene, terrain);

            OSP_DECLARE_GET_DATA_IDS(cameraCtrl,    TESTAPP_DATA_CAMERA_CTRL);

            auto &rCamCtrl = rFB.data_get<ACtxCameraController>(rcamCtrl.di.camCtrl);
            rCamCtrl.m_target = Vector3(0.0f, 0.0f, 0.0f);
            rCamCtrl.m_orbitDistanceMin = 1.0f;
            rCamCtrl.m_moveSpeed = 0.5f;

            setup_magnum_draw(rTestApp, scene, sceneRenderer, magnumScene);
        };

        #undef SCENE_SESSIONS
        #undef RENDERER_SESSIONS

        return setup_renderer;
    });


    add_scenario("terrain_small", "Planet terrain mesh test (100m radius planet)",
                 [] (TestApp& rTestApp) -> RendererSetupFunc_t
    {
        #define SCENE_SESSIONS      scene, commonScene, physics, physShapes, terrain, terrainIco, terrainSubdiv
        #define RENDERER_SESSIONS   sceneRenderer, magnumScene, cameraCtrl, shVisual, shFlat, shPhong, camThrow, shapeDraw, cursor, terrainDraw, terrainDrawGL

        using namespace testapp::scenes;

        auto const  defaultPkg      = rTestApp.m_defaultPkg;
        auto const  application     = rTestApp.m_application;
        auto        & rTopData      = rTestApp.m_topData;

        TopTaskBuilder builder{rTestApp.m_tasks, rTestApp.m_scene.m_edges, rTestApp.m_taskData};

        auto & [SCENE_SESSIONS] = resize_then_unpack<7>(rTestApp.m_scene.m_sessions);

        scene           = setup_scene               (builder, rapplication);
        commonScene     = setup_common_scene        (builder, rscene, application, defaultPkg);
        physics         = setup_physics             (builder, rscene, commonScene);
        physShapes      = setup_phys_shapes         (builder, rscene, commonScene, physics, sc_matPhong);
        terrain         = setup_terrain             (builder, rscene, commonScene);
        terrainIco      = setup_terrain_icosahedron (builder, rterrain);
        terrainSubdiv   = setup_terrain_subdiv_dist (builder, rscene, terrain, terrainIco);

        OSP_DECLARE_GET_DATA_IDS(terrain,    TESTAPP_DATA_TERRAIN);
        auto &rTerrain = rFB.data_get<ACtxTerrain>(rterrain.di.terrain);
        auto &rTerrainFrame = rFB.data_get<ACtxTerrainFrame>(rterrain.di.terrainFrame);

        initialize_ico_terrain(rterrain, terrainIco, {
            .radius                 = 100.0,
            .height                 = 2.0,
            .skelPrecision          = 10, // 2^10 units = 1024 units = 1 meter
            .skelMaxSubdivLevels    = 5,
            .chunkSubdivLevels      = 4
        });

        // Position on surface
        rTerrainFrame.position = Vector3l{0,0,100} * 1024;

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

            sceneRenderer   = setup_scene_renderer      (builder, rapplication, windowApp, commonScene);
            create_materials(rsceneRenderer, sc_materialCount);

            magnumScene     = setup_magnum_scene        (builder, rapplication, windowApp, sceneRenderer, magnum, scene, commonScene);
            cameraCtrl      = setup_camera_ctrl         (builder, rwindowApp, sceneRenderer, magnumScene);
            shVisual        = setup_shader_visualizer   (builder, rwindowApp, sceneRenderer, magnum, magnumScene, sc_matVisualizer);
            shFlat          = setup_shader_flat         (builder, rwindowApp, sceneRenderer, magnum, magnumScene, sc_matFlat);
            shPhong         = setup_shader_phong        (builder, rwindowApp, sceneRenderer, magnum, magnumScene, sc_matPhong);
            shapeDraw       = setup_phys_shapes_draw    (builder, rwindowApp, sceneRenderer, commonScene, physics, physShapes);
            cursor          = setup_cursor              (builder, rapplication, sceneRenderer, cameraCtrl, commonScene, sc_matFlat, rTestApp.m_defaultPkg);
            terrainDraw     = setup_terrain_debug_draw  (builder, rscene, sceneRenderer, cameraCtrl, commonScene, terrain, terrainIco, sc_matVisualizer);
            terrainDrawGL   = setup_terrain_draw_magnum (builder, rwindowApp, sceneRenderer, magnum, magnumScene, terrain);

            OSP_DECLARE_GET_DATA_IDS(cameraCtrl,    TESTAPP_DATA_CAMERA_CTRL);

            auto &rCamCtrl = rFB.data_get<ACtxCameraController>(rcamCtrl.di.camCtrl);
            rCamCtrl.m_target = Vector3(0.0f, 0.0f, 0.0f);
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
        scene           = setup_scene               (builder, rapplication);
        commonScene     = setup_common_scene        (builder, rscene, application, defaultPkg);
        physics         = setup_physics             (builder, rscene, commonScene);
        physShapes      = setup_phys_shapes         (builder, rscene, commonScene, physics, sc_matPhong);
        droppers        = setup_droppers            (builder, rscene, commonScene, physShapes);
        bounds          = setup_bounds              (builder, rscene, commonScene, physShapes);

        jolt            = setup_jolt              (builder, rscene, commonScene, physics);
        joltGravSet     = setup_jolt_factors      (builder, rTopData);
        joltGrav        = setup_jolt_force_accel  (builder, rjolt, joltGravSet, Vector3{0.0f, 0.0f, -9.81f});
        physShapesJolt  = setup_phys_shapes_jolt  (builder, rcommonScene, physics, physShapes, jolt, joltGravSet);

        auto const mainApp.pl = application.get_pipelines< PlApplication >();

        uniCore         = setup_uni_core            (builder, rmainApp.pl.mainLoop);
        uniScnFrame     = setup_uni_sceneframe      (builder, runiCore);
        uniTestPlanets  = setup_uni_testplanets     (builder, runiCore, uniScnFrame);

        add_floor(rphysShapes, sc_matVisualizer, defaultPkg, 0);

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

            sceneRenderer   = setup_scene_renderer      (builder, rapplication, windowApp, commonScene);
            create_materials(rsceneRenderer, sc_materialCount);

            magnumScene     = setup_magnum_scene        (builder, rapplication, windowApp, sceneRenderer, magnum, scene, commonScene);
            cameraCtrl      = setup_camera_ctrl         (builder, rwindowApp, sceneRenderer, magnumScene);
            cameraFree      = setup_camera_free         (builder, rwindowApp, scene, cameraCtrl);
            shVisual        = setup_shader_visualizer   (builder, rwindowApp, sceneRenderer, magnum, magnumScene, sc_matVisualizer);
            shFlat          = setup_shader_flat         (builder, rwindowApp, sceneRenderer, magnum, magnumScene, sc_matFlat);
            shPhong         = setup_shader_phong        (builder, rwindowApp, sceneRenderer, magnum, magnumScene, sc_matPhong);
            camThrow        = setup_thrower             (builder, rwindowApp, cameraCtrl, physShapes);
            shapeDraw       = setup_phys_shapes_draw    (builder, rwindowApp, sceneRenderer, commonScene, physics, physShapes);
            cursor          = setup_cursor              (builder, rapplication, sceneRenderer, cameraCtrl, commonScene, sc_matFlat, rTestApp.m_defaultPkg);
            planetsDraw     = setup_testplanets_draw    (builder, rwindowApp, sceneRenderer, cameraCtrl, commonScene, uniCore, uniScnFrame, uniTestPlanets, sc_matVisualizer, sc_matFlat);

            setup_magnum_draw(rTestApp, scene, sceneRenderer, magnumScene);
        };

        #undef SCENE_SESSIONS
        #undef RENDERER_SESSIONS

        return setup_renderer;
    });

    add_scenario("solar-system", "Scenario that simulates a basic solar system.",
        [](TestApp& rTestApp) -> RendererSetupFunc_t
        {
            #define SCENE_SESSIONS      scene, commonScene, solarSystemCore, solarSystemScnFrame, solarSystemTestPlanets
            #define RENDERER_SESSIONS   sceneRenderer, magnumScene, cameraCtrl, cameraFree, shFlat, planetsDraw

            using namespace testapp::scenes;

            auto const  defaultPkg = rTestApp.m_defaultPkg;
            auto const  application = rTestApp.m_application;
            auto& rTopData = rTestApp.m_topData;

            TopTaskBuilder builder{ rTestApp.m_tasks, rTestApp.m_scene.m_edges, rTestApp.m_taskData };

            auto& [SCENE_SESSIONS] = resize_then_unpack<5>(rTestApp.m_scene.m_sessions);

            // Compose together lots of Sessions
            scene = setup_scene(builder, rapplication);
            commonScene = setup_common_scene(builder, rscene, application, defaultPkg);

            auto const mainApp.pl = application.get_pipelines< PlApplication >();

            solarSystemCore = setup_uni_core(builder, rmainApp.pl.mainLoop);
            solarSystemScnFrame = setup_uni_sceneframe(builder, rsolarSystemCore);
            solarSystemTestPlanets = setup_solar_system_testplanets(builder, rsolarSystemCore, solarSystemScnFrame);

            RendererSetupFunc_t const setup_renderer = [](TestApp& rTestApp)
            {
                auto const  application = rTestApp.m_application;
                auto const  windowApp = rTestApp.m_windowApp;
                auto const  magnum = rTestApp.m_magnum;
                auto const  defaultPkg = rTestApp.m_defaultPkg;
                auto& rTopData = rTestApp.m_topData;

                TopTaskBuilder builder{ rTestApp.m_tasks, rTestApp.m_renderer.m_edges, rTestApp.m_taskData };

                auto& [SCENE_SESSIONS] = unpack<5>(rTestApp.m_scene.m_sessions);
                auto& [RENDERER_SESSIONS] = resize_then_unpack<6>(rTestApp.m_renderer.m_sessions);

                sceneRenderer = setup_scene_renderer(builder, rapplication, windowApp, commonScene);
                create_materials(rsceneRenderer, sc_materialCount);

                magnumScene = setup_magnum_scene(builder, rapplication, windowApp, sceneRenderer, magnum, scene, commonScene);
                cameraCtrl = setup_camera_ctrl(builder, rwindowApp, sceneRenderer, magnumScene);

                OSP_DECLARE_GET_DATA_IDS(cameraCtrl, TESTAPP_DATA_CAMERA_CTRL);

                // Zoom out the camera so that all planets are in view
                auto& rCameraController = rFB.data_get<ACtxCameraController>(rcamCtrl.di.camCtrl);
                rCameraController.m_orbitDistance += 75000;

                cameraFree = setup_camera_free(builder, rwindowApp, scene, cameraCtrl);
                shFlat = setup_shader_flat(builder, rwindowApp, sceneRenderer, magnum, magnumScene, sc_matFlat);
                planetsDraw = setup_solar_system_planets_draw(builder, rwindowApp, sceneRenderer, cameraCtrl, commonScene, solarSystemCore, solarSystemScnFrame, solarSystemTestPlanets, sc_matFlat);

                setup_magnum_draw(rTestApp, scene, sceneRenderer, magnumScene);
            };

            #undef SCENE_SESSIONS
            #undef RENDERER_SESSIONS

            return setup_renderer;
        });
    #endif

    return scenarioMap;
}

ScenarioMap_t const& scenarios()
{
    static ScenarioMap_t s_scenarioMap = make_scenarios();
    return s_scenarioMap;
}

} // namespace testapp


