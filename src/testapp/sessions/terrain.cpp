/**
 * Open Space Program
 * Copyright © 2019-2023 Open Space Program Project
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
#include "terrain.h"
#include "common.h"

#include <adera/drawing/CameraController.h>
#include <longeron/utility/asserts.hpp>
#include <osp/core/math_2pow.h>
#include <osp/core/math_int64.h>
#include <osp/drawing/drawing.h>
#include <planet-a/icosahedron.h>
#include <planet-a/terrain_skeleton.h>

#include <iostream>

using adera::ACtxCameraController;

using namespace planeta;
using namespace osp;
using namespace osp::math;
using namespace osp::draw;

namespace testapp::scenes
{

Session setup_terrain(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              scene)
{
    auto const tgScn = scene.get_pipelines<PlScene>();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_TERRAIN);
    auto const tgTrn = out.create_pipelines<PlTerrain>(rBuilder);

    rBuilder.pipeline(tgTrn.skeleton)    .parent(tgScn.update);
    rBuilder.pipeline(tgTrn.surfaceFrame).parent(tgScn.update);

    auto &rSurfaceFrame = top_emplace< ACtxSurfaceFrame >(topData, idSurfaceFrame);
    auto &rTerrain      = top_emplace< ACtxTerrain >     (topData, idTerrain);
    auto &rTerrainIco   = top_emplace< ACtxTerrainIco >  (topData, idTerrainIco);

    rBuilder.task()
        .name       ("Initialize terrain when entering planet coordinate space")
        .run_on     ({tgScn.update(Run)})
        .sync_with  ({tgTrn.surfaceFrame(Modify)})
        .push_to    (out.m_tasks)
        .args({                    idSurfaceFrame,             idTerrain,                idTerrainIco })
        .func([] (ACtxSurfaceFrame &rSurfaceFrame, ACtxTerrain &rTerrain, ACtxTerrainIco &rTerrainIco) noexcept
    {
        if ( ! rSurfaceFrame.active )
        {
            rSurfaceFrame.active = true;

            rTerrainIco.radius = 50.0f;
            rTerrainIco.height = 5.0f;
            rTerrain   .scale  = 10;

            rTerrain.skel = create_skeleton_icosahedron(rTerrainIco.radius, rTerrain.scale, rTerrainIco.icoVrtx, rTerrainIco.icoGroups, rTerrainIco.icoTri, rTerrain.skPositions, rTerrain.skNormals);

            rTerrain.sktriCenter.resize(rTerrain.skel.tri_group_ids().capacity() * 4);

            float const maxRadius = rTerrainIco.radius + rTerrainIco.height;
            for (SkTriGroupId const groupId : rTerrainIco.icoGroups)
            {
                calc_sphere_tri_center(groupId, rTerrain, maxRadius, rTerrainIco.height);
            }

            // Apply spherical curvature when skeleton triangles are subdivided.
            // (Terrain subdivider is intended to work for non-spherical shapes too)
            rTerrain.scratchpad.onSubdivUserData = &rTerrainIco;
            rTerrain.scratchpad.onSubdiv = [] (
                    SkTriId                 tri,
                    SkTriGroupId            groupId,
                    std::array<SkVrtxId, 3> corners,
                    std::array<MaybeNewId<SkVrtxId>, 3> middles,
                    TerrainSkeleton&        rTrn,
                    void*                   pUserData) noexcept
            {
                auto const& rTerrainIco = *reinterpret_cast<ACtxTerrainIco*>(pUserData);
                ico_calc_middles(rTerrainIco.radius, rTrn.scale, corners, middles, rTrn.skPositions, rTrn.skNormals);
                calc_sphere_tri_center(groupId, rTrn, rTerrainIco.radius + rTerrainIco.height, rTerrainIco.height);
            };

            rTerrain.scratchpad.onUnsubdiv = [] (
                    SkTriId             tri,
                    SkeletonTriangle&   rTri,
                    TerrainSkeleton&    skel,
                    void*               userData) noexcept
            {
            };

            // Calculate distance thresholds
            for (int level = 0; level < rTerrain.scratchpad.levelMax; ++level)
            {
                // Good-enough bounding sphere is ~75% of the edge length (determined using Blender)
                float const subdivRadius = 0.75f * gc_icoMaxEdgeVsLevel[level] * rTerrainIco.radius * int_2pow<int>(rTerrain.scale);

                // TODO: Pick thresholds based on the angular diameter (size on screen) of the
                //       chunk triangle mesh that will actually be rendered.

                rTerrain.scratchpad.distanceThresholdSubdiv[level] = std::uint64_t(subdivRadius);

                // Unsubdivide thresholds should be slightly larger (arbitrary x2)
                rTerrain.scratchpad.distanceThresholdUnsubdiv[level] = std::uint64_t(2.0f * subdivRadius);
            }
        }
    });

    rBuilder.task()
        .name       ("subdivide triangle skeleton")
        .run_on     ({tgScn.update(Run)})
        .sync_with  ({tgTrn.surfaceFrame(Ready), tgTrn.skeleton(New)})
        .push_to    (out.m_tasks)
        .args({                    idSurfaceFrame,             idTerrain,                idTerrainIco })
        .func([] (ACtxSurfaceFrame &rSurfaceFrame, ACtxTerrain &rTerrain, ACtxTerrainIco &rTerrainIco) noexcept
    {
        if ( ! rSurfaceFrame.active )
        {
            return;
        }

        SubdivScratchpad &rSP = rTerrain.scratchpad;

        auto const triCapacity = rTerrain.skel.tri_group_ids().capacity() * 4;
        bitvector_resize(rSP.distanceTestDone, triCapacity);
        bitvector_resize(rSP.tryUnsubdiv,      triCapacity);
        bitvector_resize(rSP.cantUnsubdiv,     triCapacity);

        for (int level = rSP.levelMax-1; level >= 0; --level)
        {
            unsubdivide_level_by_distance(level, rSurfaceFrame.position, rTerrain, rSP);
            unsubdivide_level_check_rules(level, rTerrain, rSP);
            unsubdivide_level(level, rTerrain, rSP);
        }
        rSP.distanceTestDone.reset();

        debug_check_rules(rTerrain);

        if (rSP.levelMax > 0)
        {
            for (SkTriId const sktriId : rTerrainIco.icoTri)
            {
                rSP.levels[0].distanceTestNext.push_back(sktriId);
                rSP.distanceTestDone.set(sktriId.value);
            }
            rSP.levelNeedProcess = 0;
        }
        for (int level = 0; level < rSP.levelMax; ++level)
        {
            subdivide_level_by_distance(rSurfaceFrame.position, level, rTerrain, rSP);
        }

        rSP.distanceTestDone.reset();

        debug_check_rules(rTerrain);
    });

    return out;
}


struct TerrainDebugDraw
{
    KeyedVec<SkVrtxId, osp::draw::DrawEnt> verts;
    MaterialId mat;
};

Session setup_terrain_debug_draw(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              windowApp,
        Session const&              sceneRenderer,
        Session const&              cameraCtrl,
        Session const&              commonScene,
        Session const&              terrain,
        MaterialId const            mat)
{
    OSP_DECLARE_GET_DATA_IDS(commonScene,    TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer,  TESTAPP_DATA_SCENE_RENDERER);
    OSP_DECLARE_GET_DATA_IDS(cameraCtrl,     TESTAPP_DATA_CAMERA_CTRL);
    OSP_DECLARE_GET_DATA_IDS(terrain,        TESTAPP_DATA_TERRAIN);

    auto const tgWin    = windowApp     .get_pipelines<PlWindowApp>();
    auto const tgScnRdr = sceneRenderer .get_pipelines<PlSceneRenderer>();
    auto const tgCmCt   = cameraCtrl    .get_pipelines<PlCameraCtrl>();
    auto const tgTrn    = terrain       .get_pipelines<PlTerrain>();

    Session out;
    auto const [idTrnDbgDraw] = out.acquire_data<1>(topData);

    auto &rTrnDbgDraw = top_emplace< TerrainDebugDraw > (topData, idTrnDbgDraw);

    rTrnDbgDraw.mat = mat;


    rBuilder.task()
        .name       ("Position SceneFrame center to Camera Controller target")
        .run_on     ({tgWin.inputs(Run)})
        .sync_with  ({tgCmCt.camCtrl(Ready), tgTrn.surfaceFrame(Modify)})
        .push_to    (out.m_tasks)
        .args       ({                 idCamCtrl,                  idSurfaceFrame,             idTerrain,                idTerrainIco })
        .func([] (ACtxCameraController& rCamCtrl, ACtxSurfaceFrame &rSurfaceFrame, ACtxTerrain &rTerrain, ACtxTerrainIco &rTerrainIco) noexcept
    {
        if ( ! rCamCtrl.m_target.has_value())
        {
            return;
        }
        Vector3 camPos = rCamCtrl.m_target.value();

        float const len = camPos.length();
        float const midHeightRadius = rTerrainIco.radius + 0.5f*rTerrainIco.height;
        if (len < midHeightRadius)
        {
            camPos *= midHeightRadius / len;
        }

        rSurfaceFrame.position = Vector3l(camPos * int_2pow<int>(rTerrain.scale));
    });


    rBuilder.task()
        .name       ("do spooky scary skeleton stuff")
        .run_on     ({tgScnRdr.render(Run)})
        .sync_with  ({tgScnRdr.drawTransforms(Modify_), tgScnRdr.entMeshDirty(Modify_), tgScnRdr.drawEntResized(ModifyOrSignal)})
        .push_to    (out.m_tasks)
        .args       ({        idDrawing,                 idScnRender,             idNMesh,                  idTrnDbgDraw,             idTerrain,                idTerrainIco })
        .func([] (ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, NamedMeshes& rNMesh, TerrainDebugDraw& rTrnDbgDraw, ACtxTerrain &rTerrain, ACtxTerrainIco &rTerrainIco) noexcept
    {

        Material &rMatPlanet = rScnRender.m_materials[rTrnDbgDraw.mat];
        MeshId const cubeMeshId   = rNMesh.m_shapeToMesh.at(EShape::Box);

        SubdivIdTree<SkVrtxId> const& vrtxIds = rTerrain.skel.vrtx_ids();
        rTrnDbgDraw.verts.resize(vrtxIds.capacity());

        for (std::size_t skVertInt = 0; skVertInt < vrtxIds.capacity(); ++skVertInt)
        {
            auto const skVert = SkVrtxId(skVertInt);
            DrawEnt &rDrawEnt = rTrnDbgDraw.verts[skVert];

            if (vrtxIds.exists(skVert))
            {
                if ( ! rDrawEnt.has_value())
                {
                    rDrawEnt = rScnRender.m_drawIds.create();
                }
            }
            else
            {
                if (rDrawEnt.has_value())
                {
                    if (rScnRender.m_mesh[rDrawEnt].has_value())
                    {
                        rDrawing.m_meshRefCounts.ref_release(std::exchange(rScnRender.m_mesh[rDrawEnt], {}));

                    }
                    rScnRender.m_meshDirty  .push_back  (rDrawEnt);
                    rScnRender.m_visible    .reset      (rDrawEnt.value);
                    rMatPlanet.m_ents       .reset      (rDrawEnt.value);
                    rMatPlanet.m_dirty      .push_back  (rDrawEnt);

                    rScnRender.m_drawIds.remove(std::exchange(rDrawEnt, {}));
                }
            }
        }

        rScnRender.resize_draw(); // >:)

        for (std::size_t const skVertInt : vrtxIds.bitview().zeros())
        if (auto const skVert = SkVrtxId(skVertInt);
            vrtxIds.exists(skVert))
        {
            DrawEnt const drawEnt = rTrnDbgDraw.verts[skVert];

            if ( ! drawEnt.has_value() )
            {
                continue;
            }

            if (rScnRender.m_mesh[drawEnt].has_value() == false)
            {
                rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(cubeMeshId);
                rScnRender.m_meshDirty.push_back(drawEnt);
                rScnRender.m_visible.set(drawEnt.value);
                rScnRender.m_opaque.set(drawEnt.value);
                rMatPlanet.m_ents.set(drawEnt.value);
                rMatPlanet.m_dirty.push_back(drawEnt);
            }

            rScnRender.m_drawTransform[drawEnt]
                = Matrix4::translation(Vector3(rTerrain.skPositions[skVert]) / int_2pow<int>(rTerrain.scale))
                * Matrix4::scaling({0.05f, 0.05f, 0.05f});
        }
    });

    return out;
}

} // namespace testapp::scenes
