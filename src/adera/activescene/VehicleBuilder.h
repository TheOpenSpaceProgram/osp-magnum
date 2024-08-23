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
#pragma once

#include <osp/activescene/vehicles.h>
#include <osp/core/copymove_macros.h>
#include <osp/core/resourcetypes.h>
#include <osp/link/machines.h>

#include <longeron/id_management/registry_stl.hpp>

#include <Corrade/Containers/StridedArrayView.h>

#include <entt/core/any.hpp>
#include <entt/container/dense_map.hpp>

#include <cstdint>
#include <array>
#include <optional>
#include <vector>

namespace adera
{

using osp::active::PartId;
using osp::active::WeldId;


struct StructureLink
{
    PartId m_greater;
    PartId m_less;
};

struct PerNodeType : osp::link::Nodes
{
    using MachToNodeCustom_t = lgrn::IntArrayMultiMap<osp::link::MachAnyId,
                                                      osp::link::JuncCustom>;

    MachToNodeCustom_t      m_machToNodeCustom; // parallel with m_machToNode
    entt::any               m_nodeValues;
    std::vector<int>        m_nodeConnectCount;
    int                     m_connectCountTotal{0};
};

struct VehicleData
{
    using MachToNodeCustom_t = lgrn::IntArrayMultiMap<osp::link::MachAnyId,
                                                      osp::link::JuncCustom>;
    using MapPartToMachines_t = osp::active::ACtxParts::MapPartToMachines_t;

    VehicleData() = default;
    OSP_MOVE_ONLY_CTOR(VehicleData);

    lgrn::IdRegistryStl<PartId>             m_partIds;
    std::vector<osp::Matrix4>               m_partTransformWeld;
    std::vector<osp::PrefabPair>            m_partPrefabs;
    std::vector<WeldId>                     m_partToWeld;
    MapPartToMachines_t                     m_partToMachines;

    lgrn::IdRegistryStl<WeldId>             m_weldIds;
    lgrn::IntArrayMultiMap<WeldId, PartId>  m_weldToParts;

    osp::link::Machines                     m_machines;
    std::vector<PartId>                     m_machToPart;

    std::vector<PerNodeType>                m_nodePerType;
};

/**
 * Used to easily create VehicleData
 */
class VehicleBuilder
{
    using MachAnyId     = osp::link::MachAnyId;
    using MachTypeId    = osp::link::MachTypeId;
    using NodeId        = osp::link::NodeId;
    using NodeTypeId    = osp::link::NodeTypeId;
    using PortEntry     = osp::link::PortEntry;

public:

    struct PartToWeld
    {
        PartId m_part;
        osp::Matrix4 m_transform;
    };

    using WeldVec_t = std::vector<VehicleBuilder::PartToWeld>;

    VehicleBuilder(osp::Resources *pResources)
     : m_pResources{pResources}
    {
        auto &rData = m_data.emplace();
        rData.m_machines.perType.resize(osp::link::MachTypeReg_t::size());
        rData.m_nodePerType.resize(osp::link::NodeTypeReg_t::size());
        index_prefabs();
    };

    ~VehicleBuilder();

    template <std::size_t N>
    [[nodiscard]] std::array<PartId, N> create_parts();

    struct SetPrefab
    {
        PartId m_part;
        std::string_view m_prefabName;
    };

    void set_prefabs(std::initializer_list<SetPrefab> const& setPrefab);


    WeldId weld(osp::ArrayView<PartToWeld const> toWeld);

    WeldId weld(std::initializer_list<PartToWeld const> const& toWeld)
    {
        return weld(osp::arrayView(toWeld));
    }

    osp::Matrix4 align_attach(PartId partA, std::string_view attachA,
                              PartId partB, std::string_view attachB);

    template <std::size_t N>
    [[nodiscard]] std::array<NodeId, N> create_nodes(NodeTypeId nodeType);

    template <typename VALUES_T>
    [[nodiscard]] VALUES_T& node_values(NodeTypeId nodeType);

    std::size_t node_capacity(NodeTypeId nodeType) const
    {
        return m_data->m_nodePerType[nodeType].nodeIds.capacity();
    }

    struct Connection
    {
        PortEntry m_port;
        NodeId m_node;
    };

    MachAnyId create_machine(PartId part, MachTypeId machType, std::initializer_list<Connection> const& connections);

    void connect(MachAnyId mach, std::initializer_list<Connection> const& connections);

    [[nodiscard]] VehicleData finalize_release();

private:

    void index_prefabs();

    osp::Resources *m_pResources;

    entt::dense_map< std::string_view, osp::PrefabPair > m_prefabs;

    std::vector<uint16_t> m_partMachCount;
    std::optional<VehicleData> m_data;

    // put more attachment data here
};


template <std::size_t N>
std::array<PartId, N> VehicleBuilder::create_parts()
{
    std::array<PartId, N> out;
    m_data->m_partIds.create(std::begin(out), std::end(out));

    std::size_t const capacity = m_data->m_partIds.capacity();
    m_partMachCount                 .resize(capacity);
    m_data->m_partPrefabs           .resize(capacity);
    m_data->m_partTransformWeld     .resize(capacity);
    m_data->m_partToWeld            .resize(capacity);
    m_data->m_weldToParts     .data_reserve(capacity);

    return out;
}


template <std::size_t N>
std::array<osp::link::NodeId, N> VehicleBuilder::create_nodes(NodeTypeId const nodeType)
{
    std::array<NodeId, N> out;

    PerNodeType &rPerNodeType = m_data->m_nodePerType[nodeType];

    rPerNodeType.nodeIds.create(std::begin(out), std::end(out));
    std::size_t const capacity = rPerNodeType.nodeIds.capacity();
    rPerNodeType.nodeToMach.ids_reserve(rPerNodeType.nodeIds.capacity());
    rPerNodeType.m_nodeConnectCount.resize(capacity, 0);

    return out;
}

template <typename VALUES_T>
VALUES_T& VehicleBuilder::node_values(NodeTypeId nodeType)
{
    PerNodeType &rPerNodeType = m_data->m_nodePerType[nodeType];

    // Emplace values container if it doesn't exist
    if ( ! bool(rPerNodeType.m_nodeValues))
    {
        rPerNodeType.m_nodeValues.emplace<VALUES_T>();
    }

    auto &rValues = entt::any_cast<VALUES_T&>(rPerNodeType.m_nodeValues);
    rValues.resize(node_capacity(nodeType));

    return rValues;
}

} // namespace adera
