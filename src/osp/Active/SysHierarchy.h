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

#include "ActiveScene.h"

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
     * Add hierarchy component to root entity in scene
     *
     * @param rScene [ref] scene with no hierarchy yet
     */
    //static void setup(acomp_view<ACompHierarchy> viewHier);

    /**
     * Create a new entity, and add a ACompHierarchy to it
     *
     * @param rScene [ref] Scene with hierarchy
     * @param parent [in] Entity to assign as parent
     * @param name   [in] Name of entity
     * @return New entity created
     */
    static ActiveEnt create_child(
            acomp_storage_t<ACompHierarchy>& rHier,
            acomp_storage_t<ACompName>& rNames,
            ActiveEnt parent, ActiveEnt child, std::string const& name);

    /**
     * Set parent-child relationship between two nodes containing an
     * ACompHierarchy
     *
     * @param rScene [ref] Scene with hierarchy
     * @param parent [in]
     * @param child  [in]
     */
    static void set_parent_child(acomp_view_t<ACompHierarchy> viewHier,
                                 ActiveEnt parent, ActiveEnt child);

    /**
     * Cut an entity out of the hierarchy.
     *
     * @param rScene [ref] Scene with hierarchy
     * @param ent [in]
     */
    static void cut(acomp_view_t<ACompHierarchy> viewHier, ActiveEnt ent);

    /**
     * Traverse the scene hierarchy
     *
     * Calls the specified callable on each entity of the scene hierarchy
     *
     * @param rScene [ref] Scene with hierarchy
     * @param root The entity whose sub-tree to traverse
     * @param callable A function that accepts an ActiveEnt as an argument and
     *                 returns false if traversal should stop, true otherwise
     */
    template <typename FUNC_T>
    static void traverse(acomp_view_t<ACompHierarchy> viewHier, ActiveEnt root, FUNC_T callable);

    /**
     * Sort hierarchy component pool
     *
     * @param rScene [ref] Scene with hierarchy
     */
    static void sort(acomp_storage_t<ACompHierarchy>& rHier);

    /**
     * Mark descendents of deleted hierarchy entities as deleted too
     *
     * @param rScene [ref] Scene with hierarchy
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
