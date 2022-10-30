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
#include "SysSceneGraph.h"

#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/ArrayViewStl.h>

using namespace osp;
using namespace osp::active;

using Corrade::Containers::arrayView;

SubtreeBuilder SubtreeBuilder::add_child(ActiveEnt ent, uint32_t descendentCount)
{
    // Place ent into tree at m_first
    m_rScnGraph.m_treeToEnt.at(m_first)             = ent;
    m_rScnGraph.m_treeDescendents.at(m_first)       = descendentCount;
    m_rScnGraph.m_entParent.at(std::size_t(ent))    = m_root;
    m_rScnGraph.m_entToTreePos.at(std::size_t(ent)) = m_first;

    TreePos_t const childFirst = m_first + 1;
    TreePos_t const childLast  = childFirst + descendentCount;

    m_first = childLast;

    return SubtreeBuilder(m_rScnGraph, ent, childFirst, childLast);
}

SubtreeBuilder SysSceneGraph::add_descendants(ACtxSceneGraph& rScnGraph, uint32_t descendentCount, ActiveEnt parent)
{
    TreePos_t const parentPos       = (parent == lgrn::id_null<ActiveEnt>())
                                    ? 0
                                    : rScnGraph.m_entToTreePos.at(std::size_t(parent));
    uint32_t &rParentDescendents    = rScnGraph.m_treeDescendents.at(parentPos);

    TreePos_t const subFirst        = parentPos + 1 + rParentDescendents;
    TreePos_t const subLast         = subFirst + descendentCount;

    rParentDescendents += descendentCount;

    TreePos_t const treeOldSize     = rScnGraph.m_treeToEnt.size();
    TreePos_t const treeNewSize     = treeOldSize + descendentCount;

    rScnGraph.m_treeToEnt.resize(treeNewSize);
    rScnGraph.m_treeDescendents.resize(treeNewSize);

    SubtreeBuilder out(rScnGraph, parent, subFirst, subLast);

    if (subFirst != treeOldSize)
    {
        // Right-shift tree vectors from subFirst and onwards to make
        // space for the new subtree

        for (ActiveEnt const ent : arrayView(rScnGraph.m_treeToEnt).slice(subFirst, treeOldSize))
        {
            rScnGraph.m_entToTreePos[std::size_t(ent)] += descendentCount;
        }
        std::shift_right(rScnGraph.m_treeToEnt.begin() + subFirst,
                         rScnGraph.m_treeToEnt.end(),
                         descendentCount);
        std::shift_right(rScnGraph.m_treeDescendents.begin() + subFirst,
                         rScnGraph.m_treeDescendents.end(),
                         descendentCount);
    }
    // else, subtree was inserted at end. no shifting required.

    return out;
}

ArrayView<ActiveEnt const> SysSceneGraph::descendants(ACtxSceneGraph const& rScnGraph, ActiveEnt parent)
{
    TreePos_t const parentPos   = rScnGraph.m_entToTreePos.at(std::size_t(parent));
    uint32_t const descendants  = rScnGraph.m_treeDescendents[parentPos];

    return (descendants == 0)
            ? ArrayView<ActiveEnt const>{}
            : arrayView(rScnGraph.m_treeToEnt).slice(parentPos + 1, parentPos + descendants + 1);
}

std::vector<ActiveEnt> SysSceneGraph::children(ACtxSceneGraph const& rScnGraph, ActiveEnt parent)
{
    std::vector<ActiveEnt> out;

    TreePos_t const parentPos = rScnGraph.m_entToTreePos.at(std::size_t(parent));
    TreePos_t const childLast = parentPos + 1 + rScnGraph.m_treeDescendents[parentPos];

    for (TreePos_t childPos = parentPos + 1;
         childPos < childLast;
         childPos += rScnGraph.m_treeDescendents[childPos] + 1)
    {
        out.push_back(rScnGraph.m_treeToEnt[childPos]);
    }

    return out;
}

