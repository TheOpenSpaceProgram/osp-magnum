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

#include <osp/activescene/basic.h>
#include <osp/activescene/physics_fn.h>
#include <osp/activescene/prefab_fn.h>
#include <osp/core/Resources.h>
#include <osp/core/Resources.h>
#include <osp/drawing/drawing_fn.h>
#include <osp/vehicles/ImporterData.h>

using namespace osp;
using namespace osp::active;
using namespace osp::draw;
using osp::restypes::gc_importer;
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

    rBuilder.task()
        .name       ("Schedule Shape spawn")
        .schedules  ({tgShSp.spawnRequest(Schedule_)})
        .push_to    (out.m_tasks)
        .args       ({             idSpawner })
        .func([] (ACtxShapeSpawner& rSpawner) noexcept -> TaskActions
    {
        return rSpawner.m_spawnRequest.empty() ? TaskAction::Cancel : TaskActions{};
    });
    rBuilder.pipeline(tgShSp.ownedEnts)     .parent(tgScn.update);

    top_emplace< ACtxShapeSpawner > (topData, idSpawner, ACtxShapeSpawner{ .m_materialId = materialId });
    rBuilder.task()
        .name       ("Create ActiveEnts for requested shapes to spawn")
        .run_on     ({tgShSp.spawnRequest(UseOrRun)})
        .sync_with  ({tgCS.activeEnt(New), tgCS.activeEntResized(Schedule), tgShSp.spawnedEnts(Resize)})
        .push_to    (out.m_tasks)
        .args       ({      idBasic,                  idSpawner})
        .func([] (ACtxBasic& rBasic, ACtxShapeSpawner& rSpawner) noexcept
    {
        LGRN_ASSERTM(!rSpawner.m_spawnRequest.empty(), "spawnRequest Use_ shouldn't run if rSpawner.m_spawnRequest is empty!");

        rSpawner.m_ents.resize(rSpawner.m_spawnRequest.size() * 2);
        rBasic.m_activeIds.create(rSpawner.m_ents.begin(), rSpawner.m_ents.end());
    });

    rBuilder.task()
        .name       ("Add hierarchy and transform to spawned shapes")
        .run_on     ({tgShSp.spawnRequest(UseOrRun)})
        .sync_with  ({tgShSp.spawnedEnts(UseOrRun), tgShSp.ownedEnts(Modify__), tgCS.hierarchy(New), tgCS.transform(New)})
        .push_to    (out.m_tasks)
        .args       ({      idBasic,                  idSpawner })
        .func([] (ACtxBasic& rBasic, ACtxShapeSpawner& rSpawner) noexcept
    {
        osp::bitvector_resize(rSpawner.m_ownedEnts, rBasic.m_activeIds.capacity());
        rBasic.m_scnGraph.resize(rBasic.m_activeIds.capacity());

        SubtreeBuilder bldScnRoot = SysSceneGraph::add_descendants(rBasic.m_scnGraph, rSpawner.m_spawnRequest.size() * 2);

        for (std::size_t i = 0; i < rSpawner.m_spawnRequest.size(); ++i)
        {
            SpawnShape const &spawn = rSpawner.m_spawnRequest[i];
            ActiveEnt const root    = rSpawner.m_ents[i * 2];
            ActiveEnt const child   = rSpawner.m_ents[i * 2 + 1];

            rSpawner.m_ownedEnts.set(std::size_t(root));

            rBasic.m_transform.emplace(root, ACompTransform{osp::Matrix4::translation(spawn.m_position)});
            rBasic.m_transform.emplace(child, ACompTransform{Matrix4::scaling(spawn.m_size)});
            SubtreeBuilder bldRoot = bldScnRoot.add_child(root, 1);
            bldRoot.add_child(child);
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
                Vector3 const inertia = collider_inertia_tensor(spawn.m_shape, spawn.m_size, spawn.m_mass);
                Vector3 const offset{0.0f, 0.0f, 0.0f};
                rPhys.m_mass.emplace( child, ACompMass{ inertia, offset, spawn.m_mass } );
            }

            rPhys.m_shape[child] = spawn.m_shape;
            rPhys.m_colliderDirty.push_back(child);
        }
    });

    //TODO
    rBuilder.task()
        .name       ("Delete basic components")
        .run_on     ({tgCS.activeEntDelete(UseOrRun)})
        .sync_with  ({tgShSp.ownedEnts(Modify__)})
        .push_to    (out.m_tasks)
        .args       ({      idBasic,                      idActiveEntDel })
        .func([] (ACtxBasic& rBasic, ActiveEntVec_t const& rActiveEntDel) noexcept
    {
        update_delete_basic(rBasic, rActiveEntDel.cbegin(), rActiveEntDel.cend());
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

Session setup_shape_spawn_draw(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              windowApp,
        Session const&              sceneRenderer,
        Session const&              commonScene,
        Session const&              physics,
        Session const&              shapeSpawn)
{
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer, TESTAPP_DATA_SCENE_RENDERER);
    OSP_DECLARE_GET_DATA_IDS(commonScene,   TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(physics,       TESTAPP_DATA_PHYSICS);
    OSP_DECLARE_GET_DATA_IDS(shapeSpawn,    TESTAPP_DATA_SHAPE_SPAWN);
    auto const tgWin    = windowApp     .get_pipelines< PlWindowApp >();
    auto const tgScnRdr = sceneRenderer .get_pipelines< PlSceneRenderer >();
    auto const tgCS     = commonScene   .get_pipelines< PlCommonScene >();
    auto const tgShSp   = shapeSpawn    .get_pipelines< PlShapeSpawn >();

    Session out;

    rBuilder.task()
        .name       ("Create DrawEnts for spawned shapes")
        .run_on     ({tgShSp.spawnRequest(UseOrRun)})
        .sync_with  ({tgShSp.spawnedEnts(UseOrRun), tgCS.activeEntResized(Done), tgScnRdr.drawEntResized(ModifyOrSignal)})
        .push_to    (out.m_tasks)
        .args       ({               idBasic,             idDrawing,                 idScnRender,                  idSpawner,             idNMesh })
        .func([]    (ACtxBasic const& rBasic, ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, ACtxShapeSpawner& rSpawner, NamedMeshes& rNMesh) noexcept
    {
        for (std::size_t i = 0; i < rSpawner.m_spawnRequest.size(); ++i)
        {
            ActiveEnt const child            = rSpawner.m_ents[i * 2 + 1];
            rScnRender.m_activeToDraw[child] = rScnRender.m_drawIds.create();
        }
    });

    rBuilder.task()
        .name       ("Add mesh and material to spawned shapes")
        .run_on     ({tgShSp.spawnRequest(UseOrRun)})
        .sync_with  ({tgShSp.spawnedEnts(UseOrRun),
                      tgScnRdr.entMesh(New), tgScnRdr.material(New), tgScnRdr.drawEnt(New), tgScnRdr.drawEntResized(Done),
                      tgScnRdr.materialDirty(Modify_), tgScnRdr.entMeshDirty(Modify_)})
        .push_to    (out.m_tasks)
        .args       ({            idBasic,             idDrawing,                 idScnRender,                  idSpawner,             idNMesh })
        .func([] (ACtxBasic const& rBasic, ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, ACtxShapeSpawner& rSpawner, NamedMeshes& rNMesh) noexcept
    {
        Material &rMat = rScnRender.m_materials[rSpawner.m_materialId];

        for (std::size_t i = 0; i < rSpawner.m_spawnRequest.size(); ++i)
        {
            SpawnShape const &spawn = rSpawner.m_spawnRequest[i];
            ActiveEnt const root    = rSpawner.m_ents[i * 2];
            ActiveEnt const child   = rSpawner.m_ents[i * 2 + 1];
            DrawEnt const drawEnt   = rScnRender.m_activeToDraw[child];

            rScnRender.m_needDrawTf.set(std::size_t(root));
            rScnRender.m_needDrawTf.set(std::size_t(child));

            rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(rNMesh.m_shapeToMesh.at(spawn.m_shape));
            rScnRender.m_meshDirty.push_back(drawEnt);

            rMat.m_ents.set(std::size_t(drawEnt));
            rMat.m_dirty.push_back(drawEnt);

            rScnRender.m_visible.set(std::size_t(drawEnt));
            rScnRender.m_opaque.set(std::size_t(drawEnt));
        }
    });

    // When does resync run relative to deletes?

    rBuilder.task()
        .name       ("Resync spawned shapes DrawEnts")
        .run_on     ({tgWin.resync(Run)})
        .sync_with  ({tgShSp.ownedEnts(UseOrRun_), tgCS.hierarchy(Ready), tgCS.activeEntResized(Done), tgScnRdr.drawEntResized(ModifyOrSignal)})
        .push_to    (out.m_tasks)
        .args       ({               idBasic,             idDrawing,                 idScnRender,                  idSpawner,             idNMesh })
        .func([]    (ACtxBasic const& rBasic, ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, ACtxShapeSpawner& rSpawner, NamedMeshes& rNMesh) noexcept
    {
        for (std::size_t entInt : rSpawner.m_ownedEnts.ones())
        {
            ActiveEnt const root = ActiveEnt(entInt);
            ActiveEnt const child = *SysSceneGraph::children(rBasic.m_scnGraph, root).begin();

            rScnRender.m_activeToDraw[child] = rScnRender.m_drawIds.create();
        }
    });

    rBuilder.task()
        .name       ("Resync spawned shapes mesh and material")
        .run_on     ({tgWin.resync(Run)})
        .sync_with  ({tgShSp.ownedEnts(UseOrRun_), tgScnRdr.entMesh(New), tgScnRdr.material(New), tgScnRdr.drawEnt(New), tgScnRdr.drawEntResized(Done),
                      tgScnRdr.materialDirty(Modify_), tgScnRdr.entMeshDirty(Modify_)})
        .push_to    (out.m_tasks)
        .args       ({            idBasic,             idDrawing,             idPhys,                  idSpawner,                 idScnRender,             idNMesh })
        .func([] (ACtxBasic const& rBasic, ACtxDrawing& rDrawing, ACtxPhysics& rPhys, ACtxShapeSpawner& rSpawner, ACtxSceneRender& rScnRender, NamedMeshes& rNMesh) noexcept
    {
        Material &rMat = rScnRender.m_materials[rSpawner.m_materialId];

        for (std::size_t entInt : rSpawner.m_ownedEnts.ones())
        {
            ActiveEnt const root = ActiveEnt(entInt);
            ActiveEnt const child = *SysSceneGraph::children(rBasic.m_scnGraph, root).begin();

            //SpawnShape const &spawn = rSpawner.m_spawnRequest[i];
            DrawEnt const drawEnt   = rScnRender.m_activeToDraw[child];

            rScnRender.m_needDrawTf.set(std::size_t(root));
            rScnRender.m_needDrawTf.set(std::size_t(child));

            EShape const shape = rPhys.m_shape.at(child);
            rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(rNMesh.m_shapeToMesh.at(shape));
            rScnRender.m_meshDirty.push_back(drawEnt);

            rMat.m_ents.set(std::size_t(drawEnt));
            rMat.m_dirty.push_back(drawEnt);

            rScnRender.m_visible.set(std::size_t(drawEnt));
            rScnRender.m_opaque.set(std::size_t(drawEnt));
        }
    });

    rBuilder.task()
        .name       ("Remove deleted ActiveEnts from ACtxShapeSpawner")
        .run_on     ({tgCS.activeEntDelete(UseOrRun)})
        .sync_with  ({tgShSp.ownedEnts(Modify__)})
        .push_to    (out.m_tasks)
        .args       ({             idSpawner,                      idActiveEntDel })
        .func([] (ACtxShapeSpawner& rSpawner, ActiveEntVec_t const& rActiveEntDel) noexcept
    {
        for (ActiveEnt const deleted : rActiveEntDel)
        {
            rSpawner.m_ownedEnts.reset(std::size_t(deleted));
        }
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

#endif

} // namespace testapp::scenes
