#include "lander.h"
#include "../testapp.h"
#include "../identifiers.h"
#include "../sessions/common.h"
#include "../sessions/magnum.h"
#include "../sessions/misc.h"
#include "../sessions/newton.h"
#include "../sessions/physics.h"
#include "../sessions/shapes.h"
#include "../sessions/terrain.h"
#include "../sessions/universe.h"
#include "../sessions/vehicles.h"
#include "../sessions/vehicles_machines.h"
#include "../sessions/vehicles_prebuilt.h"
#include "../scenarios.h"

#include <osp/tasks/top_utils.h>

#include <adera/activescene/vehicles_vb_fn.h>

namespace lander
{
    // MaterialIds hints which shaders should be used to draw a DrawEnt
    // DrawEnts can be assigned to multiple materials
    static constexpr auto sc_matVisualizer = osp::draw::MaterialId(0);
    static constexpr auto sc_matFlat = osp::draw::MaterialId(1);
    static constexpr auto sc_matPhong = osp::draw::MaterialId(2);
    static constexpr int sc_materialCount = 4;

    RendererSetupFunc_t lander::setup_lander_scenario(TestApp &rTestApp)
    {
#define SCENE_SESSIONS scene, commonScene, uniCore, uniScnFrame, uniPlanet, physics,         \
                       prefabs, parts, signalsFloat, vehicleSpawn, vehicleSpawnVB, vehicles, \
                       newton, vehicleSpawnNwt, nwtRocketSet, rocketsNwt,                    \
                       machRocket, machRcsDriver
#define SCENE_SESSIONS_COUNT 18
#define RENDERER_SESSIONS sceneRenderer, magnumScene, planetDraw,            \
                          cameraCtrl, cameraFree, shVisual, shFlat, shPhong, \
                          prefabDraw, vehicleDraw, vehicleCtrl, cameraVehicle
#define RENDERER_SESSIONS_COUNT 12

        using namespace testapp::scenes;
        using adera::ACtxVehicleSpawnVB;
        using osp::top_get;
        using osp::TopTaskBuilder;
        using osp::active::ACtxVehicleSpawn;
        using testapp::PlApplication;
        // using osp::active::ad;

        auto const defaultPkg = rTestApp.m_defaultPkg;
        auto const application = rTestApp.m_application;
        auto &rTopData = rTestApp.m_topData;

        TopTaskBuilder builder{rTestApp.m_tasks, rTestApp.m_scene.m_edges, rTestApp.m_taskData};

        auto &[SCENE_SESSIONS] = resize_then_unpack<SCENE_SESSIONS_COUNT>(rTestApp.m_scene.m_sessions);

        scene = setup_scene(builder, rTopData, application);
        commonScene = setup_common_scene(builder, rTopData, scene, application, defaultPkg);

        auto const tgApp = application.get_pipelines<PlApplication>();
        uniCore = setup_uni_core(builder, rTopData, tgApp.mainLoop);
        uniScnFrame = setup_uni_sceneframe(builder, rTopData, uniCore);
        uniPlanet = setup_uni_landerplanet(builder, rTopData, uniCore, uniScnFrame);

        physics = setup_physics(builder, rTopData, scene, commonScene);
        prefabs = setup_prefabs(builder, rTopData, application, scene, commonScene, physics);
        parts = setup_parts(builder, rTopData, application, scene);
        signalsFloat = setup_signals_float(builder, rTopData, scene, parts);
        vehicleSpawn = setup_vehicle_spawn(builder, rTopData, scene);
        vehicleSpawnVB = setup_vehicle_spawn_vb(builder, rTopData, application, scene, commonScene, prefabs, parts, vehicleSpawn, signalsFloat);
        vehicles = setup_prebuilt_vehicles(builder, rTopData, application, scene);

        machRocket = setup_mach_rocket(builder, rTopData, scene, parts, signalsFloat);
        machRcsDriver = setup_mach_rcsdriver(builder, rTopData, scene, parts, signalsFloat);

        newton = setup_newton(builder, rTopData, scene, commonScene, physics);
        vehicleSpawnNwt = setup_vehicle_spawn_newton(builder, rTopData, application, commonScene, physics, prefabs, parts, vehicleSpawn, newton);
        nwtRocketSet = setup_newton_factors(builder, rTopData);
        rocketsNwt = setup_rocket_thrust_newton(builder, rTopData, scene, commonScene, physics, prefabs, parts, signalsFloat, newton, nwtRocketSet);

        OSP_DECLARE_GET_DATA_IDS(vehicleSpawn, TESTAPP_DATA_VEHICLE_SPAWN);
        OSP_DECLARE_GET_DATA_IDS(vehicleSpawnVB, TESTAPP_DATA_VEHICLE_SPAWN_VB);
        OSP_DECLARE_GET_DATA_IDS(vehicles, TESTAPP_DATA_TEST_VEHICLES);

        auto &rVehicleSpawn = top_get<ACtxVehicleSpawn>(rTopData, idVehicleSpawn);
        auto &rVehicleSpawnVB = top_get<ACtxVehicleSpawnVB>(rTopData, idVehicleSpawnVB);
        auto &rPrebuiltVehicles = top_get<PrebuiltVehicles>(rTopData, idPrebuiltVehicles);

        rVehicleSpawn.spawnRequest.push_back(
            {.position = {30.0f, 0.0f, 0.0f},
             .velocity = {0.0f, 0.0f, 0.0f},
             .rotation = {}});
        rVehicleSpawnVB.dataVB.push_back(rPrebuiltVehicles[gc_pbvSimpleCommandServiceModule].get());

        RendererSetupFunc_t const setup_renderer = [](TestApp &rTestApp) -> void
        {
            auto const application = rTestApp.m_application;
            auto const windowApp = rTestApp.m_windowApp;
            auto const magnum = rTestApp.m_magnum;
            auto &rTopData = rTestApp.m_topData;

            TopTaskBuilder builder{rTestApp.m_tasks, rTestApp.m_renderer.m_edges, rTestApp.m_taskData};

            auto &[SCENE_SESSIONS] = unpack<SCENE_SESSIONS_COUNT>(rTestApp.m_scene.m_sessions);
            auto &[RENDERER_SESSIONS] = resize_then_unpack<RENDERER_SESSIONS_COUNT>(rTestApp.m_renderer.m_sessions);

            sceneRenderer = setup_scene_renderer(builder, rTopData, application, windowApp, commonScene);
            create_materials(rTopData, sceneRenderer, sc_materialCount);

            magnumScene = setup_magnum_scene(builder, rTopData, application, rTestApp.m_windowApp, sceneRenderer, rTestApp.m_magnum, scene, commonScene);
            cameraCtrl = setup_camera_ctrl(builder, rTopData, windowApp, sceneRenderer, magnumScene);
            // cameraFree      = setup_camera_free         (builder, rTopData, windowApp, scene, cameraCtrl);
            shVisual = setup_shader_visualizer(builder, rTopData, windowApp, sceneRenderer, magnum, magnumScene, sc_matVisualizer);
            // shFlat          = setup_shader_flat         (builder, rTopData, windowApp, sceneRenderer, magnum, magnumScene, sc_matFlat);
            shPhong = setup_shader_phong(builder, rTopData, windowApp, sceneRenderer, magnum, magnumScene, sc_matPhong);
            planetDraw = setup_landerplanet_draw(builder, rTopData, windowApp, sceneRenderer, cameraCtrl, commonScene, uniCore, uniScnFrame, uniPlanet, sc_matVisualizer, sc_matFlat);

            prefabDraw = setup_prefab_draw(builder, rTopData, application, windowApp, sceneRenderer, commonScene, prefabs, sc_matPhong);
            vehicleDraw = setup_vehicle_spawn_draw(builder, rTopData, sceneRenderer, vehicleSpawn);
            vehicleCtrl = setup_vehicle_control(builder, rTopData, windowApp, scene, parts, signalsFloat);
            cameraVehicle = setup_camera_vehicle(builder, rTopData, windowApp, scene, sceneRenderer, commonScene, physics, parts, cameraCtrl, vehicleCtrl);

            setup_magnum_draw(rTestApp, scene, sceneRenderer, magnumScene);
        };
        return setup_renderer;
    }

} // namespace lander