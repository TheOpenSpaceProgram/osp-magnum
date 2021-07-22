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

    CoordinateSpace const& frameCoord = coordspace_get(frameInCoord.m_coordSpace);
    CoordinateSpace const& targetCoord = coordspace_get(targetInCoord.m_coordSpace);

    if (frameCoord.m_parentSat == targetCoord.m_parentSat)
    {
        // Both Satellites are positioned relative to the same Satellite
        // TODO: deal with varrying precisions and possibly rotation

        auto frameView = frameCoord.ccomp_view_tuple<CCompX, CCompY, CCompZ>();
        auto targetView = targetCoord.ccomp_view_tuple<CCompX, CCompY, CCompZ>();

        return targetView->as<Vector3g>(targetCoordIndex.m_myIndex)
                - frameView->as<Vector3g>(frameCoordIndex.m_myIndex);
    }

    // TODO: calculation for positions across different coordinate spaces

    return {0, 0, 0};
}

Vector3 Universe::sat_calc_pos_meters(Satellite referenceFrame, Satellite target) const
{
    // 1024 units = 1 meter. TODO: will change soon
    return Vector3(sat_calc_pos(referenceFrame, target)) / gc_units_per_meter;
}

std::pair<coordspace_index_t, CoordinateSpace&> Universe::coordspace_create(Satellite parentSat)
{
    coordspace_index_t const index = m_coordSpaces.size();
    std::optional<CoordinateSpace>& rCoord = m_coordSpaces.emplace_back();
    rCoord.emplace(parentSat);
    return {index, *rCoord};

    // TODO: find deleted spaces instead of emplacing back each time
}


std::optional<CoordspaceTransform> Universe::coordspace_transform(
        CoordinateSpace const &fromCoord, CoordinateSpace const &toCoord)
{

    auto get_parent = [this] (CoordinateSpace const* current) -> CoordinateSpace*
    {
        coordspace_index_t const parent = m_registry.get<UCompInCoordspace>(current->m_parentSat).m_coordSpace;
        return &coordspace_get(parent);
    };

    auto get_pos = [this] (CoordinateSpace const* current, CoordinateSpace const* parent) -> Vector3g
    {
        uint32_t const index = m_registry.get<UCompCoordspaceIndex>(current->m_parentSat).m_myIndex;
        auto const posTuple = parent->ccomp_view_tuple<CCompX, CCompY, CCompZ>();
        return posTuple->as<Vector3g>(index);
    };

    CoordspaceTransform fromTransform;
    CoordspaceTransform toTransform;

    CoordinateSpace const *currentFrom = &fromCoord;
    CoordinateSpace const *currentTo = &toCoord;

    // Current depth to check for a common satellite
    int16_t testDepth = std::min({fromCoord.m_depth, toCoord.m_depth});

    while (true)
    {
        while (currentFrom->m_depth > testDepth)
        {
            CoordinateSpace const *parent = get_parent(currentFrom);
            // a->b(x);   b->c(x);   a->c(x) = b->c(a->b(x))
            fromTransform = transform::child_to_parent(get_pos(currentFrom, parent), currentFrom->m_pow2scale, parent->m_pow2scale)(fromTransform);
            currentFrom = parent;
        }

        while (currentTo->m_depth > testDepth)
        {
            CoordinateSpace const *parent = get_parent(currentTo);
            // b->a(x);   c->b(x);   c->a(x) = b->a(c->b(x))
            toTransform = transform::parent_to_child(get_pos(currentTo, parent), currentTo->m_pow2scale, parent->m_pow2scale)(toTransform);
            currentTo = parent;
        }

        if (currentFrom->m_parentSat == currentTo->m_parentSat)
        {
            // Common ancestor found
            return transform::scaled(toTransform(fromTransform), currentFrom->m_pow2scale, currentTo->m_pow2scale);
        }
        else
        {
            if (testDepth == 0)
            {
                return std::nullopt;
            }
            testDepth --;
        }
    }
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

void Universe::coordspace_update_depth(coordspace_index_t coordSpace)
{
    CoordinateSpace &rCoord = coordspace_get(coordSpace);

    coordspace_index_t parentCoord = m_registry.get<UCompInCoordspace>(
                rCoord.m_parentSat).m_coordSpace;

    if (entt::null == parentCoord)
    {
        rCoord.m_depth = 0;
        return;
    }

    rCoord.m_depth = coordspace_get(parentCoord).m_depth + 1;
}

