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

namespace adera::active::machines
{

class MachineUserControl;

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

    static constexpr std::string_view smc_mach_name = "UserControl";

    static void add_functions(osp::active::ActiveScene& rScene);
    static void update_construct(osp::active::ActiveScene &rScene);
    static void update_sensor(osp::active::ActiveScene &rScene);
};

/**
 * Interfaces user input into WireOutputs designed for controlling spacecraft.
 */
class MachineUserControl : public osp::active::Machine
{
    friend SysMachineUserControl;

public:
    MachineUserControl();
    MachineUserControl(MachineUserControl&& move);

    MachineUserControl& operator=(MachineUserControl&& move);

    void propagate_output(osp::active::WireOutput* output) override;

    osp::active::WireInput* request_input(osp::WireInPort port) override;
    osp::active::WireOutput* request_output(osp::WireOutPort port) override;

    std::vector<osp::active::WireInput*> existing_inputs() override;
    std::vector<osp::active::WireOutput*> existing_outputs() override;

private:
    osp::active::WireInput  m_wiTest          { this, "Test"              };
    osp::active::WireOutput m_woAttitude      { this, "AttitudeControl"   };
    osp::active::WireOutput m_woTestPropagate { this, "TestOut", m_wiTest };
    osp::active::WireOutput m_woThrottle      { this, "Throttle"          };
};

//-----------------------------------------------------------------------------

inline MachineUserControl::MachineUserControl()
{
    m_woAttitude.value() = osp::active::wiretype::AttitudeControl{};
    m_woThrottle.value() = osp::active::wiretype::Percent{0.0f};
}

inline MachineUserControl::MachineUserControl(MachineUserControl&& move)
 : Machine(std::move(move))
 , m_wiTest(this, std::move(move.m_wiTest))
 , m_woAttitude(this, std::move(move.m_woAttitude))
 , m_woTestPropagate(this, std::move(move.m_woTestPropagate)) // TODO: Why not reference m_wiTest here?
 , m_woThrottle(this, std::move(move.m_woThrottle))
{ }

inline MachineUserControl& MachineUserControl::operator=(MachineUserControl&& move)
{
    Machine::operator=(std::move(move));
    m_wiTest          = { this, std::move(move.m_wiTest)          };
    m_woAttitude      = { this, std::move(move.m_woAttitude)      };
    m_woTestPropagate = { this, std::move(move.m_woTestPropagate) }; // TODO: Why not reference m_wiTest here?
    m_woThrottle      = { this, std::move(move.m_woThrottle)      };
    return *this;
}

} // namespace adera::active::machines
