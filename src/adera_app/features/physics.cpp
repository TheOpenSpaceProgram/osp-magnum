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
#include "physics.h"
#include "common.h"

#include <osp/activescene/basic.h>
#include <osp/activescene/physics_fn.h>
#include <osp/activescene/prefab_fn.h>
#include <osp/core/Resources.h>
#include <osp/drawing/drawing_fn.h>
#include <osp/drawing/prefab_draw.h>
#include <osp/vehicles/ImporterData.h>

#include <Magnum/Trade/Trade.h>
#include <Magnum/Trade/PbrMetallicRoughnessMaterialData.h>


using namespace osp;
using namespace osp::active;
using namespace osp::draw;
using osp::restypes::gc_importer;
using Corrade::Containers::arrayView;

namespace adera
{



Session setup_physics(
        TopTaskBuilder&             rFB,
        ArrayView<entt::any> const  topData,
        Session const&              scene,
        Session const&              commonScene)
{
    OSP_DECLARE_GET_DATA_IDS(commonScene,  TESTAPP_DATA_COMMON_SCENE);
    auto const scene.pl = scene      .get_pipelines<PlScene>();
    auto const commonScene.pl  = commonScene.get_pipelines<PlCommonScene>();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_PHYSICS);
    auto const tgPhy = out.create_pipelines<PlPhysics>(rFB);

    rFB.pipeline(tgPhy.physBody)  .parent(scene.pl.update);
    rFB.pipeline(tgPhy.physUpdate).parent(scene.pl.update);

    rFB.data_emplace< ACtxPhysics >  (topData, idPhys);

    rFB.task()
        .name       ("Delete Physics components")
        .run_on     ({commonScene.di.activeEntDel(UseOrRun)})
        .sync_with  ({tgPhy.physBody(Delete)})
        .push_to    (out.m_tasks)
        .args       ({        idPhys,                      commonScene.di.activeEntDel })
        .func([] (ACtxPhysics& rPhys, ActiveEntVec_t const& rActiveEntDel) noexcept
    {
        SysPhysics::update_delete_phys(rPhys, rActiveEntDel.cbegin(), rActiveEntDel.cend());
    });

    return out;
} // setup_physics


//-----------------------------------------------------------------------------


Session setup_prefabs(
        TopTaskBuilder&             rFB,
        ArrayView<entt::any> const  topData,
        Session const&              application,
        Session const&              scene,
        Session const&              commonScene,
        Session const&              physics)
{
    OSP_DECLARE_GET_DATA_IDS(application,   TESTAPP_DATA_APPLICATION);
    OSP_DECLARE_GET_DATA_IDS(commonScene,   TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(physics,       TESTAPP_DATA_PHYSICS);
    auto const scene.pl    = scene         .get_pipelines<PlScene>();
    auto const commonScene.pl     = commonScene   .get_pipelines<PlCommonScene>();
    auto const tgPhy    = physics       .get_pipelines<PlPhysics>();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData,  TESTAPP_DATA_PREFABS);
    auto const tgPf = out.create_pipelines<PlPrefabs>(rFB);

    rFB.pipeline(tgPf.spawnRequest).parent(scene.pl.update);
    rFB.pipeline(tgPf.spawnedEnts) .parent(scene.pl.update);
    rFB.pipeline(tgPf.ownedEnts)   .parent(scene.pl.update);
    rFB.pipeline(tgPf.instanceInfo).parent(scene.pl.update);
    rFB.pipeline(tgPf.inSubtree)   .parent(scene.pl.update);

    rFB.data_emplace< ACtxPrefabs > (topData, idPrefabs);

    rFB.task()
        .name       ("Schedule Prefab spawn")
        .schedules  ({tgPf.spawnRequest(Schedule_)})
        .sync_with  ({scene.pl.update(Run)})
        .push_to    (out.m_tasks)
        .args       ({              idPrefabs })
        .func([] (ACtxPrefabs const& rPrefabs) noexcept -> TaskActions
    {
        return rPrefabs.spawnRequest.empty() ? TaskAction::Cancel : TaskActions{};
    });

    rFB.task()
        .name       ("Create Prefab entities")
        .run_on     ({tgPf.spawnRequest(UseOrRun)})
        .sync_with  ({commonScene.pl.activeEnt(New), commonScene.pl.activeEntResized(Schedule), tgPf.spawnedEnts(Resize)})
        .push_to    (out.m_tasks)
        .args       ({        idPrefabs,           commonScene.di.basic,           mainApp.di.resources})
        .func([] (ACtxPrefabs& rPrefabs, ACtxBasic& rBasic, Resources& rResources) noexcept
    {
        SysPrefabInit::create_activeents(rPrefabs, rBasic, rResources);
    });

    rFB.task()
        .name       ("Init Prefab transforms")
        .run_on     ({tgPf.spawnRequest(UseOrRun)})
        .sync_with  ({tgPf.spawnedEnts(UseOrRun), commonScene.pl.transform(New)})
        .push_to    (out.m_tasks)
        .args       ({      commonScene.di.basic,           mainApp.di.resources,             idPrefabs})
        .func([] (ACtxBasic& rBasic, Resources& rResources, ACtxPrefabs& rPrefabs) noexcept
    {
        SysPrefabInit::init_transforms(rPrefabs, rResources, rBasic.m_transform);
    });

    rFB.task()
        .name       ("Init Prefab instance info")
        .run_on     ({tgPf.spawnRequest(UseOrRun)})
        .sync_with  ({tgPf.spawnedEnts(UseOrRun), tgPf.instanceInfo(Modify)})
        .push_to    (out.m_tasks)
        .args       ({      commonScene.di.basic,           mainApp.di.resources,             idPrefabs})
        .func([] (ACtxBasic& rBasic, Resources& rResources, ACtxPrefabs& rPrefabs) noexcept
    {
        rPrefabs.instanceInfo.resize(rBasic.m_activeIds.capacity(), PrefabInstanceInfo{.prefab = lgrn::id_null<PrefabId>()});
        rPrefabs.roots.resize(rBasic.m_activeIds.capacity());
        SysPrefabInit::init_info(rPrefabs, rResources);
    });

    rFB.task()
        .name       ("Init Prefab physics")
        .run_on     ({tgPf.spawnRequest(UseOrRun)})
        .sync_with  ({tgPf.spawnedEnts(UseOrRun), tgPhy.physBody(Modify), tgPhy.physUpdate(Done)})
        .push_to    (out.m_tasks)
        .args       ({      commonScene.di.basic,           mainApp.di.resources,             idPhys,             idPrefabs})
        .func([] (ACtxBasic& rBasic, Resources& rResources, ACtxPhysics& rPhys, ACtxPrefabs& rPrefabs) noexcept
    {
        rPhys.m_hasColliders.resize(rBasic.m_activeIds.capacity());
        //rPhys.m_massDirty.resize(rActiveIds.capacity());
        rPhys.m_shape.resize(rBasic.m_activeIds.capacity());
        SysPrefabInit::init_physics(rPrefabs, rResources, rPhys);
    });

    rFB.task()
        .name       ("Clear Prefab vector")
        .run_on     ({tgPf.spawnRequest(Clear)})
        .push_to    (out.m_tasks)
        .args       ({        idPrefabs})
        .func([] (ACtxPrefabs& rPrefabs) noexcept
    {
        rPrefabs.spawnRequest.clear();
    });

    return out;
} // setup_prefabs




Session setup_prefab_draw(
        TopTaskBuilder&             rFB,
        ArrayView<entt::any> const  topData,
        Session const&              application,
        Session const&              windowApp,
        Session const&              sceneRenderer,
        Session const&              commonScene,
        Session const&              prefabs,
        MaterialId                  material)
{
    OSP_DECLARE_GET_DATA_IDS(application,   TESTAPP_DATA_APPLICATION);
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer, TESTAPP_DATA_SCENE_RENDERER);
    OSP_DECLARE_GET_DATA_IDS(commonScene,   TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(prefabs,       TESTAPP_DATA_PREFABS);
    auto const windowApp.pl    = windowApp     .get_pipelines< PlWindowApp >();
    auto const scene.plRdr = sceneRenderer .get_pipelines< PlSceneRenderer >();
    auto const commonScene.pl     = commonScene   .get_pipelines< PlCommonScene >();
    auto const tgPf     = prefabs       .get_pipelines< PlPrefabs >();

    Session out;
    auto const [idMaterial] = out.acquire_data<1>(topData);

    rFB.data_emplace<MaterialId>(topData, idMaterial, material);

    rFB.task()
        .name       ("Create DrawEnts for prefabs")
        .run_on     ({tgPf.spawnRequest(UseOrRun)})
        .sync_with  ({tgPf.spawnedEnts(UseOrRun), commonScene.pl.activeEntResized(Done), scene.plRdr.drawEntResized(ModifyOrSignal)})
        .push_to    (out.m_tasks)
        .args       ({           idPrefabs,           mainApp.di.resources,                 commonScene.di.basic,             commonScene.di.drawing,                 idScnRender})
        .func([]    (ACtxPrefabs& rPrefabs, Resources& rResources, ACtxBasic const& rBasic, ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender) noexcept
    {
        SysPrefabDraw::init_drawents(rPrefabs, rResources, rBasic, rDrawing, rScnRender);
    });

    rFB.task()
        .name       ("Add mesh and material to prefabs")
        .run_on     ({tgPf.spawnRequest(UseOrRun)})
        .sync_with  ({tgPf.spawnedEnts(UseOrRun), scene.plRdr.drawEnt(New), scene.plRdr.drawEntResized(Done),
                      scene.plRdr   .entMesh(New), scene.plRdr   .entMeshDirty(Modify_), scene.plRdr   .meshResDirty(Modify_),
                      scene.plRdr.entTexture(New), scene.plRdr.entTextureDirty(Modify_), scene.plRdr.textureResDirty(Modify_),
                      scene.plRdr  .material(New), scene.plRdr  .materialDirty(Modify_)})
        .push_to    (out.m_tasks)
        .args       ({        idPrefabs,           mainApp.di.resources,                 commonScene.di.basic,             commonScene.di.drawing,                commonScene.di.drawingRes,                 idScnRender,          idMaterial})
        .func([] (ACtxPrefabs& rPrefabs, Resources& rResources, ACtxBasic const& rBasic, ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, ACtxSceneRender& rScnRender, MaterialId material) noexcept
    {
        SysPrefabDraw::init_mesh_texture_material(rPrefabs, rResources, rBasic, rDrawing, rDrawingRes, rScnRender, material);
    });


    rFB.task()
        .name       ("Resync spawned shapes DrawEnts")
        .run_on     ({windowApp.pl.resync(Run)})
        .sync_with  ({tgPf.ownedEnts(UseOrRun_), commonScene.pl.hierarchy(Ready), commonScene.pl.activeEntResized(Done), scene.plRdr.drawEntResized(ModifyOrSignal)})
        .push_to    (out.m_tasks)
        .args       ({           idPrefabs,           mainApp.di.resources,                 commonScene.di.basic,             commonScene.di.drawing,                 idScnRender})
        .func([]    (ACtxPrefabs& rPrefabs, Resources& rResources, ACtxBasic const& rBasic, ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender) noexcept
    {
        SysPrefabDraw::resync_drawents(rPrefabs, rResources, rBasic, rDrawing, rScnRender);
    });

    rFB.task()
        .name       ("Resync spawned shapes mesh and material")
        .run_on     ({windowApp.pl.resync(Run)})
        .sync_with  ({tgPf.ownedEnts(UseOrRun_), scene.plRdr.entMesh(New), scene.plRdr.material(New), scene.plRdr.drawEnt(New), scene.plRdr.drawEntResized(Done),
                      scene.plRdr.materialDirty(Modify_), scene.plRdr.entMeshDirty(Modify_)})
        .push_to    (out.m_tasks)
        .args       ({        idPrefabs,           mainApp.di.resources,                 commonScene.di.basic,             commonScene.di.drawing,                commonScene.di.drawingRes,                 idScnRender,          idMaterial})
        .func([] (ACtxPrefabs& rPrefabs, Resources& rResources, ACtxBasic const& rBasic, ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, ACtxSceneRender& rScnRender, MaterialId material) noexcept
    {
        SysPrefabDraw::resync_mesh_texture_material(rPrefabs, rResources, rBasic, rDrawing, rDrawingRes, rScnRender, material);
    });


    return out;
} // setup_phys_shapes_draw


} // namespace adera
#endif
