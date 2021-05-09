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
        rBlueprintMach.m_partIndex = blueprintIndex;
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
        return {osp::nullvalue<mach_t>(), nullptr}; // no machines of type
    }

    for (size_t i = 0; i < m_vehicle.m_machines[id].size(); i ++)
    {
        osp::BlueprintMachine& machineBp = m_vehicle.m_machines[id][i];
        if (machineBp.m_partIndex == uint32_t(part))
        {
            // Found
            return {mach_t(machineBp.m_protoMachineIndex), &machineBp};
        }
    }
    return {osp::nullvalue<mach_t>(), nullptr};
}


nodeindex_t<void> VehicleBuilder::wire_node_add(osp::wire_id_t id,
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
    return nodeindex_t<void>(index);
}

linkindex_t<void> VehicleBuilder::wire_connect(
        osp::wire_id_t id, nodeindex_t<void> node, osp::NodeMap_t config,
        part_t part, mach_t mach, portindex_t<void> port)
{
    if (m_vehicle.m_wireNodes.size() <= id)
    {
        // No node of type is even registered
        return osp::nullvalue< linkindex_t<void> >();
    }

    // Keep track of wire connections
    auto nodeIt = m_nodeIndexMap.try_emplace({id, part, mach, port}, node);
    if (!nodeIt.second)
    {
        // Error! Port already connected!
        return osp::nullvalue<linkindex_t<void>>();
    }

    // Get the BlueprintWireNode
    std::vector<osp::BlueprintWireNode> &rNodes
            = m_vehicle.m_wireNodes[size_t(id)];
    osp::BlueprintWireNode &rNode = rNodes[size_t(node)];

    // Create BlueprintWireLink
    uint32_t index = rNode.m_links.size();
    rNode.m_links.emplace_back(
            osp::BlueprintWireLink{config, uint32_t(part), uint16_t(mach),
                                   uint16_t(port)});

    // Update panel. There's no easy way to find the correct BlueprintWirePanel
    // from m_vehicle.m_wirePanels from a wire index, part index, and machine
    // index. We can search the vector if the right panel already exists, but
    // here we're using a map
    uint32_t newIndex = m_vehicle.m_wirePanels.size();
    auto panelIt = m_panelIndexMap.try_emplace({id, part, mach}, newIndex);

    uint16_t minPortCount = uint16_t(port) + 1;

    if (panelIt.second)
    {
        // new panel entry added
        m_vehicle.m_wirePanels.resize(std::max(m_vehicle.m_wirePanels.size(),
                                               size_t(id + 1)));
        m_vehicle.m_wirePanels[size_t(id)].emplace_back(
                    osp::BlueprintWirePanel{uint32_t(part), uint16_t(mach),
                                            uint16_t(minPortCount)});
    }
    else
    {
        // update port count of existing entry
        osp::BlueprintWirePanel &panel
                = m_vehicle.m_wirePanels[size_t(id)][panelIt.first->second];
        panel.m_portCount = std::max<uint16_t>(panel.m_portCount,
                                               uint16_t(minPortCount));
    }

    return linkindex_t<void>(index);
}

nodeindex_t<void> VehicleBuilder::wire_connect_signal(osp::wire_id_t id,
        part_t partOut, mach_t machOut, portindex_t<void> portOut,
        part_t partIn, mach_t machIn, portindex_t<void> portIn)
{
    if (m_nodeIndexMap.find({id, partIn, machIn, portIn}) != m_nodeIndexMap.end())
    {
        // Input already connected
        return osp::nullvalue< nodeindex_t<void> >();
    }

    auto nodeOutIt = m_nodeIndexMap.find({id, partOut, machOut, portOut});

    // Create a or get node
    nodeindex_t<void> node = (nodeOutIt != m_nodeIndexMap.end())
                           ? nodeOutIt->second : wire_node_add(id, {});

    // Link the Output (writes into node) first
    wire_connect(id, node, {{ }}, partOut, machOut, portOut);

    // Link the Input (reads node)
    wire_connect(id, node, {{ }}, partIn, machIn, portIn);

    return node;
}
