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

    rBuilder.pipeline(tgTrn.skeleton)       .parent(tgScn.update);
    rBuilder.pipeline(tgTrn.surfaceChanges) .parent(tgScn.update);
    rBuilder.pipeline(tgTrn.terrainFrame)   .parent(tgScn.update);

    auto &rTerrainFrame = top_emplace< ACtxTerrainFrame >(topData, idTerrainFrame);
    auto &rTerrain      = top_emplace< ACtxTerrain >     (topData, idTerrain);
    auto &rTerrainIco   = top_emplace< ACtxTerrainIco >  (topData, idTerrainIco);

    rBuilder.task()
        .name       ("Initialize terrain when entering planet coordinate space")
        .run_on     ({tgScn.update(Run)})
        .sync_with  ({tgTrn.terrainFrame(Modify)})
        .push_to    (out.m_tasks)
        .args({                    idTerrainFrame,             idTerrain,                idTerrainIco })
        .func([] (ACtxTerrainFrame &rTerrainFrame, ACtxTerrain &rTerrain, ACtxTerrainIco &rTerrainIco) noexcept
    {
        if ( ! rTerrainFrame.active )
        {
            SubdivScratchpad &rSP = rTerrain.scratchpad;

            rTerrainFrame.active = true;

            rTerrainIco.radius = 50.0f;
            rTerrainIco.height = 5.0f;
            rTerrain.skTerrain.scale  = 10;

            constexpr std::uint8_t chunkSubdivLevels = 4;

            rTerrain.skTerrain.skel = create_skeleton_icosahedron(rTerrainIco.radius, rTerrain.skTerrain.scale, rTerrainIco.icoVrtx, rTerrainIco.icoGroups, rTerrainIco.icoTri, rTerrain.skTerrain.skPositions, rTerrain.skTerrain.skNormals);
            rTerrain.skChunks       = make_skeleton_chunks       (chunkSubdivLevels);
            rTerrain.chunkInfo      = make_chunked_mesh_info     (rTerrain.skChunks, 40, 400);
            rTerrain.skChunks.chunk_reserve(400);
            rTerrain.skChunks.shared_reserve(40000);

            rSP.resize(rTerrain.skTerrain);

            float const maxRadius = rTerrainIco.radius + rTerrainIco.height;
            for (SkTriGroupId const groupId : rTerrainIco.icoGroups)
            {
                calc_sphere_tri_center(groupId, rTerrain.skTerrain, maxRadius, rTerrainIco.height);
                rSP.surfaceAdded.set(tri_id(groupId, 0).value);
                rSP.surfaceAdded.set(tri_id(groupId, 1).value);
                rSP.surfaceAdded.set(tri_id(groupId, 2).value);
                rSP.surfaceAdded.set(tri_id(groupId, 3).value);
            }

            // Apply spherical curvature when skeleton triangles are subdivided.
            // (Terrain subdivider is intended to work for non-spherical shapes too)
            rTerrain.scratchpad.onSubdivUserData[0] = &rTerrainIco;

            rSP.onSubdiv = [] (
                    SkTriId                             tri,
                    SkTriGroupId                        groupId,
                    std::array<SkVrtxId, 3>             corners,
                    std::array<MaybeNewId<SkVrtxId>, 3> middles,
                    TerrainSkeleton&                    rTrn,
                    SubdivScratchpad::UserData_t        userData) noexcept
            {
                auto const& rTerrainIco = *reinterpret_cast<ACtxTerrainIco*>(userData[0]);
                ico_calc_middles(rTerrainIco.radius, rTrn.scale, corners, middles, rTrn.skPositions, rTrn.skNormals);
                calc_sphere_tri_center(groupId, rTrn, rTerrainIco.radius + rTerrainIco.height, rTerrainIco.height);
            };

            rSP.onUnsubdiv = [] (
                    SkTriId                         tri,
                    SkeletonTriangle&               rTri,
                    TerrainSkeleton&                skel,
                    SubdivScratchpad::UserData_t    userData) noexcept
            {
            };

            // Calculate distance thresholds
            for (int level = 0; level < rSP.levelMax; ++level)
            {
                // Good-enough bounding sphere is ~75% of the edge length (determined using Blender)
                float const subdivRadius = 0.75f * gc_icoMaxEdgeVsLevel[level] * rTerrainIco.radius * int_2pow<int>(rTerrain.skTerrain.scale);

                // TODO: Pick thresholds based on the angular diameter (size on screen) of the
                //       chunk triangle mesh that will actually be rendered.

                rSP.distanceThresholdSubdiv[level] = std::uint64_t(subdivRadius);

                // Unsubdivide thresholds should be slightly larger (arbitrary x2) to avoid rapid
                // terrain changes when moving back and forth quickly
                rSP.distanceThresholdUnsubdiv[level] = std::uint64_t(2.0f * subdivRadius);
            }
        }
    });

    rBuilder.task()
        .name       ("subdivide triangle skeleton")
        .run_on     ({tgScn.update(Run)})
        .sync_with  ({tgTrn.terrainFrame(Ready), tgTrn.skeleton(New), tgTrn.surfaceChanges(Resize)})
        .push_to    (out.m_tasks)
        .args({                    idTerrainFrame,             idTerrain,                idTerrainIco })
        .func([] (ACtxTerrainFrame &rTerrainFrame, ACtxTerrain &rTerrain, ACtxTerrainIco &rTerrainIco) noexcept
    {
        if ( ! rTerrainFrame.active )
        {
            return;
        }

        TerrainSkeleton  &rSkTrn = rTerrain.skTerrain;
        SubdivScratchpad &rSP    = rTerrain.scratchpad;

        for (int level = rSP.levelMax-1; level >= 0; --level)
        {
            unsubdivide_level_by_distance(level, rTerrainFrame.position, rSkTrn, rSP);
            unsubdivide_level_check_rules(level, rSkTrn, rSP);
            unsubdivide_level(level, rSkTrn, rSP);
        }
        rSP.distanceTestDone.reset();

        debug_check_rules(rSkTrn);

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
            subdivide_level_by_distance(rTerrainFrame.position, level, rSkTrn, rSP);
        }

        rSP.distanceTestDone.reset();

        debug_check_rules(rSkTrn);
    });

    rBuilder.task()
        .name       ("chunk stuff")
        .run_on     ({tgScn.update(Run)})
        .sync_with  ({tgTrn.terrainFrame(Ready), tgTrn.skeleton(New), tgTrn.surfaceChanges(UseOrRun)})
        .push_to    (out.m_tasks)
        .args({                    idTerrainFrame,             idTerrain,                idTerrainIco })
        .func([] (ACtxTerrainFrame &rTerrainFrame, ACtxTerrain &rTerrain, ACtxTerrainIco &rTerrainIco) noexcept
    {
        // TODO: Temporary code
        if ( ! rTerrainFrame.active )
        {
            return;
        }

        SubdivScratchpad &rSP    = rTerrain.scratchpad;
        TerrainSkeleton  &rSkTrn = rTerrain.skTerrain;
        SkeletonChunks   &rSkCh  = rTerrain.skChunks;

//        constexpr int const c_level = 4;
//        constexpr int const c_edgeCount = (1u << c_level) - 1;
//        static std::map<std::size_t, std::array<SkVrtxOwner_t, c_edgeCount*3>> owners;

        for (std::size_t const sktriInt : rSP.surfaceRemoved.ones())
        {
            auto const sktriId = SkTriId(sktriInt);
            ChunkId const chunkId = rSkCh.m_triToChunk[sktriId];
            rSkCh.chunk_remove(chunkId, sktriId, rTerrain.skTerrain.skel);
        }

        rSkCh.m_triToChunk.resize(rTerrain.skTerrain.skel.tri_group_ids().capacity() * 4);

        auto const chLevel = rSkCh.m_chunkSubdivLevel;

        std::vector< MaybeNewId<SkVrtxId> > edgeVrtxs;
        auto const edgeSize = rSkCh.m_chunkEdgeVrtxCount-1;
        edgeVrtxs.resize(edgeSize * 3);

        ArrayView< MaybeNewId<SkVrtxId> > const edgeVrtxView = edgeVrtxs;
        ArrayView< MaybeNewId<SkVrtxId> > const edgeRte = edgeVrtxView.sliceSize(edgeSize * 0, edgeSize);
        ArrayView< MaybeNewId<SkVrtxId> > const edgeBtm = edgeVrtxView.sliceSize(edgeSize * 1, edgeSize);
        ArrayView< MaybeNewId<SkVrtxId> > const edgeLft = edgeVrtxView.sliceSize(edgeSize * 2, edgeSize);

        for (std::size_t const sktriInt : rSP.surfaceAdded.ones())
        {
            auto const sktriId = SkTriId(sktriInt);

            LGRN_ASSERTV(!rSkCh.m_triToChunk[sktriId].has_value(), sktriInt);

            auto const &tri = rSkTrn.skel.tri_at(SkTriId(sktriInt));
            auto const& corners = tri.vertices;

            rSkTrn.skel.vrtx_create_chunk_edge_recurse(chLevel, corners[0], corners[1], edgeRte);
            rSkTrn.skel.vrtx_create_chunk_edge_recurse(chLevel, corners[1], corners[2], edgeBtm);
            rSkTrn.skel.vrtx_create_chunk_edge_recurse(chLevel, corners[2], corners[0], edgeLft);

            rSkCh.chunk_create(rSkTrn.skel, sktriId, edgeRte, edgeBtm, edgeLft);

            rSP.resize(rSkTrn);

            ico_calc_chunk_edge_recurse(rTerrainIco.radius, rSkTrn.scale, chLevel, corners[0], corners[1], edgeRte, rSkTrn.skPositions, rSkTrn.skNormals);
            ico_calc_chunk_edge_recurse(rTerrainIco.radius, rSkTrn.scale, chLevel, corners[1], corners[2], edgeBtm, rSkTrn.skPositions, rSkTrn.skNormals);
            ico_calc_chunk_edge_recurse(rTerrainIco.radius, rSkTrn.scale, chLevel, corners[2], corners[0], edgeLft, rSkTrn.skPositions, rSkTrn.skNormals);
        }

//        for (auto const& [sktriInt, _] : owners)
//        {
//            LGRN_ASSERT(rTerrain.skel.tri_at(SkTriId(sktriInt)).children.has_value() == false);
//        }
    });

    rBuilder.task()
        .name       ("Clear surfaceAdded & surfaceRemoved once we're done with it")
        .run_on     ({tgTrn.surfaceChanges(Clear)})
        .push_to    (out.m_tasks)
        .args({               idTerrain })
        .func([] (ACtxTerrain &rTerrain) noexcept
    {
        rTerrain.scratchpad.surfaceAdded  .reset();
        rTerrain.scratchpad.surfaceRemoved.reset();
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
        .sync_with  ({tgCmCt.camCtrl(Ready), tgTrn.terrainFrame(Modify)})
        .push_to    (out.m_tasks)
        .args       ({                 idCamCtrl,                  idTerrainFrame,             idTerrain,                idTerrainIco })
        .func([] (ACtxCameraController& rCamCtrl, ACtxTerrainFrame &rTerrainFrame, ACtxTerrain &rTerrain, ACtxTerrainIco &rTerrainIco) noexcept
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

        rTerrainFrame.position = Vector3l(camPos * int_2pow<int>(rTerrain.skTerrain.scale));
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

        SubdivIdTree<SkVrtxId> const& vrtxIds = rTerrain.skTerrain.skel.vrtx_ids();
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
                = Matrix4::translation(Vector3(rTerrain.skTerrain.skPositions[skVert]) / int_2pow<int>(rTerrain.skTerrain.scale))
                * Matrix4::scaling({0.05f, 0.05f, 0.05f});
        }
    });

    return out;
}

} // namespace testapp::scenes
