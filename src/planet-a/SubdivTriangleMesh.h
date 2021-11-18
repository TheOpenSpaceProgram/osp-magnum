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

using SharedVrtxStorage_t = osp::IdRefCount<SharedVrtxId>::Storage_t;

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
    return ( y * (y + 1) ) / 2 + x;
};

constexpr std::pair<ChunkLocalSharedId, bool> coord_to_shared(
        uint16_t x, uint16_t y, uint16_t chunkEdgeVrtxCount) noexcept
{
    // Tests if (x,y) lies along right, bottom, or left edges of triangle
    // branchless because why not
    bool const lft = (x == 0);
    bool const btm = !lft && (y == chunkEdgeVrtxCount);
    bool const rte = !btm && (x == y);

    // Calculate chunk local shared ID. starts at 0 from the top of the chunk,
    // and increases counterclockwise along the edge
    uint16_t const offset = lft * y
                          + btm * (chunkEdgeVrtxCount + x)
                          + rte * (chunkEdgeVrtxCount * 3 - x);

    return {ChunkLocalSharedId(offset), rte || btm || lft};
}

//-----------------------------------------------------------------------------


struct Chunk
{
    SkTriStorage_t m_skeletonTri;
};

class ChunkedTriangleMeshInfo
{

public:

    // from wolfram alpha: "sequence 1 3 9 21 45 93"
    // non-trivial to calculate. ~10 is over the practical limit for
    // subdivision levels.
    static inline constexpr std::array<uint16_t, 10> smc_minFansVsLevel =
    {
        1, 3, 9, 21, 45, 93, 189, 381, 765, 1533
    };

    /**
     * @brief Construct a ChunkedTriangleMesh
     *
     * Datatypes chosen are above practical limits, varrying by detail
     * preferences.
     *
     * @param chunkMax     [in] Max number of chunks
     * @param subdivLevels [in] Number of times a chunk triangle is subdivided.
     *                          Practical limit: ~8
     * @param sharedMax    [in] Max number of vertices shared between chunks
     * @param fanMax       [in] Max number of Fan triangles reserved per chunk
     */
    ChunkedTriangleMeshInfo(uint16_t chunkMax, uint8_t subdivLevels,
                            uint32_t sharedMax, uint16_t fanMax)
     : m_chunkMax{ chunkMax }
     , m_chunkSubdivLevel{ subdivLevels }
     , m_chunkWidth{ uint16_t(1u << subdivLevels) }
     , m_chunkIds{ chunkMax }
     , m_chunkData{ std::make_unique<Chunk[]>(chunkMax) }

     , m_chunkVrtxFillCount{ uint16_t((m_chunkWidth-2) * (m_chunkWidth-1) / 2)}

     , m_chunkVrtxSharedCount{ uint16_t(m_chunkWidth * 3) }
     , m_chunkSharedUsed{
           std::make_unique<SharedVrtxStorage_t[]>(chunkMax * m_chunkVrtxSharedCount) }

     , m_chunkIndxFanOffset{ uint32_t(m_chunkWidth)*m_chunkWidth
                             - smc_minFansVsLevel[subdivLevels] }
     , m_chunkIndxFillCount{ m_chunkIndxFanOffset + fanMax }

     , m_sharedMax{ sharedMax }
     , m_sharedIds{ sharedMax }
     , m_sharedRefCount{ sharedMax }
     , m_sharedSkVrtx{ std::make_unique<SkVrtxStorage_t[]>(m_sharedMax) }
     , m_sharedFaceCount{ std::make_unique<uint8_t[]>(m_sharedMax) }

     , m_vrtxSharedOffset{ uint32_t(chunkMax)*m_chunkVrtxFillCount }

     , m_vrtxCountMax{ uint32_t(chunkMax)*m_chunkVrtxFillCount + sharedMax }
     , m_indxCountMax{ uint32_t(chunkMax) * uint32_t(m_chunkIndxFillCount) }
    { };

    ChunkId chunk_create(
            SubdivTriangleSkeleton& rSkel,
            SkTriId skTri,
            ArrayView_t<SkVrtxId> const edgeRte,
            ArrayView_t<SkVrtxId> const edgeBtm,
            ArrayView_t<SkVrtxId> const edgeLft);

    constexpr VertexId chunk_coord_to_vrtx(
            ChunkId chunkId, uint16_t x, uint16_t y) const noexcept
    {
        if (auto const [localId, shared] = coord_to_shared(x, y, m_chunkWidth);
            shared)
        {
            return shared_get_vrtx(chunk_shared(chunkId)[size_t(localId)]);
        }

        // Center, non-shared chunk vertex
        return VertexId(m_chunkVrtxFillCount * uint32_t(chunkId) + xy_to_triangular(x - 1, y - 2));
    }

    /**
     * @return Number of triangles along the edge of a chunk
     */
    constexpr uint32_t chunk_width() const noexcept { return m_chunkWidth; }

    /**
     * @return Number of fill vertices per chunk
     */
    constexpr uint32_t chunk_vrtx_fill_count() const noexcept { return m_chunkVrtxFillCount; }

    /**
     * @brief Get shared vertices used by a specific chunk
     *
     * @param chunkId [in] Chunk Id
     *
     * @return Array view of shared vertices, access using ChunkLocalSharedId
     */
    ArrayView_t<SharedVrtxStorage_t const> chunk_shared(ChunkId chunkId) const noexcept
    {
        size_t const offset = size_t(chunkId) * m_chunkVrtxSharedCount;
        return {&m_chunkSharedUsed[offset], m_chunkVrtxSharedCount};
    };

    /**
     *
     * Param 0: Newly added shared vertices; iterate this.
     * Param 1: Maps SharedVrtxId to their associated SkVrtxId.
     * Param 2: The index of the first shared vertex in the vertex buffer.
     */
    template<typename FUNC_T>
    void shared_update(FUNC_T&& func)
    {
        std::forward<FUNC_T>(func)(
             ArrayView_t<SharedVrtxId const>(m_sharedNewlyAdded),
             ArrayView_t<SkVrtxStorage_t const>(m_sharedSkVrtx.get(), m_sharedMax));

        m_sharedNewlyAdded.clear();
    }

    /**
     * @return Max number of shared vertices
     */
    constexpr uint32_t shared_count_max() const noexcept { return m_sharedMax; }

    uint8_t& shared_face_count(SharedVrtxId sharedId) noexcept { return m_sharedFaceCount[size_t(sharedId)]; }

    /**
     * @return Max number of mesh triangles / Required index buffer size.
     */
    constexpr uint32_t index_count_max() const noexcept { return m_indxCountMax; };

    constexpr uint32_t index_chunk_offset(ChunkId chunkId) const noexcept { return m_chunkIndxFillCount * uint32_t(chunkId); }

    /**
     * @return Total max number of shared and fill vertices / Required vertex
     *         buffer size.
     */
    constexpr uint32_t vertex_count_max() const noexcept { return m_vrtxCountMax; };

    /**
     * @return Index of first shared vertex / Max total number of fill vertices
     */
    constexpr uint32_t vertex_offset_shared() const noexcept { return m_vrtxSharedOffset; }

    constexpr SharedVrtxId vertex_to_shared(VertexId sharedId) const noexcept { return SharedVrtxId(uint32_t(sharedId) - m_vrtxSharedOffset); }

    constexpr uint32_t vertex_offset_fill(ChunkId chunkId) const noexcept
    {
        return m_chunkVrtxFillCount * uint32_t(chunkId);
    }

    constexpr bool vertex_is_shared(VertexId vrtx) const noexcept
    {
        return uint32_t(vrtx) >= m_vrtxSharedOffset;
    }

    void clear(SubdivTriangleSkeleton& rSkel)
    {
        // Delete all chunks
        for (uint16_t i = 0; i < m_chunkIds.capacity(); i ++)
        {
            if ( ! m_chunkIds.exists(ChunkId(i)) )
            {
                continue;
            }

            // Release their associated skeleton triangle
            rSkel.tri_release(m_chunkData[i].m_skeletonTri);

            // Release all of their shared vertices
            for (SharedVrtxStorage_t& shared : chunk_shared_mutable(ChunkId(i)))
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

private:


    ArrayView_t<SharedVrtxStorage_t> chunk_shared_mutable(ChunkId chunkId) noexcept
    {
        size_t const offset = size_t(chunkId) * m_chunkVrtxSharedCount;
        return {&m_chunkSharedUsed[offset], m_chunkVrtxSharedCount};
    }

    SharedVrtxId chunk_shared_get(ChunkId chunkId, ChunkLocalSharedId localId) const noexcept
    {
        return m_chunkSharedUsed[size_t(chunkId) * m_chunkVrtxSharedCount
                                 + size_t(localId)];
    }

    /**
     * @brief Create or get a shared vertex associated with a skeleton vertex
     *
     * @param skVrtxId
     * @return
     */
    SharedVrtxId shared_get_or_create(SkVrtxId skVrtxId,
                                      SubdivTriangleSkeleton &rSkel)
    {
        auto const& [it, success]
                = m_skVrtxToShared.try_emplace(skVrtxId, SharedVrtxId(0));
        if (success)
        {
            SharedVrtxId const id = m_sharedIds.create();
            it->second = id;
            m_sharedFaceCount[size_t(id)] = 0;
            m_sharedSkVrtx[size_t(id)] = rSkel.vrtx_store(skVrtxId);
            m_sharedNewlyAdded.push_back(id);
        }

        return it->second;
    }

    SharedVrtxStorage_t shared_store(SharedVrtxId triId)
    {
        return m_sharedRefCount.ref_add(triId);
    }

    void shared_release(SharedVrtxStorage_t &rStorage) noexcept
    {
        m_sharedRefCount.ref_release(rStorage);
    }

    constexpr VertexId shared_get_vrtx(SharedVrtxId sharedId) const
    {
        return VertexId(uint32_t(m_vrtxSharedOffset) + uint32_t(sharedId));
    };

    uint16_t m_chunkMax;
    uint8_t m_chunkSubdivLevel;
    uint16_t m_chunkWidth;
    osp::IdRegistry<ChunkId, true> m_chunkIds;
    std::unique_ptr<Chunk[]> m_chunkData;

    uint16_t m_chunkVrtxFillCount;

    uint16_t m_chunkVrtxSharedCount;
    std::unique_ptr<SharedVrtxStorage_t[]> m_chunkSharedUsed;

    uint32_t m_chunkIndxFanOffset;
    uint32_t m_chunkIndxFillCount;

    uint32_t m_sharedMax;
    osp::IdRegistry<SharedVrtxId, true> m_sharedIds;
    osp::IdRefCount<SharedVrtxId> m_sharedRefCount;
    std::unique_ptr<SkVrtxStorage_t[]> m_sharedSkVrtx;
    // Connected face count used for vertex normal calculations
    std::unique_ptr<uint8_t[]> m_sharedFaceCount;
    std::unordered_map<SkVrtxId, SharedVrtxId> m_skVrtxToShared;

    // Newly added shared vertices, position needs to be copied from skeleton
    std::vector<SharedVrtxId> m_sharedNewlyAdded;

    // Index of first shared vertex in vertex buffer
    // = m_chunkMax*m_chunkVrtxCount
    uint32_t m_vrtxSharedOffset;

    uint32_t m_vrtxCountMax;
    uint32_t m_indxCountMax;

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

    //std::unique_ptr<unsigned char[]> m_vrtxBuffer;

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
    //std::unique_ptr<Vector3ui[]> m_indxBuffer;

    //std::variant< std::unique_ptr<Vector3us_t[]>,
    //              std::unique_ptr<Vector3ui_t[]> > m_indxBuffer;

    // When Chunked vertices, Shared vertices, or Chunks are deleted, then
    // they are simply added to a list of deleted elements in IdRegistry to
    // be reused later, and are not drawn.

    // Empty spaces in the buffers are not a problem. For index buffers, use
    // something like glMultiDrawArrays

}; // class ChunkedTriangleMeshInfo

//-----------------------------------------------------------------------------

/**
 * @brief Stores a procedure on which combinations of vertices need to be
 *        subdivided to calculate chunk fill vertices.
 */
class ChunkVrtxSubdivLUT
{

public:

    using Vector2us = Magnum::Math::Vector2<uint16_t>;

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
            ArrayView_t<SharedVrtxStorage_t const> sharedUsed,
            VRTX_BUF_T chunkVrtx,
            VRTX_BUF_T sharedVrtx) const noexcept
    {
        return (uint16_t(lutVrtx) < m_fillVrtxCount)
                ? chunkVrtx[size_t(lutVrtx)]
                : sharedVrtx[size_t(sharedUsed[uint16_t(lutVrtx) - m_fillVrtxCount].value())];
    }

    constexpr ArrayView_t<ToSubdiv const> data() const noexcept { return m_data; }

private:

    constexpr LUTVrtx id_at(Vector2us const pos) const noexcept
    {
        auto const [localId, shared] = coord_to_shared(pos.x(), pos.y(),
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


ChunkedTriangleMeshInfo make_subdivtrimesh_general(
        unsigned int chunkMax, unsigned int subdivLevels, int pow2scale);

}
