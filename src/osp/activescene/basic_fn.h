/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
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

#include "basic.h"

#include "../core/array_view.h"
#include "../core/copymove_macros.h"

#include <algorithm>
#include <compare>

namespace osp::active
{

/**
 * @brief Helps add entities to a reserved space in ACtxSceneGraph
 *
 * All reserved space must be used, or else this class will assert on
 * destruction.
 */
class SubtreeBuilder
{
public:
    constexpr SubtreeBuilder(ACtxSceneGraph& rScnGraph, ActiveEnt root, TreePos_t first, TreePos_t last) noexcept
     : m_rScnGraph{rScnGraph}
     , m_root{root}
     , m_first{first}
     , m_last{last}
    { }
    OSP_MOVE_ONLY_CTOR_CONSTEXPR_NOEXCEPT(SubtreeBuilder)
    ~SubtreeBuilder() { assert(m_first == m_last); }

    /**
     * @brief Add a child that might have more descendants
     *
     * @return
     */
    [[nodiscard]] SubtreeBuilder add_child(ActiveEnt ent, uint32_t descendantCount);

    void add_child(ActiveEnt ent)
    {
        (void) add_child(ent, 0);
    }

    std::size_t remaining()
    {
        assert(m_last >= m_first);
        return m_last - m_first;
    }

private:
    ActiveEnt m_root;
    TreePos_t m_first;
    TreePos_t m_last;

    ACtxSceneGraph& m_rScnGraph;

}; // class SubtreeBuilder

/**
 * @brief Iterates entity children in an ACtxSceneGraph
 */
class ChildIterator
{
public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = ActiveEnt;
    using pointer           = void;
    using reference         = void;

    constexpr ChildIterator() noexcept = default;
    constexpr ChildIterator(ACtxSceneGraph const* pScnGraph, TreePos_t pos) noexcept
     : m_pScnGraph{pScnGraph}
     , m_pos{pos}
    { }
    constexpr ChildIterator(ChildIterator const& copy) noexcept = default;
    constexpr ChildIterator(ChildIterator&& move) noexcept = default;

    constexpr ChildIterator& operator=(ChildIterator const& copy) noexcept = default;
    constexpr ChildIterator& operator=(ChildIterator&& move) noexcept = default;

    ChildIterator& operator++() noexcept
    {
        m_pos += 1 + m_pScnGraph->m_treeDescendants[m_pos];
        return *this;
    }

    friend auto operator<=>(ChildIterator const& lhs, ChildIterator const& rhs) noexcept = default;

    value_type operator*() const noexcept
    {
        return m_pScnGraph->m_treeToEnt[m_pos];
    }

private:
    ACtxSceneGraph const    * m_pScnGraph   {nullptr};
    TreePos_t               m_pos           {0};

}; // class ChildIterator

using ChildRange_t = lgrn::IteratorPair<ChildIterator, ChildIterator>;

class SysSceneGraph
{
public:

    /**
     * @brief Add new entities to a scene graph using a SubtreeBuilder
     *
     * Root entity and number of descendants must be known beforehand
     */
    [[nodiscard]] static SubtreeBuilder add_descendants(ACtxSceneGraph& rScnGraph, uint32_t descendantCount, ActiveEnt root = lgrn::id_null<ActiveEnt>());

    /**
     * @return Iterable range of an entity's descendants
     */
    static ArrayView<ActiveEnt const> descendants(ACtxSceneGraph const& rScnGraph, ActiveEnt root);

    /**
     * @return Iterable range of an entity's descendants by tree position
     */
    static ArrayView<ActiveEnt const> descendants(ACtxSceneGraph const& rScnGraph, TreePos_t rootPos);


    /**
     * @return Iterable range of an entity's children
     */
    static ChildRange_t children(ACtxSceneGraph const& rScnGraph, ActiveEnt parent = lgrn::id_null<ActiveEnt>());

    /**
     * @brief Remove multiple entities from a scene graph
     *
     * @warning Do not include entities that are ancestors of any other entity
     *          in the same range. All given entities should be a root of a
     *          unique subtree.
     */
    template<typename ITA_T, typename ITB_T>
    static void cut(ACtxSceneGraph& rScnGraph, ITA_T first, ITB_T const& last);

    /**
     * @brief Add multiple entities and their descendents to a delete queue
     */
    template<typename ITA_T, typename ITB_T>
    static void queue_delete_entities(ACtxSceneGraph& rScnGraph, ActiveEntVec_t &rDelete, ITA_T const& first, ITB_T const& last);

private:

    static void do_delete(ACtxSceneGraph& rScnGraph);

}; // class SysSceneGraph

template<typename ITA_T, typename ITB_T>
void SysSceneGraph::cut(ACtxSceneGraph& rScnGraph, ITA_T first, ITB_T const& last)
{
    if (first == last)
    {
        return;
    }

    while (first != last)
    {
        TreePos_t const pos = rScnGraph.m_entToTreePos[*first];

        rScnGraph.m_delete.push_back(pos);

        std::advance(first, 1);
    }

    do_delete(rScnGraph);
}

template<typename ITA_T, typename ITB_T>
void SysSceneGraph::queue_delete_entities(ACtxSceneGraph& rScnGraph, ActiveEntVec_t &rDelete, ITA_T const& first, ITB_T const& last)
{
    std::for_each(first, last, [&] (ActiveEnt const ent)
    {
        rDelete.push_back(ent);
        for (ActiveEnt const descendent : SysSceneGraph::descendants(rScnGraph, ent))
        {
            rDelete.push_back(descendent);
        }
    });

    SysSceneGraph::cut(rScnGraph, first, last);
}

} // namespace osp::active

