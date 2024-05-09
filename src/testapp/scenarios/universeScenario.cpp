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

#include "universeScenario.h"
#include "scenarioUtils.h"

#include "testapp/sessions/common.h"
#include "testapp/sessions/magnum.h"
#include "testapp/sessions/misc.h"
#include "testapp/sessions/newton.h"
#include "testapp/sessions/physics.h"
#include "testapp/sessions/shapes.h"
#include "testapp/sessions/universe.h"

namespace testapp
{

Scenario create_universe_scenario()
{
    using namespace osp;
    using namespace osp::active;

    Scenario universeScenario{};
    universeScenario.name = "universe";
    universeScenario.description = "Universe test scenario with very unrealistic planets";
    universeScenario.setupFunction = [](TestApp& rTestApp) -> RendererSetupFunc_t
    {
		#define SCENE_SESSIONS      scene, commonScene, physics, physShapes, droppers, bounds, newton, nwtGravSet, nwtGrav, physShapesNwt, uniCore, uniScnFrame, uniTestPlanets
		#define RENDERER_SESSIONS   sceneRenderer, magnumScene, cameraCtrl, cameraFree, shVisual, shFlat, shPhong, camThrow, shapeDraw, cursor, planetsDraw

        using namespace testapp::scenes;

        auto const  defaultPkg = rTestApp.m_defaultPkg;
        auto const  application = rTestApp.m_application;
        auto& rTopData = rTestApp.m_topData;

        TopTaskBuilder builder{ rTestApp.m_tasks, rTestApp.m_scene.m_edges, rTestApp.m_taskData };

        auto& [SCENE_SESSIONS] = resize_then_unpack<13>(rTestApp.m_scene.m_sessions);

        // Compose together lots of Sessions
        scene = setup_scene(builder, rTopData, application);
        commonScene = setup_common_scene(builder, rTopData, scene, application, defaultPkg);
        physics = setup_physics(builder, rTopData, scene, commonScene);
        physShapes = setup_phys_shapes(builder, rTopData, scene, commonScene, physics, sc_matPhong);
        droppers = setup_droppers(builder, rTopData, scene, commonScene, physShapes);
        bounds = setup_bounds(builder, rTopData, scene, commonScene, physShapes);

        newton = setup_newton(builder, rTopData, scene, commonScene, physics);
        nwtGravSet = setup_newton_factors(builder, rTopData);
        nwtGrav = setup_newton_force_accel(builder, rTopData, newton, nwtGravSet, Vector3{ 0.0f, 0.0f, -9.81f });
        physShapesNwt = setup_phys_shapes_newton(builder, rTopData, commonScene, physics, physShapes, newton, nwtGravSet);

        auto const tgApp = application.get_pipelines< PlApplication >();

        uniCore = setup_uni_core(builder, rTopData, tgApp.mainLoop);
        uniScnFrame = setup_uni_sceneframe(builder, rTopData, uniCore);
        uniTestPlanets = setup_uni_testplanets(builder, rTopData, uniCore, uniScnFrame);

        add_floor(rTopData, physShapes, sc_matVisualizer, defaultPkg, 0);

        RendererSetupFunc_t const setup_renderer = [](TestApp& rTestApp)
        {
            auto const  application = rTestApp.m_application;
            auto const  windowApp = rTestApp.m_windowApp;
            auto const  magnum = rTestApp.m_magnum;
            auto const  defaultPkg = rTestApp.m_defaultPkg;
            auto& rTopData = rTestApp.m_topData;

            TopTaskBuilder builder{ rTestApp.m_tasks, rTestApp.m_renderer.m_edges, rTestApp.m_taskData };

            auto& [SCENE_SESSIONS] = unpack<13>(rTestApp.m_scene.m_sessions);
            auto& [RENDERER_SESSIONS] = resize_then_unpack<11>(rTestApp.m_renderer.m_sessions);

            sceneRenderer = setup_scene_renderer(builder, rTopData, application, windowApp, commonScene);
            create_materials(rTopData, sceneRenderer, sc_materialCount);

            magnumScene = setup_magnum_scene(builder, rTopData, application, windowApp, sceneRenderer, magnum, scene, commonScene);
            cameraCtrl = setup_camera_ctrl(builder, rTopData, windowApp, sceneRenderer, magnumScene);
            cameraFree = setup_camera_free(builder, rTopData, windowApp, scene, cameraCtrl);
            shVisual = setup_shader_visualizer(builder, rTopData, windowApp, sceneRenderer, magnum, magnumScene, sc_matVisualizer);
            shFlat = setup_shader_flat(builder, rTopData, windowApp, sceneRenderer, magnum, magnumScene, sc_matFlat);
            shPhong = setup_shader_phong(builder, rTopData, windowApp, sceneRenderer, magnum, magnumScene, sc_matPhong);
            camThrow = setup_thrower(builder, rTopData, windowApp, cameraCtrl, physShapes);
            shapeDraw = setup_phys_shapes_draw(builder, rTopData, windowApp, sceneRenderer, commonScene, physics, physShapes);
            cursor = setup_cursor(builder, rTopData, application, sceneRenderer, cameraCtrl, commonScene, sc_matFlat, rTestApp.m_defaultPkg);
            planetsDraw = setup_testplanets_draw(builder, rTopData, windowApp, sceneRenderer, cameraCtrl, commonScene, uniCore, uniScnFrame, uniTestPlanets, sc_matVisualizer, sc_matFlat);

            setup_magnum_draw(rTestApp, scene, sceneRenderer, magnumScene);
        };

		#undef SCENE_SESSIONS
		#undef RENDERER_SESSIONS

		return setup_renderer;
    };

    return universeScenario;
}

} // namespace testapp