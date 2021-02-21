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

#include "UserControl.h"

using namespace adera::active::machines;
using namespace osp::active;
using namespace osp;

const std::string SysMachineUserControl::smc_name = "UserControl";

void MachineUserControl::propagate_output(WireOutput* output)
{
    std::cout << "propagate test: " << output->get_name() << "\n";
}

WireInput* MachineUserControl::request_input(WireInPort port)
{
    return existing_inputs()[port];
}

WireOutput* MachineUserControl::request_output(WireOutPort port)
{
    return existing_outputs()[port];
}

std::vector<WireInput*> MachineUserControl::existing_inputs()
{
    return {&m_wiTest};
}

std::vector<WireOutput*> MachineUserControl::existing_outputs()
{
    return {&m_woAttitude, &m_woThrottle, &m_woTestPropagate};
}

SysMachineUserControl::SysMachineUserControl(ActiveScene &rScene, UserInputHandler& userControl) :
        SysMachine<SysMachineUserControl, MachineUserControl>(rScene)
{
    rScene.reg_emplace_root<SysMachineUserControl::ACompInstanceData>(
        userControl.config_get("vehicle_thr_max"),
        userControl.config_get("vehicle_thr_min"),
        userControl.config_get("vehicle_thr_more"),
        userControl.config_get("vehicle_thr_less"),
        userControl.config_get("vehicle_self_destruct"),
        userControl.config_get("vehicle_pitch_up"),
        userControl.config_get("vehicle_pitch_dn"),
        userControl.config_get("vehicle_yaw_lf"),
        userControl.config_get("vehicle_yaw_rt"),
        userControl.config_get("vehicle_roll_lf"),
        userControl.config_get("vehicle_roll_rt"));
}

void SysMachineUserControl::update_sensor(ActiveScene& rScene)
{
    //std::cout << "updating all MachineUserControls\n";
    // InputDevice.IsActivated()
    // Combination

    auto& instance = rScene.reg_get_root<ACompInstanceData>();

    if (instance.m_selfDestruct.triggered())
    {
        std::cout << "EXPLOSION BOOM!!!!\n";
    }

    // pitch, yaw, roll
    Vector3 attitudeIn(
        instance.m_pitchDn.trigger_hold() - instance.m_pitchUp.trigger_hold(),
        instance.m_yawLf.trigger_hold() - instance.m_yawRt.trigger_hold(),
        instance.m_rollRt.trigger_hold() - instance.m_rollLf.trigger_hold());

    auto view = rScene.get_registry().view<MachineUserControl>();

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

        if (instance.m_throttleMore.trigger_hold())
        {
            throttlePos = std::clamp(throttlePos + delta, 0.0f, 1.0f);
        }

        if (instance.m_throttleLess.trigger_hold())
        {
            throttlePos = std::clamp(throttlePos - delta, 0.0f, 1.0f);
        }

        if (instance.m_throttleMin.triggered())
        {
            //std::cout << "throttle min\n";
            throttlePos = 0.0f;
        }

        if (instance.m_throttleMax.triggered())
        {
            //std::cout << "throttle max\n";
            throttlePos = 1.0f;
        }

        std::get<wiretype::AttitudeControl>(machine.m_woAttitude.value()).m_attitude = attitudeIn;
        //std::cout << "updating control\n";
    }
}



Machine& SysMachineUserControl::instantiate(ActiveEnt ent,
    PrototypeMachine config, BlueprintMachine settings)
{
    return m_scene.reg_emplace<MachineUserControl>(ent);
}


Machine& SysMachineUserControl::get(ActiveEnt ent)
{
    return m_scene.reg_get<MachineUserControl>(ent);
}
