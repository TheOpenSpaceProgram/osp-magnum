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
    auto &rSatInst          = rFB.data_emplace< UCtxSatelliteInstances >(uniCore.di.satInst);
    auto &rSimulations      = rFB.data_emplace< UCtxSimulations >       (uniCore.di.simulations);
    auto &rIntakes          = rFB.data_emplace< UCtxIntakes >           (uniTransfers.di.intakes);
    auto &rTransferBufs     = rFB.data_emplace< UCtxTransferBuffers >   (uniTransfers.di.transferBufs, rSimulations.ids.create());

    rFB.pipeline(uniCore.pl.update)                 .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.satIds)                 .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.transfer)               .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.accessorIds)            .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(uniCore.pl.accessors)              .parent(mainApp.loopblks.mainLoop);
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


    // DataSources ------------------------------------------------------------

    rFB.task()
        .name       ("Resize datasrcOf")
        .sync_with  ({uniCore.pl.datasrcOf(Resize_), uniCore.pl.satIds(Ready)  })
        .args       ({          uniCore.di.dataSrcs,               uniCore.di.satInst })
        .func       ([] (UCtxDataSources &rDataSrcs, UCtxSatelliteInstances &rSatInst) noexcept
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
                .satToIndex = {}, // TODO
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
        Implement<FIUniSceneInUniverse> uniScnFrame,
        DependOn<FIMainApp>             mainApp,
        DependOn<FIUniCore>             uniCore)
{
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


    osp::KeyedVec<SatelliteId, TrackedSatellite>    trackedSats;
    lgrn::IdSetStl<DataAccessorId>                  trackedAccessors;

    DrawEntVec_t            drawEnts;
    std::array<DrawEnt, 3>  axis;
    DrawEnt                 attractor;
    MaterialId              planetMat;
    MaterialId              axisMat;

};


FeatureDef const ftrUniverseTestPlanetsDraw = feature_def("UniverseTestPlanetsDraw", [] (
        FeatureBuilder              &rFB,
        Implement<FIUniPlanetsDraw> uniPlanetsDraw,
        DependOn<FIMainApp>         mainApp,
        DependOn<FIWindowApp>       windowApp,
        DependOn<FISceneRenderer>   scnRender,
        DependOn<FICameraControl>   camCtrl,
        DependOn<FICommonScene>     comScn,
        DependOn<FIUniCore>         uniCore,
//        DependOn<FIUniSceneFrame>   uniScnFrame,
//        DependOn<FIUniPlanets>      uniPlanets,
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
        .args       ({uniPlanetsDraw.di.planetDraw,          uniCore.di.dataAccessors,               uniCore.di.satInst,        uniCore.di.dataSrcs})
        .func       ([] (  PlanetDraw &rPlanetDraw, UCtxDataAccessors &rDataAccessors, UCtxSatelliteInstances &rSatInst, UCtxDataSources &rDataSrcs) noexcept
    {
        if (true)
        {
            //std::cout << "salkjdsabfkjdsafdsaf\n";
            rPlanetDraw.doResync = true;

            //ComponentTypeIdSet_t compsRequired = component_type_set({compDefaults.posX, compDefaults.posY, compDefaults.posZ});

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


//    rFB.task()
//        .name       ("Read universe changes")
//        .sync_with  ({windowApp.pl.sync(Run), uniPlanetsDraw.pl.resync(ModifyOrSignal), uniCore.pl.datasrcs(Ready), uniCore.pl.accessors(Ready), uniCore.pl.accessorIds(Ready)})
//        .args       ({uniPlanetsDraw.di.planetDraw,          uniCore.di.dataAccessors,               uniCore.di.satInst,        uniCore.di.dataSrcs,                   uniCore.di.compDefaults})
//        .func       ([] (  PlanetDraw &rPlanetDraw, UCtxDataAccessors &rDataAccessors, UCtxSatelliteInstances &rSatInst, UCtxDataSources &rDataSrcs, UCtxDefaultComponents const& compDefaults) noexcept
//    {
//        if ( ! rPlanetDraw.connected)
//        {
//            rPlanetDraw.connected = true;
//            // TODO: Subscribe to events
//            // TODO: somehow modify pipelines to connect to universe?
//        }
//        // on subscriber added, just push them all events so they don't directly just read?

//        // deal with events. satellites added/removed/transfered and stuff

//        // loop through all dataaccessors that we can and actually read positions
//        // the problem here is the 'resync' stage.
//        // how to initially discover all the stuff in the universe?
//    });

    rFB.task()
        .name       ("Create universe draw entities")
        .sync_with  ({windowApp.pl.sync(Run), uniPlanetsDraw.pl.resync(Run), scnRender.pl.drawEnt(New), uniPlanetsDraw.pl.trackedSats(Ready)})
        .args       ({    uniPlanetsDraw.di.planetDraw,          uniCore.di.dataAccessors,               uniCore.di.satInst, uniCore.di.dataSrcs,                       uniCore.di.compTypes,      scnRender.di.scnRender })
        .func       ([] (      PlanetDraw &rPlanetDraw, UCtxDataAccessors &rDataAccessors, UCtxSatelliteInstances &rSatInst, UCtxDataSources &rDataSrcs, UCtxComponentTypes const& compTypes, ACtxSceneRender &rScnRender) noexcept
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
        .func       ([] (      PlanetDraw &rPlanetDraw, UCtxSatelliteInstances &rSatInst, ACtxSceneRender &rScnRender, ACtxDrawing& rDrawing, NamedMeshes& rNamedMeshes) noexcept
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
        .sync_with  ({windowApp.pl.sync(Run), uniPlanetsDraw.pl.resync(Run), scnRender.pl.drawEnt(Ready), scnRender.pl.mesh(New), scnRender.pl.material(New), uniCore.pl.accessors(Ready), uniCore.pl.accessorIds(Ready), uniPlanetsDraw.pl.trackedSats(Ready)})
        .args       ({    uniPlanetsDraw.di.planetDraw,          uniCore.di.dataAccessors,        uniCore.di.simulations,             uniCore.di.stolenSats,               uniCore.di.satInst,        uniCore.di.dataSrcs,                uniCore.di.compTypes,      scnRender.di.scnRender })
        .func       ([] (      PlanetDraw &rPlanetDraw, UCtxDataAccessors &rDataAccessors, UCtxSimulations &rSimulations, UCtxStolenSatellites &rStolenSats, UCtxSatelliteInstances &rSatInst, UCtxDataSources &rDataSrcs, UCtxComponentTypes const& compTypes, ACtxSceneRender &rScnRender) noexcept
    {
        DefaultComponents const &dc = compTypes.defaults;
        for (DataAccessorId const accessorId : rPlanetDraw.trackedAccessors)
        {
            DataAccessor &rAccessor = rDataAccessors.instances[accessorId];

            UCtxStolenSatellites::Accessor const& deleted = rStolenSats.of[accessorId];

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

                LGRN_ASSERTM(iter.has(13), "other iteration methods not yet supported");

                float const timeBehindBy = rAccessor.owner.has_value() ? rSimulations.simulationOf[rAccessor.owner].timeBehindBy * 0.001f : 0.0f;


                for (std::size_t i = 0; i < rAccessor.count; ++i)
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

                        Vector3d const d = Vector3d(pos);
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

                    iter.next();
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

        #if 0
    auto const params = entt::any_cast<PlanetDrawParams>(userData);

    rFB.data_emplace<PlanetDraw>(uniPlanetsDraw.di.planetDraw, PlanetDraw{
        .planetMat = params.planetMat,
        .axisMat   = params.axisMat,
    });

    rFB.task()
        .name       ("Position SceneFrame center to Camera Controller target")
        .run_on     ({windowApp.pl.inputs(Run)})
        .sync_with  ({camCtrl.pl.camCtrl(Ready), uniScnFrame.pl.sceneFrame(Modify)})
        .args       ({               camCtrl.di.camCtrl, uniScnFrame.di.scnFrame })
        .func       ([] (ACtxCameraController& rCamCtrl,   SceneFrame& rScnFrame) noexcept
    {
        if ( ! rCamCtrl.m_target.has_value())
        {
            return;
        }
        Vector3 &rCamPl = rCamCtrl.m_target.value();

        // check origin translation
        // ADL used for Magnum::Math::sign/floor/abs
        float const maxDist = 512.0f;
        Vector3 const translate = sign(rCamPl) * floor(abs(rCamPl) / maxDist) * maxDist;

        if ( ! translate.isZero())
        {
            rCamCtrl.m_transform.translation() -= translate;
            rCamPl -= translate;

            // a bit janky to modify universe stuff directly here, but it works lol
            Vector3 const rotated = Quaternion(rScnFrame.m_rotation).transformVector(translate);
            rScnFrame.m_position += Vector3g(math::mul_2pow<Vector3, int>(rotated, rScnFrame.m_precision));
        }

        rScnFrame.m_scenePosition = Vector3g(math::mul_2pow<Vector3, int>(rCamCtrl.m_target.value(), rScnFrame.m_precision));

    });

    rFB.task()
        .name       ("Resync test planets, create DrawEnts")
        .run_on     ({windowApp.pl.resync(Run)})
        .sync_with  ({scnRender.pl.drawEntResized(ModifyOrSignal)})
        .args       ({    scnRender.di.scnRender, uniPlanetsDraw.di.planetDraw, uniCore.di.universe,   uniPlanets.di.planetMainSpace})
        .func([]    (ACtxSceneRender& rScnRender,      PlanetDraw& rPlanetDraw, Universe& rUniverse, CoSpaceId const planetMainSpace) noexcept
    {
        CoSpaceCommon &rMainSpace = rUniverse.m_coordCommon[planetMainSpace];

        rPlanetDraw.drawEnts.resize(rMainSpace.m_satCount, lgrn::id_null<DrawEnt>());

        rScnRender.m_drawIds.create(rPlanetDraw.drawEnts   .begin(), rPlanetDraw.drawEnts   .end());
        rScnRender.m_drawIds.create(rPlanetDraw.axis       .begin(), rPlanetDraw.axis       .end());
        rPlanetDraw.attractor = rScnRender.m_drawIds.create();
    });

    rFB.task()
        .name       ("Resync test planets, add mesh and material")
        .run_on     ({windowApp.pl.resync(Run)})
        .sync_with  ({scnRender.pl.drawEntResized(Done), scnRender.pl.materialDirty(Modify_), scnRender.pl.meshDirty(Modify_)})
        .args       ({       comScn.di.drawing,      scnRender.di.scnRender,     comScn.di.namedMeshes, uniPlanetsDraw.di.planetDraw, uniCore.di.universe,   uniPlanets.di.planetMainSpace})
        .func       ([] (ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, NamedMeshes& rNamedMeshes,      PlanetDraw& rPlanetDraw, Universe& rUniverse, CoSpaceId const planetMainSpace) noexcept
    {
        CoSpaceCommon &rMainSpace = rUniverse.m_coordCommon[planetMainSpace];

        Material &rMatPlanet = rScnRender.m_materials[rPlanetDraw.planetMat];
        Material &rMatAxis   = rScnRender.m_materials[rPlanetDraw.axisMat];

        MeshId const sphereMeshId = rNamedMeshes.m_shapeToMesh.at(EShape::Sphere);
        MeshId const cubeMeshId   = rNamedMeshes.m_shapeToMesh.at(EShape::Box);

        for (std::size_t i = 0; i < rMainSpace.m_satCount; ++i)
        {
            DrawEnt const drawEnt = rPlanetDraw.drawEnts[i];

            rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(sphereMeshId);
            rScnRender.m_meshDirty.push_back(drawEnt);
            rScnRender.m_visible.insert(drawEnt);
            rScnRender.m_opaque.insert(drawEnt);
            rMatPlanet.m_ents.insert(drawEnt);
            rMatPlanet.m_dirty.push_back(drawEnt);
        }

        rScnRender.m_mesh[rPlanetDraw.attractor] = rDrawing.m_meshRefCounts.ref_add(sphereMeshId);
        rScnRender.m_meshDirty.push_back(rPlanetDraw.attractor);
        rScnRender.m_visible.insert(rPlanetDraw.attractor);
        rScnRender.m_opaque.insert(rPlanetDraw.attractor);
        rMatPlanet.m_ents.insert(rPlanetDraw.attractor);
        rMatPlanet.m_dirty.push_back(rPlanetDraw.attractor);

        for (DrawEnt const drawEnt : rPlanetDraw.axis)
        {
            rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(cubeMeshId);
            rScnRender.m_meshDirty.push_back(drawEnt);
            rScnRender.m_visible.insert(drawEnt);
            rScnRender.m_opaque.insert(drawEnt);
            rMatAxis.m_ents.insert(drawEnt);
            rMatAxis.m_dirty.push_back(drawEnt);
        }

        rScnRender.m_color[rPlanetDraw.axis[0]] = {1.0f, 0.0f, 0.0f, 1.0f};
        rScnRender.m_color[rPlanetDraw.axis[1]] = {0.0f, 1.0f, 0.0f, 1.0f};
        rScnRender.m_color[rPlanetDraw.axis[2]] = {0.0f, 0.0f, 1.0f, 1.0f};
    });

    rFB.task()
        .name       ("Reposition test planet DrawEnts")
        .run_on     ({scnRender.pl.render(Run)})
        .sync_with  ({scnRender.pl.drawTransforms(Modify), scnRender.pl.drawEntResized(Done), camCtrl.pl.camCtrl(Ready), uniScnFrame.pl.sceneFrame(Modify)})
        .args       ({       comScn.di.drawing,      scnRender.di.scnRender, uniPlanetsDraw.di.planetDraw, uniCore.di.universe,     uniScnFrame.di.scnFrame,   uniPlanets.di.planetMainSpace})
        .func       ([] (ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender,      PlanetDraw& rPlanetDraw, Universe& rUniverse, SceneFrame const& rScnFrame, CoSpaceId const planetMainSpace) noexcept
    {
        CoSpaceCommon &rMainSpace = rUniverse.m_coordCommon[planetMainSpace];
        auto const [x, y, z]        = sat_views(rMainSpace.m_satPositions, rMainSpace.m_data, rMainSpace.m_satCount);
        auto const [qx, qy, qz, qw] = sat_views(rMainSpace.m_satRotations, rMainSpace.m_data, rMainSpace.m_satCount);

        // Calculate transform from universe to area/local-space for rendering.
        // This can be generalized by finding a common ancestor within the tree
        // of coordinate spaces. Since there's only two possibilities, an if
        // statement works.
        CoordTransformer mainToArea;
        if (rScnFrame.m_parent == planetMainSpace)
        {
            mainToArea = coord_parent_to_child(rMainSpace, rScnFrame);
        }
        else
        {
            CoSpaceId const landedId = rScnFrame.m_parent;
            CoSpaceCommon &rLanded = rUniverse.m_coordCommon[landedId];

            CoSpaceTransform const landedTf     = coord_get_transform(rLanded, rLanded, x, y, z, qx, qy, qz, qw);
            CoordTransformer const mainToLanded = coord_parent_to_child(rMainSpace, landedTf);
            CoordTransformer const landedToArea = coord_parent_to_child(landedTf, rScnFrame);

            mainToArea = coord_composite(landedToArea, mainToLanded);
        }
        Quaternion const mainToAreaRot{mainToArea.rotation()};

        float const scale = math::mul_2pow<float, int>(1.0f, -rMainSpace.m_precision);

        Vector3 const attractorPos = Vector3(mainToArea.transform_position({0, 0, 0})) * scale;

        // Attractor
        rScnRender.m_drawTransform[rPlanetDraw.attractor]
            = Matrix4::translation(attractorPos)
            * Matrix4{mainToAreaRot.toMatrix()}
            * Matrix4::scaling({500, 500, 500});

        rScnRender.m_drawTransform[rPlanetDraw.axis[0]]
            = Matrix4::translation(attractorPos)
            * Matrix4{mainToAreaRot.toMatrix()}
            * Matrix4::scaling({500000, 10, 10});
        rScnRender.m_drawTransform[rPlanetDraw.axis[1]]
            = Matrix4::translation(attractorPos)
            * Matrix4{mainToAreaRot.toMatrix()}
            * Matrix4::scaling({10, 500000, 10});
        rScnRender.m_drawTransform[rPlanetDraw.axis[2]]
            = Matrix4::translation(attractorPos)
            * Matrix4{mainToAreaRot.toMatrix()}
            * Matrix4::scaling({10, 10, 500000});

        for (std::size_t i = 0; i < rMainSpace.m_satCount; ++i)
        {
            Vector3g const relative = mainToArea.transform_position({x[i], y[i], z[i]});
            Vector3 const relativeMeters = Vector3(relative) * scale;

            Quaterniond const rot{{qx[i], qy[i], qz[i]}, qw[i]};

            DrawEnt const drawEnt = rPlanetDraw.drawEnts[i];

            rScnRender.m_drawTransform[drawEnt]
                = Matrix4::translation(relativeMeters)
                * Matrix4::scaling({200, 200, 200})
                * Matrix4{(mainToAreaRot * Quaternion{rot}).toMatrix()};
        }
    });
#endif
}); // setup_testplanets_draw



// Solar System Scenario

constexpr unsigned int c_planetCount = 5;

FeatureDef const ftrSolarSystem = feature_def("SolarSystem", [] (
        FeatureBuilder              &rFB,
        Implement<FISolarSys>       solarSys,
        DependOn<FIUniCore>         uniCore)
{
        #if 0
    using CoSpaceIdVec_t = std::vector<CoSpaceId>;
    using Corrade::Containers::Array;

    auto& rUniverse = rFB.data_get< Universe >(uniCore.di.universe);

    constexpr int precision = 10;

    // Create coordinate spaces
    CoSpaceId const mainSpace = rUniverse.m_coordIds.create();
    std::vector<CoSpaceId> satSurfaceSpaces(c_planetCount);
    rUniverse.m_coordIds.create(satSurfaceSpaces.begin(), satSurfaceSpaces.end());

    rUniverse.m_coordCommon.resize(rUniverse.m_coordIds.capacity());

    CoSpaceCommon& rMainSpaceCommon = rUniverse.m_coordCommon[mainSpace];
    rMainSpaceCommon.m_satCount = c_planetCount;
    rMainSpaceCommon.m_satCapacity = c_planetCount;

    auto& rCoordNBody = rFB.data_emplace< osp::KeyedVec<CoSpaceId, CoSpaceNBody> >(solarSys.di.coordNBody);
    rCoordNBody.resize(rUniverse.m_coordIds.capacity());

    // Associate each planet satellite with their surface coordinate space
    for (SatId satId = 0; satId < c_planetCount; ++satId)
    {
        CoSpaceId const surfaceSpaceId = satSurfaceSpaces[satId];
        CoSpaceCommon& rCommon = rUniverse.m_coordCommon[surfaceSpaceId];
        rCommon.m_parent = mainSpace;
        rCommon.m_parentSat = satId;
    }

    // Coordinate space data is a single allocation partitioned to hold positions, velocities, and
    // rotations.
    // TODO: Alignment is needed for SIMD (not yet implemented). see Corrade alignedAlloc

    // Positions and velocities are arranged as XXXX... YYYY... ZZZZ...
    osp::BufferFormatBuilder fb;
    rMainSpaceCommon.m_satPositions [0] = fb.insert_block<spaceint_t>(c_planetCount);
    rMainSpaceCommon.m_satPositions [1] = fb.insert_block<spaceint_t>(c_planetCount);
    rMainSpaceCommon.m_satPositions [2] = fb.insert_block<spaceint_t>(c_planetCount);
    rMainSpaceCommon.m_satVelocities[0] = fb.insert_block<double>(c_planetCount);
    rMainSpaceCommon.m_satVelocities[1] = fb.insert_block<double>(c_planetCount);
    rMainSpaceCommon.m_satVelocities[2] = fb.insert_block<double>(c_planetCount);

    // Rotations use XYZWXYZWXYZWXYZW...
    fb.insert_interleave(c_planetCount, rMainSpaceCommon.m_satRotations[0],
                                        rMainSpaceCommon.m_satRotations[1],
                                        rMainSpaceCommon.m_satRotations[2],
                                        rMainSpaceCommon.m_satRotations[3]);

    rCoordNBody[mainSpace].mass = fb.insert_block<float>(c_planetCount);
    rCoordNBody[mainSpace].radius = fb.insert_block<float>(c_planetCount);
    rCoordNBody[mainSpace].color = fb.insert_block<Magnum::Color3>(c_planetCount);

    // Allocate data for all planets
    rMainSpaceCommon.m_data = Array<std::byte>{ Corrade::NoInit, fb.total_size() };

    std::size_t nextBody = 0;
    auto const add_body = [&rMainSpaceCommon, &nextBody, &rCoordNBody, &mainSpace] (
        Vector3l position,
        Vector3d velocity,
        Quaternion rotation,
        float mass,
        float radius,
        Magnum::Color3 color)
    {
        auto const [x, y, z] = sat_views(rMainSpaceCommon.m_satPositions, rMainSpaceCommon.m_data, c_planetCount);
        auto const [vx, vy, vz] = sat_views(rMainSpaceCommon.m_satVelocities, rMainSpaceCommon.m_data, c_planetCount);
        auto const [qx, qy, qz, qw] = sat_views(rMainSpaceCommon.m_satRotations, rMainSpaceCommon.m_data, c_planetCount);

        auto const massView = rCoordNBody[mainSpace].mass.view(arrayView(rMainSpaceCommon.m_data), c_planetCount);
        auto const radiusView = rCoordNBody[mainSpace].radius.view(arrayView(rMainSpaceCommon.m_data), c_planetCount);
        auto const colorView = rCoordNBody[mainSpace].color.view(arrayView(rMainSpaceCommon.m_data), c_planetCount);

        x[nextBody] = position.x();
        y[nextBody] = position.y();
        z[nextBody] = position.z();

        vx[nextBody] = velocity.x();
        vy[nextBody] = velocity.y();
        vz[nextBody] = velocity.z();

        qx[nextBody] = rotation.vector().x();
        qy[nextBody] = rotation.vector().y();
        qz[nextBody] = rotation.vector().z();
        qw[nextBody] = rotation.scalar();

        massView[nextBody] = mass;
        radiusView[nextBody] = radius;
        colorView[nextBody] = color;

        ++nextBody;
    };

    // Sun
    add_body(
        { 0, 0, 0 },
        { 0.0, 0.0, 0.0 },
        Quaternion::rotation(Rad{ 0.0f }, Vector3{ 1.0f, 0.0f, 0.0f }),
        1.0f * std::pow(10, 1),
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

    rFB.data_emplace< CoSpaceId >       (solarSys.di.planetMainSpace, mainSpace);
    rFB.data_emplace< CoSpaceIdVec_t >  (solarSys.di.satSurfaceSpaces, std::move(satSurfaceSpaces));

    // Set initial scene frame

    auto& rScnFrame = rFB.data_get<SceneFrame>(uniScnFrame.di.scnFrame);
    rScnFrame.m_parent = mainSpace;
    rScnFrame.m_position = math::mul_2pow<Vector3g, int>({ 400, 400, 400 }, precision);

    rFB.task()
        .name       ("Update planets")
        .run_on     (uniCore.pl.update(Run))
        .sync_with  ({ uniScnFrame.pl.sceneFrame(Modify) })
        .args       ({  uniCore.di.universe,     solarSys.di.planetMainSpace, uniScnFrame.di.scnFrame,            solarSys.di.satSurfaceSpaces,     uniCore.di.deltaTimeIn,                              solarSys.di.coordNBody })
        .func       ([](Universe& rUniverse, CoSpaceId const planetMainSpace,   SceneFrame& rScnFrame, CoSpaceIdVec_t const& rSatSurfaceSpaces, float const uniDeltaTimeIn, osp::KeyedVec<CoSpaceId, CoSpaceNBody>& rCoordNBody) noexcept
    {
        CoSpaceCommon& rMainSpaceCommon = rUniverse.m_coordCommon[planetMainSpace];

        auto const scale = osp::math::mul_2pow<double, int>(1.0, -rMainSpaceCommon.m_precision);
        double const scaleDelta = uniDeltaTimeIn / scale;

        auto const [x, y, z] = sat_views(rMainSpaceCommon.m_satPositions, rMainSpaceCommon.m_data, rMainSpaceCommon.m_satCount);
        auto const [vx, vy, vz] = sat_views(rMainSpaceCommon.m_satVelocities, rMainSpaceCommon.m_data, rMainSpaceCommon.m_satCount);

        auto const massView = rCoordNBody[planetMainSpace].mass.view(arrayView(rMainSpaceCommon.m_data), c_planetCount);

        for (std::size_t i = 0; i < rMainSpaceCommon.m_satCount; ++i) {
            x[i] += vx[i] * scaleDelta;
            y[i] += vy[i] * scaleDelta;
            z[i] += vz[i] * scaleDelta;

            for (std::size_t j = 0; j < rMainSpaceCommon.m_satCount; ++j) {
                if (i == j) { continue; }

                double iMass = massView[i];
                double jMass = massView[j];

                Vector3d const iPos = Vector3d(Vector3g(x[i], y[i], z[i])) * scale;
                Vector3d const jPos = Vector3d(Vector3g(x[j], y[j], z[j])) * scale;

                double r = (jPos - iPos).length();
                Vector3d direction = (jPos - iPos).normalized();

                double forceMagnitude = (iMass * jMass) / (r * r);
                Vector3d force = direction * forceMagnitude;
                Vector3d acceleration = (force / iMass);

                vx[i] += acceleration.x() * uniDeltaTimeIn;
                vy[i] += acceleration.y() * uniDeltaTimeIn;
                vz[i] += acceleration.z() * uniDeltaTimeIn;
            }
        }
    });
#endif
}); // ftrSolarSystemPlanets



FeatureDef const ftrSolarSystemDraw = feature_def("SolarSystemDraw", [] (
        FeatureBuilder              &rFB,
        Implement<FIUniPlanetsDraw> uniPlanetsDraw,
        DependOn<FIWindowApp>       windowApp,
        DependOn<FISceneRenderer>   scnRender,
        DependOn<FICameraControl>   camCtrl,
        DependOn<FICommonScene>     comScn,
        DependOn<FIUniCore>         uniCore,
        DependOn<FISolarSys>        solarSys,
        entt::any                   userData)
{
        #if 0
    auto const params = entt::any_cast<PlanetDrawParams>(userData);

    rFB.data_emplace<PlanetDraw>(uniPlanetsDraw.di.planetDraw, PlanetDraw{
        .planetMat = params.planetMat,
        .axisMat   = params.axisMat,
    });

    rFB.task()
        .name       ("Position SceneFrame center to Camera Controller target")
        .run_on     ({ windowApp.pl.inputs(Run) })
        .sync_with  ({ camCtrl.pl.camCtrl(Ready), uniScnFrame.pl.sceneFrame(Modify) })
        .args       ({              camCtrl.di.camCtrl, uniScnFrame.di.scnFrame })
        .func       ([](ACtxCameraController& rCamCtrl,   SceneFrame& rScnFrame) noexcept
    {
        if (!rCamCtrl.m_target.has_value())
        {
            return;
        }
        Vector3& rCamPl = rCamCtrl.m_target.value();

        // check origin translation
        // ADL used for Magnum::Math::sign/floor/abs
        float const maxDist = 512.0f;
        Vector3 const translate = sign(rCamPl) * floor(abs(rCamPl) / maxDist) * maxDist;

        if (!translate.isZero())
        {
            rCamCtrl.m_transform.translation() -= translate;
            rCamPl -= translate;

            // a bit janky to modify universe stuff directly here, but it works lol
            Vector3 const rotated = Quaternion(rScnFrame.m_rotation).transformVector(translate);
            rScnFrame.m_position += Vector3g(math::mul_2pow<Vector3, int>(rotated, rScnFrame.m_precision));
        }

        rScnFrame.m_scenePosition = Vector3g(math::mul_2pow<Vector3, int>(rCamCtrl.m_target.value(), rScnFrame.m_precision));
    });

    rFB.task()
        .name       ("Resync test planets, create DrawEnts")
        .run_on     ({ windowApp.pl.resync(Run) })
        .sync_with  ({ scnRender.pl.drawEntResized(ModifyOrSignal) })
        .args       ({       scnRender.di.scnRender, uniPlanetsDraw.di.planetDraw, uniCore.di.universe,     solarSys.di.planetMainSpace })
        .func       ([](ACtxSceneRender& rScnRender,      PlanetDraw& rPlanetDraw, Universe& rUniverse, CoSpaceId const planetMainSpace) noexcept
    {
        CoSpaceCommon& rMainSpace = rUniverse.m_coordCommon[planetMainSpace];

        rPlanetDraw.drawEnts.resize(rMainSpace.m_satCount, lgrn::id_null<DrawEnt>());
        rScnRender.m_drawIds.create(rPlanetDraw.drawEnts.begin(), rPlanetDraw.drawEnts.end());
    });

    rFB.task()
        .name       ("Resync test planets, add mesh and material")
        .run_on     ({ windowApp.pl.resync(Run) })
        .sync_with  ({ scnRender.pl.drawEntResized(Done), scnRender.pl.materialDirty(Modify_), scnRender.pl.meshDirty(Modify_) })
        .args       ({      comScn.di.drawing,      scnRender.di.scnRender,     comScn.di.namedMeshes, uniPlanetsDraw.di.planetDraw, uniCore.di.universe,     solarSys.di.planetMainSpace,                              solarSys.di.coordNBody })
        .func       ([](ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, NamedMeshes& rNamedMeshes,      PlanetDraw& rPlanetDraw, Universe& rUniverse, CoSpaceId const planetMainSpace, osp::KeyedVec<CoSpaceId, CoSpaceNBody>& rCoordNBody) noexcept
    {
        CoSpaceCommon& rMainSpace = rUniverse.m_coordCommon[planetMainSpace];

        Material& rMatPlanet = rScnRender.m_materials[rPlanetDraw.planetMat];

        MeshId const sphereMeshId = rNamedMeshes.m_shapeToMesh.at(EShape::Sphere);

        CoSpaceCommon& rMainSpaceCommon = rUniverse.m_coordCommon[planetMainSpace];
        auto const colorView = rCoordNBody[planetMainSpace].color.view(arrayView(rMainSpaceCommon.m_data), c_planetCount);

        for (std::size_t i = 0; i < rMainSpace.m_satCount; ++i)
        {
            DrawEnt const drawEnt = rPlanetDraw.drawEnts[i];

            rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(sphereMeshId);
            rScnRender.m_meshDirty.push_back(drawEnt);
            rScnRender.m_visible.insert(drawEnt);
            rScnRender.m_opaque.insert(drawEnt);
            rMatPlanet.m_ents.insert(drawEnt);
            rMatPlanet.m_dirty.push_back(drawEnt);

            rScnRender.m_color[drawEnt] = colorView[i];
        }
    });

    rFB.task()
        .name       ("Reposition test planet DrawEnts")
        .run_on     ({ scnRender.pl.render(Run) })
        .sync_with  ({ scnRender.pl.drawTransforms(Modify), scnRender.pl.drawEntResized(Done), camCtrl.pl.camCtrl(Ready), uniScnFrame.pl.sceneFrame(Modify) })
        .args       ({       comScn.di.drawing,      scnRender.di.scnRender, uniPlanetsDraw.di.planetDraw, uniCore.di.universe,     uniScnFrame.di.scnFrame,     solarSys.di.planetMainSpace,                              solarSys.di.coordNBody })
        .func       ([] (ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender,      PlanetDraw& rPlanetDraw, Universe& rUniverse, SceneFrame const& rScnFrame, CoSpaceId const planetMainSpace, osp::KeyedVec<CoSpaceId, CoSpaceNBody>& rCoordNBody) noexcept
    {
        CoSpaceCommon& rMainSpace = rUniverse.m_coordCommon[planetMainSpace];
        auto const [x, y, z] = sat_views(rMainSpace.m_satPositions, rMainSpace.m_data, rMainSpace.m_satCount);
        auto const [qx, qy, qz, qw] = sat_views(rMainSpace.m_satRotations, rMainSpace.m_data, rMainSpace.m_satCount);
        auto const radiusView = rCoordNBody[planetMainSpace].radius.view(arrayView(rMainSpace.m_data), c_planetCount);

        // Calculate transform from universe to area/local-space for rendering.
        // This can be generalized by finding a common ancestor within the tree
        // of coordinate spaces. Since there's only two possibilities, an if
        // statement works.
        CoordTransformer mainToArea;
        if (rScnFrame.m_parent == planetMainSpace)
        {
            mainToArea = coord_parent_to_child(rMainSpace, rScnFrame);
        }
        else
        {
            CoSpaceId const landedId = rScnFrame.m_parent;
            CoSpaceCommon& rLanded = rUniverse.m_coordCommon[landedId];

            CoSpaceTransform const landedTf = coord_get_transform(rLanded, rLanded, x, y, z, qx, qy, qz, qw);
            CoordTransformer const mainToLanded = coord_parent_to_child(rMainSpace, landedTf);
            CoordTransformer const landedToArea = coord_parent_to_child(landedTf, rScnFrame);

            mainToArea = coord_composite(landedToArea, mainToLanded);
        }
        Quaternion const mainToAreaRot{ mainToArea.rotation() };

        float const f = math::mul_2pow<float, int>(1.0f, -rMainSpace.m_precision);

        for (std::size_t i = 0; i < rMainSpace.m_satCount; ++i)
        {
            Vector3g const relative = mainToArea.transform_position({ x[i], y[i], z[i] });
            Vector3 const relativeMeters = Vector3(relative);

            Quaterniond const rot{ {qx[i], qy[i], qz[i]}, qw[i] };

            DrawEnt const drawEnt = rPlanetDraw.drawEnts[i];

            float radius = radiusView[i];

            rScnRender.m_drawTransform[drawEnt] =
                Matrix4::translation(Vector3{ (float)x[i], (float)y[i], (float)z[i] })
                * Matrix4::scaling({ radius, radius, radius })
                * Matrix4 {
                (mainToAreaRot * Quaternion{ rot }).toMatrix()
            };
        }
    });
#endif
}); // ftrSolarSystemDraw

} // namespace adera
