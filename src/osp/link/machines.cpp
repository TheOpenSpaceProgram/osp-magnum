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
#include "machines.h"

namespace osp::link
{

NodeTypeId const gc_ntNumber = NodeTypeReg_t::create();

void copy_machines(
        Machines const &rSrc,
        Machines &rDst,
        Corrade::Containers::ArrayView<MachAnyId> remapMach)
{

    for (MachAnyId srcMach : rSrc.m_ids.bitview().zeros())
    {
        MachTypeId const srcType = rSrc.m_machTypes[srcMach];
        PerMachType &rDstPerMach = rDst.m_perType[srcType];

        // TODO: optimize multiple calls to IdRegistryStl::create()
        //       (linear search for non-zero uint64_t is done each call)
        MachAnyId const dstMach = rDst.m_ids.create();
        MachLocalId const dstLocal = rDstPerMach.m_localIds.create();

        // TODO: allocate once before this for loop
        rDst.m_machToLocal.resize(rDst.m_ids.capacity());
        rDst.m_machTypes.resize(rDst.m_ids.capacity());
        rDstPerMach.m_localToAny.resize(rDstPerMach.m_localIds.capacity());

        rDstPerMach.m_localToAny[dstLocal] = dstMach;
        rDst.m_machToLocal[dstMach] = dstLocal;
        remapMach[srcMach] = dstMach;
    }
}

void copy_nodes(
        Nodes const &rSrcNodes,
        Machines const &rSrcMach,
        Corrade::Containers::ArrayView<MachAnyId const> remapMach,
        Nodes &rDstNodes,
        Machines &rDstMach,
        Corrade::Containers::ArrayView<NodeId> remapNode)
{
    using lgrn::Span;

    // Create new node IDs
    for (NodeId srcNode : rSrcNodes.m_nodeIds.bitview().zeros())
    {
        NodeId const dstNode = rDstNodes.m_nodeIds.create();
        remapNode[srcNode] = dstNode;
    }

    // Copy node-to-machine connections
    rDstNodes.m_nodeToMach.ids_reserve(rDstNodes.m_nodeIds.capacity());
    rDstNodes.m_nodeToMach.data_reserve(rDstNodes.m_nodeToMach.data_size()
                                        + rSrcNodes.m_nodeToMach.data_size());
    for (NodeId srcNode : rSrcNodes.m_nodeIds.bitview().zeros())
    {
        NodeId const dstNode = remapNode[srcNode];
        Span<Junction const> srcJunction = rSrcNodes.m_nodeToMach[srcNode];
        rDstNodes.m_nodeToMach.emplace(dstNode, srcJunction.size());
        Span<Junction> dstJuncton = rDstNodes.m_nodeToMach[dstNode];

        auto dstJuncIt = std::begin(dstJuncton);
        for (Junction const& srcJunc : srcJunction)
        {
            MachTypeId const machType = srcJunc.m_type;
            MachAnyId const srcMach = rSrcMach.m_perType[machType].m_localToAny[srcJunc.m_local];
            MachAnyId const dstMach = remapMach[srcMach];
            MachLocalId const dstLocal = rDstMach.m_machToLocal[dstMach];

            dstJuncIt->m_local  = dstLocal;
            dstJuncIt->m_type   = machType;
            dstJuncIt->m_custom = srcJunc.m_custom;

            std::advance(dstJuncIt, 1);
        }
    }

    // copy mach-to-node connections
    rDstNodes.m_machToNode.ids_reserve(rDstMach.m_ids.capacity());
    rDstNodes.m_machToNode.data_reserve(rDstNodes.m_machToNode.data_size()
                                        + rSrcNodes.m_machToNode.data_size());
    for (MachAnyId srcMach : rSrcMach.m_ids.bitview().zeros())
    {
        if (rSrcNodes.m_machToNode.contains(srcMach))
        {
            Span<NodeId const> srcPorts = rSrcNodes.m_machToNode[srcMach];
            MachAnyId const dstMach = remapMach[srcMach];
            rDstNodes.m_machToNode.emplace(dstMach, srcPorts.size());
            Span<NodeId> dstPorts = rDstNodes.m_machToNode[dstMach];

            auto dstPortIt = std::begin(dstPorts);
            for (NodeId const srcNode : srcPorts)
            {
                *dstPortIt = remapNode[srcNode];

                std::advance(dstPortIt, 1);
            }
        }
    }
}



} // namespace osp::link
