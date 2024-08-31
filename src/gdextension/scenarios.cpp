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

#include <adera_app/application.h>

#include <adera_app/features/common.h>
#include <adera_app/features/jolt.h>
#include <adera_app/features/misc.h>
#include <adera_app/features/physics.h>
#include <adera_app/features/shapes.h>
#include <adera_app/features/terrain.h>
#include <adera_app/features/universe.h>
#include <adera_app/features/vehicles.h>
#include <adera_app/features/vehicles_machines.h>
#include <adera_app/features/vehicles_prebuilt.h>

#include <adera/activescene/vehicles_vb_fn.h>

#include <godot_cpp/variant/utility_functions.hpp>
using namespace adera;
using namespace ftr_inter;
using namespace osp::active;
using namespace osp::fw;
using namespace osp;

namespace ospgdext
{


static ScenarioMap_t make_scenarios()
{
    ScenarioMap_t scenarioMap;

    auto const add_scenario = [&scenarioMap] (ScenarioOption scenario)
    {
        scenarioMap.emplace(scenario.name, scenario);
    };

    static constexpr auto sc_gravityForce = Vector3{ 0.0f, 0.0f, -9.81f };

    add_scenario({
        .name        = "physics",
        .brief       = "Jolt Physics engine integration test",
        .description = "Controls:\n"
                       "* [WASD]            - Move camera\n"
                       "* [QE]              - Move camera up/down\n"
                       "* [Drag MouseRight] - Orbit camera\n"
                       "* [Space]           - Throw spheres\n",
        .loadFunc = [] (osp::fw::Framework &rFW, osp::fw::ContextId mainCtx, osp::PkgId pkg)
    {
        auto  const mainApp   = rFW.get_interface<FIMainApp>  (mainCtx);

        ContextId const sceneCtx = rFW.m_contextIds.create();
        rFW.data_get<adera::AppContexts&>(mainApp.di.appContexts).scene = sceneCtx;

        ContextBuilder  sceneCB { sceneCtx, {mainCtx}, rFW };
        sceneCB.add_feature(ftrScene);
        sceneCB.add_feature(ftrCommonScene, pkg);
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
        add_floor(rFW, sceneCtx, pkg, 4);
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
        .loadFunc = [] (osp::fw::Framework &rFW, osp::fw::ContextId mainCtx, osp::PkgId pkg)
    {
        auto  const mainApp   = rFW.get_interface<FIMainApp>  (mainCtx);


        ContextId const sceneCtx = rFW.m_contextIds.create();
        rFW.data_get<adera::AppContexts&>(mainApp.di.appContexts).scene = sceneCtx;

        ContextBuilder  sceneCB { sceneCtx, {mainCtx}, rFW };
        sceneCB.add_feature(ftrScene);
        sceneCB.add_feature(ftrCommonScene, pkg);
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

        add_floor(rFW, sceneCtx, pkg, 4);

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

    return scenarioMap;
}

ScenarioMap_t const &scenarios()
{
    static ScenarioMap_t s_scenarioMap = make_scenarios();
    return s_scenarioMap;
}

} // namespace ospgdext
