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
#include "ShipResources.h"

#include <osp/Resource/PrototypePart.h>
#include <osp/Active/ActiveScene.h>
#include <osp/Active/physics.h>
#include <osp/Active/SysVehicle.h>

using namespace osp;
using namespace osp::active;
using namespace adera::active::machines;

/* MachineContainer */

void MachineContainer::propagate_output(WireOutput* output)
{

}

WireInput* MachineContainer::request_input(WireInPort port)
{
    return nullptr;
}

WireOutput* MachineContainer::request_output(WireOutPort port)
{
    return &m_outputs;
}

std::vector<WireInput*> MachineContainer::existing_inputs()
{
    return {};
}

std::vector<WireOutput*> MachineContainer::existing_outputs()
{
    return {&m_outputs};
}

uint64_t MachineContainer::request_contents(uint64_t quantity)
{
    if (quantity > m_contents.m_quantity)
    {
        return std::exchange(m_contents.m_quantity, 0);
    }

    m_contents.m_quantity -= quantity;
    return quantity;
}

float MachineContainer::compute_mass() const noexcept
{
    if (m_contents.m_type.empty()) { return 0.0f; }
    return m_contents.m_type->resource_mass(m_contents.m_quantity);
}

/* SysMachineContainer */

void SysMachineContainer::add_functions(ActiveScene& rScene)
{
    rScene.debug_update_add(rScene.get_update_order(), "mach_container", "", "mach_rocket",
                            &SysMachineContainer::update_containers);
    rScene.debug_update_add(rScene.get_update_order(), "mach_container_construct", "vehicle_activate", "vehicle_modification",
                            &SysMachineContainer::update_construct);
}

void SysMachineContainer::update_containers(ActiveScene& rScene)
{
    auto view = rScene.get_registry().view<MachineContainer, ACompMass>();

    /* Currently, the only thing to do is to compute the mass of the container
     * from the contents.
     * TODO: in the future, there should be a "dirty" tag for this property
     */
    for (ActiveEnt ent : view)
    {
        auto& container = view.get<MachineContainer>(ent);
        auto& mass = view.get<ACompMass>(ent);

        mass.m_mass = container.compute_mass();
    }
}

MachineContainer& SysMachineContainer::instantiate(
        osp::active::ActiveScene& rScene,
        osp::active::ActiveEnt ent,
        osp::PrototypeMachine const& config,
        osp::BlueprintMachine const& settings)
{
    float capacity = std::get<double>(config.m_config.at("capacity"));

    ShipResource resource{};
    if (auto resItr = settings.m_config.find("resourcename");
        resItr != settings.m_config.end())
    {
        std::string_view resName = std::get<std::string>(resItr->second);
        Path resPath = decompose_path(resName);
        Package& pkg = rScene.get_application().debug_find_package(resPath.prefix);

        resource.m_type = pkg.get<ShipResourceType>(resPath.identifier);
        double fuelLevel = std::get<double>(settings.m_config.at("fuellevel"));
        resource.m_quantity = resource.m_type->resource_capacity(capacity * fuelLevel);
    }

    rScene.reg_emplace<ACompMass>(ent, 0.0f);
    // All tanks are cylindrical for now
    rScene.reg_emplace<ACompShape>(ent, phys::ECollisionShape::CYLINDER);

    return rScene.reg_emplace<MachineContainer>(ent, ent, capacity, resource);
}


void SysMachineContainer::update_construct(ActiveScene &rScene)
{
    auto view = rScene.get_registry()
            .view<osp::active::ACompVehicle,
                  osp::active::ACompVehicleInConstruction>();

    machine_id_t const id = mach_id<MachineContainer>();

    for (auto [vehEnt, rVeh, rVehConstr] : view.each())
    {
        // Check if the vehicle blueprint might store UserControls
        if (rVehConstr.m_blueprint->m_machines.size() <= id)
        {
            continue;
        }

        BlueprintVehicle const& vehBp = *rVehConstr.m_blueprint;

        // Initialize all UserControls in the vehicle
        for (BlueprintMachine &mach : rVehConstr.m_blueprint->m_machines[id])
        {
            // Get part
            ActiveEnt partEnt = rVeh.m_parts[mach.m_blueprintIndex];

            // Get machine entity previously reserved by SysVehicle
            auto& machines = rScene.reg_get<ACompMachines>(partEnt);
            ActiveEnt machEnt = machines.m_machines[mach.m_protoMachineIndex];

            BlueprintPart const& partBp = vehBp.m_blueprints[mach.m_blueprintIndex];
            instantiate(rScene, machEnt,
                        vehBp.m_prototypes[partBp.m_protoIndex]
                                ->m_protoMachines[mach.m_protoMachineIndex],
                        mach);
            rScene.reg_emplace<ACompMachineType>(machEnt, id,
                    [] (ActiveScene &rScene, ActiveEnt ent) -> Machine&
                    {
                        return rScene.reg_get<MachineContainer>(ent);
                    });
        }
    }
}
