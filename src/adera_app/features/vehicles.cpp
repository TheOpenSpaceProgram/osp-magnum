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
#include "vehicles.h"
#include "../feature_interfaces.h"

#include <adera/activescene/vehicles_vb_fn.h>
#include <adera/drawing/CameraController.h>
#include <adera/machines/links.h>

#include <osp/activescene/basic.h>
#include <osp/activescene/physics.h>
#include <osp/activescene/prefab_fn.h>
#include <osp/core/Resources.h>
#include <osp/drawing/drawing.h>
#include <osp/util/UserInputHandler.h>

using namespace adera;
using namespace ftr_inter::stages;
using namespace ftr_inter;
using namespace osp::active;
using namespace osp::draw;
using namespace osp::fw;
using namespace osp::link;
using namespace osp;

using osp::restypes::gc_importer;

using namespace Magnum::Math::Literals;

namespace adera
{


FeatureDef const ftrParts = feature_def("Parts", [] (
        FeatureBuilder              &rFB,
        Implement<FILinks>          links,
        Implement<FIParts>          parts,
        DependOn<FICleanupContext>  cleanup,
        DependOn<FIMainApp>         mainApp,
        DependOn<FIScene>           scn)
{
    rFB.loopblk(links.loopblks.link)        .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(links.pl.linkLoop)         .parent(links.loopblks.link);

    rFB.pipeline(links.pl.machIds)          .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(links.pl.nodeIds)          .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(links.pl.connect)          .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(links.pl.machUpdExtIn)     .parent(mainApp.loopblks.mainLoop);


    rFB.pipeline(parts.pl.partIds)          .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(parts.pl.partPrefabs)      .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(parts.pl.partTransformWeld).parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(parts.pl.partDirty)        .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(parts.pl.weldIds)          .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(parts.pl.weldDirty)        .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(parts.pl.mapWeldPart)      .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(parts.pl.mapPartMach)      .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(parts.pl.mapPartActive)    .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(parts.pl.mapWeldActive)    .parent(mainApp.loopblks.mainLoop);

    auto &rLinks    = rFB.data_emplace< ACtxLinks >      (links.di.links);
    auto &rUpdMach  = rFB.data_emplace< MachineUpdater > (links.di.updMach);

    auto &rScnParts = rFB.data_emplace< ACtxParts >      (parts.di.scnParts);
        //                                  machines;
        //         nodePerType;

    // Resize containers to fit all existing MachTypeIds and NodeTypeIds
    // These Global IDs are dynamically initialized just as the program starts
    rUpdMach.machTypesDirty.resize(MachTypeReg_t::size());
    rUpdMach.localDirty       .resize(MachTypeReg_t::size());
    rLinks.machines.perType.resize(MachTypeReg_t::size());
    rLinks.nodePerType     .resize(NodeTypeReg_t::size());

    rFB.task()
        .name       ("Clear Resource owners")
        .sync_with  ({cleanup.pl.cleanup(Run_)})
        .args       ({      parts.di.scnParts,  mainApp.di.resources})
        .func       ([] (ACtxParts& rScnParts, Resources& rResources) noexcept
    {
        for (osp::PrefabPair &rPrefabPair : rScnParts.partPrefabs)
        {
            rResources.owner_destroy(gc_importer, std::move(rPrefabPair.m_importer));
        }
    });

    rFB.task()
        .name       ("Clear Part dirty vectors after use")
        .sync_with  ({parts.pl.partDirty(Clear)})
        .args       ({      parts.di.scnParts})
        .func       ([] (ACtxParts& rScnParts) noexcept
    {
        rScnParts.partDirty.clear();
    });

    rFB.task()
        .name       ("Clear Weld dirty vectors after use")
        .sync_with  ({parts.pl.weldDirty(Clear)})
        .args       ({      parts.di.scnParts})
        .func       ([] (ACtxParts& rScnParts) noexcept
    {
        rScnParts.weldDirty.clear();
    });

    rFB.task()
        .name       ("Schedule Link update")
        .schedules  (links.pl.linkLoop)
        .sync_with  ({scn.pl.update(Run)})
        .args       ({           links.di.updMach})
        .func       ([] (MachineUpdater& rUpdMach) noexcept -> TaskActions
    {
        if (rUpdMach.requestMachineUpdateLoop)
        {
            rUpdMach.requestMachineUpdateLoop = false;
            return {};
        }
        else
        {
            return {.cancel = true};
        }
    });
}); // ftrParts




FeatureDef const ftrVehicleSpawn = feature_def("VehicleSpawn", [] (
        FeatureBuilder              &rFB,
        Implement<FIVehicleSpawn>   vhclSpawn,
        DependOn<FIMainApp>         mainApp,
        DependOn<FIScene>           scn)
{
    rFB.pipeline(vhclSpawn.pl.spawnRequest)  .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(vhclSpawn.pl.spawnedParts)  .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(vhclSpawn.pl.spawnedWelds)  .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(vhclSpawn.pl.rootEnts)      .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(vhclSpawn.pl.spawnedMachs)  .parent(mainApp.loopblks.mainLoop);

    rFB.data_emplace< ACtxVehicleSpawn >     (vhclSpawn.di.vehicleSpawn);

    rFB.task()
        .name       ("Schedule Vehicle spawn")
        .schedules  (vhclSpawn.pl.spawnRequest)
        .sync_with  ({scn.pl.update(Run)})
        .args       ({                   vhclSpawn.di.vehicleSpawn })
        .func([] (ACtxVehicleSpawn const& rVehicleSpawn) noexcept -> TaskActions
    {
        return {.cancel = rVehicleSpawn.spawnRequest.empty()};
    });



    rFB.task()
        .name       ("Clear Vehicle Spawning vector after use")
        .sync_with  ({vhclSpawn.pl.spawnRequest(Clear)})
        .args       ({             vhclSpawn.di.vehicleSpawn})
        .func([] (ACtxVehicleSpawn& rVehicleSpawn) noexcept
    {
        rVehicleSpawn.spawnRequest.clear();
    });
}); // ftrVehicleSpawn


FeatureDef const ftrVehicleSpawnVBData = feature_def("VehicleSpawnVBData", [] (
        FeatureBuilder              &rFB,
        Implement<FIVehicleSpawnVB> vhclSpawnVB,
        DependOn<FILinks>           links,
        DependOn<FIMainApp>         mainApp,
        DependOn<FIScene>           scn,
        DependOn<FICommonScene>     comScn,
        DependOn<FIPrefabs>         prefabs,
        DependOn<FIParts>           parts,
        DependOn<FIVehicleSpawn>    vhclSpawn,
        DependOn<FISignalsFloat>    sigFloat)
{
    rFB.pipeline(vhclSpawnVB.pl.dataVB)      .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(vhclSpawnVB.pl.remapParts)  .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(vhclSpawnVB.pl.remapWelds)  .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(vhclSpawnVB.pl.remapMachs)  .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(vhclSpawnVB.pl.remapNodes)  .parent(mainApp.loopblks.mainLoop);

    rFB.data_emplace< ACtxVehicleSpawnVB >(vhclSpawnVB.di.vehicleSpawnVB);

    rFB.task()
        .name       ("Create PartIds and WeldIds for vehicles to spawn from VehicleData")
        .sync_with  ({vhclSpawn.pl.spawnRequest(UseOrRun), vhclSpawn.pl.spawnedParts(Resize), vhclSpawnVB.pl.remapParts(Modify_), vhclSpawnVB.pl.remapWelds(Modify_), parts.pl.partIds(New), parts.pl.weldIds(New), parts.pl.mapWeldActive(New)})
        .args       ({             vhclSpawn.di.vehicleSpawn,                    vhclSpawnVB.di.vehicleSpawnVB,           parts.di.scnParts})
        .func([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB& rVehicleSpawnVB, ACtxParts& rScnParts) noexcept
    {
        SysVehicleSpawnVB::create_parts_and_welds(rVehicleSpawn, rVehicleSpawnVB, rScnParts);
    });

    rFB.task()
        .name       ("Request prefabs for vehicle parts from VehicleBuilder")
        .sync_with  ({vhclSpawn.pl.spawnRequest(UseOrRun), prefabs.pl.spawnRequest(Modify_), vhclSpawn.pl.spawnedParts(UseOrRun)})
        .args       ({             vhclSpawn.di.vehicleSpawn,                          vhclSpawnVB.di.vehicleSpawnVB,           parts.di.scnParts,             prefabs.di.prefabs,           mainApp.di.resources})
        .func([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB const& rVehicleSpawnVB, ACtxParts& rScnParts, ACtxPrefabs& rPrefabs, Resources& rResources) noexcept
    {
        SysVehicleSpawnVB::request_prefabs(rVehicleSpawn, rVehicleSpawnVB, rScnParts, rPrefabs, rResources);
    });

    rFB.task()
        .name       ("Create Machine IDs copied from VehicleData")
        .sync_with  ({vhclSpawn.pl.spawnRequest(UseOrRun), vhclSpawnVB.pl.dataVB(UseOrRun), vhclSpawnVB.pl.remapMachs(Modify_), vhclSpawn.pl.spawnedMachs(Resize), links.pl.machIds(New)})
        .args       ({         vhclSpawn.di.vehicleSpawn,       vhclSpawnVB.di.vehicleSpawnVB,    parts.di.scnParts,    links.di.links})
        .func       ([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB& rVehicleSpawnVB, ACtxParts& rScnParts, ACtxLinks &rLinks) noexcept
    {
        std::size_t const newVehicleCount = rVehicleSpawn.new_vehicle_count();
        ACtxVehicleSpawnVB &rVSVB = rVehicleSpawnVB;

        // Count total machines, and calculate offsets for remaps.

        std::size_t machTotal = 0;
        std::size_t remapMachTotal = 0;

        rVSVB.machtypeCount.clear();
        rVSVB.machtypeCount.resize(MachTypeReg_t::size(), 0);

        rVSVB.remapMachOffsets.resize(newVehicleCount);

        for (SpVehicleId vhId{0}; vhId.value < newVehicleCount; ++vhId.value)
        {
            VehicleData const* pVData = rVSVB.dataVB[vhId];
            if (pVData == nullptr)
            {
                continue;
            }

            Machines const &srcMachines = pVData->m_machines;
            std::size_t const bounds = srcMachines.ids.capacity();

            rVSVB.remapMachOffsets[vhId] = remapMachTotal;

            remapMachTotal += bounds;
            machTotal += srcMachines.ids.size();

            for (MachTypeId type = 0; type < MachTypeReg_t::largest(); ++type)
            {
                rVSVB.machtypeCount[type] += srcMachines.perType[type].localIds.size();
            }
        }

        rVehicleSpawn.spawnedMachs.resize(machTotal);
        rVSVB.remapMachs.resize(remapMachTotal);

        // Create ACtxParts MachAny/LocalIDs and populate remaps

        // MachAnyIDs created here
        rLinks.machines.ids.create(rVehicleSpawn.spawnedMachs.begin(),
                                   rVehicleSpawn.spawnedMachs.end());

        rLinks.machines.machToLocal.resize(rLinks.machines.ids.capacity());

        auto itDstMachIds = rVehicleSpawn.spawnedMachs.cbegin();

        for (SpVehicleId vhId{0}; vhId.value < newVehicleCount; ++vhId.value)
        {
            VehicleData const* pVData = rVSVB.dataVB[vhId];
            if (pVData == nullptr)
            {
                continue;
            }

            Machines const &srcMachines = pVData->m_machines;

            std::size_t const remapMachOffset = rVSVB.remapMachOffsets[vhId];

            for (MachAnyId const srcMach : srcMachines.ids)
            {
                MachAnyId const dstMach = *itDstMachIds;
                ++itDstMachIds;

                // Populate map for "VehicleBuilder MachAnyId -> ACtxParts MachAnyId"
                rVSVB.remapMachs[remapMachOffset + srcMach] = dstMach;

                // Create ACtxParts MachLocalIds
                // MachLocalIds don't need a remap, since they can be obtained
                // from a MachAnyId.
                // TODO: This can be optimized later, where all local IDs are
                //       created at once with ids.create(first, last), and make
                //       resize(..) called once per type too
                MachTypeId const    type            = srcMachines.machTypes[srcMach];
                PerMachType&        rDstPerType     = rLinks.machines.perType[type];

                MachLocalId const dstLocal = rDstPerType.localIds.create();
                rDstPerType.localToAny.resize(rDstPerType.localIds.capacity());

                rDstPerType.localToAny[dstLocal] = dstMach;
                rLinks.machines.machToLocal[dstMach] = dstLocal;
            }
        }
    });

    rFB.task()
        .name       ("Update Part<->Machine maps")
        .sync_with  ({vhclSpawn.pl.spawnRequest(UseOrRun), vhclSpawnVB.pl.dataVB(UseOrRun), vhclSpawnVB.pl.remapMachs(UseOrRun), vhclSpawnVB.pl.remapParts(UseOrRun), parts.pl.mapPartMach(New)})
        .args       ({         vhclSpawn.di.vehicleSpawn,             vhclSpawnVB.di.vehicleSpawnVB,   parts.di.scnParts,     links.di.links,    prefabs.di.prefabs,  mainApp.di.resources})
        .func       ([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB const& rVehicleSpawnVB, ACtxParts& rScnParts, ACtxLinks &rLinks, ACtxPrefabs& rPrefabs, Resources& rResources) noexcept
    {
        std::size_t const newVehicleCount = rVehicleSpawn.new_vehicle_count();
        ACtxVehicleSpawnVB const& rVSVB = rVehicleSpawnVB;

        for (SpVehicleId vhId{0}; vhId.value < newVehicleCount; ++vhId.value)
        {
            VehicleData const* pVData = rVSVB.dataVB[vhId];
            if (pVData == nullptr)
            {
                continue;
            }

            rScnParts.machineToPart.resize(rLinks.machines.ids.capacity());
            rScnParts.partToMachines.ids_reserve(rScnParts.partIds.capacity());
            rScnParts.partToMachines.data_reserve(rLinks.machines.ids.capacity());

            std::size_t const remapMachOffset = rVSVB.remapMachOffsets[vhId];
            std::size_t const remapPartOffset = rVSVB.remapPartOffsets[vhId];

            // Update rScnParts machine->part map
            for (MachAnyId const srcMach : pVData->m_machines.ids)
            {
                MachAnyId const dstMach = rVSVB.remapMachs[remapMachOffset + srcMach];
                PartId const    srcPart = pVData->m_machToPart[srcMach];
                PartId const    dstPart = rVSVB.remapParts[remapPartOffset + srcPart];

                rScnParts.machineToPart[dstMach] = dstPart;
            }

            // Update rScnParts part->machine multimap
            for (PartId const srcPart : pVData->m_partIds)
            {
                PartId const dstPart = rVSVB.remapParts[remapPartOffset + srcPart];

                auto const& srcPairs = pVData->m_partToMachines[srcPart];

                rScnParts.partToMachines.emplace(dstPart, srcPairs.size());
                auto dstPairs = rScnParts.partToMachines[dstPart];

                for (int i = 0; i < srcPairs.size(); ++i)
                {
                    MachinePair const&  srcPair  = srcPairs[i];
                    MachinePair&        rDstPair = dstPairs[i];
                    MachAnyId const     srcMach  = pVData->m_machines.perType[srcPair.type].localToAny[srcPair.local];
                    MachAnyId const     dstMach  = rVSVB.remapMachs[remapMachOffset + srcMach];
                    MachTypeId const    dstType  = srcPair.type;
                    MachLocalId const   dstLocal = rLinks.machines.machToLocal[dstMach];

                    rDstPair = { .local = dstLocal, .type = dstType };
                }
            }
        }
    });

    rFB.task()
        .name       ("Create (and connect) Node IDs copied from VehicleBuilder")
        .sync_with  ({vhclSpawn.pl.spawnRequest(UseOrRun), vhclSpawnVB.pl.dataVB(UseOrRun), vhclSpawnVB.pl.remapMachs(UseOrRun), vhclSpawnVB.pl.remapNodes(Modify_),
                      links.pl.nodeIds(New), links.pl.connect(New)})
        .args       ({         vhclSpawn.di.vehicleSpawn,       vhclSpawnVB.di.vehicleSpawnVB,    parts.di.scnParts,    links.di.links})
        .func       ([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB& rVehicleSpawnVB, ACtxParts& rScnParts, ACtxLinks &rLinks) noexcept
    {
        std::size_t const newVehicleCount = rVehicleSpawn.new_vehicle_count();
        ACtxVehicleSpawnVB &rVSVB = rVehicleSpawnVB;

        rVSVB.remapNodeOffsets.resize(newVehicleCount * NodeTypeReg_t::size());
        auto remapNodeOffsets2d = rVSVB.remap_node_offsets_2d();

        // Add up bounds needed for all nodes of every type for remaps
        std::size_t remapNodeTotal = 0;
        for (VehicleData const* pVData : rVSVB.dataVB)
        {
            if (pVData == nullptr)
            {
                continue;
            }
            for (PerNodeType const &rSrcNodeType : pVData->m_nodePerType)
            {
                remapNodeTotal += rSrcNodeType.nodeIds.capacity();
            }
        }
        rVSVB.remapNodes.resize(remapNodeTotal);

        std::size_t nodeRemapUsed = 0;

        for (SpVehicleId vhId{0}; vhId.value < newVehicleCount; ++vhId.value)
        {
            VehicleData const* pVData = rVSVB.dataVB[vhId];
            if (pVData == nullptr)
            {
                continue;
            }

            auto machRemap = arrayView(std::as_const(rVSVB.remapMachs)).exceptPrefix(rVSVB.remapMachOffsets[vhId]);

            for (NodeTypeId nodeType = 0; nodeType < NodeTypeReg_t::largest(); ++nodeType)
            {
                PerNodeType const &rSrcNodeType = pVData->m_nodePerType[nodeType];

                std::size_t const remapSize = rSrcNodeType.nodeIds.capacity();
                auto nodeRemapOut = arrayView(rVSVB.remapNodes).sliceSize(nodeRemapUsed, remapSize);
                remapNodeOffsets2d[vhId.value][nodeType] = nodeRemapUsed;
                nodeRemapUsed += remapSize;
                copy_nodes(rSrcNodeType, pVData->m_machines, machRemap,
                           rLinks.nodePerType[nodeType], rLinks.machines, nodeRemapOut);
            }
        }
    });

    rFB.task()
        .name       ("Update PartId<->ActiveEnt mapping")
        .sync_with  ({vhclSpawn.pl.spawnRequest(UseOrRun), vhclSpawn.pl.spawnedParts(UseOrRun), prefabs.pl.spawnedEnts(UseOrRun), parts.pl.mapPartActive(Modify)})
        .args       ({         vhclSpawn.di.vehicleSpawn,         comScn.di.basic,    parts.di.scnParts,     prefabs.di.prefabs})
        .func       ([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxBasic const& rBasic, ACtxParts& rScnParts,  ACtxPrefabs& rPrefabs) noexcept
    {
        rScnParts.partToActive.resize(rScnParts.partIds.capacity());
        rScnParts.activeToPart.resize(rBasic.m_activeIds.capacity());

        // Populate PartId<->ActiveEnt mapping, now that the prefabs exist

        auto itPrefab = rVehicleSpawn.spawnedPrefabs.begin();

        for (PartId const partId : rVehicleSpawn.spawnedParts)
        {
            ActiveEnt const root = rPrefabs.spawnedEntsOffset[*itPrefab].front();
            ++itPrefab;

            rScnParts.partToActive[partId]    = root;
            rScnParts.activeToPart[root]      = partId;
        }
    });

    rFB.task()
        .name       ("Copy float signal values from VehicleBuilder")
        .sync_with  ({vhclSpawn.pl.spawnRequest(UseOrRun), vhclSpawn.pl.spawnedParts(UseOrRun), vhclSpawnVB.pl.remapNodes(UseOrRun), sigFloat.pl.sigFloatValues(New), sigFloat.pl.sigFloatUpdExtIn(New)})
        .args       ({         vhclSpawn.di.vehicleSpawn,             vhclSpawnVB.di.vehicleSpawnVB,    parts.di.scnParts,    links.di.links,             sigFloat.di.sigValFloat,          sigFloat.di.sigUpdFloat})
        .func       ([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB const& rVehicleSpawnVB, ACtxParts& rScnParts, ACtxLinks &rLinks, SignalValues_t<float>& rSigValFloat, UpdateNodes<float>& rSigUpdFloat) noexcept
    {
        Nodes const         &rFloatNodes    = rLinks.nodePerType[gc_ntSigFloat];
        std::size_t const   maxNodes        = rFloatNodes.nodeIds.capacity();
        rSigUpdFloat.nodeNewValues.resize(maxNodes);
        rSigUpdFloat.nodeDirty.resize(maxNodes);
        rSigValFloat.resize(maxNodes);

        std::size_t const           newVehicleCount     = rVehicleSpawn.new_vehicle_count();
        ACtxVehicleSpawnVB const    &rVSVB              = rVehicleSpawnVB;

        auto const remapNodeOffsets2d = rVSVB.remap_node_offsets_2d();
        for (SpVehicleId vhId{0}; vhId.value < newVehicleCount; ++vhId.value)
        {
            VehicleData const* pVData = rVSVB.dataVB[vhId];
            if (pVData == nullptr)
            {
                continue;
            }

            PerNodeType const&  srcFloatNodes       = pVData->m_nodePerType[gc_ntSigFloat];
            entt::any const&    srcFloatValuesAny   = srcFloatNodes.m_nodeValues;
            auto const&         srcFloatValues      = entt::any_cast< SignalValues_t<float> >(srcFloatValuesAny);
            std::size_t const   nodeRemapOffset     = remapNodeOffsets2d[vhId.value][gc_ntSigFloat];
            auto const          nodeRemap           = arrayView(rVSVB.remapNodes).exceptPrefix(nodeRemapOffset);

            for (NodeId const srcNode : srcFloatNodes.nodeIds)
            {
                NodeId const dstNode = nodeRemap[srcNode];
                rSigValFloat[dstNode] = srcFloatValues[srcNode];
            }
        }
    });
}); // ftrVehicleSpawnVBData


FeatureDef const ftrVehicleSpawnDraw = feature_def("VehicleSpawnDraw", [] (
        FeatureBuilder              &rFB,
        DependOn<FISceneRenderer>   scnRender,
        DependOn<FIVehicleSpawn>    vhclSpawn)
{
    rFB.task()
        .name       ("Enable Draw Transforms for spawned vehicle root entities")
        .sync_with  ({vhclSpawn.pl.spawnRequest(UseOrRun), vhclSpawn.pl.rootEnts(UseOrRun), scnRender.pl.drawEnt(Ready)})
        .args       ({            scnRender.di.scnRender,                        vhclSpawn.di.vehicleSpawn})
        .func([] (ACtxSceneRender& rScnRender, ACtxVehicleSpawn const& rVehicleSpawn) noexcept
    {
        for (ActiveEnt const ent : rVehicleSpawn.rootEnts)
        {
            rScnRender.m_needDrawTf.insert(ent);
        }
    });

}); // ftrVehicleSpawnDraw


FeatureDef const ftrSignalsFloat = feature_def("SignalsFloat", [] (
        FeatureBuilder              &rFB,
        Implement<FISignalsFloat>   sigFloat,
        DependOn<FIMainApp>         mainApp,
        DependOn<FIScene>           scn,
        DependOn<FILinks>           links)
{

    rFB.pipeline(sigFloat.pl.sigFloatValues)   .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(sigFloat.pl.sigFloatUpdExtIn) .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(sigFloat.pl.sigFloatUpdLoop)  .parent(links.loopblks.link);


    rFB.data_emplace< SignalValues_t<float> >    (sigFloat.di.sigValFloat);
    rFB.data_emplace< UpdateNodes<float> >       (sigFloat.di.sigUpdFloat);

    // NOTE: Consider supporting per-thread UpdateNodes<float> to allow multiple threads to write
    //       new float values in parallel.

    rFB.task()
        .name       ("Update Signal<float> Nodes")
        .sync_with  ({links.pl.linkLoop(EStgLink::NodeUpd), sigFloat.pl.sigFloatUpdExtIn(Ready), links.pl.machUpdExtIn(Ready), sigFloat.pl.sigFloatUpdLoop(Modify), sigFloat.pl.sigFloatValues(Modify)})
        .args       ({            sigFloat.di.sigUpdFloat,             sigFloat.di.sigValFloat,         links.di.updMach,          links.di.links})
        .func       ([] (UpdateNodes<float>& rSigUpdFloat, SignalValues_t<float>& rSigValFloat, MachineUpdater& rUpdMach, ACtxLinks const& rLinks) noexcept
    {
        if ( ! rSigUpdFloat.dirty )
        {
            return; // Not dirty, nothing to do
        }

        Nodes const &rFloatNodes = rLinks.nodePerType[gc_ntSigFloat];

        // NOTE: The various use of reset() clear entire bit arrays, which may or may
        //       not be expensive. They likely optimize to memset

        for (MachTypeId const machTypeDirty : rUpdMach.machTypesDirty)
        {
            rUpdMach.localDirty[machTypeDirty].clear();
        }
        rUpdMach.machTypesDirty.clear();

        // Sees which nodes changed, and writes into rUpdMach set dirty which MACHINES
        // must be updated next
        update_signal_nodes<float>(
                rSigUpdFloat.nodeDirty,
                rFloatNodes.nodeToMach,
                rLinks.machines,
                arrayView(rSigUpdFloat.nodeNewValues),
                rSigValFloat,
                rUpdMach);
        rSigUpdFloat.nodeDirty.clear();
        rSigUpdFloat.dirty = false;

        // Run tasks needed to update machine types that are dirty
        bool anyMachineNotified = false;
        for (MachTypeId const type : rUpdMach.machTypesDirty)
        {
            anyMachineNotified = true;
        }
        if (anyMachineNotified && rUpdMach.requestMachineUpdateLoop.load())
        {
            rUpdMach.requestMachineUpdateLoop.store(true);
        }
    });
}); // ftrSignalsFloat


} // namespace adera
