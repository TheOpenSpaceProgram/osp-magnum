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

#include "osp/Resource/Package.h"
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
     * @brief Setup essential GL resources
     *
     * @param rCtxResources [ref] Context resources Package
     */
    static void setup_context(Package& rCtxResources);

    /**
     * @brief Setup Render Pipeline, Targets, and Groups for Forward Rendering
     *
     * @param rScene [ref] Scene with GL context
     */
    static void setup_forward_renderer(ActiveScene& rScene);

    /**
     * @brief Retrieve the entity which possesses the default ACompRenderTarget
     *
     * @param rScene [ref] Scene with render target
     */
    static ActiveEnt get_default_rendertarget(ActiveScene& rScene);

    /**
     * @brief Draw the default render target to the screen
     *
     * @param rScene [ref] Scene with render target
     */
    static void display_default_rendertarget(ActiveScene& rScene);

    /**
     * @brief Adds newly created and updated drawables to RenderGroups using
     *        their DrawAssigners
     *
     * @param rScene [ref] Scene with ACtxRenderGroups
     */
    static void update_drawfunc_assign(ActiveScene& rScene);

    /**
     * @brief Remove entities from draw functions
     *
     * @param rScene [ref] Scene with ACtxRenderGroups
     */
    static void update_drawfunc_delete(ActiveScene& rScene);

    /**
     * @brief Update the m_transformWorld of entities with ACompTransform and
     *        ACompHierarchy.
     *
     * Intended for physics interpolation
     */
    static void update_hierarchy_transforms(ActiveScene& rScene);

private:

    /**
     * @return RenderPipeline for forward rendering
     */
    static RenderPipeline create_forward_pipeline(ActiveScene& rScene);

}; // class SysRender

} // namespace osp::active
