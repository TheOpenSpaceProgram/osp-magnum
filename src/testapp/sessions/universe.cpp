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

#include "universe.h"
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
} // setup_uni_core




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
} // setup_uni_sceneframe


Session setup_uni_landerplanet(
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
    constexpr int           planetCount     = 1;

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

    for (std::size_t i = 0; i < planetCount; ++i)
    {
        // Assign each planet random positions and velocities
        x[i] = 0;
        y[i] = 0;
        z[i] = 0;
        vx[i] = 0;
        vy[i] = 0;
        vz[i] = 0;

        // No rotation
        qx[i] = 0.0;
        qy[i] = 0.0;
        qz[i] = 0.0;
        qw[i] = 1.0;
    }

    auto &rScnFrame      = top_get<SceneFrame>(topData, idScnFrame);
    rScnFrame.m_parent   = mainSpace;
    rScnFrame.m_position = math::mul_2pow<Vector3g, int>({400, 400, 400}, precision);

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_UNI_PLANETS);

    top_emplace< CoSpaceId >        (topData, idPlanetMainSpace, mainSpace);
    top_emplace< float >            (topData, tgUniDeltaTimeIn, 1.0f / 60.0f);
    top_emplace< CoSpaceIdVec_t >   (topData, idSatSurfaceSpaces, std::move(satSurfaceSpaces));

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

    return out;
} // setup_uni_testplanets




struct PlanetDraw
{
    DrawEntVec_t            drawEnts;
    std::array<DrawEnt, 3>  axis;
    DrawEnt                 attractor;
    MaterialId              matPlanets;
    MaterialId              matAxis;
};

Session setup_landerplanet_draw(
    TopTaskBuilder&             rBuilder,
    ArrayView<entt::any> const  topData,
    Session const&              windowApp,
    Session const&              sceneRenderer,
    Session const&              cameraCtrl,
    Session const&              commonScene,
    Session const&              uniCore,
    Session const&              uniScnFrame,
    Session const&              uniLanderPlanet,
    MaterialId const            matPlanets,
    MaterialId const            matAxis
) {
    OSP_DECLARE_GET_DATA_IDS(commonScene,    TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer,  TESTAPP_DATA_SCENE_RENDERER);
    OSP_DECLARE_GET_DATA_IDS(cameraCtrl,     TESTAPP_DATA_CAMERA_CTRL);
    OSP_DECLARE_GET_DATA_IDS(uniCore,        TESTAPP_DATA_UNI_CORE);
    OSP_DECLARE_GET_DATA_IDS(uniScnFrame,    TESTAPP_DATA_UNI_SCENEFRAME);
    OSP_DECLARE_GET_DATA_IDS(uniLanderPlanet, TESTAPP_DATA_UNI_PLANETS);

    auto const tgWin    = windowApp     .get_pipelines<PlWindowApp>();
    auto const tgScnRdr = sceneRenderer .get_pipelines<PlSceneRenderer>();
    auto const tgCmCt   = cameraCtrl    .get_pipelines<PlCameraCtrl>();
    auto const tgUSFrm  = uniScnFrame   .get_pipelines<PlUniSceneFrame>();

    Session out;

    auto const [idPlanetDraw] = out.acquire_data<1>(topData);
    auto &rPlanetDraw = top_emplace<PlanetDraw>(topData, idPlanetDraw);
    rPlanetDraw.matPlanets = matPlanets;
    rPlanetDraw.matAxis    = matAxis;

    rBuilder.task()
        .name       ("Position SceneFrame center to Camera Controller target")
        .run_on     ({tgWin.inputs(Run)})
        .sync_with  ({tgCmCt.camCtrl(Ready), tgUSFrm.sceneFrame(Modify)})
        .push_to    (out.m_tasks)
        .args       ({                 idCamCtrl,            idScnFrame })
        .func([] (ACtxCameraController& rCamCtrl, SceneFrame& rScnFrame) noexcept
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

    rBuilder.task()
        .name       ("Resync test planets, create DrawEnts")
        .run_on     ({tgWin.resync(Run)})
        .sync_with  ({tgScnRdr.drawEntResized(ModifyOrSignal)})
        .push_to    (out.m_tasks)
        .args       ({               idScnRender,            idPlanetDraw,          idUniverse,               idPlanetMainSpace})
        .func([]    (ACtxSceneRender& rScnRender, PlanetDraw& rPlanetDraw, Universe& rUniverse, CoSpaceId const planetMainSpace) noexcept
    {
        CoSpaceCommon &rMainSpace = rUniverse.m_coordCommon[planetMainSpace];

        rPlanetDraw.drawEnts.resize(rMainSpace.m_satCount, lgrn::id_null<DrawEnt>());

        rScnRender.m_drawIds.create(rPlanetDraw.drawEnts   .begin(), rPlanetDraw.drawEnts   .end());
        rScnRender.m_drawIds.create(rPlanetDraw.axis       .begin(), rPlanetDraw.axis       .end());
        rPlanetDraw.attractor = rScnRender.m_drawIds.create();
    });
    
    rBuilder.task()
        .name       ("Resync test planets, add mesh and material")
        .run_on     ({tgWin.resync(Run)})
        .sync_with  ({tgScnRdr.drawEntResized(Done), tgScnRdr.materialDirty(Modify_), tgScnRdr.entMeshDirty(Modify_)})
        .push_to    (out.m_tasks)
        .args       ({           idDrawing,                 idScnRender,             idNMesh,            idPlanetDraw,          idUniverse,               idPlanetMainSpace})
        .func([]    (ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, NamedMeshes& rNMesh, PlanetDraw& rPlanetDraw, Universe& rUniverse, CoSpaceId const planetMainSpace) noexcept
    {
        CoSpaceCommon &rMainSpace = rUniverse.m_coordCommon[planetMainSpace];

        Material &rMatPlanet = rScnRender.m_materials[rPlanetDraw.matPlanets];
        Material &rMatAxis   = rScnRender.m_materials[rPlanetDraw.matAxis];

        MeshId const sphereMeshId = rNMesh.m_shapeToMesh.at(EShape::Sphere);
        MeshId const cubeMeshId   = rNMesh.m_shapeToMesh.at(EShape::Box);

        for (std::size_t i = 0; i < rMainSpace.m_satCount; ++i)
        {
            DrawEnt const drawEnt = rPlanetDraw.drawEnts[i];

            rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(sphereMeshId);
            rScnRender.m_meshDirty.push_back(drawEnt);
            rScnRender.m_visible.set(std::size_t(drawEnt));
            rScnRender.m_opaque.set(std::size_t(drawEnt));
            rMatPlanet.m_ents.set(std::size_t(drawEnt));
            rMatPlanet.m_dirty.push_back(drawEnt);
        }

        rScnRender.m_mesh[rPlanetDraw.attractor] = rDrawing.m_meshRefCounts.ref_add(sphereMeshId);
        rScnRender.m_meshDirty.push_back(rPlanetDraw.attractor);
        rScnRender.m_visible.set(std::size_t(rPlanetDraw.attractor));
        rScnRender.m_opaque.set(std::size_t(rPlanetDraw.attractor));
        rMatPlanet.m_ents.set(std::size_t(rPlanetDraw.attractor));
        rMatPlanet.m_dirty.push_back(rPlanetDraw.attractor);

        for (DrawEnt const drawEnt : rPlanetDraw.axis)
        {
            rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(cubeMeshId);
            rScnRender.m_meshDirty.push_back(drawEnt);
            rScnRender.m_visible.set(std::size_t(drawEnt));
            rScnRender.m_opaque.set(std::size_t(drawEnt));
            rMatAxis.m_ents.set(std::size_t(drawEnt));
            rMatAxis.m_dirty.push_back(drawEnt);
        }

        rScnRender.m_color[rPlanetDraw.axis[0]] = {1.0f, 0.0f, 0.0f, 1.0f};
        rScnRender.m_color[rPlanetDraw.axis[1]] = {0.0f, 1.0f, 0.0f, 1.0f};
        rScnRender.m_color[rPlanetDraw.axis[2]] = {0.0f, 0.0f, 1.0f, 1.0f};
    });

    rBuilder.task()
        .name       ("Reposition test planet DrawEnts")
        .run_on     ({tgScnRdr.render(Run)})
        .sync_with  ({tgScnRdr.drawTransforms(Modify_), tgScnRdr.drawEntResized(Done), tgCmCt.camCtrl(Ready), tgUSFrm.sceneFrame(Modify)})
        .push_to    (out.m_tasks)
        .args       ({        idDrawing,                 idScnRender,            idPlanetDraw,          idUniverse,                  idScnFrame,               idPlanetMainSpace})
        .func([] (ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, PlanetDraw& rPlanetDraw, Universe& rUniverse, SceneFrame const& rScnFrame, CoSpaceId const planetMainSpace) noexcept
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

    return out;
}

Session setup_testplanets_draw(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              windowApp,
        Session const&              sceneRenderer,
        Session const&              cameraCtrl,
        Session const&              commonScene,
        Session const&              uniCore,
        Session const&              uniScnFrame,
        Session const&              uniTestPlanets,
        MaterialId const            matPlanets,
        MaterialId const            matAxis)
{
    OSP_DECLARE_GET_DATA_IDS(commonScene,    TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer,  TESTAPP_DATA_SCENE_RENDERER);
    OSP_DECLARE_GET_DATA_IDS(cameraCtrl,     TESTAPP_DATA_CAMERA_CTRL);
    OSP_DECLARE_GET_DATA_IDS(uniCore,        TESTAPP_DATA_UNI_CORE);
    OSP_DECLARE_GET_DATA_IDS(uniScnFrame,    TESTAPP_DATA_UNI_SCENEFRAME);
    OSP_DECLARE_GET_DATA_IDS(uniTestPlanets, TESTAPP_DATA_UNI_PLANETS);

    auto const tgWin    = windowApp     .get_pipelines<PlWindowApp>();
    auto const tgScnRdr = sceneRenderer .get_pipelines<PlSceneRenderer>();
    auto const tgCmCt   = cameraCtrl    .get_pipelines<PlCameraCtrl>();
    auto const tgUSFrm  = uniScnFrame   .get_pipelines<PlUniSceneFrame>();

    Session out;

    auto const [idPlanetDraw] = out.acquire_data<1>(topData);

    auto &rPlanetDraw = top_emplace<PlanetDraw>(topData, idPlanetDraw);

    rPlanetDraw.matPlanets = matPlanets;
    rPlanetDraw.matAxis    = matAxis;

    rBuilder.task()
        .name       ("Position SceneFrame center to Camera Controller target")
        .run_on     ({tgWin.inputs(Run)})
        .sync_with  ({tgCmCt.camCtrl(Ready), tgUSFrm.sceneFrame(Modify)})
        .push_to    (out.m_tasks)
        .args       ({                 idCamCtrl,            idScnFrame })
        .func([] (ACtxCameraController& rCamCtrl, SceneFrame& rScnFrame) noexcept
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

    rBuilder.task()
        .name       ("Resync test planets, create DrawEnts")
        .run_on     ({tgWin.resync(Run)})
        .sync_with  ({tgScnRdr.drawEntResized(ModifyOrSignal)})
        .push_to    (out.m_tasks)
        .args       ({               idScnRender,            idPlanetDraw,          idUniverse,               idPlanetMainSpace})
        .func([]    (ACtxSceneRender& rScnRender, PlanetDraw& rPlanetDraw, Universe& rUniverse, CoSpaceId const planetMainSpace) noexcept
    {
        CoSpaceCommon &rMainSpace = rUniverse.m_coordCommon[planetMainSpace];

        rPlanetDraw.drawEnts.resize(rMainSpace.m_satCount, lgrn::id_null<DrawEnt>());

        rScnRender.m_drawIds.create(rPlanetDraw.drawEnts   .begin(), rPlanetDraw.drawEnts   .end());
        rScnRender.m_drawIds.create(rPlanetDraw.axis       .begin(), rPlanetDraw.axis       .end());
        rPlanetDraw.attractor = rScnRender.m_drawIds.create();
    });

    rBuilder.task()
        .name       ("Resync test planets, add mesh and material")
        .run_on     ({tgWin.resync(Run)})
        .sync_with  ({tgScnRdr.drawEntResized(Done), tgScnRdr.materialDirty(Modify_), tgScnRdr.entMeshDirty(Modify_)})
        .push_to    (out.m_tasks)
        .args       ({           idDrawing,                 idScnRender,             idNMesh,            idPlanetDraw,          idUniverse,               idPlanetMainSpace})
        .func([]    (ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, NamedMeshes& rNMesh, PlanetDraw& rPlanetDraw, Universe& rUniverse, CoSpaceId const planetMainSpace) noexcept
    {
        CoSpaceCommon &rMainSpace = rUniverse.m_coordCommon[planetMainSpace];

        Material &rMatPlanet = rScnRender.m_materials[rPlanetDraw.matPlanets];
        Material &rMatAxis   = rScnRender.m_materials[rPlanetDraw.matAxis];

        MeshId const sphereMeshId = rNMesh.m_shapeToMesh.at(EShape::Sphere);
        MeshId const cubeMeshId   = rNMesh.m_shapeToMesh.at(EShape::Box);

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

    rBuilder.task()
        .name       ("Reposition test planet DrawEnts")
        .run_on     ({tgScnRdr.render(Run)})
        .sync_with  ({tgScnRdr.drawTransforms(Modify_), tgScnRdr.drawEntResized(Done), tgCmCt.camCtrl(Ready), tgUSFrm.sceneFrame(Modify)})
        .push_to    (out.m_tasks)
        .args       ({        idDrawing,                 idScnRender,            idPlanetDraw,          idUniverse,                  idScnFrame,               idPlanetMainSpace})
        .func([] (ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, PlanetDraw& rPlanetDraw, Universe& rUniverse, SceneFrame const& rScnFrame, CoSpaceId const planetMainSpace) noexcept
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

    return out;
} // setup_testplanets_draw


} // namespace testapp::scenes
