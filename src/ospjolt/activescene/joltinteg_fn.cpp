/**
 * Open Space Program
 * Copyright © 2019-2024 Open Space Program Project
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

#include "joltinteg_fn.h"          // IWYU pragma: associated
#include <osp/activescene/basic_fn.h>

#include <utility>                   // for std::exchange
#include <cassert>                   // for assert

// IWYU pragma: no_include <cstddef>
// IWYU pragma: no_include <type_traits>

using namespace ospjolt;

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

using osp::EShape;

using osp::active::ActiveEnt;
using osp::active::ACtxPhysics;
using osp::active::SysSceneGraph;

using osp::Matrix3;
using osp::Matrix4;
using osp::Vector3;

using Corrade::Containers::ArrayView;

void SysJolt::update_world(
        ACtxBasic                   &rBasic,
        ACtxPhysics                 &rPhys,
        ACtxJoltWorld               &rJoltWorld,
        float                       timestep) noexcept
{
    constexpr int collisionSteps = 1;

    rJoltWorld.m_listener.m_pCtxBasic        = &rBasic;
    rJoltWorld.m_listener.m_pCtxPhysics      = &rPhys;
    rJoltWorld.m_listener.m_pCtxJoltWorld    = &rJoltWorld;

    // calls PhysicsStepListenerImpl::OnStep
    rJoltWorld.m_physicsSystem.Update(timestep, collisionSteps, &rJoltWorld.m_oAllocator.value(), &rJoltWorld.m_jobSystem);
}

void SysJolt::remove_components(ACtxJoltWorld& rCtxWorld, ActiveEnt ent) noexcept
{
    auto itBodyId = rCtxWorld.m_entToBody.find(ent);
    JPH::BodyInterface &bodyInterface = rCtxWorld.m_physicsSystem.GetBodyInterface();

    if (itBodyId != rCtxWorld.m_entToBody.end())
    {
        BodyId const bodyId = itBodyId->second;
        JPH::BodyID joltBodyId = BToJolt(bodyId);
        bodyInterface.RemoveBody(joltBodyId);
        bodyInterface.DestroyBody(joltBodyId);
        rCtxWorld.m_bodyIds.remove(bodyId);
        rCtxWorld.m_bodyToEnt[bodyId] = lgrn::id_null<ActiveEnt>();
        rCtxWorld.m_entToBody.erase(itBodyId);
    }
}

JPH::Ref<JPH::Shape> SysJolt::create_primitive(ACtxJoltWorld &rCtxWorld, osp::EShape shape, JPH::Vec3Arg scale)
{
    switch (shape)
    {
    case EShape::Sphere:
    //Sphere only support uniform shape
        return JPH::SphereShapeSettings(1.0f * scale.GetX()).Create().Get();
    case EShape::Box:
        return JPH::BoxShapeSettings(scale).Create().Get();
    case EShape::Cylinder:
        //cylinder needs to be internally rotated 90° to match with graphics
        return JPH::RotatedTranslatedShapeSettings(
                    JPH::Vec3Arg::sZero(),
                    JPH::Quat::sRotation(JPH::Vec3::sAxisX(), JPH::JPH_PI/2),
                    new JPH::CylinderShapeSettings(scale.GetZ(), scale.GetX())
                ).Create().Get();
        
    default:
        return JPH::SphereShapeSettings(1.0f * scale.GetX()).Create().Get();
    }
}

void SysJolt::scale_shape(JPH::Ref<JPH::Shape> rShape, JPH::Vec3Arg scale) {
    if (rShape->GetSubType() == JPH::EShapeSubType::Scaled) {
        JPH::ScaledShape* rScaledShape = dynamic_cast<JPH::ScaledShape*>(rShape.GetPtr());
        if (rScaledShape != nullptr) 
        {
            rShape = new JPH::ScaledShape(rScaledShape->GetInnerShape(), scale * rScaledShape->GetScale());
        }
    }
    else 
    {
        rShape = new JPH::ScaledShape(rShape, scale);
    }
}

float SysJolt::get_inverse_mass_no_lock(JPH::PhysicsSystem const &physicsSystem, BodyId bodyId)
{
    const JPH::BodyLockInterfaceNoLock &lockInterface = physicsSystem.GetBodyLockInterfaceNoLock();
    {
        JPH::BodyLockRead lock(lockInterface, BToJolt(bodyId));
        if (lock.Succeeded()) // body_id may no longer be valid
        {
            const JPH::Body &body = lock.GetBody();
            return body.GetMotionProperties()->GetInverseMass();
        }
    }
    return 0.0f;
}

void SysJolt::find_shapes_recurse(
        ACtxPhysics const&                      rCtxPhys,
        ACtxJoltWorld&                          rCtxWorld,
        ACtxSceneGraph const&                   rScnGraph,
        ACompTransformStorage_t const&          rTf,
        ActiveEnt                               ent,
        Matrix4 const&                          transform,
        JPH::CompoundShapeSettings&             rCompound) noexcept
{
    // Add jolt shape if exists
    if (rCtxWorld.m_shapes.contains(ent))
    {
        JPH::Ref<JPH::Shape> rShape = rCtxWorld.m_shapes.get(ent);

        // Set transform relative to root body
        SysJolt::scale_shape(rShape, Vec3MagnumToJolt(transform.scaling()));
        rCompound.AddShape(
            Vec3MagnumToJolt(transform.translation()), 
            QuatMagnumToJolt(osp::Quaternion::fromMatrix(transform.rotation())), 
            rShape);
    }

    if ( ! rCtxPhys.m_hasColliders.contains(ent) )
    {
        return;
    }

    // Recurse into children if there are more shapes
    for (ActiveEnt child : SysSceneGraph::children(rScnGraph, ent))
    {
        if (rTf.contains(child))
        {
            ACompTransform const &rChildTransform = rTf.get(child);

            Matrix4 const childMatrix = transform * rChildTransform.m_transform;

            find_shapes_recurse(
                    rCtxPhys, rCtxWorld, rScnGraph, rTf, child, childMatrix, rCompound);
        }

    }

}

//TODO this is locking on all bodies. Is it bad ?
//The easy fix is to provide multiple step listeners for disjoint sets of bodies, which can then run in parallel.
//It might not be worth it considering this function should be quite fast.
void PhysicsStepListenerImpl::OnStep(float inDeltaTime, JPH::PhysicsSystem &rPhysicsSystem)
{
    JPH::BodyInterface &bodyInterface = rPhysicsSystem.GetBodyInterfaceNoLock();
    JPH::BodyLockInterfaceNoLock const &bodyLockInterface = rPhysicsSystem.GetBodyLockInterfaceNoLock();

    bool const doOriginTranslation = !m_pCtxBasic->m_translateOrigin.isZero();
    JPH::Vec3 const translateOrigin = Vec3MagnumToJolt(m_pCtxBasic->m_translateOrigin);


//    if (!translateOrigin.isZero())
//    {
//        JPH::BodyIDVector allBodiesIds;
//        pJoltWorld->GetBodies(allBodiesIds);

//        JPH::BodyInterface &bodyInterface = pJoltWorld->GetBodyInterface();

//        // Translate every jolt body
//        for (JPH::BodyID bodyId : allBodiesIds)
//        {
//            JPH::RVec3 position = bodyInterface.GetPosition(bodyId);S
//            Matrix4 matrix;
//            position -= Vec3MagnumToJolt(translateOrigin);
//            //As we are translating the whole world, we don't need to wake up asleep bodies.
//            bodyInterface.SetPosition(bodyId, position, JPH::EActivation::DontActivate);
//        }
//    }

    // Apply changed velocities
    for (auto const& [ent, vel] : m_pCtxPhysics->m_setVelocity)
    {
        JPH::BodyID const joltBodyId     = BToJolt(m_pCtxJoltWorld->m_entToBody.at(ent));

        bodyInterface.SetLinearVelocity(joltBodyId, Vec3MagnumToJolt(vel));
    }
    m_pCtxPhysics->m_setVelocity.clear();


    for (BodyId bodyId : m_pCtxJoltWorld->m_bodyIds)
    {
        JPH::BodyID joltBodyId = BToJolt(bodyId);

        bool        updateOspTf     = false;
        bool        forcesDirty     = false;

        JPH::Vec3 newPos;
        JPH::Quat newRot;

        ActiveEnt const ent = m_pCtxJoltWorld->m_bodyToEnt[bodyId];

        JPH::BodyLockWrite lock(bodyLockInterface, joltBodyId);

        JPH::Body &rBody = lock.GetBody();

        if (rBody.IsDynamic())
        {
            updateOspTf = true;

            //Force and torque osp -> jolt
            Vector3 force{0.0f};
            Vector3 torque{0.0f};

            LGRN_ASSERT(ForceFactors_t{}.size() == 64u);
            auto const factorsInts = std::initializer_list<std::uint64_t>{m_pCtxJoltWorld->m_bodyFactors[bodyId].to_ullong()};
            auto const factorsBits = lgrn::bit_view(factorsInts);

            for (std::size_t const factorIdx : factorsBits.ones())
            {
                ACtxJoltWorld::ForceFactorFunc const& factor = m_pCtxJoltWorld->m_factors[factorIdx];
                factor.m_func(bodyId, *m_pCtxJoltWorld, factor.m_userData, force, torque);
            }

            forcesDirty = true;

            rBody.AddForce(Vec3MagnumToJolt(force));
            rBody.AddTorque(Vec3MagnumToJolt(torque));
        }

//        if (doOriginTranslation && JPH::BodyManager::sIsValidBodyPointer(&rBody))
//        {
//            // TODO: SetPositionAndRotationInternal is the only way to set position?
//            //       Some unneeded center-of-mass calculations are involved, can this be fixed?
//            newPos = rBody.GetPosition() - translateOrigin;
//            newRot = rBody.GetRotation();
//            bodyInterface.SetPositionAndRotation(joltBodyId, newPos, newRot, JPH::EActivation::DontActivate);
//            //rBody.SetPositionAndRotationInternal(newPos, newRot, false);
//            updateOspTf = true;
//        }

        if (forcesDirty)
        {
            bodyInterface.ActivateBody(joltBodyId);
        }

        if (updateOspTf)
        {
            JPH::Quat a;
            JPH::Vec3 b;

            JPH::Vec3 comOffset = rBody.GetRotation() * rBody.GetShape()->GetCenterOfMass();

            m_pCtxBasic->m_transform.get(ent).m_transform = Matrix4::from(
                    QuatJoltToMagnum(rBody.GetRotation()).toMatrix(),
                    Vec3JoltToMagnum(rBody.GetCenterOfMassPosition()) - Vec3JoltToMagnum(comOffset));
        }
    }
}
