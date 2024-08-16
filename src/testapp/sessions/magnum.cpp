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
#include "magnum.h"
#include "terrain.h"
#include "common.h"


#include "../MagnumApplication.h"

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>

#include <adera/drawing/CameraController.h>
#include <adera_drawing_gl/flat_shader.h>
#include <adera_drawing_gl/phong_shader.h>
#include <adera_drawing_gl/visualizer_shader.h>
#include <osp/activescene/basic_fn.h>
#include <osp/drawing/drawing.h>
#include <osp_drawing_gl/rendergl.h>
#include <osp/universe/coordinates.h>
#include <osp/universe/universe.h>

#include <adera/machines/links.h>

#include <planet-a/geometry.h>

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

using namespace adera;
using namespace adera::shader;
using namespace osp::active;
using namespace osp::draw;
using namespace osp::universe;
using namespace osp::fw;
using namespace osp;

using osp::input::UserInputHandler;

using Magnum::GL::Mesh;

namespace testapp::scenes
{


FeatureDef const ftrMagnum = feature_def("Magnum", [] (
        FeatureBuilder& rFB, Implement<FIMagnum> magnum, DependOn<FIWindowApp> windowApp,
        DependOn<FIMainApp> mainApp, entt::any data)
{
    auto& rUserInput = rFB.data_get<UserInputHandler>(windowApp.di.userInput);
    config_controls(rUserInput);

    rFB.pipeline(magnum.pl.meshGL)         .parent(windowApp.pl.sync);
    rFB.pipeline(magnum.pl.textureGL)      .parent(windowApp.pl.sync);
    rFB.pipeline(magnum.pl.entMeshGL)      .parent(windowApp.pl.sync);
    rFB.pipeline(magnum.pl.entTextureGL)   .parent(windowApp.pl.sync);

    auto const& args = entt::any_cast<MagnumApplication::Arguments>(data);

    // Order-dependent; MagnumApplication construction starts OpenGL context, needed by RenderGL
    /* not used here */   rFB.data_emplace<MagnumApplication>(magnum.di.magnumApp, args, rUserInput);
    auto &rRenderGl     = rFB.data_emplace<RenderGL>         (magnum.di.renderGl);

    SysRenderGL::setup_context(rRenderGl);

    rFB.task()
        .name       ("Clean up Magnum renderer")
        .run_on     ({windowApp.pl.cleanup(Run_)})
        .args       ({      mainApp.di.resources,          magnum.di.renderGl})
        .func([] (Resources& rResources, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::clear_resource_owners(rRenderGl, rResources);
        rRenderGl = {}; // Needs the OpenGL thread for destruction
    });

}); // ftrMagnum



#if 0

Session setup_magnum_scene(
        TopTaskBuilder&             rFB,
        ArrayView<entt::any> const  topData,
        Session const&              application,
        Session const&              windowApp,
        Session const&              sceneRenderer,
        Session const&              magnum,
        Session const&              scene,
        Session const&              commonScene)
{
    OSP_DECLARE_GET_DATA_IDS(application,   TESTAPP_DATA_APPLICATION);
    OSP_DECLARE_GET_DATA_IDS(commonScene,   TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(windowApp,     TESTAPP_DATA_WINDOW_APP);
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer, TESTAPP_DATA_SCENE_RENDERER);
    OSP_DECLARE_GET_DATA_IDS(magnum,        TESTAPP_DATA_MAGNUM);

    auto const windowApp.pl    = windowApp     .get_pipelines< PlWindowApp >();
    auto const magnum.pl    = magnum        .get_pipelines< PlMagnum >();
    auto const scene.plRdr = sceneRenderer .get_pipelines< PlSceneRenderer >();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_MAGNUM_SCENE);
    auto const magnum.plScn = out.create_pipelines<PlMagnumScene>(rFB);

    rFB.pipeline(magnum.plScn.fbo)             .parent(scene.plRdr.render);
    rFB.pipeline(magnum.plScn.camera)          .parent(scene.plRdr.render);

    rFB.data_emplace< ACtxSceneRenderGL >    (topData, idScnRenderGl);
    rFB.data_emplace< RenderGroup >          (topData, idGroupFwd);

    auto &rCamera = rFB.data_emplace< Camera >(topData, idCamera);

    rCamera.m_far = 100000000.0f;
    rCamera.m_near = 1.0f;
    rCamera.m_fov = Magnum::Deg(45.0f);

    rFB.task()
        .name       ("Resize ACtxSceneRenderGL (OpenGL) to fit all DrawEnts")
        .run_on     ({scene.plRdr.drawEntResized(Run)})
        .sync_with  ({})
        .push_to    (out.m_tasks)
        .args       ({ idScnRender, idScnRenderGl })
        .func       ([] (ACtxSceneRender const& rScnRender, ACtxSceneRenderGL& rScnRenderGl) noexcept
    {
        std::size_t const capacity = rScnRender.m_drawIds.capacity();
        rScnRenderGl.m_diffuseTexId   .resize(capacity);
        rScnRenderGl.m_meshId         .resize(capacity);
    });

    rFB.task()
        .name       ("Compile Resource Meshes to GL")
        .run_on     ({scene.plRdr.meshResDirty(UseOrRun)})
        .sync_with  ({scene.plRdr.mesh(Ready), magnum.pl.meshGL(New), scene.plRdr.entMeshDirty(UseOrRun)})
        .push_to    (out.m_tasks)
        .args       ({                 commonScene.di.drawingRes,                mainApp.di.resources,          magnum.di.renderGl })
        .func([] (ACtxDrawingRes const& rDrawingRes, osp::Resources& rResources, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::compile_resource_meshes(rDrawingRes, rResources, rRenderGl);
    });

    rFB.task()
        .name       ("Compile Resource Textures to GL")
        .run_on     ({scene.plRdr.textureResDirty(UseOrRun)})
        .sync_with  ({scene.plRdr.texture(Ready), magnum.pl.textureGL(New)})
        .push_to    (out.m_tasks)
        .args       ({                 commonScene.di.drawingRes,                mainApp.di.resources,          magnum.di.renderGl })
        .func([] (ACtxDrawingRes const& rDrawingRes, osp::Resources& rResources, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::compile_resource_textures(rDrawingRes, rResources, rRenderGl);
    });

    rFB.task()
        .name       ("Sync GL textures to entities with scene textures")
        .run_on     ({scene.plRdr.entTextureDirty(UseOrRun)})
        .sync_with  ({scene.plRdr.texture(Ready), scene.plRdr.entTexture(Ready), magnum.pl.textureGL(Ready), magnum.pl.entTextureGL(Modify), scene.plRdr.drawEntResized(Done)})
        .push_to    (out.m_tasks)
        .args       ({        commonScene.di.drawing,                commonScene.di.drawingRes,                 idScnRender,                   idScnRenderGl,          magnum.di.renderGl })
        .func([] (ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, ACtxSceneRender& rScnRender, ACtxSceneRenderGL& rScnRenderGl, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::sync_drawent_texture(
                rScnRender.m_diffuseDirty.begin(),
                rScnRender.m_diffuseDirty.end(),
                rScnRender.m_diffuseTex,
                rDrawingRes.m_texToRes,
                rScnRenderGl.m_diffuseTexId,
                rRenderGl);
    });

    rFB.task()
        .name       ("Resync GL textures")
        .run_on     ({windowApp.pl.resync(Run)})
        .sync_with  ({scene.plRdr.texture(Ready), magnum.pl.textureGL(Ready), magnum.pl.entTextureGL(Modify), scene.plRdr.drawEntResized(Done)})
        .push_to    (out.m_tasks)
        .args       ({           commonScene.di.drawingRes,                 idScnRender,                   idScnRenderGl,          magnum.di.renderGl })
        .func([] (ACtxDrawingRes& rDrawingRes, ACtxSceneRender& rScnRender, ACtxSceneRenderGL& rScnRenderGl, RenderGL& rRenderGl) noexcept
    {
        for (DrawEnt const drawEnt : rScnRender.m_drawIds)
        {
            SysRenderGL::sync_drawent_texture(
                    drawEnt,
                    rScnRender.m_diffuseTex,
                    rDrawingRes.m_texToRes,
                    rScnRenderGl.m_diffuseTexId,
                    rRenderGl);
        }
    });

    rFB.task()
        .name       ("Sync GL meshes to entities with scene meshes")
        .run_on     ({scene.plRdr.entMeshDirty(UseOrRun)})
        .sync_with  ({scene.plRdr.mesh(Ready), scene.plRdr.entMesh(Ready), magnum.pl.meshGL(Ready), magnum.pl.entMeshGL(Modify), scene.plRdr.drawEntResized(Done)})
        .push_to    (out.m_tasks)
        .args       ({           commonScene.di.drawingRes,                 idScnRender,                   idScnRenderGl,          magnum.di.renderGl })
        .func([] (ACtxDrawingRes& rDrawingRes, ACtxSceneRender& rScnRender, ACtxSceneRenderGL& rScnRenderGl, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::sync_drawent_mesh(
                rScnRender.m_meshDirty.begin(),
                rScnRender.m_meshDirty.end(),
                rScnRender.m_mesh,
                rDrawingRes.m_meshToRes,
                rScnRenderGl.m_meshId,
                rRenderGl);
    });

    rFB.task()
        .name       ("Resync GL meshes")
        .run_on     ({windowApp.pl.resync(Run)})
        .sync_with  ({scene.plRdr.mesh(Ready), magnum.pl.meshGL(Ready), magnum.pl.entMeshGL(Modify), scene.plRdr.drawEntResized(Done)})
        .push_to    (out.m_tasks)
        .args       ({           commonScene.di.drawingRes,                 idScnRender,                   idScnRenderGl,          magnum.di.renderGl })
        .func([] (ACtxDrawingRes& rDrawingRes, ACtxSceneRender& rScnRender, ACtxSceneRenderGL& rScnRenderGl, RenderGL& rRenderGl) noexcept
    {
        for (DrawEnt const drawEnt : rScnRender.m_drawIds)
        {
            SysRenderGL::sync_drawent_mesh(
                    drawEnt,
                    rScnRender.m_mesh,
                    rDrawingRes.m_meshToRes,
                    rScnRenderGl.m_meshId,
                    rRenderGl);
        }
    });

    rFB.task()
        .name       ("Bind and display off-screen FBO")
        .run_on     ({scene.plRdr.render(Run)})
        .sync_with  ({magnum.plScn.fbo(EStgFBO::Bind)})
        .push_to    (out.m_tasks)
        .args       ({              commonScene.di.drawing,          magnum.di.renderGl,                   idGroupFwd,              idCamera })
        .func([] (ACtxDrawing const& rDrawing, RenderGL& rRenderGl, RenderGroup const& rGroupFwd, Camera const& rCamera) noexcept
    {
        using Magnum::GL::Framebuffer;
        using Magnum::GL::FramebufferClear;

        Framebuffer &rFbo = rRenderGl.m_fbo;
        rFbo.bind();

        Magnum::GL::Texture2D &rFboColor = rRenderGl.m_texGl.get(rRenderGl.m_fboColor);
        SysRenderGL::display_texture(rRenderGl, rFboColor);

        rFbo.clear(   FramebufferClear::Color | FramebufferClear::Depth
                    | FramebufferClear::Stencil);
    });

    rFB.task()
        .name       ("Render Entities")
        .run_on     ({scene.plRdr.render(Run)})
        .sync_with  ({scene.plRdr.group(Ready), scene.plRdr.groupEnts(Ready), magnum.plScn.camera(Ready), scene.plRdr.drawTransforms(UseOrRun), scene.plRdr.entMesh(Ready), scene.plRdr.entTexture(Ready),
                      magnum.pl.entMeshGL(Ready), magnum.pl.entTextureGL(Ready),
                      scene.plRdr.drawEnt(Ready)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender,          magnum.di.renderGl,                   idGroupFwd,              idCamera })
        .func([] (ACtxSceneRender& rScnRender, RenderGL& rRenderGl, RenderGroup const& rGroupFwd, Camera const& rCamera, WorkerContext ctx) noexcept
    {
        ViewProjMatrix viewProj{rCamera.m_transform.inverted(), rCamera.perspective()};

        // Forward Render fwd_opaque group to FBO
        SysRenderGL::render_opaque(rGroupFwd, rScnRender.m_visible, viewProj);
    });

    rFB.task()
        .name       ("Delete entities from render groups")
        .run_on     ({scene.plRdr.drawEntDelete(UseOrRun)})
        .sync_with  ({scene.plRdr.groupEnts(Delete)})
        .push_to    (out.m_tasks)
        .args       ({              commonScene.di.drawing,             idGroupFwd,                 idDrawEntDel })
        .func([] (ACtxDrawing const& rDrawing, RenderGroup& rGroup, DrawEntVec_t const& rDrawEntDel) noexcept
    {
        for (DrawEnt const drawEnt : rDrawEntDel)
        {
            rGroup.entities.remove(drawEnt);
        }
    });

    return out;
} // setup_magnum_scene




Session setup_shader_visualizer(
        TopTaskBuilder&             rFB,
        ArrayView<entt::any> const  topData,
        Session const&              windowApp,
        Session const&              sceneRenderer,
        Session const&              magnum,
        Session const&              magnumScene,
        MaterialId const            materialId)
{
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer, TESTAPP_DATA_SCENE_RENDERER);
    OSP_DECLARE_GET_DATA_IDS(magnumScene,   TESTAPP_DATA_MAGNUM_SCENE);
    OSP_DECLARE_GET_DATA_IDS(magnum,        TESTAPP_DATA_MAGNUM);
    auto const windowApp.pl    = windowApp     .get_pipelines< PlWindowApp >();
    auto const scene.plRdr = sceneRenderer .get_pipelines< PlSceneRenderer >();
    auto const magnum.pl    = magnum        .get_pipelines< PlMagnum >();

    auto &rScnRender    = rFB.data_get< ACtxSceneRender >    (topData, idScnRender);
    auto &rScnRenderGl  = rFB.data_get< ACtxSceneRenderGL >  (topData, idScnRenderGl);
    auto &rRenderGl     = rFB.data_get< RenderGL >           (topData, magnum.di.renderGl);

    Session out;
    auto const [idDrawShVisual] = out.acquire_data<1>(topData);

    auto &rDrawVisual = rFB.data_emplace< ACtxDrawMeshVisualizer >(topData, idDrawShVisual);

    rDrawVisual.m_materialId = materialId;
    rDrawVisual.m_shader = MeshVisualizer{ MeshVisualizer::Configuration{}.setFlags(MeshVisualizer::Flag::Wireframe) };
    rDrawVisual.assign_pointers(rScnRender, rScnRenderGl, rRenderGl);

    // Default colors
    rDrawVisual.m_shader.setWireframeColor({0.7f, 0.5f, 0.7f, 1.0f});
    rDrawVisual.m_shader.setColor({0.2f, 0.1f, 0.5f, 1.0f});

    if (materialId == lgrn::id_null<MaterialId>())
    {
        return out;
    }

    rFB.task()
        .name       ("Sync MeshVisualizer shader DrawEnts")
        .run_on     ({windowApp.pl.sync(Run)})
        .sync_with  ({scene.plRdr.materialDirty(UseOrRun), magnum.pl.textureGL(Ready), scene.plRdr.groupEnts(Modify)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender,             idGroupFwd,                        idDrawShVisual})
        .func([] (ACtxSceneRender& rScnRender, RenderGroup& rGroupFwd, ACtxDrawMeshVisualizer& rDrawShVisual) noexcept
    {
        Material const &rMat = rScnRender.m_materials[rDrawShVisual.m_materialId];
        sync_drawent_visualizer(rMat.m_dirty.begin(), rMat.m_dirty.end(), rMat.m_ents, rGroupFwd.entities, rDrawShVisual);
    });

    rFB.task()
        .name       ("Resync MeshVisualizer shader DrawEnts")
        .run_on     ({windowApp.pl.resync(Run)})
        .sync_with  ({scene.plRdr.groupEnts(Modify), scene.plRdr.group(Modify)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender,             idGroupFwd,                        idDrawShVisual})
        .func([] (ACtxSceneRender& rScnRender, RenderGroup& rGroupFwd, ACtxDrawMeshVisualizer& rDrawShVisual) noexcept
    {
        Material const &rMat = rScnRender.m_materials[rDrawShVisual.m_materialId];
        for (DrawEnt const drawEnt : rMat.m_ents)
        {
            sync_drawent_visualizer(drawEnt, rMat.m_ents, rGroupFwd.entities, rDrawShVisual);
        }
    });

    return out;
} // setup_shader_visualizer




Session setup_shader_flat(
        TopTaskBuilder&             rFB,
        ArrayView<entt::any> const  topData,
        Session const&              windowApp,
        Session const&              sceneRenderer,
        Session const&              magnum,
        Session const&              magnumScene,
        MaterialId const            materialId)
{
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer, TESTAPP_DATA_SCENE_RENDERER);
    OSP_DECLARE_GET_DATA_IDS(magnumScene,   TESTAPP_DATA_MAGNUM_SCENE);
    OSP_DECLARE_GET_DATA_IDS(magnum,        TESTAPP_DATA_MAGNUM);
    auto const windowApp.pl    = windowApp     .get_pipelines< PlWindowApp >();
    auto const scene.plRdr = sceneRenderer .get_pipelines< PlSceneRenderer >();
    auto const magnum.pl    = magnum        .get_pipelines< PlMagnum >();

    auto &rScnRender    = rFB.data_get< ACtxSceneRender >    (topData, idScnRender);
    auto &rScnRenderGl  = rFB.data_get< ACtxSceneRenderGL >  (topData, idScnRenderGl);
    auto &rRenderGl     = rFB.data_get< RenderGL >           (topData, magnum.di.renderGl);

    Session out;
    auto const [idDrawShFlat] = out.acquire_data<1>(topData);

    auto &rDrawFlat = rFB.data_emplace< ACtxDrawFlat >(topData, idDrawShFlat);

    rDrawFlat.shaderDiffuse       = FlatGL3D{FlatGL3D::Configuration{}.setFlags(FlatGL3D::Flag::Textured)};
    rDrawFlat.shaderUntextured    = FlatGL3D{FlatGL3D::Configuration{}};
    rDrawFlat.materialId          = materialId;
    rDrawFlat.assign_pointers(rScnRender, rScnRenderGl, rRenderGl);

    if (materialId == lgrn::id_null<MaterialId>())
    {
        return out;
    }

    rFB.task()
        .name       ("Sync Flat shader DrawEnts")
        .run_on     ({windowApp.pl.sync(Run)})
        .sync_with  ({scene.plRdr.groupEnts(Modify), scene.plRdr.group(Modify), scene.plRdr.materialDirty(UseOrRun)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender,             idGroupFwd,                         idScnRenderGl,              idDrawShFlat})
        .func([] (ACtxSceneRender& rScnRender, RenderGroup& rGroupFwd, ACtxSceneRenderGL const& rScnRenderGl, ACtxDrawFlat& rDrawShFlat) noexcept
    {
        Material const &rMat = rScnRender.m_materials[rDrawShFlat.materialId];
        sync_drawent_flat(rMat.m_dirty.begin(), rMat.m_dirty.end(),
        {
            .hasMaterial    = rMat.m_ents,
            .pStorageOpaque = &rGroupFwd.entities,
            /* TODO: set .pStorageTransparent */
            .opaque         = rScnRender.m_opaque,
            .transparent    = rScnRender.m_transparent,
            .diffuse        = rScnRenderGl.m_diffuseTexId,
            .rData          = rDrawShFlat
        });
    });

    rFB.task()
        .name       ("Resync Flat shader DrawEnts")
        .run_on     ({windowApp.pl.resync(Run)})
        .sync_with  ({scene.plRdr.materialDirty(UseOrRun), magnum.pl.textureGL(Ready), scene.plRdr.groupEnts(Modify), scene.plRdr.group(Modify)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender,             idGroupFwd,                         idScnRenderGl,              idDrawShFlat})
        .func([] (ACtxSceneRender& rScnRender, RenderGroup& rGroupFwd, ACtxSceneRenderGL const& rScnRenderGl, ACtxDrawFlat& rDrawShFlat) noexcept
    {
        Material const &rMat = rScnRender.m_materials[rDrawShFlat.materialId];
        for (DrawEnt const drawEnt : rMat.m_ents)
        {
            sync_drawent_flat(drawEnt,
            {
                .hasMaterial    = rMat.m_ents,
                .pStorageOpaque = &rGroupFwd.entities,
                /* TODO: set .pStorageTransparent */
                .opaque         = rScnRender.m_opaque,
                .transparent    = rScnRender.m_transparent,
                .diffuse        = rScnRenderGl.m_diffuseTexId,
                .rData          = rDrawShFlat
            });
        }
    });

    return out;
} // setup_shader_flat




Session setup_shader_phong(
        TopTaskBuilder&             rFB,
        ArrayView<entt::any> const  topData,
        Session const&              windowApp,
        Session const&              sceneRenderer,
        Session const&              magnum,
        Session const&              magnumScene,
        MaterialId const            materialId)
{
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer, TESTAPP_DATA_SCENE_RENDERER);
    OSP_DECLARE_GET_DATA_IDS(magnumScene,   TESTAPP_DATA_MAGNUM_SCENE);
    OSP_DECLARE_GET_DATA_IDS(magnum,        TESTAPP_DATA_MAGNUM);
    auto const windowApp.pl    = windowApp     .get_pipelines< PlWindowApp >();
    auto const scene.plRdr = sceneRenderer .get_pipelines< PlSceneRenderer >();
    auto const magnum.pl    = magnum        .get_pipelines< PlMagnum >();

    auto &rScnRender    = rFB.data_get< ACtxSceneRender >    (topData, idScnRender);
    auto &rScnRenderGl  = rFB.data_get< ACtxSceneRenderGL >  (topData, idScnRenderGl);
    auto &rRenderGl     = rFB.data_get< RenderGL >           (topData, magnum.di.renderGl);

    Session out;
    auto const [idDrawShPhong] = out.acquire_data<1>(topData);
    auto &rDrawPhong = rFB.data_emplace< ACtxDrawPhong >(topData, idDrawShPhong);

    auto const texturedFlags    = PhongGL::Flag::DiffuseTexture | PhongGL::Flag::AlphaMask | PhongGL::Flag::AmbientTexture;
    rDrawPhong.shaderDiffuse    = PhongGL{PhongGL::Configuration{}.setFlags(texturedFlags).setLightCount(2)};
    rDrawPhong.shaderUntextured = PhongGL{PhongGL::Configuration{}.setLightCount(2)};
    rDrawPhong.materialId       = materialId;
    rDrawPhong.assign_pointers(rScnRender, rScnRenderGl, rRenderGl);

    if (materialId == lgrn::id_null<MaterialId>())
    {
        return out;
    }

    rFB.task()
        .name       ("Sync Phong shader DrawEnts")
        .run_on     ({windowApp.pl.sync(Run)})
        .sync_with  ({scene.plRdr.materialDirty(UseOrRun), magnum.pl.entTextureGL(Ready), scene.plRdr.groupEnts(Modify), scene.plRdr.group(Modify)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender,             idGroupFwd,                         idScnRenderGl,               idDrawShPhong})
        .func([] (ACtxSceneRender& rScnRender, RenderGroup& rGroupFwd, ACtxSceneRenderGL const& rScnRenderGl, ACtxDrawPhong& rDrawShPhong) noexcept
    {
        Material const &rMat = rScnRender.m_materials[rDrawShPhong.materialId];
        sync_drawent_phong(rMat.m_dirty.begin(), rMat.m_dirty.end(),
        {
            .hasMaterial    = rMat.m_ents,
            .pStorageOpaque = &rGroupFwd.entities,
            /* TODO: set .pStorageTransparent */
            .opaque         = rScnRender.m_opaque,
            .transparent    = rScnRender.m_transparent,
            .diffuse        = rScnRenderGl.m_diffuseTexId,
            .rData          = rDrawShPhong
        });
    });

    rFB.task()
        .name       ("Resync Phong shader DrawEnts")
        .run_on     ({windowApp.pl.resync(Run)})
        .sync_with  ({scene.plRdr.materialDirty(UseOrRun), magnum.pl.entTextureGL(Ready), scene.plRdr.groupEnts(Modify), scene.plRdr.group(Modify)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender,             idGroupFwd,                         idScnRenderGl,               idDrawShPhong})
        .func([] (ACtxSceneRender& rScnRender, RenderGroup& rGroupFwd, ACtxSceneRenderGL const& rScnRenderGl, ACtxDrawPhong& rDrawShPhong) noexcept
    {
        Material const &rMat = rScnRender.m_materials[rDrawShPhong.materialId];
        for (DrawEnt const drawEnt : rMat.m_ents)
        {
            sync_drawent_phong(drawEnt,
            {
                .hasMaterial    = rMat.m_ents,
                .pStorageOpaque = &rGroupFwd.entities,
                .opaque         = rScnRender.m_opaque,
                .transparent    = rScnRender.m_transparent,
                .diffuse        = rScnRenderGl.m_diffuseTexId,
                .rData          = rDrawShPhong
            });
        }
    });

    return out;
} // setup_shader_phong




struct ACtxDrawTerrainGL
{
    Magnum::GL::Buffer  vrtxBufGL{Corrade::NoCreate};
    Magnum::GL::Buffer  indxBufGL{Corrade::NoCreate};
    MeshGlId            terrainMeshGl;
    bool                enabled{false};
};

Session setup_terrain_draw_magnum(
        TopTaskBuilder              &rFB,
        ArrayView<entt::any>  const topData,
        Session               const &windowApp,
        Session               const &sceneRenderer,
        Session               const &magnum,
        Session               const &magnumScene,
        Session               const &terrain)
{
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer, TESTAPP_DATA_SCENE_RENDERER);
    OSP_DECLARE_GET_DATA_IDS(magnumScene,   TESTAPP_DATA_MAGNUM_SCENE);
    OSP_DECLARE_GET_DATA_IDS(magnum,        TESTAPP_DATA_MAGNUM);
    OSP_DECLARE_GET_DATA_IDS(terrain,       TESTAPP_DATA_TERRAIN);
    auto const windowApp.pl    = windowApp     .get_pipelines< PlWindowApp >();
    auto const scene.plRdr = sceneRenderer .get_pipelines< PlSceneRenderer >();
    auto const magnum.pl    = magnum        .get_pipelines< PlMagnum >();
    auto const tgTrn    = terrain       .get_pipelines<PlTerrain>();

    auto &rScnRender    = rFB.data_get< ACtxSceneRender >    (topData, idScnRender);
    auto &rScnRenderGl  = rFB.data_get< ACtxSceneRenderGL >  (topData, idScnRenderGl);
    auto &rRenderGl     = rFB.data_get< RenderGL >           (topData, magnum.di.renderGl);

    Session out;
    auto const [idDrawTerrainGl] = out.acquire_data<1>(topData);
    auto &rDrawTerrainGl = rFB.data_emplace< ACtxDrawTerrainGL >(topData, idDrawTerrainGl);

    rDrawTerrainGl.terrainMeshGl = rRenderGl.m_meshIds.create();
    rRenderGl.m_meshGl.emplace(rDrawTerrainGl.terrainMeshGl, Magnum::GL::Mesh{Corrade::NoCreate});

    rFB.task()
        .name       ("Sync terrainMeshGl to entities with terrainMesh")
        .run_on     ({scene.plRdr.entMeshDirty(UseOrRun)})
        .sync_with  ({scene.plRdr.mesh(Ready), scene.plRdr.entMesh(Ready), magnum.pl.meshGL(Ready), magnum.pl.entMeshGL(Modify), scene.plRdr.drawEntResized(Done)})
        .push_to    (out.m_tasks)
        .args       ({              idDrawTerrainGl,             idTerrain,      idScnRender,                   idScnRenderGl,          magnum.di.renderGl })
        .func([] (ACtxDrawTerrainGL& rDrawTerrainGl, ACtxTerrain& rTerrain, ACtxSceneRender& rScnRender, ACtxSceneRenderGL& rScnRenderGl, RenderGL& rRenderGl) noexcept
    {
        for (DrawEnt const drawEnt : rScnRender.m_meshDirty)
        {
            ACompMeshGl         &rEntMeshGl   = rScnRenderGl.m_meshId[drawEnt];
            MeshIdOwner_t const &entMeshScnId = rScnRender.m_mesh[drawEnt];

            if (entMeshScnId == rTerrain.terrainMesh)
            {
                rScnRenderGl.m_meshId[drawEnt] = ACompMeshGl{
                        .m_scnId = rTerrain.terrainMesh,
                        .m_glId  = rDrawTerrainGl.terrainMeshGl };
            }
        }
    });

    rFB.task()
        .name       ("Resync terrainMeshGl to entities with terrainMesh")
        .run_on     ({windowApp.pl.resync(Run)})
        .sync_with  ({scene.plRdr.mesh(Ready), magnum.pl.meshGL(Ready), magnum.pl.entMeshGL(Modify), scene.plRdr.drawEntResized(Done)})
        .push_to    (out.m_tasks)
        .args       ({              idDrawTerrainGl,             idTerrain,                 idScnRender,              idScnRenderGl,          magnum.di.renderGl })
        .func([] (ACtxDrawTerrainGL& rDrawTerrainGl, ACtxTerrain& rTerrain, ACtxSceneRender& rScnRender, ACtxSceneRenderGL& rScnRenderGl, RenderGL& rRenderGl) noexcept
    {
        for (DrawEnt const drawEnt : rScnRender.m_drawIds)
        {
            MeshIdOwner_t const &entMeshScnId = rScnRender.m_mesh[drawEnt];

            if (entMeshScnId == rTerrain.terrainMesh)
            {
                rScnRenderGl.m_meshId[drawEnt] = ACompMeshGl{
                        .m_scnId = rTerrain.terrainMesh,
                        .m_glId  = rDrawTerrainGl.terrainMeshGl };
            }
        }
    });

    rFB.task()
        .name       ("Update terrain mesh GPU buffer data")
        .run_on     ({windowApp.pl.sync(Run)})
        .sync_with  ({tgTrn.chunkMesh(Ready)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender,             idGroupFwd,                         idScnRenderGl,          magnum.di.renderGl,                   idDrawTerrainGl,            idTerrain})
        .func([] (ACtxSceneRender& rScnRender, RenderGroup& rGroupFwd, ACtxSceneRenderGL const& rScnRenderGl, RenderGL& rRenderGl, ACtxDrawTerrainGL& rDrawTerrainGl, ACtxTerrain& rTerrain) noexcept
    {
        if ( ! rDrawTerrainGl.enabled )
        {
            rDrawTerrainGl.enabled = true;

            rDrawTerrainGl.indxBufGL = Magnum::GL::Buffer{};
            rDrawTerrainGl.vrtxBufGL = Magnum::GL::Buffer{};

            Magnum::GL::Mesh &rMesh = rRenderGl.m_meshGl.get(rDrawTerrainGl.terrainMeshGl);

            rMesh = Mesh{Magnum::GL::MeshPrimitive::Triangles};

            auto const &posFormat = rTerrain.chunkGeom.vbufPositions;
            auto const &nrmFormat = rTerrain.chunkGeom.vbufNormals;


            rMesh.addVertexBuffer(rDrawTerrainGl.vrtxBufGL, GLintptr(posFormat.offset), GLsizei(posFormat.stride - sizeof(Vector3u)), Magnum::Shaders::GenericGL3D::Position{})
                 .addVertexBuffer(rDrawTerrainGl.vrtxBufGL, GLintptr(nrmFormat.offset), GLsizei(nrmFormat.stride - sizeof(Vector3u)), Magnum::Shaders::GenericGL3D::Normal{})
                 .setIndexBuffer(rDrawTerrainGl.indxBufGL, 0, Magnum::MeshIndexType::UnsignedInt)
                 .setCount(Magnum::Int(3*rTerrain.chunkInfo.faceTotal)); // 3 vertices in each triangle
        }

        auto const indxBuffer = arrayCast<unsigned char const>(rTerrain.chunkGeom.indxBuffer);
        auto const vrtxBuffer = arrayView<std::byte const>(rTerrain.chunkGeom.vrtxBuffer);

        // There's faster ways to sync the buffer, but keeping it simple for now

        // see "Buffer re-specification" in
        // https://www.khronos.org/opengl/wiki/Buffer_Object_Streaming

        rDrawTerrainGl.indxBufGL.setData({nullptr, indxBuffer.size()});
        rDrawTerrainGl.indxBufGL.setData(indxBuffer);

        rDrawTerrainGl.vrtxBufGL.setData({nullptr, vrtxBuffer.size()});
        rDrawTerrainGl.vrtxBufGL.setData(vrtxBuffer);
    });

    return out;
} // setup_terrain_draw_magnum
#endif

} // namespace testapp::scenes

