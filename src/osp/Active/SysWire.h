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

#include "SysVehicle.h"

#include "activetypes.h"
#include "../types.h"
#include "../Resource/blueprints.h"

#include <vector>

namespace osp::active
{

namespace wiretype
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
    template<typename TYPE_T>
    struct Signal
    {
        struct NodeState
        {
            TYPE_T m_write;
            TYPE_T m_value;
        };

        struct LinkState
        { };
    };

    /**
     * A rotation in global space
     */
    struct Attitude : public Signal<Attitude>
    {
        Quaternion m_global;
    };

    /**
     * A change in rotation. Separate pitch, yaw and roll
     */
    struct AttitudeControl : public Signal<AttitudeControl>
    {
        static inline std::string smc_wire_name = "AttitudeControl";

        AttitudeControl() = default;
        constexpr AttitudeControl(Vector3 value) noexcept : m_attitude(value) { }

        //  each (-1.0 .. 1.0)
        // pitch, yaw, roll
        Vector3 m_attitude;
    };

    /**
     * A percentage for something like throttle
     */
    struct Percent : public Signal<Percent>
    {
        static inline std::string smc_wire_name = "Percent";

        Percent() = default;
        constexpr Percent(float value) noexcept : m_percent(value) { }

        float m_percent;
    };

    enum class DeployOp : std::uint8_t { NONE, ON, OFF, TOGGLE };

    /**
     * Used to turn things on and off or ignite stuff
     */
    struct Deploy
    {
        //int m_stage;
        //bool m_on, m_off, m_toggle;
        DeployOp m_op;
    };

    /**
     * your typical logic gate boi
     */
    struct Logic : public Signal<Logic>
    {
        bool m_value;
    };

    /**
     * Pipe, generically exposes the output entity to the input one
     */
    struct Pipe
    {
        ActiveEnt m_source;
    };

} // namespace wiretype


//-----------------------------------------------------------------------------

/**
 * Merge two sorted integer-type vectors and remove duplicates
 */
template<typename VEC_T>
constexpr void vecset_merge(VEC_T& dest, VEC_T const& rSrc)
{
    dest.reserve(dest.size() + rSrc.size());

    auto const oldEnd = std::end(dest);

    // insert at end of vector
    dest.insert(oldEnd, std::begin(rSrc), std::end(rSrc));

    // merge both sorted sections
    std::inplace_merge(std::begin(dest), oldEnd, std::end(dest));

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
struct ACompWirePanel
{
    ACompWirePanel(uint16_t portCount)
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

/**
 * Scene-wide storage for WireNodes
 */
template<typename WIRETYPE_T>
struct ACompWireNodes
{
    // All the WIRETYPE_T node in the scene
    std::vector< WireNode<WIRETYPE_T> > m_nodes;

    std::vector< nodeindex_t<WIRETYPE_T> > m_needPropagate;
    std::unique_ptr<std::mutex> m_needPropagateMutex{std::make_unique<std::mutex>()};

    /**
     * Create a Node
     *
     * @param args [in] Arguments passed to WIRETYPE_T::NodeState constructor
     *                  (not yet implemented)
     *
     * @return Reference and Index to newly created node
     */
    template<typename ... ARGS_T>
    std::pair<WireNode<WIRETYPE_T>&, nodeindex_t<WIRETYPE_T>> create_node(
            ARGS_T&& ... args)
    {
        uint32_t index = m_nodes.size();
        WireNode<WIRETYPE_T> &node = m_nodes.emplace_back(std::forward<ARGS_T>(args) ...);
        return {node, nodeindex_t<WIRETYPE_T>(index)};
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
     * Request a propagation update for a set of Nodes. This locks a mutex.
     * @param request [in] Vector of node indices
     */
    void propagate_request(std::vector< nodeindex_t<WIRETYPE_T> > request)
    {
        std::sort(request.begin(), request.end());

        std::lock_guard<std::mutex> guard(*m_needPropagateMutex);

        vecset_merge(m_needPropagate, request);
    }
};

//-----------------------------------------------------------------------------

struct ACompWire
{
    // TODO: replace with beefier function order
    using updfunc_t = void(*)(ActiveScene&);
    std::vector<updfunc_t> m_updCalculate;
    std::vector<updfunc_t> m_updPropagate;

    std::vector<std::vector<ActiveEnt>> m_entToCalculate;
    std::vector<std::mutex> m_entToCalculateMutex;

    bool m_updateRequest{false};

    /**
     * Request to start or continue performing wire updates this frame
     */
    constexpr void request_update()
    {
        if (!m_updateRequest)
        {
            m_updateRequest = true;
        }
    }

}; // struct ACompWire

//-----------------------------------------------------------------------------

class SysWire
{
public:

    static void add_functions(ActiveScene& rScene);
    static void setup_default(
            ActiveScene& rScene,
            uint32_t machineTypeCount,
            std::vector<ACompWire::updfunc_t> updCalculate,
            std::vector<ACompWire::updfunc_t> updPropagate);

    /**
     * Perform wire updates.
     *
     * This calls even more update functions of configured Machines and Nodes
     * multiple times so that Machines can reliably send data to other Machines
     * within a single frame.
     *
     * @param rScene [ref] Scene supporting Wires
     */
    static void update_wire(ActiveScene& rScene);

    /**
     * Construct a vehicle's ACompWirePanels according to their Blueprint
     *
     * @param rScene     [ref] Scene supporting Vehicles and the right wire type
     * @param vehicleEnt [in] Vehicle entity
     * @param vehicle    [in] Vehicle component used to locate parts
     * @param vehicleBp  [in] Blueprint of vehicle
     */
    template<typename WIRETYPE_T>
    static void construct_panels(
            ActiveScene& rScene, ActiveEnt vehicleEnt,
            ACompVehicle const& vehicle,
            BlueprintVehicle const& vehicleBp);

    /**
     * @return ACompWireNodes<WIRETYPE_T> reference from scene root
     */
    template<typename WIRETYPE_T>
    static constexpr ACompWireNodes<WIRETYPE_T>& nodes(ActiveScene& rScene)
    {
        return rScene.reg_get< ACompWireNodes<WIRETYPE_T> >(rScene.hier_get_root());
    }

    /**
     * @return Vector of entities of a specified machine type to update
     */
    template<typename MACH_T>
    static constexpr std::vector<ActiveEnt>& to_update(ActiveScene& rScene)
    {
        return rScene.reg_get<ACompWire>(rScene.hier_get_root()).m_entToCalculate[mach_id<MACH_T>()];
    }

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
            ACompWirePanel<WIRETYPE_T>& panel,
            ActiveEnt machEnt,
            portindex_t<WIRETYPE_T> port,
            typename WIRETYPE_T::LinkState link);
}; // class SysWire


template<typename WIRETYPE_T>
bool SysWire::connect(
        WireNode<WIRETYPE_T>& node,
        nodeindex_t<WIRETYPE_T> nodeIndex,
        ACompWirePanel<WIRETYPE_T>& panel,
        ActiveEnt machEnt,
        portindex_t<WIRETYPE_T> port,
        typename WIRETYPE_T::LinkState link)
{
    if (panel.m_ports.size() <= size_t(port))
    {
        return false; // Invalid port
    }

    //uint32_t index = node.m_links.size();
    node.m_links.emplace_back(WireLink<WIRETYPE_T>{machEnt, port, link});
    panel.m_ports[size_t(port)].m_nodeIndex = nodeIndex;
    return true;
}

template<typename WIRETYPE_T>
void SysWire::construct_panels(
        ActiveScene& rScene, ActiveEnt vehicleEnt,
        ACompVehicle const& vehicle, BlueprintVehicle const& vehicleBp)
{
    wire_id_t const id = wiretype_id<WIRETYPE_T>();

    // Check if the vehicle blueprint stores the right wire panel type
    if (vehicleBp.m_wirePanels.size() <= id)
    {
        return;
    }

    // Initialize all Panels in the vehicle
    for (BlueprintWirePanel const &bpPanel : vehicleBp.m_wirePanels[id])
    {
        // Get part entity from vehicle
        ActiveEnt partEnt = vehicle.m_parts[bpPanel.m_partIndex];

        // Get machine entity from vehicle
        auto &machines = rScene.reg_get<ACompMachines>(partEnt);
        ActiveEnt machEnt = machines.m_machines[bpPanel.m_protoMachineIndex];

        // Create the panel supporting the right number of ports
        rScene.reg_emplace< ACompWirePanel<WIRETYPE_T> >(machEnt,
                                                         bpPanel.m_portCount);
    }
}

//-----------------------------------------------------------------------------

template<typename WIRETYPE_T>
class SysSignal
{
public:
    /**
     * Assign a new value to a signal Node
     *
     * If the value has changed, then the specified node index will be pushed to
     * the needPropagate vector. The value is written to an 'm_write' member
     * and changes will only be visible to connected inputs after the
     * propigation update
     *
     * @param rScene        [ref] Scene the Node is part of
     * @param newValue      [in] New value to assign to Node
     * @param rNode         [out] Node to assign value of
     * @param nodeIndex     [in] Index of Node
     * @param needPropagate [out] Vector for tracking changed nodes
     */
    static void signal_assign(
            ActiveScene& rScene,
            WIRETYPE_T&& newValue,
            WireNode<WIRETYPE_T>& rNode,
            nodeindex_t<WIRETYPE_T> nodeIndex,
            std::vector< nodeindex_t<WIRETYPE_T> >& needPropagate);

    /**
     * Read the blueprint of a Vehicle and construct needed Nodes and Links.
     *
     * The vehicle must already have fully initialized Panels
     *
     * @param rScene     [ref] Scene supporting Vehicles and the right Node type
     * @param vehicleEnt [in] Vehicle entity
     * @param vehicle    [in] Vehicle component used to locate parts
     * @param vehicleBp  [in] Blueprint of vehicle
     */
    static void signal_construct_nodes(
            ActiveScene& rScene, ActiveEnt vehicleEnt,
            ACompVehicle const& vehicle,
            BlueprintVehicle const& vehicleBp);

    /**
     * Scan the scene for vehicles in construction, and construct Nodes and
     * Panels if their Blueprint specifies it.
     *
     * @param rScene [ref] Scene supporting Vehicles and the right Node type
     */
    static void signal_update_construct(ActiveScene& rScene);

    /**
     * Propagate all signal Nodes in the scene.
     *
     * This 'applies changes' to Nodes by copying their new values written by
     * outputs to their current values to be read by connected inputs.
     *
     * @param rScene [ref] Scene supporting the right Node type
     */
    static void signal_update_propagate(ActiveScene& rScene);
};

template<typename WIRETYPE_T>
void SysSignal<WIRETYPE_T>::signal_assign(
        ActiveScene& rScene,
        WIRETYPE_T&& newValue,
        WireNode<WIRETYPE_T>& rNode,
        nodeindex_t<WIRETYPE_T> nodeIndex,
        std::vector< nodeindex_t<WIRETYPE_T> >& needPropagate)
{
    // POD comparison
    if (std::memcmp(&rNode.m_state.m_value, &newValue, sizeof(WIRETYPE_T)) != 0)
    {
        rNode.m_state.m_write = std::forward<WIRETYPE_T>(newValue);
        needPropagate.push_back(nodeIndex);
    }
}

template<typename WIRETYPE_T>
void SysSignal<WIRETYPE_T>::signal_construct_nodes(
        ActiveScene& rScene, ActiveEnt vehicleEnt,
        ACompVehicle const& vehicle, BlueprintVehicle const& vehicleBp)
{
    wire_id_t const id = wiretype_id<WIRETYPE_T>();

    // Check if the vehicle blueprint stores the right wire node type
    if (vehicleBp.m_wireNodes.size() <= id)
    {
        return;
    }

    // Initialize all nodes in the vehicle
    for (BlueprintWireNode const &bpNode : vehicleBp.m_wireNodes[id])
    {
        // Create the node
        auto &nodes = rScene.reg_get< ACompWireNodes<WIRETYPE_T> >(rScene.hier_get_root());
        auto const &[node, nodeIndex] = nodes.create_node();

        // Create Links
        for (BlueprintWireLink const &bpLink : bpNode.m_links)
        {
            // Get part entity from vehicle
            ActiveEnt partEnt = vehicle.m_parts[bpLink.m_partIndex];

            // Get machine entity from vehicle
            auto &machines = rScene.reg_get<ACompMachines>(partEnt);
            ActiveEnt machEnt = machines.m_machines[bpLink.m_protoMachineIndex];

            // Get panel to connect to
            auto& panel = rScene.reg_get< ACompWirePanel<WIRETYPE_T> >(machEnt);

            // Link them
            SysWire::connect<WIRETYPE_T>(node, nodeIndex, panel, machEnt,
                                         portindex_t<WIRETYPE_T>(bpLink.m_port),
                                         typename WIRETYPE_T::LinkState{});
        }
    }
}

template<typename WIRETYPE_T>
void SysSignal<WIRETYPE_T>::signal_update_construct(ActiveScene& rScene)
{
    auto view = rScene.get_registry()
            .view<osp::active::ACompVehicle,
                  osp::active::ACompVehicleInConstruction>();

    wire_id_t const id = wiretype_id<WIRETYPE_T>();

    for (auto [vehEnt, rVeh, rVehConstr] : view.each())
    {
        SysWire::construct_panels<WIRETYPE_T>(
                    rScene, vehEnt, rVeh, *(rVehConstr.m_blueprint));

        signal_construct_nodes(rScene, vehEnt, rVeh, *(rVehConstr.m_blueprint));
    }
}

template<typename WIRETYPE_T>
void SysSignal<WIRETYPE_T>::signal_update_propagate(ActiveScene& rScene)
{
    auto &rNodes = rScene.reg_get< ACompWireNodes<WIRETYPE_T> >(rScene.hier_get_root());
    auto &rWire = rScene.reg_get<ACompWire>(rScene.hier_get_root());

    // Machines to update accumulated throughout updating nodes
    std::vector<std::vector<ActiveEnt>> machToUpdate(rWire.m_entToCalculate.size());

    // Loop through all nodes that need to be propagated
    for (nodeindex_t<WIRETYPE_T> nodeIndex : rNodes.m_needPropagate)
    {
        WireNode<WIRETYPE_T> &rNode = rNodes.get_node(nodeIndex);

        // Overwrite value with value to write
        rNode.m_state.m_value = rNode.m_state.m_write;

        // Add connected machine inputs to machToUpdate vectors, as they now
        // require a calculation update
        for (auto it = std::next(std::begin(rNode.m_links));
             it != std::end(rNode.m_links); it++)
        {
            ActiveEnt ent = it->m_entity;
            auto const& type = rScene.reg_get<ACompMachineType>(ent);
            machToUpdate[size_t(type.m_type)].push_back(ent);
        }
    }

    rNodes.m_needPropagate.clear();

    // Commit machines to update to the scene-wide vectors
    for (int i = 0; i < machToUpdate.size(); i ++)
    {
        std::vector<ActiveEnt> &rVec = machToUpdate[i];
        if (!rVec.empty())
        {
            std::lock_guard<std::mutex> guard(rWire.m_entToCalculateMutex[i]);
            vecset_merge(rWire.m_entToCalculate[i], rVec);
            rWire.request_update();
        }
    }
}

} // namespace osp::active
