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
#include "scenarios_magnum.h"
#include "testapp.h"
#include "MagnumWindowApp.h"
#include "enginetest.h"
#include "feature_interfaces.h"
#include "features/magnum.h"

#include <adera_app/application.h>
#include <adera_app/features/common.h>
#include <adera_app/features/jolt.h>
#include <adera_app/features/misc.h>
#include <adera_app/features/physics.h>
#include <adera_app/features/shapes.h>
#include <adera_app/features/terrain.h>
#include <adera_app/features/universe.h>
#include <adera_app/features/vehicles.h>
#include <adera_app/features/vehicles_machines.h>

#include <adera/drawing/CameraController.h>

#include <osp/util/UserInputHandler.h>

#include <Magnum/GL/DefaultFramebuffer.h>

using osp::input::UserInputHandler;
using namespace adera;
using namespace ftr_inter;
using namespace ftr_inter::stages;
using namespace osp::draw;
using namespace osp::fw;
using namespace osp;

namespace testapp
{

/**
 * Enginetest itself doesn't depend on the framework, but we need to store it somewhere.
 */
FeatureDef const ftrEngineTestRenderer = feature_def("EngineTestRenderer", [] (
        FeatureBuilder              &rFB,
        Implement<FIEngineTestRndr> engineTestRndr,
        DependOn<FIEngineTest>      engineTest,
        DependOn<FIScene>           scn,
        DependOn<FIMainApp>         mainApp,
        DependOn<FIWindowApp>       windowApp,
        DependOn<FIMagnum>          magnum)
{
    auto &rBigStruct    = rFB.data_get<enginetest::EngineTestScene> (engineTest.di.bigStruct);
    auto &rUserInput    = rFB.data_get<UserInputHandler>            (windowApp.di.userInput);
    auto &rMagnumApp    = rFB.data_get<MagnumWindowApp>             (magnum.di.magnumApp);
    auto &rRenderGl     = rFB.data_get<RenderGL>                    (magnum.di.renderGl);
    rFB.data(engineTestRndr.di.renderer) = enginetest::make_renderer(rBigStruct, rMagnumApp, rRenderGl, rUserInput);

    rFB.task()
        .name       ("Update & Render Engine Test Scene")
        .sync_with  ({scn.pl.update(Run)})
        .args       ({                   engineTest.di.bigStruct,                engineTestRndr.di.renderer,  magnum.di.renderGl,         magnum.di.magnumApp,      scn.di.deltaTimeIn  })
        .func       ([] (enginetest::EngineTestScene &rBigStruct, enginetest::EngineTestRenderer &rRenderer, RenderGL &rRenderGl, MagnumWindowApp &rMagnumApp, float const deltaTimeIn) noexcept
    {
        enginetest::draw(rBigStruct, rRenderer, rRenderGl, rMagnumApp, deltaTimeIn);
    });
});


ContextId make_scene_renderer(Framework &rFW, PkgId defaultPkg, ContextId mainContext, ContextId sceneCtx, ContextId windowCtx)
{
    auto const magnum           = rFW.get_interface<FIMagnum>         (windowCtx);
    auto       &rMagnumApp      = rFW.data_get<MagnumWindowApp>       (magnum.di.magnumApp);

    ContextId const scnRdrCtx = rFW.m_contextIds.create();

    // Choose which renderer features to use based on information on which features the scene
    // context contains.

    ContextBuilder  scnRdrCB { scnRdrCtx, { mainContext, windowCtx, sceneCtx }, rFW };

    auto const engineTest = rFW.get_interface<FIEngineTest>(sceneCtx);
    if (engineTest.id.has_value())
    {
        scnRdrCB.add_feature(ftrEngineTestRenderer);
        ContextBuilder::finalize(std::move(scnRdrCB));
        return scnRdrCtx;
    }

    if ( ! rFW.get_interface_id<FICommonScene>(sceneCtx).has_value() )
    {
        ContextBuilder::finalize(std::move(scnRdrCB));
        return scnRdrCtx;
    }

    scnRdrCB.add_feature(ftrCleanupCtx);
    scnRdrCB.add_feature(ftrSceneRenderer);
    scnRdrCB.add_feature(ftrMagnumScene);

    auto scnRender      = rFW.get_interface<FISceneRenderer>        (scnRdrCtx);
    auto magnumScn      = rFW.get_interface<FIMagnumScene>          (scnRdrCtx);
    auto &rScnRender    = rFW.data_get<draw::ACtxSceneRender>       (scnRender.di.scnRender);
    auto &rCamera       = rFW.data_get<draw::Camera>                (magnumScn.di.camera);
    MaterialId const matFlat        = rScnRender.m_materialIds.create();
    MaterialId const matPhong       = rScnRender.m_materialIds.create();
    MaterialId const matVisualizer  = rScnRender.m_materialIds.create();
    rScnRender.m_materials.resize(rScnRender.m_materialIds.size());
    rCamera.set_aspect_ratio(Vector2{Magnum::GL::defaultFramebuffer.viewport().size()});

    scnRdrCB.add_feature(ftrCameraControl);


    if (rFW.get_interface_id<FIPhysShapes>(sceneCtx).has_value())
    {
        scnRdrCB.add_feature(ftrPhysicsShapesDraw, matPhong);
        //scnRdrCB.add_feature(ftrThrower);
    }

    scnRdrCB.add_feature(ftrShaderPhong,        matPhong);
    scnRdrCB.add_feature(ftrShaderFlat,         matFlat);
    scnRdrCB.add_feature(ftrShaderVisualizer,   matVisualizer);

    scnRdrCB.add_feature(ftrCursor, TplPkgIdMaterialId{ defaultPkg, matFlat });
    scnRdrCB.add_feature(ftrCameraFree);
/*
    if (rFW.get_interface_id<FIPrefabs>(sceneCtx).has_value())
    {
        scnRdrCB.add_feature(ftrPrefabDraw, matPhong);
    }

    if (rFW.get_interface_id<FIVehicleSpawn>(sceneCtx).has_value())
    {
        scnRdrCB.add_feature(ftrVehicleControl);
        scnRdrCB.add_feature(ftrVehicleCamera);
        scnRdrCB.add_feature(ftrVehicleSpawnDraw);
    }
    else
    {
        scnRdrCB.add_feature(ftrCameraFree);
    }

    if (rFW.get_interface_id<FIUniPlanets>(sceneCtx).has_value())
    {
        scnRdrCB.add_feature(ftrUniverseTestPlanetsDraw, PlanetDrawParams{
            .planetMat = matVisualizer,
            .axisMat   = matFlat });
    }

    if (rFW.get_interface_id<FISolarSys>(sceneCtx).has_value())
    {
        scnRdrCB.add_feature(ftrSolarSystemDraw, PlanetDrawParams{
            .planetMat = matFlat
        });

        // Zoom out the camera so that all planets are in view
        auto scnRender      = rFW.get_interface<FICameraControl>    (scnRdrCB.m_ctx);
        auto &rCamCtrl      = rFW.data_get<ACtxCameraController>    (scnRender.di.camCtrl);
        rCamCtrl.m_orbitDistance += 75000;
    }

    if (rFW.get_interface_id<FIRocketsJolt>(sceneCtx).has_value())
    {
        scnRdrCB.add_feature(ftrMagicRocketThrustIndicator, TplPkgIdMaterialId{ defaultPkg, matFlat });
    }

    if ( ! scnRdrCB.has_error() && rFW.get_interface_id<FITerrain>(sceneCtx).has_value() )
    {
        scnRdrCB.add_feature(ftrTerrainDebugDraw, matVisualizer);
        scnRdrCB.add_feature(ftrTerrainDrawMagnum);

        auto scnRender      = rFW.get_interface<FICameraControl>    (scnRdrCB.m_ctx);
        auto &rCamCtrl      = rFW.data_get<ACtxCameraController>    (scnRender.di.camCtrl);

        rCamCtrl.m_target = Vector3(0.0f, 0.0f, 0.0f);
        rCamCtrl.m_orbitDistanceMin = 1.0f;
        rCamCtrl.m_moveSpeed = 0.5f;
    }*/

    ContextBuilder::finalize(std::move(scnRdrCB));
    return scnRdrCtx;
}



/**
 * @brief Runs Task/Pipeline main loop within MagnumWindowApp
 */
class CommonMagnumApp : public MagnumWindowApp::IEvents
{
public:

    CommonMagnumApp() noexcept { }

    // Called repeatedly by Magnum's main loop
    void draw(MagnumWindowApp& rApp, float delta) override
    {
        pExec->wait(*pFW);

        if (pMainLoopCtrl->keepOpenWaiting)
        {
            auto const mainApp        = pFW->get_interface<FIMainApp>(mainContext);
            pMainLoopCtrl->keepOpenWaiting = false;
            pExec->task_finish(*pFW, mainApp.tasks.keepOpen, true, {.cancel = false});
        }
    }

    ContextId       mainContext;
    Framework       *pFW;
    IExecutor       *pExec;
    MainLoopControl *pMainLoopCtrl;
};

void start_magnum_renderer(Framework &rFW, ContextId mainCtx, entt::any userData)
{
    auto args = entt::any_cast<MagnumRendererArgs>(userData);

    auto const mainApp   = rFW.get_interface<FIMainApp>(mainCtx);

    ContextId const sceneCtx  = rFW.data_get<AppContexts>(mainApp.di.appContexts).scene;
    ContextId const windowCtx = rFW.m_contextIds.create();

    ContextBuilder  windowCB { windowCtx, { mainCtx, sceneCtx }, rFW };
    windowCB.add_feature(adera::ftrCleanupCtx);
    windowCB.add_feature(adera::ftrWindowApp);

    // Adding this feature will open the GUI window
    windowCB.add_feature(ftrMagnum, MagnumWindowApp::Arguments{args.argc, args.argv});

    ContextBuilder::finalize(std::move(windowCB));

    ContextId const sceneRenderCtx = make_scene_renderer(rFW, args.defaultPkg, mainCtx, sceneCtx, windowCtx);

    auto &rAppCtxs = rFW.data_get<AppContexts>(mainApp.di.appContexts);

    rAppCtxs.window = windowCtx;
    rAppCtxs.sceneRender = sceneRenderCtx;

    auto const magnum           = rFW.get_interface<FIMagnum>         (windowCtx);
    auto       &rMagnumApp      = rFW.data_get<MagnumWindowApp>       (magnum.di.magnumApp);
    rMagnumApp.m_events         = std::make_unique<CommonMagnumApp>();

    auto const windowApp        = rFW.get_interface<FIWindowApp>      (rAppCtxs.window);
    auto       &rWindowLoopCtrl = rFW.data_get<WindowAppLoopControl>  (windowApp.di.windowAppLoopCtrl);

    rWindowLoopCtrl.doRender = true;
    rWindowLoopCtrl.doResync = true;
    rWindowLoopCtrl.doSync   = true;

    main_loop_stack().push_back(
            [init = true] (MainLoopArgs vars) mutable -> bool
    {
        auto const mainApp        = vars.rFW.get_interface<FIMainApp>   (vars.mainCtx);
        auto       &appContexts   = vars.rFW.data_get<AppContexts>      (mainApp.di.appContexts);
        auto const windowApp      = vars.rFW.get_interface<FIWindowApp> (appContexts.window);
        auto const magnum         = vars.rFW.get_interface<FIMagnum>    (appContexts.window);
        auto       &rMainLoopCtrl = vars.rFW.data_get<MainLoopControl&> (mainApp.di.mainLoopCtrl);
        auto       &rFWModify     = vars.rFW.data_get<FrameworkModify&> (mainApp.di.frameworkModify);
        auto       &rMagnumApp    = vars.rFW.data_get<MagnumWindowApp>  (magnum.di.magnumApp);

        bool stopMainLoop = false;
        bool closeRenderer = false;

        if (rFWModify.commands.empty())
        {
            CommonMagnumApp &rCommonMagnumApp = *static_cast<CommonMagnumApp*>(rMagnumApp.m_events.get());
            rCommonMagnumApp.mainContext    = vars.mainCtx;
            rCommonMagnumApp.pFW            = &vars.rFW;
            rCommonMagnumApp.pExec          = &vars.rExecutor;
            rCommonMagnumApp.pMainLoopCtrl  = &rMainLoopCtrl;

            bool const stayOpen = rMagnumApp.mainLoopIteration();

            if (!stayOpen)
            {
                stopMainLoop  = true;
                closeRenderer = true;
            }
        }
        else
        {
            stopMainLoop = true;
        }

        if (stopMainLoop)
        {

            auto &rWindowLoopCtrl = vars.rFW.data_get<WindowAppLoopControl>   (windowApp.di.windowAppLoopCtrl);

            rWindowLoopCtrl.doRender = false;
            rWindowLoopCtrl.doResync = false;
            rWindowLoopCtrl.doSync   = false;

            while (!rMainLoopCtrl.mainScheduleWaiting)
            {
                vars.rExecutor.wait(vars.rFW);

                if (rMainLoopCtrl.keepOpenWaiting)
                {
                    rMainLoopCtrl.keepOpenWaiting = false;
                    vars.rExecutor.task_finish(vars.rFW, mainApp.tasks.keepOpen, true, {.cancel = true});
                    vars.rExecutor.wait(vars.rFW);
                }
            }

            if (vars.rExecutor.is_running(vars.rFW, mainApp.loopblks.mainLoop))
            {
                OSP_LOG_CRITICAL("something is blocking the framework main loop from exiting. RIP");
                std::abort();
            }

            run_cleanup(appContexts.sceneRender, vars.rFW, vars.rExecutor);
            run_cleanup(appContexts.window, vars.rFW, vars.rExecutor);
            vars.rFW.close_context(appContexts.sceneRender);
            vars.rFW.close_context(appContexts.window);
            appContexts.sceneRender = {};
            appContexts.window      = {};

            //if ( ! rFWModify.commands.empty() )
            for (FrameworkModify::Command &rCmd : rFWModify.commands)
            {
                rCmd.func(vars.rFW, rCmd.mainCtx, std::move(rCmd.userData));
            }
            rFWModify.commands.clear();

            // Restart framework main loop
            vars.rExecutor.load(vars.rFW);
            LGRN_ASSERT(rMainLoopCtrl.mainScheduleWaiting);
            vars.rExecutor.task_finish(vars.rFW, mainApp.tasks.schedule, true, {.cancel = false});
            rMainLoopCtrl.mainScheduleWaiting = false;

            return false;
        }

        return true;
        /*Framework &rFW              = vars.rFW;
        auto const mainApp          = rFW.get_interface<FIMainApp>        (rTestApp.m_mainContext);

        ContextId const sceneRenderCtx = rFW.data_get<AppContexts>(mainApp.di.appContexts).sceneRender;

        auto &rFWModify = rFW.data_get<FrameworkModify>(mainApp.di.frameworkModify);
        if (init)
        {
            init = false;
            // Resynchronize renderer; Resync+Sync without stepping through time.
            // This makes sure meshes, textures, shaders, and other GPU-related resources specified by
            // the scene are properly loaded and assigned to entities within the renderer.
//            rTestApp.drive_scene_cycle({.deltaTimeIn = 0.0f,
//                                  .update      = true,
//                                  .sceneUpdate = false,
//                                  .resync      = true,
//                                  .sync        = true,
//                                  .render      = false });
        }
        else if ( rFWModify.commands.empty() )
        {
            // calls CommonMagnumApp::draw()
            auto const magnum           = rFW.get_interface<FIMagnum>         (windowCtx);
            auto       &rMagnumApp      = rFW.data_get<MagnumWindowApp>       (magnum.di.magnumApp);

            bool stayOpen = rMagnumApp.mainLoopIteration();

            if ( ! stayOpen )
            {
                // Clear up some queues and stuff by update a few times without time
                for (int i = 0; i < 2; ++i)
                {
                    rTestApp.drive_scene_cycle({.deltaTimeIn = 0.0f,
                                      .update      = true,
                                      .sceneUpdate = true,
                                      .resync      = false,
                                      .sync        = false,
                                      .render      = false });
                }

                // Stops the pipeline loop
                rTestApp.drive_scene_cycle({.deltaTimeIn = 0.0f,
                                  .update      = false,
                                  .sceneUpdate = false,
                                  .resync      = false,
                                  .sync        = false,
                                  .render      = false });


                if (rTestApp.m_pExecutor->is_running(rTestApp.m_framework))
                {
                    OSP_LOG_CRITICAL("Expected main loop to stop, but something is blocking it and cannot exit");
                    std::abort();
                }

                rTestApp.run_context_cleanup(sceneRenderCtx);
                rTestApp.run_context_cleanup(windowCtx);
                rTestApp.m_framework.close_context(sceneRenderCtx);
                rTestApp.m_framework.close_context(windowCtx);

                // Reload and restart the main loop for the CLI
                rTestApp.m_pExecutor->load(rTestApp.m_framework);
                rTestApp.m_pExecutor->run(rTestApp.m_framework, mainApp.pl.mainLoop);

                rFW.data_get<AppContexts>(mainApp.di.appContexts).window = {};

                return false;
            }
        }
        else
        {
            // Clear up some queues and stuff by update a few times without time
            for (int i = 0; i < 2; ++i)
            {
                rTestApp.drive_scene_cycle({.deltaTimeIn = 0.0f,
                                  .update      = true,
                                  .sceneUpdate = true,
                                  .resync      = false,
                                  .sync        = false,
                                  .render      = false });
            }

            // Stops the pipeline loop
            rTestApp.drive_scene_cycle({.deltaTimeIn = 0.0f,
                              .update      = false,
                              .sceneUpdate = false,
                              .resync      = false,
                              .sync        = false,
                              .render      = false });

            rTestApp.run_context_cleanup(sceneRenderCtx);
            rTestApp.m_framework.close_context(sceneRenderCtx);

            // run commands
            if (rTestApp.m_pExecutor->is_running(rFW))
            {
                OSP_LOG_CRITICAL("something is blocking the framework main loop from exiting. RIP");
                std::abort();
            }

            for (FrameworkModify::Command &rCmd : rFWModify.commands)
            {
                rCmd.func(rFW, rCmd.ctx, std::move(rCmd.userData));
            }
            rFWModify.commands.clear();


            ContextId const newSceneCtx = rFW.data_get<AppContexts>(mainApp.di.appContexts).scene;

            ContextId const newSceneRenderCtx = make_scene_renderer(rTestApp, newSceneCtx, windowCtx);

            rFW.data_get<AppContexts>(mainApp.di.appContexts).sceneRender = newSceneRenderCtx;

            // Restart framework main loop
            rTestApp.m_pExecutor->load(rFW);
            rTestApp.m_pExecutor->run(rFW, mainApp.pl.mainLoop);

            // Resync renderer
            rTestApp.drive_scene_cycle({.deltaTimeIn = 0.0f,
                      .update      = true,
                      .sceneUpdate = false,
                      .resync      = true,
                      .sync        = true,
                      .render      = false });
        }

        */

    });
}



} // namespace testapp
