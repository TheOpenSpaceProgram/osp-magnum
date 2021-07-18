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

Satellite Universe::sat_create()
{
    Satellite sat = m_registry.create();
    m_registry.emplace<UCompTransformTraj>(sat);
    m_registry.emplace<UCompInCoordspace>(
                sat, UCompInCoordspace{entt::null});
    m_registry.emplace<UCompCoordspaceIndex>(
                sat, UCompCoordspaceIndex{entt::null});
    return sat;
}

void Universe::sat_remove(Satellite sat)
{
    m_registry.destroy(sat);
}

Vector3g Universe::sat_calc_pos(Satellite referenceFrame, Satellite target) const
{
    auto const viewInCoord = m_registry.view<const UCompInCoordspace>();
    auto const viewCoordIndex = m_registry.view<const UCompCoordspaceIndex>();

    // TODO: maybe do some checks to make sure they have the components
    auto const &frameInCoord
            = viewInCoord.get<const UCompInCoordspace>(referenceFrame);
    auto const &targetInCoord
            = viewInCoord.get<const UCompInCoordspace>(target);

    auto const &frameCoordIndex
            = viewCoordIndex.get<const UCompCoordspaceIndex>(referenceFrame);
    auto const &targetCoordIndex
            = viewCoordIndex.get<const UCompCoordspaceIndex>(target);

    if (frameInCoord.m_coordSpace == targetInCoord.m_coordSpace)
    {
        // Both Satellites are in the same coordinate system

        std::optional<CoordinateSpace> const &rSpace
                = m_coordSpaces.at(frameInCoord.m_coordSpace);

        ViewCComp_t<CCompX> const& viewX = rSpace->ccomp_view<CCompX>();
        ViewCComp_t<CCompY> const& viewY = rSpace->ccomp_view<CCompY>();
        ViewCComp_t<CCompZ> const& viewZ = rSpace->ccomp_view<CCompZ>();

        Vector3g const pos{
            viewX[targetCoordIndex.m_myIndex] - viewX[frameCoordIndex.m_myIndex],
            viewY[targetCoordIndex.m_myIndex] - viewY[frameCoordIndex.m_myIndex],
            viewZ[targetCoordIndex.m_myIndex] - viewZ[frameCoordIndex.m_myIndex],
        };

        return pos;
    }

    // TODO: calculation for positions across different coordinate spaces

    return {0, 0, 0};
}

Vector3 Universe::sat_calc_pos_meters(Satellite referenceFrame, Satellite target) const
{
    // 1024 units = 1 meter. TODO: will change soon
    return Vector3(sat_calc_pos(referenceFrame, target)) / gc_units_per_meter;
}

std::pair<coordspace_index_t, CoordinateSpace&> Universe::coordspace_create()
{
    coordspace_index_t const index = m_coordSpaces.size();
    return {index, *m_coordSpaces.emplace_back(CoordinateSpace{})};

    // TODO: find deleted spaces instead of emplacing back each time
}

void Universe::coordspace_update_sats(uint32_t coordSpace)
{
    auto view = m_registry.view<UCompInCoordspace>();

    for (auto const &[sat, pos, vel] : m_coordSpaces[coordSpace]->m_toAdd)
    {
        auto &rInCoordspace = view.get<UCompInCoordspace>(sat);
        rInCoordspace.m_coordSpace = coordSpace;
    }
}

