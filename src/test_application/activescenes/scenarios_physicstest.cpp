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
#include "common_scene.h"
#include "common_renderer_gl.h"
#include "CameraController.h"

#include "../ActiveApplication.h"

#include <osp/Active/basic.h>
#include <osp/Active/drawing.h>
#include <osp/Active/physics.h>

#include <osp/Active/SysHierarchy.h>

#include <osp/tasks/worker_utils.h>

#include <osp/Resource/resources.h>

#include <longeron/id_management/registry_stl.hpp>

#include <Magnum/GL/DefaultFramebuffer.h>

#include <Corrade/Containers/ArrayViewStl.h>

#include <newtondynamics_physics/ospnewton.h>
#include <newtondynamics_physics/SysNewton.h>

using osp::active::ActiveEnt;
using osp::active::active_sparse_set_t;

using osp::phys::EShape;

using osp::Vector3;
using osp::Matrix3;
using osp::Matrix4;

using osp::active::MeshIdOwner_t;

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

void PhysicsTest::setup_scene(MainView mainView, osp::PkgId pkg, osp::MainDataIdSpan_t dataOut, osp::TagSpan_t tagsOut)
{
    using namespace osp::active;
    using ospnewton::ACtxNwtWorld;
    using osp::main_emplace;
    using osp::main_get;

    auto &rResources = main_get<osp::Resources>(mainView.m_rMainData, mainView.m_resourcesId);
    osp::MainDataSpan_t &rMainData = mainView.m_rMainData;

    // Add required scene data. This populates rMainData

    osp::MainDataIt_t itCurr = std::begin(mainView.m_rMainData);

    auto [rActiveIds, idActiveIds]      = main_emplace< lgrn::IdRegistryStl<ActiveEnt> >    (rMainData, itCurr);
    auto [rBasic, idBasic]              = main_emplace< ACtxBasic >                         (rMainData, itCurr);
    auto [rDrawing, idDrawing]          = main_emplace< ACtxDrawing >                       (rMainData, itCurr);
    auto [rDrawingRes, idDrawingRes]    = main_emplace< ACtxDrawingRes >                    (rMainData, itCurr);
    auto [rComMats, idComMats]          = main_emplace< CommonMaterials >                   (rMainData, itCurr);
    auto [rDelete, idDelete]            = main_emplace< std::vector<ActiveEnt> >            (rMainData, itCurr);
    auto [rDeleteTotal, idDeleteTotal]  = main_emplace< std::vector<ActiveEnt> >            (rMainData, itCurr);
    auto [rTPhys, idTPhys]              = main_emplace< ACtxTestPhys >                      (rMainData, itCurr);
    auto [rNMesh, idNMeshId]            = main_emplace< NamedMeshes >                       (rMainData, itCurr);
    auto [rNwt, idNwt]                  = main_emplace< ACtxNwtWorld >                      (rMainData, itCurr, gc_threadCount);
    auto [rTest, idTest]                = main_emplace< PhysicsTestData >                   (rMainData, itCurr);

    // Write IDs of data to dataOut
    auto const dataOutInit = {idActiveIds, idBasic, idDrawing, idDrawingRes, idComMats, idDelete, idDeleteTotal, idTPhys, idNMeshId, idNwt, idTest};
    std::copy(std::begin(dataOutInit), std::end(dataOutInit), std::begin(dataOut));

    // Setup the scene

    // Convenient function to get a reference-counted mesh owner
    auto const quick_add_mesh = [&rResources, &rDrawing = rDrawing, &rDrawingRes = rDrawingRes, pkg] (std::string_view const name) -> MeshIdOwner_t
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
    rDrawing.m_materials.resize(rComMats.m_materialCount);

    // Create hierarchy root entity
    rBasic.m_hierRoot = rActiveIds.create();
    rBasic.m_hierarchy.emplace(rBasic.m_hierRoot);

    // Create camera entity
    ActiveEnt camEnt = rActiveIds.create();

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
    MaterialData &rMatCommon
            = rDrawing.m_materials[rComMats.m_matVisualizer];
    rMatCommon.m_comp.emplace(floorMeshEnt);
    rMatCommon.m_added.push_back(floorMeshEnt);

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
                    rComMats.m_matCommon, 0.0f);

    // Make floor entity a (non-dynamic) rigid body
    rTPhys.m_physics.m_hasColliders.emplace(floorRootEnt);
    rTPhys.m_physics.m_physBody.emplace(floorRootEnt);

}

static void update_test_scene_delete(CommonTestScene &rScene)
{
    using namespace osp::active;

    /*
    auto &rScnTest  = rScene.get<PhysicsTestData>();
    auto &rScnPhys  = rScene.get<PhysicsData>();
    auto &rScnNwt   = rScene.get<ospnewton::ACtxNwtWorld>();

    rScene.update_hierarchy_delete();

    auto const& first = std::cbegin(rScene.m_deleteTotal);
    auto const& last = std::cend(rScene.m_deleteTotal);

    // Delete components of total entities to delete
    SysPhysics::update_delete_phys      (rScnPhys.m_physics,    first, last);
    SysPhysics::update_delete_shapes    (rScnPhys.m_physics,    first, last);
    SysPhysics::update_delete_hier_body (rScnPhys.m_hierBody,   first, last);
    ospnewton::SysNewton::update_delete (rScnNwt,               first, last);

    rScnTest.m_hasGravity         .remove(first, last);
    rScnTest.m_removeOutOfBounds  .remove(first, last);

    rScene.update_delete();
    */

}

#if 0
/**
 * @brief Update CommonTestScene containing physics test
 *
 * @param rScene [ref] scene to update
 */
static void update_test_scene(CommonTestScene& rScene, float const delta)
{
    using namespace osp::active;
    using namespace ospnewton;
    using Corrade::Containers::ArrayView;

    auto &rScnTest  = rScene.get<PhysicsTestData>();
    auto &rScnPhys  = rScene.get<PhysicsData>();
    auto &rScnNwt   = rScene.get<ospnewton::ACtxNwtWorld>();

    // Clear all drawing-related dirty flags
    SysRender::clear_dirty_all(rScene.m_drawing);

    // Create boxes every 2 seconds
    rScnTest.m_boxTimer += delta;
    if (rScnTest.m_boxTimer >= 2.0f)
    {
        rScnTest.m_boxTimer -= 2.0f;

        rScnTest.m_toThrow.emplace_back(PhysicsTestData::ThrowShape{
                Vector3{10.0f, 30.0f, 0.0f},  // position
                Vector3{0.0f}, // velocity
                Vector3{2.0f, 1.0f, 2.0f}, // size
                1.0f, // mass
                EShape::Box}); // shape
    }

    // Create cylinders every 2 seconds
    rScnTest.m_cylinderTimer += delta;
    if (rScnTest.m_cylinderTimer >= 2.0f)
    {
        rScnTest.m_cylinderTimer -= 2.0f;

        rScnTest.m_toThrow.emplace_back(PhysicsTestData::ThrowShape{
                Vector3{-10.0f, 30.0f, 0.0f},  // position
                Vector3{0.0f}, // velocity
                Vector3{1.0f, 1.5f, 1.0f}, // size
                1.0f, // mass
                EShape::Cylinder}); // shape
    }

    // Gravity System, applies a 9.81N force downwards (-Y) for select entities
    for (ActiveEnt const ent : rScnTest.m_hasGravity)
    {
        acomp_storage_t<ACompPhysNetForce> &rNetForce
                = rScnPhys.m_physIn.m_physNetForce;
        ACompPhysNetForce &rEntNetForce = rNetForce.contains(ent)
                                        ? rNetForce.get(ent)
                                        : rNetForce.emplace(ent);

        rEntNetForce.y() -= 9.81f;
    }

    // Physics update

    SysNewton::update_colliders(
            rScnPhys.m_physics, rScnNwt,
            std::exchange(rScnPhys.m_physIn.m_colliderDirty, {}));

    auto const physIn = ArrayView<ACtxPhysInputs>(&rScnPhys.m_physIn, 1);
    SysNewton::update_world(
            rScnPhys.m_physics, rScnNwt, delta, physIn,
            rScene.m_basic.m_hierarchy,
            rScene.m_basic.m_transform, rScene.m_basic.m_transformControlled,
            rScene.m_basic.m_transformMutable);

    // Start recording new elements to delete
    rScene.m_delete.clear();

    // Check position of all entities with the out-of-bounds component
    // Delete the ones that go out of bounds
    for (ActiveEnt const ent : rScnTest.m_removeOutOfBounds)
    {
        ACompTransform const &entTf = rScene.m_basic.m_transform.get(ent);
        if (entTf.m_transform.translation().y() < -10)
        {
            rScene.m_delete.push_back(ent);
        }
    }

    // Delete entities in m_delete, their descendants, and components
    update_test_scene_delete(rScene);

    // Note: Prefer creating entities near the end of the update after physics
    //       and delete systems. This allows their initial state to be rendered
    //       in a frame and avoids some possible synchronization issues from
    //       when entities are created and deleted right away.

    // Shape Thrower system, consumes rScene.m_toThrow and creates shapes
    for (PhysicsTestData::ThrowShape const &rThrow : std::exchange(rScnTest.m_toThrow, {}))
    {
        ActiveEnt shapeEnt = add_rigid_body_quick(
                rScene, rThrow.m_position, rThrow.m_velocity, rThrow.m_mass,
                rThrow.m_shape, rThrow.m_size);

        // Make gravity affect entity
        rScnTest.m_hasGravity.emplace(shapeEnt);

        // Remove when it goes out of bounds
        rScnTest.m_removeOutOfBounds.emplace(shapeEnt);
    }

    // Sort hierarchy, required by renderer
    SysHierarchy::sort(rScene.m_basic.m_hierarchy);
}

#endif

//-----------------------------------------------------------------------------

struct PhysicsTestControls
{
    PhysicsTestControls(ActiveApplication &rApp)
     : m_camCtrl(rApp.get_input_handler())
     , m_btnThrow(m_camCtrl.m_controls.button_subscribe("debug_throw"))
    { }

    ACtxCameraController m_camCtrl;

    osp::input::EButtonControlIndex m_btnThrow;
};

void PhysicsTest::setup_renderer_gl(MainView mainView, ActiveApplication& rApp) noexcept
{


    using namespace osp::active;

    /*
    auto &rControls = rRenderer.emplace<PhysicsTestControls>(rApp);

    // Select first camera for rendering
    ActiveEnt const camEnt = rScene.m_basic.m_camera.at(0);
    rRenderer.m_camera = camEnt;
    rScene.m_basic.m_camera.get(camEnt).set_aspect_ratio(
            osp::Vector2(Magnum::GL::defaultFramebuffer.viewport().size()));
    SysRender::add_draw_transforms_recurse(
            rScene.m_basic.m_hierarchy,
            rRenderer.m_renderGl.m_drawTransform,
            camEnt);

    // Set initial position of camera slightly above the ground
    rControls.m_camCtrl.m_target = osp::Vector3{0.0f, 2.0f, 0.0f};


    rRenderer.m_onDraw = [] (
            CommonSceneRendererGL& rRenderer, CommonTestScene& rScene,
            ActiveApplication& rApp, float delta) noexcept
    {
        auto &rScnTest = rScene.get<PhysicsTestData>();
        auto &rControls = rRenderer.get<PhysicsTestControls>();

        // Throw a sphere when the throw button is pressed
        if (rControls.m_camCtrl.m_controls.button_held(rControls.m_btnThrow))
        {
            Matrix4 const &camTf = rScene.m_basic.m_transform.get(rRenderer.m_camera).m_transform;
            float const speed = 120;
            float const dist = 5.0f; // Distance from camera to spawn spheres
            rScnTest.m_toThrow.emplace_back(PhysicsTestData::ThrowShape{
                    camTf.translation() - camTf.backward() * dist, // position
                    -camTf.backward() * speed, // velocity
                    Vector3{1.0f}, // size (radius)
                    100.0f, // mass
                    EShape::Sphere}); // shape
        }

        // Update the scene directly in the drawing function :)
        update_test_scene(rScene, gc_physTimestep);

        // Rotate and move the camera based on user inputs
        SysCameraController::update_view(
                rControls.m_camCtrl,
                rScene.m_basic.m_transform.get(rRenderer.m_camera), delta);
        SysCameraController::update_move(
                rControls.m_camCtrl,
                rScene.m_basic.m_transform.get(rRenderer.m_camera),
                delta, true);

        rRenderer.update_delete(rScene.m_deleteTotal);
        rRenderer.sync(rApp, rScene);
        rRenderer.prepare_fbo(rApp);
        rRenderer.draw_entities(rApp, rScene);
        rRenderer.display(rApp);
    };
    */
}

} // namespace testapp::physicstest
