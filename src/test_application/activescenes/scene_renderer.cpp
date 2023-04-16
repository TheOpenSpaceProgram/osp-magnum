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

#include <osp/Active/parts.h>
#include <osp/Active/SysRender.h>
#include <osp/Active/SysSceneGraph.h>
#include <osp/Active/opengl/SysRenderGL.h>
#include <osp/Shaders/MeshVisualizer.h>
#include <osp/Shaders/Flat.h>
#include <osp/Shaders/Phong.h>
#include <osp/universe/universe.h>
#include <osp/universe/coordinates.h>

#include <adera/machines/links.h>

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

    renderer.task() = rBuilder.task().assign({tgSyncEvt, tgTexGlMod, tgMeshGlMod, tgDrawGlMod, tgDrawReq}).data(
            "Resize Scene Render containers to fit drawable entities",
            TopDataIds_t{                   idDrawing,                   idScnRender},
            wrap_args([] (ACtxDrawing const& rDrawing, ACtxSceneRenderGL& rScnRender) noexcept
    {
        std::size_t const capacity = rDrawing.m_drawIds.capacity();
        rScnRender.m_drawTransform  .resize(capacity);
        rScnRender.m_diffuseTexId   .resize(capacity);
        rScnRender.m_meshId         .resize(capacity);
    }));

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

    renderer.task() = rBuilder.task().assign({tgRenderEvt, tgGlUse, tgBindFboReq, tgFwdRenderMod, tgDrawTransformReq, tgGroupFwdReq, tgDrawGlReq, tgCameraReq, tgEntTexMod, tgEntMeshMod}).data(
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
        SysRender::update_draw_transforms(
                    rBasic.m_scnGraph,
                    rDrawing.m_activeToDraw,
                    rBasic.m_transform,
                    rScnRender.m_drawTransform,
                    rDrawing.m_needDrawTf,
                    std::begin(rootChildren),
                    std::end(rootChildren));
    }));

    renderer.task() = rBuilder.task().assign({tgSyncEvt, tgGroupFwdDel, tgDelDrawEntReq}).data(
            "Delete entities from render groups",
            TopDataIds_t{                   idDrawing,             idGroupFwd,                    idDelDrawEnts},
            wrap_args([] (ACtxDrawing const& rDrawing, RenderGroup& rGroup, DrawEntVector_t const& rDelDrawEnts) noexcept
    {
        for (DrawEnt const drawEnt : rDelDrawEnts)
        {
            rGroup.m_entities.remove(drawEnt);
        }
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
            TopDataIds_t{                            idMatDirty,                idMatEnts,             idGroupFwd,                     idDrawShVisual},
            wrap_args([] (std::vector<DrawEnt> const& rMatDirty, EntSet_t const& rMatEnts, RenderGroup& rGroup, ACtxDrawMeshVisualizer& rDrawShVisual) noexcept
    {
        sync_visualizer(std::cbegin(rMatDirty), std::cend(rMatDirty), rMatEnts, rGroup.m_entities, rDrawShVisual);
    }));

    shVisual.task() = rBuilder.task().assign({tgSyncEvt, tgMatReq, tgTransformReq, tgDrawTransformNew}).data(
            "Add draw transforms to mesh visualizer",
            TopDataIds_t{                   idMatDirty,                   idScnRender},
            wrap_args([] (std::vector<DrawEnt> const& rMatDirty, ACtxSceneRenderGL& rScnRender) noexcept
    {
        //SysRender::assure_draw_transforms(rScnRender.m_drawTransform, std::cbegin(rMatDirty), std::cend(rMatDirty));
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
            TopDataIds_t{                            idMatDirty,                idMatEnts,                   idDrawing,                         idScnRender,             idGroupFwd,               idDrawShFlat},
            wrap_args([] (std::vector<DrawEnt> const& rMatDirty, EntSet_t const& rMatEnts, ACtxDrawing const& rDrawing, ACtxSceneRenderGL const& rScnRender, RenderGroup& rGroupFwd, ACtxDrawFlat& rDrawShFlat) noexcept
    {
        sync_flat(std::cbegin(rMatDirty), std::cend(rMatDirty), rMatEnts, &rGroupFwd.m_entities, nullptr, rDrawing.m_drawBasic, rScnRender.m_diffuseTexId, rDrawShFlat);
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
            TopDataIds_t{                            idMatDirty,                idMatEnts,                   idDrawing,                         idScnRender,             idGroupFwd,               idDrawShPhong},
            wrap_args([] (std::vector<DrawEnt> const& rMatDirty, EntSet_t const& rMatEnts, ACtxDrawing const& rDrawing, ACtxSceneRenderGL const& rScnRender, RenderGroup& rGroupFwd, ACtxDrawPhong& rDrawShPhong) noexcept
    {
        sync_phong(std::cbegin(rMatDirty), std::cend(rMatDirty), rMatEnts, &rGroupFwd.m_entities, nullptr, rDrawing.m_drawBasic, rScnRender.m_diffuseTexId, rDrawShPhong);
    }));

    return shPhong;
}

struct IndicatorMesh
{
    Magnum::Color4 m_color;
    MeshIdOwner_t m_mesh;
};


Session setup_thrust_indicators(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              magnum,
        Session const&              scnCommon,
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
    OSP_SESSION_UNPACK_TAGS(scnCommon,      TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(scnCommon,      TESTAPP_COMMON_SCENE);
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
    OSP_SESSION_ACQUIRE_DATA(cursor, topData, TESTAPP_INDICATOR);
    cursor.m_tgCleanupEvt = tgCleanupMagnumEvt;

    auto &rCursorData   = top_emplace<IndicatorMesh>(topData, idIndicator);
    rCursorData.m_color = { 0.0f, 1.0f, 0.0f, 1.0f };
    rCursorData.m_mesh  = SysRender::add_drawable_mesh(rDrawing, rDrawingRes, rResources, pkg, "cubewire");

    cursor.task() = rBuilder.task().assign({tgRenderEvt, tgGlUse, tgBindFboReq, tgFwdRenderMod, tgCamCtrlReq}).data(
            "Render cursor",
            TopDataIds_t{          idRenderGl,              idCamera,                      idDrawingRes,              idDrawShFlat,                             idCamCtrl,               idIndicator },
            wrap_args([] (RenderGL& rRenderGl, Camera const& rCamera, ACtxDrawingRes const& rDrawingRes, ACtxDrawFlat& rDrawShFlat,  ACtxCameraController const& rCamCtrl, IndicatorMesh& rIndicator) noexcept
    {
        ResId const     cursorResId     = rDrawingRes.m_meshToRes.at(rIndicator.m_mesh.value());
        MeshGlId const  cursorMeshGlId  = rRenderGl.m_resToMesh.at(cursorResId);
        Mesh&           rCursorMeshGl   = rRenderGl.m_meshGl.get(cursorMeshGlId);

        ViewProjMatrix viewProj{rCamera.m_transform.inverted(), rCamera.perspective()};

        auto const matrix = viewProj.m_viewProj * Matrix4::translation(rCamCtrl.m_target.value());

        rDrawShFlat.m_shaderUntextured
            .setColor(rIndicator.m_color)
            .setTransformationProjectionMatrix(matrix)
            .draw(rCursorMeshGl);
    }));

    cursor.task() = rBuilder.task().assign({tgCleanupMagnumEvt}).data(
            "Clean up cursor resource owners",
            TopDataIds_t{             idDrawing,               idIndicator},
            wrap_args([] (ACtxDrawing& rDrawing, IndicatorMesh& rIndicator) noexcept
    {
        rDrawing.m_meshRefCounts.ref_release(std::move(rIndicator.m_mesh));
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

    uniTestPlanetsRdr.task() = rBuilder.task().assign({tgRenderEvt, tgScnFramePosMod, tgCamCtrlMod}).data(
            "Position SceneFrame center Camera Controller",
            TopDataIds_t{                      idCamCtrl,            idScnFrame},
            wrap_args([] (ACtxCameraController& rCamCtrl, SceneFrame& rScnFrame) noexcept
    {
        if ( ! rCamCtrl.m_target.has_value())
        {
            return;
        }
        Vector3 &rCamTgt = rCamCtrl.m_target.value();

        // check origin translation
        // ADL used for Magnum::Math::sign/floor/abs
        float const maxDist = 512.0f;
        Vector3 const translate = sign(rCamTgt) * floor(abs(rCamTgt) / maxDist) * maxDist;

        if ( ! translate.isZero())
        {
            rCamCtrl.m_transform.translation() -= translate;
            rCamTgt -= translate;

            // a bit janky to modify universe stuff directly here, but it works lol
            Vector3 const rotated = Quaternion(rScnFrame.m_rotation).transformVector(translate);
            rScnFrame.m_position += Vector3g(math::mul_2pow<Vector3, int>(rotated, rScnFrame.m_precision));
        }

        rScnFrame.m_scenePosition = Vector3g(math::mul_2pow<Vector3, int>(rCamCtrl.m_target.value(), rScnFrame.m_precision));

    }));

    uniTestPlanetsRdr.task() = rBuilder.task().assign({tgRenderEvt, tgGlUse, tgBindFboReq, tgFwdRenderMod, tgDrawReq, tgCameraReq, tgScnFramePosReq}).data(
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
        Renderer::setDepthMask(true);

        // Draw center indicator
        Vector3 const centerPos = Vector3(mainToArea.transform_position({})) * scale;
        rDrawShVisual.m_shader
            .setTransformationMatrix(
                    viewProj.m_view
                    * Matrix4::translation(centerPos)
                    * Matrix4{mainToAreaRot.toMatrix()}
                    * Matrix4::scaling({500, 50, 50}))
            .draw(rSphereMeshGl)
            .setTransformationMatrix(
                    viewProj.m_view
                    * Matrix4::translation(centerPos)
                    * Matrix4{mainToAreaRot.toMatrix()}
                    * Matrix4::scaling({50, 500, 50}))
            .draw(rSphereMeshGl)
            .setTransformationMatrix(
                    viewProj.m_view
                    * Matrix4::translation(centerPos)
                    * Matrix4{mainToAreaRot.toMatrix()}
                    * Matrix4::scaling({50, 50, 500}) )
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
