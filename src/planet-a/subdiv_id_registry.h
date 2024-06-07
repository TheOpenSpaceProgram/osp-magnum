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
 * @brief Features SubdivIdRegistry
 */
#pragma once

#include <osp/core/id_utils.h>

#include <longeron/id_management/registry_stl.hpp>

#include <cstdint>
#include <unordered_map>

namespace planeta
{

/**
 * @brief Manages unique sequential IDs within a graph, where IDs are created from two parent IDs.
 *
 * SubdivIdRegistry provides a way to represent the relationships between vertices in a triangle
 * mesh that is being subdivided to higher levels of detail. Vertices are best represented with an
 * integer ID. Each edge is a pair of two vertices. Simple subdivision involves splitting edges
 * into two new edges that share a new vertex in the middle. Hence, a pair of two 'parent' vertices
 * can be associated with a single 'child'. This forms a directed acyclic graph of IDs.
 *
 * SubdivIdRegistry features reference counting to allow IDs to stay alive while they're still
 * being used, and allowing them to be deleted in a random order. Deletion is not fully automatic
 * for (reasons) that actually simplifies users of this class.
 */
template<typename ID_T>
class SubdivIdRegistry : private lgrn::IdRegistryStl<ID_T>
{

    using base_t = lgrn::IdRegistryStl<ID_T>;
    using id_int_t = lgrn::underlying_int_type_t<ID_T>;

    static_assert(std::is_integral_v<id_int_t> && sizeof(ID_T) <= 4,
                  "ID_T must be an integral type, 4 bytes or less in size");

public:

    using base_t::base_t;
    using base_t::bitview;
    using base_t::capacity;
    using base_t::exists;
    using base_t::size;
    using base_t::begin;
    using base_t::end;

    /**
     * @brief Create a single ID with no parents
     *
     * Refcount is initially zero. Use refcount_increment afterwards.
     */
    ID_T create_root()
    {
        ID_T const id = base_t::create();
        m_idRefcount.resize(capacity());
        m_idRefcount[std::size_t(id)] = 0;
        m_idToParents.resize(capacity(), ~std::uint64_t(0));
        return id;
    };

    /**
     * @brief Create an ID from two parent IDs, or obtain it if it already exists
     *
     * Order of parents does not matter
     */
    osp::MaybeNewId<ID_T> create_or_get(ID_T a, ID_T b);

    /**
     * @brief Get child given two parents. Return null if not found
     *
     * Order of parents does not matter
     */
    [[nodiscard]] ID_T get(ID_T a, ID_T b) const noexcept
    {
        auto const found = m_parentsToId.find(id_pair_to_uint64(a, b));
        return found != m_parentsToId.end() ? ID_T(found->second) : lgrn::id_null<ID_T>();
    }

    /**
     * @brief Delete an ID once its refcount is zero
     *
     * This will recursively walk up the chain of parents and delete any with refcount = 0
     */
    void remove(ID_T x) noexcept;

    /**
     * @brief Reserve to fit at least n IDs
     *
     * @param n [in] Requested capacity
     */
    void reserve(std::size_t n)
    {
        base_t::reserve(n);
        m_idToParents.reserve(base_t::capacity());
        m_idRefcount.reserve(base_t::capacity());
    }

    osp::RefCountStatus<std::uint8_t> refcount_increment(ID_T x) noexcept
    {
        return { .refCount = ++(m_idRefcount[std::size_t(x)]) };
    }

    osp::RefCountStatus<std::uint8_t> refcount_decrement(ID_T x) noexcept
    {
        return { .refCount = --(m_idRefcount[std::size_t(x)]) };
    }

private:

    static constexpr std::uint64_t id_pair_to_uint64(ID_T const a, ID_T const b) noexcept
    {
        // Sort to make A and B order-independent
        auto const ls = id_int_t(std::min(a, b));
        auto const ms = id_int_t(std::max(a, b));

        // Concatenate as uint32 into a uint64
        return (std::uint64_t(ls) << 32) | std::uint64_t(ms);
    }

    struct IdPair
    {
        ID_T a;
        ID_T b;
    };

    static constexpr IdPair uint64_to_id_pair(std::uint64_t const combination)
    {
        return { ID_T(std::uint32_t(combination)), ID_T(std::uint32_t(combination >> 32)) };
    }

    std::unordered_map<std::uint64_t, id_int_t> m_parentsToId;
    std::vector<std::uint64_t>                  m_idToParents;
    std::vector<std::uint8_t>                   m_idRefcount;

}; // class SubdivTree


template<typename ID_T>
osp::MaybeNewId<ID_T> SubdivIdRegistry<ID_T>::create_or_get(ID_T const a, ID_T const b)
{
    std::uint64_t const combination = id_pair_to_uint64(a, b);

    // Try emplacing a blank element under this combination of IDs, or get existing element
    auto const& [it, newChildAdded] = m_parentsToId.try_emplace(combination, 0);

    if (newChildAdded)
    {
        // Create a new ID for real, replacing the blank one from before (it->second is a reference)
        it->second = id_int_t(create_root());

        // Keep track of the new ID's parents
        m_idToParents[it->second] = combination;

        refcount_increment(a);
        refcount_increment(b);
    }
    // else, an existing child was obtained instead

    return { ID_T(it->second), newChildAdded };
}


template<typename ID_T>
void SubdivIdRegistry<ID_T>::remove(ID_T const x) noexcept
{
    LGRN_ASSERTM(m_idRefcount[std::size_t(x)] == 0, "Can't remove vertex with non-zero refcount");

    std::uint64_t const combination = m_idToParents[std::size_t(x)];

    if (combination != ~std::uint64_t(0))
    {
        [[maybe_unused]] auto const numberOfElementsErased = m_parentsToId.erase(combination);
        LGRN_ASSERT(numberOfElementsErased != 0);

        // Parents lost a child, RIP
        auto const [parentA, parentB] = uint64_to_id_pair(combination);
        auto const statusA = refcount_decrement(parentA);
        auto const statusB = refcount_decrement(parentB);

        // Recursively delete parents
        if (statusA.refCount == 0)
        {
            remove(parentA);
        }

        if (statusB.refCount == 0)
        {
            remove(parentB);
        }
    }
    // else, we're deleting a root value

    base_t::remove(x);
}

}; // namespace planeta

