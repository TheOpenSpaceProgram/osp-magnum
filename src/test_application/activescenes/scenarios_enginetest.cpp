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

// Materials used by the test scene. A more general application may want to
// generate IDs at runtime, and map them to named identifiers.
constexpr int const gc_mat_common      = 0;
constexpr int const gc_mat_visualizer  = 1;

constexpr int const gc_maxMaterials = 2;

/**
 * @brief State of the entire engine test scene
 */
struct EngineTestScene
{
    // ID registry generates entity IDs, and keeps track of which ones exist
    osp::IdRegistry<osp::active::ActiveEnt> m_activeIds;

    // Components and supporting data structures
    osp::active::ACtxBasic          m_basic;    
    osp::active::ACtxDrawing        m_drawing;

    // Hierarchy root, needs to exist so all hierarchy entities are connected
    osp::active::ActiveEnt          m_hierRoot;

    // The rotating cube
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
    rScene.m_hierRoot = rScene.m_activeIds.create();
    rScene.m_basic.m_hierarchy.emplace(rScene.m_hierRoot);

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
    SysHierarchy::add_child(
            rScene.m_basic.m_hierarchy, rScene.m_hierRoot, camEnt);

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
    SysHierarchy::add_child(
            rScene.m_basic.m_hierarchy, rScene.m_hierRoot, rScene.m_cube);

    return std::move(sceneAny);
}

/**
 * @brief Update an EngineTestScene, this just rotates the cube
 *
 * @param rScene [ref] scene to update
 */
void update_test_scene(EngineTestScene& rScene)
{
    // Rotate the cube
    osp::Matrix4 &rCubeTf
            = rScene.m_basic.m_transform.get(rScene.m_cube).m_transform;

    rCubeTf = Magnum::Matrix4::rotationY(360.0_degf / 60.0f) * rCubeTf;
}

//-----------------------------------------------------------------------------

/**
 * @brief Data needed to render the EngineTestScene
 */
struct EngineTestRenderer
{
    osp::active::ACtxRenderGroups m_renderGroups;

    osp::active::ACtxRenderGL m_renderGl;

    osp::active::ActiveEnt m_camera;

    osp::shader::ACtxPhongData m_phong;
};

/**
 * @brief Render an EngineTestScene
 *
 * @param rApp      [ref] Application with GL context and resources
 * @param rScene    [ref] Test scene to render
 * @param rRenderer [ref] Renderer data for test scene
 */
void render_test_scene(
        ActiveApplication& rApp, EngineTestScene& rScene,
        EngineTestRenderer& rRenderer)
{
    using namespace osp::active;
    using namespace osp::shader;
    using Magnum::GL::Framebuffer;
    using Magnum::GL::FramebufferClear;
    using Magnum::GL::Texture2D;

    osp::Package &rGlResources = rApp.get_gl_resources();

    // Assign Phong shader to entities with the gc_mat_common material, and put
    // results into the fwd_opaque render group
    RenderGroup &rGroupFwdOpaque
            = rRenderer.m_renderGroups.m_groups["fwd_opaque"];
    MaterialData &rMatCommon = rScene.m_drawing.m_materials[gc_mat_common];
    Phong::assign_phong_opaque(
            rMatCommon.m_added, rGroupFwdOpaque.m_entities,
            rScene.m_drawing.m_opaque, rRenderer.m_renderGl.m_diffuseTexGl,
            rRenderer.m_phong);
    rMatCommon.m_added.clear();

    // Load any required meshes
    SysRenderGL::compile_meshes(
            rScene.m_drawing.m_mesh, rScene.m_drawing.m_meshDirty,
            rRenderer.m_renderGl.m_meshGl, rApp.get_gl_resources());

    // Load any required textures
    SysRenderGL::compile_textures(
            rScene.m_drawing.m_diffuseTex, rScene.m_drawing.m_diffuseDirty,
            rRenderer.m_renderGl.m_diffuseTexGl, rApp.get_gl_resources());

    // Calculate hierarchy transforms
    SysHierarchy::sort(rScene.m_basic.m_hierarchy);
    SysRender::update_draw_transforms(
            rScene.m_basic.m_hierarchy,
            rScene.m_basic.m_transform,
            rScene.m_drawing.m_drawTransform);

    // Get camera, and calculate projection matrix and inverse transformation
    ACompCamera &rCamera = rScene.m_basic.m_camera.get(rRenderer.m_camera);
    ACompDrawTransform const &cameraDrawTf
            = rScene.m_drawing.m_drawTransform.get(rRenderer.m_camera);
    rCamera.m_viewport
            = osp::Vector2(Magnum::GL::defaultFramebuffer.viewport().size());
    rCamera.calculate_projection();
    rCamera.m_inverse = cameraDrawTf.m_transformWorld.inverted();

    // Bind offscreen FBO
    Framebuffer &rFbo = *rGlResources.get<Framebuffer>("offscreen_fbo");
    rFbo.bind();

    // Clear it
    rFbo.clear( FramebufferClear::Color | FramebufferClear::Depth
                | FramebufferClear::Stencil);

    // Forward Render fwd_opaque group to FBO
    SysRenderGL::render_opaque(
            rRenderer.m_renderGroups.m_groups.at("fwd_opaque"),
            rScene.m_drawing.m_visible, rCamera);

    // Display FBO
    Texture2D &rFboColor = *rGlResources.get<Texture2D>("offscreen_fbo_color");
    SysRenderGL::display_texture(rGlResources, rFboColor);
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

on_draw_t gen_draw(EngineTestScene& rScene, ActiveApplication& rApp)
{
    using namespace osp::active;
    using namespace osp::shader;

    // Create renderer data. This uses a shared_ptr to allow being stored
    // inside an std::function, which require copyable types
    std::shared_ptr<EngineTestRenderer> pRenderer
            = std::make_shared<EngineTestRenderer>();

    osp::Package &rGlResources = rApp.get_gl_resources();

    // Get or reserve Phong shaders. These are loaded in load_gl_resources,
    // which can be called before or after this function
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

    // Create render group for forward opaque pass
    pRenderer->m_renderGroups.m_groups.emplace("fwd_opaque", RenderGroup{});

    // Set all materials dirty
    for (MaterialData &rMat : rScene.m_drawing.m_materials)
    {
        rMat.m_added.assign(std::begin(rMat.m_comp), std::end(rMat.m_comp));
    }

    // Set all meshs dirty
    auto &rMeshSet = static_cast<active_sparse_set_t&>(rScene.m_drawing.m_mesh);
    rScene.m_drawing.m_meshDirty.assign(std::begin(rMeshSet), std::end(rMeshSet));

    // Set all textures dirty
    auto &rDiffSet = static_cast<active_sparse_set_t&>(rScene.m_drawing.m_diffuseTex);
    rScene.m_drawing.m_diffuseDirty.assign(std::begin(rMeshSet), std::end(rMeshSet));

    return [&rScene, pRenderer = std::move(pRenderer)] (ActiveApplication& rApp)
    {
        update_test_scene(rScene);
        render_test_scene(rApp, rScene, *pRenderer);
    };
}

} // namespace testapp::enginetest
