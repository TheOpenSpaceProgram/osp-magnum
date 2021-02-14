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

#include "../types.h"

#include "Resource.h"
#include "PrototypePart.h"

namespace osp
{

using WireInPort = uint16_t;
using WireOutPort = uint16_t;

struct BlueprintMachine
{
    // TODO specific settings for a machine
    std::map<std::string, config_node_t> m_config;
};

/**
 * Specific information on a part in a vehicle:
 * * Which kind of part
 * * Enabled/disabled properties
 * * Transformation
 */
struct BlueprintPart
{
    // Configuration of individual machines
    std::vector<BlueprintMachine> m_machines;

    Vector3 m_scale;
    Vector3 m_translation;
    Quaternion m_rotation;

    unsigned m_partIndex; // index to BlueprintVehicle's m_partsUsed

    // put some sort of config here
};

/**
 * Describes a "from output -> to input" wire connection
 *
 * [  machine  out]--->[in  other machine  ]
 */
struct BlueprintWire
{
    BlueprintWire(unsigned fromPart, unsigned fromMachine, WireOutPort fromPort,
                  unsigned toPart, unsigned toMachine, WireOutPort toPort) :
        m_fromPart(fromPart),
        m_fromMachine(fromMachine),
        m_fromPort(fromPort),
        m_toPart(toPart),
        m_toMachine(toMachine),
        m_toPort(toPort)
    {}

    unsigned m_fromPart;
    unsigned m_fromMachine;
    WireOutPort m_fromPort;

    unsigned m_toPart;
    unsigned m_toMachine;
    WireInPort m_toPort;
};

/**
 * Specific information on a vehicle
 * * List of part blueprints
 * * Attachments
 * * Wiring
 */
class BlueprintVehicle
{
public:
    BlueprintVehicle() = default;
    BlueprintVehicle(BlueprintVehicle&& move) = default;

    /**
     * Emplaces a BlueprintPart. This function searches the prototype vector
     * to see if the part has been added before.
     * @param part
     * @param translation
     * @param rotation
     * @param scale
     * @return Resulting blueprint part
     */
    BlueprintPart& add_part(DependRes<PrototypePart>& part,
                  const Vector3& translation,
                  const Quaternion& rotation,
                  const Vector3& scale);

    /**
     * Emplace a BlueprintWire
     */
    void add_wire(BlueprintWire wire);

    constexpr std::vector<DependRes<PrototypePart>>& get_prototypes()
    { return m_prototypes; }

    constexpr std::vector<BlueprintPart>& get_blueprints()
    { return m_blueprints; }

    constexpr std::vector<BlueprintWire>& get_wires()
    { return m_wires; }

private:
    // Unique part Resources used
    std::vector<DependRes<PrototypePart>> m_prototypes;

    // Arrangement of Individual Parts
    std::vector<BlueprintPart> m_blueprints;

    // Wires to connect
    std::vector<BlueprintWire> m_wires;
};

}
