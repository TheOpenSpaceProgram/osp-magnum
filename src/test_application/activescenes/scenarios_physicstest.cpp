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

/**
 * @brief State of the entire engine test scene
 */
struct PhysicsTestScene : CommonTestScene
{
    ~PhysicsTestScene()
    {
        m_drawing.m_meshRefCounts.ref_release(m_meshCube);
        for (auto & [_, rOwner] : std::exchange(m_shapeToMesh, {}))
        {
            m_drawing.m_meshRefCounts.ref_release(rOwner);
        }
    }

    // Generic physics components and data
    osp::active::ACtxPhysics        m_physics;
    osp::active::ACtxPhysInputs     m_physIn;
    osp::active::ACtxHierBody       m_hierBody;
    active_sparse_set_t             m_hasGravity;
    active_sparse_set_t             m_removeOutOfBounds;

    // Newton Dynamics physics
    std::unique_ptr<ospnewton::ACtxNwtWorld> m_pNwtWorld;

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

ActiveEnt add_solid(
        PhysicsTestScene& rScene, ActiveEnt parent,
        EShape shape, Matrix4 transform, int material, float mass)
{
    using namespace osp::active;

    // Make entity
    ActiveEnt ent = rScene.m_activeIds.create();

    // Add mesh
    rScene.m_drawing.m_mesh.emplace(
            ent, rScene.m_drawing.m_meshRefCounts.ref_add(rScene.m_shapeToMesh.at(shape)) );
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
    rScene.m_physics.m_shape.emplace(ent, shape);
    rScene.m_physics.m_solid.emplace(ent);
    Vector3 const inertia
            = osp::phys::collider_inertia_tensor(shape, transform.scaling(), mass);
    rScene.m_hierBody.m_ownDyn.emplace( ent, ACompSubBody{ inertia, mass } );
    rScene.m_physIn.m_colliderDirty.push_back(ent);

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
ActiveEnt add_quick_shape(
        PhysicsTestScene &rScene, Vector3 position, Vector3 velocity,
        float mass, EShape shape, Vector3 size)
{
    using namespace osp::active;

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
    ActiveEnt collider = add_solid(
            rScene, root, shape,
            Matrix4::scaling(size),
            rScene.m_matVisualizer, 0.0f);

    // Make ball root a dynamic rigid body
    rScene.m_physics.m_hasColliders.emplace(root);
    rScene.m_physics.m_physBody.emplace(root);
    rScene.m_physics.m_physLinearVel.emplace(root);
    rScene.m_physics.m_physAngularVel.emplace(root);
    osp::active::ACompPhysDynamic &rDyn = rScene.m_physics.m_physDynamic.emplace(root);
    rDyn.m_totalMass = 1.0f;

    // make gravity affect ball
    rScene.m_hasGravity.emplace(root);

    // Remove ball when it goes out of bounds
    rScene.m_removeOutOfBounds.emplace(root);

    // Set velocity
    rScene.m_physIn.m_setVelocity.emplace_back(root, velocity);

    return root;
}

entt::any setup_scene(osp::Resources& rResources, osp::PkgId pkg)
{
    using namespace osp::active;

    entt::any sceneAny = entt::make_any<PhysicsTestScene>();
    PhysicsTestScene &rScene = entt::any_cast<PhysicsTestScene&>(sceneAny);

    rScene.m_pResources = &rResources;

    auto const quick_add_mesh = [&rScene, &rResources, pkg] (std::string_view name) -> MeshIdOwner_t
    {
        osp::ResId const res = rResources.find(osp::restypes::gc_mesh, pkg, name);
        assert(res != lgrn::id_null<osp::ResId>());
        MeshId const meshId = SysRender::own_mesh_resource(rScene.m_drawing, rScene.m_drawingRes, rResources, res);
        return rScene.m_drawing.m_meshRefCounts.ref_add(meshId);
    };

    // Create Newton physics world that uses 4 threads to update
    rScene.m_pNwtWorld = std::make_unique<ospnewton::ACtxNwtWorld>(4);

    // Acquire mesh resources from Package
    rScene.m_shapeToMesh.emplace(EShape::Box,       quick_add_mesh("cube"));
    rScene.m_shapeToMesh.emplace(EShape::Cylinder,  quick_add_mesh("cylinder"));
    rScene.m_shapeToMesh.emplace(EShape::Sphere,    quick_add_mesh("sphere"));
    rScene.m_meshCube = quick_add_mesh("grid64solid");

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
                floorMesh, rScene.m_drawing.m_meshRefCounts.ref_add(rScene.m_meshCube));
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
        rScene.m_physics.m_hasColliders.emplace(floorRoot);
        rScene.m_physics.m_physBody.emplace(floorRoot);
    }

    // Create ball

    return std::move(sceneAny);
}

void update_test_scene_delete(PhysicsTestScene& rScene)
{
    using namespace osp::active;

    rScene.update_total_delete();

    auto first = std::cbegin(rScene.m_deleteTotal);
    auto last = std::cend(rScene.m_deleteTotal);

    // Delete components of total entities to delete
    SysPhysics::update_delete_phys      (rScene.m_physics,      first, last);
    SysPhysics::update_delete_shapes    (rScene.m_physics,      first, last);
    SysPhysics::update_delete_hier_body (rScene.m_hierBody,     first, last);
    ospnewton::SysNewton::update_delete (*rScene.m_pNwtWorld,   first, last);

    rScene.m_hasGravity         .remove(first, last);
    rScene.m_removeOutOfBounds  .remove(first, last);

    rScene.update_delete();

}

/**
 * @brief Update an EngineTestScene, this just rotates the cube
 *
 * @param rScene [ref] scene to update
 */
void update_test_scene(PhysicsTestScene& rScene, float delta)
{
    using namespace osp::active;
    using Corrade::Containers::ArrayView;

    using namespace ospnewton;

    // Create boxes every 2 seconds
    rScene.m_boxTimer += delta;
    if (rScene.m_boxTimer >= 2.0f)
    {
        rScene.m_boxTimer -= 2.0f;

        rScene.m_toThrow.emplace_back(PhysicsTestScene::ThrowShape{
                Vector3{10.0f, 30.0f, 0.0f},  // position
                Vector3{0.0f}, // velocity
                Vector3{2.0f, 1.0f, 2.0f}, // size
                1.0f, // mass
                EShape::Box}); // shape
    }

    // Create cylinders every 2 seconds
    rScene.m_cylinderTimer += delta;
    if (rScene.m_cylinderTimer >= 2.0f)
    {
        rScene.m_cylinderTimer -= 2.0f;

        rScene.m_toThrow.emplace_back(PhysicsTestScene::ThrowShape{
                Vector3{-10.0f, 30.0f, 0.0f},  // position
                Vector3{0.0f}, // velocity
                Vector3{1.0f, 1.5f, 1.0f}, // size
                1.0f, // mass
                EShape::Cylinder}); // shape
    }

    // Gravity System, applies a 9.81N force downwards (-Y) for select entities
    for (ActiveEnt ent : rScene.m_hasGravity)
    {
        acomp_storage_t<ACompPhysNetForce> &rNetForce
                = rScene.m_physIn.m_physNetForce;
        ACompPhysNetForce &rEntNetForce = rNetForce.contains(ent)
                                        ? rNetForce.get(ent)
                                        : rNetForce.emplace(ent);

        rEntNetForce.y() -= 9.81f;
    }

    // Physics update

    SysNewton::update_colliders(
            rScene.m_physics, *rScene.m_pNwtWorld,
            std::exchange(rScene.m_physIn.m_colliderDirty, {}));

    auto const physIn = ArrayView<ACtxPhysInputs>(&rScene.m_physIn, 1);
    SysNewton::update_world(
            rScene.m_physics, *rScene.m_pNwtWorld, gc_physTimestep, physIn,
            rScene.m_basic.m_hierarchy,
            rScene.m_basic.m_transform, rScene.m_basic.m_transformControlled,
            rScene.m_basic.m_transformMutable);

    // Start recording new elements to delete
    rScene.m_delete.clear();

    // Check position of all entities with the out-of-bounds component
    // Delete the ones that go out of bounds
    for (ActiveEnt const ent : rScene.m_removeOutOfBounds)
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
    for (PhysicsTestScene::ThrowShape const &rThrow : std::exchange(rScene.m_toThrow, {}))
    {
        ActiveEnt const shape = add_quick_shape(
                rScene, rThrow.m_position, rThrow.m_velocity, rThrow.m_mass,
                rThrow.m_shape, rThrow.m_size);
    }

    // Sort hierarchy, required by renderer
    SysHierarchy::sort(rScene.m_basic.m_hierarchy);
}

//-----------------------------------------------------------------------------

/**
 * @brief Data needed to render the EngineTestScene
 */
struct PhysicsTestRenderer : CommonRendererGL
{
    PhysicsTestRenderer(ActiveApplication &rApp)
     : m_camCtrl(rApp.get_input_handler())
     , m_controls(&rApp.get_input_handler())
     , m_btnThrow(m_controls.button_subscribe("debug_throw"))
    { }

    ACtxCameraController m_camCtrl;

    osp::input::ControlSubscriber m_controls;
    osp::input::EButtonControlIndex m_btnThrow;
};

on_draw_t generate_draw_func(PhysicsTestScene& rScene, ActiveApplication& rApp)
{
    using namespace osp::active;
    using namespace osp::shader;

    // Create renderer data. This uses a shared_ptr to allow being stored
    // inside an std::function, which require copyable types
    std::shared_ptr<PhysicsTestRenderer> pRenderer
            = std::make_shared<PhysicsTestRenderer>(rApp);

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
    pRenderer->m_camCtrl.m_target = osp::Vector3{0.0f, 2.0f, 0.0f};

    // Set all materials dirty
    rScene.set_all_dirty();

    return [&rScene, pRenderer = std::move(pRenderer)] (ActiveApplication& rApp, float delta)
    {
        // Throw a sphere when the throw button is pressed
        if (pRenderer->m_controls.button_triggered(pRenderer->m_btnThrow))
        {
            Matrix4 const &camTf = rScene.m_basic.m_transform.get(pRenderer->m_camera).m_transform;
            float const speed = 120;
            float const dist = 5.0f; // Distance from camera to spawn spheres
            rScene.m_toThrow.emplace_back(PhysicsTestScene::ThrowShape{
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
                pRenderer->m_camCtrl,
                rScene.m_basic.m_transform.get(pRenderer->m_camera), delta);
        SysCameraController::update_move(
                pRenderer->m_camCtrl,
                rScene.m_basic.m_transform.get(pRenderer->m_camera),
                delta, true);

        pRenderer->render(rApp, rScene);

        SysRender::clear_dirty_materials(rScene.m_drawing.m_materials);
    };
}

} // namespace testapp::physicstest
