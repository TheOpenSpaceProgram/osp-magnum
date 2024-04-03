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
#include "basic_fn.h"

#include <Corrade/Containers/ArrayViewStl.h>

#include <algorithm>

using namespace osp;
using namespace osp::active;

using Corrade::Containers::arrayView;

SubtreeBuilder SubtreeBuilder::add_child(ActiveEnt ent, uint32_t descendantCount)
{
    // Place ent into tree at m_first
    m_rScnGraph.m_treeToEnt[m_first]        = ent;
    m_rScnGraph.m_treeDescendants[m_first]  = descendantCount;
    m_rScnGraph.m_entParent[ent]            = m_root;
    m_rScnGraph.m_entToTreePos[ent]         = m_first;

    TreePos_t const childFirst = m_first + 1;
    TreePos_t const childLast  = childFirst + descendantCount;

    m_first = childLast;

    return {m_rScnGraph, ent, childFirst, childLast};
}

SubtreeBuilder SysSceneGraph::add_descendants(ACtxSceneGraph& rScnGraph, uint32_t descendantCount, ActiveEnt root)
{
    TreePos_t const rootPos         = (root == lgrn::id_null<ActiveEnt>())
                                    ? 0
                                    : rScnGraph.m_entToTreePos[root];

    uint32_t rootdescendants        = rScnGraph.m_treeDescendants[rootPos];

    TreePos_t const subFirst        = rootPos + 1 + rootdescendants;
    TreePos_t const subLast         = subFirst + descendantCount;

    TreePos_t const treeOldSize     = rScnGraph.m_treeDescendants[0] + 1;
    TreePos_t const treeNewSize     = treeOldSize + descendantCount;

    rScnGraph.m_treeToEnt.resize(treeNewSize);
    rScnGraph.m_treeDescendants.resize(treeNewSize);

    SubtreeBuilder out(rScnGraph, root, subFirst, subLast);

    if (subFirst != treeOldSize)
    {
        // Right-shift tree vectors from subFirst and onwards to make
        // space for the new subtree

        for (ActiveEnt const ent : arrayView(rScnGraph.m_treeToEnt.data(), rScnGraph.m_treeToEnt.size()).slice(subFirst, treeOldSize))
        {
            rScnGraph.m_entToTreePos[ent] += descendantCount;
        }
        std::shift_right(rScnGraph.m_treeToEnt.begin() + subFirst,
                         rScnGraph.m_treeToEnt.end(),
                         descendantCount);
        std::shift_right(rScnGraph.m_treeDescendants.begin() + subFirst,
                         rScnGraph.m_treeDescendants.end(),
                         descendantCount);
    }
    // else, subtree was inserted at end. no shifting required.

    // Update descendant counts of this and ancestors
    ActiveEnt parent = root;
    bool parentNotNull = true;
    while (parentNotNull)
    {
        parentNotNull = (parent != lgrn::id_null<ActiveEnt>());
        TreePos_t const parentPos = parentNotNull ? rScnGraph.m_entToTreePos[parent] : 0;
        rScnGraph.m_treeDescendants[parentPos] += descendantCount;
        parent = parentNotNull ? rScnGraph.m_entParent[parent] : parent;
    }


    return out;
}

ArrayView<ActiveEnt const> SysSceneGraph::descendants(ACtxSceneGraph const& rScnGraph, ActiveEnt root)
{
    TreePos_t const rootPos = rScnGraph.m_entToTreePos[root];
    return descendants(rScnGraph, rootPos);
}

ArrayView<ActiveEnt const> SysSceneGraph::descendants(ACtxSceneGraph const& rScnGraph, TreePos_t rootPos)
{
    uint32_t const  descendants = rScnGraph.m_treeDescendants[rootPos];
    TreePos_t const childFirst  = rootPos + 1;
    TreePos_t const childLast   = childFirst + descendants;

    return (descendants == 0)
            ? ArrayView<ActiveEnt const>{}
            : arrayView(rScnGraph.m_treeToEnt.data(), rScnGraph.m_treeToEnt.size()).slice(childFirst, childLast);
}


ChildRange_t SysSceneGraph::children(ACtxSceneGraph const& rScnGraph, ActiveEnt parent)
{
    TreePos_t const parentPos = (parent == lgrn::id_null<ActiveEnt>())
                              ? 0
                              : rScnGraph.m_entToTreePos[parent];

    uint32_t const  descendants = rScnGraph.m_treeDescendants[parentPos];
    TreePos_t const childFirst  = parentPos + 1;
    TreePos_t const childLast   = parentPos + 1 + descendants;

    return ChildRange_t{ChildIterator{&rScnGraph, childFirst},
                        ChildIterator{&rScnGraph, childLast}};
}

void SysSceneGraph::do_delete(ACtxSceneGraph& rScnGraph)
{
    // Delete subtrees by carefully shifting elements left
    // Operation being a single left->right swoop. This should be pretty fast
    // since this is only a few KB in size, and is done once per update

    std::sort(rScnGraph.m_delete.begin(), rScnGraph.m_delete.end());

    TreePos_t const treeLast    = 1 + rScnGraph.m_treeDescendants[0];

    auto const& itTreeDescFirst = rScnGraph.m_treeDescendants.begin();
    auto const& itTreeEntsFirst = rScnGraph.m_treeToEnt.begin();

    auto const& itDelFirst      = rScnGraph.m_delete.begin();
    auto const& itDelLast       = rScnGraph.m_delete.end();
    auto        itDel           = itDelFirst;

    TreePos_t const untouched = (*itDel);
    TreePos_t done = untouched;

    while (itDel != itDelLast)
    {
        auto const itDelNext = std::next(itDel);
        bool const notLast = (itDelNext != itDelLast);

        uint32_t const removeTotal = 1 + rScnGraph.m_treeDescendants[*itDel];

        // State of array each iteration:
        //
        // [Done] [Prev. shifted] [Delete] [Keep] [Delete Next] ....
        //        <--------SHIFT-----------|----|

        TreePos_t const keepFirst   = (*itDel) + removeTotal;
        TreePos_t const keepLast    = (notLast) ? (*itDelNext) : (treeLast);
        assert(keepFirst <= keepLast);

        std::ptrdiff_t const shift  = keepFirst - done;

        // Update descendant count of ancestors
        ActiveEnt parent = rScnGraph.m_treeToEnt[*itDel];
        bool parentNotNull = true;
        while (parentNotNull)
        {
            parent = rScnGraph.m_entParent[parent];
            parentNotNull = (parent != lgrn::id_null<ActiveEnt>());
            TreePos_t const parentPos = parentNotNull ? rScnGraph.m_entToTreePos[parent] : 0;
            rScnGraph.m_treeDescendants[parentPos] -= removeTotal;
        }

        // Clear values for entities to delete
        std::for_each(itTreeEntsFirst + (*itDel), itTreeEntsFirst + (*itDel) + removeTotal,
                      [&rScnGraph] (ActiveEnt const ent)
        {
            rScnGraph.m_entParent[ent]      = lgrn::id_null<ActiveEnt>();
            rScnGraph.m_entToTreePos[ent]   = lgrn::id_null<TreePos_t>();
        });

        // Update tree positions for elements to shift
        std::for_each(itTreeEntsFirst + keepFirst, itTreeEntsFirst + keepLast,
                      [&rScnGraph, shift] (ActiveEnt const ent)
        {
            rScnGraph.m_entToTreePos[ent] -= shift;
        });

        // Shift over tree data
        std::shift_left(itTreeDescFirst + done, itTreeDescFirst + keepLast, shift);
        std::shift_left(itTreeEntsFirst + done, itTreeEntsFirst + keepLast, shift);

        done += keepLast - keepFirst;
        itDel = itDelNext;
    }

    rScnGraph.m_treeToEnt      .resize(done);
    rScnGraph.m_treeDescendants.resize(done);

    rScnGraph.m_delete.clear();
}
