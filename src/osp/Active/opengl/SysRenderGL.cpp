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

#include <Magnum/ImageView.h>

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
using Magnum::Trade::ImageData2D;

using Magnum::GL::Mesh;
using Magnum::GL::Texture2D;

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
        acomp_storage_t<ACompMesh> const& meshes,
        std::vector<ActiveEnt>& rDirty,
        acomp_storage_t<ACompMeshGL>& rMeshGl,
        osp::Package& rGlResources)
{
    for (ActiveEnt ent : std::exchange(rDirty, {}))
    {
        if (meshes.contains(ent))
        {
            ACompMesh const &rEntMesh = meshes.get(ent);

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

DependRes<Texture2D> try_compile_texture(
        osp::Package& rGlResources, DependRes<ImageData2D> const& texData)
{
    DependRes<Texture2D> rTexGlRes
            = rGlResources.get_or_reserve<Texture2D>(texData.name());

    if (rTexGlRes.reserve_empty())
    {
        // Texture isn't compiled yet, compile it
        Texture2D &rTexGl = rTexGlRes.reserve_emplace();

        using Magnum::GL::SamplerWrapping;
        using Magnum::GL::SamplerFilter;
        using Magnum::GL::textureFormat;

        Magnum::ImageView2D view = *texData;

        rTexGl.setWrapping(SamplerWrapping::ClampToEdge)
            .setMagnificationFilter(SamplerFilter::Nearest)
            .setMinificationFilter(SamplerFilter::Nearest)
            .setStorage(1, textureFormat((*texData).format()), (*texData).size())
            .setSubImage(0, {}, view);

    }

    return rTexGlRes;
}

void SysRenderGL::compile_textures(
        acomp_storage_t<ACompTexture> const& textures,
        std::vector<ActiveEnt>& rDirty,
        acomp_storage_t<ACompTextureGL>& rTexGl,
        osp::Package& rGlResources)
{
    for (ActiveEnt ent : std::exchange(rDirty, {}))
    {
        if (textures.contains(ent))
        {
            ACompTexture const &rEntTex = textures.get(ent);

            if (rTexGl.contains(ent))
            {
                // Check if ACompTexture changed
                ACompTextureGL &rEntTexGl = rTexGl.get(ent);

                if (rEntTexGl.m_tex.name() != rEntTexGl.m_tex.name())
                {
                    // get new mesh
                    rEntTexGl.m_tex = try_compile_texture(rGlResources, rEntTex.m_texture);
                }
            }
            else
            {
                // ACompMeshGL component needed
                ACompTextureGL &rEntTexGl = rTexGl.emplace(
                        ent, ACompTextureGL{try_compile_texture(rGlResources, rEntTex.m_texture)});
            }
        }
        else
        {
            if (rTexGl.contains(ent))
            {
                // ACompMesh removed, remove ACompMeshGL too
                rTexGl.erase(ent);
            }
            else
            {
                // Why is this entity here?
            }
        }
    }
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

void SysRenderGL::render_opaque(
        RenderGroup const& group,
        acomp_storage_t<ACompVisible> const& visible,
        ACompCamera const& camera)
{
    using Magnum::GL::Renderer;

    Renderer::enable(Renderer::Feature::DepthTest);
    Renderer::enable(Renderer::Feature::FaceCulling);
    Renderer::disable(Renderer::Feature::Blending);
    Renderer::setDepthMask(true);

    draw_group(group, visible, camera);
}

void SysRenderGL::render_transparent(
        RenderGroup const& group,
        acomp_storage_t<ACompVisible> const& visible,
        ACompCamera const& camera)
{
    using Magnum::GL::Renderer;

    Renderer::enable(Renderer::Feature::DepthTest);
    Renderer::disable(Renderer::Feature::FaceCulling);
    Renderer::enable(Renderer::Feature::Blending);
    Renderer::setBlendFunction(
            Renderer::BlendFunction::SourceAlpha,
            Renderer::BlendFunction::OneMinusSourceAlpha);

    // temporary: disabled depth writing makes the plumes look nice, but
    //            can mess up other transparent objects once added
    //Renderer::setDepthMask(false);

    draw_group(group, visible, camera);
}

void SysRenderGL::draw_group(
        RenderGroup const& group,
        acomp_storage_t<ACompVisible> const& visible,
        ACompCamera const& camera)
{
    for (auto const& [ent, toDraw] : group.view().each())
    {
        if (visible.contains(ent))
        {
            toDraw(ent, camera);
        }
    }
}
