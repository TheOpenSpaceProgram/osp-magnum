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

#include <osp/util/logging.h>
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
        scnRdrCB.add_feature(ftrThrower);
    }

    scnRdrCB.add_feature(ftrShaderPhong,        matPhong);
    scnRdrCB.add_feature(ftrShaderFlat,         matFlat);
    scnRdrCB.add_feature(ftrShaderVisualizer,   matVisualizer);

    scnRdrCB.add_feature(ftrCursor, TplPkgIdMaterialId{ defaultPkg, matFlat });

    if (rFW.get_interface_id<FIPrefabs>(sceneCtx).has_value())
    {
        scnRdrCB.add_feature(ftrPrefabDraw, matPhong);
    }

    if (rFW.get_interface_id<FIVehicleSpawn>(sceneCtx).has_value())
    {
        scnRdrCB.add_feature(ftrVehicleControl);
        scnRdrCB.add_feature(ftrVehicleCamera);
        scnRdrCB.add_feature(ftrVehicleSpawnDraw);
        scnRdrCB.add_feature(ftrCameraFree);
    }
    else
    {
        scnRdrCB.add_feature(ftrCameraFree);
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
    }
/*
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

*/

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
    Framework       *pFW            {nullptr};
    IExecutor       *pExec          {nullptr};
    MainLoopControl *pMainLoopCtrl  {nullptr};
};


void FWMCStartMagnumRenderer::run(Framework &rFW)
{
    auto const mainApp   = rFW.get_interface<FIMainApp>(m_mainCtx);

    ContextId const sceneCtx  = rFW.data_get<AppContexts>(mainApp.di.appContexts).scene;
    ContextId const windowCtx = rFW.m_contextIds.create();

    ContextBuilder  windowCB { windowCtx, { m_mainCtx, sceneCtx }, rFW };
    windowCB.add_feature(adera::ftrCleanupCtx);
    windowCB.add_feature(adera::ftrWindowApp);

    // Adding this feature will open the GUI window
    windowCB.add_feature(ftrMagnum, MagnumWindowApp::Arguments{m_argc, m_argv});

    ContextBuilder::finalize(std::move(windowCB));

    ContextId const sceneRenderCtx = make_scene_renderer(rFW, m_defaultPkg, m_mainCtx, sceneCtx, windowCtx);

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
}


IMainLoopFunc::Status MagnumMainLoop::run(osp::fw::Framework &rFW, osp::fw::IExecutor &rExecutor)
{
    IMainLoopFunc::Status status;

    auto const mainApp        = rFW.get_interface<FIMainApp>   (m_mainCtx);
    auto       &appContexts   = rFW.data_get<AppContexts>      (mainApp.di.appContexts);
    auto const windowApp      = rFW.get_interface<FIWindowApp> (appContexts.window);
    auto const magnum         = rFW.get_interface<FIMagnum>    (appContexts.window);
    auto       &rMainLoopCtrl = rFW.data_get<MainLoopControl&> (mainApp.di.mainLoopCtrl);
    auto       &rWindowLoopCtrl = rFW.data_get<WindowAppLoopControl>  (windowApp.di.windowAppLoopCtrl);
    auto       &rFWModify     = rFW.data_get<FrameworkModify&> (mainApp.di.frameworkModify);
    auto       &rMagnumApp    = rFW.data_get<MagnumWindowApp>  (magnum.di.magnumApp);

    bool stopMainLoop = false;
    bool closeWindow  = false;

    if (rFWModify.commands.empty())
    {
        CommonMagnumApp &rCommonMagnumApp = *static_cast<CommonMagnumApp*>(rMagnumApp.m_events.get());
        rCommonMagnumApp.mainContext    = m_mainCtx;
        rCommonMagnumApp.pFW            = &rFW;
        rCommonMagnumApp.pExec          = &rExecutor;
        rCommonMagnumApp.pMainLoopCtrl  = &rMainLoopCtrl;

        bool const stayOpen = rMagnumApp.mainLoopIteration();

        if (!stayOpen)
        {
            stopMainLoop  = true;
            closeWindow  = true;
        }
    }
    else
    {
        stopMainLoop = true;
    }

    if (stopMainLoop)
    {
        auto &rWindowLoopCtrl = rFW.data_get<WindowAppLoopControl>   (windowApp.di.windowAppLoopCtrl);

        rWindowLoopCtrl.doRender = false;
        rWindowLoopCtrl.doResync = false;
        rWindowLoopCtrl.doSync   = false;

        while (!rMainLoopCtrl.mainScheduleWaiting)
        {
            rExecutor.wait(rFW);

            if (rMainLoopCtrl.keepOpenWaiting)
            {
                rMainLoopCtrl.keepOpenWaiting = false;
                rExecutor.task_finish(rFW, mainApp.tasks.keepOpen, true, {.cancel = true});
                rExecutor.wait(rFW);
            }
        }

        if (rExecutor.is_running(rFW, mainApp.loopblks.mainLoop))
        {
            OSP_LOG_CRITICAL("something is blocking the framework main loop from exiting. RIP");
            std::abort();
        }

        run_cleanup(appContexts.sceneRender, rFW, rExecutor);
        rFW.close_context(appContexts.sceneRender);
        appContexts.sceneRender = {};
    }

    if (closeWindow)
    {
        run_cleanup(appContexts.window, rFW, rExecutor);
        rFW.close_context(appContexts.window);
        appContexts.window = {};

        status.exit = true;
    }

    if (stopMainLoop)
    {
        for (std::unique_ptr<IFrameworkModifyCommand> &pCmd : rFWModify.commands)
        {
            pCmd->run(rFW);

            std::unique_ptr<IMainLoopFunc> newMainLoop = pCmd->main_loop();

            if (newMainLoop != nullptr)
            {
                LGRN_ASSERTM(status.pushNew == nullptr,
                             "multiple framework modify are fighting to add main loop function");
                status.pushNew = std::move(newMainLoop);
            }
        }
        rFWModify.commands.clear();

        if ( ! closeWindow )
        {
            appContexts.sceneRender = make_scene_renderer(rFW, m_defaultPkg, m_mainCtx, appContexts.scene, appContexts.window);
        }

        // Restart framework main loop
        rExecutor.load(rFW);
        LGRN_ASSERT(rMainLoopCtrl.mainScheduleWaiting);
        rExecutor.task_finish(rFW, mainApp.tasks.schedule, true, {.cancel = false});
        rMainLoopCtrl.mainScheduleWaiting = false;

        rWindowLoopCtrl.doRender = true;
        rWindowLoopCtrl.doResync = true;
        rWindowLoopCtrl.doSync   = true;
    }


    return status;
}

} // namespace testapp
