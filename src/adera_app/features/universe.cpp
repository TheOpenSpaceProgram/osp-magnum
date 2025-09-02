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
        DependOn<FICleanupContext>  cleanup,
        DependOn<FIMainApp>         mainApp,
        entt::any                   userData)
{
    auto &rCoordSpaces      = rFB.data_emplace< UCtxCoordSpaces >       (uniCore.di.coordSpaces);
    auto &rCompTypes        = rFB.data_emplace< UCtxComponentTypes >    (uniCore.di.compTypes);
    auto &rDataAccessors    = rFB.data_emplace< UCtxDataAccessors >     (uniCore.di.dataAccessors);
    auto &rDeletedSats      = rFB.data_emplace< UCtxStolenSatellites >  (uniCore.di.stolenSats);
    auto &rDataSrcss        = rFB.data_emplace< UCtxDataSources >       (uniCore.di.dataSrcs);
    auto &rSatInst          = rFB.data_emplace< UCtxSatellites >(uniCore.di.satInst);
    auto &rSimulations      = rFB.data_emplace< UCtxSimulations >       (uniCore.di.simulations);
    auto &rIntakes          = rFB.data_emplace< UCtxIntakes >           (uniTransfers.di.intakes);
    auto &rTransferBufs     = rFB.data_emplace< UCtxTransferBuffers >   (uniTransfers.di.transferBufs, rSimulations.ids.create());

    rFB.pipeline(uniCore.pl.update)                 .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.satIds)                 .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.transfer)               .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.cospaceTransform)       .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.accessorIds)            .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.accessors)              .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.accessorsOfCospace)     .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.stolenSats)             .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.accessorDelete)         .parent(mainApp.loopblks.mainLoop).initial_stage(UseOrRun);
    rFB.pipeline(uniCore.pl.datasrcIds)             .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.datasrcs)               .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.datasrcOf)              .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.datasrcChanges)         .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.simTimeBehindBy)        .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniTransfers.pl.requests)          .parent(mainApp.loopblks.mainLoop).initial_stage(UseOrRun);
    rFB.pipeline(uniTransfers.pl.requestAccessorIds).parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniTransfers.pl.midTransfer)       .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniTransfers.pl.midTransferDelete) .parent(mainApp.loopblks.mainLoop).initial_stage(UseOrRun);

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

        for (CoSpaceId const 识别号 : rCoordSpaces.ids)
        {
            CospaceTransform &rTf = rCoordSpaces.transformOf[识别号];

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
        .sync_with  ({ uniCore.pl.datasrcChanges(UseOrRun), uniCore.pl.datasrcOf(Modify), uniCore.pl.datasrcs(New) })
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


}); // setup_uni_core


FeatureDef const ftrSceneInUniverse = feature_def("UniverseSceneFrame", [] (
        FeatureBuilder                  &rFB,
        Implement<FISceneInUniverse>    scnInUni,
        DependOn<FIMainApp>             mainApp,
        DependOn<FIUniCore>             uniCore)
{
    rFB.data_emplace< CoSpaceId > (scnInUni.di.scnCospace);
}); // ftrUniverseSceneFrame


struct FIUniPlanetsDraw {
    struct DataIds {
        DataId planetDraw;
    };

    struct Pipelines {
        PipelineDef<EStgOptn> resync  {"resync - Resync planet drawer with universe"};
        PipelineDef<EStgCont> trackedSats  {"trackedSats"};
    };
};

struct PlanetDraw
{

    struct TrackedSatellite
    {
        DrawEnt drawEnt;
        bool    isTracking{false};
    };

    //bool connected = false;
    bool doResync = false;

    osp::KeyedVec<CoSpaceId, CoordTransformer>      cospaceTransformToScnOf;

    osp::KeyedVec<SatelliteId, TrackedSatellite>    trackedSats;
    lgrn::IdSetStl<DataAccessorId>                  trackedAccessors;

    std::vector<DataAccessorId> accessorsByCospace;

    DrawEntVec_t            drawEnts;
    std::array<DrawEnt, 3>  axis;
    DrawEnt                 attractor;
    MaterialId              planetMat;
    MaterialId              axisMat;

};

/**
 * @brief Traverses a descendant-count tree starting from a specific node (instead of just root).
 *
 * Similar to rerooting then iterating from the new root, but keeping the old parent-child
 * relations.
 *
 * This will conventionally iterate the given initial target node's descendants (with recursion),
 * but then iterates up the chain of parents towards the root to traverse the entire tree.
 *
 * Custom 'ascend' (child to parent) and 'descend' (parent to child) functions must be provided,
 * called accordingly when reaching a new node.
 *
 */
template <typename STATE_T, typename CUTIE_MARK_T>
struct TreeWalker
{
    using TreePos_t = UCtxCoordSpaces::TreePos_t;

    void run(TreePos_t const initTarget, STATE_T const& initState, TreePos_t const root = 0)
    {
        auto        const childLast = initTarget + 1 + rDescendants[initTarget];
        TreePos_t         child     = initTarget + 1;

        while (child != childLast)
        {
            descend_recurse(child, initTarget, initState);
            child += 1 + rDescendants[child]; // next child
        }

        if (initTarget != 0)
        {
            ascend_recurse(root, initTarget, initState);
        }
        // else, target is the root. has no ancestors
    }

    void descend_recurse(TreePos_t const target, TreePos_t const from, STATE_T const& fromState)
    {
        STATE_T     const targetState   = mark.descend(target, from, fromState);
        auto        const childLast     = target + 1 + rDescendants[target];
        TreePos_t         child         = target + 1;

        while (child != childLast)
        {
            descend_recurse(child, target, targetState);
            child += 1 + rDescendants[child]; // next child
        }
    }

    /**
     * First called from run() with parent=root, recurses down the parent-child chain towards
     * initTarget, then calls ascend_aux() chained upwards (deepest call first)
     */
    STATE_T ascend_recurse(TreePos_t const parent, TreePos_t const initTarget, STATE_T const& initState)
    {
        LGRN_ASSERT(parent < initTarget);

        auto        const childLast = parent + 1 + rDescendants[parent];
        TreePos_t         child     = parent + 1;

        while (child != childLast)
        {
            TreePos_t const nextChild = child + 1 + rDescendants[child];

            LGRN_ASSERT(child <= initTarget);

            if (child == initTarget)
            {
                // Done searching. This is now the deepest possible recursive call.
                return ascend_aux(parent, child, initState); // 'towards parent, from child'
            }
            else if (initTarget < nextChild) // is 'initTarget' a descendent of 'child'?
            {
                STATE_T childState = ascend_recurse(parent, child, initState);
                return ascend_aux(parent, child, childState);
            }
            child = nextChild;
        }
        return {}; // unreachable
    }

    STATE_T ascend_aux(TreePos_t const target, TreePos_t const from, STATE_T const& fromState)
    {
        // 'from' is a child of 'target'. Iterate it's siblings
        STATE_T           targetState   = mark.ascend(target, from, fromState);
        auto        const childLast     = target + 1 + rDescendants[target];
        TreePos_t         child         = target + 1;

        while (child != childLast)
        {
            if (child != from) // don't accidentally go back down to where we ascended from
            {
                descend_recurse(child, target, targetState);
            }
            child += 1 + rDescendants[child]; // next child
        }
        return targetState; // RVO
    }

    CUTIE_MARK_T mark;
    osp::KeyedVec<TreePos_t, std::uint32_t> const &rDescendants;
};



struct CospaceTransformCalculator
{
    using TreePos_t = UCtxCoordSpaces::TreePos_t;

    CoordTransformer ascend(TreePos_t const target, TreePos_t const from, CoordTransformer const& fromToScn)
    {
        CoSpaceId        const parent    = rCS.treeToId[target];
        CoSpaceId        const child     = rCS.treeToId[from];
        CospaceTransform const &parentTf = rCS.transformOf[parent];
        CospaceTransform const &childTf  = rCS.transformOf[child];

        CospaceRelationship const relation
        {
            .parentPrecision = parentTf.precision,
            .childPrecision  = childTf.precision,
            .childPos        = childTf.position,
            .childRot        = childTf.rotation,
        };

        CoordTransformer const targetToFrom = CoordTransformer::from_parent_to_child(relation);

        // targetToScn = fromToScn(targetToFrom)
        rCospaceTransformToScnOf[parent] = CoordTransformer::from_composite(fromToScn, targetToFrom);
        return rCospaceTransformToScnOf[parent];
    }

    CoordTransformer descend(TreePos_t const target, TreePos_t const from, CoordTransformer const& fromToScn)
    {
        CoSpaceId        const parent    = rCS.treeToId[from];
        CoSpaceId        const child     = rCS.treeToId[target];
        CospaceTransform const &parentTf = rCS.transformOf[parent];
        CospaceTransform const &childTf  = rCS.transformOf[child];

        // process satellites of parent.

        CospaceRelationship const relation
        {
            .parentPrecision = parentTf.precision,
            .childPrecision  = childTf.precision,
            .childPos        = childTf.position,
            .childRot        = childTf.rotation,
        };

        CoordTransformer const targetToFrom = CoordTransformer::from_child_to_parent(relation);

        // targetToScn = fromToScn(targetToFrom)
        rCospaceTransformToScnOf[child] = CoordTransformer::from_composite(fromToScn, targetToFrom);
        return rCospaceTransformToScnOf[child];
    }

    osp::KeyedVec<CoSpaceId, CoordTransformer>       &rCospaceTransformToScnOf;
    UCtxCoordSpaces                            const &rCS;
};

FeatureDef const ftrUniverseTestPlanetsDraw = feature_def("UniverseTestPlanetsDraw", [] (
        FeatureBuilder              &rFB,
        Implement<FIUniPlanetsDraw> uniPlanetsDraw,
        DependOn<FIMainApp>         mainApp,
        DependOn<FIWindowApp>       windowApp,
        DependOn<FISceneRenderer>   scnRender,
        DependOn<FICameraControl>   camCtrl,
        DependOn<FICommonScene>     comScn,
        DependOn<FISceneInUniverse> scnInUni,
        DependOn<FIUniCore>         uniCore,
        entt::any                   userData)
{
    auto const &params = entt::any_cast<PlanetDrawParams>(userData);

    rFB.pipeline(uniPlanetsDraw.pl.resync).parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniPlanetsDraw.pl.trackedSats).parent(mainApp.loopblks.mainLoop);

    auto &rPlanetDraw  = rFB.data_emplace<PlanetDraw>(uniPlanetsDraw.di.planetDraw);


    rPlanetDraw.planetMat = params.planetMat;

     rFB.task()
        .name       ("Read universe datasource changes")
        .sync_with  ({uniPlanetsDraw.pl.trackedSats(Modify), uniPlanetsDraw.pl.resync(ModifyOrSignal), uniCore.pl.accessorIds(Ready), uniCore.pl.satIds(Ready)})
        .args       ({uniPlanetsDraw.di.planetDraw,          uniCore.di.dataAccessors,               uniCore.di.satInst,        uniCore.di.dataSrcs, uniCore.di.coordSpaces, scnInUni.di.scnCospace})
        .func       ([] (  PlanetDraw &rPlanetDraw, UCtxDataAccessors &rDataAccessors, UCtxSatellites &rSatInst, UCtxDataSources &rDataSrcs, UCtxCoordSpaces const& rCoordSpaces, CoSpaceId scnCospace) noexcept
    {
        if (true)
        {
            //std::cout << "salkjdsabfkjdsafdsaf\n";
            rPlanetDraw.doResync = true;

            rPlanetDraw.accessorsByCospace.clear();
            for (DataAccessorId const accessorId : rDataAccessors.ids)
            {
                rPlanetDraw.accessorsByCospace.push_back(accessorId);
            }

            // TODO: right now this just tracks everything. add conditions later

            rPlanetDraw.trackedAccessors.clear();
            rPlanetDraw.trackedAccessors.resize(rDataAccessors.ids.capacity());
            for (DataAccessorId const accessorId : rDataAccessors.ids)
            {
                rPlanetDraw.trackedAccessors.emplace(accessorId);
            }

            rPlanetDraw.trackedSats.resize(rSatInst.ids.capacity());
            for (SatelliteId const satId : rSatInst.ids)
            {
                rPlanetDraw.trackedSats[satId].isTracking = true;
            }

            rPlanetDraw.doResync = true;
        }
    });

    rFB.task()
        .name       ("Create universe draw entities")
        .sync_with  ({windowApp.pl.sync(Run), uniPlanetsDraw.pl.resync(Run), scnRender.pl.drawEnt(New), uniPlanetsDraw.pl.trackedSats(Ready)})
        .args       ({    uniPlanetsDraw.di.planetDraw,          uniCore.di.dataAccessors,               uniCore.di.satInst, uniCore.di.dataSrcs,                       uniCore.di.compTypes,      scnRender.di.scnRender })
        .func       ([] (      PlanetDraw &rPlanetDraw, UCtxDataAccessors &rDataAccessors, UCtxSatellites &rSatInst, UCtxDataSources &rDataSrcs, UCtxComponentTypes const& compTypes, ACtxSceneRender &rScnRender) noexcept
    {
        if (rPlanetDraw.doResync)
        {
            auto drawEntGen = rScnRender.m_drawIds.generator();

            for (SatelliteId const satId : rSatInst.ids)
            {
                PlanetDraw::TrackedSatellite &rTrackedSat = rPlanetDraw.trackedSats[satId];

                if (rTrackedSat.isTracking)
                {
                    if ( ! rTrackedSat.drawEnt.has_value() )
                    {
                        rTrackedSat.drawEnt = drawEntGen.create();
                    }
                }
            }
        }
    });

    rFB.task()
        .name       ("Add mesh and materials to universe stuff")
        .sync_with  ({windowApp.pl.sync(Run), uniPlanetsDraw.pl.resync(Run), scnRender.pl.drawEnt(Ready), scnRender.pl.mesh(New), scnRender.pl.material(New), uniPlanetsDraw.pl.trackedSats(Ready)})
        .args       ({    uniPlanetsDraw.di.planetDraw,               uniCore.di.satInst,      scnRender.di.scnRender, comScn.di.drawing, comScn.di.namedMeshes })
        .func       ([] (      PlanetDraw &rPlanetDraw, UCtxSatellites &rSatInst, ACtxSceneRender &rScnRender, ACtxDrawing& rDrawing, NamedMeshes& rNamedMeshes) noexcept
    {
        if (rPlanetDraw.doResync)
        {
            // eat everything
            //std::cout << "eat";

            // maybe for each satellite type?
            // or each coordspace?
            //
            // which one makes the most sense?
            // usually want to specify a coordspace. 'what is here'
            // write to roughly the same physical space.
            // better to have per satset/coordspace pair. or datasource
            // (satset,coordspace)-> (list of dataacessors)

            // loop through all dataacessors, and write positions and stuff
            //

            MeshId const sphereMeshId = rNamedMeshes.m_shapeToMesh.at(EShape::Sphere);

            for (SatelliteId const satId : rSatInst.ids)
            {
                PlanetDraw::TrackedSatellite &rTrackedSat = rPlanetDraw.trackedSats[satId];

                if (rTrackedSat.isTracking)
                {
                    rScnRender.m_visible.insert(rTrackedSat.drawEnt);
                    rScnRender.m_opaque .insert(rTrackedSat.drawEnt);

                    if ( ! rScnRender.m_mesh[rTrackedSat.drawEnt].has_value() )
                    {
                        rScnRender.m_mesh[rTrackedSat.drawEnt] = rDrawing.m_meshRefCounts.ref_add(sphereMeshId);
                        rScnRender.m_meshDirty.push_back(rTrackedSat.drawEnt);

                        rScnRender.m_color[rTrackedSat.drawEnt] = {1.0f, 1.0f, 1.0f, 1.0f};

                        rScnRender.m_materials[rPlanetDraw.planetMat].m_ents.insert(rTrackedSat.drawEnt);
                        rScnRender.m_materials[rPlanetDraw.planetMat].m_dirty.push_back(rTrackedSat.drawEnt);
                    }
                }
            }
        }
    });

    rFB.task()
        .name       ("write draw transforms")
        .sync_with  ({windowApp.pl.sync(Run), uniPlanetsDraw.pl.resync(Run), scnRender.pl.drawEnt(Ready), scnRender.pl.mesh(New), scnRender.pl.material(New), uniCore.pl.accessors(Ready), uniCore.pl.accessorIds(Ready), uniCore.pl.cospaceTransform(Ready), uniPlanetsDraw.pl.trackedSats(Ready)})
        .args       ({    uniPlanetsDraw.di.planetDraw,          uniCore.di.dataAccessors,              uniCore.di.coordSpaces,        uniCore.di.simulations,             uniCore.di.stolenSats,               uniCore.di.satInst,        uniCore.di.dataSrcs,                uniCore.di.compTypes,      scnRender.di.scnRender, scnInUni.di.scnCospace})
        .func       ([] (      PlanetDraw &rPlanetDraw, UCtxDataAccessors &rDataAccessors, UCtxCoordSpaces const& rCoordSpaces, UCtxSimulations &rSimulations, UCtxStolenSatellites &rStolenSats, UCtxSatellites &rSatInst, UCtxDataSources &rDataSrcs, UCtxComponentTypes const& compTypes, ACtxSceneRender &rScnRender,   CoSpaceId scnCospace) noexcept
    {
        rPlanetDraw.cospaceTransformToScnOf.resize(rCoordSpaces.ids.capacity());

        // update rPlanetDraw.transformerOf
        TreeWalker<CoordTransformer, CospaceTransformCalculator> walker
        {
            .mark = CospaceTransformCalculator{ rPlanetDraw.cospaceTransformToScnOf, rCoordSpaces },
            .rDescendants = rCoordSpaces.treeDescendants
        };
        walker.run(rCoordSpaces.treeposOf[scnCospace], {});

        DefaultComponents const &dc = compTypes.defaults;
        for (DataAccessorId const accessorId : rPlanetDraw.trackedAccessors)
        {
            DataAccessor &rAccessor = rDataAccessors.instances[accessorId];

            UCtxStolenSatellites::OfAccessor const& deleted = rStolenSats.of[accessorId];

            CoordTransformer const &transformer = rPlanetDraw.cospaceTransformToScnOf[rAccessor.cospace];

            if (rAccessor.iterMethod == DataAccessor::IterationMethod::SkipNullSatellites)
            {
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

                float const timeBehindBy = rAccessor.owner.has_value() ? rSimulations.simulationOf[rAccessor.owner].timeBehindBy * 0.001f : 0.0f;


                for (std::size_t i = 0; i < rAccessor.count; ++i, iter.next())
                {
                    SatelliteId const satId = iter.get<SatelliteId>(13);

                    if (deleted.dirty && deleted.sats.contains(satId))
                    {
                        continue;
                    }

                    Vector3 moved{0.0f, 0.0f, 0.0f};

                    if (hasVelXYZ)
                    {
                        Vector3 const velocity {iter.get<float>(3), iter.get<float>(4), iter.get<float>(5)};
                        moved = velocity * timeBehindBy;
                    }

                    if (hasVelXYZd)
                    {
                        Vector3d const velocity {iter.get<double>(6), iter.get<double>(7), iter.get<double>(8)};
                        moved = Vector3(velocity * timeBehindBy);
                    }

                    if (hasPosXYZ)
                    {
                        Vector3g const pos {iter.get<spaceint_t>(0), iter.get<spaceint_t>(1), iter.get<spaceint_t>(2)};
                        Vector3d const d = Vector3d(transformer.transforposition(pos));
                        Vector3 const qux = Vector3(d / 1024.0) + moved;

                        PlanetDraw::TrackedSatellite &rTrackedSat = rPlanetDraw.trackedSats[satId];

                        LGRN_ASSERT(rTrackedSat.drawEnt.has_value());

                        rScnRender.m_drawTransform[rTrackedSat.drawEnt].translation() = qux;
                    }

                    if (hasRotXYZW)
                    {
                        Quaternion const rot { {iter.get<float>(9), iter.get<float>(10), iter.get<float>(11)}, iter.get<float>(12)};

                        PlanetDraw::TrackedSatellite &rTrackedSat = rPlanetDraw.trackedSats[satId];

                        LGRN_ASSERT(rTrackedSat.drawEnt.has_value());

                        Matrix3 const foo = rot.toMatrix();
                        rScnRender.m_drawTransform[rTrackedSat.drawEnt][0].xyz() = foo[0];
                        rScnRender.m_drawTransform[rTrackedSat.drawEnt][1].xyz() = foo[1];
                        rScnRender.m_drawTransform[rTrackedSat.drawEnt][2].xyz() = foo[2];
                    }
                }
            }
        }
    });

    rFB.task()
        .name       ("resync done")
        .sync_with  ({windowApp.pl.sync(Run), uniPlanetsDraw.pl.resync(Done)})
        .args       ({uniPlanetsDraw.di.planetDraw})
        .func       ([] (PlanetDraw &rPlanetDraw) noexcept
    {
        // for each
        rPlanetDraw.doResync = false;
    });

}); // setup_testplanets_draw


} // namespace adera
