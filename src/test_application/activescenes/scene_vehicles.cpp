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
#include "scenarios.h"
#include "identifiers.h"
#include "scene_physics.h"
#include "../VehicleBuilder.h"

#include <osp/Active/parts.h>
#include <osp/Active/physics.h>
#include <osp/Active/SysHierarchy.h>
#include <osp/Active/SysPrefabInit.h>

#include <osp/link/machines.h>
#include <osp/link/signal.h>

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

    auto &rScnParts = top_emplace< ACtxParts >  (topData, idScnParts);
    top_emplace< UpdMachPerType_t >             (topData, idUpdMachines);
    top_emplace< std::vector<TmpPartInit> >     (topData, idPartInit);

    rScnParts.m_machines.m_perType.resize(osp::link::MachTypeReg_t::size());
    rScnParts.m_nodePerType.resize(osp::link::NodeTypeReg_t::size());

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

    Session signalsFloat;
    OSP_SESSION_ACQUIRE_DATA(signalsFloat, topData, TESTAPP_SIGNALS_FLOAT);

    top_emplace< SignalValues_t<float> >    (topData, idSigValFloat);
    top_emplace< UpdateNodes<float> >       (topData, idSigUpdFloat);

    // note: Eventually have an array of UpdateNodes to allow multiple threads
    //       to update nodes in parallel. Tag limits would be handy here

    signalsFloat.task() = rBuilder.task().assign({}).data(
            "Update Signal-Float Nodes",
            TopDataIds_t{                    },
            wrap_args([] (UpdateNodes<float>& rSigUpdFloat, SignalValues_t<float>& rSigValFloat, ACtxParts const& rScnParts, UpdMachPerType_t& rUpdMachines) noexcept
    {
        Nodes const &rFloatNodes = rScnParts.m_nodePerType[osp::link::gc_ntSigFloat];

        update_signal_nodes<float>(
                rSigUpdFloat.m_nodeDirty.ones(),
                rFloatNodes.m_nodeToMach,
                rScnParts.m_machines,
                arrayView(rSigUpdFloat.m_nodeNewValues),
                rSigValFloat,
                rUpdMachines);
    }));

    signalsFloat.task() = rBuilder.task().assign({}).data(
            "Update PartEnt<->ActiveEnt mapping",
            TopDataIds_t{  },
            wrap_args([] (ACtxParts& rScnParts, ActiveReg_t const& rActiveIds, std::vector<TmpPartInit>& rPartInit, ACtxPrefabInit& rPrefabInit) noexcept
    {
        // Populate PartEnt<->ActiveEnt mmapping, now that the prefabs exist
        rScnParts.m_partToActive.resize(rScnParts.m_partIds.capacity());
        rScnParts.m_activeToPart.resize(rActiveIds.capacity());
        for (TmpPartInit& rPartInit : rPartInit)
        {
            ActiveEnt const root = rPrefabInit.m_ents[rPartInit.m_initPrefabs].front();

            rScnParts.m_partToActive[rPartInit.m_part] = root;
            rScnParts.m_activeToPart[std::size_t(root)] = rPartInit.m_part;
        }

    }));

    return signalsFloat;
}

Session setup_mach_magic_rocket(Builder_t& rBuilder, ArrayView<entt::any> const topData, Tags& rTags, Session const& app, Session const& scnCommon, Session const& renderer, Session const& camera)
{
    using adera::gc_mtMagicRocket;

    Session magicRockets;

    // after Update Signal-Float Nodes
    magicRockets.task() = rBuilder.task().assign({}).data(
            "Update machines",
            TopDataIds_t{  },
            wrap_args([] (ACtxParts& rScnParts, UpdMachPerType_t const& rUpdMachines, SignalValues_t<float>& rSigValFloat) noexcept
    {
        Nodes const &rFloatNodes = rScnParts.m_nodePerType[osp::link::gc_ntSigFloat];
        PerMachType &rRockets = rScnParts.m_machines.m_perType[gc_mtMagicRocket];

        for (MachLocalId const local : rUpdMachines[gc_mtMagicRocket].m_localDirty.ones())
        {
            MachAnyId const mach = rRockets.m_localToAny[local];
            lgrn::Span<NodeId const> const portSpan = rFloatNodes.m_machToNode[mach];
            if (NodeId const thrNode = connected_node(portSpan, adera::ports_magicrocket::gc_throttleIn.m_port);
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

    return magicRockets;
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
            TopDataIds_t{                  idVehicleSpawn,                          idVehicleSpawnVB,           idScnParts},
            wrap_args([] (ACtxVehicleSpawn& rVehicleSpawn, ACtxVehicleSpawnVB const& rVehicleSpawnVB, ACtxParts& rScnParts) noexcept
    {
        if (rVehicleSpawn.vehicle_count() == 0)
        {
            return;
        }

        // Count the total number of parts to create
        std::size_t countTotal = 0;
        for (VehicleData const* pVehicle : rVehicleSpawnVB.m_dataVB)
        {
            if (pVehicle == nullptr)
            {
                continue;
            }

            // TODO: size here is O(N), maybe optimize later?
            countTotal += pVehicle->m_partIds.size();
        }

        // Create the part IDs
        rVehicleSpawn.m_newParts.resize(countTotal);
        rScnParts.m_partIds.create(std::begin(rVehicleSpawn.m_newParts), std::end(rVehicleSpawn.m_newParts));

        // Resize some containers to account for newly created part IDs
        rScnParts.m_partPrefabs         .resize(rScnParts.m_partIds.capacity());
        rScnParts.m_partTransformRigid  .resize(rScnParts.m_partIds.capacity());
        rScnParts.m_partRigids          .resize(rScnParts.m_partIds.capacity());
        rScnParts.m_rigidToParts        .data_reserve(rScnParts.m_partIds.capacity());

        // Assign the part IDs to each vehicle to initialize
        // Populates rVehicleSpawn.m_parts
        rVehicleSpawn.m_parts.resize(rVehicleSpawn.m_basic.size());
        auto itPartAvailable    = std::begin(rVehicleSpawn.m_newParts);
        auto itVehiclePartsOut  = std::begin(rVehicleSpawn.m_parts);
        for (VehicleData const* pVehicle : rVehicleSpawnVB.m_dataVB)
        {
            if (pVehicle == nullptr)
            {
                continue;
            }

            std::size_t const count = pVehicle->m_partIds.size();

            (*itVehiclePartsOut) = { &(*itPartAvailable), count };

            std::advance(itPartAvailable, count);
            std::advance(itVehiclePartsOut, 1);
        }

        assert(itPartAvailable == std::end(rVehicleSpawn.m_newParts));
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

        auto itVehicleParts = std::begin(rVehicleSpawn.m_parts);

        for (VehicleData const* pVehicle : rVehicleSpawnVB.m_dataVB)
        {
            std::vector<PartEnt_t> remapPart(pVehicle->m_partIds.capacity());

            // Copy Part data from VehicleBuilder to scene
            auto dstPartIt = std::begin(*itVehicleParts);
            for (uint32_t srcPart : pVehicle->m_partIds.bitview().zeros())
            {
                PartEnt_t const dstPart = *dstPartIt;
                std::advance(dstPartIt, 1);

                remapPart[srcPart] = dstPart;

                PrefabPair const& prefabPairSrc = pVehicle->m_partPrefabs[srcPart];
                PrefabPair prefabPairDst{
                    rResources.owner_create(gc_importer, prefabPairSrc.m_importer),
                    prefabPairSrc.m_prefabId
                };
                rScnParts.m_partPrefabs[dstPart]        = std::move(prefabPairDst);
                rScnParts.m_partTransformRigid[dstPart] = pVehicle->m_partTransforms[srcPart];

                RigidGroup_t const rigid = rScnParts.m_partRigids[dstPart];
                ActiveEnt const rigidEnt = rScnParts.m_rigidToEnt[rigid];

                // Add Prefab and Part init events
                int const& initPfIndex  = rPrefabInit.m_basic.size();
                //TmpPrefabInitBasic &rInitPf  = rPrefabInit.m_basic.emplace_back();
                //rScnTest.m_initParts.emplace_back(PartInit{dstPart, initPfIndex});

                rPrefabInit.m_basic.push_back(TmpPrefabInitBasic{
                    .m_importerRes = prefabPairSrc.m_importer,
                    .m_prefabId = prefabPairSrc.m_prefabId,
                    .m_parent = rigidEnt,
                    .m_pTransform = &pVehicle->m_partTransforms[srcPart]
                });
            }

            std::advance(itVehicleParts, 1);
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

        auto itVehicleRgdGroup  = std::begin(rVehicleSpawnRgd.m_rigidGroups);
        auto itVehicleParts     = std::begin(rVehicleSpawn.m_parts);
        for (ACtxVehicleSpawn::TmpToInit const& toInit : rVehicleSpawn.m_basic)
        {
            // Add parts of each vehicle to its corresponding rigid group
            rScnParts.m_rigidToParts.emplace(*itVehicleRgdGroup, std::begin(*itVehicleParts), std::end(*itVehicleParts));
            std::ranges::for_each(*itVehicleParts, [&rScnParts, rigid = *itVehicleRgdGroup] (PartEnt_t const part)
            {
                rScnParts.m_partRigids[part] = rigid;
            });
            std::advance(itVehicleRgdGroup, 1);
            std::advance(itVehicleParts, 1);
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

/*

static void spawn_vehicle(osp::active::ACtxBasic& rBasic)
{
    using namespace osp::active;

    Matrix4 const rootTf = Matrix4::translation({0.0f, 5.0f, 0.0f});

    // Setup vehicle's root entity as a single rigid body (for now)




    std::vector<PartEnt_t> remapPart(data.m_partIds.capacity());

    // Copy Part data from VehicleBuilder to scene
    auto dstPartIt = std::begin(rigidPartsSpan);
    for (uint32_t srcPart : data.m_partIds.bitview().zeros())
    {
        PartEnt_t const dstPart = *dstPartIt;
        std::advance(dstPartIt, 1);

        remapPart[srcPart] = dstPart;

        osp::PrefabPair const& prefabPairSrc = data.m_partPrefabs[srcPart];
        osp::PrefabPair prefabPairDst{
            rScene.m_pResources->owner_create(gc_importer, prefabPairSrc.m_importer),
            prefabPairSrc.m_prefabId
        };
        rScnParts.m_partPrefabs[dstPart]        = std::move(prefabPairDst);
        rScnParts.m_partTransformRigid[dstPart] = data.m_partTransforms[srcPart];
        rScnParts.m_partRigids[dstPart]         = rigid;

        // Add Prefab and Part init events
        int const& initPfIndex  = rScnTest.m_initPrefabs.size();
        TmpPrefabInit &rInitPf  = rScnTest.m_initPrefabs.emplace_back();
        rInitPf.m_importerRes   = prefabPairSrc.m_importer;
        rInitPf.m_prefabId      = prefabPairSrc.m_prefabId;
        rInitPf.m_pTransform    = &data.m_partTransforms[srcPart];
        rInitPf.m_parent        = rigidEnt;
        rScnTest.m_initParts.emplace_back(PartInit{dstPart, initPfIndex});
    }

    using namespace osp::link;

    // remapMach[source MachAnyId] -> destination MachAnyId
    std::vector<MachAnyId> remapMach(data.m_machines.m_ids.capacity());

    // Make new machines in dst for each machine in source
    copy_machines(data.m_machines, rScnParts.m_machines, remapMach);

    rScnParts.m_machineToPart.resize(rScnParts.m_machines.m_ids.capacity());
    for (MachAnyId const srcMach : data.m_machines.m_ids.bitview().zeros())
    {
        MachAnyId const dstMach = remapMach[srcMach];
        rScnParts.m_machineToPart[dstMach] = remapPart[std::size_t(data.m_machToPart[srcMach])];
    }

    // Do the same for nodes
    std::vector<NodeId> remapNodes(data.m_nodePerType[gc_ntNumber].m_nodeIds.capacity());
    copy_nodes(
            data.m_nodePerType[gc_ntNumber],
            data.m_machines,
            std::move(remapMach),
            rScnParts.m_nodePerType[gc_ntNumber],
            rScnParts.m_machines,
            std::move(remapNodes));

    // TODO: copy node values
    rScnTest.m_nodeNumValues.resize(rScnParts.m_nodePerType[gc_ntNumber].m_nodeIds.size());
}
*/

}
