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
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              scene,
        Session const&              commonScene)
{
    OSP_DECLARE_GET_DATA_IDS(commonScene,  TESTAPP_DATA_COMMON_SCENE);
    auto const tgScn = scene      .get_pipelines<PlScene>();
    auto const tgCS  = commonScene.get_pipelines<PlCommonScene>();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_PHYSICS);
    auto const tgPhy = out.create_pipelines<PlPhysics>(rBuilder);

    rBuilder.pipeline(tgPhy.physBody)  .parent(tgScn.update);
    rBuilder.pipeline(tgPhy.physUpdate).parent(tgScn.update);

    top_emplace< ACtxPhysics >  (topData, idPhys);

    rBuilder.task()
        .name       ("Delete Physics components")
        .run_on     ({tgCS.activeEntDelete(UseOrRun)})
        .sync_with  ({tgPhy.physBody(Delete)})
        .push_to    (out.m_tasks)
        .args       ({        idPhys,                      idActiveEntDel })
        .func([] (ACtxPhysics& rPhys, ActiveEntVec_t const& rActiveEntDel) noexcept
    {
        SysPhysics::update_delete_phys(rPhys, rActiveEntDel.cbegin(), rActiveEntDel.cend());
    });

    return out;
}

Session setup_shape_spawn(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              scene,
        Session const&              commonScene,
        Session const&              physics,
        MaterialId const            materialId)
{
    OSP_DECLARE_GET_DATA_IDS(commonScene,   TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(physics,       TESTAPP_DATA_PHYSICS);
    auto const tgScn    = scene         .get_pipelines<PlScene>();
    auto const tgCS     = commonScene   .get_pipelines<PlCommonScene>();
    auto const tgPhy    = physics       .get_pipelines<PlPhysics>();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_SHAPE_SPAWN);
    auto const tgShSp = out.create_pipelines<PlShapeSpawn>(rBuilder);

    rBuilder.pipeline(tgShSp.spawnRequest)  .parent(tgScn.update);
    rBuilder.pipeline(tgShSp.spawnedEnts)   .parent(tgScn.update);

    top_emplace< ACtxShapeSpawner > (topData, idSpawner, ACtxShapeSpawner{ .m_materialId = materialId });
    rBuilder.task()
        .name       ("Create entities for requested shapes to spawn")
        .run_on     ({tgShSp.spawnRequest(UseOrRun)})
        .sync_with  ({tgCS.activeEnt(New), tgShSp.spawnedEnts(Resize)})
        .push_to    (out.m_tasks)
        .args       ({      idBasic,                  idSpawner })
        .func([] (ACtxBasic& rBasic, ACtxShapeSpawner& rSpawner) noexcept
    {
        //LGRN_ASSERTM(!rSpawner.m_spawnRequest.empty(), "spawnRequest Use_ shouldn't run if rSpawner.m_spawnRequest is empty!");

        rSpawner.m_ents.resize(rSpawner.m_spawnRequest.size() * 2);
        rBasic.m_activeIds.create(rSpawner.m_ents.begin(), rSpawner.m_ents.end());
    });

    rBuilder.task()
        .name       ("Add hierarchy and transform to spawned shapes")
        .run_on     ({tgShSp.spawnRequest(UseOrRun)})
        .sync_with  ({tgShSp.spawnedEnts(UseOrRun), tgCS.hierarchy(New), tgCS.transform(New)})
        .push_to    (out.m_tasks)
        .args       ({      idBasic,                  idSpawner })
        .func([] (ACtxBasic& rBasic, ACtxShapeSpawner& rSpawner) noexcept
    {
        rBasic.m_scnGraph.resize(rBasic.m_activeIds.capacity());
        SubtreeBuilder bldScnRoot = SysSceneGraph::add_descendants(rBasic.m_scnGraph, rSpawner.m_spawnRequest.size() * 2);

        for (std::size_t i = 0; i < rSpawner.m_spawnRequest.size(); ++i)
        {
            SpawnShape const &spawn = rSpawner.m_spawnRequest[i];
            ActiveEnt const root    = rSpawner.m_ents[i * 2];
            ActiveEnt const child   = rSpawner.m_ents[i * 2 + 1];

            rBasic.m_transform.emplace(root, ACompTransform{osp::Matrix4::translation(spawn.m_position)});
            rBasic.m_transform.emplace(child, ACompTransform{Matrix4::scaling(spawn.m_size)});
            SubtreeBuilder bldRoot = bldScnRoot.add_child(root, 1);
            bldRoot.add_child(child);
        }
    });

    rBuilder.task()
        .name       ("Add mesh and material to spawned shapes")
        .run_on     ({tgShSp.spawnRequest(UseOrRun)})
        .sync_with  ({tgShSp.spawnedEnts(UseOrRun), tgCS.mesh(New), tgCS.material(New), tgCS.drawEnt(New), tgCS.drawEntResized(ModifyOrSignal), tgCS.materialDirty(Modify_)})
        .push_to    (out.m_tasks)
        .args       ({            idBasic,             idDrawing,                  idSpawner,             idNMesh })
        .func([] (ACtxBasic const& rBasic, ACtxDrawing& rDrawing, ACtxShapeSpawner& rSpawner, NamedMeshes& rNMesh) noexcept
    {
        Material &rMat = rDrawing.m_materials[rSpawner.m_materialId];

        rDrawing.resize_active(rBasic.m_activeIds.capacity());

        for (std::size_t i = 0; i < rSpawner.m_spawnRequest.size(); ++i)
        {
            ActiveEnt const child           = rSpawner.m_ents[i * 2 + 1];
            rDrawing.m_activeToDraw[child]  = rDrawing.m_drawIds.create();
        }

        rDrawing.resize_draw();
        bitvector_resize(rMat.m_ents, rDrawing.m_drawIds.capacity());

        for (std::size_t i = 0; i < rSpawner.m_spawnRequest.size(); ++i)
        {
            SpawnShape const &spawn = rSpawner.m_spawnRequest[i];
            ActiveEnt const root    = rSpawner.m_ents[i * 2];
            ActiveEnt const child   = rSpawner.m_ents[i * 2 + 1];
            DrawEnt const drawEnt   = rDrawing.m_activeToDraw[child];

            rDrawing.m_needDrawTf.set(std::size_t(root));
            rDrawing.m_needDrawTf.set(std::size_t(child));

            rDrawing.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(rNMesh.m_shapeToMesh.at(spawn.m_shape));
            rDrawing.m_meshDirty.push_back(drawEnt);

            rMat.m_ents.set(std::size_t(drawEnt));
            rMat.m_dirty.push_back(drawEnt);

            rDrawing.m_visible.set(std::size_t(drawEnt));
            rDrawing.m_drawBasic[drawEnt].m_opaque = true;
        }
    });

    rBuilder.task()
        .name       ("Add physics to spawned shapes")
        .run_on     ({tgShSp.spawnRequest(UseOrRun)})
        .sync_with  ({tgShSp.spawnedEnts(UseOrRun), tgPhy.physBody(Modify), tgPhy.physUpdate(Done)})
        .push_to    (out.m_tasks)
        .args       ({            idBasic,                  idSpawner,             idPhys })
        .func([] (ACtxBasic const& rBasic, ACtxShapeSpawner& rSpawner, ACtxPhysics& rPhys) noexcept
    {
        rPhys.m_hasColliders.ints().resize(rBasic.m_activeIds.vec().capacity());
        rPhys.m_shape.resize(rBasic.m_activeIds.capacity());

        for (std::size_t i = 0; i < rSpawner.m_spawnRequest.size(); ++i)
        {
            SpawnShape const &spawn = rSpawner.m_spawnRequest[i];
            ActiveEnt const root    = rSpawner.m_ents[i * 2];
            ActiveEnt const child   = rSpawner.m_ents[i * 2 + 1];

            rPhys.m_hasColliders.set(std::size_t(root));
            if (spawn.m_mass != 0.0f)
            {
                rPhys.m_setVelocity.emplace_back(root, spawn.m_velocity);
                Vector3 const inertia
                        = phys::collider_inertia_tensor(spawn.m_shape, spawn.m_size, spawn.m_mass);
                Vector3 const offset{0.0f, 0.0f, 0.0f};
                rPhys.m_mass.emplace( child, ACompMass{ inertia, offset, spawn.m_mass } );
            }

            rPhys.m_shape[std::size_t(child)] = spawn.m_shape;
            rPhys.m_colliderDirty.push_back(child);
        }
    });


    rBuilder.task()
        .name       ("Clear Shape Spawning vector after use")
        .run_on     ({tgShSp.spawnRequest(Clear)})
        .push_to    (out.m_tasks)
        .args       ({             idSpawner })
        .func([] (ACtxShapeSpawner& rSpawner) noexcept
    {
        rSpawner.m_spawnRequest.clear();
    });


    return out;
}


#if 0

Session setup_prefabs(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              commonScene,
        Session const&              physics,
        Session const&              material,
        TopDataId const             idResources)
{
    OSP_SESSION_UNPACK_DATA(commonScene,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(commonScene,  TESTAPP_COMMON_SCENE);
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
        for (TmpPrefabInitBasic const& rPfBasic : rPrefabInit.m_basicIn)
        {
            auto const& rPrefabData = rResources.data_get<Prefabs>(gc_importer, rPfBasic.m_importerRes);
            auto const& objects     = rPrefabData.m_prefabs[rPfBasic.m_prefabId];

            totalEnts += objects.size();
        }

        // Create entities
        rPrefabInit.m_newEnts.resize(totalEnts);
        rActiveIds.create(std::begin(rPrefabInit.m_newEnts), std::end(rPrefabInit.m_newEnts));

        // Assign new entities to each prefab to create
        rPrefabInit.m_ents.resize(rPrefabInit.m_basicIn.size());
        auto itEntAvailable = std::begin(rPrefabInit.m_newEnts);
        auto itPfEntSpanOut = std::begin(rPrefabInit.m_ents);
        for (TmpPrefabInitBasic& rPfBasic : rPrefabInit.m_basicIn)
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

    prefabs.task() = rBuilder.task().assign({tgSceneEvt, tgPrefabReq, tgPrefabEntReq, PlransformNew}).data(
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
                TopDataIds_t{                idPrefabInit,           idResources,             idDrawing,                idDrawingRes,          idMatEnts,                      idMatDirty,                   idActiveIds },
                wrap_args([] (ACtxPrefabInit& rPrefabInit, Resources& rResources, ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, EntSet_t& rMatEnts, std::vector<DrawEnt>& rMatDirty, ActiveReg_t const& rActiveIds) noexcept
        {
            rDrawing.resize_active(rActiveIds.capacity());
            rMatEnts.ints().resize(rActiveIds.vec().capacity());
            SysPrefabInit::init_drawing(rPrefabInit, rResources, rDrawing, rDrawingRes, {{rMatEnts, rMatDirty}});
        }));
    }

    prefabs.task() = rBuilder.task().assign({tgSceneEvt, tgPrefabReq, tgPrefabEntReq, tgPhysMod}).data(
            "Init Prefab physics",
            TopDataIds_t{                   idActiveIds,                idPrefabInit,           idResources,             idPhys},
            wrap_args([] (ActiveReg_t const& rActiveIds, ACtxPrefabInit& rPrefabInit, Resources& rResources, ACtxPhysics& rPhys) noexcept
    {
        rPhys.m_hasColliders.ints().resize(rActiveIds.vec().capacity());
        //rPhys.m_massDirty.ints().resize(rActiveIds.vec().capacity());
        rPhys.m_shape.resize(rActiveIds.capacity());
        SysPrefabInit::init_physics(rPrefabInit, rResources, rPhys);
    }));


    prefabs.task() = rBuilder.task().assign({tgSceneEvt, tgPrefabClr}).data(
            "Clear Prefab vector",
            TopDataIds_t{                idPrefabInit },
            wrap_args([] (ACtxPrefabInit& rPrefabInit) noexcept
    {
        rPrefabInit.m_basicIn.clear();
    }));


    return prefabs;
}

Session setup_bounds(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Tags&                       rTags,
        Session const&              commonScene,
        Session const&              physics,
        Session const&              shapeSpawn)
{
    OSP_SESSION_UNPACK_TAGS(commonScene,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(commonScene,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(physics,    TESTAPP_PHYSICS);
    OSP_SESSION_UNPACK_DATA(shapeSpawn, TESTAPP_SHAPE_SPAWN);
    OSP_SESSION_UNPACK_TAGS(shapeSpawn, TESTAPP_SHAPE_SPAWN);

    Session bounds;
    OSP_SESSION_ACQUIRE_DATA(bounds, topData, TESTAPP_BOUNDS);
    OSP_SESSION_ACQUIRE_TAGS(bounds, rTags, TESTAPP_BOUNDS);

    top_emplace< EntSet_t >       (topData, idBounds);
    top_emplace< EntVector_t >    (topData, idOutOfBounds);

    rBuilder.tag(tgBoundsSetMod)    .depend_on({tgBoundsSetDel});
    rBuilder.tag(tgBoundsSetReq)    .depend_on({tgBoundsSetDel, tgBoundsSetMod});
    rBuilder.tag(tgOutOfBoundsMod)  .depend_on({tgOutOfBoundsPrv});

    // Bounds are checked are after transforms are final (PlransformReq)
    // Out-of-bounds entities are added to rOutOfBounds, and are deleted
    // at the start of next frame

    bounds.task() = rBuilder.task().assign({tgSceneEvt, PlransformReq, tgBoundsSetReq, tgOutOfBoundsMod}).data(
            "Check for out-of-bounds entities",
            TopDataIds_t{                 idBasic,                idBounds,             idOutOfBounds},
            wrap_args([] (ACtxBasic const& rBasic, EntSet_t const& rBounds, EntVector_t& rOutOfBounds) noexcept
    {
        rOutOfBounds.clear();
        for (std::size_t const ent : rBounds.ones())
        {
            ACompTransform const &entTf = rBasic.m_transform.get(ActiveEnt(ent));
            if (entTf.m_transform.translation().z() < -10)
            {
                rOutOfBounds.push_back(ActiveEnt(ent));
            }
        }
    }));


    bounds.task() = rBuilder.task().assign({tgSceneEvt, tgOutOfBoundsPrv, tgDelEntMod}).data(
            "Delete out-of-bounds entities",
            TopDataIds_t{                 idBasic,                   idOutOfBounds,             idDelEnts   },
            wrap_args([] (ACtxBasic const& rBasic, EntVector_t const& rOutOfBounds, EntVector_t& rDelEnts) noexcept
    {
        rDelEnts.insert(rDelEnts.end(), rOutOfBounds.cbegin(), rOutOfBounds.cend());
    }));

    bounds.task() = rBuilder.task().assign({tgSceneEvt, tgSpawnReq, tgBoundsSetMod}).data(
            "Add bounds to spawned shapes",
            TopDataIds_t{                    idSpawner,                   idSpawnerEnts,          idBounds,                   idActiveIds },
            wrap_args([] (SpawnerVec_t const& rSpawner, EntVector_t const& rSpawnerEnts, EntSet_t& rBounds, ActiveReg_t const& rActiveIds) noexcept
    {
        rBounds     .ints().resize(rActiveIds.vec().capacity());

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

    bounds.task() = rBuilder.task().assign({tgSceneEvt, tgDelTotalReq, tgBoundsSetDel}).data(
            "Delete bounds components",
            TopDataIds_t{idBounds, idDelTotal}, delete_ent_set);
//    for (osp::active::ActiveEnt const ent : rDelTotal)
//    {
//        rSet.reset(std::size_t(ent));
//    }

    return bounds;
}

#endif

} // namespace testapp::scenes


