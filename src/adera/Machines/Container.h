/**
 * Open Space Program
 * Copyright © 2019-2021 Open Space Program Project
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

#include "../ShipResources.h"

namespace adera::active::machines
{

class MachineContainer;

class SysMachineContainer
{
public:

    static void add_functions(osp::active::ActiveScene& rScene);

    /**
     * Constructs MachineContainer for vehicles in-construction
     *
     * @param rScene [ref] Scene supporting vehicles
     */
    static void update_construct(osp::active::ActiveScene &rScene);

    static void update_containers(osp::active::ActiveScene& rScene);

    static MachineContainer& instantiate(
            osp::active::ActiveScene& rScene,
            osp::active::ActiveEnt ent,
            osp::PCompMachine const& config,
            osp::BlueprintMachine const& settings);

}; // class SysMachineContainer

//-----------------------------------------------------------------------------

class MachineContainer : public osp::active::Machine
{
    friend SysMachineContainer;

public:

    static inline std::string smc_mach_name = "Container";

    MachineContainer(osp::active::ActiveEnt ownID, float capacity, ShipResource resource);
    MachineContainer(MachineContainer&& move) noexcept;
    MachineContainer& operator=(MachineContainer&& move) noexcept;
    ~MachineContainer() = default;

    void propagate_output(osp::active::WireOutput *output) override;

    osp::active::WireInput* request_input(osp::WireInPort port) override;
    osp::active::WireOutput* request_output(osp::WireOutPort port) override;

    std::vector<osp::active::WireInput*> existing_inputs() override;
    std::vector<osp::active::WireOutput*> existing_outputs() override;

    constexpr const ShipResource& check_contents() const
    { return m_contents; }

    /**
     * Request a quantity of the contained resource
     *
     * Since the resources are stored as unsigned integers, avoiding wraparound
     * is crucial. This function wraps the resource withdrawal process by
     * internally checking the requested quantity of resource, bounds checking
     * it, and only returning as much resources as are available.
     *
     * @param quantity [in] The requested quantity of resource
     *
     * @return The amount of resource that was received
     */
    uint64_t request_contents(uint64_t quantity);

    /**
     * Compute the current mass of the container contents
     * @return The current mass, in kg
     */
    float compute_mass() const noexcept;
private:
    //osp::active::WireInput m_inputs;
    osp::active::WireOutput m_outputs;

    float m_capacity;
    ShipResource m_contents;
}; // class MachineContainer

/* MachineContainer */
inline MachineContainer::MachineContainer(osp::active::ActiveEnt ownID,
    float capacity, ShipResource resource)
    : Machine(true)
    , m_capacity(capacity)
    , m_contents(resource)
    , m_outputs(this, "output")
{
    m_outputs.value() = osp::active::wiretype::Pipe{ownID};
}

inline MachineContainer::MachineContainer(MachineContainer&& move) noexcept
    : Machine(std::move(move))
    , m_capacity(std::move(move.m_capacity))
    , m_contents(std::move(move.m_contents))
    , m_outputs(this, std::move(move.m_outputs))
{}

inline MachineContainer& MachineContainer::operator=(MachineContainer&& move) noexcept
{
    m_enable = std::exchange(move.m_enable, false);
    m_capacity = std::exchange(move.m_capacity, 0.0f);
    m_contents = std::move(move.m_contents);
    m_outputs = {this, std::move(move.m_outputs)};

    return *this;
}

} // namespace adera::active::machines

