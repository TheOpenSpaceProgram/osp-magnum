/**
 * Open Space Program
 * Copyright © 2019-2024 Open Space Program Project
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
#include "jolt.h"
#include "shapes.h"

#include "../feature_interfaces.h"

#include <osp/activescene/basic_fn.h>
#include <osp/activescene/physics_fn.h>
#include <osp/activescene/prefab_fn.h>
#include <osp/activescene/vehicles.h>
#include <osp/core/Resources.h>
#include <osp/drawing/drawing.h>
#include <osp/vehicles/ImporterData.h>

#include <adera/links.h>

#include <planet-a/activescene/terrain.h>

#include <ospjolt/activescene/joltinteg_fn.h>

#include <Jolt/Physics/Collision/Shape/TriangleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>

using namespace ftr_inter::stages;
using namespace ftr_inter;
using namespace osp::active;
using namespace osp::fw;
using namespace osp::link;
using namespace osp;
using namespace ospjolt;


using ospjolt::ACtxJoltWorld;

using Corrade::Containers::arrayView;
using osp::restypes::gc_importer;

namespace adera
{


FeatureDef const ftrJolt = feature_def("Jolt", [] (
        FeatureBuilder              &rFB,
        Implement<FIJolt>           jolt,
        DependOn<FIMainApp>         mainApp,
        DependOn<FIScene>           scn,
        DependOn<FICommonScene>     comScn,
        DependOn<FIPhysics>         phys)
{
    using ospjolt::SysJolt;

    //Mandatory Jolt setup steps (start of program)
    JoltGlobalInit::init_if_required();
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)

    rFB.pipeline(jolt.pl.joltBody).parent(mainApp.loopblks.mainLoop);

    auto &rJolt = rFB.data_emplace< ACtxJoltWorld >(jolt.di.jolt);
    setup_jolt_world(rJolt);

    rFB.task()
        .name       ("Delete Jolt components")
        .sync_with  ({comScn.pl.activeEntDelete(UseOrRun), jolt.pl.joltBody(Delete)})
        .args({                jolt.di.jolt,                      comScn.di.activeEntDel })
        .func([] (ACtxJoltWorld& rJolt, ActiveEntVec_t const& rActiveEntDel) noexcept
    {
        SysJolt::update_delete (rJolt, rActiveEntDel.cbegin(), rActiveEntDel.cend());
    });

    rFB.task()
        .name       ("Update Jolt world")
        .sync_with  ({jolt.pl.joltBody(ReadyB4New), comScn.pl.translateOrigin(UseOrRun), phys.pl.mass(ReadyB4New), phys.pl.physUpdate(Run), comScn.pl.transform(Modify)})
        .args({             comScn.di.basic,             phys.di.phys,              jolt.di.jolt,           scn.di.deltaTimeIn })
        .func([] (ACtxBasic& rBasic, ACtxPhysics& rPhys, ACtxJoltWorld& rJolt, float const deltaTimeIn) noexcept
    {
        SysJolt::update_world(rBasic, rPhys, rJolt, deltaTimeIn);
    });
}); // ftrJolt



FeatureDef const ftrJoltConstAccel = feature_def("JoltAccel", [] (
        FeatureBuilder              &rFB,
        Implement<FIJoltConstAccel> joltConstAccel)
{
    rFB.data_emplace<ACtxConstAccel>(joltConstAccel.di.accel);
}); // ftrJoltAccel

ForceFactors_t add_constant_acceleration(
        osp::Vector3                forceVec,
        osp::fw::Framework          &rFW,
        osp::fw::ContextId          sceneCtx)
{
    auto const jolt         = rFW.get_interface<FIJolt>(sceneCtx);
    auto const constAccel   = rFW.get_interface<FIJoltConstAccel>(sceneCtx);
    auto       &rJolt       = rFW.data_get<ACtxJoltWorld>(jolt.di.jolt);
    auto       &rAccel      = rFW.data_get<ACtxConstAccel>(constAccel.di.accel);

    ACtxJoltWorld::ForceFactorFunc factor
    {
        .m_func = [] (BodyId const bodyId, ACtxJoltWorld const &rJolt, entt::any userData, Vector3& rForce, Vector3& rTorque) noexcept
        {
            Vector3 const force = entt::any_cast<Vector3>(userData);

            float inv_mass = SysJolt::get_inverse_mass_no_lock(rJolt.m_physicsSystem, bodyId);

            rForce += force / inv_mass;
        },
        .m_userData = entt::make_any<Vector3>(forceVec)
    };

    // Register force

    auto const index = rJolt.m_factors.size();
    rJolt.m_factors.emplace_back(factor);

    rAccel.forces.push_back({
        .vec         = forceVec,
        .factorIndex = static_cast<std::uint8_t>(index)
    });

    ForceFactors_t out;
    out.set(index);
    return out;
}

FeatureDef const ftrPhysicsShapesJolt = feature_def("PhysicsShapesJolt", [] (
        FeatureBuilder              &rFB,
        Implement<FIPhysShapesJolt> physShapesJolt,
        DependOn<FICommonScene>     comScn,
        DependOn<FIPhysics>         phys,
        DependOn<FIPhysShapes>      physShapes,
        DependOn<FIJolt>            jolt)
{
    rFB.data_emplace<ForceFactors_t>(physShapesJolt.di.factors);

    rFB.task()
        .name       ("Add Jolt physics to spawned shapes")
        .sync_with  ({physShapes.pl.spawnRequest(UseOrRun), physShapes.pl.spawnedEnts(UseOrRun), jolt.pl.joltBody(New), phys.pl.physUpdate(Done)})
        .args       ({           comScn.di.basic,    physShapes.di.physShapes,       phys.di.phys,         jolt.di.jolt,    physShapesJolt.di.factors})
        .func       ([] (ACtxBasic const &rBasic, ACtxPhysShapes& rPhysShapes, ACtxPhysics& rPhys, ACtxJoltWorld &rJolt, ForceFactors_t const factors) noexcept
    {
        JPH::BodyInterface &bodyInterface = rJolt.m_physicsSystem.GetBodyInterface();

        int numBodies = static_cast<int>(rPhysShapes.m_spawnRequest.size());

        if (numBodies == 0) { return; }

        std::vector<JPH::BodyID> addedBodies;
        addedBodies.reserve(numBodies);
        
        for (std::size_t i = 0; i < numBodies; ++i)
        {
            SpawnShape const &spawn = rPhysShapes.m_spawnRequest[i];
            ActiveEnt const root    = rPhysShapes.m_ents[i * 2];
            ActiveEnt const child   = rPhysShapes.m_ents[i * 2 + 1];

            JPH::Ref<JPH::Shape> pShape = SysJolt::create_primitive(rJolt, spawn.m_shape, Vec3MagnumToJolt(spawn.m_size));
            
            BodyId const bodyId = rJolt.m_bodyIds.create();

            JPH::BodyCreationSettings bodyCreation(pShape,
                                            Vec3MagnumToJolt(spawn.m_position), 
                                            JPH::Quat::sIdentity(),
                                            JPH::EMotionType::Dynamic,
                                            Layers::MOVING);
            bodyCreation.mMaxLinearVelocity = 65536.0f;

            if (spawn.m_mass > 0.0f) 
            { 
                JPH::MassProperties massProp;
                Vector3 const inertia = collider_inertia_tensor(spawn.m_shape, spawn.m_size, spawn.m_mass);
                massProp.mMass = spawn.m_mass; 
                massProp.mInertia = JPH::Mat44::sScale(Vec3MagnumToJolt(inertia));
                bodyCreation.mMassPropertiesOverride = massProp;
                bodyCreation.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
            }
            else
            {   
                bodyCreation.mMotionType = JPH::EMotionType::Static;
                bodyCreation.mObjectLayer = Layers::MOVING;
            }
            //TODO helper function ? 
            
            JPH::BodyID joltBodyId = BToJolt(bodyId);
            bodyInterface.CreateBodyWithID(joltBodyId, bodyCreation);
            addedBodies.push_back(joltBodyId);

            rJolt.m_bodyUseOriginTranslate.resize(rJolt.m_bodyIds.capacity());
            rJolt.m_bodyUseOriginTranslate.emplace(bodyId);

            rJolt.m_bodyToEnt[bodyId]    = root;
            rJolt.m_bodyFactors[bodyId]  = factors;
            rJolt.m_entToBody.emplace(root, bodyId);

        }
        //Bodies are added all at once for performance reasons.
        JPH::BodyInterface::AddState addState = bodyInterface.AddBodiesPrepare(addedBodies.data(), numBodies);
        bodyInterface.AddBodiesFinalize(addedBodies.data(), numBodies, addState, JPH::EActivation::Activate);
    });

}); // ftrPhysicsShapesJolt

void set_phys_shape_factors(
        ForceFactors_t              factors,
        osp::fw::Framework          &rFW,
        osp::fw::ContextId          sceneCtx)
{
    rFW.data_get<ospjolt::ForceFactors_t&>(
    rFW.get_interface<FIPhysShapesJolt>(sceneCtx).di.factors) = factors;
}



void compound_collect_recurse(
        ACtxPhysics const&      rCtxPhys,
        ACtxJoltWorld&          rCtxWorld,
        ACtxBasic const&        rBasic,
        ActiveEnt               ent,
        Matrix4 const&          transform,
        JPH::CompoundShapeSettings&  rCompound)
{
    EShape const shape = rCtxPhys.m_shape[ent];

    if (shape != EShape::None)
    {
        bool entExists = rCtxWorld.m_shapes.contains(ent);
        JPH::Ref<JPH::Shape> rShape = entExists
                               ? rCtxWorld.m_shapes.get(ent)
                               : rCtxWorld.m_shapes.emplace(ent);

        if (!entExists)
        {
            rShape = SysJolt::create_primitive(rCtxWorld, shape, Vec3MagnumToJolt(transform.scaling()));
        }
        else 
        {
            SysJolt::scale_shape(rShape, Vec3MagnumToJolt(transform.scaling()));
        }

        rCompound.AddShape( Vec3MagnumToJolt(transform.translation()), 
                            QuatMagnumToJolt(osp::Quaternion::fromMatrix(transform.rotation())), 
                            rShape);
    }

    if ( ! rCtxPhys.m_hasColliders.contains(ent) )
    {
        return;
    }

    // Recurse into children if there are more colliders
    for (ActiveEnt child : SysSceneGraph::children(rBasic.m_scnGraph, ent))
    {
        if (rBasic.m_transform.contains(child))
        {
            ACompTransform const &rChildTransform = rBasic.m_transform.get(child);

            Matrix4 const childMatrix = transform * rChildTransform.m_transform;

            compound_collect_recurse(
                    rCtxPhys, rCtxWorld, rBasic, child, childMatrix, rCompound);
        }
    }
} // void compound_collect_recurse


FeatureDef const ftrVehicleSpawnJolt = feature_def("VehicleSpawnJolt", [] (
        FeatureBuilder              &rFB,
        Implement<FIVhclSpawnJolt>  vhclSpawnJolt,
        DependOn<FIMainApp>         mainApp,
        DependOn<FICommonScene>     comScn,
        DependOn<FIPhysics>         phys,
        DependOn<FIPhysShapes>      physShapes,
        DependOn<FIPrefabs>         prefabs,
        DependOn<FIParts>           parts,
        DependOn<FIJolt>            jolt,
        DependOn<FIVehicleSpawn>    vhclSpawn)
{
    rFB.data_emplace<ForceFactors_t>(vhclSpawnJolt.di.factors);

    rFB.task()
        .name       ("Create root ActiveEnts for each Weld")
        .sync_with  ({vhclSpawn.pl.spawnRequest(UseOrRun), comScn.pl.activeEnt(New), parts.pl.mapWeldActive(New), vhclSpawn.pl.rootEnts(Resize)})
        .args       ({      comScn.di.basic,                  vhclSpawn.di.vehicleSpawn,           parts.di.scnParts})
        .func       ([] (ACtxBasic& rBasic, ACtxVehicleSpawn& rVehicleSpawn, ACtxParts& rScnParts) noexcept
    {
        LGRN_ASSERT(rVehicleSpawn.new_vehicle_count() != 0);

        rVehicleSpawn.rootEnts.resize(rVehicleSpawn.spawnedWelds.size());
        rBasic.m_activeIds.create(rVehicleSpawn.rootEnts.begin(), rVehicleSpawn.rootEnts.end());

        // update WeldId->ActiveEnt mapping
        auto itWeldEnt = rVehicleSpawn.rootEnts.begin();
        for (WeldId const weld : rVehicleSpawn.spawnedWelds)
        {
            rScnParts.weldToActive[weld] = *itWeldEnt;
            ++itWeldEnt;
        }
    });


    rFB.task()
        .name       ("Add vehicle entities to Scene Graph")
        .sync_with  ({vhclSpawn.pl.spawnRequest(UseOrRun), vhclSpawn.pl.rootEnts(UseOrRun),
                      parts.pl.mapWeldActive(Ready),
                      prefabs.pl.spawnedEnts(UseOrRun), prefabs.pl.spawnRequest(UseOrRun), prefabs.pl.inSubtree(Run),
                      comScn.pl.transform(New), comScn.pl.hierarchy(New)})
        .args       ({      comScn.di.basic,                        vhclSpawn.di.vehicleSpawn,           parts.di.scnParts,             prefabs.di.prefabs,            mainApp.di.resources})
        .func       ([] (ACtxBasic& rBasic, ACtxVehicleSpawn const& rVehicleSpawn, ACtxParts& rScnParts, ACtxPrefabs& rPrefabs, Resources& rResources) noexcept
    {
        LGRN_ASSERT(rVehicleSpawn.new_vehicle_count() != 0);

        // ActiveEnts created for welds + ActiveEnts created for vehicle prefabs
        std::size_t const totalEnts = rVehicleSpawn.rootEnts.size() + rPrefabs.newEnts.size();

        auto const& itWeldsFirst        = rVehicleSpawn.spawnedWelds.begin();
        auto const& itWeldOffsetsLast   = rVehicleSpawn.spawnedWeldOffsets.end();
        auto itWeldOffsets              = rVehicleSpawn.spawnedWeldOffsets.begin();

        rBasic.m_scnGraph.resize(rBasic.m_activeIds.capacity());

        for (ACtxVehicleSpawn::TmpToInit const& toInit : rVehicleSpawn.spawnRequest)
        {
            auto const itWeldOffsetsNext = std::next(itWeldOffsets);
            SpWeldId const weldOffsetNext = (itWeldOffsetsNext != itWeldOffsetsLast)
                                          ? (*itWeldOffsetsNext)
                                          : SpWeldId(uint32_t(rVehicleSpawn.spawnedWelds.size()));

            std::for_each(itWeldsFirst + std::ptrdiff_t{*itWeldOffsets},
                          itWeldsFirst + std::ptrdiff_t{weldOffsetNext},
                          [&rBasic, &rScnParts, &rVehicleSpawn, &rPrefabs, &rResources, &toInit] (WeldId const weld)
            {
                // Count parts in this weld first
                std::size_t entCount = 0;
                for (PartId const part : rScnParts.weldToParts[weld.value])
                {
                    SpPartId const newPart = rVehicleSpawn.partToSpawned[part];
                    uint32_t const prefabInit = rVehicleSpawn.spawnedPrefabs[newPart];
                    entCount += rPrefabs.spawnedEntsOffset[prefabInit].size();
                }

                ActiveEnt const weldEnt = rScnParts.weldToActive[weld];

                rBasic.m_transform.emplace(weldEnt, Matrix4::from(toInit.rotation.toMatrix(), toInit.position));

                SubtreeBuilder bldRoot = SysSceneGraph::add_descendants(rBasic.m_scnGraph, entCount + 1);
                SubtreeBuilder bldWeld = bldRoot.add_child(weldEnt, entCount);

                for (PartId const part : rScnParts.weldToParts[weld.value])
                {
                    SpPartId const newPart      = rVehicleSpawn.partToSpawned[part];
                    uint32_t const prefabInit   = rVehicleSpawn.spawnedPrefabs[newPart];
                    auto const& basic           = rPrefabs.spawnRequest[prefabInit];
                    auto const& ents            = rPrefabs.spawnedEntsOffset[prefabInit];

                    SysPrefabInit::add_to_subtree(basic, ents, rResources, bldWeld);
                }
            });

            itWeldOffsets = itWeldOffsetsNext;
        }
    });


    rFB.task()
        .name       ("Add Jolt physics to Weld entities")
        .sync_with  ({vhclSpawn.pl.spawnRequest(UseOrRun), vhclSpawn.pl.rootEnts(UseOrRun), prefabs.pl.spawnedEnts(UseOrRun), comScn.pl.transform(Ready), phys.pl.mass(Ready), jolt.pl.joltBody(New), comScn.pl.hierarchy(Ready)})
        .args       ({     comScn.di.basic,       phys.di.phys,         jolt.di.jolt,             vhclSpawn.di.vehicleSpawn,          parts.di.scnParts,     vhclSpawnJolt.di.factors})
        .func       ([] (ACtxBasic const& rBasic, ACtxPhysics& rPhys, ACtxJoltWorld& rJolt, ACtxVehicleSpawn const& rVehicleSpawn, ACtxParts const& rScnParts, ForceFactors_t const factors) noexcept
    {
        LGRN_ASSERT(rVehicleSpawn.new_vehicle_count() != 0);

        rPhys.m_hasColliders.resize(rBasic.m_activeIds.capacity());

        auto const& itWeldsFirst        = std::begin(rVehicleSpawn.spawnedWelds);
        auto const& itWeldOffsetsLast   = std::end(rVehicleSpawn.spawnedWeldOffsets);
        auto itWeldOffsets              = std::begin(rVehicleSpawn.spawnedWeldOffsets);

        JPH::BodyInterface &bodyInterface = rJolt.m_physicsSystem.GetBodyInterface();

        std::vector<JPH::BodyID> addedBodies;

        for (ACtxVehicleSpawn::TmpToInit const& toInit : rVehicleSpawn.spawnRequest)
        {
            auto const itWeldOffsetsNext = std::next(itWeldOffsets);
            SpWeldId const weldOffsetNext = (itWeldOffsetsNext != itWeldOffsetsLast)
                                           ? (*itWeldOffsetsNext)
                                           : SpWeldId(uint32_t(rVehicleSpawn.spawnedWelds.size()));

            std::for_each(itWeldsFirst + std::ptrdiff_t{*itWeldOffsets},
                          itWeldsFirst + std::ptrdiff_t{weldOffsetNext},
                          [&rBasic, &rScnParts, &rVehicleSpawn, &toInit, &rPhys, &rJolt, &addedBodies, factors] (WeldId const weld)
            {
                ActiveEnt const weldEnt = rScnParts.weldToActive[weld];

                JPH::MutableCompoundShapeSettings compound;

                rPhys.m_hasColliders.insert(weldEnt);

                // Collect all colliders from hierarchy.
                compound_collect_recurse( rPhys, rJolt, rBasic, weldEnt, Matrix4{}, compound );

                JPH::Ref<JPH::Shape> compoundShape = compound.Create().Get();
                JPH::BodyCreationSettings bodyCreation(compoundShape, JPH::Vec3Arg::sZero(), JPH::Quat::sZero(), JPH::EMotionType::Dynamic, Layers::MOVING);
                bodyCreation.mMaxLinearVelocity = 65536.0f;

                BodyId const bodyId = rJolt.m_bodyIds.create();

                rJolt.m_bodyUseOriginTranslate.resize(rJolt.m_bodyIds.capacity());
                rJolt.m_bodyUseOriginTranslate.emplace(bodyId);

                rJolt.m_bodyToEnt[bodyId] = weldEnt;
                rJolt.m_bodyFactors[bodyId] = factors;
                rJolt.m_entToBody.emplace(weldEnt, bodyId);

                float   totalMass = 0.0f;
                Vector3 massPos{0.0f};
                SysPhysics::calculate_subtree_mass_center(rBasic.m_transform, rPhys, rBasic.m_scnGraph, weldEnt, massPos, totalMass);

                Vector3 const com = massPos / totalMass;
                auto const comToOrigin = Matrix4::translation( - com );

                Matrix3 inertiaTensor{0.0f};
                SysPhysics::calculate_subtree_mass_inertia(rBasic.m_transform, rPhys, rBasic.m_scnGraph, weldEnt, inertiaTensor, comToOrigin);

                Matrix4 const inertiaTensorMat4{inertiaTensor};

                JPH::MassProperties massProp;
                massProp.mMass = totalMass;
                massProp.mInertia = JPH::Mat44::sLoadFloat4x4((JPH::Float4*) inertiaTensorMat4.data());

                bodyCreation.mMassPropertiesOverride = massProp;
                bodyCreation.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;

                bodyCreation.mLinearDamping = 0.0f;

                // TODO: temporary, suppose to be zero. This makes it easier to steer
                bodyCreation.mAngularDamping = 0.8f;

                bodyCreation.mPosition = Vec3MagnumToJolt(toInit.position);

                auto rawQuat = toInit.rotation.data();
                JPH::Quat joltRotation(rawQuat[0], rawQuat[1], rawQuat[2], rawQuat[3]);
    
                bodyCreation.mRotation = joltRotation;

                JPH::BodyInterface &bodyInterface = rJolt.m_physicsSystem.GetBodyInterface();
                JPH::BodyID joltBodyId = BToJolt(bodyId);
                bodyInterface.CreateBodyWithID(joltBodyId, bodyCreation);
                addedBodies.push_back(joltBodyId);
                rPhys.m_setVelocity.emplace_back(weldEnt, toInit.velocity);
            });

            itWeldOffsets = itWeldOffsetsNext;
        }
        //Bodies are added all at once for performance reasons.
        int numBodies = static_cast<int>(addedBodies.size());
        JPH::BodyInterface::AddState addState = bodyInterface.AddBodiesPrepare(addedBodies.data(), numBodies);
        bodyInterface.AddBodiesFinalize(addedBodies.data(), numBodies, addState, JPH::EActivation::Activate);
    });
}); // ftrVehicleSpawnJolt


void set_vehicle_default_factors(
        ForceFactors_t              factors,
        osp::fw::Framework          &rFW,
        osp::fw::ContextId          sceneCtx)
{
    rFW.data_get<ospjolt::ForceFactors_t&>(
    rFW.get_interface<FIVhclSpawnJolt>(sceneCtx).di.factors) = factors;
}

struct BodyRocket
{
    Quaternion      m_rotation;
    Vector3         m_offset;

    MachLocalId     m_local         {lgrn::id_null<MachLocalId>()};
    NodeId          m_throttleIn    {lgrn::id_null<NodeId>()};
    NodeId          m_multiplierIn  {lgrn::id_null<NodeId>()};
};

struct ACtxRocketsJolt
{
    // map each bodyId to a {machine, offset}
    //TODO: make an IdMultiMap or something.
    lgrn::IntArrayMultiMap<BodyId::entity_type, BodyRocket> m_bodyRockets;
    std::uint8_t factorIndex;
};

/**
 * @brief Search for Rockets in a newly added vehicle rigid body (a Weld), calculate offset and
 *        rotation, then assign the right force factors to them.
 */
static void assign_weld_rockets(
        WeldId                   const weld,
        ACtxBasic                const &rBasic,
        ACtxParts                const &rScnParts,
        ACtxJoltWorld                  &rJolt,
        ACtxRocketsJolt                &rRocketsJolt,
        NodeType                 const &rFloatNodes,
        MachType                 const &machtypeRocket,
        std::vector<BodyRocket>        &rRocketsFoundTemp)
{
    using adera::gc_mtMagicRocket;
    using adera::ports_magicrocket::gc_throttleIn;
    using adera::ports_magicrocket::gc_multiplierIn;

    rRocketsFoundTemp.clear();

    ActiveEnt const weldEnt = rScnParts.weldToActive[weld];
    BodyId const body    = rJolt.m_entToBody.at(weldEnt);

    if (rRocketsJolt.m_bodyRockets.contains(body.value))
    {
        rRocketsJolt.m_bodyRockets.erase(body.value);
    }

    // Each weld consists of multiple parts, iterate them all. Note that each part has their own
    // individual transforms, so math is needed to calculate stuff with thrust direction and
    // center-of-mass.
    for (PartId const part : rScnParts.weldToParts[weld.value])
    {
        auto const sizeBefore = rRocketsFoundTemp.size();

        // Each part contains Machines, some of which may be rockets.
        for (MachinePair const pair : rScnParts.partToMachines[part.value])
        {
            if (pair.type != gc_mtMagicRocket)
            {
                continue; // This machine is not a rocket
            }

            MachAnyId const  mach         = machtypeRocket.machanyIdOf[pair.local];
            auto      const& portSpan     = rFloatNodes.machToNode[mach.value];
            NodeId    const  throttleIn   = connected_node(portSpan, gc_throttleIn.port);
            NodeId    const  multiplierIn = connected_node(portSpan, gc_multiplierIn.port);

            if (   (throttleIn   == lgrn::id_null<NodeId>())
                || (multiplierIn == lgrn::id_null<NodeId>()) )
            {
                continue; // Throttle and/or multiplier is not connected
            }

            BodyRocket &rBodyRocket     = rRocketsFoundTemp.emplace_back();
            rBodyRocket.m_local         = pair.local;
            rBodyRocket.m_throttleIn    = throttleIn;
            rBodyRocket.m_multiplierIn  = multiplierIn;
        }

        auto const rocketsFoundForPart = arrayView(rRocketsFoundTemp).exceptPrefix(sizeBefore);

        if (rocketsFoundForPart.isEmpty())
        {
            continue; // No rockets found on this part
        }

        // calculate transform relative to body root
        // start from part, then walk parents up
        ActiveEnt const partEnt = rScnParts.partToActive[part];

        Matrix4     transform   = rBasic.m_transform.get(partEnt).m_transform;
        ActiveEnt   parent      = rBasic.m_scnGraph.m_entParent[partEnt];

        while (parent != weldEnt)
        {
            Matrix4 const& parentTransform = rBasic.m_transform.get(parent).m_transform;
            transform = parentTransform * transform;
            parent = rBasic.m_scnGraph.m_entParent[parent];
        }

        auto const      rotation    = Quaternion::fromMatrix(transform.rotation());
        Vector3 const   offset      = transform.translation();

        for (BodyRocket &rBodyRocket : rocketsFoundForPart)
        {
            rBodyRocket.m_rotation  = rotation;
            rBodyRocket.m_offset    = offset;
        }
    }

    ForceFactors_t &rBodyFactors = rJolt.m_bodyFactors[body];

    if ( rRocketsFoundTemp.empty() )
    {
        rBodyFactors.reset(rRocketsJolt.factorIndex);
    }
    else
    {
        rBodyFactors.set(rRocketsJolt.factorIndex);
        rRocketsJolt.m_bodyRockets.emplace(body.value, rRocketsFoundTemp.begin(), rRocketsFoundTemp.end());
    }
}

#if 0

struct RocketThrustUserData
{
    ACtxRocketsJolt       const &rRocketsJolt;
    Machines              const &rMachines;
    SignalValues_t<float> const &rSigValFloat;
};

// ACtxjoltWorld::ForceFactorFunc::Func_t
static void rocket_thrust_force(BodyId const bodyId, ACtxJoltWorld const& rJolt, entt::any userData, Vector3& rForce, Vector3& rTorque) noexcept
{
    auto const [rRocketsJolt, rMachines, rSigValFloat] = entt::any_cast<RocketThrustUserData>(userData);
    auto &rBodyRockets = rRocketsJolt.m_bodyRockets[bodyId.value];

    //no lock as all bodies are locked in callbacks
    JPH::BodyInterface const &bodyInterface = rJolt.m_physicsSystem.GetBodyInterfaceNoLock();

    if (rBodyRockets.empty())
    {
        return;
    }

    JPH::BodyID joltBodyId = BToJolt(bodyId);
    Quaternion const rot = QuatJoltToMagnum(bodyInterface.GetRotation(joltBodyId));

    JPH::Shape const* shape = bodyInterface.GetShape(joltBodyId).GetPtr();
    if (shape == nullptr)
    {
        return;
    }
    Vector3 com = Vec3JoltToMagnum(shape->GetCenterOfMass());

    for (BodyRocket const& bodyRocket : rBodyRockets)
    {
        float const throttle = std::clamp(rSigValFloat[bodyRocket.m_throttleIn], 0.0f, 1.0f);
        float const multiplier = rSigValFloat[bodyRocket.m_multiplierIn];

        float const thrustMag = throttle * multiplier;

        if (thrustMag == 0.0f)
        {
            continue;
        }

        Vector3 const offsetRel = rot.transformVector(bodyRocket.m_offset - com);

        Vector3 const direction = (rot * bodyRocket.m_rotation).transformVector(adera::gc_rocketForward);

        Vector3 const thrustForce = direction * thrustMag;
        Vector3 const thrustTorque = Magnum::Math::cross(offsetRel, thrustForce);
        rForce += thrustForce;
        rTorque += thrustTorque;
    }
}

FeatureDef const ftrRocketThrustJolt = feature_def("RocketThrustJolt", [] (
        FeatureBuilder              &rFB,
        Implement<FIRocketsJolt>    rktJolt,
        DependOn<FIMainApp>         mainApp,
        DependOn<FIScene>           scn,
        DependOn<FICommonScene>     comScn,
        DependOn<FIPhysics>         phys,
        DependOn<FIPrefabs>         prefabs,
        DependOn<FIParts>           parts,
        DependOn<FILinks>           links,
        DependOn<FISignalsFloat>    sigFloat,
        DependOn<FIJolt>            jolt,
        //DependOn<FIJoltFactors>     joltFactors,
        DependOn<FIVehicleSpawn>    vhclSpawn)
{
    auto &rRocketsJolt = rFB.data_emplace< ACtxRocketsJolt >(rktJolt.di.rocketsJolt);

    rFB.task()
        .name       ("Assign rockets to Jolt bodies")
        .sync_with  ({parts.pl.weldIds(Ready), jolt.pl.joltBody(Ready), links.pl.connect(Ready)})
        .args       ({     comScn.di.basic,       phys.di.phys,         jolt.di.jolt,          parts.di.scnParts,          links.di.links,        rktJolt.di.rocketsJolt})
        .func       ([] (ACtxBasic& rBasic, ACtxPhysics& rPhys, ACtxJoltWorld& rJolt, ACtxParts const& rScnParts, ACtxLinks const& rLinks, ACtxRocketsJolt& rRocketsJolt) noexcept
    {
        using adera::gc_mtMagicRocket;

        NodeType        const &rFloatNodes    = rLinks.nodePerType[gc_ntSigFloat];
        MachType        const &machtypeRocket = rLinks.machines.perType[gc_mtMagicRocket];

        rRocketsJolt.m_bodyRockets.ids_reserve(rJolt.m_bodyIds.size());
        rRocketsJolt.m_bodyRockets.data_reserve(rLinks.machines.perType[gc_mtMagicRocket].localIds.capacity());

        std::vector<BodyRocket> temp;

        for (WeldId const weld : rScnParts.weldDirty)
        {
            assign_weld_rockets(weld, rBasic, rScnParts, rJolt, rRocketsJolt, rFloatNodes, machtypeRocket, temp);
        }
    });

    auto &rScnParts     = rFB.data_get< ACtxParts >              (parts.di.scnParts);
    auto &rLinks        = rFB.data_get< ACtxLinks >              (links.di.links);
    auto &rSigValFloat  = rFB.data_get< SignalValues_t<float> >  (sigFloat.di.sigValFloat);
    Machines &rMachines = rLinks.machines;

    ACtxJoltWorld::ForceFactorFunc const factor
    {
        .m_func     = &rocket_thrust_force,
        .m_userData = RocketThrustUserData{ rRocketsJolt, rMachines, rSigValFloat }
    };

    auto &rJolt = rFB.data_get<ACtxJoltWorld>(jolt.di.jolt);

    auto const index = rJolt.m_factors.size();
    rJolt.m_factors.emplace_back(factor);

    rRocketsJolt.factorIndex = static_cast<std::uint8_t>(index);
}); // ftrRocketThrustJolt

#endif


struct ACtxTerrainJolt
{
    BodyId bodyId;
    JPH::Ref<JPH::MutableCompoundShape> shape;

    Vector3l originPos;

    // useful for when translating everything for MutableCompountShape::ModifyShapes
    std::vector<JPH::Vec3> positions;
};


FeatureDef const ftrTerrainJolt = feature_def("ftrTerrainJolt", [] (
        FeatureBuilder              &rFB,
        Implement<FITerrainJolt>    terrainJolt,
        DependOn<FITerrain>         terrain,
        DependOn<FICommonScene>     comScn,
        DependOn<FIPhysics>         phys,
        DependOn<FIJolt>            jolt)
{
    using namespace planeta;

    auto &rTerrainJolt  = rFB.data_emplace<ACtxTerrainJolt>(terrainJolt.di.terrainJolt);
    auto &rJolt         = rFB.data_get<ACtxJoltWorld>(jolt.di.jolt);

    JPH::BodyInterface &bodyInterface  = rJolt.m_physicsSystem.GetBodyInterface();

    JPH::MutableCompoundShapeSettings shapeSettings;
    rTerrainJolt.shape = static_cast<JPH::MutableCompoundShape*>(shapeSettings.Create().Get().GetPtr());
    rTerrainJolt.bodyId = rJolt.m_bodyIds.create();

    JPH::BodyCreationSettings bodyCreation(rTerrainJolt.shape, JPH::Vec3(0.0f, 0.0f, 0.0f), JPH::Quat::sIdentity(), JPH::EMotionType::Static, Layers::NON_MOVING);
    bodyCreation.mFriction = 2.0f;
    bodyCreation.mEnhancedInternalEdgeRemoval = true;

    JPH::BodyID joltBodyId = BToJolt(rTerrainJolt.bodyId);
    bodyInterface.CreateBodyWithID(joltBodyId, bodyCreation);

    JPH::BodyInterface::AddState addState = bodyInterface.AddBodiesPrepare(&joltBodyId, 1);
    bodyInterface.AddBodiesFinalize(&joltBodyId, 1, addState, JPH::EActivation::Activate);

    rFB.task()
        .name       ("Add Jolt physics shapes to chunks")
        .sync_with  ({terrain.pl.surfaceChanges(UseOrRun), terrain.pl.chunkMesh(Ready), terrain.pl.terrainFrame(Ready),  terrain.pl.skeleton(Ready),  jolt.pl.joltBody(New), phys.pl.physUpdate(Done)})
        .args       ({           comScn.di.basic,         terrain.di.terrain,                 terrain.di.terrainFrame, phys.di.phys,     terrainJolt.di.terrainJolt,         jolt.di.jolt   })
        .func       ([] (ACtxBasic const &rBasic, ACtxTerrain const& terrain, ACtxTerrainFrame const& terrainFrame, ACtxPhysics& rPhys, ACtxTerrainJolt &rTerrainJolt, ACtxJoltWorld& rJolt) noexcept
    {
        JPH::BodyInterface  &bodyInterface  = rJolt.m_physicsSystem.GetBodyInterface();
        auto                *pShape         = static_cast<JPH::MutableCompoundShape*>(rTerrainJolt.shape.GetPtr());
        JPH::Vec3           prevCOM         = pShape->GetCenterOfMass();
        JPH::BodyID         joltBodyId      = BToJolt(rTerrainJolt.bodyId);
        float         const scale           = std::exp2(float(-terrain.skData.precision));

        // possible hack/optimization: reduce number of times CalculateSubShapeBounds is called
        // auto &rSubShapes = const_cast<JPH::CompoundShape::SubShapes&>(pShape->GetSubShapes());

        {
            JPH::BodyLockWrite lock(rJolt.m_physicsSystem.GetBodyLockInterface(), joltBodyId);

            auto const &rSubShapes = const_cast<JPH::CompoundShape::SubShapes&>(pShape->GetSubShapes());

            for (std::size_t i = rSubShapes.size() - 1; i != ~std::size_t(0); --i)
            {
                auto const sktri = SkTriId::from_index(rSubShapes[i].mUserData);
                if (terrain.scratchpad.surfaceRemoved.contains(sktri))
                {
                    pShape->RemoveShape(i);
                    rTerrainJolt.positions.erase(rTerrainJolt.positions.begin() + i);
                }
            }

            for (SkTriId sktriId : terrain.scratchpad.surfaceAdded)
            {
                SkeletonTriangle const& sktri = terrain.skeleton.tri_at(sktriId);

                if (terrain.skeleton.tri_group_at(tri_group_id(sktriId)).depth != terrain.skeleton.levelMax)
                {
                    continue;
                }

                auto const sharedVrtxs = std::array<SharedVrtxId, 3>{{
                        terrain.skChunks.m_skVrtxToShared[sktri.vertices[0]],
                        terrain.skChunks.m_skVrtxToShared[sktri.vertices[1]],
                        terrain.skChunks.m_skVrtxToShared[sktri.vertices[2]]}};

                auto posView = terrain.chunkGeom.vbufPositions.view_const(terrain.chunkGeom.vrtxBuffer, terrain.chunkInfo.vrtxTotal);

                JPH::Vec3 const vrtx0    = Vec3MagnumToJolt(posView[terrain.chunkInfo.vbufSharedOffset + sharedVrtxs[0].value]);
                JPH::Vec3 const vrtx1Rel = Vec3MagnumToJolt(posView[terrain.chunkInfo.vbufSharedOffset + sharedVrtxs[1].value]) - vrtx0;
                JPH::Vec3 const vrtx2Rel = Vec3MagnumToJolt(posView[terrain.chunkInfo.vbufSharedOffset + sharedVrtxs[2].value]) - vrtx0;

                float const scaleInv = std::exp2(-float(terrain.skData.precision));

                constexpr float thickness = 24.0f;

                JPH::Vec3 const center = Vec3MagnumToJolt(Vector3(terrainFrame.position) * scaleInv)
                                       + (vrtx1Rel + vrtx2Rel) / 3.0f;

                JPH::Vec3 down = -center.Normalized() * thickness;

                std::array<JPH::Vec3, 6> const points{{
                    JPH::Vec3(0.0f, 0.0f, 0.0f), vrtx1Rel, vrtx2Rel,
                    down, vrtx1Rel + down, vrtx2Rel + down
                }};

                JPH::ConvexHullShapeSettings hull(points.data(), points.size());

                pShape->AddShape(vrtx0, JPH::Quat::sIdentity(), hull.Create().Get(), sktriId.value);
                rTerrainJolt.positions.push_back(vrtx0);
            }
        }

        bodyInterface.NotifyShapeChanged(joltBodyId, prevCOM, false, JPH::EActivation::Activate);
    });

    rFB.task()
        .name       ("Translate jolt colliders according to universe")
        .sync_with  ({terrain.pl.terrainFrame(Ready), jolt.pl.joltBody(Modify),  phys.pl.physUpdate(ModifyOrSignal)})
        .args       ({           terrain.di.terrain,              terrain.di.terrainFrame,    terrainJolt.di.terrainJolt,         jolt.di.jolt   })
        .func       ([] (ACtxTerrain const& terrain, ACtxTerrainFrame const& terrainFrame, ACtxTerrainJolt &rTerrainJolt, ACtxJoltWorld& rJolt) noexcept
    {
        JPH::BodyInterface  &bodyInterface  = rJolt.m_physicsSystem.GetBodyInterface();
        auto*         const pShape          = static_cast<JPH::MutableCompoundShape*>(rTerrainJolt.shape.GetPtr());
        JPH::Vec3     const prevCOM         = pShape->GetCenterOfMass();
        JPH::BodyID         joltBodyId      = BToJolt(rTerrainJolt.bodyId);

        if (rTerrainJolt.originPos != terrainFrame.position)
        {
            float     const scale        = std::exp2(float(-terrain.skData.precision));
            Vector3l  const deltaOffset  = rTerrainJolt.originPos - terrainFrame.position;
            JPH::Vec3 const deltaOffsetF = Vec3MagnumToJolt(Vector3(deltaOffset) * scale);

            rTerrainJolt.originPos = terrainFrame.position;

            for (JPH::Vec3 &rPos : rTerrainJolt.positions)
            {
                rPos += deltaOffsetF;
            }

            JPH::Quat const dummy = JPH::Quat::sIdentity();

            // ModifyShapes will assert if given size=0
            if ( ! rTerrainJolt.positions.empty() )
            {
               pShape->ModifyShapes(0, rTerrainJolt.positions.size(), rTerrainJolt.positions.data(), &dummy, sizeof(JPH::Vec3), 0);
            }
        }

        bodyInterface.NotifyShapeChanged(joltBodyId, prevCOM, false, JPH::EActivation::Activate);
    });

}); // ftrPhysicsShapesJolt

} // namespace adera

