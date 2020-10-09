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
#include <osp/Active/physics.h>
#include <osp/Resource/blueprints.h>

namespace adera::active::machines
{

class MachineRocket;


class SysMachineRocket :
        public osp::active::SysMachine<SysMachineRocket, MachineRocket>
{
public:

    static inline std::string smc_name = "Rocket";

    SysMachineRocket(osp::active::ActiveScene &scene);

    //void update_sensor();
    void update_physics(osp::active::ActiveScene& rScene);

    /**
     * Attach a visual exhaust plume effect to MachineRocket
     *
     * Searches the hierarchy under the specified MachineRocket entity and
     * attaches an ACompExhaustPlume to the rocket's plume node. A graphical
     * exhaust plume effect will be attached to the node by SysExhaustPlume
     * when it processes the component.
     * @param ent The MachineRocket entity
     */
    void attach_plume_effect(osp::active::ActiveEnt ent);

    /**
     * Attach a MachineRocket to an entity
     * 
     * Also attempts to attach a plume component to the appropriate child node
     * @param ent The entity that owns the MachineRocket
     * @return The new MachineRocket instance
     */
    osp::active::Machine& instantiate(osp::active::ActiveEnt ent,
        osp::PrototypeMachine config, osp::BlueprintMachine settings) override;

    osp::active::Machine& get(osp::active::ActiveEnt ent) override;

private:

    osp::active::UpdateOrderHandle_t m_updatePhysics;
}; // SysMachineRocket

/**
 *
 */
class MachineRocket : public osp::active::Machine
{
    friend SysMachineRocket;

public:
    MachineRocket(float thrust);
    MachineRocket(MachineRocket &&move) noexcept;
    MachineRocket& operator=(MachineRocket&& move) noexcept;

    MachineRocket(MachineRocket const& copy) = delete;
    MachineRocket& operator=(MachineRocket const& move) = delete;

    void propagate_output(osp::active::WireOutput *output) override;

    osp::active::WireInput* request_input(osp::WireInPort port) override;
    osp::active::WireOutput* request_output(osp::WireOutPort port) override;

    std::vector<osp::active::WireInput*> existing_inputs() override;
    std::vector<osp::active::WireOutput*> existing_outputs() override;

private:
    osp::active::WireInput m_wiGimbal   { this, "Gimbal"   };
    osp::active::WireInput m_wiIgnition { this, "Ignition" };
    osp::active::WireInput m_wiThrottle { this, "Throttle" };

    osp::active::ActiveEnt m_rigidBody  { entt::null };
    float m_thrust{0.0f};
}; // MachineRocket

inline MachineRocket::MachineRocket(float thrust)
 : Machine(true)
 , m_thrust(thrust)
{ }

inline MachineRocket::MachineRocket(MachineRocket&& move) noexcept
 : Machine(std::move(move))
 , m_wiGimbal(this, std::move(move.m_wiGimbal))
 , m_wiIgnition(this, std::move(move.m_wiIgnition))
 , m_wiThrottle(this, std::move(move.m_wiThrottle))
 , m_rigidBody(std::move(move.m_rigidBody))
 , m_thrust(std::exchange(move.m_thrust, 0.0f))
{ }

inline MachineRocket& MachineRocket::operator=(MachineRocket&& move) noexcept
{
    Machine::operator=(std::move(move));
    m_wiGimbal   = { this, std::move(move.m_wiGimbal)   };
    m_wiIgnition = { this, std::move(move.m_wiIgnition) };
    m_wiThrottle = { this, std::move(move.m_wiThrottle) };
    m_rigidBody  = std::move(move.m_rigidBody);
    m_thrust = std::exchange(move.m_thrust, 0.0f);
    return *this;
}

} // namespace adera::active::machines
