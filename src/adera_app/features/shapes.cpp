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
#include "shapes.h"

#include "../feature_interfaces.h"

#include <adera/drawing/CameraController.h>

#include <osp/activescene/basic.h>
#include <osp/activescene/physics_fn.h>
#include <osp/drawing/drawing_fn.h>
#include <osp/drawing/prefab_draw.h>

#include <random>

using namespace adera;
using namespace ftr_inter::stages;
using namespace ftr_inter;
using namespace osp::active;
using namespace osp::draw;
using namespace osp::fw;
using namespace osp;

using osp::input::EButtonControlIndex;

namespace adera
{

void add_floor(Framework &rFW, ContextId sceneCtx, PkgId pkg, int size)
{
    auto const physShapes = rFW.get_interface<FIPhysShapes>(sceneCtx);

    auto &rPhysShapes = rFW.data_get<ACtxPhysShapes>(physShapes.di.physShapes);

    std::mt19937 randGen(69);
    auto distSizeX  = std::uniform_real_distribution<float>{20.0, 80.0};
    auto distSizeY  = std::uniform_real_distribution<float>{20.0, 80.0};
    auto distHeight = std::uniform_real_distribution<float>{1.0, 10.0};

    constexpr float spread      = 128.0f;

    for (int x = -size; x < size+1; ++x)
    {
        for (int y = -size; y < size+1; ++y)
        {
            float const heightZ = distHeight(randGen);
            rPhysShapes.m_spawnRequest.emplace_back(SpawnShape{
                .m_position = Vector3{float(x)*spread, float(y)*spread, heightZ},
                .m_velocity = {0.0f, 0.0f, 0.0f},
                .m_size     = Vector3{distSizeX(randGen), distSizeY(randGen), heightZ},
                .m_mass     = 0.0f,
                .m_shape    = EShape::Box
            });
        }
    }
}


FeatureDef const ftrPhysicsShapes = feature_def("PhysicsShapes", [] (
        FeatureBuilder              &rFB,
        Implement<FIPhysShapes>     physShapes,
        DependOn<FIScene>           scn,
        DependOn<FICommonScene>     comScn,
        DependOn<FIPhysics>         phys,
        entt::any                   userData)
{
    rFB.pipeline(physShapes.pl.spawnRequest)  .parent(scn.pl.update);
    rFB.pipeline(physShapes.pl.spawnedEnts)   .parent(scn.pl.update);
    rFB.pipeline(physShapes.pl.ownedEnts)     .parent(scn.pl.update);

    rFB.data_emplace< ACtxPhysShapes > (physShapes.di.physShapes);

    // TODO: format after framework changes

    rFB.task()
        .name       ("Schedule Shape spawn")
        .schedules  ({physShapes.pl.spawnRequest(Schedule_)})
        .sync_with  ({scn.pl.update(Run)})
        .args       ({           physShapes.di.physShapes })
        .func([] (ACtxPhysShapes &rPhysShapes) noexcept -> TaskActions
    {
        return rPhysShapes.m_spawnRequest.empty() ? TaskAction::Cancel : TaskActions{};
    });

    rFB.task()
        .name       ("Create ActiveEnts for requested shapes to spawn")
        .run_on     ({physShapes.pl.spawnRequest(UseOrRun)})
        .sync_with  ({comScn.pl.activeEnt(New), comScn.pl.activeEntResized(Schedule), physShapes.pl.spawnedEnts(Resize)})
        .args       ({      comScn.di.basic,                physShapes.di.physShapes})
        .func([] (ACtxBasic &rBasic, ACtxPhysShapes &rPhysShapes) noexcept
    {
        LGRN_ASSERTM(!rPhysShapes.m_spawnRequest.empty(), "spawnRequest Use_ shouldn't run if rPhysShapes.m_spawnRequest is empty!");

        rPhysShapes.m_ents.resize(rPhysShapes.m_spawnRequest.size() * 2);
        rBasic.m_activeIds.create(rPhysShapes.m_ents.begin(), rPhysShapes.m_ents.end());
    });

    rFB.task()
        .name       ("Add hierarchy and transform to spawned shapes")
        .run_on     ({physShapes.pl.spawnRequest(UseOrRun)})
        .sync_with  ({physShapes.pl.spawnedEnts(UseOrRun), physShapes.pl.ownedEnts(Modify__), comScn.pl.hierarchy(New), comScn.pl.transform(New)})
        .args       ({      comScn.di.basic,                physShapes.di.physShapes })
        .func([] (ACtxBasic &rBasic, ACtxPhysShapes &rPhysShapes) noexcept
    {
        rPhysShapes.ownedEnts.resize(rBasic.m_activeIds.capacity());
        rBasic.m_scnGraph.resize(rBasic.m_activeIds.capacity());

        SubtreeBuilder bldScnRoot = SysSceneGraph::add_descendants(rBasic.m_scnGraph, rPhysShapes.m_spawnRequest.size() * 2);

        for (std::size_t i = 0; i < rPhysShapes.m_spawnRequest.size(); ++i)
        {
            SpawnShape const &spawn = rPhysShapes.m_spawnRequest[i];
            ActiveEnt const root    = rPhysShapes.m_ents[i * 2];
            ActiveEnt const child   = rPhysShapes.m_ents[i * 2 + 1];

            rPhysShapes.ownedEnts.insert(root);

            LGRN_ASSERT( ! rBasic.m_transform.contains(root) );
            LGRN_ASSERT( ! rBasic.m_transform.contains(child) );
            rBasic.m_transform.emplace(root, ACompTransform{osp::Matrix4::translation(spawn.m_position)});
            rBasic.m_transform.emplace(child, ACompTransform{Matrix4::scaling(spawn.m_size)});
            SubtreeBuilder bldRoot = bldScnRoot.add_child(root, 1);
            bldRoot.add_child(child);
        }
    });

    rFB.task()
        .name       ("Add physics to spawned shapes")
        .run_on     ({physShapes.pl.spawnRequest(UseOrRun)})
        .sync_with  ({physShapes.pl.spawnedEnts(UseOrRun), phys.pl.physBody(Modify), phys.pl.physUpdate(Done)})
        .args       ({            comScn.di.basic,                physShapes.di.physShapes,             phys.di.phys })
        .func([] (ACtxBasic const &rBasic, ACtxPhysShapes &rPhysShapes, ACtxPhysics &rPhys) noexcept
    {
        rPhys.m_hasColliders.resize(rBasic.m_activeIds.capacity());
        rPhys.m_shape.resize(rBasic.m_activeIds.capacity());

        for (std::size_t i = 0; i < rPhysShapes.m_spawnRequest.size(); ++i)
        {
            SpawnShape const &spawn = rPhysShapes.m_spawnRequest[i];
            ActiveEnt const root    = rPhysShapes.m_ents[i * 2];
            ActiveEnt const child   = rPhysShapes.m_ents[i * 2 + 1];

            rPhys.m_hasColliders.insert(root);
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
    rFB.task()
        .name       ("Delete basic components")
        .run_on     ({comScn.pl.activeEntDelete(UseOrRun)})
        .sync_with  ({physShapes.pl.ownedEnts(Modify__)})
        .args       ({      comScn.di.basic,                      comScn.di.activeEntDel })
        .func([] (ACtxBasic &rBasic, ActiveEntVec_t const &rActiveEntDel) noexcept
    {
        update_delete_basic(rBasic, rActiveEntDel.cbegin(), rActiveEntDel.cend());
    });

    rFB.task()
        .name       ("Clear Shape Spawning vector after use")
        .run_on     ({physShapes.pl.spawnRequest(Clear)})
        .args       ({           physShapes.di.physShapes })
        .func([] (ACtxPhysShapes &rPhysShapes) noexcept
    {
        rPhysShapes.m_spawnRequest.clear();
    });
}); // ftrPhysShapes



FeatureDef const ftrPhysicsShapesDraw = feature_def("PhysicsShapesDraw", [] (
        FeatureBuilder              &rFB,
        Implement<FIPhysShapesDraw> physShapesDraw,
        DependOn<FISceneRenderer>   scnRender,
        DependOn<FICommonScene>     comScn,
        DependOn<FIPhysics>         phys,
        DependOn<FIPhysShapes>      physShapes,
        DependOn<FIWindowApp>       windowApp,
        entt::any                   userData)
{
    auto const materialId = userData /* if not null */ ? entt::any_cast<MaterialId>(userData)
                                                       : MaterialId{};

    rFB.data_emplace<MaterialId>(physShapesDraw.di.material, materialId);

    // TODO: format after framework changes

    rFB.task()
        .name       ("Create DrawEnts for spawned shapes")
        .run_on     ({physShapes.pl.spawnRequest(UseOrRun)})
        .sync_with  ({physShapes.pl.spawnedEnts(UseOrRun), comScn.pl.activeEntResized(Done), scnRender.pl.drawEntResized(ModifyOrSignal), scnRender.pl.drawEnt(New)})
        .args       ({       comScn.di.basic,     comScn.di.drawing,      scnRender.di.scnRender,    physShapes.di.physShapes })
        .func([]    (ACtxBasic const &rBasic, ACtxDrawing &rDrawing, ACtxSceneRender &rScnRender, ACtxPhysShapes &rPhysShapes) noexcept
    {
        for (std::size_t i = 0; i < rPhysShapes.m_spawnRequest.size(); ++i)
        {
            ActiveEnt const child            = rPhysShapes.m_ents[i * 2 + 1];
            rScnRender.m_activeToDraw[child] = rScnRender.m_drawIds.create();
        }
    });

    rFB.task()
        .name       ("Add mesh and material to spawned shapes")
        .run_on     ({physShapes.pl.spawnRequest(UseOrRun)})
        .sync_with  ({physShapes.pl.spawnedEnts(UseOrRun),
                      scnRender.pl.entMesh(New), scnRender.pl.material(New), scnRender.pl.drawEnt(New), scnRender.pl.drawEntResized(Done),
                      scnRender.pl.materialDirty(Modify_), scnRender.pl.entMeshDirty(Modify_)})
        .args       ({           comScn.di.basic,     comScn.di.drawing,      scnRender.di.scnRender,    physShapes.di.physShapes,     comScn.di.namedMeshes, physShapesDraw.di.material })
        .func       ([] (ACtxBasic const &rBasic, ACtxDrawing &rDrawing, ACtxSceneRender &rScnRender, ACtxPhysShapes &rPhysShapes, NamedMeshes &rNamedMeshes,  MaterialId const material) noexcept
    {
        Material &rMat = rScnRender.m_materials[material];

        for (std::size_t i = 0; i < rPhysShapes.m_spawnRequest.size(); ++i)
        {
            SpawnShape const &spawn = rPhysShapes.m_spawnRequest[i];
            ActiveEnt const root    = rPhysShapes.m_ents[i * 2];
            ActiveEnt const child   = rPhysShapes.m_ents[i * 2 + 1];
            DrawEnt const drawEnt   = rScnRender.m_activeToDraw[child];

            rScnRender.m_needDrawTf.insert(root);
            rScnRender.m_needDrawTf.insert(child);

            rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(rNamedMeshes.m_shapeToMesh.at(spawn.m_shape));
            rScnRender.m_meshDirty.push_back(drawEnt);

            rMat.m_ents.insert(drawEnt);
            rMat.m_dirty.push_back(drawEnt);

            rScnRender.m_visible.insert(drawEnt);
            rScnRender.m_opaque.insert(drawEnt);
        }
    });

    // When does resync run relative to deletes?

    rFB.task()
        .name       ("Resync spawned shapes DrawEnts")
        .run_on     ({windowApp.pl.resync(Run)})
        .sync_with  ({physShapes.pl.ownedEnts(UseOrRun_), comScn.pl.hierarchy(Ready), comScn.pl.activeEntResized(Done), scnRender.pl.drawEntResized(ModifyOrSignal)})
        .args       ({       comScn.di.basic,     comScn.di.drawing,      scnRender.di.scnRender,    physShapes.di.physShapes, comScn.di.activeEntDel })
        .func([]    (ACtxBasic const &rBasic, ACtxDrawing &rDrawing, ACtxSceneRender &rScnRender, ACtxPhysShapes &rPhysShapes, ActiveEntVec_t const &rActiveEntDel) noexcept
    {
        for (ActiveEnt root : rPhysShapes.ownedEnts)
        {
            LGRN_ASSERT(rBasic.m_activeIds.exists(root));
            auto const children = SysSceneGraph::children(rBasic.m_scnGraph, root);
            ActiveEnt const child = *children.begin();

            rScnRender.m_activeToDraw[child] = rScnRender.m_drawIds.create();
        }
    });

    rFB.task()
        .name       ("Resync spawned shapes mesh and material")
        .run_on     ({windowApp.pl.resync(Run)})
        .sync_with  ({physShapes.pl.ownedEnts(UseOrRun_), scnRender.pl.entMesh(New), scnRender.pl.material(New), scnRender.pl.drawEnt(New), scnRender.pl.drawEntResized(Done),
                      scnRender.pl.materialDirty(Modify_), scnRender.pl.entMeshDirty(Modify_)})
        .args       ({           comScn.di.basic,     comScn.di.drawing,       phys.di.phys,    physShapes.di.physShapes,      scnRender.di.scnRender,     comScn.di.namedMeshes, physShapesDraw.di.material })
        .func       ([] (ACtxBasic const &rBasic, ACtxDrawing &rDrawing, ACtxPhysics &rPhys, ACtxPhysShapes &rPhysShapes, ACtxSceneRender &rScnRender, NamedMeshes &rNamedMeshes,  MaterialId const material) noexcept
    {
        Material &rMat = rScnRender.m_materials[material];

        for (ActiveEnt root : rPhysShapes.ownedEnts)
        {
            ActiveEnt const child = *SysSceneGraph::children(rBasic.m_scnGraph, root).begin();

            //SpawnShape const &spawn = rPhysShapes.m_spawnRequest[i];
            DrawEnt const drawEnt   = rScnRender.m_activeToDraw[child];

            rScnRender.m_needDrawTf.insert(root);
            rScnRender.m_needDrawTf.insert(child);

            EShape const shape = rPhys.m_shape.at(child);
            rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(rNamedMeshes.m_shapeToMesh.at(shape));
            rScnRender.m_meshDirty.push_back(drawEnt);

            rMat.m_ents.insert(drawEnt);
            rMat.m_dirty.push_back(drawEnt);

            rScnRender.m_visible.insert(drawEnt);
            rScnRender.m_opaque.insert(drawEnt);
        }
    });

    rFB.task()
        .name       ("Remove deleted ActiveEnts from ACtxPhysShapes")
        .run_on     ({comScn.pl.activeEntDelete(UseOrRun)})
        .sync_with  ({physShapes.pl.ownedEnts(Modify__)})
        .args       ({      physShapes.di.physShapes,              comScn.di.activeEntDel })
        .func       ([] (ACtxPhysShapes &rPhysShapes, ActiveEntVec_t const &rActiveEntDel) noexcept
    {
        for (ActiveEnt const deleted : rActiveEntDel)
        {
            rPhysShapes.ownedEnts.erase(deleted);
        }
    });
}); // setup_phys_shapes_draw



FeatureDef const ftrThrower = feature_def("Thrower", [] (
        FeatureBuilder              &rFB,
        Implement<FIThrower>        thrower,
        DependOn<FICameraControl>   camCtrl,
        DependOn<FIPhysShapes>      physShapes,
        DependOn<FIWindowApp>       windowApp)
{
    auto &rCamCtrl = rFB.data_get< ACtxCameraController > (camCtrl.di.camCtrl);

    rFB.data_emplace< EButtonControlIndex > (thrower.di.button, rCamCtrl.m_controls.button_subscribe("debug_throw"));

    rFB.task()
        .name       ("Throw spheres when pressing space")
        .run_on     ({windowApp.pl.inputs(Run)})
        .sync_with  ({camCtrl.pl.camCtrl(Ready), physShapes.pl.spawnRequest(Modify_)})
        .args       ({               camCtrl.di.camCtrl,    physShapes.di.physShapes,          thrower.di.button })
        .func       ([] (ACtxCameraController &rCamCtrl, ACtxPhysShapes &rPhysShapes, EButtonControlIndex button) noexcept
    {
        // Throw a sphere when the throw button is pressed
        if (rCamCtrl.m_controls.button_held(button))
        {
            Matrix4 const &camTf = rCamCtrl.m_transform;
            float const speed = 120;
            float const dist = 8.0f;

            for (int x = -2; x < 3; ++x)
            {
                for (int y = -2; y < 3; ++y)
                {
                    rPhysShapes.m_spawnRequest.push_back({
                        .m_position = camTf.translation() - camTf.backward()*dist + camTf.up()*float(y)*5.5f + camTf.right()*float(x)*5.5f,
                        .m_velocity = -camTf.backward()*speed,
                        .m_size     = Vector3{1.0f},
                        .m_mass     = 1.0f,
                        .m_shape    = EShape::Sphere
                    });
                }
            }
        }
    });
}); // ftrThrower



FeatureDef const ftrDroppers = feature_def("Droppers", [] (
        FeatureBuilder              &rFB,
        Implement<FIDroppers>       droppers,
        DependOn<FIScene>           scn,
        DependOn<FICommonScene>     comScn,
        DependOn<FIPhysShapes>      physShapes)
{
    rFB.data_emplace< float > (droppers.di.timerA, 0.0f);
    rFB.data_emplace< float > (droppers.di.timerB, 0.0f);

    rFB.task()
        .name       ("Spawn blocks every 2 seconds")
        .run_on     ({scn.pl.update(Run)})
        .sync_with  ({physShapes.pl.spawnRequest(Modify_)})
        .args       ({      physShapes.di.physShapes, droppers.di.timerA,     scn.di.deltaTimeIn })
        .func       ([] (ACtxPhysShapes &rPhysShapes,      float &timer, float const deltaTimeIn) noexcept

    {
        timer += deltaTimeIn;
        if (timer >= 2.0f)
        {
            timer -= 2.0f;

            rPhysShapes.m_spawnRequest.push_back({
                .m_position = {10.0f, 0.0f, 30.0f},
                .m_velocity = {0.0f, 0.0f, 0.0f},
                .m_size     = {2.0f, 2.0f, 1.0f},
                .m_mass     = 1.0f,
                .m_shape    = EShape::Box
            });
        }
    });

    rFB.task()
        .name       ("Spawn cylinders every 1 second")
        .run_on     ({scn.pl.update(Run)})
        .sync_with  ({physShapes.pl.spawnRequest(Modify_)})
        .args       ({      physShapes.di.physShapes, droppers.di.timerB,     scn.di.deltaTimeIn })
        .func       ([] (ACtxPhysShapes &rPhysShapes,      float &timer, float const deltaTimeIn) noexcept
    {
        timer += deltaTimeIn;
        if (timer >= 1.0f)
        {
            timer -= 1.0f;

            rPhysShapes.m_spawnRequest.push_back({
                .m_position = {-10.0f, 0.0, 30.0f},
                .m_velocity = {0.0f, 0.0f, 0.0f},
                .m_size     = {2.0f, 2.0f, 1.0f},
                .m_mass     = 1.0f,
                .m_shape    = EShape::Cylinder
            });
        }
    });
}); // ftrDroppers




FeatureDef const ftrBounds = feature_def("Bounds", [] (
        FeatureBuilder              &rFB,
        Implement<FIBounds>         bounds,
        DependOn<FIScene>           scn,
        DependOn<FICommonScene>     comScn,
        DependOn<FIPhysShapes>      physShapes)
{
    rFB.pipeline(bounds.pl.boundsSet)     .parent(scn.pl.update);
    rFB.pipeline(bounds.pl.outOfBounds)   .parent(scn.pl.update);

    rFB.data_emplace< ActiveEntSet_t >       (bounds.di.bounds);
    rFB.data_emplace< ActiveEntVec_t >       (bounds.di.outOfBounds);

    rFB.task()
        .name       ("Check for out-of-bounds entities")
        .run_on     ({scn.pl.update(Run)})
        .sync_with  ({comScn.pl.transform(Ready), bounds.pl.boundsSet(Ready), bounds.pl.outOfBounds(Modify__)})
        .args       ({    comScn.di.basic,              bounds.di.bounds,        bounds.di.outOfBounds })
        .func([] (ACtxBasic const &rBasic, ActiveEntSet_t const &rBounds, ActiveEntVec_t &rOutOfBounds) noexcept
    {
        for (ActiveEnt const ent : rBounds)
        {
            ACompTransform const &entTf = rBasic.m_transform.get(ent);
            if (entTf.m_transform.translation().z() < -10)
            {
                rOutOfBounds.push_back(ent);
            }
        }
    });

    rFB.task()
        .name       ("Queue-Delete out-of-bounds entities")
        .run_on     ({bounds.pl.outOfBounds(UseOrRun_)})
        .sync_with  ({comScn.pl.activeEntDelete(Modify_), comScn.pl.hierarchy(Delete)})
        .args       ({     comScn.di.basic,        comScn.di.activeEntDel,        bounds.di.outOfBounds })
        .func       ([] (ACtxBasic &rBasic, ActiveEntVec_t &rActiveEntDel, ActiveEntVec_t &rOutOfBounds) noexcept
    {
        SysSceneGraph::queue_delete_entities(rBasic.m_scnGraph, rActiveEntDel, rOutOfBounds.begin(), rOutOfBounds.end());
    });

    rFB.task()
        .name       ("Clear out-of-bounds vector once we're done with it")
        .run_on     ({bounds.pl.outOfBounds(Clear_)})
        .args       ({          bounds.di.outOfBounds })
        .func       ([] (ActiveEntVec_t &rOutOfBounds) noexcept
    {
        rOutOfBounds.clear();
    });

    rFB.task()
        .name       ("Add bounds to spawned shapes")
        .run_on     ({physShapes.pl.spawnRequest(UseOrRun)})
        .sync_with  ({physShapes.pl.spawnedEnts(UseOrRun), bounds.pl.boundsSet(Modify)})
        .args       ({     comScn.di.basic,    physShapes.di.physShapes,        bounds.di.bounds })
        .func       ([] (ACtxBasic &rBasic, ACtxPhysShapes &rPhysShapes, ActiveEntSet_t &rBounds) noexcept
    {
        rBounds.resize(rBasic.m_activeIds.capacity());

        for (std::size_t i = 0; i < rPhysShapes.m_spawnRequest.size(); ++i)
        {
            SpawnShape const &spawn = rPhysShapes.m_spawnRequest[i];
            if (spawn.m_mass == 0)
            {
                continue;
            }

            ActiveEnt const root    = rPhysShapes.m_ents[i * 2];

            rBounds.insert(root);
        }
    });

    rFB.task()
        .name       ("Delete bounds components")
        .run_on     ({comScn.pl.activeEntDelete(UseOrRun)})
        .sync_with  ({bounds.pl.boundsSet(Delete)})
        .args       ({                comScn.di.activeEntDel,        bounds.di.bounds })
        .func       ([] (ActiveEntVec_t const &rActiveEntDel, ActiveEntSet_t &rBounds) noexcept
    {
        for (osp::active::ActiveEnt const ent : rActiveEntDel)
        {
            rBounds.erase(ent);
        }
    });
}); // ftrBounds


} // namespace adera
