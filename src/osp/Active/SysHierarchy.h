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

#include <stack>

namespace osp::active
{

enum class EHierarchyTraverseStatus : bool
{
    Continue = true,
    Stop = false
};


class SysHierarchy
{
public:

    /**
     * @brief Add an ACompHierarchy to an entity that already has ACompHierarchy
     *
     * @param rHier     [ref] Storage for hierarchy components
     * @param parent    [in] Parent entity containing ACompHierarchy
     * @param child     [in] Child entity to add ACompHierarchy to
     */
    static void add_child(
            acomp_storage_t<ACompHierarchy>& rHier,
            ActiveEnt parent, ActiveEnt child);

    /**
     * @brief Set parent-child relationship between two entities with
     *        ACompHierarchy
     *
     * @param viewHier  [ref] View for hierarchy components
     * @param parent    [in] Parent entity
     * @param child     [in] Child entity
     */
    static void set_parent_child(acomp_view_t<ACompHierarchy> viewHier,
                                 ActiveEnt parent, ActiveEnt child);

    /**
     * @brief Cut an entity out of the hierarchy.
     *
     * @param viewHier  [ref] View for hierarchy components
     * @param ent       [in] Entity to remove from hierarchy
     */
    static void cut(acomp_view_t<ACompHierarchy> viewHier, ActiveEnt ent);

    /**
     * @brief Traverse the scene hierarchy
     *
     * Calls the specified callable on each entity of the scene hierarchy
     *
     * @param viewHier  [ref] View for hierarchy components
     * @param root      [in] The entity whose sub-tree to traverse
     * @param callable  [in] A function that accepts an ActiveEnt, and returns
     *                       a status stop or continue traversing
     */
    template <typename FUNC_T>
    static void traverse(acomp_view_t<ACompHierarchy> viewHier, ActiveEnt root, FUNC_T callable);

    /**
     * @brief Sort hierarchy component pool
     *
     * @param rHier     [ref] Storage for hierarchy components
     */
    static void sort(acomp_storage_t<ACompHierarchy>& rHier);

    /**
     * @brief Mark descendents of deleted hierarchy entities as deleted too
     *
     * @param viewHier  [ref] View for hierarchy components
     * @param rDelete   [ref] Storage for delete components
     */
    static void update_delete_descendents(acomp_view_t<ACompHierarchy> viewHier, acomp_storage_t<ACompDelete>& rDelete);
};

template<typename FUNC_T>
void SysHierarchy::traverse(acomp_view_t<ACompHierarchy> viewHier,
                                      ActiveEnt root, FUNC_T callable)
{
    using osp::active::ACompHierarchy;

    std::stack<ActiveEnt> parentNextSibling;
    ActiveEnt currentEnt = root;

    unsigned int rootLevel = viewHier.get<ACompHierarchy>(root).m_level;

    while (true)
    {
        ACompHierarchy &hier = viewHier.get<ACompHierarchy>(currentEnt);

        if (callable(currentEnt) == EHierarchyTraverseStatus::Stop) { return; }

        if (hier.m_childCount > 0)
        {
            // entity has some children
            currentEnt = hier.m_childFirst;

            // Save next sibling for later if it exists
            // Don't check siblings of the root node
            if ((hier.m_siblingNext != entt::null) && (hier.m_level > rootLevel))
            {
                parentNextSibling.push(hier.m_siblingNext);
            }
        }
        else if ((hier.m_siblingNext != entt::null) && (hier.m_level > rootLevel))
        {
            // no children, move to next sibling
            currentEnt = hier.m_siblingNext;
        }
        else if (parentNextSibling.size() > 0)
        {
            // last sibling, and not done yet
            // is last sibling, move to parent's (or ancestor's) next sibling
            currentEnt = parentNextSibling.top();
            parentNextSibling.pop();
        }
        else
        {
            break;
        }
    }
}


}
