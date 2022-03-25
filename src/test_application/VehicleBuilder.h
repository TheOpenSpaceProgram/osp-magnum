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

#if 0

#include <osp/Resource/blueprints.h>
#include <osp/Resource/machines.h>

#include <limits>



namespace testapp
{

// Explicit integer types to prevent accidentally mixing them

enum class part_t : uint32_t {};
enum class mach_t : uint32_t {};

using osp::portindex_t;
using osp::nodeindex_t;
using osp::linkindex_t;

/**
 * Used to easily create Vehicle blueprints
 */
class VehicleBuilder
{
public:

    // Part functions

    /**
     * Utility: computes the displacement between two parts, relative to the
     * specified sub-object (e.g. an attachment node).
     *
     * @param attachTo [in] The parent part
     * @param attachToName [in] The name of the parent's object/attach point
     * @param toAttach [in] The child part
     * @param toAttachName [in] The name of the child's object/attach point
     */
    static osp::Vector3 part_offset(
        osp::PrototypePart const& attachTo, std::string_view attachToName,
        osp::PrototypePart const& toAttach, std::string_view toAttachName) noexcept;

    /**
     * Emplaces a BlueprintPart. This function searches the prototype vector
     * to see if the part has been added before.
     * @param part
     * @param translation
     * @param rotation
     * @param scale
     * @return Resulting blueprint part
     */
    part_t part_add(
            osp::DependRes<osp::PrototypePart>& protoPart,
            const osp::Vector3& translation,
            const osp::Quaternion& rotation,
            const osp::Vector3& scale);

    /**
     * @return Current number of parts
     */
    uint32_t part_count() const noexcept { return m_vehicle.m_blueprints.size(); }

    // Machine Functions


    std::pair<mach_t, osp::BlueprintMachine*> machine_find(
            osp::machine_id_t id, part_t part) noexcept;

    template<typename MACH_T>
    osp::BlueprintMachine* machine_find_ptr(part_t part) noexcept
    {
        return machine_find(osp::mach_id<MACH_T>(), part).second;
    }

    template<typename MACH_T>
    mach_t machine_find(part_t part) noexcept
    {
        return machine_find(osp::mach_id<MACH_T>(), part).first;
    }

    // Wiring functions


    nodeindex_t<void> wire_node_add(osp::wire_id_t id, osp::NodeMap_t config);

    template<typename WIRETYPE_T>
    nodeindex_t<WIRETYPE_T> wire_node_add(osp::ConfigNode_t config)
    {
        return wire_node_add(osp::wiretype_id<WIRETYPE_T>(), config);
    }

    linkindex_t<void> wire_connect(
            osp::wire_id_t id, nodeindex_t<void> node, osp::NodeMap_t config,
            part_t part, mach_t mach, portindex_t<void> port);

    template<typename WIRETYPE_T>
    linkindex_t<WIRETYPE_T> wire_connect(
            nodeindex_t<WIRETYPE_T> node, osp::NodeMap_t config,
            part_t part, mach_t mach, portindex_t<WIRETYPE_T> port)
    {
        return linkindex_t<WIRETYPE_T>(wire_connect(osp::wiretype_id<WIRETYPE_T>(), config, part, mach, port));
    }

    nodeindex_t<void> wire_connect_signal(osp::wire_id_t id,
            part_t partOut, mach_t machOut, portindex_t<void> portOut,
            part_t partIn, mach_t machIn, portindex_t<void> portIn);

    template<typename WIRETYPE_T>
    nodeindex_t<WIRETYPE_T> wire_connect_signal(
            part_t partOut, mach_t machOut, portindex_t<WIRETYPE_T> portOut,
            part_t partIn, mach_t machIn, portindex_t<WIRETYPE_T> portIn)
    {
        return nodeindex_t<WIRETYPE_T>(wire_connect_signal(osp::wiretype_id<WIRETYPE_T>(), partOut, machOut, portindex_t<void>(portOut), partIn, machIn, portindex_t<void>(portIn)));
    }

    osp::BlueprintVehicle&& export_move() { return std::move(m_vehicle); };
    osp::BlueprintVehicle export_copy() { return m_vehicle; };

private:

    // Map used to obtain a Machine's Panel
    // panelIndex = m_panelMap[{id, part, machine}]
    // panel = m_vehicle.m_wirePanels[panelIndex]
    std::map<std::tuple<osp::wire_id_t, part_t, mach_t>,
             uint32_t> m_panelIndexMap;

    // Map used to obtain Nodes from part
    // nodeIndex = m_nodeIndexMap[{id, part, machine, port}]
    std::map<std::tuple<osp::wire_id_t, part_t, mach_t, portindex_t<void>>,
             nodeindex_t<void>> m_nodeIndexMap;

    osp::BlueprintVehicle m_vehicle;

};


}

#endif
