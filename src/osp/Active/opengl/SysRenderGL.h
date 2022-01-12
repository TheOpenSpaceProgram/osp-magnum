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

#include "osp/Resource/Package.h"
#include "../SysRender.h"

#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/Renderbuffer.h>

namespace osp::active
{

/**
 * @brief Stores a mesh to be drawn by the renderer
 */
struct ACompMeshGL
{
    osp::DependRes<Magnum::GL::Mesh> m_mesh;
};

/**
 * @brief Diffuse texture component
 */
struct ACompTextureGL
{
    osp::DependRes<Magnum::GL::Texture2D> m_tex;
};

/**
 * @brief OpenGL specific rendering components
 */
struct ACtxRenderGL
{
    acomp_storage_t<ACompMeshGL>    m_meshGl;
    acomp_storage_t<ACompTextureGL> m_diffuseTexGl;
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
     * @param rGlResources [ref] Context resources Package
     */
    static void setup_context(Package& rGlResources);

    /**
     * @brief Display a fullscreen texture to the default framebuffer
     *
     * @param rGlResources  [ref] GL resources containing fullscreen triangle
     * @param rTex          [in] Texture to display
     */
    static void display_texture(Package& rGlResources,
                                Magnum::GL::Texture2D& rTex);

    /**
     * @brief Compile and assign GPU mesh components to entities with mesh
     *        data components
     *
     * Entities with an ACompMesh will be synchronized with an ACompMeshGL
     *
     * @param meshes        [in] Storage for generic mesh components
     * @param rDirty        [ref] Vector of entities that have updated meshes,
     *                            this will be cleared.
     * @param rMeshGl       [ref] Storage for GL mesh components
     * @param rGlResources  [out] Package to store newly compiled meshes
     */
    static void compile_meshes(
            acomp_storage_t<ACompMesh> const& meshes,
            std::vector<ActiveEnt>& rDirty,
            acomp_storage_t<ACompMeshGL>& rMeshGl,
            osp::Package& rGlResources);

    /**
     * @brief Compile and assign GPU texture components to entities with
     *        texture data components
     *
     * @param textures      [in] Storage for generic texture data components
     * @param rDirty        [ref] Vector of entities that have updated meshes,
     *                            this will be cleared.
     * @param rDiffTexGl    [ref] Storage for GL texture components
     * @param rGlResources  [out] Package to store newly compiled textures
     */
    static void compile_textures(
            acomp_storage_t<ACompTexture> const& textures,
            std::vector<ActiveEnt>& rDirty,
            acomp_storage_t<ACompTextureGL>& rDiffTexGl,
            osp::Package& rGlResources);

    /**
     * @brief Call draw functions of a RenderGroup of opaque objects
     *
     * @param group         [in] RenderGroup to draw
     * @param visible       [in] Storage for visible components
     * @param camera        [in] Camera to render from
     */
    static void render_opaque(
            RenderGroup const& group,
            acomp_storage_t<ACompVisible> const& visible,
            ACompCamera const& camera);

    /**
     * @brief Call draw functions of a RenderGroup of transparent objects
     *
     * Consider sorting the render group for correct transparency
     *
     * @param group         [in] RenderGroup to draw
     * @param visible       [in] Storage for visible components
     * @param camera        [in] Camera to render from
     */
    static void render_transparent(
            RenderGroup const& group,
            acomp_storage_t<ACompVisible> const& visible,
            ACompCamera const& camera);

    static void draw_group(
            RenderGroup const& group,
            acomp_storage_t<ACompVisible> const& visible,
            ACompCamera const& camera);
};



}
