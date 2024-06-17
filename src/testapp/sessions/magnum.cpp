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
#include <adera/drawing_gl/flat_shader.h>
#include <adera/drawing_gl/phong_shader.h>
#include <adera/drawing_gl/visualizer_shader.h>
#include <osp/activescene/basic_fn.h>
#include <osp/drawing/drawing.h>
#include <osp/drawing_gl/rendergl.h>
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
using namespace osp;

using osp::input::UserInputHandler;

using Magnum::GL::Mesh;

namespace testapp::scenes
{


Session setup_magnum(
        TopTaskBuilder&                 rBuilder,
        ArrayView<entt::any> const      topData,
        Session const&                  application,
        Session const&                  windowApp,
        MagnumApplication::Arguments    args)
{
    OSP_DECLARE_GET_DATA_IDS(application, TESTAPP_DATA_APPLICATION);
    OSP_DECLARE_GET_DATA_IDS(windowApp,   TESTAPP_DATA_WINDOW_APP);
    auto const tgWin    = windowApp     .get_pipelines< PlWindowApp >();

    auto& rUserInput = top_get<UserInputHandler>(topData, idUserInput);
    config_controls(rUserInput);

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_MAGNUM);
    auto const tgMgn = out.create_pipelines<PlMagnum>(rBuilder);

    rBuilder.pipeline(tgMgn.meshGL)         .parent(tgWin.sync);
    rBuilder.pipeline(tgMgn.textureGL)      .parent(tgWin.sync);
    rBuilder.pipeline(tgMgn.entMeshGL)      .parent(tgWin.sync);
    rBuilder.pipeline(tgMgn.entTextureGL)   .parent(tgWin.sync);

    // Order-dependent; MagnumApplication construction starts OpenGL context, needed by RenderGL
    /* unused */      top_emplace<MagnumApplication>(topData, idActiveApp, args, rUserInput);
    auto &rRenderGl = top_emplace<RenderGL>         (topData, idRenderGl);

    SysRenderGL::setup_context(rRenderGl);

    rBuilder.task()
        .name       ("Clean up Magnum renderer")
        .run_on     ({tgWin.cleanup(Run_)})
        .push_to    (out.m_tasks)
        .args       ({      idResources,          idRenderGl})
        .func([] (Resources& rResources, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::clear_resource_owners(rRenderGl, rResources);
        rRenderGl = {}; // Needs the OpenGL thread for destruction
    });

    return out;
} // setup_magnum




Session setup_magnum_scene(
        TopTaskBuilder&             rBuilder,
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

    auto const tgWin    = windowApp     .get_pipelines< PlWindowApp >();
    auto const tgMgn    = magnum        .get_pipelines< PlMagnum >();
    auto const tgScnRdr = sceneRenderer .get_pipelines< PlSceneRenderer >();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_MAGNUM_SCENE);
    auto const tgMgnScn = out.create_pipelines<PlMagnumScene>(rBuilder);

    rBuilder.pipeline(tgMgnScn.fbo)             .parent(tgScnRdr.render);
    rBuilder.pipeline(tgMgnScn.camera)          .parent(tgScnRdr.render);

    top_emplace< ACtxSceneRenderGL >    (topData, idScnRenderGl);
    top_emplace< RenderGroup >          (topData, idGroupFwd);

    auto &rCamera = top_emplace< Camera >(topData, idCamera);

    rCamera.m_far = 100000000.0f;
    rCamera.m_near = 1.0f;
    rCamera.m_fov = Magnum::Deg(45.0f);

    rBuilder.task()
        .name       ("Resize ACtxSceneRenderGL (OpenGL) to fit all DrawEnts")
        .run_on     ({tgScnRdr.drawEntResized(Run)})
        .sync_with  ({})
        .push_to    (out.m_tasks)
        .args       ({ idScnRender, idScnRenderGl })
        .func       ([] (ACtxSceneRender const& rScnRender, ACtxSceneRenderGL& rScnRenderGl) noexcept
    {
        std::size_t const capacity = rScnRender.m_drawIds.capacity();
        rScnRenderGl.m_diffuseTexId   .resize(capacity);
        rScnRenderGl.m_meshId         .resize(capacity);
    });

    rBuilder.task()
        .name       ("Compile Resource Meshes to GL")
        .run_on     ({tgScnRdr.meshResDirty(UseOrRun)})
        .sync_with  ({tgScnRdr.mesh(Ready), tgMgn.meshGL(New), tgScnRdr.entMeshDirty(UseOrRun)})
        .push_to    (out.m_tasks)
        .args       ({                 idDrawingRes,                idResources,          idRenderGl })
        .func([] (ACtxDrawingRes const& rDrawingRes, osp::Resources& rResources, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::compile_resource_meshes(rDrawingRes, rResources, rRenderGl);
    });

    rBuilder.task()
        .name       ("Compile Resource Textures to GL")
        .run_on     ({tgScnRdr.textureResDirty(UseOrRun)})
        .sync_with  ({tgScnRdr.texture(Ready), tgMgn.textureGL(New)})
        .push_to    (out.m_tasks)
        .args       ({                 idDrawingRes,                idResources,          idRenderGl })
        .func([] (ACtxDrawingRes const& rDrawingRes, osp::Resources& rResources, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::compile_resource_textures(rDrawingRes, rResources, rRenderGl);
    });

    rBuilder.task()
        .name       ("Sync GL textures to entities with scene textures")
        .run_on     ({tgScnRdr.entTextureDirty(UseOrRun)})
        .sync_with  ({tgScnRdr.texture(Ready), tgScnRdr.entTexture(Ready), tgMgn.textureGL(Ready), tgMgn.entTextureGL(Modify), tgScnRdr.drawEntResized(Done)})
        .push_to    (out.m_tasks)
        .args       ({        idDrawing,                idDrawingRes,                 idScnRender,                   idScnRenderGl,          idRenderGl })
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

    rBuilder.task()
        .name       ("Resync GL textures")
        .run_on     ({tgWin.resync(Run)})
        .sync_with  ({tgScnRdr.texture(Ready), tgMgn.textureGL(Ready), tgMgn.entTextureGL(Modify), tgScnRdr.drawEntResized(Done)})
        .push_to    (out.m_tasks)
        .args       ({           idDrawingRes,                 idScnRender,                   idScnRenderGl,          idRenderGl })
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

    rBuilder.task()
        .name       ("Sync GL meshes to entities with scene meshes")
        .run_on     ({tgScnRdr.entMeshDirty(UseOrRun)})
        .sync_with  ({tgScnRdr.mesh(Ready), tgScnRdr.entMesh(Ready), tgMgn.meshGL(Ready), tgMgn.entMeshGL(Modify), tgScnRdr.drawEntResized(Done)})
        .push_to    (out.m_tasks)
        .args       ({           idDrawingRes,                 idScnRender,                   idScnRenderGl,          idRenderGl })
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

    rBuilder.task()
        .name       ("Resync GL meshes")
        .run_on     ({tgWin.resync(Run)})
        .sync_with  ({tgScnRdr.mesh(Ready), tgMgn.meshGL(Ready), tgMgn.entMeshGL(Modify), tgScnRdr.drawEntResized(Done)})
        .push_to    (out.m_tasks)
        .args       ({           idDrawingRes,                 idScnRender,                   idScnRenderGl,          idRenderGl })
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

    rBuilder.task()
        .name       ("Bind and display off-screen FBO")
        .run_on     ({tgScnRdr.render(Run)})
        .sync_with  ({tgMgnScn.fbo(EStgFBO::Bind)})
        .push_to    (out.m_tasks)
        .args       ({              idDrawing,          idRenderGl,                   idGroupFwd,              idCamera })
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

    rBuilder.task()
        .name       ("Render Entities")
        .run_on     ({tgScnRdr.render(Run)})
        .sync_with  ({tgScnRdr.group(Ready), tgScnRdr.groupEnts(Ready), tgMgnScn.camera(Ready), tgScnRdr.drawTransforms(UseOrRun), tgScnRdr.entMesh(Ready), tgScnRdr.entTexture(Ready),
                      tgMgn.entMeshGL(Ready), tgMgn.entTextureGL(Ready),
                      tgScnRdr.drawEnt(Ready)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender,          idRenderGl,                   idGroupFwd,              idCamera })
        .func([] (ACtxSceneRender& rScnRender, RenderGL& rRenderGl, RenderGroup const& rGroupFwd, Camera const& rCamera, WorkerContext ctx) noexcept
    {
        ViewProjMatrix viewProj{rCamera.m_transform.inverted(), rCamera.perspective()};

        // Forward Render fwd_opaque group to FBO
        SysRenderGL::render_opaque(rGroupFwd, rScnRender.m_visible, viewProj);
    });

    rBuilder.task()
        .name       ("Delete entities from render groups")
        .run_on     ({tgScnRdr.drawEntDelete(UseOrRun)})
        .sync_with  ({tgScnRdr.groupEnts(Delete)})
        .push_to    (out.m_tasks)
        .args       ({              idDrawing,             idGroupFwd,                 idDrawEntDel })
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
        TopTaskBuilder&             rBuilder,
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
    auto const tgWin    = windowApp     .get_pipelines< PlWindowApp >();
    auto const tgScnRdr = sceneRenderer .get_pipelines< PlSceneRenderer >();
    auto const tgMgn    = magnum        .get_pipelines< PlMagnum >();

    auto &rScnRender    = top_get< ACtxSceneRender >    (topData, idScnRender);
    auto &rScnRenderGl  = top_get< ACtxSceneRenderGL >  (topData, idScnRenderGl);
    auto &rRenderGl     = top_get< RenderGL >           (topData, idRenderGl);

    Session out;
    auto const [idDrawShVisual] = out.acquire_data<1>(topData);

    auto &rDrawVisual = top_emplace< ACtxDrawMeshVisualizer >(topData, idDrawShVisual);

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

    rBuilder.task()
        .name       ("Sync MeshVisualizer shader DrawEnts")
        .run_on     ({tgWin.sync(Run)})
        .sync_with  ({tgScnRdr.materialDirty(UseOrRun), tgMgn.textureGL(Ready), tgScnRdr.groupEnts(Modify)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender,             idGroupFwd,                        idDrawShVisual})
        .func([] (ACtxSceneRender& rScnRender, RenderGroup& rGroupFwd, ACtxDrawMeshVisualizer& rDrawShVisual) noexcept
    {
        Material const &rMat = rScnRender.m_materials[rDrawShVisual.m_materialId];
        sync_drawent_visualizer(rMat.m_dirty.begin(), rMat.m_dirty.end(), rMat.m_ents, rGroupFwd.entities, rDrawShVisual);
    });

    rBuilder.task()
        .name       ("Resync MeshVisualizer shader DrawEnts")
        .run_on     ({tgWin.resync(Run)})
        .sync_with  ({tgScnRdr.groupEnts(Modify), tgScnRdr.group(Modify)})
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
        TopTaskBuilder&             rBuilder,
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
    auto const tgWin    = windowApp     .get_pipelines< PlWindowApp >();
    auto const tgScnRdr = sceneRenderer .get_pipelines< PlSceneRenderer >();
    auto const tgMgn    = magnum        .get_pipelines< PlMagnum >();

    auto &rScnRender    = top_get< ACtxSceneRender >    (topData, idScnRender);
    auto &rScnRenderGl  = top_get< ACtxSceneRenderGL >  (topData, idScnRenderGl);
    auto &rRenderGl     = top_get< RenderGL >           (topData, idRenderGl);

    Session out;
    auto const [idDrawShFlat] = out.acquire_data<1>(topData);

    auto &rDrawFlat = top_emplace< ACtxDrawFlat >(topData, idDrawShFlat);

    rDrawFlat.shaderDiffuse       = FlatGL3D{FlatGL3D::Configuration{}.setFlags(FlatGL3D::Flag::Textured)};
    rDrawFlat.shaderUntextured    = FlatGL3D{FlatGL3D::Configuration{}};
    rDrawFlat.materialId          = materialId;
    rDrawFlat.assign_pointers(rScnRender, rScnRenderGl, rRenderGl);

    if (materialId == lgrn::id_null<MaterialId>())
    {
        return out;
    }

    rBuilder.task()
        .name       ("Sync Flat shader DrawEnts")
        .run_on     ({tgWin.sync(Run)})
        .sync_with  ({tgScnRdr.groupEnts(Modify), tgScnRdr.group(Modify), tgScnRdr.materialDirty(UseOrRun)})
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

    rBuilder.task()
        .name       ("Resync Flat shader DrawEnts")
        .run_on     ({tgWin.resync(Run)})
        .sync_with  ({tgScnRdr.materialDirty(UseOrRun), tgMgn.textureGL(Ready), tgScnRdr.groupEnts(Modify), tgScnRdr.group(Modify)})
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
        TopTaskBuilder&             rBuilder,
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
    auto const tgWin    = windowApp     .get_pipelines< PlWindowApp >();
    auto const tgScnRdr = sceneRenderer .get_pipelines< PlSceneRenderer >();
    auto const tgMgn    = magnum        .get_pipelines< PlMagnum >();

    auto &rScnRender    = top_get< ACtxSceneRender >    (topData, idScnRender);
    auto &rScnRenderGl  = top_get< ACtxSceneRenderGL >  (topData, idScnRenderGl);
    auto &rRenderGl     = top_get< RenderGL >           (topData, idRenderGl);

    Session out;
    auto const [idDrawShPhong] = out.acquire_data<1>(topData);
    auto &rDrawPhong = top_emplace< ACtxDrawPhong >(topData, idDrawShPhong);

    auto const texturedFlags    = PhongGL::Flag::DiffuseTexture | PhongGL::Flag::AlphaMask | PhongGL::Flag::AmbientTexture;
    rDrawPhong.shaderDiffuse    = PhongGL{PhongGL::Configuration{}.setFlags(texturedFlags).setLightCount(2)};
    rDrawPhong.shaderUntextured = PhongGL{PhongGL::Configuration{}.setLightCount(2)};
    rDrawPhong.materialId       = materialId;
    rDrawPhong.assign_pointers(rScnRender, rScnRenderGl, rRenderGl);

    if (materialId == lgrn::id_null<MaterialId>())
    {
        return out;
    }

    rBuilder.task()
        .name       ("Sync Phong shader DrawEnts")
        .run_on     ({tgWin.sync(Run)})
        .sync_with  ({tgScnRdr.materialDirty(UseOrRun), tgMgn.entTextureGL(Ready), tgScnRdr.groupEnts(Modify), tgScnRdr.group(Modify)})
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

    rBuilder.task()
        .name       ("Resync Phong shader DrawEnts")
        .run_on     ({tgWin.resync(Run)})
        .sync_with  ({tgScnRdr.materialDirty(UseOrRun), tgMgn.entTextureGL(Ready), tgScnRdr.groupEnts(Modify), tgScnRdr.group(Modify)})
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
        TopTaskBuilder              &rBuilder,
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
    auto const tgWin    = windowApp     .get_pipelines< PlWindowApp >();
    auto const tgScnRdr = sceneRenderer .get_pipelines< PlSceneRenderer >();
    auto const tgMgn    = magnum        .get_pipelines< PlMagnum >();
    auto const tgTrn    = terrain       .get_pipelines<PlTerrain>();

    auto &rScnRender    = top_get< ACtxSceneRender >    (topData, idScnRender);
    auto &rScnRenderGl  = top_get< ACtxSceneRenderGL >  (topData, idScnRenderGl);
    auto &rRenderGl     = top_get< RenderGL >           (topData, idRenderGl);

    Session out;
    auto const [idDrawTerrainGl] = out.acquire_data<1>(topData);
    auto &rDrawTerrainGl = top_emplace< ACtxDrawTerrainGL >(topData, idDrawTerrainGl);

    rDrawTerrainGl.terrainMeshGl = rRenderGl.m_meshIds.create();
    rRenderGl.m_meshGl.emplace(rDrawTerrainGl.terrainMeshGl, Magnum::GL::Mesh{Corrade::NoCreate});

    rBuilder.task()
        .name       ("Sync terrainMeshGl to entities with terrainMesh")
        .run_on     ({tgScnRdr.entMeshDirty(UseOrRun)})
        .sync_with  ({tgScnRdr.mesh(Ready), tgScnRdr.entMesh(Ready), tgMgn.meshGL(Ready), tgMgn.entMeshGL(Modify), tgScnRdr.drawEntResized(Done)})
        .push_to    (out.m_tasks)
        .args       ({              idDrawTerrainGl,             idTerrain,      idScnRender,                   idScnRenderGl,          idRenderGl })
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

    rBuilder.task()
        .name       ("Resync terrainMeshGl to entities with terrainMesh")
        .run_on     ({tgWin.resync(Run)})
        .sync_with  ({tgScnRdr.mesh(Ready), tgMgn.meshGL(Ready), tgMgn.entMeshGL(Modify), tgScnRdr.drawEntResized(Done)})
        .push_to    (out.m_tasks)
        .args       ({              idDrawTerrainGl,             idTerrain,                 idScnRender,              idScnRenderGl,          idRenderGl })
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

    rBuilder.task()
        .name       ("Update terrain mesh GPU buffer data")
        .run_on     ({tgWin.sync(Run)})
        .sync_with  ({tgTrn.chunkMesh(Ready)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender,             idGroupFwd,                         idScnRenderGl,          idRenderGl,                   idDrawTerrainGl,            idTerrain})
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

            rMesh.addVertexBuffer(rDrawTerrainGl.vrtxBufGL, posFormat.offset, posFormat.stride - sizeof(Vector3u), Magnum::Shaders::GenericGL3D::Position{})
                 .addVertexBuffer(rDrawTerrainGl.vrtxBufGL, nrmFormat.offset, nrmFormat.stride - sizeof(Vector3u), Magnum::Shaders::GenericGL3D::Normal{})
                 .setIndexBuffer(rDrawTerrainGl.indxBufGL, 0, Magnum::MeshIndexType::UnsignedInt)
                 .setCount(3*rTerrain.chunkInfo.faceTotal); // 3 vertices in each triangle
        }

        auto const indxBuffer = Corrade::Containers::arrayCast<unsigned char const>(rTerrain.chunkGeom.indxBuffer);
        auto const vrtxBuffer = arrayView<unsigned char const>(rTerrain.chunkGeom.vrtxBuffer);

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

} // namespace testapp::scenes
