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
#include "scene_vehicles.h"
#include "scenarios.h"
#include "identifiers.h"
#include "CameraController.h"
#include "../VehicleBuilder.h"

#include <adera/machines/links.h>

#include <osp/Active/parts.h>
#include <osp/Active/physics.h>
#include <osp/Active/SysSceneGraph.h>
#include <osp/Active/SysPrefabInit.h>
#include <osp/link/machines.h>
#include <osp/link/signal.h>
#include <osp/Resource/resources.h>
#include <osp/UserInputHandler.h>
#include <osp/logging.h>

using namespace osp;
using namespace osp::active;
using namespace osp::link;

using osp::restypes::gc_importer;

using namespace Magnum::Math::Literals;

namespace testapp::scenes
{


Session setup_parts(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              scnCommon,
        TopDataId const             idResources)
{
    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);

    Session parts;
    OSP_SESSION_ACQUIRE_DATA(parts, topData, TESTAPP_PARTS);
    OSP_SESSION_ACQUIRE_TAGS(parts, rTags,   TESTAPP_PARTS);
    parts.m_tgCleanupEvt = tgCleanupEvt;

    rBuilder.tag(tgPartReq)             .depend_on({tgPartMod});
    rBuilder.tag(tgPartClr)             .depend_on({tgPartMod, tgPartReq});
    rBuilder.tag(tgMapPartEntMod)       .depend_on({tgMapPartEntReq});
    rBuilder.tag(tgWeldReq)             .depend_on({tgWeldMod});
    rBuilder.tag(tgWeldClr)             .depend_on({tgWeldMod, tgWeldReq});
    rBuilder.tag(tgLinkReq)             .depend_on({tgLinkMod});
    rBuilder.tag(tgLinkMhUpdReq)        .depend_on({tgLinkMhUpdMod});
    rBuilder.tag(tgNodeAnyUpdReq)       .depend_on({tgNodeAnyUpdMod});
    rBuilder.tag(tgMachUpdEnqReq)       .depend_on({tgMachUpdEnqMod});

    auto &rScnParts = top_emplace< ACtxParts >      (topData, idScnParts);
    auto &rUpdMach  = top_emplace< UpdMachPerType > (topData, idUpdMach);
    top_emplace< TagId >                            (topData, idtgNodeUpdEvt, tgNodeUpdEvt);
    top_emplace< MachTypeToEvt_t >                  (topData, idMachEvtTags, MachTypeReg_t::size());
    top_emplace< std::vector<TagId> >               (topData, idMachUpdEnqueue);

    // Resize containers to fit all existing MachTypeIds and NodeTypeIds
    // These Global IDs are dynamically initialized just as the program starts
    bitvector_resize(rUpdMach.m_machTypesDirty, MachTypeReg_t::size());
    rUpdMach.m_localDirty           .resize(MachTypeReg_t::size());
    rScnParts.m_machines.m_perType  .resize(MachTypeReg_t::size());
    rScnParts.m_nodePerType         .resize(NodeTypeReg_t::size());

    auto const idNull = lgrn::id_null<TopDataId>();

    parts.task() = rBuilder.task().assign({tgCleanupEvt}).data(
            "Clean up Part prefab owners",
            TopDataIds_t{           idScnParts,           idResources },
            wrap_args([] (ACtxParts& rScnParts, Resources& rResources) noexcept
    {
        for (osp::PrefabPair &rPrefabPair : rScnParts.m_partPrefabs)
        {
            rResources.owner_destroy(gc_importer, std::move(rPrefabPair.m_importer));
        }
    }));

    parts.task() = rBuilder.task().assign({tgSceneEvt, tgPartClr, tgWeldClr}).data(
            "Clear Part and Weld dirty vectors after use",
            TopDataIds_t{           idScnParts},
            wrap_args([] (ACtxParts& rScnParts) noexcept
    {
        rScnParts.m_partDirty.clear();
        rScnParts.m_weldDirty.clear();
    }));

    parts.task() = rBuilder.task().assign({tgSceneEvt, tgNodeUpdEvt, tgMachUpdEnqReq}).data(
            "Enqueue Machine & Node update tasks",
            TopDataIds_t{             idNull,            idMachUpdEnqueue,           idtgNodeUpdEvt },
            wrap_args([] (WorkerContext ctx, std::vector<TagId>& rMachUpdEnqueue, TagId const tgNodeUpdEvt ) noexcept
    {
        if (rMachUpdEnqueue.empty())
        {
            return; // Nothing to enqueue
        }

        ctx.m_rEnqueueHappened = true;


        auto enqueueBits = lgrn::bit_view(ctx.m_enqueue);

        // Enqueue machine tags, eg: tgMhRcsDriverEvt, tgMhRocketEvt, ...
        for (TagId const tag : rMachUpdEnqueue)
        {
            enqueueBits.set(std::size_t(tag));
        }
        rMachUpdEnqueue.clear();

        // Enqueue self and all other machine update
        enqueueBits.set(std::size_t(tgNodeUpdEvt));
    }));

    return parts;
}

Session setup_signals_float(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              scnCommon,
        Session const&              parts)
{
    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(parts,      TESTAPP_PARTS);
    OSP_SESSION_UNPACK_TAGS(parts,      TESTAPP_PARTS);

    Session signalsFloat;
    OSP_SESSION_ACQUIRE_DATA(signalsFloat, topData, TESTAPP_SIGNALS_FLOAT);
    OSP_SESSION_ACQUIRE_TAGS(signalsFloat, rTags,   TESTAPP_SIGNALS_FLOAT);

    rBuilder.tag(tgSigFloatLinkReq)         .depend_on({tgSigFloatLinkMod});
    rBuilder.tag(tgSigFloatUpdReq)          .depend_on({tgSigFloatLinkMod, tgSigFloatUpdMod});

    top_emplace< SignalValues_t<float> >    (topData, idSigValFloat);
    top_emplace< UpdateNodes<float> >       (topData, idSigUpdFloat);

    // NOTE: Eventually have an array of UpdateNodes to allow multiple threads to update nodes in
    //       parallel, noting the use of "Reduce". Tag limits are intended select which UpdateNodes
    //       are passed to each thread, once they're properly implemented.

    auto const idNull = lgrn::id_null<TopDataId>();


    signalsFloat.task() = rBuilder.task().assign({tgSceneEvt, tgNodeUpdEvt, tgSigFloatUpdEvt, tgSigFloatUpdReq, tgMachUpdEnqMod}).data(
            "Reduce Signal-Float Nodes",
            TopDataIds_t{                   idNull,                   idSigUpdFloat,                       idSigValFloat,                idUpdMach,                    idMachUpdEnqueue,                 idScnParts,                       idMachEvtTags },
            wrap_args([] (WorkerContext const ctx, UpdateNodes<float>& rSigUpdFloat, SignalValues_t<float>& rSigValFloat, UpdMachPerType& rUpdMach, std::vector<TagId>& rMachUpdEnqueue, ACtxParts const& rScnParts, MachTypeToEvt_t const& rMachEvtTags ) noexcept
    {
        if ( ! rSigUpdFloat.m_dirty )
        {
            return; // Not dirty, nothing to do
        }

        Nodes const &rFloatNodes = rScnParts.m_nodePerType[gc_ntSigFloat];

        // NOTE: The various use of reset() clear entire bit arrays, which may or may
        //       not be expensive. They likely use memset

        for (std::size_t const machTypeDirty : rUpdMach.m_machTypesDirty.ones())
        {
            rUpdMach.m_localDirty[machTypeDirty].reset();
        }
        rUpdMach.m_machTypesDirty.reset();

        // Sees which nodes changed, and writes into rUpdMach set dirty which MACHINES
        // must be updated next
        update_signal_nodes<float>(
                rSigUpdFloat.m_nodeDirty.ones(),
                rFloatNodes.m_nodeToMach,
                rScnParts.m_machines,
                arrayView(rSigUpdFloat.m_nodeNewValues),
                rSigValFloat,
                rUpdMach);
        rSigUpdFloat.m_nodeDirty.reset();
        rSigUpdFloat.m_dirty = false;

        // Tasks cannot be enqueued here directly, since that will interfere with other node reduce
        // tasks. All machine tasks must be enqueued at the same time. rMachUpdEnqueue here is
        // passed to a task in setup_parts

        // Run tasks needed to update machine types that are dirty
        for (MachTypeId const type : rUpdMach.m_machTypesDirty.ones())
        {
            rMachUpdEnqueue.push_back(rMachEvtTags[type]);
        }
    }));

    signalsFloat.task() = rBuilder.task().assign({tgSceneEvt, tgLinkReq, tgSigFloatLinkMod}).data(
            "Allocate Signal-Float Node Values",
            TopDataIds_t{                    idSigUpdFloat,                       idSigValFloat,                 idScnParts  },
            wrap_args([] (UpdateNodes<float>& rSigUpdFloat, SignalValues_t<float>& rSigValFloat, ACtxParts const& rScnParts) noexcept
    {
        Nodes const &rFloatNodes = rScnParts.m_nodePerType[gc_ntSigFloat];
        rSigUpdFloat.m_nodeNewValues.resize(rFloatNodes.m_nodeIds.capacity());
        rSigUpdFloat.m_nodeDirty.ints().resize(rFloatNodes.m_nodeIds.vec().capacity());
        rSigValFloat.resize(rFloatNodes.m_nodeIds.capacity());
    }));

    return signalsFloat;
}

template <MachTypeId const& MachType_T>
TopTaskFunc_t gen_allocate_mach_bitsets()
{
    static TopTaskFunc_t const func = wrap_args([] (ACtxParts& rScnParts, UpdMachPerType& rUpdMach) noexcept
    {
        rUpdMach.m_localDirty[MachType_T].ints().resize(rScnParts.m_machines.m_perType[MachType_T].m_localIds.vec().capacity());
    });

    return func;
}

Session setup_mach_rocket(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              scnCommon,
        Session const&              parts,
        Session const&              signalsFloat)
{
    OSP_SESSION_UNPACK_TAGS(scnCommon,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(signalsFloat,   TESTAPP_SIGNALS_FLOAT)
    OSP_SESSION_UNPACK_TAGS(signalsFloat,   TESTAPP_SIGNALS_FLOAT);
    OSP_SESSION_UNPACK_DATA(parts,          TESTAPP_PARTS);
    OSP_SESSION_UNPACK_TAGS(parts,          TESTAPP_PARTS);

    using namespace adera;

    Session machRocket;
    OSP_SESSION_ACQUIRE_TAGS(machRocket, rTags,   TESTAPP_MACH_ROCKET);

    top_get< MachTypeToEvt_t >(topData, idMachEvtTags).at(gc_mtMagicRocket) = tgMhRocketEvt;

    machRocket.task() = rBuilder.task().assign({tgSceneEvt, tgLinkReq, tgLinkMhUpdMod}).data(
            "Allocate Machine update bitset for MagicRocket",
            TopDataIds_t{idScnParts, idUpdMach},
            gen_allocate_mach_bitsets<gc_mtMagicRocket>());

    return machRocket;
}

Session setup_mach_rcsdriver(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              scnCommon,
        Session const&              parts,
        Session const&              signalsFloat)
{
    OSP_SESSION_UNPACK_TAGS(scnCommon,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(signalsFloat,   TESTAPP_SIGNALS_FLOAT)
    OSP_SESSION_UNPACK_TAGS(signalsFloat,   TESTAPP_SIGNALS_FLOAT);
    OSP_SESSION_UNPACK_DATA(parts,          TESTAPP_PARTS);
    OSP_SESSION_UNPACK_TAGS(parts,          TESTAPP_PARTS);

    using namespace adera;

    Session machRcsDriver;
    OSP_SESSION_ACQUIRE_TAGS(machRcsDriver, rTags,   TESTAPP_MACH_RCSDRIVER);

    top_get< MachTypeToEvt_t >(topData, idMachEvtTags).at(gc_mtRcsDriver) = tgMhRcsDriverEvt;

    machRcsDriver.task() = rBuilder.task().assign({tgMhRcsDriverEvt, tgSigFloatUpdMod}).data(
            "RCS Drivers calculate new values",
            TopDataIds_t{           idScnParts,                      idUpdMach,                       idSigValFloat,                    idSigUpdFloat },
            wrap_args([] (ACtxParts& rScnParts, UpdMachPerType const& rUpdMach, SignalValues_t<float>& rSigValFloat, UpdateNodes<float>& rSigUpdFloat) noexcept
    {
        Nodes const &rFloatNodes = rScnParts.m_nodePerType[gc_ntSigFloat];
        PerMachType &rRockets = rScnParts.m_machines.m_perType[gc_mtRcsDriver];

        for (MachLocalId const local : rUpdMach.m_localDirty[gc_mtRcsDriver].ones())
        {
            MachAnyId const mach = rRockets.m_localToAny[local];
            lgrn::Span<NodeId const> const portSpan = rFloatNodes.m_machToNode[mach];

            NodeId const thrNode = connected_node(portSpan, ports_rcsdriver::gc_throttleOut.m_port);
            if (thrNode == lgrn::id_null<NodeId>())
            {
                continue; // Throttle Output not connected, calculations below are useless
            }

            auto const rcs_read = [&rSigValFloat, portSpan] (float& rDstVar, PortEntry const& entry)
            {
                NodeId const node = connected_node(portSpan, entry.m_port);

                if (node != lgrn::id_null<NodeId>())
                {
                    rDstVar = rSigValFloat[node];
                }
            };

            Vector3 pos{0.0f};
            Vector3 dir{0.0f};
            Vector3 cmdLin{0.0f};
            Vector3 cmdAng{0.0f};

            rcs_read( pos.x(),    ports_rcsdriver::gc_posXIn    );
            rcs_read( pos.y(),    ports_rcsdriver::gc_posYIn    );
            rcs_read( pos.z(),    ports_rcsdriver::gc_posZIn    );
            rcs_read( dir.x(),    ports_rcsdriver::gc_dirXIn    );
            rcs_read( dir.y(),    ports_rcsdriver::gc_dirYIn    );
            rcs_read( dir.z(),    ports_rcsdriver::gc_dirZIn    );
            rcs_read( cmdLin.x(), ports_rcsdriver::gc_cmdLinXIn );
            rcs_read( cmdLin.y(), ports_rcsdriver::gc_cmdLinYIn );
            rcs_read( cmdLin.z(), ports_rcsdriver::gc_cmdLinZIn );
            rcs_read( cmdAng.x(), ports_rcsdriver::gc_cmdAngXIn );
            rcs_read( cmdAng.y(), ports_rcsdriver::gc_cmdAngYIn );
            rcs_read( cmdAng.z(), ports_rcsdriver::gc_cmdAngZIn );

            OSP_LOG_TRACE("RCS controller {} pitch = {}", local, cmdAng.x());
            OSP_LOG_TRACE("RCS controller {} yaw = {}", local, cmdAng.y());
            OSP_LOG_TRACE("RCS controller {} roll = {}", local, cmdAng.z());

            float const thrCurr = rSigValFloat[thrNode];
            float const thrNew = thruster_influence(pos, dir, cmdLin, cmdAng);

            if (thrCurr != thrNew)
            {
                rSigUpdFloat.assign(thrNode, thrNew);
            }
        }
    }));

    machRcsDriver.task() = rBuilder.task().assign({tgSceneEvt, tgLinkReq, tgLinkMhUpdMod}).data(
            "Allocate Machine update bitset for RCS Drivers",
            TopDataIds_t{idScnParts, idUpdMach},
            gen_allocate_mach_bitsets<gc_mtRcsDriver>());

    return machRcsDriver;
}

Session setup_vehicle_spawn(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              scnCommon)
{
    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);

    Session vehicleSpawn;
    OSP_SESSION_ACQUIRE_DATA(vehicleSpawn, topData, TESTAPP_VEHICLE_SPAWN);
    OSP_SESSION_ACQUIRE_TAGS(vehicleSpawn, rTags,   TESTAPP_VEHICLE_SPAWN);

    rBuilder.tag(tgVsBasicInReq)        .depend_on({tgVsBasicInMod});
    rBuilder.tag(tgVsBasicInClr)        .depend_on({tgVsBasicInMod, tgVsBasicInReq});
    rBuilder.tag(tgVsPartReq)           .depend_on({tgVsPartMod});
    rBuilder.tag(tgVsMapPartMachReq)    .depend_on({tgVsMapPartMachMod});
    rBuilder.tag(tgVsPartPfReq)         .depend_on({tgVsPartPfMod});
    rBuilder.tag(tgVsWeldReq)           .depend_on({tgVsWeldMod});

    top_emplace< ACtxVehicleSpawn >     (topData, idVehicleSpawn);

    vehicleSpawn.task() = rBuilder.task().assign({tgSceneEvt, tgVsBasicInClr}).data(
            "Clear Vehicle Spawning vector after use",
            TopDataIds_t{                  idVehicleSpawn},
            wrap_args([] (ACtxVehicleSpawn& rVehicleSpawn) noexcept
    {
        rVehicleSpawn.m_newVhBasicIn.clear();
    }));

    return vehicleSpawn;
}

Session setup_vehicle_spawn_vb(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        [[maybe_unused]] Tags&      rTags,
        Session const&              scnCommon,
        Session const&              prefabs,
        Session const&              parts,
        Session const&              vehicleSpawn,
        Session const&              signalsFloat,
        TopDataId const             idResources)
{
    OSP_SESSION_UNPACK_DATA(scnCommon,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(scnCommon,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(prefabs,        TESTAPP_PREFABS);
    OSP_SESSION_UNPACK_TAGS(prefabs,        TESTAPP_PREFABS);
    OSP_SESSION_UNPACK_DATA(vehicleSpawn,   TESTAPP_VEHICLE_SPAWN);
    OSP_SESSION_UNPACK_TAGS(vehicleSpawn,   TESTAPP_VEHICLE_SPAWN);
    OSP_SESSION_UNPACK_DATA(parts,          TESTAPP_PARTS);
    OSP_SESSION_UNPACK_TAGS(parts,          TESTAPP_PARTS);
    OSP_SESSION_UNPACK_DATA(signalsFloat,   TESTAPP_SIGNALS_FLOAT);
    OSP_SESSION_UNPACK_TAGS(signalsFloat,   TESTAPP_SIGNALS_FLOAT);

    Session vehicleSpawnVB;
    OSP_SESSION_ACQUIRE_DATA(vehicleSpawnVB, topData, TESTAPP_VEHICLE_SPAWN_VB);
    OSP_SESSION_ACQUIRE_TAGS(vehicleSpawnVB, rTags,   TESTAPP_VEHICLE_SPAWN_VB);

    rBuilder.tag(tgVbSpBasicInReq)      .depend_on({tgVbSpBasicInMod});
    rBuilder.tag(tgVbPartReq)           .depend_on({tgVbPartMod});
    rBuilder.tag(tgVbWeldReq)           .depend_on({tgVbWeldMod});
    rBuilder.tag(tgVbMachReq)           .depend_on({tgVbMachMod});

    top_emplace< ACtxVehicleSpawnVB >(topData, idVehicleSpawnVB);

    vehicleSpawnVB.task() = rBuilder.task().assign({tgSceneEvt, tgVsBasicInReq, tgVbSpBasicInReq, tgEntNew, tgPartMod, tgWeldMod, tgVsPartMod, tgVsWeldMod, tgVbPartMod, tgVbWeldMod}).data(
            "Create part IDs for vehicles from VehicleData",
            TopDataIds_t{                  idVehicleSpawn,                    idVehicleSpawnVB,           idScnParts},
            wrap_args([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB& rVehicleSpawnVB, ACtxParts& rScnParts) noexcept
    {
        std::size_t const newVehicleCount = rVehicleSpawn.new_vehicle_count();
        ACtxVehicleSpawnVB &rVSVB = rVehicleSpawnVB;

        if (newVehicleCount == 0)
        {
            return;
        }

        rVSVB.m_remapPartOffsets            .resize(newVehicleCount);
        rVSVB.m_remapWeldOffsets            .resize(newVehicleCount);
        rVehicleSpawn.m_newVhPartOffsets    .resize(newVehicleCount);
        rVehicleSpawn.m_newVhWeldOffsets    .resize(newVehicleCount);

        // Count total parts and welds, and calculate offsets for remaps

        std::size_t partTotal       = 0;
        std::size_t remapPartTotal  = 0;

        std::size_t weldTotal       = 0;
        std::size_t remapWeldTotal  = 0;

        for (NewVehicleId vhId = 0; vhId < newVehicleCount; ++vhId)
        {
            VehicleData const* pVData = rVSVB.m_dataVB[vhId];
            if (pVData == nullptr)
            {
                continue;
            }

            rVehicleSpawn.m_newVhPartOffsets[vhId] = partTotal;
            partTotal += pVData->m_partIds.size();

            rVSVB.m_remapPartOffsets[vhId] = remapPartTotal;
            remapPartTotal += pVData->m_partIds.capacity();

            rVehicleSpawn.m_newVhWeldOffsets[vhId] = weldTotal;
            weldTotal += pVData->m_weldIds.size();

            rVSVB.m_remapWeldOffsets[vhId] = remapWeldTotal;
            remapWeldTotal += pVData->m_weldIds.capacity();
        }

        // Resize containers for new IDs

        rVehicleSpawn.m_newPartToPart   .resize(partTotal);
        rVehicleSpawn.m_newWeldToWeld   .resize(weldTotal);
        rVehicleSpawn.m_newPartPrefabs  .resize(partTotal);
        rVSVB.m_remapParts              .resize(remapPartTotal, lgrn::id_null<PartId>());
        rVSVB.m_remapWelds              .resize(remapPartTotal, lgrn::id_null<WeldId>());

        // Create new Scene PartIds and WeldIds

        rScnParts.m_partIds.create(std::begin(rVehicleSpawn.m_newPartToPart), std::end(rVehicleSpawn.m_newPartToPart));
        rScnParts.m_weldIds.create(std::begin(rVehicleSpawn.m_newWeldToWeld), std::end(rVehicleSpawn.m_newWeldToWeld));

        rScnParts.m_partDirty.insert(std::begin(rScnParts.m_partDirty), std::begin(rVehicleSpawn.m_newPartToPart), std::end(rVehicleSpawn.m_newPartToPart));
        rScnParts.m_weldDirty.insert(std::begin(rScnParts.m_weldDirty), std::begin(rVehicleSpawn.m_newWeldToWeld), std::end(rVehicleSpawn.m_newWeldToWeld));

        // Resize scene containers to account for new IDs

        std::size_t const maxParts = rScnParts.m_partIds.capacity();
        std::size_t const maxWelds = rScnParts.m_weldIds.capacity();
        rScnParts.m_partPrefabs         .resize(maxParts);
        rScnParts.m_partTransformWeld   .resize(maxParts);
        rScnParts.m_partToWeld          .resize(maxParts);
        rScnParts.m_weldToParts         .data_reserve(maxParts);
        rScnParts.m_weldToParts         .ids_reserve(maxWelds);
        rScnParts.m_weldToEnt           .resize(maxWelds);
        rVehicleSpawn.m_partToNewPart   .resize(maxParts);

        // Populate "Scene PartId -> NewPartId" map

        auto itPart = std::begin(rVehicleSpawn.m_newPartToPart);
        for (PartId newPart = 0; newPart < rVehicleSpawn.m_newPartToPart.size(); ++newPart)
        {
            rVehicleSpawn.m_partToNewPart[*itPart] = newPart;
            std::advance(itPart, 1);
        }

        // Populate remap vectors and set weld connections

        auto itDstPartIds = std::cbegin(rVehicleSpawn.m_newPartToPart);
        auto itDstWeldIds = std::cbegin(rVehicleSpawn.m_newWeldToWeld);

        for (NewVehicleId vhId = 0; vhId < newVehicleCount; ++vhId)
        {
            VehicleData const* pVData = rVSVB.m_dataVB[vhId];
            if (pVData == nullptr)
            {
                continue;
            }

            std::size_t const remapPartOffset = rVSVB.m_remapPartOffsets[vhId];
            std::size_t const remapWeldOffset = rVSVB.m_remapWeldOffsets[vhId];

            for (PartId const srcPart : pVData->m_partIds.bitview().zeros())
            {
                PartId const dstPart = *itDstPartIds;
                ++itDstPartIds;

                // Populate map for "VehicleBuilder PartId -> ACtxParts PartId"
                rVSVB.m_remapParts[remapPartOffset + srcPart] = dstPart;
            }

            for (WeldId const srcWeld : pVData->m_weldIds.bitview().zeros())
            {
                WeldId const dstWeld = *itDstWeldIds;
                ++itDstWeldIds;

                // Populate map for "VehicleBuilder WeldId -> ACtxParts WeldId"
                // rVehicleSpawnVB.m_remapWelds
                rVSVB.m_remapWelds[remapWeldOffset + srcWeld] = dstWeld;

                // Use remaps to connect ACtxParts WeldIds and PartIds
                // rScnParts.m_partToWeld and rScnParts.m_weldToParts

                auto const srcWeldPartSpan  = pVData->m_weldToParts[srcWeld];
                WeldId *pDstWeldPartOut     = rScnParts.m_weldToParts.emplace(dstWeld, srcWeldPartSpan.size());

                for (PartId const srcPart : srcWeldPartSpan)
                {
                    PartId const dstPart = rVSVB.m_remapParts[remapPartOffset + srcPart];

                    (*pDstWeldPartOut) = dstPart;
                    std::advance(pDstWeldPartOut, 1);

                    rScnParts.m_partToWeld[dstPart] = dstWeld;
                }
            }
        }
    }));

    vehicleSpawnVB.task() = rBuilder.task().assign({tgSceneEvt, tgVsBasicInReq, tgVsPartReq, tgVbPartReq, tgPrefabMod}).data(
            "Request prefabs for vehicle parts from VehicleBuilder",
            TopDataIds_t{                  idVehicleSpawn,                          idVehicleSpawnVB,           idScnParts,                idPrefabInit,           idResources},
            wrap_args([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB const& rVehicleSpawnVB, ACtxParts& rScnParts, ACtxPrefabInit& rPrefabInit, Resources& rResources) noexcept
    {
        if (rVehicleSpawn.new_vehicle_count() == 0)
        {
            return;
        }

        auto itDstPartIds = std::cbegin(rVehicleSpawn.m_newPartToPart);
        auto itPrefabOut   = std::begin(rVehicleSpawn.m_newPartPrefabs);

        for (VehicleData const* pVData : rVehicleSpawnVB.m_dataVB)
        {
            if (pVData == nullptr)
            {
                continue;
            }

            // Copy Part data from VehicleBuilder to scene
            for (uint32_t srcPart : pVData->m_partIds.bitview().zeros())
            {
                PartId const dstPart = *itDstPartIds;
                std::advance(itDstPartIds, 1);

                PrefabPair const& prefabPairSrc = pVData->m_partPrefabs[srcPart];
                PrefabPair prefabPairDst{
                    rResources.owner_create(gc_importer, prefabPairSrc.m_importer),
                    prefabPairSrc.m_prefabId
                };
                rScnParts.m_partPrefabs[dstPart]        = std::move(prefabPairDst);
                rScnParts.m_partTransformWeld[dstPart]  = pVData->m_partTransformWeld[srcPart];

                // Add Prefab and Part init events
                (*itPrefabOut) = rPrefabInit.m_basicIn.size();
                std::advance(itPrefabOut, 1);

                rPrefabInit.m_basicIn.push_back(TmpPrefabInitBasic{
                    .m_importerRes = prefabPairSrc.m_importer,
                    .m_prefabId = prefabPairSrc.m_prefabId,
                    .m_pTransform = &pVData->m_partTransformWeld[srcPart]
                });
            }
        }

    }));

    vehicleSpawnVB.task() = rBuilder.task().assign({tgSceneEvt, tgVsBasicInReq, tgVbMachMod, tgLinkMod}).data(
            "Copy Machine IDs from VehicleData to ACtxParts",
            TopDataIds_t{                  idVehicleSpawn,                    idVehicleSpawnVB,           idScnParts},
            wrap_args([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB& rVehicleSpawnVB, ACtxParts& rScnParts) noexcept
    {
        std::size_t const newVehicleCount = rVehicleSpawn.new_vehicle_count();
        ACtxVehicleSpawnVB &rVSVB = rVehicleSpawnVB;

        if (newVehicleCount == 0)
        {
            return;
        }

        // Count total machines, and calculate offsets for remaps.

        std::size_t machTotal = 0;
        std::size_t remapMachTotal = 0;

        rVSVB.m_machtypeCount.clear();
        rVSVB.m_machtypeCount.resize(MachTypeReg_t::size(), 0);

        rVSVB.m_remapMachOffsets.resize(newVehicleCount);

        for (NewVehicleId vhId = 0; vhId < newVehicleCount; ++vhId)
        {
            VehicleData const* pVData = rVSVB.m_dataVB[vhId];
            if (pVData == nullptr)
            {
                continue;
            }

            Machines const &srcMachines = pVData->m_machines;
            std::size_t const bounds = srcMachines.m_ids.capacity();

            rVSVB.m_remapMachOffsets[vhId] = remapMachTotal;

            remapMachTotal += bounds;
            machTotal += srcMachines.m_ids.size();

            for (MachTypeId type = 0; type < MachTypeReg_t::size(); ++type)
            {
                rVSVB.m_machtypeCount[type] += srcMachines.m_perType[type].m_localIds.size();
            }
        }

        rVehicleSpawn.m_newMachToMach.resize(machTotal);
        rVSVB.m_remapMachs.resize(remapMachTotal);

        // Create ACtxParts MachAny/LocalIDs and populate remaps

        // MachAnyIDs created here
        rScnParts.m_machines.m_ids.create(rVehicleSpawn.m_newMachToMach.begin(),
                                          rVehicleSpawn.m_newMachToMach.end());

        rScnParts.m_machines.m_machToLocal.resize(rScnParts.m_machines.m_ids.capacity());

        auto itDstMachIds = std::cbegin(rVehicleSpawn.m_newMachToMach);

        for (NewVehicleId vhId = 0; vhId < newVehicleCount; ++vhId)
        {
            VehicleData const* pVData = rVSVB.m_dataVB[vhId];
            if (pVData == nullptr)
            {
                continue;
            }

            Machines const &srcMachines = pVData->m_machines;

            std::size_t const remapMachOffset = rVSVB.m_remapMachOffsets[vhId];

            for (MachAnyId const srcMach : srcMachines.m_ids.bitview().zeros())
            {
                MachAnyId const dstMach = *itDstMachIds;
                ++itDstMachIds;

                // Populate map for "VehicleBuilder MachAnyId -> ACtxParts MachAnyId"
                rVSVB.m_remapMachs[remapMachOffset + srcMach] = dstMach;

                // Create ACtxParts MachLocalIds
                // MachLocalIds don't need a remap, since they can be obtained
                // from a MachAnyId.
                // TODO: This can be optimized later, where all local IDs are
                //       created at once with ids.create(first, last), and make
                //       resize(..) called once per type too
                MachTypeId const    type            = srcMachines.m_machTypes[srcMach];
                PerMachType&        rDstPerType     = rScnParts.m_machines.m_perType[type];

                MachLocalId const dstLocal = rDstPerType.m_localIds.create();
                rDstPerType.m_localToAny.resize(rDstPerType.m_localIds.capacity());

                rDstPerType.m_localToAny[dstLocal] = dstMach;
                rScnParts.m_machines.m_machToLocal[dstMach] = dstLocal;
            }
        }
    }));

    vehicleSpawnVB.task() = rBuilder.task().assign({tgSceneEvt, tgVsBasicInReq, tgVsPartReq, tgVbPartReq, tgVbMachReq, tgVsMapPartMachMod}).data(
            "Update Part<->Machine maps",
            TopDataIds_t{                  idVehicleSpawn,                          idVehicleSpawnVB,           idScnParts,                idPrefabInit,           idResources},
            wrap_args([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB const& rVehicleSpawnVB, ACtxParts& rScnParts, ACtxPrefabInit& rPrefabInit, Resources& rResources) noexcept
    {
        std::size_t const newVehicleCount = rVehicleSpawn.new_vehicle_count();
        ACtxVehicleSpawnVB const& rVSVB = rVehicleSpawnVB;

        if (newVehicleCount == 0)
        {
            return;
        }

        for (NewVehicleId vhId = 0; vhId < newVehicleCount; ++vhId)
        {
            VehicleData const* pVData = rVSVB.m_dataVB[vhId];
            if (pVData == nullptr)
            {
                continue;
            }

            rScnParts.m_machineToPart.resize(rScnParts.m_machines.m_ids.capacity());
            rScnParts.m_partToMachines.ids_reserve(rScnParts.m_partIds.capacity());
            rScnParts.m_partToMachines.data_reserve(rScnParts.m_machines.m_ids.capacity());

            std::size_t const remapMachOffset = rVSVB.m_remapMachOffsets[vhId];
            std::size_t const remapPartOffset = rVSVB.m_remapPartOffsets[vhId];

            // Update rScnParts machine->part map
            for (MachAnyId const srcMach : pVData->m_machines.m_ids.bitview().zeros())
            {
                MachAnyId const dstMach = rVSVB.m_remapMachs[remapMachOffset + srcMach];
                PartId const    srcPart = pVData->m_machToPart[srcMach];
                PartId const    dstPart = rVSVB.m_remapParts[remapPartOffset + srcPart];

                rScnParts.m_machineToPart[dstMach] = dstPart;
            }

            // Update rScnParts part->machine multimap
            for (PartId const srcPart : pVData->m_partIds.bitview().zeros())
            {
                PartId const dstPart = rVSVB.m_remapParts[remapPartOffset + srcPart];

                auto const& srcPairs = pVData->m_partToMachines[srcPart];

                rScnParts.m_partToMachines.emplace(dstPart, srcPairs.size());
                auto dstPairs = rScnParts.m_partToMachines[dstPart];

                for (int i = 0; i < srcPairs.size(); ++i)
                {
                    MachinePair const&  srcPair  = srcPairs[i];
                    MachinePair&        rDstPair = dstPairs[i];
                    MachAnyId const     srcMach  = pVData->m_machines.m_perType[srcPair.m_type].m_localToAny[srcPair.m_local];
                    MachAnyId const     dstMach  = rVSVB.m_remapMachs[remapMachOffset + srcMach];
                    MachTypeId const    dstType  = srcPair.m_type;
                    MachLocalId const   dstLocal = rScnParts.m_machines.m_machToLocal[dstMach];

                    rDstPair = { .m_local = dstLocal, .m_type = dstType };
                }
            }
        }
    }));

    vehicleSpawnVB.task() = rBuilder.task().assign({tgSceneEvt, tgVsPartReq, tgVbPartReq, tgVbMachReq, tgLinkMod, tgVbNodeMod}).data(
            "Copy Node IDs from VehicleBuilder to ACtxParts",
            TopDataIds_t{                  idVehicleSpawn,                    idVehicleSpawnVB,           idScnParts},
            wrap_args([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB& rVehicleSpawnVB, ACtxParts& rScnParts) noexcept
    {
        std::size_t const newVehicleCount = rVehicleSpawn.new_vehicle_count();
        ACtxVehicleSpawnVB &rVSVB = rVehicleSpawnVB;

        if (newVehicleCount == 0)
        {
            return;
        }

        rVSVB.m_remapNodeOffsets.resize(newVehicleCount * NodeTypeReg_t::size());
        auto remapNodeOffsets2d = rVSVB.remap_node_offsets_2d();

        // Add up bounds needed for all nodes of every type for remaps
        std::size_t remapNodeTotal = 0;
        for (VehicleData const* pVData : rVSVB.m_dataVB)
        {
            if (pVData == nullptr)
            {
                continue;
            }
            for (PerNodeType const &rSrcNodeType : pVData->m_nodePerType)
            {
                remapNodeTotal += rSrcNodeType.m_nodeIds.capacity();
            }
        }
        rVSVB.m_remapNodes.resize(remapNodeTotal);

        std::size_t nodeRemapUsed = 0;

        for (NewVehicleId vhId = 0; vhId < newVehicleCount; ++vhId)
        {
            VehicleData const* pVData = rVSVB.m_dataVB[vhId];
            if (pVData == nullptr)
            {
                continue;
            }

            auto machRemap = arrayView(std::as_const(rVSVB.m_remapMachs)).exceptPrefix(rVSVB.m_remapMachOffsets[vhId]);

            for (NodeTypeId nodeType = 0; nodeType < NodeTypeReg_t::size(); ++nodeType)
            {
                PerNodeType const &rSrcNodeType = pVData->m_nodePerType[nodeType];

                std::size_t const remapSize = rSrcNodeType.m_nodeIds.capacity();
                auto nodeRemapOut = arrayView(rVSVB.m_remapNodes).sliceSize(nodeRemapUsed, remapSize);
                remapNodeOffsets2d[vhId][nodeType] = nodeRemapUsed;
                nodeRemapUsed += remapSize;
                copy_nodes(rSrcNodeType, pVData->m_machines, machRemap,
                           rScnParts.m_nodePerType[nodeType], rScnParts.m_machines, nodeRemapOut);
            }
        }
    }));

    vehicleSpawnVB.task() = rBuilder.task().assign({tgSceneEvt, tgVsPartReq, tgPrefabEntReq, tgMapPartEntMod}).data(
            "Update PartId<->ActiveEnt mapping",
            TopDataIds_t{                  idVehicleSpawn,           idScnParts,                   idActiveIds,                 idPrefabInit },
            wrap_args([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxParts& rScnParts, ActiveReg_t const& rActiveIds,  ACtxPrefabInit& rPrefabInit) noexcept
    {
        if (rVehicleSpawn.new_vehicle_count() == 0)
        {
            return;
        }

        rScnParts.m_partToActive.resize(rScnParts.m_partIds.capacity());
        rScnParts.m_activeToPart.resize(rActiveIds.capacity());

        // Populate PartId<->ActiveEnt mapping, now that the prefabs exist

        auto itPrefab = std::begin(rVehicleSpawn.m_newPartPrefabs);

        for (PartId const partId : rVehicleSpawn.m_newPartToPart)
        {
            ActiveEnt const root = rPrefabInit.m_ents[*itPrefab].front();
            std::advance(itPrefab, 1);

            rScnParts.m_partToActive[partId]            = root;
            rScnParts.m_activeToPart[std::size_t(root)] = partId;
        }
    }));

    vehicleSpawnVB.task() = rBuilder.task().assign({tgSceneEvt, tgVbNodeReq, tgSigFloatLinkReq}).data(
            "Copy float signal values from VehicleBuilder",
            TopDataIds_t{                  idVehicleSpawn,                          idVehicleSpawnVB,           idScnParts,                       idSigValFloat},
            wrap_args([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB const& rVehicleSpawnVB, ACtxParts& rScnParts, SignalValues_t<float>& rSigValFloat) noexcept
    {
        std::size_t const newVehicleCount = rVehicleSpawn.new_vehicle_count();
        ACtxVehicleSpawnVB const &rVSVB = rVehicleSpawnVB;

        if (newVehicleCount == 0)
        {
            return;
        }

        auto const remapNodeOffsets2d = rVSVB.remap_node_offsets_2d();

        for (NewVehicleId vhId = 0; vhId < newVehicleCount; ++vhId)
        {
            VehicleData const* pVData = rVSVB.m_dataVB[vhId];
            if (pVData == nullptr)
            {
                continue;
            }

            PerNodeType const&  srcFloatNodes       = pVData->m_nodePerType[gc_ntSigFloat];
            entt::any const     srcFloatValuesAny   = srcFloatNodes.m_nodeValues;
            auto const&         srcFloatValues      = entt::any_cast< SignalValues_t<float> >(srcFloatValuesAny);
            std::size_t const   nodeRemapOffset     = remapNodeOffsets2d[vhId][gc_ntSigFloat];
            auto const          nodeRemap           = arrayView(rVSVB.m_remapNodes).exceptPrefix(nodeRemapOffset);

            for (NodeId const srcNode : srcFloatNodes.m_nodeIds.bitview().zeros())
            {
                NodeId const dstNode = nodeRemap[srcNode];
                rSigValFloat[dstNode] = srcFloatValues[srcNode];
            }
        }
    }));

    return vehicleSpawnVB;
}

Matrix4 quick_transform(Vector3 const pos, Quaternion const rot) noexcept
{
    return Matrix4::from(rot.toMatrix(), pos);
}

struct RCSInputs
{
    NodeId m_pitch  {lgrn::id_null<NodeId>()};
    NodeId m_yaw    {lgrn::id_null<NodeId>()};
    NodeId m_roll   {lgrn::id_null<NodeId>()};
};

void add_rcs_machines(VehicleBuilder& rBuilder, RCSInputs const& inputs, PartId part, float thrustMul, Matrix4 const& tf)
{
    using namespace adera;
    namespace ports_rcsdriver = adera::ports_rcsdriver;
    namespace ports_magicrocket = adera::ports_magicrocket;

    auto const [posX, posY, posZ, dirX, dirY, dirZ, driverOut, thrMul] = rBuilder.create_nodes<8>(gc_ntSigFloat);

    rBuilder.create_machine(part, gc_mtRcsDriver, {
        { ports_rcsdriver::gc_posXIn,       posX            },
        { ports_rcsdriver::gc_posYIn,       posY            },
        { ports_rcsdriver::gc_posZIn,       posZ            },
        { ports_rcsdriver::gc_dirXIn,       dirX            },
        { ports_rcsdriver::gc_dirYIn,       dirY            },
        { ports_rcsdriver::gc_dirZIn,       dirZ            },
        { ports_rcsdriver::gc_cmdAngXIn,    inputs.m_pitch  },
        { ports_rcsdriver::gc_cmdAngYIn,    inputs.m_yaw    },
        { ports_rcsdriver::gc_cmdAngZIn,    inputs.m_roll   },
        { ports_rcsdriver::gc_throttleOut,  driverOut       }
    } );

    rBuilder.create_machine(part, gc_mtMagicRocket, {
        { ports_magicrocket::gc_throttleIn, driverOut },
        { ports_magicrocket::gc_multiplierIn, thrMul }
    } );

    Vector3 const dir = tf.rotation() * gc_rocketForward;

    auto &rFloatValues = rBuilder.node_values< SignalValues_t<float> >(gc_ntSigFloat);

    rFloatValues[posX] = tf.translation().x();
    rFloatValues[posY] = tf.translation().y();
    rFloatValues[posZ] = tf.translation().z();
    rFloatValues[dirX] = dir.x();
    rFloatValues[dirY] = dir.y();
    rFloatValues[dirZ] = dir.z();
    rFloatValues[thrMul] = thrustMul;
}

void add_rcs_block(VehicleBuilder& rBuilder, VehicleBuilder::WeldVec_t& rWeldTo, RCSInputs const& inputs, float thrustMul, Vector3 pos, Quaternion rot)
{
    constexpr Vector3 xAxis{1.0f, 0.0f, 0.0f};

    auto const [ nozzleA, nozzleB ] = rBuilder.create_parts<2>();
    rBuilder.set_prefabs({
        { nozzleA,  "phLinRCS" },
        { nozzleB,  "phLinRCS" }
    });

    Matrix4 const nozzleTfA = quick_transform(pos, rot * Quaternion::rotation( 90.0_degf,  xAxis));
    Matrix4 const nozzleTfB = quick_transform(pos, rot * Quaternion::rotation( -90.0_degf, xAxis));

    add_rcs_machines(rBuilder, inputs, nozzleA, thrustMul, nozzleTfA);
    add_rcs_machines(rBuilder, inputs, nozzleB, thrustMul, nozzleTfB);

    rWeldTo.push_back({ nozzleA, nozzleTfA });
    rWeldTo.push_back({ nozzleB, nozzleTfB });
}

Session setup_test_vehicles(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        [[maybe_unused]] Tags&      rTags,
        Session const&              scnCommon,
        TopDataId const             idResources)
{
    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);

    Session testVehicles;
    OSP_SESSION_ACQUIRE_DATA(testVehicles, topData, TESTAPP_TEST_VEHICLES);
    testVehicles.m_tgCleanupEvt = tgCleanupEvt;

    auto &rResources = top_get<Resources>(topData, idResources);

    using namespace adera;

    // Build "PartVehicle"
    {
        VehicleBuilder vbuilder{&rResources};
        VehicleBuilder::WeldVec_t toWeld;

        auto const [ capsule, fueltank, engineA, engineB ] = vbuilder.create_parts<4>();
        vbuilder.set_prefabs({
            { capsule,  "phCapsule" },
            { fueltank, "phFuselage" },
            { engineA,  "phEngine" },
            { engineB,  "phEngine" },
        });

        toWeld.push_back( {capsule,  quick_transform({ 0.0f,  0.0f,  3.0f}, {})} );
        toWeld.push_back( {fueltank, quick_transform({ 0.0f,  0.0f,  0.0f}, {})} );
        toWeld.push_back( {engineA,  quick_transform({ 0.7f,  0.0f, -2.9f}, {})} );
        toWeld.push_back( {engineB,  quick_transform({-0.7f,  0.0f, -2.9f},{})} );

        namespace ports_magicrocket = adera::ports_magicrocket;
        namespace ports_userctrl = adera::ports_userctrl;

        auto const [ pitch, yaw, roll, throttle, thrustMul ] = vbuilder.create_nodes<5>(gc_ntSigFloat);

        auto &rFloatValues = vbuilder.node_values< SignalValues_t<float> >(gc_ntSigFloat);
        rFloatValues[thrustMul] = 50000.0f;

        vbuilder.create_machine(capsule, gc_mtUserCtrl, {
            { ports_userctrl::gc_throttleOut,   throttle },
            { ports_userctrl::gc_pitchOut,      pitch    },
            { ports_userctrl::gc_yawOut,        yaw      },
            { ports_userctrl::gc_rollOut,       roll     }
        } );

        vbuilder.create_machine(engineA, gc_mtMagicRocket, {
            { ports_magicrocket::gc_throttleIn, throttle },
            { ports_magicrocket::gc_multiplierIn, thrustMul }
        } );

        vbuilder.create_machine(engineB, gc_mtMagicRocket, {
            { ports_magicrocket::gc_throttleIn, throttle },
            { ports_magicrocket::gc_multiplierIn, thrustMul }
        } );

        RCSInputs rcsInputs{pitch, yaw, roll};

        int const   rcsRingBlocks   = 4;
        int const   rcsRingCount    = 2;
        float const rcsRingZ        = -2.0f;
        float const rcsZStep        = 4.0f;
        float const rcsRadius       = 1.1f;
        float const rcsThrust       = 3000.0f;

        for (int ring = 0; ring < rcsRingCount; ++ring)
        {
            Vector3 const rcsOset{rcsRadius, 0.0f, rcsRingZ + ring*rcsZStep };

            for (Rad ang = 0.0_degf; ang < Rad(360.0_degf); ang += Rad(360.0_degf)/rcsRingBlocks)
            {
               Quaternion rotZ = Quaternion::rotation(ang, {0.0f, 0.0f, 1.0f});
               add_rcs_block(vbuilder, toWeld, rcsInputs, rcsThrust, rotZ.transformVector(rcsOset), rotZ);
            }
        }

        vbuilder.weld(toWeld);

        top_emplace<VehicleData>(topData, idTVPartVehicle, vbuilder.finalize_release());
    }

    auto const cleanup_prefab_owners = wrap_args([] (Resources& rResources, VehicleData &rTVData) noexcept
    {
        for (osp::PrefabPair &rPrefabPair : rTVData.m_partPrefabs)
        {
            rResources.owner_destroy(gc_importer, std::move(rPrefabPair.m_importer));
        }
    });

    testVehicles.task() = rBuilder.task().assign({tgCleanupEvt}).data(
            "Clean up test vehicle's (idTVPartVehicle) owners",
            TopDataIds_t{ idResources, idTVPartVehicle }, cleanup_prefab_owners);

    return testVehicles;
}

struct VehicleTestControls
{
    MachLocalId m_selectedUsrCtrl{lgrn::id_null<MachLocalId>()};

    input::EButtonControlIndex m_btnSwitch;
    input::EButtonControlIndex m_btnThrMax;
    input::EButtonControlIndex m_btnThrMin;
    input::EButtonControlIndex m_btnThrMore;
    input::EButtonControlIndex m_btnThrLess;
    input::EButtonControlIndex m_btnPitchUp;
    input::EButtonControlIndex m_btnPitchDn;
    input::EButtonControlIndex m_btnYawLf;
    input::EButtonControlIndex m_btnYawRt;
    input::EButtonControlIndex m_btnRollLf;
    input::EButtonControlIndex m_btnRollRt;
};

Session setup_vehicle_control(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              scnCommon,
        Session const&              parts,
        Session const&              signalsFloat,
        Session const&              app)
{
    OSP_SESSION_UNPACK_DATA(scnCommon,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(signalsFloat,   TESTAPP_SIGNALS_FLOAT)
    OSP_SESSION_UNPACK_TAGS(signalsFloat,   TESTAPP_SIGNALS_FLOAT);
    OSP_SESSION_UNPACK_DATA(parts,          TESTAPP_PARTS);
    OSP_SESSION_UNPACK_DATA(app,            TESTAPP_APP);
    OSP_SESSION_UNPACK_TAGS(app,            TESTAPP_APP);

    using namespace adera;

    Session vehicleCtrl;
    OSP_SESSION_ACQUIRE_DATA(vehicleCtrl, topData, TESTAPP_VEHICLE_CONTROL);
    OSP_SESSION_ACQUIRE_TAGS(vehicleCtrl, rTags,   TESTAPP_VEHICLE_CONTROL);

    rBuilder.tag(tgSelUsrCtrlReq).depend_on({tgSelUsrCtrlMod});

    auto &rUserInput = top_get< input::UserInputHandler >(topData, idUserInput);

    // TODO: add cleanup task
    top_emplace<VehicleTestControls>(topData, idVhControls, VehicleTestControls{
        .m_btnSwitch    = rUserInput.button_subscribe("game_switch"),
        .m_btnThrMax    = rUserInput.button_subscribe("vehicle_thr_max"),
        .m_btnThrMin    = rUserInput.button_subscribe("vehicle_thr_min"),
        .m_btnThrMore   = rUserInput.button_subscribe("vehicle_thr_more"),
        .m_btnThrLess   = rUserInput.button_subscribe("vehicle_thr_less"),
        .m_btnPitchUp   = rUserInput.button_subscribe("vehicle_pitch_up"),
        .m_btnPitchDn   = rUserInput.button_subscribe("vehicle_pitch_dn"),
        .m_btnYawLf     = rUserInput.button_subscribe("vehicle_yaw_lf"),
        .m_btnYawRt     = rUserInput.button_subscribe("vehicle_yaw_rt"),
        .m_btnRollLf    = rUserInput.button_subscribe("vehicle_roll_lf"),
        .m_btnRollRt    = rUserInput.button_subscribe("vehicle_roll_rt")
    });

    auto const idNull = lgrn::id_null<TopDataId>();

    vehicleCtrl.task() = rBuilder.task().assign({tgInputEvt, tgSigFloatUpdMod}).data(
            "Write inputs to UserControl Machines",
            TopDataIds_t{           idScnParts,                       idSigValFloat,                    idSigUpdFloat,                               idUserInput,                     idVhControls,           idDeltaTimeIn },
            wrap_args([] (ACtxParts& rScnParts, SignalValues_t<float>& rSigValFloat, UpdateNodes<float>& rSigUpdFloat, input::UserInputHandler const &rUserInput, VehicleTestControls &rVhControls, float const deltaTimeIn) noexcept
    {
        Nodes const &rFloatNodes = rScnParts.m_nodePerType[gc_ntSigFloat];
        PerMachType &rUsrCtrl    = rScnParts.m_machines.m_perType[gc_mtUserCtrl];

        // Select a UsrCtrl machine when pressing the switch button
        if (rUserInput.button_state(rVhControls.m_btnSwitch).m_triggered)
        {
            ++rVhControls.m_selectedUsrCtrl;
            bool found = false;
            for (MachLocalId local = rVhControls.m_selectedUsrCtrl; local < rUsrCtrl.m_localIds.capacity(); ++local)
            {
                if (rUsrCtrl.m_localIds.exists(local))
                {
                    found = true;
                    rVhControls.m_selectedUsrCtrl = local;
                    break;
                }
            }

            if ( ! found )
            {
                rVhControls.m_selectedUsrCtrl = lgrn::id_null<MachLocalId>();
                OSP_LOG_INFO("Unselected vehicles");
            }
            else
            {
                OSP_LOG_INFO("Selected User Control: {}", rVhControls.m_selectedUsrCtrl);
            }
        }

        if (rVhControls.m_selectedUsrCtrl == lgrn::id_null<MachLocalId>())
        {
            return; // No vehicle selected
        }

        // Control selected UsrCtrl machine

        float const thrRate = deltaTimeIn;
        float const thrChange =
                  float(rUserInput.button_state(rVhControls.m_btnThrMore).m_held) * thrRate
                - float(rUserInput.button_state(rVhControls.m_btnThrLess).m_held) * thrRate
                + float(rUserInput.button_state(rVhControls.m_btnThrMax).m_triggered)
                - float(rUserInput.button_state(rVhControls.m_btnThrMin).m_triggered);

        Vector3 const attitude
        {
              float(rUserInput.button_state(rVhControls.m_btnPitchUp).m_held)
            - float(rUserInput.button_state(rVhControls.m_btnPitchDn).m_held),
              float(rUserInput.button_state(rVhControls.m_btnYawLf).m_held)
            - float(rUserInput.button_state(rVhControls.m_btnYawRt).m_held),
              float(rUserInput.button_state(rVhControls.m_btnRollLf).m_held)
            - float(rUserInput.button_state(rVhControls.m_btnRollRt).m_held)
        };



        MachAnyId const mach = rUsrCtrl.m_localToAny[rVhControls.m_selectedUsrCtrl];
        lgrn::Span<NodeId const> const portSpan = rFloatNodes.m_machToNode[mach];

        auto const write_control = [&rSigValFloat, &rSigUpdFloat, portSpan] (PortEntry const& entry, float write, bool replace = true, float min = 0.0f, float max = 1.0f)
        {
            NodeId const node = connected_node(portSpan, entry.m_port);
            if (node == lgrn::id_null<NodeId>())
            {
                return; // not connected
            }

            float const oldVal = rSigValFloat[node];
            float const newVal = replace ? write : Magnum::Math::clamp(oldVal + write, min, max);

            if (oldVal != newVal)
            {
                rSigUpdFloat.assign(node, newVal);
            }
        };

        write_control(ports_userctrl::gc_throttleOut,   thrChange, false);
        write_control(ports_userctrl::gc_pitchOut,      attitude.x());
        write_control(ports_userctrl::gc_yawOut,        attitude.y());
        write_control(ports_userctrl::gc_rollOut,       attitude.z());

    }));

    return vehicleCtrl;
}

Session setup_camera_vehicle(
        Builder_t&                  rBuilder,
        [[maybe_unused]] ArrayView<entt::any> const topData,
        [[maybe_unused]] Tags&      rTags,
        Session const&              app,
        Session const&              scnCommon,
        Session const&              parts,
        Session const&              physics,
        Session const&              camera,
        Session const&              vehicleControl)
{
    OSP_SESSION_UNPACK_DATA(scnCommon,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(parts,          TESTAPP_PARTS);
    OSP_SESSION_UNPACK_TAGS(physics,        TESTAPP_PHYSICS);
    OSP_SESSION_UNPACK_TAGS(app,            TESTAPP_APP);
    OSP_SESSION_UNPACK_DATA(camera,         TESTAPP_CAMERA_CTRL);
    OSP_SESSION_UNPACK_TAGS(camera,         TESTAPP_CAMERA_CTRL);
    OSP_SESSION_UNPACK_DATA(vehicleControl, TESTAPP_VEHICLE_CONTROL);
    OSP_SESSION_UNPACK_TAGS(vehicleControl, TESTAPP_VEHICLE_CONTROL);

    Session cameraFree;

    cameraFree.task() = rBuilder.task().assign({tgInputEvt, tgSelUsrCtrlReq, tgPhysTransformReq, tgCamCtrlMod}).data(
            "Update vehicle camera",
            TopDataIds_t{                      idCamCtrl,           idDeltaTimeIn,                 idBasic,                     idVhControls,                 idScnParts },
            wrap_args([] (ACtxCameraController& rCamCtrl, float const deltaTimeIn, ACtxBasic const& rBasic, VehicleTestControls& rVhControls, ACtxParts const& rScnParts) noexcept
    {
        if (MachLocalId const selectedLocal = rVhControls.m_selectedUsrCtrl;
            selectedLocal != lgrn::id_null<MachLocalId>())
        {
            // Follow selected UserControl machine

            // Obtain associated ActiveEnt
            // MachLocalId -> MachAnyId -> PartId -> RigidGroup -> ActiveEnt
            PerMachType const& rUsrCtrls    = rScnParts.m_machines.m_perType.at(adera::gc_mtUserCtrl);
            MachAnyId const selectedMach    = rUsrCtrls.m_localToAny        .at(selectedLocal);
            PartId const selectedPart       = rScnParts.m_machineToPart     .at(selectedMach);
            WeldId const weld               = rScnParts.m_partToWeld        .at(selectedPart);
            ActiveEnt const selectedEnt     = rScnParts.m_weldToEnt         .at(weld);

            if (rBasic.m_transform.contains(selectedEnt))
            {
                rCamCtrl.m_target = rBasic.m_transform.get(selectedEnt).m_transform.translation();
            }
        }
        else
        {
            // Free cam when no vehicle selected
            SysCameraController::update_move(
                    rCamCtrl,
                    deltaTimeIn, true);
        }

        SysCameraController::update_view(rCamCtrl, deltaTimeIn);
    }));

    return cameraFree;
}

} // namespace testapp::scenes
