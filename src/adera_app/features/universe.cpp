/**
 * Open Space Program
 * Copyright © 2019-2024 Open Space Program Project
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
#include "universe.h"

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

struct SatelliteTransform
{
    osp::Quaterniond    rotation;
    Vector3g            position;
    osp::Vector3        velocity;
    std::int64_t        timeBehind{};
    CoSpaceId           cospace;
};

SatelliteTransform get_satellite_transform(
        SatelliteId                 satId,
        UCtxDataAccessors     const &rDataAccessors,
        UCtxDataSources       const &rDataSrcs,
        UCtxStolenSatellites  const &rStolenSats,
        UCtxSimulations       const &rSimulations,
        UCtxComponentTypes    const &compTypes )
{
    DefaultComponents const &dc     = compTypes.defaults;
    DataSourceId      const dataSrc = rDataSrcs.datasrcOf[satId];

    SatelliteTransform out;

    for (DataSource::Entry const& entry : rDataSrcs.instances[dataSrc].entries)
    {
        DataAccessor const& rAccessor = rDataAccessors.instances[entry.accessor];

        UCtxStolenSatellites::OfAccessor const& stolen = rStolenSats.of[entry.accessor];

        auto iter = rAccessor.iterate(std::array{
            dc.posX,  dc.posY,  dc.posZ,            // 0, 1, 2
            dc.velX,  dc.velY,  dc.velZ,            // 3, 4, 5
            dc.velXd, dc.velYd, dc.velZd,           // 6, 7, 8
            dc.rotX,  dc.rotY,  dc.rotZ, dc.rotW,   // 9, 10, 11, 12
            dc.satId});                             // 13

        bool const hasPosXYZ  = iter.has(0) && iter.has(1) && iter.has(2);
        bool const hasVelXYZ  = iter.has(3) && iter.has(4) && iter.has(5);
        bool const hasVelXYZd = iter.has(6) && iter.has(7) && iter.has(8);
        bool const hasRotXYZW = iter.has(9) && iter.has(10) && iter.has(11) && iter.has(12);

        LGRN_ASSERTM(iter.has(13), "SatelliteId missing");

        if ( ! (hasPosXYZ || hasVelXYZ || hasVelXYZd || hasRotXYZW) )
        {
            continue; // Accessor contains no components of interest
        }

        out.timeBehind = rAccessor.owner.has_value() ? rSimulations.simulationOf[rAccessor.owner].timeBehindBy : 0;

        for (std::size_t i = 0; i < rAccessor.count; ++i, iter.next())
        {
            SatelliteId const iterSatId = iter.get<SatelliteId>(13);

            if (iterSatId == satId && !stolen.has(iterSatId))
            {
                if (hasVelXYZ)
                {
                    out.velocity = Vector3{iter.get<float>(3), iter.get<float>(4), iter.get<float>(5)};
                }

                if (hasVelXYZd)
                {
                    out.velocity = Vector3(Vector3d{iter.get<double>(6), iter.get<double>(7), iter.get<double>(8)});
                }

                if (hasPosXYZ)
                {
                    out.position = Vector3g{iter.get<spaceint_t>(0), iter.get<spaceint_t>(1), iter.get<spaceint_t>(2)};
                }

                if (hasRotXYZW)
                {
                    out.rotation = Quaterniond{ {iter.get<float>(9), iter.get<float>(10), iter.get<float>(11)}, iter.get<float>(12)};
                }

                break; // satellite only appears once per accessor
            }
        }
    }

    return out;
}


FeatureDef const ftrUniverseCore = feature_def("UniverseCore", [] (
        FeatureBuilder              &rFB,
        Implement<FIUniCore>        uniCore,
        Implement<FIUniTransfers>   uniTransfers,
        Implement<FIUniScenes>      uniScenes,
        DependOn<FICleanupContext>  cleanup,
        DependOn<FIMainApp>         mainApp,
        entt::any                   userData)
{
    auto &rCoordSpaces      = rFB.data_emplace< UCtxCoordSpaces >       (uniCore.di.coordSpaces);
    auto &rCompTypes        = rFB.data_emplace< UCtxComponentTypes >    (uniCore.di.compTypes);
    auto &rDataAccessors    = rFB.data_emplace< UCtxDataAccessors >     (uniCore.di.dataAccessors);
    auto &rDeletedSats      = rFB.data_emplace< UCtxStolenSatellites >  (uniCore.di.stolenSats);
    auto &rDataSrcss        = rFB.data_emplace< UCtxDataSources >       (uniCore.di.dataSrcs);
    auto &rSatInst          = rFB.data_emplace< UCtxSatellites >        (uniCore.di.satInst);
    auto &rSimulations      = rFB.data_emplace< UCtxSimulations >       (uniCore.di.simulations);
    auto &rIntakes          = rFB.data_emplace< UCtxIntakes >           (uniTransfers.di.intakes);
    auto &rTransferBufs     = rFB.data_emplace< UCtxTransferBuffers >   (uniTransfers.di.transferBufs, rSimulations.ids.create());
    auto &rScenes           = rFB.data_emplace< UCtxScenes >            (uniScenes.di.scenes);

    rFB.pipeline(uniCore.pl.update)                 .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.satIds)                 .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.transfer)               .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.cospaceTransform)       .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.accessorIds)            .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.accessors)              .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.accessorsOfCospace)     .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.stolenSats)             .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.accessorDelete)         .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.datasrcIds)             .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.datasrcs)               .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.datasrcOf)              .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.datasrcChanges)         .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.simTimeBehindBy)        .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniTransfers.pl.requests)          .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniTransfers.pl.requestAccessorIds).parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniTransfers.pl.midTransfer)       .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniTransfers.pl.midTransferDelete) .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniScenes.pl.requestTranslate)     .parent(mainApp.loopblks.mainLoop);

    // DataAccessors ----------------------------------------------------------

    rFB.task()
        .name       ("Delete DataAccessors and DataAccessorIds using accessorDelete")
        .sync_with  ({uniCore.pl.accessorDelete(UseOrRun), uniCore.pl.accessors(Delete), uniCore.pl.accessorIds(Delete)})
        .args       ({            uniCore.di.dataAccessors})
        .func       ([] (UCtxDataAccessors &rDataAccessors) noexcept
    {
        for (DataAccessorId const id : rDataAccessors.accessorDelete)
        {
            rDataAccessors.instances[id] = {};
            rDataAccessors.ids.remove(id);
        }
    });

    rFB.task()
        .name       ("Resize rDataAccessors.instances")
        .sync_with  ({uniCore.pl.accessors(Resize_), uniCore.pl.accessorIds(Ready)})
        .args       ({            uniCore.di.dataAccessors})
        .func       ([] (UCtxDataAccessors &rDataAccessors) noexcept
    {
        rDataAccessors.instances.resize(rDataAccessors.ids.capacity());
    });

    rFB.task()
        .name       ("Clear accessorDelete once we're done with it")
        .sync_with  ({ uniCore.pl.accessorDelete(Clear) })
        .args       ({            uniCore.di.dataAccessors})
        .func       ([] (UCtxDataAccessors &rDataAccessors) noexcept
    {
        rDataAccessors.accessorDelete.clear();
    });

    //TODO: add cospaceIds pipeline
    rFB.task()
        .name       ("Write accessorsOfCospace")
        .sync_with  ({ uniCore.pl.accessorsOfCospace(Modify), uniCore.pl.accessors(Ready), uniCore.pl.accessorIds(Ready) })
        .args       ({            uniCore.di.dataAccessors,              uniCore.di.coordSpaces})
        .func       ([] (UCtxDataAccessors &rDataAccessors, UCtxCoordSpaces const& rCoordSpaces) noexcept
    {
        // clear rDataAccessors.accessorsOfCospace and remake it all each update
        // TODO: this is temporary

        for (auto &rVec : rDataAccessors.accessorsOfCospace)
        {
            rVec.clear();
        }
        rDataAccessors.accessorsOfCospace.resize(rCoordSpaces.ids.capacity());

        for (DataAccessorId const accessorId : rDataAccessors.ids)
        {
            DataAccessor const& rAccessor = rDataAccessors.instances[accessorId];

            if (rAccessor.cospace.has_value())
            {
                rDataAccessors.accessorsOfCospace[rAccessor.cospace].push_back(accessorId);
            }
        }
    });

    // Coordinate Spaces ------------------------------------------------------

    rFB.task()
        .name       ("Updated satellite-parented CoSpace transforms")
        .sync_with  ({ uniCore.pl.cospaceTransform(Modify), uniCore.pl.accessors(Ready), uniCore.pl.accessorIds(Ready), uniCore.pl.datasrcOf(Ready), uniCore.pl.datasrcs(Ready), uniCore.pl.stolenSats(Ready) })
        .args       ({
            uniCore.di.coordSpaces,
            uniCore.di.dataAccessors,
            uniCore.di.dataSrcs,
            uniCore.di.stolenSats,
            uniCore.di.simulations,
            uniCore.di.compTypes })
        .func       ([] (
            UCtxCoordSpaces             &rCoordSpaces,
            UCtxDataAccessors           &rDataAccessors,
            UCtxDataSources       const &rDataSrcs,
            UCtxStolenSatellites  const &rStolenSats,
            UCtxSimulations       const &rSimulations,
            UCtxComponentTypes    const &compTypes ) noexcept
    {
        // Loop through every coordinate space that has a parent satellite.
        // TODO: this is very inefficient. add a thing to subscribe to changes to dataaccessors

        for (CoSpaceId const cospaceId : rCoordSpaces.ids)
        {
            CospaceTransform &rTf = rCoordSpaces.transformOf[cospaceId];

            if (rTf.parentSat.has_value())
            {
                SatelliteTransform const satTf = get_satellite_transform(rTf.parentSat, rDataAccessors, rDataSrcs, rStolenSats, rSimulations, compTypes);

                rTf.position = satTf.position;
                rTf.velocity = satTf.velocity;
            }
        }
    });

    // DataSources ------------------------------------------------------------

    rFB.task()
        .name       ("Resize datasrcOf")
        .sync_with  ({uniCore.pl.datasrcOf(Resize_), uniCore.pl.satIds(Ready)  })
        .args       ({          uniCore.di.dataSrcs,               uniCore.di.satInst })
        .func       ([] (UCtxDataSources &rDataSrcs, UCtxSatellites &rSatInst) noexcept
    {
        rDataSrcs.datasrcOf.resize(rSatInst.ids.capacity());
    });

    rFB.task()
        .name       ("create datasources")
        .sync_with  ({ uniCore.pl.datasrcChanges(UseOrRun), uniCore.pl.datasrcOf(New), uniCore.pl.datasrcs(New) })
        .args       ({          uniCore.di.dataSrcs })
        .func       ([] (UCtxDataSources &rDataSrcs) noexcept
    {
        if (rDataSrcs.changes.empty()) { return; }

        // keep a scratchpad component list
        // iterate satsAffected.
        //    copy existing datasource component list to scratchpad
        //    apply changes
        //    remove from datasource
        //    find/create new datasource
        // optimization: temporary oldDatasrc->newDatasrc map, to prevent searching too much

        DataSource scratchpad;

        for (DataSourceChange const& dsc : rDataSrcs.changes)
        {
            for (SatelliteId const satId : dsc.satsAffected)
            {
                DataSourceOwner_t &rSatDsOwner = rDataSrcs.datasrcOf[satId];
                DataSourceId newDsId;

                scratchpad.entries.clear();

                if (rSatDsOwner.has_value())
                {
                    // Satellite already has a DataSource, copy it into scratchpad then modify it.

                    DataSourceId const satDsId = rSatDsOwner.value();
                    rDataSrcs.refCounts.ref_release(std::exchange(rSatDsOwner, {}));
                    auto const refCount = rDataSrcs.refCounts[satDsId.value];

                    DataSource &rSatDs = rDataSrcs.instances[satDsId];

                    scratchpad.entries.assign(rSatDs.entries.begin(), rSatDs.entries.end());

                    bool added = false;

                    // remove occurances of ComponentTypeIds used in dsc.components from scratchpad
                    auto const newLast = std::remove_if(
                            scratchpad.entries.begin(),
                            scratchpad.entries.end(),
                            [&dsc, &added] (DataSource::Entry &rSpEntry) -> bool
                    {
                        if (rSpEntry.accessor == dsc.accessor)
                        {
                            for (ComponentTypeId const ctId : dsc.components)
                            {
                                rSpEntry.components.insert(ctId);
                            }
                            LGRN_ASSERT(added == false);
                            added = true;

                            return false;
                        }
                        else
                        {
                            for (ComponentTypeId const ctId : dsc.components)
                            {
                                rSpEntry.components.erase(ctId);
                            }

                            return rSpEntry.components.empty(); // remove if true
                        }
                    });
                    scratchpad.entries.resize(std::distance(scratchpad.entries.begin(), newLast));

                    if ( ! added )
                    {
                        scratchpad.entries.push_back(DataSource::Entry{
                            .components = dsc.components,
                            .accessor   = dsc.accessor
                        });
                    }
                    scratchpad.sort();
                }
                else
                {
                    // No existing data source, likely that the satellite is newly added.
                    scratchpad.entries.push_back(DataSource::Entry{
                        .components = dsc.components,
                        .accessor   = dsc.accessor
                    });
                }

                newDsId = rDataSrcs.find_datasource(scratchpad);

                if ( ! newDsId.has_value() )
                {
                    newDsId = rDataSrcs.ids.create();
                    rDataSrcs.instances.resize(rDataSrcs.ids.capacity());
                    rDataSrcs.instances[newDsId] = std::exchange(scratchpad, {});
                }

                rDataSrcs.datasrcOf[satId] = rDataSrcs.refCounts.ref_add(newDsId);
            }
        }
    });

    rFB.task()
        .name       ("Clear rDataSrcs.changes once we're done with it")
        .sync_with  ({ uniCore.pl.datasrcChanges(Clear) })
        .args       ({          uniCore.di.dataSrcs })
        .func       ([] (UCtxDataSources &rDataSrcs) noexcept
    {
        rDataSrcs.changes.clear();
    });


    // Transfer Requests ------------------------------------------------------

    rFB.task()
        .name       ("Make transfer request DataAccessorIds")
        .sync_with  ({uniCore.pl.accessorIds(New), uniTransfers.pl.requestAccessorIds(Modify_), uniTransfers.pl.requests(UseOrRun)})
        .args       ({         uniTransfers.di.transferBufs, uniTransfers.di.intakes,           uniCore.di.compTypes,          uniCore.di.dataAccessors,        uniCore.di.simulations })
        .func       ([] (UCtxTransferBuffers &rTransferBufs,   UCtxIntakes &rIntakes, UCtxComponentTypes &rCompTypes, UCtxDataAccessors &rDataAccessors, UCtxSimulations &rSimulations) noexcept
    {
        rTransferBufs.requestAccessorIds.resize(rTransferBufs.requests.size());
        rDataAccessors.ids.create(rTransferBufs.requestAccessorIds.begin(),
                                  rTransferBufs.requestAccessorIds.end());
    });

    rFB.task()
        .name       ("Clear requestAccessorIds once we're done with it")
        .sync_with  ({uniTransfers.pl.requestAccessorIds(Clear)})
        .args       ({         uniTransfers.di.transferBufs })
        .func       ([] (UCtxTransferBuffers &rTransferBufs) noexcept
    {
        rTransferBufs.requestAccessorIds.clear();
    });

    // MidTransfers -----------------------------------------------------------

    rFB.task()
        .name       ("Delete MidTransfers from midTransferDelete")
        .sync_with  ({uniCore.pl.accessors(Delete), uniTransfers.pl.midTransferDelete(UseOrRun), uniTransfers.pl.midTransfer(Delete)})
        .args       ({         uniTransfers.di.transferBufs,          uniCore.di.dataAccessors})
        .func       ([] (UCtxTransferBuffers &rTransferBufs, UCtxDataAccessors &rDataAccessors) noexcept
    {
        for (SimulationId const simId : rTransferBufs.midTransferDelete)
        {
            rTransferBufs.midTransfersOf[simId].clear();
        }
    });

    rFB.task()
        .name       ("Resize midTransfersOf to fit all SimulationIds")
        .sync_with  ({ uniTransfers.pl.midTransfer(Resize_)})
        .args       ({         uniTransfers.di.transferBufs,        uniCore.di.simulations })
        .func       ([] (UCtxTransferBuffers &rTransferBufs, UCtxSimulations &rSimulations) noexcept
    {
        rTransferBufs.midTransfersOf.resize(rSimulations.ids.capacity());
    });

    rFB.task()
        .name       ("Make midTransfer DataAccessor data")
        .sync_with  ({uniCore.pl.accessors(New), uniTransfers.pl.requestAccessorIds(UseOrRun), uniTransfers.pl.midTransfer(New), uniTransfers.pl.requests(Clear), uniCore.pl.datasrcChanges(Modify_)})
        .args       ({         uniTransfers.di.transferBufs, uniTransfers.di.intakes,                 uniCore.di.compTypes,          uniCore.di.dataAccessors,        uniCore.di.dataSrcs,        uniCore.di.simulations })
        .func       ([] (UCtxTransferBuffers &rTransferBufs,   UCtxIntakes &rIntakes, UCtxComponentTypes const &rCompTypes, UCtxDataAccessors &rDataAccessors, UCtxDataSources &rDataSrcs, UCtxSimulations &rSimulations) noexcept
    {
        LGRN_ASSERT(rTransferBufs.requests.size() == rTransferBufs.requestAccessorIds.size());

        for (std::size_t i = 0; i < rTransferBufs.requests.size(); ++i)
        {
            TransferRequest       &rRequest  = rTransferBufs.requests[i];
            DataAccessorId  const accessorId = rTransferBufs.requestAccessorIds[i];
            Intake          const &rTarget   = rIntakes.instances[rRequest.target];

            DataAccessor::CompMap_t components;

            std::ptrdiff_t stride = 0;
            for (ComponentTypeId const compTypeId : rTarget.components)
            {
                stride += rCompTypes.info[compTypeId].size;
            }

            SatelliteId const* satIdFirst = nullptr;

            for (std::byte const* pos = rRequest.data.get();
                 ComponentTypeId const compTypeId : rTarget.components)
            {
                if (compTypeId == rCompTypes.defaults.satId)
                {
                    satIdFirst = reinterpret_cast<SatelliteId const*>(pos);
                }
                components.emplace(compTypeId, DataAccessor::Component{pos, stride});
                pos += rCompTypes.info[compTypeId].size;
            }

            LGRN_ASSERT(satIdFirst != nullptr);

            rDataAccessors.instances[accessorId] = DataAccessor{
                .debugName  = fmt::format("TransferBuffer to intake{}", rRequest.target.value),
                .components = std::move(components),
                //.time       = 0, // TODO
                .count      = rRequest.count,
                .owner      = rTransferBufs.simId,
                .cospace    = rTarget.cospace,
                .iterMethod = DataAccessor::IterationMethod::SkipNullSatellites
            };

            using Corrade::Containers::StridedArrayView1D;

            auto const data       = ArrayView<void const>(rRequest.data.get(), stride * rRequest.count);
            auto const dataSatIds = StridedArrayView1D<SatelliteId const>(data, satIdFirst, rRequest.count, stride);

            std::vector<SatelliteId> sats;
            sats.resize(rRequest.count);

            for (std::size_t i = 0; i < rRequest.count; ++i)
            {
                sats[i] = dataSatIds[i];
            }

            rDataSrcs.changes.push_back(DataSourceChange{
                .satsAffected   = std::move(sats),
                .components     = rTarget.components,
                .accessor       = accessorId
            });

            rTransferBufs.midTransfersOf[rTarget.owner].push_back(MidTransfer{
                .data           = std::move(rRequest.data),
                .accessor       = accessorId,
                .target         = rRequest.target
            });
        }

        rTransferBufs.requests.clear();
    });


    rFB.task()
        .name       ("Clear midTransferDelete once we're done with it")
        .sync_with  ({ uniTransfers.pl.midTransferDelete(Clear)})
        .args       ({         uniTransfers.di.transferBufs})
        .func       ([] (UCtxTransferBuffers &rTransferBufs) noexcept
    {
        rTransferBufs.midTransferDelete.clear();
    });

    // Scenes -----------------------------------------------------------------

    rFB.task()
        .name       ("Apply requestTranslates")
        .sync_with  ({ uniScenes.pl.requestTranslate(UseOrRun), uniCore.pl.cospaceTransform(Modify) })
        .args       ({   uniScenes.di.scenes, uniCore.di.coordSpaces})
        .func       ([] (UCtxScenes &rScenes, UCtxCoordSpaces &coordSpaces) noexcept
    {
        for (SceneId const sceneId : rScenes.ids)
        {
            ConnectedScene const &cs = rScenes.connectionOf[sceneId];
            CospaceTransform &rTf = coordSpaces.transformOf[cs.cospace];
            rTf.position += cs.requestTranslate;
        }
    });

    rFB.task()
        .name       ("Clear requestTranslates")
        .sync_with  ({ uniScenes.pl.requestTranslate(Clear)})
        .args       ({   uniScenes.di.scenes})
        .func       ([] (UCtxScenes &rScenes) noexcept
    {
        for (ConnectedScene &rConnectedScene : rScenes.connectionOf)
        {
            rConnectedScene.requestTranslate = Vector3g{0, 0, 0};
        }
    });

    // Cleanup ----------------------------------------------------------------

    rFB.task()
        .name       ("Clean up UCtxDataSources IdOwners")
        .sync_with  ({cleanup.pl.cleanup(Run_)})
        .args       ({          uniCore.di.dataSrcs })
        .func       ([] (UCtxDataSources &rDataSrcs) noexcept
    {
        for (DataSourceOwner_t &rOwner : rDataSrcs.datasrcOf)
        {
            rDataSrcs.refCounts.ref_release(std::exchange(rOwner, {}));
        }
    });


}); // ftrUniverseCore


FeatureDef const ftrUniverseEqualTimeUpdate = feature_def("UniverseEqualTimeUpdate", [] (
        FeatureBuilder              &rFB,
        DependOn<FIUniCore>         uniCore)
{
    rFB.task()
        .name       ("Tell all simulations to advance forward in time by 15ms")
        .sync_with  ({uniCore.pl.simTimeBehindBy(Modify)})
        .args       ({           uniCore.di.simulations })
        .func       ([] ( UCtxSimulations &rSimulations) noexcept
    {
        for (SimulationId simId : rSimulations.ids)
        {
            rSimulations.simulationOf[simId].timeBehindBy += 15;
        }
    });
});

} // namespace adera
