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
#include <osp/logging.h>

#include <planet-a/Satellites/SatPlanet.h>

using namespace testapp;

using osp::universe::Satellite;
using osp::universe::Universe;

using osp::universe::SatActiveArea;
using osp::universe::SatVehicle;

using osp::universe::CoordinateSpace;
using osp::universe::CoordspaceCartesianSimple;

using osp::universe::UCompActiveArea;

using planeta::universe::SatPlanet;
using planeta::universe::UCompPlanet;

using osp::universe::Vector3g;
using osp::universe::spaceint_t;

void moon::create(osp::Resources& rResources, osp::PkgId pkg,
                  UniverseScene& rUniScn, universe_update_t& rUpdater)
{
    Universe &rUni = rUniScn.m_universe;
    Satellite root = rUni.sat_create();

    // Create a coordinate space used to position Satellites
    auto const& [coordIndex, rSpace] = rUni.coordspace_create(root);

    // Use CoordspaceSimple as data, which can store positions and velocities
    // of satellites
    rSpace.m_data.emplace<CoordspaceCartesianSimple>();
    auto *pData = entt::any_cast<CoordspaceCartesianSimple>(&rSpace.m_data);

#if 0
    // Create 4 part vehicles
    for (int i = 0; i < 4; i ++)
    {
        Satellite sat = testapp::debug_add_part_vehicle(rUniScn, rPkg, "Placeholder Mk. I " + std::to_string(i));

        Vector3g pos{i * 1024l * 4l, 0l, 0l};

        rSpace.add(sat, pos, {});
    }
#endif

    // Add Moon
    {
        Satellite sat = rUni.sat_create();

        // Create the real world moon
        float radius = 1.737E+6;
        float mass = 7.347673E+22;

        float activateRadius = radius * 256.0f;
        float resolutionScreenMax = 0.056f;
        float resolutionSurfaceMax = 12.0f;

        // assign sat as a planet
        add_planet(rUniScn, sat, radius, mass, activateRadius,
                   resolutionSurfaceMax, resolutionScreenMax);

        // Surface will appear 200 meters below the origin
        // 1024 units = 1 meter
        Vector3g pos{0, 1024l * (spaceint_t(radius) + 200), 0 * 1024l};

        rSpace.add(sat, pos, {});
    }

    // Add universe update function
    rUpdater = generate_simple_universe_update(coordIndex);

    // Update CoordinateSpace to finish adding the new Satellites
    rUpdater(rUniScn);

    OSP_LOG_INFO("Created large moon");
}
