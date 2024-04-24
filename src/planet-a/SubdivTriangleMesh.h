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

#include <osp/core/math_types.h>

#include <Corrade/Containers/ArrayViewStl.h>

#include <optional>

namespace planeta
{


// ID for all shared vertices; from 0 to m_sharedMax


// Indices to vertices, unaware of vertex size;
// from 0 to (m_chunkMax*m_chunkVrtxCount + m_sharedMax)
using VertexIdx = uint32_t;

// IDs for any chunk's shared vertices; from 0 to m_chunkVrtxSharedCount
enum class ChunkLocalSharedId : uint16_t {};

// IDs for any chunk's fill vertices; from 0 to m_chunkVrtxSharedCount
enum class ChunkLocalFillId : uint16_t {};


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
constexpr int xy_to_triangular(int const x, int const y) noexcept
{
    return ( y * (y + 1) ) / 2 + x;
};

struct CoordToSharedResult
{
    ChunkLocalSharedId localShared;
    bool isShared;
};

constexpr CoordToSharedResult coord_to_shared(
        uint16_t x, uint16_t y, uint16_t chunkWidth) noexcept
{
    // Tests if (x,y) lies along right, bottom, or left edges of triangle

    if (x == 0)         // Left
    {
        return { ChunkLocalSharedId(y), true};
    }
    else if (y == chunkWidth) // Bottom
    {
        return { ChunkLocalSharedId(chunkWidth + x), true};
    }
    else if (x == y)    // Right
    {
        return { ChunkLocalSharedId(chunkWidth * 3 - x), true};
    }
    else                // Non-shared vertex
    {
        return { .isShared = false };
    }
}

//-----------------------------------------------------------------------------


struct ChunkedTriangleMeshInfo
{
    /// Number of non-fill 'Fan' faces a chunk of a given subdiv level will contain. These are
    /// triangle faces along the edges of the chunk that have two shared vertices (a whole edge
    /// lining up along the chunk edge).
    ///
    /// This is not easy to to calculate, and was first manually counted. Wolfram alpha was then
    /// used to complete the sequence. query: "sequence 1 3 9 21 45 93"
    ///
    /// 10 subdivision levels is beyond the practical limit
    ///
    static constexpr std::array<std::uint16_t, 10> smc_fanFacesVsSubdivLevel =
    {
        1u, 3u, 9u, 21u, 45u, 93u, 189u, 381u, 765u, 1533u
    };

    constexpr bool is_vertex_shared(VertexIdx const vrtx) const noexcept
    {
        auto const sharedIdInt = uint32_t(vrtx) - vbufSharedOffset; // note: intentional overflow
        return sharedIdInt < sharedMax;
    }

    constexpr SharedVrtxId vertex_to_shared(VertexIdx const vrtx) const noexcept
    {
        return SharedVrtxId(uint32_t(vrtx) - vbufSharedOffset);
    }

    /// Number of non-shared vertices that fill the center of a chunk. Vertices that don't lie
    /// along the outer edges of their chunk are not shared with other chunks.
    std::uint32_t fillVrtxCount;
    /// Number of triangle faces needed to fill the center of the chunk. This includes faces
    /// with a single corner touching the edge. Faces with two shared vertices (a whole edge
    /// lining up along the chunk edge) are excluded.
    std::uint32_t fillFaceCount;
    /// Number of fan triangles per-chunk + extra triangles needed to fill a chunk edge with 2-triangle
    std::uint32_t fanExtraFaceCount;

    /// Max total faces per chunk. fillFaceCount + fanExtraFaceCount
    std::uint32_t chunkMaxFaceCount;

    /// Index of first fill vertex within in vertex buffer
    std::uint32_t vbufFillOffset;
    /// Index of first shared vertex within in vertex buffer
    std::uint32_t vbufSharedOffset;

    /// Total size of vertex buffer
    std::uint32_t vbufSize;

    std::uint32_t sharedMax;
    std::uint16_t chunkMax;
};

constexpr ChunkedTriangleMeshInfo make_chunked_mesh_info(
        ChunkSkeleton const&        skChunks,
        std::uint16_t const         chunkMax,
        std::uint32_t const         sharedMax)
{
    std::uint32_t const chunkWidth      = skChunks.m_chunkEdgeVrtxCount;
    std::uint32_t const fillCount       = (chunkWidth-2)*(chunkWidth-1) / 2;
    std::uint32_t const fillTotal       = fillCount * chunkMax;
    std::uint32_t const fanFaceCount    = ChunkedTriangleMeshInfo::smc_fanFacesVsSubdivLevel[skChunks.m_chunkSubdivLevel];
    std::uint32_t const fillFaceCount   = chunkWidth*chunkWidth - fanFaceCount;
    std::uint32_t const fanExtraFaceCount = fanFaceCount + fanFaceCount/3 + 1;

    return
    {
        .fillVrtxCount       = fillCount,
        .fillFaceCount       = fillFaceCount,
        .fanExtraFaceCount   = fanExtraFaceCount,
        .chunkMaxFaceCount   = fillFaceCount + fanExtraFaceCount,
        .vbufFillOffset      = 0,
        .vbufSharedOffset    = fillTotal,
        .vbufSize            = fillTotal + sharedMax,
        .sharedMax           = sharedMax,
        .chunkMax            = chunkMax
    };
}

inline VertexIdx shared_to_vrtx(ChunkSkeleton const& skChunks, ChunkedTriangleMeshInfo const& info, ChunkId const chunkId, ChunkLocalSharedId const localId)
{
    auto const&         chunkSharedVertices = skChunks.shared_vertices_used(chunkId);
    SharedVrtxId const  sharedId            = chunkSharedVertices[std::size_t(localId)].value();
    return info.vbufSharedOffset + sharedId.value;
}

constexpr VertexIdx fill_to_vrtx(ChunkedTriangleMeshInfo const& info, ChunkId const chunkId, int const triangular)
{
    return info.vbufFillOffset + info.fillVrtxCount * chunkId.value + triangular;
}

constexpr VertexIdx chunk_coord_to_vrtx(ChunkSkeleton const& skChunks, ChunkedTriangleMeshInfo const& info, ChunkId const chunkId, uint16_t const x, uint16_t const y) noexcept
{
    auto const [localId, isShared] = coord_to_shared(x, y, skChunks.m_chunkEdgeVrtxCount);
    return isShared
            ? shared_to_vrtx(skChunks, info, chunkId, localId)
            : fill_to_vrtx(info, chunkId, xy_to_triangular(x - 1, y - 2));

}

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

    constexpr VertexIdx index(
            osp::ArrayView<SharedVrtxOwner_t const> sharedUsed,
            std::uint32_t const                     fillOffset,
            std::uint32_t const                     sharedOffset,
            LUTVrtx const                           lutVrtx ) const noexcept
    {
        auto const lutVrtxInt   = std::uint16_t(lutVrtx);
        bool const isShared     = lutVrtxInt > m_fillVrtxCount;

        return isShared ? sharedOffset + sharedUsed[lutVrtxInt-m_fillVrtxCount].value().value
                        : fillOffset + lutVrtxInt;
    }

    constexpr osp::ArrayView<ToSubdiv const> data() const noexcept { return m_data; }

    friend ChunkVrtxSubdivLUT make_chunk_vrtx_subdiv_lut(uint8_t subdivLevel);

private:

    constexpr LUTVrtx id_at(Vector2us const pos) const noexcept
    {
        auto const [localId, isShared] = coord_to_shared(pos.x(), pos.y(), m_edgeVrtxCount);
        if (isShared)
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

ChunkVrtxSubdivLUT make_chunk_vrtx_subdiv_lut(uint8_t subdivLevel);

//-----------------------------------------------------------------------------

enum class ECornerDetailX2 : int { None = 0, Right = 1, Left = 2 };

template <typename FUNCTOR_T>
struct ChunkFanStitcher
{
    template <int corner, ECornerDetailX2 detailX2>
    inline void corner();

    template <int side, bool detailX2>
    inline void edge();

    FUNCTOR_T                           emit_face;

    osp::ArrayView<SharedVrtxOwner_t const> chunkSharedVertices;

    osp::ArrayView<SharedVrtxOwner_t const> edge2XSharedVertices0;
    osp::ArrayView<SharedVrtxOwner_t const> edge2XSharedVertices1;

    ChunkSkeleton const                &rSkChunks;

    std::uint32_t                       sharedOffset;
    std::uint32_t                       chunkFillOffset;
    std::uint16_t                       chunkWidth;

}; // struct ChunkFanStitcher

struct X2DetailNeighbor
{
    ChunkId chunkA;
    ChunkId chunkB;
    //std::uint8_t ownEdge;
    std::uint8_t neighborEdge;
};

template<typename FUNCTOR_T>
ChunkFanStitcher<FUNCTOR_T> make_chunk_fan_stitcher(
        ChunkId                             chunk,
        std::optional< X2DetailNeighbor >   oX2Detail,
        ChunkSkeleton                 const &rSkChunks,
        ChunkedTriangleMeshInfo       const &rInfo,
        FUNCTOR_T                           &&emit_face)
{
    if (oX2Detail.has_value())
    {
        std::uint32_t const edgeVrtxCount = rSkChunks.m_chunkEdgeVrtxCount;
        X2DetailNeighbor const x2Detail = oX2Detail.value();
        return ChunkFanStitcher<FUNCTOR_T> {
            .emit_face              = std::forward<FUNCTOR_T>(emit_face),
            .chunkSharedVertices    = rSkChunks.shared_vertices_used(chunk),
            .edge2XSharedVertices0  = rSkChunks.shared_vertices_used(x2Detail.chunkA).sliceSize(edgeVrtxCount * x2Detail.neighborEdge, edgeVrtxCount),
            .edge2XSharedVertices1  = rSkChunks.shared_vertices_used(x2Detail.chunkB).sliceSize(edgeVrtxCount * x2Detail.neighborEdge, edgeVrtxCount),
            .rSkChunks              = rSkChunks,
            .sharedOffset           = rInfo.vbufSharedOffset,
            .chunkFillOffset        = rInfo.vbufFillOffset + std::uint32_t(chunk.value)*rInfo.fillVrtxCount,
            .chunkWidth             = rSkChunks.m_chunkEdgeVrtxCount
        };
    }
    else
    {
        return ChunkFanStitcher<FUNCTOR_T> {
            .emit_face                  = std::forward<FUNCTOR_T>(emit_face),
            .chunkSharedVertices        = rSkChunks.shared_vertices_used(chunk),
            .rSkChunks                  = rSkChunks,
            .sharedOffset               = rInfo.vbufSharedOffset,
            .chunkFillOffset            = rInfo.vbufFillOffset + std::uint32_t(chunk.value)*rInfo.fillVrtxCount,
            .chunkWidth                 = rSkChunks.m_chunkEdgeVrtxCount
        };
    }
}

template <typename FUNCTOR_T>
template <int corner, ECornerDetailX2 detailX2>
void ChunkFanStitcher<FUNCTOR_T>::corner()
{
    // Chunk corner consists of 3 shared vertices
    //
    //          [1]
    //          . .
    //         .   .
    //        .     .
    //      [2]-----[0]
    //     /   \   /   \
    //    /     \ /     \
    //   X-------X-------X  <-- don't care what's down heres
    //
    std::array<int, 3> tri;

    // Figure out which 3 shared vertices make the chunk's corner
    if constexpr (corner == 0)
    {
        tri = { chunkWidth*3-1, chunkWidth*0+0, chunkWidth*0+1 };
    }
    else if constexpr (corner == 1)
    {
        tri = { chunkWidth*1-1, chunkWidth*1+0, chunkWidth*1+1 };
    }
    else if constexpr (corner == 2)
    {
        tri = { chunkWidth*2-1, chunkWidth*2+0, chunkWidth*2+1 };
    }

    std::array<SharedVrtxId, 3> const triShared
    {
        chunkSharedVertices[tri[0]].value(),
        chunkSharedVertices[tri[1]].value(),
        chunkSharedVertices[tri[2]].value()
    };

    std::array<VertexIdx, 3> const triVrtx
    {
        sharedOffset + triShared[0].value,
        sharedOffset + triShared[1].value,
        sharedOffset + triShared[2].value
    };

    if (detailX2 == ECornerDetailX2::None)
    {
        // Add 1 triangle normally
        //        1
        //       / \
        //      /   \
        //     2-----0
        emit_face(triVrtx[0], triVrtx[1], triVrtx[2]);
    }
    else if constexpr (detailX2 == ECornerDetailX2::Right)
    {
        // Add 2 triangles: 'right' side has higher detail
        //             1
        //            / \
        //           /   \
        //          /    MID
        //         /  _-'  \
        //        /_-'      \
        //       2'----------0

        SkVrtxId const skelA   = rSkChunks.m_sharedToSkVrtx[triShared[1]];
        SkVrtxId const skelB   = rSkChunks.m_sharedToSkVrtx[triShared[2]];

        /*
        SkVrtxId const skelMid = rSkeleton.vrtx_ids().try_get(skelA, skelB);

        if (skelMid != lgrn::id_null<SkVrtxId>())
        {
            SharedVrtxId const sharedMid = rSkChunks.m_skVrtxToShared[skelMid];
            VertexIdx const    vrtxMid   = sharedOffset + sharedMid.value;

            emit_face(triVrtx[2], triVrtx[1], vrtxMid);
            emit_face(triVrtx[2], vrtxMid, triVrtx[0]);
        }
        else
        {
            emit_face(triVrtx[0], triVrtx[1], triVrtx[2]);
        }
        */
    }
    else if constexpr (detailX2 == ECornerDetailX2::Left)
    {
        // Add 2 triangles: 'left' side has higher detail.
        //             1
        //            / \
        //           /   \
        //         MID    \
        //         /  `-_  \
        //        /      `-_\
        //       2----------`0

        SkVrtxId const skelA   = rSkChunks.m_sharedToSkVrtx[triShared[1]];
        SkVrtxId const skelB   = rSkChunks.m_sharedToSkVrtx[triShared[2]];

        /*
        SkVrtxId const skelMid = rSkeleton.vrtx_ids().try_get(skelA, skelB);

        if (skelMid != lgrn::id_null<SkVrtxId>())
        {
            SharedVrtxId const sharedMid = rSkChunks.m_skVrtxToShared[skelMid];
            VertexIdx const    vrtxMid   = sharedOffset + sharedMid.value;

            emit_face(triVrtx[0], triVrtx[1], vrtxMid);
            emit_face(triVrtx[0], vrtxMid, triVrtx[2]);
        }
        else
        {
            emit_face(triVrtx[0], triVrtx[1], triVrtx[2]);
        }
        */
    }
}

template <typename FUNCTOR_T>
template <int side, bool detailX2>
void ChunkFanStitcher<FUNCTOR_T>::edge()
{
    int const fillWidth = rSkChunks.m_chunkEdgeVrtxCount - 2;

    int sharedLocalA    {};
    int sharedLocalB    {};
    int fillTriangular  {};
    int triRowSize      {};

    sharedLocalA = side*chunkWidth + 1;
    sharedLocalB = side*chunkWidth + 2;

    if constexpr (side == 0)
    {
        fillTriangular = xy_to_triangular(0, 0);
        triRowSize = 1;
    }
    else if constexpr (side == 1)
    {
        fillTriangular = xy_to_triangular(0, fillWidth-1);
        // triRowSize = don't care
    }
    else if constexpr (side == 2)
    {
        fillTriangular = xy_to_triangular(fillWidth-1, fillWidth-1);
        triRowSize = fillWidth;
    }

    for (int i = 0; i < fillWidth; ++i)
    {
        SharedVrtxId const sharedA = chunkSharedVertices[sharedLocalA].value();
        SharedVrtxId const sharedB = chunkSharedVertices[sharedLocalB].value();

        VertexIdx const vrtxA = sharedOffset + sharedA.value;
        VertexIdx const vrtxB = sharedOffset + sharedB.value;
        VertexIdx const vrtxC = chunkFillOffset + fillTriangular;

        if constexpr (detailX2)
        {
            //         A
            //        / \
            //       /   \
            //      /    MID
            //     /  _-'  \
            //    /_-'      \
            //   C'----------B

            SkVrtxId const skelA   = rSkChunks.m_sharedToSkVrtx[sharedA];
            SkVrtxId const skelB   = rSkChunks.m_sharedToSkVrtx[sharedB];

            /*
            SkVrtxId const skelMid = rSkeleton.vrtx_ids().try_get(skelA, skelB);

            if (skelMid != lgrn::id_null<SkVrtxId>())
            {
                SharedVrtxId const sharedMid = rSkChunks.m_skVrtxToShared[skelMid];
                VertexIdx const    vrtxMid   = sharedOffset + sharedMid.value;

                emit_face(vrtxA, vrtxMid, vrtxC);
                emit_face(vrtxC, vrtxMid, vrtxB);
            }
            else
            {
                emit_face(vrtxA, vrtxB, vrtxC);
            }
            */
        }
        else
        {
            emit_face(vrtxA, vrtxB, vrtxC);
        }

        ++sharedLocalA;
        ++sharedLocalB;

        if constexpr (side == 0)
        {
            fillTriangular += triRowSize;
            ++triRowSize;
        }
        else if constexpr (side == 1)
        {
            fillTriangular += 1;
        }
        else if constexpr (side == 2)
        {
            fillTriangular -= triRowSize;
            --triRowSize;
        }
    }
}

} // namespace planeta
