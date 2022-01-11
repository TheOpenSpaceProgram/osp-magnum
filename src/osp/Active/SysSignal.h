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

#include "SysWire.h"

namespace osp::active
{

/**
 * Template for Signal types
 *
 * Signals have Single writers and multiple readers. They are analogous to
 * voltage levels in digital logic. Values are copied and reassigned without
 * describing any sort of mass-conserving flow.
 *
 * The first Link in a Node writes new values to the node (Machine's Output)
 * The remaining Links only read values (Connected Machine Inputs)
 */
template<typename WIRETYPE_T>
struct Signal
{
    // New values to write to the node, same data as wire type
    struct WriteValue : public WIRETYPE_T { };

    // State stored in each node, same data as wire type
    struct NodeState : public WIRETYPE_T { };

    // No link state needed
    struct LinkState { };
};

//-----------------------------------------------------------------------------

template<typename WIRETYPE_T>
class SysSignal
{
public:

    /**
     * Assign a new value to a signal Node
     *
     * If the value has changed, then changes will be pushed to the updNodes
     * vector. Changes will be applied after the node update
     *
     * @param rScene        [ref] Scene the Node is part of
     * @param newValue      [in] New value to assign to Node
     * @param rNode         [out] Node to assign value of
     * @param nodeIndex     [in] Index of Node
     * @param updNodes      [out] Vector of nodes to update
     */
    static void signal_assign(
            WIRETYPE_T&& newValue,
            WireNode<WIRETYPE_T> const& rNode,
            nodeindex_t<WIRETYPE_T> nodeIndex,
            UpdNodes_t<WIRETYPE_T>& updNodes);

    /**
     * Scan the scene for vehicles in construction, and construct Nodes and
     * Panels if their Blueprint specifies it.
     *
     * @param rScene [ref] Scene supporting Vehicles and the right Node type
     */
    //static void signal_update_construct(ActiveScene& rScene);

    /**
     * Update all signal Nodes in the scene.
     *
     * This applies changes to Nodes by processing all write requests, and also
     * marks connected machine inputs for update.
     *
     * @param rScene [ref] Scene supporting the right Node type
     */
    //static void signal_update_nodes(ActiveScene& rScene);

}; // class SysSignal

template<typename WIRETYPE_T>
void SysSignal<WIRETYPE_T>::signal_assign(
        WIRETYPE_T&& newValue,
        WireNode<WIRETYPE_T> const& rNode,
        nodeindex_t<WIRETYPE_T> nodeIndex,
        UpdNodes_t<WIRETYPE_T>& updNodes)
{
    if (static_cast<WIRETYPE_T const&>(rNode.m_state) != newValue)
    {
        updNodes.emplace_back(
                nodeIndex, typename WIRETYPE_T::WriteValue{
                        std::forward<WIRETYPE_T>(newValue)});
    }
}


}
