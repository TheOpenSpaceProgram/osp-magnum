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
 * @brief Features SubdivTriangleSkeleton and ChunkSkeleton
 */
#pragma once

#include "subdiv_id_registry.h"

#include "planeta_types.h"

#include <osp/core/array_view.h>
#include <osp/core/copymove_macros.h>
#include <osp/core/keyed_vector.h>
#include <osp/core/math_2pow.h>

#include <longeron/id_management/id_set_stl.hpp>

#include <array>
#include <vector>

namespace planeta
{

inline constexpr std::size_t gc_maxSubdivLevels = 24;

struct SkeletonTriangle
{

    /**
     * Vertices are ordered counter-clockwise, starting from top:
     * 0: Top   1: Left   2: Right
     *       0
     *      / \
     *     /   \
     *    /     \
     *   1 _____ 2
     */
    std::array<SkVrtxOwner_t, 3>    vertices;

    /**
     * @brief Neighboring Skeleton triangles [left, bottom, right]; each can be null
     */
    std::array<SkTriOwner_t, 3>     neighbors;
    SkTriGroupId                    children;

    constexpr int find_neighbor_index(SkTriId const neighbor) const noexcept
    {
        if (neighbors[0].value() == neighbor)
        {
            return 0;
        }
        else if (neighbors[1].value() == neighbor)
        {
            return 1;
        }
        else
        {
            //LGRN_ASSERTM(neighbors[2].value() == neighbor, "Neighbor not found");
            return 2;
        }
    }

}; // struct SkeletonTriangle

/**
 * @brief Group of 4 Skeleton triangles (resulting from subdividing existing ones)
 *
 * Subdivided triangles are arranged in m_triangles as followed:
 *
 *   0: Top   1: Left   2: Right   3: Center
 *
 *          /\
 *         /  \
 *        / t0 \
 *       /______\
 *      /\      /\
 *     /  \ t3 /  \
 *    / t1 \  / t2 \
 *   /______\/______\
 *
 * Center is upside-down, it's 'top' vertex is the bottom-middle one
 * This arrangement may not apply for root triangles.
 */
struct SkTriGroup
{
    std::array<SkeletonTriangle, 4> triangles;

    SkTriId parent;
    uint8_t depth{};
};

struct SkTriGroupPair
{
    SkTriGroupId id;
    SkTriGroup& rGroup;
};

/**
 * @brief A subdividable mesh with reference counted triangles and vertices.
 *
 * This class...
 * * manages Vertex IDs (SkVrtxId) and Triangle IDs (SkTriId)
 * * tracks which 3 vertices make up each triangle
 * * subdivides triangles into 4 new triangles (parents are kept, tracks parent<->child tree)
 * * does NOT store vertex data like positions and normals
 *
 * Invariants must be followed in order to support seamless transitions between levels of detail:
 *
 * * Invariant A: Non-subdivided triangles can only neighbor ONE subdivided triangle.
 * * Invariant B: For each subdivided triangle neighboring a non-subdivided triangle, the
 *                subdivided triangle's two children neighboring the non-subdivided triangle
 *                must not be subdivided.
 *
 * Triangles are created in groups of 4 (SkTriGroupId) and cannot be individually created.
 */
class SubdivTriangleSkeleton
{
public:

    SubdivTriangleSkeleton() = default;

    OSP_MOVE_ONLY_CTOR_ASSIGN(SubdivTriangleSkeleton);

    ~SubdivTriangleSkeleton();

    /**
     * @brief Get or Create 3 Vertex IDs between the 3 other Vertex IDs
     *
     * This is a shorthand to calling vrtx_create_or_get_child 3 times for each
     * edge of a triangle; intended for subdivision.
     *
     * @param vertices [in] 3 Vertex IDs, representing corners of a triangle
     *
     * @return 3 newly created or obtained Vertex IDs
     */
    std::array<osp::MaybeNewId<SkVrtxId>, 3> vrtx_create_middles(
            std::array<SkVrtxId, 3> const& vertices)
    {
        return { vrtx_create_or_get_child(vertices[0], vertices[1]),
                 vrtx_create_or_get_child(vertices[1], vertices[2]),
                 vrtx_create_or_get_child(vertices[2], vertices[0]) };
    }

    /**
     * @brief Create or get a line up of Vertex IDs between two other Vertex IDs
     *
     * Given Vertex A and B, each call will create a vertex C by combining
     * A and B. If required, the function will recurse, calling itself twice
     * with (A, C), and (C, B) to create more vertices.
     *
     * Example:
     * Creating vertices between Vertex A to B, with level = 3, provided an
     * array view of 7 Vertex IDs from 0 to 6.
     *
     * Geometric representation:
     * A--0--1--2--3--4--5--6--B
     *
     * Recursive calls to fill array view:
     *
     * Level                         ID added
     * 3: ---> A+B                -> 3
     * 2:    |---> A+3            -> 1
     * 1:    |   |---> A+1        -> 0
     * 1:    |   |---> 1+3        -> 2
     * 2:    |---> 3+B            -> 5
     * 1:    |   |---> 3+5        -> 4
     * 1:    |   |---> 5+B        -> 6
     *
     * @param level [in] Number of times to subdivide further
     * @param a     [in] First Vertex ID
     * @param b     [in] Second Vertex ID
     * @param rOut  [out] Output Vertex IDs, size must be 2^level-1
     */
    void vrtx_create_chunk_edge_recurse(
            std::uint8_t level,
            SkVrtxId vrtxA,
            SkVrtxId vrtxB,
            osp::ArrayView< osp::MaybeNewId<SkVrtxId> > rOut);


    [[nodiscard]] SkTriGroup const& tri_group_at(SkTriGroupId const group) const
    {
        return m_triGroupData.at(group);
    }

    [[nodiscard]] SkTriGroup& tri_group_at(SkTriGroupId const group)
    {
        return m_triGroupData.at(group);
    }

    /**
     * @brief Resize data to fit all possible IDs
     */
    void tri_group_resize_fit_ids();

    /**
     * @brief Create a triangle group (4 new triangles)
     *
     * @return New Triangle Group ID
     */
    SkTriGroupPair tri_group_create(
            uint8_t                                 depth,
            SkTriId                                 parentId,
            SkeletonTriangle&                       rParent,
            std::array<std::array<SkVrtxId, 3>, 4>  vertices);

    SkTriGroupPair tri_group_create_root(
            std::uint8_t                            depth,
            std::array<std::array<SkVrtxId, 3>, 4>  vertices);

    /**
     * @brief Reserve to fit at least n Triangle groups
     *
     * @param n [in] Requested capacity
     */
    void tri_group_reserve(std::size_t const n) { m_triGroupIds.reserve(n); }

    struct SkTriGroupNeighboring
    {
        SkTriGroupId id;
        SkTriGroup&  rGroup;
        int          edge;
    };

    struct SkTriGroupEdge
    {
        SkTriOwner_t        &rNeighborA;
        SkTriOwner_t        &rNeighborB;
        SkTriId             childA;
        SkTriId             childB;
    };

    struct NeighboringEdges
    {
        SkTriGroupEdge lhs;
        SkTriGroupEdge rhs;
    };

    NeighboringEdges tri_group_set_neighboring(SkTriGroupNeighboring lhs, SkTriGroupNeighboring rhs);

    /**
     * @return Triangle data from ID
     */
    [[nodiscard]] SkeletonTriangle& tri_at(SkTriId const triId)
    {
        return m_triGroupData.at(tri_group_id(triId)).triangles[tri_sibling_index(triId)];
    }

    [[nodiscard]] SkeletonTriangle const& tri_at(SkTriId const triId) const
    {
        return m_triGroupData.at(tri_group_id(triId)).triangles[tri_sibling_index(triId)];
    }

    /**
     * @return Read-only access to Triangle IDs
     */
    constexpr lgrn::IdRegistryStl<SkTriGroupId> const& tri_group_ids() const { return m_triGroupIds; }

    /**
     * @brief Subdivide a triangle, creating a new group (4 new triangles)
     *
     * @param triId   [in] ID of Triangle to subdivide
     * @param vrtxMid [in] Vertex IDs between each corner of the triangle
     *
     * @return New Triangle Group ID
     */
    SkTriGroupPair tri_subdiv(SkTriId triId, SkeletonTriangle &rTri, std::array<SkVrtxId, 3> vrtxMid);

    bool is_tri_subdivided(SkTriId const triId) const
    {
        return m_triGroupData[tri_group_id(triId)].triangles[tri_sibling_index(triId)].children.has_value();
    }

    void tri_unsubdiv(SkTriId triId, SkeletonTriangle &rTri);

    /**
     * @brief Store a Triangle ID in ref-counted long term storage
     *
     * @return Triangle
     */
    [[nodiscard]] SkTriOwner_t tri_store(SkTriId const triId)
    {
        return m_triRefCount.ref_add(triId);
    }

    /**
     * @brief Safely cleares the contents of a Triangle ID storage, making it
     *        safe to destruct.
     *
     * @param rStorage [ref] Storage to release
     */
    void tri_release(SkTriOwner_t&& rStorage)
    {
        m_triRefCount.ref_release(std::move(rStorage));
    }

    /**
     * @brief Create a single Vertex ID with no parents
     *
     * @return New Vertex ID created
     */
    SkVrtxId vrtx_create_root()
    {
        SkVrtxId const vrtxId = m_vertexIds.create_root();
        return vrtxId;
    };

    /**
     * @brief Create a single Vertex ID from two other Vertex IDs
     *
     * @return New Vertex ID created
     */
    osp::MaybeNewId<SkVrtxId> vrtx_create_or_get_child(SkVrtxId const a, SkVrtxId const b)
    {
        return m_vertexIds.create_or_get(a, b);
    }

    /**
     * @brief Store a Vertex ID in ref-counted long term storage
     *
     * @param vrtxId [in] ID to store
     *
     * @return Vertex ID Storage
     */
    SkVrtxOwner_t vrtx_store(SkVrtxId const vrtxId)
    {
        LGRN_ASSERT(m_vertexIds.exists(vrtxId));
        m_vertexIds.refcount_increment(vrtxId);
        return SkVrtxOwner_t{vrtxId};
    }

    /**
     * @brief Safely cleares the contents of a Vertex ID owner, making it safe to destruct.
     */
    osp::RefCountStatus<std::uint8_t> vrtx_release(SkVrtxOwner_t&& rOwner)
    {
        LGRN_ASSERT(m_vertexIds.exists(rOwner.value()));

        auto const status = m_vertexIds.refcount_decrement(rOwner.value());
        if (status.refCount == 0)
        {
            m_vertexIds.remove(rOwner.value());
        }

        [[maybe_unused]] SkVrtxOwner_t moved = std::move(rOwner);
        moved.m_id = lgrn::id_null<SkVrtxId>();
        return status;
    }

    /**
     * @return Read-only access to vertex IDs
     */
    SubdivIdRegistry<SkVrtxId> const& vrtx_ids() const noexcept { return m_vertexIds; }

    /**
     * @brief Reserve to fit at least n Vertex IDs
     *
     * @param n [in] Requested capacity
     */
    void vrtx_reserve(std::size_t const n)
    {
        m_vertexIds.reserve(n);
    }

    void debug_check_invariants();


public:

    struct Level
    {
        /// Subdivided triangles that neighbor a non-subdivided one
        lgrn::IdSetStl<SkTriId> hasNonSubdivedNeighbor;

        /// Non-subdivided triangles that neighbor a subdivided one
        lgrn::IdSetStl<SkTriId> hasSubdivedNeighbor;
    };

    std::array<Level, gc_maxSubdivLevels> levels;
    std::uint8_t levelMax {7};

private:

    SubdivIdRegistry<SkVrtxId>              m_vertexIds;
    lgrn::IdRegistryStl<SkTriGroupId>       m_triGroupIds;
    lgrn::IdRefCount<SkTriId>               m_triRefCount;

    // access using SkTriGroupId from m_triGroupIds
    osp::KeyedVec<SkTriGroupId, SkTriGroup> m_triGroupData;
}; // class SubdivTriangleSkeleton


//-----------------------------------------------------------------------------

/**
 * @brief Describes how the edges of a chunk are stitched together with its neighbors.
 *
 * Chunks that share an edge with higher detail chunks must have a 'detailX2' stitch so that
 * whoever is building the chunk mesh's 'Fan' triangles (ChunkFanStitcher) can generate a smooth
 * transition between low and high detail.
 */
struct ChunkStitch
{
    bool          enabled        :1;
    bool          detailX2       :1;
    unsigned char x2ownEdge      :2; ///< [left, bottom, right]
    unsigned char x2neighborEdge :2; ///< [left, bottom, right]

    friend bool operator==(ChunkStitch const& lhs, ChunkStitch const& rhs) = default;
    friend bool operator!=(ChunkStitch const& lhs, ChunkStitch const& rhs) = default;
};

// This is a suggestion
//static_assert(sizeof(ChunkStitch) == 1);

/**
 * @brief Manages 'chunks' within a SubdivTriangleSkeleton
 *
 * Chunks are triangle grid patched over a skeleton triangle, forming a smooth high-detail terrain
 * surface for physics colliders and/or rendering. This is where a heightmap can be applied.
 *
 * Chunks are created by repeatedly subdividing an initial triangle; detail is controlled by a
 * subdivision level integer from 0 to 9.
 *
 * To allow neighboring chunks to share vertices, skeleton vertices are needed along the edges of
 * chunks. This can be created by repeatedly subdividing the edges of a skeleton triangle
 * (with vrtx_create_chunk_edge_recurse). These skeleton vertices are required to create a chunk,
 * and are also owned and ref-counted by the chunk.
 *
 * To associate skeleton vertices with a vertex buffer, vertices used by chunks are assigned with
 * with a 'Shared vertex' (SharedVrtxId). This decouples skeleton vertex IDs from the vertex buffer,
 *
 * This class...
 * * manages Chunk IDs (ChunkId) and the shared/skeleton vertices they use
 * * manages Shared Vertex IDs (SharedVrtxId)
 * * owns/ref-counts skeleton vertices within a SubdivTriangleSkeleton
 * * does NOT store vertex data like positions and normals
 */
class ChunkSkeleton
{
public:

    /**
     * @brief Allocate space enough space for AT LEAST certain number of chunks.
     *
     * Real capacity won't match specified size, check m_chunkIds.capacity() afterwards.
     */
    void chunk_reserve(std::uint16_t const size)
    {
        m_chunkIds.reserve(size);

        auto const realSize = m_chunkIds.capacity();
        m_chunkSharedUsed   .resize(realSize * m_chunkSharedCount);
        m_chunkToTri        .resize(realSize);
        m_chunkStitch       .resize(realSize, {} /* zero init */);
    }

    ChunkId chunk_create(
            SkTriId                                     sktriId,
            SubdivTriangleSkeleton                      &rSkel,
            lgrn::IdSetStl<SharedVrtxId>                &rSharedAdded,
            osp::ArrayView< osp::MaybeNewId<SkVrtxId> > edgeLft,
            osp::ArrayView< osp::MaybeNewId<SkVrtxId> > edgeBtm,
            osp::ArrayView< osp::MaybeNewId<SkVrtxId> > edgeRte);

    void chunk_remove(ChunkId chunkId, SkTriId sktriId, lgrn::IdSetStl<SharedVrtxId> &rSharedRemoved, SubdivTriangleSkeleton& rSkel) noexcept;

    /**
     * @brief Get shared vertices used by a chunk
     *
     * This array view is split into 3 sections, each are (m_chunkSharedCount) elements in size
     *
     * { left edge (corner 0->1)...  bottom edge (corner 1->2)...  right edge (corner 2->0)... }
     *
     * Each edge starts with the corner, then the rest of the vertices along the edge, excluding
     * the corner of the next edge.
     *
     * Corner orders are from SkeletonTriangle: {top, left, right}
     */
    osp::ArrayView<SharedVrtxOwner_t> shared_vertices_used(ChunkId const chunkId) noexcept
    {
        std::size_t const offset = std::size_t(chunkId) * m_chunkSharedCount;
        return {&m_chunkSharedUsed[offset], m_chunkSharedCount};
    }

    /**
     * @copydoc shared_vertices_used
     */
    osp::ArrayView<SharedVrtxOwner_t const> shared_vertices_used(ChunkId const chunkId) const noexcept
    {
        std::size_t const offset = std::size_t(chunkId) * m_chunkSharedCount;
        return {&m_chunkSharedUsed[offset], m_chunkSharedCount};
    }

    /**
     * @brief Allocate space enough space for AT LEAST certain number of shared vertices.
     *
     * Real capacity won't match specified size, check m_sharedIds.capacity() afterwards.
     */
    void shared_reserve(uint32_t const size)
    {
        m_sharedIds         .reserve(size);
        m_sharedToSkVrtx    .resize(m_sharedIds.capacity());
    }

    SharedVrtxOwner_t shared_store(SharedVrtxId const triId)
    {
        return m_sharedRefCount.ref_add(triId);
    }

    osp::RefCountStatus<std::uint16_t> shared_release(SharedVrtxOwner_t&& rStorage, SubdivTriangleSkeleton &rSkel) noexcept
    {
        SharedVrtxId const sharedId = rStorage.value();
        m_sharedRefCount.ref_release(std::move(rStorage));
        auto const refCount    = m_sharedRefCount[sharedId.value];

        if (refCount == 0)
        {
            SkVrtxOwner_t skvrtxOwner = std::exchange(m_sharedToSkVrtx[sharedId], {});
            m_skVrtxToShared[skvrtxOwner.value()] = {};

            rSkel.vrtx_release(std::move(skvrtxOwner));
            m_sharedIds.remove(sharedId);
        }

        return { refCount };
    }

    /**
     * @brief Create or get a shared vertex associated with a skeleton vertex
     */
    osp::MaybeNewId<SharedVrtxId> shared_get_or_create(SkVrtxId skVrtxId, SubdivTriangleSkeleton &rSkel);

    void clear(SubdivTriangleSkeleton& rSkel);

//private:

    lgrn::IdRegistryStl<ChunkId, true>          m_chunkIds;
    std::vector<SharedVrtxOwner_t>              m_chunkSharedUsed;
    std::uint8_t                                m_chunkSubdivLevel{};
    std::uint16_t                               m_chunkEdgeVrtxCount{};
    std::uint16_t                               m_chunkSharedCount{};

    osp::KeyedVec<ChunkId, ChunkStitch>         m_chunkStitch;

    osp::KeyedVec<ChunkId, SkTriId>             m_chunkToTri;
    osp::KeyedVec<SkTriId, ChunkId>             m_triToChunk;

    lgrn::IdRegistryStl<SharedVrtxId, true>     m_sharedIds;
    lgrn::IdRefCount<SharedVrtxId>              m_sharedRefCount;

    osp::KeyedVec<SharedVrtxId, SkVrtxOwner_t>  m_sharedToSkVrtx;
    osp::KeyedVec<SkVrtxId, SharedVrtxId>       m_skVrtxToShared;

}; // class ChunkSkeleton

inline ChunkSkeleton make_skeleton_chunks(uint8_t const subdivLevels)
{
    std::uint16_t const chunkEdgeVrtxCount = 1u << subdivLevels;
    return {
        .m_chunkSubdivLevel     = subdivLevels,
        .m_chunkEdgeVrtxCount   = chunkEdgeVrtxCount,
        .m_chunkSharedCount = std::uint16_t(chunkEdgeVrtxCount * 3)
    };
}

}
