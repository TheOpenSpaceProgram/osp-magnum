#pragma once

#include <cstdint>

#include "../Resource/PrototypePart.h"

#include "../types.h"
#include "activetypes.h"

class NewtonBody;
class NewtonCollision;
class NewtonWorld;

namespace osp::active
{

class ActiveScene;

struct DataPhyRigidBody
{
    float m_mass;
    Vector3 m_velocity;
    Vector3 m_rotVelocity;

    Vector3 m_intertia;
    Vector3 m_netForce;
    Vector3 m_netTorque;
};

struct ACompNwtBody
{
    ACompNwtBody() = default;
    ACompNwtBody(ACompNwtBody&& move);
    ACompNwtBody& operator=(ACompNwtBody&& move);

    NewtonBody *m_body{nullptr};
    ActiveEnt m_entity{entt::null};
    //ActiveScene &m_scene;

    DataPhyRigidBody m_bodyData;
};



using ACompRigidBody = ACompNwtBody;

//{
//    ACompRigidBody(ActiveEnt entity, ActiveScene &scene, NewtonBody* body) :
//            m_entity(entity),
//            m_scene(scene),
//            m_body(body),
//            m_netForce(),
//            m_netTorque() {}
//    ACompRigidBody(NwtUserData&& move) = delete;
//    ACompRigidBody& operator=(NwtUserData&& move) = delete;

//    ActiveEnt m_entity;
//    ActiveScene &m_scene;

//    NewtonBody *m_body;

//    Vector3 m_netForce;
//    Vector3 m_netTorque;
//};

//struct NwtUserDataWorld
//{
//    ActiveScene &m_scene;
//};

//struct CompNewtonBody
//{
//    NewtonBody *m_body{nullptr};
//    std::unique_ptr<NwtUserData> m_data;
//};

//using ACompRigidBody = std::unique_ptr<NwtUserData>;

//struct CompNewtonCollision
//{
//    NewtonCollision *shape{nullptr};
//};


struct ACompCollisionShape
{
    NewtonCollision* m_collision{nullptr};
    ECollisionShape m_shape{ECollisionShape::NONE};
};


class SysNewton
{

public:
    SysNewton(ActiveScene &scene);
    SysNewton(SysNewton const& copy) = delete;
    SysNewton(SysNewton&& move) = delete;

    ~SysNewton();

    /**
     * Scan children for CompColliders. combine it all into a single compound
     * collision
     * @param entity [in] Entity containing CompNewtonBody
     */
    void create_body(ActiveEnt entity);

    void update_world();

    /**
     *
     * @return Pair of {level-1 entity, pointer to body found}. If hierarchy
     *         error, then {entt:null, nullptr}. If no ACompRigidBody found,
     *         then {level-1 entity, nullptr}
     */
    std::pair<ActiveEnt, ACompRigidBody*> find_rigidbody_ancestor(
            ActiveEnt ent);

    constexpr ActiveScene& get_scene() { return m_scene; }

    void body_apply_force(ACompRigidBody &body, Vector3 force);
    void body_apply_force_local(ACompRigidBody &body, Vector3 force);

    void body_apply_accel(ACompRigidBody &body, Vector3 accel);
    void body_apply_accel_local(ACompRigidBody &body, Vector3 accel);

    void body_apply_torque(ACompRigidBody &body, Vector3 force);
    void body_apply_torque_local(ACompRigidBody &body, Vector3 force);

    void shape_create_box(ACompCollisionShape &shape, Vector3 size);
    void shape_create_sphere(ACompCollisionShape &shape, float radius);

    template<class TRIANGLE_IT_T>
    void shape_create_tri_mesh_static(ACompCollisionShape &shape,
                                      TRIANGLE_IT_T const& start,
                                      TRIANGLE_IT_T const& end);

private:

    /**
     * Search descendents for CompColliders and add NewtonCollisions to a vector.
     * @param ent [in] Entity to check colliders for, and recurse into children
     * @param compound [in] Compound collision
     * @param currentTransform [in] Hierarchy transform of ancestors
     */
    void find_and_add_colliders(ActiveEnt ent,
                                NewtonCollision *compound,
                                Matrix4 const &currentTransform);

    void on_body_construct(entt::registry& reg, ActiveEnt ent);
    void on_body_destruct(entt::registry& reg, ActiveEnt ent);

    void on_shape_construct(entt::registry& reg, ActiveEnt ent);
    void on_shape_destruct(entt::registry& reg, ActiveEnt ent);

    NewtonCollision* newton_create_tree_collision(
            const NewtonWorld *newtonWorld, int shapeId);
    void newton_tree_collision_add_face(
            const NewtonCollision* treeCollision, int vertexCount,
            const float* vertexPtr, int strideInBytes, int faceAttribute);
    void newton_tree_collision_begin_build(
            const NewtonCollision* treeCollision);
    void newton_tree_collision_end_build(
            const NewtonCollision* treeCollision,  int optimize);


    ActiveScene& m_scene;
    NewtonWorld *const m_nwtWorld;

    UpdateOrderHandle m_updatePhysicsWorld;
};

template<class TRIANGLE_IT_T>
void SysNewton::shape_create_tri_mesh_static(ACompCollisionShape &shape,
                                             TRIANGLE_IT_T const& start,
                                             TRIANGLE_IT_T const& end)
{
    // TODO: this is actually horrendously slow and WILL cause issues later on.
    //       Tree collisions aren't made for real-time loading. Consider
    //       manually hacking up serialized data instead of add face, or use
    //       Newton's dgAABBPolygonSoup stuff directly

    NewtonCollision* tree = newton_create_tree_collision(m_nwtWorld, 0);

    newton_tree_collision_begin_build(tree);

    Vector3 triangle[3];

    for (TRIANGLE_IT_T it = start; it != end; it += 3)
    {
        triangle[0] = *reinterpret_cast<Vector3 const*>((it + 0).position());
        triangle[1] = *reinterpret_cast<Vector3 const*>((it + 1).position());
        triangle[2] = *reinterpret_cast<Vector3 const*>((it + 2).position());

        newton_tree_collision_add_face(tree, 3,
                                       reinterpret_cast<float*>(triangle),
                                       sizeof(float) * 3, 0);

    }

    newton_tree_collision_end_build(tree, 0);

    shape.m_shape = ECollisionShape::TERRAIN;
    shape.m_collision = tree;
}

}

