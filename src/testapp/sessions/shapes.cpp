/**
 * Open Space Program
 * Copyright © 2019-2023 Open Space Program Project
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
#include "common.h"

#include <adera/drawing/CameraController.h>

#include <osp/activescene/basic.h>
#include <osp/activescene/physics_fn.h>
#include <osp/drawing/drawing_fn.h>
#include <osp/drawing/prefab_draw.h>

#include <random>

using namespace adera;
using namespace osp;
using namespace osp::active;
using namespace osp::draw;

using osp::input::EButtonControlIndex;

namespace testapp::scenes
{

void add_floor(
        ArrayView<entt::any> const  topData,
        Session const&              physShapes,
        MaterialId const            materialId,
        PkgId const                 pkg,
        int const                   size)
{
    OSP_DECLARE_GET_DATA_IDS(physShapes, TESTAPP_DATA_PHYS_SHAPES);

    auto &rPhysShapes = top_get<ACtxPhysShapes>(topData, idPhysShapes);

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
                .m_position = Vector3{x*spread, y*spread, heightZ},
                .m_velocity = {0.0f, 0.0f, 0.0f},
                .m_size     = Vector3{distSizeX(randGen), distSizeY(randGen), heightZ},
                .m_mass     = 0.0f,
                .m_shape    = EShape::Box
            });
        }
    }
}



Session setup_phys_shapes(
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
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_PHYS_SHAPES);
    auto const tgShSp = out.create_pipelines<PlPhysShapes>(rBuilder);

    rBuilder.pipeline(tgShSp.spawnRequest)  .parent(tgScn.update);
    rBuilder.pipeline(tgShSp.spawnedEnts)   .parent(tgScn.update);
    rBuilder.pipeline(tgShSp.ownedEnts)     .parent(tgScn.update);

    top_emplace< ACtxPhysShapes > (topData, idPhysShapes, ACtxPhysShapes{ .m_materialId = materialId });

    rBuilder.task()
        .name       ("Schedule Shape spawn")
        .schedules  ({tgShSp.spawnRequest(Schedule_)})
        .sync_with  ({tgScn.update(Run)})
        .push_to    (out.m_tasks)
        .args       ({           idPhysShapes })
        .func([] (ACtxPhysShapes& rPhysShapes) noexcept -> TaskActions
    {
        return rPhysShapes.m_spawnRequest.empty() ? TaskAction::Cancel : TaskActions{};
    });

    rBuilder.task()
        .name       ("Create ActiveEnts for requested shapes to spawn")
        .run_on     ({tgShSp.spawnRequest(UseOrRun)})
        .sync_with  ({tgCS.activeEnt(New), tgCS.activeEntResized(Schedule), tgShSp.spawnedEnts(Resize)})
        .push_to    (out.m_tasks)
        .args       ({      idBasic,                idPhysShapes})
        .func([] (ACtxBasic& rBasic, ACtxPhysShapes& rPhysShapes) noexcept
    {
        LGRN_ASSERTM(!rPhysShapes.m_spawnRequest.empty(), "spawnRequest Use_ shouldn't run if rPhysShapes.m_spawnRequest is empty!");

        rPhysShapes.m_ents.resize(rPhysShapes.m_spawnRequest.size() * 2);
        rBasic.m_activeIds.create(rPhysShapes.m_ents.begin(), rPhysShapes.m_ents.end());
    });

    rBuilder.task()
        .name       ("Add hierarchy and transform to spawned shapes")
        .run_on     ({tgShSp.spawnRequest(UseOrRun)})
        .sync_with  ({tgShSp.spawnedEnts(UseOrRun), tgShSp.ownedEnts(Modify__), tgCS.hierarchy(New), tgCS.transform(New)})
        .push_to    (out.m_tasks)
        .args       ({      idBasic,                idPhysShapes })
        .func([] (ACtxBasic& rBasic, ACtxPhysShapes& rPhysShapes) noexcept
    {
        osp::bitvector_resize(rPhysShapes.ownedEnts, rBasic.m_activeIds.capacity());
        rBasic.m_scnGraph.resize(rBasic.m_activeIds.capacity());

        SubtreeBuilder bldScnRoot = SysSceneGraph::add_descendants(rBasic.m_scnGraph, rPhysShapes.m_spawnRequest.size() * 2);

        for (std::size_t i = 0; i < rPhysShapes.m_spawnRequest.size(); ++i)
        {
            SpawnShape const &spawn = rPhysShapes.m_spawnRequest[i];
            ActiveEnt const root    = rPhysShapes.m_ents[i * 2];
            ActiveEnt const child   = rPhysShapes.m_ents[i * 2 + 1];

            rPhysShapes.ownedEnts.set(std::size_t(root));

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
        .args       ({            idBasic,                idPhysShapes,             idPhys })
        .func([] (ACtxBasic const& rBasic, ACtxPhysShapes& rPhysShapes, ACtxPhysics& rPhys) noexcept
    {
        rPhys.m_hasColliders.ints().resize(rBasic.m_activeIds.vec().capacity());
        rPhys.m_shape.resize(rBasic.m_activeIds.capacity());

        for (std::size_t i = 0; i < rPhysShapes.m_spawnRequest.size(); ++i)
        {
            SpawnShape const &spawn = rPhysShapes.m_spawnRequest[i];
            ActiveEnt const root    = rPhysShapes.m_ents[i * 2];
            ActiveEnt const child   = rPhysShapes.m_ents[i * 2 + 1];

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
        .args       ({           idPhysShapes })
        .func([] (ACtxPhysShapes& rPhysShapes) noexcept
    {
        rPhysShapes.m_spawnRequest.clear();
    });


    return out;
} // setup_phys_shapes




Session setup_phys_shapes_draw(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              windowApp,
        Session const&              sceneRenderer,
        Session const&              commonScene,
        Session const&              physics,
        Session const&              physShapes)
{
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer, TESTAPP_DATA_SCENE_RENDERER);
    OSP_DECLARE_GET_DATA_IDS(commonScene,   TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(physics,       TESTAPP_DATA_PHYSICS);
    OSP_DECLARE_GET_DATA_IDS(physShapes,    TESTAPP_DATA_PHYS_SHAPES);
    auto const tgWin    = windowApp     .get_pipelines< PlWindowApp >();
    auto const tgScnRdr = sceneRenderer .get_pipelines< PlSceneRenderer >();
    auto const tgCS     = commonScene   .get_pipelines< PlCommonScene >();
    auto const tgShSp   = physShapes    .get_pipelines< PlPhysShapes >();

    Session out;

    rBuilder.task()
        .name       ("Create DrawEnts for spawned shapes")
        .run_on     ({tgShSp.spawnRequest(UseOrRun)})
        .sync_with  ({tgShSp.spawnedEnts(UseOrRun), tgCS.activeEntResized(Done), tgScnRdr.drawEntResized(ModifyOrSignal), tgScnRdr.drawEnt(New)})
        .push_to    (out.m_tasks)
        .args       ({               idBasic,             idDrawing,                 idScnRender,                idPhysShapes,             idNMesh })
        .func([]    (ACtxBasic const& rBasic, ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, ACtxPhysShapes& rPhysShapes, NamedMeshes& rNMesh) noexcept
    {
        for (std::size_t i = 0; i < rPhysShapes.m_spawnRequest.size(); ++i)
        {
            ActiveEnt const child            = rPhysShapes.m_ents[i * 2 + 1];
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
        .args       ({            idBasic,             idDrawing,                 idScnRender,                idPhysShapes,             idNMesh })
        .func([] (ACtxBasic const& rBasic, ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, ACtxPhysShapes& rPhysShapes, NamedMeshes& rNMesh) noexcept
    {
        Material &rMat = rScnRender.m_materials[rPhysShapes.m_materialId];

        for (std::size_t i = 0; i < rPhysShapes.m_spawnRequest.size(); ++i)
        {
            SpawnShape const &spawn = rPhysShapes.m_spawnRequest[i];
            ActiveEnt const root    = rPhysShapes.m_ents[i * 2];
            ActiveEnt const child   = rPhysShapes.m_ents[i * 2 + 1];
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
        .args       ({               idBasic,             idDrawing,                 idScnRender,                idPhysShapes,             idNMesh })
        .func([]    (ACtxBasic const& rBasic, ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, ACtxPhysShapes& rPhysShapes, NamedMeshes& rNMesh) noexcept
    {
        for (std::size_t entInt : rPhysShapes.ownedEnts.ones())
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
        .args       ({            idBasic,             idDrawing,             idPhys,                idPhysShapes,                 idScnRender,             idNMesh })
        .func([] (ACtxBasic const& rBasic, ACtxDrawing& rDrawing, ACtxPhysics& rPhys, ACtxPhysShapes& rPhysShapes, ACtxSceneRender& rScnRender, NamedMeshes& rNMesh) noexcept
    {
        Material &rMat = rScnRender.m_materials[rPhysShapes.m_materialId];

        for (std::size_t entInt : rPhysShapes.ownedEnts.ones())
        {
            ActiveEnt const root = ActiveEnt(entInt);
            ActiveEnt const child = *SysSceneGraph::children(rBasic.m_scnGraph, root).begin();

            //SpawnShape const &spawn = rPhysShapes.m_spawnRequest[i];
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
        .name       ("Remove deleted ActiveEnts from ACtxPhysShapeser")
        .run_on     ({tgCS.activeEntDelete(UseOrRun)})
        .sync_with  ({tgShSp.ownedEnts(Modify__)})
        .push_to    (out.m_tasks)
        .args       ({           idPhysShapes,                      idActiveEntDel })
        .func([] (ACtxPhysShapes& rPhysShapes, ActiveEntVec_t const& rActiveEntDel) noexcept
    {
        for (ActiveEnt const deleted : rActiveEntDel)
        {
            rPhysShapes.ownedEnts.reset(std::size_t(deleted));
        }
    });

    return out;
} // setup_phys_shapes_draw




Session setup_thrower(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              windowApp,
        Session const&              cameraCtrl,
        Session const&              physShapes)
{
    OSP_DECLARE_GET_DATA_IDS(physShapes,     TESTAPP_DATA_PHYS_SHAPES);
    OSP_DECLARE_GET_DATA_IDS(cameraCtrl,   TESTAPP_DATA_CAMERA_CTRL);
    auto &rCamCtrl = top_get< ACtxCameraController > (topData, idCamCtrl);

    auto const tgWin    = windowApp .get_pipelines<PlWindowApp>();
    auto const tgCmCt   = cameraCtrl.get_pipelines<PlCameraCtrl>();
    auto const tgShSp   = physShapes.get_pipelines<PlPhysShapes>();

    Session out;
    auto const [idBtnThrow] = out.acquire_data<1>(topData);

    top_emplace< EButtonControlIndex > (topData, idBtnThrow, rCamCtrl.m_controls.button_subscribe("debug_throw"));

    rBuilder.task()
        .name       ("Throw spheres when pressing space")
        .run_on     ({tgWin.inputs(Run)})
        .sync_with  ({tgCmCt.camCtrl(Ready), tgShSp.spawnRequest(Modify_)})
        .push_to    (out.m_tasks)
        .args       ({                 idCamCtrl,                idPhysShapes,                   idBtnThrow })
        .func([] (ACtxCameraController& rCamCtrl, ACtxPhysShapes& rPhysShapes, EButtonControlIndex btnThrow) noexcept
    {
        // Throw a sphere when the throw button is pressed
        if (rCamCtrl.m_controls.button_held(btnThrow))
        {
            Matrix4 const &camTf = rCamCtrl.m_transform;
            float const speed = 120;
            float const dist = 8.0f;

            for (int x = -2; x < 3; ++x)
            {
                for (int y = -2; y < 3; ++y)
                {
                    rPhysShapes.m_spawnRequest.push_back({
                        .m_position = camTf.translation() - camTf.backward()*dist + camTf.up()*y*5.5f + camTf.right()*x*5.5f,
                        .m_velocity = -camTf.backward()*speed,
                        .m_size     = Vector3{1.0f},
                        .m_mass     = 1.0f,
                        .m_shape    = EShape::Sphere
                    });
                }
            }
        }
    });

    return out;
} // setup_thrower




Session setup_droppers(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              scene,
        Session const&              commonScene,
        Session const&              physShapes)
{
    OSP_DECLARE_GET_DATA_IDS(scene,         TESTAPP_DATA_SCENE);
    OSP_DECLARE_GET_DATA_IDS(commonScene,   TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(physShapes,    TESTAPP_DATA_PHYS_SHAPES);

    auto const tgScn    = scene         .get_pipelines<PlScene>();
    auto const tgShSp   = physShapes    .get_pipelines<PlPhysShapes>();

    Session out;
    auto const [idSpawnTimerA, idSpawnTimerB] = out.acquire_data<2>(topData);

    top_emplace< float > (topData, idSpawnTimerA, 0.0f);
    top_emplace< float > (topData, idSpawnTimerB, 0.0f);

    rBuilder.task()
        .name       ("Spawn blocks every 2 seconds")
        .run_on     ({tgScn.update(Run)})
        .sync_with  ({tgShSp.spawnRequest(Modify_)})
        .push_to    (out.m_tasks)
        .args({                  idPhysShapes,       idSpawnTimerA,          idDeltaTimeIn })
        .func([] (ACtxPhysShapes& rPhysShapes, float& rSpawnTimer, float const deltaTimeIn) noexcept

    {
        rSpawnTimer += deltaTimeIn;
        if (rSpawnTimer >= 2.0f)
        {
            rSpawnTimer -= 2.0f;

            rPhysShapes.m_spawnRequest.push_back({
                .m_position = {10.0f, 0.0f, 30.0f},
                .m_velocity = {0.0f, 0.0f, 0.0f},
                .m_size     = {2.0f, 2.0f, 1.0f},
                .m_mass     = 1.0f,
                .m_shape    = EShape::Box
            });
        }
    });

    rBuilder.task()
        .name       ("Spawn cylinders every 1 second")
        .run_on     ({tgScn.update(Run)})
        .sync_with  ({tgShSp.spawnRequest(Modify_)})
        .push_to    (out.m_tasks)
        .args({                  idPhysShapes,       idSpawnTimerB,          idDeltaTimeIn })
        .func([] (ACtxPhysShapes& rPhysShapes, float& rSpawnTimer, float const deltaTimeIn) noexcept
    {
        rSpawnTimer += deltaTimeIn;
        if (rSpawnTimer >= 1.0f)
        {
            rSpawnTimer -= 1.0f;

            rPhysShapes.m_spawnRequest.push_back({
                .m_position = {-10.0f, 0.0, 30.0f},
                .m_velocity = {0.0f, 0.0f, 0.0f},
                .m_size     = {2.0f, 2.0f, 1.0f},
                .m_mass     = 1.0f,
                .m_shape    = EShape::Cylinder
            });
        }
    });

    return out;
} // setup_droppers




Session setup_bounds(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              scene,
        Session const&              commonScene,
        Session const&              physShapes)
{
    OSP_DECLARE_GET_DATA_IDS(commonScene,   TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(physShapes,    TESTAPP_DATA_PHYS_SHAPES);
    auto const tgScn    = scene         .get_pipelines<PlScene>();
    auto const tgCS     = commonScene   .get_pipelines<PlCommonScene>();
    auto const tgShSp   = physShapes    .get_pipelines<PlPhysShapes>();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_BOUNDS);
    auto const tgBnds = out.create_pipelines<PlBounds>(rBuilder);

    rBuilder.pipeline(tgBnds.boundsSet)     .parent(tgScn.update);
    rBuilder.pipeline(tgBnds.outOfBounds)   .parent(tgScn.update);

    top_emplace< ActiveEntSet_t >       (topData, idBounds);
    top_emplace< ActiveEntVec_t >       (topData, idOutOfBounds);

    rBuilder.task()
        .name       ("Check for out-of-bounds entities")
        .run_on     ({tgScn.update(Run)})
        .sync_with  ({tgCS.transform(Ready), tgBnds.boundsSet(Ready), tgBnds.outOfBounds(Modify__)})
        .push_to    (out.m_tasks)
        .args       ({            idBasic,                      idBounds,                idOutOfBounds })
        .func([] (ACtxBasic const& rBasic, ActiveEntSet_t const& rBounds, ActiveEntVec_t& rOutOfBounds) noexcept
    {
        for (std::size_t const ent : rBounds.ones())
        {
            ACompTransform const &entTf = rBasic.m_transform.get(ActiveEnt(ent));
            if (entTf.m_transform.translation().z() < -10)
            {
                rOutOfBounds.push_back(ActiveEnt(ent));
            }
        }
    });

    rBuilder.task()
        .name       ("Queue-Delete out-of-bounds entities")
        .run_on     ({tgBnds.outOfBounds(UseOrRun_)})
        .sync_with  ({tgCS.activeEntDelete(Modify_), tgCS.hierarchy(Delete)})
        .push_to    (out.m_tasks)
        .args       ({      idBasic,                idActiveEntDel,                idOutOfBounds })
        .func([] (ACtxBasic& rBasic, ActiveEntVec_t& rActiveEntDel, ActiveEntVec_t& rOutOfBounds) noexcept
    {
        SysSceneGraph::queue_delete_entities(rBasic.m_scnGraph, rActiveEntDel, rOutOfBounds.begin(), rOutOfBounds.end());
    });

    rBuilder.task()
        .name       ("Clear out-of-bounds vector once we're done with it")
        .run_on     ({tgBnds.outOfBounds(Clear_)})
        .push_to    (out.m_tasks)
        .args       ({           idOutOfBounds })
        .func([] (ActiveEntVec_t& rOutOfBounds) noexcept
    {
        rOutOfBounds.clear();
    });

    rBuilder.task()
        .name       ("Add bounds to spawned shapes")
        .run_on     ({tgShSp.spawnRequest(UseOrRun)})
        .sync_with  ({tgShSp.spawnedEnts(UseOrRun), tgBnds.boundsSet(Modify)})
        .push_to    (out.m_tasks)
        .args       ({      idBasic,                idPhysShapes,                idBounds })
        .func([] (ACtxBasic& rBasic, ACtxPhysShapes& rPhysShapes, ActiveEntSet_t& rBounds) noexcept
    {
        rBounds.ints().resize(rBasic.m_activeIds.vec().capacity());

        for (std::size_t i = 0; i < rPhysShapes.m_spawnRequest.size(); ++i)
        {
            SpawnShape const &spawn = rPhysShapes.m_spawnRequest[i];
            if (spawn.m_mass == 0)
            {
                continue;
            }

            ActiveEnt const root    = rPhysShapes.m_ents[i * 2];

            rBounds.set(std::size_t(root));
        }
    });

    rBuilder.task()
        .name       ("Delete bounds components")
        .run_on     ({tgCS.activeEntDelete(UseOrRun)})
        .sync_with  ({tgBnds.boundsSet(Delete)})
        .push_to    (out.m_tasks)
        .args       ({                 idActiveEntDel,                idBounds })
        .func([] (ActiveEntVec_t const& rActiveEntDel, ActiveEntSet_t& rBounds) noexcept
    {
        for (osp::active::ActiveEnt const ent : rActiveEntDel)
        {
            rBounds.reset(std::size_t(ent));
        }
    });

    return out;
} // setup_bounds


} // namespace testapp::scenes
