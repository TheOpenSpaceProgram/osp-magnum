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
#include "universe.h"

#include "../feature_interfaces.h"

#include <adera/drawing/CameraController.h>

#include <osp/core/math_2pow.h>
#include <osp/drawing/drawing.h>
#include <osp/universe/coordinates.h>
#include <osp/universe/universe.h>
#include <osp/util/logging.h>

#include <random>

using namespace adera;
using namespace ftr_inter::stages;
using namespace ftr_inter;
using namespace osp::draw;
using namespace osp::fw;
using namespace osp::universe;
using namespace osp;

namespace adera
{

// Universe Scenario

FeatureDef const ftrUniverseCore = feature_def("UniverseCore", [] (
        FeatureBuilder              &rFB,
        Implement<FIUniCore>        uniCore,
        entt::any                   userData)
{
    rFB.data_emplace< Universe >    (uniCore.di.universe);
    rFB.data_emplace< float >       (uniCore.di.deltaTimeIn, 1.0f / 60.0f);

    auto const updateOn = entt::any_cast<PipelineId>(userData);

    rFB.pipeline(uniCore.pl.update).parent(updateOn);
    rFB.pipeline(uniCore.pl.transfer).parent(uniCore.pl.update);

}); // setup_uni_core


FeatureDef const ftrUniverseSceneFrame = feature_def("UniverseSceneFrame", [] (
        FeatureBuilder              &rFB,
        Implement<FIUniSceneFrame>  uniScnFrame,
        DependOn<FIUniCore>         uniCore)
{
    rFB.data_emplace< SceneFrame > (uniScnFrame.di.scnFrame);
    rFB.pipeline(uniScnFrame.pl.sceneFrame).parent(uniCore.pl.update);
}); // ftrUniverseSceneFrame


FeatureDef const ftrUniverseTestPlanets = feature_def("UniverseTestPlanets", [] (
        FeatureBuilder              &rFB,
        Implement<FIUniPlanets>     uniPlanets,
        DependOn<FIUniSceneFrame>   uniScnFrame,
        DependOn<FIUniCore>         uniCore)
{
    using CoSpaceIdVec_t = std::vector<CoSpaceId>;
    using Corrade::Containers::Array;

    auto &rUniverse = rFB.data_get< Universe >(uniCore.di.universe);

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

    auto &rScnFrame      = rFB.data_get<SceneFrame>(uniScnFrame.di.scnFrame);
    rScnFrame.m_parent   = mainSpace;
    rScnFrame.m_position = math::mul_2pow<Vector3g, int>({400, 400, 400}, precision);

    rFB.data_emplace< CoSpaceId >        (uniPlanets.di.planetMainSpace, mainSpace);
    rFB.data_emplace< CoSpaceIdVec_t >   (uniPlanets.di.satSurfaceSpaces, std::move(satSurfaceSpaces));

    rFB.task()
        .name       ("Update planets")
        .run_on     (uniCore.pl.update(Run))
        .sync_with  ({uniScnFrame.pl.sceneFrame(Modify)})
        .args       ({   uniCore.di.universe,   uniPlanets.di.planetMainSpace, uniScnFrame.di.scnFrame,          uniPlanets.di.satSurfaceSpaces,     uniCore.di.deltaTimeIn })
        .func       ([] (Universe& rUniverse, CoSpaceId const planetMainSpace,   SceneFrame &rScnFrame, CoSpaceIdVec_t const& rSatSurfaceSpaces, float const uniDeltaTimeIn) noexcept
    {
        CoSpaceCommon &rMainSpaceCommon = rUniverse.m_coordCommon[planetMainSpace];

        auto const scale = osp::math::mul_2pow<double, int>(1.0, -rMainSpaceCommon.m_precision);
        double const scaleDelta = uniDeltaTimeIn / scale;

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
            double const r           = pos.length();
            double const c_gm        = 10000000000.0;
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
}); // ftrUniverseTestPlanets




struct PlanetDraw
{
    DrawEntVec_t            drawEnts;
    std::array<DrawEnt, 3>  axis;
    DrawEnt                 attractor;
    MaterialId              planetMat;
    MaterialId              axisMat;
};

FeatureDef const ftrUniverseTestPlanetsDraw = feature_def("UniverseTestPlanetsDraw", [] (
        FeatureBuilder              &rFB,
        Implement<FIUniPlanetsDraw> uniPlanetsDraw,
        DependOn<FIWindowApp>       windowApp,
        DependOn<FISceneRenderer>   scnRender,
        DependOn<FICameraControl>   camCtrl,
        DependOn<FICommonScene>     comScn,
        DependOn<FIUniCore>         uniCore,
        DependOn<FIUniSceneFrame>   uniScnFrame,
        DependOn<FIUniPlanets>      uniPlanets,
        entt::any                   userData)
{
    auto const params = entt::any_cast<PlanetDrawParams>(userData);

    rFB.data_emplace<PlanetDraw>(uniPlanetsDraw.di.planetDraw, PlanetDraw{
        .planetMat = params.planetMat,
        .axisMat   = params.axisMat,
    });

    rFB.task()
        .name       ("Position SceneFrame center to Camera Controller target")
        .run_on     ({windowApp.pl.inputs(Run)})
        .sync_with  ({camCtrl.pl.camCtrl(Ready), uniScnFrame.pl.sceneFrame(Modify)})
        .args       ({               camCtrl.di.camCtrl, uniScnFrame.di.scnFrame })
        .func       ([] (ACtxCameraController& rCamCtrl,   SceneFrame& rScnFrame) noexcept
    {
        if ( ! rCamCtrl.m_target.has_value())
        {
            return;
        }
        Vector3 &rCamPl = rCamCtrl.m_target.value();

        // check origin translation
        // ADL used for Magnum::Math::sign/floor/abs
        float const maxDist = 512.0f;
        Vector3 const translate = sign(rCamPl) * floor(abs(rCamPl) / maxDist) * maxDist;

        if ( ! translate.isZero())
        {
            rCamCtrl.m_transform.translation() -= translate;
            rCamPl -= translate;

            // a bit janky to modify universe stuff directly here, but it works lol
            Vector3 const rotated = Quaternion(rScnFrame.m_rotation).transformVector(translate);
            rScnFrame.m_position += Vector3g(math::mul_2pow<Vector3, int>(rotated, rScnFrame.m_precision));
        }

        rScnFrame.m_scenePosition = Vector3g(math::mul_2pow<Vector3, int>(rCamCtrl.m_target.value(), rScnFrame.m_precision));

    });

    rFB.task()
        .name       ("Resync test planets, create DrawEnts")
        .run_on     ({windowApp.pl.resync(Run)})
        .sync_with  ({scnRender.pl.drawEntResized(ModifyOrSignal)})
        .args       ({    scnRender.di.scnRender, uniPlanetsDraw.di.planetDraw, uniCore.di.universe,   uniPlanets.di.planetMainSpace})
        .func([]    (ACtxSceneRender& rScnRender,      PlanetDraw& rPlanetDraw, Universe& rUniverse, CoSpaceId const planetMainSpace) noexcept
    {
        CoSpaceCommon &rMainSpace = rUniverse.m_coordCommon[planetMainSpace];

        rPlanetDraw.drawEnts.resize(rMainSpace.m_satCount, lgrn::id_null<DrawEnt>());

        rScnRender.m_drawIds.create(rPlanetDraw.drawEnts   .begin(), rPlanetDraw.drawEnts   .end());
        rScnRender.m_drawIds.create(rPlanetDraw.axis       .begin(), rPlanetDraw.axis       .end());
        rPlanetDraw.attractor = rScnRender.m_drawIds.create();
    });

    rFB.task()
        .name       ("Resync test planets, add mesh and material")
        .run_on     ({windowApp.pl.resync(Run)})
        .sync_with  ({scnRender.pl.drawEntResized(Done), scnRender.pl.materialDirty(Modify_), scnRender.pl.entMeshDirty(Modify_)})
        .args       ({       comScn.di.drawing,      scnRender.di.scnRender,     comScn.di.namedMeshes, uniPlanetsDraw.di.planetDraw, uniCore.di.universe,   uniPlanets.di.planetMainSpace})
        .func       ([] (ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, NamedMeshes& rNamedMeshes,      PlanetDraw& rPlanetDraw, Universe& rUniverse, CoSpaceId const planetMainSpace) noexcept
    {
        CoSpaceCommon &rMainSpace = rUniverse.m_coordCommon[planetMainSpace];

        Material &rMatPlanet = rScnRender.m_materials[rPlanetDraw.planetMat];
        Material &rMatAxis   = rScnRender.m_materials[rPlanetDraw.axisMat];

        MeshId const sphereMeshId = rNamedMeshes.m_shapeToMesh.at(EShape::Sphere);
        MeshId const cubeMeshId   = rNamedMeshes.m_shapeToMesh.at(EShape::Box);

        for (std::size_t i = 0; i < rMainSpace.m_satCount; ++i)
        {
            DrawEnt const drawEnt = rPlanetDraw.drawEnts[i];

            rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(sphereMeshId);
            rScnRender.m_meshDirty.push_back(drawEnt);
            rScnRender.m_visible.insert(drawEnt);
            rScnRender.m_opaque.insert(drawEnt);
            rMatPlanet.m_ents.insert(drawEnt);
            rMatPlanet.m_dirty.push_back(drawEnt);
        }

        rScnRender.m_mesh[rPlanetDraw.attractor] = rDrawing.m_meshRefCounts.ref_add(sphereMeshId);
        rScnRender.m_meshDirty.push_back(rPlanetDraw.attractor);
        rScnRender.m_visible.insert(rPlanetDraw.attractor);
        rScnRender.m_opaque.insert(rPlanetDraw.attractor);
        rMatPlanet.m_ents.insert(rPlanetDraw.attractor);
        rMatPlanet.m_dirty.push_back(rPlanetDraw.attractor);

        for (DrawEnt const drawEnt : rPlanetDraw.axis)
        {
            rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(cubeMeshId);
            rScnRender.m_meshDirty.push_back(drawEnt);
            rScnRender.m_visible.insert(drawEnt);
            rScnRender.m_opaque.insert(drawEnt);
            rMatAxis.m_ents.insert(drawEnt);
            rMatAxis.m_dirty.push_back(drawEnt);
        }

        rScnRender.m_color[rPlanetDraw.axis[0]] = {1.0f, 0.0f, 0.0f, 1.0f};
        rScnRender.m_color[rPlanetDraw.axis[1]] = {0.0f, 1.0f, 0.0f, 1.0f};
        rScnRender.m_color[rPlanetDraw.axis[2]] = {0.0f, 0.0f, 1.0f, 1.0f};
    });

    rFB.task()
        .name       ("Reposition test planet DrawEnts")
        .run_on     ({scnRender.pl.render(Run)})
        .sync_with  ({scnRender.pl.drawTransforms(Modify_), scnRender.pl.drawEntResized(Done), camCtrl.pl.camCtrl(Ready), uniScnFrame.pl.sceneFrame(Modify)})
        .args       ({       comScn.di.drawing,      scnRender.di.scnRender, uniPlanetsDraw.di.planetDraw, uniCore.di.universe,     uniScnFrame.di.scnFrame,   uniPlanets.di.planetMainSpace})
        .func       ([] (ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender,      PlanetDraw& rPlanetDraw, Universe& rUniverse, SceneFrame const& rScnFrame, CoSpaceId const planetMainSpace) noexcept
    {
        CoSpaceCommon &rMainSpace = rUniverse.m_coordCommon[planetMainSpace];
        auto const [x, y, z]        = sat_views(rMainSpace.m_satPositions, rMainSpace.m_data, rMainSpace.m_satCount);
        auto const [qx, qy, qz, qw] = sat_views(rMainSpace.m_satRotations, rMainSpace.m_data, rMainSpace.m_satCount);

        // Calculate transform from universe to area/local-space for rendering.
        // This can be generalized by finding a common ancestor within the tree
        // of coordinate spaces. Since there's only two possibilities, an if
        // statement works.
        CoordTransformer mainToArea;
        if (rScnFrame.m_parent == planetMainSpace)
        {
            mainToArea = coord_parent_to_child(rMainSpace, rScnFrame);
        }
        else
        {
            CoSpaceId const landedId = rScnFrame.m_parent;
            CoSpaceCommon &rLanded = rUniverse.m_coordCommon[landedId];

            CoSpaceTransform const landedTf     = coord_get_transform(rLanded, rLanded, x, y, z, qx, qy, qz, qw);
            CoordTransformer const mainToLanded = coord_parent_to_child(rMainSpace, landedTf);
            CoordTransformer const landedToArea = coord_parent_to_child(landedTf, rScnFrame);

            mainToArea = coord_composite(landedToArea, mainToLanded);
        }
        Quaternion const mainToAreaRot{mainToArea.rotation()};

        float const scale = math::mul_2pow<float, int>(1.0f, -rMainSpace.m_precision);

        Vector3 const attractorPos = Vector3(mainToArea.transform_position({0, 0, 0})) * scale;

        // Attractor
        rScnRender.m_drawTransform[rPlanetDraw.attractor]
            = Matrix4::translation(attractorPos)
            * Matrix4{mainToAreaRot.toMatrix()}
            * Matrix4::scaling({500, 500, 500});

        rScnRender.m_drawTransform[rPlanetDraw.axis[0]]
            = Matrix4::translation(attractorPos)
            * Matrix4{mainToAreaRot.toMatrix()}
            * Matrix4::scaling({500000, 10, 10});
        rScnRender.m_drawTransform[rPlanetDraw.axis[1]]
            = Matrix4::translation(attractorPos)
            * Matrix4{mainToAreaRot.toMatrix()}
            * Matrix4::scaling({10, 500000, 10});
        rScnRender.m_drawTransform[rPlanetDraw.axis[2]]
            = Matrix4::translation(attractorPos)
            * Matrix4{mainToAreaRot.toMatrix()}
            * Matrix4::scaling({10, 10, 500000});

        for (std::size_t i = 0; i < rMainSpace.m_satCount; ++i)
        {
            Vector3g const relative = mainToArea.transform_position({x[i], y[i], z[i]});
            Vector3 const relativeMeters = Vector3(relative) * scale;

            Quaterniond const rot{{qx[i], qy[i], qz[i]}, qw[i]};

            DrawEnt const drawEnt = rPlanetDraw.drawEnts[i];

            rScnRender.m_drawTransform[drawEnt]
                = Matrix4::translation(relativeMeters)
                * Matrix4::scaling({200, 200, 200})
                * Matrix4{(mainToAreaRot * Quaternion{rot}).toMatrix()};
        }
    });
}); // setup_testplanets_draw



// Solar System Scenario

constexpr unsigned int c_planetCount = 5;

FeatureDef const ftrSolarSystem = feature_def("SolarSystem", [] (
        FeatureBuilder              &rFB,
        Implement<FISolarSys>       solarSys,
        DependOn<FIUniCore>         uniCore,
        DependOn<FIUniSceneFrame>   uniScnFrame)
{
    using CoSpaceIdVec_t = std::vector<CoSpaceId>;
    using Corrade::Containers::Array;

    auto& rUniverse = rFB.data_get< Universe >(uniCore.di.universe);

    constexpr int precision = 10;

    // Create coordinate spaces
    CoSpaceId const mainSpace = rUniverse.m_coordIds.create();
    std::vector<CoSpaceId> satSurfaceSpaces(c_planetCount);
    rUniverse.m_coordIds.create(satSurfaceSpaces.begin(), satSurfaceSpaces.end());

    rUniverse.m_coordCommon.resize(rUniverse.m_coordIds.capacity());

    CoSpaceCommon& rMainSpaceCommon = rUniverse.m_coordCommon[mainSpace];
    rMainSpaceCommon.m_satCount = c_planetCount;
    rMainSpaceCommon.m_satCapacity = c_planetCount;

    auto& rCoordNBody = rFB.data_emplace< osp::KeyedVec<CoSpaceId, CoSpaceNBody> >(solarSys.di.coordNBody);
    rCoordNBody.resize(rUniverse.m_coordIds.capacity());

    // Associate each planet satellite with their surface coordinate space
    for (SatId satId = 0; satId < c_planetCount; ++satId)
    {
        CoSpaceId const surfaceSpaceId = satSurfaceSpaces[satId];
        CoSpaceCommon& rCommon = rUniverse.m_coordCommon[surfaceSpaceId];
        rCommon.m_parent = mainSpace;
        rCommon.m_parentSat = satId;
    }

    // Coordinate space data is a single allocation partitioned to hold positions, velocities, and
    // rotations.
    // TODO: Alignment is needed for SIMD (not yet implemented). see Corrade alignedAlloc

    std::size_t bytesUsed = 0;

    // Positions and velocities are arranged as XXXX... YYYY... ZZZZ...
    partition(bytesUsed, c_planetCount, rMainSpaceCommon.m_satPositions[0]);
    partition(bytesUsed, c_planetCount, rMainSpaceCommon.m_satPositions[1]);
    partition(bytesUsed, c_planetCount, rMainSpaceCommon.m_satPositions[2]);
    partition(bytesUsed, c_planetCount, rMainSpaceCommon.m_satVelocities[0]);
    partition(bytesUsed, c_planetCount, rMainSpaceCommon.m_satVelocities[1]);
    partition(bytesUsed, c_planetCount, rMainSpaceCommon.m_satVelocities[2]);

    // Rotations use XYZWXYZWXYZWXYZW...
    partition(bytesUsed, c_planetCount, rMainSpaceCommon.m_satRotations[0],
        rMainSpaceCommon.m_satRotations[1],
        rMainSpaceCommon.m_satRotations[2],
        rMainSpaceCommon.m_satRotations[3]);

    partition(bytesUsed, c_planetCount, rCoordNBody[mainSpace].mass);
    partition(bytesUsed, c_planetCount, rCoordNBody[mainSpace].radius);
    partition(bytesUsed, c_planetCount, rCoordNBody[mainSpace].color);

    // Allocate data for all planets
    rMainSpaceCommon.m_data = Array<unsigned char>{ Corrade::NoInit, bytesUsed };

    std::size_t nextBody = 0;
    auto const add_body = [&rMainSpaceCommon, &nextBody, &rCoordNBody, &mainSpace] (
        Vector3l position,
        Vector3d velocity,
        Quaternion rotation,
        float mass,
        float radius,
        Magnum::Color3 color)
    {
        auto const [x, y, z] = sat_views(rMainSpaceCommon.m_satPositions, rMainSpaceCommon.m_data, c_planetCount);
        auto const [vx, vy, vz] = sat_views(rMainSpaceCommon.m_satVelocities, rMainSpaceCommon.m_data, c_planetCount);
        auto const [qx, qy, qz, qw] = sat_views(rMainSpaceCommon.m_satRotations, rMainSpaceCommon.m_data, c_planetCount);

        auto const massView = rCoordNBody[mainSpace].mass.view(arrayView(rMainSpaceCommon.m_data), c_planetCount);
        auto const radiusView = rCoordNBody[mainSpace].radius.view(arrayView(rMainSpaceCommon.m_data), c_planetCount);
        auto const colorView = rCoordNBody[mainSpace].color.view(arrayView(rMainSpaceCommon.m_data), c_planetCount);

        x[nextBody] = position.x();
        y[nextBody] = position.y();
        z[nextBody] = position.z();

        vx[nextBody] = velocity.x();
        vy[nextBody] = velocity.y();
        vz[nextBody] = velocity.z();

        qx[nextBody] = rotation.vector().x();
        qy[nextBody] = rotation.vector().y();
        qz[nextBody] = rotation.vector().z();
        qw[nextBody] = rotation.scalar();

        massView[nextBody] = mass;
        radiusView[nextBody] = radius;
        colorView[nextBody] = color;

        ++nextBody;
    };

    // Sun
    add_body(
        { 0, 0, 0 },
        { 0.0, 0.0, 0.0 },
        Quaternion::rotation(Rad{ 0.0f }, Vector3{ 1.0f, 0.0f, 0.0f }),
        1.0f * std::pow(10, 1),
        1000.0f,
        { 1.0f, 1.0f, 0.0f });

    // Blue Planet
    add_body(
        { 0, math::mul_2pow<spaceint_t, int>(10, precision), 0 },
        { 1.0, 0.0, 0.0 },
        Quaternion::rotation(Rad{ 0.0f }, Vector3{ 1.0f, 0.0f, 0.0f }),
        0.0000000001f,
        500.0f,
        { 0.0f, 0.0f, 1.0f });

    // Red Planet
    add_body(
        { 0, math::mul_2pow<spaceint_t, int>(5, precision), 0 },
        { 1.414213562, 0.0, 0.0 },
        Quaternion::rotation(Rad{ 0.0f }, Vector3{ 1.0f, 0.0f, 0.0f }),
        0.0000000001f,
        250.0f,
        { 1.0f, 0.0f, 0.0f });

    // Green Planet
    add_body(
        { 0, math::mul_2pow<spaceint_t, int>(7, precision), 0 },
        { 1.154700538, 0.0, 0.0 },
        Quaternion::rotation(Rad{ 0.0f }, Vector3{ 1.0f, 0.0f, 0.0f }),
        0.0000000001f,
        600.0f,
        { 0.0f, 1.0f, 0.0f });

    // Orange Planet
    add_body(
        { 0, math::mul_2pow<spaceint_t, int>(12, precision), 0 },
        { 0.912870929, 0.0, 0.0 },
        Quaternion::rotation(Rad{ 0.0f }, Vector3{ 1.0f, 0.0f, 0.0f }),
        0.0000000001f,
        550.0f,
        { 1.0f, 0.5f, 0.0f });

    rFB.data_emplace< CoSpaceId >       (solarSys.di.planetMainSpace, mainSpace);
    rFB.data_emplace< CoSpaceIdVec_t >  (solarSys.di.satSurfaceSpaces, std::move(satSurfaceSpaces));

    // Set initial scene frame

    auto& rScnFrame = rFB.data_get<SceneFrame>(uniScnFrame.di.scnFrame);
    rScnFrame.m_parent = mainSpace;
    rScnFrame.m_position = math::mul_2pow<Vector3g, int>({ 400, 400, 400 }, precision);

    rFB.task()
        .name       ("Update planets")
        .run_on     (uniCore.pl.update(Run))
        .sync_with  ({ uniScnFrame.pl.sceneFrame(Modify) })
        .args       ({  uniCore.di.universe,     solarSys.di.planetMainSpace, uniScnFrame.di.scnFrame,            solarSys.di.satSurfaceSpaces,     uniCore.di.deltaTimeIn,                              solarSys.di.coordNBody })
        .func       ([](Universe& rUniverse, CoSpaceId const planetMainSpace,   SceneFrame& rScnFrame, CoSpaceIdVec_t const& rSatSurfaceSpaces, float const uniDeltaTimeIn, osp::KeyedVec<CoSpaceId, CoSpaceNBody>& rCoordNBody) noexcept
    {
        CoSpaceCommon& rMainSpaceCommon = rUniverse.m_coordCommon[planetMainSpace];

        auto const scale = osp::math::mul_2pow<double, int>(1.0, -rMainSpaceCommon.m_precision);
        double const scaleDelta = uniDeltaTimeIn / scale;

        auto const [x, y, z] = sat_views(rMainSpaceCommon.m_satPositions, rMainSpaceCommon.m_data, rMainSpaceCommon.m_satCount);
        auto const [vx, vy, vz] = sat_views(rMainSpaceCommon.m_satVelocities, rMainSpaceCommon.m_data, rMainSpaceCommon.m_satCount);

        auto const massView = rCoordNBody[planetMainSpace].mass.view(arrayView(rMainSpaceCommon.m_data), c_planetCount);

        for (std::size_t i = 0; i < rMainSpaceCommon.m_satCount; ++i) {
            x[i] += vx[i] * scaleDelta;
            y[i] += vy[i] * scaleDelta;
            z[i] += vz[i] * scaleDelta;

            for (std::size_t j = 0; j < rMainSpaceCommon.m_satCount; ++j) {
                if (i == j) { continue; }

                double iMass = massView[i];
                double jMass = massView[j];

                Vector3d const iPos = Vector3d(Vector3g(x[i], y[i], z[i])) * scale;
                Vector3d const jPos = Vector3d(Vector3g(x[j], y[j], z[j])) * scale;

                double r = (jPos - iPos).length();
                Vector3d direction = (jPos - iPos).normalized();

                double forceMagnitude = (iMass * jMass) / (r * r);
                Vector3d force = direction * forceMagnitude;
                Vector3d acceleration = (force / iMass);

                vx[i] += acceleration.x() * uniDeltaTimeIn;
                vy[i] += acceleration.y() * uniDeltaTimeIn;
                vz[i] += acceleration.z() * uniDeltaTimeIn;
            }
        }
    });

}); // ftrSolarSystemPlanets



FeatureDef const ftrSolarSystemDraw = feature_def("SolarSystemDraw", [] (
        FeatureBuilder              &rFB,
        Implement<FIUniPlanetsDraw> uniPlanetsDraw,
        DependOn<FIWindowApp>       windowApp,
        DependOn<FISceneRenderer>   scnRender,
        DependOn<FICameraControl>   camCtrl,
        DependOn<FICommonScene>     comScn,
        DependOn<FIUniCore>         uniCore,
        DependOn<FIUniSceneFrame>   uniScnFrame,
        DependOn<FISolarSys>        solarSys,
        entt::any                   userData)
{
    auto const params = entt::any_cast<PlanetDrawParams>(userData);

    rFB.data_emplace<PlanetDraw>(uniPlanetsDraw.di.planetDraw, PlanetDraw{
        .planetMat = params.planetMat,
        .axisMat   = params.axisMat,
    });

    rFB.task()
        .name       ("Position SceneFrame center to Camera Controller target")
        .run_on     ({ windowApp.pl.inputs(Run) })
        .sync_with  ({ camCtrl.pl.camCtrl(Ready), uniScnFrame.pl.sceneFrame(Modify) })
        .args       ({              camCtrl.di.camCtrl, uniScnFrame.di.scnFrame })
        .func       ([](ACtxCameraController& rCamCtrl,   SceneFrame& rScnFrame) noexcept
    {
        if (!rCamCtrl.m_target.has_value())
        {
            return;
        }
        Vector3& rCamPl = rCamCtrl.m_target.value();

        // check origin translation
        // ADL used for Magnum::Math::sign/floor/abs
        float const maxDist = 512.0f;
        Vector3 const translate = sign(rCamPl) * floor(abs(rCamPl) / maxDist) * maxDist;

        if (!translate.isZero())
        {
            rCamCtrl.m_transform.translation() -= translate;
            rCamPl -= translate;

            // a bit janky to modify universe stuff directly here, but it works lol
            Vector3 const rotated = Quaternion(rScnFrame.m_rotation).transformVector(translate);
            rScnFrame.m_position += Vector3g(math::mul_2pow<Vector3, int>(rotated, rScnFrame.m_precision));
        }

        rScnFrame.m_scenePosition = Vector3g(math::mul_2pow<Vector3, int>(rCamCtrl.m_target.value(), rScnFrame.m_precision));
    });

    rFB.task()
        .name       ("Resync test planets, create DrawEnts")
        .run_on     ({ windowApp.pl.resync(Run) })
        .sync_with  ({ scnRender.pl.drawEntResized(ModifyOrSignal) })
        .args       ({       scnRender.di.scnRender, uniPlanetsDraw.di.planetDraw, uniCore.di.universe,     solarSys.di.planetMainSpace })
        .func       ([](ACtxSceneRender& rScnRender,      PlanetDraw& rPlanetDraw, Universe& rUniverse, CoSpaceId const planetMainSpace) noexcept
    {
        CoSpaceCommon& rMainSpace = rUniverse.m_coordCommon[planetMainSpace];

        rPlanetDraw.drawEnts.resize(rMainSpace.m_satCount, lgrn::id_null<DrawEnt>());
        rScnRender.m_drawIds.create(rPlanetDraw.drawEnts.begin(), rPlanetDraw.drawEnts.end());
    });

    rFB.task()
        .name       ("Resync test planets, add mesh and material")
        .run_on     ({ windowApp.pl.resync(Run) })
        .sync_with  ({ scnRender.pl.drawEntResized(Done), scnRender.pl.materialDirty(Modify_), scnRender.pl.entMeshDirty(Modify_) })
        .args       ({      comScn.di.drawing,      scnRender.di.scnRender,     comScn.di.namedMeshes, uniPlanetsDraw.di.planetDraw, uniCore.di.universe,     solarSys.di.planetMainSpace,                              solarSys.di.coordNBody })
        .func       ([](ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, NamedMeshes& rNamedMeshes,      PlanetDraw& rPlanetDraw, Universe& rUniverse, CoSpaceId const planetMainSpace, osp::KeyedVec<CoSpaceId, CoSpaceNBody>& rCoordNBody) noexcept
    {
        CoSpaceCommon& rMainSpace = rUniverse.m_coordCommon[planetMainSpace];

        Material& rMatPlanet = rScnRender.m_materials[rPlanetDraw.planetMat];

        MeshId const sphereMeshId = rNamedMeshes.m_shapeToMesh.at(EShape::Sphere);

        CoSpaceCommon& rMainSpaceCommon = rUniverse.m_coordCommon[planetMainSpace];
        auto const colorView = rCoordNBody[planetMainSpace].color.view(arrayView(rMainSpaceCommon.m_data), c_planetCount);

        for (std::size_t i = 0; i < rMainSpace.m_satCount; ++i)
        {
            DrawEnt const drawEnt = rPlanetDraw.drawEnts[i];

            rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(sphereMeshId);
            rScnRender.m_meshDirty.push_back(drawEnt);
            rScnRender.m_visible.insert(drawEnt);
            rScnRender.m_opaque.insert(drawEnt);
            rMatPlanet.m_ents.insert(drawEnt);
            rMatPlanet.m_dirty.push_back(drawEnt);

            rScnRender.m_color[drawEnt] = colorView[i];
        }
    });

    rFB.task()
        .name       ("Reposition test planet DrawEnts")
        .run_on     ({ scnRender.pl.render(Run) })
        .sync_with  ({ scnRender.pl.drawTransforms(Modify_), scnRender.pl.drawEntResized(Done), camCtrl.pl.camCtrl(Ready), uniScnFrame.pl.sceneFrame(Modify) })
        .args       ({       comScn.di.drawing,      scnRender.di.scnRender, uniPlanetsDraw.di.planetDraw, uniCore.di.universe,     uniScnFrame.di.scnFrame,     solarSys.di.planetMainSpace,                              solarSys.di.coordNBody })
        .func       ([] (ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender,      PlanetDraw& rPlanetDraw, Universe& rUniverse, SceneFrame const& rScnFrame, CoSpaceId const planetMainSpace, osp::KeyedVec<CoSpaceId, CoSpaceNBody>& rCoordNBody) noexcept
    {
        CoSpaceCommon& rMainSpace = rUniverse.m_coordCommon[planetMainSpace];
        auto const [x, y, z] = sat_views(rMainSpace.m_satPositions, rMainSpace.m_data, rMainSpace.m_satCount);
        auto const [qx, qy, qz, qw] = sat_views(rMainSpace.m_satRotations, rMainSpace.m_data, rMainSpace.m_satCount);
        auto const radiusView = rCoordNBody[planetMainSpace].radius.view(arrayView(rMainSpace.m_data), c_planetCount);

        // Calculate transform from universe to area/local-space for rendering.
        // This can be generalized by finding a common ancestor within the tree
        // of coordinate spaces. Since there's only two possibilities, an if
        // statement works.
        CoordTransformer mainToArea;
        if (rScnFrame.m_parent == planetMainSpace)
        {
            mainToArea = coord_parent_to_child(rMainSpace, rScnFrame);
        }
        else
        {
            CoSpaceId const landedId = rScnFrame.m_parent;
            CoSpaceCommon& rLanded = rUniverse.m_coordCommon[landedId];

            CoSpaceTransform const landedTf = coord_get_transform(rLanded, rLanded, x, y, z, qx, qy, qz, qw);
            CoordTransformer const mainToLanded = coord_parent_to_child(rMainSpace, landedTf);
            CoordTransformer const landedToArea = coord_parent_to_child(landedTf, rScnFrame);

            mainToArea = coord_composite(landedToArea, mainToLanded);
        }
        Quaternion const mainToAreaRot{ mainToArea.rotation() };

        float const f = math::mul_2pow<float, int>(1.0f, -rMainSpace.m_precision);

        for (std::size_t i = 0; i < rMainSpace.m_satCount; ++i)
        {
            Vector3g const relative = mainToArea.transform_position({ x[i], y[i], z[i] });
            Vector3 const relativeMeters = Vector3(relative);

            Quaterniond const rot{ {qx[i], qy[i], qz[i]}, qw[i] };

            DrawEnt const drawEnt = rPlanetDraw.drawEnts[i];

            float radius = radiusView[i];

            rScnRender.m_drawTransform[drawEnt] =
                Matrix4::translation(Vector3{ (float)x[i], (float)y[i], (float)z[i] })
                * Matrix4::scaling({ radius, radius, radius })
                * Matrix4 {
                (mainToAreaRot * Quaternion{ rot }).toMatrix()
            };
        }
    });
}); // ftrSolarSystemDraw


} // namespace adera
