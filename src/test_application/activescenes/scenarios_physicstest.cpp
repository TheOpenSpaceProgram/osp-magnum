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
#include "common_scene.h"
#include "common_renderer_gl.h"
#include "CameraController.h"

#include "../ActiveApplication.h"

#include <osp/Active/basic.h>
#include <osp/Active/drawing.h>
#include <osp/Active/physics.h>

#include <osp/Active/SysHierarchy.h>

#include <osp/Resource/resources.h>

#include <longeron/id_management/registry.hpp>

#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/ImageData.h>

#include <Magnum/GL/DefaultFramebuffer.h>

#include <Corrade/Containers/ArrayViewStl.h>

#include <newtondynamics_physics/ospnewton.h>
#include <newtondynamics_physics/SysNewton.h>

using Magnum::Trade::MeshData;
using Magnum::Trade::ImageData2D;

using osp::active::ActiveEnt;
using osp::active::active_sparse_set_t;

using osp::phys::EShape;

using osp::Vector3;
using osp::Matrix3;
using osp::Matrix4;

using osp::active::MeshIdOwner_t;

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

namespace testapp::physicstest
{

constexpr float gc_physTimestep = 1.0 / 60.0f;
constexpr int gc_threadCount = 4; // note: not yet passed to Newton

/**
 * @brief Data needed to support physics in any scene
 */
struct PhysicsData
{
    // Generic physics components and data
    osp::active::ACtxPhysics        m_physics;
    osp::active::ACtxHierBody       m_hierBody;

    // 'Per-thread' inputs fed into the physics engine. Only one here for now
    osp::active::ACtxPhysInputs     m_physIn;
};

/**
 * @brief Data used specifically by the physics test scene
 */
struct PhysicsTestData
{
    active_sparse_set_t             m_hasGravity;
    active_sparse_set_t             m_removeOutOfBounds;

    // Meshes used in the scene
    entt::dense_hash_map<EShape, MeshIdOwner_t> m_shapeToMesh;
    MeshIdOwner_t m_meshCube;

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

static ActiveEnt add_solid(
        CommonTestScene& rScene, ActiveEnt parent,
        EShape shape, Matrix4 transform, int material, float mass)
{
    using namespace osp::active;

    auto &rScnTest = rScene.get<PhysicsTestData>();
    auto &rScnPhys = rScene.get<PhysicsData>();

    // Make entity
    ActiveEnt ent = rScene.m_activeIds.create();

    // Add mesh
    rScene.m_drawing.m_mesh.emplace(
            ent, rScene.m_drawing.m_meshRefCounts.ref_add(rScnTest.m_shapeToMesh.at(shape)) );
    rScene.m_drawing.m_meshDirty.push_back(ent);

    // Add material to cube
    MaterialData &rMaterial = rScene.m_drawing.m_materials[material];
    rMaterial.m_comp.emplace(ent);
    rMaterial.m_added.push_back(ent);

    // Add transform
    rScene.m_basic.m_transform.emplace(ent, ACompTransform{transform});

    // Add opaque and visible component
    rScene.m_drawing.m_opaque.emplace(ent);
    rScene.m_drawing.m_visible.emplace(ent);

    // Add physics stuff
    rScnPhys.m_physics.m_shape.emplace(ent, shape);
    rScnPhys.m_physics.m_solid.emplace(ent);
    Vector3 const inertia
            = osp::phys::collider_inertia_tensor(shape, transform.scaling(), mass);
    rScnPhys.m_hierBody.m_ownDyn.emplace( ent, ACompSubBody{ inertia, mass } );

    rScnPhys.m_physIn.m_colliderDirty.push_back(ent);

    // Add to hierarchy
    SysHierarchy::add_child(rScene.m_basic.m_hierarchy, parent, ent);

    return ent;
}

/**
 * @brief Quick function to throw a drawable physics entity of a single
 *        primative shape
 *
 * @return Root of shape entity
 */
static ActiveEnt add_quick_shape(
        CommonTestScene &rScene, Vector3 position, Vector3 velocity,
        float mass, EShape shape, Vector3 size)
{
    using namespace osp::active;

    auto &rScnTest = rScene.get<PhysicsTestData>();
    auto &rScnPhys = rScene.get<PhysicsData>();

    // Root is needed to act as the rigid body entity
    // Scale of root entity must be (1, 1, 1). Descendents that act as colliders
    // are allowed to have different scales
    ActiveEnt root = rScene.m_activeIds.create();

    // Add transform
    rScene.m_basic.m_transform.emplace(
            root, ACompTransform{Matrix4::translation(position)});

    // Add root entity to hierarchy
    SysHierarchy::add_child(rScene.m_basic.m_hierarchy, rScene.m_hierRoot, root);

    // Create collider / drawable to the root entity
    add_solid(
            rScene, root, shape,
            Matrix4::scaling(size),
            rScene.m_matVisualizer, 0.0f);

    // Make ball root a dynamic rigid body
    rScnPhys.m_physics.m_hasColliders.emplace(root);
    rScnPhys.m_physics.m_physBody.emplace(root);
    rScnPhys.m_physics.m_physLinearVel.emplace(root);
    rScnPhys.m_physics.m_physAngularVel.emplace(root);
    osp::active::ACompPhysDynamic &rDyn = rScnPhys.m_physics.m_physDynamic.emplace(root);
    rDyn.m_totalMass = 1.0f;

    // make gravity affect ball
    rScnTest.m_hasGravity.emplace(root);

    // Remove ball when it goes out of bounds
    rScnTest.m_removeOutOfBounds.emplace(root);

    // Set velocity
    rScnPhys.m_physIn.m_setVelocity.emplace_back(root, velocity);

    return root;
}

void setup_scene(CommonTestScene &rScene, osp::PkgId pkg)
{
    using namespace osp::active;

    // It should be clear that the physics test scene is a composition of:
    // * PhysicsData:       Generic physics data
    // * ACtxNwtWorld:      Newton Dynamics Physics engine
    // * PhysicsTestData:   Additional scene-specific data, ie. dropping blocks
    auto &rScnPhys  = rScene.emplace<PhysicsData>();
    auto &rScnNwt   = rScene.emplace<ospnewton::ACtxNwtWorld>(gc_threadCount);
    auto &rScnTest  = rScene.emplace<PhysicsTestData>();

    // Add cleanup function to deal with reference counts
    // Note: The problem with destructors, is that REQUIRE storing pointers in
    //       the data, which is uglier to deal with
    rScene.m_onCleanup.push_back([] (CommonTestScene &rScene)
    {
        auto &rScnTest = rScene.get<PhysicsTestData>();

        rScene.m_drawing.m_meshRefCounts.ref_release(rScnTest.m_meshCube);
        for (auto & [_, rOwner] : std::exchange(rScnTest.m_shapeToMesh, {}))
        {
            rScene.m_drawing.m_meshRefCounts.ref_release(rOwner);
        }
    });

    osp::Resources &rResources = *rScene.m_pResources;

    // Convenient function to get a reference-counted mesh owner
    auto const quick_add_mesh = [&rScene, &rResources, pkg] (std::string_view name) -> MeshIdOwner_t
    {
        osp::ResId const res = rResources.find(osp::restypes::gc_mesh, pkg, name);
        assert(res != lgrn::id_null<osp::ResId>());
        MeshId const meshId = SysRender::own_mesh_resource(rScene.m_drawing, rScene.m_drawingRes, rResources, res);
        return rScene.m_drawing.m_meshRefCounts.ref_add(meshId);
    };

    // Acquire mesh resources from Package
    rScnTest.m_shapeToMesh.emplace(EShape::Box,       quick_add_mesh("cube"));
    rScnTest.m_shapeToMesh.emplace(EShape::Cylinder,  quick_add_mesh("cylinder"));
    rScnTest.m_shapeToMesh.emplace(EShape::Sphere,    quick_add_mesh("sphere"));
    rScnTest.m_meshCube = quick_add_mesh("grid64solid");

    // Allocate space to fit all materials
    rScene.m_drawing.m_materials.resize(rScene.m_materialCount);

    // Create hierarchy root entity
    rScene.m_hierRoot = rScene.m_activeIds.create();
    rScene.m_basic.m_hierarchy.emplace(rScene.m_hierRoot);

    // Create camera entity
    ActiveEnt camEnt = rScene.m_activeIds.create();

    // Create camera transform and draw transform
    ACompTransform &rCamTf = rScene.m_basic.m_transform.emplace(camEnt);
    rCamTf.m_transform.translation().z() = 25;

    // Create camera component
    ACompCamera &rCamComp = rScene.m_basic.m_camera.emplace(camEnt);
    rCamComp.m_far = 1u << 24;
    rCamComp.m_near = 1.0f;
    rCamComp.m_fov = 45.0_degf;

    // Add camera to hierarchy
    SysHierarchy::add_child(
            rScene.m_basic.m_hierarchy, rScene.m_hierRoot, camEnt);

    // Create floor entity
    {
        // Create floor root entity
        ActiveEnt floorRoot = rScene.m_activeIds.create();

        // Add transform and draw transform to root
        rScene.m_basic.m_transform.emplace(
                floorRoot, ACompTransform{Matrix4::rotationX(-90.0_degf)});

        // Create floor mesh entity
        ActiveEnt floorMesh = rScene.m_activeIds.create();

        // Add grid mesh to floor mesh
        rScene.m_drawing.m_mesh.emplace(
                floorMesh, rScene.m_drawing.m_meshRefCounts.ref_add(rScnTest.m_meshCube));
        rScene.m_drawing.m_meshDirty.push_back(floorMesh);

        // Add mesh visualizer material to floor mesh
        MaterialData &rMatCommon
                = rScene.m_drawing.m_materials[rScene.m_matVisualizer];
        rMatCommon.m_comp.emplace(floorMesh);
        rMatCommon.m_added.push_back(floorMesh);

        // Add transform, draw transform, opaque, and visible
        rScene.m_basic.m_transform.emplace(
                floorMesh, ACompTransform{Matrix4::scaling({64.0f, 64.0f, 1.0f})});
        rScene.m_drawing.m_opaque.emplace(floorMesh);
        rScene.m_drawing.m_visible.emplace(floorMesh);

        // Add floor root to hierarchy root
        SysHierarchy::add_child(
                rScene.m_basic.m_hierarchy, rScene.m_hierRoot, floorRoot);

        // Add floor mesh to floor root
        SysHierarchy::add_child(
                rScene.m_basic.m_hierarchy, floorRoot, floorMesh);

        // Add collider (yeah lol it's a big cube)
        add_solid(rScene, floorRoot, EShape::Box,
                  Matrix4::scaling({64.0f, 64.0f, 1.0f}) * Matrix4::translation({0.0f, 0.0f, -1.005f}),
                  rScene.m_matCommon, 0.0f);

        // Make floor root a (non-dynamic) rigid body
        rScnPhys.m_physics.m_hasColliders.emplace(floorRoot);
        rScnPhys.m_physics.m_physBody.emplace(floorRoot);
    }
}

void update_test_scene_delete(CommonTestScene &rScene)
{
    using namespace osp::active;

    auto &rScnTest  = rScene.get<PhysicsTestData>();
    auto &rScnPhys  = rScene.get<PhysicsData>();
    auto &rScnNwt   = rScene.get<ospnewton::ACtxNwtWorld>();

    rScene.update_hierarchy_delete();

    auto first = std::cbegin(rScene.m_deleteTotal);
    auto last = std::cend(rScene.m_deleteTotal);

    // Delete components of total entities to delete
    SysPhysics::update_delete_phys      (rScnPhys.m_physics,    first, last);
    SysPhysics::update_delete_shapes    (rScnPhys.m_physics,    first, last);
    SysPhysics::update_delete_hier_body (rScnPhys.m_hierBody,   first, last);
    ospnewton::SysNewton::update_delete (rScnNwt,               first, last);

    rScnTest.m_hasGravity         .remove(first, last);
    rScnTest.m_removeOutOfBounds  .remove(first, last);

    rScene.update_delete();

}

/**
 * @brief Update CommonTestScene containing physics test
 *
 * @param rScene [ref] scene to update
 */
static void update_test_scene(CommonTestScene& rScene, float delta)
{
    using namespace osp::active;
    using namespace ospnewton;
    using Corrade::Containers::ArrayView;

    auto &rScnTest  = rScene.get<PhysicsTestData>();
    auto &rScnPhys  = rScene.get<PhysicsData>();
    auto &rScnNwt   = rScene.get<ospnewton::ACtxNwtWorld>();

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
    for (ActiveEnt ent : rScnTest.m_hasGravity)
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
            rScnPhys.m_physics, rScnNwt, gc_physTimestep, physIn,
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
        add_quick_shape(
                rScene, rThrow.m_position, rThrow.m_velocity, rThrow.m_mass,
                rThrow.m_shape, rThrow.m_size);
    }

    // Sort hierarchy, required by renderer
    SysHierarchy::sort(rScene.m_basic.m_hierarchy);
}

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

on_draw_t generate_draw_func(CommonTestScene& rScene, ActiveApplication& rApp)
{
    using namespace osp::active;
    using namespace osp::shader;

    // Create renderer data. This uses a shared_ptr to allow being stored
    // inside an std::function, which require copyable types
    std::shared_ptr<CommonSceneRendererGL> pRenderer
            = std::make_shared<CommonSceneRendererGL>();

    CommonSceneRendererGL &rRenderer = *pRenderer;
    auto &rControls = rRenderer.emplace<PhysicsTestControls>(rApp);

    pRenderer->setup(rApp, rScene);

    // Select first camera for rendering
    ActiveEnt const camEnt = rScene.m_basic.m_camera.at(0);
    pRenderer->m_camera = camEnt;
    rScene.m_basic.m_camera.get(camEnt).set_aspect_ratio(
            osp::Vector2(Magnum::GL::defaultFramebuffer.viewport().size()));
    SysRender::add_draw_transforms_recurse(
            rScene.m_basic.m_hierarchy,
            pRenderer->m_renderGl.m_drawTransform,
            camEnt);

    // Set initial position of camera slightly above the ground
    rControls.m_camCtrl.m_target = osp::Vector3{0.0f, 2.0f, 0.0f};

    // Set all materials dirty
    rScene.set_all_dirty();

    return [&rScene, pRenderer = std::move(pRenderer)] (ActiveApplication& rApp, float delta)
    {
        auto &rScnTest = rScene.get<PhysicsTestData>();
        auto &rControls = pRenderer->get<PhysicsTestControls>();

        // Throw a sphere when the throw button is pressed
        if (rControls.m_camCtrl.m_controls.button_triggered(rControls.m_btnThrow))
        {
            Matrix4 const &camTf = rScene.m_basic.m_transform.get(pRenderer->m_camera).m_transform;
            float const speed = 120;
            float const dist = 5.0f; // Distance from camera to spawn spheres
            rScnTest.m_toThrow.emplace_back(PhysicsTestData::ThrowShape{
                    camTf.translation() - camTf.backward() * dist, // position
                    -camTf.backward() * speed, // velocity
                    Vector3{1.0f}, // size (radius)
                    100.0f, // mass
                    EShape::Sphere}); // shape
        }

        update_test_scene(rScene, delta);

        // Delete components of deleted entities on renderer's side
        pRenderer->update_delete(rScene.m_deleteTotal);

        // Rotate and move the camera based on user inputs
        SysCameraController::update_view(
                rControls.m_camCtrl,
                rScene.m_basic.m_transform.get(pRenderer->m_camera), delta);
        SysCameraController::update_move(
                rControls.m_camCtrl,
                rScene.m_basic.m_transform.get(pRenderer->m_camera),
                delta, true);

        // Do common render
        pRenderer->render(rApp, rScene);

        SysRender::clear_dirty_materials(rScene.m_drawing.m_materials);
    };
}

} // namespace testapp::physicstest
