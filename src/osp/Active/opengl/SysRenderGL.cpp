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

using osp::active::SysRenderGL;
using osp::active::RenderGL;

using osp::active::TexGlId;
using osp::active::MeshGlId;

void SysRenderGL::setup_context(RenderGL& rCtxGl)
{
    using namespace Magnum;

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
        rCtxGl.m_fullscreenTri = rCtxGl.m_meshIds.create();

        rCtxGl.m_meshGl.emplace(rCtxGl.m_fullscreenTri)
            .setPrimitive(Magnum::MeshPrimitive::Triangles)
            .setCount(3)
            .addVertexBuffer(std::move(surface), 0,
                FullscreenTriShader::Position{},
                FullscreenTriShader::TextureCoordinates{});
    }

    /* Add an offscreen framebuffer */
    {
        Vector2i const viewSize = GL::defaultFramebuffer.viewport().size();

        rCtxGl.m_fboColor = rCtxGl.m_texIds.create();
        GL::Texture2D &rFboColor = rCtxGl.m_texGl.emplace(rCtxGl.m_fboColor);
        rFboColor.setStorage(1, GL::TextureFormat::RGB8, viewSize);

        rCtxGl.m_fboDepthStencil = Magnum::GL::Renderbuffer{};
        rCtxGl.m_fboDepthStencil.setStorage(GL::RenderbufferFormat::Depth24Stencil8, viewSize);

        rCtxGl.m_fbo = GL::Framebuffer{ Range2Di{{0, 0}, viewSize} };
        rCtxGl.m_fbo.attachTexture(GL::Framebuffer::ColorAttachment{0}, rFboColor, 0);
        rCtxGl.m_fbo.attachRenderbuffer(GL::Framebuffer::BufferAttachment::DepthStencil, rCtxGl.m_fboDepthStencil);
    }
}

MeshGlId try_compile_mesh(
        RenderGL& rRenderGl, DependRes<MeshData> const& meshData)
{
    auto foundIt = rRenderGl.m_oldResToMesh.find(meshData.name());

    if (foundIt == rRenderGl.m_oldResToMesh.end())
    {
        // Mesh isn't compiled yet, compile it
        MeshGlId newId = rRenderGl.m_meshIds.create();
        rRenderGl.m_meshGl.emplace(newId, Magnum::MeshTools::compile(*meshData));
        rRenderGl.m_oldResToMesh.emplace(meshData.name(), newId);
        return newId;
    }

    return foundIt->second;
}

void SysRenderGL::compile_meshes(
        acomp_storage_t<ACompMesh> const& meshes,
        std::vector<ActiveEnt> const& dirty,
        acomp_storage_t<MeshGlId>& rMeshGl,
        RenderGL& rRenderGl)
{
    for (ActiveEnt ent : dirty)
    {
        if (meshes.contains(ent))
        {
            ACompMesh const &rEntMesh = meshes.get(ent);

            if (rMeshGl.contains(ent))
            {
                // Check if ACompMesh changed
                MeshGlId &rEntMeshGl = rMeshGl.get(ent);

                if (rRenderGl.m_oldResToMesh.at(rEntMesh.m_mesh.name()) != rEntMeshGl)
                {
                    // get new mesh
                    rEntMeshGl = try_compile_mesh(rRenderGl, rEntMesh.m_mesh);
                }
            }
            else
            {
                // ACompMeshGL component needed
                rMeshGl.emplace(
                        ent, try_compile_mesh(rRenderGl, rEntMesh.m_mesh));
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

TexGlId try_compile_texture(
        RenderGL& rRenderGl, DependRes<ImageData2D> const& texData)
{
    auto foundIt = rRenderGl.m_oldResToTex.find(texData.name());

    if (foundIt == rRenderGl.m_oldResToTex.end())
    {
        // Texture isn't compiled yet, compile it
        TexGlId newId = rRenderGl.m_texIds.create();

        Texture2D &rTexGl = rRenderGl.m_texGl.emplace(newId);
        
        using Magnum::GL::SamplerWrapping;
        using Magnum::GL::SamplerFilter;
        using Magnum::GL::textureFormat;

        Magnum::ImageView2D view = *texData;

        rTexGl.setWrapping(SamplerWrapping::ClampToEdge)
            .setMagnificationFilter(SamplerFilter::Nearest)
            .setMinificationFilter(SamplerFilter::Nearest)
            .setStorage(1, textureFormat((*texData).format()), (*texData).size())
            .setSubImage(0, {}, view);

        return newId;

    }

    return foundIt->second;
}

void SysRenderGL::compile_textures(
        acomp_storage_t<ACompTexture> const& textures,
        std::vector<ActiveEnt> const& dirty,
        acomp_storage_t<TexGlId>& rTexGl,
        RenderGL& rRenderGl)
{
    for (ActiveEnt ent : dirty)
    {
        if (textures.contains(ent))
        {
            ACompTexture const &rEntTex = textures.get(ent);

            if (rTexGl.contains(ent))
            {
                // Check if ACompTexture changed
                TexGlId &rEntTexGl = rTexGl.get(ent);

                if (rRenderGl.m_oldResToTex.at(rEntTex.m_texture.name()) != rEntTexGl)
                {
                    // get new mesh
                    rEntTexGl = try_compile_texture(rRenderGl, rEntTex.m_texture);
                }
            }
            else
            {
                // ACompMeshGL component needed
                rTexGl.emplace(
                        ent, try_compile_texture(rRenderGl, rEntTex.m_texture));
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
        RenderGL& rRenderGl, Magnum::GL::Texture2D& rTex)
{
    using Magnum::GL::Renderer;
    using Magnum::GL::Framebuffer;
    using Magnum::GL::FramebufferClear;
    using Magnum::GL::Mesh;

    Magnum::GL::defaultFramebuffer.bind();

    Renderer::disable(Renderer::Feature::DepthTest);
    Renderer::disable(Renderer::Feature::FaceCulling);
    Renderer::disable(Renderer::Feature::Blending);
    Renderer::setDepthMask(true);

    rRenderGl.m_fullscreenTriShader.display_texure(
            rRenderGl.m_meshGl.get(rRenderGl.m_fullscreenTri),
            rRenderGl.m_texGl.get(rRenderGl.m_fboColor));
}

void SysRenderGL::render_opaque(
        RenderGroup const& group,
        acomp_storage_t<ACompVisible> const& visible,
        ViewProjMatrix const& viewProj)
{
    using Magnum::GL::Renderer;

    Renderer::enable(Renderer::Feature::DepthTest);
    Renderer::enable(Renderer::Feature::FaceCulling);
    Renderer::disable(Renderer::Feature::Blending);
    Renderer::setDepthMask(true);

    draw_group(group, visible, viewProj);
}

void SysRenderGL::render_transparent(
        RenderGroup const& group,
        acomp_storage_t<ACompVisible> const& visible,
        ViewProjMatrix const& viewProj)
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

    draw_group(group, visible, viewProj);
}

void SysRenderGL::draw_group(
        RenderGroup const& group,
        acomp_storage_t<ACompVisible> const& visible,
        ViewProjMatrix const& viewProj)
{
    for (auto const& [ent, toDraw] : group.view().each())
    {
        if (visible.contains(ent))
        {
            toDraw(ent, viewProj);
        }
    }
}
