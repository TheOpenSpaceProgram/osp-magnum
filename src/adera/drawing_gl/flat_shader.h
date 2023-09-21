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

#include <osp/drawing_gl/rendergl.h>

#include <Magnum/Shaders/FlatGL.h>

namespace adera::shader
{

using FlatGL3D = Magnum::Shaders::FlatGL3D;


struct ACtxDrawFlat
{
    FlatGL3D                    shaderUntextured    {Corrade::NoCreate};
    FlatGL3D                    shaderDiffuse       {Corrade::NoCreate};

    osp::draw::DrawTransforms_t    *pDrawTf         {nullptr};
    osp::draw::DrawEntColors_t     *pColor          {nullptr};
    osp::draw::TexGlEntStorage_t   *pDiffuseTexId   {nullptr};
    osp::draw::MeshGlEntStorage_t  *pMeshId         {nullptr};

    osp::draw::TexGlStorage_t      *pTexGl          {nullptr};
    osp::draw::MeshGlStorage_t     *pMeshGl         {nullptr};

    osp::draw::MaterialId materialId { lgrn::id_null<osp::draw::MaterialId>() };

    constexpr void assign_pointers(osp::draw::ACtxSceneRender&   rScnRender,
                                   osp::draw::ACtxSceneRenderGL& rScnRenderGl,
                                   osp::draw::RenderGL&          rRenderGl) noexcept
    {
        pDrawTf         = &rScnRender   .m_drawTransform;
        pColor          = &rScnRender   .m_color;
        pDiffuseTexId   = &rScnRenderGl .m_diffuseTexId;
        pMeshId         = &rScnRenderGl .m_meshId;
        pTexGl          = &rRenderGl    .m_texGl;
        pMeshGl         = &rRenderGl    .m_meshGl;
    }
};

void draw_ent_flat(
        osp::draw::DrawEnt                   ent,
        osp::draw::ViewProjMatrix const&     viewProj,
        osp::draw::EntityToDraw::UserData_t  userData) noexcept;

struct ArgsForSyncDrawEntFlat
{
    osp::draw::DrawEntSet_t const&              hasMaterial;
    osp::draw::RenderGroup::DrawEnts_t *const   pStorageOpaque;
    osp::draw::RenderGroup::DrawEnts_t *const   pStorageTransparent;
    osp::draw::DrawEntSet_t const&              opaque;
    osp::draw::DrawEntSet_t const&              transparent;
    osp::draw::TexGlEntStorage_t const&         diffuse;
    ACtxDrawFlat&                               rData;
};

inline void sync_drawent_flat(osp::draw::DrawEnt ent, ArgsForSyncDrawEntFlat const args)
{
    auto const entInt = std::size_t(ent);

    bool const hasMaterial = args.hasMaterial.test(entInt);
    bool const hasTexture =    (args.diffuse.size() > std::size_t(ent))
                            && (args.diffuse[ent].m_glId != lgrn::id_null<osp::draw::TexGlId>());

    FlatGL3D *pShader = hasTexture
                      ? &args.rData.shaderDiffuse
                      : &args.rData.shaderUntextured;

    if (args.pStorageTransparent != nullptr)
    {
        auto value = (hasMaterial && args.transparent.test(entInt))
                   ? std::make_optional(osp::draw::EntityToDraw{&draw_ent_flat, {&args.rData, pShader}})
                   : std::nullopt;

        osp::storage_assign(*args.pStorageTransparent, ent, std::move(value));
    }

    if (args.pStorageOpaque != nullptr)
    {
        auto value = (hasMaterial && args.opaque.test(entInt))
                   ? std::make_optional(osp::draw::EntityToDraw{&draw_ent_flat, {&args.rData, pShader}})
                   : std::nullopt;

        osp::storage_assign(*args.pStorageOpaque, ent, std::move(value));
    }
}

template<typename ITA_T, typename ITB_T>
void sync_drawent_flat(
        ITA_T const&                    first,
        ITB_T const&                    last,
        ArgsForSyncDrawEntFlat const    args)
{
    std::for_each(first, last, [&args] (osp::draw::DrawEnt const ent)
    {
        sync_drawent_flat(ent, args);
    });
}


} // namespace osp::shader
