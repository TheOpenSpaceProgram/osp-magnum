#pragma once


#include <Newton.h>

#include "../Resource/PrototypePart.h"

#include "../types.h"
#include "activetypes.h"

namespace osp
{

class ActiveScene;

struct NwtUserData
{
    NwtUserData(ActiveEnt entity, ActiveScene &scene, NewtonBody* body) :
            m_entity(entity),
            m_scene(scene),
            m_body(body),
            m_netForce(),
            m_netTorque() {}
    NwtUserData(NwtUserData&& move) = delete;
    NwtUserData& operator=(NwtUserData&& move) = delete;

    ActiveEnt m_entity;
    ActiveScene &m_scene;

    NewtonBody *m_body;

    Vector3 m_netForce;
    Vector3 m_netTorque;
};

//struct CompNewtonBody
//{
//    NewtonBody *m_body{nullptr};
//    std::unique_ptr<NwtUserData> m_data;
//};

using CompRigidBody = std::unique_ptr<NwtUserData>;

//struct CompNewtonCollision
//{
//    NewtonCollision *shape{nullptr};
//};

// TODO: system base class

class SysNewton
{

public:
    SysNewton(ActiveScene &scene);
    SysNewton(SysNewton const& copy) = delete;
    SysNewton(SysNewton&& move) = delete;

    ~SysNewton();

    /**
     * Scan children for CompColliders. combine it all into a single compound
     * collision (not implemented and just adds a cube)
     * @param entity [in] Entity containing CompNewtonBody
     */
    void create_body(entt::entity entity);

    void update_world();

    /**
     *
     * @return Pair of {level-1 entity, pointer to body found}. If hierarchy
     *         error, then {entt:null, nullptr}. If no CompRigidBody found,
     *         then {level-1 entity, nullptr}
     */
    std::pair<ActiveEnt, CompRigidBody*> find_rigidbody_ancestor(
            ActiveEnt ent);

    void body_apply_force(CompRigidBody &body, Vector3 force);
    void body_apply_force_local(CompRigidBody &body, Vector3 force);

    void body_apply_torque(CompRigidBody &body, Vector3 force);
    void body_apply_torque_local(CompRigidBody &body, Vector3 force);

private:

    void on_body_construct(entt::registry& reg, ActiveEnt ent);
    void on_body_destruct(entt::registry& reg, ActiveEnt ent);


    ActiveScene& m_scene;
    NewtonWorld *const m_nwtWorld;

    UpdateOrderHandle m_updatePhysicsWorld;
};

}

