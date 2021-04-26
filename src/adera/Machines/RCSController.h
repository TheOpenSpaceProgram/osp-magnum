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

#include <osp/Active/SysMachine.h>
#include <osp/Resource/blueprints.h>

namespace adera::active::machines
{

class MachineRCSController;

/**
 * A system which provides the logic that powers Reaction Control Systems.
 * Vehicles that use propulsive reaction control will possess a number of rocket
 * thrusters which need to know whether or not a given maneuver command requires
 * their contribution. Translation and orientation commands are received from
 * the command module and output as throttle command values, and the
 * MachineRockets associated with the RCS thrusters will fire.
 */
class SysMachineRCSController
{
public:

    static void add_functions(osp::active::ActiveScene& rScene);

    /**
     * Constructs MachineRCSControllers for vehicles in-construction
     *
     * @param rScene [ref] Scene supporting vehicles
     */
    static void update_construct(osp::active::ActiveScene &rScene);

    /**
     * Primary system update function
     *
     * Iterates over MachineRCSControllers and issues throttle commands to the
     * associated MachineRocket thrusters via wire
     *
     * @param rScene [ref] Scene with MachineRCSControllers to update
     */
    //static void update_controls(osp::active::ActiveScene &rScene);

    static void update_calculate(osp::active::ActiveScene &rScene);

    static void update_propagate(osp::active::ActiveScene &rScene);

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

}; // SysMachineRCSController

//-----------------------------------------------------------------------------

class MachineRCSController
{
    friend SysMachineRCSController;

public:

    static inline std::string smc_mach_name = "RCSController";


private:
    //osp::active::WireInput m_wiCommandOrient{this, "Orient"};
    //osp::active::WireOutput m_woThrottle{this, "Throttle"};

    osp::active::ActiveEnt m_rigidBody{entt::null};
}; // MachineRCSController


} // adera::active::machines
