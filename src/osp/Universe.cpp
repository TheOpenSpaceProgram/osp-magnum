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
#include "Universe.h"

#include "Satellites/SatActiveArea.h"

#include <iostream>
#include <iterator>

using namespace osp::universe;
using namespace osp;

Universe::Universe()
{
    m_root = sat_create();
}

Satellite Universe::sat_create()
{
    Satellite sat = m_registry.create();
    m_registry.emplace<UCompTransformTraj>(sat);
    m_registry.emplace<UCompType>(sat);
    return sat;
}

void Universe::sat_remove(Satellite sat)
{
    m_registry.destroy(sat);
}


Vector3s Universe::sat_calc_pos(Satellite referenceFrame, Satellite target) const
{
    auto const view = m_registry.view<const UCompTransformTraj>();

    // TODO: maybe do some checks to make sure they have the components
    auto const &framePosTraj = view.get<const UCompTransformTraj>(referenceFrame);
    auto const &targetPosTraj = view.get<const UCompTransformTraj>(target);

    if (framePosTraj.m_parent == targetPosTraj.m_parent)
    {
        // Same parent, easy calculation
        return targetPosTraj.m_position - framePosTraj.m_position;
    }
    // TODO: calculation for different parents

    return {0, 0, 0};
}

Vector3 Universe::sat_calc_pos_meters(Satellite referenceFrame, Satellite target) const
{
    // 1024 units = 1 meter. this can change
    return Vector3(sat_calc_pos(referenceFrame, target)) / 1024.0f;
}


