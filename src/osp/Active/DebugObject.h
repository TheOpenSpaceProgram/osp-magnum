#pragma once

#include <memory>

#include "../UserInputHandler.h"
#include "activetypes.h"

namespace osp
{

class AbstractDebugObject
{
public:
    // DebugObject(ActiveScene &scene, ActiveEnt ent);
    virtual ~AbstractDebugObject() = default;


    //virtual void set_entity(ActiveEnt m_ent);
};

template <class Derived>
class DebugObject : public AbstractDebugObject
{
    friend Derived;
public:
    DebugObject(ActiveScene &scene, ActiveEnt ent) :
            m_scene(scene),
            m_ent(ent) {};
    virtual ~DebugObject() = default;

private:
    ActiveScene &m_scene;
    ActiveEnt m_ent;
};

struct CompDebugObject
{
    CompDebugObject(std::unique_ptr<AbstractDebugObject> ptr) :
        m_obj(std::move(ptr)) {}
    CompDebugObject(CompDebugObject&& move) = default;
    ~CompDebugObject() = default;
    CompDebugObject& operator=(CompDebugObject&& move) = default;
    std::unique_ptr<AbstractDebugObject> m_obj;
};


class DebugCameraController : public DebugObject<DebugCameraController>
{

public:
    DebugCameraController(ActiveScene &scene, ActiveEnt ent);
    ~DebugCameraController() = default;
    void update_physics_post();
    void view_track(ActiveEnt ent);
private:

    ActiveEnt m_tracking;
    float m_distance;

    UpdateOrderHandle m_updatePhysicsPost;

    UserInputHandler &m_userInput;
    ButtonControlHandle m_up;
    ButtonControlHandle m_dn;
    ButtonControlHandle m_lf;
    ButtonControlHandle m_rt;
};


//class SysDebugObject
//{
//public:
//    SysDebugObject(ActiveScene &scene);

//private:
//    ActiveScene &m_scene;
//};

}
