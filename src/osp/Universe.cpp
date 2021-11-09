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

#include <iterator>

using namespace osp::universe;
using namespace osp;

Satellite Universe::sat_create()
{
    Satellite sat = m_satIds.create();
    m_satCoordspace.resize(m_satIds.capacity());
    m_satIndexInCoordspace.resize(m_satIds.capacity());

    m_satCoordspace[size_t(sat)] = id_null<coordspace_index_t>();
    m_satIndexInCoordspace[size_t(sat)] = id_null<coordspace_index_t>();

    return sat;
}

void Universe::sat_remove(Satellite sat)
{
    m_satCoordspace[size_t(sat)] = id_null<coordspace_index_t>();
    m_satIndexInCoordspace[size_t(sat)] = id_null<coordspace_index_t>();
    m_satIds.remove(sat);
}

std::optional<Vector3g> Universe::sat_calc_pos(Satellite observer, Satellite target) const
{
    auto const observerIndex = size_t(observer);
    auto const targetIndex = size_t(observer);

    CoordinateSpace const& frameCoord = coordspace_get(m_satCoordspace[observerIndex]);
    CoordinateSpace const& targetCoord = coordspace_get(m_satCoordspace[targetIndex]);

    std::optional<CoordspaceTransform> transform = coordspace_transform(
                targetCoord, frameCoord);

    if ( ! transform.has_value())
    {
        return {};
    }

    auto framePos = frameCoord.ccomp_view_tuple<CCompX, CCompY, CCompZ>();
    auto targetPos = targetCoord.ccomp_view_tuple<CCompX, CCompY, CCompZ>();

    Vector3g targetPosTf = transform.value()(make_from_ccomp<Vector3g>(
        *targetPos, m_satIndexInCoordspace[targetIndex]));


    return targetPosTf - make_from_ccomp<Vector3g>(*framePos,
                                                   m_satIndexInCoordspace[observerIndex]);
}

std::optional<Vector3> Universe::sat_calc_pos_meters(Satellite referenceFrame, Satellite target) const
{
    std::optional<Vector3g> pos = sat_calc_pos(referenceFrame, target);

    if (pos.has_value())
    {
        // 1024 units = 1 meter. TODO: will change soon
        return Vector3(pos.value()) / gc_units_per_meter;
    }

    return std::nullopt;
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
        CoordinateSpace const &fromCoord, CoordinateSpace const &toCoord) const
{

    auto get_parent = [this] (CoordinateSpace const* current) -> CoordinateSpace const*
    {
        coordspace_index_t const parent = m_satCoordspace[size_t(current->m_parentSat)];
        return &coordspace_get(parent);
    };

    auto get_pos = [this] (CoordinateSpace const* current, CoordinateSpace const* parent) -> Vector3g
    {
        uint32_t const index = m_satIndexInCoordspace[size_t(current->m_parentSat)];
        auto const posTuple = parent->ccomp_view_tuple<CCompX, CCompY, CCompZ>();
        return make_from_ccomp<Vector3g>(*posTuple, index);
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
    for (auto const &[sat, pos, vel] : m_coordSpaces[coordSpace]->m_toAdd)
    {
        m_satCoordspace[size_t(sat)] = coordSpace;
    }
}

void Universe::coordspace_update_depth(coordspace_index_t coordSpace)
{
    CoordinateSpace &rCoord = coordspace_get(coordSpace);

    coordspace_index_t parentCoord = m_satCoordspace[size_t(rCoord.m_parentSat)];

    if (entt::null == parentCoord)
    {
        rCoord.m_depth = 0;
        return;
    }

    rCoord.m_depth = coordspace_get(parentCoord).m_depth + 1;
}

