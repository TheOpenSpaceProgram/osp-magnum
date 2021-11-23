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
using osp::active::ACtxPhysInputs;
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

    Vector3 force;
    bool hasForce = false;

    Vector3 torque;
    bool hasTorque = false;

    for (ACtxNwtWorld::ForceTorqueIn const& forceTorque
         : pWorldCtx->m_forceTorqueIn)
    {
        if (forceTorque.m_force.contains(ent))
        {
            force += forceTorque.m_force.get(ent);
            hasForce = true;
        }

        if (forceTorque.m_torque.contains(ent))
        {
            torque += forceTorque.m_torque.get(ent);
            hasTorque = true;
        }
    }

    if (hasForce)
    {
        NewtonBodySetForce(pBody, force.data());
    }

    if (hasTorque)
    {
        NewtonBodySetTorque(pBody, torque.data());
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

void SysNewton::update_colliders(
        ACtxPhysics &rCtxPhys, ACtxNwtWorld &rCtxWorld,
        std::vector<ActiveEnt> &rCollidersDirty)
{
    using osp::phys::EShape;

    for (ActiveEnt ent : rCollidersDirty)
    {
        ACompShape const& shape = rCtxPhys.m_shape.get(ent);

        NewtonWorld const *pNwtWorld = rCtxWorld.m_nwtWorld.get();
        NewtonCollision *pNwtCollider = nullptr;

        switch (shape.m_shape)
        {
        case EShape::Custom:
            // TODO
            break;
        case EShape::Sphere:
            pNwtCollider = NewtonCreateSphere(pNwtWorld, 1.0f, 0, nullptr);
            break;
        case EShape::Box:
            pNwtCollider = NewtonCreateBox(pNwtWorld, 2, 2, 2, 0, nullptr);
            break;
        case EShape::Capsule:
            // TODO
            break;
        case EShape::Cylinder:
            // TODO
            break;
        default:

            break;
        }

        if (nullptr != pNwtCollider)
        {
            if (rCtxWorld.m_nwtColliders.contains(ent))
            {
                // Replace existing Newton collider component
                rCtxWorld.m_nwtColliders.get(ent).reset(pNwtCollider);
            }
            else
            {
                // Add new Newton collider component
                rCtxWorld.m_nwtColliders.emplace(ent, pNwtCollider);
            }

        }
    }
}

using Corrade::Containers::ArrayView;

void SysNewton::update_world(
        ACtxPhysics& rCtxPhys,
        ACtxNwtWorld& rCtxWorld,
        ArrayView<ACtxPhysInputs> inputs,
        acomp_storage_t<ACompHierarchy> const& rHier,
        acomp_storage_t<ACompTransform>& rTf,
        acomp_storage_t<ACompTransformControlled>& rTfControlled,
        acomp_storage_t<ACompTransformMutable>& rTfMutable)
{

    NewtonWorld const* pNwtWorld = rCtxWorld.m_nwtWorld.get();

    // temporary: just delete the Newton Body if colliders change, so it can be
    //            reinitialized with new colliders
//    for (ActiveEnt ent : std::exchange(rCtxPhys.m_colliderDirty, {}))
//    {
//        rCtxWorld.m_nwtColliders.remove(ent);
//        create_body(rCtxPhys, rCtxWorld, viewHier, viewTf, rTfControlled,
//                    rTfMutable, ent, pNwtWorld);
//    }

    // Iterate rigid bodies that don't have a NewtonBody

    entt::basic_view<ActiveEnt, entt::get_t<ACompPhysBody>, entt::exclude_t<ACompNwtBody_t>> view{rCtxPhys.m_physBody, rCtxWorld.m_nwtBodies};

    for (ActiveEnt ent : view)
    {
        create_body(rCtxPhys, rCtxWorld, rHier, rTf, rTfControlled,
                    rTfMutable, ent, pNwtWorld);
    }

    // Apply changed velocities
    for (ACtxPhysInputs &rInputs : inputs)
    {
        for (auto const& [ent, vel] : std::exchange(rInputs.m_setVelocity, {}))
        {
            NewtonBodySetVelocity(rCtxWorld.m_nwtBodies.get(ent).get(), vel.data());
        }
    }

    // Expose force and torque inputs to Newton callbacks
    // This swaps their internal pointers with instances accesible from the
    // cb_force_torque callback function.
    // The alternative is pointing to the storages from the callback, which
    // requires additional indirection
    rCtxWorld.m_forceTorqueIn.resize(inputs.size());
    for (int i = 0; i < inputs.size(); i ++)
    {
        std::swap(rCtxWorld.m_forceTorqueIn[i].m_force, inputs[i].m_physNetForce);
        std::swap(rCtxWorld.m_forceTorqueIn[i].m_torque, inputs[i].m_physNetTorque);
    }

    // Update the world
    NewtonUpdate(pNwtWorld, 1.0f / 60.0f);

    // Return force and torques, then clear
    for (int i = 0; i < inputs.size(); i ++)
    {
        std::swap(rCtxWorld.m_forceTorqueIn[i].m_force, inputs[i].m_physNetForce);
        std::swap(rCtxWorld.m_forceTorqueIn[i].m_torque, inputs[i].m_physNetTorque);

        inputs[i].m_physNetForce.clear();
        inputs[i].m_physNetTorque.clear();
    }

    for (ACtxPhysInputs &rInputs : inputs)
    {
        rInputs.m_physNetForce.clear();
        rInputs.m_physNetTorque.clear();
    }

    // Apply transforms and also velocity
    for (auto &rPerThread : rCtxWorld.m_perThread)
    {
        for (auto const& [ent, pBody] : std::exchange(rPerThread.m_setTf, {}))
        {
            NewtonBodyGetMatrix(
                    pBody, rTf.get(ent).m_transform.data());

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
        acomp_storage_t<ACompHierarchy> const& rHier,
        acomp_storage_t<ACompTransform> const& rTf,
        ActiveEnt ent, ActiveEnt firstChild,
        Matrix4 const& transform, NewtonCollision* pCompound)
{
    // Add newton collider if exists
    if (rCtxPhys.m_solidCollider.contains(ent)
        && rCtxWorld.m_nwtColliders.contains(ent))
    {
        NewtonCollision const *pCollision
                = rCtxWorld.m_nwtColliders.get(ent).get();

        // Set transform relative to root body

        Matrix4 const normScale = Matrix4::from(transform.rotation(),
                                                transform.translation());

        NewtonCollisionSetMatrix(pCollision, normScale.data());

        Vector3 const scale = transform.scaling();
        NewtonCollisionSetScale(pCollision, scale.x(), scale.y(), scale.z());

        // Add body to compound collision
        NewtonCompoundCollisionAddSubCollision(pCompound, pCollision);
    }

    if ( ! rCtxPhys.m_hasColliders.contains(ent))
    {
        return;
    }

    // Recurse into children if there are more colliders

    ActiveEnt currentChild = firstChild;

    while(currentChild != entt::null)
    {
        ACompHierarchy const &rChildHeir = rHier.get(currentChild);

        if (rTf.contains(currentChild))
        {
            ACompTransform const &rChildTransform = rTf.get(currentChild);

            Matrix4 const childMatrix = transform * rChildTransform.m_transform;

            find_colliders_recurse(
                    rCtxPhys, rCtxWorld, rHier, rTf, currentChild,
                    rChildHeir.m_childFirst, childMatrix, pCompound);
        }

        // Select next child
        currentChild = rChildHeir.m_siblingNext;
    }
}

void SysNewton::create_body(
        ACtxPhysics& rCtxPhys,
        ACtxNwtWorld& rCtxWorld,
        acomp_storage_t<ACompHierarchy> const& rHier,
        acomp_storage_t<ACompTransform> const& rTf,
        acomp_storage_t<ACompTransformControlled>& rTfControlled,
        acomp_storage_t<ACompTransformMutable>& rTfMutable,
        ActiveEnt ent,
        NewtonWorld const* pNwtWorld)
{

    ACompHierarchy const& entHier = rHier.get(ent);
    ACompTransform const& entTransform = rTf.get(ent);

    // 1. Figure out colliders

    NewtonCollision const *pNwtCollider;
    bool owningNwtCollider = false;

    if (rCtxPhys.m_hasColliders.contains(ent))
    {
        // Create compound collision

        NewtonCollision *pCompound = NewtonCreateCompoundCollision(pNwtWorld, 0);

        // Collect all colliders from hierarchy.
        NewtonCompoundCollisionBeginAddRemove(pCompound);
        find_colliders_recurse(
                rCtxPhys, rCtxWorld, rHier, rTf, ent, entHier.m_childFirst,
                Matrix4{}, pCompound);
        NewtonCompoundCollisionEndAddRemove(pCompound);

        pNwtCollider = pCompound;
        owningNwtCollider = true;
    }
    else if (rCtxWorld.m_nwtColliders.contains(ent))
    {
        // Get single collider from component
        pNwtCollider = rCtxWorld.m_nwtColliders.get(ent).get();
    }
    else
    {
        assert(1); // physics body with no collider!
        return;
    }

    // from here, pNwtCollider exists

    // 2. Create/Get Newton body, and add collider to it

    NewtonBody const* pBody;

    if (rCtxWorld.m_nwtBodies.contains(ent))
    {
        // Already had NewtonBody, update its collider
        pBody = rCtxWorld.m_nwtBodies.get(ent).get();
        NewtonBodySetCollision(pBody, pNwtCollider);
    }
    else
    {
        // Make a new NewtonBody
        Matrix4 identity{};

        pBody = NewtonCreateDynamicBody(
                rCtxWorld.m_nwtWorld.get(), pNwtCollider, identity.data());
        rCtxWorld.m_nwtBodies.emplace(ent, pBody);
    }

    if (owningNwtCollider)
    {
        // Decrement Newton ref count if it was created in this function
        // This ensures that it is only owned by pBody
        NewtonDestroyCollision(std::exchange(pNwtCollider, nullptr));
    }

    // Add transform controlled indicators
    if ( ! rTfControlled.contains(ent)) { rTfControlled.emplace(ent); }
    if ( ! rTfMutable.contains(ent)) { rTfMutable.emplace(ent); }

    if (rCtxPhys.m_physDynamic.contains(ent))
    {
        ACompPhysDynamic const& entDyn = rCtxPhys.m_physDynamic.get(ent);
        // Set mass
        NewtonBodySetMassMatrix(pBody, entDyn.m_totalMass,
                entDyn.m_inertia.x(),
                entDyn.m_inertia.y(),
                entDyn.m_inertia.z());
    }


    // Set position/rotation
    NewtonBodySetMatrix(pBody, entTransform.m_transform.data());

    // Set damping to 0, as default is 0.1
    // reference frame may be moving and air pressure stuff
    NewtonBodySetLinearDamping(pBody, 0.0f);

    // Make it easier to rotate
    //NewtonBodySetAngularDamping(pBody, Vector3(1.0f, 1.0f, 1.0f).data());

    // Set callback for applying force and setting transforms
    NewtonBodySetForceAndTorqueCallback(pBody, &cb_force_torque);
    NewtonBodySetTransformCallback(pBody, &cb_set_transform);

    // Set user data to entity
    // note: void* used as storing an ActiveEnt (uint32_t) by value
    NewtonBodySetUserData(pBody, reinterpret_cast<void*>(ent));
}

