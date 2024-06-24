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

#include "solar_system.h"
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

constexpr unsigned int c_planetCount = 5;

Session setup_solar_system_testplanets(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any>        topData,
        Session const&              solarSystemCore,
        Session const&              solarSystemScnFrame)
{
    using CoSpaceIdVec_t = std::vector<CoSpaceId>;
    using Corrade::Containers::Array;

    OSP_DECLARE_GET_DATA_IDS(solarSystemCore,     TESTAPP_DATA_UNI_CORE);
    OSP_DECLARE_GET_DATA_IDS(solarSystemScnFrame, TESTAPP_DATA_UNI_SCENEFRAME);

    auto const tgUCore = solarSystemCore    .get_pipelines<PlUniCore>();
    auto const tgUSFrm = solarSystemScnFrame.get_pipelines<PlUniSceneFrame>();

    auto &rUniverse = top_get< Universe >(topData, idUniverse);


    constexpr int precision = 10;

    // Create coordinate spaces
    CoSpaceId const mainSpace = rUniverse.m_coordIds.create();
    std::vector<CoSpaceId> satSurfaceSpaces(c_planetCount);
    rUniverse.m_coordIds.create(satSurfaceSpaces.begin(), satSurfaceSpaces.end());

    rUniverse.m_coordCommon.resize(rUniverse.m_coordIds.capacity());

    CoSpaceCommon &rMainSpaceCommon = rUniverse.m_coordCommon[mainSpace];
    rMainSpaceCommon.m_satCount     = c_planetCount;
    rMainSpaceCommon.m_satCapacity  = c_planetCount;

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_SOLAR_SYSTEM_PLANETS);
    auto& rCoordNBody = top_emplace< osp::KeyedVec<CoSpaceId, CoSpaceNBody> >(topData, idCoordNBody);
    rCoordNBody.resize(rUniverse.m_coordIds.capacity());

    // Associate each planet satellite with their surface coordinate space
    for (SatId satId = 0; satId < c_planetCount; ++satId)
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
    rMainSpaceCommon.m_data = Array<unsigned char>{Corrade::NoInit, bytesUsed};

    std::size_t nextBody = 0;
    auto const add_body = [&rMainSpaceCommon, &nextBody, &rCoordNBody, &mainSpace]
    (
        Vector3l position, 
        Vector3d velocity, 
        Magnum::Vector4d rotation, 
        float mass, 
        float radius, 
        Magnum::Color3 color
    )
    {
        auto const [x, y, z]        = sat_views(rMainSpaceCommon.m_satPositions,  rMainSpaceCommon.m_data, c_planetCount);
        auto const [vx, vy, vz]     = sat_views(rMainSpaceCommon.m_satVelocities, rMainSpaceCommon.m_data, c_planetCount);
        auto const [qx, qy, qz, qw] = sat_views(rMainSpaceCommon.m_satRotations,  rMainSpaceCommon.m_data, c_planetCount);

        auto const massView =   rCoordNBody[mainSpace].mass.view(arrayView(rMainSpaceCommon.m_data), c_planetCount);
        auto const radiusView = rCoordNBody[mainSpace].radius.view(arrayView(rMainSpaceCommon.m_data), c_planetCount);
        auto const colorView =  rCoordNBody[mainSpace].color.view(arrayView(rMainSpaceCommon.m_data), c_planetCount);

        x[nextBody] = position.x();
        y[nextBody] = position.y();
        z[nextBody] = position.z();

        vx[nextBody] = velocity.x();
        vy[nextBody] = velocity.y();
        vz[nextBody] = velocity.z();

        qx[nextBody] = rotation.x();
        qy[nextBody] = rotation.y();
        qz[nextBody] = rotation.z();
        qw[nextBody] = rotation.w();

        massView  [nextBody] = mass;
        radiusView[nextBody] = radius;
        colorView [nextBody] = color;

        ++nextBody;
    };

    add_body // Sun
    (
        Vector3l{ 0, 0, 0 }, 
        Vector3d{ 0.0, 0.0, 0.0 }, 
        Magnum::Vector4d{ 0.0, 0.0, 0.0, 1.0 }, 
        1.0f * std::pow(10, 1), 
        1000.0f, 
        { 1.0f, 1.0f, 0.0f }
    );

    add_body // Blue Planet
    (
        Vector3l{ 0, math::mul_2pow<spaceint_t, int>( 10, precision), 0 }, 
        Vector3d{ 1.0,         0.0, 0.0 }, 
        Magnum::Vector4d{ 0.0, 0.0, 0.0, 1.0 }, 
        0.0000000001f,           
        500.0f, 
        { 0.0f, 0.0f, 1.0f }
    );

    add_body // Red Planet
    (
        Vector3l{ 0, math::mul_2pow<spaceint_t, int>(  5, precision), 0 }, 
        Vector3d{ 1.414213562, 0.0, 0.0 }, 
        Magnum::Vector4d{ 0.0, 0.0, 0.0, 1.0 }, 
        0.0000000001f,           
        250.0f, 
        { 1.0f, 0.0f, 0.0f }
    );

    add_body // Green Planet
    (
        Vector3l{ 0, math::mul_2pow<spaceint_t, int>(7.5, precision), 0 }, 
        Vector3d{ 1.154700538, 0.0, 0.0 }, 
        Magnum::Vector4d{ 0.0, 0.0, 0.0, 1.0 }, 
        0.0000000001f,           
        600.0f, 
        { 0.0f, 1.0f, 0.0f }
    );

    add_body // Orange Planet
    (
        Vector3l{ 0, math::mul_2pow<spaceint_t, int>( 12, precision), 0 }, 
        Vector3d{ 0.912870929, 0.0, 0.0 }, 
        Magnum::Vector4d{ 0.0, 0.0, 0.0, 1.0 }, 
        0.0000000001f,           
        550.0f, 
        { 1.0f, 0.5f, 0.0f }
    );

    top_emplace< CoSpaceId >(topData, idPlanetMainSpace, mainSpace);
    top_emplace< float >(topData, tgUniDeltaTimeIn, 1.0f / 60.0f);
    top_emplace< CoSpaceIdVec_t >(topData, idSatSurfaceSpaces, std::move(satSurfaceSpaces));

    // Set initial scene frame

    auto &rScnFrame      = top_get<SceneFrame>(topData, idScnFrame);
    rScnFrame.m_parent   = mainSpace;
    rScnFrame.m_position = math::mul_2pow<Vector3g, int>({400, 400, 400}, precision);

    rBuilder.task()
        .name       ("Update planets")
        .run_on     (tgUCore.update(Run))
        .sync_with  ({tgUSFrm.sceneFrame(Modify)})
        .push_to    (out.m_tasks)
        .args       ({     idUniverse,               idPlanetMainSpace,            idScnFrame,                      idSatSurfaceSpaces,           tgUniDeltaTimeIn,                                        idCoordNBody})
        .func([] (Universe& rUniverse, CoSpaceId const planetMainSpace, SceneFrame &rScnFrame, CoSpaceIdVec_t const& rSatSurfaceSpaces, float const uniDeltaTimeIn, osp::KeyedVec<CoSpaceId, CoSpaceNBody>& rCoordNBody) noexcept
    {
        CoSpaceCommon &rMainSpaceCommon = rUniverse.m_coordCommon[planetMainSpace];

        auto const scale = osp::math::mul_2pow<double, int>(1.0, -rMainSpaceCommon.m_precision);
        double const scaleDelta = uniDeltaTimeIn / scale;

        auto const [x, y, z]        = sat_views(rMainSpaceCommon.m_satPositions,  rMainSpaceCommon.m_data, rMainSpaceCommon.m_satCount);
        auto const [vx, vy, vz]     = sat_views(rMainSpaceCommon.m_satVelocities, rMainSpaceCommon.m_data, rMainSpaceCommon.m_satCount);

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

    return out;
} // setup_solar_system_testplanets


struct PlanetDraw
{
    DrawEntVec_t            drawEnts;
    MaterialId              matPlanets;
};

Session setup_solar_system_planets_draw(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              windowApp,
        Session const&              sceneRenderer,
        Session const&              cameraCtrl,
        Session const&              commonScene,
        Session const&              solarSystemCore,
        Session const&              solarSystemScnFrame,
        Session const&              solarSystemTestPlanets,
        MaterialId const            matPlanets)
{
    OSP_DECLARE_GET_DATA_IDS(commonScene,            TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer,          TESTAPP_DATA_SCENE_RENDERER);
    OSP_DECLARE_GET_DATA_IDS(cameraCtrl,             TESTAPP_DATA_CAMERA_CTRL);
    OSP_DECLARE_GET_DATA_IDS(solarSystemCore,        TESTAPP_DATA_UNI_CORE);
    OSP_DECLARE_GET_DATA_IDS(solarSystemScnFrame,    TESTAPP_DATA_UNI_SCENEFRAME);
    OSP_DECLARE_GET_DATA_IDS(solarSystemTestPlanets, TESTAPP_DATA_SOLAR_SYSTEM_PLANETS);

    auto const tgWin    = windowApp          .get_pipelines<PlWindowApp>();
    auto const tgScnRdr = sceneRenderer      .get_pipelines<PlSceneRenderer>();
    auto const tgCmCt   = cameraCtrl         .get_pipelines<PlCameraCtrl>();
    auto const tgUSFrm  = solarSystemScnFrame.get_pipelines<PlUniSceneFrame>();

    Session out;

    auto const [idPlanetDraw] = out.acquire_data<1>(topData);

    auto &rPlanetDraw = top_emplace<PlanetDraw>(topData, idPlanetDraw);

    rPlanetDraw.matPlanets = matPlanets;

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
        rScnRender.m_drawIds.create(rPlanetDraw.drawEnts.begin(), rPlanetDraw.drawEnts.end());
    });

    rBuilder.task()
        .name       ("Resync test planets, add mesh and material")
        .run_on     ({tgWin.resync(Run)})
        .sync_with  ({tgScnRdr.drawEntResized(Done), tgScnRdr.materialDirty(Modify_), tgScnRdr.entMeshDirty(Modify_)})
        .push_to    (out.m_tasks)
        .args       ({           idDrawing,                 idScnRender,             idNMesh,            idPlanetDraw,          idUniverse,               idPlanetMainSpace,                                        idCoordNBody})
        .func([]    (ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, NamedMeshes& rNMesh, PlanetDraw& rPlanetDraw, Universe& rUniverse, CoSpaceId const planetMainSpace, osp::KeyedVec<CoSpaceId, CoSpaceNBody>& rCoordNBody) noexcept
    {
        CoSpaceCommon &rMainSpace = rUniverse.m_coordCommon[planetMainSpace];

        Material &rMatPlanet = rScnRender.m_materials[rPlanetDraw.matPlanets];

        MeshId const sphereMeshId = rNMesh.m_shapeToMesh.at(EShape::Sphere);

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

    rBuilder.task()
        .name       ("Reposition test planet DrawEnts")
        .run_on     ({tgScnRdr.render(Run)})
        .sync_with  ({tgScnRdr.drawTransforms(Modify_), tgScnRdr.drawEntResized(Done), tgCmCt.camCtrl(Ready), tgUSFrm.sceneFrame(Modify)})
        .push_to    (out.m_tasks)
        .args       ({        idDrawing,                 idScnRender,            idPlanetDraw,          idUniverse,                  idScnFrame,               idPlanetMainSpace,                                        idCoordNBody })
        .func([] (ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, PlanetDraw& rPlanetDraw, Universe& rUniverse, SceneFrame const& rScnFrame, CoSpaceId const planetMainSpace, osp::KeyedVec<CoSpaceId, CoSpaceNBody>& rCoordNBody) noexcept
    {
        CoSpaceCommon &rMainSpace = rUniverse.m_coordCommon[planetMainSpace];
        auto const [x, y, z]        = sat_views(rMainSpace.m_satPositions, rMainSpace.m_data, rMainSpace.m_satCount);
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
            CoSpaceCommon &rLanded = rUniverse.m_coordCommon[landedId];

            CoSpaceTransform const landedTf     = coord_get_transform(rLanded, rLanded, x, y, z, qx, qy, qz, qw);
            CoordTransformer const mainToLanded = coord_parent_to_child(rMainSpace, landedTf);
            CoordTransformer const landedToArea = coord_parent_to_child(landedTf, rScnFrame);

            mainToArea = coord_composite(landedToArea, mainToLanded);
        }
        Quaternion const mainToAreaRot{mainToArea.rotation()};

        float const f = math::mul_2pow<float, int>(1.0f, -rMainSpace.m_precision);

        for (std::size_t i = 0; i < rMainSpace.m_satCount; ++i)
        {
            Vector3g const relative = mainToArea.transform_position({x[i], y[i], z[i]});
            Vector3 const relativeMeters = Vector3(relative);

            Quaterniond const rot{{qx[i], qy[i], qz[i]}, qw[i]};

            DrawEnt const drawEnt = rPlanetDraw.drawEnts[i];

            float radius = radiusView[i];

            rScnRender.m_drawTransform[drawEnt] = 
                Matrix4::translation(Vector3{ (float)x[i], (float)y[i], (float)z[i] })
                * Matrix4::scaling({ radius, radius, radius })
                * Matrix4{ (mainToAreaRot * Quaternion{ rot }).toMatrix() };
        }
    });

    return out;
} // setup_solar_system_planets_draw


} // namespace testapp::scenes
