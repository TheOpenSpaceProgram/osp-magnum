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

    ID_T create();

    void remove(ID_T id);

    bool exists(ID_T id);

private:
    std::vector<bool> m_exists;
    std::vector<id_base_t> m_deleted;

}; // class IdRegistry

/**
 * @brief A multitree directed acyclic graph of reusable IDs where new IDs can
 *        be created from two other parent IDs.
 */
template<typename ID_T>
class SubdivTree
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

    ID_T add_root();

    ID_T add_child(ID_T a, ID_T b);

    ParentPair get_parents(ID_T a);

    ID_T get_by_parents(ID_T a, ID_T b);

private:
    std::map<ParentPair, id_base_t> m_parentsToId;
    std::vector<ParentPair> m_byId;

}; // class SubdivTree

enum class VrtxId : uint32_t {};
enum class TriId : uint32_t {};

struct Triangle
{
    std::array<VrtxId, 3> m_vertices;
    TriId m_parent;
    uint8_t m_refCount;
};

struct Vertex
{
    osp::Vector3 m_position;
    uint8_t m_refCount;
};

struct SubdivSkeleton
{
    IdRegistry<TriId> m_triIds;
    std::vector<Triangle> m_triData; // access using TriId from m_triIds

    SubdivTree<VrtxId> m_vrtxTree;
    std::vector<Vertex> m_vrtxData; // access using VrtxIds from m_vrtxTree
};


static SubdivSkeleton create_skeleton_icosahedron(float radius);

}
