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

#include <osp/Resource/ImporterData.h>
#include <osp/Resource/resources.h>
#include <osp/logging.h>

namespace testapp
{

using osp::restypes::gc_importer;

VehicleBuilder::~VehicleBuilder()
{
    // clear resource owners
    for ([[maybe_unused]] auto && [_, value] : std::exchange(m_prefabs, {}))
    {
        m_pResources->owner_destroy(gc_importer, std::move(value.m_importer));
    }

    for ([[maybe_unused]] auto && rPrefabPair : std::exchange(m_data->m_partPrefabs, {}))
    {
        m_pResources->owner_destroy(gc_importer, std::move(rPrefabPair.m_importer));
    }
}

void VehicleBuilder::set_prefabs(std::initializer_list<SetPrefab> const& setPrefabs)
{
    auto const& endIt = std::end(m_prefabs);
    for (SetPrefab const& set : setPrefabs)
    {
        auto const& foundIt = m_prefabs.find(set.m_prefabName);
        if (foundIt != endIt)
        {
            auto &rPrefabPair = m_data->m_partPrefabs[std::size_t(set.m_part)];
            rPrefabPair.m_prefabId = foundIt->second.m_prefabId;
            rPrefabPair.m_importer = m_pResources->owner_create(gc_importer, foundIt->second.m_importer);
        }
        else
        {
            OSP_LOG_WARN("Prefab {} not found!", set.m_prefabName);
        }
    }
}

void VehicleBuilder::set_transform(std::initializer_list<SetTransform> const& setTransform)
{
    for (SetTransform const& set : setTransform)
    {
        m_data->m_partTransforms[std::size_t(set.m_part)] = set.m_transform;
    }
}

void VehicleBuilder::index_prefabs()
{
    for (unsigned int i = 0; i < m_pResources->ids(gc_importer).capacity(); ++i)
    {
        auto const resId = osp::ResId(i);
        if ( ! m_pResources->ids(gc_importer).exists(resId))
        {
            continue;
        }

        auto const *pPrefabData = m_pResources->data_try_get<osp::Prefabs>(gc_importer, resId);
        if (pPrefabData == nullptr)
        {
            continue; // No prefab data
        }

        for (osp::PrefabId j = 0; j < pPrefabData->m_prefabNames.size(); ++j)
        {
            osp::ResIdOwner_t owner = m_pResources->owner_create(gc_importer, resId);

            m_prefabs.emplace(pPrefabData->m_prefabNames[j],
                              osp::PrefabPair{std::move(owner), j});
        }
    }
}

osp::link::MachAnyId VehicleBuilder::create_machine(PartId const part,
                                                    MachTypeId const machType,
                                                    std::initializer_list<Connection> const& connections)
{
    auto &rData = m_data.value();
    osp::link::PerMachType &rPerMachType = rData.m_machines.m_perType[machType];

    MachTypeId const mach = rData.m_machines.m_ids.create();

    std::size_t const capacity = rData.m_machines.m_ids.capacity();
    rData.m_machines.m_machTypes.resize(capacity);
    rData.m_machines.m_machToLocal.resize(capacity);
    rData.m_machToPart.resize(capacity);
    for (PerNodeType &rPerNodeType : rData.m_nodePerType)
    {
        rPerNodeType.m_machToNode.ids_reserve(capacity);
        rPerNodeType.m_machToNodeCustom.ids_reserve(capacity);
    }

    MachTypeId const local = rPerMachType.m_localIds.create();
    rPerMachType.m_localToAny.resize(rPerMachType.m_localIds.capacity());

    rData.m_machines.m_machTypes[mach] = machType;
    rData.m_machines.m_machToLocal[mach] = local;
    rPerMachType.m_localToAny[local] = mach;

    rData.m_partMachCount[std::size_t(part)] ++;
    rData.m_machToPart[mach] = part;

    connect(mach, connections);

    return mach;
}

void VehicleBuilder::connect(MachAnyId const mach, std::initializer_list<Connection> const& connections)
{
    // get max port count for each node type
    auto &rData = m_data.value();
    std::vector<int> nodePortMax(rData.m_nodePerType.size(), 0);
    for (Connection const& connect : connections)
    {
        int &rPortMax = nodePortMax[connect.m_port.m_type];
        rPortMax = std::max<int>(rPortMax, connect.m_port.m_port + 1);
    }

    for (NodeTypeId nodeType = 0; std::size_t(nodeType) < rData.m_nodePerType.size(); ++nodeType)
    {
        int const portMax = nodePortMax[nodeType];
        PerNodeType &rPerNodeType = rData.m_nodePerType[nodeType];
        if (portMax != 0)
        {
            // reallocate each time :)
            rPerNodeType.m_machToNode.data_reserve(rPerNodeType.m_machToNode.data_capacity() + portMax);
            rPerNodeType.m_machToNodeCustom.data_reserve(rPerNodeType.m_machToNode.data_capacity() + portMax);

            // emplace and fill with null
            rPerNodeType.m_machToNode.emplace(mach, portMax);
            lgrn::Span<NodeId> const portSpan = rPerNodeType.m_machToNode[mach];
            std::fill(std::begin(portSpan), std::end(portSpan), lgrn::id_null<NodeId>());
            rPerNodeType.m_machToNodeCustom.emplace(mach, portMax);
            lgrn::Span<uint16_t> const customSpan = rPerNodeType.m_machToNodeCustom[mach];
            std::fill(std::begin(customSpan), std::end(customSpan), 0);

            for (Connection const& connect : connections)
            {
                if (connect.m_port.m_type == nodeType)
                {
                    customSpan[connect.m_port.m_port] = connect.m_port.m_custom;
                    portSpan[connect.m_port.m_port] = connect.m_node;
                    rPerNodeType.m_nodeConnectCount[connect.m_node] ++;
                    rPerNodeType.m_connectCountTotal ++;
                }
            }
        }
    }
}

using osp::link::MachLocalId;
using osp::link::Junction;
using osp::link::JuncCustom;

VehicleData VehicleBuilder::finalize_release()
{
    auto &rData = m_data.value();
    for (NodeTypeId nodeType = 0; std::size_t(nodeType) < rData.m_nodePerType.size(); ++nodeType)
    {
        PerNodeType &rPerNodeType = rData.m_nodePerType[nodeType];

        // reserve node-to-machine partitions
        rPerNodeType.m_nodeToMach.data_reserve(rPerNodeType.m_connectCountTotal);
        for (NodeId node : rPerNodeType.m_nodeIds.bitview().zeros())
        {
            rPerNodeType.m_nodeToMach.emplace(node, rPerNodeType.m_nodeConnectCount[node]);
            lgrn::Span<Junction> junction = rPerNodeType.m_nodeToMach[node];
            std::fill(std::begin(junction), std::end(junction), Junction{});
        }

        // assign node-to-machine
        for (MachAnyId const mach : rData.m_machines.m_ids.bitview().zeros())
        {
            lgrn::Span<NodeId> portSpan = rPerNodeType.m_machToNode[mach];
            lgrn::Span<JuncCustom> customSpan = rPerNodeType.m_machToNodeCustom[mach];

            auto customIt = std::begin(customSpan);
            for (NodeId node : portSpan)
            {
                lgrn::Span<Junction> const juncSpan = rPerNodeType.m_nodeToMach[node];

                // find empty spot
                // should always succeed, as they were reserved a few lines ago
                auto found = std::find_if(
                        std::begin(juncSpan), std::end(juncSpan),
                        [] (Junction const& pair)
                {
                    return pair.m_type == lgrn::id_null<MachTypeId>();
                });
                assert(found != std::end(juncSpan));

                MachTypeId const type = rData.m_machines.m_machTypes[mach];
                MachLocalId const local = rData.m_machines.m_machToLocal[mach];

                found->m_local  = local;
                found->m_type   = type;
                found->m_custom = *customIt;

                std::advance(customIt, 1);
            }
        }
    }

    VehicleData dataOut{std::move(*m_data)};
    m_data.emplace();
    return dataOut;
}

} // namespace testapp
