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
#include "physics.h"

#include "../feature_interfaces.h"

#include <osp/activescene/basic.h>
#include <osp/activescene/physics_fn.h>
#include <osp/activescene/prefab_fn.h>
#include <osp/drawing/drawing_fn.h>
#include <osp/drawing/prefab_draw.h>
#include <osp/vehicles/ImporterData.h>
#include <osp/core/Resources.h>

#include <Magnum/Trade/Trade.h>
#include <Magnum/Trade/PbrMetallicRoughnessMaterialData.h>

using namespace ftr_inter::stages;
using namespace ftr_inter;
using namespace osp::active;
using namespace osp::draw;
using namespace osp::fw;
using namespace osp;

namespace adera
{

FeatureDef const ftrPhysics = feature_def("Physics", [] (
        FeatureBuilder          &rFB,
        Implement<FIPhysics>    phys,
        DependOn<FIScene>       scn,
        DependOn<FICommonScene> comScn)
{
    rFB.pipeline(phys.pl.physBody)  .parent(scn.pl.update);
    rFB.pipeline(phys.pl.physUpdate).parent(scn.pl.update);

    rFB.data_emplace< ACtxPhysics >  (phys.di.phys);

    rFB.task()
        .name       ("Delete Physics components")
        .run_on     ({comScn.pl.activeEntDelete(UseOrRun)})
        .sync_with  ({phys.pl.physBody(Delete)})
        .args       ({         phys.di.phys,              comScn.di.activeEntDel })
        .func       ([] (ACtxPhysics &rPhys, ActiveEntVec_t const &rActiveEntDel) noexcept
    {
        SysPhysics::update_delete_phys(rPhys, rActiveEntDel.cbegin(), rActiveEntDel.cend());
    });
}); // ftrPhysics


//-----------------------------------------------------------------------------


FeatureDef const ftrPrefabs = feature_def("Prefabs", [] (
        FeatureBuilder              &rFB,
        Implement<FIPrefabs>        prefabs,
        DependOn<FIMainApp>         mainApp,
        DependOn<FIScene>           scn,
        DependOn<FICommonScene>     comScn,
        DependOn<FIPhysics>         phys)
{
    rFB.pipeline(prefabs.pl.spawnRequest).parent(scn.pl.update);
    rFB.pipeline(prefabs.pl.spawnedEnts) .parent(scn.pl.update);
    rFB.pipeline(prefabs.pl.ownedEnts)   .parent(scn.pl.update);
    rFB.pipeline(prefabs.pl.instanceInfo).parent(scn.pl.update);
    rFB.pipeline(prefabs.pl.inSubtree)   .parent(scn.pl.update);

    rFB.data_emplace< ACtxPrefabs > (prefabs.di.prefabs);

    rFB.task()
        .name       ("Schedule Prefab spawn")
        .schedules  ({prefabs.pl.spawnRequest(Schedule_)})
        .sync_with  ({scn.pl.update(Run)})
        .args       ({            prefabs.di.prefabs })
        .func       ([] (ACtxPrefabs const &rPrefabs) noexcept -> TaskActions
    {
        return rPrefabs.spawnRequest.empty() ? TaskAction::Cancel : TaskActions{};
    });

    rFB.task()
        .name       ("Create Prefab entities")
        .run_on     ({prefabs.pl.spawnRequest(UseOrRun)})
        .sync_with  ({comScn.pl.activeEnt(New), comScn.pl.activeEntResized(Schedule), prefabs.pl.spawnedEnts(Resize)})
        .args       ({      prefabs.di.prefabs,   comScn.di.basic,  mainApp.di.resources})
        .func       ([] (ACtxPrefabs &rPrefabs, ACtxBasic &rBasic, Resources &rResources) noexcept
    {
        SysPrefabInit::create_activeents(rPrefabs, rBasic, rResources);
    });

    rFB.task()
        .name       ("Init Prefab transforms")
        .run_on     ({prefabs.pl.spawnRequest(UseOrRun)})
        .sync_with  ({prefabs.pl.spawnedEnts(UseOrRun), comScn.pl.transform(New)})
        .args       ({     comScn.di.basic,  mainApp.di.resources,    prefabs.di.prefabs})
        .func       ([] (ACtxBasic &rBasic, Resources &rResources, ACtxPrefabs &rPrefabs) noexcept
    {
        SysPrefabInit::init_transforms(rPrefabs, rResources, rBasic.m_transform);
    });

    rFB.task()
        .name       ("Init Prefab instance info")
        .run_on     ({prefabs.pl.spawnRequest(UseOrRun)})
        .sync_with  ({prefabs.pl.spawnedEnts(UseOrRun), prefabs.pl.instanceInfo(Modify)})
        .args       ({     comScn.di.basic,  mainApp.di.resources,    prefabs.di.prefabs})
        .func       ([] (ACtxBasic &rBasic, Resources &rResources, ACtxPrefabs &rPrefabs) noexcept
    {
        rPrefabs.instanceInfo.resize(rBasic.m_activeIds.capacity(), PrefabInstanceInfo{.prefab = lgrn::id_null<PrefabId>()});
        rPrefabs.roots.resize(rBasic.m_activeIds.capacity());
        SysPrefabInit::init_info(rPrefabs, rResources);
    });

    rFB.task()
        .name       ("Init Prefab physics")
        .run_on     ({prefabs.pl.spawnRequest(UseOrRun)})
        .sync_with  ({prefabs.pl.spawnedEnts(UseOrRun), phys.pl.physBody(Modify), phys.pl.physUpdate(Done)})
        .args       ({     comScn.di.basic,  mainApp.di.resources,       phys.di.phys,    prefabs.di.prefabs})
        .func       ([] (ACtxBasic &rBasic, Resources &rResources, ACtxPhysics &rPhys, ACtxPrefabs &rPrefabs) noexcept
    {
        rPhys.m_hasColliders.resize(rBasic.m_activeIds.capacity());
        //rPhys.m_massDirty.resize(rActiveIds.capacity());
        rPhys.m_shape.resize(rBasic.m_activeIds.capacity());
        SysPrefabInit::init_physics(rPrefabs, rResources, rPhys);
    });

    rFB.task()
        .name       ("Clear Prefab vector")
        .run_on     ({prefabs.pl.spawnRequest(Clear)})
        .args       ({        prefabs.di.prefabs})
        .func       ([] (ACtxPrefabs &rPrefabs) noexcept
    {
        rPrefabs.spawnRequest.clear();
    });
}); // ftrPrefabs



FeatureDef const ftrPrefabDraw = feature_def("PrefabDraw", [] (
        FeatureBuilder              &rFB,
        Implement<FIPrefabDraw>     prefabDraw,
        DependOn<FIMainApp>         mainApp,
        DependOn<FIScene>           scn,
        DependOn<FICommonScene>     comScn,
        DependOn<FIPhysics>         phys,
        DependOn<FIPrefabs>         prefabs,
        DependOn<FIWindowApp>       windowApp,
        DependOn<FISceneRenderer>   scnRender,
        entt::any                   userData)
{
    rFB.data_emplace<MaterialId>(prefabDraw.di.material, entt::any_cast<MaterialId>(userData));

    rFB.task()
        .name       ("Create DrawEnts for prefabs")
        .run_on     ({prefabs.pl.spawnRequest(UseOrRun)})
        .sync_with  ({prefabs.pl.spawnedEnts(UseOrRun), comScn.pl.activeEntResized(Done), scnRender.pl.drawEntResized(ModifyOrSignal)})
        .args       ({      prefabs.di.prefabs,  mainApp.di.resources,         comScn.di.basic,     comScn.di.drawing,      scnRender.di.scnRender})
        .func       ([] (ACtxPrefabs &rPrefabs, Resources &rResources, ACtxBasic const &rBasic, ACtxDrawing &rDrawing, ACtxSceneRender &rScnRender) noexcept
    {
        SysPrefabDraw::init_drawents(rPrefabs, rResources, rBasic, rDrawing, rScnRender);
    });

    rFB.task()
        .name       ("Add mesh and material to prefabs")
        .run_on     ({prefabs.pl.spawnRequest(UseOrRun)})
        .sync_with  ({prefabs.pl.spawnedEnts(UseOrRun), scnRender.pl.drawEnt(New), scnRender.pl.drawEntResized(Done),
                      scnRender.pl.entMesh   (New), scnRender.pl.entMeshDirty   (Modify_),  scnRender.pl.meshResDirty   (Modify_),
                      scnRender.pl.entTexture(New), scnRender.pl.entTextureDirty(Modify_),  scnRender.pl.textureResDirty(Modify_),
                      scnRender.pl.material  (New), scnRender.pl.materialDirty  (Modify_)})
        .args       ({      prefabs.di.prefabs,  mainApp.di.resources,         comScn.di.basic,     comScn.di.drawing,        comScn.di.drawingRes,      scnRender.di.scnRender, prefabDraw.di.material})
        .func       ([] (ACtxPrefabs &rPrefabs, Resources &rResources, ACtxBasic const &rBasic, ACtxDrawing &rDrawing, ACtxDrawingRes &rDrawingRes, ACtxSceneRender &rScnRender,    MaterialId material) noexcept
    {
        SysPrefabDraw::init_mesh_texture_material(rPrefabs, rResources, rBasic, rDrawing, rDrawingRes, rScnRender, material);
    });

    rFB.task()
        .name       ("Resync spawned shapes DrawEnts")
        .run_on     ({windowApp.pl.resync(Run)})
        .sync_with  ({prefabs.pl.ownedEnts(UseOrRun_), comScn.pl.hierarchy(Ready), comScn.pl.activeEntResized(Done), scnRender.pl.drawEntResized(ModifyOrSignal)})
        .args       ({      prefabs.di.prefabs,  mainApp.di.resources,         comScn.di.basic,     comScn.di.drawing,      scnRender.di.scnRender})
        .func       ([] (ACtxPrefabs &rPrefabs, Resources &rResources, ACtxBasic const &rBasic, ACtxDrawing &rDrawing, ACtxSceneRender &rScnRender) noexcept
    {
        SysPrefabDraw::resync_drawents(rPrefabs, rResources, rBasic, rDrawing, rScnRender);
    });

    rFB.task()
        .name       ("Resync spawned shapes mesh and material")
        .run_on     ({windowApp.pl.resync(Run)})
        .sync_with  ({prefabs.pl.ownedEnts(UseOrRun_), scnRender.pl.entMesh(New), scnRender.pl.material(New), scnRender.pl.drawEnt(New), scnRender.pl.drawEntResized(Done),
                      scnRender.pl.materialDirty(Modify_), scnRender.pl.entMeshDirty(Modify_)})
        .args       ({      prefabs.di.prefabs,  mainApp.di.resources,         comScn.di.basic,     comScn.di.drawing,        comScn.di.drawingRes,      scnRender.di.scnRender, prefabDraw.di.material})
        .func       ([] (ACtxPrefabs &rPrefabs, Resources &rResources, ACtxBasic const &rBasic, ACtxDrawing &rDrawing, ACtxDrawingRes &rDrawingRes, ACtxSceneRender &rScnRender,    MaterialId material) noexcept
    {
        SysPrefabDraw::resync_mesh_texture_material(rPrefabs, rResources, rBasic, rDrawing, rDrawingRes, rScnRender, material);
    });
}); // ftrPrefabDraw


} // namespace adera
