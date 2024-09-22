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

#include "FullscreenTriShader.h"

#include <osp/core/strong_id.h>
#include <osp/drawing/drawing_fn.h>

#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/Renderbuffer.h>

#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/ImageData.h>

#include <longeron/id_management/registry_stl.hpp>

namespace osp::draw
{

using TexGlId  = osp::StrongId<std::uint32_t, struct DummyForTexGLId>;
using MeshGlId = osp::StrongId<std::uint32_t, struct DummyForMeshGlId>;

using TexGlStorage_t    = Storage_t<TexGlId, Magnum::GL::Texture2D>;
using MeshGlStorage_t   = Storage_t<MeshGlId, Magnum::GL::Mesh>;

/**
 * @brief Main renderer state and essential GL resources
 *
 * This may be shared between scenes
 */
struct RenderGL
{

    // Fullscreen Triangle
    MeshGlId                            m_fullscreenTri;
    FullscreenTriShader                 m_fullscreenTriShader{Corrade::NoCreate};

    // Offscreen Framebuffer
    TexGlId                             m_fboColor;
    Magnum::GL::Renderbuffer            m_fboDepthStencil{Corrade::NoCreate};
    Magnum::GL::Framebuffer             m_fbo{Corrade::NoCreate};

    // Renderer-space GL Textures
    lgrn::IdRegistryStl<TexGlId>        m_texIds;
    TexGlStorage_t                      m_texGl;

    // Renderer-space GL Meshes
    lgrn::IdRegistryStl<MeshGlId>       m_meshIds;
    MeshGlStorage_t                     m_meshGl;

    // Associate GL Texture Ids with resources
    IdMap_t<ResId, TexGlId>             m_resToTex;
    IdMap_t<TexGlId, ResIdOwner_t>      m_texToRes;

    // Associate GL Mesh Ids with resources
    IdMap_t<ResId, MeshGlId>            m_resToMesh;
    IdMap_t<MeshGlId, ResIdOwner_t>     m_meshToRes;

};

struct ACompTexGl
{
    TexId       m_scnId     {lgrn::id_null<TexId>()};
    TexGlId     m_glId      {lgrn::id_null<TexGlId>()};
};

struct ACompMeshGl
{
    MeshId      m_scnId     {lgrn::id_null<MeshId>()};
    MeshGlId    m_glId      {lgrn::id_null<MeshGlId>()};
};

using MeshGlEntStorage_t    = KeyedVec<DrawEnt, ACompMeshGl>;
using TexGlEntStorage_t     = KeyedVec<DrawEnt, ACompTexGl>;

/**
 * @brief OpenGL specific rendering components for rendering a scene
 */
struct ACtxSceneRenderGL
{
    MeshGlEntStorage_t      m_meshId;
    TexGlEntStorage_t       m_diffuseTexId;
};

/**
 * @brief OpenGL specific rendering functions
 */
class SysRenderGL
{

public:

    /**
     * @brief Setup essential GL resources
     *
     * This sets up an offscreen framebuffer and a fullscreen triangle
     *
     * @param rRenderGl [ref] Fresh default-constructed renderer state
     */
    static void setup_context(RenderGL& rRenderGl);

    /**
     * @brief Display a fullscreen texture to the default framebuffer
     *
     * @param rRenderGl [ref] Renderer state including fullscreen triangle
     * @param rTex      [in] Texture to display
     */
    static void display_texture(
            RenderGL& rRenderGl, Magnum::GL::Texture2D& rTex);

    static void clear_resource_owners(RenderGL& rRenderGl, Resources& rResources);

    /**
     * @brief Compile GPU-side TexGlIds for textures loaded from a Resource (TexId + ResId)
     *
     * @param rCtxDrawRes   [in] Resources used by the scene
     * @param rResources    [ref] Application Resources shared with the scene. New resource owners may be created.
     * @param rRenderGl     [ref] Renderer state
     */
    static void compile_resource_textures(
            ACtxDrawingRes const& rCtxDrawRes,
            Resources& rResources,
            RenderGL& rRenderGl);

    /**
     * @brief Compile GPU-side MeshGlIds for meshes loaded from a Resource (MeshId + ResId)
     *
     * @param rCtxDrawRes   [in] Resources used by the scene
     * @param rResources    [ref] Application Resources shared with the scene. New resource owners may be created.
     * @param rRenderGl     [ref] Renderer state
     */
    static void compile_resource_meshes(
            ACtxDrawingRes const& rCtxDrawRes,
            Resources& rResources,
            RenderGL& rRenderGl);

    /**
     * @brief Synchronize an entity's MeshId component to an ACompMeshGl
     *
     * @param ent           [in] DrawEnt with mesh to synchronize
     * @param cmpMeshIds    [in] Scene Mesh Id component
     * @param meshToRes     [in] Scene's Mesh Id to Resource Id
     * @param rCmpMeshGl    [ref] Renderer-side ACompMeshGl components
     * @param rRenderGl     [ref] Renderer state
     */
    static void sync_drawent_mesh(
            DrawEnt                                     ent,
            KeyedVec<DrawEnt, MeshIdOwner_t> const&     cmpMeshIds,
            IdMap_t<MeshId, ResIdOwner_t> const&        meshToRes,
            MeshGlEntStorage_t&                         rCmpMeshGl,
            RenderGL&                                   rRenderGl);

    template <typename ITA_T, typename ITB_T>
    static void sync_drawent_mesh(
            ITA_T const&                                first,
            ITB_T const&                                last,
            KeyedVec<DrawEnt, MeshIdOwner_t> const&     cmpMeshIds,
            IdMap_t<MeshId, ResIdOwner_t> const&        meshToRes,
            MeshGlEntStorage_t&                         rCmpMeshGl,
            RenderGL&                                   rRenderGl)
    {
        std::for_each(first, last, [&] (DrawEnt const ent)
        {
            sync_drawent_mesh(ent, cmpMeshIds, meshToRes, rCmpMeshGl, rRenderGl);
        });
    }

    /**
     * @brief Synchronize entities with a TexId component to an ACompTexGl
     *
     * @param cmpTexIds     [in] Scene Texture Id component
     * @param meshToRes     [in] Scene's Texture Id to Resource Id
     * @param entsDirty     [in] Entities to synchronize
     * @param rCmpTexGl     [ref] Renderer-side ACompTexGl components
     * @param rRenderGl     [ref] Renderer state
     */
    static void sync_drawent_texture(
            DrawEnt                                     ent,
            KeyedVec<DrawEnt, TexIdOwner_t> const&      cmpTexIds,
            IdMap_t<TexId, ResIdOwner_t> const&         texToRes,
            TexGlEntStorage_t&                          rCmpTexGl,
            RenderGL&                                   rRenderGl);

    template <typename ITA_T, typename ITB_T>
    static void sync_drawent_texture(
            ITA_T const&                                first,
            ITB_T const&                                last,
            KeyedVec<DrawEnt, TexIdOwner_t> const&      cmpTexIds,
            IdMap_t<TexId, ResIdOwner_t> const&         texToRes,
            TexGlEntStorage_t&                          rCmpTexGl,
            RenderGL&                                   rRenderGl)
    {
        std::for_each(first, last, [&] (DrawEnt const ent)
        {
            sync_drawent_texture(ent, cmpTexIds, texToRes, rCmpTexGl, rRenderGl);
        });
    }

    /**
     * @brief Call draw functions of a RenderGroup of opaque objects
     *
     * @param group     [in] RenderGroup to draw
     * @param visible   [in] Storage for visible components
     * @param viewProj  [in] View and projection matrix
     */
    static void render_opaque(
            RenderGroup const& group,
            DrawEntSet_t const& visible,
            ViewProjMatrix const& viewProj);

    /**
     * @brief Call draw functions of a RenderGroup of transparent objects
     *
     * Consider sorting the render group for correct transparency
     *
     * @param group     [in] RenderGroup to draw
     * @param visible   [in] Storage for visible components
     * @param viewProj  [in] View and projection matrix
     */
    static void render_transparent(
            RenderGroup const& group,
            DrawEntSet_t const& visible,
            ViewProjMatrix const& viewProj);

    static void draw_group(
            RenderGroup const& group,
            DrawEntSet_t const& visible,
            ViewProjMatrix const& viewProj);

};

} // namespace osp::draw
