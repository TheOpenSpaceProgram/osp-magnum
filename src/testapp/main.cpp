/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
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
#include "testapp.h"
#include "scenarios.h"
#include "feature_interfaces.h"
#include "scenarios_magnum.h"
#include "features/console.h"

#include <adera_app/application.h>
#include <adera_app/features/common.h>

#include <osp/util/logging.h>
#include <osp/framework/builder.h>
#include <osp/framework/executor.h>

#include <Corrade/Utility/Arguments.h>

#include <spdlog/sinks/stdout_color_sinks.h>

#include <iostream>
#include <optional>

using namespace testapp;
using namespace adera;
using namespace osp::fw;
using namespace ftr_inter;
using namespace ftr_inter::stages;

// called only from commands to display information
void print_help();
void print_resources();

osp::fw::SingleThreadedExecutor g_executor;
std::optional<TestApp>          g_testApp;

osp::Logger_t g_mainThreadLogger;
osp::Logger_t g_logExecutor;
osp::Logger_t g_logMagnumApp;


void load_scenario(Framework &rFW, ContextId ctx, entt::any userData)
{
    TestApp    &rTestApp = g_testApp.value();
    auto const mainApp   = rFW.get_interface<FIMainApp>(rTestApp.m_mainContext);
    auto       &rAppCtxs = rFW.data_get<AppContexts>(mainApp.di.appContexts);

    if (rAppCtxs.scene.has_value()) // Close existing scene
    {
        auto const cleanup = rFW.get_interface<FICleanupContext>(rAppCtxs.scene);
        if (cleanup.id.has_value())
        {

            rTestApp.m_pExecutor->run(rTestApp.m_framework, cleanup.pl.cleanup);
            rTestApp.m_pExecutor->wait(rTestApp.m_framework);
            if (rTestApp.m_pExecutor->is_running(rTestApp.m_framework))
            {
                OSP_LOG_CRITICAL("Failed to close scene context, something deadlocked.");
                std::abort();
            }
        }

        rFW.close_context(rAppCtxs.scene);

        rAppCtxs.scene = {};
    }

    auto const& rScenario = entt::any_cast<ScenarioOption const&>(userData);
    rScenario.loadFunc(g_testApp.value());

    std::cout << "Loaded scenario: " << rScenario.name << "\n"
              << "--- DESCRIPTION ---\n"
              << rScenario.description
              << "-------------------\n";
}

osp::fw::FeatureDef const ftrMainCommands = feature_def("MainCommands", [] (FeatureBuilder& rFB, DependOn<FIMainApp> mainApp, DependOn<FICinREPL> cinREPL)
{
    rFB.task()
        .name       ("Poll and evaluate commands")
        .run_on     ({mainApp.pl.mainLoop(Run)})
        .sync_with  ({cinREPL.pl.cinLines(UseOrRun)})
        .args       ({                         cinREPL.di.cinLines,        mainApp.di.frameworkModify,         mainApp.di.appContexts})
        .func       ([] (std::vector<std::string> const &rCinLines, FrameworkModify &rFrameworkModify, AppContexts const& appContexts) noexcept
    {
        for (std::string const& cmdStr : rCinLines)
        {
            // First check to see if command is the name of a scenario.
            auto const it = scenarios().find(cmdStr);
            if (it != std::end(scenarios()))
            {
                rFrameworkModify.commands.push_back({
                        .userData = entt::make_any<ScenarioOption const&>(it->second),
                        .func = &load_scenario});
                rFrameworkModify.commands.push_back({
                        .userData = entt::make_any<TestApp&>(g_testApp.value()),
                        .ctx = g_testApp->m_mainContext,
                        .func =  &start_magnum_renderer });
            }
            else if (cmdStr == "help") // Otherwise check all other commands.
            {
                print_help();
            }
            else if (cmdStr == "reopen")
            {
                rFrameworkModify.commands.push_back({
                        .userData = entt::make_any<TestApp&>(g_testApp.value()),
                        .ctx = g_testApp->m_mainContext,
                        .func =  &start_magnum_renderer });

            }
            else if (cmdStr == "exit")
            {
                std::exit(0);
            }
            else
            {
                std::cout << "That command doesn't do anything ._.\n";
            }
        }
    });
});

int main(int argc, char** argv)
{

    // Command line argument parsing
    Corrade::Utility::Arguments args;
    args.addSkippedPrefix("magnum", "Magnum options")
        .addOption("scene", "none")         .setHelp("scene",       "Set the scene to launch")
        .addOption("config")                .setHelp("config",      "path to configuration file to use")
        .addBooleanOption("norepl")         .setHelp("norepl",      "don't enter read, evaluate, print, loop.")
        .addBooleanOption("log-exec")       .setHelp("log-exec",    "Log Task/Pipeline Execution (Extremely chatty!)")
        // TODO .addBooleanOption('v', "verbose")   .setHelp("verbose",     "log verbosely")
        .setGlobalHelp("Helptext goes here.")
        .parse(argc, argv);

    // Setup logging
    auto pSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    pSink->set_pattern("[%T.%e] [%n] [%^%l%$] [%s:%#] %v");
    g_mainThreadLogger = std::make_shared<spdlog::logger>("main-thread", pSink);
    g_logExecutor  = std::make_shared<spdlog::logger>("executor", pSink);
    g_logMagnumApp = std::make_shared<spdlog::logger>("flight", std::move(pSink));

    // Set thread-local logger used by OSP_LOG_* macros
    osp::set_thread_logger(g_mainThreadLogger);

    osp::fw::SingleThreadedExecutor g_executor;
    //std::optional<TestApp> g_testApp;

    if (args.isSet("log-exec"))
    {
        g_executor.m_log = g_logExecutor;
    }

    register_stage_enums();

    TestApp &rTestApp = g_testApp.emplace();

    rTestApp.m_argc = argc;
    rTestApp.m_argv = argv;
    rTestApp.m_pExecutor = &g_executor;

    rTestApp.m_mainContext = rTestApp.m_framework.m_contextIds.create();

    ContextBuilder mainCB { rTestApp.m_mainContext, {}, rTestApp.m_framework };
    mainCB.add_feature(ftrMain);
    if( ! args.isSet("norepl"))
    {
        mainCB.add_feature(ftrREPL);
        mainCB.add_feature(ftrMainCommands);
        print_help();
    }
    bool const success = ContextBuilder::finalize(std::move(mainCB));
    LGRN_ASSERT(success);

    rTestApp.init();

    if(args.value("scene") != "none")
    {
        auto const mainApp           = rTestApp.m_framework.get_interface<FIMainApp>(rTestApp.m_mainContext);
        auto       &rFrameworkModify = rTestApp.m_framework.data_get<FrameworkModify>(mainApp.di.frameworkModify);
        auto const it = scenarios().find(args.value("scene"));
        if(it == std::end(scenarios()))
        {
            OSP_LOG_ERROR("unknown scene");
            //g_testApp.clear_resource_owners();
            return 1;
        }

        rFrameworkModify.commands.push_back({
                        .userData = entt::make_any<ScenarioOption const&>(it->second),
                        .func = &load_scenario});
        rFrameworkModify.commands.push_back({
                        .userData = entt::make_any<TestApp&>(g_testApp.value()),
                        .ctx = g_testApp->m_mainContext,
                        .func =  &start_magnum_renderer });
    }

    std::vector<MainLoopFunc_t> &rMainLoopStack = main_loop_stack();

    // Main (thread) loop
    while (true)
    {
        if ( rMainLoopStack.empty() )
        {
            g_testApp->drive_main_loop();
            std::this_thread::sleep_for (std::chrono::milliseconds(100));
        }
        else
        {
            MainLoopFunc_t func = rMainLoopStack.back();
            bool const keep = func();
            if ( ! keep )
            {
                rMainLoopStack.pop_back();
            }
        }
    }

    spdlog::shutdown();
    return 0;
}


//-----------------------------------------------------------------------------

void print_help()
{
    std::size_t longestName = 0;
    for (auto const& [rName, rScenerio] : scenarios())
    {
        longestName = std::max(rName.size(), longestName);
    }

    std::cout
        << "OSP-Magnum Temporary Debug CLI\n"
        << "Open a scenario:\n";

    for (auto const& [rName, rScenerio] : scenarios())
    {
        std::string spaces(longestName - rName.length(), ' ');
        std::cout << "* " << rName << spaces << " - " << rScenerio.brief << "\n";
    }

    std::cout
        << "Other commands:\n"
        // << "* list_pkg  - List Packages and Resources\n"
        << "* help      - Show this again\n"
        << "* reopen    - Re-open Magnum Application\n"
        << "* exit      - Deallocate everything and return memory to OS\n";
}
