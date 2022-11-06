/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
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
#include "activetypes.h"

#include <algorithm>

namespace osp::active
{

class SubtreeBuilder
{
public:
    constexpr SubtreeBuilder(ACtxSceneGraph& rScnGraph, ActiveEnt root, TreePos_t first, TreePos_t last)
     : m_rScnGraph{rScnGraph}
     , m_root{root}
     , m_first{first}
     , m_last{last}
    { }
    constexpr SubtreeBuilder(SubtreeBuilder&& move) = default;
    SubtreeBuilder(SubtreeBuilder const& copy) = delete;
    ~SubtreeBuilder() { assert(m_first == m_last); }

    [[nodiscard]] SubtreeBuilder add_child(ActiveEnt ent, uint32_t descendentCount);

    void add_child(ActiveEnt ent)
    {
        (void) add_child(ent, 0);
    }

    std::size_t remaining()
    {
        return m_last - m_first;
    }

    struct ManualAdd
    {
        ACtxSceneGraph& m_rScnGraph;
        ActiveEnt m_root;
        TreePos_t m_first;
        TreePos_t m_last;
    };

    ManualAdd manual()
    {
        ManualAdd out{ m_rScnGraph, m_root, m_first, m_last };
        m_first = m_last;
        return out;
    };

private:
    ActiveEnt m_root;
    TreePos_t m_first;
    TreePos_t m_last;

    ACtxSceneGraph& m_rScnGraph;
}; // class SubtreeBuilder

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

    constexpr ChildIterator& operator++() noexcept
    {
        m_pos += 1 + m_pScnGraph->m_treeDescendants[m_pos];
        return *this;
    }

    constexpr friend bool operator==(ChildIterator const& lhs,
                                     ChildIterator const& rhs) noexcept
    {
        return (lhs.m_pScnGraph == rhs.m_pScnGraph) && (lhs.m_pos == rhs.m_pos);
    };

    constexpr friend bool operator!=(ChildIterator const& lhs,
                                     ChildIterator const& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    constexpr value_type operator*() const noexcept
    {
        return m_pScnGraph->m_treeToEnt[m_pos];
    }

    // TODO: put more operators here

private:
    ACtxSceneGraph const *m_pScnGraph;
    TreePos_t m_pos;
};

using ChildRange_t = lgrn::IteratorPair<ChildIterator, ChildIterator>;

class SysSceneGraph
{
public:

    [[nodiscard]] static SubtreeBuilder add_descendants(ACtxSceneGraph& rScnGraph, uint32_t descendentCount, ActiveEnt root = lgrn::id_null<ActiveEnt>());

    /**
     * @return Iterable range of an entity's descendants
     */
    static ArrayView<ActiveEnt const> descendants(ACtxSceneGraph const& rScnGraph, ActiveEnt root);

    static ArrayView<ActiveEnt const> descendants(ACtxSceneGraph const& rScnGraph, TreePos_t rootPos);


    /**
     * @return Iterable range of an entity's children
     *
     */
    static ChildRange_t children(ACtxSceneGraph const& rScnGraph, ActiveEnt parent = lgrn::id_null<ActiveEnt>());

    template<typename ITA_T, typename ITB_T>
    static void cut(ACtxSceneGraph& rScnGraph, ITA_T first, ITB_T const& last);

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
        TreePos_t const pos = rScnGraph.m_entToTreePos[std::size_t(*first)];

        rScnGraph.m_delete.push_back(pos);



        std::advance(first, 1);
    }

    do_delete(rScnGraph);
}

} // namespace osp::active

