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
#include "CameraController.h"

#include "../ActiveApplication.h"

#include <osp/Active/basic.h>
#include <osp/Active/drawing.h>

#include <osp/Active/SysHierarchy.h>

#include <osp/Active/SysRender.h>
#include <osp/Active/opengl/SysRenderGL.h>

#include <osp/Shaders/Phong.h>
#include <osp/Shaders/MeshVisualizer.h>

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

// Materials used by the test scene. A more general application may want to
// generate IDs at runtime, and map them to named identifiers.
constexpr int const gc_mat_common      = 0;
constexpr int const gc_mat_visualizer  = 1;
constexpr int const gc_maxMaterials = 2;

constexpr float gc_physTimestep = 1.0 / 60.0f;

/**
 * @brief State of the entire engine test scene
 */
struct PhysicsTestScene
{
    ~PhysicsTestScene()
    {
        osp::active::SysRender::clear_owners(m_drawing);
        osp::active::SysRender::clear_resource_owners(m_drawingRes, *m_pResources);
        m_drawing.m_meshRefCounts.ref_release(m_meshCube);
    }

    osp::Resources *m_pResources;

    // ID registry generates entity IDs, and keeps track of which ones exist
    lgrn::IdRegistry<osp::active::ActiveEnt> m_activeIds;

    // Basic and Drawing components
    osp::active::ACtxBasic          m_basic;
    osp::active::ACtxDrawing        m_drawing;
    osp::active::ACtxDrawingRes     m_drawingRes;


    // Generic physics components and data
    osp::active::ACtxPhysics        m_physics;
    osp::active::ACtxPhysInputs     m_physIn;
    osp::active::ACtxHierBody       m_hierBody;
    active_sparse_set_t             m_hasGravity;
    active_sparse_set_t             m_removeOutOfBounds;

    // Newton Dynamics physics
    std::unique_ptr<ospnewton::ACtxNwtWorld> m_pNwtWorld;

    // Entity delete list/queue
    std::vector<ActiveEnt>          m_delete;
    std::vector<ActiveEnt>          m_deleteTotal;

    // Hierarchy root, needs to exist so all hierarchy entities are connected
    osp::active::ActiveEnt          m_hierRoot;

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
            gc_mat_visualizer, 0.0f);

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
    rScene.m_drawing.m_materials.resize(gc_maxMaterials);

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
                = rScene.m_drawing.m_materials[gc_mat_visualizer];
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
                  gc_mat_common, 0.0f);

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
    // Cut deleted entities out of the hierarchy
    SysHierarchy::update_delete_cut(
            rScene.m_basic.m_hierarchy,
            std::cbegin(rScene.m_delete), std::cend(rScene.m_delete));

    // Create a new delete vector that includes the descendents of our current
    // vector of entities to delete
    rScene.m_deleteTotal = rScene.m_delete;
    SysHierarchy::update_delete_descendents(
            rScene.m_basic.m_hierarchy,
            std::cbegin(rScene.m_delete), std::cend(rScene.m_delete),
            [&rScene] (ActiveEnt ent)
    {
        rScene.m_deleteTotal.push_back(ent);
    });

    auto first = std::cbegin(rScene.m_deleteTotal);
    auto last = std::cend(rScene.m_deleteTotal);

    // Delete components of total entities to delete
    update_delete_basic                 (rScene.m_basic,        first, last);
    SysRender::update_delete_drawing    (rScene.m_drawing,      first, last);
    SysPhysics::update_delete_phys      (rScene.m_physics,      first, last);
    SysPhysics::update_delete_shapes    (rScene.m_physics,      first, last);
    SysPhysics::update_delete_hier_body (rScene.m_hierBody,     first, last);
    ospnewton::SysNewton::update_delete (*rScene.m_pNwtWorld,   first, last);

    rScene.m_hasGravity         .remove(first, last);
    rScene.m_removeOutOfBounds  .remove(first, last);

    // Delete entity IDs
    for (ActiveEnt const ent : rScene.m_deleteTotal)
    {
        if (rScene.m_activeIds.exists(ent))
        {
            rScene.m_activeIds.remove(ent);
        }
    }
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
struct PhysicsTestRenderer
{
    PhysicsTestRenderer(ActiveApplication &rApp)
     : m_camCtrl(rApp.get_input_handler())
     , m_controls(&rApp.get_input_handler())
     , m_btnThrow(m_controls.button_subscribe("debug_throw"))
    { }

    osp::active::ACtxRenderGroups m_renderGroups;

    osp::active::ACtxSceneRenderGL m_renderGl;

    osp::active::ActiveEnt m_camera;
    ACtxCameraController m_camCtrl;

    osp::shader::ACtxDrawPhong m_phong;
    osp::shader::ACtxDrawMeshVisualizer m_visualizer;

    osp::input::ControlSubscriber m_controls;
    osp::input::EButtonControlIndex m_btnThrow;
};

/**
 * @brief Render a PhysicsTestScene
 *
 * @param rApp      [ref] Application with GL context and resources
 * @param rScene    [ref] Test scene to render
 * @param rRenderer [ref] Renderer data for test scene
 */
void render_test_scene(
        ActiveApplication& rApp, PhysicsTestScene const& rScene,
        PhysicsTestRenderer& rRenderer)
{
    using namespace osp::active;
    using namespace osp::shader;
    using Magnum::GL::Framebuffer;
    using Magnum::GL::FramebufferClear;
    using Magnum::GL::Texture2D;

    RenderGroup &rGroupFwdOpaque
            = rRenderer.m_renderGroups.m_groups["fwd_opaque"];

    // Assign Phong shader to entities with the gc_mat_common material, and put
    // results into the fwd_opaque render group
    {
        MaterialData const &rMatCommon = rScene.m_drawing.m_materials[gc_mat_common];
        assign_phong(
                rMatCommon.m_added, &rGroupFwdOpaque.m_entities, nullptr,
                rScene.m_drawing.m_opaque, rRenderer.m_renderGl.m_diffuseTexId,
                rRenderer.m_phong);
        SysRender::assure_draw_transforms(
                    rScene.m_basic.m_hierarchy,
                    rRenderer.m_renderGl.m_drawTransform,
                    std::cbegin(rMatCommon.m_added),
                    std::cend(rMatCommon.m_added));
    }

    // Same thing but with MeshVisualizer and gc_mat_visualizer
    {
        MaterialData const &rMatVisualizer
                = rScene.m_drawing.m_materials[gc_mat_visualizer];

        assign_visualizer(
                rMatVisualizer.m_added, rGroupFwdOpaque.m_entities,
                rRenderer.m_visualizer);
        SysRender::assure_draw_transforms(
                    rScene.m_basic.m_hierarchy,
                    rRenderer.m_renderGl.m_drawTransform,
                    std::cbegin(rMatVisualizer.m_added),
                    std::cend(rMatVisualizer.m_added));
    }

    // Load required meshes and textures into OpenGL
    SysRenderGL::sync_scene_resources(rScene.m_drawingRes, *rScene.m_pResources, rApp.get_render_gl());

    // Assign GL meshes to entities with a mesh component
    SysRenderGL::assign_meshes(
            rScene.m_drawing.m_mesh, rScene.m_drawingRes.m_meshToRes, rScene.m_drawing.m_meshDirty,
            rRenderer.m_renderGl.m_meshId, rApp.get_render_gl());

    // Assign GL textures to entities with a texture component
    SysRenderGL::assign_textures(
            rScene.m_drawing.m_diffuseTex, rScene.m_drawingRes.m_texToRes, rScene.m_drawing.m_diffuseDirty,
            rRenderer.m_renderGl.m_diffuseTexId, rApp.get_render_gl());

    // Calculate hierarchy transforms
    SysRender::update_draw_transforms(
            rScene.m_basic.m_hierarchy,
            rScene.m_basic.m_transform,
            rRenderer.m_renderGl.m_drawTransform);

    // Get camera to calculate view and projection matrix
    ACompCamera const &rCamera = rScene.m_basic.m_camera.get(rRenderer.m_camera);
    ACompDrawTransform const &cameraDrawTf
            = rRenderer.m_renderGl.m_drawTransform.get(rRenderer.m_camera);
    ViewProjMatrix viewProj{
            cameraDrawTf.m_transformWorld.inverted(),
            rCamera.calculate_projection()};

    // Bind offscreen FBO
    Framebuffer &rFbo = rApp.get_render_gl().m_fbo;
    rFbo.bind();

    // Clear it
    rFbo.clear( FramebufferClear::Color | FramebufferClear::Depth
                | FramebufferClear::Stencil);

    // Forward Render fwd_opaque group to FBO
    SysRenderGL::render_opaque(
            rRenderer.m_renderGroups.m_groups.at("fwd_opaque"),
            rScene.m_drawing.m_visible, viewProj);

    // Display FBO
    Texture2D &rFboColor = rApp.get_render_gl().m_texGl.get(rApp.get_render_gl().m_fboColor);
    SysRenderGL::display_texture(rApp.get_render_gl(), rFboColor);
}

on_draw_t generate_draw_func(PhysicsTestScene& rScene, ActiveApplication& rApp)
{
    using namespace osp::active;
    using namespace osp::shader;

    // Create renderer data. This uses a shared_ptr to allow being stored
    // inside an std::function, which require copyable types
    std::shared_ptr<PhysicsTestRenderer> pRenderer
            = std::make_shared<PhysicsTestRenderer>(rApp);

    // Setup Phong shaders
    auto const texturedFlags
            = Phong::Flag::DiffuseTexture | Phong::Flag::AlphaMask
            | Phong::Flag::AmbientTexture;
    pRenderer->m_phong.m_shaderDiffuse      = Phong{texturedFlags, 2};
    pRenderer->m_phong.m_shaderUntextured   = Phong{{}, 2};
    pRenderer->m_phong.assign_pointers(
            pRenderer->m_renderGl, rApp.get_render_gl());

    // Setup Mesh Visualizer shader
    pRenderer->m_visualizer.m_shader
            = MeshVisualizer{ MeshVisualizer::Flag::Wireframe };
    pRenderer->m_visualizer.assign_pointers(
            pRenderer->m_renderGl, rApp.get_render_gl());

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

    // Create render group for forward opaque pass
    pRenderer->m_renderGroups.m_groups.emplace("fwd_opaque", RenderGroup{});

    // Set all materials dirty
    for (MaterialData &rMat : rScene.m_drawing.m_materials)
    {
        rMat.m_added.assign(std::begin(rMat.m_comp), std::end(rMat.m_comp));
    }

    // Set all meshs dirty
    auto &rMeshSet = static_cast<active_sparse_set_t&>(rScene.m_drawing.m_mesh);
    rScene.m_drawing.m_meshDirty.assign(std::begin(rMeshSet), std::end(rMeshSet));

    // Set all textures dirty
    auto &rDiffSet = static_cast<active_sparse_set_t&>(rScene.m_drawing.m_diffuseTex);
    rScene.m_drawing.m_diffuseDirty.assign(std::begin(rMeshSet), std::end(rMeshSet));

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
        auto first = std::cbegin(rScene.m_deleteTotal);
        auto last = std::cend(rScene.m_deleteTotal);
        SysRender::update_delete_groups(pRenderer->m_renderGroups, first, last);
        SysRenderGL::update_delete(pRenderer->m_renderGl, first, last);

        // Rotate and move the camera based on user inputs
        SysCameraController::update_view(
                pRenderer->m_camCtrl,
                rScene.m_basic.m_transform.get(pRenderer->m_camera), delta);
        SysCameraController::update_move(
                pRenderer->m_camCtrl,
                rScene.m_basic.m_transform.get(pRenderer->m_camera),
                delta, true);

        render_test_scene(rApp, rScene, *pRenderer);

        SysRender::clear_dirty_materials(rScene.m_drawing.m_materials);
    };
}

} // namespace testapp::physicstest
