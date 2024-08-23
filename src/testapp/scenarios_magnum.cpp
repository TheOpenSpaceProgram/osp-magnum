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

#include "enginetest.h"
#include "feature_interfaces.h"

#include "features/magnum.h"
#include "MagnumWindowApp.h"

#include <adera_app/application.h>
#include <adera_app/features/common.h>
#include <adera_app/features/shapes.h>

#include <adera_app/features/misc.h>

#include <osp/util/UserInputHandler.h>

#include <Magnum/GL/DefaultFramebuffer.h>

using osp::input::UserInputHandler;
using namespace adera;
using namespace ftr_inter;
using namespace osp;
using namespace osp::fw;

namespace testapp
{

/**
 * @brief Runs Task/Pipeline main loop within MagnumWindowApp
 */
class CommonMagnumApp : public IOspApplication
{
public:

    struct Members
    {
        TestApp                 &rTestApp;
        MainLoopControl         &rMainLoopCtrl;
        WindowAppLoopControl    &rWindowLoopCtrl;
        SceneLoopControl        &rSceneLoopCtrl;

        PipelineId              mainLoop;
        PipelineId              inputs;
        PipelineId              renderSync;
        PipelineId              renderResync;
    };

    CommonMagnumApp(Members members) noexcept : m{std::move(members)} { }

    void run(MagnumWindowApp& rApp) override
    {
        // Called once on window startup

        // Resynchronize renderer; Resync+Sync without stepping through time.
        // This makes sure meshes, textures, shaders, and other GPU-related resources specified by
        // the scene are properly loaded and assigned to entities within the renderer.

        m.rMainLoopCtrl  .doUpdate      = true;
        m.rSceneLoopCtrl .doSceneUpdate = false;
        m.rWindowLoopCtrl.doResync      = true;
        m.rWindowLoopCtrl.doSync        = true;
        m.rWindowLoopCtrl.doRender      = false;
        signal_all();
        m.rTestApp.m_pExecutor->wait(m.rTestApp.m_framework);
    }

    void draw(MagnumWindowApp& rApp, float delta) override
    {
        // Called repeatedly by Magnum's main loop

        // TODO: m.rTestApp.run_fw_modify_commands();

        m.rMainLoopCtrl  .doUpdate      = true;
        m.rSceneLoopCtrl .doSceneUpdate = true;
        m.rWindowLoopCtrl.doResync      = false;
        m.rWindowLoopCtrl.doSync        = true;
        m.rWindowLoopCtrl.doRender      = true;
        signal_all();
        m.rTestApp.m_pExecutor->wait(m.rTestApp.m_framework);
    }

    void exit(MagnumWindowApp& rApp) override
    {
        // Stops the pipeline loop
        m.rMainLoopCtrl  .doUpdate      = false;
        m.rSceneLoopCtrl .doSceneUpdate = false;
        m.rWindowLoopCtrl.doResync      = false;
        m.rWindowLoopCtrl.doSync        = false;
        m.rWindowLoopCtrl.doRender      = false;
        signal_all();
        m.rTestApp.m_pExecutor->wait(m.rTestApp.m_framework);

        if (m.rTestApp.m_pExecutor->is_running(m.rTestApp.m_framework))
        {
            OSP_LOG_CRITICAL("Expected main loop to stop, but something is blocking it and cannot exit");
            std::abort();
        }
    }

private:

    void signal_all()
    {
        m.rTestApp.m_pExecutor->signal(m.rTestApp.m_framework, m.mainLoop);
        m.rTestApp.m_pExecutor->signal(m.rTestApp.m_framework, m.inputs);
        m.rTestApp.m_pExecutor->signal(m.rTestApp.m_framework, m.renderSync);
        m.rTestApp.m_pExecutor->signal(m.rTestApp.m_framework, m.renderResync);
    }

    Members m;
};


void start_magnum_renderer(Framework &rFW, ContextId ctx, entt::any userData)
{
    TestApp    &rTestApp = entt::any_cast<TestApp&>(userData);
    auto const mainApp   = rFW.get_interface<FIMainApp>(ctx);
    auto       &rAppCtxs = rFW.data_get<AppContexts&>(mainApp.di.appContexts);

    rAppCtxs.window = rFW.m_contextIds.create();

    ContextBuilder  windowCB { rAppCtxs.window, { ctx, rAppCtxs.scene }, rFW };
    windowCB.add_feature(adera::ftrWindowApp);

    // Adding this feature will open the GUI window
    windowCB.add_feature(ftrMagnum, MagnumWindowApp::Arguments{rTestApp.m_argc, rTestApp.m_argv});

    auto const windowApp        = rFW.get_interface<FIWindowApp>      (rAppCtxs.window);
    auto const magnum           = rFW.get_interface<FIMagnum>         (rAppCtxs.window);
    auto const cleanup          = rFW.get_interface<FICleanupContext> (rAppCtxs.window);
    auto       &rMagnumApp      = rFW.data_get<MagnumWindowApp>       (magnum.di.magnumApp);
    auto       &rMainLoopCtrl   = rFW.data_get<MainLoopControl>       (mainApp.di.mainLoopCtrl);
    auto       &rUserInput      = rFW.data_get<UserInputHandler>      (windowApp.di.userInput);
    auto       &rWindowLoopCtrl = rFW.data_get<WindowAppLoopControl>  (windowApp.di.windowAppLoopCtrl);
    auto       &rRenderGL       = rFW.data_get<draw::RenderGL>        (magnum.di.renderGl);

    // Choose which renderer features to use based on information on which features the scene
    // context contains.

    auto const engineTest       = rFW.get_interface<FIEngineTest>(rAppCtxs.scene);
    auto const scene            = rFW.get_interface<FIScene>(rAppCtxs.scene);

    bool const isEngineTest = engineTest.id.has_value();

    if (isEngineTest)
    {
        auto &rEngineTest = rFW.data_get<enginetest::EngineTestScene&>(engineTest.di.bigStruct);

        // This creates the renderer actually updates and draws the scene.
        rMagnumApp.set_osp_app(enginetest::generate_osp_magnum_app(rEngineTest, rMagnumApp, rRenderGL, rUserInput));
    }
    else if (scene.id.has_value())
    {
        windowCB.add_feature(ftrSceneRenderer);
        windowCB.add_feature(ftrMagnumScene);

        auto scnRender      = rFW.get_interface<FISceneRenderer>        (windowCB.m_ctx);
        auto magnumScn      = rFW.get_interface<FIMagnumScene>          (windowCB.m_ctx);
        auto &rScnRender    = rFW.data_get<draw::ACtxSceneRender&>      (scnRender.di.scnRender);
        auto &rCamera       = rFW.data_get<draw::Camera&>               (magnumScn.di.camera);

        auto const mat = rScnRender.m_materialIds.create();
        rScnRender.m_materials.resize(64);

        windowCB.add_feature(ftrPhysicsShapesDraw);
        windowCB.add_feature(ftrCameraControl);
        windowCB.add_feature(ftrCameraFree);
        windowCB.add_feature(ftrShaderPhong, mat);
        windowCB.add_feature(ftrCursor, TplPkgIdMaterialId{ rTestApp.m_defaultPkg, mat });
        windowCB.add_feature(ftrThrower);

        auto &rSceneLoopCtrl    = rFW.data_get<SceneLoopControl&>(scene.di.loopControl);
        rCamera.set_aspect_ratio(Vector2{Magnum::GL::defaultFramebuffer.viewport().size()});

        CommonMagnumApp::Members members
        {
            .rTestApp           = rTestApp,
            .rMainLoopCtrl      = rMainLoopCtrl,
            .rWindowLoopCtrl    = rWindowLoopCtrl,
            .rSceneLoopCtrl     = rSceneLoopCtrl,
            .mainLoop           = mainApp  .pl.mainLoop,
            .inputs             = windowApp.pl.inputs,
            .renderSync         = windowApp.pl.sync,
            .renderResync       = windowApp.pl.resync,
        };
        rMagnumApp.set_osp_app( std::make_unique<CommonMagnumApp>(std::move(members)) );
    }

    bool const success = ContextBuilder::finalize(std::move(windowCB));

    if ( success )
    {
        main_loop_stack().push_back(
                [&rTestApp, &rMagnumApp, isEngineTest,
                 magnumApp  = magnum.di.magnumApp,
                 plCleanup  = PipelineId(cleanup.pl.cleanup),
                 plMainLoop = mainApp.pl.mainLoop,
                 windowCtx  = rAppCtxs.window] () -> bool
        {
            // Blocking. Main loop runs in MagnumWindowApp::drawEvent() / CommonMagnumApp::draw()
            // continues when the close button is pressed
            rMagnumApp.exec();

            // Destruct CommonMagnumApp or EngineTestApp
            rMagnumApp.set_osp_app({});

            if ( ! isEngineTest )
            {
                // Run cleanup pipeline for the window context

                rTestApp.m_pExecutor->run(rTestApp.m_framework, plCleanup);
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
            entt::any magnumAppAny = std::exchange(rTestApp.m_framework.m_data[magnumApp], {});

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
