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

void SysJolt::resize_body_data(ACtxJoltWorld& rCtxWorld)
{
    std::size_t const capacity = rCtxWorld.m_bodyIds.capacity();
}


void SysJolt::update_translate(ACtxPhysics& rCtxPhys, ACtxJoltWorld& rCtxWorld) noexcept
{

    // Origin translation
    if (Vector3 const translate = std::exchange(rCtxPhys.m_originTranslate, {});
        ! translate.isZero())
    {
        PhysicsSystem* pJoltWorld = rCtxWorld.m_pPhysicsSystem.get();
        BodyIDVector allBodiesIds;
        pJoltWorld->GetBodies(allBodiesIds);

        BodyInterface &bodyInterface = pJoltWorld->GetBodyInterface();

        // Translate every jolt body
        for (BodyID bodyId : allBodiesIds)
        {
            RVec3 position = bodyInterface.GetPosition(bodyId);
            Matrix4 matrix;
            position += Vec3MagnumToJolt(translate);
            //As we are translating the whole world, we don't need to wake up asleep bodies. 
            bodyInterface.SetPosition(bodyId, position, EActivation::DontActivate);
        }
    }
}

using Corrade::Containers::ArrayView;

void SysJolt::update_world(
        ACtxPhysics&                rCtxPhys,
        ACtxJoltWorld&              rCtxWorld,
        float                       timestep,
        ACompTransformStorage_t&    rTf) noexcept
{
    PhysicsSystem *pJoltWorld = rCtxWorld.m_pPhysicsSystem.get();
    BodyInterface &bodyInterface = pJoltWorld->GetBodyInterface();

    // Apply changed velocities
    for (auto const& [ent, vel] : std::exchange(rCtxPhys.m_setVelocity, {}))
    {
        JPH::BodyID const bodyId     = BToJolt(rCtxWorld.m_entToBody.at(ent));

        bodyInterface.SetLinearVelocity(bodyId, Vec3MagnumToJolt(vel));
    }

    rCtxWorld.m_pTransform = std::addressof(rTf);

    int collisionSteps = 1;
    pJoltWorld->Update(timestep, collisionSteps, &rCtxWorld.m_temp_allocator, rCtxWorld.m_joltJobSystem.get());
}

void SysJolt::remove_components(ACtxJoltWorld& rCtxWorld, ActiveEnt ent) noexcept
{
    auto itBodyId = rCtxWorld.m_entToBody.find(ent);
    BodyInterface &bodyInterface = rCtxWorld.m_pPhysicsSystem->GetBodyInterface();

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

Ref<Shape> SysJolt::create_primitive(ACtxJoltWorld &rCtxWorld, osp::EShape shape, Vec3Arg scale)
{
    switch (shape)
    {
    case EShape::Sphere:
    //Sphere only support uniform shape
        return SphereShapeSettings(1.0f * scale.GetX()).Create().Get();
    case EShape::Box:
        return BoxShapeSettings(scale).Create().Get();
    case EShape::Cylinder:
        //cylinder needs to be internally rotated 90° to match with graphics
        return  RotatedTranslatedShapeSettings(
                    Vec3Arg::sZero(), 
                    Quat::sRotation(Vec3::sAxisX(), JPH_PI/2), 
                    new CylinderShapeSettings(scale.GetZ(), 2.0f * scale.GetX())
                ).Create().Get();
        
    default:
        return SphereShapeSettings(1.0f * scale.GetX()).Create().Get();
    }
}

void SysJolt::scale_shape(Ref<Shape> rShape, Vec3Arg scale) {
    if (rShape->GetSubType() == EShapeSubType::Scaled) {
        ScaledShape* rScaledShape = dynamic_cast<ScaledShape*>(rShape.GetPtr());
        if (rScaledShape != nullptr) 
        {
            rShape = new ScaledShape(rScaledShape->GetInnerShape(), scale * rScaledShape->GetScale());
        }
    }
    else 
    {
        rShape = new ScaledShape(rShape, scale);
    }
}

float SysJolt::get_inverse_mass_no_lock(PhysicsSystem &physicsSystem,
                                        BodyId bodyId) {
  const BodyLockInterfaceNoLock &lockInterface =
      physicsSystem.GetBodyLockInterfaceNoLock();
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
        CompoundShapeSettings&                  rCompound) noexcept
{
    // Add jolt shape if exists
    if (rCtxWorld.m_shapes.contains(ent))
    {
        Ref<Shape> rShape = rCtxWorld.m_shapes.get(ent);

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
void PhysicsStepListenerImpl::OnStep(float inDeltaTime, PhysicsSystem &rPhysicsSystem)
{
    //no lock as all bodies are already locked
    BodyInterface &bodyInterface = rPhysicsSystem.GetBodyInterfaceNoLock();
    for (BodyId bodyId : m_context->m_bodyIds)
    {
        JPH::BodyID joltBodyId = BToJolt(bodyId);
        if (bodyInterface.GetMotionType(joltBodyId) != EMotionType::Dynamic) 
        {
            continue;
        }

        //Transform jolt -> osp


        ActiveEnt const ent = m_context->m_bodyToEnt[bodyId];
        Mat44 worldTranform = bodyInterface.GetWorldTransform(joltBodyId);

        worldTranform.StoreFloat4x4((Float4*)m_context->m_pTransform->get(ent).m_transform.data());

        //Force and torque osp -> jolt
        Vector3 force{0.0f};
        Vector3 torque{0.0f};

        auto factorBits = lgrn::bit_view(m_context->m_bodyFactors[bodyId]);
        for (std::size_t const factorIdx : factorBits.ones())
        {   
            ACtxJoltWorld::ForceFactorFunc const& factor = m_context->m_factors[factorIdx];
            factor.m_func(bodyId, *m_context, factor.m_userData, force, torque);
        }
        Vec3 vel = bodyInterface.GetLinearVelocity(joltBodyId);

        bodyInterface.AddForceAndTorque(joltBodyId, Vec3MagnumToJolt(force), Vec3MagnumToJolt(torque));
    }
}