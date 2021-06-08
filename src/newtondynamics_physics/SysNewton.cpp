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

#include "SysNewton.h"

#include <osp/Active/SysPhysics.h>
#include <osp/Active/SysHierarchy.h>

#include <Newton.h>

using ospnewton::SysNewton;
using ospnewton::ACompNwtBody;
using ospnewton::ACompNwtWorld;

using osp::active::ActiveScene;
using osp::active::ActiveReg_t;
using osp::active::ActiveEnt;

using osp::active::SysPhysics;

using osp::active::ACompHierarchy;
using osp::active::ACompRigidBody;
using osp::active::ACompShape;
using osp::active::ACompSolidCollider;
using osp::active::ACompTransform;
using osp::active::ACompTransformControlled;
using osp::active::ACompTransformMutable;

using osp::Matrix4;
using osp::Vector3;

// Callback called for every Rigid Body (even static ones) on NewtonUpdate
void cb_force_torque(const NewtonBody* pBody, dFloat timestep, int threadIndex)
{
    // Get Scene from Newton World
    ActiveScene* scene = static_cast<ActiveScene*>(
                NewtonWorldGetUserData(NewtonBodyGetWorld(pBody)));

    // Get Associated entity
    uintptr_t userData = reinterpret_cast<uintptr_t>(NewtonBodyGetUserData(pBody));
    ActiveEnt ent = static_cast<ActiveEnt>(userData);

    auto &rBody = scene->reg_get<ACompRigidBody>(ent);
    auto &rTransformComp = scene->reg_get<ACompTransform>(ent);

    // Check if transform has been set externally
    auto &tfMutable = scene->reg_get<ACompTransformMutable>(ent);
    if (tfMutable.m_dirty)
    {
        // Set matrix
        NewtonBodySetMatrix(pBody, rTransformComp.m_transform.data());

        tfMutable.m_dirty = false;
    }

    // TODO: deal with changing inertia, mass or stuff

    // Apply force and torque
    NewtonBodySetForce(pBody, rBody.m_netForce.data());
    NewtonBodySetTorque(pBody, rBody.m_netTorque.data());

    // Reset accumolated net force and torque for next frame
    rBody.m_netForce = {0.0f, 0.0f, 0.0f};
    rBody.m_netTorque = {0.0f, 0.0f, 0.0f};
}

void SysNewton::add_functions(ActiveScene &rScene)
{
    SPDLOG_LOGGER_INFO(rScene.get_application().get_logger(),
                      "Initing Sysnewton");    rScene.debug_update_add(rScene.get_update_order(), "physics", "wire", "",
                            &SysNewton::update_world);    //NewtonWorldSetUserData(m_nwtWorld, this);

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
    auto *worldComp = rReg.try_get<ACompNwtWorld>(rScene.hier_get_root());

    if (worldComp == nullptr)
    {
        return; // No physics world component
    }

    if (worldComp->m_nwtWorld == nullptr)
    {
        // Create world if not yet initialized
        worldComp->m_nwtWorld = NewtonCreate();
        NewtonWorldSetUserData(worldComp->m_nwtWorld, &rScene);
    }

    NewtonWorld const* nwtWorld = worldComp->m_nwtWorld;

    auto viewBody = rScene.get_registry().view<ACompRigidBody>();

    // Iterate all rigid bodies in scene
    for (ActiveEnt ent : viewBody)
    {
        auto &entBody = viewBody.get<ACompRigidBody>(ent);

        // temporary: delete Newton Body if something is dirty
        if (entBody.m_colliderDirty)
        {
            rReg.remove_if_exists<ACompNwtBody>(ent);
            entBody.m_colliderDirty = false;
        }

        // Initialize body if not done so yet;
        if (! rReg.any_of<ACompNwtBody>(ent))
        {
            create_body(rScene, ent, nwtWorld);
        }

        // Recompute inertia and center of mass if rigidbody has been modified
        if (entBody.m_inertiaDirty)
        {
            compute_rigidbody_inertia(rScene, ent);
            SPDLOG_LOGGER_TRACE(m_scene.get_application().get_logger(), "Updating RB : new CoM Z = {}",
                              entBody.m_centerOfMassOffset.z());
        }

    }

    // Update the world
    NewtonUpdate(nwtWorld, rScene.get_time_delta_fixed());

    auto viewNwtBody = rScene.get_registry().view<ACompNwtBody>();
    auto viewTf = rScene.get_registry().view<ACompTransform>();

    // Apply transform changes after the Newton world(s) updates
    for (ActiveEnt ent : viewBody)
    {
        auto &entNwtBody      = viewNwtBody.get<ACompNwtBody>(ent);
        auto &entTransform    = viewTf.get<ACompTransform>(ent);

        float mass, x, y, z;
        NewtonBodyGetMass(entNwtBody.body(), &mass, &x, &y, &z);

        // Get new transform matrix from newton
        NewtonBodyGetMatrix(entNwtBody.body(), entTransform.m_transform.data());
    }
}

void SysNewton::find_colliders_recurse(
        ActiveScene& rScene, ActiveEnt ent, Matrix4 const &transform,
        NewtonWorld const* nwtWorld, NewtonCollision *rCompound)
{
    ActiveReg_t &rReg = rScene.get_registry();
    ActiveEnt nextChild = ent;

    // iterate through siblings of ent
    while(nextChild != entt::null)
    {
        auto const &childHeir = rScene.reg_get<ACompHierarchy>(nextChild);
        auto const *childTransform = rScene.get_registry().try_get<ACompTransform>(nextChild);

        if (childTransform != nullptr)
        {
            Matrix4 childMatrix = transform * childTransform->m_transform;

            if (rReg.all_of<ACompSolidCollider>(nextChild))
            {
                auto* childNwtCollider = rReg.try_get<ACompNwtCollider>(nextChild);

                NewtonCollision const *collision;

                if (nullptr == childNwtCollider)
                {
                    // No Newton collider exists yet
                    collision = NewtonCreateSphere(nwtWorld, 0.5f, 0, NULL);
                    rReg.emplace<ACompNwtCollider>(nextChild, collision);
                    ACompNwtCollider f(collision);
                }
                else
                {
                    // Already has Newton collider
                    collision = childNwtCollider->collision();
                }

                // Set transform relative to root body
                Matrix4 f = Matrix4::translation(childMatrix.translation());
                NewtonCollisionSetMatrix(collision, f.data());

                // Add body to compound collision
                NewtonCompoundCollisionAddSubCollision(rCompound, collision);
            }

            find_colliders_recurse(rScene, childHeir.m_childFirst, childMatrix,
                                   nwtWorld, rCompound);
        }

        nextChild = childHeir.m_siblingNext;
    }
}

void SysNewton::create_body(ActiveScene& rScene, ActiveEnt ent,
                            NewtonWorld const* nwtWorld)
{
    ActiveReg_t &rReg = rScene.get_registry();

    auto const& entHier = rReg.get<ACompHierarchy>(ent);
    auto const& entTransform = rReg.get<ACompTransform>(ent);
    auto const* entShape = rReg.try_get<ACompShape>(ent);

    if ((! rReg.all_of<ACompSolidCollider>(ent)) || (entShape == nullptr))
    {
        // Entity must have a collision shape to define the collision volume
        return;
    }

    NewtonBody const* body;

    switch (entShape->m_shape)
    {
    case osp::phys::ECollisionShape::COMBINED:
    {
        // Combine collision shapes from descendants

        NewtonCollision* rCompound
                = NewtonCreateCompoundCollision(nwtWorld, 0);

        NewtonCompoundCollisionBeginAddRemove(rCompound);
        find_colliders_recurse(rScene, entHier.m_childFirst, Matrix4{},
                               nwtWorld, rCompound);
        NewtonCompoundCollisionEndAddRemove(rCompound);

        if (auto* entNwtBody = rReg.try_get<ACompNwtBody>(ent);
            nullptr != entNwtBody)
        {
            body = entNwtBody->body();
            NewtonBodySetCollision(body, rCompound);
        }
        else
        {
            Matrix4 identity{};

            body = NewtonCreateDynamicBody(
                        nwtWorld, rCompound, identity.data());
            rReg.emplace<ACompNwtBody>(ent, body);
        }

        // this doesn't actually destroy, it decrements a reference count
        NewtonDestroyCollision(rCompound);

        // Update center of mass, moments of inertia
        compute_rigidbody_inertia(rScene, ent);

        break;
    }
    case osp::phys::ECollisionShape::TERRAIN:
    {
        // Get NewtonTreeCollision generated from elsewhere
        // such as SysPlanetA::debug_create_chunk_collider

        if (auto const* entNwtCollider = rReg.try_get<ACompNwtCollider>(ent);
            nullptr != entNwtCollider)
        {
            if (auto* entNwtBody = rReg.try_get<ACompNwtBody>(ent);
                nullptr != entNwtBody)
            {
                body = entNwtBody->body();
                NewtonBodySetCollision(body, entNwtCollider->collision());
            }
            else
            {
                Matrix4 identity{};

                body = NewtonCreateDynamicBody(
                        nwtWorld, entNwtCollider->collision(), identity.data());
                rReg.emplace<ACompNwtBody>(ent, body);
            }
        }
        else
        {
            // make a collision shape somehow
            body = nullptr;
        }
    }
    default:
        break;
    }

    // by now, the Newton rigid body components now exist

    rScene.get_registry().emplace_or_replace<ACompTransformControlled>(ent);
    rScene.get_registry().emplace_or_replace<ACompTransformMutable>(ent);

    // Set position/rotation
    NewtonBodySetMatrix(body, entTransform.m_transform.data());

    // Set damping to 0, as default is 0.1
    // reference frame may be moving and air pressure stuff
    NewtonBodySetLinearDamping(body, 0.0f);

    // Make it easier to rotate
    NewtonBodySetAngularDamping(body, Vector3(1.0f, 1.0f, 1.0f).data());

    // Set callback for updating position of entity and everything else
    NewtonBodySetForceAndTorqueCallback(body, cb_force_torque);

    // Set user data to entity
    // WARNING: this will cause issues if 64-bit entities are used on a
    //          32-bit machine
    NewtonBodySetUserData(body, reinterpret_cast<void*>(ent));
}

void SysNewton::compute_rigidbody_inertia(ActiveScene& rScene, ActiveEnt entity)
{
    auto& entHier = rScene.reg_get<ACompHierarchy>(entity);
    auto& entBody = rScene.reg_get<ACompRigidBody>(entity);
    auto& entNwtBody = rScene.reg_get<ACompNwtBody>(entity);

    // Compute moments of inertia, mass, and center of mass
    auto const& [inertia, centerOfMass]
            = SysPhysics::compute_hier_inertia(rScene, entity);
    entBody.m_centerOfMassOffset = centerOfMass.xyz();

    // Set inertia and mass
    entBody.m_mass = centerOfMass.w();
    entBody.m_inertia.x() = inertia[0][0];  // Ixx
    entBody.m_inertia.y() = inertia[1][1];  // Iyy
    entBody.m_inertia.z() = inertia[2][2];  // Izz

    NewtonBodySetMassMatrix(entNwtBody.body(), entBody.m_mass,
        entBody.m_inertia.x(),
        entBody.m_inertia.y(),
        entBody.m_inertia.z());
    NewtonBodySetCentreOfMass(entNwtBody.body(), centerOfMass.xyz().data());

    entBody.m_inertiaDirty = false;
    SPDLOG_LOGGER_TRACE(m_scene.get_application().get_logger(), "New mass: {}",
                        entBody.m_mass);
}

ACompNwtWorld* SysNewton::try_get_physics_world(ActiveScene &rScene)
{
    return rScene.get_registry().try_get<ACompNwtWorld>(rScene.hier_get_root());
}

void SysNewton::on_body_destruct(ActiveReg_t& reg, ActiveEnt ent)
{
    NewtonBody const *body = reg.get<ACompNwtBody>(ent).body();
    NewtonDestroyBody(body); // make sure the Newton body is destroyed
}

void SysNewton::on_shape_destruct(ActiveReg_t& reg, ActiveEnt ent)
{
    NewtonCollision const *shape = reg.get<ACompNwtCollider>(ent).collision();
    NewtonDestroyCollision(shape); // make sure the shape is destroyed
}

void SysNewton::on_world_destruct(ActiveReg_t& reg, ActiveEnt ent)
{
    NewtonWorld const *world = reg.get<ACompNwtWorld>(ent).m_nwtWorld;
    if (world != nullptr)
    {
        // delete all collision shapes first to prevent crash
        reg.clear<ACompNwtCollider>();

        // delete all rigid bodies too
        reg.clear<ACompNwtBody>();

        NewtonDestroy(world); // make sure the world is destroyed
    }
}

NewtonCollision* SysNewton::newton_create_tree_collision(
        const NewtonWorld *newtonWorld, int shapeId)
{
    return NewtonCreateTreeCollision(newtonWorld, shapeId);
}

void SysNewton::newton_tree_collision_add_face(
        const NewtonCollision* treeCollision, int vertexCount,
        const float* vertexPtr, int strideInBytes, int faceAttribute)
{
    NewtonTreeCollisionAddFace(treeCollision, vertexCount, vertexPtr,
                               strideInBytes, faceAttribute);
}

void SysNewton::newton_tree_collision_begin_build(
        const NewtonCollision* treeCollision)
{
    NewtonTreeCollisionBeginBuild(treeCollision);
}

void SysNewton::newton_tree_collision_end_build(
        const NewtonCollision* treeCollision, int optimize)
{
    NewtonTreeCollisionEndBuild(treeCollision, optimize);
}

