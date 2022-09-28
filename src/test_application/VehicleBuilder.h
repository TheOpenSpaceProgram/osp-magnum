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

#include <osp/Active/parts.h>
#include <osp/Resource/resourcetypes.h>
#include <osp/types.h>
#include <osp/link/machines.h>

#include <longeron/id_management/registry_stl.hpp>

#include <entt/core/any.hpp>
#include <entt/container/dense_map.hpp>

#include <cstdint>
#include <array>
#include <optional>
#include <vector>

namespace testapp
{

enum class AttachId : uint32_t { };
enum class PartId : uint32_t { };

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
    VehicleData() = default;
    VehicleData(VehicleData const& copy) = delete;
    VehicleData(VehicleData&& move) = default;


    lgrn::IdRegistryStl<PartId>         m_partIds;
    std::vector<osp::Matrix4>           m_partTransforms;
    std::vector<osp::PrefabPair>        m_partPrefabs;
    std::vector<uint16_t>               m_partMachCount;

    lgrn::IdRegistryStl<AttachId>       m_attachments;
    std::vector<StructureLink>          m_attachLinks;

    osp::link::Machines                 m_machines;
    std::vector<PartId>                 m_machToPart;

    std::vector<PerNodeType>            m_nodePerType;
};

/**
 * Used to easily create Vehicle blueprints
 */
class VehicleBuilder
{
    using MachAnyId     = osp::link::MachAnyId;
    using MachTypeId    = osp::link::MachTypeId;
    using NodeId        = osp::link::NodeId;
    using NodeTypeId    = osp::link::NodeTypeId;
    using PortEntry     = osp::link::PortEntry;

public:

    VehicleBuilder(osp::Resources *pResources)
     : m_pResources{pResources}
    {
        auto &rData = m_data.emplace();
        rData.m_machines.m_perType.resize(osp::link::MachTypeReg_t::size());
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

    struct SetTransform
    {
        PartId m_part;
        osp::Matrix4 const& m_transform;
    };

    void set_transform(std::initializer_list<SetTransform> const& setTransform);

    osp::Matrix4 align_attach(PartId partA, std::string_view attachA,
                              PartId partB, std::string_view attachB);

    template <std::size_t N>
    [[nodiscard]] std::array<NodeId, N> create_nodes(NodeTypeId nodeType);

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

    std::optional<VehicleData> m_data;

    // put more attachment data here
};


template <std::size_t N>
std::array<PartId, N> VehicleBuilder::create_parts()
{
    std::array<PartId, N> out;
    m_data->m_partIds.create(std::begin(out), std::end(out));

    std::size_t const capacity = m_data->m_partIds.capacity();
    m_data->m_partMachCount.resize(capacity);
    m_data->m_partPrefabs.resize(capacity);
    m_data->m_partTransforms.resize(capacity);

    return out;
}


template <std::size_t N>
std::array<osp::link::NodeId, N> VehicleBuilder::create_nodes(NodeTypeId const nodeType)
{
    std::array<NodeId, N> out;

    PerNodeType &rPerNodeType = m_data->m_nodePerType[nodeType];

    rPerNodeType.m_nodeIds.create(std::begin(out), std::end(out));
    std::size_t const capacity = rPerNodeType.m_nodeIds.capacity();
    rPerNodeType.m_nodeToMach.ids_reserve(rPerNodeType.m_nodeIds.capacity());
    rPerNodeType.m_nodeConnectCount.resize(capacity, 0);

    return out;
}

struct ACtxVehicleSpawnVB
{
    std::vector<VehicleData const*> m_dataVB;

};

} // namespace testapp
