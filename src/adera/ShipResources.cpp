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
#include "osp/Active/ActiveScene.h"
#include "osp/Resource/PrototypePart.h"

using namespace osp;
using namespace osp::active;
using namespace adera::active::machines;

/* ShipResourceType */

double ShipResourceType::resource_volume(uint64_t quantity) const
{
    double units = static_cast<double>(quantity) / std::pow(2.0, m_quanta);
    return units * m_volume;
}

double ShipResourceType::resource_mass(uint64_t quantity) const
{
    double units = static_cast<double>(quantity) / std::pow(2.0, m_quanta);
    return units * m_mass;
}

uint64_t ShipResourceType::resource_capacity(double volume) const
{
    double units = volume / m_volume;
    double quantaPerUnit = std::pow(2.0, m_quanta);
    return static_cast<uint64_t>(units * quantaPerUnit);
}

uint64_t ShipResourceType::resource_quantity(double mass) const
{
    double units = mass / m_mass;
    double quantaPerUnit = std::pow(2.0, m_quanta);
    return static_cast<uint64_t>(units * quantaPerUnit);
}

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
    return nullptr;
}

std::vector<WireInput*> MachineContainer::existing_inputs()
{
    return m_inputs;
}

std::vector<WireOutput*> MachineContainer::existing_outputs()
{
    return m_outputs;
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

SysMachineContainer::SysMachineContainer(ActiveScene& rScene)
    : SysMachine<SysMachineContainer, MachineContainer>(rScene)
    , m_updateContainers(rScene.get_update_order(), "mach_container", "", "mach_rocket",
        [this](ActiveScene& rScene) { this->update_containers(rScene); })
{ }

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

Machine& SysMachineContainer::instantiate(ActiveEnt ent,
    PrototypeMachine config, BlueprintMachine settings)
{
    float capacity = std::get<double>(config.m_config["capacity"]);

    ShipResource resource{};
    if (auto resItr = settings.m_config.find("resourcename");
        resItr != settings.m_config.end())
    {
        std::string_view resName = std::get<std::string>(resItr->second);
        Path resPath = decompose_path(resName);
        Package& pkg = m_scene.get_application().debug_find_package(resPath.prefix);

        resource.m_type = pkg.get<ShipResourceType>(resPath.identifier);
        resource.m_quantity = resource.m_type->resource_capacity(capacity);
    }

    m_scene.reg_emplace<ACompMass>(ent, 0.0f);
    // All tanks are cylindrical for now
    m_scene.reg_emplace<ACompShape>(ent, phys::ECollisionShape::CYLINDER);

    return m_scene.reg_emplace<MachineContainer>(ent, capacity, resource);
}

Machine& SysMachineContainer::get(ActiveEnt ent)
{
    return m_scene.reg_get<MachineContainer>(ent);
}
