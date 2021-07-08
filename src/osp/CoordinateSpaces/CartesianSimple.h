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

#include "../Universe.h"

namespace osp::universe
{

/**
 * @brief A simple Coordinate Space that positions Satellites using 3D cartesian
 *        spaceint_t vectors
 */
struct CoordspaceCartesianSimple
{
    /**
     * @brief Process m_toAdd and m_toRemove queues in a CoordinateSpace to Add
     *        and remove satellites from a CoordspaceCartesianSimple
     *
     * @warning This can invalidate component data due to reallocation. Make
     *          sure to always call update_views before accessing the
     *          CoordinateSpace's components.
     *
     * @param rUni   [ref]
     * @param rSpace [ref]
     * @param rData  [ref]
     */
    static void update_exchange(
            Universe& rUni, CoordinateSpace& rSpace,
            CoordspaceCartesianSimple& rData);

    /**
     * @brief Update buffer views to expose an CoordspaceCartesianSimple's
     *        data to its associated CoordinateSpace
     *
     * @param rSpace [ref]
     * @param rData  [ref]
     */
    static void update_views(
            CoordinateSpace& rSpace, CoordspaceCartesianSimple& rData);

    size_t size() const noexcept
    {
        return m_satellites.size();
    }

    void reserve(size_t n)
    {
        m_satellites.reserve(n);
        m_positions.reserve(n);
        m_velocities.reserve(n);
    }

    // Actual buffer data, these are carefully exposed in CoordinateSpace's
    // m_components vector
    std::vector<Satellite> m_satellites;
    std::vector<Vector3g> m_positions;
    std::vector<Vector3> m_velocities;
};

}
