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

    template<typename TYPE_T>
    struct Signal
    {
        //enum class LinkType : uint8_t { Input, Output };
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
        //  each (-1.0 .. 1.0)
        // pitch, yaw, roll
        Vector3 m_attitude;

        //Quaternion m_precise;
        //Quaternion m_rot;
        //Vector3 m_yawpitchroll
    };

    /**
     * A change in rotation in axis angle
     *
     */
    //struct AttitudeControlPrecise
    //{
    //    //Quaternion m_precise;
    //    Vector3 m_axis;
    //};

    /**
     * For something like throttle
     */
    struct Percent : public Signal<Percent>
    {
        static inline std::string smc_wire_name = "Percent";

        Percent() = default;
        constexpr Percent(float value) noexcept : m_percent(value) { }
//        constexpr bool operator==(Percent const& rhs)
//        { return m_value == rhs.m_value; }

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


template<typename WIRETYPE_T>
struct WireLink
{
    ActiveEnt m_entity;
    portindex_t<WIRETYPE_T> m_port;
    typename WIRETYPE_T::LinkState m_state;
};

/**
 * Stores the wire value, and connects to machine's ACompWirePanel
 */
template<typename WIRETYPE_T>
struct WireNode
{
    std::vector< WireLink<WIRETYPE_T> > m_links;
    typename WIRETYPE_T::NodeState m_state;
};

template<typename WIRETYPE_T>
struct WirePort
{
    nodeindex_t<WIRETYPE_T> m_nodeIndex{nullvalue< nodeindex_t<WIRETYPE_T> >()};
    //typename WIRETYPE_T::LinkState m_linktype;
    constexpr bool connected() const noexcept
    {
        return m_nodeIndex != nullvalue< nodeindex_t<WIRETYPE_T> >();
    };
};

/**
 * Connects machines to WireNodes
 */
template<typename WIRETYPE_T>
struct ACompWirePanel
{

    ACompWirePanel(uint16_t portCount)
     : m_ports(portCount)
    { }

    // Connect to nodes
    std::vector< WirePort<WIRETYPE_T> > m_ports;

    WirePort<WIRETYPE_T> const* connection(portindex_t<WIRETYPE_T> portIndex) const noexcept
    {
        if (m_ports.size() <= size_t(portIndex))
        {
            return nullptr;
        }
        WirePort<WIRETYPE_T> const& port = m_ports[size_t(portIndex)];

        return port.connected() ? &port : nullptr;
    }
};

/**
 * Scene-wide storage for WireNodes
 */
template<typename WIRETYPE_T>
struct ACompWireNodes
{
    std::vector< WireNode<WIRETYPE_T> > m_nodes;

    std::vector< nodeindex_t<WIRETYPE_T> > m_needPropagate;
    std::unique_ptr<std::mutex> m_needPropagateMutex{std::make_unique<std::mutex>()};

    template<typename ... ARGS_T>
    std::pair<WireNode<WIRETYPE_T>&, nodeindex_t<WIRETYPE_T>> create_node(
            ARGS_T&& ... args)
    {
        uint32_t index = m_nodes.size();
        WireNode<WIRETYPE_T> &node = m_nodes.emplace_back(std::forward<ARGS_T>(args) ...);
        return {node, nodeindex_t<WIRETYPE_T>(index)};
    }

    WireNode<WIRETYPE_T>& get_node(nodeindex_t<WIRETYPE_T> nodeIndex) noexcept
    {
        return m_nodes[size_t(nodeIndex)];
    }

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
    //std::unordered_map<wire_id_t, propagatefunc_t> m_updPropagate;

    // m_entToCalculate[machine id]
    std::vector<std::vector<ActiveEnt>> m_entToCalculate;
    std::vector<std::mutex> m_entToCalculateMutex;

    bool m_updateRequest{false};

    constexpr void request_update()
    {
        if (!m_updateRequest)
        {
            m_updateRequest = true;
        }
    }

    //std::vector<uint8_t> m_entToCalculate;
    //bool m_requestCalculate{false};
    //entt::basic_sparse_set<ActiveEnt> m_updateNext;
    //entt::basic_sparse_set<ActiveEnt> m_updateNow;
};

class SysWire
{
public:

    template<typename WIRETYPE_T>
    static bool connect(
            WireNode<WIRETYPE_T>& node,
            nodeindex_t<WIRETYPE_T> nodeIndex,
            ACompWirePanel<WIRETYPE_T>& panel,
            ActiveEnt machEnt,
            portindex_t<WIRETYPE_T> port,
            typename WIRETYPE_T::LinkState link);

    static void add_functions(ActiveScene& rScene);
    static void setup_default(
            ActiveScene& rScene,
            uint32_t machineTypeCount,
            std::vector<ACompWire::updfunc_t> updCalculate,
            std::vector<ACompWire::updfunc_t> updPropagate);

    static void update_wire(ActiveScene& rScene);

    template<typename WIRETYPE_T>
    static void construct_panels(
            ActiveScene& rScene, ActiveEnt vehicleEnt,
            ACompVehicle const& vehicle,
            BlueprintVehicle const& vehicleBp);

    template<typename WIRETYPE_T>
    static void signal_assign(
            ActiveScene& rScene,
            WIRETYPE_T&& newValue,
            WireNode<WIRETYPE_T>& rNode,
            nodeindex_t<WIRETYPE_T> nodeIndex,
            portindex_t<WIRETYPE_T> port,
            std::vector< nodeindex_t<WIRETYPE_T> >& needPropagate);

    template<typename WIRETYPE_T>
    static void signal_construct_nodes(
            ActiveScene& rScene, ActiveEnt vehicleEnt,
            ACompVehicle const& vehicle,
            BlueprintVehicle const& vehicleBp);

    template<typename WIRETYPE_T>
    static void signal_update_construct(ActiveScene& rScene);

    template<typename WIRETYPE_T>
    static void signal_update_propagate(ActiveScene& rScene);
};

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

template<typename WIRETYPE_T>
void SysWire::signal_assign(
        ActiveScene& rScene,
        WIRETYPE_T&& newValue,
        WireNode<WIRETYPE_T>& rNode,
        nodeindex_t<WIRETYPE_T> nodeIndex,
        portindex_t<WIRETYPE_T> port,
        std::vector< nodeindex_t<WIRETYPE_T> >& needPropagate)
{
    rNode.m_state.m_write = std::forward<WIRETYPE_T>(newValue);

    if (std::memcmp(&rNode.m_state.m_value,
                    &rNode.m_state.m_write, sizeof(WIRETYPE_T)) != 0)
    {
        needPropagate.push_back(nodeIndex);
    }
}

template<typename WIRETYPE_T>
void SysWire::signal_construct_nodes(
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
            connect<WIRETYPE_T>(node, nodeIndex, panel, machEnt,
                                portindex_t<WIRETYPE_T>(bpLink.m_port),
                                typename WIRETYPE_T::LinkState{});
        }
    }
}

template<typename WIRETYPE_T>
void SysWire::signal_update_construct(ActiveScene& rScene)
{
    auto view = rScene.get_registry()
            .view<osp::active::ACompVehicle,
                  osp::active::ACompVehicleInConstruction>();

    wire_id_t const id = wiretype_id<WIRETYPE_T>();

    for (auto [vehEnt, rVeh, rVehConstr] : view.each())
    {
        construct_panels<WIRETYPE_T>(
                    rScene, vehEnt, rVeh, *(rVehConstr.m_blueprint));

        signal_construct_nodes<WIRETYPE_T>(
                    rScene, vehEnt, rVeh, *(rVehConstr.m_blueprint));
    }
}

template<typename WIRETYPE_T>
void SysWire::signal_update_propagate(ActiveScene& rScene)
{
    auto &rNodes = rScene.reg_get< ACompWireNodes<WIRETYPE_T> >(rScene.hier_get_root());
    auto &rWire = rScene.reg_get<ACompWire>(rScene.hier_get_root());

    std::vector<std::vector<ActiveEnt>> machToUpdate(rWire.m_entToCalculate.size());

    std::vector< nodeindex_t<WIRETYPE_T> >& rNeedPropagate = rNodes.m_needPropagate;

    for (nodeindex_t<WIRETYPE_T> nodeIndex : rNeedPropagate)
    {
        WireNode<WIRETYPE_T> &rNode = rNodes.get_node(nodeIndex);


        rNode.m_state.m_value = rNode.m_state.m_write;

        for (auto it = std::next(std::begin(rNode.m_links));
             it != std::end(rNode.m_links); it++)
        {
            ActiveEnt ent = it->m_entity;
            auto const& type = rScene.reg_get<ACompMachineType>(ent);
            machToUpdate[size_t(type.m_type)].push_back(ent);
        }
    }

    rNeedPropagate.clear();

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
