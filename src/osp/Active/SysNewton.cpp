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

using namespace osp;
using namespace osp::active;

const std::string SysNewton::smc_name = "NewtonPhysics";

// Callback called for every Rigid Body (even static ones) on NewtonUpdate
void cb_force_torque(const NewtonBody* body, dFloat timestep, int threadIndex)
{
    // Get SysNewton from Newton World
    auto* sysNewton = static_cast<SysNewton*>(
                NewtonWorldGetUserData(NewtonBodyGetWorld(body)));
    ActiveScene &scene = sysNewton->get_scene();

    // Get ACompNwtBody. Every NewtonBody created here is associated with an
    // entity that contains one.
    auto *bodyComp = static_cast<ACompNwtBody*>(NewtonBodyGetUserData(body));
    DataPhyRigidBody &bodyPhy = bodyComp->m_bodyData;

    auto& transform = scene.get_registry().get<ACompTransform>(
                bodyComp->m_entity);

    // Check if transform has been set externally
    if (transform.m_transformDirty)
    {

        // Set matrix
        NewtonBodySetMatrix(body, transform.m_transform.data());

        transform.m_transformDirty = false;
    }

    // Apply force and torque
    NewtonBodySetForce(body, bodyPhy.m_netForce.data());
    NewtonBodySetTorque(body, bodyPhy.m_netTorque.data());

    // Reset accumolated net force and torque for next frame
    bodyPhy.m_netForce = {0.0f, 0.0f, 0.0f};
    bodyPhy.m_netTorque = {0.0f, 0.0f, 0.0f};
}

ACompNwtBody::ACompNwtBody(ACompNwtBody&& move) :
        m_body(move.m_body),
        m_entity(move.m_entity),
        //m_scene(move.m_scene),
        m_bodyData(move.m_bodyData)
{
    if (m_body != nullptr)
    {
        NewtonBodySetUserData(m_body, this);
    }
}

ACompNwtBody& ACompNwtBody::operator=(ACompNwtBody&& move)
{
    m_body = move.m_body;
    m_entity = move.m_entity;
    m_bodyData = move.m_bodyData;

    if (m_body != nullptr)
    {
        NewtonBodySetUserData(m_body, this);
    }

    return *this;
}

SysNewton::SysNewton(ActiveScene &scene) :
        m_scene(scene),
        m_nwtWorld(NewtonCreate()),
        m_updatePhysicsWorld(scene.get_update_order(), "physics", "wire", "",
                             std::bind(&SysNewton::update_world, this))
{
    //std::cout << "sysnewtoninit\n";
    NewtonWorldSetUserData(m_nwtWorld, this);

    scene.get_registry().on_construct<ACompRigidBody>()
                    .connect<&SysNewton::on_body_construct>(*this);
    scene.get_registry().on_destroy<ACompRigidBody>()
                    .connect<&SysNewton::on_body_destruct>(*this);

    scene.get_registry().on_construct<ACompCollisionShape>()
                    .connect<&SysNewton::on_shape_construct>(*this);
    scene.get_registry().on_destroy<ACompCollisionShape>()
                    .connect<&SysNewton::on_shape_destruct>(*this);
}

SysNewton::~SysNewton()
{
    //std::cout << "sysnewtondestroy\n";
    // Clean up newton dynamics stuff
    m_scene.get_registry().clear<ACompRigidBody>();
    m_scene.get_registry().clear<ACompCollisionShape>();
    NewtonDestroyAllBodies(m_nwtWorld);
    NewtonDestroy(m_nwtWorld);
}

void SysNewton::find_and_add_colliders(ActiveEnt ent, NewtonCollision *compound,
                                       Matrix4 const &currentTransform)
{

    ActiveEnt nextChild = ent;

    // iterate through siblings of ent
    while(nextChild != entt::null)
    {
        auto const &childHeir = m_scene.reg_get<ACompHierarchy>(nextChild);
        auto const &childTransform = m_scene.reg_get<ACompTransform>(nextChild);

        auto* childCollide = m_scene.get_registry()
                                .try_get<ACompCollisionShape>(nextChild);

        Matrix4 childMatrix = currentTransform * childTransform.m_transform;

        if (childCollide)
        {
            NewtonCollision *collision = childCollide->m_collision;

            if (collision)
            {

            }
            else
            {
                // TODO: care about collision shape. for now, everything is a
                //       sphere
                collision = NewtonCreateSphere(m_nwtWorld, 0.5f, 0, NULL);
                childCollide->m_collision = collision;
            }

            //Matrix4 nextTransformNorm = nextTransform.

            // Set transform relative to root body
            Matrix4 f = Matrix4::translation(childMatrix.translation());
            NewtonCollisionSetMatrix(collision, f.data());

            // Add body to compound collision
            NewtonCompoundCollisionAddSubCollision(compound, collision);

        }
        else
        {
            //return;
        }

        find_and_add_colliders(childHeir.m_childFirst, compound, childMatrix);
        nextChild = childHeir.m_siblingNext;
    }
}

void SysNewton::create_body(ActiveEnt entity)
{

    auto& entHeir = m_scene.reg_get<ACompHierarchy>(entity);
    auto& entTransform = m_scene.reg_get<ACompTransform>(entity);

    auto& entBody = m_scene.reg_get<ACompNwtBody>(entity);
    auto* entShape = m_scene.get_registry()
                        .try_get<ACompCollisionShape>(entity);

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

        NewtonCollision* compound
                = NewtonCreateCompoundCollision(m_nwtWorld, 0);

        NewtonCompoundCollisionBeginAddRemove(compound);
        find_and_add_colliders(entHeir.m_childFirst, compound, Matrix4());
        NewtonCompoundCollisionEndAddRemove(compound);

        if (entBody.m_body)
        {
            NewtonBodySetCollision(entBody.m_body, compound);
        }
        else
        {
            entBody.m_body = NewtonCreateDynamicBody(m_nwtWorld, compound,
                                                     Matrix4().data());
        }

        NewtonDestroyCollision(compound);

        // Set inertia and mass
        NewtonBodySetMassMatrix(entBody.m_body, entBody.m_bodyData.m_mass,
                                1, 1, 1);

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
                entBody.m_body = NewtonCreateDynamicBody(m_nwtWorld,
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

void SysNewton::update_world()
{
    //m_scene.floating_origin_translate_begin();

    auto view = m_scene.get_registry().view<ACompRigidBody, ACompTransform>();

    // Update the world
    NewtonUpdate(m_nwtWorld, m_scene.get_time_delta_fixed());

    // Apply transform changes after the Newton world updates
    for (ActiveEnt ent : view)
    {
        auto &entBody      = view.get<ACompRigidBody>(ent);
        auto &entTransform = view.get<ACompTransform>(ent);

        if (entBody.m_body)
        {
            // Get new transform matrix from newton
            NewtonBodyGetMatrix(entBody.m_body,
                                entTransform.m_transform.data());
        }
    }
}

std::pair<ActiveEnt, ACompRigidBody*> SysNewton::find_rigidbody_ancestor(
        ActiveEnt ent)
{
    ActiveEnt prevEnt, currEnt = ent;
    ACompHierarchy *currHeir = nullptr;

    do
    {

       currHeir = m_scene.get_registry().try_get<ACompHierarchy>(currEnt);

        if (!currHeir)
        {
            return {entt::null, nullptr};
        }

        prevEnt = currEnt;
        currEnt = currHeir->m_parent;
    }
    while (currHeir->m_level != gc_heir_physics_level);

    auto *body = m_scene.get_registry().try_get<ACompRigidBody>(prevEnt);

    return {prevEnt, body};

}

// TODO

void SysNewton::body_apply_force(ACompRigidBody &body, Vector3 force)
{
    body.m_bodyData.m_netForce += force;
}

void SysNewton::body_apply_force_local(ACompRigidBody &body, Vector3 force)
{

}

void SysNewton::body_apply_accel(ACompRigidBody &body, Vector3 accel)
{
    body_apply_force(body, accel * body.m_bodyData.m_mass);
}

void SysNewton::body_apply_accel_local(ACompRigidBody &body, Vector3 accel)
{

}

void SysNewton::body_apply_torque(ACompRigidBody &body, Vector3 torque)
{
    body.m_bodyData.m_netTorque += torque;
}

void SysNewton::body_apply_torque_local(ACompRigidBody &body, Vector3 force)
{

}



void SysNewton::on_body_construct(entt::registry& reg, ActiveEnt ent)
{
    // TODO
    reg.get<ACompRigidBody>(ent).m_bodyData.m_mass = 1.0f;
}

void SysNewton::on_body_destruct(entt::registry& reg, ActiveEnt ent)
{
    // make sure the newton body is destroyed
    NewtonBody *body = reg.get<ACompRigidBody>(ent).m_body;
    if (body)
    {
        NewtonDestroyBody(body);
    }
}

void SysNewton::on_shape_construct(entt::registry& reg, ActiveEnt ent)
{

}

void SysNewton::on_shape_destruct(entt::registry& reg, ActiveEnt ent)
{
    // make sure the collision shape destroyed
    NewtonCollision *shape = reg.get<ACompCollisionShape>(ent).m_collision;
    if (shape != nullptr)
    {
        NewtonDestroyCollision(shape);
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

