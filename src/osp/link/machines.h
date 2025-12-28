/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
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

#include "../core/array_view.h"
#include "../core/copymove_macros.h"
#include "../core/global_id.h"
#include "../core/keyed_vector.h"
#include "../core/strong_id.h"

#include <longeron/containers/intarray_multimap.hpp>
#include <longeron/containers/bit_view.hpp>
#include <longeron/id_management/registry_stl.hpp>
#include <longeron/id_management/id_set_stl.hpp>

#include <atomic>
#include <vector>

namespace osp::link
{

using MachTypeId    = osp::StrongId< std::uint16_t, struct DummyForMachTypeId >;
using MachAnyId     = osp::StrongId< std::uint32_t, struct DummyForMachAnyId >;
using MachLocalId   = osp::StrongId< std::uint32_t, struct DummyForMachLocalId >;

using NodeTypeId    = osp::StrongId< std::uint16_t, struct DummyForNodeTypeId >;
using NodeId        = osp::StrongId< std::uint32_t, struct DummyForNodeId >;

using PortId        = osp::StrongId< std::uint16_t, struct DummyForPortId >;
using JunctionId    = osp::StrongId< std::uint16_t, struct DummyForJunctionId >;
using JuncCustom    = std::uint16_t;

using MachTypeReg_t = GlobalIdReg<MachTypeId>;
using NodeTypeReg_t = GlobalIdReg<NodeTypeId>;

inline NodeTypeId const gc_ntSigFloat = NodeTypeReg_t::create();

/**
 * @brief Keeps track of Machines of a certain type that exists
 */
struct PerMachType
{
    lgrn::IdRegistryStl<MachLocalId>        localIds;
    osp::KeyedVec<MachLocalId, MachAnyId>   localToAny;
};

/**
 * @brief Keeps track of all Machines that exist and what type they are
 */
struct Machines
{
    lgrn::IdRegistryStl<MachAnyId>          ids;

    osp::KeyedVec<MachAnyId, MachTypeId>    machTypes;
    osp::KeyedVec<MachAnyId, MachLocalId>   machToLocal;

    osp::KeyedVec<MachTypeId, PerMachType>  perType;
};

struct MachineUpdater
{
    alignas(64) std::atomic<bool> requestMachineUpdateLoop {false};

    lgrn::IdSetStl<MachTypeId> machTypesDirty;

    // [MachTypeId][MachLocalId]
    osp::KeyedVec<MachTypeId, lgrn::IdSetStl<MachLocalId>> localDirty;
};

struct MachinePair
{
    MachLocalId     local;
    MachTypeId      type;
};

struct Junction
{
    MachLocalId     local;
    MachTypeId      type;
    JuncCustom      custom  {0};
};

/**
 * @brief Connects Machines together with intermediate Nodes
 */
struct Nodes
{
    // reminder: IntArrayMultiMap is kind of like an
    //           std::vector< std::vector<...> > but more memory efficient
    using NodeToMach_t = lgrn::IntArrayMultiMap<NodeId::entity_type, Junction>;
    using MachToNode_t = lgrn::IntArrayMultiMap<MachAnyId::entity_type, NodeId>;

    lgrn::IdRegistryStl<NodeId>         nodeIds;

    // Node-to-Machine connections
    // [NodeId][JunctionIndex] -> Junction (type, MachLocalId, custom int)
    NodeToMach_t                        nodeToMach;

    // Corresponding Machine-to-Node connections
    // [MachAnyId][PortIndex] -> NodeId
    MachToNode_t                        machToNode;
};

struct PortEntry
{
    NodeTypeId  type;
    PortId      port;
    JuncCustom  custom;
};

inline NodeId connected_node(lgrn::Span<NodeId const> portSpan, PortId port) noexcept
{
    return (portSpan.size() > port.value) ? portSpan[port.value] : NodeId{};
}

void copy_machines(
        Machines const &rSrc,
        Machines &rDst,
        ArrayView<MachAnyId> remapMachOut);

void copy_nodes(
        Nodes const &rSrcNodes,
        Machines const &rSrcMach,
        ArrayView<MachAnyId const> remapMach,
        Nodes &rDstNodes,
        Machines &rDstMach,
        ArrayView<NodeId> remapNodeOut);

} // namespace osp::wire
