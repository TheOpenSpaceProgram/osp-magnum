/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
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

#include "scene_universe.h"

#include "identifiers.h"

#include <osp/universe/universe.h>

#include <random>

using namespace osp;
using namespace osp::universe;

namespace testapp::scenes
{


Session setup_uni_core(
        Builder_t&                  rBuilder,
        ArrayView<entt::any>        topData,
        Tags&                       rTags)
{
    Session uniCore;
    OSP_SESSION_ACQUIRE_DATA(uniCore, topData, TESTAPP_UNI_CORE);
    OSP_SESSION_ACQUIRE_TAGS(uniCore, rTags,   TESTAPP_UNI_CORE);

    top_emplace< Universe > (topData, idUniverse);

    return uniCore;
}

Session setup_uni_sceneframe(
        Builder_t&                  rBuilder,
        ArrayView<entt::any>        topData,
        Tags&                       rTags)
{
    Session uniScnFrame;
    OSP_SESSION_ACQUIRE_DATA(uniScnFrame, topData, TESTAPP_UNI_SCENEFRAME);
    OSP_SESSION_ACQUIRE_TAGS(uniScnFrame, rTags,   TESTAPP_UNI_SCENEFRAME);

    top_emplace< SceneFrame > (topData, idUniScnFrame);

    return uniScnFrame;
}


Session setup_uni_test_planets(
        Builder_t&                  rBuilder,
        ArrayView<entt::any>        topData,
        Tags&                       rTags,
        Session const&              uniCore,
        Session const&              uniScnFrame)
{
    using Corrade::Containers::Array;

    OSP_SESSION_UNPACK_TAGS(uniCore,        TESTAPP_UNI_CORE);
    OSP_SESSION_UNPACK_DATA(uniCore,        TESTAPP_UNI_CORE);
    OSP_SESSION_UNPACK_TAGS(uniScnFrame,    TESTAPP_UNI_SCENEFRAME);
    OSP_SESSION_UNPACK_DATA(uniScnFrame,    TESTAPP_UNI_SCENEFRAME);

    auto &rUniverse = top_get< Universe >(topData, idUniverse);

    Session uniTestPlanets;

    constexpr int           precision   = 10;
    constexpr int           planetCount = 64;
    constexpr int           seed        = 1337;
    constexpr spaceint_t    maxDist     = 20000ul << precision;
    constexpr float         maxVel      = 800.0f;

    constexpr std::size_t planetSize = sizeof(spaceint_t) * 3   // Position
                                     + sizeof(double) * 3       // Velocity
                                     + sizeof(double) * 4;      // Rotation

    // Create coordinate spaces
    CoSpaceId const mainSpace = rUniverse.m_coordIds.create();
    std::vector<CoSpaceId> surfaceSpaces(planetCount);
    rUniverse.m_coordIds.create(surfaceSpaces.begin(), surfaceSpaces.end());

    rUniverse.m_coordCommon.resize(rUniverse.m_coordIds.capacity());

    CoSpaceCommon &rMainSpaceCommon = rUniverse.m_coordCommon[mainSpace];
    rMainSpaceCommon.m_satCount     = planetCount;
    rMainSpaceCommon.m_satCapacity  = planetCount;

    // Associate each planet satellite with their surface coordinate space
    for (SatId satId = 0; satId < planetCount; ++satId)
    {
        CoSpaceId const surfaceSpaceId = surfaceSpaces[satId];
        CoSpaceCommon &rCommon = rUniverse.m_coordCommon[surfaceSpaceId];
        rCommon.m_parent    = mainSpace;
        rCommon.m_parentSat = satId;
    }

    // Create description for Position, Velocity, and Rotation data
    // TODO: Alignment is needed for SIMD. see Corrade alignedAlloc

    std::size_t bytesUsed = 0;

    // Positions and velocities are arranged as XXXX... YYYY... ZZZZ...
    interleave(bytesUsed, planetCount, rMainSpaceCommon.m_satPositions[0]);
    interleave(bytesUsed, planetCount, rMainSpaceCommon.m_satPositions[1]);
    interleave(bytesUsed, planetCount, rMainSpaceCommon.m_satPositions[2]);
    interleave(bytesUsed, planetCount, rMainSpaceCommon.m_satVelocities[0]);
    interleave(bytesUsed, planetCount, rMainSpaceCommon.m_satVelocities[1]);
    interleave(bytesUsed, planetCount, rMainSpaceCommon.m_satVelocities[2]);

    // Rotations use XYZWXYZWXYZWXYZW...
    interleave(bytesUsed, planetCount, rMainSpaceCommon.m_satRotations[0],
                                       rMainSpaceCommon.m_satRotations[1],
                                       rMainSpaceCommon.m_satRotations[2],
                                       rMainSpaceCommon.m_satRotations[3]);

    // Allocate data for all planets
    rMainSpaceCommon.m_data = Array<unsigned char>{Corrade::NoInit, bytesUsed};

    // Create easily accessible array views for each component
    auto const [x, y, z]        = sat_views(rMainSpaceCommon.m_satPositions,  rMainSpaceCommon.m_data, planetCount);
    auto const [vx, vy, vz]     = sat_views(rMainSpaceCommon.m_satVelocities, rMainSpaceCommon.m_data, planetCount);
    auto const [qx, qy, qz, qw] = sat_views(rMainSpaceCommon.m_satRotations,  rMainSpaceCommon.m_data, planetCount);

    std::mt19937 gen(seed);
    std::uniform_int_distribution<spaceint_t> posDist(-maxDist, maxDist);
    std::uniform_real_distribution<double> velDist(-maxVel, maxVel);

    for (std::size_t i = 0; i < planetCount; ++i)
    {
        // Assign each planet random ositions and velocities
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

    return uniTestPlanets;
}

}




