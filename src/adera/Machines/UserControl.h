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

#include "../wiretypes.h"

#include <osp/UserInputHandler.h>
#include <osp/Active/SysMachine.h>
#include <osp/Active/SysWire.h>

namespace adera::active::machines
{

class MachineUserControl;

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
struct MachineUserControl
{
    using Percent = adera::wire::Percent;
    using AttitudeControl = adera::wire::AttitudeControl;

    static constexpr std::string_view smc_mach_name = "UserControl";

    static constexpr osp::portindex_t<Percent> smc_woThrottle{0};
    static constexpr osp::portindex_t<AttitudeControl> smc_woAttitude{0};

    bool m_enable{false};

    float m_throttlePos;

    bool m_selfDestruct;

    bool m_pitchUp;
    bool m_pitchDn;
    bool m_yawLf;
    bool m_yawRt;
    bool m_rollLf;
    bool m_rollRt;
};

//-----------------------------------------------------------------------------


} // namespace adera::active::machines
