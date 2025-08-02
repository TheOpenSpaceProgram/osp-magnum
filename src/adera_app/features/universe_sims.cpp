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
#include "universe_sims.h"

#include "../feature_interfaces.h"

#include <adera/universe_demo/simulations.h>
#include <adera/drawing/CameraController.h>

#include <osp/core/math_2pow.h>
#include <osp/drawing/drawing.h>
#include <osp/universe/coordinates.h>
#include <osp/universe/universe.h>
#include <osp/util/logging.h>

using namespace adera;
using namespace ftr_inter::stages;
using namespace ftr_inter;
using namespace osp::draw;
using namespace osp::fw;
using namespace osp::universe;
using namespace osp;

using Corrade::Containers::Array;

namespace adera
{


using CirclePathSimId   = osp::StrongId< std::uint32_t, struct DummyForCirclePathSimId >;
using ConstantSpinSimId = osp::StrongId< std::uint32_t, struct DummyForConstantSpinSimId >;
using SimpleGravitySimId = osp::StrongId< std::uint32_t, struct DummyForSimpleGravitySimId >;

// simple simulators only have 1 buffer / accessor

struct UCtxCirclePathSims
{
    struct Instance
    {
        SimulationId            simId;
        CirclePathSim           sim;
        std::int64_t            updateInterval;
        DataAccessorId          accessorId;
        CoSpaceId               cospaceId;
        IntakeId                intakeId;
    };

    lgrn::IdRegistryStl<CirclePathSimId>        ids;
    osp::KeyedVec<CirclePathSimId, Instance>    instOf;
};

struct UCtxConstantSpinSims
{
    struct Instance
    {
        ConstantSpinSim         sim;
        SimulationId            simId;
        std::int64_t            updateInterval;
        DataAccessorId          accessorId;
        CoSpaceId               cospaceId;
        IntakeId                intakeId;
    };

    lgrn::IdRegistryStl<ConstantSpinSimId>      ids;
    osp::KeyedVec<ConstantSpinSimId, Instance>  instOf;
};

struct UCtxSimpleGravitySims
{
    struct Instance
    {
        SimpleGravitySim        sim;
        SimulationId            simId;
        std::int64_t            updateInterval;
        DataAccessorId          accessorId;
        CoSpaceId               cospaceId;
        IntakeId                intakeId;
    };

    lgrn::IdRegistryStl<SimpleGravitySimId>         ids;
    osp::KeyedVec<SimpleGravitySimId, Instance>     instOf;
};

FeatureDef const ftrUniverseSimpleSimulators = feature_def("UniverseSimpleSimulators", [] (
        FeatureBuilder              &rFB,
        Implement<FIUniSimpleSims>  uniSimpleSims,
        DependOn<FIMainApp>         mainApp,
        DependOn<FIUniCore>         uniCore,
        DependOn<FIUniTransfers>    uniTransfers,
        entt::any                   userData)
{
    rFB.data_emplace< UCtxCirclePathSims >      (uniSimpleSims.di.circlePath);
    rFB.data_emplace< UCtxConstantSpinSims >    (uniSimpleSims.di.constantSpin);
    rFB.data_emplace< UCtxSimpleGravitySims >   (uniSimpleSims.di.simpleGravity);

    rFB.task()
        .name       ("Make DataAccessors for simple simulators")
        .sync_with  ({uniCore.pl.accessors(New), uniCore.pl.datasrcChanges(Modify_)})
        .args       ({            uniCore.di.dataAccessors,        uniCore.di.dataSrcs,                 uniCore.di.compTypes,     uniSimpleSims.di.circlePath,       uniSimpleSims.di.constantSpin,        uniSimpleSims.di.simpleGravity })
        .func       ([] (UCtxDataAccessors &rDataAccessors, UCtxDataSources &rDataSrcs, UCtxComponentTypes const& rCompTypes, UCtxCirclePathSims &rCirclePath, UCtxConstantSpinSims &rConstantSpin, UCtxSimpleGravitySims &rSimpleGravity) noexcept
    {
        DefaultComponents const &dc = rCompTypes.defaults;

        for (CirclePathSimId localSimId : rCirclePath.ids)
        {
            auto &rInst = rCirclePath.instOf[localSimId];
            DataAccessorId const &rAccessorId = rInst.accessorId;

            LGRN_ASSERT(rAccessorId.has_value());

            DataAccessor &rAccessor = rDataAccessors.instances[rAccessorId];

            if ( ! rAccessor.owner.has_value() )
            {
                rAccessor.debugName = fmt::format("CirclePath_{} sim{}", localSimId.value, rInst.simId.value);
                rAccessor.cospace = rInst.cospaceId;
                rAccessor.owner = rInst.simId;
                rAccessor.count = rInst.sim.m_data.size();

                constexpr std::size_t stride = sizeof(CirclePathSim::SatData);
                CirclePathSim::SatData const *pFirst = rInst.sim.m_data.data();
                rAccessor.components[dc.posX]  = make_comp(&pFirst->position.data()[0], stride);
                rAccessor.components[dc.posY]  = make_comp(&pFirst->position.data()[1], stride);
                rAccessor.components[dc.posZ]  = make_comp(&pFirst->position.data()[2], stride);
                rAccessor.components[dc.velX]  = make_comp(&pFirst->velocity.data()[0], stride);
                rAccessor.components[dc.velY]  = make_comp(&pFirst->velocity.data()[1], stride);
                rAccessor.components[dc.velZ]  = make_comp(&pFirst->velocity.data()[2], stride);
                rAccessor.components[dc.satId] = make_comp(&pFirst->id, stride);

                if ( ! rInst.sim.m_data.empty() )
                {
                    std::vector<SatelliteId> satsAffected;
                    satsAffected.reserve(rInst.sim.m_data.size());
                    for (CirclePathSim::SatData const& satData : rInst.sim.m_data)
                    {
                        satsAffected.push_back(satData.id);
                    }

                    rDataSrcs.changes.push_back(DataSourceChange{
                        .satsAffected = std::move(satsAffected),
                        .components   = component_type_set({dc.posX, dc.posY, dc.posZ,
                                                            dc.velX, dc.velY, dc.velZ, dc.satId}),
                        .accessor     = rAccessorId,
                    });
                }
            }
        }

        for (SimpleGravitySimId localSimId : rSimpleGravity.ids)
        {
            auto &rInst = rSimpleGravity.instOf[localSimId];
            DataAccessorId const &rAccessorId = rInst.accessorId;

            LGRN_ASSERT(rAccessorId.has_value());

            DataAccessor &rAccessor = rDataAccessors.instances[rAccessorId];

            if ( ! rAccessor.owner.has_value() )
            {
                rAccessor.debugName = fmt::format("SimpleGravitySim_{} sim{}", localSimId.value, rInst.simId.value);
                rAccessor.cospace = rInst.cospaceId;
                rAccessor.owner = rInst.simId;
                rAccessor.count = rInst.sim.m_data.size();

                constexpr std::size_t stride = sizeof(SimpleGravitySim::SatData);
                SimpleGravitySim::SatData const *pFirst = rInst.sim.m_data.data();
                rAccessor.components[dc.posX]  = make_comp(&pFirst->position.data()[0], stride);
                rAccessor.components[dc.posY]  = make_comp(&pFirst->position.data()[1], stride);
                rAccessor.components[dc.posZ]  = make_comp(&pFirst->position.data()[2], stride);
                rAccessor.components[dc.velXd] = make_comp(&pFirst->velocity.data()[0], stride);
                rAccessor.components[dc.velYd] = make_comp(&pFirst->velocity.data()[1], stride);
                rAccessor.components[dc.velZd] = make_comp(&pFirst->velocity.data()[2], stride);
                rAccessor.components[dc.satId] = make_comp(&pFirst->id, stride);

                if ( ! rInst.sim.m_data.empty() )
                {
                    std::vector<SatelliteId> satsAffected;
                    satsAffected.reserve(rInst.sim.m_data.size());
                    for (SimpleGravitySim::SatData const& satData : rInst.sim.m_data)
                    {
                        satsAffected.push_back(satData.id);
                    }

                    rDataSrcs.changes.push_back(DataSourceChange{
                        .satsAffected = std::move(satsAffected),
                        .components   = component_type_set({
                                                dc.satId,
                                                dc.posX, dc.posY, dc.posZ,
                                                dc.velXd, dc.velYd, dc.velZd,
                                                dc.accelX, dc.accelY, dc.accelZ}),
                        .accessor     = rAccessorId,
                    });
                }
            }
        }

        for (ConstantSpinSimId localSimId : rConstantSpin.ids)
        {
            auto &rInst = rConstantSpin.instOf[localSimId];
            DataAccessorId &rAccessorId = rInst.accessorId;

            DataAccessor &rAccessor = rDataAccessors.instances[rAccessorId];

            if ( ! rAccessor.owner.has_value() )
            {
                rAccessor.debugName = fmt::format("ConstantSpinSim_{} sim{}", localSimId.value, rInst.simId.value);
                rAccessor.cospace = rInst.cospaceId;
                rAccessor.owner = rInst.simId;
                rAccessor.count = 1;

                constexpr std::size_t stride = sizeof(ConstantSpinSim::SatData);
                ConstantSpinSim::SatData const *pFirst = rInst.sim.m_data.data();

                rAccessor.components[dc.rotX]  = make_comp(&pFirst->rot.data()[0], stride);
                rAccessor.components[dc.rotY]  = make_comp(&pFirst->rot.data()[1], stride);
                rAccessor.components[dc.rotZ]  = make_comp(&pFirst->rot.data()[2], stride);
                rAccessor.components[dc.rotW]  = make_comp(&pFirst->rot.data()[3], stride);
                rAccessor.components[dc.satId] = make_comp(&pFirst->id, stride);
            }
        }
    });

    static int counter = 0;

    counter = 0;

    rFB.task()
        .name       ("update simple simulations")
        .sync_with  ({uniCore.pl.accessors(Modify), uniCore.pl.accessorDelete(Modify_), uniCore.pl.simTimeBehindBy(Ready), uniCore.pl.datasrcChanges(Modify_), uniCore.pl.stolenSats(Modify), uniTransfers.pl.midTransfer(Ready), uniTransfers.pl.midTransferDelete(Modify_)})
        .args       ({
            uniCore.di          .simulations,
            uniCore.di          .dataSrcs,
            uniCore.di          .dataAccessors,
            uniCore.di          .compTypes,
            uniCore.di          .stolenSats,
            uniTransfers.di     .transferBufs,
            uniSimpleSims.di    .circlePath,
            uniSimpleSims.di    .constantSpin,
            uniSimpleSims.di    .simpleGravity })
        .func       ([] (
            UCtxSimulations             &rSimulations,
            UCtxDataSources             &rDataSrcs,
            UCtxDataAccessors           &rDataAccessors,
            UCtxComponentTypes    const &rCompTypes,
            UCtxStolenSatellites        &rStolenSats,
            UCtxTransferBuffers         &rTransferBufs,
            UCtxCirclePathSims          &rCirclePath,
            UCtxConstantSpinSims        &rConstantSpin,
            UCtxSimpleGravitySims       &rSimpleGravity) noexcept
    {
        DefaultComponents const &dc = rCompTypes.defaults;

        for (CirclePathSimId localSimId : rCirclePath.ids)
        {
            auto &rInst = rCirclePath.instOf[localSimId];
            std::int64_t &rTimeBehindBy = rSimulations.simulationOf[rInst.simId].timeBehindBy;

            while (rTimeBehindBy >= rInst.updateInterval)
            {
                rTimeBehindBy -= rInst.updateInterval;
                rInst.sim.update(rInst.updateInterval);
            }
        }

        for (ConstantSpinSimId localSimId : rConstantSpin.ids)
        {
            auto &rInst = rConstantSpin.instOf[localSimId];
            std::int64_t &rTimeBehindBy = rSimulations.simulationOf[rInst.simId].timeBehindBy;

            while (rTimeBehindBy >= rInst.updateInterval)
            {
                rTimeBehindBy -= rInst.updateInterval;
                rInst.sim.update(rInst.updateInterval);
            }
        }



        counter++;

        for (SimpleGravitySimId const localSimId : rSimpleGravity.ids)
        {
            auto &rInst = rSimpleGravity.instOf[localSimId];
            std::int64_t &rTimeBehindBy = rSimulations.simulationOf[rInst.simId].timeBehindBy;

            while (rTimeBehindBy >= rInst.updateInterval)
            {
                rTimeBehindBy -= rInst.updateInterval;

                rInst.sim.update(rInst.updateInterval);

                std::vector<MidTransfer> &rTransfers = rTransferBufs.midTransfersOf[rInst.simId];

                if (!rTransfers.empty() && counter == 60*5)
                {
                    rTransferBufs.midTransferDelete.push_back(rInst.simId);

                    std::vector<SatelliteId> satsAffected;

                    std::int64_t const transferbufTimeBehind = rSimulations.simulationOf[rTransferBufs.simId].timeBehindBy;

                    for (MidTransfer const& midTransfer : rTransfers)
                    {
                        auto const& rAccessor = rDataAccessors.instances[midTransfer.accessor];
                        auto iter = rAccessor.iterate(std::array{
                                dc.satId,
                                dc.posX, dc.posY, dc.posZ,
                                dc.velXd, dc.velYd, dc.velZd,
                                dc.accelX, dc.accelY, dc.accelZ});

                        float const timeDiff = (transferbufTimeBehind + rAccessor.time - rTimeBehindBy) * 0.001f;

                        satsAffected.resize(rAccessor.count);
                        for (std::size_t i = 0; i < rAccessor.count; ++i)
                        {
                            SatelliteId const satId = iter.get<SatelliteId>(0);
                            Vector3d const velocity = Vector3d{iter.get<double>(4), iter.get<double>(5), iter.get<double>(6)};
                            Vector3g const moved = Vector3g((velocity * timeDiff) / rInst.sim.m_metersPerPosUnit);
                            rInst.sim.m_data.push_back(SimpleGravitySim::SatData{
                                .position   = Vector3g{ iter.get<spaceint_t>(1), iter.get<spaceint_t>(2), iter.get<spaceint_t>(3) } + moved,
                                .velocity   = velocity,
                                .accel      = Vector3d{iter.get<float>(7), iter.get<float>(8), iter.get<float>(9)},
                                .mass       = 50.0f,
                                .id         = satId
                            });

                            satsAffected[i] = satId;

                            iter.next();
                        }

                        rStolenSats.of[midTransfer.accessor].allStolen = true;
                        rDataAccessors.accessorDelete.push_back(midTransfer.accessor);

                        rDataSrcs.changes.push_back(DataSourceChange{
                            .satsAffected = std::exchange(satsAffected, {}),
                            .components   = component_type_set({
                                                dc.satId,
                                                dc.posX, dc.posY, dc.posZ,
                                                dc.velXd, dc.velYd, dc.velZd,
                                                dc.accelX, dc.accelY, dc.accelZ}),
                            .accessor     = rInst.accessorId,
                        });
                    }

                    auto &rAccessor = rDataAccessors.instances[rInst.accessorId];

                    constexpr std::size_t stride = sizeof(SimpleGravitySim::SatData);
                    SimpleGravitySim::SatData const *pFirst = rInst.sim.m_data.data();
                    rAccessor.components[dc.posX]  = make_comp(&pFirst->position.data()[0], stride);
                    rAccessor.components[dc.posY]  = make_comp(&pFirst->position.data()[1], stride);
                    rAccessor.components[dc.posZ]  = make_comp(&pFirst->position.data()[2], stride);
                    rAccessor.components[dc.velXd]  = make_comp(&pFirst->velocity.data()[0], stride);
                    rAccessor.components[dc.velYd]  = make_comp(&pFirst->velocity.data()[1], stride);
                    rAccessor.components[dc.velZd]  = make_comp(&pFirst->velocity.data()[2], stride);
                    rAccessor.components[dc.satId] = make_comp(&pFirst->id, stride);
                    rAccessor.count = rInst.sim.m_data.size();
                }
            }
        }
    });
}); // ftrUniverseSimpleSimulators

struct TestPlanet
{
    SatelliteId satId;
    CoSpaceId   withinSOI;
    IntakeId    intake;
};

template <typename T>
void write_bytes(ArrayView<std::byte>& rRemaining, T const& value)
{
    LGRN_ASSERT(rRemaining.size() >= sizeof(T));
    std::memcpy(rRemaining.begin(), &value, sizeof(T));
    rRemaining = rRemaining.exceptPrefix(sizeof(T));
}

FeatureDef const ftrUniverseTestPlanets = feature_def("UniverseTestPlanets", [] (
        FeatureBuilder              &rFB,
        Implement<FIUniPlanets>     uniPlanets,
        DependOn<FIUniSimpleSims>   uniSimpleSims,
        DependOn<FIUniCore>         uniCore,
        DependOn<FIUniTransfers>    uniTransfers)
{
    using CoSpaceIdVec_t = std::vector<CoSpaceId>;

    auto &rCoordSpaces      = rFB.data_get< UCtxCoordSpaces >       (uniCore.di.coordSpaces);
    auto &rCompTypes        = rFB.data_get< UCtxComponentTypes >    (uniCore.di.compTypes);
    auto &rDataAccessors    = rFB.data_get< UCtxDataAccessors >     (uniCore.di.dataAccessors);
    auto &rStolenSats       = rFB.data_get< UCtxStolenSatellites >  (uniCore.di.stolenSats);
    auto &rDataSrcs         = rFB.data_get< UCtxDataSources >       (uniCore.di.dataSrcs);
    auto &rSatInst          = rFB.data_get< UCtxSatelliteInstances >(uniCore.di.satInst);
    auto &rSimulations      = rFB.data_get< UCtxSimulations >       (uniCore.di.simulations);
    auto &rIntakes          = rFB.data_get< UCtxIntakes >           (uniTransfers.di.intakes);
    auto &rTransferBufs     = rFB.data_get< UCtxTransferBuffers >   (uniTransfers.di.transferBufs);

    auto &rCirclePathSims   = rFB.data_get< UCtxCirclePathSims >    (uniSimpleSims.di.circlePath);
    auto &rConstantSpinSims = rFB.data_get< UCtxConstantSpinSims >  (uniSimpleSims.di.constantSpin);
    auto &rSimpleGravitySims = rFB.data_get< UCtxSimpleGravitySims >(uniSimpleSims.di.simpleGravity);

    CoSpaceId rootSpace = rCoordSpaces.ids.create();

    std::vector<TestPlanet> planets;

    planets.emplace_back(TestPlanet{});

    for (TestPlanet &rTestPlanet : planets)
    {
        rTestPlanet = TestPlanet{
            .satId      = rSatInst.ids.create(),
            .withinSOI  = rCoordSpaces.ids.create(),
            //.intake     = rIntakes.ids.create()
        };
    }

    DefaultComponents const &dc = rCompTypes.defaults;

    ComponentTypeIdSet_t const intakeComps = component_type_set({
            dc.satId,
            dc.posX, dc.posY, dc.posZ,
            dc.velXd, dc.velYd, dc.velZd,
            dc.accelX, dc.accelY, dc.accelZ});

    CirclePathSimId const rootSimAId = rCirclePathSims.ids.create();
    rCirclePathSims.instOf.resize(rCirclePathSims.ids.size());

    UCtxCirclePathSims::Instance &rSimInstA = rCirclePathSims.instOf[rootSimAId];
    rSimInstA = UCtxCirclePathSims::Instance{
        .simId             = rSimulations.ids.create(),
        .updateInterval    = 200,
        .accessorId        = rDataAccessors.ids.create(),
        .cospaceId         = rootSpace,
        //.intakeId          = rIntakes.make_intake(rCirclePathSims.instOf[rootSimAId].simId, rootSpace, arrayView(positionComps))
    };

    // add 1 satellite
    rSimInstA.sim.m_data.resize(1);
    rSimInstA.sim.m_data[0].radius     = 20.0 * 1024.0;
    rSimInstA.sim.m_data[0].period     = 20000;
    rSimInstA.sim.m_data[0].id         = rSatInst.ids.create();

    SimpleGravitySimId const rootSimBId = rSimpleGravitySims.ids.create();
    rSimpleGravitySims.instOf.resize(rSimpleGravitySims.ids.size());
    rSimpleGravitySims.instOf.resize(rSimpleGravitySims.ids.size());
    SimulationId const simBId = rSimulations.ids.create();

    UCtxSimpleGravitySims::Instance &rSimInstB = rSimpleGravitySims.instOf[rootSimBId];
    rSimInstB = UCtxSimpleGravitySims::Instance{
        .simId             = simBId,
        .updateInterval    = 500,
        .accessorId        = rDataAccessors.ids.create(),
        .intakeId          = rIntakes.make_intake(simBId, rootSpace, intakeComps)
    };

    rSimInstB.sim.m_data.push_back(SimpleGravitySim::SatData{
        .position   = {1000, 1000, 1000},
        .velocity   = {1.0f, 0.0f, 0.0f},
        .accel      = {},
        .mass       = 10.0f,
        .id = rSatInst.ids.create()
    });
    rSimInstB.sim.m_metersPerPosUnit = 1/1024.0f;
    rSimInstB.sim.m_secPerTimeUnit = 0.001f;

    rSimulations.simulationOf.resize(rSimulations.ids.capacity());

    IntakeId const intakeId = rIntakes.find_intake_at(rootSpace, intakeComps);
    LGRN_ASSERT(intakeId.has_value());

    static constexpr std::size_t c_reqDataSize = sizeof(SatelliteId) + sizeof(spaceint_t) * 3u + sizeof(double) * 3u + sizeof(float) * 3u;

    std::unique_ptr<std::byte[]> reqData = std::make_unique<std::byte[]>(c_reqDataSize);

    auto remaining = ArrayView<std::byte>(reqData.get(), c_reqDataSize);

    auto sat = rSatInst.ids.create();

    write_bytes<SatelliteId>    (remaining, sat);   // satId
    write_bytes<spaceint_t>     (remaining, 10);    // posX
    write_bytes<spaceint_t>     (remaining, 10);    // posY
    write_bytes<spaceint_t>     (remaining, 5000);  // posZ
    write_bytes<double>         (remaining, 0.0);   // velXd
    write_bytes<double>         (remaining, -1.0);  // velYd
    write_bytes<double>         (remaining, 0.0);   // velZd
    write_bytes<float>          (remaining, 10.0f); // accelX
    write_bytes<float>          (remaining, 10.0f); // accelY
    write_bytes<float>          (remaining, 10.0f); // accelZ

    rTransferBufs.requests.push_back(TransferRequest{
        .data   = std::move(reqData),
        .count  = 1,
        .time   = 0u,
        .target = intakeId
    });


    rStolenSats.of.resize(rDataAccessors.ids.capacity());

    constexpr int           precision       = 10;
    constexpr int           planetCount     = 64;
    constexpr int           seed            = 1337;
    constexpr spaceint_t    maxDist         = math::mul_2pow<spaceint_t, int>(20000ul, precision);
    constexpr float         maxVel          = 800.0f;

    CoSpaceId const mainSpace = rCoordSpaces.ids.create();

    std::vector<CoSpaceId> satSurfaceSpaces(planetCount);
    rCoordSpaces.ids.create(satSurfaceSpaces.begin(), satSurfaceSpaces.end());

    // finalize coordinate space tree
    auto const cospaceCapacity = rCoordSpaces.ids.capacity();
    rCoordSpaces.transforms     .resize(cospaceCapacity);
    rCoordSpaces.idParent       .resize(cospaceCapacity);
    rCoordSpaces.idToTreePos    .resize(cospaceCapacity);

    rCoordSpaces.treeDescendants.assign({0u});
    rCoordSpaces.treeToId.assign({mainSpace});
    rCoordSpaces.idToTreePos[mainSpace] = 0;

    rFB.task()
        .name       ("Tell all simulations to advance forward in time")
        .sync_with  ({uniCore.pl.simTimeBehindBy(Modify)})
        .args       ({           uniCore.di.simulations })
        .func       ([] ( UCtxSimulations &rSimulations) noexcept
    {
        for (SimulationId simId : rSimulations.ids)
        {
            rSimulations.simulationOf[simId].timeBehindBy += 15;
        }
    });

}); // ftrUniverseTestPlanets


} // namespace adera
