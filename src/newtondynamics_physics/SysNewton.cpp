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

using osp::active::acomp_view_t;
using osp::active::acomp_storage_t;

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


void SysNewton::destroy(ACtxNwtWorld &rCtxWorld)
{
    // delete collision shapes and bodies before deleting the world
    rCtxWorld.m_nwtColliders.clear();
    rCtxWorld.m_nwtBodies.clear();

    // delete world
    rCtxWorld.m_nwtWorld.reset();
}

void SysNewton::update_translate(ACtxPhysics& rCtxPhys, ACtxNwtWorld& rCtxWorld)
{

    NewtonWorld const* pNwtWorld = rCtxWorld.m_nwtWorld.get();

    // Origin translation
    if (Vector3 const translate = std::exchange(rCtxPhys.m_originTranslate, {});
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

void SysNewton::update_world(
        ACtxPhysics& rCtxPhys,
        ACtxNwtWorld& rCtxWorld,
        osp::active::acomp_view_t<osp::active::ACompHierarchy> viewHier,
        osp::active::acomp_view_t<osp::active::ACompTransform> viewTf,
        osp::active::acomp_storage_t<osp::active::ACompTransformControlled> rTfControlled,
        osp::active::acomp_storage_t<osp::active::ACompTransformMutable> rTfMutable)
{

    NewtonWorld const* pNwtWorld = rCtxWorld.m_nwtWorld.get();

    // temporary: just delete the Newton Body if colliders change, so it can be
    //            reinitialized with new colliders
    for (ActiveEnt ent : std::exchange(rCtxPhys.m_colliderDirty, {}))
    {
        rCtxWorld.m_nwtColliders.remove(ent);
        create_body(rCtxPhys, rCtxWorld, viewHier, viewTf, rTfControlled,
                    rTfMutable, ent, pNwtWorld);
    }

    // Iterate rigid bodies that don't have a NewtonBody

    entt::basic_view<ActiveEnt, entt::exclude_t<ACompNwtBody_t>, ACompPhysBody> view{rCtxPhys.m_physBody, rCtxWorld.m_nwtBodies};

    for (ActiveEnt ent : view)
    {
        create_body(rCtxPhys, rCtxWorld, viewHier, viewTf, rTfControlled,
                    rTfMutable, ent, pNwtWorld);
    }

    // Recompute inertia and center of mass if rigidbody has been modified
    for (ActiveEnt ent : std::exchange(rCtxPhys.m_inertiaDirty, {}))
    {
        compute_rigidbody_inertia(rCtxPhys, rCtxWorld, viewHier, viewTf, ent);
        OSP_LOG_TRACE("Updating RB : new CoM Z = {}",
                      entBody.m_centerOfMassOffset.z());
    }

//    auto viewNwtBody = rReg.view<ACompNwtBody_t>();
//    auto viewLinVel = rReg.view<ACompPhysLinearVel>();
//    auto viewAngVel = rReg.view<ACompPhysAngularVel>();

    // Apply changed velocities
    for (auto const& [ent, vel] : std::exchange(rCtxPhys.m_setVelocity, {}))
    {
        NewtonBodySetVelocity(rCtxWorld.m_nwtBodies.get(ent).get(), vel.data());
    }


    // Update the world
    NewtonUpdate(pNwtWorld, 1.0f / 60.0f);

    rCtxPhys.m_physNetForce.clear();
    rCtxPhys.m_physNetTorque.clear();

    // Apply transforms and also velocity
    for (auto &rPerThread : rCtxWorld.m_perThread)
    {
        for (auto const& [ent, pBody] : std::exchange(rPerThread.m_setTf, {}))
        {
            NewtonBodyGetMatrix(
                    pBody, viewTf.get<ACompTransform>(ent).m_transform.data());

            if (rCtxPhys.m_physDynamic.contains(ent))
            {
                NewtonBodyGetVelocity(
                        pBody, rCtxPhys.m_physLinearVel.get(ent).data());

                NewtonBodyGetOmega(
                        pBody, rCtxPhys.m_physAngularVel.get(ent).data());
            }
        }
    }
}

void SysNewton::find_colliders_recurse(
        ACtxPhysics& rCtxPhys, ACtxNwtWorld& rCtxWorld,
        acomp_view_t<ACompHierarchy> viewHier,
        acomp_view_t<ACompTransform> viewTf,
        ActiveEnt ent, Matrix4 const& transform, NewtonCollision* pCompound)
{
    ActiveEnt nextChild = ent;

    // iterate through siblings of ent
    while(nextChild != entt::null)
    {
        auto const &rChildHeir = viewHier.get<ACompHierarchy>(nextChild);


        if (viewTf.contains(nextChild))
        {
            auto const &rChildTransform = viewTf.get<ACompTransform>(nextChild);

            Matrix4 childMatrix = transform * rChildTransform.m_transform;

            if (rCtxPhys.m_solidCollider.contains(nextChild))
            {
                NewtonCollision const *collision;

                if (rCtxWorld.m_nwtColliders.contains(nextChild))
                {
                    // Already has Newton collider
                    collision = rCtxWorld.m_nwtColliders.get(nextChild).get();
                }
                else
                {
                    // No Newton collider exists yet
                    collision = NewtonCreateSphere(rCtxWorld.m_nwtWorld.get(), 0.5f, 0, nullptr);
                    rCtxWorld.m_nwtColliders.emplace(nextChild, collision);
                }

                // Set transform relative to root body
                Matrix4 f = Matrix4::translation(childMatrix.translation());
                NewtonCollisionSetMatrix(collision, f.data());

                // Add body to compound collision
                NewtonCompoundCollisionAddSubCollision(pCompound, collision);
            }


            find_colliders_recurse(
                    rCtxPhys, rCtxWorld, viewHier, viewTf,
                    rChildHeir.m_childFirst, childMatrix, pCompound);
        }

        nextChild = rChildHeir.m_siblingNext;
    }
}

void SysNewton::create_body(
        ACtxPhysics& rCtxPhys,
        ACtxNwtWorld& rCtxWorld,
        acomp_view_t<ACompHierarchy> viewHier,
        acomp_view_t<ACompTransform> viewTf,
        acomp_storage_t<ACompTransformControlled>& rTfControlled,
        acomp_storage_t<ACompTransformMutable>& rTfMutable,
        ActiveEnt ent,
        NewtonWorld const* pNwtWorld)
{

    auto const& entHier = viewHier.get<ACompHierarchy>(ent);
    auto const& entTransform = viewTf.get<ACompTransform>(ent);

    if ( ! ( rCtxPhys.m_solidCollider.contains(ent)
             && rCtxPhys.m_shape.contains(ent) ) )
    {
        // Entity must have a collision shape to define the collision volume
        return;
    }

    ACompShape const& entShape = rCtxPhys.m_shape.get(ent);

    NewtonBody const* pBody = nullptr;

    switch (entShape.m_shape)
    {
    case osp::phys::EShape::Combined:
    {
        // Combine collision shapes from descendants

        NewtonCollision* rCompound
                = NewtonCreateCompoundCollision(pNwtWorld, 0);

        NewtonCompoundCollisionBeginAddRemove(rCompound);
        find_colliders_recurse(
                    rCtxPhys, rCtxWorld, viewHier, viewTf, entHier.m_childFirst,
                    Matrix4{}, rCompound);
        NewtonCompoundCollisionEndAddRemove(rCompound);

        if (rCtxWorld.m_nwtBodies.contains(ent))
        {
            pBody = rCtxWorld.m_nwtBodies.get(ent).get();
            NewtonBodySetCollision(pBody, rCompound);
        }
        else
        {
            Matrix4 identity{};

            pBody = NewtonCreateDynamicBody(
                        pNwtWorld, rCompound, identity.data());
            rCtxWorld.m_nwtBodies.emplace(ent, pBody);
        }

        // this doesn't actually destroy, it decrements a reference count
        NewtonDestroyCollision(rCompound);

        // Update center of mass, moments of inertia
        compute_rigidbody_inertia(rCtxPhys, rCtxWorld, viewHier, viewTf, ent);

        break;
    }
    case osp::phys::EShape::Custom:
    {
        // Get NewtonCollision

        if (rCtxWorld.m_nwtColliders.contains(ent))
        {
            ACompNwtCollider_t const& collider = rCtxWorld.m_nwtColliders.get(ent);
            if (rCtxWorld.m_nwtBodies.contains(ent))
            {
                pBody = rCtxWorld.m_nwtBodies.get(ent).get();
                NewtonBodySetCollision(pBody, collider.get());
            }
            else
            {
                Matrix4 identity{};

                pBody = NewtonCreateDynamicBody(
                        rCtxWorld.m_nwtWorld.get(), collider.get(), identity.data());
                rCtxWorld.m_nwtBodies.emplace(ent, pBody);
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

    if ( ! rTfControlled.contains(ent)) { rTfControlled.emplace(ent); }
    if ( ! rTfMutable.contains(ent)) { rTfMutable.emplace(ent); }

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

void SysNewton::compute_rigidbody_inertia(
        ACtxPhysics& rCtxPhys,
        ACtxNwtWorld& rCtxWorld,
        acomp_view_t<ACompHierarchy> const viewHier,
        acomp_view_t<ACompTransform> const viewTf,
        ActiveEnt entity)
{
    ACompPhysDynamic &rEntDyn = rCtxPhys.m_physDynamic.get(entity);
    ACompNwtBody_t &rEntNwtBody = rCtxWorld.m_nwtBodies.get(entity);

    // Compute moments of inertia, mass, and center of mass
    auto const& [inertia, centerOfMass] = SysPhysics::compute_hier_inertia(
            viewHier, viewTf, rCtxPhys.m_mass, rCtxPhys.m_shape, entity);
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
