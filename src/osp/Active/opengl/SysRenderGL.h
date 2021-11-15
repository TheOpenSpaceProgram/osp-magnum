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
 * Stores a mesh to be drawn by the renderer
 */
struct ACompMeshGL
{
    osp::DependRes<Magnum::GL::Mesh> m_mesh;
};

/**
 * Diffuse texture component
 */
struct ACompTextureGL
{
    osp::DependRes<Magnum::GL::Texture2D> m_tex;
};

struct ACtxRenderGL
{
    acomp_storage_t<ACompMeshGL>        m_meshGl;
    acomp_storage_t<ACompTextureGL>  m_diffuseTexGl;
};

class SysRenderGL
{

public:

    /**
     * @brief Setup essential GL resources
     *
     * @param rCtxResources [ref] Context resources Package
     */
    static void setup_context(Package& rGlResources);

    static void setup_forward_renderer(ACtxRenderGroups& rCtxGroups);


    /**
     * @brief Draw the default render target to the screen
     *
     * @param rScene [ref] Scene with render target
     */
    static void display_rendertarget(Package& rGlResources, Magnum::GL::Texture2D& rTex);

    static void load_meshes(
            acomp_view_t<ACompMesh const> viewMeshs,
            std::vector<ActiveEnt>& rDirty,
            acomp_storage_t<ACompMeshGL>& rMeshGl,
            osp::Package& rGlResources);

    static void load_textures(
            acomp_view_t<ACompTexture const> viewDiffTex,
            std::vector<ActiveEnt>& rDirty,
            acomp_storage_t<ACompTextureGL>& rDiffTexGl,
            osp::Package& rGlResources);

    static void render_opaque(
            ACtxRenderGroups& rCtxGroups,
            acomp_view_t<const ACompVisible> viewVisible,
            ACompCamera const& camera);

    static void render_transparent(
            ACtxRenderGroups& rCtxGroups,
            acomp_view_t<const ACompVisible> viewVisible,
            ACompCamera const& camera);
};



}
