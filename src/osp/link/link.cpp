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
#include "link.h"

namespace osp::link
{

GlobalLinkInfo& GlobalLinkInfo::instance()
{
    static GlobalLinkInfo info;
    return info;
}

void copy_nodes(
        NodeConnections                               const &rSrcNodes,
        Machines                                      const &rSrcMach,
        Corrade::Containers::ArrayView<MachAnyId const>     remapMach,
        NodeConnections                                     &rDstNodes,
        Machines                                            &rDstMach,
        Corrade::Containers::ArrayView<NodeId>              remapNode)
{
    using lgrn::Span;

    // Create new node IDs
    for (NodeId const srcNode : rSrcNodes.nodeIds)
    {
        NodeId const dstNode = rDstNodes.nodeIds.create();
        remapNode[srcNode.value] = dstNode;
    }

    // Copy node-to-machine connections
    rDstNodes.nodeToMach.ids_reserve(rDstNodes.nodeIds.capacity());
    rDstNodes.nodeToMach.data_reserve(rDstNodes.nodeToMach.data_size()
                                        + rSrcNodes.nodeToMach.data_size());
    for (NodeId const srcNode : rSrcNodes.nodeIds)
    {
        NodeId const dstNode = remapNode[srcNode.value];
        Span<Junction const> srcJunction = rSrcNodes.nodeToMach[srcNode.value];
        rDstNodes.nodeToMach.emplace(dstNode.value, srcJunction.size());
        Span<Junction> dstJuncton = rDstNodes.nodeToMach[dstNode.value];

        auto dstJuncIt = std::begin(dstJuncton);
        for (Junction const& srcJunc : srcJunction)
        {
            MachTypeId const machType = srcJunc.type;
            MachAnyId const srcMach = rSrcMach.perType[machType].localToAny[srcJunc.local];
            MachAnyId const dstMach = remapMach[srcMach.value];
            MachLocalId const dstLocal = rDstMach.machToLocal[dstMach];

            dstJuncIt->local  = dstLocal;
            dstJuncIt->type   = machType;
            dstJuncIt->custom = srcJunc.custom;

            std::advance(dstJuncIt, 1);
        }
    }

    // copy mach-to-node connections
    rDstNodes.machToNode.ids_reserve(rDstMach.ids.capacity());
    rDstNodes.machToNode.data_reserve(rDstNodes.machToNode.data_size()
                                        + rSrcNodes.machToNode.data_size());
    for (MachAnyId const srcMach : rSrcMach.ids)
    {
        if (rSrcNodes.machToNode.contains(srcMach.value))
        {
            Span<NodeId const> srcPorts = rSrcNodes.machToNode[srcMach.value];
            MachAnyId const dstMach = remapMach[srcMach.value];
            rDstNodes.machToNode.emplace(dstMach.value, srcPorts.size());
            Span<NodeId> dstPorts = rDstNodes.machToNode[dstMach.value];

            auto dstPortIt = std::begin(dstPorts);
            for (NodeId const srcNode : srcPorts)
            {
                *dstPortIt = (srcNode != lgrn::id_null<NodeId>())
                           ? remapNode[srcNode.value]
                           : lgrn::id_null<NodeId>();

                std::advance(dstPortIt, 1);
            }
        }
    }
}

} // namespace osp::link
