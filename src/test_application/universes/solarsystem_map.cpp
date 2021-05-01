#include "solarsystem_map.h"
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
#include "solarsystem_map.h"
#include <osp/OSPApplication.h>
#include <osp/types.h>
#include <osp/Trajectories/NBody.h>
#include <adera/SysMap.h>
#include <planet-a/Satellites/SatPlanet.h>
#include <random>

#include <Magnum/Math/Color.h>

using namespace testapp;
using osp::OSPApplication;
using osp::Vector3;
using osp::Vector3d;
using osp::universe::Universe;
using osp::universe::Satellite;
using osp::universe::UCompTransformTraj;
using osp::universe::ISystemTrajectory;
using osp::universe::TrajNBody;
using osp::universe::UCompVel;
using osp::universe::UCompAccel;
using osp::universe::UCompMass;
using osp::universe::UCompEmitsGravity;
using osp::universe::UCompInsignificantBody;
using osp::universe::UCompSignificantBody;
using adera::active::ACompMapVisible;
using adera::active::ACompMapLeavesTrail;
using planeta::universe::SatPlanet;
using namespace Magnum::Math::Literals;

struct PlanetBody
{
    double m_radius;
    double m_mass;
    double m_orbitDist;
    std::string m_name;
    Magnum::Color3 m_color;
    Vector3d m_velOset;
    float m_initAngle{0.0f};
    PlanetBody* m_parent{nullptr};
};

osp::SpaceInt meter_to_spaceint(double meters);
osp::Vector3s polar_km_to_v3s(double radiusKM, double angle);
Vector3d orbit_vel(double radiusKM, double sunMass, double ownMass, float initAngle);
void add_body(OSPApplication& ospApp, Satellite sat, PlanetBody body, ISystemTrajectory* traj);
std::vector<PlanetBody> create_solar_system_bodies();
void add_asteroids(OSPApplication& ospApp, TrajNBody& traj,
    size_t count, double meanDist = 3.0, double stdevDist = 0.25);

constexpr double g_sunMass = 1.988e30;
constexpr double g_1AU = 149.6e6;  // Units here are in megameters

void testapp::create_solar_system(OSPApplication& ospApp)
{
    Universe& rUni = ospApp.get_universe();
    auto& reg = rUni.get_reg();

    TrajNBody& nbody = rUni.trajectory_create<TrajNBody>(rUni, rUni.sat_root());

    /* ####### Sun ####### */
    PlanetBody sun;
    sun.m_mass = g_sunMass;
    sun.m_radius = 6.9634e8;
    sun.m_orbitDist = 1e-10;
    sun.m_name = "The sun";
    sun.m_color = 0xFFFFFF_rgbf;

    // Cuz for some reason the first one is a large number and I'm OCD
    //Satellite nullSat = rUni.sat_create();

    Satellite sunSat = rUni.sat_create();

    /*SatPlanet::add_planet(rUni, sunSat, sun.m_radius, sun.m_mass, )
    //UCompPlanet& sunPlanet = typePlanet.add_get_ucomp(sunSat);
    //sunPlanet.m_radius = sun.m_radius;*/

    UCompTransformTraj& sunTT = reg.get<UCompTransformTraj>(sunSat);
    sunTT.m_position = {0ll, 0ll, 0ll};
    sunTT.m_name = sun.m_name;
    sunTT.m_color = sun.m_color;
    reg.emplace<UCompMass>(sunSat, sun.m_mass);
    reg.emplace<UCompAccel>(sunSat, Vector3d{0.0});
    reg.emplace<UCompVel>(sunSat, Vector3d{0.0});
    reg.emplace<UCompEmitsGravity>(sunSat);
    reg.emplace<ACompMapVisible>(sunSat);
    reg.emplace<UCompSignificantBody>(sunSat);
    nbody.add(sunSat);

    /* ####### Planets ####### */
    std::vector<PlanetBody> planets = create_solar_system_bodies();
    for (PlanetBody const& body : planets)
    {
        Satellite sat = rUni.sat_create();
        add_body(ospApp, sat, body, &nbody);
        reg.emplace<UCompEmitsGravity>(sat);
        reg.emplace<UCompSignificantBody>(sat);
        reg.emplace<ACompMapLeavesTrail>(sat, 1999u);
    }

    /* ####### Asteroids ####### */

    add_asteroids(ospApp, nbody, 1'000);

    nbody.build_table();
}

void add_body(OSPApplication& ospApp, Satellite sat, PlanetBody body, ISystemTrajectory* traj)
{
    Universe& uni = ospApp.get_universe();
    auto& reg = uni.get_reg();

    UCompTransformTraj& satTT = reg.get<UCompTransformTraj>(sat);
    UCompMass& satM = reg.emplace<UCompMass>(sat, body.m_mass);

    Vector3d velocity{0.0};
    if (body.m_parent != nullptr)
    {
        PlanetBody& parent = *body.m_parent;
        osp::Vector3s parentPos = polar_km_to_v3s(parent.m_orbitDist, parent.m_initAngle);
        Vector3d parentVel =
            orbit_vel(parent.m_orbitDist, g_sunMass, parent.m_mass, parent.m_initAngle);

        float compositeAngle = body.m_initAngle + parent.m_initAngle;
        satTT.m_position = parentPos + polar_km_to_v3s(body.m_orbitDist, compositeAngle);

        velocity = 
            parentVel
            + orbit_vel(body.m_orbitDist, parent.m_mass, body.m_mass, compositeAngle)
            + body.m_velOset;
    }
    else
    {
        satTT.m_position = polar_km_to_v3s(body.m_orbitDist, body.m_initAngle);

        velocity = 
            orbit_vel(body.m_orbitDist, g_sunMass, body.m_mass, body.m_initAngle)
            + body.m_velOset;
    }
    reg.emplace<UCompVel>(sat, velocity);
    reg.emplace<UCompAccel>(sat, Vector3d{0.0});
    reg.emplace<ACompMapVisible>(sat);

    //satPlanet.m_radius = body.m_radius;
    satTT.m_name = body.m_name;
    satTT.m_color = body.m_color;

    traj->add(sat);
}

osp::SpaceInt meter_to_spaceint(double meters)
{
    return static_cast<osp::SpaceInt>(meters) * 1024ll;
}

osp::Vector3s polar_km_to_v3s(double radiusKM, double angle)
{
    return {
        meter_to_spaceint(1000.0 * radiusKM * cos(angle)),
        meter_to_spaceint(1000.0 * radiusKM * sin(angle)),
        0ll};
}

Vector3d orbit_vel(double radiusKM, double sunMass, double ownMass, float initAngle)
{
    constexpr double G = 6.67e-11;
    double initVel = 1024.0 * sqrt(G * (sunMass + ownMass) / (1000.0 * radiusKM));
    return {sin(-initAngle) * initVel, cos(-initAngle) * initVel, 0.0};
}

void add_asteroids(OSPApplication& ospApp, TrajNBody& traj,
    size_t count, double meanDist, double stdevDist)
{
    Universe& rUni = ospApp.get_universe();

    std::random_device rd{};
    std::mt19937 gen{rd()};
    std::normal_distribution<> d{meanDist, stdevDist};
    std::uniform_real_distribution<> u{0.0, 2.0 * 3.14159265359};
    std::normal_distribution<> v{0.0, 500.0 * 1024.0};

    for (size_t i = 0; i < count; i++)
    {
        Satellite asteroid = rUni.sat_create();
        double orbitalRadius = d(gen) * g_1AU;
        float polarAngle = u(gen);
        PlanetBody body;
        body.m_mass = 2e18;
        body.m_radius = 5.0;
        body.m_orbitDist = orbitalRadius;
        body.m_name = "asteroid";
        body.m_color = 0xCCCCCC_rgbf;
        body.m_initAngle = polarAngle;
        body.m_velOset = {v(gen), v(gen), v(gen)};
        add_body(ospApp, asteroid, body, &traj);
        //rUni.get_reg().emplace<UCompAsteroid>(asteroid);
        rUni.get_reg().emplace<UCompInsignificantBody>(asteroid);
    }
}

std::vector<PlanetBody> create_solar_system_bodies()
{
    std::vector<PlanetBody> planets;
    planets.reserve(27);

    /* ####### Mercury ####### */
    PlanetBody mercury;
    mercury.m_mass = 3.30e23;
    mercury.m_radius = 1e3;
    mercury.m_orbitDist = 58e6;
    mercury.m_name = "Mercury";
    mercury.m_color = 0xCCA91f_rgbf;
    planets.push_back(std::move(mercury));

    /* ####### Venus ####### */
    PlanetBody venus;
    venus.m_mass = 4.867e24;
    venus.m_radius = 1e3;
    venus.m_orbitDist = 108e6;
    venus.m_name = "Venus";
    venus.m_color = 0xFFDF80_rgbf;
    planets.push_back(std::move(venus));

    /* ####### Earth ####### */
    PlanetBody earth;
    earth.m_mass = 5.97e24;
    earth.m_radius = 6.371e6;
    earth.m_orbitDist = 149.6e6;
    earth.m_name = "Earth";
    earth.m_color = 0x24A36E_rgbf;
    planets.push_back(std::move(earth));

    PlanetBody* earthPtr = &planets.back();

    // Moon
    PlanetBody moon;
    moon.m_mass = 7.34e22;
    moon.m_radius = 1.737e6;
    moon.m_orbitDist = 348e3;
    moon.m_name = "Moon";
    moon.m_color = 0xDDDDDD_rgbf;
    moon.m_parent = earthPtr;
    planets.push_back(std::move(moon));

    /* ####### Mars & moons ####### */
    PlanetBody mars;
    mars.m_mass = 6.42e23;
    mars.m_radius = 1e3;
    mars.m_orbitDist = 228e6;
    mars.m_name = "Mars";
    mars.m_color = 0xBF6728_rgbf;
    planets.push_back(std::move(mars));

    PlanetBody* marsPtr = &planets.back();

    // Phobos
    PlanetBody phobos;
    phobos.m_mass = 1.08e16;
    phobos.m_radius = 11.1e3;
    phobos.m_orbitDist = 9.377e3;
    phobos.m_name = "Phobos";
    phobos.m_color = 0x8C8C8C_rgbf;
    phobos.m_parent = marsPtr;
    planets.push_back(std::move(phobos));

    // Deimos
    PlanetBody deimos;
    deimos.m_mass = 2e15;
    deimos.m_radius = 7.3e3;
    deimos.m_orbitDist = 2.346e4;
    deimos.m_name = "Deimos";
    deimos.m_color = 0x8C8C8C_rgbf;
    deimos.m_parent = marsPtr;
    planets.push_back(std::move(deimos));

    /* ####### Jupiter & moons ####### */
    PlanetBody jupiter;
    jupiter.m_mass = 1.898e27;
    jupiter.m_radius = 1e3;
    jupiter.m_orbitDist = 778e6;
    jupiter.m_name = "Jupiter";
    jupiter.m_color = 0xA68444_rgbf;
    planets.push_back(std::move(jupiter));

    PlanetBody* jupiterPtr = &planets.back();

    // Io
    PlanetBody io;
    io.m_mass = 8.932e22;
    io.m_radius = 1.82e9;
    io.m_orbitDist = 4.217e5;
    io.m_name = "Io";
    io.m_color = 0xC4B54F_rgbf;
    io.m_parent = jupiterPtr;
    planets.push_back(std::move(io));

    // Europa
    PlanetBody europa;
    europa.m_mass = 4.8e22;
    europa.m_radius = 1.56e9;
    europa.m_orbitDist = 6.71e5;
    europa.m_name = "Europa";
    europa.m_color = 0xADA895_rgbf;
    europa.m_parent = jupiterPtr;
    planets.push_back(std::move(europa));

    // Ganymede
    PlanetBody ganymede;
    ganymede.m_mass = 1.48e23;
    ganymede.m_radius = 2.63e9;
    ganymede.m_orbitDist = 1.07e6;
    ganymede.m_name = "Ganymede";
    ganymede.m_color = 0x75736C_rgbf;
    ganymede.m_parent = jupiterPtr;
    planets.push_back(std::move(ganymede));

    // Callisto
    PlanetBody callisto;
    callisto.m_mass = 1.08e23;
    callisto.m_radius = 2.41e9;
    callisto.m_orbitDist = 1.88e6;
    callisto.m_name = "Callisto";
    callisto.m_color = 0xB3A292_rgbf;
    callisto.m_parent = jupiterPtr;
    planets.push_back(std::move(callisto));

    /* ####### Saturn & moons ####### */
    PlanetBody saturn;
    saturn.m_mass = 5.68e26;
    saturn.m_radius = 1e3;
    saturn.m_orbitDist = 1400e6;
    saturn.m_name = "Saturn";
    saturn.m_color = 0xCFB78A_rgbf;
    planets.push_back(std::move(saturn));

    PlanetBody* saturnPtr = &planets.back();

    // Mimas
    PlanetBody mimas;
    mimas.m_mass = 4e19;
    mimas.m_radius = 198e3;
    mimas.m_orbitDist = 1.85e5;
    mimas.m_name = "Mimas";
    mimas.m_color = 0x9C9C9C_rgbf;
    mimas.m_parent = saturnPtr;
    planets.push_back(std::move(mimas));

    // Enceladus
    PlanetBody enceladus;
    enceladus.m_mass = 1.1e20;
    enceladus.m_radius = 252e3;
    enceladus.m_orbitDist = 2.38e5;
    enceladus.m_name = "Enceladus";
    enceladus.m_color = 0xD1C3AE_rgbf;
    enceladus.m_parent = saturnPtr;
    planets.push_back(std::move(enceladus));

    // Tethys
    PlanetBody tethys;
    tethys.m_mass = 6.2e20;
    tethys.m_radius = 500e3;
    tethys.m_orbitDist = 2.95e5;
    tethys.m_name = "Tethys";
    tethys.m_color = 0x9C9C9C_rgbf;
    tethys.m_parent = saturnPtr;
    planets.push_back(std::move(tethys));

    // Dione
    PlanetBody dione;
    dione.m_mass = 1.1e21;
    dione.m_radius = 550e3;
    dione.m_orbitDist = 3.77e5;
    dione.m_name = "Dione";
    dione.m_color = 0xB0B0B0_rgbf;
    dione.m_parent = saturnPtr;
    planets.push_back(std::move(dione));

    // Rhea
    PlanetBody rhea;
    rhea.m_mass = 2.3e21;
    rhea.m_radius = 750e3;
    rhea.m_orbitDist = 5.27e5;
    rhea.m_name = "Rhea";
    rhea.m_color = 0x919191_rgbf;
    rhea.m_parent = saturnPtr;
    planets.push_back(std::move(rhea));

    // Titan
    PlanetBody titan;
    titan.m_mass = 1.35e23;
    titan.m_radius = 2500e3;
    titan.m_orbitDist = 1.22e6;
    titan.m_name = "Titan";
    titan.m_color = 0xDBB660_rgbf;
    titan.m_parent = saturnPtr;
    planets.push_back(std::move(titan));

    // Iaptus
    PlanetBody iaptus;
    iaptus.m_mass = 1.3e21;
    iaptus.m_radius = 700e3;
    iaptus.m_orbitDist = 3.56e6;
    iaptus.m_name = "Iaptus";
    iaptus.m_color = 0xE3E3E3_rgbf;
    iaptus.m_parent = saturnPtr;
    planets.push_back(std::move(iaptus));

    /* ####### Uranus & moons ####### */
    PlanetBody uranus;
    uranus.m_mass = 8.68e25;
    uranus.m_radius = 1e3;
    uranus.m_orbitDist = 3000e6;
    uranus.m_name = "Uranus";
    uranus.m_color = 0x91C7EB_rgbf;
    planets.push_back(std::move(uranus));

    PlanetBody* uranusPtr = &planets.back();

    PlanetBody miranda;
    miranda.m_mass = 6.59e19;
    miranda.m_radius = 235e3;
    miranda.m_orbitDist = 1.29e5;
    miranda.m_name = "miranda";
    miranda.m_color = 0xC2C2C2_rgbf;
    miranda.m_parent = uranusPtr;
    planets.push_back(std::move(miranda));

    PlanetBody ariel;
    ariel.m_mass = 1.35e21;
    ariel.m_radius = 550e3;
    ariel.m_orbitDist = 1.91e5;
    ariel.m_name = "ariel";
    ariel.m_color = 0xABABAB_rgbf;
    ariel.m_parent = uranusPtr;
    planets.push_back(std::move(ariel));

    PlanetBody umbriel;
    umbriel.m_mass = 1.17e21;
    umbriel.m_radius = 550e3;
    umbriel.m_orbitDist = 2.66e5;
    umbriel.m_name = "umbriel";
    umbriel.m_color = 0x6E6E6E_rgbf;
    umbriel.m_parent = uranusPtr;
    planets.push_back(std::move(umbriel));

    PlanetBody titania;
    titania.m_mass = 3.53e21;
    titania.m_radius = 750e3;
    titania.m_orbitDist = 4.36e5;
    titania.m_name = "titania";
    titania.m_color = 0xC2BFB8_rgbf;
    titania.m_parent = uranusPtr;
    planets.push_back(std::move(titania));

    PlanetBody oberon;
    oberon.m_mass = 3.0e21;
    oberon.m_radius = 750e3;
    oberon.m_orbitDist = 5.83e5;
    oberon.m_name = "oberon";
    oberon.m_color = 0xABA8A1_rgbf;
    oberon.m_parent = uranusPtr;
    planets.push_back(std::move(oberon));

    /* ####### Neptune ####### */
    PlanetBody neptune;
    neptune.m_mass = 1.02e26;
    neptune.m_radius = 1e3;
    neptune.m_orbitDist = 4488e6;
    neptune.m_name = "Neptune";
    neptune.m_color = 0x0785D9_rgbf;
    planets.push_back(std::move(neptune));

    return planets;
}
