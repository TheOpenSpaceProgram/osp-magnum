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
            acomp_storage_t<ACompHierarchy>& rHierarchy,
            ActiveEnt parent, ActiveEnt child);

    /**
     * @brief Set parent-child relationship between two entities with
     *        ACompHierarchy
     *
     * @param hierarchy [ref] Storage for hierarchy components
     * @param parent    [in] Parent entity
     * @param child     [in] Child entity
     */
    static void set_parent_child(
            acomp_storage_t<ACompHierarchy>& rHierarchy,
            ActiveEnt parent, ActiveEnt child);

    /**
     * @brief Cut an entity out of the hierarchy.
     *
     * @param hierarchy [ref] Storage for hierarchy components
     * @param ent       [in] Entity to remove from hierarchy
     */
    static void cut(acomp_storage_t<ACompHierarchy>& hierarchy, ActiveEnt ent);

    /**
     * @brief Traverse the scene hierarchy
     *
     * Calls the specified callable on each entity of the scene hierarchy
     *
     * @param rHier     [ref] Storage for hierarchy components
     * @param root      [in] The entity whose sub-tree to traverse
     * @param callable  [in] A function that accepts an ActiveEnt, and returns
     *                       a status stop or continue traversing
     */
    template <typename FUNC_T>
    static void traverse(acomp_storage_t<ACompHierarchy> const& hier, ActiveEnt root, FUNC_T&& callable);

    /**
     * @brief Sort hierarchy component pool
     *
     * @param rHier     [ref] Storage for hierarchy components
     */
    static void sort(acomp_storage_t<ACompHierarchy>& rHierarchy);

    /**
     * @brief Cut entities to delete out of the hierarchy
     */
    template<typename IT_T>
    static void update_delete_cut(
            acomp_storage_t<ACompHierarchy>& rHier, IT_T first, IT_T const& last);

    /**
     * @brief Mark descendents of deleted hierarchy entities as deleted too
     */
    template<typename IT_T, typename FUNC_T>
    static void update_delete_descendents(
            acomp_storage_t<ACompHierarchy> const& hier, IT_T first, IT_T const& last, FUNC_T&& deleteEnt);
};

template<typename IT_T>
void SysHierarchy::update_delete_cut(
        acomp_storage_t<ACompHierarchy>& rHier, IT_T first, IT_T const& last)
{
    while (first != last)
    {
        cut(rHier, *first);
        std::advance(first, 1);
    }
}

template<typename IT_T, typename FUNC_T>
void SysHierarchy::update_delete_descendents(
        acomp_storage_t<ACompHierarchy> const& hier, IT_T first, IT_T const& last,
        FUNC_T&& deleteEnt)
{
    while (first != last)
    {
        ActiveEnt const ent = *first;

        traverse(hier, ent, [&deleteEnt] (ActiveEnt descendent) {
            deleteEnt(descendent);
            return EHierarchyTraverseStatus::Continue;
        });

        std::advance(first, 1);
    }
}

template<typename FUNC_T>
void SysHierarchy::traverse(
        acomp_storage_t<ACompHierarchy> const& hier, ActiveEnt root,
        FUNC_T&& callable)
{
    using osp::active::ACompHierarchy;

    std::stack<ActiveEnt> parentNextSibling;
    ActiveEnt currEnt = root;

    unsigned int rootLevel = hier.get(root).m_level;

    while (true)
    {
        ACompHierarchy const &currHier = hier.get(currEnt);

        if (callable(currEnt) == EHierarchyTraverseStatus::Stop)
        {
            return;
        }

        if (currHier.m_childCount > 0)
        {
            // entity has some children
            currEnt = currHier.m_childFirst;

            // Save next sibling for later if it exists
            // Don't check siblings of the root node
            if ((currHier.m_siblingNext != entt::null) && (currHier.m_level > rootLevel))
            {
                parentNextSibling.push(currHier.m_siblingNext);
            }
        }
        else if ((currHier.m_siblingNext != entt::null) && (currHier.m_level > rootLevel))
        {
            // no children, move to next sibling
            currEnt = currHier.m_siblingNext;
        }
        else if (parentNextSibling.size() > 0)
        {
            // last sibling, and not done yet
            // is last sibling, move to parent's (or ancestor's) next sibling
            currEnt = parentNextSibling.top();
            parentNextSibling.pop();
        }
        else
        {
            break;
        }
    }
}


}
