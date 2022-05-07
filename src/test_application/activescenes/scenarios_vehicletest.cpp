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

#include <osp/Active/basic.h>
#include <osp/Active/drawing.h>
#include <osp/Active/physics.h>
#include <osp/Active/parts.h>

#include <osp/Active/SysHierarchy.h>
#include <osp/Active/SysPrefabInit.h>

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

using osp::active::ActiveEnt;
using osp::active::active_sparse_set_t;

using osp::active::PartEnt_t;
using osp::active::RigidGroup_t;
using osp::active::TmpPrefabInit;


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
    float m_vehicleTimer{0.0f};

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

    std::vector<TmpPrefabInit> m_initPrefabs;
    std::vector<ActiveEnt> m_initPrefabEnts;
    std::vector<PartInit> m_initParts;

    std::vector<RigidGroup_t> m_rigidDirty;
    std::vector<ActiveEnt> m_rigidEnts;

    std::optional<VehicleBuilder> m_builder;
};

osp::link::NodeTypeId const gc_number = osp::link::NodeTypeReg_t::create();
osp::link::MachTypeId const gc_usercontrol = osp::link::MachTypeReg_t::create();
osp::link::MachTypeId const gc_rocket = osp::link::MachTypeReg_t::create();

namespace ports_usrctrl
{
using osp::link::Port;

Port const smc_throttle { gc_number, 0 };
Port const smc_pitch    { gc_number, 1 };
Port const smc_yaw      { gc_number, 2 };
Port const smc_roll     { gc_number, 3 };
}

namespace ports_rocket
{
using osp::link::Port;

Port const smc_throttle { gc_number, 0 };
}


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

    auto const [ pitch, yaw, roll, throttle ] = rBuilder.create_nodes<4>(gc_number);

    auto controls = rBuilder.create_machine(capsule, gc_usercontrol, {
        { ports_usrctrl::smc_throttle,  throttle },
        { ports_usrctrl::smc_pitch,     pitch },
        { ports_usrctrl::smc_yaw,       yaw },
        { ports_usrctrl::smc_roll,      roll }
    } );

    auto rocket = rBuilder.create_machine(capsule, gc_rocket, {
        { ports_rocket::smc_throttle,  throttle }
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


static void copy_machines(osp::link::Machines const &rSrc, osp::link::Machines &rDst, Corrade::Containers::ArrayView<osp::link::MachAnyId> remapMach)
{
    using namespace osp::link;

    for (MachAnyId srcMach : rSrc.m_ids.bitview().zeros())
    {
        MachTypeId const srcType = rSrc.m_machTypes[srcMach];
        PerMachType &rDstPerMach = rDst.m_perType[srcType];

        // TODO: optimize multiple calls to IdRegistryStl::create()
        //       (linear search for non-zero uint64_t is done each call)
        MachAnyId const dstMach = rDst.m_ids.create();
        MachLocalId const dstLocal = rDstPerMach.m_localIds.create();

        // TODO: allocate once before this for loop
        rDst.m_machToLocal.resize(rDst.m_ids.capacity());
        rDst.m_machTypes.resize(rDst.m_ids.capacity());
        rDstPerMach.m_localToAny.resize(rDstPerMach.m_localIds.capacity());

        rDstPerMach.m_localToAny[dstLocal] = dstMach;
        rDst.m_machToLocal[dstMach] = dstLocal;
        remapMach[srcMach] = dstMach;
    }
}

static void copy_nodes(osp::link::Nodes const &rSrcNodes, osp::link::Machines const &rSrcMach, Corrade::Containers::ArrayView<osp::link::MachAnyId const> remapMach, osp::link::Nodes &rDstNodes, osp::link::Machines &rDstMach, Corrade::Containers::ArrayView<osp::link::NodeId> remapNode)
{
    using namespace osp::link;
    using lgrn::Span;

    // Create new node IDs
    for (NodeId srcNode : rSrcNodes.m_nodeIds.bitview().zeros())
    {
        NodeId const dstNode = rDstNodes.m_nodeIds.create();
        remapNode[srcNode] = dstNode;
    }

    // Copy node-to-machine connections
    rDstNodes.m_nodeToMach.ids_reserve(rDstNodes.m_nodeIds.capacity());
    rDstNodes.m_nodeToMach.data_reserve(rDstNodes.m_nodeToMach.data_size()
                                        + rSrcNodes.m_nodeToMach.data_size());
    for (NodeId srcNode : rSrcNodes.m_nodeIds.bitview().zeros())
    {
        NodeId const dstNode = remapNode[srcNode];
        Span<MachinePair const> srcJunction = rSrcNodes.m_nodeToMach[srcNode];
        rDstNodes.m_nodeToMach.emplace(dstNode, srcJunction.size());
        Span<MachinePair> dstJuncton = rDstNodes.m_nodeToMach[dstNode];

        auto dstPairIt = std::begin(dstJuncton);
        for (MachinePair const& srcPair : srcJunction)
        {
            MachTypeId const machType = srcPair.m_type;
            MachAnyId const srcMach = rSrcMach.m_perType[machType].m_localToAny[srcPair.m_id];
            MachAnyId const dstMach = remapMach[srcMach];
            MachLocalId const dstLocal = rDstMach.m_machToLocal[dstMach];

            dstPairIt->m_id     = dstLocal;
            dstPairIt->m_type   = machType;

            std::advance(dstPairIt, 1);
        }
    }

    // copy mach-to-node connections
    rDstNodes.m_machToNode.ids_reserve(rDstMach.m_ids.capacity());
    rDstNodes.m_machToNode.data_reserve(rDstNodes.m_machToNode.data_size()
                                        + rSrcNodes.m_machToNode.data_size());
    for (MachAnyId srcMach : rSrcMach.m_ids.bitview().zeros())
    {
        if (rSrcNodes.m_machToNode.contains(srcMach))
        {
            Span<NodeId const> srcPorts = rSrcNodes.m_machToNode[srcMach];
            MachAnyId const dstMach = remapMach[srcMach];
            rDstNodes.m_machToNode.emplace(dstMach, srcPorts.size());
            Span<NodeId> dstPorts = rDstNodes.m_machToNode[dstMach];

            auto dstPortIt = std::begin(dstPorts);
            for (NodeId const srcNode : srcPorts)
            {
                *dstPortIt = remapNode[srcNode];

                std::advance(dstPortIt, 1);
            }
        }
    }
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

    rScnTest.m_initPrefabs.clear();
    rScnTest.m_initParts.clear();
    rScnTest.m_rigidDirty.clear();

    using osp::restypes::gc_importer;

    static Matrix4 const identity{};

    rScnTest.m_vehicleTimer += delta;
    if (rScnTest.m_vehicleTimer >= 0.5f)
    {
        rScnTest.m_vehicleTimer -= 0.5f;

        VehicleData const &data = rScnTest.m_builder->data();

        ActiveEnt const rigidEnt = rScene.m_activeIds.create();
        RigidGroup_t const rigid = rScnParts.m_rigidIds.create();

        rScnTest.m_rigidEnts.resize(rScnParts.m_rigidIds.capacity());
        rScnTest.m_rigidDirty.push_back(rigid);
        rScnParts.m_rigidParts.ids_reserve(rScnParts.m_rigidIds.capacity());
        rScnParts.m_rigidParts.data_reserve(rScnParts.m_rigidParts.data_capacity() + data.m_partIds.size());

        // setup root entity
        SysHierarchy::add_child(rScene.m_basic.m_hierarchy, rScene.m_hierRoot, rigidEnt);
        rScene.m_basic.m_transform.emplace(rigidEnt, Matrix4::translation({0.0f, 5.0f, 0.0f}));
        rScnPhys.m_physics.m_hasColliders.emplace(rigidEnt);
        rScnPhys.m_physics.m_solid.emplace(rigidEnt);
        rScnPhys.m_physics.m_physBody.emplace(rigidEnt);
        rScnPhys.m_physics.m_physLinearVel.emplace(rigidEnt);
        rScnPhys.m_physics.m_physAngularVel.emplace(rigidEnt);
        osp::active::ACompPhysDynamic &rDyn = rScnPhys.m_physics.m_physDynamic.emplace(rigidEnt);
        rDyn.m_totalMass = 1.0f; // TODO
        rDyn.m_inertia = {1.0f, 1.0f, 1.0f};

        // Make gravity affect entity
        rScnTest.m_hasGravity.emplace(rigidEnt);
        //rScnTest.m_removeOutOfBounds.emplace(rigidEnt);
        rScnTest.m_rigidEnts[rigid] = rigidEnt;

        PartEnt_t *pRigidPart = rScnParts.m_rigidParts.emplace(rigid, data.m_partIds.size());

        for (unsigned int i = 0; i < data.m_partIds.size(); ++i)
        {
            if ( ! data.m_partIds.exists(PartId(i)) )
            {
                continue;
            }

            PartEnt_t const part = rScnParts.m_partIds.create();

            rScnParts.m_partPrefabs.resize(rScnParts.m_partIds.capacity());
            rScnParts.m_partRigids.resize(rScnParts.m_partIds.capacity());
            rScnParts.m_partTransformRigid.resize(rScnParts.m_partIds.capacity());

            osp::PrefabPair const& prefabPair = data.m_partPrefabs[i];

            rScnParts.m_partPrefabs[part] = osp::PrefabPair{rScene.m_pResources->owner_create(gc_importer, prefabPair.m_importer), prefabPair.m_prefabId};
            rScnParts.m_partRigids[part] = rigid;
            rScnParts.m_partTransformRigid[part] = data.m_partTransforms[i];

            *pRigidPart = part;
            ++pRigidPart;

            rScnTest.m_initParts.emplace_back(PartInit{part, int(rScnTest.m_initPrefabs.size())});
            TmpPrefabInit &rInitPf = rScnTest.m_initPrefabs.emplace_back();
            rInitPf.m_importerRes = prefabPair.m_importer;
            rInitPf.m_prefabId = prefabPair.m_prefabId;
            rInitPf.m_pTransform = &data.m_partTransforms[i];
            rInitPf.m_parent = rigidEnt;
        }

        using namespace osp::link;

        // remapMach[source MachAnyId] -> destination MachAnyId
        std::vector<MachAnyId> remapMach(data.m_machines.m_ids.capacity(), lgrn::id_null<MachAnyId>());

        // Make new machines in dst for each machine in source
        copy_machines(data.m_machines, rScnParts.m_machines, remapMach);

        // Do the same for nodes
        std::vector<NodeId> remapNodes(data.m_nodePerType[gc_number].m_nodeIds.capacity(), lgrn::id_null<NodeId>());
        copy_nodes(data.m_nodePerType[gc_number], data.m_machines, remapMach, rScnParts.m_nodePerType[gc_number], rScnParts.m_machines, remapNodes);


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

    // Set Part entities, now that the prefabs exist
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
