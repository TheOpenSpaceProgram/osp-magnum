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
            acomp_view_t<ACompMachines> const viewMachines,
            mcomp_view_t< MCompWirePanel<WIRETYPE_T> > viewPanels,
            ACtxWireNodes<WIRETYPE_T>& rNodes, ActiveEnt vehicleEnt,
            ACompVehicle const& vehicle, BlueprintVehicle const& vehicleBp);

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

template<typename WIRETYPE_T>
void SysSignal<WIRETYPE_T>::signal_construct_nodes(
        acomp_view_t<ACompMachines> const viewMachines,
        mcomp_view_t< MCompWirePanel<WIRETYPE_T> > viewPanels,
        ACtxWireNodes<WIRETYPE_T>& rNodes, ActiveEnt vehicleEnt,
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
        auto const &[node, nodeIndex] = rNodes.create_node();

        // Create Links
        for (BlueprintWireLink const &bpLink : bpNode.m_links)
        {
            // Get part entity from vehicle
            ActiveEnt partEnt = vehicle.m_parts[bpLink.m_partIndex];

            // Get machine entity from vehicle
            auto const &machines = viewMachines.get<ACompMachines>(partEnt);
            ActiveEnt machEnt = machines.m_machines[bpLink.m_protoMachineIndex];

            // Get panel to connect to
            auto& panel = viewPanels.template get< MCompWirePanel<WIRETYPE_T> >(machEnt);

            // Link them
            SysWire::connect<WIRETYPE_T>(node, nodeIndex, panel, machEnt,
                                         portindex_t<WIRETYPE_T>(bpLink.m_port),
                                         typename WIRETYPE_T::LinkState{});
        }
    }
}

#if 0
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
void SysSignal<WIRETYPE_T>::signal_update_nodes(ActiveScene& rScene)
{
    auto &rNodes = rScene.reg_get< ACtxWireNodes<WIRETYPE_T> >(
                rScene.hier_get_root());
    auto &rWire = rScene.reg_get<ACompWire>(rScene.hier_get_root());

    // Machines to update accumulated throughout updating nodes
    std::vector<std::vector<ActiveEnt>> machToUpdate(rWire.m_entToCalculate.size());

    // Loop through all nodes that need to be updated
    for (UpdNode<WIRETYPE_T> const& updNode : rNodes.m_writeRequests)
    {
        WireNode<WIRETYPE_T> &rNode = rNodes.get_node(
                nodeindex_t<WIRETYPE_T>(updNode.m_node));

        // Overwrite value with value to write
        static_cast<WIRETYPE_T&>(rNode.m_state)
                = static_cast<WIRETYPE_T const&>(updNode.m_write);

        // Add connected machine inputs to machToUpdate vectors, as they now
        // require a calculation update
        for (auto it = std::next(std::begin(rNode.m_links));
             it != std::end(rNode.m_links); std::advance(it, 1))
        {
            ActiveEnt ent = it->m_entity;
            //auto const& type = rScene.reg_get<ACompMachineType>(ent);
            //machToUpdate[size_t(type.m_type)].push_back(ent);
        }
    }

    // Clear write requests, as they've all been written
    rNodes.m_writeRequests.clear();

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
#endif

}
