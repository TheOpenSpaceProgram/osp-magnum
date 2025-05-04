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
#include "feature_interfaces.h"
#include "features/console.h"
#include "scenarios.h"
#include "scenarios_magnum.h"
#include "testapp.h"

#include <Corrade/Utility/Arguments.h>
#include <Magnum/MeshTools/Transform.h>
#include <Magnum/Primitives/Cone.h>
#include <Magnum/Primitives/Cube.h>
#include <Magnum/Primitives/Cylinder.h>
#include <Magnum/Primitives/Grid.h>
#include <Magnum/Primitives/Icosphere.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/TextureData.h>

#include <adera_app/application.h>
#include <adera_app/feature_interfaces.h>
#include <adera_app/features/common.h>

#include <osp/core/Resources.h>
#include <osp/drawing/own_restypes.h>
#include <osp/executor/singlethread_framework.h>
#include <osp/framework/builder.h>
#include <osp/framework/builder.h>
#include <osp/util/logging.h>
#include <osp/vehicles/ImporterData.h>
#include <osp/vehicles/load_tinygltf.h>

#include <spdlog/fmt/ostr.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <iostream>

using namespace testapp;
using namespace adera;
using namespace osp::fw;
using namespace ftr_inter;
using namespace ftr_inter::stages;

// called only from commands to display information
void print_help();

// prefer not to use names like this outside of main/testapp
void load_a_bunch_of_stuff();

void load_scenario(Framework &rFW, ContextId ctx, entt::any userData);

extern osp::fw::FeatureDef const ftrMainCommands;

int             g_argc;
char**          g_argv;

Framework       g_framework;
ContextId       g_mainContext;
IExecutor      *g_pExecutor  { nullptr };
osp::PkgId      g_defaultPkg  { lgrn::id_null<osp::PkgId>() };

osp::Logger_t   g_mainThreadLogger;
osp::Logger_t   g_logExecutor;
osp::Logger_t   g_logMagnumApp;

bool            g_autoLaunchMagnum = true;

//-----------------------------------------------------------------------------

int main(int argc, char** argv)
{
    // Command line argument parsing
    Corrade::Utility::Arguments args;
    args.addSkippedPrefix   ("magnum", "Magnum options")
        .addOption          ("scene", "none")   .setHelp("scene",       "Set the scene to launch")
        .addOption          ("config")          .setHelp("config",      "path to configuration file to use")
        .addBooleanOption   ("norepl")          .setHelp("norepl",      "don't enter read, evaluate, print, loop.")
        .addBooleanOption   ("log-exec")        .setHelp("log-exec",    "Log Task/Pipeline Execution (Extremely chatty!)")
        // TODO .addBooleanOption('v', "verbose")   .setHelp("verbose",     "log verbosely")
        .setGlobalHelp("Helptext goes here.")
        .parse(argc, argv);
    g_argc = argc;
    g_argv = argv;

    // Setup logging
    auto pSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    pSink->set_pattern("[%T.%e] [%n] [%^%l%$] [%s:%#] %v");
    g_mainThreadLogger  = std::make_shared<spdlog::logger>("main-thread", pSink);
    g_logExecutor       = std::make_shared<spdlog::logger>("executor", pSink);
    g_logMagnumApp      = std::make_shared<spdlog::logger>("flight", std::move(pSink));


    // Set thread-local logger used by OSP_LOG_* macros
    osp::set_thread_logger(g_mainThreadLogger);

    // needed by Tasks & Framework
    register_stage_enums();


    // Select SinglethreadFWExecutor
    auto pSingleThreadExecutor = std::make_unique<osp::exec::SinglethreadFWExecutor>();
    if (args.isSet("log-exec"))
    {
        pSingleThreadExecutor->m_log = g_logExecutor;
    }
    g_pExecutor = pSingleThreadExecutor.get();

    g_mainContext = g_framework.m_contextIds.create();
    ContextBuilder mainCB { g_mainContext, {}, g_framework };
    mainCB.add_feature(ftrMainApp);
    if( ! args.isSet("norepl"))
    {
        mainCB.add_feature(ftrREPL);
        mainCB.add_feature(ftrMainCommands);
        print_help();
    }
    ContextBuilder::finalize(std::move(mainCB));

    auto const mainApp      = g_framework.get_interface<FIMainApp>(g_mainContext);
    auto       &rResources  = g_framework.data_get<osp::Resources&>(mainApp.di.resources);
    rResources.resize_types(osp::ResTypeIdReg_t::size());
    g_defaultPkg = rResources.pkg_create();

    load_a_bunch_of_stuff();

    g_pExecutor->load(g_framework);
    g_pExecutor->wait(g_framework);

    // start
    {
        auto &rMainLoopCtrl = g_framework.data_get<MainLoopControl>(mainApp.di.mainLoopCtrl);
        LGRN_ASSERT(rMainLoopCtrl.mainScheduleWaiting);
        g_pExecutor->task_finish(g_framework, mainApp.tasks.schedule, true, {.cancel = false});
        rMainLoopCtrl.mainScheduleWaiting = false;
    }

    if(args.value("scene") != "none")
    {
        auto const mainApp    = g_framework.get_interface<FIMainApp>(g_mainContext);
        auto       &rFWModify = g_framework.data_get<FrameworkModify>(mainApp.di.frameworkModify);
        auto const it = scenarios().find(args.value("scene"));
        if(it == std::end(scenarios()))
        {
            OSP_LOG_ERROR("unknown scene");
            return 1;
        }

        rFWModify.commands.push_back({
                .userData = entt::make_any<ScenarioOption const&>(it->second),
                .func = &load_scenario});
        rFWModify.commands.push_back({
                .userData = MagnumRendererArgs{g_argc, g_argv, g_defaultPkg},
                .mainCtx = g_mainContext,
                .func =  &start_magnum_renderer });
    }

    std::vector<MainLoopFunc_t> &rMainLoopStack = main_loop_stack();

    rMainLoopStack.push_back([] (MainLoopArgs vars) -> bool
    {
        auto const mainApp        = vars.rFW.get_interface<FIMainApp>(vars.mainCtx);
        auto       &rMainLoopCtrl = vars.rFW.data_get<MainLoopControl&>(mainApp.di.mainLoopCtrl);
        auto       &rFWModify     = vars.rFW.data_get<FrameworkModify&>(mainApp.di.frameworkModify);

        if (rFWModify.commands.empty())
        {
            vars.rExecutor.wait(vars.rFW);
            if (rMainLoopCtrl.keepOpenWaiting)
            {
                rMainLoopCtrl.keepOpenWaiting = false;
                vars.rExecutor.task_finish(vars.rFW, mainApp.tasks.keepOpen, true, {.cancel = false});
            }
        }
        else
        {
            // Framework modify commands exist. Stop the main loop

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
        }
        std::this_thread::sleep_for (std::chrono::milliseconds(100));

        return true;
    });


    // Main (thread) loop
    while (!rMainLoopStack.empty())
    {
        MainLoopFunc_t &rFunc = rMainLoopStack.back();
        bool const keep = rFunc(MainLoopArgs{.rFW = g_framework, .rExecutor = *g_pExecutor, .mainCtx = g_mainContext});
        if ( ! keep )
        {
            rMainLoopStack.pop_back();
        }
    }

    spdlog::shutdown();
    return 0;
}




void load_scenario(Framework &rFW, ContextId ctx, entt::any userData)
{
    auto      const mainApp    = rFW.get_interface<FIMainApp>(g_mainContext);
    auto            &rAppCtxs  = rFW.data_get<AppContexts>(mainApp.di.appContexts);

    if (rAppCtxs.scene.has_value()) // Close existing scene
    {
        auto const cleanup = rFW.get_interface<FICleanupContext>(rAppCtxs.scene);
        if (cleanup.id.has_value())
        {

            //g_pExecutor->run(g_framework, cleanup.pl.cleanup);
            //g_pExecutor->wait(g_framework);
            if (g_pExecutor->is_running(g_framework, mainApp.loopblks.mainLoop))
            {
                OSP_LOG_CRITICAL("Failed to close scene context, something deadlocked.");
                std::abort();
            }
        }

        rFW.close_context(rAppCtxs.scene);

        rAppCtxs.scene = {};
    }

    auto const& rScenario = entt::any_cast<ScenarioOption const&>(userData);
    rScenario.loadFunc(ScenarioArgs{
        .rFW            = g_framework,
        .mainContext    = g_mainContext,
        .defaultPkg     = g_defaultPkg
    });

    std::cout << "Loaded scenario: " << rScenario.name << "\n"
              << "--- DESCRIPTION ---\n"
              << rScenario.description
              << "-------------------\n";
}

osp::fw::FeatureDef const ftrMainCommands = feature_def("MainCommands", [] (FeatureBuilder& rFB, DependOn<FIMainApp> mainApp, DependOn<FICinREPL> cinREPL)
{
    rFB.task()
        .name       ("Poll and evaluate commands")
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

                if (g_autoLaunchMagnum && ! appContexts.window.has_value())
                {
                    rFrameworkModify.commands.push_back({
                            .userData   = MagnumRendererArgs{g_argc, g_argv, g_defaultPkg},
                            .mainCtx    = g_mainContext,
                            .func       = &start_magnum_renderer });
                }

            }
            else if (cmdStr == "help") // Otherwise check all other commands.
            {
                print_help();
            }
            else if (cmdStr == "magnum")
            {
                if ( ! appContexts.window.has_value() )
                {
                    rFrameworkModify.commands.push_back({
                            .userData   = MagnumRendererArgs{g_argc, g_argv, g_defaultPkg},
                            .mainCtx    = g_mainContext,
                            .func       =  &start_magnum_renderer });
                }
                else
                {
                    std::cout << "Magnum is already open\n";
                }
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
        << "* magnum    - Open Magnum Application\n"
        << "* exit      - Deallocate everything and return memory to OS\n";
}

void load_a_bunch_of_stuff()
{
    using namespace osp::restypes;
    using namespace Magnum;
    using Primitives::ConeFlag;
    using Primitives::CylinderFlag;

    auto const fiMain      = g_framework.get_interface<FIMainApp>(g_mainContext);
    auto       &rResources = g_framework.data_get<osp::Resources&>(fiMain.di.resources);

    rResources.data_register<Trade::ImageData2D>    (gc_image);
    rResources.data_register<Trade::TextureData>    (gc_texture);
    rResources.data_register<osp::TextureImgSource> (gc_texture);
    rResources.data_register<Trade::MeshData>       (gc_mesh);
    rResources.data_register<osp::ImporterData>     (gc_importer);
    rResources.data_register<osp::Prefabs>          (gc_importer);
    osp::register_tinygltf_resources(rResources);

    // Load sturdy glTF files
    const std::string_view datapath = { "OSPData/adera/" };
    const std::vector<std::string_view> meshes =
    {
        "spamcan.sturdy.gltf",
        "stomper.sturdy.gltf",
        "ph_capsule.sturdy.gltf",
        "ph_fuselage.sturdy.gltf",
        "ph_engine.sturdy.gltf",
        //"ph_plume.sturdy.gltf",
        "ph_rcs.sturdy.gltf"
        //"ph_rcs_plume.sturdy.gltf"
    };

    // TODO: Make new gltf loader. This will read gltf files and dump meshes,
    //       images, textures, and other relevant data into osp::Resources
    for (auto const& meshName : meshes)
    {
        osp::ResId res = osp::load_tinygltf_file(osp::string_concat(datapath, meshName), rResources, g_defaultPkg);
        osp::assigns_prefabs_tinygltf(rResources, res);
    }

    // Add a default primitives
    auto const add_mesh_quick = [&rResources = rResources] (std::string_view const name, Trade::MeshData&& data)
    {
        osp::ResId const meshId = rResources.create(gc_mesh, g_defaultPkg, osp::SharedString::create(name));
        rResources.data_add<Trade::MeshData>(gc_mesh, meshId, std::move(data));
    };

    Trade::MeshData &&cylinder = Magnum::MeshTools::transform3D( Primitives::cylinderSolid(3, 16, 1.0f, CylinderFlag::CapEnds), Matrix4::rotationX(Deg(90)), 0);
    Trade::MeshData &&cone = Magnum::MeshTools::transform3D( Primitives::coneSolid(3, 16, 1.0f, ConeFlag::CapEnd), Matrix4::rotationX(Deg(90)), 0);

    add_mesh_quick("cube", Primitives::cubeSolid());
    add_mesh_quick("cubewire", Primitives::cubeWireframe());
    add_mesh_quick("sphere", Primitives::icosphereSolid(2));
    add_mesh_quick("cylinder", std::move(cylinder));
    add_mesh_quick("cone", std::move(cone));
    add_mesh_quick("grid64solid", Primitives::grid3DSolid({63, 63}));

    OSP_LOG_INFO("Resource loading complete");
}
