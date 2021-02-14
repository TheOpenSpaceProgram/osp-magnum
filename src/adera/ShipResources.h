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
#include <string>
#include <assert.h>

#include "osp/Resource/Resource.h"
#include "osp/Active/SysMachine.h"
#include "osp/CommonMath.h"
#include "osp/CommonPhysics.h"

namespace adera::active::machines
{

/**
 * Represents a type of consumable ship resource
 *
 * Resources might be quantized differently depending on the rate at which
 * they're consumed. This system allows this quantization to be configured
 * per-resource. Resources will be represented by 64-bit integers to maximize
 * the available precision to avoid infinite-fuel exploits.
 * The 64-bit range is divided into two sections by an arbitrary choice of unit.
 *
 * Example resource:
 * identifier: lox
 * name: Liquid Oxygen
 * quantaPerUnit: 2^16
 * mass: 1141.0 (kg) (per quantaPerUnit)
 * volume: 1.0 (m^3) (per quantaPerUnit)
 * 
 * Terminology:
 * 
 * UNIT - The quantity of resource used in its definition. For example, the 
 * above example defines one unit of Liquid Oxygen to contain 1 cubic meter of
 * resource, which masses 1141.0 kilograms.
 * 
 * QUANTA - The resource definition also contains a value which defines the
 * number of "quanta per unit" (QPU). This value is a power of two which
 * determines the smallest representable quantity of the resource. Given a QPU
 * value of 2^16, 16 bits of precision will be used to divide one unit of
 * resource into pieces. In the above exapmle, the smallest representable
 * quantity of resource (1) thus represents 1/(2^16) = 1.526e-5 units, or
 * 15.26ml of LOx. The remaining 48 bits of precision are free to represent
 * tank capacity.
 * 
 * A fuel tank with volume 10 m^3 would thus store 655360 units of LOx. With 16
 * bits dedicated to the subdivision of 1.0 m^3 of LOx, the remaining 48 bits can
 * represent a maximum fuel capacity up to 281474976710656 m^3 (321 Teratons) of
 * LOx. If you need to store a volume larger than this, we suggest you
 * reevaluate the design of your vessel.
 *
 * In contrast, a theoretical fuel with incredible energy density may only
 * be burned at a rate of micrograms per minute. With a quanta of 32 (and a base
 * volume unit of 1 m^3 as before), value of (1.0 m^3)*2e-10 may be represented,
 * with the tradeoff that the remaining 32 bits of precision may represent
 * "only" 4294967292 m^3.
 *
 */
struct ShipResourceType
{
    // A short, unique, identifying name readable by both human and machine
    const std::string m_identifier;

    // The full, screen-display name of the resource
    const std::string m_displayName;

    // 1/(QPU) is the smallest representable quantity of this resource; must be a power of 2
    const uint64_t m_quantaPerUnit;

    // The volume (in m^3) of one unit of this resource
    const float m_volumePerUnit;

    // The mass (in kg) of one unit of this resource
    const float m_massPerUnit;

    // The density of this resource (kg/m^3)
    const float m_density;

    // Compute the volume of the specified quantity of resource
    constexpr double resource_volume(uint64_t quantity) const
    {
        double units = static_cast<double>(quantity) / m_quantaPerUnit;
        return units * m_volumePerUnit;
    }

    // Compute the mass of the specified quantity of resource
    constexpr double resource_mass(uint64_t quantity) const
    {
        double units = static_cast<double>(quantity) / m_quantaPerUnit;
        return units * m_massPerUnit;
    }

    // Compute the quantity of resource that fits in the specified volume
    constexpr uint64_t resource_capacity(double volume) const
    {
        double units = volume / m_volumePerUnit;
        return static_cast<uint64_t>(units * m_quantaPerUnit);
    }

    // Compute the quantity of resource that masses the specified amount
    constexpr uint64_t resource_quantity(double mass) const
    {
        double units = mass / m_massPerUnit;
        return static_cast<uint64_t>(units * m_quantaPerUnit);
    }

    ShipResourceType(std::string identifier, std::string displayName,
        uint64_t quantaPerUnit, float volume, float mass, float density)
        : m_identifier(std::move(identifier))
        , m_displayName(std::move(displayName))
        , m_quantaPerUnit(quantaPerUnit)
        , m_volumePerUnit(volume)
        , m_massPerUnit(mass)
        , m_density(density)
    {
        assert(osp::math::is_power_of_2(m_quantaPerUnit));
    }
};

struct ShipResource
{
    osp::DependRes<ShipResourceType> m_type;
    uint64_t m_quantity;
};

class MachineContainer;

class SysMachineContainer
    : public osp::active::SysMachine<SysMachineContainer, MachineContainer>
{
public:
    static inline std::string smc_name = "Container";

    SysMachineContainer(osp::active::ActiveScene& rScene);

    SysMachineContainer(SysMachineContainer &&) = default;
    SysMachineContainer(SysMachineContainer const&) = delete;
    SysMachineContainer& operator=(SysMachineContainer &&) = default;
    SysMachineContainer& operator=(SysMachineContainer const&) = delete;

    static void update_containers(osp::active::ActiveScene& rScene);

    osp::active::Machine& instantiate(
        osp::active::ActiveEnt ent,
        osp::PrototypeMachine config,
        osp::BlueprintMachine settings) override;

    osp::active::Machine& get(osp::active::ActiveEnt ent) override;

private:
    osp::active::UpdateOrderHandle_t m_updateContainers;
}; // class SysMachineContainer

class MachineContainer : public osp::active::Machine
{
    friend SysMachineContainer;

public:
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
