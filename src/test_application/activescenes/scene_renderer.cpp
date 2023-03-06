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
#include "scene_renderer.h"
#include "scene_common.h"
#include "scenarios.h"
#include "identifiers.h"
#include "CameraController.h"

#include "../ActiveApplication.h"

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <osp/Active/SysRender.h>
#include <osp/Active/SysSceneGraph.h>
#include <osp/Active/opengl/SysRenderGL.h>
#include <osp/Shaders/MeshVisualizer.h>
#include <osp/Shaders/Flat.h>
#include <osp/Shaders/Phong.h>
#include <osp/universe/universe.h>
#include <osp/universe/coordinates.h>

#include <osp/unpack.h>

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

using namespace osp;
using namespace osp::shader;
using namespace osp::active;
using namespace osp::universe;

using osp::input::UserInputHandler;

using Magnum::GL::Mesh;

namespace testapp::scenes
{

Session setup_magnum_application(
        Builder_t&                      rBuilder,
        ArrayView<entt::any> const      topData,
        Tags&                           rTags,
        TopDataId const                 idResources,
        ActiveApplication::Arguments    args)
{
    Session magnum;
    OSP_SESSION_ACQUIRE_DATA(magnum, topData,   TESTAPP_APP_MAGNUM);
    OSP_SESSION_ACQUIRE_TAGS(magnum, rTags,     TESTAPP_APP_MAGNUM);

    // Order-dependent; ActiveApplication construction starts OpenGL context
    auto &rUserInput    = osp::top_emplace<UserInputHandler>(topData, idUserInput, 12);
    osp::top_emplace<ActiveApplication>                     (topData, idActiveApp, args, rUserInput);
    auto &rRenderGl     = osp::top_emplace<RenderGL>        (topData, idRenderGl);

    config_controls(rUserInput);
    SysRenderGL::setup_context(rRenderGl);

    magnum.task() = rBuilder.task().assign({tgCleanupMagnumEvt, tgGlUse}).data(
            "Clean up Magnum renderer",
            TopDataIds_t{           idResources,          idRenderGl},
            wrap_args([] (Resources& rResources, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::clear_resource_owners(rRenderGl, rResources);
        rRenderGl = {}; // Needs the OpenGL thread for destruction
    }));
    magnum.m_tgCleanupEvt = tgCleanupMagnumEvt;

    return magnum;
}


Session setup_scene_renderer(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              magnum,
        Session const&              scnCommon,
        TopDataId const             idResources)
{
    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(magnum,     TESTAPP_APP_MAGNUM);
    OSP_SESSION_UNPACK_DATA(magnum,     TESTAPP_APP_MAGNUM);

    Session renderer;
    OSP_SESSION_ACQUIRE_DATA(renderer, topData, TESTAPP_COMMON_RENDERER);
    OSP_SESSION_ACQUIRE_TAGS(renderer, rTags,   TESTAPP_COMMON_RENDERER);

    top_emplace< ACtxSceneRenderGL >    (topData, idScnRender);
    top_emplace< RenderGroup >          (topData, idGroupFwd);

    auto &rCamera = top_emplace< Camera >(topData, idCamera);

    rCamera.m_far = 1u << 24;
    rCamera.m_near = 1.0f;
    rCamera.m_fov = Magnum::Deg(45.0f);

    rBuilder.tag(tgDrawGlMod)           .depend_on({tgDrawGlDel});
    rBuilder.tag(tgDrawGlReq)           .depend_on({tgDrawGlDel, tgDrawGlMod});
    rBuilder.tag(tgMeshGlReq)           .depend_on({tgMeshGlMod});
    rBuilder.tag(tgTexGlReq)            .depend_on({tgTexGlMod});
    rBuilder.tag(tgEntTexReq)           .depend_on({tgEntTexMod});
    rBuilder.tag(tgEntMeshReq)          .depend_on({tgEntMeshMod});
    rBuilder.tag(tgCameraReq)           .depend_on({tgCameraMod});
    rBuilder.tag(tgGroupFwdMod)         .depend_on({tgGroupFwdDel});
    rBuilder.tag(tgGroupFwdReq)         .depend_on({tgGroupFwdDel, tgGroupFwdMod});
    rBuilder.tag(tgBindFboReq)          .depend_on({tgBindFboMod});
    rBuilder.tag(tgFwdRenderReq)        .depend_on({tgFwdRenderMod});
    rBuilder.tag(tgDrawTransformNew)    .depend_on({tgDrawTransformDel});
    rBuilder.tag(tgDrawTransformMod)    .depend_on({tgDrawTransformDel, tgDrawTransformNew});
    rBuilder.tag(tgDrawTransformReq)    .depend_on({tgDrawTransformDel, tgDrawTransformNew, tgDrawTransformMod});


    renderer.task() = rBuilder.task().assign({tgSyncEvt, tgGlUse, tgTexGlMod, tgTexGlMod}).data(
            "Synchronize used mesh and texture Resources with GL",
            TopDataIds_t{                      idDrawingRes,                idResources,          idRenderGl},
            wrap_args([] (ACtxDrawingRes const& rDrawingRes, osp::Resources& rResources, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::sync_scene_resources(rDrawingRes, rResources, rRenderGl);
    }));

    renderer.task() = rBuilder.task().assign({tgSyncEvt, tgTexGlReq, tgEntTexMod}).data(
            "Assign GL textures to entities with scene textures",
            TopDataIds_t{             idDrawing,                idDrawingRes,                   idScnRender,          idRenderGl},
            wrap_args([] (ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, ACtxSceneRenderGL& rScnRender, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::assign_textures(rDrawing.m_diffuseTex, rDrawingRes.m_texToRes, rDrawing.m_diffuseDirty, rScnRender.m_diffuseTexId, rRenderGl);
    }));

    renderer.task() = rBuilder.task().assign({tgSyncEvt, tgTexGlReq, tgEntMeshMod, tgMeshReq}).data(
            "Assign GL meshes to entities with scene meshes",
            TopDataIds_t{             idDrawing,                idDrawingRes,                   idScnRender,          idRenderGl},
            wrap_args([] (ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, ACtxSceneRenderGL& rScnRender, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::assign_meshes(rDrawing.m_mesh, rDrawingRes.m_meshToRes, rDrawing.m_meshDirty, rScnRender.m_meshId, rRenderGl);
    }));

    // TODO: Separate forward renderer into different tasks to allow other
    //       rendering techniques

    renderer.task() = rBuilder.task().assign({tgRenderEvt, tgGlUse, tgBindFboMod}).data(
            "Bind Offscreen FBO",
            TopDataIds_t{                   idDrawing,          idRenderGl,                   idGroupFwd,              idCamera},
            wrap_args([] (ACtxDrawing const& rDrawing, RenderGL& rRenderGl, RenderGroup const& rGroupFwd, Camera const& rCamera) noexcept
    {
        using Magnum::GL::Framebuffer;
        using Magnum::GL::FramebufferClear;

        Framebuffer &rFbo = rRenderGl.m_fbo;
        rFbo.bind();

        // Clear it
        rFbo.clear(   FramebufferClear::Color | FramebufferClear::Depth
                    | FramebufferClear::Stencil);
    }));

    renderer.task() = rBuilder.task().assign({tgRenderEvt, tgGlUse, tgBindFboReq, tgFwdRenderReq}).data(
            "Display Offscreen FBO",
            TopDataIds_t{                   idDrawing,          idRenderGl,                   idGroupFwd,              idCamera},
            wrap_args([] (ACtxDrawing const& rDrawing, RenderGL& rRenderGl, RenderGroup const& rGroupFwd, Camera const& rCamera) noexcept
    {
        Magnum::GL::Texture2D &rFboColor = rRenderGl.m_texGl.get(rRenderGl.m_fboColor);
        SysRenderGL::display_texture(rRenderGl, rFboColor);
    }));

    renderer.task() = rBuilder.task().assign({tgRenderEvt, tgGlUse, tgBindFboReq, tgFwdRenderMod, tgDrawTransformReq, tgGroupFwdReq, tgDrawReq, tgCameraReq, tgEntTexMod, tgEntMeshMod}).data(
            "Render Entities",
            TopDataIds_t{                   idDrawing,          idRenderGl,                   idGroupFwd,              idCamera},
            wrap_args([] (ACtxDrawing const& rDrawing, RenderGL& rRenderGl, RenderGroup const& rGroupFwd, Camera const& rCamera) noexcept
    {
        ViewProjMatrix viewProj{rCamera.m_transform.inverted(), rCamera.perspective()};

        // Forward Render fwd_opaque group to FBO
        SysRenderGL::render_opaque(rGroupFwd, rDrawing.m_visible, viewProj);
    }));

    renderer.task() = rBuilder.task().assign({tgSyncEvt, tgDelTotalReq, tgDrawGlDel}).data(
            "Delete GL components",
            TopDataIds_t{                   idScnRender,                   idDelTotal},
            wrap_args([] (ACtxSceneRenderGL& rScnRender, EntVector_t const& rDelTotal) noexcept
    {
        SysRenderGL::update_delete(rScnRender, std::cbegin(rDelTotal), std::cend(rDelTotal));
    }));

    renderer.task() = rBuilder.task().assign({tgSyncEvt, tgHierReq, tgTransformReq, tgDrawTransformMod}).data(
            "Calculate draw transforms",
            TopDataIds_t{                 idBasic,                   idDrawing,                   idScnRender},
            wrap_args([] (ACtxBasic const& rBasic, ACtxDrawing const& rDrawing, ACtxSceneRenderGL& rScnRender) noexcept
    {
        auto rootChildren = SysSceneGraph::children(rBasic.m_scnGraph);
        SysRender::update_draw_transforms(rBasic.m_scnGraph, rBasic.m_transform, rScnRender.m_drawTransform, rDrawing.m_drawable, std::begin(rootChildren), std::end(rootChildren));
    }));

    renderer.task() = rBuilder.task().assign({tgSyncEvt, tgDelTotalReq, tgGroupFwdDel}).data(
            "Delete entities from render groups",
            TopDataIds_t{             idGroupFwd,                idDelTotal},
            wrap_args([] (RenderGroup& rGroup, EntVector_t const& rDelTotal) noexcept
    {
        rGroup.m_entities.remove(std::cbegin(rDelTotal), std::cend(rDelTotal));
    }));

    return renderer;
}

Session setup_shader_visualizer(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              magnum,
        Session const&              scnCommon,
        Session const&              scnRender,
        Session const&              material)
{
    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(magnum,     TESTAPP_APP_MAGNUM);
    OSP_SESSION_UNPACK_TAGS(scnRender,  TESTAPP_COMMON_RENDERER);
    OSP_SESSION_UNPACK_DATA(scnRender,  TESTAPP_COMMON_RENDERER);
    auto &rScnRender    = top_get< ACtxSceneRenderGL >  (topData, idScnRender);
    auto &rRenderGl     = top_get< RenderGL >           (topData, idRenderGl);

    Session shVisual;
    OSP_SESSION_ACQUIRE_DATA(shVisual, topData, TESTAPP_SHADER_VISUALIZER)
    auto &rDrawVisual = top_emplace< ACtxDrawMeshVisualizer >(topData, idDrawShVisual);

    rDrawVisual.m_shader = MeshVisualizer{ MeshVisualizer::Configuration{}.setFlags(MeshVisualizer::Flag::Wireframe) };
    rDrawVisual.assign_pointers(rScnRender, rRenderGl);

    // Default colors
    rDrawVisual.m_shader.setWireframeColor({0.7f, 0.5f, 0.7f, 1.0f});
    rDrawVisual.m_shader.setColor({0.2f, 0.1f, 0.5f, 1.0f});

    if (material.m_dataIds.empty())
    {
        return shVisual;
    }
    OSP_SESSION_UNPACK_TAGS(material,   TESTAPP_MATERIAL);
    OSP_SESSION_UNPACK_DATA(material,   TESTAPP_MATERIAL);

    shVisual.task() = rBuilder.task().assign({tgSyncEvt, tgMatReq, tgGroupFwdMod}).data(
            "Sync MeshVisualizer shader entities",
            TopDataIds_t{                   idMatDirty,                idMatEnts,             idGroupFwd,                     idDrawShVisual},
            wrap_args([] (EntVector_t const& rMatDirty, EntSet_t const& rMatEnts, RenderGroup& rGroup, ACtxDrawMeshVisualizer& rDrawShVisual) noexcept
    {
        sync_visualizer(std::cbegin(rMatDirty), std::cend(rMatDirty), rMatEnts, rGroup.m_entities, rDrawShVisual);
    }));

    shVisual.task() = rBuilder.task().assign({tgSyncEvt, tgMatReq, tgTransformReq, tgDrawTransformNew}).data(
            "Add draw transforms to mesh visualizer",
            TopDataIds_t{                   idMatDirty,                   idScnRender},
            wrap_args([] (EntVector_t const& rMatDirty, ACtxSceneRenderGL& rScnRender) noexcept
    {
        SysRender::assure_draw_transforms(rScnRender.m_drawTransform, std::cbegin(rMatDirty), std::cend(rMatDirty));
    }));


    return shVisual;
}


Session setup_shader_flat(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              magnum,
        Session const&              scnCommon,
        Session const&              scnRender,
        Session const&              material)
{
    using osp::shader::Flat;

    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(magnum,     TESTAPP_APP_MAGNUM);
    OSP_SESSION_UNPACK_TAGS(scnRender,  TESTAPP_COMMON_RENDERER);
    OSP_SESSION_UNPACK_DATA(scnRender,  TESTAPP_COMMON_RENDERER);
    auto &rScnRender    = top_get< ACtxSceneRenderGL >  (topData, idScnRender);
    auto &rRenderGl     = top_get< RenderGL >           (topData, idRenderGl);

    Session shFlat;
    OSP_SESSION_ACQUIRE_DATA(shFlat, topData, TESTAPP_SHADER_FLAT)
    auto &rDrawFlat = top_emplace< ACtxDrawFlat >(topData, idDrawShFlat);

    rDrawFlat.m_shaderDiffuse      = Flat{Flat::Configuration{}.setFlags(Flat::Flag::Textured)};
    rDrawFlat.m_shaderUntextured   = Flat{Flat::Configuration{}};
    rDrawFlat.assign_pointers(rScnRender, rRenderGl);

    if (material.m_dataIds.empty())
    {
        return shFlat;
    }
    OSP_SESSION_UNPACK_TAGS(material,   TESTAPP_MATERIAL);
    OSP_SESSION_UNPACK_DATA(material,   TESTAPP_MATERIAL);

    shFlat.task() = rBuilder.task().assign({tgSyncEvt, tgMatReq, tgDrawReq, tgTexGlReq, tgGroupFwdMod}).data(
            "Sync Flat shader entities",
            TopDataIds_t{                   idMatDirty,                idMatEnts,                   idDrawing,                         idScnRender,             idGroupFwd,               idDrawShFlat},
            wrap_args([] (EntVector_t const& rMatDirty, EntSet_t const& rMatEnts, ACtxDrawing const& rDrawing, ACtxSceneRenderGL const& rScnRender, RenderGroup& rGroupFwd, ACtxDrawFlat& rDrawShFlat) noexcept
    {
        sync_flat(std::cbegin(rMatDirty), std::cend(rMatDirty), rMatEnts, &rGroupFwd.m_entities, nullptr, rDrawing.m_opaque, rScnRender.m_diffuseTexId, rDrawShFlat);
    }));

    shFlat.task() = rBuilder.task().assign({tgSyncEvt, tgMatReq, tgTransformReq, tgDrawTransformNew}).data(
            "Add draw transforms to entities with Phong shader",
            TopDataIds_t{                   idMatDirty,                   idScnRender},
            wrap_args([] (EntVector_t const& rMatDirty, ACtxSceneRenderGL& rScnRender) noexcept
    {
        SysRender::assure_draw_transforms(rScnRender.m_drawTransform, std::cbegin(rMatDirty), std::cend(rMatDirty));
    }));

    return shFlat;
}


Session setup_shader_phong(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              magnum,
        Session const&              scnCommon,
        Session const&              scnRender,
        Session const&              material)
{
    using osp::shader::Phong;

    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(magnum,     TESTAPP_APP_MAGNUM);
    OSP_SESSION_UNPACK_TAGS(scnRender,  TESTAPP_COMMON_RENDERER);
    OSP_SESSION_UNPACK_DATA(scnRender,  TESTAPP_COMMON_RENDERER);
    auto &rScnRender    = top_get< ACtxSceneRenderGL >  (topData, idScnRender);
    auto &rRenderGl     = top_get< RenderGL >           (topData, idRenderGl);

    Session shPhong;
    OSP_SESSION_ACQUIRE_DATA(shPhong, topData, TESTAPP_SHADER_PHONG)
    auto &rDrawPhong = top_emplace< ACtxDrawPhong >(topData, idDrawShPhong);


    auto const texturedFlags        = Phong::Flag::DiffuseTexture | Phong::Flag::AlphaMask | Phong::Flag::AmbientTexture;
    rDrawPhong.m_shaderDiffuse      = Phong{Phong::Configuration{}.setFlags(texturedFlags).setLightCount(2)};
    rDrawPhong.m_shaderUntextured   = Phong{Phong::Configuration{}.setLightCount(2)};
    rDrawPhong.assign_pointers(rScnRender, rRenderGl);

    if (material.m_dataIds.empty())
    {
        return shPhong;
    }
    OSP_SESSION_UNPACK_TAGS(material,   TESTAPP_MATERIAL);
    OSP_SESSION_UNPACK_DATA(material,   TESTAPP_MATERIAL);

    shPhong.task() = rBuilder.task().assign({tgSyncEvt, tgMatReq, tgDrawReq, tgTexGlReq, tgGroupFwdMod}).data(
            "Sync Phong shader entities",
            TopDataIds_t{                   idMatDirty,                idMatEnts,                   idDrawing,                         idScnRender,             idGroupFwd,               idDrawShPhong},
            wrap_args([] (EntVector_t const& rMatDirty, EntSet_t const& rMatEnts, ACtxDrawing const& rDrawing, ACtxSceneRenderGL const& rScnRender, RenderGroup& rGroupFwd, ACtxDrawPhong& rDrawShPhong) noexcept
    {
        sync_phong(std::cbegin(rMatDirty), std::cend(rMatDirty), rMatEnts, &rGroupFwd.m_entities, nullptr, rDrawing.m_opaque, rScnRender.m_diffuseTexId, rDrawShPhong);
    }));

    shPhong.task() = rBuilder.task().assign({tgSyncEvt, tgMatReq, tgTransformReq, tgDrawTransformNew}).data(
            "Add draw transforms to entities with Phong shader",
            TopDataIds_t{                   idMatDirty,                   idScnRender},
            wrap_args([] (EntVector_t const& rMatDirty, ACtxSceneRenderGL& rScnRender) noexcept
    {
        SysRender::assure_draw_transforms(rScnRender.m_drawTransform, std::cbegin(rMatDirty), std::cend(rMatDirty));
    }));

    return shPhong;
}

struct Cursor
{
    Magnum::Color4 m_color;
    MeshIdOwner_t m_mesh;
};

Session setup_cursor(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              magnum,
        Session const&              scnCommon,
        Session const&              scnRender,
        Session const&              cameraCtrl,
        Session const&              shFlat,
        TopDataId const             idResources,
        PkgId const                 pkg)
{
    OSP_SESSION_UNPACK_TAGS(magnum,         TESTAPP_APP_MAGNUM);
    OSP_SESSION_UNPACK_DATA(magnum,         TESTAPP_APP_MAGNUM);
    OSP_SESSION_UNPACK_TAGS(scnRender,      TESTAPP_COMMON_RENDERER);
    OSP_SESSION_UNPACK_DATA(scnRender,      TESTAPP_COMMON_RENDERER);
    OSP_SESSION_UNPACK_TAGS(scnCommon,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(scnCommon,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(cameraCtrl,     TESTAPP_CAMERA_CTRL);
    OSP_SESSION_UNPACK_TAGS(cameraCtrl,     TESTAPP_CAMERA_CTRL);
    OSP_SESSION_UNPACK_DATA(shFlat,         TESTAPP_SHADER_FLAT);

    auto &rResources    = top_get< Resources >      (topData, idResources);
    auto &rBasic        = top_get< ACtxBasic >      (topData, idBasic);
    auto &rActiveIds    = top_get< ActiveReg_t >    (topData, idActiveIds);
    auto &rDrawing      = top_get< ACtxDrawing >    (topData, idDrawing);
    auto &rDrawingRes   = top_get< ACtxDrawingRes > (topData, idDrawingRes);

    Session cursor;
    OSP_SESSION_ACQUIRE_DATA(cursor, topData, TESTAPP_CURSOR);
    cursor.m_tgCleanupEvt = tgCleanupMagnumEvt;

    auto &rCursorData   = top_emplace<Cursor>(topData, idCursorData);
    rCursorData.m_color = { 0.0f, 1.0f, 0.0f, 1.0f };
    rCursorData.m_mesh  = SysRender::add_drawable_mesh(rDrawing, rDrawingRes, rResources, pkg, "cubewire");

    cursor.task() = rBuilder.task().assign({tgRenderEvt, tgGlUse, tgBindFboReq, tgFwdRenderMod, tgCamCtrlReq}).data(
            "Render cursor",
            TopDataIds_t{          idRenderGl,              idCamera,                      idDrawingRes,              idDrawShFlat,                             idCamCtrl,        idCursorData },
            wrap_args([] (RenderGL& rRenderGl, Camera const& rCamera, ACtxDrawingRes const& rDrawingRes, ACtxDrawFlat& rDrawShFlat,  ACtxCameraController const& rCamCtrl, Cursor& rCursorData) noexcept
    {
        ResId const     cursorResId     = rDrawingRes.m_meshToRes.at(rCursorData.m_mesh.value());
        MeshGlId const  cursorMeshGlId  = rRenderGl.m_resToMesh.at(cursorResId);
        Mesh&           rCursorMeshGl   = rRenderGl.m_meshGl.get(cursorMeshGlId);

        ViewProjMatrix viewProj{rCamera.m_transform.inverted(), rCamera.perspective()};

        auto const matrix = viewProj.m_viewProj * Matrix4::translation(rCamCtrl.m_target.value());

        rDrawShFlat.m_shaderUntextured
            .setColor(rCursorData.m_color)
            .setTransformationProjectionMatrix(matrix)
            .draw(rCursorMeshGl);
    }));

    cursor.task() = rBuilder.task().assign({tgCleanupMagnumEvt}).data(
            "Clean up cursor resource owners",
            TopDataIds_t{             idDrawing,        idCursorData},
            wrap_args([] (ACtxDrawing& rDrawing, Cursor& rCursorData) noexcept
    {
        rDrawing.m_meshRefCounts.ref_release(std::move(rCursorData.m_mesh));
    }));

    return cursor;
}


Session setup_uni_test_planets_renderer(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              magnum,
        Session const&              scnRender,
        Session const&              scnCommon,
        Session const&              cameraCtrl,
        Session const&              visualizer,
        Session const&              uniCore,
        Session const&              uniScnFrame,
        Session const&              uniTestPlanets)
{
    Session uniTestPlanetsRdr;

    OSP_SESSION_UNPACK_TAGS(magnum,         TESTAPP_APP_MAGNUM);
    OSP_SESSION_UNPACK_DATA(magnum,         TESTAPP_APP_MAGNUM);
    OSP_SESSION_UNPACK_TAGS(scnRender,      TESTAPP_COMMON_RENDERER);
    OSP_SESSION_UNPACK_DATA(scnRender,      TESTAPP_COMMON_RENDERER);
    OSP_SESSION_UNPACK_TAGS(scnCommon,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(scnCommon,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(cameraCtrl,     TESTAPP_CAMERA_CTRL);
    OSP_SESSION_UNPACK_TAGS(cameraCtrl,     TESTAPP_CAMERA_CTRL);
    OSP_SESSION_UNPACK_DATA(visualizer,     TESTAPP_SHADER_VISUALIZER);
    OSP_SESSION_UNPACK_TAGS(uniCore,        TESTAPP_UNI_CORE);
    OSP_SESSION_UNPACK_DATA(uniCore,        TESTAPP_UNI_CORE);
    OSP_SESSION_UNPACK_TAGS(uniScnFrame,    TESTAPP_UNI_SCENEFRAME);
    OSP_SESSION_UNPACK_DATA(uniScnFrame,    TESTAPP_UNI_SCENEFRAME);
    OSP_SESSION_UNPACK_DATA(uniTestPlanets, TESTAPP_UNI_PLANETS);

    //OSP_SESSION_ACQUIRE_DATA(uniTestPlanets, topData, TESTAPP_UNI_PLANETS);

    uniTestPlanetsRdr.task() = rBuilder.task().assign({tgRenderEvt, tgCamCtrlReq}).data(
            "Position SceneFrame center Camera Controller",
            TopDataIds_t{                            idCamCtrl,            idScnFrame},
            wrap_args([] (ACtxCameraController const& rCamCtrl, SceneFrame& rScnFrame) noexcept
    {
        rScnFrame.m_scenePosition = Vector3g(math::mul_2pow<Vector3, int>( rCamCtrl.m_target.value(), rScnFrame.m_precision));
    }));

    uniTestPlanetsRdr.task() = rBuilder.task().assign({tgRenderEvt, tgGlUse, tgBindFboReq, tgFwdRenderMod, tgDrawReq, tgCameraReq}).data(
            "Render test planets",
            TopDataIds_t{          idRenderGl,              idCamera,                        idDrawShVisual,                      idDrawingRes,             idNMesh,          idUniverse,                  idScnFrame,               idPlanetMainSpace},
            wrap_args([] (RenderGL& rRenderGl, Camera const& rCamera, ACtxDrawMeshVisualizer& rDrawShVisual, ACtxDrawingRes const& rDrawingRes, NamedMeshes& rNMesh, Universe& rUniverse, SceneFrame const& rScnFrame, CoSpaceId const planetMainSpace) noexcept
    {
        CoSpaceCommon &rMainSpace = rUniverse.m_coordCommon[planetMainSpace];
        auto const [x, y, z]        = sat_views(rMainSpace.m_satPositions, rMainSpace.m_data, rMainSpace.m_satCount);
        auto const [qx, qy, qz, qw] = sat_views(rMainSpace.m_satRotations, rMainSpace.m_data, rMainSpace.m_satCount);

        // Calculate transform from universe to area/local-space for rendering.
        // This can be generalized by finding a common ancestor within the tree
        // of coordinate spaces. Since there's only two possibilities, an if
        // statement works.
        CoordTransformer mainToArea;
        if (rScnFrame.m_parent == planetMainSpace)
        {
            mainToArea = coord_parent_to_child(rMainSpace, rScnFrame);
        }
        else
        {
            CoSpaceId const landedId = rScnFrame.m_parent;
            CoSpaceCommon &rLanded = rUniverse.m_coordCommon[landedId];

            CoSpaceTransform const landedTf     = coord_get_transform(rLanded, rLanded, x, y, z, qx, qy, qz, qw);
            CoordTransformer const mainToLanded = coord_parent_to_child(rMainSpace, landedTf);
            CoordTransformer const landedToArea = coord_parent_to_child(landedTf, rScnFrame);

            mainToArea = coord_composite(landedToArea, mainToLanded);
        }
        Quaternion const mainToAreaRot{mainToArea.rotation()};

        float const scale = math::mul_2pow<float, int>(1.0f, -rMainSpace.m_precision);

        ViewProjMatrix viewProj{rCamera.m_transform.inverted(), rCamera.perspective()};

        MeshId const    sphereMeshId    = rNMesh.m_shapeToMesh[phys::EShape::Sphere];
        ResId const     sphereResId     = rDrawingRes.m_meshToRes.at(sphereMeshId);
        MeshGlId const  sphereMeshGlId  = rRenderGl.m_resToMesh.at(sphereResId);
        Mesh&           rSphereMeshGl   = rRenderGl.m_meshGl.get(sphereMeshGlId);

        using Magnum::GL::Renderer;
        Renderer::enable(Renderer::Feature::DepthTest);
        Renderer::enable(Renderer::Feature::FaceCulling);
        Renderer::disable(Renderer::Feature::Blending);
        //Renderer::setDepthMask(true);

        // Draw black hole
        Vector3 const blackHolePos = Vector3(mainToArea.transform_position({})) * scale;
        rDrawShVisual.m_shader
            .setTransformationMatrix(
                    viewProj.m_view
                    * Matrix4::translation(blackHolePos)
                    * Matrix4::scaling({200, 200, 200})
                    * Matrix4{mainToAreaRot.toMatrix()} )
            .draw(rSphereMeshGl);

        // Draw planets
        for (std::size_t i = 0; i < rMainSpace.m_satCount; ++i)
        {
            Vector3g const relative = mainToArea.transform_position({x[i], y[i], z[i]});
            Vector3 const relativeMeters = Vector3(relative) * scale;

            Quaterniond const rot{{qx[i], qy[i], qz[i]}, qw[i]};

            rDrawShVisual.m_shader
                .setTransformationMatrix(
                        viewProj.m_view
                        * Matrix4::translation(relativeMeters)
                        * Matrix4::scaling({200, 200, 200})
                        * Matrix4{(mainToAreaRot * Quaternion{rot}).toMatrix()} )
                .draw(rSphereMeshGl);
        }

    }));

    return uniTestPlanetsRdr;
}

} // namespace testapp::scenes
