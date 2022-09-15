/**
 * Open Space Program
 * Copyright © 2019-2021 Open Space Program Project
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

#include "scenarios.h"
#include "scenarios_enginetest.h"
#include "identifiers.h"

#include "scene_common.h"
#include "scene_renderer.h"
#include "scene_physics.h"
#include "scene_misc.h"

#include <osp/Active/basic.h>
#include <osp/Active/SysPrefabInit.h>
#include <osp/Resource/resources.h>

#include "../ActiveApplication.h"

#include <osp/logging.h>

#include <Magnum/GL/DefaultFramebuffer.h>

using namespace osp;

namespace testapp
{

static void setup_magnum_draw(MainView mainView, Session const& magnum, Session const& scnCommon, Session const& scnRender)
{
    OSP_SESSION_UNPACK_DATA(scnRender,  TESTAPP_COMMON_RENDERER);
    OSP_SESSION_UNPACK_TAGS(scnCommon,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(magnum,     TESTAPP_APP_MAGNUM);
    OSP_SESSION_UNPACK_TAGS(magnum,     TESTAPP_APP_MAGNUM);

    Tags                    &rTags      = mainView.m_rTags;
    Tasks                   &rTasks     = mainView.m_rTasks;
    TopTaskDataVec_t        &rTaskData  = mainView.m_rTaskData;
    ExecutionContext        &rExec      = mainView.m_rExec;
    ArrayView<entt::any>    topData     = mainView.m_topData;

    auto &rActiveApp = top_get<ActiveApplication>(topData, idActiveApp);

    if ( ! debug_top_verify(rTags, rTasks, rTaskData))
    {
        rActiveApp.exit();
        OSP_LOG_INFO("Errors detected, scene closed.");
        return;
    }

    auto &rCamera = top_get<active::Camera>(topData, idCamera);
    rCamera.set_aspect_ratio(Vector2{Magnum::GL::defaultFramebuffer.viewport().size()});


    top_enqueue_quick(rTags, rTasks, rExec, {tgSyncEvt, tgResyncEvt});
    top_run_blocking(rTags, rTasks, rTaskData, topData, rExec);

    auto const runTags = {tgSyncEvt, tgSceneEvt, tgTimeEvt, tgRenderEvt, tgInputEvt};

    rActiveApp.set_on_draw( [&rTags, &rTasks, &rExec, &rTaskData, topData, runTagsVec = std::vector<TagId>(runTags)] (ActiveApplication& rApp, float delta)
    {
        top_enqueue_quick(rTags, rTasks, rExec, runTagsVec);
        top_run_blocking(rTags, rTasks, rTaskData, topData, rExec);
    });
}

static ScenarioMap_t make_scenarios()
{
    ScenarioMap_t scenarioMap;

    auto const add_scenario = [&scenarioMap] (std::string_view name, std::string_view desc, SceneSetup_t run)
    {
        scenarioMap.emplace(name, ScenarioOption{desc, run});
    };

    add_scenario("enginetest", "Demonstrate basic game engine functions without using TopTasks",
                 [] (MainView mainView, PkgId pkg, Sessions_t& sceneOut) -> RendererSetup_t
    {
        sceneOut.resize(1);
        TopDataId const idSceneData = sceneOut.front().acquire_data<1>(mainView.m_topData).front();
        auto &rResources = top_get<Resources>(mainView.m_topData, mainView.m_idResources);

        // enginetest::setup_scene returns an entt::any containing one big
        // struct containing all the scene data.
        top_assign<enginetest::EngineTestScene>(mainView.m_topData, idSceneData, enginetest::setup_scene(rResources, pkg));

        return [] (MainView mainView, Session const& magnum, Sessions_t const& scene, [[maybe_unused]] Sessions_t& rendererOut)
        {
            TopDataId const idSceneData = scene.front().m_dataIds.front();
            auto& rScene = top_get<enginetest::EngineTestScene>(mainView.m_topData, idSceneData);

            OSP_SESSION_UNPACK_DATA(magnum, TESTAPP_APP_MAGNUM);
            auto &rActiveApp    = top_get< ActiveApplication >      (mainView.m_topData, idActiveApp);
            auto &rRenderGl     = top_get< active::RenderGL >       (mainView.m_topData, idRenderGl);
            auto &rUserInput    = top_get< input::UserInputHandler >(mainView.m_topData, idUserInput);

            rActiveApp.set_on_draw(enginetest::generate_draw_func(rScene, rActiveApp, rRenderGl, rUserInput));
        };
    });

    add_scenario("physicstest", "Physics lol",
                 [] (MainView mainView, PkgId pkg, Sessions_t& sceneOut) -> RendererSetup_t
    {
        using namespace testapp::scenes;

        auto const idResources  = mainView.m_idResources;
        auto &rTopData          = mainView.m_topData;
        auto &rTags             = mainView.m_rTags;
        Builder_t builder{rTags, mainView.m_rTasks, mainView.m_rTaskData};

        sceneOut.resize(9);
        auto & [scnCommon, matVisual, physics, newton, shapeSpawn, droppers, gravity, bounds, thrower] = unpack<9>(sceneOut);

        // Compose together lots of Sessions
        scnCommon   = setup_common_scene    (builder, rTopData, rTags, idResources);
        matVisual   = setup_material        (builder, rTopData, rTags, scnCommon);
        physics     = setup_physics         (builder, rTopData, rTags, scnCommon, idResources, pkg);
        newton      = setup_newton_physics  (builder, rTopData, rTags, scnCommon, physics);
        shapeSpawn  = setup_shape_spawn     (builder, rTopData, rTags, scnCommon, physics, matVisual);
        droppers    = setup_droppers        (builder, rTopData, rTags, scnCommon, shapeSpawn);
        gravity     = setup_gravity         (builder, rTopData, rTags, scnCommon, physics, shapeSpawn);
        bounds      = setup_bounds          (builder, rTopData, rTags, scnCommon, physics, shapeSpawn);

        add_floor(rTopData, scnCommon, matVisual, shapeSpawn, idResources, pkg);

        return [] (MainView mainView, Session const& magnum, Sessions_t const& scene, [[maybe_unused]] Sessions_t& rendererOut)
        {
            auto &rTopData = mainView.m_topData;
            auto &rTags = mainView.m_rTags;
            Builder_t builder{mainView.m_rTags, mainView.m_rTasks, mainView.m_rTaskData};

            auto const& [scnCommon, matVisual, physics, newton, shapeSpawn, gravity, bounds, thrower] = unpack<8>(scene);

            using namespace testapp::scenes;

            rendererOut.resize(5);
            auto & [scnRender, cameraCtrl, cameraFree, shVisual, camThrow] = unpack<5>(rendererOut);
            scnRender   = setup_scene_renderer      (builder, rTopData, rTags, magnum, scnCommon, mainView.m_idResources);
            cameraCtrl  = setup_camera_magnum       (builder, rTopData, rTags, magnum);
            cameraFree  = setup_camera_free         (builder, rTopData, rTags, magnum, scnCommon, scnRender, cameraCtrl);
            shVisual    = setup_shader_visualizer   (builder, rTopData, rTags, magnum, scnCommon, scnRender, matVisual);
            camThrow    = setup_thrower             (builder, rTopData, rTags, magnum, scnRender, cameraCtrl, shapeSpawn);

            setup_magnum_draw(mainView, magnum, scnCommon, scnRender);
        };
    });

    add_scenario("vehicletest", "Vehicles, gwah!",
                 [] (MainView mainView, PkgId pkg, Sessions_t& sceneOut) -> RendererSetup_t
    {
        using namespace testapp::scenes;

        auto const idResources  = mainView.m_idResources;
        auto &rTopData          = mainView.m_topData;
        auto &rTags             = mainView.m_rTags;
        Builder_t builder{rTags, mainView.m_rTasks, mainView.m_rTaskData};

        sceneOut.resize(10);
        auto & [scnCommon, matVisual, physics, newton, shapeSpawn, prefabs, droppers, gravity, bounds, thrower] = unpack<10>(sceneOut);

        // Compose together lots of Sessions
        scnCommon   = setup_common_scene    (builder, rTopData, rTags, idResources);
        matVisual   = setup_material        (builder, rTopData, rTags, scnCommon);
        physics     = setup_physics         (builder, rTopData, rTags, scnCommon, idResources, pkg);
        newton      = setup_newton_physics  (builder, rTopData, rTags, scnCommon, physics);
        shapeSpawn  = setup_shape_spawn     (builder, rTopData, rTags, scnCommon, physics, matVisual);
        prefabs     = setup_prefabs         (builder, rTopData, rTags, scnCommon, physics, matVisual, idResources);
        droppers    = setup_droppers        (builder, rTopData, rTags, scnCommon, shapeSpawn);
        gravity     = setup_gravity         (builder, rTopData, rTags, scnCommon, physics, shapeSpawn);
        bounds      = setup_bounds          (builder, rTopData, rTags, scnCommon, physics, shapeSpawn);

        OSP_SESSION_UNPACK_DATA(scnCommon, TESTAPP_COMMON_SCENE);
        add_floor(rTopData, scnCommon, matVisual, shapeSpawn, idResources, pkg);

        Resources &rResources = top_get<Resources>(rTopData, idResources);

        static Matrix4 uppy = Matrix4::translation({0.0f, 0.0f, 4.0f});

        top_get<osp::active::ACtxPrefabInit>(rTopData, prefabs.m_dataIds[0]).m_basic.push_back(osp::active::TmpPrefabInitBasic{
            .m_importerRes = rResources.find(restypes::gc_importer, pkg, "OSPData/adera/stomper.sturdy.gltf"),
            .m_prefabId = 0,
            .m_parent = top_get<osp::active::ACtxBasic>(rTopData, idBasic).m_hierRoot,
            .m_pTransform = &uppy
        });

        return [] (MainView mainView, Session const& magnum, Sessions_t const& scene, [[maybe_unused]] Sessions_t& rendererOut)
        {
            auto &rTopData = mainView.m_topData;
            auto &rTags = mainView.m_rTags;
            Builder_t builder{mainView.m_rTags, mainView.m_rTasks, mainView.m_rTaskData};

            auto const& [scnCommon, matVisual, physics, newton, shapeSpawn, gravity, bounds, thrower] = unpack<8>(scene);

            rendererOut.resize(4);

            using namespace testapp::scenes;

            rendererOut.resize(5);
            auto & [scnRender, cameraCtrl, cameraFree, shVisual, camThrow] = unpack<5>(rendererOut);
            scnRender   = setup_scene_renderer      (builder, rTopData, rTags, magnum, scnCommon, mainView.m_idResources);
            cameraCtrl  = setup_camera_magnum       (builder, rTopData, rTags, magnum);
            cameraFree  = setup_camera_free         (builder, rTopData, rTags, magnum, scnCommon, scnRender, cameraCtrl);
            shVisual    = setup_shader_visualizer   (builder, rTopData, rTags, magnum, scnCommon, scnRender, matVisual);
            camThrow    = setup_thrower             (builder, rTopData, rTags, magnum, scnRender, cameraCtrl, shapeSpawn);

            setup_magnum_draw(mainView, magnum, scnCommon, scnRender);
        };
    });

    return scenarioMap;
}

ScenarioMap_t const& scenarios()
{
    static ScenarioMap_t s_scenarioMap = make_scenarios();
    return s_scenarioMap;
}

} // namespace testapp


