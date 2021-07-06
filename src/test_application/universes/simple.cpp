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

#include "simple.h"
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

using osp::universe::Vector3g;

void testapp::create_simple_solar_system(osp::OSPApplication& rOspApp)
{
    using osp::universe::CoordinateSpace;

    Universe &rUni = rOspApp.get_universe();
    Universe::Reg_t &rReg = rUni.get_reg();
    Package &rPkg = rOspApp.debug_find_package("lzdb");

    // Create a coordinate space used to position Satellites
    uint32_t const coordspaceIndex = rUni.m_coordSpaces.size();
    std::optional<CoordinateSpace> &rSpace = rUni.m_coordSpaces.emplace_back(CoordinateSpace{});
    rSpace->m_center = rUni.sat_root();

    // Use CoordspaceSimple as data, which can store positions and velocities
    // of satellites
    rSpace->m_data.emplace<CoordspaceCartesianSimple>();
    auto *pData = entt::any_cast<CoordspaceCartesianSimple>(&rSpace->m_data);


    // Create 2 random vehicles
    for (int i = 0; i < 2; i ++)
    {
        // Creates a random mess of spamcans as a vehicle
        Satellite sat = debug_add_random_vehicle(rUni, rPkg, "TestyMcTestFace Mk"
                                                 + std::to_string(i));

        // Add it to the CoordspaceSimple directly
        rSpace->add(sat, {i * 1024l * 5l, 0l, 0l}, {});
    }

    {
        Satellite sat = testapp::debug_add_part_vehicle(rUni, rPkg, "Placeholder Mk. I");

        rSpace->add(sat, {22 * 1024l * 5l, 0l, 0l}, {});
    }

    // Add Grid of planets too

    for (int x = -0; x < 1; x ++)
    {
        for (int z = -1; z < 1; z ++)
        {
            Satellite sat = rUni.sat_create();

            // Configure planet as a 256m radius sphere of black hole
            float radius = 256;
            float mass = 7.03E7 * 99999999; // volume of sphere * density

            float resolutionScreenMax = 0.056f;
            float resolutionSurfaceMax = 2.0f;

            // assign sat as a planet
            SatPlanet::add_planet(rUni, sat, radius, mass,
                                  resolutionSurfaceMax, resolutionScreenMax);

            // space planets 400m apart from each other
            // 1024 units = 1 meter
            osp::universe::Vector3g pos{
                x * 1024l * 400l,
                1024l * -300l,
                z * 1024l * 6000l};

            rSpace->add(sat, pos, {0.0f, 0.0f, 0.0f});
        }
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

    SPDLOG_LOGGER_INFO(rOspApp.get_logger(), "Created simple solar system");
}
