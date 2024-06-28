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
#include "godot.h"

#include "flying_scene.h"
#include "osp/tasks/top_utils.h"
#include "render.h"
#include "testapp/identifiers.h"

#include <adera/drawing/CameraController.h>
#include <adera/machines/links.h>
#include <entt/core/fwd.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <osp/activescene/basic_fn.h>
#include <osp/drawing/drawing.h>
#include <osp/universe/coordinates.h>
#include <osp/universe/universe.h>
#include <testapp/sessions/common.h>

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

using namespace adera;
using namespace osp::active;
using namespace osp::draw;
using namespace osp::universe;
using namespace osp;

using osp::input::UserInputHandler;

namespace testapp::scenes
{

Session setup_godot(TopTaskBuilder            &rBuilder,
                    ArrayView<entt::any> const topData,
                    Session const             &application,
                    Session const             &windowApp)
{
    OSP_DECLARE_GET_DATA_IDS(application, TESTAPP_DATA_APPLICATION);
    OSP_DECLARE_GET_DATA_IDS(windowApp, TESTAPP_DATA_WINDOW_APP);
    auto const tgWin      = windowApp.get_pipelines<PlWindowApp>();

    auto      &rUserInput = top_get<UserInputHandler>(topData, idUserInput);
    // config_controls(rUserInput); //FIXME

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_MAGNUM);
    auto const tgMgn = out.create_pipelines<PlMagnum>(rBuilder);

    rBuilder.pipeline(tgMgn.meshGL).parent(tgWin.sync);
    rBuilder.pipeline(tgMgn.textureGL).parent(tgWin.sync);
    rBuilder.pipeline(tgMgn.entMeshGL).parent(tgWin.sync);
    rBuilder.pipeline(tgMgn.entTextureGL).parent(tgWin.sync);

    // Order-dependent; MagnumApplication construction starts OpenGL context, needed by RenderGL
    /* unused */ // top_emplace<MagnumApplication>(topData, idActiveApp, args, rUserInput);
    auto &rRenderGd = top_emplace<RenderGd>(topData, idRenderGl);

    godot::FlyingScene* mainApp = osp::top_get<godot::FlyingScene *>(topData, idActiveApp);
    rRenderGd.scenario = mainApp->get_main_scenario();
    rRenderGd.viewport = mainApp->get_main_viewport();

    rBuilder.task()
        .name("Clean up renderer")
        .run_on({ tgWin.cleanup(Run_) })
        .push_to(out.m_tasks)
        .args({ idResources, idRenderGl })
        .func([](Resources &rResources, RenderGd &rRenderGd) noexcept {
            SysRenderGd::clear_resource_owners(rRenderGd, rResources);
            rRenderGd = {}; // Needs the OpenGL thread for destruction
        });

    return out;
} // setup_magnum

Session setup_godot_scene(TopTaskBuilder            &rBuilder,
                          ArrayView<entt::any> const topData,
                          Session const             &application,
                          Session const             &windowApp,
                          Session const             &sceneRenderer,
                          Session const             &magnum,
                          Session const             &scene,
                          Session const             &commonScene)
{
    OSP_DECLARE_GET_DATA_IDS(application, TESTAPP_DATA_APPLICATION);
    OSP_DECLARE_GET_DATA_IDS(commonScene, TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(windowApp, TESTAPP_DATA_WINDOW_APP);
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer, TESTAPP_DATA_SCENE_RENDERER);
    OSP_DECLARE_GET_DATA_IDS(magnum, TESTAPP_DATA_MAGNUM);

    auto       rs       = godot::RenderingServer::get_singleton();

    auto const tgWin    = windowApp.get_pipelines<PlWindowApp>();
    auto const tgMgn    = magnum.get_pipelines<PlMagnum>();
    auto const tgScnRdr = sceneRenderer.get_pipelines<PlSceneRenderer>();

    Session    out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_MAGNUM_SCENE);
    auto const tgMgnScn = out.create_pipelines<PlMagnumScene>(rBuilder);

    rBuilder.pipeline(tgMgnScn.fbo).parent(tgScnRdr.render);
    rBuilder.pipeline(tgMgnScn.camera).parent(tgScnRdr.render);

    top_emplace<draw::ACtxSceneRenderGd>(topData, idScnRenderGl);
    top_emplace<osp::draw::RenderGroup>(topData, idGroupFwd);

    godot::RID &rCamera = top_emplace<godot::RID>(topData, idCamera);
    rCamera             = rs->camera_create();

    RenderGd &rRenderGd = top_get<RenderGd>(topData, idRenderGl);
    rs->viewport_attach_camera(rRenderGd.viewport, rCamera);

    rs->camera_set_perspective(rCamera, 45., 1.0f, 1u << 24);

    rBuilder.task()
        .name("Resize ACtxSceneRenderGd to fit all DrawEnts")
        .run_on({ tgScnRdr.drawEntResized(scenes::Run) })
        .sync_with({})
        .push_to(out.m_tasks)
        .args({ idScnRender, idScnRenderGl })
        .func([](osp::draw::ACtxSceneRender const &rScnRender,
                 draw::ACtxSceneRenderGd          &rScnRenderGl) noexcept {
            std::size_t const capacity = rScnRender.m_drawIds.capacity();
            rScnRenderGl.m_diffuseTexId.resize(capacity);
            rScnRenderGl.m_meshId.resize(capacity);
        });

    rBuilder.task()
        .name("Compile Resource Meshes to GL")
        .run_on({ tgScnRdr.meshResDirty(scenes::UseOrRun) })
        .sync_with({ tgScnRdr.mesh(scenes::Ready),
                     tgMgn.meshGL(scenes::New),
                     tgScnRdr.entMeshDirty(scenes::UseOrRun) })
        .push_to(out.m_tasks)
        .args({ idDrawingRes, idResources, idRenderGl })
        .func([](osp::draw::ACtxDrawingRes const &rDrawingRes,
                 osp::Resources                  &rResources,
                 osp::draw::RenderGd             &rRenderGd) noexcept {
            draw::SysRenderGd::compile_resource_meshes(rDrawingRes, rResources, rRenderGd);
        });

    rBuilder.task()
        .name("Compile Resource Textures to GL")
        .run_on({ tgScnRdr.textureResDirty(scenes::UseOrRun) })
        .sync_with({ tgScnRdr.texture(scenes::Ready), tgMgn.textureGL(scenes::New) })
        .push_to(out.m_tasks)
        .args({ idDrawingRes, idResources, idRenderGl })
        .func([](draw::ACtxDrawingRes const &rDrawingRes,
                 osp::Resources             &rResources,
                 draw::RenderGd             &rRenderGd) noexcept {
            draw::SysRenderGd::compile_resource_textures(rDrawingRes, rResources, rRenderGd);
        });

    rBuilder.task()
        .name("Sync GL textures to entities with scene textures")
        .run_on({ tgScnRdr.entTextureDirty(scenes::UseOrRun) })
        .sync_with({ tgScnRdr.texture(scenes::Ready),
                     tgScnRdr.entTexture(scenes::Ready),
                     tgMgn.textureGL(scenes::Ready),
                     tgMgn.entTextureGL(scenes::Modify),
                     tgScnRdr.drawEntResized(scenes::Done) })
        .push_to(out.m_tasks)
        .args({ idDrawing, idDrawingRes, idScnRender, idScnRenderGl, idRenderGl })
        .func([](draw::ACtxDrawing       &rDrawing,
                 draw::ACtxDrawingRes    &rDrawingRes,
                 draw::ACtxSceneRender   &rScnRender,
                 draw::ACtxSceneRenderGd &rScnRenderGl,
                 draw::RenderGd          &rRenderGl) noexcept {
            draw::SysRenderGd::sync_drawent_texture(rScnRender.m_diffuseDirty.begin(),
                                                    rScnRender.m_diffuseDirty.end(),
                                                    rScnRender.m_diffuseTex,
                                                    rDrawingRes.m_texToRes,
                                                    rScnRenderGl.m_diffuseTexId,
                                                    rRenderGl);
        });

    rBuilder.task()
        .name("Resync GL textures")
        .run_on({ tgWin.resync(scenes::Run) })
        .sync_with({ tgScnRdr.texture(scenes::Ready),
                     tgMgn.textureGL(scenes::Ready),
                     tgMgn.entTextureGL(scenes::Modify),
                     tgScnRdr.drawEntResized(scenes::Done) })
        .push_to(out.m_tasks)
        .args({ idDrawingRes, idScnRender, idScnRenderGl, idRenderGl })
        .func([](draw::ACtxDrawingRes    &rDrawingRes,
                 draw::ACtxSceneRender   &rScnRender,
                 draw::ACtxSceneRenderGd &rScnRenderGl,
                 draw::RenderGd          &rRenderGl) noexcept {
            for ( draw::DrawEnt const drawEnt : rScnRender.m_drawIds )
            {
                draw::SysRenderGd::sync_drawent_texture(drawEnt,
                                                        rScnRender.m_diffuseTex,
                                                        rDrawingRes.m_texToRes,
                                                        rScnRenderGl.m_diffuseTexId,
                                                        rRenderGl);
            }
        });

    rBuilder.task()
        .name("Sync GL meshes to entities with scene meshes")
        .run_on({ tgScnRdr.entMeshDirty(scenes::UseOrRun) })
        .sync_with({ tgScnRdr.mesh(scenes::Ready),
                     tgScnRdr.entMesh(scenes::Ready),
                     tgMgn.meshGL(scenes::Ready),
                     tgMgn.entMeshGL(scenes::Modify),
                     tgScnRdr.drawEntResized(scenes::Done) })
        .push_to(out.m_tasks)
        .args({ idDrawingRes, idScnRender, idScnRenderGl, idRenderGl })
        .func([](draw::ACtxDrawingRes    &rDrawingRes,
                 draw::ACtxSceneRender   &rScnRender,
                 draw::ACtxSceneRenderGd &rScnRenderGl,
                 draw::RenderGd          &rRenderGl) noexcept {
            draw::SysRenderGd::sync_drawent_mesh(rScnRender.m_meshDirty.begin(),
                                                 rScnRender.m_meshDirty.end(),
                                                 rScnRender.m_mesh,
                                                 rDrawingRes.m_meshToRes,
                                                 rScnRenderGl.m_meshId,
                                                 rRenderGl);
        });

    rBuilder.task()
        .name("Resync GL meshes")
        .run_on({ tgWin.resync(scenes::Run) })
        .sync_with({ tgScnRdr.mesh(scenes::Ready),
                     tgMgn.meshGL(scenes::Ready),
                     tgMgn.entMeshGL(scenes::Modify),
                     tgScnRdr.drawEntResized(scenes::Done) })
        .push_to(out.m_tasks)
        .args({ idDrawingRes, idScnRender, idScnRenderGl, idRenderGl })
        .func([](draw::ACtxDrawingRes    &rDrawingRes,
                 draw::ACtxSceneRender   &rScnRender,
                 draw::ACtxSceneRenderGd &rScnRenderGl,
                 draw::RenderGd          &rRenderGl) noexcept {
            for ( draw::DrawEnt const drawEnt : rScnRender.m_drawIds )
            {
                draw::SysRenderGd::sync_drawent_mesh(drawEnt,
                                                     rScnRender.m_mesh,
                                                     rDrawingRes.m_meshToRes,
                                                     rScnRenderGl.m_meshId,
                                                     rRenderGl);
            }
        });

    rBuilder.task()
        .name("Render Entities")
        .run_on({ tgScnRdr.render(scenes::Run) })
        .sync_with({ tgScnRdr.group(scenes::Ready),
                     tgScnRdr.groupEnts(scenes::Ready),
                     tgMgnScn.camera(scenes::Ready),
                     tgScnRdr.drawTransforms(scenes::UseOrRun),
                     tgScnRdr.entMesh(scenes::Ready),
                     tgScnRdr.entTexture(scenes::Ready),
                     tgMgn.entMeshGL(scenes::Ready),
                     tgMgn.entTextureGL(scenes::Ready),
                     tgScnRdr.drawEnt(scenes::Ready) })
        .push_to(out.m_tasks)
        .args({ idScnRender, idRenderGl, idGroupFwd, idCamera })
        .func([](draw::ACtxSceneRender   &rScnRender,
                 draw::RenderGd          &rRenderGl,
                 draw::RenderGroup const &rGroupFwd,
                 godot::RID const        &rCamera,
                 WorkerContext            ctx) noexcept {
            // FIXME
            osp::draw::ViewProjMatrix viewProj{ Matrix4(), Matrix4() };

            // Forward Render fwd_opaque group to FBO
            draw::SysRenderGd::render_opaque(rGroupFwd, rScnRender.m_visible, viewProj);
        });

    rBuilder.task()
        .name("Delete entities from render groups")
        .run_on({ tgScnRdr.drawEntDelete(scenes::UseOrRun) })
        .sync_with({ tgScnRdr.groupEnts(scenes::Delete) })
        .push_to(out.m_tasks)
        .args({ idDrawing, idGroupFwd, idDrawEntDel })
        .func([](draw::ACtxDrawing const  &rDrawing,
                 draw::RenderGroup        &rGroup,
                 draw::DrawEntVec_t const &rDrawEntDel) noexcept {
            for ( draw::DrawEnt const drawEnt : rDrawEntDel )
            {
                rGroup.entities.remove(drawEnt);
            }
        });

    return out;
} // setup_magnum_scene

void draw_ent_flat(
    DrawEnt ent, ViewProjMatrix const &viewProj, EntityToDraw::UserData_t userData) noexcept
{
    void * const pData = std::get<0>(userData);
    assert(pData != nullptr);

    auto &rData                     = *reinterpret_cast<ACtxDrawFlat *>(pData);

    // Collect uniform information
    Matrix4 const    &drawTf        = (*rData.pDrawTf)[ent];
    auto              rs            = godot::RenderingServer::get_singleton();

    MeshGdId const    meshId        = (*rData.pMeshId)[ent].m_glId;
    GodotMeshInstance rMeshInstance = rData.pMeshGl->get(meshId);
    godot::RID        material      = rs->mesh_surface_get_material(rMeshInstance.mesh, 0);

    if ( rData.pDiffuseTexId != nullptr )
    {
        TexGdId const texGdId = (*rData.pDiffuseTexId)[ent].m_gdId;
        godot::RID    rTex    = rData.pTexGl->get(texGdId);
        rs->material_set_param(material, "albedo_texture", rTex);
    }

    if ( rData.pColor != nullptr )
    {
        auto color = (*rData.pColor)[ent];
        rs->material_set_param(
            material, "albedo_color", godot::Color(color.r(), color.g(), color.b(), color.a()));
    }
    auto rotS   = drawTf.rotationScaling();
    auto basis  = godot::Basis(rotS[0][0],
                              rotS[0][1],
                              rotS[0][2],
                              rotS[1][0],
                              rotS[1][1],
                              rotS[1][2],
                              rotS[2][0],
                              rotS[2][1],
                              rotS[2][2]);
    auto pos    = drawTf.translation();
    auto origin = godot::Vector3(pos.x(), pos.y(), pos.z());
    auto tf     = godot::Transform3D(basis, origin);
    rs->instance_set_transform(rMeshInstance.instance, tf);
}

Session setup_flat_draw(TopTaskBuilder            &rBuilder,
                        ArrayView<entt::any> const topData,
                        Session const             &windowApp,
                        Session const             &sceneRenderer,
                        Session const             &godot,
                        Session const             &godotScene,
                        MaterialId const           materialId)
{
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer, TESTAPP_DATA_SCENE_RENDERER);
    OSP_DECLARE_GET_DATA_IDS(godotScene, TESTAPP_DATA_MAGNUM_SCENE);
    OSP_DECLARE_GET_DATA_IDS(godot, TESTAPP_DATA_MAGNUM);
    auto const tgWin        = windowApp.get_pipelines<PlWindowApp>();
    auto const tgScnRdr     = sceneRenderer.get_pipelines<PlSceneRenderer>();
    auto const tgMgn        = godot.get_pipelines<PlMagnum>();

    auto      &rScnRender   = top_get<ACtxSceneRender>(topData, idScnRender);
    auto      &rScnRenderGl = top_get<ACtxSceneRenderGd>(topData, idScnRenderGl);
    auto      &rRenderGl    = top_get<RenderGd>(topData, idRenderGl);

    Session    out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_SHADER_FLAT)
    auto &rDrawFlat      = top_emplace<ACtxDrawFlat>(topData, idDrawShFlat);

    rDrawFlat.materialId = materialId;
    rDrawFlat.assign_pointers(rScnRender, rScnRenderGl, rRenderGl);

    if ( materialId == lgrn::id_null<MaterialId>() )
    {
        return out;
    }

    rBuilder.task()
        .name("Sync Flat shader DrawEnts")
        .run_on({ tgWin.sync(Run) })
        .sync_with({ tgScnRdr.groupEnts(Modify),
                     tgScnRdr.group(Modify),
                     tgScnRdr.materialDirty(UseOrRun) })
        .push_to(out.m_tasks)
        .args({ idScnRender, idGroupFwd, idScnRenderGl, idDrawShFlat })
        .func([](ACtxSceneRender         &rScnRender,
                 RenderGroup             &rGroupFwd,
                 ACtxSceneRenderGd const &rScnRenderGd,
                 ACtxDrawFlat            &rDrawShFlat) noexcept {
            Material const &rMat = rScnRender.m_materials[rDrawShFlat.materialId];
            sync_drawent_flat(rMat.m_dirty.begin(),
                              rMat.m_dirty.end(),
                              { .hasMaterial    = rMat.m_ents,
                                .pStorageOpaque = &rGroupFwd.entities,
                                /* TODO: set .pStorageTransparent */
                                .opaque         = rScnRender.m_opaque,
                                .transparent    = rScnRender.m_transparent,
                                .diffuse        = rScnRenderGd.m_diffuseTexId,
                                .rData          = rDrawShFlat });
        });

    rBuilder.task()
        .name("Resync Flat shader DrawEnts")
        .run_on({ tgWin.resync(Run) })
        .sync_with({ tgScnRdr.materialDirty(UseOrRun),
                     tgMgn.textureGL(Ready),
                     tgScnRdr.groupEnts(Modify),
                     tgScnRdr.group(Modify) })
        .push_to(out.m_tasks)
        .args({ idScnRender, idGroupFwd, idScnRenderGl, idDrawShFlat })
        .func([](ACtxSceneRender         &rScnRender,
                 RenderGroup             &rGroupFwd,
                 ACtxSceneRenderGd const &rScnRenderGl,
                 ACtxDrawFlat            &rDrawShFlat) noexcept {
            Material const &rMat = rScnRender.m_materials[rDrawShFlat.materialId];
            for ( DrawEnt const drawEnt : rMat.m_ents )
            {
                sync_drawent_flat(drawEnt,
                                  { .hasMaterial    = rMat.m_ents,
                                    .pStorageOpaque = &rGroupFwd.entities,
                                    /* TODO: set .pStorageTransparent */
                                    .opaque         = rScnRender.m_opaque,
                                    .transparent    = rScnRender.m_transparent,
                                    .diffuse        = rScnRenderGl.m_diffuseTexId,
                                    .rData          = rDrawShFlat });
            }
        });

    return out;
}

Session setup_camera_ctrl_godot(TopTaskBuilder            &rBuilder,
                                ArrayView<entt::any> const topData,
                                Session const             &windowApp,
                                Session const             &sceneRenderer,
                                Session const             &magnumScene)
{
    OSP_DECLARE_GET_DATA_IDS(windowApp, TESTAPP_DATA_WINDOW_APP);
    OSP_DECLARE_GET_DATA_IDS(magnumScene, TESTAPP_DATA_MAGNUM_SCENE);
    auto const tgScnRdr   = sceneRenderer.get_pipelines<PlSceneRenderer>();
    auto const tgSR       = magnumScene.get_pipelines<PlMagnumScene>();
    auto const tgWin      = windowApp.get_pipelines<PlWindowApp>();

    auto      &rUserInput = top_get<osp::input::UserInputHandler>(topData, idUserInput);

    Session    out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_CAMERA_CTRL);
    auto const tgCmCt = out.create_pipelines<PlCameraCtrl>(rBuilder);

    top_emplace<ACtxCameraController>(topData, idCamCtrl, rUserInput);

    rBuilder.pipeline(tgCmCt.camCtrl).parent(tgWin.sync);

    rBuilder.task()
        .name("Position Rendering Camera according to Camera Controller")
        .run_on({ tgScnRdr.render(Run) })
        .sync_with({ tgCmCt.camCtrl(Ready), tgSR.camera(Modify) })
        .push_to(out.m_tasks)
        .args({ idCamCtrl, idCamera })
        .func([](ACtxCameraController const &rCamCtrl, godot::RID &rCamera) noexcept {
            godot::RenderingServer *rs     = godot::RenderingServer::get_singleton();
            Vector3                 mTrans = rCamCtrl.m_transform.translation();
            godot::Vector3          gTrans{ mTrans.x(), mTrans.y(), mTrans.z() };

            Matrix3                 mRot = rCamCtrl.m_transform.rotation();
            godot::Basis gBasis = { mRot[0][0], mRot[0][1], mRot[0][2], mRot[1][0], mRot[1][1],
                                    mRot[1][2], mRot[2][0], mRot[2][1], mRot[2][2] };
            rs->camera_set_transform(rCamera, godot::Transform3D(gBasis, gTrans));
        });

    return out;
} // setup_camera_ctrl

} // namespace testapp::scenes