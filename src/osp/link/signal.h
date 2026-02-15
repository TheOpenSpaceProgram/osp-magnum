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

#include "link.h"

namespace osp::link
{


inline TransmitModeId const gc_tmLogicSignal    = GlobalLinkInfo::instance().create_transmitmode();

inline NodeTypeId const gc_ntSigFloat = GlobalLinkInfo::instance().create_nodetype(NodeTypeInfo{
    .transmitMode = gc_tmLogicSignal, .nodeDataSize = sizeof(float)
});



constexpr JuncCustom gc_sigIn  = 0;
constexpr JuncCustom gc_sigOut = 1;

inline PortEntry sigfloat_input(std::uint16_t portId) { return PortEntry{gc_ntSigFloat, PortId{portId}, gc_sigIn}; }
inline PortEntry sigfloat_output(std::uint16_t portId) { return PortEntry{gc_ntSigFloat, PortId{portId}, gc_sigOut}; }

template <typename VALUE_T>
using SignalValues_t = osp::KeyedVec<NodeId, VALUE_T>;

template <typename VALUE_T>
struct UpdateNodes
{
    lgrn::IdSetStl<NodeId>      nodeDirty;
    SignalValues_t<VALUE_T>     nodeNewValues;

    bool                        dirty{false};

    void assign(NodeId node, VALUE_T value)
    {
        dirty = true;
        nodeDirty.insert(node);
        nodeNewValues[node] = std::forward<VALUE_T>(value);
    }
};

inline void notify_connected_inputs(NodeId const nodeId, NodeType::NodeToMach_t const& nodeToMach, MachineUpdater& rUpdMach)
{
    for (Junction junc : nodeToMach[nodeId.value])
    {
        if (junc.custom == gc_sigIn)
        {
            // A machine of type "junc.m_type" has new values to read
            rUpdMach.machTypesDirty.insert(MachTypeId{junc.type});

            // Specify using local Id on which machine needs to update
            rUpdMach.localDirty[MachTypeId{junc.type}].insert(MachLocalId{junc.local});
        }
    }
}

} // namespace osp::wire
