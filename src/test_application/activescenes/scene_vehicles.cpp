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
#include "scene_physics.h"
#include "CameraController.h"
#include "../VehicleBuilder.h"

#include <osp/Active/parts.h>
#include <osp/Active/physics.h>
#include <osp/Active/SysHierarchy.h>
#include <osp/Active/SysPrefabInit.h>

#include <osp/Active/opengl/SysRenderGL.h>

#include <osp/link/machines.h>
#include <osp/link/signal.h>

#include <osp/UserInputHandler.h>

#include <osp/Resource/resources.h>

#include <osp/logging.h>

#include <adera/machines/links.h>

#include <Corrade/Containers/ArrayView.h>

using namespace osp;
using namespace osp::active;
using namespace osp::link;

using osp::restypes::gc_importer;



using Corrade::Containers::arrayView;

namespace testapp::scenes
{



struct TmpPartInit
{
    PartEnt_t m_part;
    int m_initPrefabs;
};


Session setup_parts(Builder_t& rBuilder, ArrayView<entt::any> const topData, Tags& rTags, Session const& scnCommon, TopDataId const idResources)
{
    OSP_SESSION_UNPACK_DATA(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);

    Session parts;
    OSP_SESSION_ACQUIRE_DATA(parts, topData, TESTAPP_PARTS);
    OSP_SESSION_ACQUIRE_TAGS(parts, rTags,   TESTAPP_PARTS);
    parts.m_tgCleanupEvt = tgCleanupEvt;

    rBuilder.tag(tgPartInitReq)         .depend_on({tgPartInitMod});
    rBuilder.tag(tgPartInitClr)         .depend_on({tgPartInitMod, tgPartInitReq});
    rBuilder.tag(tgLinkReq)             .depend_on({tgLinkMod});
    rBuilder.tag(tgLinkMhUpdReq)        .depend_on({tgLinkMhUpdMod});

    auto &rScnParts = top_emplace< ACtxParts >      (topData, idScnParts);
    auto &rUpdMach = top_emplace< UpdMachPerType >  (topData, idUpdMach);
    //top_emplace< std::vector<TmpPartInit> >         (topData, idPartInit);
    top_emplace< MachTypeToEvt_t >                  (topData, idMachEvtTags, MachTypeReg_t::size());

    rUpdMach.m_machTypesDirty.ints().resize(MachTypeReg_t::size() + 1);
    rUpdMach.m_localDirty.resize(MachTypeReg_t::size());
    rScnParts.m_machines.m_perType.resize(MachTypeReg_t::size());
    rScnParts.m_nodePerType.resize(NodeTypeReg_t::size());

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



    return parts;
}

Session setup_signals_float(Builder_t& rBuilder, ArrayView<entt::any> const topData, Tags& rTags, Session const& scnCommon, Session const& parts)
{
    OSP_SESSION_UNPACK_DATA(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(parts,      TESTAPP_PARTS);
    OSP_SESSION_UNPACK_TAGS(parts,      TESTAPP_PARTS);

    Session signalsFloat;
    OSP_SESSION_ACQUIRE_DATA(signalsFloat, topData, TESTAPP_SIGNALS_FLOAT);
    OSP_SESSION_ACQUIRE_TAGS(signalsFloat, rTags,   TESTAPP_SIGNALS_FLOAT);

    rBuilder.tag(tgSigFloatLinkReq)         .depend_on({tgSigFloatLinkMod});
    rBuilder.tag(tgSigFloatValReq)          .depend_on({tgSigFloatLinkMod, tgSigFloatValMod});
    rBuilder.tag(tgSigFloatUpdReq)          .depend_on({tgSigFloatLinkMod, tgSigFloatUpdMod});

    top_emplace< SignalValues_t<float> >    (topData, idSigValFloat);
    top_emplace< UpdateNodes<float> >       (topData, idSigUpdFloat);
    top_emplace< TagId >                    (topData, idTgSigFloatUpdEvt, tgSigFloatUpdEvt);

    // note: Eventually have an array of UpdateNodes to allow multiple threads
    //       to update nodes in parallel. Tag limits would be handy here

    auto const idNull = lgrn::id_null<TopDataId>();

    signalsFloat.task() = rBuilder.task().assign({tgSigFloatUpdEvt, tgSigFloatValMod, tgSigFloatUpdReq}).data(
            "Update Signal-Float Nodes",
            TopDataIds_t{            idNull,                    idSigUpdFloat,                       idSigValFloat,                 idScnParts,                idUpdMach,                       idMachEvtTags  },
            wrap_args([] (WorkerContext ctx, UpdateNodes<float>& rSigUpdFloat, SignalValues_t<float>& rSigValFloat, ACtxParts const& rScnParts, UpdMachPerType& rUpdMach, MachTypeToEvt_t const& rMachEvtTags) noexcept
    {
        Nodes const &rFloatNodes = rScnParts.m_nodePerType[gc_ntSigFloat];

        rUpdMach.m_machTypesDirty.reset();
        update_signal_nodes<float>(
                rSigUpdFloat.m_nodeDirty.ones(),
                rFloatNodes.m_nodeToMach,
                rScnParts.m_machines,
                arrayView(rSigUpdFloat.m_nodeNewValues),
                rSigValFloat,
                rUpdMach);

        for (MachTypeId const type : rUpdMach.m_machTypesDirty.ones())
        {
            ctx.m_rEnqueueHappened = true;
            TagId const tag = rMachEvtTags[type];
            lgrn::bit_view(ctx.m_enqueue).set(std::size_t(tag));
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

Session setup_mach_rocket(Builder_t& rBuilder, ArrayView<entt::any> const topData, Tags& rTags, Session const& scnCommon, Session const& parts, Session const& signalsFloat)
{
    OSP_SESSION_UNPACK_DATA(scnCommon,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(scnCommon,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(signalsFloat,   TESTAPP_SIGNALS_FLOAT)
    OSP_SESSION_UNPACK_TAGS(signalsFloat,   TESTAPP_SIGNALS_FLOAT);
    OSP_SESSION_UNPACK_DATA(parts,          TESTAPP_PARTS);
    OSP_SESSION_UNPACK_TAGS(parts,          TESTAPP_PARTS);

    using namespace adera;

    Session machRocket;
    OSP_SESSION_ACQUIRE_DATA(machRocket, topData, TESTAPP_MACH_ROCKET);
    OSP_SESSION_ACQUIRE_TAGS(machRocket, rTags,   TESTAPP_MACH_ROCKET);

    top_get< MachTypeToEvt_t >(topData, idMachEvtTags).at(gc_mtMagicRocket) = tgMhRocketEvt;

    // TODO: This session is not needed? Architecture prefers fine grained
    //       individual sessions: thrust, plume FX, sounds, etc...

    machRocket.task() = rBuilder.task().assign({tgMhRocketEvt, tgSigFloatUpdReq, tgSigFloatValReq}).data(
            "MagicRockets print throttle input values",
            TopDataIds_t{           idScnParts,                      idUpdMach,                       idSigValFloat },
            wrap_args([] (ACtxParts& rScnParts, UpdMachPerType const& rUpdMach, SignalValues_t<float>& rSigValFloat) noexcept
    {
        Nodes const &rFloatNodes = rScnParts.m_nodePerType[gc_ntSigFloat];
        PerMachType &rRockets = rScnParts.m_machines.m_perType[gc_mtMagicRocket];

        for (MachLocalId const local : rUpdMach.m_localDirty[gc_mtMagicRocket].ones())
        {
            MachAnyId const mach = rRockets.m_localToAny[local];
            lgrn::Span<NodeId const> const portSpan = rFloatNodes.m_machToNode[mach];
            if (NodeId const thrNode = connected_node(portSpan, ports_magicrocket::gc_throttleIn.m_port);
                thrNode != lgrn::id_null<NodeId>())
            {
                if (thrNode != lgrn::id_null<NodeId>())
                {
                    float const thrNew = rSigValFloat[thrNode];
                    OSP_LOG_INFO("Rocket {} reading node {} = {}", local, thrNode, thrNew);
                }
            }
        }
    }));

    // TODO: Make this a template. All machines that read inputs will just have
    //       a different gc_mtMachineName variable
    machRocket.task() = rBuilder.task().assign({tgSceneEvt, tgLinkReq, tgLinkMhUpdMod}).data(
            "Allocate machine update stuff",
            TopDataIds_t{           idScnParts,                idUpdMach },
            wrap_args([] (ACtxParts& rScnParts, UpdMachPerType& rUpdMach) noexcept
    {
        rUpdMach.m_localDirty[gc_mtMagicRocket].ints().resize(rScnParts.m_machines.m_perType[gc_mtMagicRocket].m_localIds.vec().capacity());
    }));

    return machRocket;
}

Session setup_vehicle_spawn(Builder_t& rBuilder, ArrayView<entt::any> const topData, Tags& rTags, Session const& scnCommon, Session const& parts)
{
    OSP_SESSION_UNPACK_DATA(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(parts,      TESTAPP_PARTS);
    OSP_SESSION_UNPACK_TAGS(parts,      TESTAPP_PARTS);

    Session vehicleSpawn;
    OSP_SESSION_ACQUIRE_DATA(vehicleSpawn, topData, TESTAPP_VEHICLE_SPAWN);
    OSP_SESSION_ACQUIRE_TAGS(vehicleSpawn, rTags,   TESTAPP_VEHICLE_SPAWN);

    rBuilder.tag(tgVehicleSpawnReq)     .depend_on({tgVehicleSpawnMod});
    rBuilder.tag(tgVehicleSpawnClr)     .depend_on({tgVehicleSpawnMod, tgVehicleSpawnReq});
    rBuilder.tag(tgVSpawnRgdReq)        .depend_on({tgVSpawnRgdMod});
    rBuilder.tag(tgVSpawnRgdEntReq)     .depend_on({tgVSpawnRgdEntMod});
    rBuilder.tag(tgVSpawnPartReq)       .depend_on({tgVSpawnPartMod});

    top_emplace< ACtxVehicleSpawn >     (topData, idVehicleSpawn);

    vehicleSpawn.task() = rBuilder.task().assign({tgSceneEvt, tgVehicleSpawnClr}).data(
            "Clear Vehicle Spawning vector after use",
            TopDataIds_t{                  idVehicleSpawn},
            wrap_args([] (ACtxVehicleSpawn& rVehicleSpawn) noexcept
    {
        rVehicleSpawn.m_basic.clear();
    }));


    return vehicleSpawn;
}

Session setup_vehicle_spawn_vb(Builder_t& rBuilder, ArrayView<entt::any> const topData, Tags& rTags, Session const& scnCommon, Session const& prefabs, Session const& parts, Session const& vehicleSpawn, TopDataId const idResources)
{
    OSP_SESSION_UNPACK_DATA(scnCommon,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(scnCommon,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(prefabs,        TESTAPP_PREFABS);
    OSP_SESSION_UNPACK_TAGS(prefabs,        TESTAPP_PREFABS);
    OSP_SESSION_UNPACK_DATA(vehicleSpawn,   TESTAPP_VEHICLE_SPAWN);
    OSP_SESSION_UNPACK_TAGS(vehicleSpawn,   TESTAPP_VEHICLE_SPAWN);
    OSP_SESSION_UNPACK_DATA(parts,          TESTAPP_PARTS);
    OSP_SESSION_UNPACK_TAGS(parts,          TESTAPP_PARTS);

    Session vehicleSpawnVB;
    OSP_SESSION_ACQUIRE_DATA(vehicleSpawnVB, topData, TESTAPP_VEHICLE_SPAWN_VB);

    top_emplace< ACtxVehicleSpawnVB >(topData, idVehicleSpawnVB);

    vehicleSpawnVB.task() = rBuilder.task().assign({tgSceneEvt, tgVehicleSpawnReq, tgVSpawnPartMod}).data(
            "Create parts for vehicle",
            TopDataIds_t{                  idVehicleSpawn,                    idVehicleSpawnVB,           idScnParts},
            wrap_args([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB& rVehicleSpawnVB, ACtxParts& rScnParts) noexcept
    {
        if (rVehicleSpawn.vehicle_count() == 0)
        {
            return;
        }

        rVehicleSpawnVB.m_remapPartOffsets.resize(rVehicleSpawn.vehicle_count());

        // Count parts and calculate offsets Part ID remap vector

        std::size_t partCount = 0;
        std::size_t offsetTotal = 0;

        {
            auto itRemapPartOffset = std::begin(rVehicleSpawnVB.m_remapPartOffsets);

            for (VehicleData const* pVData : rVehicleSpawnVB.m_dataVB)
            {
                // TODO: eventually use "highest ID + 1" for bounds
                // Capacity is the bit size of the internal uint64 vector: a
                // number greater than all IDs and is a multiple of 64.
                std::size_t const bounds = pVData->m_partIds.capacity();
                (*itRemapPartOffset) = offsetTotal;
                offsetTotal += bounds;

                // Exact number of part IDs, size() counts the ones bits
                partCount += pVData->m_partIds.size();

                std::advance(itRemapPartOffset, 1);
            }

            rVehicleSpawnVB.m_remapPartAll.resize(offsetTotal, lgrn::id_null<PartEnt_t>());
            rVehicleSpawn.m_partEnts.resize(partCount);
            rVehicleSpawn.m_partPrefabs.resize(partCount);
        }

        // Create the part IDs
        // Populates rVehicleSpawn.m_partEnts
        rScnParts.m_partIds.create(std::begin(rVehicleSpawn.m_partEnts),
                                     std::end(rVehicleSpawn.m_partEnts));

        // Resize some containers to account for newly created part IDs
        rScnParts.m_partPrefabs         .resize(rScnParts.m_partIds.capacity());
        rScnParts.m_partTransformRigid  .resize(rScnParts.m_partIds.capacity());
        rScnParts.m_partRigids          .resize(rScnParts.m_partIds.capacity());
        rScnParts.m_rigidToParts  .data_reserve(rScnParts.m_partIds.capacity());

        // Assign the part IDs to each vehicle to initialize
        // Populates rVehicleSpawn.m_offsetParts
        {
            rVehicleSpawn.m_offsetParts.resize(rVehicleSpawn.m_basic.size());
            auto itOffsetPartsOut         = std::begin(rVehicleSpawn.m_offsetParts);
            auto const itPartEntsFirst    = std::cbegin(rVehicleSpawn.m_partEnts);
            auto itPartEnts               = std::cbegin(rVehicleSpawn.m_partEnts);
            for (VehicleData const* pVData : rVehicleSpawnVB.m_dataVB)
            {
                std::size_t const count = pVData->m_partIds.size();

                (*itOffsetPartsOut) = std::distance(itPartEntsFirst, itPartEnts);

                std::advance(itPartEnts, count);
                std::advance(itOffsetPartsOut, 1);
            }
            // All parts should always be used up
            assert(itPartEnts == std::end(rVehicleSpawn.m_partEnts));
        }

        // Generate "VehicleData Part -> Scene Part" remap vector
        // Populates rVehicleSpawn.m_remapPartOffsets
        {
            auto itRemapPartOffset  = std::cbegin(rVehicleSpawnVB.m_remapPartOffsets);
            auto itDstPartEnts      = std::cbegin(rVehicleSpawn.m_partEnts);

            for (VehicleData const* pVData : rVehicleSpawnVB.m_dataVB)
            {
                std::size_t const remapOffset = *itRemapPartOffset;

                for (uint32_t srcPart : pVData->m_partIds.bitview().zeros())
                {
                    rVehicleSpawnVB.m_remapPartAll[remapOffset + srcPart] = *itDstPartEnts;
                    std::advance(itDstPartEnts, 1);
                }

                std::advance(itRemapPartOffset, 1);
            }
        }

    }));

    vehicleSpawnVB.task() = rBuilder.task().assign({tgSceneEvt, tgVehicleSpawnReq, tgVSpawnRgdReq, tgVSpawnRgdEntReq, tgVSpawnPartReq, tgPrefabMod}).data(
            "Load vehicle parts from VehicleBuilder",
            TopDataIds_t{                  idVehicleSpawn,                          idVehicleSpawnVB,           idScnParts,                idPrefabInit,           idResources},
            wrap_args([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB const& rVehicleSpawnVB, ACtxParts& rScnParts, ACtxPrefabInit& rPrefabInit, Resources& rResources) noexcept
    {
        if (rVehicleSpawn.vehicle_count() == 0)
        {
            return;
        }

        auto itDstPartEnts = std::cbegin(rVehicleSpawn.m_partEnts);
        auto itPrefabOut   = std::begin(rVehicleSpawn.m_partPrefabs);

        for (VehicleData const* pVData : rVehicleSpawnVB.m_dataVB)
        {
            // Copy Part data from VehicleBuilder to scene
            for (uint32_t srcPart : pVData->m_partIds.bitview().zeros())
            {
                PartEnt_t const dstPart = *itDstPartEnts;
                std::advance(itDstPartEnts, 1);

                PrefabPair const& prefabPairSrc = pVData->m_partPrefabs[srcPart];
                PrefabPair prefabPairDst{
                    rResources.owner_create(gc_importer, prefabPairSrc.m_importer),
                    prefabPairSrc.m_prefabId
                };
                rScnParts.m_partPrefabs[dstPart]        = std::move(prefabPairDst);
                rScnParts.m_partTransformRigid[dstPart] = pVData->m_partTransforms[srcPart];

                RigidGroup_t const rigid = rScnParts.m_partRigids[dstPart];
                ActiveEnt const rigidEnt = rScnParts.m_rigidToEnt[rigid];

                // Add Prefab and Part init events
                (*itPrefabOut) = rPrefabInit.m_basic.size();
                std::advance(itPrefabOut, 1);

                rPrefabInit.m_basic.push_back(TmpPrefabInitBasic{
                    .m_importerRes = prefabPairSrc.m_importer,
                    .m_prefabId = prefabPairSrc.m_prefabId,
                    .m_parent = rigidEnt,
                    .m_pTransform = &pVData->m_partTransforms[srcPart]
                });
            }
        }

    }));

    vehicleSpawnVB.task() = rBuilder.task().assign({tgSceneEvt, tgVSpawnRgdEntReq, tgVSpawnPartReq, tgLinkMod}).data(
            "Copy machines from VehicleBuilder",
            TopDataIds_t{                  idVehicleSpawn,                    idVehicleSpawnVB,           idScnParts,                idPrefabInit,           idResources},
            wrap_args([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB rVehicleSpawnVB, ACtxParts& rScnParts, ACtxPrefabInit& rPrefabInit, Resources& rResources) noexcept
    {
        if (rVehicleSpawn.vehicle_count() == 0)
        {
            return;
        }

        // Count total number of machines to make
        {
            std::size_t machCount = 0;
            std::size_t offsetTotal = 0;

            rVehicleSpawnVB.m_remapMachOffsets.resize(rVehicleSpawn.vehicle_count());
            auto itRemapMachOffset = std::begin(rVehicleSpawnVB.m_remapMachOffsets);

            for (VehicleData const* pVData : rVehicleSpawnVB.m_dataVB)
            {
                if (pVData == nullptr)
                {
                    continue;
                }

                std::size_t const bounds = pVData->m_machines.m_ids.capacity();
                (*itRemapMachOffset) = offsetTotal;
                offsetTotal += bounds;
                machCount += pVData->m_machines.m_ids.size();

                std::advance(itRemapMachOffset, 1);
            }

        }


        auto itRemapPartOffset = std::begin(rVehicleSpawnVB.m_remapPartOffsets);

        for (VehicleData const* pVData : rVehicleSpawnVB.m_dataVB)
        {
            if (pVData == nullptr)
            {
                continue;
            }

            auto remapPart = arrayView(rVehicleSpawnVB.m_remapPartAll).exceptPrefix(*itRemapPartOffset);

            // remapMach[source MachAnyId] -> destination MachAnyId
            std::vector<MachAnyId> remapMach(pVData->m_machines.m_ids.capacity());

            copy_machines(pVData->m_machines, rScnParts.m_machines, remapMach);

            rScnParts.m_machineToPart.resize(rScnParts.m_machines.m_ids.capacity());
            for (MachAnyId const srcMach : pVData->m_machines.m_ids.bitview().zeros())
            {
                MachAnyId const dstMach = remapMach[srcMach];
                rScnParts.m_machineToPart[dstMach] = remapPart[std::size_t(pVData->m_machToPart[srcMach])];
            }

            // Do the same for nodes
            auto itDstNodeType = std::begin(rScnParts.m_nodePerType);
            for (PerNodeType const &rSrcNodeType : pVData->m_nodePerType)
            {
                std::vector<NodeId> remapNodes(rSrcNodeType.m_nodeIds.capacity());
                copy_nodes(rSrcNodeType, pVData->m_machines, remapMach,
                           *itDstNodeType, rScnParts.m_machines, remapNodes);
                std::advance(itDstNodeType, 1);
            }

            // TODO: copy node values

            std::advance(itRemapPartOffset, 1);
        }

    }));


    vehicleSpawnVB.task() = rBuilder.task().assign({tgSceneEvt, tgVehicleSpawnReq, tgPrefabEntReq}).data(
            "Update PartEnt<->ActiveEnt mapping",
            TopDataIds_t{                  idVehicleSpawn,           idScnParts,                   idActiveIds,                 idPrefabInit },
            wrap_args([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxParts& rScnParts, ActiveReg_t const& rActiveIds,  ACtxPrefabInit& rPrefabInit) noexcept
    {
        rScnParts.m_partToActive.resize(rScnParts.m_partIds.capacity());
        rScnParts.m_activeToPart.resize(rActiveIds.capacity());

        // Populate PartEnt<->ActiveEnt mmapping, now that the prefabs exist

        auto itPrefab = std::begin(rVehicleSpawn.m_partPrefabs);

        for (PartEnt_t const partEnt : rVehicleSpawn.m_partEnts)
        {
            ActiveEnt const root = rPrefabInit.m_ents[*itPrefab].front();

            rScnParts.m_partToActive[partEnt]           = root;
            rScnParts.m_activeToPart[std::size_t(root)] = partEnt;

            std::advance(itPrefab, 1);
        }

    }));

    return vehicleSpawnVB;
}

Session setup_vehicle_spawn_rigid(Builder_t& rBuilder, ArrayView<entt::any> const topData, Tags& rTags, Session const& scnCommon, Session const& physics, Session const& prefabs, Session const& parts, Session const& vehicleSpawn)
{
    OSP_SESSION_UNPACK_DATA(scnCommon,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(scnCommon,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(physics,        TESTAPP_PHYSICS);
    OSP_SESSION_UNPACK_TAGS(physics,        TESTAPP_PHYSICS);
    OSP_SESSION_UNPACK_DATA(prefabs,        TESTAPP_PREFABS);
    OSP_SESSION_UNPACK_TAGS(prefabs,        TESTAPP_PREFABS);
    OSP_SESSION_UNPACK_DATA(vehicleSpawn,   TESTAPP_VEHICLE_SPAWN);
    OSP_SESSION_UNPACK_TAGS(vehicleSpawn,   TESTAPP_VEHICLE_SPAWN);
    OSP_SESSION_UNPACK_DATA(parts,          TESTAPP_PARTS);
    OSP_SESSION_UNPACK_TAGS(parts,          TESTAPP_PARTS);

    Session vehicleSpawnRgd;
    OSP_SESSION_ACQUIRE_DATA(vehicleSpawnRgd, topData, TESTAPP_VEHICLE_SPAWN_RIGID);

    top_emplace< ACtxVehicleSpawnRigid >(topData, idVehicleSpawnRgd);

    vehicleSpawnRgd.task() = rBuilder.task().assign({tgSceneEvt, tgVehicleSpawnReq, tgVSpawnRgdMod}).data(
            "Create rigid groups for vehicles to spawn",
            TopDataIds_t{                  idVehicleSpawn,                       idVehicleSpawnRgd,           idScnParts},
            wrap_args([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnRigid& rVehicleSpawnRgd, ACtxParts& rScnParts) noexcept
    {
        if (rVehicleSpawn.vehicle_count() == 0)
        {
            return;
        }

        // Create rigid group for each vehicle to spawn
        rVehicleSpawnRgd.m_rigidGroups.resize(rVehicleSpawn.vehicle_count());
        rScnParts.m_rigidIds.create(std::begin(rVehicleSpawnRgd.m_rigidGroups), std::end(rVehicleSpawnRgd.m_rigidGroups));

        // Newly created rigid groups must be put in the dirty vector
        rScnParts.m_rigidDirty.insert(std::end(rScnParts.m_rigidDirty), std::begin(rVehicleSpawnRgd.m_rigidGroups), std::end(rVehicleSpawnRgd.m_rigidGroups));

        // Resize vectors to account for newly created rigid groups
        rScnParts.m_rigidToEnt  .resize(        rScnParts.m_rigidIds.capacity());
        rScnParts.m_rigidToParts.ids_reserve(   rScnParts.m_rigidIds.capacity());
    }));


    vehicleSpawnRgd.task() = rBuilder.task().assign({tgSceneEvt, tgVehicleSpawnReq, tgVSpawnRgdReq}).data(
            "Add vehicle parts to rigid groups",
            TopDataIds_t{                  idVehicleSpawn,                       idVehicleSpawnRgd,           idScnParts  },
            wrap_args([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnRigid& rVehicleSpawnRgd, ACtxParts& rScnParts) noexcept
    {
        if (rVehicleSpawn.vehicle_count() == 0)
        {
            return;
        }

        auto itRgdGroup    = std::begin(rVehicleSpawnRgd.m_rigidGroups);
        auto itOffsetParts = std::cbegin(rVehicleSpawn.m_offsetParts);
        for (ACtxVehicleSpawn::TmpToInit const& toInit : rVehicleSpawn.m_basic)
        {
            // Add parts of each vehicle to its corresponding rigid group
            auto vehicleParts = arrayView(rVehicleSpawn.m_partEnts).exceptPrefix(*itOffsetParts);
            rScnParts.m_rigidToParts.emplace(*itRgdGroup, std::begin(vehicleParts), std::end(vehicleParts));
            std::ranges::for_each(vehicleParts, [&rScnParts, rigid = *itRgdGroup] (PartEnt_t const part)
            {
                rScnParts.m_partRigids[part] = rigid;
            });
            std::advance(itRgdGroup, 1);
            std::advance(itOffsetParts, 1);
        }
    }));

    vehicleSpawnRgd.task() = rBuilder.task().assign({tgSceneEvt, tgEntNew, tgVehicleSpawnReq, tgVSpawnRgdReq, tgVSpawnRgdEntMod}).data(
            "Create entity for each rigid group",
            TopDataIds_t{             idActiveIds,                  idVehicleSpawn,                       idVehicleSpawnRgd,          idScnParts },
            wrap_args([] (ActiveReg_t& rActiveIds, ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnRigid& rVehicleSpawnRgd, ACtxParts& rScnParts) noexcept
    {
        if (rVehicleSpawn.vehicle_count() == 0)
        {
            return;
        }

        rVehicleSpawnRgd.m_rigidGroupEnt.resize(rVehicleSpawn.vehicle_count());
        rActiveIds.create(std::begin(rVehicleSpawnRgd.m_rigidGroupEnt), std::end(rVehicleSpawnRgd.m_rigidGroupEnt));

        // update RigidEnt->ActiveEnt mapping
        auto itRigidEnt = std::begin(rVehicleSpawnRgd.m_rigidGroupEnt);
        for (RigidGroup_t const rigid : rVehicleSpawnRgd.m_rigidGroups)
        {
            rScnParts.m_rigidToEnt[rigid] = *itRigidEnt;
            std::advance(itRigidEnt, 1);
        }
    }));

    vehicleSpawnRgd.task() = rBuilder.task().assign({tgSceneEvt, tgVehicleSpawnReq, tgVSpawnRgdEntReq, tgPfParentHierMod}).data(
            "Add hierarchy to rigid group entities",
            TopDataIds_t{           idBasic,                        idVehicleSpawn,                             idVehicleSpawnRgd},
            wrap_args([] (ACtxBasic& rBasic, ACtxVehicleSpawn const& rVehicleSpawn, ACtxVehicleSpawnRigid const& rVehicleSpawnRgd) noexcept
    {
        auto itRigidEnt = std::begin(rVehicleSpawnRgd.m_rigidGroupEnt);
        for (ACtxVehicleSpawn::TmpToInit const& toInit : rVehicleSpawn.m_basic)
        {
            ActiveEnt const rigidEnt = *itRigidEnt;
            SysHierarchy::add_child(rBasic.m_hierarchy, rBasic.m_hierRoot, rigidEnt);
            std::advance(itRigidEnt, 1);
        }
    }));

    vehicleSpawnRgd.task() = rBuilder.task().assign({tgSceneEvt, tgVehicleSpawnReq, tgVSpawnRgdEntReq}).data(
            "Add physics to rigid group entities",
            TopDataIds_t{           idBasic,              idTPhys,                        idVehicleSpawn,                             idVehicleSpawnRgd,                 idScnParts  },
            wrap_args([] (ACtxBasic& rBasic, ACtxTestPhys& rTPhys, ACtxVehicleSpawn const& rVehicleSpawn, ACtxVehicleSpawnRigid const& rVehicleSpawnRgd, ACtxParts const& rScnParts) noexcept
    {
        if (rVehicleSpawn.vehicle_count() == 0)
        {
            return;
        }

        auto itRigidEnt = std::begin(rVehicleSpawnRgd.m_rigidGroupEnt);

        for (ACtxVehicleSpawn::TmpToInit const& toInit : rVehicleSpawn.m_basic)
        {
            ActiveEnt const rigidEnt = *itRigidEnt;
            std::advance(itRigidEnt, 1);

            auto const transform = Matrix4::from(toInit.m_rotation.toMatrix(), toInit.m_position);
            rBasic.m_transform                  .emplace(rigidEnt, transform);
            rTPhys.m_physics.m_hasColliders     .emplace(rigidEnt);
            rTPhys.m_physics.m_solid            .emplace(rigidEnt);
            rTPhys.m_physics.m_physBody         .emplace(rigidEnt);
            rTPhys.m_physics.m_physLinearVel    .emplace(rigidEnt);
            rTPhys.m_physics.m_physAngularVel   .emplace(rigidEnt);
            rTPhys.m_physics.m_physDynamic      .emplace(rigidEnt, ACompPhysDynamic
            {
                .m_centerOfMassOffset = {0.0f, 0.0f, 0.0f},
                .m_inertia = {1.0f, 1.0f, 1.0f},
                .m_totalMass = 1.0f,
            });
        }
    }));

    return vehicleSpawnRgd;
}

Session setup_test_vehicles(Builder_t& rBuilder, ArrayView<entt::any> const topData, Tags& rTags, Session const& scnCommon, TopDataId const idResources)
{
    OSP_SESSION_UNPACK_DATA(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);

    Session testVehicles;
    OSP_SESSION_ACQUIRE_DATA(testVehicles, topData, TESTAPP_TEST_VEHICLES);
    testVehicles.m_tgCleanupEvt = tgCleanupEvt;

    auto &rResources = top_get<Resources>(topData, idResources);

    using namespace adera;

    VehicleBuilder builder{&rResources};
    {
        auto const [ capsule, fueltank, engine ] = builder.create_parts<3>();
        builder.set_prefabs({
            { capsule,  "phCapsule" },
            { fueltank, "phFuselage" },
            { engine,   "phEngine" },
        });

        builder.set_transform({
           { capsule,  Matrix4::translation({0, 0, 3}) },
           { fueltank, Matrix4::translation({0, 0, 0}) },
           { engine,   Matrix4::translation({0, 0, -3}) },
        });


        namespace ports_magicrocket = adera::ports_magicrocket;
        namespace ports_userctrl = adera::ports_userctrl;

        auto const [ pitch, yaw, roll, throttle ] = builder.create_nodes<4>(gc_ntSigFloat);

        builder.create_machine(capsule, gc_mtUserCtrl, {
            { ports_userctrl::gc_throttleOut,   throttle },
            { ports_userctrl::gc_pitchOut,      pitch },
            { ports_userctrl::gc_yawOut,        yaw },
            { ports_userctrl::gc_rollOut,       roll }
        } );

        builder.create_machine(engine, gc_mtMagicRocket, {
            { ports_magicrocket::gc_throttleIn, throttle }
        } );

        top_emplace<VehicleData>(topData, idTVPartVehicle, builder.finalize_release());
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
};

Session setup_vehicle_control(Builder_t& rBuilder, ArrayView<entt::any> const topData, Tags& rTags, Session const& scnCommon, Session const& parts, Session const& signalsFloat, Session const& app)
{
    OSP_SESSION_UNPACK_DATA(scnCommon,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(signalsFloat,   TESTAPP_SIGNALS_FLOAT)
    OSP_SESSION_UNPACK_TAGS(signalsFloat,   TESTAPP_SIGNALS_FLOAT);
    OSP_SESSION_UNPACK_DATA(parts,          TESTAPP_PARTS);
    OSP_SESSION_UNPACK_TAGS(parts,          TESTAPP_PARTS);
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
        .m_btnThrLess   = rUserInput.button_subscribe("vehicle_thr_less")
    });

    auto const idNull = lgrn::id_null<TopDataId>();

    vehicleCtrl.task() = rBuilder.task().assign({tgInputEvt, tgSigFloatValReq, tgSigFloatUpdMod}).data(
            "Write inputs to UserControl Machines",
            TopDataIds_t{             idNull,           idScnParts,                       idSigValFloat,                    idSigUpdFloat,           idTgSigFloatUpdEvt,                               idUserInput,                     idVhControls,           idDeltaTimeIn },
            wrap_args([] (WorkerContext ctx,  ACtxParts& rScnParts, SignalValues_t<float>& rSigValFloat, UpdateNodes<float>& rSigUpdFloat, TagId const tgSigFloatUpdEvt, input::UserInputHandler const &rUserInput, VehicleTestControls &rVhControls, float const deltaTimeIn) noexcept
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

        // Control selected UsrCtrl machine
        if (rVhControls.m_selectedUsrCtrl != lgrn::id_null<MachLocalId>())
        {
            float const thrRate = deltaTimeIn;
            float const thrChange =
                      float(rUserInput.button_state(rVhControls.m_btnThrMore).m_held) * thrRate
                    - float(rUserInput.button_state(rVhControls.m_btnThrLess).m_held) * thrRate
                    + float(rUserInput.button_state(rVhControls.m_btnThrMax).m_triggered)
                    - float(rUserInput.button_state(rVhControls.m_btnThrMin).m_triggered);

            MachAnyId const mach = rUsrCtrl.m_localToAny[rVhControls.m_selectedUsrCtrl];
            lgrn::Span<NodeId const> const portSpan = rFloatNodes.m_machToNode[mach];

            if (NodeId const thrNode = connected_node(portSpan, adera::ports_userctrl::gc_throttleOut.m_port);
                thrNode != lgrn::id_null<NodeId>())
            {
                float const thrCurr = rSigValFloat[thrNode];
                float const thrNew = Magnum::Math::clamp(thrCurr + thrChange, 0.0f, 1.0f);

                if (thrCurr != thrNew)
                {
                    rSigUpdFloat.m_nodeDirty.set(thrNode);
                    rSigUpdFloat.m_nodeNewValues[thrNode] = thrNew;

                    ctx.m_rEnqueueHappened = true;
                    lgrn::bit_view(ctx.m_enqueue).set(std::size_t(tgSigFloatUpdEvt));
                }
            }
        }

    }));

    return vehicleCtrl;
}

Session setup_camera_vehicle(Builder_t& rBuilder, ArrayView<entt::any> const topData, Tags& rTags, Session const& app, Session const& scnCommon, Session const& parts, Session const& physics, Session const& camera, Session const& vehicleControl)
{
    OSP_SESSION_UNPACK_DATA(scnCommon,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(scnCommon,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(parts,          TESTAPP_PARTS);
    OSP_SESSION_UNPACK_TAGS(parts,          TESTAPP_PARTS);
    OSP_SESSION_UNPACK_TAGS(physics,        TESTAPP_PHYSICS);
    OSP_SESSION_UNPACK_TAGS(app,            TESTAPP_APP);
    OSP_SESSION_UNPACK_DATA(camera,         TESTAPP_CAMERA_CTRL);
    OSP_SESSION_UNPACK_TAGS(camera,         TESTAPP_CAMERA_CTRL);
    OSP_SESSION_UNPACK_DATA(vehicleControl, TESTAPP_VEHICLE_CONTROL);
    OSP_SESSION_UNPACK_TAGS(vehicleControl, TESTAPP_VEHICLE_CONTROL);

    Session cameraFree;

    cameraFree.task() = rBuilder.task().assign({tgInputEvt, tgSelUsrCtrlReq, tgPhysReq, tgCamCtrlMod}).data(
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
            PerMachType const& rUsrCtrls = rScnParts.m_machines.m_perType.at(adera::gc_mtUserCtrl);
            MachAnyId const selectedMach = rUsrCtrls.m_localToAny.at(selectedLocal);
            PartEnt_t const selectedPart = rScnParts.m_machineToPart.at(selectedMach);
            RigidGroup_t const rigid     = rScnParts.m_partRigids.at(selectedPart);
            ActiveEnt const selectedEnt  = rScnParts.m_rigidToEnt.at(rigid);

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
