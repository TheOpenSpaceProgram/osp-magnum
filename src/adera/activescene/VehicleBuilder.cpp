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

#include <osp/core/Resources.h>
#include <osp/util/logging.h>
#include <osp/vehicles/ImporterData.h>

namespace adera
{

#if 0

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

WeldId VehicleBuilder::weld(osp::ArrayView<PartToWeld const> toWeld)
{
    WeldId const weld = m_data->m_weldIds.create();
    m_data->m_weldToParts.ids_reserve(m_data->m_weldIds.capacity());

    PartId *pPartInWeld = m_data->m_weldToParts.emplace(weld.value, toWeld.size());

    for (PartToWeld const& set : toWeld)
    {
        m_data->m_partTransformWeld[set.m_part.value] = set.m_transform;

        m_data->m_partToWeld[set.m_part.value] = weld;
        (*pPartInWeld) = set.m_part;
        std::advance(pPartInWeld, 1);
    }

    return weld;
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
    osp::link::MachType &rPerMachType = rData.m_links.machtype[machType];

    MachAnyId const mach = rData.m_links.machIds.create();

    std::size_t const capacity = rData.m_links.machIds.capacity();
    rData.m_links.machTypeOf    .resize(capacity);
    rData.m_links.machlocalidOf .resize(capacity);
    rData.m_machToPart          .resize(capacity);
    for (osp::link::NodeType &rPerNodeType : rData.m_links.nodetype)
    {
        rPerNodeType.machToNode.ids_reserve(capacity);
    }

    MachLocalId const local = rPerMachType.localIds.create();
    rPerMachType.machanyIdOf.resize(rPerMachType.localIds.capacity());

    rData.m_links.machTypeOf[mach]      = machType;
    rData.m_links.machlocalidOf[mach]   = local;
    rPerMachType.machanyIdOf[local]     = mach;

    m_partMachCount[std::size_t(part)] ++;
    rData.m_machToPart[mach] = part;

    connect(mach, connections);

    return mach;
}

void VehicleBuilder::connect(MachAnyId const mach, std::initializer_list<Connection> const& connections)
{
    auto const& linkInfo = osp::link::GlobalLinkInfo::instance();

    // get max port count for each node type
    auto &rData = m_data.value();
    osp::KeyedVec<NodeTypeId, int> portSizeRequired(linkInfo.nodetypeIds.capacity(), 0);
    for (Connection const& connect : connections)
    {
        int &rPortMax = portSizeRequired[connect.m_port.type];
        rPortMax = std::max<int>(rPortMax, connect.m_port.port.value + 1);
    }

    for (NodeTypeId const nodeType : linkInfo.nodetypeIds)
    {
        int const portMax = portSizeRequired[nodeType];
        osp::link::NodeType &rPerNodeType = rData.m_links.nodetype[nodeType];
        if (portMax != 0)
        {
            // reallocate each time :)
            rPerNodeType.machToNode.data_reserve(rPerNodeType.machToNode.data_capacity() + portMax);
            rPerNodeType.m_machToNodeCustom.data_reserve(rPerNodeType.machToNode.data_capacity() + portMax);

            // emplace and fill with null
            rPerNodeType.machToNode.emplace(mach.value, portMax);
            lgrn::Span<NodeId> const portSpan = rPerNodeType.machToNode[mach.value];
            std::fill(std::begin(portSpan), std::end(portSpan), lgrn::id_null<NodeId>());
            rPerNodeType.m_machToNodeCustom.emplace(mach.value, portMax);
            lgrn::Span<uint16_t> const customSpan = rPerNodeType.m_machToNodeCustom[mach.value];
            std::fill(std::begin(customSpan), std::end(customSpan), 0);

            for (Connection const& connect : connections)
            {
                if (connect.m_port.type == nodeType)
                {
                    customSpan[connect.m_port.port.value]   = connect.m_port.custom;
                    portSpan[connect.m_port.port.value]     = connect.m_node;

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
    for (std::uint16_t nodeTypeInt = 0; nodeTypeInt < rData.m_nodePerType.size(); ++nodeTypeInt)
    {
        NodeTypeId const nodeType{nodeTypeInt};

        PerNodeType &rPerNodeType = rData.m_nodePerType[nodeType];

        // reserve node-to-machine partitions
        rPerNodeType.nodeToMach.data_reserve(rPerNodeType.m_connectCountTotal);
        for (NodeId node : rPerNodeType.nodeIds)
        {
            rPerNodeType.nodeToMach.emplace(node.value, rPerNodeType.m_nodeConnectCount[node]);
            lgrn::Span<Junction> junction = rPerNodeType.nodeToMach[node.value];
            std::fill(std::begin(junction), std::end(junction), Junction{});
        }

        // assign node-to-machine
        for (MachAnyId const mach : rData.m_machines.machIds)
        {
            lgrn::Span<NodeId> portSpan = rPerNodeType.machToNode[mach.value];
            lgrn::Span<JuncCustom> customSpan = rPerNodeType.m_machToNodeCustom[mach.value];

            for (int i = 0; i < portSpan.size(); ++i)
            {
                NodeId node = portSpan[i];

                if (node == lgrn::id_null<NodeId>())
                {
                    continue;
                }

                lgrn::Span<Junction> const juncSpan = rPerNodeType.nodeToMach[node.value];

                // find empty spot
                // should always succeed, as they were reserved a few lines ago
                auto found = std::find_if(
                        std::begin(juncSpan), std::end(juncSpan),
                        [] (Junction const& pair)
                {
                    return pair.type == lgrn::id_null<MachTypeId>();
                });
                assert(found != std::end(juncSpan));

                MachTypeId const type = rData.m_machines.machTypeOf[mach];
                MachLocalId const local = rData.m_machines.machlocalidOf[mach];

                found->local  = local;
                found->type   = type;
                found->custom = customSpan[i];
            }
        }
    }

    // Reserve part-to-machine partitions
    using osp::link::MachinePair;
    rData.m_partToMachines.ids_reserve(rData.m_partIds.capacity());
    rData.m_partToMachines.data_reserve(rData.m_machines.machIds.capacity());
    for (PartId const part : rData.m_partIds)
    {
        rData.m_partToMachines.emplace(part.value, m_partMachCount[part.value]);
    }

    // Assign part-to-machine partitions
    for (MachAnyId const mach : rData.m_machines.machIds)
    {
        MachLocalId const   local       = rData.m_machines.machlocalidOf[mach];
        MachTypeId const    type        = rData.m_machines.machTypeOf[mach];
        PartId const        part        = rData.m_machToPart[mach];
        auto const          machines    = lgrn::Span<MachinePair>{rData.m_partToMachines[part.value]};

        // Reuse machine count to track how many are currently added.
        // By the end, these should all be zero
        auto &rPartMachCount = m_partMachCount[part.value];
        assert(rPartMachCount != 0);
        -- rPartMachCount;

        machines[rPartMachCount] = { .local = local, .type = type };
    }

    assert(std::all_of(m_partMachCount.begin(), m_partMachCount.end(),
                       [] (auto const& count)
    {
        return count == 0;
    }));

    VehicleData dataOut{std::move(*m_data)};
    m_data.emplace();
    return dataOut;
}

#endif

} // namespace adera
