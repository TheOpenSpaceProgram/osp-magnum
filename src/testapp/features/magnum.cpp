/**
 * Open Space Program
 * Copyright © 2019-2024 Open Space Program Project
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

#include "../MagnumWindowApp.h"
#include "../feature_interfaces.h"


#include <adera/drawing/CameraController.h>
#include <adera/machines/links.h>
#include <adera_drawing_gl/flat_shader.h>
#include <adera_drawing_gl/phong_shader.h>
#include <adera_drawing_gl/visualizer_shader.h>

#include <planet-a/activescene/terrain.h>

#include <osp/activescene/basic_fn.h>
#include <osp/drawing/drawing.h>
#include <osp/universe/coordinates.h>
#include <osp/universe/universe.h>
#include <osp/util/logging.h>
#include <osp_drawing_gl/rendergl.h>

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

using namespace adera;
using namespace adera::shader;
using namespace osp::active;
using namespace osp::draw;
using namespace osp::universe;
using namespace osp::fw;
using namespace osp;
using namespace ftr_inter;
using namespace ftr_inter::stages;

using Magnum::GL::Mesh;
using osp::input::UserInputHandler;

namespace testapp
{

FeatureDef const ftrMagnum = feature_def("Magnum", [] (
        FeatureBuilder              &rFB,
        Implement<FIMagnum>         magnum,
        DependOn<FICleanupContext>  cleanup,
        DependOn<FIWindowApp>       windowApp,
        DependOn<FIMainApp>         mainApp,
        entt::any                   userData)
{
    auto &rUserInput = rFB.data_get<UserInputHandler>(windowApp.di.userInput);
    config_controls(rUserInput);

    rFB.pipeline(magnum.pl.meshGL)         .parent(windowApp.pl.sync);
    rFB.pipeline(magnum.pl.textureGL)      .parent(windowApp.pl.sync);
    rFB.pipeline(magnum.pl.entMeshGL)      .parent(windowApp.pl.sync);
    rFB.pipeline(magnum.pl.entTextureGL)   .parent(windowApp.pl.sync);

    auto const &args = entt::any_cast<MagnumWindowApp::Arguments>(userData);

    OSP_LOG_INFO("Starting Magnum Window Application...");

    // Order-dependent; MagnumWindowApp construction starts OpenGL context, needed by RenderGL
    /* not used here */   rFB.data_emplace<MagnumWindowApp>(magnum.di.magnumApp, args, rUserInput);
    auto &rRenderGl     = rFB.data_emplace<RenderGL>         (magnum.di.renderGl);

    SysRenderGL::setup_context(rRenderGl);

    rFB.task()
        .name       ("Clean up Magnum renderer")
        .run_on     ({cleanup.pl.cleanup(Run_)})
        .args       ({    mainApp.di.resources,  magnum.di.renderGl})
        .func       ([] (Resources &rResources, RenderGL &rRenderGl) noexcept
    {
        SysRenderGL::clear_resource_owners(rRenderGl, rResources);
        rRenderGl = {}; // Needs the OpenGL thread for destruction
    });

}); // ftrMagnum



FeatureDef const ftrMagnumScene = feature_def("MagnumScene", [] (
        FeatureBuilder              &rFB,
        Implement<FIMagnumScene>    magnumScn,
        DependOn<FIMainApp>         mainApp,
        DependOn<FICommonScene>     comScn,
        DependOn<FIMagnum>          magnum,
        DependOn<FIWindowApp>       windowApp,
        DependOn<FISceneRenderer>   scnRender)
{
    rFB.pipeline(magnumScn.pl.fbo)             .parent(scnRender.pl.render);
    rFB.pipeline(magnumScn.pl.camera)          .parent(scnRender.pl.render);

    /* not used here */   rFB.data_emplace< ACtxSceneRenderGL > (magnumScn.di.scnRenderGl);
    /* not used here */   rFB.data_emplace< RenderGroup >       (magnumScn.di.groupFwd);
    auto &rCamera       = rFB.data_emplace< Camera >            (magnumScn.di.camera);

    rCamera.m_far = 100000000.0f;
    rCamera.m_near = 1.0f;
    rCamera.m_fov = Magnum::Deg(45.0f);

    // TODO: format after framework changes

    rFB.task()
        .name       ("Resize ACtxSceneRenderGL (OpenGL) to fit all DrawEnts")
        .run_on     ({scnRender.pl.drawEntResized(Run)})
        .sync_with  ({})
        .args       ({ scnRender.di.scnRender, magnumScn.di.scnRenderGl })
        .func       ([] (ACtxSceneRender const &rScnRender, ACtxSceneRenderGL &rScnRenderGl) noexcept
    {
        std::size_t const capacity = rScnRender.m_drawIds.capacity();
        rScnRenderGl.m_diffuseTexId   .resize(capacity);
        rScnRenderGl.m_meshId         .resize(capacity);
    });

    rFB.task()
        .name       ("Compile Resource Meshes to GL")
        .run_on     ({scnRender.pl.meshResDirty(UseOrRun)})
        .sync_with  ({scnRender.pl.mesh(Ready), magnum.pl.meshGL(New), scnRender.pl.entMeshDirty(UseOrRun)})
        .args       ({                 comScn.di.drawingRes,                mainApp.di.resources,          magnum.di.renderGl })
        .func([] (ACtxDrawingRes const &rDrawingRes, osp::Resources &rResources, RenderGL &rRenderGl) noexcept
    {
        SysRenderGL::compile_resource_meshes(rDrawingRes, rResources, rRenderGl);
    });

    rFB.task()
        .name       ("Compile Resource Textures to GL")
        .run_on     ({scnRender.pl.textureResDirty(UseOrRun)})
        .sync_with  ({scnRender.pl.texture(Ready), magnum.pl.textureGL(New)})
        .args       ({                 comScn.di.drawingRes,                mainApp.di.resources,          magnum.di.renderGl })
        .func([] (ACtxDrawingRes const &rDrawingRes, osp::Resources &rResources, RenderGL &rRenderGl) noexcept
    {
        SysRenderGL::compile_resource_textures(rDrawingRes, rResources, rRenderGl);
    });

    rFB.task()
        .name       ("Sync GL textures to entities with scene textures")
        .run_on     ({scnRender.pl.entTextureDirty(UseOrRun)})
        .sync_with  ({scnRender.pl.texture(Ready), scnRender.pl.entTexture(Ready), magnum.pl.textureGL(Ready), magnum.pl.entTextureGL(Modify), scnRender.pl.drawEntResized(Done)})
        .args       ({        comScn.di.drawing,                comScn.di.drawingRes,                 scnRender.di.scnRender,                   magnumScn.di.scnRenderGl,          magnum.di.renderGl })
        .func([] (ACtxDrawing &rDrawing, ACtxDrawingRes &rDrawingRes, ACtxSceneRender &rScnRender, ACtxSceneRenderGL &rScnRenderGl, RenderGL &rRenderGl) noexcept
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
        .sync_with  ({scnRender.pl.texture(Ready), magnum.pl.textureGL(Ready), magnum.pl.entTextureGL(Modify), scnRender.pl.drawEntResized(Done)})
        .args       ({           comScn.di.drawingRes,                 scnRender.di.scnRender,                   magnumScn.di.scnRenderGl,          magnum.di.renderGl })
        .func([] (ACtxDrawingRes &rDrawingRes, ACtxSceneRender &rScnRender, ACtxSceneRenderGL &rScnRenderGl, RenderGL &rRenderGl) noexcept
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
        .run_on     ({scnRender.pl.entMeshDirty(UseOrRun)})
        .sync_with  ({scnRender.pl.mesh(Ready), scnRender.pl.entMesh(Ready), magnum.pl.meshGL(Ready), magnum.pl.entMeshGL(Modify), scnRender.pl.drawEntResized(Done)})
        .args       ({           comScn.di.drawingRes,                 scnRender.di.scnRender,                   magnumScn.di.scnRenderGl,          magnum.di.renderGl })
        .func       ([] (ACtxDrawingRes &rDrawingRes, ACtxSceneRender &rScnRender, ACtxSceneRenderGL &rScnRenderGl, RenderGL &rRenderGl) noexcept
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
        .sync_with  ({scnRender.pl.mesh(Ready), magnum.pl.meshGL(Ready), magnum.pl.entMeshGL(Modify), scnRender.pl.drawEntResized(Done)})
        .args       ({           comScn.di.drawingRes,                 scnRender.di.scnRender,                   magnumScn.di.scnRenderGl,          magnum.di.renderGl })
        .func([] (ACtxDrawingRes &rDrawingRes, ACtxSceneRender &rScnRender, ACtxSceneRenderGL &rScnRenderGl, RenderGL &rRenderGl) noexcept
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
        .run_on     ({scnRender.pl.render(Run)})
        .sync_with  ({magnumScn.pl.fbo(EStgFBO::Bind)})
        .args       ({              comScn.di.drawing,          magnum.di.renderGl,                   magnumScn.di.groupFwd,              magnumScn.di.camera })
        .func       ([] (ACtxDrawing const &rDrawing, RenderGL &rRenderGl, RenderGroup const &rGroupFwd, Camera const &rCamera) noexcept
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
        .run_on     ({scnRender.pl.render(Run)})
        .sync_with  ({scnRender.pl.group(Ready), scnRender.pl.groupEnts(Ready), magnumScn.pl.camera(Ready), scnRender.pl.drawTransforms(UseOrRun), scnRender.pl.entMesh(Ready), scnRender.pl.entTexture(Ready),
                      magnum.pl.entMeshGL(Ready), magnum.pl.entTextureGL(Ready),
                      scnRender.pl.drawEnt(Ready)})
        .args       ({            scnRender.di.scnRender,          magnum.di.renderGl,    magnumScn.di.groupFwd,     magnumScn.di.camera })
        .func       ([] (ACtxSceneRender &rScnRender, RenderGL &rRenderGl, RenderGroup const &rGroupFwd, Camera const &rCamera) noexcept
    {
        ViewProjMatrix viewProj{rCamera.m_transform.inverted(), rCamera.perspective()};

        // Forward Render fwd_opaque group to FBO
        SysRenderGL::render_opaque(rGroupFwd, rScnRender.m_visible, viewProj);
    });

    rFB.task()
        .name       ("Delete entities from render groups")
        .run_on     ({scnRender.pl.drawEntDelete(UseOrRun)})
        .sync_with  ({scnRender.pl.groupEnts(Delete)})
        .args       ({              comScn.di.drawing,             magnumScn.di.groupFwd,                 comScn.di.drawEntDel })
        .func       ([] (ACtxDrawing const &rDrawing, RenderGroup &rGroup, DrawEntVec_t const &rDrawEntDel) noexcept
    {
        for (DrawEnt const drawEnt : rDrawEntDel)
        {
            rGroup.entities.remove(drawEnt);
        }
    });
}); // ftrMagnumScene


FeatureDef const ftrCameraControl = feature_def("CameraControl", [] (
        FeatureBuilder                  &rFB,
        Implement<FICameraControl>      camCtrl,
        DependOn<FICleanupContext>      cleanup,
        DependOn<FIWindowApp>           windowApp,
        DependOn<FISceneRenderer>       scnRender,
        DependOn<FIMagnumScene>         magnumScn)
{
    auto &rUserInput = rFB.data_get< osp::input::UserInputHandler >(windowApp.di.userInput);

    rFB.data_emplace< ACtxCameraController > (camCtrl.di.camCtrl, rUserInput);

    rFB.pipeline(camCtrl.pl.camCtrl).parent(windowApp.pl.sync);

    rFB.task()
        .name       ("Position Rendering Camera according to Camera Controller")
        .run_on     ({scnRender.pl.render(Run)})
        .sync_with  ({camCtrl.pl.camCtrl(Ready), magnumScn.pl.camera(Modify)})
        .args       ({                     camCtrl.di.camCtrl, magnumScn.di.camera })
        .func       ([] (ACtxCameraController const& rCamCtrl,     Camera &rCamera) noexcept
    {
        rCamera.m_transform = rCamCtrl.m_transform;
    });

    rFB.task()
        .name       ("Clean up ACtxCameraController's subscription to UserInputHandler")
        .run_on     ({cleanup.pl.cleanup(Run_)})
        .sync_with  ({})
        .args       ({               camCtrl.di.camCtrl })
        .func       ([] (ACtxCameraController &rCamCtrl) noexcept
    {
        rCamCtrl.m_controls.unsubscribe();
    });

}); // ftrCameraControl


FeatureDef const ftrShaderVisualizer = feature_def("ShaderVisualizer", [] (
        FeatureBuilder              &rFB,
        Implement<FIShaderVisualizer> shVisual,
        DependOn<FIWindowApp>       windowApp,
        DependOn<FIMagnum>          magnum,
        DependOn<FIMagnumScene>     magnumScn,
        DependOn<FISceneRenderer>   scnRender,
        entt::any                   userData)
{
    auto const materialId = userData /* if not null */ ? entt::any_cast<MaterialId>(userData)
                                                       : MaterialId{};

    auto &rScnRender    = rFB.data_get< ACtxSceneRender >    (scnRender.di.scnRender);
    auto &rScnRenderGl  = rFB.data_get< ACtxSceneRenderGL >  (magnumScn.di.scnRenderGl);
    auto &rRenderGl     = rFB.data_get< RenderGL >           (magnum.di.renderGl);

    auto &rDrawVisual = rFB.data_emplace< ACtxDrawMeshVisualizer >(shVisual.di.shader);

    rDrawVisual.m_materialId = materialId;
    rDrawVisual.m_shader = MeshVisualizer{ MeshVisualizer::Configuration{}.setFlags(MeshVisualizer::Flag::Wireframe) };
    rDrawVisual.assign_pointers(rScnRender, rScnRenderGl, rRenderGl);

    // Default colors
    rDrawVisual.m_shader.setWireframeColor({0.7f, 0.5f, 0.7f, 1.0f});
    rDrawVisual.m_shader.setColor({0.2f, 0.1f, 0.5f, 1.0f});

    if (materialId == lgrn::id_null<MaterialId>())
    {
        return;
    }

    // TODO: format after framework changes

    rFB.task()
        .name       ("Sync MeshVisualizer shader DrawEnts")
        .run_on     ({windowApp.pl.sync(Run)})
        .sync_with  ({scnRender.pl.materialDirty(UseOrRun), magnum.pl.textureGL(Ready), scnRender.pl.groupEnts(Modify)})
        .args       ({            scnRender.di.scnRender,             magnumScn.di.groupFwd,                        shVisual.di.shader})
        .func([] (ACtxSceneRender &rScnRender, RenderGroup &rGroupFwd, ACtxDrawMeshVisualizer &rDrawShVisual) noexcept
    {
        Material const &rMat = rScnRender.m_materials[rDrawShVisual.m_materialId];
        sync_drawent_visualizer(rMat.m_dirty.begin(), rMat.m_dirty.end(), rMat.m_ents, rGroupFwd.entities, rDrawShVisual);
    });

    rFB.task()
        .name       ("Resync MeshVisualizer shader DrawEnts")
        .run_on     ({windowApp.pl.resync(Run)})
        .sync_with  ({scnRender.pl.groupEnts(Modify), scnRender.pl.group(Modify)})
        .args       ({            scnRender.di.scnRender,             magnumScn.di.groupFwd,                        shVisual.di.shader})
        .func([] (ACtxSceneRender &rScnRender, RenderGroup &rGroupFwd, ACtxDrawMeshVisualizer &rDrawShVisual) noexcept
    {
        Material const &rMat = rScnRender.m_materials[rDrawShVisual.m_materialId];
        for (DrawEnt const drawEnt : rMat.m_ents)
        {
            sync_drawent_visualizer(drawEnt, rMat.m_ents, rGroupFwd.entities, rDrawShVisual);
        }
    });
}); // ftrShaderVisualizer


FeatureDef const ftrShaderFlat = feature_def("ShaderFlat", [] (
        FeatureBuilder              &rFB,
        Implement<FIShaderFlat>     shFlat,
        DependOn<FIWindowApp>       windowApp,
        DependOn<FIMagnum>          magnum,
        DependOn<FIMagnumScene>     magnumScn,
        DependOn<FISceneRenderer>   scnRender,
        entt::any                   userData)
{
    auto const materialId = userData /* if not null */ ? entt::any_cast<MaterialId>(userData)
                                                       : MaterialId{};

    auto &rScnRender    = rFB.data_get< ACtxSceneRender >    (scnRender.di.scnRender);
    auto &rScnRenderGl  = rFB.data_get< ACtxSceneRenderGL >  (magnumScn.di.scnRenderGl);
    auto &rRenderGl     = rFB.data_get< RenderGL >           (magnum.di.renderGl);

    auto &rDrawFlat = rFB.data_emplace< ACtxDrawFlat >(shFlat.di.shader);

    rDrawFlat.shaderDiffuse       = FlatGL3D{FlatGL3D::Configuration{}.setFlags(FlatGL3D::Flag::Textured)};
    rDrawFlat.shaderUntextured    = FlatGL3D{FlatGL3D::Configuration{}};
    rDrawFlat.materialId          = materialId;
    rDrawFlat.assign_pointers(rScnRender, rScnRenderGl, rRenderGl);

    if (materialId == lgrn::id_null<MaterialId>())
    {
        return;
    }

    // TODO: format after framework changes

    rFB.task()
        .name       ("Sync Flat shader DrawEnts")
        .run_on     ({windowApp.pl.sync(Run)})
        .sync_with  ({scnRender.pl.groupEnts(Modify), scnRender.pl.group(Modify), scnRender.pl.materialDirty(UseOrRun)})
        .args       ({            scnRender.di.scnRender,             magnumScn.di.groupFwd,                         magnumScn.di.scnRenderGl,              shFlat.di.shader})
        .func([] (ACtxSceneRender &rScnRender, RenderGroup &rGroupFwd, ACtxSceneRenderGL const &rScnRenderGl, ACtxDrawFlat &rDrawShFlat) noexcept
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
        .sync_with  ({scnRender.pl.materialDirty(UseOrRun), magnum.pl.textureGL(Ready), scnRender.pl.groupEnts(Modify), scnRender.pl.group(Modify)})
        .args       ({            scnRender.di.scnRender,             magnumScn.di.groupFwd,                         magnumScn.di.scnRenderGl,              shFlat.di.shader})
        .func([] (ACtxSceneRender &rScnRender, RenderGroup &rGroupFwd, ACtxSceneRenderGL const &rScnRenderGl, ACtxDrawFlat &rDrawShFlat) noexcept
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
}); // ftrShaderFlat

FeatureDef const ftrShaderPhong = feature_def("ShaderPhong", [] (
        FeatureBuilder              &rFB,
        Implement<FIShaderPhong>    shPhong,
        DependOn<FIWindowApp>       windowApp,
        DependOn<FIMagnum>          magnum,
        DependOn<FIMagnumScene>     magnumScn,
        DependOn<FISceneRenderer>   scnRender,
        entt::any                   userData)
{
    auto const materialId = userData /* if not null */ ? entt::any_cast<MaterialId>(userData)
                                                       : MaterialId{};

    auto &rScnRender    = rFB.data_get< ACtxSceneRender >    (scnRender.di.scnRender);
    auto &rScnRenderGl  = rFB.data_get< ACtxSceneRenderGL >  (magnumScn.di.scnRenderGl);
    auto &rRenderGl     = rFB.data_get< RenderGL >           (magnum.di.renderGl);

    auto &rDrawPhong = rFB.data_emplace< ACtxDrawPhong >(shPhong.di.shader);

    auto const texturedFlags    = PhongGL::Flag::DiffuseTexture | PhongGL::Flag::AlphaMask | PhongGL::Flag::AmbientTexture;
    rDrawPhong.shaderDiffuse    = PhongGL{PhongGL::Configuration{}.setFlags(texturedFlags).setLightCount(2)};
    rDrawPhong.shaderUntextured = PhongGL{PhongGL::Configuration{}.setLightCount(2)};
    rDrawPhong.materialId       = materialId;
    rDrawPhong.assign_pointers(rScnRender, rScnRenderGl, rRenderGl);

    if (materialId == lgrn::id_null<MaterialId>())
    {
        return;
    }

    // TODO: format after framework changes

    rFB.task()
        .name       ("Sync Phong shader DrawEnts")
        .run_on     ({windowApp.pl.sync(Run)})
        .sync_with  ({scnRender.pl.materialDirty(UseOrRun), magnum.pl.entTextureGL(Ready), scnRender.pl.groupEnts(Modify), scnRender.pl.group(Modify)})
        .args       ({        scnRender.di.scnRender,  magnumScn.di.groupFwd,              magnumScn.di.scnRenderGl,      shPhong.di.shader})
        .func       ([] (ACtxSceneRender &rScnRender, RenderGroup &rGroupFwd, ACtxSceneRenderGL const &rScnRenderGl, ACtxDrawPhong &rShader) noexcept
    {
        Material const &rMat = rScnRender.m_materials[rShader.materialId];
        sync_drawent_phong(rMat.m_dirty.begin(), rMat.m_dirty.end(),
        {
            .hasMaterial    = rMat.m_ents,
            .pStorageOpaque = &rGroupFwd.entities,
            /* TODO: set .pStorageTransparent */
            .opaque         = rScnRender.m_opaque,
            .transparent    = rScnRender.m_transparent,
            .diffuse        = rScnRenderGl.m_diffuseTexId,
            .rData          = rShader
        });
    });

    rFB.task()
        .name       ("Resync Phong shader DrawEnts")
        .run_on     ({windowApp.pl.resync(Run)})
        .sync_with  ({scnRender.pl.materialDirty(UseOrRun), magnum.pl.entTextureGL(Ready), scnRender.pl.groupEnts(Modify), scnRender.pl.group(Modify)})
        .args       ({        scnRender.di.scnRender,  magnumScn.di.groupFwd,              magnumScn.di.scnRenderGl,      shPhong.di.shader})
        .func       ([] (ACtxSceneRender &rScnRender, RenderGroup &rGroupFwd, ACtxSceneRenderGL const &rScnRenderGl, ACtxDrawPhong &rShader) noexcept
    {
        Material const &rMat = rScnRender.m_materials[rShader.materialId];
        for (DrawEnt const drawEnt : rMat.m_ents)
        {
            sync_drawent_phong(drawEnt,
            {
                .hasMaterial    = rMat.m_ents,
                .pStorageOpaque = &rGroupFwd.entities,
                .opaque         = rScnRender.m_opaque,
                .transparent    = rScnRender.m_transparent,
                .diffuse        = rScnRenderGl.m_diffuseTexId,
                .rData          = rShader
            });
        }
    });
}); // ftrShaderPhong




struct ACtxDrawTerrainGL
{
    Magnum::GL::Buffer  vrtxBufGL{Corrade::NoCreate};
    Magnum::GL::Buffer  indxBufGL{Corrade::NoCreate};
    MeshGlId            terrainMeshGl;
    bool                enabled{false};
};

FeatureDef const ftrTerrainDrawMagnum = feature_def("ShaderPhong", [] (
        FeatureBuilder              &rFB,
        Implement<FITerrainDrawMagnum> terrainMgn,
        DependOn<FITerrain>         terrain,
        DependOn<FIWindowApp>       windowApp,
        DependOn<FIMagnum>          magnum,
        DependOn<FIMagnumScene>     magnumScn,
        DependOn<FISceneRenderer>   scnRender)
{
    using namespace planeta;

    auto &rScnRender    = rFB.data_get< ACtxSceneRender >    (scnRender.di.scnRender);
    auto &rScnRenderGl  = rFB.data_get< ACtxSceneRenderGL >  (magnumScn.di.scnRenderGl);
    auto &rRenderGl     = rFB.data_get< RenderGL >           (magnum.di.renderGl);
    auto &rDrawTerrainGl = rFB.data_emplace< ACtxDrawTerrainGL >(terrainMgn.di.drawTerrainGL);

    rDrawTerrainGl.terrainMeshGl = rRenderGl.m_meshIds.create();
    rRenderGl.m_meshGl.emplace(rDrawTerrainGl.terrainMeshGl, Magnum::GL::Mesh{Corrade::NoCreate});

    // TODO: format after framework changes

    rFB.task()
        .name       ("Sync terrainMeshGl to entities with terrainMesh")
        .run_on     ({scnRender.pl.entMeshDirty(UseOrRun)})
        .sync_with  ({scnRender.pl.mesh(Ready), scnRender.pl.entMesh(Ready), magnum.pl.meshGL(Ready), magnum.pl.entMeshGL(Modify), scnRender.pl.drawEntResized(Done)})
        .args       ({              terrainMgn.di.drawTerrainGL,             terrain.di.terrain,      scnRender.di.scnRender,                   magnumScn.di.scnRenderGl,          magnum.di.renderGl })
        .func([] (ACtxDrawTerrainGL &rDrawTerrainGl, ACtxTerrain &rTerrain, ACtxSceneRender &rScnRender, ACtxSceneRenderGL &rScnRenderGl, RenderGL &rRenderGl) noexcept
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
        .sync_with  ({scnRender.pl.mesh(Ready), magnum.pl.meshGL(Ready), magnum.pl.entMeshGL(Modify), scnRender.pl.drawEntResized(Done)})
        .args       ({              terrainMgn.di.drawTerrainGL,             terrain.di.terrain,                 scnRender.di.scnRender,              magnumScn.di.scnRenderGl,          magnum.di.renderGl })
        .func([] (ACtxDrawTerrainGL &rDrawTerrainGl, ACtxTerrain &rTerrain, ACtxSceneRender &rScnRender, ACtxSceneRenderGL &rScnRenderGl, RenderGL &rRenderGl) noexcept
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
        .sync_with  ({terrain.pl.chunkMesh(Ready)})
        .args       ({            scnRender.di.scnRender,             magnumScn.di.groupFwd,                         magnumScn.di.scnRenderGl,          magnum.di.renderGl,                   terrainMgn.di.drawTerrainGL,            terrain.di.terrain})
        .func([] (ACtxSceneRender &rScnRender, RenderGroup &rGroupFwd, ACtxSceneRenderGL const &rScnRenderGl, RenderGL &rRenderGl, ACtxDrawTerrainGL &rDrawTerrainGl, ACtxTerrain &rTerrain) noexcept
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

}); // ftrShaderPhong


} // namespace testapp

