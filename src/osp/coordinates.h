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
#include <variant>
#include <vector>

namespace osp::universe
{

using ccomp_family_t = entt::family<struct ccomp_type>;
using ccomp_id_t = ccomp_family_t::family_type;

// Coordinate component types (CComp). ccomp_family_t generates IDs for these
// at runtime, used as indices for CoordinateSpace::m_components
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

/**
 * @brief Stores positions, velocities, and other related data for Satellites,
 *        and exposes them through read-only strided array views.
 *
 * CoordinateSpaces must be managed by some external system. They can store any
 * kind of coordinate data, such as Cartesian XYZ or orbital parameters.
 *
 * As part of ECS, this is a way to store common components in separate buffers.
 * that can each be managed by a specific system (simultaneously too).
 * aka: a non-EnTT but still ECS component pool
 *
 */
struct CoordinateSpace
{
    using SatToAdd_t = std::tuple<Satellite, Vector3g, Vector3>;

    enum class ECmdOp : uint8_t { Add, Set };
    enum class ECmdVar : uint8_t { Position, Velocity };
    using CmdValue_t = std::variant<Vector3, Vector3g>;

    using Command_t = std::tuple<ECmdOp, ECmdVar, CmdValue_t>;

    /**
     * @brief Request to add a Satellite to this coordinate space
     *
     * Every coordinate space must be able to accept Satellites by a position
     * and a velocity as a common interface. If the CoordinateSpace uses a
     * non-Cartesian coordinate system like Kepler orbits, then it must be
     * converted.
     *
     * @param sat [in] Satellite to add
     * @param pos [in] Position in this coordinate system
     * @param vel [in] Velocity of satellite
     */
    void add(Satellite sat, Vector3g pos, Vector3 vel)
    {
        m_toAdd.emplace_back(sat, pos, vel);
    }

    /**
     * @brief TODO - commands to move and accelerate satellites
     *
     * @param cmd
     */
    void command(Command_t&& cmd)
    {
        m_commands.emplace_back(std::move(cmd));
    }

    /**
     * @brief Access this CoordinateSpace's components
     *
     * Use the index from a Satellite's UCompCoordspaceIndex to access this view
     *
     * @return A StridedArrayView1D viewing coordinate space component data
     *
     * @tparam COMP_T Coordinate component to view
     */
    template<typename CCOMP_T>
    ViewCComp_t<CCOMP_T> const ccomp_view() const
    {
        return Corrade::Containers::arrayCast<typename CCOMP_T::datatype_t>(
                    m_components.at(ccomp_id<CCOMP_T>()).value());
    }

    // Queues for systems to communicate
    std::vector<SatToAdd_t> m_toAdd;
    std::vector<Satellite> m_toRemove;
    std::vector<Command_t> m_commands;

    // Data and component views are managed by the external system
    // m_data is usually something like CoordspaceCartesianSimple
    entt::any m_data;
    std::vector<CoordinateView_t> m_components;

    Satellite m_center;
    int16_t m_pow2scale;
};



}
