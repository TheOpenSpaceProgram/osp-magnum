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
#include <osp/Active/SysNewton.h>

#include <adera/Machines/UserControl.h>

using namespace testapp;

using osp::Vector3;
using osp::Vector3s;
using osp::Quaternion;
using osp::Matrix3;
using osp::Matrix4;

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

using osp::active::ActiveEnt;
using osp::active::ActiveScene;
using osp::active::ACompCamera;
using osp::active::ACompTransform;
using osp::active::ACompVehicle;

using osp::active::SysAreaAssociate;

using adera::active::machines::MachineUserControl;


//DebugObject::DebugObject(ActiveScene &scene, ActiveEnt ent) :
//    m_scene(scene),
//    m_ent(ent)
//{

//}

//SysDebugObject::SysDebugObject(ActiveScene &scene) :
//        m_scene(scene)
//{

//}


DebugCameraController::DebugCameraController(ActiveScene &rScene, ActiveEnt ent)
 : DebugObject(rScene, ent)
 , m_orbiting(entt::null)
 , m_orbitPos(0, 0, 1)
 , m_orbitDistance(20.0f)
 , m_updateVehicleModPre(
       rScene.get_update_order(), "dbg_cam_vmod", "", "vehicle_modification",
       [this] (ActiveScene&) { this->update_vehicle_mod_pre(); })
 , m_updatePhysicsPre(rScene.get_update_order(), "dbg_cam_pre", "", "physics",
                      [this] (ActiveScene&) { this->update_physics_pre(); })
 , m_updatePhysicsPost(rScene.get_update_order(), "dbg_cam_post", "physics", "",
                       [this] (ActiveScene&) { this->update_physics_post(); })
 , m_userInput(rScene.get_user_input())
 , m_mouseMotion(m_userInput.mouse_get())
 , m_scrollInput(m_userInput.scroll_get())
 , m_rmb(m_userInput.config_get("ui_rmb"))
 , m_up(m_userInput.config_get("ui_up"))
 , m_dn(m_userInput.config_get("ui_dn"))
 , m_lf(m_userInput.config_get("ui_lf"))
 , m_rt(m_userInput.config_get("ui_rt"))
 , m_switch(m_userInput.config_get("game_switch"))
 , m_selfDestruct(m_userInput.config_get("vehicle_self_destruct"))
{ }

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
        auto& xform = m_scene.reg_get<ACompTransform>(m_orbiting);

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

void DebugCameraController::update_physics_pre()
{
    // Floating Origin / Active area movement

    // When the camera is too far from the origin of the ActiveScene
    const int floatingOriginThreshold = 256;

    Matrix4 &xform = m_scene.reg_get<ACompTransform>(m_ent).m_transform;

    // round to nearest (floatingOriginThreshold)
    Vector3s tra(xform.translation() / floatingOriginThreshold);
    tra *= floatingOriginThreshold;

    // convert to space int
    tra *= osp::gc_units_per_meter;

    if (!tra.isZero())
    {
        std::cout << "Floating origin translation!\n";

        // Move the active area to center on the camera
        SysAreaAssociate::area_move(m_scene, tra);
    }
}

void DebugCameraController::update_physics_post()
{

    bool targetValid = m_scene.get_registry().valid(m_orbiting);
    auto& rReg = m_scene.get_registry();

    if (m_switch.triggered())
    {
        std::cout << "switch to new vehicle\n";

        auto view = rReg.view<ACompVehicle>();
        auto it = view.find(m_orbiting);

        if (targetValid)
        {
            // disable the first MachineUserControl because switching away
            ActiveEnt firstPart
                    = *(view.get<ACompVehicle>(m_orbiting).m_parts.begin());
            auto* pMUserCtrl = rReg.try_get<MachineUserControl>(firstPart);
            if (pMUserCtrl != nullptr) { pMUserCtrl->disable(); }
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

        targetValid = rReg.valid(m_orbiting);

        if (targetValid)
        {
            // enable the first MachineUserControl
            ActiveEnt firstPart
                    = *(view.get<ACompVehicle>(m_orbiting).m_parts.begin());

            auto* pMUserCtrl = rReg.try_get<MachineUserControl>(firstPart);
            if (pMUserCtrl != nullptr) { pMUserCtrl->enable(); }
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

    Matrix4 &xform = m_scene.reg_get<ACompTransform>(m_ent).m_transform;
    Matrix4 const& xformTgt = m_scene.reg_get<ACompTransform>(m_orbiting).m_transform;

    using osp::active::SysPhysics_t;
    // Compute Center of Mass of target, if it's a rigid body
    auto [rbEnt, pCompRb] = SysPhysics_t::find_rigidbody_ancestor(m_scene, m_orbiting);
    Vector3 comOset{0.0f};
    if (pCompRb != nullptr)
    {
        comOset = xformTgt.transformVector(pCompRb->m_centerOfMassOffset);
    }

    // Process control inputs
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

        // 1 degrees per step
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
    xform = Matrix4::lookAt(xform.translation() + comOset,
        xformTgt.translation() + comOset, xform[1].xyz());
}

void DebugCameraController::view_orbit(ActiveEnt ent)
{
    m_orbiting = ent;
}

