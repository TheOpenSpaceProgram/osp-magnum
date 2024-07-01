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
#include "chunk_generate.h"

#include <Corrade/Containers/ArrayViewStl.h>

#include <ostream>

using osp::ArrayView;
using osp::Vector3;
using osp::Vector3u;
using osp::Vector3l;
using osp::ZeroInit;
using osp::arrayView;
using osp::as_2d;

namespace planeta
{

void ChunkScratchpad::resize(ChunkSkeleton const& rChSk)
{
    auto const maxSharedVrtx = rChSk.m_sharedIds.capacity();
    auto const maxChunks     = rChSk.m_chunkIds.capacity();

    edgeVertices        .resize(std::size_t(rChSk.m_chunkEdgeVrtxCount - 1) * 3);
    stitchCmds          .resize(maxChunks, {});
    chunksAdded         .resize(maxChunks);
    chunksRemoved       .resize(maxChunks);
    sharedAdded         .resize(maxSharedVrtx);
    sharedRemoved       .resize(maxSharedVrtx);
    sharedNormalsDirty  .resize(maxSharedVrtx);
}

void restitch_check(
        ChunkId                   const chunkId,
        SkTriId                   const sktriId,
        ChunkSkeleton             const &rSkCh,
        SubdivTriangleSkeleton    const &rSkel,
        SkeletonVertexData        const &rSkData,
        ChunkScratchpad                 &rChSP)
{
    ChunkStitch ownCmd = {.enabled = true, .detailX2 = false};

    auto const& neighbors = rSkel.tri_at(sktriId).neighbors;

    for (int selfEdgeIdx = 0; selfEdgeIdx < 3; ++selfEdgeIdx)
    {
        SkTriId const neighborId = neighbors[selfEdgeIdx];
        if (neighborId.has_value())
        {
            ChunkId const neighborChunk = rSkCh.m_triToChunk[neighborId];
            if (neighborChunk.has_value())
            {
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
                if (neighborStitch.detailX2 && rSkel.tri_at(neighborId).neighbors[neighborStitch.x2ownEdge] != sktriId)
                {
                    continue; // Neighbor has detailX2 stitch but for an unrelated chunk
                }

                rNeighborStitchCmd = { .enabled = true, .detailX2 = false };
            }
            else
            {
                // Neighbor doesn't have chunk. It is either a hole in the terrain, or it has
                // chunked children which requires a detailX2 (low-to-high detail) stitch.
                SkeletonTriangle const& neighbor = rSkel.tri_at(neighborId);

                if ( ! neighbor.children.has_value() )
                {
                    continue; // Hole in terrain
                }

                int  const neighborEdgeIdx = neighbor.find_neighbor_index(sktriId);
                ChunkId const childA = rSkCh.m_triToChunk[tri_id(neighbor.children, neighborEdgeIdx) ];
                ChunkId const childB = rSkCh.m_triToChunk[tri_id(neighbor.children, (neighborEdgeIdx+1) % 3)];

                if ( ! (childA.has_value() && childB.has_value()) )
                {
                    continue; // Both neighboring children are holes in the terrain
                }

                // Remove DetailX2 stitch from any of the children, in rare cases where there was
                // previously a much higher detail chunk in sktriId's position.

                // Just Repeating Yourself Already (JRYA). what, you want me to write a lambda?

                ChunkStitch const childStitchA     = rSkCh.m_chunkStitch[childA];
                ChunkStitch const childStitchB     = rSkCh.m_chunkStitch[childB];
                ChunkStitch       &childStitchACmd = rChSP.stitchCmds[childA];
                ChunkStitch       &childStitchBCmd = rChSP.stitchCmds[childB];

                if ( ! childStitchACmd.enabled && childStitchA.detailX2
                    && childStitchA.x2ownEdge == neighborEdgeIdx)
                {
                    childStitchACmd = {.enabled = true, .detailX2 = false};
                }

                if ( ! childStitchBCmd.enabled && childStitchB.detailX2
                    && childStitchB.x2ownEdge == neighborEdgeIdx)
                {
                    childStitchBCmd = {.enabled = true, .detailX2 = false};
                }

                ownCmd = {
                        .enabled        = true,
                        .detailX2       = true,
                        .x2ownEdge      = static_cast<unsigned char>(selfEdgeIdx),
                        .x2neighborEdge = static_cast<unsigned char>(neighborEdgeIdx)
                };
            }
        }
        else if (tri_sibling_index(sktriId) != 3)
        {
            // Check parent's neighbors for lower-detail chunks and make sure they have an
            // x2detail stitch towards this new chunk.
            // Sibling 3 triangles are skipped since it's surrounded by its siblings, and
            // isn't touching any of its parent's neighbors.

            // Assumes Invariant A isn't broken, these don't need checks
            SkTriId const parent                = rSkel.tri_group_at(tri_group_id(sktriId)).parent;
            SkTriId const parentNeighbor        = rSkel.tri_at(parent).neighbors[selfEdgeIdx];
            ChunkId const parentNeighborChunk   = rSkCh.m_triToChunk[parentNeighbor];
            int     const neighborEdge          = rSkel.tri_at(parentNeighbor).find_neighbor_index(parent);

            if (parentNeighborChunk.has_value())
            {
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
            // else, hole in terrain
        }
    }

    rChSP.stitchCmds[chunkId] = ownCmd;
}


void update_faces(
        ChunkId                const chunkId,
        SkTriId                const sktriId,
        bool                   const newlyAdded,
        SubdivTriangleSkeleton       &rSkel,
        SkeletonVertexData     const &rSkData,
        BasicChunkMeshGeometry       &rGeom,
        ChunkMeshBufferInfo    const &rChInfo,
        ChunkScratchpad              &rChSP,
        ChunkSkeleton                &rSkCh)
{
    ChunkStitch const cmd = rChSP.stitchCmds[chunkId];

    if ( ! newlyAdded && ! cmd.enabled )
    {
        return; // Nothing to do
    }

    auto const vbufNormalsView   = rGeom.vbufNormals.view(rGeom.vrtxBuffer, rChInfo.vrtxTotal);
    auto const ibufSlice         = as_2d(rGeom.indxBuffer,             rChInfo.chunkMaxFaceCount).row(chunkId.value);
    auto const fanNormalContrib  = as_2d(rGeom.chunkFanNormalContrib,  rChInfo.fanMaxSharedCount).row(chunkId.value);
    auto const fillNormalContrib = as_2d(rGeom.chunkFillSharedNormals, rSkCh.m_chunkSharedCount) .row(chunkId.value);
    auto const sharedUsed        = rSkCh.shared_vertices_used(chunkId);

    TerrainFaceWriter writer{
        .vbufPos             = rGeom.vbufPositions.view_const(rGeom.vrtxBuffer, rChInfo.vrtxTotal),
        .vbufNrm             = vbufNormalsView,
        .sharedNormalSum     = rGeom.sharedNormalSum.base(),
        .fillNormalContrib   = fillNormalContrib,
        .fanNormalContrib    = fanNormalContrib,
        .sharedUsed          = sharedUsed,
        .currentFace         = ibufSlice.begin(),
        .contribLast         = fanNormalContrib.begin(),
        .rSharedNormalsDirty = rChSP.sharedNormalsDirty
    };

    // Create triangle fill for newly added triangles
    if (newlyAdded)
    {
        // Reset fill normals to zero, as values are left over from a previously deleted chunk
        auto const chunkVbufFillNormals2D = as_2d(vbufNormalsView.exceptPrefix(rChInfo.vbufFillOffset), rChInfo.fillVrtxCount);
        auto const vbufFillNormals        = chunkVbufFillNormals2D.row(chunkId.value);

        // These aren't cleaned up by the previous chunk that used them
        std::fill(vbufFillNormals  .begin(), vbufFillNormals  .end(), Vector3{ZeroInit});
        std::fill(fillNormalContrib.begin(), fillNormalContrib.end(), Vector3{ZeroInit});
        std::fill(fanNormalContrib .begin(), fanNormalContrib .end(), FanNormalContrib{});

        auto const add_fill_tri = [&rSkCh, &rChInfo, &writer, chunkId]
                (std::uint16_t const aX, std::uint16_t const aY,
                 std::uint16_t const bX, std::uint16_t const bY,
                 std::uint16_t const cX, std::uint16_t const cY)
        {
            auto const [shLocalA, vrtxA] = chunk_coord_to_vrtx(rSkCh, rChInfo, chunkId, aX, aY);
            auto const [shLocalB, vrtxB] = chunk_coord_to_vrtx(rSkCh, rChInfo, chunkId, bX, bY);
            auto const [shLocalC, vrtxC] = chunk_coord_to_vrtx(rSkCh, rChInfo, chunkId, cX, cY);

            writer.fill_add_face(vrtxA, vrtxB, vrtxC);

            shLocalA.has_value() ? writer.fill_add_normal_shared(vrtxA, shLocalA)
                                 : writer.fill_add_normal_filled(vrtxA);
            shLocalB.has_value() ? writer.fill_add_normal_shared(vrtxB, shLocalB)
                                 : writer.fill_add_normal_filled(vrtxB);
            shLocalC.has_value() ? writer.fill_add_normal_shared(vrtxC, shLocalC)
                                 : writer.fill_add_normal_filled(vrtxC);
        };

        for (unsigned int y = 0; y < rSkCh.m_chunkEdgeVrtxCount; ++y)
        {
            for (unsigned int x = 0; x < y; ++x)
            {
                // down-pointing
                //                ( aX   aY )    ( aX   aY )    ( aX   aY )
                add_fill_tri(      x+1, y+1,      x+1,  y,         x,  y      );

                // up pointing
                bool const onEdge = (x == y-1) || y == rSkCh.m_chunkEdgeVrtxCount - 1;
                if ( ! onEdge )
                {
                    //                ( aX   aY )    ( aX   aY )    ( aX   aY )
                    add_fill_tri(      x+1,  y,       x+1,  y+1,     x+2,  y+1   );
                }
            }
        }

        LGRN_ASSERTM(writer.currentFace == std::next(ibufSlice.begin(), rChInfo.fillFaceCount),
                     "Code above must always add a known number of faces");

        for (Vector3 &rNormal : vbufFillNormals)
        {
            rNormal = rNormal.normalized();
        }
    }

    writer.currentFace = std::next(ibufSlice.begin(), rChInfo.fillFaceCount);

    // Add or replace Fan triangles
    if (cmd.enabled)
    {
        ChunkStitch &rCurrentStitch = rSkCh.m_chunkStitch[chunkId];
        if (rCurrentStitch.enabled)
        {
            // Delete previous fan stitch, Subtract normal contributions
            subtract_normal_contrib(chunkId, true, rGeom, rChInfo, rChSP, rSkCh);
        }
        rSkCh.m_chunkStitch[chunkId] = cmd;
        ArrayView<SharedVrtxOwner_t const> detailX2Edge0;
        ArrayView<SharedVrtxOwner_t const> detailX2Edge1;

        // For detailX2 stitches, get the 2 neighboring higher detail triangles,
        // and get the rows of shared vertices along the edge in contact.
        if (cmd.detailX2)
        {
            SkTriId          const  neighborId = rSkel.tri_at(sktriId).neighbors[cmd.x2ownEdge];
            SkeletonTriangle const& neighbor   = rSkel.tri_at(neighborId);

            auto const child_chunk_edge = [&rSkCh, children = neighbor.children, edgeIdx = std::uint32_t(cmd.x2neighborEdge)]
                                          (std::uint8_t siblingIdx) -> ArrayView<SharedVrtxOwner_t const>
            {
                ChunkId const chunk = rSkCh.m_triToChunk[tri_id(children, siblingIdx)];
                return as_2d(rSkCh.shared_vertices_used(chunk), rSkCh.m_chunkEdgeVrtxCount).row(edgeIdx);
            };

            detailX2Edge0 = child_chunk_edge(cmd.x2neighborEdge);
            detailX2Edge1 = child_chunk_edge((cmd.x2neighborEdge + 1) % 3);
        }

        auto const stitcher = make_chunk_fan_stitcher<TerrainFaceWriter&>(writer, chunkId, detailX2Edge0, detailX2Edge1, rSkCh, rChInfo);

        stitcher.stitch(cmd);
    }

    // Fill remaining with zeros to indicate an early end if the full range isn't used
    std::fill(writer.currentFace, ibufSlice.end(), Vector3u{ZeroInit});

}

void subtract_normal_contrib(
        ChunkId                       const chunkId,
        bool                          const onlySubtractFans,
        BasicChunkMeshGeometry              &rGeom,
        ChunkMeshBufferInfo           const &rChInfo,
        ChunkScratchpad                     &rChSP,
        ChunkSkeleton                 const &rSkCh)
{
    // Subtract Fan shared vertex normal contributions

    auto const chunkFanNormalContrib2D = as_2d(arrayView(rGeom.chunkFanNormalContrib), rChInfo.fanMaxSharedCount);
    auto const fanNormalContrib        = chunkFanNormalContrib2D.row(chunkId.value);

    LGRN_ASSERT(rSkCh.m_chunkStitch[chunkId].enabled);
    for (FanNormalContrib &rContrib : fanNormalContrib)
    {
        if ( ! rContrib.shared.has_value() )
        {
            break;
        }

        if (rSkCh.m_sharedIds.exists(rContrib.shared) && ! rChSP.sharedRemoved.contains(rContrib.shared))
        {
            osp::Vector3 &rNormal = rGeom.sharedNormalSum[rContrib.shared];
            rNormal -= rContrib.sum;
            rChSP.sharedNormalsDirty.insert(rContrib.shared);
        }
        rContrib.sum *= 0.0f;
    }

    if ( ! onlySubtractFans )
    {
        // Subtract Fill shared vertex normal contributions
        auto const chunkFillNormalContrib2D = as_2d(arrayView(rGeom.chunkFillSharedNormals), rSkCh.m_chunkSharedCount);
        auto const fillNormalContrib        = chunkFillNormalContrib2D.row(chunkId.value);

        auto const sharedUsed = rSkCh.shared_vertices_used(chunkId);

        for (std::size_t i = 0; i < sharedUsed.size(); ++i)
        {
            SharedVrtxId const shared = sharedUsed[i].value();
            if ( ! shared.has_value() )
            {
                break;
            }

            if (rSkCh.m_sharedIds.exists(shared) && ! rChSP.sharedRemoved.contains(shared))
            {
                Vector3 const &contrib = fillNormalContrib[i];
                Vector3       &rNormal = rGeom.sharedNormalSum[shared];
                rNormal -= contrib;
                rChSP.sharedNormalsDirty.insert(shared);
            }
            fillNormalContrib[i] *= 0.0f;
        }
    }
}

void debug_check_invariants(
        BasicChunkMeshGeometry        const &rGeom,
        ChunkMeshBufferInfo           const &rChInfo,
        ChunkSkeleton                 const &rSkCh)
{
    auto const vbufNormalsView = rGeom.vbufNormals.view_const(rGeom.vrtxBuffer, rChInfo.vrtxTotal);

    auto const check_vertex = [&] (VertexIdx vertex, SharedVrtxId sharedId, ChunkId chunkId)
    {
        osp::Vector3 const normal = vbufNormalsView[vertex];
        float   const length = normal.length();

        LGRN_ASSERTMV(std::abs(length - 1.0f) < 0.05f, "Normal isn't normalized", length, vertex, sharedId.value, chunkId.value);
    };

    for (std::size_t const sharedInt : rSkCh.m_sharedIds.bitview().zeros())
    {
        check_vertex(rChInfo.vbufSharedOffset + sharedInt, SharedVrtxId(sharedInt), {});
    }

    for (std::size_t const chunkInt : rSkCh.m_chunkIds.bitview().zeros())
    {
        VertexIdx const first = rChInfo.vbufFillOffset + chunkInt * rChInfo.fillVrtxCount;
        VertexIdx const last  = first + rChInfo.fillVrtxCount;

        for (VertexIdx vertex = first; vertex < last; ++vertex)
        {
            check_vertex(vertex, {}, ChunkId(chunkInt));
        }
    }
}

void write_obj(
        std::ostream                        &rStream,
        BasicChunkMeshGeometry        const &rGeom,
        ChunkMeshBufferInfo           const &rChInfo,
        ChunkSkeleton                 const &rSkCh)
{
    auto const chunkCount  = rSkCh.m_chunkIds.size();
    auto const sharedCount = rSkCh.m_sharedIds.size();

    auto const vbufPosView = rGeom.vbufPositions.view_const(rGeom.vrtxBuffer, rChInfo.vrtxTotal);
    auto const vbufNrmView = rGeom.vbufNormals  .view_const(rGeom.vrtxBuffer, rChInfo.vrtxTotal);


    rStream << "# Terrain mesh debug output\n"
            << "# Chunks: "          << chunkCount  << "/" << rSkCh.m_chunkIds.capacity() << "\n"
            << "# Shared Vertices: " << sharedCount << "/" << rSkCh.m_sharedIds.capacity() << "\n";

    rStream << "o Planet\n";

    for (osp::Vector3 v : vbufPosView)
    {
        rStream << "v " << v.x() << " " << v.y() << " "  << v.z() << "\n";
    }

    for (osp::Vector3 v : vbufNrmView)
    {
        rStream << "vn " << v.x() << " " << v.y() << " "  << v.z() << "\n";
    }

    for (std::size_t const chunkIdInt : rSkCh.m_chunkIds.bitview().zeros())
    {
        auto const view = osp::as_2d(rGeom.indxBuffer, rChInfo.chunkMaxFaceCount).row(chunkIdInt);

        for (osp::Vector3u const faceIdx : view)
        {
            // Indexes start at 1 for .obj files
            // Format: "f vertex1//normal1 vertex2//normal2 vertex3//normal3"
            rStream << "f "
                    << (faceIdx.x()+1) << "//" << (faceIdx.x()+1) << " "
                    << (faceIdx.y()+1) << "//" << (faceIdx.y()+1) << " "
                    << (faceIdx.z()+1) << "//" << (faceIdx.z()+1) << "\n";
        }
    }
}


} // namespace planeta
