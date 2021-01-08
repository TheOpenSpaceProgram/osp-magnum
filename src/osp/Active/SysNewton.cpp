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

#include "ActiveScene.h"

#include <Newton.h>

using osp::active::SysNewton;

using osp::active::ActiveScene;
using osp::active::ActiveEnt;

using osp::active::ACompNwtBody;
using osp::active::ACompNwtWorld;
using osp::active::ACompTransform;

const std::string SysNewton::smc_name = "NewtonPhysics";

// Callback called for every Rigid Body (even static ones) on NewtonUpdate
void cb_force_torque(const NewtonBody* pBody, dFloat timestep, int threadIndex)
{
    // Get Scene from Newton World
    ActiveScene* scene = static_cast<ActiveScene*>(
                NewtonWorldGetUserData(NewtonBodyGetWorld(pBody)));

    // Get ACompNwtBody. Every NewtonBody created here is associated with an
    // entity that contains one.
    auto *pBodyComp = static_cast<ACompNwtBody*>(NewtonBodyGetUserData(pBody));
    auto &rTransformComp = scene->reg_get<ACompTransform>(pBodyComp->m_entity);

    // Check if transform has been set externally
    if (rTransformComp.m_transformDirty)
    {

        // Set matrix
        NewtonBodySetMatrix(pBody, rTransformComp.m_transform.data());

        rTransformComp.m_transformDirty = false;
    }

    // TODO: deal with changing inertia, mass or stuff

    // Apply force and torque
    NewtonBodySetForce(pBody, pBodyComp->m_netForce.data());
    NewtonBodySetTorque(pBody, pBodyComp->m_netTorque.data());

    // Reset accumolated net force and torque for next frame
    pBodyComp->m_netForce = {0.0f, 0.0f, 0.0f};
    pBodyComp->m_netTorque = {0.0f, 0.0f, 0.0f};
}

ACompNwtBody::ACompNwtBody(ACompNwtBody&& move) noexcept
 : DataRigidBody(std::move(move))
 , m_body(std::exchange(move.m_body, nullptr))
 , m_entity(std::exchange(move.m_entity, entt::null))
{
    if (m_body != nullptr)
    {
        NewtonBodySetUserData(m_body, this);
    }
}

ACompNwtBody& ACompNwtBody::operator=(ACompNwtBody&& move) noexcept
{
    m_body = std::exchange(move.m_body, nullptr);
    m_entity = std::exchange(move.m_entity, entt::null);

    if (m_body != nullptr)
    {
        NewtonBodySetUserData(m_body, this);
    }

    return *this;
}

SysNewton::SysNewton(ActiveScene &scene)
 : m_scene(scene)
 , m_updatePhysicsWorld(scene.get_update_order(), "physics", "wire", "",
                [this] (ActiveScene& rScene) { this->update_world(rScene); })
{
    //std::cout << "sysnewtoninit\n";
    //NewtonWorldSetUserData(m_nwtWorld, this);

    // Connect signal handlers to destruct Newton objects when components are
    // deleted.

    scene.get_registry().on_destroy<ACompNwtBody>()
                    .connect<&SysNewton::on_body_destruct>();

    scene.get_registry().on_destroy<ACompCollisionShape>()
                    .connect<&SysNewton::on_shape_destruct>();

    scene.get_registry().on_destroy<ACompNwtWorld>()
                    .connect<&SysNewton::on_world_destruct>();
}

SysNewton::~SysNewton()
{
    // Clean up newton dynamics stuff
    m_scene.get_registry().clear<ACompNwtBody>();
    m_scene.get_registry().clear<ACompCollisionShape>();
}

void SysNewton::update_world(ActiveScene& rScene)
{
    // Try getting the newton world from the scene root.
    // It is very unlikely that multiple physics worlds are ever needed
    auto *worldComp = rScene.get_registry().try_get<ACompNwtWorld>(
                                 rScene.hier_get_root());

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

    auto viewBody = rScene.get_registry().view<ACompNwtBody>();
    auto viewBodyTransform = rScene.get_registry().view<ACompNwtBody,
                                                         ACompTransform>();

    // Iterate just the entities with ACompNwtBody
    for (ActiveEnt ent : viewBody)
    {
        auto &entBody = viewBodyTransform.get<ACompNwtBody>(ent);

        // temporary: delete if something is dirty
        if (entBody.m_colliderDirty)
        {
            NewtonDestroyBody(std::exchange(entBody.m_body, nullptr));
            entBody.m_colliderDirty = false;
        }

        if (entBody.m_body == nullptr)
        {
            // Initialize body if not done so yet;
            create_body(rScene, ent, nwtWorld);
        }
    }

    // Update the world
    NewtonUpdate(nwtWorld, rScene.get_time_delta_fixed());

    // Apply transform changes after the Newton world(s) updates
    for (ActiveEnt ent : viewBodyTransform)
    {
        auto &entBody      = viewBodyTransform.get<ACompNwtBody>(ent);
        auto &entTransform = viewBodyTransform.get<ACompTransform>(ent);

        if (entBody.m_body)
        {
            // Get new transform matrix from newton
            NewtonBodyGetMatrix(entBody.m_body,
                                entTransform.m_transform.data());
        }
    }
}

void SysNewton::find_colliders_recurse(ActiveScene& rScene, ActiveEnt ent,
                                       Matrix4 const &transform,
                                       NewtonWorld const* nwtWorld,
                                       NewtonCollision *rCompound)
{
    ActiveEnt nextChild = ent;

    // iterate through siblings of ent
    while(nextChild != entt::null)
    {
        auto const &childHeir = rScene.reg_get<ACompHierarchy>(nextChild);
        auto const &childTransform = rScene.reg_get<ACompTransform>(nextChild);

        auto* childCollide = rScene.get_registry()
                                .try_get<ACompCollisionShape>(nextChild);

        Matrix4 childMatrix = transform * childTransform.m_transform;

        if (childCollide != nullptr)
        {
            NewtonCollision *collision = childCollide->m_collision;

            if (collision == nullptr)
            {
                // Newton collider has not yet been created from component

                // TODO: care about collision shape. for now, everything is a
                //       sphere
                collision = NewtonCreateSphere(nwtWorld, 0.5f, 0, NULL);
                childCollide->m_collision = collision;
            }

            //Matrix4 nextTransformNorm = nextTransform.

            // Set transform relative to root body
            Matrix4 f = Matrix4::translation(childMatrix.translation());
            NewtonCollisionSetMatrix(collision, f.data());

            // Add body to compound collision
            NewtonCompoundCollisionAddSubCollision(rCompound, collision);

        }
        else
        {
            //return;
        }

        find_colliders_recurse(rScene, childHeir.m_childFirst, childMatrix,
                               nwtWorld, rCompound);
        nextChild = childHeir.m_siblingNext;
    }
}

void SysNewton::create_body(ActiveScene& rScene, ActiveEnt entity,
                            NewtonWorld const* nwtWorld)
{

    auto const& entHeir      = rScene.reg_get<ACompHierarchy>(entity);
    auto& entTransform = rScene.reg_get<ACompTransform>(entity);

    auto& entBody  = rScene.reg_get<ACompNwtBody>(entity);
    auto* entShape = rScene.get_registry().try_get<ACompCollisionShape>(entity);

    if (entShape == nullptr)
    {
        // Entity must also have a collision shape to make a body
        return;
    }

    switch (entShape->m_shape)
    {
    case ECollisionShape::COMBINED:
    {
        // Combine collision shapes from descendants

        NewtonCollision* rCompound
                = NewtonCreateCompoundCollision(nwtWorld, 0);

        //
        NewtonCompoundCollisionBeginAddRemove(rCompound);
        find_colliders_recurse(rScene, entHeir.m_childFirst, Matrix4(),
                               nwtWorld, rCompound);
        NewtonCompoundCollisionEndAddRemove(rCompound);

        // TODO: deal with mass and stuff

        if (entBody.m_body != nullptr)
        {
            NewtonBodySetCollision(entBody.m_body, rCompound);
        }
        else
        {
            entBody.m_body = NewtonCreateDynamicBody(nwtWorld, rCompound,
                                                     Matrix4().data());
        }

        NewtonDestroyCollision(rCompound);

        // Set inertia and mass
        NewtonBodySetMassMatrix(entBody.m_body, entBody.m_mass,
                                entBody.m_intertia.x(),
                                entBody.m_intertia.x(),
                                entBody.m_intertia.z());

        break;
    }
    case ECollisionShape::TERRAIN:
    {
        // Get NewtonTreeCollision generated from elsewhere
        // such as SysPlanetA::debug_create_chunk_collider

        if (entShape->m_collision)
        {
            if (entBody.m_body)
            {
                NewtonBodySetCollision(entBody.m_body, entShape->m_collision);
            }
            else
            {
                entBody.m_body = NewtonCreateDynamicBody(nwtWorld,
                                                         entShape->m_collision,
                                                         Matrix4().data());
            }


        }
        else
        {
            // make a collision shape somehow
        }
    }
    default:
        break;
    }

    entTransform.m_controlled = true;
    entBody.m_entity = entity;

    // Set position/rotation
    NewtonBodySetMatrix(entBody.m_body, entTransform.m_transform.data());

    // Set damping to 0, as default is 0.1
    // reference frame may be moving and air pressure stuff
    NewtonBodySetLinearDamping(entBody.m_body, 0.0f);

    // Make it easier to rotate
    NewtonBodySetAngularDamping(entBody.m_body,
                                Vector3(1.0f, 1.0f, 1.0f).data());

    // Set callback for updating position of entity and everything else
    NewtonBodySetForceAndTorqueCallback(entBody.m_body, cb_force_torque);

    // Set user data
    //NwtUserData *data = new NwtUserData(entity, m_scene);
    NewtonBodySetUserData(entBody.m_body, &entBody);

    // don't leak memory
    //NewtonDestroyCollision(ball);
}

ACompNwtWorld* SysNewton::try_get_physics_world(ActiveScene &rScene)
{
    return rScene.get_registry().try_get<ACompNwtWorld>(rScene.hier_get_root());
}

std::pair<ActiveEnt, ACompNwtBody*> SysNewton::find_rigidbody_ancestor(
        ActiveScene& rScene, ActiveEnt ent)
{
    ActiveEnt prevEnt, currEnt = ent;
    ACompHierarchy *currHeir = nullptr;

    do
    {

       currHeir = rScene.get_registry().try_get<ACompHierarchy>(currEnt);

        if (!currHeir)
        {
            return {entt::null, nullptr};
        }

        prevEnt = currEnt;
        currEnt = currHeir->m_parent;
    }
    while (currHeir->m_level != gc_heir_physics_level);

    auto *body = rScene.get_registry().try_get<ACompNwtBody>(prevEnt);

    return {prevEnt, body};

}

void SysNewton::body_apply_force(ACompRigidBody_t &body, Vector3 force) noexcept
{
    body.m_netForce += force;
}

void SysNewton::body_apply_accel(ACompRigidBody_t &body, Vector3 accel) noexcept
{
    body_apply_force(body, accel * body.m_mass);
}

void SysNewton::body_apply_torque(ACompRigidBody_t &body, Vector3 torque) noexcept
{
    body.m_netTorque += torque;
}

void SysNewton::on_body_destruct(ActiveReg_t& reg, ActiveEnt ent)
{
    NewtonBody const *body = reg.get<ACompNwtBody>(ent).m_body;
    if (body)
    {
        NewtonDestroyBody(body); // make sure the Newton body is destroyed
    }
}

void SysNewton::on_shape_destruct(ActiveReg_t& reg, ActiveEnt ent)
{
    NewtonCollision const *shape = reg.get<ACompCollisionShape>(ent).m_collision;
    if (shape != nullptr)
    {
        NewtonDestroyCollision(shape); // make sure the shape is destroyed
    }
}

void SysNewton::on_world_destruct(ActiveReg_t& reg, ActiveEnt ent)
{
    NewtonWorld const *world = reg.get<ACompNwtWorld>(ent).m_nwtWorld;
    if (world != nullptr)
    {
        NewtonDestroyAllBodies(world);
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

