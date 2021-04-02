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
#include "blueprints.h"


using namespace osp;

BlueprintPart& BlueprintVehicle::add_part(
        DependRes<PrototypePart>& prototype,
        const Vector3& translation,
        const Quaternion& rotation,
        const Vector3& scale)
{
    uint32_t protoIndex = m_prototypes.size();

    // check if the Part Prototype is added already.
    for (size_t i = 0; i < protoIndex; i ++)
    {
        const DependRes<PrototypePart>& dep = m_prototypes[i];

        if (dep == prototype)
        {
            // prototype was already added to the list
            protoIndex = i;
            break;
        }
    }

    if (protoIndex == m_prototypes.size())
    {
        // Add the unlisted prototype to the end
        m_prototypes.emplace_back(prototype);
    }

    // We now know which part prototype to initialize, create it
    uint32_t blueprintIndex = m_blueprints.size();
    BlueprintPart &rPart = m_blueprints.emplace_back(
                protoIndex, translation, rotation, scale);

    // Add default machines from part prototypes

    size_t numMachines = prototype->m_protoMachines.size();

    for (PrototypeMachine const &protoMach : prototype->m_protoMachines)
    {
        machine_id_t id = protoMach.m_type;
        m_machines.resize(std::max(m_machines.size(), size_t(id + 1)));

        BlueprintMachine &rBlueprintMach = m_machines[id].emplace_back();
        rBlueprintMach.m_blueprintIndex = blueprintIndex;
        rBlueprintMach.m_config = protoMach.m_config;
    }

    return rPart;
}

void BlueprintVehicle::add_wire(
        uint32_t fromPart, uint32_t fromMachine, WireOutPort fromPort,
        uint32_t toPart, uint32_t toMachine, WireInPort toPort)
{
    m_wires.emplace_back(fromPart, fromMachine, fromPort,
                         toPart, toMachine, toPort);
}
