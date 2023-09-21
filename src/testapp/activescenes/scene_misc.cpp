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
#include "scene_misc.h"
#include "scene_physics.h"
#include "scenarios.h"
#include "identifiers.h"

#include "CameraController.h"

#include <osp/Active/basic.h>
#include <osp/Active/drawing.h>

#include <osp/Active/SysSceneGraph.h>
#include <osp/Active/SysRender.h>

#include <osp/Resource/resources.h>

#include <osp/unpack.h>

#include <random>

using namespace osp;
using namespace osp::active;

using osp::phys::EShape;
using osp::input::EButtonControlIndex;

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

namespace testapp::scenes
{

void create_materials(
        ArrayView<entt::any> const  topData,
        Session const&              sceneRenderer,
        int const                   count)
{
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer, TESTAPP_DATA_SCENE_RENDERER);
    auto &rScnRender = top_get< ACtxSceneRender >(topData, idScnRender);

    for (int i = 0; i < count; ++i)
    {
        [[maybe_unused]] MaterialId const mat = rScnRender.m_materialIds.create();
        LGRN_ASSERT(int(mat) == i);
    }

    rScnRender.m_materials.resize(count);
}

void add_floor(
        ArrayView<entt::any> const  topData,
        Session const&              shapeSpawn,
        MaterialId const            materialId,
        PkgId const                 pkg,
        int const                   size)
{
    OSP_DECLARE_GET_DATA_IDS(shapeSpawn, TESTAPP_DATA_SHAPE_SPAWN);

    auto &rSpawner = top_get<ACtxShapeSpawner>(topData, idSpawner);

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
            rSpawner.m_spawnRequest.emplace_back(SpawnShape{
                .m_position = Vector3{x*spread, y*spread, heightZ},
                .m_velocity = {0.0f, 0.0f, 0.0f},
                .m_size     = Vector3{distSizeX(randGen), distSizeY(randGen), heightZ},
                .m_mass     = 0.0f,
                .m_shape    = EShape::Box
            });
        }
    }
}

Session setup_camera_ctrl(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              windowApp,
        Session const&              sceneRenderer,
        Session const&              magnumScene)
{
    OSP_DECLARE_GET_DATA_IDS(windowApp,     TESTAPP_DATA_WINDOW_APP);
    OSP_DECLARE_GET_DATA_IDS(magnumScene,   TESTAPP_DATA_MAGNUM_SCENE);
    auto const tgScnRdr = sceneRenderer .get_pipelines<PlSceneRenderer>();
    auto const tgSR     = magnumScene   .get_pipelines<PlMagnumScene>();

    auto &rUserInput = top_get< osp::input::UserInputHandler >(topData, idUserInput);

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_CAMERA_CTRL);
    auto const tgCmCt = out.create_pipelines<PlCameraCtrl>(rBuilder);

    top_emplace< ACtxCameraController > (topData, idCamCtrl, rUserInput);

    rBuilder.pipeline(tgCmCt.camCtrl).parent(tgScnRdr.render);

    rBuilder.task()
        .name       ("Position Rendering Camera according to Camera Controller")
        .run_on     ({tgScnRdr.render(Run)})
        .sync_with  ({tgCmCt.camCtrl(Ready), tgSR.camera(Modify)})
        .push_to    (out.m_tasks)
        .args       ({                           idCamCtrl,        idCamera })
        .func([] (ACtxCameraController const& rCamCtrl, Camera &rCamera) noexcept
    {
        rCamera.m_transform = rCamCtrl.m_transform;
    });

    return out;
}

Session setup_camera_free(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              windowApp,
        Session const&              scene,
        Session const&              cameraCtrl)
{
    OSP_DECLARE_GET_DATA_IDS(scene,         TESTAPP_DATA_SCENE);
    OSP_DECLARE_GET_DATA_IDS(cameraCtrl,    TESTAPP_DATA_CAMERA_CTRL);

    auto const tgWin    = windowApp     .get_pipelines<PlWindowApp>();
    auto const tgCmCt   = cameraCtrl    .get_pipelines<PlCameraCtrl>();

    Session out;

    rBuilder.task()
        .name       ("Move Camera controller")
        .run_on     ({tgWin.inputs(Run)})
        .sync_with  ({tgCmCt.camCtrl(Modify)})
        .push_to    (out.m_tasks)
        .args       ({                 idCamCtrl,           idDeltaTimeIn })
        .func([] (ACtxCameraController& rCamCtrl, float const deltaTimeIn) noexcept
    {
        SysCameraController::update_view(rCamCtrl, deltaTimeIn);
        SysCameraController::update_move(rCamCtrl, deltaTimeIn, true);
    });

    return out;
}


Session setup_thrower(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              windowApp,
        Session const&              cameraCtrl,
        Session const&              shapeSpawn)
{
    OSP_DECLARE_GET_DATA_IDS(shapeSpawn,     TESTAPP_DATA_SHAPE_SPAWN);
    OSP_DECLARE_GET_DATA_IDS(cameraCtrl,   TESTAPP_DATA_CAMERA_CTRL);
    auto &rCamCtrl = top_get< ACtxCameraController > (topData, idCamCtrl);

    auto const tgWin    = windowApp .get_pipelines<PlWindowApp>();
    auto const tgCmCt   = cameraCtrl.get_pipelines<PlCameraCtrl>();
    auto const tgShSp   = shapeSpawn.get_pipelines<PlShapeSpawn>();

    Session out;
    auto const [idBtnThrow] = out.acquire_data<1>(topData);

    top_emplace< EButtonControlIndex > (topData, idBtnThrow, rCamCtrl.m_controls.button_subscribe("debug_throw"));

    rBuilder.task()
        .name       ("Throw spheres when pressing space")
        .run_on     ({tgWin.inputs(Run)})
        .sync_with  ({tgCmCt.camCtrl(Ready), tgShSp.spawnRequest(Modify_)})
        .push_to    (out.m_tasks)
        .args       ({                 idCamCtrl,                  idSpawner,                   idBtnThrow })
        .func([] (ACtxCameraController& rCamCtrl, ACtxShapeSpawner& rSpawner, EButtonControlIndex btnThrow) noexcept
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
                    rSpawner.m_spawnRequest.push_back({
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
}

Session setup_droppers(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              scene,
        Session const&              commonScene,
        Session const&              shapeSpawn)
{
    OSP_DECLARE_GET_DATA_IDS(scene,         TESTAPP_DATA_SCENE);
    OSP_DECLARE_GET_DATA_IDS(commonScene,   TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(shapeSpawn,    TESTAPP_DATA_SHAPE_SPAWN);

    auto const tgScn    = scene         .get_pipelines<PlScene>();
    auto const tgShSp   = shapeSpawn    .get_pipelines<PlShapeSpawn>();

    Session out;
    auto const [idSpawnTimerA, idSpawnTimerB] = out.acquire_data<2>(topData);

    top_emplace< float > (topData, idSpawnTimerA, 0.0f);
    top_emplace< float > (topData, idSpawnTimerB, 0.0f);

    rBuilder.task()
        .name       ("Spawn blocks every 2 seconds")
        .run_on     ({tgScn.update(Run)})
        .sync_with  ({tgShSp.spawnRequest(Modify_)})
        .push_to    (out.m_tasks)
        .args({                    idSpawner,       idSpawnTimerA,          idDeltaTimeIn })
        .func([] (ACtxShapeSpawner& rSpawner, float& rSpawnTimer, float const deltaTimeIn) noexcept

    {
        rSpawnTimer += deltaTimeIn;
        if (rSpawnTimer >= 2.0f)
        {
            rSpawnTimer -= 2.0f;

            rSpawner.m_spawnRequest.push_back({
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
        .args({                    idSpawner,       idSpawnTimerB,          idDeltaTimeIn })
        .func([] (ACtxShapeSpawner& rSpawner, float& rSpawnTimer, float const deltaTimeIn) noexcept
    {
        rSpawnTimer += deltaTimeIn;
        if (rSpawnTimer >= 1.0f)
        {
            rSpawnTimer -= 1.0f;

            rSpawner.m_spawnRequest.push_back({
                .m_position = {-10.0f, 0.0, 30.0f},
                .m_velocity = {0.0f, 0.0f, 0.0f},
                .m_size     = {2.0f, 2.0f, 1.0f},
                .m_mass     = 1.0f,
                .m_shape    = EShape::Cylinder
            });
        }
    });

    return out;
}

Session setup_bounds(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              scene,
        Session const&              commonScene,
        Session const&              shapeSpawn)
{
    OSP_DECLARE_GET_DATA_IDS(commonScene,   TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(shapeSpawn,    TESTAPP_DATA_SHAPE_SPAWN);
    auto const tgScn    = scene         .get_pipelines<PlScene>();
    auto const tgCS     = commonScene   .get_pipelines<PlCommonScene>();
    auto const tgShSp   = shapeSpawn    .get_pipelines<PlShapeSpawn>();

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
        .args       ({      idBasic,                  idSpawner,                idBounds })
        .func([] (ACtxBasic& rBasic, ACtxShapeSpawner& rSpawner, ActiveEntSet_t& rBounds) noexcept
    {
        rBounds.ints().resize(rBasic.m_activeIds.vec().capacity());

        for (std::size_t i = 0; i < rSpawner.m_spawnRequest.size(); ++i)
        {
            SpawnShape const &spawn = rSpawner.m_spawnRequest[i];
            if (spawn.m_mass == 0)
            {
                continue;
            }

            ActiveEnt const root    = rSpawner.m_ents[i * 2];

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
}

} // namespace testapp::scenes
