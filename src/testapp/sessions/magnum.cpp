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
    auto const tgCS     = commonScene   .get_pipelines< PlCommonScene >();
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

    rCamera.m_far = 1u << 24;
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
        for (auto const drawEntInt : rScnRender.m_drawIds.bitview().zeros())
        {
            SysRenderGL::sync_drawent_texture(
                    DrawEnt(drawEntInt),
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
        for (auto const drawEntInt : rScnRender.m_drawIds.bitview().zeros())
        {
            SysRenderGL::sync_drawent_mesh(
                    DrawEnt(drawEntInt),
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
        .name       ("Calculate draw transforms")
        .run_on     ({tgScnRdr.render(Run)})
        .sync_with  ({tgCS.hierarchy(Ready), tgCS.transform(Ready), tgCS.activeEnt(Ready), tgScnRdr.drawTransforms(Modify_), tgScnRdr.drawEnt(Ready), tgScnRdr.drawEntResized(Done), tgCS.activeEntResized(Done)})
        .push_to    (out.m_tasks)
        .args       ({            idBasic,                   idDrawing,                 idScnRender,                   idScnRenderGl })
        .func([] (ACtxBasic const& rBasic, ACtxDrawing const& rDrawing, ACtxSceneRender& rScnRender, ACtxSceneRenderGL& rScnRenderGl) noexcept
    {
        auto rootChildren = SysSceneGraph::children(rBasic.m_scnGraph);
        SysRender::update_draw_transforms(
                rBasic      .m_scnGraph,
                rScnRender  .m_activeToDraw,
                rBasic      .m_transform,
                rScnRender  .m_drawTransform,
                rScnRender  .m_needDrawTf,
                rootChildren.begin(),
                rootChildren.end());
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
            rGroup.m_entities.remove(drawEnt);
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

    auto &rScnRender    = top_get< ACtxSceneRender >    (topData, idScnRender);
    auto &rScnRenderGl  = top_get< ACtxSceneRenderGL >  (topData, idScnRenderGl);
    auto &rRenderGl     = top_get< RenderGL >           (topData, idRenderGl);

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_SHADER_VISUALIZER)

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
        .sync_with  ({tgScnRdr.groupEnts(Modify), tgScnRdr.group(Modify), tgScnRdr.materialDirty(UseOrRun)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender,             idGroupFwd,                        idDrawShVisual})
        .func([] (ACtxSceneRender& rScnRender, RenderGroup& rGroupFwd, ACtxDrawMeshVisualizer& rDrawShVisual) noexcept
    {
        Material const &rMat = rScnRender.m_materials[rDrawShVisual.m_materialId];
        sync_drawent_visualizer(rMat.m_dirty.begin(), rMat.m_dirty.end(), rMat.m_ents, rGroupFwd.m_entities, rDrawShVisual);
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
        for (auto const drawEntInt : rMat.m_ents.ones())
        {
            sync_drawent_visualizer(DrawEnt(drawEntInt), rMat.m_ents, rGroupFwd.m_entities, rDrawShVisual);
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

    auto &rScnRender    = top_get< ACtxSceneRender >    (topData, idScnRender);
    auto &rScnRenderGl  = top_get< ACtxSceneRenderGL >  (topData, idScnRenderGl);
    auto &rRenderGl     = top_get< RenderGL >           (topData, idRenderGl);

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_SHADER_FLAT)
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
            .pStorageOpaque = &rGroupFwd.m_entities,
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
        .sync_with  ({tgScnRdr.groupEnts(Modify), tgScnRdr.group(Modify)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender,             idGroupFwd,                         idScnRenderGl,              idDrawShFlat})
        .func([] (ACtxSceneRender& rScnRender, RenderGroup& rGroupFwd, ACtxSceneRenderGL const& rScnRenderGl, ACtxDrawFlat& rDrawShFlat) noexcept
    {
        Material const &rMat = rScnRender.m_materials[rDrawShFlat.materialId];
        for (auto const drawEntInt : rMat.m_ents.ones())
        {
            sync_drawent_flat(DrawEnt(drawEntInt),
            {
                .hasMaterial    = rMat.m_ents,
                .pStorageOpaque = &rGroupFwd.m_entities,
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

    auto &rScnRender    = top_get< ACtxSceneRender >    (topData, idScnRender);
    auto &rScnRenderGl  = top_get< ACtxSceneRenderGL >  (topData, idScnRenderGl);
    auto &rRenderGl     = top_get< RenderGL >           (topData, idRenderGl);

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_SHADER_PHONG)
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
        .sync_with  ({tgScnRdr.groupEnts(Modify), tgScnRdr.group(Modify), tgScnRdr.materialDirty(UseOrRun)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender,             idGroupFwd,                         idScnRenderGl,               idDrawShPhong})
        .func([] (ACtxSceneRender& rScnRender, RenderGroup& rGroupFwd, ACtxSceneRenderGL const& rScnRenderGl, ACtxDrawPhong& rDrawShPhong) noexcept
    {
        Material const &rMat = rScnRender.m_materials[rDrawShPhong.materialId];
        sync_drawent_phong(rMat.m_dirty.begin(), rMat.m_dirty.end(),
        {
            .hasMaterial    = rMat.m_ents,
            .pStorageOpaque = &rGroupFwd.m_entities,
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
        .sync_with  ({tgScnRdr.groupEnts(Modify), tgScnRdr.group(Modify)})
        .push_to    (out.m_tasks)
        .args       ({            idScnRender,             idGroupFwd,                         idScnRenderGl,               idDrawShPhong})
        .func([] (ACtxSceneRender& rScnRender, RenderGroup& rGroupFwd, ACtxSceneRenderGL const& rScnRenderGl, ACtxDrawPhong& rDrawShPhong) noexcept
    {
        Material const &rMat = rScnRender.m_materials[rDrawShPhong.materialId];
        for (auto const drawEntInt : rMat.m_ents.ones())
        {
            sync_drawent_phong(DrawEnt(drawEntInt),
            {
                .hasMaterial    = rMat.m_ents,
                .pStorageOpaque = &rGroupFwd.m_entities,
                .opaque         = rScnRender.m_opaque,
                .transparent    = rScnRender.m_transparent,
                .diffuse        = rScnRenderGl.m_diffuseTexId,
                .rData          = rDrawShPhong
            });
        }
    });

    return out;
} // setup_shader_phong



#if 0

struct IndicatorMesh
{
    Magnum::Color4 m_color;
    MeshIdOwner_t m_mesh;
};


Session setup_thrust_indicators(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              magnum,
        Session const&              commonScene,
        Session const&              parts,
        Session const&              signalsFloat,
        Session const&              scnRender,
        Session const&              cameraCtrl,
        Session const&              shFlat,
        TopDataId const             idResources,
        PkgId const                 pkg)
{


    using namespace osp::link;
    using adera::gc_mtMagicRocket;
    using adera::ports_magicrocket::gc_throttleIn;
    using adera::ports_magicrocket::gc_multiplierIn;

    static constexpr float indicatorScale = 0.0001f;

    OSP_SESSION_UNPACK_TAGS(magnum,         TESTAPP_APP_MAGNUM);
    OSP_SESSION_UNPACK_DATA(magnum,         TESTAPP_APP_MAGNUM);
    OSP_SESSION_UNPACK_TAGS(commonScene,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(commonScene,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(parts,          TESTAPP_PARTS);
    OSP_SESSION_UNPACK_DATA(parts,          TESTAPP_PARTS);
    OSP_SESSION_UNPACK_DATA(signalsFloat,   TESTAPP_SIGNALS_FLOAT)
    OSP_SESSION_UNPACK_TAGS(signalsFloat,   TESTAPP_SIGNALS_FLOAT);
    OSP_SESSION_UNPACK_TAGS(scnRender,      TESTAPP_COMMON_RENDERER);
    OSP_SESSION_UNPACK_DATA(scnRender,      TESTAPP_COMMON_RENDERER);

    OSP_SESSION_UNPACK_DATA(cameraCtrl,     TESTAPP_CAMERA_CTRL);
    OSP_SESSION_UNPACK_TAGS(cameraCtrl,     TESTAPP_CAMERA_CTRL);
    OSP_SESSION_UNPACK_DATA(shFlat,         TESTAPP_SHADER_FLAT);

    auto &rResources    = top_get< Resources >      (topData, idResources);
    auto &rBasic        = top_get< ACtxBasic >      (topData, idBasic);
    auto &rActiveIds    = top_get< ActiveReg_t >    (topData, idActiveIds);
    auto &rDrawing      = top_get< ACtxDrawing >    (topData, idDrawing);
    auto &rDrawingRes   = top_get< ACtxDrawingRes > (topData, idDrawingRes);

    Session thrustIndicator;
#if 0 // RENDERENT
    OSP_SESSION_ACQUIRE_DATA(thrustIndicator, topData, TESTAPP_INDICATOR);
    thrustIndicator.m_tgCleanupEvt = tgCleanupMagnumEvt;

    auto &rIndicator   = top_emplace<IndicatorMesh>(topData, idIndicator);
    rIndicator.m_color = { 1.0f, 0.0f, 0.0f, 1.0f };
    rIndicator.m_mesh  = SysRender::add_drawable_mesh(rDrawing, rDrawingRes, rResources, pkg, "cone");

    thrustIndicator.task() = rBuilder.task().assign({tgRenderEvt, tgGlUse, tgDrawTransformReq, tgBindFboReq, tgFwdRenderMod, tgCamCtrlReq}).data(
            "Render cursor",
            TopDataIds_t{          idRenderGl,              idCamera,                      idDrawingRes,                         idScnRender,                 idScnParts,                             idSigValFloat,              idDrawShFlat,                            idCamCtrl,               idIndicator},
            wrap_args([] (RenderGL& rRenderGl, Camera const& rCamera, ACtxDrawingRes const& rDrawingRes, ACtxSceneRenderGL const& rScnRender, ACtxParts const& rScnParts, SignalValues_t<float> const& rSigValFloat, ACtxDrawFlat& rDrawShFlat, ACtxCameraController const& rCamCtrl, IndicatorMesh& rIndicator) noexcept
    {

        ResId const     indicatorResId      = rDrawingRes.m_meshToRes.at(rIndicator.m_mesh.value());
        MeshGlId const  indicatorMeshGlId   = rRenderGl.m_resToMesh.at(indicatorResId);
        Mesh&           rIndicatorMeshGl    = rRenderGl.m_meshGl.get(indicatorMeshGlId);

        ViewProjMatrix viewProj{rCamera.m_transform.inverted(), rCamera.perspective()};

        //auto const matrix = viewProj.m_viewProj * Matrix4::translation(rCamCtrl.m_target.value());

        PerMachType const& rockets = rScnParts.m_machines.m_perType[gc_mtMagicRocket];
        Nodes const& floats = rScnParts.m_nodePerType[gc_ntSigFloat];

        for (MachLocalId const localId : rockets.m_localIds.bitview().zeros())
        {
            MachAnyId const anyId           = rockets.m_localToAny[localId];
            PartId const    part            = rScnParts.m_machineToPart[anyId];
            ActiveEnt const partEnt         = rScnParts.m_partToActive[part];

            auto const&     portSpan        = floats.m_machToNode[anyId];
            NodeId const    throttleIn      = connected_node(portSpan, gc_throttleIn.m_port);
            NodeId const    multiplierIn    = connected_node(portSpan, gc_multiplierIn.m_port);

            float const     throttle        = std::clamp(rSigValFloat[throttleIn], 0.0f, 1.0f);
            float const     multiplier      = rSigValFloat[multiplierIn];
            float const     thrustMag       = throttle * multiplier;

            if (thrustMag == 0.0f)
            {
                continue;
            }

            Matrix4 const& rocketDrawTf = rScnRender.m_drawTransform.get(partEnt);

            auto const& matrix = Matrix4::from(rocketDrawTf.rotationNormalized(), rocketDrawTf.translation())
                               * Matrix4::scaling({1.0f, 1.0f, thrustMag * indicatorScale})
                               * Matrix4::translation({0.0f, 0.0f, -1.0f})
                               * Matrix4::scaling({0.2f, 0.2f, 1.0f});

            rDrawShFlat.m_shaderUntextured
                .setColor(rIndicator.m_color)
                .setTransformationProjectionMatrix(viewProj.m_viewProj * matrix)
                .draw(rIndicatorMeshGl);
        }
    }));

    thrustIndicator.task() = rBuilder.task().assign({tgCleanupMagnumEvt}).data(
            "Clean up thrust indicator resource owners",
            TopDataIds_t{             idDrawing,               idIndicator},
            wrap_args([] (ACtxDrawing& rDrawing, IndicatorMesh& rIndicator) noexcept
    {
        rDrawing.m_meshRefCounts.ref_release(std::move(rIndicator.m_mesh));
    }));

    // Draw transforms in part entities are required for drawing indicators.
    // This solution of adding them here is a bit janky but it works.

    thrustIndicator.task() = rBuilder.task().assign({tgSyncEvt, tgPartReq}).data(
            "Add draw transforms to rocket entities",
            TopDataIds_t{                 idScnParts,                   idScnRender},
            wrap_args([] (ACtxParts const& rScnParts, ACtxSceneRenderGL& rScnRender) noexcept
    {
        for (PartId const part : rScnParts.m_partDirty)
        {
            // TODO: maybe first check if the part contains any rocket machines

            ActiveEnt const ent = rScnParts.m_partToActive[part];
            if ( ! rScnRender.m_drawTransform.contains(ent) )
            {
                rScnRender.m_drawTransform.emplace(ent);
            }
        }
    }));

    // Called when scene is reopened
    thrustIndicator.task() = rBuilder.task().assign({tgResyncEvt, tgPartReq}).data(
            "Add draw transforms to all rocket entities",
            TopDataIds_t{                 idScnParts,                   idScnRender},
            wrap_args([] (ACtxParts const& rScnParts, ACtxSceneRenderGL& rScnRender) noexcept
    {
        for (PartId const part : rScnParts.m_partIds.bitview().zeros())
        {
            ActiveEnt const ent = rScnParts.m_partToActive[part];
            if ( ! rScnRender.m_drawTransform.contains(ent) )
            {
                rScnRender.m_drawTransform.emplace(ent);
            }
        }
    }));
#endif
    return thrustIndicator;
}

#endif

} // namespace testapp::scenes
