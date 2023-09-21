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

#include "../core/math_types.h"
#include "../core/resourcetypes.h"

namespace osp
{


struct BlueprintMachine
{
    //NodeMap_t m_config;

    // Index to a BlueprintPart in BlueprintVehicle's m_blueprints
    uint32_t m_partIndex;
    // Index to a m_protoMachines in PrototypePart
    uint16_t m_protoMachineIndex;
};

/**
 * Specific information on a part in a vehicle:
 * * Which kind of part
 * * Enabled/disabled properties
 * * Transformation
 */
struct BlueprintPart
{

    BlueprintPart(uint32_t protoIndex, uint16_t machineCount,
                  Vector3 translation, Quaternion rotation, Vector3 scale)
    : m_protoIndex(protoIndex)
    , m_translation(translation)
    , m_rotation(rotation)
    , m_scale(scale)
    , m_machineCount(machineCount)
    { }

    uint32_t m_protoIndex; // index to BlueprintVehicle's m_partsUsed

    Vector3 m_translation;
    Quaternion m_rotation;
    Vector3 m_scale;

    uint16_t m_machineCount;
};

struct BlueprintWireLink
{
    // ie. Input/output; fuel flow priority
    //NodeMap_t m_config;

    // Index to a BlueprintPart in BlueprintVehicle's m_blueprints
    uint32_t m_partIndex;

    // Machine to link to, index to m_protoMachines in PrototypePart
    uint16_t m_protoMachineIndex;

    // Machine's port to connect to
    uint16_t m_port;
};

/**
 *
 */
struct BlueprintWireNode
{

    //NodeMap_t m_config;

    std::vector<BlueprintWireLink> m_links;
};

struct BlueprintWirePanel
{
    // Index to a BlueprintPart in BlueprintVehicle's m_blueprints
    uint32_t m_partIndex;

    // Machine to link to, index to m_protoMachines in PrototypePart
    uint16_t m_protoMachineIndex;

    // Number of ports
    uint16_t m_portCount;
};

/**
 * Specific information on a vehicle
 * * List of part blueprints
 * * Attachments
 * * Wiring
 */
struct BlueprintVehicle
{
    // Unique part Resources used
    std::vector<ResIdOwner_t> m_prototypes;

    // Arrangement of Individual Parts
    std::vector<BlueprintPart> m_blueprints;

    // Wire panels each machine has
    // panel = m_wirePanels[wiretype id][i]
    std::vector< std::vector<BlueprintWirePanel> > m_wirePanels;

    // Wires to connect
    // wire = m_wireNodes[wiretype id][i]
    std::vector< std::vector<BlueprintWireNode> > m_wireNodes;

    // All machines in the vehicle
    // machine = m_machines[machine id][i]
    std::vector< std::vector<BlueprintMachine> > m_machines;

};

}
