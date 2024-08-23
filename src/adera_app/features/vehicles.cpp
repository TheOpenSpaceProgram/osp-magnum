#if 0
/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
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

using namespace osp::active;
using namespace osp::draw;
using namespace osp::link;
using namespace osp;

using osp::restypes::gc_importer;

using namespace Magnum::Math::Literals;

namespace adera
{

Session setup_parts(
        TopTaskBuilder&             rFB,
        ArrayView<entt::any> const  topData,
        Session const&              application,
        Session const&              scene)
{
    OSP_DECLARE_GET_DATA_IDS(application,   TESTAPP_DATA_APPLICATION);

    auto const scn.pl = scene.get_pipelines<PlScene>();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, TESTAPP_DATA_PARTS);
    auto const parts.pl = out.create_pipelines<PlParts>(rFB);

    out.m_cleanup = scn.pl.cleanup;

    rFB.pipeline(parts.pl.partIds)          .parent(scn.pl.update);
    rFB.pipeline(parts.pl.partPrefabs)      .parent(scn.pl.update);
    rFB.pipeline(parts.pl.partTransformWeld).parent(scn.pl.update);
    rFB.pipeline(parts.pl.partDirty)        .parent(scn.pl.update);
    rFB.pipeline(parts.pl.weldIds)          .parent(scn.pl.update);
    rFB.pipeline(parts.pl.weldDirty)        .parent(scn.pl.update);
    rFB.pipeline(parts.pl.machIds)          .parent(scn.pl.update);
    rFB.pipeline(parts.pl.nodeIds)          .parent(scn.pl.update);
    rFB.pipeline(parts.pl.connect)          .parent(scn.pl.update);
    rFB.pipeline(parts.pl.mapWeldPart)      .parent(scn.pl.update);
    rFB.pipeline(parts.pl.mapPartMach)      .parent(scn.pl.update);
    rFB.pipeline(parts.pl.mapPartActive)    .parent(scn.pl.update);
    rFB.pipeline(parts.pl.mapWeldActive)    .parent(scn.pl.update);
    rFB.pipeline(parts.pl.machUpdExtIn)     .parent(scn.pl.update);
    rFB.pipeline(parts.pl.linkLoop)         .parent(scn.pl.update).loops(true);

    auto &rScnParts = rFB.data_emplace< ACtxParts >      (parts.di.scnParts);
    auto &rUpdMach  = rFB.data_emplace< MachineUpdater > (idUpdMach);

    // Resize containers to fit all existing MachTypeIds and NodeTypeIds
    // These Global IDs are dynamically initialized just as the program starts
    rUpdMach.machTypesDirty.resize(MachTypeReg_t::size());
    rUpdMach.localDirty       .resize(MachTypeReg_t::size());
    rScnParts.machines.perType.resize(MachTypeReg_t::size());
    rScnParts.nodePerType     .resize(NodeTypeReg_t::size());

    rFB.task()
        .name       ("Clear Resource owners")
        .run_on     ({scn.pl.cleanup(Run_)})
        .push_to    (out.m_tasks)
        .args       ({      parts.di.scnParts,           mainApp.di.resources})
        .func([] (ACtxParts& rScnParts, Resources& rResources) noexcept
    {
        for (osp::PrefabPair &rPrefabPair : rScnParts.partPrefabs)
        {
            rResources.owner_destroy(gc_importer, std::move(rPrefabPair.m_importer));
        }
    });

    rFB.task()
        .name       ("Clear Part dirty vectors after use")
        .run_on     ({parts.pl.partDirty(Clear)})
        .push_to    (out.m_tasks)
        .args       ({      parts.di.scnParts})
        .func([] (ACtxParts& rScnParts) noexcept
    {
        rScnParts.partDirty.clear();
    });

    rFB.task()
        .name       ("Clear Weld dirty vectors after use")
        .run_on     ({parts.pl.weldDirty(Clear)})
        .push_to    (out.m_tasks)
        .args       ({      parts.di.scnParts})
        .func([] (ACtxParts& rScnParts) noexcept
    {
        rScnParts.weldDirty.clear();
    });

    rFB.task()
        .name       ("Schedule Link update")
        .schedules  ({parts.pl.linkLoop(ScheduleLink)})
        .sync_with  ({scn.pl.update(Run)})
        .push_to    (out.m_tasks)
        .args       ({           idUpdMach})
        .func([] (MachineUpdater& rUpdMach) noexcept -> TaskActions
    {
        if (rUpdMach.requestMachineUpdateLoop)
        {
            rUpdMach.requestMachineUpdateLoop = false;
            return TaskActions{};
        }
        else
        {
            return TaskAction::Cancel;
        }
    });

    return out;
} // setup_parts

Session setup_vehicle_spawn(
        TopTaskBuilder&             rFB,
        ArrayView<entt::any> const  topData,
        Session const&              scene)
{
    auto const scn.pl = scene.get_pipelines<PlScene>();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, TESTAPP_DATA_VEHICLE_SPAWN);
    auto const vhclSpawn.pl = out.create_pipelines<PlVehicleSpawn>(rFB);

    rFB.pipeline(vhclSpawn.pl.spawnRequest)  .parent(scn.pl.update);
    rFB.pipeline(vhclSpawn.pl.spawnedParts)  .parent(scn.pl.update);
    rFB.pipeline(vhclSpawn.pl.spawnedWelds)  .parent(scn.pl.update);
    rFB.pipeline(vhclSpawn.pl.rootEnts)      .parent(scn.pl.update);
    rFB.pipeline(vhclSpawn.pl.spawnedMachs)     .parent(scn.pl.update);

    rFB.data_emplace< ACtxVehicleSpawn >     (vhclSpawn.di.vehicleSpawn);

    rFB.task()
        .name       ("Schedule Vehicle spawn")
        .schedules  ({vhclSpawn.pl.spawnRequest(Schedule_)})
        .sync_with  ({scn.pl.update(Run)})
        .push_to    (out.m_tasks)
        .args       ({                   vhclSpawn.di.vehicleSpawn })
        .func([] (ACtxVehicleSpawn const& rVehicleSpawn) noexcept -> TaskActions
    {
        return rVehicleSpawn.spawnRequest.empty() ? TaskAction::Cancel : TaskActions{};
    });

    rFB.task()
        .name       ("Clear Vehicle Spawning vector after use")
        .run_on     ({vhclSpawn.pl.spawnRequest(Clear)})
        .push_to    (out.m_tasks)
        .args       ({             vhclSpawn.di.vehicleSpawn})
        .func([] (ACtxVehicleSpawn& rVehicleSpawn) noexcept
    {
        rVehicleSpawn.spawnRequest.clear();
    });

    return out;
} // setup_vehicle_spawn

Session setup_vehicle_spawn_vb(
        TopTaskBuilder&             rFB,
        ArrayView<entt::any> const  topData,
        Session const&              application,
        Session const&              scene,
        Session const&              commonScene,
        Session const&              prefabs,
        Session const&              parts,
        Session const&              vehicleSpawn,
        Session const&              signalsFloat)
{
    OSP_DECLARE_GET_DATA_IDS(application,   TESTAPP_DATA_APPLICATION);
    OSP_DECLARE_GET_DATA_IDS(commonScene,   TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(parts,         TESTAPP_DATA_PARTS);
    OSP_DECLARE_GET_DATA_IDS(prefabs,       TESTAPP_DATA_PREFABS);
    OSP_DECLARE_GET_DATA_IDS(signalsFloat,  TESTAPP_DATA_SIGNALS_FLOAT);
    OSP_DECLARE_GET_DATA_IDS(vehicleSpawn,  TESTAPP_DATA_VEHICLE_SPAWN);
    auto const prefabs.pl     = prefabs       .get_pipelines<PlPrefabs>();
    auto const scn.pl    = scene         .get_pipelines<PlScene>();
    auto const parts.pl  = parts         .get_pipelines<PlParts>();
    auto const tgSgFlt  = signalsFloat  .get_pipelines<PlSignalsFloat>();
    auto const vhclSpawn.pl   = vehicleSpawn  .get_pipelines<PlVehicleSpawn>();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, TESTAPP_DATA_VEHICLE_SPAWN_VB);
    auto const vhclSpawn.plVB = out.create_pipelines<PlVehicleSpawnVB>(rFB);

    rFB.pipeline(vhclSpawn.plVB.dataVB)      .parent(scn.pl.update);
    rFB.pipeline(vhclSpawn.plVB.remapParts)  .parent(scn.pl.update);
    rFB.pipeline(vhclSpawn.plVB.remapWelds)  .parent(scn.pl.update);
    rFB.pipeline(vhclSpawn.plVB.remapMachs)  .parent(scn.pl.update);
    rFB.pipeline(vhclSpawn.plVB.remapNodes)  .parent(scn.pl.update);

    rFB.data_emplace< ACtxVehicleSpawnVB >(vhclSpawn.di.vehicleSpawnVB);

    rFB.task()
        .name       ("Create PartIds and WeldIds for vehicles to spawn from VehicleData")
        .run_on     ({vhclSpawn.pl.spawnRequest(UseOrRun)})
        .sync_with  ({vhclSpawn.pl.spawnedParts(Resize), vhclSpawn.plVB.remapParts(Modify_), vhclSpawn.plVB.remapWelds(Modify_), parts.pl.partIds(New), parts.pl.weldIds(New), parts.pl.mapWeldActive(New)})
        .push_to    (out.m_tasks)
        .args       ({             vhclSpawn.di.vehicleSpawn,                    vhclSpawn.di.vehicleSpawnVB,           parts.di.scnParts})
        .func([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB& rVehicleSpawnVB, ACtxParts& rScnParts) noexcept
    {
        SysVehicleSpawnVB::create_parts_and_welds(rVehicleSpawn, rVehicleSpawnVB, rScnParts);
    });

    rFB.task()
        .name       ("Request prefabs for vehicle parts from VehicleBuilder")
        .run_on     ({vhclSpawn.pl.spawnRequest(UseOrRun)})
        .sync_with  ({prefabs.pl.spawnRequest(Modify_), vhclSpawn.pl.spawnedParts(UseOrRun)})
        .push_to    (out.m_tasks)
        .args       ({             vhclSpawn.di.vehicleSpawn,                          vhclSpawn.di.vehicleSpawnVB,           parts.di.scnParts,             prefabs.di.prefabs,           mainApp.di.resources})
        .func([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB const& rVehicleSpawnVB, ACtxParts& rScnParts, ACtxPrefabs& rPrefabs, Resources& rResources) noexcept
    {
        SysVehicleSpawnVB::request_prefabs(rVehicleSpawn, rVehicleSpawnVB, rScnParts, rPrefabs, rResources);
    });

    rFB.task()
        .name       ("Create Machine IDs copied from VehicleData")
        .run_on     ({vhclSpawn.pl.spawnRequest(UseOrRun)})
        .sync_with  ({vhclSpawn.plVB.dataVB(UseOrRun), vhclSpawn.plVB.remapMachs(Modify_), vhclSpawn.pl.spawnedMachs(Resize), parts.pl.machIds(New)})
        .push_to    (out.m_tasks)
        .args       ({             vhclSpawn.di.vehicleSpawn,                    vhclSpawn.di.vehicleSpawnVB,           parts.di.scnParts})
        .func([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB& rVehicleSpawnVB, ACtxParts& rScnParts) noexcept
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
        rScnParts.machines.ids.create(rVehicleSpawn.spawnedMachs.begin(),
                                      rVehicleSpawn.spawnedMachs.end());

        rScnParts.machines.machToLocal.resize(rScnParts.machines.ids.capacity());

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
                PerMachType&        rDstPerType     = rScnParts.machines.perType[type];

                MachLocalId const dstLocal = rDstPerType.localIds.create();
                rDstPerType.localToAny.resize(rDstPerType.localIds.capacity());

                rDstPerType.localToAny[dstLocal] = dstMach;
                rScnParts.machines.machToLocal[dstMach] = dstLocal;
            }
        }
    });

    rFB.task()
        .name       ("Update Part<->Machine maps")
        .run_on     ({vhclSpawn.pl.spawnRequest(UseOrRun)})
        .sync_with  ({vhclSpawn.plVB.dataVB(UseOrRun), vhclSpawn.plVB.remapMachs(UseOrRun), vhclSpawn.plVB.remapParts(UseOrRun), parts.pl.mapPartMach(New)})
        .push_to    (out.m_tasks)
        .args       ({             vhclSpawn.di.vehicleSpawn,                          vhclSpawn.di.vehicleSpawnVB,           parts.di.scnParts,             prefabs.di.prefabs,           mainApp.di.resources})
        .func([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB const& rVehicleSpawnVB, ACtxParts& rScnParts, ACtxPrefabs& rPrefabs, Resources& rResources) noexcept
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

            rScnParts.machineToPart.resize(rScnParts.machines.ids.capacity());
            rScnParts.partToMachines.ids_reserve(rScnParts.partIds.capacity());
            rScnParts.partToMachines.data_reserve(rScnParts.machines.ids.capacity());

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
                    MachLocalId const   dstLocal = rScnParts.machines.machToLocal[dstMach];

                    rDstPair = { .local = dstLocal, .type = dstType };
                }
            }
        }
    });

    rFB.task()
        .name       ("Create (and connect) Node IDs copied from VehicleBuilder")
        .run_on     ({vhclSpawn.pl.spawnRequest(UseOrRun)})
        .sync_with  ({vhclSpawn.plVB.dataVB(UseOrRun), vhclSpawn.plVB.remapMachs(UseOrRun), vhclSpawn.plVB.remapNodes(Modify_), parts.pl.nodeIds(New), parts.pl.connect(New)})
        .push_to    (out.m_tasks)
        .args       ({             vhclSpawn.di.vehicleSpawn,                    vhclSpawn.di.vehicleSpawnVB,           parts.di.scnParts})
        .func([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB& rVehicleSpawnVB, ACtxParts& rScnParts) noexcept
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
                           rScnParts.nodePerType[nodeType], rScnParts.machines, nodeRemapOut);
            }
        }
    });

    rFB.task()
        .name       ("Update PartId<->ActiveEnt mapping")
        .run_on     ({vhclSpawn.pl.spawnRequest(UseOrRun)})
        .sync_with  ({vhclSpawn.pl.spawnedParts(UseOrRun), prefabs.pl.spawnedEnts(UseOrRun), parts.pl.mapPartActive(Modify)})
        .push_to    (out.m_tasks)
        .args       ({             vhclSpawn.di.vehicleSpawn,                 comScn.di.basic,           parts.di.scnParts,              prefabs.di.prefabs})
        .func([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxBasic const& rBasic, ACtxParts& rScnParts,  ACtxPrefabs& rPrefabs) noexcept
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
        .run_on     ({vhclSpawn.pl.spawnRequest(UseOrRun)})
        .sync_with  ({vhclSpawn.pl.spawnedParts(UseOrRun), vhclSpawn.plVB.remapNodes(UseOrRun), tgSgFlt.sigFloatValues(New), tgSgFlt.sigFloatUpdExtIn(New)})
        .push_to    (out.m_tasks)
        .args       ({             vhclSpawn.di.vehicleSpawn,                          vhclSpawn.di.vehicleSpawnVB,           parts.di.scnParts,                       sigFloat.di.sigValFloat,                    idSigUpdFloat})
        .func([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB const& rVehicleSpawnVB, ACtxParts& rScnParts, SignalValues_t<float>& rSigValFloat, UpdateNodes<float>& rSigUpdFloat) noexcept
    {
        Nodes const         &rFloatNodes    = rScnParts.nodePerType[gc_ntSigFloat];
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

    return out;
} // setup_vehicle_spawn_vb

Session setup_vehicle_spawn_draw(
        TopTaskBuilder&             rFB,
        ArrayView<entt::any> const  topData,
        Session const&              sceneRenderer,
        Session const&              vehicleSpawn)
{
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer, TESTAPP_DATA_SCENE_RENDERER);
    OSP_DECLARE_GET_DATA_IDS(vehicleSpawn,  TESTAPP_DATA_VEHICLE_SPAWN);
    auto const scnRender.pl = sceneRenderer .get_pipelines< PlSceneRenderer >();
    auto const vhclSpawn.pl   = vehicleSpawn  .get_pipelines< PlVehicleSpawn >();

    Session out;

    rFB.task()
        .name       ("Enable Draw Transforms for spawned vehicle root entities")
        .run_on     ({vhclSpawn.pl.spawnRequest(UseOrRun)})
        .sync_with  ({vhclSpawn.pl.rootEnts(UseOrRun), scnRender.pl.drawEntResized(Done)})
        .push_to    (out.m_tasks)
        .args       ({            scnRender.di.scnRender,                        vhclSpawn.di.vehicleSpawn})
        .func([] (ACtxSceneRender& rScnRender, ACtxVehicleSpawn const& rVehicleSpawn) noexcept
    {
        for (ActiveEnt const ent : rVehicleSpawn.rootEnts)
        {
            rScnRender.m_needDrawTf.insert(ent);
        }
    });

    return out;
} // setup_vehicle_spawn_draw


Session setup_signals_float(
        TopTaskBuilder&             rFB,
        ArrayView<entt::any> const  topData,
        Session const&              scene,
        Session const&              parts)
{
    OSP_DECLARE_GET_DATA_IDS(parts, TESTAPP_DATA_PARTS);
    auto const scn.pl = scene.get_pipelines<PlScene>();
    auto const parts.pl = parts.get_pipelines<PlParts>();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, TESTAPP_DATA_SIGNALS_FLOAT);
    auto const tgSgFlt = out.create_pipelines<PlSignalsFloat>(rFB);

    rFB.pipeline(tgSgFlt.sigFloatValues)   .parent(scn.pl.update);
    rFB.pipeline(tgSgFlt.sigFloatUpdExtIn) .parent(scn.pl.update);
    rFB.pipeline(tgSgFlt.sigFloatUpdLoop)  .parent(parts.pl.linkLoop);

    rFB.data_emplace< SignalValues_t<float> >    (sigFloat.di.sigValFloat);
    rFB.data_emplace< UpdateNodes<float> >       (idSigUpdFloat);

    // NOTE: Consider supporting per-thread UpdateNodes<float> to allow multiple threads to write
    //       new float values in parallel.

    rFB.task()
        .name       ("Update Signal<float> Nodes")
        .run_on     ({parts.pl.linkLoop(EStgLink::NodeUpd)})
        .sync_with  ({tgSgFlt.sigFloatUpdExtIn(Ready), parts.pl.machUpdExtIn(Ready), tgSgFlt.sigFloatUpdLoop(Modify), tgSgFlt.sigFloatValues(Modify)})
        .push_to    (out.m_tasks)
        .args       ({               idSigUpdFloat,                       sigFloat.di.sigValFloat,                idUpdMach,                 parts.di.scnParts})
        .func([] (UpdateNodes<float>& rSigUpdFloat, SignalValues_t<float>& rSigValFloat, MachineUpdater& rUpdMach, ACtxParts const& rScnParts) noexcept
    {
        if ( ! rSigUpdFloat.dirty )
        {
            return; // Not dirty, nothing to do
        }

        Nodes const &rFloatNodes = rScnParts.nodePerType[gc_ntSigFloat];

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
                rScnParts.machines,
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

    return out;
} // setup_signals_float


} // namespace adera
#endif
