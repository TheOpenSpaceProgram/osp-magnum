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

#include <osp/UserInputHandler.h>
#include <osp/Active/SysMachine.h>
#include <osp/Active/SysWire.h>

namespace adera::active::machines
{

class MachineUserControl;

/**
 * Stores ButtonControlHandles for SysMachineUserControl to detect user inputs
 */
struct ACompUserControl
{
    ACompUserControl(osp::UserInputHandler &rUsrCtrl)
     : m_throttleMax(   rUsrCtrl.config_get("vehicle_thr_max"))
     , m_throttleMin(   rUsrCtrl.config_get("vehicle_thr_min"))
     , m_throttleMore(  rUsrCtrl.config_get("vehicle_thr_more"))
     , m_throttleLess(  rUsrCtrl.config_get("vehicle_thr_less"))
     , m_selfDestruct(  rUsrCtrl.config_get("vehicle_self_destruct"))
     , m_pitchUp(       rUsrCtrl.config_get("vehicle_pitch_up"))
     , m_pitchDn(       rUsrCtrl.config_get("vehicle_pitch_dn"))
     , m_yawLf(         rUsrCtrl.config_get("vehicle_yaw_lf"))
     , m_yawRt(         rUsrCtrl.config_get("vehicle_yaw_rt"))
     , m_rollLf(        rUsrCtrl.config_get("vehicle_roll_lf"))
     , m_rollRt(        rUsrCtrl.config_get("vehicle_roll_rt"))
    { }

    osp::ButtonControlHandle m_throttleMax;
    osp::ButtonControlHandle m_throttleMin;
    osp::ButtonControlHandle m_throttleMore;
    osp::ButtonControlHandle m_throttleLess;

    osp::ButtonControlHandle m_selfDestruct;

    osp::ButtonControlHandle m_pitchUp;
    osp::ButtonControlHandle m_pitchDn;
    osp::ButtonControlHandle m_yawLf;
    osp::ButtonControlHandle m_yawRt;
    osp::ButtonControlHandle m_rollLf;
    osp::ButtonControlHandle m_rollRt;
};

/**
 * Gets ButtonControlHandle from a UserInputHandler, and updates
 * MachineUserControls
 */
class SysMachineUserControl
{
public:
    static void add_functions(osp::active::ActiveScene& rScene);

    /**
     * Constructs MachineUserControls for vehicles in-construction
     *
     * @param rScene [ref] Scene supporting vehicles
     */
    static void update_construct(osp::active::ActiveScene &rScene);

    /**
     * Updates MachineUserControls in the world by reading user controls and
     * setting wire outputs accordingly
     *
     * @param rScene [ref] Scene with MachineUserControls to update
     */
    static void update_sensor(osp::active::ActiveScene &rScene);
};

/**
 * Interfaces user input into WireOutputs designed for controlling spacecraft.
 */
class MachineUserControl
{
    friend SysMachineUserControl;

    using Percent = osp::active::wiretype::Percent;
    using AttitudeControl = osp::active::wiretype::AttitudeControl;

public:

    static constexpr std::string_view smc_mach_name = "UserControl";

    static constexpr osp::portindex_t<Percent> smc_woThrottle{0};

    static constexpr osp::portindex_t<AttitudeControl> m_woAttitude{0};


private:
    int dummy;
//    osp::active::WireInput  m_wiTest          { this, "Test"              };
//    osp::active::WireOutput m_woAttitude      { this, "AttitudeControl"   };
//    osp::active::WireOutput m_woTestPropagate { this, "TestOut", m_wiTest };
//    osp::active::WireOutput m_woThrottle      { this, "Throttle"          };
};

//-----------------------------------------------------------------------------


} // namespace adera::active::machines
