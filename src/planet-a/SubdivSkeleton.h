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

#include <osp/core/math_2pow.h>
#include <osp/core/array_view.h>
#include <osp/core/copymove_macros.h>
#include <osp/core/keyed_vector.h>
#include <osp/core/strong_id.h>

#include <longeron/id_management/registry_stl.hpp>
#include <longeron/id_management/refcount.hpp>

#include <array>
#include <unordered_map>
#include <vector>

namespace planeta
{

/**
 * @brief Concatenate bits of two uint32 ints into a uint64
 */
constexpr std::uint64_t concat_u32(std::uint32_t const lhs, std::uint32_t const rhs) noexcept
{
    return (std::uint64_t(lhs) << 32) | std::uint64_t(rhs);
}

//-----------------------------------------------------------------------------

template<typename ID_T>
struct MaybeNewId
{
    ID_T id;
    bool isNew;
};

/**
 * @brief A multitree directed acyclic graph of reusable IDs where new IDs can
 *        be created from two other parent IDs.
 */
template<typename ID_T>
class SubdivIdTree : private lgrn::IdRegistryStl<ID_T>
{

    using base_t = lgrn::IdRegistryStl<ID_T>;
    using id_int_t = lgrn::underlying_int_type_t<ID_T>;

    static_assert(std::is_integral_v<id_int_t> && sizeof(ID_T) <= 4,
                  "ID_T must be an integral type, 4 bytes or less in size");

public:

    using base_t::bitview;
    using base_t::capacity;
    using base_t::exists;
    using base_t::size;

    /**
     * @brief Create a single ID with no parents
     *
     * @return New ID created
     */
    ID_T create_root()
    {
        ID_T const id = base_t::create();
        m_idUsers.resize(capacity());
        m_idUsers[std::size_t(id)] = 0;
        return id;
    };

    /**
     * @brief Create an ID from two parent IDs
     *
     * Order of parents does not matter
     *
     * @param a [in] Parent A
     * @param b [in] Parent B
     *
     * @return New ID created
     */
    MaybeNewId<ID_T> create_or_get(ID_T const a, ID_T const b)
    {
        std::uint64_t const combination = id_pair_to_combination(a, b);

        // Try emplacing a blank element under this combination of IDs, or get
        // existing element
        auto const& [it, success] = m_parentsToId.try_emplace(combination, 0);

        if (success)
        {
            // The space was free, and a new element was succesfully emplaced

            // Create a new ID for real, replacing the blank one from before (it->second is a reference)
            it->second = id_int_t(create_root());

            // Keep track of the new ID's parents
            m_idToParents.resize(capacity());
            m_idToParents[it->second] = combination;

            // Increase use count of both parents as child is added
            user_increment(a);
            user_increment(b);
        }

        return { ID_T(it->second), success };
    }


    [[nodiscard]] ID_T get(ID_T a, ID_T b) const
    {
        std::uint64_t const combination = hash_id_combination(a, b);
        return ID_T(m_parentsToId.at(combination));
    }

    [[nodiscard]] ID_T try_get(ID_T a, ID_T b) const
    {
        std::uint64_t const combination = hash_id_combination(a, b);
        auto const found = m_parentsToId.find(combination);
        return found != m_parentsToId.end() ? ID_T(found->second) : lgrn::id_null<ID_T>();
    }

    // TODO

    std::pair<ID_T, ID_T> get_parents(ID_T a);

    void remove(ID_T x)
    {
        LGRN_ASSERTM(m_idUsers[std::size_t(x)] == 0, "Can't remove vertex with non-zero users");

        std::uint64_t const combination = m_idToParents[std::size_t(x)];
        [[maybe_unused]]
        std::size_t const erased = m_parentsToId.erase(combination);
        LGRN_ASSERT(erased != 0);

        auto const [a, b] = combination_to_id_pair(combination);

        if (user_decrement(a))
        {
            remove(a);
        }

        if (user_decrement(b))
        {
            remove(b);
        }

        base_t::remove(x);
    }

    /**
     * @brief Reserve to fit at least n IDs
     *
     * @param n [in] Requested capacity
     */
    void reserve(size_t n)
    {
        base_t::reserve(n);
        m_idToParents.reserve(base_t::capacity());
        m_idUsers.reserve(base_t::capacity());
    }

    void user_increment(ID_T x) noexcept
    {
        m_idUsers[std::size_t(x)] ++;
    }

    bool user_decrement(ID_T x) noexcept
    {
        m_idUsers[std::size_t(x)] --;
        return m_idUsers[std::size_t(x)] == 0;
    }

private:

    static constexpr std::uint64_t id_pair_to_combination(ID_T const a, ID_T const b) noexcept
    {
        // Sort to make A and B order-independent
        auto const ls = id_int_t(std::min(a, b));
        auto const ms = id_int_t(std::max(a, b));

        return concat_u32(ms, ls);
    }

    struct IdPair
    {
        ID_T a;
        ID_T b;
    };

    static constexpr IdPair combination_to_id_pair(std::uint64_t const combination)
    {
        return { ID_T(std::uint32_t(combination)), ID_T(std::uint32_t(combination >> 32)) };
    }

    std::unordered_map<std::uint64_t, id_int_t> m_parentsToId;
    std::vector<std::uint64_t>                  m_idToParents;
    std::vector<uint8_t>                        m_idUsers;

}; // class SubdivTree

//-----------------------------------------------------------------------------

enum class SkVrtxId : uint32_t {};

class SubdivTriangleSkeleton;

using SkVrtxOwner_t = lgrn::IdOwner<SkVrtxId, SubdivTriangleSkeleton>;

//-----------------------------------------------------------------------------

using SkTriId      = osp::StrongId<std::uint32_t, struct DummyForSkTriId>;
using SkTriGroupId = osp::StrongId<std::uint32_t, struct DummyForSkTriGroupId>;

using SkTriOwner_t = lgrn::IdRefCount<SkTriId>::Owner_t;

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
            LGRN_ASSERTM(neighbors[2].value() == neighbor, "Neighbor not found");
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
    uint8_t depth;
};

struct SkTriGroupPair
{
    SkTriGroupId id;
    SkTriGroup& rGroup;
};

/**
 * @return Group ID of a SkeletonTriangle's group specified by Id
 */
constexpr SkTriGroupId tri_group_id(SkTriId const id) noexcept
{
    return SkTriGroupId( uint32_t(id) / 4 );
}

/**
 * @return Sibling index of a SkeletonTriangle by Id
 */
constexpr uint8_t tri_sibling_index(SkTriId const id) noexcept
{
    return uint32_t(id) % 4;
}

/**
 * @return Id of a SkeletonTriangle from it's group Id and sibling index
 */
constexpr SkTriId tri_id(SkTriGroupId const id, uint8_t const siblingIndex) noexcept
{
    return SkTriId(uint32_t(id) * 4 + siblingIndex);
}

/**
 * @brief A subdividable mesh with reference counted triangles and vertices.
 *
 * This class...
 * * manages Vertex IDs (SkVrtxId) and Triangle IDs (SkTriId)
 * * tracks which 3 vertices make up each triangle
 * * subdivides triangles into 4 new triangles (parents are kept, tracks parent<->child tree)
 * * does NOT store vertex data like positions and normals
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
    std::array<MaybeNewId<SkVrtxId>, 3> vrtx_create_middles(
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
            SkVrtxId vrtxA, SkVrtxId vrtxB,
            osp::ArrayView< MaybeNewId<SkVrtxId> > rOut);


    [[nodiscard]] SkTriGroup const& tri_group_at(SkTriGroupId const group) const
    {
        return m_triData.at(group);
    }

    [[nodiscard]] SkTriGroup& tri_group_at(SkTriGroupId const group)
    {
        return m_triData.at(group);
    }

    /**
     * @brief Resize data to fit all possible IDs
     */
    void tri_group_resize_fit_ids()
    {
        m_triData.resize(m_triIds.capacity());
        m_triRefCount.resize(m_triIds.capacity() * 4);
    }

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
            uint8_t                                 depth,
            std::array<std::array<SkVrtxId, 3>, 4>  vertices);

    /**
     * @brief Reserve to fit at least n Triangle groups
     *
     * @param n [in] Requested capacity
     */
    void tri_group_reserve(size_t const n)
    {
        m_triIds.reserve(n);
        m_triData.reserve(m_triIds.capacity());
        m_triRefCount.resize(m_triIds.capacity() * 4);
    }

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
        return m_triData.at(tri_group_id(triId)).triangles[tri_sibling_index(triId)];
    }

    [[nodiscard]] SkeletonTriangle const& tri_at(SkTriId const triId) const
    {
        return m_triData.at(tri_group_id(triId)).triangles[tri_sibling_index(triId)];
    }

    /**
     * @return Read-only access to Triangle IDs
     */
    constexpr lgrn::IdRegistryStl<SkTriGroupId> const& tri_group_ids() const { return m_triIds; }

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
        return m_triData[tri_group_id(triId)].triangles[tri_sibling_index(triId)].children.has_value();
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
        SkVrtxId const vrtxId = m_vrtxIdTree.create_root();
        return vrtxId;
    };

    /**
     * @brief Create a single Vertex ID from two other Vertex IDs
     *
     * @return New Vertex ID created
     */
    MaybeNewId<SkVrtxId> vrtx_create_or_get_child(SkVrtxId const a, SkVrtxId const b)
    {
        return m_vrtxIdTree.create_or_get(a, b);
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
        LGRN_ASSERT(m_vrtxIdTree.exists(vrtxId));
        m_vrtxIdTree.user_increment(vrtxId);
        return SkVrtxOwner_t{vrtxId};
    }

    /**
     * @brief Safely cleares the contents of a Vertex ID storage, making it safe
     *        to destruct.
     *
     * @param rStorage [ref] Owner to release
     */
    bool vrtx_release(SkVrtxOwner_t&& rOwner)
    {
        LGRN_ASSERT(m_vrtxIdTree.exists(rOwner.value()));

        bool const noRefsRemaining = m_vrtxIdTree.user_decrement(rOwner.value());
        if (noRefsRemaining)
        {
            m_vrtxIdTree.remove(rOwner.value());
        }

        [[maybe_unused]] SkVrtxOwner_t moved = std::move(rOwner);
        moved.m_id = lgrn::id_null<SkVrtxId>();
        return noRefsRemaining;
    }

    /**
     * @return Read-only access to vertex IDs
     */
    SubdivIdTree<SkVrtxId> const& vrtx_ids() const noexcept { return m_vrtxIdTree; }

    /**
     * @brief Reserve to fit at least n Vertex IDs
     *
     * @param n [in] Requested capacity
     */
    void vrtx_reserve(std::size_t const n)
    {
        m_vrtxIdTree.reserve(n);
    }

private:

    SubdivIdTree<SkVrtxId>                  m_vrtxIdTree;
    lgrn::IdRegistryStl<SkTriGroupId>       m_triIds;
    lgrn::IdRefCount<SkTriId>               m_triRefCount;

    // access using SkTriGroupId from m_triIds
    osp::KeyedVec<SkTriGroupId, SkTriGroup> m_triData;
}; // class SubdivTriangleSkeleton


//-----------------------------------------------------------------------------


using ChunkId = osp::StrongId<uint16_t, struct DummyForChunkId>;
using SharedVrtxId = osp::StrongId<uint32_t, struct DummyForSharedVrtxId>;

using SharedVrtxOwner_t = lgrn::IdRefCount<SharedVrtxId>::Owner_t;

struct ChunkStitch
{
    bool          enabled        :1;
    bool          detailX2       :1;
    unsigned char x2ownEdge      :2;
    unsigned char x2neighborEdge :2;

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

    void chunk_reserve(uint16_t const size)
    {
        m_chunkIds.reserve(size);

        auto const realSize = m_chunkIds.capacity();
        m_chunkSharedUsed   .resize(realSize * m_chunkSharedCount);
        m_chunkToTri        .resize(realSize);
        m_chunkStitch       .resize(realSize, {} /* zero init */);
    }

    ChunkId chunk_create(
            SkTriId                                 sktriId,
            SubdivTriangleSkeleton&                 rSkel,
            std::vector<SharedVrtxId>&              rNewSharedIds,
            osp::ArrayView< MaybeNewId<SkVrtxId> >  edgeLft,
            osp::ArrayView< MaybeNewId<SkVrtxId> >  edgeBtm,
            osp::ArrayView< MaybeNewId<SkVrtxId> >  edgeRte);

    void chunk_remove(ChunkId chunkId, SkTriId sktriId, SubdivTriangleSkeleton& rSkel) noexcept;

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

    void shared_reserve(uint32_t const size)
    {
        m_sharedIds         .reserve(size);
        m_sharedToSkVrtx    .resize(m_sharedIds.capacity());
        m_sharedFaceCount   .resize(m_sharedIds.capacity());
    }

    SharedVrtxOwner_t shared_store(SharedVrtxId const triId)
    {
        return m_sharedRefCount.ref_add(triId);
    }

    void shared_release(SharedVrtxOwner_t&& rStorage, SubdivTriangleSkeleton &rSkel) noexcept
    {
        SharedVrtxId const sharedId = rStorage.value();
        m_sharedRefCount.ref_release(std::move(rStorage));

        if (m_sharedRefCount[sharedId.value] == 0)
        {
            SkVrtxOwner_t skvrtxOwner = std::exchange(m_sharedToSkVrtx[sharedId], {});
            m_skVrtxToShared[skvrtxOwner.value()] = {};

            rSkel.vrtx_release(std::move(skvrtxOwner));
            m_sharedIds.remove(sharedId);
        }
    }

    /**
     * @brief Create or get a shared vertex associated with a skeleton vertex
     *
     * @param skVrtxId
     * @return
     */
    MaybeNewId<SharedVrtxId> shared_get_or_create(SkVrtxId skVrtxId, SubdivTriangleSkeleton &rSkel);

    void clear(SubdivTriangleSkeleton& rSkel);

//private:

    lgrn::IdRegistryStl<ChunkId, true>      m_chunkIds;
    std::vector<SharedVrtxOwner_t>          m_chunkSharedUsed;
    std::uint8_t                            m_chunkSubdivLevel;
    std::uint16_t                           m_chunkEdgeVrtxCount;
    std::uint16_t                           m_chunkSharedCount;

    osp::KeyedVec<ChunkId, ChunkStitch>     m_chunkStitch;

    osp::KeyedVec<ChunkId, SkTriId>         m_chunkToTri;
    osp::KeyedVec<SkTriId, ChunkId>         m_triToChunk;

    lgrn::IdRegistryStl<SharedVrtxId, true> m_sharedIds;
    lgrn::IdRefCount<SharedVrtxId>          m_sharedRefCount;

    osp::KeyedVec<SharedVrtxId, SkVrtxOwner_t> m_sharedToSkVrtx;

    /// Connected face count used for vertex normal calculations
    osp::KeyedVec<SharedVrtxId, uint8_t>    m_sharedFaceCount;
    osp::KeyedVec<SkVrtxId, SharedVrtxId>   m_skVrtxToShared;

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
