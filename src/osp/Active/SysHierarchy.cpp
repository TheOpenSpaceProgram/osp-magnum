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

#include "SysHierarchy.h"

using osp::active::SysHierarchy;
using osp::active::ActiveEnt;


void SysHierarchy::add_child(
        acomp_storage_t<ACompHierarchy>& rHier,
        ActiveEnt parent, ActiveEnt child)
{
    rHier.emplace(child);
    set_parent_child(rHier, parent, child);
}

void SysHierarchy::set_parent_child(acomp_view_t<ACompHierarchy> viewHier, ActiveEnt parent, ActiveEnt child)
{
    ACompHierarchy &rChildHier  = viewHier.get<ACompHierarchy>(child);
    ACompHierarchy &rParentHier = viewHier.get<ACompHierarchy>(parent);

    // if child has an existing parent, cut first
    if (rChildHier.m_parent != entt::null)
    {
        cut(viewHier, child);
    }

    // set new child's parent
    rChildHier.m_parent = parent;
    rChildHier.m_level = rParentHier.m_level + 1;

    // If has siblings (not first child)
    if(0 != rParentHier.m_childCount)
    {
        ActiveEnt sibling = rParentHier.m_childFirst;
        auto &rSiblingHier = viewHier.get<ACompHierarchy>(sibling);

        // Set new child and former first child as siblings
        rSiblingHier.m_siblingPrev = child;
        rChildHier.m_siblingNext = sibling;
    }

    // Set parent's first child to new child just created
    rParentHier.m_childFirst = child;
    rParentHier.m_childCount ++; // increase child count
}

void SysHierarchy::cut(acomp_view_t<ACompHierarchy> viewHier, ActiveEnt ent)
{
    auto &rEntHier = viewHier.get<ACompHierarchy>(ent);

    // TODO: deal with m_depth

    // Unlink siblings by connecting previous and next to each other

    if (rEntHier.m_siblingNext != entt::null)
    {
        viewHier.get<ACompHierarchy>(rEntHier.m_siblingNext).m_siblingPrev
                = rEntHier.m_siblingPrev;
    }

    if (rEntHier.m_siblingPrev != entt::null)
    {
        viewHier.get<ACompHierarchy>(rEntHier.m_siblingPrev).m_siblingNext
                = rEntHier.m_siblingNext;
    }

    // Unlink parent

    auto &rParentHier = viewHier.get<ACompHierarchy>(rEntHier.m_parent);
    rParentHier.m_childCount --;

    if (rParentHier.m_childFirst == ent)
    {
        rParentHier.m_childFirst = rEntHier.m_siblingNext;
    }

    rEntHier.m_level = 0;
    rEntHier.m_parent = rEntHier.m_siblingNext = rEntHier.m_siblingPrev
                      = entt::null;
}

void SysHierarchy::sort(acomp_storage_t<ACompHierarchy>& rHier)
{
    rHier.sort( [&rHier](ActiveEnt lhs, ActiveEnt rhs)
    {
        return rHier.get(lhs).m_level < rHier.get(lhs).m_level;
    }, entt::insertion_sort());
}

void SysHierarchy::update_delete_descendents(
        acomp_view_t<ACompHierarchy> viewHier,
        acomp_storage_t<ACompDelete>& rDelete)
{
    auto view = viewHier | entt::basic_view{rDelete};

    // copy entities to delete into a buffer
    std::vector<ActiveEnt> toDelete;
    toDelete.reserve(view.size_hint());
    toDelete.assign(std::begin(view), std::end(view));

    // Add delete components to descendents of all entities to delete
    for (ActiveEnt ent : toDelete)
    {
        traverse(viewHier, ent, [&rDelete] (ActiveEnt descendent) {
            rDelete.emplace(descendent);
            return EHierarchyTraverseStatus::Continue;
        });
    }
}

