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

#include "universetypes.h"
#include "types.h"

#include <entt/core/family.hpp>
#include <entt/core/any.hpp>

#include <Corrade/Containers/StridedArrayView.h>

#include <optional>
#include <vector>

namespace osp::universe
{

using ccomp_family_t = entt::family<struct ccomp_type>;
using ccomp_id_t = ccomp_family_t::family_type;

using trajectory_family_t = entt::family<struct trajectory_type>;
using trajectory_id_t = trajectory_family_t::family_type;

struct CCompX { using datatype_t = spaceint_t; };
struct CCompY { using datatype_t = spaceint_t; };
struct CCompZ { using datatype_t = spaceint_t; };
struct CCompSat { using datatype_t = Satellite; };

template <typename CCOMP_T>
using ViewCComp_t = Corrade::Containers::StridedArrayView1D<typename CCOMP_T::datatype_t>;

template<typename CCOMP_T>
constexpr ccomp_id_t ccomp_id() noexcept
{
    return ccomp_family_t::type<CCOMP_T>;
}

using CoordinateView_t = std::optional<Corrade::Containers::StridedArrayView1D<void>>;

struct CoordinateSpace
{
    entt::any m_data;

    std::vector<CoordinateView_t> m_components;

    std::vector<Satellite> m_toAdd;
    std::vector<Satellite> m_toRemove;

    Satellite m_center;

    trajectory_id_t m_trajectoryType;

    int16_t m_pow2scale;

    template<typename CCOMP_T>
    ViewCComp_t<CCOMP_T> const ccomp_view() const
    {
        return Corrade::Containers::arrayCast<typename CCOMP_T::datatype_t>(
                    m_components.at(ccomp_id<CCOMP_T>()).value());
    }
};

struct CoordspaceSimple
{
    static void update_views(CoordinateSpace& rSpace, CoordspaceSimple& rData);

    void reserve(size_t n)
    {
        m_satellites.reserve(n);
        m_positions.reserve(n);
        m_velocities.reserve(n);
    }

    uint32_t add(Satellite sat, Vector3g pos, Vector3 vel)
    {
        uint32_t index = m_satellites.size();

        m_satellites.push_back(sat);
        m_positions.push_back(pos);
        m_velocities.push_back(vel);

        return index;
    }

    std::vector<Satellite> m_satellites;
    std::vector<Vector3g> m_positions;
    std::vector<Vector3> m_velocities;
};

}
