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
#include "scenarios_enginetest.h"
#include "identifiers.h"

#include "scene_common.h"
#include "scene_physics.h"
#include "scene_misc.h"
#include "scene_newton.h"
#include "scene_renderer.h"
#include "scene_universe.h"
#include "scene_vehicles.h"

#include "../ActiveApplication.h"
#include "../VehicleBuilder.h"

#include <osp/Active/basic.h>
#include <osp/Active/parts.h>
#include <osp/Active/SysPrefabInit.h>
#include <osp/Resource/resources.h>

#include <osp/tasks/top_utils.h>

#include <osp/logging.h>

#include <Magnum/GL/DefaultFramebuffer.h>

#include <algorithm>

using namespace osp;

namespace testapp
{

static constexpr auto   sc_matVisualizer    = active::MaterialId(0);
static constexpr auto   sc_matFlat          = active::MaterialId(1);
static constexpr auto   sc_matPhong         = active::MaterialId(2);
static constexpr int    sc_materialCount    = 4;


static void setup_magnum_draw(TestApp& rTestApp, Session const& scene, Session const& scnRenderer, std::vector<TargetId> run = {})
{
    OSP_DECLARE_GET_DATA_IDS(scnRenderer,        TESTAPP_DATA_COMMON_RENDERER);
    OSP_DECLARE_GET_DATA_IDS(rTestApp.m_windowApp,  TESTAPP_DATA_WINDOW_APP);
    OSP_DECLARE_GET_DATA_IDS(rTestApp.m_magnum,     TESTAPP_DATA_MAGNUM);

    auto &rActiveApp    = top_get<ActiveApplication>(rTestApp.m_topData, idActiveApp);
    auto &rCamera       = top_get<active::Camera>(rTestApp.m_topData, idCamera);
    rCamera.set_aspect_ratio(Vector2{Magnum::GL::defaultFramebuffer.viewport().size()});

    auto tgScn = scene                  .get_targets<TgtScene>();
    auto tgWin = rTestApp.m_windowApp   .get_targets<TgtWindowApp>();

    // Run Resync tasks to mark all used gpu resources as dirty
    auto aaa = scene.get_targets<TgtScene>();
    rTestApp.m_exec.m_targetDirty.set(std::size_t(aaa.resyncAll));

    run.insert(run.end(), {tgScn.sync, tgScn.time, tgWin.input, tgWin.render});

    // run gets copied but who cares lol
    rActiveApp.set_on_draw( [&rTestApp, runTagsVec = std::move(run), resync  =tgScn.resyncAll]
                            (ActiveApplication& rApp, float delta)
    {
        // Magnum Application's main loop is here

        std::cout << "\n---- START ----\n";

        top_enqueue_quick(rTestApp.m_tasks, rTestApp.m_graph.value(), rTestApp.m_exec, runTagsVec);

        for (TaskId const task : rTestApp.m_exec.m_tasksQueued)
        {
            std::cout << "enq: " << rTestApp.m_taskData[task].m_debugName << "\n";
        }

        top_run_blocking(rTestApp.m_tasks, rTestApp.m_graph.value(), rTestApp.m_taskData, rTestApp.m_topData, rTestApp.m_exec);

        // Enqueued tasks that don't run indicate a deadlock
//        if ( ! std::all_of(std::begin(rExec.m_taskQueuedCounts),
//                           std::end(rExec.m_taskQueuedCounts),
//                           [] (unsigned int n) { return n == 0; }))
//        {
//            OSP_LOG_ERROR("Deadlock detected!");
//            debug_top_print_deadlock( rTasks, rTaskData, rExec);
//            std::abort();
//        }
    });
}

static ScenarioMap_t make_scenarios()
{   
    ScenarioMap_t scenarioMap;

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
        auto &rResources = osp::top_get<Resources>(rTestApp.m_topData, rTestApp.m_idResources);

        // enginetest::setup_scene returns an entt::any containing one big
        // struct containing all the scene data.
        top_assign<enginetest::EngineTestScene>(rTestApp.m_topData, idSceneData, enginetest::setup_scene(rResources, rTestApp.m_defaultPkg));

        return [] (TestApp& rTestApp)
        {
            TopDataId const idSceneData = rTestApp.m_scene.m_sessions[0].m_data[0];
            auto &rScene = osp::top_get<enginetest::EngineTestScene>(rTestApp.m_topData, idSceneData);

            OSP_DECLARE_GET_DATA_IDS(rTestApp.m_magnum,     TESTAPP_DATA_MAGNUM);
            OSP_DECLARE_GET_DATA_IDS(rTestApp.m_windowApp,  TESTAPP_DATA_WINDOW_APP);
            auto &rActiveApp    = top_get< ActiveApplication >      (rTestApp.m_topData, idActiveApp);
            auto &rRenderGl     = top_get< active::RenderGL >       (rTestApp.m_topData, idRenderGl);
            auto &rUserInput    = top_get< input::UserInputHandler >(rTestApp.m_topData, idUserInput);

            // Renderer state is stored as lambda capture
            rActiveApp.set_on_draw(enginetest::generate_draw_func(rScene, rActiveApp, rRenderGl, rUserInput));
        };
    });

    add_scenario("physics", "Newton Dynamics integration test scenario",
                 [] (TestApp& rTestApp) -> RendererSetupFunc_t
    {
        using namespace testapp::scenes;

        auto const  defaultPkg      = rTestApp.m_defaultPkg;
        auto const  idResources     = rTestApp.m_idResources;
        auto        & rTopData      = rTestApp.m_topData;

        TopTaskBuilder builder{rTestApp.m_tasks, rTestApp.m_scene.m_edges, rTestApp.m_taskData};

        auto & [
            scene, commonScene, physics, shapeSpawn, droppers, bounds, newton, nwtGravSet, nwtGrav, shapeSpawnNwt
        ] = resize_then_unpack<10>(rTestApp.m_scene.m_sessions);

        // Compose together lots of Sessions
        scene           = setup_scene               (builder, rTopData);
        commonScene     = setup_common_scene        (builder, rTopData, scene, idResources, defaultPkg);
        physics         = setup_physics             (builder, rTopData, commonScene);
        shapeSpawn      = setup_shape_spawn         (builder, rTopData, commonScene, physics, sc_matVisualizer);
        //droppers        = setup_droppers            (builder, rTopData, commonScene, shapeSpawn);
        //bounds          = setup_bounds              (builder, rTopData, commonScene, physics, shapeSpawn);

        newton          = setup_newton              (builder, rTopData, scene, commonScene, physics);
        nwtGravSet      = setup_newton_factors      (builder, rTopData);
        //nwtGrav         = setup_newton_force_accel  (builder, rTopData, newton, nwtGravSet, Vector3{0.0f, 0.0f, -9.81f});
        shapeSpawnNwt   = setup_shape_spawn_newton  (builder, rTopData, commonScene, physics, shapeSpawn, newton, nwtGravSet);

        create_materials(rTopData, commonScene, sc_materialCount);
        add_floor(rTopData, commonScene, shapeSpawn, sc_matVisualizer, idResources, defaultPkg);

        rTestApp.m_exec.resize(rTestApp.m_tasks);
        auto bbb = commonScene.get_targets<TgtCommonScene>();
        rTestApp.m_exec.m_targetDirty.set(std::size_t(commonScene.get_targets<TgtCommonScene>().drawEnt_mod));
        rTestApp.m_exec.m_targetDirty.set(std::size_t(shapeSpawn.get_targets<TgtShapeSpawn>().spawnRequest_mod));


        return [] (TestApp& rTestApp)
        {
            auto const  windowApp       = rTestApp.m_windowApp;
            auto const  magnum          = rTestApp.m_magnum;
            auto const  defaultPkg      = rTestApp.m_defaultPkg;
            auto const  idResources     = rTestApp.m_idResources;
            auto        & rTopData      = rTestApp.m_topData;

            TopTaskBuilder builder{rTestApp.m_tasks, rTestApp.m_renderer.m_edges, rTestApp.m_taskData};

            auto & [
                scene, commonScene, physics, shapeSpawn, droppers, bounds, newton, nwtGravSet, nwtGrav, shapeSpawnNwt
            ] = unpack<10>(rTestApp.m_scene.m_sessions);

            auto & [
                scnRender, cameraCtrl, cameraFree, shVisual, camThrow
            ] = resize_then_unpack<5>(rTestApp.m_renderer.m_sessions);

            scnRender   = setup_scene_renderer      (builder, rTopData, windowApp, magnum, scene, commonScene, idResources);
            cameraCtrl  = setup_camera_ctrl         (builder, rTopData, windowApp, scnRender);
            cameraFree  = setup_camera_free         (builder, rTopData, windowApp, scene, cameraCtrl);
            shVisual    = setup_shader_visualizer   (builder, rTopData, magnum, commonScene, scnRender, sc_matVisualizer);
            camThrow    = setup_thrower             (builder, rTopData, windowApp, cameraCtrl, shapeSpawn);

            setup_magnum_draw(rTestApp, scene, scnRender);
        };
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
            commonScene, matVisual, physics, shapeSpawn,
            prefabs, parts,
            vehicleSpawn, vehicleSpawnVB, vehicleSpawnRgd,
            signalsFloat, machRocket, machRcsDriver,
            testVehicles, droppers, gravity, bounds, thrower,
            newton, nwtGravSet, nwtGrav, shapeSpawnNwt, vehicleSpawnNwt, nwtRocketSet, rocketsNwt
        ] = resize_then_unpack<24>(sceneOut);

        commonScene           = setup_common_scene        (builder, rTopData, rTags, idResources, mainView.m_defaultPkg);
        matVisual           = setup_material            (builder, rTopData, rTags, commonScene);
        physics             = setup_physics             (builder, rTopData, rTags, commonScene);
        shapeSpawn          = setup_shape_spawn         (builder, rTopData, rTags, commonScene, physics, matVisual);
        prefabs             = setup_prefabs             (builder, rTopData, rTags, commonScene, physics, matVisual, idResources);
        parts               = setup_parts               (builder, rTopData, rTags, commonScene, idResources);
        signalsFloat        = setup_signals_float       (builder, rTopData, rTags, commonScene, parts);
        vehicleSpawn        = setup_vehicle_spawn       (builder, rTopData, rTags, commonScene);
        vehicleSpawnVB      = setup_vehicle_spawn_vb    (builder, rTopData, rTags, commonScene, prefabs, parts, vehicleSpawn, signalsFloat, idResources);
        machRocket          = setup_mach_rocket         (builder, rTopData, rTags, commonScene, parts, signalsFloat);
        machRcsDriver       = setup_mach_rcsdriver      (builder, rTopData, rTags, commonScene, parts, signalsFloat);
        testVehicles        = setup_test_vehicles       (builder, rTopData, rTags, commonScene, idResources);
        droppers            = setup_droppers            (builder, rTopData, rTags, commonScene, shapeSpawn);
        bounds              = setup_bounds              (builder, rTopData, rTags, commonScene, physics, shapeSpawn);

        newton              = setup_newton              (builder, rTopData, rTags, commonScene, physics);
        nwtGravSet          = setup_newton_factors      (builder, rTopData, rTags);
        nwtGrav             = setup_newton_force_accel  (builder, rTopData, rTags, newton, nwtGravSet, Vector3{0.0f, 0.0f, -9.81f});
        shapeSpawnNwt       = setup_shape_spawn_newton  (builder, rTopData, rTags, commonScene, physics, shapeSpawn, newton, nwtGravSet);
        vehicleSpawnNwt     = setup_vehicle_spawn_newton(builder, rTopData, rTags, commonScene, physics, prefabs, parts, vehicleSpawn, newton, idResources);
        nwtRocketSet        = setup_newton_factors      (builder, rTopData, rTags);
        rocketsNwt          = setup_rocket_thrust_newton(builder, rTopData, rTags, commonScene, physics, prefabs, parts, signalsFloat, newton, nwtRocketSet);

        OSP_SESSION_UNPACK_DATA(commonScene,      TESTAPP_COMMON_SCENE);
        OSP_SESSION_UNPACK_DATA(vehicleSpawn,   TESTAPP_VEHICLE_SPAWN);
        OSP_SESSION_UNPACK_DATA(vehicleSpawnVB, TESTAPP_VEHICLE_SPAWN_VB);
        OSP_SESSION_UNPACK_DATA(testVehicles,   TESTAPP_TEST_VEHICLES);

        add_floor(rTopData, commonScene, matVisual, shapeSpawn, idResources, mainView.m_defaultPkg);

        auto &rActiveIds        = top_get<ActiveReg_t>          (rTopData, idActiveIds);
        auto &rTVPartVehicle    = top_get<VehicleData>          (rTopData, idTVPartVehicle);
        auto &rVehicleSpawn     = top_get<ACtxVehicleSpawn>     (rTopData, idVehicleSpawn);
        auto &rVehicleSpawnVB   = top_get<ACtxVehicleSpawnVB>   (rTopData, idVehicleSpawnVB);

        for (int i = 0; i < 10; ++i)
        {
            rVehicleSpawn.m_newVhBasicIn.push_back(
            {
               .m_position = {float(i - 2) * 8.0f, 30.0f, 10.0f},
               .m_velocity = {0.0, 0.0f, 0.0f},
               .m_rotation = {}
            });
            rVehicleSpawnVB.m_dataVB.push_back(&rTVPartVehicle);
        }

        return [] (MainView mainView, Session const& magnum, Sessions_t const& scene, [[maybe_unused]] Sessions_t& rendererOut)
        {
            auto &rTopData = mainView.m_topData;
            auto &rTags = mainView.m_rTags;
            Builder_t builder{mainView.m_rTags, mainView.m_rTasks, mainView.m_rTaskData};

            auto const&
            [
                commonScene, matVisual, physics, shapeSpawn,
                prefabs, parts,
                vehicleSpawn, vehicleSpawnVB, vehicleSpawnRgd,
                signalsFloat, machRocket, machRcsDriver,
                testVehicles, droppers, gravity, bounds, thrower,
                newton, nwtGravSet, nwtGrav, shapeSpawnNwt, vehicleSpawnNwt, nwtRocketSet, rocketsNwt
            ] = unpack<24>(scene);

            auto & [scnRender, cameraCtrl, shPhong, shFlat, camThrow, vehicleCtrl, cameraVehicle, thrustIndicator] = resize_then_unpack<8>(rendererOut);
            scnRender       = setup_scene_renderer      (builder, rTopData, rTags, magnum, commonScene, mainView.m_idResources);
            cameraCtrl      = setup_camera_ctrl         (builder, rTopData, rTags, magnum, scnRender);
            shPhong         = setup_shader_phong        (builder, rTopData, rTags, magnum, commonScene, scnRender, matVisual);
            shFlat          = setup_shader_flat         (builder, rTopData, rTags, magnum, commonScene, scnRender, {});
            camThrow        = setup_thrower             (builder, rTopData, rTags, magnum, scnRender, cameraCtrl, shapeSpawn);
            vehicleCtrl     = setup_vehicle_control     (builder, rTopData, rTags, commonScene, parts, signalsFloat, magnum);
            cameraVehicle   = setup_camera_vehicle      (builder, rTopData, rTags, magnum, commonScene, parts, physics, cameraCtrl, vehicleCtrl);
            thrustIndicator = setup_thrust_indicators   (builder, rTopData, rTags, magnum, commonScene, parts, signalsFloat, scnRender, cameraCtrl, shFlat, mainView.m_idResources, mainView.m_defaultPkg);

            setup_magnum_draw(mainView, magnum, commonScene, scnRender);
        };
    });

    add_scenario("universe", "Universe test scenario with very unrealistic planets",
                 [] (MainView mainView, Sessions_t& sceneOut) -> RendererSetup_t
    {
        using namespace testapp::scenes;

        auto const idResources  = mainView.m_idResources;
        auto &rTopData          = mainView.m_topData;
        auto &rTags             = mainView.m_rTags;
        Builder_t builder{rTags, mainView.m_rTasks, mainView.m_rTaskData};

        auto &
        [
            commonScene, matVisual, physics, shapeSpawn, droppers, bounds, newton, nwtGravSet, nwtGrav, shapeSpawnNwt, uniCore, uniScnFrame, uniTestPlanets
        ] = resize_then_unpack<13>(sceneOut);

        // Compose together lots of Sessions
        commonScene       = setup_common_scene        (builder, rTopData, rTags, idResources, mainView.m_defaultPkg);
        matVisual       = setup_material            (builder, rTopData, rTags, commonScene);
        physics         = setup_physics             (builder, rTopData, rTags, commonScene);
        shapeSpawn      = setup_shape_spawn         (builder, rTopData, rTags, commonScene, physics, matVisual);
        droppers        = setup_droppers            (builder, rTopData, rTags, commonScene, shapeSpawn);
        bounds          = setup_bounds              (builder, rTopData, rTags, commonScene, physics, shapeSpawn);

        newton          = setup_newton              (builder, rTopData, rTags, commonScene, physics);
        nwtGravSet      = setup_newton_factors      (builder, rTopData, rTags);
        nwtGrav         = setup_newton_force_accel  (builder, rTopData, rTags, newton, nwtGravSet, Vector3{0.0f, 0.0f, -9.81f});
        shapeSpawnNwt   = setup_shape_spawn_newton  (builder, rTopData, rTags, commonScene, physics, shapeSpawn, newton, nwtGravSet);

        uniCore         = setup_uni_core            (builder, rTopData, rTags);
        uniScnFrame     = setup_uni_sceneframe      (builder, rTopData, rTags);
        uniTestPlanets  = setup_uni_test_planets    (builder, rTopData, rTags, uniCore, uniScnFrame);

        add_floor(rTopData, commonScene, matVisual, shapeSpawn, idResources, mainView.m_defaultPkg);

        return [] (MainView mainView, Session const& magnum, Sessions_t const& scene, Sessions_t& rendererOut)
        {
            auto &rTopData = mainView.m_topData;
            auto &rTags = mainView.m_rTags;
            Builder_t builder{mainView.m_rTags, mainView.m_rTasks, mainView.m_rTaskData};

            auto const& [commonScene, matVisual, physics, shapeSpawn, droppers, bounds, newton, nwtGravSet, nwtGrav, shapeSpawnNwt, uniCore, uniScnFrame, uniTestPlanets] = unpack<13>(scene);

            rendererOut.resize(8);
            auto & [scnRender, cameraCtrl, cameraFree, shFlat, shVisual, camThrow, cursor, uniTestPlanetsRdr] = unpack<8>(rendererOut);
            scnRender           = setup_scene_renderer              (builder, rTopData, rTags, magnum, commonScene, mainView.m_idResources);
            cameraCtrl          = setup_camera_ctrl                 (builder, rTopData, rTags, magnum, scnRender);
            cameraFree          = setup_camera_free                 (builder, rTopData, rTags, magnum, commonScene, cameraCtrl);
            shFlat              = setup_shader_flat                 (builder, rTopData, rTags, magnum, commonScene, scnRender, {});
            shVisual            = setup_shader_visualizer           (builder, rTopData, rTags, magnum, commonScene, scnRender, matVisual);
            camThrow            = setup_thrower                     (builder, rTopData, rTags, magnum, scnRender, cameraCtrl, shapeSpawn);
            cursor              = setup_cursor                      (builder, rTopData, rTags, magnum, commonScene, scnRender, cameraCtrl, shFlat, mainView.m_idResources, mainView.m_defaultPkg);
            uniTestPlanetsRdr   = setup_uni_test_planets_renderer   (builder, rTopData, rTags, magnum, scnRender, commonScene, cameraCtrl, shVisual, uniCore, uniScnFrame, uniTestPlanets);

            OSP_SESSION_UNPACK_TAGS(uniCore, TESTAPP_UNI_CORE);

            setup_magnum_draw(mainView, magnum, commonScene, scnRender, {tgUniTimeEvt});
        };
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


