/**
 * Open Space Program
 * Copyright © 2019-2021 Open Space Program Project
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

#include <osp/Active/opengl/SysRenderGL.h>

#include <Magnum/Shaders/MeshVisualizerGL.h>

namespace osp::shader
{

using MeshVisualizer = Magnum::Shaders::MeshVisualizerGL3D;

struct ACtxDrawMeshVisualizer
{
    MeshVisualizer m_shader{Corrade::NoCreate};

    active::acomp_storage_t<active::ACompDrawTransform> *m_pDrawTf{nullptr};
    osp::active::ACompMeshGlStorage_t                   *m_pMeshId{nullptr};
    osp::active::MeshGlStorage_t                        *m_pMeshGl{nullptr};

    bool m_wireframeOnly{false};

    constexpr void assign_pointers(active::ACtxSceneRenderGL& rCtxScnGl,
                                   active::RenderGL& rRenderGl) noexcept
    {
        m_pDrawTf   = &rCtxScnGl.m_drawTransform;
        m_pMeshId   = &rCtxScnGl.m_meshId;
        m_pMeshGl   = &rRenderGl.m_meshGl;
    }
};

void draw_ent_visualizer(
        active::ActiveEnt ent,
        active::ViewProjMatrix const& viewProj,
        active::EntityToDraw::UserData_t userData) noexcept;

template<typename ITA_T, typename ITB_T>
void sync_visualizer(
        ITA_T dirtyFirst, ITB_T const& dirtyLast,
        active::EntSet_t const& hasMaterial,
        active::RenderGroup::Storage_t& rStorage,
        ACtxDrawMeshVisualizer &rData)
{
    using namespace active;

    while (dirtyFirst != dirtyLast)
    {
        ActiveEnt const ent = *dirtyFirst;
        bool alreadyAdded = rStorage.contains(ent);
        if (hasMaterial.test(std::size_t(ent)))
        {
            if ( ! alreadyAdded)
            {
                rStorage.emplace( ent, EntityToDraw{&draw_ent_visualizer, {&rData} } );
            }
        }
        else
        {
            if (alreadyAdded)
            {
                rStorage.erase(ent);
            }
        }

        std::advance(dirtyFirst, 1);
    }


}

} // namespace osp::shader
