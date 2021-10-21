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

#include "SubdivTriangleMesh.h"

#include <Magnum/Math/Vector3.h>

using namespace planeta;



ChunkedTriangleMesh planeta::make_subdivtrimesh_general(
        unsigned int chunkMax, unsigned int subdivLevels,
        unsigned int vrtxSize, int pow2scale)
{
    // calculate stuff here

    using std::sqrt;
    using std::ceil;
    using std::pow;

    float const C = chunkMax;
    float const L = subdivLevels;

    // Calculate a fair number of shared vertices, based on a triangular
    // tiling pattern.
    // worked out here: https://www.desmos.com/calculator/ffd8lraosl
    unsigned int const sharedMax
            = ceil( ( 3.0f*C + sqrt(6.0f)*sqrt(C) ) * pow(2.0f, L-1) - C + 1 );

    unsigned int const fanMax
            = ChunkedTriangleMesh::smc_minFansVsLevel[subdivLevels] * 2;

    return ChunkedTriangleMesh(
                chunkMax, subdivLevels, sharedMax, vrtxSize, fanMax);
}

ChunkId ChunkedTriangleMesh::chunk_create(
        SubdivTriangleSkeleton rSkel,
        SkTriId skTri,
        ArrayView_t<SkVrtxId> const edgeRte,
        ArrayView_t<SkVrtxId> const edgeBtm,
        ArrayView_t<SkVrtxId> const edgeLft)
{
    // Create a new chunk ID
    ChunkId const chunkId = m_chunkIds.create();

    // Increment ref count of triangle
    rSkel.tri_refcount_add(skTri);

    if (   edgeRte.size() != m_chunkEdgeVrtxCount - 1
        || edgeBtm.size() != m_chunkEdgeVrtxCount - 1
        || edgeLft.size() != m_chunkEdgeVrtxCount - 1)
    {
        throw std::runtime_error("Incorrect edge vertex count");
    }

    SkeletonTriangle const &tri = rSkel.tri_at(skTri);

    ArrayView_t<SharedVrtxId> const sharedSpace = chunk_shared(chunkId);

    std::array const edges = {edgeRte, edgeBtm, edgeLft};

    for (int i = 0; i < 3; i ++)
    {
        size_t const cornerOffset = m_chunkEdgeVrtxCount * i;
        size_t const edgeOffset = m_chunkEdgeVrtxCount * i + 1;

        ArrayView_t<SkVrtxId> const edge = edges[i];
        {
            SharedVrtxId const shared = shared_get_or_create(tri.m_vertices[i]);
            sharedSpace[cornerOffset] = shared;
            shared_refcount_add(shared);
        }
        for (unsigned int j = 0; j < m_chunkEdgeVrtxCount - 1; j ++)
        {
            SharedVrtxId const shared = shared_get_or_create(edge[j]);
            sharedSpace[edgeOffset + j] = shared;
            shared_refcount_add(shared);
        }
    }

    return chunkId;
}

VertexId ChunkedTriangleMesh::chunk_coord_to_mesh(
        ChunkId chunkId, uint16_t x, uint16_t y) const noexcept
{


    if (auto const [localId, shared] = try_get_shared(x, y, m_chunkEdgeVrtxCount);
        shared)
    {
        return shared_get_vrtx(chunk_shared(chunkId)[size_t(localId)]);
    }

    // Center, non-shared chunk vertex
    return VertexId(m_chunkVrtxFillCount * size_t(chunkId) + xy_to_triangular(x - 1, y - 2));
}

//-----------------------------------------------------------------------------

ChunkVrtxSubdivLUT::ChunkVrtxSubdivLUT(uint8_t subdivLevel)
{
    m_edgeVrtxCount = 1u << subdivLevel;
    m_fillVrtxCount = (m_edgeVrtxCount-2) * (m_edgeVrtxCount-1) / 2;

    m_data.reserve(m_fillVrtxCount);

    // Calculate LUT, this fills m_data
    fill_tri_recurse({0,               0              },
                     {0,               m_edgeVrtxCount},
                     {m_edgeVrtxCount, m_edgeVrtxCount},
                     subdivLevel);

    // Future optimization:
    // m_data can be sorted in a way that slightly improves cache locality
    // by accessing to fill vertices in a more sequential order.
}

void ChunkVrtxSubdivLUT::subdiv_line_recurse(
        Vector2us a, Vector2us b, uint8_t level)
{
    Vector2us const mid = (a + b) / 2;

    ChunkLocalSharedId out = ChunkLocalSharedId(xy_to_triangular(mid.x() - 1, mid.y() - 2));

    m_data.emplace_back(ToSubdiv{id_at(a), id_at(b), out});

    if (level > 1)
    {
        subdiv_line_recurse(a, mid, level - 1);
        subdiv_line_recurse(mid, b, level - 1);
    }

}

void ChunkVrtxSubdivLUT::fill_tri_recurse(
        Vector2us top, Vector2us lft, Vector2us rte, uint8_t level)
{
    // calculate midpoints
    std::array<Vector2us, 3> mid{{
        (top + lft) / 2,
        (lft + rte) / 2,
        (rte + top) / 2
    }};

    uint8_t const levelNext = level - 1;

    // make lines between them
    subdiv_line_recurse(mid[0], mid[1], levelNext);
    subdiv_line_recurse(mid[1], mid[2], levelNext);
    subdiv_line_recurse(mid[2], mid[0], levelNext);

    if (level > 2)
    {
        fill_tri_recurse(   top, mid[0], mid[2], levelNext); // top
        fill_tri_recurse(mid[0],    lft, mid[1], levelNext); // left
        fill_tri_recurse(mid[1], mid[2], mid[0], levelNext); // center
        fill_tri_recurse(mid[2], mid[1],    rte, levelNext); // right
    }
}



