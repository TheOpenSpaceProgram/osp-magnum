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

using osp::active::SysRender;

void SysRender::update_draw_transforms(
        acomp_storage_t<ACompHierarchy> const& hier,
        acomp_storage_t<ACompTransform> const& transform,
        acomp_storage_t<ACompDrawTransform>& rDrawTf)
{

    rDrawTf.respect(hier);

    auto viewDrawTf = entt::basic_view{rDrawTf};

    for(ActiveEnt const entity : viewDrawTf)
    {
        ACompHierarchy const &entHier   = hier.get(entity);
        ACompTransform const &entTf     = transform.get(entity);
        ACompDrawTransform &rEntDrawTf  = rDrawTf.get(entity);

        if (entHier.m_level == 1)
        {
            // top level object, parent is root
            rEntDrawTf.m_transformWorld = entTf.m_transform;
        }
        else
        {
            ACompDrawTransform const& parentDrawTf = rDrawTf.get(entHier.m_parent);

            // set transform relative to parent
            rEntDrawTf.m_transformWorld = parentDrawTf.m_transformWorld
                                        * entTf.m_transform;
        }
    }
}

void SysRender::clear_dirty_materials(std::vector<MaterialData> &rMaterials)
{
    for (MaterialData &rMat : rMaterials)
    {
        rMat.m_added.clear();
        rMat.m_removed.clear();
    }
}

