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

#include "UserControl.h"

#include <osp/Active/ActiveScene.h>
#include <osp/Active/SysSignal.h>
#include <osp/Active/SysVehicle.h>
#include <osp/Resource/machines.h>

#include <iostream>

using adera::active::machines::SysMachineUserControl;
using adera::active::machines::MachineUserControl;
using adera::wire::Percent;
using adera::wire::AttitudeControl;

using osp::active::ActiveScene;
using osp::active::ActiveEnt;
using osp::active::ACompMachines;

using osp::active::ACompWirePanel;
using osp::active::ACompWireNodes;
using osp::active::ACompWire;
using osp::active::SysWire;
using osp::active::SysSignal;
using osp::active::WireNode;
using osp::active::WirePort;
using osp::active::UpdNodes_t;
using osp::nodeindex_t;

using osp::BlueprintMachine;

using osp::Package;
using osp::DependRes;
using osp::Path;

using osp::machine_id_t;
using osp::NodeMap_t;
using osp::Vector3;

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

    machine_id_t const id = osp::mach_id<MachineUserControl>();

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
            ActiveEnt partEnt = rVeh.m_parts[mach.m_partIndex];

            // Get machine entity previously reserved by SysVehicle
            auto& machines = rScene.reg_get<ACompMachines>(partEnt);
            ActiveEnt machEnt = machines.m_machines[mach.m_protoMachineIndex];

            rScene.reg_emplace<MachineUserControl>(machEnt);
        }
    }
}

void SysMachineUserControl::update_sensor(ActiveScene &rScene)
{
    SPDLOG_LOGGER_TRACE(rScene.get_application().get_logger(),
                      "Updating all MachineUserControls");

    // InputDevice.IsActivated()
    // Combination
    auto const &usrCtrl = rScene.reg_get<ACompUserControl>(rScene.hier_get_root());
    ACompWireNodes<Percent> &rNodesPercent = SysWire::nodes<Percent>(rScene);
    ACompWireNodes<AttitudeControl> &rNodesAttCtrl = SysWire::nodes<AttitudeControl>(rScene);

    UpdNodes_t<Percent> updPercent;
    UpdNodes_t<AttitudeControl> updAttCtrl;

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

    for (ActiveEnt ent : view)
    {
        MachineUserControl const &machine = view.get<MachineUserControl>(ent);

        if (!machine.m_enable)
        {
            continue;
        }

        // Get the Percent Panel which contains the Throttle Port
        auto const *pPanelPercent = rScene.reg_try_get< ACompWirePanel<Percent> >(ent);

        if (pPanelPercent != nullptr)
        {
            WirePort<Percent> const *pPortThrottle
                    = pPanelPercent->port(MachineUserControl::smc_woThrottle);

            if (pPortThrottle != nullptr)
            {
                // Get the connected node and its value
                WireNode<Percent> const &nodeThrottle
                        = rNodesPercent.get_node(pPortThrottle->m_nodeIndex);
                float throttlePos = nodeThrottle.m_state.m_percent;

                float throttleRate = 0.5f;
                auto delta = throttleRate * rScene.get_time_delta_fixed();

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

                // Write possibly new throttle value to node
                SysSignal<Percent>::signal_assign(
                        rScene, {throttlePos}, nodeThrottle,
                        pPortThrottle->m_nodeIndex, updPercent);
            }
        }

        // Get the Attitude Control Panel which contains the Throttle Port
        auto const *pPanelAttCtrl
                = rScene.reg_try_get< ACompWirePanel<AttitudeControl> >(ent);

        if (pPanelAttCtrl != nullptr)
        {
            WirePort<AttitudeControl> const *pPortAttCtrl
                    = pPanelAttCtrl->port(MachineUserControl::smc_woAttitude);
            WireNode<AttitudeControl> &nodeAttCtrl
                    = rNodesAttCtrl.get_node(pPortAttCtrl->m_nodeIndex);

            SysSignal<AttitudeControl>::signal_assign(
                    rScene, {attitudeIn}, nodeAttCtrl,
                    pPortAttCtrl->m_nodeIndex, updAttCtrl);
        }
    }

    // Request to update any wire nodes if they were modified
    if (!updPercent.empty())
    {
        std::sort(std::begin(updPercent), std::end(updPercent));
        rNodesPercent.update_write(updPercent);
        rScene.reg_get<ACompWire>(rScene.hier_get_root()).request_update();
    }
    if (!updAttCtrl.empty())
    {
        std::sort(std::begin(updAttCtrl), std::end(updAttCtrl));
        rNodesAttCtrl.update_write(updAttCtrl);
        rScene.reg_get<ACompWire>(rScene.hier_get_root()).request_update();
    }
}
