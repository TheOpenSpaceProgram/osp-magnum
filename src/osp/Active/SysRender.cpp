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
#include "SysRender.h"

#include "osp/Shaders/Phong.h"
#include "adera/Shaders/PlumeShader.h"
#include <Magnum/Shaders/MeshVisualizer.h>
#include "osp/Shaders/FullscreenTriShader.h"

#include <Magnum/GL/Buffer.h>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/Mesh.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/GL/Renderbuffer.h>
#include <Magnum/GL/RenderbufferFormat.h>

using osp::active::SysRender;
using osp::active::RenderOrder_t;
using osp::active::RenderPipeline;

void SysRender::setup(ActiveScene& rScene)
{
    using namespace osp::shader;
    using namespace adera::shader;
    using namespace Magnum;

    Package& resources = rScene.get_context_resources();

    resources.add<FullscreenTriShader>("fullscreen_tri_shader");

    resources.add<Phong>("phong_shader",
        Phong{Magnum::Shaders::Phong::Flag::DiffuseTexture});

    /* Generate fullscreen tri for texture rendering */
    {
        Vector2 screenSize = Vector2{GL::defaultFramebuffer.viewport().size()};

        float aspectRatio = screenSize.x() / screenSize.y();

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
        resources.add<GL::Mesh>("fullscreen_tri", std::move(surfaceMesh));
    }

    /* Add an offscreen framebuffer */
    {
        Vector2i viewSize = GL::defaultFramebuffer.viewport().size();

        DependRes<GL::Texture2D> color = resources.add<GL::Texture2D>("offscreen_fbo_color");
        color->setStorage(1, GL::TextureFormat::RGB8, viewSize);

        DependRes<GL::Renderbuffer> depthStencil =
            resources.add<GL::Renderbuffer>("offscreen_fbo_depthStencil");
        depthStencil->setStorage(GL::RenderbufferFormat::Depth24Stencil8, viewSize);

        DependRes<GL::Framebuffer> fbo =
            resources.add<GL::Framebuffer>("offscreen_fbo", Range2Di{{0, 0}, viewSize});
        fbo->attachTexture(GL::Framebuffer::ColorAttachment{0}, *color, 0);
        fbo->attachRenderbuffer(GL::Framebuffer::BufferAttachment::DepthStencil, *depthStencil);


        rScene.reg_emplace<ACompRenderTarget>(rScene.hier_get_root(),
            viewSize, std::move(fbo));
        rScene.reg_emplace<ACompFBOColorAttachment>(rScene.hier_get_root(), color);
    }

    // Create the default render pipeline
    resources.add<RenderPipeline>("default", create_forward_renderer());
}

RenderPipeline SysRender::create_forward_renderer()
{
    /* Define render passes */
    RenderOrder_t pipeline;
    std::vector<RenderOrderHandle_t> handles;

    // Opaque pass
    handles.emplace_back(pipeline,
        "opaque_pass", "", "transparent_pass",
        [](ActiveScene& rScene, ACompCamera const& camera)
        {
            using namespace Magnum;
            using Magnum::GL::Renderer;

            auto& reg = rScene.get_registry();

            Renderer::enable(Renderer::Feature::DepthTest);
            Renderer::enable(Renderer::Feature::FaceCulling);
            Renderer::disable(Renderer::Feature::Blending);

            // Fetch opaque objects
            auto opaqueView = reg.view<ACompOpaque, ACompVisible, ACompShader>();

            SysRender::draw_group(rScene, opaqueView, camera);
        }
    );

    // Transparent pass
    handles.emplace_back(pipeline,
        "transparent_pass", "opaque_pass", "display_framebuffer",
        [](osp::active::ActiveScene& rScene, ACompCamera const& camera)
        {
            using Magnum::GL::Renderer;
            using namespace osp::active;

            auto& reg = rScene.get_registry();

            Renderer::enable(Renderer::Feature::DepthTest);
            Renderer::enable(Renderer::Feature::FaceCulling);
            Renderer::enable(Renderer::Feature::Blending);
            Renderer::setBlendFunction(
                Renderer::BlendFunction::SourceAlpha,
                Renderer::BlendFunction::OneMinusSourceAlpha);

            auto transparentView = reg.view<ACompTransparent, ACompVisible, ACompShader>();

            // Draw backfaces
            Renderer::setFaceCullingMode(Renderer::PolygonFacing::Front);
            SysRender::draw_group(rScene, transparentView, camera);

            // Draw frontfaces
            Renderer::setFaceCullingMode(Renderer::PolygonFacing::Back);
            SysRender::draw_group(rScene, transparentView, camera);
        }
    );

    return {pipeline};
}

osp::active::ActiveEnt SysRender::get_default_rendertarget(ActiveScene& rScene)
{
    return rScene.hier_get_root();
}

void SysRender::display_default_rendertarget(ActiveScene& rScene)
{
    using Magnum::GL::Renderer;
    using Magnum::GL::Framebuffer;
    using Magnum::GL::FramebufferClear;
    using Magnum::GL::Mesh;

    auto& target = rScene.reg_get<ACompFBOColorAttachment>(get_default_rendertarget(rScene));

    auto& resources = rScene.get_context_resources();

    DependRes<FullscreenTriShader> shader =
        resources.get<FullscreenTriShader>("fullscreen_tri_shader");
    DependRes<Mesh> surface = resources.get<Mesh>("fullscreen_tri");

    Magnum::GL::defaultFramebuffer.bind();

    Renderer::disable(Renderer::Feature::DepthTest);
    Renderer::disable(Renderer::Feature::FaceCulling);
    Renderer::disable(Renderer::Feature::Blending);

    shader->display_texure(*surface, *target.m_tex);
}

void SysRender::update_hierarchy_transforms(ActiveScene& rScene)
{

    auto &rReg = rScene.get_registry();

    rReg.sort<ACompDrawTransform, ACompHierarchy>();

    auto viewHier = rReg.view<ACompHierarchy>();
    auto viewTf = rReg.view<ACompTransform>();
    auto viewDrawTf = rReg.view<ACompDrawTransform>();

    for(ActiveEnt const entity : viewDrawTf)
    {
        auto const &hier = viewHier.get<ACompHierarchy>(entity);
        auto const &tf = viewTf.get<ACompTransform>(entity);
        auto &rDrawTf = viewDrawTf.get<ACompDrawTransform>(entity);

        if (hier.m_parent == rScene.hier_get_root())
        {
            // top level object, parent is root

            rDrawTf.m_transformWorld = tf.m_transform;

        }
        else
        {
            auto const& parentDrawTf
                    = viewDrawTf.get<ACompDrawTransform>(hier.m_parent);

            // set transform relative to parent
            rDrawTf.m_transformWorld = parentDrawTf.m_transformWorld
                                     * tf.m_transform;
        }
    }
}
