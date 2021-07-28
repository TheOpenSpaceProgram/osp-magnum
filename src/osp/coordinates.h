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
#pragma once

#include "universetypes.h"
#include "types.h"
#include "CommonMath.h"

#include <entt/core/family.hpp>
#include <entt/core/any.hpp>

#include <Corrade/Containers/StridedArrayView.h>

#include <optional>
#include <tuple>
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

using CoordinateView_t = Corrade::Containers::StridedArrayView1D<void>;

using coordspace_index_t = uint32_t;

/**
 * Runtime-generated sequential ID for a CComp
 */
template<typename CCOMP_T>
inline ccomp_id_t ccomp_id = ccomp_family_t::type<CCOMP_T>;

/**
 * @return Minimum array size needed for a set of CComp IDs to be valid indices
 */
template<typename ... CCOMP_T>
constexpr size_t ccomp_min_size() noexcept
{
    return size_t(std::max({ccomp_id<CCOMP_T> ...})) + 1;
}

template <typename ... CCOMP_T>
using TupleCComp_t = std::tuple<ViewCComp_t<CCOMP_T> ...>;

template<typename T, typename ... VIEW_T>
constexpr decltype(auto) make_from_ccomp(
        std::tuple<VIEW_T...> const& tuple, size_t index)
{
    return std::apply([index](VIEW_T const& ... view)
    {
        return T(view[index] ...);
    }, tuple);
}


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

    struct CmdPosition { Vector3g m_value; };
    struct CmdVelocity { Vector3 m_value; };

    using CmdValue_t = std::variant<CmdPosition, CmdVelocity>;

    struct Command { Satellite m_sat; ECmdOp m_op; CmdValue_t m_value; };

    CoordinateSpace(Satellite parentSat) : m_parentSat(parentSat) { }

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

    void remove(uint32_t index)
    {
        m_toRemove.emplace_back(index);
    }

    void exchange_done()
    {
        m_toAdd.clear();
        m_toRemove.clear();
    }

    /**
     * @brief TODO - commands to move and accelerate satellites
     *
     * @param cmd
     */
    void command(Command cmd)
    {
        m_commands.emplace_back(std::move(cmd));
    }

    /**
     * @brief Access this CoordinateSpace's components
     *
     * Use the index from a Satellite's UCompCoordspaceIndex to access this
     * view. The CComp must be valid, or else an exception is thrown
     *
     * @return StridedArrayView1D viewing coordinate space component data
     *
     * @tparam COMP_T Coordinate component to view
     */
    template<typename CCOMP_T>
    ViewCComp_t<CCOMP_T> const ccomp_view() const
    {
        return Corrade::Containers::arrayCast<typename CCOMP_T::datatype_t>(
                    m_components.at(ccomp_id<CCOMP_T>).value());
    }

    /**
     * @brief Access multiple components using a TupleCComp
     *
     * TupleCComp can be used to conveniently pass CComps as arguments
     *
     * @tparam COMP_T... Coordinate components to view
     */
    template<typename ... CCOMP_T>
    constexpr std::optional< TupleCComp_t<CCOMP_T ...> const>ccomp_view_tuple() const noexcept
    {
        // Make sure all CComp IDs are valid indices to m_components
        if (m_components.size() <= std::max({ccomp_id<CCOMP_T> ...}) )
        {
            return std::nullopt;
        }

        // Make sure all CComps are valid
        if ( ! (m_components.at(ccomp_id<CCOMP_T>).has_value() && ...))
        {
            return std::nullopt;
        }

        return TupleCComp_t<CCOMP_T ...>(
                    Corrade::Containers::arrayCast<typename CCOMP_T::datatype_t>(
                                        m_components.at(ccomp_id<CCOMP_T>).value()) ...);
    }

    // Queues for systems to communicate
    std::vector<SatToAdd_t> m_toAdd;
    std::vector<uint32_t> m_toRemove;
    std::vector<Command> m_commands;

    // Data and component views are managed by the external system
    // m_data is usually something like CoordspaceCartesianSimple
    entt::any m_data;
    std::vector<std::optional<CoordinateView_t> > m_components;

    Satellite m_parentSat;

    int16_t m_depth{0};
    int16_t m_pow2scale{10};

}; // struct CoordinateSpace


/**
 * @brief A functor used to transform coordinates between coordinate spaces
 *
 * Transforming coordinates from one space to another is translation and scale.
 *
 * Parent to Child: f(x) = (precision difference) * (x - childPos)
 * Child to Parent: f(x) = (precision difference) * x + childPos
 *
 * These two can be re-arranged into a general form:
 *
 * f(x) = x * 2^expX + c * 2^expC
 *
 */
struct CoordspaceTransform
{
    using Vec_t = Magnum::Math::Vector3<uint64_t>;

    /**
     * @brief Transform a Vector3g using this CoordspaceTransform
     *
     * @param in [in] Vector3g to transform
     *
     * @return Transformed Vector3g
     */
    Vector3g operator()(Vector3g in) const noexcept
    {
        using osp::math::mul_2pow;
        return mul_2pow<Vector3g, spaceint_t>(in, m_expX)
                + mul_2pow<Vector3g, spaceint_t>(Vector3g(m_c), m_expC);
    }

    /**
     * @brief Substitute another CoordspaceTransform into this transform,
     *        resulting in a new composite transform.
     *
     * With coordinate spaces A, B, and C, transforming from A->B(x) is a,
     * function and B->C(c) is also a function. This means A->C(x) is equal to
     * B->C(A->B)
     *
     * In general form, the algebra can be worked out like this:
     *
     * in(x) = x * 2^expX2  +  c2 * 2^expC2
     * out(x) = in(x) * 2^expX1  +  c1 * 2^expC1
     *
     * plug in(x) into out as x
     * out(x) = (x * 2^expX2  +  c2 * 2^expC2) * 2^expX1  +  c1 * 2^expC1
     * out(x) = x*2^(expX1+expX2)  +  c1 * 2^expC1  +  c2 * 2^(expC2+expX1)
     *
     * Combine c1 and c2 terms by splitting off one of the exponents, and
     * multiplying into a c value so that both c1 and c2 have the same exponent
     *
     * ie. c*2^expC -> (c*2^expD) * 2^(expC - expD)
     * out(x) = x*2^(expX1+expX2) + c3 * 2^expC3
     *
     * @param in [in] CoordspaceTransform to substitute as X
     *
     * @return A new CoordspaceTransform formed from substitution
     */
    constexpr CoordspaceTransform operator()(CoordspaceTransform const& in) const noexcept
    {
        CoordspaceTransform result;

        result.m_expX = m_expX + in.m_expX;

        int16_t expC1 = m_expC;
        int16_t expC2 = m_expX + in.m_expC;

        if (expC1 == expC2)
        {
            result.m_expC = expC1;
            result.m_c = m_c + in.m_c;
        }
        else if (expC1 > expC2)
        {
            // expC1 too high, multiply into m_c
            result.m_expC = expC2;
            result.m_c = m_c * 2L * (1 << (expC1-expC2)) + in.m_c;

        }
        else if (expC1 < expC2)
        {
            // expC2 too high, multiply into in.m_c
            result.m_expC = expC1;
            result.m_c = m_c + in.m_c * 2L * (1 << (expC2-expC1));
        }

        return result;
    }

    int16_t m_expX{0};

    Vec_t m_c{0};
    int16_t m_expC{0};

}; // struct CoordspaceTransform

namespace transform
{

constexpr CoordspaceTransform scaled(
        CoordspaceTransform x, int16_t from, int16_t to) noexcept
{
    return CoordspaceTransform{int16_t(x.m_expX + (from - to)), x.m_c, x.m_expC};
}

constexpr CoordspaceTransform child_to_parent(
        Vector3g const childPos, int16_t childPrec, int16_t parentPrec) noexcept
{
    int16_t exp = childPrec - parentPrec;
    return CoordspaceTransform{exp, CoordspaceTransform::Vec_t(childPos), 0};
}

constexpr CoordspaceTransform parent_to_child(
        Vector3g const childPos, int16_t childPrec, int16_t parentPrec) noexcept
{
    int16_t exp = parentPrec - childPrec;
    // individual vector components is workaround for non-constexpr operator-()
    return CoordspaceTransform{exp, CoordspaceTransform::Vec_t( -childPos.x(), -childPos.y(), -childPos.z()), exp};
}

}


}
