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

#include "scenarioUtils.h"
#include "Magnum/GL/DefaultFramebuffer.h"

#include "osp/drawing/drawing.h"

#include "testapp/MagnumApplication.h"
#include "testapp/scenarios.h"

namespace testapp
{

using namespace osp;

struct MainLoopSignals
{
    PipelineId mainLoop;
    PipelineId inputs;
    PipelineId renderSync;
    PipelineId renderResync;
    PipelineId sceneUpdate;
    PipelineId sceneRender;
};

class CommonMagnumApp : public IOspApplication
{
public:
    CommonMagnumApp(TestApp& rTestApp, MainLoopControl& rMainLoopCtrl, MainLoopSignals signals) noexcept
        : m_rTestApp{ rTestApp }
        , m_rMainLoopCtrl{ rMainLoopCtrl }
        , m_signals{ signals }
    { }

    void run(MagnumApplication& rApp) override
    {
        // Start the main loop

        PipelineId const mainLoop = m_rTestApp.m_application.get_pipelines<PlApplication>().mainLoop;
        m_rTestApp.m_pExecutor->run(m_rTestApp, mainLoop);

        // Resyncronize renderer

        m_rMainLoopCtrl = MainLoopControl{
            .doUpdate = false,
            .doSync = true,
            .doResync = true,
            .doRender = false,
        };

        signal_all();

        m_rTestApp.m_pExecutor->wait(m_rTestApp);
    }

    void draw(MagnumApplication& rApp, float delta) override
    {
        // Magnum Application's main loop calls this

        m_rMainLoopCtrl = MainLoopControl{
            .doUpdate = true,
            .doSync = true,
            .doResync = false,
            .doRender = true,
        };

        signal_all();

        m_rTestApp.m_pExecutor->wait(m_rTestApp);
    }

    void exit(MagnumApplication& rApp) override
    {
        m_rMainLoopCtrl = MainLoopControl{
            .doUpdate = false,
            .doSync = false,
            .doResync = false,
            .doRender = false,
        };

        signal_all();

        m_rTestApp.m_pExecutor->wait(m_rTestApp);

        if (m_rTestApp.m_pExecutor->is_running(m_rTestApp))
        {
            // Main loop must have stopped, but didn't!
            m_rTestApp.m_pExecutor->wait(m_rTestApp);
            std::abort();
        }
    }

private:

    void signal_all()
    {
        m_rTestApp.m_pExecutor->signal(m_rTestApp, m_signals.mainLoop);
        m_rTestApp.m_pExecutor->signal(m_rTestApp, m_signals.inputs);
        m_rTestApp.m_pExecutor->signal(m_rTestApp, m_signals.renderSync);
        m_rTestApp.m_pExecutor->signal(m_rTestApp, m_signals.renderResync);
        m_rTestApp.m_pExecutor->signal(m_rTestApp, m_signals.sceneUpdate);
        m_rTestApp.m_pExecutor->signal(m_rTestApp, m_signals.sceneRender);
    }

    TestApp& m_rTestApp;
    MainLoopControl& m_rMainLoopCtrl;

    MainLoopSignals m_signals;
};


void setup_magnum_draw(TestApp& rTestApp, Session const& scene, Session const& sceneRenderer, Session const& magnumScene)
{
    OSP_DECLARE_GET_DATA_IDS(rTestApp.m_application, TESTAPP_DATA_APPLICATION);
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer, TESTAPP_DATA_SCENE_RENDERER);
    OSP_DECLARE_GET_DATA_IDS(rTestApp.m_magnum, TESTAPP_DATA_MAGNUM);
    OSP_DECLARE_GET_DATA_IDS(magnumScene, TESTAPP_DATA_MAGNUM_SCENE);

    auto& rMainLoopCtrl = top_get<MainLoopControl>(rTestApp.m_topData, idMainLoopCtrl);
    auto& rActiveApp = top_get<MagnumApplication>(rTestApp.m_topData, idActiveApp);
    auto& rCamera = top_get<draw::Camera>(rTestApp.m_topData, idCamera);

    rCamera.set_aspect_ratio(Vector2{ Magnum::GL::defaultFramebuffer.viewport().size() });

    MainLoopSignals const signals
    {
        .mainLoop = rTestApp.m_application.get_pipelines<PlApplication>().mainLoop,
        .inputs = rTestApp.m_windowApp.get_pipelines<PlWindowApp>().inputs,
        .renderSync = rTestApp.m_windowApp.get_pipelines<PlWindowApp>().sync,
        .renderResync = rTestApp.m_windowApp.get_pipelines<PlWindowApp>().resync,
        .sceneUpdate = scene.get_pipelines<PlScene>().update,
        .sceneRender = sceneRenderer.get_pipelines<PlSceneRenderer>().render,
    };

    rActiveApp.set_osp_app(std::make_unique<CommonMagnumApp>(rTestApp, rMainLoopCtrl, signals));
}

} // namespace testapp