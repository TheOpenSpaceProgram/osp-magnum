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
#include <osp/Trajectories/NBody.h>
#include <adera/SysMap.h>

#include <planet-a/Satellites/SatPlanet.h>

using namespace testapp;

using osp::Package;
using osp::Vector3d;

using osp::universe::Satellite;
using osp::universe::Universe;

using osp::universe::SatActiveArea;
using osp::universe::SatVehicle;

using osp::universe::TrajStationary;
using osp::universe::TrajNBody;

using osp::universe::UCompTransformTraj;
using osp::universe::UCompMass;
using osp::universe::UCompAccel;
using osp::universe::UCompVel;
using osp::universe::UCompEmitsGravity;
using osp::universe::UCompSignificantBody;
using osp::universe::UCompInsignificantBody;
using adera::active::ACompMapVisible;

using planeta::universe::SatPlanet;
using planeta::universe::UCompPlanet;


void testapp::create_real_moon(osp::OSPApplication& ospApp)
{
    Universe &rUni = ospApp.get_universe();
    auto& reg = rUni.get_reg();
    Package &rPkg = ospApp.debug_find_package("lzdb");

    // Get the planet system used to create a UCompPlanet

    // Create trajectory that will perform n-body orbital integration
    auto &nbody = rUni.trajectory_create<TrajNBody>(rUni, rUni.sat_root());

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

    auto &moonPosTraj = reg.get<UCompTransformTraj>(moonSat);

    // 1024 units = 1 meter
    moonPosTraj.m_position = {0, 0, 0};
    moonPosTraj.m_color = Magnum::Color3{1.0};

    // Configure moon for n-body dynamics
    reg.emplace<UCompMass>(moonSat, mass);
    reg.emplace<UCompAccel>(moonSat, Vector3d{0.0});
    reg.emplace<UCompVel>(moonSat, Vector3d{0.0});
    reg.emplace<UCompEmitsGravity>(moonSat);
    reg.emplace<UCompSignificantBody>(moonSat);
    reg.emplace<ACompMapVisible>(moonSat);
    nbody.add(moonSat);

    SPDLOG_LOGGER_INFO(ospApp.get_logger(), "Created large moon");


    // Create part vehicle
    Satellite sat = testapp::debug_add_part_vehicle(rUni, rPkg, "Placeholder Mk. I");

    auto &posTraj = rUni.get_reg().get<UCompTransformTraj>(sat);

    constexpr double altitude = 3E+6;
    posTraj.m_position = osp::Vector3s(1024l * osp::SpaceInt(1.74E+6 + altitude), 0l, 0l);
    posTraj.m_dirty = true;
    posTraj.m_color = Magnum::Color3{1.0};

    reg.emplace<UCompMass>(sat, 1e3);
    reg.emplace<UCompVel>(sat, Vector3d{0.0, 1000.0 * 1024.0, 0.0});
    reg.emplace<UCompAccel>(sat, Vector3d{0.0});
    reg.emplace<UCompInsignificantBody>(sat);
    reg.emplace<ACompMapVisible>(sat);
    nbody.add(sat);

    // TODO hack
    nbody.build_table();
}
