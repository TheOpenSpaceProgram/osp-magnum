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

    struct Signal
    {
        //enum class LinkType : uint8_t { Input, Output };

        struct LinkState { };
    };

    /**
     * A rotation in global space
     */
    struct Attitude : public Signal
    {
        Quaternion m_global;
    };

    /**
     * A change in rotation. Separate pitch, yaw and roll
     */
    struct AttitudeControl : public Signal
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
    struct Percent : public Signal
    {
        static inline std::string smc_wire_name = "Percent";
        float m_value;
    };

    enum class DeployOp : std::uint8_t { NONE, ON, OFF, TOGGLE };

    /**
     * Used to turn things on and off or ignite stuff
     */
    struct Deploy : public Signal
    {
        //int m_stage;
        //bool m_on, m_off, m_toggle;
        DeployOp m_op;
    };

    /**
     * your typical logic gate boi
     */
    struct Logic : public Signal
    {
        bool m_value;
    };

    /**
     * Pipe, generically exposes the output entity to the input one
     */
    struct Pipe : public Signal
    {
        ActiveEnt m_source;
    };

} // namespace wiretype


//-----------------------------------------------------------------------------

template<typename WIRETYPE_T>
struct WireLink
{
    ActiveEnt m_entity;
    wire_port_t<WIRETYPE_T> m_port;
    typename WIRETYPE_T::LinkState m_linktype;
};

/**
 * Stores the wire value, and connects to machine's ACompWirePanel
 */
template<typename WIRETYPE_T>
struct WireNode
{
    std::vector< WireLink<WIRETYPE_T> > m_links;

    WIRETYPE_T m_value;
};

template<typename WIRETYPE_T>
struct WirePort
{
    wire_node_t<WIRETYPE_T> m_nodeIndex{nullvalue< wire_node_t<WIRETYPE_T> >()};
    //typename WIRETYPE_T::LinkState m_linktype;
    constexpr bool connected() noexcept
    {
        return m_nodeIndex != nullvalue< wire_node_t<WIRETYPE_T> >();
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

    WirePort<WIRETYPE_T>& get_port(wire_port_t<WIRETYPE_T> portIndex) noexcept
    {
        return m_ports[size_t(portIndex)];
    }
};

/**
 * Add to Machines when one of their inputs change
 */
struct ACompWireNeedUpdate { };

/**
 * Scene-wide storage for WireNodes
 */
template<typename WIRETYPE_T>
struct ACompWireNodes
{
    std::vector< WireNode<WIRETYPE_T> > m_nodes;

    template<typename ... ARGS_T>
    std::pair<WireNode<WIRETYPE_T>&, wire_node_t<WIRETYPE_T>> create_node(
            ARGS_T&& ... args)
    {
        uint32_t index = m_nodes.size();
        WireNode<WIRETYPE_T> &node = m_nodes.emplace_back(std::forward<ARGS_T>(args) ...);
        return {node, wire_node_t<WIRETYPE_T>(index)};
    }

    WireNode<WIRETYPE_T>& get_node(wire_node_t<WIRETYPE_T> nodeIndex) noexcept
    {
        return m_nodes[size_t(nodeIndex)];
    }
};

//-----------------------------------------------------------------------------

struct ACompWire
{
    // TODO: replace with beefier function order
    using update_func_t = void(*)(ActiveScene&);
    std::vector<update_func_t> m_updCalculate;
    std::vector<update_func_t> m_updPropagate;
};

class SysWire
{
public:

    template<typename WIRETYPE_T>
    static bool connect(
            WireNode<WIRETYPE_T>& node,
            wire_node_t<WIRETYPE_T> nodeIndex,
            ACompWirePanel<WIRETYPE_T>& panel,
            ActiveEnt machEnt,
            wire_port_t<WIRETYPE_T> port,
            typename WIRETYPE_T::LinkState link);

    template<typename WIRETYPE_T>
    static void signal_notify(ActiveScene& rScene, WireNode<WIRETYPE_T> const& node);

    static void add_functions(ActiveScene& rScene);
    static void setup_default(
            ActiveScene& rScene,
            std::vector<ACompWire::update_func_t> updCalculate,
            std::vector<ACompWire::update_func_t> updPropagate);

    static void update_wire(ActiveScene& rScene);

    template<typename WIRETYPE_T>
    static void update_construct_signal(ActiveScene& rScene);

};

template<typename WIRETYPE_T>
bool SysWire::connect(
        WireNode<WIRETYPE_T>& node,
        wire_node_t<WIRETYPE_T> nodeIndex,
        ACompWirePanel<WIRETYPE_T>& panel,
        ActiveEnt machEnt,
        wire_port_t<WIRETYPE_T> port,
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
void SysWire::signal_notify(ActiveScene& rScene, WireNode<WIRETYPE_T> const& node)
{
    // 0 is the output writing to the node, 1 and onwards are connected inputs
    for (int i = 1; i < node.m_links.size(); i ++)
    {
        rScene.get_registry().emplace_or_replace<ACompWireNeedUpdate>(node.m_links[i].m_entity);
    }
}

template<typename WIRETYPE_T>
void SysWire::update_construct_signal(ActiveScene& rScene)
{
    using LinkState = wiretype::Signal::LinkState;

    auto view = rScene.get_registry()
            .view<osp::active::ACompVehicle,
                  osp::active::ACompVehicleInConstruction>();

    wire_id_t const id = wiretype_id<WIRETYPE_T>();

    for (auto [vehEnt, rVeh, rVehConstr] : view.each())
    {
        // Check if the vehicle blueprint stores the right wire type
        if (rVehConstr.m_blueprint->m_wireNodes.size() <= id)
        {
            continue;
        }

        // Initialize all nodes in the vehicle
        for (BlueprintWireNode &bpNode : rVehConstr.m_blueprint->m_wireNodes[id])
        {
            // Create the node
            auto &nodes = rScene.reg_get< ACompWireNodes<WIRETYPE_T> >(rScene.hier_get_root());

            auto const &[node, nodeIndex] = nodes.create_node();

            // Create Links
            for (BlueprintWireLink &bpLink : bpNode.m_links)
            {
                // Get part entity from vehicle
                ActiveEnt partEnt = rVeh.m_parts[bpLink.m_blueprintIndex];

                // Get machine entity from vehicle
                auto &machines = rScene.reg_get<ACompMachines>(partEnt);
                ActiveEnt machEnt = machines.m_machines[bpLink.m_protoMachineIndex];

                // Get panel to connect to
                auto& panel = rScene.reg_get< ACompWirePanel<WIRETYPE_T> >(machEnt);

                // Link them
                connect<WIRETYPE_T>(node, nodeIndex, panel, machEnt,
                                    wire_port_t<WIRETYPE_T>(bpLink.m_port),
                                    { });
            }
        }
    }
}

} // namespace osp::active
