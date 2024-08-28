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

#include <planet-a/activescene/terrain.h>

#include <osp/activescene/basic.h>
#include <osp/util/logging.h>
#include <osp/framework/framework.h>

using namespace adera;
using namespace osp;
using namespace osp::fw;
using namespace osp::active;
using namespace planeta;
using namespace ftr_inter;

namespace testapp
{



/**
 * Enginetest itself doesn't depend on the framework, but we need to store it somewhere.
 */
FeatureDef const ftrEngineTest = feature_def("EngineTest", [] (FeatureBuilder& rFB, Implement<FIEngineTest> engineTest, DependOn<FIMainApp> mainApp, entt::any data)
{
    auto &rResources = rFB.data_get<Resources>(mainApp.di.resources);
    rFB.data(engineTest.di.bigStruct) = enginetest::make_scene(rResources, entt::any_cast<PkgId>(data));
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
        sceneCB.add_feature(ftrScene);
        sceneCB.add_feature(ftrEngineTest, rTestApp.m_defaultPkg);
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

        ContextId const sceneCtx = rFW.m_contextIds.create();
        rFW.data_get<adera::AppContexts&>(mainApp.di.appContexts).scene = sceneCtx;

        ContextBuilder  sceneCB { sceneCtx, {rTestApp.m_mainContext}, rFW };
        sceneCB.add_feature(ftrScene);
        sceneCB.add_feature(ftrCommonScene, rTestApp.m_defaultPkg);
        sceneCB.add_feature(ftrPhysics);
        sceneCB.add_feature(ftrPhysicsShapes, osp::draw::MaterialId{0});
        sceneCB.add_feature(ftrDroppers);
        sceneCB.add_feature(ftrBounds);

        sceneCB.add_feature(ftrJolt);
        sceneCB.add_feature(ftrJoltConstAccel);
        sceneCB.add_feature(ftrPhysicsShapesJolt);
        ContextBuilder::finalize(std::move(sceneCB));

        ospjolt::ForceFactors_t const gravity = add_constant_acceleration(sc_gravityForce, rFW, sceneCtx);
        set_phys_shape_factors(gravity, rFW, sceneCtx);
        add_floor(rFW, sceneCtx, rTestApp.m_defaultPkg, 4);
    }});



    add_scenario({
        .name        = "vehicles",
        .brief       = "Physics scenario but with Vehicles",
        .description = "Controls (FREECAM):\n"
                       "* [WASD]            - Move camera\n"
                       "* [QE]              - Move camera up/down\n"
                       "Controls (VEHICLE):\n"
                       "* [WS]              - RCS Pitch\n"
                       "* [AD]              - RCS Yaw\n"
                       "* [QE]              - RCS Roll\n"
                       "* [Shift]           - Throttle Up\n"
                       "* [Ctrl]            - Throttle Down\n"
                       "* [Z]               - Throttle Max\n"
                       "* [X]               - Throttle Zero\n"
                       "Controls:\n"
                       "* [Drag MouseRight] - Orbit camera\n"
                       "* [Space]           - Throw spheres\n"
                       "* [V]               - Switch vehicles\n",
        .loadFunc = [] (TestApp& rTestApp)
    {
        auto        &rFW      = rTestApp.m_framework;
        auto  const mainApp   = rFW.get_interface<FIMainApp>  (rTestApp.m_mainContext);


        ContextId const sceneCtx = rFW.m_contextIds.create();
        rFW.data_get<adera::AppContexts&>(mainApp.di.appContexts).scene = sceneCtx;

        ContextBuilder  sceneCB { sceneCtx, {rTestApp.m_mainContext}, rFW };
        sceneCB.add_feature(ftrScene);
        sceneCB.add_feature(ftrCommonScene, rTestApp.m_defaultPkg);
        sceneCB.add_feature(ftrPhysics);
        sceneCB.add_feature(ftrPhysicsShapes);
        sceneCB.add_feature(ftrDroppers);
        sceneCB.add_feature(ftrBounds);

        sceneCB.add_feature(ftrPrefabs);
        sceneCB.add_feature(ftrParts);
        sceneCB.add_feature(ftrSignalsFloat);
        sceneCB.add_feature(ftrVehicleSpawn);
        sceneCB.add_feature(ftrVehicleSpawnVBData);
        sceneCB.add_feature(ftrPrebuiltVehicles);

        sceneCB.add_feature(ftrMachMagicRockets);
        sceneCB.add_feature(ftrMachRCSDriver);

        sceneCB.add_feature(ftrJolt);
        sceneCB.add_feature(ftrJoltConstAccel);
        sceneCB.add_feature(ftrPhysicsShapesJolt);
        sceneCB.add_feature(ftrVehicleSpawnJolt);
        sceneCB.add_feature(ftrRocketThrustJolt);

        ContextBuilder::finalize(std::move(sceneCB));



        ospjolt::ForceFactors_t const gravity = add_constant_acceleration(sc_gravityForce, rFW, sceneCtx);
        set_phys_shape_factors     (gravity, rFW, sceneCtx);
        set_vehicle_default_factors(gravity, rFW, sceneCtx);

        add_floor(rFW, sceneCtx, rTestApp.m_defaultPkg, 4);

        auto vhclSpawn          = rFW.get_interface<FIVehicleSpawn>(sceneCtx);
        auto vhclSpawnVB        = rFW.get_interface<FIVehicleSpawnVB>(sceneCtx);
        auto testVhcls          = rFW.get_interface<FITestVehicles>(sceneCtx);

        auto &rVehicleSpawn     = rFW.data_get<ACtxVehicleSpawn>     (vhclSpawn.di.vehicleSpawn);
        auto &rVehicleSpawnVB   = rFW.data_get<ACtxVehicleSpawnVB>   (vhclSpawnVB.di.vehicleSpawnVB);
        auto &rPrebuiltVehicles = rFW.data_get<PrebuiltVehicles>     (testVhcls.di.prebuiltVehicles);

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

    }});



    add_scenario({
        .name        = "terrain",
        .brief       = "Planet terrain mesh test (Earth-sized planet)",
        .description = "Controls:\n"
                       "* [WASD]            - Move camera\n"
                       "* [QE]              - Move camera up/down\n"
                       "* [Drag MouseRight] - Orbit camera\n",
        .loadFunc = [] (TestApp& rTestApp)
    {
        auto        &rFW      = rTestApp.m_framework;
        auto  const mainApp   = rFW.get_interface<FIMainApp>  (rTestApp.m_mainContext);

        ContextId const sceneCtx = rFW.m_contextIds.create();
        rFW.data_get<adera::AppContexts&>(mainApp.di.appContexts).scene = sceneCtx;

        ContextBuilder  sceneCB { sceneCtx, {rTestApp.m_mainContext}, rFW };
        sceneCB.add_feature(ftrScene);
        sceneCB.add_feature(ftrCommonScene, rTestApp.m_defaultPkg);

        sceneCB.add_feature(ftrTerrain);
        sceneCB.add_feature(ftrTerrainIcosahedron);
        sceneCB.add_feature(ftrTerrainSubdivDist);
        ContextBuilder::finalize(std::move(sceneCB));

        auto terrain = rFW.get_interface<FITerrain>(sceneCtx);
        auto &rTerrain = rFW.data_get<ACtxTerrain>(terrain.di.terrain);
        auto &rTerrainFrame = rFW.data_get<ACtxTerrainFrame>(terrain.di.terrainFrame);

        constexpr std::uint64_t c_earthRadius = 6371000;

        initialize_ico_terrain(rFW, sceneCtx, {
            .radius                 = double(c_earthRadius),
            .height                 = 20000.0,   // Height between Mariana Trench and Mount Everest
            .skelPrecision          = 10,        // 2^10 units = 1024 units = 1 meter
            .skelMaxSubdivLevels    = 19,
            .chunkSubdivLevels      = 4
        });

        // Set scene position relative to planet to be just on the surface
        rTerrainFrame.position = Vector3l{0,0,c_earthRadius} * 1024;
    }});



    add_scenario({
        .name        = "terrain_small",
        .brief       = "Planet terrain mesh test (100m radius planet)",
        .description = "Controls:\n"
                       "* [WASD]            - Move camera\n"
                       "* [QE]              - Move camera up/down\n"
                       "* [Drag MouseRight] - Orbit camera\n",
        .loadFunc = [] (TestApp& rTestApp)
    {
        auto        &rFW      = rTestApp.m_framework;
        auto  const mainApp   = rFW.get_interface<FIMainApp>  (rTestApp.m_mainContext);

        ContextId const sceneCtx = rFW.m_contextIds.create();
        rFW.data_get<adera::AppContexts&>(mainApp.di.appContexts).scene = sceneCtx;

        ContextBuilder  sceneCB { sceneCtx, {rTestApp.m_mainContext}, rFW };
        sceneCB.add_feature(ftrScene);
        sceneCB.add_feature(ftrCommonScene, rTestApp.m_defaultPkg);

        sceneCB.add_feature(ftrTerrain);
        sceneCB.add_feature(ftrTerrainIcosahedron);
        sceneCB.add_feature(ftrTerrainSubdivDist);
        ContextBuilder::finalize(std::move(sceneCB));

        auto terrain = rFW.get_interface<FITerrain>(sceneCtx);
        auto &rTerrain = rFW.data_get<ACtxTerrain>(terrain.di.terrain);
        auto &rTerrainFrame = rFW.data_get<ACtxTerrainFrame>(terrain.di.terrainFrame);

        initialize_ico_terrain(rFW, sceneCtx, {
            .radius                 = 100.0,
            .height                 = 2.0,
            .skelPrecision          = 10, // 2^10 units = 1024 units = 1 meter
            .skelMaxSubdivLevels    = 5,
            .chunkSubdivLevels      = 4
        });

        // Set scene position relative to planet to be just on the surface
        rTerrainFrame.position = Vector3l{0,0,100} * 1024;
    }});



    add_scenario({
        .name        = "universe",
        .brief       = "Universe test scenario with very unrealistic planets",
        .description = "Controls:\n"
                       "* [WASD]            - Move camera\n"
                       "* [QE]              - Move camera up/down\n"
                       "* [Drag MouseRight] - Orbit camera\n"
                       "* [Space]           - Throw spheres\n",
        .loadFunc = [] (TestApp& rTestApp)
    {
        auto        &rFW      = rTestApp.m_framework;
        auto  const mainApp   = rFW.get_interface<FIMainApp>  (rTestApp.m_mainContext);

        ContextId const sceneCtx = rFW.m_contextIds.create();
        rFW.data_get<adera::AppContexts&>(mainApp.di.appContexts).scene = sceneCtx;

        ContextBuilder  sceneCB { sceneCtx, {rTestApp.m_mainContext}, rFW };
        sceneCB.add_feature(ftrScene);
        sceneCB.add_feature(ftrCommonScene, rTestApp.m_defaultPkg);
        sceneCB.add_feature(ftrPhysics);
        sceneCB.add_feature(ftrPhysicsShapes, osp::draw::MaterialId{0});
        sceneCB.add_feature(ftrDroppers);
        sceneCB.add_feature(ftrBounds);

        sceneCB.add_feature(ftrJolt);
        sceneCB.add_feature(ftrJoltConstAccel);
        sceneCB.add_feature(ftrPhysicsShapesJolt);

        auto const scene = rFW.get_interface<FIScene>(sceneCtx);

        sceneCB.add_feature(ftrUniverseCore, PipelineId{mainApp.pl.mainLoop});
        sceneCB.add_feature(ftrUniverseSceneFrame);
        sceneCB.add_feature(ftrUniverseTestPlanets);
        ContextBuilder::finalize(std::move(sceneCB));

        ospjolt::ForceFactors_t const gravity = add_constant_acceleration(sc_gravityForce, rFW, sceneCtx);
        set_phys_shape_factors(gravity, rFW, sceneCtx);
        add_floor(rFW, sceneCtx, rTestApp.m_defaultPkg, 0);
    }});



    add_scenario({
        .name        = "solar-system",
        .brief       = "Scenario that simulates a basic solar system.",
        .description = "Controls:\n"
                       "* [WASD]            - Move camera\n"
                       "* [QE]              - Move camera up/down\n"
                       "* [Drag MouseRight] - Orbit camera\n",
        .loadFunc = [] (TestApp& rTestApp)
    {
        auto        &rFW      = rTestApp.m_framework;
        auto  const mainApp   = rFW.get_interface<FIMainApp>  (rTestApp.m_mainContext);

        ContextId const sceneCtx = rFW.m_contextIds.create();
        rFW.data_get<adera::AppContexts&>(mainApp.di.appContexts).scene = sceneCtx;

        ContextBuilder  sceneCB { sceneCtx, {rTestApp.m_mainContext}, rFW };
        sceneCB.add_feature(ftrScene);
        sceneCB.add_feature(ftrCommonScene, rTestApp.m_defaultPkg);

        auto const scene = rFW.get_interface<FIScene>(sceneCtx);

        sceneCB.add_feature(ftrUniverseCore, PipelineId{scene.pl.update});
        sceneCB.add_feature(ftrUniverseSceneFrame);
        sceneCB.add_feature(ftrSolarSystem);
        ContextBuilder::finalize(std::move(sceneCB));
    }});

    return scenarioMap;
}

ScenarioMap_t const& scenarios()
{
    static ScenarioMap_t s_scenarioMap = make_scenarios();
    return s_scenarioMap;
}

} // namespace testapp


