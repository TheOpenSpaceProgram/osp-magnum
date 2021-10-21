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
#pragma once

#include "SubdivSkeleton.h"

#include <osp/types.h>

#include <Magnum/Mesh.h>

#include <memory>
#include <variant>

namespace planeta
{

// ID for all chunks, from 0 to m_chunkMax
enum class ChunkId : uint16_t {};

// ID for all shared vertices; from 0 to m_sharedMax
enum class SharedVrtxId : uint32_t {};

// Indices of vertices, unaware of vertex size;
// from 0 to (m_chunkMax*m_chunkVrtxCount + m_sharedMax)
enum class VertexId : uint32_t {};

// IDs for any chunk's shared vertices; from 0 to m_chunkVrtxSharedCount
enum class ChunkLocalSharedId : uint16_t {};

// IDs for any chunk's fill vertices; from 0 to m_chunkVrtxSharedCount
enum class ChunkLocalFillId : uint16_t {};


struct Chunk
{
    SkTriId m_skeletonTri;
};

class ChunkedTriangleMesh
{
    using Vector3ui = Magnum::Math::Vector3<Magnum::UnsignedInt>;

public:

    // from wolfram alpha: "sequence 1 3 9 21 45 93"
    // non-trivial to calculate. ~10 is over the practical limit for
    // subdivision levels.
    static inline constexpr std::array<uint16_t, 10> smc_minFansVsLevel =
    {
        1, 3, 9, 21, 45, 93, 189, 381, 765, 1533
    };

    /**
     * @brief ChunkedTriangleMesh
     *
     * @param chunkMax     [in] Max number of chunks
     * @param subdivLevels [in] Number of times a chunk triangle is subdivided.
     *                          Practical limit: ~8
     * @param sharedMax    [in] Max number of shared vertices
     * @param vrtxSize     [in] Size in bytes of each vertex
     * @param fanMax       [in] Max number of Fan triangles reserved for a chunk
     */
    ChunkedTriangleMesh(uint16_t chunkMax, uint8_t subdivLevels,
                        uint32_t sharedMax, uint8_t vrtxSize,
                        uint16_t fanMax)
     : m_chunkMax{ chunkMax }
     , m_chunkSubdivLevel{ subdivLevels }
     , m_chunkEdgeVrtxCount{ uint16_t(1u << subdivLevels) }
     , m_chunkIds{ chunkMax }
     , m_chunkData{ std::make_unique<Chunk[]>(chunkMax) }

     , m_chunkVrtxFillCount{ uint16_t((m_chunkEdgeVrtxCount-2) * (m_chunkEdgeVrtxCount-1) / 2)}

     , m_chunkVrtxSharedCount{ uint16_t(m_chunkEdgeVrtxCount * 3) }
     , m_chunkSharedUsed{
           std::make_unique<SharedVrtxId[]>(chunkMax * m_chunkVrtxSharedCount) }

     , m_chunkIndxCount{ uint16_t(subdivLevels*subdivLevels + fanMax) }
     , m_chunkIndxFanOffset{ uint16_t(subdivLevels*subdivLevels) }

     , m_sharedMax{ sharedMax }
     , m_sharedIds{ sharedMax }
     , m_sharedSkVrtx{ std::make_unique<SkVrtxId[]>(m_sharedMax) }
     , m_sharedRefCount{ std::make_unique<uint8_t[]>(m_sharedMax) }
     , m_sharedFaceCount{ std::make_unique<uint8_t[]>(m_sharedMax) }

     , m_vrtxSize( vrtxSize )
     , m_vrtxSharedOffset{ uint32_t(chunkMax)*m_chunkVrtxFillCount }
     , m_vrtxBufferSize{ (uint32_t(chunkMax)*m_chunkVrtxFillCount + sharedMax)*m_vrtxSize }
     , m_vrtxBuffer{ std::make_unique<unsigned char[]>(m_vrtxBufferSize) }
    { };

    ChunkId chunk_create(
            SubdivTriangleSkeleton rSkel,
            SkTriId skTri,
            ArrayView_t<SkVrtxId> const edgeRte,
            ArrayView_t<SkVrtxId> const edgeBtm,
            ArrayView_t<SkVrtxId> const edgeLft);

    VertexId chunk_coord_to_mesh(ChunkId chunkId, uint16_t x, uint16_t y) const noexcept;

    SharedVrtxId shared_get_or_create(SkVrtxId skVrtxId)
    {
        auto const& [it, success]
                = m_skVrtxToShared.try_emplace(skVrtxId, SharedVrtxId(0));
        if (success)
        {
            SharedVrtxId const id = m_sharedIds.create();
            it->second = id;
            m_sharedRefCount[size_t(id)] = 0;
            m_sharedFaceCount[size_t(id)] = 0;
            m_sharedSkVrtx[size_t(id)] = skVrtxId;
            m_sharedNewlyAdded.push_back(id);
        }

        return it->second;
    }

    void shared_refcount_add(SharedVrtxId id) noexcept { m_sharedRefCount[size_t(id)] ++; };
    void shared_refcount_remove(SharedVrtxId id) noexcept { m_sharedRefCount[size_t(id)] --; };

    /**
     *
     * Param 0: Newly added shared vertices; iterate this.
     * Param 1: Maps SharedVrtxId to their associated SkVrtxId.
     * Param 2: The index of the first shared vertex in the vertex buffer.
     * Param 3: Vertex buffer output in user-specified format
     */
    template<typename FUNC_T>
    void shared_update(FUNC_T func)
    {

        func(ArrayView_t<SharedVrtxId const>(m_sharedNewlyAdded),
             ArrayView_t<SkVrtxId const>(m_sharedSkVrtx.get(), m_sharedMax),
             m_vrtxSharedOffset,
             ArrayView_t<unsigned char>(m_vrtxBuffer.get(), m_vrtxBufferSize));

        m_sharedNewlyAdded.clear();
    }

    /**
     * Param 0: chunk ID
     * Param 1: Shared vertices used by chunk, access using ChunkLocalSharedId
     * Param 2: Shared max
     * Param 3: Number of filled vertices per chunk
     * Param 4: Vertex buffer output in user-specified format
     */
    template<typename FUNC_T>
    void chunk_calc_vrtx_fill(ChunkId chunkId, FUNC_T func)
    {
        auto const* thisConst = static_cast<const ChunkedTriangleMesh*>(this);

        func(chunkId,
             thisConst->chunk_shared(chunkId),
             m_chunkVrtxFillCount,
             m_vrtxSharedOffset,
             ArrayView_t<unsigned char>(m_vrtxBuffer.get(), m_vrtxBufferSize));
    }

private:

    ArrayView_t<SharedVrtxId const> chunk_shared(ChunkId chunkId) const noexcept
    {
        size_t const offset = size_t(chunkId) * m_chunkVrtxSharedCount;
        return {&m_chunkSharedUsed[offset], m_chunkVrtxSharedCount};
    };

    ArrayView_t<SharedVrtxId> chunk_shared(ChunkId chunkId) noexcept
    {
        size_t const offset = size_t(chunkId) * m_chunkVrtxSharedCount;
        return {&m_chunkSharedUsed[offset], m_chunkVrtxSharedCount};
    }

    SharedVrtxId chunk_shared_get(ChunkId chunkId, ChunkLocalSharedId localId) const noexcept
    {
        return m_chunkSharedUsed[size_t(chunkId) * m_chunkVrtxSharedCount
                                 + size_t(localId)];
    }

    constexpr VertexId shared_get_vrtx(SharedVrtxId sharedId) const
    {
        return VertexId(uint32_t(m_vrtxSharedOffset) + uint32_t(sharedId));
    };

    uint16_t m_chunkMax;
    uint8_t m_chunkSubdivLevel;
    uint16_t m_chunkEdgeVrtxCount;
    IdRegistry<ChunkId, true> m_chunkIds;
    std::unique_ptr<Chunk[]> m_chunkData;

    uint16_t m_chunkVrtxFillCount;

    uint16_t m_chunkVrtxSharedCount;
    std::unique_ptr<SharedVrtxId[]> m_chunkSharedUsed;

    uint16_t m_chunkIndxCount;
    uint16_t m_chunkIndxFanOffset;

    uint32_t m_sharedMax;
    IdRegistry<SharedVrtxId, true> m_sharedIds;
    std::unique_ptr<SkVrtxId[]> m_sharedSkVrtx;
    std::unique_ptr<uint8_t[]> m_sharedRefCount;
    // Connected face count used for vertex normal calculations
    std::unique_ptr<uint8_t[]> m_sharedFaceCount;
    std::unordered_map<SkVrtxId, SharedVrtxId> m_skVrtxToShared;

    // Newly added shared vertices, position needs to be copied from skeleton
    std::vector<SharedVrtxId> m_sharedNewlyAdded;


    // Vertex and index buffer for GPU

    // Vertex buffer is split between large blocks of Chunked vertices, and
    // individual Shared vertices:
    // * Chunked vertices are fixed-size groups of vertices associated with
    //   a SkeletonTriangle. They help form the triangular tiling mesh in the
    //   middle of a chunked SkeletonTriangle, and are not shared with other
    //   SkeletonTriangles.
    // * Shared vertices reside along the edges of SkeletonTriangles, and are
    //   shared between them. They are associated with a VrtxId in m_vrtxIdTree
    //
    // Chunk vertex data:
    // [vertex] [vertex] [vertex] [vertex] [vertex] ...
    // <-------  m_chunkVrtxCount*m_vrtxSize  ------->
    //
    // Vertex buffer m_vrtxBuffer:
    // [chunk..] [chunk..] [chunk..]  [chunk..] ... [shared] [shared] ...
    // <-----  m_chunkMax*m_chunkVrtxCount  ----->  <--  m_sharedMax  -->
    //         * m_vrtxSize                              * m_vrtxSize
    uint8_t m_vrtxSize;
    // First shared vertex, = m_chunkMax*m_chunkVrtxCount
    VertexId m_vrtxSharedOffset;
    size_t m_vrtxBufferSize;
    std::unique_ptr<unsigned char[]> m_vrtxBuffer;

    // Index buffer consists of large fixed-size blocks forming a mesh patched
    // overtop of a SkeletonTriangle, known as a Chunk. Each chunk consists of
    // a fixed number of mesh triangles to form most of the triangular tiling
    // of the chunk, but the triangles along the edges are specially treated as
    // a variable (but limited) number of 'Fan triangles' that may form fan-like
    // structures to smoothly transition to other chunks of a higher detail.
    //
    // Chunk index data:
    // [tri] [tri] [tri] [tri] ...  [fan tri] [fan tri] [fan tri] ... [empty]
    // <-  m_chunkIndxFanOffset  ->
    // <------------------------  m_chunkIndxCount  ------------------------>
    //
    // Index buffer m_indxBuffer:
    // [chunk..] [chunk..] [chunk..] [chunk..] [chunk..] [chunk..] ...
    // <---------------  m_chunkMax * m_chunkIndxCount  --------------->
    std::unique_ptr<Vector3ui[]> m_indxBuffer;

    //std::variant< std::unique_ptr<Vector3us_t[]>,
    //              std::unique_ptr<Vector3ui_t[]> > m_indxBuffer;

    // When Chunked vertices, Shared vertices, or Chunks are deleted, then
    // they are simply added to a list of deleted elements in IdRegistry to
    // be reused later, and are not drawn.

    // Empty spaces in the buffers are not a problem. For index buffers, use
    // something like glMultiDrawArrays

}; // class SubdivTriangleMesh

//-----------------------------------------------------------------------------

/**
 * Convert XY coordinates to a triangular number index
 *
 *  0
 *  1  2
 *  3  4  5
 *  6  7  8  9
 * 10 11 12 13 14
 * ...
 * x = right; y = down; (0, 0) = 0
 *
 * @param x [in]
 * @param y [in]
 * @return triangular number
 */
constexpr int xy_to_triangular(int x, int y) noexcept
{
    return y * (y + 1) / 2 + x;
};

constexpr std::pair<ChunkLocalSharedId, bool> try_get_shared(
        uint16_t x, uint16_t y, uint16_t chunkEdgeVrtxCount) noexcept
{
    // Tests if (x,y) lies along right, bottom, or left edges of triangle
    // branchless because why not
    bool lft = (x == 0);
    bool btm = !lft && (y == chunkEdgeVrtxCount);
    bool rte = !btm && (x == y);

    // Calculate chunk local shared ID. starts at 0 from the top of the chunk,
    // and increases counterclockwise along the edge
    uint32_t offset = lft * y
                    + btm * (chunkEdgeVrtxCount + x)
                    + rte * (chunkEdgeVrtxCount * 3 - x);

    return {ChunkLocalSharedId(offset), rte || btm || lft};
}

//-----------------------------------------------------------------------------

/**
 * @brief Stores a procedure on which combinations of vertices need to be
 *        subdivided to calculate chunk fill vertices.
 */
class ChunkVrtxSubdivLUT
{
    using Vector2us = Magnum::Math::Vector2<uint16_t>;

public:

    // Can either be a ChunkLocalSharedId or ChunkLocalFillId
    // Fill vertex if (0 to m_chunkVrtxCount-1)
    // Shared vertex if m_chunkVrtxCount to (m_edgeVrtxCount*3-1)
    enum class LUTVrtx : uint16_t {};

    struct ToSubdiv
    {
        LUTVrtx m_vrtxA;
        LUTVrtx m_vrtxB;

        ChunkLocalSharedId m_fillOut;
    };

    ChunkVrtxSubdivLUT(uint8_t subdivLevel);

    template<typename VRTX_BUF_T>
    constexpr decltype (auto) get(
            LUTVrtx lutVrtx,
            ArrayView_t<SharedVrtxId const> sharedUsed,
            VRTX_BUF_T chunkVrtx,
            VRTX_BUF_T sharedVrtx) const noexcept
    {
        return (uint16_t(lutVrtx) < m_fillVrtxCount)
                ? chunkVrtx[size_t(lutVrtx)]
                : sharedVrtx[size_t(sharedUsed[uint16_t(lutVrtx) - m_fillVrtxCount])];
    }

    constexpr ArrayView_t<ToSubdiv const> data() const noexcept { return m_data; }

private:

    constexpr LUTVrtx id_at(Vector2us const pos) const noexcept
    {
        auto const [localId, shared] = try_get_shared(pos.x(), pos.y(),
                                                      m_edgeVrtxCount);
        if (shared)
        {
            return LUTVrtx(m_fillVrtxCount + uint16_t(localId));
        }

        return LUTVrtx(xy_to_triangular(pos.x() - 1, pos.y() - 2));
    };

    /**
     * @brief subdiv_edge_recurse
     *
     * @param a     [in]
     * @param b     [in]
     * @param level [in] Number of times this line can be subdivided further
     */
    void subdiv_line_recurse(Vector2us a, Vector2us b, uint8_t level);

    void fill_tri_recurse(Vector2us top, Vector2us lft, Vector2us rte, uint8_t level);

    //std::unique_ptr<ToSubdiv[]> m_data;
    std::vector<ToSubdiv> m_data;
    uint16_t m_fillVrtxCount;
    uint16_t m_edgeVrtxCount;

}; // class ChunkVrtxSubdivLUT


ChunkedTriangleMesh make_subdivtrimesh_general(
        unsigned int chunkMax, unsigned int subdivLevels,
        unsigned int vrtxSize, int pow2scale);

}
