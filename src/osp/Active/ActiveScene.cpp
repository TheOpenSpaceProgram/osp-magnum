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
#include <iostream>

#include "ActiveScene.h"

using namespace osp;
using namespace osp::active;

void ACompCamera::calculate_projection()
{

    m_projection = Matrix4::perspectiveProjection(
            m_fov, m_viewport.x() / m_viewport.y(),
            m_near, m_far);
}


ActiveScene::ActiveScene(UserInputHandler &userInput, OSPApplication &app) :
        m_app(app),
        m_hierarchyDirty(false),
        m_userInput(userInput)
{
    m_registry.on_construct<ACompHierarchy>()
                    .connect<&ActiveScene::on_hierarchy_construct>(*this);

    m_registry.on_destroy<ACompHierarchy>()
                    .connect<&ActiveScene::on_hierarchy_destruct>(*this);

    // "There is no need to store groups around for they are extremely cheap to
    //  construct, even though they can be copied without problems and reused
    //  freely. A group performs an initialization step the very first time
    //  it's requested and this could be quite costly. To avoid it, consider
    //  creating the group when no components have been assigned yet."
    m_registry.group<ACompHierarchy, ACompTransform>();

    // Create the root entity

    m_root = m_registry.create();
    ACompHierarchy& hierarchy = m_registry.emplace<ACompHierarchy>(m_root);
    hierarchy.m_name = "Root Entity";

}

ActiveScene::~ActiveScene()
{
    // destruct dynamic systems before registry
    m_dynamicSys.clear();
    m_registry.clear();
}


entt::entity ActiveScene::hier_create_child(entt::entity parent,
                                            std::string const& name)
{
    entt::entity child = m_registry.create();
    ACompHierarchy& childHierarchy = m_registry.emplace<ACompHierarchy>(child);
    //ACompTransform& transform = m_registry.emplace<ACompTransform>(ent);
    childHierarchy.m_name = name;

    hier_set_parent_child(parent, child);

    return child;
}

void ActiveScene::hier_set_parent_child(entt::entity parent, entt::entity child)
{
    ACompHierarchy& childHierarchy = m_registry.get<ACompHierarchy>(child);
    //ACompTransform& transform = m_registry.emplace<ACompTransform>(ent);

    ACompHierarchy& parentHierarchy = m_registry.get<ACompHierarchy>(parent);

    // if child has an existing parent, cut first
    if (m_registry.valid(childHierarchy.m_parent))
    {
        hier_cut(child);
    }

    // set new child's parent
    childHierarchy.m_parent = parent;
    childHierarchy.m_level = parentHierarchy.m_level + 1;

    // If has siblings (not first child)
    if (parentHierarchy.m_childCount)
    {
        entt::entity const sibling = parentHierarchy.m_childFirst;
        ACompHierarchy& siblingHierarchy
                = m_registry.get<ACompHierarchy>(sibling);

        // Set new child and former first child as siblings
        siblingHierarchy.m_siblingPrev = child;
        childHierarchy.m_siblingNext = sibling;
    }

    // Set parent's first child to new child just created
    parentHierarchy.m_childFirst = child;
    parentHierarchy.m_childCount ++; // increase child count
}

void ActiveScene::hier_destroy(ActiveEnt ent)
{
    auto &entHier = m_registry.get<ACompHierarchy>(ent);

    // recurse into children. this can be optimized more but i'm lazy
    while (entHier.m_childCount)
    {
        hier_destroy(entHier.m_childFirst);
    }

    hier_cut(ent);
    m_registry.destroy(ent);
}

void ActiveScene::hier_cut(ActiveEnt ent)
{
    auto &entHier = m_registry.get<ACompHierarchy>(ent);

    // TODO: deal with m_depth

    // Set sibling's sibling's to each other

    if (m_registry.valid(entHier.m_siblingNext))
    {
        m_registry.get<ACompHierarchy>(entHier.m_siblingNext).m_siblingPrev
                = entHier.m_siblingPrev;
    }

    if (m_registry.valid(entHier.m_siblingPrev))
    {
        m_registry.get<ACompHierarchy>(entHier.m_siblingPrev).m_siblingNext
                = entHier.m_siblingNext;
    }

    // deal with parent

    auto &parentHier = m_registry.get<ACompHierarchy>(entHier.m_parent);
    parentHier.m_childCount --;

    if (parentHier.m_childFirst == ent)
    {
        parentHier.m_childFirst = entHier.m_siblingNext;
    }

    entHier.m_parent = entHier.m_siblingNext = entHier.m_siblingPrev
            = entt::null;
}

void ActiveScene::on_hierarchy_construct(entt::registry& reg, entt::entity ent)
{
    //std::cout << "hierarchy entity constructed\n";

    m_hierarchyDirty = true;
}

void ActiveScene::on_hierarchy_destruct(entt::registry& reg, entt::entity ent)
{
    std::cout << "hierarchy entity destructed\n";

}

void ActiveScene::update()
{
    // Update machine systems
    //for (ISysMachine &sysMachine : m_update_sensor)
    //{
    //    sysMachine.update_sensor();
    //}

    //m_wire.update_propigate();

    //for (ISysMachine &sysMachine : m_update_physics)
    //{
    //    sysMachine.update_physics(1.0f/60.0f);
    //}
    m_updateOrder.call();
}


void ActiveScene::update_hierarchy_transforms()
{

    auto group = m_registry.group<ACompHierarchy, ACompTransform>();

    if (m_hierarchyDirty)
    {
        // Sort by level, so that objects closer to the root are iterated first
        group.sort<ACompHierarchy>([](ACompHierarchy const& lhs,
                                     ACompHierarchy const& rhs)
        {
            return lhs.m_level < rhs.m_level;
        }, entt::insertion_sort());
        //group.sortable();
        m_hierarchyDirty = false;
    }

    for(auto entity: group)
    {
        //std::cout << "nice: " << group.get<ACompHierarchy>(entity).m_name << "\n";

        ACompHierarchy& hierarchy = group.get<ACompHierarchy>(entity);
        ACompTransform& transform = group.get<ACompTransform>(entity);

        if (hierarchy.m_parent == m_root)
        {
            // top level object, parent is root

            transform.m_transformWorld = transform.m_transform;

        }
        else
        {
            ACompTransform& parentTransform
                    = group.get<ACompTransform>(hierarchy.m_parent);

            // set transform relative to parent
            transform.m_transformWorld = parentTransform.m_transformWorld
                                          * transform.m_transform;

        }
    }

}

void ActiveScene::draw(entt::entity camera)
{
    //Matrix4 cameraProject;
    //Matrix4 cameraInverse;

    // TODO: check if camera has the right components
    ACompCamera& cameraComp = reg_get<ACompCamera>(camera);
    ACompTransform& cameraTransform = reg_get<ACompTransform>(camera);

    //cameraProject = cameraComp.m_projection;
    cameraComp.m_inverse = cameraTransform.m_transformWorld.inverted();

    m_renderOrder.call(cameraComp);
}

MapSysMachine::iterator ActiveScene::system_machine_find(const std::string &name)
{
//    MapSysMachine::iterator sysIt = m_sysMachines.find(name);
//    if (sysIt == m_sysMachines.end())
//    {
//        return nullptr;
//    }
//    else
//    {
//        return sysIt->second.get();
//    }
    return m_sysMachines.find(name);
}

bool ActiveScene::system_machine_it_valid(MapSysMachine::iterator it)
{
    return it != m_sysMachines.end();
}



MapDynamicSys::iterator ActiveScene::dynamic_system_find(const std::string &name)
{
    return m_dynamicSys.find(name);
}

bool ActiveScene::dynamic_system_it_valid(MapDynamicSys::iterator it)
{
    return it != m_dynamicSys.end();
}

