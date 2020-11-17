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
#pragma once

#include <memory>

#include <osp/Active/activetypes.h>
#include <osp/UserInputHandler.h>
#include <osp/types.h>

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
    DebugObject(active::ActiveScene &scene, active::ActiveEnt ent) :
            m_scene(scene),
            m_ent(ent) {};
    virtual ~DebugObject() = default;

private:
    active::ActiveScene &m_scene;
    active::ActiveEnt m_ent;
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
    DebugCameraController(active::ActiveScene &scene, active::ActiveEnt ent);
    ~DebugCameraController() = default;
    void update_vehicle_mod_pre();
    void update_physics_pre();
    void update_physics_post();
    void view_orbit(active::ActiveEnt ent);
private:

    active::ActiveEnt m_orbiting;
    Vector3 m_orbitPos;
    float m_orbitDistance;

    active::UpdateOrderHandle m_updateVehicleModPre;
    active::UpdateOrderHandle m_updatePhysicsPre;
    active::UpdateOrderHandle m_updatePhysicsPost;

    UserInputHandler &m_userInput;
    // Mouse inputs
    MouseMovementHandle m_mouseMotion;
    ScrollInputHandle m_scrollInput;
    ButtonControlHandle m_rmb;
    // Keyboard inputs
    ButtonControlHandle m_up;
    ButtonControlHandle m_dn;
    ButtonControlHandle m_lf;
    ButtonControlHandle m_rt;
    ButtonControlHandle m_switch;

    ButtonControlHandle m_selfDestruct;

};


//class SysDebugObject
//{
//public:
//    SysDebugObject(ActiveScene &scene);

//private:
//    ActiveScene &m_scene;
//};

}
