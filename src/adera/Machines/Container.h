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

#include <osp/Active/activetypes.h>  // for ActiveEnt
#include <adera/ShipResources.h>

#include <string>                    // for std::string

#include <cstdint>                   // for std::uint64_t

// Forward declares to avoid unneeded includes
namespace osp::active { class ActiveScene; }
namespace osp { struct BlueprintMachine; }
namespace osp { struct PCompMachine; }

namespace adera::active::machines
{

class MCompContainer;

class SysMCompContainer
{
public:

    /**
     * Constructs MCompContainer for vehicles in-construction
     *
     * @param rScene [ref] Scene supporting vehicles
     */
    static void update_construct(osp::active::ActiveScene &rScene);

    static void update_containers(osp::active::ActiveScene& rScene);

    static MCompContainer& instantiate(
            osp::active::ActiveScene& rScene,
            osp::active::ActiveEnt ent,
            osp::PCompMachine const& config,
            osp::BlueprintMachine const& settings);

}; // class SysMCompContainer

//-----------------------------------------------------------------------------

class MCompContainer
{
    friend SysMCompContainer;

public:

    static inline std::string smc_mach_name = "Container";

    MCompContainer(osp::active::ActiveEnt ownID, float capacity, ShipResource resource);

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
    std::uint64_t request_contents(std::uint64_t quantity);

    /**
     * Compute the current mass of the container contents
     * @return The current mass, in kg
     */
    double compute_mass() const noexcept;
private:

    float m_capacity;
    ShipResource m_contents;
}; // class MCompContainer

inline MCompContainer::MCompContainer(osp::active::ActiveEnt ownID,
    float capacity, ShipResource resource)
 : m_capacity(capacity)
 , m_contents(resource)
{
    //m_outputs.value() = osp::active::wiretype::Pipe{ownID};
}

} // namespace adera::active::machines

