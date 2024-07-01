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

#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MeshData.h>
#include <godot_cpp/variant/rid.hpp>
#include <longeron/id_management/registry_stl.hpp>
#include <osp/drawing/drawing_fn.h>

namespace osp::draw
{

enum class TexGdId : uint32_t
{
};
enum class MeshGdId : uint32_t
{
};

using TexGdStorage_t  = Storage_t<TexGdId, godot::RID>;
using MeshGdStorage_t = Storage_t<MeshGdId, godot::RID>;

/**
 * @brief Main renderer state and essential GL resources
 *
 * This may be shared between scenes
 */
struct RenderGd
{

    // Fullscreen Triangle
    MeshGdId m_fullscreenTri;
    // FullscreenTriShader                 m_fullscreenTriShader{Corrade::NoCreate};

    // Offscreen Framebuffer
    TexGdId m_fboColor;
    // Magnum::GL::Renderbuffer            m_fboDepthStencil{Corrade::NoCreate};
    // Magnum::GL::Framebuffer             m_fbo{Corrade::NoCreate};

    // Renderer-space GL Textures
    lgrn::IdRegistryStl<TexGdId> m_texIds;
    TexGdStorage_t               m_texGd;

    // Renderer-space GL Meshes
    lgrn::IdRegistryStl<MeshGdId> m_meshIds;
    MeshGdStorage_t               m_meshGd;

    // Associate GL Texture Ids with resources
    IdMap_t<ResId, TexGdId>        m_resToTex;
    IdMap_t<TexGdId, ResIdOwner_t> m_texToRes;

    // Associate GL Mesh Ids with resources
    IdMap_t<ResId, MeshGdId>        m_resToMesh;
    IdMap_t<MeshGdId, ResIdOwner_t> m_meshToRes;

    // godot resources
    godot::RID scenario;
    godot::RID viewport;
};

struct ACompTexGd
{
    TexId   m_scnId{ lgrn::id_null<TexId>() };
    TexGdId m_gdId{ lgrn::id_null<TexGdId>() };
};

struct ACompMeshGd
{
    MeshId   m_scnId{ lgrn::id_null<MeshId>() };
    MeshGdId m_glId{ lgrn::id_null<MeshGdId>() };
};

using MeshGdEntStorage_t     = KeyedVec<DrawEnt, ACompMeshGd>;
using TexGdEntStorage_t      = KeyedVec<DrawEnt, ACompTexGd>;
using InstanceGdEntStorage_t = KeyedVec<DrawEnt, godot::RID>;

/**
 * @brief OpenGL specific rendering components for rendering a scene
 */
struct ACtxSceneRenderGd
{
    MeshGdEntStorage_t     m_meshId;
    TexGdEntStorage_t      m_diffuseTexId;
    InstanceGdEntStorage_t m_instanceId;
    godot::RID             m_scenario;
};

/**
 * @brief OpenGL specific rendering functions
 */
class SysRenderGd
{

public:
    static void clear_resource_owners(RenderGd &rRenderGd, Resources &rResources);

    /**
     * @brief Compile GPU-side TexGlIds for textures loaded from a Resource (TexId + ResId)
     *
     * @param rCtxDrawRes   [in] Resources used by the scene
     * @param rResources    [ref] Application Resources shared with the scene. New resource owners
     * may be created.
     * @param rRenderGd     [ref] Renderer state
     */
    static void compile_resource_textures(
        ACtxDrawingRes const &rCtxDrawRes, Resources &rResources, RenderGd &rRenderGd);

    /**
     * @brief Compile GPU-side MeshGlIds for meshes loaded from a Resource (MeshId + ResId)
     *
     * @param rCtxDrawRes   [in] Resources used by the scene
     * @param rResources    [ref] Application Resources shared with the scene. New resource owners
     * may be created.
     * @param rRenderGd     [ref] Renderer state
     */
    static void compile_resource_meshes(
        ACtxDrawingRes const &rCtxDrawRes, Resources &rResources, RenderGd &rRenderGd);

    /**
     * @brief Synchronize an entity's MeshId component to an ACompMeshGl
     *
     * @param ent           [in] DrawEnt with mesh to synchronize
     * @param cmpMeshIds    [in] Scene Mesh Id component
     * @param meshToRes     [in] Scene's Mesh Id to Resource Id
     * @param rCmpMeshGl    [ref] Renderer-side ACompMeshGl components
     * @param rRenderGd     [ref] Renderer state
     */
    static void sync_drawent_mesh(DrawEnt                                 ent,
                                  KeyedVec<DrawEnt, MeshIdOwner_t> const &cmpMeshIds,
                                  IdMap_t<MeshId, ResIdOwner_t> const    &meshToRes,
                                  MeshGdEntStorage_t                     &rCmpMeshGl,
                                  InstanceGdEntStorage_t                 &rCmpInstanceGd,
                                  RenderGd                               &rRenderGd);

    template <typename ITA_T, typename ITB_T>
    static void sync_drawent_mesh(ITA_T const                            &first,
                                  ITB_T const                            &last,
                                  KeyedVec<DrawEnt, MeshIdOwner_t> const &cmpMeshIds,
                                  IdMap_t<MeshId, ResIdOwner_t> const    &meshToRes,
                                  MeshGdEntStorage_t                     &rCmpMeshGl,
                                  InstanceGdEntStorage_t                 &rCmpInstanceGd,
                                  RenderGd                               &rRenderGd)
    {
        std::for_each(first, last, [&](DrawEnt const ent) {
            sync_drawent_mesh(ent, cmpMeshIds, meshToRes, rCmpMeshGl, rCmpInstanceGd, rRenderGd);
        });
    }

    /**
     * @brief Synchronize entities with a TexId component to an ACompTexGl
     *
     * @param cmpTexIds     [in] Scene Texture Id component
     * @param meshToRes     [in] Scene's Texture Id to Resource Id
     * @param entsDirty     [in] Entities to synchronize
     * @param rCmpTexGl     [ref] Renderer-side ACompTexGl components
     * @param rRenderGd     [ref] Renderer state
     */
    static void sync_drawent_texture(DrawEnt                                ent,
                                     KeyedVec<DrawEnt, TexIdOwner_t> const &cmpTexIds,
                                     IdMap_t<TexId, ResIdOwner_t> const    &texToRes,
                                     TexGdEntStorage_t                     &rCmpTexGl,
                                     RenderGd                              &rRenderGd);

    template <typename ITA_T, typename ITB_T>
    static void sync_drawent_texture(ITA_T const                           &first,
                                     ITB_T const                           &last,
                                     KeyedVec<DrawEnt, TexIdOwner_t> const &cmpTexIds,
                                     IdMap_t<TexId, ResIdOwner_t> const    &texToRes,
                                     TexGdEntStorage_t                     &rCmpTexGl,
                                     RenderGd                              &rRenderGd)
    {
        std::for_each(first, last, [&](DrawEnt const ent) {
            sync_drawent_texture(ent, cmpTexIds, texToRes, rCmpTexGl, rRenderGd);
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
        RenderGroup const &group, DrawEntSet_t const &visible, ViewProjMatrix const &viewProj);

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
        RenderGroup const &group, DrawEntSet_t const &visible, ViewProjMatrix const &viewProj);

    static void draw_group(
        RenderGroup const &group, DrawEntSet_t const &visible, ViewProjMatrix const &viewProj);
};

} // namespace osp::draw
