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

#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/DefaultFramebuffer.h>

using namespace osp;
using namespace osp::active;

void ACompCamera::calculate_projection()
{

    m_projection = Matrix4::perspectiveProjection(
            m_fov, m_viewport.x() / m_viewport.y(),
            m_near, m_far);
}


ActiveScene::ActiveScene(UserInputHandler &userInput, OSPApplication &app, Package& context) :
        m_app(app),
        m_context(context),
        m_hierarchyDirty(false),
        m_userInput(userInput)
{
    m_registry.on_construct<ACompHierarchy>()
                    .connect<&ActiveScene::on_hierarchy_construct>(*this);

    m_registry.on_destroy<ACompHierarchy>()
                    .connect<&ActiveScene::on_hierarchy_destruct>(*this);

    // Create the root entity

    m_root = m_registry.create();
    ACompHierarchy& hierarchy = m_registry.emplace<ACompHierarchy>(m_root);
    hierarchy.m_name = "Root Entity";

}

ActiveScene::~ActiveScene()
{
    m_registry.clear();
}


ActiveEnt ActiveScene::hier_create_child(ActiveEnt parent,
                                            std::string const& name)
{
    ActiveEnt child = m_registry.create();
    ACompHierarchy& childHierarchy = m_registry.emplace<ACompHierarchy>(child);
    //ACompTransform& transform = m_registry.emplace<ACompTransform>(ent);
    childHierarchy.m_name = name;

    hier_set_parent_child(parent, child);

    return child;
}

void ActiveScene::hier_set_parent_child(ActiveEnt parent, ActiveEnt child)
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
        ActiveEnt sibling = parentHierarchy.m_childFirst;
        auto& siblingHierarchy = m_registry.get<ACompHierarchy>(sibling);

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
    auto *pEntHier = &m_registry.get<ACompHierarchy>(ent);

    // Destroy children first recursively, if there is any
    while (pEntHier->m_childCount > 0)
    {
        hier_destroy(pEntHier->m_childFirst);
        // previous hier_destroy might have caused a reallocation
        pEntHier = m_registry.try_get<ACompHierarchy>(ent);
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

void ActiveScene::on_hierarchy_construct(ActiveReg_t& reg, ActiveEnt ent)
{
    m_hierarchyDirty = true;
}

void ActiveScene::on_hierarchy_destruct(ActiveReg_t& reg, ActiveEnt ent)
{
    m_hierarchyDirty = true;
}

void ActiveScene::update()
{

    m_updateOrder.call(*this);
}


void ActiveScene::update_hierarchy_transforms()
{
    if (m_hierarchyDirty)
    {
        // Sort by level, so that objects at the top of the hierarchy are
        // updated first
        m_registry.sort<ACompHierarchy>([](ACompHierarchy const& lhs,
                                           ACompHierarchy const& rhs)
        {
            return lhs.m_level < rhs.m_level;
        }, entt::insertion_sort());

        m_registry.sort<ACompTransform, ACompHierarchy>();

        m_hierarchyDirty = false;
    }

    auto view = m_registry.view<ACompHierarchy, ACompTransform>();

    for(ActiveEnt entity : view)
    {
        auto& hierarchy = view.get<ACompHierarchy>(entity);
        auto& transform = view.get<ACompTransform>(entity);

        if (hierarchy.m_parent == m_root)
        {
            // top level object, parent is root

            transform.m_transformWorld = transform.m_transform;

        }
        else
        {
            auto& parentTransform
                    = view.get<ACompTransform>(hierarchy.m_parent);

            // set transform relative to parent
            transform.m_transformWorld = parentTransform.m_transformWorld
                                          * transform.m_transform;

        }
    }

}

void ActiveScene::draw(ActiveEnt camera)
{
    //Matrix4 cameraProject;
    //Matrix4 cameraInverse;

    // TODO: check if camera has the right components
    ACompCamera& cameraComp = reg_get<ACompCamera>(camera);
    ACompTransform& cameraTransform = reg_get<ACompTransform>(camera);

    //cameraProject = cameraComp.m_projection;
    cameraComp.m_inverse = cameraTransform.m_transformWorld.inverted();

    for (auto& pass : m_renderQueue)
    {
        pass(*this, cameraComp);
    }

    //m_renderOrder.call(*this, cameraComp);
}

MapSysMachine_t::iterator ActiveScene::system_machine_add(std::string_view name,
        std::unique_ptr<ISysMachine> sysMachine)
{
    auto const& [it, success] = m_sysMachines.emplace(
            name, std::move(sysMachine));
    if (success)
    {
        return it;
    }
    return m_sysMachines.end();
}

MapSysMachine_t::iterator ActiveScene::system_machine_find(std::string_view name)
{
//    MapSysMachine_t::iterator sysIt = m_sysMachines.find(name);
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

bool ActiveScene::system_machine_it_valid(MapSysMachine_t::iterator it)
{
    return it != m_sysMachines.end();
}

