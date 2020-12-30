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

#include <osp/Trajectories/Stationary.h>

#include <planet-a/Satellites/SatPlanet.h>

using namespace testapp;

using osp::Package;

using osp::universe::Satellite;
using osp::universe::Universe;

using osp::universe::SatActiveArea;
using osp::universe::SatVehicle;

using osp::universe::TrajStationary;

using osp::universe::UCompTransformTraj;

using planeta::universe::SatPlanet;
using planeta::universe::UCompPlanet;


void testapp::create_simple_solar_system(osp::OSPApplication& ospApp)
{
    Universe &uni = ospApp.get_universe();
    Package &pkg = ospApp.debug_find_package("lzdb");

    // Get the planet system used to create a UCompPlanet
    SatPlanet &typePlanet = static_cast<planeta::universe::SatPlanet&>(
            *uni.sat_type_find("Planet")->second);

    // Create trajectory that will make things added to the universe stationary
    auto &stationary = uni.trajectory_create<TrajStationary>(
                                        uni, uni.sat_root());

    // Create 10 random vehicles
    for (int i = 0; i < 10; i ++)
    {
        // Creates a random mess of spamcans as a vehicle
        Satellite sat = debug_add_random_vehicle(uni, pkg, "TestyMcTestFace Mk"
                                                 + std::to_string(i));

        auto &posTraj = uni.get_reg().get<UCompTransformTraj>(sat);

        posTraj.m_position = osp::Vector3s(i * 1024l * 5l, 0l, 0l);
        posTraj.m_dirty = true;

        stationary.add(sat);
    }

    Satellite sat = debug_add_deterministic_vehicle(uni, pkg, "Stomper Mk. I");
    auto& posTraj = uni.get_reg().get<UCompTransformTraj>(sat);
    posTraj.m_position = osp::Vector3s(22 * 1024l * 5l, 0l, 0l);
    posTraj.m_dirty = true;
    stationary.add(sat);


    // Add Grid of planets too

    for (int x = -0; x < 1; x ++)
    {
        for (int z = -0; z < 1; z ++)
        {
            Satellite sat = uni.sat_create();

            // assign sat as a planet
            UCompPlanet &planet = typePlanet.add_get_ucomp(sat);

            // Configure planet as a 256m radius sphere of black hole
            planet.m_radius = 256;
            planet.m_mass = 7.03E7 * 99999999; // volume of sphere * density

            planet.m_resolutionScreenMax = 0.056f;
            planet.m_resolutionSurfaceMax = 2.0f;

            auto &posTraj = uni.get_reg().get<UCompTransformTraj>(sat);

            // space planets 400m apart from each other
            // 1024 units = 1 meter
            posTraj.m_position = {x * 1024l * 400l,
                                  1024l * -300l,
                                  z * 1024l * 400l};
        }
    }

    std::cout << "Created simple solar system\n";
}
