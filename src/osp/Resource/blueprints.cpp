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
    size_t partIndex = m_prototypes.size();

    // check if the part is added already.
    for (size_t i = 0; i < partIndex; i ++)
    {
        const DependRes<PrototypePart>& dep = m_prototypes[i];

        if (dep == prototype)
        {
            // prototype was already added to the list
            partIndex = i;
            break;
        }
    }

    if (partIndex == m_prototypes.size())
    {
        // Add the unlisted prototype to the end
        m_prototypes.emplace_back(prototype);
    }

    // now we have a part index.

    // Create and default initialize object blueprint machines
    size_t numMachines = prototype->get_machines().size();
    std::vector<BlueprintMachine> machineBPs;
    machineBPs.resize(numMachines);

    BlueprintPart blueprint
    {
        static_cast<uint32_t>(partIndex),
        translation,
        rotation,
        scale,
        std::move(machineBPs)
    };

    m_blueprints.push_back(std::move(blueprint));

    return m_blueprints.back();
}

void BlueprintVehicle::add_wire(
        uint32_t fromPart, uint32_t fromMachine, WireOutPort fromPort,
        uint32_t toPart, uint32_t toMachine, WireInPort toPort)
{
    m_wires.emplace_back(fromPart, fromMachine, fromPort,
                         toPart, toMachine, toPort);
}
