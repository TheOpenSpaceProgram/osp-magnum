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
#include "DebugObject.h"

#include <osp/Active/ActiveScene.h>
#include <osp/Active/SysVehicle.h>
#include <osp/Active/physics.h>

#include <adera/Machines/UserControl.h>

using adera::active::machines::MachineUserControl;

using namespace osp;

using namespace Magnum::Math::Literals;

//DebugObject::DebugObject(ActiveScene &scene, ActiveEnt ent) :
//    m_scene(scene),
//    m_ent(ent)
//{

//}

//SysDebugObject::SysDebugObject(ActiveScene &scene) :
//        m_scene(scene)
//{

//}


DebugCameraController::DebugCameraController(active::ActiveScene &scene,
                                             active::ActiveEnt ent) :
        DebugObject(scene, ent),
        m_orbiting(entt::null),
        m_orbitPos(0, 0, 1),
        m_updatePhysicsPost(scene.get_update_order(), "dbg_cam", "physics", "",
                std::bind(&DebugCameraController::update_physics_post, this)),
        m_updateVehicleModPre(scene.get_update_order(), "dbg_cam_vmod", "", "vehicle_modification",
                std::bind(&DebugCameraController::update_vehicle_mod_pre, this)),
        m_userInput(scene.get_user_input()),
        m_mouseMotion(m_userInput.mouse_get()),
        m_scrollInput(m_userInput.scroll_get()),
        m_up(m_userInput.config_get("ui_up")),
        m_dn(m_userInput.config_get("ui_dn")),
        m_lf(m_userInput.config_get("ui_lf")),
        m_rt(m_userInput.config_get("ui_rt")),
        m_switch(m_userInput.config_get("game_switch")),
        m_rmb(m_userInput.config_get("ui_rmb")),
        m_selfDestruct(m_userInput.config_get("vehicle_self_destruct"))
{
    m_orbitDistance = 20.0f;
}

void DebugCameraController::update_vehicle_mod_pre()
{
    if (!m_scene.get_registry().valid(m_orbiting))
    {
        return;
    }

    if (m_selfDestruct.triggered())
    {
        using osp::active::ACompVehicle;
        using osp::active::ACompPart;

        auto &tgtVehicle = m_scene.reg_get<ACompVehicle>(m_orbiting);

        // delete the last part
        //auto &partPart = m_scene.reg_get<ACompPart>(tgtVehicle.m_parts.back());
        //partPart.m_destroy = true;
        //tgtVehicle.m_separationCount = 1;

        // separate all parts into their own separation islands
        for (unsigned i = 0; i < tgtVehicle.m_parts.size(); i ++)
        {
            m_scene.reg_get<ACompPart>(tgtVehicle.m_parts[i])
                    .m_separationIsland = i;
        }
        tgtVehicle.m_separationCount = tgtVehicle.m_parts.size();
    }
}

void DebugCameraController::update_physics_post()
{

    bool targetValid = m_scene.get_registry().valid(m_orbiting);

    if (m_switch.triggered())
    {
        std::cout << "switch to new vehicle\n";

        auto view = m_scene.get_registry().view<active::ACompVehicle>();
        auto it = view.find(m_orbiting);

        if (targetValid)
        {
            // disable the first MachineUserControl because switching away
            active::ActiveEnt firstPart
                    = *(view.get(m_orbiting).m_parts.begin());
            m_scene.reg_get<MachineUserControl>(firstPart).m_enable = false;
        }

        if (it == view.end() || it == view.begin())
        {
            // no vehicle selected, or last vehicle is selected (loop around)
            m_orbiting = view.back();
        }
        else
        {
            // pick the next vehicle
            m_orbiting = *(--it);
            std::cout << "next\n";
        }

        targetValid = m_scene.get_registry().valid(m_orbiting);

        if (targetValid)
        {
            // enable the first MachineUserControl
            active::ActiveEnt firstPart
                    = *(view.get(m_orbiting).m_parts.begin());

            m_scene.reg_get<MachineUserControl>(firstPart).m_enable = true;
        }

        // pick next entity, or first entity in scene
        //ActiveEnt search = targetValid
        //        ? m_scene.reg_get<ACompHierarchy>(m_orbiting).m_siblingNext
        //        : m_scene.reg_get<ACompHierarchy>(m_scene.hier_get_root())
        //                 .m_childFirst;

        // keep looping until a vehicle is found
        //while ((!m_scene.get_registry().has<ACompVehicle>(search)))
        //{
        //    search = m_scene.reg_get<ACompHierarchy>(search).m_siblingNext;
        //}
    }

    if (!targetValid)
    {
        return;
    }

    Matrix4 &xform = m_scene.reg_get<active::ACompTransform>(m_ent).m_transform;
    Matrix4 const& xformTgt = m_scene.reg_get<active::ACompTransform>(m_orbiting).m_transform;

    float keyRotYaw = static_cast<float>(m_rt.trigger_hold() - m_lf.trigger_hold());
    float keyRotPitch = static_cast<float>(m_dn.trigger_hold() - m_up.trigger_hold());

    Quaternion mcFish(Magnum::Math::IdentityInit);
    if (m_rmb.trigger_hold() || keyRotYaw || keyRotPitch)
    {
        // 180 degrees per second
        auto keyRotDelta = 180.0_degf * m_scene.get_time_delta_fixed();

        float yaw = keyRotYaw * static_cast<float>(keyRotDelta);
        float pitch = keyRotPitch * static_cast<float>(keyRotDelta);
        if (m_rmb.trigger_hold())
        {
            yaw += static_cast<float>(-m_mouseMotion.dxSmooth());
            pitch += static_cast<float>(-m_mouseMotion.dySmooth());
        }

        // 100 degrees per step
        constexpr auto rotRate = 1.0_degf;

        // rotate it
        mcFish = Quaternion::rotation(yaw * rotRate, xform.up())
            * Quaternion::rotation(pitch * rotRate, xform.right());
    }

    Vector3 posRelative = xform.translation() - xformTgt.translation();

    // set camera orbit distance
    constexpr float distSensitivity = 1.0f;
    m_orbitDistance += distSensitivity * static_cast<float>(-m_scrollInput.dy());
    
    // Clamp orbit distance to avoid producing a degenerate m_orbitPos vector
    constexpr float minDist = 5.0f;
    if (m_orbitDistance < minDist) { m_orbitDistance = minDist; }

    m_orbitPos = m_orbitPos.normalized() * m_orbitDistance;
    m_orbitPos = mcFish.transformVector(m_orbitPos);

    xform.translation() = xformTgt.translation() + m_orbitPos;

    // look at target
    xform = Matrix4::lookAt(xform.translation(), xformTgt.translation(), xform[1].xyz());
}

void DebugCameraController::view_orbit(active::ActiveEnt ent)
{
    m_orbiting = ent;
}

