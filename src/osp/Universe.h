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
#include "id_registry.h"

#include <Corrade/Containers/ArrayViewStl.h>

#include <vector>   // std::vector
#include <string>   // std::string
#include <optional> // std::optional

#include <cstdint>  // std::uint32_t

namespace osp::universe
{

/**
 * @brief Manages Satellites and Coordinate Spaces
 */
class Universe
{
    template<typename T>
    using ArrayView_t = Corrade::Containers::ArrayView<T>;

public:
    Universe() = default;
    ~Universe() = default;

    /**
     * @brief Create a Satellite
     *
     * This satellite will not be associated with any coordinate spaces
     *
     * @return The new Satellite just created
     */
    Satellite sat_create();

    /**
     * @brief Remove a satellite
     */
    void sat_remove(Satellite sat);

    /**
     * @brief Calculate position between two satellites; works between
     *        coordinate spaces.
     *
     * This function is rather inefficient and only calculates for ONE
     * satellite. Avoid using this for hot code.
     *
     * @param observer       [in] Satellite used as reference frame
     * @param target         [in] Satellite to calculate position to
     *
     * @return relative position of target in spaceint_t vector
     */
    std::optional<Vector3g> sat_calc_pos(
            Satellite observer, Satellite target) const;

    /**
     * @brief Calls sat_calc_pos, and converts results to meters
     *
     * @param referenceFrame [in] Satellite used as reference frame
     * @param target         [in] Satellite to calculate position to
     * @return relative position of target in meters
     */
    std::optional<Vector3> sat_calc_pos_meters(Satellite referenceFrame, Satellite target) const;

    /**
     * @brief Create a coordinate space
     *
     * @warning Returned reference is not in stable memory, creating more
     *          coordinate spaces can cause reallocation.
     *
     * @return {Index to coordinate space, Reference to coordinate space}
     */
    std::pair<coordspace_index_t, CoordinateSpace&> coordspace_create(Satellite parentSat);

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

    // TODO: deleting coord spaces

    /**
     * @brief Calculate a CoordspaceTransform to transform coordinates from one
     *        coordinate space to another.
     *
     * This function will chain together parent->child and child->parent
     * transforms until a common ancestor is found.
     *
     * @param fromCoord [in]
     * @param toCoord   [in]
     * @return Optional CoordspaceTransform. Empty if no common ancestor.
     */
    std::optional<CoordspaceTransform> coordspace_transform(
            CoordinateSpace const &fromCoord, CoordinateSpace const &toCoord) const;

    /**
     * @brief Reassign indices in the UCompInCoordspace components of satellites
     *        in a CoordinateSpace's m_toAdd queue
     *
     * @param coordSpace [in] Index to CoordinateSpace in m_coordSpaces
     */
    void coordspace_update_sats(coordspace_index_t coordSpace);

    /**
     * @brief Update m_depth of coordinate space based on the m_depth of its
     *        parent
     *
     * @param coordSpace [in] Index to CoordinateSpace in m_coordSpaces
     */
    void coordspace_update_depth(coordspace_index_t coordSpace);

    constexpr ArrayView_t<coordspace_index_t> sat_coordspaces()
    {
        return m_satCoordspace;
    };

    constexpr ArrayView_t<uint32_t> sat_indices_in_coordspace()
    {
        return m_satIndexInCoordspace;
    };

private:

    osp::IdRegistry<Satellite> m_satIds;

    // CoordinateSpace each Satellite belongs to
    std::vector< coordspace_index_t > m_satCoordspace;

    // Index of a Satellite within its coordinate space
    std::vector< uint32_t > m_satIndexInCoordspace;

    // Coordinate spaces
    std::vector< std::optional<CoordinateSpace> > m_coordSpaces;

}; // class Universe

}
