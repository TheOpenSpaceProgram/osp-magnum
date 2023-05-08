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

#include "../SysRender.h"
#include "../../id_map.h"
#include "../../Shaders/FullscreenTriShader.h"

#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/Renderbuffer.h>

#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/ImageData.h>

#include <longeron/id_management/registry.hpp>

namespace osp::active
{

enum class TexGlId : uint32_t { };
enum class MeshGlId : uint32_t { };

using TexGlStorage_t    = entt::basic_storage<Magnum::GL::Texture2D, TexGlId>;
using MeshGlStorage_t   = entt::basic_storage<Magnum::GL::Mesh, MeshGlId>;

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
    lgrn::IdRegistry<TexGlId>           m_texIds;
    TexGlStorage_t                      m_texGl;

    // Renderer-space GL Meshes
    lgrn::IdRegistry<MeshGlId>          m_meshIds;
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
    DrawTransforms_t        m_drawTransform;
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
     * @brief Compile required meshes and textures resources used by a scene
     *
     * @param rCtxDrawRes   [in] Resources used by the scene
     * @param rResources    [ref] Application Resources shared with the scene.
     *                            New resource owners may be created.
     * @param rRenderGl     [ref] Renderer state
     */
    static void sync_scene_resources(
            ACtxDrawingRes const& rCtxDrawRes,
            Resources& rResources,
            RenderGL& rRenderGl);

    /**
     * @brief Synchronize entities with a MeshId component to an ACompMeshGl
     *
     * @param cmpMeshIds    [in] Scene Mesh Id component
     * @param meshToRes     [in] Scene's Mesh Id to Resource Id
     * @param entsDirty     [in] Entities to synchronize
     * @param rCmpMeshGl    [ref] Renderer-side ACompMeshGl components
     * @param rRenderGl     [ref] Renderer state
     */
    static void assign_meshes(
            KeyedVec<DrawEnt, MeshIdOwner_t> const&     cmpMeshIds,
            IdMap_t<MeshId, ResIdOwner_t> const&        meshToRes,
            std::vector<DrawEnt> const&                 entsDirty,
            MeshGlEntStorage_t&                         rCmpMeshGl,
            RenderGL&                                   rRenderGl);

    /**
     * @brief Synchronize entities with a TexId component to an ACompTexGl
     *
     * @param cmpTexIds     [in] Scene Texture Id component
     * @param meshToRes     [in] Scene's Texture Id to Resource Id
     * @param entsDirty     [in] Entities to synchronize
     * @param rCmpTexGl     [ref] Renderer-side ACompTexGl components
     * @param rRenderGl     [ref] Renderer state
     */
    static void assign_textures(
            KeyedVec<DrawEnt, TexIdOwner_t> const&      cmpTexIds,
            IdMap_t<TexId, ResIdOwner_t> const&         texToRes,
            std::vector<DrawEnt> const&                 entsDirty,
            TexGlEntStorage_t&                          rCmpTexGl,
            RenderGL&                                   rRenderGl);

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



}
