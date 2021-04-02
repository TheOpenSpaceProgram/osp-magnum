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

#include <osp/Resource/blueprints.h>

namespace testapp
{

/**
 * Used to easily create Vehicle blueprints
 */
class VehicleBuilder
{
public:

    /**
     * Emplaces a BlueprintPart. This function searches the prototype vector
     * to see if the part has been added before.
     * @param part
     * @param translation
     * @param rotation
     * @param scale
     * @return Resulting blueprint part
     */
    osp::BlueprintPart& add_part(osp::DependRes<osp::PrototypePart>& part,
                  const osp::Vector3& translation,
                  const osp::Quaternion& rotation,
                  const osp::Vector3& scale);

    /**
     * Emplace a BlueprintWire
     * @param fromPart
     * @param fromMachine
     * @param fromPort
     * @param toPart
     * @param toMachine
     * @param toPort
     */
    void add_wire(unsigned fromPart, unsigned fromMachine, osp::WireOutPort fromPort,
                  unsigned toPart, unsigned toMachine, osp::WireInPort toPort);

    int part_count() const noexcept { return m_vehicle.m_blueprints.size(); }

    osp::BlueprintVehicle&& export_move() { return std::move(m_vehicle); };
    osp::BlueprintVehicle export_copy() { return m_vehicle; };

private:
    osp::BlueprintVehicle m_vehicle;

};


}
