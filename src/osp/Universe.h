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

//#include "Satellite.h"
#include "types.h"

#include <Corrade/Containers/StridedArrayView.h>

#include <entt/entity/registry.hpp>
#include <entt/core/family.hpp>

#include <cstdint>
#include <optional>
#include <tuple>
#include <vector>


namespace osp::universe
{


enum class Satellite : entt::id_type {};

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

//struct CoordinateView
//{
//    Corrade::Containers::StridedArrayView1D<void> m_data;

    //const void* m_data;
    //uint16_t m_size;
    //uint16_t m_stride;
//};

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

/**
 * A model of deep space. This class stores the data of astronomical objects
 * represented in the universe, known as Satellites. Planets, stars, comets,
 * vehicles, etc... are all Satellites.
 *
 * This class uses EnTT ECS, where Satellites are ECS entities. Components are
 * structs prefixed with UComp.
 *
 * Satellites can have types that determine which components they have, see
 * ITypeSatellite. These types are registered at runtime.
 *
 * Positions are stored in 64-bit unsigned int vectors in UCompTransformTraj.
 * 1024 units = 1 meter
 *
 * Moving or any kind of iteration over Satellites, such as Orbits, are handled
 * by Trajectory classes, see ISystemTrajectory.
 *
 * Example of usage:
 * https://github.com/TheOpenSpaceProgram/osp-magnum/wiki/Cpp:-How-to-setup-a-Solar-System
 *
 */
class Universe
{

public:

    using Reg_t = entt::basic_registry<Satellite>;

    Universe();
    Universe(Universe const &copy) = delete;
    Universe(Universe &&move) = delete;
    ~Universe() = default;

    /**
     * @return an null satellite
     */
    static constexpr Satellite sat_null() { return entt::null; };

    /**
     * Create a Satellite with default components, and adds it to the universe.
     * This Satellite will not be assigned a type.
     * @return The new Satellite just created
     */
    Satellite sat_create();

    /**
     * @return Root Satellite of the universe. This will never change.
     */
    constexpr Satellite sat_root() { return m_root; };

    /**
     * Remove a satellite
     */
    void sat_remove(Satellite sat);

    /**
     * Calculate position between two satellites.
     * @param referenceFrame [in]
     * @param target [in]
     * @return relative position of target in SpaceInt vector
     */
    Vector3s sat_calc_pos(Satellite referenceFrame, Satellite target) const;

    /**
     * Calculate position between two satellites.
     * @param referenceFrame [in]
     * @param target [in]
     * @return relative position of target in meters
     */
    Vector3 sat_calc_pos_meters(Satellite referenceFrame, Satellite target) const;

    constexpr Reg_t& get_reg() noexcept { return m_registry; }
    constexpr const Reg_t& get_reg() const noexcept
    { return m_registry; }

    std::vector< std::optional<CoordinateSpace> > m_coordSpaces;

private:

    Satellite m_root;

    Reg_t m_registry;

};


// default ECS components needed for the universe

struct UCompTransformTraj
{
    // move this somewhere else eventually
    std::string m_name;

    Quaternion m_rotation;
};

struct UCompInCoordspace
{
    uint32_t m_coordSpace;
    uint32_t m_myIndex;
};

}
