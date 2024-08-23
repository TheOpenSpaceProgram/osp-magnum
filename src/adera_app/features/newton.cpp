#if 0
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
#include "newton.h"
#include "physics.h"
#include "shapes.h"

#include <osp/activescene/basic_fn.h>
#include <osp/activescene/physics_fn.h>
#include <osp/activescene/prefab_fn.h>
#include <osp/activescene/vehicles.h>
#include <osp/core/Resources.h>
#include <osp/drawing/drawing.h>
#include <osp/vehicles/ImporterData.h>

#include <adera/machines/links.h>

#include <ospnewton/activescene/newtoninteg_fn.h>

using namespace osp;
using namespace osp::active;
using namespace osp::link;
using namespace ospnewton;

using osp::restypes::gc_importer;
using Corrade::Containers::arrayView;

namespace adera
{


Session setup_newton(
        TopTaskBuilder&             rFB,
        ArrayView<entt::any> const  topData,
        osp::Session const&         scene,
        Session const&              commonScene,
        Session const&              physics)
{
    OSP_DECLARE_GET_DATA_IDS(scene,         TESTAPP_DATA_SCENE);
    OSP_DECLARE_GET_DATA_IDS(commonScene,   TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(physics,       TESTAPP_DATA_PHYSICS);

    auto const scn.pl    = scene         .get_pipelines<PlScene>();
    auto const comScn.pl     = commonScene   .get_pipelines<PlCommonScene>();
    auto const phys.pl    = physics       .get_pipelines<PlPhysics>();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, TESTAPP_DATA_NEWTON);
    auto const tgNwt = out.create_pipelines<PlNewton>(rFB);

    rFB.pipeline(tgNwt.nwtBody).parent(scn.pl.update);

    rFB.data_emplace< ACtxNwtWorld >(idNwt, 2);

    using ospnewton::SysNewton;

    rFB.task()
        .name       ("Delete Newton components")
        .run_on     ({comScn.di.activeEntDel(UseOrRun)})
        .sync_with  ({tgNwt.nwtBody(Delete)})
        .push_to    (out.m_tasks)
        .args({                idNwt,                      comScn.di.activeEntDel })
        .func([] (ACtxNwtWorld& rNwt, ActiveEntVec_t const& rActiveEntDel) noexcept
    {
        SysNewton::update_delete (rNwt, rActiveEntDel.cbegin(), rActiveEntDel.cend());
    });

    rFB.task()
        .name       ("Update Newton world")
        .run_on     ({scn.pl.update(Run)})
        .sync_with  ({tgNwt.nwtBody(Prev), comScn.pl.hierarchy(Prev), phys.pl.physBody(Prev), phys.pl.physUpdate(Run), comScn.pl.transform(Prev)})
        .push_to    (out.m_tasks)
        .args({             comScn.di.basic,             phys.di.phys,              idNwt,           scn.di.deltaTimeIn })
        .func([] (ACtxBasic& rBasic, ACtxPhysics& rPhys, ACtxNwtWorld& rNwt, float const deltaTimeIn, WorkerContext ctx) noexcept
    {
        SysNewton::update_world(rPhys, rNwt, deltaTimeIn, rBasic.m_scnGraph, rBasic.m_transform);
    });

    rFB.data_emplace< ACtxNwtWorld >(idNwt, 2);

    return out;
} // setup_newton




osp::Session setup_newton_factors(
        TopTaskBuilder&             rFB,
        ArrayView<entt::any>        topData)
{
    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, TESTAPP_DATA_NEWTON_FORCES);

    auto &rFactors = rFB.data_emplace<ForceFactors_t>(idNwtFactors);

    std::fill(rFactors.begin(), rFactors.end(), 0);

    return out;
} // setup_newton_factors




osp::Session setup_newton_force_accel(
        TopTaskBuilder&             rFB,
        ArrayView<entt::any>        topData,
        Session const&              newton,
        Session const&              nwtFactors,
        Vector3                     accel)
{
    using UserData_t = ACtxNwtWorld::ForceFactorFunc::UserData_t;
    OSP_DECLARE_GET_DATA_IDS(newton,     TESTAPP_DATA_NEWTON);
    OSP_DECLARE_GET_DATA_IDS(nwtFactors, TESTAPP_DATA_NEWTON_FORCES);

    auto &rNwt      = rFB.data_get<ACtxNwtWorld>(idNwt);

    Session nwtAccel;
    OSP_DECLARE_CREATE_DATA_IDS(nwtAccel, TESTAPP_DATA_NEWTON_ACCEL);

    auto &rAccel    = rFB.data_emplace<Vector3>(idAcceleration, accel);

    ACtxNwtWorld::ForceFactorFunc const factor
    {
        .m_func = [] (NewtonBody const* pBody, BodyId const bodyID, ACtxNwtWorld const& rNwt, UserData_t data, Vector3& rForce, Vector3& rTorque) noexcept
        {
            float mass = 0.0f;
            float dummy = 0.0f;
            NewtonBodyGetMass(pBody, &mass, &dummy, &dummy, &dummy);

            auto const& force = *reinterpret_cast<Vector3 const*>(data[0]);
            rForce += force * mass;
        },
        .m_userData = {&rAccel}
    };

    // Register force

    std::size_t const index = rNwt.m_factors.size();
    rNwt.m_factors.emplace_back(factor);

    auto factorBits = lgrn::bit_view(rFB.data_get<ForceFactors_t>(idNwtFactors));
    factorBits.set(index);

    return nwtAccel;
} // setup_newton_force_accel




Session setup_phys_shapes_newton(
        TopTaskBuilder&             rFB,
        ArrayView<entt::any> const  topData,
        Session const&              commonScene,
        Session const&              physics,
        Session const&              physShapes,
        Session const&              newton,
        Session const&              nwtFactors)
{

    OSP_DECLARE_GET_DATA_IDS(commonScene,   TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(physics,       TESTAPP_DATA_PHYSICS);
    OSP_DECLARE_GET_DATA_IDS(physShapes,    TESTAPP_DATA_PHYS_SHAPES);
    OSP_DECLARE_GET_DATA_IDS(newton,        TESTAPP_DATA_NEWTON);
    OSP_DECLARE_GET_DATA_IDS(nwtFactors,    TESTAPP_DATA_NEWTON_FORCES);

    auto const phys.pl    = physics       .get_pipelines<PlPhysics>();
    auto const physShapes.pl   = physShapes    .get_pipelines<PlPhysShapes>();
    auto const tgNwt    = newton        .get_pipelines<PlNewton>();

    Session out;

    rFB.task()
        .name       ("Add Newton physics to spawned shapes")
        .run_on     ({physShapes.pl.spawnRequest(UseOrRun)})
        .sync_with  ({physShapes.pl.spawnedEnts(UseOrRun), tgNwt.nwtBody(New), phys.pl.physUpdate(Done)})
        .push_to    (out.m_tasks)
        .args({                   comScn.di.basic,                physShapes.di.physShapes,             phys.di.phys,              idNwt,              idNwtFactors })
        .func([] (ACtxBasic const &rBasic, ACtxPhysShapes& rPhysShapes, ACtxPhysics& rPhys, ACtxNwtWorld& rNwt, ForceFactors_t nwtFactors) noexcept
    {
        for (std::size_t i = 0; i < rPhysShapes.m_spawnRequest.size(); ++i)
        {
            SpawnShape const &spawn = rPhysShapes.m_spawnRequest[i];
            ActiveEnt const root    = rPhysShapes.m_ents[i * 2];
            ActiveEnt const child   = rPhysShapes.m_ents[i * 2 + 1];

            NwtColliderPtr_t pCollision{ SysNewton::create_primative(rNwt, spawn.m_shape) };
            SysNewton::orient_collision(pCollision.get(), spawn.m_shape, {0.0f, 0.0f, 0.0f}, Matrix3{}, spawn.m_size);
            NewtonBody *pBody = NewtonCreateDynamicBody(rNwt.m_world.get(), pCollision.get(), Matrix4{}.data());

            BodyId const bodyId = rNwt.m_bodyIds.create();
            SysNewton::resize_body_data(rNwt);

            rNwt.m_bodyPtrs[bodyId].reset(pBody);

            rNwt.m_bodyToEnt[bodyId]    = root;
            rNwt.m_bodyFactors[bodyId]  = nwtFactors;
            rNwt.m_entToBody.emplace(root, bodyId);

            Vector3 const inertia = collider_inertia_tensor(spawn.m_shape, spawn.m_size, spawn.m_mass);

            NewtonBodySetMassMatrix(pBody, spawn.m_mass, inertia.x(), inertia.y(), inertia.z());
            NewtonBodySetMatrix(pBody, Matrix4::translation(spawn.m_position).data());
            NewtonBodySetLinearDamping(pBody, 0.0f);
            NewtonBodySetForceAndTorqueCallback(pBody, &SysNewton::cb_force_torque);
            NewtonBodySetTransformCallback(pBody, &SysNewton::cb_set_transform);
            SysNewton::set_userdata_bodyid(pBody, bodyId);
        }
    });

    return out;
} // setup_phys_shapes_newton





void compound_collect_recurse(
        ACtxPhysics const&  rCtxPhys,
        ACtxNwtWorld&       rCtxWorld,
        ACtxBasic const&    rBasic,
        ActiveEnt           ent,
        Matrix4 const&      transform,
        NewtonCollision*    pCompound)
{
    EShape const shape = rCtxPhys.m_shape[ent];

    if (shape != EShape::None)
    {
        NwtColliderPtr_t &rPtr = rCtxWorld.m_colliders.contains(ent)
                               ? rCtxWorld.m_colliders.get(ent)
                               : rCtxWorld.m_colliders.emplace(ent);

        if (rPtr.get() == nullptr)
        {
            rPtr = SysNewton::create_primative(rCtxWorld, shape);
        }

        SysNewton::orient_collision(rPtr.get(), shape, transform.translation(), transform.rotation(), transform.scaling());
        NewtonCompoundCollisionAddSubCollision(pCompound, rPtr.get());
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
                    rCtxPhys, rCtxWorld, rBasic, child, childMatrix, pCompound);
        }
    }
}



Session setup_vehicle_spawn_newton(
        TopTaskBuilder&             rFB,
        ArrayView<entt::any> const  topData,
        Session const&              application,
        Session const&              commonScene,
        Session const&              physics,
        Session const&              prefabs,
        Session const&              parts,
        Session const&              vehicleSpawn,
        Session const&              newton)
{
    OSP_DECLARE_GET_DATA_IDS(application,   TESTAPP_DATA_APPLICATION);
    OSP_DECLARE_GET_DATA_IDS(commonScene,   TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(physics,       TESTAPP_DATA_PHYSICS);
    OSP_DECLARE_GET_DATA_IDS(prefabs,       TESTAPP_DATA_PREFABS);
    OSP_DECLARE_GET_DATA_IDS(vehicleSpawn,  TESTAPP_DATA_VEHICLE_SPAWN);
    OSP_DECLARE_GET_DATA_IDS(newton,        TESTAPP_DATA_NEWTON);
    OSP_DECLARE_GET_DATA_IDS(parts,         TESTAPP_DATA_PARTS);
    auto const comScn.pl     = commonScene   .get_pipelines<PlCommonScene>();
    auto const phys.pl    = physics       .get_pipelines<PlPhysics>();
    auto const prefabs.pl     = prefabs       .get_pipelines<PlPrefabs>();
    auto const parts.pl  = parts         .get_pipelines<PlParts>();
    auto const vhclSpawn.pl   = vehicleSpawn  .get_pipelines<PlVehicleSpawn>();
    auto const tgNwt    = newton        .get_pipelines<PlNewton>();

    Session out;

    rFB.task()
        .name       ("Create root ActiveEnts for each Weld")
        .run_on     ({vhclSpawn.pl.spawnRequest(UseOrRun)})
        .sync_with  ({comScn.pl.activeEnt(New), comScn.pl.activeEntResized(Schedule), parts.pl.mapWeldActive(Modify), vhclSpawn.pl.rootEnts(Resize)})
        .push_to    (out.m_tasks)
        .args       ({      comScn.di.basic,                  vhclSpawn.di.vehicleSpawn,           parts.di.scnParts})
        .func([] (ACtxBasic& rBasic, ACtxVehicleSpawn& rVehicleSpawn, ACtxParts& rScnParts) noexcept
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
        .run_on     ({vhclSpawn.pl.spawnRequest(UseOrRun)})
        .sync_with  ({vhclSpawn.pl.rootEnts(UseOrRun), parts.pl.mapWeldActive(Ready), prefabs.pl.spawnedEnts(UseOrRun), prefabs.pl.spawnRequest(UseOrRun), prefabs.pl.inSubtree(Run), comScn.pl.transform(Ready), comScn.pl.hierarchy(Modify)})
        .push_to    (out.m_tasks)
        .args       ({      comScn.di.basic,                        vhclSpawn.di.vehicleSpawn,           parts.di.scnParts,             prefabs.di.prefabs,            mainApp.di.resources})
        .func([] (ACtxBasic& rBasic, ACtxVehicleSpawn const& rVehicleSpawn, ACtxParts& rScnParts, ACtxPrefabs& rPrefabs, Resources& rResources) noexcept
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
                for (PartId const part : rScnParts.weldToParts[weld])
                {
                    SpPartId const newPart = rVehicleSpawn.partToSpawned[part];
                    uint32_t const prefabInit = rVehicleSpawn.spawnedPrefabs[newPart];
                    entCount += rPrefabs.spawnedEntsOffset[prefabInit].size();
                }

                ActiveEnt const weldEnt = rScnParts.weldToActive[weld];

                rBasic.m_transform.emplace(weldEnt, Matrix4::from(toInit.rotation.toMatrix(), toInit.position));

                SubtreeBuilder bldRoot = SysSceneGraph::add_descendants(rBasic.m_scnGraph, entCount + 1);
                SubtreeBuilder bldWeld = bldRoot.add_child(weldEnt, entCount);

                for (PartId const part : rScnParts.weldToParts[weld])
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
        .name       ("Add Newton physics to Weld entities")
        .run_on     ({vhclSpawn.pl.spawnRequest(UseOrRun)})
        .sync_with  ({vhclSpawn.pl.rootEnts(UseOrRun), prefabs.pl.spawnedEnts(UseOrRun), comScn.pl.transform(Ready), phys.pl.physBody(Ready), tgNwt.nwtBody(New), phys.pl.physUpdate(Done), comScn.pl.hierarchy(Ready)})
        .push_to    (out.m_tasks)
        .args       ({      comScn.di.basic,             phys.di.phys,              idNwt,                        vhclSpawn.di.vehicleSpawn,                 parts.di.scnParts})
        .func([] (ACtxBasic& rBasic, ACtxPhysics& rPhys, ACtxNwtWorld& rNwt, ACtxVehicleSpawn const& rVehicleSpawn, ACtxParts const& rScnParts) noexcept
    {
        LGRN_ASSERT(rVehicleSpawn.new_vehicle_count() != 0);

        rPhys.m_hasColliders.resize(rBasic.m_activeIds.capacity());

        auto const& itWeldsFirst        = std::begin(rVehicleSpawn.spawnedWelds);
        auto const& itWeldOffsetsLast   = std::end(rVehicleSpawn.spawnedWeldOffsets);
        auto itWeldOffsets              = std::begin(rVehicleSpawn.spawnedWeldOffsets);

        for (ACtxVehicleSpawn::TmpToInit const& toInit : rVehicleSpawn.spawnRequest)
        {
            auto const itWeldOffsetsNext = std::next(itWeldOffsets);
            SpWeldId const weldOffsetNext = (itWeldOffsetsNext != itWeldOffsetsLast)
                                           ? (*itWeldOffsetsNext)
                                           : SpWeldId(uint32_t(rVehicleSpawn.spawnedWelds.size()));

            std::for_each(itWeldsFirst + std::ptrdiff_t{*itWeldOffsets},
                          itWeldsFirst + std::ptrdiff_t{weldOffsetNext},
                          [&rBasic, &rScnParts, &rVehicleSpawn, &toInit, &rPhys, &rNwt] (WeldId const weld)
            {
                ActiveEnt const weldEnt = rScnParts.weldToActive[weld];

                auto const transform = Matrix4::from(toInit.rotation.toMatrix(), toInit.position);
                NwtColliderPtr_t pCompound{ NewtonCreateCompoundCollision(rNwt.m_world.get(), 0) };

                rPhys.m_hasColliders.insert(weldEnt);

                // Collect all colliders from hierarchy.
                NewtonCompoundCollisionBeginAddRemove(pCompound.get());
                compound_collect_recurse( rPhys, rNwt, rBasic, weldEnt, Matrix4{}, pCompound.get() );
                NewtonCompoundCollisionEndAddRemove(pCompound.get());

                NewtonBody *pBody = NewtonCreateDynamicBody(rNwt.m_world.get(), pCompound.get(), Matrix4{}.data());

                BodyId const bodyId = rNwt.m_bodyIds.create();
                SysNewton::resize_body_data(rNwt);


                rNwt.m_bodyPtrs[bodyId].reset(pBody);
                rNwt.m_bodyToEnt[bodyId] = weldEnt;
                rNwt.m_bodyFactors[bodyId] = {1}; // TODO: temporary
                rNwt.m_entToBody.emplace(weldEnt, bodyId);

                float   totalMass = 0.0f;
                Vector3 massPos{0.0f};
                SysPhysics::calculate_subtree_mass_center(rBasic.m_transform, rPhys, rBasic.m_scnGraph, weldEnt, massPos, totalMass);

                Vector3 const com = massPos / totalMass;
                auto const comToOrigin = Matrix4::translation( - com );

                Matrix3 inertiaTensor{0.0f};
                SysPhysics::calculate_subtree_mass_inertia(rBasic.m_transform, rPhys, rBasic.m_scnGraph, weldEnt, inertiaTensor, comToOrigin);

                Matrix4 const inertiaTensorMat4{inertiaTensor};
                //NewtonBodySetMassMatrix(pBody, 0.0f, 1.0f, 1.0f, 1.0f);
                NewtonBodySetFullMassMatrix         (pBody, totalMass, inertiaTensorMat4.data());
                NewtonBodySetCentreOfMass           (pBody, com.data());
                NewtonBodySetGyroscopicTorque       (pBody, 1);
                NewtonBodySetMatrix                 (pBody, transform.data());
                NewtonBodySetLinearDamping          (pBody, 0.0f);
                NewtonBodySetAngularDamping         (pBody, Vector3{0.0f}.data());
                NewtonBodySetForceAndTorqueCallback (pBody, &SysNewton::cb_force_torque);
                NewtonBodySetTransformCallback      (pBody, &SysNewton::cb_set_transform);
                SysNewton::set_userdata_bodyid      (pBody, bodyId);

                rPhys.m_setVelocity.emplace_back(weldEnt, toInit.velocity);
            });

            itWeldOffsets = itWeldOffsetsNext;
        }
    });

    return out;
} // setup_vehicle_spawn_newton


struct BodyRocket
{
    Quaternion      m_rotation;
    Vector3         m_offset;

    MachLocalId     m_local         {lgrn::id_null<MachLocalId>()};
    NodeId          m_throttleIn    {lgrn::id_null<NodeId>()};
    NodeId          m_multiplierIn  {lgrn::id_null<NodeId>()};
};

struct ACtxRocketsNwt
{
    // map each bodyId to a {machine, offset}
    lgrn::IntArrayMultiMap<BodyId, BodyRocket> m_bodyRockets;

};

static void assign_rockets(
        ACtxBasic const&            rBasic,
        ACtxParts const&            rScnParts,
        ACtxNwtWorld&               rNwt,
        ACtxRocketsNwt&             rRocketsNwt,
        Nodes const&                rFloatNodes,
        PerMachType const&          machtypeRocket,
        ForceFactors_t const&       rNwtFactors,
        WeldId const                weld,
        std::vector<BodyRocket>&    rTemp)
{
    using adera::gc_mtMagicRocket;
    using adera::ports_magicrocket::gc_throttleIn;
    using adera::ports_magicrocket::gc_multiplierIn;

    ActiveEnt const weldEnt = rScnParts.weldToActive[weld];
    BodyId const    body    = rNwt.m_entToBody.at(weldEnt);

    if (rRocketsNwt.m_bodyRockets.contains(body))
    {
        rRocketsNwt.m_bodyRockets.erase(body);
    }

    for (PartId const part : rScnParts.weldToParts[weld])
    {
        auto const sizeBefore = rTemp.size();

        for (MachinePair const pair : rScnParts.partToMachines[part])
        {
            if (pair.type != gc_mtMagicRocket)
            {
                continue; // This machine is not a rocket
            }

            MachAnyId const mach            = machtypeRocket.localToAny[pair.local];
            auto const&     portSpan        = rFloatNodes.machToNode[mach];
            NodeId const    throttleIn      = connected_node(portSpan, gc_throttleIn.port);
            NodeId const    multiplierIn    = connected_node(portSpan, gc_multiplierIn.port);

            if (   (throttleIn   == lgrn::id_null<NodeId>())
                || (multiplierIn == lgrn::id_null<NodeId>()) )
            {
                continue; // Throttle and/or multiplier is not connected
            }

            BodyRocket &rBodyRocket     = rTemp.emplace_back();
            rBodyRocket.m_local         = pair.local;
            rBodyRocket.m_throttleIn    = throttleIn;
            rBodyRocket.m_multiplierIn  = multiplierIn;
        }

        if (sizeBefore == rTemp.size())
        {
            continue; // No rockets found
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

        for (BodyRocket &rBodyRocket : arrayView(rTemp).exceptPrefix(sizeBefore))
        {
            rBodyRocket.m_rotation  = rotation;
            rBodyRocket.m_offset    = offset;
        }
    }

    ForceFactors_t &rBodyFactors = rNwt.m_bodyFactors[body];

    // TODO: Got lazy, eventually iterate ForceFactors_t instead of
    //       just using [0]. This breaks if more factor bits are added
    static_assert(ForceFactors_t{}.size() == 1u);

    if ( rTemp.empty() )
    {
        rBodyFactors[0] &= ~rNwtFactors[0];
        return;
    }

    rBodyFactors[0] |= rNwtFactors[0];

    rRocketsNwt.m_bodyRockets.emplace(body, rTemp.begin(), rTemp.end());
    rTemp.clear();
}

// ACtxNwtWorld::ForceFactorFunc::Func_t
static void rocket_thrust_force(NewtonBody const* pBody, BodyId const body, ACtxNwtWorld const& rNwt, ACtxNwtWorld::ForceFactorFunc::UserData_t data, Vector3& rForce, Vector3& rTorque) noexcept
{
    auto const& rRocketsNwt     = *reinterpret_cast<ACtxRocketsNwt const*>          (data[0]);
    auto const& rMachines       = *reinterpret_cast<Machines const*>                (data[1]);
    auto const& rSigValFloat    = *reinterpret_cast<SignalValues_t<float> const*>   (data[2]);

    auto &rBodyRockets = rRocketsNwt.m_bodyRockets[body];

    if (rBodyRockets.size() == 0)
    {
        return;
    }

    std::array<dFloat, 4> nwtRot; // quaternion xyzw
    NewtonBodyGetRotation(pBody, nwtRot.data());
    Quaternion const rot{{nwtRot[0], nwtRot[1], nwtRot[2]}, nwtRot[3]};

    Vector3 com;
    NewtonBodyGetCentreOfMass(pBody, com.data());

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

Session setup_rocket_thrust_newton(
        TopTaskBuilder&             rFB,
        ArrayView<entt::any> const  topData,
        Session const&              scene,
        Session const&              commonScene,
        Session const&              physics,
        Session const&              prefabs,
        Session const&              parts,
        Session const&              signalsFloat,
        Session const&              newton,
        Session const&              nwtFactors)
{
    OSP_DECLARE_GET_DATA_IDS(scene,         TESTAPP_DATA_SCENE);
    OSP_DECLARE_GET_DATA_IDS(commonScene,   TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(physics,       TESTAPP_DATA_PHYSICS);
    OSP_DECLARE_GET_DATA_IDS(parts,         TESTAPP_DATA_PARTS);
    OSP_DECLARE_GET_DATA_IDS(signalsFloat,  TESTAPP_DATA_SIGNALS_FLOAT)
    OSP_DECLARE_GET_DATA_IDS(newton,        TESTAPP_DATA_NEWTON);
    OSP_DECLARE_GET_DATA_IDS(nwtFactors,    TESTAPP_DATA_NEWTON_FORCES);
    auto const scn.pl    = scene         .get_pipelines<PlScene>();
    auto const parts.pl  = parts         .get_pipelines<PlParts>();
    auto const tgNwt    = newton        .get_pipelines<PlNewton>();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, TESTAPP_DATA_ROCKETS_NWT);

    auto &rRocketsNwt   = rFB.data_emplace< ACtxRocketsNwt >(idRocketsNwt);

    rFB.task()
        .name       ("Assign rockets to Newton bodies")
        .run_on     ({scn.pl.update(Run)})
        .sync_with  ({parts.pl.weldIds(Ready), tgNwt.nwtBody(Ready), parts.pl.connect(Ready)})
        .push_to    (out.m_tasks)
        .args       ({      comScn.di.basic,             phys.di.phys,              idNwt,                 parts.di.scnParts,                idRocketsNwt,                      idNwtFactors})
        .func([] (ACtxBasic& rBasic, ACtxPhysics& rPhys, ACtxNwtWorld& rNwt, ACtxParts const& rScnParts, ACtxRocketsNwt& rRocketsNwt, ForceFactors_t const& rNwtFactors) noexcept
    {
        using adera::gc_mtMagicRocket;

        Nodes const &rFloatNodes = rScnParts.nodePerType[gc_ntSigFloat];
        PerMachType const& machtypeRocket = rScnParts.machines.perType[gc_mtMagicRocket];

        rRocketsNwt.m_bodyRockets.ids_reserve(rNwt.m_bodyIds.size());
        rRocketsNwt.m_bodyRockets.data_reserve(rScnParts.machines.perType[gc_mtMagicRocket].localIds.capacity());

        std::vector<BodyRocket> temp;

        for (WeldId const weld : rScnParts.weldDirty)
        {
            assign_rockets(rBasic, rScnParts, rNwt, rRocketsNwt, rFloatNodes, machtypeRocket, rNwtFactors, weld, temp);
        }
    });

    auto &rScnParts     = rFB.data_get< ACtxParts >              (parts.di.scnParts);
    auto &rSigValFloat  = rFB.data_get< SignalValues_t<float> >  (sigFloat.di.sigValFloat);
    Machines &rMachines = rScnParts.machines;


    ACtxNwtWorld::ForceFactorFunc const factor
    {
        .m_func = &rocket_thrust_force,
        .m_userData = { &rRocketsNwt, &rMachines, &rSigValFloat }
    };

    auto &rNwt = rFB.data_get<ACtxNwtWorld>(idNwt);

    std::size_t const index = rNwt.m_factors.size();
    rNwt.m_factors.emplace_back(factor);

    auto factorBits = lgrn::bit_view(rFB.data_get<ForceFactors_t>(idNwtFactors));
    factorBits.set(index);

    return out;
} // setup_rocket_thrust_newton


} // namespace adera


#endif
