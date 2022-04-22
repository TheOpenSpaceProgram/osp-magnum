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

#include <osp/Resource/resources.h>
#include <osp/Resource/ImporterData.h>

#include <longeron/id_management/registry.hpp>

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/Trade/PbrMetallicRoughnessMaterialData.h>


#include <Corrade/Containers/ArrayViewStl.h>

#include <newtondynamics_physics/ospnewton.h>
#include <newtondynamics_physics/SysNewton.h>

#include <iostream>

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

struct PrefabInit
{
    osp::ResId m_res;
    int m_id;

    std::vector<ActiveEnt> m_prefabToEnt;
};

/**
 * @brief Data used specifically by the physics test scene
 */
struct VehicleTestData
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

    osp::ResId m_prefab;

    // Queue for balls to throw
    std::vector<ThrowShape> m_toThrow;

    std::vector<PrefabInit> m_prefabInit;
};


void VehicleTest::setup_scene(CommonTestScene &rScene, osp::PkgId pkg)
{
    using namespace osp::active;

    // It should be clear that the physics test scene is a composition of:
    // * PhysicsData:       Generic physics data
    // * ACtxNwtWorld:      Newton Dynamics Physics engine
    // * VehicleTestData:   Additional scene-specific data, ie. dropping blocks
    auto &rScnPhys  = rScene.emplace<PhysicsData>();
    auto &rScnNwt   = rScene.emplace<ospnewton::ACtxNwtWorld>(gc_threadCount);
    auto &rScnTest  = rScene.emplace<VehicleTestData>();

    // Add cleanup function to deal with reference counts
    // Note: The problem with destructors, is that REQUIRE storing pointers in
    //       the data, which is uglier to deal with
    rScene.m_onCleanup.push_back(&PhysicsData::cleanup);

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
    rScnPhys.m_shapeToMesh.emplace(EShape::Box,       quick_add_mesh("cube"));
    rScnPhys.m_shapeToMesh.emplace(EShape::Cylinder,  quick_add_mesh("cylinder"));
    rScnPhys.m_shapeToMesh.emplace(EShape::Sphere,    quick_add_mesh("sphere"));
    rScnPhys.m_namedMeshs.emplace("floor", quick_add_mesh("grid64solid"));

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

    // start making floor

    static constexpr Vector3 const sc_floorSize{5.0f, 5.0f, 1.0f};
    static constexpr Vector3 const sc_floorPos{0.0f, 0.0f, -1.005f};

    // Create floor root entity
    ActiveEnt const floorRootEnt = rScene.m_activeIds.create();

    // Add transform and draw transform to root
    rScene.m_basic.m_transform.emplace(
            floorRootEnt, ACompTransform{Matrix4::rotationX(-90.0_degf)});

    // Create floor mesh entity
    ActiveEnt const floorMeshEnt = rScene.m_activeIds.create();

    // Add mesh to floor mesh entity
    rScene.m_drawing.m_mesh.emplace(floorMeshEnt, quick_add_mesh("grid64solid"));
    rScene.m_drawing.m_meshDirty.push_back(floorMeshEnt);

    // Add mesh visualizer material to floor mesh entity
    MaterialData &rMatCommon
            = rScene.m_drawing.m_materials[rScene.m_matVisualizer];
    rMatCommon.m_comp.emplace(floorMeshEnt);
    rMatCommon.m_added.push_back(floorMeshEnt);

    // Add transform, draw transform, opaque, and visible entity
    rScene.m_basic.m_transform.emplace(
            floorMeshEnt, ACompTransform{Matrix4::scaling(sc_floorSize)});
    rScene.m_drawing.m_opaque.emplace(floorMeshEnt);
    rScene.m_drawing.m_visible.emplace(floorMeshEnt);

    // Add floor root to hierarchy root
    SysHierarchy::add_child(
            rScene.m_basic.m_hierarchy, rScene.m_hierRoot, floorRootEnt);

    // Parent floor mesh entity to floor root entity
    SysHierarchy::add_child(
            rScene.m_basic.m_hierarchy, floorRootEnt, floorMeshEnt);

    // Add collider to floor root entity (yeah lol it's a big cube)
    Matrix4 const floorTf = Matrix4::scaling(sc_floorSize)
                          * Matrix4::translation(sc_floorPos);
    add_solid_quick(rScene, floorRootEnt, EShape::Box, floorTf,
                    rScene.m_matCommon, 0.0f);

    // Make floor entity a (non-dynamic) rigid body
    rScnPhys.m_physics.m_hasColliders.emplace(floorRootEnt);
    rScnPhys.m_physics.m_physBody.emplace(floorRootEnt);

}

static void update_test_scene_delete(CommonTestScene &rScene)
{
    using namespace osp::active;

    auto &rScnTest  = rScene.get<VehicleTestData>();
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

    auto &rScnTest  = rScene.get<VehicleTestData>();
    auto &rScnPhys  = rScene.get<PhysicsData>();
    auto &rScnNwt   = rScene.get<ospnewton::ACtxNwtWorld>();

    // Clear all drawing-related dirty flags
    SysRender::clear_dirty_all(rScene.m_drawing);

    // Create boxes every 2 seconds
    rScnTest.m_boxTimer += delta;
    if (rScnTest.m_boxTimer >= 2.0f)
    {
        rScnTest.m_boxTimer -= 2.0f;

        rScnTest.m_toThrow.emplace_back(VehicleTestData::ThrowShape{
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

        rScnTest.m_toThrow.emplace_back(VehicleTestData::ThrowShape{
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

        rEntNetForce.y() -= 9.81f * rScnPhys.m_physics.m_physDynamic.get(ent).m_totalMass;
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

    using osp::restypes::gc_importer;

    static int haha = 0, foo = 0;
    haha += 1;
    if (haha >= 6)
    {
        haha = 0;
        if (rScene.m_pResources->ids(gc_importer).exists(osp::ResId(foo)))
        {
            rScnTest.m_prefabInit.emplace_back(PrefabInit{osp::ResId(foo), 0, {}});
        }

        foo ++;
        foo %= rScene.m_pResources->ids(gc_importer).capacity();
    }

    // Initialize prefab entities
    for (PrefabInit& rPrefab : rScnTest.m_prefabInit)
    {

        //auto const &rImportData = rScene.m_pResources->data_get<osp::ImporterData>(gc_importer, rPrefab.m_res);
        auto const &rPrefabData = rScene.m_pResources->data_get<osp::Prefabs>(gc_importer, rPrefab.m_res);

        auto const objects = rPrefabData.m_prefabs[rPrefab.m_id];

        rPrefab.m_prefabToEnt.resize(objects.size());

        rScene.m_activeIds.create(std::begin(rPrefab.m_prefabToEnt), objects.size());
    }

    //std::cout << "Entity count: " << rScene.m_activeIds.size() << "/" << rScene.m_activeIds.capacity() << "\n";

    // Init prefab hierarchy: Add hierarchy components
    for (PrefabInit& rPrefab : rScnTest.m_prefabInit)
    {
        auto const &rImportData = rScene.m_pResources->data_get<osp::ImporterData>(gc_importer, rPrefab.m_res);
        auto const &rPrefabData = rScene.m_pResources->data_get<osp::Prefabs>(gc_importer, rPrefab.m_res);

        auto const objects = rPrefabData.m_prefabs[rPrefab.m_id];
        auto const parents = rPrefabData.m_prefabParents[rPrefab.m_id];

        for (int i = 0; i < objects.size(); ++i)
        {
            int const prefabParent = parents[i];
            ActiveEnt const parent = (prefabParent == -1)
                    ? rScene.m_hierRoot
                    : rPrefab.m_prefabToEnt[prefabParent];

            ActiveEnt const child = rPrefab.m_prefabToEnt[i];

            SysHierarchy::add_child(rScene.m_basic.m_hierarchy, parent, child);
        }
    }

    // Init prefab transforms: Add transform components
    for (PrefabInit& rPrefab : rScnTest.m_prefabInit)
    {
        auto const &rImportData = rScene.m_pResources->data_get<osp::ImporterData>(gc_importer, rPrefab.m_res);
        auto const &rPrefabData = rScene.m_pResources->data_get<osp::Prefabs>(gc_importer, rPrefab.m_res);

        auto const objects = rPrefabData.m_prefabs[rPrefab.m_id];

        for (int i = 0; i < objects.size(); ++i)
        {
            rScene.m_basic.m_transform.emplace(rPrefab.m_prefabToEnt[i], rImportData.m_objTransforms[objects[i]]);
        }

        // move root up because why not
        rScene.m_basic.m_transform.get(rPrefab.m_prefabToEnt[0]).m_transform.translation().y() += 5;
    }


    // init prefab drawables
    for (PrefabInit& rPrefab : rScnTest.m_prefabInit)
    {
        auto const &rImportData = rScene.m_pResources->data_get<osp::ImporterData>(gc_importer, rPrefab.m_res);
        auto const &rPrefabData = rScene.m_pResources->data_get<osp::Prefabs>(gc_importer, rPrefab.m_res);

        auto const objects = rPrefabData.m_prefabs[rPrefab.m_id];

        for (int i = 0; i < objects.size(); ++i)
        {
            if (int const meshImportId = rImportData.m_objMeshes[objects[i]];
               meshImportId != -1)
            {
                //
                // has mesh

                ActiveEnt const ent = rPrefab.m_prefabToEnt[i];

                osp::ResId const meshRes = rImportData.m_meshes[meshImportId];
                MeshId const meshId = SysRender::own_mesh_resource(rScene.m_drawing, rScene.m_drawingRes, *rScene.m_pResources, meshRes);
                rScene.m_drawing.m_mesh.emplace(ent, rScene.m_drawing.m_meshRefCounts.ref_add(meshId));
                rScene.m_drawing.m_meshDirty.push_back(ent);

                int const matImportId = rImportData.m_objMaterials[objects[i]];

                if (Magnum::Trade::MaterialData const &mat = *rImportData.m_materials.at(matImportId);
                    mat.types() & Magnum::Trade::MaterialType::PbrMetallicRoughness)
                {
                    auto const& matPbr = mat.as<Magnum::Trade::PbrMetallicRoughnessMaterialData>();
                    if (int const baseColor = matPbr.baseColorTexture();
                        baseColor != -1)
                    {
                        osp::ResId const texRes = rImportData.m_textures[baseColor];
                        TexId const texId = SysRender::own_texture_resource(rScene.m_drawing, rScene.m_drawingRes, *rScene.m_pResources, texRes);
                        rScene.m_drawing.m_diffuseTex.emplace(ent, rScene.m_drawing.m_texRefCounts.ref_add(texId));
                        rScene.m_drawing.m_diffuseDirty.push_back(ent);
                    }

                }


                MaterialData &rMaterial = rScene.m_drawing.m_materials[rScene.m_matCommon];
                rMaterial.m_comp.emplace(ent);
                rMaterial.m_added.push_back(ent);

                rScene.m_drawing.m_opaque.emplace(ent);
                rScene.m_drawing.m_visible.emplace(ent);


            }
        }
    }

    // Init prefab physics
    for (PrefabInit& rPrefab : rScnTest.m_prefabInit)
    {
        auto const &rImportData = rScene.m_pResources->data_get<osp::ImporterData>(gc_importer, rPrefab.m_res);
        auto const &rPrefabData = rScene.m_pResources->data_get<osp::Prefabs>(gc_importer, rPrefab.m_res);

        auto const objects = rPrefabData.m_prefabs[rPrefab.m_id];

        float totalMass = 0.0f;
        Vector3 totalInertia{0.0f}; // ultra-accurate calculation :)

        for (int i = 0; i < objects.size(); ++i)
        {
            ActiveEnt const ent = rPrefab.m_prefabToEnt[i];

            EShape const shape = rPrefabData.m_objShape[objects[i]];

            if (shape != EShape::None)
            {
                rScnPhys.m_physics.m_shape.emplace(ent, shape);
                rScnPhys.m_physics.m_solid.emplace(ent);
                rScnPhys.m_physIn.m_colliderDirty.push_back(ent);
            }

            if (float mass = rPrefabData.m_objMass[objects[i]];
               mass != 0.0f)
            {

                osp::Vector3 const inertia
                        = osp::phys::collider_inertia_tensor(shape != EShape::None ? shape : EShape::Sphere, rImportData.m_objTransforms[objects[i]].scaling(), mass);
                rScnPhys.m_hierBody.m_ownDyn.emplace( ent, ACompSubBody{ inertia, mass } );
                totalMass += mass;
                totalInertia += inertia;
            }
        }

        // temporary: make root a rigid body

        ActiveEnt const root = rPrefab.m_prefabToEnt[0];

        rScnPhys.m_physics.m_hasColliders.emplace(root);
        rScnPhys.m_physics.m_physBody.emplace(root);
        rScnPhys.m_physics.m_physLinearVel.emplace(root);
        rScnPhys.m_physics.m_physAngularVel.emplace(root);
        osp::active::ACompPhysDynamic &rDyn = rScnPhys.m_physics.m_physDynamic.emplace(root);
        rDyn.m_totalMass = totalMass;
        rDyn.m_inertia = totalInertia;

        // Make gravity affect entity
        rScnTest.m_hasGravity.emplace(root);
        rScnTest.m_removeOutOfBounds.emplace(root);
    }

    rScnTest.m_prefabInit.clear();


    // Note: Prefer creating entities near the end of the update after physics
    //       and delete systems. This allows their initial state to be rendered
    //       in a frame and avoids some possible synchronization issues from
    //       when entities are created and deleted right away.

    // Shape Thrower system, consumes rScene.m_toThrow and creates shapes
    for (VehicleTestData::ThrowShape const &rThrow : std::exchange(rScnTest.m_toThrow, {}))
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

//-----------------------------------------------------------------------------

struct VehicleTestControls
{
    VehicleTestControls(ActiveApplication &rApp)
     : m_camCtrl(rApp.get_input_handler())
     , m_btnThrow(m_camCtrl.m_controls.button_subscribe("debug_throw"))
    { }

    ACtxCameraController m_camCtrl;

    osp::input::EButtonControlIndex m_btnThrow;
};

void VehicleTest::setup_renderer_gl(CommonSceneRendererGL& rRenderer, CommonTestScene& rScene, ActiveApplication& rApp) noexcept
{
    using namespace osp::active;

    auto &rControls = rRenderer.emplace<VehicleTestControls>(rApp);

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

    rRenderer.m_onCustomDraw = [] (
            CommonSceneRendererGL& rRenderer, CommonTestScene& rScene,
            ActiveApplication& rApp, float delta) noexcept
    {
        auto &rScnTest = rScene.get<VehicleTestData>();
        auto &rControls = rRenderer.get<VehicleTestControls>();

        // Throw a sphere when the throw button is pressed
        if (rControls.m_camCtrl.m_controls.button_held(rControls.m_btnThrow))
        {
            Matrix4 const &camTf = rScene.m_basic.m_transform.get(rRenderer.m_camera).m_transform;
            float const speed = 120;
            float const dist = 8.0f; // Distance from camera to spawn spheres
            rScnTest.m_toThrow.emplace_back(VehicleTestData::ThrowShape{
                    camTf.translation() - camTf.backward() * dist, // position
                    -camTf.backward() * speed, // velocity
                    Vector3{1.0f}, // size (radius)
                    700.0f, // mass
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
    };
}

} // namespace testapp::VehicleTest
