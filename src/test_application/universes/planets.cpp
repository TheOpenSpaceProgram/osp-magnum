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


void testapp::create_real_moon(osp::OSPApplication& ospApp)
{
    Universe &rUni = ospApp.get_universe();
    Package &rPkg = ospApp.debug_find_package("lzdb");

    // Get the planet system used to create a UCompPlanet

    // Create trajectory that will make things added to the universe stationary
    auto &stationary = rUni.trajectory_create<TrajStationary>(
                                        rUni, rUni.sat_root());

    // Create 4 part vehicles
    for (int i = 0; i < 4; i ++)
    {
        Satellite sat = testapp::debug_add_part_vehicle(rUni, rPkg, "Placeholder Mk. I " + std::to_string(i));

        auto &posTraj = rUni.get_reg().get<UCompTransformTraj>(sat);

        posTraj.m_position = osp::Vector3s(i * 1024l * 4l, 0l, 0l);
        posTraj.m_dirty = true;

        stationary.add(sat);
    }

    /*Satellite sat = debug_add_deterministic_vehicle(rUni, rPkg, "Stomper Mk. I");
    auto& posTraj = rUni.get_reg().get<UCompTransformTraj>(sat);
    posTraj.m_position = osp::Vector3s(22 * 1024l * 5l, 0l, 0l);
    posTraj.m_dirty = true;
    stationary.add(sat);*/


    // Add Moon

    Satellite moonSat = rUni.sat_create();

    // Create the real world moon
    float radius = 1.737E+6;
    float mass = 7.347673E+22;

    float resolutionScreenMax = 0.056f;
    float resolutionSurfaceMax = 12.0f;

    // assign sat as a planet
    SatPlanet::add_planet(rUni, moonSat, radius, mass, resolutionSurfaceMax,
                          resolutionScreenMax);

    auto &moonPosTraj = rUni.get_reg().get<UCompTransformTraj>(moonSat);

    // 1024 units = 1 meter
    moonPosTraj.m_position = {0 * 1024l, 1024l * osp::SpaceInt(1.74E+6), 0 * 1024l};

    SPDLOG_LOGGER_INFO(ospApp.get_logger(), "Created large moon");
}
