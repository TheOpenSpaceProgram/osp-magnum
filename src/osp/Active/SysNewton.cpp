#include "SysNewton.h"

#include "ActiveScene.h"

#include <Newton.h>


namespace osp
{

void cb_force_torque(const NewtonBody* body, dFloat timestep, int threadIndex)
{

    NwtUserData *data = (NwtUserData*)NewtonBodyGetUserData(body);
    ActiveScene &scene = data->m_scene;

    //Matrix4 matrix;

    CompTransform& transform = scene.get_registry()
                                        .get<CompTransform>(data->m_entity);
    NewtonBodyGetMatrix(body, transform.m_transform.data());

    Matrix4 matrix;
    NewtonBodyGetMatrix(body, matrix.data());

    // Check if floating origin translation is in progress
    if (scene.floating_origin_in_progress())
    {
        // Do floating origin translation, then set position

        matrix[3].xyz() += scene.floating_origin_get_total();

        NewtonBodySetMatrix(body, matrix.data());

        //std::cout << "fish\n";
    }

    // Apply force and torque
    NewtonBodySetForce(body, data->m_netForce.data());
    NewtonBodySetTorque(body, data->m_netTorque.data());

    data->m_netForce = {0.0f, 0.0f, 0.0f};
    data->m_netTorque = {0.0f, 0.0f, 0.0f};

    // temporary fun stuff:
    //NewtonBodySetForce(body, (-(matrix[3].xyz() + Vector3(0, 0, 20)) * 0.1f).data());
    //Vector3 torque(0, 1, 0);
    //NewtonBodySetTorque(body, torque.data());

}


SysNewton::SysNewton(ActiveScene &scene) :
        m_scene(scene),
        m_nwtWorld(NewtonCreate()),
        m_updatePhysicsWorld(scene.get_update_order(), "physics", "wire", "",
                             std::bind(&SysNewton::update_world, this))
{
    //std::cout << "sysnewtoninit\n";

    scene.get_registry().on_construct<CompRigidBody>()
                    .connect<&SysNewton::on_body_construct>(*this);
    scene.get_registry().on_destroy<CompRigidBody>()
                    .connect<&SysNewton::on_body_destruct>(*this);

    scene.get_registry().on_construct<CompCollisionShape>()
                    .connect<&SysNewton::on_shape_construct>(*this);
    scene.get_registry().on_destroy<CompCollisionShape>()
                    .connect<&SysNewton::on_shape_destruct>(*this);
}

SysNewton::~SysNewton()
{
    //std::cout << "sysnewtondestroy\n";
    // Clean up newton dynamics stuff
    m_scene.get_registry().clear<CompRigidBody>();
    m_scene.get_registry().clear<CompCollisionShape>();
    NewtonDestroyAllBodies(m_nwtWorld);
    NewtonDestroy(m_nwtWorld);
}

void SysNewton::find_and_add_colliders(ActiveEnt ent, NewtonCollision *compound, Matrix4 const &currentTransform)
{

    ActiveEnt nextChild = ent;

    while(nextChild != entt::null)
    {
        CompHierarchy const &childHeir = m_scene.reg_get<CompHierarchy>(nextChild);
        CompTransform const &childTransform = m_scene.reg_get<CompTransform>(nextChild);

        CompCollisionShape* childCollide = m_scene.get_registry().try_get<CompCollisionShape>(nextChild);
        Matrix4 childMatrix = currentTransform * childTransform.m_transform;

        if (childCollide)
        {
            NewtonCollision *collision = childCollide->m_collision;

            if (collision)
            {

            }
            else
            {
                // TODO: care about collision shape
                childCollide->m_collision = collision = NewtonCreateSphere(m_nwtWorld, 0.5f, 0, NULL);
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

    CompHierarchy& entHeir = m_scene.reg_get<CompHierarchy>(entity);
    CompRigidBody& entBody = m_scene.reg_get<CompRigidBody>(entity);
    CompCollisionShape& entShape = m_scene.get_registry().get_or_emplace<CompCollisionShape>(entity);
    CompTransform& entTransform = m_scene.reg_get<CompTransform>(entity);

    // Note: entBody is a unique_ptr<NwtUserData>

    if (entBody)
    {
        // body is already initialized
        return;
    }

    Matrix4 matrix;
    NewtonCollision* compound = NewtonCreateCompoundCollision(m_nwtWorld, 0);

    NewtonCompoundCollisionBeginAddRemove(compound);
    find_and_add_colliders(entHeir.m_childFirst, compound, Matrix4());
    NewtonCompoundCollisionEndAddRemove(compound);

    NewtonBody *body = NewtonCreateDynamicBody(m_nwtWorld, compound,
                                               matrix.data());

    NewtonDestroyCollision(compound);

    // Initialize user data
    entBody = std::make_unique<NwtUserData>(entity, m_scene, body);

    // Set inertia and mass
    NewtonBodySetMassMatrix(body, 1.0f, 1, 1, 1);

    // Set position/rotation
    NewtonBodySetMatrix(body, entTransform.m_transform.data());

    // Set damping to 0, as default is 0.1
    // reference frame may be moving and air pressure stuff
    NewtonBodySetLinearDamping(body, 0.0f);

    // Make it easier to rotate
    NewtonBodySetAngularDamping(body, Vector3(1.0f, 1.0f, 1.0f).data());

    // Set callback for updating position of entity and everything else
    NewtonBodySetForceAndTorqueCallback(body, cb_force_torque);

    // Set user data
    //NwtUserData *data = new NwtUserData(entity, m_scene);
    NewtonBodySetUserData(body, entBody.get());

    // don't leak memory
    //NewtonDestroyCollision(ball);
}

void SysNewton::update_world()
{
    m_scene.floating_origin_translate_begin();

    NewtonUpdate(m_nwtWorld, m_scene.get_time_delta_fixed());
    //std::cout << "hi\n";
}

std::pair<ActiveEnt, CompRigidBody*> SysNewton::find_rigidbody_ancestor(
        ActiveEnt ent)
{
    ActiveEnt prevEnt, currEnt = ent;
    CompHierarchy *currHeir = nullptr;

    do
    {

       currHeir = m_scene.get_registry().try_get<CompHierarchy>(currEnt);

        if (!currHeir)
        {
            return {entt::null, nullptr};
        }

        prevEnt = currEnt;
        currEnt = currHeir->m_parent;
    }
    while (currHeir->m_level != gc_heir_physics_level);

    CompRigidBody *body = m_scene.get_registry().try_get<CompRigidBody>(prevEnt);

    return {prevEnt, body};

}

// TODO

void SysNewton::body_apply_force(CompRigidBody &body, Vector3 force)
{
    body->m_netForce += force;
}

void SysNewton::body_apply_force_local(CompRigidBody &body, Vector3 force)
{

}

void SysNewton::body_apply_torque(CompRigidBody &body, Vector3 torque)
{
    body->m_netTorque += torque;
}

void SysNewton::body_apply_torque_local(CompRigidBody &body, Vector3 force)
{

}

void SysNewton::on_body_construct(entt::registry& reg, ActiveEnt ent)
{

}

void SysNewton::on_body_destruct(entt::registry& reg, ActiveEnt ent)
{
    // make sure the newton body is destroyed
    NewtonBody *body = reg.get<CompRigidBody>(ent)->m_body;
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
    NewtonCollision *shape = reg.get<CompCollisionShape>(ent).m_collision;
    if (shape)
    {
        NewtonDestroyCollision(shape);
    }
}


}
