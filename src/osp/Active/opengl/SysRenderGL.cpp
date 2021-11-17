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

#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/ImageData.h>

#include <Magnum/Mesh.h>
#include <Magnum/MeshTools/Compile.h>

using Magnum::Trade::MeshData;
using Magnum::GL::Mesh;
using osp::DependRes;
using osp::Package;
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

DependRes<Mesh> try_compile_mesh(
        osp::Package& rGlResources, DependRes<MeshData> const& meshData)
{
    DependRes<Mesh> rMeshGlRes = rGlResources.get_or_reserve<Mesh>(meshData.name());

    if (rMeshGlRes.reserve_empty())
    {
        // Mesh isn't compiled yet, compile it
        rMeshGlRes.reserve_emplace(Magnum::MeshTools::compile(*meshData));
    }

    return rMeshGlRes;
}

void SysRenderGL::compile_meshes(
        acomp_view_t<ACompMesh const> viewMeshs,
        std::vector<ActiveEnt>& rDirty,
        acomp_storage_t<ACompMeshGL>& rMeshGl,
        osp::Package& rGlResources)
{

    for (ActiveEnt ent : std::exchange(rDirty, {}))
    {
        if (viewMeshs.contains(ent))
        {
            auto const &rEntMesh = viewMeshs.get<ACompMesh const>(ent);

            if (rMeshGl.contains(ent))
            {
                // Check if ACompMesh changed
                ACompMeshGL &rEntMeshGl = rMeshGl.get(ent);

                if (rEntMeshGl.m_mesh.name() != rEntMesh.m_mesh.name())
                {
                    // get new mesh
                    rEntMeshGl.m_mesh = try_compile_mesh(rGlResources, rEntMesh.m_mesh);
                }
            }
            else
            {
                // ACompMeshGL component needed
                rMeshGl.emplace(
                        ent, ACompMeshGL{try_compile_mesh(rGlResources, rEntMesh.m_mesh)});
            }
        }
        else
        {
            if (rMeshGl.contains(ent))
            {
                // ACompMesh removed, remove ACompMeshGL too
                rMeshGl.erase(ent);
            }
            else
            {
                // Why is this entity here?
            }
        }
    }
}

void SysRenderGL::compile_textures(
        acomp_view_t<const ACompTexture> viewDiffTex,
        std::vector<ActiveEnt>& rDirty,
        acomp_storage_t<ACompTextureGL>& rDiffTexGl,
        osp::Package& rGlResources)
{
    rDirty.clear();
}

void SysRenderGL::display_texture(
        Package& rGlResources, Magnum::GL::Texture2D& rTex)
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

    shader->display_texure(*surface, rTex);
}

// TODO: problem got simpler, maybe generalize these two somehow

void SysRenderGL::render_opaque(
        RenderGroup const& rGroup,
        acomp_view_t<const ACompVisible> viewVisible,
        ACompCamera const& camera)
{
    using Magnum::GL::Renderer;

    Renderer::enable(Renderer::Feature::DepthTest);
    Renderer::enable(Renderer::Feature::FaceCulling);
    Renderer::disable(Renderer::Feature::Blending);
    Renderer::setDepthMask(true);

    for (auto const& [ent, toDraw] : rGroup.view().each())
    {
        if (viewVisible.contains(ent))
        {
            toDraw(ent, camera);
        }
    }
}

void SysRenderGL::render_transparent(
        RenderGroup const& group,
        acomp_view_t<const ACompVisible> viewVisible,
        ACompCamera const& camera)
{
    using Magnum::GL::Renderer;

    Renderer::enable(Renderer::Feature::DepthTest);
    Renderer::enable(Renderer::Feature::FaceCulling);
    Renderer::enable(Renderer::Feature::Blending);
    Renderer::setBlendFunction(
            Renderer::BlendFunction::SourceAlpha,
            Renderer::BlendFunction::OneMinusSourceAlpha);

    // temporary: disabled depth writing makes the plumes look nice, but
    //            can mess up other transparent objects once added
    Renderer::setDepthMask(false);

    for (auto const& [ent, toDraw] : group.view().each())
    {
        if (viewVisible.contains(ent))
        {
            toDraw(ent, camera);
        }
    }
}
