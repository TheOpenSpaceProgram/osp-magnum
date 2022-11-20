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
#include <osp/Active/SysSceneGraph.h>
#include <osp/Active/SysPrefabInit.h>
#include <osp/Active/SysRender.h>

#include <osp/Resource/ImporterData.h>
#include <osp/Resource/resources.h>
#include <osp/Resource/resourcetypes.h>

#include <newtondynamics_physics/ospnewton.h>
#include <newtondynamics_physics/SysNewton.h>

using namespace osp;
using namespace osp::active;

using osp::restypes::gc_importer;
using osp::phys::EShape;
using ospnewton::SysNewton;
using ospnewton::ACtxNwtWorld;
using ospnewton::NewtonBodyId;
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



Session setup_shape_spawn_newton(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              scnCommon,
        Session const&              physics,
        Session const&              shapeSpawn,
        Session const&              newton)
{
    Session shapeSpawnNwt;

    OSP_SESSION_UNPACK_DATA(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(physics,    TESTAPP_PHYSICS);
    OSP_SESSION_UNPACK_TAGS(physics,    TESTAPP_PHYSICS);
    OSP_SESSION_UNPACK_DATA(shapeSpawn, TESTAPP_SHAPE_SPAWN);
    OSP_SESSION_UNPACK_TAGS(shapeSpawn, TESTAPP_SHAPE_SPAWN);
    OSP_SESSION_UNPACK_DATA(newton,     TESTAPP_NEWTON);

    shapeSpawnNwt.task() = rBuilder.task().assign({tgSceneEvt, tgSpawnReq, tgSpawnEntReq, tgPhysBodyMod}).data(
            "Add physics to spawned shapes",
            TopDataIds_t{                   idActiveIds,              idSpawner,             idSpawnerEnts,             idPhys,              idNwt },
            wrap_args([] (ActiveReg_t const &rActiveIds, SpawnerVec_t& rSpawner, EntVector_t& rSpawnerEnts, ACtxPhysics& rPhys, ACtxNwtWorld& rNwt) noexcept
    {
        for (std::size_t i = 0; i < rSpawner.size(); ++i)
        {
            SpawnShape const &spawn = rSpawner[i];
            ActiveEnt const root    = rSpawnerEnts[i * 2];
            ActiveEnt const child   = rSpawnerEnts[i * 2 + 1];

            NewtonCollision *pCollision = SysNewton::create_primative(rNwt, spawn.m_shape, {0.0f, 0.0f, 0.0f}, Matrix3{}, spawn.m_size);
            NewtonBody *pBody = NewtonCreateDynamicBody(rNwt.m_world.get(), pCollision, Matrix4{}.data());

            NewtonBodyId const bodyId = rNwt.m_bodyIds.create();
            rNwt.m_bodyPtrs.resize(rNwt.m_bodyIds.capacity());
            rNwt.m_bodyToEnt.resize(rNwt.m_bodyIds.capacity());

            rNwt.m_bodyPtrs[bodyId].reset(pBody);

            rNwt.m_bodyToEnt[bodyId] = root;
            rNwt.m_entToBody.emplace(root, bodyId);

            Vector3 const inertia = collider_inertia_tensor(spawn.m_shape, spawn.m_size, spawn.m_mass);

            NewtonBodySetMassMatrix(pBody, spawn.m_mass, inertia.x(), inertia.y(), inertia.z());
            NewtonDestroyCollision(std::exchange(pCollision, nullptr));
            NewtonBodySetMatrix(pBody, Matrix4::translation(spawn.m_position).data());
            NewtonBodySetLinearDamping(pBody, 0.0f);
            NewtonBodySetForceAndTorqueCallback(pBody, &SysNewton::cb_force_torque);
            NewtonBodySetTransformCallback(pBody, &SysNewton::cb_set_transform);
            NewtonBodySetUserData(pBody, reinterpret_cast<void*>(root));
        }
    }));

    return shapeSpawnNwt;
}


} // namespace testapp::scenes


