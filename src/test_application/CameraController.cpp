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
#include "CameraController.h"

#include <adera/Machines/UserControl.h>

#include <osp/Satellites/SatVehicle.h>

#include <osp/Active/ActiveScene.h>
#include <osp/Active/SysAreaAssociate.h>
#include <osp/Active/SysPhysics.h>
#include <osp/Active/SysVehicle.h>
#include <osp/Active/SysVehicleSync.h>

using testapp::SysCameraController;
using testapp::ACompCameraController;

using adera::active::machines::MachineUserControl;

using osp::universe::Satellite;
using osp::universe::Universe;

using osp::active::ActiveScene;
using osp::active::ActiveEnt;
using osp::active::ActiveReg_t;

using osp::active::ACompTransform;
using osp::active::ACompVehicle;
using osp::active::ACompAreaLink;

using osp::active::SysAreaAssociate;

using osp::input::ControlSubscriber;


// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

using osp::Matrix4;
using osp::Quaternion;
using osp::Vector3;
using osp::universe::Vector3g;

std::pair<ActiveEnt, ACompCameraController&>
SysCameraController::get_camera_controller(ActiveScene &rScene)
{
    ActiveEnt ent = rScene.get_registry().view<ACompCameraController>().front();
    return {ent, rScene.reg_get<ACompCameraController>(ent)};
}

bool SysCameraController::try_switch_vehicle(ActiveScene &rScene,
                                             ACompCameraController &rCamCtrl)
{
    ACompAreaLink *pAreaLink = SysAreaAssociate::try_get_area_link(rScene);

    if (pAreaLink == nullptr)
    {
        return false; // Scene is not linked to any universe
    }

    Universe &rUni = pAreaLink->get_universe();
    auto const viewUni = rUni.get_reg().view<osp::universe::UCompVehicle>();
    auto it = viewUni.find(rCamCtrl.m_selected);

    if (it == viewUni.end() || it == viewUni.begin())
    {
        // no vehicle selected, or last vehicle is selected (loop around)
        rCamCtrl.m_selected = viewUni.back();
    }
    else
    {
        // pick the next vehicle
        rCamCtrl.m_selected = *std::prev(it);
    }

    if (rUni.get_reg().valid(rCamCtrl.m_selected))
    {
      SPDLOG_LOGGER_INFO(rScene.get_application().get_logger(), "Selected: {} ",
          rUni.get_reg().get<osp::universe::UCompTransformTraj>(rCamCtrl.m_selected).m_name);
        return true;
    }

    return false;
}

void SysCameraController::update_vehicle(ActiveScene &rScene)
{
    auto const& [ent, rCamCtrl] = get_camera_controller(rScene);

    if (rCamCtrl.m_controls.button_triggered(rCamCtrl.m_switch))
    {
        try_switch_vehicle(rScene, rCamCtrl);
    }

    ActiveEnt const vehicle = find_vehicle_from_sat(rScene, rCamCtrl.m_selected);

    if (!rScene.get_registry().valid(vehicle))
    {
        return; // No vehicle selected
    }

    // Make the craft explode apart when pressing [self destruct]
    if (rCamCtrl.m_controls.button_triggered(rCamCtrl.m_selfDestruct))
    {
        using osp::active::ACompVehicle;
        using osp::active::ACompPart;

        auto &rTgtVehicle = rScene.reg_get<ACompVehicle>(vehicle);

        // delete the last part
        //auto &partPart = m_scene.reg_get<ACompPart>(tgtVehicle.m_parts.back());
        //partPart.m_destroy = true;
        //tgtVehicle.m_separationCount = 1;

        // separate all parts into their own separation islands
        for (size_t i = 0; i < rTgtVehicle.m_parts.size(); i ++)
        {
            rScene.reg_get<ACompPart>(rTgtVehicle.m_parts[i])
                    .m_separationIsland = i;
        }
        rTgtVehicle.m_separationCount = rTgtVehicle.m_parts.size();
    }
}

MachineUserControl* find_user_control(ActiveScene& rScene, ActiveEnt vehicle)
{
    using osp::active::ACompMachines;
    using osp::active::ACompMachineType;

    osp::machine_id_t const id = osp::mach_id<MachineUserControl>();
    // Search all parts
    std::vector<ActiveEnt> const& parts = rScene.reg_get<ACompVehicle>(vehicle).m_parts;
    for (ActiveEnt partEnt : parts)
    {
        // Search all machines of that part
        auto const* pMachines = rScene.reg_try_get<ACompMachines>(partEnt);
        if (nullptr == pMachines)
        {
            return nullptr;
        }

        for (ActiveEnt machEnt : pMachines->m_machines)
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

void SysCameraController::update_controls(ActiveScene &rScene)
{
    ActiveReg_t &rReg = rScene.get_registry();
    auto const [camEnt, rCamCtrl] = get_camera_controller(rScene);

    ActiveEnt const vehicle = find_vehicle_from_sat(rScene, rCamCtrl.m_selected);

    if (!rReg.valid(vehicle))
    {
        return; // No active vehicle to control
    }

    MachineUserControl* pUsrCtrl = find_user_control(rScene, vehicle);

    if (nullptr == pUsrCtrl)
    {
        return; // No MachineUserControl found
    }

    ControlSubscriber const& controls = rCamCtrl.m_controls;

    // Assign attitude control
    pUsrCtrl->m_attitude = Vector3(
            controls.button_held(rCamCtrl.m_pitchDn)
                - controls.button_held(rCamCtrl.m_pitchUp),
            controls.button_held(rCamCtrl.m_yawLf)
                - controls.button_held(rCamCtrl.m_yawRt),
            controls.button_held(rCamCtrl.m_rollRt)
                - controls.button_held(rCamCtrl.m_rollLf));

    // Set Throttle

    float const throttleRate
            = rCamCtrl.m_throttleRate * rScene.get_time_delta_fixed();

    float const throttleDelta
            = controls.button_held(rCamCtrl.m_throttleMore) * throttleRate
            - controls.button_held(rCamCtrl.m_throttleLess) * throttleRate
            + controls.button_triggered(rCamCtrl.m_throttleMax) * 1.0f
            - controls.button_triggered(rCamCtrl.m_throttleMin) * 1.0f;

    pUsrCtrl->m_throttle = std::clamp(pUsrCtrl->m_throttle + throttleDelta,
                                      0.0f, 1.0f);
}

void SysCameraController::update_area(ActiveScene &rScene)
{

    ActiveReg_t &rReg = rScene.get_registry();
    auto const [camEnt, rCamCtrl] = get_camera_controller(rScene);

    ActiveEnt const vehicle = find_vehicle_from_sat(rScene, rCamCtrl.m_selected);

    // Floating Origin / Active area movement

    if (!rReg.valid(vehicle))
    {
        // No vehicle activated in scene, try to find

        ACompAreaLink const *areaLink = SysAreaAssociate::try_get_area_link(rScene);

        if (areaLink == nullptr)
        {
            return; // Scene is not linked to any universe
        }

        Universe const &rUni = areaLink->get_universe();

        if (!rUni.get_reg().valid(rCamCtrl.m_selected))
        {
            return;
        }

        // Smoothly move towards target satellite
        Vector3 &rTranslate = rScene.reg_get<ACompTransform>(camEnt)
                                    .m_transform.translation();
        Vector3 const diff
                = rUni.sat_calc_pos_meters(areaLink->m_areaSat,
                                           rCamCtrl.m_selected) - rTranslate;

        // Move 20% of the distance each frame
        float const distance = diff.length();
        rTranslate += diff.normalized() * distance * rCamCtrl.m_travelSpeed;
    }


    // Trigger floating origin translations if the camera gets too far from
    // the scene origin

    Matrix4 &camTf = rScene.reg_get<ACompTransform>(camEnt).m_transform;

    // round to nearest (m_originDistanceThreshold)
    Vector3g translate = Vector3g((camTf.translation() - rCamCtrl.m_orbitPos)
                            / rCamCtrl.m_originDistanceThreshold)
                       * rCamCtrl.m_originDistanceThreshold;

    // convert to space int
    translate *= osp::universe::gc_units_per_meter;

    if (!translate.isZero())
    {
        SPDLOG_LOGGER_TRACE(m_scene.get_application().get_logger(),
                            "Floating origin translation!");

        // Move the active area to center on the camera
        SysAreaAssociate::area_move(rScene, translate);
    }
}

void SysCameraController::update_view(ActiveScene &rScene)
{
    ActiveReg_t &rReg = rScene.get_registry();
    auto const [camEnt, rCamCtrl] = get_camera_controller(rScene);

    ActiveEnt const vehicle = find_vehicle_from_sat(rScene, rCamCtrl.m_selected);

    if (!rReg.valid(vehicle))
    {
        return;
    }

    Matrix4 &rCamTf = rScene.reg_get<ACompTransform>(camEnt).m_transform;
    Matrix4 const& vehicleTf = rScene.reg_get<ACompTransform>(vehicle).m_transform;

    // Compute Center of Mass of target, if it's a rigid body
    auto [rbEnt, pCompRb]
            = osp::active::SysPhysics::find_rigidbody_ancestor(rScene, vehicle);
    Vector3 comOset{0.0f};
    if (pCompRb != nullptr)
    {
        comOset = vehicleTf.transformVector(pCompRb->m_centerOfMassOffset);
    }

    // Process control inputs

    using Magnum::Rad;

    ControlSubscriber const& controls = rCamCtrl.m_controls;

    Rad yaw = 0.0_degf;
    Rad pitch = 0.0_degf;

    // Arrow key rotation

    // 180 degrees per second
    Rad const keyRotDelta = 180.0_degf * rScene.get_time_delta_fixed();

    yaw += (controls.button_held(rCamCtrl.m_rt)
            - controls.button_held(rCamCtrl.m_lf)) * keyRotDelta;
    pitch += (controls.button_held(rCamCtrl.m_dn)
              - controls.button_held(rCamCtrl.m_up)) * keyRotDelta;

    // Mouse rotation, if right mouse button is down

    if (rCamCtrl.m_controls.button_held(rCamCtrl.m_rmb))
    {
        // 1 degrees per step
        constexpr Rad const mouseRotDelta = 1.0_degf;

        yaw -= controls.get_input_handler()->mouse_state().m_smoothDelta.x()
                * mouseRotDelta;
        pitch -= controls.get_input_handler()->mouse_state().m_smoothDelta.y()
                  * mouseRotDelta;
    }

    Quaternion const camRotate = Quaternion::rotation(yaw, rCamTf.up())
                                  * Quaternion::rotation(pitch, rCamTf.right());
    
    // set camera orbit distance
    constexpr float distSensitivity = 0.3f;
    float const scroll = rCamCtrl.m_controls.get_input_handler()
                                ->scroll_state().offset.y();
    rCamCtrl.m_orbitDistance -= rCamCtrl.m_orbitDistance * distSensitivity
                                * scroll;

    // Clamp orbit distance to avoid producing a degenerate m_orbitPos vector
    constexpr float minDist = 5.0f;
    rCamCtrl.m_orbitDistance = std::max(rCamCtrl.m_orbitDistance, minDist);

    rCamCtrl.m_orbitPos = rCamCtrl.m_orbitPos.normalized() * rCamCtrl.m_orbitDistance;
    rCamCtrl.m_orbitPos = camRotate.transformVector(rCamCtrl.m_orbitPos);

    rCamTf.translation() = vehicleTf.translation() + rCamCtrl.m_orbitPos;

    // look at target
    rCamTf = Matrix4::lookAt(
                rCamTf.translation() + comOset,
                vehicleTf.translation() + comOset,
                rCamTf[1].xyz());
}

ActiveEnt SysCameraController::find_vehicle_from_sat(
        ActiveScene &rScene, Satellite sat)
{
    using osp::universe::UCompActiveArea;

    ActiveReg_t const& reg = rScene.get_registry();
    auto const viewVehicles = reg.view<const ACompVehicle>();

    if (viewVehicles.empty())
    {
        // No vehicles in scene!
        return entt::null;
    }

    ACompAreaLink const *pLink = SysAreaAssociate::try_get_area_link(rScene);

    if (pLink == nullptr)
    {
        return entt::null; // Scene not connected to universe
    }

    auto &rUni = pLink->m_rUniverse.get();
    auto &rArea = rUni.get_reg().get<UCompActiveArea>(pLink->m_areaSat);
    auto &rSync = rScene.get_registry().ctx<osp::active::SyncVehicles>();

    if (auto const findIt = rArea.m_inside.find(sat);
        findIt != rArea.m_inside.end())
    {
        ActiveEnt const activated = rSync.m_inArea.at(sat);
        if (reg.valid(activated))
        {
            return activated;
        }
    }
    return entt::null;
}
