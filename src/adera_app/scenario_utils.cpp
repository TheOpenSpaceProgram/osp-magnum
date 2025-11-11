/**
 * Open Space Program
 * Copyright © 2019-2025 Open Space Program Project
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
#include "scenario_utils.h"
#include "feature_interfaces.h"

#include "features/shapes.h"
#include "features/universe_sims.h"

#include <adera/universe_demo/simulations.h>
#include <adera/universe_demo/simulations_glue.h>

#include <osp/activescene/physics.h>
#include <osp/drawing/drawing.h>

#include <osp/core/math_2pow.h>

#include <random>

using namespace adera;
using namespace ftr_inter::stages;
using namespace ftr_inter;
using namespace osp::active;
using namespace osp::universe;
using namespace osp::draw;
using namespace osp::fw;
using namespace osp;

void adera::add_floor(Framework &rFW, ContextId sceneCtx, PkgId pkg, int size)
{
    auto const physShapes = rFW.get_interface<FIPhysShapes>(sceneCtx);

    auto &rPhysShapes = rFW.data_get<ACtxPhysShapes>(physShapes.di.physShapes);

    std::mt19937 randGen(69);
    auto distSizeX  = std::uniform_real_distribution<float>{20.0, 80.0};
    auto distSizeY  = std::uniform_real_distribution<float>{20.0, 80.0};
    auto distHeight = std::uniform_real_distribution<float>{1.0, 10.0};

    constexpr float spread      = 128.0f;

    for (int x = -size; x < size+1; ++x)
    {
        for (int y = -size; y < size+1; ++y)
        {
            float const heightZ = distHeight(randGen);
            rPhysShapes.m_spawnRequest.emplace_back(SpawnShape{
                .m_position = Vector3{float(x)*spread, float(y)*spread, heightZ},
                .m_velocity = {0.0f, 0.0f, 0.0f},
                .m_size     = Vector3{distSizeX(randGen), distSizeY(randGen), heightZ},
                .m_mass     = 0.0f,
                .m_shape    = EShape::Box
            });
        }
    }
} // adera::add_floor


void adera::setup_uni_solar_system(Framework &rFW, ContextId sceneCtx)
{
    using CoSpaceIdVec_t = std::vector<CoSpaceId>;

    auto const uniCore          = rFW.get_interface<FIUniCore>          (sceneCtx);
    auto const uniTransfers     = rFW.get_interface<FIUniTransfers>     (sceneCtx);
    auto const uniSimpleSims    = rFW.get_interface<FIUniSimpleSims>    (sceneCtx);
    auto const uniScenes        = rFW.get_interface<FIUniScenes>        (sceneCtx);
    auto const scnInUni         = rFW.get_interface<FISceneInUniverse>  (sceneCtx);

    auto &rCoordSpaces      = rFW.data_get< UCtxCoordSpaces >       (uniCore.di.coordSpaces);
    auto &rCompTypes        = rFW.data_get< UCtxComponentTypes >    (uniCore.di.compTypes);
    auto &rDataAccessors    = rFW.data_get< UCtxDataAccessors >     (uniCore.di.dataAccessors);
    auto &rStolenSats       = rFW.data_get< UCtxStolenSatellites >  (uniCore.di.stolenSats);
    auto &rDataSrcs         = rFW.data_get< UCtxDataSources >       (uniCore.di.dataSrcs);
    auto &rSatInst          = rFW.data_get< UCtxSatellites >        (uniCore.di.satInst);
    auto &rSimulations      = rFW.data_get< UCtxSimulations >       (uniCore.di.simulations);
    auto &rIntakes          = rFW.data_get< UCtxIntakes >           (uniTransfers.di.intakes);
    auto &rTransferBufs     = rFW.data_get< UCtxTransferBuffers >   (uniTransfers.di.transferBufs);
    auto &rCirclePath       = rFW.data_get< UCtxCirclePathSims >    (uniSimpleSims.di.circlePath);
    auto &rConstantSpin     = rFW.data_get< UCtxConstantSpinSims >  (uniSimpleSims.di.constantSpin);
    auto &rSimpleGravity    = rFW.data_get< UCtxSimpleGravitySims > (uniSimpleSims.di.simpleGravity);
    auto &rScenes           = rFW.data_get< UCtxScenes >            (uniScenes.di.scenes);
    auto &rSceneId          = rFW.data_get< SceneId >               (scnInUni.di.sceneId);

    CoSpaceId const rootSpace = rCoordSpaces.ids.create();
    rCoordSpaces.resize();
    rCoordSpaces.insert({}, rootSpace);

    SimpleGravitySimId const rootSimBId = rSimpleGravity.ids.create();
    rSimpleGravity.instOf.resize(rSimpleGravity.ids.size());
    SimulationId const simBId = rSimulations.ids.create();

    UCtxSimpleGravitySims::Instance &rSimInstB = rSimpleGravity.instOf[rootSimBId];

    DefaultComponents const &dc = rCompTypes.defaults;
    ComponentTypeIdSet_t const intakeComps = component_type_set({
            dc.satId,
            dc.posX, dc.posY, dc.posZ,
            dc.velXd, dc.velYd, dc.velZd,
            dc.accelX, dc.accelY, dc.accelZ});

    rSimInstB = UCtxSimpleGravitySims::Instance{
        .sim = SimpleGravitySim{
            .m_metersPerPosUnit = 1.0/1024.0,
            .m_secPerTimeUnit   = 0.001
        },
        .simId             = simBId,
        .updateInterval    = 15,
        .accessorId        = rDataAccessors.ids.create(),
        .cospaceId         = rootSpace,
        .intakeId          = rIntakes.make_intake(simBId, rootSpace, intakeComps)
    };

    auto const add_body = [&rSimInstB, &rSatInst] (Vector3g position, Vector3d velocity, Quaternion rotation,
                                        float mass, float radius, Magnum::Color3 color)
    {
        rSimInstB.sim.m_data.push_back(SimpleGravitySim::SatData{
            .position   = position,
            .velocity   = velocity,
            .accel      = {},
            .mass       = mass,
            .id = rSatInst.ids.create()
        });
    };

    constexpr int precision = 10;

    // Sun
    add_body(
        { 0, 0, 0 },
        { 0.0, 0.0, 0.0 },
        Quaternion::rotation(Rad{ 0.0f }, Vector3{ 1.0f, 0.0f, 0.0f }),
        1.0f * std::pow(10.0f, 1.0f),
        1000.0f,
        { 1.0f, 1.0f, 0.0f });

    // Blue Planet
    add_body(
        { 0, math::mul_2pow<spaceint_t, int>(10, precision), 0 },
        { 1.0, 0.0, 0.0 },
        Quaternion::rotation(Rad{ 0.0f }, Vector3{ 1.0f, 0.0f, 0.0f }),
        0.0000000001f,
        500.0f,
        { 0.0f, 0.0f, 1.0f });

    // Red Planet
    add_body(
        { 0, math::mul_2pow<spaceint_t, int>(5, precision), 0 },
        { 1.414213562, 0.0, 0.0 },
        Quaternion::rotation(Rad{ 0.0f }, Vector3{ 1.0f, 0.0f, 0.0f }),
        0.0000000001f,
        250.0f,
        { 1.0f, 0.0f, 0.0f });

    // Green Planet
    add_body(
        { 0, math::mul_2pow<spaceint_t, int>(7, precision), 0 },
        { 1.154700538, 0.0, 0.0 },
        Quaternion::rotation(Rad{ 0.0f }, Vector3{ 1.0f, 0.0f, 0.0f }),
        0.0000000001f,
        600.0f,
        { 0.0f, 1.0f, 0.0f });

    // Orange Planet
    add_body(
        { 0, math::mul_2pow<spaceint_t, int>(12, precision), 0 },
        { 0.912870929, 0.0, 0.0 },
        Quaternion::rotation(Rad{ 0.0f }, Vector3{ 1.0f, 0.0f, 0.0f }),
        0.0000000001f,
        550.0f,
        { 1.0f, 0.5f, 0.0f });

    IntakeId const intakeId = rIntakes.find_intake_at(rootSpace, intakeComps);
    LGRN_ASSERT(intakeId.has_value());

    // Setup coordinate space used by Scene-In-Universe system
    CoSpaceId const sceneSpace = rCoordSpaces.ids.create();
    rCoordSpaces.resize();
    rCoordSpaces.insert(rootSpace, sceneSpace);

    rSceneId = rScenes.ids.create();
    rScenes.connectionOf.resize(rScenes.ids.capacity());
    rScenes.connectionOf[rSceneId].cospace = sceneSpace;

    rSimulations.simulationOf   .resize(rSimulations.ids.capacity());
    rTransferBufs.midTransfersOf.resize(rSimulations.ids.capacity());

    rStolenSats.of.resize(rDataAccessors.ids.capacity());

} // adera::add_solar_system_test


void adera::setup_uni_cospace_test(Framework &rFW, ContextId sceneCtx)
{
    using CoSpaceIdVec_t = std::vector<CoSpaceId>;

    auto const uniCore          = rFW.get_interface<FIUniCore>          (sceneCtx);
    auto const uniTransfers     = rFW.get_interface<FIUniTransfers>     (sceneCtx);
    auto const uniSimpleSims    = rFW.get_interface<FIUniSimpleSims>    (sceneCtx);
    auto const uniScenes        = rFW.get_interface<FIUniScenes>        (sceneCtx);
    auto const scnInUni         = rFW.get_interface<FISceneInUniverse>  (sceneCtx);

    auto &rCoordSpaces      = rFW.data_get< UCtxCoordSpaces >       (uniCore.di.coordSpaces);
    auto &rCompTypes        = rFW.data_get< UCtxComponentTypes >    (uniCore.di.compTypes);
    auto &rDataAccessors    = rFW.data_get< UCtxDataAccessors >     (uniCore.di.dataAccessors);
    auto &rStolenSats       = rFW.data_get< UCtxStolenSatellites >  (uniCore.di.stolenSats);
    auto &rDataSrcs         = rFW.data_get< UCtxDataSources >       (uniCore.di.dataSrcs);
    auto &rSatInst          = rFW.data_get< UCtxSatellites >        (uniCore.di.satInst);
    auto &rSimulations      = rFW.data_get< UCtxSimulations >       (uniCore.di.simulations);
    auto &rIntakes          = rFW.data_get< UCtxIntakes >           (uniTransfers.di.intakes);
    auto &rTransferBufs     = rFW.data_get< UCtxTransferBuffers >   (uniTransfers.di.transferBufs);
    auto &rScenes           = rFW.data_get< UCtxScenes >            (uniScenes.di.scenes);

    auto &rCirclePath       = rFW.data_get< UCtxCirclePathSims >    (uniSimpleSims.di.circlePath);
    auto &rConstantSpinSims = rFW.data_get< UCtxConstantSpinSims >  (uniSimpleSims.di.constantSpin);
    auto &rSimpleGravity    = rFW.data_get< UCtxSimpleGravitySims > (uniSimpleSims.di.simpleGravity);
    auto &rSceneId          = rFW.data_get< SceneId >               (scnInUni.di.sceneId);

    CoSpaceId const rootSpace  = rCoordSpaces.ids.create();
    rCoordSpaces.resize();
    rCoordSpaces.insert({}, rootSpace);

    constexpr int seed = 328;
    std::mt19937 gen(seed);

    auto add_circle_orbit = [&rCirclePath, &rSimulations, &rDataAccessors, &rCoordSpaces, &rSatInst, &gen, dist = std::uniform_real_distribution<double>(0.0, 1.0)]
            (CoSpaceId parentCospace, SatelliteId parentSat, Quaterniond rot, double minR, double maxR, double GM, std::initializer_list<double> dists) mutable -> CirclePathSimId
    {
        CirclePathSimId const circleSimId = rCirclePath.ids.create();
        rCirclePath.instOf.resize(rCirclePath.ids.size());

        UCtxCirclePathSims::Instance &rCircleSim = rCirclePath.instOf[circleSimId];
        rCircleSim = UCtxCirclePathSims::Instance{
            .simId             = rSimulations.ids.create(),
            .updateInterval    = 15,
            .accessorId        = rDataAccessors.ids.create(),
            .cospaceId         = parentCospace,
        };

        if (parentSat.has_value())
        {
            rCircleSim.cospaceId = rCoordSpaces.ids.create();
            auto const cospaceCapacity = rCoordSpaces.ids.capacity();
            rCoordSpaces.resize();
            rCoordSpaces.transformOf[rCircleSim.cospaceId].parentSat = parentSat;
            rCoordSpaces.transformOf[rCircleSim.cospaceId].rotation = rot;
            rCoordSpaces.insert(parentCospace, rCircleSim.cospaceId);
        }
        else
        {
            rCircleSim.cospaceId = parentCospace;
        }

        rCircleSim.sim.m_data.resize(dists.size());
        for (int i = 0; i < dists.size(); ++i)
        {
            double const r = *(dists.begin() + i) * 1000.0;
            double const T = (r == 0.0) ? (123456.0) : (2 * 3.1415926536 * std::sqrt(r*r*r / GM) * 1000.0);

            CirclePathSim::SatData &rSatData = rCircleSim.sim.m_data[i];
            rSatData = CirclePathSim::SatData{
                .radius     = r,
                .period     = std::uint64_t(T),
                .cycleTime  = std::uint64_t(dist(gen) * T),
                .id         = rSatInst.ids.create()
            };
        }
        return circleSimId;
    };

    CirclePathSimId const circleSimId = add_circle_orbit(rootSpace, {}, {}, 10.0*1024.0, 100.0*1024.0, 2000000000000.0,
    {
        0.0, 5.0, 20.0, 30.0, 38.0, 49.0, 60.0, 85.0, 90.0, 110.0
    });

    CirclePathSimId const fnslfalfl = add_circle_orbit(rootSpace, rCirclePath.instOf[circleSimId].sim.m_data[4].id, Quaterniond({1.0, 0.0, 0.0}, 0.69*3.1415926536), 10.0*1024.0, 100.0*1024.0, 2000000000000.0,
    {
        2.0, 4.0
    });


    // Setup coordinate space used by Scene-In-Universe system
    CoSpaceId const sceneSpace = rCoordSpaces.ids.create();
    rCoordSpaces.resize();
    rCoordSpaces.transformOf[sceneSpace].parentSat = rCirclePath.instOf[circleSimId].sim.m_data[5].id;
    rCoordSpaces.insert(rootSpace, sceneSpace);


    rSceneId = rScenes.ids.create();
    rScenes.connectionOf.resize(rScenes.ids.capacity());
    rScenes.connectionOf[rSceneId].cospace = sceneSpace;

    rSimulations.simulationOf   .resize(rSimulations.ids.capacity());
    rTransferBufs.midTransfersOf.resize(rSimulations.ids.capacity());

    rStolenSats.of.resize(rDataAccessors.ids.capacity());



}

