/**
 * Open Space Program
 * Copyright © 2019-2021 Open Space Program Project
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

#include "SubdivSkeleton.h"

using namespace planeta;

SkTriGroupId SubdivTriangleSkeleton::tri_subdiv(SkTriId const triId,
                                                std::array<SkVrtxId, 3> const vrtxMid)
{
    SkeletonTriangle &rTri = tri_at(triId);

    if (rTri.m_children.has_value())
    {
        throw std::runtime_error("SkeletonTriangle is already subdivided");
    }

    SkTriGroup const& parentGroup = m_triData[tri_group_id(triId)];

    auto corner = [&rTri]    (int i) -> SkVrtxId { return rTri.m_vertices[i]; };
    auto middle = [&vrtxMid] (int i) -> SkVrtxId { return vrtxMid[i]; };

    // c?: Corner vertex corner(?) aka: rTri.m_vertices[?]
    // m?: Middle vertex middle(?) aka: vrtxMid[?]
    // t?: Skeleton Triangle
    //
    //          c0
    //          /\                 Vertex order reminder:
    //         /  \                0:Top   1:Left   2:Right
    //        / t0 \                        0
    //    m0 /______\ m2                   / \
    //      /\      /\                    /   \
    //     /  \ t3 /  \                  /     \
    //    / t1 \  / t2 \                1 _____ 2
    //   /______\/______\
    // c1       m1       c2
    //
    SkTriGroupId const groupId = tri_group_create(
        parentGroup.m_depth + 1,
        triId,
        {{
            { corner(0), middle(0), middle(2) }, // 0: Top
            { middle(0), corner(1), middle(1) }, // 1: Left
            { middle(2), middle(1), corner(2) }, // 2: Right
            { middle(1), middle(2), middle(0) }  // 3: Center
        }}
    );

    rTri.m_children = groupId;

    return groupId;
}

//-----------------------------------------------------------------------------


ChunkId SkeletonChunks::chunk_create(
        SubdivTriangleSkeleton&     rSkel,
        SkTriId const               skTri,
        ArrayView_t<SkVrtxId> const edgeRte,
        ArrayView_t<SkVrtxId> const edgeBtm,
        ArrayView_t<SkVrtxId> const edgeLft)
{
    if (   edgeRte.size() != m_chunkWidth - 1
        || edgeBtm.size() != m_chunkWidth - 1
        || edgeLft.size() != m_chunkWidth - 1)
    {
        throw std::runtime_error("Incorrect edge vertex count");
    }

    // Create a new chunk ID
    ChunkId const chunkId = m_chunkIds.create();
    SkTriOwner_t &rChunkTri = m_chunkTris[chunkId];

    rChunkTri = rSkel.tri_store(skTri);

    SkeletonTriangle const &tri = rSkel.tri_at(skTri);

    ArrayView_t<SharedVrtxOwner_t> const sharedSpace = shared_vertices_used(chunkId);

    std::array const edges = {edgeRte, edgeBtm, edgeLft};

    for (int i = 0; i < 3; i ++)
    {
        size_t const cornerOffset = m_chunkWidth * i;
        size_t const edgeOffset = m_chunkWidth * i + 1;

        ArrayView_t<SkVrtxId> const edge = edges[i];
        {
            SharedVrtxId const sharedId = shared_get_or_create(tri.m_vertices[i], rSkel);
            sharedSpace[cornerOffset] = shared_store(sharedId);
        }
        for (unsigned int j = 0; j < m_chunkWidth - 1; j ++)
        {
            SharedVrtxId const sharedId = shared_get_or_create(edge[j], rSkel);
            sharedSpace[edgeOffset + j] = shared_store(sharedId);
        }
    }

    return chunkId;
}



SharedVrtxId SkeletonChunks::shared_get_or_create(SkVrtxId const skVrtxId, SubdivTriangleSkeleton &rSkel)
{
    m_skVrtxToShared.resize(m_sharedIds.capacity());
    SharedVrtxId &rShared = m_skVrtxToShared[skVrtxId];
    if (rShared == lgrn::id_null<SharedVrtxId>())
    {
        rShared = m_sharedIds.create();
        m_sharedFaceCount[rShared] = 0;
        m_sharedSkVrtx[rShared.value] = rSkel.vrtx_store(skVrtxId);
        m_sharedNewlyAdded.push_back(rShared);
    }

    return rShared;
}


void SkeletonChunks::clear(SubdivTriangleSkeleton& rSkel)
{
    // Delete all chunks
    for (std::size_t chunkIdInt : m_chunkIds.bitview().zeros())
    {
        auto const chunkId = ChunkId{static_cast<uint16_t>(chunkIdInt)};

        // Release associated skeleton triangle
        rSkel.tri_release(m_chunkTris[chunkId]);

        // Release all shared vertices
        for (SharedVrtxOwner_t& shared : shared_vertices_used(chunkId))
        {
            shared_release(shared);
        }
    }

    // Delete all shared vertices
    for (uint32_t i = 0; i < m_sharedIds.capacity(); i ++)
    {
        if ( ! m_sharedIds.exists(SharedVrtxId(i)) )
        {
            continue;
        }

        // Release associated skeleton vertex
        rSkel.vrtx_release(m_sharedSkVrtx[i]);
    }
}

