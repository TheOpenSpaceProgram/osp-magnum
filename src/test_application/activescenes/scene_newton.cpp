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
#include <osp/Active/SysSceneGraph.h>
#include <osp/Active/SysPrefabInit.h>
#include <osp/Active/SysRender.h>

#include <osp/Resource/ImporterData.h>
#include <osp/Resource/resources.h>
#include <osp/Resource/resourcetypes.h>

#include <newtondynamics_physics/ospnewton.h>
#include <newtondynamics_physics/SysNewton.h>

#include <iostream>

using namespace osp;
using namespace osp::active;
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

    top_emplace< ACtxNwtWorld >(topData, idNwt, 2);

    using ospnewton::SysNewton;

    newton.task() = rBuilder.task().assign({tgSceneEvt, tgDelTotalReq, tgPhysBodyDel}).data(
            "Delete Newton components",
            TopDataIds_t{              idNwt,                   idDelTotal},
            wrap_args([] (ACtxNwtWorld& rNwt, EntVector_t const& rDelTotal) noexcept
    {
        SysNewton::update_delete (rNwt, std::cbegin(rDelTotal), std::cend(rDelTotal));
    }));

    newton.task() = rBuilder.task().assign({tgTimeEvt, tgTransformMod, tgHierMod, tgPhysMod}).data(
            "Update Newton world",
            TopDataIds_t{           idBasic,             idPhys,              idNwt,           idDeltaTimeIn },
            wrap_args([] (ACtxBasic& rBasic, ACtxPhysics& rPhys, ACtxNwtWorld& rNwt, float const deltaTimeIn) noexcept
    {
        SysNewton::update_world(rPhys, rNwt, deltaTimeIn, rBasic.m_scnGraph, rBasic.m_transform);
    }));

    top_emplace< ACtxNwtWorld >(topData, idNwt, 2);

    return newton;
}

osp::Session setup_newton_force_set(
        Builder_t&                  rBuilder,
        ArrayView<entt::any>        topData,
        Tags&                       rTags)
{
    Session nwtForces;
    OSP_SESSION_ACQUIRE_DATA(nwtForces, topData, TESTAPP_NEWTON_FORCES);

    auto &rForces = top_emplace<ForceSet_t>(topData, idNwtForces);

    std::fill(rForces.begin(), rForces.end(), 0);

    return nwtForces;
}

osp::Session setup_newton_force_accel(
        Builder_t&                  rBuilder,
        ArrayView<entt::any>        topData,
        Tags&                       rTags,
        Session const&              newton,
        Session const&              nwtForce,
        Vector3                     accel)
{
    using UserData_t = ACtxNwtWorld::ForceFactor::UserData_t;
    OSP_SESSION_UNPACK_DATA(newton,     TESTAPP_NEWTON);
    OSP_SESSION_UNPACK_DATA(nwtForce,   TESTAPP_NEWTON_FORCES);

    Session nwtAccel;
    OSP_SESSION_ACQUIRE_DATA(nwtAccel, topData, TESTAPP_NEWTON_ACCEL);

    auto &rAccel = top_emplace<Vector3>(topData, idAcceleration, accel);

    ACtxNwtWorld::ForceFactor const factor
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
    auto &rNwt = top_get<ACtxNwtWorld>(topData, idNwt);
    std::size_t const index = rNwt.m_forces.size();
    rNwt.m_forces.emplace_back(factor);

    auto forcesBits = lgrn::bit_view(top_get<ForceSet_t>(topData, idNwtForces));
    forcesBits.set(index);

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
        Session const&              nwtForces)
{
    Session shapeSpawnNwt;


    OSP_SESSION_UNPACK_DATA(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(physics,    TESTAPP_PHYSICS);
    OSP_SESSION_UNPACK_TAGS(physics,    TESTAPP_PHYSICS);
    OSP_SESSION_UNPACK_DATA(shapeSpawn, TESTAPP_SHAPE_SPAWN);
    OSP_SESSION_UNPACK_TAGS(shapeSpawn, TESTAPP_SHAPE_SPAWN);
    OSP_SESSION_UNPACK_DATA(newton,     TESTAPP_NEWTON);
    OSP_SESSION_UNPACK_DATA(nwtForces,  TESTAPP_NEWTON_FORCES);

    shapeSpawnNwt.task() = rBuilder.task().assign({tgSceneEvt, tgSpawnReq, tgSpawnEntReq, tgPhysBodyMod}).data(
            "Add physics to spawned shapes",
            TopDataIds_t{                   idActiveIds,              idSpawner,             idSpawnerEnts,             idPhys,              idNwt,          idNwtForces },
            wrap_args([] (ActiveReg_t const &rActiveIds, SpawnerVec_t& rSpawner, EntVector_t& rSpawnerEnts, ACtxPhysics& rPhys, ACtxNwtWorld& rNwt, ForceSet_t nwtForces) noexcept
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

            rNwt.m_bodyToEnt[bodyId] = root;
            rNwt.m_bodyForces[bodyId] = nwtForces;
            rNwt.m_entToBody.emplace(root, bodyId);

            Vector3 const inertia = collider_inertia_tensor(spawn.m_shape, spawn.m_size, spawn.m_mass);

            NewtonBodySetMassMatrix(pBody, spawn.m_mass, inertia.x(), inertia.y(), inertia.z());
            NewtonBodySetMatrix(pBody, Matrix4::translation(spawn.m_position).data());
            NewtonBodySetLinearDamping(pBody, 0.0f);
            NewtonBodySetForceAndTorqueCallback(pBody, &SysNewton::cb_force_torque);
            NewtonBodySetTransformCallback(pBody, &SysNewton::cb_set_transform);
            NewtonBodySetUserData(pBody, reinterpret_cast<void*>(bodyId));
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
            rPtr.reset(SysNewton::create_primative(rCtxWorld, shape));
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

Session setup_vehicle_spawn_rigid_newton(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              scnCommon,
        Session const&              physics,
        Session const&              prefabs,
        Session const&              parts,
        Session const&              vehicleSpawn,
        Session const&              vehicleSpawnRgd,
        Session const&              newton)
{
    OSP_SESSION_UNPACK_DATA(scnCommon,          TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(scnCommon,          TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(physics,            TESTAPP_PHYSICS);
    OSP_SESSION_UNPACK_TAGS(physics,            TESTAPP_PHYSICS);
    OSP_SESSION_UNPACK_DATA(prefabs,            TESTAPP_PREFABS);
    OSP_SESSION_UNPACK_TAGS(prefabs,            TESTAPP_PREFABS);
    OSP_SESSION_UNPACK_DATA(vehicleSpawn,       TESTAPP_VEHICLE_SPAWN);
    OSP_SESSION_UNPACK_TAGS(vehicleSpawn,       TESTAPP_VEHICLE_SPAWN);
    OSP_SESSION_UNPACK_DATA(vehicleSpawnRgd,    TESTAPP_VEHICLE_SPAWN_RIGID);
    OSP_SESSION_UNPACK_DATA(newton,             TESTAPP_NEWTON);
    OSP_SESSION_UNPACK_DATA(parts,              TESTAPP_PARTS);
    OSP_SESSION_UNPACK_TAGS(parts,              TESTAPP_PARTS);

    Session vehicleSpawnRgdNwt;


    vehicleSpawnRgdNwt.task() = rBuilder.task().assign({tgSceneEvt, tgVehicleSpawnReq, tgVSpawnRgdEntReq, tgPfParentHierReq, tgPhysBodyMod}).data(
            "Add physics to rigid group entities",
            TopDataIds_t{                   idActiveIds,           idBasic,             idPhys,              idNwt,                        idVehicleSpawn,                             idVehicleSpawnRgd,                 idScnParts  },
            wrap_args([] (ActiveReg_t const &rActiveIds, ACtxBasic& rBasic, ACtxPhysics& rPhys, ACtxNwtWorld& rNwt, ACtxVehicleSpawn const& rVehicleSpawn, ACtxVehicleSpawnRigid const& rVehicleSpawnRgd, ACtxParts const& rScnParts) noexcept
    {
        if (rVehicleSpawn.vehicle_count() == 0)
        {
            return;
        }

        rPhys.m_hasColliders.ints().resize(rActiveIds.vec().capacity());

        auto itRigidEnt = std::begin(rVehicleSpawnRgd.m_rigidGroupEnt);

        for (ACtxVehicleSpawn::TmpToInit const& toInit : rVehicleSpawn.m_basic)
        {
            ActiveEnt const rigidEnt = *itRigidEnt;
            std::advance(itRigidEnt, 1);

            auto const transform = Matrix4::from(toInit.m_rotation.toMatrix(), toInit.m_position);
            NwtColliderPtr_t pCompound{ NewtonCreateCompoundCollision(rNwt.m_world.get(), 0) };

            rPhys.m_hasColliders.set(std::size_t(rigidEnt));

            // Collect all colliders from hierarchy.
            NewtonCompoundCollisionBeginAddRemove(pCompound.get());
            compound_collect_recurse( rPhys, rNwt, rBasic, rigidEnt, Matrix4{}, pCompound.get() );
            NewtonCompoundCollisionEndAddRemove(pCompound.get());

            NewtonBody *pBody = NewtonCreateDynamicBody(rNwt.m_world.get(), pCompound.get(), Matrix4{}.data());


            BodyId const bodyId = rNwt.m_bodyIds.create();
            SysNewton::resize_body_data(rNwt);


            rNwt.m_bodyPtrs[bodyId].reset(pBody);
            rNwt.m_bodyToEnt[bodyId] = rigidEnt;
            rNwt.m_bodyForces[bodyId] = {1};
            rNwt.m_entToBody.emplace(rigidEnt, bodyId);

            Vector3 const inertia = {1.0f, 1.0f, 1.0f};

            NewtonBodySetMassMatrix(pBody, 1.0f, inertia.x(), inertia.y(), inertia.z());
            NewtonBodySetMatrix(pBody, transform.data());
            NewtonBodySetLinearDamping(pBody, 0.0f);
            NewtonBodySetForceAndTorqueCallback(pBody, &SysNewton::cb_force_torque);
            NewtonBodySetTransformCallback(pBody, &SysNewton::cb_set_transform);
            NewtonBodySetUserData(pBody, reinterpret_cast<void*>(bodyId));

            rPhys.m_setVelocity.emplace_back(rigidEnt, toInit.m_velocity);
        }
    }));

    return vehicleSpawnRgdNwt;
}



} // namespace testapp::scenes


