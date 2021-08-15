/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
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

#include "osp/Resource/Resource.h"
#include "osp/Active/Shader.h"
#include "osp/Active/activetypes.h"

#include "drawing.h"

#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/Renderbuffer.h>

namespace osp::active
{

/**
 * Stores a mesh to be drawn by the renderer
 */
struct ACompMesh
{
    osp::DependRes<Magnum::GL::Mesh> m_mesh;
};

/**
 * Diffuse texture component
 */
struct ACompDiffuseTex
{
    osp::DependRes<Magnum::GL::Texture2D> m_tex;
};

/**
 * A surface that can be rendered to
 */
struct ACompRenderTarget
{
    Magnum::Vector2i m_size;
    osp::DependRes<Magnum::GL::Framebuffer> m_fbo;
};

/**
 * Stores the name of the renderer to be used by a rendering agent
 */
struct ACompRenderer
{
    std::string m_name{"default"};
};

/**
 * Stores a reference to the color attachment texture of a render target entity
 */
struct ACompFBOColorAttachment
{
    DependRes<Magnum::GL::Texture2D> m_tex;
};

using RenderStep_t = void(*)(ActiveScene&, ACompCamera const&);

/**
 * Resource storing the steps of a multipass renderer
 */
struct RenderPipeline
{
    std::vector<RenderStep_t> m_order;
};

class SysRender
{
public:

    /**
     * @brief setup_context
     *
     * @param rCtxResources
     */
    static void setup_context(Package& rCtxResources);

    /**
     * @brief Setup Forward Renderer
     *
     * @param rScene [ref]
     */
    static void setup_forward_renderer(ActiveScene& rScene);

    /* Retrieve the entity which possessses the default rendertarget component */
    static ActiveEnt get_default_rendertarget(ActiveScene& rScene);

    /* Draw the default render target to the screen */
    static void display_default_rendertarget(ActiveScene& rScene);

    /**
     * @brief
     *
     * @param rScene
     */
    static void update_drawfunc_assign(ActiveScene& rScene);

    /**
     * Update the m_transformWorld of entities with ACompTransform and
     * ACompHierarchy. Intended for physics interpolation
     */
    static void update_hierarchy_transforms(ActiveScene& rScene);

private:
    /* Define render passes */
    static RenderPipeline create_forward_pipeline(ActiveScene& rScene);

};

} // namespace osp::active
