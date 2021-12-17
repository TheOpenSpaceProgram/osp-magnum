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
#include "Container.h"                    // IWYU pragma: associated

#include <adera/ShipResources.h>          // for ShipResource, ShipResourceType

#include <osp/Active/basic.h>             // for ACompMass
#include <osp/Active/physics.h>
#include <osp/Active/SysVehicle.h>
#include <osp/Active/machines.h>          // for ACompMachines
#include <osp/Active/activetypes.h>       // for ActiveEnt, ActiveReg_t, active

#include <osp/Resource/Package.h>         // for Path, decompose_path, Package
#include <osp/Resource/machines.h>        // for mach_id, machine_id_t
#include <osp/Resource/Resource.h>        // for DependRes
#include <osp/Resource/blueprints.h>      // for BlueprintMachine, BlueprintV...
#include <osp/Resource/PrototypePart.h>

#include <osp/CommonPhysics.h>            // for ECollisionShape, ECollisionS...
#include <osp/Resource/PackageRegistry.h> // for PackageRegistry

#include <variant>                        // for std::get
#include <utility>                        // for std::exchange

// IWYU pragma: no_include <map>
// IWYU pragma: no_include <vector>
// IWYU pragma: no_include <type_traits>

using namespace osp;
using namespace osp::active;
using namespace adera::active::machines;

/* MCompContainer */

#if 0

std::uint64_t MCompContainer::request_contents(std::uint64_t quantity)
{
    if (quantity > m_contents.m_quantity)
    {
        return std::exchange(m_contents.m_quantity, 0);
    }

    m_contents.m_quantity -= quantity;
    return quantity;
}

double MCompContainer::compute_mass() const noexcept
{
    if (m_contents.m_type.empty()) { return 0.0f; }
    return m_contents.m_type->resource_mass(m_contents.m_quantity);
}

/* SysMCompContainer */

void SysMCompContainer::update_containers(ActiveScene& rScene)
{
    auto view = rScene.get_registry().view<MCompContainer, ACompMass>();

    /* Currently, the only thing to do is to compute the mass of the container
     * from the contents.
     * TODO: in the future, there should be a "dirty" tag for this property
     */
    for (ActiveEnt ent : view)
    {
        auto& container = view.get<MCompContainer>(ent);
        auto& mass = view.get<ACompMass>(ent);

        // Surpress narrowing conversion warning.
        mass.m_mass = static_cast<float>(container.compute_mass());
    }
}

MCompContainer& SysMCompContainer::instantiate(
        osp::active::ActiveScene& rScene,
        osp::active::ActiveEnt ent,
        osp::PCompMachine const& config,
        osp::BlueprintMachine const& settings)
{
    double capacity = std::get<double>(config.m_config.at("capacity"));

    ShipResource resource{};
    if (auto resItr = settings.m_config.find("resourcename");
        resItr != settings.m_config.end())
    {
        std::string const& resName = std::get<std::string>(resItr->second);
        Path resPath = decompose_path(resName);
        Package &rPkg = rScene.get_packages().find(resPath.prefix);

        resource.m_type = rPkg.get<ShipResourceType>(resPath.identifier);
        double fuelLevel = std::get<double>(settings.m_config.at("fuellevel"));
        resource.m_quantity = resource.m_type->resource_capacity(capacity * fuelLevel);
    }

    rScene.reg_emplace<ACompMass>(ent, 0.0f);
    // All tanks are cylindrical for now
    rScene.reg_emplace<ACompShape>(ent, phys::EShape::Cylinder);

    return rScene.reg_emplace<MCompContainer>(ent, ent, capacity, resource);
}


void SysMCompContainer::update_construct(ActiveScene &rScene)
{
    auto view = rScene.get_registry()
            .view<osp::active::ACompVehicle,
                  osp::active::ACompVehicleInConstruction>();

    machine_id_t const id = mach_id<MCompContainer>();

    for (auto const& [vehEnt, rVeh, rVehConstr] : view.each())
    {
        // Check if the vehicle blueprint might store MCompContainers
        if (rVehConstr.m_blueprint->m_machines.size() <= id)
        {
            continue;
        }

        BlueprintVehicle const& vehBp = *rVehConstr.m_blueprint;

        // Initialize all MCompContainers in the vehicle
        for (BlueprintMachine &mach : rVehConstr.m_blueprint->m_machines[id])
        {
            // Get part
            ActiveEnt partEnt = rVeh.m_parts[mach.m_partIndex];

            // Get machine entity previously reserved by SysVehicle
            auto& machines = rScene.reg_get<ACompMachines>(partEnt);
            ActiveEnt machEnt = machines.m_machines[mach.m_protoMachineIndex];

            BlueprintPart const& partBp = vehBp.m_blueprints[mach.m_partIndex];
            instantiate(rScene, machEnt,
                        vehBp.m_prototypes[partBp.m_protoIndex]
                                ->m_protoMachines[mach.m_protoMachineIndex],
                        mach);
        }
    }
}

#endif
