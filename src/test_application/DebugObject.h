#pragma once

#include <memory>

#include <osp/Active/activetypes.h>
#include <osp/UserInputHandler.h>
#include <osp/types.h>

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
    DebugObject(osp::active::ActiveScene &scene, osp::active::ActiveEnt ent) :
            m_scene(scene),
            m_ent(ent) {};
    virtual ~DebugObject() = default;

private:
    osp::active::ActiveScene &m_scene;
    osp::active::ActiveEnt m_ent;
};

struct ACompDebugObject
{
    ACompDebugObject(std::unique_ptr<AbstractDebugObject> ptr) :
        m_obj(std::move(ptr)) {}
    ACompDebugObject(ACompDebugObject&& move) = default;
   ~ACompDebugObject() = default;
    ACompDebugObject& operator=(ACompDebugObject&& move) = default;
    std::unique_ptr<AbstractDebugObject> m_obj;
};


class DebugCameraController : public DebugObject<DebugCameraController>
{

public:
    DebugCameraController(osp::active::ActiveScene &scene,
                          osp::active::ActiveEnt ent);
    ~DebugCameraController() = default;
    void update_vehicle_mod_pre();
    void update_physics_pre();
    void update_physics_post();
    void view_orbit(osp::active::ActiveEnt ent);
private:

    osp::active::ActiveEnt m_orbiting;
    osp::Vector3 m_orbitPos;
    float m_orbitDistance;

    osp::active::UpdateOrderHandle m_updateVehicleModPre;
    osp::active::UpdateOrderHandle m_updatePhysicsPre;
    osp::active::UpdateOrderHandle m_updatePhysicsPost;

    osp::UserInputHandler &m_userInput;
    // Mouse inputs
    osp::MouseMovementHandle m_mouseMotion;
    osp::ScrollInputHandle m_scrollInput;
    osp::ButtonControlHandle m_rmb;
    // Keyboard inputs
    osp::ButtonControlHandle m_up;
    osp::ButtonControlHandle m_dn;
    osp::ButtonControlHandle m_lf;
    osp::ButtonControlHandle m_rt;
    osp::ButtonControlHandle m_switch;

    osp::ButtonControlHandle m_selfDestruct;

};

