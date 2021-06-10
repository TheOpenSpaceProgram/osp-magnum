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
#include <osp/Active/SysMachine.h>
#include <osp/Active/SysPhysics.h>
#include <osp/Active/SysAreaAssociate.h>

#include <osp/Satellites/SatVehicle.h>

#include <adera/Machines/UserControl.h>

using namespace testapp;

using osp::Vector3;
using osp::Vector3s;
using osp::Quaternion;
using osp::Matrix3;
using osp::Matrix4;
using osp::machine_id_t;

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

using osp::active::ActiveEnt;
using osp::active::ActiveScene;
using osp::active::ACompAreaLink;
using osp::active::ACompCamera;
using osp::active::ACompTransform;
using osp::active::ACompVehicle;
using osp::active::ACompMachines;
using osp::active::ACompMachineType;

using osp::active::SysAreaAssociate;

using osp::universe::Universe;
using osp::universe::UCompTransformTraj;
using osp::universe::UCompVehicle;

using adera::active::machines::MachineUserControl;

DebugCameraController::DebugCameraController(
        ActiveScene &rScene, ActiveEnt ent,
        osp::input::UserInputHandler &rInput)
 : DebugObject(rScene, ent)
 , m_selected(entt::null)
 , m_orbitPos(0, 0, 1)
 , m_orbitDistance(20.0f)
 , m_updateVehicleModPre(
       rScene.get_update_order(), "dbg_cam_vmod", "", "vehicle_modification",
       [this] (ActiveScene&) { this->update_vehicle_mod_pre(); })
 , m_updatePhysicsPre(rScene.get_update_order(), "dbg_cam_pre", "", "physics",
                      [this] (ActiveScene&) { this->update_physics_pre(); })
 , m_updatePhysicsPost(rScene.get_update_order(), "dbg_cam_post", "physics", "",
                       [this] (ActiveScene&) { this->update_physics_post(); })
 , m_controls(&rInput)
 , m_rmb(m_controls.button_subscribe("ui_rmb"))
 , m_up(m_controls.button_subscribe("ui_up"))
 , m_dn(m_controls.button_subscribe("ui_dn"))
 , m_lf(m_controls.button_subscribe("ui_lf"))
 , m_rt(m_controls.button_subscribe("ui_rt"))
 , m_switch(m_controls.button_subscribe("game_switch"))
 , m_selfDestruct(m_controls.button_subscribe("vehicle_self_destruct"))
{ }

void DebugCameraController::update_vehicle_mod_pre()
{
    ActiveEnt vehicle = try_get_vehicle_ent();

    if (!m_scene.get_registry().valid(vehicle))
    {
        return; // No vehicle selected
    }

    if (m_controls.button_triggered(m_selfDestruct))
    {
        using osp::active::ACompVehicle;
        using osp::active::ACompPart;

        auto &tgtVehicle = m_scene.reg_get<ACompVehicle>(vehicle);
        auto& xform = m_scene.reg_get<ACompTransform>(vehicle);

        // delete the last part
        //auto &partPart = m_scene.reg_get<ACompPart>(tgtVehicle.m_parts.back());
        //partPart.m_destroy = true;
        //tgtVehicle.m_separationCount = 1;

        // separate all parts into their own separation islands
        for (size_t i = 0; i < tgtVehicle.m_parts.size(); i ++)
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

    auto &rReg = m_scene.get_registry();
    ActiveEnt vehicle = try_get_vehicle_ent();

    if (m_controls.button_triggered(m_switch))
    {
        try_switch_vehicle();
    }

    if (!rReg.valid(vehicle))
    {
        // No vehicle activated in scene, try to find

        ACompAreaLink *areaLink =  SysAreaAssociate::try_get_area_link(m_scene);

        if (areaLink == nullptr)
        {
            return; // Scene is not linked to any universe
        }

        Universe &rUni = areaLink->get_universe();

        if (!rUni.get_reg().valid(m_selected))
        {
            return;
        }

        // Smoothly move towards target satellite
        Vector3 &rTranslate = m_scene.reg_get<ACompTransform>(m_ent).m_transform.translation();
        Vector3 diff = rUni.sat_calc_pos_meters(areaLink->m_areaSat, m_selected)
                     - rTranslate;

        // Move 20% of the distance each frame
        float distance = diff.length();
        rTranslate += diff.normalized() * distance * 0.2f;
    }


    // When the camera is too far from the origin of the ActiveScene
    const int floatingOriginThreshold = 256;

    Matrix4 &xform = m_scene.reg_get<ACompTransform>(m_ent).m_transform;

    // round to nearest (floatingOriginThreshold)
    Vector3s tra = Vector3s((xform.translation() - m_orbitPos) / floatingOriginThreshold)
                 * floatingOriginThreshold;

    // convert to space int
    tra *= osp::gc_units_per_meter;

    if (!tra.isZero())
    {
      SPDLOG_LOGGER_TRACE(m_scene.get_application().get_logger(),
                          "Floating origin translation!");

        // Move the active area to center on the camera
        SysAreaAssociate::area_move(m_scene, tra);
    }
}

MachineUserControl* find_user_control(ActiveScene& rScene, ActiveEnt vehicle)
{
    machine_id_t const id = osp::mach_id<MachineUserControl>();
    // Search all parts
    std::vector<ActiveEnt> const& parts = rScene.reg_get<ACompVehicle>(vehicle).m_parts;
    for (ActiveEnt partEnt : parts)
    {
        // Search all machines of that part
        auto const* machines = rScene.reg_try_get<ACompMachines>(partEnt);
        if (machines == nullptr)
        {
            return nullptr;
        }
        for (ActiveEnt machEnt : machines->m_machines)
        {
            if (!rScene.get_registry().valid(machEnt))
            {
                return nullptr;
            }
            if (rScene.reg_get<ACompMachineType>(machEnt).m_type == id)
            {
                // Found!
                return &rScene.reg_get<MachineUserControl>(machEnt);
            }
        }
    }
    return nullptr;
}

void DebugCameraController::update_physics_post()
{

    auto &rReg = m_scene.get_registry();
    ActiveEnt vehicle = try_get_vehicle_ent();

    if (!rReg.valid(vehicle))
    {
        return;
    }

    // enable the first MachineUserControl
    if (MachineUserControl *pMUserCtrl = find_user_control(m_scene, vehicle);
        pMUserCtrl != nullptr)
    {
        pMUserCtrl->m_enable = true;
    }


    Matrix4 &xform = m_scene.reg_get<ACompTransform>(m_ent).m_transform;
    Matrix4 const& xformTgt = m_scene.reg_get<ACompTransform>(vehicle).m_transform;

    using osp::active::SysPhysics;
    // Compute Center of Mass of target, if it's a rigid body
    auto [rbEnt, pCompRb] = SysPhysics::find_rigidbody_ancestor(m_scene, vehicle);
    Vector3 comOset{0.0f};
    if (pCompRb != nullptr)
    {
        comOset = xformTgt.transformVector(pCompRb->m_centerOfMassOffset);
    }

    // Process control inputs
    float keyRotYaw = static_cast<float>(m_controls.button_held(m_rt)
                                         - m_controls.button_held(m_lf));
    float keyRotPitch = static_cast<float>(m_controls.button_held(m_dn)
                                           - m_controls.button_held(m_up));

    Quaternion mcFish(Magnum::Math::IdentityInit);
    if (m_controls.button_held(m_rmb) || keyRotYaw || keyRotPitch)
    {
        // 180 degrees per second
        auto keyRotDelta = 180.0_degf * m_scene.get_time_delta_fixed();

        float yaw = keyRotYaw * static_cast<float>(keyRotDelta);
        float pitch = keyRotPitch * static_cast<float>(keyRotDelta);
        if (m_controls.button_held(m_rmb))
        {
            yaw += static_cast<float>(-m_controls.get_input_handler()->mouse_state().m_smoothDelta.x());
            pitch += static_cast<float>(-m_controls.get_input_handler()->mouse_state().m_smoothDelta.y());
        }

        // 1 degrees per step
        constexpr auto rotRate = 1.0_degf;

        // rotate it
        mcFish = Quaternion::rotation(yaw * rotRate, xform.up())
            * Quaternion::rotation(pitch * rotRate, xform.right());
    }

    Vector3 posRelative = xform.translation() - xformTgt.translation();

    // set camera orbit distance
    constexpr float distSensitivity = 0.3f;
    m_orbitDistance += m_orbitDistance * distSensitivity * static_cast<float>(-m_controls.get_input_handler()->scroll_state().offset.y());
    
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


bool DebugCameraController::try_switch_vehicle()
{
    ACompAreaLink *areaLink =  SysAreaAssociate::try_get_area_link(m_scene);

    if (areaLink == nullptr)
    {
        return false; // Scene is not linked to any universe
    }

    ActiveEnt vehicle = try_get_vehicle_ent();
    Vector3 prevVehiclePos;

    auto& rReg = m_scene.get_registry();
    auto viewActive = rReg.view<ACompVehicle>();

    if (m_scene.get_registry().valid(vehicle))
    {
        // Switching away, disable the first MachineUserControl
        if (MachineUserControl *pMUserCtrl = find_user_control(m_scene, vehicle);
            pMUserCtrl != nullptr)
        {
            pMUserCtrl->m_enable = false;
        }

        prevVehiclePos = rReg.get<ACompTransform>(vehicle).m_transform.translation();
    }

    Universe &rUni = areaLink->get_universe();
    auto viewUni = rUni.get_reg().view<UCompVehicle>();
    auto it = viewUni.find(m_selected);

    if (it == viewUni.end() || it == viewUni.begin())
    {
        // no vehicle selected, or last vehicle is selected (loop around)
        m_selected = viewUni.back();
    }
    else
    {
        // pick the next vehicle
        m_selected = *(--it);
    }
    
    if (rUni.get_reg().valid(m_selected))
    {
      SPDLOG_LOGGER_INFO(m_scene.get_application().get_logger(), "Selected: {} ",
          rUni.get_reg().get<UCompTransformTraj>(m_selected).m_name);
        return true;
    }

    return false;
}

ActiveEnt DebugCameraController::try_get_vehicle_ent()
{
    auto& rReg = m_scene.get_registry();
    auto viewVehicles = rReg.view<ACompVehicle>();

    if (viewVehicles.empty())
    {
        // No vehicles in scene!
        return entt::null;
    }

    ACompAreaLink *areaLink =  SysAreaAssociate::try_get_area_link(m_scene);

    if (areaLink == nullptr)
    {
        return entt::null; // Scene not connected to universe
    }

    auto findIt = areaLink->m_inside.find(m_selected);

    if (findIt != areaLink->m_inside.end())
    {
        ActiveEnt activated = findIt->second;
        if (rReg.valid(activated))
        {
            return activated;
        }
    }
    return entt::null;
}

