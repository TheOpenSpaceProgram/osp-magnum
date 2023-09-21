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

    osp::active::DrawTransforms_t       *m_pDrawTf{nullptr};
    osp::active::MeshGlEntStorage_t     *m_pMeshId{nullptr};
    osp::active::MeshGlStorage_t        *m_pMeshGl{nullptr};

    osp::active::MaterialId             m_materialId { lgrn::id_null<osp::active::MaterialId>() };

    bool m_wireframeOnly{false};

constexpr void assign_pointers(active::ACtxSceneRender&         rScnRender,
                                   active::ACtxSceneRenderGL&   rScnRenderGl,
                                   active::RenderGL&            rRenderGl) noexcept
    {
        m_pDrawTf   = &rScnRender.m_drawTransform;
        m_pMeshId   = &rScnRenderGl.m_meshId;
        m_pMeshGl   = &rRenderGl.m_meshGl;
    }
};

void draw_ent_visualizer(
        active::DrawEnt                     ent,
        active::ViewProjMatrix const&       viewProj,
        active::EntityToDraw::UserData_t    userData) noexcept;

inline void sync_drawent_visualizer(
        active::DrawEnt const               ent,
        active::DrawEntSet_t const&         hasMaterial,
        active::RenderGroup::Storage_t&     rStorage,
        ACtxDrawMeshVisualizer&             rData)
{
    using namespace active;

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
}
template <typename ITA_T, typename ITB_T>
static void sync_drawent_visualizer(
        ITA_T const&                        first,
        ITB_T const&                        last,
        active::DrawEntSet_t const&         hasMaterial,
        active::RenderGroup::Storage_t&     rStorage,
        ACtxDrawMeshVisualizer&             rData)
{
    std::for_each(first, last, [&] (active::DrawEnt const ent)
    {
        sync_drawent_visualizer(ent, hasMaterial, rStorage, rData);
    });
}

} // namespace osp::shader
