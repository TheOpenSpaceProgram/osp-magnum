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

#include "coordinates.h"
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

    Universe() = default;
    Universe(Universe const &copy) = delete;
    Universe(Universe &&move) = delete;
    ~Universe() = default;

    /**
     * @brief Create a Satellite with default components
     *
     * @return The new Satellite just created
     */
    Satellite sat_create();

    /**
     * @brief Remove a satellite
     */
    void sat_remove(Satellite sat);

    /**
     * @brief Calculate position between two satellites.
     *
     * @param referenceFrame [in] Satellite used as reference frame
     * @param target         [in] Satellite to calculate position to
     * @return relative position of target in spaceint_t vector
     */
    Vector3g sat_calc_pos(Satellite referenceFrame, Satellite target) const;

    /**
     * @brief Calculate position between two satellites.
     *
     * @param referenceFrame [in] Satellite used as reference frame
     * @param target         [in] Satellite to calculate position to
     * @return relative position of target in meters
     */
    Vector3 sat_calc_pos_meters(Satellite referenceFrame, Satellite target) const;

    /**
     * @brief Create a coordinate space
     *
     * @warning Returned reference is not in stable memory, creating more
     *          coordinate spaces can cause reallocation.
     *
     * @return {Index to coordinate space, Reference to coordinate space}
     */
    std::pair<coordspace_index_t, CoordinateSpace&> coordspace_create();

    CoordinateSpace& coordspace_get(coordspace_index_t coordSpace)
    {
        return m_coordSpaces.at(coordSpace).value();
    }

    CoordinateSpace const& coordspace_get(coordspace_index_t coordSpace) const
    {
        return m_coordSpaces.at(coordSpace).value();
    }

    void coordspace_clear()
    {
        m_coordSpaces.clear();
    };

    /**
     * @brief Ressign indices in the UCompInCoordspace components of satellites
     *        in a CoordinateSpace's m_toAdd queue
     *
     * @param coordSpace [in] Index to CoordinateSpace in m_coordSpaces
     */
    void coordspace_update_sats(coordspace_index_t coordSpace);

    constexpr Reg_t& get_reg() noexcept { return m_registry; }
    constexpr const Reg_t& get_reg() const noexcept
    { return m_registry; }

private:

    std::vector< std::optional<CoordinateSpace> > m_coordSpaces;
    Reg_t m_registry;

}; // class Universe


// default ECS components needed for the universe

struct UCompTransformTraj
{
    // move this somewhere else eventually
    std::string m_name;

    Quaternion m_rotation;
};

struct UCompInCoordspace
{
    coordspace_index_t m_coordSpace;
};

struct UCompCoordspaceIndex
{
    uint32_t m_myIndex;
};

}
