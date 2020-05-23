#pragma once

#include <entt/entt.hpp>

#include <Newton.h>

#include "../Resource/PartPrototype.h"


namespace osp
{

class ActiveScene;

struct NwtUserData
{
    entt::entity m_entity;
    ActiveScene *m_scene;
};

struct CompNewtonBody
{
    NewtonBody *m_body{nullptr};
};

//struct CompNewtonCollision
//{
//    NewtonCollision *shape{nullptr};
//};

// TODO: system base class

class SysNewton
{

public:
    SysNewton(ActiveScene *scene);
    ~SysNewton();

    /**
     * Scan children for CompColliders. combine it all into a single compound
     * collision (not implemented and just adds a cube)
     * @param entity Entity containing CompNewtonBody
     */
    void create_body(entt::entity entity);

    void update(float timestep);

private:

    ActiveScene *const m_scene;
    NewtonWorld *const m_nwtWorld;
};

}

