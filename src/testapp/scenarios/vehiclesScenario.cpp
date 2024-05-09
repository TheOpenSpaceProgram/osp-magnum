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

#include "vehiclesScenario.h"
#include "scenarioUtils.h"

#include "adera/activescene/vehicles_vb_fn.h"

#include "testapp/sessions/magnum.h"
#include "testapp/sessions/misc.h"
#include "testapp/sessions/newton.h"
#include "testapp/sessions/physics.h"
#include "testapp/sessions/shapes.h"
#include "testapp/sessions/vehicles.h"
#include "testapp/sessions/vehicles_machines.h"
#include "testapp/sessions/vehicles_prebuilt.h"
#include <testapp/sessions/common.h>

namespace testapp
{

Scenario create_vehicles_scenario()
{
    using namespace osp;
    using namespace osp::active;

    Scenario vehicleScenario{};
    vehicleScenario.name = "vehicles";
    vehicleScenario.description = "Physics scenario but with Vehicles";
    vehicleScenario.setupFunction = [](TestApp& rTestApp) -> RendererSetupFunc_t
    {
		#define SCENE_SESSIONS  scene, commonScene, physics, physShapes, droppers, bounds, newton, nwtGravSet, nwtGrav, physShapesNwt, \
                                prefabs, parts, vehicleSpawn, signalsFloat, \
                                vehicleSpawnVB, vehicleSpawnRgd, vehicleSpawnNwt, \
                                testVehicles, machRocket, machRcsDriver, nwtRocketSet, rocketsNwt
		#define RENDERER_SESSIONS   sceneRenderer, magnumScene, cameraCtrl, shVisual, shFlat, shPhong, camThrow, shapeDraw, cursor, \
                                prefabDraw, vehicleDraw, vehicleCtrl, cameraVehicle, thrustIndicator

        using namespace testapp::scenes;

        auto const  defaultPkg = rTestApp.m_defaultPkg;
        auto const  application = rTestApp.m_application;
        auto& rTopData = rTestApp.m_topData;

        TopTaskBuilder builder{ rTestApp.m_tasks, rTestApp.m_scene.m_edges, rTestApp.m_taskData };

        auto& [SCENE_SESSIONS] = resize_then_unpack<22>(rTestApp.m_scene.m_sessions);

        scene = setup_scene(builder, rTopData, application);
        commonScene = setup_common_scene(builder, rTopData, scene, application, defaultPkg);
        physics = setup_physics(builder, rTopData, scene, commonScene);
        physShapes = setup_phys_shapes(builder, rTopData, scene, commonScene, physics, sc_matPhong);
        droppers = setup_droppers(builder, rTopData, scene, commonScene, physShapes);
        bounds = setup_bounds(builder, rTopData, scene, commonScene, physShapes);

        prefabs = setup_prefabs(builder, rTopData, application, scene, commonScene, physics);
        parts = setup_parts(builder, rTopData, application, scene);
        signalsFloat = setup_signals_float(builder, rTopData, scene, parts);
        vehicleSpawn = setup_vehicle_spawn(builder, rTopData, scene);
        vehicleSpawnVB = setup_vehicle_spawn_vb(builder, rTopData, application, scene, commonScene, prefabs, parts, vehicleSpawn, signalsFloat);
        testVehicles = setup_prebuilt_vehicles(builder, rTopData, application, scene);

        machRocket = setup_mach_rocket(builder, rTopData, scene, parts, signalsFloat);
        machRcsDriver = setup_mach_rcsdriver(builder, rTopData, scene, parts, signalsFloat);

        newton = setup_newton(builder, rTopData, scene, commonScene, physics);
        nwtGravSet = setup_newton_factors(builder, rTopData);
        nwtGrav = setup_newton_force_accel(builder, rTopData, newton, nwtGravSet, sc_gravityForce);
        physShapesNwt = setup_phys_shapes_newton(builder, rTopData, commonScene, physics, physShapes, newton, nwtGravSet);
        vehicleSpawnNwt = setup_vehicle_spawn_newton(builder, rTopData, application, commonScene, physics, prefabs, parts, vehicleSpawn, newton);
        nwtRocketSet = setup_newton_factors(builder, rTopData);
        rocketsNwt = setup_rocket_thrust_newton(builder, rTopData, scene, commonScene, physics, prefabs, parts, signalsFloat, newton, nwtRocketSet);

        OSP_DECLARE_GET_DATA_IDS(vehicleSpawn, TESTAPP_DATA_VEHICLE_SPAWN);
        OSP_DECLARE_GET_DATA_IDS(vehicleSpawnVB, TESTAPP_DATA_VEHICLE_SPAWN_VB);
        OSP_DECLARE_GET_DATA_IDS(testVehicles, TESTAPP_DATA_TEST_VEHICLES);

        auto& rVehicleSpawn = top_get<ACtxVehicleSpawn>(rTopData, idVehicleSpawn);
        auto& rVehicleSpawnVB = top_get<adera::ACtxVehicleSpawnVB>(rTopData, idVehicleSpawnVB);
        auto& rPrebuiltVehicles = top_get<PrebuiltVehicles>(rTopData, idPrebuiltVehicles);

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

        RendererSetupFunc_t const setup_renderer = [](TestApp& rTestApp)
        {
            auto const  application = rTestApp.m_application;
            auto const  windowApp = rTestApp.m_windowApp;
            auto const  magnum = rTestApp.m_magnum;
            auto const  defaultPkg = rTestApp.m_defaultPkg;
            auto& rTopData = rTestApp.m_topData;

            TopTaskBuilder builder{ rTestApp.m_tasks, rTestApp.m_renderer.m_edges, rTestApp.m_taskData };

            auto& [SCENE_SESSIONS] = unpack<22>(rTestApp.m_scene.m_sessions);
            auto& [RENDERER_SESSIONS] = resize_then_unpack<14>(rTestApp.m_renderer.m_sessions);

            sceneRenderer = setup_scene_renderer(builder, rTopData, application, windowApp, commonScene);
            create_materials(rTopData, sceneRenderer, sc_materialCount);

            magnumScene = setup_magnum_scene(builder, rTopData, application, windowApp, sceneRenderer, magnum, scene, commonScene);
            cameraCtrl = setup_camera_ctrl(builder, rTopData, windowApp, sceneRenderer, magnumScene);
            shVisual = setup_shader_visualizer(builder, rTopData, windowApp, sceneRenderer, magnum, magnumScene, sc_matVisualizer);
            shFlat = setup_shader_flat(builder, rTopData, windowApp, sceneRenderer, magnum, magnumScene, sc_matFlat);
            shPhong = setup_shader_phong(builder, rTopData, windowApp, sceneRenderer, magnum, magnumScene, sc_matPhong);
            camThrow = setup_thrower(builder, rTopData, windowApp, cameraCtrl, physShapes);
            shapeDraw = setup_phys_shapes_draw(builder, rTopData, windowApp, sceneRenderer, commonScene, physics, physShapes);
            cursor = setup_cursor(builder, rTopData, application, sceneRenderer, cameraCtrl, commonScene, sc_matFlat, rTestApp.m_defaultPkg);
            prefabDraw = setup_prefab_draw(builder, rTopData, application, windowApp, sceneRenderer, commonScene, prefabs, sc_matPhong);
            vehicleDraw = setup_vehicle_spawn_draw(builder, rTopData, sceneRenderer, vehicleSpawn);
            vehicleCtrl = setup_vehicle_control(builder, rTopData, windowApp, scene, parts, signalsFloat);
            cameraVehicle = setup_camera_vehicle(builder, rTopData, windowApp, scene, sceneRenderer, commonScene, physics, parts, cameraCtrl, vehicleCtrl);
            thrustIndicator = setup_thrust_indicators(builder, rTopData, application, windowApp, commonScene, parts, signalsFloat, sceneRenderer, defaultPkg, sc_matFlat);

            setup_magnum_draw(rTestApp, scene, sceneRenderer, magnumScene);
        };

		#undef SCENE_SESSIONS
		#undef RENDERER_SESSIONS

        return setup_renderer;
    };

    return vehicleScenario;
}

} // namespace testapp