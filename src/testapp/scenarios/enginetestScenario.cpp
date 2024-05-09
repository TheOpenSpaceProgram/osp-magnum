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

#include "enginetestScenario.h"
#include "scenarioUtils.h"

#include "testapp/enginetest.h"
#include "testapp/identifiers.h"

namespace testapp
{

Scenario create_engine_test_scenario()
{
	using namespace osp;
	using namespace osp::active;

	Scenario engineTestScenario{};
	engineTestScenario.name = "enginetest";
	engineTestScenario.description = "Simple rotating cube scenario without using Pipelines/Tasks";
	engineTestScenario.setupFunction = [](TestApp& rTestApp) -> RendererSetupFunc_t
	{
		// Declares idResources TopDataId variable, obtained from Session m_application.m_data[0].
		//
		// This macro expands to:
		//
		//     auto const [idResources, idMainLoopCtrl] = osp::unpack<2>(rTestApp.m_application.m_data);
		//
		// TopDataIds can be used to access rTestApp.m_topData. The purpose of TopData is to store
		// all data in a safe, type-erased, and addressable manner that can be easily accessed by
		// Tasks. This also allow reserving IDs before instantiation (which cannot be achieved
		// with (smart) pointers alone)
		OSP_DECLARE_GET_DATA_IDS(rTestApp.m_application, TESTAPP_DATA_APPLICATION);
		auto& rResources = top_get<Resources>(rTestApp.m_topData, idResources);

		// Create 1 unnamed session as m_sessions[0], reserving 1 TopDataId as idSceneData
		rTestApp.m_scene.m_sessions.resize(1);
		TopDataId const idSceneData = rTestApp.m_scene.m_sessions[0].acquire_data<1>(rTestApp.m_topData)[0];

		// Create scene, store it in rTestApp.m_topData[idSceneData].
		// enginetest::setup_scene returns an entt::any containing one big EngineTestScene struct
		// containing all the scene data: a spinning cube.
		top_assign<enginetest::EngineTestScene>(rTestApp.m_topData, idSceneData, enginetest::setup_scene(rResources, rTestApp.m_defaultPkg));

		// Called when the MagnumApplication / window is opened, called again if the window is
		// re-opened after its closed. Closing the window completely destructs MagnumApplication
		// and all GPU resources. EngineTestScene will remain untouched in the background.
		RendererSetupFunc_t const setup_renderer = [](TestApp& rTestApp) -> void
		{
			TopDataId const idSceneData = rTestApp.m_scene.m_sessions[0].m_data[0];
			auto& rScene = top_get<enginetest::EngineTestScene>(rTestApp.m_topData, idSceneData);

			OSP_DECLARE_GET_DATA_IDS(rTestApp.m_magnum, TESTAPP_DATA_MAGNUM);
			OSP_DECLARE_GET_DATA_IDS(rTestApp.m_windowApp, TESTAPP_DATA_WINDOW_APP);
			auto& rActiveApp = top_get< MagnumApplication >(rTestApp.m_topData, idActiveApp);
			auto& rRenderGl = top_get< draw::RenderGL >(rTestApp.m_topData, idRenderGl);
			auto& rUserInput = top_get< input::UserInputHandler >(rTestApp.m_topData, idUserInput);

			// This creates the renderer actually updates and draws the scene.
			rActiveApp.set_osp_app(generate_osp_magnum_app(rScene, rActiveApp, rRenderGl, rUserInput));
		};

		return setup_renderer;
	};

	return engineTestScenario;

}

} // namespace testapp