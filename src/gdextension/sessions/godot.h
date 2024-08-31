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
#pragma once

#include "render.h"

#include <osp/framework/builder.h>

#include <osp/activescene/basic.h>
#include <osp/drawing/drawing.h>

namespace godot
{
class FlyingScene;
}

namespace ospgdext
{
using namespace osp;


extern osp::fw::FeatureDef const ftrGodot;

/**
 * @brief stuff needed to render a scene using Magnum
 */
extern osp::fw::FeatureDef const ftrGodotScene;

extern osp::fw::FeatureDef const ftrFlatMaterial;

extern osp::fw::FeatureDef const ftrCameraControlGD;

#if 0
/**
 * @brief Magnum MeshVisualizer shader and optional material for drawing ActiveEnts with it
 */
osp::Session setup_shader_visualizer(
    osp::TopTaskBuilder      &rBuilder,
    osp::ArrayView<entt::any> topData,
    osp::Session const       &windowApp,
    osp::Session const       &sceneRenderer,
    osp::Session const       &magnum,
    osp::Session const       &magnumScene,
    osp::draw::MaterialId     materialId = lgrn::id_null<osp::draw::MaterialId>());

/**
 * @brief Magnum Flat shader and optional material for drawing ActiveEnts with it
 */
osp::Session setup_flat_draw(
    osp::TopTaskBuilder      &rBuilder,
    osp::ArrayView<entt::any> topData,
    osp::Session const       &windowApp,
    osp::Session const       &sceneRenderer,
    osp::Session const       &godot,
    osp::Session const       &godotScene,
    osp::draw::MaterialId     materialId = lgrn::id_null<osp::draw::MaterialId>());

/**
 * @brief Magnum Phong shader and optional material for drawing ActiveEnts with it
 */
osp::Session setup_shader_phong(
    osp::TopTaskBuilder      &rBuilder,
    osp::ArrayView<entt::any> topData,
    osp::Session const       &windowApp,
    osp::Session const       &sceneRenderer,
    osp::Session const       &magnum,
    osp::Session const       &magnumScene,
    osp::draw::MaterialId     materialId = lgrn::id_null<osp::draw::MaterialId>());

Session setup_camera_ctrl_godot(TopTaskBuilder      &rBuilder,
                                ArrayView<entt::any> topData,
                                Session const       &windowApp,
                                Session const       &sceneRenderer,
                                Session const       &magnumScene);
#endif
struct ACtxDrawFlat
{

    osp::draw::DrawTransforms_t       *pDrawTf{ nullptr };
    osp::draw::DrawEntColors_t        *pColor{ nullptr };
    osp::draw::TexGdEntStorage_t      *pDiffuseTexId{ nullptr };
    osp::draw::MeshGdEntStorage_t     *pMeshId{ nullptr };
    osp::draw::InstanceGdEntStorage_t *pInstanceId{ nullptr };

    osp::draw::TexGdStorage_t         *pTexGl{ nullptr };
    osp::draw::MeshGdStorage_t        *pMeshGl{ nullptr };

    osp::draw::MaterialId              materialId{ lgrn::id_null<osp::draw::MaterialId>() };

    godot::RID                         *scenario;

    constexpr void                     assign_pointers(osp::draw::ACtxSceneRender   &rScnRender,
                                                       osp::draw::ACtxSceneRenderGd &rScnRenderGd,
                                                       osp::draw::RenderGd          &rRenderGd) noexcept
    {
        pDrawTf       = &rScnRender.m_drawTransform;
        pColor        = &rScnRender.m_color;
        pDiffuseTexId = &rScnRenderGd.m_diffuseTexId;
        pMeshId       = &rScnRenderGd.m_meshId;
        pTexGl        = &rRenderGd.m_texGd;
        pMeshGl       = &rRenderGd.m_meshGd;
        pInstanceId   = &rScnRenderGd.m_instanceId;
        scenario      = &rRenderGd.scenario;
    }
};

void draw_ent_flat(osp::draw::DrawEnt                  ent,
                   osp::draw::ViewProjMatrix const    &viewProj,
                   osp::draw::EntityToDraw::UserData_t userData) noexcept;

struct ArgsForSyncDrawEntFlat
{
    osp::draw::DrawEntSet_t const             &hasMaterial;
    osp::draw::RenderGroup::DrawEnts_t * const pStorageOpaque;
    osp::draw::RenderGroup::DrawEnts_t * const pStorageTransparent;
    osp::draw::DrawEntSet_t const             &opaque;
    osp::draw::DrawEntSet_t const             &transparent;
    osp::draw::TexGdEntStorage_t const        &diffuse;
    ACtxDrawFlat                              &rData;
};

inline void sync_drawent_flat(osp::draw::DrawEnt ent, ArgsForSyncDrawEntFlat const args)
{
    bool const hasMaterial = args.hasMaterial.contains(ent);
    bool const hasTexture  = (args.diffuse.size() > std::size_t(ent))
                            && (args.diffuse[ent].m_gdId != lgrn::id_null<osp::draw::TexGdId>());

    if ( args.pStorageTransparent != nullptr )
    {
        auto value = (hasMaterial && args.transparent.contains(ent)) ? std::make_optional(
                         osp::draw::EntityToDraw{ &draw_ent_flat, { &args.rData } })
                                                                     : std::nullopt;

        osp::storage_assign(*args.pStorageTransparent, ent, value);
    }

    if ( args.pStorageOpaque != nullptr )
    {
        auto value = (hasMaterial && args.opaque.contains(ent)) ? std::make_optional(
                         osp::draw::EntityToDraw{ &draw_ent_flat, { &args.rData } })
                                                                : std::nullopt;

        osp::storage_assign(*args.pStorageOpaque, ent, value);
    }
}

template <typename ITA_T, typename ITB_T>
void sync_drawent_flat(ITA_T const &first, ITB_T const &last, ArgsForSyncDrawEntFlat const args)
{
    std::for_each(first, last, [&args](osp::draw::DrawEnt const ent) {
        sync_drawent_flat(ent, args);
    });
}

} // namespace ospgdext::scenes

