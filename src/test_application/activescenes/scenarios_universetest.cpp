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
#if 0
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

#include <osp/universe/universe.h>
#include <osp/universe/coordinates.h>

#include <osp/Resource/resources.h>
#include <osp/CommonMath.h>
#include <osp/logging.h>

#include <longeron/id_management/registry.hpp>

#include <Magnum/GL/DefaultFramebuffer.h>

#include <Corrade/Containers/ArrayViewStl.h>

#include <newtondynamics_physics/ospnewton.h>
#include <newtondynamics_physics/SysNewton.h>

#include <random>

using osp::active::ActiveEnt;
using osp::active::active_sparse_set_t;
using osp::active::MeshIdOwner_t;

using osp::phys::EShape;

using osp::Vector3;
using osp::Vector3d;
using osp::Quaternion;
using osp::Quaterniond;
using osp::Matrix3;
using osp::Matrix4;

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

using Corrade::Containers::Array;

namespace testapp::scenes
{

constexpr float gc_physTimestep = 1.0 / 60.0f;
constexpr int gc_threadCount = 4; // note: not yet passed to Newton


/**
 * @brief Data used specifically by the universe test scene
 */
struct UniverseTestData
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

    // Universe is stored directly in the test scene for now
    osp::universe::Universe m_universe;
    osp::universe::CoSpaceId m_mainSpace{lgrn::id_null<osp::universe::CoSpaceId>()};
    std::vector<osp::universe::CoSpaceId> m_mainSatLanded;

    // Active area stuff
    // Non-centralized universe allows coordinate spaces to be stored externally
    osp::universe::CoSpaceCommon m_areaSpace;
    osp::universe::Vector3g m_areaCenter;
    bool m_areaRotating;
};


void UniverseTest::setup_scene(CommonTestScene &rScene, osp::PkgId const pkg)
{
    using namespace osp::active;

    auto &rScnPhys  = rScene.emplace<PhysicsData>();
    auto &rScnNwt   = rScene.emplace<ospnewton::ACtxNwtWorld>(gc_threadCount);
    auto &rScnTest  = rScene.emplace<UniverseTestData>();

    rScene.m_onCleanup.push_back(&PhysicsData::cleanup);

    osp::Resources &rResources = *rScene.m_pResources;

    // Convenient function to get a reference-counted mesh owner
    auto const quick_add_mesh = [&rScene, &rResources, pkg] (std::string_view const name) -> MeshIdOwner_t
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
    ActiveEnt const camEnt = rScene.m_activeIds.create();

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

    static constexpr Vector3 const sc_floorSize{64.0f, 64.0f, 1.0f};
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

    // Setup universe
    using namespace osp::universe;

    constexpr int planetCount       = 64;
    constexpr int c_seed            = 1337;
    constexpr spaceint_t c_maxDist  = 20000ul << 10;
    constexpr float c_maxVel        = 800.0f;

    Universe &rUni = rScnTest.m_universe;

    // Create Coordinate Spaces
    rScnTest.m_mainSpace = rUni.m_coordIds.create();
    rScnTest.m_mainSatLanded.resize(planetCount);
    rUni.m_coordIds.create(std::begin(rScnTest.m_mainSatLanded), std::end(rScnTest.m_mainSatLanded));
    rUni.m_coordCommon.resize(rUni.m_coordIds.capacity());

    // Setup Main coordinate space
    CoSpaceCommon &rMainSpace = rUni.m_coordCommon[std::size_t(rScnTest.m_mainSpace)];
    rMainSpace.m_satCount = planetCount;
    rMainSpace.m_satCapacity = planetCount;

    // TODO: alignment. also see Corrade alignedAlloc
    std::size_t const dataSize = (sizeof(double) * (3 + 4) + sizeof(spaceint_t) * 3) * rMainSpace.m_satCapacity;
    rMainSpace.m_data = Array<unsigned char>{dataSize};

    // Arrange position and velocity in XXXXX... YYYYY... ZZZZZ...
    std::size_t const posStart = 0;
    std::size_t const posComp = sizeof(spaceint_t) * rMainSpace.m_satCapacity;
    rMainSpace.m_satPositions = {
        .m_offsets = {posStart + posComp * 0,
                      posStart + posComp * 1,
                      posStart + posComp * 2},
        .m_stride  = sizeof(spaceint_t) };

    std::size_t const velStart = posStart + posComp * 3;
    std::size_t const velComp = sizeof(double) * rMainSpace.m_satCapacity;
    rMainSpace.m_satVelocities = {
        .m_offsets = {velStart + velComp * 0,
                      velStart + velComp * 1,
                      velStart + velComp * 2},
        .m_stride  = sizeof(double) };

    // Quaternion rotation as XYZW XYZW XYZW
    std::size_t const rotStart = velStart + velComp * 3;
    rMainSpace.m_satRotations = {
        .m_offsets = {rotStart + sizeof(double) * 0,
                      rotStart + sizeof(double) * 1,
                      rotStart + sizeof(double) * 2,
                      rotStart + sizeof(double) * 3},
        .m_stride  = sizeof(double) * 4 };

    auto const [x, y, z]        = sat_views(rMainSpace.m_satPositions,  rMainSpace.m_data, planetCount);
    auto const [vx, vy, vz]     = sat_views(rMainSpace.m_satVelocities, rMainSpace.m_data, planetCount);
    auto const [qx, qy, qz, qw] = sat_views(rMainSpace.m_satRotations,  rMainSpace.m_data, planetCount);

    std::mt19937 gen(c_seed);
    std::uniform_int_distribution<spaceint_t> posDist(-c_maxDist, c_maxDist);
    std::uniform_real_distribution<double> velDist(-c_maxVel, c_maxVel);

    for (std::size_t i = 0; i < planetCount; ++i)
    {
        // Assign each planet random ositions and velocities
        x[i] = posDist(gen);
        y[i] = posDist(gen);
        z[i] = posDist(gen);
        vx[i] = velDist(gen);
        vy[i] = velDist(gen);
        vz[i] = velDist(gen);

        // No rotation
        qx[i] = 0.0;
        qy[i] = 0.0;
        qz[i] = 0.0;
        qw[i] = 1.0;

        // Setup Landed coordinate space for this planet
        CoSpaceId const id = rScnTest.m_mainSatLanded[i];
        CoSpaceCommon &rLanded = rUni.m_coordCommon[std::size_t(id)];
        rLanded.m_parent = rScnTest.m_mainSpace;
        rLanded.m_parentSat = SatId(i);
    }

    rScnTest.m_areaSpace.m_parent = rScnTest.m_mainSpace;
    rScnTest.m_areaSpace.m_position = Vector3g{400, 400, 400} * 1024;
}

static void update_test_scene_delete(CommonTestScene &rScene)
{
    using namespace osp::active;

    auto &rScnTest  = rScene.get<UniverseTestData>();
    auto &rScnPhys  = rScene.get<PhysicsData>();
    auto &rScnNwt   = rScene.get<ospnewton::ACtxNwtWorld>();

    rScene.update_hierarchy_delete();

    auto const& first = std::cbegin(rScene.m_deleteTotal);
    auto const& last  = std::cend(rScene.m_deleteTotal);

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
static void update_test_scene(CommonTestScene& rScene, float const delta)
{
    using namespace osp::active;
    using namespace ospnewton;
    using Corrade::Containers::ArrayView;

    auto &rScnTest  = rScene.get<UniverseTestData>();
    auto &rScnPhys  = rScene.get<PhysicsData>();
    auto &rScnNwt   = rScene.get<ospnewton::ACtxNwtWorld>();

    // Clear all drawing-related dirty flags
    SysRender::clear_dirty_all(rScene.m_drawing);

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

    // Shape Thrower system, consumes rScene.m_toThrow and creates shapes
    for (UniverseTestData::ThrowShape const &rThrow : std::exchange(rScnTest.m_toThrow, {}))
    {
        ActiveEnt const shapeEnt = add_rigid_body_quick(
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


static void update_universe(CommonTestScene& rScene, float  const delta)
{
    using namespace osp::universe;

    auto &rScnTest = rScene.get<UniverseTestData>();
    auto &rScnPhys = rScene.get<PhysicsData>();
    Universe &rUni = rScnTest.m_universe;
    CoSpaceCommon &rMainSpace = rUni.m_coordCommon[std::size_t(rScnTest.m_mainSpace)];

    float const scale = osp::math::mul_2pow<float, int>(1.0f, -rMainSpace.m_precision);
    float const scaleDelta = delta / scale;

    auto const [x, y, z]        = sat_views(rMainSpace.m_satPositions,  rMainSpace.m_data, rMainSpace.m_satCount);
    auto const [vx, vy, vz]     = sat_views(rMainSpace.m_satVelocities, rMainSpace.m_data, rMainSpace.m_satCount);
    auto const [qx, qy, qz, qw] = sat_views(rMainSpace.m_satRotations,  rMainSpace.m_data, rMainSpace.m_satCount);

    // Phase 1: Move satellites

    for (std::size_t i = 0; i < rMainSpace.m_satCount; ++i)
    {
        x[i] += vx[i] * scaleDelta;
        y[i] += vy[i] * scaleDelta;
        z[i] += vz[i] * scaleDelta;

        // Apply arbitrary inverse-square gravity towards origin
        Vector3d const pos       = Vector3d( Vector3g( x[i], y[i], z[i] ) ) * scale;
        float const r           = pos.length();
        float const c_gm        = 10000000000.0f;
        Vector3d const accel     = -pos * delta * c_gm / (r * r * r);

        vx[i] += accel.x();
        vy[i] += accel.y();
        vz[i] += accel.z();

        // Rotate based on i, semi-random
        Vector3d const axis = Vector3d{std::sin(i), std::cos(i), double(i % 8 - 4)}.normalized();
        Magnum::Radd const speed{(i % 16) / 16.0};

        Quaterniond const rot =   Quaterniond{{qx[i], qy[i], qz[i]}, qw[i]}
                                * Quaterniond::rotation(speed * delta, axis);
        qx[i] = rot.vector().x();
        qy[i] = rot.vector().y();
        qz[i] = rot.vector().z();
        qw[i] = rot.scalar();
    }

    // Phase 2: Transfers and stuff

    constexpr float captureDist = 500.0f;

    CoSpaceCommon &rAreaSpace = rScnTest.m_areaSpace;

    Vector3g const cameraPos{rAreaSpace.m_rotation.transformVector(Vector3d(rScnTest.m_areaCenter))};
    Vector3g const areaPos{rAreaSpace.m_position + cameraPos};

    if (rAreaSpace.m_parent == rScnTest.m_mainSpace)
    {
        // Not captured within planet, search for nearby planet
        std::size_t nearbyPlanet = rMainSpace.m_satCount;
        for (std::size_t i = 0; i < rMainSpace.m_satCount; ++i)
        {
            Vector3 const diff = (Vector3( x[i], y[i], z[i] ) - Vector3(areaPos)) * scale;
            if (diff.length() < captureDist)
            {
                nearbyPlanet = i;
                break;
            }
        }

        if (nearbyPlanet < rMainSpace.m_satCount)
        {
            OSP_LOG_INFO("Captured into Satellite {} under CoordSpace {}",
                         nearbyPlanet, int(rScnTest.m_mainSatLanded[nearbyPlanet]));

            CoSpaceId const landedId = rScnTest.m_mainSatLanded[nearbyPlanet];
            CoSpaceCommon &rLanded = rUni.m_coordCommon[std::size_t(landedId)];

            CoSpaceTransform const landedTf = coord_get_transform(rLanded, rLanded, x, y, z, qx, qy, qz, qw);
            CoordTransformer const mainToLanded = coord_parent_to_child(rMainSpace, landedTf);

            // Transfer area from Main to Landed
            rAreaSpace.m_parent = landedId;
            rAreaSpace.m_position = mainToLanded.transform_position(rAreaSpace.m_position);
            rAreaSpace.m_rotation = mainToLanded.rotation() * rAreaSpace.m_rotation;
        }
    }
    else
    {
        // Currently within planet, try to escape planet
        Vector3 const diff = Vector3(areaPos) * scale;
        if (diff.length() > captureDist)
        {
            OSP_LOG_INFO("Leaving planet");

            CoSpaceId const landedId = rScnTest.m_areaSpace.m_parent;
            CoSpaceCommon &rLanded = rUni.m_coordCommon[std::size_t(landedId)];

            CoSpaceTransform const landedTf = coord_get_transform(rLanded, rLanded, x, y, z, qx, qy, qz, qw);
            CoordTransformer const landedToMain = coord_child_to_parent(rMainSpace, landedTf);

            // Transfer area from Landed to Main
            rAreaSpace.m_parent = rScnTest.m_mainSpace;
            rAreaSpace.m_position = landedToMain.transform_position(rAreaSpace.m_position);
            rAreaSpace.m_rotation = landedToMain.rotation() * rAreaSpace.m_rotation;
        }
    }
}

//-----------------------------------------------------------------------------

struct UniverseTestRenderer
{
    UniverseTestRenderer(ActiveApplication &rApp)
     : m_camCtrl(rApp.get_input_handler())
     , m_btnThrow(m_camCtrl.m_controls.button_subscribe("debug_throw"))
    { }

    ACtxCameraController m_camCtrl;

    osp::input::EButtonControlIndex m_btnThrow;
};

void UniverseTest::setup_renderer_gl(CommonSceneRendererGL& rRenderer, CommonTestScene& rScene, ActiveApplication& rApp) noexcept
{
    using namespace osp::active;

    auto &rControls = rRenderer.emplace<UniverseTestRenderer>(rApp);

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
        auto &rScnTest = rScene.get<UniverseTestData>();
        auto &rScnPhys = rScene.get<PhysicsData>();
        auto &rControls = rRenderer.get<UniverseTestRenderer>();

        // Throw a sphere when the throw button is pressed
        if (rControls.m_camCtrl.m_controls.button_held(rControls.m_btnThrow))
        {
            Matrix4 const &camTf = rScene.m_basic.m_transform.get(rRenderer.m_camera).m_transform;
            float const speed = 120;
            float const dist = 5.0f; // Distance from camera to spawn spheres
            rScnTest.m_toThrow.emplace_back(UniverseTestData::ThrowShape{
                    camTf.translation() - camTf.backward() * dist, // position
                    -camTf.backward() * speed, // velocity
                    Vector3{1.0f}, // size (radius)
                    100.0f, // mass
                    EShape::Sphere}); // shape
        }

        update_universe(rScene, gc_physTimestep);

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
        rScnTest.m_areaCenter = osp::universe::Vector3g(
                osp::math::mul_2pow<Vector3, int>(
                    rControls.m_camCtrl.m_target.value(),
                    rScnTest.m_areaSpace.m_precision));

        rRenderer.update_delete(rScene.m_deleteTotal);
        rRenderer.sync(rApp, rScene);
        rRenderer.prepare_fbo(rApp);
        rRenderer.draw_entities(rApp, rScene);

        using Magnum::GL::Mesh;

        RenderGL &rRenderGl = rApp.get_render_gl();
        auto const mesh_from_id = [&rScnPhys, &rScene, &rRenderGl] (MeshId const meshId) -> Mesh&
        {
            osp::ResId const meshRes    = rScene.m_drawingRes.m_meshToRes.at(meshId);
            MeshGlId const meshGlId     = rRenderGl.m_resToMesh.at(meshRes);
            return rRenderGl.m_meshGl.get(meshGlId);
        };

        Mesh &rBox      = mesh_from_id(rScnPhys.m_shapeToMesh.at(EShape::Box));
        Mesh &rSphere   = mesh_from_id(rScnPhys.m_shapeToMesh.at(EShape::Sphere));

        ACompCamera const &rCamera = rScene.m_basic.m_camera.get(rRenderer.m_camera);
        ACompDrawTransform const &cameraDrawTf
                = rRenderer.m_renderGl.m_drawTransform.get(rRenderer.m_camera);
        ViewProjMatrix viewProj{
                cameraDrawTf.m_transformWorld.inverted(),
                rCamera.calculate_projection()};

        auto &rPhong = rRenderer.m_phong.m_shaderUntextured;
        auto &rVisualizer = rRenderer.m_visualizer.m_shader;

        // Cursor
        rPhong.setDiffuseColor(0xFFFFFF_rgbf)
              .setNormalMatrix(Matrix3{})
              .setTransformationMatrix(viewProj.m_view * Matrix4::translation(rControls.m_camCtrl.m_target.value()))
              .setProjectionMatrix(viewProj.m_proj)
              .draw(rBox);

        // Origin indicator
        rPhong.setDiffuseColor(0xFF0000_rgbf)
              .setTransformationMatrix(viewProj.m_view * Matrix4::scaling({400, 10, 10}))
              .draw(rBox);
        rPhong.setDiffuseColor(0x00FF00_rgbf)
              .setTransformationMatrix(viewProj.m_view * Matrix4::scaling({10, 400, 10}))
              .draw(rBox);
        rPhong.setDiffuseColor(0x0000FF_rgbf)
              .setTransformationMatrix(viewProj.m_view * Matrix4::scaling({10, 10, 400}))
              .draw(rBox);

        using namespace osp::universe;
        Universe &rUni = rScnTest.m_universe;

        CoSpaceCommon &rMainSpace = rUni.m_coordCommon[std::size_t(rScnTest.m_mainSpace)];
        auto const [x, y, z]        = sat_views(rMainSpace.m_satPositions, rMainSpace.m_data, rMainSpace.m_satCount);
        auto const [qx, qy, qz, qw] = sat_views(rMainSpace.m_satRotations, rMainSpace.m_data, rMainSpace.m_satCount);

        // Calculate transform from universe to area/local-space for rendering.
        // This can be generalized by finding a common ancestor within the tree
        // of coordinate spaces. Since there's only two possibilities, an if
        // statement works.
        CoordTransformer mainToArea;
        if (rScnTest.m_areaSpace.m_parent == rScnTest.m_mainSpace)
        {
            mainToArea = coord_parent_to_child(rMainSpace, rScnTest.m_areaSpace);
        }
        else
        {
            CoSpaceId const landedId = rScnTest.m_areaSpace.m_parent;
            CoSpaceCommon &rLanded = rUni.m_coordCommon[std::size_t(landedId)];

            CoSpaceTransform const landedTf = coord_get_transform(rLanded, rLanded, x, y, z, qx, qy, qz, qw);
            CoordTransformer const mainToLanded = coord_parent_to_child(rMainSpace, landedTf);
            CoordTransformer const landedToArea = coord_parent_to_child(landedTf, rScnTest.m_areaSpace);

            mainToArea = coord_composite(landedToArea, mainToLanded);
        }
        osp::Quaternion const mainToAreaRot{mainToArea.rotation()};

        float const scale = osp::math::mul_2pow<float, int>(1.0f, -rMainSpace.m_precision);

        // Draw black hole
        Vector3 const blackHolePos = Vector3(mainToArea.transform_position({})) * scale;
        rVisualizer
            .setColor(0x0E0E0E_rgbf)
            .setTransformationMatrix(
                    viewProj.m_view
                    * Matrix4::translation(blackHolePos)
                    * Matrix4::scaling({200, 200, 200})
                    * Matrix4{mainToAreaRot.toMatrix()} )
            .draw(rSphere);

        // Draw planets
        rVisualizer.setColor(0xFFFFFF_rgbf);
        for (std::size_t i = 0; i < rMainSpace.m_satCount; ++i)
        {
            Vector3g const relative = mainToArea.transform_position({x[i], y[i], z[i]});
            Vector3 const relativeMeters = Vector3(relative) * scale;

            Quaterniond const rot{{qx[i], qy[i], qz[i]}, qw[i]};

            rVisualizer
                .setColor(0xFFFFFF_rgbf)
                .setTransformationMatrix(
                        viewProj.m_view
                        * Matrix4::translation(relativeMeters)
                        * Matrix4::scaling({200, 200, 200})
                        * Matrix4{(mainToAreaRot * Quaternion{rot}).toMatrix()} )
                .draw(rSphere);
        }

        rRenderer.display(rApp);
    };
}

} // namespace testapp::UniverseTest
#endif
