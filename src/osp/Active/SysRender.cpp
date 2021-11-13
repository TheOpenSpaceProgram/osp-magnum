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
#include "SysRender.h"

#include "ActiveScene.h"

#include <osp/Resource/Package.h>

#include <functional>

using osp::active::SysRender;


void SysRender::update_material_assign(
        ACtxRenderGroups& rGroups,
        acomp_storage_t<ACompMaterial>& rMaterials)
{
    // Add ACompMaterial components, and clear m_added queues
    for (material_id_t i = 0; i < rGroups.m_added.size(); i ++)
    {
        for ( ActiveEnt ent : std::exchange(rGroups.m_added[i], {}) )
        {
            rMaterials.emplace(ent, ACompMaterial{i});
        }
    }
}

void SysRender::update_material_delete(
        ACtxRenderGroups& rGroups,
        acomp_storage_t<ACompMaterial>& rMaterials,
        acomp_view_t<const ACompDelete> viewDelete)
{
    auto viewDelMat = viewDelete | entt::basic_view{rMaterials};

    // copy entities to delete into a buffer
    std::vector<ActiveEnt> toDelete;
    toDelete.reserve(viewDelMat.size_hint());

    for (auto const& [ent, mat] : viewDelMat.each())
    {
        toDelete.push_back(ent);
    }

    if (toDelete.empty())
    {
        return;
    }

    // Delete from each draw group
    for ([[maybe_unused]] auto& [name, rGroup] : rGroups.m_groups)
    {
        for (ActiveEnt ent : toDelete)
        {
            if (rGroup.m_entities.contains(ent))
            {
                rGroup.m_entities.remove(ent);
            }
        }
    }

    rMaterials.erase(std::begin(toDelete), std::end(toDelete));
}

void SysRender::update_hierarchy_transforms(
        acomp_storage_t<ACompHierarchy> const& hier,
        acomp_view_t<ACompTransform> const& viewTf,
        acomp_storage_t<ACompDrawTransform>& rDrawTf)
{
    //rReg.sort<ACompDrawTransform, ACompHierarchy>();
    rDrawTf.respect(hier);

    auto viewDrawTf = entt::basic_view{rDrawTf};

    for(ActiveEnt const entity : viewDrawTf)
    {
        ACompHierarchy const &entHier = hier.get(entity);
        auto const &entTf = viewTf.get<ACompTransform>(entity);
        auto &entDrawTf = viewDrawTf.get<ACompDrawTransform>(entity);

        if (entHier.m_level == 1)
        {
            // top level object, parent is root
            entDrawTf.m_transformWorld = entTf.m_transform;
        }
        else
        {
            ACompDrawTransform const& parentDrawTf = rDrawTf.get(entHier.m_parent);

            // set transform relative to parent
            entDrawTf.m_transformWorld = parentDrawTf.m_transformWorld
                                       * entTf.m_transform;
        }
    }
}
