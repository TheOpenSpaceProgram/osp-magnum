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
#include "scene_newton.h"
#include "scene_physics.h"
#include "identifiers.h"

#include <osp/Active/basic.h>
#include <osp/Active/drawing.h>
#include <osp/Active/physics.h>
#include <osp/Active/parts.h>
#include <osp/Active/SysPhysics.h>
#include <osp/Active/SysPrefabInit.h>
#include <osp/Active/SysRender.h>
#include <osp/Active/SysSceneGraph.h>

#include <osp/Resource/ImporterData.h>
#include <osp/Resource/resources.h>
#include <osp/Resource/resourcetypes.h>

#include <adera/machines/links.h>

#include <newtondynamics_physics/ospnewton.h>
#include <newtondynamics_physics/SysNewton.h>

using namespace osp;
using namespace osp::active;
using namespace osp::link;
using namespace ospnewton;

using osp::restypes::gc_importer;
using osp::phys::EShape;
using Corrade::Containers::arrayView;

namespace testapp::scenes
{


Session setup_newton(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              scnCommon,
        Session const&              physics)
{
    OSP_SESSION_UNPACK_DATA(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(physics,    TESTAPP_PHYSICS);
    OSP_SESSION_UNPACK_TAGS(physics,    TESTAPP_PHYSICS);

    Session newton;
    OSP_SESSION_ACQUIRE_DATA(newton, topData, TESTAPP_NEWTON);
    OSP_SESSION_ACQUIRE_TAGS(newton, rTags,   TESTAPP_NEWTON);

    rBuilder.tag(tgNwtBodyDel).depend_on({tgNwtBodyPrv});
    rBuilder.tag(tgNwtBodyMod).depend_on({tgNwtBodyPrv, tgNwtBodyDel});
    rBuilder.tag(tgNwtBodyReq).depend_on({tgNwtBodyPrv, tgNwtBodyDel, tgNwtBodyMod});
    rBuilder.tag(tgNwtBodyClr).depend_on({tgNwtBodyPrv, tgNwtBodyDel, tgNwtBodyMod, tgNwtBodyReq});

    top_emplace< ACtxNwtWorld >(topData, idNwt, 2);

    using ospnewton::SysNewton;

    newton.task() = rBuilder.task().assign({tgSceneEvt, tgDelTotalReq, tgNwtBodyDel}).data(
            "Delete Newton components",
            TopDataIds_t{              idNwt,                   idDelTotal},
            wrap_args([] (ACtxNwtWorld& rNwt, EntVector_t const& rDelTotal) noexcept
    {
        SysNewton::update_delete (rNwt, std::cbegin(rDelTotal), std::cend(rDelTotal));
    }));

    newton.task() = rBuilder.task().assign({tgTimeEvt, tgPhysPrv, tgNwtBodyPrv, tgPhysTransformMod, tgTransformMod}).data(
            "Update Newton world",
            TopDataIds_t{           idBasic,             idPhys,              idNwt,           idDeltaTimeIn },
            wrap_args([] (ACtxBasic& rBasic, ACtxPhysics& rPhys, ACtxNwtWorld& rNwt, float const deltaTimeIn) noexcept
    {
        SysNewton::update_world(rPhys, rNwt, deltaTimeIn, rBasic.m_scnGraph, rBasic.m_transform);
    }));

    top_emplace< ACtxNwtWorld >(topData, idNwt, 2);

    return newton;
}

osp::Session setup_newton_factors(
        Builder_t&                  rBuilder,
        ArrayView<entt::any>        topData,
        Tags&                       rTags)
{
    Session nwtFactors;
    OSP_SESSION_ACQUIRE_DATA(nwtFactors, topData, TESTAPP_NEWTON_FORCES);

    auto &rFactors = top_emplace<ForceFactors_t>(topData, idNwtFactors);

    std::fill(rFactors.begin(), rFactors.end(), 0);

    return nwtFactors;
}

osp::Session setup_newton_force_accel(
        Builder_t&                  rBuilder,
        ArrayView<entt::any>        topData,
        Tags&                       rTags,
        Session const&              newton,
        Session const&              nwtFactors,
        Vector3                     accel)
{
    using UserData_t = ACtxNwtWorld::ForceFactorFunc::UserData_t;
    OSP_SESSION_UNPACK_DATA(newton,     TESTAPP_NEWTON);
    OSP_SESSION_UNPACK_DATA(nwtFactors, TESTAPP_NEWTON_FORCES);

    auto &rNwt      = top_get<ACtxNwtWorld>(topData, idNwt);

    Session nwtAccel;
    OSP_SESSION_ACQUIRE_DATA(nwtAccel, topData, TESTAPP_NEWTON_ACCEL);

    auto &rAccel    = top_emplace<Vector3>(topData, idAcceleration, accel);

    ACtxNwtWorld::ForceFactorFunc const factor
    {
        .m_func = [] (NewtonBody const* pBody, BodyId const bodyID, ACtxNwtWorld const& rNwt, UserData_t data, Vector3& rForce, Vector3& rTorque) noexcept
        {
            float mass = 0.0f;
            float dummy = 0.0f;
            NewtonBodyGetMass(pBody, &mass, &dummy, &dummy, &dummy);

            auto const& force = *reinterpret_cast<Vector3 const*>(data[0]);
            rForce += force * mass;
        },
        .m_userData = {&rAccel}
    };

    // Register force

    std::size_t const index = rNwt.m_factors.size();
    rNwt.m_factors.emplace_back(factor);

    auto factorBits = lgrn::bit_view(top_get<ForceFactors_t>(topData, idNwtFactors));
    factorBits.set(index);

    return nwtAccel;
}


Session setup_shape_spawn_newton(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              scnCommon,
        Session const&              physics,
        Session const&              shapeSpawn,
        Session const&              newton,
        Session const&              nwtFactors)
{
    Session shapeSpawnNwt;


    OSP_SESSION_UNPACK_DATA(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(physics,    TESTAPP_PHYSICS);
    OSP_SESSION_UNPACK_TAGS(physics,    TESTAPP_PHYSICS);
    OSP_SESSION_UNPACK_DATA(shapeSpawn, TESTAPP_SHAPE_SPAWN);
    OSP_SESSION_UNPACK_TAGS(shapeSpawn, TESTAPP_SHAPE_SPAWN);
    OSP_SESSION_UNPACK_DATA(newton,     TESTAPP_NEWTON);
    OSP_SESSION_UNPACK_TAGS(newton,     TESTAPP_NEWTON);
    OSP_SESSION_UNPACK_DATA(nwtFactors, TESTAPP_NEWTON_FORCES);

    shapeSpawnNwt.task() = rBuilder.task().assign({tgSceneEvt, tgSpawnReq, tgSpawnEntReq, tgNwtBodyMod}).data(
            "Add physics to spawned shapes",
            TopDataIds_t{                   idActiveIds,              idSpawner,             idSpawnerEnts,             idPhys,              idNwt,              idNwtFactors },
            wrap_args([] (ActiveReg_t const &rActiveIds, SpawnerVec_t& rSpawner, EntVector_t& rSpawnerEnts, ACtxPhysics& rPhys, ACtxNwtWorld& rNwt, ForceFactors_t nwtFactors) noexcept
    {
        for (std::size_t i = 0; i < rSpawner.size(); ++i)
        {
            SpawnShape const &spawn = rSpawner[i];
            ActiveEnt const root    = rSpawnerEnts[i * 2];
            ActiveEnt const child   = rSpawnerEnts[i * 2 + 1];

            NwtColliderPtr_t pCollision{ SysNewton::create_primative(rNwt, spawn.m_shape) };
            SysNewton::orient_collision(pCollision.get(), spawn.m_shape, {0.0f, 0.0f, 0.0f}, Matrix3{}, spawn.m_size);
            NewtonBody *pBody = NewtonCreateDynamicBody(rNwt.m_world.get(), pCollision.get(), Matrix4{}.data());

            BodyId const bodyId = rNwt.m_bodyIds.create();
            SysNewton::resize_body_data(rNwt);

            rNwt.m_bodyPtrs[bodyId].reset(pBody);

            rNwt.m_bodyToEnt[bodyId]    = root;
            rNwt.m_bodyFactors[bodyId]  = nwtFactors;
            rNwt.m_entToBody.emplace(root, bodyId);

            Vector3 const inertia = collider_inertia_tensor(spawn.m_shape, spawn.m_size, spawn.m_mass);

            NewtonBodySetMassMatrix(pBody, spawn.m_mass, inertia.x(), inertia.y(), inertia.z());
            NewtonBodySetMatrix(pBody, Matrix4::translation(spawn.m_position).data());
            NewtonBodySetLinearDamping(pBody, 0.0f);
            NewtonBodySetForceAndTorqueCallback(pBody, &SysNewton::cb_force_torque);
            NewtonBodySetTransformCallback(pBody, &SysNewton::cb_set_transform);
            SysNewton::set_userdata_bodyid(pBody, bodyId);
        }
    }));

    return shapeSpawnNwt;
}

void compound_collect_recurse(
        ACtxPhysics const&  rCtxPhys,
        ACtxNwtWorld&       rCtxWorld,
        ACtxBasic const&    rBasic,
        ActiveEnt           ent,
        Matrix4 const&      transform,
        NewtonCollision*    pCompound)
{
    EShape const shape = rCtxPhys.m_shape[std::size_t(ent)];

    if (shape != EShape::None)
    {

        NwtColliderPtr_t &rPtr = rCtxWorld.m_colliders.contains(ent)
                               ? rCtxWorld.m_colliders.get(ent)
                               : rCtxWorld.m_colliders.emplace(ent);

        if (rPtr.get() == nullptr)
        {
            rPtr = SysNewton::create_primative(rCtxWorld, shape);
        }

        SysNewton::orient_collision(rPtr.get(), shape, transform.translation(), transform.rotation(), transform.scaling());
        NewtonCompoundCollisionAddSubCollision(pCompound, rPtr.get());
    }

    if ( ! rCtxPhys.m_hasColliders.test(std::size_t(ent)) )
    {
        return;
    }

    // Recurse into children if there are more colliders
    for (ActiveEnt child : SysSceneGraph::children(rBasic.m_scnGraph, ent))
    {
        if (rBasic.m_transform.contains(child))
        {
            ACompTransform const &rChildTransform = rBasic.m_transform.get(child);

            Matrix4 const childMatrix = transform * rChildTransform.m_transform;

            compound_collect_recurse(
                    rCtxPhys, rCtxWorld, rBasic, child, childMatrix, pCompound);
        }
    }

}

Session setup_vehicle_spawn_newton(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              scnCommon,
        Session const&              physics,
        Session const&              prefabs,
        Session const&              parts,
        Session const&              vehicleSpawn,
        Session const&              newton,
        TopDataId const             idResources)
{
    OSP_SESSION_UNPACK_DATA(scnCommon,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(scnCommon,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(physics,        TESTAPP_PHYSICS);
    OSP_SESSION_UNPACK_TAGS(physics,        TESTAPP_PHYSICS);
    OSP_SESSION_UNPACK_DATA(prefabs,        TESTAPP_PREFABS);
    OSP_SESSION_UNPACK_TAGS(prefabs,        TESTAPP_PREFABS);
    OSP_SESSION_UNPACK_DATA(vehicleSpawn,   TESTAPP_VEHICLE_SPAWN);
    OSP_SESSION_UNPACK_TAGS(vehicleSpawn,   TESTAPP_VEHICLE_SPAWN);
    OSP_SESSION_UNPACK_DATA(newton,         TESTAPP_NEWTON);
    OSP_SESSION_UNPACK_TAGS(newton,         TESTAPP_NEWTON);
    OSP_SESSION_UNPACK_DATA(parts,          TESTAPP_PARTS);
    OSP_SESSION_UNPACK_TAGS(parts,          TESTAPP_PARTS);

    Session vehicleSpawnNwt;
    //OSP_SESSION_ACQUIRE_DATA(vehicleSpawnNwt, topData, TESTAPP_VEHICLE_SPAWN_NWT);
    OSP_SESSION_ACQUIRE_TAGS(vehicleSpawnNwt, rTags,   TESTAPP_VEHICLE_SPAWN_NWT);

    rBuilder.tag(tgNwtVhWeldEntReq)     .depend_on({tgNwtVhWeldEntMod});
    rBuilder.tag(tgNwtVhHierReq)        .depend_on({tgNwtVhHierMod});

    vehicleSpawnNwt.task() = rBuilder.task().assign({tgSceneEvt, tgEntNew, tgNwtVhWeldEntMod}).data(
            "Create entity for each rigid group",
            TopDataIds_t{             idActiveIds,                  idVehicleSpawn,           idScnParts },
            wrap_args([] (ActiveReg_t& rActiveIds, ACtxVehicleSpawn& rVehicleSpawn, ACtxParts& rScnParts) noexcept
    {
        if (rVehicleSpawn.new_vehicle_count() == 0)
        {
            return;
        }

        rVehicleSpawn.m_newWeldToEnt.resize(rVehicleSpawn.m_newWeldToWeld.size());
        rActiveIds.create(std::begin(rVehicleSpawn.m_newWeldToEnt), std::end(rVehicleSpawn.m_newWeldToEnt));

        // update WeldId->ActiveEnt mapping
        auto itWeldEnt = std::begin(rVehicleSpawn.m_newWeldToEnt);
        for (WeldId const weld : rVehicleSpawn.m_newWeldToWeld)
        {
            rScnParts.m_weldToEnt[weld] = *itWeldEnt;
            std::advance(itWeldEnt, 1);
        }
    }));

    vehicleSpawnNwt.task() = rBuilder.task().assign({tgSceneEvt, tgVsBasicInReq, tgVsWeldReq, tgNwtVhWeldEntReq, tgPrefabEntReq, tgNwtVhHierMod, tgPfParentHierMod, tgHierMod, tgTransformNew}).data(
            "Add vehicle entities to Scene Graph",
            TopDataIds_t{           idBasic,                   idActiveIds,                        idVehicleSpawn,           idScnParts,                idPrefabInit,           idResources },
            wrap_args([] (ACtxBasic& rBasic, ActiveReg_t const& rActiveIds, ACtxVehicleSpawn const& rVehicleSpawn, ACtxParts& rScnParts, ACtxPrefabInit& rPrefabInit, Resources& rResources) noexcept
    {
        // ActiveEnts created for welds + ActiveEnts created for vehicle prefabs
        std::size_t const totalEnts = rVehicleSpawn.m_newWeldToEnt.size() + rPrefabInit.m_newEnts.size();

        auto const& itWeldsFirst        = std::begin(rVehicleSpawn.m_newWeldToWeld);
        auto const& itWeldOffsetsLast   = std::end(rVehicleSpawn.m_newVhWeldOffsets);
        auto itWeldOffsets              = std::begin(rVehicleSpawn.m_newVhWeldOffsets);

        rBasic.m_scnGraph.resize(rActiveIds.capacity());

        for (ACtxVehicleSpawn::TmpToInit const& toInit : rVehicleSpawn.m_newVhBasicIn)
        {
            auto const itWeldOffsetsNext = std::next(itWeldOffsets);
            NewWeldId const weldOffsetNext = (itWeldOffsetsNext != itWeldOffsetsLast)
                                           ? (*itWeldOffsetsNext)
                                           : rVehicleSpawn.m_newWeldToWeld.size();

            std::for_each(itWeldsFirst + (*itWeldOffsets),
                          itWeldsFirst + weldOffsetNext,
                          [&rBasic, &rScnParts, &rVehicleSpawn, &rPrefabInit, &rResources, &toInit] (WeldId const weld)
            {
                // Count parts in this weld first
                std::size_t entCount = 0;
                for (PartId const part : rScnParts.m_weldToParts[weld])
                {
                    NewPartId const newPart = rVehicleSpawn.m_partToNewPart[part];
                    uint32_t const prefabInit = rVehicleSpawn.m_newPartPrefabs[newPart];
                    entCount += rPrefabInit.m_ents[prefabInit].size();
                }

                ActiveEnt const weldEnt = rScnParts.m_weldToEnt[weld];

                rBasic.m_transform.emplace(weldEnt, Matrix4::from(toInit.m_rotation.toMatrix(), toInit.m_position));

                SubtreeBuilder bldRoot = SysSceneGraph::add_descendants(rBasic.m_scnGraph, entCount + 1);
                SubtreeBuilder bldWeld = bldRoot.add_child(weldEnt, entCount);

                for (PartId const part : rScnParts.m_weldToParts[weld])
                {
                    NewPartId const newPart = rVehicleSpawn.m_partToNewPart[part];
                    uint32_t const prefabInit   = rVehicleSpawn.m_newPartPrefabs[newPart];
                    auto const& basic           = rPrefabInit.m_basicIn[prefabInit];
                    auto const& ents            = rPrefabInit.m_ents[prefabInit];

                    SysPrefabInit::add_to_subtree(basic, ents, rResources, bldWeld);
                }
            });

            itWeldOffsets = itWeldOffsetsNext;
        }
    }));

    vehicleSpawnNwt.task() = rBuilder.task().assign({tgSceneEvt, tgVsBasicInReq, tgVsWeldReq, tgNwtVhWeldEntReq, tgNwtVhHierReq, tgPfParentHierReq, tgNwtBodyMod}).data(
            "Add Newton physics to rigid group entities",
            TopDataIds_t{                   idActiveIds,           idBasic,             idPhys,              idNwt,                        idVehicleSpawn,                 idScnParts  },
            wrap_args([] (ActiveReg_t const &rActiveIds, ACtxBasic& rBasic, ACtxPhysics& rPhys, ACtxNwtWorld& rNwt, ACtxVehicleSpawn const& rVehicleSpawn, ACtxParts const& rScnParts) noexcept
    {
        if (rVehicleSpawn.new_vehicle_count() == 0)
        {
            return;
        }

        rPhys.m_hasColliders.ints().resize(rActiveIds.vec().capacity());

        auto const& itWeldsFirst        = std::begin(rVehicleSpawn.m_newWeldToWeld);
        auto const& itWeldOffsetsLast   = std::end(rVehicleSpawn.m_newVhWeldOffsets);
        auto itWeldOffsets              = std::begin(rVehicleSpawn.m_newVhWeldOffsets);

        for (ACtxVehicleSpawn::TmpToInit const& toInit : rVehicleSpawn.m_newVhBasicIn)
        {
            auto const itWeldOffsetsNext = std::next(itWeldOffsets);
            NewWeldId const weldOffsetNext = (itWeldOffsetsNext != itWeldOffsetsLast)
                                           ? (*itWeldOffsetsNext)
                                           : rVehicleSpawn.m_newWeldToWeld.size();

            std::for_each(itWeldsFirst + (*itWeldOffsets),
                          itWeldsFirst + weldOffsetNext,
                          [&rBasic, &rScnParts, &rVehicleSpawn, &toInit, &rPhys, &rNwt] (WeldId const weld)
            {
                ActiveEnt const weldEnt = rScnParts.m_weldToEnt[weld];

                auto const transform = Matrix4::from(toInit.m_rotation.toMatrix(), toInit.m_position);
                NwtColliderPtr_t pCompound{ NewtonCreateCompoundCollision(rNwt.m_world.get(), 0) };

                rPhys.m_hasColliders.set(std::size_t(weldEnt));

                // Collect all colliders from hierarchy.
                NewtonCompoundCollisionBeginAddRemove(pCompound.get());
                compound_collect_recurse( rPhys, rNwt, rBasic, weldEnt, Matrix4{}, pCompound.get() );
                NewtonCompoundCollisionEndAddRemove(pCompound.get());

                NewtonBody *pBody = NewtonCreateDynamicBody(rNwt.m_world.get(), pCompound.get(), Matrix4{}.data());

                BodyId const bodyId = rNwt.m_bodyIds.create();
                SysNewton::resize_body_data(rNwt);


                rNwt.m_bodyPtrs[bodyId].reset(pBody);
                rNwt.m_bodyToEnt[bodyId] = weldEnt;
                rNwt.m_bodyFactors[bodyId] = {1}; // TODO: temporary
                rNwt.m_entToBody.emplace(weldEnt, bodyId);

                float   totalMass = 0.0f;
                Vector3 massPos{0.0f};
                SysPhysics::calculate_subtree_mass_center(rBasic.m_transform, rPhys, rBasic.m_scnGraph, weldEnt, massPos, totalMass);

                Vector3 const com = massPos / totalMass;
                auto const comToOrigin = Matrix4::translation( - com );

                Matrix3 inertiaTensor{0.0f};
                SysPhysics::calculate_subtree_mass_inertia(rBasic.m_transform, rPhys, rBasic.m_scnGraph, weldEnt, inertiaTensor, comToOrigin);

                Matrix4 const inertiaTensorMat4{inertiaTensor};
                NewtonBodySetFullMassMatrix(pBody, totalMass, inertiaTensorMat4.data());

                NewtonBodySetCentreOfMass(pBody, com.data());

                NewtonBodySetGyroscopicTorque(pBody, 1);
                NewtonBodySetMatrix(pBody, transform.data());
                NewtonBodySetLinearDamping(pBody, 0.0f);
                NewtonBodySetAngularDamping(pBody, Vector3{0.0f}.data());
                NewtonBodySetForceAndTorqueCallback(pBody, &SysNewton::cb_force_torque);
                NewtonBodySetTransformCallback(pBody, &SysNewton::cb_set_transform);
                SysNewton::set_userdata_bodyid(pBody, bodyId);

                rPhys.m_setVelocity.emplace_back(weldEnt, toInit.m_velocity);
            });

            itWeldOffsets = itWeldOffsetsNext;
        }
    }));

    return vehicleSpawnNwt;
}

struct BodyRocket
{
    Quaternion      m_rotation;
    Vector3         m_offset;

    MachLocalId     m_local;
    NodeId          m_throttleIn;
    NodeId          m_multiplierIn;
};

struct ACtxRocketsNwt
{
    // map each bodyId to a {machine, offset}
    lgrn::IntArrayMultiMap<BodyId, BodyRocket> m_bodyRockets;

};

static void assign_rockets(
        ACtxBasic const&            rBasic,
        ACtxParts const&            rScnParts,
        ACtxNwtWorld&               rNwt,
        ACtxRocketsNwt&             rRocketsNwt,
        Nodes const&                rFloatNodes,
        PerMachType const&          machtypeRocket,
        ForceFactors_t const&       rNwtFactors,
        WeldId const                weld,
        std::vector<BodyRocket>&    rTemp)
{
    using adera::gc_mtMagicRocket;
    using adera::ports_magicrocket::gc_throttleIn;
    using adera::ports_magicrocket::gc_multiplierIn;

    ActiveEnt const weldEnt = rScnParts.m_weldToEnt[weld];
    BodyId const body = rNwt.m_entToBody.at(weldEnt);

    if (rRocketsNwt.m_bodyRockets.contains(body))
    {
        rRocketsNwt.m_bodyRockets.erase(body);
    }

    for (PartId const part : rScnParts.m_weldToParts[weld])
    {
        auto const sizeBefore = rTemp.size();

        for (MachinePair const pair : rScnParts.m_partToMachines[part])
        {
            if (pair.m_type != gc_mtMagicRocket)
            {
                continue; // This machine is not a rocket
            }

            MachAnyId const mach            = machtypeRocket.m_localToAny[pair.m_local];
            auto const&     portSpan        = rFloatNodes.m_machToNode[mach];
            NodeId const    throttleIn      = connected_node(portSpan, gc_throttleIn.m_port);
            NodeId const    multiplierIn    = connected_node(portSpan, gc_multiplierIn.m_port);

            if (   (throttleIn   == lgrn::id_null<NodeId>())
                || (multiplierIn == lgrn::id_null<NodeId>()) )
            {
                continue; // Throttle and/or multiplier is not connected
            }

            BodyRocket &rBodyRocket     = rTemp.emplace_back();
            rBodyRocket.m_local         = pair.m_local;
            rBodyRocket.m_throttleIn    = throttleIn;
            rBodyRocket.m_multiplierIn  = multiplierIn;
        }

        if (sizeBefore == rTemp.size())
        {
            continue; // No rockets found
        }

        // calculate transform relative to body root
        // start from part, then walk parents up
        ActiveEnt const partEnt = rScnParts.m_partToActive[part];

        Matrix4     transform   = rBasic.m_transform.get(partEnt).m_transform;
        ActiveEnt   parent      = rBasic.m_scnGraph.m_entParent[std::size_t(partEnt)];

        while (parent != weldEnt)
        {
            Matrix4 const& parentTransform = rBasic.m_transform.get(parent).m_transform;
            transform = parentTransform * transform;
            parent = rBasic.m_scnGraph.m_entParent[std::size_t(parent)];
        }

        auto const      rotation    = Quaternion::fromMatrix(transform.rotation());
        Vector3 const   offset      = transform.translation();

        for (BodyRocket &rBodyRocket : arrayView(rTemp).exceptPrefix(sizeBefore))
        {
            rBodyRocket.m_rotation  = rotation;
            rBodyRocket.m_offset    = offset;
        }
    }

    ForceFactors_t &rBodyFactors = rNwt.m_bodyFactors[body];

    // TODO: Got lazy, eventually iterate ForceFactors_t instead of
    //       just using [0]. This breaks if more factor bits are added
    static_assert(ForceFactors_t{}.size() == 1u);

    if ( rTemp.empty() )
    {
        rBodyFactors[0] &= ~rNwtFactors[0];
        return;
    }

    rBodyFactors[0] |= rNwtFactors[0];

    rRocketsNwt.m_bodyRockets.emplace(body, rTemp.begin(), rTemp.end());
    rTemp.clear();
}

Session setup_rocket_thrust_newton(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              scnCommon,
        Session const&              physics,
        Session const&              prefabs,
        Session const&              parts,
        Session const&              signalsFloat,
        Session const&              newton,
        Session const&              nwtFactors)
{
    OSP_SESSION_UNPACK_DATA(scnCommon,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(scnCommon,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(physics,        TESTAPP_PHYSICS);
    OSP_SESSION_UNPACK_TAGS(physics,        TESTAPP_PHYSICS);
    OSP_SESSION_UNPACK_DATA(prefabs,        TESTAPP_PREFABS);
    OSP_SESSION_UNPACK_TAGS(prefabs,        TESTAPP_PREFABS);
    OSP_SESSION_UNPACK_DATA(parts,          TESTAPP_PARTS);
    OSP_SESSION_UNPACK_TAGS(parts,          TESTAPP_PARTS);
    OSP_SESSION_UNPACK_DATA(signalsFloat,   TESTAPP_SIGNALS_FLOAT)
    OSP_SESSION_UNPACK_TAGS(signalsFloat,   TESTAPP_SIGNALS_FLOAT);
    OSP_SESSION_UNPACK_DATA(newton,         TESTAPP_NEWTON);
    OSP_SESSION_UNPACK_TAGS(newton,         TESTAPP_NEWTON);
    OSP_SESSION_UNPACK_DATA(nwtFactors,     TESTAPP_NEWTON_FORCES);


    Session rocketNwt;
    OSP_SESSION_ACQUIRE_DATA(rocketNwt, topData, TESTAPP_ROCKETS_NWT);

    auto &rRocketsNwt   = top_emplace< ACtxRocketsNwt >(topData, idRocketsNwt);


    rocketNwt.task() = rBuilder.task().assign({tgSceneEvt, tgLinkReq, tgWeldReq, tgNwtBodyReq}).data(
            "Assign rockets to Newton bodies",
            TopDataIds_t{                   idActiveIds,           idBasic,             idPhys,              idNwt,                 idScnParts,                idRocketsNwt,                      idNwtFactors},
            wrap_args([] (ActiveReg_t const &rActiveIds, ACtxBasic& rBasic, ACtxPhysics& rPhys, ACtxNwtWorld& rNwt, ACtxParts const& rScnParts, ACtxRocketsNwt& rRocketsNwt, ForceFactors_t const& rNwtFactors) noexcept
    {
        using adera::gc_mtMagicRocket;

        Nodes const &rFloatNodes = rScnParts.m_nodePerType[gc_ntSigFloat];
        PerMachType const& machtypeRocket = rScnParts.m_machines.m_perType[gc_mtMagicRocket];

        rRocketsNwt.m_bodyRockets.ids_reserve(rNwt.m_bodyIds.size());
        rRocketsNwt.m_bodyRockets.data_reserve(rScnParts.m_machines.m_perType[gc_mtMagicRocket].m_localIds.capacity());

        std::vector<BodyRocket> temp;

        for (WeldId const weld : rScnParts.m_weldDirty)
        {
            assign_rockets(rBasic, rScnParts, rNwt, rRocketsNwt, rFloatNodes, machtypeRocket, rNwtFactors, weld, temp);
        }
    }));

    auto &rScnParts     = top_get< ACtxParts >              (topData, idScnParts);
    auto &rSigValFloat  = top_get< SignalValues_t<float> >  (topData, idSigValFloat);
    Machines &rMachines = rScnParts.m_machines;

    using UserData_t = ACtxNwtWorld::ForceFactorFunc::UserData_t;
    ACtxNwtWorld::ForceFactorFunc const factor
    {
        .m_func = [] (NewtonBody const* pBody, BodyId const body, ACtxNwtWorld const& rNwt, UserData_t data, Vector3& rForce, Vector3& rTorque) noexcept
        {
            auto const& rRocketsNwt     = *reinterpret_cast<ACtxRocketsNwt const*>          (data[0]);
            auto const& rMachines       = *reinterpret_cast<Machines const*>                (data[1]);
            auto const& rSigValFloat    = *reinterpret_cast<SignalValues_t<float> const*>   (data[2]);

            auto &rBodyRockets = rRocketsNwt.m_bodyRockets[body];

            if (rBodyRockets.size() == 0)
            {
                return;
            }

            std::array<dFloat, 4> nwtRot; // quaternion xyzw
            NewtonBodyGetRotation(pBody, nwtRot.data());
            Quaternion const rot{{nwtRot[0], nwtRot[1], nwtRot[2]}, nwtRot[3]};

            Vector3 com;
            NewtonBodyGetCentreOfMass(pBody, com.data());

            for (BodyRocket const& bodyRocket : rBodyRockets)
            {
                float const throttle = std::clamp(rSigValFloat[bodyRocket.m_throttleIn], 0.0f, 1.0f);
                float const multiplier = rSigValFloat[bodyRocket.m_multiplierIn];

                float const thrustMag = throttle * multiplier;

                if (thrustMag == 0.0f)
                {
                    continue;
                }

                Vector3 const offsetRel = rot.transformVector(bodyRocket.m_offset - com);

                Vector3 const direction = (rot * bodyRocket.m_rotation).transformVector(adera::gc_rocketForward);

                Vector3 const thrustForce = direction * thrustMag;
                Vector3 const thrustTorque = Magnum::Math::cross(offsetRel, thrustForce);

                rForce += thrustForce;
                rTorque += thrustTorque;
            }
        },
        .m_userData = { &rRocketsNwt, &rMachines, &rSigValFloat }
    };

    auto &rNwt = top_get<ACtxNwtWorld>(topData, idNwt);

    std::size_t const index = rNwt.m_factors.size();
    rNwt.m_factors.emplace_back(factor);

    auto factorBits = lgrn::bit_view(top_get<ForceFactors_t>(topData, idNwtFactors));
    factorBits.set(index);

    return rocketNwt;
}


} // namespace testapp::scenes


