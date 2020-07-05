#include "SysNewton.h"

#include "ActiveScene.h"


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
}

SysNewton::~SysNewton()
{
    //std::cout << "sysnewtondestroy\n";
    // Clean up newton dynamics stuff
    NewtonDestroyAllBodies(m_nwtWorld);
    NewtonDestroy(m_nwtWorld);
}


void SysNewton::create_body(entt::entity entity)
{

    CompRigidBody& bodyComp = m_scene.reg_get<CompRigidBody>(entity);
    CompTransform& transform = m_scene.reg_get<CompTransform>(entity);

    // Note: bodyComp is a unique_ptr<NwtUserData>

    if (bodyComp)
    {
        // body is already initialized
        return;
    }

    Matrix4 matrix;

    NewtonCollision *ball = NewtonCreateSphere(m_nwtWorld, 1, 0, NULL);

    // TODO: go through hierarchy children and add collision shapes
    // temporary: Create the body
    NewtonBody *body = NewtonCreateDynamicBody(m_nwtWorld, ball,
                                                     matrix.data());

    // Initialize user data
    bodyComp = std::make_unique<NwtUserData>(entity, m_scene, body);

    // Set inertia and mass
    NewtonBodySetMassMatrix(body, 1.0f, 1, 1, 1);

    // Set position/rotation
    NewtonBodySetMatrix(body, transform.m_transform.data());

    // Set damping to 0, as default is 0.1
    // reference frame may be moving and air pressure stuff
    NewtonBodySetLinearDamping(body, 0.0f);

    // Make it easier to rotate
    NewtonBodySetAngularDamping(body, Vector3(1.0f, 1.0f, 1.0f).data());

    // Set callback for updating position of entity and everything else
    NewtonBodySetForceAndTorqueCallback(body, cb_force_torque);

    // Set user data
    //NwtUserData *data = new NwtUserData(entity, m_scene);
    NewtonBodySetUserData(body, bodyComp.get());

    // don't leak memory
    NewtonDestroyCollision(ball);
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
    // TODO: make sure the newton body is destroyed
}


}
