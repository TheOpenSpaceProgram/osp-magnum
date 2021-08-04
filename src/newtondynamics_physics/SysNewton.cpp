/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
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

#include "SysNewton.h"               // IWYU pragma: associated

#include <osp/Active/scene.h>        // for ACompHierarchy
#include <osp/Active/physics.h>      // for ACompRigidBody
#include <osp/Active/SysPhysics.h>   // for SysPhysics
#include <osp/Active/ActiveScene.h>  // for ActiveScene
#include <osp/Active/activetypes.h>  // for ActiveReg_t

#include <osp/types.h>               // for Matrix4, Vector3
#include <osp/CommonPhysics.h>       // for ECollisionShape

#include <entt/signal/sigh.hpp>      // for entt::sink
#include <entt/entity/entity.hpp>    // for entt::null, entt::null_t

#include <spdlog/spdlog.h>           // for SPDLOG_LOGGER_TRACE

#include <Newton.h>                  // for NewtonBodySetCollision

#include <utility>                   // for std::exchange
#include <cassert>                   // for assert

// IWYU pragma: no_include <cstddef>
// IWYU pragma: no_include <type_traits>

#include <new>

using ospnewton::SysNewton;
using ospnewton::ACompNwtBody;
using ospnewton::ACompNwtWorld;

using osp::active::ActiveScene;
using osp::active::ActiveReg_t;
using osp::active::ActiveEnt;

using osp::active::ACompPhysBody;
using osp::active::ACompPhysDynamic;
using osp::active::ACompPhysNetForce;
using osp::active::ACompPhysNetTorque;
using osp::active::ACtxPhysics;
using osp::active::SysPhysics;

using osp::active::ACompHierarchy;
using osp::active::ACompShape;
using osp::active::ACompSolidCollider;
using osp::active::ACompTransform;
using osp::active::ACompTransformControlled;
using osp::active::ACompTransformMutable;

using osp::Matrix4;
using osp::Vector3;

// TODO: not compiling, std::hardware_destructive_interference_size not found?
template<typename T>
struct alignas(64) CachePadded : T { };

struct ACtxNwtWorld
{
    ACtxNwtWorld(ActiveScene &rScene, int threadCount)
     : m_pScene(&rScene)
     , m_viewForce(rScene.get_registry().view<ACompPhysNetForce>())
     , m_viewTorque(rScene.get_registry().view<ACompPhysNetTorque>())
     , m_setTf(threadCount)
    { }

    ActiveScene *m_pScene;


    entt::basic_view<ActiveEnt, entt::exclude_t<>, ACompPhysNetForce> m_viewForce;
    entt::basic_view<ActiveEnt, entt::exclude_t<>, ACompPhysNetTorque> m_viewTorque;

    // transformations to set recorded in cb_set_transform
    using SetTfPair_t = std::pair<ActiveEnt, NewtonBody const* const>;
    std::vector< CachePadded< std::vector<SetTfPair_t> > > m_setTf;
};

// Callback called for dynamic rigid bodies for applying force and torque
void cb_force_torque(const NewtonBody* pBody, dFloat timestep, int threadIndex)
{
    // Get context from Newton World
    ACtxNwtWorld* pWorldCtx = static_cast<ACtxNwtWorld*>(
            NewtonWorldGetUserData(NewtonBodyGetWorld(pBody)));

    // Get Associated entity
    uintptr_t userData = reinterpret_cast<uintptr_t>(NewtonBodyGetUserData(pBody));
    ActiveEnt ent = static_cast<ActiveEnt>(userData);

    // Apply force
    if (pWorldCtx->m_viewForce.contains(ent))
    {
        NewtonBodySetForce(
                pBody, pWorldCtx->m_viewForce.get<ACompPhysNetForce>(ent).data());
    }

    // Apply torque
    if (pWorldCtx->m_viewTorque.contains(ent))
    {
        NewtonBodySetTorque(
                pBody, pWorldCtx->m_viewTorque.get<ACompPhysNetTorque>(ent).data());
    }
}

void cb_set_transform(NewtonBody const* const pBody,
                      const dFloat* const pMatrix, int threadIndex)
{
    // Get context from Newton World
    ACtxNwtWorld* pWorldCtx = static_cast<ACtxNwtWorld*>(
            NewtonWorldGetUserData(NewtonBodyGetWorld(pBody)));

    // Get Associated entity
    uintptr_t userData = reinterpret_cast<uintptr_t>(NewtonBodyGetUserData(pBody));
    ActiveEnt ent = static_cast<ActiveEnt>(userData);

    pWorldCtx->m_setTf[threadIndex].emplace_back(ent, pBody);
}

void SysNewton::setup(ActiveScene &rScene)
{
    // Connect signal handlers to destruct Newton objects when components are
    // deleted.

    rScene.get_registry().on_destroy<ACompNwtBody>()
                    .connect<&SysNewton::on_body_destruct>();

    rScene.get_registry().on_destroy<ACompNwtCollider>()
                    .connect<&SysNewton::on_shape_destruct>();

    rScene.get_registry().on_destroy<ACompNwtWorld>()
                    .connect<&SysNewton::on_world_destruct>();
}

void SysNewton::update_world(ActiveScene& rScene)
{
    ActiveReg_t &rReg = rScene.get_registry();

    // Try getting the newton world from the scene root.
    // It is very unlikely that multiple physics worlds are ever needed
    auto *pWorldComp = rReg.try_get<ACompNwtWorld>(rScene.hier_get_root());

    if (pWorldComp == nullptr)
    {
        return; // No physics world component
    }

    if (pWorldComp->m_nwtWorld == nullptr)
    {
        // Create world if not yet initialized
        pWorldComp->m_nwtWorld = NewtonCreate();
        auto& rWorldCtx = rReg.set<ACtxNwtWorld>(
                rScene, NewtonGetThreadsCount(pWorldComp->m_nwtWorld));

        NewtonWorldSetUserData(pWorldComp->m_nwtWorld, &rWorldCtx);
    }

    auto &rPhysCtx = rReg.ctx<ACtxPhysics>();
    auto &rWorldCtx = rReg.ctx<ACtxNwtWorld>();

    NewtonWorld const* nwtWorld = pWorldComp->m_nwtWorld;

    // temporary: just delete the Newton Body if colliders change, so it can be
    //            reinitialized with new colliders
    for (ActiveEnt ent : std::exchange(rPhysCtx.m_colliders, {}))
    {
        rReg.remove<ACompNwtBody>(ent);
    }

    // Iterate rigid bodies that don't have a NewtonBody
    for (ActiveEnt ent : rScene.get_registry().view<ACompPhysBody>(entt::exclude<ACompNwtBody>))
    {
        create_body(rScene, ent, nwtWorld);
    }

    // Recompute inertia and center of mass if rigidbody has been modified
    for (ActiveEnt ent : std::exchange(rPhysCtx.m_inertia, {}))
    {
        compute_rigidbody_inertia(rScene, ent);
        SPDLOG_LOGGER_TRACE(m_scene.get_application().get_logger(),
                            "Updating RB : new CoM Z = {}",
                            entBody.m_centerOfMassOffset.z());
    }

    // Update the world
    NewtonUpdate(nwtWorld, rScene.get_time_delta_fixed());

    rReg.clear<ACompPhysNetForce>();
    rReg.clear<ACompPhysNetTorque>();

    auto viewTf = rReg.view<ACompTransform>();

    // Apply transforms
    for (auto &vec : rWorldCtx.m_setTf)
    {
        for (auto const& [ent, pBody] : std::exchange(vec, {}))
        {
            NewtonBodyGetMatrix(
                    pBody, viewTf.get<ACompTransform>(ent).m_transform.data());
        }
    }
}

void SysNewton::find_colliders_recurse(
        ActiveScene& rScene, ActiveEnt ent, Matrix4 const &transform,
        NewtonWorld const* pNwtWorld, NewtonCollision *rCompound)
{
    ActiveReg_t &rReg = rScene.get_registry();
    ActiveEnt nextChild = ent;

    // iterate through siblings of ent
    while(nextChild != entt::null)
    {
        auto const &childHeir = rScene.reg_get<ACompHierarchy>(nextChild);


        if (auto const *pChildTransform
                    = rScene.get_registry().try_get<ACompTransform>(nextChild);
            pChildTransform != nullptr)
        {
            Matrix4 childMatrix = transform * pChildTransform->m_transform;

            if (rReg.all_of<ACompSolidCollider>(nextChild))
            {
                auto* pChildNwtCollider = rReg.try_get<ACompNwtCollider>(nextChild);

                NewtonCollision const *collision;

                if (nullptr == pChildNwtCollider)
                {
                    // No Newton collider exists yet
                    collision = NewtonCreateSphere(pNwtWorld, 0.5f, 0, nullptr);
                    rReg.emplace<ACompNwtCollider>(nextChild, collision);
                    ACompNwtCollider f(collision);
                }
                else
                {
                    // Already has Newton collider
                    collision = pChildNwtCollider->collision();
                }

                // Set transform relative to root body
                Matrix4 f = Matrix4::translation(childMatrix.translation());
                NewtonCollisionSetMatrix(collision, f.data());

                // Add body to compound collision
                NewtonCompoundCollisionAddSubCollision(rCompound, collision);
            }

            find_colliders_recurse(rScene, childHeir.m_childFirst, childMatrix,
                                   pNwtWorld, rCompound);
        }

        nextChild = childHeir.m_siblingNext;
    }
}

void SysNewton::create_body(ActiveScene& rScene, ActiveEnt ent,
                            NewtonWorld const* pNwtWorld)
{
    ActiveReg_t &rReg = rScene.get_registry();

    auto const& entHier = rReg.get<ACompHierarchy>(ent);
    auto const& entTransform = rReg.get<ACompTransform>(ent);
    auto const* entShape = rReg.try_get<ACompShape>(ent);

    if (( ! rReg.all_of<ACompSolidCollider>(ent)) || (entShape == nullptr))
    {
        // Entity must have a collision shape to define the collision volume
        return;
    }

    NewtonBody const* pBody = nullptr;

    switch (entShape->m_shape)
    {
    case osp::phys::EShape::Combined:
    {
        // Combine collision shapes from descendants

        NewtonCollision* rCompound
                = NewtonCreateCompoundCollision(pNwtWorld, 0);

        NewtonCompoundCollisionBeginAddRemove(rCompound);
        find_colliders_recurse(rScene, entHier.m_childFirst, Matrix4{},
                               pNwtWorld, rCompound);
        NewtonCompoundCollisionEndAddRemove(rCompound);

        if (auto* entNwtBody = rReg.try_get<ACompNwtBody>(ent);
            nullptr != entNwtBody)
        {
            pBody = entNwtBody->body();
            NewtonBodySetCollision(pBody, rCompound);
        }
        else
        {
            Matrix4 identity{};

            pBody = NewtonCreateDynamicBody(
                        pNwtWorld, rCompound, identity.data());
            rReg.emplace<ACompNwtBody>(ent, pBody);
        }

        // this doesn't actually destroy, it decrements a reference count
        NewtonDestroyCollision(rCompound);

        // Update center of mass, moments of inertia
        compute_rigidbody_inertia(rScene, ent);

        break;
    }
    case osp::phys::EShape::Custom:
    {
        // Get NewtonCollision

        if (auto const* pEntNwtCollider = rReg.try_get<ACompNwtCollider>(ent);
            nullptr != pEntNwtCollider)
        {
            if (auto const* pEntNwtBody = rReg.try_get<ACompNwtBody>(ent);
                nullptr != pEntNwtBody)
            {
                pBody = pEntNwtBody->body();
                NewtonBodySetCollision(pBody, pEntNwtCollider->collision());
            }
            else
            {
                Matrix4 identity{};

                pBody = NewtonCreateDynamicBody(
                        pNwtWorld, pEntNwtCollider->collision(), identity.data());
                rReg.emplace<ACompNwtBody>(ent, pBody);
            }
        }
        else
        {
            // make a collision shape somehow
            pBody = nullptr;
        }
        break;
    }
    default:
        assert(false);
        return;
    }

    // by now, the Newton rigid body components now exist

    rScene.get_registry().emplace_or_replace<ACompTransformControlled>(ent);
    rScene.get_registry().emplace_or_replace<ACompTransformMutable>(ent);

    // Set position/rotation
    NewtonBodySetMatrix(pBody, entTransform.m_transform.data());

    // Set damping to 0, as default is 0.1
    // reference frame may be moving and air pressure stuff
    NewtonBodySetLinearDamping(pBody, 0.0f);

    // Make it easier to rotate
    NewtonBodySetAngularDamping(pBody, Vector3(1.0f, 1.0f, 1.0f).data());

    // Set callback for applying force and setting transforms
    NewtonBodySetForceAndTorqueCallback(pBody, &cb_force_torque);
    NewtonBodySetTransformCallback(pBody, &cb_set_transform);

    // Set user data to entity
    // WARNING: this will cause issues if 64-bit entities are used on a
    //          32-bit machine
    NewtonBodySetUserData(pBody, reinterpret_cast<void*>(ent));
}

void SysNewton::compute_rigidbody_inertia(ActiveScene& rScene, ActiveEnt entity)
{
    auto &rEntDyn = rScene.reg_get<ACompPhysDynamic>(entity);
    auto &rEntNwtBody = rScene.reg_get<ACompNwtBody>(entity);

    // Compute moments of inertia, mass, and center of mass
    auto const& [inertia, centerOfMass]
            = SysPhysics::compute_hier_inertia(rScene, entity);
    rEntDyn.m_centerOfMassOffset = centerOfMass.xyz();

    // Set inertia and mass
    rEntDyn.m_totalMass = centerOfMass.w();
    rEntDyn.m_inertia.x() = inertia[0][0];  // Ixx
    rEntDyn.m_inertia.y() = inertia[1][1];  // Iyy
    rEntDyn.m_inertia.z() = inertia[2][2];  // Izz

    NewtonBodySetMassMatrix(rEntNwtBody.body(), rEntDyn.m_totalMass,
        rEntDyn.m_inertia.x(),
        rEntDyn.m_inertia.y(),
        rEntDyn.m_inertia.z());
    NewtonBodySetCentreOfMass(rEntNwtBody.body(), centerOfMass.xyz().data());

    SPDLOG_LOGGER_TRACE(m_scene.get_application().get_logger(), "New mass: {}",
                        entBody.m_mass);
}

ACompNwtWorld* SysNewton::try_get_physics_world(ActiveScene &rScene)
{
    return rScene.get_registry().try_get<ACompNwtWorld>(rScene.hier_get_root());
}

void SysNewton::on_body_destruct(ActiveReg_t& rReg, ActiveEnt ent)
{
    NewtonBody const *pBody = rReg.get<ACompNwtBody>(ent).body();
    NewtonDestroyBody(pBody); // make sure the Newton body is destroyed
}

void SysNewton::on_shape_destruct(ActiveReg_t& rReg, ActiveEnt ent)
{
    NewtonCollision const *pShape = rReg.get<ACompNwtCollider>(ent).collision();
    NewtonDestroyCollision(pShape); // make sure the shape is destroyed
}

void SysNewton::on_world_destruct(ActiveReg_t& rReg, ActiveEnt ent)
{
    NewtonWorld const *pWorld = rReg.get<ACompNwtWorld>(ent).m_nwtWorld;
    if (pWorld != nullptr)
    {
        // delete all collision shapes first to prevent crash
        rReg.clear<ACompNwtCollider>();

        // delete all rigid bodies too
        rReg.clear<ACompNwtBody>();

        NewtonDestroy(pWorld); // make sure the world is destroyed
    }
}

NewtonCollision* SysNewton::newton_create_tree_collision(
        const NewtonWorld *pNewtonWorld, int shapeId)
{
    return NewtonCreateTreeCollision(pNewtonWorld, shapeId);
}

void SysNewton::newton_tree_collision_add_face(
        const NewtonCollision* pTreeCollision, int vertexCount,
        const float* vertexPtr, int strideInBytes, int faceAttribute)
{
    NewtonTreeCollisionAddFace(pTreeCollision, vertexCount, vertexPtr,
                               strideInBytes, faceAttribute);
}

void SysNewton::newton_tree_collision_begin_build(
        const NewtonCollision* pTreeCollision)
{
    NewtonTreeCollisionBeginBuild(pTreeCollision);
}

void SysNewton::newton_tree_collision_end_build(
        const NewtonCollision* pTreeCollision, int optimize)
{
    NewtonTreeCollisionEndBuild(pTreeCollision, optimize);
}

