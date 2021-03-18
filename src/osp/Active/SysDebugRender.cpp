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
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Mesh.h>
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
    Package& glResources = rScene.get_context_resources();

    using namespace adera::shader;

    glResources.add<Phong>("phong_shader",
        Phong{Magnum::Shaders::Phong::Flag::DiffuseTexture});

    glResources.add<PlumeShader>("plume_shader");

    // Generate fullscreen tri for texture rendering
    using namespace Magnum; 
    if (glResources.get<GL::Mesh>("fullscreen_tri").empty())
    {
        std::array<float, 12> surfData
        {
            // Vert position    // UV coordinate
            -1.0f,  1.0f,       0.0f,  1.0f,
            -1.0f, -2.0f,       0.0f, -1.0f,
             2.0f,  1.0f,       2.0f,  1.0f
        };

        GL::Buffer surface(std::move(surfData), GL::BufferUsage::StaticDraw);
        GL::Mesh surfaceMesh;
        surfaceMesh
            .setPrimitive(Magnum::MeshPrimitive::Triangles)
            .addVertexBuffer(std::move(surface), 0,
                RenderTexture::Position{}, RenderTexture::TextureCoordinates{});
        glResources.add<GL::Mesh>("fullscreen_tri", std::move(surfaceMesh));
    }

}

void SysDebugRender::draw(ACompCamera& camera)
{
    GL::AbstractFramebuffer* framebuffer = (camera.m_renderTarget.empty()) ?
        reinterpret_cast<GL::AbstractFramebuffer*>(&GL::defaultFramebuffer)
        : reinterpret_cast<GL::AbstractFramebuffer*>(&(*camera.m_renderTarget));

    framebuffer->bind();

    framebuffer->clear(
        GL::FramebufferClear::Color
        | GL::FramebufferClear::Depth
        | GL::FramebufferClear::Stencil);

    for (auto& pass : m_renderPasses)
    {
        pass(m_scene, camera);
    }
}

void SysDebugRender::render_framebuffer(ActiveScene& rScene, Magnum::GL::Texture2D& rTexture)
{
    using namespace Magnum;
    
    auto& glResources = rScene.get_context_resources();

    DependRes<GL::Mesh> surface = glResources.get<GL::Mesh>("fullscreen_tri");
    DependRes<RenderTexture> shader = glResources.get<RenderTexture>("render_texture");

    shader->render_texure(*surface, rTexture);
}
