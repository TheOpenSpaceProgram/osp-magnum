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
#include "input.h"
#include "render.h"

#include "../feature_interfaces.h"

#include <Magnum/Magnum.h>

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <adera/drawing/CameraController.h>

#include <osp/activescene/basic_fn.h>
#include <osp/core/math_types.h>
#include <osp/drawing/drawing.h>
#include <osp/universe/coordinates.h>
#include <osp/universe/universe.h>

#include <entt/core/fwd.hpp>

#include <longeron/id_management/null.hpp>


// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

using namespace adera;
using namespace osp::active;
using namespace osp::draw;
using namespace osp::universe;
using namespace osp::fw;
using namespace ftr_inter;
using namespace ftr_inter::stages;

using namespace osp;

using osp::input::UserInputHandler;

namespace ospgdext
{


osp::fw::FeatureDef const ftrGodot = feature_def("Godot", [] (
        FeatureBuilder              &rFB,
        Implement<FIGodot>          godot,
        DependOn<FICleanupContext>  cleanup,
        DependOn<FIMainApp>         mainApp,
        DependOn<FIWindowApp>       windowApp,
        entt::any                   userData)
{
    auto pMainApp    = entt::any_cast<godot::FlyingScene*>(userData);
    auto &rUserInput = rFB.data_get<UserInputHandler>(windowApp.di.userInput);

    config_controls(rUserInput);
    //pMainApp->set_user_input(&rUserInput);

    rFB.data_emplace<godot::FlyingScene *>(godot.di.app, pMainApp);

    rFB.pipeline(godot.pl.mesh)         .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(godot.pl.texture)      .parent(mainApp.loopblks.mainLoop);

    auto &rRenderGd    = rFB.data_emplace<RenderGd>(godot.di.render);

    rRenderGd.scenario = pMainApp->get_main_scenario();
    rRenderGd.viewport = pMainApp->get_main_viewport();
    rFB.task()
        .name       ("Clean up renderer")
        .sync_with  ({ cleanup.pl.cleanup(Run_) })
        .args       ({ mainApp.di.resources, godot.di.render })
        .func       ([](Resources &rResources, RenderGd &rRenderGd) noexcept {
            SysRenderGd::clear_resource_owners(rRenderGd, rResources);
            rRenderGd = {};
        });
}); // ftrGodot



/**
 * @brief stuff needed to render a scene using Magnum
 */
osp::fw::FeatureDef const ftrGodotScene = feature_def("GodotScene", [] (
        FeatureBuilder              &rFB,
        Implement<FIGodotScene>     gdScn,
        DependOn<FIGodot>           godot,
        DependOn<FIMainApp>         mainApp,
        DependOn<FICleanupContext>  cleanup,
        DependOn<FIWindowApp>       windowApp,
        DependOn<FISceneRenderer>   scnRender,
        DependOn<FIScene>           scn,
        DependOn<FICommonScene>     comScn)
{

    auto       rs       = godot::RenderingServer::get_singleton();

    rFB.pipeline(gdScn.pl.fbo)              .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(gdScn.pl.camera)           .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(gdScn.pl.entDiffuseGD)     .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(gdScn.pl.entMeshGD)        .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(gdScn.pl.entInstIds)       .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(gdScn.pl.renderEnts)       .parent(mainApp.loopblks.mainLoop);

    rFB.data_emplace<draw::ACtxSceneRenderGd>(gdScn.di.scnRenderGd);
    godot::RID &rCamera = rFB.data_emplace<godot::RID>(gdScn.di.camera);
    rCamera             = rs->camera_create();

    RenderGd &rRenderGd = rFB.data_get<RenderGd>(godot.di.render);
    rs->viewport_attach_camera(rRenderGd.viewport, rCamera);
    // rs->camera_set_perspective(rCamera, 45., 1.0f, 1u << 24);
    rFB.task()
        .name("Resize ACtxSceneRenderGd to fit all DrawEnts")
        .sync_with({scnRender.pl.drawEnt(Ready), gdScn.pl.entMeshGD(Resize_), gdScn.pl.entDiffuseGD(Resize_), gdScn.pl.entInstIds(Resize_), gdScn.pl.renderEnts(Resize_) })
        .args({ scnRender.di.scnRender, gdScn.di.scnRenderGd })
        .func([](osp::draw::ACtxSceneRender const &rScnRender,
                 draw::ACtxSceneRenderGd          &rScnRenderGd) noexcept
    {
        std::size_t const capacity = rScnRender.m_drawIds.capacity();
        rScnRenderGd.m_diffuseTexId .resize(capacity);
        rScnRenderGd.m_meshId       .resize(capacity);
        rScnRenderGd.m_instanceId   .resize(capacity);
        rScnRenderGd.m_render       .resize(capacity);
    });

    rFB.task()
        .name("Compile Resource Meshes to Gd")
        .sync_with({ scnRender.pl.mesh(Ready), godot.pl.mesh(New)})
        .args({ comScn.di.drawingRes, mainApp.di.resources, godot.di.render })
        .func([](osp::draw::ACtxDrawingRes const &rDrawingRes,
                 osp::Resources                  &rResources,
                 osp::draw::RenderGd             &rRenderGd) noexcept {
            draw::SysRenderGd::compile_resource_meshes(rDrawingRes, rResources, rRenderGd);
        });

    rFB.task()
        .name("Compile Resource Textures to Gd")
        .sync_with({ scnRender.pl.diffuseTex(Ready), godot.pl.texture(New) })
        .args({ comScn.di.drawingRes, mainApp.di.resources, godot.di.render })
        .func([](draw::ACtxDrawingRes const &rDrawingRes,
                 osp::Resources             &rResources,
                 draw::RenderGd             &rRenderGd) noexcept {
            draw::SysRenderGd::compile_resource_textures(rDrawingRes, rResources, rRenderGd);
        });

    rFB.task()
        .name("Assign GD textures to DrawEnts with diffuse textures")
        .sync_with({ scnRender.pl.diffuseTexDirty(UseOrRun),
                     scnRender.pl.diffuseTex(Ready),
                     gdScn.pl.entDiffuseGD(New),
                     godot.pl.texture(Ready)})
        .args({ comScn.di.drawing, comScn.di.drawingRes, scnRender.di.scnRender, gdScn.di.scnRenderGd, godot.di.render })
        .func([](draw::ACtxDrawing       &rDrawing,
                 draw::ACtxDrawingRes    &rDrawingRes,
                 draw::ACtxSceneRender   &rScnRender,
                 draw::ACtxSceneRenderGd &rScnRenderGd,
                 draw::RenderGd          &rRenderGd) noexcept {
            draw::SysRenderGd::sync_drawent_texture(rScnRender.m_diffuseTexDirty.begin(),
                                                    rScnRender.m_diffuseTexDirty.end(),
                                                    rScnRender.m_diffuseTex,
                                                    rDrawingRes.m_texToRes,
                                                    rScnRenderGd.m_diffuseTexId,
                                                    rRenderGd);
        });

    rFB.task()
        .name("Resync godot textures")
        .sync_with({ windowApp.pl.resync(Run),
                     scnRender.pl.diffuseTex(Ready),
                     godot.pl.texture(Ready),
                     gdScn.pl.entDiffuseGD(New) })
        .args({ comScn.di.drawingRes, scnRender.di.scnRender, gdScn.di.scnRenderGd, godot.di.render })
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

    rFB.task()
        .name("Assign GD meshes to entities with scene meshes")
        .sync_with({ scnRender.pl.meshDirty(UseOrRun), scnRender.pl.mesh(Ready),
                     godot.pl.mesh(Ready), gdScn.pl.entMeshGD(New) })
        .args({ comScn.di.drawingRes, scnRender.di.scnRender, gdScn.di.scnRenderGd, godot.di.render })
        .func([](draw::ACtxDrawingRes    &rDrawingRes,
                 draw::ACtxSceneRender   &rScnRender,
                 draw::ACtxSceneRenderGd &rScnRenderGl,
                 draw::RenderGd          &rRenderGl) noexcept {
            draw::SysRenderGd::sync_drawent_mesh(rScnRender.m_meshDirty.begin(),
                                                 rScnRender.m_meshDirty.end(),
                                                 rScnRender.m_mesh,
                                                 rDrawingRes.m_meshToRes,
                                                 rScnRenderGl.m_meshId,
                                                 rScnRenderGl.m_instanceId,
                                                 rRenderGl);
        });

    rFB.task()
        .name("Resync GD meshes")
        .sync_with({ windowApp.pl.resync(Run),
                     scnRender.pl.drawEnt(Ready),
                     scnRender.pl.mesh(Ready),
                     godot.pl.mesh(Ready),
                     gdScn.pl.entMeshGD(New) })
        .args({ comScn.di.drawingRes, scnRender.di.scnRender, gdScn.di.scnRenderGd, godot.di.render })
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
                                                     rScnRenderGl.m_instanceId,
                                                     rRenderGl);
            }
        });

    rFB.task()
        .name("Sync DrawEnt parameters")
        .sync_with({ scnRender.pl.render(Run),
                     scnRender.pl.drawTransforms(Ready),
                     scnRender.pl.mesh(Ready),
                     scnRender.pl.diffuseTex(Ready),
                     scnRender.pl.drawEnt(Ready),
                     gdScn.pl.camera(Ready),
                     gdScn.pl.renderEnts(Ready),
                     gdScn.pl.entMeshGD(Ready),
                     gdScn.pl.entDiffuseGD(Ready)})
        .args({ scnRender.di.scnRender, godot.di.render, gdScn.di.scnRenderGd })
        .func([](draw::ACtxSceneRender   &rScnRender,
                 draw::RenderGd          &rRenderGd,
                 draw::ACtxSceneRenderGd &rScnRenderGd) noexcept {
            for (DrawEnt const& ent : rScnRenderGd.m_render)
            {
                sync_godot_ent(ent, rScnRender, rScnRenderGd, rRenderGd);
            }
        });

    rFB.task()
        .name("Delete DrawEnts from ACtxSceneRenderGd::m_render")
        .sync_with({ scnRender.pl.drawEntDelete(UseOrRun), gdScn.pl.renderEnts(Delete) })
        .args({ scnRender.di.drawEntDel, gdScn.di.scnRenderGd})
        .func([](draw::DrawEntVec_t const &rDrawEntDel, 
                    draw::ACtxSceneRenderGd  &rScnRenderGl) noexcept {
            rScnRenderGl.m_render.erase(rDrawEntDel.begin(), rDrawEntDel.end());
        });

    rFB.task()
        .name("Delete DrawEnts from Godot scene")
        .sync_with({ scnRender.pl.drawEntDelete(UseOrRun) })
        .args({ comScn.di.drawing, scnRender.di.drawEntDel, gdScn.di.scnRenderGd })
        .func([](draw::ACtxDrawing const  &rDrawing,
                 draw::DrawEntVec_t const &rDrawEntDel,
                 draw::ACtxSceneRenderGd  &rScnRenderGl) noexcept {
            for ( draw::DrawEnt const drawEnt : rDrawEntDel )
            {
                auto        rs        = godot::RenderingServer::get_singleton();
                godot::RID &rInstance = rScnRenderGl.m_instanceId[drawEnt];
                rs->free_rid(rInstance);
                rInstance = {};
            }
        });
    
    rFB.task()
        .name("Sync Flat shader ACtxSceneRenderGd::m_render")
        .sync_with({ windowApp.pl.sync(Run),
                     gdScn.pl.renderEnts(New),
                     scnRender.pl.materialDirty(UseOrRun) })
        .args({ scnRender.di.scnRender, gdScn.di.scnRenderGd, godot.di.render})
        .func([](ACtxSceneRender         &rScnRender,
                 ACtxSceneRenderGd &rScnRenderGd, 
                 RenderGd          &rRenderGd) noexcept {
                
            for (Material const &rMat : rScnRender.m_materials) 
            {
                rScnRenderGd.m_render.insert(rMat.m_dirty.begin(), rMat.m_dirty.end());
            }
        });

    rFB.task()
        .name("Resync Flat shader ACtxSceneRenderGd::m_render")
        .sync_with({ windowApp.pl.resync(Run),
                     gdScn.pl.renderEnts(Ready),
                     scnRender.pl.material(Ready)})
        .args({ scnRender.di.scnRender, gdScn.di.scnRenderGd, godot.di.render})
        .func([](ACtxSceneRender         &rScnRender,
                 ACtxSceneRenderGd  &rScnRenderGd, 
                 RenderGd           &rRenderGd) noexcept {
            for (Material const &rMat : rScnRender.m_materials) 
            {
                rScnRenderGd.m_render.insert(rMat.m_ents.begin(), rMat.m_ents.end());
            }
        });
    rFB.task()
        .name("Clean up scene")
        .sync_with({ cleanup.pl.cleanup(Run_) })
        .args({ gdScn.di.scnRenderGd , gdScn.di.camera})
        .func([](ACtxSceneRenderGd &rScnRenderGd, godot::RID& rCamera) noexcept {
            godot::RenderingServer *rs = godot::RenderingServer::get_singleton();
            rs->free_rid(rCamera);
            rScnRenderGd.clear_resource_owners();
            rScnRenderGd = {}; // Needs the OpenGL thread for destruction
        });
}); // ftrGodotScene

void sync_godot_ent(DrawEnt ent, ACtxSceneRender &rScnRender, ACtxSceneRenderGd &rScnRenderGd, RenderGd &rRenderGd) noexcept
{
    // Collect uniform information
    Matrix4 const &drawTf    = rScnRender.m_drawTransform[ent];
    godot::RID    &rInstance = rScnRenderGd.m_instanceId[ent];

    auto           rs        = godot::RenderingServer::get_singleton();

    // Create instance if it does not exist
    if ( ! rInstance.is_valid() )
    {
        rInstance = rs->instance_create();
        rs->instance_set_scenario(rInstance, rRenderGd.scenario);
    }

    // set visibility
    if (rScnRender.m_visible.contains(ent)) {
        rs->instance_set_visible(rInstance, true);
    }
    else
    {
        rs->instance_set_visible(rInstance, false);
        return;
    }

    MeshGdId const meshId    = rScnRenderGd.m_meshId[ent].m_glId;
    if ( meshId == lgrn::id_null<MeshGdId>())
    {
        return;
    }
    godot::RID     rMesh     = rRenderGd.m_meshGd.get(meshId);
    godot::RID     material  = rs->mesh_surface_get_material(rMesh, 0);
    // create the material if it does not already exists
    if ( ! material.is_valid() )
    {
        material = rs->material_create();
        rs->mesh_surface_set_material(rMesh, 0, material);
    }
    // test if the mesh is textured or not.
    if ( rScnRenderGd.m_diffuseTexId[ent].m_gdId != lgrn::id_null<TexGdId>() )
    {
        TexGdId const texGdId = rScnRenderGd.m_diffuseTexId[ent].m_gdId;
        godot::RID    rTex    = rRenderGd.m_texGd.get(texGdId);
        rs->material_set_param(material, "albedo_texture", rTex);
    }

    // Set albdedo color
    auto color = rScnRender.m_color[ent];
    rs->material_set_param(
        material, "albedo_color", godot::Color(1., 0., 0., 0.5));

    rs->mesh_surface_set_material(rMesh, 0, material);
    rs->instance_set_base(rInstance, rMesh);

    auto         rot   = Magnum::Quaternion::fromMatrix(drawTf.rotation()).data();
    auto         scale = drawTf.scaling();
    godot::Basis basis{ godot::Quaternion(rot[0], rot[1], rot[2], rot[3]),
                        godot::Vector3(scale.x(), scale.y(), scale.z()) };

    auto         pos    = drawTf.translation();
    auto         origin = godot::Vector3(pos.x(), pos.y(), pos.z());
    auto         tf     = godot::Transform3D(basis, origin);
    rs->instance_set_transform(rInstance, tf);

}

osp::fw::FeatureDef const ftrCameraControlGD = feature_def("CameraControlGodot", [] (
        FeatureBuilder              &rFB,
        Implement<FICamCtrlBase>    camCtrlBase,
        DependOn<FIMainApp>         mainApp,
        DependOn<FIGodot>           godot,
        DependOn<FIGodotScene>      gdScn,
        DependOn<FIWindowApp>       windowApp,
        DependOn<FISceneRenderer>   scnRender)
{
    auto &rUserInput = rFB.data_get< osp::input::UserInputHandler >(windowApp.di.userInput);

    rFB.data_emplace< ACtxCameraController >    (camCtrlBase.di.camCtrl);
    rFB.data_emplace< ACtxCameraButtons >       (camCtrlBase.di.camButtons, rUserInput);

    rFB.pipeline(camCtrlBase.pl.camTarget)   .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(camCtrlBase.pl.camRefFrame) .parent(mainApp.loopblks.mainLoop);
    rFB.pipeline(camCtrlBase.pl.camTransform).parent(mainApp.loopblks.mainLoop);

    rFB.task()
        .name       ("Update Camera controller transform")
        .sync_with  ({camCtrlBase.pl.camTransform(Modify), camCtrlBase.pl.camTarget(Ready), camCtrlBase.pl.camRefFrame(Ready)})
        .args       ({          camCtrlBase.di.camCtrl})
        .func       ([] (ACtxCameraController& rCamCtrl) noexcept
    {
        rCamCtrl.update_transform();
    });

    rFB.task()
        .name("Position Rendering Camera according to Camera Controller")
        .sync_with({ scnRender.pl.render(Run), camCtrlBase.pl.camTransform(Ready), gdScn.pl.camera(Modify) })
        .args({ camCtrlBase.di.camCtrl, gdScn.di.camera })
        .func([](ACtxCameraController const &rCamCtrl, godot::RID &rCamera) noexcept {
            godot::RenderingServer *rs     = godot::RenderingServer::get_singleton();
            Vector3                 mTrans = rCamCtrl.m_transform.translation();
            godot::Vector3          gTrans{ mTrans.x(), mTrans.y(), mTrans.z() };

            Matrix3                 mRot = rCamCtrl.m_transform.rotation();
            auto                    quat = Magnum::Quaternion::fromMatrix(mRot).data();
            godot::Basis gBasis = { godot::Quaternion(quat[0], quat[1], quat[2], quat[3]) };

            rs->camera_set_transform(rCamera, godot::Transform3D(gBasis, gTrans));
        });
}); // ftrCameraControlGD

} // namespace ospgdext
