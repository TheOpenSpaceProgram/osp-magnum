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

#include <map>
#include <type_traits>
#include <vector>
#include <cstdint>

namespace planeta
{

/**
 * @brief Generates reusable sequential IDs
 */
template<typename ID_T>
class IdRegistry
{
    using id_base_t = std::underlying_type_t<ID_T>;

public:
    ID_T create();

    /**
     * @return Array size required to fit all currently existing IDs
     */
    id_base_t size_required() const noexcept { return m_exists.size(); }

    void remove(ID_T id);

    bool exists(ID_T id) const noexcept;

private:
    std::vector<bool> m_exists; // this guy is weird
    std::vector<id_base_t> m_deleted;

}; // class IdRegistry

template<typename ID_T>
ID_T IdRegistry<ID_T>::create()
{
    if (m_deleted.empty())
    {
        id_base_t const id = m_exists.size();
        m_exists.push_back(true);
        return ID_T(id);
    }

    id_base_t const id = m_deleted.back();
    m_deleted.pop_back();
    m_exists[id] = true;
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
    using id_base_t = std::underlying_type_t<ID_T>;

    struct ParentPair
    {
        constexpr ParentPair(id_base_t a, id_base_t b)
         : m_a{ (a > b) ? a : b }
         , m_b{ (a > b) ? b : a }
        { }

        id_base_t const m_a;
        id_base_t const m_b;
    };

public:

    using IdRegistry<ID_T>::size_required;

    ID_T add_root() { return IdRegistry<ID_T>::create(); };

    ID_T add_child(ID_T a, ID_T b);

    ParentPair get_parents(ID_T a);

    ID_T get_by_parents(ID_T a, ID_T b);

private:
    std::map<ParentPair, id_base_t> m_parentsToId;
    std::vector<ParentPair> m_idToParents;

}; // class SubdivTree



//-----------------------------------------------------------------------------

enum class SkVrtxId : uint32_t {};
enum class SkTriId : uint32_t {};

struct SkeletonTriangle
{
    std::array<SkVrtxId, 3> m_vertices;
    SkTriId m_parent;
};

struct SubdivTriangleSkeleton
{
    void vrtx_resize_fit_ids()
    {
        m_vrtxPositions.resize(m_vrtxIdTree.size_required());
    }

    void tri_resize_fit_ids()
    {
        m_triData.resize(m_triIds.size_required());
    }

    SkeletonTriangle& tri_at(SkTriId id) { return m_triData.at(size_t(id)); }

    IdRegistry<SkTriId> m_triIds;
    std::vector<SkeletonTriangle> m_triData; // access using TriId from m_triIds

    SubdivIdTree<SkVrtxId> m_vrtxIdTree;
    std::vector<osp::Vector3> m_vrtxPositions; // access using VrtxIds from m_vrtxTree
};


SubdivTriangleSkeleton create_skeleton_icosahedron(float radius);

}
