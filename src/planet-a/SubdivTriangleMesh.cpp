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



ChunkedTriangleMeshInfo planeta::make_subdivtrimesh_general(
        unsigned int const chunkMax, unsigned int const subdivLevels, int const pow2scale)
{
    // calculate stuff here

    using std::sqrt;
    using std::ceil;
    using std::pow;

    float const C = static_cast<float>(chunkMax);
    float const L = static_cast<float>(subdivLevels);

    // Calculate a fair number of shared vertices, based on a triangular
    // tiling pattern.
    // worked out here: https://www.desmos.com/calculator/ffd8lraosl
    unsigned int const sharedMax
            = ceil( ( 3.0f*C + sqrt(6.0f)*sqrt(C) ) * pow(2.0f, L-1) - C + 1 );

    unsigned int const fanMax
            = ChunkedTriangleMeshInfo::smc_minFansVsLevel[subdivLevels] * 2;

    return ChunkedTriangleMeshInfo(
                chunkMax, subdivLevels, sharedMax, fanMax);
}

ChunkId ChunkedTriangleMeshInfo::chunk_create(
        SubdivTriangleSkeleton& rSkel,
        SkTriId skTri,
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
    Chunk &rChunk = m_chunkData[std::size_t(chunkId)];

    rChunk.m_skeletonTri = rSkel.tri_store(skTri);

    SkeletonTriangle const &tri = rSkel.tri_at(skTri);

    ArrayView_t<SharedVrtxStorage_t> const sharedSpace = chunk_shared_mutable(chunkId);

    std::array const edges = {edgeRte, edgeBtm, edgeLft};

    for (int i = 0; i < 3; i ++)
    {
        std::size_t const cornerOffset = static_cast<std::size_t>(m_chunkWidth) * i;
        std::size_t const edgeOffset   = static_cast<std::size_t>(m_chunkWidth) * i + 1;

        ArrayView_t<SkVrtxId> const edge = edges[i];
        {
            SharedVrtxId const sharedId = shared_get_or_create(
                    tri.m_vertices[i], rSkel);
            sharedSpace[cornerOffset] = shared_store(sharedId);
        }
        for (std::size_t j = 0; j < m_chunkWidth - 1; j ++)
        {
            SharedVrtxId const sharedId = shared_get_or_create(edge[j], rSkel);
            sharedSpace[edgeOffset + j] = shared_store(sharedId);
        }
    }

    return chunkId;
}

//-----------------------------------------------------------------------------

ChunkVrtxSubdivLUT::ChunkVrtxSubdivLUT(uint8_t const subdivLevel)
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
        Vector2us const a, Vector2us const b, uint8_t const level)
{
    Vector2us const mid = (a + b) / 2;

    ChunkLocalSharedId const out = ChunkLocalSharedId(xy_to_triangular(mid.x() - 1, mid.y() - 2));

    m_data.emplace_back(ToSubdiv{id_at(a), id_at(b), out});

    if (level > 1)
    {
        subdiv_line_recurse(a, mid, level - 1);
        subdiv_line_recurse(mid, b, level - 1);
    }

}

void ChunkVrtxSubdivLUT::fill_tri_recurse(
        Vector2us const top, Vector2us const lft, Vector2us const rte, uint8_t const level)
{
    // calculate midpoints
    std::array<Vector2us, 3> const mid
    {{
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



