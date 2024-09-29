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
#include <adera/machines/links.h>

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

    rFB.pipeline(godot.pl.mesh).parent(windowApp.pl.sync);
    rFB.pipeline(godot.pl.texture).parent(windowApp.pl.sync);
    rFB.pipeline(godot.pl.entMesh).parent(windowApp.pl.sync);
    rFB.pipeline(godot.pl.entTexture).parent(windowApp.pl.sync);
    // Order-dependent; MagnumApplication construction starts OpenGL context, needed by RenderGL
    /* unused */ // rFB.data_emplace<MagnumApplication>(idActiveApp, args, rUserInput);
    auto &rRenderGd    = rFB.data_emplace<RenderGd>(godot.di.render);

    rRenderGd.scenario = pMainApp->get_main_scenario();
    rRenderGd.viewport = pMainApp->get_main_viewport();
    rFB.task()
        .name("Clean up renderer")
        .run_on({ cleanup.pl.cleanup(Run_) })
        .args({ mainApp.di.resources, godot.di.render })
        .func([](Resources &rResources, RenderGd &rRenderGd) noexcept {
            SysRenderGd::clear_resource_owners(rRenderGd, rResources);
            rRenderGd = {}; // Needs the OpenGL thread for destruction
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
        DependOn<FIWindowApp>       windowApp,
        DependOn<FISceneRenderer>   scnRender,
        DependOn<FIScene>           scn,
        DependOn<FICommonScene>     comScn)
{

    auto       rs       = godot::RenderingServer::get_singleton();

    rFB.pipeline(gdScn.pl.fbo).parent(scnRender.pl.render);
    rFB.pipeline(gdScn.pl.camera).parent(scnRender.pl.render);

    rFB.data_emplace<draw::ACtxSceneRenderGd>(gdScn.di.scnRenderGl);
    godot::RID &rCamera = rFB.data_emplace<godot::RID>(gdScn.di.camera);
    rCamera             = rs->camera_create();

    RenderGd &rRenderGd = rFB.data_get<RenderGd>(godot.di.render);
    rs->viewport_attach_camera(rRenderGd.viewport, rCamera);
    // rs->camera_set_perspective(rCamera, 45., 1.0f, 1u << 24);
    rFB.task()
        .name("Resize ACtxSceneRenderGd to fit all DrawEnts")
        .run_on({ scnRender.pl.drawEntResized(Run) })
        .sync_with({})
        .args({ scnRender.di.scnRender, gdScn.di.scnRenderGl })
        .func([](osp::draw::ACtxSceneRender const &rScnRender,
                 draw::ACtxSceneRenderGd          &rScnRenderGd) noexcept {
            std::size_t const capacity = rScnRender.m_drawIds.capacity();
            rScnRenderGd.m_diffuseTexId.resize(capacity);
            rScnRenderGd.m_meshId.resize(capacity);
            rScnRenderGd.m_instanceId.resize(capacity);
            rScnRenderGd.m_render.resize(capacity);
        });

    rFB.task()
        .name("Compile Resource Meshes to Gd")
        .run_on({ scnRender.pl.meshResDirty(UseOrRun) })
        .sync_with({ scnRender.pl.mesh(Ready),
                     godot.pl.mesh(New),
                     scnRender.pl.entMeshDirty(UseOrRun) })
        .args({ comScn.di.drawingRes, mainApp.di.resources, godot.di.render })
        .func([](osp::draw::ACtxDrawingRes const &rDrawingRes,
                 osp::Resources                  &rResources,
                 osp::draw::RenderGd             &rRenderGd) noexcept {
            draw::SysRenderGd::compile_resource_meshes(rDrawingRes, rResources, rRenderGd);
        });

    rFB.task()
        .name("Compile Resource Textures to Gd")
        .run_on({ scnRender.pl.textureResDirty(UseOrRun) })
        .sync_with({ scnRender.pl.texture(Ready), godot.pl.texture(New) })
        .args({ comScn.di.drawingRes, mainApp.di.resources, godot.di.render })
        .func([](draw::ACtxDrawingRes const &rDrawingRes,
                 osp::Resources             &rResources,
                 draw::RenderGd             &rRenderGd) noexcept {
            draw::SysRenderGd::compile_resource_textures(rDrawingRes, rResources, rRenderGd);
        });

    rFB.task()
        .name("Sync GL textures to entities with scene textures")
        .run_on({ scnRender.pl.entTextureDirty(UseOrRun) })
        .sync_with({ scnRender.pl.texture(Ready),
                     scnRender.pl.entTexture(Ready),
                     godot.pl.texture(Ready),
                     godot.pl.entTexture(Modify),
                     scnRender.pl.drawEntResized(Done) })
        .args({ comScn.di.drawing, comScn.di.drawingRes, scnRender.di.scnRender, gdScn.di.scnRenderGl, godot.di.render })
        .func([](draw::ACtxDrawing       &rDrawing,
                 draw::ACtxDrawingRes    &rDrawingRes,
                 draw::ACtxSceneRender   &rScnRender,
                 draw::ACtxSceneRenderGd &rScnRenderGd,
                 draw::RenderGd          &rRenderGd) noexcept {
            draw::SysRenderGd::sync_drawent_texture(rScnRender.m_diffuseDirty.begin(),
                                                    rScnRender.m_diffuseDirty.end(),
                                                    rScnRender.m_diffuseTex,
                                                    rDrawingRes.m_texToRes,
                                                    rScnRenderGd.m_diffuseTexId,
                                                    rRenderGd);
        });

    rFB.task()
        .name("Resync GL textures")
        .run_on({ windowApp.pl.resync(Run) })
        .sync_with({ scnRender.pl.texture(Ready),
                     godot.pl.texture(Ready),
                     godot.pl.entTexture(Modify),
                     scnRender.pl.drawEntResized(Done) })
        .args({ comScn.di.drawingRes, scnRender.di.scnRender, gdScn.di.scnRenderGl, godot.di.render })
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
        .name("Sync GL meshes to entities with scene meshes")
        .run_on({ scnRender.pl.entMeshDirty(UseOrRun) })
        .sync_with({ scnRender.pl.mesh(Ready),
                     scnRender.pl.entMesh(Ready),
                     godot.pl.mesh(Ready),
                     godot.pl.entMesh(Modify),
                     scnRender.pl.drawEntResized(Done) })
        .args({ comScn.di.drawingRes, scnRender.di.scnRender, gdScn.di.scnRenderGl, godot.di.render })
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
        .name("Resync GL meshes")
        .run_on({ windowApp.pl.resync(Run) })
        .sync_with({ scnRender.pl.mesh(Ready),
                     godot.pl.mesh(Ready),
                     godot.pl.entMesh(Modify),
                     scnRender.pl.drawEntResized(Done) })
        .args({ comScn.di.drawingRes, scnRender.di.scnRender, gdScn.di.scnRenderGl, godot.di.render })
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
        .name("Sync entity parameters")
        .run_on({ scnRender.pl.render(Run) })
        .sync_with({ scnRender.pl.group(Ready),
                     scnRender.pl.groupEnts(Ready),
                     gdScn.pl.camera(Ready),
                     scnRender.pl.drawTransforms(UseOrRun),
                     scnRender.pl.entMesh(Ready),
                     scnRender.pl.entTexture(Ready),
                     godot.pl.entMesh(Ready),
                     godot.pl.entTexture(Ready),
                     scnRender.pl.drawEnt(Ready) })
        .args({ scnRender.di.scnRender, godot.di.render, gdScn.di.scnRenderGl })
        .func([](draw::ACtxSceneRender   &rScnRender,
                 draw::RenderGd          &rRenderGd,
                 draw::ACtxSceneRenderGd &rScnRenderGd) noexcept {
            for (DrawEnt const& ent : rScnRenderGd.m_render) 
            {
                if ( rScnRender.m_visible.contains(ent) )
                {
                    sync_godot_ent(ent, rScnRender, rScnRenderGd, rRenderGd);
                }
            }
        });

    rFB.task()
        .name("Delete entities from render groups")
        .run_on({ scnRender.pl.drawEntDelete(UseOrRun) })
        .sync_with({ scnRender.pl.groupEnts(Delete) })
        .args({ comScn.di.drawEntDel, gdScn.di.scnRenderGl})
        .func([](draw::DrawEntVec_t const &rDrawEntDel, 
                    draw::ACtxSceneRenderGd  &rScnRenderGl) noexcept {
            rScnRenderGl.m_render.erase(rDrawEntDel.begin(), rDrawEntDel.end());
        });

    rFB.task()
        .name("Delete entities instance from scene")
        .run_on({ scnRender.pl.drawEntDelete(UseOrRun) })
        .args({ comScn.di.drawing, comScn.di.drawEntDel, gdScn.di.scnRenderGl })
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
        .name("Sync Flat shader DrawEnts")
        .run_on({ windowApp.pl.sync(Run) })
        .sync_with({ scnRender.pl.groupEnts(Modify),
                     scnRender.pl.group(Modify),
                     scnRender.pl.materialDirty(UseOrRun) })
        .args({ scnRender.di.scnRender, gdScn.di.scnRenderGl, godot.di.render})
        .func([](ACtxSceneRender         &rScnRender,
                 ACtxSceneRenderGd &rScnRenderGd, 
                 RenderGd          &rRenderGd) noexcept {
                
            for (Material const &rMat : rScnRender.m_materials) 
            {
                rScnRenderGd.m_render.insert(rMat.m_dirty.begin(), rMat.m_dirty.end());
            }
        });

    rFB.task()
        .name("Resync Flat shader DrawEnts")
        .run_on({ windowApp.pl.resync(Run) })
        .sync_with({ scnRender.pl.materialDirty(UseOrRun),
                     godot.pl.texture(Ready),
                     scnRender.pl.groupEnts(Modify),
                     scnRender.pl.group(Modify) })
        .args({ scnRender.di.scnRender, gdScn.di.scnRenderGl, godot.di.render})
        .func([](ACtxSceneRender         &rScnRender,
                 ACtxSceneRenderGd  &rScnRenderGd, 
                 RenderGd           &rRenderGd) noexcept {
            for (Material const &rMat : rScnRender.m_materials) 
            {
                rScnRenderGd.m_render.insert(rMat.m_ents.begin(), rMat.m_ents.end());
            }
        });
}); // ftrGodotScene

void sync_godot_ent(DrawEnt ent, ACtxSceneRender &rScnRender, ACtxSceneRenderGd &rScnRenderGd, RenderGd &rRenderGd) noexcept
{
    // Collect uniform information
    Matrix4 const &drawTf    = rScnRender.m_drawTransform[ent];
    godot::RID    &rInstance = rScnRenderGd.m_instanceId[ent];

    auto           rs        = godot::RenderingServer::get_singleton();

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
        material, "albedo_color", godot::Color(color.r(), color.g(), color.b(), color.a()));

    // Create instance if it does not exist
    if ( ! rInstance.is_valid() )
    {
        rInstance = rs->instance_create2(rMesh, rRenderGd.scenario);
    }

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
        Implement<FICameraControl>  camCtrl,
        DependOn<FIGodot>           godot,
        DependOn<FIGodotScene>      gdScn,
        DependOn<FIWindowApp>       windowApp,
        DependOn<FISceneRenderer>   scnRender)
{
    auto      &rUserInput = rFB.data_get<osp::input::UserInputHandler>(windowApp.di.userInput);

    rFB.data_emplace<ACtxCameraController>(camCtrl.di.camCtrl, rUserInput);

    rFB.pipeline(camCtrl.pl.camCtrl).parent(windowApp.pl.sync);

    rFB.task()
        .name("Position Rendering Camera according to Camera Controller")
        .run_on({ scnRender.pl.render(Run) })
        .sync_with({ camCtrl.pl.camCtrl(Ready), gdScn.pl.camera(Modify) })
        .args({ camCtrl.di.camCtrl, gdScn.di.camera })
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
