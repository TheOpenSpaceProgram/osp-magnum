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
#include <utility>
#include <osp/Active/SysMachine.h>
#include <osp/Active/physics.h>
#include <osp/Resource/blueprints.h>
#include "adera/ShipResources.h"

namespace adera::active::machines
{

/**
 *
 */
class MachineRocket
{
    friend class SysMachineRocket;

    struct Parameters
    {
        float m_maxThrust;
        float m_specImpulse;
    };

public:

    static inline std::string smc_mach_name = "Rocket";

    /**
     * Return normalized power output level of the rocket this frame
     *
     * Returns a value [0,1] corresponding to the current output power of the
     * engine. This value is equal to the throttle input level, unless the
     * engine has run out of fuel, has a nonlinear throttle response, or some
     * similar reason. Used primarily by SysExhaustPlume to determine what the
     * exhaust plume effect should look like.
     *
     * @return normalized float [0,1] representing engine power output
     */
    float current_output_power() const
    {
        return m_powerOutput;
    }

private:
//    osp::active::WireInput m_wiGimbal{this, "Gimbal"};
//    osp::active::WireInput m_wiIgnition{this, "Ignition"};
//    osp::active::WireInput m_wiThrottle{this, "Throttle"};
//    std::vector<ResourceInput> m_resourceLines;

    osp::active::ActiveEnt m_rigidBody{entt::null};
    Parameters m_params;

    // Rocket power output for the current frame
    float m_powerOutput{0.0f};
}; // MachineRocket

//-----------------------------------------------------------------------------

class SysMachineRocket
{
public:

    static void add_functions(osp::active::ActiveScene& rScene);

    /**
     * Constructs MachineRockets for vehicles in-construction
     *
     * @param rScene [ref] Scene supporting vehicles
     */
    static void update_construct(osp::active::ActiveScene &rScene);

    /**
     * Updates all MachineRockets in the scene
     *
     * This function handles draining fuel and applying thrust.
     *
     * @param rScene [ref] Scene with MachineRockets to update
     */
    static void update_physics(osp::active::ActiveScene& rScene);

    /**
     * Attach a visual exhaust plume effect to MachineRocket
     *
     * Searches the hierarchy under the specified MachineRocket entity and
     * attaches an ACompExhaustPlume to the rocket's plume node. A graphical
     * exhaust plume effect will be attached to the node by SysExhaustPlume
     * when it processes the component.
     *
     * @param rScene [ref] Scene containing the following entities
     * @param part [in] Entity containing a plume in its descendents
     * @param mach [in] Entity containing MachineRocket
     */
    static void attach_plume_effect(osp::active::ActiveScene& rScene,
                                    osp::active::ActiveEnt part,
                                    osp::active::ActiveEnt mach);

    static MachineRocket& instantiate(
            osp::active::ActiveScene& rScene,
            osp::active::ActiveEnt ent,
            osp::PCompMachine const& config,
            osp::BlueprintMachine const& settings);

private:

}; // SysMachineRocket


} // namespace adera::active::machines
