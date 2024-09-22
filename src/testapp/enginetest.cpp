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
#include "MagnumWindowApp.h"

#include <osp/activescene/basic.h>
#include <osp/activescene/basic_fn.h>
#include <osp/core/Resources.h>
#include <osp/drawing/drawing.h>
#include <osp/drawing/own_restypes.h>
#include <osp_drawing_gl/rendergl.h>

#include <adera/drawing/CameraController.h>
#include <adera_drawing_gl/phong_shader.h>

#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/ImageData.h>

#include <Magnum/GL/DefaultFramebuffer.h>

#include <Corrade/Containers/ArrayViewStl.h>

using Magnum::Trade::ImageData2D;
using Magnum::Trade::MeshData;
using adera::ACtxCameraController;
using adera::SysCameraController;
using osp::active::ActiveEnt;
using osp::draw::DrawEnt;
using osp::draw::RenderGL;
using osp::input::UserInputHandler;

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

namespace testapp::enginetest
{

/**
 * @brief State of the entire engine test scene all in one struct
 *
 * This is a simplified example of how OSP scenes are organized.
 * Other test scenes use 'TopData' (aka: std::vector<entt::any>) instead of a
 * big struct.
 */
struct EngineTestScene
{
    ~EngineTestScene();

    // Global Resources, owned by the top-level application
    // Note that multiple scenes are intended to be supported
    osp::Resources                  *m_pResources;

    // Tracks used/free unique 'Active Entity' IDs, starts from zero and counts up
    lgrn::IdRegistryStl<ActiveEnt>  m_activeIds;

    // Supports transforms, hierarchy, cameras, and other components assignable
    // to ActiveEnts
    osp::active::ACtxBasic          m_basic;

    // The rotating cube
    ActiveEnt                       m_cube{lgrn::id_null<ActiveEnt>()};


    // Everything below is for rendering

    // Support for meshes and textures. This is intended to be shared across multiple scenes, but
    // there is only one scene.
    osp::draw::ACtxDrawing          m_drawing;

    // Support for associating scene-space meshes/textures with Resources
    // Meshes/textures can span 3 different spaces, with their own ID types:
    // * Resources  (ResId)            Loaded data, from files or generated
    // * Renderer   (MeshGlId/TexGlId) Shared between scenes, used by GPU
    // * Scene      (MeshId/TexId)     Local to one scene
    // ACtxDrawingRes is a two-way mapping between MeshId/TexId <--> ResId
    osp::draw::ACtxDrawingRes       m_drawingRes;

    // Set of DrawEnts that are assigned a Phong material
    osp::draw::DrawEntSet_t         m_matPhong;
    std::vector<osp::draw::DrawEnt> m_matPhongDirty;

    osp::draw::ACtxSceneRender      m_scnRdr;
};

EngineTestScene::~EngineTestScene()
{
    // A bit of manual cleanup is needed on destruction (for good reason)

    // 'lgrn::IdOwner's cleared here are reference-counted integer IDs.
    // Unlike typical RAII types like std::shared_ptr, IdOwners don't store
    // an internal pointer to their reference count, and are simply just a
    // single integer internally.
    // Cleanup must be manual, but this has the advantage of having no side
    // effects, and practically zero runtime overhead.
    osp::draw::SysRender::clear_owners(m_scnRdr, m_drawing);
    osp::draw::SysRender::clear_resource_owners(m_drawingRes, *m_pResources);
}

entt::any make_scene(osp::Resources& rResources, osp::PkgId const pkg)
{
    using namespace osp::active;
    using namespace osp::draw;

    entt::any sceneAny = entt::make_any<EngineTestScene>();
    auto &rScene = entt::any_cast<EngineTestScene&>(sceneAny);

    rScene.m_pResources = &rResources;

    // Make a cube
    ActiveEnt const cubeEnt = rScene.m_activeIds.create();
    DrawEnt const   cubeDraw = rScene.m_scnRdr.m_drawIds.create();

    // Resize some containers to fit all existing entities
    std::size_t const maxEnts = rScene.m_activeIds.vec().capacity();
    rScene.m_matPhong.resize(maxEnts);
    rScene.m_basic.m_scnGraph.resize(maxEnts);
    rScene.m_scnRdr.resize_active(maxEnts);
    rScene.m_scnRdr.resize_draw();

    // Take ownership of the cube mesh Resource. This will create a scene-space
    // MeshId that we can assign to ActiveEnts
    osp::ResId const resCube = rResources.find(osp::restypes::gc_mesh, pkg, "cube");
    assert(resCube != lgrn::id_null<osp::ResId>());
    MeshId const meshCube = SysRender::own_mesh_resource(rScene.m_drawing, rScene.m_drawingRes, rResources, resCube);

    // Add cube mesh to cube

    rScene.m_scnRdr.m_needDrawTf.insert(cubeEnt);
    rScene.m_scnRdr.m_activeToDraw[cubeEnt] = cubeDraw;
    rScene.m_scnRdr.m_mesh[cubeDraw] = rScene.m_drawing.m_meshRefCounts.ref_add(meshCube);
    rScene.m_scnRdr.m_meshDirty.push_back(cubeDraw);

    // Add transform
    rScene.m_basic.m_transform.emplace(cubeEnt);

    // Add phong material to cube
    rScene.m_matPhong.insert(cubeDraw);
    rScene.m_matPhongDirty.push_back(cubeDraw);

    // Add drawble, opaque, and visible component
    rScene.m_scnRdr.m_visible.insert(cubeDraw);
    rScene.m_scnRdr.m_opaque.insert(cubeDraw);

    // Add cube to hierarchy, parented to root
    SubtreeBuilder builder = SysSceneGraph::add_descendants(rScene.m_basic.m_scnGraph, 1);
    builder.add_child(cubeEnt);

    rScene.m_cube = cubeEnt;

    return sceneAny;
}

/**
 * @brief Update an EngineTestScene, this just rotates the cube
 *
 * @param rScene [ref] scene to update
 */
void update_test_scene(EngineTestScene& rScene, float const delta)
{
    // Clear drawing-related dirty flags/vectors
    rScene.m_scnRdr.m_meshDirty.clear();
    rScene.m_scnRdr.m_diffuseDirty.clear();
    rScene.m_matPhongDirty.clear();

    // Rotate the cube
    osp::Matrix4 &rCubeTf = rScene.m_basic.m_transform.get(rScene.m_cube).m_transform;

    rCubeTf = Magnum::Matrix4::rotationZ(90.0_degf * delta) * rCubeTf;
}

//-----------------------------------------------------------------------------

// Everything below is for rendering

/**
 * @brief Data needed to render the EngineTestScene
 *
 * This will only exist when the window is open, and will be destructed when it
 * closes.
 */
struct EngineTestRenderer
{
    EngineTestRenderer(UserInputHandler &rInputs)
     : m_camCtrl(rInputs)
    { }

    // Support for assigning render-space GL meshes/textures and transforms
    // for ActiveEnts
    osp::draw::ACtxSceneRenderGL m_sceneRenderGL{};

    // Pre-built easy camera controls
    osp::draw::Camera m_cam;
    ACtxCameraController m_camCtrl;

    // Phong shaders and their required data
    adera::shader::ACtxDrawPhong m_phong{};

    // An ordered set of entities and draw function pointers intended to be
    // forward-rendered
    osp::draw::RenderGroup m_groupFwdOpaque;
};

/**
 * @brief Keeps the EngineTestRenderer up-to-date with the EngineTestScene
 *
 * @param rRenderGl [ref] Application-level GL renderer data
 * @param rScene    [in] Test scene to render
 * @param rRenderer [ref] Renderer data for test scene
 */
void sync_test_scene(
        RenderGL& rRenderGl, EngineTestScene& rScene,
        EngineTestRenderer& rRenderer)
{
    using namespace osp::draw;
    using namespace adera::shader;

    rScene.m_scnRdr.m_drawTransform         .resize(rScene.m_scnRdr.m_drawIds.capacity());
    rRenderer.m_sceneRenderGL.m_diffuseTexId.resize(rScene.m_scnRdr.m_drawIds.capacity());
    rRenderer.m_sceneRenderGL.m_meshId      .resize(rScene.m_scnRdr.m_drawIds.capacity());

    // Assign or remove phong shaders from entities marked dirty
    sync_drawent_phong(rScene.m_matPhongDirty.cbegin(), rScene.m_matPhongDirty.cend(),
    {
        .hasMaterial    = rScene.m_matPhong,
        .pStorageOpaque = &rRenderer.m_groupFwdOpaque.entities,
        .opaque         = rScene.m_scnRdr.m_opaque,
        .transparent    = rScene.m_scnRdr.m_transparent,
        .diffuse        = rRenderer.m_sceneRenderGL.m_diffuseTexId,
        .rData          = rRenderer.m_phong
    });

    // Load required meshes and textures into OpenGL
    SysRenderGL::compile_resource_meshes  (rScene.m_drawingRes, *rScene.m_pResources, rRenderGl);
    SysRenderGL::compile_resource_textures(rScene.m_drawingRes, *rScene.m_pResources, rRenderGl);

    // Assign GL meshes to entities with a mesh component
    SysRenderGL::sync_drawent_mesh(
            rScene.m_scnRdr.m_meshDirty.begin(),
            rScene.m_scnRdr.m_meshDirty.end(),
            rScene.m_scnRdr.m_mesh,
            rScene.m_drawingRes.m_meshToRes,
            rRenderer.m_sceneRenderGL.m_meshId,
            rRenderGl);

    // Assign GL textures to entities with a texture component
    SysRenderGL::sync_drawent_texture(
            rScene.m_scnRdr.m_meshDirty.begin(),
            rScene.m_scnRdr.m_meshDirty.end(),
            rScene.m_scnRdr.m_diffuseTex,
            rScene.m_drawingRes.m_texToRes,
            rRenderer.m_sceneRenderGL.m_diffuseTexId,
            rRenderGl);

    // Calculate hierarchy transforms

    auto drawTfDirty = {rScene.m_cube};

    SysRender::update_draw_transforms(
            {
                .scnGraph     = rScene.m_basic .m_scnGraph,
                .transforms   = rScene.m_basic .m_transform,
                .activeToDraw = rScene.m_scnRdr.m_activeToDraw,
                .needDrawTf   = rScene.m_scnRdr.m_needDrawTf,
                .rDrawTf      = rScene.m_scnRdr.m_drawTransform
            },
            drawTfDirty.begin(),
            drawTfDirty.end());
}

/**
 * @brief Render an EngineTestScene
 *
 * @param rRenderGl [ref] Application-level GL renderer data
 * @param rScene    [in] Test scene to render
 * @param rRenderer [ref] Renderer data for test scene
 */
void render_test_scene(
        RenderGL& rRenderGl, EngineTestScene const& rScene,
        EngineTestRenderer& rRenderer)
{
    using namespace osp::draw;
    using Magnum::GL::Framebuffer;
    using Magnum::GL::FramebufferClear;
    using Magnum::GL::Texture2D;

    // Get camera to calculate view and projection matrix
    ViewProjMatrix const viewProj{
            rRenderer.m_cam.m_transform.inverted(),
            rRenderer.m_cam.perspective()};

    // Bind offscreen FBO
    Framebuffer &rFbo = rRenderGl.m_fbo;
    rFbo.bind();

    // Clear it
    rFbo.clear( FramebufferClear::Color | FramebufferClear::Depth
                | FramebufferClear::Stencil);

    // Forward Render fwd_opaque group to FBO
    SysRenderGL::render_opaque(
            rRenderer.m_groupFwdOpaque,
            rScene.m_scnRdr.m_visible, viewProj);

    // Display FBO
    Texture2D &rFboColor = rRenderGl.m_texGl.get(rRenderGl.m_fboColor);
    SysRenderGL::display_texture(rRenderGl, rFboColor);
}

entt::any make_renderer(EngineTestScene& rScene, MagnumWindowApp &rApp, RenderGL& rRenderGl, UserInputHandler& rUserInput)
{
    using namespace osp::active;
    using namespace osp::draw;
    using namespace adera::shader;

    entt::any rendererAny = entt::make_any<EngineTestRenderer>(rUserInput);

    EngineTestRenderer &rRenderer = entt::any_cast<EngineTestRenderer&>(rendererAny);

    // Create Phong shaders
    auto const texturedFlags
            = PhongGL::Flag::DiffuseTexture | PhongGL::Flag::AlphaMask
            | PhongGL::Flag::AmbientTexture;
    rRenderer.m_phong.shaderDiffuse    = PhongGL{PhongGL::Configuration{}.setFlags(texturedFlags).setLightCount(2)};
    rRenderer.m_phong.shaderUntextured = PhongGL{PhongGL::Configuration{}.setLightCount(2)};
    rRenderer.m_phong.assign_pointers(rScene.m_scnRdr, rRenderer.m_sceneRenderGL, rRenderGl);

    rRenderer.m_cam.set_aspect_ratio(
            osp::Vector2(Magnum::GL::defaultFramebuffer.viewport().size()));

    // Set all drawing stuff dirty then sync with renderer.
    // This allows clean re-openning of the scene
    for (DrawEnt const drawEnt : rScene.m_scnRdr.m_drawIds)
    {
        // Set all meshs dirty
        if (rScene.m_scnRdr.m_mesh[drawEnt] != lgrn::id_null<MeshId>())
        {
            rScene.m_scnRdr.m_meshDirty.push_back(drawEnt);
        }

        // Set all textures dirty
        if (rScene.m_scnRdr.m_diffuseTex[drawEnt] != lgrn::id_null<TexId>())
        {
            rScene.m_scnRdr.m_diffuseDirty.push_back(drawEnt);
        }
    }

    for (MaterialId const materialId : rScene.m_scnRdr.m_materialIds)
    {
        Material &mat = rScene.m_scnRdr.m_materials[materialId];
        for (DrawEnt const drawEnt : mat.m_ents)
        {
            mat.m_dirty.push_back(drawEnt);
        }
    }

    for (DrawEnt const drawEnt : rScene.m_matPhong)
    {
        rScene.m_matPhongDirty.emplace_back(drawEnt);
    }

    sync_test_scene(rRenderGl, rScene, rRenderer);

    return rendererAny;
}

void draw(EngineTestScene &rScene, EngineTestRenderer &rRenderer, RenderGL &rRenderGl, MagnumWindowApp &rApp, float delta)
{
    update_test_scene(rScene, delta);

    // Rotate and move the camera based on user inputs
    SysCameraController::update_view(rRenderer.m_camCtrl, delta);
    SysCameraController::update_move(rRenderer.m_camCtrl, delta, true);
    rRenderer.m_cam.m_transform = rRenderer.m_camCtrl.m_transform;

    sync_test_scene  (rRenderGl, rScene, rRenderer);
    render_test_scene(rRenderGl, rScene, rRenderer);
}


} // namespace testapp::enginetest
