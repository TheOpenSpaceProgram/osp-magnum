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
 * @brief Utilities for managing chunk mesh buffers and writing faces
 *
 * Does not depend on types from geometry.h
 */
#pragma once

#include "planeta_types.h"
#include "skeleton.h"

#include <osp/core/math_types.h>


namespace planeta
{


/**
 * @brief Describes how a chunk mesh's vertex and index buffers are laid out
 */
struct ChunkMeshBufferInfo
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

    /// Number of non-shared vertices that fill the center of a chunk. Vertices that don't lie
    /// along the outer edges of their chunk are not shared with other chunks.
    std::uint32_t fillVrtxCount;

    /// Number of triangle faces needed to fill the center of the chunk. This includes faces
    /// with a single corner touching the edge. Faces with two shared vertices (a whole edge
    /// lining up along the chunk edge) are excluded.
    std::uint32_t fillFaceCount;

    /// Number of fan triangles per-chunk + extra triangles needed to fill a chunk edge with 2-triangle
    std::uint32_t fanMaxFaceCount;

    /// Max shared vertices used by fan triangles, including shared vertices from higher-detail
    /// neighbors through a detailX2 stitch
    std::uint32_t fanMaxSharedCount;

    /// Max total faces per chunk. fillFaceCount + fanMaxFaceCount
    std::uint32_t chunkMaxFaceCount;

    /// Total number of faces
    std::uint32_t faceTotal;

    /// Index of first fill vertex within in vertex buffer
    std::uint32_t vbufFillOffset;
    /// Index of first shared vertex within in vertex buffer
    std::uint32_t vbufSharedOffset;

    /// Total number of vertices
    std::uint32_t vrtxTotal;
};

constexpr ChunkMeshBufferInfo make_chunk_mesh_buffer_info(ChunkSkeleton const &skChunks)
{
    std::uint32_t const maxChunks     = std::uint32_t(skChunks.m_chunkIds.capacity());
    std::uint32_t const maxSharedVrtx = std::uint32_t(skChunks.m_sharedIds.capacity());

    std::uint32_t const chunkWidth        = skChunks.m_chunkEdgeVrtxCount;
    std::uint32_t const fillCount         = (chunkWidth-2)*(chunkWidth-1) / 2;
    std::uint32_t const fillTotal         = fillCount * maxChunks;
    std::uint32_t const fanFaceCount      = ChunkMeshBufferInfo::smc_fanFacesVsSubdivLevel[skChunks.m_chunkSubdivLevel];
    std::uint32_t const fillFaceCount     = chunkWidth*chunkWidth - fanFaceCount;
    std::uint32_t const fanMaxFaceCount   = fanFaceCount + fanFaceCount/3 + 1;
    std::uint32_t const chunkMaxFaceCount = fillFaceCount + fanMaxFaceCount;
    std::uint32_t const fanMaxSharedCount = fanMaxFaceCount + 4;

    return
    {
        .fillVrtxCount       = fillCount,
        .fillFaceCount       = fillFaceCount,
        .fanMaxFaceCount     = fanMaxFaceCount,
        .fanMaxSharedCount   = fanMaxSharedCount,
        .chunkMaxFaceCount   = chunkMaxFaceCount,
        .faceTotal           = maxChunks * chunkMaxFaceCount,
        .vbufFillOffset      = 0,
        .vbufSharedOffset    = fillTotal,
        .vrtxTotal           = std::uint32_t(fillTotal + maxSharedVrtx)
    };
}

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
constexpr std::uint32_t xy_to_triangular(std::uint32_t const x, std::uint32_t const y) noexcept
{
    return ( y * (y + 1) ) / 2 + x;
};

constexpr ChunkLocalSharedId coord_to_shared(
        std::uint16_t x, std::uint16_t y, std::uint16_t chunkWidth) noexcept
{
    // Tests if (x,y) lies along right, bottom, or left edges of triangle

    if (x == 0)         // Left
    {
        return ChunkLocalSharedId(y);
    }
    else if (y == chunkWidth) // Bottom
    {
        return ChunkLocalSharedId(chunkWidth + x);
    }
    else if (x == y)    // Right
    {
        return ChunkLocalSharedId(chunkWidth * 3 - x);
    }
    else                // Non-shared vertex
    {
        return {};
    }
}

constexpr VertexIdx fill_to_vrtx(ChunkMeshBufferInfo const& info, ChunkId const chunkId, std::uint32_t const triangular)
{
    return info.vbufFillOffset + info.fillVrtxCount * chunkId.value + triangular;
}

struct ReturnThingUvU
{
    ChunkLocalSharedId localShared;
    VertexIdx vertex;
};

constexpr ReturnThingUvU chunk_coord_to_vrtx(
        ChunkSkeleton         const &skChunks,
        ChunkMeshBufferInfo   const &info,
        ChunkId               const chunkId,
        std::uint16_t         const x,
        std::uint16_t         const y) noexcept
{
    ChunkLocalSharedId const localShared = coord_to_shared(x, y, skChunks.m_chunkEdgeVrtxCount);
    return {
        .localShared = localShared,
        .vertex      = localShared.has_value()
                     ? info.vbufSharedOffset + skChunks.shared_vertices_used(chunkId)[localShared.value].value().value
                     : fill_to_vrtx(info, chunkId, xy_to_triangular(x - 1, y - 2))
    };
}

//-----------------------------------------------------------------------------

/**
 * @brief Stores a procedure on which combinations of vertices need to be
 *        subdivided to calculate chunk fill vertices.
 */
class ChunkFillSubdivLUT
{
public:
    using Vector2us = Magnum::Math::Vector2<std::uint16_t>;


    struct ToSubdiv
    {
        // Both of these can either be a Fill vertex or ChunkLocalSharedId, depending on *IsShared
        std::uint32_t vrtxA;
        std::uint32_t vrtxB;

        std::uint32_t fillOut;

        bool aIsShared;
        bool bIsShared;
    };

    constexpr std::vector<ToSubdiv> const& data() const noexcept { return m_data; }

    friend ChunkFillSubdivLUT make_chunk_vrtx_subdiv_lut(std::uint8_t subdivLevel);

private:

    /**
     * @brief subdiv_edge_recurse
     *
     * @param a     [in]
     * @param b     [in]
     * @param level [in] Number of times this line can be subdivided further
     */
    void subdiv_line_recurse(Vector2us a, Vector2us b, std::uint8_t level);

    void fill_tri_recurse(Vector2us top, Vector2us lft, Vector2us rte, std::uint8_t level);

    std::vector<ToSubdiv> m_data;

    std::uint16_t m_fillVrtxCount{};
    std::uint16_t m_edgeVrtxCount{};

}; // class ChunkFillSubdivLUT

ChunkFillSubdivLUT make_chunk_vrtx_subdiv_lut(std::uint8_t subdivLevel);


//-----------------------------------------------------------------------------


template <typename T>
concept CFaceWriter = requires(T t, VertexIdx idx)
{
    t.fill_add_face(idx, idx, idx);
    t.fill_add_normal_filled(idx);
    t.fill_add_normal_shared(idx, ChunkLocalSharedId{});
    t.fan_add_face(idx, idx, idx);
    t.fan_add_normal_shared(idx, SharedVrtxId{});
};

enum class ECornerDetailX2 : int { None = 0, Right = 1, Left = 2 };

template <CFaceWriter WRITER_T>
struct ChunkFanStitcher
{
    template <int cornerIdx, ECornerDetailX2 detailX2>
    inline void corner() const;

    template <int side, bool detailX2>
    inline void edge() const;

    void stitch(ChunkStitch cmd) const;

    WRITER_T                            writer;

    osp::ArrayView<SharedVrtxOwner_t const> chunkSharedVertices;

    osp::ArrayView<SharedVrtxOwner_t const> detailX2Edge0;
    osp::ArrayView<SharedVrtxOwner_t const> detailX2Edge1;

    ChunkSkeleton                 const &rSkChunks;

    std::uint32_t                       sharedOffset;
    std::uint32_t                       chunkFillOffset;
    std::uint16_t                       chunkWidth;

}; // struct ChunkFanStitcher

template <CFaceWriter WRITER_T>
ChunkFanStitcher<WRITER_T> make_chunk_fan_stitcher(
        WRITER_T                                &&writer,
        ChunkId                           const chunk,
        osp::ArrayView<SharedVrtxOwner_t const> detailX2Edge0,
        osp::ArrayView<SharedVrtxOwner_t const> detailX2Edge1,
        ChunkSkeleton                     const &rSkChunks,
        ChunkMeshBufferInfo           const &rInfo)
{
    LGRN_ASSERT(detailX2Edge0.size() == detailX2Edge1.size());
    LGRN_ASSERT(detailX2Edge0.size() == 0 || detailX2Edge0.size() == rSkChunks.m_chunkEdgeVrtxCount);
    return ChunkFanStitcher<WRITER_T> {
        .writer                 = std::forward<WRITER_T>(writer),
        .chunkSharedVertices    = rSkChunks.shared_vertices_used(chunk),
        .detailX2Edge0          = detailX2Edge0,
        .detailX2Edge1          = detailX2Edge1,
        .rSkChunks              = rSkChunks,
        .sharedOffset           = rInfo.vbufSharedOffset,
        .chunkFillOffset        = rInfo.vbufFillOffset + std::uint32_t(chunk.value)*rInfo.fillVrtxCount,
        .chunkWidth             = rSkChunks.m_chunkEdgeVrtxCount
    };
}

template <CFaceWriter WRITER_T>
void ChunkFanStitcher<WRITER_T>::stitch(ChunkStitch const cmd) const
{
    using enum ECornerDetailX2;

    if (cmd.detailX2)
    {
        switch (cmd.x2ownEdge)
        {
        case 0: // DetailX2 Left edge
            corner <0, Left >();
            edge   <0, true >();
            corner <1, Right>();
            edge   <1, false>();
            corner <2, None >();
            edge   <2, false>();
            break;
        case 1: // DetailX2 Bottom edge
            corner <0, None >();
            edge   <0, false>();
            corner <1, Left >();
            edge   <1, true >();
            corner <2, Right>();
            edge   <2, false>();
            break;
        case 2: // DetailX2 Right edge
            corner <0, Right>();
            edge   <0, false>();
            corner <1, None >();
            edge   <1, false>();
            corner <2, Left >();
            edge   <2, true >();
            break;
        }
    }
    else // No DetailX2
    {
        corner <0, None>();
        edge   <0, false>();
        corner <1, None>();
        edge   <1, false>();
        corner <2, None>();
        edge   <2, false>();
    }
}



template <CFaceWriter WRITER_T>
template <int cornerIdx, ECornerDetailX2 detailX2>
void ChunkFanStitcher<WRITER_T>::corner() const
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
    if constexpr (cornerIdx == 0)
    {
        tri = { chunkWidth*3-1, chunkWidth*0+0, chunkWidth*0+1 };
    }
    else if constexpr (cornerIdx == 1)
    {
        tri = { chunkWidth*1-1, chunkWidth*1+0, chunkWidth*1+1 };
    }
    else if constexpr (cornerIdx == 2)
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
        writer.fan_add_face(triVrtx[0], triVrtx[1], triVrtx[2]);
        writer.fan_add_normal_shared(triVrtx[0], triShared[0]);
        writer.fan_add_normal_shared(triVrtx[1], triShared[1]);
        writer.fan_add_normal_shared(triVrtx[2], triShared[2]);
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

        SkVrtxId     const skelA    = rSkChunks.m_sharedToSkVrtx[triShared[1]];
        SkVrtxId     const skelB    = rSkChunks.m_sharedToSkVrtx[triShared[2]];
        SharedVrtxId const mid      = std::prev(detailX2Edge1.end())->value();
        VertexIdx    const vrtxMid  = sharedOffset + mid.value;

        writer.fan_add_face(triVrtx[0], triVrtx[1], vrtxMid);
        writer.fan_add_normal_shared(triVrtx[0], triShared[0]);
        writer.fan_add_normal_shared(triVrtx[1], triShared[1]);
        writer.fan_add_normal_shared(vrtxMid, mid);

        writer.fan_add_face(triVrtx[0], vrtxMid, triVrtx[2]);
        writer.fan_add_normal_shared(triVrtx[0], triShared[0]);
        writer.fan_add_normal_shared(vrtxMid, mid);
        writer.fan_add_normal_shared(triVrtx[2], triShared[2]);
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

        SkVrtxId     const skelA   = rSkChunks.m_sharedToSkVrtx[triShared[1]];
        SkVrtxId     const skelB   = rSkChunks.m_sharedToSkVrtx[triShared[2]];
        SharedVrtxId const mid     = std::next(detailX2Edge0.begin())->value();
        VertexIdx    const vrtxMid = sharedOffset + mid.value;

        writer.fan_add_face(triVrtx[2], vrtxMid, triVrtx[1]);
        writer.fan_add_normal_shared(triVrtx[2], triShared[2]);
        writer.fan_add_normal_shared(vrtxMid, mid);
        writer.fan_add_normal_shared(triVrtx[1], triShared[1]);

        writer.fan_add_face(triVrtx[2], triVrtx[0], vrtxMid);
        writer.fan_add_normal_shared(triVrtx[2], triShared[2]);
        writer.fan_add_normal_shared(triVrtx[0], triShared[0]);
        writer.fan_add_normal_shared(vrtxMid, mid);
    }
}

template <CFaceWriter WRITER_T>
template <int side, bool detailX2>
void ChunkFanStitcher<WRITER_T>::edge() const
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

    [[maybe_unused]] decltype(detailX2Edge0.begin()) detailX2It;

    auto const process = [&] ()
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

            SharedVrtxId const mid     = (*detailX2It).value();
            VertexIdx    const vrtxMid = sharedOffset + mid.value;

            writer.fan_add_face(vrtxA, vrtxMid, vrtxC);
            writer.fan_add_normal_shared(vrtxA, sharedA);
            writer.fan_add_normal_shared(vrtxMid, mid);

            writer.fan_add_face(vrtxC, vrtxMid, vrtxB);
            writer.fan_add_normal_shared(vrtxMid, mid);
            writer.fan_add_normal_shared(vrtxB, sharedB);

            std::advance(detailX2It, -2);
        }
        else
        {
            writer.fan_add_face(vrtxA, vrtxB, vrtxC);
            writer.fan_add_normal_shared(vrtxA, sharedA);
            writer.fan_add_normal_shared(vrtxB, sharedB);
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
    };

    if constexpr (detailX2)
    {
        detailX2It = std::prev(detailX2Edge1.end(), 3);
    }

    for (int i = 0; i < fillWidth/2; ++i)
    {
        process();
    }

    if constexpr (detailX2)
    {
        detailX2It = std::prev(detailX2Edge0.end());
    }

    for (int i = 0; i < fillWidth/2; ++i)
    {
        process();
    }
}


} // namespace planeta
