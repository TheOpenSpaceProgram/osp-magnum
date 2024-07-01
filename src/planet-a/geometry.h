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

/**
 * @file
 * @brief Provides types to assign vertex position and normal data to skeletons and chunk mesh
 */
#pragma once

#include "skeleton.h"
#include "chunk_utils.h"

#include <osp/core/math_int64.h>
#include <osp/core/buffer_format.h>

namespace planeta
{

/**
 * @brief Position and normal data for \c SubdivTriangleSkeleton
 */
struct SkeletonVertexData
{
    void resize(SubdivTriangleSkeleton const &rSkel)
    {
        auto const vrtxCapacity = rSkel.vrtx_ids().capacity();
        auto const triCapacity  = rSkel.tri_group_ids().capacity() * 4;

        centers  .resize(triCapacity);
        positions.resize(vrtxCapacity);
        normals  .resize(vrtxCapacity);
    }

    osp::KeyedVec<planeta::SkVrtxId, osp::Vector3l> positions;
    osp::KeyedVec<planeta::SkVrtxId, osp::Vector3>  normals;
    osp::KeyedVec<planeta::SkTriId,  osp::Vector3l> centers;

    /// For the Vector3l variables used in this struct. 2^precision units = 1 meter
    int precision{};
};

/**
 * @brief Contributions to \c BasicChunkMeshGeometry::sharedNormalSum
 *
 * When a chunk is deleted, it needs subtract face normals of all of its deleted faces from all
 * connected shared vertices.
 */
struct FanNormalContrib
{
    SharedVrtxId shared;
    osp::Vector3 sum{osp::ZeroInit};
};

/**
 * @brief Basic float vertex and index buffer for a chunk mesh.
 *
 * To be able to efficiently calculate vertex normals of shared vertices, all triangles connected
 * to shared vertices must add their normal contributions to \c sharedNormalSum, then remove their
 * contributions when deleted.
 */
struct BasicChunkMeshGeometry
{
    void resize(ChunkSkeleton const& skCh, ChunkMeshBufferInfo const& info);

    Corrade::Containers::Array<std::byte>     vrtxBuffer; ///< Output vertex buffer
    Corrade::Containers::Array<osp::Vector3u> indxBuffer; ///< Output index buffer

    osp::BufAttribFormat<osp::Vector3> vbufPositions;   ///< Describes Position data in vrtxBuffer
    osp::BufAttribFormat<osp::Vector3> vbufNormals;     ///< Describes Normal data in vrtxBuffer

    /// Shared vertex positions copied from the skeleton and offsetted with no heightmap applied
    osp::KeyedVec<planeta::SharedVrtxId, osp::Vector3>  sharedPosNoHeightmap;

    /// See \c FanNormalContrib; 2D, each row is \c ChunkMeshBufferInfo::fanMaxSharedCount
    std::vector<planeta::FanNormalContrib>              chunkFanNormalContrib;

    /// 2D, parallel with ChunkSkeleton::m_chunkSharedUsed
    std::vector<osp::Vector3>                           chunkFillSharedNormals;

    /// Non-normalized sum of face normals of connected faces
    osp::KeyedVec<planeta::SharedVrtxId, osp::Vector3>  sharedNormalSum;

    /// Offset of vertex positions relative to the skeleton positions they were copied from
    /// "Chunk Mesh Vertex positions = to_float(skeleton positions + skelOffset)". This is intended
    /// to move the mesh's origin closer to the viewer, preventing floating point imprecision.
    osp::Vector3l originSkelPos{osp::ZeroInit};
};

/**
 * @brief Face writer used for ChunkFanStitcher
 *
 * See \c CFaceWriter
 *
 * TODO: Add vertex angle calculations for more accurate vertex normals. Vertex normals look fine
 *       for the most part, but are actually calculated incorrectly. Face Normals added to
 *       sharedNormalSum should be scaled depending on the vertex angle.
 */
struct TerrainFaceWriter
{
    // 'iterators' used by ArrayView
    using IndxIt_t      = osp::Vector3u*;
    using FanNormalIt_t = osp::Vector3*;
    using ContribIt_t   = FanNormalContrib*;

    void fill_add_face(VertexIdx a, VertexIdx b, VertexIdx c) noexcept
    {
        fan_add_face(a, b, c);
    }

    void fill_add_normal_shared(VertexIdx const vertex, ChunkLocalSharedId const local)
    {
        SharedVrtxId const shared = sharedUsed[local.value];

        fillNormalContrib[local.value]  += selectedFaceNormal;
        sharedNormalSum  [shared.value] += selectedFaceNormal;

        rSharedNormalsDirty.insert(shared);
    }

    void fill_add_normal_filled(VertexIdx const vertex)
    {
        vbufNrm[vertex] += selectedFaceNormal;
    }

    void fan_add_face(VertexIdx a, VertexIdx b, VertexIdx c) noexcept
    {
        calculate_face_normal(a, b, c);

        selectedFaceIndx   = {a, b, c};
        *currentFace = selectedFaceIndx;
        std::advance(currentFace, 1);
    }

    void fan_add_normal_shared(VertexIdx const vertex, SharedVrtxId const shared)
    {
        sharedNormalSum[shared.value] += selectedFaceNormal;

        // Record contributions to shared vertex normal, since this needs to be subtracted when
        // the associated chunk is removed or restitched.

        // Find an existing FanNormalContrib for the given shared vertex.
        // Since each triangle added is in contact with the previous triangle added, we only need
        // to linear-search the previous few (4) contributions added. We also need to consider the
        // first few (4), since the last triangle added will loop around and touch the start,
        // forming a ring of triangles.
        bool found = false;
        FanNormalContrib &rContrib = [this, shared, &found] () -> FanNormalContrib&
        {
            auto const matches = [shared] (FanNormalContrib const& x) noexcept { return x.shared == shared; };

            ContribIt_t const searchAFirst = std::max<ContribIt_t>(std::prev(contribLast, 4), fanNormalContrib.begin());
            ContribIt_t const searchALast  = contribLast;
            ContribIt_t const searchBFirst = fanNormalContrib.begin();
            ContribIt_t const searchBLast  = std::min<ContribIt_t>(std::next(fanNormalContrib.begin(), 4), searchAFirst);

            if (ContribIt_t const foundTemp = std::find_if(searchAFirst, searchALast, matches);
                foundTemp != searchALast)
            {
                found = true;
                return *foundTemp;
            }

            if (ContribIt_t const foundTemp = std::find_if(searchBFirst, searchBLast, matches);
                foundTemp != searchBLast)
            {
                found = true;
                return *foundTemp;
            }

            LGRN_ASSERTM(std::none_of(fanNormalContrib.begin(), contribLast, matches), "search code above is broken XD");

            return *contribLast;
        }();

        if ( ! found )
        {
            rContrib.shared = shared;
            rContrib.sum = osp::Vector3{osp::ZeroInit};
            rSharedNormalsDirty.insert(shared);
            std::advance(contribLast, 1);
            LGRN_ASSERT(contribLast != fanNormalContrib.end());
        }

        rContrib.sum += selectedFaceNormal;
    }

    void calculate_face_normal(VertexIdx a, VertexIdx b, VertexIdx c)
    {
        osp::Vector3 const u = vbufPos[b] - vbufPos[a];
        osp::Vector3 const v = vbufPos[c] - vbufPos[a];
        selectedFaceNormal = Magnum::Math::cross(u, v).normalized();
    }

    osp::BufAttribFormat<osp::Vector3>::ViewConst_t vbufPos;
    osp::BufAttribFormat<osp::Vector3>::View_t      vbufNrm;

    osp::ArrayView<osp::Vector3>        sharedNormalSum;
    osp::ArrayView<osp::Vector3>        fillNormalContrib;
    osp::ArrayView<FanNormalContrib>    fanNormalContrib;
    osp::ArrayView<SharedVrtxOwner_t>   sharedUsed;
    osp::Vector3                        selectedFaceNormal;
    osp::Vector3u                       selectedFaceIndx;
    IndxIt_t                            currentFace;
    ContribIt_t                         contribLast;
    lgrn::IdSetStl<SharedVrtxId>        &rSharedNormalsDirty;
};
static_assert(CFaceWriter<TerrainFaceWriter>, "TerrainFaceWriter must satisfy concept CFaceWriter");



} // namespace planeta
