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

#include "../Universe.h"

namespace osp::universe
{

struct CoordspaceSimple
{
    static void update_views(CoordinateSpace& rSpace, CoordspaceSimple& rData);

    void reserve(size_t n)
    {
        m_satellites.reserve(n);
        m_positions.reserve(n);
        m_velocities.reserve(n);
    }

    uint32_t add(Satellite sat, Vector3s pos, Vector3 vel)
    {
        uint32_t index = m_satellites.size();

        m_satellites.push_back(sat);
        m_positions.push_back(pos);
        m_velocities.push_back(vel);

        return index;
    }

    std::vector<Satellite> m_satellites;
    std::vector<Vector3s> m_positions;
    std::vector<Vector3> m_velocities;
};

class TrajStationary
{
public:

    static void update(CoordinateSpace& rSpace);
};

}
