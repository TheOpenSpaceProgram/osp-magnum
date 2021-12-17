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

#include <adera/wiretypes.h>        // for wire::Percent, wire::AttitudeControl

#include <osp/Resource/machines.h>  // for osp::portindex_t

#include <osp/types.h>              // for osp::Vector3

#include <string>                   // for std::string

namespace osp::active { class ActiveScene; }

namespace adera::active::machines
{

/**
 * A system which provides the logic that powers Reaction Control Systems.
 * Vehicles that use propulsive reaction control will possess a number of rocket
 * thrusters which need to know whether or not a given maneuver command requires
 * their contribution. Translation and orientation commands are received from
 * the command module and output as throttle command values, and the
 * MCompRockets associated with the RCS thrusters will fire.
 */
class SysMCompRCSController
{
public:

    /**
     * Constructs MCompRCSControllers for vehicles in-construction
     *
     * @param rScene [ref] Scene supporting vehicles
     */
    static void update_construct(osp::active::ActiveScene &rScene);

    /**
     * Primary system update function
     *
     * Iterates over MCompRCSControllers and issues throttle commands to the
     * associated MCompRocket thrusters via wire
     *
     * @param rScene [ref] Scene with MCompRCSControllers to update
     */
    //static void update_controls(osp::active::ActiveScene &rScene);

    static void update_calculate(osp::active::ActiveScene &rScene);

private:
    /**
     * Command-thrust influence calculator
     *
     * Given a thruster's orientation and position relative to ship center of
     * mass, and a translation and rotation command, calculates how much
     * influence the thruster has on the commanded motion. Called on all
     * vehicle RCS thrusters to decide which are necessary to respond to the
     * maneuver command.
     *
     * @param posOset   [in] The position of the thruster relative to the ship CoM
     * @param direction [in] Direction that the thruster points
     * @param cmdTransl [in] Commanded translation vector
     * @param cmdRot    [in] Commanded axis of rotation
     */
    static float thruster_influence(
        osp::Vector3 posOset, osp::Vector3 direction,
        osp::Vector3 cmdTransl, osp::Vector3 cmdRot);

}; // SysMCompRCSController

//-----------------------------------------------------------------------------

class MCompRCSController
{
    friend SysMCompRCSController;

    using Percent = wire::Percent;
    using AttitudeControl = wire::AttitudeControl;

public:

    static inline std::string smc_mach_name = "RCSController";

    static constexpr osp::portindex_t<AttitudeControl> smc_wiCommandOrient{0};

    static constexpr osp::portindex_t<Percent> m_woThrottle{0};

}; // MCompRCSController


} // adera::active::machines
