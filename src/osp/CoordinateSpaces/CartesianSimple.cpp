/**
 * Open Space Program
 * Copyright © 2019-2021 Open Space Program Project
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

#include "CartesianSimple.h"

#include <Corrade/Containers/ArrayViewStl.h>

using namespace osp::universe;

void CoordspaceCartesianSimple::update_exchange(
        Universe& rUni, CoordinateSpace& rSpace,
        CoordspaceCartesianSimple& rData)
{
    rData.reserve(rData.size() + rSpace.m_toAdd.size() - rSpace.m_toRemove.size());

    auto coordIndexView = rUni.get_reg().view<UCompCoordspaceIndex>();

    // Sort in descending order. If any of these are the last element, then
    // then they may get invalidated in the following swaps.
    std::sort(std::begin(rSpace.m_toRemove), std::end(rSpace.m_toRemove),
              std::greater<>());

    // Remove using swap-and-pop
    for (uint32_t index : rSpace.m_toRemove)
    {
        Satellite back = rData.m_satellites.back();

        // Swap last element
        if (rData.m_satellites[index] != back)
        {
            rData.m_satellites[index] = back;
            rData.m_positions[index] = rData.m_positions.back();
            rData.m_velocities[index] = rData.m_velocities.back();

            // Update index of moved last element
            coordIndexView.get<UCompCoordspaceIndex>(back).m_myIndex = index;
        }

        // Pop
        rData.m_satellites.pop_back();
        rData.m_positions.pop_back();
        rData.m_velocities.pop_back();
    }

    // Add new requested satellites
    for (auto const &[sat, pos, vel] : rSpace.m_toAdd)
    {
        auto &rCoordIndex = coordIndexView.get<UCompCoordspaceIndex>(sat);
        rCoordIndex.m_myIndex = rData.size();

        rData.m_satellites.push_back(sat);
        rData.m_positions.push_back(pos);
        rData.m_velocities.push_back(vel);
    }
}

void CoordspaceCartesianSimple::update_views(
        CoordinateSpace& rSpace, CoordspaceCartesianSimple& rData)
{
    size_t const satCount = rData.m_satellites.size();

    rSpace.m_components.resize(
                ccomp_min_size<CCompSat, CCompX, CCompY, CCompZ>());

    rSpace.m_components[ccomp_id<CCompSat>] = {
                rData.m_satellites, satCount,
                sizeof(Satellite)};

    rSpace.m_components[ccomp_id<CCompX>] = {
            rData.m_positions, &rData.m_positions[0].x(),
            satCount, sizeof (Vector3g)};

    rSpace.m_components[ccomp_id<CCompY>] = {
            rData.m_positions, &rData.m_positions[0].y(),
            satCount, sizeof (Vector3g)};

    rSpace.m_components[ccomp_id<CCompZ>] = {
            rData.m_positions, &rData.m_positions[0].z(),
            satCount, sizeof (Vector3g)};
}
