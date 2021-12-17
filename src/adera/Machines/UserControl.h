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

#include "../wiretypes.h"           // for AttitudeControl, Percent

#include "osp/Resource/machines.h"  // for portindex_t

#include "osp/types.h"              // for Vector3

#include <string_view>              // for string_view

namespace osp::active { class ActiveScene; }

namespace adera::active::machines
{

/**
 * Gets ButtonControlHandle from a UserInputHandler, and updates
 * MCompUserControls
 */
class SysMCompUserControl
{
public:

    /**
     * Constructs MCompUserControls for vehicles in-construction
     *
     * @param rScene [ref] Scene supporting vehicles
     */
    static void update_construct(osp::active::ActiveScene &rScene);

    /**
     * Updates MCompUserControls in the world by reading user controls and
     * setting wire outputs accordingly
     *
     * @param rScene [ref] Scene with MCompUserControls to update
     */
    static void update_sensor(osp::active::ActiveScene &rScene);
};

/**
 * Interfaces user input into WireOutputs designed for controlling spacecraft.
 */
struct MCompUserControl
{
    using Percent = adera::wire::Percent;
    using AttitudeControl = adera::wire::AttitudeControl;

    static constexpr std::string_view smc_mach_name = "UserControl";

    static constexpr osp::portindex_t<Percent> smc_woThrottle{0};
    static constexpr osp::portindex_t<AttitudeControl> smc_woAttitude{0};

    float m_throttle;
    bool m_selfDestruct;
    osp::Vector3 m_attitude; // pitch, yaw, roll
};

//-----------------------------------------------------------------------------


} // namespace adera::active::machines
