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

#include <array>

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/Renderbuffer.h>
#include <Magnum/GL/RenderbufferFormat.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/Shaders/MeshVisualizer.h>
#include <Magnum/Mesh.h>
#include <Magnum/Math/Range.h>
#include <Corrade/Containers/ArrayViewStl.h>

#include "SysDebugRender.h"
#include "ActiveScene.h"
#include "adera/Shaders/Phong.h"
#include "adera/Shaders/PlumeShader.h"
#include <osp/Shaders/RenderTexture.h>


using namespace Magnum;

using namespace osp::active;

// for _1, _2, _3, ... std::bind arguments
using namespace std::placeholders;

// for the 0xrrggbb_rgbf and _deg literals
using namespace Magnum::Math::Literals;


void SysDebugRender::add_functions(ActiveScene &rScene)
{
    initialize_context_resources(rScene);
    configure_render_passes(rScene);
}

void SysDebugRender::display_framebuffer(ActiveScene& rScene, GL::Texture2D& rTexture)
{
    auto& resources = rScene.get_context_resources();

    DependRes<GL::Mesh> surface = resources.get<GL::Mesh>("fullscreen_tri");
    DependRes<RenderTexture> shader = resources.get<RenderTexture>("render_texture");

    shader->render_texure(*surface, rTexture);
}

void SysDebugRender::initialize_context_resources(ActiveScene& rScene)
{
    Package& resources = rScene.get_context_resources();

    using namespace adera::shader;
    using Magnum::Shaders::MeshVisualizer3D;

    /* Initialize shader programs */

    resources.add<MeshVisualizer3D>("mesh_vis_shader",
        MeshVisualizer3D{MeshVisualizer3D::Flag::Wireframe | MeshVisualizer3D::Flag::NormalDirection});

    resources.add<Phong>("phong_shader",
        Phong{Magnum::Shaders::Phong::Flag::DiffuseTexture});

    resources.add<PlumeShader>("plume_shader");

    resources.add<RenderTexture>("render_texture");

    // Generate fullscreen tri for texture rendering
    using namespace Magnum;
    if (resources.get<GL::Mesh>("fullscreen_tri").empty())
    {
        Vector2 screenSize = Vector2{GL::defaultFramebuffer.viewport().size()};

        float aspectRatio = screenSize.x() / screenSize.y();

        std::array<float, 12> surfData
        {
            // Vert position    // UV coordinate
            -1.0f,  1.0f,       0.0f,  1.0f,
            -1.0f, -3.0f,       0.0f, -1.0f,
             3.0f,  1.0f,       2.0f,  1.0f
        };

        GL::Buffer surface(std::move(surfData), GL::BufferUsage::StaticDraw);
        GL::Mesh surfaceMesh;
        surfaceMesh
            .setPrimitive(Magnum::MeshPrimitive::Triangles)
            .setCount(3)
            .addVertexBuffer(std::move(surface), 0,
                RenderTexture::Position{}, RenderTexture::TextureCoordinates{});
        resources.add<GL::Mesh>("fullscreen_tri", std::move(surfaceMesh));
    }
}

void SysDebugRender::configure_render_passes(ActiveScene& rScene)
{
    /* Configure pipeline resources */
    
    // Add an offscreen framebuffer

    auto& resources = rScene.get_context_resources();

    Vector2i viewSize = GL::defaultFramebuffer.viewport().size();

    GL::Texture2D color;
    color.setStorage(1, GL::TextureFormat::RGB8, viewSize);
    DependRes<GL::Texture2D> colorRes =
        resources.add<GL::Texture2D>("offscreen_fbo_color", std::move(color));

    GL::Renderbuffer depthStencil;
    depthStencil.setStorage(GL::RenderbufferFormat::Depth24Stencil8, viewSize);
    DependRes<GL::Renderbuffer> depthStencilRes =
        resources.add<GL::Renderbuffer>("offscreen_fbo_depthStencil", std::move(depthStencil));

    GL::Framebuffer fbo(Range2Di{{0, 0}, viewSize});
    fbo.attachTexture(GL::Framebuffer::ColorAttachment{0}, *colorRes, 0);
    fbo.attachRenderbuffer(GL::Framebuffer::BufferAttachment::DepthStencil, *depthStencilRes);

    resources.add<GL::Framebuffer>("offscreen_fbo", std::move(fbo));

    /* Define render passes */

    // Opaque pass
    rScene.debug_render_add(rScene.get_render_order(),
        "opaque_pass", "", "transparent_pass",
        [](ActiveScene& rScene, ACompCamera& camera)
        {
            using namespace Magnum;
            using Magnum::GL::Renderer;

            auto& reg = rScene.get_registry();

            camera.m_renderTarget->bind();

            camera.m_renderTarget->clear(
                GL::FramebufferClear::Color
                | GL::FramebufferClear::Depth
                | GL::FramebufferClear::Stencil);

            Renderer::enable(Renderer::Feature::DepthTest);
            Renderer::enable(Renderer::Feature::FaceCulling);
            Renderer::disable(Renderer::Feature::Blending);

            // Fetch opaque objects
            auto opaqueView = reg.view<CompDrawableDebug, ACompTransform>(
                entt::exclude<CompTransparentDebug>);

            SysDebugRender::draw_group(rScene, opaqueView, camera);
        }
    );

    // Transparent pass
    rScene.debug_render_add(rScene.get_render_order(),
        "transparent_pass", "opaque_pass", "display_framebuffer",
        [](osp::active::ActiveScene& rScene, ACompCamera& camera)
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

            auto transparentView = reg.view<CompDrawableDebug, CompVisibleDebug,
                CompTransparentDebug, ACompTransform>();

            // Draw backfaces
            Renderer::setFaceCullingMode(Renderer::PolygonFacing::Front);
            SysDebugRender::draw_group(rScene, transparentView, camera);

            // Draw frontfaces
            Renderer::setFaceCullingMode(Renderer::PolygonFacing::Back);
            SysDebugRender::draw_group(rScene, transparentView, camera);
        }
    );

    // Render offscreen buffer
    rScene.debug_render_add(rScene.get_render_order(),
        "display_framebuffer", "transparent_pass", "",
        [](osp::active::ActiveScene& rScene, ACompCamera& camera)
        {
            using namespace Magnum;
            using Magnum::GL::Renderer;
            using namespace osp::active;
            using namespace osp;

            GL::defaultFramebuffer.bind();
            GL::defaultFramebuffer.clear(
                GL::FramebufferClear::Color
                | GL::FramebufferClear::Depth
                | GL::FramebufferClear::Stencil);

            Renderer::disable(Renderer::Feature::DepthTest);
            Renderer::disable(Renderer::Feature::FaceCulling);
            Renderer::disable(Renderer::Feature::Blending);

            DependRes<GL::Texture2D> colorTex =
                rScene.get_context_resources().get<GL::Texture2D>("offscreen_fbo_color");
            SysDebugRender::display_framebuffer(rScene, *colorTex);
        }
    );
}
