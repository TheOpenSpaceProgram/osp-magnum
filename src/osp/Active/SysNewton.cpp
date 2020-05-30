#include "SysNewton.h"

#include "ActiveScene.h"


namespace osp
{

void cb_force_torque(const NewtonBody* body, dFloat timestep, int threadIndex)
{

    NwtUserData *data = (NwtUserData*)NewtonBodyGetUserData(body);
    ActiveScene *scene = data->m_scene;

    //Matrix4 matrix;


    CompTransform& transform = scene->get_registry()
                                        .get<CompTransform>(data->m_entity);
    NewtonBodyGetMatrix(body, transform.m_transform.data());

    // Check if floating origin translation is in progress
    if (scene->floating_origin_in_progress())
    {
        // Get matrix, translate, then set
        Matrix4 matrix;
        NewtonBodyGetMatrix(body, matrix.data());

        matrix[3].xyz() += scene->floating_origin_get_total();

        NewtonBodySetMatrix(body, matrix.data());

        //std::cout << "fish\n";
    }


}


SysNewton::SysNewton(ActiveScene *scene) : m_nwtWorld(NewtonCreate()),
                                                 m_scene(scene)
{

}

SysNewton::~SysNewton()
{
    // Clean up newton dynamics stuff
    NewtonDestroyAllBodies(m_nwtWorld);
    NewtonDestroy(m_nwtWorld);
}


void SysNewton::create_body(entt::entity entity)
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

    // Set damping to 0, as default is 0.1
    // reference frame may be moving and air pressure stuff
    NewtonBodySetLinearDamping(bodyComp.m_body, 0.0f);

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

void SysNewton::update(float timestep)
{
    NewtonUpdate(m_nwtWorld, timestep);
    //std::cout << "hi\n";
}

}
