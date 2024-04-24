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

#include <chrono>
#include <format>
#include <fstream>

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

            rTerrainIco.radius          = 50.0f;
            rTerrainIco.height          = 5.0f;
            rTerrain.skTerrain.scale    = 10;

            rTerrain.skTerrain.skel = create_skeleton_icosahedron(rTerrainIco.radius, rTerrain.skTerrain.scale, rTerrainIco.icoVrtx, rTerrainIco.icoGroups, rTerrainIco.icoTri, rTerrain.skTerrain.positions, rTerrain.skTerrain.normals);

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
                ico_calc_middles(rTerrainIco.radius, rTrn.scale, corners, middles, rTrn.positions, rTrn.normals);
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

            // TODO: just make this dynamically resizable


            constexpr std::uint8_t chunkSubdivLevels = 4;

            rTerrain.skChunks = make_skeleton_chunks(chunkSubdivLevels);
            rTerrain.skChunks.chunk_reserve(300);
            rTerrain.skChunks.shared_reserve(40000);

            std::uint16_t const maxChunks     = rTerrain.skChunks.m_chunkIds.capacity();
            std::uint32_t const maxSharedVrtx = rTerrain.skChunks.m_sharedIds.capacity();

            rTerrain.chunkInfo = make_chunked_mesh_info(rTerrain.skChunks, maxChunks, maxSharedVrtx);
            rTerrain.chunkVbufPos.resize(rTerrain.chunkInfo.vbufSize);
            rTerrain.chunkVbufNrm.resize(rTerrain.chunkInfo.vbufSize);
            rTerrain.chunkIbuf   .resize(rTerrain.chunkInfo.chunkMaxFaceCount * maxChunks);

            rTerrain.chunkSP.lut = make_chunk_vrtx_subdiv_lut(chunkSubdivLevels);
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
        if ( ! rTerrainFrame.active )
        {
            return;
        }

        SubdivScratchpad &rSP    = rTerrain.scratchpad;
        ChunkScratchpad  &rChSP  = rTerrain.chunkSP;
        TerrainSkeleton  &rSkTrn = rTerrain.skTerrain;
        ChunkSkeleton    &rSkCh  = rTerrain.skChunks;

        // Delete chunk IDs
        for (std::size_t const sktriInt : rSP.surfaceRemoved.ones())
        {
            auto const sktriId = SkTriId(sktriInt);
            ChunkId const chunkId = rSkCh.m_triToChunk[sktriId];
            rSkCh.chunk_remove(chunkId, sktriId, rTerrain.skTerrain.skel);
        }

        rChSP.stitchCmds.resize(rSkCh.m_chunkIds.capacity(), {} /* zero init */);

        rSkCh.m_triToChunk.resize(rTerrain.skTerrain.skel.tri_group_ids().capacity() * 4);

        auto const chLevel = rSkCh.m_chunkSubdivLevel;

        std::vector< MaybeNewId<SkVrtxId> > edgeVrtxs;
        auto const edgeSize = rSkCh.m_chunkEdgeVrtxCount-1;
        edgeVrtxs.resize(edgeSize * 3);

        ArrayView< MaybeNewId<SkVrtxId> > const edgeVrtxView = edgeVrtxs;

        ArrayView< MaybeNewId<SkVrtxId> > const edgeLft = edgeVrtxView.sliceSize(edgeSize * 0, edgeSize);
        ArrayView< MaybeNewId<SkVrtxId> > const edgeBtm = edgeVrtxView.sliceSize(edgeSize * 1, edgeSize);
        ArrayView< MaybeNewId<SkVrtxId> > const edgeRte = edgeVrtxView.sliceSize(edgeSize * 2, edgeSize);

        for (std::size_t const sktriInt : rSP.surfaceAdded.ones())
        {
            auto const sktriId = SkTriId::from_index(sktriInt);

            LGRN_ASSERTV(!rSkCh.m_triToChunk[sktriId].has_value(), sktriInt);

            auto const &tri = rSkTrn.skel.tri_at(SkTriId(sktriInt));
            auto const& corners = tri.vertices;

            rSkTrn.skel.vrtx_create_chunk_edge_recurse(chLevel, corners[0], corners[1], edgeLft);
            rSkTrn.skel.vrtx_create_chunk_edge_recurse(chLevel, corners[1], corners[2], edgeBtm);
            rSkTrn.skel.vrtx_create_chunk_edge_recurse(chLevel, corners[2], corners[0], edgeRte);

            ChunkId const chunk = rSkCh.chunk_create(sktriId, rSkTrn.skel, rChSP.sharedNewlyAdded, edgeLft, edgeBtm, edgeRte);

            rChSP.stitchCmds[chunk] = {.enabled = true, .detailX2 = false};

            rSP.resize(rSkTrn);

            ico_calc_chunk_edge_recurse(rTerrainIco.radius, rSkTrn.scale, chLevel, corners[0], corners[1], edgeLft, rSkTrn.positions, rSkTrn.normals);
            ico_calc_chunk_edge_recurse(rTerrainIco.radius, rSkTrn.scale, chLevel, corners[1], corners[2], edgeBtm, rSkTrn.positions, rSkTrn.normals);
            ico_calc_chunk_edge_recurse(rTerrainIco.radius, rSkTrn.scale, chLevel, corners[2], corners[0], edgeRte, rSkTrn.positions, rSkTrn.normals);
        }

        for (std::size_t const sktriInt : rSP.surfaceAdded.ones())
        {
            auto const sktriId = SkTriId::from_index(sktriInt);

            auto const chunk      = rSkCh.m_triToChunk[SkTriId::from_index(sktriInt)];
            auto const& neighbors = rSkTrn.skel.tri_at(sktriId).neighbors;

            for (int selfEdgeIdx = 0; selfEdgeIdx < 3; ++selfEdgeIdx)
            {
                SkTriId const neighborId = neighbors[selfEdgeIdx];
                if ( neighborId.has_value() )
                {

                    ChunkId const neighborChunk = rSkCh.m_triToChunk[neighborId];
                    if ( ! neighborChunk.has_value() )
                    {
                        // Neighbor doesn't have chunk, has chunked children, or hole in terrain
                        SkeletonTriangle const& neighbor = rSkTrn.skel.tri_at(neighborId);

                        if ( ! neighbor.children.has_value() )
                        {
                            continue;
                        }

                        int const neighborEdgeIdx = neighbor.find_neighbor_index(sktriId);

                        bool const neighborHasChunkedChildren
                                =  rSkCh.m_triToChunk[tri_id(neighbor.children, neighborEdgeIdx) ]       .has_value()
                                || rSkCh.m_triToChunk[tri_id(neighbor.children, (neighborEdgeIdx+1) % 3)].has_value();

                        if ( ! neighborHasChunkedChildren )
                        {
                            continue;
                        }

                        rChSP.stitchCmds[chunk] = {
                                .enabled        = true,
                                .detailX2       = true,
                                .x2ownEdge      = static_cast<unsigned char>(selfEdgeIdx),
                                .x2neighborEdge = static_cast<unsigned char>(neighborEdgeIdx)
                        };

                        continue;
                    }
                    // In cases where high-detail chunks were in sktriId's position previously,
                    // but were then unsubdivided and replaced with one low-detail chunk,
                    // remove any detailX2 (low-to-high detail) stitches from neighbors.
                    ChunkStitch &rNeighborStitchCmd = rChSP.stitchCmds[neighborChunk];
                    if (rNeighborStitchCmd.enabled)
                    {
                        continue; // Command already issued by neighbor's neighbor who happens to
                                  // be in surfaceAdded
                    }
                    ChunkStitch const neighborStitch = rSkCh.m_chunkStitch[neighborChunk];
                    if (neighborStitch.enabled && ! neighborStitch.detailX2)
                    {
                        continue; // Neighbor stitch is up-to-date
                    }
                    if (neighborStitch.detailX2 && rSkTrn.skel.tri_at(neighborId).neighbors[neighborStitch.x2ownEdge] != sktriId)
                    {
                        continue; // Neighbor has detailX2 stitch but for an unrelated chunk
                    }

                    rNeighborStitchCmd = { .enabled = true, .detailX2 = false };
                }
                else if (tri_sibling_index(sktriId) != 3)
                {
                    // Check parent's neighbors for lower-detail chunks and make sure they have an
                    // x2detail stitch towards this new chunk.
                    // Sibling 3 triangles are skipped since it's surrounded by its siblings, and
                    // isn't touching any of its parent's neighbors.

                    // Assumes Rule A isn't broken, these don't need checks
                    SkTriId const parent                = rSkTrn.skel.tri_group_at(tri_group_id(sktriId)).parent;
                    SkTriId const parentNeighbor        = rSkTrn.skel.tri_at(parent).neighbors[selfEdgeIdx];
                    ChunkId const parentNeighborChunk   = rSkCh.m_triToChunk[parentNeighbor];
                    int     const neighborEdge          = rSkTrn.skel.tri_at(parentNeighbor).find_neighbor_index(parent);

                    ChunkStitch const desiredStitch =  {
                            .enabled        = true,
                            .detailX2       = true,
                            .x2ownEdge      = static_cast<unsigned char>(neighborEdge),
                            .x2neighborEdge = static_cast<unsigned char>(selfEdgeIdx)
                    };

                    ChunkStitch &rStitchCmd = rChSP.stitchCmds[parentNeighborChunk];
                    LGRN_ASSERT(   (rStitchCmd.enabled == false)
                                || (rStitchCmd.detailX2 == false)
                                || (rStitchCmd == desiredStitch));
                    rStitchCmd = desiredStitch;
                }
            }
        }

        ChunkedTriangleMeshInfo &rChInfo = rTerrain.chunkInfo;

        auto const to_vrtx = [offset = rChInfo.vbufSharedOffset] (SharedVrtxId in)
        {
            return VertexIdx( offset + std::uint32_t(in) );
        };

        // Calculate shared vertex positions
        float const scalepow = std::pow(2.0f, -rSkTrn.scale);
        for (SharedVrtxId sharedVrtx : rChSP.sharedNewlyAdded)
        {
            SkVrtxId const skelVrtx = rSkCh.m_sharedToSkVrtx[sharedVrtx];

            //Vector3l const translated = positions[size_t(skelId)] + translaton;
            auto const scaled = Vector3d(rTerrain.skTerrain.positions[skelVrtx]) * scalepow;

            rTerrain.chunkVbufPos[to_vrtx(sharedVrtx)] = Vector3(scaled);
        }
        rChSP.sharedNewlyAdded.clear();

        // Calculate fill vertex positions
        for (std::size_t const sktriInt : rSP.surfaceAdded.ones())
        {
            auto const chunk      = rSkCh.m_triToChunk[SkTriId::from_index(sktriInt)];
            auto const chunkIdInt = std::size_t(chunk);

            std::size_t const fillOffset = rChInfo.vbufFillOffset + chunkIdInt*rChInfo.fillVrtxCount;

            osp::ArrayView<SharedVrtxOwner_t const> sharedUsed = rTerrain.skChunks.shared_vertices_used(chunk);

            for (ChunkVrtxSubdivLUT::ToSubdiv const& toSubdiv : rChSP.lut.data())
            {
                std::size_t const vrtxA = rChSP.lut.index(sharedUsed, fillOffset, rChInfo.vbufSharedOffset, toSubdiv.m_vrtxA);
                std::size_t const vrtxB = rChSP.lut.index(sharedUsed, fillOffset, rChInfo.vbufSharedOffset, toSubdiv.m_vrtxB);

                std::size_t const vrtxC = rChInfo.vbufFillOffset + rChInfo.fillVrtxCount*chunkIdInt + std::uint32_t(toSubdiv.m_fillOut);

                Vector3 &rPosC = rTerrain.chunkVbufPos[vrtxC];
                //Vector3 &rNormC = rTerrain.chunkVbufNrm[vrtxC];

                // Heightmap goes here
                Vector3 const avg = (rTerrain.chunkVbufPos[vrtxA] + rTerrain.chunkVbufPos[vrtxB]) / 2.0f;
                float const avgLen = avg.length();
                float const roundness = rTerrainIco.radius - avgLen;

                rPosC = avg + (avg / avgLen) * roundness;
            }
        }

        // Fill index buffer (connect the dots with vertices)
        for (unsigned int chunkInt = 0; chunkInt < rSkCh.m_chunkIds.capacity(); ++chunkInt)
        {
            auto const chunkId = ChunkId::from_index(chunkInt);

            if ( ! rSkCh.m_chunkIds.exists(chunkId) )
            {
                continue;
            }

            auto const firstFace = rTerrain.chunkIbuf.begin() + (chunkInt * rChInfo.chunkMaxFaceCount);
            auto const lastFace  = firstFace + rChInfo.chunkMaxFaceCount;

            auto currentFace = firstFace;

            auto const emit_face = [&currentFace, &lastFace] (std::uint32_t a, std::uint32_t b, std::uint32_t c)
            {
                LGRN_ASSERTM(currentFace != lastFace, "Max faces exceeded");
                *currentFace = {a, b, c};
                ++currentFace;
            };

            bool const newlyAdded = rSP.surfaceAdded.test(rSkCh.m_chunkToTri[chunkId].value);

            if (newlyAdded)
            {
                for (unsigned int y = 0; y < rSkCh.m_chunkEdgeVrtxCount; y ++)
                for (unsigned int x = 0; x < y * 2 + 1; x ++)
                {
                    // alternate between up-pointing and down-pointing triangles
                    bool const upPointing = (x % 2 == 0);
                    bool const onEdge = (x == 0) || (x == y * 2)
                                        || (upPointing && y == rSkCh.m_chunkEdgeVrtxCount - 1);

                    auto const indx
                        = upPointing
                        ? Vector3u{ chunk_coord_to_vrtx(rSkCh, rChInfo, chunkId, x / 2,      y       ),
                                    chunk_coord_to_vrtx(rSkCh, rChInfo, chunkId, x / 2,      y + 1   ),
                                    chunk_coord_to_vrtx(rSkCh, rChInfo, chunkId, x / 2 + 1,  y + 1   ) }
                        : Vector3u{ chunk_coord_to_vrtx(rSkCh, rChInfo, chunkId, x / 2 + 1,  y + 1   ),
                                    chunk_coord_to_vrtx(rSkCh, rChInfo, chunkId, x / 2 + 1,  y       ),
                                    chunk_coord_to_vrtx(rSkCh, rChInfo, chunkId, x / 2,      y       ) };

                    // Calculate face normal
                    Vector3 const u = rTerrain.chunkVbufPos[indx.y()] - rTerrain.chunkVbufPos[indx.x()];
                    Vector3 const v = rTerrain.chunkVbufPos[indx.z()] - rTerrain.chunkVbufPos[indx.x()];
                    Vector3 const faceNorm = Magnum::Math::cross(u, v).normalized();

                    for (int i = 0; i < 3; i ++)
                    {
                        Vector3 &rVrtxNorm = rTerrain.chunkVbufNrm[indx[i]];

                        if (rChInfo.is_vertex_shared(indx[i]))
                        {
                            if ( ! onEdge)
                            {
                                // Shared vertices can have a variable number of
                                // connected faces

                                SharedVrtxId const shared   = rChInfo.vertex_to_shared(indx[i]);
                                uint8_t &rFaceCount         = rSkCh.m_sharedFaceCount[shared];

                                // Add new face normal to the average
                                rVrtxNorm = (rVrtxNorm * rFaceCount + faceNorm) / (rFaceCount + 1);
                                rFaceCount ++;

                            }
                            // edge triangles handled by fans, and are left empty
                        }
                        else
                        {
                            // All fill vertices have 6 connected faces
                            // (just look at a picture of a triangular tiling)

                            // Fans with multiple triangles may be connected to a fill
                            // vertex, but the normals are calculated as if there was
                            // only one triangle to (potentially) improve blending

                            rVrtxNorm += faceNorm / 6.0f;
                        }
                    }

                    if ( ! onEdge )
                    {
                        emit_face(indx[0], indx[1], indx[2]);
                    }
                }

                LGRN_ASSERT(currentFace == (firstFace + rChInfo.fillFaceCount));
            }

            ChunkStitch const cmd = rChSP.stitchCmds[chunkId];

            if (cmd.enabled)
            {
                rSkCh.m_chunkStitch[chunkId] = cmd;

                currentFace = firstFace + rChInfo.fillFaceCount;

                auto stitcher = make_chunk_fan_stitcher(chunkId, std::nullopt, rSkCh, rChInfo, emit_face);

                using enum ECornerDetailX2;

                if (cmd.detailX2)
                {
                    switch (cmd.x2ownEdge)
                    {
                    case 0:
                        stitcher.corner <0, Left >();
                        stitcher.edge   <0, true >();
                        stitcher.corner <1, Right>();
                        stitcher.edge   <1, false>();
                        stitcher.corner <2, None >();
                        stitcher.edge   <2, false>();
                        break;
                    case 1:
                        stitcher.corner <0, None >();
                        stitcher.edge   <0, false>();
                        stitcher.corner <1, Left >();
                        stitcher.edge   <1, true >();
                        stitcher.corner <2, Right>();
                        stitcher.edge   <2, false>();
                        break;
                    case 2:
                        stitcher.corner <0, Right>();
                        stitcher.edge   <0, false>();
                        stitcher.corner <1, None >();
                        stitcher.edge   <1, false>();
                        stitcher.corner <2, Left >();
                        stitcher.edge   <2, true >();
                        break;
                    }
                }
                else
                {
                    stitcher.corner <0, None>();
                    stitcher.edge   <0, false>();
                    stitcher.corner <1, None>();
                    stitcher.edge   <1, false>();
                    stitcher.corner <2, None>();
                    stitcher.edge   <2, false>();
                }

                std::fill(currentFace, lastFace, Vector3u{0, 0, 0});
            }
        }
        std::fill(rChSP.stitchCmds.begin(), rChSP.stitchCmds.end(), ChunkStitch{});


        // TODO: temporary, write debug obj file every ~10 seconds
        static int fish = 0;
        if (fish == 60*10)
        {
            auto const chunkCount = rSkCh.m_chunkIds.size();
            auto const sharedCount = rSkCh.m_sharedIds.size();

            std::ofstream objfile;
            auto time = std::chrono::system_clock::now().time_since_epoch().count();

            std::string const filename = std::format("planetdebug_{}.obj", time);
            OSP_LOG_INFO("Writing planet terrain obj: {}\n"
                         "* Chunks:          {}/{}\n"
                         "* Shared Vertices: {}/{}\n",
                         filename,
                         chunkCount, rSkCh.m_chunkIds.capacity(),
                         sharedCount, rSkCh.m_sharedIds.capacity() );

            objfile.open(filename);

            objfile << "# Terrain mesh debug output\n"
                    << "# Chunks: "          << chunkCount  << "/" << rSkCh.m_chunkIds.capacity() << "\n"
                    << "# Shared Vertices: " << sharedCount << "/" << rSkCh.m_sharedIds.capacity() << "\n";

            objfile << "o Planet\n";

            for (Vector3 v : rTerrain.chunkVbufPos)
            {
                objfile << "v " << v.x() << " " << v.y() << " "  << v.z() << "\n";
            }

            for (Vector3 v : rTerrain.chunkVbufNrm)
            {
                objfile << "vn " << v.x() << " " << v.y() << " "  << v.z() << "\n";
            }


            for (std::size_t const chunkIdInt : rSkCh.m_chunkIds.bitview().zeros())
            {
                auto const view = arrayView(rTerrain.chunkIbuf).sliceSize(chunkIdInt * rChInfo.chunkMaxFaceCount, rChInfo.chunkMaxFaceCount);

                for (Vector3u const faceIdx : view)
                {
                    // Indexes start at 1 for .obj files
                    // Format: "f vertex1//normal1 vertex2//normal2 vertex3//normal3"
                    objfile << "f "
                            << (faceIdx.x()+1) << "//" << (faceIdx.x()+1) << " "
                            << (faceIdx.y()+1) << "//" << (faceIdx.y()+1) << " "
                            << (faceIdx.z()+1) << "//" << (faceIdx.z()+1) << "\n";
                }
            }

            fish = 0;
        }
        ++fish;


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
                = Matrix4::translation(Vector3(rTerrain.skTerrain.positions[skVert]) / int_2pow<int>(rTerrain.skTerrain.scale))
                * Matrix4::scaling({0.05f, 0.05f, 0.05f});
        }
    });

    return out;
}

} // namespace testapp::scenes
