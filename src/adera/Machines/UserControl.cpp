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
#include <iostream>

#include <osp/Active/ActiveScene.h>
#include <osp/Active/SysVehicle.h>
#include <osp/Resource/machines.h>

#include "UserControl.h"

using namespace adera::active::machines;
using namespace osp::active;
using namespace osp;

void SysMachineUserControl::add_functions(ActiveScene &rScene)
{
    rScene.debug_update_add(rScene.get_update_order(), "mach_usercontrol", "", "wire",
                            &SysMachineUserControl::update_sensor);
    rScene.debug_update_add(rScene.get_update_order(), "mach_usercontrol_construct", "vehicle_activate", "vehicle_modification",
                            &SysMachineUserControl::update_construct);
}

void SysMachineUserControl::update_construct(ActiveScene &rScene)
{
    auto view = rScene.get_registry()
            .view<osp::active::ACompVehicle,
                  osp::active::ACompVehicleInConstruction>();

    machine_id_t const id = mach_id<MachineUserControl>();

    for (auto [vehEnt, rVeh, rVehConstr] : view.each())
    {
        // Check if the vehicle blueprint might store UserControls
        if (rVehConstr.m_blueprint->m_machines.size() <= id)
        {
            continue;
        }

        // Initialize all UserControls in the vehicle
        for (BlueprintMachine &mach : rVehConstr.m_blueprint->m_machines[id])
        {
            // Get part
            ActiveEnt partEnt = rVeh.m_parts[mach.m_blueprintIndex];

            // Get machine entity previously reserved by SysVehicle
            auto& machines = rScene.reg_get<ACompMachines>(partEnt);
            ActiveEnt machEnt = machines.m_machines[mach.m_protoMachineIndex];

            rScene.reg_emplace<MachineUserControl>(machEnt);
            rScene.reg_emplace<ACompMachineType>(machEnt, id);
        }
    }
}

void SysMachineUserControl::update_sensor(ActiveScene &rScene)
{
    SPDLOG_LOGGER_TRACE(rScene.get_application().get_logger(),
                      "Updating all MachineUserControls");

    // InputDevice.IsActivated()
    // Combination
    auto const &usrCtrl = rScene.reg_root_get_or_emplace<ACompUserControl>(rScene.get_user_input());

    if (usrCtrl.m_selfDestruct.triggered())
    {
        SPDLOG_LOGGER_INFO(rScene.get_application().get_logger(),
                        "Self destruct -- EXPLOSION BOOM!!!!");
    }

    // pitch, yaw, roll
    Vector3 attitudeIn(
            usrCtrl.m_pitchDn.trigger_hold() - usrCtrl.m_pitchUp.trigger_hold(),
            usrCtrl.m_yawLf.trigger_hold()   - usrCtrl.m_yawRt.trigger_hold(),
            usrCtrl.m_rollRt.trigger_hold()  - usrCtrl.m_rollLf.trigger_hold());

    auto view = rScene.get_registry().view<MachineUserControl>();


#if 0
    for (ActiveEnt ent : view)
    {
        MachineUserControl &machine = view.get<MachineUserControl>(ent);
        auto& throttlePos = std::get<wiretype::Percent>(machine.m_woThrottle.value()).m_value;

        float throttleRate = 0.5f;
        auto delta = throttleRate * rScene.get_time_delta_fixed();

        if (!machine.m_enable)
        {
            continue;
        }

        if (usrCtrl.m_throttleMore.trigger_hold())
        {
            throttlePos = std::clamp(throttlePos + delta, 0.0f, 1.0f);
        }

        if (usrCtrl.m_throttleLess.trigger_hold())
        {
            throttlePos = std::clamp(throttlePos - delta, 0.0f, 1.0f);
        }

        if (usrCtrl.m_throttleMin.triggered())
        {
            SPDLOG_LOGGER_TRACE(rScene.get_application().get_logger(),
                              "Minimum throttle");
            throttlePos = 0.0f;
        }

        if (usrCtrl.m_throttleMax.triggered())
        {
            SPDLOG_LOGGER_TRACE(rScene.get_application().get_logger(),
                              "Maximum throttle");
            throttlePos = 1.0f;
        }

        std::get<wiretype::AttitudeControl>(machine.m_woAttitude.value()).m_attitude = attitudeIn;
        SPDLOG_LOGGER_TRACE(rScene.get_application().get_logger(),
                            "Updating control");
    }

#endif
}
