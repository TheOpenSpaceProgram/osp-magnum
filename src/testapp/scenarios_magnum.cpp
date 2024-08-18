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
#include <osp/util/UserInputHandler.h>

using osp::input::UserInputHandler;
using namespace adera;

using namespace ftr_inter;

using namespace osp::fw;

namespace testapp
{



struct MainLoopSignals
{
    osp::PipelineId mainLoop;
    osp::PipelineId inputs;
    osp::PipelineId renderSync;
    osp::PipelineId renderResync;
    osp::PipelineId sceneUpdate;
    osp::PipelineId sceneRender;
};

/**
 * @brief Runs Task/Pipeline main loop within MagnumWindowApp
 */
class CommonMagnumApp : public IOspApplication
{
public:
    CommonMagnumApp(TestApp &rTestApp, MainLoopControl &rMainLoopCtrl, MainLoopSignals signals) noexcept
     : m_rTestApp       { rTestApp }
     , m_rMainLoopCtrl  { rMainLoopCtrl }
     , m_signals        { signals }
    { }

    void run(MagnumWindowApp& rApp) override
    {
        // Start the main loop

//        PipelineId const mainLoop = m_rTestApp.m_application.get_pipelines<PlApplication>().mainLoop;
//        m_rTestApp.m_pExecutor->run(m_rTestApp, mainLoop);

//        // Resyncronize renderer

//        m_rMainLoopCtrl = MainLoopControl{
//            .doUpdate = false,
//            .doSync   = true,
//            .doResync = true,
//            .doRender = false,
//        };

//        signal_all();

//        m_rTestApp.m_pExecutor->wait(m_rTestApp);
    }

    void draw(MagnumWindowApp& rApp, float delta) override
    {
        // Magnum Application's main loop calls this
        m_rTestApp.drive_main_loop();

//        auto const magnum    = g_testApp->m_framework.get_interface<FIMagnum>   (g_testApp->m_mainContext);
//        auto       &rRenderGl = entt::any_cast<osp::draw::RenderGL&>  (g_testApp->m_framework.data[magnum.di.renderGl]);


//        Magnum::GL::defaultFramebuffer.bind();
//        auto &rFboColor = rRenderGl.m_texGl.get(rRenderGl.m_fboColor);
//        osp::draw::SysRenderGL::display_texture(rRenderGl, rFboColor);

//        m_rMainLoopCtrl = MainLoopControl{
//            .doUpdate = true,
//            .doSync   = true,
//            .doResync = false,
//            .doRender = true,
//        };

//        signal_all();

//        m_rTestApp.m_pExecutor->wait(m_rTestApp);
    }

    void exit(MagnumWindowApp& rApp) override
    {
//        m_rMainLoopCtrl = MainLoopControl{
//            .doUpdate = false,
//            .doSync   = false,
//            .doResync = false,
//            .doRender = false,
//        };

//        signal_all();

//        m_rTestApp.m_pExecutor->wait(m_rTestApp);

//        if (m_rTestApp.m_pExecutor->is_running(m_rTestApp))
//        {
//            // Main loop must have stopped, but didn't!
//            m_rTestApp.m_pExecutor->wait(m_rTestApp);
//            std::abort();
//        }
    }

private:

    void signal_all()
    {
//        m_rTestApp.m_pExecutor->signal(m_rTestApp, m_signals.mainLoop);
//        m_rTestApp.m_pExecutor->signal(m_rTestApp, m_signals.inputs);
//        m_rTestApp.m_pExecutor->signal(m_rTestApp, m_signals.renderSync);
//        m_rTestApp.m_pExecutor->signal(m_rTestApp, m_signals.renderResync);
//        m_rTestApp.m_pExecutor->signal(m_rTestApp, m_signals.sceneUpdate);
//        m_rTestApp.m_pExecutor->signal(m_rTestApp, m_signals.sceneRender);
    }

    TestApp         &m_rTestApp;
    MainLoopControl &m_rMainLoopCtrl;
    MainLoopSignals m_signals;
};



void setup_magnum_renderer(
        TestApp                                 &rTestApp,
        FInterfaceShorthand<FIMainApp>    const mainApp,
        AppContexts                       const &rAppCtxs,
        FInterfaceShorthand<FIWindowApp>  const windowApp,
        FInterfaceShorthand<FIMagnum>     const magnum,
        MagnumWindowApp                         &rMagnumApp)
{
    Framework &rFW = rTestApp.m_framework;

    auto &rMainLoopCtrl = entt::any_cast<MainLoopControl&>      (rFW.data[mainApp.di.mainLoopCtrl]);
    auto &rUserInput    = entt::any_cast<UserInputHandler&>     (rFW.data[windowApp.di.userInput]);
    auto &rRenderGL     = entt::any_cast<osp::draw::RenderGL&>  (rFW.data[magnum.di.renderGl]);

    if (auto engineTest = rFW.get_interface<FIEngineTest>(rAppCtxs.scene);
        engineTest.id.has_value())
    {
        auto &rEngineTest = entt::any_cast<enginetest::EngineTestScene&>(rFW.data[engineTest.di.bigStruct]);

        // This creates the renderer actually updates and draws the scene.
        rMagnumApp.set_osp_app(enginetest::generate_osp_magnum_app(rEngineTest, rMagnumApp, rRenderGL, rUserInput));
        return;
    }


    MainLoopSignals const signals
    {
        .mainLoop     = mainApp  .pl.mainLoop,
        .inputs       = windowApp.pl.inputs,
        .renderSync   = windowApp.pl.sync,
        .renderResync = windowApp.pl.resync,
        //.sceneUpdate  = scene                  .get_pipelines<PlScene>()         .update,
        //.sceneRender  = sceneRenderer          .get_pipelines<PlSceneRenderer>() .render,
    };


    rMagnumApp.set_osp_app( std::make_unique<CommonMagnumApp>(rTestApp, rMainLoopCtrl, signals) );
}



void start_magnum_renderer(Framework &rFW, ContextId ctx, entt::any userData)
{
    std::cout << "fish :3\n";

    TestApp    &rTestApp = entt::any_cast<TestApp&>(userData);
    auto const mainApp   = rFW.get_interface<FIMainApp>(ctx);
    auto       &rAppCtxs = entt::any_cast<AppContexts&>  (rFW.data[mainApp.di.appContexts]);

    rAppCtxs.window = rFW.contextIds.create();

    ContextBuilder   cb { .m_ctxScope = { ctx }, .m_ctx = rAppCtxs.window, .m_rFW = rFW };
    cb.add_feature(adera::ftrWindowApp);
    cb.add_feature(ftrMagnum, MagnumWindowApp::Arguments{rTestApp.m_argc, rTestApp.m_argv});
    LGRN_ASSERTM(cb.m_errors.empty(), "Error adding main feature");
    ContextBuilder::apply(std::move(cb));

    main_loop_stack().push_back([&rTestApp] () -> bool {

        Framework &rFW     = rTestApp.m_framework;

        auto const mainApp        = rFW.get_interface<FIMainApp>(rTestApp.m_mainContext);
        auto       &rAppCtxs      = entt::any_cast<AppContexts&>  (rFW.data[mainApp.di.appContexts]);
        auto const windowApp      = rFW.get_interface<FIWindowApp>(rAppCtxs.window);
        auto const magnum         = rFW.get_interface<FIMagnum>   (rAppCtxs.window);
        auto       &rMagnumApp    = entt::any_cast<MagnumWindowApp&>(rFW.data[magnum.di.magnumApp]);

        setup_magnum_renderer(rTestApp, mainApp, rAppCtxs, windowApp, magnum, rMagnumApp);

        // Blocking. Main loop runs in MagnumWindowApp::drawEvent continues when the close button is pressed
        rMagnumApp.exec();

        rMagnumApp.set_osp_app({});

        // Destructing MagnumWindowApp will close the window
        rFW.data[magnum.di.magnumApp].reset();

        return false;
    });
}



} // namespace testapp
