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

#include <osp/util/UserInputHandler.h>

#include <Magnum/GL/DefaultFramebuffer.h>

using osp::input::UserInputHandler;
using namespace adera;
using namespace ftr_inter;
using namespace osp::draw;
using namespace osp::fw;
using namespace osp;

namespace testapp
{

/**
 * @brief Runs Task/Pipeline main loop within MagnumWindowApp
 */
class CommonMagnumApp : public IOspApplication
{

    struct UpdateParams {
        float deltaTimeIn;
        bool update;
        bool sceneUpdate;
        bool resync;
        bool sync;
        bool render;
    };

public:

    CommonMagnumApp(TestApp &rTestApp) noexcept : m_rTestApp{rTestApp} { }

    void run(MagnumWindowApp& rApp) override
    {
        // Called once on window startup

        // Resynchronize renderer; Resync+Sync without stepping through time.
        // This makes sure meshes, textures, shaders, and other GPU-related resources specified by
        // the scene are properly loaded and assigned to entities within the renderer.

        run_update_cycle({.deltaTimeIn = 0.0f,
                          .update      = true,
                          .sceneUpdate = false,
                          .resync      = true,
                          .sync        = true,
                          .render      = false });
    }

    void draw(MagnumWindowApp& rApp, float delta) override
    {
        // Called repeatedly by Magnum's main loop

        // TODO: m.rTestApp.run_fw_modify_commands();

        run_update_cycle({.deltaTimeIn = 1.0f/60.0f,
                          .update      = true,
                          .sceneUpdate = true,
                          .resync      = false,
                          .sync        = true,
                          .render      = true });
    }

    void exit(MagnumWindowApp& rApp) override
    {
        // Clear up some queues and stuff by update a few times without time
        for (int i = 0; i < 2; ++i)
        {
            run_update_cycle({.deltaTimeIn = 0.0f,
                              .update      = true,
                              .sceneUpdate = true,
                              .resync      = false,
                              .sync        = false,
                              .render      = false });
        }

        // Stops the pipeline loop
        run_update_cycle({.deltaTimeIn = 0.0f,
                          .update      = false,
                          .sceneUpdate = false,
                          .resync      = false,
                          .sync        = false,
                          .render      = false });


        if (m_rTestApp.m_pExecutor->is_running(m_rTestApp.m_framework))
        {
            OSP_LOG_CRITICAL("Expected main loop to stop, but something is blocking it and cannot exit");
            std::abort();
        }
    }

private:

    void run_fw_modify_commands()
    {
        Framework &rFW = m_rTestApp.m_framework;

        auto const mainApp    = rFW.get_interface<FIMainApp>  (m_rTestApp.m_mainContext);
        auto       &rFWModify = rFW.data_get<FrameworkModify&>(mainApp.di.frameworkModify);

        if ( ! rFWModify.commands.empty() )
        {

        }
        rFWModify.commands.clear();

        // Restart framework main loop
        m_rTestApp.m_pExecutor->load(m_rTestApp.m_framework);
        m_rTestApp.m_pExecutor->run(m_rTestApp.m_framework, mainApp.pl.mainLoop);
    }

    void run_update_cycle(UpdateParams p)
    {
        Framework &rFW = m_rTestApp.m_framework;

        auto const mainApp          = rFW.get_interface<FIMainApp>  (m_rTestApp.m_mainContext);
        auto const &rAppCtxs        = rFW.data_get<AppContexts>     (mainApp.di.appContexts);
        auto       &rMainLoopCtrl   = rFW.data_get<MainLoopControl> (mainApp.di.mainLoopCtrl);
        rMainLoopCtrl.doUpdate      = p.update;

        auto const scene            = rFW.get_interface<FIScene>    (rAppCtxs.scene);
        auto       &rSceneLoopCtrl  = rFW.data_get<SceneLoopControl>(scene.di.loopControl);
        auto       &rDeltaTimeIn    = rFW.data_get<float>           (scene.di.deltaTimeIn);
        rSceneLoopCtrl.doSceneUpdate = p.sceneUpdate;
        rDeltaTimeIn                = p.deltaTimeIn;

        auto const windowApp        = rFW.get_interface<FIWindowApp>      (rAppCtxs.window);
        auto       &rWindowLoopCtrl = rFW.data_get<WindowAppLoopControl>  (windowApp.di.windowAppLoopCtrl);
        rWindowLoopCtrl.doRender    = p.render;
        rWindowLoopCtrl.doSync      = p.sync;
        rWindowLoopCtrl.doResync    = p.resync;

        m_rTestApp.m_pExecutor->signal(m_rTestApp.m_framework, mainApp.pl.mainLoop);
        m_rTestApp.m_pExecutor->signal(m_rTestApp.m_framework, windowApp.pl.inputs);
        m_rTestApp.m_pExecutor->signal(m_rTestApp.m_framework, windowApp.pl.sync);
        m_rTestApp.m_pExecutor->signal(m_rTestApp.m_framework, windowApp.pl.resync);

        m_rTestApp.m_pExecutor->wait(m_rTestApp.m_framework);

    }

    TestApp &m_rTestApp;
};

void add_renderer_features(ContextBuilder &rWindowCB, ContextId sceneCtx, ContextId windowCtx, TestApp &rTestApp)
{
    Framework &rFW              = rTestApp.m_framework;
    auto const mainApp          = rFW.get_interface<FIMainApp>        (rTestApp.m_mainContext);
    auto const windowApp        = rFW.get_interface<FIWindowApp>      (windowCtx);
    auto const magnum           = rFW.get_interface<FIMagnum>         (windowCtx);
    auto       &rMagnumApp      = rFW.data_get<MagnumWindowApp>       (magnum.di.magnumApp);

    // Choose which renderer features to use based on information on which features the scene
    // context contains.

    auto const engineTest       = rFW.get_interface<FIEngineTest>(sceneCtx);
    auto const scene            = rFW.get_interface<FIScene>(sceneCtx);

    if (engineTest.id.has_value())
    {
        auto &rEngineTest = rFW.data_get<enginetest::EngineTestScene&>(engineTest.di.bigStruct);

        auto       &rUserInput      = rFW.data_get<UserInputHandler>      (windowApp.di.userInput);
        auto       &rRenderGL       = rFW.data_get<draw::RenderGL>        (magnum.di.renderGl);

        // This creates the renderer actually updates and draws the scene.
        rMagnumApp.set_osp_app(enginetest::generate_osp_magnum_app(rEngineTest, rMagnumApp, rRenderGL, rUserInput));
        return;
    }

    rWindowCB.add_feature(ftrSceneRenderer);
    rWindowCB.add_feature(ftrMagnumScene);

    auto scnRender      = rFW.get_interface<FISceneRenderer>        (windowCtx);
    auto magnumScn      = rFW.get_interface<FIMagnumScene>          (windowCtx);
    auto &rScnRender    = rFW.data_get<draw::ACtxSceneRender>       (scnRender.di.scnRender);
    auto &rCamera       = rFW.data_get<draw::Camera>                (magnumScn.di.camera);
    MaterialId const matFlat        = rScnRender.m_materialIds.create();
    MaterialId const matPhong       = rScnRender.m_materialIds.create();
    MaterialId const matVisualizer  = rScnRender.m_materialIds.create();
    rScnRender.m_materials.resize(rScnRender.m_materialIds.size());
    rCamera.set_aspect_ratio(Vector2{Magnum::GL::defaultFramebuffer.viewport().size()});

    rWindowCB.add_feature(ftrCameraControl);

    if (rFW.get_interface_id<FIPhysShapes>(sceneCtx).has_value())
    {
        rWindowCB.add_feature(ftrPhysicsShapesDraw, matPhong);
        rWindowCB.add_feature(ftrThrower);
    }

    rWindowCB.add_feature(ftrShaderPhong,        matPhong);
    rWindowCB.add_feature(ftrShaderFlat,         matFlat);
    rWindowCB.add_feature(ftrShaderVisualizer,   matVisualizer);

    rWindowCB.add_feature(ftrCursor, TplPkgIdMaterialId{ rTestApp.m_defaultPkg, matFlat });

    if (rFW.get_interface_id<FIPrefabs>(sceneCtx).has_value())
    {
        rWindowCB.add_feature(ftrPrefabDraw, matPhong);
    }

    if (rFW.get_interface_id<FIVehicleSpawn>(sceneCtx).has_value())
    {
        rWindowCB.add_feature(ftrVehicleControl);
        rWindowCB.add_feature(ftrVehicleCamera);
        rWindowCB.add_feature(ftrVehicleSpawnDraw);
    }
    else
    {
        rWindowCB.add_feature(ftrCameraFree);
    }

    if (rFW.get_interface_id<FIUniPlanets>(sceneCtx).has_value())
    {
        rWindowCB.add_feature(ftrUniverseTestPlanetsDraw, PlanetDrawParams{
            .planetMat = matVisualizer,
            .axisMat   = matFlat });
    }

    if (rFW.get_interface_id<FISolarSys>(sceneCtx).has_value())
    {
        rWindowCB.add_feature(ftrSolarSystemDraw, PlanetDrawParams{
            .planetMat = matFlat
        });

        // Zoom out the camera so that all planets are in view
        auto scnRender      = rFW.get_interface<FICameraControl>    (rWindowCB.m_ctx);
        auto &rCamCtrl      = rFW.data_get<ACtxCameraController>    (scnRender.di.camCtrl);
        rCamCtrl.m_orbitDistance += 75000;
    }

    if (rFW.get_interface_id<FIRocketsJolt>(sceneCtx).has_value())
    {
        rWindowCB.add_feature(ftrMagicRocketThrustIndicator, TplPkgIdMaterialId{ rTestApp.m_defaultPkg, matFlat });
    }

    if ( ! rWindowCB.has_error() && rFW.get_interface_id<FITerrain>(sceneCtx).has_value() )
    {
        rWindowCB.add_feature(ftrTerrainDebugDraw, matVisualizer);
        rWindowCB.add_feature(ftrTerrainDrawMagnum);

        auto scnRender      = rFW.get_interface<FICameraControl>    (rWindowCB.m_ctx);
        auto &rCamCtrl      = rFW.data_get<ACtxCameraController>    (scnRender.di.camCtrl);

        rCamCtrl.m_target = Vector3(0.0f, 0.0f, 0.0f);
        rCamCtrl.m_orbitDistanceMin = 1.0f;
        rCamCtrl.m_moveSpeed = 0.5f;
    }

    rMagnumApp.set_osp_app( std::make_unique<CommonMagnumApp>(rTestApp) );

}

void start_magnum_renderer(Framework &rFW, ContextId ctx, entt::any userData)
{
    TestApp    &rTestApp = entt::any_cast<TestApp&>(userData);
    auto const mainApp   = rFW.get_interface<FIMainApp>(ctx);
    auto       &rAppCtxs = rFW.data_get<AppContexts>(mainApp.di.appContexts);

    ContextId const sceneCtx  = rAppCtxs.scene;
    ContextId const windowCtx = rFW.m_contextIds.create();
    rAppCtxs.window = windowCtx;

    ContextBuilder  windowCB { windowCtx, { ctx, sceneCtx }, rFW };
    windowCB.add_feature(adera::ftrWindowApp);

    // Adding this feature will open the GUI window
    windowCB.add_feature(ftrMagnum, MagnumWindowApp::Arguments{rTestApp.m_argc, rTestApp.m_argv});

    add_renderer_features(windowCB, sceneCtx, windowCtx, rTestApp);

    bool const success = ContextBuilder::finalize(std::move(windowCB));

    if ( success )
    {
        main_loop_stack().push_back(
                [&rTestApp, windowCtx,
                 plMainLoop = mainApp.pl.mainLoop] () -> bool
        {
            Framework &rFW              = rTestApp.m_framework;
            auto const magnum           = rFW.get_interface<FIMagnum>         (windowCtx);
            auto       &rMagnumApp      = rFW.data_get<MagnumWindowApp>       (magnum.di.magnumApp);

            // Blocking. Main loop runs in MagnumWindowApp::drawEvent() / CommonMagnumApp::draw()
            // continues when the close button is pressed
            rMagnumApp.exec();

            // Destruct CommonMagnumApp or EngineTestApp
            rMagnumApp.set_osp_app({});

            auto const cleanup = rFW.get_interface<FICleanupContext> (windowCtx);
            if ( cleanup.id.has_value() )
            {
                // Run cleanup pipeline for the window context

                rTestApp.m_pExecutor->run(rTestApp.m_framework, cleanup.pl.cleanup);
                rTestApp.m_pExecutor->wait(rTestApp.m_framework);

                if (rTestApp.m_pExecutor->is_running(rTestApp.m_framework))
                {
                    OSP_LOG_CRITICAL("Deadlock in cleanup pipeline");
                    std::abort();
                }
            }

            // Steal the entt::any (internally a void*) for MagnumWindowApp out of the framework
            // as the GL context is tied to MagnumWindowApp's lifetime. GL context is still needed
            // to properly destruct other GPU resources in the framework. close_context will
            // destruct them in an unspecified order otherwise, leading to errors.
            entt::any magnumAppAny = std::exchange(rTestApp.m_framework.m_data[magnum.di.magnumApp], {});

            rTestApp.m_framework.close_context(windowCtx);

            magnumAppAny.reset();

            // Reload and restart the main loop for the CLI
            rTestApp.m_pExecutor->load(rTestApp.m_framework);
            rTestApp.m_pExecutor->run(rTestApp.m_framework, plMainLoop);

            return false;
        });
    }
}



} // namespace testapp
