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

#include <osp/id_registry.h>
#include <osp/Resource/Package.h>

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

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

namespace testapp::physicstest
{

// Materials used by the test scene. A more general application may want to
// generate IDs at runtime, and map them to named identifiers.
constexpr int const gc_mat_common      = 0;
constexpr int const gc_mat_visualizer  = 1;

constexpr int const gc_maxMaterials = 2;

/**
 * @brief State of the entire engine test scene
 */
struct PhysicsTestScene
{
    // ID registry generates entity IDs, and keeps track of which ones exist
    osp::IdRegistry<osp::active::ActiveEnt> m_activeIds;

    // Components and supporting data structures
    osp::active::ACtxBasic          m_basic;
    osp::active::ACtxDrawing        m_drawing;

    // Physics
    std::unique_ptr<ospnewton::ACtxNwtWorld> m_nwtWorld;
    osp::active::ACtxPhysics        m_physics;
    osp::active::ACtxPhysInputs     m_physIn;

    active_sparse_set_t             m_hasGravity;

    osp::active::ACtxHierBody       m_hierBody;

    // Hierarchy root, needs to exist so all hierarchy entities are connected
    osp::active::ActiveEnt          m_hierRoot;

    osp::DependRes<MeshData>        m_meshCube;
    osp::DependRes<MeshData>        m_meshCylinder;
    osp::DependRes<MeshData>        m_meshSphere;

    float m_ballTimer{0.0f};

    struct BallThrow
    {
        Vector3 m_position;
        Vector3 m_velocity;
        float m_radius;
        float m_mass;
    };

    // Queue for balls to throw
    std::vector<BallThrow> m_throwBall;

};

std::optional<osp::active::ACompMesh> mesh_from_shape(PhysicsTestScene& rScene, osp::phys::EShape shape)
{
    using osp::active::ACompMesh;

    switch (shape)
    {
    case EShape::Box:
        return {{ rScene.m_meshCube }};
    case EShape::Cylinder:
        return {{ rScene.m_meshCylinder }};
    case EShape::Sphere:
        return {{ rScene.m_meshSphere }};
    default:
        return std::nullopt;
    }
}

ActiveEnt add_solid(
        PhysicsTestScene& rScene, ActiveEnt parent,
        EShape shape, Matrix4 transform, int material, float mass)
{
    using namespace osp::active;

    // Make entity
    ActiveEnt ent = rScene.m_activeIds.create();

    // Add mesh
    rScene.m_drawing.m_mesh.emplace(
            ent, mesh_from_shape(rScene, shape).value() );
    rScene.m_drawing.m_meshDirty.push_back(ent);

    // Add material to cube
    MaterialData &rMaterial = rScene.m_drawing.m_materials[material];
    rMaterial.m_comp.emplace(ent);
    rMaterial.m_added.push_back(ent);

    // Add transform and draw transform
    rScene.m_basic.m_transform.emplace(ent, ACompTransform{transform});
    rScene.m_drawing.m_drawTransform.emplace(ent);

    // Add opaque and visible component
    rScene.m_drawing.m_opaque.emplace(ent);
    rScene.m_drawing.m_visible.emplace(ent);

    // Add physics stuff
    rScene.m_physics.m_shape.emplace(ent, shape);
    rScene.m_physics.m_solidCollider.emplace(ent);
    Vector3 const inertia
            = osp::phys::collider_inertia_tensor(shape, transform.scaling(), mass);
    rScene.m_hierBody.m_ownDyn.emplace( ent, ACompSubBody{ inertia, mass } );
    rScene.m_physIn.m_colliderDirty.push_back(ent);

    // Add to hierarchy
    SysHierarchy::add_child(rScene.m_basic.m_hierarchy, parent, ent);

    return ent;
}

ActiveEnt add_quick_ball(PhysicsTestScene &rScene, Vector3 position, Vector3 velocity, float radius, float mass)
{
    using namespace osp::active;

    ActiveEnt ballRoot = rScene.m_activeIds.create();

    // Add transform and draw transform
    rScene.m_basic.m_transform.emplace(
            ballRoot, ACompTransform{Matrix4::translation(position)});
    rScene.m_drawing.m_drawTransform.emplace(ballRoot);

    // Add to hierarchy
    SysHierarchy::add_child(rScene.m_basic.m_hierarchy, rScene.m_hierRoot, ballRoot);

    ActiveEnt ballCollider = add_solid(
            rScene, ballRoot, EShape::Sphere,
            Matrix4::scaling(Vector3{radius}), gc_mat_visualizer, 0.0f);

    // Make ball root a dynamic rigid body
    rScene.m_physics.m_hasColliders.emplace(ballRoot);
    rScene.m_physics.m_physBody.emplace(ballRoot);
    rScene.m_physics.m_physLinearVel.emplace(ballRoot);
    rScene.m_physics.m_physAngularVel.emplace(ballRoot);
    osp::active::ACompPhysDynamic &rDyn = rScene.m_physics.m_physDynamic.emplace(ballRoot);
    rDyn.m_totalMass = 1.0f;

    // make gravity affect ball
    rScene.m_hasGravity.emplace(ballRoot);

    // Set velocity
    rScene.m_physIn.m_setVelocity.emplace_back(ballRoot, velocity);

    return ballRoot;
}

entt::any setup_scene(osp::Package &rPkg)
{
    using namespace osp::active;

    entt::any sceneAny = entt::make_any<PhysicsTestScene>();
    PhysicsTestScene &rScene = entt::any_cast<PhysicsTestScene&>(sceneAny);

    rScene.m_nwtWorld = std::make_unique<ospnewton::ACtxNwtWorld>(4);

    rScene.m_meshCube       = rPkg.get_or_reserve<MeshData>("cube");
    rScene.m_meshCylinder   = rPkg.get_or_reserve<MeshData>("cylinder");
    rScene.m_meshSphere     = rPkg.get_or_reserve<MeshData>("sphere");

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
    rScene.m_drawing.m_drawTransform.emplace(camEnt);

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
        rScene.m_drawing.m_drawTransform.emplace(floorRoot);


        // Create floor mesh entity
        ActiveEnt floorMesh = rScene.m_activeIds.create();

        // Add grid mesh to floor mesh
        rScene.m_drawing.m_mesh.emplace(
                floorMesh, ACompMesh{ rPkg.get<MeshData>("grid64") });
        rScene.m_drawing.m_meshDirty.push_back(floorMesh);

        // Add mesh visualizer material to floor mesh
        MaterialData &rMatCommon
                = rScene.m_drawing.m_materials[gc_mat_visualizer];
        rMatCommon.m_comp.emplace(floorMesh);
        rMatCommon.m_added.push_back(floorMesh);

        // Add transform, draw transform, opaque, and visible
        rScene.m_basic.m_transform.emplace(
                floorMesh, ACompTransform{Matrix4::scaling({64.0f, 64.0f, 1.0f})});
        rScene.m_drawing.m_drawTransform.emplace(floorMesh);
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

    //rScene.m_physIn.m_physNetTorque.emplace(cube, ACompPhysNetTorque{{5.0f, 0.0f, 0.0f}});
    rScene.m_ballTimer += delta;
    if (rScene.m_ballTimer >= 0.2f)
    {
        rScene.m_throwBall.emplace_back(PhysicsTestScene::BallThrow{
                Vector3{0.0f, 4.0f, 0.0f}, Vector3{0.0f}, 1.0f, 1.0f});
        rScene.m_ballTimer -= 0.2f;
    }

    // Ball Thrower consumer
    for (PhysicsTestScene::BallThrow const &rThrow : std::exchange(rScene.m_throwBall, {}))
    {
        add_quick_ball(rScene, rThrow.m_position, rThrow.m_velocity, rThrow.m_radius, rThrow.m_mass);
    }

    // Gravity System
    for (ActiveEnt ent : rScene.m_hasGravity)
    {
        acomp_storage_t<ACompPhysNetForce> &rNetForce
                = rScene.m_physIn.m_physNetForce;
        ACompPhysNetForce &rEntNetForce = rNetForce.contains(ent)
                                        ? rNetForce.get(ent)
                                        : rNetForce.emplace(ent);

        rEntNetForce.y() -= 9.81f;
    }

    SysNewton::update_colliders(
            rScene.m_physics, *rScene.m_nwtWorld,
            rScene.m_physIn.m_colliderDirty);

    auto const physIn = ArrayView<ACtxPhysInputs>(&rScene.m_physIn, 1);
    SysNewton::update_world(
            rScene.m_physics, *rScene.m_nwtWorld, physIn, rScene.m_basic.m_hierarchy,
            rScene.m_basic.m_transform, rScene.m_basic.m_transformControlled,
            rScene.m_basic.m_transformMutable);
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

    osp::active::ACtxRenderGL m_renderGl;

    osp::active::ActiveEnt m_camera;
    ACtxCameraController m_camCtrl;

    osp::shader::ACtxPhongData m_phong;
    osp::shader::ACtxMeshVisualizerData m_visualizer;

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
        ActiveApplication& rApp, PhysicsTestScene& rScene,
        PhysicsTestRenderer& rRenderer)
{
    using namespace osp::active;
    using namespace osp::shader;
    using Magnum::GL::Framebuffer;
    using Magnum::GL::FramebufferClear;
    using Magnum::GL::Texture2D;

    osp::Package &rGlResources = rApp.get_gl_resources();

    RenderGroup &rGroupFwdOpaque
            = rRenderer.m_renderGroups.m_groups["fwd_opaque"];

    // Assign Phong shader to entities with the gc_mat_common material, and put
    // results into the fwd_opaque render group
    {
        MaterialData &rMatCommon = rScene.m_drawing.m_materials[gc_mat_common];
        Phong::assign_phong_opaque(
                rMatCommon.m_added, rGroupFwdOpaque.m_entities,
                rScene.m_drawing.m_opaque, rRenderer.m_renderGl.m_diffuseTexGl,
                rRenderer.m_phong);
        rMatCommon.m_added.clear();
    }

    // Same thing but with MeshVisualizer and gc_mat_visualizer
    {
        MaterialData &rMatVisualizer
                = rScene.m_drawing.m_materials[gc_mat_visualizer];
        MeshVisualizer::assign(
                rMatVisualizer.m_added, rGroupFwdOpaque.m_entities,
                rRenderer.m_visualizer);
        rMatVisualizer.m_added.clear();
    }

    // Load any required meshes
    SysRenderGL::compile_meshes(
            rScene.m_drawing.m_mesh, rScene.m_drawing.m_meshDirty,
            rRenderer.m_renderGl.m_meshGl, rApp.get_gl_resources());

    // Load any required textures
    SysRenderGL::compile_textures(
            rScene.m_drawing.m_diffuseTex, rScene.m_drawing.m_diffuseDirty,
            rRenderer.m_renderGl.m_diffuseTexGl, rApp.get_gl_resources());

    // Calculate hierarchy transforms
    SysHierarchy::sort(rScene.m_basic.m_hierarchy);
    SysRender::update_draw_transforms(
            rScene.m_basic.m_hierarchy,
            rScene.m_basic.m_transform,
            rScene.m_drawing.m_drawTransform);

    // Get camera, and calculate projection matrix and inverse transformation
    ACompCamera &rCamera = rScene.m_basic.m_camera.get(rRenderer.m_camera);
    ACompDrawTransform const &cameraDrawTf
            = rScene.m_drawing.m_drawTransform.get(rRenderer.m_camera);
    rCamera.m_viewport
            = osp::Vector2(Magnum::GL::defaultFramebuffer.viewport().size());
    rCamera.calculate_projection();
    rCamera.m_inverse = cameraDrawTf.m_transformWorld.inverted();

    // Bind offscreen FBO
    Framebuffer &rFbo = *rGlResources.get<Framebuffer>("offscreen_fbo");
    rFbo.bind();

    // Clear it
    rFbo.clear( FramebufferClear::Color | FramebufferClear::Depth
                | FramebufferClear::Stencil);

    // Forward Render fwd_opaque group to FBO
    SysRenderGL::render_opaque(
            rRenderer.m_renderGroups.m_groups.at("fwd_opaque"),
            rScene.m_drawing.m_visible, rCamera);

    // Display FBO
    Texture2D &rFboColor = *rGlResources.get<Texture2D>("offscreen_fbo_color");
    SysRenderGL::display_texture(rGlResources, rFboColor);
}

void load_gl_resources(ActiveApplication& rApp)
{
    using osp::shader::Phong;
    using osp::shader::MeshVisualizer;

    osp::Package &rGlResources = rApp.get_gl_resources();

    rGlResources.add<Phong>("textured", Phong{Phong::Flag::DiffuseTexture});
    rGlResources.add<Phong>("notexture", Phong{});

    rGlResources.add<MeshVisualizer>(
            "mesh_vis_shader",
            MeshVisualizer{ MeshVisualizer::Flag::Wireframe });

}

on_draw_t gen_draw(PhysicsTestScene& rScene, ActiveApplication& rApp)
{
    using namespace osp::active;
    using namespace osp::shader;

    // Create renderer data. This uses a shared_ptr to allow being stored
    // inside an std::function, which require copyable types
    std::shared_ptr<PhysicsTestRenderer> pRenderer
            = std::make_shared<PhysicsTestRenderer>(rApp);

    osp::Package &rGlResources = rApp.get_gl_resources();

    // Get or reserve Phong shaders. These are loaded in load_gl_resources,
    // which can be called before or after this function
    pRenderer->m_phong.m_shaderUntextured
            = rGlResources.get_or_reserve<Phong>("notexture");
    pRenderer->m_phong.m_shaderDiffuse
            = rGlResources.get_or_reserve<Phong>("textured");

    // Same with mesh visualizer shader
    pRenderer->m_visualizer.m_shader
            = rGlResources.get_or_reserve<MeshVisualizer>("mesh_vis_shader");

    pRenderer->m_phong.m_pDrawTf       = &rScene.m_drawing.m_drawTransform;
    pRenderer->m_phong.m_pDiffuseTexGl = &pRenderer->m_renderGl.m_diffuseTexGl;
    pRenderer->m_phong.m_pMeshGl       = &pRenderer->m_renderGl.m_meshGl;

    pRenderer->m_visualizer.m_pDrawTf  = &rScene.m_drawing.m_drawTransform;
    pRenderer->m_visualizer.m_pMeshGl  = &pRenderer->m_renderGl.m_meshGl;

    // Select first camera for rendering
    pRenderer->m_camera = rScene.m_basic.m_camera.at(0);

    pRenderer->m_camCtrl.m_target = osp::Vector3{0.0f, 2.0f, 0.0f};
    pRenderer->m_camCtrl.m_up = osp::Vector3{0.0f, 1.0f, 0.0f};

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
        if (pRenderer->m_controls.button_triggered(pRenderer->m_btnThrow))
        {
            ACompTransform const &camTf = rScene.m_basic.m_transform.get(pRenderer->m_camera);
            float const speed = 120;
            rScene.m_throwBall.emplace_back(PhysicsTestScene::BallThrow{
                    camTf.m_transform.translation(), -camTf.m_transform.backward() * speed, 1.0f, 1.0f});
        }

        update_test_scene(rScene, delta);

        SysCameraController::update_view(
                pRenderer->m_camCtrl,
                rScene.m_basic.m_transform.get(pRenderer->m_camera), delta);
        SysCameraController::update_move(
                pRenderer->m_camCtrl,
                rScene.m_basic.m_transform.get(pRenderer->m_camera),
                delta, true);

        render_test_scene(rApp, rScene, *pRenderer);
    };
}

} // namespace testapp::physicstest
