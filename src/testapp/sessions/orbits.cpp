/**
 * Open Space Program
 * Copyright © 2019-2024 Open Space Program Project
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

#include "orbits.h"
#include "common.h"

#include <adera/drawing/CameraController.h>

#include <osp/core/math_2pow.h>
#include <osp/drawing/drawing.h>
#include <osp/universe/coordinates.h>
#include <osp/universe/universe.h>
#include <osp/util/logging.h>

#include <random>

using namespace adera;
using namespace osp::draw;
using namespace osp::universe;
using namespace osp;

namespace testapp::scenes
{

osp::Session setup_orbit_planets(
    TopTaskBuilder &rBuilder,
    ArrayView<entt::any> topData,
    Session &uniCore,
    Session &uniScnFrame)
{
    using CoSpaceIdVec_t = std::vector<CoSpaceId>;
    using Corrade::Containers::Array;

    OSP_DECLARE_GET_DATA_IDS(uniCore, TESTAPP_DATA_UNI_CORE);
    OSP_DECLARE_GET_DATA_IDS(uniScnFrame, TESTAPP_DATA_UNI_SCENEFRAME);

    auto const tgUCore = uniCore.get_pipelines<PlUniCore>();

    auto &rUniverse = top_get<Universe>(topData, idUniverse);

    static constexpr int precision = 10;
    static constexpr int planetCount = 64;
    static constexpr int seed = 1337;
    static constexpr spaceint_t maxDist = math::mul_2pow<spaceint_t, int>(20000ul, precision);
    static constexpr float maxVel = 800.0f;

    // Create coordinate spaces
    CoSpaceId const mainSpace = rUniverse.m_coordIds.create();
    std::vector<CoSpaceId> satSurfaceSpaces(planetCount);
    rUniverse.m_coordIds.create(satSurfaceSpaces.begin(), satSurfaceSpaces.end());

    rUniverse.m_coordCommon.resize(rUniverse.m_coordIds.capacity());

    CoSpaceCommon &rMainSpaceCommon = rUniverse.m_coordCommon[mainSpace];
    rMainSpaceCommon.m_satCount = planetCount;
    rMainSpaceCommon.m_satCapacity = planetCount;

    // Associate each planet satellite with their surface coordinate space
    for (SatId satId = 0; satId < planetCount; ++satId)
    {
        CoSpaceId const surfaceSpaceId = satSurfaceSpaces[satId];
        CoSpaceCommon &rCommon = rUniverse.m_coordCommon[surfaceSpaceId];
        rCommon.m_parent = mainSpace;
        rCommon.m_parentSat = satId;
    }

    // Coordinate space data is a single allocation partitioned to hold positions, velocities, and
    // rotations.
    // TODO: Alignment is needed for SIMD (not yet implemented). see Corrade alignedAlloc

    std::size_t bytesUsed = 0;

    // Positions and velocities are arranged as XXXX... YYYY... ZZZZ...
    partition(bytesUsed, planetCount, rMainSpaceCommon.m_satPositions[0]);
    partition(bytesUsed, planetCount, rMainSpaceCommon.m_satPositions[1]);
    partition(bytesUsed, planetCount, rMainSpaceCommon.m_satPositions[2]);
    partition(bytesUsed, planetCount, rMainSpaceCommon.m_satVelocities[0]);
    partition(bytesUsed, planetCount, rMainSpaceCommon.m_satVelocities[1]);
    partition(bytesUsed, planetCount, rMainSpaceCommon.m_satVelocities[2]);

    // Rotations use XYZWXYZWXYZWXYZW...
    partition(bytesUsed, planetCount, rMainSpaceCommon.m_satRotations[0],
                rMainSpaceCommon.m_satRotations[1],
                rMainSpaceCommon.m_satRotations[2],
                rMainSpaceCommon.m_satRotations[3]);

    // Allocate data for all planets
    rMainSpaceCommon.m_data = Array<unsigned char>{Corrade::NoInit, bytesUsed};

    // Create easily accessible array views for each component
    auto const [x, y, z] = sat_views(rMainSpaceCommon.m_satPositions, rMainSpaceCommon.m_data, planetCount);
    auto const [vx, vy, vz] = sat_views(rMainSpaceCommon.m_satVelocities, rMainSpaceCommon.m_data, planetCount);
    auto const [qx, qy, qz, qw] = sat_views(rMainSpaceCommon.m_satRotations, rMainSpaceCommon.m_data, planetCount);

    std::mt19937 gen(seed);
    std::uniform_int_distribution<spaceint_t> posDist(-maxDist, maxDist);
    std::uniform_real_distribution<double> velDist(-maxVel, maxVel);

    for (std::size_t i = 0; i < planetCount; ++i)
    {
        // Assign each planet random positions and velocities
        x[i] = posDist(gen);
        y[i] = posDist(gen);
        z[i] = posDist(gen);
        vx[i] = velDist(gen);
        vy[i] = velDist(gen);
        vz[i] = velDist(gen);

        // No rotation
        qx[i] = 0.0;
        qy[i] = 0.0;
        qz[i] = 0.0;
        qw[i] = 1.0;
    }

    // Set initial scene frame
    // Is this needed?

    auto &rScnFrame = top_get<SceneFrame>(topData, idScnFrame);
    rScnFrame.m_parent = mainSpace;
    rScnFrame.m_position = math::mul_2pow<Vector3g, int>({400, 400, 400}, precision);

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_UNI_PLANETS);

    top_emplace<CoSpaceId>(topData, idPlanetMainSpace, mainSpace);
    top_emplace<float>(topData, tgUniDeltaTimeIn, 1.0f / 60.0f);
    top_emplace<CoSpaceIdVec_t>(topData, idSatSurfaceSpaces, std::move(satSurfaceSpaces));

    return out;
}

osp::Session setup_orbit_dynamics_kepler(
    TopTaskBuilder &rBuilder,
    ArrayView<entt::any> topData,
    Session &uniCore,
    Session &uniPlanets,
    Session &uniScnFrame)
{
    using CoSpaceIdVec_t = std::vector<CoSpaceId>;
    using Corrade::Containers::Array;

    OSP_DECLARE_GET_DATA_IDS(uniCore, TESTAPP_DATA_UNI_CORE);
    OSP_DECLARE_GET_DATA_IDS(uniPlanets, TESTAPP_DATA_UNI_PLANETS);
    OSP_DECLARE_GET_DATA_IDS(uniScnFrame, TESTAPP_DATA_UNI_SCENEFRAME);

    auto const tgUCore = uniCore.get_pipelines<PlUniCore>();
    auto const tgUSFrm = uniScnFrame.get_pipelines<PlUniSceneFrame>();

    auto &rUniverse = top_get<Universe>(topData, idUniverse);
    auto const &rPlanetMainSpace = top_get<CoSpaceId>(topData, idPlanetMainSpace);
    auto const &rSatSurfaceSpaces = top_get<CoSpaceIdVec_t>(topData, idSatSurfaceSpaces);
    Session out;

    rBuilder.task()
        .name("Advanced planet orbits")
        .run_on(tgUCore.update(Run))
        .sync_with({tgUSFrm.sceneFrame(Modify)})
        .push_to(out.m_tasks)
        .args({idUniverse, idPlanetMainSpace, idSatSurfaceSpaces, tgUniDeltaTimeIn})
        .func([](Universe &rUniverse, CoSpaceId const rPlanetMainSpace, CoSpaceIdVec_t const &rSatSurfaceSpaces, float const uniDeltaTimeIn) noexcept
    {
        auto &rMainSpaceCommon = rUniverse.m_coordCommon[rPlanetMainSpace];
        auto const planetCount = rMainSpaceCommon.m_satCount;
        auto const [x, y, z]        = sat_views(rMainSpaceCommon.m_satPositions,  rMainSpaceCommon.m_data, planetCount);
        auto const [vx, vy, vz]     = sat_views(rMainSpaceCommon.m_satVelocities, rMainSpaceCommon.m_data, planetCount);
        auto const [qx, qy, qz, qw] = sat_views(rMainSpaceCommon.m_satRotations,  rMainSpaceCommon.m_data, planetCount);

        // Create KeplerOrbit objects for each planet
        std::vector<KeplerOrbit> orbits;
        double const scale = osp::math::mul_2pow<double, int>(1.0, -rMainSpaceCommon.m_precision);
        double const invScale = osp::math::mul_2pow<double, int>(1.0, rMainSpaceCommon.m_precision);
        double const scaleDelta = uniDeltaTimeIn / scale;

        for (std::size_t i = 0; i < planetCount; ++i)
        {
            // Create KeplerOrbit from current conditions
            // This won't typically be done every timestep like this, but it will probably be done on some fraction of timesteps
            // So it's a bit easier to test this way
            Vector3d position = Vector3d(Vector3g(x[i], y[i], z[i]))*scale;    
            Vector3d velocity = Vector3d(vx[i], vy[i], vz[i]);

            orbits.push_back(KeplerOrbit::from_initial_conditions(position, velocity, 1.0e10, 0.000f));
        }

        for (std::size_t i = 0; i < planetCount; ++i)
        {
            // Advance time by a horrendously large amount to simulate worst-case timewarp conditions
            // double deltaTime = 1.0E15;
            double deltaTime = uniDeltaTimeIn;
            Vector3d velocity;
            Vector3d position;
            orbits[i].get_state_vectors_at_time(deltaTime, position, velocity);
            // Update position and velocity in universe

            Vector3g newPos = Vector3g(position*invScale);
            
            x[i] = newPos.x();
            y[i] = newPos.y();
            z[i] = newPos.z();
            vx[i] = velocity.x();
            vy[i] = velocity.y();
            vz[i] = velocity.z();
        } 
    });

    return out;
}

}
