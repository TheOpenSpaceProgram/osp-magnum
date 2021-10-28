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

#include <osp/id_registry.h>

#include <Corrade/Containers/ArrayViewStl.h>

#include <optional>
#include <limits>
#include <unordered_map>
#include <vector>

namespace planeta
{

template<typename T>
using ArrayView_t = Corrade::Containers::ArrayView<T>;

//-----------------------------------------------------------------------------

/**
 * @brief A multitree directed acyclic graph of reusable IDs where new IDs can
 *        be created from two other parent IDs.
 */
template<typename ID_T>
class SubdivIdTree : private osp::IdRegistry<ID_T>
{

    using base_t = osp::IdRegistry<ID_T>;
    using id_int_t = std::underlying_type_t<ID_T>;

    static_assert(std::is_integral<id_int_t>::value && sizeof(ID_T) <= 4,
                  "ID_T must be an integral type, 4 bytes or less in size");

    using combination_t = uint64_t;

public:

    using base_t::size_required;
    using base_t::capacity;
    using base_t::size;

    ID_T create_root()
    {
        ID_T const id = base_t::create();
        m_idChildCount.resize(size_required());
        m_idChildCount[size_t(id)] = 0;
        return id;
    };

    std::pair<ID_T, bool> create_or_get(ID_T a, ID_T b)
    {
        combination_t const combination = hash_id_combination(a, b);

        // Try emplacing a blank element under this combination of IDs, or get
        // existing element
        auto const& [it, success] = m_parentsToId.try_emplace(combination, 0);

        if (success)
        {
            // The space was free, and a new element was succesfully emplaced

            // Create a new ID for real, replacing the blank one from before
            it->second = id_int_t(create_root());

            // Keep track of the new ID's parents
            m_idToParents.resize(size_required());
            m_idToParents[it->second] = combination;

            // Increase child count of the two parents
            m_idChildCount[size_t(a)] ++;
            m_idChildCount[size_t(b)] ++;
        }

        return { ID_T(it->second), success };
    }

    ID_T get(ID_T a, ID_T b) const;

    std::pair<ID_T, ID_T> get_parents(ID_T a);

    void reserve(size_t n)
    {
        base_t::reserve(n);
        m_idToParents.reserve(base_t::capacity());
        m_idChildCount.reserve(base_t::capacity());
    }

    static constexpr combination_t
    hash_id_combination(id_int_t a, id_int_t b) noexcept
    {
        id_int_t const ls = (a > b) ? a : b;
        id_int_t const ms = (a > b) ? b : a;

        return combination_t(ls)
                | (combination_t(ms) << (sizeof(id_int_t) * 8));
    }

    static constexpr combination_t
    hash_id_combination(ID_T a, ID_T b) noexcept
    {
        return hash_id_combination( id_int_t(a), id_int_t(b) );
    }

private:
    std::unordered_map<combination_t, id_int_t> m_parentsToId;
    std::vector<combination_t> m_idToParents;
    std::vector<uint8_t> m_idChildCount;

}; // class SubdivTree

//-----------------------------------------------------------------------------

enum class SkVrtxId : uint32_t {};

using SkVrtxStorage_t = osp::IdRefCount<SkVrtxId>::Storage_t;

/**
 * @brief Uses a SubdivIdTree to manage relationships between Vertex IDs, and
 *        adds reference counting features.
 *
 * This class does NOT store vertex data like positions and normals.
 */
class SubdivSkeleton
{

public:


    SkVrtxId vrtx_create_root()
    {
        SkVrtxId const vrtxId = m_vrtxIdTree.create_root();
        return vrtxId;
    };

    SkVrtxId vrtx_create_or_get_child(SkVrtxId a, SkVrtxId b)
    {
        auto const [vrtxId, created] = m_vrtxIdTree.create_or_get(a, b);
        return vrtxId;
    }

    SkVrtxStorage_t vrtx_store(SkVrtxId vrtxId)
    {
        return m_vrtxRefCount.ref_add(vrtxId);
    }

    void vrtx_release(SkVrtxStorage_t &rStorage)
    {
        m_vrtxRefCount.ref_release(rStorage);
    }

    SubdivIdTree<SkVrtxId> vrtx_ids() const noexcept { return m_vrtxIdTree; }

    void vrtx_reserve(size_t n)
    {
        m_vrtxIdTree.reserve(n);
        m_vrtxRefCount.resize(m_vrtxIdTree.capacity());
    }

private:

    SubdivIdTree<SkVrtxId> m_vrtxIdTree;

    // access using VrtxIds from m_vrtxTree
    osp::IdRefCount<SkVrtxId> m_vrtxRefCount;

    std::vector<SkVrtxId> m_maybeDelete;

}; // class SubdivSkeleton

//-----------------------------------------------------------------------------

enum class SkTriId : uint32_t {};
enum class SkTriGroupId : uint32_t {};

using SkTriStorage_t = osp::IdRefCount<SkTriId>::Storage_t;

struct SkeletonTriangle
{
    // Vertices are ordered counter-clockwise, starting from top:
    // 0: Top   1: Left   2: Right
    //       0
    //      / \
    //     /   \
    //    /     \
    //   1 _____ 2
    //
    std::array<SkVrtxStorage_t, 3> m_vertices;

    std::optional<SkTriGroupId> m_children;

}; // struct SkeletonTriangle

// Skeleton triangles are added and removed in groups of 4
struct SkTriGroup
{
    // Subdivided triangles are arranged in m_triangles as followed:
    // 0: Top   1: Left   2: Right   3: Center
    //
    //        /\
    //       /  \
    //      / t0 \
    //     /______\
    //    /\      /\
    //   /  \ t3 /  \
    //  / t1 \  / t2 \
    // /______\/______\
    //
    // Center is upside-down, it's 'top' vertex is the bottom-middle one
    // This arrangement may not apply for root triangles.
    std::array<SkeletonTriangle, 4> m_triangles;

    SkTriId m_parent;
    uint8_t m_depth;
};

/**
 * @return Group ID of a SkeletonTriangle's group specified by Id
 */
constexpr SkTriGroupId tri_group_id(SkTriId id) noexcept
{
    return SkTriGroupId( uint32_t(id) / 4 );
}

/**
 * @return Sibling index of a SkeletonTriangle by Id
 */
constexpr uint8_t tri_sibling_index(SkTriId id) noexcept
{
    return uint32_t(id) % 4;
}

/**
 * @return Id of a SkeletonTriangle from it's group Id and sibling index
 */
constexpr SkTriId tri_id(SkTriGroupId id, uint8_t siblingIndex) noexcept
{
    return SkTriId(uint32_t(id) * 4 + siblingIndex);
}

/**
 * @brief A subdividable mesh with reference counted triangles and vertices;
 *        A SubdivSkeleton that also features triangles.
 *
 * This class does NOT store vertex data like positions and normals.
 */
class SubdivTriangleSkeleton : public SubdivSkeleton
{
public:

    SubdivTriangleSkeleton() = default;

    SubdivTriangleSkeleton(SubdivTriangleSkeleton const&) = delete;

    SubdivTriangleSkeleton(SubdivTriangleSkeleton&& move) = default;

    ~SubdivTriangleSkeleton()
    {
        m_triIds.for_each([this] (SkTriGroupId id)
        {
            for (SkeletonTriangle& rTri : m_triData[size_t(id)].m_triangles)
            {
                for (SkVrtxStorage_t& rVrtx : rTri.m_vertices)
                {
                    vrtx_release(rVrtx);
                }
            }
        });
    }

    std::array<SkVrtxId, 3> vrtx_create_middles(
            std::array<SkVrtxId, 3> const& vertices)
    {
        return {
            vrtx_create_or_get_child(vertices[0], vertices[1]),
            vrtx_create_or_get_child(vertices[1], vertices[2]),
            vrtx_create_or_get_child(vertices[2], vertices[0])
        };
    }

    void vrtx_create_chunk_edge_recurse(
            unsigned int level,
            SkVrtxId a, SkVrtxId b,
            ArrayView_t< SkVrtxId > rOut)
    {
        if (level == 0)
        {
            return;
        }

        SkVrtxId const mid = vrtx_create_or_get_child(a, b);
        size_t const halfSize = rOut.size() / 2;
        rOut[halfSize] = mid;
        vrtx_create_chunk_edge_recurse(level - 1, a, mid, rOut.prefix(halfSize));
        vrtx_create_chunk_edge_recurse(level - 1, mid, b, rOut.suffix(halfSize));
    }

    void tri_group_resize_fit_ids()
    {
        m_triData.resize(m_triIds.size_required());
        m_triRefCount.resize(m_triIds.size_required() * 4);
    }

    SkTriGroupId tri_group_create(
            uint8_t depth,
            SkTriId parent,
            std::array<std::array<SkVrtxId, 3>, 4> vertices)
    {
        SkTriGroupId const groupId = m_triIds.create();
        tri_group_resize_fit_ids();

        SkTriGroup &rGroup = m_triData[size_t(groupId)];
        rGroup.m_parent = parent;
        rGroup.m_depth = depth;

        for (int i = 0; i < 4; i ++)
        {
            SkeletonTriangle &rTri = rGroup.m_triangles[i];
            rTri.m_children = std::nullopt;
            rTri.m_vertices = {
                vrtx_store(vertices[i][0]),
                vrtx_store(vertices[i][1]),
                vrtx_store(vertices[i][2])
            };
        }
        return groupId;
    }

    SkeletonTriangle& tri_at(SkTriId triId)
    {
        auto groupIndex = size_t(tri_group_id(triId));
        uint8_t siblingIndex = tri_sibling_index(triId);
        return m_triData.at(groupIndex).m_triangles[siblingIndex];
    }

    constexpr osp::IdRegistry<SkTriGroupId> const& tri_ids() const { return m_triIds; }

    SkTriGroupId tri_subdiv(SkTriId triId, std::array<SkVrtxId, 3> vrtxMid);

    void tri_group_reserve(size_t n)
    {
        m_triIds.reserve(n);
        m_triData.reserve(m_triIds.capacity());
        m_triRefCount.resize(m_triIds.capacity() * 4);
    }

    SkTriStorage_t tri_store(SkTriId triId)
    {
        return m_triRefCount.ref_add(triId);
    }

    void tri_release(SkTriStorage_t &rStorage)
    {
        m_triRefCount.ref_release(rStorage);
    }

private:

    osp::IdRegistry<SkTriGroupId> m_triIds;
    osp::IdRefCount<SkTriId> m_triRefCount;

    // access using SkTriGroupId from m_triIds
    std::vector<SkTriGroup> m_triData;


}; // class SubdivTriangleSkeleton



}
