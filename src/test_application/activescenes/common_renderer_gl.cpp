/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
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
#include "common_renderer_gl.h"

#include <Magnum/GL/DefaultFramebuffer.h>

#include <Corrade/Containers/ArrayViewStl.h>

using namespace osp::active;

namespace testapp
{

void CommonSceneRendererGL::setup(ActiveApplication& rApp)
{
    using namespace osp::shader;

    // Setup Phong shaders
    auto const texturedFlags
            = Phong::Flag::DiffuseTexture | Phong::Flag::AlphaMask
            | Phong::Flag::AmbientTexture;
    m_phong.m_shaderDiffuse      = Phong{texturedFlags, 2};
    m_phong.m_shaderUntextured   = Phong{{}, 2};
    m_phong.assign_pointers(
            m_renderGl, rApp.get_render_gl());

    // Setup Mesh Visualizer shader
    m_visualizer.m_shader
            = MeshVisualizer{ MeshVisualizer::Flag::Wireframe };
    m_visualizer.assign_pointers(
            m_renderGl, rApp.get_render_gl());

    // Create render group for forward opaque pass
    m_renderGroups.m_groups.emplace("fwd_opaque", RenderGroup{});

}

void CommonSceneRendererGL::sync(ActiveApplication& rApp, CommonTestScene const& rScene)
{
    using namespace osp::shader;

    RenderGroup &rGroupFwdOpaque
            = m_renderGroups.m_groups["fwd_opaque"];

    // Assign Phong shader to entities with the gc_mat_common material, and put
    // results into the fwd_opaque render group
    {
        MaterialData const &rMatCommon = rScene.m_drawing.m_materials[rScene.m_matCommon];
        assign_phong(
                rMatCommon.m_added, &rGroupFwdOpaque.m_entities, nullptr,
                rScene.m_drawing.m_opaque, m_renderGl.m_diffuseTexId,
                m_phong);
        SysRender::assure_draw_transforms(
                    rScene.m_basic.m_hierarchy,
                    m_renderGl.m_drawTransform,
                    std::cbegin(rMatCommon.m_added),
                    std::cend(rMatCommon.m_added));
    }

    // Same thing but with MeshVisualizer and gc_mat_visualizer
    {
        MaterialData const &rMatVisualizer
                = rScene.m_drawing.m_materials[rScene.m_matVisualizer];

        assign_visualizer(
                rMatVisualizer.m_added, rGroupFwdOpaque.m_entities,
                m_visualizer);
        SysRender::assure_draw_transforms(
                    rScene.m_basic.m_hierarchy,
                    m_renderGl.m_drawTransform,
                    std::cbegin(rMatVisualizer.m_added),
                    std::cend(rMatVisualizer.m_added));
    }

    // Load required meshes and textures into OpenGL
    SysRenderGL::sync_scene_resources(rScene.m_drawingRes, *rScene.m_pResources, rApp.get_render_gl());

    // Assign GL meshes to entities with a mesh component
    SysRenderGL::assign_meshes(
            rScene.m_drawing.m_mesh, rScene.m_drawingRes.m_meshToRes, rScene.m_drawing.m_meshDirty,
            m_renderGl.m_meshId, rApp.get_render_gl());

    // Assign GL textures to entities with a texture component
    SysRenderGL::assign_textures(
            rScene.m_drawing.m_diffuseTex, rScene.m_drawingRes.m_texToRes, rScene.m_drawing.m_diffuseDirty,
            m_renderGl.m_diffuseTexId, rApp.get_render_gl());

    // Calculate hierarchy transforms
    SysRender::update_draw_transforms(
            rScene.m_basic.m_hierarchy,
            rScene.m_basic.m_transform,
            m_renderGl.m_drawTransform);
}

void CommonSceneRendererGL::render(ActiveApplication& rApp, CommonTestScene const& rScene)
{
    using Magnum::GL::Framebuffer;
    using Magnum::GL::FramebufferClear;
    using Magnum::GL::Texture2D;

    // Get camera to calculate view and projection matrix
    ACompCamera const &rCamera = rScene.m_basic.m_camera.get(m_camera);
    ACompDrawTransform const &cameraDrawTf
            = m_renderGl.m_drawTransform.get(m_camera);
    ViewProjMatrix viewProj{
            cameraDrawTf.m_transformWorld.inverted(),
            rCamera.calculate_projection()};

    // Bind offscreen FBO
    Framebuffer &rFbo = rApp.get_render_gl().m_fbo;
    rFbo.bind();

    // Clear it
    rFbo.clear( FramebufferClear::Color | FramebufferClear::Depth
                | FramebufferClear::Stencil);

    // Forward Render fwd_opaque group to FBO
    SysRenderGL::render_opaque(
            m_renderGroups.m_groups.at("fwd_opaque"),
            rScene.m_drawing.m_visible, viewProj);

    // Display FBO
    Texture2D &rFboColor = rApp.get_render_gl().m_texGl.get(rApp.get_render_gl().m_fboColor);
    SysRenderGL::display_texture(rApp.get_render_gl(), rFboColor);
}

void CommonSceneRendererGL::update_delete(std::vector<ActiveEnt> const& toDelete)
{
    auto first = std::cbegin(toDelete);
    auto last = std::cend(toDelete);
    SysRender::update_delete_groups(m_renderGroups, first, last);
    SysRenderGL::update_delete(m_renderGl, first, last);
}

on_draw_t generate_common_draw(CommonTestScene& rScene, ActiveApplication& rApp, setup_renderer_t setup_scene)
{
    auto pRenderer = std::make_shared<CommonSceneRendererGL>();

    // Setup default resources
    pRenderer->setup(rApp);

    // Setup scene-specifc stuff
    setup_scene(*pRenderer, rScene, rApp);

    // Set all drawing stuff dirty then sync with renderer.
    // This allows clean re-openning of the scene
    SysRender::set_dirty_all(rScene.m_drawing);
    pRenderer->sync(rApp, rScene);

    return [&rScene, pRenderer = std::move(pRenderer)] (ActiveApplication& rApp, float delta)
    {
        pRenderer->m_onCustomDraw(*pRenderer, rScene, rApp, delta);

        // Delete components of deleted entities on renderer's side
        pRenderer->update_delete(rScene.m_deleteTotal);

        pRenderer->sync(rApp, rScene);

        // Render to screen
        pRenderer->render(rApp, rScene);
    };
};

} // namespace testapp
