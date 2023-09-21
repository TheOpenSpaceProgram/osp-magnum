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

#include <osp/universe/coordinates.h>
#include <osp/universe/universe.h>

#include <osp/CommonMath.h>
#include <osp/logging.h>

#include <random>

using namespace osp;
using namespace osp::universe;

namespace testapp::scenes
{

Session setup_uni_core(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any>        topData,
        PipelineId const            updateOn)
{
    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_UNI_CORE);

    top_emplace< Universe > (topData, idUniverse);

    auto const tgUCore = out.create_pipelines<PlUniCore>(rBuilder);

    rBuilder.pipeline(tgUCore.update).parent(updateOn);//.wait_for_signal(EStgOptn::ModifyOrSignal);

    rBuilder.pipeline(tgUCore.transfer).parent(tgUCore.update);

    return out;
}


Session setup_uni_sceneframe(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any>        topData,
        Session const&              uniCore)
{
    auto const tgUCore =  uniCore.get_pipelines<PlUniCore>();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_UNI_SCENEFRAME);

    top_emplace< SceneFrame > (topData, idScnFrame);

    auto const tgUSFrm = out.create_pipelines<PlUniSceneFrame>(rBuilder);

    rBuilder.pipeline(tgUSFrm.sceneFrame).parent(tgUCore.update);

    return out;
}


Session setup_uni_testplanets(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any>        topData,
        Session const&              uniCore,
        Session const&              uniScnFrame)
{
    using CoSpaceIdVec_t = std::vector<CoSpaceId>;
    using Corrade::Containers::Array;

    OSP_DECLARE_GET_DATA_IDS(uniCore, TESTAPP_DATA_UNI_CORE);
    OSP_DECLARE_GET_DATA_IDS(uniScnFrame, TESTAPP_DATA_UNI_SCENEFRAME);

    auto const tgUCore = uniCore    .get_pipelines<PlUniCore>();
    auto const tgUSFrm = uniScnFrame.get_pipelines<PlUniSceneFrame>();

    auto &rUniverse = top_get< Universe >(topData, idUniverse);

    constexpr int           precision       = 10;
    constexpr int           planetCount     = 64;
    constexpr int           seed            = 1337;
    constexpr spaceint_t    maxDist         = math::mul_2pow<spaceint_t, int>(20000ul, precision);
    constexpr float         maxVel          = 800.0f;

    // Create coordinate spaces
    CoSpaceId const mainSpace = rUniverse.m_coordIds.create();
    std::vector<CoSpaceId> satSurfaceSpaces(planetCount);
    rUniverse.m_coordIds.create(satSurfaceSpaces.begin(), satSurfaceSpaces.end());

    rUniverse.m_coordCommon.resize(rUniverse.m_coordIds.capacity());

    CoSpaceCommon &rMainSpaceCommon = rUniverse.m_coordCommon[mainSpace];
    rMainSpaceCommon.m_satCount     = planetCount;
    rMainSpaceCommon.m_satCapacity  = planetCount;

    // Associate each planet satellite with their surface coordinate space
    for (SatId satId = 0; satId < planetCount; ++satId)
    {
        CoSpaceId const surfaceSpaceId = satSurfaceSpaces[satId];
        CoSpaceCommon &rCommon = rUniverse.m_coordCommon[surfaceSpaceId];
        rCommon.m_parent    = mainSpace;
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
    auto const [x, y, z]        = sat_views(rMainSpaceCommon.m_satPositions,  rMainSpaceCommon.m_data, planetCount);
    auto const [vx, vy, vz]     = sat_views(rMainSpaceCommon.m_satVelocities, rMainSpaceCommon.m_data, planetCount);
    auto const [qx, qy, qz, qw] = sat_views(rMainSpaceCommon.m_satRotations,  rMainSpaceCommon.m_data, planetCount);

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

    auto &rScnFrame      = top_get<SceneFrame>(topData, idScnFrame);
    rScnFrame.m_parent   = mainSpace;
    rScnFrame.m_position = math::mul_2pow<Vector3g, int>({400, 400, 400}, precision);

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_UNI_PLANETS);

    top_emplace< CoSpaceId >        (topData, idPlanetMainSpace, mainSpace);
    top_emplace< float >            (topData, tgUniDeltaTimeIn, 1.0f / 60.0f);
    top_emplace< CoSpaceIdVec_t >   (topData, idSatSurfaceSpaces, std::move(satSurfaceSpaces));

    rBuilder.task()
        .name       ("Update planets")
        .run_on     (tgUCore.update(Run))
        .sync_with  ({tgUSFrm.sceneFrame(Modify)})
        .push_to    (out.m_tasks)
        .args       ({     idUniverse,               idPlanetMainSpace,            idScnFrame,                      idSatSurfaceSpaces,           tgUniDeltaTimeIn })
        .func([] (Universe& rUniverse, CoSpaceId const planetMainSpace, SceneFrame &rScnFrame, CoSpaceIdVec_t const& rSatSurfaceSpaces, float const uniDeltaTimeIn) noexcept
    {
        CoSpaceCommon &rMainSpaceCommon = rUniverse.m_coordCommon[planetMainSpace];

        float const scale = osp::math::mul_2pow<float, int>(1.0f, -rMainSpaceCommon.m_precision);
        float const scaleDelta = uniDeltaTimeIn / scale;

        auto const [x, y, z]        = sat_views(rMainSpaceCommon.m_satPositions,  rMainSpaceCommon.m_data, rMainSpaceCommon.m_satCount);
        auto const [vx, vy, vz]     = sat_views(rMainSpaceCommon.m_satVelocities, rMainSpaceCommon.m_data, rMainSpaceCommon.m_satCount);
        auto const [qx, qy, qz, qw] = sat_views(rMainSpaceCommon.m_satRotations,  rMainSpaceCommon.m_data, rMainSpaceCommon.m_satCount);

        // Phase 1: Move satellites

        for (std::size_t i = 0; i < rMainSpaceCommon.m_satCount; ++i)
        {
            x[i] += vx[i] * scaleDelta;
            y[i] += vy[i] * scaleDelta;
            z[i] += vz[i] * scaleDelta;

            // Apply arbitrary inverse-square gravity towards origin
            Vector3d const pos       = Vector3d( Vector3g( x[i], y[i], z[i] ) ) * scale;
            float const r           = pos.length();
            float const c_gm        = 10000000000.0f;
            Vector3d const accel     = -pos * uniDeltaTimeIn * c_gm / (r * r * r);

            vx[i] += accel.x();
            vy[i] += accel.y();
            vz[i] += accel.z();

            // Rotate based on i, semi-random
            Vector3d const axis = Vector3d{std::sin(i), std::cos(i), double(i % 8 - 4)}.normalized();
            Radd const speed{(i % 16) / 16.0};

            Quaterniond const rot =   Quaterniond{{qx[i], qy[i], qz[i]}, qw[i]}
                                    * Quaterniond::rotation(speed * uniDeltaTimeIn, axis);
            qx[i] = rot.vector().x();
            qy[i] = rot.vector().y();
            qz[i] = rot.vector().z();
            qw[i] = rot.scalar();
        }

        // Phase 2: Transfers and stuff

        constexpr float captureDist = 500.0f;

        Vector3g const cameraPos{rScnFrame.m_rotation.transformVector(Vector3d(rScnFrame.m_scenePosition))};
        Vector3g const areaPos{rScnFrame.m_position + cameraPos};

        bool const notInPlanet = (rScnFrame.m_parent == planetMainSpace);

        if (notInPlanet)
        {
            // Find a planet to enter
            std::size_t nearbyPlanet = rMainSpaceCommon.m_satCount;
            for (std::size_t i = 0; i < rMainSpaceCommon.m_satCount; ++i)
            {
                Vector3 const diff = (Vector3( x[i], y[i], z[i] ) - Vector3(areaPos)) * scale;
                if (diff.length() < captureDist)
                {
                    nearbyPlanet = i;
                    break;
                }
            }

            if (nearbyPlanet < rMainSpaceCommon.m_satCount)
            {
                OSP_LOG_INFO("Captured into Satellite {} under CoordSpace {}",
                             nearbyPlanet, int(rSatSurfaceSpaces[nearbyPlanet]));

                CoSpaceId const surface         = rSatSurfaceSpaces[nearbyPlanet];
                CoSpaceCommon  &rSurfaceCommon  = rUniverse.m_coordCommon[surface];

                CoSpaceTransform const surfaceTf     = coord_get_transform(rSurfaceCommon, rSurfaceCommon, x, y, z, qx, qy, qz, qw);
                CoordTransformer const mainToSurface = coord_parent_to_child(rMainSpaceCommon, surfaceTf);

                // Transfer scene frame from Main to Surface coordinate space
                rScnFrame.m_parent   = surface;
                rScnFrame.m_position = mainToSurface.transform_position(rScnFrame.m_position);
                rScnFrame.m_rotation = mainToSurface.rotation() * rScnFrame.m_rotation;
            }
        }
        else
        {
            // Currently within planet, try to escape it

            Vector3 const diff = Vector3(areaPos) * scale;
            if (diff.length() > captureDist)
            {
                OSP_LOG_INFO("Leaving planet");

                CoSpaceId const surface       = rScnFrame.m_parent;
                CoSpaceCommon &rSurfaceCommon = rUniverse.m_coordCommon[surface];

                CoSpaceTransform const surfaceTf     = coord_get_transform(rSurfaceCommon, rSurfaceCommon, x, y, z, qx, qy, qz, qw);
                CoordTransformer const surfaceToMain = coord_child_to_parent(rMainSpaceCommon, surfaceTf);

                // Transfer scene frame from Surface to Main coordinate space
                rScnFrame.m_parent   = planetMainSpace;
                rScnFrame.m_position = surfaceToMain.transform_position(rScnFrame.m_position);
                rScnFrame.m_rotation = surfaceToMain.rotation() * rScnFrame.m_rotation;
            }
        }
    });

    return out;
}

} // namespace testapp::scenes
