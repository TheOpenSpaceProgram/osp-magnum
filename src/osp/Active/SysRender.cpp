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

#include "../Shaders/FullscreenTriShader.h"
#include "ActiveScene.h"

#include <osp/Resource/Package.h>

#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/GL/Renderbuffer.h>
#include <Magnum/GL/RenderbufferFormat.h>

#include <Magnum/Mesh.h>

#include <Corrade/Containers/ArrayViewStl.h>

#include <functional>

using osp::active::SysRender;
using osp::active::RenderPipeline;

void SysRender::setup_context(Package& rCtxResources)
{
    using namespace Magnum;

    rCtxResources.add<FullscreenTriShader>("fullscreen_tri_shader");

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
        rCtxResources.add<GL::Mesh>("fullscreen_tri", std::move(surfaceMesh));
    }

    /* Add an offscreen framebuffer */
    {
        Vector2i viewSize = GL::defaultFramebuffer.viewport().size();

        DependRes<GL::Texture2D> color = rCtxResources.add<GL::Texture2D>("offscreen_fbo_color");
        color->setStorage(1, GL::TextureFormat::RGB8, viewSize);

        DependRes<GL::Renderbuffer> depthStencil =
            rCtxResources.add<GL::Renderbuffer>("offscreen_fbo_depthStencil");
        depthStencil->setStorage(GL::RenderbufferFormat::Depth24Stencil8, viewSize);

        DependRes<GL::Framebuffer> fbo =
            rCtxResources.add<GL::Framebuffer>("offscreen_fbo", Range2Di{{0, 0}, viewSize});
        fbo->attachTexture(GL::Framebuffer::ColorAttachment{0}, *color, 0);
        fbo->attachRenderbuffer(GL::Framebuffer::BufferAttachment::DepthStencil, *depthStencil);
    }
}


void SysRender::setup_forward_renderer(ActiveScene &rScene)
{
    Package &rCtxResources = rScene.get_context_resources();

    Vector2i const viewSize = Magnum::GL::defaultFramebuffer.viewport().size();

    rScene.reg_emplace<ACompRenderTarget>(
            rScene.hier_get_root(),
            viewSize,
            rCtxResources.get<Magnum::GL::Framebuffer>("offscreen_fbo"));
    rScene.reg_emplace<ACompFBOColorAttachment>(
            rScene.hier_get_root(),
            rCtxResources.get<Magnum::GL::Texture2D>("offscreen_fbo_color"));

    // Add render groups
    auto &rGroups = rScene.get_registry().set<ACtxRenderGroups>();
    rGroups.m_groups.emplace("fwd_opaque", RenderGroup{});
    rGroups.m_groups.emplace("fwd_transparent", RenderGroup{});

    // Create the default render pipeline (if not already added yet)
    rCtxResources.add<RenderPipeline>("default", create_forward_pipeline(rScene));
}

RenderPipeline SysRender::create_forward_pipeline(ActiveScene& rScene)
{
    /* Define render passes */
    std::vector<RenderStep_t> pipeline;

    // Opaque pass
    pipeline.emplace_back(
        [](ActiveScene& rScene, ACompCamera const& camera)
        {
            using Magnum::GL::Renderer;

            Renderer::enable(Renderer::Feature::DepthTest);
            Renderer::enable(Renderer::Feature::FaceCulling);
            Renderer::disable(Renderer::Feature::Blending);
            Renderer::setDepthMask(true);

            ActiveReg_t const &reg = rScene.get_registry();
            auto const viewVisible = reg.view<const ACompVisible>();
            auto const &groups = reg.ctx<ACtxRenderGroups>();

            for (auto const& [ent, toDraw]
                 : groups.m_groups.at("fwd_opaque").view().each())
            {
                if (viewVisible.contains(ent))
                {
                    toDraw(ent, rScene, camera);
                }
            }
        }
    );

    // Transparent pass
    pipeline.emplace_back(
        [](osp::active::ActiveScene& rScene, ACompCamera const& camera)
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

            ActiveReg_t const &reg = rScene.get_registry();
            auto const viewVisible = reg.view<const ACompVisible>();
            auto const &groups = reg.ctx<ACtxRenderGroups>();

            for (auto const& [ent, toDraw]
                 : groups.m_groups.at("fwd_transparent").view().each())
            {
                if (viewVisible.contains(ent))
                {
                    toDraw(ent, rScene, camera);
                }
            }
        }
    );

    return {std::move(pipeline)};
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
    Renderer::setDepthMask(true);

    shader->display_texure(*surface, *target.m_tex);
}

void SysRender::update_drawfunc_assign(ActiveScene& rScene)
{
    ActiveReg_t &rReg = rScene.get_registry();
    auto &rGroups = rReg.ctx<ACtxRenderGroups>();

    // TODO: check entities that change their material type by checking
    //       ACompMaterial

    // Loop through each draw group
    for (auto & [name, rGroup] : rGroups.m_groups)
    {
        // Loop through each possibly supported Material ID
        // from 0 torGroup.m_assigner.size()
        for (material_id_t i = 0; i < rGroup.m_assigners.size(); i ++)
        {
            if (rGroups.m_added.at(i).empty())
            {
                continue; // No drawables of this type to assign shaders to
            }

            RenderGroup::DrawAssigner_t const &assigner
                    = rGroup.m_assigners.at(i);

            if ( ! bool(assigner) )
            {
                continue; // Assigner is null, not set to a function
            }

            // Pass all new drawables (rGroups.m_added[i]) to the assigner.
            // This adds accepted entities to rGroup.m_entities
            assigner(rScene, rGroup.m_entities, rGroups.m_added.at(i));
        }
    }

    // Add ACompMaterial components, and clear m_added queues
    for (material_id_t i = 0; i < rGroups.m_added.size(); i ++)
    {
        for ( ActiveEnt ent : std::exchange(rGroups.m_added[i], {}) )
        {
            rScene.reg_emplace<ACompMaterial>(ent, ACompMaterial{i});
        }
    }
}

void SysRender::update_drawfunc_delete(ActiveScene& rScene)
{
    auto &rReg = rScene.get_registry();
    auto &rGroups = rReg.ctx<ACtxRenderGroups>();
    auto viewDelMat = rReg.view<ACompDelete, ACompMaterial>();


    // copy entities to delete into a buffer
    std::vector<ActiveEnt> toDelete;
    toDelete.reserve(viewDelMat.size_hint());

    for (auto const& [ent, mat] : viewDelMat.each())
    {
        toDelete.push_back(ent);
    }

    if (toDelete.empty())
    {
        return;
    }

    // Delete from each draw group
    for ([[maybe_unused]] auto& [name, rGroup] : rGroups.m_groups)
    {
        for (ActiveEnt ent : toDelete)
        {
            if (rGroup.m_entities.contains(ent))
            {
                rGroup.m_entities.remove(ent);
            }
        }
    }

    rReg.remove<ACompMaterial>(std::begin(toDelete), std::end(toDelete));
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
