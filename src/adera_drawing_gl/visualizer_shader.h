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

#include <osp_drawing_gl/rendergl.h>

#include <Magnum/Shaders/MeshVisualizerGL.h>

namespace adera::shader
{

using MeshVisualizer = Magnum::Shaders::MeshVisualizerGL3D;

struct ACtxDrawMeshVisualizer
{
    MeshVisualizer m_shader{Corrade::NoCreate};

    osp::draw::DrawTransforms_t         *m_pDrawTf{nullptr};
    osp::draw::MeshGlEntStorage_t       *m_pMeshId{nullptr};
    osp::draw::MeshGlStorage_t          *m_pMeshGl{nullptr};

    osp::draw::MaterialId               m_materialId { lgrn::id_null<osp::draw::MaterialId>() };

    bool m_wireframeOnly{false};

constexpr void assign_pointers(osp::draw::ACtxSceneRender&      rScnRender,
                               osp::draw::ACtxSceneRenderGL&    rScnRenderGl,
                               osp::draw::RenderGL&             rRenderGl) noexcept
    {
        m_pDrawTf   = &rScnRender.m_drawTransform;
        m_pMeshId   = &rScnRenderGl.m_meshId;
        m_pMeshGl   = &rRenderGl.m_meshGl;
    }
};

void draw_ent_visualizer(
        osp::draw::DrawEnt                  ent,
        osp::draw::ViewProjMatrix const&    viewProj,
        osp::draw::EntityToDraw::UserData_t userData) noexcept;

inline void sync_drawent_visualizer(
        osp::draw::DrawEnt const            ent,
        osp::draw::DrawEntSet_t const&      hasMaterial,
        osp::draw::RenderGroup::DrawEnts_t& rStorage,
        ACtxDrawMeshVisualizer&             rData)
{
    bool alreadyAdded = rStorage.contains(ent);
    if (hasMaterial.contains(ent))
    {
        if ( ! alreadyAdded)
        {
            rStorage.emplace( ent, osp::draw::EntityToDraw{&draw_ent_visualizer, {&rData} } );
        }
    }
    else
    {
        if (alreadyAdded)
        {
            rStorage.erase(ent);
        }
    }
}
template <typename ITA_T, typename ITB_T>
static void sync_drawent_visualizer(
        ITA_T const&                        first,
        ITB_T const&                        last,
        osp::draw::DrawEntSet_t const&      hasMaterial,
        osp::draw::RenderGroup::DrawEnts_t& rStorage,
        ACtxDrawMeshVisualizer&             rData)
{
    std::for_each(first, last, [&] (osp::draw::DrawEnt const ent)
    {
        sync_drawent_visualizer(ent, hasMaterial, rStorage, rData);
    });
}

} // namespace osp::shader
