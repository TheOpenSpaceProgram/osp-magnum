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

#include "planets.h"
#include "vehicles.h"

#include <osp/Satellites/SatActiveArea.h>
#include <osp/Satellites/SatVehicle.h>

#include <osp/CoordinateSpaces/CartesianSimple.h>
#include <osp/Trajectories/Stationary.h>

#include <planet-a/Satellites/SatPlanet.h>

using namespace testapp;

using osp::Package;

using osp::universe::Satellite;
using osp::universe::Universe;

using osp::universe::SatActiveArea;
using osp::universe::SatVehicle;

using osp::universe::CoordspaceCartesianSimple;

using osp::universe::UCompInCoordspace;
using osp::universe::UCompTransformTraj;
using osp::universe::UCompActiveArea;

using planeta::universe::SatPlanet;
using planeta::universe::UCompPlanet;


void testapp::create_real_moon(osp::OSPApplication& ospApp)
{
    using osp::universe::CoordinateSpace;

    Universe &rUni = ospApp.get_universe();
    Universe::Reg_t &rReg = rUni.get_reg();
    Package &rPkg = ospApp.debug_find_package("lzdb");

    // Create a coordinate space used to position Satellites
    uint32_t const coordspaceIndex = rUni.m_coordSpaces.size();
    std::optional<CoordinateSpace> &rSpace = rUni.m_coordSpaces.emplace_back(CoordinateSpace{});
    rSpace->m_center = rUni.sat_root();

    // Use CoordspaceSimple as data, which can store positions and velocities
    // of satellites
    rSpace->m_data.emplace<CoordspaceCartesianSimple>();
    auto *pData = entt::any_cast<CoordspaceCartesianSimple>(&rSpace->m_data);

    // Create 4 part vehicles
    for (int i = 0; i < 4; i ++)
    {
        Satellite sat = testapp::debug_add_part_vehicle(rUni, rPkg, "Placeholder Mk. I " + std::to_string(i));

        auto &posTraj = rReg.get<UCompTransformTraj>(sat);

        rSpace->add(sat, {i * 1024l * 4l, 0l, 0l}, {});
    }

    // Add Moon
    {
        Satellite sat = rUni.sat_create();

        // Create the real world moon
        float radius = 1.737E+6;
        float mass = 7.347673E+22;

        float resolutionScreenMax = 0.056f;
        float resolutionSurfaceMax = 12.0f;

        // assign sat as a planet
        SatPlanet::add_planet(rUni, sat, radius, mass, resolutionSurfaceMax,
                              resolutionScreenMax);

        osp::universe::Vector3g pos{
            0 * 1024l,
            osp::universe::spaceint_t(1024l * radius),
            0 * 1024l};

        // 1024 units = 1 meter
        rSpace->add(sat, pos, {});
    }

    {
        // create a Satellite with an ActiveArea
        Satellite sat = rUni.sat_create();

        // assign sat as an ActiveArea
        UCompActiveArea &area = SatActiveArea::add_active_area(rUni, sat);

        rSpace->add(sat, {0l, 0l, 0l}, {});
    }

    rUni.update_sat_coordspace(coordspaceIndex);
    CoordspaceCartesianSimple::update_exchange(rUni, rSpace.value(), *pData);
    CoordspaceCartesianSimple::update_views(rSpace.value(), *pData);

    SPDLOG_LOGGER_INFO(ospApp.get_logger(), "Created large moon");
}
