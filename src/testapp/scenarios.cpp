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
#include "sessions/physics.h"
#include "sessions/misc.h"
#include "sessions/newton.h"
#include "sessions/magnum.h"
#include "sessions/universe.h"
#include "sessions/vehicles.h"

#include "MagnumApplication.h"

#include <adera/activescene/vehicles_vb_fn.h>

#include <osp/activescene/basic.h>
#include <osp/core/Resources.h>
#include <osp/tasks/top_utils.h>
#include <osp/util/logging.h>

#include <Magnum/GL/DefaultFramebuffer.h>

#include <algorithm>

using namespace adera;
using namespace osp;
using namespace osp::active;

namespace testapp
{

static constexpr auto   sc_matVisualizer    = draw::MaterialId(0);
static constexpr auto   sc_matFlat          = draw::MaterialId(1);
static constexpr auto   sc_matPhong         = draw::MaterialId(2);
static constexpr int    sc_materialCount    = 4;

struct MainLoopSignals
{
    PipelineId mainLoop;
    PipelineId inputs;
    PipelineId renderSync;
    PipelineId renderResync;
    PipelineId sceneUpdate;
    PipelineId sceneRender;
};

class CommonMagnumApp : public IOspApplication
{
public:
    CommonMagnumApp(TestApp &rTestApp, MainLoopControl &rMainLoopCtrl, MainLoopSignals signals) noexcept
     : m_rTestApp       { rTestApp }
     , m_rMainLoopCtrl  { rMainLoopCtrl }
     , m_signals        { signals }
    { }

    void run(MagnumApplication& rApp) override
    {
        // Start the main loop

        PipelineId const mainLoop = m_rTestApp.m_application.get_pipelines<PlApplication>().mainLoop;
        m_rTestApp.m_pExecutor->run(m_rTestApp, mainLoop);

        // Resyncronize renderer

        m_rMainLoopCtrl = MainLoopControl{
            .doUpdate = false,
            .doSync   = true,
            .doResync = true,
            .doRender = false,
        };

        signal_all();

        m_rTestApp.m_pExecutor->wait(m_rTestApp);
    }

    void draw(MagnumApplication& rApp, float delta) override
    {
        // Magnum Application's main loop calls this

        m_rMainLoopCtrl = MainLoopControl{
            .doUpdate = true,
            .doSync   = true,
            .doResync = false,
            .doRender = true,
        };

        signal_all();

        m_rTestApp.m_pExecutor->wait(m_rTestApp);
    }

    void exit(MagnumApplication& rApp) override
    {
        m_rMainLoopCtrl = MainLoopControl{
            .doUpdate = false,
            .doSync   = false,
            .doResync = false,
            .doRender = false,
        };

        signal_all();

        m_rTestApp.m_pExecutor->wait(m_rTestApp);

        if (m_rTestApp.m_pExecutor->is_running(m_rTestApp))
        {
            // Main loop must have stopped, but didn't!
            m_rTestApp.m_pExecutor->wait(m_rTestApp);
            std::abort();
        }
    }

private:

    void signal_all()
    {
        m_rTestApp.m_pExecutor->signal(m_rTestApp, m_signals.mainLoop);
        m_rTestApp.m_pExecutor->signal(m_rTestApp, m_signals.inputs);
        m_rTestApp.m_pExecutor->signal(m_rTestApp, m_signals.renderSync);
        m_rTestApp.m_pExecutor->signal(m_rTestApp, m_signals.renderResync);
        m_rTestApp.m_pExecutor->signal(m_rTestApp, m_signals.sceneUpdate);
        m_rTestApp.m_pExecutor->signal(m_rTestApp, m_signals.sceneRender);
    }

    TestApp         &m_rTestApp;
    MainLoopControl &m_rMainLoopCtrl;

    MainLoopSignals m_signals;
};

static void setup_magnum_draw(TestApp& rTestApp, Session const& scene, Session const& sceneRenderer, Session const& magnumScene)
{
    OSP_DECLARE_GET_DATA_IDS(rTestApp.m_application,    TESTAPP_DATA_APPLICATION);
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer,             TESTAPP_DATA_SCENE_RENDERER);
    OSP_DECLARE_GET_DATA_IDS(rTestApp.m_magnum,         TESTAPP_DATA_MAGNUM);
    OSP_DECLARE_GET_DATA_IDS(magnumScene,               TESTAPP_DATA_MAGNUM_SCENE);

    auto &rMainLoopCtrl = top_get<MainLoopControl>  (rTestApp.m_topData, idMainLoopCtrl);
    auto &rActiveApp    = top_get<MagnumApplication>(rTestApp.m_topData, idActiveApp);
    auto &rCamera       = top_get<draw::Camera>     (rTestApp.m_topData, idCamera);

    rCamera.set_aspect_ratio(Vector2{Magnum::GL::defaultFramebuffer.viewport().size()});

    MainLoopSignals const signals
    {
        .mainLoop     = rTestApp.m_application .get_pipelines<PlApplication>()   .mainLoop,
        .inputs       = rTestApp.m_windowApp   .get_pipelines<PlWindowApp>()     .inputs,
        .renderSync   = rTestApp.m_windowApp   .get_pipelines<PlWindowApp>()     .sync,
        .renderResync = rTestApp.m_windowApp   .get_pipelines<PlWindowApp>()     .resync,
        .sceneUpdate  = scene                  .get_pipelines<PlScene>()         .update,
        .sceneRender  = sceneRenderer          .get_pipelines<PlSceneRenderer>() .render,
    };

    rActiveApp.set_osp_app( std::make_unique<CommonMagnumApp>(rTestApp, rMainLoopCtrl, signals) );
}

static ScenarioMap_t make_scenarios()
{   
    ScenarioMap_t scenarioMap;

    register_stage_enums();

    auto const add_scenario = [&scenarioMap] (std::string_view name, std::string_view desc, SceneSetupFunc_t run)
    {
        scenarioMap.emplace(name, ScenarioOption{desc, run});
    };

    add_scenario("enginetest", "Basic game engine and drawing scenario (without using TopTasks)",
                 [] (TestApp& rTestApp) -> RendererSetupFunc_t
    {
        SessionGroup &rOut = rTestApp.m_scene;
        rOut.m_sessions.resize(1);
        TopDataId const idSceneData = rOut.m_sessions[0].acquire_data<1>(rTestApp.m_topData)[0];

        OSP_DECLARE_GET_DATA_IDS(rTestApp.m_application, TESTAPP_DATA_APPLICATION);

        auto &rResources = top_get<Resources>(rTestApp.m_topData, idResources);

        // enginetest::setup_scene returns an entt::any containing one big
        // struct containing all the scene data.
        top_assign<enginetest::EngineTestScene>(rTestApp.m_topData, idSceneData, enginetest::setup_scene(rResources, rTestApp.m_defaultPkg));

        return [] (TestApp& rTestApp)
        {
            TopDataId const idSceneData = rTestApp.m_scene.m_sessions[0].m_data[0];
            auto &rScene = top_get<enginetest::EngineTestScene>(rTestApp.m_topData, idSceneData);

            OSP_DECLARE_GET_DATA_IDS(rTestApp.m_magnum,     TESTAPP_DATA_MAGNUM);
            OSP_DECLARE_GET_DATA_IDS(rTestApp.m_windowApp,  TESTAPP_DATA_WINDOW_APP);
            auto &rActiveApp    = top_get< MagnumApplication >      (rTestApp.m_topData, idActiveApp);
            auto &rRenderGl     = top_get< draw::RenderGL >         (rTestApp.m_topData, idRenderGl);
            auto &rUserInput    = top_get< input::UserInputHandler >(rTestApp.m_topData, idUserInput);

            // Renderer state is stored as lambda capture
            rActiveApp.set_osp_app(enginetest::generate_draw_func(rScene, rActiveApp, rRenderGl, rUserInput));
        };
    });

    add_scenario("physics", "Newton Dynamics integration test scenario",
                 [] (TestApp& rTestApp) -> RendererSetupFunc_t
    {
        #define SCENE_SESSIONS      scene, commonScene, physics, physShapes, droppers, bounds, newton, nwtGravSet, nwtGrav, physShapesNwt
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

        newton          = setup_newton              (builder, rTopData, scene, commonScene, physics);
        nwtGravSet      = setup_newton_factors      (builder, rTopData);
        nwtGrav         = setup_newton_force_accel  (builder, rTopData, newton, nwtGravSet, Vector3{0.0f, 0.0f, -9.81f});
        physShapesNwt   = setup_phys_shapes_newton  (builder, rTopData, commonScene, physics, physShapes, newton, nwtGravSet);

        add_floor(rTopData, physShapes, sc_matVisualizer, defaultPkg, 4);

        return [] (TestApp& rTestApp)
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
    });


    add_scenario("vehicles", "Physics scenario but with Vehicles",
                 [] (TestApp& rTestApp) -> RendererSetupFunc_t
    {
        #define SCENE_SESSIONS      scene, commonScene, physics, physShapes, droppers, bounds, newton, nwtGravSet, nwtGrav, physShapesNwt, \
                                    prefabs, parts, vehicleSpawn, signalsFloat, vehicleSpawnVB, vehicleSpawnRgd, vehicleSpawnNwt, testVehicles
        #define RENDERER_SESSIONS   sceneRenderer, magnumScene, cameraCtrl, cameraFree, shVisual, shFlat, shPhong, camThrow, shapeDraw, cursor, \
                                    prefabDraw, vehicleDraw

        using namespace testapp::scenes;

        auto const  defaultPkg      = rTestApp.m_defaultPkg;
        auto const  application     = rTestApp.m_application;
        auto        & rTopData      = rTestApp.m_topData;

        TopTaskBuilder builder{rTestApp.m_tasks, rTestApp.m_scene.m_edges, rTestApp.m_taskData};

        auto & [SCENE_SESSIONS] = resize_then_unpack<18>(rTestApp.m_scene.m_sessions);

        // Compose together lots of Sessions
        scene           = setup_scene               (builder, rTopData, application);
        commonScene     = setup_common_scene        (builder, rTopData, scene, application, defaultPkg);
        physics         = setup_physics             (builder, rTopData, scene, commonScene);
        physShapes      = setup_phys_shapes         (builder, rTopData, scene, commonScene, physics, sc_matPhong);
        //droppers        = setup_droppers            (builder, rTopData, scene, commonScene, physShapes);
        bounds          = setup_bounds              (builder, rTopData, scene, commonScene, physShapes);

        prefabs         = setup_prefabs             (builder, rTopData, application, scene, commonScene, physics);
        parts           = setup_parts               (builder, rTopData, scene);
        vehicleSpawn    = setup_vehicle_spawn       (builder, rTopData, scene);
        vehicleSpawnVB  = setup_vehicle_spawn_vb    (builder, rTopData, application, scene, commonScene, prefabs, parts, vehicleSpawn, signalsFloat);
        testVehicles    = setup_test_vehicles       (builder, rTopData, application);

        newton          = setup_newton              (builder, rTopData, scene, commonScene, physics);
        nwtGravSet      = setup_newton_factors      (builder, rTopData);
        nwtGrav         = setup_newton_force_accel  (builder, rTopData, newton, nwtGravSet, Vector3{0.0f, 0.0f, -9.81f});
        physShapesNwt   = setup_phys_shapes_newton  (builder, rTopData, commonScene, physics, physShapes, newton, nwtGravSet);
        vehicleSpawnNwt = setup_vehicle_spawn_newton(builder, rTopData, application, commonScene, physics, prefabs, parts, vehicleSpawn, newton);

        OSP_DECLARE_GET_DATA_IDS(vehicleSpawn,   TESTAPP_DATA_VEHICLE_SPAWN);
        OSP_DECLARE_GET_DATA_IDS(vehicleSpawnVB, TESTAPP_DATA_VEHICLE_SPAWN_VB);
        OSP_DECLARE_GET_DATA_IDS(testVehicles,   TESTAPP_DATA_TEST_VEHICLES);

        auto &rVehicleSpawn     = top_get<ACtxVehicleSpawn>     (rTopData, idVehicleSpawn);
        auto &rVehicleSpawnVB   = top_get<ACtxVehicleSpawnVB>   (rTopData, idVehicleSpawnVB);
        auto &rTVPartVehicle    = top_get<VehicleData>          (rTopData, idTVPartVehicle);

        for (int i = 0; i < 1; ++i)
        {
            rVehicleSpawn.spawnRequest.push_back(
            {
               .m_position = {float(i - 2) * 8.0f, 30.0f, 10.0f},
               .m_velocity = {0.0, 0.0f, 0.0f},
               .m_rotation = {}
            });
            rVehicleSpawnVB.dataVB.push_back(&rTVPartVehicle);
        }


        add_floor(rTopData, physShapes, sc_matVisualizer, defaultPkg, 4);

        return [] (TestApp& rTestApp)
        {
            auto const  application     = rTestApp.m_application;
            auto const  windowApp       = rTestApp.m_windowApp;
            auto const  magnum          = rTestApp.m_magnum;
            auto const  defaultPkg      = rTestApp.m_defaultPkg;
            auto        & rTopData      = rTestApp.m_topData;

            TopTaskBuilder builder{rTestApp.m_tasks, rTestApp.m_renderer.m_edges, rTestApp.m_taskData};

            auto & [SCENE_SESSIONS] = unpack<18>(rTestApp.m_scene.m_sessions);
            auto & [RENDERER_SESSIONS] = resize_then_unpack<12>(rTestApp.m_renderer.m_sessions);

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
            prefabDraw      = setup_prefab_draw         (builder, rTopData, application, windowApp, sceneRenderer, commonScene, prefabs);
            vehicleDraw     = setup_vehicle_spawn_draw  (builder, rTopData, sceneRenderer, vehicleSpawn);

            // HACK
            //OSP_DECLARE_GET_DATA_IDS(sceneRenderer, TESTAPP_DATA_SCENE_RENDERER);
            //OSP_DECLARE_GET_DATA_IDS(veh, TESTAPP_DATA_SCENE_RENDERER);
            //auto &a = osp::top_get<osp::draw::ACtxSceneRender>(rTopData, idScnRender);
            //a.m_needDrawTf.set(std::size_t(root));

            setup_magnum_draw(rTestApp, scene, sceneRenderer, magnumScene);
        };

        #undef SCENE_SESSIONS
        #undef RENDERER_SESSIONS
    });


#if 0

    add_scenario("vehicles", "Physics scenario but with Vehicles",
                 [] (MainView mainView, Sessions_t& sceneOut) -> RendererSetup_t
    {
        using namespace testapp::scenes;
        using namespace osp::active;

        auto const idResources  = mainView.m_idResources;
        auto &rTopData          = mainView.m_topData;
        auto &rTags             = mainView.m_rTags;
        Builder_t builder{rTags, mainView.m_rTasks, mainView.m_rTaskData};

        auto &
        [
            commonScene, matVisual, physics, physShapes,
            prefabs, parts,
            vehicleSpawn, vehicleSpawnVB, vehicleSpawnRgd,
            signalsFloat, machRocket, machRcsDriver,
            testVehicles, droppers, gravity, bounds, thrower,
            newton, nwtGravSet, nwtGrav, physShapesNwt, vehicleSpawnNwt, nwtRocketSet, rocketsNwt
        ] = resize_then_unpack<24>(sceneOut);

        commonScene           = setup_common_scene        (builder, rTopData, rTags, idResources, mainView.m_defaultPkg);
        matVisual           = setup_material            (builder, rTopData, rTags, commonScene);
        physics             = setup_physics             (builder, rTopData, rTags, commonScene);
        physShapes          = setup_phys_shapes         (builder, rTopData, rTags, commonScene, physics, matVisual);
        prefabs             = setup_prefabs             (builder, rTopData, rTags, commonScene, physics, matVisual, idResources);
        parts               = setup_parts               (builder, rTopData, rTags, commonScene, idResources);
        signalsFloat        = setup_signals_float       (builder, rTopData, rTags, commonScene, parts);
        vehicleSpawn        = setup_vehicle_spawn       (builder, rTopData, rTags, commonScene);
        vehicleSpawnVB      = setup_vehicle_spawn_vb    (builder, rTopData, rTags, commonScene, prefabs, parts, vehicleSpawn, signalsFloat, idResources);
        machRocket          = setup_mach_rocket         (builder, rTopData, rTags, commonScene, parts, signalsFloat);
        machRcsDriver       = setup_mach_rcsdriver      (builder, rTopData, rTags, commonScene, parts, signalsFloat);
        testVehicles        = setup_test_vehicles       (builder, rTopData, rTags, commonScene, idResources);
        droppers            = setup_droppers            (builder, rTopData, rTags, commonScene, physShapes);
        bounds              = setup_bounds              (builder, rTopData, rTags, commonScene, physics, physShapes);

        newton              = setup_newton              (builder, rTopData, rTags, commonScene, physics);
        nwtGravSet          = setup_newton_factors      (builder, rTopData, rTags);
        nwtGrav             = setup_newton_force_accel  (builder, rTopData, rTags, newton, nwtGravSet, Vector3{0.0f, 0.0f, -9.81f});
        physShapesNwt       = setup_phys_shapes_newton  (builder, rTopData, rTags, commonScene, physics, physShapes, newton, nwtGravSet);
        vehicleSpawnNwt     = setup_vehicle_spawn_newton(builder, rTopData, rTags, commonScene, physics, prefabs, parts, vehicleSpawn, newton, idResources);
        nwtRocketSet        = setup_newton_factors      (builder, rTopData, rTags);
        rocketsNwt          = setup_rocket_thrust_newton(builder, rTopData, rTags, commonScene, physics, prefabs, parts, signalsFloat, newton, nwtRocketSet);

        OSP_SESSION_UNPACK_DATA(commonScene,      TESTAPP_COMMON_SCENE);
        OSP_SESSION_UNPACK_DATA(vehicleSpawn,   TESTAPP_VEHICLE_SPAWN);
        OSP_SESSION_UNPACK_DATA(vehicleSpawnVB, TESTAPP_VEHICLE_SPAWN_VB);
        OSP_SESSION_UNPACK_DATA(testVehicles,   TESTAPP_TEST_VEHICLES);

        add_floor(rTopData, commonScene, matVisual, physShapes, idResources, mainView.m_defaultPkg);

        auto &rActiveIds        = top_get<ActiveReg_t>          (rTopData, idActiveIds);
        auto &rTVPartVehicle    = top_get<VehicleData>          (rTopData, idTVPartVehicle);
        auto &rVehicleSpawn     = top_get<ACtxVehicleSpawn>     (rTopData, idVehicleSpawn);
        auto &rVehicleSpawnVB   = top_get<ACtxVehicleSpawnVB>   (rTopData, idVehicleSpawnVB);

        for (int i = 0; i < 10; ++i)
        {
            rVehicleSpawn.spawnRequest.push_back(
            {
               .m_position = {float(i - 2) * 8.0f, 30.0f, 10.0f},
               .m_velocity = {0.0, 0.0f, 0.0f},
               .m_rotation = {}
            });
            rVehicleSpawnVB.dataVB.push_back(&rTVPartVehicle);
        }

        return [] (MainView mainView, Session const& magnum, Sessions_t const& scene, [[maybe_unused]] Sessions_t& rendererOut)
        {
            auto &rTopData = mainView.m_topData;
            auto &rTags = mainView.m_rTags;
            Builder_t builder{mainView.m_rTags, mainView.m_rTasks, mainView.m_rTaskData};

            auto const&
            [
                commonScene, matVisual, physics, physShapes,
                prefabs, parts,
                vehicleSpawn, vehicleSpawnVB, vehicleSpawnRgd,
                signalsFloat, machRocket, machRcsDriver,
                testVehicles, droppers, gravity, bounds, thrower,
                newton, nwtGravSet, nwtGrav, physShapesNwt, vehicleSpawnNwt, nwtRocketSet, rocketsNwt
            ] = unpack<24>(scene);

            auto & [scnRender, cameraCtrl, shPhong, shFlat, camThrow, vehicleCtrl, cameraVehicle, thrustIndicator] = resize_then_unpack<8>(rendererOut);
            scnRender       = setup_scene_renderer      (builder, rTopData, rTags, magnum, commonScene, mainView.m_idResources);
            cameraCtrl      = setup_camera_ctrl         (builder, rTopData, rTags, magnum, scnRender);
            shPhong         = setup_shader_phong        (builder, rTopData, rTags, magnum, commonScene, scnRender, matVisual);
            shFlat          = setup_shader_flat         (builder, rTopData, rTags, magnum, commonScene, scnRender, {});
            camThrow        = setup_thrower             (builder, rTopData, rTags, magnum, scnRender, cameraCtrl, physShapes);
            vehicleCtrl     = setup_vehicle_control     (builder, rTopData, rTags, commonScene, parts, signalsFloat, magnum);
            cameraVehicle   = setup_camera_vehicle      (builder, rTopData, rTags, magnum, commonScene, parts, physics, cameraCtrl, vehicleCtrl);
            thrustIndicator = setup_thrust_indicators   (builder, rTopData, rTags, magnum, commonScene, parts, signalsFloat, scnRender, cameraCtrl, shFlat, mainView.m_idResources, mainView.m_defaultPkg);

            setup_magnum_draw(mainView, magnum, commonScene, scnRender);
        };
    });

#endif

    add_scenario("universe", "Universe test scenario with very unrealistic planets",
                 [] (TestApp& rTestApp) -> RendererSetupFunc_t
    {
        #define SCENE_SESSIONS      scene, commonScene, physics, physShapes, droppers, bounds, newton, nwtGravSet, nwtGrav, physShapesNwt, uniCore, uniScnFrame, uniTestPlanets
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

        newton          = setup_newton              (builder, rTopData, scene, commonScene, physics);
        nwtGravSet      = setup_newton_factors      (builder, rTopData);
        nwtGrav         = setup_newton_force_accel  (builder, rTopData, newton, nwtGravSet, Vector3{0.0f, 0.0f, -9.81f});
        physShapesNwt   = setup_phys_shapes_newton  (builder, rTopData, commonScene, physics, physShapes, newton, nwtGravSet);

        auto const tgApp = application.get_pipelines< PlApplication >();

        uniCore         = setup_uni_core            (builder, rTopData, tgApp.mainLoop);
        uniScnFrame     = setup_uni_sceneframe      (builder, rTopData, uniCore);
        uniTestPlanets  = setup_uni_testplanets     (builder, rTopData, uniCore, uniScnFrame);

        add_floor(rTopData, physShapes, sc_matVisualizer, defaultPkg, 0);

        return [] (TestApp& rTestApp)
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
    });

    return scenarioMap;
}

ScenarioMap_t const& scenarios()
{
    static ScenarioMap_t s_scenarioMap = make_scenarios();
    return s_scenarioMap;
}

} // namespace testapp


