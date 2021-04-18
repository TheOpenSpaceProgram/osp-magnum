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
#include "VehicleBuilder.h"

using namespace testapp;

using osp::BlueprintPart;
using osp::BlueprintMachine;
using osp::PrototypePart;
using osp::PCompMachine;
using osp::DependRes;
using osp::Vector3;
using osp::Quaternion;
using osp::machine_id_t;

part_t VehicleBuilder::part_add(
        DependRes<PrototypePart>& prototype,
        const Vector3& translation,
        const Quaternion& rotation,
        const Vector3& scale)
{
    uint32_t protoIndex = m_vehicle.m_prototypes.size();

    // check if the Part Prototype is added already.
    for (size_t i = 0; i < protoIndex; i ++)
    {
        const DependRes<PrototypePart>& dep = m_vehicle.m_prototypes[i];

        if (dep == prototype)
        {
            // prototype was already added to the list
            protoIndex = i;
            break;
        }
    }

    if (protoIndex == m_vehicle.m_prototypes.size())
    {
        // Add the unlisted prototype to the end
        m_vehicle.m_prototypes.emplace_back(prototype);
    }

    size_t numMachines = prototype->m_protoMachines.size();

    // We now know which part prototype to initialize, create it
    uint32_t blueprintIndex = m_vehicle.m_blueprints.size();
    BlueprintPart &rPart = m_vehicle.m_blueprints.emplace_back(
                protoIndex, numMachines, translation, rotation, scale);

    // Add default machines from part prototypes

    for (size_t i = 0; i < numMachines; i ++)
    {
        PCompMachine const &protoMach = prototype->m_protoMachines[i];
        machine_id_t id = protoMach.m_type;
        m_vehicle.m_machines.resize(std::max(m_vehicle.m_machines.size(),
                                             size_t(id + 1)));

        BlueprintMachine &rBlueprintMach = m_vehicle.m_machines[id].emplace_back();
        rBlueprintMach.m_protoMachineIndex = i;
        rBlueprintMach.m_blueprintIndex = blueprintIndex;
        rBlueprintMach.m_config = protoMach.m_config;
    }

    return part_t(blueprintIndex);
}

Vector3 VehicleBuilder::part_offset(
        PrototypePart const& attachTo, std::string_view attachToName,
        PrototypePart const& toAttach, std::string_view toAttachName) noexcept
{
    Vector3 oset1{0.0f};
    Vector3 oset2{0.0f};

    for (osp::PCompName const& name : attachTo.m_partName)
    {
        if (name.m_name == attachToName)
        {
            oset1 = attachTo.m_partTransform[name.m_entity].m_translation;
            break;
        }
    }

    for (osp::PCompName const& name : toAttach.m_partName)
    {
        if (name.m_name == toAttachName)
        {
            oset2 = toAttach.m_partTransform[name.m_entity].m_translation;
            break;
        }
    }

    return oset1 - oset2;
}


std::pair<mach_t, osp::BlueprintMachine*> VehicleBuilder::machine_find(
        osp::machine_id_t id, part_t part) noexcept
{
    if (id >= m_vehicle.m_machines.size())
    {
        return {nullvalue<mach_t>(), nullptr}; // no machines of type
    }

    for (size_t i = 0; i < m_vehicle.m_machines[id].size(); i ++)
    {
        osp::BlueprintMachine& machineBp = m_vehicle.m_machines[id][i];
        if (machineBp.m_blueprintIndex == uint32_t(part))
        {
            // Found
            return {mach_t(i), &machineBp};
        }
    }
    return {nullvalue<mach_t>(), nullptr};
}


node_t<void> VehicleBuilder::wire_node_add(osp::wire_id_t id,
                                           osp::NodeMap_t config)
{
    // Resize wire type vector if it's too small
    if (m_vehicle.m_wireNodes.size() <= id)
    {
        m_vehicle.m_wireNodes.resize(size_t(id) + 1);
    }

    // Get vector of nodes
    std::vector<osp::BlueprintWireNode> &rNodes
            = m_vehicle.m_wireNodes[size_t(id)];
    uint32_t index = rNodes.size();

    // Create new node
    rNodes.emplace_back(osp::BlueprintWireNode{config, {}});
    return node_t<void>(index);
}

link_t<void> VehicleBuilder::wire_connect(
        osp::wire_id_t id, node_t<void> node, osp::NodeMap_t config,
        part_t part, mach_t mach, port_t<void> port)
{
    if (m_vehicle.m_wireNodes.size() <= id)
    {
        // No node of type is even registered
        return nullvalue< link_t<void> >();
    }

    // Get the BlueprintWireNode
    std::vector<osp::BlueprintWireNode> &rNodes
            = m_vehicle.m_wireNodes[size_t(id)];
    osp::BlueprintWireNode &rNode = rNodes[size_t(node)];

    // Create BlueprintWireLink
    uint32_t index = rNode.m_links.size();
    rNode.m_links.emplace_back(
            osp::BlueprintWireLink{config, uint32_t(part), uint16_t(mach), uint16_t(port)});

    return link_t<void>(index);
}

node_t<void> VehicleBuilder::wire_connect_signal(osp::wire_id_t id,
        part_t partOut, mach_t machOut, port_t<void> portOut,
        part_t partIn, mach_t machIn, port_t<void> portIn)
{
    // Create a node
    node_t<void> node = wire_node_add(id, {});

    // Link the Output first
    wire_connect(id, node, {{"output", 1}}, partOut, machOut, portOut);

    // Link the Input
    wire_connect(id, node, {{"input", 1}}, partIn, machIn, portIn);

    return node;
}
