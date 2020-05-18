#include "SystemNewton.h"

#include "ActiveScene.h"


namespace osp
{

void cb_force_torque(const NewtonBody* body, dFloat timestep, int threadIndex)
{

    NwtUserData *data = (NwtUserData*)NewtonBodyGetUserData(body);

    //Matrix4 matrix;


    CompTransform& transform = data->m_scene->get_registry()
                                        .get<CompTransform>(data->m_entity);
    NewtonBodyGetMatrix(body, transform.m_transform.data());

    //std::cout << "fish\n";
}


SystemNewton::SystemNewton(ActiveScene *scene) : m_nwtWorld(NewtonCreate()),
                                                 m_scene(scene)
{

}

SystemNewton::~SystemNewton()
{
    // Clean up newton dynamics stuff
    NewtonDestroyAllBodies(m_nwtWorld);
    NewtonDestroy(m_nwtWorld);
}


void SystemNewton::create_body(entt::entity entity)
{
    // TODO: everything here, this is all temporary


    CompNewtonBody& bodyComp = m_scene->get_registry()
                                        .get<CompNewtonBody>(entity);

    CompTransform& transform = m_scene->get_registry()
                                        .get<CompTransform>(entity);

    Matrix4 matrix;

    NewtonCollision const *ball = NewtonCreateSphere(m_nwtWorld, 1, 0, NULL);

    // Create the body
    bodyComp.m_body = NewtonCreateDynamicBody(m_nwtWorld, ball, matrix.data());

    // Set inertia and mass
    NewtonBodySetMassMatrix(bodyComp.m_body, 1.0f, 1, 1, 1);

    // Set position/rotation
    NewtonBodySetMatrix(bodyComp.m_body, transform.m_transform.data());

    // Set callback for updating position of entity and everything else
    NewtonBodySetForceAndTorqueCallback(bodyComp.m_body, cb_force_torque);

    // Set user data
    NwtUserData *data = new NwtUserData();
    data->m_entity = entity;
    data->m_scene = m_scene;
    NewtonBodySetUserData(bodyComp.m_body, data);

    // don't leak memory
    NewtonDestroyCollision(ball);
}

void SystemNewton::update(float timestep)
{
    NewtonUpdate(m_nwtWorld, timestep);
    //std::cout << "hi\n";
}

}
