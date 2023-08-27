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
#include "CameraController.h"

#include "../MagnumApplication.h"

#include <osp/Active/basic.h>
#include <osp/Active/drawing.h>
#include <osp/Active/SysSceneGraph.h>
#include <osp/Active/SysRender.h>
#include <osp/Active/opengl/SysRenderGL.h>

#include <osp/Shaders/Phong.h>

#include <osp/Resource/resources.h>

#include <longeron/id_management/registry_stl.hpp>

#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/ImageData.h>

#include <Magnum/GL/DefaultFramebuffer.h>

#include <Corrade/Containers/ArrayViewStl.h>

using osp::active::ActiveEnt;
using osp::active::RenderGL;
using osp::input::UserInputHandler;

using Magnum::Trade::MeshData;
using Magnum::Trade::ImageData2D;

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
    osp::Resources *m_pResources;

    // ID registry generates entity IDs, and keeps track of which ones exist
    lgrn::IdRegistryStl<ActiveEnt>  m_activeIds;

    // Supports transforms, hierarchy, cameras, and other components assignable
    // to ActiveEnts
    osp::active::ACtxBasic          m_basic;

    // Support for 'scene-space' meshes and textures, drawing components for
    // ActiveEnt such as visible, opaque, and diffuse texture.
    osp::active::ACtxDrawing        m_drawing;

    // Support for associating scene-space meshes/textures with Resources
    // Meshes/textures can span 3 different spaces, with their own ID types:
    // * Resources  (ResId)            Loaded data, from files or generated
    // * Renderer   (MeshGlId/TexGlId) Shared between scenes, used by GPU
    // * Scene      (MeshId/TexId)     Local to one scene
    // ACtxDrawingRes is a two-way mapping between MeshId/TexId <--> ResId
    osp::active::ACtxDrawingRes     m_drawingRes;

    // The rotating cube
    ActiveEnt                       m_cube{lgrn::id_null<ActiveEnt>()};

    // Set of ActiveEnts that are assigned a Phong material
    osp::active::ActiveEntSet_t         m_matPhong;
    std::vector<osp::active::DrawEnt>   m_matPhongDirty;

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
    osp::active::SysRender::clear_owners(m_drawing);
    osp::active::SysRender::clear_resource_owners(m_drawingRes, *m_pResources);
}

entt::any setup_scene(osp::Resources& rResources, osp::PkgId const pkg)
{
    using namespace osp::active;

    entt::any sceneAny = entt::make_any<EngineTestScene>();
    auto &rScene = entt::any_cast<EngineTestScene&>(sceneAny);

    rScene.m_pResources = &rResources;

    // Make a cube
    ActiveEnt const cubeEnt = rScene.m_activeIds.create();
    DrawEnt const   cubeDraw = rScene.m_drawing.m_drawIds.create();

    // Resize some containers to fit all existing entities
    rScene.m_matPhong.ints()    .resize(rScene.m_activeIds.vec().capacity());
    rScene.m_basic.m_scnGraph   .resize(rScene.m_activeIds.capacity());
    rScene.m_drawing.resize_active(rScene.m_activeIds.capacity());
    rScene.m_drawing.resize_draw();

    // Take ownership of the cube mesh Resource. This will create a scene-space
    // MeshId that we can assign to ActiveEnts
    osp::ResId const resCube = rResources.find(osp::restypes::gc_mesh, pkg, "cube");
    assert(resCube != lgrn::id_null<osp::ResId>());
    MeshId const meshCube = SysRender::own_mesh_resource(rScene.m_drawing, rScene.m_drawingRes, rResources, resCube);

    // Add cube mesh to cube

    rScene.m_drawing.m_needDrawTf.set(std::size_t(cubeEnt));
    rScene.m_drawing.m_activeToDraw[cubeEnt] = cubeDraw;
    rScene.m_drawing.m_mesh[cubeDraw] = rScene.m_drawing.m_meshRefCounts.ref_add(meshCube);
    rScene.m_drawing.m_meshDirty.push_back(cubeDraw);

    // Add transform
    rScene.m_basic.m_transform.emplace(cubeEnt);

    // Add phong material to cube
    rScene.m_matPhong.set(std::size_t(cubeDraw));
    rScene.m_matPhongDirty.push_back(cubeDraw);

    // Add drawble, opaque, and visible component
    rScene.m_drawing.m_visible.set(std::size_t(cubeDraw));
    rScene.m_drawing.m_drawBasic[cubeDraw].m_opaque = true;

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
    rScene.m_drawing.m_meshDirty.clear();
    rScene.m_drawing.m_diffuseDirty.clear();
    rScene.m_matPhongDirty.clear();

    // Rotate the cube
    osp::Matrix4 &rCubeTf
            = rScene.m_basic.m_transform.get(rScene.m_cube).m_transform;

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
    osp::active::ACtxSceneRenderGL m_sceneRenderGL{};

    // Pre-built easy camera controls
    osp::active::Camera m_cam;
    ACtxCameraController m_camCtrl;

    // Phong shaders and their required data
    osp::shader::ACtxDrawPhong m_phong{};

    // An ordered set of entities and draw function pointers intended to be
    // forward-rendered
    osp::active::RenderGroup m_groupFwdOpaque;
};

/**
 * @brief Keeps the EngineTestRenderer up-to-date with the EngineTestScene
 *
 * @param rRenderGl [ref] Application-level GL renderer data
 * @param rScene    [in] Test scene to render
 * @param rRenderer [ref] Renderer data for test scene
 */
void sync_test_scene(
        RenderGL& rRenderGl, EngineTestScene const& rScene,
        EngineTestRenderer& rRenderer)
{
    using namespace osp::active;
    using namespace osp::shader;

    rRenderer.m_sceneRenderGL.m_drawTransform.resize(rScene.m_drawing.m_drawIds.capacity());
    rRenderer.m_sceneRenderGL.m_diffuseTexId.resize(rScene.m_drawing.m_drawIds.capacity());
    rRenderer.m_sceneRenderGL.m_meshId.resize(rScene.m_drawing.m_drawIds.capacity());

    // Assign or remove phong shaders from entities marked dirty
    sync_phong(
            std::cbegin(rScene.m_matPhongDirty),
            std::cend(rScene.m_matPhongDirty),
            rScene.m_matPhong, &rRenderer.m_groupFwdOpaque.m_entities, nullptr,
            rScene.m_drawing.m_drawBasic, rRenderer.m_sceneRenderGL.m_diffuseTexId,
            rRenderer.m_phong);

    // Load required meshes and textures into OpenGL
    SysRenderGL::compile_resource_meshes  (rScene.m_drawingRes, *rScene.m_pResources, rRenderGl);
    SysRenderGL::compile_resource_textures(rScene.m_drawingRes, *rScene.m_pResources, rRenderGl);

    // Assign GL meshes to entities with a mesh component
    SysRenderGL::sync_drawent_mesh(
            rScene.m_drawing.m_meshDirty.begin(),
            rScene.m_drawing.m_meshDirty.end(),
            rScene.m_drawing.m_mesh,
            rScene.m_drawingRes.m_meshToRes,
            rRenderer.m_sceneRenderGL.m_meshId,
            rRenderGl);

    // Assign GL textures to entities with a texture component
    SysRenderGL::sync_drawent_texture(
            rScene.m_drawing.m_meshDirty.begin(),
            rScene.m_drawing.m_meshDirty.end(),
            rScene.m_drawing.m_diffuseTex,
            rScene.m_drawingRes.m_texToRes,
            rRenderer.m_sceneRenderGL.m_diffuseTexId,
            rRenderGl);

    // Calculate hierarchy transforms

    auto drawTfDirty = {rScene.m_cube};

    SysRender::update_draw_transforms(
            rScene.m_basic.m_scnGraph,
            rScene.m_drawing.m_activeToDraw,
            rScene.m_basic.m_transform,
            rRenderer.m_sceneRenderGL.m_drawTransform,
            rScene.m_drawing.m_needDrawTf,
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
    using namespace osp::active;
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
            rScene.m_drawing.m_visible, viewProj);

    // Display FBO
    Texture2D &rFboColor = rRenderGl.m_texGl.get(rRenderGl.m_fboColor);
    SysRenderGL::display_texture(rRenderGl, rFboColor);
}

class EngineTestApp : public IOspApplication
{
public:
    EngineTestApp(EngineTestRenderer renderer, EngineTestScene& rScene, RenderGL& rRenderGl)
     : m_renderer   {std::move(renderer)}
     , m_rScene     {rScene}
     , m_rRenderGl  {rRenderGl}
    { }

    ~EngineTestApp() override
    { };

    void run(MagnumApplication& rApp) override
    { }

    void draw(MagnumApplication& rApp, float delta) override
    {
        update_test_scene(m_rScene, delta);

        // Rotate and move the camera based on user inputs
        SysCameraController::update_view(m_renderer.m_camCtrl, delta);
        SysCameraController::update_move(m_renderer.m_camCtrl, delta, true);
        m_renderer.m_cam.m_transform = m_renderer.m_camCtrl.m_transform;

        sync_test_scene  (m_rRenderGl, m_rScene, m_renderer);
        render_test_scene(m_rRenderGl, m_rScene, m_renderer);
    }

    void exit(MagnumApplication& rApp) override
    { }

    EngineTestRenderer  m_renderer;

    EngineTestScene     &m_rScene;
    RenderGL            &m_rRenderGl;
};

MagnumApplication::AppPtr_t generate_draw_func(EngineTestScene& rScene, MagnumApplication &rApp, RenderGL& rRenderGl, UserInputHandler& rUserInput)
{
    using namespace osp::active;
    using namespace osp::shader;

    auto pApp = std::make_unique<EngineTestApp>(EngineTestRenderer{rUserInput}, rScene, rRenderGl);

    EngineTestRenderer &rRenderer = pApp->m_renderer;

    // Create Phong shaders
    auto const texturedFlags
            = Phong::Flag::DiffuseTexture | Phong::Flag::AlphaMask
            | Phong::Flag::AmbientTexture;
    rRenderer.m_phong.m_shaderDiffuse      = Phong{Phong::Configuration{}.setFlags(texturedFlags).setLightCount(2)};
    rRenderer.m_phong.m_shaderUntextured   = Phong{Phong::Configuration{}.setLightCount(2)};
    rRenderer.m_phong.assign_pointers(rRenderer.m_sceneRenderGL, rRenderGl);

    rRenderer.m_cam.set_aspect_ratio(
            osp::Vector2(Magnum::GL::defaultFramebuffer.viewport().size()));

    // Set all drawing stuff dirty then sync with renderer.
    // This allows clean re-openning of the scene
    SysRender::set_dirty_all(rScene.m_drawing);
    for (std::size_t const entInt : rScene.m_matPhong.ones())
    {
        rScene.m_matPhongDirty.push_back(DrawEnt(entInt));
    }

    sync_test_scene(rRenderGl, rScene, rRenderer);

    return pApp;
}

} // namespace testapp::enginetest
