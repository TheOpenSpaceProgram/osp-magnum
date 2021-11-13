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

#include "SysRenderGL.h"

#include "../../Shaders/FullscreenTriShader.h"

#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/GL/RenderbufferFormat.h>

#include <Corrade/Containers/ArrayViewStl.h>

#include <Magnum/Mesh.h>


using osp::active::SysRenderGL;

void SysRenderGL::setup_context(Package& rGlResources)
{
    using namespace Magnum;

    rGlResources.add<FullscreenTriShader>("fullscreen_tri_shader");

    /* Generate fullscreen tri for texture rendering */
    {
        Vector2 screenSize = Vector2{GL::defaultFramebuffer.viewport().size()};

        static constexpr std::array<float, 12> surfData
        {
            // Vert position    // UV coordinate
            -1.0f,  1.0f,       0.0f,  1.0f,
            -1.0f, -3.0f,       0.0f, -1.0f,
             3.0f,  1.0f,       2.0f,  1.0f
        };

        GL::Buffer surface(surfData, GL::BufferUsage::StaticDraw);
        GL::Mesh surfaceMesh;
        surfaceMesh
            .setPrimitive(Magnum::MeshPrimitive::Triangles)
            .setCount(3)
            .addVertexBuffer(std::move(surface), 0,
                FullscreenTriShader::Position{},
                FullscreenTriShader::TextureCoordinates{});
        rGlResources.add<GL::Mesh>("fullscreen_tri", std::move(surfaceMesh));
    }

    /* Add an offscreen framebuffer */
    {
        Vector2i viewSize = GL::defaultFramebuffer.viewport().size();

        DependRes<GL::Texture2D> color = rGlResources.add<GL::Texture2D>("offscreen_fbo_color");
        color->setStorage(1, GL::TextureFormat::RGB8, viewSize);

        DependRes<GL::Renderbuffer> depthStencil
                = rGlResources.add<GL::Renderbuffer>("offscreen_fbo_depthStencil");
        depthStencil->setStorage(GL::RenderbufferFormat::Depth24Stencil8, viewSize);

        DependRes<GL::Framebuffer> fbo
                = rGlResources.add<GL::Framebuffer>("offscreen_fbo", Range2Di{{0, 0}, viewSize});
        fbo->attachTexture(GL::Framebuffer::ColorAttachment{0}, *color, 0);
        fbo->attachRenderbuffer(GL::Framebuffer::BufferAttachment::DepthStencil, *depthStencil);
    }
}

void SysRenderGL::setup_forward_renderer(
        Package& rGlResources, ACtxRenderGroups& rCtxGroups,
        FBOTarget& rTarget, FBOColorAttachment& rTargetColor)
{
    Vector2i const viewSize = Magnum::GL::defaultFramebuffer.viewport().size();

    rTarget = { viewSize, rGlResources.get<Magnum::GL::Framebuffer>("offscreen_fbo") };
    rTargetColor = { rGlResources.get<Magnum::GL::Texture2D>("offscreen_fbo_color") };

    // Add render groups
    rCtxGroups.m_groups.emplace("fwd_opaque", RenderGroup{});
    rCtxGroups.m_groups.emplace("fwd_transparent", RenderGroup{});
}

void SysRenderGL::display_rendertarget(Package& rGlResources, FBOColorAttachment& rTargetColor)
{
    using Magnum::GL::Renderer;
    using Magnum::GL::Framebuffer;
    using Magnum::GL::FramebufferClear;
    using Magnum::GL::Mesh;

    DependRes<FullscreenTriShader> shader
            = rGlResources.get<FullscreenTriShader>("fullscreen_tri_shader");
    DependRes<Mesh> surface = rGlResources.get<Mesh>("fullscreen_tri");

    Magnum::GL::defaultFramebuffer.bind();

    Renderer::disable(Renderer::Feature::DepthTest);
    Renderer::disable(Renderer::Feature::FaceCulling);
    Renderer::disable(Renderer::Feature::Blending);
    Renderer::setDepthMask(true);

    shader->display_texure(*surface, *rTargetColor.m_tex);
}


void SysRenderGL::render_opaque(
        ACtxRenderGroups rCtxGroups,
        acomp_view_t<const ACompVisible> viewVisible,
        ACompCamera const& camera)
{
    using Magnum::GL::Renderer;

    Renderer::enable(Renderer::Feature::DepthTest);
    Renderer::enable(Renderer::Feature::FaceCulling);
    Renderer::disable(Renderer::Feature::Blending);
    Renderer::setDepthMask(true);

    for (auto const& [ent, toDraw]
         : rCtxGroups.m_groups.at("fwd_opaque").view().each())
    {
        if (viewVisible.contains(ent))
        {
            toDraw(ent, camera);
        }
    }
}

void SysRenderGL::render_transparent(
        ACtxRenderGroups rCtxGroups,
        acomp_view_t<const ACompVisible> viewVisible,
        ACompCamera const& camera)
{
    using Magnum::GL::Renderer;
    using namespace osp::active;

    Renderer::enable(Renderer::Feature::DepthTest);
    Renderer::enable(Renderer::Feature::FaceCulling);
    Renderer::enable(Renderer::Feature::Blending);
    Renderer::setBlendFunction(
            Renderer::BlendFunction::SourceAlpha,
            Renderer::BlendFunction::OneMinusSourceAlpha);

    // temporary: disabled depth writing makes the plumes look nice, but
    //            can mess up other transparent objects once added
    Renderer::setDepthMask(false);

    for (auto const& [ent, toDraw]
         : rCtxGroups.m_groups.at("fwd_transparent").view().each())
    {
        if (viewVisible.contains(ent))
        {
            toDraw(ent, camera);
        }
    }
}
