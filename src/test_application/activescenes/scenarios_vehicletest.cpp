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
#include "test_application/VehicleBuilder.h"

#include <adera/machines/links.h>

#include <osp/logging.h>

#include <osp/Active/basic.h>
#include <osp/Active/drawing.h>
#include <osp/Active/physics.h>
#include <osp/Active/parts.h>

#include <osp/Active/SysHierarchy.h>
#include <osp/Active/SysPrefabInit.h>

#include <osp/link/signal.h>

#include <osp/Resource/resources.h>
#include <osp/Resource/ImporterData.h>

#include <longeron/id_management/registry.hpp>

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/Trade/PbrMetallicRoughnessMaterialData.h>

#include <Corrade/Containers/ArrayViewStl.h>

#include <newtondynamics_physics/ospnewton.h>
#include <newtondynamics_physics/SysNewton.h>

#include <iostream>
#include <optional>

using adera::gc_mtMagicRocket;
using adera::gc_mtUserCtrl;
using osp::link::gc_ntNumber;

using osp::active::ActiveEnt;
using osp::active::active_sparse_set_t;

using osp::active::PartEnt_t;
using osp::active::RigidGroup_t;
using osp::active::TmpPrefabInit;

using osp::active::MeshIdOwner_t;

using osp::phys::EShape;

using osp::restypes::gc_importer;

using osp::Vector3;
using osp::Matrix3;
using osp::Matrix4;


// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

namespace testapp::scenes
{

constexpr float gc_physTimestep = 1.0 / 60.0f;
constexpr int gc_threadCount = 4; // note: not yet passed to Newton

struct PartInit
{
    PartEnt_t m_part;
    int m_initPrefabs;
};

/**
 * @brief Data used specifically by the physics test scene
 */
struct VehicleTestData
{
    // Required for std::is_copy_assignable to work properly inside of entt::any
    VehicleTestData() = default;
    VehicleTestData(VehicleTestData const& copy) = delete;
    VehicleTestData(VehicleTestData&& move) = default;

    active_sparse_set_t             m_hasGravity;
    active_sparse_set_t             m_removeOutOfBounds;

    // Timers for when to create boxes and cylinders
    float m_boxTimer{0.0f};
    float m_cylinderTimer{0.0f};
    float m_vehicleTimer{9.0f};

    struct ThrowShape
    {
        Vector3 m_position;
        Vector3 m_velocity;
        Vector3 m_size;
        float m_mass;
        EShape m_shape;
    };

    osp::ResId m_prefab;

    std::optional<VehicleBuilder> m_builder;

    // Queue for balls to throw
    std::vector<ThrowShape> m_toThrow;

    std::vector<TmpPrefabInit> m_initPrefabs;
    std::vector<ActiveEnt> m_initPrefabEnts;
    std::vector<PartInit> m_initParts;

    std::vector<RigidGroup_t> m_rigidDirty;
    std::vector<ActiveEnt> m_rigidToActive;

    osp::link::SignalValues_t<float> m_nodeNumValues;

    osp::link::UpdMachPerType_t m_updMachTypes;
    osp::link::UpdateNodes<float> m_updNodeNum;
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
    auto &rScnParts = rScene.emplace<ACtxParts>();

    // Add cleanup function to deal with reference counts
    // Note: The problem with destructors, is that REQUIRE storing pointers in
    //       the data, which is uglier to deal with
    rScene.m_onCleanup.push_back(&PhysicsData::cleanup);
    rScene.m_onCleanup.push_back([] (CommonTestScene& rScene) -> void
    {
        auto &rScnParts = rScene.get<ACtxParts>();
        for (osp::PrefabPair &rPrefabPair : rScnParts.m_partPrefabs)
        {
            rScene.m_pResources->owner_destroy(
                    osp::restypes::gc_importer, std::move(rPrefabPair.m_importer));
        }
    });

    rScnParts.m_machines.m_perType.resize(osp::link::MachTypeReg_t::size());
    rScnParts.m_nodePerType.resize(osp::link::NodeTypeReg_t::size());

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


    // Start making test vehicle

    VehicleBuilder &rBuilder = rScnTest.m_builder.emplace(rScene.m_pResources);

    auto const [ capsule, fueltank, engine ] = rBuilder.create_parts<3>();
    rBuilder.set_prefabs({
        { capsule,  "phCapsule" },
        { fueltank, "phFuselage" },
        { engine,   "phEngine" },
    });

    rBuilder.set_transform({
       { capsule,  Matrix4::translation({0, 0, 3}) },
       { fueltank, Matrix4::translation({0, 0, 0}) },
       { engine,   Matrix4::translation({0, 0, -3}) },
    });


    namespace ports_magicrocket = adera::ports_magicrocket;
    namespace ports_userctrl = adera::ports_userctrl;

    auto const [ pitch, yaw, roll, throttle ] = rBuilder.create_nodes<4>(gc_ntNumber);

    rBuilder.create_machine(capsule, gc_mtUserCtrl, {
        { ports_userctrl::gc_throttleOut,   throttle },
        { ports_userctrl::gc_pitchOut,      pitch },
        { ports_userctrl::gc_yawOut,        yaw },
        { ports_userctrl::gc_rollOut,       roll }
    } );

    rBuilder.create_machine(engine, gc_mtMagicRocket, {
        { ports_magicrocket::gc_throttleIn, throttle }
    } );

    rBuilder.finalize_machines();
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

static void update_links(CommonTestScene& rScene)
{
    using namespace osp::link;

    auto &rScnTest  = rScene.get<VehicleTestData>();
    auto &rScnParts = rScene.get<osp::active::ACtxParts>();

    Nodes const &rNumberNodes = rScnParts.m_nodePerType[gc_ntNumber];
    PerMachType &rRockets = rScnParts.m_machines.m_perType[gc_mtMagicRocket];

    update_signal_nodes<float>(
            rScnTest.m_updNodeNum.m_nodeDirty.ones(),
            rNumberNodes.m_nodeToMach,
            rScnParts.m_machines,
            Corrade::Containers::arrayView(rScnTest.m_updNodeNum.m_nodeNewValues),
            rScnTest.m_nodeNumValues,
            rScnTest.m_updMachTypes);

    for (MachLocalId local : rScnTest.m_updMachTypes[gc_mtMagicRocket].m_localDirty.ones())
    {
        MachAnyId const mach = rRockets.m_localToAny[local];
        lgrn::Span<NodeId const> portSpan = rNumberNodes.m_machToNode[mach];

        if (NodeId const thrNode = connected_node(portSpan, adera::ports_magicrocket::gc_throttleIn.m_port);
            thrNode != lgrn::id_null<NodeId>())
        {
            if (thrNode != lgrn::id_null<NodeId>())
            {
                float const thrNew = rScnTest.m_nodeNumValues[thrNode];
                OSP_LOG_INFO("Rocket {} reading node {} = {}", local, thrNode, thrNew);
            }
        }
    }
}

static void spawn_vehicle(CommonTestScene& rScene, VehicleData const &data)
{
    using namespace osp::active;

    auto &rScnTest  = rScene.get<VehicleTestData>();
    auto &rScnPhys  = rScene.get<PhysicsData>();
    auto &rScnNwt   = rScene.get<ospnewton::ACtxNwtWorld>();
    auto &rScnParts = rScene.get<ACtxParts>();

    Matrix4 const rootTf = Matrix4::translation({0.0f, 5.0f, 0.0f});

    // Setup vehicle's root entity as a single rigid body (for now)
    ActiveEnt const rigidEnt = rScene.m_activeIds.create();
    SysHierarchy::add_child(rScene.m_basic.m_hierarchy, rScene.m_hierRoot, rigidEnt);
    rScene.m_basic.m_transform                  .emplace(rigidEnt, rootTf);
    rScnPhys.m_physics.m_hasColliders           .emplace(rigidEnt);
    rScnPhys.m_physics.m_solid                  .emplace(rigidEnt);
    rScnPhys.m_physics.m_physBody               .emplace(rigidEnt);
    rScnPhys.m_physics.m_physLinearVel          .emplace(rigidEnt);
    rScnPhys.m_physics.m_physAngularVel         .emplace(rigidEnt);
    rScnTest.m_hasGravity                       .emplace(rigidEnt);
    //rScnTest.m_removeOutOfBounds.emplace(rigidEnt);
    osp::active::ACompPhysDynamic &rDyn
            = rScnPhys.m_physics.m_physDynamic  .emplace(rigidEnt);
    rDyn.m_totalMass    = 1.0f; // TODO
    rDyn.m_inertia      = {1.0f, 1.0f, 1.0f};

    // Put all parts into a single rigid group
    RigidGroup_t const rigid = rScnParts.m_rigidIds.create();
    rScnTest.m_rigidToActive     .resize(rScnParts.m_rigidIds.capacity());
    rScnParts.m_rigidToParts.ids_reserve(rScnParts.m_rigidIds.capacity());
    rScnParts.m_rigidToParts.data_reserve(
            rScnParts.m_rigidToParts.data_capacity() + data.m_partIds.size());

    rScnTest.m_rigidToActive[rigid] = rigidEnt;
    rScnTest.m_rigidDirty.push_back(rigid);

    // Create part entities
    rScnParts.m_rigidToParts.emplace(rigid, data.m_partIds.size());
    lgrn::Span<PartEnt_t> rigidPartsSpan = rScnParts.m_rigidToParts[rigid];
    rScnParts.m_partIds.create(std::begin(rigidPartsSpan), std::end(rigidPartsSpan));
    rScnParts.m_partPrefabs         .resize(rScnParts.m_partIds.capacity());
    rScnParts.m_partTransformRigid  .resize(rScnParts.m_partIds.capacity());
    rScnParts.m_partRigids          .resize(rScnParts.m_partIds.capacity());

    std::vector<PartEnt_t> remapPart(data.m_partIds.capacity());

    // Copy Part data from VehicleBuilder to scene
    auto dstPartIt = std::begin(rigidPartsSpan);
    for (uint32_t srcPart : data.m_partIds.bitview().zeros())
    {
        PartEnt_t const dstPart = *dstPartIt;
        std::advance(dstPartIt, 1);

        remapPart[srcPart] = dstPart;

        osp::PrefabPair const& prefabPairSrc = data.m_partPrefabs[srcPart];
        osp::PrefabPair prefabPairDst{
            rScene.m_pResources->owner_create(gc_importer, prefabPairSrc.m_importer),
            prefabPairSrc.m_prefabId
        };
        rScnParts.m_partPrefabs[dstPart]        = std::move(prefabPairDst);
        rScnParts.m_partTransformRigid[dstPart] = data.m_partTransforms[srcPart];
        rScnParts.m_partRigids[dstPart]         = rigid;

        // Add Prefab and Part init events
        int initPfIndex         = rScnTest.m_initPrefabs.size();
        TmpPrefabInit &rInitPf  = rScnTest.m_initPrefabs.emplace_back();
        rInitPf.m_importerRes   = prefabPairSrc.m_importer;
        rInitPf.m_prefabId      = prefabPairSrc.m_prefabId;
        rInitPf.m_pTransform    = &data.m_partTransforms[srcPart];
        rInitPf.m_parent        = rigidEnt;
        rScnTest.m_initParts.emplace_back(PartInit{dstPart, initPfIndex});
    }

    using namespace osp::link;

    // remapMach[source MachAnyId] -> destination MachAnyId
    std::vector<MachAnyId> remapMach(data.m_machines.m_ids.capacity());

    // Make new machines in dst for each machine in source
    copy_machines(data.m_machines, rScnParts.m_machines, remapMach);

    rScnParts.m_machineToPart.resize(rScnParts.m_machines.m_ids.capacity());
    for (MachAnyId srcMach : data.m_machines.m_ids.bitview().zeros())
    {
        MachAnyId const dstMach = remapMach[srcMach];
        rScnParts.m_machineToPart[dstMach] = remapPart[std::size_t(data.m_machToPart[srcMach])];
    }

    // Do the same for nodes
    std::vector<NodeId> remapNodes(data.m_nodePerType[gc_ntNumber].m_nodeIds.capacity());
    copy_nodes(
            data.m_nodePerType[gc_ntNumber],
            data.m_machines,
            remapMach,
            rScnParts.m_nodePerType[gc_ntNumber],
            rScnParts.m_machines, remapNodes);

    // TODO: copy node values
    rScnTest.m_nodeNumValues.resize(rScnParts.m_nodePerType[gc_ntNumber].m_nodeIds.size());
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
    auto &rScnParts = rScene.get<ACtxParts>();

    update_links(rScene);

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

    rScnTest.m_initPrefabs.clear();
    rScnTest.m_initParts.clear();
    rScnTest.m_rigidDirty.clear();

    // Create a vehicle every 10 seconds
    rScnTest.m_vehicleTimer += delta;
    if (rScnTest.m_vehicleTimer >= 10.0f)
    {
        rScnTest.m_vehicleTimer -= 10.0f;

        spawn_vehicle(rScene, rScnTest.m_builder->data());
    }

    // Count total number of entities needed to be created for prefabs
    std::size_t totalEnts = 0;
    for (TmpPrefabInit& rPrefab : rScnTest.m_initPrefabs)
    {
        auto const &rPrefabData = rScene.m_pResources->data_get<osp::Prefabs>(gc_importer, rPrefab.m_importerRes);
        auto const objects = rPrefabData.m_prefabs[rPrefab.m_prefabId];

        totalEnts += objects.size();
    }

    // Initialize prefab entities
    rScnTest.m_initPrefabEnts.resize(totalEnts);
    std::size_t pos = 0;
    for (TmpPrefabInit& rPrefab : rScnTest.m_initPrefabs)
    {
        auto const &rPrefabData = rScene.m_pResources->data_get<osp::Prefabs>(gc_importer, rPrefab.m_importerRes);
        auto const objects = rPrefabData.m_prefabs[rPrefab.m_prefabId];

        ActiveEnt *pEnts = &rScnTest.m_initPrefabEnts.data()[pos];

        rScene.m_activeIds.create(pEnts, objects.size());
        rPrefab.m_prefabToEnt = {pEnts, objects.size()};
        pos += objects.size();
    }

    // Populate PartEnt<->ActiveEnt mmapping, now that the prefabs exist
    rScnParts.m_partToActive.resize(rScnParts.m_partIds.capacity());
    rScnParts.m_activeToPart.resize(rScene.m_activeIds.capacity());
    for (PartInit& rPartInit : rScnTest.m_initParts)
    {
        ActiveEnt const root = rScnTest.m_initPrefabs[rPartInit.m_initPrefabs].m_prefabToEnt.front();

        rScnParts.m_partToActive[rPartInit.m_part] = root;
        rScnParts.m_activeToPart[std::size_t(root)] = rPartInit.m_part;
    }

    // Init prefab hierarchy: Add hierarchy components
    SysPrefabInit::init_hierarchy(rScnTest.m_initPrefabs, *rScene.m_pResources, rScene.m_basic.m_hierarchy);

    // Init prefab transforms: Add transform components
    SysPrefabInit::init_transforms(rScnTest.m_initPrefabs, *rScene.m_pResources, rScene.m_basic.m_transform);

    // init prefab drawables
    SysPrefabInit::init_drawing(rScnTest.m_initPrefabs, *rScene.m_pResources, rScene.m_drawing, rScene.m_drawingRes, rScene.m_matCommon);

    // Init prefab physics
    SysPrefabInit::init_physics(rScnTest.m_initPrefabs, *rScene.m_pResources, rScnPhys.m_physIn, rScnPhys.m_physics, rScnPhys.m_hierBody);

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
     , m_btnSwitch(m_camCtrl.m_controls.button_subscribe("game_switch"))
     , m_btnThrMax(m_camCtrl.m_controls.button_subscribe("vehicle_thr_max"))
     , m_btnThrMin(m_camCtrl.m_controls.button_subscribe("vehicle_thr_min"))
     , m_btnThrMore(m_camCtrl.m_controls.button_subscribe("vehicle_thr_more"))
     , m_btnThrLess(m_camCtrl.m_controls.button_subscribe("vehicle_thr_less"))
    { }

    ACtxCameraController m_camCtrl;

    osp::input::EButtonControlIndex m_btnThrow;

    osp::input::EButtonControlIndex m_btnSwitch;

    osp::input::EButtonControlIndex m_btnThrMax;
    osp::input::EButtonControlIndex m_btnThrMin;
    osp::input::EButtonControlIndex m_btnThrMore;
    osp::input::EButtonControlIndex m_btnThrLess;

    osp::link::MachLocalId m_selectedUsrCtrl{lgrn::id_null<osp::link::MachLocalId>()};
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

    rRenderer.m_onDraw = [] (
            CommonSceneRendererGL& rRenderer, CommonTestScene& rScene,
            ActiveApplication& rApp, float delta) noexcept
    {
        using namespace osp::link;

        auto &rScnTest = rScene.get<VehicleTestData>();
        auto &rControls = rRenderer.get<VehicleTestControls>();
        auto &rScnParts = rScene.get<ACtxParts>();

        Nodes const &rNumberNodes = rScnParts.m_nodePerType[gc_ntNumber];
        PerMachType &rUsrCtrl = rScnParts.m_machines.m_perType[gc_mtUserCtrl];

        // Clear and resize Machine and Node updates
        rScnTest.m_updMachTypes.resize(rScnParts.m_machines.m_perType.size());
        auto perMachTypeIt = std::begin(rScnParts.m_machines.m_perType);
        for (UpdateMach &rUpdMach : rScnTest.m_updMachTypes)
        {
            rUpdMach.m_localDirty.reset();
            rUpdMach.m_localDirty.ints().resize(perMachTypeIt->m_localIds.vec().size());
            std::advance(perMachTypeIt, 1);
        }
        rScnTest.m_updNodeNum.m_nodeDirty.reset();
        rScnTest.m_updNodeNum.m_nodeDirty.ints().resize(rNumberNodes.m_nodeIds.vec().size());
        rScnTest.m_updNodeNum.m_nodeNewValues.resize(rNumberNodes.m_nodeIds.capacity());


        osp::input::ControlSubscriber const &ctrlSub = rControls.m_camCtrl.m_controls;

        // Select a UsrCtrl machine when pressing the switch button
        if (ctrlSub.button_triggered(rControls.m_btnSwitch))
        {
            ++rControls.m_selectedUsrCtrl;
            bool found = false;
            for (MachLocalId local = rControls.m_selectedUsrCtrl; local < rUsrCtrl.m_localIds.capacity(); ++local)
            {
                if (rUsrCtrl.m_localIds.exists(local))
                {
                    found = true;
                    rControls.m_selectedUsrCtrl = local;
                    break;
                }
            }

            if ( ! found )
            {
                rControls.m_selectedUsrCtrl = lgrn::id_null<MachLocalId>();
                OSP_LOG_INFO("Unselected vehicles");
            }
            else
            {
                OSP_LOG_INFO("Selected User Control: {}", rControls.m_selectedUsrCtrl);
            }
        }

        // Control selected UsrCtrl machine
        if (rControls.m_selectedUsrCtrl != lgrn::id_null<MachLocalId>())
        {
            float const thrRate = delta;
            float const thrChange =
                      float(ctrlSub.button_held(rControls.m_btnThrMore)) * thrRate
                    - float(ctrlSub.button_held(rControls.m_btnThrLess)) * thrRate
                    + float(ctrlSub.button_triggered(rControls.m_btnThrMax))
                    - float(ctrlSub.button_triggered(rControls.m_btnThrMin));

            MachAnyId const mach = rUsrCtrl.m_localToAny[rControls.m_selectedUsrCtrl];
            lgrn::Span<NodeId const> portSpan = rNumberNodes.m_machToNode[mach];

            if (NodeId const thrNode = connected_node(portSpan, adera::ports_userctrl::gc_throttleOut.m_port);
                thrNode != lgrn::id_null<NodeId>())
            {
                float const thrCurr = rScnTest.m_nodeNumValues[thrNode];
                float const thrNew = Magnum::Math::clamp(thrCurr + thrChange, 0.0f, 1.0f);

                if (thrCurr != thrNew)
                {
                    rScnTest.m_updNodeNum.m_nodeDirty.set(thrNode);
                    rScnTest.m_updNodeNum.m_nodeNewValues[thrNode] = thrNew;
                }
            }
        }

        // Throw a sphere when the throw button is pressed
        if (rControls.m_camCtrl.m_controls.button_held(rControls.m_btnThrow))
        {
            Matrix4 const &camTf = rScene.m_basic.m_transform.get(rRenderer.m_camera).m_transform;
            float const speed = 120;
            float const dist = 8.0f;
            rScnTest.m_toThrow.emplace_back(VehicleTestData::ThrowShape{
                /* Position */      camTf.translation() - camTf.backward() * dist,
                /* Velocity */      -camTf.backward() * speed,
                /* Size/Radius */   Vector3{1.0f},
                /* Mass */          700.0f,
                /* Shape */         EShape::Sphere});
        }

        // Move camera freely when no vehicle is selected
        if (rControls.m_selectedUsrCtrl == lgrn::id_null<MachLocalId>())
        {
            SysCameraController::update_move(
                    rControls.m_camCtrl,
                    rScene.m_basic.m_transform.get(rRenderer.m_camera),
                    delta, true);
        }

        // Update the scene directly in the drawing function :)
        update_test_scene(rScene, gc_physTimestep);

        // Follow selected UsrCtrl machine's entity
        if (rControls.m_selectedUsrCtrl != lgrn::id_null<MachLocalId>())
        {
            MachAnyId const mach        = rUsrCtrl.m_localToAny[rControls.m_selectedUsrCtrl];
            PartEnt_t const part        = rScnParts.m_machineToPart[mach];
            RigidGroup_t const rigid    = rScnParts.m_partRigids[part];
            ActiveEnt const ent         = rScnTest.m_rigidToActive[rigid];

            rControls.m_camCtrl.m_target.value() = rScene.m_basic.m_transform.get(ent).m_transform.translation();
        }

        // Rotate camera
        SysCameraController::update_view(
                rControls.m_camCtrl,
                rScene.m_basic.m_transform.get(rRenderer.m_camera), delta);

        rRenderer.update_delete(rScene.m_deleteTotal);
        rRenderer.sync(rApp, rScene);
        rRenderer.prepare_fbo(rApp);
        rRenderer.draw_entities(rApp, rScene);
        rRenderer.display(rApp);
    };
}

} // namespace testapp::VehicleTest
