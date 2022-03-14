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

#include "../../Resource/resources.h"

#include "../../logging.h"

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

using osp::ResId;

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

void SysRenderGL::sync_scene_resources(
        const ACtxDrawingRes &rCtxDrawRes,
        Resources &rResources,
        RenderGL &rRenderGl)
{
    // TODO: Eventually have dirty flags instead of checking every entry.

    // Compile required texture resources
    for (auto const & [_, scnOwner] : rCtxDrawRes.m_texToRes)
    {
        ResId const texRes = scnOwner.value();

        // New element will be emplaced if it isn't present yet
        auto const [it, success] = rRenderGl.m_resToTex.try_emplace(texRes);
        if ( ! success)
        {
            return;
        }

        // New element emplaced, this means we've just found a resource that
        // isn't synchronized yet.

        // TODO!!!!
    }

    // Compile required mesh resources
    for (auto const & [_, scnOwner] : rCtxDrawRes.m_meshToRes)
    {
        ResId const meshRes = scnOwner.value();

        // New element will be emplaced if it isn't present yet
        auto const [it, success] = rRenderGl.m_resToMesh.try_emplace(meshRes);
        if ( ! success)
        {
            return;
        }

        // New element emplaced, this means we've just found a resource that
        // isn't synchronized yet.

        // Create new Mesh GL Id
        MeshGlId const newId = rRenderGl.m_meshIds.create();

        // Create owner, this adds to the resource's reference count
        ResIdOwner_t renderOwner
                = rResources.owner_create(restypes::gc_mesh, meshRes);

        // Track with two-way map and store owner
        rRenderGl.m_meshToRes.emplace(newId, std::move(renderOwner));
        it->second = newId;

        // Get mesh data
        MeshData const &meshData = rResources.data_get<MeshData>(restypes::gc_mesh, meshRes);

        // Compile and store mesh
        rRenderGl.m_meshGl.emplace(newId, Magnum::MeshTools::compile(meshData));
    }
}

void SysRenderGL::assign_meshes(
        acomp_storage_t<MeshIdOwner_t> const& cmpMeshIds,
        IdMap_t<MeshId, ResIdOwner_t> const& meshToRes,
        std::vector<ActiveEnt> const& entsDirty,
        acomp_storage_t<ACompMeshGl>& rCmpMeshGl,
        RenderGL& rRenderGl)
{
    for (ActiveEnt ent : entsDirty)
    {
        // Make sure dirty entity has a MeshId component
        if (cmpMeshIds.contains(ent))
        {
            MeshId const entMeshScnId = cmpMeshIds.get(ent);

            ACompMeshGl &rEntMeshGl = rCmpMeshGl.contains(ent)
                                    ? rCmpMeshGl.get(ent)
                                    : rCmpMeshGl.emplace(ent);

            // Check if scene mesh ID is properly synchronized
            if (rEntMeshGl.m_scnId == entMeshScnId)
            {
                continue; // No changes needed
            }

            rEntMeshGl.m_scnId = entMeshScnId;

            // Check if MeshId is associated with a resource
            if (auto found = meshToRes.find(entMeshScnId);
                found != meshToRes.end())
            {
                ResId const meshResId = found->second;

                // Mesh should have been loaded beforehand, assign it!
                rEntMeshGl.m_glId = rRenderGl.m_resToMesh.at(meshResId);
            }
            else
            {
                OSP_LOG_WARN("No mesh data found for Mesh {} from Entity {}",
                             std::size_t(entMeshScnId), std::size_t(ent));
            }
        }
        else
        {
            if (rCmpMeshGl.contains(ent))
            {
                // ACompMesh removed, remove ACompMeshGL too
                rCmpMeshGl.erase(ent);
            }
            else
            {
                // Why is this entity here?
            }
        }
    }
}

void SysRenderGL::assign_textures(
        acomp_storage_t<TexIdOwner_t> const& cmpTexIds,
        IdMap_t<TexId, ResIdOwner_t> const& texToRes,
        std::vector<ActiveEnt> const& entsDirty,
        acomp_storage_t<ACompTexGl>& rCmpTexGl,
        RenderGL& rRenderGl)
{
    // TODO
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

void SysRenderGL::clear_resource_owners(RenderGL& rRenderGl, Resources& rResources)
{
    for (auto & [_, rOwner] : std::exchange(rRenderGl.m_texToRes, {}))
    {
        rResources.owner_destroy(restypes::gc_texture, std::move(rOwner));
    }
    rRenderGl.m_resToTex.clear();

    for (auto & [_, rOwner] : std::exchange(rRenderGl.m_meshToRes, {}))
    {
        rResources.owner_destroy(restypes::gc_mesh, std::move(rOwner));
    }
    rRenderGl.m_resToMesh.clear();
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
