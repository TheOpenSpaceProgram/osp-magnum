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

#include "SatActiveArea.h"

using osp::universe::UCompActiveArea;
using osp::universe::SatActiveArea;

void SatActiveArea::scan(osp::universe::Universe& rUni, Satellite areaSat)
{
    auto &rArea = rUni.get_reg().get<UCompActiveArea>(areaSat);

    // Deal with activating satellites that have a UCompActivationRadius
    // Satellites that have an Activation Radius have a sphere around them. If
    // this sphere overlaps the ActiveArea's sphere 'm_areaRadius', then it
    // will be activated.

    auto viewActRadius = rUni.get_reg().view<UCompActivationRadius,
                                             UCompActivatable>();

    for (Satellite sat : viewActRadius)
    {
        auto satRadius = viewActRadius.get<UCompActivationRadius>(sat);

        // Check if already activated
        bool alreadyInside = rArea.m_inside.contains(sat);

        // Simple sphere-sphere-intersection check
        float distanceSquared = rUni.sat_calc_pos_meters(areaSat, sat).dot();
        float radius = rArea.m_areaRadius + satRadius.m_radius;

        if ((radius * radius) > distanceSquared)
        {
            if (alreadyInside)
            {
                continue; // Satellite already activated
            }
            // Satellite is nearby, activate it!
            rArea.m_inside.emplace(sat);
            rArea.m_enter.emplace_back(sat);
        }
        else if (alreadyInside)
        {
            // Satellite exited area
            rArea.m_inside.erase(sat);
            rArea.m_leave.emplace_back(sat);
        }
    }
}

UCompActiveArea& SatActiveArea::add_active_area(Universe &rUni, Satellite sat)
{
    return rUni.get_reg().emplace<UCompActiveArea>(sat);
}
