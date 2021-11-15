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
#include "scenarios.h"

#include "../ActiveApplication.h"

#include <osp/Active/basic.h>
#include <osp/Active/physics.h>
#include <osp/Active/drawing.h>

#include <osp/Active/SysHierarchy.h>

#include <osp/Active/SysRender.h>
#include <osp/Active/opengl/SysRenderGL.h>

#include <osp/Shaders/Phong.h>
#include <osp/Shaders/MeshVisualizer.h>

#include <osp/id_registry.h>
#include <osp/Resource/Package.h>

#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/ImageData.h>

#include <Magnum/GL/DefaultFramebuffer.h>

#include <Corrade/Containers/ArrayViewStl.h>

using Magnum::Trade::MeshData;
using Magnum::Trade::ImageData2D;

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

namespace testapp::enginetest
{

constexpr int const gc_maxMaterials = 4;

constexpr int const gc_mat_common      = 0;
constexpr int const gc_mat_visualizer  = 1;

struct EngineTestScene
{
    osp::IdRegistry<osp::active::ActiveEnt> m_activeIds;

    osp::active::ACtxBasic          m_basic;
    osp::active::ACtxDrawing        m_drawing;
    osp::active::ACtxPhysics        m_physics;

    osp::active::ActiveEnt          m_rootEntity;
    osp::active::ActiveEnt          m_cube;
};

entt::any setup_scene(osp::Package &rPkg)
{
    using namespace osp::active;

    entt::any sceneAny = entt::make_any<EngineTestScene>();
    EngineTestScene &rScene = entt::any_cast<EngineTestScene&>(sceneAny);

    // Allocate space to fit all materials
    rScene.m_drawing.m_materials.resize(gc_maxMaterials);

    // Create hierarchy root entity
    rScene.m_rootEntity = rScene.m_activeIds.create();
    rScene.m_basic.m_hierarchy.emplace(rScene.m_rootEntity);

    // Create camera entity
    ActiveEnt camEnt = rScene.m_activeIds.create();

    // Create camera transform and draw transform
    ACompTransform &rCamTf = rScene.m_basic.m_transform.emplace(camEnt);
    rCamTf.m_transform.translation().z() = 25;
    rScene.m_drawing.m_drawTransform.emplace(camEnt);

    // Create camera component
    ACompCamera &rCamComp = rScene.m_basic.m_camera.emplace(camEnt);
    rCamComp.m_far = 1u << 24;
    rCamComp.m_near = 1.0f;
    rCamComp.m_fov = 45.0_degf;

    // Add camera to hierarchy
    SysHierarchy::add_parent_child(
            rScene.m_basic.m_hierarchy, rScene.m_basic.m_name,
            rScene.m_rootEntity, camEnt, "Camera");

    // Make a cube
    rScene.m_cube = rScene.m_activeIds.create();

    // Add cube mesh to cube
    rScene.m_drawing.m_mesh.emplace(
            rScene.m_cube, ACompMesh{ rPkg.get<MeshData>("cube") });
    rScene.m_drawing.m_meshDirty.push_back(rScene.m_cube);

    // Add common material to cube
    MaterialData &rMatCommon = rScene.m_drawing.m_materials[gc_mat_common];
    rMatCommon.m_comp.emplace(rScene.m_cube);
    rMatCommon.m_added.push_back(rScene.m_cube);

    // Add transform and draw transform
    rScene.m_basic.m_transform.emplace(rScene.m_cube);
    rScene.m_drawing.m_drawTransform.emplace(rScene.m_cube);

    // Add opaque and visible component
    rScene.m_drawing.m_opaque.emplace(rScene.m_cube);
    rScene.m_drawing.m_visible.emplace(rScene.m_cube);

    // Add cube to hierarchy, parented to root
    SysHierarchy::add_parent_child(
            rScene.m_basic.m_hierarchy, rScene.m_basic.m_name,
            rScene.m_rootEntity, rScene.m_cube, "Cube");

    return std::move(sceneAny);
}

struct EngineTestRenderer
{
    osp::active::ACtxRenderGroups m_renderGroups;

    osp::active::ACtxRenderGL m_renderGl;

    osp::active::ActiveEnt m_camera;

    osp::shader::ACtxPhongData m_phong;
};

void render_test_scene(
        ActiveApplication& rApp, EngineTestScene& rScene,
        EngineTestRenderer& rRenderer)
{
    using namespace osp::active;
    using Magnum::GL::Framebuffer;
    using Magnum::GL::FramebufferClear;
    using Magnum::GL::Texture2D;

    // Calculate hierarchy transforms

    SysHierarchy::sort(rScene.m_basic.m_hierarchy);
    SysRender::update_hierarchy_transforms(
            rScene.m_basic.m_hierarchy,
            rScene.m_basic.m_transform,
            rScene.m_drawing.m_drawTransform);

    // Get camera

    ACompCamera &rCamera = rScene.m_basic.m_camera.get(rRenderer.m_camera);
    rCamera.m_viewport
            = osp::Vector2(Magnum::GL::defaultFramebuffer.viewport().size());
    rCamera.calculate_projection();

    ACompDrawTransform const &cameraDrawTf
            = rScene.m_drawing.m_drawTransform.get(rRenderer.m_camera);
    rCamera.m_inverse = cameraDrawTf.m_transformWorld.inverted();

    // Bind offscreen FBO

    osp::Package &rGlResources = rApp.get_gl_resources();
    Framebuffer &rFbo = *rGlResources.get<Framebuffer>("offscreen_fbo");
    Texture2D &rFboColor = *rGlResources.get<Texture2D>("offscreen_fbo_color");

    rFbo.bind();
    rFbo.clear( FramebufferClear::Color | FramebufferClear::Depth
                | FramebufferClear::Stencil);

    // Render scene to FBO

    SysRenderGL::render_opaque(
            rRenderer.m_renderGroups, rScene.m_drawing.m_visible, rCamera);
    SysRenderGL::render_transparent(
            rRenderer.m_renderGroups, rScene.m_drawing.m_visible, rCamera);

    // Display FBO

    SysRenderGL::display_rendertarget(rGlResources, rFboColor);
}



on_draw_t gen_draw(EngineTestScene& rScene, ActiveApplication& rApp)
{
    using namespace osp::active;
    using namespace osp::shader;

    std::shared_ptr<EngineTestRenderer> pRenderer
            = std::make_shared<EngineTestRenderer>();

    osp::Package &rGlResources = rApp.get_gl_resources();

    pRenderer->m_phong.m_shaderUntextured
            = rGlResources.get_or_reserve<Phong>("notexture");
    pRenderer->m_phong.m_shaderDiffuse
            = rGlResources.get_or_reserve<Phong>("textured");

    pRenderer->m_phong.m_views.emplace(ACtxPhongData::Views{
            rScene.m_drawing.m_drawTransform,
            pRenderer->m_renderGl.m_diffuseTexGl,
            pRenderer->m_renderGl.m_meshGl});

    // Select first camera for rendering
    pRenderer->m_camera = rScene.m_basic.m_camera.at(0);

    SysRenderGL::setup_forward_renderer(pRenderer->m_renderGroups);

    return [&rScene, pRenderer = std::move(pRenderer)] (ActiveApplication& rApp)
    {
        // Rotate the cube
        osp::Matrix4 &rCubeTf = rScene.m_basic.m_transform.get(rScene.m_cube).m_transform;
        rCubeTf = Magnum::Matrix4::rotationY(360.0_degf / 60.0f) * rCubeTf;


        RenderGroup &rGroupFwdOpaque = pRenderer->m_renderGroups.m_groups["fwd_opaque"];
        MaterialData &rMatCommon = rScene.m_drawing.m_materials[gc_mat_common];
        Phong::assign_phong_opaque(
                rMatCommon.m_added, rGroupFwdOpaque.m_entities,
                rScene.m_drawing.m_opaque, pRenderer->m_renderGl.m_diffuseTexGl,
                pRenderer->m_phong);
        rMatCommon.m_added.clear();

        SysRenderGL::load_meshes(
                rScene.m_drawing.m_mesh, rScene.m_drawing.m_meshDirty,
                pRenderer->m_renderGl.m_meshGl, rApp.get_gl_resources());
        SysRenderGL::load_textures(
                rScene.m_drawing.m_diffuseTex, rScene.m_drawing.m_diffuseDirty,
                pRenderer->m_renderGl.m_diffuseTexGl, rApp.get_gl_resources());

        render_test_scene(rApp, rScene, *pRenderer);
    };
}

void load_gl_resources(ActiveApplication& rApp)
{
    using osp::shader::Phong;
    using osp::shader::MeshVisualizer;

    osp::Package &rGlResources = rApp.get_gl_resources();

    rGlResources.add<Phong>("textured", Phong{Phong::Flag::DiffuseTexture});
    rGlResources.add<Phong>("notexture", Phong{});

    rGlResources.add<MeshVisualizer>(
            "mesh_vis_shader",
            MeshVisualizer{ MeshVisualizer::Flag::Wireframe
                            | MeshVisualizer::Flag::NormalDirection});

}

} // namespace testapp::enginetest
