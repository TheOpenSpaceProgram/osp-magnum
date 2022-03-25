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

#include "activetypes.h"
#include "../types.h"
#include "../Resource/blueprints.h"
#include "../Resource/machines.h"


#include <mutex>
#include <vector>
#include <utility>

namespace osp::active
{

/**
 * Merge two sorted vectors and remove duplicates
 */
template<typename VEC_T>
constexpr void vecset_merge(VEC_T& dest, VEC_T const& rSrc)
{
    size_t oldSize = dest.size();

    dest.reserve(oldSize + rSrc.size());

    // insert at end of vector
    dest.insert(std::end(dest), std::begin(rSrc), std::end(rSrc));

    // merge both sorted sections
    std::inplace_merge(std::begin(dest),
                       std::next(std::begin(dest), oldSize), std::end(dest));

    // Remove duplicates
    dest.erase(std::unique(std::begin(dest), std::end(dest)), std::end(dest));
}

//-----------------------------------------------------------------------------

/**
 * Stored in a WireNode and describes a connection to a Machine's Panel Port.
 * Links can optionally store a LinkState specified by the templated wire type.
 */
template<typename WIRETYPE_T>
struct WireLink
{
    ActiveEnt m_entity;
    portindex_t<WIRETYPE_T> m_port;
    typename WIRETYPE_T::LinkState m_state;
};

/**
 * Stores the wire value, and connects multiple Machines using Links
 */
template<typename WIRETYPE_T>
struct WireNode
{
    std::vector< WireLink<WIRETYPE_T> > m_links;
    typename WIRETYPE_T::NodeState m_state;
};

//-----------------------------------------------------------------------------

/**
 * Stored in a Panel as part of a Machine, used to connect to a Node's Link
 */
template<typename WIRETYPE_T>
struct WirePort
{
    nodeindex_t<WIRETYPE_T> m_nodeIndex{nullvalue< nodeindex_t<WIRETYPE_T> >()};

    constexpr bool is_connected() const noexcept
    {
        return m_nodeIndex != nullvalue< nodeindex_t<WIRETYPE_T> >();
    };
};

/**
 * Added to Machine entities to connect to Nodes
 */
template<typename WIRETYPE_T>
struct MCompWirePanel
{
    MCompWirePanel(uint16_t portCount)
     : m_ports(portCount)
    { }

    // Connect to nodes
    std::vector< WirePort<WIRETYPE_T> > m_ports;

    /**
     * Try to get an already connected Port
     * @param portIndex [in] Index of specified port
     * @return Pointer to Port, or nullptr if unconnected or nonexistent
     */
    WirePort<WIRETYPE_T> const* port(portindex_t<WIRETYPE_T> portIndex) const noexcept
    {
        if (m_ports.size() <= size_t(portIndex))
        {
            return nullptr;
        }
        WirePort<WIRETYPE_T> const& port = m_ports[size_t(portIndex)];

        return port.is_connected() ? &port : nullptr;
    }
};

//-----------------------------------------------------------------------------

/**
 * A request to write a new value to a Node, generated during machine calculate
 * updates
 */
template<typename WIRETYPE_T>
struct UpdNode
{
    constexpr UpdNode(nodeindex_t<WIRETYPE_T> node,
                      typename WIRETYPE_T::WriteValue write)
     : m_write(write)
     , m_node(std::underlying_type_t< nodeindex_t<WIRETYPE_T> >(node))
    { }

    constexpr bool operator<(UpdNode<WIRETYPE_T> const& rhs) const noexcept
    {
        return m_node < rhs.m_node;
    }

    constexpr bool operator==(UpdNode<WIRETYPE_T> const& rhs) const noexcept
    {
        return (m_node == rhs.m_node) && (m_write == rhs.m_write);
    }

    typename WIRETYPE_T::WriteValue m_write;
    std::underlying_type_t< nodeindex_t<WIRETYPE_T> > m_node;
};

template<typename WIRETYPE_T>
using UpdNodes_t = std::vector< UpdNode<WIRETYPE_T> >;

/**
 * Scene-wide storage for WireNodes
 */
template<typename WIRETYPE_T>
struct ACtxWireNodes
{
    // All the WIRETYPE_T node in the scene
    std::vector< WireNode<WIRETYPE_T> > m_nodes;

    // New values to write during node update
    // TODO: Use some kind of lock-free queue for future concurrency.
    UpdNodes_t<WIRETYPE_T> m_writeRequests;

    /**
     * Create a Node
     *
     * @param args [in] Arguments passed to WIRETYPE_T::NodeState constructor
     *                  (not yet implemented)
     *0
     * @return Reference and Index to newly created node
     */
    template<typename ... ARGS_T>
    std::pair<WireNode<WIRETYPE_T>&, nodeindex_t<WIRETYPE_T>> create_node(
            ARGS_T&& ... args)
    {
        uint32_t index = m_nodes.size();
        WireNode<WIRETYPE_T> &rNode
                = m_nodes.emplace_back(std::forward<ARGS_T>(args) ...);
        return {rNode, nodeindex_t<WIRETYPE_T>(index)};
    }

    /**
     * Get a Node by index
     * @param nodeIndex [in] Index to node to get
     * @return Reference to Node; not in stable memory.
     */
    WireNode<WIRETYPE_T>& get_node(nodeindex_t<WIRETYPE_T> nodeIndex) noexcept
    {
        return m_nodes[size_t(nodeIndex)];
    }

    WireNode<WIRETYPE_T> const& get_node(nodeindex_t<WIRETYPE_T> nodeIndex) const noexcept
    {
        return m_nodes[size_t(nodeIndex)];
    }

    /**
     * Request to write new values to a set of Nodes.
     * @param request [in] Sorted vector of new node values to write
     */
    void update_write(UpdNodes_t<WIRETYPE_T> const& write)
    {
        // TODO: Use some kind of lock-free queue for future concurrency
        m_writeRequests.insert(std::end(m_writeRequests),
                               std::begin(write), std::end(write));
    }
};

//-----------------------------------------------------------------------------

class SysWire
{
public:

    /**
     * Connect a Node to a Machine's Panel
     *
     * @param node      [ref] Node to connect
     * @param nodeIndex [in] Index of node to connect
     * @param panel     [in] Panel to connect
     * @param machEnt   [in] Machine entity Panel is part of
     * @param port      [in] Port number of Panel to connect to
     * @param link      [in] Optional Link state to store in Link
     *
     * @return true if connection is successfully made, false if port is invalid
     */
    template<typename WIRETYPE_T>
    static bool connect(
            WireNode<WIRETYPE_T>& node,
            nodeindex_t<WIRETYPE_T> nodeIndex,
            MCompWirePanel<WIRETYPE_T>& panel,
            ActiveEnt machEnt,
            portindex_t<WIRETYPE_T> port,
            typename WIRETYPE_T::LinkState link);

}; // class SysWire


template<typename WIRETYPE_T>
bool SysWire::connect(
        WireNode<WIRETYPE_T>& node,
        nodeindex_t<WIRETYPE_T> nodeIndex,
        MCompWirePanel<WIRETYPE_T>& panel,
        ActiveEnt machEnt,
        portindex_t<WIRETYPE_T> port,
        typename WIRETYPE_T::LinkState link)
{
    if (panel.m_ports.size() <= size_t(port))
    {
        return false; // Invalid port
    }

    node.m_links.emplace_back(WireLink<WIRETYPE_T>{machEnt, port, link});
    panel.m_ports[size_t(port)].m_nodeIndex = nodeIndex;
    return true;
}

} // namespace osp::active
