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
#include "common_scene.h"

#include <osp/Active/SysRender.h>

#include <osp/Active/SysHierarchy.h>

namespace testapp
{

CommonTestScene::~CommonTestScene()
{
    // Clear data first
    for (on_cleanup_t *cleanup : m_onCleanup)
    {
        cleanup(*this);
    }

    osp::active::SysRender::clear_owners(m_drawing);
    osp::active::SysRender::clear_resource_owners(m_drawingRes, *m_pResources);
}

void CommonTestScene::update_hierarchy_delete()
{
    using namespace osp::active;
    // Cut deleted entities out of the hierarchy
    SysHierarchy::update_delete_cut(
            m_basic.m_hierarchy,
            std::cbegin(m_delete), std::cend(m_delete));

    // Create a new delete vector that includes the descendents of our current
    // vector of entities to delete
    m_deleteTotal = m_delete;
    SysHierarchy::update_delete_descendents(
            m_basic.m_hierarchy,
            std::cbegin(m_delete), std::cend(m_delete),
            [=] (ActiveEnt ent)
    {
        m_deleteTotal.push_back(ent);
    });
}

void CommonTestScene::update_delete()
{
    auto first = std::cbegin(m_deleteTotal);
    auto last = std::cend(m_deleteTotal);
    osp::active::update_delete_basic                (m_basic,   first, last);
    osp::active::SysRender::update_delete_drawing   (m_drawing, first, last);

    // Delete entity IDs
    for (ActiveEnt const ent : m_deleteTotal)
    {
        if (m_activeIds.exists(ent))
        {
            m_activeIds.remove(ent);
        }
    }
}

void CommonTestScene::set_all_dirty()
{
    using osp::active::active_sparse_set_t;

    for (osp::active::MaterialData &rMat : m_drawing.m_materials)
    {
        rMat.m_added.assign(std::begin(rMat.m_comp), std::end(rMat.m_comp));
    }

    // Set all meshs dirty
    auto &rMeshSet = static_cast<active_sparse_set_t&>(m_drawing.m_mesh);
    m_drawing.m_meshDirty.assign(std::begin(rMeshSet), std::end(rMeshSet));

    // Set all textures dirty
    auto &rDiffSet = static_cast<active_sparse_set_t&>(m_drawing.m_diffuseTex);
    m_drawing.m_diffuseDirty.assign(std::begin(rMeshSet), std::end(rMeshSet));
}

} // namespace testapp
