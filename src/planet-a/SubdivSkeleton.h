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

#include <osp/types.h>

#include <Corrade/Containers/ArrayViewStl.h>

#include <cstdint>
#include <unordered_map>
#include <optional>
#include <type_traits>
#include <vector>

namespace planeta
{

/**
 * @brief Generates reusable sequential IDs
 */
template<typename ID_T>
class IdRegistry
{
    using id_int_t = std::underlying_type_t<ID_T>;

public:
    ID_T create();

    /**
     * @return Array size required to fit all currently existing IDs
     */
    id_int_t size_required() const noexcept { return m_exists.size(); }

    void remove(ID_T id);

    bool exists(ID_T id) const noexcept;

private:
    std::vector<bool> m_exists; // this guy is weird
    std::vector<id_int_t> m_deleted;

}; // class IdRegistry

template<typename ID_T>
ID_T IdRegistry<ID_T>::create()
{
    // Attempt to reuse a deleted ID
    if ( ! m_deleted.empty())
    {
        id_int_t const id = m_deleted.back();
        m_deleted.pop_back();
        m_exists[id] = true;
        return ID_T(id);
    }

    // Create a new Id
    id_int_t const id = m_exists.size();
    m_exists.push_back(true);
    return ID_T(id);
}

//-----------------------------------------------------------------------------

/**
 * @brief A multitree directed acyclic graph of reusable IDs where new IDs can
 *        be created from two other parent IDs.
 */
template<typename ID_T>
class SubdivIdTree : private IdRegistry<ID_T>
{
    using id_int_t = std::underlying_type_t<ID_T>;

    static_assert(std::is_integral<id_int_t>::value && sizeof(ID_T) <= 4,
                  "ID_T must be an integral type, 4 bytes or less in size");

    using combination_t = uint64_t;

public:

    using IdRegistry<ID_T>::size_required;


    ID_T create_root()
    {
        ID_T const id = IdRegistry<ID_T>::create();
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

enum class SkTriId : uint32_t {};
enum class SkTriGroupId : uint32_t {};

struct SkeletonTriangle
{
    std::array<SkVrtxId, 3> m_vertices;

    SkTriId m_parent;
    std::optional<SkTriGroupId> m_children;

    uint8_t m_refCount{0};

}; // struct SkeletonTriangle

// Skeleton triangles are added and removed in groups of 4
// 0:Top 1:Left 2:Right 3:Center
using SkeletonTriangleGroup_t = std::array<SkeletonTriangle, 4>;

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

//constexpr std::array<SkTriId, 4> tri_ids(SkTriGroupId id)
//{
//    return {tri_id(id, 0), tri_id(id, 1), tri_id(id, 2), tri_id(id, 3)};
//}

//-----------------------------------------------------------------------------

class SubdivTriangleSkeleton
{
    template<typename T>
    using ArrayView_t = Corrade::Containers::ArrayView<T>;

public:

    SkVrtxId vrtx_create_root()
    {
        SkVrtxId const vrtxId = m_vrtxIdTree.create_root();
        vrtx_resize_fit_ids();
        m_vrtxRefCount[size_t(vrtxId)] = 0;
        return vrtxId;
    };

    SkVrtxId vrtx_create_or_get_child(SkVrtxId a, SkVrtxId b)
    {
        auto const [vrtxId, created] = m_vrtxIdTree.create_or_get(a, b);
        if (created)
        {
            vrtx_resize_fit_ids();
            m_vrtxRefCount[size_t(vrtxId)] = 0;
        }
        return vrtxId;
    }

    void vrtx_resize_fit_ids()
    {
        m_vrtxRefCount.resize(m_vrtxIdTree.size_required());
        m_vrtxPositions.resize(m_vrtxIdTree.size_required());
        m_vrtxNormals.resize(m_vrtxIdTree.size_required());
    }

    ArrayView_t<uint8_t> vrtx_get_refcounts() { return m_vrtxRefCount; };
    ArrayView_t<osp::Vector3> vrtx_get_positions() { return m_vrtxPositions; };
    ArrayView_t<osp::Vector3> vrtx_get_normals() { return m_vrtxNormals; };

    void tri_group_resize_fit_ids()
    {
        m_triData.resize(m_triIds.size_required());
    }

    SkTriGroupId tri_group_create_root()
    {
        SkTriGroupId const id = m_triIds.create();
        tri_group_resize_fit_ids();
        m_triData[size_t(id)] = {};
        return id;
    }

    SkeletonTriangle& tri_at(SkTriId id)
    {
        auto groupIndex = size_t(tri_group_id(id));
        uint8_t siblingIndex = tri_sibling_index(id);
        return m_triData.at(groupIndex)[siblingIndex];
    }

private:
    IdRegistry<SkTriGroupId> m_triIds;

    // access using SkTriGroupId from m_triIds
    std::vector<SkeletonTriangleGroup_t> m_triData;

    SubdivIdTree<SkVrtxId> m_vrtxIdTree;

    // access using VrtxIds from m_vrtxTree
    std::vector<uint8_t> m_vrtxRefCount;
    std::vector<osp::Vector3> m_vrtxPositions;
    std::vector<osp::Vector3> m_vrtxNormals;

    std::vector<SkVrtxId> m_maybeDelete;

}; // class SubdivTriangleSkeleton

//

struct SkTriToSubdivide
{

};


SubdivTriangleSkeleton create_skeleton_icosahedron(float radius);

}
