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

#include "../CoordinateSpaces/CartesianSimple.h"

using osp::universe::SatActiveArea;
using osp::universe::UCompActiveArea;


void SatActiveArea::move(Universe& rUni, Satellite areaSat)
{
    Universe::Reg_t &rReg = rUni.get_reg();
    auto &rArea = rReg.get<UCompActiveArea>(areaSat);
    auto &rInCoord = rReg.get<UCompInCoordspace>(areaSat);
    auto &rInCoordIndex = rReg.get<UCompCoordspaceIndex>(areaSat);

    CoordinateSpace& rDomainCoord = rUni.coordspace_get(rInCoord.m_coordSpace);
    auto *pDomainData = entt::any_cast<CoordspaceCartesianSimple>(&rDomainCoord.m_data);

    Vector3g &rPos = pDomainData->m_positions[rInCoordIndex.m_myIndex];

    std::vector<Vector3g> const toMove = std::exchange(rArea.m_requestMove, {});

    for (Vector3g const& delta: toMove)
    {
        rPos += delta;
        rArea.m_moved.push_back(delta);
    }
}

void SatActiveArea::scan_radius(Universe& rUni, Satellite areaSat)
{
    auto &rArea = rUni.get_reg().get<UCompActiveArea>(areaSat);

    // Deal with activating satellites that have a UCompActivationRadius
    // Satellites that have an Activation Radius have a sphere around them. If
    // this sphere overlaps the ActiveArea's sphere 'm_areaRadius', then it
    // will be activated.

    auto viewActRadius = rUni.get_reg().view<UCompActivationRadius,
                                             UCompActivatable>();

    for (Satellite const sat : viewActRadius)
    {
        auto const satRadius = viewActRadius.get<UCompActivationRadius>(sat);

        // Check if already activated
        bool alreadyInside = rArea.m_inside.contains(sat);

        // Simple sphere-sphere-intersection check
        float const distanceSquared
                        = rUni.sat_calc_pos_meters(areaSat, sat).value().dot();
        float const radius = rArea.m_areaRadius + satRadius.m_radius;

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
            rArea.m_inside.remove(sat);
            rArea.m_leave.emplace_back(sat);
        }
    }
}

void SatActiveArea::capture(Universe& rUni, Satellite areaSat,
                            Corrade::Containers::ArrayView<Satellite> toCapture)
{
    auto &rArea = rUni.get_reg().get<UCompActiveArea>(areaSat);
    CoordinateSpace &rCaptureCoord = rUni.coordspace_get(rArea.m_captureSpace);

    for (Satellite sat : toCapture)
    {
        auto const& satInCoord = rUni.get_reg().get<UCompInCoordspace>(sat);
        auto const& satCoordIndex = rUni.get_reg().get<UCompCoordspaceIndex>(sat);
        CoordinateSpace &rSatCoord = rUni.coordspace_get(satInCoord.m_coordSpace);

        Vector3g const pos = rUni.sat_calc_pos(areaSat, sat).value();

        Vector3 const vel{}; // TODO: velocity not yet implemented

        rSatCoord.remove(satCoordIndex.m_myIndex);
        rCaptureCoord.add(sat, pos, vel);

    }
}

void SatActiveArea::update_capture(Universe& rUni, coordspace_index_t capture)
{
    CoordinateSpace &rCoord = rUni.coordspace_get(capture);
    auto *pData = entt::any_cast<CoordspaceCartesianSimple>(&rCoord.m_data);
    auto viewCoordIndex = rUni.get_reg().view<UCompCoordspaceIndex>();

    for (CoordinateSpace::Command const& cmd : std::exchange(rCoord.m_commands, {}))
    {
        std::visit( [pData, &viewCoordIndex, cmd] (auto&& v)
        {
            using T = std::decay_t<decltype(v)>;

            auto const& inCoord = viewCoordIndex.get<UCompCoordspaceIndex>(cmd.m_sat);

            if constexpr (std::is_same_v<T, CoordinateSpace::CmdPosition>)
            {
                pData->m_positions.at(inCoord.m_myIndex) = v.m_value;
            }
            else if constexpr (std::is_same_v<T, CoordinateSpace::CmdVelocity>)
            {
                // capture space does not have velocity
            }
        }, cmd.m_value);
    }
}
