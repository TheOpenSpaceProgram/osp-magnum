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
#include "scenarios.h"
#include "scene_physics.h"
#include "CameraController.h"

#include <osp/Active/basic.h>
#include <osp/Active/drawing.h>
#include <osp/Active/physics.h>

#include <osp/Active/SysHierarchy.h>

#include <osp/tasks/builder.h>
#include <osp/tasks/top_utils.h>

#include <osp/Resource/resources.h>

#include <osp/unpack.h>

#include <Corrade/Containers/ArrayViewStl.h>

#include <newtondynamics_physics/ospnewton.h>
#include <newtondynamics_physics/SysNewton.h>

// TODO: move to renderer

#include <Magnum/GL/DefaultFramebuffer.h>
#include <osp/Active/SysRender.h>
#include <osp/Active/opengl/SysRenderGL.h>
#include <osp/Shaders/MeshVisualizer.h>
#include <osp/Shaders/Flat.h>
#include <osp/Shaders/Phong.h>


using namespace osp::active;
using ospnewton::ACtxNwtWorld;
using osp::input::UserInputHandler;
using osp::phys::EShape;
using osp::TopDataIds_t;
using osp::TopTaskStatus;
using osp::Vector3;
using osp::Matrix3;
using osp::Matrix4;

using osp::top_emplace;
using osp::top_get;
using osp::wrap_args;

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

namespace testapp::scenes
{

constexpr float gc_physTimestep = 1.0 / 60.0f;
constexpr int gc_threadCount = 4; // note: not yet passed to Newton


/**
 * @brief Data used specifically by the physics test scene
 */
struct PhysicsTestData
{
    active_sparse_set_t             m_hasGravity;
    active_sparse_set_t             m_removeOutOfBounds;

    // Timers for when to create boxes and cylinders
    float m_boxTimer{0.0f};
    float m_cylinderTimer{0.0f};

    struct ThrowShape
    {
        Vector3 m_position;
        Vector3 m_velocity;
        Vector3 m_size;
        float m_mass;
        EShape m_shape;
    };

    // Queue for balls to throw
    std::vector<ThrowShape> m_toThrow;
};

void PhysicsTest::setup_scene(MainView mainView, osp::PkgId const pkg, Session const& sceneOut)
{
    auto &rResources = osp::top_get<osp::Resources>(mainView.m_topData, mainView.m_resourcesId);
    auto &rTopData = mainView.m_topData;

    // Add required scene data. This populates rTopData

    auto const [idActiveIds, idBasic, idDrawing, idDrawingRes, idComMats,
                idDelete, idDeleteTotal, idTPhys, idNMeshId, idNwt, idTest,
                idPhong, idPhongDirty, idVisual, idVisualDirty]
               = osp::unpack<15>(sceneOut.m_dataIds);

    auto &rActiveIds    = top_emplace< ActiveReg_t >    (rTopData, idActiveIds);
    auto &rBasic        = top_emplace< ACtxBasic >      (rTopData, idBasic);
    auto &rDrawing      = top_emplace< ACtxDrawing >    (rTopData, idDrawing);
    auto &rDrawingRes   = top_emplace< ACtxDrawingRes > (rTopData, idDrawingRes);
    auto &rDelete       = top_emplace< EntVector_t >    (rTopData, idDelete);
    auto &rDeleteTotal  = top_emplace< EntVector_t >    (rTopData, idDeleteTotal);
    auto &rTPhys        = top_emplace< ACtxTestPhys >   (rTopData, idTPhys);
    auto &rNMesh        = top_emplace< NamedMeshes >    (rTopData, idNMeshId);
    auto &rNwt          = top_emplace< ACtxNwtWorld >   (rTopData, idNwt, gc_threadCount);
    auto &rTest         = top_emplace< PhysicsTestData >(rTopData, idTest);
    auto &rPhong        = top_emplace< EntSet_t >       (rTopData, idPhong);
    auto &rPhongDirty   = top_emplace< EntVector_t >    (rTopData, idPhongDirty);
    auto &rVisual       = top_emplace< EntSet_t >       (rTopData, idVisual);
    auto &rVisualDirty  = top_emplace< EntVector_t >    (rTopData, idVisualDirty);

    auto builder = osp::TaskBuilder{mainView.m_rTasks, mainView.m_rTaskData};


    auto const [tgUpdScene, tgUpdTime, tgUpdSync, tgUpdResync,
                tgStartVisualDirty, tgFactorVisualDirty, tgNeedVisualDirty]
               = osp::unpack<7>(sceneOut.m_tags);

    builder.tag(tgFactorVisualDirty)    .depend_on({tgStartVisualDirty});
    builder.tag(tgNeedVisualDirty)      .depend_on({tgStartVisualDirty, tgFactorVisualDirty});

    builder.task().assign({tgUpdScene, tgStartVisualDirty}).data(
            TopDataIds_t{            idVisualDirty},
            wrap_args([] (EntVector_t& rDirty) noexcept
    {
        rDirty.clear();
    }));

    builder.task().assign({tgUpdResync}).data(
            TopDataIds_t{             idDrawing},
            wrap_args([] (ACtxDrawing& rDrawing) noexcept
    {
        SysRender::set_dirty_all(rDrawing);
    }));

    static auto const resync_material = wrap_args([] (EntSet_t const& hasMaterial, EntVector_t& rDirty) noexcept
    {
        for (std::size_t const entInt : hasMaterial.ones())
        {
            rDirty.push_back(ActiveEnt(entInt));
        }
    });

    builder.task().assign({tgUpdResync}).data(TopDataIds_t{idPhong, idPhongDirty}, resync_material);
    builder.task().assign({tgUpdResync}).data(TopDataIds_t{idVisual, idVisualDirty}, resync_material);


    // Setup the scene

    // Convenient function to get a reference-counted mesh owner
    auto const quick_add_mesh = [&rResources, &rDrawing, &rDrawingRes, pkg] (std::string_view const name) -> MeshIdOwner_t
    {
        osp::ResId const res = rResources.find(osp::restypes::gc_mesh, pkg, name);
        assert(res != lgrn::id_null<osp::ResId>());
        MeshId const meshId = SysRender::own_mesh_resource(rDrawing, rDrawingRes, rResources, res);
        return rDrawing.m_meshRefCounts.ref_add(meshId);
    };

    // Acquire mesh resources from Package
    rNMesh.m_shapeToMesh.emplace(EShape::Box,       quick_add_mesh("cube"));
    rNMesh.m_shapeToMesh.emplace(EShape::Cylinder,  quick_add_mesh("cylinder"));
    rNMesh.m_shapeToMesh.emplace(EShape::Sphere,    quick_add_mesh("sphere"));
    rNMesh.m_namedMeshs.emplace("floor", quick_add_mesh("grid64solid"));

    // Allocate space to fit all materials
    //rDrawing.m_materials.resize(rComMats.m_materialCount);

    // Create hierarchy root entity
    rBasic.m_hierRoot = rActiveIds.create();
    rBasic.m_hierarchy.emplace(rBasic.m_hierRoot);

    // Create camera entity
    ActiveEnt const camEnt = rActiveIds.create();

    // Create camera transform and draw transform
    ACompTransform &rCamTf = rBasic.m_transform.emplace(camEnt);
    rCamTf.m_transform.translation().z() = 25;

    // Create camera component
    ACompCamera &rCamComp = rBasic.m_camera.emplace(camEnt);
    rCamComp.m_far = 1u << 24;
    rCamComp.m_near = 1.0f;
    rCamComp.m_fov = 45.0_degf;

    // Add camera to hierarchy
    SysHierarchy::add_child(
            rBasic.m_hierarchy, rBasic.m_hierRoot, camEnt);

    // start making floor

    static constexpr Vector3 const sc_floorSize{64.0f, 64.0f, 1.0f};
    static constexpr Vector3 const sc_floorPos{0.0f, 0.0f, -1.005f};

    // Create floor root entity
    ActiveEnt const floorRootEnt = rActiveIds.create();

    // Add transform and draw transform to root
    rBasic.m_transform.emplace(
            floorRootEnt, ACompTransform{Matrix4::rotationX(-90.0_degf)});

    // Create floor mesh entity
    ActiveEnt const floorMeshEnt = rActiveIds.create();

    // Add mesh to floor mesh entity
    rDrawing.m_mesh.emplace(floorMeshEnt, quick_add_mesh("grid64solid"));
    rDrawing.m_meshDirty.push_back(floorMeshEnt);

    // Add mesh visualizer material to floor mesh entity
    rVisual.ints().resize(rActiveIds.vec().capacity());
    rVisual.set(std::size_t(floorMeshEnt));
    rVisualDirty.push_back(floorMeshEnt);

    // Add transform, draw transform, opaque, and visible entity
    rBasic.m_transform.emplace(
            floorMeshEnt, ACompTransform{Matrix4::scaling(sc_floorSize)});
    rDrawing.m_opaque.emplace(floorMeshEnt);
    rDrawing.m_visible.emplace(floorMeshEnt);

    // Add floor root to hierarchy root
    SysHierarchy::add_child(
            rBasic.m_hierarchy, rBasic.m_hierRoot, floorRootEnt);

    // Parent floor mesh entity to floor root entity
    SysHierarchy::add_child(
            rBasic.m_hierarchy, floorRootEnt, floorMeshEnt);

    // Add collider to floor root entity (yeah lol it's a big cube)
    Matrix4 const floorTf = Matrix4::scaling(sc_floorSize)
                          * Matrix4::translation(sc_floorPos);
    add_solid_quick({rActiveIds, rBasic, rTPhys, rNMesh, rDrawing}, floorRootEnt, EShape::Box, floorTf,
                    0, 0.0f);

    // Make floor entity a (non-dynamic) rigid body
    rTPhys.m_physics.m_hasColliders.emplace(floorRootEnt);
    rTPhys.m_physics.m_physBody.emplace(floorRootEnt);

}


//-----------------------------------------------------------------------------

struct PhysicsTestControls
{
    PhysicsTestControls(osp::input::UserInputHandler &rInputs)
     :  m_btnThrow(rInputs.button_subscribe("debug_throw"))
    { }

    osp::input::EButtonControlIndex m_btnThrow;
};

void PhysicsTest::setup_renderer_gl(
        MainView        mainView,
        Session const&  appIn,
        Session const&  sceneIn,
        Session const&  magnumIn,
        Session const&  sceneRenderOut) noexcept
{
    using namespace osp::shader;

    auto &rTopData = mainView.m_topData;

    auto const [idActiveApp, idRenderGl, idUserInput] = osp::unpack<3>(magnumIn.m_dataIds);
    auto &rRenderGl     = osp::top_get<RenderGL>(rTopData, idRenderGl);
    auto &rUserInput    = osp::top_get<UserInputHandler>(rTopData, idUserInput);

    auto const [idScnRender, idGroupFwd, idDrawPhong, idDrawVisual, idCamEnt, idCamCtrl, idControls]
            = osp::unpack<7>(sceneRenderOut.m_dataIds);
    auto &rScnRender    = top_emplace< ACtxSceneRenderGL >     (rTopData, idScnRender);
    auto &rGroupFwd     = top_emplace< RenderGroup >           (rTopData, idGroupFwd);
    auto &rDrawPhong    = top_emplace< ACtxDrawPhong >         (rTopData, idDrawPhong);
    auto &rDrawVisual   = top_emplace< ACtxDrawMeshVisualizer >(rTopData, idDrawVisual);
    auto &rCamEnt       = top_emplace< ActiveEnt >             (rTopData, idCamEnt);
    auto &rCamCtrl      = top_emplace< ACtxCameraController >  (rTopData, idCamCtrl, rUserInput);
    auto &rControls     = top_emplace< PhysicsTestControls  >  (rTopData, idControls, rUserInput);

    // Setup Phong shaders
    auto const texturedFlags        = Phong::Flag::DiffuseTexture | Phong::Flag::AlphaMask | Phong::Flag::AmbientTexture;
    rDrawPhong.m_shaderDiffuse      = Phong{texturedFlags, 2};
    rDrawPhong.m_shaderUntextured   = Phong{{}, 2};
    rDrawPhong.assign_pointers(rScnRender, rRenderGl);

    // Setup Mesh Visualizer shader
    rDrawVisual.m_shader = MeshVisualizer{ MeshVisualizer::Flag::Wireframe };
    rDrawVisual.assign_pointers(rScnRender, rRenderGl);

    [[maybe_unused]]
    auto const [idActiveIds, idBasic, idDrawing, idDrawingRes, idComMats,
                idDelete, idDeleteTotal, idTPhys, idNMeshId, idNwt, idTest,
                idPhong, idPhongDirty, idVisual, idVisualDirty]
               = osp::unpack<15>(sceneIn.m_dataIds);
    auto &rBasic = top_get< ACtxBasic >(rTopData, idBasic);

    auto const [idResources] = osp::unpack<1>(appIn.m_dataIds);

    // Select first camera for rendering
    rCamEnt = rBasic.m_camera.at(0);
    rBasic.m_camera.get(rCamEnt).set_aspect_ratio(
            osp::Vector2(Magnum::GL::defaultFramebuffer.viewport().size()));
    SysRender::add_draw_transforms_recurse(
            rBasic.m_hierarchy, rScnRender.m_drawTransform, rCamEnt);

    // Set initial position of camera slightly above the ground
    rCamCtrl.m_target = osp::Vector3{0.0f, 2.0f, 0.0f};

    auto builder = osp::TaskBuilder{mainView.m_rTasks, mainView.m_rTaskData};

    auto const [tgUpdRender, tgUpdInputs, tgUsesGL] = osp::unpack<3>(magnumIn.m_tags);

    auto const [tgUpdScene, tgUpdTime, tgUpdSync,
                tgStartVisualDirty, tgFactorVisualDirty, tgNeedVisualDirty]
               = osp::unpack<6>(sceneIn.m_tags);

    auto const [tgCompileMesh,          tgNeedMesh,
                tgCompileTex,           tgNeedTex,
                tgFactorEntTex,         tgNeedEntTex,
                tgFactorEntMesh,        tgNeedEntMesh,
                tgFactorGroupFwd,       tgNeedGroupFwd,
                tgFactorDrawTransform,  tgNeedDrawTransform]
                = osp::unpack<12>(sceneRenderOut.m_tags);

    builder.tag(tgNeedMesh)             .depend_on({tgCompileMesh});
    builder.tag(tgNeedTex)              .depend_on({tgCompileTex});
    builder.tag(tgNeedEntTex)           .depend_on({tgFactorEntTex});
    builder.tag(tgNeedEntMesh)          .depend_on({tgFactorEntMesh});
    builder.tag(tgNeedGroupFwd)         .depend_on({tgFactorGroupFwd});
    builder.tag(tgNeedDrawTransform)    .depend_on({tgFactorDrawTransform});


    builder.task().assign({tgUpdInputs}).data(
            TopDataIds_t{           idBasic,                      idCamCtrl,                idCamEnt},
            wrap_args([] (ACtxBasic& rBasic, ACtxCameraController& rCamCtrl, ActiveEnt const camEnt) noexcept
    {
                    float delta = 1.0f/60.0f;
        SysCameraController::update_view(rCamCtrl,
                rBasic.m_transform.get(camEnt), delta);
        SysCameraController::update_move(
                rCamCtrl,
                rBasic.m_transform.get(camEnt),
                delta, true);
    }));

    builder.task().assign({tgUpdSync, tgUsesGL, tgCompileMesh, tgCompileTex}).data(
            TopDataIds_t{                      idDrawingRes,                idResources,          idRenderGl},
            wrap_args([] (ACtxDrawingRes const& rDrawingRes, osp::Resources& rResources, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::sync_scene_resources(rDrawingRes, rResources, rRenderGl);
    }));

    builder.task().assign({tgUpdSync, tgNeedTex, tgFactorEntTex}).data(
            TopDataIds_t{             idDrawing,                idDrawingRes,                   idScnRender,          idRenderGl},
            wrap_args([] (ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, ACtxSceneRenderGL& rScnRender, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::assign_textures(rDrawing.m_diffuseTex, rDrawingRes.m_texToRes, rDrawing.m_diffuseDirty, rScnRender.m_diffuseTexId, rRenderGl);
    }));

    builder.task().assign({tgUpdSync, tgNeedMesh, tgFactorEntMesh}).data(
            TopDataIds_t{             idDrawing,                idDrawingRes,                   idScnRender,          idRenderGl},
            wrap_args([] (ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, ACtxSceneRenderGL& rScnRender, RenderGL& rRenderGl) noexcept
    {
        SysRenderGL::assign_meshes(rDrawing.m_mesh, rDrawingRes.m_meshToRes, rDrawing.m_meshDirty, rScnRender.m_meshId, rRenderGl);
    }));

    builder.task().assign({tgUpdSync, tgNeedVisualDirty, tgFactorGroupFwd}).data(
            TopDataIds_t{                   idVisualDirty,          idVisual,               idGroupFwd,                     idDrawVisual},
            wrap_args([] (EntVector_t const& rDirty, EntSet_t const& rMaterial, RenderGroup& rGroup, ACtxDrawMeshVisualizer& rVisualizer) noexcept
    {
        sync_visualizer(std::cbegin(rDirty), std::cend(rDirty), rMaterial, rGroup.m_entities, rVisualizer);
    }));

    // TODO: phong shader

    builder.task().assign({tgUpdSync, tgNeedVisualDirty, tgFactorDrawTransform}).data(
            TopDataIds_t{                 idBasic,                   idVisualDirty,                   idScnRender},
            wrap_args([] (ACtxBasic const& rBasic, EntVector_t const& rVisualDirty, ACtxSceneRenderGL& rScnRender) noexcept
    {
        SysRender::assure_draw_transforms(rBasic.m_hierarchy, rScnRender.m_drawTransform, std::cbegin(rVisualDirty), std::cend(rVisualDirty));
        SysRender::update_draw_transforms(rBasic.m_hierarchy, rBasic.m_transform, rScnRender.m_drawTransform);
    }));

    builder.task().assign({tgUpdRender, tgUsesGL, tgFactorEntTex, tgFactorEntMesh, tgNeedDrawTransform, tgNeedGroupFwd}).data(
            TopDataIds_t{                 idBasic,                   idDrawing,                   idScnRender,          idRenderGl,                   idGroupFwd,                idCamEnt},
            wrap_args([] (ACtxBasic const& rBasic, ACtxDrawing const& rDrawing, ACtxSceneRenderGL& rScnRender, RenderGL& rRenderGl, RenderGroup const& rGroupFwd, ActiveEnt const camEnt) noexcept
    {
        using Magnum::GL::Framebuffer;
        using Magnum::GL::FramebufferClear;
        using Magnum::GL::Texture2D;

        // Bind offscreen FBO
        Framebuffer &rFbo = rRenderGl.m_fbo;
        rFbo.bind();

        // Clear it
        rFbo.clear( FramebufferClear::Color | FramebufferClear::Depth
                    | FramebufferClear::Stencil);

        ACompCamera const &rCamera = rBasic.m_camera.get(camEnt);
        ACompDrawTransform const &cameraDrawTf
                = rScnRender.m_drawTransform.get(camEnt);
        ViewProjMatrix viewProj{
                cameraDrawTf.m_transformWorld.inverted(),
                rCamera.calculate_projection()};

        // Forward Render fwd_opaque group to FBO
        SysRenderGL::render_opaque(rGroupFwd, rDrawing.m_visible, viewProj);

        Texture2D &rFboColor = rRenderGl.m_texGl.get(rRenderGl.m_fboColor);
        SysRenderGL::display_texture(rRenderGl, rFboColor);
    }));
}

} // namespace testapp::physicstest
