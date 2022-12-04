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
#include "scene_physics.h"
#include "scene_common.h"
#include "scenarios.h"
#include "identifiers.h"

#include <osp/Active/basic.h>
#include <osp/Active/drawing.h>
#include <osp/Active/physics.h>
#include <osp/Active/SysSceneGraph.h>
#include <osp/Active/SysPrefabInit.h>
#include <osp/Active/SysPhysics.h>
#include <osp/Active/SysRender.h>

#include <osp/Resource/ImporterData.h>
#include <osp/Resource/resources.h>
#include <osp/Resource/resourcetypes.h>

using namespace osp;
using namespace osp::active;
using osp::restypes::gc_importer;
using osp::phys::EShape;
using Corrade::Containers::arrayView;

namespace testapp::scenes
{

Session setup_physics(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              scnCommon)
{
    OSP_SESSION_UNPACK_DATA(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);

    Session physics;
    OSP_SESSION_ACQUIRE_DATA(physics, topData, TESTAPP_PHYSICS);
    OSP_SESSION_ACQUIRE_TAGS(physics, rTags, TESTAPP_PHYSICS);


    top_emplace< ACtxPhysics >  (topData, idPhys);

    // 'Per-thread' inputs fed into the physics engine. Only one here for now
    //top_emplace< ACtxPhysInputs >(topData, idPhysIn);

    rBuilder.tag(tgPhysBodyMod)     .depend_on({tgPhysBodyDel});
    rBuilder.tag(tgPhysBodyReq)     .depend_on({tgPhysBodyDel, tgPhysBodyMod, tgPhysMod});
    rBuilder.tag(tgPhysReq)         .depend_on({tgPhysMod});

    physics.task() = rBuilder.task().assign({tgSceneEvt, tgDelTotalReq, tgPhysBodyDel}).data(
            "Delete Physics components",
            TopDataIds_t{             idPhys,                   idDelTotal},
            wrap_args([] (ACtxPhysics& rPhys, EntVector_t const& rDelTotal) noexcept
    {
        auto const &delFirst    = std::cbegin(rDelTotal);
        auto const &delLast     = std::cend(rDelTotal);

        SysPhysics::update_delete_phys      (rPhys,     delFirst, delLast);
    }));

    return physics;
}

Session setup_shape_spawn(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              scnCommon,
        Session const&              physics,
        Session const&              material)
{
    OSP_SESSION_UNPACK_DATA(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(material,   TESTAPP_MATERIAL);
    OSP_SESSION_UNPACK_TAGS(material,   TESTAPP_MATERIAL);
    OSP_SESSION_UNPACK_DATA(physics,    TESTAPP_PHYSICS);
    OSP_SESSION_UNPACK_TAGS(physics,    TESTAPP_PHYSICS);


    Session shapeSpawn;
    OSP_SESSION_ACQUIRE_DATA(shapeSpawn, topData, TESTAPP_SHAPE_SPAWN);
    OSP_SESSION_ACQUIRE_TAGS(shapeSpawn, rTags, TESTAPP_SHAPE_SPAWN);

    top_emplace< SpawnerVec_t > (topData, idSpawner);
    top_emplace< EntVector_t >  (topData, idSpawnerEnts);

    rBuilder.tag(tgSpawnReq)        .depend_on({tgSpawnMod});
    rBuilder.tag(tgSpawnClr)        .depend_on({tgSpawnMod, tgSpawnReq});
    rBuilder.tag(tgSpawnEntReq)     .depend_on({tgSpawnEntMod});

    shapeSpawn.task() = rBuilder.task().assign({tgSceneEvt, tgSpawnReq, tgEntNew, tgSpawnEntMod}).data(
            "Create entities for requested shapes to spawn",
            TopDataIds_t{           idBasic,             idActiveIds,              idSpawner,             idSpawnerEnts },
            wrap_args([] (ACtxBasic& rBasic, ActiveReg_t& rActiveIds, SpawnerVec_t& rSpawner, EntVector_t& rSpawnerEnts) noexcept
    {
        if (rSpawner.size() == 0)
        {
            return;
        }

        rSpawnerEnts.resize(rSpawner.size() * 2);
        rActiveIds.create(std::begin(rSpawnerEnts), std::end(rSpawnerEnts));
    }));

    shapeSpawn.task() = rBuilder.task().assign({tgSceneEvt, tgSpawnReq, tgSpawnEntReq, tgTransformNew, tgHierNew}).data(
            "Add hierarchy and transform to spawned shapes",
            TopDataIds_t{           idBasic,             idActiveIds,              idSpawner,             idSpawnerEnts },
            wrap_args([] (ACtxBasic& rBasic, ActiveReg_t& rActiveIds, SpawnerVec_t& rSpawner, EntVector_t& rSpawnerEnts) noexcept
    {
        if (rSpawner.size() == 0)
        {
            return;
        }

        rBasic.m_scnGraph.resize(rActiveIds.capacity());
        SubtreeBuilder bldScnRoot = SysSceneGraph::add_descendants(rBasic.m_scnGraph, rSpawner.size() * 2);

        for (std::size_t i = 0; i < rSpawner.size(); ++i)
        {
            SpawnShape const &spawn = rSpawner[i];
            ActiveEnt const root    = rSpawnerEnts[i * 2];
            ActiveEnt const child   = rSpawnerEnts[i * 2 + 1];

            rBasic.m_transform.emplace(root, ACompTransform{osp::Matrix4::translation(spawn.m_position)});
            rBasic.m_transform.emplace(child, ACompTransform{Matrix4::scaling(spawn.m_size)});
            SubtreeBuilder bldRoot = bldScnRoot.add_child(root, 1);
            bldRoot.add_child(child);
        }
    }));

    shapeSpawn.task() = rBuilder.task().assign({tgSceneEvt, tgSpawnReq, tgSpawnEntReq, tgMeshMod, tgDrawMod, tgMatMod}).data(
            "Add mesh and material to spawned shapes",
            TopDataIds_t{             idDrawing,              idSpawner,             idSpawnerEnts,             idNMesh,          idMatEnts,             idMatDirty,                idActiveIds},
            wrap_args([] (ACtxDrawing& rDrawing, SpawnerVec_t& rSpawner, EntVector_t& rSpawnerEnts, NamedMeshes& rNMesh, EntSet_t& rMatEnts, EntVector_t& rMatDirty, ActiveReg_t const& rActiveIds ) noexcept
    {
        if (rSpawner.size() == 0)
        {
            return;
        }
        rMatEnts.ints().resize(rActiveIds.vec().capacity());
        rDrawing.m_drawable.ints().resize(rActiveIds.vec().capacity());

        for (std::size_t i = 0; i < rSpawner.size(); ++i)
        {
            SpawnShape const &spawn = rSpawner[i];
            ActiveEnt const root    = rSpawnerEnts[i * 2];
            ActiveEnt const child   = rSpawnerEnts[i * 2 + 1];

            rDrawing.m_mesh.emplace( child, rDrawing.m_meshRefCounts.ref_add(rNMesh.m_shapeToMesh.at(spawn.m_shape)) );
            rDrawing.m_meshDirty.push_back(child);

            rMatEnts.set(std::size_t(child));
            rMatDirty.push_back(child);

            rDrawing.m_visible.emplace(child);
            rDrawing.m_opaque.emplace(child);

            rDrawing.m_drawable.set(std::size_t(root));
            rDrawing.m_drawable.set(std::size_t(child));
        }
    }));

    shapeSpawn.task() = rBuilder.task().assign({tgSceneEvt, tgSpawnReq, tgSpawnEntReq, tgPhysBodyMod}).data(
            "Add physics to spawned shapes",
            TopDataIds_t{                   idActiveIds,              idSpawner,             idSpawnerEnts,             idPhys },
            wrap_args([] (ActiveReg_t const &rActiveIds, SpawnerVec_t& rSpawner, EntVector_t& rSpawnerEnts, ACtxPhysics& rPhys) noexcept
    {
        rPhys.m_hasColliders.ints().resize(rActiveIds.vec().capacity());
        rPhys.m_shape.resize(rActiveIds.capacity());
        for (std::size_t i = 0; i < rSpawner.size(); ++i)
        {
            SpawnShape const &spawn = rSpawner[i];
            ActiveEnt const root    = rSpawnerEnts[i * 2];
            ActiveEnt const child   = rSpawnerEnts[i * 2 + 1];

            rPhys.m_hasColliders.set(std::size_t(root));
            if (spawn.m_mass != 0.0f)
            {
                rPhys.m_setVelocity.emplace_back(root, spawn.m_velocity);
                Vector3 const inertia
                        = phys::collider_inertia_tensor(spawn.m_shape, spawn.m_size, spawn.m_mass);
                Vector3 const offset{0.0f, 0.0f, 0.0f};
                rPhys.m_ownDyn.emplace( child, ACompSubBody{ inertia, offset, spawn.m_mass } );
            }

            rPhys.m_shape[std::size_t(child)] = spawn.m_shape;
            rPhys.m_colliderDirty.push_back(child);
        }
    }));

    shapeSpawn.task() = rBuilder.task().assign({tgSceneEvt, tgSpawnClr}).data(
            "Clear Shape Spawning vector after use",
            TopDataIds_t{              idSpawner},
            wrap_args([] (SpawnerVec_t& rSpawner) noexcept
    {
        rSpawner.clear();
    }));

    return shapeSpawn;
}


Session setup_prefabs(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              scnCommon,
        Session const&              physics,
        Session const&              material,
        TopDataId const             idResources)
{
    OSP_SESSION_UNPACK_DATA(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(physics,    TESTAPP_PHYSICS);
    OSP_SESSION_UNPACK_TAGS(physics,    TESTAPP_PHYSICS);

    Session prefabs;
    OSP_SESSION_ACQUIRE_DATA(prefabs, topData,  TESTAPP_PREFABS);
    OSP_SESSION_ACQUIRE_TAGS(prefabs, rTags,    TESTAPP_PREFABS);

    rBuilder.tag(tgPrefabReq)       .depend_on({tgPrefabMod});
    rBuilder.tag(tgPrefabClr)       .depend_on({tgPrefabMod, tgPrefabReq});
    rBuilder.tag(tgPfParentHierReq) .depend_on({tgPfParentHierMod});
    rBuilder.tag(tgPrefabEntReq)    .depend_on({tgPrefabEntMod});

    top_emplace< ACtxPrefabInit > (topData, idPrefabInit);

    prefabs.task() = rBuilder.task().assign({tgSceneEvt, tgPrefabReq, tgPrefabEntMod, tgEntNew}).data(
            "Create Prefab entities",
            TopDataIds_t{             idActiveIds,                idPrefabInit,           idResources },
            wrap_args([] (ActiveReg_t& rActiveIds, ACtxPrefabInit& rPrefabInit, Resources& rResources) noexcept
    {
        // Count number of entities needed to be created
        std::size_t totalEnts = 0;
        for (TmpPrefabInitBasic const& rPfBasic : rPrefabInit.m_basic)
        {
            auto const& rPrefabData = rResources.data_get<Prefabs>(gc_importer, rPfBasic.m_importerRes);
            auto const& objects     = rPrefabData.m_prefabs[rPfBasic.m_prefabId];

            totalEnts += objects.size();
        }

        // Create entities
        rPrefabInit.m_newEnts.resize(totalEnts);
        rActiveIds.create(std::begin(rPrefabInit.m_newEnts), std::end(rPrefabInit.m_newEnts));

        // Assign new entities to each prefab to create
        rPrefabInit.m_ents.resize(rPrefabInit.m_basic.size());
        auto itEntAvailable = std::begin(rPrefabInit.m_newEnts);
        auto itPfEntSpanOut = std::begin(rPrefabInit.m_ents);
        for (TmpPrefabInitBasic& rPfBasic : rPrefabInit.m_basic)
        {
            auto const& rPrefabData = rResources.data_get<Prefabs>(gc_importer, rPfBasic.m_importerRes);
            auto const& objects     = rPrefabData.m_prefabs[rPfBasic.m_prefabId];

            (*itPfEntSpanOut) = { &(*itEntAvailable), objects.size() };

            std::advance(itEntAvailable, objects.size());
            std::advance(itPfEntSpanOut, 1);
        }

        assert(itEntAvailable == std::end(rPrefabInit.m_newEnts));
    }));

    prefabs.task() = rBuilder.task().assign({tgSceneEvt, tgPrefabReq, tgPrefabEntReq, tgHierNew, tgPfParentHierReq}).data(
            "Init Prefab hierarchy",
            TopDataIds_t{                idPrefabInit,           idResources,           idBasic },
            wrap_args([] (ACtxPrefabInit& rPrefabInit, Resources& rResources, ACtxBasic& rBasic) noexcept
    {
        //SysPrefabInit::init_hierarchy(rPrefabInit, rResources, rBasic.m_hierarchy);
    }));

    prefabs.task() = rBuilder.task().assign({tgSceneEvt, tgPrefabReq, tgPrefabEntReq, tgTransformNew}).data(
            "Init Prefab transforms",
            TopDataIds_t{                idPrefabInit,           idResources,           idBasic },
            wrap_args([] (ACtxPrefabInit& rPrefabInit, Resources& rResources, ACtxBasic& rBasic) noexcept
    {
        SysPrefabInit::init_transforms(rPrefabInit, rResources, rBasic.m_transform);
    }));

    if (material.m_dataIds.empty())
    {
        prefabs.task() = rBuilder.task().assign({tgSceneEvt, tgPrefabReq, tgPrefabEntReq, tgDrawMod, tgMeshMod}).data(
                "Init Prefab drawables (no material)",
                TopDataIds_t{                idPrefabInit,           idResources,             idDrawing,                idDrawingRes },
                wrap_args([] (ACtxPrefabInit& rPrefabInit, Resources& rResources, ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes) noexcept
        {
            SysPrefabInit::init_drawing(rPrefabInit, rResources, rDrawing, rDrawingRes, {});
        }));
    }
    else
    {
        OSP_SESSION_UNPACK_DATA(material,   TESTAPP_MATERIAL);
        OSP_SESSION_UNPACK_TAGS(material,   TESTAPP_MATERIAL);

        prefabs.task() = rBuilder.task().assign({tgSceneEvt, tgPrefabReq, tgPrefabEntReq, tgDrawMod, tgMeshMod, tgMatMod}).data(
                "Init Prefab drawables (single material)",
                TopDataIds_t{                idPrefabInit,           idResources,             idDrawing,                idDrawingRes,          idMatEnts,             idMatDirty,                   idActiveIds },
                wrap_args([] (ACtxPrefabInit& rPrefabInit, Resources& rResources, ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, EntSet_t& rMatEnts, EntVector_t& rMatDirty, ActiveReg_t const& rActiveIds) noexcept
        {
            rDrawing.m_drawable.ints().resize(rActiveIds.vec().capacity());
            rMatEnts.ints().resize(rActiveIds.vec().capacity());
            SysPrefabInit::init_drawing(rPrefabInit, rResources, rDrawing, rDrawingRes, {{rMatEnts, rMatDirty}});
        }));
    }

    prefabs.task() = rBuilder.task().assign({tgSceneEvt, tgPrefabReq, tgPrefabEntReq, tgPhysBodyMod}).data(
            "Init Prefab physics",
            TopDataIds_t{                   idActiveIds,                idPrefabInit,           idResources,             idPhys},
            wrap_args([] (ActiveReg_t const& rActiveIds, ACtxPrefabInit& rPrefabInit, Resources& rResources, ACtxPhysics& rPhys) noexcept
    {
        rPhys.m_hasColliders.ints().resize(rActiveIds.vec().capacity());
        rPhys.m_shape.resize(rActiveIds.capacity());
        SysPrefabInit::init_physics(rPrefabInit, rResources, rPhys);
    }));


    prefabs.task() = rBuilder.task().assign({tgSceneEvt, tgPrefabClr}).data(
            "Clear Prefab vector",
            TopDataIds_t{                idPrefabInit },
            wrap_args([] (ACtxPrefabInit& rPrefabInit) noexcept
    {
        rPrefabInit.m_basic.clear();
    }));


    return prefabs;
}

Session setup_gravity(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              scnCommon,
        Session const&              physics,
        Session const&              shapeSpawn)
{
    OSP_SESSION_UNPACK_DATA(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(physics,    TESTAPP_PHYSICS);
    OSP_SESSION_UNPACK_TAGS(physics,    TESTAPP_PHYSICS);
    OSP_SESSION_UNPACK_DATA(shapeSpawn, TESTAPP_SHAPE_SPAWN);
    OSP_SESSION_UNPACK_TAGS(shapeSpawn, TESTAPP_SHAPE_SPAWN);

    Session gravity;
    OSP_SESSION_ACQUIRE_DATA(gravity, topData, TESTAPP_GRAVITY);
    OSP_SESSION_ACQUIRE_TAGS(gravity, rTags, TESTAPP_GRAVITY);

    top_emplace< EntSet_t >       (topData, idGravity);

    rBuilder.tag(tgGravityReq)      .depend_on({tgGravityDel, tgGravityNew});
    rBuilder.tag(tgGravityNew)      .depend_on({tgGravityDel, tgPhysReq});

//    gravity.task() = rBuilder.task().assign({tgSceneEvt, tgGravityReq}).data(
//            "Apply gravity (-Z 9.81m/s^2 acceleration) to entities in the rGravity set",
//            TopDataIds_t{             idPhys,                idPhysIn,          idGravity   },
//            wrap_args([] (ACtxPhysics& rPhys, ACtxPhysInputs& rPhysIn, EntSet_t& rGravity) noexcept
//    {
//        acomp_storage_t<ACompPhysNetForce> &rNetForce = rPhysIn.m_physNetForce;
//        for (std::size_t const entInt : rGravity.ones())
//        {
//            auto const ent = ActiveEnt(entInt);
//            ACompPhysNetForce &rEntNetForce = rNetForce.contains(ent)
//                                            ? rNetForce.get(ent)
//                                            : rNetForce.emplace(ent);

//            rEntNetForce.z() -= 9.81f * rPhys.m_physDynamic.get(ent).m_totalMass;
//        }
//    }));

    gravity.task() = rBuilder.task().assign({tgSceneEvt, tgSpawnReq, tgSpawnEntReq, tgGravityNew}).data(
            "Add gravity to spawned shapes",
            TopDataIds_t{                    idSpawner,                   idSpawnerEnts,          idGravity,                   idActiveIds },
            wrap_args([] (SpawnerVec_t const& rSpawner, EntVector_t const& rSpawnerEnts, EntSet_t& rGravity, ActiveReg_t const& rActiveIds) noexcept
    {
        rGravity.ints().resize(rActiveIds.vec().capacity());

        for (std::size_t i = 0; i < rSpawner.size(); ++i)
        {
            SpawnShape const &spawn = rSpawner[i];
            if (spawn.m_mass == 0)
            {
                continue;
            }

            ActiveEnt const root    = rSpawnerEnts[i * 2];

            rGravity.set(std::size_t(root));
        }
    }));

    gravity.task() = rBuilder.task().assign({tgSceneEvt, tgDelTotalReq, tgGravityDel}).data(
            "Delete gravity components",
            TopDataIds_t{idGravity, idDelTotal}, delete_ent_set);

    return gravity;
}

Session setup_bounds(
        Builder_t&                  rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              scnCommon,
        Session const&              physics,
        Session const&              shapeSpawn)
{
    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(physics,    TESTAPP_PHYSICS);
    OSP_SESSION_UNPACK_DATA(shapeSpawn, TESTAPP_SHAPE_SPAWN);
    OSP_SESSION_UNPACK_TAGS(shapeSpawn, TESTAPP_SHAPE_SPAWN);

    Session bounds;
    OSP_SESSION_ACQUIRE_DATA(bounds, topData, TESTAPP_BOUNDS);
    OSP_SESSION_ACQUIRE_TAGS(bounds, rTags, TESTAPP_BOUNDS);

    top_emplace< EntSet_t >       (topData, idBounds);

    rBuilder.tag(tgBoundsDel)       .depend_on({tgBoundsReq});
    rBuilder.tag(tgBoundsNew)       .depend_on({tgBoundsReq, tgBoundsDel});

    bounds.task() = rBuilder.task().assign({tgSceneEvt, tgPhysReq, tgBoundsReq, tgDelEntMod}).data(
            "Bounds check",
            TopDataIds_t{                 idBasic,                idBounds,             idDelEnts   },
            wrap_args([] (ACtxBasic const& rBasic, EntSet_t const& rBounds, EntVector_t& rDelEnts) noexcept
    {
        for (std::size_t const entInt : rBounds.ones())
        {
            auto const ent = ActiveEnt(entInt);
            ACompTransform const &entTf = rBasic.m_transform.get(ent);
            if (entTf.m_transform.translation().z() < -10)
            {
                rDelEnts.push_back(ent);
            }
        }
    }));

    bounds.task() = rBuilder.task().assign({tgSceneEvt, tgSpawnReq, tgBoundsNew}).data(
            "Add bounds to spawned shapes",
            TopDataIds_t{                    idSpawner,                   idSpawnerEnts,          idBounds,                   idActiveIds },
            wrap_args([] (SpawnerVec_t const& rSpawner, EntVector_t const& rSpawnerEnts, EntSet_t& rBounds, ActiveReg_t const& rActiveIds) noexcept
    {
        rBounds.ints().resize(rActiveIds.vec().capacity());

        for (std::size_t i = 0; i < rSpawner.size(); ++i)
        {
            SpawnShape const &spawn = rSpawner[i];
            if (spawn.m_mass == 0)
            {
                continue;
            }

            ActiveEnt const root    = rSpawnerEnts[i * 2];

            rBounds.set(std::size_t(root));
        }
    }));

    bounds.task() = rBuilder.task().assign({tgSceneEvt, tgDelTotalReq, tgBoundsDel}).data(
            "Delete bounds components",
            TopDataIds_t{idBounds, idDelTotal}, delete_ent_set);

    return bounds;
}


} // namespace testapp::scenes


