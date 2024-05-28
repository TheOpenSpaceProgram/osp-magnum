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

#include "newtoninteg_fn.h"          // IWYU pragma: associated

#include <osp/activescene/basic_fn.h>

#include <Newton.h>                  // for NewtonBodySetCollision

#include <utility>                   // for std::exchange
#include <cassert>                   // for assert

// IWYU pragma: no_include <cstddef>
// IWYU pragma: no_include <type_traits>

using namespace ospnewton;

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

using osp::EShape;

using osp::active::ActiveEnt;
using osp::active::ACtxPhysics;
using osp::active::SysSceneGraph;

using osp::Matrix3;
using osp::Matrix4;
using osp::Vector3;


// Callback called for dynamic rigid bodies for applying force and torque
void SysNewton::cb_force_torque(
        NewtonBody const* pBody, dFloat const timestep, NwtThreadIndex_t const thread)
{
    ACtxNwtWorld &rWorldCtx = SysNewton::context_from_nwtbody(pBody);
    BodyId const bodyId     = SysNewton::get_userdata_bodyid(pBody);

    Vector3 force{0.0f};
    Vector3 torque{0.0f};

    auto factorBits = lgrn::bit_view(rWorldCtx.m_bodyFactors[bodyId]);
    for (std::size_t const factorIdx : factorBits.ones())
    {
        ACtxNwtWorld::ForceFactorFunc const& factor = rWorldCtx.m_factors[factorIdx];

        factor.m_func(pBody, bodyId, rWorldCtx, factor.m_userData, force, torque);
    }

    NewtonBodySetForce(pBody, force.data());
    NewtonBodySetTorque(pBody, torque.data());
} // cb_force_torque()

void SysNewton::cb_set_transform(
        NewtonBody const* pBody, dFloat const* const pMatrix, NwtThreadIndex_t const thread)
{
    ACtxNwtWorld &rWorldCtx = SysNewton::context_from_nwtbody(pBody);
    BodyId const bodyId     = SysNewton::get_userdata_bodyid(pBody);

    ActiveEnt const ent = rWorldCtx.m_bodyToEnt[bodyId];

    NewtonBodyGetMatrix(pBody, rWorldCtx.m_pTransform->get(ent).m_transform.data());
} // cb_set_transform()


void SysNewton::resize_body_data(ACtxNwtWorld& rCtxWorld)
{
    std::size_t const capacity = rCtxWorld.m_bodyIds.capacity();
    rCtxWorld.m_bodyPtrs    .resize(capacity);
    rCtxWorld.m_bodyToEnt   .resize(capacity);
    rCtxWorld.m_bodyFactors .resize(capacity);
}

NwtColliderPtr_t SysNewton::create_primative(
        ACtxNwtWorld&   rCtxWorld,
        EShape          shape)
{
    NewtonWorld* pNwtWorld = rCtxWorld.m_world.get();
    NewtonCollision* pCollision;
    switch (shape)
    {
    case EShape::Sphere:
        pCollision = NewtonCreateSphere(pNwtWorld, 1.0f, 0, nullptr);
        break;
    case EShape::Box:
        pCollision = NewtonCreateBox(pNwtWorld, 2, 2, 2, 0, nullptr);
        break;
    case EShape::Cylinder:
        pCollision = NewtonCreateCylinder(pNwtWorld, 1, 1, 2, 0, nullptr);
        break;
    default:
        // TODO: support other shapes, sphere is used for now
        pCollision = NewtonCreateSphere(pNwtWorld, 1.0f, 0, nullptr);
        break;
    }

    return NwtColliderPtr_t{pCollision};
}

void SysNewton::orient_collision(
        NewtonCollision const*  pCollision,
        osp::EShape             shape,
        osp::Vector3 const&     translation,
        osp::Matrix3 const&     rotation,
        osp::Vector3 const&     scale)
{
    if (shape != EShape::Cylinder)
    {
        Matrix4 const matrix = Matrix4::from(rotation, translation);
        NewtonCollisionSetMatrix(pCollision, matrix.data());
        NewtonCollisionSetScale(pCollision, scale.x(), scale.y(), scale.z());
    }
    else
    {
        static constexpr Matrix3 const rotX90deg
        {
            { 0.0f,  0.0f, -1.0f},
            { 0.0f,  1.0f,  0.0f},
            { 1.0f,  0.0f,  0.0f}
        };
        Matrix4 const matrix = Matrix4::from(rotation * rotX90deg, translation);
        NewtonCollisionSetMatrix(pCollision, matrix.data());
        NewtonCollisionSetScale(pCollision, scale.z(), scale.y(), scale.x());
    }
}

void SysNewton::update_translate(ACtxPhysics& rCtxPhys, ACtxNwtWorld& rCtxWorld) noexcept
{
    NewtonWorld const* pNwtWorld = rCtxWorld.m_world.get();

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

using Corrade::Containers::ArrayView;

void SysNewton::update_world(
        ACtxPhysics&                rCtxPhys,
        ACtxNwtWorld&               rCtxWorld,
        float                       timestep,
        ACtxSceneGraph const&       rScnGraph,
        ACompTransformStorage_t&    rTf) noexcept
{
    NewtonWorld const* pNwtWorld = rCtxWorld.m_world.get();

    // Apply changed velocities
    for (auto const& [ent, vel] : std::exchange(rCtxPhys.m_setVelocity, {}))
    {
        BodyId const bodyId     = rCtxWorld.m_entToBody.at(ent);
        NewtonBody const *pBody = rCtxWorld.m_bodyPtrs[bodyId].get();

        NewtonBodySetVelocity(pBody, vel.data());
    }

    rCtxWorld.m_pTransform = std::addressof(rTf);

    // Update the world
    NewtonUpdate(pNwtWorld, timestep);
}

void SysNewton::remove_components(ACtxNwtWorld& rCtxWorld, ActiveEnt ent) noexcept
{
    auto itBodyId = rCtxWorld.m_entToBody.find(ent);

    if (itBodyId != rCtxWorld.m_entToBody.end())
    {
        BodyId const bodyId = itBodyId->second;
        rCtxWorld.m_bodyPtrs[bodyId].reset();
        rCtxWorld.m_bodyToEnt[bodyId] = lgrn::id_null<ActiveEnt>();
        rCtxWorld.m_entToBody.erase(itBodyId);
    }

    rCtxWorld.m_colliders.remove(ent);
}


void SysNewton::find_colliders_recurse(
        ACtxPhysics const&                      rCtxPhys,
        ACtxNwtWorld&                           rCtxWorld,
        ACtxSceneGraph const&                   rScnGraph,
        ACompTransformStorage_t const&          rTf,
        ActiveEnt                               ent,
        Matrix4 const&                          transform,
        NewtonCollision*                        pCompound) noexcept
{
    // Add newton collider if exists
    if (rCtxWorld.m_colliders.contains(ent))
    {
        NewtonCollision const *pCollision
                = rCtxWorld.m_colliders.get(ent).get();

        // Set transform relative to root body

        // cylinder needs to be rotated 90 degrees Z to aligned with Y axis
        // TODO: replace this with something more sophisticated some time
        Matrix4 const &colliderTf
                = (rCtxPhys.m_shape[ent] != EShape::Cylinder)
                ? transform : transform * Matrix4::rotationZ(90.0_degf);

        Matrix4 const normScale = Matrix4::from(colliderTf.rotation(), colliderTf.translation());

        NewtonCollisionSetMatrix(pCollision, normScale.data());

        Vector3 const scale = colliderTf.scaling();
        NewtonCollisionSetScale(pCollision, scale.x(), scale.y(), scale.z());

        // Add body to compound collision
        NewtonCompoundCollisionAddSubCollision(pCompound, pCollision);
    }

    if ( ! rCtxPhys.m_hasColliders.contains(ent) )
    {
        return;
    }

    // Recurse into children if there are more colliders
    for (ActiveEnt child : SysSceneGraph::children(rScnGraph, ent))
    {
        if (rTf.contains(child))
        {
            ACompTransform const &rChildTransform = rTf.get(child);

            Matrix4 const childMatrix = transform * rChildTransform.m_transform;

            find_colliders_recurse(
                    rCtxPhys, rCtxWorld, rScnGraph, rTf, child, childMatrix, pCompound);
        }

    }

}


