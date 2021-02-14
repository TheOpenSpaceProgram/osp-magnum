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

namespace testapp
{

class IDebugObject
{
public:
    // DebugObject(ActiveScene &scene, ActiveEnt ent);
    virtual ~IDebugObject() = default;


    //virtual void set_entity(ActiveEnt m_ent);
};

template <class Derived>
class DebugObject : public IDebugObject
{
    friend Derived;
public:
    DebugObject(osp::active::ActiveScene &scene,
                osp::active::ActiveEnt ent) noexcept :
            m_scene(scene),
            m_ent(ent) {};

    DebugObject(DebugObject &&) = default;
    DebugObject(DebugObject const&) = delete;
    DebugObject& operator=(DebugObject &&) = default;
    DebugObject& operator=(DebugObject const&) = delete;

private:
    osp::active::ActiveScene &m_scene;
    osp::active::ActiveEnt m_ent;
};

struct ACompDebugObject
{
    ACompDebugObject(std::unique_ptr<IDebugObject> ptr) noexcept:
        m_obj(std::move(ptr)) {}
    ACompDebugObject(ACompDebugObject&& move) noexcept = default;
    ~ACompDebugObject() noexcept = default;
    ACompDebugObject& operator=(ACompDebugObject&& move) = default;

    std::unique_ptr<IDebugObject> m_obj;
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

    osp::active::UpdateOrderHandle_t m_updateVehicleModPre;
    osp::active::UpdateOrderHandle_t m_updatePhysicsPre;
    osp::active::UpdateOrderHandle_t m_updatePhysicsPost;

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

}

