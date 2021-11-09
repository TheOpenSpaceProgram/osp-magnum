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
#include "ospnewton.h"

#include <osp/Active/basic.h>        // for ACompHierarchy
#include <osp/Active/physics.h>      // for ACompRigidBody
#include <osp/Active/SysPhysics.h>   // for SysPhysics
#include <osp/Active/ActiveScene.h>  // for ActiveScene
#include <osp/Active/activetypes.h>  // for ActiveReg_t

#include <osp/logging.h>

#include <osp/types.h>               // for Matrix4, Vector3
#include <osp/CommonPhysics.h>       // for ECollisionShape

#include <entt/signal/sigh.hpp>      // for entt::sink
#include <entt/entity/entity.hpp>    // for entt::null, entt::null_t

#include <Newton.h>                  // for NewtonBodySetCollision

#include <utility>                   // for std::exchange
#include <cassert>                   // for assert

// IWYU pragma: no_include <cstddef>
// IWYU pragma: no_include <type_traits>

#include <new>

using namespace ospnewton;

using osp::active::ActiveScene;
using osp::active::ActiveReg_t;
using osp::active::ActiveEnt;

using osp::active::ACompPhysBody;
using osp::active::ACompPhysDynamic;
using osp::active::ACompPhysLinearVel;
using osp::active::ACompPhysAngularVel;
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

// Callback called for dynamic rigid bodies for applying force and torque
void cb_force_torque(const NewtonBody* pBody, dFloat timestep, int threadIndex)
{
    // Get context from Newton World
    auto const *const pWorldCtx = static_cast<ACtxNwtWorld*>(
            NewtonWorldGetUserData(NewtonBodyGetWorld(pBody)));

    // Get Associated entity
    uintptr_t const userData = reinterpret_cast<uintptr_t>(
            NewtonBodyGetUserData(pBody));
    ActiveEnt const ent = static_cast<ActiveEnt>(userData);

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
    uintptr_t const userData = reinterpret_cast<uintptr_t>(NewtonBodyGetUserData(pBody));
    ActiveEnt const ent = static_cast<ActiveEnt>(userData);

    pWorldCtx->m_perThread[threadIndex].m_setTf.emplace_back(ent, pBody);
}


void SysNewton::destroy(ActiveScene &rScene)
{
    ActiveReg_t &rReg = rScene.get_registry();

    // delete collision shapes and bodies before deleting the world
    rReg.clear<ACompNwtCollider_t>();
    rReg.clear<ACompNwtBody_t>();

    // delete world
    rReg.unset<ACtxNwtWorld>();
}

void SysNewton::update_translate(ActiveScene& rScene)
{
    ActiveReg_t &rReg = rScene.get_registry();

    auto &rPhysCtx = rReg.ctx<ACtxPhysics>();
    auto &rWorldCtx = rReg.ctx<ACtxNwtWorld>();

    NewtonWorld const* pNwtWorld = rWorldCtx.m_nwtWorld.get();

    // Origin translation
    if (Vector3 const translate = std::exchange(rPhysCtx.m_originTranslate, {});
        ! translate.isZero())
    {
        // Translate every newton body
        for (NewtonBody const* pBody = NewtonWorldGetFirstBody(pNwtWorld);
             pBody != nullptr; pBody = NewtonWorldGetNextBody(pNwtWorld, pBody))
        {
            Matrix4 matrix;
            NewtonBodyGetMatrix(pBody, matrix.data());
            matrix.translation() += translate;
            NewtonBodySetMatrix(pBody, matrix.data());
        }
    }
}

void SysNewton::update_world(ActiveScene& rScene)
{
    ActiveReg_t &rReg = rScene.get_registry();

    auto &rPhysCtx = rReg.ctx<ACtxPhysics>();
    auto &rWorldCtx = rReg.ctx<ACtxNwtWorld>();

    NewtonWorld const* pNwtWorld = rWorldCtx.m_nwtWorld.get();

    // temporary: just delete the Newton Body if colliders change, so it can be
    //            reinitialized with new colliders
    for (ActiveEnt ent : std::exchange(rPhysCtx.m_colliderDirty, {}))
    {
        rReg.remove<ACompNwtCollider_t>(ent);
        create_body(rScene, ent, pNwtWorld);
    }

    // Iterate rigid bodies that don't have a NewtonBody
    for (ActiveEnt ent : rScene.get_registry().view<ACompPhysBody>(entt::exclude<ACompNwtBody_t>))
    {
        create_body(rScene, ent, pNwtWorld);
    }

    // Recompute inertia and center of mass if rigidbody has been modified
    for (ActiveEnt ent : std::exchange(rPhysCtx.m_inertiaDirty, {}))
    {
        compute_rigidbody_inertia(rScene, ent);
        OSP_LOG_TRACE("Updating RB : new CoM Z = {}",
                      entBody.m_centerOfMassOffset.z());
    }

    auto viewNwtBody = rReg.view<ACompNwtBody_t>();
    auto viewLinVel = rReg.view<ACompPhysLinearVel>();
    auto viewAngVel = rReg.view<ACompPhysAngularVel>();

    // Apply changed velocities
    for (auto const& [ent, vel] : std::exchange(rPhysCtx.m_setVelocity, {}))
    {
        NewtonBodySetVelocity(viewNwtBody.get<ACompNwtBody_t>(ent).get(),
                              vel.data());
    }


    // Update the world
    NewtonUpdate(pNwtWorld, rScene.get_time_delta_fixed());

    rReg.clear<ACompPhysNetForce>();
    rReg.clear<ACompPhysNetTorque>();

    auto viewTf = rReg.view<ACompTransform>();
    auto viewDyn = rReg.view<ACompPhysDynamic>();


    // Apply transforms and also velocity
    for (auto &rPerThread : rWorldCtx.m_perThread)
    {
        for (auto const& [ent, pBody] : std::exchange(rPerThread.m_setTf, {}))
        {
            NewtonBodyGetMatrix(
                    pBody, viewTf.get<ACompTransform>(ent).m_transform.data());

            if (viewDyn.contains(ent))
            {
                NewtonBodyGetVelocity(
                        pBody, viewLinVel.get<ACompPhysLinearVel>(ent).data());

                NewtonBodyGetOmega(
                        pBody, viewAngVel.get<ACompPhysAngularVel>(ent).data());
            }
        }
    }
}

void SysNewton::find_colliders_recurse(
        ActiveScene& rScene, ActiveEnt ent, Matrix4 const &transform,
        NewtonWorld const* pNwtWorld, NewtonCollision *pCompound)
{
    ActiveReg_t &rReg = rScene.get_registry();
    ActiveEnt nextChild = ent;

    // iterate through siblings of ent
    while(nextChild != entt::null)
    {
        auto const &rChildHeir = rScene.reg_get<ACompHierarchy>(nextChild);

        if (auto const *pChildTransform
                    = rScene.get_registry().try_get<ACompTransform>(nextChild);
            pChildTransform != nullptr)
        {
            Matrix4 childMatrix = transform * pChildTransform->m_transform;

            if (rReg.all_of<ACompSolidCollider>(nextChild))
            {
                auto* pChildNwtCollider = rReg.try_get<ACompNwtCollider_t>(nextChild);

                NewtonCollision const *collision;

                if (nullptr == pChildNwtCollider)
                {
                    // No Newton collider exists yet
                    collision = NewtonCreateSphere(pNwtWorld, 0.5f, 0, nullptr);
                    rReg.emplace<ACompNwtCollider_t>(nextChild, collision);
                }
                else
                {
                    // Already has Newton collider
                    collision = pChildNwtCollider->get();
                }

                // Set transform relative to root body
                Matrix4 f = Matrix4::translation(childMatrix.translation());
                NewtonCollisionSetMatrix(collision, f.data());

                // Add body to compound collision
                NewtonCompoundCollisionAddSubCollision(pCompound, collision);
            }

            find_colliders_recurse(rScene, rChildHeir.m_childFirst, childMatrix,
                                   pNwtWorld, pCompound);
        }

        nextChild = rChildHeir.m_siblingNext;
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

        if (auto* entNwtBody = rReg.try_get<ACompNwtBody_t>(ent);
            nullptr != entNwtBody)
        {
            pBody = entNwtBody->get();
            NewtonBodySetCollision(pBody, rCompound);
        }
        else
        {
            Matrix4 identity{};

            pBody = NewtonCreateDynamicBody(
                        pNwtWorld, rCompound, identity.data());
            rReg.emplace<ACompNwtBody_t>(ent, pBody);
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

        if (auto const* pEntNwtCollider = rReg.try_get<ACompNwtCollider_t>(ent);
            nullptr != pEntNwtCollider)
        {
            if (auto const* pEntNwtBody = rReg.try_get<ACompNwtBody_t>(ent);
                nullptr != pEntNwtBody)
            {
                pBody = pEntNwtBody->get();
                NewtonBodySetCollision(pBody, pEntNwtCollider->get());
            }
            else
            {
                Matrix4 identity{};

                pBody = NewtonCreateDynamicBody(
                        pNwtWorld, pEntNwtCollider->get(), identity.data());
                rReg.emplace<ACompNwtBody_t>(ent, pBody);
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

    // by now, the Newton rigid body components exist

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
    // note: void* used as storing an ActiveEnt (uint32_t) by value
    NewtonBodySetUserData(pBody, reinterpret_cast<void*>(ent));
}

void SysNewton::compute_rigidbody_inertia(ActiveScene& rScene, ActiveEnt entity)
{
    auto &rEntDyn = rScene.reg_get<ACompPhysDynamic>(entity);
    auto &rEntNwtBody = rScene.reg_get<ACompNwtBody_t>(entity);

    // Compute moments of inertia, mass, and center of mass
    auto const& [inertia, centerOfMass]
            = SysPhysics::compute_hier_inertia(rScene, entity);
    rEntDyn.m_centerOfMassOffset = centerOfMass.xyz();

    // Set inertia and mass
    rEntDyn.m_totalMass = centerOfMass.w();
    rEntDyn.m_inertia.x() = inertia[0][0];  // Ixx
    rEntDyn.m_inertia.y() = inertia[1][1];  // Iyy
    rEntDyn.m_inertia.z() = inertia[2][2];  // Izz

    NewtonBodySetMassMatrix(rEntNwtBody.get(), rEntDyn.m_totalMass,
        rEntDyn.m_inertia.x(),
        rEntDyn.m_inertia.y(),
        rEntDyn.m_inertia.z());
    NewtonBodySetCentreOfMass(rEntNwtBody.get(), centerOfMass.xyz().data());

    OSP_LOG_TRACE("New mass: {}", entBody.m_mass);
}
